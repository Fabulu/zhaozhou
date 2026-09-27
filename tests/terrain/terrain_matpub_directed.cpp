// terrain_matpub_directed.cpp -- TERRAIN.COMPOSED_MATERIAL's publisher.
//
// WHAT THIS TEST IS EVIDENCE FOR, stated before the code so it can be checked
// against what it actually asserts:
//
//   1. A committed plane REACHES MEMORY, byte for byte, at the address the
//      slot names. Not "64 bursts happened" -- the 4,096 bytes are read back
//      and differenced against the cell words that went in.
//   2. The cell values are an ASYMMETRIC function of the cell index, so a
//      swapped mat_a/mat_b, a rotated word, a transposed plane or an
//      off-by-one in the burst walk all show. A plane of {1,1,1} would pass
//      under every one of those faults.
//   3. The three publish arms and the one skip arm are each reached
//      SEPARATELY, on otherwise identical stimulus.
//   4. THE STRANGER ARM IS THE ONE THIS TEST EXISTS FOR. A publish gate keyed
//      on "did the plane change" alone is blind when a slot changes occupant
//      and the new occupant's plane happens to hash equal to the old one's --
//      the region then describes a stranger under this patch's address, with
//      every counter balancing. Case 4 constructs exactly that collision by
//      committing a BIT-IDENTICAL plane under a DIFFERENT patch id, and
//      requires a publication.
//   5. Every counter the block exports is SEEN TO FIRE. A counter asserted
//      zero and never observed to move is a claim, not a measurement, and this
//      console has shipped four never-executed assertions in one week.
//
// WHAT IT DOES NOT COVER, named rather than left to be discovered: arbitration,
// credits, refresh and bank conflicts are MEM.VRAM.ARBITER's and are measured
// where they live. The bench's guard models the HANDSHAKE only -- including the
// two-cycle verdict, which is the half of it that has cost this tree time.

#include "Vtb_terrain_matpub.h"

#include <cstdint>
#include <cstdio>
#include <vector>

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

void check_eq(uint64_t got, uint64_t want, const char* what) {
  ++checks;
  if (got != want) {
    ++failures;
    std::printf("FAIL: %s -- got %llu (0x%llx), want %llu (0x%llx)\n", what,
                (unsigned long long)got, (unsigned long long)got,
                (unsigned long long)want, (unsigned long long)want);
  }
}

constexpr int kCellsX = 32;
constexpr int kCellsY = 32;
constexpr int kCells = kCellsX * kCellsY;
constexpr uint32_t kRegionBase = 0x058B0000u;
constexpr uint32_t kSlotBytes = 8192u;

// THE ASYMMETRY IS THE POINT. Three different odd functions of the cell index,
// none of which equals another for any cell in the plane, so mat_a arriving
// where mat_b belongs is visible at every cell rather than at a lucky one.
uint8_t cell_a(int idx, int salt) { return (uint8_t)(0x11 + 3 * idx + salt); }
uint8_t cell_b(int idx, int salt) { return (uint8_t)(0xC0 - 5 * idx + salt); }
uint8_t cell_w(int idx, int salt) { return (uint8_t)(0x07 + 11 * idx + salt); }

uint32_t cell_word(int idx, int salt) {
  return ((uint32_t)cell_w(idx, salt) << 16) | ((uint32_t)cell_b(idx, salt) << 8) |
         (uint32_t)cell_a(idx, salt);
}

void idle(Vtb_terrain_matpub& t, int n) {
  t.c_we_i = 0;
  t.fill_start_i = 0;
  t.commit_i = 0;
  for (int i = 0; i < n; ++i) zhao::tick(t);
}

/** Run the publisher until it goes idle, or give up (a hang is not a pass). */
bool run_to_idle(Vtb_terrain_matpub& t, int max_cycles = 400000) {
  for (int i = 0; i < max_cycles; ++i) {
    if (!t.busy_o) return true;
    idle(t, 1);
  }
  return false;
}

/**
 * Drive one whole plane through the tap, then commit it.
 * `cells` defaults to the full plane; a smaller count is a SHORT FILL, which
 * is the stimulus `short_fill_o` needs.
 */
void fill_plane(Vtb_terrain_matpub& t, int salt, int cells = kCells) {
  t.fill_start_i = 1;
  t.c_we_i = 0;
  zhao::tick(t);
  t.fill_start_i = 0;

  for (int idx = 0; idx < cells; ++idx) {
    t.c_we_i = 1;
    t.c_ci_i = (uint8_t)(idx % kCellsX);
    t.c_cj_i = (uint8_t)(idx / kCellsX);
    t.c_mat_a_i = cell_a(idx, salt);
    t.c_mat_b_i = cell_b(idx, salt);
    t.c_weight_i = cell_w(idx, salt);
    zhao::tick(t);
  }
  t.c_we_i = 0;
}

void commit(Vtb_terrain_matpub& t, uint16_t pid, uint8_t slot) {
  t.commit_i = 1;
  t.patch_id_i = pid;
  t.slot_i = slot;
  zhao::tick(t);
  t.commit_i = 0;
}

/** Read one 64-bit window word. */
uint64_t peek(Vtb_terrain_matpub& t, uint32_t word) {
  t.probe_word_i = word;
  t.eval();
  return t.probe_data_o;
}

/**
 * Difference the published slot against the plane that was committed into it.
 * Returns the number of MISMATCHED CELLS, so the failure message can say how
 * wrong it is rather than only that it is wrong.
 */
int diff_slot(Vtb_terrain_matpub& t, uint8_t slot, int salt) {
  int bad = 0;
  const uint32_t base_word = (uint32_t)slot * (kSlotBytes / 8);
  for (int idx = 0; idx < kCells; ++idx) {
    const uint64_t w = peek(t, base_word + (uint32_t)(idx / 2));
    const uint32_t got = (idx % 2) ? (uint32_t)(w >> 32) : (uint32_t)(w & 0xffffffffu);
    if (got != cell_word(idx, salt)) ++bad;
  }
  return bad;
}

void reset_dut(Vtb_terrain_matpub& t) {
  t.clk = 0;
  t.rst_n = 0;
  t.cfg_enable_i = 1;
  t.cfg_vram_client_i = 6;  // ZHAO_CLIENT_TERRAIN_BUILD
  t.c_we_i = 0;
  t.c_ci_i = 0;
  t.c_cj_i = 0;
  t.c_mat_a_i = 0;
  t.c_mat_b_i = 0;
  t.c_weight_i = 0;
  t.fill_start_i = 0;
  t.commit_i = 0;
  t.patch_id_i = 0;
  t.slot_i = 0;
  t.g_deny_i = 0;
  t.probe_word_i = 0;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
  zhao::tick(t);
}

}  // namespace

int main() {
  Vtb_terrain_matpub top;

  // =======================================================================
  // CASE 1 -- A COMMITTED PLANE REACHES MEMORY, BYTE FOR BYTE
  // =======================================================================
  reset_dut(top);
  fill_plane(top, /*salt=*/0);
  check_eq(top.cells_captured_o, kCells, "case1: every cell was captured");
  commit(top, /*pid=*/0x1234, /*slot=*/1);
  check(run_to_idle(top), "case1: the publish completed rather than hanging");

  check_eq(top.commits_o, 1, "case1: one commit");
  check_eq(top.patches_published_o, 1, "case1: one patch published");
  check_eq(top.skipped_clean_o, 0, "case1: nothing was skipped");
  check_eq(top.short_fill_o, 0, "case1: the plane was complete");
  check_eq(top.guard_denied_o, 0, "case1: the guard granted every request");
  check_eq(top.bursts_written_o, 64, "case1: 4096 B is exactly 64 bursts of 64 B");
  check_eq(top.greqs_seen_o, 64, "case1: the BENCH saw 64 requests");
  check_eq(top.wbeats_seen_o, 512, "case1: the BENCH saw 64 x 8 beats");
  check_eq(top.oob_writes_o, 0, "case1: every beat landed inside the window");
  check_eq(top.last_req_len_o, 64, "case1: each request is a 64-byte write");

  // THE ADDRESS IS CHECKED AGAINST THE ARITHMETIC, not against itself. The
  // last burst of slot 1 sits at base + 1*8192 + 63*64.
  check_eq(top.last_req_addr_o, kRegionBase + 1u * kSlotBytes + 63u * 64u,
           "case1: the last burst addressed the right slot and offset");

  // AND THE PAYLOAD. This is the check the other nine are scaffolding for.
  check_eq((uint64_t)diff_slot(top, 1, 0), 0,
           "case1: all 1024 published cells match the plane that went in");

  // The neighbour slot must be untouched -- a publisher that wrote the whole
  // window would pass every count above.
  bool neighbour_clean = true;
  for (uint32_t w = 0; w < 512; ++w)
    if (peek(top, 2u * (kSlotBytes / 8) + w) != 0) neighbour_clean = false;
  check(neighbour_clean, "case1: slot 2 was not touched");

  // =======================================================================
  // CASE 2 -- THE SKIP ARM. Same occupant, same plane: nothing is written.
  // =======================================================================
  {
    const uint32_t bursts_before = top.bursts_written_o;
    fill_plane(top, /*salt=*/0);
    commit(top, /*pid=*/0x1234, /*slot=*/1);
    check(run_to_idle(top), "case2: the decision completed");
    check_eq(top.skipped_clean_o, 1, "case2: skipped_clean FIRED");
    check_eq(top.patches_published_o, 1, "case2: no second publication");
    check_eq(top.bursts_written_o, bursts_before, "case2: not one burst was spent");
  }

  // =======================================================================
  // CASE 3 -- THE SIGNATURE ARM. Same occupant, one cell different.
  // =======================================================================
  {
    fill_plane(top, /*salt=*/0);
    // One cell, re-written with a different value, after the plane is full.
    top.c_we_i = 1;
    top.c_ci_i = 5;
    top.c_cj_i = 9;
    top.c_mat_a_i = 0xAA;
    top.c_mat_b_i = 0xBB;
    top.c_weight_i = 0xCC;
    zhao::tick(top);
    top.c_we_i = 0;

    commit(top, /*pid=*/0x1234, /*slot=*/1);
    check(run_to_idle(top), "case3: the publish completed");
    check_eq(top.patches_published_o, 2, "case3: a changed plane WAS published");
    check_eq(top.skipped_clean_o, 1, "case3: and it was not skipped");

    const int idx = 9 * kCellsX + 5;
    const uint64_t w = peek(top, 1u * (kSlotBytes / 8) + (uint32_t)(idx / 2));
    const uint32_t got = (idx % 2) ? (uint32_t)(w >> 32) : (uint32_t)(w & 0xffffffffu);
    check_eq(got, 0x00CCBBAAu, "case3: the changed cell reached memory");
  }

  // =======================================================================
  // CASE 4 -- THE STRANGER ARM, AND IT IS THE REASON THIS BLOCK HAS A GATE
  //           WITH THREE ARMS RATHER THAN ONE.
  //
  // A BIT-IDENTICAL plane is committed into the SAME slot under a DIFFERENT
  // patch id. The signature therefore agrees exactly, so a change-only gate
  // would SKIP -- leaving the region describing the previous occupant under
  // the new occupant's address, silently, with every counter balanced.
  // =======================================================================
  {
    reset_dut(top);
    fill_plane(top, /*salt=*/7);
    commit(top, /*pid=*/0x0A0A, /*slot=*/3);
    check(run_to_idle(top), "case4: the first occupant published");
    check_eq(top.patches_published_o, 1, "case4: occupant A published");

    // The identical plane, a different patch. Same bytes, same signature.
    fill_plane(top, /*salt=*/7);
    commit(top, /*pid=*/0x0B0B, /*slot=*/3);
    check(run_to_idle(top), "case4: the second occupant's decision completed");

    check_eq(top.stranger_pub_o, 1, "case4: stranger_pub FIRED on the occupant change");
    check_eq(top.patches_published_o, 2,
             "case4: an occupant change republishes even when the plane is identical");
    check_eq(top.skipped_clean_o, 0,
             "case4: a change-only gate would have SKIPPED here -- this is the fault");
    check_eq((uint64_t)diff_slot(top, 3, 7), 0, "case4: the slot holds the right plane");
  }

  // =======================================================================
  // CASE 5 -- THE GUARD'S REFUSAL. `guard_denied_o` is fired deliberately,
  //           because a detector reading zero is a claim.
  // =======================================================================
  {
    reset_dut(top);
    top.g_deny_i = 1;
    fill_plane(top, /*salt=*/2);
    commit(top, /*pid=*/0x2222, /*slot=*/0);
    check(run_to_idle(top), "case5: the denial returned the block to idle");
    check_eq(top.guard_denied_o, 1, "case5: guard_denied FIRED");
    check_eq(top.patches_published_o, 0, "case5: a denied patch is not published");
    check_eq(top.bursts_written_o, 0, "case5: a denied request writes no beats");
    top.g_deny_i = 0;
  }

  // =======================================================================
  // CASE 6 -- A SHORT FILL IS COUNTED. A plane that owes 1,024 cells and
  //           commits with 100 is a fault, not a small patch.
  // =======================================================================
  {
    reset_dut(top);
    fill_plane(top, /*salt=*/3, /*cells=*/100);
    check_eq(top.cells_captured_o, 100, "case6: only the offered cells were captured");
    commit(top, /*pid=*/0x3333, /*slot=*/0);
    check(run_to_idle(top), "case6: the commit completed");
    check_eq(top.short_fill_o, 1, "case6: short_fill FIRED");
  }

  // =======================================================================
  // CASE 7 -- A COMMIT DURING A PUBLISH IS DROPPED AND COUNTED, never
  //           silently absorbed.
  // =======================================================================
  {
    reset_dut(top);
    fill_plane(top, /*salt=*/4);
    commit(top, /*pid=*/0x4444, /*slot=*/0);
    // Do not run to idle: the publish takes ~1,700 cycles, so a commit a few
    // cycles in lands squarely inside it.
    idle(top, 20);
    check(top.busy_o != 0, "case7: the publisher is still busy (the premise)");
    commit(top, /*pid=*/0x5555, /*slot=*/1);
    check(run_to_idle(top), "case7: the first publish still completed");
    check_eq(top.commit_busy_o, 1, "case7: commit_busy FIRED");
    check_eq(top.patches_published_o, 1, "case7: the second commit was dropped, not queued");
  }

  // =======================================================================
  // CASE 8 -- DISARMED, THE BLOCK ISSUES NOTHING. A guard window opens with
  //           its block and never ahead of it; a console that has not armed
  //           the publisher must see no request at all.
  // =======================================================================
  {
    reset_dut(top);
    top.cfg_enable_i = 0;
    fill_plane(top, /*salt=*/5);
    commit(top, /*pid=*/0x6666, /*slot=*/0);
    idle(top, 200);
    check_eq(top.greqs_seen_o, 0, "case8: disarmed, the BENCH saw no guard request");
    check_eq(top.patches_published_o, 0, "case8: disarmed, nothing was published");
    check_eq(top.commits_o, 1, "case8: the commit was still COUNTED");
    // AND THE CONTROL'S OTHER HALF: re-arming publishes. Without this, case 8
    // passes for a block that is broken rather than disarmed.
    top.cfg_enable_i = 1;
    fill_plane(top, /*salt=*/5);
    commit(top, /*pid=*/0x6666, /*slot=*/0);
    check(run_to_idle(top), "case8: the re-armed publish completed");
    check_eq(top.patches_published_o, 1, "case8: re-armed, it publishes");
  }

  std::printf("terrain_matpub_directed: %d checks, %d failures\n", checks, failures);
  zhao::exit_hard(failures == 0 ? 0 : 1);
}
