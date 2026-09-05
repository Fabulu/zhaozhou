// QA-lane INDEPENDENT re-measurement of Direction-5 5c/5d gates, in CORRECT
// units. manafold_probe.cpp converts fx16 -> mm with ">>16", but fxu() is
// mm*65536/1000, so ">>16" yields METRES. Correct conversion: (raw*1000)>>16.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
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

static inline int64_t tomm(int64_t raw) { return (raw * 1000) >> 16; }

static void inv_point(const zc::mat3x4fx& m, int32_t x, int32_t y, int32_t z,
                      int32_t& ox, int32_t& oy, int32_t& oz) {
  const int64_t dx = (int64_t)x - m.m[3], dy = (int64_t)y - m.m[7], dz = (int64_t)z - m.m[11];
  ox = (int32_t)((m.m[0] * dx + m.m[4] * dy + m.m[8] * dz) >> 16);
  oy = (int32_t)((m.m[1] * dx + m.m[5] * dy + m.m[9] * dz) >> 16);
  oz = (int32_t)((m.m[2] * dx + m.m[6] * dy + m.m[10] * dz) >> 16);
}
struct V { int32_t x, y, z; int side; bool lens; };
static V g_ca, g_cb;

int main(int argc, char** argv) {
  const zc::CreatureType& T = u02::type();
  const int inject_mm = (argc > 1) ? std::atoi(argv[1]) : 0;
  std::printf("QA units: fxu(1000mm)=%d ; tomm=%lld mm ; (raw>>16)=%d  <-- what the shipped probe prints as mm\n",
              u02::fxu(1000), (long long)tomm(u02::fxu(1000)), u02::fxu(1000) >> 16);
  std::printf("QA: eye_shift_a16(1000)=%d a16 = %.1f deg (kEyeShiftPivotMm=%d)\n",
              u02::eye_shift_a16(1000), u02::eye_shift_a16(1000) * 360.0 / 65536.0,
              u02::kEyeShiftPivotMm);
  std::printf("QA: kEyeRollMaxA16=%d = %.2f deg\n\n", u02::kEyeRollMaxA16,
              u02::kEyeRollMaxA16 * 360.0 / 65536.0);

  const int32_t bx = u02::fxu(u02::kBodyRadiusMm), by = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
  const int32_t root_y = u02::fxu(u02::kHoverHeightMm);
  struct Case { const char* name; int roll_pm; bool shift; int gaze_mul; };
  const Case cases[] = {
      {"roll  0.00 deg", 0, false, 1},
      {"roll  2.00 deg", 200, false, 1},
      {"roll  4.00 deg", 400, false, 1},
      {"roll  5.00 deg", 500, false, 1},
      {"roll  6.00 deg", 600, false, 1},
      {"roll  6.50 deg", 650, false, 1},
      {"roll  7.00 deg", 700, false, 1},
      {"roll  7.50 deg", 750, false, 1},
      {"roll  8.00 deg", 800, false, 1},
      {"roll  9.00 deg", 900, false, 1},
      {"roll 10.00 deg", 1000, false, 1},
  };




  for (const Case& c : cases) {
    int64_t closest2 = 1LL << 50;
    int32_t worst_ellip = 1 << 30;
    int wc = -1, cc = -1;
    int32_t inside_pm = 1 << 30; int inside_n = 0;
    for (int i = 0; i < 16; ++i) {
      u02::Rig g;
      g.reset();
      u02::loop_rest(g);
      u02::face_rest(g);
      const int32_t sr = (i & 1) ? 1000 : -1000, ss = (i & 2) ? 1000 : -1000,
                    sl = (i & 4) ? 1000 : -1000, sh = (i & 8) ? 1000 : -1000;
      u02::apply_eye_roll(g, c.roll_pm * sr / 1000, c.roll_pm * sr / 1000);
      if (c.gaze_mul == 1) {
        u02::apply_gaze(g, u02::kGazeMaxA16 * ss / 1000, u02::kGazeLiftMaxA16 * sl / 1000);
      } else {
        g.q[u02::kBPupilL] = u02::quat_mul(
            u02::quat_y(-(u02::kGazeMaxA16 * ss / 1000 * c.gaze_mul)),
            u02::quat_z(u02::kGazeLiftMaxA16 * sl / 1000 * c.gaze_mul));
        g.q[u02::kBPupilR] = g.q[u02::kBPupilL];
      }
      if (c.shift) u02::apply_eye_shift(g, sh, sh);
      zc::Clip cl;
      cl.slot_id = 7;
      cl.frame_count = 1;
      cl.quats.assign(u02::kBoneCount, zc::quat16_identity());
      cl.root.assign(3, 0);
      cl.deform.assign(1, zc::DeformSample{});
      g.write(cl, 0);
      cl.root[1] = root_y;
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, cl, 0, pose, nullptr, 0);
      std::vector<V> L, R;
      for (const zc::Meshlet& m : T.mesh) {
        const bool lens = (m.r == u02::kLensR && m.g == u02::kLensG && m.b == u02::kLensB);
        const bool star = (m.r == 246 && m.g == 242 && m.b == 250) ||
                          (m.r == u02::kStarR && m.g == u02::kStarG && m.b == u02::kStarB);
        if (!lens && !star) continue;
        for (const zc::SkinVertex& sv : m.verts) {
          int32_t x, y, z;
          zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
          const int side = (sv.b0 == u02::kBEyeL || sv.b0 == u02::kBPupilL) ? 0 : 1;
          if (inject_mm) {
            const int32_t d = u02::fxu(inject_mm);
            z += (side == 0) ? -d : d;
          }
          (side == 0 ? L : R).push_back({x, y, z, side, lens});
          if (lens) {
            const int64_t ex = ((int64_t)x << 16) / bx, ey = ((int64_t)(y - root_y) << 16) / by,
                          ez = ((int64_t)z << 16) / bx;
            const int32_t e = (int32_t)((u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
            if (e < worst_ellip) { worst_ellip = e; wc = i; }
          }
        }
      }
      {
        const int32_t ax = u02::fxu(u02::kEyeDeepMm);
        const int32_t ay = u02::fxu(u02::vmm(u02::kEyeLongMm));
        const int32_t az = u02::fxu(u02::kEyeWideMm);
        for (const V& a : L) {
          int32_t lx, ly, lz;
          inv_point(pose[u02::kBEyeR], a.x, a.y, a.z, lx, ly, lz);
          lx -= u02::fxu(u02::kEyeXMm);
          ly -= u02::fxu(u02::vmm(u02::kEyeYMm));
          lz += u02::fxu(u02::kEyeZMm);
          const int64_t ex = ((int64_t)lx << 16) / ax, ey = ((int64_t)ly << 16) / ay,
                        ez = ((int64_t)lz << 16) / az;
          const int32_t e = (int32_t)((u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
          if (e < inside_pm) inside_pm = e;
          if (e < 1000) ++inside_n;
        }
      }
      for (const V& a : L)
        for (const V& b : R) {
          const int64_t dx = tomm((int64_t)a.x - b.x), dy = tomm((int64_t)a.y - b.y),
                        dz = tomm((int64_t)a.z - b.z);
          const int64_t d2 = dx * dx + dy * dy + dz * dz;
          if (d2 < closest2) { closest2 = d2; cc = i; g_ca = a; g_cb = b; }
        }
    }
    std::printf("QA %-16s  min vert gap %4lld mm   deepest LEFT vertex vs the RIGHT lens surface: %4d pm (<1000 == INSIDE), %d such verts  %s\n",
                c.name, (long long)u02::isqrt64(closest2), inside_pm, inside_n, inside_n ? "*** EYES INTERSECT ***" : "clear");
  }
  if (inject_mm) std::printf("QA: (each eye translated %d mm toward the centre plane)\n", inject_mm);

  // ---- 5c leash, re-measured in correct units over the whole clip bank ----
  const int32_t star_half_mm =
      (int32_t)(((int64_t)u02::kStarArmSideMm * u02::kStarScalePm) / 1000) + u02::kStarWhiteRimMm;
  const int32_t cap_mm = (int32_t)(((int64_t)star_half_mm * u02::kStarOverhangMaxPm) / 1000);
  const int32_t eye_long_fx = u02::fxu(u02::vmm(u02::kEyeLongMm));
  int32_t worst_over = INT32_MIN, worst_on = 1000;
  uint32_t ws = 0, wk = 0;
  for (const zc::Clip& clip : T.bank.clips) {
    for (uint32_t f = 0; f < clip.frame_count; ++f) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, clip, f, pose, nullptr, 0);
      for (const zc::Meshlet& m : T.mesh) {
        if (!(m.r == 246 && m.g == 242 && m.b == 250)) continue;
        int on = 0, tot = 0;
        for (const zc::SkinVertex& sv : m.verts) {
          int32_t x, y, z;
          zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
          const uint8_t eb =
              (sv.b0 == u02::kBPupilL || sv.b0 == u02::kBEyeL) ? u02::kBEyeL : u02::kBEyeR;
          int32_t lx, ly, lz;
          inv_point(pose[eb], x, y, z, lx, ly, lz);
          // FRAME FIX: inv_point against a SKINNING matrix (world * inv_bind)
          // returns BIND space, not eye-bone space. The eye bone's bind is a
          // pure translation off the root, so subtract it to get eye-local.
          lx -= u02::fxu(u02::kEyeXMm);
          ly -= u02::fxu(u02::vmm(u02::kEyeYMm));
          lz -= (eb == u02::kBEyeL) ? u02::fxu(u02::kEyeZMm) : -u02::fxu(u02::kEyeZMm);
          int32_t w_pm = 0;
          if (eye_long_fx > 0) {
            const int64_t dyp = ((int64_t)ly * 1000) / eye_long_fx;
            if (dyp > -1000 && dyp < 1000) {
              int32_t t = (int32_t)((dyp + 1000) * (u02::kEyeLensRings - 1) / 2000);
              if (t < 0) t = 0;
              if (t > u02::kEyeLensRings - 1) t = u02::kEyeLensRings - 1;
              const int32_t t2 = t + 1 < u02::kEyeLensRings ? t + 1 : t;
              w_pm = (u02::kEyeLensWidthPm[t] + u02::kEyeLensWidthPm[t2]) / 2;
            }
          }
          const int32_t rim_mm = (int32_t)(((int64_t)u02::kEyeWideMm * w_pm) / 1000);
          const int32_t off_mm = (int32_t)tomm(lz < 0 ? -(int64_t)lz : (int64_t)lz);
          const int32_t over = off_mm - rim_mm;
          ++tot;
          if (over <= 0) ++on;
          if (over > worst_over) { worst_over = over; ws = clip.slot_id; wk = f; }
        }
        if (tot > 0) {
          const int32_t pm = on * 1000 / tot;
          if (pm < worst_on) worst_on = pm;
        }
      }
    }
  }
  std::printf("\nQA 5c rule 1 CORRECT UNITS: worst overhang %d mm (cap %d mm, star half-width %d mm)"
              " at slot %u key %u - %s\n",
              worst_over, cap_mm, star_half_mm, ws, wk, worst_over <= cap_mm ? "OK" : "FAIL");
  std::printf("QA 5c rule 2 CORRECT UNITS: worst fraction of star on the purple %d pm (floor 600) - %s\n",
              worst_on, worst_on >= 600 ? "OK" : "FAIL");
  // ---- GAZE SWEEP: at what fraction of the gaze clamp does the leash break?
  {
    std::printf("\nQA ==== GAZE SWEEP vs the 5c leash (cap %d mm, floor 600 pm) ====\n", cap_mm);
    for (int frac = 0; frac <= 100; frac += 10) {
      u02::Rig g;
      g.reset();
      u02::loop_rest(g);
      u02::face_rest(g);
      u02::apply_gaze(g, (int32_t)((int64_t)u02::kGazeMaxA16 * frac / 100), 0);
      zc::Clip cl;
      cl.slot_id = 7; cl.frame_count = 1;
      cl.quats.assign(u02::kBoneCount, zc::quat16_identity());
      cl.root.assign(3, 0); cl.deform.assign(1, zc::DeformSample{});
      g.write(cl, 0);
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, cl, 0, pose, nullptr, 0);
      int32_t wo = INT32_MIN; int on = 0, tot = 0;
      for (const zc::Meshlet& m : T.mesh) {
        if (!(m.r == 246 && m.g == 242 && m.b == 250)) continue;
        for (const zc::SkinVertex& sv : m.verts) {
          int32_t x, y, z; zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
          const uint8_t eb =
              (sv.b0 == u02::kBPupilL || sv.b0 == u02::kBEyeL) ? u02::kBEyeL : u02::kBEyeR;
          int32_t lx, ly, lz;
          inv_point(pose[eb], x, y, z, lx, ly, lz);
          lx -= u02::fxu(u02::kEyeXMm);
          ly -= u02::fxu(u02::vmm(u02::kEyeYMm));
          lz -= (eb == u02::kBEyeL) ? u02::fxu(u02::kEyeZMm) : -u02::fxu(u02::kEyeZMm);
          int32_t w_pm = 0;
          if (eye_long_fx > 0) {
            const int64_t dyp = ((int64_t)ly * 1000) / eye_long_fx;
            if (dyp > -1000 && dyp < 1000) {
              int32_t t = (int32_t)((dyp + 1000) * (u02::kEyeLensRings - 1) / 2000);
              if (t < 0) t = 0;
              if (t > u02::kEyeLensRings - 1) t = u02::kEyeLensRings - 1;
              const int32_t t2 = t + 1 < u02::kEyeLensRings ? t + 1 : t;
              w_pm = (u02::kEyeLensWidthPm[t] + u02::kEyeLensWidthPm[t2]) / 2;
            }
          }
          const int32_t rim_mm = (int32_t)(((int64_t)u02::kEyeWideMm * w_pm) / 1000);
          const int32_t off_mm = (int32_t)tomm(lz < 0 ? -(int64_t)lz : (int64_t)lz);
          const int32_t over = off_mm - rim_mm;
          ++tot; if (over <= 0) ++on;
          if (over > wo) wo = over;
        }
      }
      std::printf("QA gaze %3d%% of kGazeMaxA16 (%5.2f deg): worst overhang %4d mm  on-purple %4d pm  %s\n",
                  frac, (int64_t)u02::kGazeMaxA16 * frac / 100 * 360.0 / 65536.0, wo,
                  tot ? on * 1000 / tot : 0,
                  (wo <= cap_mm && (tot ? on * 1000 / tot : 0) >= 600) ? "OK" : "BREACH");
    }
  }

  // ---- THE SHIPPED CLIP BANK: do any real clip frames intersect? ----
  {
    const int32_t axx = u02::fxu(u02::kEyeDeepMm);
    const int32_t ayy = u02::fxu(u02::vmm(u02::kEyeLongMm));
    const int32_t azz = u02::fxu(u02::kEyeWideMm);
    std::printf("\nQA ==== SHIPPED CLIP BANK: eye vs eye, every clip, every key ====\n");
    int grand_bad = 0;
    for (const zc::Clip& clip : T.bank.clips) {
      int32_t worst = 1 << 30;
      uint32_t wf = 0;
      int bad_frames = 0, worst_n = 0;
      int64_t gap2 = 1LL << 50;
      for (uint32_t f = 0; f < clip.frame_count; ++f) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, clip, f, pose, nullptr, 0);
        std::vector<V> LL, RR;
        for (const zc::Meshlet& m : T.mesh) {
          const bool lens = (m.r == u02::kLensR && m.g == u02::kLensG && m.b == u02::kLensB);
          const bool star = (m.r == 246 && m.g == 242 && m.b == 250) ||
                            (m.r == u02::kStarR && m.g == u02::kStarG && m.b == u02::kStarB);
          if (!lens && !star) continue;
          for (const zc::SkinVertex& sv : m.verts) {
            int32_t x, y, z;
            zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
            const int side = (sv.b0 == u02::kBEyeL || sv.b0 == u02::kBPupilL) ? 0 : 1;
            (side == 0 ? LL : RR).push_back({x, y, z, side, lens});
          }
        }
        int nbad = 0;
        int32_t fmin = 1 << 30;
        for (const V& a : LL) {
          int32_t lx, ly, lz;
          inv_point(pose[u02::kBEyeR], a.x, a.y, a.z, lx, ly, lz);
          lx -= u02::fxu(u02::kEyeXMm);
          ly -= u02::fxu(u02::vmm(u02::kEyeYMm));
          lz += u02::fxu(u02::kEyeZMm);
          const int64_t ex = ((int64_t)lx << 16) / axx, ey = ((int64_t)ly << 16) / ayy,
                        ez = ((int64_t)lz << 16) / azz;
          const int32_t e = (int32_t)((u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
          if (e < fmin) fmin = e;
          if (e < 1000) ++nbad;
        }
        for (const V& a : LL)
          for (const V& b : RR) {
            const int64_t dx = tomm((int64_t)a.x - b.x), dy = tomm((int64_t)a.y - b.y),
                          dz = tomm((int64_t)a.z - b.z);
            const int64_t d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < gap2) gap2 = d2;
          }
        if (nbad) ++bad_frames;
        if (fmin < worst) { worst = fmin; wf = f; worst_n = nbad; }
      }
      grand_bad += bad_frames;
      std::printf("QA slot %2u (%3u keys): closest LEFT vertex %4d pm of the RIGHT lens surface"
                  " (key %3u, %d verts inside); min vert gap %4lld mm  %s\n",
                  clip.slot_id, clip.frame_count, worst, wf, worst_n,
                  (long long)u02::isqrt64(gap2), bad_frames ? "*** INTERSECTS ***" : "clear");
    }
    std::printf("QA TOTAL clip frames with an eye-vs-eye intersection: %d\n", grand_bad);
  }
  return 0;
}
