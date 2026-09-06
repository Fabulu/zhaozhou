// manafold_qa_p7.cpp — QA-lane experiment probe for Manafold pass 7.
//
// NOT a shipped gate. This exists to attack three pass-7 claims with the
// SAME arithmetic the committed probe uses, changing exactly one thing per
// experiment so the delta is attributable:
//
//  E1  5c rule 3 removes the root TRANSLATION but not the root ROTATION,
//      while the clearance block 200 lines above it records in its own words
//      that "subtracting only the translation lied twice ... the TUMBLE
//      rotates the body". E1 recomputes rule 3 in the FULL root-local frame
//      (R^T (p - t)), with the view directions carried into that frame too,
//      and reports both counts side by side.
//
//  E1b The same violations bucketed by HOW SIDE-ON the fixed view is to the
//      creature's own facing. The probe DECLARES that side-on is excluded
//      ("from side-on the eye is outside the body outline BY DESIGN"), but
//      its two views are fixed in WORLD space while pirouette yaws the
//      creature through a full turn -- so a yawing clip reproduces the
//      excluded case anyway. If the violations concentrate at side-on, the
//      1499 headline is largely the probe measuring what it said it was not.
//
//  E2  5d gate A sweeps ROLL amplitude but pins gaze side / gaze lift at
//      their signed extremes, asserting their contribution is "monotonic".
//      That is exactly the assumption roll violated. E2 sweeps gaze too.
//
//  E3  Every 5c rule selects the star by a HARD-CODED literal colour
//      (246,242,250). If that ever stops matching, all three rules pass
//      vacuously. E3 runs the block with a colour that matches nothing and
//      prints what the gate would report.
//
// Angles in E1b are computed in double ONLY for the report; every pass/fail
// test uses the shipped fixed-point path unchanged.

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>

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

static inline void inv_point(const zc::mat3x4fx& m, int32_t x, int32_t y, int32_t z,
                             int32_t& ox, int32_t& oy, int32_t& oz) {
  const int64_t dx = static_cast<int64_t>(x) - m.m[3];
  const int64_t dy = static_cast<int64_t>(y) - m.m[7];
  const int64_t dz = static_cast<int64_t>(z) - m.m[11];
  ox = static_cast<int32_t>((m.m[0] * dx + m.m[4] * dy + m.m[8] * dz) >> 16);
  oy = static_cast<int32_t>((m.m[1] * dx + m.m[5] * dy + m.m[9] * dz) >> 16);
  oz = static_cast<int32_t>((m.m[2] * dx + m.m[6] * dy + m.m[10] * dz) >> 16);
}

struct View { const char* name; int32_t dx, dy, dz; };
static const View kViews[2] = {{"three-quarter", 46341, -17000, 46341},
                               {"front", 65536, -17000, 0}};

// Returns true when the scaled-space perpendicular distance exceeds 1 (outside
// the body outline). Identical arithmetic to manafold_probe.cpp's rule 3.
static bool outside_outline(int64_t ux, int64_t uy, int64_t uz,
                            int64_t ex, int64_t ey, int64_t ez, int64_t& perp_out) {
  const int64_t elen = u02::isqrt64(ex * ex + ey * ey + ez * ez);
  if (elen == 0) return false;
  const int64_t dot = (ux * ex + uy * ey + uz * ez) / elen;
  const int64_t px = ux - dot * ex / elen;
  const int64_t py = uy - dot * ey / elen;
  const int64_t pz = uz - dot * ez / elen;
  const int64_t perp = u02::isqrt64(px * px + py * py + pz * pz);
  perp_out = perp;
  return perp > (1LL << 16);
}

// ---- the 5c block, parameterised on the star colour and the frame policy ----
struct Rule3Result {
  int32_t worst_overhang_mm = 0;
  int32_t worst_on_purple_pm = 1000;
  int shipped_violations = 0;   // root translation only, world-space views
  int rotfix_violations = 0;    // full root-local frame, views carried in
  int64_t star_verts_seen = 0;
  int side_bucket[3] = {};      // shipped violations by view-vs-facing angle
  const char* bucket_name[3] = {"front-ish (<40 deg)", "oblique (40-70)", "side-on (>70)"};
};

static Rule3Result run_5c(const zc::CreatureType& T, uint8_t sr, uint8_t sg, uint8_t sb) {
  Rule3Result R;
  const int32_t bx = u02::fxu(u02::kBodyRadiusMm);
  const int32_t by = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
  const int32_t eye_long_fx = u02::fxu(u02::vmm(u02::kEyeLongMm));
  for (const zc::Clip& clip : T.bank.clips) {
    for (uint32_t f = 0; f < clip.frame_count; ++f) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, clip, f, pose, nullptr, 0);
      const int32_t root_x = clip.root[static_cast<size_t>(f) * 3 + 0];
      const int32_t root_y = clip.root[static_cast<size_t>(f) * 3 + 1];
      const int32_t root_z = clip.root[static_cast<size_t>(f) * 3 + 2];
      const zc::mat3x4fx& rm = pose[u02::kBRoot];
      // The creature's own forward axis in world = R * (1,0,0).
      const double fwd[3] = {static_cast<double>(rm.m[0]), static_cast<double>(rm.m[4]),
                             static_cast<double>(rm.m[8])};
      const double fl = std::sqrt(fwd[0]*fwd[0] + fwd[1]*fwd[1] + fwd[2]*fwd[2]);
      for (const zc::Meshlet& m : T.mesh) {
        if (!(m.r == sr && m.g == sg && m.b == sb)) continue;
        int on = 0, tot = 0;
        for (const zc::SkinVertex& sv : m.verts) {
          int32_t x, y, z;
          zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
          const uint8_t eb = (sv.b0 == u02::kBPupilL || sv.b0 == u02::kBEyeL) ? u02::kBEyeL
                                                                              : u02::kBEyeR;
          const zc::mat3x4fx& em = pose[eb];
          int32_t lx, ly, lz;
          inv_point(em, x, y, z, lx, ly, lz);
          lx -= u02::fxu(u02::kEyeXMm);
          ly -= u02::fxu(u02::vmm(u02::kEyeYMm));
          lz -= (eb == u02::kBEyeL) ? u02::fxu(u02::kEyeZMm) : -u02::fxu(u02::kEyeZMm);
          (void)lx;
          int32_t w_pm = 0;
          if (eye_long_fx > 0) {
            const int64_t dy_pm = (static_cast<int64_t>(ly) * 1000) / eye_long_fx;
            if (dy_pm > -1000 && dy_pm < 1000) {
              int32_t t = static_cast<int32_t>((dy_pm + 1000) * (u02::kEyeLensRings - 1) / 2000);
              if (t < 0) t = 0;
              if (t > u02::kEyeLensRings - 1) t = u02::kEyeLensRings - 1;
              const int32_t t2 = t + 1 < u02::kEyeLensRings ? t + 1 : t;
              w_pm = (u02::kEyeLensWidthPm[t] + u02::kEyeLensWidthPm[t2]) / 2;
            }
          }
          const int32_t rim_mm =
              static_cast<int32_t>((static_cast<int64_t>(u02::kEyeWideMm) * w_pm) / 1000);
          const int32_t off_mm = static_cast<int32_t>(
              ((lz < 0 ? -static_cast<int64_t>(lz) : static_cast<int64_t>(lz)) * 1000) >> 16);
          const int32_t over = off_mm - rim_mm;
          ++tot;
          ++R.star_verts_seen;
          if (over <= 0) ++on;
          if (over > R.worst_overhang_mm) R.worst_overhang_mm = over;
          if (over > 0) {
            // --- SHIPPED: root translation only, world-space view dirs ---
            for (const View& v : kViews) {
              const int64_t ux = (static_cast<int64_t>(x - root_x) << 16) / bx;
              const int64_t uy = (static_cast<int64_t>(y - root_y) << 16) / by;
              const int64_t uz = (static_cast<int64_t>(z - root_z) << 16) / bx;
              const int64_t ex = (static_cast<int64_t>(v.dx) << 16) / bx;
              const int64_t ey = (static_cast<int64_t>(v.dy) << 16) / by;
              const int64_t ez = (static_cast<int64_t>(v.dz) << 16) / bx;
              int64_t perp = 0;
              if (outside_outline(ux, uy, uz, ex, ey, ez, perp)) {
                ++R.shipped_violations;
                // how side-on is this fixed world view to the creature's facing?
                const double vl = std::sqrt(double(v.dx)*v.dx + double(v.dy)*v.dy +
                                            double(v.dz)*v.dz);
                double c = 0.0;
                if (fl > 0 && vl > 0)
                  c = (fwd[0]*v.dx + fwd[1]*v.dy + fwd[2]*v.dz) / (fl * vl);
                if (c > 1.0) c = 1.0;
                if (c < -1.0) c = -1.0;
                double ang = std::acos(std::fabs(c)) * 180.0 / 3.14159265358979;
                int b = ang < 40.0 ? 0 : (ang < 70.0 ? 1 : 2);
                ++R.side_bucket[b];
              }
            }
            // --- ROTATION-CORRECTED: full root-local frame, views carried in ---
            {
              int32_t rlx, rly, rlz;
              inv_point(rm, x, y, z, rlx, rly, rlz);
              const int64_t ux = (static_cast<int64_t>(rlx) << 16) / bx;
              const int64_t uy = (static_cast<int64_t>(rly) << 16) / by;
              const int64_t uz = (static_cast<int64_t>(rlz) << 16) / bx;
              for (const View& v : kViews) {
                // rotate the world view direction into root-local: R^T v
                const int64_t vx = (static_cast<int64_t>(rm.m[0]) * v.dx +
                                    static_cast<int64_t>(rm.m[4]) * v.dy +
                                    static_cast<int64_t>(rm.m[8]) * v.dz) >> 16;
                const int64_t vy = (static_cast<int64_t>(rm.m[1]) * v.dx +
                                    static_cast<int64_t>(rm.m[5]) * v.dy +
                                    static_cast<int64_t>(rm.m[9]) * v.dz) >> 16;
                const int64_t vz = (static_cast<int64_t>(rm.m[2]) * v.dx +
                                    static_cast<int64_t>(rm.m[6]) * v.dy +
                                    static_cast<int64_t>(rm.m[10]) * v.dz) >> 16;
                const int64_t ex = (vx << 16) / bx;
                const int64_t ey = (vy << 16) / by;
                const int64_t ez = (vz << 16) / bx;
                int64_t perp = 0;
                if (outside_outline(ux, uy, uz, ex, ey, ez, perp)) ++R.rotfix_violations;
              }
            }
          }
        }
        if (tot > 0) {
          const int32_t pm = on * 1000 / tot;
          if (pm < R.worst_on_purple_pm) R.worst_on_purple_pm = pm;
        }
      }
    }
  }
  return R;
}

int main() {
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) { std::printf("qa-p7: FAIL no meshlets\n"); return 1; }

  // ================= E1 / E1b =================
  {
    Rule3Result R = run_5c(T, 246, 242, 250);
    const int32_t star_half_mm =
        static_cast<int32_t>((static_cast<int64_t>(u02::kStarArmSideMm) *
                              u02::kStarScalePm) / 1000) + u02::kStarWhiteRimMm;
    const int32_t cap = static_cast<int32_t>((static_cast<int64_t>(star_half_mm) *
                                              u02::kStarOverhangMaxPm) / 1000);
    std::printf("qa-p7 E1: star verts walked %lld\n", (long long)R.star_verts_seen);
    std::printf("qa-p7 E1: rule 1 worst overhang %d mm (cap %d) | rule 2 %d pm\n",
                R.worst_overhang_mm, cap, R.worst_on_purple_pm);
    std::printf("qa-p7 E1: rule 3 SHIPPED (root TRANSLATION only) = %d violations\n",
                R.shipped_violations);
    std::printf("qa-p7 E1: rule 3 ROTATION-CORRECTED (full root-local) = %d violations\n",
                R.rotfix_violations);
    std::printf("qa-p7 E1b: shipped violations by how SIDE-ON the fixed view is to "
                "the creature's own facing:\n");
    for (int i = 0; i < 3; ++i)
      std::printf("             %-22s %6d\n", R.bucket_name[i], R.side_bucket[i]);
  }

  // ================= E3 (cheap, do it before the slow sweep) =================
  {
    Rule3Result R = run_5c(T, 1, 2, 3);  // a colour no meshlet carries
    const int32_t star_half_mm =
        static_cast<int32_t>((static_cast<int64_t>(u02::kStarArmSideMm) *
                              u02::kStarScalePm) / 1000) + u02::kStarWhiteRimMm;
    const int32_t cap = static_cast<int32_t>((static_cast<int64_t>(star_half_mm) *
                                              u02::kStarOverhangMaxPm) / 1000);
    std::printf("qa-p7 E3: star colour filter changed to a value nothing carries.\n");
    std::printf("qa-p7 E3: star verts walked %lld\n", (long long)R.star_verts_seen);
    std::printf("qa-p7 E3: rule 1 %d mm / cap %d -> %s | rule 2 %d pm / 600 -> %s | "
                "rule 3 %d violations\n",
                R.worst_overhang_mm, cap,
                R.worst_overhang_mm <= cap ? "OK" : "FAIL",
                R.worst_on_purple_pm,
                R.worst_on_purple_pm >= 600 ? "OK" : "FAIL",
                R.shipped_violations);
  }

  // ================= E2: gate A with GAZE amplitude swept =================
  {
    const int kRollSteps = 21;
    const int kGazeSteps = 11;      // 0, 10% ... 100% of the authored gaze cap
    const int kCorners = 16 * kRollSteps * kGazeSteps;
    u02::Rig g;
    zc::Clip ex;
    ex.slot_id = 7;
    ex.frame_count = static_cast<uint32_t>(kCorners);
    ex.quats.assign(static_cast<size_t>(kCorners) * u02::kBoneCount, zc::quat16_identity());
    ex.root.assign(static_cast<size_t>(kCorners) * 3, 0);
    ex.deform.assign(static_cast<size_t>(kCorners), zc::DeformSample{});
    for (int i = 0; i < kCorners; ++i) {
      const int c = i % 16;
      const int rstep = (i / 16) % kRollSteps;
      const int gstep = (i / 16) / kRollSteps;
      const int32_t rmag = 1000 * rstep / (kRollSteps - 1);
      const int32_t gmag = 1000 * gstep / (kGazeSteps - 1);
      const int32_t sr = ((c & 1) ? rmag : -rmag);
      const int32_t ss = (c & 2) ? gmag : -gmag;
      const int32_t sl = (c & 4) ? gmag : -gmag;
      const int32_t sh = (c & 8) ? 1000 : -1000;
      g.reset();
      u02::loop_rest(g);
      u02::face_rest(g);
      u02::apply_eye_roll(g, sr, sr);
      u02::apply_gaze(g, u02::kGazeMaxA16 * ss / 1000, u02::kGazeLiftMaxA16 * sl / 1000);
      u02::apply_eye_shift(g, sh, sh);
      g.write(ex, i);
      ex.root[static_cast<size_t>(i) * 3 + 1] = u02::fxu(u02::kHoverHeightMm);
    }
    int64_t closest_mm = 1LL << 40;
    int closest_corner = -1;
    int32_t closest_roll_pm = 0, closest_gaze_pm = 0;
    for (int i = 0; i < kCorners; ++i) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, ex, static_cast<uint32_t>(i), pose, nullptr, 0);
      std::vector<std::array<int32_t, 3>> left, right;
      for (const zc::Meshlet& m : T.mesh) {
        const bool lens = (m.r == u02::kLensR && m.g == u02::kLensG && m.b == u02::kLensB);
        const bool star = (m.r == 246 && m.g == 242 && m.b == 250) ||
                          (m.r == u02::kStarR && m.g == u02::kStarG && m.b == u02::kStarB);
        if (!lens && !star) continue;
        for (const zc::SkinVertex& sv : m.verts) {
          int32_t x, y, z;
          zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
          (z > 0 ? left : right).push_back({x, y, z});
        }
      }
      for (const auto& a : left)
        for (const auto& b : right) {
          const int64_t dx = ((static_cast<int64_t>(a[0]) - b[0]) * 1000) >> 16;
          const int64_t dy = ((static_cast<int64_t>(a[1]) - b[1]) * 1000) >> 16;
          const int64_t dz = ((static_cast<int64_t>(a[2]) - b[2]) * 1000) >> 16;
          const int64_t d2 = dx * dx + dy * dy + dz * dz;
          if (d2 < closest_mm) {
            closest_mm = d2;
            closest_corner = i;
            closest_roll_pm = 1000 * ((i / 16) % kRollSteps) / (kRollSteps - 1);
            closest_gaze_pm = 1000 * ((i / 16) / kRollSteps) / (kGazeSteps - 1);
          }
        }
    }
    const int64_t closest = u02::isqrt64(closest_mm);
    std::printf("qa-p7 E2: gate A with GAZE amplitude swept too (%d corners = "
                "16 signs x %d roll steps x %d gaze steps)\n",
                kCorners, kRollSteps, kGazeSteps);
    std::printf("qa-p7 E2: closest approach %lld mm (floor 12) at corner %d "
                "= roll %d pm of cap, gaze %d pm of cap -> %s\n",
                (long long)closest, closest_corner, closest_roll_pm, closest_gaze_pm,
                closest >= 12 ? "OK" : "FAIL");
    std::printf("qa-p7 E2: shipped gate A (gaze pinned at full) reports 22 mm; "
                "this sweep reports %lld mm\n", (long long)closest);
  }
  return 0;
}
