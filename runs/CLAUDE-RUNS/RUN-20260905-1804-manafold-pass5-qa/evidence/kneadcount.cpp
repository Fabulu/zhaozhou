// QA probe (RUN-20260905-1804): count KNEAD frames per clip slot straight out
// of fold_phase, for EVERY slot -- not the three the pass-5 log sampled. This
// tests both halves of pass-5's item-3 claim in one run: that the short clips
// now knead, AND that clips whose first cycle already fitted are untouched.
// The fx lane evaluates fold_phase(slot, keys, frame*8) at 2 frames per key.
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
#include "render_helpers.hpp"
#include "zrender/internal.hpp"

namespace zc = zref::creature;

#include "manafold.h"
int main() {
  // slot -> key count, read off manafold-probe's own clip walk
  const int keys[15] = {300, 150, 210, 90, 80, 200, 120, 2,
                        120, 170, 70, 140, 120, 200, 232};
  for (int slot = 0; slot < 15; ++slot) {
    const int frames = keys[slot] * 2;
    int knead = 0, gather = 0, hold = 0, release = 0;
    for (int f = 0; f < frames; ++f) {
      const u02::FoldPhase ph = u02::fold_phase(slot, keys[slot], f * 8);
      switch (ph.seg) {
        case u02::kSegGather: ++gather; break;
        case u02::kSegHold: ++hold; break;
        case u02::kSegKnead: ++knead; break;
        case u02::kSegRelease: ++release; break;
      }
    }
    std::printf("slot %2d keys %3d frames %3d : knead %3d  gather %3d  hold %3d  release %3d\n",
                slot, keys[slot], frames, knead, gather, hold, release);
  }
  return 0;
}
