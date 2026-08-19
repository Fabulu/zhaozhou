// zref_terrain_lod.hpp — the TERRAIN.LOD oracle.
//
// UNLIKE `zref_terrain_normals.hpp` AND `zref_terrain_tess.hpp`, THIS HEADER IS
// MOSTLY DEFINITION, NOT VIEW — and it says so at every line, because a header
// that quietly invents law is exactly what charter §29-6 exists to prevent.
//
// What is FOUND:
//   · charter §11.5 lists what a terrain projected error COMBINES — "stored
//     coarse-level height deviation; live deformation curvature; camera
//     distance; terrain velocity; semantic importance near units and spell
//     impacts; both camera requirements" — but states no arithmetic.
//   · charter §9 "Stability" makes hysteresis, a minimum hold duration and
//     parent/child geomorph MANDATORY for every LOD path. They are required
//     here; their CONSTANTS are explicitly provisional
//     (design/contracts/MEASURE.GOVERNOR.md: "Hysteresis/hold constants
//     provisional until Wound Lab evidence").
//   · spec/terrain_rules.md §2 derives the coarse height mips (17×17 + 9×9 per
//     surface) and names TERRAIN.LOD as the consumer, so the per-level height
//     deviation is a real stored quantity and not an invention.
//   · spec/qformats.md §7.2 `isqrt_u64` is the RATIFIED exact floor square
//     root, already used for a distance in terrain_rules §3.7 ("the largest
//     distance … floored to whole metres (isqrt of the squared distance)").
//   · The legal level set is {0,1,2,3} because TERRAIN.TESS's `lod_target`
//     encodes it in two bits and the stride must divide 8.
//
// What is CHOSEN here (and argued in fpga/rtl/terrain/zhao_terrain_lod.sv and
// design/contracts/TERRAIN.LOD.md):
//   1. the ladder is `dev[L] · scale ≤ distance`, tested per level, coarsest
//      wins — with EXACT integer arithmetic and no rounding of its own;
//   2. distance is EUCLIDEAN eye-to-subpatch-centre, via the §7.2 isqrt;
//   3. the two cameras combine by taking the FINER decision;
//   4. hysteresis is a band between a strict and a relaxed ladder;
//   5. the hysteresis/hold/morph state RIDES THE PACKET rather than living in
//      a history RAM inside the block;
//   6. curvature, velocity and semantic weight are NOT in v1 — they are folded
//      into `dev` by whoever writes the mips, or they are a later amendment.

#pragma once

#include <cstdint>

#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"  // §7.2 isqrt_u64

namespace zref {
namespace terrain {

/** The legal level set: stride = 1 << level, and the strides divide 8. */
constexpr int kLodLevels = 4;

/** Q16 geomorph unity — TERRAIN.TESS's `job_morph_i` full-scale value. */
constexpr int32_t kMorphOne = 65536;

/** One camera's contribution to the decision (charter §9 "Each camera provides"). */
struct LodCamera {
  int32_t ex = 0, ey = 0, ez = 0;  // the eye, fx16 world units
  /**
   * The governor's per-camera policy, as ONE ratio in Q8.8.
   *
   * CHOSEN. Charter §9 gives the camera "projection scale" and a "per-camera
   * pixel-error threshold" as two numbers, but the ladder only ever uses their
   * QUOTIENT, and carrying the quotient makes the comparison exact integer
   * arithmetic with no rounding of its own. Carrying both would force a
   * division (or a rounding) inside the block for no expressive gain.
   *
   * Read it as "world-units of allowed error per world-unit of distance",
   * scaled by 256. Larger = coarser.
   */
  uint16_t scale = 256;
  bool enabled = true;
};

/** The governor's stability policy (charter §9 — required, constants provisional). */
struct LodPolicy {
  /**
   * The hysteresis band, Q8.8, >= 256 (1.0). The current level is retained
   * while the RELAXED ladder (distance multiplied by this) still admits it.
   */
  uint16_t hyst = 256;
  /** Frames a level must have held before a change may commit. */
  uint8_t min_hold = 0;
  /**
   * Geomorph advance per frame, Q16. ZERO MEANS SNAP: a governor that turns
   * geomorph off gets instant level changes, which is the honest reading —
   * the alternative (a morph that never reaches unity, so the level never
   * changes) would silently freeze the ladder.
   */
  int32_t morph_step = 0;
};

/** One subpatch's inputs: its geometry, its mips, and its own history. */
struct LodSubpatch {
  int32_t cx = 0, cy = 0, cz = 0;  // subpatch centre, fx16 world units
  /**
   * `dev[L]` = the largest |fine − coarse| height deviation this subpatch would
   * suffer at level L, fx16 world units, unsigned. `dev[0]` is ZERO BY
   * DEFINITION (level 0 is the full lattice, so it has no deviation) and any
   * value passed in `dev[0]` is ignored — which is what guarantees the ladder
   * always terminates.
   *
   * NOT ASSUMED MONOTONIC. Physically dev rises with the level, but nothing
   * enforces it, so the ladder is written as "the coarsest level that passes"
   * rather than "the first level that fails".
   */
  uint32_t dev[kLodLevels] = {0, 0, 0, 0};
  int level = 0;      // the level currently being displayed
  int32_t morph = 0;  // the geomorph factor currently being displayed, Q16
  uint8_t hold = 0;   // frames since this level was committed
};

/** What the block emits, plus the history to write back. */
struct LodDecision {
  int level = 0;
  int32_t morph = 0;
  uint8_t hold = 0;
  uint32_t tris = 0;  // predicted triangles for this subpatch at this level
};

/**
 * Squared eye-to-centre distance, saturating at UINT64_MAX.
 *
 * The differences are exact; the sum is formed in s128 and saturated, which is
 * what lets the RTL's 66-bit accumulator agree with this everywhere instead of
 * only inside a declared envelope.
 */
inline uint64_t lod_dsq(const LodSubpatch& sp, const LodCamera& cam) {
  const __int128 dx = static_cast<__int128>(sp.cx) - cam.ex;
  const __int128 dy = static_cast<__int128>(sp.cy) - cam.ey;
  const __int128 dz = static_cast<__int128>(sp.cz) - cam.ez;
  const __int128 s = dx * dx + dy * dy + dz * dz;
  const __int128 lim = static_cast<__int128>(UINT64_MAX);
  return static_cast<uint64_t>(s > lim ? lim : s);
}

/** Eye-to-centre distance in fx16 raw, the §7.2 exact floor root. */
inline uint32_t lod_dist(const LodSubpatch& sp, const LodCamera& cam) {
  return static_cast<uint32_t>(isqrt_u64(lod_dsq(sp, cam)));
}

/**
 * THE LADDER — the coarsest level whose projected error fits the budget.
 *
 * Level L is admissible when
 *
 *     dev[L] · scale  ≤  distance · h                      (all raw integers)
 *
 * which is the exact integer form of `dev_metres · (scale/256) ≤ dist_metres`
 * once both sides are multiplied by 2^24: dev and dist are fx16 (÷2^16) and
 * scale and h are Q8.8 (÷2^8). `h` is 256 for the strict ladder and
 * `policy.hyst` for the relaxed one, so ONE comparator serves both.
 *
 * There is no rounding anywhere in this test. That is deliberate: a projected
 * error compared through a rounded intermediate would flip level at a different
 * distance than the spec's own inequality, and the flip point is the ONLY thing
 * a LOD law is really made of.
 */
inline int lod_ladder(const LodSubpatch& sp, const LodCamera& cam, uint16_t h) {
  const uint64_t rhs = static_cast<uint64_t>(lod_dist(sp, cam)) * h;
  int best = 0;  // dev[0] == 0 always passes, so the ladder always terminates
  for (int L = 1; L < kLodLevels; ++L) {
    const uint64_t lhs = static_cast<uint64_t>(sp.dev[L]) * cam.scale;
    if (lhs <= rhs) best = L;
  }
  return best;
}

/** Triangles a subpatch emits at level L, ignoring void cells and stitching. */
inline uint32_t lod_tris(int level) {
  const uint32_t n = 8u >> level;  // cells per side at this stride
  return 2u * n * n;               // 128, 32, 8, 2
}

/**
 * One subpatch's decision.
 *
 * The band: `T_strict` is the coarsest level the plain ladder admits and
 * `T_relaxed` the coarsest the relaxed ladder admits, so `T_strict ≤ T_relaxed`
 * and the current level is retained while it lies between them. Outside the
 * band the target is the near edge of it — never an overshoot, so a camera
 * moving smoothly walks the ladder one rung at a time.
 *
 * Both cameras are consulted and the FINER decision wins (charter §9 Duo
 * fairness: one player looking into a volcano cannot make the other player's
 * ground coarse). A disabled camera contributes nothing.
 */
inline LodDecision lod_select(const LodSubpatch& sp, const LodCamera cams[2],
                              const LodPolicy& policy) {
  // cppcheck-suppress duplicateAssignExpression  // FALSE POSITIVE on the
  // cppcheck CI pins (2.19.0; local 2.20.0 does not report it). These are two
  // INDEPENDENT minimum-accumulators over the camera loop below, and both
  // legitimately start at the same sentinel — the coarsest level — because a
  // fold to a minimum must start above every candidate. They diverge on the
  // very next lines, where `s` and `r` come from lod_ladder called with
  // DIFFERENT hysteresis arguments (256 versus policy.hyst). Collapsing them
  // into one variable would delete the retention band and with it the whole
  // point of this function.
  int t_strict = kLodLevels - 1;
  int t_relaxed = kLodLevels - 1;
  bool any = false;
  for (int c = 0; c < 2; ++c) {
    if (!cams[c].enabled) continue;
    any = true;
    const int s = lod_ladder(sp, cams[c], 256);
    const int r = lod_ladder(sp, cams[c], policy.hyst < 256 ? 256 : policy.hyst);
    if (s < t_strict) t_strict = s;
    if (r < t_relaxed) t_relaxed = r;
  }
  // No camera at all: nothing is visible, so nothing changes. Stated rather
  // than left to an accidental "coarsest".
  if (!any) {
    t_strict = sp.level;
    t_relaxed = sp.level;
  }

  int want = sp.level;
  if (sp.level < t_strict) {
    want = t_strict;  // finer than even the strict ladder demands: coarsen
  } else if (sp.level > t_relaxed) {
    want = t_relaxed;  // coarser than even the relaxed ladder allows: refine
  }

  LodDecision out;
  out.level = sp.level;
  out.morph = sp.morph < 0 ? 0 : (sp.morph > kMorphOne ? kMorphOne : sp.morph);
  const bool hold_ok = sp.hold >= policy.min_hold;
  const int32_t step = policy.morph_step < 0 ? 0 : policy.morph_step;
  bool changed = false;

  if (want > out.level && hold_ok) {
    // Coarsening: TERRAIN.TESS's morph blends the CURRENT level toward the next
    // coarser one, so the level is held and the factor walks up to unity.
    if (step == 0 || out.morph + step >= kMorphOne) {
      out.level += 1;
      out.morph = 0;
      changed = true;
    } else {
      out.morph += step;
    }
  } else if (want < out.level && hold_ok) {
    // Refining: the finer level is adopted AT ONCE with the factor at unity, so
    // the geometry on screen does not move at the moment of the swap, and the
    // factor then walks back down to zero.
    if (out.morph == 0) {
      out.level -= 1;
      out.morph = (step == 0) ? 0 : kMorphOne;
      changed = true;
    } else {
      out.morph = (step == 0 || out.morph <= step) ? 0 : out.morph - step;
    }
  } else {
    // Holding: settle any partial morph back to the level's own geometry.
    out.morph = (step == 0 || out.morph <= step) ? 0 : out.morph - step;
  }

  out.hold = changed ? 0 : (sp.hold == 255 ? 255 : static_cast<uint8_t>(sp.hold + 1));
  out.tris = lod_tris(out.level);
  return out;
}

}  // namespace terrain
}  // namespace zref
