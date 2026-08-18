// terrain_lod_directed.cpp — TERRAIN.LOD against its oracle.
//
// THE ORACLE IS MOSTLY DEFINITION, NOT VIEW, and this file is written knowing
// that. `zref::terrain::lod_select` states a law nothing in this tree had
// stated before (charter §11.5 lists ingredients, §9 lists required stability
// properties, MEASURE.GOVERNOR's contract is a stub), so "RTL == oracle" here
// is a weaker claim than it is for TERRAIN.NORMALS, where the oracle was a view
// onto shipped shading.
//
// The cases below therefore do TWO things: they compare against the oracle, and
// they check the law's OWN properties by hand — the exact distance at which a
// level flips, that the hysteresis band really retains a level the plain ladder
// would move, that the minimum hold really refuses a change, that a geomorph
// transition ends on the level it was walking toward, and that the neighbour
// levels in the emitted packet are the ones TERRAIN.TESS needs to stitch
// against. A law that only its own oracle agrees with is not tested at all.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_terrain_lod.h"

#include "lod_dev.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_terrain_lod.hpp"
#include "zref/zref_trig.hpp"

using lod_test::Dev;
using lod_test::edge_lane;
using lod_test::kNSub;
using lod_test::LodJob;
using lod_test::LodOut;
using lod_test::plain_job;
using zhao::check;

namespace zt = zref::terrain;

namespace {

constexpr int32_t kOne = 1 << 16;

/** Compare a whole job's emission against the oracle, packet for packet. */
void expect(Dev& dev, const LodJob& job, const char* what, uint32_t stall_mask = 0) {
  const std::vector<LodOut> want = lod_test::oracle(job);
  const std::vector<LodOut> got = dev.run(job, stall_mask);
  check(got.size() == want.size(), what, want.size(), got.size());
  const size_t n = got.size() < want.size() ? got.size() : want.size();
  for (size_t i = 0; i < n; ++i) {
    const bool ok = got[i].ox == want[i].ox && got[i].oz == want[i].oz &&
                    got[i].level == want[i].level && got[i].nz == want[i].nz &&
                    got[i].pz == want[i].pz && got[i].nx == want[i].nx && got[i].px == want[i].px &&
                    got[i].morph == want[i].morph && got[i].surface == want[i].surface &&
                    got[i].dual == want[i].dual && got[i].src_id == want[i].src_id &&
                    got[i].hold == want[i].hold;
    if (!ok) {
      char buf[224];
      std::snprintf(buf, sizeof(buf),
                    "%s: packet %u — want (ox %u oz %u L%u n %u%u%u%u m %u s%u h%u), "
                    "got (ox %u oz %u L%u n %u%u%u%u m %u s%u h%u)",
                    what, static_cast<unsigned>(i), want[i].ox, want[i].oz, want[i].level,
                    want[i].nz, want[i].pz, want[i].nx, want[i].px, want[i].morph, want[i].surface,
                    want[i].hold, got[i].ox, got[i].oz, got[i].level, got[i].nz, got[i].pz,
                    got[i].nx, got[i].px, got[i].morph, got[i].surface, got[i].hold);
      check(false, buf, want[i].level, got[i].level);
      return;
    }
  }
  check(true, what, 0, 0);
}

/**
 * Run frames, feeding the history back, until the decision stops moving.
 *
 * THE LADDER WALKS ONE RUNG PER FRAME, BY CONSTRUCTION. A geomorph can only
 * blend a level with the next one, so a subpatch that wants to move two rungs
 * must pass through the one between — and the target is always the near edge of
 * the hysteresis band, never an overshoot. Anything that asks "where does this
 * subpatch end up" therefore has to iterate, exactly as the caller does.
 * Returns the number of frames the walk took.
 */
int settle(Dev& dev, LodJob& job, std::vector<LodOut>* last, int max_frames = 32) {
  int frames = 0;
  for (; frames < max_frames; ++frames) {
    const std::vector<LodOut> got = dev.run(job);
    if (got.empty()) break;
    bool moved = false;
    for (size_t i = 0; i < got.size(); ++i) {
      if (got[i].surface != 0) continue;
      const int n = (got[i].oz / 8) * 4 + (got[i].ox / 8);
      if (job.sp[n].level != got[i].level ||
          job.sp[n].morph != static_cast<int32_t>(got[i].morph)) {
        moved = true;
      }
      job.sp[n].level = got[i].level;
      job.sp[n].morph = static_cast<int32_t>(got[i].morph);
      job.sp[n].hold = got[i].hold;
    }
    if (last != nullptr) *last = got;
    if (!moved) break;
  }
  return frames;
}

/** Where one subpatch's decision comes to rest. */
uint8_t level_of(Dev& dev, LodJob job, const char* what) {
  std::vector<LodOut> got;
  settle(dev, job, &got);
  check(got.size() == 16, what, 16, got.size());
  return got.empty() ? 0xFF : got[0].level;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_terrain_lod dut;
  Dev dev(dut);
  dev.reset();

  // =========================================================================
  // 1. every level is reachable, and the ladder is the coarsest that passes
  // =========================================================================
  // scale = 256 (1.0), so the ladder is `dev ≤ distance` in fx16 raw. With the
  // eye 100 units away, a subpatch whose level-3 deviation is under 100 units
  // takes level 3; raising each deviation past the distance walks it down.
  {
    const int32_t d = 100 * kOne;
    for (int target = 0; target <= 3; ++target) {
      LodJob j = plain_job(d, 256);
      for (int n = 0; n < kNSub; ++n) {
        // dev[L] passes for L <= target and fails above it.
        j.sp[n].dev[1] =
            (target >= 1) ? static_cast<uint32_t>(d - 1) : static_cast<uint32_t>(d + 1);
        j.sp[n].dev[2] =
            (target >= 2) ? static_cast<uint32_t>(d - 1) : static_cast<uint32_t>(d + 1);
        j.sp[n].dev[3] =
            (target >= 3) ? static_cast<uint32_t>(d - 1) : static_cast<uint32_t>(d + 1);
      }
      char buf[64];
      std::snprintf(buf, sizeof(buf), "the ladder reaches level %d", target);
      LodJob walk = j;
      std::vector<LodOut> last;
      const int frames = settle(dev, walk, &last);
      check(!last.empty() && last[0].level == target, buf, static_cast<uint64_t>(target),
            last.empty() ? 0xFF : last[0].level);
      // One rung per frame, and the settle costs exactly that many frames plus
      // the one that proves it has stopped. A ladder that jumped straight to the
      // target would skip the geomorph between the levels it skipped.
      check(frames == target || (target == 0 && frames == 0), "the ladder walks ONE rung per frame",
            static_cast<uint64_t>(target), static_cast<uint64_t>(frames));
      expect(dev, j, buf);
    }

    // The ladder is COARSEST-that-passes, not first-that-fails: a non-monotonic
    // dev vector (level 2 fails, level 3 passes) must still give level 3.
    LodJob j = plain_job(d, 256);
    for (int n = 0; n < kNSub; ++n) {
      j.sp[n].dev[1] = static_cast<uint32_t>(d - 1);
      j.sp[n].dev[2] = static_cast<uint32_t>(d + 1);  // fails
      j.sp[n].dev[3] = static_cast<uint32_t>(d - 1);  // passes anyway
    }
    const uint8_t nonmono = level_of(dev, j, "non-monotonic dev");
    check(nonmono == 3, "the ladder takes the COARSEST level that passes, not the first that fails",
          3, nonmono);
  }

  // =========================================================================
  // 2. the flip point is EXACT, and it is at the inequality's own boundary
  // =========================================================================
  // `dev · scale ≤ distance · 256`. With scale = 256 that is `dev ≤ distance`,
  // and `distance` is the §7.2 FLOOR root of the squared distance — so the flip
  // is at the floored value and not at the real one. A rounding anywhere in
  // this path moves the flip by one raw unit, which is exactly what this checks.
  {
    const int32_t eye = 12345 * 7;  // an eye distance with no special structure
    zt::LodSubpatch probe;
    probe.cz = eye;
    zt::LodCamera cam;
    cam.ez = 0;
    cam.scale = 256;
    const uint32_t dist = zt::lod_dist(probe, cam);
    check(dist == static_cast<uint32_t>(
                      zref::isqrt_u64(static_cast<uint64_t>(eye) * static_cast<uint64_t>(eye))),
          "the distance IS the §7.2 floor root", dist, dist);

    for (int k = -1; k <= 1; ++k) {
      LodJob j = plain_job(0, 256);
      for (int n = 0; n < kNSub; ++n) {
        j.sp[n].cz = eye;
        j.sp[n].dev[1] = static_cast<uint32_t>(static_cast<int64_t>(dist) + k);
        j.sp[n].dev[2] = 0xFFFFFFu;
        j.sp[n].dev[3] = 0xFFFFFFu;
      }
      const uint8_t want = (k <= 0) ? 1 : 0;  // `<=` — equality PASSES
      char buf[80];
      std::snprintf(buf, sizeof(buf), "the flip point, dev = distance %+d", k);
      check(level_of(dev, j, buf) == want, buf, want, level_of(dev, j, buf));
      expect(dev, j, buf);
    }
  }

  // =========================================================================
  // 3. hysteresis: the band retains a level the plain ladder would move
  // =========================================================================
  // hyst = 512 (2.0) doubles the distance the relaxed ladder sees. A subpatch
  // sitting at level 2 whose strict ladder says 1 and whose relaxed ladder says
  // 2 is INSIDE the band, so it holds — and the same subpatch at level 3 is
  // above the band, so it refines to 2. That is the whole mechanism, and it is
  // checked as a difference from the un-hysteretic answer, not as a value.
  {
    const int32_t d = 100 * kOne;
    LodJob base = plain_job(d, 256);
    for (int n = 0; n < kNSub; ++n) {
      base.sp[n].dev[1] = static_cast<uint32_t>(d - 1);      // passes strict
      base.sp[n].dev[2] = static_cast<uint32_t>(d + d / 2);  // fails strict, passes relaxed
      base.sp[n].dev[3] = 0xFFFFFFu;                         // fails both
    }

    LodJob strict = base;
    check(level_of(dev, strict, "no hysteresis") == 1, "without hysteresis the ladder says level 1",
          1, level_of(dev, strict, "no hysteresis"));

    LodJob band = base;
    band.policy.hyst = 512;
    for (int n = 0; n < kNSub; ++n) band.sp[n].level = 2;
    check(level_of(dev, band, "inside the band") == 2,
          "a level INSIDE the hysteresis band is retained", 2,
          level_of(dev, band, "inside the band"));
    expect(dev, band, "inside the band");

    LodJob above = band;
    for (int n = 0; n < kNSub; ++n) above.sp[n].level = 3;
    check(level_of(dev, above, "above the band") == 2,
          "a level ABOVE the band refines to the band's near edge", 2,
          level_of(dev, above, "above the band"));

    LodJob below = band;
    for (int n = 0; n < kNSub; ++n) below.sp[n].level = 0;
    check(level_of(dev, below, "below the band") == 1,
          "a level BELOW the band coarsens to the band's near edge", 1,
          level_of(dev, below, "below the band"));
    expect(dev, below, "below the band");

    // hyst below unity must read as unity: a governor cannot make the band
    // negative and invert the retention rule.
    LodJob tiny = band;
    tiny.policy.hyst = 1;
    for (int n = 0; n < kNSub; ++n) tiny.sp[n].level = 2;
    check(level_of(dev, tiny, "hyst below unity") == 1, "a hysteresis below 1.0 reads as 1.0", 1,
          level_of(dev, tiny, "hyst below unity"));
  }

  // =========================================================================
  // 4. the minimum hold really refuses a change
  // =========================================================================
  {
    const int32_t d = 100 * kOne;
    LodJob j = plain_job(d, 256);
    j.policy.min_hold = 3;
    for (int n = 0; n < kNSub; ++n) {
      j.sp[n].dev[1] = static_cast<uint32_t>(d - 1);
      j.sp[n].dev[2] = 0xFFFFFFu;
      j.sp[n].dev[3] = 0xFFFFFFu;
      j.sp[n].level = 0;  // the ladder wants 1
      j.sp[n].hold = 0;
    }
    check(level_of(dev, j, "hold refuses") == 0, "a change is refused below the minimum hold", 0,
          level_of(dev, j, "hold refuses"));
    expect(dev, j, "hold refuses");

    for (int n = 0; n < kNSub; ++n) j.sp[n].hold = 2;
    check(level_of(dev, j, "hold still refuses") == 0, "and at hold = min_hold − 1 too", 0,
          level_of(dev, j, "hold still refuses"));

    for (int n = 0; n < kNSub; ++n) j.sp[n].hold = 3;
    check(level_of(dev, j, "hold permits") == 1, "and permitted at exactly the minimum hold", 1,
          level_of(dev, j, "hold permits"));
    expect(dev, j, "hold permits");

    // A refused change still ages the subpatch, or the hold could never expire.
    for (int n = 0; n < kNSub; ++n) j.sp[n].hold = 1;
    const std::vector<LodOut> got = dev.run(j);
    check(!got.empty() && got[0].hold == 2, "a refused change still ages the subpatch", 2,
          got.empty() ? 0 : got[0].hold);
  }

  // =========================================================================
  // 5. the geomorph walk, coarsen and refine, run to completion
  // =========================================================================
  // The walk is checked as a TRAJECTORY, by feeding each frame's output back in
  // as the next frame's history — which is exactly what the caller does, and
  // the only way a state machine with the history outside it can be tested.
  {
    const int32_t d = 100 * kOne;
    const int32_t step = kOne / 4;  // four frames to unity

    LodJob j = plain_job(d, 256);
    j.policy.morph_step = step;
    for (int n = 0; n < kNSub; ++n) {
      j.sp[n].dev[1] = static_cast<uint32_t>(d - 1);
      j.sp[n].dev[2] = 0xFFFFFFu;
      j.sp[n].dev[3] = 0xFFFFFFu;
      j.sp[n].level = 0;  // wants 1: coarsen
      j.sp[n].morph = 0;
    }
    int frames = 0;
    bool saw_partial = false;
    for (; frames < 16; ++frames) {
      const std::vector<LodOut> got = dev.run(j);
      check(got.size() == 16, "coarsen walk emits a full patch", 16, got.size());
      if (got.empty()) break;
      expect(dev, j, "coarsen walk");
      if (got[0].level == 0 && got[0].morph > 0) saw_partial = true;
      for (int n = 0; n < kNSub; ++n) {
        j.sp[n].level = got[n].level;
        j.sp[n].morph = static_cast<int32_t>(got[n].morph);
        j.sp[n].hold = got[n].hold;
      }
      if (got[0].level == 1) break;
    }
    check(saw_partial, "the coarsen walk really did pass through a partial morph", 1,
          saw_partial ? 1 : 0);
    check(frames == 3, "and it arrived after exactly 65536/step frames", 3,
          static_cast<uint64_t>(frames));
    check(j.sp[0].morph == 0, "and the factor resets when the level commits", 0,
          static_cast<uint64_t>(j.sp[0].morph));
    check(j.sp[0].hold == 0, "and the hold resets when the level commits", 0, j.sp[0].hold);

    // Refine: the finer level is adopted at once with the factor at unity, then
    // the factor walks back down. The geometry on screen must not jump at the
    // swap, which is what "morph = unity at the swap" means.
    LodJob r = plain_job(d, 256);
    r.policy.morph_step = step;
    for (int n = 0; n < kNSub; ++n) {
      r.sp[n].dev[1] = 0xFFFFFFu;  // nothing coarser passes: wants 0
      r.sp[n].dev[2] = 0xFFFFFFu;
      r.sp[n].dev[3] = 0xFFFFFFu;
      r.sp[n].level = 1;
      r.sp[n].morph = 0;
    }
    const std::vector<LodOut> first = dev.run(r);
    check(!first.empty() && first[0].level == 0, "refine adopts the finer level at once", 0,
          first.empty() ? 9 : first[0].level);
    check(!first.empty() && first[0].morph == 65536,
          "and it adopts it with the factor at unity, so nothing jumps", 65536,
          first.empty() ? 0 : first[0].morph);
    expect(dev, r, "refine adopts at once");

    for (int n = 0; n < kNSub; ++n) {
      r.sp[n].level = first[n].level;
      r.sp[n].morph = static_cast<int32_t>(first[n].morph);
      r.sp[n].hold = first[n].hold;
    }
    int down = 0;
    for (; down < 16; ++down) {
      const std::vector<LodOut> g = dev.run(r);
      if (g.empty()) break;
      for (int n = 0; n < kNSub; ++n) {
        r.sp[n].level = g[n].level;
        r.sp[n].morph = static_cast<int32_t>(g[n].morph);
        r.sp[n].hold = g[n].hold;
      }
      if (g[0].morph == 0) break;
    }
    check(r.sp[0].morph == 0 && r.sp[0].level == 0, "and the factor walks back down to zero", 0,
          static_cast<uint64_t>(r.sp[0].morph));

    // morph_step 0 SNAPS: a governor that turns geomorph off gets instant
    // changes, not a ladder that can never move.
    LodJob s = plain_job(d, 256);
    s.policy.morph_step = 0;
    for (int n = 0; n < kNSub; ++n) {
      s.sp[n].dev[1] = static_cast<uint32_t>(d - 1);
      s.sp[n].dev[2] = 0xFFFFFFu;
      s.sp[n].dev[3] = 0xFFFFFFu;
      s.sp[n].level = 0;
    }
    const std::vector<LodOut> snap = dev.run(s);
    check(!snap.empty() && snap[0].level == 1 && snap[0].morph == 0,
          "morph_step 0 snaps to the new level in one frame", 1, snap.empty() ? 9 : snap[0].level);
    expect(dev, s, "morph_step 0 snaps");
  }

  // =========================================================================
  // 6. the two cameras, and none
  // =========================================================================
  {
    const int32_t d = 100 * kOne;
    LodJob j = plain_job(d, 256);
    for (int n = 0; n < kNSub; ++n) {
      j.sp[n].dev[1] = static_cast<uint32_t>(d / 4);
      j.sp[n].dev[2] = static_cast<uint32_t>(d / 2);
      j.sp[n].dev[3] = static_cast<uint32_t>(d - 1);
    }
    const uint8_t alone = level_of(dev, j, "camera 0 alone");
    check(alone == 3, "the far camera alone comes to rest at the coarsest level", 3, alone);
    expect(dev, j, "camera 0 alone");

    // A SECOND camera much closer must pull the decision finer, not coarser.
    j.cam[1].ex = 0;
    j.cam[1].ey = 0;
    j.cam[1].ez = 10 * kOne;
    j.cam[1].scale = 256;
    j.cam[1].enabled = true;
    const uint8_t both = level_of(dev, j, "both cameras");
    check(both < alone, "the FINER camera wins — Duo fairness, charter §9",
          static_cast<uint64_t>(alone), both);
    expect(dev, j, "both cameras");

    // Camera 1 alone must give the same answer as the pair, since it is finer.
    j.cam[0].enabled = false;
    check(level_of(dev, j, "camera 1 alone") == both, "and the pair's answer IS the finer camera's",
          both, level_of(dev, j, "camera 1 alone"));

    // No camera at all: nothing is visible, so nothing changes.
    j.cam[1].enabled = false;
    for (int n = 0; n < kNSub; ++n) j.sp[n].level = 2;
    check(level_of(dev, j, "no camera") == 2, "with no camera enabled nothing changes", 2,
          level_of(dev, j, "no camera"));
    expect(dev, j, "no camera");
  }

  // =========================================================================
  // 7. the neighbour levels — every position, interior and border
  // =========================================================================
  // This is the reason the block buffers a whole patch. Each subpatch's packet
  // must carry its four NEIGHBOURS' levels, from inside the patch where they
  // exist and from the edge inputs where they do not, in TERRAIN.TESS's own
  // side order (−z, +z, −x, +x).
  {
    const int32_t d = 100 * kOne;
    LodJob j = plain_job(d, 256);
    j.edge[0] = 0xE4;  // −z lanes: 0,1,2,3
    j.edge[1] = 0x1B;  // +z lanes: 3,2,1,0
    j.edge[2] = 0x4E;  // −x lanes: 2,3,0,1
    j.edge[3] = 0xB1;  // +x lanes: 1,0,3,2
    // Give every subpatch a DIFFERENT level, so a neighbour lookup that reads
    // the wrong cell cannot accidentally be right.
    for (int n = 0; n < kNSub; ++n) {
      const int want_level = n & 3;
      j.sp[n].dev[1] = (want_level >= 1) ? static_cast<uint32_t>(d - 1) : 0xFFFFFFu;
      j.sp[n].dev[2] = (want_level >= 2) ? static_cast<uint32_t>(d - 1) : 0xFFFFFFu;
      j.sp[n].dev[3] = (want_level >= 3) ? static_cast<uint32_t>(d - 1) : 0xFFFFFFu;
    }
    const std::vector<LodOut> got = dev.run(j);
    check(got.size() == 16, "neighbour case emits a full patch", 16, got.size());
    expect(dev, j, "neighbour levels");
    for (size_t n = 0; n < got.size(); ++n) {
      const int i = static_cast<int>(n) & 3;
      const int jj = static_cast<int>(n) >> 2;
      check(got[n].ox == i * 8 && got[n].oz == jj * 8,
            "the subpatch origin follows the arrival order, x fastest",
            static_cast<uint64_t>(i * 8), got[n].ox);
      const uint8_t nz = (jj != 0) ? got[n - 4].level : edge_lane(j.edge[0], i);
      const uint8_t pz = (jj != 3) ? got[n + 4].level : edge_lane(j.edge[1], i);
      const uint8_t nx = (i != 0) ? got[n - 1].level : edge_lane(j.edge[2], jj);
      const uint8_t px = (i != 3) ? got[n + 1].level : edge_lane(j.edge[3], jj);
      check(got[n].nz == nz && got[n].pz == pz && got[n].nx == nx && got[n].px == px,
            "each packet carries its four neighbours' levels", nz, got[n].nz);
    }
  }

  // =========================================================================
  // 8. the dual page: thirty-two packets, top then underside, same level
  // =========================================================================
  {
    const int32_t d = 100 * kOne;
    LodJob j = plain_job(d, 256);
    j.dual = true;
    for (int n = 0; n < kNSub; ++n) {
      j.sp[n].dev[1] = static_cast<uint32_t>(d - 1);
      j.sp[n].dev[2] = static_cast<uint32_t>(d - 1);
      j.sp[n].dev[3] = 0xFFFFFFu;
    }
    const std::vector<LodOut> got = dev.run(j);
    check(got.size() == 32, "a dual page emits both surfaces", 32, got.size());
    expect(dev, j, "dual page");
    for (size_t n = 0; n + 1 < got.size(); n += 2) {
      check(got[n].surface == 0 && got[n + 1].surface == 1, "top first, then underside", 0,
            got[n].surface);
      check(got[n].level == got[n + 1].level,
            "the underside takes the top's level — equal on every edge, so no rim crack",
            got[n].level, got[n + 1].level);
      check(got[n].dual == 1 && got[n + 1].dual == 1, "and both carry the dual flag", 1,
            got[n].dual);
    }
  }

  // =========================================================================
  // 9. the counters
  // =========================================================================
  {
    const int32_t d = 100 * kOne;
    LodJob j = plain_job(d, 256);
    for (int n = 0; n < kNSub; ++n) {
      const int want_level = n & 3;
      j.sp[n].dev[1] = (want_level >= 1) ? static_cast<uint32_t>(d - 1) : 0xFFFFFFu;
      j.sp[n].dev[2] = (want_level >= 2) ? static_cast<uint32_t>(d - 1) : 0xFFFFFFu;
      j.sp[n].dev[3] = (want_level >= 3) ? static_cast<uint32_t>(d - 1) : 0xFFFFFFu;
    }
    // Walk the patch to its resting mix FIRST, then zero the counters and take
    // one more frame, so the counts describe a known level distribution.
    settle(dev, j, nullptr);
    dev.reset();
    const std::vector<LodOut> got = dev.run(j);
    check(got.size() == 16, "counter case emits a full patch", 16, got.size());
    check(dut.lod_rep_count0_o == 4 && dut.lod_rep_count1_o == 4 && dut.lod_rep_count2_o == 4 &&
              dut.lod_rep_count3_o == 4,
          "the representation counts split by level", 4, dut.lod_rep_count0_o);
    // 4·128 + 4·32 + 4·8 + 4·2 = 680 — an UPPER BOUND, since void cells and the
    // stitch annulus both emit fewer. The contract says so.
    check(dut.terrain_triangles_emitted_o == 680, "the predicted triangle count is the level sum",
          680, dut.terrain_triangles_emitted_o);
  }

  // =========================================================================
  // 10. backpressure, the domain limit, and idle
  // =========================================================================
  {
    const int32_t d = 100 * kOne;
    LodJob j = plain_job(d, 256);
    for (int n = 0; n < kNSub; ++n) {
      j.sp[n].dev[1] = static_cast<uint32_t>(d - 1);
      j.sp[n].dev[2] = static_cast<uint32_t>(d / 2);
      j.sp[n].dev[3] = 0xFFFFFFu;
      j.sp[n].cx = n * 4 * kOne;
    }
    const uint32_t masks[4] = {0u, 0xAAAAAAAAu, 0xFFFFFFFEu, 0x0F0F0F0Fu};
    const char* names[4] = {"no stall", "alternate stall", "stalled 31 in 32", "burst stall"};
    for (int m = 0; m < 4; ++m) {
      dev.reset();
      expect(dev, j, names[m], masks[m]);
      check(dut.idle_o == 1, "idle once the patch has drained", 1, dut.idle_o);
    }

    // The domain limit: coordinates at the whole word, so the squared distance
    // saturates the 64-bit root input. The reference forms the same sum in
    // s128 and saturates identically, so the two agree HERE rather than only
    // inside an envelope.
    dev.reset();
    LodJob big = plain_job(0, 0xFFFF);
    big.cam[0].ex = INT32_MIN;
    big.cam[0].ey = INT32_MIN;
    big.cam[0].ez = INT32_MIN;
    for (int n = 0; n < kNSub; ++n) {
      big.sp[n].cx = INT32_MAX;
      big.sp[n].cy = INT32_MAX;
      big.sp[n].cz = INT32_MAX;
      big.sp[n].dev[1] = 0xFFFFFFu;
      big.sp[n].dev[2] = 0xFFFFFFu;
      big.sp[n].dev[3] = 0xFFFFFFu;
    }
    zt::LodSubpatch probe = big.sp[0];
    check(zt::lod_dsq(probe, big.cam[0]) == UINT64_MAX,
          "the domain-limit case really does saturate the squared distance", 1, 1);
    expect(dev, big, "the domain limit");
  }

  return zhao::report_and_exit("terrain_lod_directed");
}
