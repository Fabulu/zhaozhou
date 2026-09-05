// MANAFOLD — creature 02, the floating mana conduit that FOLDS ITS MANA.
// THE KNOBS.
//
// NAMING (pass 4, R13): the creature is Manafold on every owner-facing
// surface — files, subjects, site, docs. The `u02::` namespace, `kU02*`
// constants, `U02_*` env lanes, `u02-s*` diagnostic subjects and the
// kUnnamed02 species enum are KEPT: they are creature-02 shorthand, not
// the placeholder name, and churning them buys no owner-visible value
// while risking the shared reel file.
//
// Built from S. Hofer's two concept sheets (Upheaval/creature/Manafold/
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

#ifndef ZHAO_REEL_MANAFOLD_ART_H
#define ZHAO_REEL_MANAFOLD_ART_H

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
// arc lengths along the tube (stretched space): junctionF->neck, neck->A,
// A->B, B->C, C->D, and D->end — the AIMED segment, long enough that the
// closure arithmetic keeps the arm end buried across the whole clip
// fold-scale range (the committed closure probe sweeps 700..1160 and
// asserts it). PASS 4: the old neck->A span (680) splits at the NEW neck
// hinge (336 + 344 = 680), so every drawn station stays where it was.
constexpr int32_t kLoopArcMm[6] = {336, 344, 340, 380, 380, 1420};
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
// per-station blade radii (the taper): {buried base, junctionF, neck, A,
// B, C, D, end}. rx = in the loop plane, rz = across it (the FRONT
// sheet's blade).
// PASS 4 (Direction 4 §1 thickness): "the frontmost part is still too
// thick — thin, thickening only a little, VERY CLOSE to where it meets
// the creature". The flare now lives only at the junction station itself
// (the ball masks the entry); the free tube runs at the mid gauge from
// just above the junction ball. The back "thickens out a small amount"
// at the re-entry (end station up a little, still under the back ball).
// Brackets, judged by eye at native beside the sheets.
constexpr int32_t kLoopBladeRxMm[8] = {130, 78, 64, 62, 60, 66, 72, 82};
constexpr int32_t kLoopBladeRzMm[8] = {140, 76, 52, 40, 32, 27, 34, 46};

// ---- the junction balls (PASS 4, Direction 4 §1: "the ball inside the
// antenna is completely wrong — remove it. The other is almost right — it
// belongs where the antenna meets the creature at the BACK. Add one where
// the antenna meets the creature at the FRONT.") ----
// The FRONT ball rides kBJunctionF (a real hinge — its bind IS the
// antenna's base at the body surface), half-buried in the crown at the
// visible surface crossing. The BACK ball rides kBLoopBase2, offset from
// the deep closure anchor out to the probed posed surface crossing on the
// upper-left flank, then placed finally BY EYE. Both are hinge-family
// balls with page tiles from birth (gotcha §0).
constexpr int32_t kKnuckleRadiusMm = 118;
// front-junction ball offset from kBJunctionF's bind (small: the bind is
// already the surface exit; the offset rides the ball up the tube a touch
// so it straddles the crown surface — by eye)
constexpr int32_t kJunctionFBallOffYMm = 70;  // centres on the probed crossing (83, 735)
constexpr int32_t kKnuckleReentryOffXMm = -100, kKnuckleReentryOffYMm = 285;  // probed crossing (-328, 467); eye adjusts

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
constexpr int32_t kEyeXMm = 381, kEyeYMm = 90, kEyeZMm = 215;  // centre, ±z
                                          // (pass 4: +25 z separation --
                                          // "they touch at the top. They
                                          // must not." -- plus the V angle
                                          // eased below)
constexpr int32_t kEyeVAngleA16 = -3600;  // Λ: tips converge at the top (eased
                                          // from -4400: the apexes were meeting)
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
// PASS 4 (Stage E). The reviewer's arithmetic: arm 185 vs lens half-width
// 125 -- the star could not fit across the short axis, so no clamp could
// ever contain it. The arms are now PER-AXIS and sized to FIT: the long
// arm rides the lens long axis, the short arm stays under the half-width
// minus the white ring's territory.
constexpr int32_t kPupilStarArmLongMm = 150;   // along the lens long axis
constexpr int32_t kPupilStarArmShortMm = 88;   // across: <= ~80% of
                                               // (kEyeWideMm - ring margin)
constexpr int32_t kPupilStarThinMm = 26;  // blade thickness (depth)
constexpr int32_t kPupilStarWideMm = 64;  // blade width
// the white annulus (X1/X2): a flat torus riding the PUPIL bone between
// star and lens -- the whites trace the pupils BY CONSTRUCTION, which is
// the tracking requirement the static page could never meet.
constexpr int32_t kWhiteRingRMm = 112;     // major radius (rings the star)
constexpr int32_t kWhiteRingTubeMm = 15;   // tube gauge (~1-2 px)
constexpr int32_t kWhiteRingOffXMm = 52;   // sits between lens face and star
constexpr int kWhiteRingSegs = 14;         // stations around the torus
// the X1 TEARDROP profile (Direction 4: "very pointy at the top, bottom is
// more round... too simple of a primitive"): per-ring width in pm of
// kEyeWideMm, bottom ring first. The depth (dome) follows the same
// profile. kEyeApexSharpPm pulls the last rings toward the apex point.
constexpr int kEyeRings2 = 10;
constexpr int kEyeRingWidthPm[kEyeRings2] = {330, 620, 830, 950, 1000,
                                             940,  800, 590, 340, 120};
constexpr int kEyeApexSharpPm = 780;  // >1000 widens the tip gap; <1000 sharpens
constexpr uint8_t kLensR = 116, kLensG = 58, kLensB = 178;   // purple (grey pass)
// The star's SHIPPED pigment lives in mkmanafoldpage.py (STAR_CYAN, same value):
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
// PASS 3 (Direction 3 §7 idle: "a bit too fast and nervous"): periods up
// ~1.5x, amplitudes kept.
constexpr int kBobPeriodAKeys = 37, kBobPeriodBKeys = 95;
// the constant compression (Q0.16 flatten peak; slight but UNMISTAKABLE —
// PASS 2: the old 3300 was ~4 px, swallowed by the toon band edge. Direction
// 2 §4 wants MORE stretch than Zixxtrixx.)
constexpr int32_t kCompressAmpPm = 9000;
constexpr int32_t kSpreadRatioPm = 550;    // the positive-volume partner
constexpr int kCompressPeriodKeys = 30;
constexpr int32_t kCompressLoopCouplePm = 14;  // sympathetic hinge-root bob
// PASS 3 — THE WHOLE-CREATURE WOBBLE (Direction 3 §4), mechanically: a
// slow bend STARTS at the loop peak (hinge B leads), travels down through
// C and A into the neck, and ARRIVES IN THE BODY as a lean-plus-squash a
// few keys later — front leads, bottom follows. Two incommensurate periods
// (the 46/102-frame class at 60 fps = 23/51 keys); root PITCH (up/down
// angling) rides the slow wave alongside the existing yaw channels.
constexpr int32_t kWobbleAmpPm = 130;      // hinge-scale swing (per station)
constexpr int32_t kWobbleLeanA16 = 950;    // the body lean, arriving late
constexpr int32_t kWobblePitchA16 = 780;   // root pitch (up/down angling)
constexpr int kWobbleLagKeys = 5;          // per-station arrival lag
constexpr int kWobblePerAKeys = 23;        // ~46 frames on screen
constexpr int kWobblePerBKeys = 51;        // ~102 frames on screen
// the antenna's life
constexpr int32_t kAntennaSwayPm = 45;
constexpr int kAntennaLagKeys = 4;
constexpr int32_t kAntennaTiltA16 = 700;
// the gaze (the pupil pivots sweep the stars across the lenses)
// PASS 3 (F4, Direction 3 §2): star containment — the star plus its white
// ring must never cross the lens ink at any authored gaze extreme. The
// clamps are cut with the bigger star and proven by rendering the extremes.
// PASS 4 (Stage E): containment is ARITHMETIC. The star's centre sweeps
// z ~= kEyeBulgeMm * sin(gaze/2 in angle16 halves); the short arm (88) +
// the white ring tube (15) must stay inside the lens half-width (125):
// allowed z travel ~= 125 - 88 - 15 = 22 mm -> sin = 22/88 = 0.25 ->
// ~14.5 deg full angle ~= 2640 angle16. Held under it with margin, then
// PROVEN by rendering the authored extremes (the eye places the value).
constexpr int32_t kGazeMaxA16 = 2400;
// lift: along the long axis the room is (250*0.93 - 150 - 15) ~= 67 mm ->
// sin = 67/88 = 0.76; the practical clamp stays far below (the squint and
// the V-angle eat into it) -- picked by the same margin discipline.
constexpr int32_t kGazeLiftMaxA16 = 2000;
constexpr int32_t kSquintMaxA16 = 9000;    // 1000pm = mostly closed
constexpr int32_t kBlazeTwinkleA16 = 10923;  // the channel's slow star spin
// per-clip character
// PASS 3 (Direction 3 §7 drift: "just rotating. That is not how it
// works."): rebuilt as a WIND-BLOWN LATERAL GLIDE — the body banks into a
// sideways slide, translates across the shot in a lazy S, the antenna
// trails against the travel, and it over-banks and recovers twice.
constexpr int32_t kDriftBankA16 = 2600;      // the working bank into the slide
constexpr int32_t kDriftOverBankA16 = 1500;  // the two over-bank corrections
constexpr int32_t kDriftSpeedMmPerKey = 46;  // lateral ground covered per key
constexpr int32_t kDriftSCurveMm = 700;      // the lazy S's fore-aft swing
constexpr int32_t kDriftTrailA16 = 1400;     // antenna trailing off-plane
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
// PASS 3 (Direction 3 §7: "do not run in a circle. Run in ONE DIRECTION"):
// straight-line travel crossing the fixed shot, the Zixxtrixx walk staging
// precedent — start half the travel back, cross through centre.
constexpr int32_t kHastySpeedMmPerKey = 70;  // ~8.4 m across the clip
constexpr int32_t kHastyPitchA16 = 2400;   // body pitched into the travel
constexpr int32_t kHastyBankA16 = 1900;    // clumsy bank
constexpr int32_t kHastyFishtailA16 = 1500;// the slight fishtail yaw wobble
constexpr int kHastyFishtailCycles = 8;
// PASS 3 (Direction 3 §7: "make it longer"): keys 100 -> 170, higher
// start, and an extra tumble axis (a slow yaw under the pitch tumble).
constexpr int kFallKeys = 170;
constexpr int32_t kFallHeightMm = 3600;    // blown this high above the hover
constexpr int kFallCatchKey = 130;         // the tumble ends, the catch begins
constexpr int32_t kFallYawTumbleA16 = 21845;  // the extra axis: 1/3 turn over the drop
constexpr int32_t kFallStreamPm = 700;     // the antenna streams (folds open)
constexpr int kHitKeys = 70;
constexpr int32_t kHitKnockMm = 430;       // knocked back this far
constexpr int32_t kHitSquashXPm = 2600;    // impact squash, x kCompressAmpPm
constexpr int kTauntKeys = 140;
constexpr int32_t kTauntPlayA16 = 1900;    // hinge-D play swing (the closure holds)
// PASS 3 (Direction 3 §7: "can be more fun"): comedy is the HOLD — a big
// anticipation wind-up, then the waggle FROZEN at its extreme for a
// readable beat with the wink, then a smug settle-bob.
constexpr int32_t kTauntWagglePm = 380;    // per-hinge asymmetric waggle depth
constexpr int kTauntWindupEndKey = 36;     // anticipation: crouch + pull back
constexpr int kTauntHoldStartKey = 62;     // the waggle freezes at its extreme
constexpr int kTauntHoldEndKey = 86;       // >= 16-key beat, wink inside it
constexpr int kTaunt2Keys = 120;
constexpr int32_t kTaunt2LassoA16 = 1900;  // the loop-peak lasso tilt sweep
// PASS 3 — THE HEADSTAND TRICK (owner-suggested, uncuttable): it pitches
// over, PLANTS the loop peak on the ground (declared, authored contact —
// the probe asserts the window and depth), balances upside down with the
// body wobbling above and the antenna flexing at the junction hinges,
// then rights itself with overshoot.
constexpr int kTrickKeys = 200;
constexpr int32_t kTrickPlantRootMm = 1670;   // root height while planted: the
                                              // loop peak (~1665 above root,
                                              // inverted) meets the dirt with
                                              // the declared penetration
constexpr int32_t kTrickPlantDepthMm = 25;    // DECLARED penetration at plant
constexpr int kTrickFlipStartKey = 42;        // the pitch-over begins
constexpr int kTrickPlantKey = 78;            // contact window opens
constexpr int kTrickLiftKey = 156;            // contact window closes (the
                                              // peak DRAGS a few keys into
                                              // the righting — probed)
constexpr int kTrickHomeKey = 186;            // righted (overshoot inside)
constexpr int32_t kTrickBalanceWobbleA16 = 900;  // inverted-pendulum sway
constexpr int32_t kTrickOvershootA16 = 2600;     // the righting overshoot

// PASS 4 (Stage H, Direction 4 §3b): DIRECTIONAL HITS. Four named
// authored contact stations in one clip, the zixxtrixx-damage precedent;
// no runtime collision -- the contact point is authored per station. It
// FLOATS: recoil is displacement + overshoot + a slow damped settle in
// the air. The struck side leads; the antenna whips opposite a beat
// later; the mana coupling shatters the held shape for free.
constexpr int kDamageKeys = 232;
constexpr int kDamageHitKeys[4] = {12, 70, 128, 186};  // one blow per station
// stations: 0 body-front (+X blow), 1 body-side (+Z), 2 body-back (-X),
// 3 LOOP-PEAK (struck on the antenna -- the loop takes it, the body
// follows late and less)
constexpr int32_t kDamageKnockMm = 520;      // body displacement per blow
constexpr int32_t kDamagePeakKnockMm = 260;  // the loop-peak hit moves the body less
constexpr int32_t kDamageWhipPm = 320;       // antenna whip depth (fold-scale)
constexpr int32_t kDamagePeakWhipPm = 620;   // ...and when struck ON the loop
constexpr int kDamageWhipLagKeys = 2;        // the whip trails the blow
constexpr int32_t kDamageWinceSquintPm = 780;
constexpr int32_t kDamageSquashPm = 2200;    // impact squash, x kCompressAmpPm

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
// Rows here must match mkmanafoldpage.py's band layout.
constexpr uint8_t kPageAtlasTile = 0;
constexpr uint8_t kPageEyeTile = 1;
constexpr uint8_t kPageStarTile = 2;
constexpr uint8_t kBodyV0 = 8, kBodyV1 = 120;
constexpr uint8_t kLoopV0 = 136, kLoopV1 = 200;
constexpr uint8_t kHingeV0 = 208, kHingeV1 = 248;
constexpr uint8_t kWhiteV0 = 250, kWhiteV1 = 254;  // pass 4: the white ring band

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
// PASS 4 (Direction 4 §4: "the glowy bit inside the creature: make it go
// away"): the belly glow's SUBJECT gain, held at 0. This is the revert
// path — any positive value re-lights the belly through the untouched
// glow machinery (the mana keeps using glow_splat regardless).
constexpr int kBellyGlowGainPm = 0;
// S5 spike staging: three phantom conduit centres sharing one frame ramp
constexpr int32_t kS5PhantomOffsMm[3][2] = {{0, 0}, {-2700, -1400}, {2500, -2000}};

// ================= THE FOLDING (pass 4 centrepiece knobs) ==================
// Manafold FOLDS ITS MANA: the antenna is the hand, the mana is a stencil
// of fat glow motes at FIXED barycentric weights over the posed antenna
// anchors -- the shape folds because the rig folds, by construction -- and
// GRIP / KNEAD / DRAG derive purely from joint state. The choreography
// layer antenna_knead (manafold_clips.h) runs the fold-hold-knead loop in
// EVERY clip; the mote system (manafold_fx.h) reads only the posed joints.

// deterministic hash (shared by the choreography timeline and the fx lane;
// zlib-free, integer). Moved here from the fx header so both can see it.
inline uint32_t fx_hash(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 0x9E3779B9u ^ b * 0x85EBCA6Bu ^ c * 0xC2B2AE35u;
  h ^= h >> 15;
  h *= 0x2545F491u;
  h ^= h >> 13;
  return h;
}
inline int32_t fx_jit(uint32_t h, int32_t amp_mm) {
  if (amp_mm <= 0) return 0;
  return static_cast<int32_t>(h % static_cast<uint32_t>(2 * amp_mm + 1)) - amp_mm;
}

// The six fold anchors' REST positions, root-local mm (junctionF, neck,
// hingeA, hingeB, hingeC, junctionB) -- measured by the committed probe's
// REST ANCHOR report on the still pose and held here as the OWNER-EDITABLE
// reference layout the stencil weights are computed against. Re-run the
// probe and update after any rig/arc change.
constexpr int32_t kFoldAnchorRestMm[6][3] = {
    {89, 664, 0},     {45, 996, 14},   {0, 1337, 29},
    {-231, 1564, 132}, {-566, 1434, 256}, {-230, 179, 0}};
// the stencil frame: shapes are authored around this pocket centre, at
// this scale (mm), inside the anchor hexagon (kPocketBoundPm clamps)
constexpr int32_t kStencilCentreUMm = -240, kStencilCentreVMm = 1120;
constexpr int32_t kStencilScaleMm = 300;   // iter 1: 230 drew a blob smaller
                                           // than its own motes; bigger shape,
constexpr int kPocketBoundPm = 900;        // stencil points clamp inside the hexagon
// The plan's named fallback, NOW ON (authored, judged by eye): the house
// camera sits ~45° off the loop plane's ~18° yaw, so a stencil drawn in
// the plane loses ~45% of its across-axis on screen and the STAR/TRIANGLE
// died at native (looked at, iter 6). The stencil OFFSETS (never the
// anchors) rotate about the vertical axis by this yaw toward the camera
// side; positions stay rig-anchored sums, so the folding law is untouched.
constexpr int32_t kStencilFaceYawA16 = -5000;  // ~27 deg toward the camera (the
                                               // offset rotation is -theta about
                                               // +Y; +5000 rotated AWAY, looked at)
// motes
constexpr int kMoteCount = 24;
constexpr int kWanderCount = 3;            // of kMoteCount: the odd drifters
constexpr int32_t kMoteHaloRPxMin = 7, kMoteHaloRPxMax = 10;   // iter 5; iter 2: 8-11 still
                                           // flooded the ~40 px pocket; iter 1: 11-15
                                           // merged into one cloud that
                                           // swallowed the antenna (looked at);
                                           // the STROKE must be thinner than
                                           // the shape it draws
constexpr int kMoteCoreOfHaloPm = 580;     // opaque heart under the halo
constexpr int kMoteHaloGainPm = 340;       // under the ceiling: hue survives
constexpr int kMoteCrowdPm = 700;          // per-conduit mote scale-down when
                                           // several conduits are on screen
// grip / knead / drag (all derived from JOINT STATE, never contact)
constexpr int32_t kGripGamma = 14;         // coherence per area-shrink pm
constexpr int kCohBasePm = 320;            // coherence at rest area (low: the
                                           // limp cloud must read limp)
constexpr int kCohMinPm = 130;
constexpr int32_t kCloudSpreadMm = 380;    // low-grip relax offsets
constexpr int32_t kKneadJitterMm = 70;     // agitation jitter at full knead
constexpr int32_t kKneadVelRefMm = 50;     // EXCESS anchor speed (mm/frame over
                                           // the slow-tracked baseline) = full
                                           // agitation. Iter 3: raw speed
                                           // saturated on the resting wobble
                                           // itself (measured 57-82 mm/frame at
                                           // rest), so agitation is now the
                                           // excess over a ~64-frame EMA.
constexpr int kFoldFeedBasePm = 520;       // the fold's smear feed at rest
                                           // (pass 5: a 760 ladder rung was
                                           // rendered and REJECTED by looking
                                           // -- more feed whitens the trail,
                                           // because the ramp itself whitens
                                           // with intensity; 520 stands)
constexpr int kKneadFeedPm = 380;          // extra smear feed at full agitation
constexpr int kDragLagFrames = 2;          // + hash%4 per mote (2..5) -- the
                                           // iron-filings lag
constexpr int kDragGainPm = 2600;
// PASS 5 (QA item 2: "the mana covers the antenna", taunt2 worst): the
// drag displacement is CLAMPED by magnitude. On a stationary clip with a
// violent gesture (the lasso) hinge B's 3-frame summed velocity reached
// hundreds of mm, and 2.6x that flung the motes clear across the loop --
// the fog that buried the hand. Travel clips are untouched by
// construction: their anchor velocities are body-relative, so hasty's
// gentle stream stays under the clamp and keeps its lagging-gap read
// (protected by both gates). Sized by looking at taunt2/rest vs hasty.
constexpr int32_t kDragMaxMm = 380;
constexpr int32_t kWanderEscapeMm = 780;   // the wander motes may leave the pocket
// mote micro-orbit (R7 smoother rotation: ONE angular velocity per mote,
// long periods, no frequency doubling)
constexpr int kMoteOrbitPeriodMinF = 130, kMoteOrbitPeriodMaxF = 260;
constexpr int32_t kMoteOrbitRMinMm = 40, kMoteOrbitRMaxMm = 95;
// the fold-hold-knead timeline (KEYS; frames on screen = 2x)
constexpr int kGatherKeysBase = 30, kGatherKeysHash = 16;   // 60..90 frames
constexpr int kHoldKeysBase = 32, kHoldKeysHash = 32;       // 64..128 frames
constexpr int kKneadKeysBase = 30, kKneadKeysHash = 30;     // 60..120 frames
constexpr int kReleaseKeys = 14;           // every clip's tail: amp eases to 0
                                           // so the loop seam carries no pop
// the choreography amplitudes (angle16; "very mobile" -- authored large,
// bounded by the 07 bands and the closure probe)
constexpr int32_t kKneadGripJfA16 = 1500;  // gather: the junctions close...
                                           // (iter 3: the first authoring
                                           // moved the pocket area by <1% --
                                           // the grip must be SEEN)
constexpr int32_t kKneadGripNeckA16 = 2600;
constexpr int32_t kKneadGripAA16 = 2200;
constexpr int32_t kKneadGripBA16 = 1400;
constexpr int32_t kKneadGripCA16 = 1800;
constexpr int32_t kKneadWagJfA16 = 900;   // knead: the two hands work...
constexpr int32_t kKneadWagNeckA16 = 400;  // (neck stirs out-of-plane)
constexpr int32_t kKneadWagBA16 = 500;
constexpr int32_t kKneadWagCA16 = 1000;    // ...in counter-rotation
constexpr int32_t kKneadWagB2A16 = 900;    // the back-junction ball slides
constexpr int kKneadWagPeriodKeys = 22;
constexpr int32_t kKneadTremorA16 = 130;   // the hold's small tremor
// per-clip gain (pm) for the always-on knead layer, indexed by slot:
// 0 hover, 1 drift, 2 channel, 3 curious, 4 startle, 5 rest, 6 pirouette,
// 7 still(diagnostic: OFF), 8 hasty, 9 fall, 10 hit, 11 taunt, 12 taunt2,
// 13 trick, 14 damage
// (trick is 0: the committed probe showed the knead grip LIFTING the
// planted loop peak out of its declared ground contact -- the antenna is
// busy standing; the mana still reads the balance flex.)
// PASS 5 (QA item 2, judged by LOOKING at the worst frames): rest 700->500
// and taunt2 500->380 -- on those two stationary clips the added knead
// choreography stacked its agitation on the clip's own big gesture and the
// smear fog buried the loop (taunt2 f166 was the exhibit). damage's 250 is
// simply REACHED now (the dead-knob fix): it shipped at 700 by accident.
constexpr int kKneadClipPm[15] = {1000, 600, 900, 800, 350, 500, 700,
                                  0,    450, 300, 350, 550, 380, 0, 250};

// ============================ END KNOBS ====================================

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_ART_H
