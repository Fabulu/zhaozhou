// zref_sky.hpp — W3.5 sky ZRef: zref::sky::emit_layers, the six-layer sky
// emitter the Phase-3 software renderer consumes (plan W3.5 / D7).
//
// Law (in citation order):
//   spec/sky_and_beams.md — §1   hybrid pass placement (pass 1 backdrop
//                                prefill, pass 3 under-plane, pass 6 cloud +
//                                sun; deterministic sub-order sun before
//                                cloud), assembly anchored to map centre,
//                                world-fixed, rotation-only rot_proj, the
//                                fallback flat-clear rule, "no pixel is ever
//                                left unwritten"
//                          §1.1 the layer table (drum bands 48 cols / 8
//                                mirrored repeats, zenith cap 16-tri fan,
//                                under-plane 10240x10240 at z-2560, cloud
//                                sheet 8x8 vertex grid = 128 tris UV 0..4
//                                with vertex alpha (1-r^2)*max_alpha baked in
//                                fx16, sun quad world-fixed at z+2560, the
//                                tick-exact scroll formulas
//                                scroll_u = ((tick % 3840) << 16)/3840 (floor),
//                                scroll_v = -scroll_u)
//                          §6   ZRef preview functions: emit_layers /
//                                cloud_vertex_alpha, "integer-only, single
//                                rounding law"
//   spec/qformats.md      — §2/§3 fx16 arithmetic, §7.1 fx_sin/fx_cos for the
//                                drum column placement, §7.4 normalize3_approx
//                                  for the sun anchor direction
//   spec/commands.zidl    — DrawSky 0x0310 payload (rot_proj[2] rotation-only
//                                validated by the RENDERER per sky_and_beams
//                                §1; drum_yaw; viewport_mask; layer flags
//                                b0 under, b1 cloud, b2 sun, b3 cap, b4
//                                sun_glow_tag)
//
// Phase-3 scope note: the sky spec's textures (CLUT8 drum, ARGB4444
// cloud/sun) are asset-compile products that do not exist yet (W3.6 packer,
// W3.7 demo). emit_layers emits the full GEOMETRY + UV + vertex-alpha set —
// the UV laws are complete and byte-testable today (sky_scroll_determinism)
// — while the raster stage shades layers with flat per-set colours until
// the SKY_SET page (spec/cartridge.md §4 kind 3) carries real texels. Every
// deviation is marked [w3.5-software] in the .cpp.

#pragma once

#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"

#include <cstdint>
#include <vector>

namespace zref {
namespace sky {

// ---- §1.1 layer table constants (frozen by the spec, restated verbatim) ----

inline constexpr int kDrumCols = 48;                         // sky_drum band columns
inline constexpr int kDrumMirrorRepeats = 8;                 // u-mirror repeats per band
inline constexpr int kBandRows = 8;                          // rows per band (spec: 8 mirrored
                                                             // repeats over the circumference;
                                                             // the vertical subdivision is the
                                                             // software stand-in for the 128-tex
                                                             // v axis, [w3.5-software])
inline constexpr int32_t kSkyMinY = -INT32_C(2560) * 65536;  // fx16 raw
inline constexpr int32_t kSkyMidY = 0;
inline constexpr int32_t kSkyMaxY = INT32_C(2560) * 65536;
inline constexpr int32_t kLowerInnerR = INT32_C(4096) * 65536;  // y = -2560
inline constexpr int32_t kDrumR = INT32_C(5120) * 65536;        // y >= 0
inline constexpr int32_t kUnderHalf = INT32_C(5120) * 65536;    // 10240x10240
inline constexpr int32_t kCloudY = INT32_C(1792) * 65536;
inline constexpr int32_t kSunR = INT32_C(4096) * 65536;    // drum anchor radius
inline constexpr int32_t kSunHalf = INT32_C(512) * 65536;  // quad half-size

// DrawSky layer enable flags (spec/commands.zidl DrawSky.flags).
inline constexpr uint8_t kLayerUnder = 0x01;
inline constexpr uint8_t kLayerCloud = 0x02;
inline constexpr uint8_t kLayerSun = 0x04;
inline constexpr uint8_t kLayerCap = 0x08;
inline constexpr uint8_t kLayerSunGlowTag = 0x10;

// ---- asset set --------------------------------------------------------------
//
// The SkySet is the renderer-side shape of the cartridge SKY_SET page
// (spec/cartridge.md §4 kind 3: "the page shape is the sky spec's own asset
// layout" — this struct IS that layout at Phase 3; W3.6 maps page bytes onto
// it 1:1). Colours are RGB888 working values (charter §8 active-tile
// storage); the texture fields arrive with the asset compiler.

struct SkyColor {
  uint8_t r, g, b;
};

struct SkySet {
  SkyColor background;  // §1 fallback flat-clear colour
  // drum band gradients (lower: horizon -> top; upper: bottom -> zenith)
  SkyColor band_lower_horizon;
  SkyColor band_lower_top;
  SkyColor band_upper_bottom;
  SkyColor band_upper_top;
  SkyColor cap;                         // zenith cap flat colour
  SkyColor under;                       // under-plane (fog-exempt, §1.1)
  SkyColor cloud;                       // cloud sheet source colour
  SkyColor sun;                         // sun quad source colour
  fx16 cloud_max_alpha = fx16{0x8000};  // §1.1 cloud: max_alpha per set
  fx16 sun_energy = fx16{0x10000};      // §1.1 sun: energy per set
  fx16 sun_dir_x = fx16{0};             // world-fixed sun anchor direction
  fx16 sun_dir_z = fx16{INT32_C(-1) << 16};
  fx16 map_cx = fx16{0};  // assembly anchor = map centre (§1)
  fx16 map_cz = fx16{0};
};

// ---- emitted primitives -----------------------------------------------------

enum class SkyLayer : uint8_t {
  BandLower = 0,  // pass 1 (§1.1 layer table row 1)
  BandUpper = 1,  // pass 1 (row 2)
  Cap = 2,        // pass 1 (row 3)
  Under = 3,      // pass 3 (row 4)
  Cloud = 4,      // pass 6 (row 5)
  Sun = 5,        // pass 6 (row 6; drawn BEFORE cloud, §1 pass-6 sub-order)
};

struct SkyVertex {
  fx16 x, y, z;   // world (y up), anchored to SkySet.map_c*
  fx16 u, v;      // texture UV in turns (repeat), scroll already applied
  uint8_t alpha;  // baked vertex alpha (cloud law; 255 elsewhere)
};

struct SkyPrimitive {
  SkyLayer layer;
  uint8_t row;  // band row 0..7 (flat-colour gradient index); 0 otherwise
  SkyVertex v[3];
};

// §1.1 cloud vertex-alpha law: alpha = (1 - r^2) * max_alpha computed in fx16
// (single rounding via fx_mul, qformats §3) then narrowed to u8 (§2
// conversion). r2 = squared normalized distance from the sheet centre.
inline uint8_t cloud_vertex_alpha(fx16 r2, fx16 max_alpha, SatLedger* L) {
  const fx16 one = fx16{1 << 16};
  const fx16 a = fx_mul(fx_sub(one, fx_clamp(r2, fx16{0}, one), L), max_alpha, L);
  return unit8_from_fx16(fx_clamp(a, fx16{0}, one), L).raw;
}

// §1.1 tick-exact cloud scroll (frozen): scroll_u = ((tick % 3840) << 16) /
// 3840 (floor); scroll_v = -scroll_u. 1 tile / 64 s at 60 Hz, direction
// (1, -1). Tick T and T+3840 produce byte-identical offsets by construction.
inline fx16 cloud_scroll_u(uint32_t tick) {
  return fx16{static_cast<int32_t>(((tick % 3840u) << 16) / 3840u)};
}
inline fx16 cloud_scroll_v(uint32_t tick) { return fx16{-cloud_scroll_u(tick).raw}; }

/**
 * §6 emit_layers — emit the enabled layer geometry for one frame.
 *
 * Every vertex carries the layer-table UV law (bands: u-mirror over 8
 * repeats + drum_yaw-rotated geometry, v = band height fraction; cap/under:
 * planar map; cloud: UV 0..4 + tick-exact scroll) and the baked cloud vertex
 * alpha. The renderer applies the per-view rotation-only rot_proj and the
 * pass placement (§1); emission itself is view-independent at Phase 3, so
 * the spec's `view` argument is carried as an ignored parameter to keep the
 * frozen signature (recorded as [w3.5-software] in emit_layers.cpp).
 *
 * layer_flags = DrawSky.flags (bands are the pass-1 backdrop: they are always
 * emitted — the flag bits gate under/cloud/sun/cap per the .zidl bit table;
 * a disabled layer leaves its directions on the flat clear, §1 fallback).
 */
std::vector<SkyPrimitive> emit_layers(const SkySet& set, uint32_t tick, angle16 drum_yaw,
                                      uint8_t layer_flags, int view = 0);

/**
 * §1 rot_proj validation: a legal per-view sky matrix is rotation-only —
 * translation column (m03,m13,m23) zero, bottom row (m30..m32) zero, m33 =
 * 1. Returns false when violated (the caller then takes the fallback
 * flat-clear path, never renders a sheared sky).
 */
bool rot_proj_is_rotation_only(const mat4fx& m);

}  // namespace sky
}  // namespace zref
