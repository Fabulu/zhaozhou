// manafold_spangate.cpp -- Pass 17 signed-span structural gate
//
// Direction 16 requires the visible F-A, A-B, B-C and C-End sticks to consume
// signed endpoint distance: extension and compaction. The production mechanism
// is ordinary two-weight skinning between an upstream articulation and co-located
// translation-only SpanDelta palettes. A/B/C write the full signed local-Y delta
// to the real child and a constant-slope fraction to the partial helper; the
// rigid carrier and descendants still land at the exact endpoint. Rear
// finalization writes full and staged C-End receipts while RearSocket stays
// attached to the deformed body target.
//
// This gate reads the compiled skin, decoded palettes and authored key/midpoint
// tracks. It deliberately does not choose public amplitudes; pictures do that.
//
// Failable controls mutate the gate's real inputs and must return non-zero:
//   --fail-rigid-span F-A|A-B|B-C|C-E
//   --fail-clamp-negative F-A|A-B|B-C|C-E
//   --fail-delta-drift A|B|C|E
//   --fail-overcompact F-A|A-B|B-C|C-E
//   --fail-e-start
//   --fail-e-mid
//   --fail-e-presocket
//   --fail-posed-order
//   --csv  print shipping signed-delta traces

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
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

int g_failures = 0;

// Closure uses quantized angle16 aim and integer-millimetre distance before the
// 90 mm delta-to-socket bend zone. This is a structural coincidence tolerance,
// not an art value; the observed shipping worst remains printed beside it.
constexpr double kRearEndpointToleranceMm = 10.0;

void fail(const char* what) {
  std::printf("  FAIL: %s\n", what);
  ++g_failures;
}

struct Vec3 {
  double x = 0.0, y = 0.0, z = 0.0;
};

Vec3 operator-(const Vec3& a, const Vec3& b) {
  return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

double length(const Vec3& v) {
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

double cross_length(const Vec3& a, const Vec3& b) {
  const double x = a.y * b.z - a.z * b.y;
  const double y = a.z * b.x - a.x * b.z;
  const double z = a.x * b.y - a.y * b.x;
  return std::sqrt(x * x + y * y + z * z);
}

double dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 posed_point(const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
                  uint8_t bone, int32_t x, int32_t y, int32_t z = 0) {
  const zc::SkinVertex v{x, y, z, bone, bone, 64};
  int32_t ox = 0, oy = 0, oz = 0;
  zc::skin_vertex(pose.data(), v, ox, oy, oz, nullptr);
  constexpr double kToMm = 1000.0 / 65536.0;
  return Vec3{ox * kToMm, oy * kToMm, oz * kToMm};
}

bool matrix_equal(const zc::mat3x4fx& a, const zc::mat3x4fx& b) {
  return std::memcmp(a.m, b.m, sizeof(a.m)) == 0;
}

enum class Span : int { kFA = 0, kAB, kBC, kCE, kNone };

Span parse_span(const char* s) {
  if (s == nullptr) return Span::kNone;
  if (std::strcmp(s, "F-A") == 0 || std::strcmp(s, "FA") == 0)
    return Span::kFA;
  if (std::strcmp(s, "A-B") == 0 || std::strcmp(s, "AB") == 0)
    return Span::kAB;
  if (std::strcmp(s, "B-C") == 0 || std::strcmp(s, "BC") == 0)
    return Span::kBC;
  if (std::strcmp(s, "C-E") == 0 || std::strcmp(s, "CE") == 0)
    return Span::kCE;
  return Span::kNone;
}

Span parse_drift(const char* s) {
  if (s == nullptr) return Span::kNone;
  if (std::strcmp(s, "A") == 0) return Span::kFA;
  if (std::strcmp(s, "B") == 0) return Span::kAB;
  if (std::strcmp(s, "C") == 0) return Span::kBC;
  if (std::strcmp(s, "E") == 0) return Span::kCE;
  return Span::kNone;
}

const char* span_name(Span s) {
  static constexpr const char* kNames[4] = {"F-A", "A-B", "B-C", "C-E"};
  const int i = static_cast<int>(s);
  return i >= 0 && i < 4 ? kNames[i] : "none";
}

struct Stations {
  int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  int32_t st_neck = u02::kLoopBuryMm + u02::kLoopArcMm[0];
  int32_t st_a = st_neck + u02::kLoopArcMm[1];
  int32_t st_b = st_a + u02::kLoopArcMm[2];
  int32_t st_c = st_b + u02::kLoopArcMm[3];
  int32_t st_d = st_c + u02::kLoopArcMm[4];
  int32_t total = st_d + u02::kLoopArcMm[5];
  int32_t st_e = u02::kKnuckleAtEndMm;

  int32_t in_n0 = st_neck - u02::kLoopCarrierCoreHalfMm[0] - u02::kFoldBlendMm[0];
  int32_t in_n1 = st_neck - u02::kLoopCarrierCoreHalfMm[0];
  int32_t out_n0 = st_neck + u02::kLoopCarrierCoreHalfMm[0];
  int32_t out_n1 = out_n0 + u02::kFoldBlendMm[0];

  int32_t in_a0 = st_a - u02::kLoopCarrierCoreHalfMm[1] - u02::kFoldBlendMm[1];
  int32_t in_a1 = st_a - u02::kLoopCarrierCoreHalfMm[1];
  int32_t out_a0 = st_a + u02::kLoopCarrierCoreHalfMm[1];
  int32_t in_b0 = st_b - u02::kLoopCarrierCoreHalfMm[2] - u02::kFoldBlendMm[2];
  int32_t in_b1 = st_b - u02::kLoopCarrierCoreHalfMm[2];
  int32_t out_b0 = st_b + u02::kLoopCarrierCoreHalfMm[2];
  int32_t in_c0 = st_c - u02::kLoopCarrierCoreHalfMm[3] - u02::kFoldBlendMm[3];
  int32_t in_c1 = st_c - u02::kLoopCarrierCoreHalfMm[3];
  int32_t out_c0 = st_c + u02::kLoopCarrierCoreHalfMm[3];
  int32_t out_c1 = out_c0 + u02::kFoldBlendMm[3];
  int32_t mid_e = out_c0 + u02::kSpanEMidRunMm;
  int32_t in_e0 = st_e - u02::kLoopCarrierCoreHalfMm[4] - u02::kFoldBlendMm[4];
  int32_t in_e1 = st_e - u02::kLoopCarrierCoreHalfMm[4];
  int32_t out_e0 = st_e + u02::kLoopCarrierCoreHalfMm[4];
  int32_t out_e1 = out_e0 + u02::kFoldBlendMm[4];
};

constexpr int kSpanCount = 4;

struct SpanSpec {
  const char* name;
  uint8_t lower;
  uint8_t partial_helper;
  uint8_t full_receipt;
  uint8_t child;
  int32_t centre_station;
  int32_t centre_length;
  int32_t gradient_start;
  int32_t gradient_end;
  int32_t helper_run;
};

std::array<SpanSpec, kSpanCount> span_specs(const Stations& s) {
  return {{
      {"F-A", u02::kBNeck, u02::kBSpanDeltaA, u02::kBHingeA,
       u02::kBHingeA, s.st_a, u02::kLoopArcMm[1], s.out_n1, s.in_a1,
       u02::kSpanHelperRunMm[0]},
      {"A-B", u02::kBHingeA, u02::kBSpanDeltaB, u02::kBHingeB,
       u02::kBHingeB, s.st_b, u02::kLoopArcMm[2], s.out_a0, s.in_b1,
       u02::kSpanHelperRunMm[1]},
      {"B-C", u02::kBHingeB, u02::kBSpanDeltaC, u02::kBHingeC,
       u02::kBHingeC, s.st_c, u02::kLoopArcMm[3], s.out_b0, s.in_c1,
       u02::kSpanHelperRunMm[2]},
      {"C-E", u02::kBHingeC, u02::kBSpanDeltaEStart,
       u02::kBSpanDeltaE, u02::kBRearSocket, s.st_e,
       u02::kRearSocketFromCMm, s.out_c0, s.in_e1,
       u02::kSpanHelperRunMm[3]},
  }};
}

bool is_loop_bone(uint8_t b) {
  return (b >= u02::kBJunctionF && b <= u02::kBHingeD) ||
         b == u02::kBRearSocket || b == u02::kBReturnTip ||
         b == u02::kBSpanDeltaA || b == u02::kBSpanDeltaB ||
         b == u02::kBSpanDeltaC || b == u02::kBSpanDeltaE ||
         b == u02::kBSpanDeltaEStart || b == u02::kBSpanDeltaEMid ||
         b == u02::kBSpanDeltaEPreSocket;
}

bool is_loop_meshlet(const zc::Meshlet& m) {
  for (const zc::SkinVertex& v : m.verts)
    if (is_loop_bone(v.b0) || is_loop_bone(v.b1)) return true;
  return false;
}

int32_t ramp64(int32_t s, int32_t start, int32_t end) {
  if (s <= start) return 0;
  if (s >= end) return 64;
  return static_cast<int32_t>(
      (static_cast<int64_t>(s - start) * 64) / (end - start));
}

struct ExpectedSkin {
  uint8_t b0 = 0, b1 = 0, w0 = 64;
  int zone = -1;
};

ExpectedSkin pair(uint8_t lower, uint8_t upper, int32_t t, int zone) {
  if (t < 0) t = 0;
  if (t > 64) t = 64;
  return ExpectedSkin{lower, upper, static_cast<uint8_t>(64 - t), zone};
}

ExpectedSkin expected_skin(int32_t x, const Stations& s) {
  if (x < s.in_n1)
    return pair(u02::kBRoot, u02::kBJunctionF, ramp64(x, s.in_n0, s.in_n1), 0);
  if (x < s.out_n0)
    return pair(u02::kBJunctionF, u02::kBNeck, 0, 1);
  if (x < s.out_n1)
    return pair(u02::kBJunctionF, u02::kBNeck, ramp64(x, s.out_n0, s.out_n1), 2);
  if (x < s.in_a0)
    return pair(u02::kBNeck, u02::kBSpanDeltaA, ramp64(x, s.out_n1, s.in_a0), 3);
  if (x < s.in_a1)
    return pair(u02::kBSpanDeltaA, u02::kBHingeA, ramp64(x, s.in_a0, s.in_a1), 4);
  if (x < s.out_a0)
    return pair(u02::kBHingeA, u02::kBHingeA, 0, 5);
  if (x < s.in_b0)
    return pair(u02::kBHingeA, u02::kBSpanDeltaB, ramp64(x, s.out_a0, s.in_b0), 6);
  if (x < s.in_b1)
    return pair(u02::kBSpanDeltaB, u02::kBHingeB, ramp64(x, s.in_b0, s.in_b1), 7);
  if (x < s.out_b0)
    return pair(u02::kBHingeB, u02::kBHingeB, 0, 8);
  if (x < s.in_c0)
    return pair(u02::kBHingeB, u02::kBSpanDeltaC, ramp64(x, s.out_b0, s.in_c0), 9);
  if (x < s.in_c1)
    return pair(u02::kBSpanDeltaC, u02::kBHingeC, ramp64(x, s.in_c0, s.in_c1), 10);
  if (x < s.out_c0)
    return pair(u02::kBHingeC, u02::kBHingeC, 0, 11);
  if (x < s.out_c1)
    return pair(u02::kBHingeC, u02::kBSpanDeltaEStart,
                ramp64(x, s.out_c0, s.out_c1), 12);
  if (x < s.mid_e)
    return pair(u02::kBSpanDeltaEStart, u02::kBSpanDeltaEMid,
                ramp64(x, s.out_c1, s.mid_e), 13);
  if (x < s.in_e0)
    return pair(u02::kBSpanDeltaEMid, u02::kBSpanDeltaEPreSocket,
                ramp64(x, s.mid_e, s.in_e0), 14);
  if (x < s.in_e1)
    return pair(u02::kBSpanDeltaEPreSocket, u02::kBRearSocket,
                ramp64(x, s.in_e0, s.in_e1), 15);
  if (x < s.out_e0)
    return pair(u02::kBRearSocket, u02::kBRearSocket, 0, 16);
  if (x < s.out_e1)
    return pair(u02::kBRearSocket, u02::kBReturnTip,
                ramp64(x, s.out_e0, s.out_e1), 17);
  return pair(u02::kBReturnTip, u02::kBReturnTip, 0, 18);
}

std::map<int32_t, int32_t> ring_station_map(const Stations& s) {
  std::map<int32_t, int32_t> out;
  for (int i = 0; i < u02::kLoopRings; ++i) {
    const int32_t x = static_cast<int32_t>(
        (static_cast<int64_t>(s.total) * i) / (u02::kLoopRings - 1));
    out[u02::fxu(s.y0 + x)] = x;
  }
  return out;
}

void mutate_rigid_span(zc::CreatureType& type, Span selected,
                       const Stations& stations) {
  if (selected == Span::kNone) return;
  const auto specs = span_specs(stations);
  const SpanSpec& spec = specs[static_cast<int>(selected)];
  const auto ring_map = ring_station_map(stations);
  for (zc::Meshlet& m : type.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (zc::SkinVertex& v : m.verts) {
      const auto it = ring_map.find(v.y);
      if (it == ring_map.end()) continue;
      const int32_t x = it->second;
      if (x >= spec.gradient_start && x < spec.gradient_end) {
        v.b0 = spec.lower;
        v.b1 = spec.lower;
        v.w0 = 64;
      }
    }
  }
}

void check_compiled_zones(const zc::CreatureType& type,
                          const Stations& stations) {
  const auto ring_map = ring_station_map(stations);
  std::array<size_t, 19> zone_count{};
  size_t vertices = 0;
  size_t mismatches = 0;
  size_t bad_ring_y = 0;
  size_t bad_lane_meta = 0;

  for (const zc::Meshlet& m : type.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (size_t i = 0; i < m.verts.size(); ++i) {
      const zc::SkinVertex& v = m.verts[i];
      const auto it = ring_map.find(v.y);
      if (it == ring_map.end()) {
        ++bad_ring_y;
        continue;
      }
      const ExpectedSkin e = expected_skin(it->second, stations);
      ++vertices;
      if (e.zone >= 0) ++zone_count[static_cast<size_t>(e.zone)];
      if (v.b0 != e.b0 || v.b1 != e.b1 || v.w0 != e.w0)
        ++mismatches;
      if (!m.deform.empty()) {
        const zc::DeformVertex& d = m.deform[i];
        if (d.strength_ex[0] != 0 || d.strength_ex[1] != 0 ||
            d.strength_ex[2] != 0)
          ++bad_lane_meta;
      }
    }
  }

  size_t empty_zones = 0;
  for (size_t i = 0; i < zone_count.size(); ++i)
    if (zone_count[i] == 0) ++empty_zones;
  std::printf("G1 compiled zones: %zu loop vertices, %zu pair/weight mismatches, "
              "%zu unknown ring-y, %zu empty zones\n",
              vertices, mismatches, bad_ring_y, empty_zones);
  std::printf("   rigid carrier cores F/A/B/C/E: %zu/%zu/%zu/%zu/%zu vertices\n",
              zone_count[1], zone_count[5], zone_count[8], zone_count[11],
              zone_count[16]);
  if (vertices == 0) fail("compiled loop skin was not found");
  if (mismatches != 0)
    fail("compiled loop ownership differs from the named signed-span zone table");
  if (bad_ring_y != 0)
    fail("a compiled loop vertex does not belong to one of the authored rings");
  if (empty_zones != 0)
    fail("one or more signed-span/core/bend zones have no compiled vertices");
  if (bad_lane_meta != 0)
    fail("retired deform lanes 1..3 still own loop vertices");
}

void check_lane_samples(const zc::CreatureType& type) {
  size_t samples = 0, nonzero = 0;
  const size_t ex = static_cast<size_t>(zc::kDeformLaneCount) - 1u;
  const auto scan = [&](const std::vector<zc::DeformSample>& v) {
    if (v.empty()) return;
    for (size_t i = 0; i < v.size(); ++i) {
      const size_t lane = i % ex;
      if (lane >= 3) continue;
      ++samples;
      if (v[i].flatten != 0 || v[i].spread != 0) ++nonzero;
    }
  };
  for (const zc::Clip& c : type.bank.clips) {
    scan(c.deform_ex);
    scan(c.mid_deform_ex);
  }
  std::printf("G2 retired lane samples: %zu checked, %zu nonzero\n",
              samples, nonzero);
  if (nonzero != 0)
    fail("retired deform lanes 1..3 still carry authored samples");
}

zc::Clip straight_clip() {
  zc::Clip c = u02::clip_shell(7, 1, 0);
  c.interpolate = false;
  return c;
}

void check_identity_palettes(const zc::CreatureType& type) {
  zc::Clip c = straight_clip();
  u02::Rig g;
  g.reset();
  g.write(c, 0);
  std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
  zc::decode_pose(type, c, 0, pose, nullptr, 0);
  constexpr uint8_t kParent[7] = {u02::kBNeck, u02::kBHingeA,
                                  u02::kBHingeB, u02::kBHingeD,
                                  u02::kBHingeD, u02::kBHingeD,
                                  u02::kBHingeD};
  constexpr uint8_t kHelper[7] = {u02::kBSpanDeltaA, u02::kBSpanDeltaB,
                                  u02::kBSpanDeltaC, u02::kBSpanDeltaE,
                                  u02::kBSpanDeltaEStart,
                                  u02::kBSpanDeltaEMid,
                                  u02::kBSpanDeltaEPreSocket};
  int different = 0;
  for (int i = 0; i < 7; ++i)
    if (!matrix_equal(pose[kParent[i]], pose[kHelper[i]])) ++different;
  std::printf("G3 zero-delta helper identity: %d/7 palettes differ from parent\n",
              different);
  if (different != 0)
    fail("a zero-delta helper palette is not exactly its parent palette");
}

void set_synthetic_delta(u02::Rig& g, int span_index, int32_t delta_mm,
                         bool clamp_negative, bool drift,
                         bool e_start_drift, bool e_mid_drift,
                         bool e_presocket_drift) {
  const int32_t applied = clamp_negative && delta_mm < 0 ? 0 : delta_mm;
  if (span_index < 3) {
    g.set_span_delta(span_index, applied);
    if (drift)
      g.local_t[u02::kBSpanDeltaA + span_index][1] = 0;
  } else {
    const int32_t t = u02::fxu(applied);
    g.local_t[u02::kBSpanDeltaEStart][1] =
        e_start_drift ? 0 : u02::span_e_start_delta_fx(t);
    g.local_t[u02::kBSpanDeltaEMid][1] =
        e_mid_drift ? 0 : u02::span_e_mid_delta_fx(t);
    g.local_t[u02::kBSpanDeltaEPreSocket][1] =
        e_presocket_drift ? 0 : u02::span_e_presocket_delta_fx(t);
    g.local_t[u02::kBSpanDeltaE][1] = drift ? 0 : t;
    g.local_t[u02::kBRearSocket][1] = t;
    g.local_t[u02::kBReturnTip][1] = t;
  }
}

struct RingAccum {
  Vec3 p{};
  int count = 0;
};

double minimum_free_ring_step_y(
    const zc::CreatureType& type,
    const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
    const Stations& stations, int32_t free_start, int32_t free_end) {
  const auto ring_map = ring_station_map(stations);
  std::map<int32_t, RingAccum> rings;
  for (const zc::Meshlet& m : type.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (const zc::SkinVertex& v : m.verts) {
      const auto it = ring_map.find(v.y);
      if (it == ring_map.end()) continue;
      int32_t ox = 0, oy = 0, oz = 0;
      zc::skin_vertex(pose.data(), v, ox, oy, oz, nullptr);
      constexpr double kToMm = 1000.0 / 65536.0;
      RingAccum& a = rings[it->second];
      a.p.x += ox * kToMm;
      a.p.y += oy * kToMm;
      a.p.z += oz * kToMm;
      ++a.count;
    }
  }
  double min_step = std::numeric_limits<double>::infinity();
  bool have_previous = false;
  Vec3 previous{};
  for (auto& entry : rings) {
    if (entry.first < free_start || entry.first > free_end) continue;
    RingAccum& a = entry.second;
    if (a.count <= 0) continue;
    a.p.x /= a.count;
    a.p.y /= a.count;
    a.p.z /= a.count;
    if (have_previous) min_step = std::min(min_step, a.p.y - previous.y);
    previous = a.p;
    have_previous = true;
  }
  return min_step;
}

void check_synthetic_signs(const zc::CreatureType& type,
                           const Stations& stations,
                           Span clamp_span, Span drift_span,
                           Span overcompact_span, bool e_start_drift,
                           bool e_mid_drift, bool e_presocket_drift) {
  const auto specs = span_specs(stations);
  const int32_t x = u02::fxu(u02::kLoopTubeXMm);
  for (int si = 0; si < kSpanCount; ++si) {
    const Span selected = static_cast<Span>(si);
    const SpanSpec& spec = specs[si];
    const int32_t legal[2] = {
        static_cast<int32_t>(
            (static_cast<int64_t>(spec.centre_length) *
             u02::kSpanStretchMaxPm[si]) / 1000),
        static_cast<int32_t>(
            (static_cast<int64_t>(spec.centre_length) *
             u02::kSpanCompactionMinPm[si]) / 1000),
    };
    for (int leg = 0; leg < 2; ++leg) {
      int32_t requested = legal[leg];
      if (leg == 1 && selected == overcompact_span)
        requested = -(u02::kSpanGradientMm[si] + u02::kSpanMinRunMm);

      zc::Clip rest = straight_clip();
      u02::Rig gr;
      gr.reset();
      gr.write(rest, 0);
      std::array<zc::mat3x4fx, zc::kMaxBones> rest_pose{};
      zc::decode_pose(type, rest, 0, rest_pose, nullptr, 0);

      zc::Clip moved = straight_clip();
      u02::Rig gm;
      gm.reset();
      set_synthetic_delta(gm, si, requested,
                          leg == 1 && selected == clamp_span,
                          leg == 1 && selected == drift_span,
                          leg == 1 && si == 3 && e_start_drift,
                          leg == 1 && si == 3 && e_mid_drift,
                          leg == 1 && si == 3 && e_presocket_drift);
      const int32_t full_delta_fx =
          si < 3 ? gm.local_t[spec.child][1]
                 : gm.local_t[u02::kBSpanDeltaE][1];
      if (si < 3 &&
          gm.local_t[spec.partial_helper][1] !=
              u02::span_helper_delta_fx(si, full_delta_fx))
        fail("A/B/C partial helper does not carry its constant-slope fraction");
      if (si == 3 &&
          (gm.local_t[u02::kBSpanDeltaEStart][1] !=
               u02::span_e_start_delta_fx(full_delta_fx) ||
           gm.local_t[u02::kBSpanDeltaEMid][1] !=
               u02::span_e_mid_delta_fx(full_delta_fx) ||
           gm.local_t[u02::kBSpanDeltaEPreSocket][1] !=
               u02::span_e_presocket_delta_fx(full_delta_fx)))
        fail("C-End staged helpers do not carry their constant-slope fractions");
      gm.write(moved, 0);
      std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
      zc::decode_pose(type, moved, 0, pose, nullptr, 0);

      const int32_t y = u02::fxu(stations.y0 + spec.centre_station);
      const Vec3 p0 = posed_point(rest_pose, spec.child, x, y);
      const Vec3 pc = posed_point(pose, spec.child, x, y);
      const Vec3 ph = posed_point(pose, spec.full_receipt, x, y);
      const double endpoint_error = length(pc - ph);
      const double signed_y = pc.y - p0.y;
      const double movement_error = std::abs(signed_y - requested);
      const double min_step = minimum_free_ring_step_y(
          type, pose, stations, spec.gradient_start, spec.gradient_end);
      const int32_t visible_run = u02::kSpanGradientMm[si] + requested;

      std::printf("G4 synthetic %-3s %s %+4d mm: endpoint err %.3f, "
                  "signed y %+7.2f, run %d, min ring dy %.2f mm\n",
                  spec.name, leg == 0 ? "extend" : "compact", requested,
                  endpoint_error, signed_y, visible_run, min_step);
      if (endpoint_error > 0.6)
        fail("child/socket endpoint does not coincide with its span-delta skin endpoint");
      if (movement_error > 0.6)
        fail("a signed span request was clamped, lost or changed sign");
      if (!(min_step > 0.05))
        fail("ordered loop rings collapsed or inverted under signed span motion");
      if (leg == 1 && selected != overcompact_span &&
          visible_run < u02::kSpanMinRunMm)
        fail("the named legal compaction floor violates the signed-run margin");
    }
  }
}

int32_t sample_local_y(const zc::Clip& c, int frame, uint8_t bone,
                       uint8_t sub) {
  const std::vector<int32_t>& v =
      sub == 0 ? c.local_translation : c.mid_local_translation;
  const size_t want = static_cast<size_t>(c.frame_count) * u02::kBoneCount * 3u;
  if (v.size() != want) return 0;
  return v[(static_cast<size_t>(frame) * u02::kBoneCount + bone) * 3u + 1u];
}

void check_shipping_tracks(const zc::CreatureType& type,
                           const Stations& stations, bool csv) {
  const auto specs = span_specs(stations);
  constexpr uint8_t kHelper[3] = {u02::kBSpanDeltaA, u02::kBSpanDeltaB,
                                  u02::kBSpanDeltaC};
  constexpr uint8_t kChild[3] = {u02::kBHingeA, u02::kBHingeB,
                                 u02::kBHingeC};
  int fraction_mismatch = 0;
  int e_stage_mismatch = 0;
  int bound_fail = 0;
  std::array<int32_t, 4> lo;
  std::array<int32_t, 4> hi;
  lo.fill(std::numeric_limits<int32_t>::max());
  hi.fill(std::numeric_limits<int32_t>::min());

  if (csv)
    std::printf("\nslot,key,sub,FA_mm,AB_mm,BC_mm,CE_mm\n");

  for (const zc::Clip& c : type.bank.clips) {
    for (int f = 0; f < c.frame_count; ++f) {
      for (uint8_t sub = 0; sub <= 1; ++sub) {
        if (sub == 1 && c.mid_local_translation.empty()) continue;
        int32_t mm[4]{};
        for (int si = 0; si < 3; ++si) {
          const int32_t full = sample_local_y(c, f, kChild[si], sub);
          const int32_t partial = sample_local_y(c, f, kHelper[si], sub);
          if (partial != u02::span_helper_delta_fx(si, full))
            ++fraction_mismatch;
          mm[si] = static_cast<int32_t>(
              (static_cast<int64_t>(full) * 1000) >> 16);
        }
        const int32_t e_full =
            sample_local_y(c, f, u02::kBSpanDeltaE, sub);
        const int32_t e_start =
            sample_local_y(c, f, u02::kBSpanDeltaEStart, sub);
        const int32_t e_mid =
            sample_local_y(c, f, u02::kBSpanDeltaEMid, sub);
        const int32_t e_presocket =
            sample_local_y(c, f, u02::kBSpanDeltaEPreSocket, sub);
        if (e_start != u02::span_e_start_delta_fx(e_full) ||
            e_mid != u02::span_e_mid_delta_fx(e_full) ||
            e_presocket != u02::span_e_presocket_delta_fx(e_full))
          ++e_stage_mismatch;
        mm[3] = static_cast<int32_t>(
            (static_cast<int64_t>(e_full) * 1000) >> 16);
        for (int si = 0; si < 4; ++si) {
          lo[si] = std::min(lo[si], mm[si]);
          hi[si] = std::max(hi[si], mm[si]);
          const int64_t scaled = static_cast<int64_t>(mm[si]) * 1000;
          const int32_t pm = specs[si].centre_length > 0
                                 ? static_cast<int32_t>(scaled /
                                                        specs[si].centre_length)
                                 : 0;
          const int32_t run_len = u02::kSpanGradientMm[si] + mm[si];
          if (pm > u02::kSpanStretchMaxPm[si] + 2 ||
              pm < u02::kSpanCompactionMinPm[si] - 2 ||
              run_len < u02::kSpanMinRunMm)
            ++bound_fail;
        }
        if (csv)
          std::printf("%u,%d,%u,%d,%d,%d,%d\n", c.slot_id, f, sub,
                      mm[0], mm[1], mm[2], mm[3]);
      }
    }
  }

  std::printf("G5 shipping signed tracks: A/B/C fraction mismatches %d, "
              "E-stage fraction mismatches %d, bound/margin breaches %d\n",
              fraction_mismatch, e_stage_mismatch, bound_fail);
  for (int si = 0; si < 4; ++si)
    std::printf("   %-3s signed delta %+d..%+d mm%s\n", specs[si].name,
                lo[si], hi[si], (lo[si] < 0 && hi[si] > 0) ? " (both signs)" : "");
  if (fraction_mismatch != 0)
    fail("A/B/C partial helpers are not their exact constant-slope fractions");
  if (e_stage_mismatch != 0)
    fail("C-End staged helpers are not the exact constant-slope fractions");
  if (bound_fail != 0)
    fail("a shipping key/midpoint exceeds its signed bound or free-span margin");
}

std::map<int32_t, Vec3> posed_ring_centroids(
    const zc::CreatureType& type,
    const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
    const Stations& stations) {
  const auto ring_map = ring_station_map(stations);
  std::map<int32_t, RingAccum> accum;
  for (const zc::Meshlet& m : type.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (const zc::SkinVertex& v : m.verts) {
      const auto it = ring_map.find(v.y);
      if (it == ring_map.end()) continue;
      int32_t ox = 0, oy = 0, oz = 0;
      zc::skin_vertex(pose.data(), v, ox, oy, oz, nullptr);
      constexpr double kToMm = 1000.0 / 65536.0;
      RingAccum& a = accum[it->second];
      a.p.x += ox * kToMm;
      a.p.y += oy * kToMm;
      a.p.z += oz * kToMm;
      ++a.count;
    }
  }
  std::map<int32_t, Vec3> out;
  for (const auto& entry : accum) {
    if (entry.second.count <= 0) continue;
    const double n = static_cast<double>(entry.second.count);
    out[entry.first] = Vec3{entry.second.p.x / n, entry.second.p.y / n,
                            entry.second.p.z / n};
  }
  return out;
}

void check_shipping_posed_ring_order(const zc::CreatureType& type,
                                      const Stations& stations,
                                      bool fail_posed_order) {
  const auto specs = span_specs(stations);
  double worst_projection = std::numeric_limits<double>::infinity();
  double worst_separation = std::numeric_limits<double>::infinity();
  uint16_t worst_slot = 0;
  int worst_frame = 0;
  uint8_t worst_sub = 0;
  const char* worst_span = "none";
  size_t steps = 0;
  size_t reversed = 0;
  size_t pinched = 0;
  bool mutant_applied = false;

  for (const zc::Clip& shipping : type.bank.clips) {
    zc::Clip mutant;
    const zc::Clip* clip = &shipping;
    if (fail_posed_order && !mutant_applied && shipping.frame_count > 0) {
      mutant = shipping;
      mutant.quats[u02::kBSpanDeltaC] = u02::quat_z(32768);
      if (mutant.mid_quats.size() == mutant.quats.size())
        mutant.mid_quats[u02::kBSpanDeltaC] = u02::quat_z(32768);
      clip = &mutant;
      mutant_applied = true;
    }
    for (int f = 0; f < clip->frame_count; ++f) {
      for (uint8_t sub = 0; sub <= 1; ++sub) {
        if (sub == 1 && clip->mid_quats.empty()) continue;
        std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
        zc::decode_pose(type, *clip, static_cast<uint16_t>(f), pose, nullptr,
                        sub);
        const auto rings = posed_ring_centroids(type, pose, stations);
        for (const SpanSpec& spec : specs) {
          std::vector<Vec3> zone;
          for (const auto& entry : rings)
            if (entry.first >= spec.gradient_start &&
                entry.first <= spec.gradient_end)
              zone.push_back(entry.second);
          if (zone.size() < 2) {
            fail("a signed-gradient zone has fewer than two compiled rings");
            continue;
          }
          const Vec3 chord = zone.back() - zone.front();
          const double chord_len = length(chord);
          if (chord_len <= 1e-6) {
            ++reversed;
            continue;
          }
          for (size_t i = 1; i < zone.size(); ++i) {
            const Vec3 step = zone[i] - zone[i - 1];
            const double separation = length(step);
            const double projection = dot(step, chord) / chord_len;
            ++steps;
            if (projection < worst_projection ||
                (projection == worst_projection &&
                 separation < worst_separation)) {
              worst_projection = projection;
              worst_separation = separation;
              worst_slot = clip->slot_id;
              worst_frame = f;
              worst_sub = sub;
              worst_span = spec.name;
            }
            if (!(projection > 0.25)) ++reversed;
            if (!(separation > 0.50)) ++pinched;
          }
        }
      }
    }
  }

  std::printf("G6 shipping posed ring order: %zu steps, min projection %.3f mm, "
              "separation %.3f mm at slot %u key %d sub %u %s; "
              "%zu reversed, %zu pinched\n",
              steps, worst_projection, worst_separation, worst_slot,
              worst_frame, worst_sub, worst_span, reversed, pinched);
  if (fail_posed_order && !mutant_applied)
    fail("posed-order mutant did not reach a shipping clip");
  if (reversed != 0)
    fail("a shipping key/midpoint reverses a signed-gradient ring step");
  if (pinched != 0)
    fail("a shipping key/midpoint collapses neighbouring signed-gradient rings");
}

void check_rear_closure(const zc::CreatureType& type,
                        const Stations& stations) {
  const int32_t x = u02::fxu(u02::kLoopTubeXMm);
  const int32_t cy = u02::fxu(stations.y0 + stations.st_c);
  const int32_t ey = u02::fxu(stations.y0 + stations.st_e);
  const int32_t ty = u02::fxu(stations.y0 + stations.total);
  double worst_endpoint = 0.0;
  double worst_tip_length = 0.0;
  double worst_sine = 0.0;
  int bad_direction = 0;
  size_t samples = 0;

  for (const zc::Clip& c : type.bank.clips) {
    for (int f = 0; f < c.frame_count; ++f) {
      for (uint8_t sub = 0; sub <= 1; ++sub) {
        if (sub == 1 && c.mid_quats.empty()) continue;
        std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
        zc::decode_pose(type, c, static_cast<uint16_t>(f), pose, nullptr, sub);
        const Vec3 pc = posed_point(pose, u02::kBHingeD, x, cy);
        const Vec3 pe_helper = posed_point(pose, u02::kBSpanDeltaE, x, ey);
        const Vec3 pe_socket = posed_point(pose, u02::kBRearSocket, x, ey);
        const Vec3 pt = posed_point(pose, u02::kBReturnTip, x, ty);
        const Vec3 ce = pe_socket - pc;
        const Vec3 et = pt - pe_socket;
        const double lce = length(ce);
        const double let = length(et);
        worst_endpoint = std::max(worst_endpoint,
                                  length(pe_helper - pe_socket));
        worst_tip_length = std::max(
            worst_tip_length,
            std::abs(let - static_cast<double>(u02::kRearSocketBurialMm)));
        if (lce > 1e-6 && let > 1e-6) {
          worst_sine = std::max(worst_sine,
                                cross_length(ce, et) / (lce * let));
          if (dot(ce, et) <= 0.0) ++bad_direction;
        } else {
          ++bad_direction;
        }
        ++samples;
      }
    }
  }

  std::printf("G7 rear key/midpoint closure: %zu samples, endpoint %.3f mm, "
              "tip-length error %.3f mm, straightness sin %.6f\n",
              samples, worst_endpoint, worst_tip_length, worst_sine);
  if (worst_endpoint > kRearEndpointToleranceMm)
    fail("SpanDeltaE and body-attached RearSocket do not meet at End");
  if (worst_tip_length > 2.1)
    fail("ReturnTip no longer stays the declared burial distance past End");
  if (worst_sine > 0.012 || bad_direction != 0)
    fail("C-End-ReturnTip is not one forward straight closure at a key/midpoint");
}

void usage(const char* argv0) {
  std::fprintf(stderr,
               "usage: %s [--csv] [--fail-rigid-span F-A|A-B|B-C|C-E] "
               "[--fail-clamp-negative F-A|A-B|B-C|C-E] "
               "[--fail-delta-drift A|B|C|E] "
               "[--fail-overcompact F-A|A-B|B-C|C-E] "
               "[--fail-e-start] [--fail-e-mid] [--fail-e-presocket] "
               "[--fail-posed-order]\n",
               argv0);
}

}  // namespace

int main(int argc, char** argv) {
  bool csv = false;
  Span rigid_span = Span::kNone;
  Span clamp_span = Span::kNone;
  Span drift_span = Span::kNone;
  Span overcompact_span = Span::kNone;
  bool fail_e_start = false;
  bool fail_e_mid = false;
  bool fail_e_presocket = false;
  bool fail_posed_order = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--csv") == 0) {
      csv = true;
    } else if (std::strcmp(argv[i], "--fail-rigid-span") == 0 && i + 1 < argc) {
      rigid_span = parse_span(argv[++i]);
      if (rigid_span == Span::kNone) { usage(argv[0]); return 2; }
    } else if (std::strcmp(argv[i], "--fail-clamp-negative") == 0 && i + 1 < argc) {
      clamp_span = parse_span(argv[++i]);
      if (clamp_span == Span::kNone) { usage(argv[0]); return 2; }
    } else if (std::strcmp(argv[i], "--fail-delta-drift") == 0 && i + 1 < argc) {
      drift_span = parse_drift(argv[++i]);
      if (drift_span == Span::kNone) { usage(argv[0]); return 2; }
    } else if (std::strcmp(argv[i], "--fail-overcompact") == 0 && i + 1 < argc) {
      overcompact_span = parse_span(argv[++i]);
      if (overcompact_span == Span::kNone) { usage(argv[0]); return 2; }
    } else if (std::strcmp(argv[i], "--fail-e-start") == 0) {
      fail_e_start = true;
    } else if (std::strcmp(argv[i], "--fail-e-mid") == 0) {
      fail_e_mid = true;
    } else if (std::strcmp(argv[i], "--fail-e-presocket") == 0) {
      fail_e_presocket = true;
    } else if (std::strcmp(argv[i], "--fail-posed-order") == 0) {
      fail_posed_order = true;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  const Stations stations;
  zc::CreatureType type = u02::type();
  mutate_rigid_span(type, rigid_span, stations);

  std::printf("MANAFOLD PASS-17 SIGNED-SPAN GATE\n");
  std::printf("  bones %u/%d; helpers A/B/C/E/EStart/EMid/EPre = "
              "%u/%u/%u/%u/%u/%u/%u\n",
              type.skeleton.bone_count, zc::kMaxBones,
              u02::kBSpanDeltaA, u02::kBSpanDeltaB,
              u02::kBSpanDeltaC, u02::kBSpanDeltaE,
              u02::kBSpanDeltaEStart, u02::kBSpanDeltaEMid,
              u02::kBSpanDeltaEPreSocket);
  std::printf("  signed bounds pm F-A %d..%d, A-B %d..%d, "
              "B-C %d..%d, C-E %d..%d; minimum signed run %d mm\n",
              u02::kSpanCompactionMinPm[0], u02::kSpanStretchMaxPm[0],
              u02::kSpanCompactionMinPm[1], u02::kSpanStretchMaxPm[1],
              u02::kSpanCompactionMinPm[2], u02::kSpanStretchMaxPm[2],
              u02::kSpanCompactionMinPm[3], u02::kSpanStretchMaxPm[3],
              u02::kSpanMinRunMm);
  if (rigid_span != Span::kNone)
    std::printf("  [MUTANT] rigid free span %s\n", span_name(rigid_span));
  if (clamp_span != Span::kNone)
    std::printf("  [MUTANT] clamp negative %s\n", span_name(clamp_span));
  if (drift_span != Span::kNone)
    std::printf("  [MUTANT] delta drift at %s endpoint\n", span_name(drift_span));
  if (overcompact_span != Span::kNone)
    std::printf("  [MUTANT] overcompact %s\n", span_name(overcompact_span));
  if (fail_e_start)
    std::printf("  [MUTANT] omit C-End start-helper fraction\n");
  if (fail_e_mid)
    std::printf("  [MUTANT] omit C-End midpoint-helper fraction\n");
  if (fail_e_presocket)
    std::printf("  [MUTANT] omit C-End pre-socket-helper fraction\n");
  if (fail_posed_order)
    std::printf("  [MUTANT] rotate one shipping partial helper 180 degrees\n");
  std::printf("\n");

  if (type.skeleton.bone_count != u02::kBoneCount)
    fail("compiled skeleton does not carry all Manafold bones");
  if (u02::kBoneCount > zc::kMaxBones)
    fail("Manafold exceeds the 32-bone donor ceiling");

  check_compiled_zones(type, stations);
  check_lane_samples(type);
  check_identity_palettes(type);
  check_synthetic_signs(type, stations, clamp_span, drift_span,
                        overcompact_span, fail_e_start, fail_e_mid,
                        fail_e_presocket);
  check_shipping_tracks(type, stations, csv);
  check_shipping_posed_ring_order(type, stations, fail_posed_order);
  check_rear_closure(type, stations);

  std::printf("\n%s: %d failure(s)\n",
              g_failures == 0 ? "PASS" : "FAIL", g_failures);
  return g_failures == 0 ? 0 : 1;
}
