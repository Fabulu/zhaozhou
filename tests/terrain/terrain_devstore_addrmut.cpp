// terrain_devstore_addrmut.cpp -- THE POSITIVE CONTROL FOR
// `slot_addr_bad_o`, and its negative control, from one source.
//
// Owner ruling R242 moved TERRAIN.DEVSTORE's records into SDRAM.  The block's
// `slot_addr_bad_o` differences the address the memory engine is HOLDING
// against the address the read FSM REQUIRES for the patch and burst it is
// serving.  Those two are loaded by different enables -- the engine's at op
// start, the FSM's at `r_start_i` and at each burst advance -- so it is not
// the lockstep-blind kind of check.  But while the block is correct they are
// equal by construction, SO NO LEGAL STIMULUS CAN MOVE IT.
//
// CLAUDE.md: "A guard you cannot reach with legal stimulus needs a COMMITTED
// MUTANT", and the mutant must be a committed file rather than a temporary
// edit to production RTL, because a temporary edit is a live-tree hazard AND
// leaves the next person the same argument with no evidence.
//
// TWO CTESTS ARE BUILT FROM THIS ONE FILE:
//
//   terrain_devstore_addrmut_fires   -DZHAO_DEVSTORE_ADDRLATCH_MUT, and the
//                                    counter MUST move.  Inverted polarity:
//                                    it passes when the block is broken.
//   terrain_devstore_addrmut_silent  no macro, unmutated production, and the
//                                    counter MUST stay at zero.
//
// The second is not decoration.  A macro seam that never engaged would compile
// production twice, both runs would agree, and the pair would say nothing --
// which is exactly how two combiner mutants once passed while measuring
// unmutated production.
//
// THE STIMULUS IS A READ OF AN UNWRITTEN SLOT, deliberately, and it needs no
// page at all.  A patch read is FIVE bursts: one history row followed by four
// record bursts at four different addresses.  A held address is therefore
// wrong from the second burst onwards, whatever the slot contains.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vtb_terrain_lodpath.h"
#include "zhao_sim.hpp"

using Dut = Vtb_terrain_lodpath;

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
#ifdef ZHAO_DEVSTORE_ADDRLATCH_MUT
  const bool expect_fire = true;
  const char* who = "MUTANT (zhao_terrain_devstore_addrlatch_mutant)";
#else
  const bool expect_fire = false;
  const char* who = "PRODUCTION (zhao_terrain_devstore)";
#endif

  Dut t;
  t.rst_n = 0;
  t.f_start_i = t.f_valid_i = 0;
  t.f_slot_i = t.f_gen_i = 0;
  t.f_epoch_i = 0;
  t.f_src_id_i = 0;
  t.f_h_i = 0;
  t.chk_valid_i = t.chk_stale_i = 0;
  t.r_start_i = 0;
  t.r_slot_i = 0;
  t.r_ready_i = 0;
  t.h_valid_i = 0;
  t.h_level_i = 0;
  t.h_morph_i = 0;
  t.h_hold_i = 0;
  t.cfg_region_ok_i = 1;
  t.cfg_deny_mode_i = 0;
  t.cfg_deny_idx_i = 0;
  t.cfg_rd_latency_i = 0;
  t.cfg_rd_gap_i = 0;
  t.cfg_wr_latency_i = 0;
  t.cfg_grant_hold_i = 0;
  t.cfg_short_mode_i = 0;
  t.cfg_short_idx_i = 0;
  t.cfg_short_beat_i = 0;
  t.cfg_stray_beat_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  // One patch read of slot 3: a history burst and four record bursts.
  t.r_start_i = 1;
  t.r_slot_i = 3;
  t.eval();
  zhao::tick(t);
  t.r_start_i = 0;
  t.eval();

  int taken = 0;
  long long guard = 0;
  while (taken < 16 && guard < 20000) {
    t.r_ready_i = 1;
    t.eval();
    if (t.r_valid_o && t.r_ready_i) ++taken;
    zhao::tick(t);
    t.eval();
    ++guard;
  }
  t.r_ready_i = 0;
  t.eval();

  int fail = 0;
  // THE STREAM MUST STILL COMPLETE EITHER WAY.  A mutant that hung would
  // "pass" a fires-test written as a timeout, and would be measuring the
  // hang rather than the detector.
  if (taken != 16) {
    std::printf("FAIL: %s -- the read stream did not complete (%d of 16)\n", who, taken);
    ++fail;
  }
  if (t.bursts_read_o != 5) {
    std::printf("FAIL: %s -- expected 5 bursts (1 history + 4 records), got %u\n", who,
                t.bursts_read_o);
    ++fail;
  }
  // The engine's addresses stay INSIDE the region either way, so the real
  // guard is silent in both runs.  That is the point: this fault is invisible
  // to MEM.GUARD, which is why the block has to watch for it itself.
  if (t.shadow_viol_o != 0) {
    std::printf("FAIL: %s -- the real guard refused a request (%u)\n", who, t.shadow_viol_o);
    ++fail;
  }

  const bool fired = t.slot_addr_bad_o != 0;
  if (fired != expect_fire) {
    std::printf("FAIL: %s -- slot_addr_bad_o = %u, expected %s\n", who, t.slot_addr_bad_o,
                expect_fire ? "NONZERO (the mutant must be caught)"
                            : "ZERO (production must be silent)");
    ++fail;
  } else {
    std::printf("ok: %s -- slot_addr_bad_o = %u (%s)\n", who, t.slot_addr_bad_o,
                expect_fire ? "the detector FIRED on the fault it exists for"
                            : "silent, as the negative control requires");
  }

  std::printf("terrain_devstore_addrmut [%s]: %d failures\n", who, fail);
  zhao::exit_hard(fail == 0 ? 0 : 1);
}
