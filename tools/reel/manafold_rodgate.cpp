// manafold_rodgate.cpp -- PASS 21: THE POSED-SURFACE BAND GATE (mrod).
//
// Owner Direction 22 and its same-day addition: *"don't let actual antennae
// parts bend, just stretch. the bending is at the ball joints."* This is the
// instrument that judges it, and it judges the POSED SKIN -- zc::decode_pose +
// zc::skin_vertex on every key AND every baked midpoint of every clip -- never a
// rendered frame and never a bone quantity that can cancel a fold. Pass 19
// shipped a gate family blind to a fold because it measured operands that moved
// together; pass 20 shipped five wrong-operand instruments. So:
//
//   * EVERY LEG HAS A POSITIVE CONTROL, and the control is fired in the same
//     invocation as the assertion. A leg reading zero is a claim.
//   * the controls are REAL configurations, not asserted bugs: `--fail-*` runs
//     the same measurement against the pass-20 rig (whose corners sit one to
//     three rings before each ball) or against a named ball-blend mutant
//     control. After the repair the legs still assert the CORRECT behaviour; the
//     controls prove the instrument can see the wrong one.
//   * it reads the SHIPPING constants. print_judged_config is called first and
//     prints the RIG selector, which the pass-20 banner could not see.
//
// THE LEGS
//   R6  ROD STRAIGHT   per-ring centreline turn inside every rod <= 1.5 deg,
//                      ring-plane shear <= 5 deg, and the rod's worst
//                      perpendicular deviation from its own end-to-end chord
//                      (a smooth bow that per-ring turn would smear).
//   R7  JOINT ON BALL  the share of the band's total articulation that sits at
//                      the four balls, >= 98 %.
//   R8  JOINT SMOOTH   per-ball joint-angle step <= 8 deg/sample, and per-ring
//                      turn RATE inside every rod <= 8 deg/sample.
//   R9  ROD UNIFORM    per rod, min rail / max rail >= 850 pm (uniform stretch,
//                      not a fold).
//   R10 BALL RIGID     every ball ring's centre distance and hoop radius equal
//                      their BIND values (a rigid body preserves them exactly);
//                      an LBS blend across the equator shrinks by cos(theta/2).
//
// Usage:
//   manafold-rodgate.exe --gate [slot ...]        assert + fire every control
//   manafold-rodgate.exe --csv <out.csv> [slot ...]   the per-ring diagnostic
//   manafold-rodgate.exe --report [slot ...]      per-element tables, no verdict
//
// ⚠ WITH NO SLOT ARGUMENT THIS GATE MEASURES THE WHOLE BANK, and that is a
// PASS-21-CLOSE REPAIR, not how it shipped. The first version hard-coded
// `slots = {0, 2, 11, 5, 21, 7}` as its default -- SIX of the bank's
// twenty-four clips -- while mrear's gate enumerated `T.bank.clips`. So every
// headline this instrument printed was a claim about a QUARTER of the bank
// wearing the words "worst" and "every frame of every clip", and the pass-21
// implementation report published exactly that sentence. The review re-ran all
// 24 and the headline held (R9 947 -> 938 pm and R10 0.059 -> 0.107 % on the
// slots nobody had sampled, both far inside their bounds), which is luck, not
// evidence: a gate whose default inspects a fraction of its subject is the
// "a gate that cannot reach the state is not evidence about the state" law with
// the unreachable state being most of the bank. The default is now the bank, for
// --gate, --report AND --csv alike -- ONE rule, so no mode can quietly sample
// less than another. Passing slots explicitly still narrows it for diagnosis.
//
// Controls (each runs the measurement in a configuration that MUST fail):
//   --fail-rig-pass20 --fail-rod-twist --fail-ball-blend --fail-joint-step
//
// ⚠ THERE ARE FOUR CONTROLS AND THERE USED TO BE SIX NAMES FOR THEM. PASS-21
// CLOSE: `--fail-rod-bend`, `--fail-rod-uniform` and `--fail-rod-flicker` all
// set exactly `g_u02_rig = kPass20` and nothing else -- ONE blanket mutant under
// three names, firing all eight legs identically every time. The implementation
// report tabled them as three targeted controls with three distinct numbers,
// which is a coverage claim three times larger than the evidence. They are now
// the single honest name `--fail-rig-pass20`; the retired names are a hard error
// rather than a silent alias, because a script that still passes one must be
// told, not humoured. Each control also DECLARES the legs it must fail
// (kCtlTable) and the run is UNATTRIBUTED unless the observed failure set is
// exactly that -- the attribution check mspan has had since pass 17 and this
// gate shipped without.
//
// ⚠ IT GATES NOTHING ABOUT THE LOOK. The ball radius, the crotch at deep
// joints, the compacted rear rod: all reported as INFO, all decided by eye.

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
V3 cross(V3 a, V3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double len(V3 a) { return std::sqrt(dot(a, a)); }
double ang_deg(V3 a, V3 b) {
  const double la = len(a), lb = len(b);
  if (la < 1e-9 || lb < 1e-9) return 0.0;
  double c = dot(a, b) / (la * lb);
  c = std::max(-1.0, std::min(1.0, c));
  return std::acos(c) * 180.0 / 3.14159265358979;
}
V3 unitv(V3 a) {
  const double l = len(a);
  return l < 1e-12 ? V3{} : a * (1.0 / l);
}
/** Rodrigues: `v` carried by the MINIMAL rotation taking `from` to `to`. This is
 *  the parallel transport that makes a roll measurement mean something -- without
 *  it, "the spoke moved" and "the rod turned" are the same number, and an angle
 *  against any fixed reference folds at 0 and 180 (which is exactly how a first
 *  version of this leg read 88.9 deg/sample on a creature whose blade does not
 *  flip: the measure, not the rig). */
V3 rotate_min(V3 v, V3 from, V3 to) {
  const V3 a = unitv(from), b = unitv(to);
  const V3 ax = cross(a, b);
  const double sn = len(ax), cs = dot(a, b);
  if (sn < 1e-9) return cs >= 0 ? v : v * -1.0;
  const V3 k = ax * (1.0 / sn);
  const double th = std::atan2(sn, cs);
  return v * std::cos(th) + cross(k, v) * std::sin(th) +
         k * (dot(k, v) * (1.0 - std::cos(th)));
}
double signed_ang_about(V3 a, V3 b, V3 axis) {
  const V3 n = unitv(axis);
  const V3 pa = unitv(a - n * dot(a, n));
  const V3 pb = unitv(b - n * dot(b, n));
  if (len(pa) < 0.5 || len(pb) < 0.5) return 0.0;
  return std::atan2(dot(cross(pa, pb), n), dot(pa, pb)) * 180.0 /
         3.14159265358979;
}
constexpr double kFx = 1.0 / 65536.0;
V3 col(const zc::mat3x4fx& m, int j) {
  return {m.m[j] * kFx, m.m[4 + j] * kFx, m.m[8 + j] * kFx};
}
V3 to_root_dir(const zc::mat3x4fx& r, V3 w) {
  return {dot(col(r, 0), w), dot(col(r, 1), w), dot(col(r, 2), w)};
}
V3 to_root_pt(const zc::mat3x4fx& r, V3 w) {
  const V3 t{r.m[3] * kFx * 1000.0, r.m[7] * kFx * 1000.0, r.m[11] * kFx * 1000.0};
  return to_root_dir(r, w - t);
}

bool is_loop_bone(uint8_t b) {
  return b == u02::kBJunctionF || b == u02::kBNeck || b == u02::kBHingeA ||
         b == u02::kBHingeB || b == u02::kBHingeC || b == u02::kBHingeD ||
         b == u02::kBRearSocket || b == u02::kBReturnTip ||
         (b >= u02::kBSpanDeltaA && b <= u02::kBRearRootTurnMid);
}
bool is_loop_meshlet(const zc::Meshlet& m) {
  for (const zc::SkinVertex& v : m.verts)
    if (is_loop_bone(v.b0) || is_loop_bone(v.b1)) return true;
  return false;
}

// ---------------------------------------------------------------------------
// THE BAND ELEMENTS, one classification for both rigs.
//
// ⚠ IT IS DERIVED TWO DIFFERENT WAYS ON PURPOSE, and that is what makes the
// pass-20 control MEAN something. Under rods the roles are authored, so the
// classifier reads the table. Under pass20 there is no table -- the ladder is
// uniform stations with fold blends -- so the same elements are recovered from
// the STATIONS: a ball is the window [pivot - Ry, pivot + Ry] and a rod is the
// open run between two such windows. The two classifications name the same
// pieces of the same creature, which is the only way "the rod bends under
// pass20 and does not under rods" is a comparison rather than two measurements.
// ---------------------------------------------------------------------------
// R8's joint-step control: judge the same clips at HALF the sample rate.
bool g_skip_alternate_samples = false;

enum class Elem : uint8_t { kBase, kRod, kBall, kTail, kCone };
struct RingClass {
  Elem elem = Elem::kBase;
  int idx = -1;   // rod 0..3 / ball 0..3
  int k = -1;     // position within the element
  int32_t st = 0;
};

std::vector<RingClass> classify() {
  std::vector<RingClass> out(u02::kLoopRings);
  for (int i = 0; i < u02::kLoopRings; ++i) {
    const int32_t s = u02::loop_ring_station_at(i);
    RingClass rc;
    rc.st = s;
    if (u02::rig_rods()) {
      const u02::RodsRing rr = u02::rods_ring(i);
      switch (rr.role) {
        case u02::RingRole::kBase: rc.elem = Elem::kBase; break;
        case u02::RingRole::kRod:
          rc.elem = Elem::kRod;
          rc.idx = rr.elem;
          rc.k = rr.k;
          break;
        case u02::RingRole::kBall:
          rc.elem = rr.cone ? Elem::kCone : Elem::kBall;
          rc.idx = rr.elem;
          rc.k = rr.k;
          break;
        default: rc.elem = Elem::kTail; break;
      }
      out[i] = rc;
      continue;
    }
    // pass20: the same windows, read off the stations.
    rc.elem = Elem::kBase;
    if (s > u02::kRodsPivotFMm) {
      rc.elem = Elem::kTail;
      for (int e = 0; e < 4; ++e) {
        const int32_t p = u02::kRodsPivotMm[e + 1];
        const int32_t R = u02::kBallRyMm[e];
        if (s >= p - R && s <= p + R) {
          rc.elem = Elem::kBall;
          rc.idx = e;
          break;
        }
        const int32_t lo = e == 0 ? u02::kRodsPivotFMm
                                  : u02::kRodsPivotMm[e] + u02::kBallRyMm[e - 1];
        if (s > lo && s < p - R) {
          rc.elem = Elem::kRod;
          rc.idx = e;
          break;
        }
      }
    }
    out[i] = rc;
  }
  // number the rings within each element, in station order for pass20
  for (int e = 0; e < 4; ++e)
    for (Elem which : {Elem::kRod, Elem::kBall}) {
      int k = 0;
      for (int i = 0; i < u02::kLoopRings; ++i)
        if (out[i].elem == which && out[i].idx == e && out[i].k < 0)
          out[i].k = k++;
    }
  return out;
}

struct RingSeg {
  int ring = -1, seg = -1;
};
constexpr int kSeg = u02::kLoopSegments;

std::map<int32_t, int> ring_map() {
  std::map<int32_t, int> out;
  const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  for (int i = 0; i < u02::kLoopRings; ++i)
    out[u02::fxu(y0 + u02::loop_ring_station_at(i))] = i;
  return out;
}

std::map<std::array<int32_t, 3>, RingSeg> seg_index_map(const zc::CreatureType& T) {
  const auto rmap = ring_map();
  std::vector<std::vector<std::array<int32_t, 3>>> per_ring(u02::kLoopRings);
  for (const zc::Meshlet& m : T.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (const zc::SkinVertex& v : m.verts) {
      const auto it = rmap.find(v.y);
      if (it == rmap.end()) continue;
      const std::array<int32_t, 3> key{v.x, v.y, v.z};
      auto& b = per_ring[it->second];
      if (std::find(b.begin(), b.end(), key) == b.end()) b.push_back(key);
    }
  }
  std::map<std::array<int32_t, 3>, RingSeg> out;
  const double cx = static_cast<double>(u02::kLoopTubeXMm);
  for (int r = 0; r < u02::kLoopRings; ++r) {
    auto& b = per_ring[r];
    if (static_cast<int>(b.size()) != kSeg) continue;
    std::sort(b.begin(), b.end(), [&](const std::array<int32_t, 3>& a,
                                      const std::array<int32_t, 3>& c) {
      return std::atan2(a[2] * kFx * 1000.0, a[0] * kFx * 1000.0 - cx) <
             std::atan2(c[2] * kFx * 1000.0, c[0] * kFx * 1000.0 - cx);
    });
    for (int k = 0; k < kSeg; ++k) out[b[k]] = RingSeg{r, k};
  }
  return out;
}

// ---- one posed sample's band geometry -------------------------------------
struct Sample {
  std::vector<V3> cen;    // ring centroid, root-local mm
  std::vector<V3> nrm;    // ring plane normal
  std::vector<V3> seg0;   // the ring's segment-0 spoke: a MATERIAL direction,
                          // so its motion about the rod axis IS the blade roll
  std::vector<double> hoop;  // mean vertex distance from the centroid
  std::vector<bool> ok;
};

Sample read_sample(const zc::CreatureType& T, const zc::Clip& clip, int f,
                   uint8_t sub,
                   const std::map<std::array<int32_t, 3>, RingSeg>& smap) {
  std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
  zc::decode_pose(T, clip, static_cast<uint16_t>(f), pose, nullptr, sub);
  const zc::mat3x4fx& R = pose[u02::kBRoot];
  std::vector<std::array<V3, kSeg>> pos(u02::kLoopRings);
  std::vector<int> have(u02::kLoopRings, 0);
  for (const zc::Meshlet& m : T.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (const zc::SkinVertex& v : m.verts) {
      const auto it = smap.find(std::array<int32_t, 3>{v.x, v.y, v.z});
      if (it == smap.end()) continue;
      int32_t ox, oy, oz;
      zc::skin_vertex(pose.data(), v, ox, oy, oz, nullptr);
      pos[it->second.ring][it->second.seg] = to_root_pt(
          R, V3{ox * kFx * 1000.0, oy * kFx * 1000.0, oz * kFx * 1000.0});
      have[it->second.ring] |= 1 << it->second.seg;
    }
  }
  Sample s;
  s.cen.assign(u02::kLoopRings, V3{});
  s.nrm.assign(u02::kLoopRings, V3{});
  s.seg0.assign(u02::kLoopRings, V3{});
  s.hoop.assign(u02::kLoopRings, 0.0);
  s.ok.assign(u02::kLoopRings, false);
  const int full = (1 << kSeg) - 1;
  for (int i = 0; i < u02::kLoopRings; ++i) {
    if (have[i] != full) continue;
    V3 c{};
    for (int k = 0; k < kSeg; ++k) c = c + pos[i][k];
    s.cen[i] = c * (1.0 / kSeg);
    s.nrm[i] = cross(pos[i][0] - pos[i][kSeg / 2],
                     pos[i][kSeg / 4] - pos[i][3 * kSeg / 4]);
    s.seg0[i] = pos[i][0] - s.cen[i];
    double h = 0;
    for (int k = 0; k < kSeg; ++k) h += len(pos[i][k] - s.cen[i]);
    s.hoop[i] = h / kSeg;
    s.ok[i] = true;
  }
  return s;
}

// ---- the accumulated verdict ----------------------------------------------
struct Worst {
  double v = 0;
  std::string where;
  void take(double x, const std::string& w) {
    if (x > v) {
      v = x;
      where = w;
    }
  }
};
struct Low {
  double v = 1e30;
  std::string where;
  void take(double x, const std::string& w) {
    if (x < v) {
      v = x;
      where = w;
    }
  }
};

struct Tally {
  Worst rod_turn, rod_shear, rod_sag, rod_rate, joint_step, ball_rigid;
  Low rod_rail, rear_rail;
  double turn_in_rods = 0, turn_at_balls = 0;
  Worst joint_max[4];
  double joint_sum[4] = {0, 0, 0, 0};
  long joint_n = 0;
  Low rear_rod_len;
  Worst rear_rod_len_hi;
  // STAGE 4's QUESTION, measured before anything is changed. The blade is
  // elliptical (rx 44-58 against rz 20-30), so an aim's residual ROLL about the
  // rod is the flat side flipping. P21-ARCHITECTURE section 3.6 proposed moving
  // the five rod aims onto the roll-stable primitive; this is the measurement
  // that decides whether they need it. The quantity is the per-sample change in
  // the angle between the rod's own segment-0 spoke (a material direction) and
  // the LOOP PLANE, which is the reference the eye uses.
  Worst blade_roll_step;
  long samples = 0;
};

const char* kRodName[4] = {"F-A", "A-B", "B-C", "C-End"};
const char* kBallName[4] = {"A", "B", "C", "End"};

void measure(const zc::CreatureType& T, const zc::Clip& clip, int slot,
             const std::map<std::array<int32_t, 3>, RingSeg>& smap,
             const std::vector<RingClass>& cls, const Sample& bind,
             Tally& t, std::FILE* csv) {
  // per-element ring lists, in station order within the element
  std::vector<std::vector<int>> rod(4), ball(4);
  for (int i = 0; i < u02::kLoopRings; ++i) {
    if (cls[i].elem == Elem::kRod) rod[cls[i].idx].push_back(i);
    if (cls[i].elem == Elem::kBall) ball[cls[i].idx].push_back(i);
  }
  for (int e = 0; e < 4; ++e) {
    std::sort(rod[e].begin(), rod[e].end(),
              [&](int a, int b) { return cls[a].st < cls[b].st; });
    std::sort(ball[e].begin(), ball[e].end(),
              [&](int a, int b) { return cls[a].st < cls[b].st; });
  }

  std::vector<double> prev_turn(u02::kLoopRings, 0.0);
  std::vector<double> prev_joint(4, 0.0);
  std::vector<V3> prev_axis(4), prev_spoke(4);
  bool have_prev = false;

  for (int f = 0; f < clip.frame_count; ++f) {
    for (uint8_t sub = 0; sub <= 1; ++sub) {
      if (sub == 1 && clip.mid_quats.empty()) continue;
      if (g_skip_alternate_samples && sub == 1) continue;
      const Sample s = read_sample(T, clip, f, sub, smap);
      char at[160];
      std::snprintf(at, sizeof(at), "slot%d f%d.%u", slot, f, sub);
      std::vector<double> turn(u02::kLoopRings, 0.0);

      // ---- R6/R8/R9 per rod --------------------------------------------
      for (int e = 0; e < 4; ++e) {
        const std::vector<int>& r = rod[e];
        if (r.size() < 3) continue;
        bool all = true;
        for (int i : r) all = all && s.ok[i];
        if (!all) continue;
        for (size_t j = 1; j + 1 < r.size(); ++j) {
          const double tv = ang_deg(s.cen[r[j]] - s.cen[r[j - 1]],
                                    s.cen[r[j + 1]] - s.cen[r[j]]);
          turn[r[j]] = tv;
          char w[200];
          std::snprintf(w, sizeof(w), "%s rod %s ring %d (st %d)", at,
                        kRodName[e], r[j], cls[r[j]].st);
          t.rod_turn.take(tv, w);
          t.turn_in_rods += tv;
          double sh = ang_deg(s.nrm[r[j]], s.cen[r[j + 1]] - s.cen[r[j - 1]]);
          if (sh > 90.0) sh = 180.0 - sh;
          t.rod_shear.take(sh, w);
          if (have_prev) {
            const double rate = std::fabs(tv - prev_turn[r[j]]);
            t.rod_rate.take(rate, w);
          }
        }
        // the rod's own chord, and the worst perpendicular deviation from it:
        // a SMOOTH bow spreads its turn thin enough to pass a per-ring test.
        const V3 a = s.cen[r.front()], b = s.cen[r.back()];
        const V3 d = b - a;
        const double L = len(d);
        if (L > 1e-6) {
          for (size_t j = 1; j + 1 < r.size(); ++j) {
            const V3 p = s.cen[r[j]] - a;
            const double sag = len(cross(p, d)) / L;
            char w[200];
            std::snprintf(w, sizeof(w), "%s rod %s ring %d sag", at,
                          kRodName[e], r[j]);
            t.rod_sag.take(sag, w);
          }
        }
        // R9: the rail. Posed ring-to-ring spacing over BIND spacing; a
        // uniform stretch gives one ratio for the whole rod, a fold gives a
        // rigid block beside a squeezed gap.
        double lo = 1e30, hi = 0;
        for (size_t j = 0; j + 1 < r.size(); ++j) {
          const double bl = len(bind.cen[r[j + 1]] - bind.cen[r[j]]);
          if (bl < 1e-6) continue;
          const double ratio = len(s.cen[r[j + 1]] - s.cen[r[j]]) / bl;
          lo = std::min(lo, ratio);
          hi = std::max(hi, ratio);
        }
        if (hi > 1e-9 && lo < 1e29) {
          char w[200];
          std::snprintf(w, sizeof(w), "%s rod %s min/max rail", at, kRodName[e]);
          t.rod_rail.take(lo / hi, w);
          if (e == 3) {
            t.rear_rail.take(lo, w);
            t.rear_rod_len.take(L, w);
            t.rear_rod_len_hi.take(L, w);
          }
        }
      }

      // ---- BLADE ROLL, reported not gated -------------------------------
      // The rod's segment-0 spoke is a MATERIAL direction. Carry the previous
      // sample's spoke along with the rod's own rigid turn (parallel transport)
      // and the residual is the roll -- the flat side of the blade twisting,
      // which an elliptical section shows and a round one would not.
      for (int e = 0; e < 4; ++e) {
        if (rod[e].size() < 2) continue;
        const int m = rod[e][rod[e].size() / 2];
        if (!s.ok[m] || !s.ok[rod[e].front()] || !s.ok[rod[e].back()]) continue;
        const V3 ax = s.cen[rod[e].back()] - s.cen[rod[e].front()];
        if (len(ax) < 1e-6) continue;
        if (have_prev && len(prev_axis[e]) > 1e-6) {
          const V3 carried = rotate_min(prev_spoke[e], prev_axis[e], ax);
          char w[200];
          std::snprintf(w, sizeof(w), "%s rod %s blade roll", at, kRodName[e]);
          t.blade_roll_step.take(std::fabs(signed_ang_about(carried, s.seg0[m], ax)), w);
        }
        prev_axis[e] = ax;
        prev_spoke[e] = s.seg0[m];
      }

      // ---- R7/R8 the joints ---------------------------------------------
      // The joint angle at ball e is the angle between the chord of the rod
      // arriving and the chord of the rod leaving. That is the articulation the
      // eye sees, and it is measured on the SURFACE, not on a bone.
      for (int e = 0; e < 4; ++e) {
        const std::vector<int>& in = rod[e];
        if (in.size() < 2) continue;
        V3 d_in = s.cen[in.back()] - s.cen[in.front()];
        V3 d_out{};
        if (e < 3) {
          const std::vector<int>& ou = rod[e + 1];
          if (ou.size() < 2) continue;
          d_out = s.cen[ou.back()] - s.cen[ou.front()];
        } else {
          // the End joint: the rear rod against the buried tail's own run
          std::vector<int> tail;
          for (int i = 0; i < u02::kLoopRings; ++i)
            if (cls[i].elem == Elem::kTail && s.ok[i]) tail.push_back(i);
          std::sort(tail.begin(), tail.end(),
                    [&](int a, int b) { return cls[a].st < cls[b].st; });
          if (tail.size() < 2) continue;
          d_out = s.cen[tail.back()] - s.cen[tail.front()];
        }
        const double ja = ang_deg(d_in, d_out);
        char w[200];
        std::snprintf(w, sizeof(w), "%s joint %s", at, kBallName[e]);
        t.joint_max[e].take(ja, w);
        t.joint_sum[e] += ja;
        t.turn_at_balls += ja;
        if (have_prev) t.joint_step.take(std::fabs(ja - prev_joint[e]), w);
        prev_joint[e] = ja;
      }
      ++t.joint_n;

      // ---- R10 the balls are rigid --------------------------------------
      for (int e = 0; e < 4; ++e) {
        const std::vector<int>& b = ball[e];
        if (b.size() < 3) continue;
        const int mid = b[b.size() / 2];
        if (!s.ok[mid] || !bind.ok[mid]) continue;
        for (int i : b) {
          if (!s.ok[i] || !bind.ok[i]) continue;
          const double bd = len(bind.cen[i] - bind.cen[mid]);
          const double pd = len(s.cen[i] - s.cen[mid]);
          char w[200];
          std::snprintf(w, sizeof(w), "%s ball %s ring %d", at, kBallName[e], i);
          if (bd > 1.0) t.ball_rigid.take(std::fabs(pd / bd - 1.0), w);
          if (bind.hoop[i] > 1.0)
            t.ball_rigid.take(std::fabs(s.hoop[i] / bind.hoop[i] - 1.0), w);
        }
      }

      if (csv != nullptr) {
        std::fprintf(csv, "%d,%d,%u", slot, f, sub);
        for (int i = 0; i < u02::kLoopRings; ++i)
          std::fprintf(csv, ",%.2f", turn[i]);
        for (int i = 0; i < u02::kLoopRings; ++i)
          std::fprintf(csv, ",%.1f,%.1f,%.1f", s.cen[i].x, s.cen[i].y,
                       s.cen[i].z);
        std::fprintf(csv, "\n");
      }
      prev_turn = turn;
      have_prev = true;
      ++t.samples;
    }
  }
}

Sample bind_sample(const zc::CreatureType& T,
                   const std::map<std::array<int32_t, 3>, RingSeg>& smap) {
  // The BIND band, read through the same path with an identity pose, so the
  // rest reference is the same measurement rather than a second derivation of
  // the ring table (which is how a stale copy gets into an instrument).
  std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
  for (int b = 0; b < zc::kMaxBones; ++b) {
    zc::mat3x4fx m{};
    m.m[0] = m.m[5] = m.m[10] = 65536;
    pose[b] = m;
  }
  Sample s;
  std::vector<std::array<V3, kSeg>> pos(u02::kLoopRings);
  std::vector<int> have(u02::kLoopRings, 0);
  for (const zc::Meshlet& m : T.mesh) {
    if (!is_loop_meshlet(m)) continue;
    for (const zc::SkinVertex& v : m.verts) {
      const auto it = smap.find(std::array<int32_t, 3>{v.x, v.y, v.z});
      if (it == smap.end()) continue;
      pos[it->second.ring][it->second.seg] =
          V3{v.x * kFx * 1000.0, v.y * kFx * 1000.0, v.z * kFx * 1000.0};
      have[it->second.ring] |= 1 << it->second.seg;
    }
  }
  s.cen.assign(u02::kLoopRings, V3{});
  s.nrm.assign(u02::kLoopRings, V3{});
  s.seg0.assign(u02::kLoopRings, V3{});
  s.hoop.assign(u02::kLoopRings, 0.0);
  s.ok.assign(u02::kLoopRings, false);
  const int full = (1 << kSeg) - 1;
  for (int i = 0; i < u02::kLoopRings; ++i) {
    if (have[i] != full) continue;
    V3 c{};
    for (int k = 0; k < kSeg; ++k) c = c + pos[i][k];
    s.cen[i] = c * (1.0 / kSeg);
    s.seg0[i] = pos[i][0] - s.cen[i];
    double h = 0;
    for (int k = 0; k < kSeg; ++k) h += len(pos[i][k] - s.cen[i]);
    s.hoop[i] = h / kSeg;
    s.ok[i] = true;
  }
  return s;
}

// ---- thresholds: named, and the same ones P21-ARCHITECTURE section 6 names --
constexpr double kRodTurnMaxDeg = 1.5;    // the 6-bit weight quantisation ceiling
constexpr double kRodShearMaxDeg = 5.0;
constexpr double kRodSagMaxMm = 4.0;      // 2.5 mm quantisation + integer skin
constexpr double kRodRateMaxDeg = 8.0;
constexpr double kJointStepMaxDeg = 8.0;  // kAntennaMaxAngularStepDeg, unmoved
constexpr double kRodRailUniformPm = 850;
constexpr double kBallRigidTol = 0.01;    // 1 %; integer skinning is ~0.0005
constexpr double kJointShareMinPm = 980;

// ---- the FOUR controls, and the legs each one is DECLARED to break ---------
// Bit i is legs[i] below, in order:
//   0 R6 rod turn   1 R6 rod shear   2 R6 rod sag        3 R7 joint share
//   4 R8 joint step 5 R8 rod rate    6 R9 rod uniform    7 R10 ball rigid
enum class Ctl { kNone, kRigPass20, kBallBlend, kJointStep, kRodTwist };
struct CtlDecl {
  Ctl ctl;
  const char* flag;
  unsigned expect;  // the EXACT failure set this control must produce
  const char* why;
};
// ⚠ THESE MASKS ARE MEASUREMENTS, WRITTEN DOWN. They were read off the whole
// bank at the pass-21 close and are asserted from here on, so a later change
// that broadens a control into a blanket mutant (or narrows one into silence)
// is a red gate rather than a paragraph nobody re-derives. An `expect` of 0xFF
// is an HONEST declaration that the control is blanket, not a targeted probe.
constexpr CtlDecl kCtlTable[] = {
    {Ctl::kRigPass20, "--fail-rig-pass20", 0xFF,
     "the pass-20 rig: corners one to three rings BEFORE each ball, so every "
     "leg fails. Blanket by nature -- it is the whole wrong ladder."},
    {Ctl::kRodTwist, "--fail-rod-twist", 0x4F,
     "a rotation planted on the rear rod's middle helper: the two bones of one "
     "segment disagree, so THAT rod bends inside the rods rig. R6's three legs "
     "(the bend itself), R7 (articulation has left the balls) and R9 (the bend "
     "makes the rod's rails non-uniform) -- it reaches R9 from INSIDE the rods "
     "rig, which the rig swap cannot claim to do. It does NOT reach R8's joint "
     "step or R10, and that silence is part of the declaration: a planted static "
     "rotation is smooth in time and leaves the balls rigid."},
    {Ctl::kBallBlend, "--fail-ball-blend", 0x80,
     "an LBS blend across a ball's equator: only R10 can see it."},
    {Ctl::kJointStep, "--fail-joint-step", 0x10,
     "the same clips judged at half the sample rate: only R8's joint step."},
};

}  // namespace

int main(int argc, char** argv) {
  bool gate = false, report = false;
  const char* csv_path = nullptr;
  std::vector<int> slots;
  Ctl ctl = Ctl::kNone;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--gate") gate = true;
    else if (a == "--report") report = true;
    else if (a == "--csv" && i + 1 < argc) csv_path = argv[++i];
    else if (a == "--fail-rig-pass20") ctl = Ctl::kRigPass20;
    else if (a == "--fail-ball-blend") ctl = Ctl::kBallBlend;
    else if (a == "--fail-joint-step") ctl = Ctl::kJointStep;
    else if (a == "--fail-rod-twist") ctl = Ctl::kRodTwist;
    // ⚠ THE THREE RETIRED ALIASES FAIL LOUDLY. They were one configuration --
    // the pass-20 rig swap -- under three names that a report then quoted as
    // three independent controls. A silent alias would let that happen again.
    else if (a == "--fail-rod-bend" || a == "--fail-rod-uniform" ||
             a == "--fail-rod-flicker") {
      std::fprintf(stderr,
                   "%s was one of THREE NAMES for the single pass-20 rig swap "
                   "and is retired (pass-21 close). Use --fail-rig-pass20.\n",
                   a.c_str());
      return 2;
    } else if (!a.empty() && (a[0] == '-' )) {
      std::fprintf(stderr, "unknown flag %s\n", a.c_str());
      return 2;
    } else slots.push_back(std::atoi(a.c_str()));
  }
  if (!gate && !report && csv_path == nullptr) {
    std::fprintf(stderr,
                 "usage: %s (--gate | --report | --csv <out.csv>) [slot ...]\n"
                 "  with no slot argument the WHOLE BANK is measured\n"
                 "controls: --fail-rig-pass20 --fail-rod-twist "
                 "--fail-ball-blend --fail-joint-step\n",
                 argv[0]);
    return 2;
  }
  if (!u02::apply_knead_dip_env()) return 2;
  // The controls are CONFIGURATIONS, applied before the static creature is
  // built. Each one is a real rig the tree can render, not a patched number.
  if (ctl == Ctl::kRigPass20) u02::g_u02_rig = u02::RigMode::kPass20;
  if (ctl == Ctl::kBallBlend) u02::g_u02_rod_ball_blend_control = true;
  // R6's RODS-NATIVE control. The pass-20 rig fires R6 hard, but it fires it by
  // being a different ladder; this one keeps the rods ladder and plants a
  // rotation on the rear rod's middle helper, so the two bones of one segment
  // disagree and THAT rod bends. It proves R6 sees a broken rod inside its own
  // rig rather than only recognising the old one.
  if (ctl == Ctl::kRodTwist) u02::g_u02_rods_helper_twist_a16 = 2000;
  // R8's JOINT-STEP control. The step ceiling is a property of the AUTHORED
  // MOTION, so no rig configuration can breach it -- the pass-20 rig reads 5.44
  // deg/sample against an 8 deg ceiling, i.e. running the gate on the broken
  // creature does NOT fire this leg, and quoting its silence would then be
  // quoting an unproven instrument. The control halves the sample rate instead:
  // the same clips judged at 30 Hz must breach 8 deg, which proves the leg reads
  // the real per-sample joint change and would see a motion twice as fast.
  g_skip_alternate_samples = ctl == Ctl::kJointStep;
  u02::print_judged_config("mrod");

  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::fprintf(stderr, "compile produced no meshlets\n");
    return 1;
  }
  // ⚠ THE DEFAULT IS THE WHOLE BANK (pass-21 close; see the header note). It is
  // enumerated from T.bank.clips, the way mrear's gate always did, so a clip
  // ADDED to the bank is measured without anyone remembering to extend a list.
  // A hard-coded slot list is a coverage claim frozen at the moment it was
  // typed; `T.bank.clips` is the coverage claim the bank itself makes.
  if (slots.empty())
    for (const zc::Clip& c : T.bank.clips) slots.push_back(c.slot_id);

  const auto smap = seg_index_map(T);
  const auto cls = classify();
  const Sample bind = bind_sample(T, smap);

  std::FILE* csv = nullptr;
  if (csv_path != nullptr) {
    csv = std::fopen(csv_path, "w");
    if (csv == nullptr) return 1;
    std::fprintf(csv, "slot,frame,sub");
    for (int i = 0; i < u02::kLoopRings; ++i) std::fprintf(csv, ",turn%d", i);
    for (int i = 0; i < u02::kLoopRings; ++i)
      std::fprintf(csv, ",cx%d,cy%d,cz%d", i, i, i);
    std::fprintf(csv, "\n");
  }

  Tally t;
  int found = 0;
  for (int want : slots) {
    const zc::Clip* clip = nullptr;
    for (const zc::Clip& c : T.bank.clips)
      if (c.slot_id == want) clip = &c;
    if (clip == nullptr) {
      std::printf("slot %d absent\n", want);
      continue;
    }
    ++found;
    measure(T, *clip, want, smap, cls, bind, t, csv);
  }
  if (csv != nullptr) std::fclose(csv);
  if (found == 0) {
    std::fprintf(stderr, "no slot measured\n");
    return 1;
  }

  // ---- the element map, printed, so a stale ring table is visible ---------
  std::printf("RIG %s  rings %d  samples %ld\n",
              u02::rig_rods() ? "rods" : "pass20", u02::kLoopRings, t.samples);
  for (int e = 0; e < 4; ++e) {
    int nr = 0, nb = 0;
    int32_t r0 = 1 << 30, r1 = -1;
    for (int i = 0; i < u02::kLoopRings; ++i) {
      if (cls[i].elem == Elem::kRod && cls[i].idx == e) {
        ++nr;
        r0 = std::min(r0, cls[i].st);
        r1 = std::max(r1, cls[i].st);
      }
      if (cls[i].elem == Elem::kBall && cls[i].idx == e) ++nb;
    }
    std::printf("  rod %-5s %2d rings st %4d..%4d | ball %-3s %d rings R %d/%d\n",
                kRodName[e], nr, r0, r1, kBallName[e], nb, u02::kBallRxMm[e],
                u02::kBallRzMm[e]);
  }

  const double share_pm =
      (t.turn_in_rods + t.turn_at_balls) > 1e-9
          ? 1000.0 * t.turn_at_balls / (t.turn_in_rods + t.turn_at_balls)
          : 1000.0;
  std::printf(
      "R6  ROD STRAIGHT   worst turn %6.2f deg (<= %.2f)  [%s]\n"
      "                   worst shear %6.2f deg (<= %.1f)  [%s]\n"
      "                   worst sag  %7.2f mm  (<= %.1f)  [%s]\n"
      "R7  JOINT ON BALL  articulation at the balls %6.1f pm (>= %.0f)"
      " (rods %.1f deg, balls %.1f deg)\n"
      "R8  JOINT SMOOTH   worst joint step %6.2f deg (<= %.1f)  [%s]\n"
      "                   worst rod turn rate %6.2f deg (<= %.1f)  [%s]\n"
      "R9  ROD UNIFORM    worst min/max rail %6.1f pm (>= %.0f)  [%s]\n"
      "R10 BALL RIGID     worst strain %8.5f (<= %.4f)  [%s]\n",
      t.rod_turn.v, kRodTurnMaxDeg, t.rod_turn.where.c_str(), t.rod_shear.v,
      kRodShearMaxDeg, t.rod_shear.where.c_str(), t.rod_sag.v, kRodSagMaxMm,
      t.rod_sag.where.c_str(), share_pm, kJointShareMinPm, t.turn_in_rods,
      t.turn_at_balls, t.joint_step.v, kJointStepMaxDeg,
      t.joint_step.where.c_str(), t.rod_rate.v, kRodRateMaxDeg,
      t.rod_rate.where.c_str(), 1000.0 * t.rod_rail.v, kRodRailUniformPm,
      t.rod_rail.where.c_str(), t.ball_rigid.v, kBallRigidTol,
      t.ball_rigid.where.c_str());

  // ---- INFO, not gated: the crotch floor and the rear rod's length ---------
  std::printf("INFO blade roll worst step %6.2f deg/sample  [%s]\n",
              t.blade_roll_step.v, t.blade_roll_step.where.c_str());
  std::printf("INFO rear rod posed length %.0f..%.0f mm (rest %d), worst rear "
              "rail floor %.3f\n",
              t.rear_rod_len.v, t.rear_rod_len_hi.v,
              u02::kRodsPivotEndMm - u02::kRodsPivotCMm, t.rear_rail.v);
  for (int e = 0; e < 4; ++e) {
    const double th = t.joint_max[e].v;
    const double c = std::cos(th * 0.5 * 3.14159265358979 / 180.0);
    const int32_t rod_r = e < 3 ? u02::kLoopBladeRxMm[2 + e] : 49;
    std::printf("INFO ball %-3s joint max %6.1f deg mean %5.1f | crotch floor "
                "r/cos(th/2) = %5.0f mm vs shipping R %d\n",
                kBallName[e], th, t.joint_n > 0 ? t.joint_sum[e] / t.joint_n : 0.0,
                c > 0.01 ? rod_r / c : 9999.0, u02::kBallRxMm[e]);
  }

  if (!gate) return 0;

  struct Leg { const char* name; bool pass; };
  const Leg legs[] = {
      {"R6 rod turn", t.rod_turn.v <= kRodTurnMaxDeg},
      {"R6 rod shear", t.rod_shear.v <= kRodShearMaxDeg},
      {"R6 rod sag", t.rod_sag.v <= kRodSagMaxMm},
      {"R7 joint share", share_pm >= kJointShareMinPm},
      {"R8 joint step", t.joint_step.v <= kJointStepMaxDeg},
      {"R8 rod turn rate", t.rod_rate.v <= kRodRateMaxDeg},
      {"R9 rod uniform", 1000.0 * t.rod_rail.v >= kRodRailUniformPm},
      {"R10 ball rigid", t.ball_rigid.v <= kBallRigidTol},
  };
  int bad = 0;
  unsigned observed = 0;
  for (size_t li = 0; li < sizeof(legs) / sizeof(legs[0]); ++li) {
    std::printf("%-20s %s\n", legs[li].name, legs[li].pass ? "OK" : "FAIL");
    if (!legs[li].pass) {
      ++bad;
      observed |= 1u << li;
    }
  }
  // ⚠ A CONTROL RUN INVERTS THE POLARITY. The control exists to prove the
  // instrument can SEE the wrong creature, so it passes when a leg FAILS. A
  // control that comes back clean is a broken detector, not a good result.
  //
  // ⚠ AND "SOMETHING FAILED" IS NOT ATTRIBUTION. Pass 21 shipped this gate with
  // only the count, which is why three names for one blanket mutant read as
  // three targeted controls: every one of them failed eight legs and the count
  // could not say so. The declared mask is compared exactly -- a control that
  // fires the WRONG detector is UNATTRIBUTED and non-zero, same as one that does
  // not fire at all.
  if (ctl != Ctl::kNone) {
    const CtlDecl* d = nullptr;
    for (const CtlDecl& c : kCtlTable)
      if (c.ctl == ctl) d = &c;
    const bool attributed = d != nullptr && observed == d->expect;
    std::printf("CONTROL %s: %d leg(s) failed -- the control %s\n"
                "  expected=0x%02X observed=0x%02X -> %s\n"
                "  declared cause: %s\n",
                d != nullptr ? d->flag : "?", bad,
                bad > 0 ? "FIRED (instrument proven)" : "DID NOT FIRE (BROKEN)",
                d != nullptr ? d->expect : 0u, observed,
                attributed ? "ATTRIBUTED" : "UNATTRIBUTED",
                d != nullptr ? d->why : "?");
    return attributed ? 0 : 6;
  }
  std::printf("mrod: %s\n", bad == 0 ? "ALL LEGS OK" : "FAIL");
  return bad == 0 ? 0 : 5;
}
