// REVIEWER probe (scratch, not committed to the implementer's file).
// Closes two gaps in u02_probe's closure pass:
//   (1) the bank loop tests sub=0 only -> the 60 Hz INTERPOLATED midpoints
//       are never closure-tested, and interpolating between two closed
//       poses is exactly how a shape opens mid-motion.
//   (2) it tests the last three ring CENTRELINE points only -> a tube whose
//       centre sits at 0.886 of the surface can still put its RIM outside,
//       since the terminal blade radius is 130 mm against a 450 mm body.
// Here: every real mesh vertex bound to hinge D that belongs to the terminal
// rings, at BOTH subs, over every clip.
#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"
namespace zc = zref::creature;
#include "unnamed02.h"

int main() {
  const zc::CreatureType& T = u02::type();
  const int32_t rx = u02::fxu(u02::kBodyRadiusMm);
  const int32_t ry = u02::fxu(u02::vmm(u02::kBodyRadiusMm));
  const int32_t y0 = u02::kLoopNeckExitYMm - u02::kLoopBuryMm;
  const int32_t total = u02::kLoopBuryMm + u02::kLoopArcMm[0] + u02::kLoopArcMm[1] +
                        u02::kLoopArcMm[2] + u02::kLoopArcMm[3] + u02::kLoopArcMm[4];
  // bind-space y of the last ring, and of ring kLoopRings-3
  const int32_t y_last = y0 + total;
  const int32_t y_cut  = y0 + (int32_t)(((int64_t)total * (u02::kLoopRings - 4)) / (u02::kLoopRings - 1));

  auto ellip_of = [&](const std::array<zc::mat3x4fx, zc::kMaxBones>& pose,
                      const zc::SkinVertex& sv) {
    int32_t x,y,z; zc::skin_vertex(pose.data(), sv, x,y,z,nullptr);
    const zc::mat3x4fx& rm = pose[u02::kBRoot];
    const int64_t dx=x-rm.m[3], dy=y-rm.m[7], dz=z-rm.m[11];
    const int64_t lx=(rm.m[0]*dx+rm.m[4]*dy+rm.m[8]*dz)>>16;
    const int64_t ly=(rm.m[1]*dx+rm.m[5]*dy+rm.m[9]*dz)>>16;
    const int64_t lz=(rm.m[2]*dx+rm.m[6]*dy+rm.m[10]*dz)>>16;
    const int64_t ex=(lx<<16)/rx, ey=(ly<<16)/ry, ez=(lz<<16)/rx;
    return (int32_t)((u02::isqrt64(ex*ex+ey*ey+ez*ez)*1000)>>16);
  };

  // collect real terminal-ring vertices bound to hinge D
  std::vector<zc::SkinVertex> term;
  for (const zc::Meshlet& m : T.mesh)
    for (const zc::SkinVertex& sv : m.verts)
      if ((sv.b0 == u02::kBHingeD || sv.b1 == u02::kBHingeD) &&
          sv.y >= u02::fxu(y_cut) && sv.y <= u02::fxu(y_last + 5))
        term.push_back(sv);
  std::printf("reviewer: %zu real terminal-ring vertices on hinge D (bind y %d..%d mm)\n",
              term.size(), y_cut, y_last);
  if (term.empty()) { std::printf("reviewer: NO terminal verts found - selection failed\n"); return 2; }

  int32_t worst_c=0, worst_r=0; int wcs=0,wcslot=0,wck=0, wrs=0,wrslot=0,wrk=0;
  for (const zc::Clip& clip : T.bank.clips) {
    for (uint16_t f=0; f<clip.frame_count; ++f) {
      for (uint8_t sub=0; sub<2; ++sub) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, clip, f, pose, nullptr, sub);
        // (a) their centreline metric, but at BOTH subs
        for (int ri=u02::kLoopRings-3; ri<u02::kLoopRings; ++ri) {
          const int32_t s=(int32_t)(((int64_t)total*ri)/(u02::kLoopRings-1));
          zc::SkinVertex sv{}; sv.x=u02::fxu(u02::kLoopTubeXMm); sv.y=u02::fxu(y0+s); sv.z=0;
          sv.b0=u02::kBHingeD; sv.b1=u02::kBHingeD; sv.w0=64;
          int32_t e=ellip_of(pose,sv);
          if(e>worst_c){worst_c=e;wcs=sub;wcslot=clip.slot_id;wck=f;}
        }
        // (b) the real tube RIM
        for (const zc::SkinVertex& sv : term) {
          int32_t e=ellip_of(pose,sv);
          if(e>worst_r){worst_r=e;wrs=sub;wrslot=clip.slot_id;wrk=f;}
        }
      }
    }
  }
  std::printf("reviewer: CENTRELINE worst %d pm (slot %d key %d sub %d) [their gate 920]\n",
              worst_c, wcslot, wck, wcs);
  std::printf("reviewer: TUBE RIM   worst %d pm (slot %d key %d sub %d) -- >1000 means the arm's\n"
              "          end surface is OUTSIDE the body ellipsoid (punch-through / open loop)\n",
              worst_r, wrslot, wrk, wrs);
  return 0;
}
