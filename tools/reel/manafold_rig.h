// MANAFOLD (creature 02) — the rig: bone ids + skeleton. 12 bones of the 32
// allowed.
//
// Parent-before-child, rest rotations identity, bind a pure translation
// chain (the zref bind convention).
//
// PASS 9 (Direction 7 §9.1) -- WHERE THE JOINTS ARE, and it is now checkable
// against where the BALLS are, which is the comparison nobody had made:
//
//     station          arc mm   what is there
//     junctionF/neck      250   the front junction + its ball  (TWO bones, one
//                               pivot: see kLoopArcMm's note on the 2*blend
//                               spacing rule that forces it)
//     hinge A             930   ball
//     hinge B            1270   ball
//     hinge C            1650   ball
//     (hinge D)          2030   the CLOSURE SOLVER, not a knead joint
//     (arm tip)          3300   buried in the body; not a station
//
// Every station the knead layer drives now sits on a ball or the front
// junction; none is left in a straight run. Pass 8 had fixed the MOTION of
// these stations, their PLACEMENT was still wrong, and those are different
// faults. ⚠ The RE-ENTRY ball (2660) still has no joint -- see kLoopArcMm for
// the closure measurement that blocked it. That half of §9.1 is owed.
//
// PASS 4 (Direction 4 §1: "wherever there is one of these balls, there needs
// to be bones to bend stuff" — the third direction raising the junction
// hinges): the FRONT JUNCTION becomes a real bone. The old kBNeck bind (the
// body-surface exit at the antenna's base) IS the new kBJunctionF — every
// accepted pivot stays exactly where it was — and a NEW kBNeck hinge joins
// midway up the lower tube, so the antenna has TWO body-side hinges
// (junction + neck) plus A/B/C: the "very mobile" hand the folding needs.
// Chain: root -> junctionF -> neck -> A -> B -> C -> D. Bending at the
// front junction bends the whole antenna, by construction.

#ifndef ZHAO_REEL_MANAFOLD_RIG_H
#define ZHAO_REEL_MANAFOLD_RIG_H

#include "manafold_art.h"
#include "manafold_eyelab.h"  // LANE-ONLY: kEyeTravelPivotXMm

namespace u02 {

enum BoneId : uint8_t {
  kBRoot = 0,       // body ball; hover translation + body attitude
  kBJunctionF = 1,  // the FRONT JUNCTION (the old neck bind: the antenna's
                    // base at the body surface — carries the rest yaw/kink
                    // and the junction ball; pass 4)
  kBNeck = 2,       // PASS 9 (Direction 7 §9.1): NO LONGER a mid-tube hinge.
                    // The owner looked at the site and said the kneading joint
                    // "is in the wrong place. It's in the straight antennae
                    // bit." It was: this bone sat at arc 586, with no ball
                    // within 340 mm of it. It now shares kBJunctionF's pivot
                    // exactly (kLoopArcMm[0] == 0), giving the FRONT JUNCTION a
                    // second, independently driven rotation instead of putting
                    // a crease in a smooth run. Still identity at rest.
  kBHingeA = 3,     // lower-front hinge ball + first loop arc
  kBHingeB = 4,     // peak hinge ball
  kBHingeC = 5,     // upper-rear hinge ball
  kBHingeD = 6,     // the return arm. NOT an articulation station: HingePlay
                    // and antenna_knead never touch it, and its fold is solved
                    // per frame by the closure aim in loop_pose. Pass 9 tried
                    // to move it onto the re-entry ball at 2660 and the loop
                    // stopped closing (989 -> 2401 pm, gate 1120); the sweep is
                    // recorded at kLoopArcMm.
  kBLoopBase2 = 7,  // the re-entry anchor (child of the BODY). PASS 10 C.4 --
                    // WHAT IS TRUE, replacing "carries the BACK-JUNCTION ball
                    // and gains authored rotation (pass 4)", which was false in
                    // both halves and shipped in four consecutive passes:
                    //   * it SKINS NOTHING. No make_* call binds a vertex to
                    //     it; the back ball became a swell on the chain's own
                    //     skin at pass 6, and nothing replaced it here.
                    //   * its rotation did nothing until PASS 10 either. The
                    //     closure aimed at kLoopReentryXMm/YMm, the BIND
                    //     constants, so kKneadWagB2A16 drove this bone every
                    //     frame and moved no pixel.
                    // It is now a real control: loop_pose aims at this bone's
                    // POSED anchor, so its rotation slides the re-entry point
                    // along the body surface and re-aims the whole return arm.
                    // Still no vertex is skinned to it -- that is deliberate,
                    // and the joint AT the re-entry ball remains a DECLARED
                    // GAP (see kLoopArcMm, and pass 10's C.2 prototype).
  // ---- EYE LAB (Direction 7 12.4), LANE-ONLY, INERT AT REST --------------
  // The two travel pivots. A bone rotates about ITS OWN origin, and the eye
  // bone's origin IS the lens centre, so nothing available on kBEyeL can make
  // the eye TRAVEL -- it can only spin the lens on the spot. These sit on the
  // BODY'S OWN VERTICAL AXIS instead, so a rotation here carries the whole eye
  // assembly (lens + both stars + their orientation) around the head on an arc.
  //
  // They must be inserted BEFORE the eye bones, not appended: the skeleton
  // contract is PARENT-BEFORE-CHILD, and appending them at 12/13 while their
  // children sit at 8..11 breaks it. Every reference to these ids is symbolic
  // (checked: nothing in the tree indexes a manafold bone by literal), so the
  // renumber is safe and the rest pose is bit-identical -- identity quats on a
  // (0, 0, 0)-relative bind change nothing.
  //
  // The X of the axis is kEyeTravelPivotXMm, NOT zero: the body's horizontal
  // section is a circle, but the teardrop lean puts its centre 33 mm forward at
  // the eye's height, and swinging about x=0 would drag the eye through that
  // lean and sink it. See manafold_eyelab.h.
  kBEyeTravelL = 8,
  kBEyeTravelR = 9,
  kBEyeL = 10,      // left lens
  kBEyeR = 11,      // right lens
  kBPupilL = 12,    // left star
  kBPupilR = 13,    // right star
};
constexpr int kBoneCount = 14;

/**
 * Bind translations: each hinge bone's pivot sits AT its ball's own centre
 * (the pivot-offset lesson — a pivot away from the mass makes the part orbit
 * a distant point). The chain runs root -> junctionF -> neck -> A -> B -> C
 * -> D, so translations are deltas along the straight-bound loop
 * (kLoopArcMm, now six spans: junction->neck, neck->A, A->B, B->C, C->D,
 * D->end).
 */
inline zc::Skeleton build_skeleton() {
  zc::Skeleton sk;
  sk.bone_count = kBoneCount;
  sk.bones[kBRoot] = zc::Bone{kBRoot, 0, 0, 0};
  // The front junction takes the OLD neck bind verbatim: accepted pivots
  // (the lasso swing, the drift trail, the drawn kink) stay where they were.
  sk.bones[kBJunctionF] =
      zc::Bone{kBRoot, fxu(kLoopTubeXMm), fxu(kLoopNeckExitYMm), 0};
  sk.bones[kBNeck] = zc::Bone{kBJunctionF, 0, fxu(kLoopArcMm[0]), 0};
  sk.bones[kBHingeA] = zc::Bone{kBNeck, 0, fxu(kLoopArcMm[1]), 0};
  sk.bones[kBHingeB] = zc::Bone{kBHingeA, 0, fxu(kLoopArcMm[2]), 0};
  sk.bones[kBHingeC] = zc::Bone{kBHingeB, 0, fxu(kLoopArcMm[3]), 0};
  sk.bones[kBHingeD] = zc::Bone{kBHingeC, 0, fxu(kLoopArcMm[4]), 0};
  // The re-entry anchor is a child of the BODY at the deep plunge target.
  sk.bones[kBLoopBase2] =
      zc::Bone{kBRoot, fxu(kLoopReentryXMm), fxu(kLoopReentryYMm), 0};
  // PASS 6 (Direction 5 §5c): the eye bone's pivot moves INWARD by
  // kEyeShiftPivotMm and make_eye_lens pushes the lens geometry back out by the
  // same amount. Rest is bit-identical; what changes is that a rotation here
  // now sweeps the eye ACROSS the body surface -- "the eye itself can move a
  // bit too" -- instead of spinning the lens on the spot. The rig authors
  // rotations only, so this is how a shift is expressed.
  // EYE LAB: the travel pivots, on the body's own axis at the eye's height.
  // Bind is a pure translation like every other bone; at identity the eye
  // bones below land exactly where they always did.
  sk.bones[kBEyeTravelL] =
      zc::Bone{kBRoot, fxu(eyelab::kEyeTravelPivotXMm), fxu(vmm(kEyeYMm)), 0};
  sk.bones[kBEyeTravelR] =
      zc::Bone{kBRoot, fxu(eyelab::kEyeTravelPivotXMm), fxu(vmm(kEyeYMm)), 0};
  // The eye bones are now children of their travel pivot, so their bind is the
  // DELTA from the axis rather than from the root. Same world position.
  sk.bones[kBEyeL] = zc::Bone{kBEyeTravelL, fxu(kEyeXMm - eyelab::kEyeTravelPivotXMm),
                              0, fxu(kEyeZMm)};
  sk.bones[kBEyeR] = zc::Bone{kBEyeTravelR, fxu(kEyeXMm - eyelab::kEyeTravelPivotXMm),
                              0, -fxu(kEyeZMm)};
  // Pupil pivots sit AT the lens centre; the star GEOMETRY is offset
  // outward (+X) in the part, so pupil-bone rotations sweep the star
  // across the lens face like an eyeball turning (the zixx gaze mechanism,
  // pivot radius = the bulge).
  // ...and the pupil pivot takes the matching offset, so the gaze pivot stays
  // exactly at the LENS centre and the star's mechanism is untouched by §5c.
  sk.bones[kBPupilL] = zc::Bone{kBEyeL, fxu(kEyeShiftPivotMm), 0, 0};
  sk.bones[kBPupilR] = zc::Bone{kBEyeR, fxu(kEyeShiftPivotMm), 0, 0};
  return sk;
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_RIG_H
