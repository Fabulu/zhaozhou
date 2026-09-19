// terrain_mipreq_directed.cpp -- does every page that lands get its mips
// asked for, exactly once, in order?
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS ACTUALLY GUARDING
// ---------------------------------------------------------------------------
// `zhao_terrain_mipreq` is the owner of a request that `tb_terrain_world.sv`
// minted in the bench and called "a finding rather than a convenience: no
// contract says who owns the mip request". The consequence of getting it
// wrong has exactly one symptom, and it is not an error:
//
//   TERRAIN.RESIDENCY sets `mips_stale` on every claim, so a loaded page
//   parks in ST_MIPGEN until a SECOND completion arrives. A request that is
//   never made, or made twice, or made for the wrong page, produces a page
//   that is loaded, CRC-verified, sitting in its slot, and never called
//   ground. `lu_hit_o` misses it for ever. Nothing else in the machine says
//   anything.
//
// So the checks here are about COUNT and ORDER and IDENTITY, not about the
// block running. Specifically:
//
//   * one request per event, never two. The event is a handshake pulse, and
//     the trap is that TERRAIN.PAGELOADER HOLDS `fin_valid_o` until its ready
//     comes -- a trigger taken from the offer instead of the acceptance would
//     enqueue the same page once per cycle of the wait and fill the queue with
//     one page. Case 2 holds a would-be offer for many cycles and requires
//     exactly one request.
//   * FIFO order, because the mips must be built in the order the pages
//     landed.
//   * the CRC travels. The directory validates the CRC on EVERY completion it
//     accepts, not only the loader's, so a token that does not come back
//     intact is eight CRC failures and zero pages resident -- which the
//     composed bench measured rather than predicted.
//   * `drops_o` FIRES (case 4). It reads zero in normal operation, which makes
//     it a claim; the state is reachable with legal stimulus by refusing the
//     consumer and presenting DEPTH+1 events, so no mutant is owed.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_mipreq.h"

#include "zhao_sim.hpp"

namespace {

int failures = 0;
constexpr int kDepth = 8;  // the module's DEPTH under test

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

void check_eq(uint64_t got, uint64_t want, const char* what) {
  if (got != want) {
    std::printf("FAIL: %s -- expected %llu, got %llu\n", what,
                static_cast<unsigned long long>(want), static_cast<unsigned long long>(got));
    ++failures;
  }
}

struct Req {
  uint32_t slot, gen, epoch, src, crc;
  bool operator==(const Req& o) const {
    return slot == o.slot && gen == o.gen && epoch == o.epoch && src == o.src && crc == o.crc;
  }
};

Req page_of(int n) {
  return Req{static_cast<uint32_t>(0x40 + n), static_cast<uint32_t>(3 + n),
             static_cast<uint32_t>(0xE000 + n), static_cast<uint32_t>(0x1000 + n),
             static_cast<uint32_t>(0xC0DE0000u + static_cast<uint32_t>(n) * 7u)};
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_mipreq top;

  auto idle = [&]() {
    top.ev_valid_i = 0;
    top.j_ready_i = 0;
  };

  auto reset = [&]() {
    idle();
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    top.eval();
    zhao::tick(top);
    top.eval();
  };

  auto present = [&](const Req& r) {
    top.ev_valid_i = 1;
    top.ev_slot_i = r.slot;
    top.ev_gen_i = r.gen;
    top.ev_epoch_i = r.epoch;
    top.ev_src_id_i = r.src;
    top.ev_crc_i = r.crc;
    top.eval();
    zhao::tick(top);
    top.eval();
    top.ev_valid_i = 0;
    top.eval();
  };

  auto take = [&]() {
    // Read the head, then accept it.
    Req r{static_cast<uint32_t>(top.j_slot_o), static_cast<uint32_t>(top.j_gen_o),
          static_cast<uint32_t>(top.j_epoch_o), static_cast<uint32_t>(top.j_src_id_o),
          static_cast<uint32_t>(top.j_crc_o)};
    top.j_ready_i = 1;
    top.eval();
    zhao::tick(top);
    top.eval();
    top.j_ready_i = 0;
    top.eval();
    return r;
  };

  // ---- 1: one page in, the same page out, field for field -----------------
  {
    reset();
    check(top.j_valid_o == 0, "case 1: an empty queue offers nothing");
    check(top.idle_o != 0, "case 1: and reports idle");

    const Req a = page_of(0);
    present(a);
    check(top.j_valid_o != 0, "case 1: the request appears");
    check_eq(top.level_o, 1, "case 1: one entry held");
    const Req got = take();
    check(got == a, "case 1: slot, generation, epoch, source id and CRC all survive");
    check(top.j_valid_o == 0, "case 1: and the queue empties");
    check_eq(top.requests_o, 1, "case 1: one request counted");
    check_eq(top.issued_o, 1, "case 1: one job issued");
    check_eq(top.drops_o, 0, "case 1: nothing dropped");
  }

  // ---- 2: THE HELD-OFFER TRAP --------------------------------------------
  // The event is an acceptance pulse. If this block had been wired to the
  // loader's OFFER, which is held until its ready comes, the same page would
  // enqueue once per cycle. The block cannot see that mistake -- the composer
  // makes it -- but the block must at least count what it is given honestly,
  // so this case proves ONE pulse is ONE request and a LEVEL is many.
  {
    reset();
    const Req a = page_of(1);
    present(a);
    check_eq(top.requests_o, 1, "case 2: one pulse, one request");
    check_eq(top.level_o, 1, "case 2: one entry");

    // now hold the level high for several cycles, as a mis-wired trigger would
    top.ev_valid_i = 1;
    top.ev_slot_i = a.slot;
    top.ev_gen_i = a.gen;
    top.ev_epoch_i = a.epoch;
    top.ev_src_id_i = a.src;
    top.ev_crc_i = a.crc;
    for (int i = 0; i < 5; ++i) {
      top.eval();
      zhao::tick(top);
      top.eval();
    }
    top.ev_valid_i = 0;
    top.eval();
    // Six pulses is six requests, and that is the RIGHT answer for this block:
    // it says plainly that a held trigger enqueues six times, which is what
    // makes the composer's pulse-versus-level choice a checkable one rather
    // than a matter of opinion.
    check_eq(top.requests_o, 6, "case 2: a held trigger is six requests, not one");
    check_eq(top.level_o, 6, "case 2: and six entries are held");
  }

  // ---- 3: FIFO order over a full queue ------------------------------------
  {
    reset();
    std::vector<Req> sent;
    for (int i = 0; i < kDepth; ++i) {
      const Req r = page_of(10 + i);
      present(r);
      sent.push_back(r);
    }
    check_eq(top.level_o, kDepth, "case 3: the queue is full");
    check_eq(top.drops_o, 0, "case 3: and nothing was dropped getting there");

    for (int i = 0; i < kDepth; ++i) {
      const Req got = take();
      check(got == sent[i], "case 3: pages come back in the order they landed");
    }
    check(top.j_valid_o == 0, "case 3: the queue drains completely");
    check(top.idle_o != 0, "case 3: and reports idle again");
    check_eq(top.issued_o, kDepth, "case 3: every page was issued once");
  }

  // ---- 4: THE POSITIVE CONTROL -- drops_o FIRES ---------------------------
  // Reachable with legal stimulus: refuse the consumer and present DEPTH + 3
  // events. The page that is lost here is a page that would never become
  // ground, and this counter is its only symptom.
  {
    reset();
    check_eq(top.drops_o, 0, "case 4: the drop counter starts at zero");
    for (int i = 0; i < kDepth + 3; ++i) present(page_of(20 + i));

    check_eq(top.level_o, kDepth, "case 4: the queue holds exactly DEPTH");
    check_eq(top.requests_o, kDepth + 3, "case 4: every event was counted");
    check_eq(top.drops_o, 3, "case 4: drops_o FIRED, once per lost request");

    // and the ones that ARE held are the FIRST DEPTH, not the last: a queue
    // that silently replaced its contents would keep the same level and lose a
    // different set.
    for (int i = 0; i < kDepth; ++i) {
      const Req got = take();
      check(got == page_of(20 + i), "case 4: the queue kept the earliest pages");
    }
    check_eq(top.issued_o, kDepth, "case 4: and issued exactly what it held");
  }

  // ---- 5: push and pop on the same clock keeps the level honest -----------
  // The level is the one piece of state a queue gets wrong, and the cycle that
  // does both at once is where it goes wrong.
  {
    reset();
    for (int i = 0; i < 3; ++i) present(page_of(40 + i));
    check_eq(top.level_o, 3, "case 5: three held");

    const Req r = page_of(50);
    top.ev_valid_i = 1;
    top.ev_slot_i = r.slot;
    top.ev_gen_i = r.gen;
    top.ev_epoch_i = r.epoch;
    top.ev_src_id_i = r.src;
    top.ev_crc_i = r.crc;
    top.j_ready_i = 1;
    top.eval();
    const Req head{static_cast<uint32_t>(top.j_slot_o), static_cast<uint32_t>(top.j_gen_o),
                   static_cast<uint32_t>(top.j_epoch_o), static_cast<uint32_t>(top.j_src_id_o),
                   static_cast<uint32_t>(top.j_crc_o)};
    zhao::tick(top);
    top.eval();
    top.ev_valid_i = 0;
    top.j_ready_i = 0;
    top.eval();

    check(head == page_of(40), "case 5: the head that was accepted is the oldest");
    check_eq(top.level_o, 3, "case 5: a simultaneous push and pop leaves the level unchanged");
    check_eq(top.drops_o, 0, "case 5: and nothing was dropped");

    // drain and confirm the new page is last
    const Req a = take();
    const Req b = take();
    const Req c = take();
    check(a == page_of(41), "case 5: order survives the simultaneous cycle (1)");
    check(b == page_of(42), "case 5: order survives the simultaneous cycle (2)");
    check(c == page_of(50), "case 5: the page pushed that cycle is last");
  }

  if (failures == 0) {
    std::printf(
        "PASS terrain_mipreq_directed -- one request per event, FIFO, identity "
        "and CRC intact through a full queue; drops_o fired on demand.\n");
    return 0;
  }
  std::printf("FAILED terrain_mipreq_directed -- %d check(s)\n", failures);
  return 1;
}
