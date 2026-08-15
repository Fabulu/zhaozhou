// terrain.cpp — heightfield patch draw (DrawProcedural forge_kind 0), earth
// field application (TerrainField) and the surface sheet (SurfaceStamp).
//
// Law:
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
namespace {

// a + (b-a)*num/den, round-half-up, one rounding (qformats §4; den > 0).
inline int32_t grid_lerp(int32_t a, int32_t b, int32_t num, int32_t den) {
  const int64_t span = static_cast<int64_t>(b) - a;
  const int64_t v = a + (span * num + den / 2) / den;
  return static_cast<int32_t>(v);
}

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

// ------------------------------------------------------------ heightfield

void draw_heightfield(WorkSurface& surf, const Viewport& vpp, const mat4fx& vp,
                      const TerrainPatch& patch, const ZhTransform2fx& xform, const Material& mat,
                      const SurfaceSheet* sheet, const std::vector<FieldApp>& fields,
                      uint32_t frame_tick, std::vector<TerrainVelocitySample>* velocity_out,
                      SatLedger* L) {
  const int w = patch.width;
  const int h = patch.height;
  if (w < 2 || h < 2) return;  // a degenerate patch draws nothing

  // world grid: envelope lerp (one rounding per interior line, §4), then the
  // transform2fx placement (one rounding per vertex component, §3)
  std::vector<int32_t> wx(static_cast<size_t>(w));
  std::vector<int32_t> wz(static_cast<size_t>(h));
  for (int i = 0; i < w; ++i)
    wx[i] = place_x(xform, fx16{grid_lerp(patch.env_x0, patch.env_x1, i, w - 1)}, fx16{0}, L).raw;
  for (int j = 0; j < h; ++j)
    wz[j] = place_z(xform, fx16{0}, fx16{grid_lerp(patch.env_z0, patch.env_z1, j, h - 1)}, L).raw;

  // per-column height: authored height16 (exact <<8, §9) + field deltas
  std::vector<int32_t> y(static_cast<size_t>(w) * h);
  for (size_t k = 0; k < y.size(); ++k) y[k] = static_cast<int32_t>(patch.heights[k]) << 8;

  // TerrainField application: apps in command order; columns ascending
  // z-then-x (the cartridge patch order) — the recorded velocity order.
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
        const int32_t cx = wx[i], cz = wz[j];
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
        y[idx] = fx_add(fx16{y[idx]}, fx16{out[0]}, L).raw;  // height lane
        if (velocity_out != nullptr) {
          velocity_out->push_back(TerrainVelocitySample{cx, cz, field_velocity_lane(out)});
        }
      }
    }
  }

  // project the grid once per view call
  std::vector<ScreenV> sv(static_cast<size_t>(w) * h);
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
    }
  }

  // painter's order: far cells first (small 1/w), ties by cell index (D7)
  struct Cell {
    int32_t depth;
    uint32_t index;
  };
  std::vector<Cell> cells;
  cells.reserve(static_cast<size_t>((w - 1) * (h - 1)));
  for (int j = 0; j + 1 < h; ++j) {
    for (int i = 0; i + 1 < w; ++i) {
      const size_t i00 = static_cast<size_t>(j) * w + i;
      const size_t i10 = i00 + 1;
      const size_t i01 = i00 + w;
      const size_t i11 = i01 + 1;
      const int32_t d = (sv[i00].d + sv[i10].d + sv[i01].d + sv[i11].d) / 4;
      cells.push_back(Cell{d, static_cast<uint32_t>(i00)});
    }
  }
  std::stable_sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
    return a.depth != b.depth ? a.depth < b.depth : a.index < b.index;
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
  for (const Cell& cell : cells) {
    const int i = static_cast<int>(cell.index) % w;
    const int j = static_cast<int>(cell.index) / w;
    const size_t i00 = cell.index;
    const size_t i10 = i00 + 1;
    const size_t i01 = i00 + w;
    const size_t i11 = i01 + 1;

    // flat shading per triangle: world-space normal from the EXACT cross
    // product of fx16 edge vectors (s64 32.32 lanes, no rounding), then the
    // §7.4-style normalize (3 rescales + isqrt_u64 §7.2) and ONE rounded
    // dot (§3 single-rounding law).
    // returns the lambert weight in Q16.16 (0..0x10000), NOT u8 — the
    // colour modulation consumes it as a 16.16 factor below
    const auto shade_tri = [&](const size_t ia, const size_t ib, const size_t ic) -> int32_t {
      // grid-space edges (transform already applied to x/z; y from heights)
      const int32_t ax = wx[ia % w], az = wz[ia / w], ay = y[ia];
      const int32_t bx = wx[ib % w], bz = wz[ib / w], by = y[ib];
      const int32_t cx = wx[ic % w], cz = wz[ic / w], cy = y[ic];
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
                            static_cast<__int128>(fy) * kLightY +
                            static_cast<__int128>(fz) * kLightZ;
      const uint64_t nmag2 = static_cast<uint64_t>(fx) * static_cast<uint64_t>(fx) +
                             static_cast<uint64_t>(fy) * static_cast<uint64_t>(fy) +
                             static_cast<uint64_t>(fz) * static_cast<uint64_t>(fz);
      if (nmag2 == 0) return 0;  // exactly degenerate (zero-area) triangle
      // scale check: n_fx is Q16.16, so ndot = n.L is Q32.32 raw and
      // nmag2 = |n_fx|^2 is Q32.32 raw, hence isqrt_u64(nmag2) = |n_fx| is
      // Q16.16 raw — ndot/nmag is exactly (nhat.L) in Q16.16 (§4 one
      // rounding). No extra shift. (This paragraph was already right; the
      // rescale above was the lie.)
      const int32_t shade = div_rhu_s128(ndot, static_cast<__int128>(isqrt_u64(nmag2)));
      return shade < 0 ? 0 : (shade > 0x10000 ? 0x10000 : shade);
    };

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
  }
}

}  // namespace render
}  // namespace zref
