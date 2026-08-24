// surface_sheet_directed.cpp — directed suite for SURFACE.SHEET.
//
// Everything this block promises is a residency or a persistence promise, so
// the cases are shaped around those: what a fresh sheet reads, what survives a
// re-acquire, what does NOT survive a release, and what happens when there is
// no room. The ledger's one hard sentence — "overflow rejects the stamp, never
// partial-writes" — is checked from both ends: the acquire is refused, AND a
// write naming a non-resident handle is dropped rather than landing in
// somebody else's slot.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_surface_sheet.h"

#include "surface_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_surface.hpp"

using sdev::SheetResponse;
using zhao::check;
namespace zs = zref::surface;

namespace {

constexpr uint32_t kHandleA = 0x0000'2C01;  // patch 44, generation 1
constexpr uint32_t kHandleB = 0x0000'2D01;
constexpr uint32_t kHandleC = 0x0000'2E01;

void test_reset_state(Vzhao_surface_sheet& dut) {
  dut.eval();
  check(dut.res_occupancy_o == 0, "reset: no sheet is resident", 0, dut.res_occupancy_o);
  check(dut.res_busy_o == 0, "reset: no clear sweep is running", 0, dut.res_busy_o);
  check(dut.idle_o == 1, "reset: idle", 1, dut.idle_o);
  check(dut.surface_texels_touched_o == 0, "reset: the counter is zero", 0,
        dut.surface_texels_touched_o);
  check(dut.pg_valid_o == 0, "reset: no response is pending", 0, dut.pg_valid_o);
}

void test_acquire_allocates_and_clears(Vzhao_surface_sheet& dut) {
  const SheetResponse r = sdev::sheet_request(dut, sdev::kOpAcquire, kHandleA, 0, 0x1234);
  check(r.got, "the first ACQUIRE answers", 1, r.got ? 1 : 0);
  check(r.status == sdev::kStAllocated, "a fresh handle ALLOCATES", sdev::kStAllocated, r.status);
  check(r.op == sdev::kOpAcquire, "the response echoes the opcode", sdev::kOpAcquire, r.op);
  check(r.src_id == 0x1234, "source_id rides its own response", 0x1234, r.src_id);
  dut.eval();
  check(dut.res_occupancy_o == 1, "slot 0 is now occupied", 1, dut.res_occupancy_o);
  check(dut.res_busy_o == 0, "the clear sweep finished before the answer", 0, dut.res_busy_o);

  // A freshly allocated sheet reads ZERO EVERYWHERE — all 4,096 texels, not a
  // sample. The RAM is deliberately not reset, so this is the sweep's proof.
  uint32_t nonzero = 0;
  for (int t = 0; t < zs::kSheetTexels; ++t) {
    const SheetResponse q =
        sdev::sheet_request(dut, sdev::kOpRead, kHandleA, static_cast<uint16_t>(t), 0);
    if (!q.got || q.status != sdev::kStHit || q.tag != 0 || q.strength != 0) ++nonzero;
  }
  check(nonzero == 0, "all 4,096 texels of a fresh sheet read zero", 0, nonzero);
}

void test_write_readback_and_byte_enables(Vzhao_surface_sheet& dut) {
  check(sdev::sheet_write(dut, kHandleA, 100, 0xAB, 0xCD, true, true, 7), "the write port accepts",
        1, 1);
  SheetResponse r = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 100, 0);
  check(r.tag == 0xAB && r.strength == 0xCD, "write then read returns both bytes", 0xABCD,
        (static_cast<uint32_t>(r.tag) << 8) | r.strength);

  // strength only
  sdev::sheet_write(dut, kHandleA, 100, 0x00, 0x11, false, true, 7);
  r = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 100, 0);
  check(r.tag == 0xAB && r.strength == 0x11, "we_strength alone leaves the tag alone", 0xAB11,
        (static_cast<uint32_t>(r.tag) << 8) | r.strength);

  // tag only
  sdev::sheet_write(dut, kHandleA, 100, 0x22, 0x00, true, false, 7);
  r = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 100, 0);
  check(r.tag == 0x22 && r.strength == 0x11, "we_tag alone leaves the strength alone", 0x2211,
        (static_cast<uint32_t>(r.tag) << 8) | r.strength);

  // neighbours are untouched — a one-off addressing error shows up here
  r = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 99, 0);
  check(r.tag == 0 && r.strength == 0, "texel 99 untouched", 0, r.tag + r.strength);
  r = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 101, 0);
  check(r.tag == 0 && r.strength == 0, "texel 101 untouched", 0, r.tag + r.strength);

  dut.eval();
  check(dut.surface_texels_touched_o == 3, "surface_texels_touched counted three writes", 3,
        dut.surface_texels_touched_o);
}

void test_reacquire_persists(Vzhao_surface_sheet& dut) {
  const SheetResponse r = sdev::sheet_request(dut, sdev::kOpAcquire, kHandleA, 0, 0);
  check(r.status == sdev::kStHit, "re-acquiring a resident handle HITS", sdev::kStHit, r.status);
  const SheetResponse q = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 100, 0);
  check(q.tag == 0x22 && q.strength == 0x11,
        "a re-acquire does NOT clear — this line is persistence", 0x2211,
        (static_cast<uint32_t>(q.tag) << 8) | q.strength);
}

void test_second_slot_and_isolation(Vzhao_surface_sheet& dut) {
  const SheetResponse r = sdev::sheet_request(dut, sdev::kOpAcquire, kHandleB, 0, 0);
  check(r.status == sdev::kStAllocated, "a second handle takes the free slot", sdev::kStAllocated,
        r.status);
  dut.eval();
  check(dut.res_occupancy_o == 3, "both slots are occupied", 3, dut.res_occupancy_o);

  // Writing B must not touch A. This is the slot-addressing seam: an address
  // that dropped the slot bits would corrupt A here and nowhere else.
  sdev::sheet_write(dut, kHandleB, 100, 0x77, 0x88, true, true, 0);
  const SheetResponse a = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 100, 0);
  const SheetResponse b = sdev::sheet_request(dut, sdev::kOpRead, kHandleB, 100, 0);
  check(a.tag == 0x22 && a.strength == 0x11, "sheet A is unchanged by a write to B", 0x2211,
        (static_cast<uint32_t>(a.tag) << 8) | a.strength);
  check(b.tag == 0x77 && b.strength == 0x88, "sheet B holds its own write", 0x7788,
        (static_cast<uint32_t>(b.tag) << 8) | b.strength);
  const SheetResponse b0 = sdev::sheet_request(dut, sdev::kOpRead, kHandleB, 0, 0);
  check(b0.tag == 0 && b0.strength == 0, "sheet B was cleared on allocation", 0,
        b0.tag + b0.strength);
}

void test_overflow_rejects_and_never_evicts(Vzhao_surface_sheet& dut) {
  const uint32_t occ_before = dut.res_occupancy_o;
  const SheetResponse r = sdev::sheet_request(dut, sdev::kOpAcquire, kHandleC, 0, 0x55);
  check(r.status == sdev::kStOverflow, "a third handle OVERFLOWS", sdev::kStOverflow, r.status);
  dut.eval();
  check(dut.res_occupancy_o == occ_before, "overflow evicts NOTHING", occ_before,
        dut.res_occupancy_o);
  // The two incumbents still answer, and still hold their contents.
  const SheetResponse a = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 100, 0);
  const SheetResponse b = sdev::sheet_request(dut, sdev::kOpRead, kHandleB, 100, 0);
  check(a.status == sdev::kStHit && a.tag == 0x22, "A survived the overflow", 0x22, a.tag);
  check(b.status == sdev::kStHit && b.tag == 0x77, "B survived the overflow", 0x77, b.tag);

  // and the rejected handle is not resident
  const SheetResponse c = sdev::sheet_request(dut, sdev::kOpRead, kHandleC, 0, 0);
  check(c.status == sdev::kStMiss, "the rejected handle is not resident", sdev::kStMiss, c.status);
  check(c.tag == 0 && c.strength == 0, "a missed read answers zero, never another patch's data", 0,
        c.tag + c.strength);
}

void test_non_resident_write_is_dropped(Vzhao_surface_sheet& dut) {
  const uint32_t before = dut.surface_texels_touched_o;
  // Offer the write and watch the miss pulse in the accept cycle.
  dut.wr_valid_i = 1;
  dut.wr_handle_i = kHandleC;
  dut.wr_texel_i = 100;
  dut.wr_tag_i = 0xFF;
  dut.wr_strength_i = 0xFF;
  dut.wr_we_tag_i = 1;
  dut.wr_we_strength_i = 1;
  dut.wr_src_id_i = 0x0BAD;
  dut.eval();
  check(dut.wr_ready_o == 1, "the write port is ready", 1, dut.wr_ready_o);
  zhao::tick(dut);
  dut.wr_valid_i = 0;
  dut.eval();
  check(dut.wr_miss_o == 1, "a write for a non-resident handle raises wr_miss", 1, dut.wr_miss_o);
  check(dut.wr_miss_src_id_o == 0x0BAD, "the miss carries its source_id", 0x0BAD,
        dut.wr_miss_src_id_o);
  check(dut.surface_texels_touched_o == before, "a dropped write does not count a texel", before,
        dut.surface_texels_touched_o);
  // and nothing landed in either resident sheet
  const SheetResponse a = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 100, 0);
  const SheetResponse b = sdev::sheet_request(dut, sdev::kOpRead, kHandleB, 100, 0);
  check(a.tag == 0x22 && b.tag == 0x77, "the dropped write landed nowhere", 1,
        (a.tag == 0x22 && b.tag == 0x77) ? 1 : 0);
}

void test_release_ends_persistence(Vzhao_surface_sheet& dut) {
  const SheetResponse r = sdev::sheet_request(dut, sdev::kOpRelease, kHandleA, 0, 0);
  check(r.status == sdev::kStHit, "RELEASE of a resident handle succeeds", sdev::kStHit, r.status);
  dut.eval();
  check(dut.res_occupancy_o == 2, "slot 0 is free again", 2, dut.res_occupancy_o);

  const SheetResponse miss = sdev::sheet_request(dut, sdev::kOpRelease, kHandleA, 0, 0);
  check(miss.status == sdev::kStMiss, "releasing twice MISSES", sdev::kStMiss, miss.status);

  // The freed slot is available again, and re-acquiring A now CLEARS: release
  // is where persistence ends.
  const SheetResponse a = sdev::sheet_request(dut, sdev::kOpAcquire, kHandleA, 0, 0);
  check(a.status == sdev::kStAllocated, "re-acquiring after release ALLOCATES", sdev::kStAllocated,
        a.status);
  const SheetResponse q = sdev::sheet_request(dut, sdev::kOpRead, kHandleA, 100, 0);
  check(q.tag == 0 && q.strength == 0, "the re-allocated sheet was cleared", 0, q.tag + q.strength);
  // B, which was never released, kept everything.
  const SheetResponse b = sdev::sheet_request(dut, sdev::kOpRead, kHandleB, 100, 0);
  check(b.tag == 0x77 && b.strength == 0x88, "B's contents survived A's whole cycle", 0x7788,
        (static_cast<uint32_t>(b.tag) << 8) | b.strength);
}

void test_clear_sweep_is_visible(Vzhao_surface_sheet& dut) {
  // The sweep is 4,096 cycles by law (C3) and must show as res_busy_o with
  // req_ready_o low — that is the `latency: variable` the ledger records.
  sdev::sheet_request(dut, sdev::kOpRelease, kHandleB, 0, 0);
  dut.req_valid_i = 1;
  dut.req_op_i = sdev::kOpAcquire;
  dut.req_handle_i = kHandleC;
  dut.req_texel_i = 0;
  dut.req_src_id_i = 0;
  dut.pg_ready_i = 1;
  dut.eval();
  zhao::tick(dut);
  dut.req_valid_i = 0;
  // AND THE WRITE PORT IS REFUSED FOR THE WHOLE SWEEP TOO.
  //
  // Added RUN-20260824-0317. `wr_ready_o = !clr_active` is in the contract's
  // backpressure rules and NOTHING offered a write during a sweep, so mutant
  // S17 -- `wr_ready_o` tied high -- SURVIVED all six consumer lanes. It is not
  // an equivalent: an accepted write during a sweep has its address overridden
  // by the sweep's, so it lands nowhere, but it still counts a texel in
  // `surface_texels_touched_o`. The counter then disagrees with what the sheet
  // holds, which is exactly what the counter's own comment says must never
  // happen ("a dropped write does NOT count -- the counter has to agree with
  // what the sheet holds or it is not observability"). The shape differential
  // separates the two behaviours by 663,970 port-cycles.
  const uint32_t touched_before = dut.surface_texels_touched_o;
  int busy_cycles = 0;
  int refused = 0;
  int wr_refused = 0;
  dut.wr_handle_i = kHandleC;  // the handle being allocated: resident, so a
  dut.wr_texel_i = 123;        // fired write WOULD hit if the port let it
  dut.wr_tag_i = 0xAB;
  dut.wr_strength_i = 0xCD;
  dut.wr_we_tag_i = 1;
  dut.wr_we_strength_i = 1;
  dut.wr_src_id_i = 0x0C5;
  for (int c = 0; c < 6000; ++c) {
    dut.eval();
    if (dut.res_busy_o) ++busy_cycles;
    if (dut.res_busy_o && !dut.req_ready_o) ++refused;
    if (dut.res_busy_o && !dut.wr_ready_o) ++wr_refused;
    if (dut.pg_valid_o) break;
    // Offer the write ONLY while the sweep is running, so that a correct block
    // simply never accepts it and the test does not depend on when it stops.
    dut.wr_valid_i = dut.res_busy_o ? 1 : 0;
    dut.eval();
    zhao::tick(dut);
  }
  dut.wr_valid_i = 0;
  dut.eval();
  check(busy_cycles == 4096, "the clear sweep is exactly 4,096 cycles", 4096,
        static_cast<uint64_t>(busy_cycles));
  check(refused == busy_cycles, "requests are refused for the whole sweep", busy_cycles,
        static_cast<uint64_t>(refused));
  check(wr_refused == busy_cycles, "WRITES are refused for the whole sweep too", busy_cycles,
        static_cast<uint64_t>(wr_refused));
  check(dut.surface_texels_touched_o == touched_before,
        "a write offered during the sweep counts no texel", touched_before,
        dut.surface_texels_touched_o);
  check(dut.pg_status_o == sdev::kStAllocated, "the sweep ends with the ALLOCATED answer",
        sdev::kStAllocated, dut.pg_status_o);
  zhao::tick(dut);
  dut.eval();
  const SheetResponse swept = sdev::sheet_request(dut, sdev::kOpRead, kHandleC, 123, 0);
  check(swept.tag == 0 && swept.strength == 0, "and it did not land: the swept texel is still zero",
        0, (static_cast<uint32_t>(swept.tag) << 8) | swept.strength);
}

void test_read_throughput_and_backpressure(Vzhao_surface_sheet& dut) {
  // Fill 256 texels, then read them back-to-back with no stalls: the ledger's
  // "1 sheet texel per clock".
  for (int t = 0; t < 256; ++t)
    sdev::sheet_write(dut, kHandleC, static_cast<uint16_t>(t), static_cast<uint8_t>(t),
                      static_cast<uint8_t>(255 - t), true, true, 0);

  dut.pg_ready_i = 1;
  dut.req_valid_i = 1;
  dut.req_op_i = sdev::kOpRead;
  dut.req_handle_i = kHandleC;
  dut.req_src_id_i = 0;
  int issued = 0, received = 0, cycles = 0, bad = 0;
  while (received < 256 && cycles < 4000) {
    dut.req_texel_i = static_cast<uint16_t>(issued);
    dut.req_valid_i = issued < 256 ? 1 : 0;
    dut.eval();
    const bool req_fire = dut.req_valid_i && dut.req_ready_o;
    const bool pg_fire = dut.pg_valid_o && dut.pg_ready_i;
    uint8_t tag = 0, str = 0;
    if (pg_fire) {
      tag = static_cast<uint8_t>(dut.pg_tag_o);
      str = static_cast<uint8_t>(dut.pg_strength_o);
    }
    zhao::tick(dut);
    if (req_fire) ++issued;
    if (pg_fire) {
      if (tag != static_cast<uint8_t>(received) || str != static_cast<uint8_t>(255 - received))
        ++bad;
      ++received;
    }
    ++cycles;
  }
  dut.req_valid_i = 0;
  dut.eval();
  check(received == 256, "all 256 reads came back", 256, static_cast<uint64_t>(received));
  check(bad == 0, "pipelined reads return the right texel each time", 0,
        static_cast<uint64_t>(bad));
  std::printf("surface_sheet_directed: 256 pipelined reads in %d cycles\n", cycles);
  check(cycles <= 260, "1 sheet texel per clock (ledger target)", 260,
        static_cast<uint64_t>(cycles));

  // Now stall the response channel: the block must hold the word, not lose it.
  const SheetResponse warm = sdev::sheet_request(dut, sdev::kOpRead, kHandleC, 7, 0);
  check(warm.strength == static_cast<uint8_t>(255 - 7), "warm read is right", 248, warm.strength);

  dut.pg_ready_i = 0;
  dut.req_valid_i = 1;
  dut.req_texel_i = 9;
  dut.eval();
  bool sent = false;
  for (int c = 0; c < 8 && !sent; ++c) {
    if (dut.req_ready_o) sent = true;
    zhao::tick(dut);
    dut.req_valid_i = 0;
    dut.eval();
  }
  check(sent, "the read was accepted", 1, sent ? 1 : 0);
  // Hold pg_ready low for a while; the answer must still be there afterwards.
  //
  // AND WIGGLE THE READ ADDRESS WHILE IT IS HELD. Added RUN-20260824-0317:
  // without this the loop left `req_texel_i` parked on 9 for all twenty cycles,
  // so a block that re-read the array EVERY cycle instead of only on an
  // accepted request kept fetching the same texel and answered correctly by
  // accident. Mutant S07 — the read-data register's enable deleted — SURVIVED
  // this test for exactly that reason: the check was real, and its stimulus was
  // constant. Texel 7 holds 248 where texel 9 holds 246, so an ungated read
  // now changes the held word and the check below fails.
  //
  // Nothing is accepted during the stall (`req_ready_o` is low because the
  // response register is occupied), so this must be invisible.
  for (int c = 0; c < 20; ++c) {
    dut.req_texel_i = (c & 1) ? 7 : 11;
    dut.req_handle_i = kHandleC;
    dut.eval();
    zhao::tick(dut);
    dut.eval();
  }
  check(dut.pg_valid_o == 1, "the response is held under backpressure", 1, dut.pg_valid_o);
  check(dut.req_ready_o == 0, "and the request port is closed while it is held", 0,
        dut.req_ready_o);
  check(dut.pg_strength_o == static_cast<uint8_t>(255 - 9),
        "the held word is the one that was read", 246, dut.pg_strength_o);
  dut.pg_ready_i = 1;
  dut.eval();
  zhao::tick(dut);
  dut.eval();
  check(dut.pg_valid_o == 0, "the response drains once ready goes high", 0, dut.pg_valid_o);
}

void test_simultaneous_read_and_write(Vzhao_surface_sheet& dut) {
  // C5: the two ports are independent, so a write and a read may retire in the
  // same cycle at different addresses — which is exactly what SURFACE.STAMP
  // does at full rate.
  sdev::sheet_write(dut, kHandleC, 500, 0x5A, 0xA5, true, true, 0);
  dut.pg_ready_i = 1;
  dut.req_valid_i = 1;
  dut.req_op_i = sdev::kOpRead;
  dut.req_handle_i = kHandleC;
  dut.req_texel_i = 500;
  dut.wr_valid_i = 1;
  dut.wr_handle_i = kHandleC;
  dut.wr_texel_i = 501;
  dut.wr_tag_i = 0x3C;
  dut.wr_strength_i = 0xC3;
  dut.wr_we_tag_i = 1;
  dut.wr_we_strength_i = 1;
  dut.eval();
  const bool both = dut.req_ready_o && dut.wr_ready_o;
  zhao::tick(dut);
  dut.req_valid_i = 0;
  dut.wr_valid_i = 0;
  dut.eval();
  check(both, "a read and a write are accepted in the same cycle", 1, both ? 1 : 0);
  check(dut.pg_valid_o && dut.pg_tag_o == 0x5A, "the read returned texel 500", 0x5A, dut.pg_tag_o);
  zhao::tick(dut);
  const SheetResponse q = sdev::sheet_request(dut, sdev::kOpRead, kHandleC, 501, 0);
  check(q.tag == 0x3C && q.strength == 0xC3, "the concurrent write landed at texel 501", 0x3CC3,
        (static_cast<uint32_t>(q.tag) << 8) | q.strength);
}

void test_read_during_write_returns_the_old_word(Vzhao_surface_sheet& dut) {
  // C5's LAST SENTENCE, which had no test until RUN-20260824-0317:
  //
  //   "Read-during-write at the same address returns the OLD word (both
  //    accesses live in one `always_ff`). SURFACE.STAMP never does that -- its
  //    cursor marches forward and its write trails its read by two texels --
  //    but the semantics are stated rather than left to the synthesiser."
  //
  // WHY THIS EXISTS. `test_simultaneous_read_and_write` above drives both ports
  // in one cycle at DIFFERENT addresses (500 and 501), which is the case
  // SURFACE.STAMP actually generates. The SAME-address case was stated in the
  // contract, exercised by no consumer, and checked by nothing -- and the
  // reference cannot cover it either, because `zref::surface::SheetStore` is a
  // C++ model with no notion of a cycle, so the shipped differential is blind
  // to it by construction.
  //
  // That combination is exactly what a storage-shape change moves silently. The
  // mutation sweep proved it: mutant S05 -- "read-during-write returns the
  // POST-write word" -- SURVIVED the whole suite, all six consumer lanes, on
  // the first run. It is not an equivalent; the two behaviours are
  // distinguishable in 408 port-cycles of the run's shape differential. So the
  // gap is closed with a test rather than argued away.
  auto rdw = [&](uint16_t texel, uint8_t tag, uint8_t str, bool we_tag, bool we_str) {
    dut.pg_ready_i = 1;
    dut.req_valid_i = 1;
    dut.req_op_i = sdev::kOpRead;
    dut.req_handle_i = kHandleC;
    dut.req_texel_i = texel;
    dut.wr_valid_i = 1;
    dut.wr_handle_i = kHandleC;
    dut.wr_texel_i = texel;  // THE SAME TEXEL, in THE SAME CYCLE
    dut.wr_tag_i = tag;
    dut.wr_strength_i = str;
    dut.wr_we_tag_i = we_tag ? 1 : 0;
    dut.wr_we_strength_i = we_str ? 1 : 0;
    dut.eval();
    const bool both = dut.req_ready_o && dut.wr_ready_o;
    zhao::tick(dut);
    dut.req_valid_i = 0;
    dut.wr_valid_i = 0;
    dut.eval();
    const uint32_t seen = (static_cast<uint32_t>(dut.pg_tag_o) << 8) | dut.pg_strength_o;
    check(both && dut.pg_valid_o, "read-during-write: both ports accepted, one answer", 1,
          (both && dut.pg_valid_o) ? 1 : 0);
    zhao::tick(dut);
    return seen;
  };

  // ---- both halves written under the collision --------------------------
  sdev::sheet_write(dut, kHandleC, 700, 0x11, 0x22, true, true, 0);
  const uint32_t old_word = rdw(700, 0xEE, 0xDD, true, true);
  check(old_word == 0x1122, "a same-address read-during-write returns the PRE-write word", 0x1122,
        old_word);
  const SheetResponse after = sdev::sheet_request(dut, sdev::kOpRead, kHandleC, 700, 0);
  check((static_cast<uint32_t>(after.tag) << 8 | after.strength) == 0xEEDD,
        "and the write it raced still landed", 0xEEDD,
        (static_cast<uint32_t>(after.tag) << 8) | after.strength);

  // ---- ONE half written under the collision ------------------------------
  // The enables used to be byte enables on one 16-bit word and are now
  // per-plane write enables on two arrays. A collision that writes only the tag
  // must still answer the old word in BOTH halves, and must leave the strength
  // alone afterwards -- which is the one place the split could have gone wrong
  // in a way the whole-word case cannot see.
  sdev::sheet_write(dut, kHandleC, 701, 0x33, 0x44, true, true, 0);
  const uint32_t old_tag_only = rdw(701, 0x99, 0x88, true, false);
  check(old_tag_only == 0x3344,
        "a tag-only read-during-write still returns the whole PRE-write word", 0x3344,
        old_tag_only);
  const SheetResponse half = sdev::sheet_request(dut, sdev::kOpRead, kHandleC, 701, 0);
  check((static_cast<uint32_t>(half.tag) << 8 | half.strength) == 0x9944,
        "the tag moved and the strength did not", 0x9944,
        (static_cast<uint32_t>(half.tag) << 8) | half.strength);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Vzhao_surface_sheet dut;
  sdev::reset_sheet(dut);

  test_reset_state(dut);
  test_acquire_allocates_and_clears(dut);
  test_write_readback_and_byte_enables(dut);
  test_reacquire_persists(dut);
  test_second_slot_and_isolation(dut);
  test_overflow_rejects_and_never_evicts(dut);
  test_non_resident_write_is_dropped(dut);
  test_release_ends_persistence(dut);
  test_clear_sweep_is_visible(dut);
  test_read_throughput_and_backpressure(dut);
  test_simultaneous_read_and_write(dut);
  test_read_during_write_returns_the_old_word(dut);

  return zhao::report_and_exit("surface_sheet_directed");
}
