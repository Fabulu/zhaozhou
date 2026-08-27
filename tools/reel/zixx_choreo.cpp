// zixx_choreo — the CHOREO-ROOT PROOF for the programmable salto (C1/C3).
//
// The amendment's architecture: shared clips own LOCAL BODY SHAPE; a
// per-instance full-3D root transform owns trajectory, spin count, spin
// plane and attack direction. The owner-approved salto must survive that
// decomposition EXACTLY, so this tool proves two things and exits nonzero
// if either fails:
//
//   1. POSE-CACHE SHARING: two instances on the same (type, clip, frame)
//      with different ChoreoRoots acquire the SAME cached palette pointer
//      -- the creature_rules 2.2 economy is untouched by the root move.
//
//   2. SPIN MIGRATION: the attack rebuilt with the bone-0 somersault and
//      every root channel REMOVED (build_attack(choreo=true)), recomposed
//      through the instance root from attack_choreo_sample's trajectory
//      (same curves, same fixed-point trig, the c - R(c) re-pivot law),
//      must reproduce the golden clip's skinned WORLD-SPACE stations to
//      within quantisation: the paths round differently (quat_mul + one
//      quat16 quantise vs matrix compose), so the gate is a small mm
//      tolerance, not bit equality. Determinism and replay-exactness are
//      untouched: both paths are pure integer functions of the key index.
//
// Build exactly like zixx_probe (no cmake).
#include "zref/zref_creature.hpp"
#include "zrender/internal.hpp"

#include <algorithm>
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

  // ---- 1. pose-cache sharing across different ChoreoRoots -----------------
  zc::PoseBank bank;
  bank.begin_frame();
  const zc::mat3x4fx* p1 = bank.acquire(T, 3, 40, 0);
  const zc::mat3x4fx* p2 = bank.acquire(T, 3, 40, 0);  // "second instance"
  if (p1 != p2) {
    std::printf("SHARING: FAIL — same (type, clip, frame) decoded twice\n");
    return 1;
  }
  std::printf("SHARING: one palette serves both roots (pointer-identical)\n");

  // ---- 2. the spin-migration diff -----------------------------------------
  // the choreo variant type: same skeleton, attack clip without spin/root
  zc::CreatureType T2 = T;
  T2.bank.clips[2] = zixx::build_attack(/*choreo=*/true);
  const zc::Clip& golden = T.bank.clips[2];
  const zc::Clip& local = T2.bank.clips[2];

  // stations to compare: every profile station's ring centre (the probe's
  // own skeleton of the surface)
  std::vector<zc::SkinVertex> sts;
  for (int i = 0; i < zixx::kProfileStations; ++i) {
    const zixx::Bind bd =
        i <= zixx::kHeadEnd ? zixx::head_station_bind(i) : zixx::station_bind(i);
    sts.push_back(zc::SkinVertex{-fxm(zixx::station_x(i)), fxm(zixx::kBodyY), 0,
                                 bd.b0, bd.b1, bd.w0, 0, 0});
  }

  int32_t worst = 0;
  int worst_key = -1, worst_st = -1;
  for (uint16_t f = 0; f < golden.frame_count; ++f) {
    std::array<zc::mat3x4fx, zc::kMaxBones> poseG, poseL;
    zc::decode_pose(T, golden, f, poseG, nullptr, 0);
    zc::decode_pose(T2, local, f, poseL, nullptr, 0);
    // the instance root the AttackPlan would supply at this key
    const zixx::ChoreoSample cs = zixx::attack_choreo_sample(f);
    zc::mat3x4fx rot;
    zc::quat16_to_mat3(zixx::quat_z(cs.theta), rot, nullptr);
    rot.m[3] = fxm(cs.x_mm);
    rot.m[7] = fxm(cs.y_mm);
    std::array<zc::mat3x4fx, zc::kMaxBones> worldL;
    for (int b = 0; b < T.bank.bone_count; ++b) {
      zc::mat3x4_mul(rot, poseL[b], worldL[b], nullptr);
    }
    for (size_t i = 0; i < sts.size(); ++i) {
      int32_t gx, gy, gz, lx, ly, lz;
      zc::skin_vertex(poseG.data(), sts[i], gx, gy, gz, nullptr);
      zc::skin_vertex(worldL.data(), sts[i], lx, ly, lz, nullptr);
      const int64_t dx = to_mm(gx) - to_mm(lx);
      const int64_t dy = to_mm(gy) - to_mm(ly);
      const int64_t dz = to_mm(gz) - to_mm(lz);
      const int32_t d = static_cast<int32_t>(
          zref::isqrt_u64(static_cast<uint64_t>(dx * dx + dy * dy + dz * dz)));
      if (d > worst) {
        worst = d;
        worst_key = f;
        worst_st = static_cast<int>(i);
      }
    }
  }
  // TOLERANCE: the two decompositions round differently (the golden path
  // quantises quat_mul(spin, q0) into ONE quat16; the choreo path decodes
  // spin and q0 separately and composes matrices). At 14-bit lanes over a
  // 3.8 m reach that is millimetres. 12 mm ≈ 1 px at showcase distance.
  const int32_t kTolMm = 12;
  std::printf("SPIN MIGRATION: worst |Δ| %d mm at key %d station %d (tolerance %d)\n",
              worst, worst_key, worst_st, kTolMm);
  if (worst > kTolMm) {
    std::printf("CHOREO PROOF: FAIL\n");
    return 1;
  }
  std::printf("CHOREO PROOF: the root decomposition reproduces the approved salto\n");
  return 0;
}
