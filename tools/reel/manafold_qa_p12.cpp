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
//  Q3 ROOT CONTINUITY.  No gate bounds the per-key root step, so a one-key
//     teleport passes every clearance and contact check. Reports the largest
//     single-key root displacement per clip and where it lands.
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
  int clips_with_travel = 0;
  double bank_worst = 0.0;
  for (const zc::Clip& c : T.bank.clips) {
    double worst = 0.0;
    for (int f = 0; f < c.frame_count; ++f) {
      const zc::quat16& q = c.quats[static_cast<size_t>(f) * u02::kBoneCount + u02::kBEyeTravelL];
      // identity quat16 is (0,0,0,1<<14) in this codec; any travel shows as a
      // non-zero y term. Report the half-angle in degrees.
      const double y = static_cast<double>(q.q[1]) / 16384.0;
      const double a = std::asin(y > 1.0 ? 1.0 : (y < -1.0 ? -1.0 : y)) * 2.0 * 180.0 / 3.14159265358979;
      if (std::fabs(a) > worst) worst = std::fabs(a);
    }
    if (worst > 0.05) ++clips_with_travel;
    if (worst > bank_worst) bank_worst = worst;
    std::printf("   slot %2u %4u keys   max carrier rotation %6.2f deg%s\n", c.slot_id,
                c.frame_count, worst, worst <= 0.05 ? "   <-- NO TRAVEL" : "");
  }
  std::printf("   BANK: %d of %zu clips drive the eye travel; bank max %.2f deg of %d\n",
              clips_with_travel, T.bank.clips.size(), bank_worst,
              static_cast<int>(u02::kEyeTravelMaxDeg));
  if (clips_with_travel == 0) {
    std::printf("   FAIL the travel channel is built and gated but NOTHING DRIVES IT\n");
    ++fails;
  }

  // ---------------- Q3: root continuity ------------------------------------
  std::printf("\nQ3 ROOT CONTINUITY -- largest single-key root step per clip (no gate bounds this)\n");
  for (const zc::Clip& c : T.bank.clips) {
    double worst = 0.0; int at = -1;
    for (int f = 0; f + 1 < c.frame_count; ++f) {
      const double dx = (c.root[(size_t)(f + 1) * 3 + 0] - c.root[(size_t)f * 3 + 0]) / 65536.0 * 1000.0;
      const double dy = (c.root[(size_t)(f + 1) * 3 + 1] - c.root[(size_t)f * 3 + 1]) / 65536.0 * 1000.0;
      const double dz = (c.root[(size_t)(f + 1) * 3 + 2] - c.root[(size_t)f * 3 + 2]) / 65536.0 * 1000.0;
      const double m = std::sqrt(dx * dx + dy * dy + dz * dz);
      if (m > worst) { worst = m; at = f; }
    }
    std::printf("   slot %2u  worst step %7.1f mm at key %d -> %d\n", c.slot_id, worst, at, at + 1);
  }

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
