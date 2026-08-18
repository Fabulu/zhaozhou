// terrain_tess_random.cpp — randomized differential for TERRAIN.TESS.
//
// TWO LANES, because one uniform lane would test almost only the second and
// would pass while the block was useless for real terrain:
//
//   Lane A, LATTICE-SHAPED. An authored Island Patch: heights on the height16
//   grid (exactly `raw << 8`, qformats §9), a deep keel below, scattered void
//   cells the way a bitten island carries them, levels drawn from the whole
//   legal set with neighbours usually equal or one coarser — the shape a
//   projected-error LOD selector produces. This is the regime the §4.3 emit
//   order, the void rule and the annulus actually decide in.
//
//   Lane B, DOMAIN-LIMIT. Heights spread across the fx16 word so the geomorph
//   blend's fx_mul and its saturating add reach their rails, plus the whole
//   morph range and both surfaces. Nothing here is a plausible island; the
//   point is that the blend saturates exactly where the reference's does.
//
// Each lane ASSERTS it reached the states it exists for: lane A must sample
// void skips, the annulus, and the reject path, and must NEVER saturate a
// vertex; lane B must saturate. Without those a green run could mean the lane
// sampled nothing worth sampling, which is how a flooring defect elsewhere in
// this tree survived 20,000 random triangles.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_tess.h"

#include "tess_harness.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain_tess.hpp"

using zhao::check;
namespace zt = zref::terrain;
using tess_test::Driver;
using tess_test::make_lattice;

namespace {

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
  int32_t range(int32_t lo, int32_t hi) {
    const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
    return lo + static_cast<int32_t>(next() % span);
  }
  bool chance(int n) { return (next() % static_cast<uint64_t>(n)) == 0; }
};

struct Stats {
  uint32_t jobs = 0;
  uint32_t tris = 0;
  uint32_t stitched = 0;
  uint32_t rejected = 0;
  uint32_t void_skips = 0;  // run-cells the void rule removed
  uint32_t morphed = 0;     // jobs with a nonzero factor that moved a vertex
  uint32_t saturated = 0;   // the geomorph arithmetic recorded a clamp
  uint32_t nondual = 0;     // legacy single-surface pages
  uint32_t underside = 0;
  uint32_t empty = 0;
};

bool same(const zt::MeshTri& a, const zt::MeshTri& b) {
  return a.ax == b.ax && a.ay == b.ay && a.az == b.az && a.bx == b.bx && a.by == b.by &&
         a.bz == b.bz && a.cx == b.cx && a.cy == b.cy && a.cz == b.cz;
}

void run_lane(Driver& drv, Vzhao_terrain_tess& dut, Rng& rng, int jobs, bool lattice, Stats& st) {
  for (int n = 0; n < jobs; ++n) {
    // ---- the lattice ------------------------------------------------------
    const bool dual = lattice || !rng.chance(5);
    zt::ComposedLattice lat = make_lattice(dual);
    for (int j = 0; j < 33; ++j) {
      for (int i = 0; i < 33; ++i) {
        const size_t k = static_cast<size_t>(j) * 33 + static_cast<size_t>(i);
        if (lattice) {
          // an authored island: height16 relief over a deep keel, EXACT << 8
          lat.top[k] = static_cast<int32_t>(static_cast<int16_t>(rng.range(-2048, 8192))) << 8;
          if (dual)
            lat.bottom[k] = static_cast<int32_t>(static_cast<int16_t>(rng.range(-32000, -8192)))
                            << 8;
        } else {
          lat.top[k] = static_cast<int32_t>(static_cast<uint32_t>(rng.next() >> 32));
          if (dual) lat.bottom[k] = static_cast<int32_t>(static_cast<uint32_t>(rng.next() >> 32));
        }
      }
    }
    if (dual) {
      const int voids = lattice ? rng.range(0, 20) : rng.range(0, 6);
      for (int v = 0; v < voids; ++v) {
        const int ci = rng.range(0, 31), cj = rng.range(0, 31);
        lat.cell_state[static_cast<size_t>(cj) * 32 + static_cast<size_t>(ci)] =
            rng.chance(2) ? zt::kVoidAuthored : zt::kVoidBreached;
      }
    }

    // ---- the job ----------------------------------------------------------
    zt::SubpatchJob job;
    job.ox = rng.range(0, 3) * 8;
    job.oz = rng.range(0, 3) * 8;
    job.level = rng.range(0, zt::kMaxLevel);
    for (int k = 0; k < 4; ++k) {
      if (lattice) {
        // a projected-error selector produces neighbours equal or one apart far
        // more often than the extremes; the extremes still appear
        const int d = rng.chance(6) ? rng.range(0, 3) : (job.level + rng.range(-1, 1));
        job.nlevel[k] = d < 0 ? 0 : (d > zt::kMaxLevel ? zt::kMaxLevel : d);
      } else {
        job.nlevel[k] = rng.range(0, zt::kMaxLevel);
      }
    }
    job.surface = rng.chance(3) ? zt::Surface::kUnderside : zt::Surface::kTop;
    job.morph = rng.chance(3) ? 0 : rng.range(0, 65536);
    if (rng.chance(20)) job.morph = 65536;
    if (rng.chance(30)) job.morph = 100000;  // the clamp path

    zref::SatLedger L;
    const zt::TessResult want = zt::tessellate(lat, job, &L);
    if (L.total() > 0) ++st.saturated;
    bool rejected = false;
    const uint32_t stall = rng.chance(3) ? static_cast<uint32_t>(rng.next()) : 0u;
    const std::vector<zt::MeshTri> got = drv.run(lat, job, &rejected, stall);

    ++st.jobs;
    if (job.surface == zt::Surface::kUnderside) ++st.underside;
    bool stitched = false;
    for (int k = 0; k < 4; ++k)
      if (job.nlevel[k] > job.level) stitched = true;
    if (stitched) ++st.stitched;
    if (!dual) ++st.nondual;

    check(rejected == (want.verdict != zt::TessVerdict::kOk),
          lattice ? "lane A reject verdict matches the oracle"
                  : "lane B reject verdict matches the oracle",
          want.verdict != zt::TessVerdict::kOk ? 1 : 0, rejected ? 1 : 0);
    if (rejected) {
      ++st.rejected;
      check(got.empty(), "a rejected job emits nothing", 0, static_cast<uint32_t>(got.size()));
      continue;
    }
    check(got.size() == want.tris.size(),
          lattice ? "lane A triangle count matches the oracle"
                  : "lane B triangle count matches the oracle",
          static_cast<uint32_t>(want.tris.size()), static_cast<uint32_t>(got.size()));
    const size_t nmin = got.size() < want.tris.size() ? got.size() : want.tris.size();
    size_t bad = nmin;
    for (size_t i = 0; i < nmin; ++i) {
      if (!same(got[i], want.tris[i])) {
        bad = i;
        break;
      }
    }
    check(bad == nmin,
          lattice ? "lane A mesh matches the oracle triangle for triangle"
                  : "lane B mesh matches the oracle triangle for triangle",
          static_cast<uint32_t>(nmin), static_cast<uint32_t>(bad));

    st.tris += static_cast<uint32_t>(got.size());
    if (got.empty()) ++st.empty;

    // coverage bookkeeping
    const int s = 1 << job.level;
    const int nrun = 8 / s;
    if (!stitched && !(job.surface == zt::Surface::kUnderside && !dual)) {
      const size_t full = static_cast<size_t>(2 * nrun * nrun);
      if (got.size() < full) st.void_skips += static_cast<uint32_t>((full - got.size()) / 2);
    }
    if (job.morph != 0 && !got.empty()) {
      zt::SubpatchJob z = job;
      z.morph = 0;
      const zt::TessResult base = zt::tessellate(lat, z);
      if (base.tris.size() == got.size()) {
        for (size_t i = 0; i < got.size(); ++i) {
          if (!same(got[i], base.tris[i])) {
            ++st.morphed;
            break;
          }
        }
      }
    }
  }
  (void)dut;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  Vzhao_terrain_tess dut;
  Driver drv(dut);
  drv.reset();

  const int jobs = nightly ? 1200 : 400;
  Stats lat_st, word_st;

  Rng rng_a(0x7E55A1'0001ULL);
  run_lane(drv, dut, rng_a, jobs, true, lat_st);
  drv.reset();
  Rng rng_b(0x7E55A1'0002ULL);
  run_lane(drv, dut, rng_b, jobs, false, word_st);

  check(lat_st.stitched > 0, "lane A actually exercised the annulus", 1, lat_st.stitched);
  check(lat_st.void_skips > 0, "lane A actually sampled void run-cell skips", 1, lat_st.void_skips);
  check(lat_st.rejected > 0, "lane A actually took the coarsened-with-void reject path", 1,
        lat_st.rejected);
  check(lat_st.morphed > 0, "lane A actually moved vertices with geomorph", 1, lat_st.morphed);
  check(lat_st.underside > 0, "lane A actually tessellated undersides", 1, lat_st.underside);
  check(lat_st.saturated == 0, "lane A never saturates: real terrain does not rail", 0,
        lat_st.saturated);

  // Lane B's interesting state is NOT an output rail, and saying so is worth a
  // line: the geomorph blend is convex, so an emitted height always lies
  // between the vertex's own and its coarse target and can never itself rail.
  // The saturating path that DOES exist is inside `coarse_height`, when the two
  // coarse parents are a whole fx16 word apart and the half-difference leaves
  // the word. That is what the SatLedger records, and it is what lane B must
  // reach for its differential to mean anything.
  check(word_st.saturated > 0, "lane B actually reached the saturating coarse-height path", 1,
        word_st.saturated);
  check(word_st.stitched > 0, "lane B actually exercised the annulus", 1, word_st.stitched);
  check(word_st.morphed > 0, "lane B actually moved vertices with geomorph", 1, word_st.morphed);
  // The legacy single-surface page combined with a coarser neighbour is a real
  // defect class: the first version of this block forced the plain path for any
  // non-dual page and emitted the full grid where the annulus was required.
  check(word_st.nondual > 0, "lane B actually tessellated legacy single-surface pages", 1,
        word_st.nondual);

  std::printf(
      "terrain_tess_random: lane A %u jobs / %u triangles (stitched %u, rejected %u, "
      "void-skips %u, morphed %u, underside %u, saturated %u); "
      "lane B %u jobs / %u triangles (stitched %u, rejected %u, morphed %u, saturated %u, "
      "legacy %u)%s\n",
      lat_st.jobs, lat_st.tris, lat_st.stitched, lat_st.rejected, lat_st.void_skips, lat_st.morphed,
      lat_st.underside, lat_st.saturated, word_st.jobs, word_st.tris, word_st.stitched,
      word_st.rejected, word_st.morphed, word_st.saturated, word_st.nondual,
      nightly ? " [nightly]" : "");

  return zhao::report_and_exit("terrain_tess_random");
}
