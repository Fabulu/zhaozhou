// zref_terrain_normalmap.hpp — the detail that rides on the terrain's light.
//
// ---------------------------------------------------------------------------
// THIS HEADER IS THE CUTTABLE HALF
// ---------------------------------------------------------------------------
// The base light moved to `zref_terrain_shade.hpp` when TERRAIN.SHADE was
// contracted, which is what `design/contracts/TERRAIN.NORMALMAP.md` said would
// happen. The split matches the blocks, and the blocks are split so the
// measured delta of the optional detail cannot be confused with the cost of
// the terrain finally being lit at all:
//
//   TERRAIN.SHADE      ~730 ALM, 10 DSP   needed with or without normal maps
//   TERRAIN.NORMALMAP  ~380 ALM,  2 DSP   this file. Cuttable.
//
// ---------------------------------------------------------------------------
// WHY THERE IS NO TANGENT FRAME
// ---------------------------------------------------------------------------
// A heightfield's tangent frame is axis-aligned in world space, so a detail
// normal perturbs the surface normal in world XZ DIRECTLY. Nothing extra is
// interpolated and no frame is built. That, plus the per-triangle/per-fragment
// split below, is what makes a real per-pixel normal map affordable here.
//
//     dot(n/|n| + s*d, L)  =  dot(n, L)/|n|  +  s*dot(d, L)
//                             \___________/     \__________/
//                              TERRAIN.SHADE     THIS FILE
//
// ---------------------------------------------------------------------------
// Q FORMATS (spec/qformats.md)
// ---------------------------------------------------------------------------
//   detail texel   two s8 — value = raw / 128, packed {dz, dx}
//   sun direction  **Q16.16 unit**, the same light the ratified flat-shade law
//                  uses (kShadeLightX/Y/Z). The first version of this file said
//                  s1.15, which was an assumption rather than a reading — the
//                  renderer's light is Q16.16 and getting that wrong is a
//                  factor of two in the relief.
//   strength       u8     — value = raw / 256
//   detail out     Q16.16, added to the base BEFORE the ratified clamp
#pragma once

#include <cstdint>

#include "zref/zref_terrain_shade.hpp"

namespace zref {
namespace terrain {

// Two signed 8-bit perturbations, in world X and Z.
//
// THE Y COMPONENT IS ABSENT BY CONSTRUCTION. A detail normal that could tip
// the surface downward is a dent in the geometry, not a texture.
//
// One consequence, DECLARED rather than discovered: under a sun at the zenith
// L is almost pure +Y, so dot(d, L) goes to zero and the relief fades out.
// That reads like real flat-light photography — and it is exactly why the
// look-gate is a MOVING sun and not a still frame.
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

// strength * dot(d, L), in s1.15. Two multiplies and an add, per fragment.
//
// The detail is `strength * dot(d, L)` in the SAME Q16.16 scale the base uses,
// so the two can be added before the clamp.
//
// Scaling, stated rather than assumed: `d` is s8 with value raw/128 and
// `strength` is u8 with value raw/256, so the product carries 15 fraction bits
// beyond the light's own 16. `rshift_round(., 15)` removes exactly those and
// leaves Q16.16.
//
// Rounding is round-half-up, NOT a shift. A shift floors, and floors disagree
// on every negative product — which is half of what a detail normal produces.
inline int32_t normalmap_detail(const DetailNormal& d, int32_t lx, int32_t lz, int strength) {
  const int64_t dot = static_cast<int64_t>(d.nx) * lx + static_cast<int64_t>(d.nz) * lz;
  return static_cast<int32_t>(rshift_round(dot * strength, 15));
}

// THE CUT SEAM, as a property a test can hold: strength 0 is a bit-exact
// no-op. If the resource number comes back bad, removing the detail changes
// nothing else — which is what makes this organ cleanly cuttable rather than
// entangled.
inline bool normalmap_is_noop(int strength) { return strength == 0; }

// ---------------------------------------------------------------------------
// THE HARDWARE BLOCK'S LAW (amendment 2026-09-09, with the RTL rebuild)
// ---------------------------------------------------------------------------
// TERRAIN.NORMALMAP's config carries the sun XZ as s1.15 (an s16 register
// cannot hold Q16.16's 0x10000), derived per frame from the ratified Q16.16
// light by one halving: sun15 = clamp(rshift_round(L16, 1), -32768, 32767).
// The delta it emits is s9 with value raw/256 — colour-lane LSBs under a
// white sun, the contract's declared monochrome approximation.
//
// The scale algebra, stated so the factor of two stays dead: strength has 8
// fraction bits, d has 7, an s1.15 sun has 15 — 30 in, 8 out, so the ONE
// rounding is a rescale by 30 - 8 = 22. The contract's original text said 23,
// which is the same s1.15-vs-Q16.16 slip this header's format table already
// documents ("a factor of two in the relief"); 22 is what agrees with
// `normalmap_detail` above (with lx = 2*sun15, the two functions differ only
// by their output grain).
//
// Multiple suns fold BEFORE the rounding by distributivity —
// strength*(d.x*(sx0+sx1) + d.z*(sz0+sz1)) — every product exact, one
// rounding, bit-identical to per-sun accumulation. That identity is why the
// hardware can hold sun-sum coefficient tables instead of multipliers.
constexpr int kNormalmapDeltaShift = 22;

inline int32_t normalmap_delta_s9(const DetailNormal& d, int sun_x15, int sun_z15,
                                  int sun_x15_b, int sun_z15_b, int strength) {
  const int64_t dot = static_cast<int64_t>(d.nx) * (sun_x15 + sun_x15_b) +
                      static_cast<int64_t>(d.nz) * (sun_z15 + sun_z15_b);
  int64_t delta = rshift_round(dot * strength, kNormalmapDeltaShift);
  if (delta > 255) delta = 255;
  if (delta < -256) delta = -256;
  return static_cast<int32_t>(delta);
}

// The application seam's arithmetic: the delta lands on the flat lit colour
// lanes, saturating unsigned 8-bit. (The seam itself may clamp to the lit
// range [ambient_c, ambient_c + sun_c] instead of [0, 255] — that refinement
// is the fold-image of the ruled per-light clamp and lives in the seam's own
// contract; THIS function is the plain contract form.)
inline uint8_t normalmap_apply(uint8_t v, int32_t delta) {
  const int32_t sum = static_cast<int32_t>(v) + delta;
  if (sum < 0) return 0;
  if (sum > 255) return 255;
  return static_cast<uint8_t>(sum);
}

// ---------------------------------------------------------------------------
// THE PYRAMID ADDRESSING LAW (mip tail, addendum of 2026-09-05)
// ---------------------------------------------------------------------------
// Seven square levels, 64 down to 1, packed flat: 5,461 words. The pyramid is
// built OFFLINE by averaging SIGNED dx/dz per 2x2 (never re-normalising);
// the hardware only addresses it. The asset packer and the RTL's directed
// test both take this function as the layout's single definition.
constexpr int kNormalmapLevels = 7;
constexpr int kNormalmapPyramidWords = 5461;
constexpr int kNormalmapLevelBase[kNormalmapLevels] = {0, 4096, 5120, 5376, 5440, 5456, 5460};

inline int normalmap_pyramid_addr(int level, int u6, int v6) {
  return kNormalmapLevelBase[level] + ((v6 >> level) << (6 - level)) + (u6 >> level);
}

// The level actually sampled: fragment integer LOD, plus an authored signed
// bias, clamped to an authored ceiling. max_level 0 (the reset state) is the
// un-mipped contract behaviour, bit-exactly.
inline int normalmap_level_select(int frag_lod, int lod_bias, int max_level) {
  int lvl = frag_lod + lod_bias;
  if (max_level > kNormalmapLevels - 1) max_level = kNormalmapLevels - 1;
  if (lvl < 0) lvl = 0;
  if (lvl > max_level) lvl = max_level;
  return lvl;
}

}  // namespace terrain
}  // namespace zref
