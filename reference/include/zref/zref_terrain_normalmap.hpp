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
//   sun direction  s1.15 unit
//   strength       u8     — value = raw / 256
//   detail out     s1.15, to be added to TERRAIN.SHADE's base
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
// The format scaling is exact and free: strength/256 * d/128 expressed in
// s1.15 is strength*d*32768/(256*128), and 32768/(256*128) is 1. That the
// three Q formats cancel to a bare product is why they were chosen.
//
// Rounding is round-half-up via `rshift_round`, NOT a shift. A shift floors,
// and floors disagree on every negative product — which is half of what a
// detail normal produces.
inline int normalmap_detail(const DetailNormal& d, int lx, int lz,
                            int strength) {
  const int64_t dot = static_cast<int64_t>(d.nx) * lx +
                      static_cast<int64_t>(d.nz) * lz;
  return static_cast<int>(rshift_round(dot * strength, 15));
}

// THE CUT SEAM, as a property a test can hold: strength 0 is a bit-exact
// no-op. If the resource number comes back bad, removing the detail changes
// nothing else — which is what makes this organ cleanly cuttable rather than
// entangled.
inline bool normalmap_is_noop(int strength) { return strength == 0; }

}  // namespace terrain
}  // namespace zref
