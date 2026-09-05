// fjournal_directed.cpp — the F-sheet journal barrier.
// Authored 2026-09-05 (roadmap G2-D).
//
// The contract's own sentence is the acceptance criterion:
//
//   B and D are never written back on eviction ... F is not, and LOSING IT
//   LOSES GROUND THE PLAYER DESTROYED.
//
// So the test that matters is the full round trip: deform, evict, journal,
// acknowledge, reuse the slot for another page, come back -- and find the
// crater still there. Everything else here exists to make sure that round trip
// cannot be passed by accident.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zref/zref_fjournal.hpp"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

namespace ter = zref::terrain;

std::vector<int16_t> flat(std::size_t n) { return std::vector<int16_t>(n, 0); }

void load_resident(ter::Streamer& S, ter::Journal& J, int slot, uint32_t page, ter::Ledger* L) {
  S.begin_load(slot, page, L);
  S.loader_write(slot, flat(16), L);
  S.finish_load(slot, J, L);
}

void test_a_clean_slot_evicts_without_journalling() {
  ter::Streamer S(2);
  ter::Journal J;
  ter::Ledger L;
  load_resident(S, J, 0, 100, &L);

  S.begin_evict(0, &J, &L);
  check(S.slot(0).state == ter::SlotState::kFree,
        "a CLEAN slot frees immediately -- B and D are reproducible", 0,
        static_cast<long long>(S.slot(0).state));
  check(J.size() == 0, "and nothing was journalled", 0, static_cast<long long>(J.size()));
  check(L.journalled == 0, "no journal traffic for clean evictions", 0, L.journalled);
}

void test_a_dirty_slot_is_not_reusable_until_acknowledged() {
  ter::Streamer S(2);
  ter::Journal J;
  ter::Ledger L;
  load_resident(S, J, 0, 100, &L);
  S.deform(0, 3, -50);
  check(S.slot(0).dirty_f, "deformation marks F dirty", 1, S.slot(0).dirty_f ? 1 : 0);

  // THE BARRIER, first half: without journalling, LOADING is refused.
  const ter::Refusal r1 = S.begin_load(0, 200, &L);
  check(r1 == ter::Refusal::kDirtyFNotJournalled,
        "a dirty slot may NOT enter LOADING before the journal copy", 1,
        static_cast<long long>(r1));
  check(L.refused_dirty_f == 1, "counted", 1, L.refused_dirty_f);

  // Journal it. The slot is STILL not reusable -- the ACK has not arrived.
  S.begin_evict(0, &J, &L);
  check(S.slot(0).state == ter::SlotState::kEvicting, "journalling moves it to EVICTING, not FREE",
        3, static_cast<long long>(S.slot(0).state));
  const ter::Refusal r2 = S.begin_load(0, 200, &L);
  check(r2 == ter::Refusal::kAwaitingAck, "and LOADING is still refused while awaiting the ACK", 2,
        static_cast<long long>(r2));
  check(L.refused_awaiting_ack == 1, "counted", 1, L.refused_awaiting_ack);

  // Only now.
  S.ack(0, &L);
  check(S.slot(0).state == ter::SlotState::kFree, "the ACK frees the slot", 0,
        static_cast<long long>(S.slot(0).state));
  check(S.begin_load(0, 200, &L) == ter::Refusal::kNone, "and LOADING is finally permitted", 0, 0);
}

void test_the_loader_may_write_only_a_loading_slot() {
  ter::Streamer S(2);
  ter::Journal J;
  ter::Ledger L;
  load_resident(S, J, 0, 100, &L);

  // RESIDENT is not LOADING. Deny-by-default with state-aware permissions.
  const ter::Refusal r = S.loader_write(0, flat(16), &L);
  check(r == ter::Refusal::kWriteToNonLoadingSlot, "the loader may not write a RESIDENT slot", 3,
        static_cast<long long>(r));
  check(L.refused_write_non_loading == 1, "counted", 1, L.refused_write_non_loading);

  S.begin_evict(0, &J, &L);  // clean -> free
  check(S.loader_write(0, flat(16), &L) == ter::Refusal::kWriteToNonLoadingSlot, "nor a FREE one",
        3, 3);
  check(L.refused_write_non_loading == 2, "counted again", 2, L.refused_write_non_loading);
}

// ---------------------------------------------------------------------------
// The one the contract exists for.
// ---------------------------------------------------------------------------
void test_ground_the_player_destroyed_survives_eviction_and_return() {
  ter::Streamer S(1);  // ONE slot, so the page must genuinely leave
  ter::Journal J;
  ter::Ledger L;

  load_resident(S, J, 0, 100, &L);
  check(S.slot(0).f[3] == 0, "the ground starts flat", 0, S.slot(0).f[3]);

  // The player blows a crater.
  S.deform(0, 3, -50);
  S.deform(0, 4, -30);
  check(S.slot(0).f[3] == -50, "a crater is dug", -50, S.slot(0).f[3]);

  // Travel away: the page is evicted, journalled, acknowledged, and the slot
  // is reused for a DIFFERENT page. This is the step that destroys F if the
  // barrier is missing.
  S.begin_evict(0, &J, &L);
  S.ack(0, &L);
  load_resident(S, J, 0, 200, &L);
  check(S.slot(0).page_id == 200, "the slot now holds another page", 200, S.slot(0).page_id);
  check(S.slot(0).f[3] == 0, "whose ground is its own, and flat", 0, S.slot(0).f[3]);

  // Travel back.
  S.begin_evict(0, &J, &L);
  S.ack(0, &L);
  S.begin_load(0, 100, &L);
  S.loader_write(0, flat(16), &L);  // the loader brings a flat sheet...
  S.finish_load(0, J, &L);          // ...and the journal overwrites it with F

  check(S.slot(0).f[3] == -50, "THE CRATER IS STILL THERE after eviction and return", -50,
        S.slot(0).f[3]);
  check(S.slot(0).f[4] == -30, "and so is the rest of it", -30, S.slot(0).f[4]);
  check(L.f_reloaded >= 1, "F was reloaded from the journal, not regenerated", 1,
        L.f_reloaded >= 1 ? 1 : 0);
}

void test_a_page_never_deformed_needs_no_journal_entry() {
  ter::Streamer S(1);
  ter::Journal J;
  ter::Ledger L;
  load_resident(S, J, 0, 300, &L);
  S.begin_evict(0, &J, &L);
  S.ack(0, &L);
  load_resident(S, J, 0, 300, &L);

  check(J.size() == 0,
        "an undeformed page leaves no journal entry -- the journal holds only "
        "what cannot be reproduced",
        0, static_cast<long long>(J.size()));
  check(L.f_reloaded == 0, "and nothing was reloaded from it", 0, L.f_reloaded);
}

void test_two_pages_keep_separate_craters() {
  ter::Streamer S(1);
  ter::Journal J;
  ter::Ledger L;

  load_resident(S, J, 0, 10, &L);
  S.deform(0, 1, -11);
  S.begin_evict(0, &J, &L);
  S.ack(0, &L);

  load_resident(S, J, 0, 20, &L);
  S.deform(0, 1, -22);
  S.begin_evict(0, &J, &L);
  S.ack(0, &L);

  S.begin_load(0, 10, &L);
  S.loader_write(0, flat(16), &L);
  S.finish_load(0, J, &L);
  check(S.slot(0).f[1] == -11, "page 10 keeps ITS crater", -11, S.slot(0).f[1]);

  S.begin_evict(0, &J, &L);
  S.ack(0, &L);
  S.begin_load(0, 20, &L);
  S.loader_write(0, flat(16), &L);
  S.finish_load(0, J, &L);
  check(S.slot(0).f[1] == -22, "and page 20 keeps its own", -22, S.slot(0).f[1]);
  check(J.size() == 2, "two journal entries, one per page", 2, static_cast<long long>(J.size()));
}

}  // namespace

int main() {
  test_a_clean_slot_evicts_without_journalling();
  test_a_dirty_slot_is_not_reusable_until_acknowledged();
  test_the_loader_may_write_only_a_loading_slot();
  test_ground_the_player_destroyed_survives_eviction_and_return();
  test_a_page_never_deformed_needs_no_journal_entry();
  test_two_pages_keep_separate_craters();

  if (g_failed) {
    std::printf("[fjournal_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[fjournal_directed] %d checks passed\n", g_checks);
  return 0;
}
