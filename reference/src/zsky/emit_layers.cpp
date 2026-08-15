// emit_layers.cpp — zref::sky::emit_layers (spec/sky_and_beams.md §1.1/§6).
//
// Law (citation order):
//   sky_and_beams.md §1.1  the layer table — every constant below is
//                         restated from a table row, never invented:
//                         lower band z -2560..0 r 4096->5120, upper band
//                         z 0..+2560 r 5120, both 48 cols / 8 mirrored
//                         u-repeats / inside-facing, zenith cap 16-tri fan
//                         z +2560, under-plane 10240x10240 quad z -2560,
//                         cloud sheet 8x8 cell grid (128 tris) z +1792
//                         UV 0..4 with alpha = (1-r^2)*max_alpha, sun quad
//                         z +2560 world-fixed (NOT billboarded)
//           §1            assembly anchored to map centre (SkySet.map_c*),
//                         drum_yaw rotates the drum geometry (the UVs stay
//                         glued to the columns, so the texture visibly
//                         rotates with the drum)
//           §6            this file is the named ZRef preview implementation
//   qformats.md  §3       single-rounding fx16 arithmetic (fx_mul/fx_mad)
//           §7.1          fx_sin/fx_cos quarter-wave table for column angles
//           §7.4          normalize3_approx for the sun anchor direction
//
// [w3.5-software] deviations, each visible to W3.6/W3.7:
//   * the spec's `view` parameter is accepted and unused — per-view state is
//     the rot_proj the RENDERER applies (sky_and_beams.md §1); emission is
//     view-independent at Phase 3;
//   * the band vertical subdivision is 8 rows (the v texture is 128 texels;
//     without texels the rows exist to give the flat-colour gradient steps);
//   * the sun quad is a 4-tri fan around its centre vertex so the additive
//     radial falloff can ride the vertex-alpha interpolator (the real falloff
//     is baked into the ARGB4444 alpha at asset compile, §1.1 row 6).

#include "zref/zref_sky.hpp"

namespace zref {
namespace sky {
namespace {

// Band u law (§1.1): u-mirror over kDrumMirrorRepeats repeats. s = a*8 in
// texture turns (a = circumferential parameter in [0,1)); each repeat pair
// mirrors: phase = s mod 2, u = phase <= 1 ? phase : 2 - phase. One fx_mul
// rounding total (§3); the mod/mirror are exact integer steps.
fx16 band_u(fx16 a) {
  const int32_t s = fx_mul(a, fx16{kDrumMirrorRepeats << 16}, nullptr).raw;
  const int32_t phase = s & ((2 << 16) - 1);  // s mod 2, s >= 0 by caller
  const int32_t u = (phase <= (1 << 16)) ? phase : (2 << 16) - phase;
  return fx16{u};
}

// One drum vertex: world position at angle `a` (turns), radius r, height y,
// anchored to the map centre. One fx_mad per horizontal component (§3).
SkyVertex drum_vertex(fx16 cx, fx16 cz, angle16 a, fx16 r, fx16 y, fx16 u, fx16 v) {
  return SkyVertex{
      fx_mad(fx_cos(a), r, cx, nullptr), y, fx_mad(fx_sin(a), r, cz, nullptr), u, v, 0xFF};
}

// lerp t in [0,1] fx16 between two fx16 (one fx_mad rounding, §3).
fx16 lerp_fx(fx16 t, fx16 a, fx16 b) { return fx_mad(t, fx_sub(b, a, nullptr), a, nullptr); }

}  // namespace

bool rot_proj_is_rotation_only(const mat4fx& m) {
  const int32_t one = 1 << 16;
  return m.m[0][3].raw == 0 && m.m[1][3].raw == 0 && m.m[2][3].raw == 0 && m.m[3][0].raw == 0 &&
         m.m[3][1].raw == 0 && m.m[3][2].raw == 0 && m.m[3][3].raw == one;
}

std::vector<SkyPrimitive> emit_layers(const SkySet& set, uint32_t tick, angle16 drum_yaw,
                                      uint8_t layer_flags,
                                      int /*view — [w3.5-software], see header*/) {
  std::vector<SkyPrimitive> out;
  const fx16 cx = set.map_cx;
  const fx16 cz = set.map_cz;

  // ---- pass 1: drum bands (§1.1 rows 1-2; always emitted — backdrop) -----
  for (int band = 0; band < 2; ++band) {
    const SkyLayer layer = band == 0 ? SkyLayer::BandLower : SkyLayer::BandUpper;
    const fx16 y0 = fx16{band == 0 ? kSkyMinY : kSkyMidY};
    const fx16 y1 = fx16{band == 0 ? kSkyMidY : kSkyMaxY};
    const fx16 r0 = fx16{band == 0 ? kLowerInnerR : kDrumR};
    const fx16 r1 = fx16{kDrumR};  // both bands close at the drum radius
    for (int col = 0; col < kDrumCols; ++col) {
      // circumferential parameter of the two column edges (local, no yaw)
      const fx16 a_l = fx16{(static_cast<int32_t>(col) << 16) / kDrumCols};
      const fx16 a_r = fx16{(static_cast<int32_t>(col + 1) << 16) / kDrumCols};
      const fx16 u_l = band_u(a_l);
      const fx16 u_r = band_u(a_r);
      // geometry angles add drum_yaw (angle16 arithmetic wraps mod 2^16,
      // qformats §2 — the angle lane is the u16 turns container itself)
      const angle16 ga_l = angle16{static_cast<uint16_t>(drum_yaw.raw + (a_l.raw >> 16))};
      const angle16 ga_r = angle16{static_cast<uint16_t>(drum_yaw.raw + (a_r.raw >> 16))};
      for (int row = 0; row < kBandRows; ++row) {
        const fx16 t0 = fx16{(row << 16) / kBandRows};
        const fx16 t1 = fx16{((row + 1) << 16) / kBandRows};
        // v-clamp: band height fraction [0,1]; radius lerps across the band
        const SkyVertex bl =
            drum_vertex(cx, cz, ga_l, lerp_fx(t0, r0, r1), lerp_fx(t0, y0, y1), u_l, t0);
        const SkyVertex tl =
            drum_vertex(cx, cz, ga_l, lerp_fx(t1, r0, r1), lerp_fx(t1, y0, y1), u_l, t1);
        const SkyVertex br =
            drum_vertex(cx, cz, ga_r, lerp_fx(t0, r0, r1), lerp_fx(t0, y0, y1), u_r, t0);
        const SkyVertex tr =
            drum_vertex(cx, cz, ga_r, lerp_fx(t1, r0, r1), lerp_fx(t1, y0, y1), u_r, t1);
        // inside-facing winding recorded for the Phase-11 RTL freeze; the
        // Phase-3 raster is double-sided (rast.cpp) so both splits shade.
        out.push_back(SkyPrimitive{layer, static_cast<uint8_t>(row), {bl, br, tr}});
        out.push_back(SkyPrimitive{layer, static_cast<uint8_t>(row), {bl, tr, tl}});
      }
    }
  }

  // ---- pass 1: zenith cap (§1.1 row 3; gated by kLayerCap) ---------------
  if (layer_flags & kLayerCap) {
    for (int k = 0; k < 16; ++k) {
      const angle16 a0 = angle16{static_cast<uint16_t>((k * 65536u) / 16u)};
      const angle16 a1 = angle16{static_cast<uint16_t>(((k + 1) * 65536u) / 16u)};
      // planar UV over the 64x64 clamp texture: direction/2 + 1/2
      const fx16 u0 = lerp_fx(fx_cos(a0), fx16{0}, fx16{1 << 16});
      const fx16 u1 = lerp_fx(fx_cos(a1), fx16{0}, fx16{1 << 16});
      const fx16 v0 = lerp_fx(fx_sin(a0), fx16{0}, fx16{1 << 16});
      const fx16 v1 = lerp_fx(fx_sin(a1), fx16{0}, fx16{1 << 16});
      out.push_back(
          SkyPrimitive{SkyLayer::Cap,
                       0,
                       {SkyVertex{cx, fx16{kSkyMaxY}, cz, fx16{1 << 15}, fx16{1 << 15}, 0xFF},
                        SkyVertex{fx_mad(fx_cos(a0), fx16{kDrumR}, cx, nullptr), fx16{kSkyMaxY},
                                  fx_mad(fx_sin(a0), fx16{kDrumR}, cz, nullptr), u0, v0, 0xFF},
                        SkyVertex{fx_mad(fx_cos(a1), fx16{kDrumR}, cx, nullptr), fx16{kSkyMaxY},
                                  fx_mad(fx_sin(a1), fx16{kDrumR}, cz, nullptr), u1, v1, 0xFF}}});
    }
  }

  // ---- pass 3: under-plane (§1.1 row 4; 10240x10240 quad, 2 tris) --------
  if (layer_flags & kLayerUnder) {
    const fx16 x0 = fx_sub(cx, fx16{kUnderHalf}, nullptr);
    const fx16 x1 = fx_add(cx, fx16{kUnderHalf}, nullptr);
    const fx16 z0 = fx_sub(cz, fx16{kUnderHalf}, nullptr);
    const fx16 z1 = fx_add(cz, fx16{kUnderHalf}, nullptr);
    const fx16 y = fx16{kSkyMinY};
    // clamp-texture UV: 0..1 across the quad (512x512 texels Phase-3-untextured)
    const SkyVertex v00{x0, y, z0, fx16{0}, fx16{0}, 0xFF};
    const SkyVertex v10{x1, y, z0, fx16{1 << 16}, fx16{0}, 0xFF};
    const SkyVertex v11{x1, y, z1, fx16{1 << 16}, fx16{1 << 16}, 0xFF};
    const SkyVertex v01{x0, y, z1, fx16{0}, fx16{1 << 16}, 0xFF};
    out.push_back(SkyPrimitive{SkyLayer::Under, 0, {v00, v10, v11}});
    out.push_back(SkyPrimitive{SkyLayer::Under, 0, {v00, v11, v01}});
  }

  // ---- pass 6: cloud sheet (§1.1 row 5; 8x8 cells = 128 tris) ------------
  if (layer_flags & kLayerCloud) {
    const fx16 su = cloud_scroll_u(tick);
    const fx16 sv = cloud_scroll_v(tick);
    constexpr int n = 8;  // 8x8 cells -> 9x9 vertices -> 128 tris (§1.1)
    SkyVertex grid[(n + 1) * (n + 1)];
    for (int j = 0; j <= n; ++j) {
      for (int i = 0; i <= n; ++i) {
        // UV 0..4 over the sheet (§1.1), tick-exact scroll applied here so
        // consumers see the frozen law directly (u/v-repeat stays theirs)
        const int32_t uraw = su.raw + (static_cast<int32_t>(i) * 4 * (1 << 16)) / n;
        const int32_t vraw = sv.raw + (static_cast<int32_t>(j) * 4 * (1 << 16)) / n;
        const fx16 x = fx_sub(
            fx_add(cx, fx16{static_cast<int32_t>((static_cast<int64_t>(i) * 2 * kUnderHalf) / n)},
                   nullptr),
            fx16{kUnderHalf}, nullptr);
        const fx16 z = fx_sub(
            fx_add(cz, fx16{static_cast<int32_t>((static_cast<int64_t>(j) * 2 * kUnderHalf) / n)},
                   nullptr),
            fx16{kUnderHalf}, nullptr);
        // r^2 for the vertex-alpha law, normalized so the sheet edge (not the
        // corner) hits r^2 = 1: (dx^2 + dz^2)/half^2, half = kUnderHalf.
        // Corner r^2 = 2 clamps to alpha 0 inside cloud_vertex_alpha.
        const int64_t dx = fx_sub(x, cx, nullptr).raw;
        const int64_t dz = fx_sub(z, cz, nullptr).raw;
        const int64_t num = ((dx * dx) >> 16) + ((dz * dz) >> 16);  // fx scale
        const int64_t den = (static_cast<int64_t>(kUnderHalf) * kUnderHalf) >> 16;
        const fx16 r2 = fx16{static_cast<int32_t>((num << 16) / den)};
        grid[j * (n + 1) + i] = SkyVertex{
            x,          fx16{kCloudY}, z,
            fx16{uraw}, fx16{vraw},    cloud_vertex_alpha(r2, set.cloud_max_alpha, nullptr)};
      }
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        const SkyVertex& v00 = grid[j * (n + 1) + i];
        const SkyVertex& v10 = grid[j * (n + 1) + i + 1];
        const SkyVertex& v11 = grid[(j + 1) * (n + 1) + i + 1];
        const SkyVertex& v01 = grid[(j + 1) * (n + 1) + i];
        out.push_back(SkyPrimitive{SkyLayer::Cloud, 0, {v00, v10, v11}});
        out.push_back(SkyPrimitive{SkyLayer::Cloud, 0, {v00, v11, v01}});
      }
    }
  }

  // ---- pass 6: sun (§1.1 row 6; world-fixed, NOT billboarded) ------------
  if (layer_flags & kLayerSun) {
    // anchor direction normalized (§7.4), placed on the drum at kSunR
    const vec3fx d = normalize3_approx(vec3fx{set.sun_dir_x, fx16{0}, set.sun_dir_z}, nullptr);
    const fx16 px = fx_mad(d.x, fx16{kSunR}, cx, nullptr);
    const fx16 pz = fx_mad(d.z, fx16{kSunR}, cz, nullptr);
    const fx16 py = fx16{INT32_C(2048) << 16};  // mid upper band (z+2560 top)
    // centre vertex alpha = min(3*energy, 1) — the software stand-in for the
    // asset-baked min(3*lum, 1) alpha ([w3.5-software], §1.1 row 6)
    const fx16 e3 = fx_mul(set.sun_energy, fx16{3 << 16}, nullptr);
    const int32_t a_raw = e3.raw > (1 << 16) ? (1 << 16) : e3.raw;
    const uint8_t ca = static_cast<uint8_t>((a_raw + 128) >> 8);
    // horizontal tangent of the drum at the anchor: T = (d.z, 0, -d.x)
    const fx16 hx = fx_mul(d.z, fx16{kSunHalf}, nullptr);
    const fx16 hz = fx_mul(fx16{-d.x.raw}, fx16{kSunHalf}, nullptr);
    const fx16 hy = fx16{kSunHalf};
    const SkyVertex c{px, py, pz, fx16{1 << 15}, fx16{1 << 15}, ca};
    const SkyVertex t0{fx_add(px, hx, nullptr),
                       fx_add(py, hy, nullptr),
                       fx_add(pz, hz, nullptr),
                       fx16{0},
                       fx16{0},
                       0};
    const SkyVertex t1{fx_sub(px, hx, nullptr),
                       fx_add(py, hy, nullptr),
                       fx_sub(pz, hz, nullptr),
                       fx16{1 << 16},
                       fx16{0},
                       0};
    const SkyVertex t2{fx_sub(px, hx, nullptr), fx_sub(py, hy, nullptr), fx_sub(pz, hz, nullptr),
                       fx16{1 << 16},           fx16{1 << 16},           0};
    const SkyVertex t3{fx_add(px, hx, nullptr), fx_sub(py, hy, nullptr),
                       fx_add(pz, hz, nullptr), fx16{0},
                       fx16{1 << 16},           0};
    out.push_back(SkyPrimitive{SkyLayer::Sun, 0, {c, t0, t1}});
    out.push_back(SkyPrimitive{SkyLayer::Sun, 0, {c, t1, t2}});
    out.push_back(SkyPrimitive{SkyLayer::Sun, 0, {c, t2, t3}});
    out.push_back(SkyPrimitive{SkyLayer::Sun, 0, {c, t3, t0}});
  }

  return out;
}

}  // namespace sky
}  // namespace zref
