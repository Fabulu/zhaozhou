// terrain_velocity_random.cpp — TERRAIN.VELOCITY, randomized differential
// against the oracle, in TWO lanes with different jobs.
//
//   LANE A — GAMEPLAY-SHAPED. The wake workload the console is being built for
//     (untitled-game DESIGN.md: "deformation waves in your wake ... the live
//     terrain deformation the console is being built for"). One to four live
//     lanes, velocity magnitudes a real wave produces (metres per tick, not
//     random s32 noise), footprint coverage that looks like a moving band
//     rather than a coin flip. This lane exercises the ROUNDING: with words
//     this size the height16 rails are unreachable and every result is decided
//     by `(acc + 128) >> 8`.
//   LANE B — DOMAIN-LIMIT. Full s32 lane words, up to the §9.1 ceiling of 16
//     lanes, coverage a coin flip. This lane exercises the SATURATIONS: the
//     fx_add rail and the height16 rail, both constantly.
//
// EXACT-EQUALITY BOUNDARIES ARE CONSTRUCTED, NOT HOPED FOR. Uniform random
// input never lands on a chosen accumulator: lane A's magnitudes make the rails
// unreachable, and lane B's magnitudes make the sub-LSB and tie cases
// vanishingly rare. Every patch in both lanes therefore has a seeded row of
// constructed accumulators — the ties, the last-value-before-the-rail, the
// first-value-that-saturates, exact cancellation, and INT32_MAX/MIN — and the
// counters below assert that each class was REACHED. Four increments running
// have shipped coverage counters reading zero while every differential passed.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_velocity.h"

#include "velocity_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain_patch.hpp"
#include "zref/zref_terrain_velocity.hpp"

using zhao::check;
namespace zt = zref::terrain;

namespace {

struct Stats {
  uint32_t patches = 0;
  uint64_t vertices = 0;
  uint32_t add_sats = 0;
  uint32_t rescale_sats = 0;
  uint32_t moving = 0;
  uint32_t covered = 0;
  uint32_t uncovered = 0;
  uint32_t exact_tie = 0;       // acc + 128 == 0 (mod 256): the rounding tie
  uint32_t rail_exact = 0;      // the LAST accumulator that reaches a rail cleanly
  uint32_t rail_first_sat = 0;  // the FIRST accumulator that saturates it
  uint32_t exact_cancel = 0;    // non-zero lanes summing to exactly 0
  uint32_t zero_lane = 0;       // patches with no live field at all
  uint32_t all_miss = 0;        // vertices where every lane's footprint missed
  uint32_t mask_bits = 0;       // moving-mask bits set across the run
};

// The constructed accumulators, and what each one is for. Each is planted as a
// SINGLE covering lane so `acc` is exactly the word — no arithmetic between the
// intent and the boundary.
constexpr int32_t kSeeds[] = {
    0,          // the V2 not-moving word
    128,        // the +0.5 tie: rounds UP
    -128,       // the -0.5 tie: ALSO rounds up, to 0
    -129,       // one LSB below it
    127,        // one LSB below the +0.5 tie
    384,        // the 1.5 tie
    8388479,    // the LAST accumulator that reaches +32767 cleanly
    8388480,    // the FIRST that saturates the high rail
    -8388736,   // the LAST that reaches -32768 cleanly
    -8388737,   // the FIRST that saturates the low rail
    INT32_MAX,  // the fx16 ceiling
    INT32_MIN,  // the fx16 floor
};
constexpr int kSeedCount = static_cast<int>(sizeof(kSeeds) / sizeof(kSeeds[0]));

bool is_tie(int32_t acc) { return ((static_cast<int64_t>(acc) + 128) & 0xFF) == 0; }

/**
 * Build one patch's lane plane.
 *
 * `gameplay` picks lane A's shape: a moving band of coverage across the patch
 * (the wake) and metre-scale velocities. Otherwise lane B: full-range words and
 * coin-flip coverage.
 *
 * The last lattice ROW is always overwritten with the constructed seeds on lane
 * 0 (every other lane made to miss), so every patch in the run carries the
 * exact boundaries whatever the random draw did.
 */
vdev::LanePlane make_plane(vdev::Rng& rng, bool gameplay, int lanes, int band) {
  vdev::LanePlane p;
  p.resize(lanes);
  if (lanes == 0) return p;
  for (int j = 0; j < vdev::kLat; ++j) {
    for (int i = 0; i < vdev::kLat; ++i) {
      for (int k = 0; k < lanes; ++k) {
        const size_t idx = p.at(i, j, k);
        if (gameplay) {
          // A wake band: lane k covers a diagonal strip offset by k, which is
          // what a player leaves behind while the spell is up.
          const int d = ((i + j) - (band + 3 * k)) % vdev::kLat;
          const int dd = d < 0 ? d + vdev::kLat : d;
          p.covers[idx] = (dd < 6) ? 1 : 0;
          // metres/tick at fx16, plus a per-lane sign so overlapping waves can
          // cancel as well as reinforce
          const int32_t mag = rng.range(0, 3 << 17);
          p.velocity[idx] = (k & 1) != 0 ? -mag : mag;
        } else {
          p.covers[idx] = rng.chance(3) ? 0 : 1;
          p.velocity[idx] = static_cast<int32_t>(rng.next() & 0xFFFFFFFFULL);
        }
      }
    }
  }
  // ---- the constructed row ------------------------------------------------
  const int j = vdev::kLat - 1;
  for (int i = 0; i < vdev::kLat; ++i) {
    for (int k = 1; k < lanes; ++k) p.covers[p.at(i, j, k)] = 0;
    p.covers[p.at(i, j, 0)] = 1;
    p.velocity[p.at(i, j, 0)] = kSeeds[i % kSeedCount];
  }
  // ---- exact cancellation, only possible with two or more lanes -----------
  if (lanes >= 2) {
    const int jc = vdev::kLat - 2;
    for (int i = 0; i < vdev::kLat; ++i) {
      const int32_t v = 1 << (10 + (i % 12));
      p.velocity[p.at(i, jc, 0)] = v;
      p.velocity[p.at(i, jc, 1)] = -v;
      p.covers[p.at(i, jc, 0)] = 1;
      p.covers[p.at(i, jc, 1)] = 1;
      for (int k = 2; k < lanes; ++k) p.covers[p.at(i, jc, k)] = 0;
    }
  }
  return p;
}

void tally(const vdev::LanePlane& p, const vdev::SweepOut& ora, Stats& st) {
  ++st.patches;
  st.vertices += vdev::kVerts;
  if (p.lanes == 0) ++st.zero_lane;
  for (int bit = 0; bit < 16; ++bit)
    if ((ora.moving_mask >> bit) & 1u) ++st.mask_bits;
  for (int j = 0; j < vdev::kLat; ++j) {
    for (int i = 0; i < vdev::kLat; ++i) {
      const size_t li = static_cast<size_t>(j) * vdev::kLat + static_cast<size_t>(i);
      if (ora.moving[li] != 0) ++st.moving;
      if (ora.covered[li] != 0) {
        ++st.covered;
      } else {
        ++st.uncovered;
        ++st.all_miss;
      }
      // Recompute the accumulator to classify the boundary this vertex hit.
      int32_t acc = 0;
      bool any = false, nonzero_lane = false;
      for (int k = 0; k < p.lanes; ++k) {
        const size_t idx = p.at(i, j, k);
        if (p.covers[idx] == 0) continue;
        any = true;
        if (p.velocity[idx] != 0) nonzero_lane = true;
        acc = zref::fx_add(zref::fx16{acc}, zref::fx16{p.velocity[idx]}, nullptr).raw;
      }
      if (!any) continue;
      if (is_tie(acc)) ++st.exact_tie;
      if (acc == 8388479 || acc == -8388736) ++st.rail_exact;
      if (acc == 8388480 || acc == -8388737) ++st.rail_first_sat;
      if (acc == 0 && nonzero_lane) ++st.exact_cancel;
    }
  }
}

void run_lane(Vzhao_terrain_velocity& dut, vdev::Rng& rng, int n_patches, bool gameplay,
              Stats& st) {
  for (int t = 0; t < n_patches; ++t) {
    const int lanes = gameplay ? rng.range(0, 4) : rng.range(0, 16);
    const int band = rng.range(0, vdev::kLat - 1);
    const vdev::LanePlane p = make_plane(rng, gameplay, lanes, band);

    zref::SatLedger L;
    const vdev::SweepOut ora = vdev::oracle_sweep(p, static_cast<uint16_t>(t), &L);
    const int stall_lane = rng.chance(3) ? rng.range(2, 7) : 0;
    const int stall_sink = rng.chance(3) ? rng.range(2, 9) : 0;
    const vdev::SweepOut rtl = vdev::run_sweep(dut, p, static_cast<uint16_t>(0x200 + t),
                                               static_cast<uint16_t>(t), stall_lane, stall_sink);

    if (rtl.timed_out) {
      check(false, "the sweep completed", 0, 1);
      return;
    }
    int bad = 0, bad_mov = 0, bad_cov = 0, first = -1;
    for (int k = 0; k < vdev::kVerts; ++k) {
      if (rtl.velocity[static_cast<size_t>(k)] != ora.velocity[static_cast<size_t>(k)]) {
        ++bad;
        if (first < 0) first = k;
      }
      if (rtl.moving[static_cast<size_t>(k)] != ora.moving[static_cast<size_t>(k)]) ++bad_mov;
      if (rtl.covered[static_cast<size_t>(k)] != ora.covered[static_cast<size_t>(k)]) ++bad_cov;
    }
    if (bad != 0) {
      std::printf(
          "[terrain_velocity] patch %d (%s, %d lanes): %d word mismatches, first at %d "
          "rtl %d oracle %d\n",
          t, gameplay ? "gameplay" : "limit", p.lanes, bad, first,
          rtl.velocity[static_cast<size_t>(first)], ora.velocity[static_cast<size_t>(first)]);
      std::vector<uint8_t> vec;
      const size_t nlane = static_cast<size_t>(p.lanes < 1 ? 1 : p.lanes);
      for (size_t k = 0; k < nlane; ++k) {
        const size_t idx = (static_cast<size_t>(first)) * nlane + k;
        for (int b = 0; b < 4; ++b) vec.push_back(static_cast<uint8_t>(p.velocity[idx] >> (8 * b)));
        vec.push_back(p.covers[idx]);
      }
      zhao::save_failing_vector("terrain_velocity_random", vec,
                                std::to_string(ora.velocity[static_cast<size_t>(first)]),
                                std::to_string(rtl.velocity[static_cast<size_t>(first)]));
    }
    check(bad == 0, "every lattice word matches the oracle", 0, static_cast<uint64_t>(bad));
    check(bad_mov == 0, "every moving bit matches", 0, static_cast<uint64_t>(bad_mov));
    check(bad_cov == 0, "every covered bit matches", 0, static_cast<uint64_t>(bad_cov));
    check(rtl.moving_mask == ora.moving_mask, "the moving mask matches", ora.moving_mask,
          rtl.moving_mask);
    check(rtl.add_sats == L.add, "the add-saturation count is the SatLedger's", L.add,
          rtl.add_sats);
    check(rtl.rescale_sats == L.rescale, "the rescale-saturation count is the SatLedger's",
          L.rescale, rtl.rescale_sats);
    check(rtl.samples == static_cast<uint32_t>(vdev::kVerts), "1,089 samples were evaluated",
          vdev::kVerts, rtl.samples);
    check(rtl.done_pulsed, "patch_done_o pulsed", 1, rtl.done_pulsed ? 1 : 0);

    st.add_sats += rtl.add_sats;
    st.rescale_sats += rtl.rescale_sats;
    tally(p, ora, st);
  }
}

void report(const char* name, const Stats& s) {
  std::printf(
      "[terrain_velocity] lane %s: %u patches, %llu vertices, %u moving, %u covered, %u uncovered,"
      " %u add-sat, %u rescale-sat | CONSTRUCTED: %u ties, %u rail-exact, %u rail-first-sat,"
      " %u exact-cancel, %u zero-lane patches, %u mask bits\n",
      name, s.patches, static_cast<unsigned long long>(s.vertices), s.moving, s.covered,
      s.uncovered, s.add_sats, s.rescale_sats, s.exact_tie, s.rail_exact, s.rail_first_sat,
      s.exact_cancel, s.zero_lane, s.mask_bits);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  Vzhao_terrain_velocity dut;
  vdev::reset_dut(dut);

  const int n_game = nightly ? 400 : 40;
  const int n_limit = nightly ? 400 : 40;

  Stats a, b;
  vdev::Rng rng_a(0x5E10'0001ULL);
  vdev::Rng rng_b(0x5E10'0002ULL);
  run_lane(dut, rng_a, n_game, true, a);
  run_lane(dut, rng_b, n_limit, false, b);

  report("A (gameplay/wake)", a);
  report("B (domain limit) ", b);

  // ---- each lane must have REACHED what it exists for ---------------------
  check(a.exact_tie > 0, "lane A hit the rounding tie", 1, a.exact_tie > 0 ? 1 : 0);
  check(a.rail_exact > 0, "lane A hit the CONSTRUCTED last-clean-rail accumulator", 1,
        a.rail_exact > 0 ? 1 : 0);
  check(a.rail_first_sat > 0, "lane A hit the CONSTRUCTED first-saturating accumulator", 1,
        a.rail_first_sat > 0 ? 1 : 0);
  check(a.exact_cancel > 0, "lane A hit exact cancellation of non-zero lanes", 1,
        a.exact_cancel > 0 ? 1 : 0);
  check(a.uncovered > 0, "lane A produced V2 uncovered vertices", 1, a.uncovered > 0 ? 1 : 0);
  check(a.moving > 0, "lane A actually moved ground", 1, a.moving > 0 ? 1 : 0);
  check(a.zero_lane > 0, "lane A included patches with no live field at all", 1,
        a.zero_lane > 0 ? 1 : 0);
  check(a.mask_bits > 0, "lane A marked moving subpatches", 1, a.mask_bits > 0 ? 1 : 0);
  check(a.rescale_sats > 0, "lane A recorded rail saturations (from the constructed row)", 1,
        a.rescale_sats > 0 ? 1 : 0);

  check(b.exact_tie > 0, "lane B hit the rounding tie", 1, b.exact_tie > 0 ? 1 : 0);
  check(b.rail_first_sat > 0, "lane B hit the CONSTRUCTED first-saturating accumulator", 1,
        b.rail_first_sat > 0 ? 1 : 0);
  check(b.add_sats > 0, "lane B saturated the fx_add chain — the domain limit it exists for", 1,
        b.add_sats > 0 ? 1 : 0);
  check(b.rescale_sats > 0, "lane B saturated the height16 rail", 1, b.rescale_sats > 0 ? 1 : 0);
  check(b.uncovered > 0, "lane B produced uncovered vertices", 1, b.uncovered > 0 ? 1 : 0);

  const int rc = zhao::report_and_exit("terrain_velocity_random");
  zhao::exit_hard(rc);
}
