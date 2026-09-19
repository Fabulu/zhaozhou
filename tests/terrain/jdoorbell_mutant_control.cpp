// jdoorbell_mutant_control.cpp -- POSITIVE CONTROL for zhao_terrain_jdoorbell's
// `ret_overflow_o`. INVERTED POLARITY: this PASSES when the guard FIRES.
//
// The mutant (tests/mutants/zhao_terrain_jdoorbell_mutant.sv) removes the
// credit, so grants are consumed without reserving return space. Refused jobs
// each owe one FINAL record; with the HPS not taking returns, the (2*TICKETS+1)th
// record has nowhere to go. The production block holds the (TICKETS+1)th job
// instead -- jdoorbell_directed.cpp case 6 -- so the counter never moves there.
// It is evidence about the instrument, not about the design.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_terrain_jdoorbell_mutant.h"

#include "zhao_sim.hpp"

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_jdoorbell_mutant t;
  constexpr int kTickets = 4;

  t.post_valid_i = 0;
  t.sj_valid_i = 0;
  t.wj_ready_i = 0;
  t.landed_valid_i = 0;
  t.done_valid_i = 0;
  t.ret_ready_i = 0;  // the HPS never takes a return
  t.cfg_journal_base_i = 0x30000000u;
  t.rst_n = 0;
  for (int i = 0; i < 3; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  int jobs = 0;
  for (int n = 0; n < 2 * kTickets + 2; ++n) {
    t.post_valid_i = 1;
    t.post_slot_i = n;
    t.post_ticket_i = 0x900 + n;
    t.eval();
    zhao::tick(t);
    t.post_valid_i = 0;
    t.sj_valid_i = 1;
    t.sj_slot_i = n;
    t.wj_ready_i = 1;
    t.eval();
    const bool fired = t.sj_ready_o && t.wj_valid_o;
    zhao::tick(t);
    t.sj_valid_i = 0;
    t.wj_ready_i = 0;
    if (fired) {
      ++jobs;
      // refused before landing: one FINAL, which the HPS never takes
      t.done_valid_i = 1;
      t.done_slot_i = n;
      t.done_seq_i = 0x900 + n;
      t.done_ok_i = 0;
      t.done_verdict_i = 3;
      t.eval();
      zhao::tick(t);
      t.done_valid_i = 0;
    }
    t.eval();
  }

  const uint32_t ov = t.ret_overflow_o;
  std::printf("mutant: %d jobs consumed with no credit, ret_overflow_o=%u\n", jobs, ov);
  if (jobs > 2 * kTickets && ov > 0) {
    std::printf("PASS jdoorbell_mutant_control -- with the credit removed the guard FIRES\n");
    zhao::exit_hard(0);
  }
  std::printf("FAIL jdoorbell_mutant_control -- the guard did not fire (jobs=%d)\n", jobs);
  zhao::exit_hard(1);
}
