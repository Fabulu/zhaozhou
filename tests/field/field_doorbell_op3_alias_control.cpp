// field_doorbell_op3_alias_control.cpp -- FT060's NEGATIVE CONTROL.
//
// `tests/mutants/zhao_field_doorbell_op3_alias_mutant.sv` restores the
// pre-FH14 catch-all LOAD arm, re-lifted from current production with two
// one-token changes. This driver runs FT060's own stimulus against it and
// PASSES WHEN THE MUTANT MISBEHAVES.
//
// WHY THIS CONTROL IS SHAPED DIFFERENTLY FROM ITS SIBLING. The other doorbell
// mutant exists because `ret_overflow_o` is UNREACHABLE by legal stimulus --
// there the mutant is a POSITIVE control for a counter that can otherwise
// never be seen to move. Op-3 aliasing is not like that: it is reachable with
// an ordinary post, so the honest question is not "can the counter fire" but
// "would my test have NOTICED the old behaviour". A repair whose test passes
// against both the repaired and the broken block has measured nothing, and
// that is the failure ruling R123 records -- a mutation the design's own
// stimulus never exercises is a green control attached to nothing.
//
// So the assertion here is the INVERSE of the production assertion:
//
//   production (field_doorbell_directed FT060):  load_words_o DOES NOT MOVE
//   this file   (the mutant):                    load_words_o MOVES
//
// If this file ever passes against unmutated production, or fails against the
// mutant, the pair has stopped discriminating and FT060's green is worthless.
//
// ENFORCED-BY: itself. Registered as the `field_doorbell_op3_alias_control`
// ctest, label "mutant".

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_field_doorbell_op3_alias_mutant.h"

#include "zhao_sim.hpp"

namespace {

using zhao::check;

using Dut = Vzhao_field_doorbell_op3_alias_mutant;

constexpr uint8_t kOpFh2 = 3;
constexpr uint8_t kFh2Install = 0;
constexpr uint32_t kPlan = 0xF1E1D000u;

void step(Dut& d) { zhao::tick(d); }

void reset(Dut& d, int cycles) {
  d.rst_n = 0;
  d.post_valid_i = 0;
  d.ld_ready_i = 0;
  d.pc_lu_ready_i = 0;
  d.pc_lu_resp_valid_i = 0;
  d.pc_lu_hit_i = 0;
  d.pc_lu_slot_i = 0;
  d.pc_cm_ready_i = 0;
  d.pc_cm_resp_valid_i = 0;
  d.pc_cm_inserted_i = 0;
  d.pc_cm_evicted_i = 0;
  d.pc_cm_slot_i = 0;
  d.ret_ready_i = 0;
  d.cfg_plan_base_i = kPlan;
  d.fh2_ready_i = 0;
  d.fh2_resp_valid_i = 0;
  d.fh2_resp_ok_i = 0;
  d.fh2_resp_verdict_i = 0;
  d.fh2_resp_handle_i = 0;
  d.fh2_resp_slot_i = 0;
  d.fh2_resp_evicted_i = 0;
  for (int i = 0; i < cycles; ++i) step(d);
  d.rst_n = 1;
  step(d);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut dut;
  reset(dut, 4);

  // FT060's stimulus, unchanged: one op-3 INSTALL post.
  dut.post_valid_i = 1;
  dut.post_op_i = kOpFh2;
  dut.post_kind_i = kFh2Install;
  dut.post_slot_i = 0;
  dut.post_addr_i = 0x5;
  dut.post_data_i[0] = 0xDEADBE00u;
  dut.post_data_i[1] = 0x00000000u;
  dut.post_data_i[2] = 0x00001000u;
  dut.post_hash_i = 0x9E3779B9u;
  dut.post_ok_i = 0;
  dut.post_ticket_i = 0x8001u;
  bool taken = false;
  for (int g = 0; g < 64 && !taken; ++g) {
    dut.eval();
    if (dut.post_ready_o) taken = true;
    step(dut);
  }
  dut.post_valid_i = 0;
  check(taken, "the mutant's mailbox took the op-3 post", 1, taken ? 1 : 0);

  // Service it exactly as the production driver would.
  bool fh2_seen = false;
  dut.ld_ready_i = 1;
  dut.pc_lu_ready_i = 1;
  dut.pc_cm_ready_i = 1;
  dut.fh2_ready_i = 1;
  dut.ret_ready_i = 1;
  for (int i = 0; i < 80; ++i) {
    dut.eval();
    if (dut.fh2_valid_o) fh2_seen = true;
    const bool fh2_taken = dut.fh2_valid_o && dut.fh2_ready_i;
    step(dut);
    dut.fh2_resp_valid_i = fh2_taken ? 1 : 0;
    dut.fh2_resp_ok_i = 1;
    dut.fh2_resp_verdict_i = 0;
  }
  dut.fh2_resp_valid_i = 0;

  std::printf("field_doorbell_op3_alias_control: load_words_o=%u fh2_posts_o=%u "
              "fh2_valid seen=%d\n",
              static_cast<unsigned>(dut.load_words_o),
              static_cast<unsigned>(dut.fh2_posts_o), fh2_seen ? 1 : 0);

  // ---- THE INVERTED ASSERTIONS -------------------------------------------
  // Under the restored catch-all, op 3 is neither a commit, a lookup nor
  // (because `head_is_fh2` is forced low) an FH2 command, so it falls into the
  // LOAD arm and performs a real loader write. That is the defect, stated as
  // the thing this file requires to be present.
  check(dut.load_words_o == 1,
        "MUTANT FIRED: op 3 became a LOADER WRITE, which is the pre-FH14 defect "
        "FT060 exists to catch",
        1, static_cast<int>(dut.load_words_o));
  check(dut.fh2_posts_o == 0,
        "MUTANT FIRED: and it never reached the FH2 seam", 0,
        static_cast<int>(dut.fh2_posts_o));
  check(!fh2_seen, "MUTANT FIRED: fh2_valid_o never asserted", 1,
        fh2_seen ? 0 : 1);

  return zhao::report_and_exit("field_doorbell_op3_alias_control");
}
