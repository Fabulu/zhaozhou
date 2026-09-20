// manafold_p21_curvature.cpp -- PASS 21 ARCHITECT PROBE: WHERE DOES THE POSED
// BAND ACTUALLY BEND, and how fast does each place change?
//
// Owner (Direction 22): "Too many joints. Joints should be on the balls. End
// part is spazzing. Whole thing should be smooth." The eye has already said
// there are extra hinges; this instrument exists to LOCATE them along the band,
// not to decide whether they are there (CLAUDE.md: measurement on the
// comparison side).
//
// It reads the POSED SKIN through zc::decode_pose / zc::skin_vertex on every
// key and baked midpoint (60 Hz), never a rendered frame. Per ring i of the
// loop chain it reports, root-local:
//
//   turn_i   the turning angle of the ring-centroid centreline at ring i
//            (angle between c_i - c_{i-1} and c_{i+1} - c_i), degrees. A
//            smooth arc spreads its total turn evenly over its rings; a hinge
//            piles it into one or two rings. Summed over a run it is the run's
//            total bend; its distribution along the run is the thing asked.
//   shear_i  the angle between the ring's PLANE NORMAL and the centreline
//            tangent through it, degrees. A tube posed by rotation has rings
//            perpendicular to its axis (shear ~ 0); a tube whose centreline
//            is moved by TRANSLATED copies of one frame (the rear helpers)
//            or by an LBS blend of two frames keeps its rings parallel to the
//            old frame while the centreline curves -- the ring planes lean
//            off the tangent. That is a shear the eye reads as a crease or a
//            flat facet, and no centroid metric can see it.
//   rate_i   |turn_i(sample) - turn_i(previous sample)|, deg per 60 Hz
//            sample: how fast the bend AT THAT PLACE is changing. A joint that
//            spazzes is a ring whose turn moves fast, not one whose turn is
//            large.
//
// Output: one CSV row per (slot, frame, sub) with 64 turn values then 64
// shear values; the plotting script makes ring x time heat maps and the
// per-ring max/mean tables. A committed probe, so the numbers are
// reproducible (CLAUDE.md, "commit the probe"). Diagnostic only; it chooses no
// value and gates nothing.
//
// Build (from the zhaozhou root, with the direct-build objects present):
//   g++ -O2 -std=c++17 -Ireference/include -Iruntime/include -Itests/render
//       -Icompiler/tests/generated -Ireference/src
//       runs/CLAUDE-RUNS/RUN-20260920-2315-manafold-pass21/P21-PROBES/manafold_p21_curvature.cpp
//       .tmp/<build>/obj/*.o -o .tmp/<build>/bin/manafold-p21-curvature.exe
// Usage: manafold-p21-curvature.exe <out.csv> [slot ...]   (default 0 2 11)

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

struct RingSeg {
  int ring = -1, seg = -1;
};
constexpr int kSeg = u02::kLoopSegments;

// (bind x,y,z) -> (ring, segment by cross-section angle), as mrear does it.
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

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <out.csv> [slot ...]\n", argv[0]);
    return 2;
  }
  std::vector<int> slots;
  for (int i = 2; i < argc; ++i) slots.push_back(std::atoi(argv[i]));
  if (slots.empty()) slots = {0, 2, 11};
  if (!u02::apply_knead_dip_env()) return 2;
  // The renderer's rear selectors, so the probe can attribute a stripe to a
  // mechanism by switching it (diagnostic; the same strict parse mrear uses).
  if (const char* e = std::getenv("ZHAO_U02_REAR_BOW")) {
    if (std::strcmp(e, "arc") == 0) u02::g_u02_rear_bow = u02::RearBow::kArc;
    else if (std::strcmp(e, "legacy") == 0) u02::g_u02_rear_bow = u02::RearBow::kLegacy;
    else return 2;
  }
  if (const char* e = std::getenv("ZHAO_U02_REAR_AMBIENT_GAIN_PM"))
    u02::g_u02_rear_ambient_gain_pm = std::atoi(e);
  if (const char* e = std::getenv("ZHAO_U02_REAR_CARRIER_CALM_PM"))
    u02::g_u02_rear_carrier_calm_pm = std::atoi(e);
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::fprintf(stderr, "compile produced no meshlets\n");
    return 1;
  }
  std::FILE* out = std::fopen(argv[1], "w");
  if (!out) return 1;
  std::fprintf(out, "slot,frame,sub");
  for (int i = 0; i < u02::kLoopRings; ++i) std::fprintf(out, ",turn%d", i);
  for (int i = 0; i < u02::kLoopRings; ++i) std::fprintf(out, ",shear%d", i);
  for (int i = 0; i < u02::kLoopRings; ++i) std::fprintf(out, ",cx%d,cy%d,cz%d", i, i, i);
  std::fprintf(out, "\n");
  const auto smap = seg_index_map(T);

  for (int want : slots) {
    const zc::Clip* clip = nullptr;
    for (const zc::Clip& c : T.bank.clips)
      if (c.slot_id == want) clip = &c;
    if (!clip) {
      std::printf("slot %d absent\n", want);
      continue;
    }
    int samples = 0;
    for (int f = 0; f < clip->frame_count; ++f) {
      for (uint8_t sub = 0; sub <= 1; ++sub) {
        if (sub == 1 && clip->mid_quats.empty()) continue;
        std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
        zc::decode_pose(T, *clip, static_cast<uint16_t>(f), pose, nullptr, sub);
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
        const int full = (1 << kSeg) - 1;
        std::vector<V3> cen(u02::kLoopRings), nrm(u02::kLoopRings);
        std::vector<bool> ok(u02::kLoopRings, false);
        for (int i = 0; i < u02::kLoopRings; ++i) {
          if (have[i] != full) continue;
          V3 c{};
          for (int k = 0; k < kSeg; ++k) c = c + pos[i][k];
          cen[i] = c * (1.0 / kSeg);
          // ring plane normal from two diameters
          nrm[i] = cross(pos[i][0] - pos[i][kSeg / 2],
                         pos[i][kSeg / 4] - pos[i][3 * kSeg / 4]);
          ok[i] = true;
        }
        std::vector<double> turn(u02::kLoopRings, 0.0), shear(u02::kLoopRings, 0.0);
        for (int i = 1; i + 1 < u02::kLoopRings; ++i) {
          if (!ok[i - 1] || !ok[i] || !ok[i + 1]) continue;
          turn[i] = ang_deg(cen[i] - cen[i - 1], cen[i + 1] - cen[i]);
          double s = ang_deg(nrm[i], cen[i + 1] - cen[i - 1]);
          if (s > 90.0) s = 180.0 - s;
          shear[i] = s;
        }
        std::fprintf(out, "%d,%d,%d", want, f, sub);
        for (int i = 0; i < u02::kLoopRings; ++i) std::fprintf(out, ",%.2f", turn[i]);
        for (int i = 0; i < u02::kLoopRings; ++i) std::fprintf(out, ",%.2f", shear[i]);
        for (int i = 0; i < u02::kLoopRings; ++i)
          std::fprintf(out, ",%.1f,%.1f,%.1f", cen[i].x, cen[i].y, cen[i].z);
        std::fprintf(out, "\n");
        ++samples;
      }
    }
    std::printf("slot %d: %d samples\n", want, samples);
  }
  std::fclose(out);
  return 0;
}
