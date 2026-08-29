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
#include <cstdint>
#include <cstdio>
#include <cstring>
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
  int32_t blade_min_y_fx = INT32_MAX;
  int min_b0 = -1;
  int min_b1 = -1;
  std::array<int32_t, zixx::kProfileStations> x_mm{};
  std::array<int32_t, zixx::kProfileStations> y_mm{};
  std::array<int32_t, zixx::kProfileStations> z_mm{};
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
  uint64_t saturation = 0;
};

ClipScan scan_clip(const zc::CreatureType& type, const zc::Clip& clip,
                   const std::vector<Station>& stations) {
  ClipScan scan;
  scan.clip = &clip;
  const int ns = presentation_samples(clip);
  scan.samples.reserve(ns);
  for (int tick = 0; tick < ns; ++tick) {
    PosedSample s;
    s.tick = tick;
    const uint16_t key = static_cast<uint16_t>(tick / 2);
    const uint8_t sub = static_cast<uint8_t>(tick & 1);
    std::array<zc::mat3x4fx, zc::kMaxBones> pose;
    zref::SatLedger ledger;
    zc::decode_pose(type, clip, key, pose, &ledger, sub);
    for (const auto& meshlet : type.mesh) {
      for (const auto& v : meshlet.verts) {
        int32_t x = 0, y = 0, z = 0;
        zc::skin_vertex(pose.data(), v, x, y, z, &ledger);
        if (y < s.min_y_fx) {
          s.min_y_fx = y;
          s.min_b0 = v.b0;
          s.min_b1 = v.b1;
        }
        s.max_y_fx = std::max(s.max_y_fx, y);
        if (v.b0 >= zixx::kBBladeL && v.b0 <= zixx::kBBladeR2)
          s.blade_min_y_fx = std::min(s.blade_min_y_fx, y);
      }
    }
    for (int i = 0; i < zixx::kProfileStations; ++i) {
      int32_t x = 0, y = 0, z = 0;
      zc::skin_vertex(pose.data(), stations[i].v, x, y, z, &ledger);
      s.x_mm[i] = to_mm(x);
      s.y_mm[i] = to_mm(y);
      s.z_mm[i] = to_mm(z);
    }
    s.saturation = ledger.total();
    scan.saturation += s.saturation;
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
                "at %d%s; maxY %5d mm; saturation %llu\n",
                clip.slot_id, clip.frame_count, s.samples.size(),
                to_mm(s.worst_min_fx), s.worst_tick / 2,
                (s.worst_tick & 1) ? ".5" : "",
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

  // Shared spring: compare the authored rest against the first held deepest
  // compression in slot 35.  Thresholds are regression envelopes around the
  // visually accepted native side/top sheets, not generators for the pose.
  const ClipScan* spring = find_scan(scans, zixx::kSlotAtkSix);
  require(spring != nullptr, "missing slot 35 shared-spring clip");
  if (spring) {
    const zc::AttackPlan plan = zixx::zixx_variant_plan(zixx::kSlotAtkSix);
    const zixx::AttackVariantPhases ph =
        zixx::zixx_attack_variant_phases(plan, false);
    const int deep_tick = 2 * ph.compress_end;
    require(deep_tick < static_cast<int>(spring->samples.size()),
            "spring deepest sample outside clip");
    if (deep_tick < static_cast<int>(spring->samples.size())) {
      const PosedSample& rest = spring->samples[0];
      const PosedSample& deep = spring->samples[deep_tick];
      struct Region { const char* name; int lo; int hi; };
      const Region regions[] = {
          {"head", 0, 5}, {"neck", 6, 12}, {"front", 13, 20},
          {"middle", 21, 32}, {"grounded run", 33, 44},
          {"taper", 45, 51}, {"tail", 52, 56}};
      int32_t center_lo = INT32_MAX, center_hi = INT32_MIN;
      int64_t whole_rest_y = 0, whole_deep_y = 0;
      std::printf("SPRING regions (deep minus rest centre Y):");
      int region_index = 0;
      for (const Region& r : regions) {
        int64_t a = 0, b = 0;
        for (int i = r.lo; i <= r.hi; ++i) {
          a += rest.y_mm[i];
          b += deep.y_mm[i];
        }
        whole_rest_y += a;
        whole_deep_y += b;
        const int32_t delta = static_cast<int32_t>(
            (b - a) / (r.hi - r.lo + 1));
        std::printf(" %s=%d", r.name, delta);
        // Compression participation is motion into the shared concertina, not
        // a forced sign for every point.  The raised front must come down; rear
        // troughs rise into the same nearly-flat silhouette.  Requiring every
        // region to lower contradicted the accepted complete-animal pose and
        // merely measured which side of the original S each region occupied.
        require(delta <= -250 || region_index >= 3,
                "a raised front spring region failed to descend strongly");
        require(delta <= -100 || delta >= 100,
                "a spring region stopped participating in whole-body compression");
        ++region_index;
      }
      require(whole_deep_y < whole_rest_y,
              "the complete spring no longer descends overall");
      std::printf(" mm\n");
      for (int i = 0; i < zixx::kProfileStations; ++i) {
        center_lo = std::min(center_lo, deep.y_mm[i]);
        center_hi = std::max(center_hi, deep.y_mm[i]);
      }
      const int32_t center_span = center_hi - center_lo;
      const int32_t mesh_span = to_mm(deep.max_y_fx - deep.min_y_fx);
      std::printf("SPRING deepest: centre span %d mm, mesh span %d mm, "
                  "minY %d mm\n",
                  center_span, mesh_span, to_mm(deep.min_y_fx));
      require(center_span <= 520,
              "deepest spring no longer has the accepted almost-flat silhouette");
      require(mesh_span >= 150,
              "deepest spring lost the tube's volumetric cross-section");
      int32_t spring_worst = INT32_MAX;
      for (int t = 0; t <= 2 * ph.hold_end &&
                      t < static_cast<int>(spring->samples.size()); ++t)
        spring_worst = std::min(spring_worst,
                                to_mm(spring->samples[t].min_y_fx));
      std::printf("SPRING declared terrain bite: %d mm (law -40..-15)\n",
                  spring_worst);
      require(spring_worst >= -zixx::kSpringDeclaredBiteMm &&
                  spring_worst <= -15,
              "shared spring terrain bite left its authored declaration");
    }
  }

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
    int32_t contact_worst = INT32_MAX;
    for (const PosedSample& s : scan->samples)
      contact_worst = std::min(contact_worst, to_mm(s.min_y_fx));
    require(contact_worst >= -zixx::kSpringDeclaredBiteMm &&
                contact_worst <= -15,
            "jump ground contact left the declared spring/absorption band");
    const int clear_begin = 2 * (ph.launch_key + 2);
    const int clear_end = 2 * (ph.landing_key - 2);
    bool clear_flight = true;
    for (int t = clear_begin; t <= clear_end; ++t)
      if (to_mm(scan->samples[t].min_y_fx) <= 0) clear_flight = false;
    require(clear_flight, "jump touches terrain in the undeclared flight core");
    std::printf("JUMP slot %d: apex %d mm, turns %d, landing key %d, "
                "contact %d mm, max 60 Hz station step %d mm at %d%s "
                "station %d\n",
                spec.first, apex, spec.second, ph.landing_key, contact_worst,
                continuity.mm, continuity.tick / 2,
                (continuity.tick & 1) ? ".5" : "", continuity.station);
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
    const RelativePeak front = relative_peak_mm(*hit, 0, 12, 0, 16);
    const RelativePeak tail = relative_peak_mm(
        *hit, zixx::kProfileStations - 1, 44, 0,
        static_cast<int>(hit->samples.size()) - 1);
    const StationStepMaximum step = station_step_max_mm(
        *hit, 1, static_cast<int>(hit->samples.size()) - 1);
    std::printf("HIT generic: front relative peak %d mm at %d%s, tail "
                "relative peak %d mm at %d%s, max 60 Hz step %d mm at %d%s\n",
                front.mm, front.tick / 2, (front.tick & 1) ? ".5" : "",
                tail.mm, tail.tick / 2, (tail.tick & 1) ? ".5" : "",
                step.mm, step.tick / 2, (step.tick & 1) ? ".5" : "");
    require(front.mm >= 230,
            "generic hit lost its strong displaced struck length");
    require(tail.mm >= 60,
            "generic hit shock no longer reaches the supporting tail");
    require(tail.tick >= front.tick + 6,
            "generic hit tail no longer reacts after the struck section");
    require(step.mm <= 500,
            "generic hit regained a high-frequency station twitch");
  }

  const ClipScan* right = find_scan(scans, 23);
  const ClipScan* back = find_scan(scans, 24);
  const ClipScan* left = find_scan(scans, 25);
  const ClipScan* top = find_scan(scans, 26);
  for (const ClipScan* directional : {right, back, left, top}) {
    require(directional != nullptr, "missing directional damage clip");
    if (directional)
      require(key_pose_equal(*directional->clip, 0,
                             directional->clip->frame_count - 1,
                             type.bank.bone_count, true),
              "directional hit does not recover bit-exactly to rest");
  }
  if (right && left) {
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
    std::printf("HIT sides: root shove R/L %d/%d mm, mirrored station error "
                "%d mm\n", right_shove, left_shove, mirror_error);
    require(right_shove >= 180 && left_shove >= 180,
            "side hit lost its unmistakable whole-body displacement");
    require(mirror_error <= 90,
            "right/left directional hits stopped being spatial mirrors");
  }
  if (back && top) {
    int32_t back_surge = 0;
    for (int k = 0; k < back->clip->frame_count; ++k)
      back_surge = std::max(back_surge,
                            std::abs(to_mm(back->clip->root[k * 3 + 0])));
    const RelativePeak back_front = relative_peak_mm(*back, 0, 12, 0, 16);
    const RelativePeak top_front = relative_peak_mm(*top, 0, 12, 0, 16);
    std::printf("HIT back/top: back root surge %d mm; struck-length peaks "
                "%d/%d mm\n", back_surge, back_front.mm, top_front.mm);
    require(back_surge >= 210,
            "back hit lost its forward whole-body surge");
    require(back_front.mm >= 180 && top_front.mm >= 180,
            "back/top hit lost its large local struck-section deformation");
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
    require(outside_worst >= -zixx::kSpringDeclaredBiteMm,
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
                "impact/spring/jump/limit/overlap gates\n");
    return 0;
  }
  std::printf("ZIXX PROBE: FAIL — %d assertion(s)\n", failures);
  return 1;
}
