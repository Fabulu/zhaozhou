// walk_meta_hold_directed.cpp -- the walked triangle's material state, held.
//
// WHAT THIS TEST IS EVIDENCE FOR, stated before the code so it can be checked
// against what it actually asserts:
//
//   1. THE RECORD THE JOB READS IS THE RECORD THE CAPTURE TOOK, across an
//      arbitrary gap. That is the whole defect: at GEOM_WALK_RASTER = 1 the
//      door read `pw_t_matstate_w` LIVE at the job accept, several cycles
//      after `zhao_geom_tilewalk` released that record in T_TAKE, so it could
//      pair A's corners with B's material state.
//   2. THE BUS MOVING UNDER THE HOLD CHANGES NOTHING. Case 2 drives a
//      DIFFERENT matstate onto the input every cycle of the gap. A hold that
//      was combinational, or whose enable was wrong, fails here and passes
//      every test that leaves the input still.
//   3. BOTH COUNTERS ARE SEEN TO FIRE, by ordinary stimulus rather than by a
//      mutant, and both are seen to STAY SILENT on legal traffic. A counter
//      asserted zero and never observed to move is a claim; this console has
//      shipped four never-executed assertions in one week.
//   4. AND THE ASSERTIONS ARE ABOUT THE CORRECT BEHAVIOUR, not about the bug.
//      "The counter fires on the swap" would pass only while the defect
//      exists. What is asserted is that the record HOLDS; the counters get
//      their own positive controls, separately, in cases 4 and 5.
//
// WHAT IT DOES NOT COVER, named rather than left to be discovered: the
// composer's WIRING of this block -- that `cap_fire_i` really is the tile
// walker's take and `job_fire_i` really is the shell's walk door -- is not a
// property this bench can see. That is measured in the console smoke, by the
// tagphase probe's `walk_bus_idle` / `ms_nonzero_tag` / `ms_zero_sample` row
// going to zero at the job accept.

#include "Vzhao_walk_meta_hold.h"

#include <cstdint>
#include <cstdio>

#include "zhao_sim.hpp"

namespace {

int checks = 0;
int failures = 0;

void check(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL: %s\n", what);
  }
}

template <typename A, typename B>
void check_eq(A got, B want, const char* what) {
  ++checks;
  if (static_cast<uint64_t>(got) != static_cast<uint64_t>(want)) {
    ++failures;
    std::printf("FAIL: %s (got %llu, want %llu)\n", what,
                static_cast<unsigned long long>(got),
                static_cast<unsigned long long>(want));
  }
}

// The matstate is 128 bits, so Verilator presents it as a four-word array.
// Writing it through a helper keeps the word indices in ONE place: a
// hand-indexed wide signal that is wrong by a word produces a test that drives
// a plausible value and checks the wrong half of it.
void set_ms(Vzhao_walk_meta_hold& top, uint32_t w0, uint32_t w1, uint32_t w2,
            uint32_t w3) {
  top.cap_matstate_i[0] = w0;
  top.cap_matstate_i[1] = w1;
  top.cap_matstate_i[2] = w2;
  top.cap_matstate_i[3] = w3;
}

bool held_is(Vzhao_walk_meta_hold& top, uint32_t w0, uint32_t w1, uint32_t w2,
             uint32_t w3) {
  return top.held_matstate_o[0] == w0 && top.held_matstate_o[1] == w1 &&
         top.held_matstate_o[2] == w2 && top.held_matstate_o[3] == w3;
}

void tick(Vzhao_walk_meta_hold& top) {
  top.clk = 0;
  top.eval();
  top.clk = 1;
  top.eval();
  top.clk = 0;
  top.eval();
}

void reset_dut(Vzhao_walk_meta_hold& top) {
  top.rst_n = 0;
  top.active_i = 0;
  top.cap_fire_i = 0;
  top.job_fire_i = 0;
  set_ms(top, 0, 0, 0, 0);
  top.cap_arena_id_i = 0;
  for (int i = 0; i < 4; ++i) tick(top);
  top.rst_n = 1;
  top.active_i = 1;
  tick(top);
}

// One capture, with the payload presented only on the capture cycle.
void capture(Vzhao_walk_meta_hold& top, uint32_t w0, uint32_t w1, uint32_t w2,
             uint32_t w3, uint32_t id) {
  set_ms(top, w0, w1, w2, w3);
  top.cap_arena_id_i = id;
  top.cap_fire_i = 1;
  tick(top);
  top.cap_fire_i = 0;
}

void job(Vzhao_walk_meta_hold& top) {
  top.job_fire_i = 1;
  tick(top);
  top.job_fire_i = 0;
}

}  // namespace

int main() {
  Vzhao_walk_meta_hold top;

  // =======================================================================
  // CASE 1 -- THE HOLD SURVIVES A GAP. The record is captured, the input is
  //           then left alone, and the job comes eight cycles later.
  // =======================================================================
  {
    reset_dut(top);
    capture(top, 0xAABBCCDDu, 0x11223344u, 0xDEADBEEFu, 0x0F0F0F0Fu, 0x1234);
    check_eq(top.held_valid_o, 1, "case1: the hold is occupied after the capture");
    for (int i = 0; i < 8; ++i) tick(top);
    check(held_is(top, 0xAABBCCDDu, 0x11223344u, 0xDEADBEEFu, 0x0F0F0F0Fu),
          "case1: the matstate is unchanged eight cycles later");
    check_eq(top.held_arena_id_o, 0x1234, "case1: the arena id is unchanged");
    job(top);
    check_eq(top.held_valid_o, 0, "case1: the job consumed the hold");
    check_eq(top.err_job_unheld_o, 0, "case1: no unheld job");
    check_eq(top.err_overwrite_o, 0, "case1: no overwrite");
  }

  // =======================================================================
  // CASE 2 -- THE BUS MOVES UNDERNEATH AND THE HOLD DOES NOT. This is the
  //           case the defect fails: the composer read the bus live, so a
  //           moving bus moved the job's material state with it.
  // =======================================================================
  {
    reset_dut(top);
    capture(top, 0x01020304u, 0x05060708u, 0x090A0B0Cu, 0x0D0E0F10u, 0x2A);
    // GEOM.PARAMWALK advancing to the next record, then to nothing, exactly as
    // it does while the tile walker sits in T_WAIT.
    for (uint32_t k = 1; k <= 6; ++k) {
      set_ms(top, 0xF0000000u + k, 0xF1000000u + k, 0xF2000000u + k,
             0xF3000000u + k);
      top.cap_arena_id_i = 0x3F000u + k;
      tick(top);
    }
    set_ms(top, 0, 0, 0, 0);
    top.cap_arena_id_i = 0;
    tick(top);
    check(held_is(top, 0x01020304u, 0x05060708u, 0x090A0B0Cu, 0x0D0E0F10u),
          "case2: the held matstate ignored seven cycles of a moving bus");
    check_eq(top.held_arena_id_o, 0x2A, "case2: the held arena id ignored it too");
    job(top);
    check_eq(top.err_job_unheld_o, 0, "case2: no unheld job");
    check_eq(top.err_overwrite_o, 0, "case2: no overwrite");
  }

  // =======================================================================
  // CASE 3 -- BACK TO BACK. Two records in a row, each consumed by its own
  //           job, must not blur into each other.
  // =======================================================================
  {
    reset_dut(top);
    capture(top, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x11);
    for (int i = 0; i < 3; ++i) tick(top);
    check(held_is(top, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u),
          "case3: the first record stands");
    job(top);
    capture(top, 0x55555555u, 0x66666666u, 0x77777777u, 0x88888888u, 0x22);
    for (int i = 0; i < 5; ++i) tick(top);
    check(held_is(top, 0x55555555u, 0x66666666u, 0x77777777u, 0x88888888u),
          "case3: the second record replaced it exactly");
    check_eq(top.held_arena_id_o, 0x22, "case3: and so did its arena id");
    job(top);
    check_eq(top.err_job_unheld_o, 0, "case3: no unheld job over two records");
    check_eq(top.err_overwrite_o, 0, "case3: no overwrite over two records");
  }

  // =======================================================================
  // CASE 4 -- `err_job_unheld_o` FIRES. A job taken with nothing held is the
  //           defect's own signature: the door would have read the live bus.
  //           It is unreachable on legal traffic, so it is reached here by
  //           offering a job the walker never captured for.
  // =======================================================================
  {
    reset_dut(top);
    check_eq(top.err_job_unheld_o, 0, "case4: silent before the fault");
    job(top);
    check_eq(top.err_job_unheld_o, 1, "case4: err_job_unheld FIRED");
    // AND IT IS NOT A FREE-RUNNING COUNTER: a legal capture/job pair after it
    // must not move it again. Without this the case passes for a counter that
    // increments on every job.
    capture(top, 0xCAFEBABEu, 0u, 0u, 0u, 0x7);
    job(top);
    check_eq(top.err_job_unheld_o, 1, "case4: a LEGAL job does not move it");
    check_eq(top.err_overwrite_o, 0, "case4: the other counter stayed silent");
  }

  // =======================================================================
  // CASE 5 -- `err_overwrite_o` FIRES. A second record captured before the
  //           first was consumed. This is the serial-handshake violation
  //           `zhao_geom_tilewalk`'s `overlap_o` claims to watch and cannot:
  //           that guard demands `tstate_q != T_TAKE` and then ANDs in
  //           `t_ready_o`, which is only true when `tstate_q == T_TAKE`.
  //           Watched from the CONSUMER'S side, no producer term can cancel it.
  // =======================================================================
  {
    reset_dut(top);
    capture(top, 0xA0A0A0A0u, 0u, 0u, 0u, 0x1);
    check_eq(top.err_overwrite_o, 0, "case5: silent before the fault");
    capture(top, 0xB0B0B0B0u, 0u, 0u, 0u, 0x2);
    check_eq(top.err_overwrite_o, 1, "case5: err_overwrite FIRED");
    // The second record still WINS -- the block does not drop data to report a
    // fault, because a hold that discarded on overwrite would turn one wrong
    // triangle into two.
    check(held_is(top, 0xB0B0B0B0u, 0u, 0u, 0u),
          "case5: the newer record is the one held");
    job(top);
    check_eq(top.err_job_unheld_o, 0, "case5: the other counter stayed silent");
  }

  // =======================================================================
  // CASE 6 -- A SAME-CYCLE CAPTURE AND JOB. The walker cannot produce both on
  //           one clock (T_TAKE and T_JOB are different sub-states), but a
  //           block that is only correct when its caller behaves is a trap for
  //           the next composition. The capture WINS and neither counter moves.
  // =======================================================================
  {
    reset_dut(top);
    capture(top, 0x0BADF00Du, 0u, 0u, 0u, 0x5);
    set_ms(top, 0x600DF00Du, 0u, 0u, 0u);
    top.cap_arena_id_i = 0x6;
    top.cap_fire_i = 1;
    top.job_fire_i = 1;
    tick(top);
    top.cap_fire_i = 0;
    top.job_fire_i = 0;
    check(held_is(top, 0x600DF00Du, 0u, 0u, 0u),
          "case6: the same-cycle capture won");
    check_eq(top.held_valid_o, 1, "case6: and it left the hold occupied");
    check_eq(top.err_overwrite_o, 0, "case6: a consumed hold is not an overwrite");
    check_eq(top.err_job_unheld_o, 0, "case6: the job had a record to read");
  }

  // =======================================================================
  // CASE 7 -- THE SWEEP ENDS. `active_i` low empties the hold SILENTLY: a
  //           walker that finishes a tile with a record taken and no job
  //           issued is a tile boundary, not a fault, and counting it would
  //           make a real counter read nonzero on every good frame.
  // =======================================================================
  {
    reset_dut(top);
    capture(top, 0xFEEDFACEu, 0u, 0u, 0u, 0x9);
    check_eq(top.held_valid_o, 1, "case7: occupied");
    top.active_i = 0;
    tick(top);
    check_eq(top.held_valid_o, 0, "case7: the hold emptied when the sweep ended");
    check_eq(top.err_overwrite_o, 0, "case7: silently");
    check_eq(top.err_job_unheld_o, 0, "case7: and without an unheld job");
    // AND THE CONTROL'S OTHER HALF: re-arming holds again. Without this, case 7
    // passes for a block that is broken rather than idle.
    top.active_i = 1;
    tick(top);
    capture(top, 0x5EA51DE5u, 0u, 0u, 0u, 0xA);
    check_eq(top.held_valid_o, 1, "case7: re-armed, it holds again");
    check(held_is(top, 0x5EA51DE5u, 0u, 0u, 0u), "case7: re-armed, with the new record");
  }

  std::printf("walk_meta_hold_directed: %d checks, %d failures\n", checks,
              failures);
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
