// manafold_qa_p12.cpp -- PASS 12 QA: the checks the shipped gates cannot make.
//
// Committed rather than improvised, per CLAUDE.md ("a probe that does this was
// written once and thrown away, so its numbers are unreproducible -- commit the
// probe"). Five checks, each aimed at a gap found by reading the shipped
// gates rather than at a hypothesis they already hold:
//
//  Q1 THE CORPSE ACROSS EVERY DEFORM LANE.  manafold_probe.cpp's death gate
//     calls `deformation_sample()`, which is LANE 0 ONLY (zref_creature.hpp:
//     "Lane 0 is the historic single channel"). Pass 12 then added lanes 1..4
//     and wired the span stretch onto them, written by Rig::write() on EVERY
//     key of EVERY clip. So "the deform reaches bit-zero and stays there" is
//     proven for one of five lanes. This reads `deformation_frame()`, which
//     resolves all five, over the eternal rest of both deaths.
//
//  Q2 THE EYE TRAVEL CHANNEL'S DRIVE.  `apply_eye_travel` is called by nothing
//     but the probe. This walks the SHIPPED bank and reports, per clip, how
//     far the eye-travel carrier bones actually rotate -- measured through
//     decode_pose, the renderer's own call, on the posed eye vertex.
//
//  Q3 ROOT CONTINUITY.  Nothing else bounds the per-key root step, so a
//     one-key teleport passes every clearance and contact check. PASS 13 gave
//     it a DECLARED PER-CLIP CEILING and a failable leg (--fail-rootstep). Reports the largest
//     single-key root displacement per clip and where it lands -- AND the LOOP
//     SEAM, last key back to key 0, which every interior-only walk misses and
//     which the site plays on every repeat. The by-eye review found the corpses
//     standing back up there; this is that fault as a number.
//
//  Q4 DEATH LOOP SEAMS. Both death clips must hold the final corpse instead of
//     presenting a half-frame resurrection toward key 0.
//
//  Q5 FALL IS A ONE-SHOT. Pass 17 changes slot 9 from a root-delta-protected
//     loop into a true hold-last action. The final presentation frame (f339,
//     key 169 sub 1) must preserve f338's authored root, quats, local
//     translations, scale and every deform lane. --fail-fall-wrap restores the
//     old root-only wrap and proves the gate sees the pose reset.
//
//  Q6 TRICK PLANTED 360 (version 18, Owner Direction 19 SS9). The spin is
//     EXTRACTED, never read back from its own progress table: the shipped
//     slot-13 root quaternion is divided by the same builder's output with the
//     spin OFF (the no-spin control), and the relative rotation is unwrapped
//     key by key. Four failable categories, each with its own control:
//       Q6a REVOLUTION  a pure world-vertical yaw; zero through the pause; ONE
//           monotone turn to a declared small overshoot; ONE reversal; exact
//           identity (1000 per-mille) before the righting; a bounded step.
//           --fail-trick-spin-gain (two revolutions, 2000 per-mille)
//       Q6b C2 JOINS    the discrete acceleration AT each join (motion start,
//           overshoot peak, correction end) is small against its segments' own
//           peak acceleration, for the progress AND for the compensating root XZ.
//           --fail-trick-spin-ease (C1-only smoothstep segments)
//       Q6c SUPPORT     carrier B's posed support centroid stays where the
//           no-spin clip has it, key by key through the whole plant.
//           --fail-trick-spin-pivot (turn about the root, no compensation)
//       Q6d PLANT PIN   (integration) the NO-SPIN contact patch stays where it
//           touched down through the WHOLE contact window: the balance wobble
//           sways the body about a fixed support instead of skating it. With
//           Q6c (spin vs no-spin) this bounds the shipped support's absolute
//           XZ drift by the sum of the two bounds, which is also reported.
//           --fail-trick-plant-pin (the Wave-F height-only pin: ~178 mm)
//
//  Q7 FLIGHT ONE CLOCK (version 18, Owner Direction 19 SS7). On the shipped
//     slot-22 root: exactly the declared number of height maxima, the declared
//     amplitude, and a loop seam whose step/acceleration/jerk are no larger than
//     the clip's own interior maxima. --fail-flight-seam stretches the clock's
//     denominator so the loop does not close.
//
// Build:
//   g++ -O2 -std=c++17 -Ireference/include -Iruntime/include -Itests/render \
//       -Ireference/src tools/reel/manafold_qa_p12.cpp -o manafold-qa-p12.exe
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <vector>
#include <cmath>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"
#include "render_helpers.hpp"
#include "zrender/internal.hpp"

namespace zc = zref::creature;
#include "manafold.h"

namespace {

bool sample_zero(const zc::DeformSample& d) { return d.flatten == 0 && d.spread == 0; }

// ---- VERSION 18 WAVE F bounds (declared, each with its reason) -------------
// Q6a: the extracted relative rotation is quantised by quat16 (1/16384): about
// 0.02 per-mille of a turn per key. 1 per-mille (0.36 deg) is 50x that, and
// 5x below the smallest overshoot the gate accepts.
constexpr double kSpinTolPm = 1.0;
// The owner asked for a SLIGHT overshoot: at least visible (5 per-mille = 1.8
// deg) and never a second partial turn (150 per-mille = 54 deg).
constexpr double kSpinOvershootMinPm = 5.0, kSpinOvershootMaxPm = 150.0;
// Largest turn per KEY (two presentation frames). 90 per-mille = 32 deg/key,
// 16 deg per displayed frame: above that a 240p turn strobes instead of turning.
constexpr double kSpinMaxStepPm = 90.0;
// Off-axis content of the relative quaternion (x/z lanes of a unit quat): a
// pure world-Y yaw has none; quat16 quantisation leaves well under 0.001.
constexpr double kSpinAxisTol = 0.004;
// Q6b: discrete acceleration AT a join over the peak acceleration of the
// segments meeting there. A quintic (C2) sampled on n >= 8 keys gives <= 0.22;
// a C1 smoothstep gives ~0.5 whatever n is.
constexpr double kJoinAccelRatioMax = 0.25;
// Root XZ joins are only judged when that segment's motion is measurable.
constexpr double kJoinRootFloorMm = 0.5;
// Q6c: carrier B's CONTACT PATCH vs the no-spin clip. The builder pivots about
// the chain point at carrier B's core station (250 + 680 + 340 = 1270 mm =
// kKnuckleAtBMm), close above the contact at the inverted loop peak.
// Measured on the first build: 15.8 mm with the support pivot, 568 mm with the
// root-pivot control. The builder pins the CHAIN point at carrier B's core
// exactly; the height-weighted contact sits ~8 mm beside that vertical axis at
// the bent inverted peak, so a full turn carries it round a circle of <= 2x8 mm
// (about one pixel at the Trick camera). 24 mm = 1.5x that residual and 24x
// under the control, so it cannot be tripped by rounding or slipped past by a
// root pivot. It bounds what the SPIN adds; the pause's own wander is reported.
constexpr double kSupportDriftMaxMm = 24.0;
// Q6d: the no-spin contact patch vs its own touchdown, every planted key. The
// builder pins carrier B's CHAIN point exactly (micrometres); what remains is the
// height-weighted contact rolling round the curved swell as the wobble tilts it.
// Measured on the first pinned build: 12.99 mm (key 131), against 178.5 mm for
// the Wave-F height-only pin (the control). 24 mm is about 1.5 px at the Trick
// camera (Q6c: 16 mm is about one pixel), 1.85x the measured roll and 7x under
// the control, so neither rounding nor a retune of the wobble trips it while a
// skating support cannot slip past it.
constexpr double kPlantDriftMaxMm = 24.0;
// The contact patch: carrier-B vertices weighted exp(-height above the lowest
// one / this scale), so the dirt-touching side dominates.
constexpr double kContactPatchMm = 25.0;
// Join LOCATION: the last/first key whose progress differs from its hold by no
// more than quat16 quantisation (~0.02 per-mille); a quintic's first moving key
// on a 30-key segment already moves 0.4 per-mille.
constexpr double kJoinDetectPm = 0.1;
// Q6a control: TWO revolutions. It ends at orientation identity (so the root
// compensation settles and Q6b/Q6c stay green) but is not the one turn asked for.
constexpr int32_t kSpinGainControlPm = 2000;

// Same predicate as manafold_probe.cpp's is_trick_support_vertex (carrier B):
// membership from the UNDEFORMED bind vertex, weight-checked bone, and the
// swell-core station window.
bool is_support_b(const zc::SkinVertex& v) {
  const uint8_t bone = u02::kBHingeB;
  const bool b0_live = v.b0 == bone && v.w0 > 0;
  const bool b1_live = v.b1 == bone && v.w0 < 64;
  if (!b0_live && !b1_live) return false;
  const int32_t bind_y_mm = static_cast<int32_t>((static_cast<int64_t>(v.y) * 1000) >> 16);
  const int32_t station_mm = bind_y_mm - (u02::kLoopNeckExitYMm - u02::kLoopBuryMm);
  const int32_t lo = u02::kLoopCarrierCoreAtMm[2] - u02::kKnuckleSwellHalfMm[2] - 1;
  const int32_t hi = u02::kLoopCarrierCoreAtMm[2] + u02::kKnuckleSwellHalfMm[2] + 1;
  return station_mm >= lo && station_mm <= hi;
}

}  // namespace

int main(int argc, char** argv) {
  const bool fail_leg = argc > 1 && std::strcmp(argv[1], "--fail-lane") == 0;
  // Q4's leg must be set BEFORE the first u02::type() call -- the bank is a
  // function-local static, so a leg is one process, not a toggle.
  const bool seam_leg = argc > 1 && std::strcmp(argv[1], "--fail-seam") == 0;
  if (seam_leg) u02::g_u02_death_fail = 4;
  const bool snap_leg = argc > 1 && std::strcmp(argv[1], "--fail-eyesnap") == 0;
  const bool eyes_only = argc > 1 && std::strcmp(argv[1], "--eyes-only") == 0;
  if (snap_leg) u02::g_u02_death_fail = 5;
  const bool startle_leg =
      argc > 1 && std::strcmp(argv[1], "--fail-startle-step") == 0;
  if (startle_leg)
    u02::g_u02_startle_timing = u02::StartleTimingControl::kLegacy;
  // PASS 13 / R5: leg 6 collapses death-gutter's sag carry to a single key,
  // which is exactly the pre-R5 behaviour -- the 240 mm one-key root teleport.
  // Witnessed on THE SHIPPED BUILDER, not on a copy of the clip.
  const bool rootstep_leg = argc > 1 && std::strcmp(argv[1], "--fail-rootstep") == 0;
  if (rootstep_leg) u02::g_u02_death_fail = 6;
  // Pass 17's Fall control must also be selected before u02::type() constructs
  // its static bank. Each CLI leg is a separate process, so this cannot leak
  // into the normal shipping verdict.
  const bool fall_only = argc > 1 && std::strcmp(argv[1], "--fall-only") == 0;
  const bool fall_wrap_leg =
      argc > 1 && std::strcmp(argv[1], "--fail-fall-wrap") == 0;
  if (fall_wrap_leg) u02::g_u02_fall_wrap_control = true;
  // VERSION 18 WAVE F controls -- also set BEFORE the bank is built.
  const bool spin_gain_leg = argc > 1 && std::strcmp(argv[1], "--fail-trick-spin-gain") == 0;
  const bool spin_ease_leg = argc > 1 && std::strcmp(argv[1], "--fail-trick-spin-ease") == 0;
  const bool spin_pivot_leg = argc > 1 && std::strcmp(argv[1], "--fail-trick-spin-pivot") == 0;
  const bool flight_seam_leg = argc > 1 && std::strcmp(argv[1], "--fail-flight-seam") == 0;
  const bool plant_pin_leg = argc > 1 && std::strcmp(argv[1], "--fail-trick-plant-pin") == 0;
  if (plant_pin_leg) u02::g_u02_trick_plant_pin = u02::TrickPlantPin::kLegacy;
  if (spin_gain_leg) u02::g_u02_trick_spin_gain_pm = kSpinGainControlPm;
  if (spin_ease_leg) u02::g_u02_trick_spin_ease = u02::TrickSpinEase::kCubic;
  if (spin_pivot_leg) u02::g_u02_trick_spin_pivot = u02::TrickSpinPivot::kRoot;
  if (flight_seam_leg) u02::g_u02_flight_seam_control = true;
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) { std::fprintf(stderr, "qa-p12: compile produced no meshlets\n"); return 1; }
  int fails = 0;
  int q1_fails = 0;

  // ---------------- Q1: the corpse, across every lane ----------------------
  std::printf("Q1 THE CORPSE ACROSS ALL %d DEFORM LANES (the shipped gate reads lane 0 only)\n",
              static_cast<int>(zc::kDeformLaneCount));
  // The settle keys come from the clip's OWN arithmetic -- death_beats() /
  // deathb_beats(), the same functions the builders and the shipped probe use
  // -- so this probe cannot bless a window the clip does not have.
  const struct { uint16_t slot; int settle; const char* name; } kDeaths[2] = {
      {17, u02::death_beats().settle, "death-drop"},
      {18, u02::deathb_beats().settle, "death-gutter"}};
  for (const auto& d : kDeaths) {
    const zc::Clip* clip = nullptr;
    for (const zc::Clip& c : T.bank.clips) if (c.slot_id == d.slot) clip = &c;
    if (!clip) { std::printf("  slot %u MISSING\n", d.slot); ++fails; continue; }
    int nz[zc::kDeformLaneCount] = {0};
    int worst[zc::kDeformLaneCount] = {0};
    int lo[zc::kDeformLaneCount], hi[zc::kDeformLaneCount];
    int moves[zc::kDeformLaneCount] = {0};
    int prevf[zc::kDeformLaneCount] = {0}, prevs[zc::kDeformLaneCount] = {0};
    bool have_prev = false;
    for (uint8_t L = 0; L < zc::kDeformLaneCount; ++L) { lo[L] = 1 << 30; hi[L] = -(1 << 30); }
    for (int f = d.settle; f < clip->frame_count; ++f)
      for (uint8_t sub = 0; sub < 2; ++sub) {
        zc::DeformFrame fr = zc::deformation_frame(T, d.slot, static_cast<uint16_t>(f), sub);
        for (uint8_t L = 0; L < zc::kDeformLaneCount; ++L) {
          // THE FAILABLE LEG: pretend lane 0's answer speaks for every lane,
          // which is exactly what the shipped gate does.
          const zc::DeformSample s = fail_leg ? fr.lane[0] : fr.lane[L];
          if (!sample_zero(s)) {
            ++nz[L];
            const int m = s.flatten > s.spread ? s.flatten : s.spread;
            if (m > worst[L]) worst[L] = m;
          }
          const int v = static_cast<int>(s.spread);
          if (v < lo[L]) lo[L] = v;
          if (v > hi[L]) hi[L] = v;
          // MOVEMENT, not mere presence. A corpse HOLDING one stretched span
          // is still; a corpse whose span changes key to key is breathing.
          // Those are different faults of very different size and reporting
          // them as one number would overstate the finding.
          if (have_prev && (prevf[L] != static_cast<int>(s.flatten) ||
                            prevs[L] != static_cast<int>(s.spread)))
            ++moves[L];
          prevf[L] = static_cast<int>(s.flatten);
          prevs[L] = static_cast<int>(s.spread);
        }
        have_prev = true;
      }
    std::printf("  slot %u %-13s eternal rest keys %d..%d, both subs:\n", d.slot, d.name,
                d.settle, clip->frame_count - 1);
    // ==== PASS 14 / R2(c) -- LANE 0 IS SCORED ON WHETHER IT CHANGES ========
    //
    // ⚠ THIS CRITERION MOVED. It read `nz[L] != 0` for every lane: eternal rest
    // meant bit-zero. That was right when it was written and it is wrong now,
    // because zero flatten is not stillness, it is the ROUND BIND POSE -- so
    // the old criterion required the corpse to be the roundest shape in the
    // clip, and R2(c) requires it to be flatter than the living animal. One of
    // the two had to give, and it is not the owner's item.
    //
    // The contract this gate means to enforce is D9 SS11.2's "eternal rest":
    // the corpse STOPS. This file already knew the difference -- its own
    // verdict string distinguishes "HELD non-zero: a frozen stretch, not
    // breathing" from "THE CORPSE IS STILL MOVING" -- and the change is to
    // score lane 0 on that distinction instead of printing it as commentary
    // beside a failure.
    //
    // ⚠ LANES 1..3 ARE UNTOUCHED and still require bit-zero. That is deliberate
    // on two counts: nothing authored has asked those spans to hold a stretch
    // on a corpse, and the --fail-lane leg (lane 0's answer pretending to speak
    // for every lane) has to stay exactly as failable as it was. Weakening
    // every lane to buy one lane's change would have removed the leg's teeth
    // as a side effect, which is how a gate quietly stops gating.
    //
    // Lane 0 is additionally pinned to the ONE authored value, so "held" cannot
    // be satisfied by holding some other number that happens not to move.
    const int want0 = static_cast<int>(u02::corpse_sample().flatten);
    for (uint8_t L = 0; L < zc::kDeformLaneCount; ++L) {
      // ⚠ THE LEG TAKES LANE 0'S CRITERION ALONG WITH LANE 0'S DATA, and it has
      // to. "lane 0 answered for every lane" means the gate that reads only
      // lane 0 -- so it applies lane 0's RULE as well. Substituting the data
      // and keeping each lane's own rule judges lane 0's held sag against
      // lanes 1..3's bit-zero rule, which fails for a reason the leg is not
      // about; the leg then reports faults, its self-check correctly says it
      // "did not take effect", and a real proof of failability quietly becomes
      // a broken one. Caught by running it, which is why it is run.
      const bool as_lane0 = fail_leg || L == 0;
      const bool bad = as_lane0 ? (moves[L] != 0 || (nz[L] != 0 && worst[L] != want0))
                                : nz[L] != 0;
      if (bad) { ++fails; ++q1_fails; }
      const char* verdict =
          moves[L] != 0
              ? "<-- THE CORPSE IS STILL MOVING"
              : (nz[L] == 0
                     ? "bit-zero"
                     : (as_lane0 && worst[L] == want0
                            ? "HELD at the authored sag (kDeathCorpseFlatPm) -- still"
                            : "<-- HELD non-zero: a frozen stretch, not breathing"));
      std::printf("    lane %u: %5d non-zero, worst %5d, spread %6d..%-6d, %4d key-to-key "
                  "changes  %s\n",
                  L, nz[L], worst[L], lo[L], hi[L], moves[L], verdict);
    }
    std::printf("    lane 0 authored corpse sag: %d (flatten), gate wants it HELD\n", want0);
    std::printf("    deform_ex track present: %s (%zu samples, want %zu)\n",
                clip->deform_ex.empty() ? "NO (identity)" : "yes", clip->deform_ex.size(),
                static_cast<size_t>(clip->frame_count) * (zc::kDeformLaneCount - 1u));
  }

  // ---------------- Q2: is the eye travel driven at all? -------------------
  std::printf("\nQ2 THE EYE TRAVEL CHANNEL -- posed travel of the eye carrier, per clip\n");
  std::printf("   (kEyeTravelMaxDeg = %d, so full travel is %d a16 on kBEyeTravelL/R)\n",
              static_cast<int>(u02::kEyeTravelMaxDeg),
              static_cast<int>(u02::kEyeTravelMaxA16));

  // *** THE SELF-CHECK, ON EVERY RUN ***
  // This section's whole answer is a column of zeroes, and a reader that is
  // simply broken produces exactly the same column. 10-GATE-CHECKLIST item 11:
  // a verification tool that finds ZERO of the thing it counts must prove it
  // could have found some. So the PRODUCTION function is called on a clean rig
  // and read by the SAME arithmetic the bank walk uses; if that does not come
  // back as the full 45 degrees, every zero below is meaningless and this
  // refuses to report them.
  // TOTAL rotation angle of the carrier, about ANY axis, from the scalar lane.
  //
  // ⚠ THE FIRST VERSION OF THIS READ q[1] AS "the y term" AND WAS WRONG: the
  // codec is (w, x, y, z) -- zref_creature.hpp's quat16_identity() is
  // {kQuatOne, 0, 0, 0} -- so q[1] is x. It returned 0.00 for a carrier driven
  // to the full 45 degrees, which is byte-for-byte the same column of zeroes
  // this section reports for the bank. The SELF-CHECK below is the only reason
  // that was caught, which is checklist item 11 landing on its own author.
  //
  // Reading the scalar lane instead of a named axis also means a rotation
  // introduced about ANY axis shows up, so a future driver that turns the
  // carrier some other way cannot slip past this.
  const auto carrier_deg = [](const zc::quat16& q) {
    double w = static_cast<double>(q.q[0]) / 16384.0;
    if (w > 1.0) w = 1.0;
    if (w < -1.0) w = -1.0;
    const double a = 2.0 * std::acos(std::fabs(w)) * 180.0 / 3.14159265358979;
    return a;
  };
  {
    u02::Rig probe_rig;
    probe_rig.reset();
    u02::apply_eye_travel(probe_rig, 1000);  // the production call, full travel
    const double got = carrier_deg(probe_rig.q[u02::kBEyeTravelL]);
    const double want = static_cast<double>(u02::kEyeTravelMaxDeg);
    const bool ok = std::fabs(got - want) <= 1.0;
    std::printf("   SELF-CHECK: apply_eye_travel(rig, 1000) reads %.2f deg (want %.0f) -- %s\n",
                got, want, ok ? "the reader CAN see travel" : "READER IS BROKEN");
    if (!ok) {
      std::printf("   FAIL Q2's reader cannot see a driven carrier; its zeroes prove nothing\n");
      ++fails;
    }
  }
  const auto relative_deg = [](const zc::quat16& a, const zc::quat16& b) {
    double d = 0.0, na = 0.0, nb = 0.0;
    for (int i = 0; i < 4; ++i) {
      const double av = static_cast<double>(a.q[i]);
      const double bv = static_cast<double>(b.q[i]);
      d += av * bv;
      na += av * av;
      nb += bv * bv;
    }
    const double den = std::sqrt(na * nb);
    if (den <= 0.0) return 180.0;
    d = std::fabs(d) / den;
    if (d > 1.0) d = 1.0;
    return 2.0 * std::acos(d) * 180.0 / 3.14159265358979;
  };
  int clips_with_travel = 0;
  int q2_fails = 0;
  int death_snap_fires = 0;
  double bank_worst = 0.0;
  for (const zc::Clip& c : T.bank.clips) {
    double worst = 0.0, jump = 0.0;
    int jat = -1;
    zc::quat16 prev_l = zc::quat16_identity();
    zc::quat16 prev_r = zc::quat16_identity();
    for (int f = 0; f < c.frame_count; ++f) {
      const zc::quat16& ql =
          c.quats[static_cast<size_t>(f) * u02::kBoneCount + u02::kBEyeTravelL];
      const zc::quat16& qr =
          c.quats[static_cast<size_t>(f) * u02::kBoneCount + u02::kBEyeTravelR];
      const double a = std::max(carrier_deg(ql), carrier_deg(qr));
      if (a > worst) worst = a;
      // Relative quaternion distance is sign- and axis-safe. Differencing scalar
      // magnitudes was blind to equal-magnitude direction reversals.
      if (f > 0) {
        const double d = std::max(relative_deg(prev_l, ql), relative_deg(prev_r, qr));
        if (d > jump) { jump = d; jat = f; }
      }
      prev_l = ql;
      prev_r = qr;
    }
    if (worst > 0.05) ++clips_with_travel;
    if (worst > bank_worst) bank_worst = worst;
    // 8 deg/key is several times the busiest smooth key in the bank and far
    // under a 45 deg switch-off, so it separates authored motion from a snap.
    const bool snap = jump > 8.0;
    if (snap) {
      ++fails;
      ++q2_fails;
      int settle = -1;
      if (c.slot_id == u02::kDeathSlot) settle = u02::death_beats().settle;
      if (c.slot_id == u02::kDeathBSlot) settle = u02::deathb_beats().settle;
      if (settle >= 0 && jat == settle) ++death_snap_fires;
    }
    std::printf("   slot %2u %4u keys   travel %6.2f deg   worst relative step %5.2f deg at key %4d%s%s\n",
                c.slot_id, c.frame_count, worst, jump, jat,
                worst <= 0.05 ? "   <-- NO TRAVEL" : "",
                snap ? "   <-- SNAPS" : "");
  }
  std::printf("   BANK: %d of %zu clips drive the eye travel; bank max %.2f deg of %d\n",
              clips_with_travel, T.bank.clips.size(), bank_worst,
              static_cast<int>(u02::kEyeTravelMaxDeg));
  if (clips_with_travel == 0) {
    std::printf("   FAIL the travel channel is built and gated but NOTHING DRIVES IT\n");
    ++fails;
  }

  // ---------------- Q3: root continuity ------------------------------------
  //
  //  PASS 13 / R5 -- Q3 IS NOW BOUNDED, AND THE BOUND IS DECLARED PER CLIP.
  //
  //  Until this pass Q3 printed and did not judge, and a 240 mm one-key root
  //  teleport in `death-gutter` sat in the shipped bank while every clearance,
  //  contact and closure gate passed. A number nobody bounds is a number
  //  nobody reads.
  //
  //  WHY THERE IS NO SINGLE BANK-WIDE NUMBER. The bank's honest interior steps
  //  span 0 to 218 mm, and the large ones are their clips' whole point: a
  //  startle recoils, a knockback knocks back, a death falls, a blast blasts.
  //  A bound loose enough for those cannot see a 240 mm teleport, and a bound
  //  tight enough to see it refuses four authored payoffs. So the shape is the
  //  one CLAUDE.md already uses for ground penetration: the default is strict,
  //  and a clip that legitimately exceeds it DECLARES a ceiling and says why.
  //
  //  Each ceiling carries real headroom over what its clip measures today and
  //  names the beat it protects, so a later change that moves one has to say
  //  which beat got faster rather than nudging a bank-wide constant nobody
  //  owns (07-MOTION-STYLE SS6: a gate re-recorded to admit its own answer will
  //  fail on the next legitimate change and look like a regression).
  //
  //  The WRAP column stays reported-not-gated: a travelling clip's seam is
  //  authored and correct (PASS-13-FINDINGS-C SS5.1), and its presentation
  //  half is measured by tools/reel/wrapseam.py.
  //
  //  THE DEFAULT, AND WHERE ITS HEADROOM COMES FROM. 240 mm read as about
  //  16 px on the shipping camera (the pop this gate exists for), so ~15 mm
  //  per pixel: a step over about 9 px in one key is a jump a viewer sees.
  //  The tightest LEGITIMATE step left in the bank under the default is
  //  `death-gutter`'s first strike into the dirt at 102.6 mm, and 135 leaves
  //  that about 30% of room -- 07-MOTION-STYLE SS6 is explicit that a band
  //  re-recorded to just admit its own measurement will fail on the next
  //  honest change and look like a regression. It still refuses the 240 mm
  //  teleport by nearly a factor of two.
  constexpr double kRootStepDefaultMm = 135.0;  // about 9 px at the shipping camera
  struct RootStepCeiling { uint16_t slot; double mm; const char* why; };
  static const RootStepCeiling kRootStepCeilings[] = {
      {4,  260.0, "startle: the recoil IS the clip"},
      {14, 210.0, "damage: the knockback impact"},
      {17, 165.0, "death-drop: the float fails and it falls"},
      {20, 300.0, "blown: the blast off the ground and the fall into the catch (R4)"},
      // VERSION 18 WAVE F: the planted 360 turns about the planted SUPPORT, so
      // the body swings round its antenna on a ~0.4 m radius; at the turn's
      // peak speed that is 178 mm/key, C2-smooth (Q6b), not a teleport.
      {13, 240.0, "trick: the planted 360 swings the body round its planted antenna"},
  };
  int q3_fails = 0;
  bool startle_over = false;
  std::printf("\nQ3 ROOT CONTINUITY -- largest single-key INTERIOR root step per clip, BOUNDED\n");
  std::printf("   Default ceiling %.0f mm; five clips declare their own (table in the source).\n",
              kRootStepDefaultMm);
  std::printf("   THE WRAP COLUMN IS THE LOOP SEAM (last key -> key 0), reported not gated. The\n"
              "   site loops every clip, so the seam is a frame the owner watches; a TRAVELLING\n"
              "   clip's seam is authored, and its presentation half is wrapseam.py.\n");
  for (const zc::Clip& c : T.bank.clips) {
    const auto step = [&](int a, int b) {
      const double dx = (c.root[(size_t)b * 3 + 0] - c.root[(size_t)a * 3 + 0]) / 65536.0 * 1000.0;
      const double dy = (c.root[(size_t)b * 3 + 1] - c.root[(size_t)a * 3 + 1]) / 65536.0 * 1000.0;
      const double dz = (c.root[(size_t)b * 3 + 2] - c.root[(size_t)a * 3 + 2]) / 65536.0 * 1000.0;
      return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    double worst = 0.0; int at = -1;
    for (int f = 0; f + 1 < c.frame_count; ++f) {
      const double m = step(f, f + 1);
      if (m > worst) { worst = m; at = f; }
    }
    const double wrap = c.frame_count > 1 ? step(c.frame_count - 1, 0) : 0.0;
    // A seam bigger than the clip's own biggest interior step is a POP: the
    // loop point moves the creature further in one key than anything the
    // animation does on purpose. Reported, not gated -- a travelling clip
    // legitimately snaps back, and only the author can say which is which.
    const char* flag = (wrap > worst && wrap > 60.0) ? "  <-- SEAM POP" : "";
    double ceiling = kRootStepDefaultMm;
    const char* why = "";
    for (const auto& rc : kRootStepCeilings)
      if (rc.slot == c.slot_id) { ceiling = rc.mm; why = rc.why; }
    const bool over = worst > ceiling;
    if (over) {
      ++q3_fails;
      if (c.slot_id == 4) startle_over = true;
    }
    std::printf("   slot %2u  worst step %7.1f mm at key %d -> %d  (ceiling %5.0f)  |  WRAP %7.1f mm%s%s\n",
                c.slot_id, worst, at, at + 1, ceiling, wrap, flag,
                over ? "  <-- OVER ITS DECLARED CEILING" : "");
    if (over)
      std::printf("        declared %.0f mm because -- %s\n", ceiling,
                  *why ? why : "nothing: this clip is expected to stay under the default");
  }
  fails += q3_fails;

  // ---------------- Q4: THE LOOP SEAM ---------------------------------------
  //
  //  A death must not blend back toward key 0. Q1 proves the DEFORM reaches
  //  bit-zero; it cannot see this one, because the resurrection is in the POSE
  //  and the ROOT. decode_pose takes its sub-frame partner as
  //  `frame + 1 >= frame_count ? (hold_last ? frame : 0) : frame + 1`, so with
  //  hold_last off the last key's sub 1 is a half-blend into the ALIVE hover
  //  pose. Measured on POSED VERTICES through decode_pose + skin_vertex -- the
  //  renderer's own calls -- and NOT on the authored keys, which are already
  //  correct and which is exactly why this survived a whole pass.
  int q4_fails = 0;
  std::printf("\nQ4 THE LOOP SEAM -- does the corpse hold, or blend back toward key 0?\n");
  std::printf("   (posed vertices, last key sub 0 vs sub 1, through decode_pose)\n");
  for (const auto& d : kDeaths) {
    const zc::Clip* clip = nullptr;
    for (const zc::Clip& c : T.bank.clips) if (c.slot_id == d.slot) clip = &c;
    if (!clip) { std::printf("   slot %u MISSING\n", d.slot); ++fails; continue; }
    const uint16_t last = static_cast<uint16_t>(clip->frame_count - 1);
    std::array<zc::mat3x4fx, zc::kMaxBones> p0, p1;
    zc::decode_pose(T, *clip, last, p0, nullptr, 0);
    zc::decode_pose(T, *clip, last, p1, nullptr, 1);
    const zc::DeformSample s0 = zc::deformation_sample(T, d.slot, last, 0);
    const zc::DeformSample s1 = zc::deformation_sample(T, d.slot, last, 1);
    double worst = 0.0;
    for (const zc::Meshlet& m : T.mesh)
      for (size_t vi = 0; vi < m.verts.size(); ++vi) {
        zc::SkinVertex a = m.verts[vi], b = m.verts[vi];
        if (!m.deform.empty()) {
          a = zc::deform_skin_vertex(a, m.deform[vi], s0);
          b = zc::deform_skin_vertex(b, m.deform[vi], s1);
        }
        int32_t ax, ay, az, bx, by, bz;
        zc::skin_vertex(p0.data(), a, ax, ay, az, nullptr);
        zc::skin_vertex(p1.data(), b, bx, by, bz, nullptr);
        const double dx = (bx - ax) / 65536.0 * 1000.0;
        const double dy = (by - ay) / 65536.0 * 1000.0;
        const double dz = (bz - az) / 65536.0 * 1000.0;
        const double mm = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (mm > worst) worst = mm;
      }
    // THE BOUND, and why it is not zero. With hold_last the sub-frame partner
    // is the key ITSELF, so root and deform are bit-identical -- but the pose
    // still goes through quat16_nlerp(q, q, 1, 2), whose renormalisation
    // rounds. Measured residual: 0.9 mm on both deaths, on a ~1.3 m creature
    // (0.07%). That is codec rounding, not motion.
    //
    // Held: 0.9 mm.  Wrapping to key 0: 509.1 mm (drop) / 411.9 mm (gutter),
    // witnessed through --fail-seam. A 5 mm bound sits 5x above the rounding
    // and 80x below the fault, so it cannot be tripped by rounding drift and
    // cannot be slipped past by a real wrap. A 1 mm bound would have shipped
    // a gate passing by 0.1 mm -- true today, and a lie one rounding change
    // from now.
    const bool bad = worst > 5.0;
    if (bad) { ++fails; ++q4_fails; }
    std::printf("   slot %u %-13s hold_last=%-3s worst posed vertex move %8.1f mm  %s\n",
                d.slot, d.name, clip->hold_last ? "yes" : "NO", worst,
                bad ? "<-- THE CORPSE STANDS BACK UP" : "held");
  }

  // ---------------- Q5: FALL HOLDS THE WHOLE FINAL POSE ---------------------
  //
  // Fall has 170 authored keys and 340 presentation frames. f338 is key 169;
  // f339 is that key's sub=1 companion. Pass 16 protected only root travel with
  // wrap_root_delta, so f339 still blended quats/translations/deformation toward
  // key 0 and visibly reset the pose. A one-shot must clamp EVERY pose channel.
  int q5_fails = 0;
  std::printf("\nQ5 FALL ONE-SHOT -- f338 key 169 vs f339 held presentation partner\n");
  const zc::Clip* fall = nullptr;
  for (const zc::Clip& c : T.bank.clips)
    if (c.slot_id == 9) fall = &c;
  if (fall == nullptr || fall->frame_count == 0) {
    std::printf("   slot 9 MISSING\n");
    ++fails;
    ++q5_fails;
  } else {
    const size_t n = fall->frame_count;
    const size_t last = n - 1u;
    const size_t bc = u02::kBoneCount;
    const size_t ex = static_cast<size_t>(zc::kDeformLaneCount) - 1u;
    const auto check = [&](const char* name, bool shape_ok, bool equal) {
      const bool ok = shape_ok && equal;
      std::printf("   %-24s %s\n", name,
                  ok ? "held exactly" : (shape_ok ? "DIFFERS -- WRAPPED" : "MISSING/MALFORMED"));
      if (!ok) {
        ++fails;
        ++q5_fails;
      }
    };

    const bool flags_ok = fall->interpolate && fall->hold_last && !fall->wrap_root_delta;
    std::printf("   flags interpolate=%s hold_last=%s wrap_root_delta=%s  %s\n",
                fall->interpolate ? "yes" : "NO", fall->hold_last ? "yes" : "NO",
                fall->wrap_root_delta ? "YES" : "no",
                flags_ok ? "one-shot" : "<-- NOT THE SHIPPING ONE-SHOT CONTRACT");
    if (!flags_ok) {
      ++fails;
      ++q5_fails;
    }

    std::array<zc::mat3x4fx, zc::kMaxBones> key_pose{}, mid_pose{};
    zc::decode_pose(T, *fall, static_cast<uint16_t>(last), key_pose, nullptr, 0);
    zc::decode_pose(T, *fall, static_cast<uint16_t>(last), mid_pose, nullptr, 1);
    const zc::DeformFrame key_deform =
        zc::deformation_frame(T, fall->slot_id, static_cast<uint16_t>(last), 0);
    const zc::DeformFrame mid_deform =
        zc::deformation_frame(T, fall->slot_id, static_cast<uint16_t>(last), 1);

    const bool root_shape = fall->root.size() == n * 3u;
    const zc::mat3x4fx& kr = key_pose[u02::kBRoot];
    const zc::mat3x4fx& mr = mid_pose[u02::kBRoot];
    check("root xyz", root_shape,
          root_shape && kr.m[3] == mr.m[3] && kr.m[7] == mr.m[7] &&
              kr.m[11] == mr.m[11]);

    const size_t quat_i = last * bc;
    const bool quat_shape = fall->quats.size() == n * bc && fall->mid_quats.size() == n * bc;
    check("all bone quaternions", quat_shape,
          quat_shape && std::memcmp(fall->quats.data() + quat_i,
                                    fall->mid_quats.data() + quat_i,
                                    bc * sizeof(zc::quat16)) == 0);

    const size_t local_count = n * bc * 3u;
    const size_t local_i = last * bc * 3u;
    const bool local_absent = fall->local_translation.empty();
    const bool local_shape = local_absent ? fall->mid_local_translation.empty()
                                          : (fall->local_translation.size() == local_count &&
                                             fall->mid_local_translation.size() == local_count);
    check("local translations", local_shape,
          local_shape && (local_absent ||
              std::memcmp(fall->local_translation.data() + local_i,
                          fall->mid_local_translation.data() + local_i,
                          bc * 3u * sizeof(int32_t)) == 0));

    const size_t scale_count = n * bc;
    const size_t scale_i = last * bc;
    const bool scale_absent = fall->uniform_scale_q15.empty();
    const bool scale_shape = scale_absent ? fall->mid_uniform_scale_q15.empty()
                                          : (fall->uniform_scale_q15.size() == scale_count &&
                                             fall->mid_uniform_scale_q15.size() == scale_count);
    check("uniform bone scale", scale_shape,
          scale_shape && (scale_absent ||
              std::memcmp(fall->uniform_scale_q15.data() + scale_i,
                          fall->mid_uniform_scale_q15.data() + scale_i,
                          bc * sizeof(uint16_t)) == 0));

    const bool deform_shape = fall->deform.size() == n;
    check("primary deform lane", deform_shape,
          deform_shape && std::memcmp(&key_deform.lane[0], &mid_deform.lane[0],
                                      sizeof(zc::DeformSample)) == 0);

    const size_t dex_count = n * ex;
    const bool dex_absent = fall->deform_ex.empty();
    const bool dex_shape = dex_absent || fall->deform_ex.size() == dex_count;
    bool dex_equal = dex_shape;
    for (size_t lane = 1; lane < zc::kDeformLaneCount && dex_equal; ++lane)
      dex_equal = std::memcmp(&key_deform.lane[lane], &mid_deform.lane[lane],
                              sizeof(zc::DeformSample)) == 0;
    check("extra deform lanes", dex_shape, dex_equal);
  }

  // ---------------- Q6: TRICK PLANTED 360 ---------------------------------
  int q6a_fails = 0, q6b_fails = 0, q6c_fails = 0, q6d_fails = 0;
  {
    std::printf("\nQ6 TRICK PLANTED 360 -- extracted from the shipped root vs the no-spin control\n");
    const zc::Clip* spin = nullptr;
    for (const zc::Clip& c : T.bank.clips) if (c.slot_id == 13) spin = &c;
    // The control: the SAME builder with the spin off, finalized like the bank.
    const u02::TrickSpinMode saved = u02::g_u02_trick_spin;
    u02::g_u02_trick_spin = u02::TrickSpinMode::kNone;
    zc::Clip none = u02::build_trick();
    u02::finalize_rear_follow(none);
    u02::g_u02_trick_spin = saved;
    const size_t bc = u02::kBoneCount;
    if (spin == nullptr || spin->frame_count != none.frame_count) {
      std::printf("   slot 13 MISSING or length mismatch\n");
      ++q6a_fails;
    } else {
      const int k0 = u02::kTrickPlantKey, k1 = u02::kTrickLiftKey;
      const int n = k1 - k0;
      std::vector<double> pm(static_cast<size_t>(n), 0.0);
      double prev = 0.0, axis_worst = 0.0;
      for (int f = k0; f < k1; ++f) {
        const zc::quat16& a = spin->quats[static_cast<size_t>(f) * bc + u02::kBRoot];
        const zc::quat16& b = none.quats[static_cast<size_t>(f) * bc + u02::kBRoot];
        // rel = a * conj(b), in doubles from the stored lanes
        const double aw = a.q[0], ax = a.q[1], ay = a.q[2], az = a.q[3];
        const double bw = b.q[0], bx = -b.q[1], by = -b.q[2], bz = -b.q[3];
        double rw = aw * bw - ax * bx - ay * by - az * bz;
        double rx = aw * bx + ax * bw + ay * bz - az * by;
        double ry = aw * by - ax * bz + ay * bw + az * bx;
        double rz = aw * bz + ax * by - ay * bx + az * bw;
        const double nn = std::sqrt(rw * rw + rx * rx + ry * ry + rz * rz);
        rw /= nn; rx /= nn; ry /= nn; rz /= nn;
        axis_worst = std::max(axis_worst, std::max(std::fabs(rx), std::fabs(rz)));
        // yaw in per-mille of a turn, unwrapped against the previous key (the
        // quaternion sign ambiguity is a whole turn, removed by the same unwrap)
        double th = std::atan2(ry, rw) / 3.14159265358979323846 * 1000.0;
        while (th - prev > 500.0) th -= 1000.0;
        while (th - prev < -500.0) th += 1000.0;
        pm[static_cast<size_t>(f - k0)] = th;
        prev = th;
      }
      int peak = 0;
      for (int i = 1; i < n; ++i) if (pm[i] > pm[peak]) peak = i;
      int start = 0;  // last key of the pause
      while (start + 1 < n && std::fabs(pm[start + 1]) <= kJoinDetectPm) ++start;
      const double final_pm = pm[n - 1];
      int end = n - 1;  // first key of the corrected hold
      while (end - 1 > peak && std::fabs(pm[end - 1] - final_pm) <= kJoinDetectPm) --end;
      int reversals = 0, last_sign = 0;
      double step_worst = 0.0;
      for (int i = 1; i < n; ++i) {
        const double d = pm[i] - pm[i - 1];
        step_worst = std::max(step_worst, std::fabs(d));
        const int sg = d > 0.2 ? 1 : d < -0.2 ? -1 : 0;
        if (sg != 0) {
          if (last_sign != 0 && sg != last_sign) ++reversals;
          last_sign = sg;
        }
      }
      const double over = pm[peak] - 1000.0;
      const bool pause_ok = start > 0;
      const bool ident_ok = std::fabs(final_pm - 1000.0) <= kSpinTolPm;
      const bool over_ok = over >= kSpinOvershootMinPm && over <= kSpinOvershootMaxPm;
      const bool rev_ok = reversals == 1;
      const bool step_ok = step_worst <= kSpinMaxStepPm;
      const bool axis_ok = axis_worst <= kSpinAxisTol;
      std::printf("   Q6a pause keys %d..%d | turn to key %d peak %.2f pm (overshoot %.2f, declared %d)\n"
                  "       corrected by key %d to %.2f pm | reversals %d | worst step %.2f pm/key"
                  " | off-axis %.5f\n",
                  k0, k0 + start, k0 + peak, pm[peak], over,
                  static_cast<int>(u02::g_u02_trick_spin_overshoot_pm), k0 + end, final_pm,
                  reversals, step_worst, axis_worst);
      if (!pause_ok) { std::printf("   Q6a FAIL no pause: the turn starts at the plant\n"); ++q6a_fails; }
      if (!ident_ok) { std::printf("   Q6a FAIL not ONE full revolution: %.2f pm before the righting\n", final_pm); ++q6a_fails; }
      if (!over_ok) { std::printf("   Q6a FAIL overshoot %.2f pm outside %.0f..%.0f\n", over, kSpinOvershootMinPm, kSpinOvershootMaxPm); ++q6a_fails; }
      if (!rev_ok) { std::printf("   Q6a FAIL %d reversals (exactly one: the correction)\n", reversals); ++q6a_fails; }
      if (!step_ok) { std::printf("   Q6a FAIL step %.2f pm/key over %.0f\n", step_worst, kSpinMaxStepPm); ++q6a_fails; }
      if (!axis_ok) { std::printf("   Q6a FAIL not a pure world-vertical yaw (off-axis %.5f)\n", axis_worst); ++q6a_fails; }
      // identity from the lift through the rest of the clip (same rotation as
      // the no-spin righting, either quaternion sign)
      for (int f = k1; f < spin->frame_count; ++f) {
        const zc::quat16& a = spin->quats[static_cast<size_t>(f) * bc + u02::kBRoot];
        const zc::quat16& b = none.quats[static_cast<size_t>(f) * bc + u02::kBRoot];
        const int64_t dot = static_cast<int64_t>(a.q[0]) * b.q[0] + static_cast<int64_t>(a.q[1]) * b.q[1] +
                            static_cast<int64_t>(a.q[2]) * b.q[2] + static_cast<int64_t>(a.q[3]) * b.q[3];
        if (std::llabs(dot) < 16384LL * 16384LL - 16384LL * 8LL) {
          std::printf("   Q6a FAIL key %d after the lift differs from the no-spin righting\n", f);
          ++q6a_fails;
          break;
        }
      }

      // Q6b: C2 at the three joins, progress and root XZ
      const auto acc = [&](const std::vector<double>& v, int i) {
        return v[static_cast<size_t>(i + 1)] - 2.0 * v[static_cast<size_t>(i)] +
               v[static_cast<size_t>(i - 1)];
      };
      // The SPIN's compensation is the shipped root XZ minus the no-spin
      // control's: since the plant pin, the control's root also moves in XZ
      // (it absorbs the balance sway), and that motion is not the spin's join.
      std::vector<double> rxv(static_cast<size_t>(n)), rzv(static_cast<size_t>(n));
      for (int f = k0; f < k1; ++f) {
        const size_t r = static_cast<size_t>(f) * 3;
        rxv[static_cast<size_t>(f - k0)] = (spin->root[r + 0] - none.root[r + 0]) / 65536.0 * 1000.0;
        rzv[static_cast<size_t>(f - k0)] = (spin->root[r + 2] - none.root[r + 2]) / 65536.0 * 1000.0;
      }
      const auto seg_peak = [&](const std::vector<double>& v, int lo, int hi) {
        double m = 0.0;
        for (int i = std::max(1, lo); i <= std::min(n - 2, hi); ++i) m = std::max(m, std::fabs(acc(v, i)));
        return m;
      };
      const int joins[3] = {start, peak, end};
      const char* jname[3] = {"motion start", "overshoot peak", "correction end"};
      for (int j = 0; j < 3; ++j) {
        const int J = joins[j];
        if (J < 1 || J > n - 2) {
          std::printf("   Q6b FAIL join %s at the window edge\n", jname[j]);
          ++q6b_fails;
          continue;
        }
        const int lo = j == 2 ? peak : start;
        const int hi = j == 0 ? peak : end;
        const double pk = seg_peak(pm, lo, hi);
        const double r = pk > 0 ? std::fabs(acc(pm, J)) / pk : 0.0;
        double rr = 0.0;
        const double rpk = std::max(seg_peak(rxv, lo, hi), seg_peak(rzv, lo, hi));
        if (rpk > kJoinRootFloorMm)
          rr = std::max(std::fabs(acc(rxv, J)), std::fabs(acc(rzv, J))) / rpk;
        const bool ok = r <= kJoinAccelRatioMax && rr <= kJoinAccelRatioMax;
        std::printf("   Q6b %-15s key %3d  progress accel ratio %.3f  root-XZ ratio %.3f"
                    " (segment peak %.2f mm/key^2)  %s\n",
                    jname[j], k0 + J, r, rr, rpk, ok ? "C2" : "FAIL -- ACCELERATION JUMPS");
        if (!ok) ++q6b_fails;
      }

      // Q6c: carrier B's CONTACT PATCH (its vertices within kContactPatchMm of
      // its lowest point) spin vs no-spin, every key of the plant. The patch is
      // what touches the dirt; the centroid of the whole curved swell sits off
      // the arc and is REPORTED only (a turn carries it round a small circle).
      double drift_worst = 0.0, cen_worst = 0.0, wander_worst = 0.0;
      double wander_x0 = 0.0, wander_z0 = 0.0;
      double ship_x0 = 0.0, ship_z0 = 0.0, ship_worst = 0.0;
      int drift_at = -1, wander_at = -1;
      for (int f = k0; f < k1; ++f) {
        std::array<zc::mat3x4fx, zc::kMaxBones> ps, pn;
        zc::decode_pose(T, *spin, static_cast<uint16_t>(f), ps, nullptr, 0);
        zc::decode_pose(T, none, static_cast<uint16_t>(f), pn, nullptr, 0);
        struct P { int32_t x, y, z; };
        std::vector<P> vs, vn;
        for (const zc::Meshlet& m : T.mesh)
          for (size_t vi = 0; vi < m.verts.size(); ++vi) {
            if (!is_support_b(m.verts[vi])) continue;
            zc::SkinVertex a = m.verts[vi], b = m.verts[vi];
            if (!m.deform.empty()) {
              a = zc::deform_skin_vertex(a, m.deform[vi], spin->deform[static_cast<size_t>(f)]);
              b = zc::deform_skin_vertex(b, m.deform[vi], none.deform[static_cast<size_t>(f)]);
            }
            P p1{}, p2{};
            zc::skin_vertex(ps.data(), a, p1.x, p1.y, p1.z, nullptr);
            zc::skin_vertex(pn.data(), b, p2.x, p2.y, p2.z, nullptr);
            vs.push_back(p1);
            vn.push_back(p2);
          }
        if (vs.empty()) {
          ++q6c_fails;
          std::printf("   Q6c FAIL no carrier-B vertices\n");
          break;
        }
        const auto patch = [](const std::vector<P>& v, double& cx, double& cz, double& ax,
                              double& az) {
          // Height-weighted, so the estimate slides smoothly instead of jumping
          // when the single lowest vertex changes (the loop has 8 vertices per
          // ring, ~50 mm apart: a hard cut-off jitters by tens of mm).
          int32_t ymin = INT32_MAX;
          for (const P& p : v) ymin = std::min(ymin, p.y);
          double sx = 0, sz = 0, sw = 0, tx = 0, tz = 0;
          for (const P& p : v) {
            tx += p.x; tz += p.z;
            const double w = std::exp(-(p.y - ymin) / 65.536 / kContactPatchMm);
            sx += w * p.x; sz += w * p.z; sw += w;
          }
          cx = sx / sw; cz = sz / sw;
          ax = tx / v.size(); az = tz / v.size();
        };
        double scx, scz, sax, saz, ncx, ncz, nax, naz;
        patch(vs, scx, scz, sax, saz);
        patch(vn, ncx, ncz, nax, naz);
        const double k = 1000.0 / 65536.0;
        const double d = std::hypot((scx - ncx) * k, (scz - ncz) * k);
        const double dc = std::hypot((sax - nax) * k, (saz - naz) * k);
        cen_worst = std::max(cen_worst, dc);
        // PRE-EXISTING, reported only: how far the NO-SPIN contact wanders from
        // where it touched down (the balance wobble is height-pivoted only).
        // Q6d gates it (the plant pin); the shipped absolute drift is reported.
        if (f == k0) { wander_x0 = ncx; wander_z0 = ncz; ship_x0 = scx; ship_z0 = scz; }
        const double wd = std::hypot((ncx - wander_x0) * k, (ncz - wander_z0) * k);
        if (wd > wander_worst) { wander_worst = wd; wander_at = f; }
        ship_worst = std::max(ship_worst, std::hypot((scx - ship_x0) * k, (scz - ship_z0) * k));
        if (d > drift_worst) { drift_worst = d; drift_at = f; }
      }
      const bool drift_ok = drift_worst <= kSupportDriftMaxMm;
      std::printf("   Q6c carrier-B contact-patch drift vs no-spin: worst %.2f mm at key %d (allowed %.0f)  %s\n"
                  "       reported only: whole-swell centroid %.2f mm; the NO-SPIN contact itself"
                  " is gated by Q6d below\n",
                  drift_worst, drift_at, kSupportDriftMaxMm,
                  drift_ok ? "planted" : "FAIL -- THE SUPPORT SLIDES", cen_worst);
      if (!drift_ok) ++q6c_fails;
      const bool pin_ok = wander_worst <= kPlantDriftMaxMm;
      std::printf("   Q6d no-spin contact patch vs touchdown, keys %d..%d: worst %.2f mm at key %d"
                  " (allowed %.0f)  %s\n"
                  "       reported: SHIPPED contact vs its touchdown worst %.2f mm (<= Q6c + Q6d bounds %.0f)\n",
                  k0, k1 - 1, wander_worst, wander_at, kPlantDriftMaxMm,
                  pin_ok ? "planted" : "FAIL -- THE PLANTED TIP SKATES", ship_worst,
                  kSupportDriftMaxMm + kPlantDriftMaxMm);
      if (!pin_ok) ++q6d_fails;
    }
    fails += q6a_fails + q6b_fails + q6c_fails + q6d_fails;
  }

  // ---------------- Q7: FLIGHT ONE CLOCK -----------------------------------
  int q7_fails = 0;
  {
    std::printf("\nQ7 FLIGHT ONE CLOCK -- slot 22 root height: cycles, amplitude, seam\n");
    const zc::Clip* fl = nullptr;
    for (const zc::Clip& c : T.bank.clips) if (c.slot_id == u02::kFlightSlot) fl = &c;
    if (fl == nullptr || fl->frame_count < 8) {
      std::printf("   slot %u MISSING\n", static_cast<unsigned>(u02::kFlightSlot));
      ++q7_fails;
    } else {
      const int K = fl->frame_count;
      std::vector<double> y(static_cast<size_t>(K));
      double lo = 1e9, hi = -1e9, xabs = 0.0;
      for (int f = 0; f < K; ++f) {
        y[static_cast<size_t>(f)] = fl->root[static_cast<size_t>(f) * 3 + 1] / 65536.0 * 1000.0;
        lo = std::min(lo, y[static_cast<size_t>(f)]);
        hi = std::max(hi, y[static_cast<size_t>(f)]);
        xabs = std::max(xabs, std::fabs(fl->root[static_cast<size_t>(f) * 3 + 0] / 65536.0 * 1000.0));
      }
      const auto Y = [&](int f) { return y[static_cast<size_t>(((f % K) + K) % K)]; };
      int maxima = 0;
      for (int f = 0; f < K; ++f)
        if (Y(f) > Y(f - 1) && Y(f) >= Y(f + 1)) ++maxima;
      const auto d1 = [&](int f) { return Y(f + 1) - Y(f); };
      const auto d2 = [&](int f) { return Y(f + 1) - 2 * Y(f) + Y(f - 1); };
      const auto d3 = [&](int f) { return Y(f + 2) - 3 * Y(f + 1) + 3 * Y(f) - Y(f - 1); };
      // interior: every stencil that stays inside keys 0..K-1
      double s1 = 0, s2 = 0, s3 = 0;
      for (int f = 0; f + 1 < K; ++f) s1 = std::max(s1, std::fabs(d1(f)));
      for (int f = 1; f + 1 < K; ++f) s2 = std::max(s2, std::fabs(d2(f)));
      for (int f = 1; f + 2 < K; ++f) s3 = std::max(s3, std::fabs(d3(f)));
      // the seam: every stencil that crosses key K-1 -> 0
      const double e1 = std::fabs(d1(K - 1));
      const double e2 = std::max(std::fabs(d2(K - 1)), std::fabs(d2(0)));
      const double e3 = std::max(std::max(std::fabs(d3(K - 2)), std::fabs(d3(K - 1))), std::fabs(d3(0)));
      const double amp = (hi - lo) / 2.0;
      const double want = u02::g_u02_flight_amp_mm;
      const bool cyc_ok = maxima == u02::g_u02_flight_cycles;
      const bool amp_ok = std::fabs(amp - want) <= want * 0.02 + 2.0;
      const bool seam_ok = e1 <= s1 * 1.05 + 0.5 && e2 <= s2 * 1.05 + 0.5 && e3 <= s3 * 1.05 + 0.5;
      std::printf("   maxima %d (declared %d) | amplitude %.1f mm (declared %d) | root x max %.1f mm\n",
                  maxima, u02::g_u02_flight_cycles, amp, static_cast<int>(want), xabs);
      std::printf("   seam step/accel/jerk %.2f/%.3f/%.4f vs interior max %.2f/%.3f/%.4f mm  %s\n",
                  e1, e2, e3, s1, s2, s3, seam_ok ? "closes" : "FAIL -- THE LOOP SEAM IS A POP");
      if (!cyc_ok) { std::printf("   FAIL height maxima != declared cycles\n"); ++q7_fails; }
      if (!amp_ok) { std::printf("   FAIL amplitude off the declared knob\n"); ++q7_fails; }
      if (!seam_ok) ++q7_fails;
    }
    fails += q7_fails;
  }

  // Each Wave-F leg is judged on its OWN category, and must leave the other
  // Wave-F categories green, so a control cannot be certified by a neighbour.
  const auto attributed = [&](const char* leg, int own, int others) {
    if (own == 0 || others != 0) {
      std::printf("qa-p12: %s did NOT fire alone (own %d, other Wave-F categories %d)\n", leg, own,
                  others);
      return 1;
    }
    std::printf("qa-p12: FAILABLE LEG OK -- %s fired only its own category (%d item(s))\n", leg, own);
    return 0;
  };
  if (spin_gain_leg)
    return attributed("--fail-trick-spin-gain [Q6a]", q6a_fails, q6b_fails + q6c_fails + q6d_fails + q7_fails);
  if (spin_ease_leg)
    return attributed("--fail-trick-spin-ease [Q6b]", q6b_fails, q6a_fails + q6c_fails + q6d_fails + q7_fails);
  if (spin_pivot_leg)
    return attributed("--fail-trick-spin-pivot [Q6c]", q6c_fails, q6a_fails + q6b_fails + q6d_fails + q7_fails);
  if (plant_pin_leg)
    return attributed("--fail-trick-plant-pin [Q6d]", q6d_fails, q6a_fails + q6b_fails + q6c_fails + q7_fails);
  if (flight_seam_leg)
    return attributed("--fail-flight-seam [Q7]", q7_fails, q6a_fails + q6b_fails + q6c_fails + q6d_fails);

  std::printf("\n%s: %d failure(s)%s\n", fails ? "FAIL" : "PASS", fails,
              fail_leg ? "   [FAILABLE LEG: lane 0 answered for every lane]" : "");
  if (seam_leg) {
    // Judged on Q4's OWN count. Judging a leg on the total would let an
    // unrelated failure elsewhere certify a check that never detected anything
    // -- the shape of a gate that cannot fail.
    if (q4_fails == 0) {
      std::printf("qa-p12: the leg removed hold_last from both death builders and Q4 "
                  "STILL reported the corpse held -- the leg did not take effect, so "
                  "Q4 is NOT proved failable\n");
      return 1;
    }
    std::printf("qa-p12: FAILABLE LEG OK -- with hold_last off Q4 reports %d death(s) "
                "standing back up, which is the fault pass 12 shipped\n", q4_fails);
    return 0;
  }

  if (fall_wrap_leg) {
    // Judge the control on Q5 alone. Root-delta protection can keep the final
    // root equal while the rest of the pose wraps, so an unrelated failure or
    // root-only check must not certify this instrument.
    if (q5_fails == 0) {
      std::printf("qa-p12: --fail-fall-wrap restored the old Fall wrap but Q5 "
                  "still reported every final pose channel held -- Q5 is NOT "
                  "proved failable\n");
      return 1;
    }
    std::printf("qa-p12: FAILABLE LEG OK -- legacy Fall wrap makes Q5 reject "
                "%d contract/channel item(s), including the non-root pose reset\n",
                q5_fails);
    return 0;
  }

  if (fall_only) {
    std::printf("qa-p12: FALL-ONLY verdict -- Q5 has %d failure(s)\n", q5_fails);
    return q5_fails == 0 ? 0 : 1;
  }

  if (snap_leg) {
    if (death_snap_fires != 2 || q2_fails != 2) {
      std::printf("qa-p12: --fail-eyesnap restored the old reset but Q2 caught "
                  "%d/2 death settle snaps and %d total Q2 failure(s) -- the "
                  "control is not solely attributed to those two resets\n",
                  death_snap_fires, q2_fails);
      return 1;
    }
    std::printf("qa-p12: FAILABLE LEG OK -- Q2 rejects both death eye resets "
                "at their own settle keys\n");
    return 0;
  }

  if (eyes_only) {
    std::printf("qa-p12: EYES-ONLY verdict -- Q2 has %d failure(s)\n", q2_fails);
    return q2_fails == 0 ? 0 : 1;
  }

  if (startle_leg) {
    if (!startle_over || q3_fails != 1) {
      std::printf("qa-p12: --fail-startle-step selected legacy timing but slot 4 "
                  "over=%s with %d total Q3 failure(s) -- the control is not "
                  "solely attributed to Startle\n",
                  startle_over ? "yes" : "NO", q3_fails);
      return 1;
    }
    std::printf("qa-p12: FAILABLE LEG OK -- legacy Startle timing exceeds its "
                "unchanged 260 mm ceiling\n");
    return 0;
  }

  if (rootstep_leg) {
    if (q3_fails == 0) {
      std::printf("qa-p12: the leg collapsed death-gutter's sag carry to one key and Q3 "
                  "STILL reported every clip inside its ceiling -- the leg did not take "
                  "effect, so Q3's bound is NOT proved failable\n");
      return 1;
    }
    std::printf("qa-p12: FAILABLE LEG OK -- with the sag carry at one key Q3 reports %d clip(s) "
                "over ceiling, which is the 240 mm teleport pass 12 shipped\n", q3_fails);
    return 0;
  }

  if (fail_leg) {
    // The leg is about Q1 ONLY, so it is judged on Q1's own count. Judging it
    // on the total would let Q2's unrelated failure certify a Q1 that never
    // detected anything -- which is the shape of a gate that cannot fail.
    if (q1_fails != 0) {
      std::printf("qa-p12: the leg made Q1 blind as intended -- but Q1 STILL "
                  "reported %d lane fault(s), so the leg did not take effect\n", q1_fails);
      return 1;
    }
    std::printf("qa-p12: FAILABLE LEG OK -- with lane 0 answering for every lane Q1 reports "
                "the corpse as dead, which is exactly what the shipped gate does\n");
    return 0;
  }
  return fails ? 1 : 0;
}
