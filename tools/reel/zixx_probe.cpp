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
    for (const auto& meshlet : type.mesh) {
      for (const auto& v : meshlet.verts) {
        int32_t x = 0, y = 0, z = 0;
        zc::skin_vertex(pose.data(), v, x, y, z, &ledger);
        // Keep actual skinned-vertex minima per influencing bone. Balance uses
        // these to prove several body segments, rather than only blade tips,
        // share the authored terrain support.
        if (v.b0 < zc::kMaxBones && v.w0 != 0)
          s.bone_min_y_fx[v.b0] = std::min(s.bone_min_y_fx[v.b0], y);
        if (v.b1 < zc::kMaxBones && v.w0 != 255)
          s.bone_min_y_fx[v.b1] = std::min(s.bone_min_y_fx[v.b1], y);
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
    for (const PosedSample& s : balance->samples) {
      const bool impact =
          s.tick >= 2 * zixx::kBalImpactBeginKey -
                        zixx::kBalImpactLeadPresentationTicks &&
          s.tick <= 2 * zixx::kBalImpactEndKey;
      const int32_t y = to_mm(s.min_y_fx);
      if (impact) {
        if (y < impact_worst) {
          impact_worst = y;
          impact_tick = s.tick;
          impact_b0 = s.min_b0;
          impact_b1 = s.min_b1;
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
    std::printf("BALANCE terrain: outside impact %d mm at %d%s (bones %d/%d); "
                "declared impact %d mm at %d%s (bones %d/%d)\n",
                outside_worst, outside_tick / 2,
                (outside_tick & 1) ? ".5" : "", outside_b0, outside_b1,
                impact_worst, impact_tick / 2,
                (impact_tick & 1) ? ".5" : "", impact_b0, impact_b1);

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
    require(key_pose_equal(*balance->clip, 0, zixx::kBalKeys - 1,
                           type.bank.bone_count, true),
            "balance does not recover bit-exactly to authored rest");
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
      int32_t lateral_lo = INT32_MAX, lateral_hi = INT32_MIN;
      int64_t whole_rest_y = 0, whole_deep_y = 0;
      int32_t region_delta[7] = {};
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
        region_delta[region_index++] = delta;
        std::printf(" %s=%d", r.name, delta);
      }
      // Raised regions must visibly descend; the already-grounded run and
      // taper are allowed to move little while the radius-aware final profile
      // settles onto terrain. The tail still comes down rather than rolling up.
      require(region_delta[0] <= -700 && region_delta[1] <= -600 &&
                  region_delta[2] <= -450 && region_delta[3] <= -100,
              "raised spring regions no longer descend from above");
      require(region_delta[6] <= -50,
              "spring tail has started rolling upward during compression");
      require(whole_deep_y < whole_rest_y,
              "the complete spring no longer descends overall");
      std::printf(" mm\n");
      for (int i = 0; i < zixx::kProfileStations; ++i) {
        center_lo = std::min(center_lo, deep.y_mm[i]);
        center_hi = std::max(center_hi, deep.y_mm[i]);
        lateral_lo = std::min(lateral_lo, deep.z_mm[i]);
        lateral_hi = std::max(lateral_hi, deep.z_mm[i]);
      }
      const int32_t center_span = center_hi - center_lo;
      const int32_t lateral_span = lateral_hi - lateral_lo;
      const int32_t mesh_span = to_mm(deep.max_y_fx - deep.min_y_fx);
      const int32_t head_surface_y = to_mm(deep.bone_min_y_fx[zixx::kBHead]);
      const int32_t rear_surface_y = to_mm(
          std::min(deep.bone_min_y_fx[zixx::kBSpine0 + 16],
                   deep.bone_min_y_fx[zixx::kBSpine0 + 17]));
      std::printf("SPRING deepest: centre/lateral span %d/%d mm, mesh span "
                  "%d mm, minY %d mm at bones %d/%d; head/rear surface "
                  "%d/%d mm\n",
                  center_span, lateral_span, mesh_span, to_mm(deep.min_y_fx),
                  deep.min_b0, deep.min_b1, head_surface_y, rear_surface_y);
      std::printf("SPRING deepest centre Y (each fourth station):");
      for (int i = 0; i < zixx::kProfileStations; i += 4)
        std::printf(" %d:%d", i, deep.y_mm[i]);
      std::printf(" %d:%d mm\n", zixx::kProfileStations - 1,
                  deep.y_mm[zixx::kProfileStations - 1]);
      require(center_span <= 150,
              "deepest spring no longer has the accepted almost-flat silhouette");
      require(lateral_span <= 15,
              "spring centreline left the side plane and regained a concertina");
      require(head_surface_y >= -15 && head_surface_y <= 5,
              "compressed spring head no longer meets its authored contact band");
      require(rear_surface_y >= -15,
              "compressed spring rear penetrates terrain instead of staying rigid");
      require(mesh_span >= 150,
              "deepest spring lost the tube's volumetric cross-section");
      int32_t spring_worst = INT32_MAX;
      int spring_worst_tick = -1;
      for (int t = 0; t <= 2 * ph.hold_end &&
                      t < static_cast<int>(spring->samples.size()); ++t) {
        const int32_t min_y = to_mm(spring->samples[t].min_y_fx);
        if (min_y < spring_worst) {
          spring_worst = min_y;
          spring_worst_tick = t;
        }
      }
      const PosedSample& worst_pose = spring->samples[spring_worst_tick];
      std::printf("SPRING declared terrain bite: %d mm at %d%s, bones %d/%d "
                  "(law -40..-15)\n", spring_worst,
                  spring_worst_tick / 2,
                  (spring_worst_tick & 1) ? ".5" : "",
                  worst_pose.min_b0, worst_pose.min_b1);
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
    int contact_worst_tick = -1;
    for (const PosedSample& s : scan->samples) {
      const int32_t min_y = to_mm(s.min_y_fx);
      if (min_y < contact_worst) {
        contact_worst = min_y;
        contact_worst_tick = s.tick;
      }
    }
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
                "contact %d mm at %d%s, max 60 Hz station step %d mm at %d%s "
                "station %d\n",
                spec.first, apex, spec.second, ph.landing_key, contact_worst,
                contact_worst_tick / 2,
                (contact_worst_tick & 1) ? ".5" : "", continuity.mm,
                continuity.tick / 2,
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
                "balance/taunt/fall/impact/spring/jump/limit/overlap gates\n");
    return 0;
  }
  std::printf("ZIXX PROBE: FAIL — %d assertion(s)\n", failures);
  return 1;
}
