// zixx_sideprofile — COMMITTED diagnostic: the posed side-silhouette envelope.
//
// RUN-20260828-0757 (the head/neck notch). The side silhouette of the front
// half is the envelope of the posed station circles: centre (x,y) plus the
// vertical half-extent rz. A notch in the rendered outline is a concavity in
// this envelope, and this probe says WHICH STATIONS carry it — comparison
// side only (art law: the render and the sheet still choose the values).
//
// Prints, for stations 0..30 of idle key 0: station, x_mm, y_mm (world,
// +x fwd, +y up, nose = station 0), rz_mm (vertical half-extent as built:
// head_ring for the head part, station_r for the body).
//
// Build (same flags as the reel, no cmake):
//   g++ -O2 -std=c++17 -I... tools/reel/zixx_sideprofile.cpp <zref objs>
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace zc = zref::creature;
constexpr int32_t fxm(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}
#include "zixxtrixx.h"

namespace {
int32_t to_mm(int64_t fx) { return static_cast<int32_t>(fx * 1000 >> 16); }
}  // namespace

int main(int argc, char** argv) {
  const int key = argc > 1 ? std::atoi(argv[1]) : 0;
  const zc::CreatureType& T = zixx::type();
  const zc::Clip& idle = T.bank.clips[0];
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(T, idle, key, pose, nullptr, 0);
  std::printf("idle key %d, kHeadAttitude=%d\n", key, (int)zixx::kHeadAttitude);
  std::printf("station   x_mm    y_mm    rz_mm\n");
  for (int i = 0; i <= 30; ++i) {
    const zixx::Bind bd =
        i <= zixx::kHeadEnd ? zixx::head_station_bind(i) : zixx::station_bind(i);
    zc::SkinVertex v{-fxm(zixx::station_x(i)), fxm(zixx::kBodyY), 0,
                     bd.b0, bd.b1, bd.w0, 0, 0};
    int32_t x, y, z;
    zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
    int32_t rx = 0, rz = 0;
    if (i <= zixx::kHeadEnd) {
      zixx::head_ring(i, rx, rz);
    } else {
      rz = zixx::station_r(i);
    }
    std::printf("  %2d   %6d  %6d  %6d\n", i, to_mm(x), to_mm(y), rz);
  }
  return 0;
}
