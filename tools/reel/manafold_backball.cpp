// manafold_backball.cpp -- THE COMMITTED BACK-BALL MOTION DECOMPOSITION probe
// and gate for creature 02 (Manafold).
//
// Owner's complaint, standing since Direction 20 and restated in Direction 26:
// on HOVER the rear ball is too finicky and moves too much; every other clip is
// right. Two levers were tried and both failed, and both failures were
// MEASUREMENT failures rather than art failures:
//
//   * pass 24 lowered the rear AMBIENT scale (400 -> 170). It moved 178 px,
//     because the ambient is a small minority of the motion.
//   * pass 25 laddered kRearCarrierCalmPm across its whole range including
//     fully off. Carrier C's motion changed by 1.3 %, because a joint's own
//     rotation cannot move that joint -- and the "52x" that motivated the
//     ladder was CHANGED PIXELS BETWEEN TWO CONFIGURATIONS (an offset) quoted
//     for a question about motion.
//
// Nobody had ever asked the only question that matters: WHAT ACTUALLY MOVES THE
// BACK BALL. This tool answers it, and it answers it the way CLAUDE.md requires
// -- by muting one authority at a time ON THE POSED RESULT and measuring the
// posed SURFACE, never a rendered frame and never a screen-space diff.
//
// ---------------------------------------------------------------------------
// WHAT "THE BACK BALL" IS, EXACTLY
//
// Under the pass-21 rods rig the band is four rods and four balls. The BACK
// BALL is ball element 3 -- `rods_ring(i).role == kBall && elem == 3` -- the
// End swell at the body surface, carried by kBRearSocket. The LAST ROD is rod
// element 3, the C->End rod ("the last antennae part at the back", Direction
// 20). Both are read off the posed skin, so neither can be confused with a bone
// origin that moves no pixel (the pass-20 lesson at mrear's R5).
//
// The loop's skin carries NO deform authority (kLoopStretchStrength == 0, so
// every loop ring's role is kNone), which is why posing without a deform frame
// is exact here and not an approximation.
//
// ---------------------------------------------------------------------------
// THE DECOMPOSITION, AND WHY IT IS DONE ON THE BAKED CLIP
//
// The authorities that could move the back ball do not exist as separable
// signals at pose time -- they are composed into three baked channels:
//
//   quats[kBRearSocket]              the ball's own orientation
//   local_translation[kBRearSocket]  the ball's own position, which
//                                    finalize_rear_follow sets to the DEFORMED
//                                    body surface point, so it is exactly "the
//                                    socket following the breathing body"
//   quats[kBJunctionF..kBHingeD]     the chain that carries the last rod
//
// and the socket's orientation is itself a product,
//
//   q_socket = normalize(qd * authored)
//   qd       = q[JunctionF] * q[Neck] * q[A] * q[B] * q[C] * q[D]
//
// where `qd` is the arm's solved ARRIVAL FRAME (the rear rod aim: the whole
// antenna's accumulated life, plus the two-stage aim at the moving socket
// target) and `authored` is everything the clip wrote onto the End carrier --
// the End ambient oscillator station and the knead's B2 wag.
//
// So a mute is a named, exact edit of the BAKED clip, applied to every key and
// every presentation midpoint, and then the whole thing is re-posed. Freezing
// is to SAMPLE 0. Motion metrics (path, speed, acceleration, jerk) are
// offset-invariant, so freezing at sample 0 rather than at a mean changes no
// reported number.
//
// A share is 1 - muted/baseline. Shares do NOT sum to 1 and are not claimed to:
// the authorities overlap by construction (muting the chain also freezes part
// of qd). The table prints them side by side and says which is dominant; it
// does not pretend to be an orthogonal basis.
//
// ---------------------------------------------------------------------------
// THE DAMPING THIS TOOL EXISTS TO MEASURE, AND THE GATE
//
// `--gate` bounds Hover's back-ball motion energy and JERK per presentation
// sample, on the posed surface, against ceilings named below. `--fail-undamped`
// is its positive control: the damping switched off, which is the state the
// owner complained about, so the control fires on the exact fault the gate
// names. Run it before quoting the gate's silence (CLAUDE.md: a detector
// reading zero is a claim, and it is the claim to check hardest).
//
// Usage:
//   manafold-backball.exe [--slot N] [--decompose] [--csv] [--worst]
//   manafold-backball.exe --gate [--fail-undamped | --fail-window]

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

constexpr double kFx = 1.0 / 65536.0;
constexpr double kPi = 3.14159265358979;

struct V3 {
  double x = 0, y = 0, z = 0;
};
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double len(V3 a) { return std::sqrt(dot(a, a)); }

V3 col(const zc::mat3x4fx& m, int j) {
  return {m.m[j] * kFx, m.m[4 + j] * kFx, m.m[8 + j] * kFx};
}
V3 to_root_dir(const zc::mat3x4fx& r, V3 w) {
  return {dot(col(r, 0), w), dot(col(r, 1), w), dot(col(r, 2), w)};
}
V3 to_root_pt(const zc::mat3x4fx& r, V3 w) {
  const V3 t{r.m[3] * kFx * 1000.0, r.m[7] * kFx * 1000.0,
             r.m[11] * kFx * 1000.0};
  return to_root_dir(r, w - t);
}

// The relative rotation Ra^T Rb as a row-major 3x3, columns normalized. Same
// construction mrear uses, for the same reason: quat16 products are not unit
// and quat16_to_mat3 scales by |q|^2, which blinds a trace-based angle.
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
// The angle of P^T Q. |angle(Q)| - |angle(P)| is only a lower bound and reads
// ZERO for a joint sweeping at constant bend, which is exactly what the End's
// tilt and yaw oscillators do in quadrature.
double step_angle_deg(const M3& p, const M3& q) {
  double tr = 0;
  for (int k = 0; k < 3; ++k)
    for (int i = 0; i < 3; ++i) tr += p.m[3 * i + k] * q.m[3 * i + k];
  double c = (tr - 1.0) / 2.0;
  c = std::max(-1.0, std::min(1.0, c));
  return std::acos(c) * 180.0 / kPi;
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

// bind y -> ring index, exactly as the model samples it, through the ONE
// station accessor (u02::loop_ring_station_at). No second copy of the law.
std::map<int32_t, int> ring_map() {
  std::map<int32_t, int> out;
  const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  for (int i = 0; i < u02::kLoopRings; ++i)
    out[u02::fxu(y0 + u02::loop_ring_station_at(i))] = i;
  return out;
}

// ---- THE THREE READ WINDOWS, named off the rods layout ---------------------
//
// Under the pass-20 (non-rods) rig `rods_ring` does not describe the band, so
// the windows fall back to a station distance around the authored pivots. The
// shipping rig is rods; the fallback exists so the tool cannot silently
// describe the wrong band if the rig knob is moved.
// Element indices, so the owner's own words map onto named code: ball 0 is the
// lower-FRONT ball at pivot A ("Hover's front ball is already right"), ball 1
// the peak at B, ball 2 the upper-rear at C, ball 3 the End swell at the body
// surface -- the BACK BALL. Rod e runs from pivot e to pivot e+1, so rod 3 is
// the C->End run, "the last antennae part at the back" (Direction 20).
constexpr int kBallCount = 4;
const int32_t kBallPivotMm[kBallCount] = {u02::kKnuckleAtAMm, u02::kKnuckleAtBMm,
                                          u02::kKnuckleAtCMm,
                                          u02::kKnuckleAtEndMm};

int ring_ball_elem(int i) {
  if (u02::rig_rods()) {
    const u02::RodsRing r = u02::rods_ring(i);
    return r.role == u02::RingRole::kBall ? r.elem : -1;
  }
  const int32_t s = u02::loop_ring_station_at(i);
  for (int e = 0; e < kBallCount; ++e) {
    const int32_t d = s > kBallPivotMm[e] ? s - kBallPivotMm[e]
                                          : kBallPivotMm[e] - s;
    if (d <= u02::kLoopCarrierCoreHalfMm[e + 1]) return e;
  }
  return -1;
}
int ring_rod_elem(int i) {
  if (u02::rig_rods()) {
    const u02::RodsRing r = u02::rods_ring(i);
    return r.role == u02::RingRole::kRod ? r.elem : -1;
  }
  const int32_t s = u02::loop_ring_station_at(i);
  for (int e = 0; e < kBallCount; ++e) {
    const int32_t lo = e == 0 ? 0 : kBallPivotMm[e - 1] +
                                        u02::kLoopCarrierCoreHalfMm[e];
    const int32_t hi = kBallPivotMm[e] - u02::kLoopCarrierCoreHalfMm[e + 1];
    if (s > lo && s < hi) return e;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// THE MUTES. Each is an exact, named edit of the BAKED clip.
enum class Mute {
  kNone,
  kBodyFollow,   // the socket following the breathing body (its translation)
  kArmAim,       // the rear rod aim: the arm's solved arrival frame qd
  kEndAuthored,  // the End's own authored rotation (End ambient + knead B2 wag)
  kChain,        // the antenna's upstream life (JunctionF/Neck/A/B/C/D quats)
  // ...and the same chain ONE JOINT AT A TIME, which is the resolution the
  // whole question turns on: a share against "the antenna's life" is not
  // actionable, because that life is what four owner directions approved. A
  // share against a NAMED STATION is.
  kJointF, kJointNeck, kJointA, kJointB, kJointC, kJointD,
  kNodules,      // the carrier nodule offsets (the swallow beat + the ambient
                 // nodule schedule + the knead dip), carried by the span
                 // helpers' translations
  kHelpers,      // the rear span/bow helper bones, rotation and translation
  kRootBob,      // the body's own bob and attitude
  kCount
};
const char* mute_name(Mute m) {
  switch (m) {
    case Mute::kNone: return "baseline (nothing muted)";
    case Mute::kBodyFollow: return "socket follows breathing body";
    case Mute::kArmAim: return "rear rod aim (arm arrival qd)";
    case Mute::kEndAuthored: return "End authored rot (ambient+knead B2)";
    case Mute::kChain: return "antenna upstream life (F/N/A/B/C/D)";
    case Mute::kJointF: return "  .. station F alone (junction)";
    case Mute::kJointNeck: return "  .. station Neck alone";
    case Mute::kJointA: return "  .. station A alone";
    case Mute::kJointB: return "  .. station B alone (the peak)";
    case Mute::kJointC: return "  .. station C alone";
    case Mute::kJointD: return "  .. station D alone (closure solver)";
    case Mute::kNodules: return "carrier nodule offsets (swallow+ambient)";
    case Mute::kHelpers: return "rear span/bow helpers";
    case Mute::kRootBob: return "body bob + attitude (root)";
    default: return "?";
  }
}
const char* mute_tag(Mute m) {
  switch (m) {
    case Mute::kNone: return "base";
    case Mute::kBodyFollow: return "bodyfollow";
    case Mute::kArmAim: return "armaim";
    case Mute::kEndAuthored: return "endauth";
    case Mute::kChain: return "chain";
    case Mute::kJointF: return "jointF";
    case Mute::kJointNeck: return "jointNeck";
    case Mute::kJointA: return "jointA";
    case Mute::kJointB: return "jointB";
    case Mute::kJointC: return "jointC";
    case Mute::kJointD: return "jointD";
    case Mute::kNodules: return "nodules";
    case Mute::kHelpers: return "helpers";
    case Mute::kRootBob: return "rootbob";
    default: return "?";
  }
}

const uint8_t kChainBones[6] = {u02::kBJunctionF, u02::kBNeck, u02::kBHingeA,
                                u02::kBHingeB,    u02::kBHingeC, u02::kBHingeD};
const uint8_t kHelperBones[8] = {
    u02::kBSpanDeltaA,       u02::kBSpanDeltaB,     u02::kBSpanDeltaC,
    u02::kBSpanDeltaE,       u02::kBSpanDeltaEStart, u02::kBSpanDeltaEMid,
    u02::kBSpanDeltaEPreSocket, u02::kBRearRootDelta};

/** qd for one sample: the product of the chain's six local quats, which is
 *  exactly what finalize_rear_follow composed (`Q * aim`, and `aim` is what it
 *  stored into quats[kBHingeD]). */
zc::quat16 arm_arrival(const std::vector<zc::quat16>& q, size_t qbase) {
  zc::quat16 r = q[qbase + u02::kBJunctionF];
  for (int i = 1; i < 6; ++i)
    r = u02::quat_mul(r, q[qbase + kChainBones[i]]);
  return r;
}

/** Apply one mute to a copy of the baked clip. Keys and presentation midpoints
 *  are edited identically -- a mute that skipped the midpoints would leave half
 *  the samples live and report a share that is half the truth. */
zc::Clip muted(const zc::Clip& in, Mute m) {
  zc::Clip c = in;
  if (m == Mute::kNone) return c;
  const int BC = u02::kBoneCount;
  const int N = c.frame_count;
  if (N <= 0) return c;
  const bool have_mid_q = c.mid_quats.size() == c.quats.size();
  const bool have_mid_t =
      c.mid_local_translation.size() == c.local_translation.size();

  auto freeze_trans = [&](uint8_t b) {
    const size_t src = (static_cast<size_t>(b)) * 3u;
    if (c.local_translation.size() < static_cast<size_t>(N) * BC * 3u) return;
    const int32_t v0 = c.local_translation[src + 0];
    const int32_t v1 = c.local_translation[src + 1];
    const int32_t v2 = c.local_translation[src + 2];
    for (int f = 0; f < N; ++f) {
      const size_t i = (static_cast<size_t>(f) * BC + b) * 3u;
      c.local_translation[i + 0] = v0;
      c.local_translation[i + 1] = v1;
      c.local_translation[i + 2] = v2;
      if (have_mid_t) {
        c.mid_local_translation[i + 0] = v0;
        c.mid_local_translation[i + 1] = v1;
        c.mid_local_translation[i + 2] = v2;
      }
    }
  };
  auto freeze_quat = [&](uint8_t b) {
    const zc::quat16 q0 = c.quats[static_cast<size_t>(b)];
    for (int f = 0; f < N; ++f) {
      c.quats[static_cast<size_t>(f) * BC + b] = q0;
      if (have_mid_q) c.mid_quats[static_cast<size_t>(f) * BC + b] = q0;
    }
  };

  switch (m) {
    case Mute::kBodyFollow:
      freeze_trans(u02::kBRearSocket);
      freeze_trans(u02::kBReturnTip);
      break;
    case Mute::kArmAim:
    case Mute::kEndAuthored: {
      // q_socket = qd * authored. Hold ONE factor at sample 0 and keep the
      // other live, for keys and midpoints alike.
      const zc::quat16 qd0 = arm_arrival(c.quats, 0);
      const zc::quat16 s0 = c.quats[u02::kBRearSocket];
      const zc::quat16 auth0 = u02::quat_mul(u02::quat_conj(qd0), s0);
      auto rewrite = [&](std::vector<zc::quat16>& qv) {
        for (int f = 0; f < N; ++f) {
          const size_t qb = static_cast<size_t>(f) * BC;
          const zc::quat16 qd = arm_arrival(qv, qb);
          zc::quat16 out;
          if (m == Mute::kArmAim) {
            const zc::quat16 auth =
                u02::quat_mul(u02::quat_conj(qd), qv[qb + u02::kBRearSocket]);
            out = u02::quat_mul(qd0, auth);
          } else {
            out = u02::quat_mul(qd, auth0);
          }
          // The production renormalizer, exactly as rear_socket_compose ends.
          qv[qb + u02::kBRearSocket] = zc::quat16_nlerp(out, out, 1, 2);
        }
      };
      rewrite(c.quats);
      if (have_mid_q) rewrite(c.mid_quats);
      break;
    }
    case Mute::kChain:
      for (uint8_t b : kChainBones) freeze_quat(b);
      break;
    case Mute::kJointF: freeze_quat(u02::kBJunctionF); break;
    case Mute::kJointNeck: freeze_quat(u02::kBNeck); break;
    case Mute::kJointA: freeze_quat(u02::kBHingeA); break;
    case Mute::kJointB: freeze_quat(u02::kBHingeB); break;
    case Mute::kJointC: freeze_quat(u02::kBHingeC); break;
    case Mute::kJointD: freeze_quat(u02::kBHingeD); break;
    case Mute::kNodules:
      // The carrier offsets reach the skin ONLY through the span-delta
      // helpers' local translations (mrear's R5 lesson: a ball travels half a
      // metre with its nominal bone's origin never moving). Freezing the
      // translations and leaving the rotations live separates "the carriers
      // travel" from "the chain turns", which no earlier instrument did.
      for (uint8_t b : kHelperBones) freeze_trans(b);
      freeze_trans(u02::kBRearPreRootDelta);
      freeze_trans(u02::kBRearRootTurnMid);
      break;
    case Mute::kHelpers:
      for (uint8_t b : kHelperBones) {
        freeze_quat(b);
        freeze_trans(b);
      }
      freeze_quat(u02::kBRearPreRootDelta);
      freeze_trans(u02::kBRearPreRootDelta);
      freeze_quat(u02::kBRearRootTurnMid);
      freeze_trans(u02::kBRearRootTurnMid);
      break;
    case Mute::kRootBob: {
      freeze_quat(u02::kBRoot);
      if (c.root.size() >= static_cast<size_t>(N) * 3u) {
        const int32_t r0 = c.root[0], r1 = c.root[1], r2 = c.root[2];
        for (int f = 0; f < N; ++f) {
          c.root[static_cast<size_t>(f) * 3 + 0] = r0;
          c.root[static_cast<size_t>(f) * 3 + 1] = r1;
          c.root[static_cast<size_t>(f) * 3 + 2] = r2;
        }
      }
      break;
    }
    default: break;
  }
  return c;
}

// ---------------------------------------------------------------------------
struct Series {
  std::vector<V3> world, local;
  std::vector<M3> frame;  // the ball's own orientation, root-relative
};
struct Energy {
  double path = 0, vmax = 0, amax = 0, jmax = 0, jrms = 0;
  size_t jmax_at = 0, vmax_at = 0;
};
struct AngEnergy {
  double path = 0, vmax = 0, jmax = 0, jrms = 0;
  size_t jmax_at = 0;
};

// Cyclic differencing on a looping clip: the seam is a real sample pair, and
// treating it as an endpoint hides exactly the frame a loop is most likely to
// be wrong on.
template <typename T, typename F>
std::vector<double> step_series(const std::vector<T>& s, bool cyc, F stepfn) {
  const size_t n = s.size();
  std::vector<double> d(n, 0.0);
  if (n < 2) return d;
  const size_t first = cyc ? 0 : 1;
  for (size_t i = first; i < n; ++i)
    d[i] = stepfn(s[(i + n - 1) % n], s[i]);
  return d;
}

Energy energy_of(const std::vector<V3>& p, bool cyc) {
  Energy e;
  const size_t n = p.size();
  if (n < 4) return e;
  std::vector<V3> v(n), a(n), j(n);
  for (size_t i = 0; i < n; ++i) v[i] = p[i] - p[(i + n - 1) % n];
  for (size_t i = 0; i < n; ++i) a[i] = v[i] - v[(i + n - 1) % n];
  for (size_t i = 0; i < n; ++i) j[i] = a[i] - a[(i + n - 1) % n];
  const size_t first = cyc ? 0 : 3;
  double j2 = 0;
  size_t cnt = 0;
  for (size_t i = first; i < n; ++i) {
    const double lv = len(v[i]), la = len(a[i]), lj = len(j[i]);
    e.path += lv;
    if (lv > e.vmax) { e.vmax = lv; e.vmax_at = i; }
    e.amax = std::max(e.amax, la);
    if (lj > e.jmax) { e.jmax = lj; e.jmax_at = i; }
    j2 += lj * lj;
    ++cnt;
  }
  e.jrms = cnt ? std::sqrt(j2 / static_cast<double>(cnt)) : 0.0;
  return e;
}

AngEnergy ang_energy_of(const std::vector<M3>& f, bool cyc) {
  AngEnergy e;
  const size_t n = f.size();
  if (n < 4) return e;
  const std::vector<double> w =
      step_series(f, true, [](const M3& a, const M3& b) { return step_angle_deg(a, b); });
  std::vector<double> acc(n, 0.0), jrk(n, 0.0);
  for (size_t i = 0; i < n; ++i) acc[i] = w[i] - w[(i + n - 1) % n];
  for (size_t i = 0; i < n; ++i) jrk[i] = acc[i] - acc[(i + n - 1) % n];
  const size_t first = cyc ? 0 : 3;
  double j2 = 0;
  size_t cnt = 0;
  for (size_t i = first; i < n; ++i) {
    e.path += w[i];
    e.vmax = std::max(e.vmax, w[i]);
    if (std::fabs(jrk[i]) > e.jmax) { e.jmax = std::fabs(jrk[i]); e.jmax_at = i; }
    j2 += jrk[i] * jrk[i];
    ++cnt;
  }
  e.jrms = cnt ? std::sqrt(j2 / static_cast<double>(cnt)) : 0.0;
  return e;
}

struct Read {
  Series ball[kBallCount];  // 0 = front (A) ... 3 = the BACK BALL (End)
  Series rod[kBallCount];   // rod e runs pivot e -> pivot e+1
  size_t samples = 0;
  bool cyclic = false;
};

Read read_clip(const zc::CreatureType& T, const zc::Clip& clip) {
  Read out;
  out.cyclic = clip.interpolate && !clip.hold_last;
  const auto rmap = ring_map();
  std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
  for (int f = 0; f < clip.frame_count; ++f) {
    for (uint8_t sub = 0; sub <= 1; ++sub) {
      if (sub == 1 && clip.mid_quats.empty()) continue;
      zc::decode_pose(T, clip, static_cast<uint16_t>(f), pose, nullptr, sub);
      const zc::mat3x4fx& R = pose[u02::kBRoot];
      V3 wb[kBallCount]{}, lb[kBallCount]{}, wr[kBallCount]{}, lr[kBallCount]{};
      int nb[kBallCount]{}, nr[kBallCount]{};
      for (const zc::Meshlet& m : T.mesh) {
        if (!is_loop_meshlet(m)) continue;
        for (const zc::SkinVertex& v : m.verts) {
          const auto it = rmap.find(v.y);
          if (it == rmap.end()) continue;
          const int ring = it->second;
          const int be = ring_ball_elem(ring);
          const int re = ring_rod_elem(ring);
          if (be < 0 && re < 0) continue;
          int32_t ox, oy, oz;
          zc::skin_vertex(pose.data(), v, ox, oy, oz, nullptr);
          const V3 w{ox * kFx * 1000.0, oy * kFx * 1000.0, oz * kFx * 1000.0};
          const V3 rp = to_root_pt(R, w);
          if (be >= 0) { wb[be] = wb[be] + w; lb[be] = lb[be] + rp; ++nb[be]; }
          if (re >= 0) { wr[re] = wr[re] + w; lr[re] = lr[re] + rp; ++nr[re]; }
        }
      }
      const uint8_t ball_bone[kBallCount] = {u02::kBHingeA, u02::kBHingeB,
                                             u02::kBHingeC, u02::kBRearSocket};
      for (int e = 0; e < kBallCount; ++e) {
        if (nb[e]) { wb[e] = wb[e] * (1.0 / nb[e]); lb[e] = lb[e] * (1.0 / nb[e]); }
        if (nr[e]) { wr[e] = wr[e] * (1.0 / nr[e]); lr[e] = lr[e] * (1.0 / nr[e]); }
        out.ball[e].world.push_back(wb[e]);
        out.ball[e].local.push_back(lb[e]);
        out.ball[e].frame.push_back(rel_matrix(R, pose[ball_bone[e]]));
        out.rod[e].world.push_back(wr[e]);
        out.rod[e].local.push_back(lr[e]);
      }
      ++out.samples;
    }
  }
  return out;
}
constexpr int kBack = 3;   // the back ball / the last rod
constexpr int kFront = 0;  // the front ball, which the owner says is right

// ---------------------------------------------------------------------------
// THE GATE.
//
// Both ceilings are read off the SHIPPING bank, not invented: they sit a
// declared margin above the worst live subject other than Hover, because the
// owner's sentence is "every other clip is right". A Hover that reads inside
// the band every other clip already occupies is a Hover that no longer stands
// out. The margin is generous on purpose -- this gate protects a repair from
// regressing, it does not decide the art.
// THREE BOUNDS, TWO-SIDED, on Hover's rear -- and the quantities are the ones
// the decomposition said an eye can see, not the ones the earlier passes named.
// The End swell's own position is deliberately NOT gated: it moves 0.99 mm per
// sample, which is under a tenth of a native pixel at this framing, so a bound
// on it would be a bound on something nobody can look at.
//
// Shipping (win 21, gain 700, slot 0): C jerk rms 4.929, End angular 1.380
// deg/sample, C travel 13.005 mm/sample, C angular 1.850 deg/sample.
//
// G1  carrier C's positional JERK rms. The "finicky" operand.
//     Ceiling 5.60 -- 14 % over the shipped value; the undamped bake reads
//     6.586 and fires it.
constexpr double kGateCJerkRms = 5.60;
// G2  the End swell's ANGULAR rate, which the decomposition found is 90 % the
//     rear rod aim and which the damping cuts by 41 %.
//     Ceiling 1.70 deg/sample -- 23 % over; undamped reads 2.339 and fires it.
constexpr double kGateEndAngPerSample = 1.70;
// G3  THE TWO FLOORS, and they are the half of this gate that protects the ART.
//     Direction 20: the antenna stays "a bit wiggly". A future pass that
//     widened the window or raised the gain until the rear went dead would pass
//     G1 and G2 with flying colours -- a gate that can only be satisfied by
//     REMOVING motion is a gate that asks for a corpse, and this creature has
//     already shipped one knob whose whole job was to remove motion.
//     Floors 11.50 mm/sample and 1.20 deg/sample against the shipped 13.005 and
//     1.850. Reachable with LEGAL stimulus -- window 151 at full gain reads
//     10.587 and 0.846 and fires both -- so no committed mutant is needed and
//     "it can fire" is not left as an argument. Sized so the owner can still
//     take the gain to 1000 at the shipped window (12.004 / 1.403) without
//     tripping them: a floor that forbids the next rung of its own ladder is a
//     floor that has stopped describing the art.
constexpr double kGateCTravelFloor = 11.50;
constexpr double kGateCAngFloor = 1.20;
constexpr int kGateSlot = 0;  // hover/inspect play slot 0

}  // namespace

int main(int argc, char** argv) {
  int slot = 0;
  bool csv = false, gate = false, worst = false, all_slots = false;
  bool census = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--slot") == 0 && i + 1 < argc) {
      slot = std::atoi(argv[++i]);
    } else if (std::strcmp(argv[i], "--census") == 0) {
      census = true;
    } else if (std::strcmp(argv[i], "--csv") == 0) {
      csv = true;
    } else if (std::strcmp(argv[i], "--worst") == 0) {
      worst = true;
    } else if (std::strcmp(argv[i], "--all-slots") == 0) {
      all_slots = true;
    } else if (std::strcmp(argv[i], "--decompose") == 0) {
      // the default; accepted so the receipts can name it
    } else if (std::strcmp(argv[i], "--gate") == 0) {
      gate = true;
    } else if (std::strcmp(argv[i], "--fail-undamped") == 0) {
      // THE POSITIVE CONTROL, and it is the owner's own fault state: the back-
      // ball damping switched off with its production knob. It is also the
      // EXACT-OFF control for the mechanism, so one switch proves both that the
      // gate can see the damping and that the feature is cleanly removable.
      gate = true;
      u02::g_u02_backball_damp_pm = 0;  // 0, not the sentinel -- see kBackBallDampPmUnset
      std::printf("MUTANT: --fail-undamped (back-ball damping switched off "
                  "everywhere; this is also the EXACT-OFF state, so one switch "
                  "proves both that the gate sees the damping and that the "
                  "mechanism is cleanly removable)\n");
    } else if (std::strcmp(argv[i], "--fail-window") == 0) {
      // The SECOND operand. The damping is a moving-average window; a gain of
      // 1000 with a window of 1 sample is an identity filter, so a gate that
      // only watched the gain would read "damping on" over a creature that is
      // not damped at all. This control moves the window instead.
      gate = true;
      u02::g_u02_backball_damp_win = 1;
      std::printf("MUTANT: --fail-window (damping window collapsed to 1 key "
                  "-- an identity filter with the gain still reading 700)\n");
    } else if (std::strcmp(argv[i], "--fail-dead") == 0) {
      // G3's control, and it is the one that matters most for the ART. It
      // over-damps until the rear stops being wiggly, which every ceiling in
      // this gate would happily accept. Reachable with LEGAL stimulus -- a
      // window of 151 keys at full gain -- so no committed mutant is needed and
      // "it can fire" is not left as an argument.
      gate = true;
      u02::g_u02_backball_damp_win = 151;
      u02::g_u02_backball_damp_pm = 1000;
      std::printf("MUTANT: --fail-dead (window 151 keys at full gain -- the "
                  "rear damped until it stops living)\n");
    } else {
      std::fprintf(stderr, "manafold-backball: unknown argument %s\n", argv[i]);
      return 2;
    }
  }
  // The ONE shared parser -- an env control is only a control in a binary that
  // reads it, and this creature has twice shipped a knob that only one main()
  // could see.
  if (!u02::apply_knead_dip_env()) return 2;

  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::fprintf(stderr, "manafold-backball: FAIL compile produced no meshlets\n");
    return 1;
  }
  std::printf("back-ball damping: bank-wide %d pm (-1 = the table decides), "
              "window %d KEYS, clip table [slot 0 = %d pm]\n",
              u02::g_u02_backball_damp_pm, u02::g_u02_backball_damp_win,
              u02::backball_damp_pm(0));

  if (census) {
    // THE LIKE-FOR-LIKE COMPARISON, and it exists because the owner's premise
    // -- "every other clip is right" -- is a claim about a COMPARISON and
    // nothing had ever made it. Per bake slot, the same three posed-surface
    // readings Hover's decomposition reports, so Hover's numbers can be put
    // beside the clips nobody complained about instead of judged alone.
    // ⚠ EVERY RATE IS PER SAMPLE, NEVER A CLIP TOTAL. A summed path measures
    // how LONG the clip is at least as much as how busy it is, and Hover is the
    // longest clip in the bank (300 keys, 600 presentation samples, against 90
    // to 200 for most of the rest). The first run of this census printed totals
    // and made Hover look like the worst clip in the bank by 2x -- which is the
    // mismatched-comparison law in CLAUDE.md, committed by the very tool
    // written to avoid it. Totals are still printed, marked, so the mistake is
    // visible rather than silently corrected away.
    std::printf("%-5s %6s | %7s %7s %7s %7s | %7s %7s %7s %7s | %6s %6s %7s\n",
                "slot", "smpls", "A mm/s", "B mm/s", "C mm/s", "ENDmm/s",
                "Adeg/s", "Bdeg/s", "Cdeg/s", "ENDdeg/s", "END/A", "ANGr",
                "ENDjrms");
    for (const zc::Clip& c : T.bank.clips) {
      const Read r = read_clip(T, c);
      const double n = r.samples > 0 ? static_cast<double>(r.samples) : 1.0;
      double bs[kBallCount], as[kBallCount];
      for (int e = 0; e < kBallCount; ++e) {
        bs[e] = energy_of(r.ball[e].local, r.cyclic).path / n;
        as[e] = ang_energy_of(r.ball[e].frame, r.cyclic).path / n;
      }
      // THE REAR-VS-FRONT RATIO is the owner's own comparison -- "Hover's front
      // ball is already right" -- and it is the one number immune to clip
      // length, to clip energy and to how hard the whole creature is working.
      const double ratio = bs[kFront] > 1e-9 ? bs[kBack] / bs[kFront] : 0.0;
      const double ang_ratio = as[kFront] > 1e-9 ? as[kBack] / as[kFront] : 0.0;
      std::printf("%-5d %6zu | %7.3f %7.3f %7.3f %7.3f | %7.3f %7.3f %7.3f %7.3f "
                  "| %6.2f %6.2f %7.3f\n",
                  static_cast<int>(c.slot_id), r.samples, bs[0], bs[1], bs[2],
                  bs[3], as[0], as[1], as[2], as[3], ratio, ang_ratio,
                  energy_of(r.ball[kBack].local, r.cyclic).jrms);
    }
    return 0;
  }

  std::vector<int> slots;
  if (gate || all_slots) {
    for (const zc::Clip& c : T.bank.clips) slots.push_back(c.slot_id);
  } else {
    slots.push_back(slot);
  }

  int rc = 0;
  for (int s : slots) {
    const zc::Clip* clip = nullptr;
    for (const zc::Clip& c : T.bank.clips)
      if (c.slot_id == s) clip = &c;
    if (clip == nullptr) {
      std::fprintf(stderr, "manafold-backball: no clip in slot %d\n", s);
      return 2;
    }

    if (gate) {
      const Read r = read_clip(T, *clip);
      const Energy ec = energy_of(r.ball[2].local, r.cyclic);
      const AngEnergy ea = ang_energy_of(r.ball[kBack].frame, r.cyclic);
      const double n = r.samples > 0 ? static_cast<double>(r.samples) : 1.0;
      const double c_travel = ec.path / n;
      const double end_ang = ea.path / n;
      const bool judged = s == kGateSlot;
      const bool b1 = judged && ec.jrms > kGateCJerkRms;
      const bool b2 = judged && end_ang > kGateEndAngPerSample;
      const double c_ang = ang_energy_of(r.ball[2].frame, r.cyclic).path / n;
      const bool b3 = judged && (c_travel < kGateCTravelFloor ||
                                 c_ang < kGateCAngFloor);
      std::printf("G1 %-4s slot %2d: carrier C jerk rms %6.3f (<= %.3f)%s\n",
                  b1 ? "FAIL" : (judged ? "OK" : "info"), s, ec.jrms,
                  kGateCJerkRms,
                  judged ? "" : "  [not judged: the complaint is Hover's]");
      std::printf("G2 %-4s slot %2d: End angular %6.3f deg/sample (<= %.3f)\n",
                  b2 ? "FAIL" : (judged ? "OK" : "info"), s, end_ang,
                  kGateEndAngPerSample);
      std::printf("G3 %-4s slot %2d: carrier C travel %6.3f mm/sample (>= %.3f)"
                  " and C angular %6.3f deg/sample (>= %.3f)"
                  "  [the ART floors: the rear must stay wiggly]\n",
                  b3 ? "FAIL" : (judged ? "OK" : "info"), s, c_travel,
                  kGateCTravelFloor, c_ang, kGateCAngFloor);
      if (b1 || b2 || b3) rc = 1;
      continue;
    }

    // ---- the decomposition ------------------------------------------------
    const Read base = read_clip(T, *clip);
    const Energy be_l = energy_of(base.ball[kBack].local, base.cyclic);
    const Energy be_w = energy_of(base.ball[kBack].world, base.cyclic);
    const AngEnergy ba = ang_energy_of(base.ball[kBack].frame, base.cyclic);
    const Energy br_l = energy_of(base.rod[kBack].local, base.cyclic);
    const Energy bc_l = energy_of(base.ball[2].local, base.cyclic);

    std::printf("\n=== slot %d: %zu presentation samples, %s ===\n", s,
                base.samples, base.cyclic ? "looping (cyclic diff)" : "one-shot");
    std::printf("BASELINE, root-local, posed surface:\n");
    std::printf("  back ball : path %8.1f mm  vmax %6.3f  amax %6.3f  "
                "jmax %6.3f @%zu  jrms %6.3f\n",
                be_l.path, be_l.vmax, be_l.amax, be_l.jmax, be_l.jmax_at,
                be_l.jrms);
    std::printf("  last rod  : path %8.1f mm  vmax %6.3f  jmax %6.3f  jrms %6.3f\n",
                br_l.path, br_l.vmax, br_l.jmax, br_l.jrms);
    std::printf("  C ball    : path %8.1f mm  vmax %6.3f  jmax %6.3f  jrms %6.3f\n",
                bc_l.path, bc_l.vmax, bc_l.jmax, bc_l.jrms);
    std::printf("  ball angle: path %8.1f deg vmax %6.3f  jmax %6.3f  jrms %6.3f\n",
                ba.path, ba.vmax, ba.jmax, ba.jrms);
    std::printf("  (world frame, for scale: back-ball path %.1f mm)\n", be_w.path);

    const AngEnergy bca = ang_energy_of(base.ball[2].frame, base.cyclic);
    std::printf("\nMUTE ONE AUTHORITY AT A TIME (share = 1 - muted/baseline).\n"
                "Shares do NOT sum to 100%%: the authorities overlap by "
                "construction.\n");
    std::printf("%-40s | %7s %7s %7s | %7s %7s | %7s\n", "authority muted",
                "ENDpos", "ENDang", "ENDjrms", "C pos", "C ang", "rod pos");
    for (int mi = 1; mi < static_cast<int>(Mute::kCount); ++mi) {
      const Mute m = static_cast<Mute>(mi);
      const zc::Clip mc = muted(*clip, m);
      const Read r = read_clip(T, mc);
      const Energy e = energy_of(r.ball[kBack].local, r.cyclic);
      const AngEnergy a = ang_energy_of(r.ball[kBack].frame, r.cyclic);
      const Energy rr = energy_of(r.rod[kBack].local, r.cyclic);
      const Energy ec = energy_of(r.ball[2].local, r.cyclic);
      const AngEnergy ac = ang_energy_of(r.ball[2].frame, r.cyclic);
      auto sh = [](double muted_v, double base_v) {
        return base_v > 1e-9 ? 100.0 * (1.0 - muted_v / base_v) : 0.0;
      };
      std::printf("%-40s | %6.1f%% %6.1f%% %6.1f%% | %6.1f%% %6.1f%% | %6.1f%%\n",
                  mute_name(m), sh(e.path, be_l.path), sh(a.path, ba.path),
                  sh(e.jrms, be_l.jrms), sh(ec.path, bc_l.path),
                  sh(ac.path, bca.path), sh(rr.path, br_l.path));
      if (csv)
        std::printf("csv,%d,%s,%.3f,%.4f,%.3f,%.4f,%.3f,%.3f\n", s, mute_tag(m),
                    e.path, e.jrms, a.path, a.jrms, rr.path, ec.path);
    }

    if (worst) {
      // Sampling by BADNESS, not by index (CLAUDE.md: uniform sampling finds
      // the typical frame and misses the broken one).
      const size_t n = base.ball[kBack].local.size();
      std::vector<std::pair<double, size_t>> rank;
      std::vector<V3> v(n), a(n), j(n);
      for (size_t i = 0; i < n; ++i)
        v[i] = base.ball[kBack].local[i] - base.ball[kBack].local[(i + n - 1) % n];
      for (size_t i = 0; i < n; ++i) a[i] = v[i] - v[(i + n - 1) % n];
      for (size_t i = 0; i < n; ++i) j[i] = a[i] - a[(i + n - 1) % n];
      for (size_t i = 0; i < n; ++i) rank.push_back({len(j[i]), i});
      std::sort(rank.begin(), rank.end(),
                [](const std::pair<double, size_t>& x,
                   const std::pair<double, size_t>& y) { return x.first > y.first; });
      std::printf("\nWORST SAMPLES BY BACK-BALL JERK (sample = key*2+sub):\n");
      for (int k = 0; k < 8 && k < static_cast<int>(rank.size()); ++k)
        std::printf("  sample %4zu (key %3zu sub %zu): jerk %6.3f mm  speed %6.3f\n",
                    rank[k].second, rank[k].second / 2, rank[k].second % 2,
                    rank[k].first, len(v[rank[k].second]));
      // The same eight samples, with each authority muted, so the worst-frame
      // share is a measured number and not an extrapolation from the totals.
      std::printf("\nWORST-FRAME SHARES (jerk at the baseline's worst sample %zu):\n",
                  rank.empty() ? 0 : rank[0].second);
      const size_t wat = rank.empty() ? 0 : rank[0].second;
      const double wbase = rank.empty() ? 0 : rank[0].first;
      for (int mi = 1; mi < static_cast<int>(Mute::kCount); ++mi) {
        const Mute m = static_cast<Mute>(mi);
        const zc::Clip mc = muted(*clip, m);
        const Read r = read_clip(T, mc);
        const size_t nn = r.ball[kBack].local.size();
        if (nn != n) continue;
        std::vector<V3> vv(nn), aa(nn), jj(nn);
        for (size_t i = 0; i < nn; ++i)
          vv[i] = r.ball[kBack].local[i] - r.ball[kBack].local[(i + nn - 1) % nn];
        for (size_t i = 0; i < nn; ++i) aa[i] = vv[i] - vv[(i + nn - 1) % nn];
        for (size_t i = 0; i < nn; ++i) jj[i] = aa[i] - aa[(i + nn - 1) % nn];
        const double mv = len(jj[wat]);
        std::printf("  %-36s jerk %6.3f  share %5.1f%%\n", mute_name(m), mv,
                    wbase > 1e-9 ? 100.0 * (1.0 - mv / wbase) : 0.0);
      }
    }
  }
  if (gate) std::printf(rc == 0 ? "\nBACKBALL GATE: PASS\n"
                                : "\nBACKBALL GATE: FAIL\n");
  return rc;
}
