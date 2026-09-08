// manafold_qa_p12.cpp -- PASS 12 QA: the checks the shipped gates cannot make.
//
// Committed rather than improvised, per CLAUDE.md ("a probe that does this was
// written once and thrown away, so its numbers are unreproducible -- commit the
// probe"). Three checks, each aimed at a gap found by reading the shipped
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
// Build:
//   g++ -O2 -std=c++17 -Ireference/include -Iruntime/include -Itests/render \
//       -Ireference/src tools/reel/manafold_qa_p12.cpp -o manafold-qa-p12.exe
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

}  // namespace

int main(int argc, char** argv) {
  const bool fail_leg = argc > 1 && std::strcmp(argv[1], "--fail-lane") == 0;
  // Q4's leg must be set BEFORE the first u02::type() call -- the bank is a
  // function-local static, so a leg is one process, not a toggle.
  const bool seam_leg = argc > 1 && std::strcmp(argv[1], "--fail-seam") == 0;
  if (seam_leg) u02::g_u02_death_fail = 4;
  const bool snap_leg = argc > 1 && std::strcmp(argv[1], "--fail-eyesnap") == 0;
  if (snap_leg) u02::g_u02_death_fail = 5;
  // PASS 13 / R5: leg 6 collapses death-gutter's sag carry to a single key,
  // which is exactly the pre-R5 behaviour -- the 240 mm one-key root teleport.
  // Witnessed on THE SHIPPED BUILDER, not on a copy of the clip.
  const bool rootstep_leg = argc > 1 && std::strcmp(argv[1], "--fail-rootstep") == 0;
  if (rootstep_leg) u02::g_u02_death_fail = 6;
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
    for (uint8_t L = 0; L < zc::kDeformLaneCount; ++L) {
      const bool bad = nz[L] != 0;
      if (bad) { ++fails; ++q1_fails; }
      const char* verdict =
          !bad ? "bit-zero"
               : (moves[L] == 0 ? "<-- HELD non-zero: a frozen stretch, not breathing"
                                : "<-- THE CORPSE IS STILL MOVING");
      std::printf("    lane %u: %5d non-zero, worst %5d, spread %6d..%-6d, %4d key-to-key "
                  "changes  %s\n",
                  L, nz[L], worst[L], lo[L], hi[L], moves[L], verdict);
    }
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
  int clips_with_travel = 0;
  double bank_worst = 0.0;
  for (const zc::Clip& c : T.bank.clips) {
    double worst = 0.0, prev = 0.0, jump = 0.0;
    int jat = -1;
    for (int f = 0; f < c.frame_count; ++f) {
      const zc::quat16& q = c.quats[static_cast<size_t>(f) * u02::kBoneCount + u02::kBEyeTravelL];
      // ⚠ THE COMMENT THIS LINE USED TO CARRY WAS FALSE IN BOTH HALVES, and it
      //  is corrected rather than deleted because it is what misled the reader.
      //  It said "identity quat16 is (0,0,0,1<<14)" and read q.q[1]. In fact
      //  zref_creature.hpp has
      //      quat16_identity() { return quat16{{kQuatOne, 0, 0, 0}}; }
      //  so w is lane 0, the lanes are (w, x, y, z), and lane 1 is X --
      //  while apply_eye_travel uses quat_y(), which lives in lane 2. The gate
      //  read the X lane of a pure-Y rotation and printed 0.00 deg for every
      //  clip no matter what the channel did.
      //
      //  carrier_deg is 2*acos(|w|): an AXIS-AGNOSTIC magnitude, so moving the
      //  travel onto another axis cannot silently blind this again. The
      //  SELF-CHECK above drives the carrier through the production call and
      //  refuses to believe a zero it cannot prove it could have seen.
      const double a = carrier_deg(q);
      if (a > worst) worst = a;
      // THE SNAP CHECK. A channel switched OFF rather than faded out shows
      // here and nowhere else: the deaths stop calling antenna_knead at the
      // settle key, so an unfaded carrier drops from up to 45 deg to identity
      // in ONE key, at the exact instant the corpse goes still. Peak travel
      // says nothing about that -- only the step does.
      if (f > 0) {
        const double d = a - prev;
        if (std::fabs(d) > jump) { jump = std::fabs(d); jat = f; }
      }
      prev = a;
    }
    if (worst > 0.05) ++clips_with_travel;
    if (worst > bank_worst) bank_worst = worst;
    // 8 deg/key is several times the busiest smooth key in the bank and far
    // under a 45 deg switch-off, so it separates authored motion from a snap.
    const bool snap = jump > 8.0;
    if (snap) ++fails;
    std::printf("   slot %2u %4u keys   travel %6.2f deg   worst step %5.2f deg at key %4d%s%s\n",
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
  };
  int q3_fails = 0;
  std::printf("\nQ3 ROOT CONTINUITY -- largest single-key INTERIOR root step per clip, BOUNDED\n");
  std::printf("   Default ceiling %.0f mm; four clips declare their own (table in the source).\n",
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
    if (over) ++q3_fails;
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
