// geom_paramarena_reservemut.cpp -- THE POSITIVE CONTROL FOR
// `giant_reserve_breach_o`, ITS NEGATIVE CONTROL, AND THE MEASUREMENT THAT
// "THE GIANT IS WHOLE" ACTUALLY MEANS. One source, two ctests.
//
// ===========================================================================
// WHAT IS BEING DEMONSTRATED, AND WHY A MUTANT IS THE ONLY WAY
// ===========================================================================
// `giant_reserve_breach_o` fires when an ACCEPTED ordinary chunk carries total
// allocation into the region reserved for R7's guaranteed giant. Reaching that
// state needs TWO guarantees to be false at once, and they live in two
// different blocks:
//
//   * `zhao_measure_sealplan` refuses any plan whose ordinary chunk demand
//     exceeds MAX_CHUNKS minus the reservation;
//   * `zhao_geom_paramarena`'s `ck_fits_c` refuses any allocation past the
//     sealed quota.
//
// Compose them and no legal stimulus can move the counter -- so "it can fire"
// would stay an argument forever, and CLAUDE.md is explicit that a counter
// asserted zero which has never been seen to move is a claim rather than an
// instrument. The break is a COMMITTED file, generated from production by
// `tools/rtl/gen_paramarena_mutants.py`, renamed so no production source list
// can elaborate it.
//
// TWO CTESTS ARE BUILT FROM THIS ONE FILE:
//
//   geom_paramarena_reservemut_fires   -DZHAO_PARAMARENA_RESERVE_MUT, against
//                                      tests/mutants/zhao_geom_paramarena_reserve_mutant.sv,
//                                      and the counter MUST move. INVERTED
//                                      POLARITY: it passes when the block is
//                                      broken.
//   geom_paramarena_reservemut_silent  no macro, unmutated production, and the
//                                      counter MUST stay at zero -- while the
//                                      SAME stimulus is shown to reach the
//                                      wall, so the silence is the silence of
//                                      a guard that worked rather than of a
//                                      test that never arrived.
//
// The second is not decoration. CLAUDE.md records two combiner mutants that
// passed while measuring UNMUTATED production, because a `-D` could not reach a
// function-like `define and said nothing when it failed to. The seam here is a
// bare `ifdef/`elsif selecting the MODULE NAME, which a `-D` does reach, and
// this pair is what shows it engaged.
//
// ===========================================================================
// THE STIMULUS IS THE SHAPE OF THE REAL FRAME, AT A SIZE THAT RUNS
// ===========================================================================
// `tb_zhao_geom_paramarena` exposes the arena's capacities as parameters "so a
// test can shrink the arena without editing this file", and this ctest takes
// that offer: MAX_CHUNKS = 32 rather than 16,384. The ARITHMETIC IS IDENTICAL
// and only the magnitudes move --
//
//   production   MAX_CHUNKS 16,384   reservation 2,341   ordinary quota 14,043
//   here         MAX_CHUNKS     32   reservation     5   ordinary quota     27
//
// -- and pushing 28 chunks costs thousands of cycles where pushing 14,044
// would cost millions. The property under test is "the cursor cannot reach the
// reserve", which is a statement about two comparisons and not about a
// magnitude.
//
// THE THIRD THING THIS FILE MEASURES, and it is the one the packet's evidence
// bar actually asks for: in the PRODUCTION run, at the moment the ordinary
// stream is refused, the arena still holds exactly `q_giant_chunks_o`
// unallocated chunks. That is what "the giant is whole" means at this seam --
// not that the arena can tell a giant's chunk from an ordinary one (it cannot,
// and `zhao_measure_sealplan`'s header says why), but that the ordinary budget
// provably cannot reach the reserved units.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vtb_zhao_geom_paramarena.h"
#include "zhao_sim.hpp"

using Dut = Vtb_zhao_geom_paramarena;

namespace {

// The shrunk arena, and the reservation expressed against it. See the header:
// these are production's ratio, not production's magnitudes.
constexpr uint32_t kMaxChunks = 32;
constexpr uint32_t kReserveChunks = 5;
constexpr uint32_t kOrdinaryQuota = kMaxChunks - kReserveChunks;  // 27
// R7's reservation in REFERENCES rides the seal beside the chunk figure and is
// carried, not enforced here -- the units stay distinct all the way down.
constexpr uint32_t kReserveRefs = 70;

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
  t.seal_giant_refs_i = 0;
  t.seal_giant_chunks_i = 0;
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

/**
 * Seal the frame the way `zhao_measure_sealplan` seals one that declares the
 * guaranteed giant: the ORDINARY quota, strictly below capacity, with the
 * reservation named beside it in both units.
 */
void seal(Dut& t) {
  t.seal_verts_i = 64;
  t.seal_tris_i = 64;
  t.seal_chunks_i = kOrdinaryQuota;
  t.seal_giant_refs_i = kReserveRefs;
  t.seal_giant_chunks_i = kReserveChunks;
  t.frame_gen_i = 0x1234;
  for (int c = 0; c < 2000; ++c) {
    t.eval();
    if (t.seal_ready_o) break;
    zhao::tick(t);
  }
  t.seal_valid_i = 1;
  zhao::tick(t);
  t.seal_valid_i = 0;
  t.eval();
}

/** Offer one chunk until the arena takes the offer. */
bool push_chunk(Dut& t, uint32_t tag) {
  t.ck_valid_i = 1;
  t.ck_next_i = 0;
  t.ck_count_i = 3;
  for (int k = 0; k < 14; ++k) t.ck_ids_i[k] = tag * 100u + static_cast<uint32_t>(k);

  bool took = false;
  for (int c = 0; c < 4000 && !took; ++c) {
    t.eval();
    took = t.ck_ready_o != 0;
    zhao::tick(t);
  }
  t.ck_valid_i = 0;
  t.eval();
  for (int i = 0; i < 200; ++i) zhao::tick(t);
  t.eval();
  return took;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

#ifdef ZHAO_PARAMARENA_RESERVE_MUT
  const bool expect_fire = true;
  const char* who = "MUTANT (ck_fits_c is `<=`)";
#else
  const bool expect_fire = false;
  const char* who = "PRODUCTION";
#endif

  Dut t;
  bring_up(t);
  seal(t);

  ckt(t.q_giant_chunks_o == kReserveChunks,
      "the seal NAMED the reservation in CHUNKS and the arena latched it");
  ckt(t.q_giant_refs_o == kReserveRefs,
      "and in TILE REFERENCES -- the two units stay distinct through the seal");

  // Fill the ordinary quota exactly. Every one of these must be accepted in
  // BOTH builds: the mutation is one comparison at the boundary and must not
  // change anything before it.
  for (uint32_t i = 0; i < kOrdinaryQuota; ++i) {
    if (!push_chunk(t, i)) {
      ckt(false, "a chunk inside the ordinary quota was never taken");
      break;
    }
  }
  t.eval();
  const uint32_t cursor_at_quota = t.ck_alloc_id_o;
  ckt(cursor_at_quota == kOrdinaryQuota,
      "the cursor sits exactly at the ordinary quota with the quota full");
  ckt(t.quota_overflow_o == 0, "nothing overflowed while inside the quota");
  ckt(t.giant_reserve_breach_o == 0, "and nothing has reached the reserve yet");

  // ONE MORE. This is the whole experiment.
  const uint32_t overflow_before = t.quota_overflow_o;
  push_chunk(t, kOrdinaryQuota);
  t.eval();

  const uint32_t cursor_after = t.ck_alloc_id_o;
  const uint32_t breach = t.giant_reserve_breach_o;
  const bool did_fire = breach != 0;
  const uint32_t unallocated = kMaxChunks - cursor_after;

  std::printf(
      "%s\n"
      "  MAX_CHUNKS / reservation / ordinary quota = %u / %u / %u\n"
      "  q_giant_chunks_o                          = %u\n"
      "  q_giant_refs_o                            = %u\n"
      "  chunk cursor after the extra push         = %u\n"
      "  arena chunks still UNALLOCATED            = %u\n"
      "  quota_overflow_o                          = %u\n"
      "  giant_reserve_breach_o                    = %u\n"
      "  frame_fault_o                             = %u\n",
      who, kMaxChunks, kReserveChunks, kOrdinaryQuota, t.q_giant_chunks_o, t.q_giant_refs_o,
      cursor_after, unallocated, t.quota_overflow_o, breach, t.frame_fault_o);

  if (!expect_fire) {
    // ---- THE PRODUCTION CLAIM, IN THREE PARTS -----------------------------
    // The stimulus REACHED the wall. Without this the silence below would be
    // the silence of a test that never got there, which is the shape CLAUDE.md
    // calls "a gate that cannot reach the state is not evidence about it".
    ckt(t.quota_overflow_o == overflow_before + 1,
        "PRODUCTION: the extra chunk was REFUSED at ck_fits_c and counted");
    ckt(t.frame_fault_o != 0,
        "PRODUCTION: and the frame FAULTED -- the whole-frame fallback, not a truncation");
    // The cursor did not move, so the refusal allocated nothing.
    ckt(cursor_after == kOrdinaryQuota,
        "PRODUCTION: the refused chunk advanced no cursor");
    // AND THE GIANT IS WHOLE, MEASURED RATHER THAN ASSERTED. At the moment the
    // ordinary stream was refused, the arena still held at least the whole
    // reservation in unallocated chunks.
    ckt(unallocated >= kReserveChunks,
        "PRODUCTION: at the refusal the arena still holds the WHOLE reservation, unallocated");
    ckt(unallocated == kReserveChunks,
        "PRODUCTION: exactly the reservation and not one chunk more -- no ordinary budget wasted");
  } else {
    // ---- THE MUTANT'S FAULT MUST BE THE ONE UNDER TEST ---------------------
    // The mutation makes the allocator MORE permissive, so the tell is that
    // `quota_overflow_o` does NOT fire: the frame believes it fits. That is
    // exactly why the breach counter has to exist -- a silent overrun into the
    // reservation moves no other instrument in the tree.
    ckt(t.quota_overflow_o == overflow_before,
        "MUTANT: quota_overflow_o did NOT fire -- the overrun is silent, which is the point");
    ckt(cursor_after == kOrdinaryQuota + 1,
        "MUTANT: the cursor advanced INTO the reserved region");
    ckt(unallocated == kReserveChunks - 1,
        "MUTANT: one chunk of the giant's reservation has been taken by ordinary allocation");
  }

  if (did_fire != expect_fire) {
    ++g_fail;
    std::printf("FAIL: %s -- giant_reserve_breach_o = %u, expected %s\n", who, breach,
                expect_fire ? "NONZERO (the mutant must be caught)"
                            : "ZERO (production must be silent)");
  } else {
    std::printf("ok: %s -- giant_reserve_breach_o = %u (%s)\n", who, breach,
                expect_fire ? "the detector FIRED on the fault it exists for"
                            : "silent, as the negative control requires");
  }

  std::printf("geom_paramarena_reservemut [%s]: %d failures\n", who, g_fail);
  zhao::exit_hard(g_fail == 0 ? 0 : 1);
}
