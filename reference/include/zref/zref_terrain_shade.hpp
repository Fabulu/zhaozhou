// zref_terrain_shade.hpp — the terrain's base light.
//
// ---------------------------------------------------------------------------
// WHY THIS IS ITS OWN HEADER
// ---------------------------------------------------------------------------
// `design/contracts/TERRAIN.NORMALMAP.md` said it plainly:
//
//   `normalmap_length` / `normalmap_base` belong to TERRAIN.SHADE, not this
//   block ... They move out of this header when TERRAIN.SHADE is contracted.
//
// TERRAIN.SHADE is now contracted, so they move. The split is not tidiness: it
// is the same split as the blocks, and the blocks are split so the measured
// cost of the optional detail organ cannot be confused with the cost of the
// terrain finally being lit at all.
//
//   TERRAIN.SHADE      the ORDINARY light, needed with or without normal maps
//   TERRAIN.NORMALMAP  the high-frequency detail that rides on it, cuttable
//
// ---------------------------------------------------------------------------
// THE FINDING BEHIND IT
// ---------------------------------------------------------------------------
// Production terrain has NO lighting path. TERRAIN.NORMALS computes face
// normals and is verified at 41,731 checks; nothing consumes them,
// TERRAIN.PROJECT has no colour port, and the composed shell contains no
// terrain at all. The normals are computed and thrown away.
//
// ---------------------------------------------------------------------------
// Q FORMATS (spec/qformats.md)
// ---------------------------------------------------------------------------
//   face normal in     Q16.16, UN-normalised, as TERRAIN.NORMALS emits it
//   sun direction      s1.15 unit, FROM the surface TOWARD the light
//   base out           s1.15, saturating, SIGN PRESERVED
#pragma once

#include <cstdint>

#include "zref/zref_terrain_normals.hpp"

namespace zref {
namespace terrain {

// ---------------------------------------------------------------------------
// Rounding, owned once for both blocks.
// ---------------------------------------------------------------------------
// qformats §3 is round-half-up, one rounding per result. A shift FLOORS, so
// the two disagree on every negative value — which is half of what a detail
// normal produces and half of what a face turned from the sun produces.
inline int64_t rshift_round(int64_t v, int shift) {
  const int64_t half = int64_t(1) << (shift - 1);
  return (v + half) >> shift;
}

// |n| of a Q16.16 face normal, in the same Q16.16 scale.
//
// THE ACCUMULATION IS UNSIGNED 64 ON PURPOSE. A component at the fx16 rail
// squares to 2^62 and three of those reach 1.38e19, against a signed-64
// maximum of 9.22e18. TERRAIN.NORMALS' contract says the rails are reachable
// inside the domain, so this is not a theoretical input. Signed accumulation
// was undefined behaviour in C++ and would be a silent wrap in RTL — giving a
// SMALL length and therefore a huge, wrong shade.
inline int64_t shade_length(const FaceNormal& n) {
  const uint64_t x = static_cast<uint64_t>(static_cast<int64_t>(n.x) * n.x);
  const uint64_t y = static_cast<uint64_t>(static_cast<int64_t>(n.y) * n.y);
  const uint64_t z = static_cast<uint64_t>(static_cast<int64_t>(n.z) * n.z);
  const uint64_t sq = x + y + z;
  if (sq == 0) return 0;
  // Restoring square root, the shape the RTL walks.
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

// dot(n, L) / |n|, in s1.15. ONCE PER TRIANGLE — that is the whole economy.
//
// The sign is KEPT, not clamped. A face turned away from the sun gives a
// negative base, and TERRAIN.NORMALMAP's per-fragment term is ADDED to it;
// clamping here would discard exactly what the addition needs. The clamp
// belongs where the shade is packed to unit8.
inline int shade_base(const FaceNormal& n, int lx, int ly, int lz) {
  const int64_t len = shade_length(n);
  if (len == 0) return 0;  // a degenerate triangle has no direction to light
  const int64_t d = static_cast<int64_t>(n.x) * lx +
                    static_cast<int64_t>(n.y) * ly +
                    static_cast<int64_t>(n.z) * lz;
  int64_t q = d / len;  // Q16.16 * s1.15 / Q16.16 -> s1.15
  if (q > 32767) q = 32767;
  if (q < -32768) q = -32768;
  return static_cast<int>(q);
}

// The shipped shade, in unit8.
//
// AMBIENT IS ADDED, per `SetEnvironment 0x0311` (sky_and_beams §4a), which
// carries it as a colour beside the sun. An earlier draft made it a FLOOR and
// argued the case in a comment; that was a terrain header re-legislating
// ratified law.
//
// The lit term is clamped at zero BEFORE ambient is added: a surface facing
// away receives no sun, not negative sun that eats the ambient.
inline uint8_t shade_pack(int base, int detail, uint8_t ambient) {
  int64_t s = static_cast<int64_t>(base) + detail;
  if (s < 0) s = 0;
  int64_t u = rshift_round(s * 256, 15);  // s1.15 -> unit8 (value = raw/256)
  u += ambient;
  if (u > 255) u = 255;
  return static_cast<uint8_t>(u);
}

// Several suns accumulate before ambient, saturating rather than wrapping —
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
