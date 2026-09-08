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
    double worst = 0.0;
    for (int f = 0; f < c.frame_count; ++f) {
      const zc::quat16& q = c.quats[static_cast<size_t>(f) * u02::kBoneCount + u02::kBEyeTravelL];
      // identity quat16 is (0,0,0,1<<14) in this codec; any travel shows as a
      // non-zero y term. SAME arithmetic as the self-check above, deliberately.
      const double a = carrier_deg(q);
      if (a > worst) worst = a;
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
  std::printf("   THE WRAP COLUMN IS THE LOOP SEAM (last key -> key 0). The site loops every\n"
              "   clip, so the seam is a frame the owner watches; nothing else measures it.\n");
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
    std::printf("   slot %2u  worst step %7.1f mm at key %d -> %d   |  WRAP %7.1f mm%s\n",
                c.slot_id, worst, at, at + 1, wrap, flag);
  }

  std::printf("\n%s: %d failure(s)%s\n", fails ? "FAIL" : "PASS", fails,
              fail_leg ? "   [FAILABLE LEG: lane 0 answered for every lane]" : "");
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
