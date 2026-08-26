// zixx_probe — the COMMITTED pose probe for the Zixxtrixx model.
//
// Ground contact must be authored, never accidental (CLAUDE.md), and it must
// be MEASURED with a 3D pose probe, not read off a rendered frame: a 2D frame
// cannot separate "in front of the dirt" from "inside it". This probe decodes
// every key of every clip, skins EVERY mesh vertex, and reports world-Y
// statistics against the flat ground plane the reel's root ground-snap
// establishes (root at terrain top, so ground = 0).
//
// Per clip it prints per-key min/max Y (sampled), the worst penetration and
// where it happens, and clip-specific numbers:
//   - idle/walk: belly excursion across the loop (the authored few-mm sink);
//   - attack:    blade-tip minimum per key around contact (the authored bite);
//   - fall:      whole-loop clearance (must NEVER touch).
//
// Build (from zhaozhou/, same flags as the reel — no cmake, see CLAUDE.md):
//   G="C:/Programmieren/dsstuff/mingw64/bin/g++.exe"
//   "$G" -Itests/render -Icompiler/tests/generated -Ireference/src \
//        -Ireference/include -Iruntime/include -O2 -DNDEBUG -std=c++17 \
//        tools/reel/zixx_probe.cpp build/reference/libzhao_zref.a \
//        -o build/tools/zixx-probe.exe
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

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
  size_t nv = 0, nt = 0;
  for (const auto& m : T.mesh) {
    nv += m.verts.size();
    nt += m.idx.size() / 3;
  }
  std::printf("bones=%d meshlets=%zu verts=%zu tris=%zu\n", (int)T.bank.bone_count, T.mesh.size(),
              nv, nt);

  for (const zc::Clip& clip : T.bank.clips) {
    int32_t worst_min = INT32_MAX, worst_max = INT32_MIN;
    int worst_f = -1;
    int32_t belly_lo = INT32_MAX, belly_hi = INT32_MIN;  // per-loop belly band
    std::printf("clip slot %d (%d keys)\n", clip.slot_id, clip.frame_count);
    for (uint16_t f = 0; f < clip.frame_count; ++f) {
      std::array<zc::mat3x4fx, zc::kMaxBones> pose;
      zc::decode_pose(T, clip, f, pose, nullptr, 0);
      int32_t mn = INT32_MAX, mx = INT32_MIN;
      // blade-tip minimum: the last two blade bones' meshlets carry the tips;
      // cheaper and robust to just track the min over vertices bound to the
      // blade bones.
      int32_t blade_mn = INT32_MAX;
      for (const auto& m : T.mesh) {
        for (const auto& v : m.verts) {
          int32_t x, y, z;
          zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
          mn = std::min(mn, y);
          mx = std::max(mx, y);
          if (v.b0 >= zixx::kBBladeL && v.b0 <= zixx::kBBladeR2)
            blade_mn = std::min(blade_mn, y);
        }
      }
      if (mn < worst_min) {
        worst_min = mn;
        worst_f = f;
      }
      worst_max = std::max(worst_max, mx);
      belly_lo = std::min(belly_lo, mn);
      belly_hi = std::max(belly_hi, mn);
      const bool attack = clip.slot_id == 3;
      if ((attack && f >= 44) || f % 8 == 0 || mn < -30)
        std::printf("  k%3d  minY %6d  maxY %6d  bladeMin %6d  (mm)\n", f, to_mm(mn), to_mm(mx),
                    to_mm(blade_mn));
    }
    std::printf("  WORST minY %d mm at key %d; apex %d mm; belly band [%d..%d] mm\n",
                to_mm(worst_min), worst_f, to_mm(worst_max), to_mm(belly_lo), to_mm(belly_hi));
  }
  return 0;
}
