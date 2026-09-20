// field_doorbell_mutant_control.cpp -- THE POSITIVE CONTROL for
// `ret_overflow_o` in zhao_field_doorbell.sv.
//
// INVERTED POLARITY. This program PASSES when the counter FIRES and FAILS when
// it reads zero. It is evidence about the INSTRUMENT, not about the design, and
// its DUT is `tests/mutants/zhao_field_doorbell_mutant.sv` -- a renamed copy of
// production with the return credit removed and nothing else.
//
// WHY IT HAS TO BE A MUTANT. The guarded state is unreachable while the credit
// is right: an answerable post is consumed only while fewer than RETQ of them
// still owe their return record, and each owes exactly one. No legal stimulus
// can reach the overflow, so "the counter can fire" would remain an argument
// and its zero in `field_doorbell_directed` would be a claim quoted as a
// measurement -- the broken-instrument law exactly.
//
// THE NEGATIVE CONTROL is `field_doorbell_directed`'s case 7: the same block,
// unmutated, under lawful stimulus, must read the counter zero. Both halves are
// required before that zero may be cited.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_doorbell_mutant.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;
using Dut = Vzhao_field_doorbell_mutant;

constexpr uint8_t kOpLookup = 2;

void step(Dut& d) { zhao::tick(d); }

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;

  dut.rst_n = 0;
  dut.post_valid_i = 0;
  dut.ld_ready_i = 1;
  dut.pc_lu_ready_i = 1;
  dut.pc_lu_resp_valid_i = 0;
  dut.pc_lu_hit_i = 0;
  dut.pc_lu_slot_i = 0;
  dut.pc_cm_ready_i = 1;
  dut.pc_cm_resp_valid_i = 0;
  dut.pc_cm_inserted_i = 0;
  dut.pc_cm_evicted_i = 0;
  dut.pc_cm_slot_i = 0;
  // THE HPS NEVER DRAINS. That is the whole stimulus: with the credit present
  // this simply back-pressures the mailbox and nothing is lost, which is the
  // behaviour `field_doorbell_directed` case 5 asserts. With the credit gone it
  // walks the return queue past its own depth.
  dut.ret_ready_i = 0;
  dut.cfg_plan_base_i = 0xF1E1D000u;
  for (int i = 0; i < 4; ++i) step(dut);
  dut.rst_n = 1;
  step(dut);

  // Post lookups continuously and play the directory. RETQ is 4, so five
  // answered lookups with nothing drained is one more record than the queue
  // owns.
  int posted = 0;
  for (int cycle = 0; cycle < 2000 && dut.ret_overflow_o == 0; ++cycle) {
    dut.post_valid_i = (posted < 12) ? 1 : 0;
    dut.post_op_i = kOpLookup;
    dut.post_kind_i = 0;
    dut.post_slot_i = 0;
    dut.post_addr_i = 0;
    dut.post_data_i[0] = 0;
    dut.post_data_i[1] = 0;
    dut.post_data_i[2] = 0;
    dut.post_hash_i = 0xFEEDBEEFu + posted;
    dut.post_ok_i = 0;
    dut.post_ticket_i = 0x9000u + posted;

    dut.eval();
    const bool took_post = dut.post_valid_i && dut.post_ready_o;
    const bool lu_taken = dut.pc_lu_valid_o && dut.pc_lu_ready_i;
    step(dut);
    if (took_post) ++posted;
    // The played directory answers one cycle after it takes the request.
    dut.pc_lu_resp_valid_i = lu_taken ? 1 : 0;
  }
  dut.post_valid_i = 0;

  std::printf("field_doorbell_mutant_control: posted=%d ret_overflow_o=%u\n", posted,
              static_cast<unsigned>(dut.ret_overflow_o));

  // INVERTED: firing is the pass.
  check(dut.ret_overflow_o > 0,
        "MUTANT: ret_overflow_o FIRED with the return credit removed -- the "
        "detector works, so production's zero is a measurement",
        1, dut.ret_overflow_o > 0 ? 1 : 0);

  return zhao::report_and_exit("field_doorbell_mutant_control");
}
