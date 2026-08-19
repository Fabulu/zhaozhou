// zref_measure.hpp — the MEASURE.TOKENS oracle (phase 8, ZH-048).
//
// The global token guard and the charter §9 Duo fairness split, as a scalar
// state machine. This is a FIRST implementation, not a view onto an executed
// reference, and that is stated plainly: nothing in `reference/src` allocates
// tokens — the software console renders whatever it is given. So unlike
// `zref_terrain_velocity.hpp`, which is a thin view onto `compose_lattice`,
// this header IS the semantics, and every law in it is a decision recorded in
// `design/contracts/MEASURE.TOKENS.md` with what it rejected.
//
// Law, in citation order:
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Duo fairness" — "45%
//     guaranteed to player 1; 45% guaranteed to player 2; 10% shared emergency
//     pool. One player looking directly into a volcano cannot make the other
//     player's army disappear." The block is that sentence, implemented
//     structurally: a view's guaranteed pool is reachable only by that view.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Practical implementation
//     path", Version 1 — "a global token guard rejects only low-priority
//     refinement when the budget is nearly exhausted".
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Representation ladder" — the
//     seven rungs the `lod_representation_counts` lanes count.
//   spec/commands.zidl SetPresentationContract 0x0020 — `u32
//     geometry_tokens[2]`, `u32 fragment_tokens[2]`, `u32 shared_tokens`: the
//     pool set's SHAPE, taken from the ABI verbatim. The shared pool is ONE
//     scalar spanning both views and both classes because the ABI field is a
//     scalar.
//   spec/counters.md §4 — counters saturate, never wrap.
//   fpga/rtl/geometry/zhao_geom_binner.sv law E — the landed consumer's
//     combinational request/grant seam, which is why `grant` here is a pure
//     function of the pre-state and the request.
//
// The nine chosen laws are numbered T1..T9 in the contract and in
// `fpga/rtl/measure/zhao_measure_tokens.sv`; the ones with arithmetic content
// are repeated at their implementation below rather than only listed here.

#pragma once

#include <cstdint>

namespace zref {
namespace measure {

/** §9's representation ladder: seven rungs, plus one spare index. */
inline constexpr int kRepLanes = 8;

/** Denial reasons, wire-identical with `den_reason_o`. */
enum TokenDenyReason : uint8_t {
  kDenyLowPriority = 0,  //!< private pool short and the request is refinement
  kDenyExhausted = 1,    //!< essential, but the emergency pool is short too
  kDenyReload = 2        //!< a budget load landed in the same cycle (law T9)
};

/** The five numbers SetPresentationContract carries. */
struct TokenBudgets {
  uint32_t geom[2] = {0, 0};
  uint32_t frag[2] = {0, 0};
  uint32_t shared = 0;
};

/** One dispatch asking to spend. */
struct TokenRequest {
  bool valid = false;
  int view = 0;        //!< 0 or 1
  int cls = 0;         //!< 0 = geometry, 1 = fragment
  bool essential = 0;  //!< false = low-priority refinement (law T1)
  int rep = 0;         //!< §9 ladder rung, 0..7
  uint32_t cost = 0;
  uint16_t src_id = 0;
};

/** One handback. `shared` echoes the grant's own answer (law T4). */
struct TokenReturn {
  bool valid = false;
  int view = 0;
  int cls = 0;
  bool shared = false;
  uint32_t cost = 0;
};

/** What one cycle produces. `grant`/`shared` are COMBINATIONAL (same cycle);
 *  `deny*` is what the block registers and presents one cycle later. */
struct TokenAnswer {
  bool grant = false;
  bool shared = false;
  bool deny = false;
  TokenDenyReason reason = kDenyLowPriority;
};

/** Saturating 32-bit counter add (spec/counters.md §4: saturate, never wrap). */
inline uint32_t cnt_add(uint32_t cur, uint32_t inc) {
  const uint64_t w = static_cast<uint64_t>(cur) + inc;
  return w > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(w);
}

/**
 * The guard.
 *
 * `step()` is one clock: it returns the COMBINATIONAL answer computed against
 * the pools as they stand, then applies the debit and the credit. Calling it
 * once per cycle with the same stimulus the RTL sees reproduces the RTL
 * bit-for-bit, including the counters.
 */
class TokenGuard {
 public:
  /** Reset: everything zero, so every non-zero request is denied until a load. */
  void reset() { *this = TokenGuard(); }

  /**
   * A budget load (`budget_valid_i`): the five budgets are latched AND every
   * pool is refilled to them. A frame starts with its whole allowance.
   */
  void load(const TokenBudgets& b) {
    bud_ = b;
    for (int v = 0; v < 2; ++v) {
      geom_[v] = b.geom[v];
      frag_[v] = b.frag[v];
    }
    shared_ = b.shared;
  }

  /**
   * ONE CYCLE.
   *
   * LAW T1 — the private pool is tried first, always; the shared pool is a
   * fallback and only `essential` work may reach it. LAW T2 — `view` selects
   * only that view's pools, so there is no path from view 1's requests to
   * view 0's availability. LAW T6 — a return arriving in the same cycle does
   * not help the request: `answer` is computed before the credit is applied.
   * LAW T9 — a request refused because of a same-cycle budget load is refused
   * with a reason, never dropped.
   */
  TokenAnswer step(const TokenRequest& rq, const TokenReturn& rt, bool budget_valid,
                   const TokenBudgets& budgets) {
    TokenAnswer a;

    const uint32_t avail_priv = priv(rq.view, rq.cls);
    const bool fits_priv = rq.cost <= avail_priv;
    const bool fits_shared = rq.cost <= shared_;
    const bool may_share = rq.essential && !fits_priv && fits_shared;

    a.grant = rq.valid && !budget_valid && (fits_priv || may_share);
    a.shared = a.grant && !fits_priv;
    a.deny = rq.valid && !a.grant;
    a.reason = budget_valid ? kDenyReload : (rq.essential ? kDenyExhausted : kDenyLowPriority);

    // ---- counters (both are registered in the RTL; both move here) --------
    // LAW T7: `lod_representation_counts` is GRANTS per §9 ladder rung.
    if (a.grant) rep_count_[rq.rep & 7] = cnt_add(rep_count_[rq.rep & 7], 1u);
    // LAW T8: a denied GEOMETRY request culls its cost in triangles; a denied
    // fragment request culls no triangles, because fragments are not triangles.
    if (a.deny && rq.cls == 0) triangles_culled_ = cnt_add(triangles_culled_, rq.cost);

    // ---- pools ------------------------------------------------------------
    if (budget_valid) {
      load(budgets);
      return a;
    }

    // The debit CANNOT underflow: a grant guarantees cost <= the pool drawn.
    if (a.grant) {
      if (a.shared) {
        shared_ -= rq.cost;
      } else if (rq.cls == 0) {
        geom_[rq.view] -= rq.cost;
      } else {
        frag_[rq.view] -= rq.cost;
      }
    }

    // LAW T4 — the return names the pool its grant drew from.
    // LAW T5 — and it cannot inflate that pool past its budget.
    if (rt.valid) {
      if (rt.shared) {
        shared_ = clamp_add(shared_, rt.cost, bud_.shared);
      } else if (rt.cls == 0) {
        geom_[rt.view] = clamp_add(geom_[rt.view], rt.cost, bud_.geom[rt.view]);
      } else {
        frag_[rt.view] = clamp_add(frag_[rt.view], rt.cost, bud_.frag[rt.view]);
      }
    }
    return a;
  }

  uint32_t avail_geom(int view) const { return geom_[view & 1]; }
  uint32_t avail_frag(int view) const { return frag_[view & 1]; }
  uint32_t avail_shared() const { return shared_; }
  uint32_t rep_count(int lane) const { return rep_count_[lane & 7]; }
  uint32_t triangles_culled() const { return triangles_culled_; }

 private:
  uint32_t priv(int view, int cls) const { return cls ? frag_[view & 1] : geom_[view & 1]; }

  /** `pool + add`, clamped at `cap`. Formed wide, so it cannot wrap (law T5). */
  static uint32_t clamp_add(uint32_t pool, uint32_t add, uint32_t cap) {
    const uint64_t w = static_cast<uint64_t>(pool) + add;
    return w > cap ? cap : static_cast<uint32_t>(w);
  }

  TokenBudgets bud_;
  uint32_t geom_[2] = {0, 0};
  uint32_t frag_[2] = {0, 0};
  uint32_t shared_ = 0;
  uint32_t rep_count_[kRepLanes] = {0, 0, 0, 0, 0, 0, 0, 0};
  uint32_t triangles_culled_ = 0;
};

// ===========================================================================
// MEASURE.GOVERNOR (phase 8, ZH-047) — the per-camera screen-error policy.
//
// Charter section 9 Version 1 is explicit that "ARM predicts a pixel-error
// threshold per camera from prior counters". So this block does NOT predict a
// threshold: it CONVERTS the two numbers charter section 9 gives each camera
// (a projection scale and a pixel-error threshold) into the ONE ratio
// TERRAIN.LOD's Notes law 8 already committed to consuming, and it DEGRADES
// that ratio per view under budget pressure.
//
// THE DIRECTION OF THE RATIO. TERRAIN.LOD's ladder is
// `dev[L] * scale <= distance * h`, coarsest level wins. `scale` multiplies
// the DEVIATION, so larger scale => harder test => FINER. `zref_terrain_lod
// .hpp` says the opposite in prose ("Larger = coarser", "allowed error per
// unit distance"); the prose is backwards relative to the arithmetic in the
// same file, and the arithmetic is what ships and what terrain_lod_directed
// pins. This oracle is written against the arithmetic, and the discrepancy is
// reported in design/contracts/MEASURE.GOVERNOR.md rather than resolved here.
//
//     projected pixels of error ~= dev * proj_scale / distance
//     admissible  <=>  dev * proj_scale / distance <= px_err
//                 <=>  dev * (proj_scale / px_err) <= distance
//
// so scale = proj_scale / px_err. In raw integers, with proj Q8.8 and px_err
// fx16 and the result Q8.8:  scale_raw = proj_raw * 2^16 / px_err_raw.
//
// The seven chosen laws are numbered G1..G7 in the contract and in
// fpga/rtl/measure/zhao_measure_governor.sv.
// ===========================================================================

/** The provisional charter section 9 stability constants (law G5). These are
 *  the RTL's parameter defaults; the ledger already calls them provisional
 *  ("Hysteresis/hold constants provisional until Wound Lab evidence"). */
struct GovernorConstants {
  uint16_t hyst = 320;          //!< Q8.8, 1.25x. 256 would mean NO hysteresis.
  uint8_t min_hold = 6;         //!< frames = 100 ms at 60 Hz
  uint32_t morph_step = 10923;  //!< Q16/frame; min_hold * morph_step >= 65536
  uint8_t deg_hold = 12;        //!< frames before a rung recovers
  uint8_t deg_max = 3;          //!< the top rung
};

/** One camera's inputs for one frame. */
struct GovernorCamera {
  uint32_t px_err = 0;   //!< SetView.pixel_error, fx16 unsigned
  uint16_t proj = 0;     //!< projection scale, Q8.8 unsigned
  bool starved = false;  //!< this view had work refused against its OWN pool
};

/** The `lod_targets` this block publishes. */
struct GovernorTargets {
  uint16_t scale[2] = {256, 256};
  bool en[2] = {true, false};
  uint16_t hyst = 320;
  uint8_t min_hold = 6;
  uint32_t morph_step = 10923;
  uint8_t deg[2] = {0, 0};
  uint16_t src_id = 0;
};

/**
 * LAW G1 — the ratio, with EXACTLY ONE rounding.
 *
 * `round_half_up(n/d) = floor((n + floor(d/2)) / d)` (spec/qformats.md section
 * 3), clamped to the 16-bit Q8.8 port TERRAIN.LOD takes.
 *
 * LAW G2 — `deg` shifts the NUMERATOR right, which divides the ratio by 2^deg,
 * which multiplies the allowed pixel error by 2^deg. A shift is exact, so G1's
 * single rounding survives.
 *
 * LAW G6 — `px_err == 0` is the limit "infinite precision demanded", and the
 * finest the ladder can be asked for is the largest scale, so it yields
 * 0xFFFF. `proj == 0` puts nothing on screen and yields 0, which is the
 * coarsest. Neither is a special case bolted on.
 */
inline uint16_t governor_scale(uint16_t proj, uint32_t px_err, int deg) {
  if (px_err == 0) return 0xFFFFu;
  const uint64_t num = (static_cast<uint64_t>(proj) << (16 - deg)) + (px_err >> 1);
  const uint64_t q = num / px_err;
  return q > 0xFFFFull ? 0xFFFFu : static_cast<uint16_t>(q);
}

/**
 * The governor.
 *
 * `frame()` is one decision. The rungs move FIRST, from the frame that just
 * ended, and the ratios are computed at the NEW rung.
 */
class LodGovernor {
 public:
  explicit LodGovernor(const GovernorConstants& k = GovernorConstants()) : k_(k) {}

  void reset() {
    deg_[0] = deg_[1] = 0;
    hold_[0] = hold_[1] = 0;
    for (int i = 0; i < 4; ++i) rep_[i] = 0;
    // The RTL resets its targets to the ladder's neutral point rather than to
    // zero: a zero scale would put the whole world at its coarsest before the
    // first frame arrives.
    out_ = GovernorTargets();
  }

  /**
   * LAWS G3/G4 — one view's rung. It is a function of THAT VIEW'S state and
   * THAT VIEW'S pressure and nothing else (G3), it climbs immediately and
   * recovers only after `deg_hold` unstarved frames (G4).
   */
  void step_rung(int v, bool starved) {
    if (starved) {
      if (deg_[v] < k_.deg_max) ++deg_[v];
      hold_[v] = 0;  // climbing re-arms the recovery clock
    } else if (deg_[v] == 0) {
      hold_[v] = 0;
    } else if (hold_[v] >= static_cast<uint8_t>(k_.deg_hold - 1)) {
      --deg_[v];
      hold_[v] = 0;
    } else {
      ++hold_[v];
    }
  }

  /** One frame's decision. `view_count` is SetPresentationContract's. */
  GovernorTargets frame(const GovernorCamera cam[2], int view_count, uint16_t src_id) {
    step_rung(0, cam[0].starved);
    step_rung(1, cam[1].starved);

    GovernorTargets t;
    t.scale[0] = governor_scale(cam[0].proj, cam[0].px_err, deg_[0]);
    t.scale[1] = governor_scale(cam[1].proj, cam[1].px_err, deg_[1]);
    t.en[0] = view_count >= 1;
    t.en[1] = view_count >= 2;
    t.hyst = k_.hyst;
    t.min_hold = k_.min_hold;
    t.morph_step = k_.morph_step;
    t.deg[0] = deg_[0];
    t.deg[1] = deg_[1];
    t.src_id = src_id;

    // LAW G7 — frames at each rung, summed over ENABLED views.
    if (view_count >= 1) rep_[deg_[0]] = cnt_add(rep_[deg_[0]], 1u);
    if (view_count >= 2) rep_[deg_[1]] = cnt_add(rep_[deg_[1]], 1u);

    out_ = t;
    return t;
  }

  const GovernorTargets& targets() const { return out_; }
  uint32_t rep_count(int lane) const { return rep_[lane & 3]; }
  int deg(int v) const { return deg_[v & 1]; }
  int hold(int v) const { return hold_[v & 1]; }

 private:
  GovernorConstants k_;
  int deg_[2] = {0, 0};
  uint8_t hold_[2] = {0, 0};
  uint32_t rep_[4] = {0, 0, 0, 0};
  GovernorTargets out_;
};

}  // namespace measure
}  // namespace zref
