// zixx_headaim — COMMITTED diagnostic: the posed head-axis report.
//
// The head-attitude saga burned three passes on sign conventions and one on
// reading a pitched, blended, texture-mapped ball off single renders. This
// tool ends the guessing on the MEASUREMENT side (comparison only — it
// chooses nothing): it decodes idle key 0, skins the head-station centres
// exactly the way the mesh is skinned, and prints where the snout actually
// points, per candidate attitude. The VALUE is still picked by looking at
// renders (art law); this just says what each candidate IS.
//
// Build (same flags as the reel, no cmake):
//   g++ -O2 -std=c++17 -I... tools/reel/zixx_headaim.cpp <zref objs> -o
//   build/tools/zixx-headaim.exe
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}
#include "zixxtrixx.h"

namespace {
int32_t to_mm(int64_t fx) { return static_cast<int32_t>(fx * 1000 >> 16); }
}  // namespace

int main() {
  const zc::CreatureType& T = zixx::type();
  const zc::Clip& idle = T.bank.clips[0];
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(T, idle, 0, pose, nullptr, 0);
  std::printf("idle key 0, kHeadAttitude=%d\n", (int)zixx::kHeadAttitude);
  std::printf("station   x_mm    y_mm   (world: +x fwd, +y up; nose=station 0)\n");
  double x0 = 0, y0 = 0, x5 = 0, y5 = 0;
  for (int i = 0; i <= 12; ++i) {
    const zixx::Bind bd =
        i <= zixx::kHeadEnd ? zixx::head_station_bind(i) : zixx::station_bind(i);
    zc::SkinVertex v{-fxm(zixx::station_x(i)), fxm(zixx::kBodyY), 0,
                     bd.b0, bd.b1, bd.w0, 0, 0};
    int32_t x, y, z;
    zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
    std::printf("  %2d   %6d  %6d\n", i, to_mm(x), to_mm(y));
    if (i == 0) { x0 = to_mm(x); y0 = to_mm(y); }
    if (i == 5) { x5 = to_mm(x); y5 = to_mm(y); }
  }
  // the snout axis: rigid-skull rear (station 5) toward the nose (station 0)
  const double pitch = std::atan2(y0 - y5, x0 - x5) * 180.0 / 3.14159265;
  std::printf("snout axis (station 5 -> 0): %+.1f deg from horizontal "
              "(positive = nose UP)\n", pitch);
  return 0;
}
