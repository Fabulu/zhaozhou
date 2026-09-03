// zref_terrain_normalmap.hpp — terrain base light, and the detail that rides it.
//
// ---------------------------------------------------------------------------
// TWO LAWS, DELIBERATELY SEPARATE
// ---------------------------------------------------------------------------
// Owner brief, 2026-09-03 (reports/BRO-20260903-NORMALMAP-AND-ANIMATION-PATH.md):
//
//   TERRAIN.SHADE     the terrain's missing ORDINARY light   ~730 ALM, 10 DSP
//   TERRAIN.NORMALMAP the high-frequency moving-light detail ~380 ALM,  2 DSP
//
//   "The crucial discovery is that the 10-DSP piece is not really a normal-map
//    expense. Production terrain currently has no proper lighting path at all
//    ... TERRAIN.SHADE is necessary whether you use normal maps or not."
//
// They are separate functions here for the same reason they are separate
// blocks: so the measured cost of the detail cannot be confused with the cost
// of the terrain finally being lit at all. Add NORMALMAP second, and its delta
// is its own.
//
// ---------------------------------------------------------------------------
// THE ALGEBRA THAT MAKES THE DETAIL CHEAP
// ---------------------------------------------------------------------------
// A heightfield's tangent frame is axis-aligned in world space, so a detail
// normal perturbs the surface normal in world XZ directly: no frame to build,
// nothing extra to interpolate.
//
//     dot(n/|n| + s*d, L)  =  dot(n, L)/|n|  +  s*dot(d, L)
//                             \___________/     \__________/
//                              PER TRIANGLE      per fragment
//
// TERRAIN.NORMALS emits ONE face normal per triangle, so the square root and
// the divide happen once per triangle and every fragment of it reuses the
// answer. What a fragment pays is two multiplies and an add.
//
// ---------------------------------------------------------------------------
// FOUR CORRECTIONS TO THE FIRST DRAFT (2026-09-03)
// ---------------------------------------------------------------------------
// 1. THE SUM OF SQUARES OVERFLOWED int64 ON LEGAL INPUT. A Q16.16 component at
//    the fx16 rail squares to 2^62, and three of those is 1.38e19 against an
//    int64 maximum of 9.22e18 — undefined behaviour in C++ and a wrap in RTL.
//    TERRAIN.NORMALS' contract says the rails are reachable inside the domain,
//    so this was not a theoretical case. Each product is now widened to
//    uint64 BEFORE the addition, where 1.38e19 fits.
// 2. THE ROUNDING DISAGREED THREE WAYS: this file truncated, the RTL floored
//    with `>>>`, and spec/qformats.md says round-half-up. They differ on every
//    negative detail term. One helper now owns it.
// 3. AMBIENT IS AN ADDEND, NOT A FLOOR. The draft made it a floor and argued
//    the case; that re-legislated ratified law from inside a terrain header.
//    `SetEnvironment 0x0311` (sky_and_beams §4a) carries ambient as a COLOUR
//    alongside the sun, which is an added term. Corrected, and the argument
//    for a floor is recorded in the brief rather than enacted here.
// 4. The draft RTL's divider computed the wrong 32 bits and returned zero for
//    every realistic triangle. That is an RTL fault, not an oracle fault, and
//    `zhao_terrain_normalmap.sv` is quarantined out of the production manifest
//    until the replacement is built.
//
// ---------------------------------------------------------------------------
// Q FORMATS (spec/qformats.md)
// ---------------------------------------------------------------------------
//   face normal in     Q16.16, UN-normalised, as TERRAIN.NORMALS emits it
//   unit vectors       s1.15  -- value = raw / 32768
//   detail texel       two s8 -- value = raw / 128, packed {dz, dx}
//   strength           u8     -- value = raw / 256
//   shade out          unit8  -- value = raw / 256 (qformats §2), so 255 is the
//                               largest representable and NOT 1.0
#pragma once

#include <cstdint>

#include "zref/zref_terrain_normals.hpp"

namespace zref {
namespace terrain {

// ---------------------------------------------------------------------------
// Rounding, owned once.
// ---------------------------------------------------------------------------
// qformats §3: one rounding per result, round-half-up. A shift is NOT this:
// `>>> 15` floors, so it disagrees with the oracle on every negative value,
// which is exactly where a detail normal spends half its time.
inline int64_t rshift_round(int64_t v, int shift) {
  const int64_t half = int64_t(1) << (shift - 1);
  return (v + half) >> shift;
}

struct DetailNormal {
  int nx;  // s8, world +X
  int nz;  // s8, world +Z
};

inline DetailNormal normalmap_decode(uint16_t texel) {
  DetailNormal d;
  d.nx = static_cast<int8_t>(texel & 0xFF);
  d.nz = static_cast<int8_t>((texel >> 8) & 0xFF);
  return d;
}

// |n| of a Q16.16 face normal, in the same Q16.16 scale.
//
// The squares are accumulated in UNSIGNED 64 because each is at most 2^62 and
// three of them reach 1.38e19 -- past int64's 9.22e18 and inside uint64's
// 1.84e19. Squares are non-negative, so unsigned costs nothing and is the
// natural type.
inline int64_t normalmap_length(const FaceNormal& n) {
  const uint64_t x = static_cast<uint64_t>(static_cast<int64_t>(n.x) * n.x);
  const uint64_t y = static_cast<uint64_t>(static_cast<int64_t>(n.y) * n.y);
  const uint64_t z = static_cast<uint64_t>(static_cast<int64_t>(n.z) * n.z);
  const uint64_t sq = x + y + z;
  if (sq == 0) return 0;
  uint64_t rem = 0, root = 0;
  for (int i = 31; i >= 0; --i) {
    rem = (rem << 2) | ((sq >> (i * 2)) & 3u);
    root <<= 1;
    const uint64_t trial = (root << 1) | 1u;
    if (rem >= trial) {
      rem -= trial;
      root |= 1u;
    }
  }
  return static_cast<int64_t>(root);
}

// ===========================================================================
// TERRAIN.SHADE -- the base light. Needed with or without normal maps.
// ===========================================================================
// dot(n, L) / |n|, in s1.15.
//
// L is a unit direction in s1.15 pointing FROM the surface TOWARD the light,
// so a face aimed at the light gives +32767 and one aimed away gives negative.
// The sign is KEPT, not clamped: the per-fragment detail term is added to this,
// and clamping here would discard the information that addition needs.
inline int shade_base(const FaceNormal& n, int lx, int ly, int lz) {
  const int64_t len = normalmap_length(n);
  if (len == 0) return 0;  // a degenerate triangle has no direction to light
  const int64_t d = static_cast<int64_t>(n.x) * lx +
                    static_cast<int64_t>(n.y) * ly +
                    static_cast<int64_t>(n.z) * lz;
  int64_t q = d / len;  // Q16.16 * s1.15 / Q16.16 -> s1.15
  if (q > 32767) q = 32767;
  if (q < -32768) q = -32768;
  return static_cast<int>(q);
}

// ===========================================================================
// TERRAIN.NORMALMAP -- the detail delta. The cuttable organ.
// ===========================================================================
// strength * dot(d, L), in s1.15.
//
// The format scaling is exact and free: strength/256 * d/128 expressed in s1.15
// is strength*d*32768/(256*128), and 32768/(256*128) is 1. That the three Q
// formats cancel to a bare product is why they were chosen.
//
// Y is absent by construction -- a detail normal that could tip the surface
// downward is a dent in the geometry, not a texture. One consequence, declared
// rather than discovered: under a sun at the zenith, L is almost pure +Y, so
// dot(d, L) goes to zero and the detail fades out. That reads like real
// flat-light photography and it is why the look-gate is a MOVING sun.
inline int normalmap_detail(const DetailNormal& d, int lx, int lz,
                            int strength) {
  const int64_t dot = static_cast<int64_t>(d.nx) * lx +
                      static_cast<int64_t>(d.nz) * lz;
  return static_cast<int>(rshift_round(dot * strength, 15));
}

// ===========================================================================
// The shipped shade
// ===========================================================================
// AMBIENT IS ADDED, per `SetEnvironment 0x0311` (sky_and_beams §4a), which
// carries it as a colour alongside the sun. The draft made it a floor; that
// was this file legislating, and it is corrected.
//
// The lit term is clamped at zero BEFORE ambient is added, because a surface
// facing away from the sun receives no sun -- it does not receive negative
// sun that eats the ambient.
inline uint8_t shade_pack(int base, int detail, uint8_t ambient) {
  int64_t s = static_cast<int64_t>(base) + detail;
  if (s < 0) s = 0;
  int64_t u = rshift_round(s * 256, 15);  // s1.15 -> unit8
  u += ambient;
  if (u > 255) u = 255;
  return static_cast<uint8_t>(u);
}

// Several suns accumulate before ambient, saturating rather than wrapping --
// two suns on one face is brighter, never darker.
inline uint8_t shade_pack_multi(const int* bases, const int* details, int suns,
                                uint8_t ambient) {
  int64_t total = 0;
  for (int i = 0; i < suns; ++i) {
    int64_t s = static_cast<int64_t>(bases[i]) + details[i];
    if (s < 0) s = 0;
    total += s;
    if (total > 32767) total = 32767;
  }
  int64_t u = rshift_round(total * 256, 15);
  u += ambient;
  if (u > 255) u = 255;
  return static_cast<uint8_t>(u);
}

}  // namespace terrain
}  // namespace zref
