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

// the crayon page, generated from the sheets (pigment IS measurable).
// -DZIXX_DEBUG_PAGE swaps in the T7 sector/band debug page (same symbols,
// mkcreaturepage.py --debug) so a render proves the UV mapping before any
// real paint is judged. Diagnostic builds only, never shipped.
#ifdef ZIXX_DEBUG_PAGE
#include "zixxtrixx_page_debug.h"
#else
#include "zixxtrixx_page.h"
#endif

// NOTE 2026-08-26: zixxtrixx_profile.h (the taper derived from the drawing's
// distance transform) is DEMOTED to a comparison tool and no longer included.
// Fabian: "the worst thing you did was constructing the snake from measuring.
// V1 looked better because it was visually authored." The medial-axis
// half-thickness of a 2D drawing conflates real thickness with foreshortening
// and with the bends -- it measures a projection, not the animal. The taper
// below is authored BY EYE and lives in KNOBS where it can be turned.

// ============================== KNOBS ======================================
// Millimetres unless noted. Angles in angle16: 65536 = one turn, 182 ~ 1 deg.

// VARIABLE RADIAL DETAIL (G2, 2026-08-27 round-skull run; Fabian: "you
// really need to get the poly reduction ... most important thing for
// smooth Zixxtrixx"). The 30-sides-everywhere answer was buying triangles
// where nothing curves: with SMOOTH VERTEX NORMALS now in the renderer the
// shading cannot facet, so the sides go where the SILHOUETTE needs them --
// the ball skull and the eye corners -- and the calm trunk and thin tail
// run far leaner. The ring zipper stitches unequal counts (verified,
// creature_core.cpp), and U is the ring ANGLE so the texture never
// notices. ~1,930 tris at LOD0, inside the 1,400-2,000 authored target
// (was 3,680); the micro rung still derives by halving (min 3).
constexpr int kSidesSkull = 22;  // stations 0..kHeadEnd: the ball + eyes
constexpr int kSidesNeck = 18;   // the hook behind the skull
// 16, not 14, and NOT for the silhouette: with align 0 a 16-gon puts a
// vertex EXACTLY on the belly line (k=4 -> angle 16384 = straight down),
// so the grounded run's authored millimetre sink survives the poly cut --
// at 14 the bottom chord rode ~3 mm high and the probe's idle band went
// [-8..-3] -> [-8..-1], one key from reading as hover (ground-contact
// doctrine: the absence of declared penetration is also a bug).
constexpr int kSidesTrunk = 16;  // the long calm body + the grounded run
constexpr int kSidesTail = 10;   // the raised thinning stem
constexpr int kNeckEndStation = 17;   // last neck-count station
constexpr int kTrunkEndStation = 48;  // last trunk-count station: PAST the
                                      // grounded run's final node, so every
                                      // ground-touching ring keeps its
                                      // belly-line vertex
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
// ONE TUBE, CULMINATING IN A HEAD (2026-08-28, Fabian: "There should
// really be no skull. It's one tube that bulges more and more towards the
// end, culminating in a head."). The previous profile dipped at the neck
// (800 at t=320 against a trunk of 860) and hid the head's volume in a
// separate ball envelope that eased back to nothing -- so head, neck and
// body flowed into each other with no culmination. Now the radius GROWS
// MONOTONICALLY from the trunk forward, accelerating toward the nose, and
// peaks just behind the blunt dome: the head is simply where the tube has
// become widest, exactly as Side.png draws it (the authority on shape).
// Authored BY EYE against Side.png; the sheet's widest point reads about
// 1.5-1.7x the mid-body width, which is a comparison-side check, not a
// generator. Body thickness from t=620 back stays EXACTLY as approved.
constexpr TaperKey kTaper[] = {
    {0, 1150},  {40, 1290}, {90, 1360}, {150, 1330}, {230, 1190},  // the CULMINATION
    {330, 1040}, {450, 940}, {560, 885}, {620, 860},               // the swell builds in
    {720, 790}, {820, 620}, {900, 450}, {950, 330}, {1000, 260}    // tail stem
};
constexpr int kTaperKeys = static_cast<int>(sizeof(kTaper) / sizeof(TaperKey));
// Bind height of the body axis. This is the HEAD height: bone 0 is the nose
// and the reel ground-snaps the ROOT, so the head is CARRIED at this height
// and the whole S hangs from it. TUNED against the pose probe
// (tools/reel/zixx_probe.cpp): the grounded run's belly rides a few mm under.
// The skinned mesh sits ~15 mm below the centreline prototype (ring blending
// sags into the bends), so this is chosen off the PROBE, not the sketch.
constexpr int32_t kBodyY = 539;  // retuned 2026-08-27 pass 3: the blade
                                 // re-rake and the neck re-sum left the idle
                                 // belly touching 0 at one key -- 3 mm down
                                 // restores the authored sink (probe: idle
                                 // [-7..-3] mm, walk [-13..+10] mm)
// Planform centre of the posed S, nose to tail extent midpoint, for staging:
// the folded S spans ~1.8 m behind the nose, so the reel offsets the instance
// by this to keep the animal centred in an orbit shot.
constexpr int32_t kStageCentreMm = 920;

// THE NOSE IS A DOME, NOT A DISC. The measured profile starts at full
// half-thickness, so station 0 used to be a full-radius ring closed by a flat
// 20-gon cap -- the "weird spinning disc" at the front of the face. These
// factors (1/1000 of the profile value) round the first stations into a
// blunt dome; the cap that remains is a dot. Factors raised slightly with
// the head-only retaper: the nose base radius shrank, so the dome rounds
// off BLUNTER rather than drawing the smaller radius out into a point.
constexpr int kNoseDomeStations = 4;
constexpr int16_t kNoseDome[kNoseDomeStations] = {380, 760, 925, 985};

// section ellipticity, measured: wider than tall
constexpr int32_t kHeadWideNum = 112;  // head 1.12 : 1
constexpr int32_t kBodyWideNum = 119;  // body 1.19 : 1
constexpr int kHeadStations = 9;       // how many stations read as head
// The last station of the HEAD PART (inclusive). The head is no longer an
// overlay shell riding 3 mm over the body (2026-08-27 head-only run,
// Headache.md: "stop using the overlay as the head shape" -- twelve rings of
// near-duplicate anatomy fought the body the moment the bulb got fatter and
// the hook tighter). The head part now IS the surface for stations
// 0..kHeadEnd and the body part begins AT kHeadEnd with the identical ring
// spec, so the two meet vertex-for-vertex and nothing overlaps by
// construction. The blue/pink/green split stays on the TEXTURE tiles.
constexpr int kHeadEnd = kHeadStations + 2;  // station 11, x = 599 mm

// ---- THE HEAD-ATTITUDE BONE (2026-08-27 head-only run) --------------------
// The droop survived a +4000 first stance slope because the visible skull is
// NOT the first segment: the bulb spans stations 0..8 = 2.7 segments, and
// stations 3..8 -- the cranium -- ride bones 1..3, which are the steep
// -11200/-12800/-12000 hook. The nose tipped up 22 deg while the mass of
// the head bent down around the hook behind it. So the skull now binds to
// ONE dedicated bone (kBHead, child of bone 0 at the nose): the cranium is
// a rigid mass whose pitch is THIS knob, the canonical S is untouched, and
// the neck blend eases the skull into the hook over three stations.
// kHeadAttitude is angle16 relative to the first segment's slope, and the
// SIGN WAS LEARNED FROM THE RENDER, not derived: POSITIVE PITCHES THE NOSE
// DOWN in the composed skeleton -- the inverse of what every slope comment
// assumed, which is why pass 3's "+4000 = 22 deg of nose lift" shipped as
// droop. -12000 was PICKED OFF THE EIGHTEEN-POSE ORIENTATION SWEEP
// (attitude-sweep.png in the run folder; -8000..+8000, then -14000..+2000
// after the whole first sheet still hung nose-down): the skull rides level
// with a slight forward-up lift, the eye reads mid-ball, the nose stays
// clear of both the ground and the hook.
// -6000, RE-PICKED 2026-08-28 (run 2339; owner: "Head should look up more.
// You should see the face straight on ... Snout should point horizontal
// and maybe even up a little" — the -12000 verdict was overturned by the
// published render). HOW IT WAS FOUND, because the sign convention burned
// its FOURTH pass first: the -12000..-40000 visual sweeps read "better"
// the more negative they got, but the committed head-axis probe
// (zixx_headaim.cpp — skins the actual station centres) showed NEGATIVE
// pitches the nose DOWN (~5.5 deg per 1000): -12000 was snout -28 deg,
// -34000 was the head folded 149 deg under with its BACK reading as a
// convincing "face". The old comment's "POSITIVE PITCHES THE NOSE DOWN"
// was inverted. At -6000 the measured snout axis is +4.6 deg (horizontal,
// a touch up) and the RENDER agrees: front camera reads pink cap / blue
// face / side eyes / mouth like Front.png; the measurement removed the
// bias, the render chose the value.
#ifndef ZIXX_ATTITUDE
#define ZIXX_ATTITUDE (-6000)
#endif
constexpr int32_t kHeadAttitude = ZIXX_ATTITUDE;
// where the skull bone pivots, mm behind the nose (~station 3.5, the
// culminating head's centroid — see the bone-table note)
constexpr int32_t kHeadPivotMm = 0;
constexpr int kSkullRigidTo = 5;  // stations 0..5 fully on the head bone
constexpr int kSkullBlendTo = 9;  // stations 6..9 blend head -> spine (four
                                  // stations: three collapsed the fold onto
                                  // the eye's rear -- seen on the sweep)

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
constexpr int kEyeStation0 = 4;      // first head station that carries the bulge
constexpr int kEyeStation1 = 7;      // last. 4..7, was 3..8 (v3 run): six
                                     // stations of swell survived as a
                                     // continuous lateral ridge along the
                                     // whole head -- the "brim". Two fewer
                                     // stations localise it at the eyes.
#ifndef ZIXX_EYEBULGE
#define ZIXX_EYEBULGE 16
#endif
constexpr int32_t kEyeBulgeNum = ZIXX_EYEBULGE;
                                     // extra lateral half-width, % of the ring.
                                     // 16, was 42 (v3 run): at 42 the head's
                                     // head-on silhouette was a narrow pink
                                     // dome sitting on a wide flat disc -- a
                                     // MUSHROOM, not Front.png's rounded
                                     // blob. The owner's "give them more
                                     // bulge" stands as TWO LOCAL swellings
                                     // at the eyes (see kEyeStation0/1), not
                                     // a rim around the whole head. Picked
                                     // off a rendered head-on ladder
                                     // (12/16/20/24, v3 run evidence);
                                     // judged beside Front.png.

// THE BALL ENVELOPE IS RETIRED (2026-08-28). Fabian: "There should really
// be no skull. It's one tube that bulges more and more towards the end,
// culminating in a head." The culmination now lives IN kTaper itself --
// one hand-authored radius profile for the one tube -- so a second local
// swell grafted on top would re-create exactly the seam he rejected.
// kBallNum stays as a knob (0 = off) because the machinery also carries
// the eye rim; the envelope stations are kept for it.
constexpr int kBallStation0 = 1;   // envelope start (unused at kBallNum 0)
constexpr int kBallPeak = 4;       // envelope peak (unused at kBallNum 0)
constexpr int32_t kBallNum = 0;    // peak swell, 1/1000 of station radius

// the dorsal crest: geometry, because there is no texture page pipeline yet
constexpr int32_t kCrestNum = 46;   // crest half-width = body half-width * n/100
constexpr int32_t kCrestLift = 104;  // crest centre, as a % of body half-height

// the tail: two SMALL flat pointy blades left and right, plus a tiny middle
// spike. Fabian, 2026-08-26: "The fins are gargantuan while on the reference
// sketch they are small." The first sizing (1180 mm on a 3050 mm body) put a
// blade at 39% of the animal; the sketch's slivers are a sixth of that mass.
// 6/6, was 8/7 (G2): a blade is a thin flat sliver -- with smooth normals
// the faces cannot facet, and six sides keep the sharp lateral rim the
// sheet draws while dropping a third of the fin cost.
constexpr int kBladeSides = 6;
constexpr int kBladeRings = 6;
// 780: the sheet's fins are LONG slivers -- ~26% of the body's path length --
// not paddles (the first "gargantuan" verdict was about their WIDTH). Length
// up, half-width kept slim.
constexpr int32_t kBladeLen = 780;
constexpr int32_t kBladeW0 = 70;       // half-width at the root (LATERAL)
constexpr int32_t kBladeThick0 = 16;   // half-thickness at the root (VERTICAL)
// 1500, was 6900 (Fabian, 2026-08-27 pass 3: the fins "should be rotated
// almost 90 degrees so [the fins] are almost lined up with body. But not
// quite"): each blade now trails ~8 deg off the tail's own axis instead of
// splaying ~38 deg across it.
constexpr int32_t kBladeSplay = 1500;
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
// PINK, third pass (Fabian, 2026-08-27): "a bit less neon and a bit more
// like on the sketch. A bit neon-y is fine though" -- pulled ~40% from the
// 2026-08-26 neon toward the measured sheet pigment (233,188,206), NOT all
// the way back to the pale rose that failed. Keep tools/pack/mkcreaturepage.py
// in step.
constexpr uint8_t kPink[3] = {246, 94, 183};     // dorsal band
constexpr uint8_t kBlue[3] = {3, 145, 205};      // head and throat
constexpr uint8_t kYellow[3] = {243, 232, 142};  // eye
// ONE pencil serving TWO features: the ring round the eye in Front.png and the
// wavy slit pupil in Side.png.
constexpr uint8_t kOrange[3] = {218, 106, 71};

// -- animation --------------------------------------------------------------
// A key is held 2 sim ticks, so reel frames = keys * 2 at step 1.
constexpr int kIdleKeys = 96;  // SLOW. 3.2 s of breathing.
constexpr int kWalkKeys = 40;
// 226 keys = 452 frames = 7.53 s at 60 Hz. The salto sticks its landing as
// a planted spear for FIVE REAL SECONDS (Fabian: "Make it stick for 5
// actual seconds"): keys 62..212 are the stick -- 150 keys, 300 frames,
// 5.000 s at the site's 60 fps -- and the remaining 13 keys pull it out and
// close the loop. 220 -> 226 keys 2026-08-28, OWNER-LICENSED EDIT to the
// frozen salto (Fabian: "the salto is great. Maybe have it hold a tad
// longer at its apex before the spear comes down, for effect"): the apex
// HANG grows from 2 keys (~0.07 s) to 8 (~0.27 s) -- a hang-time beat,
// not slow motion; the plunge keeps its exact speed and violence, every
// later key shifts +6. clip-3.bin re-pins with this provenance.
constexpr int kAttackKeys = 226;
constexpr int kFallKeys = 144;  // SLOWER STILL (2026-08-27 pass 3, Fabian:
                                // "When falling, the rotation is too
                                // strong"): one tumble now takes 4.8 s, so
                                // the full turn keeps its stand-on-its-head
                                // moment but at 2/3 the angular speed.

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
    // neck: the cobra hook. THE HEAD LOOKS UP (2026-08-27 pass 3, Fabian:
    // "it looks to ofar downward. You can't see most of the blue face, and
    // you can't see the mouth ... make it look up more"): the FIRST segment
    // is now POSITIVE -- the nose rides ~14 deg ABOVE level, so the blue
    // face and the mouth present to the camera -- and the three hook
    // segments behind it steepen to keep the neck's SINE SUM (so the apex
    // height and every grounded number are unchanged; probe-verified).
    // +4000 (~22 deg up), not less: the showcase cameras look DOWN ~15 deg,
    // so a merely-level head still presents its crown -- the lift must beat
    // the camera pitch before the face reads (checked on head-on frames).
    4000, -11200, -12800, -12000,
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
// THE HEAD RISES WITH THE IN-BREATH (2026-08-28). Two jobs in one knob:
// the culminating head + look-up attitude nests ~235 mm into the deepened
// hook at the breath extreme (probe, run 2339), and lifting the face as
// the S gathers moves the skull rear DOWN away from the stroke (positive
// attitude = nose up, zixx_headaim) while reading as the animal actually
// inhaling. Angle16 of extra attitude at full breath.
constexpr int32_t kIdleBreathLift = 2200;
// TORSIONAL BREATH for the grounded run (2026-08-27, reports/
// ZixxtrixxReport: the middle is deliberately protected from both the
// front wave and the tail sway -- "animated front | dead zone | animated
// tail" -- and it read static). A tiny local-axis ROLL wave travels the
// grounded joints: it turns the cross-section, so the dorsal stripe and
// the crayon visibly breathe, but it cannot lift the centreline -- the
// belly stays planted (elliptical-section depth change at +-4 deg is
// under half a millimetre; probe-verified).
constexpr int32_t kIdleTorsion = 800;
// SIDEWAYS SNAKING for the grounded run (2026-08-28, Fabian: "Our idle has
// an unmoving part where it's standing. Find something for that to do,
// maybe sideways snaking."). The idle's dead zone was exactly the part
// that is touching the ground, and the house rule says that is where the
// travelling motion belongs -- this is the walk's principle at rest: a
// slow lateral S travels the grounded joints, one temporal cycle per
// 3.2 s loop (loop-exact, wobble not jitter). Two recorded traps steered
// the numbers: a lateral wave once measured INVISIBLE at 240p (so the
// amplitude is real, ~17 deg at the crest, judged on a contact sheet),
// and a lateral quat on a PITCHED grounded joint digs the belly (so the
// envelope tapers hard over the rear joints where the grounded slopes
// steepen -- the probe's idle band is the arbiter).
constexpr int32_t kIdleSnakeAmp = 2900;      // peak yaw per joint
// EXACTLY one full wavelength across the six grounded joints (65536/6), so
// the accumulated yaw over the run is ~ZERO at every phase: the steep tail
// rise behind the run stays in the sagittal plane. The first cut used an
// arbitrary 7200 step with a tapered envelope, and the accumulated ~30 deg
// of yaw tilted the -5600/-11400 rise sideways -- the blade tips dug
// -71 mm and one key HOVERED (+4). Zero-sum is the same trick as the
// walk's zero-sum neck curl, sideways.
constexpr int32_t kIdleSnakeSpatial = 10923;

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
// THE SALTO, third pass (Fabian, 2026-08-27 pass 3: "it should go up
// higher ... I mean a lot higher. And it should shoot downward many meters,
// maybe tens of meters. All at about 30 degree angle ... the skewering
// should be one very long straight shot. It should stick in the ground."):
//   - apex lift 5600 -> 12000 mm: a twelve-metre leap;
//   - the PLUNGE IS ONE STRAIGHT SHOT ALONG THE SPEAR'S OWN 30-DEG LINE:
//     from the apex the root drops 9645 mm while driving 5570 mm forward --
//     dy/dx = tan(60) exactly, so the flight path is parallel to the held
//     javelin axis: ~11.1 m of dead-straight travel into the ground;
//   - spin 3333 holds the tail 60 deg below horizontal, down-and-FORWARD;
//   - the tip buries 420 mm (the authorised clipping exception) and STICKS
//     for 150 keys = 300 frames = 5.000 s, bit-constant;
//   - the tracking camera aims at the SPEAR MIDPOINT through the dive and
//     the stick (kAtkAim below), so the ground hit -- THE shot -- is framed.
// Tail direction at spin s (1/1000 turn, minus the 3 whole somersaults):
// 180 deg + s*0.36 deg. At 3333 that is 300 deg == 60 deg below horizontal
// toward +X. Tip drop from the nose = reach * sin(60) = 3830 * 0.86603 =
// 3317 mm (reach = kBodyLenMm + kBladeLen with the blades on the spear line);
// tip forward reach = 3830 * 0.5 = 1915 mm.
constexpr int32_t kAtkApexLift = 12000;  // mm of root lift at the apex
constexpr int32_t kAtkFwdMax = 7420;     // mm forward at impact: 1850 by the
                                         // apex + 5570 on the 30-deg plunge
constexpr int32_t kAtkSpinStick = 3333;  // 1/1000 turns: 30 deg from vertical
constexpr int32_t kAtkStickDepth = 420;  // mm of authored burial
constexpr int32_t kAtkTipDrop = 3317;    // reach * sin(60 deg), see above
constexpr int32_t kAtkTipFwd = 1915;     // reach * cos(60 deg): how far the
                                         // buried tip leads the nose in +X
constexpr int32_t kAtkStickLift = kAtkTipDrop - kBodyY - kAtkStickDepth;
constexpr int kAtkImpactKey = 62;        // reel frame 124 (keys held 2 ticks)
constexpr int kAtkStickEnd = 212;        // impact + 150 keys = 5.0 s stuck

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
// ROTATION DOWN, WOBBLE UP (2026-08-27 pass 3, Fabian: "When falling, the
// rotation is too strong. On the other hand, the snake should be wobbly ...
// creature in fall mode should wobble more, not jitter"). The rigid-body
// rotation amplitudes shrink; the BENDY amplitudes -- neck loll, serpentine
// wave, writhe -- grow, all on the same slow one-per-loop periods (slower
// still now the loop is 4.8 s), so the extra motion is loose and continuous,
// never a twitch. The idle/walk wobble is the reference.
constexpr int32_t kFallRollAmp = 4400;  // ~24 deg of slow roll wobble (was 35)
constexpr int32_t kFallYawAmp = 3400;   // ~19 deg of slow yaw wobble (was 28)
constexpr int32_t kFallNeckAmp = 5000;  // slow loose head/neck flail, up again
constexpr int32_t kFallWritheAmp = 1700;  // mid-body roll-twist writhe
// THE LATERAL WAVE (Fabian, 2026-08-27: "It needs to be wobbly side to
// side ... It needs to be wavey. The walk does waveyness pretty well for the
// caterpillar part"). A slow serpentine wave travelling nose -> tail through
// every spine joint, strongest at the head, free of the ground because the
// fall never touches it. This is the walk's principle turned sideways.
constexpr int32_t kFallWaveAmp = 3600;     // ~20 deg at the head (was 14 --
                                           // "should wobble more")
constexpr int32_t kFallWaveSpatial = 4700; // ~1.3 wavelengths down the body
// THE S RELAXES BY A TON (2026-08-28, Fabian: "it should be less rigid and
// S shape needs to relax by a ton" — F1's time- and region-varying
// authority, landed with the owner's own number on it). The falling body
// no longer holds the full canonical S every frame: per-joint authority
// swings kFallAuthMid ± kFallAuthSwing on ONE slow cycle per loop
// (loop-exact), phase-stepped kFallAuthSpatial per joint so the gather
// and release TRAVEL down the body — the animal stretches almost
// straight, collapses into a tighter curve, and the recognisable S
// RECURS instead of being mandatory. Wobble not jitter: one term, slow.
constexpr int32_t kFallAuthMid = 500;      // mean S authority, 1/1000
constexpr int32_t kFallAuthSwing = 340;    // swing: 16%..84% over the loop
constexpr int32_t kFallAuthSpatial = 3400; // phase step per joint
// NONUNIFORM tumble (2026-08-27, reports/ZixxtrixxReport: "a perfectly
// uniform full revolution reads like a display turntable"). The tumble
// phase is warped by this * sin(phase): it accelerates through one half of
// the turn and hesitates through the other, while the endpoints stay exact
// so the loop still closes. ~9% of a turn of maximum lead/lag.
constexpr int32_t kFallTumbleWarp = 5800;

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

// quaternion conjugate (unit inverse): the tool that lets a WORLD-axis
// rotation be expressed exactly in a joint's local frame, L = q* W q.
inline zc::quat16 quat_conj(const zc::quat16& q) {
  return zc::quat16{{q.q[0], static_cast<int16_t>(-q.q[1]),
                     static_cast<int16_t>(-q.q[2]), static_cast<int16_t>(-q.q[3])}};
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
// Lift in mm. A LOT HIGHER (kAtkApexLift = 12 m, Fabian 2026-08-27 pass 3),
// and the launch now has ANTICIPATION (owner, via reports/ZixxtrixxReport:
// "lack of a cool jump start that compresses the snake S then shoots up"):
// keys 0..6 COMPRESS (kAtkPre deepens the S while the root stays down),
// keys 6..9 HOLD the loaded pose almost still, and the RELEASE at key 10+
// is a hard acceleration -- the lift stays at 0 through the hold and then
// snaps 0 -> 3200 mm inside six keys. The climb tops out at the apex hang
// (keys 47..49), then PLUNGES on a quadratic-in-time ramp -- dive keys
// 49..56 are lift = apex - t^2 * 9645, accelerating the whole way down,
// landing on kAtkStickLift EXACTLY at kAtkImpactKey; held, dead still, to
// kAtkStickEnd; then it pulls straight out along the lift axis BEFORE the
// fourth turn is allowed to swing.
static const Key kAtkLift[] = {
    {0, 0},          {10, 0},        {12, 180},      {14, 700},
    {16, 1500},      {20, 3200},     {26, 5600},     {32, 8200},
    {38, 10600},     {43, 11700},
    {47, kAtkApexLift}, {55, kAtkApexLift},   // the LICENSED longer hang
    {56, 11803},     {57, 11213},    {58, 10228},    {59, 8851},
    {60, 7079},      {61, 4914},
    {kAtkImpactKey, kAtkStickLift},  {kAtkStickEnd, kAtkStickLift},
    {214, 3200},     {216, 3400},    {219, 2200},    {222, 900},
    {224, 200},      {225, 0}};
// forward drive in mm. THE PLUNGE IS THE STRAIGHT SHOT: over the dive keys
// the drive is 1850 + t^2 * 5570 -- the SAME t^2 as the lift, so every dive
// key sits exactly on the 30-degrees-from-vertical line the spear points
// along. Held through the stick, returned across the landing for the loop.
static const Key kAtkFwd[] = {
    {0, 0},     {14, 0},    {18, 150},  {26, 500},  {34, 1000},
    {42, 1550}, {49, 1850}, {55, 1850},  // the drive hangs with the lift
    {56, 1964}, {57, 2305}, {58, 2873}, {59, 3669}, {60, 4692}, {61, 5942},
    {kAtkImpactKey, kAtkFwdMax},
    {kAtkStickEnd, kAtkFwdMax}, {216, 5200}, {220, 2600}, {225, 0}};
// THE PRELOAD (anticipation): 1/1000 of extra descent-lobe authority fed to
// apply_stance's deepen -- the same mechanism as the idle's breath, pushed
// far past it, so the S visibly TIGHTENS and shortens ("stored energy"),
// with the computed root rise keeping the belly planted. Released as the
// coil begins.
static const Key kAtkPre[] = {
    {0, 0}, {3, 380}, {6, 700}, {9, 700}, {12, 260}, {15, 0}, {225, 0}};
constexpr int kAtkPreN = static_cast<int>(sizeof(kAtkPre) / sizeof(Key));
// how much the TRACKING CAMERA aims at the spear's midpoint instead of the
// nose, in 1/1000 (Fabian, 2026-08-27 pass 3: the camera "doesn't catch the
// most important thing, which is the ground hit where the tail actually
// buries"). 0 while the body is a coil around the root; blended in as the
// spear forms at the apex; HELD through the dive, the impact and the whole
// five-second stick -- the buried tail is the shot -- and released only as
// the extraction re-gathers the S.
static const Key kAtkAim[] = {
    {0, 0}, {40, 0}, {47, 1000}, {214, 1000}, {220, 0}, {225, 0}};
constexpr int kAtkLiftN = static_cast<int>(sizeof(kAtkLift) / sizeof(Key));
constexpr int kAtkFwdN = static_cast<int>(sizeof(kAtkFwd) / sizeof(Key));
constexpr int kAtkAimN = static_cast<int>(sizeof(kAtkAim) / sizeof(Key));

// The coil / stance-authority / spin curves live at FILE SCOPE (moved
// 2026-08-27 round-skull run) because the CHOREO PROOF needs them: the
// programmable-salto architecture (C1) re-derives the per-key root from
// these same numbers, and tools/reel/zixx_choreo.cpp diffs the result
// against the golden decomposition.
// 1000 = rolled into the coil, 0 = straight. The roll-up waits for the
// anticipation (keys 0..9 are the compress + hold).
static const Key kAtkCurl[] = {
    {0, 0}, {9, 0}, {13, 350}, {18, 1000}, {40, 1000}, {47, 0}, {kAttackKeys - 1, 0}};
// how much of the canonical S remains -- FULL through the compress/hold
// (the preload deepens it on top), gone by the time the coil owns the body
static const Key kAtkAuth[] = {{0, 1000},          {9, 1000},  {18, 0},
                               {214, 0},           {220, 650}, {kAttackKeys - 1, 1000}};
// accumulated turn of the WHOLE BODY in 1/1000 of a full rotation. 3000 =
// the three somersaults; kAtkSpinStick (3333) = the DIAGONAL spear, tail
// 60 deg below horizontal pointing down-and-forward, HELD from the apex
// through the dive, the impact and the whole stick; the extraction keeps
// it held until the tip is probed clear of the ground (key 206), and only
// then does the fourth turn land it. The -40 at key 12 is the wind-up --
// INSIDE the release, not the hold: at spin -40 the whole body pitches
// about the nose, and during the grounded compress that floated the rear
// 750 mm off the dirt (probe). By key 12 the launch is already airborne.
static const Key kAtkSpin[] = {{0, 0},          {10, 0},          {12, -40},
                               {15, 0},         {20, 700},        {28, 1600},
                               {36, 2600},      {43, 3050},       {47, kAtkSpinStick},
                               {214, kAtkSpinStick},              {218, 3650},
                               {221, 3900},     {kAttackKeys - 1, 4000}};
constexpr int kAtkCurlN = static_cast<int>(sizeof(kAtkCurl) / sizeof(Key));
constexpr int kAtkAuthN = static_cast<int>(sizeof(kAtkAuth) / sizeof(Key));
constexpr int kAtkSpinN = static_cast<int>(sizeof(kAtkSpin) / sizeof(Key));

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
inline int32_t attack_aim_mille(int frame) { return curve_half(kAtkAim, kAtkAimN, frame); }

// ------------------------------------------------------------- bone map ----
enum : uint8_t {
  kBSpine0 = 0,
  kBFork = static_cast<uint8_t>(kSpineBones - 1),
  kBBladeL = static_cast<uint8_t>(kSpineBones),
  kBBladeL2 = static_cast<uint8_t>(kSpineBones + 1),
  kBBladeR = static_cast<uint8_t>(kSpineBones + 2),
  kBBladeR2 = static_cast<uint8_t>(kSpineBones + 3),
  kBSpike = static_cast<uint8_t>(kSpineBones + 4),
  // the dedicated skull bone: child of the root at the nose, carries the
  // rigid cranium (see kHeadAttitude). 26 of 32.
  kBHead = static_cast<uint8_t>(kSpineBones + 5),
  kBoneCount = static_cast<uint8_t>(kSpineBones + 6)
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

// station -> radial segment count (variable detail, see the knobs). The
// junction ring (kHeadEnd) takes the SKULL count in both parts -- a function
// of the station alone, so the two parts' shared ring stays bit-identical.
inline int station_sides(int i) {
  if (i <= kHeadEnd) return kSidesSkull;
  if (i <= kNeckEndStation) return kSidesNeck;
  if (i <= kTrunkEndStation) return kSidesTrunk;
  return kSidesTail;
}

// THE HEAD RING, shared by the mesh builder and the pose probe so the probe
// measures the surface that is actually built. Returns the LATERAL (rx) and
// VERTICAL (rz) half-extents in mm for a head-part station 0..kHeadEnd.
//   - the ball envelope e (0..1000) smoothsteps up from kBallStation0 to
//     kBallPeak and back down to ZERO at kHeadEnd, so the junction ring is
//     bit-identical with the body part's formula (rx = r*wide/100, rz = r)
//     and the skull eases into the neck over six stations;
//   - both axes carry the swell; the wide-aspect residual (station_wide - 1)
//     fades with e, so at full swell the section is CIRCULAR plus the eye rim;
//   - the eye bulge is the modest googly rim (kEyeBulgeNum), eased over the
//     eye stations exactly as before.
inline void head_ring(int i, int32_t& rx_mm, int32_t& rz_mm) {
  const int32_t r = station_r(i);
  int e = 0;
  if (i >= kBallStation0 && i < kHeadEnd) {
    const int t = i <= kBallPeak
                      ? ((i - kBallStation0) * 1000) / (kBallPeak - kBallStation0)
                      : ((kHeadEnd - i) * 1000) / (kHeadEnd - kBallPeak);
    e = t * t * (3000 - 2 * t) / 1000000;  // integer smoothstep, 0..1000
  }
  const int32_t swollen =
      r + static_cast<int32_t>((static_cast<int64_t>(r) * kBallNum * e) / 1000000);
  int32_t eye_w = 0;
  if (i >= kEyeStation0 && i <= kEyeStation1) {
    const int span = kEyeStation1 - kEyeStation0;
    const int t = span > 0 ? ((i - kEyeStation0) * 1000) / span : 500;
    const int ease = 1000 - (2 * t - 1000) * (2 * t - 1000) / 1000;  // 0..1000..0
    eye_w = static_cast<int32_t>((static_cast<int64_t>(r) * kEyeBulgeNum * ease) / 100000);
  }
  const int32_t wide = station_wide(i);
  rz_mm = swollen;
  rx_mm = static_cast<int32_t>(
      (static_cast<int64_t>(swollen) * (100 + ((wide - 100) * (1000 - e)) / 1000)) / 100 +
      eye_w);
}

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

// The HEAD PART's binding: the skull is rigid on kBHead, eases into the
// spine over kSkullBlendTo, and matches station_bind exactly by the last
// head stations -- so the junction ring at kHeadEnd is bit-identical
// between the head part and the body part and the surfaces meet closed.
inline Bind head_station_bind(int i) {
  if (i <= kSkullRigidTo) return Bind{kBHead, kBHead, 64};
  if (i <= kSkullBlendTo) {
    const Bind sb = station_bind(i);
    const uint8_t spine = sb.w0 >= 32 ? sb.b0 : sb.b1;  // the majority bone
    const int w = 51 - (i - kSkullRigidTo - 1) * 13;    // 51, 38, 25, 12 of 64
    return Bind{kBHead, spine, static_cast<uint8_t>(w)};
  }
  return station_bind(i);
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
  kTileBladePinkUp = 4,  // tail blade: both faces pink, green slice at one edge
  kTileBladeGreenUp = 5  // tail blade: both faces pink, green slice at the other
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

// THE DIRECT-COLOUR PAGE (T1/T2, 2026-08-27). The same painted tiles as
// RGB565 mip chains, sampled through the actual Tmu model: bilinear with
// the frozen weight law, mips, REPEAT around the ring (U — the wrap must
// be seamless), CLAMP along the body (V — the nose row must not bleed
// into the junction row). This is why the surface stops reading as
// pixels: filtering requires direct colour (stars_and_flares 1), and the
// TMU decodes RGB565 today. The CLUT8 page above stays as the
// ordinary-creature format tier and the fallback.
// THE LAYOUT (T4, 2026-08-28): tiles 0..3 all address the ONE 128x256
// BODY ATLAS at byte 0 (head and body parts share it -- their v0/v1 ranges
// split the V axis at the junction row); tiles 4..5 are the fins' own
// 64x64 pages appended after the atlas chain, each with its own mode word
// (per-tile modes are lawful: the TMU mode is per-bind). Bilinear + mips
// BLEED across atlas neighbours, which is exactly why the fins do NOT live
// in the atlas.
inline const zref::DirectPageSet& page_direct() {
  static const zref::DirectPageSet ps = [] {
    zref::DirectPageSet p;
    constexpr int kWords = static_cast<int>(sizeof(kPageDirect[0]) / sizeof(uint16_t));
    p.mem.base = 0;
    const uint32_t atlas_bytes = static_cast<uint32_t>(kPageAtlasWords) * 2;
    p.mem.bytes.resize(atlas_bytes + static_cast<size_t>(2) * kWords * 2);
    for (int i = 0; i < kPageAtlasWords; ++i) {  // little-endian halfwords
      p.mem.bytes[static_cast<size_t>(i) * 2] = static_cast<uint8_t>(kPageAtlas[i] & 0xFF);
      p.mem.bytes[static_cast<size_t>(i) * 2 + 1] = static_cast<uint8_t>(kPageAtlas[i] >> 8);
    }
    for (int t = 0; t < 2; ++t) {  // the two blade tiles (page indices 4, 5)
      const size_t dst = atlas_bytes + static_cast<size_t>(t) * kWords * 2;
      for (int i = 0; i < kWords; ++i) {
        p.mem.bytes[dst + i * 2] = static_cast<uint8_t>(kPageDirect[4 + t][i] & 0xFF);
        p.mem.bytes[dst + i * 2 + 1] = static_cast<uint8_t>(kPageDirect[4 + t][i] >> 8);
      }
    }
    zref::Tmu::Mode ma;  // the atlas
    ma.fmt = zref::Tmu::kRgb565;
    ma.bilinear = true;
    ma.wrap_u = zref::Tmu::kRepeat;  // seamless around the ring
    ma.wrap_v = zref::Tmu::kClamp;   // the nose must not bleed into the fork
    ma.log2w = 7;                    // 128
    ma.log2h = 8;                    // 256 -- LOG2W != LOG2H is legal today
    ma.max_level = 7;
    ma.mip_en = true;
    zref::Tmu::Mode mb = ma;  // the blade pages
    mb.log2w = 6;
    mb.log2h = 6;
    mb.max_level = 6;
    const uint32_t am = ma.pack(), bm = mb.pack();
    p.mode = am;
    p.tile_base = {0, 0, 0, 0, atlas_bytes, atlas_bytes + kWords * 2};
    p.tile_mode = {am, am, am, am, bm, bm};
    return p;
  }();
  return ps;
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
      // THE SKULL BONE: the fixed attitude, plus what the rigid skull no
      // longer inherits from joints 1..2 -- the front wave's first two
      // slope-deltas (the nod the flexible skull used to carry) and two
      // steps of the sway yaw -- so the head's idle life is unchanged
      // while its pitch is owned by kHeadAttitude, not by the hook.
      g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + wave[1] + wave[2] +
                                    (breath * kIdleBreathLift) / 1000),
                             quat_y(2 * ((sh * kIdleHeadSway) >> 16)));
    }

    // 3. the tail plays: a lazy left-right sway on the RAISED tail only.
    // It used to spread over "the rear third" (joints 13+), but the 2026-08-27
    // long grounded run now owns those joints, and a lateral quat on a
    // pitched GROUNDED joint tilts its descent sideways -- the probe caught
    // the belly digging 30 mm in time with the sway. The raised stem starts
    // past kStanceGround1.
    // the grounded run's TORSIONAL breath (see kIdleTorsion): a slow roll
    // wave through the protected middle, phase-lagged behind the breath
    // the accumulated chain rotation up to (not including) the grounded run,
    // for expressing the snake's WORLD-vertical yaw exactly in each joint's
    // frame: L = acc* Y(psi) acc. Approximate per-joint axis corrections
    // leaked first-order pitch into the steep tail rise behind the run
    // (probe: +15 mm hover at half amplitude); the conjugation is exact up
    // to quat16 rounding, so the authored belly sink survives the wave.
    zc::quat16 snacc = zc::quat16_identity();
    for (int j = 0; j < kStanceGround0; ++j)
      snacc = quat_mul(snacc, g.q[kBSpine0 + j]);
    for (int k = kStanceGround0; k <= kStanceGround1; ++k) {
      // THE SIDEWAYS SNAKE first (kIdleSnake*): a slow lateral S travelling
      // the grounded joints -- the idle's dead zone was exactly the part
      // touching the ground, and that is where the travelling motion
      // belongs (the walk's principle at rest). One temporal cycle per
      // loop, one full wavelength across the six joints (zero-sum), yaw
      // about the TRUE world vertical by conjugation.
      snacc = quat_mul(snacc, g.q[kBSpine0 + k]);  // this joint's stance pitch
      const int32_t psn = ph - (k - kStanceGround0) * kIdleSnakeSpatial - 4000;
      const int32_t ss =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(psn & 0xFFFF)}).raw;
      {
        const zc::quat16 W = quat_y((ss * kIdleSnakeAmp) >> 16);
        const zc::quat16 L = quat_mul(quat_mul(quat_conj(snacc), W), snacc);
        g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], L);
        snacc = quat_mul(snacc, L);
      }
      // the TORSION roll composes AFTER the snake: a roll about the
      // (yawed) tube axis moves no nodes
      const int32_t pt = ph - 9000 - (k - kStanceGround0) * 5400;
      const int32_t sr =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(pt & 0xFFFF)}).raw;
      const zc::quat16 tq = quat_x((sr * kIdleTorsion) >> 16);
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], tq);
      snacc = quat_mul(snacc, tq);
    }

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

    // THE SKULL BONE: attitude plus the gait wave's first two slope-deltas
    // (the nod the flexible skull used to inherit from joints 1..2), so the
    // head surges with the gait exactly as approved while its pitch is
    // owned by kHeadAttitude. The visible head-BOB stays the root comp.
    g.q[kBHead] = quat_z(kHeadAttitude + wave[1] + wave[2]);

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
// `choreo`: build the LOCAL-BODY-SHAPE-ONLY variant (C1/C3): the bone-0
// somersault spin and EVERY root channel (fwd, lift, preload rise, coil
// re-pivot) are omitted -- they move to the per-instance ChoreoRoot, which
// is exactly the amendment's split ("shared clips own local body shape; a
// per-instance root transform owns trajectory, spin count, spin plane and
// attack direction"). tools/reel/zixx_choreo.cpp proves the recomposition
// reproduces the golden world-space result. Default false: the shipped
// clip is bit-identical to the approved one.
inline zc::Clip build_attack(bool choreo = false) {
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

  const int nC = kAtkCurlN;
  const int nA = kAtkAuthN;
  const int nS = kAtkSpinN;

  // rolling up: 360 degrees spread over the 18 interior joints
  const int32_t coil_pitch = -(65536 / (kSpineBones - 2));

  for (int f = 0; f < kAttackKeys; ++f) {
    Rig g;
    g.reset();
    const int curl = curve(kAtkCurl, nC, f);
    const int auth = curve(kAtkAuth, nA, f);
    const int spin = curve(kAtkSpin, nS, f);
    const int lift = curve(kAtkLift, kAtkLiftN, f);
    const int fwd = curve(kAtkFwd, kAtkFwdN, f);
    const int pre = curve(kAtkPre, kAtkPreN, f);

    // the anticipation preload deepens the S (same lever as the idle's
    // breath, much harder); the returned rise keeps the belly planted
    // through the compress -- it is ~0 whenever pre is 0.
    const int32_t pre_rise = apply_stance(g, auth, pre);
    // the coil: every interior joint bends the same way, so the body is a
    // wheel; bone 0 is left to the spin alone
    for (int k = 1; k < kSpineBones - 1; ++k) {
      g.q[kBSpine0 + k] =
          quat_mul(g.q[kBSpine0 + k], quat_z((coil_pitch * curl) / 1000));
    }
    // the somersault: the whole body turns on bone 0 -- unless the choreo
    // split owns it (then the ROOT carries this same theta)
    const int32_t theta = static_cast<int32_t>(
        (static_cast<int64_t>(spin) * 65536) / 1000);
    const uint16_t th16 = static_cast<uint16_t>(theta & 0xFFFF);
    if (!choreo) g.q[kBSpine0] = quat_mul(quat_z(theta), g.q[kBSpine0]);

    // re-pivot the spin from the nose to the coil centre, faded with the curl
    const int32_t sth = zref::fx_sin(zref::angle16{th16}).raw;
    const int32_t cth = zref::fx_cos(zref::angle16{th16}).raw;
    const int32_t piv_x = static_cast<int32_t>((static_cast<int64_t>(kCoilR) * sth) >> 16);
    const int32_t piv_y = kCoilR - static_cast<int32_t>((static_cast<int64_t>(kCoilR) * cth) >> 16);

    // THE SKULL BONE. The attitude fades with the S authority -- at the
    // spear (auth 0) the skull lies dead straight on the javelin line, so
    // the head bone cannot bend the weapon. While the body is a wheel, the
    // skull follows the coil's own curvature (one joint's worth of coil
    // pitch approximates the bulb's 1.7-segment arc) instead of chording
    // across it.
    g.q[kBHead] = quat_z((kHeadAttitude * auth) / 1000 + (coil_pitch * curl) / 1000);

    // the blades close to the spear line while coiled or straight-diving,
    // and flare as the S returns
    g.tail_rest((kBladeSplay * auth) / 1000 + kBladeSplay / 5,
                (kBladeRise * auth) / 1000);
    g.write(c, f);
    if (!choreo) {
      c.root[f * 3 + 0] = fxm(fwd + (piv_x * curl) / 1000);
      c.root[f * 3 + 1] = fxm(lift + (piv_y * curl) / 1000 + pre_rise);
    }  // choreo: root channels stay ZERO -- trajectory is the instance's
  }
  c.events = {{kAtkImpactKey, zc::kEvAttack, 0}};  // contact: reel frame 112
  return c;
}

// The per-key ROOT the choreo split hands the instance (the AttackPlan's
// trajectory sample): the same fwd/lift/preload/re-pivot arithmetic the
// monolithic clip used to bake, exposed so the sim (and the proof) compute
// it from the plan instead of the clip. Returns mm; theta in angle16.
struct ChoreoSample {
  int32_t x_mm, y_mm;   // instance translation
  int32_t theta;        // spin angle (angle16) about the world Z
};
inline ChoreoSample attack_choreo_sample(int key) {
  const int curl = curve(kAtkCurl, kAtkCurlN, key);
  const int auth = curve(kAtkAuth, kAtkAuthN, key);
  const int spin = curve(kAtkSpin, kAtkSpinN, key);
  const int lift = curve(kAtkLift, kAtkLiftN, key);
  const int fwd = curve(kAtkFwd, kAtkFwdN, key);
  const int pre = curve(kAtkPre, kAtkPreN, key);
  Rig g;
  g.reset();
  const int32_t pre_rise = apply_stance(g, auth, pre);  // the belly-planting rise
  const int32_t theta = static_cast<int32_t>((static_cast<int64_t>(spin) * 65536) / 1000);
  const uint16_t th16 = static_cast<uint16_t>(theta & 0xFFFF);
  const int32_t sth = zref::fx_sin(zref::angle16{th16}).raw;
  const int32_t cth = zref::fx_cos(zref::angle16{th16}).raw;
  const int32_t piv_x = static_cast<int32_t>((static_cast<int64_t>(kCoilR) * sth) >> 16);
  const int32_t piv_y = kCoilR - static_cast<int32_t>((static_cast<int64_t>(kCoilR) * cth) >> 16);
  ChoreoSample out;
  out.x_mm = fwd + (piv_x * curl) / 1000;
  out.y_mm = lift + (piv_y * curl) / 1000 + pre_rise;
  out.theta = theta;
  // PIVOT CORRECTION: on bone 0 the spin acted about the NOSE at kBodyY;
  // the instance rotation acts about the world origin under it. Re-pivot
  // with the same c - R(c) law the fall clip uses (quat_rot_vec).
  int32_t rx, ry, rz;
  quat_rot_vec(quat_z(theta), 0, kBodyY, 0, rx, ry, rz);
  out.x_mm += 0 - rx;
  out.y_mm += kBodyY - ry;
  return out;
}


// ---- C4: THE ZIXXTRIXX ATTACK PLANNER -------------------------------------
// Builds an AttackPlan from sim truth. The NUMBERS here are Zixxtrixx
// authoring (its jump heights, its spin appetite); the plan record and the
// branch law are sim architecture (zref_creature.hpp). Pure integer
// function of its inputs: replay-exact by construction.
inline zc::AttackPlan zixx_plan_attack(int32_t tgt_x_mm, int32_t tgt_y_mm,
                                       int32_t tgt_vx_mmk, int32_t tgt_vy_mmk) {
  zc::AttackPlan p;
  // the golden preset: a grounded target at the approved strike point takes
  // the approved showcase verbatim
  if (tgt_y_mm <= 0 && tgt_vx_mmk == 0 && tgt_vy_mmk == 0 &&
      tgt_x_mm >= kAtkFwdMax + kAtkTipFwd - 400 &&
      tgt_x_mm <= kAtkFwdMax + kAtkTipFwd + 400) {
    p.preset_golden = true;
    p.apex_mm = kAtkApexLift;
    p.apex_fwd_mm = 1850;
    p.spin_mturns = 3000;
    p.spear_dx_mm = kAtkFwdMax - 1850;
    p.spear_dy_mm = kAtkStickLift - kAtkApexLift;
    p.intercept_x_mm = tgt_x_mm;
    p.intercept_y_mm = tgt_y_mm;
    return p;
  }
  // flight sizing: the coil flight lasts ~1 key per 260 mm of straight-line
  // distance, clamped to a readable band
  const int64_t dx = tgt_x_mm, dy = tgt_y_mm;
  const int32_t dist = static_cast<int32_t>(
      zref::isqrt_u64(static_cast<uint64_t>(dx * dx + dy * dy)));
  int32_t flight = dist / 260;
  if (flight < 18) flight = 18;
  if (flight > 44) flight = 44;
  p.coil_keys = static_cast<uint16_t>(flight);
  // one fixed-point intercept iteration: where the target will be when the
  // spear can reach it (coil + unroll + half the plunge)
  const int32_t lead = flight + p.unroll_keys + p.plunge_keys / 2;
  p.intercept_x_mm = tgt_x_mm + tgt_vx_mmk * lead;
  p.intercept_y_mm = tgt_y_mm + tgt_vy_mmk * lead;
  // the apex: high enough to dive on the intercept -- 2 m above it for an
  // aerial target, the full showcase height for a grounded one, never more
  // than the approved 12 m
  p.apex_mm = p.intercept_y_mm > 0 ? p.intercept_y_mm + 2000 : 8000;
  if (p.apex_mm > kAtkApexLift) p.apex_mm = kAtkApexLift;
  if (p.apex_mm < 3000) p.apex_mm = 3000;
  // forward travel by the apex: a third of the way to the intercept
  p.apex_fwd_mm = p.intercept_x_mm / 3;
  // spin appetite follows the APEX, not the flight length (the amendment:
  // a low/distant target gets FEWER flips and a long horizontal shot; the
  // proof caught the first cut giving the flat lance more turns than the
  // high dive because it flew longer): one somersault per ~3 m of height
  int32_t turns = p.apex_mm / 3000;
  if (turns < 1) turns = 1;
  if (turns > 5) turns = 5;
  p.spin_mturns = turns * 1000;
  // THE COMMITMENT: the spear vector is the line from the commit point
  // (the apex) to the intercept, LOCKED here. Projectile, not missile.
  p.spear_dx_mm = p.intercept_x_mm - p.apex_fwd_mm;
  p.spear_dy_mm = p.intercept_y_mm - p.apex_mm;
  // plunge duration from its length (t^2 law, ~110 mm/key^2 at T=10)
  const int64_t sx = p.spear_dx_mm, sy = p.spear_dy_mm;
  const int32_t slen = static_cast<int32_t>(
      zref::isqrt_u64(static_cast<uint64_t>(sx * sx + sy * sy)));
  int32_t pk = slen / 1100;
  if (pk < 6) pk = 6;
  if (pk > 14) pk = 14;
  p.plunge_keys = static_cast<uint16_t>(pk);
  return p;
}

// the plan's per-key root sample -- the general form of
// attack_choreo_sample. Golden preset: the authored tables verbatim.
inline ChoreoSample zixx_plan_sample(const zc::AttackPlan& p, int key) {
  if (p.preset_golden) return attack_choreo_sample(key);
  ChoreoSample out{0, 0, 0};
  const int t0 = p.compress_keys + p.release_keys;        // launch key
  const int t1 = t0 + p.coil_keys;                        // commit (apex)
  const int t2 = t1 + p.unroll_keys;                      // spear locked
  const int t3 = t2 + p.plunge_keys;                      // impact
  if (key <= t0) {
    // grounded: the local compress/release clips own the shape; the root
    // waits (the preload rise lives in the local clips' own root lane)
    return out;
  }
  if (key <= t1) {
    // the coil flight: lift eases out (1-(1-t)^2), forward linear, the
    // spin is planned_turns * eased flight phase (the amendment's law)
    const int32_t t = ((key - t0) * 1000) / p.coil_keys;
    const int32_t ease = 1000 - ((1000 - t) * (1000 - t)) / 1000;
    out.y_mm = static_cast<int32_t>(
        (static_cast<int64_t>(p.apex_mm) * ease) / 1000);
    out.x_mm = static_cast<int32_t>(
        (static_cast<int64_t>(p.apex_fwd_mm) * t) / 1000);
    const int32_t sm = t * t / 1000 * (3000 - 2 * t) / 1000;  // smoothstep
    out.theta = static_cast<int32_t>(
        (static_cast<int64_t>(p.spin_mturns) * sm / 1000) * 65536 / 1000);
    return out;
  }
  if (key <= t2) {
    // unrolling at the apex: the root hangs; the orientation blends from
    // the last spin angle toward the spear line (the local unroll clip
    // straightens the body; the residual fraction of a turn completes)
    out.x_mm = p.apex_fwd_mm;
    out.y_mm = p.apex_mm;
    const int32_t t = ((key - t1) * 1000) / p.unroll_keys;
    const int32_t whole = (p.spin_mturns / 1000) * 1000;
    const int32_t frac = p.spin_mturns - whole;  // settle any fraction
    out.theta = static_cast<int32_t>(
        ((static_cast<int64_t>(whole) + (static_cast<int64_t>(frac) * t) / 1000) *
         65536) / 1000);
    return out;
  }
  // the plunge: ONE STRAIGHT SHOT along the locked spear vector, t^2
  // acceleration -- and past t3 (a miss) the projectile simply continues
  const int32_t t = ((key - t2) * 1000) / p.plunge_keys;
  const int64_t tt = static_cast<int64_t>(t) * t / 1000;
  out.x_mm = p.apex_fwd_mm + static_cast<int32_t>(
      (static_cast<int64_t>(p.spear_dx_mm) * tt) / 1000);
  out.y_mm = p.apex_mm + static_cast<int32_t>(
      (static_cast<int64_t>(p.spear_dy_mm) * tt) / 1000);
  out.theta = static_cast<int32_t>(
      (static_cast<int64_t>(p.spin_mturns) * 65536) / 1000);
  return out;
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
    // THE S RECURS, IT IS NOT MANDATORY (2026-08-28; was apply_stance at a
    // flat 1000 — "THE S FIRST" made the fall a rigid S-shaped sign with
    // wobble attached). Per-joint authority swings on one slow loop-exact
    // cycle, phase-travelling down the body: nearly straight at one beat,
    // bunched tighter than rest at another. See the kFallAuth knobs.
    {
      int32_t prev = 0;
      for (int k = 0; k < kStanceSlopes; ++k) {
        const int32_t pa = ph - k * kFallAuthSpatial + 21000;
        const int32_t sa =
            zref::fx_sin(zref::angle16{static_cast<uint16_t>(pa & 0xFFFF)}).raw;
        const int32_t auth = kFallAuthMid + ((sa * kFallAuthSwing) >> 16);
        const int32_t d =
            static_cast<int32_t>((static_cast<int64_t>(kStanceSlope[k]) * auth) / 1000);
        const int32_t pitch = d - prev;
        prev = d;
        g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
      }
    }

    // SLOW LOOSE NECK: the head lolls on one- and two-cycle waves, phase
    // staggered down the first joints so the motion travels instead of
    // snapping. Strongest right at the head.
    // Neck loll: the PITCH half runs at 60% since the head-only run -- the
    // skull is a rigid ball on its own bone now, and a full-amp pitch loll
    // folded the hook CLOSED over it: at key 92 the whole head vanished
    // inside the front loop (fall-k92-k126.png in the run folder; the
    // overlap probe measured it 229 mm deep). The LATERAL half keeps its
    // full amplitude -- the wobble the owner asked for is side-to-side.
    for (int k = 1; k <= 4; ++k) {
      const int32_t p1 = ph + k * 7000;
      const int32_t p2 = ph * 2 + 16000 + k * 9000;
      const int32_t s1 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p1 & 0xFFFF)}).raw;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p2 & 0xFFFF)}).raw;
      const int32_t amp = kFallNeckAmp - (k - 1) * 500;
      g.q[kBSpine0 + k] =
          quat_mul(g.q[kBSpine0 + k],
                   quat_mul(quat_z((s1 * (amp * 3 / 5)) >> 16),
                            quat_y((s2 * (amp * 2 / 3)) >> 16)));
    }
    // THE SKULL BONE lolls loosely on top of the attitude: a leading pitch
    // loll at half amp and the lateral yaw at full -- slow one- and
    // two-cycle periods, wobble not jitter.
    {
      const int32_t p1 = ph + 3500;
      const int32_t p2 = ph * 2 + 16000 + 4500;
      const int32_t s1 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p1 & 0xFFFF)}).raw;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p2 & 0xFFFF)}).raw;
      g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + ((s1 * (kFallNeckAmp / 2)) >> 16)),
                             quat_y((s2 * (kFallNeckAmp * 2 / 3)) >> 16));
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
    // The phase is WARPED (kFallTumbleWarp) so the revolution hesitates and
    // then tips over instead of turning like a turntable -- sin(0) = 0 at
    // both ends, so the loop seam is untouched.
    const int32_t theta_u =
        static_cast<int32_t>((static_cast<int64_t>(f) << 16) / kFallKeys);
    const int32_t theta =
        theta_u + static_cast<int32_t>(
                      (static_cast<int64_t>(kFallTumbleWarp) *
                       zref::fx_sin(zref::angle16{static_cast<uint16_t>(theta_u & 0xFFFF)}).raw) >>
                      16);
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

// ---- the NEW VOCABULARY (2026-08-28, Fabian: "It didn't include all the
// sacrifice animations. Hit, death, maybe more idle.") --------------------
// HIT, slot 5: a short readable flinch that starts and ends on the
// canonical S so a hard cut in and out of any clip lands clean.
constexpr int kHitKeys = 18;
constexpr int32_t kHitDeepen = 560;    // recoil: the S snaps tighter...
constexpr int32_t kHitHeadJerk = 5200; // ...and the head jerks back-up
constexpr int32_t kHitSway = 2600;     // and a little away
// DEATH, slot 6: shudder -> the S drains out -> the body keels onto its
// side -> one last tail curl -> stillness. The player watches this one.
constexpr int kDeathKeys = 96;
constexpr int32_t kDeathShudder = 260;   // deepen tremor amplitude (two slow cycles)
constexpr int32_t kDeathAuthEnd = 240;   // how much S is left in the corpse
constexpr int32_t kDeathRoll = -11600;   // ~64 deg: keels onto the flank
                                         // AWAY from the site camera (run
                                         // 0326: +11600 turned the dorsal
                                         // band square at the lens and the
                                         // death read as a magenta smear;
                                         // negative shows flank+belly with
                                         // the pink on the top edge.
                                         // |84| pressed the down-side fin
                                         // 294 mm through the dirt)
constexpr int32_t kDeathPivotX = -990;   // roll pivot: under the S centre...
constexpr int32_t kDeathDropMm = 300;    // ...while the carried front sinks
constexpr int32_t kDeathTailCurl = 7000; // the last slow curl and release
constexpr int32_t kDeathRollLift = 132;   // mm: keeps the rolled tube's centre
                                         // one radius up (h*(1-cos) comp)
// TAIL-BALANCE, slot 7 (owner: "Balancing on its tail and trying to
// stretch up while standing, almost becoming a spear, and then falling
// down. Then getting back up."): gather -> rear up toward vertical
// (ALMOST the attack's spear, wobbling, never achieving it -- the
// contrast is the joke) -> lose it -> topple flat forward -> get back up.
constexpr int kBalKeys = 160;
constexpr int32_t kBalVertSlope = 15600;   // ~86 deg: ALMOST vertical, never 90
constexpr int32_t kBalWobble = 900;        // the balance fight, growing
constexpr int32_t kBalImpactSink = 22;     // mm: the flop's authored bite
constexpr int32_t kBalFootReach = 690;     // blade-foot reach below the fork
constexpr int32_t kBalFootMargin = 98;     // fork height when the tail lies flat
                                           // (tuned on probe + render until the
                                           // tips kiss dirt, no hover, no dig)
constexpr int32_t kBalFinFlare = 2600;     // fins flare wide for balance
// LOOK-AROUND, slot 8: the head-aim rig capability performed — Zixx looks
// left, up, right, down, unhurried, while the body idles quietly. The aim
// lives on the HEAD BONE (the head is bone 0, the ROOT — a joint rotation
// cannot move it; kBHead is the one bone that aims the skull), with the
// first two neck joints following at kLookNeckFollow to soften the turn
// into the tube (origin chosen by eye off renders — a tube has no
// anatomical neck to derive).
constexpr int kLookKeys = 128;
constexpr int32_t kLookYaw = 6200;        // full left/right head turn
constexpr int32_t kLookPitchUp = 4200;    // the upward glance
constexpr int32_t kLookPitchDn = 2600;    // the downward glance
constexpr int32_t kLookNeckFollow = 340;  // 1/1000: how much joints 1-2 follow



// THE CORPSE POSE, shared by the death's drain and the balance's flop: a
// lying body whose centreline DESCENDS nose-to-tail following the TAPER --
// the head's centre rests one head-radius up (~218 mm) while the thin tail
// rests at ~70, so "flat" slopes would bury the skull (the probe caught
// -248 mm exactly there). The sines of the first 17 entries sum to ~0.75:
// ~120 mm of centreline drop across the resting run, matching the radius
// fall from skull to tail stem.
constexpr int32_t kCorpseSlope[kStanceSlopes] = {
    1500, 1300, 1100, 900, 700, 550, 420, 300, 220, 150,
    100,  60,   30,   0,   -30, -60, -90, -100, -200};

// ---- shared by the new clips: the canonical REST rig --------------------
// The pose every interruptible clip starts and ends on: full-authority S,
// resting tail fan, the ratified head attitude. Hard cuts land clean when
// both sides of the cut are this pose.
inline void rest_rig(Rig& g) {
  g.reset();
  apply_stance(g, 1000);
  g.q[kBHead] = quat_z(kHeadAttitude);
  g.tail_rest();
}

// smoothstep in 1/1000 over [a..b] keys
inline int32_t ss1000(int f, int a, int b) {
  if (f <= a) return 0;
  if (f >= b) return 1000;
  const int64_t t = (static_cast<int64_t>(f - a) * 1000) / (b - a);
  return static_cast<int32_t>(t * t * (3000 - 2 * t) / 1000000);
}

// Slot 5 - HIT. A damage flinch: the S snaps tighter, the head jerks back
// and a little aside, one damped overshoot, and the whole thing is back on
// the canonical S inside 0.6 s. Reads at 240p because the motion is the
// whole front lobe (the same deepen lever as the breath), not a limb.
inline zc::Clip build_hit() {
  zc::Clip c;
  c.slot_id = 5;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kHitKeys);
  c.root.assign(static_cast<size_t>(kHitKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kHitKeys) * kBoneCount, zc::quat16_identity());
  // the flinch envelope: sharp rise, damped return with ONE overshoot
  // (wobble not jitter: a single undershoot reads as recoil, a train of
  // them reads as vibration). Keyed by hand:
  static const Key kEnv[] = {{0, 0},   {2, 780},  {4, 1000}, {6, 620},
                             {8, -180}, {10, 60},  {12, -20}, {14, 0},
                             {17, 0}};
  constexpr int kEnvN = static_cast<int>(sizeof(kEnv) / sizeof(Key));
  for (int f = 0; f < kHitKeys; ++f) {
    const int32_t e = curve(kEnv, kEnvN, f);
    Rig g;
    g.reset();
    // the neck's share of the whiplash rides the WAVE lane so
    // apply_stance's exact root compensation keeps the belly planted (the
    // first cut composed raw neck quats and the probe caught the grounded
    // run swinging -67..+174 mm with the flinch)
    int32_t wave[kStanceSlopes] = {};
    wave[1] = -(e * kHitHeadJerk) / 3000;
    wave[2] = -(e * kHitHeadJerk) / 3000;
    const int32_t rise = apply_stance(g, 1000, (e * kHitDeepen) / 1000, wave);
    // the head jerks BACK-UP and aside, then settles with the envelope
    g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + (e * kHitHeadJerk) / 1000),
                           quat_y((e * kHitSway) / 1000));
    g.tail_rest(kBladeSplay + (e * 1400) / 1000, kBladeRise);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  c.events = {{1, zc::kEvFoot, 2}};  // the hit reaction tick, for the sim
  return c;
}

// Slot 6 - DEATH. Shudder; the S drains; the body keels over onto its
// flank about a ground line under the S's centre; one last slow tail curl
// releases; stillness. The final two dozen keys are DELIBERATELY almost
// still -- stillness after motion is what reads as death, and the clip is
// expected to hold its last key.
inline zc::Clip build_death() {
  zc::Clip c;
  c.slot_id = 6;
  c.interpolate = true;
  c.hold_last = true;  // one-shot: the corpse holds; no wrap-to-stance flash
  c.frame_count = static_cast<uint16_t>(kDeathKeys);
  c.root.assign(static_cast<size_t>(kDeathKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kDeathKeys) * kBoneCount, zc::quat16_identity());
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);
  int64_t base_rear_sin = 0;  // stance sum over segments 0..16 (to the rear node)
  for (int k = 0; k <= kStanceGround1; ++k) {
    const uint16_t a = static_cast<uint16_t>(kStanceSlope[k] & 0xFFFF);
    base_rear_sin += zref::fx_sin(zref::angle16{a}).raw;
  }
  for (int f = 0; f < kDeathKeys; ++f) {
    Rig g;
    g.reset();
    // phase 1, k0..14: the shudder -- two slow tremors, the head shaking
    // NO (small, slow; a fast tremor would read as electricity)
    int32_t headshake = 0;
    int32_t shud = 0;
    if (f <= 14) {
      const int32_t ph = f * 9362;  // two cycles over 14 keys
      const int32_t sh = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
      shud = (sh * kDeathShudder) >> 16;
      headshake = (sh * 2200) >> 16;
    }
    // phase 2, k14..48: the collapse -- every slope drains toward the
    // near-flat dead pose while the computed root keeps the REAR node
    // planted, so the carried front comes DOWN to the dirt instead of the
    // rear floating up
    const int32_t drain = ss1000(f, 14, 48);
    int64_t rear_sin = 0;
    int32_t prev = 0;
    for (int k = 0; k < kStanceSlopes; ++k) {
      int64_t d = kStanceSlope[k];
      if (k >= kStanceDescend0 && k <= kStanceDescend1 && shud != 0) {
        if (d <= 16384) d += (d * shud) / 1000;
        else d -= ((32768 - d) * shud) / 1000;
      }
      d += ((kCorpseSlope[k] - d) * drain) / 1000;
      // phase 4, k52..78: the last tail curl, risen slowly, released slower
      if (k > kStanceGround1) {
        const int32_t curl_env = ss1000(f, 52, 62) - ss1000(f, 62, 78);
        // negative: the curl lifts the tail UP off the dirt and lets it
        // sink back (positive dug it under)
        d -= (kDeathTailCurl * curl_env) / (1000 * (kSpineBones - kStanceGround1));
      }
      const uint16_t a = static_cast<uint16_t>(d & 0xFFFF);
      if (k <= kStanceGround1) rear_sin += zref::fx_sin(zref::angle16{a}).raw;
      const int32_t pitch = static_cast<int32_t>(d) - prev;
      prev = static_cast<int32_t>(d);
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
    }
    // the rear-node constraint: the node at the end of the grounded run
    // keeps its stance height whatever the slopes do
    const int32_t root_y =
        static_cast<int32_t>((segL * (rear_sin - base_rear_sin)) >> 16);
    // phase 3, k30..62: keels onto the flank -- a roll about the world
    // forward axis through a ground line under the S's centre (the fall
    // clip's re-pivot law, one axis), applied to the now-low body
    const int32_t roll = (kDeathRoll * ss1000(f, 30, 62)) / 1000;
    // a rolling tube's centre stays one radius up: the rigid roll about
    // the fixed ground axis drops it by h(1-cos), so the root rises to
    // match (the probe read the front quarter -175 without it)
    const int32_t roll_lift = (kDeathRollLift * ss1000(f, 30, 62)) / 1000;
    // the head: the attitude HOLDS (draining it to zero pitched the
    // 218 mm ball 60 deg under and the probe read -226); the dying droop
    // is a small authored delta, and the last shake rides on top
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude - (1400 * drain) / 1000 + headshake / 2),
        quat_y(headshake));
    // fins fold in death, slowly
    g.tail_rest((kBladeSplay * (1000 - drain / 2)) / 1000,
                (kBladeRise * (1000 - drain)) / 1000);
    const zc::quat16 rq = quat_x(roll);
    g.q[kBSpine0] = quat_mul(rq, g.q[kBSpine0]);
    g.write(c, f);
    int32_t rx, ry, rz;
    // the roll pivots about the ground line under the S centre: relative
    // to bone 0 that is (x irrelevant for a roll about X, -kBodyY-root_y)
    quat_rot_vec(rq, kDeathPivotX, -kBodyY - root_y, 0, rx, ry, rz);
    c.root[f * 3 + 0] = fxm(kDeathPivotX - rx);
    c.root[f * 3 + 1] = fxm(root_y + roll_lift + (-kBodyY - root_y - ry));
    c.root[f * 3 + 2] = fxm(-rz);
  }
  c.events = {{44, zc::kEvFoot, 3}};  // the body hits its flank
  return c;
}

// Slot 7 - TAIL-BALANCE, the idle stunt. Gather onto the tail, rear up
// toward vertical -- ALMOST the attack's rigid spear, but effortful and
// wobbling, never straight -- lose the fight, topple flat forward with an
// authored ground bite, and get back up into the canonical S. The root is
// COMPUTED from the tail-tip ground constraint the whole way up and over,
// so the stunt pivots on the planted tail like a real balance, not a
// root curve pretending to be one.
inline zc::Clip build_balance() {
  zc::Clip c;
  c.slot_id = 7;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kBalKeys);
  c.root.assign(static_cast<size_t>(kBalKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kBalKeys) * kBoneCount, zc::quat16_identity());
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);
  // the stance's own sums, for the root constraint deltas
  int64_t base_sin = 0, base_cos = 0;
  for (int k = 0; k < kStanceSlopes; ++k) {
    const uint16_t a = static_cast<uint16_t>(kStanceSlope[k] & 0xFFFF);
    base_sin += zref::fx_sin(zref::angle16{a}).raw;
    base_cos += zref::fx_cos(zref::angle16{a}).raw;
  }
  for (int f = 0; f < kBalKeys; ++f) {
    Rig g;
    g.reset();
    // the choreography, phase by phase, all in one blend weight table:
    //   gather   k0..20    weight into the rear, tail curls under
    //   rise     k20..55   slopes -> kBalVertSlope (almost vertical)
    //   balance  k55..85   held, wobble GROWS (the fight)
    //   lose     k85..100  wobble diverges into a lean
    //   topple   k100..112 slopes -> flat, accelerating; IMPACT at 112
    //   rise2    k118..140 back up into the S
    //   settle   k140..159 exact canonical S for the loop
    const int32_t up = ss1000(f, 20, 55);           // stance -> vertical
    const int32_t back1 = ss1000(f, 100, 112);      // vertical -> flat...
    const int32_t over = (back1 * back1) / 1000;    // ...ACCELERATING (gravity)
    const int32_t recover = ss1000(f, 118, 140);    // flat -> stance
    // the wobble: grows through the balance, spills into the loss
    const int32_t fight = ss1000(f, 55, 95);
    const int32_t ph = f * (65536 / 24);  // ~one wobble cycle per 0.8 s
    const int32_t wob =
        (zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw *
         ((kBalWobble * fight) / 1000)) >> 16;
    int64_t sum_sin = 0, sum_cos = 0;
    int32_t prev = 0;
    for (int k = 0; k < kStanceSlopes; ++k) {
      // per-joint blend: stance -> almost-vertical -> flat -> stance
      int64_t d = kStanceSlope[k];
      // the rise leaves the last two segments (the planted foot) steeper
      // late, so the animal visibly takes its weight on the tail
      const int32_t vert = k >= kStanceSlopes - 2 ? kBalVertSlope + 500
                                                  : kBalVertSlope;
      d += ((vert - d) * up) / 1000;
      // the topple lands in the CORPSE pose, not a flat line -- a lying
      // body's centreline follows the taper (see kCorpseSlope)
      d += ((kCorpseSlope[k] - d) * over) / 1000;
      d += ((kStanceSlope[k] - d) * recover) / 1000;  // and the S returns
      // the fight -- gated OUT by the topple: once falling the fight is
      // lost, and a full-amplitude wobble on the flat pose pivoted the
      // whole plank +-5 deg about the fork (+-233 mm at the nose; probe)
      if (f >= 55 && f < 118 && k < kStanceSlopes - 2)
        d += (wob * (1000 - over)) / 1000;
      const uint16_t a = static_cast<uint16_t>(d & 0xFFFF);
      sum_sin += zref::fx_sin(zref::angle16{a}).raw;
      sum_cos += zref::fx_cos(zref::angle16{a}).raw;
      const int32_t pitch = static_cast<int32_t>(d) - prev;
      prev = static_cast<int32_t>(d);
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
    }
    // ROOT: the fork's world HEIGHT is authored as a hand-keyed delta
    // curve (the attack's lift-curve method -- three generated foot models
    // in a row each failed a different phase on the probe, so the height
    // is now owned by a knob per phase and the constraint just enforces
    // it exactly): fork = its stance height + kBalFork(f), whatever the
    // slopes and the wobble do. 0 at both ends makes the loop exact; the
    // correction absorbing the wobble is what keeps the foot PLANTED
    // through the balance fight.
    static const Key kBalFork[] = {
        {0, 0},      {20, 0},     {32, -175}, {40, 120},  {47, 420},
        {55, 480},   {100, 480},  {104, 420}, {108, 120},
        {112, -300}, {118, -330}, {123, -140}, {130, 30}, {140, 0},  {159, 0}};
    constexpr int kBalForkN = static_cast<int>(sizeof(kBalFork) / sizeof(Key));
    const int32_t base_fork =
        kBodyY - static_cast<int32_t>((segL * base_sin) >> 16);
    const int32_t fork_y0 =
        kBodyY - static_cast<int32_t>((segL * sum_sin) >> 16);
    const int32_t stunt = f < 118 ? ss1000(f, 20, 50) : 1000 - recover;
    const int32_t dx = static_cast<int32_t>((segL * (sum_cos - base_cos)) >> 16);
    int32_t root_x = -(dx * stunt) / 1000;
    int32_t root_y = base_fork + curve(kBalFork, kBalForkN, f) - fork_y0;
    // the IMPACT: the front third slaps the dirt -- a brief authored bite
    // (kBalImpactSink) that releases over four keys. Declared, probed.
    if (f >= 112 && f < 120) {
      root_y -= (kBalImpactSink * (1000 - ss1000(f, 114, 119))) / 1000;
    }
    // the head: tucks slightly with the effort on the way up, jams
    // nose-first at the flop, recovers with the S
    g.q[kBHead] = quat_z(kHeadAttitude + (up * -2200) / 1000 + (over * 1800) / 1000
                         - (recover * -400) / 1000);
    // fins: they ARE the foot -- they stay pressed along the tail line
    // while the animal stands on them (flaring them mid-stand lifted the
    // whole support 373 mm off the dirt; the probe caught it), with only a
    // small strain flare during the fight, and they slap flat at the flop
    g.tail_rest(kBladeSplay + (kBalFinFlare * fight) / 3000 - (600 * over) / 1000,
                kBladeRise - (kBladeRise * up) / 1400 - (600 * over) / 1000);
    g.write(c, f);
    c.root[f * 3 + 0] = fxm(root_x);
    c.root[f * 3 + 1] = fxm(root_y);
  }
  c.events = {{112, zc::kEvFoot, 4}};  // the flop lands
  return c;
}

// Slot 8 - LOOK-AROUND, the head-aim rig performed. The body idles
// quietly (half-amplitude breath, no front wave -- the head is the
// subject); the head turns LEFT, glances UP, turns RIGHT, dips DOWN and
// comes home, each move eased with a tiny arrival overshoot, the first
// two neck joints following softly so the turn reads "at the neck" on a
// creature that has none. THE ROOT TRAP, honoured: the head is bone 0,
// so all of this lives on kBHead -- verified by rendering a turn with
// the body visibly planted.
inline zc::Clip build_look() {
  zc::Clip c;
  c.slot_id = 8;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kLookKeys);
  c.root.assign(static_cast<size_t>(kLookKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kLookKeys) * kBoneCount, zc::quat16_identity());
  // the gaze itinerary: {key, yaw, pitch} targets, eased between with a
  // 6% arrival overshoot. Slow and curious; the holds are where it lives.
  struct Aim { int f; int32_t yaw, pitch; };
  static const Aim kAim[] = {
      {0, 0, 0},        {10, 0, 0},
      {22, kLookYaw, 400},   {40, kLookYaw, 0},          // left, hold
      {54, 2000, kLookPitchUp}, {68, 2400, kLookPitchUp},  // up, hold
      {82, -kLookYaw, 600},  {98, -kLookYaw, 0},         // right, hold
      {108, -800, -kLookPitchDn}, {114, 0, -kLookPitchDn / 2},  // down
      {122, 0, 0},      {127, 0, 0}};
  constexpr int kAimN = static_cast<int>(sizeof(kAim) / sizeof(Aim));
  const auto aim_at = [&](int f, int32_t& yaw, int32_t& pitch) {
    yaw = kAim[kAimN - 1].yaw;
    pitch = kAim[kAimN - 1].pitch;
    for (int i = 0; i + 1 < kAimN; ++i) {
      if (f >= kAim[i].f && f <= kAim[i + 1].f) {
        const int span = kAim[i + 1].f - kAim[i].f;
        int32_t t = span > 0 ? ((f - kAim[i].f) * 1000) / span : 1000;
        t = t * t * (3000 - 2 * t) / 1000000;  // ease
        t = t + (t * (1000 - t) / 1000) * 60 / 1000;  // ~6% arrival overshoot
        yaw = kAim[i].yaw + ((kAim[i + 1].yaw - kAim[i].yaw) * t) / 1000;
        pitch = kAim[i].pitch + ((kAim[i + 1].pitch - kAim[i].pitch) * t) / 1000;
        return;
      }
    }
  };
  const int32_t per_key = 65536 / kLookKeys;
  for (int f = 0; f < kLookKeys; ++f) {
    const int32_t ph = f * per_key;
    const int32_t s = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
    Rig g;
    g.reset();
    // a quiet half-breath keeps the body alive under the performance.
    // The neck's PITCH share of the follow rides the wave lane so the
    // root compensation keeps the belly planted; the YAW share is applied
    // about the TRUE WORLD VERTICAL by conjugation (the snake's trick) --
    // raw neck quats on the steep hook dug the belly -153 and hovered +33.
    int32_t lyaw, lpitch;
    aim_at(f >= 2 ? f - 2 : 0, lyaw, lpitch);
    int32_t wave[kStanceSlopes] = {};
    wave[1] = (lpitch * kLookNeckFollow) / 2000;
    wave[2] = (lpitch * kLookNeckFollow) / 2000;
    const int32_t breath = ((s + 65536) * 250) >> 16;
    const int32_t rise =
        apply_stance(g, 1000, (breath * kIdleDeepen) / 1000, wave);
    int32_t yaw, pitch;
    aim_at(f, yaw, pitch);
    g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + pitch), quat_y(yaw));
    {
      zc::quat16 acc = g.q[kBSpine0 + 0];
      for (int k = 1; k <= 2; ++k) {
        acc = quat_mul(acc, g.q[kBSpine0 + k]);
        const zc::quat16 W = quat_y((lyaw * kLookNeckFollow) / 2000);
        const zc::quat16 L = quat_mul(quat_mul(quat_conj(acc), W), acc);
        g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], L);
        acc = quat_mul(acc, L);
      }
    }
    g.tail_rest(kBladeSplay + ((s * 500) >> 16), kBladeRise);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}


// ---- C2: THE PHASE CLIPS (2026-08-28) -------------------------------------
// The programmable-salto architecture's shared local-body vocabulary
// (amendment: "Shared clips own local body shape ... A per-instance full-3D
// root transform owns trajectory, spin count, spin plane and attack
// direction"). Sliced from the LOCAL-BODY-ONLY attack (build_attack(true))
// at existing keys, so every declared seam is bit-identical BY
// CONSTRUCTION -- and compile_creature now enforces it (ClipBank::seams).
// Slots 10..17 of the 64 vocabulary.
enum : uint16_t {
  kSlotAtkCompress = 10,  // settle + compress + hold (keys 0..9)
  kSlotAtkRelease = 11,   // preload releases, rolls to the coil (9..18)
  kSlotAtkCoil = 12,      // the wheel, looping (18..19; spin is the ROOT's)
  kSlotAtkUnroll = 13,    // coil -> rigid spear (40..47)
  kSlotAtkSpearFlex = 14, // NEW: elastic flex wave on the held spear
  kSlotAtkStick = 15,     // the planted spear, looping (62..63)
  kSlotAtkAirHit = 16,    // NEW: mid-air impact recoil, spear to spear
  kSlotAtkRecover = 17    // spear -> the canonical S (212..225)
};

inline zc::Clip slice_clip(const zc::Clip& src, uint16_t slot, int k0, int k1) {
  zc::Clip c;
  c.slot_id = slot;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(k1 - k0 + 1);
  c.root.assign(src.root.begin() + static_cast<size_t>(k0) * 3,
                src.root.begin() + (static_cast<size_t>(k1) + 1) * 3);
  c.quats.assign(src.quats.begin() + static_cast<size_t>(k0) * kBoneCount,
                 src.quats.begin() + (static_cast<size_t>(k1) + 1) * kBoneCount);
  return c;
}

// the straight-spear local rig every flex/hit phase starts and ends on:
// EXACTLY the attack's key-47/key-62 local pose (auth 0, curl 0)
inline void spear_rig(Rig& g) {
  g.reset();
  g.q[kBHead] = quat_z(0);
  g.tail_rest(kBladeSplay / 5, 0);
}

// SPEAR FLEX (slot 14): while embedded (or held), the otherwise straight
// spear carries one damped elastic bow -- a compression wave travelling
// tail -> head, mean axis straight, ENDS EXACTLY straight (the envelope is
// integer k*(n-1-k), zero at both ends by construction, so the seams to
// the stick/unroll phases are bit-identical).
constexpr int kFlexKeys = 10;
constexpr int32_t kFlexAmp = 1500;
inline zc::Clip build_spear_flex() {
  zc::Clip c;
  c.slot_id = kSlotAtkSpearFlex;
  c.interpolate = true;
  c.frame_count = kFlexKeys;
  c.root.assign(static_cast<size_t>(kFlexKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kFlexKeys) * kBoneCount, zc::quat16_identity());
  for (int f = 0; f < kFlexKeys; ++f) {
    Rig g;
    spear_rig(g);
    const int env = f * (kFlexKeys - 1 - f);  // 0 at both ends, integer
    const int envmax = (kFlexKeys - 1) * (kFlexKeys - 1) / 4;
    for (int k = 1; k < kSpineBones - 1; ++k) {
      const int32_t ph = f * 9000 - k * 5500 + 20000;
      const int32_t sw =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k],
          quat_z(static_cast<int32_t>(
              (static_cast<int64_t>(sw) * kFlexAmp * env / envmax) >> 16)));
    }
    g.write(c, f);
  }
  return c;
}

// AIR HIT (slot 16): the mid-air impact recoil -- a hard bend against the
// travel with a two-lobe ring-down, spear to spear (the recovery phase is
// where the S returns; this is just the blow landing).
constexpr int kAirHitKeys = 12;
constexpr int32_t kAirHitAmp = 3600;
inline zc::Clip build_air_hit() {
  zc::Clip c;
  c.slot_id = kSlotAtkAirHit;
  c.interpolate = true;
  c.frame_count = kAirHitKeys;
  c.root.assign(static_cast<size_t>(kAirHitKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kAirHitKeys) * kBoneCount, zc::quat16_identity());
  // envelope: sharp rise, damped alternation, EXACT zero at both ends
  static const Key kEnv[] = {{0, 0},  {1, 700}, {2, 1000}, {4, 250},
                             {6, -350}, {8, 120}, {10, -40}, {11, 0}};
  constexpr int kEnvN = static_cast<int>(sizeof(kEnv) / sizeof(Key));
  for (int f = 0; f < kAirHitKeys; ++f) {
    Rig g;
    spear_rig(g);
    const int32_t e = curve(kEnv, kEnvN, f);
    for (int k = 1; k < kSpineBones - 1; ++k) {
      // strongest at the impact end (the tail tip is the weapon), fading
      // toward the head, one soft spatial arc rather than a zigzag
      const int env_k = 1000 - (k * 700) / (kSpineBones - 2);
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k], quat_z((e * kAirHitAmp * env_k) / (1000 * 1000) / 8));
    }
    g.write(c, f);
  }
  return c;
}


// ---- F2: THE OFFLINE SPRING-CHAIN BAKER (2026-08-28) ----------------------
// The lasting falling architecture: per-joint pitch/yaw driven by a
// deterministic fixed-point spring chain -- spring to a weak rest S,
// neighbour bend coupling, root angular inertia, slow aero forcing, minus
// damping -- run OFFLINE at 60 Hz and BAKED to poses. No runtime physics:
// the shipped bytes are keys, replay-exact by construction.
//
// SHIPPING STATUS: the baked result lives on SLOT 19 as an ALTERNATE fall
// beside the owner-directed F1 relax on slot 4 (Fabian's "relax by a ton"
// landed today and is the shipped look; promoting the baked chain over it
// is one line, after his eye rules). House style holds either way: the
// chain's terms are FEW and SLOW.
constexpr int kFallBakeWarmLoops = 4;   // settle into the periodic orbit
constexpr int kFallBakeBlend = 16;      // keys of tail->head closure fade
// region dynamics, indexed head..tail in 4 bands: heavy slow head, loose
// neck, heavier middle, light delayed tail (masses as inverse-acc scale)
constexpr int32_t kFbSpring[4] = {150, 210, 170, 250}; // pull to the rest S
constexpr int32_t kFbCouple[4] = {160, 220, 190, 260}; // neighbour bend
constexpr int32_t kFbDamp[4] = {120, 104, 112, 86};    // per-mille per step
constexpr int32_t kFbInertia[4] = {150, 100, 60, 120}; // root spin coupling
constexpr int32_t kFbAero[4] = {500, 700, 400, 1100};  // slow flutter drive
constexpr int32_t kFbRestAuth = 350;    // the weak rest S, 1/1000
inline int fb_region(int j) {
  if (j <= 3) return 0;
  if (j <= 8) return 1;
  if (j <= 14) return 2;
  return 3;
}

inline zc::Clip build_fall_baked() {
  zc::Clip c;
  c.slot_id = 19;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kFallKeys);
  c.root.assign(static_cast<size_t>(kFallKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kFallKeys) * kBoneCount, zc::quat16_identity());
  const int nj = kStanceSlopes;
  // state: per joint pitch/yaw (angle16) and velocity (angle16 per step)
  std::vector<int32_t> th_p(nj, 0), om_p(nj, 0), th_y(nj, 0), om_y(nj, 0);
  // recorded pitch/yaw per key of the LAST loop
  std::vector<int32_t> rec_p(static_cast<size_t>(kFallKeys) * nj, 0);
  std::vector<int32_t> rec_y(static_cast<size_t>(kFallKeys) * nj, 0);
  const int steps_per_loop = kFallKeys * 2;  // 60 Hz, keys are 30 Hz
  int32_t prev_theta = 0, prev_om_root = 0;
  for (int loop = 0; loop <= kFallBakeWarmLoops; ++loop) {
    for (int st = 0; st < steps_per_loop; ++st) {
      const int key = st / 2;
      const int32_t ph = key * (65536 / kFallKeys) + (st & 1) * (32768 / kFallKeys);
      // the tumble's warped phase (build_fall's own law) -> root angular
      // velocity -> its DERIVATIVE is the inertia forcing
      const int32_t theta_u = static_cast<int32_t>((static_cast<int64_t>(st) << 16) / steps_per_loop);
      const int32_t warp = static_cast<int32_t>(
          (static_cast<int64_t>(kFallTumbleWarp) *
           zref::fx_sin(zref::angle16{static_cast<uint16_t>(theta_u & 0xFFFF)}).raw) >> 16);
      const int32_t theta = theta_u + warp;
      const int32_t om_root = theta - prev_theta;
      const int32_t al_root = om_root - prev_om_root;  // root angular accel
      prev_theta = theta;
      prev_om_root = om_root;
      for (int j = 0; j < nj; ++j) {
        const int r = fb_region(j);
        // the weak rest S (region-scaled stance slope delta from straight)
        const int32_t rest =
            static_cast<int32_t>((static_cast<int64_t>(kStanceSlope[j]) * kFbRestAuth) / 1000) -
            (j > 0 ? static_cast<int32_t>(
                         (static_cast<int64_t>(kStanceSlope[j - 1]) * kFbRestAuth) / 1000)
                   : 0);
        const int32_t left_p = j > 0 ? th_p[j - 1] : 0;
        const int32_t right_p = j + 1 < nj ? th_p[j + 1] : 0;
        const int32_t left_y = j > 0 ? th_y[j - 1] : 0;
        const int32_t right_y = j + 1 < nj ? th_y[j + 1] : 0;
        // slow aero flutter: ONE incommensurate slow wave per axis (house
        // style: fewer and slower, never a noise bank)
        const int32_t aero_p = static_cast<int32_t>(
            (static_cast<int64_t>(kFbAero[r]) *
             zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph - j * 4200 + 11000) & 0xFFFF)}).raw) >> 16);
        const int32_t aero_y = static_cast<int32_t>(
            (static_cast<int64_t>(kFbAero[r]) *
             zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2 - j * 3600 + 30000) & 0xFFFF)}).raw) >> 17);
        int64_t acc_p = (static_cast<int64_t>(kFbSpring[r]) * (rest - th_p[j])) / 1000 +
                        (static_cast<int64_t>(kFbCouple[r]) * (left_p + right_p - 2 * th_p[j])) / 1000 +
                        (static_cast<int64_t>(kFbInertia[r]) * al_root) / 100 + aero_p / 8;
        int64_t acc_y = (static_cast<int64_t>(kFbSpring[r]) * (0 - th_y[j])) / 1000 +
                        (static_cast<int64_t>(kFbCouple[r]) * (left_y + right_y - 2 * th_y[j])) / 1000 +
                        aero_y / 8;
        om_p[j] += static_cast<int32_t>(acc_p);
        om_y[j] += static_cast<int32_t>(acc_y);
        om_p[j] -= static_cast<int32_t>((static_cast<int64_t>(kFbDamp[r]) * om_p[j]) / 1000);
        om_y[j] -= static_cast<int32_t>((static_cast<int64_t>(kFbDamp[r]) * om_y[j]) / 1000);
        th_p[j] += om_p[j];
        th_y[j] += om_y[j];
        // hard sanity clamp so a mis-tuned knob cannot explode the bake
        if (th_p[j] > 9000) th_p[j] = 9000;
        if (th_p[j] < -9000) th_p[j] = -9000;
        if (th_y[j] > 9000) th_y[j] = 9000;
        if (th_y[j] < -9000) th_y[j] = -9000;
      }
      if (loop == kFallBakeWarmLoops && (st & 1) == 0) {
        for (int j = 0; j < nj; ++j) {
          rec_p[static_cast<size_t>(key) * nj + j] = th_p[j];
          rec_y[static_cast<size_t>(key) * nj + j] = th_y[j];
        }
      }
    }
  }
  // exact loop closure: cross-fade the recorded tail into the head
  for (int k = 0; k < kFallBakeBlend; ++k) {
    const int kk = kFallKeys - kFallBakeBlend + k;
    const int32_t w = ((k + 1) * 1000) / (kFallBakeBlend + 1);
    for (int j = 0; j < nj; ++j) {
      const int32_t hp = rec_p[static_cast<size_t>(0) * nj + j];
      const int32_t hy = rec_y[static_cast<size_t>(0) * nj + j];
      int32_t& tp = rec_p[static_cast<size_t>(kk) * nj + j];
      int32_t& ty = rec_y[static_cast<size_t>(kk) * nj + j];
      tp += ((hp - tp) * w) / 1000;
      ty += ((hy - ty) * w) / 1000;
    }
  }
  // pose the loop: baked chain angles + the F1 tumble root (unchanged law)
  for (int f = 0; f < kFallKeys; ++f) {
    Rig g;
    g.reset();
    int32_t prev = 0;
    for (int j = 0; j < nj; ++j) {
      const int32_t d = rec_p[static_cast<size_t>(f) * nj + j] + prev;  // absolute-ish
      const int32_t pitch = rec_p[static_cast<size_t>(f) * nj + j];
      g.q[kBSpine0 + j] = quat_mul(quat_z(pitch),
                                   quat_y(rec_y[static_cast<size_t>(f) * nj + j]));
      (void)d;
      prev = 0;
    }
    // the head lolls with the first joints' baked motion, on its bone
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude + rec_p[static_cast<size_t>(f) * nj + 0]),
        quat_y(rec_y[static_cast<size_t>(f) * nj + 0] * 2));
    const int32_t fl = zref::fx_sin(
        zref::angle16{static_cast<uint16_t>((f * (65536 / kFallKeys) * 2 + 9000) & 0xFFFF)}).raw;
    g.tail_rest(kBladeSplay + ((fl * 1800) >> 16), kBladeRise + ((fl * 1000) >> 16));
    // the tumble root, exactly build_fall's law
    const int32_t theta_u = static_cast<int32_t>((static_cast<int64_t>(f) << 16) / kFallKeys);
    const int32_t theta = theta_u + static_cast<int32_t>(
        (static_cast<int64_t>(kFallTumbleWarp) *
         zref::fx_sin(zref::angle16{static_cast<uint16_t>(theta_u & 0xFFFF)}).raw) >> 16);
    const int32_t ph = f * (65536 / kFallKeys);
    const int32_t t2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph + 17000) & 0xFFFF)}).raw;
    const int32_t t3 = zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2 + 40000) & 0xFFFF)}).raw;
    const zc::quat16 tumble =
        quat_mul(quat_z(theta), quat_mul(quat_x((t2 * kFallRollAmp) >> 16),
                                         quat_y((t3 * kFallYawAmp) >> 16)));
    g.q[kBSpine0] = quat_mul(tumble, g.q[kBSpine0]);
    g.write(c, f);
    int32_t rx, ry, rz;
    quat_rot_vec(tumble, kFallPivotX, kFallPivotY, 0, rx, ry, rz);
    c.root[f * 3 + 0] = fxm(kFallPivotX - rx);
    c.root[f * 3 + 1] = fxm(kFallLift + kFallPivotY - ry);
    c.root[f * 3 + 2] = fxm(-rz);
  }
  return c;
}

#ifdef ZIXX_SWEEP
#ifndef ZIXX_SWEEP_BASE
#define ZIXX_SWEEP_BASE (-8000)
#endif
// DIAGNOSTIC ONLY (compiled solely by the -DZIXX_SWEEP build, never shipped):
// the head-attitude ORIENTATION SWEEP. Nine keys of the plain stance, each
// with a different skull attitude, -8000..+8000 in steps of 2000 -- rendered
// from one fixed side camera and judged on one contact sheet, per
// Headache.md: "ten minutes of brute-force visual evidence".
inline zc::Clip build_sweep() {
  zc::Clip c;
  c.slot_id = 9;  // after the 2026-08-28 vocabulary (5..8)
  c.interpolate = false;
  c.frame_count = 9;
  c.root.assign(9 * 3, 0);
  c.quats.assign(9 * static_cast<size_t>(kBoneCount), zc::quat16_identity());
  // NOTE the sweep's own first lesson, kept for the record: POSITIVE
  // attitude pitches the nose DOWN in the composed skeleton -- the axis
  // convention every slope comment assumed was inverted, which is exactly
  // why pass 3's "+4000 = 22 deg of nose lift" rendered as droop.
  for (int f = 0; f < 9; ++f) {
    Rig g;
    g.reset();
    apply_stance(g, 1000);
    g.q[kBHead] = quat_z(ZIXX_SWEEP_BASE + f * 2000);
    g.tail_rest();
    g.write(c, f);
  }
  return c;
}
#endif

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
    // the skull bone: child of the root, PIVOT AT THE SKULL'S OWN CENTRE
    // (2026-08-28; was at the nose). Pitching about the nose swung the
    // skull's REAR down into the hook -- the owner's look-up attitude dug
    // the cranium ~235 mm into the dive stroke (probe). About the centroid
    // (~station 3.5) the same axis angle lifts the nose and drops the rear
    // half as much each, so the culminating head rides the hook the way
    // Side.png nests it. All of the cranium's pitch lives on this bone
    // (kHeadAttitude plus per-clip head motion).
    sk.bones[kBHead] = zc::Bone{kBSpine0, -fxm(kHeadPivotMm), 0, 0};
    std::vector<zc::RingPart> parts;

    // ---- THE HEAD: the skull surface itself, stations 0..kHeadEnd --------
    // NOT an overlay any more (2026-08-27 head-only run). This part IS the
    // front of the animal: the nose dome, the cranium with the eye bulges,
    // the throat -- bound to the dedicated skull bone and easing into the
    // spine. The body part below begins at the same station with the same
    // ring spec, so the junction is vertex-coincident and closed.
    {
      zc::RingPart p;
      p.chain = true;
      p.pitch_q = 1;
      p.yaw_q = 3;
      p.caps = zc::kCapBot;
      for (int i = 0; i <= kHeadEnd; ++i) {
        // THE BALL (head_ring): every axis swells toward the mid-eye peak
        // and eases back to the body formula at the junction ring, with the
        // googly eye rim riding on top. See head_ring for the law.
        int32_t rx_mm, rz_mm;
        head_ring(i, rx_mm, rz_mm);
        const Bind bd = head_station_bind(i);
        zc::RingSpec rs;
        rs.y = fxm(station_x(i));
        rs.radius = fxm(station_r(i));
        rs.segments = static_cast<uint8_t>(station_sides(i));
        rs.b0 = bd.b0;
        rs.b1 = bd.b1;
        rs.w0 = bd.w0;
        rs.rx = fxm(rx_mm);    // LATERAL, ball + the eye rim
        rs.rz = fxm(rz_mm);    // VERTICAL, ball
        rs.cz = -fxm(kBodyY);  // chain rings are creature-global; UP is -cz
        p.rings.push_back(rs);
      }
      p.page = kTileHead;
      // T4: the head part owns atlas V rows 0..50 (nose to the junction
      // station); the body part continues at 50 with the SAME junction V,
      // so the painted surface is continuous across the shared ring.
      p.v0 = 0;
      p.v1 = 50;
      set_rgb(p, kBlue);
      parts.push_back(p);
    }

    // ---- THE BODY: one chain part, junction station to fork --------------
    {
      zc::RingPart p;
      p.chain = true;
      p.pitch_q = 1;
      p.yaw_q = 3;
      p.caps = zc::kCapTop;  // the fork cap; the front closes against the head
      for (int i = kHeadEnd; i < kProfileStations; ++i) {
        const int32_t r = station_r(i);
        const Bind bd = station_bind(i);
        zc::RingSpec rs;
        rs.y = fxm(station_x(i));
        rs.radius = fxm(r);
        rs.segments = static_cast<uint8_t>(station_sides(i));
        rs.b0 = bd.b0;
        rs.b1 = bd.b1;
        rs.w0 = bd.w0;
        rs.rx = fxm(r * station_wide(i) / 100);  // LATERAL
        rs.rz = fxm(r);                          // VERTICAL
        rs.cz = -fxm(kBodyY);
        p.rings.push_back(rs);
      }
      p.page = kTileBody;
      p.v0 = 50;   // T4: continues the atlas exactly where the head ends
      p.v1 = 255;
      set_rgb(p, kGreen);  // fallback if the page is ever absent
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
      // BOTH faces of each fin carry BOTH colours (Fabian, 2026-08-27
      // pass 3: "both sides should actually have both colors. big slice of
      // pink, weaker slice of creen") -- each blade tile is pink with a
      // green slice laid along one thin edge, and the two blades put the
      // slice on opposite edges so they stay distinguishable.
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
    // A1: Zixxtrixx is the hero tier -- bake the 60 Hz presentation
    // companion at compile (~doubles this bank's pose bytes; poses are
    // final for this pass, which is why A1 lands LAST in Wave D)
    bank.bake60 = true;
    bank.clips.push_back(build_idle());
    bank.clips.push_back(build_walk());
    bank.clips.push_back(build_attack());
    bank.clips.push_back(build_fall());
    bank.clips.push_back(build_hit());
    bank.clips.push_back(build_death());
    bank.clips.push_back(build_balance());
    bank.clips.push_back(build_look());
    // C2: the phase vocabulary, sliced from the local-body attack at shared
    // keys; the two authored phases start/end on the exact spear pose. The
    // declared seams below are ENFORCED by compile_creature -- a phase edit
    // that breaks a seam fails the whole creature compile.
    {
      const zc::Clip atk_local = build_attack(true);
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkCompress, 0, 9));
      // release runs one key past the curl's arrival: integer curve
      // truncation leaves auth = 1 (not 0) at key 18 exactly, so the clean
      // coil keys are 19..20 (the compiler's seam check caught it)
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkRelease, 9, 19));
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkCoil, 19, 20));
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkUnroll, 40, 48));  // 48: curl truncation (=1 at 47)
      bank.clips.push_back(build_spear_flex());
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkStick, 62, 63));
      bank.clips.push_back(build_air_hit());
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkRecover, 212, 225));
      bank.seams = {
          {kSlotAtkCompress, 9, kSlotAtkRelease, 0},
          {kSlotAtkRelease, 10, kSlotAtkCoil, 0},
          {kSlotAtkCoil, 0, kSlotAtkCoil, 1},          // the hold loops
          {kSlotAtkCoil, 1, kSlotAtkUnroll, 0},
          {kSlotAtkUnroll, 8, kSlotAtkSpearFlex, 0},   // unroll ends straight
          {kSlotAtkSpearFlex, 0, kSlotAtkSpearFlex, 9},// flex returns straight
          {kSlotAtkSpearFlex, 0, kSlotAtkStick, 0},
          {kSlotAtkStick, 0, kSlotAtkStick, 1},        // the stick loops
          {kSlotAtkStick, 1, kSlotAtkAirHit, 0},
          {kSlotAtkAirHit, 0, kSlotAtkAirHit, 11},     // the recoil rings out
          {kSlotAtkAirHit, 11, kSlotAtkRecover, 0},
          {kSlotAtkRecover, 13, kSlotAtkCompress, 0},  // ...back to the S
      };
    }
#ifdef ZIXX_F2_PREVIEW
    // F2, slot 19: the baked spring-chain fall. PREVIEW-BUILD ONLY -- the
    // bake has real dynamics character (delayed waves, overshoot, mass)
    // but still curls ~330 mm into itself at its tightest, and the owner
    // has not ruled on it against the F1 relax he directed. It does not
    // ship half-tuned; -DZIXX_F2_PREVIEW builds it for the side-by-side.
    bank.clips.push_back(build_fall_baked());
#endif
#ifdef ZIXX_SWEEP
    bank.clips.push_back(build_sweep());
#endif

    zc::CreatureType type;
    type.type_id = 2;
    const char* reason = "";
    if (!zc::compile_creature(sk, bank, parts, type, &reason)) {
      std::fprintf(stderr, "zixxtrixx: compile failed: %s\n", reason);
    }
    type.page_set = &page();
    type.page_direct = &page_direct();  // RGB565 + bilinear + mips (T1/T2)
    return type;
  }();
  return t;
}

}  // namespace zixx

#endif  // ZHAO_REEL_ZIXXTRIXX_H
