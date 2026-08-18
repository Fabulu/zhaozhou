// raster_earlyz_random.cpp — randomized differential test for RASTER.EARLYZ
// against zref::EarlyZ (design/contracts/RASTER.EARLYZ.md, ledger ZH-059).
//
// Deterministic from fixed seeds (the PCG shape every other random lane in
// this tree uses). Two lanes, because the block has two very different
// interesting regimes and a single uniform stream would visit neither often:
//
//   Lane A — FREE TRAFFIC. Fragments with random addresses, random depths and
//     random STATE words, offered and drained through independently gated
//     ready/valid streams. Depths are drawn from a deliberately NARROW window
//     around the tile's clear depth, so `depth == floor` (the tie that must
//     fail) and `depth == floor + 1` (the one-LSB margin that must survive)
//     are common events rather than 1-in-16-million accidents. The lane
//     asserts its own coverage and FAILS if any of those buckets is empty.
//
//   Lane B — THE PREFILL DUTY CYCLE. What the block is actually for: sweep an
//     opaque depth-writing surface across every pixel of the tile, watch the
//     hierarchical floor rise, and then draw geometry behind and in front of
//     it. This is the lane where the floor moves at all; lane A's random
//     addresses almost never complete a 256-pixel cover.
//
// Every decision, every carried field, both counters, the bin mask and the
// floor are compared on every fragment of both lanes.

#include "raster_earlyz_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using zhao::check;
using zhao_raster::ez_describe;
using zhao_raster::ez_expect;
using zhao_raster::ez_same;
using zhao_raster::EzDecision;
using zhao_raster::EzDev;
using zhao_raster::EzFrag;
using zhao_raster::EzRun;
using zhao_raster::EzState;
using zhao_raster::kEzWords;

namespace {

uint32_t g_saved = 0;
uint32_t g_failures = 0;

// coverage counters the lanes assert on themselves
uint32_t g_tie = 0;         // depth exactly at the floor (must be rejected)
uint32_t g_just_above = 0;  // depth exactly one LSB above (must survive)
uint32_t g_rejected = 0;
uint32_t g_kept = 0;
uint32_t g_floor_rises = 0;
uint32_t g_qualify_blocked = 0;  // full covers by NON-qualifying fragments

uint32_t next(uint32_t* s) {
  *s = (*s) * 747796405u + 2891336453u;
  const uint32_t w = ((*s >> ((*s >> 28) + 4)) ^ *s) * 277803737u;
  return (w >> 22) ^ w;
}

/**
 * A random but LEGAL fragment state. The bits are drawn independently; the
 * reserved-free 32-bit encoding means every draw is a state the block must
 * handle, so nothing has to be filtered out.
 */
uint32_t random_state(uint32_t* rng) {
  const uint32_t r = next(rng);
  EzState s;
  s.z_test_en = (r >> 0) & 1u;
  s.z_write_dis = (r >> 1) & 1u;
  s.z_force_far = (r >> 2) & 1u;
  s.blend = static_cast<uint8_t>((r >> 3) & 3u);
  s.shade_mod = (r >> 5) & 1u;
  s.alpha_mod = (r >> 6) & 1u;
  s.atest_en = (r >> 7) & 1u;
  s.atest_ref = static_cast<uint8_t>(next(rng));
  s.sten_func = static_cast<uint8_t>((r >> 8) & 3u);
  s.sten_op = static_cast<uint8_t>((r >> 10) & 3u);
  s.tag_write_dis = (r >> 12) & 1u;
  s.tag_from_texel = (r >> 13) & 1u;
  s.tag_channel = static_cast<uint8_t>((r >> 14) & 3u);
  s.sten_mask = static_cast<uint8_t>(next(rng));
  return s.pack();
}

/** A state that is CERTAIN to qualify for the hierarchical-Z accumulator. */
uint32_t qualifying_state(bool test_on, bool force_far) {
  EzState s;
  s.z_test_en = test_on;
  s.z_force_far = force_far;
  return s.pack();
}

EzFrag mk(uint32_t* rng, uint8_t addr, uint32_t depth, uint32_t state) {
  EzFrag f;
  f.addr = addr;
  f.depth = depth & 0xFFFFFFu;
  f.state = state;
  f.src_id = static_cast<uint16_t>(next(rng));
  f.payload_lo = (static_cast<uint64_t>(next(rng)) << 32) | next(rng);
  f.payload_hi = next(rng) & 0xFFFFFFu;
  return f;
}

bool diff(EzDev* dev, zref::EarlyZ* ez, const std::vector<EzFrag>& frags, uint32_t in_seed,
          uint32_t cand_seed, const char* lane, uint32_t iter) {
  std::string err;
  const EzRun got = dev->feed(frags, in_seed, cand_seed, &err);
  bool ok = err.empty();
  if (!ok) {
    if (g_saved < 6) std::printf("  %s[%u]: protocol violation: %s\n", lane, iter, err.c_str());
    ++g_failures;
  }

  const std::vector<EzDecision> want = ez_expect(*ez, frags);
  for (size_t i = 0; i < want.size(); ++i) {
    if (want[i].keep)
      ++g_kept;
    else
      ++g_rejected;
    if (ez_same(want[i], got.out[i])) continue;
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      const std::string body = ez_describe(i, want[i], got.out[i]);
      std::printf("  %s[%u]: %s\n", lane, iter, body.c_str());
      zhao::save_failing_vector("raster_earlyz_random", zhao_raster::ez_serialize(frags),
                                "zref::EarlyZ", body);
      ++g_saved;
    }
  }

  if (got.z_floor != ez->floor() || got.bin_mask != ez->bin_mask() ||
      got.counter_rejects != ez->early_z_rejects() ||
      got.counter_covered != ez->covered_fragments()) {
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      std::printf("  %s[%u]: state diverged - floor %06X/%06X bins %02X/%02X rej %u/%u cov %u/%u\n",
                  lane, iter, ez->floor(), got.z_floor, ez->bin_mask(), got.bin_mask,
                  ez->early_z_rejects(), got.counter_rejects, ez->covered_fragments(),
                  got.counter_covered);
      ++g_saved;
    }
  }
  return ok;
}

// ------------------------------------------------------------------ lane A --
// Free traffic in a NARROW depth window around the floor, so the strict-test
// boundary is hit constantly.
void lane_a(EzDev* dev, int batches) {
  uint32_t rng = 0xC0FFEE01u;
  for (int b = 0; b < batches; ++b) {
    dev->reset();
    zref::EarlyZ ez;
    ez.reset();

    const uint32_t clear = (next(&rng) & 0xFFFFFFu);
    dev->tile_begin(clear);
    ez.tile_begin(clear);

    std::vector<EzFrag> frags;
    const int n = 24 + static_cast<int>(next(&rng) % 40u);
    for (int i = 0; i < n; ++i) {
      // The window: [floor - 2, floor + 5]. `floor` is the ORACLE's, which is
      // legitimate - the test is allowed to know where the interesting values
      // are, it just may not decide what the answer is.
      const uint32_t f = ez.floor();
      const int32_t off = static_cast<int32_t>(next(&rng) % 8u) - 2;
      int64_t d = static_cast<int64_t>(f) + off;
      if (d < 0) d = 0;
      if (d > 0xFFFFFF) d = 0xFFFFFF;
      const uint32_t depth = static_cast<uint32_t>(d);
      if (depth == f) ++g_tie;
      if (depth == f + 1u) ++g_just_above;
      // A sprinkling of wide-range depths so the bins are all visited.
      const uint32_t use = ((next(&rng) & 7u) == 0u) ? (next(&rng) & 0xFFFFFFu) : depth;
      frags.push_back(mk(&rng, static_cast<uint8_t>(next(&rng)), use, random_state(&rng)));
    }

    const uint32_t in_seed = (b & 1) ? (next(&rng) | 1u) : 0u;
    const uint32_t cand_seed = (b & 2) ? (next(&rng) | 1u) : 0u;
    diff(dev, &ez, frags, in_seed, cand_seed, "A", static_cast<uint32_t>(b));
  }
}

// ------------------------------------------------------------------ lane B --
// The prefill duty cycle: sweep the whole tile with a qualifying surface, then
// draw behind and in front of the floor it establishes.
void lane_b(EzDev* dev, int batches) {
  uint32_t rng = 0x5EED0B2Bu;
  for (int b = 0; b < batches; ++b) {
    dev->reset();
    zref::EarlyZ ez;
    ez.reset();

    const uint32_t clear = next(&rng) & 0x00FFFFu;
    dev->tile_begin(clear);
    ez.tile_begin(clear);

    std::vector<EzFrag> frags;

    // The sweep. Depths vary across the tile, so the floor must rise to the
    // MINIMUM and not to the first, the last or the mean.
    const uint32_t base = 0x400000u + (next(&rng) & 0x0FFFFFu);
    const bool force_far = (next(&rng) & 7u) == 0u;
    uint32_t sweep_min = 0xFFFFFFu;
    // A PCG permutation of the 256 addresses, so the completing pixel is not
    // always address 255.
    uint8_t order[kEzWords];
    for (int i = 0; i < kEzWords; ++i) order[i] = static_cast<uint8_t>(i);
    for (int i = kEzWords - 1; i > 0; --i) {
      const int j = static_cast<int>(next(&rng) % static_cast<uint32_t>(i + 1));
      const uint8_t t = order[i];
      order[i] = order[j];
      order[j] = t;
    }
    // Some batches leave ONE pixel out, which must leave the floor unmoved.
    const bool complete = (next(&rng) & 3u) != 0u;
    const int cover = complete ? kEzWords : kEzWords - 1;
    for (int i = 0; i < cover; ++i) {
      const uint32_t d = base + (next(&rng) & 0xFFFFu);
      const uint32_t contributed = force_far ? 0u : d;
      if (contributed < sweep_min) sweep_min = contributed;
      frags.push_back(mk(&rng, order[i], d, qualifying_state(true, force_far)));
    }
    if (complete) ++g_floor_rises;

    // Then a NON-qualifying full sweep, which must move nothing.
    if ((next(&rng) & 1u) != 0u) {
      EzState blocked;
      blocked.z_test_en = true;
      blocked.blend = zref::FragmentPipeline::kAdd;
      blocked.z_write_dis = true;
      for (int i = 0; i < kEzWords; ++i)
        frags.push_back(mk(&rng, order[i], 0xFF0000u, blocked.pack()));
      ++g_qualify_blocked;
    }

    // Geometry behind and in front of wherever the floor now is.
    for (int i = 0; i < 24; ++i) {
      const uint32_t f = complete ? sweep_min : clear;
      const int32_t off = static_cast<int32_t>(next(&rng) % 6u) - 2;
      int64_t d = static_cast<int64_t>(f) + off;
      if (d < 0) d = 0;
      if (d > 0xFFFFFF) d = 0xFFFFFF;
      frags.push_back(
          mk(&rng, static_cast<uint8_t>(next(&rng)), static_cast<uint32_t>(d), random_state(&rng)));
    }

    const uint32_t cand_seed = (b & 1) ? (next(&rng) | 1u) : 0u;
    diff(dev, &ez, frags, 0u, cand_seed, "B", static_cast<uint32_t>(b));
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  EzDev dev;
  const int a = nightly ? 6000 : 400;
  const int b = nightly ? 900 : 60;
  lane_a(&dev, a);
  lane_b(&dev, b);

  std::printf(
      "raster_earlyz_random lane A: %d batches; lane B: %d batches; %u kept / %u rejected; "
      "%u ties at the floor, %u one LSB above; %u floor rises, %u blocked full covers\n",
      a, b, g_kept, g_rejected, g_tie, g_just_above, g_floor_rises, g_qualify_blocked);

  check(g_failures == 0, "raster_earlyz_random: every decision matches zref::EarlyZ", 0,
        g_failures);

  // The lane asserts its own coverage: a differential test that never reached
  // the strict-test boundary or never moved the floor would pass while
  // proving nothing about either.
  check(g_tie > 0, "coverage: fragments landed EXACTLY at the floor (the tie that must fail)", 1,
        g_tie > 0 ? 1 : 0);
  check(g_just_above > 0, "coverage: fragments landed exactly ONE LSB above the floor", 1,
        g_just_above > 0 ? 1 : 0);
  check(g_rejected > 0 && g_kept > 0, "coverage: the run both kept and rejected fragments", 1,
        (g_rejected > 0 && g_kept > 0) ? 1 : 0);
  check(g_floor_rises > 0, "coverage: the hierarchical floor actually rose", 1,
        g_floor_rises > 0 ? 1 : 0);
  check(g_qualify_blocked > 0, "coverage: full covers by NON-qualifying fragments were exercised",
        1, g_qualify_blocked > 0 ? 1 : 0);

  return zhao::report_and_exit("raster_earlyz_random");
}
