// tokens_dev.hpp — the shared driver for MEASURE.TOKENS.
//
// One place that knows the block's cycle shape, so the directed lane and both
// random lanes drive it identically. If the port list changes, exactly one file
// needs editing and every lane stops compiling.
//
// THE CYCLE SHAPE, which is the whole subtlety of this block:
//   * `tok_grant_o` / `tok_shared_o` are COMBINATIONAL — they answer in the
//     same cycle as the request, because GEOM.BINNER's law E samples the grant
//     on the accept edge. So the driver sets the inputs, EVALUATES, reads the
//     grant, and only THEN ticks.
//   * `den_valid_o` and its payload are REGISTERED — they present exactly one
//     cycle later. The driver therefore returns the grant for cycle N and the
//     denial belonging to cycle N-1, and the caller lines them up.

#pragma once

#include <cstdint>

#include "Vzhao_measure_tokens.h"

#include "zhao_sim.hpp"
#include "zref/zref_measure.hpp"

namespace tokens_test {

namespace zm = zref::measure;

/** Everything the block is offered in one cycle. */
struct Stim {
  bool budget_valid = false;
  zm::TokenBudgets budgets;
  zm::TokenRequest req;
  zm::TokenReturn ret;
};

/** What one cycle produced: the combinational answer, and the PREVIOUS
 *  cycle's registered denial (which is what the DUT presents right now). */
struct Obs {
  bool grant = false;
  bool shared = false;
  bool den_valid = false;
  uint8_t den_view = 0, den_class = 0, den_rep = 0, den_reason = 0;
  uint16_t den_src_id = 0;
  uint32_t den_cost = 0;
};

inline void reset_dut(Vzhao_measure_tokens& dut) {
  dut.rst_n = 0;
  dut.clk = 0;
  dut.budget_valid_i = 0;
  dut.budget_geom0_i = 0;
  dut.budget_geom1_i = 0;
  dut.budget_frag0_i = 0;
  dut.budget_frag1_i = 0;
  dut.budget_shared_i = 0;
  dut.req_valid_i = 0;
  dut.req_view_i = 0;
  dut.req_class_i = 0;
  dut.req_essential_i = 0;
  dut.req_rep_i = 0;
  dut.req_cost_i = 0;
  dut.req_src_id_i = 0;
  dut.ret_valid_i = 0;
  dut.ret_view_i = 0;
  dut.ret_class_i = 0;
  dut.ret_shared_i = 0;
  dut.ret_cost_i = 0;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/**
 * Drive ONE cycle. Reads the combinational grant BEFORE the edge (which is what
 * the consumer does) and the registered denial that is standing on the port at
 * the same moment (which belongs to the previous cycle's request).
 */
inline Obs cycle(Vzhao_measure_tokens& dut, const Stim& s) {
  dut.budget_valid_i = s.budget_valid ? 1 : 0;
  dut.budget_geom0_i = s.budgets.geom[0];
  dut.budget_geom1_i = s.budgets.geom[1];
  dut.budget_frag0_i = s.budgets.frag[0];
  dut.budget_frag1_i = s.budgets.frag[1];
  dut.budget_shared_i = s.budgets.shared;
  dut.req_valid_i = s.req.valid ? 1 : 0;
  dut.req_view_i = static_cast<uint8_t>(s.req.view & 1);
  dut.req_class_i = static_cast<uint8_t>(s.req.cls & 1);
  dut.req_essential_i = s.req.essential ? 1 : 0;
  dut.req_rep_i = static_cast<uint8_t>(s.req.rep & 7);
  dut.req_cost_i = s.req.cost;
  dut.req_src_id_i = s.req.src_id;
  dut.ret_valid_i = s.ret.valid ? 1 : 0;
  dut.ret_view_i = static_cast<uint8_t>(s.ret.view & 1);
  dut.ret_class_i = static_cast<uint8_t>(s.ret.cls & 1);
  dut.ret_shared_i = s.ret.shared ? 1 : 0;
  dut.ret_cost_i = s.ret.cost;

  dut.clk = 0;
  dut.eval();  // the combinational grant settles here

  Obs o;
  o.grant = dut.tok_grant_o != 0;
  o.shared = dut.tok_shared_o != 0;
  o.den_valid = dut.den_valid_o != 0;
  o.den_view = dut.den_view_o;
  o.den_class = dut.den_class_o;
  o.den_rep = dut.den_rep_o;
  o.den_reason = dut.den_reason_o;
  o.den_src_id = dut.den_src_id_o;
  o.den_cost = dut.den_cost_o;

  dut.clk = 1;
  dut.eval();
  dut.clk = 0;
  dut.eval();
  return o;
}

/** An idle cycle: nothing offered, nothing returned. */
inline Obs idle_cycle(Vzhao_measure_tokens& dut) {
  Stim s;
  return cycle(dut, s);
}

/** The five pools as the DUT holds them right now. */
struct Pools {
  uint32_t g0 = 0, g1 = 0, f0 = 0, f1 = 0, sh = 0;
  bool operator==(const Pools& o) const {
    return g0 == o.g0 && g1 == o.g1 && f0 == o.f0 && f1 == o.f1 && sh == o.sh;
  }
};

inline Pools pools(const Vzhao_measure_tokens& dut) {
  Pools p;
  p.g0 = dut.avail_geom0_o;
  p.g1 = dut.avail_geom1_o;
  p.f0 = dut.avail_frag0_o;
  p.f1 = dut.avail_frag1_o;
  p.sh = dut.avail_shared_o;
  return p;
}

inline Pools pools(const zm::TokenGuard& g) {
  Pools p;
  p.g0 = g.avail_geom(0);
  p.g1 = g.avail_geom(1);
  p.f0 = g.avail_frag(0);
  p.f1 = g.avail_frag(1);
  p.sh = g.avail_shared();
  return p;
}

}  // namespace tokens_test
