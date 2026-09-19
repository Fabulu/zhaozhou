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

struct Sample {
  double rel = 0, axis = 0, bend = 0, front_bend = 0, sock = 0, arm = 0;
  int bend_ring = 0;
  V3 end, last, c;
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
// worst rotation 16.5 deg, worst turn ~35 deg, worst joint step 2.41 deg. The
// frame control genuinely violates R1 AND R2 (v18 had both a hairpin and a
// fast joint): its DECLARED mask is 0x3 and any other mask fails the leg.
// Raising kRearSocketAmbientGainPm past ~650 trips R2 by design -- move the
// ceiling consciously with the picture that justifies it.
constexpr double kGateRelMaxDeg = 40.0;
constexpr double kGateBendMaxDeg = 60.0;
constexpr double kGateJointStepMaxDeg = 4.0;
constexpr int32_t kGateLineFarRadiusPx = 128;  // Drift's projected radius

struct ClipStats {
  size_t samples = 0;
  double rel_max = 0, rel_mean = 0, rel_step = 0, axis_max = 0;
  double bend_max = 0, bend_mean = 0, front_bend_max = 0;
  double sock_max = 0, sock_step = 0, arm_step = 0, arm_dev = 0;
  int bend_ring = 0;
  size_t bend_at = 0, rel_step_at = 0;
  Motion end, last, c;
};

ClipStats analyse(const zc::CreatureType& T, const zc::Clip& clip, int slot,
                  bool csv, bool dump_rings, int dump_frame) {
  const auto rmap = ring_map();
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
      for (const zc::Meshlet& m : T.mesh) {
        if (!is_loop_meshlet(m)) continue;
        for (const zc::SkinVertex& v : m.verts) {
          const auto it = rmap.find(v.y);
          if (it == rmap.end()) continue;
          int32_t ox, oy, oz;
          zc::skin_vertex(pose.data(), v, ox, oy, oz, nullptr);
          const V3 w{ox * kFx * 1000.0, oy * kFx * 1000.0, oz * kFx * 1000.0};
          cen[it->second] = cen[it->second] + to_root_pt(R, w);
          ++cnt[it->second];
        }
      }
      for (int i = 0; i < u02::kLoopRings; ++i)
        if (cnt[i] > 0) cen[i] = cen[i] * (1.0 / cnt[i]);
      Sample s;
      s.rel = rel_angle_deg(pose[u02::kBHingeD], pose[u02::kBRearSocket]);
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
                    "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
                    slot, f, sub, s.rel, s.axis, s.bend, s.bend_ring,
                    s.front_bend, s.sock, s.arm, s.end.x, s.end.y, s.end.z,
                    s.last.x, s.last.y, s.last.z);
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
    if (s.bend > st.bend_max) {
      st.bend_max = s.bend;
      st.bend_ring = s.bend_ring;
      st.bend_at = i;
    }
    if (i > 0) {
      const double dr = std::fabs(s.rel - seq[i - 1].rel);
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

int main(int argc, char** argv) {
  std::vector<int> slots;
  bool csv = false;
  bool dump_rings = false;
  bool gate = false;
  int dump_frame = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--csv") == 0) {
      csv = true;
    } else if (std::strcmp(argv[i], "--rings") == 0 && i + 1 < argc) {
      dump_rings = true;
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
    const ClipStats st = analyse(T, *clip, want, csv, dump_rings, dump_frame);
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
  const int lf = line_law_failures(true);
  std::printf("R3 LINE: %d law violations\n", lf);
  if (lf != 0) {
    mask |= 0x4;
    std::printf("FAIL R3 LINE: mana lines do not thin with distance like the ink\n");
  }
  std::printf("rear gate mask 0x%X -> %s\n", mask, mask ? "RED" : "GREEN");
  return mask ? 1 : 0;
}
