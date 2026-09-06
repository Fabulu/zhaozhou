// manafold_c2proto — PASS 10 STAGE C.2, THE PROBE-ONLY PROTOTYPE.
//
// THE QUESTION, and nothing else: if the single 1270 mm return segment is
// SPLIT at the re-entry ball (arc 2660) into 630 + 640 with a real joint
// between them, can the loop still close inside the committed 1120 pm rim
// gate across the whole fold-scale sweep -- AND across the whole range of
// authored bend directions the knead would drive that joint through?
//
// Pass 9 proved a SINGLE aimed fold moved to arc 2660 cannot close
// (989 -> 2401 pm, arm length and anchor swept, no help). This prototype
// tests the redesign that result named, WITHOUT touching the rig, the mesh
// or any shipped constant: pure pose arithmetic, exactly as the architecture
// requires ("run that prototype FIRST THING in the pass"), so its answer
// arrives before the schedule depends on it.
//
// ---- WHY A TWO-LINK SOLVE HAS A FREE PARAMETER, AND WHY THAT IS THE POINT
//
// Aiming BOTH segments at the anchor reproduces today's straight strut
// exactly (segment 2 continues segment 1 direction, so the joint is
// geometrically inert -- the reviewer's "dead-straight leg" with an extra
// bone in it). The bend has to come from somewhere, and closure has to
// survive it. The two-link solve gives both, in closed form:
//
//   * the ELBOW ANGLE is not free -- it is determined by the distance d from
//     hinge D pivot to the posed anchor (law of cosines). That is what
//     makes the tip land ON the anchor instead of wherever 1270 mm along an
//     aim happens to fall.
//   * the ELBOW ROLL about the D->anchor axis IS free. That is the knob
//     the knead authors: the strut bends at the ball, in an authored
//     direction, and the closure is exact for every value of it.
//
// So the gate question is a SWEEP over that roll, not a sample of it
// (09-ENGINE-GOTCHAS section 17: a gate that samples the corners cannot see
// a minimum in the middle -- here, a maximum).
//
// ---- THE INSTRUMENT VALIDATES ITSELF FIRST (section 16, and checklist 4/6)
//
// This file reconstructs ring positions analytically instead of calling
// skin_vertex, because the bone it would skin to does not exist yet. A new
// instrument whose number nobody has corroborated is exactly the trap that
// cost pass 7 a day. So leg (1) below reconstructs the CURRENT single-segment
// arm the same analytic way and must reproduce the committed probe published
// 989 pm sweep worst. If it does not, every other number here is void and the
// run says so and exits.
//
// Legs (2) and (3) are compared LIKE WITH LIKE: both sweep the rim offset
// around the full circle at the terminal blade radius (a conservative upper
// bound on the anisotropic ellipsoid), so the prototype is never flattered by
// a measurement the control did not get.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <array>
#include <algorithm>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"

namespace zc = zref::creature;
#include "manafold.h"

namespace {

struct V3 { int64_t x, y, z; };

inline V3 sub(const V3& a, const V3& b) { return V3{a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 add(const V3& a, const V3& b) { return V3{a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 cross(const V3& a, const V3& b) {
  return V3{(a.y * b.z - a.z * b.y) >> 16, (a.z * b.x - a.x * b.z) >> 16,
            (a.x * b.y - a.y * b.x) >> 16};
}
inline int64_t len(const V3& a) { return u02::isqrt64(a.x * a.x + a.y * a.y + a.z * a.z); }
inline V3 scal(const V3& a, int64_t num, int64_t den) {
  if (den == 0) return V3{0, 0, 0};
  return V3{a.x * num / den, a.y * num / den, a.z * num / den};
}
// angle16 -> Q16.16 sine/cosine, through the house table (never a new one)
inline int64_t s16(int32_t a16) {
  return zref::fx_sin(zref::angle16{static_cast<uint16_t>(a16 & 0xFFFF)}).raw;
}
inline int64_t c16(int32_t a16) {
  return zref::fx_cos(zref::angle16{static_cast<uint16_t>(a16 & 0xFFFF)}).raw;
}

inline V3 unit(const V3& a) {
  const int64_t l = len(a);
  if (l == 0) return V3{0, 0, 0};
  return V3{(a.x << 16) / l, (a.y << 16) / l, (a.z << 16) / l};
}

// apply a bone matrix to a point (mat3x4fx is a flat int32_t[12], row-major)
inline V3 xform(const zc::mat3x4fx& m, int64_t x, int64_t y, int64_t z) {
  return V3{((int64_t)m.m[0] * x + (int64_t)m.m[1] * y + (int64_t)m.m[2] * z) / 65536 + m.m[3],
            ((int64_t)m.m[4] * x + (int64_t)m.m[5] * y + (int64_t)m.m[6] * z) / 65536 + m.m[7],
            ((int64_t)m.m[8] * x + (int64_t)m.m[9] * y + (int64_t)m.m[10] * z) / 65536 + m.m[11]};
}
inline V3 xdir(const zc::mat3x4fx& m, int64_t x, int64_t y, int64_t z) {
  return V3{((int64_t)m.m[0] * x + (int64_t)m.m[1] * y + (int64_t)m.m[2] * z) / 65536,
            ((int64_t)m.m[4] * x + (int64_t)m.m[5] * y + (int64_t)m.m[6] * z) / 65536,
            ((int64_t)m.m[8] * x + (int64_t)m.m[9] * y + (int64_t)m.m[10] * z) / 65536};
}
// world -> root-local (the frame the committed ellipsoid test uses)
inline V3 to_root(const zc::mat3x4fx& rm, const V3& p) {
  const int64_t dx = p.x - rm.m[3], dy = p.y - rm.m[7], dz = p.z - rm.m[11];
  return V3{((int64_t)rm.m[0] * dx + (int64_t)rm.m[4] * dy + (int64_t)rm.m[8] * dz) >> 16,
            ((int64_t)rm.m[1] * dx + (int64_t)rm.m[5] * dy + (int64_t)rm.m[9] * dz) >> 16,
            ((int64_t)rm.m[2] * dx + (int64_t)rm.m[6] * dy + (int64_t)rm.m[10] * dz) >> 16};
}
inline V3 to_root_dir(const zc::mat3x4fx& rm, const V3& v) {
  return V3{((int64_t)rm.m[0] * v.x + (int64_t)rm.m[4] * v.y + (int64_t)rm.m[8] * v.z) >> 16,
            ((int64_t)rm.m[1] * v.x + (int64_t)rm.m[5] * v.y + (int64_t)rm.m[9] * v.z) >> 16,
            ((int64_t)rm.m[2] * v.x + (int64_t)rm.m[6] * v.y + (int64_t)rm.m[10] * v.z) >> 16};
}

const int32_t kRx = u02::fxu(u02::kBodyRadiusMm);
const int32_t kRy = u02::fxu(u02::vmm(u02::kBodyRadiusMm));

// the committed ellipsoid reading, in per-mille of the body surface
inline int32_t ellip_pm(const V3& p_root) {
  const int64_t ex = (p_root.x << 16) / kRx;
  const int64_t ey = (p_root.y << 16) / kRy;
  const int64_t ez = (p_root.z << 16) / kRx;
  return static_cast<int32_t>((u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
}

constexpr int32_t kTotalArc = u02::kLoopBuryMm + u02::kLoopArcMm[0] + u02::kLoopArcMm[1] +
                              u02::kLoopArcMm[2] + u02::kLoopArcMm[3] + u02::kLoopArcMm[4] +
                              u02::kLoopArcMm[5];
constexpr int32_t kArcD = u02::kLoopBuryMm + u02::kLoopArcMm[0] + u02::kLoopArcMm[1] +
                          u02::kLoopArcMm[2] + u02::kLoopArcMm[3] + u02::kLoopArcMm[4];
constexpr int32_t kArcReentry = u02::kKnuckleAtEndMm;   // 2660, the re-entry ball
constexpr int32_t kSeg1Mm = kArcReentry - kArcD;        // 630
constexpr int32_t kSeg2Mm = kTotalArc - kArcReentry;    // 640
constexpr int32_t kRimMm = 70;  // kLoopBladeRxMm[6]; the terminal blade radius
constexpr int32_t kRimGate = 1120;

// The three terminal rings, as distances measured BACK from the arm tip --
// replicated from make_loop ring law exactly as the committed probe does.
inline int32_t ring_back_mm(int ri) {
  const int32_t s =
      static_cast<int32_t>((static_cast<int64_t>(kTotalArc) * ri) / (u02::kLoopRings - 1));
  return kTotalArc - s;
}

// Worst ellipsoid reading over one straight terminal run: tip is the arm end,
// axis the unit direction the tube points along, and the rim is swept all the
// way round rather than sampled at four corners.
int32_t worst_on_run(const V3& tip, const V3& axis, const V3& e1, const V3& e2,
                     int rim_steps) {
  int32_t worst = 0;
  for (int ri = u02::kLoopRings - 3; ri < u02::kLoopRings; ++ri) {
    const int32_t back = ring_back_mm(ri);
    const V3 c = sub(tip, scal(axis, u02::fxu(back), 65536));
    const int32_t ec = ellip_pm(c);
    if (ec > worst) worst = ec;
    for (int k = 0; k < rim_steps; ++k) {
      const int32_t a16 = static_cast<int32_t>(65536LL * k / rim_steps);
      const int64_t cs = c16(a16), sn = s16(a16);
      const V3 off = add(scal(e1, cs * u02::fxu(kRimMm) / 65536, 65536),
                         scal(e2, sn * u02::fxu(kRimMm) / 65536, 65536));
      const int32_t e = ellip_pm(add(c, off));
      if (e > worst) worst = e;
    }
  }
  return worst;
}

}  // namespace

int main() {
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::printf("c2proto: FAIL compile produced no meshlets\n");
    return 1;
  }
  std::printf("c2proto: split the %d mm return arm at the re-entry ball (arc %d): "
              "seg1 %d mm + seg2 %d mm; rim gate %d pm\n",
              u02::kLoopArcMm[5], kArcReentry, kSeg1Mm, kSeg2Mm, kRimGate);

  const int kSteps = 24;
  const int kRollSteps = 48;   // the authored bend direction, SWEPT (section 17)
  const int kRimSteps = 24;    // the rim, swept round rather than at 4 corners

  int32_t ctl_exact_worst = 0, ctl_exact_pm = 0;
  int32_t ctl_circ_worst = 0, ctl_circ_pm = 0;
  int32_t proto_worst = 0, proto_pm = 0, proto_roll = 0;
  int unreachable = 0;
  int32_t d_min = INT32_MAX, d_max = 0;
  // leg 4: the BOUNDED AUTHORED BEND form -- worst rim per bend angle
  const int kBendSteps = 25;          // 0..60 degrees in 2.5 degree steps
  int32_t bend_worst[kBendSteps];
  for (int b = 0; b < kBendSteps; ++b) bend_worst[b] = 0;

  for (int i = 0; i < kSteps; ++i) {
    const int32_t pm = 700 + (1160 - 700) * i / (kSteps - 1);
    u02::Rig g;
    g.reset();
    u02::loop_pose(g, pm, pm, pm, pm);
    u02::face_rest(g);
    zc::Clip sweep = u02::clip_shell(7, 1, u02::kHoverHeightMm);
    g.write(sweep, 0);
    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zc::decode_pose(T, sweep, 0, pose, nullptr, 0);
    const zc::mat3x4fx& rm = pose[u02::kBRoot];

    // hinge D posed pivot and posed frame, from the REAL decoded pose
    const V3 bindD{u02::fxu(u02::kLoopTubeXMm),
                   u02::fxu(u02::kLoopNeckExitYMm + kArcD - u02::kLoopBuryMm), 0};
    const V3 pD = to_root(rm, xform(pose[u02::kBHingeD], bindD.x, bindD.y, bindD.z));
    const V3 axY = to_root_dir(rm, xdir(pose[u02::kBHingeD], 0, 65536, 0));
    const V3 axX = to_root_dir(rm, xdir(pose[u02::kBHingeD], 65536, 0, 0));
    const V3 axZ = to_root_dir(rm, xdir(pose[u02::kBHingeD], 0, 0, 65536));

    // the POSED re-entry anchor (C.1 construction: the bind offset carried by
    // kBLoopBase2 own rotation about the body). Identity here -- the sweep
    // drives no knead -- which is exactly what keeps this comparison
    // like-for-like against pass 9 published number.
    int32_t ax, ay, az;
    u02::quat_rot_vec(g.q[u02::kBLoopBase2], u02::kLoopReentryXMm, u02::kLoopReentryYMm, 0,
                      ax, ay, az);
    const V3 anchor{u02::fxu(ax), u02::fxu(ay), u02::fxu(az)};

    // ---- (1) CONTROL, exact posed frame: must reproduce the committed 989 pm
    {
      const V3 tip = add(pD, scal(axY, u02::fxu(u02::kLoopArcMm[5]), 65536));
      const int32_t e = worst_on_run(tip, axY, axX, axZ, 4);  // 4 = committed corners
      if (e > ctl_exact_worst) { ctl_exact_worst = e; ctl_exact_pm = pm; }
    }
    // ---- (2) CONTROL, rim swept round: the like-for-like baseline
    {
      const V3 tip = add(pD, scal(axY, u02::fxu(u02::kLoopArcMm[5]), 65536));
      const int32_t e = worst_on_run(tip, axY, axX, axZ, kRimSteps);
      if (e > ctl_circ_worst) { ctl_circ_worst = e; ctl_circ_pm = pm; }
    }
    // ---- (3) THE PROTOTYPE: two-link exact-reach closure, bend direction swept
    {
      const V3 v = sub(anchor, pD);
      const int64_t d = len(v);
      const int32_t d_mm = static_cast<int32_t>(d * 1000 / 65536);
      if (d_mm < d_min) d_min = d_mm;
      if (d_mm > d_max) d_max = d_mm;
      const int64_t L1 = u02::fxu(kSeg1Mm), L2 = u02::fxu(kSeg2Mm);
      const int64_t reach_min = L1 > L2 ? L1 - L2 : L2 - L1;
      // NOT `continue` -- leg 4 below must still run for these fold scales.
      // It once was, and it silently measured Form B over only the 15
      // REACHABLE folds, missing fold 700 where the worst rim actually lives:
      // Form B read 545 pm at zero bend where the identical straight-strut
      // control read 991. Two numbers that must be equal by construction were
      // not, which is the only reason it was caught (gotcha section 16).
      if (d > L1 + L2 || d < reach_min) {
        ++unreachable;
      } else {
      const V3 uu = unit(v);
      // a = (d^2 + L1^2 - L2^2) / 2d ; h = sqrt(L1^2 - a^2)   -- all Q16.16
      const int64_t a = ((d * d + L1 * L1 - L2 * L2) / 65536) * 65536 / (2 * d);
      const int64_t h2 = L1 * L1 - a * a;
      const int64_t h = h2 > 0 ? u02::isqrt64(h2) : 0;
      V3 seed = (std::llabs(uu.x) < std::llabs(uu.y) && std::llabs(uu.x) < std::llabs(uu.z))
                    ? V3{65536, 0, 0}
                    : (std::llabs(uu.y) < std::llabs(uu.z) ? V3{0, 65536, 0} : V3{0, 0, 65536});
      const V3 e1 = unit(cross(uu, seed));
      const V3 e2 = unit(cross(uu, e1));
      for (int r = 0; r < kRollSteps; ++r) {
        const int32_t a16 = static_cast<int32_t>(65536LL * r / kRollSteps);
        const int64_t cs = c16(a16), sn = s16(a16);
        const V3 w = add(scal(e1, cs, 65536), scal(e2, sn, 65536));
        const V3 elbow = add(pD, add(scal(uu, a, 65536), scal(w, h, 65536)));
        // segment 2 runs elbow -> anchor, and THE TIP LANDS ON THE ANCHOR
        const V3 s = unit(sub(anchor, elbow));
        V3 sd = (std::llabs(s.x) < std::llabs(s.y) && std::llabs(s.x) < std::llabs(s.z))
                    ? V3{65536, 0, 0}
                    : (std::llabs(s.y) < std::llabs(s.z) ? V3{0, 65536, 0} : V3{0, 0, 65536});
        const V3 f1 = unit(cross(s, sd));
        const V3 f2 = unit(cross(s, f1));
        const int32_t e = worst_on_run(anchor, s, f1, f2, kRimSteps);
        if (e > proto_worst) { proto_worst = e; proto_pm = pm; proto_roll = a16; }
      }
      }  // end of the reachable branch
    }
    // ---- (4) THE FORM THAT IS ACTUALLY REACHABLE: hinge D aims at the anchor
    // EXACTLY as it does today (so closure is untouched at every fold scale),
    // and the re-entry joint carries a BOUNDED AUTHORED BEND. The strut bends
    // at the ball because the knead says so, not because the arithmetic had a
    // residual to spend -- and the arm keeps the reach it has always had.
    {
      const V3 pR = add(pD, scal(axY, u02::fxu(kSeg1Mm), 65536));
      for (int b = 0; b < kBendSteps; ++b) {
        const int32_t bend_a16 = static_cast<int32_t>(65536LL * b * 25 / 3600);  // 2.5 deg steps
        const int64_t cb = c16(bend_a16), sb = s16(bend_a16);
        for (int r = 0; r < kRollSteps; ++r) {
          const int32_t a16 = static_cast<int32_t>(65536LL * r / kRollSteps);
          const V3 perp = add(scal(axX, c16(a16), 65536), scal(axZ, s16(a16), 65536));
          // segment 2 = axY tilted by `bend` toward `perp`
          const V3 s2 = unit(add(scal(axY, cb, 65536), scal(perp, sb, 65536)));
          const V3 tip = add(pR, scal(s2, u02::fxu(kSeg2Mm), 65536));
          V3 sd = (std::llabs(s2.x) < std::llabs(s2.y) && std::llabs(s2.x) < std::llabs(s2.z))
                      ? V3{65536, 0, 0}
                      : (std::llabs(s2.y) < std::llabs(s2.z) ? V3{0, 65536, 0} : V3{0, 0, 65536});
          const V3 g1 = unit(cross(s2, sd));
          const V3 g2 = unit(cross(s2, g1));
          const int32_t e = worst_on_run(tip, s2, g1, g2, kRimSteps);
          if (e > bend_worst[b]) bend_worst[b] = e;
        }
      }
    }
  }

  std::printf("c2proto: hinge D pivot -> posed anchor distance over the sweep: "
              "%d..%d mm (two-link reach %d..%d mm) -- %d of %d fold scales UNREACHABLE\n",
              d_min, d_max, kSeg2Mm - kSeg1Mm, kSeg1Mm + kSeg2Mm, unreachable, kSteps);

  // ---- the self-validation, BEFORE any verdict is drawn from leg 3
  const int32_t kPublished = 989;
  const int32_t drift = ctl_exact_worst > kPublished ? ctl_exact_worst - kPublished
                                                     : kPublished - ctl_exact_worst;
  const bool instrument_ok = drift <= 15;
  std::printf("c2proto: [1] CONTROL (single segment, exact posed frame, 4 rim corners) "
              "worst %d pm at fold %d -- committed probe publishes %d pm; drift %d -- %s\n",
              ctl_exact_worst, ctl_exact_pm, kPublished, drift,
              instrument_ok ? "INSTRUMENT CORROBORATED" : "INSTRUMENT NOT TRUSTED");
  if (!instrument_ok) {
    std::printf("c2proto: the analytic reconstruction does not reproduce the committed "
                "number, so legs 2 and 3 are VOID. No C.2 verdict from this run.\n");
    return 2;
  }
  std::printf("c2proto: [2] CONTROL (single segment, rim swept %d ways) worst %d pm at fold %d\n",
              kRimSteps, ctl_circ_worst, ctl_circ_pm);
  std::printf("c2proto: [3] PROTOTYPE (two-link, tip ON the anchor, bend direction swept "
              "%d ways, rim swept %d ways) worst %d pm at fold %d, bend roll %d a16\n",
              kRollSteps, kRimSteps, proto_worst, proto_pm, proto_roll);

  const bool residual_go = proto_worst <= kRimGate && unreachable == 0;
  std::printf("c2proto: FORM A (residual closure, tip ON the anchor) -- %s: the rim is "
              "COMFORTABLE (%d pm vs gate %d) but the arm CANNOT REACH at %d of %d fold "
              "scales. Hinge D sits up to %d mm from the anchor and 630+640 reaches %d. "
              "Closure quality is not the blocker; REACH is.\n",
              residual_go ? "GO" : "NO-GO", proto_worst, kRimGate, unreachable, kSteps,
              d_max, kSeg1Mm + kSeg2Mm);

  // ---- FORM B: the form that keeps the reach the arm has always had.
  // Hinge D aims at the anchor exactly as it does today -- so closure is
  // untouched at every fold scale, including the nine Form A cannot serve --
  // and the re-entry joint carries a BOUNDED AUTHORED BEND. The strut bends at
  // the ball because the knead says so, not because the arithmetic had a
  // residual to spend. The question becomes: how much bend fits under the gate?
  // THE IDENTITY THAT MUST HOLD, asserted rather than assumed: Form B at ZERO
  // bend is 630 mm + 640 mm along the same axis = the 1270 mm straight strut,
  // so it must reproduce control leg 2 exactly. This check is the only reason
  // the subset bug above was found; it stays so the next change cannot lose it.
  const int32_t id_drift = bend_worst[0] > ctl_circ_worst ? bend_worst[0] - ctl_circ_worst
                                                          : ctl_circ_worst - bend_worst[0];
  std::printf("c2proto: IDENTITY CHECK Form B at 0 bend = %d pm vs straight-strut control "
              "%d pm; drift %d -- %s\n",
              bend_worst[0], ctl_circ_worst, id_drift,
              id_drift <= 4 ? "OK" : "BROKEN (Form B numbers are VOID)");
  if (id_drift > 4) {
    std::printf("c2proto: Form B is not measuring the same arm as the control. No verdict.\n");
    return 2;
  }

  int32_t best_deg_x10 = -1;
  std::printf("c2proto: FORM B (D aims as today; bounded authored bend AT the ball)\n");
  for (int b = 0; b < kBendSteps; ++b) {
    const int32_t deg_x10 = b * 25;
    const bool ok = bend_worst[b] <= kRimGate;
    if (ok) best_deg_x10 = deg_x10;
    if (b % 4 == 0 || !ok)
      std::printf("c2proto:   bend %5.1f deg -> worst rim %4d pm  %s\n", deg_x10 / 10.0,
                  bend_worst[b], ok ? "OK" : "OVER GATE");
    if (!ok) break;
  }
  std::printf("c2proto: FORM B HEADROOM: the re-entry joint can carry up to %.1f degrees of "
              "authored bend, in ANY direction, at EVERY fold scale, and still hold the "
              "%d pm rim gate (straight-strut baseline %d pm).\n",
              best_deg_x10 / 10.0, kRimGate, ctl_circ_worst);

  // 10 degrees is the floor for the bend to READ at 240p on a 70 mm-radius
  // strut; below that this would be a joint nobody can see, which is the
  // dead-straight leg with extra arithmetic.
  const bool go = best_deg_x10 >= 100;
  std::printf("c2proto: VERDICT %s -- Form A aborts on REACH; Form B carries %.1f deg "
              "(pass 9 single-joint-at-2660 was 2401 pm against the same %d pm gate)\n",
              go ? "GO, via FORM B" : "NO-GO (C.2 ABORTS; C.1 ships alone, gap stays declared)",
              best_deg_x10 / 10.0, kRimGate);
  return go ? 0 : 1;
}
