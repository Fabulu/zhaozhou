// zixx_springpose — COMMITTED diagnostic for the whole-body groundspring.
//
// Owner Direction 20 asks for a spring that leans BACK, grows a BIGGER S,
// arms slowly and evenly, and never reads rigid. Judging that needs the real
// posed CENTRELINE, not the rendered projection: a 2D frame cannot separate
// "in front of the dirt" from "inside it", and it cannot say which station
// stopped participating in the S.
//
// This probe walks the SAME fixed-point spine the reel walks — the authored
// headings, the quantised quat chain, the station-14 support compensation —
// and prints world millimetre centreline points from head (station 0) to the
// last tail station. It is COMPARISON SIDE ONLY. It never chooses a value;
// it reports what the authored tables produced so the eye can be checked
// after it has already decided.
//
// Modes:
//   pose <entry> <squash>       one sample, 0..1000 each
//   sweep <n>                   n samples along the authored arming route
//   clip <slot> <key0> <key1>   real clip keys, decoded through decode_pose
//
// Build (same flags as the reel, no cmake):
//   g++ -O2 -std=c++17 -I... tools/reel/zixx_springpose.cpp <zref objs>
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}
#include "zixxtrixx.h"

namespace {

int32_t to_mm(int64_t fx) { return static_cast<int32_t>(fx * 1000 >> 16); }

// Walk the real quantised chain for an authored (entry, squash) sample and
// print every spine joint origin in world millimetres, root-compensated the
// same way apply_spring_stance compensates it.
void dump_pose(int32_t entry, int32_t squash, bool middle) {
  zc::quat16 posed[zixx::kBoneCount];
  for (int b = 0; b < zixx::kBoneCount; ++b) posed[b] = zc::quat16_identity();
  int32_t prev = 0;
  for (int k = 0; k < zixx::kStanceSlopes; ++k) {
    const int32_t h = zixx::spring_profile_slope(k, 1000, entry, squash, middle);
    posed[zixx::kBSpine0 + k] = zixx::quat_z(h - prev);
    prev = h;
  }
  int32_t ax = 0, ay = 0;
  zixx::spring_anchor_offset(1000, entry, squash, ax, ay, middle);

  const int32_t seg = zixx::kBodyLenMm / (zixx::kSpineBones - 1);
  zc::mat3x4fx world = zc::mat3x4_identity();
  zc::quat16 base2[zixx::kBoneCount];
  for (int b = 0; b < zixx::kBoneCount; ++b) base2[b] = zc::quat16_identity();
  int32_t pv = 0;
  for (int k = 0; k < zixx::kStanceSlopes; ++k) {
    base2[zixx::kBSpine0 + k] = zixx::quat_z(zixx::kStanceSlope[k] - pv);
    pv = zixx::kStanceSlope[k];
  }
  int32_t bx = 0, by = 0, bz = 0, px = 0, py = 0, pz = 0;
  zixx::spring_support_origin_raw(base2, bx, by, bz);
  zixx::spring_support_origin_raw(posed, px, py, pz);
  const int32_t rox = zixx::spring_root_anchor_x(entry, squash, middle);
  const int32_t roy = zixx::spring_root_offset(entry, squash, middle);
  std::printf(
      "entry=%d squash=%d anchor=(%d,%d) root=(%d,%d) "
      "support_base=(%d,%d) support_posed=(%d,%d) raw_dx=%d raw_dy=%d\n",
      entry, squash, ax, ay, rox, roy, to_mm(bx), to_mm(by), to_mm(px),
      to_mm(py), to_mm(bx - px), to_mm(by - py));
  std::printf("  seg     x_mm     y_mm   head_a16   r_mm\n");
  for (int b = 0; b < zixx::kSpineBones; ++b) {
    zc::mat3x4fx local;
    zc::quat16_to_mat3(posed[zixx::kBSpine0 + b], local, nullptr);
    if (b == 0) {
      local.m[7] += fxm(zixx::kBodyY);
      world = local;
    } else {
      local.m[3] -= fxm(seg);
      zc::mat3x4fx next;
      zc::mat3x4_mul(world, local, next, nullptr);
      world = next;
    }
    const int32_t h = b < zixx::kStanceSlopes
                          ? zixx::spring_profile_slope(b, 1000, entry, squash,
                                                       middle)
                          : 0;
    // the tube half-thickness at this bone's ring station, so stacking can
    // be judged against the real radii (comparison side; the caller applies
    // the flatten/spread deform if it wants the squeezed section)
    const int st = (b * (zixx::kProfileStations - 1)) / (zixx::kSpineBones - 1);
    std::printf("  %2d  %7d  %7d  %8d  %5d\n", b, to_mm(world.m[3]) + ax,
                to_mm(world.m[7]) + ay, h, zixx::station_r(st));
  }
}

void dump_clip(int slot, int k0, int k1) {
  const zc::CreatureType& T = zixx::type();
  const zc::Clip& c = T.bank.clips[slot];
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  if (k1 < k0) k1 = k0;
  for (int k = k0; k <= k1 && k < static_cast<int>(c.frame_count); ++k) {
    zc::decode_pose(T, c, k, pose, nullptr, 0);
    std::printf("slot %d key %d\n", slot, k);
    std::printf("  seg     x_mm     y_mm\n");
    for (int b = 0; b < zixx::kSpineBones; ++b) {
      const zc::mat3x4fx& m = pose[zixx::kBSpine0 + b];
      std::printf("  %2d  %7d  %7d\n", b, to_mm(m.m[3]), to_mm(m.m[7]));
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "sweep";
  if (std::strcmp(mode, "pose") == 0) {
    const int32_t e = argc > 2 ? std::atoi(argv[2]) : 0;
    const int32_t q = argc > 3 ? std::atoi(argv[3]) : 0;
    const bool mid = argc > 4 ? std::atoi(argv[4]) != 0 : false;
    dump_pose(e, q, mid);
    return 0;
  }
  if (std::strcmp(mode, "clip") == 0) {
    dump_clip(argc > 2 ? std::atoi(argv[2]) : 5,
              argc > 3 ? std::atoi(argv[3]) : 0,
              argc > 4 ? std::atoi(argv[4]) : 0);
    return 0;
  }
  if (std::strcmp(mode, "schedule") == 0) {
    // The ARMING SCHEDULE in pose space: what the eye actually reads as speed
    // is how far the animal moves per key, not the parameter value. Prints the
    // shared clock and the head's travel per key so "slow and steady" can be
    // checked as an even column rather than asserted.
    int32_t px = 0, py = 0;
    std::printf("key   arm  entry squash   head_x  head_y   move_mm\n");
    for (int key = 0; key <= zixx::kSaltoSpringReleasePoseKey + 2; ++key) {
      const int32_t arm = zixx::spring_shared_arm_amount(key);
      const int32_t e = zixx::spring_shared_entry_amount(key);
      const int32_t q = zixx::spring_shared_squash_amount(key);
      const int32_t hx = zixx::spring_root_anchor_x(e, q, false, key * 1000);
      const int32_t hy = zixx::spring_root_offset(e, q, false, key * 1000);
      const int32_t dx = hx - px, dy = hy - py;
      int32_t d = 0;
      for (int64_t g = 0; g * g <= static_cast<int64_t>(dx) * dx +
                                       static_cast<int64_t>(dy) * dy;
           ++g)
        d = static_cast<int32_t>(g);
      std::printf("%3d  %4d   %4d   %4d   %6d  %6d    %6d\n", key, arm, e, q,
                  hx, hy, key == 0 ? 0 : d);
      px = hx;
      py = hy;
    }
    return 0;
  }
  // sweep: the authored arming route, entry 0..1000 then squash 0..1000
  const int n = argc > 2 ? std::atoi(argv[2]) : 9;
  for (int i = 0; i <= n; ++i)
    dump_pose(static_cast<int32_t>((static_cast<int64_t>(i) * 1000) / n), 0,
              false);
  for (int i = 1; i <= n; ++i)
    dump_pose(1000, static_cast<int32_t>((static_cast<int64_t>(i) * 1000) / n),
              false);
  return 0;
}
