// measure_histogram_directed.cpp -- MEASURE.HISTOGRAM directed tests
// (phase 8, ZH-049).
//
// THIS IS NOT A DIFFERENTIAL LANE AND CANNOT BE ONE. `design/blocks.yml`
// declares `reference_model: zref::MeasureHistogram`; that name resolves to
// nothing and never has. `python tools/budget/refmodel_liveness.py` reports it
// as one of eleven unresolved models and says what that costs:
//
//     "These blocks have NO differential test available. A directed test for
//      one of them checks self-consistency against contract prose, not
//      agreement with a ratified model."
//
// So every expectation below is either HAND-COMPUTED (the bucket table is
// written out value by value with its arithmetic; the cycle counts are derived
// from the pipeline in the comment above the lane that asserts them) or is a
// STRUCTURAL property read off the block's own ports. `hist_test::Model` is
// bookkeeping across long stimulus, not an oracle, and no lane rests on it
// alone.
//
// What each lane would catch -- the "could have been red" statement:
//
//   1. THE ONE-TIME SCRUB -- `ev_ready_o` and `rd_ready_o` are low for exactly
//      2^(BINW+1) = 128 cycles after reset and then rise, and every bin reads
//      zero afterwards. Red on: a scrub that never ends, a scrub one address
//      short (which leaves one bin holding power-up garbage that the epoch
//      compare might accept), or no scrub at all.
//   2. THE BUCKET TABLE, HAND-COMPUTED -- eighteen magnitudes whose bins are
//      worked out by hand in the comment, driven through the DUT and read back
//      out of the memory. Red on: an off-by-one in the exponent, a mantissa
//      taken from the wrong end, a priority encoder that stops at the lowest
//      set bit instead of the highest, bin 63 wrapping to 0 at 0xFFFFFFFF.
//   3. SCALE INVARIANCE, ON THE DUT -- doubling a magnitude moves it exactly
//      2^SUB_BITS = 2 bins. This is the property that lets the error metric's
//      Q format stay undecided (module header, invention 1), so it is checked
//      on the block rather than asserted in prose. Red on: any bucketing that
//      is not log2 with a fixed mantissa.
//   4. SAME-BIN AGGREGATION IN ONE CYCLE (law H1) -- four events at one bin
//      cost ONE memory update and ZERO stall cycles, and the bin gains four.
//      This is the plan's first requirement. Red on: a block that serialises
//      unconditionally (right totals, four times the memory traffic) -- which
//      is invisible to any test that checks only the bin contents, and is
//      exactly CLAUDE.md's "counters see what pictures cannot".
//   5. DISTINCT BINS SERIALISE WITH BACKPRESSURE (law H2) -- four distinct
//      bins cost four updates and refuse the next beat for exactly three
//      cycles. Red on: a block that accepts a beat it cannot retire, i.e. one
//      that drops events at four-per-cycle.
//   6. READ-AFTER-WRITE FORWARDING (law H3) -- eight beats to the SAME bin on
//      eight consecutive cycles must total eight, with exactly seven
//      forwarding substitutions. Without the forward the count comes out at
//      one. This is the hazard the plan names and the one the memory cannot
//      solve by itself. Red on: no forwarding, or forwarding from the wrong
//      pipeline stage.
//   7. AN EVENT HITTING A BIN THAT IS BEING READ BACK -- the host holds
//      `rd_valid_i` on bin 4 of the frozen bank for six cycles while four
//      events are queued for the active bank. The host sees the frozen value
//      on every read, unchanged; the events all land afterwards; and
//      `host_conflict_o` counts exactly six. Red on: a shared read port with
//      no arbitration (the host's answer would be an accumulator read), or a
//      single bank (the host would watch the interval move under it).
//   8. A SNAPSHOT TAKEN MID-INTERVAL (laws S1-S3) -- `snapshot_i` is pulsed
//      while three of four lanes are still queued. All four must land in the
//      OLD interval and the new one must start empty. Red on: a swap that does
//      not drain, which puts part of a beat in each interval -- the "mixture"
//      the plan forbids by name.
//   9. THE EPOCH CLEAR IS REAL (law S2) -- a bank that held counts two
//      intervals ago reads as all zeros when it is recycled, with no scrub in
//      between. Red on: an epoch bit that is compared but not flipped, which
//      leaks the whole of interval N-2 into interval N.
//  10. THE SNAPSHOT SUMMARY -- total, index and src_id describe the frozen
//      interval and not the live one. Red on: a total sampled from the wrong
//      bank, an index that counts requests instead of swaps.
//  11. A LONG MIXED STREAM -- 400 beats of mixed masks and magnitudes through
//      real ready/valid backpressure, every bin compared. Red on: any loss
//      under sustained backpressure that the short lanes do not reach.
//  12. THE GUARD READS ZERO, AND SAYS SO HONESTLY -- `frozen_write_o` is
//      asserted zero at the end of every lane. A zero is a claim, not
//      evidence, so the evidence that it CAN fire is a separate committed
//      mutant: tests/mutants/zhao_measure_histogram_drain_mutant.sv, driven by
//      measure_histogram_drain_mutant.cpp, which PASSES WHEN IT FIRES.
//
// Saturation at the chosen width is lane 13 and lives in
// measure_histogram_sat.cpp, because reaching it at the shipped CW = 24 needs
// 16.7 million events; that file is the same RTL built with -GCW=8.

#include "Vzhao_measure_histogram.h"

#include "histogram_dev.hpp"

#include <cstdio>
#include <vector>

namespace {

using hist_test::Beat;
using hist_test::Counters;
using hist_test::kAddrBins;
using hist_test::kScrubCycles;
using zhao::check;

/** A single-lane beat carrying one magnitude. */
Beat one(uint32_t err, uint16_t src = 0) {
  Beat b;
  b.mask = 0x1;
  b.err[0] = err;
  b.src = src;
  return b;
}

/** Count how many bins hold a non-zero value, and where. */
size_t occupied(const std::vector<uint32_t>& v) {
  size_t n = 0;
  for (uint32_t x : v) {
    if (x != 0) ++n;
  }
  return n;
}

// ---------------------------------------------------------------------------
// 1. The one-time scrub
// ---------------------------------------------------------------------------
void test_scrub(Vzhao_measure_histogram& d) {
  hist_test::reset(d);

  check(d.ev_ready_o == 0, "scrub: ev_ready_o is LOW immediately after reset", 0, d.ev_ready_o);
  check(d.rd_ready_o == 0, "scrub: rd_ready_o is LOW immediately after reset", 0, d.rd_ready_o);

  // The block walks all 2^AW = 2^(BINW+1) = 128 addresses, one per cycle,
  // writing {epoch 0, count 0}. Not 64 (that would leave the second bank
  // holding power-up contents) and not 129.
  const int cycles = hist_test::wait_scrub(d);
  check(cycles == kScrubCycles, "scrub: takes exactly 2^(BINW+1) cycles", kScrubCycles,
        static_cast<uint64_t>(cycles));
  check(d.rd_ready_o != 0, "scrub: rd_ready_o rises with ev_ready_o", 1, d.rd_ready_o);

  const Counters c = hist_test::counters(d);
  check(c.events == 0 && c.updates == 0 && c.snapshots == 0,
        "scrub: the walk moves no event counter", 0, c.events + c.updates + c.snapshots);

  // Both banks must read zero. One snapshot exposes the first, a second the
  // other -- so this checks 128 words, not 64.
  for (int pass = 0; pass < 2; ++pass) {
    check(hist_test::snapshot(d) >= 0, "scrub: snapshot completes", 1, 1);
    const std::vector<uint32_t> bins = hist_test::read_all(d);
    check(occupied(bins) == 0, "scrub: every scrubbed bin reads zero", 0,
          static_cast<uint64_t>(occupied(bins)));
  }
  check(d.frozen_write_o == 0, "scrub: frozen_write_o zero", 0, d.frozen_write_o);
}

// ---------------------------------------------------------------------------
// 2 + 3. The bucket table, hand-computed, and scale invariance
// ---------------------------------------------------------------------------
// SUB_BITS = 1, so bin = (e) exact below 2, else ((e-1+1)<<1) + bit(e-1).
// Worked out one at a time:
//   0          -> no set bit                      -> 0
//   1          -> e=0 < SUB_BITS                  -> 1
//   2  (0b10)  -> e=1, (1<<1)=2, bit0 of 2 = 0    -> 2
//   3  (0b11)  -> e=1, 2,          bit0 of 3 = 1  -> 3
//   4  (0b100) -> e=2, (2<<1)=4,   bit1 of 4 = 0  -> 4
//   5          -> e=2, 4,          bit1 of 5 = 0  -> 4
//   6          -> e=2, 4,          bit1 of 6 = 1  -> 5
//   7          -> e=2, 4,          bit1 of 7 = 1  -> 5
//   8          -> e=3, (3<<1)=6,   bit2 of 8 = 0  -> 6
//   11         -> e=3, 6,          bit2 of 11 = 0 -> 6
//   12         -> e=3, 6,          bit2 of 12 = 1 -> 7
//   15         -> e=3, 6,          bit2 of 15 = 1 -> 7
//   16         -> e=4, (4<<1)=8,   bit3 of 16 = 0 -> 8
//   100        -> e=6, (6<<1)=12,  bit5 of 100 = 1-> 13
//   0x8000     -> e=15, (15<<1)=30,bit14 = 0      -> 30
//   0xFFFF     -> e=15, 30,        bit14 = 1      -> 31
//   0x80000000 -> e=31, (31<<1)=62,bit30 = 0      -> 62
//   0xFFFFFFFF -> e=31, 62,        bit30 = 1      -> 63   (and 63 < NBINS)
struct TableRow {
  uint32_t err;
  int bin;
};
const TableRow kTable[] = {
    {0u, 0},  {1u, 1},    {2u, 2},       {3u, 3},       {4u, 4},           {5u, 4},
    {6u, 5},  {7u, 5},    {8u, 6},       {11u, 6},      {12u, 7},          {15u, 7},
    {16u, 8}, {100u, 13}, {0x8000u, 30}, {0xFFFFu, 31}, {0x80000000u, 62}, {0xFFFFFFFFu, 63},
};

void test_bucket_table(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);

  uint32_t want[kAddrBins] = {0};
  for (const TableRow& r : kTable) {
    check(hist_test::send_beat(d, one(r.err)) == 0, "table: beat accepted without stall", 0, 1);
    want[r.bin] += 1;
  }
  hist_test::idle(d, 8);
  check(hist_test::snapshot(d) >= 0, "table: snapshot completes", 1, 1);
  const std::vector<uint32_t> bins = hist_test::read_all(d);

  int wrong = 0;
  for (int b = 0; b < kAddrBins; ++b) {
    if (bins[static_cast<size_t>(b)] != want[b]) ++wrong;
  }
  check(wrong == 0, "table: every hand-computed magnitude lands in its hand-computed bin", 0,
        static_cast<uint64_t>(wrong));
  check(bins[63] == 1u, "table: 0xFFFFFFFF reaches the TOP bin and does not wrap", 1, bins[63]);
  check(bins[0] == 1u, "table: a zero magnitude is bin 0, not 'no event'", 1, bins[0]);
  check(static_cast<uint32_t>(d.snap_total_o) == 18u, "table: total is the 18 events offered", 18,
        d.snap_total_o);
  check(d.frozen_write_o == 0, "table: frozen_write_o zero", 0, d.frozen_write_o);
}

void test_scale_invariance(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);

  // 5, 10, 20, 40, 80 -- each double of the last. Each doubling must move the
  // event by exactly 2^SUB_BITS = 2 bins and by nothing else. This is what
  // makes the error metric's Q format a non-question (module header,
  // invention 1): rescaling the metric translates the histogram.
  const uint32_t seq[] = {5u, 10u, 20u, 40u, 80u};
  for (uint32_t v : seq) {
    check(hist_test::send_beat(d, one(v)) == 0, "scale: beat accepted", 0, 1);
  }
  hist_test::idle(d, 8);
  hist_test::snapshot(d);
  const std::vector<uint32_t> bins = hist_test::read_all(d);

  std::vector<int> hit;
  for (int b = 0; b < kAddrBins; ++b) {
    if (bins[static_cast<size_t>(b)] != 0u) hit.push_back(b);
  }
  check(hit.size() == 5, "scale: five doublings occupy five distinct bins", 5,
        static_cast<uint64_t>(hit.size()));
  int bad = 0;
  for (size_t i = 1; i < hit.size(); ++i) {
    if (hit[i] - hit[i - 1] != (1 << hist_test::kSubBits)) ++bad;
  }
  check(bad == 0, "scale: doubling the magnitude moves the event exactly 2^SUB_BITS bins", 0,
        static_cast<uint64_t>(bad));
  check(d.frozen_write_o == 0, "scale: frozen_write_o zero", 0, d.frozen_write_o);
}

// ---------------------------------------------------------------------------
// 4. Same-bin aggregation (law H1)
// ---------------------------------------------------------------------------
void test_same_bin_aggregation(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);
  const Counters c0 = hist_test::counters(d);

  // Four magnitudes that all bucket to bin 4 (4,5 -> 4 and 4,5 -> 4 above).
  Beat b;
  b.mask = 0xF;
  b.err[0] = 4u;
  b.err[1] = 5u;
  b.err[2] = 4u;
  b.err[3] = 5u;
  check(hist_test::send_beat(d, b) == 0, "aggregate: the beat is accepted with no stall", 0, 1);
  // ... and the NEXT beat is accepted on the very next cycle, because the one
  // group retires immediately. A block that serialised would refuse it thrice.
  check(hist_test::send_beat(d, b) == 0, "aggregate: the following beat is accepted immediately", 0,
        1);
  hist_test::idle(d, 8);

  const Counters c1 = hist_test::counters(d);
  check(c1.events - c0.events == 8u, "aggregate: eight events accepted", 8, c1.events - c0.events);
  check(c1.updates - c0.updates == 2u, "aggregate: EIGHT events cost TWO memory updates, not eight",
        2, c1.updates - c0.updates);
  check(c1.stalls - c0.stalls == 0u, "aggregate: no stall cycle was needed", 0,
        c1.stalls - c0.stalls);

  hist_test::snapshot(d);
  const std::vector<uint32_t> bins = hist_test::read_all(d);
  check(bins[4] == 8u, "aggregate: bin 4 gained all eight", 8, bins[4]);
  check(occupied(bins) == 1, "aggregate: and nothing landed anywhere else", 1,
        static_cast<uint64_t>(occupied(bins)));
  check(d.frozen_write_o == 0, "aggregate: frozen_write_o zero", 0, d.frozen_write_o);
}

// ---------------------------------------------------------------------------
// 5. Distinct bins serialise, with backpressure (law H2)
// ---------------------------------------------------------------------------
void test_distinct_bins_serialise(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);
  const Counters c0 = hist_test::counters(d);

  // 1 -> 1, 4 -> 4, 100 -> 13, 0xFFFF -> 31. Four bins, so four groups, so the
  // NEXT beat is refused for exactly three cycles: the beat retires one group
  // per cycle and `ev_ready_o` rises only as the last one issues.
  Beat b;
  b.mask = 0xF;
  b.err[0] = 1u;
  b.err[1] = 4u;
  b.err[2] = 100u;
  b.err[3] = 0xFFFFu;

  check(hist_test::send_beat(d, b) == 0, "serialise: the first beat is accepted immediately", 0, 1);
  const int refused = hist_test::send_beat(d, b);
  check(refused == 3, "serialise: the next beat is refused for exactly LANES-1 = 3 cycles", 3,
        static_cast<uint64_t>(refused));
  hist_test::idle(d, 12);

  const Counters c1 = hist_test::counters(d);
  check(c1.events - c0.events == 8u, "serialise: eight events accepted", 8, c1.events - c0.events);
  check(c1.updates - c0.updates == 8u, "serialise: four distinct bins cost four updates a beat", 8,
        c1.updates - c0.updates);
  check(c1.stalls - c0.stalls == 3u, "serialise: three cycles were counted as refused", 3,
        c1.stalls - c0.stalls);

  hist_test::snapshot(d);
  const std::vector<uint32_t> bins = hist_test::read_all(d);
  check(bins[1] == 2u && bins[4] == 2u && bins[13] == 2u && bins[31] == 2u,
        "serialise: each of the four bins holds both beats' events", 2, bins[13]);
  check(occupied(bins) == 4, "serialise: exactly four bins occupied", 4,
        static_cast<uint64_t>(occupied(bins)));
  check(d.frozen_write_o == 0, "serialise: frozen_write_o zero", 0, d.frozen_write_o);
}

// ---------------------------------------------------------------------------
// 6. Read-after-write forwarding (law H3)
// ---------------------------------------------------------------------------
// The memory is read in cycle T and written in T+1. Eight single-lane beats to
// one bin are accepted on eight CONSECUTIVE cycles, so every group but the
// first reads the memory in the same cycle its predecessor is writing it.
// Without the forward every one of them would read the pre-write value and the
// bin would end at 1, not 8 -- and `fwd_hits_o` is what says the forward was
// the reason, rather than luck in the scheduling.
void test_forwarding(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);
  const Counters c0 = hist_test::counters(d);

  for (int i = 0; i < 8; ++i) {
    check(hist_test::send_beat(d, one(5u)) == 0, "forward: back-to-back beat accepted", 0, 1);
  }
  hist_test::idle(d, 8);

  const Counters c1 = hist_test::counters(d);
  check(c1.updates - c0.updates == 8u, "forward: eight separate memory updates", 8,
        c1.updates - c0.updates);
  check(c1.fwd_hits - c0.fwd_hits == 7u,
        "forward: seven of the eight needed the write-in-flight substituted", 7,
        c1.fwd_hits - c0.fwd_hits);

  hist_test::snapshot(d);
  const std::vector<uint32_t> bins = hist_test::read_all(d);
  check(bins[4] == 8u, "forward: the bin holds all eight, not one", 8, bins[4]);

  // And the hazard is exactly one cycle deep: spacing the beats out removes
  // every forward and must not change the answer.
  const Counters c2 = hist_test::counters(d);
  for (int i = 0; i < 8; ++i) {
    hist_test::send_beat(d, one(5u));
    hist_test::idle(d, 4);
  }
  hist_test::idle(d, 8);
  const Counters c3 = hist_test::counters(d);
  check(c3.fwd_hits - c2.fwd_hits == 0u, "forward: spaced updates need no forwarding at all", 0,
        c3.fwd_hits - c2.fwd_hits);
  hist_test::snapshot(d);
  const std::vector<uint32_t> bins2 = hist_test::read_all(d);
  check(bins2[4] == 8u, "forward: and the spaced run totals the same eight", 8, bins2[4]);
  check(d.frozen_write_o == 0, "forward: frozen_write_o zero", 0, d.frozen_write_o);
}

// ---------------------------------------------------------------------------
// 7. An event hitting a bin that is being read back
// ---------------------------------------------------------------------------
void test_read_during_update(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);

  // Interval A: three events into bin 4, then freeze it. That is what the host
  // will be reading.
  for (int i = 0; i < 3; ++i) hist_test::send_beat(d, one(5u));
  hist_test::idle(d, 8);
  hist_test::snapshot(d);
  const uint32_t frozen_seed = hist_test::read_bin(d, 4).count;
  check(frozen_seed == 3u, "read/update: the frozen bin starts at three", 3, frozen_seed);

  const Counters c0 = hist_test::counters(d);

  // Interval B: queue four events across four DISTINCT bins -- one of which is
  // bin 4, the bin the host is reading -- then hold `rd_valid_i` high for six
  // cycles. The read port has priority, so no group can retire while the host
  // is reading, and every one of those six cycles is a conflict.
  Beat b;
  b.mask = 0xF;
  b.err[0] = 5u;       // bin 4, the contended one
  b.err[1] = 1u;       // bin 1
  b.err[2] = 100u;     // bin 13
  b.err[3] = 0xFFFFu;  // bin 31
  check(hist_test::send_beat(d, b) == 0, "read/update: the beat is accepted", 0, 1);

  d.rd_valid_i = 1;
  d.rd_bin_i = 4;
  d.eval();
  int reads_seen = 0;
  int reads_wrong = 0;
  for (int i = 0; i < 6; ++i) {
    zhao::tick(d);
    if (d.rd_data_valid_o) {
      ++reads_seen;
      if (static_cast<uint32_t>(d.rd_count_o) != 3u) ++reads_wrong;
    }
  }
  d.rd_valid_i = 0;
  d.eval();
  hist_test::idle(d, 16);

  check(reads_seen >= 4, "read/update: the host was served throughout the contention", 4,
        static_cast<uint64_t>(reads_seen));
  check(reads_wrong == 0,
        "read/update: every read returned the FROZEN value, unmoved by the live events", 0,
        static_cast<uint64_t>(reads_wrong));

  const Counters c1 = hist_test::counters(d);
  check(c1.host_conflict - c0.host_conflict == 6u,
        "read/update: six cycles of the host holding the read port off the accumulator", 6,
        c1.host_conflict - c0.host_conflict);
  check(c1.events - c0.events == 4u, "read/update: no event was dropped for the contention", 4,
        c1.events - c0.events);

  hist_test::snapshot(d);
  const std::vector<uint32_t> bins = hist_test::read_all(d);
  check(bins[4] == 1u && bins[1] == 1u && bins[13] == 1u && bins[31] == 1u,
        "read/update: all four queued events landed, in the NEW interval", 1, bins[4]);
  check(occupied(bins) == 4, "read/update: and only those four", 4,
        static_cast<uint64_t>(occupied(bins)));
  check(d.frozen_write_o == 0, "read/update: frozen_write_o zero", 0, d.frozen_write_o);
}

// ---------------------------------------------------------------------------
// 8. A snapshot taken mid-interval (laws S1-S3)
// ---------------------------------------------------------------------------
void test_snapshot_drains(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);

  // An idle block swaps as early as the design permits, which is ONE edge
  // after the pulse and not zero: `snapshot_i` is a one-cycle pulse and is
  // REGISTERED into `snap_req_q`, so the earliest cycle in which `swap_c` can
  // be true is the one after the pulse. `hist_test::snapshot` spends its first
  // tick on the pulse edge and then counts, so an undrained swap reads 1.
  // (This expectation was 0 on first writing, from a derivation that forgot the
  // request register. The block was right and the arithmetic was wrong; the
  // number is corrected here WITH its derivation rather than to whatever the
  // DUT happened to print.)
  const int idle_drain = hist_test::snapshot(d);
  check(idle_drain == 1, "snapshot: an empty pipeline swaps on the very next edge", 1,
        static_cast<uint64_t>(idle_drain));

  const Counters c0 = hist_test::counters(d);

  // Now pulse `snapshot_i` on the cycle immediately after a four-distinct-bin
  // beat is accepted, i.e. with all four lanes still queued and nothing yet
  // written. The swap must wait: four issue cycles, one for the last update in
  // flight, then the swap -- so a handful of cycles, never zero.
  Beat b;
  b.mask = 0xF;
  b.err[0] = 1u;
  b.err[1] = 4u;
  b.err[2] = 100u;
  b.err[3] = 0xFFFFu;
  check(hist_test::send_beat(d, b) == 0, "snapshot: the beat is accepted", 0, 1);
  const int drain = hist_test::snapshot(d);
  check(drain >= 4 && drain <= 8, "snapshot: the swap WAITED for the queued lanes", 1,
        static_cast<uint64_t>(drain));

  const std::vector<uint32_t> frozen = hist_test::read_all(d);
  check(frozen[1] == 1u && frozen[4] == 1u && frozen[13] == 1u && frozen[31] == 1u,
        "snapshot: all four events are in the OLD interval", 1, frozen[13]);
  check(occupied(frozen) == 4, "snapshot: the old interval holds exactly those four", 4,
        static_cast<uint64_t>(occupied(frozen)));
  check(static_cast<uint32_t>(d.snap_total_o) == 4u, "snapshot: and its total says four", 4,
        d.snap_total_o);

  // The NEW interval must be empty -- no part of that beat leaked forward.
  hist_test::idle(d, 8);
  hist_test::snapshot(d);
  const std::vector<uint32_t> next = hist_test::read_all(d);
  check(occupied(next) == 0, "snapshot: the new interval started EMPTY -- no mixture", 0,
        static_cast<uint64_t>(occupied(next)));
  check(static_cast<uint32_t>(d.snap_total_o) == 0u, "snapshot: and its total is zero", 0,
        d.snap_total_o);

  const Counters c1 = hist_test::counters(d);
  check(c1.events - c0.events == 4u, "snapshot: nothing was dropped across the boundary", 4,
        c1.events - c0.events);
  check(c1.frozen_write == 0u, "snapshot: no update ever targeted the frozen bank", 0,
        c1.frozen_write);
}

// ---------------------------------------------------------------------------
// 9. The epoch clear is real (law S2)
// ---------------------------------------------------------------------------
// Bank 0 is filled, frozen, recycled, and must come back EMPTY without any
// scrub having run. Nothing else in the block clears it.
void test_epoch_clear(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);

  // Interval A (bank 0): 5 events into bin 4.
  for (int i = 0; i < 5; ++i) hist_test::send_beat(d, one(5u));
  hist_test::idle(d, 8);
  hist_test::snapshot(d);  // bank 0 frozen, bank 1 live
  const std::vector<uint32_t> a = hist_test::read_all(d);
  check(a[4] == 5u, "epoch: interval A holds its five", 5, a[4]);

  // Interval B (bank 1): 2 events into bin 13.
  hist_test::send_beat(d, one(100u));
  hist_test::send_beat(d, one(100u));
  hist_test::idle(d, 8);
  hist_test::snapshot(d);  // bank 1 frozen, bank 0 live AGAIN
  const std::vector<uint32_t> bb = hist_test::read_all(d);
  check(bb[13] == 2u && occupied(bb) == 1, "epoch: interval B holds its two and nothing of A", 2,
        bb[13]);

  // Interval C (bank 0 recycled): ONE event into bin 1. If the epoch flip were
  // missing, bin 4 would still be showing interval A's five.
  hist_test::send_beat(d, one(1u));
  hist_test::idle(d, 8);
  hist_test::snapshot(d);
  const std::vector<uint32_t> cc = hist_test::read_all(d);
  check(cc[1] == 1u, "epoch: interval C holds its one", 1, cc[1]);
  check(cc[4] == 0u, "epoch: interval A's five did NOT survive the bank recycle", 0, cc[4]);
  check(occupied(cc) == 1, "epoch: a recycled bank comes back completely empty", 1,
        static_cast<uint64_t>(occupied(cc)));
  check(d.frozen_write_o == 0, "epoch: frozen_write_o zero", 0, d.frozen_write_o);
}

// ---------------------------------------------------------------------------
// 10. The snapshot summary describes the FROZEN interval
// ---------------------------------------------------------------------------
void test_snapshot_summary(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);

  check(d.snap_valid_o == 0, "summary: no interval is frozen before the first snapshot", 0,
        d.snap_valid_o);
  check(static_cast<uint32_t>(d.snap_index_o) == 0u, "summary: the index starts at zero", 0,
        d.snap_index_o);

  hist_test::send_beat(d, one(5u, 0x1111));
  hist_test::send_beat(d, one(5u, 0x2222));
  hist_test::send_beat(d, one(100u, 0x3333));
  hist_test::idle(d, 8);
  hist_test::snapshot(d);

  check(d.snap_valid_o != 0, "summary: an interval is frozen after the first snapshot", 1,
        d.snap_valid_o);
  check(static_cast<uint32_t>(d.snap_total_o) == 3u, "summary: total is the three events", 3,
        d.snap_total_o);
  check(static_cast<uint32_t>(d.snap_index_o) == 1u, "summary: the index counts SWAPS", 1,
        d.snap_index_o);
  check(static_cast<uint32_t>(d.snap_src_id_o) == 0x3333u,
        "summary: src_id is the last event folded into the frozen interval", 0x3333,
        d.snap_src_id_o);

  // A second snapshot with nothing in between still advances the index, and
  // the summary follows the new frozen bank rather than holding the old one.
  hist_test::snapshot(d);
  check(static_cast<uint32_t>(d.snap_index_o) == 2u, "summary: the index advances again", 2,
        d.snap_index_o);
  check(static_cast<uint32_t>(d.snap_total_o) == 0u,
        "summary: the total follows the NEW frozen bank, which is empty", 0, d.snap_total_o);
  check(d.frozen_write_o == 0, "summary: frozen_write_o zero", 0, d.frozen_write_o);
}

// ---------------------------------------------------------------------------
// 11. A long mixed stream through real backpressure
// ---------------------------------------------------------------------------
void test_long_stream(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);
  const Counters c0 = hist_test::counters(d);

  hist_test::Model m;
  uint32_t lcg = 0xC0FFEEu;
  auto next = [&lcg]() {
    lcg = lcg * 1664525u + 1013904223u;
    return lcg;
  };

  uint64_t offered = 0;
  for (int beat = 0; beat < 400; ++beat) {
    Beat b;
    b.mask = static_cast<uint8_t>((next() & 0xFu) | 0x1u);  // never an empty beat
    b.src = static_cast<uint16_t>(next() & 0xFFFFu);
    for (int l = 0; l < hist_test::kLanes; ++l) {
      // A spread that reaches high bins as well as low: a raw draw is almost
      // always huge, so shift it by a varying amount.
      const uint32_t raw = next();
      b.err[l] = raw >> (next() % 30u);
      if ((b.mask >> l) & 1u) {
        m.add(b.err[l]);
        ++offered;
      }
    }
    check(hist_test::send_beat(d, b) >= 0, "stream: no beat hung", 1, 1);
    if ((beat % 7) == 0) hist_test::idle(d, 1);
  }
  hist_test::idle(d, 16);
  hist_test::snapshot(d);

  const Counters c1 = hist_test::counters(d);
  check(c1.events - c0.events == offered, "stream: every offered event was accepted", offered,
        c1.events - c0.events);
  check(static_cast<uint32_t>(d.snap_total_o) == offered,
        "stream: the interval total equals the events offered", offered, d.snap_total_o);
  check(c1.updates - c0.updates <= c1.events - c0.events,
        "stream: memory updates never EXCEED events (aggregation can only reduce)", 1,
        static_cast<uint64_t>((c1.updates - c0.updates) <= (c1.events - c0.events)));

  const std::vector<uint32_t> bins = hist_test::read_all(d);
  int wrong = 0;
  uint64_t sum = 0;
  for (int b = 0; b < kAddrBins; ++b) {
    sum += bins[static_cast<size_t>(b)];
    if (bins[static_cast<size_t>(b)] != m.bins[b]) ++wrong;
  }
  check(wrong == 0, "stream: every one of the 64 bins matches the bookkeeping", 0,
        static_cast<uint64_t>(wrong));
  check(sum == offered, "stream: the bins sum to the events, so nothing was lost or doubled",
        offered, sum);
  check(c1.bin_sat == 0u, "stream: nothing came near saturating a 24-bit bin", 0, c1.bin_sat);
  check(c1.frozen_write == 0u, "stream: frozen_write_o zero", 0, c1.frozen_write);
}

// ---------------------------------------------------------------------------
// 12. COUNTER LIVENESS -- every counter seen to MOVE, in one continuous case
// ---------------------------------------------------------------------------
// Plan section 11.5: "each mandatory functional stage sees positive work in a
// fixture and a deliberate bypass/mute changes the expected result." The lanes
// above supply the second half -- mute the aggregation and lane 4's
// `updates == 2` becomes 8; mute the forwarding and lane 6's bin reads 1
// instead of 8; mute the drain and the committed mutant fires. This lane
// supplies the first half for the counters themselves, and prints them, so a
// counter that is quietly stuck at zero cannot hide behind a passing suite.
//
// Two counters CANNOT move here and are covered elsewhere on purpose:
//   bin_sat_o     -- needs 16.7M events at CW = 24; measure_histogram_sat.cpp.
//   frozen_write_o -- structurally unreachable; the committed mutant.
void test_counter_liveness(Vzhao_measure_histogram& d) {
  hist_test::reset(d);
  hist_test::wait_scrub(d);

  Beat quad;  // four distinct bins -> serialisation, stalls, many updates
  quad.mask = 0xF;
  quad.err[0] = 1u;
  quad.err[1] = 4u;
  quad.err[2] = 100u;
  quad.err[3] = 0xFFFFu;
  hist_test::send_beat(d, quad);
  hist_test::send_beat(d, quad);  // refused for three cycles -> stall_cycles_o

  for (int i = 0; i < 4; ++i) hist_test::send_beat(d, one(5u));  // -> fwd_hits_o

  hist_test::send_beat(d, quad);  // queue four groups, then contend the port
  d.rd_valid_i = 1;
  d.rd_bin_i = 4;
  d.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(d);  // -> host_conflict_o
  d.rd_valid_i = 0;
  d.eval();
  hist_test::idle(d, 24);
  hist_test::snapshot(d);  // -> snapshots_o

  const Counters c = hist_test::counters(d);
  std::printf(
      "  counter liveness: events=%u updates=%u stalls=%u fwd_hits=%u\n"
      "                    host_conflict=%u snapshots=%u bin_sat=%u frozen_write=%u\n",
      c.events, c.updates, c.stalls, c.fwd_hits, c.host_conflict, c.snapshots, c.bin_sat,
      c.frozen_write);

  check(c.events > 0u, "liveness: events_o moved", 1, c.events);
  check(c.updates > 0u, "liveness: updates_o moved", 1, c.updates);
  check(c.stalls > 0u, "liveness: stall_cycles_o moved", 1, c.stalls);
  check(c.fwd_hits > 0u, "liveness: fwd_hits_o moved", 1, c.fwd_hits);
  check(c.host_conflict > 0u, "liveness: host_conflict_o moved", 1, c.host_conflict);
  check(c.snapshots > 0u, "liveness: snapshots_o moved", 1, c.snapshots);
  check(c.bin_sat == 0u, "liveness: bin_sat_o cannot move at CW=24 -- see measure_histogram_sat", 0,
        c.bin_sat);
  check(c.frozen_write == 0u,
        "liveness: frozen_write_o cannot move at all -- see the committed drain mutant", 0,
        c.frozen_write);
}

}  // namespace

int main() {
  auto* dut = new Vzhao_measure_histogram;  // heap, per the harness contract

  test_scrub(*dut);
  test_bucket_table(*dut);
  test_scale_invariance(*dut);
  test_same_bin_aggregation(*dut);
  test_distinct_bins_serialise(*dut);
  test_forwarding(*dut);
  test_read_during_update(*dut);
  test_snapshot_drains(*dut);
  test_epoch_clear(*dut);
  test_snapshot_summary(*dut);
  test_long_stream(*dut);
  test_counter_liveness(*dut);

  std::printf(
      "measure_histogram_directed: NOT a differential lane -- zref::MeasureHistogram\n"
      "  does not resolve (refmodel_liveness.py). Expectations are hand-computed or\n"
      "  structural; agreement with the in-file Model is bookkeeping, not evidence.\n");

  const int rc = zhao::report_and_exit("measure_histogram_directed");
  zhao::exit_hard(rc);
}
