// zixx_probe — committed 3D pose/contact/continuity probe for Zixxtrixx.
//
// A rendered frame cannot distinguish terrain in front of the animal from a
// vertex inside it.  This tool decodes the actual 60 Hz presentation timeline
// (every authored key and every runtime-equivalent midpoint), skins every mesh
// vertex once per sample, and applies declared per-clip contact policy.  It also
// checks non-neighbouring station overlap, the shared whole-body spring, the
// programmable jumps, and the nine-salto fixed-point limit case.
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) /
                              1000);
}
#include "zixxtrixx.h"

namespace {

int32_t to_mm(int64_t fx) { return static_cast<int32_t>(fx * 1000 >> 16); }

constexpr int32_t kSpringTrunkLateralSpanMaxMm = 30;
// Re-recorded for OWNER DIRECTION 24: the tail is PLANTED at its grounded
// headings through the whole arming, so the deep pose now carries the rest
// tail's own intentional construction roll (the fin assembly's authored Z
// offsets), which the old curled-tail deep pose rotated mostly out of this
// axis. The trunk gate above is unchanged -- the body itself stays planar.
constexpr int32_t kSpringWholeTailLateralSpanMaxMm = 55;
// Retiming samples the same named landing curve at new quantized body poses.
// One millimetre bounds fixed-point/pose-clock rounding without changing art.
constexpr int32_t kRetimedLandingContactRoundingMm = 1;

int presentation_samples(const zc::Clip& c) {
  return c.frame_count == 0 ? 0 : 2 * (static_cast<int>(c.frame_count) - 1) + 1;
}

struct Station {
  zc::SkinVertex v;
  int32_t r_mm;
};

std::vector<Station> make_stations() {
  std::vector<Station> out;
  out.reserve(zixx::kProfileStations);
  for (int i = 0; i < zixx::kProfileStations; ++i) {
    const zixx::Bind bd = i <= zixx::kHeadEnd
                              ? zixx::head_station_bind(i)
                              : zixx::station_bind(i);
    Station st;
    st.v = zc::SkinVertex{-fxm(zixx::station_x(i)), fxm(zixx::kBodyY), 0,
                          bd.b0, bd.b1, bd.w0, 0, 0};
    if (i <= zixx::kHeadEnd) {
      int32_t rx_mm = 0, rz_mm = 0;
      zixx::head_ring(i, rx_mm, rz_mm);
      st.r_mm = rz_mm;
    } else {
      st.r_mm = zixx::station_r(i);
    }
    out.push_back(st);
  }
  return out;
}

struct PosedSample {
  int tick = 0;  // 60 Hz presentation tick; even=authored key, odd=midpoint
  int32_t min_y_fx = INT32_MAX;
  int32_t max_y_fx = INT32_MIN;
  std::array<int32_t, 2> rung_min_y_fx{INT32_MAX, INT32_MAX};
  std::array<int32_t, 2> rung_max_y_fx{INT32_MIN, INT32_MIN};
  std::array<uint64_t, 2> normal_faults{};
  std::array<int32_t, 2> normal_min_len2{INT32_MAX, INT32_MAX};
  std::array<int32_t, 2> normal_max_len2{};
  int32_t blade_min_y_fx = INT32_MAX;
  int min_b0 = -1;
  int min_b1 = -1;
  std::array<int32_t, zixx::kProfileStations> x_mm{};
  std::array<int32_t, zixx::kProfileStations> y_mm{};
  std::array<int32_t, zixx::kProfileStations> z_mm{};
  int32_t support_x_mm = 0;
  int32_t support_y_mm = 0;
  int32_t support_z_mm = 0;
  std::array<int32_t, zc::kMaxBones> bone_min_y_fx{};
  uint64_t saturation = 0;
};

struct ClipScan {
  const zc::Clip* clip = nullptr;
  std::vector<PosedSample> samples;
  int32_t worst_min_fx = INT32_MAX;
  int32_t worst_max_fx = INT32_MIN;
  int worst_tick = -1;
  int worst_b0 = -1;
  int worst_b1 = -1;
  std::array<uint64_t, 2> normal_faults{};
  std::array<int32_t, 2> normal_min_len2{INT32_MAX, INT32_MAX};
  std::array<int32_t, 2> normal_max_len2{};
  uint64_t saturation = 0;
};

ClipScan scan_clip(const zc::CreatureType& type, const zc::Clip& clip,
                   const std::vector<Station>& stations) {
  ClipScan scan;
  scan.clip = &clip;
  const int ns = clip.slot_id == zixx::kSlotFall
                     ? 2 * static_cast<int>(clip.frame_count)
                     : presentation_samples(clip);
  scan.samples.reserve(ns);
  for (int tick = 0; tick < ns; ++tick) {
    PosedSample s;
    s.tick = tick;
    s.bone_min_y_fx.fill(INT32_MAX);
    const uint16_t key = static_cast<uint16_t>(tick / 2);
    const uint8_t sub = static_cast<uint8_t>(tick & 1);
    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zref::SatLedger ledger;
    zc::decode_pose(type, clip, key, pose, &ledger, sub);
    const zc::DeformSample deform =
        zc::deformation_sample(type, clip.slot_id, key, sub);
    for (int rung = 0; rung < 2; ++rung) {
      const auto& mesh = rung == 0 ? type.mesh : type.micro;
      for (const auto& meshlet : mesh) {
        for (size_t vi = 0; vi < meshlet.verts.size(); ++vi) {
          const zc::SkinVertex& bind = meshlet.verts[vi];
          zc::SkinVertex v = bind;
          if (!meshlet.deform.empty())
            v = zc::deform_skin_vertex(bind, meshlet.deform[vi], deform);
          int32_t x = 0, y = 0, z = 0;
          zc::skin_vertex(pose.data(), v, x, y, z, &ledger);
          s.rung_min_y_fx[rung] = std::min(s.rung_min_y_fx[rung], y);
          s.rung_max_y_fx[rung] = std::max(s.rung_max_y_fx[rung], y);
          // Keep actual LOD0 skinned-vertex minima per influencing bone. Balance
          // uses these to prove several body segments, rather than only blade
          // tips, share the authored terrain support.
          if (rung == 0) {
            if (v.b0 < zc::kMaxBones && v.w0 != 0)
              s.bone_min_y_fx[v.b0] = std::min(s.bone_min_y_fx[v.b0], y);
            if (v.b1 < zc::kMaxBones && v.w0 != 255)
              s.bone_min_y_fx[v.b1] = std::min(s.bone_min_y_fx[v.b1], y);
            if (v.b0 >= zixx::kBBladeL && v.b0 <= zixx::kBBladeR2)
              s.blade_min_y_fx = std::min(s.blade_min_y_fx, y);
          }
          if (y < s.min_y_fx) {
            s.min_y_fx = y;
            s.min_b0 = v.b0;
            s.min_b1 = v.b1;
          }
          s.max_y_fx = std::max(s.max_y_fx, y);

          if (bind.nx != 0 || bind.ny != 0 || bind.nz != 0) {
            const int32_t n2 = static_cast<int32_t>(v.nx) * v.nx +
                               static_cast<int32_t>(v.ny) * v.ny +
                               static_cast<int32_t>(v.nz) * v.nz;
            s.normal_min_len2[rung] = std::min(s.normal_min_len2[rung], n2);
            s.normal_max_len2[rung] = std::max(s.normal_max_len2[rung], n2);
            const int32_t dx = zc::skin_normal_lambert(
                                   pose.data(), v, 65536, 0, 0) -
                               zc::skin_normal_lambert(
                                   pose.data(), v, -65536, 0, 0);
            const int32_t dy = zc::skin_normal_lambert(
                                   pose.data(), v, 0, 65536, 0) -
                               zc::skin_normal_lambert(
                                   pose.data(), v, 0, -65536, 0);
            const int32_t dz = zc::skin_normal_lambert(
                                   pose.data(), v, 0, 0, 65536) -
                               zc::skin_normal_lambert(
                                   pose.data(), v, 0, 0, -65536);
            if (n2 == 0 || (dx == 0 && dy == 0 && dz == 0))
              ++s.normal_faults[rung];
          }
        }
      }
    }
    for (int i = 0; i < zixx::kProfileStations; ++i) {
      int32_t x = 0, y = 0, z = 0;
      zc::skin_vertex(pose.data(), stations[i].v, x, y, z, &ledger);
      s.x_mm[i] = to_mm(x);
      s.y_mm[i] = to_mm(y);
      s.z_mm[i] = to_mm(z);
    }
    const uint8_t support_bone =
        static_cast<uint8_t>(zixx::kBSpine0 + zixx::kSpringPlantSegment);
    const zc::SkinVertex support{
        type.baked.world_x[support_bone], type.baked.world_y[support_bone],
        type.baked.world_z[support_bone], support_bone, support_bone, 64, 0, 0};
    int32_t support_x = 0, support_y = 0, support_z = 0;
    zc::skin_vertex(pose.data(), support, support_x, support_y, support_z,
                    &ledger);
    s.support_x_mm = to_mm(support_x);
    s.support_y_mm = to_mm(support_y);
    s.support_z_mm = to_mm(support_z);
    s.saturation = ledger.total();
    scan.saturation += s.saturation;
    for (int rung = 0; rung < 2; ++rung) {
      scan.normal_faults[rung] += s.normal_faults[rung];
      scan.normal_min_len2[rung] =
          std::min(scan.normal_min_len2[rung], s.normal_min_len2[rung]);
      scan.normal_max_len2[rung] =
          std::max(scan.normal_max_len2[rung], s.normal_max_len2[rung]);
    }
    if (s.min_y_fx < scan.worst_min_fx) {
      scan.worst_min_fx = s.min_y_fx;
      scan.worst_tick = tick;
      scan.worst_b0 = s.min_b0;
      scan.worst_b1 = s.min_b1;
    }
    scan.worst_max_fx = std::max(scan.worst_max_fx, s.max_y_fx);
    scan.samples.push_back(s);
  }
  return scan;
}

const ClipScan* find_scan(const std::vector<ClipScan>& scans, int slot) {
  for (const auto& s : scans)
    if (s.clip && s.clip->slot_id == slot) return &s;
  return nullptr;
}

int32_t overlap_allowance_mm(int slot) {
  switch (slot) {
    case 1: return 250;
    case 2: return 300;
    case 3: return 370;
    case 4: return 120;
    case 5: return 390;
    case 6: return 265;
    case 7: return 210;
    case 8: return 250;
    case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
      return 370;
    case 20: return 285;
    case 21: return 265;
    case 22: return 265;
    case 23: return 235;
    case 24: return 290;
    case 25: return 235;
    case 26: return 400;
    case 27: return 340;
    case 28: return 410;
    case 29: return 285;
    case 30: return 215;  // frozen legacy quick taunt
    case 31: return 265;
    case 32: return 190;
    case 33: case 34: case 35:
      return 370;  // attack-wheel family, visually accepted at its worst key
    case 36: return 390;
    case 37: return 275;
    case 38: return 250;
    case 39: return 240;
    case 40: return 240;
    case 41: return 215;
    case 42: return 245;
    case 43: return 125;
    case 46: case 47: case 48:
      // Task #12 uses the same clean, nose-to-tail wheel closure as the
      // accepted attack family.  Every-frame native sheets were judged before
      // declaring this shared ceiling; the probe may compare, never author it.
      return 370;
    default: return 0;
  }
}

bool key_pose_equal(const zc::Clip& c, int a, int b, int bones,
                    bool compare_root) {
  if (a < 0 || b < 0 || a >= c.frame_count || b >= c.frame_count) return false;
  if (compare_root &&
      std::memcmp(&c.root[static_cast<size_t>(a) * 3],
                  &c.root[static_cast<size_t>(b) * 3],
                  3 * sizeof(c.root[0])) != 0)
    return false;
  return std::memcmp(&c.quats[static_cast<size_t>(a) * bones],
                     &c.quats[static_cast<size_t>(b) * bones],
                     static_cast<size_t>(bones) * sizeof(c.quats[0])) == 0;
}

struct StationStepMaximum {
  int32_t mm = 0;
  int tick = -1;
  int station = -1;
};

StationStepMaximum station_step_max_mm(const ClipScan& scan, int begin_tick,
                                       int end_tick) {
  begin_tick = std::max(begin_tick, 1);
  end_tick = std::min(end_tick, static_cast<int>(scan.samples.size()) - 1);
  StationStepMaximum worst;
  for (int t = begin_tick; t <= end_tick; ++t) {
    const auto& a = scan.samples[t - 1];
    const auto& b = scan.samples[t];
    for (int i = 0; i < zixx::kProfileStations; ++i) {
      const int64_t dx = b.x_mm[i] - a.x_mm[i];
      const int64_t dy = b.y_mm[i] - a.y_mm[i];
      const int64_t dz = b.z_mm[i] - a.z_mm[i];
      const int32_t d = static_cast<int32_t>(zref::isqrt_u64(
          static_cast<uint64_t>(dx * dx + dy * dy + dz * dz)));
      if (d > worst.mm) {
        worst.mm = d;
        worst.tick = t;
        worst.station = i;
      }
    }
  }
  return worst;
}

struct RelativePeak {
  int32_t mm = 0;
  int tick = -1;
};

// Motion of one station relative to another removes the authored root shove.
// This compares the accepted animation; it does not derive any art value.
RelativePeak relative_peak_mm(const ClipScan& scan, int station, int anchor,
                              int begin_tick, int end_tick) {
  RelativePeak peak;
  if (scan.samples.empty()) return peak;
  begin_tick = std::max(begin_tick, 0);
  end_tick = std::min(end_tick, static_cast<int>(scan.samples.size()) - 1);
  const PosedSample& rest = scan.samples[0];
  const int32_t rx = rest.x_mm[station] - rest.x_mm[anchor];
  const int32_t ry = rest.y_mm[station] - rest.y_mm[anchor];
  const int32_t rz = rest.z_mm[station] - rest.z_mm[anchor];
  for (int t = begin_tick; t <= end_tick; ++t) {
    const PosedSample& s = scan.samples[t];
    const int64_t dx = (s.x_mm[station] - s.x_mm[anchor]) - rx;
    const int64_t dy = (s.y_mm[station] - s.y_mm[anchor]) - ry;
    const int64_t dz = (s.z_mm[station] - s.z_mm[anchor]) - rz;
    const int32_t d = static_cast<int32_t>(zref::isqrt_u64(
        static_cast<uint64_t>(dx * dx + dy * dy + dz * dz)));
    if (d > peak.mm) peak = {d, t};
  }
  return peak;
}

struct BowMaximum {
  int32_t mm = 0;
  int tick = -1;
  int station = -1;
};

// Maximum centreline distance from the chord joining the head and tail.  Slot
// 16 starts as a straight spear, so this is a direct comparison of the visible
// whole-spear bow after impact, independent of world translation or rotation.
BowMaximum chord_bow_max_mm(const ClipScan& scan) {
  BowMaximum worst;
  constexpr int a = 0;
  constexpr int b = zixx::kProfileStations - 1;
  for (const PosedSample& s : scan.samples) {
    const int64_t vx = s.x_mm[b] - s.x_mm[a];
    const int64_t vy = s.y_mm[b] - s.y_mm[a];
    const int64_t vz = s.z_mm[b] - s.z_mm[a];
    const uint64_t vv = static_cast<uint64_t>(vx * vx + vy * vy + vz * vz);
    const uint64_t vlen = zref::isqrt_u64(vv);
    if (vlen == 0) continue;
    for (int i = 1; i < b; ++i) {
      const int64_t wx = s.x_mm[i] - s.x_mm[a];
      const int64_t wy = s.y_mm[i] - s.y_mm[a];
      const int64_t wz = s.z_mm[i] - s.z_mm[a];
      const int64_t cx = wy * vz - wz * vy;
      const int64_t cy = wz * vx - wx * vz;
      const int64_t cz = wx * vy - wy * vx;
      const uint64_t cross = zref::isqrt_u64(
          static_cast<uint64_t>(cx * cx + cy * cy + cz * cz));
      const int32_t d = static_cast<int32_t>(cross / vlen);
      if (d > worst.mm) worst = {d, s.tick, i};
    }
  }
  return worst;
}

struct PosedRung {
  std::vector<std::vector<zc::SkinVertex>> deformed;
  std::vector<std::vector<std::array<int32_t, 3>>> xyz_fx;
};

PosedRung pose_rung(const zc::CreatureType& type, const zc::Clip& clip,
                    int rung, int tick) {
  const auto& mesh = rung == 0 ? type.mesh : type.micro;
  const uint16_t key = static_cast<uint16_t>(tick / 2);
  const uint8_t sub = static_cast<uint8_t>(tick & 1);
  const zc::DeformSample sample =
      zc::deformation_sample(type, clip.slot_id, key, sub);
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(type, clip, key, pose, nullptr, sub);
  PosedRung out;
  out.deformed.resize(mesh.size());
  out.xyz_fx.resize(mesh.size());
  for (size_t mi = 0; mi < mesh.size(); ++mi) {
    const zc::Meshlet& m = mesh[mi];
    out.deformed[mi].resize(m.verts.size());
    out.xyz_fx[mi].resize(m.verts.size());
    for (size_t vi = 0; vi < m.verts.size(); ++vi) {
      zc::SkinVertex v = m.verts[vi];
      if (!m.deform.empty())
        v = zc::deform_skin_vertex(v, m.deform[vi], sample);
      out.deformed[mi][vi] = v;
      zc::skin_vertex(pose.data(), v, out.xyz_fx[mi][vi][0],
                      out.xyz_fx[mi][vi][1], out.xyz_fx[mi][vi][2], nullptr);
    }
  }
  return out;
}

int station_for_center_x(int32_t center_x_fx) {
  const int32_t x_mm = to_mm(center_x_fx);
  int best = 0;
  int32_t best_error = INT32_MAX;
  for (int i = 0; i < zixx::kProfileStations; ++i) {
    const int32_t error = std::abs(x_mm + zixx::station_x(i));
    if (error < best_error) {
      best = i;
      best_error = error;
    }
  }
  return best;
}

struct Vec3d {
  double x = 0, y = 0, z = 0;
};

Vec3d mm_vec(const std::array<int32_t, 3>& p) {
  return Vec3d{static_cast<double>(to_mm(p[0])),
               static_cast<double>(to_mm(p[1])),
               static_cast<double>(to_mm(p[2]))};
}

Vec3d sub(Vec3d a, Vec3d b) {
  return Vec3d{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3d cross(Vec3d a, Vec3d b) {
  return Vec3d{a.y * b.z - a.z * b.y,
               a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x};
}

double dot(Vec3d a, Vec3d b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

bool segment_hits_triangle(Vec3d p0, Vec3d p1, Vec3d a, Vec3d b,
                           Vec3d c) {
  const Vec3d dir = sub(p1, p0);
  const Vec3d e1 = sub(b, a);
  const Vec3d e2 = sub(c, a);
  const Vec3d h = cross(dir, e2);
  const double det = dot(e1, h);
  constexpr double kEps = 1.0e-9;
  if (std::abs(det) < kEps) return false;
  const double inv = 1.0 / det;
  const Vec3d s = sub(p0, a);
  const double u = inv * dot(s, h);
  if (u < -kEps || u > 1.0 + kEps) return false;
  const Vec3d q = cross(s, e1);
  const double v = inv * dot(dir, q);
  if (v < -kEps || u + v > 1.0 + kEps) return false;
  const double t = inv * dot(e2, q);
  return t >= -kEps && t <= 1.0 + kEps;
}

bool triangles_intersect(const std::array<Vec3d, 3>& a,
                         const std::array<Vec3d, 3>& b) {
  const auto disjoint_axis = [](const std::array<Vec3d, 3>& x,
                                const std::array<Vec3d, 3>& y, int lane) {
    double xlo = 1.0e30, xhi = -1.0e30, ylo = 1.0e30, yhi = -1.0e30;
    for (int i = 0; i < 3; ++i) {
      const double xv = lane == 0 ? x[i].x : (lane == 1 ? x[i].y : x[i].z);
      const double yv = lane == 0 ? y[i].x : (lane == 1 ? y[i].y : y[i].z);
      xlo = std::min(xlo, xv);
      xhi = std::max(xhi, xv);
      ylo = std::min(ylo, yv);
      yhi = std::max(yhi, yv);
    }
    return xhi < ylo || yhi < xlo;
  };
  for (int lane = 0; lane < 3; ++lane)
    if (disjoint_axis(a, b, lane)) return false;
  for (int i = 0; i < 3; ++i) {
    if (segment_hits_triangle(a[i], a[(i + 1) % 3], b[0], b[1], b[2]))
      return true;
    if (segment_hits_triangle(b[i], b[(i + 1) % 3], a[0], a[1], a[2]))
      return true;
  }
  return false;
}

struct SurfaceTriangle {
  std::array<Vec3d, 3> p{};
  int station_lo = 0;
  int station_hi = 0;
};

std::vector<SurfaceTriangle> body_triangles(const zc::CreatureType& type,
                                            int rung,
                                            const PosedRung& posed) {
  const auto& mesh = rung == 0 ? type.mesh : type.micro;
  std::vector<SurfaceTriangle> out;
  for (size_t mi = 0; mi < mesh.size(); ++mi) {
    const zc::Meshlet& m = mesh[mi];
    if (m.page != zixx::kTileHead && m.page != zixx::kTileBody) continue;
    if (m.deform.empty()) continue;
    for (size_t ti = 0; ti + 2 < m.idx.size(); ti += 3) {
      SurfaceTriangle tri;
      tri.station_lo = zixx::kProfileStations;
      tri.station_hi = -1;
      bool radial = true;
      for (int k = 0; k < 3; ++k) {
        const uint8_t vi = m.idx[ti + static_cast<size_t>(k)];
        const zc::DeformVertex& d = m.deform[vi];
        radial = radial && d.role == zc::DeformRole::kRadial;
        const int station = station_for_center_x(d.center_x);
        tri.station_lo = std::min(tri.station_lo, station);
        tri.station_hi = std::max(tri.station_hi, station);
        tri.p[k] = mm_vec(posed.xyz_fx[mi][vi]);
      }
      if (radial) out.push_back(tri);
    }
  }
  return out;
}

struct IntersectionPeak {
  int count = 0;
  int tick = -1;
  int station_a = -1;
  int station_b = -1;
};

IntersectionPeak spring_self_intersections(const zc::CreatureType& type,
                                           const zc::Clip& clip, int rung,
                                           int end_tick) {
  IntersectionPeak worst;
  for (int tick = 0; tick <= end_tick; ++tick) {
    const PosedRung posed = pose_rung(type, clip, rung, tick);
    const std::vector<SurfaceTriangle> tris = body_triangles(type, rung, posed);
    int hits = 0;
    int hit_a = -1, hit_b = -1;
    for (size_t i = 0; i < tris.size(); ++i) {
      for (size_t j = i + 1; j < tris.size(); ++j) {
        const bool separated =
            tris[i].station_hi + 7 <= tris[j].station_lo ||
            tris[j].station_hi + 7 <= tris[i].station_lo;
        if (!separated) continue;
        if (!triangles_intersect(tris[i].p, tris[j].p)) continue;
        ++hits;
        if (hit_a < 0) {
          hit_a = tris[i].station_lo;
          hit_b = tris[j].station_lo;
        }
      }
    }
    if (hits > 0)
      std::printf("SPRING intersection sample %s %d%s: %d (%d/%d)\n",
                  rung == 0 ? "full" : "micro", tick / 2,
                  (tick & 1) ? ".5" : "", hits, hit_a, hit_b);
    if (hits > worst.count)
      worst = IntersectionPeak{hits, tick, hit_a, hit_b};
  }
  return worst;
}

struct TerrainWindow {
  int32_t worst_mm = INT32_MAX;
  int worst_tick = -1;
  int32_t shallowest_mm = INT32_MIN;
  int shallowest_tick = -1;
};

// A terrain declaration owns an inclusive presentation-tick interval. `worst`
// catches excess penetration; `shallowest` proves a supposedly grounded phase
// never loses all surface contact. Both are actual posed LOD0 vertices.
TerrainWindow terrain_window(const ClipScan& scan, int begin_tick, int end_tick) {
  TerrainWindow out;
  begin_tick = std::max(begin_tick, 0);
  end_tick = std::min(end_tick, static_cast<int>(scan.samples.size()) - 1);
  for (int tick = begin_tick; tick <= end_tick; ++tick) {
    const int32_t y = to_mm(scan.samples[tick].min_y_fx);
    if (y < out.worst_mm) {
      out.worst_mm = y;
      out.worst_tick = tick;
    }
    if (y > out.shallowest_mm) {
      out.shallowest_mm = y;
      out.shallowest_tick = tick;
    }
  }
  return out;
}

void print_terrain_window(const char* clip, const char* phase,
                          const TerrainWindow& w) {
  std::printf("CONTACT %s %s: deepest %d mm at %d%s; shallowest %d mm "
              "at %d%s\n",
              clip, phase, w.worst_mm, w.worst_tick / 2,
              (w.worst_tick & 1) ? ".5" : "", w.shallowest_mm,
              w.shallowest_tick / 2, (w.shallowest_tick & 1) ? ".5" : "");
}

}  // namespace

int main() {
  const zc::CreatureType& type = zixx::type();
  size_t nv = 0, nt = 0, mnt = 0;
  for (const auto& m : type.mesh) {
    nv += m.verts.size();
    nt += m.idx.size() / 3;
  }
  for (const auto& m : type.micro) mnt += m.idx.size() / 3;
  std::printf("bones=%d meshlets=%zu verts=%zu tris=%zu | micro: %zu meshlets "
              "%zu tris, compiler micro_error=%d\n",
              static_cast<int>(type.bank.bone_count), type.mesh.size(), nv, nt,
              type.micro.size(), mnt, static_cast<int>(type.micro_error));

  const std::vector<Station> stations = make_stations();
  std::vector<std::pair<int, int>> pairs;
  for (int i = 0; i < zixx::kProfileStations; ++i) {
    for (int j = i + 8; j < zixx::kProfileStations; ++j) {
      const int64_t bind_mm = zixx::station_x(j) - zixx::station_x(i);
      if (bind_mm * 100 >
          static_cast<int64_t>(stations[i].r_mm + stations[j].r_mm) * 115)
        pairs.push_back({i, j});
    }
  }

  std::vector<ClipScan> scans;
  scans.reserve(type.bank.clips.size());
  for (const zc::Clip& clip : type.bank.clips) {
    scans.push_back(scan_clip(type, clip, stations));
    const ClipScan& s = scans.back();
    std::printf("slot %2d: %3d keys / %3zu presentation samples; minY %4d mm "
                "at %d%s (bones %d/%d); maxY %5d mm; saturation %llu\n",
                clip.slot_id, clip.frame_count, s.samples.size(),
                to_mm(s.worst_min_fx), s.worst_tick / 2,
                (s.worst_tick & 1) ? ".5" : "", s.worst_b0, s.worst_b1,
                to_mm(s.worst_max_fx),
                static_cast<unsigned long long>(s.saturation));
  }

  int failures = 0;
  auto require = [&](bool ok, const char* what) {
    if (!ok) {
      ++failures;
      std::printf("  ** FAIL: %s\n", what);
    }
  };

  // Tail-balance contact is proved from actual posed vertices at every key and
  // midpoint. The accepted art deliberately puts all six vertex regions across
  // the five-segment tapered support into a shallow terrain bite; merely touching
  // blade tips can no longer pass. Only the declared flop window may go deeper.
  if (const ClipScan* balance = find_scan(scans, 7)) {
    std::array<int32_t, zixx::kSpineBones> low{};
    std::array<int32_t, zixx::kSpineBones> high{};
    low.fill(INT32_MAX);
    high.fill(INT32_MIN);
    int least_near = zixx::kSpineBones;
    int least_tick = -1;
    int32_t plateau_worst = INT32_MAX;
    int32_t plateau_blade_worst = INT32_MAX;
    for (int t = 2 * zixx::kBalSupportBeginKey;
         t <= 2 * zixx::kBalSupportEndKey; ++t) {
      const PosedSample& s = balance->samples[t];
      plateau_worst = std::min(plateau_worst, to_mm(s.min_y_fx));
      plateau_blade_worst =
          std::min(plateau_blade_worst, to_mm(s.blade_min_y_fx));
      int near = 0;
      for (int b = zixx::kBalSupport0; b < zixx::kSpineBones; ++b) {
        const int32_t y = to_mm(s.bone_min_y_fx[b]);
        low[b] = std::min(low[b], y);
        high[b] = std::max(high[b], y);
        if (y >= -zixx::kBalSupportBiteMm &&
            y <= zixx::kBalSupportHoverMm)
          ++near;
      }
      if (near < least_near) {
        least_near = near;
        least_tick = t;
      }
    }

    int32_t outside_worst = INT32_MAX;
    int outside_tick = -1;
    int outside_b0 = -1, outside_b1 = -1;
    int32_t impact_worst = INT32_MAX;
    int impact_tick = -1;
    int impact_b0 = -1, impact_b1 = -1;
    int32_t aftermath_worst = INT32_MAX;
    int aftermath_tick = -1;
    int aftermath_b0 = -1, aftermath_b1 = -1;
    for (const PosedSample& s : balance->samples) {
      const bool impact =
          s.tick >= 2 * zixx::kBalImpactBeginKey -
                        zixx::kBalImpactLeadPresentationTicks &&
          s.tick <= 2 * zixx::kBalImpactEndKey;
      const bool aftermath =
          s.tick >= 2 * zixx::kBalAftermathBeginKey &&
          s.tick <= 2 * zixx::kBalAftermathEndKey;
      const int32_t y = to_mm(s.min_y_fx);
      if (impact) {
        if (y < impact_worst) {
          impact_worst = y;
          impact_tick = s.tick;
          impact_b0 = s.min_b0;
          impact_b1 = s.min_b1;
        }
      } else if (aftermath) {
        if (y < aftermath_worst) {
          aftermath_worst = y;
          aftermath_tick = s.tick;
          aftermath_b0 = s.min_b0;
          aftermath_b1 = s.min_b1;
        }
      } else if (y < outside_worst) {
        outside_worst = y;
        outside_tick = s.tick;
        outside_b0 = s.min_b0;
        outside_b1 = s.min_b1;
      }
    }

    // Chord-length ranges are rigid-transform invariant: a nonzero range in
    // every successive upper-body span proves the accepted silhouette really
    // changes shape throughout the fight rather than rotating as one rod.
    constexpr std::array<std::pair<int, int>, 4> kShapeSpans = {
        std::pair<int, int>{0, 10}, {10, 20}, {20, 30}, {30, 40}};
    std::array<int32_t, kShapeSpans.size()> chord_low{};
    std::array<int32_t, kShapeSpans.size()> chord_high{};
    chord_low.fill(INT32_MAX);
    chord_high.fill(INT32_MIN);
    for (int t = 2 * zixx::kBalSupportBeginKey;
         t <= 2 * zixx::kBalSupportEndKey; ++t) {
      const PosedSample& s = balance->samples[t];
      for (size_t i = 0; i < kShapeSpans.size(); ++i) {
        const int a = kShapeSpans[i].first;
        const int b = kShapeSpans[i].second;
        const int64_t dx = s.x_mm[b] - s.x_mm[a];
        const int64_t dy = s.y_mm[b] - s.y_mm[a];
        const int64_t dz = s.z_mm[b] - s.z_mm[a];
        const int32_t chord = static_cast<int32_t>(zref::isqrt_u64(
            static_cast<uint64_t>(dx * dx + dy * dy + dz * dz)));
        chord_low[i] = std::min(chord_low[i], chord);
        chord_high[i] = std::max(chord_high[i], chord);
      }
    }
    const StationStepMaximum continuity = station_step_max_mm(
        *balance, 1, static_cast<int>(balance->samples.size()) - 1);

    std::printf("BALANCE support posed-vertex minima by bone:");
    for (int b = zixx::kBalSupport0; b < zixx::kSpineBones; ++b)
      std::printf(" b%d=%d..%d", b, low[b], high[b]);
    std::printf(" mm; least body regions=%d at %d%s; plateau all/blade "
                "%d/%d mm\n",
                least_near, least_tick / 2,
                (least_tick & 1) ? ".5" : "", plateau_worst,
                plateau_blade_worst);
    std::printf(
        "BALANCE terrain: outside declared contact %d mm at %d%s "
        "(bones %d/%d); impact %d mm at %d%s (bones %d/%d); "
        "aftermath %d mm at %d%s (bones %d/%d)\n",
        outside_worst, outside_tick / 2, (outside_tick & 1) ? ".5" : "",
        outside_b0, outside_b1, impact_worst, impact_tick / 2,
        (impact_tick & 1) ? ".5" : "", impact_b0, impact_b1,
        aftermath_worst, aftermath_tick / 2,
        (aftermath_tick & 1) ? ".5" : "", aftermath_b0, aftermath_b1);

    std::printf("BALANCE shape-travel chord ranges:");
    for (size_t i = 0; i < kShapeSpans.size(); ++i)
      std::printf(" %d-%d=%d", kShapeSpans[i].first,
                  kShapeSpans[i].second, chord_high[i] - chord_low[i]);
    std::printf(" mm; max 60 Hz station step %d mm at %d%s station %d\n",
                continuity.mm, continuity.tick / 2,
                (continuity.tick & 1) ? ".5" : "", continuity.station);

    bool whole_body_shape_travels = true;
    for (size_t i = 0; i < kShapeSpans.size(); ++i)
      if (chord_high[i] - chord_low[i] <
          zixx::kBalMinShapeChordTravelMm)
        whole_body_shape_travels = false;
    require(whole_body_shape_travels,
            "balance shape change stopped travelling through the upper body");
    require(continuity.mm <= zixx::kBalMaxStationStepMm,
            "balance contains a high-frequency 60 Hz station step");
    require(least_near == zixx::kSpineBones - zixx::kBalSupport0,
            "balance reverted from broad body support to tip-only contact");
    require(plateau_worst >= -zixx::kBalSupportBiteMm,
            "balance support phase exceeds its authored terrain bite");
    require(outside_worst >= -zixx::kBalSupportBiteMm,
            "balance has undeclared terrain penetration outside the flop");
    require(impact_worst >= -zixx::kBalImpactBiteMm &&
                impact_worst <= -zixx::kBalImpactContactMinMm,
            "balance flop left its declared impact-contact band");
    require(aftermath_worst >= -zixx::kBalAftermathBiteMm &&
                aftermath_worst <= -zixx::kBalAftermathContactMinMm,
            "balance grounded aftermath left its authored contact band");
    require(key_pose_equal(*balance->clip, 0, zixx::kBalKeys - 1,
                           type.bank.bone_count, true),
            "balance does not recover bit-exactly to authored rest");
  }

  // KO, landing, recovery, and all three death approaches now have complete,
  // non-overlapping 3D terrain declarations. Values were named in the authored
  // clips after every-frame visual acceptance; this probe only compares the real
  // posed surface against them.
  if (const ClipScan* knock = find_scan(scans, zixx::kSlotKnock)) {
    const TerrainWindow before = terrain_window(
        *knock, 0, zixx::kKnockImpactContactBeginPresentationTick - 1);
    const TerrainWindow impact = terrain_window(
        *knock, zixx::kKnockImpactContactBeginPresentationTick,
        zixx::kKnockImpactContactEndPresentationTick);
    const TerrainWindow held = terrain_window(
        *knock, zixx::kKnockImpactContactEndPresentationTick + 1,
        static_cast<int>(knock->samples.size()) - 1);
    print_terrain_window("knock", "head-led throw", before);
    print_terrain_window("knock", "flank impact", impact);
    print_terrain_window("knock", "stunned hold", held);
    require(before.worst_mm >= -zixx::kKnockLeadTerrainBiteMm &&
                before.shallowest_mm <= zixx::kKnockGroundHoverMm,
            "knock pre-impact support left its terrain declaration");
    require(impact.worst_mm >= -zixx::kKnockImpactTerrainBiteMm &&
                impact.worst_mm <= -zixx::kKnockImpactContactMinMm &&
                impact.shallowest_mm <= zixx::kKnockGroundHoverMm,
            "knock flank impact left its authored contact band");
    require(held.worst_mm >= -zixx::kKnockHeldTerrainBiteMm &&
                held.worst_mm <= -zixx::kKnockHeldContactMinMm,
            "knock stunned hold left its authored flank contact");
  } else {
    require(false, "missing knock clip contact declaration");
  }

  if (const ClipScan* getup = find_scan(scans, zixx::kSlotGetUp)) {
    const TerrainWindow contact =
        terrain_window(*getup, 0, static_cast<int>(getup->samples.size()) - 1);
    print_terrain_window("get-up", "complete recovery", contact);
    require(contact.worst_mm >= -zixx::kGetUpTerrainBiteMm &&
                contact.worst_mm <= -zixx::kGetUpContactMinMm &&
                contact.shallowest_mm <= zixx::kGetUpGroundHoverMm,
            "get-up left its complete grounded recovery declaration");
  } else {
    require(false, "missing get-up clip contact declaration");
  }

  if (const ClipScan* landing = find_scan(scans, zixx::kSlotHitFloor)) {
    const TerrainWindow approach = terrain_window(
        *landing, 0, zixx::kHitFloorApproachEndPresentationTick);
    const TerrainWindow impact = terrain_window(
        *landing, zixx::kHitFloorImpactBeginPresentationTick,
        zixx::kHitFloorImpactEndPresentationTick);
    const TerrainWindow absorb = terrain_window(
        *landing, zixx::kHitFloorImpactEndPresentationTick + 1,
        zixx::kHitFloorAbsorbEndPresentationTick);
    const TerrainWindow settle = terrain_window(
        *landing, zixx::kHitFloorSettleBeginPresentationTick,
        static_cast<int>(landing->samples.size()) - 1);
    print_terrain_window("landing", "clear approach", approach);
    print_terrain_window("landing", "impact bite", impact);
    print_terrain_window("landing", "rebound absorption", absorb);
    print_terrain_window("landing", "stunned settle", settle);
    require(approach.worst_mm >= 0,
            "landing touched terrain before its declared impact lead");
    require(impact.worst_mm >= -zixx::kHitFloorImpactTerrainBiteMm &&
                impact.worst_mm <= -zixx::kHitFloorImpactContactMinMm,
            "landing impact left its authored deep contact band");
    require(absorb.worst_mm >= -zixx::kHitFloorAbsorbTerrainBiteMm,
            "landing rebound exceeded its authored absorption bite");
    require(settle.worst_mm >= -zixx::kHitFloorSettleTerrainBiteMm &&
                settle.worst_mm <= -zixx::kHitFloorSettleContactMinMm &&
                settle.shallowest_mm <= zixx::kKnockGroundHoverMm,
            "landing stunned settle left its authored shallow contact");
  } else {
    require(false, "missing hit-floor clip contact declaration");
  }

  if (const ClipScan* death = find_scan(scans, 6)) {
    const TerrainWindow living = terrain_window(
        *death, 0, 2 * zixx::kDeathCollapseContactBeginKey - 1);
    const TerrainWindow collapse = terrain_window(
        *death, 2 * zixx::kDeathCollapseContactBeginKey,
        2 * zixx::kDeathCollapseContactEndKey);
    const TerrainWindow corpse = terrain_window(
        *death, 2 * zixx::kDeathCollapseContactEndKey + 1,
        static_cast<int>(death->samples.size()) - 1);
    print_terrain_window("death0", "shudder and give", living);
    print_terrain_window("death0", "flank collapse", collapse);
    print_terrain_window("death0", "corpse hold", corpse);
    require(living.worst_mm >= -zixx::kDeathRestTerrainBiteMm &&
                living.shallowest_mm <= zixx::kDeathGroundHoverMm,
            "death0 living interval left its terrain declaration");
    require(collapse.worst_mm >= -zixx::kDeathCollapseTerrainBiteMm &&
                collapse.shallowest_mm <= zixx::kDeathGroundHoverMm,
            "death0 collapse exceeded its declared flank contact");
    require(corpse.worst_mm >= -zixx::kDeathCorpseTerrainBiteMm &&
                corpse.shallowest_mm <= zixx::kDeathGroundHoverMm,
            "death0 corpse interval left its shallow contact declaration");
  } else {
    require(false, "missing death0 contact declaration");
  }

  if (const ClipScan* death = find_scan(scans, zixx::kSlotDeath1)) {
    const TerrainWindow fight = terrain_window(
        *death, 0, 2 * zixx::kD1FightContactEndKey);
    const TerrainWindow collapse = terrain_window(
        *death, 2 * zixx::kD1FightContactEndKey + 1,
        2 * zixx::kD1CollapseContactEndKey);
    const TerrainWindow aftermath = terrain_window(
        *death, 2 * zixx::kD1CollapseContactEndKey + 1,
        static_cast<int>(death->samples.size()) - 1);
    print_terrain_window("death1", "whole-body fight", fight);
    print_terrain_window("death1", "delayed collapse", collapse);
    print_terrain_window("death1", "tail slaps and hold", aftermath);
    require(fight.worst_mm >= -zixx::kD1FightTerrainBiteMm &&
                fight.shallowest_mm >=
                    zixx::kD1FightTerrainClearanceMinMm &&
                fight.shallowest_mm <= zixx::kD1FightTerrainClearanceMm,
            "death1 fight left its authored contact/buck interval");
    require(collapse.worst_mm >= -zixx::kD1CollapseTerrainBiteMm &&
                collapse.shallowest_mm <= zixx::kD1GroundHoverMm,
            "death1 delayed collapse exceeded its terrain declaration");
    require(aftermath.worst_mm >= -zixx::kD1AftermathTerrainBiteMm &&
                aftermath.shallowest_mm <= zixx::kD1GroundHoverMm,
            "death1 aftermath left its tail-slap/corpse declaration");
  } else {
    require(false, "missing death1 contact declaration");
  }

  if (const ClipScan* death = find_scan(scans, zixx::kSlotDeath2)) {
    const TerrainWindow complaint = terrain_window(
        *death, 0, 2 * zixx::kD2ComplaintContactEndKey);
    const TerrainWindow unwind = terrain_window(
        *death, 2 * zixx::kD2ComplaintContactEndKey + 1,
        2 * zixx::kD2UnwindContactEndKey);
    const TerrainWindow held = terrain_window(
        *death, 2 * zixx::kD2UnwindContactEndKey + 1,
        static_cast<int>(death->samples.size()) - 1);
    print_terrain_window("death2", "complaint phrases", complaint);
    print_terrain_window("death2", "loss of S", unwind);
    print_terrain_window("death2", "paid-out hold", held);
    require(complaint.worst_mm >= -zixx::kD2ComplaintTerrainBiteMm &&
                complaint.shallowest_mm <=
                    zixx::kD2ComplaintTerrainClearanceMm,
            "death2 complaint interval left its terrain/heave declaration");
    require(unwind.worst_mm >= -zixx::kD2UnwindTerrainBiteMm &&
                unwind.shallowest_mm <= zixx::kD2GroundHoverMm,
            "death2 unwind left its terrain declaration");
    require(held.worst_mm >= -zixx::kD2HeldTerrainBiteMm &&
                held.shallowest_mm <= zixx::kD2GroundHoverMm,
            "death2 paid-out hold left its shallow terrain declaration");
  } else {
    require(false, "missing death2 contact declaration");
  }

  // The historical quick taunt remains slot 30. Slot 44 is deliberately a
  // separate, much slower performance: lower-neck rotation starts first, then
  // the skull's local wobble arrives four authored keys later. These checks
  // guard the visually accepted 239-frame sequence; they do not choose motion.
  if (const ClipScan* slow = find_scan(scans, zixx::kSlotSlowTaunt)) {
    const ClipScan* quick = find_scan(scans, zixx::kSlotTaunt);
    require(slow->clip->frame_count == zixx::kSlowTauntKeys &&
                slow->samples.size() ==
                    static_cast<size_t>(2 * (zixx::kSlowTauntKeys - 1) + 1),
            "slow taunt lost its authored 120-key / 239-sample cadence");
    require(quick != nullptr &&
                slow->clip->frame_count > 2 * quick->clip->frame_count,
            "slow taunt is no longer substantially slower than quick slot 30");
    require(key_pose_equal(*slow->clip, 0, zixx::kSlowTauntKeys - 1,
                           type.bank.bone_count, true),
            "slow taunt does not begin and end at bit-exact rest");

    const auto idle_at = [](int f) {
      zixx::Rig g;
      g.reset();
      const int32_t ph = f * (65536 / zixx::kSlowTauntKeys);
      const int32_t life = zixx::ss1000(f, 0, 12) -
                           zixx::ss1000(f, 106, 119);
      zixx::idle_body(g, ph, (zixx::kSlowTauntBodyLife * life) / 1000);
      return g;
    };
    constexpr int kNeckLeadKey = 12;
    constexpr int kHeadFollowKey =
        kNeckLeadKey + zixx::kSlowTauntHeadLagKeys;
    const zixx::Rig lead_idle = idle_at(kNeckLeadKey);
    const zixx::Rig follow_idle = idle_at(kHeadFollowKey);
    const zc::quat16& lead_neck =
        slow->clip->quats[kNeckLeadKey * type.bank.bone_count +
                          zixx::kBSpine0];
    const zc::quat16& lead_head =
        slow->clip->quats[kNeckLeadKey * type.bank.bone_count + zixx::kBHead];
    const zc::quat16& follow_head =
        slow->clip->quats[kHeadFollowKey * type.bank.bone_count + zixx::kBHead];
    require(std::memcmp(&lead_neck, &lead_idle.q[zixx::kBSpine0],
                        sizeof(lead_neck)) != 0,
            "slow taunt lower neck no longer initiates the gesture");
    require(std::memcmp(&lead_head, &lead_idle.q[zixx::kBHead],
                        sizeof(lead_head)) == 0,
            "slow taunt skull moved before its declared neck lead");
    require(std::memcmp(&follow_head, &follow_idle.q[zixx::kBHead],
                        sizeof(follow_head)) != 0,
            "slow taunt skull no longer follows the neck with delayed tilt");

    const StationStepMaximum continuity = station_step_max_mm(
        *slow, 1, static_cast<int>(slow->samples.size()) - 1);
    constexpr int32_t kAcceptedSlowTauntMaxStationStepMm = 40;
    require(continuity.mm <= kAcceptedSlowTauntMaxStationStepMm,
            "slow taunt regained a high-frequency station twitch");
    std::printf("TAUNT slow: %d keys / %zu samples, neck lead %d keys, "
                "max 60 Hz station step %d mm at %d%s station %d\n",
                slow->clip->frame_count, slow->samples.size(),
                zixx::kSlowTauntHeadLagKeys, continuity.mm,
                continuity.tick / 2, (continuity.tick & 1) ? ".5" : "",
                continuity.station);
  } else {
    require(false, "missing separate slow taunt in slot 44");
  }

  // Falling is a looping 4.8-second tumble, so unlike one-shot clips its 60 Hz
  // scan includes the real midpoint from final key back to key zero. Rigid-
  // transform-invariant chord ranges compare the accepted faster travelling
  // bend through four complete body regions, while station steps catch twitch.
  if (const ClipScan* fall = find_scan(scans, zixx::kSlotFall)) {
    require(fall->clip->frame_count == zixx::kFallKeys &&
                fall->samples.size() ==
                    static_cast<size_t>(2 * zixx::kFallKeys),
            "fall lost its authored 144-key / 288-sample loop cadence");

    // Authored-art timing contract, added only after the shared-clock render
    // was accepted by eye. Inspect the helper's UNWRAPPED output at the real
    // 60 Hz cadence: every step must advance, endpoints must make one exact
    // turn, and the one broad hesitation must remain near the inverted pose.
    const int fall_phase_ticks = 2 * zixx::kFallKeys;
    int32_t previous_phase = zixx::fall_tumble_phase(0, fall_phase_ticks);
    int32_t min_phase_step = INT32_MAX;
    int32_t max_phase_step = INT32_MIN;
    int slowest_phase_tick = -1;
    bool phase_monotonic = true;
    for (int tick = 1; tick <= fall_phase_ticks; ++tick) {
      const int32_t phase =
          zixx::fall_tumble_phase(tick, fall_phase_ticks);
      const int32_t step = phase - previous_phase;
      if (step <= 0) phase_monotonic = false;
      if (step < min_phase_step) {
        min_phase_step = step;
        slowest_phase_tick = tick;
      }
      max_phase_step = std::max(max_phase_step, step);
      previous_phase = phase;
    }
    require(phase_monotonic &&
                previous_phase == (1 << 16),
            "fall tumble phase stopped, reversed or lost its exact turn");
    require(slowest_phase_tick >= fall_phase_ticks / 2 - 12 &&
                slowest_phase_tick <= fall_phase_ticks / 2 + 12 &&
                min_phase_step * 4 < max_phase_step,
            "fall tumble lost its accepted single broad hesitation");
    constexpr std::array<std::pair<int, int>, 4> kFallSpans = {
        std::pair<int, int>{0, 14}, {14, 28}, {28, 42}, {42, 56}};
    std::array<int32_t, kFallSpans.size()> chord_low{};
    std::array<int32_t, kFallSpans.size()> chord_high{};
    std::array<int, kFallSpans.size()> low_tick{};
    std::array<int, kFallSpans.size()> high_tick{};
    chord_low.fill(INT32_MAX);
    chord_high.fill(INT32_MIN);
    for (const PosedSample& s : fall->samples) {
      for (size_t i = 0; i < kFallSpans.size(); ++i) {
        const int a = kFallSpans[i].first;
        const int b = kFallSpans[i].second;
        const int64_t dx = s.x_mm[b] - s.x_mm[a];
        const int64_t dy = s.y_mm[b] - s.y_mm[a];
        const int64_t dz = s.z_mm[b] - s.z_mm[a];
        const int32_t chord = static_cast<int32_t>(zref::isqrt_u64(
            static_cast<uint64_t>(dx * dx + dy * dy + dz * dz)));
        if (chord < chord_low[i]) {
          chord_low[i] = chord;
          low_tick[i] = s.tick;
        }
        if (chord > chord_high[i]) {
          chord_high[i] = chord;
          high_tick[i] = s.tick;
        }
      }
    }
    const StationStepMaximum continuity = station_step_max_mm(
        *fall, 1, static_cast<int>(fall->samples.size()) - 1);
    int32_t seam_first_half_step = 0;
    int32_t seam_second_half_step = 0;
    const PosedSample& seam_key = fall->samples[fall->samples.size() - 2];
    const PosedSample& seam_mid = fall->samples.back();
    const PosedSample& first = fall->samples.front();
    for (int i = 0; i < zixx::kProfileStations; ++i) {
      const int64_t ax = seam_mid.x_mm[i] - seam_key.x_mm[i];
      const int64_t ay = seam_mid.y_mm[i] - seam_key.y_mm[i];
      const int64_t az = seam_mid.z_mm[i] - seam_key.z_mm[i];
      seam_first_half_step = std::max(
          seam_first_half_step,
          static_cast<int32_t>(zref::isqrt_u64(
              static_cast<uint64_t>(ax * ax + ay * ay + az * az))));
      const int64_t dx = first.x_mm[i] - seam_mid.x_mm[i];
      const int64_t dy = first.y_mm[i] - seam_mid.y_mm[i];
      const int64_t dz = first.z_mm[i] - seam_mid.z_mm[i];
      seam_second_half_step = std::max(
          seam_second_half_step,
          static_cast<int32_t>(zref::isqrt_u64(
              static_cast<uint64_t>(dx * dx + dy * dy + dz * dz))));
    }
    constexpr std::array<int32_t, 4> kAcceptedFallMinChordTravelMm = {
        280, 160, 50, 140};
    bool whole_body_shape_travels = true;
    for (size_t i = 0; i < kFallSpans.size(); ++i)
      if (chord_high[i] - chord_low[i] < kAcceptedFallMinChordTravelMm[i])
        whole_body_shape_travels = false;
    require(whole_body_shape_travels,
            "fall lost the accepted stronger bend through a body region");
    // The first three regional compression extrema follow one another by 16
    // authored keys in the accepted motion. A broad 12..20-key envelope proves
    // propagation without turning that observed art into a generated value.
    const int lead_lag_01 = low_tick[0] - low_tick[1];
    const int lead_lag_12 = low_tick[1] - low_tick[2];
    require(lead_lag_01 >= 24 && lead_lag_01 <= 40 &&
                lead_lag_12 >= 24 && lead_lag_12 <= 40,
            "fall bend no longer propagates progressively through the body");
    constexpr int32_t kAcceptedFallMaxStationStepMm = 220;
    require(continuity.mm <= kAcceptedFallMaxStationStepMm &&
                seam_first_half_step <= kAcceptedFallMaxStationStepMm &&
                seam_second_half_step <= kAcceptedFallMaxStationStepMm,
            "fall regained a high-frequency step or looping-seam twitch");
    std::printf("FALL tumble phase: exact unwrapped turn, step %d..%d angle16, "
                "slowest at %d%s\n",
                min_phase_step, max_phase_step, slowest_phase_tick / 2,
                (slowest_phase_tick & 1) ? ".5" : "");
    std::printf("FALL shape-travel chord ranges:");
    for (size_t i = 0; i < kFallSpans.size(); ++i)
      std::printf(" %d-%d=%d (lo %d%s / hi %d%s)",
                  kFallSpans[i].first, kFallSpans[i].second,
                  chord_high[i] - chord_low[i], low_tick[i] / 2,
                  (low_tick[i] & 1) ? ".5" : "", high_tick[i] / 2,
                  (high_tick[i] & 1) ? ".5" : "");
    std::printf(" mm; regional propagation lags %d/%d ticks; max 60 Hz "
                "station step %d mm at %d%s station %d; seam half-steps "
                "%d/%d mm\n",
                lead_lag_01, lead_lag_12, continuity.mm,
                continuity.tick / 2,
                (continuity.tick & 1) ? ".5" : "", continuity.station,
                seam_first_half_step, seam_second_half_step);
  } else {
    require(false, "missing looping fall clip in slot 4");
  }

  // Non-adjacent station overlap at the real 60 Hz presentation cadence.
  int total_overlaps = 0;
  for (const ClipScan& scan : scans) {
    int clip_overlaps = 0;
    int32_t worst_depth = 0;
    int worst_tick = -1, worst_i = -1, worst_j = -1;
    for (const PosedSample& s : scan.samples) {
      for (const auto& pr : pairs) {
        const int i = pr.first, j = pr.second;
        const int64_t dx = s.x_mm[i] - s.x_mm[j];
        const int64_t dy = s.y_mm[i] - s.y_mm[j];
        const int64_t dz = s.z_mm[i] - s.z_mm[j];
        const int64_t d2 = dx * dx + dy * dy + dz * dz;
        const int64_t rr = stations[i].r_mm + stations[j].r_mm;
        if (d2 < rr * rr) {
          ++clip_overlaps;
          const int32_t depth = static_cast<int32_t>(
              rr - static_cast<int64_t>(zref::isqrt_u64(
                       static_cast<uint64_t>(d2))));
          if (depth > worst_depth) {
            worst_depth = depth;
            worst_tick = s.tick;
            worst_i = i;
            worst_j = j;
          }
        }
      }
    }
    total_overlaps += clip_overlaps;
    const int32_t allow = overlap_allowance_mm(scan.clip->slot_id);
    if (worst_depth > allow) {
      ++failures;
      std::printf("  ** FAIL: slot %d overlap %d mm > declared %d mm at "
                  "%d%s, stations %d/%d\n",
                  scan.clip->slot_id, worst_depth, allow, worst_tick / 2,
                  (worst_tick & 1) ? ".5" : "", worst_i, worst_j);
    } else if (clip_overlaps > 0) {
      std::printf("slot %2d overlap: %d samples/pairs, worst %d/%d mm at "
                  "%d%s (stations %d/%d)\n",
                  scan.clip->slot_id, clip_overlaps, worst_depth, allow,
                  worst_tick / 2, (worst_tick & 1) ? ".5" : "", worst_i,
                  worst_j);
    }
  }
  std::printf("OVERLAP: %d sample/pair hits, all checked at keys + midpoints\n",
              total_overlaps);

  // Whole-body gummy spring. These envelopes are written only after the fixed
  // side/high-three-quarter playback has been accepted. They compare the chosen
  // art at every real vertex, key and runtime midpoint; no probe value authors
  // the pose.
  const ClipScan* spring = find_scan(scans, 3);
  require(spring != nullptr, "missing primary slot 3 shared spring clip");
  if (spring) {
    const int entry_tick = 2 * zixx::kSaltoSpringEntryEndKey;
    const int deep_tick = 2 * zixx::kSaltoCompressEndKey;
    const int hold_end_tick = 2 * zixx::kSaltoCompressHoldEndKey;
    const int released_tick = 2 * zixx::kSaltoSpringReleasePoseKey;
    const int rigid_air_tick = 2 * zixx::kSaltoRigidReleaseEndKey;
    require(entry_tick < static_cast<int>(spring->samples.size()) &&
                deep_tick < static_cast<int>(spring->samples.size()) &&
                rigid_air_tick < static_cast<int>(spring->samples.size()),
            "spring phase sample outside primary clip");
    // The golden and planned paths now share ONE arming schedule, so the
    // ordering law is checked on that schedule rather than on a parallel
    // curve table that could silently drift away from the clip.
    // The LAW is the ordering, not a coincidence at one key index. With the
    // single arming clock the assembled knot no longer falls on an integer key,
    // so check the ordering everywhere: squash may only be non-zero once entry
    // has completed, entry must never retreat while squash is opening, and the
    // squash must actually reach full depth.
    bool ordering_holds = true;
    bool squash_reaches_full = false;
    for (int key = 0; key <= zixx::kSaltoCompressHoldEndKey; ++key) {
      const int32_t e = zixx::spring_shared_entry_amount(key);
      const int32_t q = zixx::spring_shared_squash_amount(key);
      if (q > 0 && e != 1000) ordering_holds = false;
      if (q >= 1000) squash_reaches_full = true;
    }
    require(ordering_holds && squash_reaches_full &&
                zixx::spring_shared_entry_amount(0) == 0 &&
                zixx::spring_shared_squash_amount(0) == 0,
            "spring squash begins before full-tail entry is complete");

    const PosedSample& rest = spring->samples[0];
    const PosedSample& entry = spring->samples[entry_tick];
    const PosedSample& deep = spring->samples[deep_tick];
    const PosedSample& released = spring->samples[released_tick];
    const PosedSample& rigid_air = spring->samples[rigid_air_tick];
    struct Region { const char* name; int lo; int hi; };
    const Region regions[] = {
        {"head", 0, 5}, {"neck", 6, 12}, {"front", 13, 20},
        {"middle", 21, 32}, {"grounded run", 33, 44},
        {"taper", 45, 51}, {"tail", 52, 56}};
    std::array<int32_t, 7> region_motion{};
    std::array<int32_t, 7> region_descent{};
    bool every_region_joins = true;
    std::printf("SPRING full-S entry mean station travel / compression descent:");
    for (size_t ri = 0; ri < region_motion.size(); ++ri) {
      const Region& r = regions[ri];
      int64_t travel = 0;
      int64_t descent = 0;
      for (int i = r.lo; i <= r.hi; ++i) {
        const int64_t dx = entry.x_mm[i] - rest.x_mm[i];
        const int64_t dy = entry.y_mm[i] - rest.y_mm[i];
        const int64_t dz = entry.z_mm[i] - rest.z_mm[i];
        travel += static_cast<int32_t>(zref::isqrt_u64(
            static_cast<uint64_t>(dx * dx + dy * dy + dz * dz)));
        descent += deep.y_mm[i] - entry.y_mm[i];
      }
      region_motion[ri] =
          static_cast<int32_t>(travel / (r.hi - r.lo + 1));
      region_descent[ri] =
          static_cast<int32_t>(descent / (r.hi - r.lo + 1));
      every_region_joins = every_region_joins && region_motion[ri] > 0;
      std::printf(" %s=%d/%d", r.name, region_motion[ri],
                  region_descent[ri]);
    }
    std::printf(" mm\n");
    require(every_region_joins,
            "enlarged jump S stopped recruiting a body region");
    // Regional distances remain comparison output, not generation-side gates.
    // The explicit whole-centreline pass intentionally replaced iteration 15's
    // unrelated procedural magnitudes. The durable mechanical law is that every
    // region participates; likeness is judged from the committed every-frame
    // sheets above this probe, not by fitting the superseded numeric envelope.
    // OWNER DIRECTION 23 (RUN-20260902-1816) re-authors this band. The old
    // band pinned the arming to the rejected 16/18 schedule in absolute keys,
    // which would forbid exactly the slower arming the owner has now asked
    // for three directions running. It is a regression band, not a law, so
    // it is re-recorded as DERIVED relationships: beat 1 (become the S) ends
    // strictly inside the arming and carries at least half of it; beat 2
    // (the compression) gets at least 16 frames; the loaded hold lives for
    // 8-24 frames. The RELEASE deltas are unchanged from the accepted
    // Direction-20 windows -- the release stays fast by design (D20 #4).
    require(zixx::kSaltoSpringEntryEndKey > 0 &&
                zixx::kSaltoSpringEntryEndKey <
                        zixx::kSaltoCompressEndKey &&
                2 * zixx::kSaltoSpringEntryEndKey >=
                        zixx::kSaltoCompressEndKey &&
                zixx::kSaltoCompressEndKey -
                        zixx::kSaltoSpringEntryEndKey >= 8 &&
                zixx::kSaltoCompressHoldEndKey -
                        zixx::kSaltoCompressEndKey >= 4 &&
                zixx::kSaltoCompressHoldEndKey -
                        zixx::kSaltoCompressEndKey <= 12 &&
                zixx::kSaltoSpringReleasePoseKey -
                        zixx::kSaltoCompressHoldEndKey >= 3 &&
                zixx::kSaltoSpringReleasePoseKey -
                        zixx::kSaltoCompressHoldEndKey <= 5 &&
                zixx::kSaltoRigidReleaseEndKey -
                        zixx::kSaltoSpringReleasePoseKey >= 2 &&
                zixx::kSaltoRigidReleaseEndKey -
                        zixx::kSaltoSpringReleasePoseKey <= 3 &&
                zixx::kSaltoReleaseEndKey -
                        zixx::kSaltoRigidReleaseEndKey >= 3 &&
                zixx::kSaltoReleaseEndKey -
                        zixx::kSaltoRigidReleaseEndKey <= 5,
            "spring phase timing left the accepted entry/hold/release envelope");

    const PosedRung rest_full = pose_rung(type, *spring->clip, 0, 0);
    const PosedRung entry_full = pose_rung(type, *spring->clip, 0, entry_tick);
    int64_t tail_follower_travel = 0;
    int32_t tail_follower_max = 0;
    int tail_follower_count = 0;
    for (size_t mi = 0; mi < type.mesh.size(); ++mi) {
      const zc::Meshlet& m = type.mesh[mi];
      if (m.deform.empty()) continue;
      for (size_t vi = 0; vi < m.verts.size(); ++vi) {
        if (m.deform[vi].role != zc::DeformRole::kFollower ||
            (m.verts[vi].b0 < zixx::kBBladeL &&
             m.verts[vi].b1 < zixx::kBBladeL))
          continue;
        const auto& a = rest_full.xyz_fx[mi][vi];
        const auto& b = entry_full.xyz_fx[mi][vi];
        const int64_t dx = to_mm(b[0]) - to_mm(a[0]);
        const int64_t dy = to_mm(b[1]) - to_mm(a[1]);
        const int64_t dz = to_mm(b[2]) - to_mm(a[2]);
        const int32_t d = static_cast<int32_t>(zref::isqrt_u64(
            static_cast<uint64_t>(dx * dx + dy * dy + dz * dz)));
        tail_follower_travel += d;
        tail_follower_max = std::max(tail_follower_max, d);
        ++tail_follower_count;
      }
    }
    const int32_t tail_follower_mean = tail_follower_count == 0
        ? 0 : static_cast<int32_t>(tail_follower_travel / tail_follower_count);
    std::printf("SPRING real tail followers: %d vertices, entry travel mean/max "
                "%d/%d mm\n", tail_follower_count, tail_follower_mean,
                tail_follower_max);
    require(tail_follower_count >= 150 && tail_follower_mean > 0 &&
                tail_follower_max > 0,
            "real tail tips stopped participating in the enlarged S");

    auto centre_span = [](const PosedSample& s) {
      int32_t lo = INT32_MAX, hi = INT32_MIN;
      for (int i = 0; i < zixx::kProfileStations; ++i) {
        lo = std::min(lo, s.y_mm[i]);
        hi = std::max(hi, s.y_mm[i]);
      }
      return hi - lo;
    };
    const int32_t entry_span = centre_span(entry);
    const int32_t deep_span = centre_span(deep);
    const int32_t head_support_dx =
        (deep.x_mm[0] - deep.support_x_mm) -
        (entry.x_mm[0] - entry.support_x_mm);
    const int32_t head_support_dy =
        (deep.y_mm[0] - deep.support_y_mm) -
        (entry.y_mm[0] - entry.support_y_mm);
    const int32_t support_dx = deep.support_x_mm - entry.support_x_mm;
    const int32_t support_dy = deep.support_y_mm - entry.support_y_mm;
    int32_t lateral_lo = INT32_MAX, lateral_hi = INT32_MIN;
    int32_t body_lateral_lo = INT32_MAX, body_lateral_hi = INT32_MIN;
    int lateral_lo_station = -1, lateral_hi_station = -1;
    for (int i = 0; i < zixx::kProfileStations; ++i) {
      if (deep.z_mm[i] < lateral_lo) {
        lateral_lo = deep.z_mm[i];
        lateral_lo_station = i;
      }
      if (deep.z_mm[i] > lateral_hi) {
        lateral_hi = deep.z_mm[i];
        lateral_hi_station = i;
      }
      if (i <= zixx::kTrunkEndStation) {
        body_lateral_lo = std::min(body_lateral_lo, deep.z_mm[i]);
        body_lateral_hi = std::max(body_lateral_hi, deep.z_mm[i]);
      }
    }
    const int32_t body_lateral_span = body_lateral_hi - body_lateral_lo;
    const int32_t whole_lateral_span = lateral_hi - lateral_lo;
    std::printf("SPRING ordered pose: entry/deep centre span %d/%d mm; "
                "head relative support dX/dY %d/%d mm; support dX/dY %d/%d "
                "mm; body/whole lateral span %d/%d mm (whole extrema "
                "stations %d/%d)\n", entry_span, deep_span, head_support_dx,
                head_support_dy, support_dx, support_dy, body_lateral_span,
                whole_lateral_span, lateral_lo_station, lateral_hi_station);
    // The authored heading centreline through the trunk remains planar. The
    // final tail stations inherit the model's preserved axial construction roll,
    // which shifts their skinned centres slightly in Z; guard that separately
    // rather than mistaking the intentional tail assembly for a crooked S.
    require(entry_span > deep_span && deep_span > 0 &&
                head_support_dy < 0 &&
                body_lateral_span <= kSpringTrunkLateralSpanMaxMm &&
                whole_lateral_span <= kSpringWholeTailLateralSpanMaxMm,
            "explicit spring lost its ordered assembled-to-collapsed planar S");
    // Station 14's full per-sample authored route is checked independently
    // below from every integer and true half-key. A single entry-to-deep delta
    // cannot describe that deliberately non-monotonic surface compensation.

    int32_t hold_shape_drift = 0;
    int32_t hold_support_drift = 0;
    for (int t = deep_tick; t <= hold_end_tick; ++t) {
      const PosedSample& held = spring->samples[t];
      hold_support_drift = std::max(
          hold_support_drift,
          std::max({std::abs(held.support_x_mm - deep.support_x_mm),
                    std::abs(held.support_y_mm - deep.support_y_mm),
                    std::abs(held.support_z_mm - deep.support_z_mm)}));
      for (int i = 0; i < zixx::kProfileStations; ++i) {
        hold_shape_drift = std::max(
            hold_shape_drift,
            std::max({std::abs((held.x_mm[i] - held.support_x_mm) -
                               (deep.x_mm[i] - deep.support_x_mm)),
                      std::abs((held.y_mm[i] - held.support_y_mm) -
                               (deep.y_mm[i] - deep.support_y_mm)),
                      std::abs((held.z_mm[i] - held.support_z_mm) -
                               (deep.z_mm[i] - deep.support_z_mm))}));
      }
    }
    std::printf("SPRING compressed hold: shape/support drift %d/%d mm over %d "
                "keys\n", hold_shape_drift, hold_support_drift,
                zixx::kSaltoCompressHoldEndKey - zixx::kSaltoCompressEndKey);
    require(hold_shape_drift <= zixx::kSpringHoldLivingDriftMm &&
                hold_support_drift <= 1,
            "spring lost its readable, genuinely held maximum brace");

    int32_t release_shape_error = 0;
    int32_t rigid_air_shape_error = 0;
    for (int i = 0; i < zixx::kProfileStations; ++i) {
      release_shape_error = std::max(
          release_shape_error,
          std::max({std::abs((released.x_mm[i] - released.support_x_mm) -
                             (rest.x_mm[i] - rest.support_x_mm)),
                    std::abs((released.y_mm[i] - released.support_y_mm) -
                             (rest.y_mm[i] - rest.support_y_mm)),
                    std::abs((released.z_mm[i] - released.support_z_mm) -
                             (rest.z_mm[i] - rest.support_z_mm))}));
      rigid_air_shape_error = std::max(
          rigid_air_shape_error,
          std::max({std::abs((rigid_air.x_mm[i] - rigid_air.support_x_mm) -
                             (released.x_mm[i] - released.support_x_mm)),
                    std::abs((rigid_air.y_mm[i] - rigid_air.support_y_mm) -
                             (released.y_mm[i] - released.support_y_mm)),
                    std::abs((rigid_air.z_mm[i] - rigid_air.support_z_mm) -
                             (released.z_mm[i] - released.support_z_mm))}));
    }
    const int32_t rigid_air_lift =
        rigid_air.support_y_mm - released.support_y_mm;
    std::printf("SPRING release: rest-shape error %d mm, intact airborne-S "
                "error %d mm, whole-support lift %d mm\n",
                release_shape_error, rigid_air_shape_error, rigid_air_lift);
    require(release_shape_error <= 1 && rigid_air_shape_error <= 1 &&
                rigid_air_lift >= 550 && rigid_air_lift <= 650,
            "spring no longer releases and rises as one intact S before coiling");

    bool identity_keys_exact = true;
    for (int f = 0; f < spring->clip->frame_count; ++f) {
      // The authorised window is wherever the shared schedule actually asks
      // for squash. Naming a key index instead was only ever a restatement of
      // one particular timing, and the timing has moved.
      const bool authorised =
          f < zixx::kSaltoSpringReleasePoseKey &&
          zixx::spring_shared_squash_amount(f) > 0;
      const zc::DeformSample d = spring->clip->deform.empty()
          ? zc::DeformSample{} : spring->clip->deform[static_cast<size_t>(f)];
      if (!authorised && (d.flatten != 0 || d.spread != 0))
        identity_keys_exact = false;
    }
    for (int rung = 0; rung < 2; ++rung) {
      const auto& mesh = rung == 0 ? type.mesh : type.micro;
      for (const zc::Meshlet& m : mesh) {
        if (m.deform.empty()) continue;
        for (size_t vi = 0; vi < m.verts.size(); ++vi) {
          const zc::SkinVertex id =
              zc::deform_skin_vertex(m.verts[vi], m.deform[vi], {});
          if (std::memcmp(&id, &m.verts[vi], sizeof(id)) != 0)
            identity_keys_exact = false;
        }
      }
    }
    require(identity_keys_exact,
            "deformation sidecar lost exact identity outside spring frames");

    for (int rung = 0; rung < 2; ++rung) {
      const auto& mesh = rung == 0 ? type.mesh : type.micro;
      const zc::DeformSample active = zc::deformation_sample(
          type, spring->clip->slot_id, zixx::kSaltoCompressEndKey, 0);
      int32_t min_radial_ratio = INT32_MAX;
      int32_t body_radial_ratio = INT32_MAX;
      int32_t head_radial_ratio = INT32_MAX;
      int follower_count = 0;
      int follower_faults = 0;
      for (const zc::Meshlet& m : mesh) {
        if (m.deform.empty()) continue;
        for (size_t vi = 0; vi < m.verts.size(); ++vi) {
          const zc::SkinVertex& v = m.verts[vi];
          const zc::DeformVertex& d = m.deform[vi];
          const zc::SkinVertex moved = zc::deform_skin_vertex(v, d, active);
          if (d.role == zc::DeformRole::kRadial) {
            const int64_t bx = to_mm(v.x) - to_mm(d.center_x);
            const int64_t by = to_mm(v.y) - to_mm(d.center_y);
            const int64_t bz = to_mm(v.z) - to_mm(d.center_z);
            const int64_t ax = to_mm(moved.x) - to_mm(d.center_x);
            const int64_t ay = to_mm(moved.y) - to_mm(d.center_y);
            const int64_t az = to_mm(moved.z) - to_mm(d.center_z);
            const int32_t before = static_cast<int32_t>(zref::isqrt_u64(
                static_cast<uint64_t>(bx * bx + by * by + bz * bz)));
            const int32_t after = static_cast<int32_t>(zref::isqrt_u64(
                static_cast<uint64_t>(ax * ax + ay * ay + az * az)));
            if (before >= 10) {
              const int32_t ratio = after * 1000 / before;
              min_radial_ratio = std::min(min_radial_ratio, ratio);
              if (d.strength == zixx::kSpringBodyDeformStrength)
                body_radial_ratio = std::min(body_radial_ratio, ratio);
              if (d.strength == zixx::kSpringSkullDeformStrength)
                head_radial_ratio = std::min(head_radial_ratio, ratio);
            }
          } else if (d.role == zc::DeformRole::kFollower) {
            ++follower_count;
            zc::SkinVertex carrier = v;
            carrier.x = d.carrier_x;
            carrier.y = d.carrier_y;
            carrier.z = d.carrier_z;
            zc::DeformVertex radial = d;
            radial.role = zc::DeformRole::kRadial;
            const zc::SkinVertex moved_carrier =
                zc::deform_skin_vertex(carrier, radial, active);
            if (moved.x - v.x != moved_carrier.x - carrier.x ||
                moved.y - v.y != moved_carrier.y - carrier.y ||
                moved.z - v.z != moved_carrier.z - carrier.z ||
                moved.nx != v.nx || moved.ny != v.ny || moved.nz != v.nz)
              ++follower_faults;
          }
        }
      }
      std::printf("SPRING rung %s: radial retained min/body/head %d/%d/%d "
                  "per-mille; rigid followers %d, faults %d; normals len2 "
                  "%d..%d faults %llu\n", rung == 0 ? "full" : "micro",
                  min_radial_ratio, body_radial_ratio, head_radial_ratio,
                  follower_count, follower_faults,
                  spring->normal_min_len2[rung], spring->normal_max_len2[rung],
                  static_cast<unsigned long long>(spring->normal_faults[rung]));
      // Band re-derived for owner direction 21: the squeeze's contact relief
      // is the flatten, and it must be VISIBLE, so kSpringBodyFlattenQ16 rose
      // from ~26% to ~31% and the accepted retained-radius floor moves with
      // it. Still positive volume, still selective (head barely squashes).
      require(min_radial_ratio >= 650 && min_radial_ratio <= 730 &&
                  body_radial_ratio >= 650 && body_radial_ratio <= 730 &&
                  head_radial_ratio >= 900 && head_radial_ratio <= 950,
              "spring cross-sections left the accepted positive-volume, "
              "selective-squash envelope");
      require(follower_count >= (rung == 0 ? 160 : 70) &&
                  follower_faults == 0,
              "spring attachment no longer follows its radial carrier rigidly");
      require(spring->normal_faults[rung] == 0,
              "spring deformation produced an invalid full/micro normal");
    }

    auto same_bind_source = [](const zc::SkinVertex& a,
                               const zc::SkinVertex& b) {
      return a.x == b.x && a.y == b.y && a.z == b.z && a.b0 == b.b0 &&
             a.b1 == b.b1 && a.w0 == b.w0;
    };
    auto same_deform_source = [](const zc::DeformVertex& a,
                                 const zc::DeformVertex& b) {
      return a.center_x == b.center_x && a.center_y == b.center_y &&
             a.center_z == b.center_z && a.carrier_x == b.carrier_x &&
             a.carrier_y == b.carrier_y && a.carrier_z == b.carrier_z &&
             a.role == b.role && a.axis == b.axis &&
             a.strength == b.strength;
    };
    int shared_micro_vertices = 0;
    int shared_metadata_faults = 0;
    for (const zc::Meshlet& micro : type.micro) {
      if (micro.deform.empty()) continue;
      for (size_t mvi = 0; mvi < micro.verts.size(); ++mvi) {
        if (micro.deform[mvi].role == zc::DeformRole::kNone) continue;
        bool shares_bind_source = false;
        bool metadata_agrees = false;
        for (const zc::Meshlet& full : type.mesh) {
          if (full.deform.empty()) continue;
          for (size_t fvi = 0; fvi < full.verts.size(); ++fvi) {
            if (!same_bind_source(micro.verts[mvi], full.verts[fvi])) continue;
            shares_bind_source = true;
            if (same_deform_source(micro.deform[mvi], full.deform[fvi]))
              metadata_agrees = true;
          }
        }
        if (shares_bind_source) {
          ++shared_micro_vertices;
          if (!metadata_agrees) ++shared_metadata_faults;
        }
      }
    }
    std::printf("SPRING shared full/micro deform sources: %d vertices, %d "
                "metadata faults\n", shared_micro_vertices,
                shared_metadata_faults);
    require(shared_micro_vertices > 0 && shared_metadata_faults == 0,
            "shared full/micro ring vertices disagree on deformation metadata");

    const IntersectionPeak full_hits = spring_self_intersections(
        type, *spring->clip, 0, rigid_air_tick);
    const IntersectionPeak micro_hits = spring_self_intersections(
        type, *spring->clip, 1, rigid_air_tick);
    std::printf("SPRING real surface intersections full/micro: ");
    if (full_hits.count == 0)
      std::printf("none");
    else
      std::printf("%d@%d%s (%d/%d)", full_hits.count, full_hits.tick / 2,
                  (full_hits.tick & 1) ? ".5" : "", full_hits.station_a,
                  full_hits.station_b);
    std::printf(" / ");
    if (micro_hits.count == 0)
      std::printf("none\n");
    else
      std::printf("%d@%d%s (%d/%d)\n", micro_hits.count,
                  micro_hits.tick / 2, (micro_hits.tick & 1) ? ".5" : "",
                  micro_hits.station_a, micro_hits.station_b);
    require(full_hits.count == 0 && micro_hits.count == 0,
            "spring body runs intersect on the real full or micro surface");

    std::array<int32_t, 2> terrain_worst{INT32_MAX, INT32_MAX};
    std::array<int, 2> terrain_tick{-1, -1};
    for (int t = 0; t <= hold_end_tick; ++t) {
      for (int rung = 0; rung < 2; ++rung) {
        const int32_t y = to_mm(spring->samples[t].rung_min_y_fx[rung]);
        if (y < terrain_worst[rung]) {
          terrain_worst[rung] = y;
          terrain_tick[rung] = t;
        }
      }
    }
    std::printf("SPRING terrain full/micro: %d mm at %d%s / %d mm at %d%s "
                "(declared bite %d mm)\n", terrain_worst[0],
                terrain_tick[0] / 2, (terrain_tick[0] & 1) ? ".5" : "",
                terrain_worst[1], terrain_tick[1] / 2,
                (terrain_tick[1] & 1) ? ".5" : "",
                zixx::kSpringDeclaredBiteMm);
    // The loaded coil is entitled to kSpringDeclaredLoadedBiteMm, not the
    // resting bite: it is standing on its tail with its whole weight through a
    // small contact patch. Both rungs must still BITE -- a body resting at
    // exactly zero reads as hovering, so too little is as much a fault as too
    // much.
    require(terrain_worst[0] >= -zixx::kSpringDeclaredLoadedBiteMm &&
                terrain_worst[0] <= -zixx::kSpringDeclaredBiteMm &&
                terrain_worst[1] >= -zixx::kSpringDeclaredLoadedBiteMm &&
                terrain_worst[1] <= -zixx::kSpringDeclaredBiteMm,
            "spring left its accepted authored full/micro ground-bite envelope");
  }

  // Direction #19's fixed support and ground-bite contract covers every real
  // 60 Hz sample through exact grounded key 22, not only the deepest hold. The
  // support path is measured from the posed station-14 joint; contact comes
  // independently from every posed full/micro vertex.
  if (spring) {
    constexpr int kPreLiftEndTick = 2 * zixx::kSaltoSpringReleasePoseKey;
    const PosedSample& support_rest = spring->samples[0];
    int32_t support_x_drift = 0;
    int support_x_tick = 0;
    int32_t support_z_drift = 0;
    int support_z_tick = 0;
    int32_t support_y_low = INT32_MAX;
    int support_y_low_tick = -1;
    int32_t support_y_high = INT32_MIN;
    int support_y_high_tick = -1;
    int32_t support_target_error = 0;
    int support_target_error_tick = -1;
    std::array<int32_t, 2> contact_deepest{INT32_MAX, INT32_MAX};
    std::array<int32_t, 2> contact_shallowest{INT32_MIN, INT32_MIN};
    std::array<int, 2> contact_deepest_tick{-1, -1};
    std::array<int, 2> contact_shallowest_tick{-1, -1};
    for (int t = 0; t <= kPreLiftEndTick; ++t) {
      const PosedSample& s = spring->samples[t];
      const int32_t dx = std::abs(
          s.support_x_mm - support_rest.support_x_mm);
      if (dx > support_x_drift) {
        support_x_drift = dx;
        support_x_tick = t;
      }
      const int32_t dz = std::abs(
          s.support_z_mm - support_rest.support_z_mm);
      if (dz > support_z_drift) {
        support_z_drift = dz;
        support_z_tick = t;
      }
      const int32_t support_dy = s.support_y_mm - support_rest.support_y_mm;
      if (support_dy < support_y_low) {
        support_y_low = support_dy;
        support_y_low_tick = t;
      }
      if (support_dy > support_y_high) {
        support_y_high = support_dy;
        support_y_high_tick = t;
      }
      int32_t expected_support_y = 0;
      if ((t & 1) == 0) {
        const int key = t / 2;
        expected_support_y = zixx::spring_support_target_y(
            zixx::spring_shared_entry_amount(key),
            zixx::spring_shared_squash_amount(key));
      } else {
        expected_support_y = zixx::spring_shared_midpoint_target_y(
            t / 2, zixx::kSaltoCompressHoldEndKey);
      }
      const int32_t target_error =
          std::abs(support_dy - expected_support_y);
      if (target_error > support_target_error) {
        support_target_error = target_error;
        support_target_error_tick = t;
      }
      for (int rung = 0; rung < 2; ++rung) {
        const int32_t y = to_mm(s.rung_min_y_fx[rung]);
        if (y < contact_deepest[rung]) {
          contact_deepest[rung] = y;
          contact_deepest_tick[rung] = t;
        }
        if (y > contact_shallowest[rung]) {
          contact_shallowest[rung] = y;
          contact_shallowest_tick[rung] = t;
        }
      }
    }
    std::printf("SPRING pre-lift station-14 support: X drift %d mm at %d%s, "
                "Z drift %d mm at %d%s, Y delta %d at %d%s .. %d at "
                "%d%s mm, target error %d at %d%s; full surface %d@%d%s..%d@%d%s, micro "
                "%d@%d%s..%d@%d%s through key 22 + half-keys\n",
                support_x_drift, support_x_tick / 2,
                (support_x_tick & 1) ? ".5" : "", support_z_drift,
                support_z_tick / 2, (support_z_tick & 1) ? ".5" : "",
                support_y_low, support_y_low_tick / 2,
                (support_y_low_tick & 1) ? ".5" : "", support_y_high,
                support_y_high_tick / 2,
                (support_y_high_tick & 1) ? ".5" : "",
                support_target_error, support_target_error_tick / 2,
                (support_target_error_tick & 1) ? ".5" : "",
                contact_deepest[0], contact_deepest_tick[0] / 2,
                (contact_deepest_tick[0] & 1) ? ".5" : "",
                contact_shallowest[0], contact_shallowest_tick[0] / 2,
                (contact_shallowest_tick[0] & 1) ? ".5" : "",
                contact_deepest[1], contact_deepest_tick[1] / 2,
                (contact_deepest_tick[1] & 1) ? ".5" : "",
                contact_shallowest[1], contact_shallowest_tick[1] / 2,
                (contact_shallowest_tick[1] & 1) ? ".5" : "");
    for (int t = 0; t <= kPreLiftEndTick; ++t) {
      const PosedSample& s = spring->samples[t];
      std::printf("SPRING contact sample %d%s: support dY %d, full/micro %d/%d mm\n",
                  t / 2, (t & 1) ? ".5" : "",
                  s.support_y_mm - support_rest.support_y_mm,
                  to_mm(s.rung_min_y_fx[0]), to_mm(s.rung_min_y_fx[1]));
    }
    require(support_x_drift <= 1 && support_z_drift <= 1 &&
                support_target_error <= 1,
            "spring station-14 support left its authored per-sample path");
    require(contact_deepest[0] >= -zixx::kSpringDeclaredLoadedBiteMm &&
                contact_shallowest[0] <= 0 &&
                contact_deepest[1] >= -zixx::kSpringDeclaredLoadedBiteMm &&
                contact_shallowest[1] <= 0,
            "spring full/micro pre-lift samples left the declared ground bite");
  }

  // Full consumers own station-derived roots on every pre-lift half-key.
  // Programmable jumps additionally own every landing/recovery midpoint root,
  // because interpolation cannot keep a changing bent chain on its 3D support.
  // Entry keys 1.5 and 4.5 plus the four release bridges replace complete
  // quaternion and deformation channels. The local-body release slice owns no
  // root, because trajectory belongs to its ChoreoRoot consumer.
  constexpr uint8_t kOwnedQuatsDeform =
      zc::kMidpointQuatsAuthored | zc::kMidpointDeformAuthored;
  // entry_owned_end: exclusive upper bound of the schedule-authored arming
  // half-keys (Direction 23 authors EVERY arming+hold midpoint; a slice that
  // begins mid-arming carries the tail of that ownership).
  auto check_midpoint_authorship = [&](const char* name,
                                       const zc::PresentationMidpointAuthorship& a,
                                       int first, bool full_consumer,
                                       int entry_owned_end) {
    const bool landing_root_owner =
        a.slot_id == zixx::kSlotJumpOne ||
        a.slot_id == zixx::kSlotJumpMulti;
    int landing_root_begin = -1;
    int landing_root_end = -1;
    if (landing_root_owner) {
      const int turns = a.slot_id == zixx::kSlotJumpOne ? 1 : 3;
      const zixx::JumpPhases phase = zixx::zixx_jump_phases(
          zixx::zixx_jump_plan(a.slot_id, turns));
      landing_root_begin = phase.landing_key;
      landing_root_end = phase.last_key;
    }
    const int needed = landing_root_owner
                           ? landing_root_end
                           : (full_consumer
                                  ? zixx::kSaltoSpringReleasePoseKey
                                  : first + 4);
    bool exact = a.channels.size() > static_cast<size_t>(needed - 1);
    int owned = 0;
    int expected_owned = 0;
    for (size_t i = 0; i < a.channels.size(); ++i) {
      uint8_t expected = 0;
      if ((full_consumer &&
           i < static_cast<size_t>(zixx::kSaltoSpringReleasePoseKey)) ||
          (landing_root_owner &&
           i >= static_cast<size_t>(landing_root_begin) &&
           i < static_cast<size_t>(landing_root_end)))
        expected = zc::kMidpointRootAuthored;
      if (i < static_cast<size_t>(entry_owned_end) ||
          (i >= static_cast<size_t>(first) &&
           i < static_cast<size_t>(first + 4)))
        expected |= kOwnedQuatsDeform;
      if (expected != 0) ++expected_owned;
      if (a.channels[i] != expected) exact = false;
      if (a.channels[i] != 0) ++owned;
    }
    const int landing_owned =
        landing_root_owner ? landing_root_end - landing_root_begin : 0;
    std::printf("MIDPOINT provenance %s: %d owned segments, "
                "pre-lift root span %d, landing root span %d, exact=%d\n",
                name, owned,
                full_consumer ? zixx::kSaltoSpringReleasePoseKey : 0,
                landing_owned, exact ? 1 : 0);
    require(exact && owned == expected_owned,
            "midpoint per-channel provenance drifted");
  };

  zc::PresentationMidpointAuthorship golden_owned;
  const zc::Clip golden_source = zixx::build_attack(false, &golden_owned);
  check_midpoint_authorship("golden", golden_owned,
                            zixx::kSaltoCompressHoldEndKey, true,
                            zixx::kSaltoCompressHoldEndKey);

  zc::PresentationMidpointAuthorship local_owned;
  const zc::Clip local_source = zixx::build_attack(true, &local_owned);
  const zc::PresentationMidpointAuthorship compression_owned =
      zixx::slice_midpoint_authorship(
          local_owned, zixx::kSlotAtkCompress,
          zixx::kAtkCompressSliceFirstKey,
          zixx::kAtkCompressSliceLastKey);
  bool compression_ownership_exact =
      compression_owned.slot_id == zixx::kSlotAtkCompress &&
      compression_owned.channels.size() == static_cast<size_t>(
          zixx::kAtkCompressSliceLastKey -
          zixx::kAtkCompressSliceFirstKey + 1);
  int compression_owned_segments = 0;
  // Direction 23: every arming half-key is schedule-authored, so the whole
  // compression slice owns its quats/deform channels.
  for (size_t key = 0; key < compression_owned.channels.size(); ++key) {
    if (compression_owned.channels[key] != kOwnedQuatsDeform)
      compression_ownership_exact = false;
    if (compression_owned.channels[key] != 0) ++compression_owned_segments;
  }
  std::printf("MIDPOINT provenance compression slice: %d owned segments, "
              "all schedule-authored exact=%d\n",
              compression_owned_segments,
              compression_ownership_exact ? 1 : 0);
  require(compression_ownership_exact &&
              compression_owned_segments ==
                  static_cast<int>(compression_owned.channels.size()),
          "compression midpoint provenance did not remap into slot 10");

  const zc::PresentationMidpointAuthorship release_owned =
      zixx::slice_midpoint_authorship(
          local_owned, zixx::kSlotAtkRelease,
          zixx::kAtkCompressSliceLastKey, zixx::kSaltoCoilPoseKey);
  // The release slice begins at the last compression key, so its local
  // half-key 0 is attack midpoint (hold_end - 1) + 0.5 -- schedule-authored.
  check_midpoint_authorship("release slice", release_owned, 1, false, 1);

  // Retiming changes phase locations, never support obligations. The default-only
  // 1.5/4.5 complete poses stay absent, a four-key release owns its complete
  // plan-local bridges, attacks own every pre-lift root, and jumps additionally
  // own every landing/recovery root. Compile all adversarial plans together so
  // malformed or uninitialized provenance cannot hide behind source-only checks.
  constexpr uint16_t kSyntheticRetimedAttackSlot = 50;
  constexpr uint16_t kSyntheticRetimedJumpSlot = 51;
  constexpr uint16_t kSyntheticAttackRelease2Slot = 52;
  constexpr uint16_t kSyntheticAttackRelease3Slot = 53;
  constexpr uint16_t kSyntheticJumpRelease2Slot = 54;
  constexpr uint16_t kSyntheticJumpRelease3Slot = 55;
  constexpr uint16_t kSyntheticJumpShortRecoverySlot = 56;
  constexpr uint16_t kSyntheticJumpLongRecoverySlot = 57;
  constexpr uint16_t kSyntheticCompress48Slot = 58;
  constexpr uint16_t kSyntheticAttackRelease0Slot = 59;
  constexpr uint16_t kSyntheticJumpRelease0Slot = 60;

  zc::AttackPlan retimed_attack =
      zixx::zixx_variant_plan(zixx::kSlotAtkDummy);
  retimed_attack.compress_hold_keys += 1;
  const zixx::AttackVariantPhases retimed_attack_phase =
      zixx::zixx_attack_variant_phases(retimed_attack, true);
  zc::PresentationMidpointAuthorship retimed_attack_owned;
  const zc::Clip retimed_attack_source = zixx::build_attack_variant(
      kSyntheticRetimedAttackSlot, retimed_attack, true,
      &retimed_attack_owned);

  zixx::JumpPlan retimed_jump = zixx::zixx_jump_plan(
      kSyntheticRetimedJumpSlot, 1);
  retimed_jump.compress_hold_keys += 1;
  const zixx::JumpPhases retimed_jump_phase =
      zixx::zixx_jump_phases(retimed_jump);
  zc::PresentationMidpointAuthorship retimed_jump_owned;
  const zc::Clip retimed_jump_source =
      zixx::build_jump(retimed_jump, &retimed_jump_owned);

  auto midpoint_contract_exact = [&](
      const char* name, const zc::Clip& clip,
      const zc::PresentationMidpointAuthorship& owned, int pre_lift_end,
      int release_pose_begin, int release_pose_count,
      int landing_root_begin, int landing_root_end,
      bool expect_default_entry_poses) {
    bool exact = owned.slot_id == clip.slot_id &&
                 owned.channels.size() ==
                     static_cast<size_t>(clip.frame_count) &&
                 clip.mid_quats.size() ==
                     static_cast<size_t>(clip.frame_count) *
                         type.bank.bone_count &&
                 clip.mid_root.size() ==
                     static_cast<size_t>(clip.frame_count) * 3 &&
                 clip.mid_deform.size() ==
                     static_cast<size_t>(clip.frame_count);
    int root_segments = 0;
    int pose_segments = 0;
    for (size_t key = 0; key < owned.channels.size(); ++key) {
      uint8_t expected = 0;
      if (key < static_cast<size_t>(pre_lift_end) ||
          (landing_root_begin >= 0 &&
           key >= static_cast<size_t>(landing_root_begin) &&
           key < static_cast<size_t>(landing_root_end)))
        expected |= zc::kMidpointRootAuthored;
      if ((expect_default_entry_poses &&
           key < static_cast<size_t>(zixx::kSaltoCompressHoldEndKey)) ||
          (release_pose_count > 0 &&
           key >= static_cast<size_t>(release_pose_begin) &&
           key < static_cast<size_t>(release_pose_begin + release_pose_count)))
        expected |= kOwnedQuatsDeform;
      if (owned.channels[key] != expected) exact = false;
      if ((owned.channels[key] & zc::kMidpointRootAuthored) != 0)
        ++root_segments;
      if ((owned.channels[key] & kOwnedQuatsDeform) == kOwnedQuatsDeform)
        ++pose_segments;
    }
    const bool default_entry_poses_exact =
        owned.channels.size() > static_cast<size_t>(
                                    zixx::kSpringEntryOwnedMidpointKey) &&
        (((owned.channels[zixx::kSpringEarlyEntryOwnedMidpointKey] &
           kOwnedQuatsDeform) == kOwnedQuatsDeform) ==
         expect_default_entry_poses) &&
        (((owned.channels[zixx::kSpringEntryOwnedMidpointKey] &
           kOwnedQuatsDeform) == kOwnedQuatsDeform) ==
         expect_default_entry_poses);
    exact = exact && default_entry_poses_exact;
    std::printf("MIDPOINT retimed %s: slot/count exact=%d, roots=%d, "
                "plan-release poses=%d, default entry poses %s exact=%d\n",
                name, exact ? 1 : 0, root_segments, pose_segments,
                expect_default_entry_poses ? "present" : "absent",
                default_entry_poses_exact ? 1 : 0);
    require(exact, "retimed midpoint channel/timeline ownership drifted");
    return exact;
  };

  midpoint_contract_exact(
      "attack", retimed_attack_source, retimed_attack_owned,
      retimed_attack_phase.release_end, retimed_attack_phase.hold_end,
      zixx::kSpringReleaseMidpointCount, -1, -1, false);
  midpoint_contract_exact(
      "jump", retimed_jump_source, retimed_jump_owned,
      retimed_jump_phase.launch_key, retimed_jump_phase.hold_end,
      zixx::kSpringReleaseMidpointCount, retimed_jump_phase.landing_key,
      retimed_jump_phase.last_key, false);

  zc::AttackPlan attack_release0 =
      zixx::zixx_variant_plan(zixx::kSlotAtkDummy);
  attack_release0.release_keys = 0;
  const zixx::AttackVariantPhases attack_release0_phase =
      zixx::zixx_attack_variant_phases(attack_release0, true);
  zc::PresentationMidpointAuthorship attack_release0_owned;
  const zc::Clip attack_release0_source = zixx::build_attack_variant(
      kSyntheticAttackRelease0Slot, attack_release0, true,
      &attack_release0_owned);
  midpoint_contract_exact(
      "attack release-0", attack_release0_source, attack_release0_owned,
      attack_release0_phase.release_end, -1, 0, -1, -1, false);

  zixx::JumpPlan jump_release0 = zixx::zixx_jump_plan(
      kSyntheticJumpRelease0Slot, 1);
  jump_release0.release_keys = 0;
  const zixx::JumpPhases jump_release0_phase =
      zixx::zixx_jump_phases(jump_release0);
  zc::PresentationMidpointAuthorship jump_release0_owned;
  const zc::Clip jump_release0_source =
      zixx::build_jump(jump_release0, &jump_release0_owned);
  midpoint_contract_exact(
      "jump release-0", jump_release0_source, jump_release0_owned,
      jump_release0_phase.launch_key, -1, 0,
      jump_release0_phase.landing_key, jump_release0_phase.last_key, false);

  zc::AttackPlan attack_release2 =
      zixx::zixx_variant_plan(zixx::kSlotAtkDummy);
  attack_release2.release_keys = 2;
  const zixx::AttackVariantPhases attack_release2_phase =
      zixx::zixx_attack_variant_phases(attack_release2, true);
  zc::PresentationMidpointAuthorship attack_release2_owned;
  const zc::Clip attack_release2_source = zixx::build_attack_variant(
      kSyntheticAttackRelease2Slot, attack_release2, true,
      &attack_release2_owned);
  midpoint_contract_exact(
      "attack release-2", attack_release2_source, attack_release2_owned,
      attack_release2_phase.release_end, -1, 0, -1, -1, false);

  zc::AttackPlan attack_release3 =
      zixx::zixx_variant_plan(zixx::kSlotAtkDummy);
  attack_release3.release_keys = 3;
  const zixx::AttackVariantPhases attack_release3_phase =
      zixx::zixx_attack_variant_phases(attack_release3, true);
  zc::PresentationMidpointAuthorship attack_release3_owned;
  const zc::Clip attack_release3_source = zixx::build_attack_variant(
      kSyntheticAttackRelease3Slot, attack_release3, true,
      &attack_release3_owned);
  midpoint_contract_exact(
      "attack release-3", attack_release3_source, attack_release3_owned,
      attack_release3_phase.release_end, -1, 0, -1, -1, false);

  zixx::JumpPlan jump_release2 = zixx::zixx_jump_plan(
      kSyntheticJumpRelease2Slot, 1);
  jump_release2.release_keys = 2;
  const zixx::JumpPhases jump_release2_phase =
      zixx::zixx_jump_phases(jump_release2);
  zc::PresentationMidpointAuthorship jump_release2_owned;
  const zc::Clip jump_release2_source =
      zixx::build_jump(jump_release2, &jump_release2_owned);
  midpoint_contract_exact(
      "jump release-2", jump_release2_source, jump_release2_owned,
      jump_release2_phase.launch_key, -1, 0,
      jump_release2_phase.landing_key, jump_release2_phase.last_key, false);

  zixx::JumpPlan jump_release3 = zixx::zixx_jump_plan(
      kSyntheticJumpRelease3Slot, 1);
  jump_release3.release_keys = 3;
  const zixx::JumpPhases jump_release3_phase =
      zixx::zixx_jump_phases(jump_release3);
  zc::PresentationMidpointAuthorship jump_release3_owned;
  const zc::Clip jump_release3_source =
      zixx::build_jump(jump_release3, &jump_release3_owned);
  midpoint_contract_exact(
      "jump release-3", jump_release3_source, jump_release3_owned,
      jump_release3_phase.launch_key, -1, 0,
      jump_release3_phase.landing_key, jump_release3_phase.last_key, false);

  zixx::JumpPlan jump_short_recovery = zixx::zixx_jump_plan(
      kSyntheticJumpShortRecoverySlot, 1);
  jump_short_recovery.landing_keys = 3;
  jump_short_recovery.settle_keys = 5;
  const zixx::JumpPhases jump_short_recovery_phase =
      zixx::zixx_jump_phases(jump_short_recovery);
  zc::PresentationMidpointAuthorship jump_short_recovery_owned;
  const zc::Clip jump_short_recovery_source =
      zixx::build_jump(jump_short_recovery, &jump_short_recovery_owned);
  midpoint_contract_exact(
      "jump short recovery", jump_short_recovery_source,
      jump_short_recovery_owned, jump_short_recovery_phase.launch_key,
      jump_short_recovery_phase.hold_end,
      zixx::kSpringReleaseMidpointCount,
      jump_short_recovery_phase.landing_key,
      jump_short_recovery_phase.last_key, true);

  zixx::JumpPlan jump_long_recovery = zixx::zixx_jump_plan(
      kSyntheticJumpLongRecoverySlot, 1);
  jump_long_recovery.landing_keys = 10;
  jump_long_recovery.settle_keys = 22;
  const zixx::JumpPhases jump_long_recovery_phase =
      zixx::zixx_jump_phases(jump_long_recovery);
  zc::PresentationMidpointAuthorship jump_long_recovery_owned;
  const zc::Clip jump_long_recovery_source =
      zixx::build_jump(jump_long_recovery, &jump_long_recovery_owned);
  midpoint_contract_exact(
      "jump long recovery", jump_long_recovery_source,
      jump_long_recovery_owned, jump_long_recovery_phase.launch_key,
      jump_long_recovery_phase.hold_end,
      zixx::kSpringReleaseMidpointCount,
      jump_long_recovery_phase.landing_key,
      jump_long_recovery_phase.last_key, true);

  zc::AttackPlan compress48 =
      zixx::zixx_variant_plan(zixx::kSlotAtkDummy);
  compress48.compress_keys = 48;
  const zixx::AttackVariantPhases compress48_phase =
      zixx::zixx_attack_variant_phases(compress48, true);
  zc::PresentationMidpointAuthorship compress48_owned;
  const zc::Clip compress48_source = zixx::build_attack_variant(
      kSyntheticCompress48Slot, compress48, true, &compress48_owned);
  midpoint_contract_exact(
      "compress-48 attack", compress48_source, compress48_owned,
      compress48_phase.release_end, compress48_phase.hold_end,
      zixx::kSpringReleaseMidpointCount, -1, -1, false);

  zc::ClipBank synthetic_bank;
  synthetic_bank.bone_count = type.bank.bone_count;
  synthetic_bank.bake60 = true;
  synthetic_bank.clips = {
      retimed_attack_source,
      retimed_jump_source,
      attack_release0_source,
      jump_release0_source,
      attack_release2_source,
      attack_release3_source,
      jump_release2_source,
      jump_release3_source,
      jump_short_recovery_source,
      jump_long_recovery_source,
      compress48_source,
  };
  const std::vector<zc::PresentationMidpointAuthorship> synthetic_owned = {
      retimed_attack_owned,
      retimed_jump_owned,
      attack_release0_owned,
      jump_release0_owned,
      attack_release2_owned,
      attack_release3_owned,
      jump_release2_owned,
      jump_release3_owned,
      jump_short_recovery_owned,
      jump_long_recovery_owned,
      compress48_owned,
  };
  zc::CreatureType synthetic_type;
  const char* synthetic_reason = "";
  // compile_creature requires at least one authoring meshlet even though this
  // regression concerns only clips. Supply one inert compiler carrier, then
  // replace its output with the registered full/micro rungs below.
  std::vector<zc::RingPart> synthetic_parts(1);
  synthetic_parts[0].bone = zixx::kBSpine0;
  synthetic_parts[0].caps = zc::kCapTop | zc::kCapBot;
  synthetic_parts[0].rings = {
      {0, fxm(1), 3},
      {fxm(2), fxm(1), 3},
  };
  const bool synthetic_compiled = zc::compile_creature(
      type.skeleton, synthetic_bank, synthetic_parts, synthetic_type,
      &synthetic_reason, synthetic_owned);
  std::printf("RETIMED synthetic bank compile: %d (%s)\n",
              synthetic_compiled ? 1 : 0,
              synthetic_compiled ? "accepted" : synthetic_reason);
  require(synthetic_compiled, "synthetic retimed clip bank did not compile");
  if (synthetic_compiled) {
    // Compilation validates/rebakes the synthetic bank; the real accepted mesh
    // rungs are then attached so scans walk the same full/micro vertices as the
    // registered creature without reconstructing a second authoring mesh.
    synthetic_type.mesh = type.mesh;
    synthetic_type.micro = type.micro;
    synthetic_type.bound_radius = type.bound_radius;
    synthetic_type.micro_error = type.micro_error;
    synthetic_type.splat_error = type.splat_error;
    synthetic_type.glint_error = type.glint_error;
    synthetic_type.page_set = type.page_set;
    synthetic_type.page_direct = type.page_direct;
  }

  const auto synthetic_clip = [&](int slot) -> const zc::Clip* {
    if (!synthetic_compiled) return nullptr;
    for (const zc::Clip& clip : synthetic_type.bank.clips)
      if (clip.slot_id == slot) return &clip;
    return nullptr;
  };
  const auto compiled_owns_exact_bytes = [&] (
      const zc::Clip& source,
      const zc::PresentationMidpointAuthorship& owned) {
    const zc::Clip* compiled = synthetic_clip(source.slot_id);
    bool exact = compiled != nullptr &&
                 compiled->mid_quats.size() == source.mid_quats.size() &&
                 compiled->mid_root.size() == source.mid_root.size() &&
                 compiled->mid_deform.size() == source.mid_deform.size();
    if (!exact) return false;
    for (size_t key = 0; key < owned.channels.size(); ++key) {
      const uint8_t channels = owned.channels[key];
      if ((channels & zc::kMidpointQuatsAuthored) != 0 &&
          std::memcmp(&compiled->mid_quats[key * type.bank.bone_count],
                      &source.mid_quats[key * type.bank.bone_count],
                      type.bank.bone_count * sizeof(zc::quat16)) != 0)
        exact = false;
      if ((channels & zc::kMidpointRootAuthored) != 0 &&
          std::memcmp(&compiled->mid_root[key * 3],
                      &source.mid_root[key * 3],
                      3 * sizeof(int32_t)) != 0)
        exact = false;
      if ((channels & zc::kMidpointDeformAuthored) != 0 &&
          std::memcmp(&compiled->mid_deform[key], &source.mid_deform[key],
                      sizeof(zc::DeformSample)) != 0)
        exact = false;
    }
    return exact;
  };
  require(compiled_owns_exact_bytes(retimed_attack_source,
                                    retimed_attack_owned) &&
              compiled_owns_exact_bytes(retimed_jump_source,
                                        retimed_jump_owned) &&
              compiled_owns_exact_bytes(attack_release0_source,
                                        attack_release0_owned) &&
              compiled_owns_exact_bytes(jump_release0_source,
                                        jump_release0_owned),
          "compiled retimed bank regenerated an explicitly owned channel");

  const auto exact_deform_endpoint = [](const zc::Clip& clip, int endpoint) {
    return endpoint >= 0 && endpoint < clip.frame_count &&
           clip.deform.size() == static_cast<size_t>(clip.frame_count) &&
           clip.deform[0].flatten == clip.deform[endpoint].flatten &&
           clip.deform[0].spread == clip.deform[endpoint].spread;
  };
  struct ReleaseCase {
    const char* name;
    const zc::Clip* source;
    int hold_end;
    int endpoint;
    bool motion_endpoint_exact;
  };
  const std::array<ReleaseCase, 6> release_cases{{
      {"attack-0", &attack_release0_source, attack_release0_phase.hold_end,
       attack_release0_phase.release_end,
       zixx::zixx_plan_spring_entry_amount(
           attack_release0, attack_release0_phase.release_end) == 0 &&
           zixx::zixx_plan_spring_amount(
               attack_release0, attack_release0_phase.release_end) == 0},
      {"jump-0", &jump_release0_source, jump_release0_phase.hold_end,
       jump_release0_phase.launch_key,
       zixx::zixx_jump_motion_sample(
           jump_release0, jump_release0_phase.launch_key).entry == 0 &&
           zixx::zixx_jump_motion_sample(
               jump_release0, jump_release0_phase.launch_key).spring == 0},
      {"attack-2", &attack_release2_source, attack_release2_phase.hold_end,
       attack_release2_phase.release_end,
       zixx::zixx_plan_spring_entry_amount(
           attack_release2, attack_release2_phase.release_end) == 0 &&
           zixx::zixx_plan_spring_amount(
               attack_release2, attack_release2_phase.release_end) == 0},
      {"attack-3", &attack_release3_source, attack_release3_phase.hold_end,
       attack_release3_phase.release_end,
       zixx::zixx_plan_spring_entry_amount(
           attack_release3, attack_release3_phase.release_end) == 0 &&
           zixx::zixx_plan_spring_amount(
               attack_release3, attack_release3_phase.release_end) == 0},
      {"jump-2", &jump_release2_source, jump_release2_phase.hold_end,
       jump_release2_phase.launch_key,
       zixx::zixx_jump_motion_sample(
           jump_release2, jump_release2_phase.launch_key).entry == 0 &&
           zixx::zixx_jump_motion_sample(
               jump_release2, jump_release2_phase.launch_key).spring == 0},
      {"jump-3", &jump_release3_source, jump_release3_phase.hold_end,
       jump_release3_phase.launch_key,
       zixx::zixx_jump_motion_sample(
           jump_release3, jump_release3_phase.launch_key).entry == 0 &&
           zixx::zixx_jump_motion_sample(
               jump_release3, jump_release3_phase.launch_key).spring == 0},
  }};
  for (const ReleaseCase& release : release_cases) {
    const zc::Clip* compiled = synthetic_clip(release.source->slot_id);
    bool endpoint_exact = release.motion_endpoint_exact &&
                          key_pose_equal(*release.source, 0, release.endpoint,
                                         type.bank.bone_count, true) &&
                          exact_deform_endpoint(*release.source,
                                                release.endpoint) &&
                          compiled != nullptr &&
                          key_pose_equal(*compiled, 0, release.endpoint,
                                         type.bank.bone_count, true) &&
                          exact_deform_endpoint(*compiled,
                                                release.endpoint);
    int32_t release_step_mm = INT32_MAX;
    int release_step_tick = -1;
    if (compiled != nullptr) {
      const ClipScan scan = scan_clip(synthetic_type, *compiled, stations);
      // A zero-duration release has no tick after hold_end; scan its final
      // half-segment into the aliased launch endpoint instead.
      const int begin_tick = release.endpoint == release.hold_end
                                 ? std::max(1, 2 * release.endpoint - 1)
                                 : 2 * release.hold_end + 1;
      const StationStepMaximum step = station_step_max_mm(
          scan, begin_tick, 2 * release.endpoint);
      release_step_mm = step.mm;
      release_step_tick = step.tick;
      endpoint_exact = endpoint_exact &&
                       step.mm <= zixx::kJumpMaxStationStepMm;
    }
    std::printf("RETIMED release %s: motion identity=%d, source+compiled "
                "grounded endpoint exact=%d, compiled max step %d mm at %d%s\n",
                release.name, release.motion_endpoint_exact ? 1 : 0,
                endpoint_exact ? 1 : 0, release_step_mm,
                release_step_tick / 2,
                (release_step_tick & 1) ? ".5" : "");
    require(endpoint_exact,
            "short release missed grounded endpoint or continuity bound");
  }

  struct SyntheticContactRange {
    int32_t deepest_mm = INT32_MAX;
    int deepest_tick = -1;
    int32_t highest_mm = INT32_MIN;
  };
  const auto synthetic_contact_range = [](
      const ClipScan& scan, int rung, int begin_tick, int end_tick) {
    SyntheticContactRange range;
    begin_tick = std::max(begin_tick, 0);
    end_tick = std::min(end_tick,
                        static_cast<int>(scan.samples.size()) - 1);
    for (int tick = begin_tick; tick <= end_tick; ++tick) {
      const int32_t y = to_mm(scan.samples[tick].rung_min_y_fx[rung]);
      if (y < range.deepest_mm) {
        range.deepest_mm = y;
        range.deepest_tick = tick;
      }
      range.highest_mm = std::max(range.highest_mm, y);
    }
    return range;
  };
  const auto check_compiled_attack_grounding = [&] (
      const char* name, const zc::AttackPlan& plan,
      const zixx::AttackVariantPhases& phase) {
    const zc::Clip* compiled = synthetic_clip(kSyntheticRetimedAttackSlot);
    // The fixture is "the shared timing with ONE extra hold key". Transcribing
    // its numbers meant the fixture silently stopped matching the moment the
    // shared timing was re-authored.
    bool exact = compiled != nullptr &&
                 plan.compress_keys == zixx::kSaltoCompressEndKey &&
                 plan.compress_hold_keys ==
                     zixx::kSaltoCompressHoldEndKey -
                         zixx::kSaltoCompressEndKey + 1 &&
                 plan.release_keys == zixx::kSpringReleaseMidpointCount;
    int32_t support_x_drift = 0;
    int32_t support_z_drift = 0;
    int32_t support_target_error = 0;
    int32_t max_support_step = 0;
    int32_t max_expected_support_step = 0;
    int32_t max_station_step = INT32_MAX;
    std::array<SyntheticContactRange, 2> contact{};
    if (compiled != nullptr) {
      const ClipScan scan = scan_clip(synthetic_type, *compiled, stations);
      const PosedSample& rest = scan.samples[0];
      int32_t previous_expected = 0;
      for (int tick = 0; tick <= 2 * phase.release_end; ++tick) {
        const PosedSample& sample = scan.samples[tick];
        const int key = tick / 2;
        int32_t expected_y = 0;
        if ((tick & 1) == 0) {
          expected_y = zixx::spring_support_target_y(
              zixx::zixx_plan_spring_entry_amount(plan, key),
              zixx::zixx_plan_spring_amount(plan, key));
        } else {
          expected_y = zixx::spring_plan_midpoint_target_y(
              key, phase.hold_end, false,
              plan.release_keys == zixx::kSpringReleaseMidpointCount,
              zixx::zixx_plan_spring_entry_amount(plan, key),
              zixx::zixx_plan_spring_amount(plan, key),
              zixx::zixx_plan_spring_entry_amount(plan, key + 1),
              zixx::zixx_plan_spring_amount(plan, key + 1));
        }
        support_x_drift = std::max(
            support_x_drift,
            std::abs(sample.support_x_mm - rest.support_x_mm));
        support_z_drift = std::max(
            support_z_drift,
            std::abs(sample.support_z_mm - rest.support_z_mm));
        support_target_error = std::max(
            support_target_error,
            std::abs((sample.support_y_mm - rest.support_y_mm) - expected_y));
        if (tick > 0) {
          const PosedSample& previous = scan.samples[tick - 1];
          const int64_t dx = sample.support_x_mm - previous.support_x_mm;
          const int64_t dy = sample.support_y_mm - previous.support_y_mm;
          const int64_t dz = sample.support_z_mm - previous.support_z_mm;
          max_support_step = std::max(
              max_support_step,
              static_cast<int32_t>(zref::isqrt_u64(
                  static_cast<uint64_t>(dx * dx + dy * dy + dz * dz))));
          max_expected_support_step = std::max(
              max_expected_support_step,
              std::abs(expected_y - previous_expected));
        }
        previous_expected = expected_y;
      }
      const StationStepMaximum continuity = station_step_max_mm(
          scan, 1, 2 * phase.release_end);
      max_station_step = continuity.mm;
      for (int rung = 0; rung < 2; ++rung)
        contact[rung] = synthetic_contact_range(
            scan, rung, 0, 2 * phase.release_end);
      exact = exact &&
              key_pose_equal(*compiled, 0, phase.release_end,
                             type.bank.bone_count, true) &&
              exact_deform_endpoint(*compiled, phase.release_end) &&
              support_x_drift <= 1 && support_z_drift <= 1 &&
              support_target_error <= 1 &&
              max_support_step <= max_expected_support_step + 3 &&
              max_station_step <= zixx::kJumpMaxStationStepMm;
      for (int rung = 0; rung < 2; ++rung)
        exact = exact &&
                contact[rung].deepest_mm >=
                    -zixx::kSpringDeclaredLoadedBiteMm &&
                contact[rung].highest_mm <= 0;
    }
    std::printf("RETIMED attack %s 12/7/4: exact=%d, station-14 X/Z drift "
                "%d/%d mm, target error %d mm, support step %d/%d mm, "
                "full/micro contact %d..%d/%d..%d mm, station step %d mm\n",
                name, exact ? 1 : 0, support_x_drift, support_z_drift,
                support_target_error, max_support_step,
                max_expected_support_step, contact[0].deepest_mm,
                contact[0].highest_mm, contact[1].deepest_mm,
                contact[1].highest_mm, max_station_step);
    require(exact,
            "compiled retimed attack support/contact/continuity contract drifted");
  };
  check_compiled_attack_grounding(
      "hold+1", retimed_attack, retimed_attack_phase);

  const auto check_compiled_jump_landing = [&] (
      const char* name, const zixx::JumpPlan& plan,
      const zixx::JumpPhases& phase) {
    const zc::Clip* compiled = synthetic_clip(plan.slot);
    bool exact = compiled != nullptr;
    int32_t max_step = INT32_MAX;
    std::array<SyntheticContactRange, 2> impact{};
    std::array<SyntheticContactRange, 2> handoff{};
    std::array<SyntheticContactRange, 2> settle{};
    if (compiled != nullptr) {
      const ClipScan scan = scan_clip(synthetic_type, *compiled, stations);
      const int recovery_keys = plan.landing_keys + plan.settle_keys;
      const int handoff_relative =
          std::min(zixx::kJumpLandingSupportHandoffEnd, recovery_keys);
      const int handoff_end = 2 * (phase.landing_key + handoff_relative);
      const int settle_begin = std::min(
          handoff_end + 1, 2 * phase.last_key);
      const StationStepMaximum continuity = station_step_max_mm(
          scan, 2 * phase.landing_key, 2 * phase.last_key);
      max_step = continuity.mm;
      exact = exact &&
              key_pose_equal(*compiled, 0, phase.last_key,
                             type.bank.bone_count, true) &&
              zixx::jump_landing_surface_bias_mm(plan, recovery_keys) == 0 &&
              continuity.mm <= zixx::kJumpMaxStationStepMm;
      for (int rung = 0; rung < 2; ++rung) {
        impact[rung] = synthetic_contact_range(
            scan, rung, 2 * phase.landing_key,
            2 * (phase.landing_key + 1));
        handoff[rung] = synthetic_contact_range(
            scan, rung, 2 * (phase.landing_key + 1) + 1, handoff_end);
        settle[rung] = synthetic_contact_range(
            scan, rung, settle_begin, 2 * phase.last_key);
        const SyntheticContactRange whole = synthetic_contact_range(
            scan, rung, 2 * phase.landing_key, 2 * phase.last_key);
        exact = exact &&
                whole.deepest_mm >=
                    -(zixx::kJumpLandingLoadedBiteMm +
                      kRetimedLandingContactRoundingMm) &&
                impact[rung].deepest_mm >=
                    -(zixx::kJumpLandingLoadedBiteMm +
                      kRetimedLandingContactRoundingMm) &&
                impact[rung].highest_mm <= -zixx::kJumpImpactMinBiteMm &&
                handoff[rung].deepest_mm >=
                    -(zixx::kJumpLandingLoadedBiteMm +
                      kRetimedLandingContactRoundingMm) &&
                handoff[rung].highest_mm <= 0 &&
                settle[rung].deepest_mm >=
                    -(zixx::kJumpLandingLoadedBiteMm +
                      kRetimedLandingContactRoundingMm) &&
                settle[rung].highest_mm <= 0;
      }
    }
    std::printf("RETIMED landing %s: exact=%d, full impact/handoff/settle "
                "%d..%d/%d..%d/%d..%d mm, micro "
                "%d..%d/%d..%d/%d..%d mm, max step=%d mm, "
                "settle deepest ticks=%d%s/%d%s\n",
                name, exact ? 1 : 0,
                impact[0].deepest_mm, impact[0].highest_mm,
                handoff[0].deepest_mm, handoff[0].highest_mm,
                settle[0].deepest_mm, settle[0].highest_mm,
                impact[1].deepest_mm, impact[1].highest_mm,
                handoff[1].deepest_mm, handoff[1].highest_mm,
                settle[1].deepest_mm, settle[1].highest_mm, max_step,
                settle[0].deepest_tick / 2,
                (settle[0].deepest_tick & 1) ? ".5" : "",
                settle[1].deepest_tick / 2,
                (settle[1].deepest_tick & 1) ? ".5" : "");
    require(exact,
            "compiled retimed jump landing/contact/recovery contract drifted");
  };
  check_compiled_jump_landing(
      "hold+1", retimed_jump, retimed_jump_phase);
  check_compiled_jump_landing(
      "short 3+5", jump_short_recovery, jump_short_recovery_phase);
  check_compiled_jump_landing(
      "long 10+22", jump_long_recovery, jump_long_recovery_phase);

  const auto angle_distance = [](int32_t a, int32_t b) {
    return std::abs(static_cast<int32_t>(static_cast<int16_t>(
        static_cast<uint16_t>((b - a) & 0xFFFF))));
  };
  // OWNER DIRECTION 20 #1/#3. The rejected route interpolated the four authored
  // poses as independent smoothstep legs, so every station's speed fell to ZERO
  // at absorb and again at assembled, and one integer key was additionally
  // snapped onto an off-route override table. That is a rigid, plate-snapping
  // arming with a 30 Hz judder in it. The route is now one C1 spline, so the
  // property worth protecting is no longer "the override table is exact" but
  // "NOWHERE on the whole arming does a station jump or stall". Sweep it all.
  int32_t generic_seam_step = 0;
  int32_t route_min_move = INT32_MAX;
  int32_t route_max_move = 0;
  for (int station = 0; station < zixx::kStanceSlopes; ++station) {
    for (int arm = 1; arm <= 1000; ++arm) {
      generic_seam_step = std::max(
          generic_seam_step,
          angle_distance(zixx::spring_route_heading(station, arm - 1),
                         zixx::spring_route_heading(station, arm)));
    }
    // OWNER DIRECTION 24 #1: the tail stations are PLANTED -- all four knots
    // authored identical, so their route is a constant. A planted station is
    // an anchor, not a stall; only stations that are authored to move may be
    // required to keep moving through the interior knots.
    const bool authored_planted =
        zixx::kSpringGroundedHeading[station] ==
            zixx::kSpringAbsorbHeading[station] &&
        zixx::kSpringGroundedHeading[station] ==
            zixx::kSpringAssembledHeading[station] &&
        zixx::kSpringGroundedHeading[station] ==
            zixx::kSpringCollapsedHeading[station];
    if (authored_planted) continue;
    const int32_t at_absorb = angle_distance(
        zixx::spring_route_heading(station, zixx::kSpringArmAbsorbAt - 20),
        zixx::spring_route_heading(station, zixx::kSpringArmAbsorbAt + 20));
    const int32_t at_assembled = angle_distance(
        zixx::spring_route_heading(station, zixx::kSpringArmAssembledAt - 20),
        zixx::spring_route_heading(station, zixx::kSpringArmAssembledAt + 20));
    route_min_move = std::min(route_min_move, std::min(at_absorb, at_assembled));
    route_max_move = std::max(route_max_move, std::max(at_absorb, at_assembled));
  }
  int middle_region_key = -1;
  int32_t middle_region_entry = -1;
  for (int key = 0; key <= compress48.compress_keys; ++key) {
    const int32_t entry =
        zixx::zixx_plan_spring_entry_amount(compress48, key);
    if (middle_region_key < 0 ||
        std::abs(entry - zixx::kSpringMiddleEntryProfile) <
            std::abs(middle_region_entry - zixx::kSpringMiddleEntryProfile)) {
      middle_region_key = key;
      middle_region_entry = entry;
    }
  }
  const zc::Clip* compiled_compress48 =
      synthetic_clip(kSyntheticCompress48Slot);
  int32_t compress48_step = INT32_MAX;
  if (compiled_compress48 != nullptr) {
    const ClipScan scan = scan_clip(
        synthetic_type, *compiled_compress48, stations);
    compress48_step = station_step_max_mm(
        scan, 2 * middle_region_key - 1,
        2 * middle_region_key + 1).mm;
  }
  const bool seam_context_exact = generic_seam_step <= 256 &&
                                  route_min_move > 0 &&
                                  compress48_step <=
                                      zixx::kJumpMaxStationStepMm;
  std::printf("SPRING continuous route: max per-milli heading step %d, "
              "slowest/fastest station turn across the two interior knots "
              "%d/%d (zero would be a dead stop); compress-48 reaches %d at "
              "key %d, compiled step %d mm\n",
              generic_seam_step, route_min_move, route_max_move,
              middle_region_entry, middle_region_key, compress48_step);
  require(seam_context_exact,
          "spring route jumps, or a station stalls at an interior pose");

  zc::PresentationMidpointAuthorship dummy_owned;
  const zc::Clip dummy_source = zixx::build_attack_dummy(&dummy_owned);
  const zixx::AttackVariantPhases dummy_phase =
      zixx::zixx_attack_variant_phases(
          zixx::zixx_variant_plan(zixx::kSlotAtkDummy),
          zixx::zixx_variant_air_hit(zixx::kSlotAtkDummy));
  check_midpoint_authorship("dummy attack", dummy_owned,
                            dummy_phase.hold_end, true,
                            dummy_phase.hold_end);

  zc::PresentationMidpointAuthorship fly_owned;
  const zc::Clip fly_source = zixx::build_attack_fly(&fly_owned);
  const zixx::AttackVariantPhases fly_phase =
      zixx::zixx_attack_variant_phases(
          zixx::zixx_variant_plan(zixx::kSlotAtkFly),
          zixx::zixx_variant_air_hit(zixx::kSlotAtkFly));
  check_midpoint_authorship("flying attack", fly_owned,
                            fly_phase.hold_end, true, fly_phase.hold_end);

  zc::PresentationMidpointAuthorship six_owned;
  const zc::Clip six_source = zixx::build_attack_six(&six_owned);
  const zixx::AttackVariantPhases six_phase =
      zixx::zixx_attack_variant_phases(
          zixx::zixx_variant_plan(zixx::kSlotAtkSix),
          zixx::zixx_variant_air_hit(zixx::kSlotAtkSix));
  check_midpoint_authorship("six-salto", six_owned,
                            six_phase.hold_end, true, six_phase.hold_end);

  zc::PresentationMidpointAuthorship jump_one_owned;
  const zc::Clip jump_one_source = zixx::build_jump_one(&jump_one_owned);
  const zixx::JumpPhases jump_one_phase = zixx::zixx_jump_phases(
      zixx::zixx_jump_plan(zixx::kSlotJumpOne, 1));
  check_midpoint_authorship("one-turn jump", jump_one_owned,
                            jump_one_phase.hold_end, true,
                            jump_one_phase.hold_end);

  zc::PresentationMidpointAuthorship jump_multi_owned;
  const zc::Clip jump_multi_source = zixx::build_jump_multi(&jump_multi_owned);
  const zixx::JumpPhases jump_multi_phase = zixx::zixx_jump_phases(
      zixx::zixx_jump_plan(zixx::kSlotJumpMulti, 3));
  check_midpoint_authorship("multi-turn jump", jump_multi_owned,
                            jump_multi_phase.hold_end, true,
                            jump_multi_phase.hold_end);

  zc::PresentationMidpointAuthorship nine_owned;
  const zc::Clip nine_source = zixx::build_attack_nine(&nine_owned);
  const zixx::AttackVariantPhases nine_phase =
      zixx::zixx_attack_variant_phases(
          zixx::zixx_variant_plan(zixx::kSlotAtkNine),
          zixx::zixx_variant_air_hit(zixx::kSlotAtkNine));
  check_midpoint_authorship("nine-salto", nine_owned,
                            nine_phase.hold_end, true, nine_phase.hold_end);

  (void)golden_source;
  (void)local_source;
  (void)dummy_source;
  (void)fly_source;
  (void)six_source;
  (void)jump_one_source;
  (void)jump_multi_source;
  (void)nine_source;

  struct ReleaseConsumer {
    const char* name;
    int slot;
    int first;
  };
  const std::array<ReleaseConsumer, 8> release_consumers{{
      {"golden", 3, zixx::kSaltoCompressHoldEndKey},
      {"release slice", zixx::kSlotAtkRelease, 1},
      {"dummy attack", zixx::kSlotAtkDummy, dummy_phase.hold_end},
      {"flying attack", zixx::kSlotAtkFly, fly_phase.hold_end},
      {"six-salto", zixx::kSlotAtkSix, six_phase.hold_end},
      {"one-turn jump", zixx::kSlotJumpOne, jump_one_phase.hold_end},
      {"multi-turn jump", zixx::kSlotJumpMulti, jump_multi_phase.hold_end},
      {"nine-salto", zixx::kSlotAtkNine, nine_phase.hold_end},
  }};
  auto clip_for_slot = [&](int slot) -> const zc::Clip* {
    for (const zc::Clip& c : type.bank.clips)
      if (c.slot_id == slot) return &c;
    return nullptr;
  };

  // Probe the compiled vocabulary consumer, not only its pre-compile source.
  // Without slot-10 provenance compile_creature legitimately regenerates this
  // sample and erases the authored key-4.5 bridge while all source checks pass.
  const zc::Clip* compiled_compression =
      clip_for_slot(zixx::kSlotAtkCompress);
  zc::Clip generic_compression = zixx::slice_clip(
      local_source, zixx::kSlotAtkCompress,
      zixx::kAtkCompressSliceFirstKey,
      zixx::kAtkCompressSliceLastKey);
  zc::bake_presentation_midpoints(generic_compression, zixx::kBoneCount);
  const size_t compression_key =
      static_cast<size_t>(zixx::kSpringEntryOwnedMidpointKey);
  const size_t source_q = compression_key * zixx::kBoneCount;
  bool compiled_quats_exact =
      compiled_compression != nullptr &&
      compiled_compression->mid_quats.size() >= source_q + zixx::kBoneCount &&
      local_source.mid_quats.size() >= source_q + zixx::kBoneCount;
  int generic_quat_diffs = 0;
  if (compiled_quats_exact) {
    for (int bone = 0; bone < zixx::kBoneCount; ++bone) {
      if (std::memcmp(&compiled_compression->mid_quats[source_q + bone],
                      &local_source.mid_quats[source_q + bone],
                      sizeof(zc::quat16)) != 0)
        compiled_quats_exact = false;
      if (std::memcmp(&generic_compression.mid_quats[source_q + bone],
                      &local_source.mid_quats[source_q + bone],
                      sizeof(zc::quat16)) != 0)
        ++generic_quat_diffs;
    }
  }
  const size_t source_root = compression_key * 3;
  const bool compiled_root_exact =
      compiled_compression != nullptr &&
      compiled_compression->mid_root.size() >= source_root + 3 &&
      local_source.mid_root.size() >= source_root + 3 &&
      std::memcmp(&compiled_compression->mid_root[source_root],
                  &local_source.mid_root[source_root],
                  3 * sizeof(int32_t)) == 0;
  const bool generic_root_differs =
      generic_compression.mid_root.size() >= source_root + 3 &&
      local_source.mid_root.size() >= source_root + 3 &&
      std::memcmp(&generic_compression.mid_root[source_root],
                  &local_source.mid_root[source_root],
                  3 * sizeof(int32_t)) != 0;
  const bool compiled_deform_exact =
      compiled_compression != nullptr &&
      compiled_compression->mid_deform.size() > compression_key &&
      local_source.mid_deform.size() > compression_key &&
      compiled_compression->mid_deform[compression_key].flatten ==
          local_source.mid_deform[compression_key].flatten &&
      compiled_compression->mid_deform[compression_key].spread ==
          local_source.mid_deform[compression_key].spread;
  const bool generic_deform_differs =
      generic_compression.mid_deform.size() > compression_key &&
      local_source.mid_deform.size() > compression_key &&
      (generic_compression.mid_deform[compression_key].flatten !=
           local_source.mid_deform[compression_key].flatten ||
       generic_compression.mid_deform[compression_key].spread !=
           local_source.mid_deform[compression_key].spread);
  std::printf("MIDPOINT compiled compression key 4.5: "
              "quats/root/deform exact=%d/%d/%d, generic differs "
              "quats/root/deform=%d/%d/%d\n",
              compiled_quats_exact ? 1 : 0, compiled_root_exact ? 1 : 0,
              compiled_deform_exact ? 1 : 0, generic_quat_diffs,
              generic_root_differs ? 1 : 0,
              generic_deform_differs ? 1 : 0);
  require(compiled_quats_exact && compiled_root_exact &&
              compiled_deform_exact && generic_quat_diffs > 0,
          "compiled compression slot lost authored key-4.5 midpoint data");

  // Real ChoreoRoot composition at the authored 4.5 sample. The complete
  // build_attack(choreo=true) clip owns the local body midpoint but intentionally
  // owns no trajectory root. Supply the corresponding compiled monolithic
  // midpoint root as the instance transform, exactly as compose_creatures
  // left-multiplies ci.orient/ci.x/y/z with the palette returned for
  // anim.frame=4, anim.sub=1. The slot-10 checks above independently prove that
  // this same local midpoint survives production vocabulary compilation.
  const zc::Clip* monolithic_attack = clip_for_slot(3);
  bool half_choreo_exact = monolithic_attack != nullptr &&
                           monolithic_attack->mid_root.size() >=
                               source_root + 3 &&
                           local_source.mid_root.size() >= source_root + 3;
  int32_t half_world_vertex_delta_mm = INT32_MAX;
  int32_t half_world_station_delta_mm = INT32_MAX;
  int32_t half_support_delta_mm = INT32_MAX;
  bool half_deform_exact = false;
  bool half_local_root_zero = false;
  bool half_spin_zero = false;
  if (half_choreo_exact) {
    half_local_root_zero =
        local_source.mid_root[source_root] == 0 &&
        local_source.mid_root[source_root + 1] == 0 &&
        local_source.mid_root[source_root + 2] == 0;
    half_spin_zero = zixx::attack_choreo_sample(4).theta == 0 &&
                     zixx::attack_choreo_sample(5).theta == 0;
    zc::CreatureType local_type = type;
    bool installed_local_attack = false;
    for (zc::Clip& clip : local_type.bank.clips) {
      if (clip.slot_id == monolithic_attack->slot_id) {
        clip = local_source;
        installed_local_attack = true;
        break;
      }
    }
    half_choreo_exact = half_choreo_exact && installed_local_attack;
    const zc::DeformSample monolithic_deform = zc::deformation_sample(
        type, monolithic_attack->slot_id,
        zixx::kSpringEntryOwnedMidpointKey, 1);
    const zc::DeformSample local_deform = zc::deformation_sample(
        local_type, local_source.slot_id,
        zixx::kSpringEntryOwnedMidpointKey, 1);
    half_deform_exact =
        monolithic_deform.flatten == local_deform.flatten &&
        monolithic_deform.spread == local_deform.spread;

    zc::PoseBank monolithic_half_bank;
    zc::PoseBank local_half_bank;
    monolithic_half_bank.begin_frame();
    local_half_bank.begin_frame();
    const zc::mat3x4fx* monolithic_pose = monolithic_half_bank.acquire(
        type, monolithic_attack->slot_id,
        zixx::kSpringEntryOwnedMidpointKey, 1);
    const zc::mat3x4fx* local_pose = local_half_bank.acquire(
        local_type, local_source.slot_id,
        zixx::kSpringEntryOwnedMidpointKey, 1);
    zc::mat3x4fx instance_root;
    zc::quat16_to_mat3(zixx::quat_z(0), instance_root, nullptr);
    instance_root.m[3] = monolithic_attack->mid_root[source_root];
    instance_root.m[7] = monolithic_attack->mid_root[source_root + 1];
    instance_root.m[11] = monolithic_attack->mid_root[source_root + 2];
    std::array<zc::mat3x4fx, zc::kMaxBones> local_world;
    for (int bone = 0; bone < type.bank.bone_count; ++bone)
      zc::mat3x4_mul(instance_root, local_pose[bone], local_world[bone], nullptr);

    half_world_vertex_delta_mm = 0;
    for (int rung = 0; rung < 2; ++rung) {
      const auto& mesh = rung == 0 ? type.mesh : type.micro;
      for (const zc::Meshlet& meshlet : mesh) {
        for (size_t vertex = 0; vertex < meshlet.verts.size(); ++vertex) {
          const zc::SkinVertex& bind = meshlet.verts[vertex];
          zc::SkinVertex monolithic_vertex = bind;
          zc::SkinVertex local_vertex = bind;
          if (!meshlet.deform.empty()) {
            monolithic_vertex = zc::deform_skin_vertex(
                bind, meshlet.deform[vertex], monolithic_deform);
            local_vertex = zc::deform_skin_vertex(
                bind, meshlet.deform[vertex], local_deform);
          }
          int32_t mx = 0, my = 0, mz = 0;
          int32_t lx = 0, ly = 0, lz = 0;
          zc::skin_vertex(monolithic_pose, monolithic_vertex,
                          mx, my, mz, nullptr);
          zc::skin_vertex(local_world.data(), local_vertex,
                          lx, ly, lz, nullptr);
          const int64_t dx = to_mm(mx) - to_mm(lx);
          const int64_t dy = to_mm(my) - to_mm(ly);
          const int64_t dz = to_mm(mz) - to_mm(lz);
          half_world_vertex_delta_mm = std::max(
              half_world_vertex_delta_mm,
              static_cast<int32_t>(zref::isqrt_u64(
                  static_cast<uint64_t>(dx * dx + dy * dy + dz * dz))));
        }
      }
    }

    half_world_station_delta_mm = 0;
    for (const Station& station : stations) {
      int32_t mx = 0, my = 0, mz = 0;
      int32_t lx = 0, ly = 0, lz = 0;
      zc::skin_vertex(monolithic_pose, station.v, mx, my, mz, nullptr);
      zc::skin_vertex(local_world.data(), station.v, lx, ly, lz, nullptr);
      const int64_t dx = to_mm(mx) - to_mm(lx);
      const int64_t dy = to_mm(my) - to_mm(ly);
      const int64_t dz = to_mm(mz) - to_mm(lz);
      half_world_station_delta_mm = std::max(
          half_world_station_delta_mm,
          static_cast<int32_t>(zref::isqrt_u64(
              static_cast<uint64_t>(dx * dx + dy * dy + dz * dz))));
    }
    const uint8_t support_bone = static_cast<uint8_t>(
        zixx::kBSpine0 + zixx::kSpringPlantSegment);
    const zc::SkinVertex support{
        type.baked.world_x[support_bone], type.baked.world_y[support_bone],
        type.baked.world_z[support_bone], support_bone, support_bone,
        64, 0, 0};
    int32_t mx = 0, my = 0, mz = 0;
    int32_t lx = 0, ly = 0, lz = 0;
    zc::skin_vertex(monolithic_pose, support, mx, my, mz, nullptr);
    zc::skin_vertex(local_world.data(), support, lx, ly, lz, nullptr);
    const int64_t dx = to_mm(mx) - to_mm(lx);
    const int64_t dy = to_mm(my) - to_mm(ly);
    const int64_t dz = to_mm(mz) - to_mm(lz);
    half_support_delta_mm = static_cast<int32_t>(zref::isqrt_u64(
        static_cast<uint64_t>(dx * dx + dy * dy + dz * dz)));
    half_choreo_exact = half_choreo_exact &&
                        half_local_root_zero && half_spin_zero &&
                        half_deform_exact &&
                        half_world_vertex_delta_mm <= 1 &&
                        half_world_station_delta_mm <= 1 &&
                        half_support_delta_mm <= 1;
  }
  std::printf("CHOREO key 4.5 frame=4 sub=1: exact=%d, local root zero=%d, "
              "half spin zero=%d, deform exact=%d, world full/micro max "
              "delta=%d mm, stations=%d mm, station-14 support=%d mm\n",
              half_choreo_exact ? 1 : 0, half_local_root_zero ? 1 : 0,
              half_spin_zero ? 1 : 0, half_deform_exact ? 1 : 0,
              half_world_vertex_delta_mm, half_world_station_delta_mm,
              half_support_delta_mm);
  require(half_choreo_exact,
          "default local-body ChoreoRoot key-4.5 composition drifted");

  const zc::Clip* release_reference = monolithic_attack;
  bool release_parity = release_reference != nullptr;
  for (const ReleaseConsumer& consumer : release_consumers) {
    const zc::Clip* candidate = clip_for_slot(consumer.slot);
    bool exact = candidate != nullptr && release_reference != nullptr;
    int fault_step = -1;
    int fault_sub = -1;
    int fault_bone = -1;
    bool fault_deform = false;
    if (exact) {
      for (int step = 0; step < 5; ++step) {
        const size_t ref_q = static_cast<size_t>(
            zixx::kSaltoCompressHoldEndKey + step) * type.bank.bone_count;
        const size_t got_q = static_cast<size_t>(consumer.first + step) *
                             type.bank.bone_count;
        for (int b = 0; b < zixx::kSpineBones; ++b) {
          if (std::memcmp(&release_reference->quats[ref_q + b],
                          &candidate->quats[got_q + b],
                          sizeof(candidate->quats[0])) != 0) {
            exact = false;
            if (fault_step < 0) {
              fault_step = step;
              fault_sub = 0;
              fault_bone = b;
            }
          }
        }
        if (std::memcmp(&release_reference->quats[ref_q + zixx::kBHead],
                        &candidate->quats[got_q + zixx::kBHead],
                        sizeof(candidate->quats[0])) != 0) {
          exact = false;
          if (fault_step < 0) {
            fault_step = step;
            fault_sub = 0;
            fault_bone = zixx::kBHead;
          }
        }
        const zc::DeformSample ref_d = zc::deformation_sample(
            type, release_reference->slot_id,
            zixx::kSaltoCompressHoldEndKey + step, 0);
        const zc::DeformSample got_d = zc::deformation_sample(
            type, candidate->slot_id, consumer.first + step, 0);
        if (ref_d.flatten != got_d.flatten || ref_d.spread != got_d.spread) {
          exact = false;
          if (fault_step < 0) {
            fault_step = step;
            fault_sub = 0;
            fault_deform = true;
          }
        }
      }
      for (int step = 0; step < 4; ++step) {
        const size_t ref_q = static_cast<size_t>(
            zixx::kSaltoCompressHoldEndKey + step) * type.bank.bone_count;
        const size_t got_q = static_cast<size_t>(consumer.first + step) *
                             type.bank.bone_count;
        if (release_reference->mid_quats.size() <
                ref_q + type.bank.bone_count ||
            candidate->mid_quats.size() < got_q + type.bank.bone_count) {
          exact = false;
          if (fault_step < 0) {
            fault_step = step;
            fault_sub = 1;
            fault_bone = -2;
          }
          continue;
        }
        for (int b = 0; b < zixx::kSpineBones; ++b) {
          if (std::memcmp(&release_reference->mid_quats[ref_q + b],
                          &candidate->mid_quats[got_q + b],
                          sizeof(candidate->mid_quats[0])) != 0) {
            exact = false;
            if (fault_step < 0) {
              fault_step = step;
              fault_sub = 1;
              fault_bone = b;
            }
          }
        }
        if (std::memcmp(&release_reference->mid_quats[ref_q + zixx::kBHead],
                        &candidate->mid_quats[got_q + zixx::kBHead],
                        sizeof(candidate->mid_quats[0])) != 0) {
          exact = false;
          if (fault_step < 0) {
            fault_step = step;
            fault_sub = 1;
            fault_bone = zixx::kBHead;
          }
        }
        const zc::DeformSample ref_d = zc::deformation_sample(
            type, release_reference->slot_id,
            zixx::kSaltoCompressHoldEndKey + step, 1);
        const zc::DeformSample got_d = zc::deformation_sample(
            type, candidate->slot_id, consumer.first + step, 1);
        if (ref_d.flatten != got_d.flatten || ref_d.spread != got_d.spread) {
          exact = false;
          if (fault_step < 0) {
            fault_step = step;
            fault_sub = 1;
            fault_deform = true;
          }
        }
      }
    }
    std::printf("SPRING release parity %s: integer + midpoint finless "
                "silhouette/deform exact=%d",
                consumer.name, exact ? 1 : 0);
    if (!exact) {
      std::printf(" (first fault step %d sub %d, %s %d)", fault_step,
                  fault_sub, fault_deform ? "deform" : "bone", fault_bone);
      if (!fault_deform && fault_bone >= 0 && release_reference != nullptr &&
          candidate != nullptr) {
        const size_t ref_i = static_cast<size_t>(
            zixx::kSaltoCompressHoldEndKey + fault_step) *
                                 type.bank.bone_count + fault_bone;
        const size_t got_i = static_cast<size_t>(consumer.first + fault_step) *
                                 type.bank.bone_count + fault_bone;
        const zc::quat16& rq = fault_sub == 0
                                  ? release_reference->quats[ref_i]
                                  : release_reference->mid_quats[ref_i];
        const zc::quat16& gq = fault_sub == 0
                                  ? candidate->quats[got_i]
                                  : candidate->mid_quats[got_i];
        std::printf(" ref={%d,%d,%d,%d} got={%d,%d,%d,%d}",
                    rq.q[0], rq.q[1], rq.q[2], rq.q[3],
                    gq.q[0], gq.q[1], gq.q[2], gq.q[3]);
      }
    }
    std::printf("\n");
    release_parity = release_parity && exact;
  }
  require(release_parity,
          "spring release timing/silhouette drifted across consumers");

  // The shared law begins at key zero for every complete consumer, so compare
  // the whole finless animal at all 45 integer/true-half samples through key 22.
  // Root is included: matching bones with a drifting support would only prove a
  // local pose, not the promised planted whole-body spring.
  bool whole_spring_parity = release_reference != nullptr;
  for (const ReleaseConsumer& consumer : release_consumers) {
    if (consumer.slot == zixx::kSlotAtkRelease) continue;
    const zc::Clip* candidate = clip_for_slot(consumer.slot);
    bool exact = candidate != nullptr && release_reference != nullptr;
    int fault_tick = -1;
    int fault_bone = -1;
    const char* fault_channel = "storage";
    for (int tick = 0; exact && tick <= 2 * zixx::kSaltoSpringReleasePoseKey;
         ++tick) {
      const int key = tick / 2;
      const int sub = tick & 1;
      const size_t qi = static_cast<size_t>(key) * type.bank.bone_count;
      const size_t ri = static_cast<size_t>(key) * 3;
      const std::vector<zc::quat16>& ref_quats =
          sub == 0 ? release_reference->quats : release_reference->mid_quats;
      const std::vector<zc::quat16>& got_quats =
          sub == 0 ? candidate->quats : candidate->mid_quats;
      const std::vector<int32_t>& ref_root =
          sub == 0 ? release_reference->root : release_reference->mid_root;
      const std::vector<int32_t>& got_root =
          sub == 0 ? candidate->root : candidate->mid_root;
      if (ref_quats.size() < qi + type.bank.bone_count ||
          got_quats.size() < qi + type.bank.bone_count ||
          ref_root.size() < ri + 3 || got_root.size() < ri + 3) {
        exact = false;
        fault_tick = tick;
        continue;
      }
      for (int b = 0; b < zixx::kSpineBones && exact; ++b) {
        if (std::memcmp(&ref_quats[qi + b], &got_quats[qi + b],
                        sizeof(ref_quats[0])) != 0) {
          exact = false;
          fault_tick = tick;
          fault_bone = b;
          fault_channel = "bone";
        }
      }
      if (exact &&
          std::memcmp(&ref_quats[qi + zixx::kBHead],
                      &got_quats[qi + zixx::kBHead],
                      sizeof(ref_quats[0])) != 0) {
        exact = false;
        fault_tick = tick;
        fault_bone = zixx::kBHead;
        fault_channel = "bone";
      }
      if (exact &&
          std::memcmp(&ref_root[ri], &got_root[ri], 3 * sizeof(int32_t)) != 0) {
        exact = false;
        fault_tick = tick;
        fault_channel = "root";
      }
      if (exact) {
        const zc::DeformSample ref_d = zc::deformation_sample(
            type, release_reference->slot_id, key, sub);
        const zc::DeformSample got_d =
            zc::deformation_sample(type, candidate->slot_id, key, sub);
        if (ref_d.flatten != got_d.flatten || ref_d.spread != got_d.spread) {
          exact = false;
          fault_tick = tick;
          fault_channel = "deform";
        }
      }
    }
    std::printf("SPRING whole pre-lift parity %s: 45-sample finless "
                "silhouette/deform/root exact=%d",
                consumer.name, exact ? 1 : 0);
    if (!exact)
      std::printf(" (first fault %d%s, %s %d)", fault_tick / 2,
                  (fault_tick & 1) ? ".5" : "", fault_channel, fault_bone);
    std::printf("\n");
    whole_spring_parity = whole_spring_parity && exact;
  }
  require(whole_spring_parity,
          "whole pre-lift spring drifted across complete consumers");

  // Immediate programmable jump family.
  for (const auto spec : {std::pair<int, int>{zixx::kSlotJumpOne, 1},
                          std::pair<int, int>{zixx::kSlotJumpMulti, 3}}) {
    const ClipScan* scan = find_scan(scans, spec.first);
    require(scan != nullptr, "missing programmable jump clip");
    if (!scan) continue;
    const zixx::JumpPlan p = zixx::zixx_jump_plan(
        static_cast<uint16_t>(spec.first), spec.second);
    const zixx::JumpPhases ph = zixx::zixx_jump_phases(p);
    require(scan->clip->frame_count == ph.frame_count,
            "jump duration drifted from shared phase table");
    require(scan->clip->events.size() == 1 &&
                scan->clip->events[0].frame == ph.landing_key &&
                scan->clip->events[0].event == zc::kEvFoot,
            "jump landing event drifted from shared phase table");
    const zixx::JumpMotionSample at_launch =
        zixx::zixx_jump_motion_sample(p, ph.launch_key);
    const zixx::JumpMotionSample after_launch =
        zixx::zixx_jump_motion_sample(p, ph.launch_key + 1);
    require(at_launch.lift == 0 && after_launch.lift > 0,
            "jump does not leave immediately after release");
    int32_t apex = 0;
    int32_t prev_theta = 0;
    bool monotonic = true;
    for (int k = ph.launch_key; k <= ph.landing_key; ++k) {
      const zixx::JumpMotionSample m = zixx::zixx_jump_motion_sample(p, k);
      apex = std::max(apex, m.lift);
      if (k > ph.launch_key && m.theta < prev_theta) monotonic = false;
      prev_theta = m.theta;
    }
    const zixx::JumpMotionSample landed =
        zixx::zixx_jump_motion_sample(p, ph.landing_key);
    require(apex == p.apex_mm, "jump authored root-lift apex drifted");
    require(monotonic && landed.theta == spec.second * 65536 &&
                (landed.theta & 0xFFFF) == 0,
            "jump turn count/wrap is not exact and monotonic");
    const StationStepMaximum continuity = station_step_max_mm(
        *scan, 1, static_cast<int>(scan->samples.size()) - 1);
    require(continuity.mm <= zixx::kJumpMaxStationStepMm,
            "jump has a discontinuous 60 Hz station step");
    require(key_pose_equal(*scan->clip, 0, ph.last_key,
                           type.bank.bone_count, true),
            "jump does not recover bit-exactly to its starting rest pose");
    struct ContactRange {
      int32_t deepest_mm = INT32_MAX;
      int deepest_tick = -1;
      int32_t highest_mm = INT32_MIN;
      int highest_tick = -1;
    };
    const auto contact_range = [&](int rung, int begin_tick, int end_tick) {
      ContactRange out;
      for (int tick = begin_tick; tick <= end_tick; ++tick) {
        const int32_t min_y =
            to_mm(scan->samples[tick].rung_min_y_fx[rung]);
        if (min_y < out.deepest_mm) {
          out.deepest_mm = min_y;
          out.deepest_tick = tick;
        }
        if (min_y > out.highest_mm) {
          out.highest_mm = min_y;
          out.highest_tick = tick;
        }
      }
      return out;
    };
    std::array<ContactRange, 2> whole_clip;
    std::array<ContactRange, 2> impact;
    std::array<ContactRange, 2> handoff;
    std::array<ContactRange, 2> settle;
    for (int rung = 0; rung < 2; ++rung) {
      whole_clip[rung] = contact_range(
          rung, 0, static_cast<int>(scan->samples.size()) - 1);
      impact[rung] = contact_range(
          rung, 2 * ph.landing_key, 2 * (ph.landing_key + 1));
      // Include the 61.5 ingress midpoint: it is where the two-key slam hands
      // vertical support to the sampled loaded-S root.
      handoff[rung] = contact_range(
          rung, 2 * (ph.landing_key + 1) + 1,
          2 * (ph.landing_key + zixx::kJumpLandingSupportHandoffEnd));
      settle[rung] = contact_range(
          rung,
          2 * (ph.landing_key + zixx::kJumpLandingSupportHandoffEnd) + 1,
          2 * ph.last_key);
      // The whole jump clip includes its own spring anticipation, so the
      // deepest legitimate sample is the SPRING's loaded declaration.
      require(whole_clip[rung].deepest_mm >=
                  -zixx::kSpringDeclaredLoadedBiteMm,
              "jump full/micro surface exceeds its declared maximum bite");
      require(impact[rung].deepest_mm >= -zixx::kJumpLandingLoadedBiteMm &&
                  impact[rung].highest_mm <= -zixx::kJumpImpactMinBiteMm,
              "jump full/micro impact lost its deliberate terrain bite");
      require(handoff[rung].deepest_mm >= -zixx::kJumpLandingLoadedBiteMm &&
                  handoff[rung].highest_mm <= 0,
              "jump full/micro support handoff penetrates or hovers");
      require(settle[rung].deepest_mm >= -zixx::kJumpLandingLoadedBiteMm &&
                  settle[rung].highest_mm <= 0,
              "jump full/micro settle penetrates or hovers");
    }
    const int clear_begin = 2 * (ph.launch_key + 2);
    const int clear_end = 2 * (ph.landing_key - 2);
    bool clear_flight = true;
    for (int t = clear_begin; t <= clear_end; ++t)
      if (to_mm(scan->samples[t].min_y_fx) <= 0) clear_flight = false;
    require(clear_flight, "jump touches terrain in the undeclared flight core");
    const auto print_contact_range = [](const char* rung, const char* name,
                                        const ContactRange& range) {
      std::printf("%s %s %d..%d mm (deepest %d%s, highest %d%s)", rung,
                  name, range.deepest_mm, range.highest_mm,
                  range.deepest_tick / 2,
                  (range.deepest_tick & 1) ? ".5" : "",
                  range.highest_tick / 2,
                  (range.highest_tick & 1) ? ".5" : "");
    };
    std::printf("JUMP slot %d: apex %d mm, turns %d, landing key %d; ",
                spec.first, apex, spec.second, ph.landing_key);
    for (int rung = 0; rung < 2; ++rung) {
      const char* rung_name = rung == 0 ? "full" : "micro";
      if (rung != 0) std::printf("; ");
      print_contact_range(rung_name, "impact", impact[rung]);
      std::printf(", ");
      print_contact_range(rung_name, "handoff", handoff[rung]);
      std::printf(", ");
      print_contact_range(rung_name, "settle", settle[rung]);
    }
    std::printf("; max 60 Hz station step %d mm at %d%s station %d\n",
                continuity.mm, continuity.tick / 2,
                (continuity.tick & 1) ? ".5" : "", continuity.station);
    if (spec.first == zixx::kSlotJumpOne) {
      for (int tick = 2 * ph.landing_key; tick <= 2 * ph.last_key; ++tick) {
        const int relative_tick = tick - 2 * ph.landing_key;
        std::printf("JUMP landing contact sample %d%s: full/micro %d/%d mm\n",
                    relative_tick / 2, (relative_tick & 1) ? ".5" : "",
                    to_mm(scan->samples[tick].rung_min_y_fx[0]),
                    to_mm(scan->samples[tick].rung_min_y_fx[1]));
      }
    }
  }

  // Impact vocabulary acceptance. These envelopes surround the native 240p
  // every-frame silhouettes selected by eye; they prevent later edits from
  // quietly returning the hits to local twitches or high-frequency shake.
  const ClipScan* hit = find_scan(scans, 5);
  require(hit != nullptr, "missing generic hit clip");
  if (hit) {
    require(key_pose_equal(*hit->clip, 0, hit->clip->frame_count - 1,
                           type.bank.bone_count, true),
            "generic hit does not recover bit-exactly to rest");
    const int contact =
        zixx::hit_station_profile_index(zixx::HitStation::kFront);
    const RelativePeak local = relative_peak_mm(*hit, contact, contact + 8, 0, 20);
    const RelativePeak front = relative_peak_mm(*hit, 0, 12, 0, 16);
    const RelativePeak tail = relative_peak_mm(
        *hit, zixx::kProfileStations - 1, 44, 0,
        static_cast<int>(hit->samples.size()) - 1);
    const StationStepMaximum step = station_step_max_mm(
        *hit, 1, static_cast<int>(hit->samples.size()) - 1);
    std::printf("HIT generic front station %d: local peak %d mm at %d%s; "
                "head-span %d mm at %d%s; tail %d mm at %d%s; "
                "max 60 Hz step %d mm at %d%s; terrain %d mm\n",
                contact, local.mm, local.tick / 2,
                (local.tick & 1) ? ".5" : "", front.mm, front.tick / 2,
                (front.tick & 1) ? ".5" : "", tail.mm, tail.tick / 2,
                (tail.tick & 1) ? ".5" : "", step.mm, step.tick / 2,
                (step.tick & 1) ? ".5" : "", to_mm(hit->worst_min_fx));
    require(local.mm >= 350 && local.mm <= 460,
            "generic hit front-station fold left its accepted local band");
    require(front.mm >= 230,
            "generic hit lost its strong displaced struck length");
    require(tail.mm >= 60,
            "generic hit shock no longer reaches the supporting tail");
    require(tail.tick >= front.tick + 6,
            "generic hit tail no longer reacts after the struck section");
    require(step.mm <= 500,
            "generic hit regained a high-frequency station twitch");
    const int32_t contact_y = to_mm(hit->worst_min_fx);
    require(contact_y >= -zixx::kHitFrontBiteMm &&
                contact_y <= -zixx::kHitFrontContactMinMm,
            "generic front hit left its declared 3D contact band");
  }

  const ClipScan* right = find_scan(scans, 23);
  const ClipScan* back = find_scan(scans, 24);
  const ClipScan* left = find_scan(scans, 25);
  const ClipScan* top = find_scan(scans, 26);
  const int middle_contact =
      zixx::hit_station_profile_index(zixx::HitStation::kMiddle);
  const int rear_contact =
      zixx::hit_station_profile_index(zixx::HitStation::kRear);
  for (const ClipScan* directional : {right, back, left, top}) {
    require(directional != nullptr, "missing directional damage clip");
    if (directional)
      require(key_pose_equal(*directional->clip, 0,
                             directional->clip->frame_count - 1,
                             type.bank.bone_count, true),
              "directional hit does not recover bit-exactly to rest");
  }
  if (right) {
    const int32_t y = to_mm(right->worst_min_fx);
    require(y >= -zixx::kDmgRightBiteMm &&
                y <= -zixx::kDmgRightContactMinMm,
            "right middle-station hit left its declared 3D contact band");
  }
  if (back) {
    const int32_t y = to_mm(back->worst_min_fx);
    require(y >= -zixx::kDmgBackBiteMm &&
                y <= -zixx::kDmgBackContactMinMm,
            "back rear-station hit left its declared 3D contact band");
  }
  if (left) {
    const int32_t y = to_mm(left->worst_min_fx);
    require(y >= -zixx::kDmgLeftBiteMm &&
                y <= -zixx::kDmgLeftContactMinMm,
            "left middle-station hit left its declared 3D contact band");
  }
  if (top) {
    const int32_t y = to_mm(top->worst_min_fx);
    require(y >= -zixx::kDmgTopBiteMm &&
                y <= -zixx::kDmgTopContactMinMm,
            "top middle-station hit left its declared 3D contact band");
  }
  if (right && left) {
    const RelativePeak right_local =
        relative_peak_mm(*right, middle_contact, middle_contact + 8, 0, 20);
    const RelativePeak left_local =
        relative_peak_mm(*left, middle_contact, middle_contact + 8, 0, 20);
    int32_t mirror_error = 0;
    for (size_t t = 0; t < std::min(right->samples.size(), left->samples.size()); ++t) {
      for (int i = 0; i < zixx::kProfileStations; ++i) {
        mirror_error = std::max(
            mirror_error,
            std::abs(right->samples[t].x_mm[i] - left->samples[t].x_mm[i]));
        mirror_error = std::max(
            mirror_error,
            std::abs(right->samples[t].y_mm[i] - left->samples[t].y_mm[i]));
        mirror_error = std::max(
            mirror_error,
            std::abs(right->samples[t].z_mm[i] + left->samples[t].z_mm[i]));
      }
    }
    int32_t right_shove = 0, left_shove = 0;
    for (int k = 0; k < right->clip->frame_count; ++k) {
      right_shove = std::max(right_shove,
                             std::abs(to_mm(right->clip->root[k * 3 + 2])));
      left_shove = std::max(left_shove,
                            std::abs(to_mm(left->clip->root[k * 3 + 2])));
    }
    std::printf("HIT sides middle station %d: local peaks R/L %d/%d mm at "
                "%d%s/%d%s; root shove %d/%d mm; mirrored station error "
                "%d mm; terrain %d/%d mm\n",
                middle_contact, right_local.mm, left_local.mm,
                right_local.tick / 2, (right_local.tick & 1) ? ".5" : "",
                left_local.tick / 2, (left_local.tick & 1) ? ".5" : "",
                right_shove, left_shove, mirror_error,
                to_mm(right->worst_min_fx), to_mm(left->worst_min_fx));
    require(right_shove >= 180 && left_shove >= 180,
            "side hit lost its unmistakable whole-body displacement");
    require(right_local.mm >= 110 && right_local.mm <= 150 &&
                left_local.mm >= 110 && left_local.mm <= 150,
            "side hit middle-station folds left their accepted local band");
    require(mirror_error <= 90,
            "right/left directional hits stopped being spatial mirrors");
  }
  if (back && top) {
    int32_t back_surge = 0;
    for (int k = 0; k < back->clip->frame_count; ++k)
      back_surge = std::max(back_surge,
                            std::abs(to_mm(back->clip->root[k * 3 + 0])));
    const RelativePeak back_local =
        relative_peak_mm(*back, rear_contact, rear_contact - 8, 0, 20);
    const RelativePeak top_local =
        relative_peak_mm(*top, middle_contact, middle_contact + 8, 0, 20);
    const RelativePeak back_front = relative_peak_mm(*back, 0, 12, 0, 30);
    const RelativePeak top_front = relative_peak_mm(*top, 0, 12, 0, 30);
    std::printf("HIT back rear station %d / top middle station %d: local "
                "peaks %d/%d mm at %d%s/%d%s; delayed head spans %d/%d "
                "mm; back surge %d mm; terrain %d/%d mm\n",
                rear_contact, middle_contact, back_local.mm, top_local.mm,
                back_local.tick / 2, (back_local.tick & 1) ? ".5" : "",
                top_local.tick / 2, (top_local.tick & 1) ? ".5" : "",
                back_front.mm, top_front.mm, back_surge,
                to_mm(back->worst_min_fx), to_mm(top->worst_min_fx));
    require(back_surge >= 210,
            "back hit lost its forward whole-body surge");
    require(back_local.mm >= 65 && back_local.mm <= 95 &&
                top_local.mm >= 165 && top_local.mm <= 215,
            "back/top named contact stations left their accepted local bands");
    require(std::memcmp(back->clip->quats.data(), top->clip->quats.data(),
                        back->clip->quats.size() * sizeof(back->clip->quats[0])) != 0,
            "back and top hit silhouettes collapsed into one reaction");
  }

  const ClipScan* air_hit = find_scan(scans, zixx::kSlotAtkAirHit);
  require(air_hit != nullptr, "missing standalone air-hit clip");
  if (air_hit) {
    const BowMaximum bow = chord_bow_max_mm(*air_hit);
    const StationStepMaximum step = station_step_max_mm(
        *air_hit, 1, static_cast<int>(air_hit->samples.size()) - 1);
    std::printf("HIT air: whole-spear chord bow %d mm at %d%s station %d; "
                "max 60 Hz step %d mm\n", bow.mm, bow.tick / 2,
                (bow.tick & 1) ? ".5" : "", bow.station, step.mm);
    require(key_pose_equal(*air_hit->clip, 0, air_hit->clip->frame_count - 1,
                           type.bank.bone_count, true),
            "standalone air hit no longer has exact spear seams");
    require(bow.mm >= 100,
            "standalone air hit lost its visible whole-spear bow");
    require(step.mm <= 600,
            "standalone air hit regained a high-frequency station twitch");
  }

  // Six/nine matched limit declarations and phase-specific terrain policy.
  const zc::AttackPlan six_plan =
      zixx::zixx_variant_plan(zixx::kSlotAtkSix);
  const zc::AttackPlan nine_plan =
      zixx::zixx_variant_plan(zixx::kSlotAtkNine);
  require(six_plan.apex_mm == 12000 && nine_plan.apex_mm == 24000 &&
              nine_plan.apex_mm == 2 * six_plan.apex_mm,
          "six/nine authored root apex is not exactly 12000/24000 mm");
  require(six_plan.spin_mturns == 6000 && nine_plan.spin_mturns == 9000,
          "six/nine programmed whole-salto count drifted");
  for (const auto spec : {
           std::pair<int, int>{zixx::kSlotAtkSix,
                               zixx::kAtkSixGroundStrikeDepthMm},
           std::pair<int, int>{zixx::kSlotAtkNine,
                               zixx::kAtkNineTargetContactDepthMm}}) {
    const ClipScan* scan = find_scan(scans, spec.first);
    require(scan != nullptr, "missing six/nine attack limit clip");
    if (!scan) continue;
    const bool target = zixx::zixx_variant_air_hit(
        static_cast<uint16_t>(spec.first));
    const zc::AttackPlan p = zixx::zixx_variant_plan(
        static_cast<uint16_t>(spec.first));
    const zixx::AttackVariantPhases ph =
        zixx::zixx_attack_variant_phases(p, target);
    require(scan->clip->frame_count == ph.frame_count,
            "six/nine duration drifted from shared phase table");
    require(scan->clip->events.size() == 1 &&
                scan->clip->events[0].frame == ph.impact &&
                scan->clip->events[0].event == zc::kEvAttack,
            "six/nine impact event drifted from shared phase table");
    int32_t outside_worst = INT32_MAX;
    int32_t committed_worst = INT32_MAX;
    for (const PosedSample& s : scan->samples) {
      const bool committed =
          s.tick >= 2 * ph.impact && s.tick <= 2 * ph.recoil_begin;
      if (committed)
        committed_worst = std::min(committed_worst, to_mm(s.min_y_fx));
      else
        outside_worst = std::min(outside_worst, to_mm(s.min_y_fx));
    }
    require(committed_worst >= -spec.second,
            "committed strike exceeded its declared 3D contact depth");
    // Outside the declared strike window the clip is doing its ordinary spring
    // anticipation and landing, so the ordinary LOADED declaration applies.
    require(outside_worst >= -zixx::kSpringDeclaredLoadedBiteMm,
            "six/nine clip penetrates terrain outside declared phases");
    if (spec.first == zixx::kSlotAtkNine) {
      require(committed_worst <= -50,
              "nine-salto target insertion no longer reaches its declared depth");
      require(scan->saturation == 0,
              "nine-salto decode/skinning saturated fixed-point arithmetic");
    }
    const StationStepMaximum continuity = station_step_max_mm(
        *scan, 1, static_cast<int>(scan->samples.size()) - 1);
    require(continuity.mm <= 2200,
            "six/nine attack has a discontinuous 60 Hz station step");
    require(key_pose_equal(*scan->clip, ph.impact, ph.extract_begin,
                           type.bank.bone_count, true),
            "impact hold is not bit-constant through extraction start");
    std::printf("ATTACK slot %d: %d keys / %zu samples, impact %d, "
                "contact committed %d mm, outside %d mm, max step %d mm at "
                "%d%s station %d, saturation %llu\n",
                spec.first, scan->clip->frame_count, scan->samples.size(),
                ph.impact, committed_worst, outside_worst, continuity.mm,
                continuity.tick / 2, (continuity.tick & 1) ? ".5" : "",
                continuity.station,
                static_cast<unsigned long long>(scan->saturation));
  }

  if (failures == 0) {
    std::printf("ZIXX PROBE: PASS — every key + midpoint, declared 3D contact, "
                "balance/taunt/fall/impact/spring/jump/limit/overlap gates\n");
    return 0;
  }
  std::printf("ZIXX PROBE: FAIL — %d assertion(s)\n", failures);
  return 1;
}
