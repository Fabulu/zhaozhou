// geom_paramarena_alignmut.cpp -- THE POSITIVE CONTROL FOR
// `burst_unaligned_o`, AND ITS NEGATIVE CONTROL, FROM ONE SOURCE.
//
// ===========================================================================
// WHY THIS FILE EXISTS, AND WHY IT IS THE ONLY EVIDENCE AVAILABLE
// ===========================================================================
// `zhao_sdram_ctrl` sets the mode register to BL8 SEQUENTIAL and derives its
// column straight from the byte address (`waddr = req.addr[26:1]`,
// `req_col = waddr[10:0]`), so a column is one 16-bit word and the aligned
// eight-column block a JEDEC sequential burst wraps inside is SIXTEEN BYTES.
// `zhao_vram_arbiter.burst_words` chops a request into
// `min(remaining, 8, row_tail)` words, which aligns to the 2048-word ROW and
// to nothing finer.  A burst therefore wraps exactly when
// `(start_col mod 8) + words > 8`.
//
// THE BEHAVIOURAL SDRAM MODEL READS AND WRITES LINEARLY.  So a misaligned
// request is served CORRECTLY here and WRONGLY by the part, and NO FUNCTIONAL
// TEST IN THIS REPOSITORY CAN FAIL ON IT -- not a shadow-memory comparison,
// not a decode round trip, not a framebuffer CRC, in either polarity.  That is
// the whole reason `zhao_geom_paramarena` COUNTS the invariant instead of
// relying on it, and the reason the counter's silence is worth nothing until
// the counter has been seen to move.  This file is where it moves.
//
// TWO CTESTS ARE BUILT FROM THIS ONE FILE:
//
//   geom_paramarena_alignmut_fires    -DZHAO_PARAMARENA_ALIGN_MUT, against
//                                     tests/mutants/zhao_geom_paramarena_align_mutant.sv,
//                                     and the counter MUST move.  INVERTED
//                                     POLARITY: it passes when the block is
//                                     broken.
//   geom_paramarena_alignmut_silent   no macro, unmutated production, and the
//                                     counter MUST stay at zero.
//
// The second is not decoration.  CLAUDE.md records two combiner mutants that
// passed while measuring UNMUTATED production, because a `-D` could not reach
// a function-like `define and said nothing when it failed to.  The seam here is
// a bare `ifdef/`elsif selecting the MODULE NAME, which a `-D` does reach, and
// this pair is what shows it engaged.
//
// ===========================================================================
// THE STIMULUS, AND WHY IT IS THE STIMULUS
// ===========================================================================
// The mutant restores the pre-repair layout: a 24-byte vertex stride and no
// round-up of the sub-region offsets.  That makes THREE separate things
// misaligned, and this driver exercises all three so a partial repair could
// not pass:
//
//   descriptors  TRI_OFF_B becomes 65,535 * 24 = 1,572,840, which is 8 mod 16,
//                so EVERY TriangleDescriptor write is misaligned.  This is the
//                record type with a live production producer -- the eight the
//                console smoke reports as `arena_tris=8`.
//   vertices     the 24-byte stride puts every ODD vertex 8 bytes into an
//                aligned block, so the counter must fire on some vertices and
//                not others.  A run of only one vertex would prove nothing.
//   chunks       CHUNK_OFF_B inherits the same offset.
//
// Nothing illegal is presented.  This is ordinary traffic, at full rate,
// against the real guard, the real arbiter and the real `zhao_sdram_ctrl` --
// the same arrangement the acceptance bench uses.  The ONLY difference between
// the two builds is which module the `ifdef selected.
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

// Offer one record of one kind until the arena takes it, then drop the valid.
// Returns false if it was never accepted, which the caller asserts on -- an
// offer that was refused for the whole window is a stimulus that never
// arrived, and a counter's silence after one is not evidence.
bool push(Dut& t, int kind, uint32_t tag) {
  t.pv_valid_i = (kind == 0);
  t.td_valid_i = (kind == 1);
  t.ck_valid_i = (kind == 2);
  t.pv_x_i = static_cast<int32_t>(0x00010000u + tag);
  t.pv_y_i = static_cast<int32_t>(0x00020000u + tag);
  t.pv_invw_i = 0x00ABCDu;
  t.pv_status_i = static_cast<uint8_t>(tag & 0xFF);
  t.pv_uow_i = static_cast<int32_t>(tag);
  t.pv_vow_i = static_cast<int32_t>(~tag);
  t.pv_rgba_i = 0xFF00FF00u ^ tag;
  t.td_v0_i = static_cast<uint16_t>(tag * 3u + 0u);
  t.td_v1_i = static_cast<uint16_t>(tag * 3u + 1u);
  t.td_v2_i = static_cast<uint16_t>(tag * 3u + 2u);
  t.td_material_i = 0x1234;
  t.td_raster_i = 0xABCD0000u;
  t.td_source_i = 0x0000FE00u + tag;
  t.ck_next_i = 0;
  t.ck_count_i = 3;
  for (int k = 0; k < 14; ++k) t.ck_ids_i[k] = tag * 100u + static_cast<uint32_t>(k);

  bool took = false;
  for (int c = 0; c < 4000 && !took; ++c) {
    t.eval();
    const bool rdy = (kind == 0) ? t.pv_ready_o
                   : (kind == 1) ? t.td_ready_o
                                 : t.ck_ready_o;
    took = rdy != 0;
    zhao::tick(t);
  }
  t.pv_valid_i = 0;
  t.td_valid_i = 0;
  t.ck_valid_i = 0;
  t.eval();
  // Let the write reach the socket and the engine return to idle, so the next
  // record's address is computed from an advanced cursor rather than racing it.
  for (int i = 0; i < 300; ++i) zhao::tick(t);
  t.eval();
  return took;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
#ifdef ZHAO_PARAMARENA_ALIGN_MUT
  const bool expect_fire = true;
  const char* who = "MUTANT (zhao_geom_paramarena_align_mutant)";
#else
  const bool expect_fire = false;
  const char* who = "PRODUCTION (zhao_geom_paramarena)";
#endif

  Dut t;
  bring_up(t);
  ckt(t.init_done_o != 0, "the SDRAM controller finished its init sequence");

  // Room for everything, so nothing faults on quota and the run is about
  // addresses and nothing else.
  t.seal_valid_i = 1;
  t.seal_verts_i = 256;
  t.seal_tris_i = 256;
  t.seal_chunks_i = 256;
  t.frame_gen_i = 0x0303;
  bool sealed = false;
  for (int i = 0; i < 2000 && !sealed; ++i) {
    t.eval();
    sealed = t.seal_ready_o != 0;
    zhao::tick(t);
  }
  t.seal_valid_i = 0;
  t.eval();
  ckt(sealed, "the seal took effect");

  // EIGHT VERTICES, so both parities of the 24-byte stride occur; then four
  // descriptors and two chunks, whose whole sub-region is displaced by the
  // unrounded offset.
  bool all_taken = true;
  for (uint32_t i = 0; i < 8; ++i) all_taken &= push(t, 0, i);
  for (uint32_t i = 0; i < 4; ++i) all_taken &= push(t, 1, 0x40u + i);
  for (uint32_t i = 0; i < 2; ++i) all_taken &= push(t, 2, 0x80u + i);

  for (int i = 0; i < 4000; ++i) zhao::tick(t);
  t.eval();

  // ------------------------------------------------------------------------
  // THE STIMULUS MUST HAVE ARRIVED.  A gate that cannot reach the state is not
  // evidence about the state, and a silent counter on a run where nothing was
  // ever written is the most reassuring wrong answer available here.
  // ------------------------------------------------------------------------
  ckt(all_taken, "every record offered was accepted");
  ckt(t.verts_written_o == 8, "eight vertices were written");
  ckt(t.tris_written_o == 4, "four descriptors were written");
  ckt(t.chunks_written_o == 2, "two chunks were written");
  ckt(t.arena_overrun_o == 0, "no allocation left the view");
  ckt(t.quota_overflow_o == 0, "no allocation exceeded the sealed quota");

  const uint32_t fired = t.arena_burst_unaligned_o;
  const bool did_fire = fired != 0;

  std::printf(
      "%s\n"
      "  arena_burst_unaligned_o = %u\n"
      "  walk_burst_unaligned_o  = %u\n"
      "  verts/tris/chunks       = %u / %u / %u\n"
      "  guard_violations        = %u\n"
      "  arena_overrun_o         = %u\n",
      who, fired, t.walk_burst_unaligned_o, t.verts_written_o, t.tris_written_o,
      t.chunks_written_o, t.guard_violations_o, t.arena_overrun_o);

  if (!expect_fire) {
    // The guard must also have passed every one of those requests, or the
    // silence would be the silence of a block that never got to issue.
    ckt(t.guard_violations_o == 0,
        "PRODUCTION: the guard refused nothing -- every aligned request was legal");
  } else {
    // THE MUTANT'S OWN FAULT MUST BE THE ONE UNDER TEST.  A misaligned address
    // is still inside the view and still legal to the guard, so a guard
    // violation here would mean the copy broke something else as well.
    ckt(t.guard_violations_o == 0,
        "MUTANT: the guard refused nothing -- the fault is ALIGNMENT, not permission");
  }

  if (did_fire != expect_fire) {
    ++g_fail;
    std::printf("FAIL: %s -- burst_unaligned_o = %u, expected %s\n", who, fired,
                expect_fire ? "NONZERO (the mutant must be caught)"
                            : "ZERO (production must be silent)");
  } else {
    std::printf("ok: %s -- burst_unaligned_o = %u (%s)\n", who, fired,
                expect_fire ? "the detector FIRED on the fault it exists for"
                            : "silent, as the negative control requires");
  }

  std::printf("geom_paramarena_alignmut [%s]: %d failures\n", who, g_fail);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
