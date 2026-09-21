// zref_forge_ring.hpp -- POSITIONS for the four forge families that had none.
//
// Owner decision R234 D2 (2026-09-21, `(owner, explicit)`): the owner *"has
// chosen to pay for the evaluators rather than accept the deferral"*. R199, the
// deferral being reversed, named the debt exactly:
//
//   > four of the six forge families have no evaluator at all.
//   > `zhao_forge_prim_eval` is the LIGHTNING evaluator, RIBBON family only.
//
// `spec/commands.zidl` names the four: *"Four of the six families have NO
// EVALUATOR (fan, tube, shell, billboard)"*. This file is their position law and
// `fpga/rtl/forge/zhao_forge_ring_eval.sv` is its silicon.
//
// THE OTHER TWO ARE NOT HERE, AND THAT IS DELIBERATE
// --------------------------------------------------
// **RIBBON belongs to `zref::forge::eval_job`** (FORGE.PRIM.EVAL) -- the
// owner's lightning law with its jitter streams and branches. Re-deriving it
// here would be a second implementation of ratified arithmetic, which is the
// failure `CLAUDE.md` records for the terrain shade header and the projector's
// two cores. **CLIFF belongs to FORGE.CLIFF**, whose positions come from the
// terrain lattice and not from a parameter block at all.
//
// So this evaluator REFUSES those two families and counts the refusal
// separately from an illegal one: a job naming family 6 is a caller bug, and a
// job naming the ribbon is a DISPATCH bug. Different faults, different fixes,
// and a counter that cannot tell them apart cannot discriminate (R95).
//
// ---------------------------------------------------------------------------
// THE LAW -- ONE SWEPT RING, WHICH IS WHY IT IS ONE EVALUATOR AND NOT FOUR
// ---------------------------------------------------------------------------
// `zhao_forge_prim` walks a (rings x ring-vertices) GRID for every family --
// that is the property that makes it one topology generator instead of six.
// The positions have the same shape:
//
//     C(s)      the centre of ring s, s = 0..N
//     R(s)      the ring's radius at s
//     W(k)      the unit ring direction at vertex k, k = 0..K-1
//     P(s,k)    = C(s) + R(s) * W(k)
//
// and the four families are four settings of N, K and the two sweeps:
//
//     FAN        N = 1,        K = sides   closed   hub ring -> rim ring
//     TUBE       N = segments, K = sides   closed   a swept ring
//     SHELL      N = segments, K = sides   closed   a cone (LINEAR) or dome (DOME)
//     BILLBOARD  N = 1,        K = 2       open     one quad
//
// `N` and `K` are `zhao_forge_prim`'s own `eff_seg_c` and `ring_q`, restated --
// not a second topology law. An OPEN family's "ring" is the width-axis PAIR,
// `C - R*U` then `C + R*U`, which is EXACTLY the pair `zref::forge::eval_job`
// emits for the ribbon. The two evaluators therefore agree about what an open
// ring is without either one importing the other.
//
// ---------------------------------------------------------------------------
// THE TWO SWEEPS
// ---------------------------------------------------------------------------
//     LINEAR  C(s) = A0 + rhu((A1 - A0) * s / N)      a straight sweep
//             R(s) = r0 + rhu((r1 - r0) * s / N)      a linear taper
//
//     DOME    phi(s) = rhu(0x4000 * s / N)            0 -> a QUARTER turn
//             C(s)   = A0 + rescale16((A1 - A0) * sin phi)
//             R(s)   = rescale16(Rlin(s) * cos phi)   Rlin = the LINEAR taper
//
// DOME degrades to LINEAR exactly at its endpoints -- `sin 0 = 0` so `C(0) =
// A0`, `sin(quarter) = 0x10000` EXACTLY so `C(N) = A1` with no rounding slack,
// and `cos 0 = 0x10000` so `R(0) = r0`. Those are arithmetic facts of the frozen
// `SIN_Q16` table, not approximations, and the directed test asserts them.
//
// DOME is legal on the SHELL alone (`spec/cartridge.md` 4d). Sky domes and
// bowls are radial-shell uses; allowing the sweep elsewhere would make it a
// second, silent family selector.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS THE HOUSE ARITHMETIC, NOT A NEW ONE
// ---------------------------------------------------------------------------
// * The lerp is `zref::forge::eval_lerp_off` -- the same exact rational
//   `floor((2*D*i + N) / (2N))` FORGE.PRIM.EVAL uses, INCLUDED rather than
//   retyped, so the two evaluators cannot drift apart on the one operation they
//   share. One bit-serial restoring divide in silicon, zero DSP.
// * The ring angle is that same function: `theta(k) = rhu(0x10000 * k / K)`, an
//   exact rational, so a ring of 3, 5, 6 or 7 sides is uniform to the ulp
//   rather than accumulating a stepped error. The turn is the WIDTH of
//   `angle16`, so a closed ring closes by INDEX and never by angle.
// * Sine and cosine are `zref::fx_sin` / `zref::fx_cos` -- the ratified
//   `spec/qformats.md` 7.1 quarter-wave law, which `zhao_field_sin` already
//   implements and `field_sin_directed` already holds equal. **No new
//   trigonometry is introduced**, which is FORGE.PRIM.md's own requirement:
//   *"a ring here and a rotation elsewhere agree exactly."*
// * Every rounding is the qformats 4 rescale, round-half-up, and the ring
//   displacement is FUSED -- one exact sum, one rounding -- exactly as
//   FORGE.PRIM.EVAL's `disp_c` is. Every overflow SATURATES and is counted;
//   nothing wraps.
//
// ---------------------------------------------------------------------------
// `sat_events` COUNTS OPERATIONS IN A DECLARED ORDER
// ---------------------------------------------------------------------------
// A saturation count is only comparable between two implementations if both
// perform the same operations in the same sequence. The order is fixed here and
// the RTL follows it: per ring, C.x, C.y, C.z, then R; per vertex, W.x, P.x,
// W.y, P.y, W.z, P.z. Under DOME each centre component costs TWO sites (the
// rescale of the sine product, then the add) and the radius costs two likewise.
#pragma once

#include <cstdint>
#include <vector>

#include "zref_forge.hpp"
#include "zref_forge_eval.hpp"  // eval_lerp_off -- INCLUDED, never retyped
#include "zref_trig.hpp"        // fx_sin / fx_cos, the ratified qformats 7.1 law

namespace zref {
namespace forge_ring {

/** The silicon family encoding, `zhao_forge_prim.sv`'s FAM_*. */
enum Family : int {
  kFamRibbon = 0,
  kFamFan = 1,
  kFamTube = 2,
  kFamShell = 3,
  kFamBillboard = 4,
  kFamCliff = 5,
};

enum Sweep : int { kSweepLinear = 0, kSweepDome = 1 };

inline constexpr int kMaxSegments = 64;  // FORGE.PRIM.md, frozen
inline constexpr int kMaxSides = 8;      // FORGE.PRIM.md, frozen

/** A quarter turn in angle16 -- the DOME sweep's full excursion. */
inline constexpr int32_t kQuarterTurn = 0x4000;
/** A whole turn is the WIDTH of angle16, which is why a ring closes by index. */
inline constexpr int32_t kWholeTurn = 0x10000;

struct Vec3 {
  int32_t x = 0, y = 0, z = 0;  // fx16
};

struct Params {
  Vec3 anchor0, anchor1;  //!< ring 0's centre and ring N's
  Vec3 axis_u, axis_v;    //!< the ring plane, caller-normalised, used as supplied
  int32_t radius0 = 0;    //!< fx16, >= 0
  int32_t radius1 = 0;    //!< fx16, >= 0
  int family = kFamTube;
  int sweep = kSweepLinear;
  int segments = 1;  //!< 1..64
  int sides = 1;     //!< 1..8
  int view_mask = 3;
  uint16_t src_id = 0;
};

/**
 * The refusal taxonomy, in the order the block applies it.
 *
 * `kRefusedElsewhere` is the one worth reading twice: the ribbon and the cliff
 * are LEGAL families whose positions another block owns. Folding them into
 * `kRefusedFamily` would make a dispatch error look like a caller error.
 */
enum Verdict : int {
  kAccept = 0,
  kRefusedFamily = 1,     //!< family outside 0..5
  kRefusedElsewhere = 2,  //!< ribbon (FORGE.PRIM.EVAL) or cliff (FORGE.CLIFF)
  kRefusedLimit = 3,      //!< segments or sides outside the frozen limits
  kSkippedView = 4,       //!< a well-formed job for another view: not an error
};

struct Counts {
  uint64_t rings = 0;
  uint64_t vertices = 0;
  uint64_t sat_events = 0;  //!< one per saturating component operation
};

inline bool family_ring_closed(int family) {
  return family == kFamFan || family == kFamTube || family == kFamShell;
}

/** `zhao_forge_prim`'s `eff_seg_c`, restated -- NOT a second topology law. */
inline int eff_segments(const Params& p) {
  return (p.family == kFamFan || p.family == kFamBillboard) ? 1 : p.segments;
}

/** `zhao_forge_prim`'s `ring_q`: `sides` closed, the width-axis PAIR open. */
inline int ring_vertices(const Params& p) { return family_ring_closed(p.family) ? p.sides : 2; }

inline int vertex_count(const Params& p) { return (eff_segments(p) + 1) * ring_vertices(p); }

inline Verdict verdict(const Params& p, int view_sel) {
  if (p.family < 0 || p.family > kFamCliff) return kRefusedFamily;
  if (p.family == kFamRibbon || p.family == kFamCliff) return kRefusedElsewhere;
  if (p.segments < 1 || p.segments > kMaxSegments) return kRefusedLimit;
  if (p.sides < 1 || p.sides > kMaxSides) return kRefusedLimit;
  if ((p.view_mask & view_sel) == 0) return kSkippedView;
  return kAccept;
}

// ---- the primitives, all of them borrowed rather than re-derived ----------

/** Saturate an exact wide sum to s32, counting. `satw` in the RTL. */
inline int32_t sat_s32(int64_t x, Counts& c) {
  if (x > INT64_C(2147483647)) {
    ++c.sat_events;
    return INT32_C(2147483647);
  }
  if (x < INT64_C(-2147483648)) {
    ++c.sat_events;
    return INT32_C(-2147483648);
  }
  return static_cast<int32_t>(x);
}

/** qformats 4: `(x + 2^15) >> 16`, round-half-up, then saturate. `rs16sat`. */
inline int32_t rescale16(__int128 x, Counts& c) {
  const __int128 r = (x + 32768) >> 16;  // arithmetic shift: floor, so ties go UP
  if (r > __int128(INT64_C(2147483647))) {
    ++c.sat_events;
    return INT32_C(2147483647);
  }
  if (r < __int128(INT64_C(-2147483648))) {
    ++c.sat_events;
    return INT32_C(-2147483648);
  }
  return static_cast<int32_t>(static_cast<int64_t>(r));
}

/**
 * `rhu(scale * i / n)` as the exact rational `floor((2*scale*i + n) / (2n))`.
 *
 * This is `zref::forge::eval_lerp_off`, CALLED and not copied -- the one
 * operation this evaluator and the lightning evaluator share, so they cannot
 * drift apart on it.
 */
inline int64_t lerp_off(int64_t scale, int i, int n) { return zref::forge::eval_lerp_off(scale, i, n); }

/** The ring angle at vertex k of a K-vertex ring, exact. */
inline uint16_t ring_angle(int k, int K) {
  return static_cast<uint16_t>(lerp_off(kWholeTurn, k, K) & 0xFFFF);
}

/** The DOME sweep's angle at ring s of N -- 0 to a quarter turn, exact. */
inline uint16_t dome_angle(int s, int N) {
  return static_cast<uint16_t>(lerp_off(kQuarterTurn, s, N) & 0xFFFF);
}

/**
 * Evaluate one job into `out`, in `zhao_forge_prim`'s ring-major order:
 * ring s = 0..N outer, vertex k = 0..K-1 inner, so `vidx(s,k) = s*K + k` --
 * exactly the walk the topology generator's indices reference.
 *
 * A refused or view-skipped job emits NOTHING. `out` is cleared first, so a
 * caller reusing a vector cannot read a previous job's tail as this one's.
 */
inline Verdict eval_job(const Params& p, int view_sel, std::vector<Vec3>& out, Counts& c) {
  out.clear();
  const Verdict v = verdict(p, view_sel);
  if (v != kAccept) return v;

  const int N = eff_segments(p);
  const int K = ring_vertices(p);
  const bool closed = family_ring_closed(p.family);
  const bool dome = (p.sweep == kSweepDome);

  const int64_t d0 = int64_t(p.anchor1.x) - p.anchor0.x;
  const int64_t d1 = int64_t(p.anchor1.y) - p.anchor0.y;
  const int64_t d2 = int64_t(p.anchor1.z) - p.anchor0.z;
  const int64_t dr = int64_t(p.radius1) - p.radius0;
  const int32_t a0[3] = {p.anchor0.x, p.anchor0.y, p.anchor0.z};
  const int64_t dd[3] = {d0, d1, d2};
  const int32_t uu[3] = {p.axis_u.x, p.axis_u.y, p.axis_u.z};
  const int32_t vv[3] = {p.axis_v.x, p.axis_v.y, p.axis_v.z};

  out.reserve(size_t(N + 1) * size_t(K));

  for (int s = 0; s <= N; ++s) {
    ++c.rings;

    int32_t C[3];
    int32_t R;
    if (!dome) {
      for (int k = 0; k < 3; ++k) C[k] = sat_s32(int64_t(a0[k]) + lerp_off(dd[k], s, N), c);
      R = sat_s32(int64_t(p.radius0) + lerp_off(dr, s, N), c);
    } else {
      const uint16_t phi = dome_angle(s, N);
      const int64_t sn = fx_sin(angle16{phi}).raw;
      const int64_t cs = fx_cos(angle16{phi}).raw;
      for (int k = 0; k < 3; ++k) {
        const int32_t off = rescale16(__int128(dd[k]) * sn, c);
        C[k] = sat_s32(int64_t(a0[k]) + off, c);
      }
      const int32_t rlin = sat_s32(int64_t(p.radius0) + lerp_off(dr, s, N), c);
      R = rescale16(__int128(int64_t(rlin)) * cs, c);
    }

    // Fold the radius into the ring plane ONCE PER RING rather than once per
    // vertex: the same two products serve every vertex of the ring, and the
    // fused displacement below keeps the single-rounding law regardless.
    int32_t RU[3], RV[3];
    for (int k = 0; k < 3; ++k) {
      RU[k] = rescale16(__int128(int64_t(R)) * uu[k], c);
      RV[k] = rescale16(__int128(int64_t(R)) * vv[k], c);
    }

    for (int k = 0; k < K; ++k) {
      int64_t cu, cv;
      if (closed) {
        const uint16_t th = ring_angle(k, K);
        cu = fx_cos(angle16{th}).raw;
        cv = fx_sin(angle16{th}).raw;
      } else {
        // An OPEN ring is the width-axis PAIR: `C - R*U` then `C + R*U`. The
        // same two vertices, in the same order, that the ribbon evaluator
        // emits -- so the two agree without either importing the other.
        cu = (k == 0) ? -int64_t(kWholeTurn) : int64_t(kWholeTurn);
        cv = 0;
      }
      Vec3 P;
      int32_t comp[3];
      for (int a = 0; a < 3; ++a) {
        // FUSED: one exact sum of two products, ONE rounding (qformats 4).
        const __int128 fused = __int128(int64_t(RU[a])) * cu + __int128(int64_t(RV[a])) * cv;
        const int32_t W = rescale16(fused, c);
        comp[a] = sat_s32(int64_t(C[a]) + W, c);
      }
      P.x = comp[0];
      P.y = comp[1];
      P.z = comp[2];
      out.push_back(P);
      ++c.vertices;
    }
  }
  return kAccept;
}

}  // namespace forge_ring
}  // namespace zref
