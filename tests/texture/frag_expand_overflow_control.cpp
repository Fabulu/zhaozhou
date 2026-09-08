// frag_expand_overflow_control.cpp
//
// ---------------------------------------------------------------------------
// THE POSITIVE CONTROL FOR wq_overflow_o
// ---------------------------------------------------------------------------
// `frag_expand_directed` asserts `wq_overflow_o == 0` under correct operation.
// On its own that is a hopeful zero: the counter watches for a state violation
// that is UNREACHABLE while the queue's full-guard is right, so no stimulus can
// make it move. A detector that cannot be reached by any legal input has not
// been tested by any legal input.
//
// So this drives the deliberately broken copy in
// `tests/mutants/zhao_texture_frag_expand_mutant.sv`, whose only substantive
// difference is
//
//     fq_full_c = (fq_occ_c >= FQD)   ->   (fq_occ_c > FQD)
//
// admitting a fifth entry into a four-deep queue. The post-fit brief named
// exactly this mutation as the discriminating one, and the monitor's FIRST
// version could not have caught it: that version tested `accept_c && fq_full_c`,
// which substitutes to `f_valid_i && !fq_full_c && fq_full_c` and is
// algebraically false. The mutation moved acceptance and detection together,
// which is the lockstep-detector failure in miniature.
//
// This test PASSES when the counter FIRES. It is the inverse of the usual
// polarity and that is deliberate: it is evidence about the instrument, not
// about the design.
#include "Vzhao_texture_frag_expand_mutant.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

void tick(Vzhao_texture_frag_expand_mutant* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_frag_expand_mutant* d = new Vzhao_texture_frag_expand_mutant;

  d->clk = 0;
  d->rst_n = 0;
  d->f_valid_i = 0;
  d->req_ready_i = 0;
  d->aux_ready_i = 0;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  // Both sinks held SHUT so nothing drains and the input queue fills. FQD is 4,
  // so a correct guard stops accepting at four; the mutant takes a fifth.
  d->req_ready_i = 0;
  d->aux_ready_i = 0;

  int accepted = 0;
  for (int i = 0; i < 40; ++i) {
    d->f_valid_i = 1;
    d->f_owner_i = static_cast<uint16_t>(((i & 0x3F) << 8) | (i & 0xFF));
    d->f_u_i = 0x00010000 + i;
    d->f_v_i = 0x00020000 - i;
    d->f_binding_i = static_cast<uint8_t>(i);
    d->f_lod_i = static_cast<uint8_t>(i * 3);
    d->f_count_i = 3;  // three samples, so nothing retires early
    d->f_aux_i = 0;
    d->f_class_i = static_cast<uint8_t>(i & 1);
    d->f_ctx_i = 0xC0DE000000000000ull + i;
    d->eval();
    if (d->f_valid_i && d->f_ready_o) ++accepted;
    tick(d);
  }
  d->f_valid_i = 0;
  d->eval();
  tick(d);
  tick(d);
  d->eval();

  std::printf(
      "  mutant with both sinks shut: accepted %d into a 4-deep queue, "
      "wq_overflow_o = %u\n",
      accepted, d->wq_overflow_o);

  zhao::check(accepted > 4,
              "the mutant ACCEPTED more than the four entries its queue owns -- "
              "the >= to > change really does admit a fifth, so the fault this "
              "control depends on is present",
              1, accepted > 4 ? 1 : 0);
  zhao::check(d->wq_overflow_o > 0,
              "and wq_overflow_o FIRED. The monitor derived from the extended "
              "pointer difference detects a queue holding more than it owns, "
              "without reference to f_ready_o -- which is why it catches a fault "
              "that moves acceptance and detection together. The FIRST version of "
              "this monitor tested `accept_c && fq_full_c` and was algebraically "
              "false; this is the demonstration that the replacement is not",
              1, d->wq_overflow_o > 0 ? 1 : 0);

  const int rc = zhao::report_and_exit("frag_expand_overflow_control");
  delete d;
  zhao::exit_hard(rc);
}
