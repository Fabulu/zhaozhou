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
constexpr int kBodyTaperPm[kBodyRings] = {1000, 1000, 1000, 1000, 1000, 1000,
                                          985,  950,  890,  790,  660};
constexpr int kBodyLeanXMm[kBodyRings] = {0, 0, 0, 0, 0, 0, 0, 15, 40, 70, 90};

// ---- the three hinge balls (the drawn nodes the loop articulates around) --
constexpr int32_t kHingeRadiusMm = 85;
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
constexpr int32_t kLoopBladeRxMm = 105;  // broad in the loop plane
constexpr int32_t kLoopBladeRzMm = 32;   // narrow across it (front = blade)
constexpr int kLoopRings = 26;
constexpr int kLoopSegments = 8;
constexpr int32_t kLoopTubeXMm = 90;     // tube bind x (the neck exit)
constexpr int32_t kLoopNeckExitYMm = 664;   // STRETCHED units from here down
constexpr int32_t kLoopBuryMm = 250;        // both ends plunge into the body
// arc lengths along the tube (stretched space, from the sheet anchors by eye)
constexpr int32_t kLoopArcMm[4] = {1441, 540, 949, 1227};
// fold angles at the neck exit and the three hinges (angle16, about Z);
// derived from the sheet's anchor directions, then tuned by LOOKING
constexpr int32_t kLoopFoldRootA16 = 1038;    // ~6 deg back lean at the neck
constexpr int32_t kLoopFoldAA16 = 9903;       // ~54 deg at the front hinge
constexpr int32_t kLoopFoldBA16 = 17500;      // ~96 deg over the peak (opens the loop)
constexpr int32_t kLoopFoldCA16 = 9800;       // ~54 deg at the rear hinge (return arm dives into the body)

// ---- the eyes (the whole face) ----
// Two big purple almond lenses close together on the lower front, angled
// outward in a V; a four-pointed cyan star rides a pupil bone inside each.
// The lens is REAL FACETED GEOMETRY (the direction's partly-polygonal read);
// the white rim is paint at the texture pass.
constexpr int32_t kEyeXMm = 425, kEyeYMm = -70, kEyeZMm = 150;  // centre, ±z
constexpr int32_t kEyeVAngleA16 = 2600;   // outward V (roll about +X)
constexpr int32_t kEyeYawOutA16 = 3600;   // lenses face along the body surface
constexpr int32_t kEyeTiltA16 = 2200;     // the almond's backward lean
constexpr int32_t kEyeBulgeMm = 88;       // pupil star stands proud of the lens
constexpr int32_t kEyeLongMm = 250;       // almond half-length (long axis)
constexpr int32_t kEyeWideMm = 92;        // almond half-width
constexpr int32_t kEyeDeepMm = 58;        // bulge depth off the body
constexpr int kEyeRings = 5;
constexpr int kEyeFacetSegments = 8;      // the facet read at 240p
constexpr int32_t kPupilStarArmMm = 78;   // star arm half-length (inside the rim)
constexpr int32_t kPupilStarThinMm = 16;  // blade thinness
constexpr int32_t kPupilStarWideMm = 30;  // blade width
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
constexpr int32_t kBobAmpAMm = 26, kBobAmpBMm = 11;
constexpr int kBobPeriodAKeys = 25, kBobPeriodBKeys = 60;
// the constant compression (Q0.16 flatten peak; slight but UNMISTAKABLE)
constexpr int32_t kCompressAmpPm = 3300;   // ~5% vertical at the equator
constexpr int32_t kSpreadRatioPm = 550;    // the positive-volume partner
constexpr int kCompressPeriodKeys = 30;
constexpr int32_t kCompressLoopCouplePm = 14;  // sympathetic hinge-root bob
// the antenna's life
constexpr int32_t kAntennaSwayPm = 45;
constexpr int kAntennaLagKeys = 4;
constexpr int32_t kAntennaTiltA16 = 700;
// the gaze (the pupil pivots sweep the stars across the lenses)
constexpr int32_t kGazeMaxA16 = 5000;
constexpr int32_t kGazeLiftMaxA16 = 3200;
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

// ============================== STAGE ======================================

constexpr int32_t kStageCentreMm = 0;     // the fixed camera aims at world x=0
constexpr int32_t kHoverHeightMm = 900;   // body CENTRE above terrain
constexpr int32_t kRestHeightMm = 845;    // the rest clip lower hover (the body
                                          // half-height is ~747 stretched: 640
                                          // would have SUNK the belly 107 mm)

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
constexpr uint8_t kGlowLo[3] = {40, 8, 64};      // deep violet skirt
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
