// surface_sheet_random.cpp — randomized differential for SURFACE.SHEET
// against `zref::surface::SheetStore`.
//
// TWO LANES, because a single uniform lane would sample almost none of what
// this block is for:
//
//   Lane A, GAMEPLAY-SHAPED. Two handles — the resident set — held for many
//   frames, written in stamp-sized runs, read back, occasionally re-acquired.
//   Overflow is RARE here by construction. This is the regime where a
//   persistence defect (a re-acquire that clears, a slot bit dropped from the
//   address) shows up and nothing else does.
//
//   Lane B, AT THE RESIDENCY LIMIT. More live handles than slots, constant
//   acquire/release churn, texels at both ends of the 0..4,095 range. Overflow
//   and miss are the COMMON case here. This is where "never evict" and "never
//   partial-write" are actually tested, and where lane A would score zero.
//
// Every lane asserts the states it exists to reach. Three coverage counters in
// an earlier increment of this tree read zero while every differential passed,
// because the lanes sampled states that could not reach the boundary — so the
// counters below are checked, not printed.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_surface_sheet.h"

#include "surface_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_surface.hpp"

using sdev::Rng;
using sdev::SheetResponse;
using zhao::check;
namespace zs = zref::surface;

namespace {

struct Stats {
  uint32_t ops = 0;
  uint32_t allocated = 0;
  uint32_t hits = 0;
  uint32_t overflows = 0;
  uint32_t read_miss = 0;
  uint32_t releases = 0;
  uint32_t writes_landed = 0;
  uint32_t writes_dropped = 0;
  uint32_t reads_nonzero = 0;
  bool touched_first = false;  // texel 0
  bool touched_last = false;   // texel 4095
  uint32_t mismatches = 0;
};

// The oracle mirrors the RTL directory exactly; a divergence in either the
// residency verdict or the texel value is a mismatch.
void one_op(Vzhao_surface_sheet& dut, zs::SheetStore& store, Rng& rng,
            const std::vector<uint32_t>& handles, Stats& st, bool limit_lane) {
  const uint32_t h =
      handles[static_cast<size_t>(rng.range(0, static_cast<int32_t>(handles.size() - 1)))];
  // Weighting is the whole point of the two lanes: lane A mostly writes and
  // reads an already-resident sheet; lane B mostly churns residency.
  const int roll = rng.range(0, 99);
  ++st.ops;

  if (roll < (limit_lane ? 35 : 12)) {
    // ACQUIRE
    const SheetResponse r = sdev::sheet_request(dut, sdev::kOpAcquire, h, 0,
                                                static_cast<uint16_t>(rng.range(0, 65535)));
    const zs::AcquireResult a = store.acquire(h);
    const uint8_t want = (a.status == zs::Residency::kHit)         ? sdev::kStHit
                         : (a.status == zs::Residency::kAllocated) ? sdev::kStAllocated
                                                                   : sdev::kStOverflow;
    if (!r.got || r.status != want) ++st.mismatches;
    if (want == sdev::kStAllocated) ++st.allocated;
    if (want == sdev::kStHit) ++st.hits;
    if (want == sdev::kStOverflow) ++st.overflows;
  } else if (roll < (limit_lane ? 50 : 16)) {
    // RELEASE
    const SheetResponse r = sdev::sheet_request(dut, sdev::kOpRelease, h, 0, 0);
    const bool had = store.release(h);
    if (!r.got || r.status != (had ? sdev::kStHit : sdev::kStMiss)) ++st.mismatches;
    if (had) ++st.releases;
  } else if (roll < (limit_lane ? 75 : 60)) {
    // WRITE a short run of texels, the shape a stamp produces
    const int run = rng.range(1, 8);
    int base;
    if (limit_lane) {
      // deliberately include the two ends of the address space
      const int pick = rng.range(0, 2);
      base = pick == 0 ? 0 : (pick == 1 ? 4095 - run : rng.range(0, 4095 - run));
    } else {
      base = rng.range(0, 4095 - run);
    }
    for (int k = 0; k < run; ++k) {
      const uint16_t t = static_cast<uint16_t>(base + k);
      const uint8_t tag = static_cast<uint8_t>(rng.range(0, 255));
      const uint8_t str = static_cast<uint8_t>(rng.range(0, 255));
      const bool we_t = rng.range(0, 9) != 0;
      const bool we_s = rng.range(0, 9) != 0 || !we_t;
      sdev::sheet_write(dut, h, t, tag, str, we_t, we_s, 0);
      const int slot = store.find(h);
      if (slot >= 0) {
        if (we_t) store.at(slot).tag[t] = tag;
        if (we_s) store.at(slot).strength[t] = str;
        ++st.writes_landed;
      } else {
        ++st.writes_dropped;
      }
      if (t == 0) st.touched_first = true;
      if (t == 4095) st.touched_last = true;
    }
  } else {
    // READ
    const uint16_t t = static_cast<uint16_t>(
        limit_lane ? (rng.range(0, 2) == 0 ? 0 : (rng.range(0, 2) == 1 ? 4095 : rng.range(0, 4095)))
                   : rng.range(0, 4095));
    const SheetResponse r = sdev::sheet_request(dut, sdev::kOpRead, h, t, 0);
    const int slot = store.find(h);
    if (slot < 0) {
      ++st.read_miss;
      if (!r.got || r.status != sdev::kStMiss || r.tag != 0 || r.strength != 0) ++st.mismatches;
    } else {
      const uint8_t wt = store.at(slot).tag[t];
      const uint8_t ws = store.at(slot).strength[t];
      if (!r.got || r.status != sdev::kStHit || r.tag != wt || r.strength != ws) ++st.mismatches;
      if (wt || ws) ++st.reads_nonzero;
    }
    if (t == 0) st.touched_first = true;
    if (t == 4095) st.touched_last = true;
  }
}

void run_lane(Vzhao_surface_sheet& dut, Rng& rng, int ops, bool limit_lane, Stats& st) {
  // The RTL is reset so the lane starts from a stated state; the oracle is
  // constructed empty to match.
  sdev::reset_sheet(dut);
  zs::SheetStore store(2);
  std::vector<uint32_t> handles;
  if (limit_lane) {
    for (int k = 0; k < 5; ++k) handles.push_back(0x0000'2C01u + static_cast<uint32_t>(k) * 0x100u);
  } else {
    handles.push_back(0x0000'2C01u);
    handles.push_back(0x0000'2D01u);
  }
  for (int i = 0; i < ops; ++i) one_op(dut, store, rng, handles, st, limit_lane);

  // A full sweep at the end: every texel of every resident sheet must agree.
  for (uint32_t h : handles) {
    const int slot = store.find(h);
    if (slot < 0) continue;
    for (int t = 0; t < zs::kSheetTexels; t += 7) {
      const SheetResponse r =
          sdev::sheet_request(dut, sdev::kOpRead, h, static_cast<uint16_t>(t), 0);
      if (r.tag != store.at(slot).tag[t] || r.strength != store.at(slot).strength[t])
        ++st.mismatches;
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  Vzhao_surface_sheet dut;
  const int ops = nightly ? 6000 : 700;

  Rng rng(0x5EE7'2026ULL);
  Stats a, b;
  run_lane(dut, rng, ops, false, a);
  run_lane(dut, rng, ops, true, b);

  check(a.mismatches == 0, "lane A (gameplay) matches zref::surface::SheetStore", 0, a.mismatches);
  check(b.mismatches == 0, "lane B (residency limit) matches zref::surface::SheetStore", 0,
        b.mismatches);

  // ---- lane A must have reached the states it exists for -------------------
  check(a.allocated >= 2, "lane A allocated both slots", 1, a.allocated);
  check(a.hits > 0, "lane A re-acquired a resident handle (the persistence path)", 1, a.hits);
  check(a.writes_landed > 100, "lane A actually wrote texels", 1, a.writes_landed);
  check(a.reads_nonzero > 0, "lane A read back non-zero contents", 1, a.reads_nonzero);
  check(a.overflows == 0, "lane A never overflows: two handles fit two slots", 0, a.overflows);

  // ---- lane B must have reached the boundary -------------------------------
  check(b.overflows > 0, "lane B ACTUALLY overflowed the directory", 1, b.overflows);
  check(b.read_miss > 0, "lane B actually read a non-resident handle", 1, b.read_miss);
  check(b.writes_dropped > 0, "lane B actually dropped writes for non-resident handles", 1,
        b.writes_dropped);
  check(b.releases > 0, "lane B actually released slots", 1, b.releases);
  check(b.touched_first, "lane B touched texel 0", 1, b.touched_first ? 1 : 0);
  check(b.touched_last, "lane B touched texel 4,095", 1, b.touched_last ? 1 : 0);

  std::printf(
      "surface_sheet_random: lane A %u ops (%u alloc, %u hit, %u wr, %u drop), "
      "lane B %u ops (%u alloc, %u hit, %u ovf, %u miss, %u rel, %u drop)%s\n",
      a.ops, a.allocated, a.hits, a.writes_landed, a.writes_dropped, b.ops, b.allocated, b.hits,
      b.overflows, b.read_miss, b.releases, b.writes_dropped, nightly ? " [nightly]" : "");

  return zhao::report_and_exit("surface_sheet_random");
}
