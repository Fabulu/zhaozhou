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

namespace u02 {

enum BoneId : uint8_t {
  kBRoot = 0,       // body ball; hover translation + body attitude
  kBJunctionF = 1,  // the FRONT JUNCTION (the old neck bind: the antenna's
                    // base at the body surface — carries the rest yaw/kink.
                    // PASS 11 (QA §7.4): it does NOT carry a junction ball.
                    // The rigid ball parts went at pass 6; the junction is a
                    // SWELL in the chain's own skin, and pass 11 F.4 made it
                    // a long low one so it reads as a thicker antenna rather
                    // than a bead.)
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
  // ---- PASS 12 WAVE 2a: THE EYE TRAVEL BONES (Direction 9 SS6, SS12.3) ----
  //
  // Two INERT bones on the body's own vertical axis, one per eye, inserted
  // between the root and the eye. At rest they are identity and their bind is a
  // pure translation to the axis, so the composed eye bind is unchanged to the
  // bit and pass 11's accepted face is reproduced exactly, not approximately.
  //
  // ⚠ THIS IS ALSO SS12.3's FIX, AND IT COSTS NOTHING EXTRA. The owner:
  //
  //   "the eye stuffs don't just need to rotate around the ball, they need to
  //    rotate around themselves. That's why the white and eyes vanish. They're
  //    basically 2d so at 45 degrees rotation they vanish."
  //
  // A bone rotation carries its CHILDREN'S FRAMES, not just their positions. So
  // travelling the eye by rotating a parent on the body axis turns the lens and
  // both stars through the same angle at the same time -- they arrive at the
  // new place already facing outward, because their local frame came with them.
  // The flat-plate-viewed-edge-on fault is a property of travel implemented as
  // a TRANSLATION along the surface; implemented as a rotation about the body
  // centre it cannot occur. On a sphere the surface normal IS the direction
  // from the centre, so "turn by the travel angle" and "rotate the parent" are
  // the same operation -- which is what the owner meant by "should be easier
  // now it's a ball", and why SS5 and SS6 are one piece of work.
  //
  // They stay ONE RIGID UNIT (SS5a/SS5b) by construction: nothing slides
  // against anything, because nothing moves relative to anything.
  kBEyeTravelL = 8,   // left eye's carrier, pivoting on the body axis
  kBEyeTravelR = 9,   // right eye's carrier
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
/** Integer square root, for the eye's bind radius. Local because the rig is a
 *  header included before the clip helpers that carry the other one. */
inline int64_t isqrt_i64(int64_t v) {
  if (v <= 0) return 0;
  int64_t x = v, y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + v / x) / 2;
  }
  return x;
}

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
  // ---- the travel carriers, on the body's vertical axis --------------------
  //
  // The pivot MUST be the centre of the sphere the eye is asked to trace, or
  // the eye leaves the surface as it travels. On the round body (SS5) that is
  // the root origin, and the knob is kept at zero rather than deleted because
  // it is the value that would have to move if the body ever leaned again.
  sk.bones[kBEyeTravelL] = zc::Bone{kBRoot, fxu(kEyeTravelPivotXMm), 0, 0};
  sk.bones[kBEyeTravelR] = zc::Bone{kBRoot, fxu(kEyeTravelPivotXMm), 0, 0};
  // ---- the eyes, now children of their carriers ----------------------------
  //
  // THE STANDOFF (D9 SS6.1, "maybe just remove them off the body a little").
  // Pushed along the HORIZONTAL RADIAL direction -- the (x, z) direction from
  // the body axis -- and not along y. That is deliberate and it is the same
  // fact that makes tracing work: a rotation about the body axis preserves the
  // horizontal radius exactly, so a standoff applied in that direction is
  // CONSTANT all the way round the travel. A standoff along the true ellipsoid
  // normal would be geometrically prettier and would drift as the eye moved.
  const int32_t ex0 = kEyeXMm, ez0 = kEyeZMm;
  const int32_t r0 = static_cast<int32_t>(
      isqrt_i64(static_cast<int64_t>(ex0) * ex0 + static_cast<int64_t>(ez0) * ez0));
  const int32_t exs = r0 > 0 ? ex0 + static_cast<int32_t>(
                                        (static_cast<int64_t>(ex0) * kEyeStandoffMm) / r0)
                             : ex0;
  const int32_t ezs = r0 > 0 ? ez0 + static_cast<int32_t>(
                                        (static_cast<int64_t>(ez0) * kEyeStandoffMm) / r0)
                             : ez0;
  sk.bones[kBEyeL] =
      zc::Bone{kBEyeTravelL, fxu(exs - kEyeTravelPivotXMm), fxu(vmm(kEyeYMm)), fxu(ezs)};
  sk.bones[kBEyeR] =
      zc::Bone{kBEyeTravelR, fxu(exs - kEyeTravelPivotXMm), fxu(vmm(kEyeYMm)), -fxu(ezs)};
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
