// Zixxtrixx — the first Upheaval creature. PRODUCTION model.
//
// Built from S. Hofer's two concept sheets. Nothing is copied from the donor;
// the laws live in spec/creature_rules.md and zref_creature.hpp. This file is
// pure AUTHORING.
//
// ---------------------------------------------------------------------------
// HOW TO TURN THE KNOBS
//   1. edit a constant in the KNOBS block below
//   2. rebuild zhao-reel
//   3. build\tools\zhao-reel.exe <out-dir> zixxtrixx-idle
// EVERYTHING shape- and colour-related is a knob, the taper included: author
// visually, render, look, compare, adjust (Fabian, 2026-08-26).
// ---------------------------------------------------------------------------
//
// WHAT CHANGED FROM THE FIRST MODEL
//
// * THE BODY IS ONE CONTINUOUS CHAIN PART. It used to be ten independent rigid
//   cylinders, which is why the attack opened a measured 61 mm hole (5.3 px on
//   a 19 px body). Rings now carry their own {b0,b1,w0} and blend across every
//   joint, so the skin cannot come apart however hard it is bent.
// * IT IS NOT A 100-TRIANGLE SNAKE. 28 sides, 57 stations, ~4,000 verts --
//   inside the donor roster's 1,600-10,500 band, and round enough to read as
//   a body instead of a prism.
// * THE TAPER IS AUTHORED BY EYE (kTaper): neck pinch, full mid-body, one
//   long taper to the fork. The drawing-derived profile is a comparison tool
//   only -- it measured a projection, not the animal (Fabian, 2026-08-26).
// * SECTIONS ARE ELLIPTICAL, wider than tall, and the tail blades are FLAT.
// * COLOURS: hue from the sheets, saturation/value art-directed by looking at
//   renders. The dorsal pink is NEON by the owner's word. See PALETTE.md.
// * THE S IS A STANCE, NOT THE BIND POSE. A ring stack is all-parallel rings,
//   so baking a curve that doubles back (the drawing's does: x runs
//   819 -> 486 -> 730 -> 283) would shear every section. Bind straight, pose
//   the S.
//
// AXIS MAP, because it is not guessable. Rings stack along local +Y and the
// body uses pitch_q=1, yaw_q=3, mapping local (x,y,z) -> world (-y,-z,x):
//     local +Y -> world -X   the body runs backward from the nose
//     local +X -> world +Z   LATERAL, so RingSpec::cx and rx are lateral
//     local +Z -> world -Y   VERTICAL, so cz and rz are vertical and UP IS
//                            NEGATIVE cz
// Forward is +X, which is what the reel's facing expects.

#ifndef ZHAO_REEL_ZIXXTRIXX_H
#define ZHAO_REEL_ZIXXTRIXX_H

namespace zixx {

// the crayon page, generated from the sheets (pigment IS measurable)
#include "zixxtrixx_page.h"

// NOTE 2026-08-26: zixxtrixx_profile.h (the taper derived from the drawing's
// distance transform) is DEMOTED to a comparison tool and no longer included.
// Fabian: "the worst thing you did was constructing the snake from measuring.
// V1 looked better because it was visually authored." The medial-axis
// half-thickness of a 2D drawing conflates real thickness with foreshortening
// and with the bends -- it measures a projection, not the animal. The taper
// below is authored BY EYE and lives in KNOBS where it can be turned.

// ============================== KNOBS ======================================
// Millimetres unless noted. Angles in angle16: 65536 = one turn, 182 ~ 1 deg.

// 28 sides: Fabian 2026-08-26, "give more polygons for this thing to actually
// look rount". 28 puts a ring at 29 verts (the textured seam duplicate), two
// rings per meshlet under the 64-vert cap; 32 sides would not fit two rings.
constexpr int kSides = 28;       // sides around the body at LOD0
constexpr int kSpineBones = 20;  // chain bones nose -> fork

// ---- the body, authored by eye --------------------------------------------
constexpr int kProfileStations = 57;   // ring stations nose -> fork
constexpr int32_t kBodyLenMm = 3050;   // nose to fork
// SLIMMED 2026-08-26. The sheet's animal is ~12 body-widths long; at 270 the
// model was 5.6 -- a slug, and it is most of why "the front is so much fatter
// than the back" read as weird: a fat head on a body that thinned away.
constexpr int32_t kHeadHalfMm = 160;   // half-thickness at the skull

// THE TAPER, HAND-SET (Fabian, 2026-08-26: author visually, render, look,
// compare, adjust -- the drawing-derived profile measured a projection, not
// the animal). {position along the body in 1/1000, half-thickness in 1/1000
// of kHeadHalfMm}. The sheet's body is nearly UNIFORM: the head is barely
// fatter than the trunk, the trunk holds its girth for two thirds of the
// length, and only the tail stem thins -- to about a quarter, not to a hair.
struct TaperKey {
  int t, r;
};
// SLIMMED AT THE FRONT 2026-08-27 (Fabian: "The upper part is too fat") --
// the raised front lobe (skull through the arch) drops ~13%, the grounded
// rear keeps its girth, so the snakelike part reads full while the reared
// part reads lean.
constexpr TaperKey kTaper[] = {
    {0, 870},   {60, 870},  {150, 840}, {230, 800},              // skull, soft jaw
    {320, 800}, {500, 840}, {620, 860},                          // trunk swells rearward
    {720, 790}, {820, 620}, {900, 450}, {950, 330}, {1000, 260}  // tail stem
};
constexpr int kTaperKeys = static_cast<int>(sizeof(kTaper) / sizeof(TaperKey));
// Bind height of the body axis. This is the HEAD height: bone 0 is the nose
// and the reel ground-snaps the ROOT, so the head is CARRIED at this height
// and the whole S hangs from it. TUNED against the pose probe
// (tools/reel/zixx_probe.cpp): the grounded run's belly rides a few mm under.
// The skinned mesh sits ~15 mm below the centreline prototype (ring blending
// sags into the bends), so this is chosen off the PROBE, not the sketch.
constexpr int32_t kBodyY = 542;  // retuned 2026-08-27 for the new stance
                                 // (probe: idle belly [-13..-4] mm)
// Planform centre of the posed S, nose to tail extent midpoint, for staging:
// the folded S spans ~1.8 m behind the nose, so the reel offsets the instance
// by this to keep the animal centred in an orbit shot.
constexpr int32_t kStageCentreMm = 920;

// THE NOSE IS A DOME, NOT A DISC. The measured profile starts at full
// half-thickness, so station 0 used to be a full-radius ring closed by a flat
// 20-gon cap -- the "weird spinning disc" at the front of the face. These
// factors (1/1000 of the profile value) round the first stations into a
// blunt dome; the cap that remains is a dot.
constexpr int kNoseDomeStations = 4;
constexpr int16_t kNoseDome[kNoseDomeStations] = {320, 720, 910, 980};

// section ellipticity, measured: wider than tall
constexpr int32_t kHeadWideNum = 112;  // head 1.12 : 1
constexpr int32_t kBodyWideNum = 119;  // body 1.19 : 1
constexpr int kHeadStations = 9;       // how many stations read as head

// THE EYE IS NOT GEOMETRY. A yellow ball on the side of the head was the
// obvious thing and it looked exactly like what it was: a sphere glued to a
// tube. MODELINGGUIDE asks for eyes "integrated into the head contour" so they
// influence the SILHOUETTE rather than sitting on it.
//
// So the eye is two things instead. The drawing's own eye -- disc, ink ring
// and red-orange slit pupil -- is painted into the head page by
// tools/pack/mkcreaturepage.py; and the head's own rings swell LATERALLY where
// it sits, so the skull is widest exactly at the eyes and the outline says so.
// That also deleted two bones and four ring parts.
constexpr int kEyeStation0 = 3;      // first head station that carries the bulge
constexpr int kEyeStation1 = 8;      // last
constexpr int32_t kEyeBulgeNum = 72; // extra lateral half-width, % of the ring.
                                     // 72, was 40: Fabian 2026-08-27, "the
                                     // eyes do need to be a bit googly so
                                     // that even from front, you can see the
                                     // orange left and right" -- the flush
                                     // swell kept the eye off the front
                                     // silhouette entirely.

// the dorsal crest: geometry, because there is no texture page pipeline yet
constexpr int32_t kCrestNum = 46;   // crest half-width = body half-width * n/100
constexpr int32_t kCrestLift = 104;  // crest centre, as a % of body half-height

// the tail: two SMALL flat pointy blades left and right, plus a tiny middle
// spike. Fabian, 2026-08-26: "The fins are gargantuan while on the reference
// sketch they are small." The first sizing (1180 mm on a 3050 mm body) put a
// blade at 39% of the animal; the sketch's slivers are a sixth of that mass.
constexpr int kBladeSides = 8;
constexpr int kBladeRings = 7;
// 780: the sheet's fins are LONG slivers -- ~26% of the body's path length --
// not paddles (the first "gargantuan" verdict was about their WIDTH). Length
// up, half-width kept slim.
constexpr int32_t kBladeLen = 780;
constexpr int32_t kBladeW0 = 70;       // half-width at the root (LATERAL)
constexpr int32_t kBladeThick0 = 16;   // half-thickness at the root (VERTICAL)
constexpr int32_t kBladeSplay = 6900;  // ~38 deg apart, about the vertical axis
constexpr int32_t kBladeRise = 2600;   // lifted past the raised stem, so the
                                       // fan reads against the sky as drawn
// 280: Fabian 2026-08-27, "make the middle prong of the tail a bit longer"
constexpr int32_t kSpikeLen = 280;
constexpr int32_t kSpikeR = 26;

// -- colours: hue from the sheets, saturation and value ART-DIRECTED --------
// The measured pigment (PALETTE.md) is a HUE REFERENCE, not the answer: the
// scanner flattens, and a value that reads as pink on white paper reads as
// grey-white on dark ochre ground at 240p. Shipped colours are chosen by
// LOOKING at renders. These are the flat-colour fallbacks; the texture page's
// copies live at the top of tools/pack/mkcreaturepage.py -- keep them in step.
// TWO greens (Fabian, 2026-08-27): dark for the front, light for the rest.
// The page carries the split; these are the flat-colour fallbacks.
constexpr uint8_t kGreen[3] = {122, 192, 70};      // LIGHT green: most of the body
constexpr uint8_t kGreenDark[3] = {44, 146, 86};   // DARK green: the front
// ART DIRECTION OVERRIDE (Fabian, 2026-08-26): "The pink on the back should
// be like neon pink." The measured pale sheet pink is overruled by the owner.
constexpr uint8_t kPink[3] = {255, 32, 168};     // dorsal band, NEON
constexpr uint8_t kBlue[3] = {3, 145, 205};      // head and throat
constexpr uint8_t kYellow[3] = {243, 232, 142};  // eye
// ONE pencil serving TWO features: the ring round the eye in Front.png and the
// wavy slit pupil in Side.png.
constexpr uint8_t kOrange[3] = {218, 106, 71};

// -- animation --------------------------------------------------------------
// A key is held 2 sim ticks, so reel frames = keys * 2 at step 1.
constexpr int kIdleKeys = 96;  // SLOW. 3.2 s of breathing.
constexpr int kWalkKeys = 40;
// 220 keys = 440 frames = 7.33 s at 60 Hz. The 2026-08-27 salto sticks its
// landing as a planted spear for FIVE REAL SECONDS (Fabian: "Make it stick
// for 5 actual seconds"): keys 53..203 are the stick -- 150 keys, 300
// frames, 5.000 s at the site's 60 fps -- and the remaining 17 keys pull it
// out and close the loop.
constexpr int kAttackKeys = 220;
constexpr int kFallKeys = 96;  // SLOW. One 3.2 s tumble per loop (Fabian,
                               // 2026-08-26: "slowly flailing", not jitter).

// THE CANONICAL S, as a slope table. d[k] is the direction of body segment k
// (walking BACKWARD from the nose) in angle16, POSITIVE = DESCENDING (the
// sign convention positive joint pitch established).
//
// REBUILT 2026-08-26. Fabian: "There is no S, just a single half S bend.
// Terrible." The old table arched ONCE behind the head and then lay flat --
// half an S. The sheet's S, traced curve by curve (Side.png):
//   - the head is carried HIGH at the front, nose level, and the neck rises
//     only GENTLY to an apex a little above the skull;
//   - from the apex the body DIVES: past vertical, DOUBLING BACK under
//     itself (the middle stroke of the letter runs down-forward) -- slope
//     137 deg > 90 deg is that doubling, and it is what makes the shape
//     read as an S instead of an arch;
//   - the bottom curve lands the belly on the ground for a short grounded
//     run (slopes just past 0 keep the thinning belly ON the ground);
//   - the tail rises steeply behind, and the blade fan carries on up.
// Joint pitch is the DIFFERENCE of adjacent slopes, so the whole chain makes
// the shape and no single joint carries a corner.
// REDISTRIBUTED 2026-08-27 (Fabian): "make the part that touches the ground
// longer. Make some real snakelike part there", "head and neck ... too long
// and should be more part of the S", "exaggerate the creature's S more.
// We're departing from the concept art here a bit, so use my words as
// guidance." So against the 2026-08-26 table:
//   - the GROUNDED RUN doubles, 3 segments -> 6 (~0.96 m of snake lying
//     along the ground instead of ~0.48);
//   - the NECK shortens, 6 segments -> 4, with a STEEPER hook, so the head
//     hangs inside the curve instead of sticking out ahead of it;
//   - the DIVE compresses to 6 segments and doubles back HARDER: the
//     steepest slope is now 150 deg past horizontal (was 137) -- the middle
//     stroke of the letter cuts back under itself further;
//   - the TAIL rise drops to 2 steep segments.
constexpr int kStanceSlopes = kSpineBones - 1;  // 19 segments
constexpr int32_t kStanceSlope[kStanceSlopes] = {
    // neck: the cobra hook, tighter than ever -- but the FIRST segment is
    // gentler (the nose was pitched 29 deg at the dirt, which hid the face,
    // the eye and the mouth from every camera; the drawn head hangs roughly
    // level under the arch). Same sine sum, so the apex height is unchanged.
    -3400, -7600, -9800, -6200,
    // apex
    0,
    // the dive, past vertical and back under itself, exaggerated
    8000, 16500, 23500, 27300, 21000, 12000,
    // the grounded run, LONG. These slopes RAMP because the belly line must
    // follow the TAPER: the centreline of a grounded run sits one radius up,
    // and the tail-stem radius falls 136 -> 68 mm across these six nodes --
    // a flat centreline here lifts the thinning belly off the ground and a
    // flat 900-ish table dug it 30 mm under (probe, 2026-08-27). Each slope
    // is asin(radius drop / segment): the belly rides the ground exactly.
    40, 380, 710, 960, 1100, 1220,
    // the tail rises behind, short and steep
    -5600, -11400};
// which slope entries the descent lobe occupies (breathing deepens these)
constexpr int kStanceDescend0 = 5;
constexpr int kStanceDescend1 = 10;
// and which are the grounded run (the walk's hump travels here)
constexpr int kStanceGround0 = 11;
constexpr int kStanceGround1 = 16;

// The idle is RELAXED. The breath DEEPENS the descent lobe while the root
// (the nose) rises to match, so the head and the arch visibly bob while the
// grounded belly stays put -- the only way to bob a creature whose root is
// ground-snapped by the nose without floating or burying it.
constexpr int32_t kIdleDeepen = 200;      // 1/1000 extra descent authority.
                                          // 200, was 130: Fabian 2026-08-27,
                                          // "the entire upper body ... needs
                                          // to move up and down" -- real
                                          // bobbing, not a token offset.
// kIdleBobComp is GONE: the root rise that keeps the belly planted under the
// breath (and under the front wave below) is now COMPUTED EXACTLY inside
// apply_stance from the slope deltas' sine sums, so it cannot drift out of
// tune when the stance table changes. The probe still verifies it.
constexpr int32_t kIdleGirth = 42;        // girth swing, 1/1000 of scale
constexpr int32_t kIdleTailSway = 2200;   // lazy left-right tail sway
// THE FRONT WAVE (Fabian, 2026-08-27: "front part needs to be more animated
// ... the entire upper body needs to be less rigid", "Everything about this
// creature is bendy"). A slow travelling undulation through the raised front
// lobe -- the walk's caterpillar principle applied to the part of the animal
// that is off the ground. Slope-delta per joint; apply_stance compensates
// the root so the grounded belly never feels it.
constexpr int32_t kIdleWaveAmp = 2100;     // peak slope delta (~11 deg).
                                           // 2100, was 1500: at 1500 the
                                           // arch's travel measured 2 px on
                                           // screen -- present, invisible.
constexpr int32_t kIdleWaveSpatial = 5200; // phase step per joint: one slow
                                           // wave over the whole front lobe
constexpr int32_t kIdleHeadSway = 800;     // slight head side-to-side yaw
                                           // ("only slight")

// CATERPILLAR walk: the S holds, the head glides high, and ONLY the grounded
// run carries a travelling hump. The hump is authored as a HEIGHT field and
// converted to joint pitches by second difference, so it is a bump above the
// ground line by construction and can never reach below it.
// 2026-08-26: hump 95 -> 230 ("caterpillar wave is too small") and sway
// 500 -> 150 ("too much side to side"). The wave is the walk; the sway is
// seasoning.
constexpr int32_t kWalkHumpMm = 210;      // hump height
constexpr int32_t kWalkHumpHalf = 1600;   // hump half-width, milli-bones: narrow
                                          // enough that an edge node of the short
                                          // grounded run keeps contact mid-traverse
constexpr int32_t kWalkSway = 150;        // secondary lateral life only
constexpr int32_t kWalkSpeed = 13;      // mm per reel frame. 13, was 11: the
                                        // grounded window grew from 5 to 7
                                        // nodes, so the hump travels 1123 mm
                                        // per cycle; 13*80 = 1040 mm of
                                        // advance keeps the contact points
                                        // from skating.
// The walk's breath: the head is BONE 0, the root, so no joint wave can
// move it -- the visible head-bob IS the root compensation, and the wave
// alone compensates to almost nothing (its sine deltas half-cancel across
// the lobe: measured 8 px of head travel). The deepen drives a real comp
// exactly the way the idle's breath does, at gait rate.
constexpr int32_t kWalkDeepen = 400;    // 1/1000 extra descent authority.
                                        // 400: at 150 the bob was ~50 mm =
                                        // 4 px at shot distance -- present,
                                        // invisible; at 400 the arch and
                                        // head surge ~100 mm with the gait.
// the walk's own front wave: same mechanism as the idle's, faster, so the
// head and raised body surge with the gait instead of gliding rigidly
constexpr int32_t kWalkWaveAmp = 2400;  // 2400, was 1000: clip_report showed
                                        // 6 px of head travel -- the wave
                                        // existed and did not read
constexpr int32_t kWalkWaveSpatial = 5600;

// ATTACK geometry: the body coils into a near-circle of this radius
// (kBodyLenMm / 2 pi), spins about the coil's centre, and the tail-first
// spear dive needs the root this high for the tail tip to just reach ground.
constexpr int32_t kCoilR = 485;
// THE 2026-08-27 SALTO REWORK (Fabian, verbatim guidance):
//   "make it jump up higher"            -> apex lift 4100 -> 5600 mm
//   "It should jump forward"            -> 1900 mm of forward travel
//   "hit while it's diagonal 30 degrees"-> spin 3333: the tail points 60 deg
//      below horizontal, down-and-FORWARD, a javelin -- not straight down
//   "ignore the clipping rule and actually stick in the ground some"
//                                       -> tip buried 420 mm, AUTHORED
//   "Make it stick for 5 actual seconds. Completely straight."
//                                       -> lift/spin/curl all HELD for 150
//                                          keys = 300 frames = 5.000 s
// Tail direction at spin s (1/1000 turn, minus the 3 whole somersaults):
// 180 deg + s*0.36 deg. At 3333 that is 300 deg == 60 deg below horizontal
// toward +X. Tip drop from the nose = reach * sin(60) = 3830 * 0.86603 =
// 3317 mm (reach = kBodyLenMm + kBladeLen with the blades on the spear line).
constexpr int32_t kAtkApexLift = 5600;   // mm of root lift at the apex
constexpr int32_t kAtkFwdMax = 1900;     // mm of forward travel at impact
constexpr int32_t kAtkSpinStick = 3333;  // 1/1000 turns: 30 deg from vertical
constexpr int32_t kAtkStickDepth = 420;  // mm of authored burial
constexpr int32_t kAtkTipDrop = 3317;    // reach * sin(60 deg), see above
constexpr int32_t kAtkStickLift = kAtkTipDrop - kBodyY - kAtkStickDepth;
constexpr int kAtkImpactKey = 53;        // reel frame 106 (keys held 2 ticks)
constexpr int kAtkStickEnd = 203;        // impact + 150 keys = 5.0 s stuck

// FALL: the slow distress tumble. The whole S rotates about its own centre
// (re-pivoted off the nose exactly the way the salto re-pivots its spin to
// the coil centre -- "I think we did that well on salto"), one full pitch
// turn per loop, so it stands on its head at the half and comes back up;
// slow roll and yaw wobbles ride along so every axis moves. The pivot is the
// posed S's planform centre, measured by the pose probe.
constexpr int32_t kFallPivotX = -990;   // mm behind the nose
constexpr int32_t kFallPivotY = -60;    // mm below the nose
constexpr int32_t kFallLift = 1300;     // mm of air under the nose; the probe
                                        // verifies the loop never touches
                                        // (1150 left the longer blades 24 mm)
constexpr int32_t kFallRollAmp = 6400;  // ~35 deg of slow roll wobble
constexpr int32_t kFallYawAmp = 5100;   // ~28 deg of slow yaw wobble
constexpr int32_t kFallNeckAmp = 4200;  // slow loose head/neck flail (up
                                        // 2026-08-27: "with head movement")
constexpr int32_t kFallWritheAmp = 1300;  // mid-body roll-twist writhe
// THE LATERAL WAVE (Fabian, 2026-08-27: "It needs to be wobbly side to
// side ... It needs to be wavey. The walk does waveyness pretty well for the
// caterpillar part"). A slow serpentine wave travelling nose -> tail through
// every spine joint, strongest at the head, free of the ground because the
// fall never touches it. This is the walk's principle turned sideways.
constexpr int32_t kFallWaveAmp = 2600;     // ~14 deg at the head
constexpr int32_t kFallWaveSpatial = 4700; // ~1.3 wavelengths down the body

// ============================ END KNOBS ====================================

// ---- rotation helpers -----------------------------------------------------
// The quaternion takes the HALF angle, which is why every amplitude above is
// about twice the visible swing.
inline zc::quat16 quat_axis(int32_t ax, int32_t ay, int32_t az, int32_t a) {
  const zref::angle16 h{static_cast<uint16_t>((a >> 1) & 0xFFFF)};
  return zc::quat16_axis_angle(zref::fx16{ax}, zref::fx16{ay}, zref::fx16{az}, zref::fx_sin(h),
                               zref::fx_cos(h));
}
inline zc::quat16 quat_x(int32_t a) { return quat_axis(1 << 16, 0, 0, a); }
inline zc::quat16 quat_y(int32_t a) { return quat_axis(0, 1 << 16, 0, a); }
inline zc::quat16 quat_z(int32_t a) { return quat_axis(0, 0, 1 << 16, a); }

/**
 * Hamilton product of two quat16, S 1.0.14 lanes, ONE rescale(.,14) per lane.
 *
 * There was no quaternion multiply anywhere in zref, which is why the first
 * attack SWITCHED a joint between quat_y and quat_z on a magnitude threshold
 * and popped as it crossed. With this, pitch, yaw and roll compose
 * continuously and nothing has to choose an axis.
 */
inline zc::quat16 quat_mul(const zc::quat16& a, const zc::quat16& b) {
  const int64_t aw = a.q[0], ax = a.q[1], ay = a.q[2], az = a.q[3];
  const int64_t bw = b.q[0], bx = b.q[1], by = b.q[2], bz = b.q[3];
  const auto r = [](int64_t v) {
    int64_t q = (v + (1 << 13)) >> 14;
    if (q > zc::kQuatOne) q = zc::kQuatOne;
    if (q < -zc::kQuatOne) q = -zc::kQuatOne;
    return static_cast<int16_t>(q);
  };
  return zc::quat16{{r(aw * bw - ax * bx - ay * by - az * bz),
                     r(aw * bx + ax * bw + ay * bz - az * by),
                     r(aw * by - ax * bz + ay * bw + az * bx),
                     r(aw * bz + ax * by - ay * bx + az * bw)}};
}

/**
 * Rotate an integer mm vector by a quat16 (S 1.0.14 lanes), integer-only:
 * t = 2 (qv x v); v' = v + w t + qv x t. The fall clip needs this to re-pivot
 * its whole-body tumble from the nose (bone 0) to the S's planform centre,
 * the same job the salto's disp = c - R(theta) c does with sin/cos -- but the
 * tumble turns about three axes at once, so it needs the full rotation.
 */
inline void quat_rot_vec(const zc::quat16& q, int32_t vx, int32_t vy, int32_t vz,
                         int32_t& ox, int32_t& oy, int32_t& oz) {
  const int64_t w = q.q[0], x = q.q[1], y = q.q[2], z = q.q[3];  // 2^14 = 1.0
  const int64_t tx = 2 * (y * vz - z * vy);  // scale 2^14
  const int64_t ty = 2 * (z * vx - x * vz);
  const int64_t tz = 2 * (x * vy - y * vx);
  ox = vx + static_cast<int32_t>((w * tx + y * tz - z * ty) >> 28);
  oy = vy + static_cast<int32_t>((w * ty + z * tx - x * tz) >> 28);
  oz = vz + static_cast<int32_t>((w * tz + x * ty - y * tx) >> 28);
}

/**
 * asin in angle16 by integer bisection on fx_sin: the angle whose sine lifts
 * a segment of length L (mm) by dh (mm). Monotone on [-90, 90] deg, 18
 * halvings, deterministic. The walk's hump needs the EXACT angle: at 230 mm
 * of hump the small-angle dd*65 conversion accumulated 93 mm of belly dig
 * (pose probe, 2026-08-26) because sin(a) < a by 7% at 40 deg.
 */
inline int32_t asin16(int32_t dh, int32_t L) {
  const bool neg = dh < 0;
  if (neg) dh = -dh;
  if (dh >= L) return neg ? -16384 : 16384;
  const int64_t target = (static_cast<int64_t>(dh) << 16) / L;
  int32_t lo = 0, hi = 16384;
  for (int i = 0; i < 18; ++i) {
    const int32_t mid = (lo + hi) / 2;
    if (zref::fx_sin(zref::angle16{static_cast<uint16_t>(mid)}).raw < target) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return neg ? -hi : hi;
}

// piecewise-linear keyed curve in thousandths, integer, clamped at both ends
struct Key {
  int f;
  int v;
};
inline int curve(const Key* k, int n, int f) {
  if (f <= k[0].f) return k[0].v;
  for (int i = 0; i + 1 < n; ++i) {
    if (f >= k[i].f && f <= k[i + 1].f) {
      const int span = k[i + 1].f - k[i].f;
      if (span <= 0) return k[i + 1].v;
      return k[i].v + ((k[i + 1].v - k[i].v) * (f - k[i].f) + span / 2) / span;
    }
  }
  return k[n - 1].v;
}

// ---- the attack's flight path, at file scope ------------------------------
// The salto's lift and forward-drive curves live HERE, not inside
// build_attack, because two consumers need the same numbers: the clip
// builder, and the REEL'S TRACKING CAMERA (Fabian, 2026-08-27: "It is very
// important the camera follow it, you did not do that"). The camera follows
// the authored path -- not the decoded pose, whose root also carries the
// coil re-pivot wobble that would shake the shot.
//
// Lift in mm. HIGH ON PURPOSE, and now HIGHER (kAtkApexLift): the whole
// spear hangs a beat at the apex, then PLUNGES on an accelerating ramp --
// the last key drops 1400+ mm in two frames, which is what "strong and
// hard" costs. It lands on kAtkStickLift EXACTLY at kAtkImpactKey and holds
// it, dead still, to kAtkStickEnd; then pulls straight out along the lift
// axis BEFORE the fourth turn is allowed to swing.
static const Key kAtkLift[] = {
    {0, 0},          {8, 40},        {16, 560},      {24, 1250},
    {32, 2600},      {40, 4200},     {44, 5100},     {47, kAtkApexLift},
    {49, kAtkApexLift}, {50, 5480},  {51, 4900},     {52, 3800},
    {kAtkImpactKey, kAtkStickLift},  {kAtkStickEnd, kAtkStickLift},
    {205, 3000},     {207, 3100},    {210, 1800},    {214, 700},
    {217, 150},      {219, 0}};
// forward drive in mm: the leap TRAVELS (kAtkFwdMax by impact, held through
// the stick, returned across the landing so the loop closes)
static const Key kAtkFwd[] = {
    {0, 0},    {12, 0},    {16, 120},  {24, 450},  {32, 900},
    {40, 1400}, {47, 1750}, {50, 1850}, {kAtkImpactKey, kAtkFwdMax},
    {206, kAtkFwdMax}, {210, 1400}, {214, 700}, {219, 0}};
constexpr int kAtkLiftN = static_cast<int>(sizeof(kAtkLift) / sizeof(Key));
constexpr int kAtkFwdN = static_cast<int>(sizeof(kAtkFwd) / sizeof(Key));

// evaluate a key-domain curve at REEL-FRAME resolution (2 frames per key,
// lerped at the half key) -- the tracking camera calls these per frame
inline int32_t curve_half(const Key* k, int n, int frame) {
  const int f0 = frame >> 1;
  const int a = curve(k, n, f0);
  if ((frame & 1) == 0) return a;
  return (a + curve(k, n, f0 + 1)) / 2;
}
inline int32_t attack_lift_mm(int frame) { return curve_half(kAtkLift, kAtkLiftN, frame); }
inline int32_t attack_fwd_mm(int frame) { return curve_half(kAtkFwd, kAtkFwdN, frame); }

// ------------------------------------------------------------- bone map ----
enum : uint8_t {
  kBSpine0 = 0,
  kBFork = static_cast<uint8_t>(kSpineBones - 1),
  kBBladeL = static_cast<uint8_t>(kSpineBones),
  kBBladeL2 = static_cast<uint8_t>(kSpineBones + 1),
  kBBladeR = static_cast<uint8_t>(kSpineBones + 2),
  kBBladeR2 = static_cast<uint8_t>(kSpineBones + 3),
  kBSpike = static_cast<uint8_t>(kSpineBones + 4),
  kBoneCount = static_cast<uint8_t>(kSpineBones + 5)
};
static_assert(kBoneCount <= 32, "creature_rules 1.2: <= 32 bones");

// station -> world distance back from the nose
inline int32_t station_x(int i) {
  return static_cast<int32_t>((static_cast<int64_t>(kBodyLenMm) * i) / (kProfileStations - 1));
}
// station -> half-thickness in mm, off the HAND-AUTHORED taper (kTaper in
// KNOBS -- adjust by rendering and looking), with the first stations rounded
// into the nose dome (see kNoseDome)
inline int32_t station_r(int i) {
  const int t = (i * 1000) / (kProfileStations - 1);
  int r1000 = kTaper[kTaperKeys - 1].r;
  for (int k = 0; k + 1 < kTaperKeys; ++k) {
    if (t >= kTaper[k].t && t <= kTaper[k + 1].t) {
      const int span = kTaper[k + 1].t - kTaper[k].t;
      r1000 = kTaper[k].r + ((kTaper[k + 1].r - kTaper[k].r) * (t - kTaper[k].t) + span / 2) / span;
      break;
    }
  }
  int64_t r = (static_cast<int64_t>(kHeadHalfMm) * r1000) / 1000;
  if (i < kNoseDomeStations) r = (r * kNoseDome[i]) / 1000;
  return static_cast<int32_t>(r);
}
// station -> how wide the section is relative to how tall
inline int32_t station_wide(int i) { return i < kHeadStations ? kHeadWideNum : kBodyWideNum; }

// Which two bones a station blends between, and the first one's weight in
// 1/64. This linear ramp across every segment is what makes the chain one
// continuous deforming surface instead of a row of rigid tubes.
struct Bind {
  uint8_t b0, b1, w0;
};
inline Bind station_bind(int i) {
  const int64_t seg = (static_cast<int64_t>(i) * (kSpineBones - 1) * 1024) / (kProfileStations - 1);
  int k = static_cast<int>(seg >> 10);
  int frac = static_cast<int>(seg & 1023);
  if (k >= kSpineBones - 1) {
    k = kSpineBones - 2;
    frac = 1024;
  }
  const int w = 64 - ((frac * 64 + 512) >> 10);
  return Bind{static_cast<uint8_t>(kBSpine0 + k), static_cast<uint8_t>(kBSpine0 + k + 1),
              static_cast<uint8_t>(w)};
}

// ------------------------------------------------------------ the page ----
// The generated tables become a render::Tileset once, at first use. The tile
// indices below are the page's own order, and they are what a part's
// `page` field selects.
enum : uint8_t {
  kTileBody = 0,         // flank: dorsal band at U=192, throat wedge on the belly
  kTileHead = 1,         // head: blue front/underside, pink crown, side eyes
  kTileEye = 2,          // (reserved)
  kTileRim = 3,          // (reserved)
  kTileBladePinkUp = 4,  // tail blade: PINK upper face, GREEN lower
  kTileBladeGreenUp = 5  // tail blade: GREEN upper face, PINK lower
};

inline const zref::render::Tileset& page() {
  static const zref::render::Tileset ts = [] {
    zref::render::Tileset t;
    for (int i = 0; i < 256; ++i) t.palette[i] = kPagePalette[i];
    for (int k = 0; k < kPageTiles; ++k)
      for (int i = 0; i < 64 * 64; ++i) t.tiles[k][i] = kPageTexels[k][i];
    return t;
  }();
  return ts;
}

inline void set_rgb(zc::RingPart& p, const uint8_t c[3]) {
  p.r = c[0];
  p.g = c[1];
  p.b = c[2];
}

// ---------------------------------------------------------------- clips ----

// Every clip writes EVERY bone every key, composing with quat_mul rather than
// picking an axis, so nothing can pop at a threshold.
struct Rig {
  zc::quat16 q[kBoneCount];
  void reset() {
    for (int b = 0; b < kBoneCount; ++b) q[b] = zc::quat16_identity();
  }
  void tail_rest(int32_t splay = kBladeSplay, int32_t rise = kBladeRise) {
    q[kBBladeL] = quat_mul(quat_y(splay), quat_z(-rise));
    q[kBBladeR] = quat_mul(quat_y(-splay), quat_z(-rise));
    q[kBSpike] = quat_z(-rise / 2);
  }
  void write(zc::Clip& c, int f) const {
    for (int b = 0; b < kBoneCount; ++b) c.quats[static_cast<size_t>(f) * kBoneCount + b] = q[b];
  }
};

// The canonical S from the slope table: joint pitch is the DIFFERENCE of
// adjacent segment slopes, so the whole chain makes the shape and no single
// joint carries a corner. `authority` in 1/1000 scales the whole pose (the
// attack blends it away as it coils); `deepen` in 1/1000 scales ONLY the
// descent lobe -- the idle's breath, paired with a root rise so the belly
// stays on the ground while the head and arch bob.
// `wave`, when given, is a per-segment slope DELTA (angle16) -- the front
// wave that makes the raised body undulate. RETURNS the mm the root must
// RISE so the grounded run stays exactly where the plain stance put it:
// the height of the first grounded node is kBodyY - segL * sum(sin(slope))
// over the segments before it, so any slope change (deepen or wave) moves
// the belly by segL * sum(sin_new - sin_base), and the root compensates it
// EXACTLY. This replaces the hand-tuned kIdleBobComp -- and it IS the bob:
// as the breath deepens the lobe, the computed rise lifts the whole front.
inline int32_t apply_stance(Rig& g, int32_t authority, int32_t deepen = 0,
                            const int32_t* wave = nullptr) {
  int32_t prev = 0;
  int64_t sink = 0;  // fx16 mm of belly drop vs the plain stance
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);
  for (int k = 0; k < kStanceSlopes; ++k) {
    int64_t d = kStanceSlope[k];
    if (k >= kStanceDescend0 && k <= kStanceDescend1 && deepen != 0) {
      // The breath pushes every lobe slope toward MORE DESCENT. sin() rises
      // with the angle only below 90 deg (16384); past vertical it falls
      // again, so a doubled-back slope must move DOWN toward 90 to drop
      // deeper. A plain multiplicative deepen INVERTS there -- the pose
      // probe caught the belly LIFTING on the in-breath (2026-08-26).
      if (d <= 16384) {
        d += (d * deepen) / 1000;
      } else {
        d -= ((32768 - d) * deepen) / 1000;
      }
    }
    d = (d * authority) / 1000;
    if (wave != nullptr && wave[k] != 0) d += wave[k];
    if (k < kStanceGround0) {
      const int64_t base = (static_cast<int64_t>(kStanceSlope[k]) * authority) / 1000;
      const int32_t s_new =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(d & 0xFFFF)}).raw;
      const int32_t s_base =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(base & 0xFFFF)}).raw;
      sink += static_cast<int64_t>(segL) * (s_new - s_base);
    }
    const int32_t pitch = static_cast<int32_t>(d) - prev;
    prev = static_cast<int32_t>(d);
    g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
  }
  return static_cast<int32_t>(sink >> 16);  // mm of root RISE needed
}

// Slot 1 - IDLE. The canonical S, RELAXED. Fabian: "Like breathing, up and
// down, even body expanding a little and shrinking back. Make it slow." Plus
// "S form idle should also move body up and down slightly, a relaxed bob" and
// "play with tail a little. A little left right sway."
//
// Four things happen at once, all on different periods so the loop never reads
// as one oscillation:
//   1. the whole S gathers and relaxes      (stance authority breathes)
//   2. the animal bobs vertically           (root displacement)
//   3. the body swells and shrinks in girth (instance bulk, driven by the
//      reel -- clips carry rotations and a root offset, not scale)
//   4. the tail sways left and right        (yaw about the world vertical)
// 96 keys is 3.2 s, which is slow on purpose.
inline zc::Clip build_idle() {
  zc::Clip c;
  c.slot_id = 1;
  // 60 Hz presentation interpolation. Keys stay authored at 30 Hz and every
  // event frame is untouched; only the shown pose is blended at the half
  // tick. Without it the somersault and the flail visibly step.
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kIdleKeys);
  c.root.assign(static_cast<size_t>(kIdleKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kIdleKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kIdleKeys;
  for (int f = 0; f < kIdleKeys; ++f) {
    const int32_t ph = f * per_key;
    const int32_t s = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
    // the tail runs on its own faster period so the loop never reads as one
    // oscillation
    const int32_t st =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2 - 21000) & 0xFFFF)}).raw;

    Rig g;
    g.reset();
    // 1+2. THE BREATH IS THE BOB, and the FRONT WAVE rides it (2026-08-27:
    // "front part needs to be more animated ... the entire upper body needs
    // to be less rigid and move up and down"). The wave is a slow slope-
    // delta undulation travelling through the raised front lobe; the breath
    // deepens the dive as before. apply_stance computes the EXACT root rise
    // that keeps the grounded belly planted under both -- and that rise IS
    // the visible bob of the whole front.
    const int32_t breath = ((s + 65536) * 500) >> 16;  // 0..1000
    int32_t wave[kStanceSlopes] = {};
    for (int k = 0; k < kStanceGround0; ++k) {
      // envelope: quiet at the nose, full through the arch, gone at ground
      const int env = k < 4 ? 550 + k * 150 : (kStanceGround0 - k) * 1000 / (kStanceGround0 - 4);
      const int32_t pw = ph + 14000 - k * kIdleWaveSpatial;
      const int32_t sw =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
      wave[k] = static_cast<int32_t>(
          (static_cast<int64_t>(sw) * kIdleWaveAmp * env / 1000) >> 16);
    }
    const int32_t rise = apply_stance(g, 1000, (breath * kIdleDeepen) / 1000, wave);

    // the head plays too: a SLIGHT side-to-side yaw on the first joints
    // ("Head needs a slight side to side, but only slight")
    {
      const int32_t sh =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph + 30000) & 0xFFFF)}).raw;
      for (int k = 0; k <= 2; ++k) {
        g.q[kBSpine0 + k] =
            quat_mul(g.q[kBSpine0 + k], quat_y((sh * kIdleHeadSway) >> 16));
      }
    }

    // 3. the tail plays: a lazy left-right sway on the RAISED tail only.
    // It used to spread over "the rear third" (joints 13+), but the 2026-08-27
    // long grounded run now owns those joints, and a lateral quat on a
    // pitched GROUNDED joint tilts its descent sideways -- the probe caught
    // the belly digging 30 mm in time with the sway. The raised stem starts
    // past kStanceGround1.
    const int32_t sway = (st * kIdleTailSway) >> 16;
    for (int k = kStanceGround1 + 1; k < kSpineBones; ++k) {
      const int reach = ((k - kStanceGround1 - 1) * 1000) / (kSpineBones - kStanceGround1 - 1);
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k],
          quat_y(static_cast<int32_t>((static_cast<int64_t>(sway) * (400 + reach)) / 1000)));
    }
    g.tail_rest(kBladeSplay + ((st * 900) >> 16), kBladeRise + ((s * 500) >> 16));
    g.write(c, f);
    // the computed root rise that keeps the belly planted (see apply_stance).
    // ROOT CHANNEL UNITS ARE fx16 METRES -- the first pass wrote plain mm
    // here, which is 1/65536 of a mm once decoded: the "bob" never existed.
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}

// Slot 2 - CATERPILLAR WALK. Vertical and longitudinal, not lateral: an arch
// travels down the body, the middle rises and bunches, the rear is drawn
// forward. The head and neck bob with it (Fabian). Lateral sway survives only
// as secondary life at a fraction of the vertical authority.
inline zc::Clip build_walk() {
  zc::Clip c;
  c.slot_id = 2;
  // 60 Hz presentation interpolation. Keys stay authored at 30 Hz and every
  // event frame is untouched; only the shown pose is blended at the half
  // tick. Without it the somersault and the flail visibly step.
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kWalkKeys);
  c.root.assign(static_cast<size_t>(kWalkKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kWalkKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kWalkKeys;
  // hump window: STRICTLY the near-flat grounded run. The height-field trick
  // assumes the base line under it is the ground line; letting the window
  // reach into the steep lobe segments swung the whole tail off a high joint
  // and dug the belly (probe, 2026-08-26).
  const int32_t bLo = kStanceGround0;                         // first node of the span
  const int32_t bHi = kStanceGround1 + 2;                     // last node of the span
  const int32_t span1000 = (bHi - bLo) * 1000;                // window width, milli-bones
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);        // segment length, mm
  for (int f = 0; f < kWalkKeys; ++f) {
    Rig g;
    g.reset();
    // the full S every frame, with the FRONT WAVE riding it (2026-08-27:
    // "We have the same upper body issues we have with idle. Too rigid, no
    // bobbing"). Two wave cycles per gait cycle; the wave stops one joint
    // short of the hump window so the height-field conversion below still
    // owns joint bLo-1 outright, and apply_stance's computed root rise keeps
    // the grounded run planted under the wave.
    const int32_t phw = f * per_key;
    int32_t wave[kStanceSlopes] = {};
    for (int k = 0; k + 1 < kStanceGround0; ++k) {
      const int env =
          k < 4 ? 550 + k * 150 : (kStanceGround0 - 1 - k) * 1000 / (kStanceGround0 - 5);
      const int32_t pw = phw * 2 + 9000 - k * kWalkWaveSpatial;
      const int32_t sw =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
      wave[k] = static_cast<int32_t>(
          (static_cast<int64_t>(sw) * kWalkWaveAmp * env / 1000) >> 16);
    }
    // the gait breath: two cycles per loop, phased a quarter ahead of the
    // wave so the surge reads as one motion travelling through the animal
    const int32_t sb =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((phw * 2 + 26000) & 0xFFFF)}).raw;
    const int32_t breath = ((sb + 65536) * 500) >> 16;  // 0..1000
    const int32_t rise = apply_stance(g, 1000, (breath * kWalkDeepen) / 1000, wave);

    // THE CATERPILLAR HUMP, as a height field h >= 0 over the grounded nodes,
    // converted to joint pitches through EXACT segment angles (asin16), so
    // the posed heights ARE the authored heights and the belly can arch UP
    // off the ground but never through it. The hump travels the window once
    // per loop; its amplitude fades in and out at the ends (sin envelope) so
    // the loop closes without a pop.
    int32_t h[kSpineBones] = {};
    const int32_t c1000 = bLo * 1000 + (f * span1000) / kWalkKeys;  // hump centre
    const int32_t env = zref::fx_sin(zref::angle16{static_cast<uint16_t>(
                            ((c1000 - bLo * 1000) * 32768 / span1000) & 0xFFFF)})
                            .raw;  // 0..65536 over the traverse
    for (int b = bLo; b <= bHi; ++b) {
      const int32_t d = b * 1000 - c1000;  // milli-bones from the hump centre
      if (d <= -kWalkHumpHalf || d >= kWalkHumpHalf) continue;
      // cos^2 bump
      const int32_t ca = zref::fx_cos(zref::angle16{static_cast<uint16_t>(
                             ((d * 16384) / kWalkHumpHalf) & 0xFFFF)})
                             .raw;
      const int64_t bump = (static_cast<int64_t>(ca) * ca) >> 16;  // 0..65536
      h[b] = static_cast<int32_t>((bump * env >> 16) * kWalkHumpMm >> 16);
    }
    // Convert to pitches through EXACT total segment slopes OVER THE BASE
    // stance: desired dy of segment k = its stance dy + (h[k+1]-h[k]), and
    // asin16 finds the one slope that produces it -- so the posed heights
    // ARE base + h whatever the base slope under the window is. (Treating
    // the window as flat ground mis-posed the 70-deg lobe segment at its
    // edge by tens of mm -- probe, 2026-08-26.)
    int32_t prev_extra = 0;
    for (int k = bLo - 1; k <= bHi && k < kSpineBones - 1; ++k) {
      const int32_t base = kStanceSlope[k];
      const int32_t dy_base = -static_cast<int32_t>(
          (static_cast<int64_t>(segL) *
           zref::fx_sin(zref::angle16{static_cast<uint16_t>(base & 0xFFFF)}).raw) >>
          16);
      const int32_t dh = h[k + 1] - h[k];
      const int32_t s_new = -asin16(dy_base + dh, segL);
      const int32_t extra = s_new - base;
      if (extra != prev_extra) {
        g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(extra - prev_extra));
      }
      prev_extra = extra;
    }

    // (the old zero-sum neck curl is gone -- the front wave above IS the
    // neck and upper-body motion now, and its belly cost is compensated
    // exactly instead of being forced to cancel)

    // secondary lateral life on the RAISED mid-body only (not the grounded
    // run -- a lateral quat on a pitched grounded joint digs the belly)
    for (int k = 5; k <= kStanceGround0 - 1; ++k) {
      const int32_t ph = f * per_key + k * 5000;
      const int32_t sw =
          (zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw * kWalkSway) >> 16;
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_y(sw));
    }
    g.tail_rest();
    g.write(c, f);
    // the computed root rise that keeps the grounded run planted under the
    // front wave (fx16 METRES, like every root channel)
    c.root[f * 3 + 1] = fxm(rise);
  }
  c.events = {{0, zc::kEvFoot, 0}, {static_cast<uint16_t>(kWalkKeys / 2), zc::kEvFoot, 1}};
  return c;
}

// Slot 3 - TRIPLE SALTO MORTALE, ending as a STRAIGHT SPEAR straight down.
// Fabian, at the head of MODELINGGUIDE: "salto up, become like a straight
// spear and smash down with real power" -- and "when it attacks, it's not
// relaxed, rigid, that's how it stabs after the sommersault in spear form."
//
// So `curl` runs 1000 (coiled, relaxed) -> 0 (a rigid straight spear) and the
// stance's authority goes with it: at the moment of the stab there is no
// relaxation left in the animal at all.
inline zc::Clip build_attack() {
  zc::Clip c;
  c.slot_id = 3;
  // 60 Hz presentation interpolation. Keys stay authored at 30 Hz and every
  // event frame is untouched; only the shown pose is blended at the half
  // tick. Without it the somersault and the flail visibly step.
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kAttackKeys);
  c.root.assign(static_cast<size_t>(kAttackKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kAttackKeys) * kBoneCount, zc::quat16_identity());

  // WHAT HAPPENS, in order (Fabian, 2026-08-26 base + 2026-08-27 rework):
  //
  //   1. the S gathers and the body ROLLS UP into a near-circle (curl)
  //   2. the WHOLE coil somersaults three times about its own centre while
  //      climbing HIGHER than before and TRAVELLING FORWARD: the spin lives
  //      on BONE 0 and the root displacement disp = c - R(theta)*c re-pivots
  //      it from the nose to the coil centre c = (0, kCoilR) above it
  //   3. the unroll finishes AT the apex: a rigid spear, hanging DIAGONAL --
  //      kAtkSpinStick leans the tail 30 deg from vertical, down-and-forward,
  //      a thrown javelin, NOT a plumb drop
  //   4. the spear PLUNGES on an accelerating ramp and the tip BITES 420 mm
  //      into the ground -- the ONE authorised clipping exception, now deep
  //      and lasting ("ignore the clipping rule and actually stick in the
  //      ground some")
  //   5. IT STICKS. Completely straight, dead still, 150 keys = 300 frames =
  //      5.000 s at 60 fps ("Make it stick there, don't reset it")
  //   6. it pulls straight OUT along the lift axis, and only then does the
  //      fourth turn swing (tip probed clear), the S re-gathers, the forward
  //      travel returns, and the loop closes clean.
  //
  // Lift and forward drive live at file scope (kAtkLift / kAtkFwd) because
  // the reel's TRACKING CAMERA follows the same authored path.

  // 1000 = rolled into the coil, 0 = straight.
  static const Key kCurl[] = {
      {0, 0}, {8, 350}, {16, 1000}, {40, 1000}, {47, 0}, {kAttackKeys - 1, 0}};
  // how much of the canonical S remains
  static const Key kAuth[] = {{0, 1000},          {8, 450},   {16, 0},
                              {206, 0},           {212, 650}, {kAttackKeys - 1, 1000}};
  // accumulated turn of the WHOLE BODY in 1/1000 of a full rotation. 3000 =
  // the three somersaults; kAtkSpinStick (3333) = the DIAGONAL spear, tail
  // 60 deg below horizontal pointing down-and-forward, HELD from the apex
  // through the dive, the impact and the whole stick; the extraction keeps
  // it held until the tip is probed clear of the ground (key 206), and only
  // then does the fourth turn land it. The -40 at key 10 is the wind-up.
  static const Key kSpin[] = {{0, 0},          {10, -40},        {16, 0},
                              {22, 700},       {30, 1600},       {38, 2600},
                              {44, 3050},      {47, kAtkSpinStick},
                              {206, kAtkSpinStick},              {210, 3650},
                              {214, 3900},     {kAttackKeys - 1, 4000}};
  const int nC = static_cast<int>(sizeof(kCurl) / sizeof(Key));
  const int nA = static_cast<int>(sizeof(kAuth) / sizeof(Key));
  const int nS = static_cast<int>(sizeof(kSpin) / sizeof(Key));

  // rolling up: 360 degrees spread over the 18 interior joints
  const int32_t coil_pitch = -(65536 / (kSpineBones - 2));

  for (int f = 0; f < kAttackKeys; ++f) {
    Rig g;
    g.reset();
    const int curl = curve(kCurl, nC, f);
    const int auth = curve(kAuth, nA, f);
    const int spin = curve(kSpin, nS, f);
    const int lift = curve(kAtkLift, kAtkLiftN, f);
    const int fwd = curve(kAtkFwd, kAtkFwdN, f);

    apply_stance(g, auth);
    // the coil: every interior joint bends the same way, so the body is a
    // wheel; bone 0 is left to the spin alone
    for (int k = 1; k < kSpineBones - 1; ++k) {
      g.q[kBSpine0 + k] =
          quat_mul(g.q[kBSpine0 + k], quat_z((coil_pitch * curl) / 1000));
    }
    // the somersault: the whole body turns on bone 0
    const int32_t theta = static_cast<int32_t>(
        (static_cast<int64_t>(spin) * 65536) / 1000);
    const uint16_t th16 = static_cast<uint16_t>(theta & 0xFFFF);
    g.q[kBSpine0] = quat_mul(quat_z(theta), g.q[kBSpine0]);

    // re-pivot the spin from the nose to the coil centre, faded with the curl
    const int32_t sth = zref::fx_sin(zref::angle16{th16}).raw;
    const int32_t cth = zref::fx_cos(zref::angle16{th16}).raw;
    const int32_t piv_x = static_cast<int32_t>((static_cast<int64_t>(kCoilR) * sth) >> 16);
    const int32_t piv_y = kCoilR - static_cast<int32_t>((static_cast<int64_t>(kCoilR) * cth) >> 16);

    // the blades close to the spear line while coiled or straight-diving,
    // and flare as the S returns
    g.tail_rest((kBladeSplay * auth) / 1000 + kBladeSplay / 5,
                (kBladeRise * auth) / 1000);
    g.write(c, f);
    c.root[f * 3 + 0] = fxm(fwd + (piv_x * curl) / 1000);
    c.root[f * 3 + 1] = fxm(lift + (piv_y * curl) / 1000);
  }
  c.events = {{kAtkImpactKey, zc::kEvAttack, 0}};  // contact: reel frame 106
  return c;
}

// Slot 4 - FALLING, the slow helpless tumble. REWRITTEN 2026-08-26. Fabian:
// "The falling it's become super jittery and rigid. It should be the
// opposite, slowly flailing. Particularly head and 'neck' needs to move
// instead of being rigid. And instead of this hyperactive jitter, slowly
// rotate it on all axes so it stands on its head at one point and comes up
// again. I think we did that well on salto."
//
// So, borrowed from the salto: the whole S turns about its OWN CENTRE -- the
// rotation lives on bone 0 and the root displacement c - R(q) c re-pivots it
// from the nose to the planform centre (quat_rot_vec, because this rotation
// runs on three axes at once). One full pitch turn per 3.2 s loop, so it is
// head-down at the half and comes back up; slow roll and yaw wobbles ride
// along so every axis moves. The flail is SLOW and LOOSE: the head and neck
// loll on one- and two-cycle waves, the mid-body writhes gently, the blades
// wave. The S is applied at full authority every frame -- the signature.
// The animal is AIRBORNE for the whole loop (kFallLift), no contact at all.
inline zc::Clip build_fall() {
  zc::Clip c;
  c.slot_id = 4;
  // 60 Hz presentation interpolation. Keys stay authored at 30 Hz and every
  // event frame is untouched; only the shown pose is blended at the half
  // tick. Without it the tumble visibly steps.
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kFallKeys);
  c.root.assign(static_cast<size_t>(kFallKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kFallKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kFallKeys;
  for (int f = 0; f < kFallKeys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    // THE S FIRST. Everything else is decoration on it.
    apply_stance(g, 1000);

    // SLOW LOOSE NECK: the head lolls on one- and two-cycle waves, phase
    // staggered down the first joints so the motion travels instead of
    // snapping. Strongest right at the head.
    for (int k = 1; k <= 4; ++k) {
      const int32_t p1 = ph + k * 7000;
      const int32_t p2 = ph * 2 + 16000 + k * 9000;
      const int32_t s1 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p1 & 0xFFFF)}).raw;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p2 & 0xFFFF)}).raw;
      const int32_t amp = kFallNeckAmp - (k - 1) * 500;
      g.q[kBSpine0 + k] =
          quat_mul(g.q[kBSpine0 + k],
                   quat_mul(quat_z((s1 * amp) >> 16), quat_y((s2 * (amp * 2 / 3)) >> 16)));
    }
    // THE LATERAL WAVE (2026-08-27): a slow serpentine undulation travelling
    // nose -> tail through EVERY spine joint -- the walk's caterpillar
    // principle turned sideways and freed from the ground. Strongest at the
    // head, easing toward the fork, one slow temporal cycle per loop: WAVEY,
    // not a spazz. This is what replaces the rigid-rod read.
    for (int k = 1; k < kSpineBones - 1; ++k) {
      const int32_t pw = ph - k * kFallWaveSpatial + 24000;
      const int32_t sw = zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
      const int env = 1000 - (k * 550) / (kSpineBones - 2);  // 1000 -> ~450
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k],
          quat_y(static_cast<int32_t>(
              (static_cast<int64_t>(sw) * kFallWaveAmp * env / 1000) >> 16)));
    }
    // a slow travelling roll-twist through the middle -- distress, not
    // enough to erase the S
    for (int k = 6; k <= 14; ++k) {
      const int32_t pw = ph - k * (65536 / 12) + 30000;
      const int32_t sw = zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_x((sw * kFallWritheAmp) >> 16));
    }
    // the blades wave, slow
    const int32_t fl =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2 + 9000) & 0xFFFF)}).raw;
    g.tail_rest(kBladeSplay + ((fl * 1800) >> 16), kBladeRise + ((fl * 1000) >> 16));

    // THE TUMBLE. One full pitch turn per loop (wraps exactly: 65536 = 0) so
    // the loop closes; slow roll and yaw wobbles complete whole cycles too.
    const int32_t theta =
        static_cast<int32_t>((static_cast<int64_t>(f) << 16) / kFallKeys);
    const int32_t t2 =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph + 17000) & 0xFFFF)}).raw;
    const int32_t t3 =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2 + 40000) & 0xFFFF)}).raw;
    const zc::quat16 tumble =
        quat_mul(quat_z(theta), quat_mul(quat_x((t2 * kFallRollAmp) >> 16),
                                         quat_y((t3 * kFallYawAmp) >> 16)));
    g.q[kBSpine0] = quat_mul(tumble, g.q[kBSpine0]);
    g.write(c, f);

    // re-pivot the tumble from the nose to the S's planform centre, exactly
    // the salto's disp = c - R(theta) c but with the full 3-axis rotation
    int32_t rx, ry, rz;
    quat_rot_vec(tumble, kFallPivotX, kFallPivotY, 0, rx, ry, rz);
    c.root[f * 3 + 0] = fxm(kFallPivotX - rx);
    c.root[f * 3 + 1] = fxm(kFallLift + kFallPivotY - ry);
    c.root[f * 3 + 2] = fxm(-rz);
  }
  return c;
}

// ------------------------------------------------------------ the build ----

inline const zc::CreatureType& type() {
  static const zc::CreatureType t = [] {
    zc::Skeleton sk;
    sk.bone_count = kBoneCount;

    // The spine chain: bone 0 at the nose, each next bone one segment further
    // back. Backward is -X and the animal sits kBodyY up.
    const int32_t seg = kBodyLenMm / (kSpineBones - 1);
    sk.bones[kBSpine0] = zc::Bone{kBSpine0, 0, fxm(kBodyY), 0};
    for (int k = 1; k < kSpineBones; ++k) {
      sk.bones[kBSpine0 + k] = zc::Bone{static_cast<uint8_t>(kBSpine0 + k - 1), -fxm(seg), 0, 0};
    }
    sk.bones[kBBladeL] = zc::Bone{kBFork, 0, 0, fxm(56)};
    sk.bones[kBBladeL2] = zc::Bone{kBBladeL, -fxm(kBladeLen / 2), 0, 0};
    sk.bones[kBBladeR] = zc::Bone{kBFork, 0, 0, -fxm(56)};
    sk.bones[kBBladeR2] = zc::Bone{kBBladeR, -fxm(kBladeLen / 2), 0, 0};
    sk.bones[kBSpike] = zc::Bone{kBFork, 0, fxm(30), 0};
    std::vector<zc::RingPart> parts;

    // ---- THE BODY: ONE continuous chain part, nose to fork ---------------
    {
      zc::RingPart p;
      p.chain = true;
      p.pitch_q = 1;
      p.yaw_q = 3;
      p.caps = zc::kCapBot | zc::kCapTop;
      for (int i = 0; i < kProfileStations; ++i) {
        const int32_t r = station_r(i);
        const Bind bd = station_bind(i);
        zc::RingSpec rs;
        rs.y = fxm(station_x(i));
        rs.radius = fxm(r);
        rs.segments = static_cast<uint8_t>(kSides);
        rs.b0 = bd.b0;
        rs.b1 = bd.b1;
        rs.w0 = bd.w0;
        rs.rx = fxm(r * station_wide(i) / 100);  // LATERAL
        rs.rz = fxm(r);                          // VERTICAL
        rs.cz = -fxm(kBodyY);  // chain rings are creature-global; UP is -cz
        p.rings.push_back(rs);
      }
      p.page = kTileBody;
      set_rgb(p, kGreen);  // fallback if the page is ever absent
      parts.push_back(p);
    }

    // ---- HEAD AND THROAT: a second chain over the SAME bones -------------
    // Marginally larger so it sits ON the body, and blue. Sharing the binds
    // means it deforms identically and no seam between them can ever open.
    {
      zc::RingPart p;
      p.chain = true;
      p.pitch_q = 1;
      p.yaw_q = 3;
      p.caps = zc::kCapBot;
      for (int i = 0; i < kHeadStations + 3; ++i) {
        const int32_t r = station_r(i) + 3;
        // THE EYE, as a lateral swell in the skull itself. Eased in and out
        // over the stations it spans so the head reads as one form with a wide
        // brow, not a tube with two lumps.
        int32_t eye_w = 0;
        if (i >= kEyeStation0 && i <= kEyeStation1) {
          const int span = kEyeStation1 - kEyeStation0;
          const int t = span > 0 ? ((i - kEyeStation0) * 1000) / span : 500;
          const int ease = 1000 - (2 * t - 1000) * (2 * t - 1000) / 1000;  // 0..1000..0
          eye_w = static_cast<int32_t>((static_cast<int64_t>(r) * kEyeBulgeNum * ease) / 100000);
        }
        const Bind bd = station_bind(i);
        zc::RingSpec rs;
        rs.y = fxm(station_x(i));
        rs.radius = fxm(r);
        rs.segments = static_cast<uint8_t>(kSides);
        rs.b0 = bd.b0;
        rs.b1 = bd.b1;
        rs.w0 = bd.w0;
        rs.rx = fxm(r * station_wide(i) / 100 + eye_w);  // LATERAL, + the eye
        rs.rz = fxm(r);
        rs.cz = -fxm(kBodyY);
        p.rings.push_back(rs);
      }
      p.page = kTileHead;
      set_rgb(p, kBlue);
      parts.push_back(p);
    }

    // ---- THE DORSAL CREST IS NOW PAINT, NOT GEOMETRY --------------------
    // It used to be a third chain riding on the back, because a ring part's
    // texture ran AROUND the body while the concept's stripe runs ALONG it,
    // and V restarted at every rigid part so a longitudinal marking could not
    // survive at all. One chain part fixed the V continuity and the page
    // paints the band at U=192 (the back), so the geometry is redundant --
    // 57 rings and 8 sides of it. This is the texture lane paying for itself
    // the first time it is used.

    // ---- THE EYES ARE PAINT AND CONTOUR, NOT PARTS ----------------------
    // They used to be four rigid ring parts on two dedicated bones: a yellow
    // ball and an orange rim, per side. See the knobs block for why that is
    // gone. Two bones and four parts recovered.

    // ---- TAIL: two big FLAT POINTY blades, plus a tiny middle spike ------
    // Flat is an elliptical case: broad laterally, thin vertically. Each blade
    // is a two-bone chain so it can flex when the animal flails.
    for (int side = 0; side < 2; ++side) {
      zc::RingPart p;
      p.chain = true;
      p.pitch_q = 1;
      p.yaw_q = 3;
      p.caps = zc::kCapBot | zc::kCapTop;
      const uint8_t broot = side == 0 ? kBBladeL : kBBladeR;
      const uint8_t btip = side == 0 ? kBBladeL2 : kBBladeR2;
      const int32_t bx = station_x(kProfileStations - 1);
      const int32_t bz = side == 0 ? 56 : -56;
      for (int i = 0; i < kBladeRings; ++i) {
        const int t = (i * 1000) / (kBladeRings - 1);
        const int32_t k = 1000 - (t * t) / 1000;  // pointy: taper accelerates
        zc::RingSpec rs;
        rs.y = fxm(bx + (kBladeLen * t) / 1000);
        rs.segments = static_cast<uint8_t>(kBladeSides);
        rs.radius = fxm(kBladeW0 * k / 1000);
        rs.rx = fxm(kBladeW0 * k / 1000 + 6);      // LATERAL: broad
        rs.rz = fxm(kBladeThick0 * k / 1000 + 3);  // VERTICAL: flat
        rs.cx = fxm(bz);
        rs.cz = -fxm(kBodyY);
        // root half on the root bone, tip half on the tip bone, blended
        const int wroot = t < 500 ? 64 - (t * 64) / 500 : 0;
        rs.b0 = btip;
        rs.b1 = broot;
        rs.w0 = static_cast<uint8_t>(64 - wroot);
        p.rings.push_back(rs);
      }
      // each fin is PINK on one face and GREEN on the other (Fabian,
      // 2026-08-26: "One side of the fin parts at the tail will have the
      // pink, the other the green, a bit difficult to texture") -- solved by
      // the U-split blade tiles; the two blades mirror which face is which,
      // so Front.png reads one green blade and one pink one.
      p.page = side == 0 ? kTileBladePinkUp : kTileBladeGreenUp;
      set_rgb(p, side == 0 ? kPink : kGreen);
      parts.push_back(p);
    }

    // the tiny middle spike
    {
      zc::RingPart p;
      p.pitch_q = 1;
      p.yaw_q = 3;
      p.bone = kBSpike;
      p.caps = zc::kCapBot | zc::kCapTop;
      p.rings = {{0, fxm(kSpikeR), 6},
                 {fxm(kSpikeLen / 2), fxm(kSpikeR * 6 / 10), 6},
                 {fxm(kSpikeLen), fxm(kSpikeR / 5), 6}};
      p.page = 255;  // a 26 mm spike does not repay a texture page
      set_rgb(p, kPink);
      parts.push_back(p);
    }

    zc::ClipBank bank;
    bank.bone_count = kBoneCount;
    bank.clips.push_back(build_idle());
    bank.clips.push_back(build_walk());
    bank.clips.push_back(build_attack());
    bank.clips.push_back(build_fall());

    zc::CreatureType type;
    type.type_id = 2;
    const char* reason = "";
    if (!zc::compile_creature(sk, bank, parts, type, &reason)) {
      std::fprintf(stderr, "zixxtrixx: compile failed: %s\n", reason);
    }
    type.page_set = &page();
    return type;
  }();
  return t;
}

}  // namespace zixx

#endif  // ZHAO_REEL_ZIXXTRIXX_H
