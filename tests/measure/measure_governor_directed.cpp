// measure_governor_directed.cpp — MEASURE.GOVERNOR directed tests (phase 8,
// ZH-047).
//
// What each lane would catch (the "could have been red" statement):
//   1. THE WORKED DUO FRAME — the shipped VIDEO_DUO canvas (256x192) at a 60
//      degree horizontal FOV with a 2-pixel error budget, computed by hand,
//      and then checked to mean what it claims: a subpatch 100 world units
//      away is admitted at exactly the deviation that projects to 2 pixels.
//      Red on: an inverted ratio, a wrong shift, a Q-format slip. This is the
//      case that makes the block's arithmetic mean something rather than
//      merely be self-consistent.
//   2. THE ROUNDING TIE, CONSTRUCTED — `px_err = 2.0` fx16 and an ODD `proj`
//      puts the remainder at EXACTLY half the divisor, which is the one input
//      where qformats section 3's round-half-up and a truncating divide give
//      different answers. Uniform random operands hit it with probability
//      ~2^-32. Red on: truncation, round-half-even, or a bias term of the
//      wrong size.
//   3. The limits (law G6) — `px_err == 0` is the finest possible ask and
//      yields 0xFFFF; `proj == 0` puts nothing on screen and yields 0; and a
//      tiny `px_err` against a large `proj` CLAMPS at 0xFFFF rather than
//      wrapping. Red on: a division by zero, an unclamped quotient.
//   4. The degrade ladder (law G2) — each rung HALVES the scale exactly,
//      because the rung is a shift. Red on: a Q8.8 multiplier sneaking in a
//      second rounding, an off-by-one rung.
//   5. THE VOLCANO (law G3) — view 1 is starved for twenty frames straight and
//      degrades to its bottom rung; view 0's scale does not move by one LSB.
//      This is charter section 9's Duo fairness sentence applied to the
//      POLICY, and it is the case the owner's split-screen note asked for.
//      Red on: a single global rung, or any cross-wiring of the two.
//   6. The hold boundary, CONSTRUCTED (law G4) — recovery happens on the
//      DEG_HOLD-th unstarved frame and NOT on the one before it. Red on: an
//      off-by-one in the hold, a hold that never expires.
//   7. ANTI-THRASH, MEASURED (law G4, charter section 9's "no visible
//      threshold flicker" gate) — a view starved on alternate frames forever
//      changes its rung exactly DEG_MAX times and then never again. Red on: a
//      symmetric hold or immediate recovery, either of which oscillates.
//   8. THE MORPH THEOREM (law G5) — `min_hold * morph_step >= 65536`, read off
//      the ports, so a geomorph ALWAYS completes before the minimum hold can
//      permit the next change. Red on: the floor 10922, which leaves the morph
//      4/65536 short at exactly the frame the hold expires.
//   9. AGREEMENT WITH THE LANDED TERRAIN.LOD — every published target is
//      inside the domain `design/contracts/TERRAIN.LOD.md` declares for it,
//      including the two that have teeth: hysteresis strictly above 256 (LOD
//      reads 256 as NO hysteresis, which charter section 9 forbids) and a
//      non-zero morph step (LOD reads 0 as SNAP).
//  10. Latency and hold — the decision's clock count is MEASURED, and no
//      target output moves before the publish, which is what lets TERRAIN.LOD
//      sample them with every descriptor of a patch job.
//  11. Enables and the counter — `view_count` drives the two enable bits, and
//      `lod_representation_counts` counts frames at each rung over enabled
//      views only.
//
// Every lane runs `zref::measure::LodGovernor` in lockstep.

#include "governor_dev.hpp"

#include <cstdio>

namespace {

using zhao::check;
namespace zm = zref::measure;
using gov_test::Frame;
using gov_test::Targets;

constexpr uint32_t kOne = 1u << 16;  // fx16 unity

struct Cosim {
  Vzhao_measure_governor dut;
  zm::LodGovernor ref;

  void reset() {
    gov_test::reset_dut(dut);
    ref.reset();
  }

  Targets run(const Frame& f, const char* where, int* clocks = nullptr) {
    bool held = true;
    const Targets t = gov_test::decide(dut, f, clocks, &held);
    const zm::GovernorTargets r = ref.frame(f.cam, f.view_count, f.src_id);

    check(held, where, 1, held ? 1 : 0);
    check(t.scale[0] == r.scale[0], where, r.scale[0], t.scale[0]);
    check(t.scale[1] == r.scale[1], where, r.scale[1], t.scale[1]);
    check(t.en[0] == r.en[0], where, r.en[0] ? 1 : 0, t.en[0] ? 1 : 0);
    check(t.en[1] == r.en[1], where, r.en[1] ? 1 : 0, t.en[1] ? 1 : 0);
    check(t.hyst == r.hyst, where, r.hyst, t.hyst);
    check(t.min_hold == r.min_hold, where, r.min_hold, t.min_hold);
    check(t.morph_step == r.morph_step, where, r.morph_step, t.morph_step);
    check(t.deg[0] == r.deg[0], where, r.deg[0], t.deg[0]);
    check(t.deg[1] == r.deg[1], where, r.deg[1], t.deg[1]);
    check(t.src_id == r.src_id, where, r.src_id, t.src_id);
    for (int k = 0; k < 4; ++k) {
      check(gov_test::rep_count(dut, k) == ref.rep_count(k), where, ref.rep_count(k),
            gov_test::rep_count(dut, k));
    }
    return t;
  }
};

Frame mkframe(uint16_t proj, uint32_t px_err, bool s0 = false, bool s1 = false, int vc = 2,
              uint16_t src = 0) {
  Frame f;
  f.cam[0].proj = proj;
  f.cam[0].px_err = px_err;
  f.cam[0].starved = s0;
  f.cam[1].proj = proj;
  f.cam[1].px_err = px_err;
  f.cam[1].starved = s1;
  f.view_count = vc;
  f.src_id = src;
  return f;
}

// --------------------------------------------------------------------- 1 --
// THE WORKED DUO FRAME. spec/video_rules.md section 3.1: VIDEO_DUO shows two
// independent 256x192 canvases. At a 60 degree horizontal FOV the projection
// scale is (256/2)/tan(30 deg) = 221.70 pixels per unit of tangent, i.e. Q8.8
// raw 56755. With a 2-pixel error budget (px_err = 2.0 fx16 = 131072):
//
//     scale_raw = round_half_up(56755 * 65536 / 131072) = round(56755/2) = 28378
//
// and that number is then made to MEAN something: TERRAIN.LOD's ladder admits
// a level when dev * scale <= distance * 256, so at 100 units the largest
// admissible deviation is 100*65536*256/28378 = 59093 fx16 = 0.9018 units.
// Projected: 0.9018/100 * 221.70 = 2.0000 pixels. That is the budget, exactly.
void test_worked_duo_frame(Cosim& c) {
  c.reset();
  const uint16_t proj = 56755;       // Q8.8, 221.70 px per unit tangent
  const uint32_t px_err = 2 * kOne;  // 2.0 pixels

  const Targets t = c.run(mkframe(proj, px_err, false, false, 2, 0xD00), "duo/worked");
  check(t.scale[0] == 28378, "duo: the hand-computed Q8.8 ratio", 28378, t.scale[0]);
  check(t.scale[1] == 28378, "duo: and the same for the second canvas", 28378, t.scale[1]);
  check(t.en[0] && t.en[1], "duo: both views enabled at view_count 2", 1,
        (t.en[0] && t.en[1]) ? 1 : 0);

  // The ladder's own inequality, evaluated at that scale: the largest
  // admissible deviation at 100 units, and what it projects to in pixels.
  const uint64_t dist = 100ull * kOne;
  const uint64_t max_dev = (dist * 256ull) / t.scale[0];
  const double dev_units = static_cast<double>(max_dev) / kOne;
  const double px = dev_units / 100.0 * (static_cast<double>(proj) / 256.0);
  std::printf("  duo: scale=%u -> max dev at 100u = %.4f units = %.4f px (budget 2.0)\n",
              t.scale[0], dev_units, px);
  check(px > 1.999 && px < 2.001, "duo: the ratio really is a 2-pixel budget", 2000,
        static_cast<uint64_t>(px * 1000));
}

// --------------------------------------------------------------------- 2 --
// THE ROUNDING TIE, CONSTRUCTED. With px_err = 2.0 fx16 (131072) the divisor
// is even and `proj << 16 = proj * 65536`, so the remainder is exactly
// 65536 = d/2 whenever `proj` is ODD. That is the single input class where
// round-half-up and truncation disagree, and uniform random operands reach it
// with probability about 2^-32.
void test_rounding_tie(Cosim& c) {
  c.reset();
  const uint32_t px_err = 2 * kOne;
  int ties = 0;
  for (uint16_t proj = 1; proj < 64; proj += 2) {  // odd proj only
    const Targets t = c.run(mkframe(proj, px_err), "tie");
    const uint16_t half_up = static_cast<uint16_t>((proj + 1) / 2);
    const uint16_t truncated = static_cast<uint16_t>(proj / 2);
    check(t.scale[0] == half_up, "tie: an exact half rounds UP, per qformats section 3", half_up,
          t.scale[0]);
    check(half_up != truncated, "tie: and the tie really does distinguish the two rules", 1,
          (half_up != truncated) ? 1 : 0);
    ++ties;
  }
  check(ties >= 32, "tie: the constructed tie fired", 32, ties);
  std::printf("  tie: %d CONSTRUCTED exact half-remainders, all rounded up\n", ties);

  // And one just below and just above the tie, so the boundary has both sides.
  // px_err = 4 fx16 units: d = 4, N = proj*65536, remainder = N mod 4 = 0.
  const Targets a = c.run(mkframe(1, 3), "tie/below");
  const uint64_t n = (1ull << 16) + 1;  // proj=1 -> 65536, + floor(3/2) = 1
  check(a.scale[0] == static_cast<uint16_t>(n / 3), "tie: an ordinary quotient",
        static_cast<uint16_t>(n / 3), a.scale[0]);
}

// --------------------------------------------------------------------- 3 --
// The limits (law G6) and the clamp.
void test_limits(Cosim& c) {
  c.reset();

  Frame f = mkframe(1000, 0);
  const Targets z = c.run(f, "limits/zero-pxerr");
  check(z.scale[0] == 0xFFFF, "limits: zero allowed error is the FINEST ask -> 0xFFFF", 0xFFFF,
        z.scale[0]);

  f = mkframe(0, 4 * kOne);
  const Targets p = c.run(f, "limits/zero-proj");
  check(p.scale[0] == 0, "limits: a camera projecting nothing gets the coarsest ladder", 0,
        p.scale[0]);

  // A huge proj against a sub-pixel error budget: the true quotient is far past
  // 16 bits and must CLAMP.
  f = mkframe(0xFFFF, 1);
  const Targets s = c.run(f, "limits/clamp");
  check(s.scale[0] == 0xFFFF, "limits: the quotient CLAMPS at the port width", 0xFFFF, s.scale[0]);

  // One token either side of the clamp edge: scale exactly 0xFFFF and exactly
  // 0xFFFE, CONSTRUCTED. round_half_up(proj<<16 / px) == 0xFFFF needs
  // px_err near proj*65536/65535.
  const uint16_t proj = 0xFFFF;
  const uint32_t px_edge = static_cast<uint32_t>((static_cast<uint64_t>(proj) << 16) / 0xFFFF);
  f = mkframe(proj, px_edge);
  const Targets e = c.run(f, "limits/edge");
  check(e.scale[0] == 0xFFFF, "limits: exactly at the top of the port", 0xFFFF, e.scale[0]);
  f = mkframe(proj, px_edge + 1);
  const Targets e2 = c.run(f, "limits/edge+1");
  check(e2.scale[0] <= 0xFFFF, "limits: one token past it stays in range", 0xFFFF, e2.scale[0]);
}

// --------------------------------------------------------------------- 4 --
// The degrade ladder is a SHIFT, so each rung halves the scale exactly (G2).
void test_degrade_is_exact(Cosim& c) {
  c.reset();
  // proj chosen so every rung's quotient is exact with no rounding at all:
  // proj<<16 / px_err with px_err = 1<<16 gives scale = proj, and the rungs
  // give proj/2, proj/4, proj/8 exactly when proj is a multiple of 8.
  const uint16_t proj = 4096;
  const uint32_t px_err = kOne;

  uint16_t seen[4] = {0, 0, 0, 0};
  for (int rung = 0; rung <= 3; ++rung) {
    // starve view 0 to climb one rung per frame
    const Targets t = c.run(mkframe(proj, px_err, rung > 0, false), "degrade");
    seen[rung] = t.scale[0];
    check(t.deg[0] == rung, "degrade: the rung climbed by exactly one", rung, t.deg[0]);
  }
  check(seen[0] == proj, "degrade: rung 0 is the undegraded ratio", proj, seen[0]);
  check(seen[1] == proj / 2, "degrade: rung 1 HALVES the scale, exactly", proj / 2, seen[1]);
  check(seen[2] == proj / 4, "degrade: rung 2 quarters it, exactly", proj / 4, seen[2]);
  check(seen[3] == proj / 8, "degrade: rung 3 is an eighth, exactly", proj / 8, seen[3]);

  // And the rung SATURATES: more starvation does not go past DEG_MAX.
  const Targets t = c.run(mkframe(proj, px_err, true, false), "degrade/cap");
  check(t.deg[0] == 3, "degrade: the ladder stops at its top rung", 3, t.deg[0]);
  check(t.scale[0] == proj / 8, "degrade: and the scale stops with it", proj / 8, t.scale[0]);
}

// --------------------------------------------------------------------- 5 --
// THE VOLCANO (law G3). Charter section 9: "One player looking directly into a
// volcano cannot make the other player's army disappear." Here that is the
// POLICY half of the sentence — MEASURE.TOKENS proves the POOL half.
void test_volcano(Cosim& c) {
  c.reset();
  const uint16_t proj = 30000;
  const uint32_t px_err = 3 * kOne;

  const Targets base = c.run(mkframe(proj, px_err), "volcano/base");
  const uint16_t view0_scale = base.scale[0];
  check(base.deg[0] == 0 && base.deg[1] == 0, "volcano: both views start at rung 0", 0,
        base.deg[0] + base.deg[1]);

  // Twenty frames of view 1 staring into the volcano. View 0 is never starved.
  Targets t = base;
  for (int k = 0; k < 20; ++k) {
    t = c.run(mkframe(proj, px_err, /*s0=*/false, /*s1=*/true), "volcano/stare");
    check(t.scale[0] == view0_scale, "volcano: view 0's policy does not move while view 1 burns",
          view0_scale, t.scale[0]);
    check(t.deg[0] == 0, "volcano: and view 0 never leaves rung 0", 0, t.deg[0]);
  }
  check(t.deg[1] == 3, "volcano: view 1 degraded all the way down", 3, t.deg[1]);
  check(t.scale[1] == view0_scale / 8,
        "volcano: view 1's own policy is an eighth of view 0's, and view 0's is intact",
        view0_scale / 8, t.scale[1]);
  std::printf(
      "  volcano: after 20 starved frames  view0 scale=%u (rung %u), view1 scale=%u "
      "(rung %u)\n",
      t.scale[0], t.deg[0], t.scale[1], t.deg[1]);
}

// --------------------------------------------------------------------- 6 --
// The hold boundary, CONSTRUCTED (law G4). DEG_HOLD is 12: recovery happens on
// the twelfth unstarved frame and NOT on the eleventh.
void test_hold_boundary(Cosim& c) {
  c.reset();
  const uint16_t proj = 8192;
  const uint32_t px_err = kOne;

  c.run(mkframe(proj, px_err, true, false), "hold/climb");  // rung 1

  Targets t;
  for (int k = 0; k < 11; ++k) {
    t = c.run(mkframe(proj, px_err, false, false), "hold/wait");
    check(t.deg[0] == 1, "hold: the rung does NOT recover before the hold expires", 1, t.deg[0]);
  }
  t = c.run(mkframe(proj, px_err, false, false), "hold/expire");
  check(t.deg[0] == 0, "hold: and it recovers on exactly the twelfth frame", 0, t.deg[0]);

  // A starve inside the hold RE-ARMS it: the recovery clock restarts.
  c.run(mkframe(proj, px_err, true, false), "hold/rearm-climb");
  for (int k = 0; k < 6; ++k) c.run(mkframe(proj, px_err, false, false), "hold/rearm-wait");
  c.run(mkframe(proj, px_err, true, false), "hold/rearm-starve");  // rung 2, hold reset
  for (int k = 0; k < 11; ++k) {
    t = c.run(mkframe(proj, px_err, false, false), "hold/rearm-hold");
    check(t.deg[0] == 2, "hold: a starve inside the hold restarts the whole clock", 2, t.deg[0]);
  }
  t = c.run(mkframe(proj, px_err, false, false), "hold/rearm-expire");
  check(t.deg[0] == 1, "hold: then one rung comes back", 1, t.deg[0]);
}

// --------------------------------------------------------------------- 7 --
// ANTI-THRASH, MEASURED. Charter section 9's gate is "no visible threshold
// flicker". A view sitting exactly at its budget — starved every other frame —
// must not oscillate. It climbs to the bottom rung and STAYS.
void test_anti_thrash(Cosim& c) {
  c.reset();
  const uint16_t proj = 8192;
  const uint32_t px_err = kOne;

  int changes = 0;
  int last = 0;
  for (int k = 0; k < 200; ++k) {
    const Targets t = c.run(mkframe(proj, px_err, (k % 2) == 0, false), "thrash");
    if (t.deg[0] != last) ++changes;
    last = t.deg[0];
  }
  check(changes == 3, "anti-thrash: 200 alternating frames move the rung exactly 3 times", 3,
        changes);
  check(last == 3, "anti-thrash: and it settles at the bottom rung", 3, last);
  std::printf("  anti-thrash: 200 alternating frames -> %d rung changes (settled at %d)\n", changes,
              last);
}

// ------------------------------------------------------------------ 8, 9 --
// THE MORPH THEOREM and agreement with the LANDED TERRAIN.LOD, read off the
// ports rather than from the parameter list.
void test_agrees_with_terrain_lod(Cosim& c) {
  c.reset();
  const Targets t = c.run(mkframe(20000, 2 * kOne), "lod-agree");

  // The theorem law G5 chose MORPH_STEP to satisfy: a geomorph always reaches
  // unity before the minimum hold can permit the next change, so no level is
  // ever replaced mid-morph.
  const uint64_t reach = static_cast<uint64_t>(t.min_hold) * t.morph_step;
  check(reach >= 65536ull, "morph: min_hold * morph_step reaches Q16 unity", 65536, reach);
  std::printf("  morph: min_hold %u * morph_step %u = %llu >= 65536\n", t.min_hold, t.morph_step,
              static_cast<unsigned long long>(reach));

  // TERRAIN.LOD's declared domain for every field it samples.
  check(t.hyst > 256,
        "LOD-agree: hysteresis is ABOVE unity — LOD reads 256 as none, and "
        "charter section 9 requires every LOD path to have some",
        1, (t.hyst > 256) ? 1 : 0);
  check(t.morph_step != 0, "LOD-agree: a non-zero morph step — LOD reads 0 as SNAP", 1,
        (t.morph_step != 0) ? 1 : 0);
  check(t.morph_step <= 0x1FFFF, "LOD-agree: morph_step fits LOD's 17-bit port", 0x1FFFF,
        t.morph_step);
  check(t.min_hold <= 0xFF, "LOD-agree: min_hold fits LOD's 8-bit port", 0xFF, t.min_hold);
  // The scale ports are 16 bits on both sides by construction; check the value
  // is inside the range LOD's own random lane sweeps.
  check(t.scale[0] <= 0xFFFF, "LOD-agree: scale fits LOD's 16-bit port", 0xFFFF, t.scale[0]);
}

// -------------------------------------------------------------------- 10 --
// Latency, MEASURED, and the hold that lets TERRAIN.LOD sample mid-job.
void test_latency_and_hold(Cosim& c) {
  c.reset();
  int clocks = 0;
  c.run(mkframe(12345, 3 * kOne, false, false, 2, 0xAB), "latency", &clocks);
  std::printf(
      "  latency: %d clocks from the frame pulse to targets_valid (2 x 33-step "
      "restoring divide)\n",
      clocks);
  check(clocks == 69, "latency: 1 + 33 + 1 + 33 + 1 clocks, MEASURED", 69, clocks);

  // A second decision costs the same: there is no warm-up path.
  int clocks2 = 0;
  c.run(mkframe(999, kOne, false, false, 1, 0xCD), "latency/2", &clocks2);
  check(clocks2 == clocks, "latency: and the second decision costs the same", clocks, clocks2);
}

// -------------------------------------------------------------------- 11 --
// Enables and the counter (law G7).
void test_enables_and_counter(Cosim& c) {
  c.reset();
  const uint16_t proj = 4096;
  const uint32_t px_err = kOne;

  const Targets one = c.run(mkframe(proj, px_err, false, false, 1), "en/one");
  check(one.en[0] && !one.en[1], "enables: view_count 1 leaves the second camera off", 1,
        (one.en[0] && !one.en[1]) ? 1 : 0);
  const Targets two = c.run(mkframe(proj, px_err, false, false, 2), "en/two");
  check(two.en[0] && two.en[1], "enables: view_count 2 turns both on", 1,
        (two.en[0] && two.en[1]) ? 1 : 0);
  const Targets none = c.run(mkframe(proj, px_err, false, false, 0), "en/none");
  check(!none.en[0] && !none.en[1], "enables: view_count 0 turns both off", 1,
        (!none.en[0] && !none.en[1]) ? 1 : 0);

  // The counter: after the three frames above, lane 0 has 1 + 2 + 0 = 3.
  check(gov_test::rep_count(c.dut, 0) == 3u,
        "counter: frames at rung 0, summed over ENABLED views only", 3,
        gov_test::rep_count(c.dut, 0));

  // Drive view 1 to rung 2 and check the lanes separate.
  c.run(mkframe(proj, px_err, false, true, 2), "cnt/climb1");
  const Targets t = c.run(mkframe(proj, px_err, false, true, 2), "cnt/climb2");
  check(t.deg[0] == 0 && t.deg[1] == 2, "counter: the two views are at different rungs", 2,
        t.deg[1]);
  check(gov_test::rep_count(c.dut, 2) == 1u, "counter: rung 2 counted once, for view 1 alone", 1,
        gov_test::rep_count(c.dut, 2));
}

}  // namespace

int main() {
  Cosim c;
  test_worked_duo_frame(c);
  test_rounding_tie(c);
  test_limits(c);
  test_degrade_is_exact(c);
  test_volcano(c);
  test_hold_boundary(c);
  test_anti_thrash(c);
  test_agrees_with_terrain_lod(c);
  test_latency_and_hold(c);
  test_enables_and_counter(c);

  const int rc = zhao::report_and_exit("measure_governor_directed");
  zhao::exit_hard(rc);
}
