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
  const __int128 ndot = static_cast<__int128>(fx) * kLightX +
                        static_cast<__int128>(fy) * kLightY + static_cast<__int128>(fz) * kLightZ;
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
                      SatLedger* L) {
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

  // project the grid once per view call (top, and the bottom lattice when
  // the patch models an underside)
  std::vector<ScreenV> sv(static_cast<size_t>(w) * h);
  std::vector<ScreenV> svb;
  if (dual) svb.resize(static_cast<size_t>(w) * h);
  for (int j = 0; j < h; ++j) {
    for (int i = 0; i < w; ++i) {
      const size_t idx = static_cast<size_t>(j) * w + i;
      const ProjOut p = project_vertex(vp, vpp, fx16{wx[i]}, fx16{y[idx]}, fx16{wz[j]}, L);
      if (!p.in) {
        // Phase-3 near-plane law: ANY off-screen-behind vertex rejects the
        // whole patch (the island sits inside the frustum in the demo; the
        // clipper is Phase 4/5 charter §8 work). Deterministic either way.
        return;
      }
      sv[idx] = p.s;
      if (dual) {
        const ProjOut pb =
            project_vertex(vp, vpp, fx16{wx[i]}, fx16{lat.bottom[idx]}, fx16{wz[j]}, L);
        if (!pb.in) return;  // same Phase-3 near-plane law, bottom lattice
        svb[idx] = pb.s;
      }
    }
  }

  // painter's order: far primitives first (small 1/w), ties by kind then
  // index (D7; legacy pages carry a single kind, so the order is exactly the
  // pre-migration cell order). Kinds: 0 = top surface cell, 1 = underside
  // cell, 2 = rim wall (index = cell*4 + side).
  struct Prim {
    int32_t depth;
    uint8_t kind;
    uint32_t index;
  };
  std::vector<Prim> prims;
  prims.reserve(static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1));
  // substance of the neighbour across a cell edge; outside the patch = OUT
  // (absent patch = open sky, terrain_rules §3.2)
  const auto neighbour_solid = [&](int ci, int cj) {
    if (ci < 0 || cj < 0 || ci >= w - 1 || cj >= h - 1) return false;
    return lat.substance(ci, cj) == terrain::kSolid;
  };
  for (int j = 0; j + 1 < h; ++j) {
    for (int i = 0; i + 1 < w; ++i) {
      if (dual && lat.substance(i, j) != terrain::kSolid) continue;  // void: no surface
      const size_t i00 = static_cast<size_t>(j) * w + i;
      const size_t i10 = i00 + 1;
      const size_t i01 = i00 + w;
      const size_t i11 = i01 + 1;
      const int32_t d = (sv[i00].d + sv[i10].d + sv[i01].d + sv[i11].d) / 4;
      prims.push_back(Prim{d, 0, static_cast<uint32_t>(i00)});
      if (!dual) continue;
      const int32_t db = (svb[i00].d + svb[i10].d + svb[i01].d + svb[i11].d) / 4;
      prims.push_back(Prim{db, 1, static_cast<uint32_t>(i00)});
      // rim walls (FORGE.CLIFF law, terrain_rules §5): one quad per edge
      // against a void/OUT neighbour, top at the composed top, bottom at the
      // MODELLED underside. Sides: 0 = -z, 1 = +z, 2 = -x, 3 = +x.
      const size_t edge_v[4][2] = {{i00, i10}, {i11, i01}, {i01, i00}, {i10, i11}};
      const int noff[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
      for (int s = 0; s < 4; ++s) {
        if (neighbour_solid(i + noff[s][0], j + noff[s][1])) continue;
        const size_t va = edge_v[s][0], vb = edge_v[s][1];
        const int32_t dw = (sv[va].d + sv[vb].d + svb[va].d + svb[vb].d) / 4;
        prims.push_back(Prim{dw, 2, static_cast<uint32_t>(i00) * 4u + static_cast<uint32_t>(s)});
      }
    }
  }
  std::stable_sort(prims.begin(), prims.end(), [](const Prim& a, const Prim& b) {
    if (a.depth != b.depth) return a.depth < b.depth;
    if (a.kind != b.kind) return a.kind < b.kind;
    return a.index < b.index;
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
  // Returns the lambert weight in Q16.16 (0..0x10000), NOT u8 — the colour
  // modulation consumes it as a 16.16 factor below.
  const auto shade_points = [&](const int32_t ax, const int32_t ay, const int32_t az,
                                const int32_t bx, const int32_t by, const int32_t bz,
                                const int32_t cx, const int32_t cy, const int32_t cz) -> int32_t {
    return shade_flat_tri(ax, ay, az, bx, by, bz, cx, cy, cz, L);
  };
  // top-surface shading — the pre-migration arithmetic verbatim (shade_points
  // receives exactly the values the old closure read from the same arrays)
  const auto shade_tri = [&](const size_t ia, const size_t ib, const size_t ic) -> int32_t {
    return shade_points(wx[ia % w], y[ia], wz[ia / w], wx[ib % w], y[ib], wz[ib / w], wx[ic % w],
                        y[ic], wz[ic / w]);
  };
  // walls and undersides get an ambient floor (0.25 + 0.75*lambert): the
  // renderer's ONE light is high (1,2,1)/sqrt(6), so a raw lambert leaves a
  // down-facing keel pitch-black. Applies ONLY to the new dual-format
  // geometry — top-surface shading is untouched (golden CRC law). Recorded
  // as the Phase-3 software stand-in for the strata/underside TEXTURE lane
  // (terrain_rules §5/§6 — texturing is explicitly out of this wave).
  const auto ambient = [](int32_t shade) -> int32_t {
    return 16384 + static_cast<int32_t>((static_cast<int64_t>(shade) * 49152 + 32768) >> 16);
  };
  // strata/underside placeholder colours derived from the patch material
  // (deterministic integer scales; the tileset-reserved strata tiles land
  // with TEXTURE.MOSAIC)
  const uint8_t wall_r = static_cast<uint8_t>((mat.r * 200 + 128) >> 8);
  const uint8_t wall_g = static_cast<uint8_t>((mat.g * 200 + 128) >> 8);
  const uint8_t wall_b = static_cast<uint8_t>((mat.b * 200 + 128) >> 8);
  const uint8_t under_r = static_cast<uint8_t>((mat.r * 140 + 128) >> 8);
  const uint8_t under_g = static_cast<uint8_t>((mat.g * 140 + 128) >> 8);
  const uint8_t under_b = static_cast<uint8_t>((mat.b * 140 + 128) >> 8);

  for (const Prim& prim : prims) {
    const uint32_t cell = prim.kind == 2 ? prim.index / 4 : prim.index;
    const int i = static_cast<int>(cell) % w;
    const int j = static_cast<int>(cell) / w;
    const size_t i00 = cell;
    const size_t i10 = i00 + 1;
    const size_t i01 = i00 + w;
    const size_t i11 = i01 + 1;

    if (prim.kind == 0) {
      // ---- top surface cell (the pre-migration path, byte-identical) ----
      // sheet tint at the cell centre (charter §12 Phase-3 reading)
      int32_t tint = 255;
      if (sheet != nullptr) {
        const int64_t cwxc = (static_cast<int64_t>(wx[i]) + wx[i + 1]) / 2;
        const int64_t cwzc = (static_cast<int64_t>(wz[j]) + wz[j + 1]) / 2;
        const uint8_t strength = sample_sheet(*sheet, patch, fx16{static_cast<int32_t>(cwxc)},
                                              fx16{static_cast<int32_t>(cwzc)});
        tint = 255 - (strength >> 1);
      }
      const auto emit = [&](const size_t ia, const size_t ib, const size_t ic) {
        const int32_t shade = shade_tri(ia, ib, ic);
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
      // y-up winding: e1 x e2 = +Y for a flat cell (checked by hand:
      // (x+z) x x = z x x = +y, z x (x+z) = z x x = +y) — the flat-shade
      // normal points UP, so the island top lights up (kLightY term).
      emit(i00, i11, i10);
      emit(i00, i01, i11);
    } else if (prim.kind == 1) {
      // ---- underside cell (bottom lattice, same §4.3 diagonal, inverted
      // winding: TERRAIN.TESS law) ----
      const auto emit_b = [&](const size_t ia, const size_t ib, const size_t ic) {
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
    } else {
      // ---- rim wall quad (FORGE.CLIFF law): top edge on the composed top,
      // bottom edge on the modelled underside; vertex order per side keeps
      // the flat-shade normal pointing OUT of the solid cell ----
      const int side = static_cast<int>(prim.index & 3u);
      const size_t order[4][2] = {{i00, i10}, {i11, i01}, {i01, i00}, {i10, i11}};
      const size_t va = order[side][0], vb = order[side][1];
      const int32_t ax = wx[va % w], az = wz[va / w];
      const int32_t bx = wx[vb % w], bz = wz[vb / w];
      const int32_t shade =
          ambient(shade_points(ax, y[va], az, bx, y[vb], bz, bx, lat.bottom[vb], bz));
      const auto lit = [shade](uint8_t base) {
        return static_cast<uint8_t>((static_cast<int32_t>(base) * shade + 32768) >> 16);
      };
      const uint8_t r = lit(wall_r), g = lit(wall_g), b = lit(wall_b);
      raster_tri(surf, vpp, sv[va], sv[vb], svb[vb], r, g, b, mode);
      raster_tri(surf, vpp, sv[va], svb[vb], svb[va], r, g, b, mode);
    }
  }
}

}  // namespace render
}  // namespace zref
