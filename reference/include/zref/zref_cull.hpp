// zref_cull.hpp — GEOM.MESHFETCH's conservative per-camera frustum rejection
// of an instance bounding sphere (phase 8, ZH-037).
//
// THE LAW, as the owner ruled it on 2026-08-22 (docs/OWNER_DOCKET.md, "RULED
// 2026-08-22 — 'visibility sectors' is deleted"):
//
//   · conservative per-camera frustum rejection of an instance bound, BEFORE
//     vertex decode;
//   · the bound is a SPHERE, `bound_centre` + `bound_radius`;
//   · reject only when the sphere is outside EVERY active camera;
//   · carry a two-bit per-camera visibility result downstream.
//
// ---------------------------------------------------------------------------
// WHAT IS BORROWED AND WHAT IS NEW — this header is BOTH kinds at once
// ---------------------------------------------------------------------------
// reports/PHANTOM_REFERENCES.md draws the line between a reference that already
// exists under another name (kind 1) and one that has to be written and thereby
// BECOMES the law (kind 2). This file straddles it, and the two halves are kept
// visibly apart because they carry different weight:
//
//   KIND 1 — THE CAMERA. The frustum is not a new camera model. It is the
//   SHIPPED view-projection `mat4fx`, read a different way. Every condition
//   below comes out of `zref::render::project_vertex`
//   (reference/src/zrender/rast.cpp:43), which is the law GEOM.PROJECT is
//   already verified against, and nothing here may disagree with it. Three
//   facts of that convention are load-bearing and each is easy to get wrong
//   from habit:
//
//     · `mat4fx` is ROW-MAJOR and `mat4_vec4` computes M·v with v a column, so
//       clip.x/y/w are dot products with matrix ROWS. Plane extraction is
//       therefore a combination of ROWS, not of columns.
//     · +Y NDC maps to the +Y canvas ROW, i.e. DOWNWARD (video_rules §2,
//       top-left origin). There is no Y flip anywhere in this machine. A cull
//       written from the usual Y-up habit swaps top and bottom — and because
//       both are symmetric about zero it would still look plausible on centred
//       content and fail only near the top and bottom edges.
//     · There is NO near/far z clip. `project_vertex`'s only depth condition is
//       `clip.w > 0`. The frustum therefore has FIVE planes, not six, and
//       adding a sixth would reject geometry the renderer would have drawn.
//       `row2` is unused here, which is that same fact stated in arithmetic.
//
//   KIND 2 — THE SPHERE TEST. Nothing in this repository tested a sphere
//   against a plane before this file: `grep -ri frustum reference/` returns
//   nothing. So `sphere_outside_plane` and the ceiling bound below are not a
//   transcription of anything — they ARE the law, and the RTL is measured
//   against them. That is the weaker position of the two, and it is why the
//   conservatism argument in §"THE ROUNDING GOES UP" is written as a proof
//   rather than as a sampled result.
//
// ---------------------------------------------------------------------------
// THE FIVE PLANES
// ---------------------------------------------------------------------------
// With clip.x = row0·v, clip.y = row1·v, clip.w = row3·v, the volume
// `project_vertex` actually draws is w > 0, -w <= x <= w, -w <= y <= w.
// Rearranged, each condition is a half-space `p·v >= 0`:
//
//     left   = row3 + row0
//     right  = row3 - row0
//     bottom = row3 + row1
//     top    = row3 - row1
//     near   = row3            (this is w > 0, and it is the ONLY depth test)
//
// A plane component is a SUM OF TWO fx16 words, so it does not fit in an
// int32: the range is [-2^32, 2^32-2]. It is carried as int64 here and as a
// signed 33-bit word in the RTL. Storing it back into an fx16 would silently
// saturate on any matrix with a large row, which is exactly the case a cull
// must not get wrong.
//
// ---------------------------------------------------------------------------
// THE ROUNDING GOES UP, AND THAT IS THE OPPOSITE OF THE HABIT
// ---------------------------------------------------------------------------
// A sphere (c, r) lies wholly outside plane p iff
//
//     a*cx + b*cy + c*cz + d  <  -r * |(a,b,c)|
//
// and |(a,b,c)| is irrational in general, so it must be bounded. The ratified
// primitive is `zref::isqrt_u64` (qformats §7.2), an EXACT FLOOR square root —
// and floor is the wrong direction here.
//
// Using floor(|n|) makes the right-hand side LESS NEGATIVE, which makes the
// rejection easier to satisfy, which rejects spheres that are actually visible.
// A too-tight cull does not cost performance, it DELETES GEOMETRY, and it does
// so only near the screen edges where it reads as objects popping out of
// existence. A too-loose cull costs a little wasted decode work and nothing
// else. So the bound is a CEILING:
//
//     len_hi = ceil(sqrt(a^2+b^2+c^2))  >=  |(a,b,c)|
//     outside  iff  dot < -r * len_hi
//
// and since len_hi >= |n| and r >= 0, `dot < -r*len_hi` IMPLIES
// `dot < -r*|n|`. Conservatism is therefore PROVEN by the direction of the
// bound, not sampled. That matters: a sample can always miss a sphere that
// pokes into the frustum by a sliver, and the whole point of the argument is
// that such a sphere must never be dropped.
//
// The planes are deliberately NOT normalised by division (the textbook move).
// Dividing four components by a length introduces four roundings per plane in a
// direction nobody has analysed; comparing against `r * len_hi` needs ONE bound
// and keeps the plane exact.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS NOT
// ---------------------------------------------------------------------------
// It is not `zref::MeshFetch`. GEOM.MESHFETCH's purpose line gives the block
// three jobs; the LOD ladder is `zref::creature::lod_raw` / `lod_update` and is
// already implemented by `zhao_geom_lod`, and the DESCRIPTOR FETCH has no
// format yet — the meshlet schema is explicitly unfrozen (blocks.yml notes,
// "Meshlet limits are Phase-0 data (P2 risk 1)"). Inventing a descriptor layout
// to have something to fetch would be inventing data, so this covers the cull
// and says so. `zref::MeshFetch` stays unresolved in
// reports/PHANTOM_REFERENCES.md until that format is ruled.

#pragma once

#include <cstdint>

#include "zref_fixp.hpp"
#include "zref_trig.hpp"

namespace zref {
namespace cull {

/**
 * A clip-space half-space: the points with `a*x + b*y + c*z + d >= 0` (x,y,z
 * fx16 raw; d is an fx16 word and is therefore scaled by 2^16 relative to the
 * others when the dot product is taken — see `plane_dot`).
 *
 * int64 rather than int32 BECAUSE the components are sums of two fx16 words
 * and reach 2^32 in magnitude. See the header note.
 */
struct Plane {
  int64_t a, b, c, d;
};

/** Plane order, fixed, and shared with the RTL's plane index. */
enum PlaneIndex : int { kLeft = 0, kRight = 1, kBottom = 2, kTop = 3, kNear = 4 };
constexpr int kPlaneCount = 5;

/** The two Duo cameras. */
constexpr int kViewCount = 2;

// ---------------------------------------------------------------------------
// plane extraction — row combinations of the SHIPPED view-projection
// ---------------------------------------------------------------------------

/**
 * The five planes of `vp`, in `PlaneIndex` order.
 *
 * Row combinations, never column combinations: `mat4fx` is row-major and
 * `mat4_vec4` computes M·v, so clip.x/y/w are the row dot products.
 */
inline void frustum_planes(const mat4fx& vp, Plane out[kPlaneCount]) {
  const int64_t r0[4] = {vp.m[0][0].raw, vp.m[0][1].raw, vp.m[0][2].raw, vp.m[0][3].raw};
  const int64_t r1[4] = {vp.m[1][0].raw, vp.m[1][1].raw, vp.m[1][2].raw, vp.m[1][3].raw};
  // row 2 is deliberately unread: this machine has no z clip.
  const int64_t r3[4] = {vp.m[3][0].raw, vp.m[3][1].raw, vp.m[3][2].raw, vp.m[3][3].raw};

  out[kLeft] = {r3[0] + r0[0], r3[1] + r0[1], r3[2] + r0[2], r3[3] + r0[3]};
  out[kRight] = {r3[0] - r0[0], r3[1] - r0[1], r3[2] - r0[2], r3[3] - r0[3]};
  out[kBottom] = {r3[0] + r1[0], r3[1] + r1[1], r3[2] + r1[2], r3[3] + r1[3]};
  out[kTop] = {r3[0] - r1[0], r3[1] - r1[1], r3[2] - r1[2], r3[3] - r1[3]};
  out[kNear] = {r3[0], r3[1], r3[2], r3[3]};
}

// ---------------------------------------------------------------------------
// the ceiling length bound
// ---------------------------------------------------------------------------

/**
 * Exact floor square root over the 66-bit sums of squares this cull produces.
 *
 * THIS IS THE RATIFIED §7.2 RECURRENCE, WIDENED — the identical restoring
 * digit recurrence as `zref::isqrt_u64`, with the starting bit moved from 4^31
 * to 4^32 so that the argument may reach 3*2^64. It is a widening and not a
 * second algorithm, and the differential proves it: `cull_isqrt` is checked
 * against `zref::isqrt_u64` over the whole u64 range where the two overlap
 * (tests/differential/geom_meshfetch_cull_directed.cpp, section 1).
 *
 * Widening is necessary rather than convenient. A plane component reaches 2^32,
 * so a^2+b^2+c^2 reaches 3*2^64 — outside u64 — and the alternative, clamping
 * the argument, would make the length bound depend on a saturation nobody had
 * analysed. Exact is cheaper to reason about than clamped.
 */
constexpr uint64_t cull_isqrt(unsigned __int128 n) {
  unsigned __int128 num = n;
  unsigned __int128 res = 0;
  unsigned __int128 bit = static_cast<unsigned __int128>(1) << 64;  // 4^32
  while (bit > num) bit >>= 2;
  while (bit != 0) {
    if (num >= res + bit) {
      num -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return static_cast<uint64_t>(res);
}

/** a^2 + b^2 + c^2, exact. Reaches 3*2^64, hence the 128-bit accumulator. */
constexpr unsigned __int128 normal_sumsq(const Plane& p) {
  const unsigned __int128 aa = static_cast<unsigned __int128>(static_cast<__int128>(p.a) * p.a);
  const unsigned __int128 bb = static_cast<unsigned __int128>(static_cast<__int128>(p.b) * p.b);
  const unsigned __int128 cc = static_cast<unsigned __int128>(static_cast<__int128>(p.c) * p.c);
  return aa + bb + cc;
}

/**
 * ceil(|(a,b,c)|) — THE SAFE DIRECTION. See the header: a floor here would
 * delete visible geometry near the screen edges.
 *
 * `cull_isqrt` leaves floor(sqrt(n)); the ceiling is one more exactly when n is
 * not a perfect square. The RTL gets that bit for free from the restoring
 * recurrence's own remainder rather than by squaring the result back.
 */
constexpr uint64_t normal_len_ceil(const Plane& p) {
  const unsigned __int128 sq = normal_sumsq(p);
  if (sq == 0) return 0;
  const uint64_t lo = cull_isqrt(sq);
  const unsigned __int128 lo2 = static_cast<unsigned __int128>(lo) * lo;
  return (lo2 < sq) ? (lo + 1) : lo;
}

// ---------------------------------------------------------------------------
// the sphere test
// ---------------------------------------------------------------------------

/**
 * a*cx + b*cy + c*cz + d, exact, in units of 2^-32 world (the plane components
 * and the centre are each fx16, so their product carries 2^-32; `d` is lifted
 * by 16 to match). The scale never has to be undone — the comparison it feeds
 * is against `r * len_hi`, which carries the same 2^-32.
 */
constexpr __int128 plane_dot(const Plane& p, vec3fx c) {
  return static_cast<__int128>(p.a) * c.x.raw + static_cast<__int128>(p.b) * c.y.raw +
         static_cast<__int128>(p.c) * c.z.raw + (static_cast<__int128>(p.d) << 16);
}

/**
 * True iff the sphere (centre, radius) lies WHOLLY outside `p`.
 *
 * `len_ceil` must be `normal_len_ceil(p)`; it is passed in rather than
 * recomputed because it is a per-camera-per-frame quantity and this function is
 * the per-instance path — five square roots per view per frame against
 * potentially thousands of instances.
 */
constexpr bool sphere_outside_plane(const Plane& p, uint64_t len_ceil, vec3fx centre, fx16 radius) {
  const __int128 slack = static_cast<__int128>(radius.raw) * static_cast<__int128>(len_ceil);
  return plane_dot(p, centre) < -slack;
}

/**
 * One camera's cull state: the five planes and their ceiling lengths, valid
 * for as long as the view-projection is unchanged.
 */
struct View {
  Plane plane[kPlaneCount];
  uint64_t len_ceil[kPlaneCount];
};

/** Build a camera's cull state from its view-projection. Per frame, not per instance. */
inline View make_view(const mat4fx& vp) {
  View v{};
  frustum_planes(vp, v.plane);
  for (int i = 0; i < kPlaneCount; ++i) v.len_ceil[i] = normal_len_ceil(v.plane[i]);
  return v;
}

/**
 * True iff the sphere is outside AT LEAST ONE of this camera's five planes,
 * which is the (conservative) statement that this camera cannot see it.
 */
inline bool view_rejects(const View& v, vec3fx centre, fx16 radius) {
  for (int i = 0; i < kPlaneCount; ++i) {
    if (sphere_outside_plane(v.plane[i], v.len_ceil[i], centre, radius)) return true;
  }
  return false;
}

/** The block's whole answer for one instance. */
struct Verdict {
  uint8_t visible_mask;  // bit v: camera v is active AND may see the sphere
  bool reject;           // no active camera may see it
};

/**
 * The ruled law in one function: per-camera visibility, and rejection only when
 * NO active camera can see the sphere.
 *
 * `active_mask` bit v enables camera v. An INACTIVE camera contributes no
 * visibility, so with `active_mask == 0` the verdict is reject — and that is
 * correct rather than a special case: with no active camera nothing is drawn at
 * all, so rejecting deletes nothing. It falls out of the single expression
 * `reject = (visible_mask == 0)` instead of needing a branch.
 */
inline Verdict cull_instance(const View views[kViewCount], uint8_t active_mask, vec3fx centre,
                             fx16 radius) {
  Verdict out{};
  for (int v = 0; v < kViewCount; ++v) {
    const bool active = ((active_mask >> v) & 1u) != 0u;
    if (active && !view_rejects(views[v], centre, radius)) {
      out.visible_mask = static_cast<uint8_t>(out.visible_mask | (1u << v));
    }
  }
  out.reject = (out.visible_mask == 0);
  return out;
}

}  // namespace cull
}  // namespace zref
