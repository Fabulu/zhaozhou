// hps_boundary_directed.cpp — the HPS/runtime boundary protocol.
// Authored 2026-09-05 (roadmap G2-C).
//
// The point of these tests is ownership. In the machine the two sides are
// separated by a bus, so "who may touch this slot right now" cannot be a
// convention -- it has to be derivable from state, and every illegal touch has
// to be refused rather than tolerated. A tolerated one rewrites a descriptor
// under a DMA that is still reading it, which is the kind of fault that appears
// once a week on hardware and never in simulation.

#include <cstdint>
#include <cstdio>

#include "zref/zref_hps_boundary.hpp"

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

namespace hps = zref::hps;

constexpr uint64_t kArenaBase = 0x3000'0000ull;
constexpr uint64_t kArenaBytes = 0x0010'0000ull;

hps::Arena arena() {
  hps::Arena a;
  a.base = kArenaBase;
  a.bytes = kArenaBytes;
  return a;
}

hps::Descriptor good(uint32_t tag = 1) {
  hps::Descriptor d;
  d.src = kArenaBase + 128;
  d.dst = 0x0100'0000u;
  d.length = 256;
  d.epoch = 1;
  d.tag = tag;
  return d;
}

void test_the_happy_path_moves_ownership_exactly_once_each_way() {
  hps::Ring r(4, arena());
  hps::Ledger L;

  const int s = r.submit(good(0xAA), 1, &L);
  check(s >= 0, "a legal descriptor is accepted", 1, s >= 0 ? 1 : 0);
  check(r.state(s) == hps::SlotState::kSubmitted,
        "and the DEVICE now owns the slot", 1,
        static_cast<long long>(r.state(s)));

  const int c = r.claim(&L);
  check(c == s, "the device claims that slot", s, c);
  check(r.state(s) == hps::SlotState::kClaimed, "which is now in flight", 2,
        static_cast<long long>(r.state(s)));

  check(r.complete(s, true, hps::Fault::kNone, &L), "the device completes it", 1, 1);
  check(r.state(s) == hps::SlotState::kComplete, "the ARM owns it again", 3,
        static_cast<long long>(r.state(s)));

  hps::Completion done;
  check(r.reap(s, &done, &L), "the ARM reaps it", 1, 1);
  check(done.tag == 0xAA, "and the tag returns UNTOUCHED", 0xAA, done.tag);
  check(done.ok, "reported ok", 1, done.ok ? 1 : 0);
  check(r.state(s) == hps::SlotState::kFree, "the slot is free again", 0,
        static_cast<long long>(r.state(s)));
  check(L.submitted == 1 && L.claimed == 1 && L.completed == 1 && L.reaped == 1,
        "each transition counted exactly once", 4,
        L.submitted + L.claimed + L.completed + L.reaped);
}

void test_a_userspace_pointer_is_not_a_dma_address() {
  hps::Ring r(4, arena());
  hps::Ledger L;

  hps::Descriptor d = good();
  d.src = kArenaBase + 1;  // inside the arena, but not DMA-aligned
  check(r.submit(d, 1, &L) < 0, "an unaligned SOURCE is refused", 1, 1);
  check(r.last_fault() == hps::Fault::kUnalignedPointer, "as unaligned", 3,
        static_cast<long long>(r.last_fault()));

  d = good();
  d.length = 100;  // not a multiple of the burst
  check(r.submit(d, 1, &L) < 0, "an unaligned LENGTH is refused", 1, 1);

  d = good();
  d.dst = 0x0100'0001u;
  check(r.submit(d, 1, &L) < 0, "an unaligned DESTINATION is refused", 1, 1);

  check(L.refused_unaligned == 3, "all three counted", 3, L.refused_unaligned);
  check(r.count(hps::SlotState::kFree) == 4,
        "and NO refusal consumed a ring slot -- a rejected descriptor that took "
        "an entry would turn a producer bug into a deadlock",
        4, r.count(hps::SlotState::kFree));
}

void test_the_source_must_be_inside_the_registered_arena() {
  hps::Ring r(4, arena());
  hps::Ledger L;

  hps::Descriptor d = good();
  d.src = kArenaBase - 64;  // just below
  check(r.submit(d, 1, &L) < 0, "a source below the arena is refused", 1, 1);

  d = good();
  d.src = kArenaBase + kArenaBytes - 64;
  d.length = 256;  // runs past the end
  check(r.submit(d, 1, &L) < 0, "a source RUNNING PAST the end is refused", 1, 1);
  check(r.last_fault() == hps::Fault::kOutsideArena, "as outside the arena", 4,
        static_cast<long long>(r.last_fault()));
  check(L.refused_outside_arena == 2, "counted", 2, L.refused_outside_arena);
}

void test_a_stale_epoch_is_refused() {
  hps::Ring r(4, arena());
  hps::Ledger L;
  hps::Descriptor d = good();
  d.epoch = 1;
  check(r.submit(d, /*current_epoch=*/2, &L) < 0,
        "a descriptor from a closed epoch is refused", 1, 1);
  check(r.last_fault() == hps::Fault::kEpochStale, "as stale", 5,
        static_cast<long long>(r.last_fault()));
  check(L.refused_stale_epoch == 1, "counted", 1, L.refused_stale_epoch);
}

// ---------------------------------------------------------------------------
// The ownership violations. These are the reason the protocol exists.
// ---------------------------------------------------------------------------
void test_neither_side_may_touch_a_slot_it_does_not_own() {
  hps::Ring r(4, arena());
  hps::Ledger L;
  const int s = r.submit(good(), 1, &L);

  // The ARM tries to reap a slot the device has not completed.
  hps::Completion c;
  check(!r.reap(s, &c, &L), "the ARM may NOT reap a submitted slot", 0, 0);
  check(r.state(s) == hps::SlotState::kSubmitted, "and the state is unchanged", 1,
        static_cast<long long>(r.state(s)));

  // The device tries to complete a slot it has not claimed.
  check(!r.complete(s, true, hps::Fault::kNone, &L),
        "the device may NOT complete an unclaimed slot", 0, 0);
  check(r.state(s) == hps::SlotState::kSubmitted, "state still unchanged", 1,
        static_cast<long long>(r.state(s)));

  // Claim, then the ARM tries to reap mid-flight -- the exact case that would
  // rewrite a descriptor under a live DMA.
  const int c2 = r.claim(&L);
  check(!r.reap(c2, &c, &L), "the ARM may NOT reap a slot in flight", 0, 0);

  check(L.refused_not_owned == 3, "all three violations counted", 3,
        L.refused_not_owned);
}

void test_a_full_ring_is_refused_not_overwritten() {
  hps::Ring r(2, arena());
  hps::Ledger L;
  check(r.submit(good(1), 1, &L) >= 0, "slot 1", 1, 1);
  check(r.submit(good(2), 1, &L) >= 0, "slot 2", 1, 1);
  check(r.submit(good(3), 1, &L) < 0, "a full ring REFUSES rather than wrapping",
        1, 1);
  check(r.last_fault() == hps::Fault::kRingFull, "as full", 2,
        static_cast<long long>(r.last_fault()));
  check(L.refused_ring_full == 1, "counted", 1, L.refused_ring_full);

  // The two in flight are untouched.
  check(r.descriptor(0).tag == 1 && r.descriptor(1).tag == 2,
        "and the descriptors already in flight are intact", 3,
        r.descriptor(0).tag + r.descriptor(1).tag);
}

void test_a_device_side_failure_returns_a_fault_not_a_lie() {
  hps::Ring r(4, arena());
  hps::Ledger L;
  const int s = r.submit(good(0x77), 1, &L);
  r.claim(&L);
  r.complete(s, false, hps::Fault::kOutsideArena, &L);

  hps::Completion c;
  r.reap(s, &c, &L);
  check(!c.ok, "a failed transfer reports NOT ok", 0, c.ok ? 1 : 0);
  check(c.fault == hps::Fault::kOutsideArena, "with the fault that caused it", 4,
        static_cast<long long>(c.fault));
  check(c.tag == 0x77, "and still returns the caller's tag, so it can be traced",
        0x77, c.tag);
}

}  // namespace

int main() {
  test_the_happy_path_moves_ownership_exactly_once_each_way();
  test_a_userspace_pointer_is_not_a_dma_address();
  test_the_source_must_be_inside_the_registered_arena();
  test_a_stale_epoch_is_refused();
  test_neither_side_may_touch_a_slot_it_does_not_own();
  test_a_full_ring_is_refused_not_overwritten();
  test_a_device_side_failure_returns_a_fault_not_a_lie();

  if (g_failed) {
    std::printf("[hps_boundary_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[hps_boundary_directed] %d checks passed\n", g_checks);
  return 0;
}
