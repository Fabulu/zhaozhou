// MANAFOLD (creature 02) — the rig: bone ids + skeleton. 8 bones of the 32 allowed.
//
// Parent-before-child, rest rotations identity, bind a pure translation
// chain (the zref bind convention). The optional kBAttitude bone from the
// plan's §4.4 is CUT for now — the root alone carries hover + attitude; add
// it back only if clips want independent channels.

#ifndef ZHAO_REEL_MANAFOLD_RIG_H
#define ZHAO_REEL_MANAFOLD_RIG_H

#include "manafold_art.h"

namespace u02 {

// PASS 2 (Direction 2 §1b: every hinge — including the body-connected
// parts — needs a BONE): 8 -> 11 of the 32 allowed.
//  * kBNeck moves the neck fold OFF kBRoot, so folding the antenna no
//    longer leans the whole body and the eyes with it (the worst missing
//    bone in the first pass).
//  * kBHingeD makes the return arm a driven limb instead of C's rigid tail.
//  * kBLoopBase2 is the drawn re-entry made a named joint: its bind IS the
//    closure anchor the aimed segment plunges toward, and effects (the
//    caged pulsar) can anchor on it so hinge play moves the light.
enum BoneId : uint8_t {
  kBRoot = 0,      // body ball; hover translation + body attitude
  kBNeck = 1,      // the loop's neck exit (the antenna's own base joint)
  kBHingeA = 2,    // lower-front hinge ball + first loop arc
  kBHingeB = 3,    // peak hinge ball
  kBHingeC = 4,    // upper-rear hinge ball
  kBHingeD = 5,    // the return arm (fold computed by closure in loop_pose)
  kBLoopBase2 = 6, // the re-entry anchor (child of the BODY)
  kBEyeL = 7,      // left lens
  kBEyeR = 8,      // right lens
  kBPupilL = 9,    // left star
  kBPupilR = 10,   // right star
};
constexpr int kBoneCount = 11;

/**
 * Bind translations: each hinge bone's pivot sits AT its ball's own centre
 * (the pivot-offset lesson — a pivot away from the mass makes the part orbit
 * a distant point). The chain runs root -> neck -> A -> B -> C -> D, so
 * translations are deltas along the straight-bound loop.
 */
inline zc::Skeleton build_skeleton() {
  zc::Skeleton sk;
  sk.bone_count = kBoneCount;
  sk.bones[kBRoot] = zc::Bone{kBRoot, 0, 0, 0};
  // Hinge pivots sit ON the straight-bound tube at the fold stations (the
  // fold is a pose; see the loop block in manafold_art.h). Pure +Y chain.
  sk.bones[kBNeck] = zc::Bone{kBRoot, fxu(kLoopTubeXMm), fxu(kLoopNeckExitYMm), 0};
  sk.bones[kBHingeA] = zc::Bone{kBNeck, 0, fxu(kLoopArcMm[0]), 0};
  sk.bones[kBHingeB] = zc::Bone{kBHingeA, 0, fxu(kLoopArcMm[1]), 0};
  sk.bones[kBHingeC] = zc::Bone{kBHingeB, 0, fxu(kLoopArcMm[2]), 0};
  sk.bones[kBHingeD] = zc::Bone{kBHingeC, 0, fxu(kLoopArcMm[3]), 0};
  // The re-entry anchor is a child of the BODY at the deep plunge target.
  sk.bones[kBLoopBase2] =
      zc::Bone{kBRoot, fxu(kLoopReentryXMm), fxu(kLoopReentryYMm), 0};
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

#endif  // ZHAO_REEL_MANAFOLD_RIG_H
