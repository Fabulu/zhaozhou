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

}  // namespace terrain
}  // namespace zref
