// u02_probe — the COMMITTED hover-clearance probe for creature 02.
//
// THE CONTRACT IS CLEARANCE, NOT PENETRATION (OWNER-DIRECTION-1: the
// creature FLOATS — no legs, no gait, no ground contact; Zixxtrixx's
// authored-penetration law does not apply). For EVERY clip, EVERY key AND
// every 60 Hz presentation midpoint, skin EVERY full-detail vertex through
// the decoded pose and assert
//
//     world_y(vertex) >= kMinClearanceMm
//
// where world y is measured from the instance's terrain-snap plane (the
// reel sets instance y to the column top; the clip root carries the hover).
// Terrain sloping away from the snap column only increases clearance, so
// the flat-plane bound is the conservative one.
//
// The constant is derived from the ACCEPTED motion (a window from named
// constants, never absolute ticks): the rest clip's low hover minus its bob
// leaves ~70 mm; the gate sits below it with honest headroom (the
// gate-fitted-to-its-own-answer trap).
//
// Committed because a thrown-away probe is unreproducible (CLAUDE.md).

#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"

namespace zc = zref::creature;
#include "unnamed02.h"

int main() {
  constexpr int32_t kMinClearanceMm = 40;
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::printf("u02-probe: FAIL compile produced no meshlets\n");
    return 1;
  }
  int rc = 0;
  for (const zc::Clip& clip : T.bank.clips) {
    int32_t worst = INT32_MAX;
    uint16_t worst_frame = 0;
    uint8_t worst_sub = 0;
    for (uint16_t f = 0; f < clip.frame_count; ++f) {
      for (uint8_t sub = 0; sub < 2; ++sub) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, clip, f, pose, nullptr, sub);
        const zc::DeformSample d = zc::deformation_sample(T, clip.slot_id, f, sub);
        for (const zc::Meshlet& m : T.mesh) {
          for (size_t vi = 0; vi < m.verts.size(); ++vi) {
            zc::SkinVertex sv = m.verts[vi];
            if (!m.deform.empty()) sv = zc::deform_skin_vertex(sv, m.deform[vi], d);
            int32_t x, y, z;
            zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
            if (y < worst) {
              worst = y;
              worst_frame = f;
              worst_sub = sub;
            }
          }
        }
      }
    }
    const int32_t worst_mm = static_cast<int32_t>((static_cast<int64_t>(worst) * 1000) >> 16);
    const bool ok = worst_mm >= kMinClearanceMm;
    std::printf("u02-probe: slot %u (%u keys): min clearance %d mm at key %u sub %u — %s\n",
                clip.slot_id, clip.frame_count, worst_mm, worst_frame, worst_sub,
                ok ? "OK" : "FAIL");
    if (!ok) rc = 1;
  }
  std::printf(rc == 0 ? "u02-probe: CLEARANCE CONTRACT HOLDS (>= %d mm everywhere)\n"
                      : "u02-probe: CLEARANCE VIOLATED (< %d mm)\n",
              kMinClearanceMm);

  // ---- PASS 2: the LOOP-CLOSURE probe (committed, per CLAUDE.md) ---------
  //
  // The return arm must stay ENGAGED with the body at every representable
  // fold scale: loop_pose aims hinge D's last segment at the re-entry
  // anchor in closed form, and this probe is the 3D proof (the 2D aim
  // ignores the neck yaw and the rest tilt, so the verdict lives here).
  // Contract: the arm's terminal ring CENTRES sit inside the body
  // ellipsoid at <= kMaxEndEllipPm of the surface — for a synthetic fold
  // sweep 780..1160 (the historical clip range) AND for every key of every
  // shipped clip. The floating dongle and the punch-through both violate
  // this; connection cannot regress silently again.
  {
    constexpr int32_t kMaxEndEllipPm = 920;  // <= 0.92 of the surface radius
    const int32_t rx = u02::fxu(u02::kBodyRadiusMm);
    const int32_t ry = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
    // terminal ring stations (mm along the tube from y0), replicated from
    // make_loop's law: the last three rings of the chain.
    const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
    const int32_t total = u02::kLoopBuryMm + u02::kLoopArcMm[0] + u02::kLoopArcMm[1] +
                          u02::kLoopArcMm[2] + u02::kLoopArcMm[3] + u02::kLoopArcMm[4];
    int32_t worst_pm = 0;
    int32_t worst_ellip = 0;
    const auto end_ellip = [&](const std::array<zc::mat3x4fx, zc::kMaxBones>& pose) {
      // Test in the ROOT-LOCAL frame: invert the root's affine (R^T (p - t))
      // before the ellipsoid test. Subtracting only the translation lied
      // twice — first on a jump clip (x/z moved), then on the fall (the
      // TUMBLE rotates the body, and the anchor rotates with it).
      const zc::mat3x4fx& rm = pose[u02::kBRoot];
      int32_t worst = 0;
      for (int ri = u02::kLoopRings - 3; ri < u02::kLoopRings; ++ri) {
        const int32_t s =
            static_cast<int32_t>((static_cast<int64_t>(total) * ri) / (u02::kLoopRings - 1));
        zc::SkinVertex sv{};
        sv.x = u02::fxu(u02::kLoopTubeXMm);
        sv.y = u02::fxu(y0 + s);
        sv.z = 0;
        sv.b0 = u02::kBHingeD;  // the terminal rings are fully on the arm bone
        sv.b1 = u02::kBHingeD;
        sv.w0 = 64;
        int32_t x, y, z;
        zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
        const int64_t dx = x - rm.m[3], dy = y - rm.m[7], dz = z - rm.m[11];
        const int64_t lx = (rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16;
        const int64_t ly = (rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16;
        const int64_t lz = (rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16;
        const int64_t ex = (lx << 16) / rx;
        const int64_t ey = (ly << 16) / ry;
        const int64_t ez = (lz << 16) / rx;
        const int32_t e = static_cast<int32_t>(
            (u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
        if (e > worst) worst = e;
      }
      return worst;
    };
    // (a) the synthetic sweep: one clip, one key per fold scale
    {
      const int kSteps = 24;
      zc::Clip sweep = u02::clip_shell(7, kSteps, u02::kHoverHeightMm);
      for (int i = 0; i < kSteps; ++i) {
        const int32_t pm = 700 + (1160 - 700) * i / (kSteps - 1);
        u02::Rig g;
        g.reset();
        u02::loop_pose(g, pm, pm, pm, pm);
        u02::face_rest(g);
        g.write(sweep, i);
      }
      for (int i = 0; i < kSteps; ++i) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, sweep, static_cast<uint16_t>(i), pose, nullptr, 0);
        const int32_t e = end_ellip(pose);
        const int32_t pm = 700 + (1160 - 700) * i / (kSteps - 1);
        if (e > worst_ellip) {
          worst_ellip = e;
          worst_pm = pm;
        }
        if (e > kMaxEndEllipPm) {
          std::printf("u02-probe: CLOSURE FAIL at fold scale %d: arm end at %d pm of surface\n",
                      pm, e);
          rc = 1;
        }
      }
      std::printf("u02-probe: closure sweep 700..1160 worst arm-end %d pm of surface (at pm %d)"
                  " — %s (gate %d)\n",
                  worst_ellip, worst_pm, worst_ellip <= kMaxEndEllipPm ? "OK" : "FAIL",
                  kMaxEndEllipPm);
    }
    // (b) every key of every shipped clip
    int32_t bank_worst = 0;
    uint16_t bank_slot = 0, bank_key = 0;
    for (const zc::Clip& clip : T.bank.clips) {
      for (uint16_t f = 0; f < clip.frame_count; ++f) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, clip, f, pose, nullptr, 0);
        const int32_t e = end_ellip(pose);
        if (e > bank_worst) {
          bank_worst = e;
          bank_slot = clip.slot_id;
          bank_key = f;
        }
      }
    }
    const bool ok = bank_worst <= kMaxEndEllipPm;
    std::printf("u02-probe: closure over the clip bank: worst arm-end %d pm (slot %u key %u)"
                " — %s\n",
                bank_worst, bank_slot, bank_key, ok ? "OK" : "FAIL");
    if (!ok) rc = 1;
  }

  // ---- PASS 2: the EYE-PROTRUSION probe (committed; the read is PROTECTED)
  //
  // Owner: the eyes must keep poking out as 3D things — the artist likes
  // it. The protected value is the READ the first pass shipped: the lens
  // crown standing ~37% of the body radius proud (~166 mm, ~13 px), the
  // star ~100 mm. Growing/splaying the almonds swings their tips further
  // out along the radial, so every eye-geometry change re-measures here and
  // pulls kEyeDeepMm/kEyeXMm back to the protected read. Computed in 3D
  // over the posed face-region vertices against the body ellipsoid — a
  // rendered-frame measurement would conflate protrusion with perspective.
  {
    const int32_t rx = u02::fxu(u02::kBodyRadiusMm);
    const int32_t ry = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
    const zc::Clip& still = T.bank.clips[7];  // slot 7: the still pose
    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zc::decode_pose(T, still, 0, pose, nullptr, 0);
    const int32_t root_y = still.root[1];
    int32_t max_e = 0;
    for (const zc::Meshlet& m : T.mesh) {
      for (const zc::SkinVertex& sv : m.verts) {
        int32_t x, y, z;
        zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
        if (x < u02::fxu(300)) continue;  // the face region: forward of +300 mm
        const int64_t ex = (static_cast<int64_t>(x) << 16) / rx;
        const int64_t ey = (static_cast<int64_t>(y - root_y) << 16) / ry;
        const int64_t ez = (static_cast<int64_t>(z) << 16) / rx;
        const int32_t e = static_cast<int32_t>(
            (u02::isqrt64(ex * ex + ey * ey + ez * ez) * 1000) >> 16);
        if (e > max_e) max_e = e;
      }
    }
    const int32_t proud_mm =
        static_cast<int32_t>((static_cast<int64_t>(max_e - 1000) * u02::kBodyRadiusMm) / 1000);
    std::printf("u02-probe: eye crown ellip %d pm — stands %d mm proud of the body "
                "(protected read: ~166 mm / 1369 pm; re-tune kEyeDeepMm/kEyeXMm toward it)\n",
                max_e, proud_mm);
  }
  return rc;
}
