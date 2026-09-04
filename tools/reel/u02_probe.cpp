// u02_probe — the COMMITTED hover-clearance probe for creature 02.
//
// THE CONTRACT IS CLEARANCE, NOT PENETRATION (OWNER-DIRECTION-1: the
// creature FLOATS — no legs, no gait, no ground contact; Zixxtrixx's
// authored-penetration law does not apply). For EVERY clip, EVERY key AND
// every 60 Hz presentation midpoint, skin EVERY full-detail vertex through
// the decoded pose and assert
//
//     world_y(vertex) >= kMinClearanceMm
//
// where world y is measured from the instance's terrain-snap plane (the
// reel sets instance y to the column top; the clip root carries the hover).
// Terrain sloping away from the snap column only increases clearance, so
// the flat-plane bound is the conservative one.
//
// The constant is derived from the ACCEPTED motion (a window from named
// constants, never absolute ticks): the rest clip's low hover minus its bob
// leaves ~70 mm; the gate sits below it with honest headroom (the
// gate-fitted-to-its-own-answer trap).
//
// Committed because a thrown-away probe is unreproducible (CLAUDE.md).

#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_texture.hpp"

namespace zc = zref::creature;
#include "unnamed02.h"

int main() {
  constexpr int32_t kMinClearanceMm = 40;
  const zc::CreatureType& T = u02::type();
  if (T.mesh.empty()) {
    std::printf("u02-probe: FAIL compile produced no meshlets\n");
    return 1;
  }
  int rc = 0;
  for (const zc::Clip& clip : T.bank.clips) {
    int32_t worst = INT32_MAX;
    uint16_t worst_frame = 0;
    uint8_t worst_sub = 0;
    for (uint16_t f = 0; f < clip.frame_count; ++f) {
      for (uint8_t sub = 0; sub < 2; ++sub) {
        std::array<zc::mat3x4fx, zc::kMaxBones> pose;
        zc::decode_pose(T, clip, f, pose, nullptr, sub);
        const zc::DeformSample d = zc::deformation_sample(T, clip.slot_id, f, sub);
        for (const zc::Meshlet& m : T.mesh) {
          for (size_t vi = 0; vi < m.verts.size(); ++vi) {
            zc::SkinVertex sv = m.verts[vi];
            if (!m.deform.empty()) sv = zc::deform_skin_vertex(sv, m.deform[vi], d);
            int32_t x, y, z;
            zc::skin_vertex(pose.data(), sv, x, y, z, nullptr);
            if (y < worst) {
              worst = y;
              worst_frame = f;
              worst_sub = sub;
            }
          }
        }
      }
    }
    const int32_t worst_mm = static_cast<int32_t>((static_cast<int64_t>(worst) * 1000) >> 16);
    const bool ok = worst_mm >= kMinClearanceMm;
    std::printf("u02-probe: slot %u (%u keys): min clearance %d mm at key %u sub %u — %s\n",
                clip.slot_id, clip.frame_count, worst_mm, worst_frame, worst_sub,
                ok ? "OK" : "FAIL");
    if (!ok) rc = 1;
  }
  std::printf(rc == 0 ? "u02-probe: CLEARANCE CONTRACT HOLDS (>= %d mm everywhere)\n"
                      : "u02-probe: CLEARANCE VIOLATED (< %d mm)\n",
              kMinClearanceMm);
  return rc;
}
