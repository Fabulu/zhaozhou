// zref_forge.hpp — the six families' topology (owner ruling 2026-08-31 §6.5).
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// FORGE.PRIM.md cited `zref::ForgePrim` as its scalar reference and no such
// symbol had ever been written — another entry in
// `reports/PHANTOM-CITATIONS-AUDIT.md`. The right answer to a phantom is to
// write the symbol, so this is it, named for what it actually owns rather than
// for the block.
//
// ---------------------------------------------------------------------------
// WHAT IS HERE AND WHAT DELIBERATELY IS NOT
// ---------------------------------------------------------------------------
// TOPOLOGY ONLY: which vertices form which triangles, in the one declared
// order. Positions are not here, because the contract's own sentence is
//
//   > A vertex stream to GEOM.SETUP, in a DECLARED DETERMINISTIC ORDER —
//   > because two orderings of the same primitive produce the same picture but
//   > different capture CRCs, and the capture is the contract.
//
// and it is the ORDER that a scalar oracle can pin. The evaluator's positions
// come from `params` and belong with SIN_Q16, not here.
//
// The four DELETED families (shard burst, chain, spline wall, low cone) have no
// encoding. `prim_family_legal` is false for everything above 5 — the way a
// deleted feature comes back is an oracle that quietly admits it.
#pragma once

#include <cstdint>

namespace zref {
namespace forge {

enum Family : int {
  kRibbon = 0,
  kFan = 1,      // radial fan / ring
  kTube = 2,
  kShell = 3,    // radial shell
  kBillboard = 4,
  kCliff = 5     // terrain cliff / skirt
};

constexpr int kMaxSegments = 64;
constexpr int kMaxSides = 8;

inline bool prim_family_legal(int family) { return family >= 0 && family <= 5; }

// Every family is a (segments x sides) grid of quads; the one-dimensional ones
// PIN AN AXIS TO 1 rather than being a separate walk. That is what makes one
// generator instead of six, and it is what pins the worst case to the single
// number the contract gives: 64 x 8 = 512 quads = 1,024 triangles.
inline int prim_eff_segments(int family, int segments) {
  if (family == kFan || family == kBillboard) return 1;
  return segments;
}

inline int prim_eff_sides(int family, int sides) {
  if (family == kRibbon || family == kCliff || family == kBillboard) return 1;
  return sides;
}

// A ring CLOSES for tube, shell and fan and is OPEN for ribbon, cliff and
// billboard. This is the whole difference between a tube and a ribbon, it is
// invisible in a triangle count, and getting it backwards welds a ribbon's two
// edges together.
inline bool prim_ring_closed(int family) {
  return family == kTube || family == kShell || family == kFan;
}

// Vertices per ring: an open ring needs one more than it has sides.
inline int prim_ring_vertices(int family, int sides) {
  const int s = prim_eff_sides(family, sides);
  return prim_ring_closed(family) ? s : s + 1;
}

inline bool prim_limits_legal(int segments, int sides) {
  return segments >= 1 && segments <= kMaxSegments && sides >= 1 &&
         sides <= kMaxSides;
}

// Two triangles per quad.
inline int prim_triangles(int family, int segments, int sides) {
  if (!prim_family_legal(family) || !prim_limits_legal(segments, sides))
    return 0;
  return 2 * prim_eff_segments(family, segments) *
         prim_eff_sides(family, sides);
}

// The index of vertex (segment s, ring position k), ring-major.
inline int prim_vertex_index(int s, int k, int ring) { return s * ring + k; }

// THE ORDER. Triangle `n` of the walk, as three vertex indices. n runs
// 0 .. prim_triangles()-1 in the emission sequence, and a generator that
// produced the same set in a different sequence would look identical on screen
// and break every capture in the repository.
struct Tri {
  int i0, i1, i2;
};

inline Tri prim_triangle(int family, int segments, int sides, int n) {
  const int nside = prim_eff_sides(family, sides);
  const int ring = prim_ring_vertices(family, sides);
  const int quad = n / 2;
  const bool second = (n & 1) != 0;
  const int seg = quad / nside;
  const int side = quad % nside;

  const int k0 = side;
  // The next vertex around the ring wraps only for a closed family.
  const int k1 = (side + 1 == nside && ring == nside) ? 0 : side + 1;

  if (!second)
    return {prim_vertex_index(seg, k0, ring), prim_vertex_index(seg, k1, ring),
            prim_vertex_index(seg + 1, k0, ring)};
  return {prim_vertex_index(seg, k1, ring),
          prim_vertex_index(seg + 1, k1, ring),
          prim_vertex_index(seg + 1, k0, ring)};
}

// The refusal taxonomy, in the order the block applies it. A job outside the
// view is SKIPPED, not refused: it is a well-formed job for another view, and
// counting it as a caller error would bury the real ones.
enum Verdict : int {
  kAccept = 0,
  kRefusedFamily = 1,
  kRefusedLimit = 2,
  kSkippedView = 3
};

inline Verdict prim_verdict(int family, int segments, int sides, int view_mask,
                            int view_sel) {
  if (!prim_family_legal(family)) return kRefusedFamily;
  if (!prim_limits_legal(segments, sides)) return kRefusedLimit;
  if ((view_mask & view_sel) == 0) return kSkippedView;
  return kAccept;
}

}  // namespace forge
}  // namespace zref
