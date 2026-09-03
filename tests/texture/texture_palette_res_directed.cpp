// texture_palette_res_directed.cpp — can a reload ever hand back the wrong
// colour to a request that was already in flight?
//
// ---------------------------------------------------------------------------
// THE GATE THE BRIEF NAMES
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md, TMU gates: "Palette generation/invalidation under
// requests in flight."
//
// That is the whole reason a generation exists rather than a valid bit. A slot
// can be reloaded while lookups resolved against its OLD contents are still
// moving, and there are exactly three possible behaviours:
//
//   * return the NEW palette under the OLD binding -- fast and WRONG. A
//     creature briefly wears another creature's colours: obvious in motion,
//     invisible in a still frame, and impossible to trace back to a block.
//   * clear the slot and make everything miss -- correct but throws away
//     lookups that were perfectly fine.
//   * report STALE for exactly the affected lookups -- what this block does.
//
// A test that only checked "the right colour comes back when nothing changes"
// would pass on all three. So the central case here reloads a slot WHILE
// lookups against the old generation are outstanding, and requires every one
// of them to be flagged stale rather than answered.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_palette_res.h"

#include "zhao_sim.hpp"

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_texture_palette_res top;

  auto reset = [&]() {
    top.ld_valid_i = 0;
    top.lu_valid_i = 0;
    top.rst_n = 0;
    for (int i = 0; i < 6; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  // ---- the ruled protocol: BEGIN / WRITE / END (X6) ------------------------
  constexpr int kBEGIN = 0, kWRITE = 1, kEND = 2;

  auto op = [&](int o, int slot, int gen, int idx, uint16_t val, bool crc_ok) {
    top.ld_valid_i = 1;
    top.ld_op_i = o;
    top.ld_slot_i = slot;
    top.ld_gen_i = gen;
    top.ld_idx_i = idx;
    top.ld_rgb565_i = val;
    top.ld_crc_ok_i = crc_ok ? 1 : 0;
    zhao::tick(top);
    top.ld_valid_i = 0;
  };

  auto begin_load = [&](int slot, int gen) { op(kBEGIN, slot, gen, 0, 0, true); };
  auto write_e = [&](int idx, uint16_t v) { op(kWRITE, 0, 0, idx, v, true); };
  auto end_load = [&](int slot, int gen, bool crc_ok = true) {
    op(kEND, slot, gen, 0, 0, crc_ok);
  };

  // Load one whole slot with a known pattern at a given generation.
  auto load_slot = [&](int slot, int gen, uint32_t seed) {
    begin_load(slot, gen);
    for (int e = 0; e < 256; ++e)
      write_e(e, static_cast<uint16_t>(seed + e));
    end_load(slot, gen);
    zhao::tick(top);
  };

  // ---- 1: a cold slot is not resident, and says so -----------------------
  {
    reset();
    top.lu_valid_i = 1;
    top.lu_slot_i = 0;
    top.lu_gen_i = 0;
    top.lu_idx_i = 7;
    zhao::tick(top);
    top.lu_valid_i = 0;
    top.eval();
    zhao::check(top.lu_valid_o == 1, "a lookup answers", 1, top.lu_valid_o);
    zhao::check(top.lu_resident_o == 0,
                "a slot that was never loaded reports NOT resident", 0,
                top.lu_resident_o);
  }

  // ---- 2: after a load, the right colour comes back -----------------------
  {
    reset();
    load_slot(0, 1, 0x1000);
    int bad = 0;
    for (int e = 0; e < 256; ++e) {
      top.lu_valid_i = 1;
      top.lu_slot_i = 0;
      top.lu_gen_i = 1;
      top.lu_idx_i = e;
      zhao::tick(top);
      top.eval();
      if (top.lu_valid_o) {
        // The read is REGISTERED at the same edge that captures the index, so
        // the value out after this tick belongs to index e -- not e-1. The
        // first draft assumed a one-clock skew that is not there and reported
        // 255 of 256 wrong while the RTL was correct. An off-by-one in the
        // EXPECTATION looks exactly like an off-by-one in the design.
        const uint16_t want = static_cast<uint16_t>(0x1000 + e);
        if (top.lu_rgb565_o != want || top.lu_stale_o || !top.lu_resident_o) ++bad;
      }
    }
    top.lu_valid_i = 0;
    zhao::check(bad == 0, "every entry reads back its loaded value, resident and fresh",
                0, bad);
  }

  // ---- 3: THE GATE -- reload under requests in flight ---------------------
  {
    reset();
    load_slot(0, 1, 0x2000);

    // Start a stream of lookups against generation 1, then reload the slot to
    // generation 2 WHILE they are in flight. Every lookup issued at gen 1
    // after the reload begins must be reported STALE -- never answered with
    // generation 2's data.
    int answered_wrong = 0, stale_seen = 0, fresh_before = 0;
    for (int c = 0; c < 40; ++c) {
      // The reload starts at c == 10 and runs concurrently with lookups.
      if (c >= 10 && c < 20) {
        // BEGIN first, then writes. The generation moves on at BEGIN, which
        // is what makes the interleaved lookups stale from the FIRST clock of
        // the reload rather than from the last -- the window this case exists
        // to close.
        top.ld_valid_i = 1;
        top.ld_crc_ok_i = 1;
        top.ld_slot_i = 0;
        top.ld_gen_i = 2;
        if (c == 10) {
          top.ld_op_i = kBEGIN;
        } else {
          top.ld_op_i = kWRITE;
          top.ld_idx_i = c - 11;
          top.ld_rgb565_i = static_cast<uint16_t>(0x9000 + (c - 11));
        }
      } else {
        top.ld_valid_i = 0;
      }

      top.lu_valid_i = 1;
      top.lu_slot_i = 0;
      top.lu_gen_i = 1;          // the OLD binding
      top.lu_idx_i = 5;
      top.eval();
      if (top.lu_valid_o) {
        if (top.lu_stale_o) {
          ++stale_seen;
        } else {
          // Answered as fresh. Only legitimate BEFORE the reload started.
          if (c > 12) ++answered_wrong;
          else ++fresh_before;
        }
      }
      zhao::tick(top);
    }
    top.lu_valid_i = 0;
    top.ld_valid_i = 0;

    zhao::check(answered_wrong == 0,
                "no gen-1 lookup is answered as fresh once the slot moved to gen 2",
                0, answered_wrong);
    zhao::check(stale_seen > 0, "and the affected lookups ARE flagged stale", 1,
                stale_seen > 0 ? 1 : 0);
    std::printf("  reload under flight: %d stale, %d fresh-before, %d wrong\n",
                stale_seen, fresh_before, answered_wrong);
  }

  // ---- 4: a mid-load slot is not resident ---------------------------------
  // The generation advances on the FIRST beat, so a half-written palette can
  // never answer under either generation.
  {
    reset();
    load_slot(0, 1, 0x3000);
    // begin a reload but do NOT finish it
    begin_load(0, 2);
    for (int e = 0; e < 8; ++e) {
      write_e(e, static_cast<uint16_t>(0x4000 + e));
    }
    // a lookup at the NEW generation, mid-load
    top.lu_valid_i = 1;
    top.lu_slot_i = 0;
    top.lu_gen_i = 2;
    top.lu_idx_i = 3;
    zhao::tick(top);
    top.lu_valid_i = 0;
    top.eval();
    zhao::check(top.lu_stale_o == 0, "a mid-load lookup at the NEW generation is not stale",
                0, top.lu_stale_o);
    zhao::check(top.lu_resident_o == 0,
                "but it is NOT resident until the load completes", 0,
                top.lu_resident_o);
  }

  // ---- 5: slots are independent ------------------------------------------
  {
    reset();
    load_slot(0, 1, 0x5000);
    load_slot(1, 1, 0x6000);
    load_slot(0, 2, 0x7000);   // reload slot 0 only

    top.lu_valid_i = 1;
    top.lu_slot_i = 1;
    top.lu_gen_i = 1;
    top.lu_idx_i = 4;
    zhao::tick(top);
    top.lu_valid_i = 0;
    top.eval();
    zhao::check(top.lu_stale_o == 0 && top.lu_resident_o == 1,
                "reloading one slot does not disturb another", 1,
                (top.lu_stale_o == 0 && top.lu_resident_o == 1) ? 1 : 0);
  }


  // ---- X6: THE PROTOCOL'S OWN FAILURE MODES -------------------------------
  // The implicit protocol this replaced could not express any of these. It had
  // no way to say a load FAILED, no way to notice one was INCOMPLETE, and no
  // opinion about a reload that did not move the generation on.

  // A CRC failure leaves the slot NONRESIDENT.
  {
    reset();
    load_slot(1, 1, 0x1000u);            // a good load first
    const uint32_t crc_before = top.err_crc_o;
    begin_load(1, 2);
    for (int e = 0; e < 256; ++e) write_e(e, static_cast<uint16_t>(0xBEEF + e));
    end_load(1, 2, /*crc_ok=*/false);
    zhao::tick(top);

    top.lu_valid_i = 1; top.lu_slot_i = 1; top.lu_gen_i = 2; top.lu_idx_i = 3;
    zhao::tick(top);
    top.lu_valid_i = 0;
    zhao::tick(top);
    top.eval();
    zhao::check(top.lu_resident_o == 0,
                "a CRC-failed load leaves the slot NONRESIDENT -- the caller "
                "takes the cold path, which is slower and correct",
                0, top.lu_resident_o);
    zhao::check(top.err_crc_o == crc_before + 1, "and the failure is counted", 1,
                static_cast<int>(top.err_crc_o - crc_before));
  }

  // An INCOMPLETE load leaves the slot nonresident, even with a good CRC.
  // A palette that is 255 entries of new data and one of old is the kind of
  // wrong that looks right on most pixels.
  {
    reset();
    const uint32_t inc_before = top.err_incomplete_o;
    begin_load(2, 1);
    for (int e = 0; e < 255; ++e) write_e(e, static_cast<uint16_t>(0x2000 + e));
    end_load(2, 1, /*crc_ok=*/true);
    zhao::tick(top);
    top.lu_valid_i = 1; top.lu_slot_i = 2; top.lu_gen_i = 1; top.lu_idx_i = 0;
    zhao::tick(top);
    top.lu_valid_i = 0;
    zhao::tick(top);
    top.eval();
    zhao::check(top.lu_resident_o == 0,
                "ONE missing entry out of 256 leaves the slot nonresident", 0,
                top.lu_resident_o);
    zhao::check(top.err_incomplete_o == inc_before + 1,
                "and the incompleteness is counted separately from a CRC "
                "failure -- one is a bad cartridge, the other a bad loader",
                1, static_cast<int>(top.err_incomplete_o - inc_before));
  }

  // A DUPLICATE entry does not fill the hole left by a missing one. This is
  // why presence is a bit per entry and not a counter: a counter cannot tell
  // 256 distinct writes from 255 plus a repeat.
  {
    reset();
    const uint32_t inc_before = top.err_incomplete_o;
    begin_load(3, 1);
    for (int e = 0; e < 255; ++e) write_e(e, static_cast<uint16_t>(0x3000 + e));
    write_e(0, 0x1234);                   // 256 writes, 255 distinct entries
    end_load(3, 1, true);
    zhao::tick(top);
    zhao::check(top.err_incomplete_o == inc_before + 1,
                "256 writes with a DUPLICATE is still an incomplete load", 1,
                static_cast<int>(top.err_incomplete_o - inc_before));
  }

  // NEVER RELOAD A SLOT WITH THE SAME GENERATION.
  {
    reset();
    load_slot(0, 5, 0x4000u);
    const uint32_t same_before = top.err_same_gen_o;
    begin_load(0, 5);                     // same generation again
    zhao::tick(top);
    zhao::check(top.err_same_gen_o == same_before + 1,
                "BEGIN reusing a slot's current generation is REFUSED -- every "
                "handle to the old palette would still match the new one",
                1, static_cast<int>(top.err_same_gen_o - same_before));
    // and the refusal did not disturb the resident palette
    top.lu_valid_i = 1; top.lu_slot_i = 0; top.lu_gen_i = 5; top.lu_idx_i = 9;
    zhao::tick(top);
    top.lu_valid_i = 0;
    zhao::tick(top);
    top.eval();
    zhao::check(top.lu_resident_o == 1 && top.lu_stale_o == 0,
                "and the slot it refused to reload is untouched", 1,
                (top.lu_resident_o && !top.lu_stale_o) ? 1 : 0);
  }

  // A WRITE outside a load is refused, not applied to a live palette.
  {
    reset();
    load_slot(0, 1, 0x5000u);
    const uint32_t out_before = top.err_write_outside_o;
    write_e(4, 0xDEAD);                   // no BEGIN
    zhao::tick(top);
    zhao::check(top.err_write_outside_o == out_before + 1,
                "a WRITE with no open load is refused and counted", 1,
                static_cast<int>(top.err_write_outside_o - out_before));
    top.lu_valid_i = 1; top.lu_slot_i = 0; top.lu_gen_i = 1; top.lu_idx_i = 4;
    zhao::tick(top);
    top.lu_valid_i = 0;
    zhao::tick(top);
    top.eval();
    zhao::check(top.lu_rgb565_o == static_cast<uint16_t>(0x5000u + 4),
                "and the resident palette still holds its own value", 1,
                top.lu_rgb565_o == static_cast<uint16_t>(0x5000u + 4) ? 1 : 0);
  }

  // A LOOKUP ON THE SAME CLOCK AS BEGIN reports nonresident. X6 is explicit
  // that this must not rely on read-during-write behaviour.
  {
    reset();
    load_slot(0, 1, 0x6000u);
    top.lu_valid_i = 1; top.lu_slot_i = 0; top.lu_gen_i = 1; top.lu_idx_i = 2;
    top.ld_valid_i = 1; top.ld_op_i = kBEGIN; top.ld_slot_i = 0; top.ld_gen_i = 2;
    top.ld_crc_ok_i = 1;
    zhao::tick(top);
    top.lu_valid_i = 0;
    top.ld_valid_i = 0;
    zhao::tick(top);
    top.eval();
    zhao::check(top.lu_resident_o == 0 || top.lu_stale_o == 1,
                "a lookup accepted on the SAME CLOCK as BEGIN reports "
                "nonresident or stale, never a colour from the slot being "
                "overwritten",
                1, (top.lu_resident_o == 0 || top.lu_stale_o == 1) ? 1 : 0);
  }

  return zhao::report_and_exit("texture_palette_res_directed");
}
