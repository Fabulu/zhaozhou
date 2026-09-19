// zref_light_env.hpp -- the BRIDGE from SetEnvironment 0x0311 to GEOM.LIGHT's
// descriptor bank (`zhao_light_stream`), owner ruling R25 (2026-09-19).
//
// R25: "Promote SetEnvironment 0x0311 from reserved to IMPLEMENTED. The bank's
// Q16.16 / u20-gain values are THE LAW; section 4a's u8 formula becomes a derived
// view with a zref bridge. CMD.EXEC lowers SetEnvironment into the bank, and its
// sun direction also feeds R21's terrain shading."
//
// This header IS that bridge: the one place an EnvState becomes bank words.
// `zhao_light_env` (fpga/rtl/geometry/zhao_light_env.sv) writes exactly these
// words, and tests/geometry/light_env_directed.cpp differences every one.
//
// WHAT IS DERIVED, AND FROM WHICH SENTENCE (nothing here is chosen):
//   * light 0's DIRECTION, half A words 0..2 -- spec/sky_and_beams.md 4a, kept
//     unchanged by the R2 amendment:
//         L = ( fx_mul(cos p, sin y),  sin p,  fx_mul(cos p, cos y) )
//     with fx_sin/fx_cos (qformats 7.1) and single-rounded products (qformats 3).
//   * the 8-BIT CHANNEL of an rgb565 value -- 4a's frozen bit-replication law,
//     `zref::sky::rgb565::to_rgb888`.
//   * the LANE of an 8-bit channel, `c8 << 8` -- 4a's own equivalence sentence:
//     the stand-in "is this law at `ambient = 0x4000`, `sun = 0xC000` in the
//     Q16.16 lanes", i.e. 0x40 -> 0x4000 and 0xC0 -> 0xC000. The bank's gains
//     and environment words are those Q16.16 lanes (1.0 = 0x1_0000,
//     zhao_light_stream's NDL_ONE), held in u20.
//   * sun colour -> light 0's GAIN, ambient -> the environment's AMBIENT: 4a's
//     u8 law is `sat_u8(ambient_c + rescale_u(sun_c * ndl, 8))`, and the bank
//     law is `sat01(rhu16(gain_c * ndl) + ambient_c)`; the terms correspond one
//     for one. They ROUND differently (R25: "a derived view"), which is why the
//     bank is THE LAW and the u8 formula is not re-derived here.
//
// WHAT IS ZERO, AND WHY IT IS NOT A GUESS:
//   * light 0's EMISSION -- SetEnvironment carries none.
//   * light 0's DETAIL (half A word 3) -- the creature profile never admits a
//     detail term, and SetEnvironment carries none for the render profile.
//   * the SPILL words -- SetEnvironment carries no spill.
//   * lights 1..7 -- "no dynamic point lights in the format" (4a); the one sun is
//     light 0 and the bank's light COUNT is 1.
//
// WHAT THE BRIDGE DOES NOT CARRY, recorded rather than dropped silently:
//   * TINT (colour + strength) -- 4a applies it to the LIT colour after
//     lighting (`lit' = sat_u8(lit + rescale_s((tint_c - lit) * s, 8))`), a
//     per-vertex stage the bank has no place for; its Q16 form is R2's open
//     item ("where tint applies") and is an OWNER decision, not a bank word.
//   * FOG -- qformats 8's per-vertex fog factor; its consumer is GEOM.FOGFACTOR,
//     not the light bank.
#pragma once

#include <cstdint>

#include "zref/zref_fixp.hpp"
#include "zref/zref_sky.hpp"
#include "zref/zref_trig.hpp"

namespace zref {
namespace light_env {

/** The bank words one SetEnvironment produces, in zhao_light_stream's layout. */
struct Bank {
  int32_t  light0_a[4];   // cfg {light 0, half A, word 0..3}: Lx, Ly, Lz, detail
  uint32_t light0_b[4];   // cfg {light 0, half B, word 0..3}: the 128-bit record
  uint32_t env[6];        // cfg {light 0xF, half A, word 0..5}: ambient rgb, spill rgb
  uint8_t  nlights;       // lights the bank holds after the load
};

/** 4a's sun direction, Q16.16 lanes. */
inline void sun_direction(angle16 yaw, angle16 pitch, int32_t L[3]) {
  const fx16 sp = fx_sin(pitch);
  const fx16 cp = fx_cos(pitch);
  const fx16 sy = fx_sin(yaw);
  const fx16 cy = fx_cos(yaw);
  L[0] = fx_mul(cp, sy, nullptr).raw;
  L[1] = sp.raw;
  L[2] = fx_mul(cp, cy, nullptr).raw;
}

/** An 8-bit channel as a Q16.16 lane (4a: 0xC0 -> 0xC000). */
constexpr uint32_t lane_of(uint8_t c8) { return static_cast<uint32_t>(c8) << 8; }

/** zhao_light_stream's half-B record: {flags8, eb, eg, er, cb, cg, cr} at 20
 *  bits a field, little-endian, split into four 32-bit cfg words. */
inline void pack_coeffs(uint32_t cr, uint32_t cg, uint32_t cb, uint32_t out[4]) {
  unsigned __int128 rec = 0;
  const uint32_t f[6] = {cr, cg, cb, 0u, 0u, 0u};  // gains, then emissions (none)
  for (int i = 0; i < 6; ++i)
    rec |= static_cast<unsigned __int128>(f[i] & 0xFFFFFu) << (20 * i);
  // flags byte [127:120] = 0
  for (int w = 0; w < 4; ++w) out[w] = static_cast<uint32_t>(rec >> (32 * w));
}

/** THE BRIDGE. */
inline Bank bank_of(const sky::EnvState& e) {
  Bank b{};
  sun_direction(e.sun_yaw, e.sun_pitch, b.light0_a);
  b.light0_a[3] = 0;
  uint8_t sr, sg, sb, ar, ag, ab;
  e.sun_colour.to_rgb888(sr, sg, sb);
  e.ambient.to_rgb888(ar, ag, ab);
  pack_coeffs(lane_of(sr), lane_of(sg), lane_of(sb), b.light0_b);
  b.env[0] = lane_of(ar);
  b.env[1] = lane_of(ag);
  b.env[2] = lane_of(ab);
  b.env[3] = b.env[4] = b.env[5] = 0u;
  b.nlights = 1;
  return b;
}

}  // namespace light_env
}  // namespace zref
