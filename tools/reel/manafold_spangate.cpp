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
//   --fail-order
//   --fail-mute F|A|B|C|E
//   --csv  print shipping signed-delta and public ordering traces

#include <algorithm>
#include <array>
#include <charconv>
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
#include "manafold_public_joint_metric.h"

namespace {
namespace pjm = u02::public_joint_metric;

int g_failures = 0;
enum GateCategory : uint32_t {
  kCatConfig = 1u << 0,
  kCatZones = 1u << 1,
  kCatLanes = 1u << 2,
  kCatIdentity = 1u << 3,
  kCatSynthetic = 1u << 4,
  kCatShipping = 1u << 5,
  kCatPosedOrder = 1u << 6,
  kCatCrown = 1u << 7,
  kCatContinuity = 1u << 8,
  kCatClosure = 1u << 9,
  kCatAttribution = 1u << 10,
};
uint32_t g_failure_bits = 0;
uint32_t g_current_category = kCatConfig;

// Closure uses quantized angle16 aim and integer-millimetre distance before the
// 90 mm delta-to-socket bend zone. This is a structural coincidence tolerance,
// not an art value; the observed shipping worst remains printed beside it.
constexpr double kRearEndpointToleranceMm = 10.0;

void fail(const char* what) {
  std::printf("  FAIL: %s\n", what);
  ++g_failures;
  g_failure_bits |= g_current_category;
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

bool parse_strict_int(const char* name, const char* text,
                      int min_value, int max_value, int& out) {
  if (text == nullptr || *text == '\0') {
    std::fprintf(stderr, "%s is empty; expected integer %d..%d\n",
                 name, min_value, max_value);
    return false;
  }
  const char* first = text;
  const char* last = text + std::strlen(text);
  if (*first == '+') {
    ++first;
    if (first == last) {
      std::fprintf(stderr, "%s=%s is not an integer; expected %d..%d\n",
                   name, text, min_value, max_value);
      return false;
    }
  }
  int value = 0;
  const std::from_chars_result parsed = std::from_chars(first, last, value);
  if (parsed.ec != std::errc{} || parsed.ptr != last) {
    std::fprintf(stderr, "%s=%s is not an integer; expected %d..%d\n",
                 name, text, min_value, max_value);
    return false;
  }
  if (value < min_value || value > max_value) {
    std::fprintf(stderr, "%s=%s outside %d..%d\n",
                 name, text, min_value, max_value);
    return false;
  }
  out = value;
  return true;
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

u02::PublicJointMute parse_public_mute(const char* s) {
  if (s == nullptr || s[0] == 0 || s[1] != 0)
    return u02::PublicJointMute::kNone;
  switch (s[0]) {
    case 'F': return u02::PublicJointMute::kFront;
    case 'A': return u02::PublicJointMute::kA;
    case 'B': return u02::PublicJointMute::kB;
    case 'C': return u02::PublicJointMute::kC;
    case 'E': return u02::PublicJointMute::kEnd;
    default: return u02::PublicJointMute::kNone;
  }
}

const char* public_mute_name(u02::PublicJointMute mute) {
  switch (mute) {
    case u02::PublicJointMute::kFront: return "F";
    case u02::PublicJointMute::kA: return "A";
    case u02::PublicJointMute::kB: return "B";
    case u02::PublicJointMute::kC: return "C";
    case u02::PublicJointMute::kEnd: return "E";
    case u02::PublicJointMute::kNone: return "none";
  }
  return "none";
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

const zc::Clip* clip_by_slot(const zc::CreatureType& type, uint16_t slot) {
  for (const zc::Clip& c : type.bank.clips)
    if (c.slot_id == slot) return &c;
  return nullptr;
}

Vec3 root_local(const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
                int32_t wx, int32_t wy, int32_t wz) {
  const zc::mat3x4fx& root = pose[u02::kBRoot];
  const int64_t dx = wx - root.m[3], dy = wy - root.m[7], dz = wz - root.m[11];
  constexpr double kToMm = 1000.0 / 65536.0;
  return Vec3{
      static_cast<double>((root.m[0] * dx + root.m[4] * dy + root.m[8] * dz) >> 16) * kToMm,
      static_cast<double>((root.m[1] * dx + root.m[5] * dy + root.m[9] * dz) >> 16) * kToMm,
      static_cast<double>((root.m[2] * dx + root.m[6] * dy + root.m[10] * dz) >> 16) * kToMm,
  };
}

Vec3 visible_core_centroid(
    const zc::CreatureType& type,
    const std::array<zc::mat3x4fx, zc::kMaxBones>& pose, uint8_t bone) {
  Vec3 sum{};
  size_t count = 0;
  for (const zc::Meshlet& m : type.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (const zc::SkinVertex& v : m.verts) {
      const bool rigid = (v.b0 == bone && v.w0 == 64) ||
                         (v.b1 == bone && v.w0 == 0);
      if (!rigid) continue;
      int32_t x = 0, y = 0, z = 0;
      zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
      const Vec3 p = root_local(pose, x, y, z);
      sum.x += p.x;
      sum.y += p.y;
      sum.z += p.z;
      ++count;
    }
  }
  if (count == 0) return sum;
  const double n = static_cast<double>(count);
  return Vec3{sum.x / n, sum.y / n, sum.z / n};
}

struct OrderSample {
  double y[3] = {0.0, 0.0, 0.0};
  int32_t span_mm[4] = {0, 0, 0, 0};
};

OrderSample public_order_sample(const zc::CreatureType& type,
                                const zc::Clip& clip, int presentation_frame) {
  const int key = presentation_frame / 2;
  const uint8_t sub = static_cast<uint8_t>(presentation_frame & 1);
  std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
  zc::decode_pose(type, clip, static_cast<uint16_t>(key), pose, nullptr, sub);
  const uint8_t bone[3] = {u02::kBHingeA, u02::kBHingeB, u02::kBHingeC};
  OrderSample out;
  for (int i = 0; i < 3; ++i)
    out.y[i] = visible_core_centroid(type, pose, bone[i]).y;
  const uint8_t receipt[4] = {u02::kBHingeA, u02::kBHingeB,
                              u02::kBHingeC, u02::kBSpanDeltaE};
  for (int i = 0; i < 4; ++i) {
    const int32_t fx = sample_local_y(clip, key, receipt[i], sub);
    out.span_mm[i] = static_cast<int32_t>((static_cast<int64_t>(fx) * 1000) >> 16);
  }
  return out;
}

int order_crossings(const std::vector<OrderSample>& samples, int a, int b,
                    double deadband_mm) {
  int previous = 0;
  int crossings = 0;
  for (const OrderSample& s : samples) {
    const double d = s.y[a] - s.y[b];
    const int sign = d > deadband_mm ? 1 : (d < -deadband_mm ? -1 : 0);
    if (sign == 0) continue;
    if (previous != 0 && sign != previous) ++crossings;
    previous = sign;
  }
  return crossings;
}

bool quat_equal(const zc::quat16& a, const zc::quat16& b) {
  return std::memcmp(&a, &b, sizeof(a)) == 0;
}

void check_public_ordering(const zc::CreatureType& type, bool csv) {
  const zc::Clip* clip = clip_by_slot(type, u02::kTaunt3Slot);
  if (clip == nullptr || clip->frame_count != u02::kTaunt3Keys) {
    fail("shipping Taunt III is absent from the compiled bank");
    return;
  }
  constexpr int kBegin = 112;
  constexpr int kEnd = 288;
  constexpr int kWitness[4] = {142, 178, 212, 268};
  constexpr int kTop[4] = {0, 1, 2, 0};
  constexpr int kBottom[4] = {2, 0, 1, 2};
  const double margin = static_cast<double>(u02::kTaunt3OrderReadMarginMm);

  std::vector<OrderSample> samples;
  samples.reserve(kEnd - kBegin + 1);
  int witness_fail = 0;
  int sign_fail = 0;
  int32_t span_lo[4] = {INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX};
  int32_t span_hi[4] = {INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN};
  if (csv)
    std::printf("\nframe,key,sub,A_y,B_y,C_y,AB_y,AC_y,BC_y,FA_mm,AB_mm,BC_mm,CE_mm\n");
  for (int pf = kBegin; pf <= kEnd; ++pf) {
    const OrderSample s = public_order_sample(type, *clip, pf);
    samples.push_back(s);
    for (int i = 0; i < 4; ++i) {
      span_lo[i] = std::min(span_lo[i], s.span_mm[i]);
      span_hi[i] = std::max(span_hi[i], s.span_mm[i]);
    }
    if (csv)
      std::printf("%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d\n",
                  pf, pf / 2, pf & 1, s.y[0], s.y[1], s.y[2],
                  s.y[0] - s.y[1], s.y[0] - s.y[2], s.y[1] - s.y[2],
                  s.span_mm[0], s.span_mm[1], s.span_mm[2], s.span_mm[3]);
  }
  for (int w = 0; w < 4; ++w) {
    const OrderSample s = public_order_sample(type, *clip, kWitness[w]);
    const int top = kTop[w], bottom = kBottom[w];
    for (int i = 0; i < 3; ++i) {
      if (i != top && !(s.y[top] - s.y[i] >= margin)) ++witness_fail;
      if (i != bottom && !(s.y[i] - s.y[bottom] >= margin)) ++witness_fail;
    }
    std::printf("G8 tableau f%04d: A/B/C %.1f/%.1f/%.1f mm, top %c bottom %c\n",
                kWitness[w], s.y[0], s.y[1], s.y[2], "ABC"[top],
                "ABC"[bottom]);
  }
  const int crossings[3] = {
      order_crossings(samples, 0, 1, margin),
      order_crossings(samples, 0, 2, margin),
      order_crossings(samples, 1, 2, margin),
  };
  for (int i = 0; i < 3; ++i)
    if (crossings[i] < 2) ++sign_fail;
  for (int i = 0; i < 4; ++i)
    if (!(span_lo[i] <= -u02::kTaunt3OrderSpanReadMm &&
          span_hi[i] >= u02::kTaunt3OrderSpanReadMm))
      ++sign_fail;

  // Attribute Front/End order punctuation against a direct no-order build. The
  // body/root channels may differ; only the local endpoint quaternions are read.
  const int32_t saved_gain = u02::g_u02_order_gain_pm;
  const u02::PublicJointMute saved_mute = u02::g_u02_public_joint_mute;
  u02::g_u02_order_gain_pm = 0;
  u02::g_u02_public_joint_mute = u02::PublicJointMute::kNone;
  const zc::Clip no_order = u02::build_taunt3();
  u02::g_u02_order_gain_pm = saved_gain;
  u02::g_u02_public_joint_mute = saved_mute;
  int endpoint_live[2] = {0, 0};
  const uint8_t endpoint_bone[2] = {u02::kBJunctionF, u02::kBRearSocket};
  for (int key = u02::kTaunt3ShimmyKey; key < u02::kTaunt3FlickKey; ++key)
    for (int i = 0; i < 2; ++i) {
      const zc::quat16& a = clip->quats[static_cast<size_t>(key) * u02::kBoneCount + endpoint_bone[i]];
      const zc::quat16& b = no_order.quats[static_cast<size_t>(key) * u02::kBoneCount + endpoint_bone[i]];
      if (!quat_equal(a, b)) ++endpoint_live[i];
    }

  std::printf("G8 public crown shuffle: witness failures %d, crossings AB/AC/BC %d/%d/%d, "
              "endpoint live F/E %d/%d\n",
              witness_fail, crossings[0], crossings[1], crossings[2],
              endpoint_live[0], endpoint_live[1]);
  std::printf("   phrase span deltas F-A %+d..%+d, A-B %+d..%+d, "
              "B-C %+d..%+d, C-E %+d..%+d mm\n",
              span_lo[0], span_hi[0], span_lo[1], span_hi[1],
              span_lo[2], span_hi[2], span_lo[3], span_hi[3]);
  if (witness_fail != 0)
    fail("a named crown-shuffle tableau lacks its visible top/bottom margin");
  if (sign_fail != 0)
    fail("the crown shuffle lacks pair crossings or signed span witnesses");
  if (endpoint_live[0] == 0 || endpoint_live[1] == 0)
    fail("Front or End lacks independent crown-shuffle punctuation");
}

zc::Clip build_taunt3_for_mute(u02::PublicJointMute mute) {
  const u02::PublicJointMute saved = u02::g_u02_public_joint_mute;
  u02::g_u02_public_joint_mute = mute;
  zc::Clip clip = u02::build_taunt3();
  u02::finalize_rear_follow(clip);
  zc::bake_presentation_midpoints(clip, u02::kBoneCount);
  u02::finalize_rear_follow_midpoints(clip);
  u02::g_u02_public_joint_mute = saved;
  return clip;
}

void check_held_punchline(const zc::CreatureType& type,
                          u02::PublicJointMute selected_mute) {
  constexpr int kWitness = 324;
  constexpr uint16_t kKey = static_cast<uint16_t>(kWitness / 2);
  static constexpr u02::PublicJointMute kMute[5] = {
      u02::PublicJointMute::kFront, u02::PublicJointMute::kA,
      u02::PublicJointMute::kB, u02::PublicJointMute::kC,
      u02::PublicJointMute::kEnd};
  const auto carriers = pjm::carriers();

  const zc::Clip normal = build_taunt3_for_mute(u02::PublicJointMute::kNone);
  std::array<pjm::CoreDelta, 5> mute_delta{};
  int read_failures = 0;
  for (int i = 0; i < 5; ++i) {
    int32_t input[5] = {11, 22, 33, 44, 55};
    const int32_t expected[5] = {11, 22, 33, 44, 55};
    const u02::PublicJointMute saved = u02::g_u02_public_joint_mute;
    u02::g_u02_public_joint_mute = kMute[i];
    u02::apply_public_joint_mute(input);
    u02::g_u02_public_joint_mute = saved;
    for (int j = 0; j < 5; ++j) {
      const int32_t want = j == i ? 0 : expected[j];
      if (input[j] != want)
        fail("a held-punchline mute changed an unrelated carrier input");
    }

    const zc::Clip muted = build_taunt3_for_mute(kMute[i]);
    const pjm::CoreDelta d =
        pjm::core_delta(type, normal, muted, kKey, carriers[i]);
    mute_delta[static_cast<size_t>(i)] = d;
    const bool readable = pjm::clears_public_floor(d, carriers[i]);
    std::printf("G8 held punchline f%04d mute %s core %zu centroid/max "
                "%.2f/%.2f mm (%s metric) %s\n",
                kWitness, carriers[i].name, d.count, d.centroid_mm,
                d.max_vertex_mm, carriers[i].anchored ? "anchored-max" :
                                                      "centroid+max",
                readable ? "OK" : "FAIL");
    if (!readable) ++read_failures;
  }
  if (read_failures != 0)
    fail("a held-punchline carrier mute does not remove a publicly readable contribution");

  const zc::Clip* shipping = clip_by_slot(type, u02::kTaunt3Slot);
  if (shipping == nullptr) {
    fail("shipping Taunt III is absent from the held-punchline gate");
    return;
  }
  const int muted_index = u02::public_joint_mute_index(selected_mute);
  if (muted_index < 0) {
    double worst = 0.0;
    for (int i = 0; i < 5; ++i) {
      const pjm::CoreDelta d =
          pjm::core_delta(type, normal, *shipping, kKey, carriers[i]);
      worst = std::max(worst, std::max(d.centroid_mm, d.max_vertex_mm));
    }
    std::printf("   shipping-vs-normal held-punchline worst %.3f mm\n", worst);
    if (worst > 0.6)
      fail("shipping held-punchline carrier pose differs from the normal production build");
  } else {
    const pjm::CoreDelta& d = mute_delta[static_cast<size_t>(muted_index)];
    const bool removed = pjm::clears_public_floor(d, carriers[muted_index]);
    std::printf("   selected held mute %s removes centroid/max %.2f/%.2f mm\n",
                carriers[muted_index].name, d.centroid_mm, d.max_vertex_mm);
    if (!removed)
      fail("the selected held mute did not remove its named held-punchline carrier");
    else
      fail("the selected held mute removed its named held-punchline contribution");
  }
}

zc::mat3x4fx root_relative_basis(const zc::mat3x4fx& root,
                                  const zc::mat3x4fx& bone) {
  zc::mat3x4fx out{};
  constexpr int at[3][3] = {{0, 1, 2}, {4, 5, 6}, {8, 9, 10}};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      int64_t v = 0;
      for (int k = 0; k < 3; ++k)
        v += static_cast<int64_t>(root.m[at[k][i]]) * bone.m[at[k][j]];
      out.m[at[i][j]] = static_cast<int32_t>(v >> 16);
    }
  return out;
}

Vec3 rotation_step_deg(const zc::mat3x4fx& from,
                       const zc::mat3x4fx& to) {
  constexpr int at[3][3] = {{0, 1, 2}, {4, 5, 6}, {8, 9, 10}};
  double a[3][3]{}, b[3][3]{}, r[3][3]{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      a[i][j] = static_cast<double>(from.m[at[i][j]]) / 65536.0;
      b[i][j] = static_cast<double>(to.m[at[i][j]]) / 65536.0;
    }
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      for (int k = 0; k < 3; ++k) r[i][j] += a[k][i] * b[k][j];
  double cs = (r[0][0] + r[1][1] + r[2][2] - 1.0) * 0.5;
  if (cs < -1.0) cs = -1.0;
  if (cs > 1.0) cs = 1.0;
  const double angle = std::acos(cs);
  if (angle < 1e-9) return Vec3{};
  const double sn = std::sin(angle);
  if (std::fabs(sn) < 1e-8)
    return Vec3{angle * 180.0 / 3.14159265358979, 0.0, 0.0};
  const double scale = angle * 180.0 / 3.14159265358979 / (2.0 * sn);
  return Vec3{(r[2][1] - r[1][2]) * scale,
              (r[0][2] - r[2][0]) * scale,
              (r[1][0] - r[0][1]) * scale};
}

void check_carrier_continuity(const zc::CreatureType& type, bool motion_csv,
                              bool fail_final_dwell) {
  static constexpr uint8_t kBone[5] = {
      u02::kBJunctionF, u02::kBHingeA, u02::kBHingeB,
      u02::kBHingeC, u02::kBRearSocket};
  static constexpr char kName[5] = {'F', 'A', 'B', 'C', 'E'};
  double max_step = 0.0, max_accel = 0.0, max_jerk = 0.0;
  double max_ang_step = 0.0, max_ang_accel = 0.0, max_ang_jerk = 0.0;
  uint16_t step_slot = 0, accel_slot = 0, jerk_slot = 0;
  uint16_t astep_slot = 0, aaccel_slot = 0, ajerk_slot = 0;
  int step_frame = 0, accel_frame = 0, jerk_frame = 0;
  int astep_frame = 0, aaccel_frame = 0, ajerk_frame = 0;
  int step_carrier = 0, accel_carrier = 0, jerk_carrier = 0;
  int astep_carrier = 0, aaccel_carrier = 0, ajerk_carrier = 0;
  size_t samples = 0;
  size_t held_tail_samples = 0;
  bool dwell_mutant_applied = false;
  if (motion_csv)
    std::printf("\nslot,frame,carrier,x_mm,y_mm,z_mm,velocity_mm,accel_mm,jerk_mm,angular_velocity_deg,angular_accel_deg,angular_jerk_deg\n");

  for (const zc::Clip& clip : type.bank.clips) {
    const int count = static_cast<int>(clip.frame_count) * 2;
    if (count < 4) continue;
    const int sample_count = count + (clip.hold_last ? 3 : 0);
    std::vector<std::array<Vec3, 5>> p(static_cast<size_t>(sample_count));
    std::vector<std::array<zc::mat3x4fx, 5>> m(static_cast<size_t>(sample_count));
    for (int f = 0; f < sample_count; ++f) {
      const int pf = f < count ? f : count - 1;
      std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
      zc::decode_pose(type, clip, static_cast<uint16_t>(pf / 2), pose, nullptr,
                      static_cast<uint8_t>(pf & 1));
      for (int i = 0; i < 5; ++i) {
        p[static_cast<size_t>(f)][i] = visible_core_centroid(type, pose, kBone[i]);
        m[static_cast<size_t>(f)][i] =
            root_relative_basis(pose[u02::kBRoot], pose[kBone[i]]);
      }
      if (clip.hold_last && f >= count) ++held_tail_samples;
      if (fail_final_dwell && clip.hold_last && !dwell_mutant_applied &&
          f == count) {
        // Consumer-side positive control: authored final key/midpoint are
        // untouched, but the first held tick fails to clamp the visible core.
        p[static_cast<size_t>(f)][1].x += 400.0;
        dwell_mutant_applied = true;
      }
    }
    const auto index = [&](int f) {
      if (clip.hold_last)
        return f < 0 ? 0 : (f >= sample_count ? sample_count - 1 : f);
      f %= count;
      return f < 0 ? f + count : f;
    };
    const int begin = clip.hold_last ? 3 : 0;
    for (int f = begin; f < sample_count; ++f) {
      const int f0 = index(f), f1 = index(f - 1), f2 = index(f - 2), f3 = index(f - 3);
      for (int i = 0; i < 5; ++i) {
        const Vec3 v0 = p[static_cast<size_t>(f0)][i] - p[static_cast<size_t>(f1)][i];
        const Vec3 v1 = p[static_cast<size_t>(f1)][i] - p[static_cast<size_t>(f2)][i];
        const Vec3 v2 = p[static_cast<size_t>(f2)][i] - p[static_cast<size_t>(f3)][i];
        const Vec3 ac0 = v0 - v1;
        const Vec3 ac1 = v1 - v2;
        const double step = length(v0), accel = length(ac0), jerk = length(ac0 - ac1);
        if (step > max_step) { max_step = step; step_slot = clip.slot_id; step_frame = f; step_carrier = i; }
        if (accel > max_accel) { max_accel = accel; accel_slot = clip.slot_id; accel_frame = f; accel_carrier = i; }
        if (jerk > max_jerk) { max_jerk = jerk; jerk_slot = clip.slot_id; jerk_frame = f; jerk_carrier = i; }

        const Vec3 w0 = rotation_step_deg(m[static_cast<size_t>(f1)][i],
                                          m[static_cast<size_t>(f0)][i]);
        const Vec3 w1 = rotation_step_deg(m[static_cast<size_t>(f2)][i],
                                          m[static_cast<size_t>(f1)][i]);
        const Vec3 w2 = rotation_step_deg(m[static_cast<size_t>(f3)][i],
                                          m[static_cast<size_t>(f2)][i]);
        const Vec3 aa0 = w0 - w1;
        const Vec3 aa1 = w1 - w2;
        const double astep = length(w0), aaccel = length(aa0), ajerk = length(aa0 - aa1);
        if (astep > max_ang_step) { max_ang_step = astep; astep_slot = clip.slot_id; astep_frame = f; astep_carrier = i; }
        if (aaccel > max_ang_accel) { max_ang_accel = aaccel; aaccel_slot = clip.slot_id; aaccel_frame = f; aaccel_carrier = i; }
        if (ajerk > max_ang_jerk) { max_ang_jerk = ajerk; ajerk_slot = clip.slot_id; ajerk_frame = f; ajerk_carrier = i; }
        if (motion_csv) {
          const Vec3& q = p[static_cast<size_t>(f0)][i];
          std::printf("%u,%d,%c,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                      clip.slot_id, f, kName[i], q.x, q.y, q.z,
                      step, accel, jerk, astep, aaccel, ajerk);
        }
        ++samples;
      }
    }
  }

  std::printf("G9 full-bank 60Hz carrier continuity: %zu samples, %zu held-tail samples\n",
              samples, held_tail_samples);
  std::printf("   position step %.3f mm slot %u f%04d %c; accel %.3f slot %u f%04d %c; jerk %.3f slot %u f%04d %c\n",
              max_step, step_slot, step_frame, kName[step_carrier],
              max_accel, accel_slot, accel_frame, kName[accel_carrier],
              max_jerk, jerk_slot, jerk_frame, kName[jerk_carrier]);
  std::printf("   angular step %.3f deg slot %u f%04d %c; accel %.3f slot %u f%04d %c; jerk %.3f slot %u f%04d %c\n",
              max_ang_step, astep_slot, astep_frame, kName[astep_carrier],
              max_ang_accel, aaccel_slot, aaccel_frame, kName[aaccel_carrier],
              max_ang_jerk, ajerk_slot, ajerk_frame, kName[ajerk_carrier]);
  if (const zc::Clip* worst = clip_by_slot(type, step_slot)) {
    const u02::FoldPhase before = u02::fold_phase(
        step_slot, worst->frame_count, (step_frame - 1) * 8);
    const u02::FoldPhase at = u02::fold_phase(
        step_slot, worst->frame_count, step_frame * 8);
    std::printf("   worst-position fold phase: prev seg/amp/agit/morph %d/%d/%d/%d %u->%u; "
                "at %d/%d/%d/%d %u->%u\n",
                static_cast<int>(before.seg), before.amp_pm, before.agit_pm,
                before.morph_pm, before.shape_from, before.shape_to,
                static_cast<int>(at.seg), at.amp_pm, at.agit_pm, at.morph_pm,
                at.shape_from, at.shape_to);
    const int key = step_frame / 2;
    const int lag[5] = {u02::kKneadLagJfKeys, u02::kKneadLagNeckKeys,
                        u02::kKneadLagAKeys, u02::kKneadLagBKeys,
                        u02::kKneadLagCKeys};
    std::printf("   lagged key %d phases J/N/A/B/C:", key);
    for (int i = 0; i < 5; ++i) {
      const int fl = ((key - lag[i]) % worst->frame_count + worst->frame_count) %
                     worst->frame_count;
      const u02::FoldPhase p = u02::fold_phase(step_slot, worst->frame_count, fl * 16);
      std::printf(" %d/%d/%d", static_cast<int>(p.seg), p.amp_pm, p.agit_pm);
    }
    std::printf("\n   lagged key %d+1 phases:", key);
    for (int i = 0; i < 5; ++i) {
      const int fl = ((key + 1 - lag[i]) % worst->frame_count + worst->frame_count) %
                     worst->frame_count;
      const u02::FoldPhase p = u02::fold_phase(step_slot, worst->frame_count, fl * 16);
      std::printf(" %d/%d/%d", static_cast<int>(p.seg), p.amp_pm, p.agit_pm);
    }
    std::printf("\n");
  }
  if (held_tail_samples == 0)
    fail("no hold-last carrier pose was checked after the final presentation sample");
  if (fail_final_dwell && !dwell_mutant_applied)
    fail("final-dwell carrier mutant did not reach a hold-last clip");
  if (max_step > u02::kTaunt3OrderMaxCoreStepMm ||
      max_accel > u02::kTaunt3OrderMaxCoreAccelMm ||
      max_jerk > u02::kTaunt3OrderMaxCoreJerkMm)
    fail("a shipping visible carrier has a position step/acceleration/jerk discontinuity");
  if (max_ang_step > u02::kAntennaMaxAngularStepDeg ||
      max_ang_accel > u02::kAntennaMaxAngularAccelDeg ||
      max_ang_jerk > u02::kAntennaMaxAngularJerkDeg)
    fail("a shipping visible carrier has an angular step/acceleration/jerk discontinuity");
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
               "[--fail-posed-order] [--fail-order] "
               "[--fail-mute F|A|B|C|E] [--fail-antenna-snap] "
               "[--fail-accent-switch] [--fail-hold-tremor] "
               "[--fail-compress-wrap] [--fail-final-dwell] [--motion-csv] "
               "[--held-only]\n",
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
  bool fail_order = false;
  bool fail_antenna_snap = false;
  bool fail_accent_switch = false;
  bool fail_hold_tremor = false;
  bool fail_compress_wrap = false;
  bool fail_final_dwell = false;
  bool motion_csv = false;
  bool held_only = false;
  u02::PublicJointMute fail_mute = u02::PublicJointMute::kNone;

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
    } else if (std::strcmp(argv[i], "--fail-order") == 0) {
      fail_order = true;
    } else if (std::strcmp(argv[i], "--fail-antenna-snap") == 0) {
      fail_antenna_snap = true;
    } else if (std::strcmp(argv[i], "--fail-accent-switch") == 0) {
      fail_accent_switch = true;
    } else if (std::strcmp(argv[i], "--fail-hold-tremor") == 0) {
      fail_hold_tremor = true;
    } else if (std::strcmp(argv[i], "--fail-compress-wrap") == 0) {
      fail_compress_wrap = true;
    } else if (std::strcmp(argv[i], "--fail-final-dwell") == 0) {
      fail_final_dwell = true;
    } else if (std::strcmp(argv[i], "--motion-csv") == 0) {
      motion_csv = true;
    } else if (std::strcmp(argv[i], "--held-only") == 0) {
      held_only = true;
    } else if (std::strcmp(argv[i], "--fail-mute") == 0 && i + 1 < argc) {
      fail_mute = parse_public_mute(argv[++i]);
      if (fail_mute == u02::PublicJointMute::kNone) {
        usage(argv[0]);
        return 2;
      }
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  int mutant_count = 0;
  uint32_t expected_category = 0, allowed_categories = 0;
  const char* mutant_name = "none";
  const auto select_mutant = [&](bool active, const char* name,
                                 uint32_t expected, uint32_t allowed) {
    if (!active) return;
    ++mutant_count;
    mutant_name = name;
    expected_category = expected;
    allowed_categories = allowed;
  };
  select_mutant(rigid_span != Span::kNone, "rigid-span", kCatZones,
                kCatZones | kCatPosedOrder | kCatCrown);
  select_mutant(clamp_span != Span::kNone, "clamp-negative", kCatSynthetic,
                kCatSynthetic);
  select_mutant(drift_span != Span::kNone, "delta-drift", kCatSynthetic,
                kCatSynthetic);
  select_mutant(overcompact_span != Span::kNone, "overcompact", kCatSynthetic,
                kCatSynthetic);
  select_mutant(fail_e_start, "e-start", kCatSynthetic, kCatSynthetic);
  select_mutant(fail_e_mid, "e-mid", kCatSynthetic, kCatSynthetic);
  select_mutant(fail_e_presocket, "e-presocket", kCatSynthetic, kCatSynthetic);
  select_mutant(fail_posed_order, "posed-order", kCatPosedOrder, kCatPosedOrder);
  select_mutant(fail_order, "crown-order", kCatCrown, kCatCrown);
  select_mutant(fail_mute != u02::PublicJointMute::kNone, "carrier-mute",
                kCatCrown,
                held_only ? kCatCrown
                          : (kCatShipping | kCatCrown |
                             kCatContinuity | kCatClosure));
  select_mutant(fail_antenna_snap, "antenna-snap", kCatContinuity,
                kCatShipping | kCatContinuity);
  select_mutant(fail_accent_switch, "accent-switch", kCatContinuity,
                kCatContinuity);
  select_mutant(fail_hold_tremor, "hold-tremor", kCatContinuity,
                kCatShipping | kCatContinuity | kCatClosure);
  select_mutant(fail_compress_wrap, "compress-wrap", kCatContinuity,
                kCatContinuity);
  select_mutant(fail_final_dwell, "final-dwell", kCatContinuity,
                kCatContinuity);
  if (mutant_count > 1 ||
      (held_only && mutant_count == 1 &&
       fail_mute == u02::PublicJointMute::kNone)) {
    usage(argv[0]);
    return 2;
  }

  if (const char* e = std::getenv("ZHAO_U02_TAUNT3_PUNCH_A_MM")) {
    int v = 0;
    if (!parse_strict_int("ZHAO_U02_TAUNT3_PUNCH_A_MM", e, 0, 320, v))
      return 2;
    u02::g_u02_taunt3_punch_a_mm = v;
  }

  if (fail_order) u02::g_u02_order_gain_pm = 0;
  if (fail_antenna_snap) u02::g_u02_order_snap_control = true;
  if (fail_accent_switch) u02::g_u02_accent_switch_control = true;
  if (fail_hold_tremor) u02::g_u02_hold_tremor_control = true;
  if (fail_compress_wrap) u02::g_u02_compress_wrap_control = true;
  if (fail_mute != u02::PublicJointMute::kNone && !held_only)
    u02::g_u02_public_joint_mute = fail_mute;
  const Stations stations;
  zc::CreatureType type = u02::type();
  mutate_rigid_span(type, rigid_span, stations);

  std::printf("MANAFOLD PASS-17 SIGNED-SPAN GATE%s\n",
              held_only ? " [HELD-PUNCHLINE FOCUS]" : "");
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
  std::printf("  Taunt III held A punctuation %d mm\n",
              u02::g_u02_taunt3_punch_a_mm);
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
  if (fail_order)
    std::printf("  [MUTANT] zero the production crown-shuffle gain\n");
  if (fail_mute != u02::PublicJointMute::kNone)
    std::printf("  [MUTANT] mute crown-shuffle carrier %s\n",
                public_mute_name(fail_mute));
  if (fail_antenna_snap)
    std::printf("  [MUTANT] restore instantaneous crown tableau replacement\n");
  if (fail_accent_switch)
    std::printf("  [MUTANT] restore per-cycle hashed accent/lead switch\n");
  if (fail_hold_tremor)
    std::printf("  [MUTANT] restore hard-gated hold tremor\n");
  if (fail_compress_wrap)
    std::printf("  [MUTANT] restore raw u16 impact-deform wrap\n");
  if (fail_final_dwell)
    std::printf("  [MUTANT] fail to clamp the first held carrier tick\n");
  std::printf("\n");

  g_current_category = kCatConfig;
  if (type.skeleton.bone_count != u02::kBoneCount)
    fail("compiled skeleton does not carry all Manafold bones");
  if (u02::kBoneCount > zc::kMaxBones)
    fail("Manafold exceeds the 32-bone donor ceiling");

  if (held_only) {
    g_current_category = kCatCrown;
    check_public_ordering(type, csv);
    check_held_punchline(type, fail_mute);
    g_current_category = kCatPosedOrder;
    check_shipping_posed_ring_order(type, stations, false);
    g_current_category = kCatContinuity;
    check_carrier_continuity(type, motion_csv, false);
    g_current_category = kCatClosure;
    check_rear_closure(type, stations);
  } else {
    g_current_category = kCatZones;
    check_compiled_zones(type, stations);
    g_current_category = kCatLanes;
    check_lane_samples(type);
    g_current_category = kCatIdentity;
    check_identity_palettes(type);
    g_current_category = kCatSynthetic;
    check_synthetic_signs(type, stations, clamp_span, drift_span,
                          overcompact_span, fail_e_start, fail_e_mid,
                          fail_e_presocket);
    g_current_category = kCatShipping;
    check_shipping_tracks(type, stations, csv);
    g_current_category = kCatPosedOrder;
    check_shipping_posed_ring_order(type, stations, fail_posed_order);
    g_current_category = kCatCrown;
    check_public_ordering(type, csv);
    check_held_punchline(type, fail_mute);
    g_current_category = kCatContinuity;
    check_carrier_continuity(type, motion_csv, fail_final_dwell);
    g_current_category = kCatClosure;
    check_rear_closure(type, stations);
  }

  if (mutant_count == 1) {
    const uint32_t observed = g_failure_bits;
    const bool named = (observed & expected_category) != 0;
    const uint32_t unrelated = observed & ~allowed_categories;
    if (!named || unrelated != 0) {
      g_current_category = kCatAttribution;
      fail("the selected mutant did not fail only its named/causal detector");
      std::printf("MUTANT %s: UNATTRIBUTED expected=0x%X observed=0x%X allowed=0x%X\n",
                  mutant_name, expected_category, observed, allowed_categories);
    } else {
      std::printf("MUTANT %s: attributed detector fired (0x%X)\n",
                  mutant_name, observed);
    }
  }

  std::printf("\n%s: %d failure(s)\n",
              g_failures == 0 ? "PASS" : "FAIL", g_failures);
  return g_failures == 0 ? 0 : 1;
}
