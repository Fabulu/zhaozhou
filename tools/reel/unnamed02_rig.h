// Unnamed02 — the rig: bone ids + skeleton. 8 bones of the 32 allowed.
//
// Parent-before-child, rest rotations identity, bind a pure translation
// chain (the zref bind convention). The optional kBAttitude bone from the
// plan's §4.4 is CUT for now — the root alone carries hover + attitude; add
// it back only if clips want independent channels.

#ifndef ZHAO_REEL_UNNAMED02_RIG_H
#define ZHAO_REEL_UNNAMED02_RIG_H

#include "unnamed02_art.h"

namespace u02 {

enum BoneId : uint8_t {
  kBRoot = 0,    // body ball; hover translation + body attitude
  kBHingeA = 1,  // lower-front hinge ball + first loop arc
  kBHingeB = 2,  // peak hinge ball
  kBHingeC = 3,  // upper-rear hinge ball
  kBEyeL = 4,    // left lens
  kBEyeR = 5,    // right lens
  kBPupilL = 6,  // left star
  kBPupilR = 7,  // right star
};
constexpr int kBoneCount = 8;

/**
 * Bind translations: each hinge bone's pivot sits AT its ball's own centre
 * (the pivot-offset lesson — a pivot away from the mass makes the part orbit
 * a distant point). The chain runs root -> A -> B -> C, so translations are
 * deltas along the loop.
 */
inline zc::Skeleton build_skeleton() {
  zc::Skeleton sk;
  sk.bone_count = kBoneCount;
  sk.bones[kBRoot] = zc::Bone{kBRoot, 0, 0, 0};
  // Hinge pivots sit ON the straight-bound tube at the fold stations (the
  // fold is a pose; see the loop block in unnamed02_art.h). Pure +Y chain.
  sk.bones[kBHingeA] =
      zc::Bone{kBRoot, fxu(kLoopTubeXMm), fxu(kLoopNeckExitYMm + kLoopArcMm[0]), 0};
  sk.bones[kBHingeB] = zc::Bone{kBHingeA, 0, fxu(kLoopArcMm[1]), 0};
  sk.bones[kBHingeC] = zc::Bone{kBHingeB, 0, fxu(kLoopArcMm[2]), 0};
  sk.bones[kBEyeL] = zc::Bone{kBRoot, fxu(kEyeXMm), fxu(vmm(kEyeYMm)), fxu(kEyeZMm)};
  sk.bones[kBEyeR] = zc::Bone{kBRoot, fxu(kEyeXMm), fxu(vmm(kEyeYMm)), -fxu(kEyeZMm)};
  // Pupil pivots sit AT the lens centre; the star GEOMETRY is offset
  // outward (+X) in the part, so pupil-bone rotations sweep the star
  // across the lens face like an eyeball turning (the zixx gaze mechanism,
  // pivot radius = the bulge).
  sk.bones[kBPupilL] = zc::Bone{kBEyeL, 0, 0, 0};
  sk.bones[kBPupilR] = zc::Bone{kBEyeR, 0, 0, 0};
  return sk;
}

}  // namespace u02

#endif  // ZHAO_REEL_UNNAMED02_RIG_H
