// histogram_dev.hpp -- the shared driver for MEASURE.HISTOGRAM.
//
// THERE IS NO ORACLE, AND THIS FILE IS NOT ONE. `design/blocks.yml` declares
// `reference_model: zref::MeasureHistogram`, and that name RESOLVES TO NOTHING
// -- `python tools/budget/refmodel_liveness.py` lists it among eleven declared
// models absent from the tree, and the contract calls it "the tenth phantom".
// The tool's own sentence about this class of block is the honest statement of
// what every lane built on this header can and cannot claim:
//
//     "These blocks have NO differential test available. A directed test for
//      one of them checks self-consistency against contract prose, not
//      agreement with a ratified model."
//
// So `Model` below is NOT a reference implementation and agreement with it is
// NOT evidence of correctness -- it is a second statement of the same intent by
// the same author on the same day, and it can be wrong in exactly the same
// direction as the RTL. It exists only to carry bookkeeping across a long
// stimulus so a lane does not have to. Everything that actually pins the
// block's behaviour is either HAND-COMPUTED in the lane that asserts it (the
// bucket table, the aggregation cycle counts, the forwarding count, the
// snapshot boundary) or is a structural property of the RTL read off its
// ports. Where a lane's expectation is hand-computed, the derivation is in the
// comment above it.
//
// One place knows the handshake, so every lane drives the block identically
// and a port change breaks compilation once rather than silently in four
// files.

#pragma once

// The INCLUDER supplies its own verilated header. Every driver below is
// TEMPLATED on the model type, so the same driver runs the shipped block and
// the committed drain mutant -- which is what lets the mutant lane be a
// positive AND a negative control in one executable, instead of two files that
// can drift apart.

#include <cstdint>
#include <vector>

#include "zhao_sim.hpp"

namespace hist_test {

// Geometry at the shipped defaults. These are NOT read from the RTL; they are
// restated here so that a parameter change breaks a test rather than silently
// rescaling one.
constexpr int kLanes = 4;
constexpr int kEw = 32;
constexpr int kSubBits = 1;
constexpr int kBinw = 6;
constexpr int kAddrBins = 1 << kBinw;           // bins addressable per bank
constexpr int kScrubCycles = 1 << (kBinw + 1);  // the one-time post-reset walk

/** The bucket law, restated. Mirrors `bin_of` in the RTL. */
inline int bin_of(uint32_t v) {
  if (v == 0) return 0;
  int e = 31;
  while (e > 0 && ((v >> e) & 1u) == 0u) --e;
  if (e < kSubBits) return static_cast<int>(v);
  const uint32_t mant = (v >> (e - kSubBits)) & ((1u << kSubBits) - 1u);
  return ((e - kSubBits + 1) << kSubBits) + static_cast<int>(mant);
}

/** Bookkeeping only. See the header: this is not an oracle. */
struct Model {
  uint64_t bins[kAddrBins] = {0};
  uint64_t total = 0;

  void add(uint32_t err) {
    bins[bin_of(err)] += 1;
    total += 1;
  }
  void clear() {
    for (int i = 0; i < kAddrBins; ++i) bins[i] = 0;
    total = 0;
  }
};

/** One beat of up to kLanes events. */
struct Beat {
  uint8_t mask = 0;
  uint32_t err[kLanes] = {0, 0, 0, 0};
  uint16_t src = 0;
};

template <typename Top>
inline void drive_idle(Top& d) {
  d.ev_valid_i = 0;
  d.ev_lane_valid_i = 0;
  d.ev_src_id_i = 0;
  d.snapshot_i = 0;
  d.rd_valid_i = 0;
  d.rd_bin_i = 0;
}

/** Async reset, then hold the block until its one-time scrub is finished. */
template <typename Top>
inline void reset(Top& d) {
  d.rst_n = 0;
  drive_idle(d);
  for (int w = 0; w < kLanes; ++w) d.ev_err_i[w] = 0;
  d.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

/** Tick until the block leaves its scrub; returns the cycles it took. */
template <typename Top>
inline int wait_scrub(Top& d, int max_wait = 4 * kScrubCycles) {
  int n = 0;
  while (!d.ev_ready_o && n < max_wait) {
    zhao::tick(d);
    ++n;
  }
  return n;
}

template <typename Top>
inline void idle(Top& d, int cycles) {
  drive_idle(d);
  d.eval();
  for (int i = 0; i < cycles; ++i) zhao::tick(d);
}

/**
 * Offer one beat and hold it until accepted. Returns the number of cycles the
 * block REFUSED it (0 = accepted immediately), or -1 on a hang.
 */
template <typename Top>
inline int send_beat(Top& d, const Beat& b, int max_wait = 64) {
  d.ev_valid_i = 1;
  d.ev_lane_valid_i = b.mask;
  d.ev_src_id_i = b.src;
  for (int l = 0; l < kLanes; ++l) d.ev_err_i[l] = b.err[l];
  d.eval();
  int refused = 0;
  while (!d.ev_ready_o) {
    zhao::tick(d);
    if (++refused > max_wait) return -1;
  }
  zhao::tick(d);  // the accepting edge
  d.ev_valid_i = 0;
  d.ev_lane_valid_i = 0;
  d.eval();
  return refused;
}

/** What one host read observed, including the latency it was served at. */
struct ReadResult {
  uint32_t count = 0;
  bool ready = false;       // rd_ready_o when the request was offered
  bool early_valid = true;  // rd_data_valid_o one cycle after the request
  bool valid_at_2 = false;  // rd_data_valid_o two cycles after the request
  bool valid_at_3 = true;   // rd_data_valid_o three cycles after (must fall)
};

/**
 * One host read of the FROZEN bank. The block's read latency is fixed at two
 * clocks, so this does not poll -- it counts, and reports what it saw at each
 * of the three edges so a lane can assert the latency rather than absorb it.
 */
template <typename Top>
inline ReadResult read_bin(Top& d, int bin) {
  ReadResult r;
  d.rd_valid_i = 1;
  d.rd_bin_i = static_cast<uint8_t>(bin);
  d.eval();
  r.ready = d.rd_ready_o != 0;
  zhao::tick(d);  // T: the request is taken
  d.rd_valid_i = 0;
  d.eval();
  r.early_valid = d.rd_data_valid_o != 0;  // T+1: must still be low
  zhao::tick(d);
  r.valid_at_2 = d.rd_data_valid_o != 0;  // T+2: the answer
  r.count = static_cast<uint32_t>(d.rd_count_o);
  zhao::tick(d);
  r.valid_at_3 = d.rd_data_valid_o != 0;  // T+3: must have fallen
  return r;
}

/** Read every addressable bin of the frozen bank. */
template <typename Top>
inline std::vector<uint32_t> read_all(Top& d) {
  std::vector<uint32_t> v(kAddrBins, 0);
  for (int b = 0; b < kAddrBins; ++b) v[static_cast<size_t>(b)] = read_bin(d, b).count;
  return v;
}

/**
 * Pulse `snapshot_i` and tick until the swap retires. Returns the cycles the
 * drain took (law S3), or -1 on a hang.
 */
template <typename Top>
inline int snapshot(Top& d, int max_wait = 64) {
  const uint32_t before = static_cast<uint32_t>(d.snapshots_o);
  d.snapshot_i = 1;
  d.eval();
  zhao::tick(d);
  d.snapshot_i = 0;
  d.eval();
  int n = 0;
  while (static_cast<uint32_t>(d.snapshots_o) == before) {
    zhao::tick(d);
    if (++n > max_wait) return -1;
  }
  return n;
}

/** Every counter, in one struct, so a lane can difference two of them. */
struct Counters {
  uint32_t events = 0;
  uint32_t updates = 0;
  uint32_t stalls = 0;
  uint32_t bin_sat = 0;
  uint32_t fwd_hits = 0;
  uint32_t host_conflict = 0;
  uint32_t snapshots = 0;
  uint32_t frozen_write = 0;
};

template <typename Top>
inline Counters counters(const Top& d) {
  Counters c;
  c.events = static_cast<uint32_t>(d.events_o);
  c.updates = static_cast<uint32_t>(d.updates_o);
  c.stalls = static_cast<uint32_t>(d.stall_cycles_o);
  c.bin_sat = static_cast<uint32_t>(d.bin_sat_o);
  c.fwd_hits = static_cast<uint32_t>(d.fwd_hits_o);
  c.host_conflict = static_cast<uint32_t>(d.host_conflict_o);
  c.snapshots = static_cast<uint32_t>(d.snapshots_o);
  c.frozen_write = static_cast<uint32_t>(d.frozen_write_o);
  return c;
}

}  // namespace hist_test
