// measure_governor_lod.cpp — MEASURE.GOVERNOR -> TERRAIN.LOD, BOTH BLOCKS
// REAL (phase 8, ZH-047 / ZH-050).
//
// WHY THIS FILE EXISTS. TERRAIN.LOD landed in phase 6 against a STUB contract
// for the governor, and it had to CHOOSE the policy shape itself. Its Notes
// law 8 says so in as many words: "The governor's per-camera policy is ONE
// ratio, not two numbers." Everything else in this increment can show that
// MEASURE.GOVERNOR is internally correct and agrees with its own oracle. Only
// this file can show that it agrees with WHAT THE LANDED BLOCK ASSUMED — and
// it does that by wiring the governor's published target ports straight into
// TERRAIN.LOD's target ports with NO ADAPTER. If that stops being true, this
// file stops compiling.
//
// THE THEOREM IT PROVES, on emitted geometry rather than in prose:
//
//   A. Charter §9's Duo fairness sentence, END TO END. View 1 is starved until
//      the governor drives it to its bottom rung. The ground TERRAIN.LOD emits
//      does not change by a single triangle, because charter §9's third rule
//      ("the two cameras combine by taking the FINER decision") means view 0
//      still holds it fine. That is "one player looking directly into a
//      volcano cannot make the other player's army disappear", measured in
//      triangles.
//   B. And the degradation is NOT vacuous: when BOTH views are starved the
//      emitted triangle count actually FALLS. Without B, A would also hold for
//      a governor whose degrade did nothing at all.
//
// The wiring itself is the third assertion: `wire_targets()` assigns every
// governor output to the LOD input the ledger pairs it with, by name, with no
// conversion. A width or type change on either side is a compile error here
// before it is a silent truncation in the machine.

#include "governor_dev.hpp"
#include "lod_dev.hpp"

#include <cstdio>

namespace {

using zhao::check;
namespace zm = zref::measure;
namespace zt = zref::terrain;
using gov_test::Frame;
using gov_test::Targets;

constexpr int32_t kOne = 1 << 16;

/**
 * THE SEAM, port for port. Every one of these is a direct assignment from a
 * MEASURE.GOVERNOR output to the TERRAIN.LOD input of the same meaning. There
 * is no scaling, no repacking and no reinterpretation anywhere in this
 * function, and that is the point of it.
 */
void wire_targets(const Vzhao_measure_governor& gov, Vzhao_terrain_lod& lod) {
  lod.cam0_scale_i = gov.cam0_scale_o;
  lod.cam1_scale_i = gov.cam1_scale_o;
  lod.cam0_en_i = gov.cam0_en_o;
  lod.cam1_en_i = gov.cam1_en_o;
  lod.hyst_i = gov.hyst_o;
  lod.min_hold_i = gov.min_hold_o;
  lod.morph_step_i = gov.morph_step_o;
}

/** A patch of sixteen subpatches with a spread of deviations, at `dist`. */
lod_test::LodJob make_patch(int32_t dist) {
  lod_test::LodJob j;
  j.cam[0].ex = 0;
  j.cam[0].ey = 0;
  j.cam[0].ez = 0;
  j.cam[0].enabled = true;
  j.cam[1] = j.cam[0];
  j.dual = false;
  for (int n = 0; n < lod_test::kNSub; ++n) {
    j.sp[n].cx = dist;
    j.sp[n].cy = 0;
    j.sp[n].cz = 0;
    // A spread, so a change in the ladder moves SOME subpatches and not others
    // — a scene where every subpatch flips together would hide a partial law.
    j.sp[n].dev[1] = static_cast<uint32_t>((dist / 512) * (1 + n));
    j.sp[n].dev[2] = static_cast<uint32_t>((dist / 256) * (1 + n));
    j.sp[n].dev[3] = static_cast<uint32_t>((dist / 96) * (1 + n));
    j.sp[n].level = 0;
    j.sp[n].morph = 0;
    j.sp[n].hold = 255;  // the hold is long expired, so a change may commit
    j.src[n] = static_cast<uint16_t>(0x600 + n);
  }
  return j;
}

/** Run one governor frame, wire its targets into LOD, and run one patch. */
uint32_t run_frame(Vzhao_measure_governor& gov, Vzhao_terrain_lod& lodd, lod_test::Dev& dev,
                   const Frame& f, lod_test::LodJob* job, Targets* out) {
  const Targets t = gov_test::decide(gov, f);
  if (out) *out = t;
  wire_targets(gov, lodd);
  // The job carries the same numbers the ports now hold — the driver writes
  // them from the job, and `wire_targets` has already asserted the ports
  // themselves connect. Reading them back off the DUT is what makes this the
  // GOVERNOR's policy rather than a hand-written one.
  job->cam[0].scale = t.scale[0];
  job->cam[1].scale = t.scale[1];
  job->cam[0].enabled = t.en[0];
  job->cam[1].enabled = t.en[1];
  job->policy.hyst = t.hyst;
  job->policy.min_hold = t.min_hold;
  job->policy.morph_step = static_cast<int32_t>(t.morph_step);

  const std::vector<lod_test::LodOut> emitted = dev.run(*job);
  uint32_t tris = 0;
  for (size_t k = 0; k < emitted.size(); ++k) tris += zt::lod_tris(emitted[k].level);
  // The decisions become next frame's history, which is what makes the
  // hysteresis and the hold mean anything across a sequence of frames.
  for (size_t k = 0; k < emitted.size() && k < lod_test::kNSub; ++k) {
    const int n = (emitted[k].oz / 8) * 4 + (emitted[k].ox / 8);
    job->sp[n].level = emitted[k].level;
    job->sp[n].morph = static_cast<int32_t>(emitted[k].morph);
    job->sp[n].hold = emitted[k].hold;
  }
  return tris;
}

Frame duo_frame(uint16_t proj, uint32_t px_err, bool s0, bool s1) {
  Frame f;
  f.cam[0].proj = proj;
  f.cam[0].px_err = px_err;
  f.cam[0].starved = s0;
  f.cam[1].proj = proj;
  f.cam[1].px_err = px_err;
  f.cam[1].starved = s1;
  f.view_count = 2;
  f.src_id = 0x600;
  return f;
}

}  // namespace

int main() {
  Vzhao_measure_governor gov;
  Vzhao_terrain_lod lodd;
  gov_test::reset_dut(gov);
  lod_test::Dev dev(lodd);
  dev.reset();

  // The shipped VIDEO_DUO canvas at a plausible field of view, and an error
  // budget of two pixels.
  const uint16_t proj = 56755;
  const uint32_t px_err = 2u * static_cast<uint32_t>(kOne);
  const int32_t dist = 220 * kOne;

  lod_test::LodJob job = make_patch(dist);
  Targets t;

  // Settle. This takes far longer than it looks: TERRAIN.LOD walks its ladder
  // ONE rung at a time, each rung costs a full geomorph (6 frames at
  // MORPH_STEP) plus the minimum hold (6 more) before the next change may
  // commit, and the patch's subpatches are spread across three rungs. Twelve
  // frames was tried first and was NOT enough -- the count was still drifting
  // when the volcano phase began, which showed up as a false failure of the
  // fairness check below. So the settle now runs long AND proves it settled.
  uint32_t base = 0;
  uint32_t prev_tris = 0;
  int stable_run = 0;
  for (int k = 0; k < 200; ++k) {
    base = run_frame(gov, lodd, dev, duo_frame(proj, px_err, false, false), &job, &t);
    stable_run = (base == prev_tris) ? (stable_run + 1) : 0;
    prev_tris = base;
  }
  check(stable_run >= 20, "composition: the scene really did reach a steady state", 20, stable_run);
  std::printf("  settled: scale0=%u scale1=%u -> %u triangles from the patch\n", t.scale[0],
              t.scale[1], base);
  check(base > 0, "composition: the patch actually emitted geometry", 1, base > 0 ? 1 : 0);
  check(t.deg[0] == 0 && t.deg[1] == 0, "composition: both views start undegraded", 0,
        t.deg[0] + t.deg[1]);

  // ---- A. THE VOLCANO, END TO END ----------------------------------------
  // View 1 stares into the volcano for long enough to reach the bottom rung.
  // The ground must not change by one triangle, because view 0 still needs it.
  uint32_t worst = base;
  for (int k = 0; k < 60; ++k) {
    const uint32_t tris = run_frame(gov, lodd, dev, duo_frame(proj, px_err, false, true), &job, &t);
    check(tris == base,
          "VOLCANO: view 1 degrading does not remove one triangle of the other player's ground",
          base, tris);
    if (tris < worst) worst = tris;
  }
  check(t.deg[1] == 3, "VOLCANO: and view 1 really did reach the bottom rung", 3, t.deg[1]);
  check(t.deg[0] == 0, "VOLCANO: while view 0 never left rung 0", 0, t.deg[0]);
  check(t.scale[1] == t.scale[0] / 8, "VOLCANO: view 1's policy is an eighth of view 0's",
        t.scale[0] / 8, t.scale[1]);
  std::printf(
      "  volcano: view1 rung %u (scale %u) vs view0 rung %u (scale %u) -> still %u "
      "triangles\n",
      t.deg[1], t.scale[1], t.deg[0], t.scale[0], worst);

  // ---- B. AND THE DEGRADE IS NOT VACUOUS ---------------------------------
  // Now starve BOTH. The ground must actually coarsen, or A above would hold
  // for a governor whose degrade did nothing at all.
  uint32_t both = base;
  for (int k = 0; k < 120; ++k) {
    both = run_frame(gov, lodd, dev, duo_frame(proj, px_err, true, true), &job, &t);
  }
  check(t.deg[0] == 3 && t.deg[1] == 3, "composition: both views at the bottom rung", 3, t.deg[0]);
  check(both < base, "composition: with BOTH views degraded the ground really does coarsen", base,
        both);
  std::printf("  both starved: scale0=%u scale1=%u -> %u triangles (was %u, %.1f%% of it)\n",
              t.scale[0], t.scale[1], both, base,
              100.0 * static_cast<double>(both) / static_cast<double>(base));

  // ---- C. AND IT COMES BACK -- BUT NOT TO THE SAME PLACE -----------------
  // DEG_HOLD is 12 frames per rung, so the governor alone needs 36 clean
  // frames, and only THEN does TERRAIN.LOD start walking its ladder back, one
  // rung per (geomorph + minimum hold). The recovery is the slowest thing in
  // this file by a wide margin, and that is the stability charter section 9
  // asked for rather than a defect.
  //
  // MEASURED, AND NOT WHAT WAS FIRST ASSERTED. The scene does NOT return to
  // the triangle count it started with, and it should not: TERRAIN.LOD's
  // hysteresis is a BAND, and a level inside the band is RETAINED. A patch
  // that arrives at the band from the coarse side therefore settles on a
  // different, coarser stable point than the same patch arriving from a cold
  // start -- which is the definition of hysteresis, and the whole reason
  // charter section 9 requires it ("no visible threshold flicker"). The first
  // version of this file asserted exact return and was WRONG: it asserted the
  // ABSENCE of the property the consumer's contract requires.
  //
  // What is asserted instead is the three things that are actually true:
  // the recovery is real, it never overshoots past the cold-start state, and
  // it is STABLE once reached.
  uint32_t back = both;
  uint32_t back_prev = 0;
  int back_stable = 0;
  for (int k = 0; k < 400; ++k) {
    back = run_frame(gov, lodd, dev, duo_frame(proj, px_err, false, false), &job, &t);
    back_stable = (back == back_prev) ? (back_stable + 1) : 0;
    back_prev = back;
  }
  check(t.deg[0] == 0 && t.deg[1] == 0, "composition: both views recover to rung 0", 0,
        t.deg[0] + t.deg[1]);
  check(back > both, "composition: the ground really does come back", both, back);
  check(back <= base, "composition: and never finer than the cold-start state", base, back);
  check(back_stable >= 50, "composition: the recovered state is STABLE, not drifting", 50,
        back_stable);
  std::printf(
      "  recovered: %u triangles (%.1f%% of the %u cold-start count, up from %u). The "
      "gap is TERRAIN.LOD's hysteresis band: a level inside the band is retained, so "
      "arriving from the coarse side settles coarser than a cold start. Stable for %d "
      "frames.\n",
      back, 100.0 * static_cast<double>(back) / static_cast<double>(base), base, both, back_stable);

  const int rc = zhao::report_and_exit("measure_governor_lod");
  zhao::exit_hard(rc);
}
