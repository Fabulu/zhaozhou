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

  std::printf("\n%s: %d failure(s)\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
  return g_fail == 0 ? 0 : 1;
}
