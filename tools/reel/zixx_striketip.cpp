// zixx_striketip — COMMITTED diagnostic: where the WEAPON actually is.
//
// RUN-20260828-1730, the tail-tip strike law. The owner: "The tip of the
// tail should stab into it, then it should stop." The plan stops the ROOT
// one tip-reach short of the intercept so the TIP lands the strike — and
// this probe reports where the posed tail actually ends at the impact key,
// against the plan's intercept. Comparison side only (art law): it says
// what IS; the render and the constants choose what SHOULD BE.
//
// For each attack-variant slot (33 dummy / 34 flyer / 35 six): decode the
// impact key (the clip's own kEvAttack event), skin the last stations and
// the blade-tip vertex of one tail blade, and print them beside the plan's
// intercept. Build exactly like zixx_probe (no cmake).
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

const zc::Clip* find_slot(const zc::CreatureType& T, uint16_t slot) {
  for (const zc::Clip& c : T.bank.clips)
    if (c.slot_id == slot) return &c;
  return nullptr;
}

void report(const zc::CreatureType& T, uint16_t slot, const char* name,
            int32_t icx, int32_t icy) {
  const zc::Clip* cp = find_slot(T, slot);
  if (!cp) { std::printf("%s: slot missing\n", name); return; }
  const zc::Clip& c = *cp;
  int impact_key = -1;
  for (const auto& ev : c.events)
    if (ev.event == zc::kEvAttack) impact_key = ev.frame;
  if (impact_key < 0) {
    std::printf("%s: no kEvAttack event\n", name);
    return;
  }
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(T, c, impact_key, pose, nullptr, 0);
  // decode_pose already bakes the clip root into the bone matrices -- the
  // first cut of this probe added it AGAIN and reported metre-scale
  // phantom gaps. Skinned positions below are world positions.
  const int32_t rootx = 0;
  const int32_t rooty = 0;
  std::printf("%s: impact key %d, plan intercept (%d, %d) mm\n", name,
              impact_key, icx, icy);
  // the NOSE (bone 0's joint) -- the plan's root drives this point, so the
  // measured nose->tip vector is the pose's own reach and baseline angle,
  // free of any reconstruction of the plan arithmetic
  int32_t nx = 0, ny = 0;
  {
    const zixx::Bind bd = zixx::head_station_bind(0);
    zc::SkinVertex v{-fxm(zixx::station_x(0)), fxm(zixx::kBodyY), 0,
                     bd.b0, bd.b1, bd.w0, 0, 0};
    int32_t x, y, z;
    zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
    nx = to_mm(x); ny = to_mm(y);
    std::printf("  nose        (%6d, %6d)\n", nx, ny);
  }
  // the last spine stations
  for (int i = 52; i <= 56; ++i) {
    const zixx::Bind bd = zixx::station_bind(i);
    zc::SkinVertex v{-fxm(zixx::station_x(i)), fxm(zixx::kBodyY), 0,
                     bd.b0, bd.b1, bd.w0, 0, 0};
    int32_t x, y, z;
    zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
    std::printf("  station %2d  (%6d, %6d)\n", i,
                to_mm(x + rootx), to_mm(y + rooty));
  }
  // the blade tip: a vertex at the far end of the left blade bone. The
  // blade rings run along the bone's local axis for kBladeLen mm from the
  // fork station; skinning the endpoint through the blade bone gives the
  // posed weapon tip.
  {
    zc::SkinVertex v{-fxm(zixx::station_x(56) + zixx::kBladeLen),
                     fxm(zixx::kBodyY), 0,
                     zixx::kBBladeL2, zixx::kBBladeL2, 64, 0, 0};
    int32_t x, y, z;
    zc::skin_vertex(pose.data(), v, x, y, z, nullptr);
    const int32_t tx = to_mm(x + rootx), ty = to_mm(y + rooty);
    const int64_t gx = icx - tx, gy = icy - ty;
    const int64_t rx2 = tx - nx, ry2 = ty - ny;
    std::printf("  blade tip   (%6d, %6d)   gap to intercept %lld mm\n", tx,
                ty,
                static_cast<long long>(
                    zref::isqrt_u64(static_cast<uint64_t>(gx * gx + gy * gy))));
    std::printf("  nose->tip: reach %lld mm, angle %.1f deg (the POSE's own"
                " weapon vector)\n",
                static_cast<long long>(zref::isqrt_u64(
                    static_cast<uint64_t>(rx2 * rx2 + ry2 * ry2))),
                std::atan2(static_cast<double>(ry2), static_cast<double>(rx2)) *
                    57.29578);
  }
}
}  // namespace

int main() {
  const zc::CreatureType& T = zixx::type();
  // the reel's fixed variant plans (build_attack_dummy/fly/six)
  report(T, 33, "salto-dummy (slot 33)", 4600, 350);
  report(T, 34, "salto-fly   (slot 34)", 3800, 3200);
  report(T, 35, "salto-six   (slot 35)", 5200, 0);
  return 0;
}
