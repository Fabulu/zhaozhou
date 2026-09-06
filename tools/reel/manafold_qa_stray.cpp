#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>
#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"
#include "render_helpers.hpp"
#include "zrender/internal.hpp"
namespace zc = zref::creature;
#include "manafold.h"
int main(){
  const zc::CreatureType& T = u02::type();
  u02::Rig g; g.reset(); u02::loop_rest(g); u02::face_rest(g);
  zc::Clip cl; cl.slot_id=7; cl.frame_count=1;
  cl.quats.assign(u02::kBoneCount, zc::quat16_identity());
  cl.root.assign(3,0); cl.deform.assign(1, zc::DeformSample{});
  g.write(cl,0); cl.root[1]=u02::fxu(u02::kHoverHeightMm);
  std::array<zc::mat3x4fx, zc::kMaxBones> pose;
  zc::decode_pose(T, cl, 0, pose, nullptr, 0);
  std::printf("meshlets=%zu  root_y=%d mm  bones: EyeL=%d EyeR=%d PupilL=%d PupilR=%d\n",
    T.mesh.size(), u02::kHoverHeightMm, u02::kBEyeL,u02::kBEyeR,u02::kBPupilL,u02::kBPupilR);
  int mi=-1;
  for (const zc::Meshlet& m : T.mesh){
    ++mi;
    int stray=0, refd=0, total=(int)m.verts.size();
    long long minr2=1LL<<50; int minidx=-1;
    for (size_t k=0;k<m.verts.size();++k){
      const zc::SkinVertex& sv=m.verts[k];
      int32_t x,y,z; zc::skin_vertex(pose.data(), sv, x,y,z,nullptr);
      const long long mx=x>>16, my=y>>16, mz=z>>16;
      const long long r2=mx*mx+my*my+mz*mz;
      if (r2 < 100LL*100LL) {  // within 100 mm of the world origin
        ++stray;
        bool used=false;
        for (uint8_t v : m.idx) if (v==k) {used=true;break;}
        if (used) ++refd;
        if (stray<=3)
          std::printf("  mesh %2d rgb(%3d,%3d,%3d) vert %3zu bind(%d,%d,%d)fx b0=%d b1=%d w0=%d "
                      "-> world(%lld,%lld,%lld)mm  referenced-by-a-triangle=%s\n",
                      mi,m.r,m.g,m.b,k,sv.x,sv.y,sv.z,sv.b0,sv.b1,sv.w0,mx,my,mz,used?"YES":"no");
      }
      if (r2<minr2){minr2=r2;minidx=(int)k;}
    }
    if (stray) std::printf("  mesh %2d rgb(%3d,%3d,%3d): %d/%d verts within 100mm of ORIGIN, %d of them in triangles; idx entries=%zu\n",
                           mi,m.r,m.g,m.b,stray,total,refd,m.idx.size());
  }
  return 0;
}
