// manafold_rear_audit.cpp -- the COMMITTED rear-chain audit for creature 02
// (Manafold pass 19, Owner Direction 20 items 1+2: "the antennae connection to
// the back ball looks like it's almost ripped off" and "the last ball and the
// last antennae part at the back are too animated ... like a bone too much").
//
// It reads the POSED SKELETON and the COMPILED SKIN through zc::decode_pose and
// zc::skin_vertex, never a rendered frame (CLAUDE.md: measure the thing, not a
// projection of it). It is a DIAGNOSTIC instrument used to FIND the cause; it
// chooses no art value.
//
// Per presentation sample (every key and its baked midpoint, i.e. 60 Hz) it
// reports, root-local:
//
//   rel_deg     the full rotation angle between the return arm's world frame
//               (HingeD) and the End carrier's (RearSocket). This is exactly
//               the rotation finalize_rear_follow hands to the three staged
//               rear helpers (1/3, 2/3, 1) across two ring gaps in front of the
//               End swell. A large or fast-changing value is a kink there.
//   axis_deg    the angle between the two bones' tube (+Y) axes: the visible
//               part of rel_deg (a twist about the tube is not a bend).
//   bend_deg    the largest turning angle of the posed tube CENTRELINE (ring
//               centroids) inside the rear window [kRearAuditFirstRing, 62],
//               and bend_ring, where it happens. The front window's largest
//               turn is printed alongside as the like-for-like reference.
//   sock_deg    the End carrier's local rotation away from its Root frame
//               (the sum of every authority composed onto kBRearSocket).
//   arm_deg     the arm's root-local direction change since sample 0.
//   end/last/c  root-local positions of the End-swell centroid ring, the
//               last free segment's centroid ring and carrier C's ring, used
//               for the speed / acceleration / jerk energy summary.
//
// Usage: manafold-rear-audit.exe [slot ...] [--csv] [--rings <key>]
//        manafold-rear-audit.exe --gate [--fail-rear-frame | --fail-rear-joint |
//                                        --fail-line-scale]
//   default slots: 0 (hover/inspect), 5 (rest), 21 (taunt III), 2 (channel),
//                  1 (drift), 13 (trick)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

struct V3 {
  double x = 0, y = 0, z = 0;
};
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double len(V3 a) { return std::sqrt(dot(a, a)); }
double ang_deg(V3 a, V3 b) {
  const double la = len(a), lb = len(b);
  if (la < 1e-9 || lb < 1e-9) return 0.0;
  double c = dot(a, b) / (la * lb);
  c = std::max(-1.0, std::min(1.0, c));
  return std::acos(c) * 180.0 / 3.14159265358979;
}

constexpr double kFx = 1.0 / 65536.0;

// Rotation columns of a pose matrix (column j = m[j], m[4+j], m[8+j]).
V3 col(const zc::mat3x4fx& m, int j) {
  return {m.m[j] * kFx, m.m[4 + j] * kFx, m.m[8 + j] * kFx};
}

// Express a world vector in the root's frame (transpose of a rotation).
V3 to_root_dir(const zc::mat3x4fx& r, V3 w) {
  return {dot(col(r, 0), w), dot(col(r, 1), w), dot(col(r, 2), w)};
}
V3 to_root_pt(const zc::mat3x4fx& r, V3 w) {
  const V3 t{r.m[3] * kFx * 1000.0, r.m[7] * kFx * 1000.0,
             r.m[11] * kFx * 1000.0};
  return to_root_dir(r, w - t);
}

// Angle of the relative rotation Ra^T Rb, from the trace.
double rel_angle_deg(const zc::mat3x4fx& a, const zc::mat3x4fx& b) {
  double tr = 0;
  for (int j = 0; j < 3; ++j) {
    V3 ca = col(a, j), cb = col(b, j);
    const double la = len(ca), lb = len(cb);
    if (la > 1e-9) ca = ca * (1.0 / la);
    if (lb > 1e-9) cb = cb * (1.0 / lb);
    tr += dot(ca, cb);
  }
  double c = (tr - 1.0) / 2.0;
  c = std::max(-1.0, std::min(1.0, c));
  return std::acos(c) * 180.0 / 3.14159265358979;
}

// PASS 19 REVIEW: the relative rotation Ra^T Rb as a row-major 3x3, with the
// columns normalized exactly as rel_angle_deg does.
struct M3 {
  double m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
};
M3 rel_matrix(const zc::mat3x4fx& a, const zc::mat3x4fx& b) {
  V3 ca[3], cb[3];
  for (int j = 0; j < 3; ++j) {
    ca[j] = col(a, j);
    cb[j] = col(b, j);
    const double la = len(ca[j]), lb = len(cb[j]);
    if (la > 1e-9) ca[j] = ca[j] * (1.0 / la);
    if (lb > 1e-9) cb[j] = cb[j] * (1.0 / lb);
  }
  M3 r;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) r.m[3 * i + j] = dot(ca[i], cb[j]);
  return r;
}
// The angle of P^T Q: how far the joint itself turned between two samples,
// whatever the axis. (|angle(Q)| - |angle(P)| is only a lower bound on this and
// reads ZERO for a joint sweeping round at constant bend -- e.g. the End's tilt
// and yaw oscillators in quadrature.)
double step_angle_deg(const M3& p, const M3& q) {
  double tr = 0;
  for (int k = 0; k < 3; ++k)
    for (int i = 0; i < 3; ++i) tr += p.m[3 * i + k] * q.m[3 * i + k];
  double c = (tr - 1.0) / 2.0;
  c = std::max(-1.0, std::min(1.0, c));
  return std::acos(c) * 180.0 / 3.14159265358979;
}

bool is_loop_bone(uint8_t b) {
  return (b >= u02::kBJunctionF && b <= u02::kBHingeD) ||
         b == u02::kBRearSocket || b == u02::kBReturnTip ||
         (b >= u02::kBSpanDeltaA && b <= u02::kBRearRootTurnMid);
}

bool is_loop_meshlet(const zc::Meshlet& m) {
  for (const zc::SkinVertex& v : m.verts)
    if (is_loop_bone(v.b0) || is_loop_bone(v.b1)) return true;
  return false;
}

// ring index by bind y (stretched units), exactly as the model samples it.
std::map<int32_t, int> ring_map() {
  std::map<int32_t, int> out;
  const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  for (int i = 0; i < u02::kLoopRings; ++i) {
    const int32_t s = static_cast<int32_t>(
        (static_cast<int64_t>(u02::kLoopTotalMm) * i) / (u02::kLoopRings - 1));
    out[u02::fxu(y0 + s)] = i;
  }
  return out;
}

int ring_of_station(int32_t s) {
  return static_cast<int>((static_cast<int64_t>(s) * (u02::kLoopRings - 1) +
                           u02::kLoopTotalMm / 2) /
                          u02::kLoopTotalMm);
}

// ---- PASS 20 (Owner Direction 21 item 1, as corrected): THE SKIN'S STRAIN --
//
// Owner, 2026-09-20: "the rear doesn't leave the body, but it RIPS A BIG PIECE
// OUT and it STRETCHES TOO MUCH, which leads me to conclude there's too much
// motion in the back nodule."
//
// ⚠ EVERY METRIC ABOVE THIS LINE IS BLIND TO THAT, AND STRUCTURALLY SO. rel,
// axis and sock are bone ROTATIONS; bend, end/last/c and the whole motion
// summary are built from `cen[]`, a ring's vertices AVERAGED INTO ONE CENTROID
// before anything is measured. A ring dragged into an ellipse has the same
// centroid. Two rings pulled apart have the same centroid each. So the one
// quantity the owner is describing -- the skin being stretched -- is cancelled
// by the averaging step, on purpose, three lines before any ceiling sees it.
// That is why pass 19 shipped 128/128 green with this visible in Inspect: no
// gate in the matrix measured a posed SURFACE at all. mspan measures station
// bookkeeping, mprobe clearance and burial, mmeshcheck the BIND mesh, mjointpub
// a 20 mm End floor (which asks for MORE motion), msmooth effect identities.
//
// Strain is the thing itself. Per posed sample, for every (ring, segment) of
// the loop skin, against the same edge in BIND space:
//   rail  the longitudinal edge, ring i segment k -> ring i+1 segment k. This
//         is what a long rigid run swinging on the socket stretches.
//   hoop  the circumferential edge, segment k -> k+1 inside one ring. This is
//         what an ellipsed ring shows.
// The front window's worst rail is printed beside the rear's as the LIKE-FOR-
// LIKE reference (the authored kRadial loop stretch lengthens the whole chain
// by a few per cent and is not a rear fault), exactly as bend_deg already does.
constexpr int kStrainSegments = u02::kLoopSegments;
constexpr int kRestSlot = 7;  // the still/rest form diagnostic

struct RingSeg {
  int ring = -1;
  int seg = -1;
};

// (bind x, y, z) -> (ring, segment). Segment order is derived from the bind
// cross-section ANGLE about the tube centre, never from meshlet emission order,
// so a duplicated seam vertex and a re-ordered meshlet both land in the same
// slot. Built once from the compiled type.
std::map<std::array<int32_t, 3>, RingSeg> seg_index_map(
    const zc::CreatureType& T,
    std::vector<std::array<V3, kStrainSegments>>& bind_pos,
    std::vector<int>& bind_have) {
  const auto rmap = ring_map();
  // ring -> the distinct bind cross-section points found on it
  std::vector<std::vector<std::array<int32_t, 3>>> per_ring(u02::kLoopRings);
  for (const zc::Meshlet& m : T.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (const zc::SkinVertex& v : m.verts) {
      const auto it = rmap.find(v.y);
      if (it == rmap.end()) continue;
      const std::array<int32_t, 3> key{v.x, v.y, v.z};
      auto& bucket = per_ring[it->second];
      if (std::find(bucket.begin(), bucket.end(), key) == bucket.end())
        bucket.push_back(key);
    }
  }
  std::map<std::array<int32_t, 3>, RingSeg> out;
  bind_pos.assign(u02::kLoopRings, {});
  bind_have.assign(u02::kLoopRings, 0);
  const double cx = static_cast<double>(u02::kLoopTubeXMm);
  for (int r = 0; r < u02::kLoopRings; ++r) {
    auto& bucket = per_ring[r];
    if (bucket.empty()) continue;
    std::sort(bucket.begin(), bucket.end(),
              [&](const std::array<int32_t, 3>& a,
                  const std::array<int32_t, 3>& b) {
                const double ax = a[0] * kFx * 1000.0 - cx;
                const double az = a[2] * kFx * 1000.0;
                const double bx = b[0] * kFx * 1000.0 - cx;
                const double bz = b[2] * kFx * 1000.0;
                return std::atan2(az, ax) < std::atan2(bz, bx);
              });
    const int n = static_cast<int>(bucket.size());
    if (n != kStrainSegments) continue;  // not a full ring: left out of both windows
    for (int k = 0; k < n; ++k) {
      out[bucket[k]] = RingSeg{r, k};
      bind_pos[r][k] = V3{bucket[k][0] * kFx * 1000.0,
                          bucket[k][1] * kFx * 1000.0,
                          bucket[k][2] * kFx * 1000.0};
    }
    bind_have[r] = 1;
  }
  return out;
}

// ⚠ THE REFERENCE LENGTH IS THE REST POSE, NOT THE BIND POSE, and the first
// version of this instrument got that wrong in a way that read as evidence.
// Against BIND, the FRONT window measured rail 2.45 max / 0.18 min while the
// rear measured 1.22 / 0.14 -- so the rear looked like the CALMER half of the
// antenna and the metric quietly argued there was no defect. Slot 7, the
// codebase's own static rest diagnostic, settled it: at rest the front already
// reads 1.96 / 0.41 and the rear reads 1.000 / 0.969. The front's strain is the
// AUTHORED LOOP -- the bind tube is straight and the rest pose curves it, so its
// outer rail is stretched and its inner rail folded by design, on every frame,
// including ones nobody is complaining about. Dividing by bind therefore
// measures "how bent is this creature", which is a shape decision, and buries
// the thing being asked about underneath it.
//
// Against REST, the rear's entire strain is motion: 1.000/0.969 standing still
// becomes 1.43/0.125 moving. THAT is the owner's sentence, measured.
//
// (CLAUDE.md: measurement can remove a bias; it cannot choose a value. Removing
// this bias is the whole job of the rest baseline. The ceilings are still set
// from looked-at frames.)
// The rest reference: slot 7 is this codebase's static form diagnostic (see
// manafold_art.h, "slot 7 reports the rest pose's lowest vertex"), so it is the
// pose every rest-relative number here is divided by. Filled once.
bool rest_edge_lengths(const zc::CreatureType& T,
                       const std::map<std::array<int32_t, 3>, RingSeg>& smap,
                       std::vector<std::array<double, kStrainSegments>>& rail0,
                       std::vector<std::array<double, kStrainSegments>>& hoop0) {
  rail0.assign(u02::kLoopRings, {});
  hoop0.assign(u02::kLoopRings, {});
  const zc::Clip* still = nullptr;
  for (const zc::Clip& c : T.bank.clips)
    if (c.slot_id == kRestSlot) still = &c;
  if (still == nullptr || still->frame_count <= 0) return false;
  std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
  zc::decode_pose(T, *still, 0, pose, nullptr, 0);
  const zc::mat3x4fx& R = pose[u02::kBRoot];
  std::vector<std::array<V3, kStrainSegments>> pos(u02::kLoopRings);
  std::vector<int> have(u02::kLoopRings, 0);
  for (const zc::Meshlet& m : T.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (const zc::SkinVertex& v : m.verts) {
      const auto sit = smap.find(std::array<int32_t, 3>{v.x, v.y, v.z});
      if (sit == smap.end()) continue;
      int32_t ox, oy, oz;
      zc::skin_vertex(pose.data(), v, ox, oy, oz, nullptr);
      pos[sit->second.ring][sit->second.seg] = to_root_pt(
          R, V3{ox * kFx * 1000.0, oy * kFx * 1000.0, oz * kFx * 1000.0});
      have[sit->second.ring] |= 1 << sit->second.seg;
    }
  }
  const int full = (1 << kStrainSegments) - 1;
  for (int i = 0; i < u02::kLoopRings; ++i) {
    if (have[i] != full) continue;
    for (int k = 0; k < kStrainSegments; ++k) {
      const int k2 = (k + 1) % kStrainSegments;
      hoop0[i][k] = len(pos[i][k2] - pos[i][k]);
      if (i + 1 < u02::kLoopRings && have[i + 1] == full)
        rail0[i][k] = len(pos[i + 1][k] - pos[i][k]);
    }
  }
  return true;
}

struct Strain {
  double rail_max = 1.0;        // worst posed/REST longitudinal ratio, rear window
  double rail_min = 1.0;        // worst fold-over there
  double hoop_max = 1.0;
  double front_rail_max = 1.0;  // the same measure on the front, as the
  double front_rail_min = 1.0;  // like-for-like reference it can only be once
                                // both sides are rest-relative
  int rail_ring = 0;
  int rail_min_ring = 0;
  int hoop_ring = 0;
  int front_rail_min_ring = 0;
  // THE HAND-OFF DISAGREEMENT, which is what linear blend skinning is being
  // asked to hide. For a two-bone vertex, the distance between where bone b0
  // alone would put it and where bone b1 alone would put it. LBS places it on
  // the straight line between those two points, so this length IS the size of
  // the collapse the blend can produce, in millimetres, before any weight is
  // chosen. A vertex whose two bones agree cannot fold however it is weighted.
  double handoff_mm = 0;
  int handoff_ring = 0;
  int handoff_b0 = 0, handoff_b1 = 0;
};

struct Sample {
  double rel = 0, axis = 0, bend = 0, front_bend = 0, sock = 0, arm = 0;
  int bend_ring = 0;
  double span_mm = 0;
  V3 end, last, c;
  M3 relm;
  Strain str;
};

struct Motion {
  double path = 0, vmax = 0, amax = 0, jmax = 0, jrms = 0;
};

Motion motion_of(const std::vector<V3>& p) {
  Motion m;
  const size_t n = p.size();
  if (n < 4) return m;
  // periodic finite differences over the presentation sequence (mm per sample)
  std::vector<V3> v(n), a(n), j(n);
  for (size_t i = 0; i < n; ++i) v[i] = p[(i + 1) % n] - p[i];
  for (size_t i = 0; i < n; ++i) a[i] = v[(i + 1) % n] - v[i];
  for (size_t i = 0; i < n; ++i) j[i] = a[(i + 1) % n] - a[i];
  double js = 0;
  for (size_t i = 0; i < n; ++i) {
    m.path += len(v[i]);
    m.vmax = std::max(m.vmax, len(v[i]));
    m.amax = std::max(m.amax, len(a[i]));
    m.jmax = std::max(m.jmax, len(j[i]));
    js += dot(j[i], j[i]);
  }
  m.jrms = std::sqrt(js / static_cast<double>(n));
  return m;
}

double angle_series_rate_max(const std::vector<double>& s) {
  double r = 0;
  for (size_t i = 1; i < s.size(); ++i) r = std::max(r, std::fabs(s[i] - s[i - 1]));
  return r;
}

}  // namespace

// ---- the gate (--gate) ----------------------------------------------------
// Categories, each with its own committed positive control:
//   R1 FRAME 0x1  every bank key/midpoint: the arm<->End frame rotation and the
//                 rear centreline turn stay under named ceilings. Control
//                 --fail-rear-frame restores the v18 legacy-root frame.
//   R2 JOINT 0x2  the End joint's own angular step per 60 Hz sample stays under
//                 a named ceiling. Control --fail-rear-joint triples the
//                 End's ambient rotation (the "spazz" amplified).
//   R3 LINE  0x4  the mana-line width law: exact legacy width at/above the
//                 full radius, never wider than legacy, monotone in projected
//                 size, >= 1 px, and strictly thinner at Drift's distance.
//                 Control --fail-line-scale selects the legacy law.
// The ceilings are regression guards chosen with margin over the looked-at
// shipping values (P19-IMPLEMENTATION.md); they decide no art value. Shipping:
// worst rotation 16.5 deg, worst turn ~35 deg, worst joint step 2.76 deg. The
// frame control genuinely violates R1 AND R2 (v18 had both a hairpin and a
// fast joint): its DECLARED mask is 0x3 and any other mask fails the leg.
// PASS 19 REVIEW: R2 now measures the joint's TRUE angular step (shipping
// worst 2.76, Hover) and its ceiling moved 4.0 -> 6.0. At 4.0 it sat on the
// 600 ambient rung (4.07) -- a rung judged by eye -- so the gate was holding an
// art decision and would have refused the owner a notch more wiggle. 6.0 guards
// the REGRESSION instead: the version-18 snap (ambient 1000 in the arm frame
// 6.67; legacy-root frame 7.16) fires, and ambient values the eye may still
// choose (up to ~850) are left to the eye.
constexpr double kGateRelMaxDeg = 40.0;
constexpr double kGateBendMaxDeg = 60.0;
constexpr double kGateJointStepMaxDeg = 6.0;
constexpr int32_t kGateLineFarRadiusPx = 128;  // Drift's projected radius

struct ClipStats {
  size_t samples = 0;
  double rel_max = 0, rel_mean = 0, rel_step = 0, axis_max = 0;
  double bend_max = 0, bend_mean = 0, front_bend_max = 0;
  double sock_max = 0, sock_step = 0, arm_step = 0, arm_dev = 0;
  int bend_ring = 0;
  size_t bend_at = 0, rel_step_at = 0;
  Motion end, last, c;
  // PASS 20 strain (R4)
  double rail_max = 1.0, rail_min = 1.0, hoop_max = 1.0;
  double front_rail_max = 1.0, front_rail_min = 1.0;
  double rail_step = 0.0;
  int rail_ring = 0, rail_min_ring = 0, hoop_ring = 0, front_rail_min_ring = 0;
  size_t rail_at = 0, rail_min_at = 0, rail_step_at = 0;
  double handoff_mm = 0;
  int handoff_ring = 0, handoff_b0 = 0, handoff_b1 = 0;
  // THE REAR SIGNED-SPAN EXCURSION. kBSpanDeltaE carries, as an unskinned
  // receipt, the full |C->socket| distance minus its rest value -- the amount
  // the rear span is asked to STRETCH on this sample. It is written straight
  // into the three SpanDeltaE helpers as pure +Y, so the rear skin absorbs all
  // of it. This is the authority behind "it stretches too much".
  double span_max_mm = 0, span_min_mm = 0, span_step_mm = 0;
  size_t span_max_at = 0;
};

ClipStats analyse(const zc::CreatureType& T, const zc::Clip& clip, int slot,
                  bool csv, bool dump_rings, int dump_frame,
                  bool strain_rings = false) {
  const auto rmap = ring_map();
  // PASS 20: the strain grid. Built once per clip from the compiled type.
  std::vector<std::array<V3, kStrainSegments>> bind_pos;
  std::vector<int> bind_have;
  const auto smap = seg_index_map(T, bind_pos, bind_have);
  std::vector<std::array<double, kStrainSegments>> rail0, hoop0;
  const bool have_rest = rest_edge_lengths(T, smap, rail0, hoop0);
  const int ring_c = ring_of_station(u02::kKnuckleAtCMm);
  const int ring_end = ring_of_station(u02::kKnuckleAtEndMm);
  const int ring_last = (ring_c + ring_end) / 2;
  // first ring whose BOTH neighbouring segments lie past C's exit blend
  const int rear_first =
      ring_of_station(u02::kKnuckleAtCMm + u02::kLoopCarrierCoreHalfMm[3] +
                      u02::kFoldBlendMm[3]) + 2;
  std::vector<Sample> seq;
  V3 arm0{};
  bool have_arm0 = false;
  for (int f = 0; f < clip.frame_count; ++f) {
    for (uint8_t sub = 0; sub <= 1; ++sub) {
      if (sub == 1 && clip.mid_quats.empty()) continue;
      std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
      zc::decode_pose(T, clip, static_cast<uint16_t>(f), pose, nullptr, sub);
      const zc::mat3x4fx& R = pose[u02::kBRoot];
      std::vector<V3> cen(u02::kLoopRings);
      std::vector<int> cnt(u02::kLoopRings, 0);
      // PASS 20: the same walk fills the strain grid. The posed point is kept
      // per (ring, segment) BEFORE the centroid sum below averages it away.
      std::vector<std::array<V3, kStrainSegments>> pos(u02::kLoopRings);
      std::vector<int> have(u02::kLoopRings, 0);
      double ho_mm = 0;
      int ho_ring = 0, ho_b0 = 0, ho_b1 = 0;
      for (const zc::Meshlet& m : T.mesh) {
        if (!is_loop_meshlet(m)) continue;
        for (const zc::SkinVertex& v : m.verts) {
          const auto it = rmap.find(v.y);
          if (it == rmap.end()) continue;
          int32_t ox, oy, oz;
          zc::skin_vertex(pose.data(), v, ox, oy, oz, nullptr);
          const V3 w{ox * kFx * 1000.0, oy * kFx * 1000.0, oz * kFx * 1000.0};
          const V3 rp = to_root_pt(R, w);
          cen[it->second] = cen[it->second] + rp;
          ++cnt[it->second];
          const auto sit = smap.find(std::array<int32_t, 3>{v.x, v.y, v.z});
          if (sit != smap.end()) {
            pos[sit->second.ring][sit->second.seg] = rp;
            have[sit->second.ring] |= 1 << sit->second.seg;
            // The two ends of the blend, through the PRODUCTION skinning path:
            // the same vertex with all weight on b0, then all on b1.
            if (v.b0 != v.b1 && sit->second.ring >= rear_first) {
              zc::SkinVertex a = v, b = v;
              a.w0 = 64;
              b.w0 = 0;
              int32_t ax, ay, az, bx, by, bz;
              zc::skin_vertex(pose.data(), a, ax, ay, az, nullptr);
              zc::skin_vertex(pose.data(), b, bx, by, bz, nullptr);
              const double d = len(V3{(ax - bx) * kFx * 1000.0,
                                      (ay - by) * kFx * 1000.0,
                                      (az - bz) * kFx * 1000.0});
              if (d > ho_mm) {
                ho_mm = d;
                ho_ring = sit->second.ring;
                ho_b0 = v.b0;
                ho_b1 = v.b1;
              }
            }
          }
        }
      }
      for (int i = 0; i < u02::kLoopRings; ++i)
        if (cnt[i] > 0) cen[i] = cen[i] * (1.0 / cnt[i]);
      Sample s;
      {
        const std::vector<int32_t>& tr =
            sub == 1 && !clip.mid_local_translation.empty()
                ? clip.mid_local_translation
                : clip.local_translation;
        const size_t ti =
            (static_cast<size_t>(f) * u02::kBoneCount +
             static_cast<size_t>(u02::kBSpanDeltaE)) * 3u + 1u;
        if (ti < tr.size()) s.span_mm = tr[ti] * kFx * 1000.0;
      }
      s.str.handoff_mm = ho_mm;
      s.str.handoff_ring = ho_ring;
      s.str.handoff_b0 = ho_b0;
      s.str.handoff_b1 = ho_b1;
      s.rel = rel_angle_deg(pose[u02::kBHingeD], pose[u02::kBRearSocket]);
      s.relm = rel_matrix(pose[u02::kBHingeD], pose[u02::kBRearSocket]);
      s.axis = ang_deg(col(pose[u02::kBHingeD], 1), col(pose[u02::kBRearSocket], 1));
      s.sock = rel_angle_deg(R, pose[u02::kBRearSocket]);
      const V3 arm = to_root_dir(R, col(pose[u02::kBHingeD], 1));
      if (!have_arm0) {
        arm0 = arm;
        have_arm0 = true;
      }
      s.arm = ang_deg(arm0, arm);
      for (int i = 1; i + 1 < u02::kLoopRings - 1; ++i) {  // skip buried cap
        if (cnt[i - 1] == 0 || cnt[i] == 0 || cnt[i + 1] == 0) continue;
        const double b = ang_deg(cen[i] - cen[i - 1], cen[i + 1] - cen[i]);
        if (i >= rear_first) {
          if (b > s.bend) {
            s.bend = b;
            s.bend_ring = i;
          }
        } else if (i >= 8 && i <= ring_c - 2) {
          s.front_bend = std::max(s.front_bend, b);
        }
      }
      // PASS 20: the strain sweep. The rear window is the same one bend uses;
      // the last rail (62->63) is EXCLUDED because ring 63 is the declared
      // rigid buried cap (kRearTerminalTipStationMm) and its authored collapse
      // to kReturnTipCapRxMm is a profile decision, not a stretch. It is
      // printed separately rather than silently folded in.
      {
        const int full = (1 << kStrainSegments) - 1;
        const int last_rail = u02::kLoopRings - 3;  // rail 61->62 is the last gated one
        for (int i = 0; i <= last_rail; ++i) {
          if (!bind_have[i] || !bind_have[i + 1]) continue;
          if (have[i] != full || have[i + 1] != full) continue;
          const bool rear = i >= rear_first;
          const bool front = i >= 8 && i <= ring_c - 2;
          if (!rear && !front) continue;
          for (int k = 0; k < kStrainSegments; ++k) {
            const double b = have_rest ? rail0[i][k]
                                      : len(bind_pos[i + 1][k] - bind_pos[i][k]);
            if (b < 1e-6) continue;
            const double r = len(pos[i + 1][k] - pos[i][k]) / b;
            if (rear) {
              if (r > s.str.rail_max) {
                s.str.rail_max = r;
                s.str.rail_ring = i;
              }
              if (r < s.str.rail_min) {
                s.str.rail_min = r;
                s.str.rail_min_ring = i;
              }
            } else {
              s.str.front_rail_max = std::max(s.str.front_rail_max, r);
              if (r < s.str.front_rail_min) {
                s.str.front_rail_min = r;
                s.str.front_rail_min_ring = i;
              }
            }
            if (strain_rings && f == dump_frame && sub == 0)
              std::printf("strain,%d,%d,%d,%d,%.4f\n", slot, f, i, k, r);
          }
        }
        for (int i = 0; i <= last_rail + 1; ++i) {
          if (!bind_have[i] || have[i] != full || i < rear_first) continue;
          for (int k = 0; k < kStrainSegments; ++k) {
            const int k2 = (k + 1) % kStrainSegments;
            const double b = have_rest ? hoop0[i][k]
                                      : len(bind_pos[i][k2] - bind_pos[i][k]);
            if (b < 1e-6) continue;
            const double r = len(pos[i][k2] - pos[i][k]) / b;
            if (r > s.str.hoop_max) {
              s.str.hoop_max = r;
              s.str.hoop_ring = i;
            }
          }
        }
      }
      s.end = cen[ring_end];
      s.last = cen[ring_last];
      s.c = cen[ring_c];
      if (dump_rings && f == dump_frame && sub == 0) {
        for (int i = 0; i < u02::kLoopRings; ++i)
          std::printf("ring,%d,%d,%d,%.1f,%.1f,%.1f\n", slot, f, i, cen[i].x,
                      cen[i].y, cen[i].z);
        const V3 ya = to_root_dir(R, col(pose[u02::kBHingeD], 1));
        const V3 ys = to_root_dir(R, col(pose[u02::kBRearSocket], 1));
        std::printf("axis,HingeD +Y %.3f %.3f %.3f | RearSocket +Y %.3f %.3f %.3f\n",
                    ya.x, ya.y, ya.z, ys.x, ys.y, ys.z);
      }
      if (csv)
        std::printf("csv,%d,%d,%u,%.2f,%.2f,%.2f,%d,%.2f,%.2f,%.2f,"
                    "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.4f,%.4f,%d,%d,%.4f\n",
                    slot, f, sub, s.rel, s.axis, s.bend, s.bend_ring,
                    s.front_bend, s.sock, s.arm, s.end.x, s.end.y, s.end.z,
                    s.last.x, s.last.y, s.last.z,
                    s.str.rail_max, s.str.rail_min, s.str.rail_ring,
                    s.str.rail_min_ring, s.str.hoop_max);
      seq.push_back(s);
    }
  }
  ClipStats st;
  st.samples = seq.size();
  std::vector<V3> pe, pl, pc;
  for (size_t i = 0; i < seq.size(); ++i) {
    const Sample& s = seq[i];
    pe.push_back(s.end);
    pl.push_back(s.last);
    pc.push_back(s.c);
    st.rel_max = std::max(st.rel_max, s.rel);
    st.axis_max = std::max(st.axis_max, s.axis);
    st.front_bend_max = std::max(st.front_bend_max, s.front_bend);
    st.sock_max = std::max(st.sock_max, s.sock);
    st.arm_dev = std::max(st.arm_dev, s.arm);
    st.rel_mean += s.rel;
    st.bend_mean += s.bend;
    // PASS 20 strain aggregation
    if (s.str.rail_max > st.rail_max) {
      st.rail_max = s.str.rail_max;
      st.rail_ring = s.str.rail_ring;
      st.rail_at = i;
    }
    if (s.str.rail_min < st.rail_min) {
      st.rail_min = s.str.rail_min;
      st.rail_min_ring = s.str.rail_min_ring;
      st.rail_min_at = i;
    }
    if (s.str.front_rail_min < st.front_rail_min) {
      st.front_rail_min = s.str.front_rail_min;
      st.front_rail_min_ring = s.str.front_rail_min_ring;
    }
    if (s.str.hoop_max > st.hoop_max) {
      st.hoop_max = s.str.hoop_max;
      st.hoop_ring = s.str.hoop_ring;
    }
    st.front_rail_max = std::max(st.front_rail_max, s.str.front_rail_max);
    if (s.span_mm > st.span_max_mm) {
      st.span_max_mm = s.span_mm;
      st.span_max_at = i;
    }
    st.span_min_mm = std::min(st.span_min_mm, s.span_mm);
    if (i > 0)
      st.span_step_mm =
          std::max(st.span_step_mm, std::fabs(s.span_mm - seq[i - 1].span_mm));
    if (s.str.handoff_mm > st.handoff_mm) {
      st.handoff_mm = s.str.handoff_mm;
      st.handoff_ring = s.str.handoff_ring;
      st.handoff_b0 = s.str.handoff_b0;
      st.handoff_b1 = s.str.handoff_b1;
    }
    if (i > 0) {
      const double ds = std::fabs(s.str.rail_max - seq[i - 1].str.rail_max);
      if (ds > st.rail_step) {
        st.rail_step = ds;
        st.rail_step_at = i;
      }
    }
    if (s.bend > st.bend_max) {
      st.bend_max = s.bend;
      st.bend_ring = s.bend_ring;
      st.bend_at = i;
    }
    if (i > 0) {
      // PASS 19 REVIEW: the joint's true angular step, not the change of its
      // bend magnitude (see step_angle_deg).
      const double dr = step_angle_deg(seq[i - 1].relm, s.relm);
      if (dr > st.rel_step) {
        st.rel_step = dr;
        st.rel_step_at = i;
      }
      st.sock_step = std::max(st.sock_step, std::fabs(s.sock - seq[i - 1].sock));
      st.arm_step = std::max(st.arm_step, std::fabs(s.arm - seq[i - 1].arm));
    }
  }
  if (!seq.empty()) {
    st.rel_mean /= seq.size();
    st.bend_mean /= seq.size();
  }
  st.end = motion_of(pe);
  st.last = motion_of(pl);
  st.c = motion_of(pc);
  return st;
}

void print_stats(int slot, const ClipStats& s) {
  std::printf(
      "slot %2d (%zu samples): rel max %.1f mean %.1f step %.2f (sample %zu) deg | "
      "axis max %.1f | rear bend max %.1f (ring %d, sample %zu) mean %.1f | "
      "front bend max %.1f | socket-local max %.1f step %.2f | arm dev %.1f "
      "step %.2f\n",
      slot, s.samples, s.rel_max, s.rel_mean, s.rel_step, s.rel_step_at,
      s.axis_max, s.bend_max, s.bend_ring, s.bend_at, s.bend_mean,
      s.front_bend_max, s.sock_max, s.sock_step, s.arm_dev, s.arm_step);
  std::printf(
      "  motion (root-local, mm/sample): End  path %.0f vmax %.1f amax %.2f "
      "jmax %.2f jrms %.3f\n"
      "                                  last path %.0f vmax %.1f amax %.2f "
      "jmax %.2f jrms %.3f\n"
      "                                  C    path %.0f vmax %.1f amax %.2f "
      "jmax %.2f jrms %.3f\n",
      s.end.path, s.end.vmax, s.end.amax, s.end.jmax, s.end.jrms, s.last.path,
      s.last.vmax, s.last.amax, s.last.jmax, s.last.jrms, s.c.path, s.c.vmax,
      s.c.amax, s.c.jmax, s.c.jrms);
  std::printf(
      "  strain (posed/REST): REAR rail max %.3f (ring %d, sample %zu) min %.3f "
      "(ring %d, sample %zu) step %.4f (sample %zu) hoop max %.3f (ring %d)\n"
      "                       FRONT rail max %.3f min %.3f (ring %d)"
      "  | rear-vs-front excess: max %+.3f min %+.3f\n"
      "                       HAND-OFF worst two-bone disagreement %.0f mm "
      "(ring %d, bones %d/%d)\n"
      "                       REAR SPAN excursion %+.0f .. %+.0f mm "
      "(max at sample %zu), worst step %.1f mm/sample\n",
      s.rail_max, s.rail_ring, s.rail_at, s.rail_min, s.rail_min_ring,
      s.rail_min_at, s.rail_step, s.rail_step_at, s.hoop_max, s.hoop_ring,
      s.front_rail_max, s.front_rail_min, s.front_rail_min_ring,
      s.rail_max - s.front_rail_max, s.rail_min - s.front_rail_min,
      s.handoff_mm, s.handoff_ring, s.handoff_b0, s.handoff_b1,
      s.span_max_mm, s.span_min_mm, s.span_max_at, s.span_step_mm);
}

// R3: the width law, swept over projected size for every line radius in use.
int line_law_failures(bool verbose) {
  const int32_t radii[] = {u02::u02_strand_core_r(), u02::u02_strand_dark_r(),
                           u02::u02_shimmer_r(),     u02::kBoltCoreRPx,
                           u02::kBoltHaloRPx,        u02::kFoldEdgeHaloRPx,
                           u02::kFoldEdgeCoreRPx};
  int fails = 0;
  bool far_thinner = false;
  for (int32_t r : radii) {
    int32_t prev = 0;
    for (int32_t px = 1; px <= 2 * u02::g_u02_mana_line_full_radius_px; ++px) {
      const int32_t w = u02::mana_line_r_px(r, px * 256);
      if (w < 1 || w > r) ++fails;
      if (w < prev) ++fails;  // monotone in projected size
      if (px >= u02::g_u02_mana_line_full_radius_px && w != r) ++fails;
      prev = w;
    }
    const int32_t far = u02::mana_line_r_px(r, kGateLineFarRadiusPx * 256);
    if (r >= 4 && far < r) far_thinner = true;
    if (verbose)
      std::printf("  line r %2d px: far(%d px) -> %d px, full(%d px) -> %d px\n", r,
                  kGateLineFarRadiusPx, far, u02::g_u02_mana_line_full_radius_px,
                  u02::mana_line_r_px(r, u02::g_u02_mana_line_full_radius_px * 256));
  }
  if (!far_thinner) ++fails;
  return fails;
}

// PASS 19 REVIEW: R3's law sweep proves mana_line_r_px is right, but not that
// any production splat is routed through it. This census runs the production
// fold + bolt producers (the same mana_fill(3)/mana_lightning msmooth traces)
// over complete Drift and Hover and requires BOTH populations: flagged LINE
// splats (so the renderer's scaling has something to act on) and unflagged
// splats (so motes/bodies/glows kept their size). Control --fail-line-flag
// strips the flags after production and must fire R3 alone.
bool g_fail_line_flag = false;
int line_flag_failures(const zc::CreatureType& T, bool verbose) {
  int fails = 0;
  for (int want : {1, 0}) {
    const zc::Clip* clip = nullptr;
    for (const zc::Clip& c : T.bank.clips)
      if (c.slot_id == want) clip = &c;
    if (!clip) {
      ++fails;
      continue;
    }
    u02::FoldState state{};
    std::vector<u02::ManaSplat> splats;
    size_t n_line = 0, n_other = 0;
    const int samples = clip->frame_count * 2;
    for (int pf = 0; pf < samples; ++pf) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
      zc::decode_pose(T, *clip, static_cast<uint16_t>(pf / 2), pose, nullptr,
                      static_cast<uint8_t>(pf & 1));
      const u02::FxAnchors anchors = u02::fx_anchors_from_pose(T, pose);
      splats.clear();
      int32_t agit = 0;
      u02::mana_fill(3, static_cast<uint32_t>(pf), clip->slot_id,
                     clip->frame_count, anchors, state, 1000, splats, &agit);
      u02::mana_lightning(static_cast<uint32_t>(pf), clip->slot_id,
                          clip->frame_count, anchors, splats, 1000);
      for (u02::ManaSplat& ms : splats) {
        if (g_fail_line_flag) ms.line = false;
        (ms.line ? n_line : n_other) += 1;
      }
    }
    if (verbose)
      std::printf("  line census slot %d: %zu line splats, %zu other splats\n",
                  want, n_line, n_other);
    if (n_line == 0 || n_other == 0) ++fails;
  }
  return fails;
}

int main(int argc, char** argv) {
  std::vector<int> slots;
  bool csv = false;
  bool dump_rings = false;
  bool strain_rings = false;
  bool gate = false;
  int dump_frame = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--csv") == 0) {
      csv = true;
    } else if (std::strcmp(argv[i], "--rings") == 0 && i + 1 < argc) {
      dump_rings = true;
      dump_frame = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--strain-rings") == 0 && i + 1 < argc) {
      strain_rings = true;
      dump_frame = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--gate") == 0) {
      gate = true;
    } else if (std::strcmp(argv[i], "--fail-rear-frame") == 0) {
      gate = true;
      u02::g_u02_rear_socket_frame = u02::RearSocketFrame::kLegacyRoot;
      std::printf("MUTANT: --fail-rear-frame (v18 legacy-root End frame)\n");
    } else if (std::strcmp(argv[i], "--fail-rear-joint") == 0) {
      gate = true;
      u02::g_u02_rear_ambient_gain_pm = 3 * u02::kRearSocketAmbientGainPm;
      std::printf("MUTANT: --fail-rear-joint (End ambient rotation x3)\n");
    } else if (std::strcmp(argv[i], "--fail-line-flag") == 0) {
      gate = true;
      g_fail_line_flag = true;
      std::printf("MUTANT: --fail-line-flag (production line flags stripped)\n");
    } else if (std::strcmp(argv[i], "--fail-line-scale") == 0) {
      gate = true;
      u02::g_u02_mana_line_scale = u02::ManaLineScale::kLegacy;
      std::printf("MUTANT: --fail-line-scale (legacy constant-pixel lines)\n");
    } else if (argv[i][0] >= '0' && argv[i][0] <= '9') {
      slots.push_back(std::atoi(argv[i]));
    } else {
      std::fprintf(stderr, "manafold-rear-audit: unknown argument %s\n", argv[i]);
      return 2;
    }
  }
  // The renderer's knobs, so ladder rungs can be audited too (diagnostic).
  if (const char* e = std::getenv("ZHAO_U02_REAR_SOCKET_FRAME")) {
    if (std::strcmp(e, "arm") == 0)
      u02::g_u02_rear_socket_frame = u02::RearSocketFrame::kArm;
    else if (std::strcmp(e, "legacy-root") == 0)
      u02::g_u02_rear_socket_frame = u02::RearSocketFrame::kLegacyRoot;
    else
      return 2;
  }
  if (const char* e = std::getenv("ZHAO_U02_REAR_SOCKET_FOLLOW_PM"))
    u02::g_u02_rear_socket_follow_pm = std::atoi(e);
  if (const char* e = std::getenv("ZHAO_U02_REAR_AMBIENT_GAIN_PM"))
    u02::g_u02_rear_ambient_gain_pm = std::atoi(e);

  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::fprintf(stderr, "manafold-rear-audit: FAIL compile produced no meshlets\n");
    return 1;
  }
  std::printf("rear frame %s, follow %d pm, ambient gain %d pm, line law %s full %d px\n",
              u02::g_u02_rear_socket_frame == u02::RearSocketFrame::kArm
                  ? "arm"
                  : "legacy-root",
              u02::g_u02_rear_socket_follow_pm, u02::g_u02_rear_ambient_gain_pm,
              u02::g_u02_mana_line_scale == u02::ManaLineScale::kDistance
                  ? "distance"
                  : "legacy",
              u02::g_u02_mana_line_full_radius_px);
  if (slots.empty()) {
    if (gate) {
      for (const zc::Clip& c : T.bank.clips) slots.push_back(c.slot_id);
    } else {
      slots = {0, 5, 21, 2, 1, 13};
    }
  }
  unsigned mask = 0;
  double w_rel = 0, w_bend = 0, w_step = 0;
  int w_rel_slot = -1, w_bend_slot = -1, w_step_slot = -1;
  for (int want : slots) {
    const zc::Clip* clip = nullptr;
    for (const zc::Clip& c : T.bank.clips)
      if (c.slot_id == want) clip = &c;
    if (!clip) {
      std::printf("slot %d: absent\n", want);
      continue;
    }
    const ClipStats st =
        analyse(T, *clip, want, csv, dump_rings, dump_frame, strain_rings);
    print_stats(want, st);
    if (st.rel_max > w_rel) {
      w_rel = st.rel_max;
      w_rel_slot = want;
    }
    if (st.bend_max > w_bend) {
      w_bend = st.bend_max;
      w_bend_slot = want;
    }
    if (st.rel_step > w_step) {
      w_step = st.rel_step;
      w_step_slot = want;
    }
  }
  if (!gate) return 0;
  std::printf("\nR1 FRAME: worst arm<->End rotation %.2f deg (slot %d, ceiling %.1f); "
              "worst rear centreline turn %.2f deg (slot %d, ceiling %.1f)\n",
              w_rel, w_rel_slot, kGateRelMaxDeg, w_bend, w_bend_slot,
              kGateBendMaxDeg);
  if (w_rel > kGateRelMaxDeg || w_bend > kGateBendMaxDeg) {
    mask |= 0x1;
    std::printf("FAIL R1 FRAME: the End carrier does not continue the arm\n");
  }
  std::printf("R2 JOINT: worst End joint step %.2f deg/sample (slot %d, ceiling %.1f)\n",
              w_step, w_step_slot, kGateJointStepMaxDeg);
  if (w_step > kGateJointStepMaxDeg) {
    mask |= 0x2;
    std::printf("FAIL R2 JOINT: the End joint turns too fast\n");
  }
  const int lf = line_law_failures(true) + line_flag_failures(T, true);
  std::printf("R3 LINE: %d law violations\n", lf);
  if (lf != 0) {
    mask |= 0x4;
    std::printf("FAIL R3 LINE: mana lines do not thin with distance like the ink\n");
  }
  std::printf("rear gate mask 0x%X -> %s\n", mask, mask ? "RED" : "GREEN");
  return mask ? 1 : 0;
}
