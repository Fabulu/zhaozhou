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

}  // namespace measure
}  // namespace zref
