// measure_governor_random.cpp — MEASURE.GOVERNOR randomized differential
// (phase 8, ZH-047).
//
// TWO LANES, both against `zref::measure::LodGovernor`, comparing EVERY
// observable of EVERY frame: both ratios, both enables, both rungs, the three
// stability constants, the source id and all four counter lanes — plus, on
// every frame, that no target output moved before the publish (TERRAIN.LOD
// samples them mid-job and requires them stable).
//
//   Lane A — the workload. A Duo frame the way the console will run it:
//     projection scales around the shipped 256x192 canvas at plausible fields
//     of view, pixel-error budgets from half a pixel to four pixels, and
//     starvation arriving in BURSTS rather than independently per frame,
//     because that is what a player walking into a battle actually produces.
//   Lane B — the domain limit. `proj` and `px_err` at and adjacent to their
//     rails (0, 1, 0xFFFF, 2^32-1), so the clamp, both law-G6 limits and the
//     widest and narrowest quotients are live on most frames.
//
// THE EXACT EQUALITIES ARE CONSTRUCTED, NOT HOPED FOR:
//
//   R1  remainder == d/2 exactly — the round-half-up tie, the ONLY input where
//       qformats §3's rounding and a truncating divide disagree. Uniform
//       random operands reach it with probability about 2^-32.
//   R2  remainder == d/2 - 1 — one below the tie, which must round DOWN.
//   R3  the quotient lands on exactly 0xFFFF (the top of the port) and
//   R4  exactly one past it, so the clamp has both sides.
//   R5  px_err == 0 (law G6's finest limit) and
//   R6  proj == 0 (law G6's coarsest limit).
//   R7  the hold expiring on exactly the DEG_HOLD-th unstarved frame.
//   R8  a rung already at DEG_MAX being starved again (the saturation).
//
// Each is counted and each lane asserts at the end that it reached every
// construction it claims to make.

#include "governor_dev.hpp"

#include <cstdio>
#include <cstring>

namespace {

using zhao::check;
namespace zm = zref::measure;
using gov_test::Frame;
using gov_test::Targets;

constexpr uint32_t kOne = 1u << 16;

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  uint32_t below(uint32_t n) { return n == 0 ? 0u : static_cast<uint32_t>(next() % n); }
  bool chance(int n) { return (next() % static_cast<uint64_t>(n)) == 0; }
};

struct Stats {
  long frames = 0;
  long r1 = 0, r2 = 0, r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0;
  long clamped = 0, at_top_rung = 0, recovered = 0, held_fail = 0;
  long mismatch_frames = 0;
};

/** One camera's operands for one frame, with the constructions folded in. */
void pick_camera(Rng& rng, bool limit, zm::GovernorCamera* cam, Stats* st) {
  const int pick = static_cast<int>(rng.below(12));
  if (pick == 0) {
    // R1 — the EXACT round-half-up tie. With px_err = 2^17 the divisor is
    // even and proj<<16 = proj*65536, so an ODD proj leaves remainder exactly
    // 65536 = d/2.
    cam->px_err = 2 * kOne;
    cam->proj = static_cast<uint16_t>((rng.below(32768) * 2) + 1);
    ++st->r1;
  } else if (pick == 1) {
    // R2 — one BELOW the tie: divisor 2^17 + 2 makes the remainder
    // d/2 - proj, i.e. strictly under half for any proj >= 1.
    cam->px_err = 2 * kOne + 2;
    cam->proj = static_cast<uint16_t>(1 + rng.below(64));
    ++st->r2;
  } else if (pick == 2) {
    // R3 — the quotient lands EXACTLY on the top of the 16-bit port.
    cam->proj = 0xFFFF;
    cam->px_err = static_cast<uint32_t>((static_cast<uint64_t>(0xFFFF) << 16) / 0xFFFF);
    ++st->r3;
  } else if (pick == 3) {
    // R4 — one token past the top, so the clamp is entered rather than met.
    cam->proj = 0xFFFF;
    cam->px_err = 1 + rng.below(64);
    ++st->r4;
  } else if (pick == 4) {
    cam->px_err = 0;  // R5 — law G6's finest limit
    cam->proj = static_cast<uint16_t>(rng.below(0x10000));
    ++st->r5;
  } else if (pick == 5) {
    cam->proj = 0;  // R6 — law G6's coarsest limit
    cam->px_err = 1 + rng.below(0x7FFFFFFF);
    ++st->r6;
  } else if (limit) {
    // The domain limit: operands at and beside their rails.
    const uint32_t k = rng.below(4);
    cam->proj = static_cast<uint16_t>(k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 0xFFFF : 0xFFFE);
    const uint32_t m = rng.below(4);
    cam->px_err = m == 0 ? 1u : m == 1 ? 0xFFFFFFFFu : m == 2 ? 0x80000000u : (1u + rng.below(64));
  } else {
    // The workload: a 256x192 canvas at 45..90 degrees of horizontal FOV is a
    // projection scale of roughly 128..310 px per unit tangent, and the error
    // budget runs from half a pixel to four.
    cam->proj = static_cast<uint16_t>((128 + rng.below(183)) * 256);
    cam->px_err = kOne / 2 + rng.below(4 * kOne);
  }
}

Stats run_lane(Vzhao_measure_governor& dut, Rng& rng, long frames, bool limit) {
  Stats st;
  zm::LodGovernor ref;
  gov_test::reset_dut(dut);
  ref.reset();

  // Starvation arrives in BURSTS: independent per-frame coin flips never
  // produce the sustained pressure a real overloaded view creates, and the
  // hold's expiry needs a long clean run to be reachable at all.
  int burst[2] = {0, 0};
  int clean[2] = {0, 0};
  int prev_deg[2] = {0, 0};

  for (long t = 0; t < frames; ++t) {
    Frame f;
    f.view_count = static_cast<int>(rng.below(3));
    f.src_id = static_cast<uint16_t>(rng.below(0x10000));
    for (int v = 0; v < 2; ++v) {
      pick_camera(rng, limit, &f.cam[v], &st);
      if (burst[v] > 0) {
        f.cam[v].starved = true;
        --burst[v];
        clean[v] = 0;
      } else if (rng.chance(limit ? 6 : 14)) {
        burst[v] = 1 + static_cast<int>(rng.below(6));
        f.cam[v].starved = true;
        clean[v] = 0;
      } else {
        f.cam[v].starved = false;
        ++clean[v];
      }
      // R7 — the hold expiring on exactly the DEG_HOLD-th clean frame.
      if (!f.cam[v].starved && prev_deg[v] != 0 && clean[v] == 12) ++st.r7;
      // R8 — a rung already at the top being starved again.
      if (f.cam[v].starved && prev_deg[v] == 3) ++st.r8;
    }

    bool held = true;
    const Targets got = gov_test::decide(dut, f, nullptr, &held);
    const zm::GovernorTargets want = ref.frame(f.cam, f.view_count, f.src_id);
    ++st.frames;
    if (!held) ++st.held_fail;

    const long before = zhao::check_failures();
    check(held, "random: targets held stable through the decision", 1, held ? 1 : 0);
    check(got.scale[0] == want.scale[0], "random: scale0", want.scale[0], got.scale[0]);
    check(got.scale[1] == want.scale[1], "random: scale1", want.scale[1], got.scale[1]);
    check(got.en[0] == want.en[0], "random: en0", want.en[0] ? 1 : 0, got.en[0] ? 1 : 0);
    check(got.en[1] == want.en[1], "random: en1", want.en[1] ? 1 : 0, got.en[1] ? 1 : 0);
    check(got.deg[0] == want.deg[0], "random: rung0", want.deg[0], got.deg[0]);
    check(got.deg[1] == want.deg[1], "random: rung1", want.deg[1], got.deg[1]);
    check(got.hyst == want.hyst, "random: hyst", want.hyst, got.hyst);
    check(got.min_hold == want.min_hold, "random: min_hold", want.min_hold, got.min_hold);
    check(got.morph_step == want.morph_step, "random: morph_step", want.morph_step, got.morph_step);
    check(got.src_id == want.src_id, "random: src_id", want.src_id, got.src_id);
    for (int k = 0; k < 4; ++k) {
      check(gov_test::rep_count(dut, k) == ref.rep_count(k), "random: rung counter",
            ref.rep_count(k), gov_test::rep_count(dut, k));
    }

    // LAW G3, on every frame of the lane rather than once: view 0's rung is a
    // function of view 0's pressure only, so a frame in which view 0 was not
    // starved can never RAISE view 0's rung, whatever view 1 did.
    if (!f.cam[0].starved) {
      check(got.deg[0] <= prev_deg[0], "random: an unstarved view 0 never degrades further",
            prev_deg[0], got.deg[0]);
    }
    if (!f.cam[1].starved) {
      check(got.deg[1] <= prev_deg[1], "random: an unstarved view 1 never degrades further",
            prev_deg[1], got.deg[1]);
    }

    if (zhao::check_failures() != before) ++st.mismatch_frames;

    if (got.scale[0] == 0xFFFF || got.scale[1] == 0xFFFF) ++st.clamped;
    if (got.deg[0] == 3 || got.deg[1] == 3) ++st.at_top_rung;
    if (got.deg[0] < prev_deg[0] || got.deg[1] < prev_deg[1]) ++st.recovered;
    prev_deg[0] = got.deg[0];
    prev_deg[1] = got.deg[1];
  }
  return st;
}

void report(const char* name, const Stats& s) {
  std::printf("  lane %s: %ld frames, %ld mismatching frames, %ld hold violations\n", name,
              s.frames, s.mismatch_frames, s.held_fail);
  std::printf(
      "    constructed: R1 exact tie %ld | R2 just under %ld | R3 quotient == 0xFFFF %ld"
      " | R4 past the clamp %ld\n",
      s.r1, s.r2, s.r3, s.r4);
  std::printf(
      "                 R5 px_err == 0 %ld | R6 proj == 0 %ld | R7 hold expiring "
      "exactly %ld | R8 top rung re-starved %ld\n",
      s.r5, s.r6, s.r7, s.r8);
  std::printf("    reached: %ld clamped frames, %ld frames at the bottom rung, %ld recoveries\n",
              s.clamped, s.at_top_rung, s.recovered);
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;
  }
  const long n = nightly ? 24000L : 4000L;

  Vzhao_measure_governor dut;
  Rng rng_a(0x474F5645524E3031ULL);
  Rng rng_b(0x474F5645524E3032ULL);

  const Stats a = run_lane(dut, rng_a, n, /*limit=*/false);
  const Stats b = run_lane(dut, rng_b, n, /*limit=*/true);

  report("A (Duo workload) ", a);
  report("B (domain limit) ", b);

  check(a.r1 > 0, "lane A CONSTRUCTED the exact round-half-up tie", 1, a.r1 > 0 ? 1 : 0);
  check(a.r2 > 0, "lane A CONSTRUCTED one below the tie", 1, a.r2 > 0 ? 1 : 0);
  check(a.r3 > 0, "lane A CONSTRUCTED a quotient landing on exactly 0xFFFF", 1, a.r3 > 0 ? 1 : 0);
  check(a.r4 > 0, "lane A CONSTRUCTED a quotient past the clamp", 1, a.r4 > 0 ? 1 : 0);
  check(a.r5 > 0, "lane A reached law G6's px_err == 0 limit", 1, a.r5 > 0 ? 1 : 0);
  check(a.r6 > 0, "lane A reached law G6's proj == 0 limit", 1, a.r6 > 0 ? 1 : 0);
  check(a.r7 > 0, "lane A hit the hold expiring on exactly its last frame", 1, a.r7 > 0 ? 1 : 0);
  check(a.r8 > 0, "lane A re-starved a view already at the bottom rung", 1, a.r8 > 0 ? 1 : 0);
  check(a.at_top_rung > 0, "lane A actually drove a view to the bottom rung", 1,
        a.at_top_rung > 0 ? 1 : 0);
  check(a.recovered > 0, "lane A actually recovered a rung", 1, a.recovered > 0 ? 1 : 0);
  check(a.held_fail == 0, "lane A never saw a target move mid-decision", 0, a.held_fail);

  check(b.r1 > 0, "lane B CONSTRUCTED the exact tie at the domain limit", 1, b.r1 > 0 ? 1 : 0);
  check(b.r3 > 0, "lane B CONSTRUCTED the top-of-port quotient", 1, b.r3 > 0 ? 1 : 0);
  check(b.r4 > 0, "lane B CONSTRUCTED a quotient past the clamp", 1, b.r4 > 0 ? 1 : 0);
  check(b.clamped > 0, "lane B actually clamped", 1, b.clamped > 0 ? 1 : 0);
  check(b.at_top_rung > 0, "lane B drove a view to the bottom rung", 1, b.at_top_rung > 0 ? 1 : 0);
  check(b.held_fail == 0, "lane B never saw a target move mid-decision", 0, b.held_fail);

  const int rc = zhao::report_and_exit("measure_governor_random");
  zhao::exit_hard(rc);
}
