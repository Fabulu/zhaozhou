// Unnamed02 — creature 02, the floating mana-conduit companion. COMPOSITION.
//
// The only header zhao_reel.cpp includes for this creature. It must sit
// after `namespace zc = zref::creature;`, the same contract zixxtrixx.h
// uses. The headers are split for the ownership migration: a later move
// into Upheaval/creature/Unnamed02/source/ is a git mv, not a rewrite.
//
//   unnamed02_art.h    every named knob
//   unnamed02_model.h  make_ball(), the body, the hinges (the loop + eyes
//                      land at the form milestone)
//   unnamed02_rig.h    bone ids + skeleton
//   unnamed02_clips.h  maths helpers (sanctioned copy) + clip builders
//
// The creature FLOATS (OWNER-DIRECTION-1): no gait, no attacks, no ground
// contact — the probe asserts clearance, and Zixxtrixx's ground-contact law
// is declared not applicable in CREATURE.json.

#ifndef ZHAO_REEL_UNNAMED02_H
#define ZHAO_REEL_UNNAMED02_H

#include "unnamed02_art.h"
#include "unnamed02_model.h"
#include "unnamed02_rig.h"
#include "unnamed02_clips.h"
#include "unnamed02_fx.h"

namespace u02 {

inline const zc::CreatureType& type() {
  static const zc::CreatureType t = [] {
    zc::Skeleton sk = build_skeleton();

    std::vector<zc::RingPart> parts;
    parts.push_back(make_body(kBRoot));
    parts.push_back([] {
      zc::RingPart p = make_hinge(kBHingeA);
      return p;
    }());
    parts.push_back(make_hinge(kBHingeB));
    parts.push_back(make_hinge(kBHingeC));
    parts.push_back(make_loop());
    parts.push_back(make_lens(kBEyeL));
    parts.push_back(make_lens(kBEyeR));
    parts.push_back(make_star_blade(kBPupilL, false));
    parts.push_back(make_star_blade(kBPupilL, true));
    parts.push_back(make_star_blade(kBPupilR, false));
    parts.push_back(make_star_blade(kBPupilR, true));
    // Hinge parts are rigid on their own bones whose bind translation IS the
    // ball centre; the loop is one straight-bound chain the fold pose bends.

    zc::ClipBank bank;
    bank.bone_count = kBoneCount;
    bank.clips.push_back(build_still());

    zc::CreatureType type;
    type.type_id = 3;  // 1 watchdog, 2 zixxtrixx, 3 unnamed02
    const char* reason = "";
    if (!zc::compile_creature(sk, bank, parts, type, &reason)) {
      std::fprintf(stderr, "unnamed02: compile failed: %s\n", reason);
    }
    return type;
  }();
  return t;
}

}  // namespace u02

#endif  // ZHAO_REEL_UNNAMED02_H
