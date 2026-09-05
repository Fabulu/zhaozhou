// Unnamed02 — creature 02, the floating mana conduit. THE KNOBS.
//
// Built from S. Hofer's two concept sheets (Upheaval/creature/Unnamed02/
// Concept/). Authored BY EYE per the art law: every value here is a starting
// orientation and an owner knob, never a measurement. Nothing is sampled
// from the scans.
//
// Millimetres unless noted. Angles in angle16: 65536 = one turn, 182 ~ 1 deg.
// Per-mille lanes are marked Pm.
//
// AXIS MAP: this creature is authored upright. Ball parts stack rings along
// local +Y with no quarter turns, so local == world: +X forward (the face),
// +Y up, +Z the creature's left. The antenna loop lives in the X-Y plane
// (flat in Z — the side sheet shows the broad loop, the front sheet a blade).

#ifndef ZHAO_REEL_UNNAMED02_ART_H
#define ZHAO_REEL_UNNAMED02_ART_H

namespace u02 {

// Q16.16 raw from millimetres (same rounding as the reel's fxm; local so the
// probe and the page tools can include these headers standalone).
constexpr int32_t fxu(int64_t milli) {
  return static_cast<int32_t>((milli * 65536 + (milli >= 0 ? 500 : -500)) / 1000);
}

// ============================== FORM =======================================

// THE PROJECTION ANISOTROPY (discovered at S4, the first world-sphere ever
// rendered here): the house camera maps isotropic NDC through a 384x240
// viewport, so one metre of world X paints ~1.66x the pixels of one metre of
// world Y. Zixxtrixx absorbed this silently (a long tube, proportions tuned
// by eye). This creature is BALLS -- so every vertical form dimension is
// authored in on-screen proportions and stretched by this knob at build
// time. A sphere becomes a prolate spheroid that READS round from any
// horizontal camera. Chosen by LOOKING at the rendered ball, not computed.
constexpr int kVStretchPm = 1660;
constexpr int32_t vmm(int32_t mm) {
  return static_cast<int32_t>((static_cast<int64_t>(mm) * kVStretchPm) / 1000);
}


// ---- the body ball (the big pink teardrop head) ----
constexpr int32_t kBodyRadiusMm = 450;
constexpr int kBodyRings = 11;
constexpr int kBodySegments = 16;      // at the equator
constexpr int kBodyPoleSegments = 16;  // uniform: the segment-taper zipper cut a
                                       // visible sliver into the face at 240p
// Teardrop reshaping (per-ring, ring 0 = bottom): radius multiplier in
// per-mille of the sphere ring, and a per-ring forward lean. 1000/0
// everywhere = the pure sphere (the S4 gate ball). Authored at the form
// milestone by eye against Side.png.
// PASS 2 (R1/eye recon): the sheet is a teardrop, not a sphere — the taper
// starts at the equator and pulls much harder, and the upper rings lean
// forward so the crown flows toward the neck (the lollipop fix's value half;
// the structural half is the loop shoulder in the model builder).
// PASS 3 (Direction 3 §4: "a bit more tear shape"): the upper taper pulls
// harder and the crown lean grows — judged beside the side sheet at
// matched height.
constexpr int kBodyTaperPm[kBodyRings] = {1000, 1008, 1015, 1012, 1000, 968,
                                          915,  838,  730,  595,  450};
constexpr int kBodyLeanXMm[kBodyRings] = {0, 0, 0, 0, 8, 20, 40, 70, 105, 145, 180};

// ---- the three hinge balls (the drawn nodes the loop articulates around) --
// PASS 3 (R12): the BALLS are the thickest points on the antenna — raised
// while the tube gauge drops 0.7x, so every ball reads visibly fatter than
// the tube it joins at native (Direction 3 §3).
constexpr int32_t kHingeRadiusMm = 100;
constexpr int kHingeRings = 7;
constexpr int kHingeSegments = 10;
constexpr int kHingePoleSegments = 10;  // uniform (same sliver lesson)
// The DRAWN hinge anchors (documentation: the fold angles and arc lengths
// above were derived from these sheet positions by eye). The BIND positions
// sit on the straight tube — see build_skeleton.
constexpr int32_t kHingeAXMm = 170, kHingeAYMm = 780;
constexpr int32_t kHingeBXMm = -120, kHingeBYMm = 1450;
constexpr int32_t kHingeCXMm = -620, kHingeCYMm = 1150;

// ---- the flat antenna loop ----
// THE LOOP BINDS STRAIGHT AND THE FOLD IS A POSE (the house law: a bind
// curve that doubles back shears every section; zixx's S is a stance, and
// so is this loop). One chain part sweeps straight up from the neck; the
// hinge bones sit ON the tube at the fold stations; constant fold
// rotations (loop_rest, called by every clip) bend tube AND hinge balls
// into the drawn shape. Articulation modulates the same rotations.
//
// PASS 2 STRUCTURE (R1 + R3, one job):
//  * the loop is SHRUNK (~0.6x — the eye recon refuted "a quarter short":
//    it was ~1.7x too LARGE; the tube gauge relative to the body was right)
//  * the fold count grows: neck (its own bone now — folding the antenna no
//    longer leans the body and the eyes), A, B, C and the new D on the
//    return arm, so the four-corner paperclip becomes a rounder pentagon
//  * the loop CLOSES BY CONSTRUCTION: loop_pose computes hinge D's fold in
//    closed form so the last segment always aims at the re-entry anchor —
//    no fold scale can detach the return arm (the dongle and the
//    punch-through become unrepresentable). Closed-form per-key arithmetic
//    is the house precedent (root compensation), not IK.
//  * the blade tapers per ring: broad shoulder flaring INTO the body at the
//    neck (the lollipop fix), slim over the peak, modest on the return.
constexpr int kLoopRings = 34;
constexpr int kLoopSegments = 8;
constexpr int32_t kLoopTubeXMm = 90;     // tube bind x (the neck exit)
constexpr int32_t kLoopNeckExitYMm = 664;   // STRETCHED units from here down
constexpr int32_t kLoopBuryMm = 250;        // the near end plunges into the body
// arc lengths along the tube (stretched space): neck->A, A->B, B->C, C->D,
// and D->end — the AIMED segment, long enough that the closure arithmetic
// keeps the arm end buried across the whole clip fold-scale range (the
// committed closure probe sweeps 780..1160 and asserts it).
constexpr int32_t kLoopArcMm[5] = {680, 340, 380, 380, 1420};
// fold angles at the neck exit and hinges A..C (angle16, about Z); hinge D
// has NO authored fold — loop_pose computes it per key (closure). Derived
// from the sheet's ring read (tall upright egg, W/H ~0.8), tuned by LOOKING.
constexpr int32_t kLoopFoldNeckA16 = 1450;    // ~8 deg back lean at the neck
constexpr int32_t kLoopFoldAA16 = 7280;       // ~40 deg at the front hinge
constexpr int32_t kLoopFoldBA16 = 11284;      // ~62 deg over the peak
constexpr int32_t kLoopFoldCA16 = 12740;      // ~70 deg at the rear hinge
// the re-entry anchor (body-local, the deep point the aimed segment plunges
// toward; also kBLoopBase2's bind — the drawn re-entry made a named joint)
constexpr int32_t kLoopReentryXMm = -230;
constexpr int32_t kLoopReentryYMm = 180;
// the drawn kink/lean lives in the REST POSE on the neck bone (R8): a small
// yaw opens the front view's slot-hole read and gives the antenna the
// sheet's asymmetric attitude; the rest tilt at A is the drawn front KINK.
constexpr int32_t kNeckRestYawA16 = 3300;     // ~18 deg loop-plane yaw
constexpr int32_t kLoopRestTiltA16 = 800;     // ~4 deg out-of-plane at A
// per-station blade radii (the taper): {buried base, neck, A, B, C, D, end}.
// rx = in the loop plane (the side sheet's tube gauge — roughly right
// before, kept ~105 mid-tube, flared at the shoulder); rz = across the
// plane (the FRONT sheet's blade: ~45% of body width at the base tapering
// toward a point over the peak).
// PASS 3 (R12): the mid-tube gauge drops ~0.7x (the owner: "way too
// thick"); the broad base flaring INTO the body and the thin tip stay.
constexpr int32_t kLoopBladeRxMm[7] = {190, 130, 66, 60, 66, 70, 91};
constexpr int32_t kLoopBladeRzMm[7] = {180, 150, 66, 32, 27, 29, 42};

// ---- the junction knuckles (R11 / Direction 3 §3: "there are no hinges
// where the antennae meet the creature") ----
// Two new hinge-ball parts make the body-side joints THINGS: one at the
// neck exit (on kBNeck — half-buried at the antenna's base) and one at the
// re-entry (on kBLoopBase2, offset from the deep anchor out to the visible
// surface crossing on the upper-left flank). Same construction as balls
// A/B/C, page tiles from birth (gotcha §0), sized into the ball family.
constexpr int32_t kKnuckleRadiusMm = 110;
constexpr int32_t kKnuckleNeckOffXMm = 0, kKnuckleNeckOffYMm = 0;
constexpr int32_t kKnuckleReentryOffXMm = -70, kKnuckleReentryOffYMm = 270;

// ---- the eyes (the whole face) ----
// Two big purple almond lenses close together on the lower front, angled
// outward in a V; a four-pointed cyan star rides a pupil bone inside each.
// The lens is REAL FACETED GEOMETRY (the direction's partly-polygonal read);
// the white rim is paint at the texture pass.
// PASS 2 (R2 + the eye recon): the sheet's eyes are a wide Λ — long pointed
// almonds converging at the TOP near the midline, splayed ~28° each, upper
// tips near 0.68 R above the ball centre. The old values drew a pinched V of
// two short pills turned 20° sideways. Width was already right (R2): the
// apparent narrowness was the yaw foreshortening, so the yaw is cut, not the
// width doubled. The PROTRUSION READ is protected (artist-approved): after
// growing/splaying, kEyeDeepMm/kEyeXMm were pulled back so the crown's
// stand-off read matches the shipped one (re-measured with u02-probe).
// PASS 3 (R2: the shipping READ governs; the trace is demoted to a sanity
// bracket). The pass-2 almond (330x92, 3.6:1) foreshortened into a splinter
// at the shipped three-quarter and its white ring stretched into the
// brightest arc on the creature. Shortened and FATTENED by eye at the
// shipping camera toward the sheet's plump teardrop; the apex is raised
// toward the sheet's high meeting point (the pass-2 residual). The lens is
// DOMED (kEyeDeepMm up = curvature, not tip stand-off) and partial outward
// yaw is restored (R4: zero yaw made each lens edge-on in profile — the
// owner: "invisible from the side; more 3D") — picked from a rendered
// 0/1200/2400/3600 ladder at front/three-quarter/side under the shipping
// sun. kEyeXMm is pulled back so the assembly sits inside the silhouette at
// three-quarter and the crown keeps the protected ~160 mm read (probed).
constexpr int32_t kEyeXMm = 381, kEyeYMm = 90, kEyeZMm = 190;  // centre, ±z
constexpr int32_t kEyeVAngleA16 = -4400;  // Λ: tips converge at the top (~24°)
constexpr int32_t kEyeYawOutA16 = 2400;   // partial outward yaw (R4 ladder pick)
constexpr int32_t kEyeTiltA16 = 2200;     // the almond's backward lean
constexpr int32_t kEyeBulgeMm = 88;       // pupil star stands proud of the lens
constexpr int32_t kEyeLongMm = 250;       // almond half-length (long axis)
constexpr int32_t kEyeWideMm = 125;       // almond half-width — the plump read
constexpr int32_t kEyeDeepMm = 90;        // bulge depth off the body (the dome)
constexpr int kEyeRings = 5;
constexpr int kEyeFacetSegments = 8;      // the facet read at 240p
// PASS 3 (R3): the star GROWS toward the sheet's ~20% lens share (it never
// grew in pass 2: 5.5-9.8% measured). Judged by eye on the lit path.
constexpr int32_t kPupilStarArmMm = 185;  // star arm half-length (the sheet's
constexpr int32_t kPupilStarThinMm = 26;  // fat organic star, not a thin cross)
constexpr int32_t kPupilStarWideMm = 72;  // blade width
constexpr uint8_t kLensR = 116, kLensG = 58, kLensB = 178;   // purple (grey pass)
// The star's SHIPPED pigment lives in mku02page.py (STAR_CYAN, same value):
// the star must carry a page because untextured parts render black under
// celmain (09-ENGINE-GOTCHAS.md §7). These stay as the pageless fallback.
constexpr uint8_t kStarR = 64, kStarG = 220, kStarB = 240;   // cyan

// ============================== MOTION =====================================
// keys are 30 Hz, held 2 sim ticks; a clip's frames on screen = keys * 2.
constexpr int kIdleKeys = 300;      // 600 frames, 10 s loop
constexpr int kDriftKeys = 150;
constexpr int kChannelKeys = 210;
constexpr int kCuriousKeys = 90;
constexpr int kStartleKeys = 80;
constexpr int kRestKeys = 200;
constexpr int kPirouetteKeys = 120;
// the hover: two incommensurate bobs (periods in keys; integer cycles/loop)
// PASS 2: the eye recon measured the old bob at 2–4 px over whole clips — a
// flat line. The floor is raised so motion clears the noise floor at 240p.
constexpr int32_t kBobAmpAMm = 90, kBobAmpBMm = 34;
constexpr int kBobPeriodAKeys = 25, kBobPeriodBKeys = 60;
// the constant compression (Q0.16 flatten peak; slight but UNMISTAKABLE —
// PASS 2: the old 3300 was ~4 px, swallowed by the toon band edge. Direction
// 2 §4 wants MORE stretch than Zixxtrixx.)
constexpr int32_t kCompressAmpPm = 9000;
constexpr int32_t kSpreadRatioPm = 550;    // the positive-volume partner
constexpr int kCompressPeriodKeys = 30;
constexpr int32_t kCompressLoopCouplePm = 14;  // sympathetic hinge-root bob
// the antenna's life
constexpr int32_t kAntennaSwayPm = 45;
constexpr int kAntennaLagKeys = 4;
constexpr int32_t kAntennaTiltA16 = 700;
// the gaze (the pupil pivots sweep the stars across the lenses)
// PASS 3 (F4, Direction 3 §2): star containment — the star plus its white
// ring must never cross the lens ink at any authored gaze extreme. The
// clamps are cut with the bigger star and proven by rendering the extremes.
constexpr int32_t kGazeMaxA16 = 3000;
constexpr int32_t kGazeLiftMaxA16 = 2000;
constexpr int32_t kSquintMaxA16 = 9000;    // 1000pm = mostly closed
constexpr int32_t kBlazeTwinkleA16 = 10923;  // the channel's slow star spin
// per-clip character
constexpr int32_t kDriftLeanA16 = 1300;
constexpr int32_t kDriftRadiusMm = 600;
constexpr int kDriftSwayPeriodKeys = 25;
constexpr int kDriftCompressPeriodKeys = 25;
constexpr int kChannelCompressPeriodKeys = 42;
constexpr int32_t kCuriousYawA16 = 4500;   // ~25 deg body yaw after the eyes
constexpr int32_t kStartleJumpMm = 520;
constexpr int32_t kStartleLiftMm = 300;
constexpr int kRestSwayPeriodKeys = 50;
constexpr int kRestCompressPeriodKeys = 50;
constexpr int kRestBobPeriodKeys = 40;
constexpr int32_t kRestSquintPm = 520;
constexpr int32_t kPirouetteFlarePm = 90;
// PASS 2 — the new clips (Direction 2 §5) and the eye-life floor (§4).
constexpr int kHastyKeys = 120;
constexpr int32_t kHastyRadiusMm = 900;    // wider, faster circuit than drift
constexpr int32_t kHastyPitchA16 = 2400;   // body pitched into the travel
constexpr int32_t kHastyBankA16 = 1900;    // clumsy bank
constexpr int32_t kHastyFishtailA16 = 1500;// the slight fishtail yaw wobble
constexpr int kHastyFishtailCycles = 8;
constexpr int kFallKeys = 100;
constexpr int32_t kFallHeightMm = 2400;    // blown this high above the hover
constexpr int kFallCatchKey = 70;          // the tumble ends, the catch begins
constexpr int32_t kFallStreamPm = 700;     // the antenna streams (folds open)
constexpr int kHitKeys = 70;
constexpr int32_t kHitKnockMm = 430;       // knocked back this far
constexpr int32_t kHitSquashXPm = 2600;    // impact squash, x kCompressAmpPm
constexpr int kTauntKeys = 140;
constexpr int32_t kTauntPlayA16 = 1250;    // hinge-D play swing (the closure holds)
constexpr int32_t kTauntWagglePm = 170;    // per-hinge asymmetric waggle depth
constexpr int kTaunt2Keys = 120;
constexpr int32_t kTaunt2LassoA16 = 1900;  // the loop-peak lasso tilt sweep
// the blink floor (the never-off life law): a quick lid pulse every period,
// staggered per clip by the offset so no two clips blink in sync
constexpr int kBlinkPeriodKeys = 96;
constexpr int kBlinkLenKeys = 5;
constexpr int32_t kBlinkDepthPm = 870;

// ============================== STAGE ======================================

constexpr int32_t kStageCentreMm = 0;     // the fixed camera aims at world x=0
// PASS 2: Direction 1's headline is IT FLOATS, and at 900 the belly ink sat
// on the dirt line (~150 mm of air = 1–2 px). Raised until there is VISIBLE
// air under the creature at native res, judged by looking.
constexpr int32_t kHoverHeightMm = 1250;  // body CENTRE above terrain
constexpr int32_t kRestHeightMm = 1130;   // the rest clip lower hover (body
                                          // half-height ~747 stretched; this
                                          // keeps real air even at bob minima)

// ============================== PAGE =======================================
// tiles: 0 = the atlas (body/loop/hinge V row bands), 1 = the eye page,
// 2 = the pupil-star page (flat cyan; exists because untextured parts render
// black under celmain — 09-ENGINE-GOTCHAS.md §7).
// Rows here must match mku02page.py's band layout.
constexpr uint8_t kPageAtlasTile = 0;
constexpr uint8_t kPageEyeTile = 1;
constexpr uint8_t kPageStarTile = 2;
constexpr uint8_t kBodyV0 = 8, kBodyV1 = 120;
constexpr uint8_t kLoopV0 = 136, kLoopV1 = 200;
constexpr uint8_t kHingeV0 = 208, kHingeV1 = 248;

// ============================== PALETTE ====================================
// Grey until the texture pass; the pinks are chosen by eye in scene at 240p.
constexpr uint8_t kGreyR = 150, kGreyG = 148, kGreyB = 152;
constexpr uint8_t kHingeGreyR = 132, kHingeGreyG = 130, kHingeGreyB = 136;

// ============================== EFFECTS ====================================
// The centre glow (S5): ONE shared ramp per frame, one baked sprite per
// process, one splat per conduit. Colours chosen by eye at 240p in scene.
constexpr uint8_t kGlowLo[3] = {0, 0, 0};        // BLACK floor: a ramp
                                                 // floor above zero rims
                                                 // every blob (gotcha §11)
constexpr uint8_t kGlowMid[3] = {150, 40, 200};  // the mana magenta body
constexpr uint8_t kGlowHi[3] = {255, 190, 255};  // white-pink core
constexpr int32_t kCentreGlowRadiusPx = 46;  // OUTER halo: rims the ~32 px body
constexpr int32_t kCentreGlowCorePx = 13;    // INNER core: shines THROUGH the
                                             // body (no depth test) -- the
                                             // light lives in the belly
constexpr int kCentreGlowCoreGainPm = 420;   // core is a tint, not a flood
constexpr int kCentreGlowGainPm = 380;
// S5 spike staging: three phantom conduit centres sharing one frame ramp
constexpr int32_t kS5PhantomOffsMm[3][2] = {{0, 0}, {-2700, -1400}, {2500, -2000}};

// ============================ END KNOBS ====================================

}  // namespace u02

#endif  // ZHAO_REEL_UNNAMED02_ART_H
