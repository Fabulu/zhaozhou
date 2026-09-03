// zref_terrain_normalmap.hpp — detail normals on the heightfield.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS AND WHY IT IS TERRAIN-SHAPED
// ---------------------------------------------------------------------------
// Owner, 2026-09-03, asked whether the architecture provisions normal maps and
// said to build the real per-pixel path: "we make it and see how bad it is ...
// Normal maps would be a huge gain though."
//
// The textbook path is a tangent frame per vertex, three interpolated normal
// attributes, and a per-fragment normalise. On this machine that is expensive
// in the one resource the production count says is tight -- and most of it is
// unnecessary for a HEIGHTFIELD, because a heightfield's tangent frame is
// axis-aligned in world space. A detail normal can perturb the surface normal
// in world XZ directly, with no frame to build and nothing extra to
// interpolate.
//
// ---------------------------------------------------------------------------
// THE ALGEBRA THAT MAKES IT CHEAP -- normalise the RESULT, not the vector
// ---------------------------------------------------------------------------
// Lighting wants dot(normalise(n + s*d), L). Written naively that needs the
// unit face normal per fragment, so it needs a divide per fragment.
//
// Drop the re-normalisation of the perturbed vector (see APPROXIMATION below)
// and distribute:
//
//     dot(n/|n| + s*d, L)  =  dot(n, L)/|n|  +  s*dot(d, L)
//                             \___________/     \__________/
//                              PER TRIANGLE      per fragment
//
// The face normal is CONSTANT over a triangle -- TERRAIN.NORMALS emits one per
// triangle, not one per vertex -- so the whole first term, isqrt and divide
// included, is computed once per triangle and reused by every fragment of it.
// What is left per fragment is `s*dot(d, L)`: with d = (dx, 0, dz) that is two
// multiplies and an add.
//
// ---------------------------------------------------------------------------
// THE APPROXIMATION, DECLARED, WITH ITS KNOB
// ---------------------------------------------------------------------------
// `n/|n| + s*d` is not a unit vector, so the shade is not exactly the cosine.
// The error grows with `strength`; at strength 0 it is zero. This is a
// LOOK-TUNED value and it stays a named knob, because "this is derived from
// the maths, so it is not a knob" is how a wrong number becomes an
// unadjustable wrong number. Author by eye, render, look, adjust.
//
// ---------------------------------------------------------------------------
// Q FORMATS (spec/qformats.md)
// ---------------------------------------------------------------------------
//   face normal in     Q16.16, UN-normalised, as TERRAIN.NORMALS emits it
//   unit vectors       s1.15  -- value = raw / 32768
//   detail texel       two s8 -- value = raw / 128, packed {dz, dx} in 16 bits
//   strength           u8     -- value = raw / 256
//   ambient, shade     unit8  -- value = raw / 256 (qformats 2), so 255 is the
//                                largest representable and NOT 1.0
#pragma once

#include <cstdint>

#include "zref/zref_terrain_normals.hpp"

namespace zref {
namespace terrain {

// A detail texel: two signed 8-bit perturbations in world X and Z. The Y
// (vertical) component is deliberately absent -- a detail normal that could
// point the surface downward is a dent in the geometry, not a texture.
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

// |n| of a Q16.16 face normal, as an integer in the same Q16.16 scale.
// The squares are taken in 64 bits because a Q16.16 component near one world
// unit squares to 2^32 and three of them overflow 32 bits by construction.
inline int64_t normalmap_length(const FaceNormal& n) {
  const int64_t x = n.x, y = n.y, z = n.z;
  const uint64_t sq = static_cast<uint64_t>(x * x + y * y + z * z);
  if (sq == 0) return 0;
  // Integer sqrt, the same restoring shape the RTL walks.
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

// THE PER-TRIANGLE TERM: dot(n, L) / |n|, in s1.15.
//
// L is a unit direction in s1.15 pointing FROM the surface TOWARD the light,
// so a face aimed straight at the light gives +32768 before saturation and one
// aimed away gives negative. Negative is kept, not clamped: the clamp belongs
// with the ambient floor at the end, and clamping here would lose the sign that
// the per-fragment detail term is added to.
inline int normalmap_base(const FaceNormal& n, int lx, int ly, int lz) {
  const int64_t len = normalmap_length(n);
  if (len == 0) return 0;  // a degenerate triangle has no direction to light
  const int64_t d = static_cast<int64_t>(n.x) * lx +
                    static_cast<int64_t>(n.y) * ly +
                    static_cast<int64_t>(n.z) * lz;
  // d is Q16.16 * s1.15; dividing by the Q16.16 length leaves s1.15.
  int64_t q = d / len;
  if (q > 32767) q = 32767;
  if (q < -32768) q = -32768;
  return static_cast<int>(q);
}

// THE PER-FRAGMENT TERM: strength * dot(d, L), in s1.15.
//
// The scaling is exact and free: strength/256 * dx/128 in s1.15 units is
// strength*dx*32768/(256*128) = strength*dx. That the three formats cancel to
// a bare product is why they were chosen.
inline int normalmap_detail(const DetailNormal& d, int lx, int lz,
                            int strength) {
  return strength * (d.nx * lx + d.nz * lz) / 32768;
}

// The shipped shade: base + detail, floored by ambient, as unit8.
//
// Ambient is a FLOOR rather than a sum. An added ambient term lifts a lit face
// past white and flattens exactly the shapes the detail normals were added to
// show; a floor leaves the lit range alone and only stops the unlit side going
// black.
inline uint8_t normalmap_shade(int base, int detail, uint8_t ambient) {
  int s = base + detail;
  if (s < 0) s = 0;
  // s1.15 -> unit8. 255 is the largest unit8, so a fully lit face saturates
  // there rather than wrapping to zero.
  int u = (s * 256) / 32768;
  if (u > 255) u = 255;
  return static_cast<uint8_t>(u < ambient ? ambient : u);
}

// Several suns accumulate before the ambient floor, saturating rather than
// wrapping -- two suns on one face is brighter, never darker.
inline uint8_t normalmap_shade_multi(const int* bases, const int* details,
                                     int suns, uint8_t ambient) {
  int total = 0;
  for (int i = 0; i < suns; ++i) {
    int s = bases[i] + details[i];
    if (s < 0) s = 0;
    total += s;
    if (total > 32768) total = 32768;
  }
  int u = (total * 256) / 32768;
  if (u > 255) u = 255;
  return static_cast<uint8_t>(u < ambient ? ambient : u);
}

}  // namespace terrain
}  // namespace zref
