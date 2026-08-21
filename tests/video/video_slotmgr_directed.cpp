// video_slotmgr_directed.cpp — VIDEO.SLOTMGR, RTL against
// `zref::video::SlotManager`.
//
// This block is the authority DEBUG.FRAMEBLIT's whole safety argument rested on
// and which did not exist: something has to decide that the slot being written
// speculatively is not the one on screen. Until now the shell granted a guard
// window at dispatch and cleared it at done, with no generation, no notion of
// DISPLAYED, and no way to refuse a stale event.
//
// FIVE LAWS, each one a place an implementation drifts:
//
//   1. THE GENERATION INCREMENTS ON EVERY ENTRY INTO WRITING -- not per lease
//      request, not per frame. Section 3 leases the same slot repeatedly and
//      checks the number moves every time; section 4 proves an event carrying
//      the previous generation is refused even though the slot and the state
//      look identical. That is the ABA case, and it is the whole reason the
//      field exists.
//   2. A STALE EVENT CHANGES NOTHING AND IS COUNTED. Section 5 checks both
//      halves: the state is untouched AND the counter moved. A silent refusal
//      turns a lease bug into a frame that mysteriously never appears.
//   3. ONLY A FREE SLOT IS LEASABLE. Section 2 attempts a lease from every one
//      of the four states.
//   4. THE DISPLAYED SLOT IS FREED AT THE SWAP, NOT AT THE PUBLICATION.
//      Section 6 publishes a replacement and checks the displayed slot is STILL
//      displayed until the swap actually happens -- freeing it when a
//      replacement merely became available hands a visible buffer to the next
//      blit.
//   5. ONE LEASE AT A TIME. Section 2b: a second grant is refused while one is
//      outstanding, because two slots WRITING with one writer is a bookkeeping
//      error, not a capability.
//
// Section 8 runs the whole thing against the reference under a random event
// stream, which is where an ordering difference between the two shows up.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "verilated.h"

#include "Vzhao_video_slotmgr.h"

#include "zhao_sim.hpp"
#include "zref/zref_slotmgr.hpp"

namespace {

using zhao::check;
namespace zv = zref::video;

constexpr uint8_t S_FREE = 0, S_WRITING = 1, S_READY = 2, S_DISPLAYED = 3;

/** Drive one cycle with every event input low, then tick. */
void idle(Vzhao_video_slotmgr& dut) {
  dut.lease_req_valid_i = 0;
  dut.publish_valid_i = 0;
  dut.release_valid_i = 0;
  dut.swap_valid_i = 0;
  dut.eval();
  zhao::tick(dut);
  dut.eval();
}

bool do_lease(Vzhao_video_slotmgr& dut, uint8_t slot, uint16_t* gen) {
  dut.lease_req_valid_i = 1;
  dut.lease_req_slot_i = slot;
  dut.eval();
  zhao::tick(dut);
  dut.lease_req_valid_i = 0;
  dut.eval();
  const bool granted = dut.lease_grant_o != 0;
  if (granted && gen) *gen = static_cast<uint16_t>(dut.fb_lease_generation_o);
  return granted;
}

void do_publish(Vzhao_video_slotmgr& dut, uint8_t slot, uint16_t gen) {
  dut.publish_valid_i = 1;
  dut.publish_slot_i = slot;
  dut.publish_generation_i = gen;
  dut.eval();
  zhao::tick(dut);
  dut.publish_valid_i = 0;
  dut.eval();
}

void do_release(Vzhao_video_slotmgr& dut, uint8_t slot, uint16_t gen) {
  dut.release_valid_i = 1;
  dut.release_slot_i = slot;
  dut.release_generation_i = gen;
  dut.eval();
  zhao::tick(dut);
  dut.release_valid_i = 0;
  dut.eval();
}

void do_swap(Vzhao_video_slotmgr& dut, uint8_t slot) {
  dut.swap_valid_i = 1;
  dut.swap_slot_i = slot;
  dut.eval();
  zhao::tick(dut);
  dut.swap_valid_i = 0;
  dut.eval();
}

uint8_t st(Vzhao_video_slotmgr& dut, int slot) {
  return static_cast<uint8_t>(slot == 0 ? dut.slot_state_o[0] : dut.slot_state_o[1]);
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

}  // namespace

int main(int argc, char** argv) {
  int random_iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && i + 1 < argc) {
      random_iters = std::atoi(argv[++i]);
    }
  }

  Vzhao_video_slotmgr dut;
  dut.rst_n = 0;
  dut.lease_req_valid_i = 0;
  dut.lease_req_slot_i = 0;
  dut.publish_valid_i = 0;
  dut.release_valid_i = 0;
  dut.swap_valid_i = 0;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // ---- 1. reset: both slots FREE, nothing displayed -----------------------
  {
    check(st(dut, 0) == S_FREE, "reset: slot 0 FREE", S_FREE, st(dut, 0));
    check(st(dut, 1) == S_FREE, "reset: slot 1 FREE", S_FREE, st(dut, 1));
    check(!dut.displayed_valid_o, "reset: nothing displayed", 0, dut.displayed_valid_o ? 1 : 0);
    check(dut.slot_ready_o == 0, "reset: neither slot READY", 0, dut.slot_ready_o);
    check(!dut.fb_lease_valid_o, "reset: no lease", 0, dut.fb_lease_valid_o ? 1 : 0);
  }

  // ---- 2. only a FREE slot is leasable: law 3 -----------------------------
  {
    uint16_t g = 0;
    check(do_lease(dut, 0, &g), "a FREE slot leases", 1, 1);
    check(st(dut, 0) == S_WRITING, "and becomes WRITING", S_WRITING, st(dut, 0));

    // 2b. law 5: one lease at a time.
    check(!do_lease(dut, 1, nullptr), "a second lease is refused while one is out", 0, 0);
    check(st(dut, 1) == S_FREE, "and the other slot is untouched", S_FREE, st(dut, 1));
    // The refusal is a registered ONE-CYCLE pulse: high on the edge that
    // refused, low the cycle after. Sampling it right after the request and
    // expecting zero tests nothing -- it tests when I looked.
    check(dut.lease_refused_o == 1, "a refusal pulses", 1, dut.lease_refused_o);
    idle(dut);
    check(dut.lease_refused_o == 0, "and it is exactly one cycle wide", 0, dut.lease_refused_o);

    do_publish(dut, 0, g);
    check(st(dut, 0) == S_READY, "publish makes it READY", S_READY, st(dut, 0));
    // A READY slot is not leasable.
    check(!do_lease(dut, 0, nullptr), "a READY slot is NOT leasable", 0, 0);
    check(st(dut, 0) == S_READY, "and stays READY", S_READY, st(dut, 0));

    do_swap(dut, 0);
    check(st(dut, 0) == S_DISPLAYED, "swap makes it DISPLAYED", S_DISPLAYED, st(dut, 0));
    // A DISPLAYED slot is not leasable -- the one that matters.
    check(!do_lease(dut, 0, nullptr), "a DISPLAYED slot is NOT leasable", 0, 0);
    check(st(dut, 0) == S_DISPLAYED, "and stays DISPLAYED", S_DISPLAYED, st(dut, 0));
  }

  // ---- 3. the generation moves on EVERY entry into WRITING: law 1 ---------
  {
    uint16_t seen[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 6; ++i) {
      uint16_t g = 0;
      const bool ok = do_lease(dut, 1, &g);
      char nm[64];
      std::snprintf(nm, sizeof nm, "3.lease %d granted", i);
      check(ok, nm, 1, ok ? 1 : 0);
      seen[i] = g;
      // Alternate how the lease ends; the generation must move either way,
      // because it counts ENTRIES into WRITING, not successes.
      if (i % 2 == 0) {
        do_release(dut, 1, g);
      } else {
        do_publish(dut, 1, g);
        do_swap(dut, 1);
        do_swap(dut, 0);  // put slot 1 back to FREE via the swap away
        // slot 0 is DISPLAYED and not READY, so that swap is refused; free
        // slot 1 the lawful way instead.
      }
      if (st(dut, 1) != S_FREE) {
        // It ended DISPLAYED; displace it by publishing and swapping slot 0.
        uint16_t g0 = 0;
        if (do_lease(dut, 0, &g0)) {
          do_publish(dut, 0, g0);
          do_swap(dut, 0);
        }
      }
    }
    for (int i = 1; i < 6; ++i) {
      char nm[64];
      std::snprintf(nm, sizeof nm, "3.generation %d moved", i);
      check(seen[i] != seen[i - 1], nm, 1, seen[i] != seen[i - 1] ? 1 : 0);
    }
    check(seen[5] == static_cast<uint16_t>(seen[0] + 5), "3.and moved by exactly one each time",
          static_cast<uint16_t>(seen[0] + 5), seen[5]);
  }

  dut.rst_n = 0;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  // ---- 4. THE ABA CASE: a stale generation is refused: law 1 --------------
  // The slot number is the same, the state is the same, and the event still has
  // to be refused. Nothing but the generation can tell these apart.
  {
    uint16_t g1 = 0;
    do_lease(dut, 0, &g1);
    do_release(dut, 0, g1);  // the first lease dies
    uint16_t g2 = 0;
    do_lease(dut, 0, &g2);  // re-granted, SAME slot, new generation
    check(g2 != g1, "4.the re-grant has a new generation", 1, g2 != g1 ? 1 : 0);
    check(st(dut, 0) == S_WRITING, "4.and the state looks identical", S_WRITING, st(dut, 0));

    const uint32_t stale_before = dut.stale_events_o;
    do_publish(dut, 0, g1);  // the DEAD lease publishes
    check(st(dut, 0) == S_WRITING, "4.a stale publication changes NOTHING", S_WRITING, st(dut, 0));
    check(dut.stale_events_o == stale_before + 1, "4.and is counted", stale_before + 1,
          dut.stale_events_o);

    do_release(dut, 0, g1);  // and so does a stale release
    check(st(dut, 0) == S_WRITING, "4.a stale release changes NOTHING either", S_WRITING,
          st(dut, 0));
    check(dut.stale_events_o == stale_before + 2, "4.counted too", stale_before + 2,
          dut.stale_events_o);

    // The LIVE lease still works.
    do_publish(dut, 0, g2);
    check(st(dut, 0) == S_READY, "4.the live generation is honoured", S_READY, st(dut, 0));
  }

  // ---- 5. every other stale shape: law 2 ----------------------------------
  {
    const uint32_t before = dut.stale_events_o;
    // Publishing a slot that is READY, not WRITING.
    do_publish(dut, 0, 1);
    check(st(dut, 0) == S_READY, "5.publishing a READY slot changes nothing", S_READY, st(dut, 0));
    // Releasing a FREE slot.
    do_release(dut, 1, 0);
    check(st(dut, 1) == S_FREE, "5.releasing a FREE slot changes nothing", S_FREE, st(dut, 1));
    // Swapping to a slot that is not READY.
    do_swap(dut, 1);
    check(st(dut, 1) == S_FREE, "5.swapping to a non-READY slot changes nothing", S_FREE,
          st(dut, 1));
    check(!dut.displayed_valid_o || dut.displayed_slot_o != 1, "5.and it does not become displayed",
          1, (!dut.displayed_valid_o || dut.displayed_slot_o != 1) ? 1 : 0);
    check(dut.stale_events_o == before + 3, "5.all three counted", before + 3, dut.stale_events_o);
  }

  // ---- 6. the displayed slot is freed AT THE SWAP: law 4 ------------------
  {
    dut.rst_n = 0;
    dut.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut);
    dut.rst_n = 1;
    dut.eval();

    uint16_t g0 = 0;
    do_lease(dut, 0, &g0);
    do_publish(dut, 0, g0);
    do_swap(dut, 0);
    check(st(dut, 0) == S_DISPLAYED, "6.slot 0 is on screen", S_DISPLAYED, st(dut, 0));

    // A replacement becomes available...
    uint16_t g1 = 0;
    check(do_lease(dut, 1, &g1), "6.slot 1 leases", 1, 1);
    do_publish(dut, 1, g1);
    check(st(dut, 1) == S_READY, "6.slot 1 is READY", S_READY, st(dut, 1));
    // ...and slot 0 is STILL on screen. Freeing it here would hand a visible
    // buffer to the next blit.
    check(st(dut, 0) == S_DISPLAYED, "6.slot 0 is STILL displayed", S_DISPLAYED, st(dut, 0));
    check(!do_lease(dut, 0, nullptr), "6.and still not leasable", 0, 0);

    do_swap(dut, 1);
    check(st(dut, 1) == S_DISPLAYED, "6.slot 1 takes the screen", S_DISPLAYED, st(dut, 1));
    check(st(dut, 0) == S_FREE, "6.and NOW slot 0 is free", S_FREE, st(dut, 0));
    check(do_lease(dut, 0, nullptr), "6.and leasable", 1, 1);
  }

  // ---- 7. slot_ready_o tracks READY exactly -------------------------------
  {
    dut.rst_n = 0;
    dut.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut);
    dut.rst_n = 1;
    dut.eval();
    check(dut.slot_ready_o == 0, "7.nothing ready at reset", 0, dut.slot_ready_o);

    // BOTH BITS, both slots. A first draft exercised slot 1 only, and a
    // mutation that broke slot 0's bit alone walked straight through the whole
    // directed set -- caught by the random lane, which is the wrong place for
    // something this simple to be caught.
    for (uint8_t slot = 0; slot < 2; ++slot) {
      const uint8_t bit = static_cast<uint8_t>(1u << slot);
      char nm[80];
      uint16_t g = 0;
      check(do_lease(dut, slot, &g), "7.lease", 1, 1);
      std::snprintf(nm, sizeof nm, "7.slot %u WRITING is not ready", slot);
      check(dut.slot_ready_o == 0, nm, 0, dut.slot_ready_o);
      do_publish(dut, slot, g);
      std::snprintf(nm, sizeof nm, "7.slot %u READY shows in bit %u only", slot, slot);
      check(dut.slot_ready_o == bit, nm, bit, dut.slot_ready_o);
      do_swap(dut, slot);
      std::snprintf(nm, sizeof nm, "7.slot %u DISPLAYED is not ready", slot);
      check(dut.slot_ready_o == 0, nm, 0, dut.slot_ready_o);
    }
  }

  // ---- 7b. EVENTS IN THE SAME CYCLE --------------------------------------
  // The swap arrives from the video domain and can land on the same edge as a
  // publication. The order is part of the law, not an accident of the code: the
  // SWAP is applied first, against the state as it was at the start of the
  // cycle, then the publication. So a swap can never consume a slot that only
  // became READY on that same edge, and a publication can never be lost to a
  // swap that freed the outgoing slot.
  {
    dut.rst_n = 0;
    dut.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut);
    dut.rst_n = 1;
    dut.eval();

    uint16_t g0 = 0;
    do_lease(dut, 0, &g0);
    do_publish(dut, 0, g0);
    do_swap(dut, 0);  // slot 0 displayed

    uint16_t g1 = 0;
    do_lease(dut, 1, &g1);

    // Publish slot 1 and swap to slot 1 on the SAME edge. The swap sees slot 1
    // as still WRITING, so it is refused and counted; the publication lands.
    const uint32_t stale_before = dut.stale_events_o;
    dut.publish_valid_i = 1;
    dut.publish_slot_i = 1;
    dut.publish_generation_i = g1;
    dut.swap_valid_i = 1;
    dut.swap_slot_i = 1;
    dut.eval();
    zhao::tick(dut);
    dut.publish_valid_i = 0;
    dut.swap_valid_i = 0;
    dut.eval();

    check(st(dut, 1) == S_READY, "7b.the publication landed", S_READY, st(dut, 1));
    check(st(dut, 0) == S_DISPLAYED, "7b.the swap did NOT consume it", S_DISPLAYED, st(dut, 0));
    check(dut.stale_events_o == stale_before + 1, "7b.and the swap was counted as stale",
          stale_before + 1, dut.stale_events_o);

    // The next swap succeeds, because slot 1 is READY by then.
    do_swap(dut, 1);
    check(st(dut, 1) == S_DISPLAYED, "7b.the following swap takes it", S_DISPLAYED, st(dut, 1));
    check(st(dut, 0) == S_FREE, "7b.and frees the outgoing slot", S_FREE, st(dut, 0));
  }

  // ---- 7c. PUBLISH AND RELEASE IN THE SAME CYCLE --------------------------
  // One transaction has one outcome, so this pair is not lawful. The formal
  // lane found it: with both asserted the two state writes raced in one cycle
  // and the later one silently won, so a slot could go FREE on the very edge it
  // was told to become READY.
  //
  // DEBUG.FRAMEBLIT proves it never emits both (its `a_excl`), but this block is
  // the authority on slot ownership and must not rest on a peer behaving. The
  // pair is refused and counted ONCE -- it is one bad event, not two.
  {
    dut.rst_n = 0;
    dut.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut);
    dut.rst_n = 1;
    dut.eval();

    uint16_t g = 0;
    check(do_lease(dut, 0, &g), "7c.leased", 1, 1);
    const uint32_t stale_before = dut.stale_events_o;

    dut.publish_valid_i = 1;
    dut.publish_slot_i = 0;
    dut.publish_generation_i = g;
    dut.release_valid_i = 1;
    dut.release_slot_i = 0;
    dut.release_generation_i = g;
    dut.eval();
    zhao::tick(dut);
    dut.publish_valid_i = 0;
    dut.release_valid_i = 0;
    dut.eval();

    check(st(dut, 0) == S_WRITING, "7c.the unlawful pair changes NOTHING", S_WRITING, st(dut, 0));
    check(dut.stale_events_o == stale_before + 1, "7c.and is counted exactly once",
          stale_before + 1, dut.stale_events_o);
    check(dut.fb_lease_valid_o == 1, "7c.the lease is still live", 1, dut.fb_lease_valid_o);

    // And the transaction can still end properly afterwards.
    do_publish(dut, 0, g);
    check(st(dut, 0) == S_READY, "7c.a lawful publication still works", S_READY, st(dut, 0));
  }

  // ---- 8. the whole machine against the reference -------------------------
  // One event per cycle, chosen at random, with generations drawn from a pool
  // that deliberately includes DEAD ones -- so most stale events here are real
  // ABA shapes rather than obviously-wrong numbers.
  if (random_iters > 0) {
    dut.rst_n = 0;
    dut.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(dut);
    dut.rst_n = 1;
    dut.eval();

    zv::SlotManager ref;
    ref.reset();

    Prng rng(0x5107u);
    uint16_t pool[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int pool_n = 0;
    int granted = 0, published = 0, released = 0, swapped = 0, stale_seen = 0;

    for (int i = 0; i < random_iters; ++i) {
      const uint8_t slot = static_cast<uint8_t>(rng.below(2));
      switch (rng.below(4)) {
        case 0: {
          uint8_t rs = 0;
          uint16_t rg = 0;
          const bool ref_ok = ref.request_lease(slot, &rs, &rg);
          uint16_t dg = 0;
          const bool dut_ok = do_lease(dut, slot, &dg);
          if (ref_ok) ++granted;
          if (ref_ok != dut_ok) {
            check(false, "8.lease grant agrees", ref_ok ? 1 : 0, dut_ok ? 1 : 0);
          } else if (ref_ok) {
            check(dg == rg, "8.granted generation agrees", rg, dg);
            if (pool_n < 8)
              pool[pool_n++] = rg;
            else
              pool[rng.below(8)] = rg;
          }
          break;
        }
        case 1: {
          // Half the time the LIVE generation, half the time one from the pool
          // -- which may be long dead.
          uint16_t g = ref.lease_generation();
          if (pool_n > 0 && rng.below(2)) g = pool[rng.below(static_cast<uint32_t>(pool_n))];
          const zv::SlotEvent v = ref.publish(slot, g);
          do_publish(dut, slot, g);
          if (v == zv::SlotEvent::kAccepted)
            ++published;
          else
            ++stale_seen;
          break;
        }
        case 2: {
          uint16_t g = ref.lease_generation();
          if (pool_n > 0 && rng.below(2)) g = pool[rng.below(static_cast<uint32_t>(pool_n))];
          const zv::SlotEvent v = ref.release(slot, g);
          do_release(dut, slot, g);
          if (v == zv::SlotEvent::kAccepted)
            ++released;
          else
            ++stale_seen;
          break;
        }
        default: {
          const zv::SlotEvent v = ref.swap(slot);
          do_swap(dut, slot);
          if (v == zv::SlotEvent::kAccepted)
            ++swapped;
          else
            ++stale_seen;
          break;
        }
      }

      // Everything the two can disagree about, every single step.
      char nm[64];
      std::snprintf(nm, sizeof nm, "8.step %d slot0 state", i);
      check(st(dut, 0) == static_cast<uint8_t>(ref.state(0)), nm,
            static_cast<uint8_t>(ref.state(0)), st(dut, 0));
      std::snprintf(nm, sizeof nm, "8.step %d slot1 state", i);
      check(st(dut, 1) == static_cast<uint8_t>(ref.state(1)), nm,
            static_cast<uint8_t>(ref.state(1)), st(dut, 1));
      const uint8_t want_ready =
          static_cast<uint8_t>((ref.ready(0) ? 1u : 0u) | (ref.ready(1) ? 2u : 0u));
      std::snprintf(nm, sizeof nm, "8.step %d slot_ready", i);
      check(dut.slot_ready_o == want_ready, nm, want_ready, dut.slot_ready_o);
      std::snprintf(nm, sizeof nm, "8.step %d lease live", i);
      check((dut.fb_lease_valid_o != 0) == ref.lease_active(), nm, ref.lease_active() ? 1 : 0,
            dut.fb_lease_valid_o ? 1 : 0);
      std::snprintf(nm, sizeof nm, "8.step %d stale count", i);
      check(dut.stale_events_o == ref.stale_events(), nm, ref.stale_events(), dut.stale_events_o);
      std::snprintf(nm, sizeof nm, "8.step %d leases granted", i);
      check(dut.leases_granted_o == ref.leases_granted(), nm, ref.leases_granted(),
            dut.leases_granted_o);
      std::snprintf(nm, sizeof nm, "8.step %d displayed", i);
      check((dut.displayed_valid_o != 0) == ref.displayed_valid() &&
                (!ref.displayed_valid() || dut.displayed_slot_o == ref.displayed()),
            nm, ref.displayed_valid() ? ref.displayed() + 1u : 0u,
            dut.displayed_valid_o ? dut.displayed_slot_o + 1u : 0u);
    }

    // Coverage, asserted rather than hoped for: a run in which nothing was ever
    // granted, published, swapped or refused would pass every check above while
    // exercising nothing.
    check(granted > 0, "8.leases were granted", 1, static_cast<uint32_t>(granted));
    check(published > 0, "8.publications happened", 1, static_cast<uint32_t>(published));
    check(released > 0, "8.releases happened", 1, static_cast<uint32_t>(released));
    check(swapped > 0, "8.swaps happened", 1, static_cast<uint32_t>(swapped));
    check(stale_seen > 0, "8.stale events happened", 1, static_cast<uint32_t>(stale_seen));
    std::printf(
        "random: %d steps, %d granted, %d published, %d released, %d swapped, "
        "%d refused\n",
        random_iters, granted, published, released, swapped, stale_seen);
  }

  return zhao::report_and_exit("video_slotmgr_directed");
}
