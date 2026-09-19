// Shared visible-core metric for Direction-14/16 public carrier proof.
// Both mjointpub and mspan call this exact production-path implementation.
#ifndef ZHAO_REEL_MANAFOLD_PUBLIC_JOINT_METRIC_H
#define ZHAO_REEL_MANAFOLD_PUBLIC_JOINT_METRIC_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace u02::public_joint_metric {

constexpr double kVisibleEffectMinMm = 20.0;

struct Point {
  double x = 0.0, y = 0.0, z = 0.0;
};

inline Point operator-(const Point& a, const Point& b) {
  return Point{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline double norm(const Point& a) {
  return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

struct Carrier {
  const char* name;
  uint8_t bone;
  int32_t station_mm;
  int32_t half_mm;
  bool anchored;
};

inline std::array<Carrier, 5> carriers() {
  return {{{"F", kBJunctionF, kKnuckleAtJfMm,
            kLoopCarrierCoreHalfMm[0], true},
           {"A", kBHingeA, kKnuckleAtAMm,
            kLoopCarrierCoreHalfMm[1], false},
           {"B", kBHingeB, kKnuckleAtBMm,
            kLoopCarrierCoreHalfMm[2], false},
           {"C", kBHingeC, kKnuckleAtCMm,
            kLoopCarrierCoreHalfMm[3], false},
           {"E", kBRearSocket, kKnuckleAtEndMm,
            kLoopCarrierCoreHalfMm[4], true}}};
}

inline Point root_local(
    const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
    int32_t wx, int32_t wy, int32_t wz) {
  const zc::mat3x4fx& rm = pose[kBRoot];
  const int64_t dx = wx - rm.m[3], dy = wy - rm.m[7], dz = wz - rm.m[11];
  return {
      static_cast<double>(((rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16) * 1000 >> 16),
      static_cast<double>(((rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16) * 1000 >> 16),
      static_cast<double>(((rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16) * 1000 >> 16)};
}

inline Point posed_vertex(
    const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
    const zc::SkinVertex& v) {
  int32_t x = 0, y = 0, z = 0;
  zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
  return root_local(pose, x, y, z);
}

inline zc::DeformFrame clip_deform_frame(const zc::Clip& c, uint16_t frame) {
  zc::DeformFrame fr;
  if (c.deform.size() == c.frame_count) fr.lane[0] = c.deform[frame];
  const size_t ex_lanes = static_cast<size_t>(zc::kDeformLaneCount) - 1u;
  if (c.deform_ex.size() == static_cast<size_t>(c.frame_count) * ex_lanes)
    for (size_t lane = 1; lane < zc::kDeformLaneCount; ++lane)
      fr.lane[lane] =
          c.deform_ex[static_cast<size_t>(frame) * ex_lanes + lane - 1u];
  return fr;
}

struct CoreDelta {
  double centroid_mm = 0.0;
  double max_vertex_mm = 0.0;
  size_t count = 0;
};

inline CoreDelta core_delta(const zc::CreatureType& t,
                            const zc::Clip& normal,
                            const zc::Clip& muted,
                            uint16_t frame,
                            const Carrier& carrier) {
  std::array<zc::mat3x4fx, zc::kMaxBones> pn{}, pm{};
  zc::decode_pose(t, normal, frame, pn, nullptr, 0);
  zc::decode_pose(t, muted, frame, pm, nullptr, 0);
  const zc::DeformFrame fn = clip_deform_frame(normal, frame);
  const zc::DeformFrame fm = clip_deform_frame(muted, frame);
  const int32_t y0 = kLoopNeckExitYMm - kLoopBuryMm;
  const int32_t want = fxu(y0 + carrier.station_mm);
  const int32_t win = fxu(carrier.half_mm);
  Point cn, cm;
  CoreDelta out;
  for (const zc::Meshlet& m : t.mesh) {
    for (size_t vi = 0; vi < m.verts.size(); ++vi) {
      const zc::SkinVertex& bind = m.verts[vi];
      if (carrier.bone != kBRearSocket &&
          (bind.y < want - win || bind.y > want + win))
        continue;
      const bool rigid =
          (bind.b0 == carrier.bone && bind.w0 == 64) ||
          (bind.b1 == carrier.bone && bind.w0 == 0);
      if (!rigid) continue;
      const zc::DeformVertex meta =
          vi < m.deform.size() ? m.deform[vi] : zc::DeformVertex{};
      const zc::SkinVertex vn =
          zc::deform_skin_vertex_lanes(bind, meta, fn);
      const zc::SkinVertex vm =
          zc::deform_skin_vertex_lanes(bind, meta, fm);
      const Point a = posed_vertex(pn, vn);
      const Point b = posed_vertex(pm, vm);
      cn.x += a.x; cn.y += a.y; cn.z += a.z;
      cm.x += b.x; cm.y += b.y; cm.z += b.z;
      out.max_vertex_mm = std::max(out.max_vertex_mm, norm(a - b));
      ++out.count;
    }
  }
  if (out.count != 0) {
    const double n = static_cast<double>(out.count);
    cn.x /= n; cn.y /= n; cn.z /= n;
    cm.x /= n; cm.y /= n; cm.z /= n;
    out.centroid_mm = norm(cn - cm);
  }
  return out;
}

inline bool clears_public_floor(const CoreDelta& d, const Carrier& carrier) {
  if (d.count == 0) return false;
  if (carrier.anchored) return d.max_vertex_mm >= kVisibleEffectMinMm;
  return d.centroid_mm >= kVisibleEffectMinMm &&
         d.max_vertex_mm >= kVisibleEffectMinMm;
}

}  // namespace u02::public_joint_metric

#endif  // ZHAO_REEL_MANAFOLD_PUBLIC_JOINT_METRIC_H
