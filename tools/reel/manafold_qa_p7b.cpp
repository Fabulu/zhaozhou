// manafold_qa_p7b.cpp — QA-lane experiment E4: CAN GATE A ACTUALLY FAIL?
//
// `apply_eye_roll` hard-clamps its pm argument to +/-1000 of kEyeRollMaxA16,
// so the shipped gate structurally cannot be driven past the shipped cap by
// any amount of sweeping. To feed it a known-bad input without touching a
// single creature constant, this drives the eye-roll quaternion DIRECTLY at
// a raw angle16 -- exactly what raising kEyeRollMaxA16 would do -- and walks
// the closest-approach profile past the cap.
//
// It answers three things:
//   * can gate A fail at all (does the closest approach ever cross its floor)
//   * is the eye gap really NON-MONOTONIC in roll, as pass 7 claims
//   * how much margin does the shipped cap actually have

#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>

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

int main() {
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) { std::printf("qa-p7b: FAIL no meshlets\n"); return 1; }

  const int kStep = 20;         // angle16 per sample: 20 a16 = 0.11 deg
  const int kMaxA16 = 2600;     // well past the shipped 900 and pass 6's 1820
  const int kSamples = kMaxA16 / kStep + 1;
  const int kSigns = 8;         // gaze side x gaze lift x roll direction
  const int kCorners = kSamples * kSigns;

  u02::Rig g;
  zc::Clip ex;
  ex.slot_id = 7;
  ex.frame_count = static_cast<uint32_t>(kCorners);
  ex.quats.assign(static_cast<size_t>(kCorners) * u02::kBoneCount, zc::quat16_identity());
  ex.root.assign(static_cast<size_t>(kCorners) * 3, 0);
  ex.deform.assign(static_cast<size_t>(kCorners), zc::DeformSample{});
  for (int i = 0; i < kCorners; ++i) {
    const int s = i % kSigns;
    const int a = i / kSigns;
    const int32_t roll = static_cast<int32_t>(a) * kStep * ((s & 1) ? 1 : -1);
    const int32_t ss = (s & 2) ? 1000 : -1000;
    const int32_t sl = (s & 4) ? 1000 : -1000;
    g.reset();
    u02::loop_rest(g);
    u02::face_rest(g);
    // THE ONE DEVIATION: drive the roll joints directly at a raw angle16,
    // bypassing apply_eye_roll's clamp. Same two joints, same composition
    // order, same sign convention as apply_eye_roll itself.
    g.q[u02::kBEyeL] = u02::quat_mul(g.q[u02::kBEyeL], u02::quat_x(roll));
    g.q[u02::kBEyeR] = u02::quat_mul(g.q[u02::kBEyeR], u02::quat_x(-roll));
    u02::apply_gaze(g, u02::kGazeMaxA16 * ss / 1000, u02::kGazeLiftMaxA16 * sl / 1000);
    g.write(ex, i);
    ex.root[static_cast<size_t>(i) * 3 + 1] = u02::fxu(u02::kHoverHeightMm);
  }

  std::vector<int64_t> best(static_cast<size_t>(kSamples), 1LL << 40);
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
    int64_t closest = 1LL << 40;
    for (const auto& a2 : left)
      for (const auto& b2 : right) {
        const int64_t dx = ((static_cast<int64_t>(a2[0]) - b2[0]) * 1000) >> 16;
        const int64_t dy = ((static_cast<int64_t>(a2[1]) - b2[1]) * 1000) >> 16;
        const int64_t dz = ((static_cast<int64_t>(a2[2]) - b2[2]) * 1000) >> 16;
        const int64_t d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < closest) closest = d2;
      }
    const int a = i / kSigns;
    const int64_t d = u02::isqrt64(closest);
    if (d < best[static_cast<size_t>(a)]) best[static_cast<size_t>(a)] = d;
  }

  std::printf("qa-p7b E4: closest eye-to-eye approach vs RAW roll angle "
              "(floor 12 mm; shipped cap kEyeRollMaxA16 = %d a16)\n", u02::kEyeRollMaxA16);
  std::printf("   a16    deg     closest_mm   verdict\n");
  int first_fail = -1;
  int64_t min_seen = 1LL << 40; int min_at = 0;
  for (int a = 0; a < kSamples; ++a) {
    const int a16 = a * kStep;
    if (best[static_cast<size_t>(a)] < min_seen) {
      min_seen = best[static_cast<size_t>(a)]; min_at = a16;
    }
    if (first_fail < 0 && best[static_cast<size_t>(a)] < 12) first_fail = a16;
    const bool interesting = (a16 % 100 == 0) || a16 == u02::kEyeRollMaxA16 ||
                             best[static_cast<size_t>(a)] < 12;
    if (interesting)
      std::printf("  %5d  %5.2f   %8lld   %s%s\n", a16, a16 * 360.0 / 65536.0,
                  (long long)best[static_cast<size_t>(a)],
                  best[static_cast<size_t>(a)] < 12 ? "FAIL" : "OK",
                  a16 == u02::kEyeRollMaxA16 ? "   <-- SHIPPED CAP" : "");
  }
  std::printf("qa-p7b E4: minimum %lld mm at %d a16 (%.2f deg); first FAIL at %d a16 "
              "(%.2f deg)\n", (long long)min_seen, min_at, min_at * 360.0 / 65536.0,
              first_fail, first_fail * 360.0 / 65536.0);
  std::printf("qa-p7b E4: pass 6 shipped kEyeRollMaxA16 = 1820 a16 (10.00 deg); "
              "pass 7 ships 900 a16 (4.94 deg)\n");
  return 0;
}
