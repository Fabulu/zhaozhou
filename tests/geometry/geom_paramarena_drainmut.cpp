// geom_paramarena_drainmut.cpp -- THE POSITIVE CONTROL FOR `addr_view_bad_o`,
// AND ITS NEGATIVE CONTROL, FROM ONE SOURCE.
//
// ===========================================================================
// WHY THIS FILE EXISTS
// ===========================================================================
// `zhao_geom_paramarena`'s header says `addr_view_bad_o` "IS NOT REACHABLE
// WITH LEGAL STIMULUS while the drain precondition is correct, which is why it
// needs a COMMITTED MUTANT rather than an argument".  This is that argument
// being replaced by a measurement.
//
// The counter differences the view the HELD request address lies in against
// the view the LEASE names.  Its two operands load on DIFFERENT enables --
// `m_addr_q` when the engine takes an op, `view_q` at the frame seal -- so it
// is NOT the lockstep-blind shape CLAUDE.md's metadata-bank law is about: it
// can structurally see a request that outlived its selector.  What makes it
// unreachable is `drained_c`: a seal is held pending while `wr_words_q != 0`
// or the engine is not idle, so no request ever survives a view flip.
//
// TWO CTESTS ARE BUILT FROM THIS ONE FILE:
//
//   geom_paramarena_drainmut_fires    -DZHAO_PARAMARENA_DRAIN_MUT, against
//                                     tests/mutants/zhao_geom_paramarena_drain_mutant.sv,
//                                     and the counter MUST move.  INVERTED
//                                     POLARITY: it passes when the block is
//                                     broken.
//   geom_paramarena_drainmut_silent   no macro, unmutated production, and the
//                                     counter MUST stay at zero.
//
// The second is not decoration.  CLAUDE.md records two combiner mutants that
// passed while measuring UNMUTATED production, because a `-D` could not reach
// a function-like `define and said nothing when it failed to.  The seam here is
// a bare `ifdef selecting the MODULE NAME, which a `-D` does reach, and this
// pair is what shows it engaged.
//
// ===========================================================================
// THE STIMULUS, AND WHY IT IS THE STIMULUS
// ===========================================================================
// `addr_view_bad_o` counts a cycle in which the engine is in M_REQ -- holding
// an address, waiting for the socket -- while `view_q` names the OTHER view.
// So the fault needs a seal to land while a request is held unaccepted, and
// that needs the socket to be BUSY for more than a cycle.
//
// It is made busy legally, by the share's own backpressure.  `zhao_mem_share_wr`
// carries an RQ-deep retirement ledger and withholds every offer while it has
// fewer than two free entries (`ledger_full_o` counts it).  A write retires
// only when the SDRAM controller returns its credits, tens of clocks later, so
// pushing records back to back fills the ledger and the arena sits in M_REQ for
// a long window.  Nothing here is illegal stimulus: it is ordinary traffic at
// full rate against a real memory.
//
// Then `seal_valid_i` is held.  In PRODUCTION the seal is refused for as long
// as anything is outstanding and `view_flip_blocked_o` counts every clock of
// it -- which this driver ALSO asserts in the silent build, because "the
// counter did not fire" is only evidence if the stimulus that should have
// fired it actually arrived.  In the MUTANT the seal takes effect immediately,
// the view moves under the held address, and the counter fires.
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vtb_zhao_geom_paramarena.h"
#include "zhao_sim.hpp"

using Dut = Vtb_zhao_geom_paramarena;

namespace {

int g_fail = 0;

void ckt(bool cond, const char* what) {
  if (!cond) {
    ++g_fail;
    std::printf("FAIL: %s\n", what);
  }
}

void bring_up(Dut& t) {
  t.clk = 0;
  t.rst_n = 0;
  // No workaround knob: the lease the guard sees is the arena's own output.
  // (`geom_paramarena_directed.cpp` case 1a is the regression guard on it.)
  t.seal_valid_i = 0;
  t.seal_verts_i = 0;
  t.seal_tris_i = 0;
  t.seal_chunks_i = 0;
  t.frame_gen_i = 0;
  t.frame_end_i = 0;
  t.pv_valid_i = 0;
  t.pv_x_i = 0;
  t.pv_y_i = 0;
  t.pv_invw_i = 0;
  t.pv_status_i = 0;
  t.pv_uow_i = 0;
  t.pv_vow_i = 0;
  t.pv_rgba_i = 0;
  t.td_valid_i = 0;
  t.td_v0_i = 0;
  t.td_v1_i = 0;
  t.td_v2_i = 0;
  t.td_material_i = 0;
  t.td_raster_i = 0;
  t.td_source_i = 0;
  t.ck_valid_i = 0;
  t.ck_next_i = 0;
  t.ck_count_i = 0;
  for (int k = 0; k < 14; ++k) t.ck_ids_i[k] = 0;
  t.walk_valid_i = 0;
  t.walk_head_i = 0;
  t.t_ready_i = 0;
  t.peek_en_i = 0;
  t.peek_waddr_i = 0;
  t.poke_en_i = 0;
  t.poke_waddr_i = 0;
  t.poke_data_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
  for (int i = 0; i < 400 && !t.init_done_o; ++i) zhao::tick(t);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
#ifdef ZHAO_PARAMARENA_DRAIN_MUT
  const bool expect_fire = true;
  const char* who = "MUTANT (zhao_geom_paramarena_drain_mutant)";
#else
  const bool expect_fire = false;
  const char* who = "PRODUCTION (zhao_geom_paramarena)";
#endif

  Dut t;
  bring_up(t);
  ckt(t.init_done_o != 0, "the SDRAM controller finished its init sequence");

  // A frame with room for everything this driver pushes, so nothing faults on
  // quota and the run is about the view and nothing else.
  t.seal_valid_i = 1;
  t.seal_verts_i = 256;
  t.seal_tris_i = 256;
  t.seal_chunks_i = 256;
  t.frame_gen_i = 0x0101;
  bool sealed = false;
  for (int i = 0; i < 2000 && !sealed; ++i) {
    t.eval();
    sealed = t.seal_ready_o != 0;
    zhao::tick(t);
  }
  t.seal_valid_i = 0;
  t.eval();
  ckt(sealed, "the first seal took effect");
  const uint32_t view_after_first_seal = t.pb_wr_view_o;

  // ------------------------------------------------------------------------
  // THE WINDOW.  Descriptors are offered continuously -- `td_valid_i` held
  // high, which is how a live producer behaves -- and from the moment the
  // ledger starts backing up, `seal_valid_i` is held high beside them with a
  // DIFFERENT generation.
  //
  // In production this is a seal that is simply never ready.  In the mutant it
  // is a view that flips out from under whatever the engine is holding.
  // ------------------------------------------------------------------------
  t.td_valid_i = 1;
  t.td_v0_i = 0;
  t.td_v1_i = 1;
  t.td_v2_i = 2;
  t.td_material_i = 0x1234;
  t.td_raster_i = 0xABCD0000u;
  t.td_source_i = 0x0000FEEDu;

  int accepted = 0;
  int seal_offered_clocks = 0;
  for (int c = 0; c < 20000; ++c) {
    t.eval();
    if (t.td_ready_o) ++accepted;
    // Start offering the seal once enough records are in flight that the
    // share's ledger is backing the socket up.
    if (accepted >= 3) {
      t.seal_valid_i = 1;
      t.seal_verts_i = 256;
      t.seal_tris_i = 256;
      t.seal_chunks_i = 256;
      t.frame_gen_i = 0x0202;
      ++seal_offered_clocks;
    }
    // Vary the descriptor a little so a wrong-address write is distinguishable
    // in memory from a right one; the counter is the verdict, but a run whose
    // records are all identical hides a swap from any later inspection.
    t.td_source_i = 0x0000FE00u + static_cast<uint32_t>(accepted & 0xFF);
    zhao::tick(t);
    if (accepted >= 40) break;
  }
  t.td_valid_i = 0;
  t.seal_valid_i = 0;
  t.eval();
  for (int i = 0; i < 4000; ++i) zhao::tick(t);
  t.eval();

  // ------------------------------------------------------------------------
  // THE STIMULUS MUST HAVE ARRIVED.  "The counter did not fire" is evidence
  // only if the thing that should have fired it was actually presented -- a
  // gate that cannot reach the state is not evidence about the state.
  // ------------------------------------------------------------------------
  ckt(accepted >= 40, "the record stream ran to completion");
  ckt(seal_offered_clocks > 0, "a seal was OFFERED while records were in flight");
  ckt(t.share_ledger_full_o > 0,
      "the share's ledger really did back the socket up (the M_REQ window exists)");

  const uint32_t fired = t.addr_view_bad_o;
  const bool did_fire = fired != 0;

  std::printf(
      "%s\n"
      "  addr_view_bad_o      = %u\n"
      "  view_flip_blocked_o  = %u\n"
      "  pb_wr_view_o         = %u (was %u after the first seal)\n"
      "  seal offered for     = %d clocks\n"
      "  records accepted     = %d\n"
      "  share ledger_full_o  = %u\n"
      "  guard_violations     = %u\n"
      "  tris_written_o       = %u\n",
      who, fired, t.view_flip_blocked_o, static_cast<unsigned>(t.pb_wr_view_o),
      view_after_first_seal, seal_offered_clocks, accepted, t.share_ledger_full_o,
      t.guard_violations_o, t.tris_written_o);

  if (!expect_fire) {
    // THE DRAIN MUST BE SEEN TO HAVE DONE WORK.  Without this the silent build
    // would pass on a run in which no seal was ever refused -- which is the
    // "gate that cannot reach the state" trap, wearing the clothes of a green
    // negative control.
    ckt(t.view_flip_blocked_o > 0,
        "PRODUCTION: view_flip_blocked_o moved -- the drain precondition REFUSED a seal");
    ckt(t.guard_violations_o == 0,
        "PRODUCTION: the guard refused nothing -- no request outlived its selector");
  }

  if (did_fire != expect_fire) {
    ++g_fail;
    std::printf("FAIL: %s -- addr_view_bad_o = %u, expected %s\n", who, fired,
                expect_fire ? "NONZERO (the mutant must be caught)"
                            : "ZERO (production must be silent)");
  } else {
    std::printf("ok: %s -- addr_view_bad_o = %u (%s)\n", who, fired,
                expect_fire ? "the detector FIRED on the fault it exists for"
                            : "silent, as the negative control requires");
  }

  std::printf("geom_paramarena_drainmut [%s]: %d failures\n", who, g_fail);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
