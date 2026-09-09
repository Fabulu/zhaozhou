// manafold_spangate.cpp -- THE COMMITTED STRETCHY-SPAN GATE
// (Manafold pass 12 wave 2a, Owner Direction 9 SS13.)
//
// WHY THIS EXISTS, and why the gate that looked like it already covered this
// could not:
//
//   "can we make the antennae parts between the blobs stretchy? That'd have to
//    stretch the bones too when they stretch. But it'd be awesome. Possible?"
//   "Alright, make them stretchy so the balls can move further apart and become
//    more expressive"
//
// SS13.3 item 2 asks for the stretch to be COMPUTED FROM THE POSED INTER-NODULE
// DISTANCE. Wave 1 could not: `DeformSample` was one {flatten, spread} per key
// per clip, so every span stretched together on the body's breath, and wave 1
// wrote that limitation down rather than hiding it. Wave 2a builds the deform
// LANES and drives each span from its own posed shortfall.
//
// *** THE BLIND SPOT THIS GATE EXISTS TO CLOSE ***
//
// The stretch is a VERTEX effect and every gate that reads a nodule reads a
// BONE. `manafold_nodule.cpp`'s `posed_ball()` skins a synthetic vertex sitting
// at the bone's own bind origin with NO deform metadata attached, so it is
// structurally incapable of seeing this feature: it reported ball A's vertical
// reach as -7..+3 mm with the stretch fully live. That is not a fault in that
// gate -- a bone gate should measure bones, and its independence verdict is
// exactly right. It is the same blind spot wave 1 named for the closure probe
// ("that probe measures BONE geometry and this is a VERTEX effect ... named
// here so the next person does not trust silence").
//
// So this gate reads the SKIN, through the whole shipping path:
//     u02::type()                        the compiled CreatureType
//     zc::decode_pose(T, clip, f, pose)  the production decoder
//     zc::deformation_frame(...)         the production lane resolve
//     zc::deform_skin_vertex_lanes(...)  the production deform
//     zc::skin_vertex(pose, sv, ...)     the production skinner
// and a "ball" here is the CENTROID OF THE ACTUAL SKIN in a window around its
// bind station -- the surface the owner looks at, not a proxy for it.
//
// *** THE FOUR CHECKS ***
//   G1  REST IS BIT-IDENTICAL. Every deform vertex, lanes all zero: the deform
//       must return the vertex unchanged. That contract is the whole licence
//       for having touched a shared engine struct at all.
//   G2  THE SKIN NEVER FOLDS BACK. Past hinge C every lane's authority ramps
//       to zero, and that ramp is the only place the bind-y map can lose
//       monotonicity. Walked at the CEILING (kSpanStretchMaxPm on all three
//       spans at once), which is the worst case any clip can reach.
//   G3  THE BURIED ARM TIP DOES NOT MOVE. Wave 1's render showed a tube stub
//       poking out of the ball's lower right when the whole chain grew; the
//       ramp exists to stop that, and this is what holds it there.
//   G4  THE STRETCH IS VISIBLE, NOT MERELY PRESENT. Ball A's vertical SKIN
//       reach on the solo diagnostic, measured with the lanes live and again
//       with their authority ablated, and the difference must clear a declared
//       floor. This is CLAUDE.md's crayon-grain failure written as a check:
//       "mathematically present and visually invisible" passes G1, G2 and G3
//       and is still wrong.
//
// *** THE FAILABLE LEGS (checklist item 10) *** -- each removes the mechanism
// from the SAME code path the verdict is taken on, and each was witnessed
// failing before this file was committed:
//   --fail-nolanes         zero every ring's lane authority   -> G4 fails
//   --fail-steepramp <n>   taper^n past hinge C               -> G2 fails
//   --fail-ceiling <pm>    walk G2/G3 at a different stretch  (capped at 1000)
//   --fail-noramp          restore authority past hinge C     -> G3 fails
//
// Usage:
//   manafold-spangate.exe                 rc 0 pass, rc 1 fail
//   manafold-spangate.exe --fail-nolanes  failable leg 1 (expects FAIL)
//   manafold-spangate.exe --fail-steepramp 5   failable leg 2 (expects FAIL)
//   manafold-spangate.exe --fail-noramp   failable leg 3 (expects FAIL)
//   manafold-spangate.exe --csv           per-key ball track through the skin

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

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

// ---- the declared floors, both named --------------------------------------
//
// G4's floor, and it is a READ threshold, not a measurement. Ball A's span
// points essentially straight up (the nodule gate measures its direction as
// (-0.13, +0.99, +0.04)), so before the stretch a 200 mm vertical request moved
// the skin about as little as it moved the bone. What counts as delivered is
// what is visible at 240p -- a couple of pixels of antenna travel -- and a gain
// under this is the crayon-grain failure wearing a passing gate.
constexpr double kReachGainMinMm = 60.0;
// G3. The arm tip is buried inside the body; anything above rounding is the
// stub coming back out through the ball's lower right.
constexpr double kTipMoveMaxMm = 1.0;
// G5's floor. G4 proves the stretch on the DIAGNOSTIC, whose amplitudes sit
// at the ceiling on purpose. The owner never watches the diagnostic.
// `taunt3` is the shipped clip whose performance IS the per-nodule
// vocabulary and whose shrug is the owner's own configuration, so ball A's
// vertical travel THERE is the thing that was actually asked for. 45 mm is
// about 6 px at native -- the same read threshold kOpposeMinMm uses in the
// nodule gate, and well clear of the 12 mm band that gate calls dither.
constexpr double kShippedReachMinMm = 45.0;

// ---- G6's constants (PASS 15, Direction 11 §3) ----------------------------
//
// THE SUBJECT SCALE, and it is INHERITED rather than measured here. The nodule
// gate documents 12 mm as "~1.7 px at native" and 40 mm as "about 6 px"; both
// give 7 mm per native pixel, from two constants authored independently, so
// the figure is corroborated rather than asserted (09-ENGINE-GOTCHAS §16: do
// not act on a number until something else reproduces it).
// ⚠ It is a BANK-SCALE figure. A close-up subject reads larger and this
// conversion does not apply to it; the gate walks the shipped bank only.
constexpr int kMmPerNativePx = 7;

// THE MEASURED NOISE FLOOR. A "ball" here is the CENTROID of the skin in a
// window around its bind station, and a centroid inside a bending section is
// pulled toward the inside of the curve -- so even with perfectly rigid spans
// and the stretch fully ablated, the measured distance wanders a little. On the
// known-negative leg that residual is 13.3 mm, about 1.9 native pixels. It is
// geometry, not leakage, and it is recorded here rather than hidden because the
// gate's whole credibility is the gap between this number and the beat floor.
constexpr double kDiffNoiseFloorMm = 13.3;
// The known-negative's ceiling, set ABOVE the measured floor with room, and far
// below the beat floor. It is not fitted to its answer: it exists to catch the
// metric drifting back toward reading the carrier, which is exactly what the
// first two versions of this gate did (155 mm and 973 mm on this same leg).
constexpr double kDiffNegMaxMm = 20.0;

// The floor for a clip that carries an AUTHORED ball beat. The target was
// "at least 10 px for a gesture clip" -- 70 mm at the scale above -- and the
// three gated clips measure 83.5, 79.1 and 89.0 mm (11.9, 11.3, 12.7 px).
// ⚠ SET BELOW WHAT WAS MEASURED, ON PURPOSE. 07-MOTION-STYLE §6: two gates have
// already shipped here with 6 mm and 1 mm of headroom because the band was
// re-recorded to admit the measurement, which is a gate fitted to its own answer
// and fails on the next legitimate re-timing. 65 mm keeps 14-24 mm of room and
// still sits 5x above the noise floor, so "present" and "absent" are not close.
constexpr double kDiffBeatMinMm = 65.0;

// The clips that carry an authored ball beat and are therefore GATED. This list
// is the switch as well as the gate (10-GATE-CHECKLIST item 39: an opt-in fix
// has TWO landings, and the second is the one that gets forgotten) -- adding a
// beat to a clip without adding it here leaves the beat ungated, and removing
// a beat without removing it here fails loudly, which is the correct direction
// for that mistake to break in.
constexpr uint16_t kBeatSlots[] = {0, 2, u02::kTaunt3Slot};
constexpr int kBeatSlotCount = static_cast<int>(sizeof(kBeatSlots) / sizeof(kBeatSlots[0]));

int g_fail = 0;
bool g_no_lanes = false;
bool g_no_ramp = false;
int32_t g_stC_bind_y = 0;  // bind y of hinge C, fx16 -- the ramp's start
int32_t g_stC_mm = 0, g_total_mm = 0;
// The stretch G2/G3 walk at. Defaults to the AUTHORED ceiling; --fail-ceiling
// raises it past the monotonicity bound, which is a configuration one edit to
// kSpanStretchMaxPm could really produce -- so that leg fails through the real
// compiled ring authorities and the real deform, nothing reconstructed.
int32_t g_ceiling_pm = u02::kSpanStretchMaxPm;
// G2's failable leg. The taper past hinge C is LINEAR in production; this
// raises it to the Nth power, which is the shape of authoring change that could
// really reach this check (someone makes the arm "settle more smoothly"). A
// steeper taper sheds authority faster than 1/s and that is the one way the
// bind-y map can run backwards. See the note beside G2 for why the CEILING
// cannot reach it: no representable lane value can.
int g_steep = 1;

void fail(const char* what) {
  std::printf("  FAIL: %s\n", what);
  ++g_fail;
}

/** Loop-chain deform metadata is identifiable WITHOUT a part index: the antenna
 *  is the only part authored on local axis 0 (the body ball is axis 1). Named
 *  as a predicate because "meshlet 3 is the loop" is exactly the index-into-a-
 *  list assertion that rots the next time a part is added. */
bool is_loop_deform(const zc::DeformVertex& d) {
  return d.role != zc::DeformRole::kNone && d.axis == 0;
}

/** Apply the failable legs to one vertex's authored metadata, IN THE PRODUCTION
 *  STRUCT, so an ablation is a real change to what the renderer reads rather
 *  than a simulation of one. `bind_y` selects the ramp region for --fail-noramp
 *  the same way make_loop's own `s > stC` test does. */
zc::DeformVertex legged(const zc::DeformVertex& in, int32_t bind_y, bool in_loop_meshlet) {
  zc::DeformVertex d = in;
  if (g_no_lanes && is_loop_deform(d)) {
    for (uint8_t i = 0; i + 1 < zc::kDeformLaneCount; ++i) d.strength_ex[i] = 0;
    return d;
  }
  if (g_steep > 1 && in_loop_meshlet && bind_y > g_stC_bind_y) {
    // ramp -> ramp^N, applied to the COMPILED authority, so the production
    // linear taper is what is being modified rather than re-derived.
    const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
    const double s = static_cast<double>(bind_y) * 1000.0 / 65536.0 - y0;
    const double total = static_cast<double>(g_total_mm);
    const double stC = static_cast<double>(g_stC_mm);
    double ramp = (total - s) / (total - stC);
    if (ramp < 0) ramp = 0;
    if (ramp > 1) ramp = 1;
    double f = 1.0;
    for (int k = 1; k < g_steep; ++k) f *= ramp;
    for (uint8_t i = 0; i + 1 < zc::kDeformLaneCount; ++i)
      d.strength_ex[i] = static_cast<uint8_t>(d.strength_ex[i] * f);
    return d;
  }
  if (!g_no_ramp || !in_loop_meshlet || bind_y <= g_stC_bind_y) return d;
  // ---- LEG 2: restore what the ramp took away ---------------------------
  //
  // ⚠ THIS LEG RECONSTRUCTS PRODUCTION CONSTANTS ON PURPOSE, which everywhere
  // else in this file would be the checklist-item-10 sin of measuring against a
  // same-named copy instead of the renderer's own symbol. Here it is the point:
  // the leg's job is to MANUFACTURE the known-bad configuration wave 1 rendered
  // and rejected, and that configuration does not exist in the compiled data to
  // be read out. The ramp does not merely reduce authority past hinge C, it
  // takes every lane to zero -- and a ring with no authority on any lane is
  // compiled with role kNone and a zeroed centre, so there is nothing left to
  // scale up. The tip rings have to be brought back to life to prove that G3
  // would notice them moving.
  const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  const double s = static_cast<double>(bind_y) * 1000.0 / 65536.0 - y0;
  const int32_t L[3] = {u02::kLoopArcMm[1], u02::kLoopArcMm[2], u02::kLoopArcMm[3]};
  d.role = zc::DeformRole::kRadial;
  d.axis = 0;
  d.center_x = u02::fxu(u02::kLoopTubeXMm);
  d.center_y = u02::fxu(y0);
  d.center_z = 0;
  for (int i = 0; i < 3; ++i) {
    double st = s > 0 ? 255.0 * L[i] / s : 0.0;
    if (st > 255.0) st = 255.0;
    d.strength_ex[i] = static_cast<uint8_t>(st);
  }
  return d;
}

/** The lane frame at the authored CEILING -- the worst case any clip reaches. */
zc::DeformFrame ceiling_frame() {
  zc::DeformFrame fr;
  // ⚠ A LANE SAMPLE IS u16, SO 1000 pm IS THE STRUCTURAL MAXIMUM -- 65535/65536
  // of extra length and not one part more. Found by asking --fail-ceiling for
  // 1200 and watching the gate pass: 1200 pm is 78643, which wrapped to 13107
  // and quietly walked the check at 200 pm, LOWER than the authored ceiling. A
  // leg that silently tests less than the default is worse than no leg, so the
  // clamp is explicit and the cap is stated wherever the number is used.
  int32_t spread = static_cast<int32_t>((static_cast<int64_t>(g_ceiling_pm) * 65536) / 1000);
  if (spread > 65535) spread = 65535;
  int32_t flat = static_cast<int32_t>((static_cast<int64_t>(spread) * u02::kSpanThinRatioPm) / 1000);
  if (flat > 65535) flat = 65535;
  for (int i = 1; i <= 3; ++i)
    fr.lane[i] = zc::DeformSample{static_cast<uint16_t>(flat), static_cast<uint16_t>(spread)};
  return fr;
}

struct Vec3 {
  double x = 0, y = 0, z = 0;
};

/** Root-local posed position of one already-deformed skin vertex. */
Vec3 posed_skin(const std::array<zc::mat3x4fx, zc::kMaxBones>& pose, const zc::SkinVertex& v) {
  int32_t wx, wy, wz;
  zc::skin_vertex(pose.data(), v, wx, wy, wz, nullptr);
  const zc::mat3x4fx& rm = pose[u02::kBRoot];
  const int64_t dx = wx - rm.m[3], dy = wy - rm.m[7], dz = wz - rm.m[11];
  Vec3 o;
  o.x = static_cast<double>(((rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16) * 1000 >> 16);
  o.y = static_cast<double>(((rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16) * 1000 >> 16);
  o.z = static_cast<double>(((rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16) * 1000 >> 16);
  return o;
}

/** The SKIN centroid of the loop rings nearest bind station `s_mm`, posed and
 *  deformed through the production path. One ring step is ~73 mm. */
Vec3 ball_skin(const zc::CreatureType& T, const zc::Clip& clip, uint16_t f, int32_t s_mm,
               int32_t half_mm) {
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(T, clip, f, pose, nullptr, 0);
  const zc::DeformFrame fr = zc::deformation_frame(T, clip.slot_id, f, 0);
  const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  const int32_t want = u02::fxu(y0 + s_mm);
  const int32_t win = u02::fxu(half_mm);
  Vec3 acc;
  int n = 0;
  for (const zc::Meshlet& m : T.mesh) {
    if (m.deform.empty()) continue;
    for (size_t vi = 0; vi < m.verts.size(); ++vi) {
      if (!is_loop_deform(m.deform[vi])) continue;
      const int32_t by = m.verts[vi].y;
      if (by < want - win || by > want + win) continue;
      const zc::SkinVertex sv =
          zc::deform_skin_vertex_lanes(m.verts[vi], legged(m.deform[vi], by, true), fr);
      const Vec3 p = posed_skin(pose, sv);
      acc.x += p.x;
      acc.y += p.y;
      acc.z += p.z;
      ++n;
    }
  }
  if (n > 0) {
    acc.x /= n;
    acc.y /= n;
    acc.z /= n;
  }
  return acc;
}

}  // namespace

int main(int argc, char** argv) {
  bool csv = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--fail-nolanes") == 0) g_no_lanes = true;
    else if (std::strcmp(argv[i], "--fail-noramp") == 0) g_no_ramp = true;
    else if (std::strcmp(argv[i], "--fail-ceiling") == 0) {
      g_ceiling_pm = (i + 1 < argc) ? std::atoi(argv[++i]) : 1200;
      if (g_ceiling_pm <= 0) g_ceiling_pm = 1200;
    } else if (std::strcmp(argv[i], "--fail-steepramp") == 0) {
      g_steep = (i + 1 < argc) ? std::atoi(argv[++i]) : 5;
      if (g_steep < 1) g_steep = 5;
    } else if (std::strcmp(argv[i], "--csv") == 0) csv = true;
  }

  const zc::CreatureType& T = u02::type();
  const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  const int32_t stNeck = u02::kLoopBuryMm + u02::kLoopArcMm[0];
  const int32_t stA = stNeck + u02::kLoopArcMm[1];
  const int32_t stB = stA + u02::kLoopArcMm[2];
  const int32_t stC = stB + u02::kLoopArcMm[3];
  const int32_t stD = stC + u02::kLoopArcMm[4];
  const int32_t total = stD + u02::kLoopArcMm[5];
  g_stC_bind_y = u02::fxu(y0 + stC);
  g_stC_mm = stC;
  g_total_mm = total;

  std::printf("MANAFOLD STRETCHY-SPAN GATE (D9 SS13) -- reads the SKIN, not the bones\n");
  std::printf("  stations mm from tube base: neck %d  A %d  B %d  C %d  D %d  end %d\n", stNeck,
              stA, stB, stC, stD, total);
  std::printf("  ceiling %d pm  thin ratio %d pm  lanes %d\n", u02::kSpanStretchMaxPm,
              u02::kSpanThinRatioPm, static_cast<int>(zc::kDeformLaneCount));
  // The monotonicity bound, printed from the SAME constants the skin is built
  // from, so a station move cannot leave a stale claim behind (checklist 19).
  {
    const double worst = g_ceiling_pm / 1000.0 *
                         (u02::kLoopArcMm[1] + u02::kLoopArcMm[2] + u02::kLoopArcMm[3]);
    std::printf("  ramp bound: sum k*L = %.0f mm  <  total-stC = %d mm   dy/ds >= %.3f\n\n",
                worst, total - stC, 1.0 - worst / (total - stC));
  }
  if (g_no_lanes) std::printf("  [LEG] --fail-nolanes: lane authority zeroed\n");
  if (g_no_ramp) std::printf("  [LEG] --fail-noramp: authority held past hinge C\n");

  // ---- G1: rest is bit-identical -----------------------------------------
  {
    zc::DeformFrame zero;
    size_t checked = 0, moved = 0;
    for (const zc::Meshlet& m : T.mesh) {
      if (m.deform.empty()) continue;
      for (size_t vi = 0; vi < m.verts.size(); ++vi) {
        const zc::SkinVertex& a = m.verts[vi];
        const zc::SkinVertex b = zc::deform_skin_vertex_lanes(a, m.deform[vi], zero);
        ++checked;
        if (a.x != b.x || a.y != b.y || a.z != b.z || a.nx != b.nx || a.ny != b.ny ||
            a.nz != b.nz)
          ++moved;
      }
    }
    std::printf("G1 rest identity: %zu deform vertices, %zu moved\n", checked, moved);
    if (moved != 0) fail("an all-zero lane frame moved a vertex -- identity contract broken");
  }

  // ---- G2/G3: monotonicity and the buried tip, at the CEILING -------------
  {
    const zc::DeformFrame fr = ceiling_frame();
    std::vector<std::pair<int32_t, double>> map;
    double tip_move = 0.0;
    const int32_t tip_y = u02::fxu(y0 + total);
    for (const zc::Meshlet& m : T.mesh) {
      if (m.deform.empty()) continue;
      // A meshlet belongs to the loop if ANY of its vertices carries the
      // antenna's axis-0 authorship. It has to be decided per MESHLET, not per
      // vertex: the rings past hinge C have had every lane ramped to zero and
      // are compiled role-kNone, so a per-vertex test cannot see the very
      // vertices G3 exists to watch.
      bool loop_meshlet = false;
      for (const zc::DeformVertex& dv : m.deform)
        if (is_loop_deform(dv)) { loop_meshlet = true; break; }
      if (!loop_meshlet) continue;
      for (size_t vi = 0; vi < m.verts.size(); ++vi) {
        const int32_t by = m.verts[vi].y;
        const zc::SkinVertex d =
            zc::deform_skin_vertex_lanes(m.verts[vi], legged(m.deform[vi], by, loop_meshlet), fr);
        map.push_back(std::make_pair(by, static_cast<double>(d.y)));
        if (by >= tip_y - u02::fxu(40)) {
          const double mv = std::fabs(static_cast<double>(d.y - by)) * 1000.0 / 65536.0;
          if (mv > tip_move) tip_move = mv;
        }
      }
    }
    std::sort(map.begin(), map.end());
    double worst_back = 0.0;
    int32_t worst_at = 0;
    for (size_t i = 1; i < map.size(); ++i) {
      if (map[i].first == map[i - 1].first) continue;
      const double back = map[i - 1].second - map[i].second;
      if (back > worst_back) {
        worst_back = back;
        worst_at = map[i].first;
      }
    }
    const double worst_mm = worst_back * 1000.0 / 65536.0;
    std::printf("G2 no fold-back at ceiling: worst reversal %.2f mm (station %d mm)\n", worst_mm,
                static_cast<int>(worst_at * 1000.0 / 65536.0) - y0);
    if (worst_back > 0.0) fail("the skin folds back on itself -- dy/ds went negative");
    std::printf("G3 buried arm tip motion at ceiling: %.2f mm (max %.2f)\n", tip_move,
                kTipMoveMaxMm);
    if (tip_move > kTipMoveMaxMm) fail("the buried arm tip moves -- the stub is back");
  }

  // ---- G4: the stretch is VISIBLE ----------------------------------------
  const zc::Clip* solo = nullptr;
  for (const zc::Clip& c : T.bank.clips)
    if (c.slot_id == 16) solo = &c;
  if (solo == nullptr) {
    fail("slot 16 (nodule-solo) is not in the bank");
  } else {
    const int S = u02::kNoduleSoloSegKeys;
    const bool saved = g_no_lanes;
    double reach[2] = {0, 0};
    for (int leg = 0; leg < 2; ++leg) {
      // leg 1 is the ablation, and it is the SAME measurement with the
      // mechanism removed -- the comparison the art law asks for.
      g_no_lanes = (leg == 1) ? true : saved;
      double lo = 1e9, hi = -1e9;
      for (int f = 0; f < S && f < solo->frame_count; ++f) {
        const Vec3 p = ball_skin(T, *solo, static_cast<uint16_t>(f), stA, 90);
        if (p.y < lo) lo = p.y;
        if (p.y > hi) hi = p.y;
      }
      reach[leg] = hi - lo;
    }
    g_no_lanes = saved;
    const double gain = reach[0] - reach[1];
    std::printf("G4 ball A VERTICAL SKIN reach over the solo-A segment:\n");
    std::printf("     lanes live    %6.1f mm\n", reach[0]);
    std::printf("     lanes ablated %6.1f mm\n", reach[1]);
    std::printf("     gain          %6.1f mm (floor %.1f)\n", gain, kReachGainMinMm);
    if (gain < kReachGainMinMm)
      fail("the span stretch is present but not visible -- the crayon-grain failure");

    if (csv) {
      std::printf("\nkey,Ax,Ay,Az,Bx,By,Bz,Cx,Cy,Cz\n");
      for (int f = 0; f < solo->frame_count; ++f) {
        const Vec3 a = ball_skin(T, *solo, static_cast<uint16_t>(f), stA, 90);
        const Vec3 b = ball_skin(T, *solo, static_cast<uint16_t>(f), stB, 90);
        const Vec3 c = ball_skin(T, *solo, static_cast<uint16_t>(f), stC, 90);
        std::printf("%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n", f, a.x, a.y, a.z, b.x,
                    b.y, b.z, c.x, c.y, c.z);
      }
    }
  }

  // ---- G5: AND IT REACHES THE CLIP THE OWNER WATCHES ----------------------
  //
  // WHY THIS EXISTS SEPARATELY FROM G4. G4 measures the solo DIAGNOSTIC, which
  // is authored at the ceiling because a diagnostic should show the ceiling. A
  // feature can pass G4 and never appear in the bank -- and for one wave it
  // very nearly did: `build_taunt3` authored nodule A's rise as a SIDEWAYS
  // swing, with a comment explaining that a vertical request moved it 3 mm.
  // That comment was true when it was written and FALSE the moment the spans
  // learned to stretch, and no gate in the tree would have noticed. This is
  // the check that notices.
  //
  // It walks every shipped slot and prints ball A's vertical SKIN reach, so the
  // table itself says which clips exercise the channel and which do not (many
  // legitimately do not -- a death is not a shrug). Only `taunt3` is GATED,
  // because it is the clip that authors the owner's own configuration:
  // "the middle one might go down while the other two swing up."
  //
  // The failable leg is `--fail-nolanes`, the same one G4 uses: it removes the
  // lane authority from the production struct the renderer reads, and G5 goes
  // red with it. Witnessed failing before this block was committed.
  {
    std::printf("\nG5 ball A VERTICAL SKIN reach, SHIPPED clips "
                "(root-local, live vs lanes-ablated):\n");
    const bool saved = g_no_lanes;
    double gated_live = -1.0, gated_gain = -1.0;
    for (const zc::Clip& c : T.bank.clips) {
      // 7 is the 2-key form still, 15 the mana lab, 16 the solo diagnostic --
      // none of them is a clip the owner watches.
      if (c.slot_id == 7 || c.slot_id == 15 || c.slot_id == 16) continue;
      double reach[2] = {0, 0};
      for (int leg = 0; leg < 2; ++leg) {
        g_no_lanes = (leg == 1) ? true : saved;
        double lo = 1e9, hi = -1e9;
        for (int f = 0; f < c.frame_count; ++f) {
          const Vec3 p = ball_skin(T, c, static_cast<uint16_t>(f), stA, 90);
          if (p.y < lo) lo = p.y;
          if (p.y > hi) hi = p.y;
        }
        reach[leg] = hi - lo;
      }
      g_no_lanes = saved;
      const bool gated = (c.slot_id == u02::kTaunt3Slot);
      std::printf("     slot %2u  live %6.1f mm   ablated %6.1f mm   gain %6.1f%s\n",
                  c.slot_id, reach[0], reach[1], reach[0] - reach[1],
                  gated ? "   <-- GATED (taunt3, the shrug)" : "");
      if (gated) {
        gated_live = reach[0];
        gated_gain = reach[0] - reach[1];
      }
    }
    if (gated_live < 0.0) {
      fail("taunt3 is not in the bank -- G5 has nothing to gate");
    } else {
      std::printf("     taunt3 floor %.1f mm on BOTH reach and gain\n",
                  kShippedReachMinMm);
      if (gated_live < kShippedReachMinMm)
        fail("taunt3's nodule A does not travel vertically -- the owner's "
             "'the other two swing up' is still a sidestep");
      if (gated_gain < kShippedReachMinMm)
        fail("taunt3's nodule A vertical travel does not come from the span "
             "stretch -- present without the mechanism means this gate is blind");
    }
  }

  // ---- G6: DO THE BALLS MOVE AGAINST EACH OTHER, ON THE CLIPS THAT SHIP? ---
  //
  //   "the balls still don't move, and not independently."   -- Direction 11 §3
  //
  // ⚠ THIS GATE EXISTS BECAUSE THE OLD ONE COULD NOT ASK THE OWNER'S QUESTION.
  // The pass-12 per-nodule table is honest and reproduces to the millimetre --
  // and it measures the SOLO DIAGNOSTIC, which drives one nodule 200 mm and
  // which the shipping bank never runs. Four passes reported this item done on
  // that number. 10-GATE-CHECKLIST items 12 and 39 are exactly this shape: a
  // measurement of the mechanism is not evidence about the picture, and the
  // check has to be against the artefact that ships.
  //
  // WHAT IS MEASURED, and why this quantity and not another. For each pair of
  // balls, the VECTOR BETWEEN THEM, over every key of the clip; the score is
  // the diagonal of that vector's bounding box. It is the right quantity for
  // the owner's sentence for one specific reason: a whole-antenna knead swings
  // all three balls together, and a rigid swing leaves the vector between two
  // of them almost unchanged. So the carrier CANCELS, and what is left is the
  // part a viewer reads as one ball moving against its neighbours -- which is
  // the complaint, stated as arithmetic.
  //
  // It reads the SKIN through `ball_skin`, the same production path G4/G5 use,
  // because ball A's independent travel is a VERTEX effect: at bone level its
  // span points along the request and it moves ~3 mm however hard it is driven
  // (see the nodule gate's own declared gap).
  {
    std::printf("\nG6 DIFFERENTIAL BALL MOTION on the SHIPPED clips "
                "-- the RANGE of the DISTANCE between ADJACENT balls\n");
    std::printf("   (%d mm per native pixel at bank scale. A rigid span keeps "
                "its length under any amount of knead,\n    so the carrier "
                "cancels and what is left is ball-against-neighbour. Measured "
                "noise floor %.1f mm.)\n",
                kMmPerNativePx, kDiffNoiseFloorMm);
    const int32_t st[3] = {u02::kKnuckleAtAMm, u02::kKnuckleAtBMm, u02::kKnuckleAtCMm};
    // ⚠ ADJACENT PAIRS ONLY, and this is the second thing this gate got wrong.
    // The rigid-span invariance holds for A-B and B-C, which are ONE bind span
    // each (kLoopArcMm[2] and [3]). A-C crosses hinge B, so folding the loop
    // changes that chord by hundreds of millimetres with perfectly rigid bones
    // -- pure carrier. Including it made A-C the maximum on almost every clip
    // and put 155 mm into the ablated leg, which is what the failable leg then
    // reported. Two balls only tell you about each other if nothing hinges
    // between them.
    const char* pn[2] = {"A-B", "B-C"};
    const int pi[2][2] = {{0, 1}, {1, 2}};
    constexpr int kPairs = 2;

    // ⚠ THE QUANTITY IS THE DISTANCE BETWEEN TWO BALLS, NOT THE VECTOR BETWEEN
    // THEM, AND THE FIRST VERSION OF THIS GATE GOT THAT WRONG. Measuring the
    // vector's bounding box scored EVERY clip in the bank at 60-140 px --
    // including ones where nothing independent happens -- and scored taunt3,
    // the single clip whose ball gesture demonstrably reads, LOWEST of all. The
    // reason is that a rigid rotation of the whole antenna sweeps the vector
    // through a huge arc while changing its LENGTH not at all: the carrier does
    // not cancel, it dominates. That is the same gate this pass was sent to
    // replace, rebuilt by accident in one afternoon.
    //
    // The DISTANCE is the invariant that works. Bones are rigid and `nodule_aim`
    // lands each ball at exactly its bind arc length, so under any amount of
    // knead, fold or body swing the ball-to-ball distance is CONSTANT. It moves
    // only when a span actually stretches -- which is precisely the mechanism
    // that carries one ball away from its neighbours, and precisely what a
    // viewer reads as a ball moving on its own.
    const auto score = [&](const zc::Clip& c, double per_pair[kPairs]) {
      double lo[kPairs], hi[kPairs];
      for (int q = 0; q < kPairs; ++q) { lo[q] = 1e30; hi[q] = -1e30; }
      for (uint16_t f = 0; f < c.frame_count; ++f) {
        Vec3 p[3];
        for (int b = 0; b < 3; ++b) p[b] = ball_skin(T, c, f, st[b], 90);
        for (int q = 0; q < kPairs; ++q) {
          const Vec3& u = p[pi[q][0]];
          const Vec3& v = p[pi[q][1]];
          const double dx = u.x - v.x, dy = u.y - v.y, dz = u.z - v.z;
          const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
          if (d < lo[q]) lo[q] = d;
          if (d > hi[q]) hi[q] = d;
        }
      }
      double best = 0.0;
      for (int q = 0; q < kPairs; ++q) {
        per_pair[q] = (c.frame_count > 0) ? hi[q] - lo[q] : 0.0;
        if (per_pair[q] > best) best = per_pair[q];
      }
      return best;
    };

    // ---- THE KNOWN-NEGATIVE FIRST (10-GATE-CHECKLIST item 40) --------------
    //
    // "Before trusting any presence metric, run it on a frame where you know
    //  the answer is NO. A known-positive proves nothing."
    //
    // ⚠ AND THE OBVIOUS NEGATIVE IS THE WRONG ONE, which this gate learned the
    // hard way. The first version used slot 7, the two-key form still: it
    // scored 0.0, the gate looked calibrated, and the metric underneath it was
    // measuring the whole-antenna carrier. A clip with NO MOTION AT ALL cannot
    // tell "measures independent ball motion" apart from "measures any motion",
    // so it certifies nothing.
    //
    // The negative that works is THE SAME BEAT CLIP WITH THE MECHANISM SWITCHED
    // OFF: slot 0, full carrier -- knead, fold, bob, body rock, all of it --
    // with the span-stretch lanes ablated, which is the deform that actually
    // carries a ball away from its neighbours. Same clip, same motion, feature
    // removed. That is the failable leg items 10 and 11 require, and its
    // failure has now been WITNESSED rather than assumed: with the vector
    // metric this leg read 973 mm, identical to the live number, which is what
    // exposed the fault.
    const zc::Clip* beat0 = nullptr;
    for (const zc::Clip& c : T.bank.clips)
      if (c.slot_id == 0) beat0 = &c;
    if (beat0 == nullptr) {
      fail("G6 has no known-negative: slot 0 is not in the bank");
    } else {
      const bool saved = g_no_lanes;
      g_no_lanes = true;
      double pp[kPairs];
      const double neg = score(*beat0, pp);
      g_no_lanes = saved;
      std::printf("   KNOWN-NEGATIVE slot 0 with the span-stretch ABLATED "
                  "(same clip, same carrier, mechanism off): %.1f mm = %.1f px"
                  "  -- ceiling %.1f mm\n",
                  neg, neg / kMmPerNativePx, kDiffNegMaxMm);
      if (neg > kDiffNegMaxMm)
        fail("G6's known-negative MOVES. With the span stretch ablated the balls "
             "cannot travel against each other, so a large number here means the "
             "metric is reading the CARRIER -- nothing else it prints is "
             "evidence. Fix the instrument before reading the table");
    }

    // ---- and then the bank, with the beat clips gated ----------------------
    for (const zc::Clip& c : T.bank.clips) {
      if (c.slot_id == u02::kNoduleSoloSlot) continue;  // the diagnostic is not the bank
      double pp[kPairs];
      const double s = score(c, pp);
      bool gated = false;
      for (int i = 0; i < kBeatSlotCount; ++i)
        if (c.slot_id == kBeatSlots[i]) gated = true;
      std::printf("   slot %2u  %6.1f mm = %5.1f px   (%s %5.1f  %s %5.1f)%s\n",
                  c.slot_id, s, s / kMmPerNativePx, pn[0], pp[0], pn[1], pp[1],
                  gated ? "  <-- GATED (authored ball beat)" : "");
      if (gated && s < kDiffBeatMinMm)
        fail("a clip that carries an AUTHORED ball beat does not show the balls "
             "moving against each other. That is the owner's sentence, on the "
             "clip he is looking at -- not on the solo diagnostic");
    }
  }

  std::printf("\n%s: %d failure(s)\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
  return g_fail == 0 ? 0 : 1;
}
