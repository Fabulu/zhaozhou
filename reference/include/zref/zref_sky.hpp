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

inline constexpr int kDrumCols = 48;          // sky_drum band columns
inline constexpr int kDrumMirrorRepeats = 8;  // u-mirror repeats per band
inline constexpr int kBandRows = 16;          // rows per band — the software
// stand-in for the 128-texel v axis ([w3.5-software]): each row renders one
// flat colour, so the row count IS the gradient's quantisation until the
// drum textures exist. Raised 8 -> 16 (2026-08-16): at 8 the per-row colour
// delta (~8/255) exceeded one RGB565 step and the S1.2 elevation ramp read
// as discrete bands; at 16 the delta sits at the 565 quantum and the
// resolve's ordered dither absorbs it. Phase-6 drum textures supersede the
// rows entirely.
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

// ---- §4a environment state (amendment v1.2) ----------------------------------
//
// The ZRef mirror of SetEnvironment 0x0311 (spec/commands.zidl; law:
// spec/sky_and_beams.md 4a): the world's ONE light/environment record —
// sun direction (angle16 pair), sun colour / ambient / tint (RGB565
// working storage here, packed on the wire), tint strength, fog mode +
// extents. Serialized into the ENVIRONMENT_STATE .zcap chunk (0x000C,
// capture_format.md 4.2) as a byte-mirror of the command payload so the
// chunk and the command can never drift.
//
// The Phase-3 stand-in renderer does NOT consume this yet (its hard-coded
// light is terrain.cpp's 0.25 + 0.75*lambert floor — sky_and_beams 4a
// records the equivalence); the record exists for capture and future
// consumers. Wiring it into zrender is the weather wave's change.

/** Packed RGB565 wire value: rrrrrggg ggbbbbbb. */
struct rgb565 {
  uint16_t bits = 0;

  static constexpr rgb565 from_rgb888(uint8_t r, uint8_t g, uint8_t b) {
    return rgb565{static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))};
  }
  /** qformats 2 / stars 2 expansion law: c8 = (c5 << 3) | (c5 >> 2),
   *  (c6 << 2) | (c6 >> 4) — round-trips 0xFF and 0x00 exactly. */
  void to_rgb888(uint8_t& r, uint8_t& g, uint8_t& b) const {
    const uint8_t r5 = static_cast<uint8_t>((bits >> 11) & 0x1F);
    const uint8_t g6 = static_cast<uint8_t>((bits >> 5) & 0x3F);
    const uint8_t b5 = static_cast<uint8_t>(bits & 0x1F);
    r = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
    g = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
    b = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
  }
};

enum class FogMode : uint8_t {
  Off = 0,
  Linear = 1,  // the frozen v1 formula (qformats 8); exponential deferred
};

/** The environment record. Defaults = sky_and_beams 4a power-on law: zenith
 *  sun, sun 0xBDF7 (lanes 23,47,23 -> expands (189,190,189) ~ 0.75) and
 *  ambient 0x4208 (lanes 8,16,8 -> (66,65,66) ~ 0.25) — the stand-in's
 *  0.25 + 0.75*ndl to within one 565 quantum per channel (the lanes carry
 *  the strengths; white+white would rail every lit channel; no exact grey
 *  exists off the 0/255 endpoints because the 5- and 6-bit lanes replicate
 *  differently — the packed values are the law) — white tint at strength 0,
 *  fog off. */
struct EnvState {
  angle16 sun_yaw{0};
  angle16 sun_pitch{0x4000};  // zenith
  rgb565 sun_colour = rgb565{static_cast<uint16_t>((23u << 11) | (47u << 5) | 23u)};
  rgb565 ambient = rgb565{static_cast<uint16_t>((8u << 11) | (16u << 5) | 8u)};
  rgb565 tint = rgb565::from_rgb888(255, 255, 255);
  uint8_t tint_strength = 0;  // unit8
  FogMode fog = FogMode::Off;
  fx16 fog_near{0};
  fx16 fog_far{0};
};

inline constexpr size_t kEnvStateBytes = 20;

/** Fixed little-endian layout (capture_format.md 4.2 [v3] ENVIRONMENT_STATE):
 *  yaw 2, pitch 2, sun 2, ambient 2, tint 2, strength 1, fog 1, near 4,
 *  far 4. */
void env_state_serialize(const EnvState& st, uint8_t out[kEnvStateBytes]);
EnvState env_state_deserialize(const uint8_t in[kEnvStateBytes]);

}  // namespace sky
}  // namespace zref
