// terrain_lod_random.cpp — randomized differential for TERRAIN.LOD.
//
// Two lanes, because one uniform lane would test almost only the second:
//
//   Lane A, ISLAND-SHAPED and RUN AS A TRAJECTORY. Sixteen subpatch centres on
//   a real 64 m patch, deviations of centimetres to metres (what a 17×17/9×9
//   height mip actually produces), a camera walking toward and away from the
//   island, and a governor policy with real hysteresis, hold and morph step.
//   Crucially the history is FED BACK frame to frame, so the block is exercised
//   as the state machine it is rather than as a pure function evaluated at
//   random points. A LOD law's bugs live in its transitions.
//
//   Lane B, DOMAIN LIMIT. Coordinates over the whole word (so the squared
//   distance saturates the root's 64-bit input), deviations at the 24-bit rail,
//   scale and hysteresis at the 16-bit rail, morph step at the 17-bit rail.
//
// Each lane ASSERTS IT REACHED ITS INTERESTING STATES. A green lane that
// sampled nothing is how a flooring defect elsewhere in this tree survived
// 20,000 random triangles.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_lod.h"

#include "lod_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain_lod.hpp"

using lod_test::Dev;
using lod_test::kNSub;
using lod_test::LodJob;
using lod_test::LodOut;
using zhao::check;

namespace zt = zref::terrain;

namespace {

constexpr int32_t kOne = 1 << 16;

// Deterministic: same sequence every run, on every host. splitmix64.
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  int32_t s32() { return static_cast<int32_t>(static_cast<uint32_t>(next() >> 32)); }
  int32_t range(int32_t lo, int32_t hi) {
    const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
    return lo + static_cast<int32_t>(next() % span);
  }
};

struct Stats {
  uint32_t jobs = 0;
  uint32_t chose[4] = {0, 0, 0, 0};
  uint32_t coarsened = 0;  // a level committed upward
  uint32_t refined = 0;    // a level committed downward
  uint32_t mid_morph = 0;  // a decision left the factor strictly between 0 and 1
  uint32_t held_back = 0;  // the ladder wanted a change and the hold refused it
  uint32_t dsq_sat = 0;    // the squared distance saturated the root's input
};

void tally(const LodJob& job, const std::vector<LodOut>& got, Stats& st) {
  ++st.jobs;
  for (int c = 0; c < 2; ++c) {
    if (!job.cam[c].enabled) continue;
    for (int n = 0; n < kNSub; ++n) {
      if (zt::lod_dsq(job.sp[n], job.cam[c]) == UINT64_MAX) ++st.dsq_sat;
    }
  }
  for (size_t i = 0; i < got.size(); ++i) {
    if (got[i].surface != 0) continue;
    const int n = (got[i].oz / 8) * 4 + (got[i].ox / 8);
    st.chose[got[i].level & 3]++;
    if (got[i].level > job.sp[n].level) ++st.coarsened;
    if (got[i].level < job.sp[n].level) ++st.refined;
    if (got[i].morph > 0 && got[i].morph < 65536) ++st.mid_morph;
    // The hold refused a change when the plain band wanted one and nothing
    // moved. Computed from the ORACLE's own band, as instrumentation.
    zt::LodPolicy free_policy = job.policy;
    free_policy.min_hold = 0;
    const zt::LodDecision unheld = zt::lod_select(job.sp[n], job.cam, free_policy);
    if (unheld.level != got[i].level && job.sp[n].hold < job.policy.min_hold) ++st.held_back;
  }
}

void compare(const LodJob& job, const std::vector<LodOut>& got, const char* what,
             uint32_t* failed) {
  const std::vector<LodOut> want = lod_test::oracle(job);
  bool ok = got.size() == want.size();
  size_t bad = 0;
  for (size_t i = 0; ok && i < want.size(); ++i) {
    ok = got[i].ox == want[i].ox && got[i].oz == want[i].oz && got[i].level == want[i].level &&
         got[i].nz == want[i].nz && got[i].pz == want[i].pz && got[i].nx == want[i].nx &&
         got[i].px == want[i].px && got[i].morph == want[i].morph &&
         got[i].surface == want[i].surface && got[i].dual == want[i].dual &&
         got[i].src_id == want[i].src_id && got[i].hold == want[i].hold;
    if (!ok) bad = i;
  }
  // The two packets at the first disagreement, or a zeroed one when the block
  // emitted the wrong COUNT and there is no packet to name. Value-initialised
  // so no field is read before it is written (cppcheck uninitvar).
  LodOut w{};
  LodOut g{};
  if (bad < want.size()) w = want[bad];
  if (bad < got.size()) g = got[bad];
  check(ok, what, w.level, g.level);
  if (!ok && *failed < 4) {
    ++*failed;
    std::vector<uint8_t> bytes(64, 0);
    const int32_t words[8] = {job.sp[0].cx,
                              job.sp[0].cy,
                              job.sp[0].cz,
                              job.cam[0].ex,
                              job.cam[0].ey,
                              job.cam[0].ez,
                              static_cast<int32_t>(job.sp[0].dev[1]),
                              static_cast<int32_t>(job.policy.morph_step)};
    std::memcpy(bytes.data(), words, sizeof(words));
    char e[160];
    char a[160];
    std::snprintf(e, sizeof(e), "packet %u level %u morph %u hold %u", static_cast<unsigned>(bad),
                  static_cast<unsigned>(w.level), static_cast<unsigned>(w.morph),
                  static_cast<unsigned>(w.hold));
    std::snprintf(a, sizeof(a), "packet %u level %u morph %u hold %u", static_cast<unsigned>(bad),
                  static_cast<unsigned>(g.level), static_cast<unsigned>(g.morph),
                  static_cast<unsigned>(g.hold));
    zhao::save_failing_vector(what, bytes, e, a);
  }
}

/** Lane A: a real island patch, a walking camera, and history fed back. */
void lane_island(Dev& dev, Rng& rng, int runs, Stats& st, uint32_t* failed) {
  for (int r = 0; r < runs; ++r) {
    LodJob j;
    j.policy.hyst = static_cast<uint16_t>(rng.range(256, 448));  // 1.0 .. 1.75
    j.policy.min_hold = static_cast<uint8_t>(rng.range(0, 4));
    j.policy.morph_step = (rng.next() & 3) ? (kOne / rng.range(2, 8)) : 0;
    j.dual = ((rng.next() & 3) == 0);
    for (int e = 0; e < 4; ++e) j.edge[e] = static_cast<uint8_t>(rng.next() & 0xFF);

    // A 64 m patch: sixteen subpatch centres on a 4×4 grid of 16 m squares.
    const int32_t px = rng.range(-2048, 2048) * kOne;
    const int32_t pz = rng.range(-2048, 2048) * kOne;
    for (int n = 0; n < kNSub; ++n) {
      const int i = n & 3;
      const int k = n >> 2;
      j.sp[n].cx = px + (i * 16 + 8) * kOne;
      j.sp[n].cz = pz + (k * 16 + 8) * kOne;
      j.sp[n].cy = rng.range(-128, 128) * kOne;
      // A coarse height mip's deviation: centimetres at level 1 up to a few
      // metres at level 3, and rising with the level (which the LAW does not
      // assume, but which real mips do).
      j.sp[n].dev[1] = static_cast<uint32_t>(rng.range(kOne / 100, kOne));
      j.sp[n].dev[2] = static_cast<uint32_t>(rng.range(kOne / 4, 4 * kOne));
      j.sp[n].dev[3] = static_cast<uint32_t>(rng.range(kOne, 16 * kOne));
      // A RANDOM starting level, not zero. Starting every subpatch at the
      // finest level means the ladder only ever walks upward and the refine
      // branch is never reached — which a green lane would not have told us.
      j.sp[n].level = static_cast<int>(rng.next() & 3);
      j.sp[n].morph = 0;
      j.sp[n].hold = static_cast<uint8_t>(rng.range(0, 8));
      j.src[n] = static_cast<uint16_t>(rng.next() & 0xFFFF);
    }

    // The camera walks from far away to inside the patch and back out, which is
    // what makes both coarsen and refine transitions ordinary events.
    const int32_t start = rng.range(200, 4000);
    j.cam[0].enabled = true;
    j.cam[0].scale = static_cast<uint16_t>(rng.range(64, 2048));
    j.cam[1].enabled = ((rng.next() & 1) != 0);
    j.cam[1].scale = static_cast<uint16_t>(rng.range(64, 2048));

    const int frames = 12;
    for (int f = 0; f < frames; ++f) {
      // Quartering the distance each frame drives the camera from far outside
      // the island to right on top of it and back out again inside twelve
      // frames. A linear walk mostly stays far away, where every level passes
      // the ladder and the REFINE branch is never reached.
      const int32_t away =
          1 + ((f < frames / 2) ? (start >> (2 * f)) : (start >> (2 * (frames - 1 - f))));
      j.cam[0].ex = px;
      j.cam[0].ey = 0;
      j.cam[0].ez = pz - away * kOne;
      j.cam[1].ex = px + 32 * kOne;
      j.cam[1].ey = 0;
      j.cam[1].ez = pz - (away / 2) * kOne;

      const std::vector<LodOut> got = dev.run(j, static_cast<uint32_t>(rng.next()));
      compare(j, got, "terrain_lod_laneA", failed);
      tally(j, got, st);
      if (got.size() < (j.dual ? 32u : 16u)) break;
      // Feed the history back — the caller's job, and the only way a state
      // machine whose state lives outside it can be exercised at all.
      for (size_t i = 0; i < got.size(); ++i) {
        if (got[i].surface != 0) continue;
        const int n = (got[i].oz / 8) * 4 + (got[i].ox / 8);
        j.sp[n].level = got[i].level;
        j.sp[n].morph = static_cast<int32_t>(got[i].morph);
        j.sp[n].hold = got[i].hold;
      }
    }
  }
}

/** Lane B: the whole word on every field. */
void lane_limit(Dev& dev, Rng& rng, int runs, Stats& st, uint32_t* failed) {
  for (int r = 0; r < runs; ++r) {
    LodJob j;
    j.policy.hyst = static_cast<uint16_t>(rng.next() & 0xFFFF);
    j.policy.min_hold = static_cast<uint8_t>(rng.next() & 0xFF);
    j.policy.morph_step = static_cast<int32_t>(rng.next() & 0x1FFFF);
    j.dual = ((rng.next() & 1) != 0);
    for (int e = 0; e < 4; ++e) j.edge[e] = static_cast<uint8_t>(rng.next() & 0xFF);
    for (int c = 0; c < 2; ++c) {
      j.cam[c].ex = rng.s32();
      j.cam[c].ey = rng.s32();
      j.cam[c].ez = rng.s32();
      j.cam[c].scale = static_cast<uint16_t>(rng.next() & 0xFFFF);
      j.cam[c].enabled = ((rng.next() & 7) != 0);
    }
    // Half the runs sit at the WORD limit (where the squared distance saturates
    // the root's 64-bit input) and half sit close to the origin (where the
    // distance is small, the ladder rejects every coarse level and the refine
    // branch is reachable at all). A lane that only sampled the word limit
    // would have every level pass the ladder and would test one branch.
    const bool word = ((rng.next() & 1) != 0);
    const int32_t span = 1 << 16;
    for (int c = 0; c < 2; ++c) {
      if (!word) {
        j.cam[c].ex = rng.range(-span, span);
        j.cam[c].ey = rng.range(-span, span);
        j.cam[c].ez = rng.range(-span, span);
      }
    }
    for (int n = 0; n < kNSub; ++n) {
      j.sp[n].cx = word ? rng.s32() : rng.range(-span, span);
      j.sp[n].cy = word ? rng.s32() : rng.range(-span, span);
      j.sp[n].cz = word ? rng.s32() : rng.range(-span, span);
      j.sp[n].dev[1] = static_cast<uint32_t>(rng.next() & 0xFFFFFFu);
      j.sp[n].dev[2] = static_cast<uint32_t>(rng.next() & 0xFFFFFFu);
      j.sp[n].dev[3] = static_cast<uint32_t>(rng.next() & 0xFFFFFFu);
      j.sp[n].level = static_cast<int>(rng.next() & 3);
      // A morph of exactly zero a quarter of the time: the refine branch only
      // COMMITS a level when the factor has unwound to zero, so a lane whose
      // factor is uniformly random would never see a refine commit at all.
      j.sp[n].morph = ((rng.next() & 3) == 0) ? 0 : static_cast<int32_t>(rng.next() & 0x1FFFF);
      j.sp[n].hold = static_cast<uint8_t>(rng.next() & 0xFF);
      j.src[n] = static_cast<uint16_t>(rng.next() & 0xFFFF);
    }
    const std::vector<LodOut> got = dev.run(j, static_cast<uint32_t>(rng.next()));
    compare(j, got, "terrain_lod_laneB", failed);
    tally(j, got, st);
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;
  }
  const int runs_a = nightly ? 200 : 24;
  const int runs_b = nightly ? 3000 : 400;

  Vzhao_terrain_lod dut;
  Dev dev(dut);
  dev.reset();

  Rng rng(0x10D'5EED5ULL);
  Stats a{};
  Stats b{};
  uint32_t fa = 0;
  uint32_t fb = 0;
  lane_island(dev, rng, runs_a, a, &fa);
  dev.reset();
  lane_limit(dev, rng, runs_b, b, &fb);

  std::printf(
      "[terrain_lod_random] lane A: %u jobs, levels %u/%u/%u/%u, coarsen %u, refine %u, "
      "mid-morph %u, held-back %u\n",
      a.jobs, a.chose[0], a.chose[1], a.chose[2], a.chose[3], a.coarsened, a.refined, a.mid_morph,
      a.held_back);
  std::printf(
      "[terrain_lod_random] lane B: %u jobs, levels %u/%u/%u/%u, dsq-saturated %u, "
      "coarsen %u, refine %u\n",
      b.jobs, b.chose[0], b.chose[1], b.chose[2], b.chose[3], b.dsq_sat, b.coarsened, b.refined);

  // ---- each lane must have reached the states it exists to reach -----------
  for (int L = 0; L < 4; ++L) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "lane A chose level %d", L);
    check(a.chose[L] > 16, buf, 16, a.chose[L]);
  }
  check(a.coarsened > 16, "lane A committed coarsening transitions", 16, a.coarsened);
  check(a.refined > 16, "lane A committed refining transitions", 16, a.refined);
  check(a.mid_morph > 16, "lane A sat mid-geomorph", 16, a.mid_morph);
  check(a.held_back > 0, "lane A had a change refused by the minimum hold", 1, a.held_back);
  check(a.dsq_sat == 0, "lane A never saturates the squared distance (it is the real regime)", 0,
        a.dsq_sat);

  check(b.dsq_sat > 16, "lane B saturated the squared distance", 16, b.dsq_sat);
  check(b.chose[0] > 16, "lane B reached level 0", 16, b.chose[0]);
  check(b.chose[3] > 16, "lane B reached level 3", 16, b.chose[3]);
  check(b.refined > 16, "lane B committed refining transitions", 16, b.refined);

  return zhao::report_and_exit("terrain_lod_random");
}
