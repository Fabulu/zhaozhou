// manafold_public_jointgate.cpp -- Direction 14's PUBLIC five-carrier proof.
//
// The private slot-16 gate proves that five carrier channels exist. This gate
// asks the owner's harder question: does each channel reach the VISIBLE SWELL
// SKIN in a shipping performance? It builds the real Taunt/Taunt-II clips with
// the production builders, then compares identical frames against F/A/B/C/E
// muted at swallow_nodules consumption.
//
// Usage:
//   manafold-public-jointgate.exe                 green shipping gate
//   manafold-public-jointgate.exe --fail-mute F  inverted red leg (F/A/B/C/E)
//
// A red leg returns zero only when the named public visible-skin check FAILS.
// Pictures remain the likeness evidence; this gate preserves the mechanism.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
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

// Fixed mechanism floor, deliberately below the final-resolution art decision:
// 20 mm is ~3 native pixels at this bank scale and far above integer rounding.
constexpr double kVisibleEffectMinMm = 20.0;

struct Vec3 {
  double x = 0, y = 0, z = 0;
};

Vec3 operator-(const Vec3& a, const Vec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double norm(const Vec3& a) {
  return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

struct Carrier {
  const char* name;
  uint8_t bone;
  int32_t station_mm;
  int32_t half_mm;
  bool anchored;
};

std::array<Carrier, 5> carriers() {
  return {{{"F", u02::kBJunctionF, u02::kKnuckleAtJfMm,
            u02::kLoopCarrierCoreHalfMm[0], true},
           {"A", u02::kBHingeA, u02::kKnuckleAtAMm,
            u02::kLoopCarrierCoreHalfMm[1], false},
           {"B", u02::kBHingeB, u02::kKnuckleAtBMm,
            u02::kLoopCarrierCoreHalfMm[2], false},
           {"C", u02::kBHingeC, u02::kKnuckleAtCMm,
            u02::kLoopCarrierCoreHalfMm[3], false},
           {"E", u02::kBRearSocket, u02::kKnuckleAtEndMm,
            u02::kLoopCarrierCoreHalfMm[4], true}}};
}

u02::PublicJointMute mute_for_index(int i) {
  static constexpr u02::PublicJointMute kMute[5] = {
      u02::PublicJointMute::kFront, u02::PublicJointMute::kA,
      u02::PublicJointMute::kB, u02::PublicJointMute::kC,
      u02::PublicJointMute::kEnd};
  return i >= 0 && i < 5 ? kMute[i] : u02::PublicJointMute::kNone;
}

int index_for_name(const char* s) {
  if (s == nullptr || s[0] == 0 || s[1] != 0) return -1;
  const char* names = "FABCE";
  const char* p = std::strchr(names, s[0]);
  return p == nullptr ? -1 : static_cast<int>(p - names);
}

const zc::Clip* clip_by_slot(const zc::CreatureType& t, uint16_t slot) {
  for (const zc::Clip& c : t.bank.clips)
    if (c.slot_id == slot) return &c;
  return nullptr;
}

zc::Clip build_public_clip(uint16_t slot, u02::PublicJointMute mute) {
  u02::g_u02_public_joint_mute = mute;
  zc::Clip c = slot == 11 ? u02::build_taunt() : u02::build_taunt2();
  u02::finalize_rear_follow(c);
  return c;
}

template <class T>
bool bytes_equal(const std::vector<T>& a, const std::vector<T>& b) {
  return a.size() == b.size() &&
         (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0);
}

bool source_clip_equal(const zc::Clip& a, const zc::Clip& b) {
  return a.slot_id == b.slot_id && a.frame_count == b.frame_count &&
         a.hold_last == b.hold_last && a.interpolate == b.interpolate &&
         a.wrap_root_delta == b.wrap_root_delta &&
         bytes_equal(a.quats, b.quats) && bytes_equal(a.root, b.root) &&
         bytes_equal(a.local_translation, b.local_translation) &&
         bytes_equal(a.uniform_scale_q15, b.uniform_scale_q15) &&
         bytes_equal(a.deform, b.deform) && bytes_equal(a.deform_ex, b.deform_ex);
}

bool body_channels_equal(const zc::Clip& normal, const zc::Clip& muted) {
  if (!bytes_equal(normal.root, muted.root) ||
      !bytes_equal(normal.deform, muted.deform))
    return false;
  const uint8_t fixed[] = {u02::kBRoot, u02::kBLoopBase2,
                           u02::kBEyeTravelL, u02::kBEyeTravelR,
                           u02::kBEyeL, u02::kBEyeR,
                           u02::kBPupilL, u02::kBPupilR};
  for (uint16_t f = 0; f < normal.frame_count; ++f)
    for (uint8_t bone : fixed) {
      const zc::quat16& a = normal.quats[static_cast<size_t>(f) * u02::kBoneCount + bone];
      const zc::quat16& b = muted.quats[static_cast<size_t>(f) * u02::kBoneCount + bone];
      if (std::memcmp(&a, &b, sizeof(a)) != 0) return false;
    }
  return true;
}

Vec3 root_local(const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
                int32_t wx, int32_t wy, int32_t wz) {
  const zc::mat3x4fx& rm = pose[u02::kBRoot];
  const int64_t dx = wx - rm.m[3], dy = wy - rm.m[7], dz = wz - rm.m[11];
  return {static_cast<double>(((rm.m[0] * dx + rm.m[4] * dy + rm.m[8] * dz) >> 16) * 1000 >> 16),
          static_cast<double>(((rm.m[1] * dx + rm.m[5] * dy + rm.m[9] * dz) >> 16) * 1000 >> 16),
          static_cast<double>(((rm.m[2] * dx + rm.m[6] * dy + rm.m[10] * dz) >> 16) * 1000 >> 16)};
}

Vec3 posed_vertex(const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
                   const zc::SkinVertex& v) {
  int32_t x, y, z;
  zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
  return root_local(pose, x, y, z);
}

zc::DeformFrame clip_deform_frame(const zc::Clip& c, uint16_t frame) {
  zc::DeformFrame fr;
  if (c.deform.size() == c.frame_count) fr.lane[0] = c.deform[frame];
  const size_t ex_lanes = static_cast<size_t>(zc::kDeformLaneCount) - 1u;
  if (c.deform_ex.size() == static_cast<size_t>(c.frame_count) * ex_lanes)
    for (size_t lane = 1; lane < zc::kDeformLaneCount; ++lane)
      fr.lane[lane] = c.deform_ex[static_cast<size_t>(frame) * ex_lanes + lane - 1u];
  return fr;
}

struct CoreDelta {
  double centroid_mm = 0;
  double max_vertex_mm = 0;
  size_t count = 0;
};

CoreDelta core_delta(const zc::CreatureType& t, const zc::Clip& normal,
                     const zc::Clip& muted, uint16_t frame,
                     const Carrier& carrier) {
  std::array<zc::mat3x4fx, zc::kMaxBones> pn{}, pm{};
  zc::decode_pose(t, normal, frame, pn, nullptr, 0);
  zc::decode_pose(t, muted, frame, pm, nullptr, 0);
  const zc::DeformFrame fn = clip_deform_frame(normal, frame);
  const zc::DeformFrame fm = clip_deform_frame(muted, frame);
  const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  const int32_t want = u02::fxu(y0 + carrier.station_mm);
  const int32_t win = u02::fxu(carrier.half_mm);
  Vec3 cn, cm;
  CoreDelta out;
  for (const zc::Meshlet& m : t.mesh) {
    for (size_t vi = 0; vi < m.verts.size(); ++vi) {
      const zc::SkinVertex& bind = m.verts[vi];
      // RearSocket is a unique body-root carrier: every rigidly owned vertex is
      // part of the End swell, so no station window is needed (its translated
      // body-surface bind no longer shares the chain's authored Y coordinate).
      if (carrier.bone != u02::kBRearSocket &&
          (bind.y < want - win || bind.y > want + win))
        continue;
      const bool rigid =
          (bind.b0 == carrier.bone && bind.w0 == 64) ||
          (bind.b1 == carrier.bone && bind.w0 == 0);
      if (!rigid) continue;
      const zc::DeformVertex meta =
          vi < m.deform.size() ? m.deform[vi] : zc::DeformVertex{};
      const zc::SkinVertex vn = zc::deform_skin_vertex_lanes(bind, meta, fn);
      const zc::SkinVertex vm = zc::deform_skin_vertex_lanes(bind, meta, fm);
      const Vec3 a = posed_vertex(pn, vn);
      const Vec3 b = posed_vertex(pm, vm);
      cn.x += a.x; cn.y += a.y; cn.z += a.z;
      cm.x += b.x; cm.y += b.y; cm.z += b.z;
      out.max_vertex_mm = std::max(out.max_vertex_mm, norm(a - b));
      ++out.count;
    }
  }
  if (out.count != 0) {
    cn.x /= out.count; cn.y /= out.count; cn.z /= out.count;
    cm.x /= out.count; cm.y /= out.count; cm.z /= out.count;
    out.centroid_mm = norm(cn - cm);
  }
  return out;
}

int public_peak(uint16_t slot, int carrier_index) {
  const int start = slot == 11 ? u02::kTauntJointBeatKey : u02::kTaunt2JointBeatKey;
  const int stagger = slot == 11 ? u02::kTauntJointBeatStagger
                                 : u02::kTaunt2JointBeatStagger;
  const int width = slot == 11 ? u02::kTauntJointBeatWidth
                               : u02::kTaunt2JointBeatWidth;
  return start + carrier_index * stagger + width * 2 / 5;
}

bool public_input_guard(u02::PublicJointMute mute) {
  int32_t v[5] = {11, 22, 33, 44, 55};
  const int32_t expected[5] = {11, 22, 33, 44, 55};
  u02::g_u02_public_joint_mute = mute;
  u02::apply_public_joint_mute(v);
  const int muted = u02::public_joint_mute_index(mute);
  for (int i = 0; i < 5; ++i) {
    const int32_t want = i == muted ? 0 : expected[i];
    if (v[i] != want) return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (const char* e = std::getenv("ZHAO_U02_FRONT_JOINT_PER_MM")) {
    const int v = std::atoi(e);
    if (v <= 0 || v > 64) return 2;
    u02::g_u02_swallow_front_joint_per_mm = v;
  }
  if (const char* e = std::getenv("ZHAO_U02_END_JOINT_PER_MM")) {
    const int v = std::atoi(e);
    if (v <= 0 || v > 64) return 2;
    u02::g_u02_swallow_end_joint_per_mm = v;
  }
  int candidate_index = -1;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--fail-mute") == 0 && i + 1 < argc) {
      candidate_index = index_for_name(argv[++i]);
      if (candidate_index < 0) {
        std::fprintf(stderr, "mjointpub: --fail-mute expects F|A|B|C|E\n");
        return 2;
      }
    } else {
      std::fprintf(stderr, "mjointpub: unknown argument %s\n", argv[i]);
      return 2;
    }
  }

  u02::g_u02_public_joint_mute = u02::PublicJointMute::kNone;
  const zc::CreatureType& t = u02::type();
  std::array<std::array<zc::Clip, 2>, 6> clips;
  for (int mi = -1; mi < 5; ++mi) {
    clips[mi + 1][0] = build_public_clip(11, mute_for_index(mi));
    clips[mi + 1][1] = build_public_clip(12, mute_for_index(mi));
  }
  u02::g_u02_public_joint_mute = u02::PublicJointMute::kNone;

  int failures = 0;
  for (int si = 0; si < 2; ++si) {
    const uint16_t slot = si == 0 ? 11 : 12;
    const zc::Clip* shipped = clip_by_slot(t, slot);
    if (shipped == nullptr || !source_clip_equal(clips[0][si], *shipped)) {
      std::printf("FAIL: direct public slot %u build differs from shipping\n", slot);
      ++failures;
    }
    for (int ci = 0; ci < 5; ++ci)
      if (!body_channels_equal(clips[0][si], clips[ci + 1][si])) {
        std::printf("FAIL: slot %u mute %s changed root/deform/body channels\n",
                    slot, carriers()[ci].name);
        ++failures;
      }
  }

  const u02::PublicJointMute candidate_mute = mute_for_index(candidate_index);
  if (!public_input_guard(candidate_mute)) {
    std::printf("FAIL: public mute changed an input other than its named carrier\n");
    ++failures;
  }

  const auto cs = carriers();
  std::printf("PUBLIC FIVE-CARRIER VISIBLE-SKIN GATE (candidate %s)\n",
              candidate_index < 0 ? "shipping" : cs[candidate_index].name);
  std::printf("  fixed mechanism floor %.1f mm (~3 native px); pictures decide likeness\n",
              kVisibleEffectMinMm);
  std::printf("  carrier cores  Taunt centroid/max     Taunt-II centroid/max    verdict\n");
  bool carrier_ok[5] = {false, false, false, false, false};
  for (int ci = 0; ci < 5; ++ci) {
    CoreDelta d[2];
    for (int si = 0; si < 2; ++si) {
      const uint16_t slot = si == 0 ? 11 : 12;
      const int frame = public_peak(slot, ci);
      d[si] = core_delta(t, clips[candidate_index + 1][si],
                         clips[ci + 1][si], static_cast<uint16_t>(frame), cs[ci]);
    }
    bool ok = d[0].count != 0 && d[1].count != 0;
    if (cs[ci].anchored)
      ok = ok && std::max(d[0].max_vertex_mm, d[1].max_vertex_mm) >= kVisibleEffectMinMm;
    else
      ok = ok && std::max(d[0].centroid_mm, d[1].centroid_mm) >= kVisibleEffectMinMm &&
           std::max(d[0].max_vertex_mm, d[1].max_vertex_mm) >= kVisibleEffectMinMm;
    carrier_ok[ci] = ok;
    std::printf("  %-7s %3zu/%-3zu %7.2f/%7.2f      %7.2f/%7.2f       %s\n",
                cs[ci].name, d[0].count, d[1].count,
                d[0].centroid_mm, d[0].max_vertex_mm,
                d[1].centroid_mm, d[1].max_vertex_mm, ok ? "OK" : "FAIL");
    if (!ok) ++failures;
  }

  const int influenced = [&] {
    int n = 0;
    for (const zc::Meshlet& m : t.mesh)
      for (const zc::SkinVertex& v : m.verts)
        if ((v.b0 == u02::kBRearSocket && v.w0 > 0) ||
            (v.b1 == u02::kBRearSocket && v.w0 < 64))
          ++n;
    return n;
  }();
  const double rx = static_cast<double>(u02::kRearSocketTargetXMm) / u02::kBodyRadiusMm;
  const double ry = static_cast<double>(u02::kRearSocketTargetYMm) /
                    u02::vmm(u02::kBodyRadiusMm);
  std::printf("rear socket: %d influenced vertices, authored rho %.3f\n",
              influenced, std::sqrt(rx * rx + ry * ry));
  if (influenced == 0) {
    std::printf("FAIL: rear socket owns no visible skin\n");
    ++failures;
  }

  const bool leg = candidate_index >= 0;
  std::printf("%s: %d failure(s)%s\n", failures ? "FAIL" : "PASS", failures,
              leg ? " [FAILABLE PUBLIC MUTE ENGAGED]" : "");
  if (leg) {
    if (carrier_ok[candidate_index]) {
      std::fprintf(stderr,
                   "mjointpub: red leg %s did not remove its own public visible-skin beat\n",
                   cs[candidate_index].name);
      return 1;
    }
    std::printf("RED LEG OK: mute %s makes its own public visible-skin gate fail\n",
                cs[candidate_index].name);
    return 0;
  }
  return failures ? 1 : 0;
}
