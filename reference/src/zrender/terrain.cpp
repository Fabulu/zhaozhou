// terrain.cpp — heightfield patch draw (DrawProcedural forge_kind 0), earth
// field application (TerrainField) and the surface sheet (SurfaceStamp).
//
// [world-identity wave] Migrated to the dual-heightfield island format
// (spec/terrain_rules.md): compose_lattice is THE one §4.1 evaluation (base +
// scar clamped at bottom, live fields clamped at bottom); dual patches emit
// underside cells and rim walls to the MODELLED bottom; void cells emit no
// surface. Legacy single-surface pages render pixel-identically to the
// pre-migration path (reel_sequence_crc + render goldens pin it).
//
// Law:
//   spec/terrain_rules.md  §3.4 composition/clamp, §4.1-§4.3 lattice law,
//                       §5 rim geometry (FORGE.CLIFF reference reading)
//   spec/commands.zidl  DrawProcedural 0x0302 (heightfield_patch: `program`
//                       names the terrain-patch page, transform = placement,
//                       screen_error = pixels-per-error policy — recorded,
//                       fixed grid resolution at Phase 3), TerrainField
//                       0x0200 (earth .zprog over the footprint columns,
//                       parameter-blob convention: params-struct lanes packed
//                       Q16.16 LE in declaration order into p0..p7; bytes
//                       beyond 4*lane_count zero — packet-side guarantee),
//                       SurfaceStamp 0x0210 (circle/annulus at the transform
//                       translation, radius/ring_width in world metres)
//   spec/cartridge.md §4 terrain patch page: {u16 w,h; rectfx envelope;
//                       w*h height16, ascending z-then-x}
//   spec/form/field-ir.md §7.1 earth I/O record: in (x:fx, z:fx, age:u32,
//                       phase:fx, p0..p7:fx) -> out (height:fx, velocity:fx,
//                       material:u32, nav_cost:fx); the ONE interpreter
//                       (zfield::interpret) is called — never reimplemented
//   spec/qformats.md §3 single-rounding fx16 arithmetic, §7.2 isqrt_u64,
//                       §9 height16 -> fx16 exact (raw << 8)
//   charter §12         surface sheet: 64x64 texels, 8-bit tag + 8-bit
//                       strength, per patch, persistent; the Phase-3 sheet
//                       influence is a SHADING TINT (plan W3.5)
//   plan W3.5/D7        painter's algorithm (far-to-near by centroid 1/z)
//                       with Q16.16 1/z depth per view
//
// [w3.5-software] readings (flagged for W3.6/W3.7):
//   * TerrainField is applied at the DrawProcedural draws (the field needs
//     the patch's transform to place columns); age = frame_tick - start_tick
//     saturated to duration_ticks, and an app whose frame tick precedes
//     start_tick has no effect yet; phase = round(age/duration) fx16;
//   * stamp op 0 = replace/max (charter §12 family), op 1 = decay-accumulate
//     (existing strength halves, then the stamp adds, saturating u8);
//   * sheet tint law: rgb = rgb * (255 - strength/2) / 256 (max ~50%
//     darkening; the tag byte is stored for the L4 mosaic sampler).

#include "internal.hpp"

#include "zfield/zfield.hpp"

#include <algorithm>
#include <cstring>

namespace zref {
namespace render {

// The ONE flat-shade law (hoisted verbatim from the draw_heightfield lambda
// 2026-08-16 when the creature lane needed the identical arithmetic —
// charter 29-6; the golden CRCs pin that nothing changed but the address).
int32_t shade_flat_tri(int32_t ax, int32_t ay, int32_t az, int32_t bx, int32_t by, int32_t bz,
                       int32_t cx, int32_t cy, int32_t cz, SatLedger* L) {
  const int64_t e1x = static_cast<int64_t>(bx) - ax;
  const int64_t e1y = static_cast<int64_t>(by) - ay;
  const int64_t e1z = static_cast<int64_t>(bz) - az;
  const int64_t e2x = static_cast<int64_t>(cx) - ax;
  const int64_t e2y = static_cast<int64_t>(cy) - ay;
  const int64_t e2z = static_cast<int64_t>(cz) - az;
  // Q-format algebra, stated: e1/e2 components are fx16 differences, i.e.
  // Q16.16 raw. A product of two Q16.16 raws is Q32.32 raw (32 fraction
  // bits), so the cross-product lanes n0/n1/n2 are Q32.32 and the shift
  // back to the Q16.16 the code below assumes is rescale(.,16) — NOT 32.
  //
  // DEFECT FIXED 2026-08-15: it was rescale(.,32), which yields Q32.0 —
  // the normal quantised to WHOLE world-units^2. Under ~1 m grid spacing
  // every component of a near-flat cell rounds to 0, the nmag2 == 0 guard
  // fires for every triangle, and the entire patch shades solid black.
  // (41x41 over +-12 m = 0.6 m spacing was a black silhouette; 25x25 over
  // the same envelope = 1.0 m rendered correctly.) Phase-6 Mantle patches
  // are 32x32 cells per world patch — sub-metre by design.
  // Test: tests/render/render_heightfield.cpp test_submetre_shading.
  const int64_t n0 = e1y * e2z - e1z * e2y;  // Q32.32 raw
  const int64_t n1 = e1z * e2x - e1x * e2z;
  const int64_t n2 = e1x * e2y - e1y * e2x;
  const int32_t fx = rescale_s32(n0, 16, L);  // -> Q16.16 world-units^2
  const int32_t fy = rescale_s32(n1, 16, L);
  const int32_t fz = rescale_s32(n2, 16, L);
  const __int128 ndot = static_cast<__int128>(fx) * kLightX + static_cast<__int128>(fy) * kLightY +
                        static_cast<__int128>(fz) * kLightZ;
  const uint64_t nmag2 = static_cast<uint64_t>(fx) * static_cast<uint64_t>(fx) +
                         static_cast<uint64_t>(fy) * static_cast<uint64_t>(fy) +
                         static_cast<uint64_t>(fz) * static_cast<uint64_t>(fz);
  if (nmag2 == 0) return 0;  // exactly degenerate (zero-area) triangle
  // scale check: n_fx is Q16.16, so ndot = n.L is Q32.32 raw and
  // nmag2 = |n_fx|^2 is Q32.32 raw, hence isqrt_u64(nmag2) = |n_fx| is
  // Q16.16 raw — ndot/nmag is exactly (nhat.L) in Q16.16 (§4 one
  // rounding). No extra shift.
  const int32_t shade = div_rhu_s128(ndot, static_cast<__int128>(isqrt_u64(nmag2)));
  return shade < 0 ? 0 : (shade > 0x10000 ? 0x10000 : shade);
}

namespace {

// (the a + (b-a)*num/den grid lerp moved to zref::terrain::lattice_lerp —
// ONE implementation shared with the bake stencil placement, charter §29-6)

// transform2fx placement: translation then row-major 2x2 (commands.zidl),
// computed as ONE exact s128 sum + ONE rescale (the §3 single-rounding law
// applied to the two-product form).
inline fx16 place_x(const ZhTransform2fx& t, fx16 lx, fx16 lz, SatLedger* L) {
  const __int128 p = static_cast<__int128>(t.r00) * lx.raw + static_cast<__int128>(t.r01) * lz.raw +
                     (static_cast<__int128>(t.tx) << 16);
  return fx16{rescale_s32(static_cast<int64_t>(p), 16, L, &SatLedger::mul)};
}
inline fx16 place_z(const ZhTransform2fx& t, fx16 lx, fx16 lz, SatLedger* L) {
  const __int128 p = static_cast<__int128>(t.r10) * lx.raw + static_cast<__int128>(t.r11) * lz.raw +
                     (static_cast<__int128>(t.ty) << 16);
  return fx16{rescale_s32(static_cast<int64_t>(p), 16, L, &SatLedger::mul)};
}

}  // namespace

// ------------------------------------------------------------- surface sheet

void stamp_surface(SurfaceSheet& sheet, const TerrainPatch& patch, const ZhCmdSurfaceStamp& st) {
  // circle (ring_width <= 0) or annulus [radius-ring_width, radius] in world
  // metres at the transform translation (commands.zidl SurfaceStamp). The
  // 2x2 rotation of a circle/annulus is the identity figure — applied for
  // record, not needed by the distance test.
  const int64_t r = st.radius;
  const int64_t rw = st.ring_width > 0 ? st.ring_width : 0;
  const int64_t r_outer2 = r * r;
  const int64_t r_inner = rw > 0 ? (r - rw > 0 ? r - rw : 0) : 0;
  const int64_t r_inner2 = r_inner * r_inner;
  const uint8_t strength = static_cast<uint8_t>(st.strength >> 8);
  const int64_t ex0 = patch.env_x0, ex1 = patch.env_x1;
  const int64_t ez0 = patch.env_z0, ez1 = patch.env_z1;
  for (int j = 0; j < 64; ++j) {
    for (int i = 0; i < 64; ++i) {
      // texel centre in world units ((i+0.5)/64 across the envelope)
      const int64_t wx = ex0 + ((ex1 - ex0) * (2 * i + 1)) / 128;
      const int64_t wz = ez0 + ((ez1 - ez0) * (2 * j + 1)) / 128;
      const int64_t dx = wx - st.transform.tx;
      const int64_t dz = wz - st.transform.ty;
      const int64_t d2 = dx * dx + dz * dz;
      if (d2 > r_outer2 || d2 < r_inner2) continue;
      const size_t idx = static_cast<size_t>(j) * 64 + i;
      if (st.operation == 1) {
        // decay-accumulate: halve, then add, saturate (charter §12 family)
        const int32_t v = (sheet.strength[idx] >> 1) + strength;
        sheet.strength[idx] = static_cast<uint8_t>(v > 255 ? 255 : v);
      } else {
        // stamp: keep the peak (replace/max)
        if (strength > sheet.strength[idx]) sheet.strength[idx] = strength;
      }
      sheet.tag[idx] = st.tag;
    }
  }
}

uint8_t sample_sheet(const SurfaceSheet& sheet, const TerrainPatch& patch, fx16 wx, fx16 wz) {
  const int64_t ex0 = patch.env_x0, ex1 = patch.env_x1;
  const int64_t ez0 = patch.env_z0, ez1 = patch.env_z1;
  if (ex1 <= ex0 || ez1 <= ez0) return 0;
  int64_t iu = ((static_cast<int64_t>(wx.raw) - ex0) * 64) / (ex1 - ex0);
  int64_t iv = ((static_cast<int64_t>(wz.raw) - ez0) * 64) / (ez1 - ez0);
  if (iu < 0) iu = 0;
  if (iu > 63) iu = 63;
  if (iv < 0) iv = 0;
  if (iv > 63) iv = 63;
  return sheet.strength[static_cast<size_t>(iv) * 64 + static_cast<size_t>(iu)];
}

// ------------------------------------------------------------- TerrainField

int32_t field_velocity_lane(const int32_t out[4]) { return out[1]; }

// ---------------------------------------------------- composed lattice ----

terrain::ComposedLattice compose_lattice(const TerrainPatch& patch, const ZhTransform2fx& xform,
                                         const std::vector<FieldApp>& fields, uint32_t frame_tick,
                                         std::vector<TerrainVelocitySample>* velocity_out,
                                         SatLedger* L) {
  terrain::ComposedLattice lat;
  const int w = patch.width;
  const int h = patch.height;
  lat.w = w;
  lat.h = h;
  if (w < 2 || h < 2) return lat;
  const size_t n = static_cast<size_t>(w) * h;

  // world grid: envelope lerp (one rounding per interior line, §4), then the
  // transform2fx placement (one rounding per vertex component, §3)
  lat.wx.resize(static_cast<size_t>(w));
  lat.wz.resize(static_cast<size_t>(h));
  for (int i = 0; i < w; ++i)
    lat.wx[static_cast<size_t>(i)] =
        place_x(xform, fx16{terrain::lattice_lerp(patch.env_x0, patch.env_x1, i, w - 1)}, fx16{0},
                L)
            .raw;
  for (int j = 0; j < h; ++j)
    lat.wz[static_cast<size_t>(j)] =
        place_z(xform, fx16{0}, fx16{terrain::lattice_lerp(patch.env_z0, patch.env_z1, j, h - 1)},
                L)
            .raw;

  // compose_top per §3.4: authored base (exact <<8, §9) + scar (fx_add),
  // clamped at the underside; the legacy page reduces to base alone
  lat.dual = patch.bottom.size() == n;
  const bool has_scar = patch.scar.size() == n;
  lat.top.resize(n);
  if (lat.dual) lat.bottom.resize(n);
  for (size_t k = 0; k < n; ++k) {
    int32_t top = static_cast<int32_t>(patch.heights[k]) << 8;
    if (has_scar) top = fx_add(fx16{top}, fx16{static_cast<int32_t>(patch.scar[k]) << 8}, L).raw;
    if (lat.dual) {
      const int32_t bot = static_cast<int32_t>(patch.bottom[k]) << 8;
      lat.bottom[k] = bot;
      if (top < bot) top = bot;  // clamp at the underside (§3.4)
    }
    lat.top[k] = top;
  }

  // TerrainField application: apps in command order; columns ascending
  // z-then-x (the cartridge patch order) — the recorded velocity order.
  // Live fields act on the top surface only (§3.4).
  for (const FieldApp& app : fields) {
    const ZhCmdTerrainField& cmd = app.cmd;
    const zfield::Decoded* prog = app.prog;
    if (prog == nullptr) continue;              // resource miss counted at walk time
    if (frame_tick < cmd.start_tick) continue;  // not begun yet
    const uint64_t span = static_cast<uint64_t>(frame_tick) - cmd.start_tick;
    const uint32_t age =
        span > cmd.duration_ticks ? cmd.duration_ticks : static_cast<uint32_t>(span);
    for (int j = 0; j < h; ++j) {
      for (int i = 0; i < w; ++i) {
        const int32_t cx = lat.wx[static_cast<size_t>(i)], cz = lat.wz[static_cast<size_t>(j)];
        if (cx < cmd.footprint.x0 || cx > cmd.footprint.x1 || cz < cmd.footprint.y0 ||
            cz > cmd.footprint.y1)
          continue;
        int32_t in[12] = {cx, cz, static_cast<int32_t>(age), 0};
        in[3] = cmd.duration_ticks == 0
                    ? (1 << 16)
                    : static_cast<int32_t>(
                          (static_cast<uint64_t>(age) * (1 << 16) + cmd.duration_ticks / 2) /
                          cmd.duration_ticks);
        const size_t n_in = prog->in_lanes.size() < 12 ? prog->in_lanes.size() : 12;
        for (size_t k = 4; k < n_in; ++k) {
          uint32_t lane = 0;
          std::memcpy(&lane, cmd.parameters + 4 * (k - 4), 4);
          in[k] = static_cast<int32_t>(lane);
        }
        int32_t out[4] = {0, 0, 0, 0};
        const size_t n_out = prog->out_lanes.size() < 4 ? prog->out_lanes.size() : 4;
        zfield::interpret(*prog, in, n_in, out, n_out);
        const size_t idx = static_cast<size_t>(j) * w + i;
        lat.top[idx] = fx_add(fx16{lat.top[idx]}, fx16{out[0]}, L).raw;  // height lane
        if (velocity_out != nullptr) {
          velocity_out->push_back(TerrainVelocitySample{cx, cz, field_velocity_lane(out)});
        }
      }
    }
  }

  // live_top = max(compose_top + fields, bottom): the ONE clamp after the
  // command-order fx_add chain (§3.4) — waves can never punch below the
  // underside, so a transient can never fake a breach
  if (lat.dual) {
    for (size_t k = 0; k < n; ++k)
      if (lat.top[k] < lat.bottom[k]) lat.top[k] = lat.bottom[k];
  }

  if (patch.cell_state.size() == static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1))
    lat.cell_state = patch.cell_state;
  return lat;
}

// ------------------------------------------------------------ heightfield

void draw_heightfield(WorkSurface& surf, const Viewport& vpp, const mat4fx& vp,
                      const TerrainPatch& patch, const ZhTransform2fx& xform, const Material& mat,
                      const SurfaceSheet* sheet, const std::vector<FieldApp>& fields,
                      uint32_t frame_tick, std::vector<TerrainVelocitySample>* velocity_out,
                      SatLedger* L, const Tileset* tileset) {
  const int w = patch.width;
  const int h = patch.height;
  if (w < 2 || h < 2) return;  // a degenerate patch draws nothing

  // THE lattice law (§4.1): one evaluation; the renderer only tessellates it
  const terrain::ComposedLattice lat =
      compose_lattice(patch, xform, fields, frame_tick, velocity_out, L);
  const std::vector<int32_t>& wx = lat.wx;
  const std::vector<int32_t>& wz = lat.wz;
  const std::vector<int32_t>& y = lat.top;
  const bool dual = lat.dual;

  // the texture lane (terrain_rules §6): active only when the island brings
  // its tileset AND layer E candidates — otherwise every path below is the
  // byte-identical legacy flat shading (golden CRC law)
  const bool textured = tileset != nullptr && patch.textured();
  const bool has_tint = textured && patch.tint.size() == static_cast<size_t>(w) * h;

  // project the grid once per view call (top, and the bottom lattice when
  // the patch models an underside). [deep-keel wave] Near-plane rejection
  // is PER PRIMITIVE — the documented Phase-3 clip model ("whole-primitive
  // near-plane rejection", sky_and_beams.md §1.2 projection corollary; the
  // sky under-plane subdivides its grid for exactly this reason). The old
  // whole-PATCH abort made a near camera erase the island; now a cell (or
  // wall/underside quad) whose corner vertices include one behind the eye
  // is dropped and the rest draws. Deterministic either way.
  std::vector<ScreenV> sv(static_cast<size_t>(w) * h);
  std::vector<uint8_t> vis(static_cast<size_t>(w) * h, 0);
  std::vector<ScreenV> svb;
  if (dual) svb.resize(static_cast<size_t>(w) * h);
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      const size_t idx = static_cast<size_t>(j) * w + i;
      const ProjOut p = project_vertex(vp, vpp, fx16{wx[i]}, fx16{y[idx]}, fx16{wz[j]}, L);
      if (!p.in) continue;  // behind the eye: prims touching this vertex drop
      sv[idx] = p.s;
      vis[idx] = 1;
      if (dual) {
        const ProjOut pb =
            project_vertex(vp, vpp, fx16{wx[i]}, fx16{lat.bottom[idx]}, fx16{wz[j]}, L);
        if (!pb.in) continue;
        svb[idx] = pb.s;
        vis[idx] = 2;  // both surfaces of this vertex project
      }
    }
  }
  const uint8_t vis_need = dual ? 2 : 1;
  const auto cell_visible = [&](size_t a, size_t b, size_t c, size_t d) {
    return vis[a] == vis_need && vis[b] == vis_need && vis[c] == vis_need && vis[d] == vis_need;
  };

  // painter's order: far primitives first (small 1/w), ties by kind then
  // index (D7; legacy pages carry a single kind, so the order is exactly the
  // pre-migration cell order). Kinds: 0 = top surface cell, 1 = underside
  // cell, 2 = rim wall (index/index2 = the wall's two top-edge vertices).
  struct Prim {
    int32_t depth;
    uint8_t kind;
    uint32_t index;
    uint32_t index2 = 0;     // wall: the second top-edge vertex
    int32_t u0 = 0, u1 = 0;  // wall strata U at index/index2 (Q16.16, §5)
  };
  std::vector<Prim> prims;
  prims.reserve(static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1));
  for (int j = 0; j + 1 < h; ++j) {
    for (int i = 0; i + 1 < w; ++i) {
      if (dual && lat.substance(i, j) != terrain::kSolid) continue;  // void: no surface
      const size_t i00 = static_cast<size_t>(j) * w + i;
      const size_t i10 = i00 + 1;
      const size_t i01 = i00 + w;
      const size_t i11 = i01 + 1;
      if (!cell_visible(i00, i10, i01, i11)) continue;  // near-plane rejection
      const int32_t d = (sv[i00].d + sv[i10].d + sv[i01].d + sv[i11].d) / 4;
      prims.push_back(Prim{d, 0, static_cast<uint32_t>(i00)});
      if (!dual) continue;
      const int32_t db = (svb[i00].d + svb[i10].d + svb[i01].d + svb[i11].d) / 4;
      prims.push_back(Prim{db, 1, static_cast<uint32_t>(i00)});
    }
  }

  // rim walls (FORGE.CLIFF law, terrain_rules §5): THE one rim plan —
  // enumeration, per-32x32-page budget, contiguous-collinear span merge,
  // near-camera priority clamp (zref::forge::rim_plan; charter §29-6: the
  // renderer emits exactly the plan, never a second enumeration)
  if (dual) {
    std::vector<int32_t> vdist(static_cast<size_t>(w) * h, 0);
    for (size_t k = 0; k < vdist.size(); ++k) {
      if (vis[k] == 0) continue;
      int32_t d = sv[k].d;
      if (vis[k] == 2 && svb[k].d > d) d = svb[k].d;
      vdist[k] = d;
    }
    const forge::RimPlan plan = forge::rim_plan(lat, vdist.data());
    // strata U accumulation (§5): rim length in the plan's scan order, 8 m
    // STRATA_M period -> exact >>3 (§1.3's power-of-two shift law)
    int64_t acc = 0;
    const int32_t pitch_raw = wx[1] - wx[0];
    for (const forge::RimEdge& e : plan.edges) {
      // wall endpoints on the lattice: sides 0/1 run along +x on rows
      // cj/cj+1, sides 2/3 along +z on columns ci/ci+1; vertex order keeps
      // the flat-shade normal pointing OUT of the solid cell (as before)
      const size_t base = static_cast<size_t>(e.cj) * w + e.ci;
      size_t va, vb;
      switch (e.side) {
        case 0:
          va = base;
          vb = base + e.span;
          break;  // -z, left to right
        case 1:
          va = base + w + e.span;
          vb = base + w;
          break;  // +z, right to left
        case 2:
          va = base + static_cast<size_t>(e.span) * w;
          vb = base;
          break;  // -x, far to near
        default:
          va = base + 1;
          vb = base + static_cast<size_t>(e.span) * w + 1;
          break;  // +x
      }
      const int32_t u0 = static_cast<int32_t>(acc >> 3);
      acc += static_cast<int64_t>(e.span) * pitch_raw;
      const int32_t u1 = static_cast<int32_t>(acc >> 3);
      if (vis[va] != 2 || vis[vb] != 2) continue;  // near-plane rejection
      const int32_t dw = (sv[va].d + sv[vb].d + svb[va].d + svb[vb].d) / 4;
      prims.push_back(Prim{dw, 2, static_cast<uint32_t>(va), static_cast<uint32_t>(vb), u0, u1});
    }
  }
  std::stable_sort(prims.begin(), prims.end(), [](const Prim& a, const Prim& b) {
    if (a.depth != b.depth) return a.depth < b.depth;
    if (a.kind != b.kind) return a.kind < b.kind;
    if (a.index != b.index) return a.index < b.index;
    return a.index2 < b.index2;
  });

  // Terrain raster law (internal.hpp PAINTER/DEPTH LAW, D7): the painter
  // sort IS the ordering between terrain cells — a constant-w orthographic
  // view-projection makes the Q16.16 1/z IDENTICAL for every cell, so a
  // strict-greater depth test would reject each later cell against the
  // first writer. Cells therefore rasterize with depth_test = OFF but still
  // WRITE their 1/z, so every depth-testing pass that follows (sky pass-6
  // sun/cloud, DrawForm markers, DrawPopulation sprites) resolves against
  // the terrain exactly per qformats §8.
  TriMode mode;
  mode.depth_test = false;
  mode.depth_write = true;

  // flat shading per triangle: the ONE shared law (shade_flat_tri above —
  // hoisted 2026-08-16 for the creature lane; arithmetic verbatim).
  const auto shade_points = [&](int32_t ax, int32_t ay, int32_t az, int32_t bx, int32_t by,
                                int32_t bz, int32_t cx, int32_t cy, int32_t cz) -> int32_t {
    return shade_flat_tri(ax, ay, az, bx, by, bz, cx, cy, cz, L);
  };
  // top-surface shading — the pre-migration arithmetic verbatim
  const auto shade_tri = [&](const std::vector<int32_t>& hgt, size_t ia, size_t ib,
                             size_t ic) -> int32_t {
    return shade_points(wx[ia % w], hgt[ia], wz[ia / w], wx[ib % w], hgt[ib], wz[ib / w],
                        wx[ic % w], hgt[ic], wz[ic / w]);
  };
  // walls and undersides get an ambient floor (0.25 + 0.75*lambert): the
  // renderer's ONE light is high (1,2,1)/sqrt(6), so a raw lambert leaves a
  // down-facing keel pitch-black. Applies ONLY to the dual-format geometry
  // — top-surface shading is untouched (golden CRC law).
  const auto ambient = [](int32_t shade) -> int32_t {
    return 16384 + static_cast<int32_t>((static_cast<int64_t>(shade) * 49152 + 32768) >> 16);
  };

  // ---- the texture lane (terrain_rules §5/§6; active only when `textured`)
  //
  // UV law, all exact shifts (§1.3: power-of-two periods keep addressing
  // division-free):
  //   top cells:  u = wx / pitch, v = wz / pitch  (one tile per cell, §6.2;
  //               the mirrored fold makes same-id neighbours seamless)
  //   underside:  u = wx / 8, v = wz / 8          (planar world, §5)
  //   walls:      u = accumulated rim length / 8 (the plan walk above),
  //               v = (top - y) / 8 per vertex    (§5: V spans true thickness)
  // The palette ladder: the flat-shade weight is quantised to 2 bits
  // ((shade + 8191) >> 14) BEFORE modulation so the modulated palette stays
  // inside the 256-colour capture law; the modulation is ONE round-half-up
  // over the s128 product shade x tint x sheet, and all-unity is EXACT
  // unity (the sheet-tint lesson at the top of this file).
  int top_shift = 0;
  {
    const int32_t pr0 = wx[1] - wx[0];
    int32_t pr = pr0 > 0 ? pr0 : (1 << 16);
    int sh = 0;
    while ((pr & 1) == 0 && sh < 30) {
      pr >>= 1;
      ++sh;
    }
    if (pr == 1) top_shift = sh - 16;  // pitch = 2^k metres -> u = wx >> k
    if (top_shift < 0) top_shift = 0;
  }
  const size_t nlattice = static_cast<size_t>(w) * h;
  std::vector<int32_t> u_top, v_top, u_und, v_und;
  if (textured) {
    u_top.resize(nlattice);
    v_top.resize(nlattice);
    u_und.resize(nlattice);
    v_und.resize(nlattice);
    for (int j = 0; j < h; ++j) {
      const int32_t vv = wz[static_cast<size_t>(j)];
      const int32_t vu = vv >> 3;
      const int32_t vt = vv >> top_shift;
      for (int i = 0; i < w; ++i) {
        const size_t k = static_cast<size_t>(j) * w + i;
        u_top[k] = wx[static_cast<size_t>(i)] >> top_shift;
        u_und[k] = wx[static_cast<size_t>(i)] >> 3;
        v_top[k] = vt;
        v_und[k] = vu;
      }
    }
  }
  // layer-H tint per cell: the FLAT stand-in for the Gouraud tint — the
  // average of the four corner RGB565 values, ONE rounding per channel over
  // the 4-corner sum (factor = rhu(SUM x 65536, 4 x (2^b - 1))); a uniform
  // full-field tint is exact unity. The per-vertex ride is the Phase 4/5
  // Gouraud path (charter §8).
  const auto cell_tint = [&](size_t a, int32_t* tr, int32_t* tg, int32_t* tb) {
    const size_t corners[4] = {a, a + 1, a + w, a + w + 1};
    int32_t sr = 0, sg = 0, sb = 0;
    for (size_t q : corners) {
      const uint16_t t = patch.tint.empty() ? 0xFFFF : patch.tint[q];
      sr += (t >> 11) & 0x1F;
      sg += (t >> 5) & 0x3F;
      sb += t & 0x1F;
    }
    *tr = static_cast<int32_t>(div_rhu_s128(static_cast<__int128>(sr) * 65536, 124));
    *tg = static_cast<int32_t>(div_rhu_s128(static_cast<__int128>(sg) * 65536, 252));
    *tb = static_cast<int32_t>(div_rhu_s128(static_cast<__int128>(sb) * 65536, 124));
  };
  // the composed modulation: ONE rounding over shade_q x tint x sheet (s128);
  // walls/underside pass sheet_q = unity (no sheet lane below the top)
  const auto mod_of = [&](int32_t shade, int32_t tint_q, int32_t sheet_q) -> int32_t {
    const int32_t shade_q = (shade + 8191) >> 14;  // the palette ladder (0..4)
    const __int128 prod = static_cast<__int128>(shade_q << 14) * tint_q * sheet_q;
    return static_cast<int32_t>(div_rhu_s128(prod, static_cast<__int128>(1) << 32));
  };
  const auto sheet_factor = [&](uint8_t strength) -> int32_t {
    if (sheet == nullptr) return 65536;
    return (255 - (strength >> 1)) << 8;  // the §12 tint law verbatim
  };
  const auto sheet_at = [&](int i, int j) -> uint8_t {
    if (sheet == nullptr) return 0;
    const int64_t cwxc = (static_cast<int64_t>(wx[i]) + wx[i + 1]) / 2;
    const int64_t cwzc = (static_cast<int64_t>(wz[j]) + wz[j + 1]) / 2;
    return sample_sheet(*sheet, patch, fx16{static_cast<int32_t>(cwxc)},
                        fx16{static_cast<int32_t>(cwzc)});
  };

  // strata/underside placeholder colours derived from the patch material
  // (deterministic integer scales; the untextured stand-in — the reserved
  // strata tiles carry the look when the island brings its tileset)
  const uint8_t wall_r = static_cast<uint8_t>((mat.r * 200 + 128) >> 8);
  const uint8_t wall_g = static_cast<uint8_t>((mat.g * 200 + 128) >> 8);
  const uint8_t wall_b = static_cast<uint8_t>((mat.b * 200 + 128) >> 8);
  const uint8_t under_r = static_cast<uint8_t>((mat.r * 140 + 128) >> 8);
  const uint8_t under_g = static_cast<uint8_t>((mat.g * 140 + 128) >> 8);
  const uint8_t under_b = static_cast<uint8_t>((mat.b * 140 + 128) >> 8);

  for (const Prim& prim : prims) {
    if (prim.kind == 2) {
      // ---- rim wall quad (FORGE.CLIFF law): top edge on the composed top,
      // bottom edge on the modelled underside; vertex order per side keeps
      // the flat-shade normal pointing OUT of the solid cell ----
      const size_t va = prim.index;
      const size_t vb = prim.index2;
      const int32_t ax = wx[va % w], az = wz[va / w];
      const int32_t bx = wx[vb % w], bz = wz[vb / w];
      const int32_t shade =
          ambient(shade_points(ax, y[va], az, bx, y[vb], bz, bx, lat.bottom[vb], bz));
      if (textured) {
        TextureSpan span;
        span.ts = tileset;
        span.tile_a = 240;  // the rim strata tile (§6.6 frozen assignment)
        span.mosaic = false;
        // unity tint on walls/underside: the Phase-3 flat LMAP stand-in
        // rides the TOP surface only (palette law - a second tint family
        // here doubled the modulated colour count past 256)
        span.mod_r = mod_of(shade, 65536, 65536);
        span.mod_g = mod_of(shade, 65536, 65536);
        span.mod_b = mod_of(shade, 65536, 65536);
        // U spans the accumulated rim length (prim.u0/u1); V = (top-y)/8 per
        // vertex: 0 at the top edge, thickness/8 at the modelled bottom —
        // mirrored repeat turns the strata tile into geology (§5)
        ScreenV ta = sv[va], tb = sv[vb], ba = svb[va], bb = svb[vb];
        ta.u = prim.u0;
        ta.v = 0;
        tb.u = prim.u1;
        tb.v = 0;
        ba.u = prim.u0;
        ba.v = (y[va] - lat.bottom[va]) >> 3;
        bb.u = prim.u1;
        bb.v = (y[vb] - lat.bottom[vb]) >> 3;
        raster_tri(surf, vpp, ta, tb, bb, 0, 0, 0, mode, &span);
        raster_tri(surf, vpp, ta, bb, ba, 0, 0, 0, mode, &span);
        continue;
      }
      const auto lit = [shade](uint8_t base) {
        return static_cast<uint8_t>((static_cast<int32_t>(base) * shade + 32768) >> 16);
      };
      const uint8_t r = lit(wall_r), g = lit(wall_g), b = lit(wall_b);
      raster_tri(surf, vpp, sv[va], sv[vb], svb[vb], r, g, b, mode);
      raster_tri(surf, vpp, sv[va], svb[vb], svb[va], r, g, b, mode);
      continue;
    }
    const uint32_t cell = prim.index;
    const int i = static_cast<int>(cell) % w;
    const int j = static_cast<int>(cell) / w;
    const size_t i00 = cell;
    const size_t i10 = i00 + 1;
    const size_t i01 = i00 + w;
    const size_t i11 = i01 + 1;

    if (prim.kind == 0) {
      // ---- top surface cell (the pre-migration path, byte-identical when
      // untextured) ---- sheet tint at the cell centre (charter §12)
      int32_t tint = 255;
      if (sheet != nullptr) tint = 255 - (sheet_at(i, j) >> 1);
      const TextureSpan* spanp = nullptr;
      TextureSpan span;
      if (textured) {
        const size_t c = static_cast<size_t>(j) * (w - 1) + i;
        span.ts = tileset;
        span.tile_a = patch.mat_a[c];
        span.tile_b = patch.mat_b[c];
        span.weight = patch.mat_w[c];
        span.mosaic = true;  // the per-texel pick (§6.2)
        int32_t tr = 65536, tg = 65536, tb = 65536;
        if (has_tint) cell_tint(i00, &tr, &tg, &tb);
        const int32_t sq = sheet_factor(sheet_at(i, j));
        const int32_t shade = shade_tri(y, i00, i11, i10);
        span.mod_r = mod_of(shade, tr, sq);
        span.mod_g = mod_of(shade, tg, sq);
        span.mod_b = mod_of(shade, tb, sq);
        spanp = &span;
        tint = 255;  // the sheet rides span.mod_* on the textured path
      }
      const auto emit = [&](const size_t ia, const size_t ib, const size_t ic) {
        if (spanp != nullptr) {
          ScreenV a = sv[ia], b = sv[ib], c = sv[ic];
          a.u = u_top[ia];
          a.v = v_top[ia];
          b.u = u_top[ib];
          b.v = v_top[ib];
          c.u = u_top[ic];
          c.v = v_top[ic];
          raster_tri(surf, vpp, a, b, c, 0, 0, 0, mode, spanp);
          return;
        }
        const int32_t shade = shade_tri(y, ia, ib, ic);
        // tint only when a sheet exists: even t = 255 would darken by ~0.4%
        // ((v*255+128)>>8 < v for v >= 129), shifting every unstamped colour
        const auto tinted = [tint, shade, sheet](uint8_t base) {
          const int32_t lit = (static_cast<int32_t>(base) * shade + 32768) >> 16;
          if (sheet == nullptr) return static_cast<uint8_t>(lit);
          return static_cast<uint8_t>((lit * tint + 128) >> 8);
        };
        const uint8_t r = tinted(mat.r);
        const uint8_t g = tinted(mat.g);
        const uint8_t b = tinted(mat.b);
        raster_tri(surf, vpp, sv[ia], sv[ib], sv[ic], r, g, b, mode);
      };
      // y-up winding: e1 x e2 = +Y for a flat cell — the flat-shade normal
      // points UP, so the island top lights up (kLightY term).
      emit(i00, i11, i10);
      emit(i00, i01, i11);
    } else {
      // ---- underside cell (bottom lattice, same §4.3 diagonal, inverted
      // winding: TERRAIN.TESS law) ----
      const TextureSpan* spanp = nullptr;
      TextureSpan span;
      if (textured) {
        span.ts = tileset;
        span.tile_a = 241;  // the underside tile (§6.6 frozen assignment)
        span.mosaic = false;
        const int32_t shade = ambient(shade_tri(lat.bottom, i00, i10, i11));
        span.mod_r = mod_of(shade, 65536, 65536);
        span.mod_g = mod_of(shade, 65536, 65536);
        span.mod_b = mod_of(shade, 65536, 65536);
        spanp = &span;
      }
      const auto emit_b = [&](const size_t ia, const size_t ib, const size_t ic) {
        if (spanp != nullptr) {
          ScreenV a = svb[ia], b = svb[ib], c = svb[ic];
          a.u = u_und[ia];
          a.v = v_und[ia];
          b.u = u_und[ib];
          b.v = v_und[ib];
          c.u = u_und[ic];
          c.v = v_und[ic];
          raster_tri(surf, vpp, a, b, c, 0, 0, 0, mode, spanp);
          return;
        }
        const int32_t shade =
            ambient(shade_points(wx[ia % w], lat.bottom[ia], wz[ia / w], wx[ib % w], lat.bottom[ib],
                                 wz[ib / w], wx[ic % w], lat.bottom[ic], wz[ic / w]));
        const auto lit = [shade](uint8_t base) {
          return static_cast<uint8_t>((static_cast<int32_t>(base) * shade + 32768) >> 16);
        };
        raster_tri(surf, vpp, svb[ia], svb[ib], svb[ic], lit(under_r), lit(under_g), lit(under_b),
                   mode);
      };
      emit_b(i00, i10, i11);  // inverted relative to the top: normal points DOWN
      emit_b(i00, i11, i01);
    }
  }
}

}  // namespace render
}  // namespace zref
