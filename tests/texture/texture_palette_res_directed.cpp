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

  // Load one whole slot with a known pattern at a given generation.
  auto load_slot = [&](int slot, int gen, uint32_t seed) {
    for (int e = 0; e < 256; ++e) {
      top.ld_valid_i = 1;
      top.ld_slot_i = slot;
      top.ld_gen_i = gen;
      top.ld_idx_i = e;
      top.ld_rgb565_i = static_cast<uint16_t>(seed + e);
      top.ld_last_i = (e == 255);
      zhao::tick(top);
    }
    top.ld_valid_i = 0;
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
        top.ld_valid_i = 1;
        top.ld_slot_i = 0;
        top.ld_gen_i = 2;
        top.ld_idx_i = c - 10;
        top.ld_rgb565_i = static_cast<uint16_t>(0x9000 + (c - 10));
        top.ld_last_i = (c == 19);
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
    for (int e = 0; e < 8; ++e) {
      top.ld_valid_i = 1;
      top.ld_slot_i = 0;
      top.ld_gen_i = 2;
      top.ld_idx_i = e;
      top.ld_rgb565_i = static_cast<uint16_t>(0x4000 + e);
      top.ld_last_i = 0;
      zhao::tick(top);
    }
    top.ld_valid_i = 0;
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

  return zhao::report_and_exit("texture_palette_res_directed");
}
