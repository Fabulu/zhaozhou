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
#elif defined(ZIXX_PAGE_VARIANT)
// RUN 1939 texture experiments: a generated variant page (same symbols,
// written by mkcreaturepage.py --experiment NAME into tools/reel/exp/,
// which is gitignored -- the generator is committed and deterministic,
// the headers are reproducible artefacts). Experimental builds only; a
// build without the define is bit-identical to the shipping page.
#include ZIXX_PAGE_VARIANT
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
// RE-AUTHORED, RUN 0326 owner redirect: "The upper S part gets a bit too
// broad and big. The broadest biggest should be neck and then head. Neck
// doesn't really exist right now because it's just a huge dropoff." So the
// profile now BUILDS through a broad NECK (the animal's widest stretch,
// t~150..270, peak 1410) into the head, and the upper S behind it SLIMS
// (1090/890 through the arc against the old 1040/940 ramp -- the body
// reads as a distinct slimmer region so the neck can exist at all). The
// neck arrives and departs on curves, not steps: one tube, no cliff --
// both recorded failure modes (the sheer drop AND the dissolved blend)
// judged against on the side render beside Side.png.
// THE GROUNDED RUN'S RADII (t >= 560) ARE LOAD-BEARING: the grounded
// slope table is asin(radius drop/segment) against exactly these values,
// so they stay UNTOUCHED to the millimetre.
// sidecmp-02: the first neck build put the girth peak at t=220 -- BEHIND
// the head part, hidden inside the tight curl -- so the render showed a
// PINCH: narrow behind the skull, then the bulb. The sheet widens
// MONOTONICALLY into the head with no reversal anywhere. The peak now
// sits at t=110, inside the skull itself: body -> neck -> head is one
// unbroken rise, and the dome rounds it off at the tip.
// sidecmp-03, the owner's EXACT order: "the head shouldn't be the widest
// part, the neck should be. After neck, body should quickly become
// smaller until it's normal. Head meanwhile should be maybe the second
// widest part." So: one monotone rise nose -> head (1360) -> NECK PEAK
// (1430, at the head/body junction) -> a FAST drop to normal body width
// by t~340. No pinch anywhere; the reduction neck -> head is the drawn
// one, not a reversal in the middle.
// sidecmp-04: the head was a modest bulb against the sheet's LARGE
// terminal lobe ("second widest is still big"). Head radii up ~4%,
// neck peak follows (still the widest), fast drop unchanged.
// sidecmp-05 -- THE OUTLINE GATE'S VERDICT (fronthalf overlay): the whole
// front tube was a WIRE against the sheet's CHUBBY comma. The sheet's
// tube reads ~40%% of the loop's height; ours read ~22%%. The front half
// fattens boldly (neck 1900, head 1830 -- the girth order holds), the
// fast drop lands on the approved body width by t~420, and the grounded
// radii (t>=560) stay untouched to the millimetre.
// RUN 0757 notch campaign, iteration 2: the last visible bump-dip on the
// upper contour was the fast neck->body drop LANDING ON the crown's right
// flank -- 1900 -> 1560 across one key span put a girth corner exactly
// where the arch turns. The drop now arrives on a curve (1780 at 260)
// and finishes by t~400: still the owner's fast drop-off behind the neck
// (girth order untouched: neck 1900 > head 1830 > body 860), the corner
// gone. Judged on the unlit outline beside Side.png (sidecmp-08).
// THE GIRTH KNOB (deferred from RUN 0757, UNBLOCKED this run: the owner
// froze girth "until the head position, neck tangent, and unbroken
// contour are correct" -- sidecmp-14 and the coordinator's own reading
// say they are). The matched-pose instrument (sheetpose-girth-overlay,
// RUN 0757) showed the tube ~2x the DRAWN tube at matched pose, and the
// owner's eye said it first: "the upper S part gets a bit too broad and
// big." Per-mille scale on every body/head radius; the girth ORDER
// (body < head < neck, fast drop) is a ratio law and survives any
// uniform scale. The VALUE is picked off a rendered ladder BY EYE
// against the side gate and the site cameras (the overlay removes the
// bias; the render chooses the value) -- a 2D outline is an
// interpretation, not a cross-section, and 240p legibility votes too.
#ifndef ZIXX_GIRTH
// 850, PICKED OFF THE LADDER BY EYE (girth-ladder.png, rungs 1000/850/
// 700/550 at the side gate and the site camera): 850 answers the owner's
// "A BIT too broad and big" -- his own words scale the correction -- while
// the chubby comma character and the culminating head survive; 700 opens
// the loop but starts shedding the approved presence; 550 is the recorded
// WIRE fault reborn. The 2x matched-pose instrument finding removed the
// bias; the render chose the value (sidecmp-15-girth850).
#define ZIXX_GIRTH 850
#endif

// V9 complete nose-to-tail progression. Every regional value is a named art
// knob: nose only a little slimmer than the subtle neck maximum; the long
// front, middle S and grounded run change gently; the one strong contrast is
// the sustained tail taper. These were authored as a continuous silhouette
// and are accepted only through the numbered full-side + walk evidence.
constexpr int32_t kRadiusNose = 1120;
constexpr int32_t kRadiusNoseFull = 1160;
constexpr int32_t kRadiusHead = 1210;
constexpr int32_t kRadiusNeckFull = 1240;
constexpr int32_t kRadiusNeckRelease = 1210;
constexpr int32_t kRadiusLongFront = 1160;
constexpr int32_t kRadiusMiddleFront = 1140;
constexpr int32_t kRadiusMiddle = 1100;
constexpr int32_t kRadiusGroundEntry = 1060;
constexpr int32_t kRadiusGroundRun = 1040;
constexpr int32_t kRadiusTaperShoulder = 980;
constexpr int32_t kRadiusTailHeavy = 780;
constexpr int32_t kRadiusTailMiddle = 580;
constexpr int32_t kRadiusTailFine = 400;
constexpr int32_t kRadiusTailThin = 260;
constexpr int32_t kRadiusTailTip = 180;
constexpr TaperKey kTaper[] = {
    {0, kRadiusNose},
    {50, kRadiusNoseFull},
    {110, kRadiusHead},
    {170, kRadiusNeckFull},
    {230, kRadiusNeckRelease},
    {320, kRadiusLongFront},
    {430, kRadiusMiddleFront},
    {560, kRadiusMiddle},
    {650, kRadiusGroundEntry},
    {720, kRadiusGroundRun},
    {760, kRadiusTaperShoulder},
    {820, kRadiusTailHeavy},
    {880, kRadiusTailMiddle},
    {930, kRadiusTailFine},
    {970, kRadiusTailThin},
    {1000, kRadiusTailTip},
};
constexpr int kTaperKeys = static_cast<int>(sizeof(kTaper) / sizeof(TaperKey));
// Bind height of the body axis. The v9 grounded run is intentionally thicker,
// so its named carry starts 32 mm above the v8 value; the committed 3D probe,
// never a rendered terrain pixel, verifies the authored contact.
constexpr int32_t kBodyCarryMm = 1084;
constexpr int32_t kBodyY = kBodyCarryMm;
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
// RUN 1939, fault 2 (owner: "The front of the face is completely flat
// instead of rounded. I hope we can do that without attaching a weird
// helmet."). No overlay part, ever -- the rounding comes from the terminal
// rings of the ONE tube: the head stations are REPACKED toward the nose
// (kHeadStationYMm below) and these factors put the packed rings on a
// blunt superellipse (n~2.4, run-in ~135 mm), so the face is a genuine
// dome ending in a small cap dot instead of a 114 mm flat disc. The old
// 520/840/950/990 factors on 54.5 mm uniform spacing WERE the flat face:
// the first ring still carried half the full radius. Blunt, not pointed:
// the 270 tip factor keeps a real cap (the recorded soft-point fault came
// from drawing a SMALL radius out over a LONG first segment; the packed
// 18 mm first segment cannot do that).
constexpr int16_t kNoseDome[kNoseDomeStations] = {270, 590, 810, 950};

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
// -12000, was -6000 (run 0326 owner redirect: "drops down like crazy...
// needs to move way up"). Picked off the WIDE -16000..+14000 sweep sheet
// (attitude-sweep-wide.png), judged with the re-authored shallow neck: at
// -12000 the head CONTINUES the crown's line -- seamless, eye presented,
// snout just above level; -6000 already tips down; -16000 creases the
// crown. The render chose the value; the number is just its name.
// NEUTRAL, RUN 1730 (owner direction #3, followed literally): "Return
// the head attitude bone close to neutral. It should provide small
// expressive look adjustments, not compensate for a fundamentally
// wrong body curve." The -12000 was exactly that compensation --
// counter-rotation against a neck that DESCENDED into the skull. The
// front spline (kFrontSnoutSlopeA16) now arrives slightly upward and
// the skull CONTINUES the final neck tangent at attitude 0 by
// construction. The sweep mechanism stays for expressive trims.
#define ZIXX_ATTITUDE (0)
#endif
constexpr int32_t kHeadAttitude = ZIXX_ATTITUDE;
// where the skull bone pivots, mm behind the nose (~station 3.5, the
// culminating head's centroid). RUN 0326, owner: "head should be joint
// rotation, it's just that the joint needs to fit onto a body that
// doesn't suddenly have the head drop 5 meters" -- the value was 0, so
// the joint pivoted at the NOSE TIP and every attitude change ORBITED
// the cranium about its own snout (the sweep sheet shows the arc). At
// the centroid the head turns roughly about itself.
constexpr int32_t kHeadPivotMm = 190;
constexpr int kSkullRigidTo = 2;  // stations 0..2 fully on the head bone
                                  // (4 -> 2, RUN 0757 notch campaign: the
                                  // remaining head/neck notch is the hinge
                                  // elbow -- an even longer falloff lets the
                                  // dorsal contour carry the skull's unwind
                                  // across five more stations; judged on the
                                  // unlit outline beside Side.png)
                                  // (5 -> 4, run 0326: the hinge must be
                                  // SEAMLESS -- a shorter rigid core and a
                                  // longer falloff let the tube bend into
                                  // the skull instead of creasing at it)
constexpr int kSkullBlendTo = 10; // last station that carries any kBHead
                                  // weight. 11 -> 10, RUN 1939 (fault 4,
                                  // "you can see the seams"): at 11 the
                                  // blend window INCLUDED the junction ring
                                  // (kHeadEnd), so the head part's copy of
                                  // that ring carried {kBHead, spine, 6/64}
                                  // while the body part's copy carried pure
                                  // station_bind -- identical bind POSITIONS
                                  // but disagreeing binds, and the two
                                  // copies skinned APART whenever the head
                                  // bone moved: a pose-dependent OPEN seam,
                                  // 24 mm at the idle's breath, 79 mm in the
                                  // fall (zixx-meshcheck, committed this
                                  // run). The junction ring now takes
                                  // station_bind EXACTLY in both parts; the
                                  // meshcheck gate holds it at zero.

// THE EYE DISC IS PAINT; ONLY THE MOVING PUPIL IS TINY GEOMETRY. A whole
// yellow eyeball mesh looked exactly like a sphere glued to a tube, so the
// drawing's disc and ink ring stay in the head page and the head's own rings
// swell LATERALLY beneath it. V9 adds one shallow orange slit decal per side:
// both follow the same authored gaze and never contribute to the silhouette.
constexpr int kEyeStation0 = 3;      // first head station that carries the bulge
constexpr int kEyeStation1 = 6;      // last: shifted one station noseward so
                                     // the two local swellings support the
                                     // painted side eyes instead of trailing
                                     // them toward the neck.
#ifndef ZIXX_EYEBULGE
// V9 local eye support: stronger than 26 without returning to the old
// six-station 42-percent brim. Judge with the noseward atlas row in fixed
// front and side views.
#define ZIXX_EYEBULGE 32
#endif
constexpr int32_t kEyeBulgeNum = ZIXX_EYEBULGE;
// Moving-pupil construction and motion knobs. The orange marking is ONE
// deforming stripe: its swollen middle follows the pupil pivot while the two
// tips remain attached to the painted eyeball boundary. Thus vertical or
// diagonal gaze extends one arm and contracts the other instead of letting a
// fixed decal float inside the eye (owner direction #8). Two mirrored bones
// still share one intent; their signs differ only because the eyes face
// opposite sides.
constexpr int kPupilStation = 5;
constexpr int32_t kPupilCoreHalfWidthMm = 20;      // the stripe's peculiar swell
constexpr int32_t kPupilCoreHalfAngleA16 = 1900;   // extent of the moving swell
constexpr int32_t kPupilStripeShoulderA16 = 3300;  // elastic arm control point
constexpr int32_t kPupilStripeBoundaryA16 = 5700;  // endpoint on eyeball rim
constexpr int32_t kPupilStripeCoreEdgeMm = 13;
constexpr int32_t kPupilStripeArmHalfWidthMm = 7;
constexpr int32_t kPupilStripeTipHalfWidthMm = 3;
constexpr int32_t kPupilStripeSurfaceLiftMm = 7;
constexpr int32_t kPupilStripeDepthMm = 2;
constexpr uint8_t kPupilStripeShoulderFollow = 28; // /64 pupil, remainder head
constexpr int kPupilStripeSides = 4;
#ifndef ZIXX_PUPIL_MOTION
#define ZIXX_PUPIL_MOTION 1
#endif
constexpr bool kPupilMotion = ZIXX_PUPIL_MOTION != 0;
constexpr int32_t kPupilGlanceA16 = 1700;       // readable ~9-degree side glance
constexpr int32_t kPupilGlanceLiftA16 = 720;    // restrained vertical settle
constexpr int kPupilHeadLagKeys = 4;
constexpr int kPupilIdleMoveInKey = 26;
constexpr int kPupilIdleSettleKey = 36;
constexpr int kPupilIdleHoldEndKey = 62;
constexpr int kPupilIdleMoveOutKey = 74;
constexpr int kPupilIdleRestKey = 84;
// Static-head acceptance clip: it deliberately reaches both vertical extrema
// and both diagonal corners, then holds before and after the reversal. This is
// committed proof machinery, not runtime wandering.
constexpr uint16_t kSlotPupilProof = 45;
constexpr int kPupilProofKeys = 64;
constexpr int kPupilProofRestEndKey = 8;
constexpr int kPupilProofUpKey = 18;
constexpr int kPupilProofUpHoldEndKey = 26;
constexpr int kPupilProofDownKey = 38;
constexpr int kPupilProofDownHoldEndKey = 46;
constexpr int kPupilProofReturnKey = 56;
                                     // extra lateral half-width, % of the ring.
                                     // 22, was 16 (run 0326): head-on at 16
                                     // each eye was a thin crescent hugging
                                     // the silhouette rim. A 16/22/28 ladder
                                     // on the head-on still: 22 reads as two
                                     // proper eye pads with the crown gap
                                     // intact; 28 starts wedging the skull
                                     // outline. Profile checked, unchanged
                                     // read. 16's note kept below.
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
// RUN 0757, THE BLADE DECISION (the owner left the shape open; the sheet is
// the gold standard and draws LONG SLENDER blades, so they are rebuilt to
// its proportion). Measured off Side.png on the comparison side: the drawn
// blades are ~420 mm at body scale -- SHORTER than the old 780 -- but read
// long because they are ~12:1 slender; ours were 5.6:1, which is why every
// review called them "short and broad" despite being the longer ones. So:
// slenderness is the fix, not length. 860 long, 36 half-width (~12:1),
// thinner, with a LEAF profile (widest ~30% out, then one long straight
// taper to the point) replacing the root-heavy 1-t^2 paddle. Splay 3000
// and the 80-deg roll keep their owner-ordered values.
constexpr int32_t kBladeLen = 860;
constexpr int32_t kBladeW0 = 36;       // half-width at the leaf's widest (LATERAL)
constexpr int32_t kBladeThick0 = 12;   // half-thickness at the root (VERTICAL)
// 1500, was 6900 (Fabian, 2026-08-27 pass 3: the fins "should be rotated
// almost 90 degrees so [the fins] are almost lined up with body. But not
// quite"): each blade now trails ~8 deg off the tail's own axis instead of
// splaying ~38 deg across it.
constexpr int32_t kBladeSplay = 3000;  // 1500 -> 3000 (run 0326 owner:
                                       // "should be further apart")
// RUN 1939, fault 1 (owner: "You rotated fins by 80 degrees. That wasn't
// the idea. You were supposed to rotate the entire tail. Unrotate the
// fins, rotate the end of the tail, the fins should go along with it
// automatically."). So the 80 degrees moves OFF the blades' own bones and
// ONTO THE END OF THE TAIL: kTailRoll twists the last three spine joints
// about the tube's own axis (a roll moves no centreline nodes), and the
// blades -- children of the fork -- inherit it in every clip for free.
// This is also the sounder construction: a blade rotated independently of
// the limb it grows from fights that limb in every animation.
constexpr int32_t kBladeRoll = 0;      // owner: "Unrotate the fins"
constexpr int32_t kTailRoll = 14563;   // ~80 deg, distributed over the
                                       // tail-rise joints so the tube
                                       // twists organically instead of
                                       // snapping at one ring
constexpr int32_t kBladeRise = 2600;   // in the ROLLED frame this is the
                                       // lateral flap component; clips
                                       // animate it for blade life
// THE FAN'S UP-BIAS (RUN 1939, with the tail roll): in the rolled frame
// the splay spreads the pair in the SAGITTAL plane, so a symmetric +-3000
// hung one blade ~240 mm under the tail line and the fall/hit/balance
// probes caught it sweeping dirt. Side.png draws the fork ASYMMETRIC:
// the lower blade nearly CONTINUES the tail's own line, the upper one
// splays high. The bias recreates exactly that: pair spread stays
// 2*kBladeSplay ("should be further apart" kept), nothing reaches below
// the stem. Sign judged on the render (the rolled frame's up is not
// guessable on paper).
constexpr int32_t kBladeUpBias = 2600;
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
// 240 keys = 480 frames = 8.00 s at 60 Hz. Direction #9 adds a real,
// theatrical whole-body spring anticipation before the approved airborne
// wheel. The previous wobble-only prep was 10 keys; the new prep takes
// 18 keys to compress and hold, then releases over the next 10. Everything
// from the approved flight is shifted by 12 keys, without changing its wheel,
// apex hang, plunge, five-real-second planted-spear hold, or recovery timing.
constexpr int kAttackKeys = 240;
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
// ==== THE FRONT SPLINE (RUN 1730, owner direction #3, 2026-08-28) ====
// FIVE PASSES FAILED THE SAME WAY: preserve the descending S, improve
// the head, rotate it locally. Owner: "The head's baseline position
// and apparent gaze are primarily determined by the final third of the
// S-curve. A local head joint can rotate the skull, but it cannot make
// a descending neck suddenly read as a proud, upward-held head." And:
// "Today the head position is an accidental consequence of a preserved
// S plus local rotation. That is why it keeps oscillating."
// So the front segments are no longer five hand-fought angle constants.
// They are GENERATED: one smooth C1-continuous tangent ramp between two
// authored knobs (a spline constraint, the owner's named architecture):
//   - kFrontSnoutSlopeA16 -- the final neck tangent INTO the skull;
//     with kHeadAttitude neutral this IS the snout direction. Tailward-
//     positive = the neck CLIMBS toward the head. Authored from
//     Side.png: "slightly upward into the skull".
//   - the mid-body ANCHOR -- seg kFrontSegs keeps the dive entry's own
//     slope (kFrontAnchorSlopeA16), so everything from the dive down is
//     untouched and the handover turn equals the ramp's own turn rate:
//     no single hinge anywhere, the correction spans six bones.
// The ramp is MONOTONE: every front slope is positive, so walking
// headward the chain rises the whole way -- the neck carries the head
// UP and the head is the swollen conclusion at the top of the climb,
// never a lobe hanging under a hook. The existing S is broken here ON
// PURPOSE; preserving it was preserving the failure (the owner's own
// words), and the goldens are re-pinned for it with loud provenance.
// kFrontEaseQ bends the ramp (0 = constant turn per joint; 1000 =
// fully quadratic, turn gathering toward the anchor) -- a named knob,
// picked by looking at the Side.png overlay, like every value here.
constexpr int kFrontSegs = 4;                   // seg0..3 generated
    // 4, was 5 (coordinator gate on sidecmp-13): the climb was RIGHT but it
    // read as a long shallow ramp against the sheet's compact curled S. The
    // mid-body ANCHOR moves one segment closer to the head -- the dive
    // starts sooner, the upper loop closes tighter, and the head carries IN
    // over the body instead of out at arm's length. The snout tangent and
    // the C1 handover are untouched: the neck still climbs, on a tighter
    // curve. The freed segment becomes kFrontApproachSlopeA16 below.
constexpr int32_t kFrontSnoutSlopeA16 = 900;    // ~4.9 deg up into the skull
    // 900, was 1600 (sidecmp-11): at 1600 the whole skull rode a ~29 deg
    // rocket climb; the sheet's terminal lobe runs nearly LEVEL, arcing
    // gently over. Snout shallower, still up -- the climb is real.
constexpr int32_t kFrontAnchorSlopeA16 = 6800;  // the dive entry, unchanged
constexpr int32_t kFrontEaseQ = 1000;           // 0 linear .. 1000 quadratic
// The segment the shorter front frees: a FLAT approach between the dive's
// steep arrival and the grounded run -- the body pays out along the ground
// a full segment earlier, which is also the owner's standing preference
// ("make the part that touches the ground longer"). Slope 0 = the node
// rides at exactly the grounded height (taper is flat 860 there), and the
// descent-lobe deepen is a multiplicative no-op on it by construction.
constexpr int32_t kFrontApproachSlopeA16 = 0;
    // 1000, was 0 (sidecmp-11): the sheet's comma is straightish through
    // the fat lobe and gathers its turn into the dive; the constant-rate
    // ramp spent too much turn at the head end.
constexpr int32_t front_slope(int k) {
  const int32_t u = (k * 1000) / kFrontSegs;  // 0 at the snout
  const int32_t v = u + (kFrontEaseQ * ((u * u) / 1000 - u)) / 1000;
  return kFrontSnoutSlopeA16 +
         ((kFrontAnchorSlopeA16 - kFrontSnoutSlopeA16) * v) / 1000;
}

// Named centreline slopes for the weight-bearing run. The first three keep
// the newly thick middle close to level; the last three follow the authored
// tail taper. The 3D probe corrects contact after each visual profile choice.
constexpr int32_t kGroundSlopeEntryA16 = 260;
constexpr int32_t kGroundSlopeCarryA16 = 220;
constexpr int32_t kGroundSlopeReleaseA16 = 360;
constexpr int32_t kGroundSlopeTaper0A16 = 1380;
constexpr int32_t kGroundSlopeTaper1A16 = 1990;
constexpr int32_t kGroundSlopeTaper2A16 = 1760;

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
    // RE-AUTHORED, RUN 0326 OWNER REDIRECT ("You completely fucked up the
    // head... drops down like crazy... droopy ball... It needs to move way
    // up", and the rigid S is UNFROZEN as a named cause). Side.png measured
    // by eye: the head is the fat end of the arc at ~62% of the crown
    // height -- a SHORT fall out of the crown, the tube flowing into it --
    // not the bottom of a 66-degree plunge. The neck behind the head now
    // starts shallow (-27 deg) and curls progressively; the crown is
    // rounded (-1500 instead of a flat 0 corner); the dive is slightly
    // softened. kBodyY rises with it so the grounded run lands EXACTLY
    // where it always did (sine-sum solved, then probe-verified).
    // sidecmp-02 iteration: the upper loop was FLAT-wide against the
    // sheet's tall round arch and the head read mid-height. Steeper on
    // both flanks (a narrower, taller loop), the crown still rising
    // through -2400, and kBodyY re-solved -- head centre lands at ~63%%
    // of the crown, the sheet's own proportion.
    // sidecmp-04: the head hung below the crown and the loop read tight
    // and flat. The first neck segment steepens (the head hangs LESS below
    // the crown), the crown keeps RISING through -4400 (a taller, more
    // open arch whose apex sits behind the head), and the dive's entry
    // shallows so the descending side opens out.
    // RUN 0757, THE NOTCH CAMPAIGN. The residual head/neck notch was the
    // AXIS, not the hinge: stations 0..10 ran as a nearly straight -28..-49
    // deg diagonal (sideprofile probe) with the whole unwind concentrated
    // at the skull joint -- a polygonal elbow between two round lobes, and
    // the envelope of the posed station circles reproduces the notch with
    // no skinning at all. The sheet's comma turns CONTINUOUSLY: the head
    // reaches FORWARD just under the crown and the descent unwinds ~20 deg
    // per joint. So: seg1 shallows hard (the head reaches forward), seg2/3
    // carry the descent, seg4 steepens (the crown hands its steepness to
    // the descent earlier). Sine sum seg0..4 -2.859 -> -2.637; kBodyY
    // +36 re-plants the grounded run (probe-verified).
    // THE FRONT IS GENERATED -- see THE FRONT SPLINE above (RUN 1730,
    // owner direction #3). The 2026-08-27/28 hand tables that lived here
    // (3000/-4000/-8000/-11650/-7650 and their ancestors) preserved the
    // descending hook that five head passes could not compensate away.
    front_slope(0), front_slope(1), front_slope(2), front_slope(3),
    // the dive, past vertical and back under itself; its entry is the
    // front spline's mid-body anchor (one segment closer to the head since
    // the compact-S pass -- see kFrontSegs)
    kFrontAnchorSlopeA16, 14600, 21400, 25200, 20000, 11600,
    // the flat approach: the dive has landed; the body lies out along the
    // ground for a segment before the walking/snaking grounded set begins
    kFrontApproachSlopeA16,
    // the grounded run: three weight-bearing, nearly-level segments followed
    // by the sustained taper's centreline compensation
    (kGroundSlopeEntryA16 * ZIXX_GIRTH) / 1000,
    (kGroundSlopeCarryA16 * ZIXX_GIRTH) / 1000,
    (kGroundSlopeReleaseA16 * ZIXX_GIRTH) / 1000,
    (kGroundSlopeTaper0A16 * ZIXX_GIRTH) / 1000,
    (kGroundSlopeTaper1A16 * ZIXX_GIRTH) / 1000,
    (kGroundSlopeTaper2A16 * ZIXX_GIRTH) / 1000,
    // the tail rises behind, short and steep
    -5600, -11400};

// THE WHOLE-BODY SPRING (owner direction #9). This is a separately authored
// pose, not a larger idle breath and not a scale trick. The vertical S pays
// out almost flat just above the floor while a broad lateral concertina keeps
// the continuous tube visibly compact and volumetric in 3D. Every salto and
// jump calls apply_spring_stance below, so the obsolete deepen-only wobble
// cannot survive in a variant. These are absolute segment directions, like
// kStanceSlope; pitch/yaw joints are their adjacent differences.
constexpr int32_t kSpringCompressionDepth = 1000;  // profile authority, 1/1000
constexpr int32_t kSpringCompressionDropMm = 600;  // full-squash root descent
constexpr int32_t kSpringDeclaredBiteMm = 40;      // permitted posed-surface bite
constexpr int32_t kSpringCompressedSlope[kStanceSlopes] = {
    200, 400, 700, 900, 700, 300, -300, -600, -400, 0,
    500, 800, 650, 300, 0, -200, -100, 300, 600};
constexpr int32_t kSpringCompressedYaw[kStanceSlopes] = {
    0, 3000, 7000, 11000, 13500, 11000, 5000, -6500, -13000, -11000,
    -4500, 7000, 13000, 11000, 5000, -6000, -10500, -5000, 0};
constexpr int32_t kSpringHeadAttitude = 0;          // snout joins the flat spring
constexpr int32_t kSpringBladeFlare = 900;          // fan braces during compression

// Named theatrical timing. The existing attack spends long enough at maximum
// squash to be readable; immediate jumps use the short controls declared with
// their plan below.
constexpr int kSaltoCompressEndKey = 11;
constexpr int kSaltoCompressHoldEndKey = 17;
constexpr int kSaltoReleaseEndKey = 28;
constexpr int kSaltoCoilPoseKey = kSaltoReleaseEndKey + 1;  // past signed-rounding residue
constexpr int kSaltoUnrollStartKey = 52;
constexpr int kSaltoUnrollEndKey = 60;

// Slot 30 is an accepted quick taunt and remains a frozen animation. V9's
// thicker weight-bearing run required new stance slopes for the living clips,
// but must not silently retime or reshape this existing gesture. These are its
// historical v8 stance values, kept as a named local bind so all 26 pre-pupil
// bone channels and the root remain byte-identical to the committed golden.
constexpr int32_t kQuickTauntStanceSlope[kStanceSlopes] = {
    front_slope(0), front_slope(1), front_slope(2), front_slope(3),
    kFrontAnchorSlopeA16, 14600, 21400, 25200, 20000, 11600,
    kFrontApproachSlopeA16,
    (40 * ZIXX_GIRTH) / 1000, (380 * ZIXX_GIRTH) / 1000,
    (710 * ZIXX_GIRTH) / 1000, (960 * ZIXX_GIRTH) / 1000,
    (1100 * ZIXX_GIRTH) / 1000, (1220 * ZIXX_GIRTH) / 1000,
    -5600, -11400};
// which slope entries the descent lobe occupies (breathing deepens these)
constexpr int kStanceDescend0 = 4;  // follows the anchor (compact-S pass)
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
constexpr int32_t kAtkApexLift = 12000;  // mm of root lift at the six-salto apex
constexpr int32_t kAtkNineApexLift = 2 * kAtkApexLift;  // exact direction-9 ratio
constexpr int32_t kAtkFwdMax = 7420;     // mm forward at impact: 1850 by the
                                         // apex + 5570 on the 30-deg plunge
constexpr int32_t kAtkSpinStick = 3333;  // 1/1000 turns: 30 deg from vertical
constexpr int32_t kAtkStickDepth = 420;  // mm of authored burial
constexpr int32_t kAtkTipDrop = 3317;    // reach * sin(60 deg), see above
constexpr int32_t kAtkTipFwd = 1915;     // reach * cos(60 deg): how far the
                                         // buried tip leads the nose in +X
constexpr int32_t kAtkTipReachMm = 3908; // nose -> blade tip of the POSED
                                         // spear, MEASURED by the committed
                                         // zixx-striketip probe (3908/3909/
                                         // 3910 across all three variants;
                                         // the ideal-straight 3830 was 78 mm
                                         // short -- the blades extend). THE
                                         // WEAPON IS THE TAIL TIP: every
                                         // parametric plan stops the ROOT
                                         // short by this so the TIP lands
                                         // the strike. The probe also proved
                                         // the pose's weapon vector tracks
                                         // the committed aim within ~1 deg,
                                         // and that the NOSE rides kBodyY
                                         // above the plan's root -- see
                                         // zixx_plan_lock_spear.
constexpr int32_t kAtkPlungeMinMm = 600; // the dive must exist: the commit
                                         // point sits at least reach+this
                                         // from the intercept
constexpr int32_t kAtkStickLift = kAtkTipDrop - kBodyY - kAtkStickDepth;
constexpr int kAtkImpactKey = 74;        // approved flight shifted by spring prep
constexpr int kAtkStickEnd = 224;        // impact + 150 keys = 5.0 s stuck

// FALL: the slow distress tumble. The whole S rotates about its own centre
// (re-pivoted off the nose exactly the way the salto re-pivots its spin to
// the coil centre -- "I think we did that well on salto"), one full pitch
// turn per loop, so it stands on its head at the half and comes back up;
// slow roll and yaw wobbles ride along so every axis moves. The pivot is the
// posed S's planform centre, measured by the pose probe.
constexpr int32_t kFallPivotX = -990;   // mm behind the nose
constexpr int32_t kFallPivotY = -60;    // mm below the nose
constexpr int32_t kFallLift = 1371;  // 916 -> 1371, RUN 1939: the rolled
    // fan swings a deeper arc through the tumble; probe-set back to the
    // approved ~20 mm near-brush after the up-bias reclaimed most of it.    // 890 -> 916, girth 850: the slimmer
    // tube (and its kBodyY carry) lowered the tumble to a -6 mm ground
    // kiss; +26 restores the authored ~20 mm near-brush (probe).
    // 934 -> 890, compact-S pass: the
    // shorter front shrank the tumble disc and the bottom rose to 64 mm
    // of air; the approved character NEAR-BRUSHES (~20 mm).
    // RE-AUTHORED RUN 1730: the front-S
    // reconstruction carries the bind pose 547 mm higher and the raised
    // climb sweeps a bigger tumble disc -- at 1180 the loop left the
    // fixed side frame for ~2 contact-sheet rows (motion-fall-sheet).
    // 934 = 1180 - 246 restores the approved near-brush clearance
    // (0757 band bottom 18 mm; probe re-run after). The camera widens
    // one step in the reel for the larger disc, which is real geometry.
    // (1300 rode the then-570 kBodyY out the frame top, run 0326.)     // mm of air under the nose; the probe
                                        // verifies the loop never touches
                                        // (1150 left the longer blades 24 mm)
// ROTATION DOWN, WOBBLE UP (2026-08-27 pass 3, Fabian: "When falling, the
// rotation is too strong. On the other hand, the snake should be wobbly ...
// creature in fall mode should wobble more, not jitter"). The rigid-body
// rotation amplitudes shrink; the BENDY amplitudes -- neck loll, serpentine
// wave, writhe -- grow, all on the same slow one-per-loop periods (slower
// still now the loop is 4.8 s), so the extra motion is loose and continuous,
// never a twitch. The idle/walk wobble is the reference.
// RUN 0326 owner redirect ("the fall is bad, it just rigidly falls over
// like a log"): the tumble's rigid rotation dominated the read. Every
// BEND term grows and the rotation WANDERS more (roll/yaw wobble up, the
// warp hesitates harder), the per-joint authority swings 8..92%% and its
// collapse travels faster down the body -- a body losing its structure,
// not a log toppling. All values picked on contact sheets.
constexpr int32_t kFallRollAmp = 5600;  // ~24 deg of slow roll wobble (was 35)
constexpr int32_t kFallYawAmp = 4200;   // ~19 deg of slow yaw wobble (was 28)
constexpr int32_t kFallNeckAmp = 6200;  // slow loose head/neck flail, up again
constexpr int32_t kFallWritheAmp = 2400;  // mid-body roll-twist writhe
// THE LATERAL WAVE (Fabian, 2026-08-27: "It needs to be wobbly side to
// side ... It needs to be wavey. The walk does waveyness pretty well for the
// caterpillar part"). A slow serpentine wave travelling nose -> tail through
// every spine joint, strongest at the head, free of the ground because the
// fall never touches it. This is the walk's principle turned sideways.
constexpr int32_t kFallWaveAmp = 5200;     // ~20 deg at the head (was 14 --
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
constexpr int32_t kFallAuthSwing = 420;    // swing: 16%..84% over the loop
constexpr int32_t kFallAuthSpatial = 4400; // phase step per joint
// NONUNIFORM tumble (2026-08-27, reports/ZixxtrixxReport: "a perfectly
// uniform full revolution reads like a display turntable"). The tumble
// phase is warped by this * sin(phase): it accelerates through one half of
// the turn and hesitates through the other, while the endpoints stay exact
// so the loop still closes. ~9% of a turn of maximum lead/lag.
constexpr int32_t kFallTumbleWarp = 8200;
constexpr int32_t kFallTumbleWarp2 = 3000;  // second harmonic: the tumble
                                            // SLOWS near upright (a righting
                                            // attempt) and gives way -- the
                                            // fight, not more oscillation
constexpr int32_t kFallHeadAim = 4600;      // the head LOOKS where it falls
                                            // (the aim rig's payoff): pitches
                                            // toward the ground through the
                                            // head-down half, eased

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
    {0, 0},          {22, 0},        {24, 180},      {26, 700},
    {28, 1500},      {32, 3200},     {38, 5600},     {44, 8200},
    {50, 10600},     {55, 11700},
    {59, kAtkApexLift}, {67, kAtkApexLift},   // approved eight-key apex hang
    {68, 11803},     {69, 11213},    {70, 10228},    {71, 8851},
    {72, 7079},      {73, 4914},
    {kAtkImpactKey, kAtkStickLift},  {kAtkStickEnd, kAtkStickLift},
    {226, 3200},     {228, 3400},    {231, 2200},     {234, 900},
    {237, 200},      {kAttackKeys - 1, 0}};
// forward drive in mm. THE PLUNGE IS THE STRAIGHT SHOT: over the dive keys
// the drive is 1850 + t^2 * 5570 -- the SAME t^2 as the lift, so every dive
// key sits exactly on the 30-degrees-from-vertical line the spear points
// along. Held through the stick, returned across the landing for the loop.
static const Key kAtkFwd[] = {
    {0, 0},     {26, 0},    {30, 150},  {38, 500},  {46, 1000},
    {54, 1550}, {61, 1850}, {67, 1850},  // drive hangs with the lift
    {68, 1964}, {69, 2305}, {70, 2873}, {71, 3669}, {72, 4692}, {73, 5942},
    {kAtkImpactKey, kAtkFwdMax},
    {kAtkStickEnd, kAtkFwdMax}, {228, 5200}, {232, 2600}, {kAttackKeys - 1, 0}};
// REAL PRELOAD: authority of the shared almost-flat spring profile. The whole
// animal descends over eleven keys, holds fully loaded for six, then releases
// while the approved wheel gathers. No descent-lobe deepen remains here.
static const Key kAtkPre[] = {
    {0, 0}, {3, 170}, {7, 620}, {kSaltoCompressEndKey, 1000},
    {kSaltoCompressHoldEndKey, 1000}, {22, 520},
    {kSaltoReleaseEndKey, 0}, {kAttackKeys - 1, 0}};
constexpr int kAtkPreN = static_cast<int>(sizeof(kAtkPre) / sizeof(Key));
// how much the TRACKING CAMERA aims at the spear's midpoint instead of the
// nose, in 1/1000 (Fabian, 2026-08-27 pass 3: the camera "doesn't catch the
// most important thing, which is the ground hit where the tail actually
// buries"). 0 while the body is a coil around the root; blended in as the
// spear forms at the apex; HELD through the dive, the impact and the whole
// five-second stick -- the buried tail is the shot -- and released only as
// the extraction re-gathers the S.
static const Key kAtkAim[] = {
    {0, 0}, {52, 0}, {59, 1000}, {226, 1000}, {232, 0}, {kAttackKeys - 1, 0}};
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
    {0, 0}, {kSaltoCompressHoldEndKey, 0}, {21, 350},
    {kSaltoReleaseEndKey, 1000}, {52, 1000}, {59, 0},
    {kAttackKeys - 1, 0}};
// how much of the canonical S remains -- the shared spring profile owns the
// loaded pose, then authority hands to the unchanged airborne wheel.
static const Key kAtkAuth[] = {{0, 1000}, {kSaltoCompressHoldEndKey, 1000},
                               {kSaltoReleaseEndKey, 0}, {226, 0},
                               {232, 650}, {kAttackKeys - 1, 1000}};
// accumulated turn of the WHOLE BODY in 1/1000 of a full rotation. 3000 =
// the three somersaults; kAtkSpinStick (3333) = the DIAGONAL spear, tail
// 60 deg below horizontal pointing down-and-forward, HELD from the apex
// through the dive, the impact and the whole stick; the extraction keeps
// it held until the tip is probed clear of the ground (key 206), and only
// then does the fourth turn land it. The -40 at key 12 is the wind-up --
// INSIDE the release, not the hold: at spin -40 the whole body pitches
// about the nose, and during the grounded compress that floated the rear
// 750 mm off the dirt (probe). By key 12 the launch is already airborne.
static const Key kAtkSpin[] = {{0, 0},          {22, 0},          {24, -40},
                               {27, 0},         {32, 700},        {40, 1600},
                               {48, 2600},      {55, 3050},       {59, kAtkSpinStick},
                               {226, kAtkSpinStick},               {230, 3650},
                               {233, 3900},     {kAttackKeys - 1, 4000}};
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
  // rigid cranium (see kHeadAttitude).
  kBHead = static_cast<uint8_t>(kSpineBones + 5),
  // Mirrored pupils have separate pivots but one authored intent. Two bones
  // avoid a shared off-centre rotation arc and still leave four spare slots.
  kBPupilL = static_cast<uint8_t>(kSpineBones + 6),
  kBPupilR = static_cast<uint8_t>(kSpineBones + 7),
  kBoneCount = static_cast<uint8_t>(kSpineBones + 8)
};
static_assert(kBoneCount <= 32, "creature_rules 1.2: <= 32 bones");

// station -> world distance back from the nose.
// THE DOME REPACK (RUN 1939, fault 2): ring COUNT in the head part is
// frozen (V maps by ring index, so inserting rings would shift every
// painted face row), but ring POSITIONS are authoring-free. The head
// stations pack toward the nose so the dome factors above have close
// stations to curve through -- and the eye/mouth paint rides its rings
// ~70 mm forward with the repack ("The eyes need to come forward more").
// Station kHeadEnd stays EXACTLY the linear station_x value: it is the
// junction ring shared bit-identically with the body part.
constexpr int16_t kHeadStationYMm[kHeadEnd + 1] = {
    0, 18, 45, 85, 135, 190, 250, 315, 385, 455, 525, 599};
inline int32_t station_x_linear(int i) {
  return static_cast<int32_t>((static_cast<int64_t>(kBodyLenMm) * i) / (kProfileStations - 1));
}
inline int32_t station_x(int i) {
  return i <= kHeadEnd ? kHeadStationYMm[i] : station_x_linear(i);
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
  r = (r * ZIXX_GIRTH) / 1000;  // the girth knob (see kTaper's header note)
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
    // long, even falloff across the blend window (run 0326 seamless-hinge
    // rework): weight walks 64 -> 0 linearly over the window instead of
    // the old four hard 13-steps

    const Bind sb = station_bind(i);
    const uint8_t spine = sb.w0 >= 32 ? sb.b0 : sb.b1;  // the majority bone
    // SMOOTHSTEP falloff (RUN 1939): the linear ramp ended on a 7/64 step
    // at the junction, and the meshcheck read the last head segment
    // stretching 2.8x under the walk's head surge -- the seam was closed
    // but the skin next to it still tore. Smoothstep is flat at BOTH ends:
    // the rigid core hands over gently and the junction arrives at zero
    // with a ~2/64 final step.
    const int span = kSkullBlendTo + 1 - kSkullRigidTo;
    const int t = ((kSkullBlendTo + 1 - i) * 1000) / span;
    const int ss = t * t * (3000 - 2 * t) / 1000000;  // 0..1000 (ss1000 law)
    const int w = (64 * ss) / 1000;
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
  kTilePupil = 3,       // flat orange page on the two shallow pupil decals
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
// THE LAYOUT (T4, 2026-08-28; pupil extension 2026-08-29): tiles 0..2
// address the ONE 256x512 BODY ATLAS at byte 0. Tile 3 is the pupil's flat
// orange 64x64 page; tiles 4..5 are the fins' own 64x64 pages. Each appended
// page has its own mode word (per-tile modes are lawful: the TMU mode is
// per-bind). Bilinear + mips bleed across atlas neighbours, which is why the
// small rigid parts do not live in the atlas.
inline const zref::DirectPageSet& page_direct() {
  static const zref::DirectPageSet ps = [] {
    zref::DirectPageSet p;
    constexpr int kWords = static_cast<int>(sizeof(kPageDirect[0]) / sizeof(uint16_t));
    p.mem.base = 0;
    const uint32_t atlas_bytes = static_cast<uint32_t>(kPageAtlasWords) * 2;
    p.mem.bytes.resize(atlas_bytes + static_cast<size_t>(3) * kWords * 2);
    for (int i = 0; i < kPageAtlasWords; ++i) {  // little-endian halfwords
      p.mem.bytes[static_cast<size_t>(i) * 2] = static_cast<uint8_t>(kPageAtlas[i] & 0xFF);
      p.mem.bytes[static_cast<size_t>(i) * 2 + 1] = static_cast<uint8_t>(kPageAtlas[i] >> 8);
    }
    constexpr uint8_t kAppendedPage[3] = {kTilePupil, kTileBladePinkUp,
                                          kTileBladeGreenUp};
    for (int t = 0; t < 3; ++t) {
      const size_t dst = atlas_bytes + static_cast<size_t>(t) * kWords * 2;
      const int page = kAppendedPage[t];
      for (int i = 0; i < kWords; ++i) {
        p.mem.bytes[dst + i * 2] = static_cast<uint8_t>(kPageDirect[page][i] & 0xFF);
        p.mem.bytes[dst + i * 2 + 1] = static_cast<uint8_t>(kPageDirect[page][i] >> 8);
      }
    }
    zref::Tmu::Mode ma;  // the atlas
    ma.fmt = zref::Tmu::kRgb565;
    ma.bilinear = true;
    ma.wrap_u = zref::Tmu::kRepeat;  // seamless around the ring
    ma.wrap_v = zref::Tmu::kClamp;   // the nose must not bleed into the fork
    ma.log2w = 8;                    // 256 (run 0326: the hi-res atlas)
    ma.log2h = 9;                    // 512 -- LOG2W != LOG2H is legal today
    ma.max_level = 7;
    ma.mip_en = true;
    zref::Tmu::Mode mb = ma;  // the blade pages
    mb.log2w = 6;
    mb.log2h = 6;
    mb.max_level = 6;
    const uint32_t am = ma.pack(), bm = mb.pack();
    p.mode = am;
    const uint32_t page_bytes = static_cast<uint32_t>(kWords) * 2;
    p.tile_base = {0, 0, 0, atlas_bytes, atlas_bytes + page_bytes,
                   atlas_bytes + page_bytes * 2};
    p.tile_mode = {am, am, am, bm, bm, bm};
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
  void tail_rest(int32_t splay = kBladeSplay, int32_t rise = kBladeRise,
                 int32_t bias = kBladeUpBias) {
    // THE END OF THE TAIL ROLLS (kTailRoll, fault 1): composed onto the
    // last three spine joints AFTER their stance pitch, about local X --
    // the tube's own axis -- so the centreline never moves and the whole
    // fan assembly (blades, spike, their offsets) inherits the turn.
    // Every clip calls tail_rest, so every clip carries the construction.
    const int32_t third = kTailRoll / 3;
    q[kSpineBones - 3] = quat_mul(q[kSpineBones - 3], quat_x(third));
    q[kSpineBones - 2] = quat_mul(q[kSpineBones - 2], quat_x(third));
    q[kSpineBones - 1] =
        quat_mul(q[kSpineBones - 1], quat_x(kTailRoll - 2 * third));
    // bias sign PROBE-CHOSEN (the rolled frame's +yaw turned out to point
    // DOWN: the first cut biased +2600 and the fall/hit worst-Y numbers
    // DEEPENED ~60-160 mm; negative lifts the pair so the lower blade
    // continues the tail line, the sheet's own asymmetry)
    q[kBBladeL] = quat_mul(quat_y(splay - bias), quat_z(-rise));
    q[kBBladeR] = quat_mul(quat_y(-splay - bias), quat_z(-rise));
    q[kBSpike] = quat_z(-rise / 2);
  }
  void write(zc::Clip& c, int f) const {
    for (int b = 0; b < kBoneCount; ++b) c.quats[static_cast<size_t>(f) * kBoneCount + b] = q[b];
  }
};

inline int32_t pupil_clamp(int32_t v, int32_t limit) {
  return v < -limit ? -limit : (v > limit ? limit : v);
}

// One apparent gaze, mirrored onto the two side-facing pupil pivots. A +side
// glance rotates both pupils noseward in world space; the mirrored signs are
// construction, not independent intent. Vertical travel is clamped tighter.
inline void apply_pupil_gaze(Rig& g, int32_t side_a16, int32_t lift_a16) {
  if (!kPupilMotion) {
    g.q[kBPupilL] = zc::quat16_identity();
    g.q[kBPupilR] = zc::quat16_identity();
    return;
  }
  const int32_t side = pupil_clamp(side_a16, kPupilGlanceA16);
  const int32_t lift = pupil_clamp(lift_a16, kPupilGlanceLiftA16);
  g.q[kBPupilL] = quat_mul(quat_x(-lift), quat_y(side));
  g.q[kBPupilR] = quat_mul(quat_x(lift), quat_y(-side));
}

inline void apply_idle_pupil_gaze(Rig& g, int f) {
  // One slow intentional glance and calm hold per 3.2-second idle loop. The
  // return reaches rest well before the wrap, so interpolation cannot pop.
  static const Key kSide[] = {
      {0, 0}, {kPupilIdleMoveInKey, 0},
      {kPupilIdleSettleKey, kPupilGlanceA16},
      {kPupilIdleHoldEndKey, kPupilGlanceA16},
      {kPupilIdleMoveOutKey, 0}, {kPupilIdleRestKey, 0},
      {kIdleKeys - 1, 0}};
  static const Key kLift[] = {
      {0, 0}, {kPupilIdleMoveInKey + kPupilHeadLagKeys, 0},
      {kPupilIdleSettleKey + kPupilHeadLagKeys, -kPupilGlanceLiftA16},
      {kPupilIdleHoldEndKey, -kPupilGlanceLiftA16},
      {kPupilIdleMoveOutKey + kPupilHeadLagKeys, 0},
      {kPupilIdleRestKey, 0}, {kIdleKeys - 1, 0}};
  apply_pupil_gaze(g,
                   curve(kSide, static_cast<int>(sizeof(kSide) / sizeof(Key)), f),
                   curve(kLift, static_cast<int>(sizeof(kLift) / sizeof(Key)), f));
}

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
                            const int32_t* wave = nullptr,
                            const int32_t* stance_override = nullptr) {
  const int32_t* stance = stance_override != nullptr ? stance_override : kStanceSlope;
  int32_t prev = 0;
  int64_t sink = 0;  // fx16 mm of belly drop vs the plain stance
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);
  for (int k = 0; k < kStanceSlopes; ++k) {
    int64_t d = stance[k];
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
      const int64_t base = (static_cast<int64_t>(stance[k]) * authority) / 1000;
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

// Root descent accelerates only as the whole concertina arrives. A linear
// drop made the still-tall middle pose push its grounded run through the floor
// and then lift back out at maximum squash. Quadratic easing keeps the
// travelling compression visible above ground and makes contact at the
// authored deepest pose, where stored spring energy should read. Kept as a
// shared pure helper so local clips and programmable root plans cannot drift.
inline int32_t spring_root_drop(int32_t amount) {
  if (amount < 0) amount = 0;
  if (amount > 1000) amount = 1000;
  const int32_t drop_amount = static_cast<int32_t>(
      (static_cast<int64_t>(amount) * amount) / 1000);
  return -static_cast<int32_t>(
      (static_cast<int64_t>(kSpringCompressionDropMm) * drop_amount) / 1000);
}

// Shared real spring pose for every salto and jump. `amount` is 0..1000 and
// blends absolute vertical and lateral segment directions before taking joint
// differences, so the chain remains continuous and never changes thickness.
// The root drop is authored explicitly: compression contact is intentional and
// checked by the committed posed-vertex probe, never inferred from a render.
inline int32_t apply_spring_stance(Rig& g, int32_t authority, int32_t amount) {
  if (amount < 0) amount = 0;
  if (amount > 1000) amount = 1000;
  amount = (amount * kSpringCompressionDepth) / 1000;
  zc::quat16 prev_world = zc::quat16_identity();
  for (int k = 0; k < kStanceSlopes; ++k) {
    const int32_t slope = kStanceSlope[k] + static_cast<int32_t>(
        (static_cast<int64_t>(kSpringCompressedSlope[k] - kStanceSlope[k]) * amount) / 1000);
    const int32_t yaw = static_cast<int32_t>(
        (static_cast<int64_t>(kSpringCompressedYaw[k]) * amount) / 1000);
    const int32_t pitch_auth = static_cast<int32_t>(
        (static_cast<int64_t>(slope) * authority) / 1000);
    const int32_t yaw_auth = static_cast<int32_t>(
        (static_cast<int64_t>(yaw) * authority) / 1000);

    // Author each segment's complete WORLD orientation, then derive this
    // joint's local difference. World yaw precedes pitch, so lateral folding
    // cannot tilt with an already-pitched parent and secretly become a dive.
    // Building the absolute orientation directly also avoids accumulating 19
    // conjugations in quat16 (the first world-delta draft magnified rounding
    // at the fork into a one-key spike). One inverse-product per joint keeps
    // the broad concertina deterministic and vertically honest.
    const zc::quat16 world =
        quat_mul(quat_y(yaw_auth), quat_z(pitch_auth));
    const zc::quat16 local = quat_mul(quat_conj(prev_world), world);
    g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], local);
    prev_world = world;
  }
  return spring_root_drop(amount);
}

inline int32_t spring_head_attitude(int32_t authority, int32_t amount) {
  if (amount < 0) amount = 0;
  if (amount > 1000) amount = 1000;
  const int32_t a = kHeadAttitude + static_cast<int32_t>(
      (static_cast<int64_t>(kSpringHeadAttitude - kHeadAttitude) * amount) / 1000);
  return static_cast<int32_t>((static_cast<int64_t>(a) * authority) / 1000);
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
// THE IDLE'S LIVING BODY, extracted (run 0326 owner redirect: the
// look-around "should look more like the idle animation, with an actually
// live moving body"). Everything build_idle composes below the head --
// breath+front-wave with exact root compensation, grounded sideways snake
// and torsion by world-vertical conjugation, tail sway, blade play, the
// neck's slight side-to-side -- lives here so other clips can carry the
// SAME life. `amp` in 1/1000 scales every amplitude; at exactly 1000 the
// scaling is skipped outright so the beloved idle stays BIT-IDENTICAL.
// `extra_wave` adds per-segment slope deltas into the compensated wave
// lane (a rider's neck-follow, for instance). Writes the head bone too
// (attitude + nod + sway); a clip that owns the head overwrites it after.
// Returns the computed root rise in mm.
inline int32_t idle_body(Rig& g, int32_t ph, int32_t amp,
                         const int32_t* extra_wave = nullptr,
                         const int32_t* stance_override = nullptr) {
  const auto A = [amp](int32_t v) {
    return amp == 1000 ? v : static_cast<int32_t>((static_cast<int64_t>(v) * amp) / 1000);
  };
  const int32_t s = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
  const int32_t st =
      zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2 - 21000) & 0xFFFF)}).raw;
  const int32_t breath = ((s + 65536) * 500) >> 16;  // 0..1000
  int32_t wave[kStanceSlopes] = {};
  for (int k = 0; k < kStanceGround0; ++k) {
    const int env = k < 4 ? 550 + k * 150 : (kStanceGround0 - k) * 1000 / (kStanceGround0 - 4);
    const int32_t pw = ph + 14000 - k * kIdleWaveSpatial;
    const int32_t sw =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
    wave[k] = A(static_cast<int32_t>(
        (static_cast<int64_t>(sw) * kIdleWaveAmp * env / 1000) >> 16));
  }
  if (extra_wave != nullptr)
    for (int k = 0; k < kStanceSlopes; ++k) wave[k] += extra_wave[k];
  const int32_t rise =
      apply_stance(g, 1000, A((breath * kIdleDeepen) / 1000), wave,
                   stance_override);
  const int32_t sh =
      zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph + 30000) & 0xFFFF)}).raw;
  for (int k = 0; k <= 2; ++k) {
    g.q[kBSpine0 + k] =
        quat_mul(g.q[kBSpine0 + k], quat_y(A((sh * kIdleHeadSway) >> 16)));
  }
  g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + wave[1] + wave[2] +
                                A((breath * kIdleBreathLift) / 1000)),
                         quat_y(2 * A((sh * kIdleHeadSway) >> 16)));
  zc::quat16 snacc = zc::quat16_identity();
  for (int j = 0; j < kStanceGround0; ++j)
    snacc = quat_mul(snacc, g.q[kBSpine0 + j]);
  for (int k = kStanceGround0; k <= kStanceGround1; ++k) {
    snacc = quat_mul(snacc, g.q[kBSpine0 + k]);
    const int32_t psn = ph - (k - kStanceGround0) * kIdleSnakeSpatial - 4000;
    const int32_t ss =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>(psn & 0xFFFF)}).raw;
    {
      const zc::quat16 W = quat_y(A((ss * kIdleSnakeAmp) >> 16));
      const zc::quat16 L = quat_mul(quat_mul(quat_conj(snacc), W), snacc);
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], L);
      snacc = quat_mul(snacc, L);
    }
    const int32_t pt = ph - 9000 - (k - kStanceGround0) * 5400;
    const int32_t sr =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>(pt & 0xFFFF)}).raw;
    const zc::quat16 tq = quat_x(A((sr * kIdleTorsion) >> 16));
    g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], tq);
    snacc = quat_mul(snacc, tq);
  }
  const int32_t sway = A((st * kIdleTailSway) >> 16);
  for (int k = kStanceGround1 + 1; k < kSpineBones; ++k) {
    const int reach = ((k - kStanceGround1 - 1) * 1000) / (kSpineBones - kStanceGround1 - 1);
    g.q[kBSpine0 + k] = quat_mul(
        g.q[kBSpine0 + k],
        quat_y(static_cast<int32_t>((static_cast<int64_t>(sway) * (400 + reach)) / 1000)));
  }
  g.tail_rest(kBladeSplay + A((st * 900) >> 16), kBladeRise + A((s * 500) >> 16));
  return rise;
}

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
    apply_idle_pupil_gaze(g, f);
    g.write(c, f);
    // the computed root rise that keeps the belly planted (see apply_stance).
    // ROOT CHANNEL UNITS ARE fx16 METRES -- the first pass wrote plain mm
    // here, which is 1/65536 of a mm once decoded: the "bob" never existed.
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}

// Diagnostic-only pupil/stripe sweep. The BODY and HEAD are frozen at idle
// key zero so any change in the rendered eye belongs to the authored gaze and
// its elastic boundary-following stripe, not to camera or neck motion.
inline zc::Clip build_pupil_proof() {
  const zc::Clip idle = build_idle();
  zc::Clip c;
  c.slot_id = kSlotPupilProof;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kPupilProofKeys);
  c.root.assign(static_cast<size_t>(kPupilProofKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kPupilProofKeys) * kBoneCount,
                 zc::quat16_identity());
  static const Key kSide[] = {
      {0, 0}, {kPupilProofRestEndKey, 0},
      {kPupilProofUpKey, kPupilGlanceA16},
      {kPupilProofUpHoldEndKey, kPupilGlanceA16},
      {kPupilProofDownKey, -kPupilGlanceA16},
      {kPupilProofDownHoldEndKey, -kPupilGlanceA16},
      {kPupilProofReturnKey, 0}, {kPupilProofKeys - 1, 0}};
  static const Key kLift[] = {
      {0, 0}, {kPupilProofRestEndKey, 0},
      {kPupilProofUpKey, kPupilGlanceLiftA16},
      {kPupilProofUpHoldEndKey, kPupilGlanceLiftA16},
      {kPupilProofDownKey, -kPupilGlanceLiftA16},
      {kPupilProofDownHoldEndKey, -kPupilGlanceLiftA16},
      {kPupilProofReturnKey, 0}, {kPupilProofKeys - 1, 0}};
  for (int f = 0; f < kPupilProofKeys; ++f) {
    Rig g;
    for (int b = 0; b < kBoneCount; ++b) g.q[b] = idle.quats[b];
    apply_pupil_gaze(
        g, curve(kSide, static_cast<int>(sizeof(kSide) / sizeof(Key)), f),
        curve(kLift, static_cast<int>(sizeof(kLift) / sizeof(Key)), f));
    g.write(c, f);
    c.root[f * 3 + 0] = idle.root[0];
    c.root[f * 3 + 1] = idle.root[1];
    c.root[f * 3 + 2] = idle.root[2];
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

    // Direction #9's real anticipation: the shared authored floor spring
    // replaces the old descent-lobe deepen. It lowers and laterally compacts
    // every region, holds, then pays out into the approved wheel.
    const int32_t pre_drop = apply_spring_stance(g, auth, pre);
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
    g.q[kBHead] = quat_z(spring_head_attitude(auth, pre) +
                            (coil_pitch * curl) / 1000);

    // the blades close to the spear line while coiled or straight-diving,
    // and flare as the S returns
    g.tail_rest((kBladeSplay * auth) / 1000 + kBladeSplay / 5 +
                    (pre * kSpringBladeFlare) / 1000,
                (kBladeRise * auth) / 1000, (kBladeUpBias * auth) / 1000);
    g.write(c, f);
    if (!choreo) {
      c.root[f * 3 + 0] = fxm(fwd + (piv_x * curl) / 1000);
      c.root[f * 3 + 1] = fxm(lift + (piv_y * curl) / 1000 + pre_drop);
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
  const int32_t pre_drop = apply_spring_stance(g, auth, pre);
  const int32_t theta = static_cast<int32_t>((static_cast<int64_t>(spin) * 65536) / 1000);
  const uint16_t th16 = static_cast<uint16_t>(theta & 0xFFFF);
  const int32_t sth = zref::fx_sin(zref::angle16{th16}).raw;
  const int32_t cth = zref::fx_cos(zref::angle16{th16}).raw;
  const int32_t piv_x = static_cast<int32_t>((static_cast<int64_t>(kCoilR) * sth) >> 16);
  const int32_t piv_y = kCoilR - static_cast<int32_t>((static_cast<int64_t>(kCoilR) * cth) >> 16);
  ChoreoSample out;
  out.x_mm = fwd + (piv_x * curl) / 1000;
  out.y_mm = lift + (piv_y * curl) / 1000 + pre_drop;
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


// THE SPEAR LOCK -- shared by the planner and by any variant that edits
// apex/spin AFTER planning (the six-salto did, and its committed vector
// still aimed at the PRE-override apex: the probe read its tip 4.6 m from
// the mark; re-locking after overrides is now the law).
//
// THE WEAPON IS THE TAIL TIP (owner, 2026-08-28: "The tip of the tail
// should stab into it, then it should stop. Right now I think you
// confused the middle of Zixxtrixx with the tip of its tail"). The
// trajectory drives the ROOT, and two measured facts (zixx-striketip,
// committed) place the weapon relative to it: the blade tip leads the
// NOSE by kAtkTipReachMm along the committed line (within ~1 deg), and
// the nose rides exactly kBodyY above the root (bone 0's joint sits at
// (0, kBodyY)). The first cut stopped the ROOT on the intercept, so the
// tip had already passed ~3.9 m through the target ("goes way through it
// before it stops"), and the second cut ignored the kBodyY carry and
// struck 1.07 m high. Law: the ROOT stops where the TIP arrives at the
// intercept and buries the declared kAtkStickDepth -- a stab that lands
// and plants, never a pass-through, never a weightless surface stop.
inline void zixx_plan_lock_spear(zc::AttackPlan& p,
                                 int32_t apex_limit_mm = kAtkApexLift) {
  // the committed AIM line, apex -> intercept
  p.spear_dx_mm = p.intercept_x_mm - p.apex_fwd_mm;
  p.spear_dy_mm = p.intercept_y_mm - p.apex_mm;
  const int32_t back = kAtkTipReachMm - kAtkStickDepth;
  int64_t sx0 = p.spear_dx_mm, sy0 = p.spear_dy_mm;
  int32_t sl = static_cast<int32_t>(
      zref::isqrt_u64(static_cast<uint64_t>(sx0 * sx0 + sy0 * sy0)));
  if (sl < back + kAtkPlungeMinMm) {
    // the commit point must sit at least the animal's own reach from the
    // intercept, or the TIP starts past the target: raise the apex (dx
    // kept, the dive steepens) until the aim line is long enough
    const int64_t need = back + kAtkPlungeMinMm;
    const int64_t dx2 = sx0 * sx0;
    int32_t dyn = static_cast<int32_t>(need);
    if (need * need > dx2)
      dyn = static_cast<int32_t>(
          zref::isqrt_u64(static_cast<uint64_t>(need * need - dx2)));
    const int32_t raised_apex = p.intercept_y_mm + dyn;
    // Generic attacks may not borrow slot 48's 24 m exception.  If a target
    // already sits too near the 12 m ceiling, approaching from above would
    // exceed that limit; commit the same minimum-length line from below
    // instead of silently raising the whole family into the limit lane.
    p.apex_mm = raised_apex <= apex_limit_mm
                    ? raised_apex
                    : p.intercept_y_mm - dyn;
    if (p.apex_mm > apex_limit_mm) p.apex_mm = apex_limit_mm;
    p.spear_dy_mm = p.intercept_y_mm - p.apex_mm;
    sy0 = p.spear_dy_mm;
    sl = static_cast<int32_t>(
        zref::isqrt_u64(static_cast<uint64_t>(sx0 * sx0 + sy0 * sy0)));
  }
  // the ROOT's terminus: intercept, minus the tip lead along the aim
  // line, minus the nose's kBodyY carry -- the plunge line the root
  // travels is the aim line offset by those two constants, so the TIP
  // crosses the intercept exactly at the impact key
  if (sl > back) {
    const int32_t rex = p.intercept_x_mm -
                        static_cast<int32_t>(sx0 * back / sl);
    const int32_t rey = p.intercept_y_mm -
                        static_cast<int32_t>(sy0 * back / sl) - kBodyY;
    p.spear_dx_mm = rex - p.apex_fwd_mm;
    p.spear_dy_mm = rey - p.apex_mm;
  }
  // plunge duration from its length (t^2 law, ~110 mm/key^2 at T=10)
  const int64_t sx = p.spear_dx_mm, sy = p.spear_dy_mm;
  const int32_t slen2 = static_cast<int32_t>(
      zref::isqrt_u64(static_cast<uint64_t>(sx * sx + sy * sy)));
  int32_t pk = slen2 / 1100;
  if (pk < 6) pk = 6;
  if (pk > 14) pk = 14;
  p.plunge_keys = static_cast<uint16_t>(pk);
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
  // Lead the target to the actual impact key.  Anticipation, release, the full
  // coil/unroll and the complete plunge all elapse after the trigger; the old
  // `coil + unroll + half plunge` estimate omitted 28 prep keys and half the
  // plunge, so a 120 mm/key target was 3.48 m beyond the committed intercept.
  // Spear locking can change the discrete plunge duration, so iterate that
  // small 6..14-key fixed point deterministically before freezing the plan.
  const auto configure_locked_intercept = [&]() {
    // The generic family stays under the approved 12 m ceiling.  Slot 48 is
    // the sole 24 m limit exception and explicitly passes its larger ceiling
    // when it re-locks the overridden plan.
    p.apex_mm = p.intercept_y_mm > 0 ? p.intercept_y_mm + 2000 : 8000;
    if (p.apex_mm > kAtkApexLift) p.apex_mm = kAtkApexLift;
    if (p.apex_mm < 3000) p.apex_mm = 3000;
    p.apex_fwd_mm = p.intercept_x_mm / 3;
    zixx_plan_lock_spear(p);
    // Spin appetite follows the final locked apex, not the pre-lock guess.
    int32_t turns = p.apex_mm / 3000;
    if (turns < 1) turns = 1;
    if (turns > 5) turns = 5;
    p.spin_mturns = turns * 1000;
  };
  for (int iteration = 0; iteration < 16; ++iteration) {
    const int32_t lead = p.compress_keys + p.compress_hold_keys +
                         p.release_keys + p.coil_keys + p.unroll_keys +
                         p.plunge_keys;
    const int32_t intercept_x = tgt_x_mm + tgt_vx_mmk * lead;
    const int32_t intercept_y = tgt_y_mm + tgt_vy_mmk * lead;
    const uint16_t prior_plunge = p.plunge_keys;
    const bool intercept_stable =
        p.intercept_x_mm == intercept_x && p.intercept_y_mm == intercept_y;
    p.intercept_x_mm = intercept_x;
    p.intercept_y_mm = intercept_y;
    configure_locked_intercept();
    if (intercept_stable && p.plunge_keys == prior_plunge) break;
  }
  return p;
}

// the plan's per-key root sample -- the general form of
// attack_choreo_sample. Golden preset: the authored tables verbatim.
inline ChoreoSample zixx_plan_sample(const zc::AttackPlan& p, int key) {
  if (p.preset_golden) return attack_choreo_sample(key);
  ChoreoSample out{0, 0, 0};
  const int tc = p.compress_keys;
  const int th = tc + p.compress_hold_keys;
  const int t0 = th + p.release_keys;                  // launch key
  const int t1 = t0 + p.coil_keys;                     // commit (apex)
  const int t2 = t1 + p.unroll_keys;                   // spear locked
  const int t3 = t2 + p.plunge_keys;                   // impact
  if (key <= t0) {
    // The same whole-body spring root used by the local pose: descend, hold,
    // and release to zero before flight. There is no hidden runtime physics.
    int32_t amount = 0;
    if (key < tc) {
      if (tc <= 1) {
        amount = 1000;
      } else {
        const int64_t u = (static_cast<int64_t>(key) * 1000) / (tc - 1);
        amount = static_cast<int32_t>(u * u * (3000 - 2 * u) / 1000000);
      }
    } else if (key < th) {
      amount = 1000;
    } else if (p.release_keys > 0) {
      const int64_t u = (static_cast<int64_t>(key - th) * 1000) / p.release_keys;
      amount = 1000 - static_cast<int32_t>(u * u * (3000 - 2 * u) / 1000000);
    }
    out.y_mm = spring_root_drop(amount);
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
    // WHOLE turns only in the coil (RUN 1730, the six-salto flicker): the
    // spear-alignment FRACTION belongs to the unroll, which starts from
    // the whole-turn count -- the first cut spun the coil to whole+frac
    // and the unroll restarted at whole, a backward snap of up to a full
    // turn at the apex that the presentation interpolator rendered as the
    // owner's "flickers back and forth". Theta is one continuous,
    // explicitly accumulated function of the key across every phase now.
    const int32_t whole_mt = (p.spin_mturns / 1000) * 1000;
    out.theta = static_cast<int32_t>(
        (static_cast<int64_t>(whole_mt) * sm / 1000) * 65536 / 1000);
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

    // the tumble's warped angle, PRE-computed for the head's aim below
    // (the tumble itself recomputes it identically later)
    const int32_t theta_u_pre =
        static_cast<int32_t>((static_cast<int64_t>(f) << 16) / kFallKeys);
    const int32_t theta_pre =
        theta_u_pre + static_cast<int32_t>(
                          (static_cast<int64_t>(kFallTumbleWarp) *
                           zref::fx_sin(zref::angle16{static_cast<uint16_t>(theta_u_pre & 0xFFFF)}).raw) >>
                          16);

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
      // THE HEAD FIGHTS (run 0326 owner: "try making it more alive"): a
      // falling creature's head is the most alive thing about it -- it
      // looks where it is going. Through the head-down half of the tumble
      // the skull pitches toward the ground (counter to the body's spin),
      // eased by the tumble's own sine so the glance arrives and releases
      // slowly. Layered UNDER the loose loll, not replacing it.
      const int32_t aim =
          (zref::fx_sin(zref::angle16{static_cast<uint16_t>(theta_pre & 0xFFFF)}).raw *
           kFallHeadAim) >> 16;
      g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + aim + ((s1 * (kFallNeckAmp / 2)) >> 16)),
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
                      16) +
        static_cast<int32_t>(
            (static_cast<int64_t>(kFallTumbleWarp2) *
             zref::fx_sin(zref::angle16{static_cast<uint16_t>((2 * theta_u + 9000) & 0xFFFF)}).raw) >>
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
// RE-AUTHORED, run 0326 owner redirect: "just a weird twitch. Should be
// more exaggerated and impactful." Longer (28 keys), the recoil violent
// (deepen and jerk half again bigger), the WHOLE BODY shoved back along
// the blow, and the ring-out settles through two slow decaying
// overshoots -- recoil, not vibration.
constexpr int kHitKeys = 50;  // long enough for the delayed tail envelope to
                              // finish naturally and land on exact rest; the
                              // violent onset remains two keys sharp
constexpr int32_t kHitDeepen = 950;    // recoil compression through the
                                       // struck section. 700 -> 950: the
                                       // "face eaten at 900" verdict was
                                       // recorded on the OLD tight hook;
                                       // the reconstructed open climb nests
                                       // far less (probe), so the strength
                                       // the -85 mm shove trade gave away
                                       // comes back here. Worst key
                                       // rendered + probed (allowance 390).
constexpr int32_t kHitHeadJerk = 9000; // ...the head whips back-up hard
constexpr int32_t kHitSway = 5200;     // and away
constexpr int32_t kHitShoveMm = 300;   // the body is MOVED by the blow --
                                       // a full tube-width at 240p, not a
                                       // local neck flinch
// THE SHOCKWAVE (RUN 1730, owner: "more of the snake should be affected").
// The blow is a WAVE, not a local flinch: every joint receives the same
// envelope DELAYED by its distance from the struck point and DECAYED as it
// travels, so the impact visibly runs head -> body -> grounded run -> tail
// and the far end still knows it was hit. Three lanes, all house machinery:
// the front rides apply_stance's wave lane (root-compensated, belly stays
// planted); the grounded run ripples LATERALLY by world-vertical
// conjugation (a yaw about world up cannot dig the belly -- the idle
// snake's own trick); the tail whips on the sway lane, biggest at the tip.
constexpr int32_t kShockLagMk = 520;      // milli-keys of delay per joint
constexpr int32_t kShockDecay = 420;      // per-mille amplitude lost across
                                          // the front chain (the rest
                                          // reaches the grounded run)
constexpr int32_t kShockFrontAmp = 4800;  // pitch pulse through the raised
                                          // front (angle16 at the head end)
constexpr int32_t kShockGroundAmp = 3300; // lateral ripple, grounded joints
constexpr int32_t kShockTailAmp = 7600;   // the tail's delayed whip
// THE IMPACT SHAPE. The old +F/-2F/+F triple was mathematically tidy but
// visually self-cancelled: it made a tiny crease and deliberately returned the
// next section to its untouched line. This profile is authored as WORLD slope
// displacement at the first seven segments. The first two make the struck
// hairpin, the next three are carried bodily out of line, and the last two pay
// the bend back into the travelling shockwave. It is a real displaced length
// of animal rather than an isolated hinge; every value remains an eye-tunable
// art control.
constexpr int32_t kHitFoldSlope[kStanceSlopes] = {
    0, 8200, -12400, -8600, 5200, 3400, 1500, 400, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0};
static const Key kFoldEnv[] = {{0, 0}, {1, 1000}, {2, 940}, {4, 380}, {7, 0}};
constexpr int kFoldEnvN = static_cast<int>(sizeof(kFoldEnv) / sizeof(Key));

// the delayed-envelope sampler: curve() at milli-key resolution, so a
// per-joint lag under one key still lands between keys instead of stepping
inline int curve_mk(const Key* k, int n, int fmk) {
  if (fmk <= k[0].f * 1000) return k[0].v;
  for (int i = 0; i + 1 < n; ++i) {
    const int a = k[i].f * 1000, b = k[i + 1].f * 1000;
    if (fmk >= a && fmk <= b) {
      const int span = b - a;
      if (span <= 0) return k[i + 1].v;
      return k[i].v +
             static_cast<int>((static_cast<int64_t>(k[i + 1].v - k[i].v) *
                                   (fmk - a) + span / 2) / span);
    }
  }
  return k[n - 1].v;
}
// the impact envelope, shared by the hit and the directional damages:
// sharp violent onset (peak inside three keys), then a long loose damped
// ring-out -- amplitudes fall 1000 / -320 / 150 / -70 / 28 on a slowing
// period, which is wobble, not vibration, and lands exactly on rest.
static const Key kImpactEnv[] = {{0, 0},   {1, 720},  {3, 1000}, {6, 470},
                                 {10, -320}, {15, 150}, {21, -70}, {28, 28},
                                 {34, -10},  {39, 0}};
constexpr int kImpactEnvN = static_cast<int>(sizeof(kImpactEnv) / sizeof(Key));
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
constexpr int kBalKeys = 224;  // long, effortful stunt: rise, fight, buckle,
                               // flop, then a loose complete recovery
// The raised silhouette is an authored L, not a rigid spear. Eleven upper
// segments struggle toward vertical, three make the weight-bearing elbow, and
// the final five lie almost flat as a broad supporting tail. These are complete
// segment directions, deliberately named and eye-tunable.
constexpr int32_t kBalRaisedSlope[kStanceSlopes] = {
    15000, 16500, 14300, 16600, 14800, 16800, 14400, 16400, 14600, 16400,
    14800, 13600, 9500, 4200, 1350, 1200, 1100, 1000, 900};
constexpr int kBalSupport0 = 14;          // five body segments rest near-flat
constexpr int kBalSupportBeginKey = 77;
constexpr int kBalSupportEndKey = 140;
constexpr int kBalImpactBeginKey = 157;
constexpr int kBalImpactEndKey = 166;
constexpr int kBalImpactLeadPresentationTicks = 1;  // baked 156.5 contact lead
// Committed 3D declarations for the accepted raised-L contact. These compare
// actual posed vertices; they never generate the shape or root curve.
constexpr int32_t kBalSupportBiteMm = 40;
constexpr int32_t kBalSupportHoverMm = 20;
constexpr int32_t kBalImpactBiteMm = 70;
constexpr int32_t kBalImpactContactMinMm = 25;
constexpr int32_t kBalMinShapeChordTravelMm = 5;
constexpr int32_t kBalMaxStationStepMm = 320;
constexpr int32_t kBalWobble = 3900;      // travelling primary struggle
constexpr int32_t kBalWobble2 = 2400;     // slow incommensurate body answer
constexpr int32_t kBalImpactSink = 22;     // mm: the flop's authored bite
constexpr int32_t kBalFinFlare = 2600;     // fins flare wide for balance
constexpr int32_t kBalBladeUpBias = 9000;  // body bears weight; fan clears terrain
// THE BUCKLING TOPPLE (RUN 1939, item 6; owner: "Tail balance is wonky.
// Not wobbly enough. Particularly the fall -- it just falls rigid like
// it's a stick."). The falling flail's cure carried across: a toppling
// body loses its structure PROGRESSIVELY -- the head end gives way first
// and the planted base last (kBalBuckleLagK keys of spread), each section
// whips PAST the corpse slope (kBalOvershoot) and settles back, and the
// flop's shock travels tailward on the shared impact envelope
// (kBalRippleAmp) so nothing arrives together. Before the fall, the fight
// gets a second slower wobble term and a one-way LEAN the corrections no
// longer pull back (kBalLeanA16 per joint) -- the loss reads as a loss.
// All slow, loose, low-frequency: wobble, not jitter.
constexpr int kBalBuckleLagK = 6;        // keys between head-end and base give
                                         // (10 dug the still-steep foot -819:
                                         // the fork height curve and the base's
                                         // delayed clock must overlap)
constexpr int32_t kBalOvershoot = 2600;  // slope past the corpse at the whip
constexpr int32_t kBalLeanA16 = 3000;    // growing headward failure bend
constexpr int32_t kBalBreath = 700;      // the life layer under the stunt
constexpr int32_t kBalRippleAmp = 2200;  // the flop's travelling ring-out
// LOOK-AROUND, slot 8: the head-aim rig capability performed — Zixx looks
// left, up, right, down, unhurried, while the body idles quietly. The aim
// lives on the HEAD BONE (the head is bone 0, the ROOT — a joint rotation
// cannot move it; kBHead is the one bone that aims the skull), with the
// first two neck joints following at kLookNeckFollow to soften the turn
// into the tube (origin chosen by eye off renders — a tube has no
// anatomical neck to derive).
constexpr int kLookKeys = 192;  // 128 -> 192 (run 0326: "a bit twitchy...
                                // too rigid" -- the same itinerary a half
                                // slower, on the idle's living body)
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
  // THE SHOCKWAVE build (see the constants above). e is the envelope at
  // the struck point; ek[j] is the same envelope arriving at joint j late
  // and lighter -- the whole serpent reacts, in order, and the recovery is
  // slow and loose (the owner's structure: violent onset, yielding return).
  for (int f = 0; f < kHitKeys; ++f) {
    const int32_t e = curve(kImpactEnv, kImpactEnvN, f);
    int32_t ek[kStanceSlopes];
    for (int j = 0; j < kStanceSlopes; ++j)
      ek[j] = curve_mk(kImpactEnv, kImpactEnvN, f * 1000 - j * kShockLagMk);
    Rig g;
    g.reset();
    // FRONT: the travelling pitch pulse rides the wave lane (exact root
    // compensation -- the belly never feels it). Negative = the climb
    // momentarily rears back, and the pulse runs on into the dive.
    int32_t wave[kStanceSlopes] = {};
    for (int k = 1; k < kStanceGround0; ++k) {
      const int32_t dec = 1000 - (k * kShockDecay) / (kStanceGround0 - 1);
      wave[k] = -(ek[k] * ((kShockFrontAmp * dec) / 1000)) / 1000;
    }
    // THE IMPACT SHAPE: an asymmetric hairpin with a carried downstream
    // displacement, authored in segment-slope space so the side silhouette
    // changes at once and then hands motion to the delayed whole-body wave.
    const int32_t fold = curve(kFoldEnv, kFoldEnvN, f);
    for (int k = 1; k < kStanceGround0; ++k)
      wave[k] += (fold * kHitFoldSlope[k]) / 1000;
    const int32_t rise = apply_stance(g, 1000, (e * kHitDeepen) / 1000, wave);
    // the head jerks BACK-UP and aside first (it is the struck end), then
    // settles on the same envelope
    g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + (e * kHitHeadJerk) / 1000),
                           quat_y((e * kHitSway) / 1000));
    // GROUNDED RUN: the pulse continues as a LATERAL ripple -- a yaw about
    // world vertical cannot dig the belly (the idle snake's conjugation)
    zc::quat16 snacc = zc::quat16_identity();
    for (int j = 0; j < kStanceGround0; ++j)
      snacc = quat_mul(snacc, g.q[kBSpine0 + j]);
    for (int k = kStanceGround0; k <= kStanceGround1; ++k) {
      snacc = quat_mul(snacc, g.q[kBSpine0 + k]);
      const zc::quat16 W = quat_y((ek[k] * kShockGroundAmp) / 1000);
      const zc::quat16 L = quat_mul(quat_mul(quat_conj(snacc), W), snacc);
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], L);
      snacc = quat_mul(snacc, L);
    }
    // TAIL: the whip arrives last and biggest at the tip
    for (int k = kStanceGround1 + 1; k < kSpineBones; ++k) {
      const int reach =
          400 + ((k - kStanceGround1 - 1) * 600) / (kSpineBones - kStanceGround1 - 2);
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k],
          quat_y((ek[k - 1] * ((kShockTailAmp * reach) / 1000)) / 1000));
    }
    // the blades snap open when the wave REACHES the fork, not at impact
    g.tail_rest(kBladeSplay + (ek[kStanceSlopes - 1] * 1400) / 1000, kBladeRise);
    g.write(c, f);
    // the shove: the blow MOVES the animal backward, and it recovers with
    // the same envelope -- displacement is what sells impact at 240p
    c.root[f * 3 + 0] = fxm(-(e * kHitShoveMm) / 1000);
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
    // phase 2: the collapse, IN STAGES (run 0326 owner: "unusually good
    // for a first attempt, but needs to be more organic"). Strength goes
    // out unevenly: the front gives, the body half-CATCHES itself, then a
    // sudden give takes the rest, and the settle is slow. Never a
    // constant-velocity keel.
    static const Key kGive[] = {{0, 0},   {14, 0},   {21, 430}, {26, 400},
                                {30, 370}, {35, 720}, {39, 880}, {50, 1000},
                                {95, 1000}};
    constexpr int kGiveN = static_cast<int>(sizeof(kGive) / sizeof(Key));
    const int32_t drain = curve(kGive, kGiveN, f);
    int64_t rear_sin = 0;
    int32_t prev = 0;
    for (int k = 0; k < kStanceSlopes; ++k) {
      int64_t d = kStanceSlope[k];
      if (k >= kStanceDescend0 && k <= kStanceDescend1 && shud != 0) {
        if (d <= 16384) d += (d * shud) / 1000;
        else d -= ((32768 - d) * shud) / 1000;
      }
      d += ((kCorpseSlope[k] - d) * drain) / 1000;
      // phase 4, k52..78: the last tail curl, risen slowly, released
      // slower -- plus the LAST-SETTLE beat at k82..93 (see the head): the
      // tail-tip lifts once more, small, and drops
      if (k > kStanceGround1) {
        const int32_t curl_env = ss1000(f, 52, 62) - ss1000(f, 62, 78) +
                                 (ss1000(f, 82, 86) - ss1000(f, 86, 93)) / 3;
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
    const int32_t roll = (kDeathRoll * ss1000(f, 32, 60)) / 1000;
    // a rolling tube's centre stays one radius up: the rigid roll about
    // the fixed ground axis drops it by h(1-cos), so the root rises to
    // match (the probe read the front quarter -175 without it)
    const int32_t roll_lift = (kDeathRollLift * ss1000(f, 32, 60)) / 1000;
    // the head: the attitude HOLDS (draining it to zero pitched the
    // 218 mm ball 60 deg under and the probe read -226); the dying droop
    // is a small authored delta, and the last shake rides on top.
    // THE LAST SETTLE (organic pass): one small beat after the body looks
    // finished -- the head shifts once, the tail-tip lifts and drops --
    // "this was alive a moment ago".
    const int32_t last = ss1000(f, 82, 86) - ss1000(f, 86, 93);
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude - (1400 * drain) / 1000 + headshake / 2 -
               (last * 700) / 1000),
        quat_y(headshake + (last * 900) / 1000));
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

// Slot 7 - TAIL-BALANCE, the idle stunt. Gather onto a broad length of tail,
// rise into a difficult L that never becomes a rigid spear, lose the fight,
// buckle flat forward with an authored ground bite, and get back up into the
// canonical S. The root curve lays six tapered body regions across five tail
// segments onto terrain; the raised fan is visibly incapable of being the foot.
// Two phase-lagged waves keep changing the upper-body shape over that base.
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
    //   gather   k0..28    weight spreads into the supporting tail
    //   rise     k28..77   slopes -> the wobbling raised L
    //   balance  k77..119  body-shape waves keep travelling (the fight)
    //   lose     k119..140 corrections diverge into a headward lean
    //   topple   k140..157 slopes -> flat, accelerating; IMPACT at 157
    //   rise2    k165..196 back up into the S
    //   settle   k196..223 exact canonical S for the loop
    const int32_t up = ss1000(f, 28, 77);           // stance -> vertical
    const int32_t over = (ss1000(f, 140, 157) * ss1000(f, 140, 157)) / 1000;
    const int32_t recover = ss1000(f, 165, 196);    // flat -> stance
    // THE FIGHT is a travelling shape change, not one rigid-body sway. Each
    // upper segment samples two slow waves at its own phase below; the tail
    // authority fades to zero before the five-segment ground support. A one-way
    // headward bend grows from key 122 so the corrections visibly stop winning.
    const int32_t fight = ss1000(f, 30, 77);
    const int32_t struggle = 300 + (700 * fight) / 1000;
    const int32_t lean = ss1000(f, 122, 142);
    // the life layer: a slow breath through the raised stretch, damped
    // while standing (the effort holds the breath), full at the ends
    const int32_t phl = f * (65536 / kBalKeys);
    const int32_t s_breath =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((phl * 2) & 0xFFFF)}).raw;
    const int32_t breathw = 300 + (700 * (1000 - up)) / 1000;
    int64_t sum_sin = 0, sum_cos = 0;
    int32_t prev = 0;
    int32_t head_fight_wave = 0;
    for (int k = 0; k < kStanceSlopes; ++k) {
      // stance -> the authored raised L -> flat corpse -> stance. The tail
      // target itself is horizontal; support is several body joints, never a
      // fork-tip balancing trick.
      int64_t d = kStanceSlope[k];
      d += ((kBalRaisedSlope[k] - d) * up) / 1000;
      // THE BUCKLING TOPPLE (see the knobs): each joint collapses to the
      // corpse pose on its OWN clock -- head end first, base last -- with
      // an overshoot whip past the target, so the fall is a body losing
      // its structure, never a rotating rod
      const int fk = f - (k * kBalBuckleLagK) / (kStanceSlopes - 1);
      const int32_t b1 = ss1000(fk, 140, 157);
      const int32_t over_k = (b1 * b1) / 1000;
      // the whip stays OFF the planted foot (last three joints): an
      // overshoot there drives the fan through the dirt (probe, -819)
      const int32_t osh = k < kStanceSlopes - 3
                              ? ss1000(fk, 150, 158) - ss1000(fk, 158, 172)
                              : 0;
      const int64_t target = kCorpseSlope[k] + (kBalOvershoot * osh) / 1000;
      d += ((target - d) * over_k) / 1000;
      d += ((kStanceSlope[k] - d) * recover) / 1000;  // and the S returns
      // Two slow, spatially lagged waves keep changing the complete upper-body
      // shape through gather, rise and fight. Their authority fades across the
      // elbow and is exactly zero on the broad ground support.
      if (f >= 12 && f < 165 && k < kBalSupport0) {
        const int32_t live =
            500 + ((kBalSupport0 - 1 - k) * 500) / (kBalSupport0 - 1);
        const int32_t ph1 = f * (65536 / 47) - k * 5600;
        const int32_t ph2 = f * (65536 / 103) + k * 3100 + 17000;
        const int32_t sw1 = zref::fx_sin(
            zref::angle16{static_cast<uint16_t>(ph1 & 0xFFFF)}).raw;
        const int32_t sw2 = zref::fx_sin(
            zref::angle16{static_cast<uint16_t>(ph2 & 0xFFFF)}).raw;
        int32_t shape_wave =
            static_cast<int32_t>((static_cast<int64_t>(sw1) * kBalWobble) >> 16) +
            static_cast<int32_t>((static_cast<int64_t>(sw2) * kBalWobble2) >> 16);
        shape_wave = (shape_wave * live) / 1000;
        shape_wave = (shape_wave * struggle) / 1000;
        shape_wave = (shape_wave * (1000 - over_k)) / 1000;
        shape_wave = (shape_wave * (1000 - recover)) / 1000;
        d += shape_wave;
        if (k == 0) head_fight_wave = shape_wave;

        int32_t failure = (kBalLeanA16 * lean) / 1000;
        failure = (failure * (kBalSupport0 - k)) / kBalSupport0;
        failure = (failure * (1000 - over_k)) / 1000;
        failure = (failure * (1000 - recover)) / 1000;
        d += failure;
      }
      // the breath rides the raised stretch
      if (k >= 1 && k <= 9)
        d += (((((s_breath * kBalBreath) >> 16) * breathw) / 1000) *
              (1000 - recover)) /
             1000;
      // THE IMPACT RIPPLE: the flop's shock travels tailward on the
      // shared impact envelope and rings out -- absorb, follow through,
      // settle loosely
      if (f >= 157 && k < kStanceSlopes - 3) {  // the foot never ripples
        const int32_t ek = curve_mk(kImpactEnv, kImpactEnvN,
                                    (f - 157) * 1000 - k * kShockLagMk);
        d += (static_cast<int64_t>(ek) * kBalRippleAmp * (1000 - recover)) /
             1000000;
      }
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
    // The raised-L pass puts the tapered BODY support on terrain, not the fan
    // tips. The old +580 plateau held bones 14..19 roughly 750 mm in the air.
    // This curve now eases the fork downward as the five-segment support lays
    // out, keeps that broad base planted through the fight and topple, then
    // hands the already-nearby corpse height into the loose recovery.
    static const Key kBalFork[] = {
        {0, 0},      {28, 0},      {45, 0},      {56, -50},
        {66, -172},  {77, -255},   {140, -255},  {148, -255},
        {154, -250}, {157, -220},  {160, -240},  {163, -250},
        {166, -240}, {172, -120},  {179, -62},   {185, -34},
        {190, -16},  {196, 0},     {223, 0}};
    // run 0326: the get-up used to hand the fork straight back to +30 by
    // k130, which lifted the WHOLE half-flat body off the dirt (probe: minY
    // +65 at k128 -- a 200 mm-class push-up hop on the render). The keys
    // above keep the belly kissing the ground while the S re-forms.
    constexpr int kBalForkN = static_cast<int>(sizeof(kBalFork) / sizeof(Key));
    const int32_t base_fork =
        kBodyY - static_cast<int32_t>((segL * base_sin) >> 16);
    const int32_t fork_y0 =
        kBodyY - static_cast<int32_t>((segL * sum_sin) >> 16);
    const int32_t stunt = f < 165 ? ss1000(f, 28, 70) : 1000 - recover;
    const int32_t dx = static_cast<int32_t>((segL * (sum_cos - base_cos)) >> 16);
    int32_t root_x = -(dx * stunt) / 1000;
    int32_t root_y = base_fork + curve(kBalFork, kBalForkN, f) - fork_y0;
    // the IMPACT: the front third slaps the dirt -- a brief authored bite
    // (kBalImpactSink) that releases over four keys. Declared, probed.
    if (f >= 157 && f < 167) {
      root_y -= (kBalImpactSink * (1000 - ss1000(f, 160, 166))) / 1000;
    }
    // the head: tucks with the effort on the way up, COUNTERS the wobble
    // while the fight is on (the strongest aliveness signal: the animal
    // visibly correcting), looks AT the arriving ground through the
    // topple, jams nose-first at the flop, and wakes with a small loose
    // settle as the S returns
    const int32_t head_fight = -head_fight_wave / 2;
    const int32_t see = (over * 2600) / 1000;  // the ground is coming
    const int32_t wake = ss1000(f, 168, 176) - ss1000(f, 176, 192);
    const int32_t head_stunt =
        (up * -2200) / 1000 + head_fight + see + (over * 1800) / 1000;
    g.q[kBHead] =
        quat_z(kHeadAttitude + (head_stunt * (1000 - recover)) / 1000 -
               (wake * 900) / 1000);
    // The BODY tail is the foot. The fan lifts just clear of terrain during
    // the raised L so it cannot masquerade as point support, flares with the
    // effort, then returns exactly to its canonical authored rest by key 223.
    const int32_t blade_support = up * (1000 - recover) / 1000;
    const int32_t blade_bias =
        kBladeUpBias + ((kBalBladeUpBias - kBladeUpBias) * blade_support) / 1000;
    const int32_t blade_anim = 1000 - recover;
    const int32_t blade_splay_offset =
        (((kBalFinFlare * fight) / 3000 - (600 * over) / 1000) *
         blade_anim) /
        1000;
    const int32_t blade_rise_offset =
        (-(kBladeRise * up) / 1400 - (600 * over) / 1000) * blade_anim /
        1000;
    g.tail_rest(kBladeSplay + blade_splay_offset,
                kBladeRise + blade_rise_offset, blade_bias);
    g.write(c, f);
    c.root[f * 3 + 0] = fxm(root_x);
    c.root[f * 3 + 1] = fxm(root_y);
  }
  c.events = {{157, zc::kEvFoot, 4}};  // the flop lands
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
  // the 128-key itinerary retimed onto 192 keys -- every move a half
  // slower, the holds longer (the twitch was pace, not path)
  static const Aim kAim[] = {
      {0, 0, 0},        {15, 0, 0},
      {33, kLookYaw, 400},   {60, kLookYaw, 0},          // left, hold
      {81, 2000, kLookPitchUp}, {102, 2400, kLookPitchUp},  // up, hold
      {123, -kLookYaw, 600},  {147, -kLookYaw, 0},        // right, hold
      {162, -800, -kLookPitchDn}, {171, 0, -kLookPitchDn / 2},  // down
      {183, 0, 0},      {191, 0, 0}};
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
    aim_at(f >= 3 ? f - 3 : 0, lyaw, lpitch);
    int32_t follow[kStanceSlopes] = {};
    follow[1] = (lpitch * kLookNeckFollow) / 2000;
    follow[2] = (lpitch * kLookNeckFollow) / 2000;
    // THE IDLE'S LIVING BODY underneath (run 0326: "should look more like
    // the idle animation, with an actually live moving body") -- the full
    // recipe at 800/1000, the neck's pitch-follow riding its wave lane
    const int32_t rise = idle_body(g, ph, 800, follow);
    int32_t yaw, pitch;
    aim_at(f, yaw, pitch);
    // the aim owns the head; a share of the body's nod rides along so the
    // skull never freezes against a breathing neck
    g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + pitch + follow[1] + follow[2]),
                           quat_y(yaw));
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
    // The pupils preserve the previous apparent target for four authored keys
    // while the head begins each gesture, then settle back to centre on the
    // long holds. This is coordinated head-motion lag, not a second itinerary.
    int32_t pupil_yaw, pupil_pitch;
    aim_at(f >= kPupilHeadLagKeys ? f - kPupilHeadLagKeys : 0,
           pupil_yaw, pupil_pitch);
    apply_pupil_gaze(g, pupil_yaw - yaw, pupil_pitch - pitch);
    // blades: idle_body already gave them the idle's play
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}



// ================= run 0326: THE VOCABULARY CLOSE-OUT ======================
// (Fabian: "figure out from there how many more animations we need, then get
// a fable agent started on making them. Zixxtrixx shall be nominally
// complete.") Which donor slot each of these answers, and the engine
// semantics behind the choices, is documented in
// Upheaval/creature/Zixxtrixx/ANIMATION-VOCABULARY.md.
constexpr int kSlotKnock = 20;     // donor knocked2Floor
constexpr int kSlotGetUp = 21;     // donor getUp
constexpr int kSlotHitFloor = 22;  // donor hitFloor (the landing after falling)
constexpr int kSlotDmgRight = 23;  // donor damageRight (slot 5 IS damageFront)
constexpr int kSlotDmgBack = 24;   // donor damageBack
constexpr int kSlotDmgLeft = 25;   // donor damageLeft
constexpr int kSlotDmgTop = 26;    // donor damageTop
constexpr int kSlotRun = 27;       // donor run
constexpr int kSlotDeath1 = 28;    // donor death1 (random pick with death0)
constexpr int kSlotTaunt = 30;     // donor laugh -- the owner's taunt
constexpr int kSlotCorpse = 31;    // donor corpse (the dead body is a clip)

// ---- the knockdown chain --------------------------------------------------
// THE KNOCKED POSE is ONE set of bytes: knocked2Floor and hitFloor END on it
// and getUp STARTS on it, and the seams are declared in the bank so
// compile_creature FAILS if an edit ever splits them.
constexpr int kKnockKeys = 26;
constexpr int kGetUpKeys = 40;
constexpr int kHitFloorKeys = 26;
constexpr int32_t kKnockRoll = -7400;    // ~41 deg onto the flank, AWAY from
                                         // the site lens (the death's lesson:
                                         // dorsal square at the camera reads
                                         // as a magenta smear). Shallower
                                         // than the death's -11600: knocked,
                                         // not dead.
constexpr int32_t kKnockRollLift = 58;   // h*(1-cos41): death's 132@64deg law
constexpr int32_t kKnockFold = 650;      // blades fold most of the way
constexpr int32_t kKnockJolt = 3100;     // the blow's head-snap, key 0..5
                                         // (4200 on the wave lane carried
                                         // the skull 301 mm into the hook;
                                         // softened until the worst key sat
                                         // back in the hit family)
constexpr int32_t kKnockBounceMm = 12;   // the little rebound after the slam
constexpr int32_t kHitFloorBiteMm = 24;  // authored impact bite, keys 9..13

// The shared lying-body composer: the death clip's collapse law factored to
// knobs. drain (stance -> corpse slopes), roll1000 (share of kKnockRoll),
// head_extra (angle16 on top of the drain's own droop), fold1000 (blade
// fold). Writes every spine joint + head + blades; root_mm gets the
// pivot-corrected displacement (rear node planted, rolling-tube lift).
// When roll1000 is 0 the roll compose is SKIPPED outright so a drain-0
// roll-0 frame is BIT-IDENTICAL to the canonical rest pose (seam law).
inline void lying_frame(Rig& g, int32_t drain, int32_t roll1000,
                        int32_t head_extra, int32_t fold1000,
                        int32_t root_mm[3], const int32_t* wave = nullptr) {
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);
  static const int64_t base_rear_sin = [] {
    int64_t acc = 0;
    for (int k = 0; k <= kStanceGround1; ++k)
      acc += zref::fx_sin(zref::angle16{static_cast<uint16_t>(kStanceSlope[k] & 0xFFFF)}).raw;
    return acc;
  }();
  int64_t rear_sin = 0;
  int32_t prev = 0;
  for (int k = 0; k < kStanceSlopes; ++k) {
    int64_t d = kStanceSlope[k];
    d += ((kCorpseSlope[k] - d) * drain) / 1000;
    // the wave lane (run 0326): a per-segment slope delta whose belly cost
    // the rear-node constraint below absorbs EXACTLY -- composing raw neck
    // quats after this function pitched the whole body and the probe caught
    // the grounded run at +94 mm (the hit clip's recorded trap, repeated)
    if (wave != nullptr && wave[k] != 0) d += wave[k];
    const uint16_t a = static_cast<uint16_t>(d & 0xFFFF);
    if (k <= kStanceGround1) rear_sin += zref::fx_sin(zref::angle16{a}).raw;
    const int32_t pitch = static_cast<int32_t>(d) - prev;
    prev = static_cast<int32_t>(d);
    g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
  }
  const int32_t root_y =
      static_cast<int32_t>((segL * (rear_sin - base_rear_sin)) >> 16);
  g.q[kBHead] = quat_z(kHeadAttitude - (1400 * drain) / 1000 + head_extra);
  g.tail_rest((kBladeSplay * (1000 - fold1000 / 2)) / 1000,
              (kBladeRise * (1000 - fold1000)) / 1000);
  if (roll1000 != 0) {
    const int32_t roll = (kKnockRoll * roll1000) / 1000;
    const int32_t roll_lift = (kKnockRollLift * roll1000) / 1000;
    const zc::quat16 rq = quat_x(roll);
    g.q[kBSpine0] = quat_mul(rq, g.q[kBSpine0]);
    int32_t rx, ry, rz;
    quat_rot_vec(rq, kDeathPivotX, -kBodyY - root_y, 0, rx, ry, rz);
    root_mm[0] = kDeathPivotX - rx;
    root_mm[1] = root_y + roll_lift + (-kBodyY - root_y - ry);
    root_mm[2] = -rz;
  } else {
    root_mm[0] = 0;
    root_mm[1] = root_y;
    root_mm[2] = 0;
  }
}

// Slot 20 - KNOCKED2FLOOR. The blow has already landed when the sim cuts to
// this clip (donor semantics), so key 0 IS the impact: a hit-style head
// snap, then the body drains fast onto the flank, one small rebound, and
// the clip HOLDS the knocked pose until getUp cuts in.
inline zc::Clip build_knock() {
  zc::Clip c;
  c.slot_id = kSlotKnock;
  c.interpolate = true;
  c.hold_last = true;  // held down until the state machine says get up
  c.frame_count = static_cast<uint16_t>(kKnockKeys);
  c.root.assign(static_cast<size_t>(kKnockKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kKnockKeys) * kBoneCount, zc::quat16_identity());
  static const Key kJolt[] = {{0, 0}, {1, 900}, {3, 1000}, {6, 260}, {9, 0}, {25, 0}};
  // the final segment is PINNED to zero one key early: curve()'s integer
  // rounding on a falling segment returns 1 (not 0) at the last key, and
  // one leftover millimetre of bounce broke the byte-identical seam to
  // getUp key 0 (the compiler's seam check caught it, run 0326)
  static const Key kBounce[] = {{0, 0}, {17, 0}, {19, 12}, {22, 3}, {24, 0}, {25, 0}};
  constexpr int kJoltN = static_cast<int>(sizeof(kJolt) / sizeof(Key));
  constexpr int kBounceN = static_cast<int>(sizeof(kBounce) / sizeof(Key));
  for (int f = 0; f < kKnockKeys; ++f) {
    Rig g;
    g.reset();
    const int32_t jolt = curve(kJolt, kJoltN, f);
    const int32_t drain = ss1000(f, 2, 15);
    const int32_t roll = ss1000(f, 8, 19);
    // the neck's share of the snap rides the WAVE lane so the rear-node
    // constraint keeps the grounded run planted (raw quats lifted it +94)
    int32_t wave[kStanceSlopes] = {};
    wave[1] = -(jolt * kKnockJolt) / 2600;
    wave[2] = -(jolt * kKnockJolt) / 3400;
    int32_t root_mm[3];
    lying_frame(g, drain, roll, (jolt * kKnockJolt) / 1000,
                (drain * kKnockFold) / 1000, root_mm, wave);
    g.write(c, f);
    c.root[f * 3 + 0] = fxm(root_mm[0]);
    c.root[f * 3 + 1] = fxm(root_mm[1] + (curve(kBounce, kBounceN, f) * kKnockBounceMm) / 12);
    c.root[f * 3 + 2] = fxm(root_mm[2]);
  }
  c.events = {{15, zc::kEvFoot, 5}};  // the flank hits the dirt
  return c;
}

// Slot 21 - GET UP. Starts on the EXACT knocked pose (declared seam). The
// head lifts first (intention), the roll rights itself, then the body
// gathers back into the S with its belly kissing the dirt the whole way --
// the balance clip's get-up hover (run 0326, minY +65) is the recorded
// failure mode this build avoids by keeping the rear node planted.
inline zc::Clip build_getup() {
  zc::Clip c;
  c.slot_id = kSlotGetUp;
  c.interpolate = true;
  c.hold_last = true;  // ends on the rest pose and stays there
  c.frame_count = static_cast<uint16_t>(kGetUpKeys);
  c.root.assign(static_cast<size_t>(kGetUpKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kGetUpKeys) * kBoneCount, zc::quat16_identity());
  for (int f = 0; f < kGetUpKeys; ++f) {
    Rig g;
    g.reset();
    const int32_t wake = ss1000(f, 2, 10);           // the head lifts first
    const int32_t roll = 1000 - ss1000(f, 5, 20);    // rights itself
    const int32_t drain = 1000 - ss1000(f, 14, 36);  // gathers into the S
    int32_t root_mm[3];
    lying_frame(g, drain, roll, 0, (drain * kKnockFold) / 1000, root_mm);
    // the waking head-lift: a small extra pitch-up while the body is still
    // down, so the head visibly wakes before the body moves. A smooth
    // PRODUCT envelope (wake in, drain out) -- a threshold gate here is the
    // recorded pop trap. Zero at f0 (seam law) and zero at the end.
    const int32_t lift = (wake * drain) / 1000;
    if (lift != 0) {
      g.q[kBHead] = quat_mul(g.q[kBHead], quat_z(-(lift * 1100) / 1000));
    }
    g.write(c, f);
    c.root[f * 3 + 0] = fxm(root_mm[0]);
    c.root[f * 3 + 1] = fxm(root_mm[1]);
    c.root[f * 3 + 2] = fxm(root_mm[2]);
  }
  c.events = {{36, zc::kEvFoot, 6}};  // back on the S
  return c;
}

// Slot 22 - HITFLOOR. The landing that ends `falling` (donor semantics: the
// state machine cuts falling -> hitFloor at ground contact, then getUp).
// The body flattens as it drops, slams with an authored bite, one small
// rebound, and HOLDS the same knocked pose getUp starts from.
inline zc::Clip build_hitfloor() {
  zc::Clip c;
  c.slot_id = kSlotHitFloor;
  c.interpolate = true;
  c.hold_last = true;
  c.frame_count = static_cast<uint16_t>(kHitFloorKeys);
  c.root.assign(static_cast<size_t>(kHitFloorKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kHitFloorKeys) * kBoneCount, zc::quat16_identity());
  // the drop accelerates (gravity), the bite releases over four keys, a
  // small rebound rings out -- all one authored height curve (mm)
  static const Key kDrop[] = {{0, 540}, {3, 430},  {6, 240},  {8, 80},
                              {9, -24}, {12, -24}, {14, 8},   {17, 2},
                              {20, 0},  {25, 0}};
  constexpr int kDropN = static_cast<int>(sizeof(kDrop) / sizeof(Key));
  for (int f = 0; f < kHitFloorKeys; ++f) {
    Rig g;
    g.reset();
    const int32_t drain = 350 + (650 * ss1000(f, 0, 9)) / 1000;  // already loose
    const int32_t roll = ss1000(f, 5, 14);
    int32_t root_mm[3];
    lying_frame(g, drain, roll, 0, (drain * kKnockFold) / 1000, root_mm);
    g.write(c, f);
    c.root[f * 3 + 0] = fxm(root_mm[0]);
    c.root[f * 3 + 1] = fxm(root_mm[1] + curve(kDrop, kDropN, f));
    c.root[f * 3 + 2] = fxm(root_mm[2]);
  }
  c.events = {{9, zc::kEvFoot, 7}};  // the slam
  return c;
}

// ---- the directional damage set -------------------------------------------
// Donor law (state.d damageAnimation): the direction of the blow picks the
// slot, and a MISSING directional slot falls back to stance1 -- no reaction
// at all. Slot 5 (the 2026-08-28 hit) IS damageFront; these four complete
// the set. Same 18-key envelope as the hit (one damped overshoot -- wobble,
// not vibration); what differs is WHERE the reaction lives.
constexpr int kDmgKeys = kHitKeys;
// RUN 1730, the owner's hit verdict applied to all four directions ("more
// of the snake should be affected and a hit should look a lot stronger",
// and the directions must be OBVIOUSLY different -- that is the point of
// five slots). Each direction now drives the same travelling shockwave as
// the hit, in its own lane: sides throw the whole chain LATERALLY (plus a
// real sideways root shove -- displacement sells force); back drives a
// pitch pulse down the axis with a bigger surge; top crushes and shimmies.
constexpr int32_t kDmgSideSway = 8000;   // head-bone lateral whiplash, R/L
constexpr int32_t kDmgSideChain = 1200;  // per-joint lateral wave, decaying
constexpr int32_t kDmgSideShove = 230;   // mm the whole animal is thrown
                                         // SIDEWAYS -- the unmissable tell
constexpr int32_t kDmgSideRollAmp = 2200;// raised front rolls off the blow
constexpr int32_t kDmgBackJerk = 8500;   // head whips DOWN-forward
constexpr int32_t kDmgBackSurge = 260;   // mm the body shoves forward
constexpr int32_t kDmgTopCrush = 1400;   // arch flattens under a top strike
constexpr int32_t kDmgTopDuck = 7000;    // the head ducks
constexpr int32_t kDmgShimmy = 900;      // back/top whole-body absorption

// Direction-specific impact silhouettes. Back and top are absolute pitch-
// slope displacements; sides are local yaw bends and are mirrored by sign.
// None is a zero-sum crease: the struck length stays thrown out of line long
// enough for the delayed wave to pick it up and carry it to the tail.
constexpr int32_t kDmgBackFoldSlope[kStanceSlopes] = {
    0, 0, -2200, -7600, 10800, 7600, -3600, -1800, -700, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0};
constexpr int32_t kDmgTopFoldSlope[kStanceSlopes] = {
    0, 0, 6500, -13800, -9200, 5800, 3200, 1200, 300, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0};
constexpr int32_t kDmgSideFoldYaw[kSpineBones] = {
    0, 0, 6000, -10000, -5000, 4000, 2000, 800, 200, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

inline zc::Clip build_damage(uint16_t slot, int dir) {  // 0 R, 1 B, 2 L, 3 T
  zc::Clip c;
  c.slot_id = slot;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kDmgKeys);
  c.root.assign(static_cast<size_t>(kDmgKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kDmgKeys) * kBoneCount, zc::quat16_identity());
  for (int f = 0; f < kDmgKeys; ++f) {
    const int32_t e = curve(kImpactEnv, kImpactEnvN, f);
    // the travelling envelope: joint j gets the blow late and lighter
    int32_t ek[kSpineBones];
    for (int j = 0; j < kSpineBones; ++j)
      ek[j] = curve_mk(kImpactEnv, kImpactEnvN, f * 1000 - j * kShockLagMk);
    Rig g;
    g.reset();
    int32_t deepen = 0;
    if (dir == 3) deepen = (e * kDmgTopCrush) / 1000;   // crush flattens the arch
    else if (dir == 1) deepen = -(e * 420) / 1000;      // back-shove opens the S
    else deepen = (e * kHitDeepen) / 2500;              // side: a whisper of it
    // BACK and TOP carry the travelling PITCH pulse down the axis in the
    // wave lane (exact root compensation, belly planted -- the raw-quat
    // trap that drove the tail 233 mm under stays dead); the sides carry
    // only a whisper of pitch, their event is lateral.
    int32_t bwave[kStanceSlopes] = {};
    for (int k = 1; k < kStanceGround0; ++k) {
      const int32_t dec = 1000 - (k * kShockDecay) / (kStanceGround0 - 1);
      const int32_t amp = dir == 1 ? kDmgBackJerk / 2
                          : dir == 3 ? kShockFrontAmp / 2
                                     : kShockFrontAmp / 6;
      bwave[k] = ((dir == 1 ? ek[k] : -ek[k]) * ((amp * dec) / 1000)) / 1000;
    }
    // The local struck length is displaced in the direction's own pitch
    // silhouette. The carried, non-zero profile then releases into the wave.
    const int32_t fold = curve(kFoldEnv, kFoldEnvN, f);
    if (dir == 1 || dir == 3) {
      const int32_t* profile =
          dir == 1 ? kDmgBackFoldSlope : kDmgTopFoldSlope;
      for (int k = 1; k < kStanceGround0; ++k)
        bwave[k] += (fold * profile[k]) / 1000;
    }
    const int32_t rise = apply_stance(g, 1000, deepen, bwave);
    switch (dir) {
      case 0:    // RIGHT: lateral whiplash, head and neck swing off the blow
      case 2: {  // LEFT: mirrored
        const int32_t sgn = dir == 0 ? 1 : -1;
        g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + (e * 1100) / 1000),
                               quat_y(sgn * (e * kDmgSideSway) / 1000));
        // a touch of roll through the raised front -- the body leans off
        // the blow while the grounded run stays planted
        for (int k = 3; k <= 6; ++k)
          g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_x(sgn * (e * kDmgSideRollAmp) / 4000));
        break;
      }
      case 1: {  // BACK: shoved forward, the head whips down then recovers
        g.q[kBHead] = quat_z(kHeadAttitude - (e * kDmgBackJerk) / 1000);
        break;
      }
      case 3: {  // TOP: crushed down -- the deepen lowers the whole front
        g.q[kBHead] = quat_z(kHeadAttitude - (e * kDmgTopDuck) / 1000);
        break;
      }
    }
    // THE LATERAL SHOCKWAVE. Two lanes, because the conjugation trick is
    // exact only where the accumulated bind chain is shallow: the first
    // cut ran it over the STEEP front climb and the probe read the body
    // 1.6 METRES under (probe-hits.txt) -- the same class of dig the idle
    // snake's own comment warns about. So the FRONT takes direct
    // travelling yaw on the first joints (the previously proven-planted
    // side-whip lane), and the grounded run + tail take build_hit's exact
    // machinery.
    {
      const int32_t latsgn = dir == 0 ? 1 : dir == 2 ? -1 : dir == 1 ? 1 : -1;
      const int32_t latamp = (dir == 0 || dir == 2) ? kDmgSideChain : kDmgShimmy;
      const int32_t lfold =
          (dir == 0 || dir == 2) ? curve(kFoldEnv, kFoldEnvN, f) : 0;
      for (int k = 1; k <= 8; ++k) {
        const int32_t dec = 1000 - (k * 700) / 9;
        int32_t yawk = latsgn * ((ek[k] * ((latamp * dec) / 1000)) / 1000);
        // Side impacts bend a whole visible length out of the image plane;
        // the residual turn is deliberate, then propagates down-chain.
        yawk += latsgn * (lfold * kDmgSideFoldYaw[k]) / 1000;
        g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_y(yawk));
      }
      zc::quat16 snacc = zc::quat16_identity();
      for (int j = 0; j < kStanceGround0; ++j)
        snacc = quat_mul(snacc, g.q[kBSpine0 + j]);
      for (int k = kStanceGround0; k <= kStanceGround1; ++k) {
        snacc = quat_mul(snacc, g.q[kBSpine0 + k]);
        const zc::quat16 W =
            quat_y(latsgn * ((ek[k] * ((latamp * 900) / 1000)) / 1000));
        const zc::quat16 L = quat_mul(quat_mul(quat_conj(snacc), W), snacc);
        g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], L);
        snacc = quat_mul(snacc, L);
      }
      const int32_t tamp =
          (dir == 0 || dir == 2) ? kShockTailAmp : kShockTailAmp / 2;
      for (int k = kStanceGround1 + 1; k < kSpineBones; ++k) {
        const int reach = 400 + ((k - kStanceGround1 - 1) * 600) /
                                    (kSpineBones - kStanceGround1 - 2);
        g.q[kBSpine0 + k] = quat_mul(
            g.q[kBSpine0 + k],
            quat_y(latsgn * ((ek[k - 1] * ((tamp * reach) / 1000)) / 1000)));
      }
    }
    // the blades react when the wave REACHES the fork, not at impact
    g.tail_rest(kBladeSplay + (ek[kSpineBones - 1] * 1400) / 1000,
                kBladeRise - (dir == 3 ? (ek[kSpineBones - 1] * 900) / 1000 : 0));
    g.write(c, f);
    // displacement per direction: back = a real forward shove; sides = the
    // whole animal thrown SIDEWAYS -- the unmissable directional tell
    c.root[f * 3 + 0] = fxm(dir == 1 ? (e * kDmgBackSurge) / 1000 : 0);
    c.root[f * 3 + 1] = fxm(rise);
    if (dir == 0 || dir == 2)
      c.root[f * 3 + 2] =
          fxm(((dir == 0 ? 1 : -1) * (e * kDmgSideShove)) / 1000);
  }
  c.events = {{1, zc::kEvFoot, static_cast<uint8_t>(10 + dir)}};
  return c;
}

// ---- Slot 27 - RUN --------------------------------------------------------
// The walk's caterpillar law at a hungry cadence: the hump travels once per
// 24-key loop (0.8 s vs the walk's 1.33), taller hump, deeper front wave,
// the nose leaning in. One full cycle per clip, period held (03-ANIMATION:
// hold the cadence, let the stride go -- the sim owns the travel).
constexpr int kRunKeys = 24;
constexpr int32_t kRunHumpMm = 300;
constexpr int32_t kRunHumpHalf = 2400;  // WIDER than the walk's 1600: at 300 mm
                                        // tall over the walk's half-width the
                                        // per-segment climb exceeded segL and
                                        // asin16 clamped -- the height error
                                        // landed as a -68 mm dig (probe)
constexpr int32_t kRunWaveAmp = 3400;
constexpr int32_t kRunDeepen = 520;
constexpr int32_t kRunSway = 220;
constexpr int32_t kRunLean = 900;   // nose-forward
constexpr int32_t kRunSpeed = 24;   // mm per reel frame (the walk moves 13)

inline zc::Clip build_run() {
  zc::Clip c;
  c.slot_id = kSlotRun;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kRunKeys);
  c.root.assign(static_cast<size_t>(kRunKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kRunKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kRunKeys;
  const int32_t bLo = kStanceGround0;
  const int32_t bHi = kStanceGround1 + 2;
  const int32_t span1000 = (bHi - bLo) * 1000;
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);
  for (int f = 0; f < kRunKeys; ++f) {
    Rig g;
    g.reset();
    const int32_t phw = f * per_key;
    int32_t wave[kStanceSlopes] = {};
    for (int k = 0; k + 1 < kStanceGround0; ++k) {
      const int env =
          k < 4 ? 550 + k * 150 : (kStanceGround0 - 1 - k) * 1000 / (kStanceGround0 - 5);
      const int32_t pw = phw * 2 + 9000 - k * kWalkWaveSpatial;
      const int32_t sw =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
      wave[k] = static_cast<int32_t>(
          (static_cast<int64_t>(sw) * kRunWaveAmp * env / 1000) >> 16);
    }
    const int32_t sb =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((phw * 2 + 26000) & 0xFFFF)}).raw;
    const int32_t breath = ((sb + 65536) * 500) >> 16;
    const int32_t rise = apply_stance(g, 1000, (breath * kRunDeepen) / 1000, wave);
    int32_t h[kSpineBones] = {};
    const int32_t c1000 = bLo * 1000 + (f * span1000) / kRunKeys;
    const int32_t env = zref::fx_sin(zref::angle16{static_cast<uint16_t>(
                            ((c1000 - bLo * 1000) * 32768 / span1000) & 0xFFFF)})
                            .raw;
    for (int b = bLo; b <= bHi; ++b) {
      const int32_t d = b * 1000 - c1000;
      if (d <= -kRunHumpHalf || d >= kRunHumpHalf) continue;
      const int32_t ca = zref::fx_cos(zref::angle16{static_cast<uint16_t>(
                             ((d * 16384) / kRunHumpHalf) & 0xFFFF)})
                             .raw;
      const int64_t bump = (static_cast<int64_t>(ca) * ca) >> 16;
      h[b] = static_cast<int32_t>((bump * env >> 16) * kRunHumpMm >> 16);
    }
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
    g.q[kBHead] = quat_z(kHeadAttitude - kRunLean + wave[1] + wave[2]);
    for (int k = 5; k <= kStanceGround0 - 1; ++k) {
      const int32_t ph = f * per_key + k * 5000;
      const int32_t sw =
          (zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw * kRunSway) >> 16;
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_y(sw));
    }
    g.tail_rest();
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  c.events = {{0, zc::kEvFoot, 0}, {static_cast<uint16_t>(kRunKeys / 2), zc::kEvFoot, 1}};
  return c;
}

// ---- Slot 28 - DEATH 1 ----------------------------------------------------
// The second death (the donor picks at random among the filled deaths):
// agony rear-up, then the collapse runs FORWARD into a mostly-prone corpse
// (a shallow tilt, same away-from-lens sign as death0's keel -- the
// run-0326 magenta-smear lesson), two decaying tail slaps, stillness.
constexpr int kDeath1Keys = 96;
constexpr int32_t kD1Agony = 520;      // negative deepen: the front rears UP
constexpr int32_t kD1HeadRear = 3200;  // the nose climbs with it
constexpr int32_t kD1Roll = -5200;     // shallow flank tilt -- prone, not keeled
constexpr int32_t kD1RollLift = 30;
constexpr int32_t kD1Slap = 13000;     // the decaying tail slaps (6000
                                       // read as a whisper on the strip;
                                       // a dying slap must READ)
constexpr int32_t kD1Shudder = 2000;   // the head's dying tremor (angle16)

inline zc::Clip build_death1() {
  zc::Clip c;
  c.slot_id = kSlotDeath1;
  c.interpolate = true;
  c.hold_last = true;
  c.frame_count = static_cast<uint16_t>(kDeath1Keys);
  c.root.assign(static_cast<size_t>(kDeath1Keys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kDeath1Keys) * kBoneCount, zc::quat16_identity());
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);
  static const int64_t base_rear_sin = [] {
    int64_t acc = 0;
    for (int k = 0; k <= kStanceGround1; ++k)
      acc += zref::fx_sin(zref::angle16{static_cast<uint16_t>(kStanceSlope[k] & 0xFFFF)}).raw;
    return acc;
  }();
  for (int f = 0; f < kDeath1Keys; ++f) {
    Rig g;
    g.reset();
    // agony: rises over k4..16, gone by k34 as the collapse takes over
    const int32_t agony = ss1000(f, 4, 16) - ss1000(f, 16, 34);
    const int32_t drain = ss1000(f, 18, 46);
    // the head's dying tremor rides the agony (slow, two cycles)
    int32_t shake = 0;
    if (f < 20) {
      const int32_t ph = f * 6554;
      shake = (zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw *
               kD1Shudder) >> 16;
    }
    int64_t rear_sin = 0;
    int32_t prev = 0;
    for (int k = 0; k < kStanceSlopes; ++k) {
      int64_t d = kStanceSlope[k];
      // the rear-up: the descent lobe UN-deepens (the breath's own formula
      // run negative), so the whole front lobe climbs
      if (k >= kStanceDescend0 && k <= kStanceDescend1 && agony != 0) {
        const int64_t deepen = -(static_cast<int64_t>(agony) * kD1Agony) / 1000;
        if (d <= 16384) d += (d * deepen) / 1000;
        else d -= ((32768 - d) * deepen) / 1000;
      }
      d += ((kCorpseSlope[k] - d) * drain) / 1000;
      // the tail slaps: two decaying cycles, k48..80, lift-dominant with a
      // small authored bite on the down-beat (the sine's negative half is
      // clamped to a quarter of the lift). The slap owns the last FIVE
      // segments, not just the two past the stance's grounded run -- on the
      // lying body only the thin 320 mm tip moved and a frame diff showed
      // ~130 changed pixels: a whisper, not a slap (run 0326).
      // the REAR-NODE CONSTRAINT MUST NOT SEE THE SLAP: three slap joints
      // sit inside its window, and compensating them dove the whole root
      // and pressed the FRONT 146 mm under (probe, run 0326). rear_sin
      // accumulates the un-slapped slope; the slap moves only the tail.
      const uint16_t a = static_cast<uint16_t>(d & 0xFFFF);
      if (k <= kStanceGround1) rear_sin += zref::fx_sin(zref::angle16{a}).raw;
      if (k >= kSpineBones - 6 && f >= 48 && f < 80) {
        const int32_t ph = ((f - 48) * 4096);  // two cycles over 32 keys
        int32_t sl = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
        if (sl < -16384) sl = -16384;
        const int32_t decay = ((80 - f) * 1000) / 32;
        d -= (static_cast<int64_t>(kD1Slap) * sl / 65536) * decay /
             (1000 * 6);
      }
      const int32_t pitch = static_cast<int32_t>(d) - prev;
      prev = static_cast<int32_t>(d);
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
    }
    const int32_t root_y =
        static_cast<int32_t>((segL * (rear_sin - base_rear_sin)) >> 16);
    const int32_t roll = (kD1Roll * ss1000(f, 26, 50)) / 1000;
    const int32_t roll_lift = (kD1RollLift * ss1000(f, 26, 50)) / 1000;
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude + (agony * kD1HeadRear) / 1000 - (1400 * drain) / 1000 +
               shake / 2),
        quat_y(shake));
    g.tail_rest((kBladeSplay * (1000 + (agony * 800) / 1000 - drain / 2)) / 1000,
                (kBladeRise * (1000 - drain)) / 1000);
    const zc::quat16 rq = quat_x(roll);
    g.q[kBSpine0] = quat_mul(rq, g.q[kBSpine0]);
    g.write(c, f);
    int32_t rx, ry, rz;
    quat_rot_vec(rq, kDeathPivotX, -kBodyY - root_y, 0, rx, ry, rz);
    c.root[f * 3 + 0] = fxm(kDeathPivotX - rx);
    c.root[f * 3 + 1] = fxm(root_y + roll_lift + (-kBodyY - root_y - ry));
    c.root[f * 3 + 2] = fxm(-rz);
  }
  c.events = {{40, zc::kEvFoot, 3}};  // the body lands
  return c;
}

// ---- Slot 30 - TAUNT ------------------------------------------------------
// The owner's ask; the donor's `laugh` slot. The front lobe rears up proud,
// the blades flare wide, and the head wags side to side -- slow, eased, the
// neck following. Both ends on the rest pose so the cut back to stance is
// clean.
constexpr int kTauntKeys = 56;
constexpr int32_t kTauntRear = 1250;  // angle16 PER FRONT SEGMENT (k0..4).
                                      // The first cut un-deepened the
                                      // descent lobe instead, which RAISED
                                      // the descending stroke toward the
                                      // head and folded the hook over it
                                      // (overlap 355 mm; the proud peak
                                      // read as a crumple). Lifting the
                                      // ascending neck rears the front up
                                      // WITH the head. ~35 deg accumulated.
constexpr int32_t kTauntWag = 2200;   // 3600 -> 2200, RUN 1939: the slow
                                      // lateral wander UNDER the bobble --
                                      // the fast layer owns the gesture now
constexpr int32_t kTauntFlare = 2600;
constexpr int32_t kTauntRise = 1400;
constexpr int32_t kTauntNose = 3400;  // the head rises WITH the rear-up
// THE BOBBLE (RUN 1939, item 14; owner: "cheeky fast side to side to side
// etc. headshake, like the Indian 'I am being funny' headshake"). A
// TILT/ROLL, not a yaw -- ear toward shoulder -- quick and light, four-ish
// rocks and done, with a second-harmonic pitch trace so it draws a slight
// figure-eight instead of a metronome. THIS IS A CONSCIOUS EXCEPTION to
// "fewer and slower": the gesture is DEFINED by being fast and light; the
// body underneath stays on the slow loose idle recipe, and that contrast
// is what sells it. Composed with quat_mul (roll * yaw * pitch), never an
// axis switch.
constexpr int32_t kTauntBobble = 6200;    // roll amplitude (reads at 240p)
constexpr int32_t kTauntBobbleFig8 = 1400;  // the figure-eight pitch trace

inline zc::Clip build_taunt() {
  zc::Clip c;
  c.slot_id = kSlotTaunt;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kTauntKeys);
  c.root.assign(static_cast<size_t>(kTauntKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kTauntKeys) * kBoneCount, zc::quat16_identity());
  // REBUILT RUN 1939 (item 7, owner: "Taunt is too rigid too, all parts of
  // body should bend"): the performance now rides idle_body's full living
  // recipe -- breath + front wave with exact root compensation, grounded
  // sideways snake, torsion, tail sway, blade play -- so no segment holds
  // a fixed shape for the duration. The gesture layers ON TOP: rear-up in
  // the wave lane, the slow wag, and the fast cheeky BOBBLE (item 14).
  const int32_t per_key = 65536 / kTauntKeys;
  for (int f = 0; f < kTauntKeys; ++f) {
    Rig g;
    g.reset();
    const int32_t ph = f * per_key;
    const int32_t up = ss1000(f, 6, 16) - ss1000(f, 42, 52);
    const int32_t wag_env = ss1000(f, 14, 20) - ss1000(f, 38, 44);
    int32_t wag = 0;
    if (wag_env != 0) {
      const int32_t pw = ((f - 14) * 5851);  // 2.5 slow cycles
      wag = (zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw *
             ((kTauntWag * wag_env) / 1000)) >> 16;
    }
    // the rear-up rides the compensated wave lane OVER the living body
    int32_t wave[kStanceSlopes] = {};
    for (int k = 0; k <= 4; ++k) wave[k] = (up * kTauntRear) / 1000;
    const int32_t rise =
        idle_body(g, ph, 750, wave, kQuickTauntStanceSlope);
    // THE BOBBLE: fast light roll, ~4 rocks over keys 20..42, gone after
    const int32_t bob_env = ss1000(f, 20, 25) - ss1000(f, 38, 44);
    int32_t roll = 0, fig8 = 0;
    if (bob_env != 0) {
      const int32_t pb = (f - 20) * 13107;  // one rock per 5 keys: FAST
      roll = (zref::fx_sin(zref::angle16{static_cast<uint16_t>(pb & 0xFFFF)}).raw *
              ((kTauntBobble * bob_env) / 1000)) >> 16;
      fig8 = (zref::fx_sin(zref::angle16{static_cast<uint16_t>((pb * 2 + 16000) & 0xFFFF)}).raw *
              ((kTauntBobbleFig8 * bob_env) / 1000)) >> 16;
    }
    g.q[kBHead] = quat_mul(
        quat_mul(quat_z(kHeadAttitude + (up * kTauntNose) / 1000 + fig8),
                 quat_y(wag)),
        quat_x(roll));
    if (wag != 0) {
      g.q[kBSpine0 + 1] = quat_mul(g.q[kBSpine0 + 1], quat_y(wag / 3));
      g.q[kBSpine0 + 2] = quat_mul(g.q[kBSpine0 + 2], quat_y(wag / 5));
    }
    // (a roll leak onto joint 1 was tried and REMOVED: a roll about the
    // climbing tube's axis swings the entire downstream body -- the probe
    // read the grounded run digging -26 in time with the bobble. The
    // skull bone pivots at the cranium centroid, which IS the neck-borne
    // read.)
    // blades: the flare rides ON the idle play instead of replacing it
    const int32_t st = zref::fx_sin(
        zref::angle16{static_cast<uint16_t>((ph * 2 - 21000) & 0xFFFF)}).raw;
    g.tail_rest(kBladeSplay + (up * kTauntFlare) / 1000 + ((st * 900) >> 16),
                kBladeRise + (up * kTauntRise) / 1000);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  c.events = {{14, zc::kEvSound, 1}};  // the taunt call
  return c;
}

// ---- Slot 31 - CORPSE -----------------------------------------------------
// The donor's law: the dead body is a playing clip. Frame 0 is the death
// clip's own final key BYTE-FOR-BYTE (declared seam), and the loop adds only
// a barely-there blade settle -- stillness after motion is the read, so the
// motion here sits at the edge of perception.
constexpr int kCorpseKeys = 32;
constexpr int32_t kCorpseBladeStir = 130;  // angle16: sub-degree

inline zc::Clip build_corpse(const zc::Clip& death) {
  zc::Clip c;
  c.slot_id = kSlotCorpse;
  c.interpolate = true;  // loops
  c.frame_count = static_cast<uint16_t>(kCorpseKeys);
  c.root.assign(static_cast<size_t>(kCorpseKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kCorpseKeys) * kBoneCount, zc::quat16_identity());
  const int last = death.frame_count - 1;
  for (int f = 0; f < kCorpseKeys; ++f) {
    for (int b = 0; b < kBoneCount; ++b)
      c.quats[static_cast<size_t>(f) * kBoneCount + b] =
          death.quats[static_cast<size_t>(last) * kBoneCount + b];
    for (int i = 0; i < 3; ++i)
      c.root[f * 3 + i] = death.root[static_cast<size_t>(last) * 3 + i];
    // one whole sine cycle so the loop closes; f 0 composes NOTHING so the
    // seam to the death's last key stays bit-identical
    const int32_t ph = f * (65536 / kCorpseKeys);
    const int32_t stir =
        (zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw *
         kCorpseBladeStir) >> 16;
    if (stir != 0) {
      const int blades[3] = {kBBladeL, kBBladeR, kBSpike};
      for (int bi = 0; bi < 3; ++bi) {
        const int b = blades[bi];
        zc::quat16& q = c.quats[static_cast<size_t>(f) * kBoneCount + b];
        q = quat_mul(q, quat_z(b == kBSpike ? stir / 2 : stir));
      }
    }
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
  kSlotAtkCompress = 10,  // settle + real compression + hold (keys 0..17)
  kSlotAtkRelease = 11,   // shared spring releases into the coil (17..29)
  kSlotAtkCoil = 12,      // the wheel, looping (duplicate of clean key 29)
  kSlotAtkUnroll = 13,    // coil -> rigid spear (52..60)
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

inline zc::Clip duplicate_pose_clip(const zc::Clip& src, uint16_t slot,
                                    int key) {
  zc::Clip c = slice_clip(src, slot, key, key);
  c.frame_count = 2;
  c.root.resize(6);
  for (int i = 0; i < 3; ++i) c.root[3 + i] = c.root[i];
  c.quats.resize(static_cast<size_t>(2) * kBoneCount);
  for (int b = 0; b < kBoneCount; ++b) c.quats[kBoneCount + b] = c.quats[b];
  return c;
}

// the straight-spear local rig every flex/hit phase starts and ends on:
// EXACTLY the attack's key-47/key-62 local pose (auth 0, curl 0)
inline void spear_rig(Rig& g) {
  g.reset();
  g.q[kBHead] = quat_z(0);
  g.tail_rest(kBladeSplay / 5, 0, 0);  // the javelin closes the fan: no bias
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
constexpr int32_t kAirHitAmp = 7000;       // whole spear bows visibly at 240p
constexpr int32_t kAirHitHeadKick = 2600;  // skull counter-whip at impact
constexpr int32_t kAirHitBladeFlare = 1800;// weapon-end impact punctuation
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
      // A continuous hard bow through the complete spear. The previous /8
      // attenuation was present in numbers but visually invisible; /5 keeps
      // the tube volumetric while making the impact silhouette unmistakable.
      const int env_k = 1000 - (k * 700) / (kSpineBones - 2);
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k], quat_z((e * kAirHitAmp * env_k) / (1000 * 1000) / 5));
    }
    g.q[kBHead] = quat_z(-(e * kAirHitHeadKick) / 1000);
    const int32_t flare_e = e < 0 ? -e : e;
    if (flare_e != 0)
      g.tail_rest(kBladeSplay / 5 + (flare_e * kAirHitBladeFlare) / 1000, 0, 0);
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
// SHIPPING STATUS — DECIDED, RUN 0326 (owner's standing order: decide by
// eye, do not defer). Both falls rendered on the same fixed side camera,
// contact-sheeted every frame (fall-F1-sheet.png / fall-F2-sheet.png /
// fall-F2-tune1-sheet.png in the run): as shipped, F2 knotted the animal
// into a ball for most of the loop; after the tune below it opens into
// hooks and half-S shapes but still reads crumpled and BUSY -- adjacent
// thumbnails differ wildly, the high-energy read the house style calls
// jitter-adjacent. F1 is a long legible serpent with slow travelling
// S-language in every frame: the owner's "relax by a ton" look.
// F1 SHIPS on slot 4. F2 stays preview-gated (-DZIXX_F2_PREVIEW) with the
// improved tune kept; promotion is still one line if his eye ever rules
// the other way.
constexpr int kFallBakeWarmLoops = 4;   // settle into the periodic orbit
constexpr int kFallBakeBlend = 16;      // keys of tail->head closure fade
// region dynamics, indexed head..tail in 4 bands: heavy slow head, loose
// neck, heavier middle, light delayed tail (masses as inverse-acc scale)
// run 0326 TUNE ATTEMPT: the shipped values knotted the animal into a ball
// for most of the loop (fall-F2-sheet.png) -- the inertia+aero drive
// saturated the +-9000 clamp and the weak springs never pulled it open.
// Stronger rest pull, heavier damping, gentler drives:
constexpr int32_t kFbSpring[4] = {280, 340, 300, 380}; // pull to the rest S
constexpr int32_t kFbCouple[4] = {160, 220, 190, 260}; // neighbour bend
constexpr int32_t kFbDamp[4] = {155, 140, 148, 120};   // per-mille per step
constexpr int32_t kFbInertia[4] = {80, 55, 32, 65};    // root spin coupling
constexpr int32_t kFbAero[4] = {320, 450, 260, 700};   // slow flutter drive
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


// ======================= RUN 0757: THE VOCABULARY CLOSE-OUT ================
// The gaps ANIMATION-VOCABULARY.md still listed: stance2, tumble, a third
// death, the last two idle flourishes (the donor's 4-flourish convention,
// 86/86), a true quick melee beside the salto family, and the useful half
// of the personality block. All of it obeys the house style: slow, loose,
// low-frequency, the bend travelling through the body; the head reacts.
// Deliberately SKIPPED (stated in the run findings): doubletake,
// ambivalence, disgust, disoriented -- without a face rig they differ from
// notify/look/talk only by timing nuance and would dilute the vocabulary;
// thrash/carried/float/rise wait for their mechanics.
constexpr int kSlotDeath2 = 29;   // donor death2 -- the unwinding death
constexpr int kSlotStance2 = 32;  // donor stance2 -- the damaged stance loop
constexpr int kSlotTumble = 36;   // donor tumble -- thrown through the air
constexpr int kSlotIdle2 = 37;    // donor idle2 -- the stretch (flourish 3)
constexpr int kSlotIdle3 = 38;    // donor idle3 -- fork-watch (flourish 4)
constexpr int kSlotStrike = 39;   // donor attack1 -- the quick coil strike
constexpr int kSlotNotify = 40;   // donor notify -- the alert
constexpr int kSlotBow = 41;      // donor bow
constexpr int kSlotTalk = 42;     // donor talk
constexpr int kSlotSorrow = 43;   // donor sorrow

constexpr int kStance2Keys = 62;  // donor stance median
constexpr int kTumbleKeys = 72;   // 2.4 s per somersault -- thrown, not frantic
constexpr int kDeath2Keys = 96;   // deaths get three seconds (donor law)
constexpr int kIdle2Keys = 98;    // flourish median
constexpr int kIdle3Keys = 98;
constexpr int kStrikeKeys = 45;   // melee median
constexpr int kNotifyKeys = 48;
constexpr int kBowKeys = 72;      // donor bow median
constexpr int kTalkKeys = 98;     // donor talk median
constexpr int kSorrowKeys = 98;

// SLOT 32 -- STANCE2, the damaged stance. Reads wounded with no HP bar:
// the proud S SAGS (a compensated wave-lane sag, so the belly stays
// planted), the head hangs below its line, the breath is weaker with a
// slow second tremor riding it, the fins droop. A loop, cut-compatible
// with stance1 (both are the canonical S under authored deltas).
constexpr int32_t kS2Sag = 2400;      // peak sag delta on the neck segments
constexpr int32_t kS2Droop = 3000;    // extra head-down attitude
constexpr int32_t kS2Tremor = 620;    // the weak second breath harmonic
inline zc::Clip build_stance2() {
  zc::Clip c;
  c.slot_id = kSlotStance2;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kStance2Keys);
  c.root.assign(static_cast<size_t>(kStance2Keys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kStance2Keys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kStance2Keys;
  static const int kSagEnv[5] = {0, 600, 1000, 800, 400};  // per neck segment
  for (int f = 0; f < kStance2Keys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    int32_t extra[kStanceSlopes] = {};
    for (int k = 1; k <= 4; ++k) {
      // positive delta = more descent = the front hangs lower
      extra[k] = (kS2Sag * kSagEnv[k]) / 1000;
      // the labored second tremor, slow (three cycles per 2 s loop),
      // travelling down the sagged stretch
      const int32_t pt = ph * 3 + k * 8000;
      const int32_t st =
          zref::fx_sin(zref::angle16{static_cast<uint16_t>(pt & 0xFFFF)}).raw;
      extra[k] += (st * kS2Tremor) >> 16;
    }
    const int32_t rise = idle_body(g, ph, 620, extra);
    // the head hangs; a slow weary wander instead of the idle's sway
    const int32_t sw =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph + 21000) & 0xFFFF)}).raw;
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude - kS2Droop + extra[1] + extra[2] + ((sw * 500) >> 16)),
        quat_y((sw * 1100) >> 16));
    // fins droop
    g.tail_rest((kBladeSplay * 850) / 1000, (kBladeRise * 600) / 1000);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}

// SLOT 36 -- TUMBLE, thrown through the air. The fall's re-pivot law with
// a GATHERED body: where the falling flail is a long loose serpent, the
// tumble is the animal bunched around itself, turning end over end once
// per loop (warped, so it hesitates and tips rather than turntabling),
// with a slow roll wobble riding along and the bend still travelling.
constexpr int32_t kTumAuthMid = 1120;   // bunched tighter than rest
                                        // (1230 folded the coil 440 mm
                                        // through itself -- probe, first cut)
constexpr int32_t kTumAuthSwing = 140;  // the travelling gather/release
constexpr int32_t kTumWarp = 5200;      // hesitation in the somersault
constexpr int32_t kTumRollAmp = 3600;   // slow roll wobble
constexpr int32_t kTumNeckAmp = 4200;   // the head lolls, reduced from the fall
inline zc::Clip build_tumble() {
  zc::Clip c;
  c.slot_id = kSlotTumble;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kTumbleKeys);
  c.root.assign(static_cast<size_t>(kTumbleKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kTumbleKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kTumbleKeys;
  for (int f = 0; f < kTumbleKeys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    // the gathered S: per-joint authority swings about a bunched mean,
    // phase travelling down the body (the fall's recurrence, tightened)
    {
      int32_t prev = 0;
      for (int k = 0; k < kStanceSlopes; ++k) {
        const int32_t pa = ph - k * 5200 + 18000;
        const int32_t sa =
            zref::fx_sin(zref::angle16{static_cast<uint16_t>(pa & 0xFFFF)}).raw;
        const int32_t auth = kTumAuthMid + ((sa * kTumAuthSwing) >> 16);
        const int32_t d =
            static_cast<int32_t>((static_cast<int64_t>(kStanceSlope[k]) * auth) / 1000);
        const int32_t pitch = d - prev;
        prev = d;
        g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
      }
    }
    // loose neck and head, the fall's language at reduced amplitude
    for (int k = 1; k <= 4; ++k) {
      const int32_t p1 = ph + k * 7000;
      const int32_t p2 = ph * 2 + 16000 + k * 9000;
      const int32_t s1 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p1 & 0xFFFF)}).raw;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p2 & 0xFFFF)}).raw;
      const int32_t amp = kTumNeckAmp - (k - 1) * 400;
      g.q[kBSpine0 + k] =
          quat_mul(g.q[kBSpine0 + k],
                   quat_mul(quat_z((s1 * (amp / 2)) >> 16),
                            quat_y((s2 * (amp * 2 / 3)) >> 16)));
    }
    // the somersault, warped; the head looks INTO the turn (it is thrown,
    // it still wants the ground)
    const int32_t theta_u =
        static_cast<int32_t>((static_cast<int64_t>(f) << 16) / kTumbleKeys);
    const int32_t theta =
        theta_u + static_cast<int32_t>(
                      (static_cast<int64_t>(kTumWarp) *
                       zref::fx_sin(zref::angle16{static_cast<uint16_t>(theta_u & 0xFFFF)}).raw) >>
                      16);
    const int32_t aim =
        (zref::fx_sin(zref::angle16{static_cast<uint16_t>(theta & 0xFFFF)}).raw * 3400) >> 16;
    {
      const int32_t p2 = ph * 2 + 21000;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p2 & 0xFFFF)}).raw;
      g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + aim),
                             quat_y((s2 * (kTumNeckAmp / 2)) >> 16));
    }
    // blades trail the spin
    const int32_t fl =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph + 12000) & 0xFFFF)}).raw;
    g.tail_rest(kBladeSplay + ((fl * 1400) >> 16), kBladeRise + ((fl * 800) >> 16));
    const int32_t t2 =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph + 17000) & 0xFFFF)}).raw;
    const zc::quat16 tum = quat_mul(quat_z(theta), quat_x((t2 * kTumRollAmp) >> 16));
    g.q[kBSpine0] = quat_mul(tum, g.q[kBSpine0]);
    g.write(c, f);
    int32_t rx, ry, rz;
    quat_rot_vec(tum, kFallPivotX, kFallPivotY, 0, rx, ry, rz);
    c.root[f * 3 + 0] = fxm(kFallPivotX - rx);
    c.root[f * 3 + 1] = fxm(kFallLift + kFallPivotY - ry);
    c.root[f * 3 + 2] = fxm(-rz);
  }
  return c;
}

// SLOT 29 -- DEATH2, the unwinding. Distinct from death0 (keel onto the
// flank) and death1 (rear-up collapse): the S itself dies -- after the
// freeze and two slow head-shakes the whole letter UNROLLS forward into a
// stretched line, belly down, the nose sliding out along the dirt as the
// coils pay out; two decaying struggle-waves travel the body while it
// still can; the fork folds; one small late tail beat, then stillness.
// A creature whose identity is the S reads dead the moment the S is gone.
constexpr int32_t kD2NoseRest = 96;   // nose-centre rest height: the dome
                                      // presses ~-8 mm (authored bite)
static const int32_t kD2Stretch[kStanceSlopes] = {
    // the paid-out line: nearly straight, the gentle belly-following ramp
    // of a lying body (the corpse table's law, stretched longer)
    900, 700, 550, 420, 320, 240, 180, 130, 90, 60,
    40,  20,  0,   -20, -40, -60, -80, -120, -220};
inline zc::Clip build_death2() {
  zc::Clip c;
  c.slot_id = kSlotDeath2;
  c.interpolate = true;
  c.hold_last = true;
  c.frame_count = static_cast<uint16_t>(kDeath2Keys);
  c.root.assign(static_cast<size_t>(kDeath2Keys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kDeath2Keys) * kBoneCount, zc::quat16_identity());
  const int32_t segL = kBodyLenMm / (kSpineBones - 1);
  int64_t base_cos = 0;
  for (int k = 0; k < kStanceSlopes; ++k) {
    const uint16_t a = static_cast<uint16_t>(kStanceSlope[k] & 0xFFFF);
    base_cos += zref::fx_cos(zref::angle16{a}).raw;
  }
  // the declared bite: the unwound body's lowest surface presses this deep.
  // The root is COMPUTED per key so the deepest node's underside holds it --
  // a linear nose-drop buried the mid-body -316 (probe, first cut): the
  // paid-out line's belly is not at the nose, so the nose cannot be the
  // thing that meters the drop.
  constexpr int32_t kD2Bite = 10;
  // the unwind gives unevenly: give, a half-catch, the big give, slow end
  static const Key kPay[] = {{0, 0},   {12, 0},  {20, 320}, {25, 280},
                             {33, 640}, {44, 900}, {58, 1000}, {95, 1000}};
  constexpr int kPayN = static_cast<int>(sizeof(kPay) / sizeof(Key));
  for (int f = 0; f < kDeath2Keys; ++f) {
    Rig g;
    g.reset();
    int32_t headshake = 0;
    if (f <= 12) {
      const int32_t ph = f * 10923;  // two slow shakes over 12 keys
      const int32_t sh = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
      headshake = (sh * 1900) >> 16;
    }
    const int32_t pay = curve(kPay, kPayN, f);
    // the struggle: two travelling waves that die as the body pays out
    const int32_t strug_amp = (1800 * (1000 - pay)) / 1000;
    int64_t sum_cos = 0;
    int32_t prev = 0;
    for (int k = 0; k < kStanceSlopes; ++k) {
      int64_t d = kStanceSlope[k];
      d += ((kD2Stretch[k] - d) * pay) / 1000;
      if (k >= 2 && k <= 12 && strug_amp > 0) {
        const int32_t pw = f * 4200 - k * 9000;
        const int32_t sw =
            zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
        d += (sw * strug_amp) >> 16;
      }
      // the late tail beat: k76..88, small
      if (k > kStanceGround1) {
        const int32_t late = ss1000(f, 76, 81) - ss1000(f, 81, 88);
        d -= (900 * late) / 1000;
      }
      const uint16_t a = static_cast<uint16_t>(d & 0xFFFF);
      sum_cos += zref::fx_cos(zref::angle16{a}).raw;
      const int32_t pitch = static_cast<int32_t>(d) - prev;
      prev = static_cast<int32_t>(d);
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
    }
    // the nose slides FORWARD as the coils pay out: a share of the chain's
    // horizontal extension goes to the root, so the head visibly slides
    // along the dirt instead of the tail shooting backward
    const int32_t ext_mm =
        static_cast<int32_t>((segL * (sum_cos - base_cos)) >> 16);
    // the root height: walk the posed node heights (relative to the root)
    // and hold the LOWEST underside at the declared bite. Recompute the
    // slopes exactly as composed above so the struggle waves are included.
    int32_t drop;
    {
      int64_t rel = 0;      // fx16 mm, node height relative to the root
      int64_t minv = 0;     // lowest (node - its radius)
      for (int k = 0; k < kStanceSlopes; ++k) {
        int64_t d = kStanceSlope[k];
        d += ((kD2Stretch[k] - d) * pay) / 1000;
        if (k >= 2 && k <= 12 && strug_amp > 0) {
          const int32_t pw = f * 4200 - k * 9000;
          const int32_t sw =
              zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
          d += (sw * strug_amp) >> 16;
        }
        rel -= static_cast<int64_t>(segL) *
               zref::fx_sin(zref::angle16{static_cast<uint16_t>(d & 0xFFFF)}).raw;
        const int st = (k * 3 + 2) > (kProfileStations - 1) ? (kProfileStations - 1)
                                                            : (k * 3 + 2);
        const int64_t under = (rel >> 16) - station_r(st);
        if (under < minv) minv = under;
      }
      drop = static_cast<int32_t>(-(kBodyY + minv) - kD2Bite);
    }
    // the dying head: eases TOWARD the paid-out line (a further droop
    // tucked the ball chin-under -- first render), the early shakes ride
    // on top, and the very end has one small settling turn
    const int32_t last = ss1000(f, 84, 88) - ss1000(f, 88, 95);
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude + (5200 * pay) / 1000 + headshake - (last * 500) / 1000),
        quat_y(headshake + (last * 1100) / 1000));
    // the fork folds with the paying-out, most of the way
    g.tail_rest((kBladeSplay * (1000 - (pay * 550) / 1000)) / 1000,
                (kBladeRise * (1000 - (pay * 700) / 1000)) / 1000);
    g.write(c, f);
    c.root[f * 3 + 0] = fxm((ext_mm * 380) / 1000);
    c.root[f * 3 + 1] = fxm(drop);
  }
  c.events = {{44, zc::kEvFoot, 2}};  // the body settles flat
  return c;
}

// SLOT 37 -- IDLE2, the STRETCH (flourish 3). The idle's living body, and
// once per loop a slow luxurious stretch travels up the front: each neck
// segment lifts a beat after the one behind it, the head pitches up and
// back with the arriving wave, the fins flare wide, everything releases
// even slower than it rose. A cat-stretch for an animal that is one arch.
inline zc::Clip build_idle2() {
  zc::Clip c;
  c.slot_id = kSlotIdle2;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kIdle2Keys);
  c.root.assign(static_cast<size_t>(kIdle2Keys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kIdle2Keys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kIdle2Keys;
  for (int f = 0; f < kIdle2Keys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    int32_t extra[kStanceSlopes] = {};
    int32_t head_env = 0;
    for (int k = 1; k <= 4; ++k) {
      // the stretch arrives at each segment 4 keys after the one behind
      // it (bend travels), rises over 24 keys, releases over 30
      const int fo = f - (4 - k) * 4;
      const int32_t env = ss1000(fo, 22, 46) - ss1000(fo, 58, 88);
      // negative = more climb: the front lifts and opens. 2300 -> 1100:
      // at 2300 the crown climbed so hard the head folded back INTO it
      // and the stretch read as a crumple (first render)
      extra[k] = -(1100 * env) / 1000;
      if (k == 1) head_env = env;
    }
    const int32_t rise = idle_body(g, ph, 700, extra);
    // the head rides the stretch: up and slightly back, slow
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude + (2400 * head_env) / 1000 + extra[1] + extra[2]),
        quat_y((head_env * 600) / 1000));
    // fins flare with the stretch peak
    g.tail_rest(kBladeSplay + (1500 * head_env) / 1000,
                kBladeRise + (900 * head_env) / 1000);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}

// SLOT 38 -- IDLE3, FORK-WATCH (flourish 4). The head turns back and
// WATCHES ITS OWN TAIL: the fork fans open and shut twice, the middle
// spike wiggles, and the head tracks it with two small interested tilts
// before coming home. The strongest aliveness signal available is a
// reacting head, so the fourth flourish is built on exactly that.
inline zc::Clip build_idle3() {
  zc::Clip c;
  c.slot_id = kSlotIdle3;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kIdle3Keys);
  c.root.assign(static_cast<size_t>(kIdle3Keys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kIdle3Keys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kIdle3Keys;
  // the gaze: turn back over the shoulder toward the raised fork, watch
  // with two tilts, return -- the look-around's itinerary machinery
  struct Aim { int f; int32_t yaw, pitch; };
  // NEGATIVE yaw: +15500 turned the face to the CAMERA, not the fork
  // (first render); the tail rises behind-left of the gaze line
  static const Aim kAim[] = {
      {0, 0, 0},          {10, 0, 0},
      {30, -15500, 2600}, {40, -15500, 2600},
      {50, -14200, 1400}, {58, -15800, 3000},
      {70, -15300, 2400}, {78, -15300, 2400},
      {92, 0, 0},         {97, 0, 0}};
  constexpr int kAimN = static_cast<int>(sizeof(kAim) / sizeof(Aim));
  const auto aim_at = [&](int f, int32_t& yaw, int32_t& pitch) {
    yaw = kAim[kAimN - 1].yaw; pitch = kAim[kAimN - 1].pitch;
    for (int i = 0; i + 1 < kAimN; ++i) {
      if (f >= kAim[i].f && f <= kAim[i + 1].f) {
        const int span = kAim[i + 1].f - kAim[i].f;
        int32_t t = span > 0 ? ((f - kAim[i].f) * 1000) / span : 1000;
        t = t * t * (3000 - 2 * t) / 1000000;
        t = t + (t * (1000 - t) / 1000) * 60 / 1000;
        yaw = kAim[i].yaw + ((kAim[i + 1].yaw - kAim[i].yaw) * t) / 1000;
        pitch = kAim[i].pitch + ((kAim[i + 1].pitch - kAim[i].pitch) * t) / 1000;
        return;
      }
    }
  };
  for (int f = 0; f < kIdle3Keys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    int32_t lyaw, lpitch;
    aim_at(f >= 3 ? f - 3 : 0, lyaw, lpitch);
    int32_t follow[kStanceSlopes] = {};
    follow[1] = (lpitch * kLookNeckFollow) / 2000;
    follow[2] = (lpitch * kLookNeckFollow) / 2000;
    const int32_t rise = idle_body(g, ph, 750, follow);
    int32_t yaw, pitch;
    aim_at(f, yaw, pitch);
    g.q[kBHead] = quat_mul(quat_z(kHeadAttitude + pitch + follow[1] + follow[2]),
                           quat_y(yaw));
    // the neck's yaw share about the true world vertical (the snake trick)
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
    // THE PERFORMANCE: while watched (k30..78) the fork fans twice,
    // slowly, and the spike wiggles out of phase
    const int32_t show = ss1000(f, 26, 34) - ss1000(f, 78, 90);
    const int32_t pf = (f - 30) * 2340;  // two slow fans over 56 keys
    const int32_t sf = zref::fx_sin(zref::angle16{static_cast<uint16_t>(pf & 0xFFFF)}).raw;
    const int32_t sw = zref::fx_sin(zref::angle16{static_cast<uint16_t>((pf * 2 + 9000) & 0xFFFF)}).raw;
    g.tail_rest(kBladeSplay + (((sf * 1900) >> 16) * show) / 1000,
                kBladeRise + (((sf * 700) >> 16) * show) / 1000 + (500 * show) / 1000);
    g.q[kBSpike] = quat_mul(g.q[kBSpike], quat_y((((sw * 2200) >> 16) * show) / 1000));
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}

// SLOT 39 -- STRIKE, the quick melee (donor attack1's role). The salto is
// the spectacle; a battle needs a fast bite. Gather back and cock the
// head; SHOOT the front forward-level (the hook opens, the body shoves
// along the blow the way the hit taught us displacement sells); two slow
// decaying overshoots ring out; exact rest at both ends for clean cuts.
static const int32_t kStrikeOpen[5] = {1500, -900, -2600, -3800, -5200};
inline zc::Clip build_strike() {
  zc::Clip c;
  c.slot_id = kSlotStrike;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kStrikeKeys);
  c.root.assign(static_cast<size_t>(kStrikeKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kStrikeKeys) * kBoneCount, zc::quat16_identity());
  // the strike envelope: cock back slowly, SNAP forward, ring out
  static const Key kCock[] = {{0, 0}, {5, 200}, {12, 1000}, {15, 0}, {45, 0}};
  // the lunge OVERSHOOTS its own line for two keys (1080) and the shove is
  // heavier -- the first render read polite (displacement sells the blow)
  static const Key kJab[]  = {{0, 0}, {12, 0}, {15, 1080}, {19, 880},
                              {24, 950}, {30, 890}, {37, 920}, {44, 0}};
  static const Key kShove[] = {{0, 0}, {12, -70}, {14, 60}, {16, 300},
                               {20, 190}, {27, 70}, {36, 15}, {44, 0}};
  constexpr int nC = static_cast<int>(sizeof(kCock) / sizeof(Key));
  constexpr int nJ = static_cast<int>(sizeof(kJab) / sizeof(Key));
  constexpr int nS = static_cast<int>(sizeof(kShove) / sizeof(Key));
  const int32_t per_key = 65536 / kStrikeKeys;
  for (int f = 0; f < kStrikeKeys; ++f) {
    Rig g;
    g.reset();
    const int32_t cock = curve(kCock, nC, f);
    const int32_t jab = curve(kJab, nJ, f);
    int32_t extra[kStanceSlopes] = {};
    for (int k = 1; k <= 4; ++k) {
      // cock: the hook tightens (more climb); jab: the hook OPENS toward
      // the strike line
      extra[k] = (-700 * cock) / 1000 +
                 static_cast<int32_t>(
                     (static_cast<int64_t>(kStrikeOpen[k] - kStanceSlope[k]) * jab) / 1000);
    }
    // RUN 1939 rigidity audit: the strike was the last action clip on a
    // bare apply_stance -- the grounded run and tail held one shape for
    // 1.5 s. The idle's living body now runs underneath at half
    // amplitude; the strike's own deltas ride its wave lane.
    const int32_t rise = idle_body(g, f * per_key, 500, extra);
    // the head: cocks up-back, drives level through the bite, settles
    // (owns the head bone -- overwrites idle_body's write)
    g.q[kBHead] =
        quat_z(kHeadAttitude + (3400 * cock) / 1000 - (1600 * jab) / 1000 +
               extra[1] + extra[2]);
    g.tail_rest(kBladeSplay + (1100 * cock) / 1000,
                kBladeRise + (600 * jab) / 1000);
    g.write(c, f);
    c.root[f * 3 + 0] = fxm(curve(kShove, nS, f));
    c.root[f * 3 + 1] = fxm(rise);
  }
  c.events = {{15, zc::kEvAttack, 0}};
  return c;
}

// SLOT 40 -- NOTIFY, the alert. One sharp motion (a snap is one authored
// action, not jitter): the head comes up, the fins FLARE, the S gathers a
// touch -- then the tell is STILLNESS: the breath nearly stops while the
// head makes two slow scanning turns; release back to rest.
inline zc::Clip build_notify() {
  zc::Clip c;
  c.slot_id = kSlotNotify;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kNotifyKeys);
  c.root.assign(static_cast<size_t>(kNotifyKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kNotifyKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kNotifyKeys;
  for (int f = 0; f < kNotifyKeys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    const int32_t alert = ss1000(f, 1, 7) - ss1000(f, 36, 47);
    int32_t extra[kStanceSlopes] = {};
    for (int k = 1; k <= 3; ++k) extra[k] = (-500 * alert) / 1000;
    // the breath dims while alert -- stillness reads as listening
    const int32_t rise = idle_body(g, ph, 1000 - (alert * 720) / 1000, extra);
    // two slow scanning turns while held
    const int32_t scan =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((f * 3200 + 6000) & 0xFFFF)}).raw;
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude + (4600 * alert) / 1000 + extra[1] + extra[2]),
        quat_y((((scan * 2400) >> 16) * alert) / 1000));
    g.tail_rest(kBladeSplay + (2100 * alert) / 1000,
                kBladeRise + (1400 * alert) / 1000);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}

// SLOT 41 -- BOW. The front lowers in one stately ease, the head dips to
// just above the dirt, the fins fold flat along the tail; a held beat
// with a sub-degree sway so it stays alive; rise back with a small
// arrival overshoot. Court manners for a battle serpent.
static const int32_t kBowOpen[5] = {3600, 2400, -200, -3400, -5800};
// (deepened from 2600/1400/-1200/-4600/-7000: the first render read as a
// nod, not a bow -- the dip now carries the head visibly to the dirt)
inline zc::Clip build_bow() {
  zc::Clip c;
  c.slot_id = kSlotBow;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kBowKeys);
  c.root.assign(static_cast<size_t>(kBowKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kBowKeys) * kBoneCount, zc::quat16_identity());
  static const Key kDip[] = {{0, 0}, {6, 60}, {26, 1000}, {46, 1000},
                             {62, -60}, {68, 20}, {71, 0}};
  constexpr int nD = static_cast<int>(sizeof(kDip) / sizeof(Key));
  const int32_t per_key = 65536 / kBowKeys;
  for (int f = 0; f < kBowKeys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    const int32_t dip = curve(kDip, nD, f);
    int32_t extra[kStanceSlopes] = {};
    for (int k = 1; k <= 4; ++k)
      extra[k] = static_cast<int32_t>(
          (static_cast<int64_t>(kBowOpen[k] - kStanceSlope[k]) * dip) / 1000);
    const int32_t rise = idle_body(g, ph, 520, extra);
    // held beat: a slow sub-degree sway keeps the bow alive
    const int32_t sway =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph + 9000) & 0xFFFF)}).raw;
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude - (7800 * dip) / 1000 + extra[1] + extra[2] +
               ((sway * 300) >> 16)),
        quat_y((((sway * 500) >> 16) * dip) / 1000));
    // fins fold along the tail
    g.tail_rest(kBladeSplay - (1600 * dip) / 1000,
                kBladeRise - (1700 * dip) / 1000);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}

// SLOT 42 -- TALK. The idle's living body with the head doing the
// talking: nod accents of varying depth at irregular intervals, small
// yaw shifts between phrases, one emphatic dip, a blade flick on the
// emphasis. No jaw needed -- the head-language IS the talk read.
inline zc::Clip build_talk() {
  zc::Clip c;
  c.slot_id = kSlotTalk;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kTalkKeys);
  c.root.assign(static_cast<size_t>(kTalkKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kTalkKeys) * kBoneCount, zc::quat16_identity());
  // the phrase track: pitch nods (negative v = a downward nod) and yaws
  static const Key kNod[] = {{0, 0},    {8, -900},  {12, 300},  {18, -1500},
                             {24, 0},   {34, -700}, {40, 200},  {52, -2400},
                             {58, -400}, {66, -1100}, {74, 0},  {84, -600},
                             {90, 100}, {97, 0}};
  static const Key kSide[] = {{0, 0},   {12, 700}, {26, -900}, {44, 500},
                              {58, -1400}, {72, 900}, {86, -400}, {97, 0}};
  constexpr int nN = static_cast<int>(sizeof(kNod) / sizeof(Key));
  constexpr int nS2 = static_cast<int>(sizeof(kSide) / sizeof(Key));
  const int32_t per_key = 65536 / kTalkKeys;
  for (int f = 0; f < kTalkKeys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    const int32_t rise = idle_body(g, ph, 750, nullptr);
    const int32_t nod = curve(kNod, nN, f);
    const int32_t side = curve(kSide, nS2, f);
    g.q[kBHead] = quat_mul(quat_z(kHeadAttitude - nod), quat_y(side));
    // one blade flick on the emphatic phrase
    const int32_t emph = ss1000(f, 50, 54) - ss1000(f, 56, 64);
    g.tail_rest(kBladeSplay + (700 * emph) / 1000, kBladeRise);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
  }
  return c;
}

// SLOT 43 -- SORROW. The stance2 language pushed all the way down: the
// front sinks until the head hangs near the dirt, the breath is one slow
// heavy sigh per loop, and the tail-tip drags a slow half-arc through
// the air behind. Mourning, at the pace of weather.
inline zc::Clip build_sorrow() {
  zc::Clip c;
  c.slot_id = kSlotSorrow;
  c.interpolate = true;
  c.frame_count = static_cast<uint16_t>(kSorrowKeys);
  c.root.assign(static_cast<size_t>(kSorrowKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kSorrowKeys) * kBoneCount, zc::quat16_identity());
  static const int kSagEnv[5] = {0, 700, 1000, 900, 500};
  const int32_t per_key = 65536 / kSorrowKeys;
  for (int f = 0; f < kSorrowKeys; ++f) {
    const int32_t ph = f * per_key;
    Rig g;
    g.reset();
    // one extra deep sigh mid-loop rides the wave lane
    const int32_t sigh = ss1000(f, 24, 42) - ss1000(f, 50, 76);
    int32_t extra[kStanceSlopes] = {};
    for (int k = 1; k <= 4; ++k)
      extra[k] = (3800 * kSagEnv[k]) / 1000 + (500 * sigh * kSagEnv[k] / 1000) / 1000;
    const int32_t rise = idle_body(g, ph, 480, extra);
    // the head hangs; one slow half-cycle wander
    const int32_t sw =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((static_cast<uint32_t>(ph >> 1) + 26000) & 0xFFFF)}).raw;
    g.q[kBHead] = quat_mul(
        quat_z(kHeadAttitude - 5600 - (900 * sigh) / 1000 + extra[1] + extra[2]),
        quat_y((sw * 1500) >> 16));
    // the tail drags a slow half-arc; the fins hang
    const int32_t drag =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((static_cast<uint32_t>(ph >> 1) + 5000) & 0xFFFF)}).raw;
    for (int k = kStanceSlopes - 2; k < kStanceSlopes; ++k)
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_y((drag * 1400) >> 16));
    g.tail_rest((kBladeSplay * 800) / 1000, (kBladeRise * 450) / 1000);
    g.write(c, f);
    c.root[f * 3 + 1] = fxm(rise);
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


// ============ run 0326: SALTO VARIATIONS (the planner's payoff) ============
// Fabian: "We have salto controls now, so you should be able to make some
// salto variations. Hit something in the air (use some target dummy
// creature...). Hit a flying target dummy. One with 6 saltos, maybe you
// get some ideas." The donor format has THREE attack slots -- these are
// attack1/attack2, not extras (ANIMATION-VOCABULARY.md).
constexpr int kSlotAtkDummy = 33;  // strike the grounded target dummy
constexpr int kSlotAtkFly = 34;    // strike the FLYING target dummy
constexpr int kSlotAtkSix = 35;    // six somersaults, the long ground dive
constexpr int kSlotSlowTaunt = 44; // reserved for the slower neck-led taunt
constexpr int kSlotJumpOne = 46;   // immediate spring jump, one salto
constexpr int kSlotJumpMulti = 47; // same jump builder, three saltos
constexpr int kSlotAtkNine = 48;   // nine-salto target attack / limit probe

// Shared attack-outcome timing. Target attacks now ENTER, hold bit-constant,
// extract fully, and only then recoil; the old "hold" moved during recoil and
// could never prove stable embedding.
constexpr int kAtkTargetHoldKeys = 14;
constexpr int kAtkGroundHoldKeys = 8;
constexpr int kAtkExtractKeys = 8;
constexpr int kAtkRecoilKeys = 12;
constexpr int kAtkOutcomeDropKeys = 16;
constexpr int kAtkOutcomeSettleKeys = 10;
// A visible full pull-back, authored by eye from the accepted 760 mm pass.
// The earlier value crossed from 420 mm beyond the target centre to only
// 340 mm before it, leaving the long watchdog volume still threaded on the
// blade.  This longer stroke reads as an extraction and clears the actual
// posed target before recoil rather than merely moving inside it.
constexpr int32_t kAtkExtractionMm = 1200;
constexpr int32_t kAtkDelayedRecoilMm = 260;
constexpr int32_t kAtkLandingBiteMm = 18;
// Per-clip committed 3D terrain/contact declarations.  These are comparison
// envelopes around visually accepted authored impacts, never generation data.
constexpr int32_t kAtkSixGroundStrikeDepthMm = 450;
constexpr int32_t kAtkNineTargetContactDepthMm = 110;

// One exclusive-end phase table owns baking, tracking and diagnostics.  The
// impact value is the authored key that first enters the target/ground; hold
// spans [impact, extract_begin), extraction spans
// [extract_begin, recoil_begin), and recoil cannot begin before that range is
// complete.
struct AttackVariantPhases {
  int compress_end = 0;
  int hold_end = 0;
  int release_end = 0;
  int coil_end = 0;
  int unroll_end = 0;
  int impact = 0;
  int extract_begin = 0;
  int recoil_begin = 0;
  int recover_begin = 0;
  int frame_count = 0;
  int last_key = 0;
};

inline AttackVariantPhases zixx_attack_variant_phases(
    const zc::AttackPlan& p, bool target_hit) {
  AttackVariantPhases v;
  v.compress_end = p.compress_keys;
  v.hold_end = v.compress_end + p.compress_hold_keys;
  v.release_end = v.hold_end + p.release_keys;
  v.coil_end = v.release_end + p.coil_keys;
  v.unroll_end = v.coil_end + p.unroll_keys;
  v.impact = v.unroll_end + p.plunge_keys;
  v.extract_begin = v.impact +
                    (target_hit ? kAtkTargetHoldKeys : kAtkGroundHoldKeys);
  v.recoil_begin = v.extract_begin + kAtkExtractKeys;
  v.recover_begin = v.recoil_begin + (target_hit ? kAtkRecoilKeys : 0);
  v.frame_count = v.recover_begin + kAtkOutcomeDropKeys +
                  kAtkOutcomeSettleKeys + 1;
  v.last_key = v.frame_count - 1;
  return v;
}

inline bool zixx_variant_air_hit(uint16_t slot) {
  return slot == kSlotAtkDummy || slot == kSlotAtkFly ||
         slot == kSlotAtkNine;
}

inline int zixx_attack_variant_key_count(const zc::AttackPlan& p,
                                         bool target_hit) {
  return zixx_attack_variant_phases(p, target_hit).frame_count;
}

// integer atan2 in 1/1000 turns (offline bake only; deterministic scan --
// 4096 candidates, argmax dot, ~0.09 deg steps)
inline int32_t atan2_mturns(int32_t y, int32_t x) {
  int64_t best = INT64_MIN;
  int32_t best_phi = 0;
  for (int i = 0; i < 4096; ++i) {
    const uint16_t phi = static_cast<uint16_t>(i << 4);
    const int32_t c = zref::fx_cos(zref::angle16{phi}).raw;
    const int32_t sn = zref::fx_sin(zref::angle16{phi}).raw;
    const int64_t dot = static_cast<int64_t>(c) * x + static_cast<int64_t>(sn) * y;
    if (dot > best) { best = dot; best_phi = phi; }
  }
  return (best_phi * 1000) / 65536;  // 1/1000 turns
}

// Add only the final spear-alignment fraction after preserving the requested
// whole-salto count.  Shared by the baker and the trajectory validator so a
// diagnostic can never silently count a nearby, differently oriented plan.
inline zc::AttackPlan zixx_orient_variant_spin(zc::AttackPlan p) {
  const int32_t phi = atan2_mturns(p.intercept_y_mm - p.apex_mm,
                                   p.intercept_x_mm - p.apex_fwd_mm);
  int32_t frac = phi - 500;
  while (frac < 0) frac += 1000;
  const int32_t turns = p.spin_mturns / 1000;
  p.spin_mturns = turns * 1000 + frac;
  return p;
}

// The variant baker: local phase poses (the C2 slices' own source keys)
// re-timed to the PLAN's phases, the plan's trajectory on the root, the
// spin on bone 0 with the golden's own coil re-pivot law, and an authored
// outcome tail (ground stick + recover, or mid-air hit + fall out of the
// sky + settle). Pure integers; baked, replay-exact.
inline zc::Clip build_attack_variant(uint16_t slot, zc::AttackPlan p,
                                     bool air_hit_outcome) {
  const zc::Clip local = build_attack(true);
  const zc::Clip recoil = build_air_hit();
  // orient the spear along the committed AIM line (apex -> intercept):
  // that is the line the eye reads and the line the TIP travels. The
  // spear_d fields hold the ROOT's offset path since the tail-tip lock
  // (tip lead + kBodyY carry) -- orienting on THOSE pitched the dummy
  // variants ~30 deg too steep (striketip probe, this run). The local
  // spear pose at theta 0 points -X; preserve the requested whole turns and
  // add the shared final aim-line alignment fraction.
  p = zixx_orient_variant_spin(p);
  const int32_t turns = p.spin_mturns / 1000;
  const AttackVariantPhases phase =
      zixx_attack_variant_phases(p, air_hit_outcome);
  const int tc = phase.compress_end;
  const int th = phase.hold_end;
  const int t0 = phase.release_end;
  const int t1 = phase.coil_end;
  const int t2 = phase.unroll_end;
  const int t3 = phase.impact;
  const int extract0 = phase.extract_begin;
  const int recoil0 = phase.recoil_begin;
  const int recover0 = phase.recover_begin;
  const int total = phase.frame_count;
  zc::Clip c;
  c.slot_id = slot;
  c.interpolate = true;
  c.hold_last = true;  // ends settled on the rest pose
  c.frame_count = static_cast<uint16_t>(total);
  c.root.assign(static_cast<size_t>(total) * 3, 0);
  c.quats.assign(static_cast<size_t>(total) * kBoneCount, zc::quat16_identity());
  const ChoreoSample end_s = zixx_plan_sample(p, t3);
  const int64_t aim_dx = p.intercept_x_mm - p.apex_fwd_mm;
  const int64_t aim_dy = p.intercept_y_mm - p.apex_mm;
  int32_t aim_len = static_cast<int32_t>(zref::isqrt_u64(
      static_cast<uint64_t>(aim_dx * aim_dx + aim_dy * aim_dy)));
  if (aim_len < 1) aim_len = 1;
  const int32_t extract_x = end_s.x_mm -
      static_cast<int32_t>(aim_dx * kAtkExtractionMm / aim_len);
  const int32_t extract_y = end_s.y_mm -
      static_cast<int32_t>(aim_dy * kAtkExtractionMm / aim_len);
  // rest rig for the settle blend
  Rig rest;
  rest_rig(rest);
  for (int k = 0; k < total; ++k) {
    // ---- the local body pose for this phase --------------------------------
    Rig g;
    g.reset();
    int32_t curl = 0;  // how coiled the body is (drives the wheel re-pivot)
    if (k < t3) {
      int lk;
      if (k < tc) {
        lk = tc > 1 ? (k * kSaltoCompressEndKey) / (tc - 1)
                    : kSaltoCompressEndKey;
      } else if (k < th) {
        lk = kSaltoCompressHoldEndKey;
      } else if (k < t0) {
        lk = kSaltoCompressHoldEndKey +
             ((k - th) * (kSaltoCoilPoseKey - kSaltoCompressHoldEndKey)) /
                 (p.release_keys > 0 ? p.release_keys : 1);
      } else if (k < t1) {
        lk = kSaltoCoilPoseKey;
      } else if (k < t2) {
        lk = kSaltoUnrollStartKey +
             ((k - t1) * (kSaltoUnrollEndKey - kSaltoUnrollStartKey)) /
                 (p.unroll_keys > 0 ? p.unroll_keys : 1);
      } else {
        lk = kAtkImpactKey;
      }
      curl = curve(kAtkCurl, kAtkCurlN, lk);
      for (int b = 0; b < kBoneCount; ++b)
        g.q[b] = local.quats[static_cast<size_t>(lk) * kBoneCount + b];
    } else if (k < recoil0) {
      // Impact hold and extraction are the exact spear pose. No root or bone
      // changes are permitted during the embedded hold.
      for (int b = 0; b < kBoneCount; ++b)
        g.q[b] = local.quats[static_cast<size_t>(kAtkImpactKey) * kBoneCount + b];
    } else if (k < recover0 && air_hit_outcome) {
      const int rk = k - recoil0;
      const int use = rk < static_cast<int>(recoil.frame_count)
                          ? rk
                          : static_cast<int>(recoil.frame_count) - 1;
      for (int b = 0; b < kBoneCount; ++b)
        g.q[b] = recoil.quats[static_cast<size_t>(use) * kBoneCount + b];
    } else {
      // Drop and settle: reclaim the signature S only after extraction and,
      // for target hits, the delayed recoil.
      Rig from;
      from.reset();
      if (air_hit_outcome) {
        const int rmax = static_cast<int>(recoil.frame_count) - 1;
        for (int b = 0; b < kBoneCount; ++b)
          from.q[b] = recoil.quats[static_cast<size_t>(rmax) * kBoneCount + b];
      } else {
        for (int b = 0; b < kBoneCount; ++b)
          from.q[b] = local.quats[static_cast<size_t>(kAtkImpactKey) * kBoneCount + b];
      }
      const int j = k - recover0;
      const int32_t t = ss1000(j, 0,
                               kAtkOutcomeDropKeys + kAtkOutcomeSettleKeys - 2);
      for (int b = 0; b < kBoneCount; ++b)
        g.q[b] = zc::quat16_nlerp(from.q[b], rest.q[b], t, 1000);
    }
    // ---- spin + trajectory -------------------------------------------------
    ChoreoSample sm;
    if (k < t3) {
      sm = zixx_plan_sample(p, k);
    } else if (k < extract0) {
      // Entered target / planted ground: bit-constant root and spear pose.
      sm = end_s;
    } else if (k < recoil0) {
      // Pull completely back along the committed line before any recoil.
      const int j = k - extract0;
      const int32_t e = ss1000(j, 0, kAtkExtractKeys - 1);
      sm = end_s;
      sm.x_mm += ((extract_x - end_s.x_mm) * e) / 1000;
      sm.y_mm += ((extract_y - end_s.y_mm) * e) / 1000;
    } else if (k < recover0) {
      // Delayed target recoil, already clear: a small further kick opposite
      // the impact line, with no chance to jitter inside the model.
      const int j = k - recoil0;
      const int32_t e = ss1000(j, 0, 3) - ss1000(j, 5, kAtkRecoilKeys - 1);
      sm = end_s;
      sm.x_mm = extract_x - static_cast<int32_t>(
          (aim_dx * kAtkDelayedRecoilMm * e) / (static_cast<int64_t>(aim_len) * 1000));
      sm.y_mm = extract_y - static_cast<int32_t>(
          (aim_dy * kAtkDelayedRecoilMm * e) / (static_cast<int64_t>(aim_len) * 1000));
    } else {
      // Fall/recover to the ground beside the strike after the weapon is clear.
      const int j = k - recover0;
      const int32_t tj = j >= kAtkOutcomeDropKeys
                             ? 1000
                             : (j * 1000) / kAtkOutcomeDropKeys;
      const int32_t tt = (tj * tj) / 1000;
      const int32_t gx = extract_x - (air_hit_outcome ? 420 : -380);
      sm.x_mm = extract_x + ((gx - extract_x) * tj) / 1000;
      sm.y_mm = extract_y - static_cast<int32_t>(
          (static_cast<int64_t>(extract_y) * tt) / 1000);
      if (j >= kAtkOutcomeDropKeys) sm.y_mm = 0;
      if (j == kAtkOutcomeDropKeys || j == kAtkOutcomeDropKeys + 1)
        sm.y_mm = -kAtkLandingBiteMm;
      // Complete only the alignment fraction after the count's whole turns;
      // every count therefore reaches the same upright landing phase.
      const int32_t th_end = (turns + 1) * 1000;
      sm.theta = static_cast<int32_t>(
          ((static_cast<int64_t>(p.spin_mturns) +
            (static_cast<int64_t>(th_end - p.spin_mturns) *
             ss1000(j, 0, kAtkOutcomeDropKeys + 2)) /
                1000) *
           65536) /
          1000);
    }
    const uint16_t th16 = static_cast<uint16_t>(sm.theta & 0xFFFF);
    const zc::quat16 spin_q = quat_z(sm.theta);
    g.q[kBSpine0] = quat_mul(spin_q, g.q[kBSpine0]);
    // the wheel re-pivot (the golden's law): while coiled, the spin acts
    // about the coil centre (0, kCoilR) above the nose, scaled by curl
    const int32_t sth = zref::fx_sin(zref::angle16{th16}).raw;
    const int32_t cth = zref::fx_cos(zref::angle16{th16}).raw;
    const int32_t piv_x = static_cast<int32_t>((static_cast<int64_t>(kCoilR) * sth) >> 16);
    const int32_t piv_y = kCoilR - static_cast<int32_t>((static_cast<int64_t>(kCoilR) * cth) >> 16);
    g.write(c, k);
    c.root[k * 3 + 0] = fxm(sm.x_mm + (piv_x * curl) / 1000);
    c.root[k * 3 + 1] = fxm(sm.y_mm + (piv_y * curl) / 1000);
  }
  // event: the strike lands at t3 (collision verdict in the sim; here the
  // baked showcase's own moment)
  c.events = {{static_cast<uint16_t>(t3), zc::kEvAttack, air_hit_outcome ? uint8_t{1} : uint8_t{0}}};
  return c;
}

// the three variants' PLANS, fixed for the reel showcases (the sim would
// make its own): the grounded dummy stands 4.6 m out; the flyer hovers at
// 3.2 m; the six-salto dives on a far ground mark with its spin forced.
// EXTRACTED (RUN 1939): the reel's tracking camera needs the same plan.
inline zc::AttackPlan zixx_variant_plan(uint16_t slot) {
  if (slot == kSlotAtkDummy) return zixx_plan_attack(4600, 350, 0, 0);
  if (slot == kSlotAtkFly) return zixx_plan_attack(3800, 3200, 0, 0);
  if (slot == kSlotAtkNine) {
    zc::AttackPlan p = zixx_plan_attack(8500, 350, 0, 0);
    p.spin_mturns = 9000;
    p.apex_mm = kAtkNineApexLift;  // exactly 2x slot 35's 12 m apex
    p.coil_keys = 72;              // spend nine coherent turns in one wheel
    zixx_plan_lock_spear(p, kAtkNineApexLift);
    return p;
  }
  zc::AttackPlan p = zixx_plan_attack(5200, 0, 0, 0);
  p.spin_mturns = 6000;  // the preserved ask: SIX somersaults
  p.apex_mm = kAtkApexLift;
  p.coil_keys = 44;
  zixx_plan_lock_spear(p);
  return p;
}

// THE VARIANT CAMERA'S TRACK (RUN 1939, owner: "Salto camera is too
// jittery"). The old camera followed the variant clip's BAKED ROOT, which
// carries the coil re-pivot orbit disp = c - R(theta)c -- at six
// somersaults that is six 485 mm camera orbits per flight, and the shot
// shook in time with the spin. The golden attack's camera already learned
// this lesson ("Not the decoded root, which also carries the coil
// re-pivot wobble"): follow the PLAN's smooth trajectory instead -- where
// the animal is GOING, never how it is oriented. Same phase math as the
// baker, minus the wheel orbit, minus the recoil kick.
inline void zixx_variant_track(uint16_t slot, int key,
                               int32_t& x_mm, int32_t& y_mm) {
  const zc::AttackPlan p = zixx_variant_plan(slot);
  const bool target_hit = zixx_variant_air_hit(slot);
  const AttackVariantPhases phase =
      zixx_attack_variant_phases(p, target_hit);
  const int t3 = phase.impact;
  const int extract0 = phase.extract_begin;
  const int recoil0 = phase.recoil_begin;
  const int recover0 = phase.recover_begin;
  const ChoreoSample end_s = zixx_plan_sample(p, t3);
  const int64_t aim_dx = p.intercept_x_mm - p.apex_fwd_mm;
  const int64_t aim_dy = p.intercept_y_mm - p.apex_mm;
  int32_t aim_len = static_cast<int32_t>(zref::isqrt_u64(
      static_cast<uint64_t>(aim_dx * aim_dx + aim_dy * aim_dy)));
  if (aim_len < 1) aim_len = 1;
  const int32_t ex = end_s.x_mm -
      static_cast<int32_t>(aim_dx * kAtkExtractionMm / aim_len);
  const int32_t ey = end_s.y_mm -
      static_cast<int32_t>(aim_dy * kAtkExtractionMm / aim_len);
  if (key <= t3) {
    const ChoreoSample sm = zixx_plan_sample(p, key);
    x_mm = sm.x_mm;
    y_mm = sm.y_mm;
  } else if (key < extract0) {
    x_mm = end_s.x_mm;
    y_mm = end_s.y_mm;
  } else if (key < recoil0) {
    const int32_t e = ss1000(key - extract0, 0, kAtkExtractKeys - 1);
    x_mm = end_s.x_mm + ((ex - end_s.x_mm) * e) / 1000;
    y_mm = end_s.y_mm + ((ey - end_s.y_mm) * e) / 1000;
  } else if (key < recover0) {
    const int j = key - recoil0;
    const int32_t e = ss1000(j, 0, 3) - ss1000(j, 5, kAtkRecoilKeys - 1);
    x_mm = ex - static_cast<int32_t>(
        (aim_dx * kAtkDelayedRecoilMm * e) / (static_cast<int64_t>(aim_len) * 1000));
    y_mm = ey - static_cast<int32_t>(
        (aim_dy * kAtkDelayedRecoilMm * e) / (static_cast<int64_t>(aim_len) * 1000));
  } else {
    const int j = key - recover0;
    const int32_t tj = j >= kAtkOutcomeDropKeys
                           ? 1000
                           : (j * 1000) / kAtkOutcomeDropKeys;
    const int32_t tt = (tj * tj) / 1000;
    const int32_t gx = ex - (target_hit ? 420 : -380);
    x_mm = ex + ((gx - ex) * tj) / 1000;
    y_mm = ey - static_cast<int32_t>(
        (static_cast<int64_t>(ey) * tt) / 1000);
    if (j >= kAtkOutcomeDropKeys) y_mm = 0;
  }
}

inline zc::Clip build_attack_dummy() {
  return build_attack_variant(kSlotAtkDummy, zixx_variant_plan(kSlotAtkDummy),
                              zixx_variant_air_hit(kSlotAtkDummy));
}
inline zc::Clip build_attack_fly() {
  return build_attack_variant(kSlotAtkFly, zixx_variant_plan(kSlotAtkFly),
                              zixx_variant_air_hit(kSlotAtkFly));
}
inline zc::Clip build_attack_six() {
  return build_attack_variant(kSlotAtkSix, zixx_variant_plan(kSlotAtkSix),
                              zixx_variant_air_hit(kSlotAtkSix));
}
inline zc::Clip build_attack_nine() {
  return build_attack_variant(kSlotAtkNine, zixx_variant_plan(kSlotAtkNine),
                              zixx_variant_air_hit(kSlotAtkNine));
}

// PROGRAMMABLE IMMEDIATE JUMP FAMILY (direction #9). One builder owns the
// spring, wheel, whole-turn count, exact ground return and signature-S settle.
// It differs from AttackPlan only in outcome: this is a ground stunt, so there
// is no unroll/spear/target branch.
struct JumpPlan {
  uint16_t slot = 0;
  uint16_t compress_keys = 5;
  uint16_t compress_hold_keys = 2;
  // Six authored intervals keep the launch immediate (0.20 s) while letting
  // the complete animal visibly pay the flat spring into its wheel.  Four
  // intervals snapped the rear half through a metre-scale station step.
  uint16_t release_keys = 6;
  uint16_t flight_keys = 38;
  uint16_t landing_keys = 6;
  uint16_t settle_keys = 14;
  int32_t apex_mm = 4800;
  int32_t salto_count = 1;
};
constexpr int32_t kJumpLandingBiteMm = 10;
constexpr int kJumpLandingGatherKeys = 5;
// Every native 60 Hz frame was reviewed after the six-key release was authored.
// The fastest accepted sample is the intentional landing slam (1121 mm on the
// three-salto take), not a launch reset; 1150 keeps a narrow regression guard.
constexpr int32_t kJumpMaxStationStepMm = 1150;

inline JumpPlan zixx_jump_plan(uint16_t slot, int32_t count) {
  JumpPlan p;
  p.slot = slot;
  p.salto_count = count < 1 ? 1 : (count > 9 ? 9 : count);
  return p;
}

struct JumpPhases {
  int compress_end = 0;
  int hold_end = 0;
  int launch_key = 0;
  int landing_key = 0;
  int landing_end = 0;
  int frame_count = 0;
  int last_key = 0;
};

inline JumpPhases zixx_jump_phases(const JumpPlan& p) {
  JumpPhases v;
  v.compress_end = p.compress_keys;
  v.hold_end = v.compress_end + p.compress_hold_keys;
  v.launch_key = v.hold_end + p.release_keys;
  v.landing_key = v.launch_key + p.flight_keys;
  v.landing_end = v.landing_key + p.landing_keys;
  v.frame_count = v.landing_end + p.settle_keys + 1;
  v.last_key = v.frame_count - 1;
  return v;
}

inline int zixx_jump_key_count(const JumpPlan& p) {
  return zixx_jump_phases(p).frame_count;
}

struct JumpMotionSample {
  int32_t spring = 0;
  int32_t curl = 0;
  int32_t theta = 0;
  int32_t lift = 0;
};

// One deterministic motion sample serves the baker, camera and limit probe.
// Keeping the count/phase law here prevents a camera-only or diagnostic-only
// copy from hiding a rotation-wrap or landing drift in the actual clip.
inline JumpMotionSample zixx_jump_motion_sample(const JumpPlan& p, int f) {
  const JumpPhases phase = zixx_jump_phases(p);
  const int tc = phase.compress_end;
  const int th = phase.hold_end;
  const int launch = phase.launch_key;
  const int land = phase.landing_key;
  JumpMotionSample m;
  if (f < tc) {
    m.spring = tc > 1 ? ss1000(f, 0, tc - 1) : 1000;
  } else if (f < th) {
    m.spring = 1000;
  } else if (f < launch) {
    const int32_t u = ss1000(f, th, launch);
    m.spring = 1000 - u;
    m.curl = u;
  } else if (f <= land) {
    const int j = f - launch;
    const int32_t t = p.flight_keys > 0
                          ? (j * 1000) / p.flight_keys
                          : 1000;
    const int32_t sm = (t * t / 1000) * (3000 - 2 * t) / 1000;
    m.theta = static_cast<int32_t>(
        (static_cast<int64_t>(p.salto_count) * 65536 * sm) / 1000);
    m.lift = static_cast<int32_t>(
        (static_cast<int64_t>(4) * p.apex_mm * t * (1000 - t)) /
        1000000);
    // Release has already paid the compressed S continuously into the complete
    // wheel by the launch key.  Keep that wheel through flight, then gather the
    // last few keys into the landing spring.  The earlier version restarted
    // curl at zero on the launch key, producing a real 2.67 m one-tick shape
    // discontinuity between the final release key and takeoff.
    const int gather_in = p.flight_keys > 0
                              ? (kJumpLandingGatherKeys * 1000) / p.flight_keys
                              : 0;
    const int gather_out = 1000 - gather_in;
    if (t > gather_out && gather_in > 0) {
      m.spring = ((t - gather_out) * 1000) / gather_in;
      if (m.spring > 1000) m.spring = 1000;
      m.curl = 1000 - m.spring;
    } else {
      m.curl = 1000;
    }
  } else {
    // Absorb in the loaded spring, then recover slowly and hold exact rest.
    const int j = f - land;
    m.spring = 1000 - ss1000(j, 1, p.landing_keys + p.settle_keys - 4);
    if (m.spring < 0) m.spring = 0;
    m.theta = p.salto_count * 65536;  // whole counts: identity at landing
  }
  return m;
}

inline void zixx_jump_track(const JumpPlan& p, int key,
                            int32_t& x_mm, int32_t& y_mm) {
  if (key < 0) key = 0;
  const int last = zixx_jump_phases(p).last_key;
  if (key > last) key = last;
  const JumpMotionSample m = zixx_jump_motion_sample(p, key);
  x_mm = 0;
  y_mm = m.lift + spring_root_drop(m.spring);
}

inline zc::Clip build_jump(const JumpPlan& p) {
  const JumpPhases phase = zixx_jump_phases(p);
  const int land = phase.landing_key;
  const int total = phase.frame_count;
  zc::Clip c;
  c.slot_id = p.slot;
  c.interpolate = true;
  c.hold_last = true;
  c.frame_count = static_cast<uint16_t>(total);
  c.root.assign(static_cast<size_t>(total) * 3, 0);
  c.quats.assign(static_cast<size_t>(total) * kBoneCount,
                 zc::quat16_identity());
  const int32_t coil_pitch = -(65536 / (kSpineBones - 2));
  for (int f = 0; f < total; ++f) {
    const JumpMotionSample motion = zixx_jump_motion_sample(p, f);
    const int32_t spring = motion.spring;
    const int32_t curl = motion.curl;
    const int32_t theta = motion.theta;
    const int32_t lift = motion.lift;

    Rig g;
    g.reset();
    const int32_t drop = apply_spring_stance(g, 1000 - curl, spring);
    for (int k = 1; k < kSpineBones - 1; ++k)
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k], quat_z((coil_pitch * curl) / 1000));
    g.q[kBHead] = quat_z(spring_head_attitude(1000 - curl, spring) +
                          (coil_pitch * curl) / 1000);
    g.tail_rest(kBladeSplay + (spring * kSpringBladeFlare) / 1000,
                (kBladeRise * (1000 - curl)) / 1000,
                (kBladeUpBias * (1000 - curl)) / 1000);
    g.q[kBSpine0] = quat_mul(quat_z(theta), g.q[kBSpine0]);
    const uint16_t th16 = static_cast<uint16_t>(theta & 0xFFFF);
    const int32_t sth = zref::fx_sin(zref::angle16{th16}).raw;
    const int32_t cth = zref::fx_cos(zref::angle16{th16}).raw;
    const int32_t piv_x = static_cast<int32_t>(
        (static_cast<int64_t>(kCoilR) * sth) >> 16);
    const int32_t piv_y = kCoilR - static_cast<int32_t>(
        (static_cast<int64_t>(kCoilR) * cth) >> 16);
    g.write(c, f);
    c.root[f * 3 + 0] = fxm((piv_x * curl) / 1000);
    int32_t y = lift + drop + (piv_y * curl) / 1000;
    if (f == land || f == land + 1) y -= kJumpLandingBiteMm;
    c.root[f * 3 + 1] = fxm(y);
  }
  c.events = {{static_cast<uint16_t>(land), zc::kEvFoot, 4}};
  return c;
}

inline zc::Clip build_jump_one() {
  return build_jump(zixx_jump_plan(kSlotJumpOne, 1));
}
inline zc::Clip build_jump_multi() {
  return build_jump(zixx_jump_plan(kSlotJumpMulti, 3));
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
    // the skull bone: child of the root, PIVOT AT THE SKULL'S OWN CENTRE
    // (2026-08-28; was at the nose). Pitching about the nose swung the
    // skull's REAR down into the hook -- the owner's look-up attitude dug
    // the cranium ~235 mm into the dive stroke (probe). About the centroid
    // (~station 3.5) the same axis angle lifts the nose and drops the rear
    // half as much each, so the culminating head rides the hook the way
    // Side.png nests it. All of the cranium's pitch lives on this bone
    // (kHeadAttitude plus per-clip head motion).
    sk.bones[kBHead] = zc::Bone{kBSpine0, -fxm(kHeadPivotMm), 0, 0};
    const int32_t pupil_x = station_x(kPupilStation) - kHeadPivotMm;
    sk.bones[kBPupilL] = zc::Bone{kBHead, -fxm(pupil_x), 0, 0};
    sk.bones[kBPupilR] = zc::Bone{kBHead, -fxm(pupil_x), 0, 0};
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

    // ---- MOVING PUPILS: one deforming boundary-to-boundary stripe per eye --
    // The yellow eyeball and ink perimeter remain paint on the swollen head.
    // Each orange stripe is a seven-ring shallow ribbon. Its middle three
    // rings follow the pupil pivot, its two shoulder rings blend back toward
    // the skull, and its tapered tips use the same bind as the painted eye.
    // The tips therefore stay joined to the eyeball boundary while diagonal
    // travel stretches/contracts the arms continuously. This is geometry and
    // skinning, not a screen-space UV trick, so the marking cannot texture-swim.
    {
      int32_t eye_rx = 0, eye_rz = 0;
      head_ring(kPupilStation, eye_rx, eye_rz);
      const Bind eye_bind = head_station_bind(kPupilStation);
      const int32_t eye_x = -station_x(kPupilStation);
      struct PupilStripeRing {
        int32_t angle_a16;
        int32_t half_width_mm;
        uint8_t follow;  // 0 = painted eye, 1 = shoulder blend, 2 = pupil
      };
      static constexpr PupilStripeRing kStripe[] = {
          {-kPupilStripeBoundaryA16, kPupilStripeTipHalfWidthMm, 0},
          {-kPupilStripeShoulderA16, kPupilStripeArmHalfWidthMm, 1},
          {-kPupilCoreHalfAngleA16, kPupilStripeCoreEdgeMm, 2},
          {0, kPupilCoreHalfWidthMm, 2},
          {kPupilCoreHalfAngleA16, kPupilStripeCoreEdgeMm, 2},
          {kPupilStripeShoulderA16, kPupilStripeArmHalfWidthMm, 1},
          {kPupilStripeBoundaryA16, kPupilStripeTipHalfWidthMm, 0},
      };
      for (int side = 0; side < 2; ++side) {
        const uint8_t pupil_bone = side == 0 ? kBPupilL : kBPupilR;
        const int32_t flank = side == 0 ? 1 : -1;
        zc::RingPart stripe;
        stripe.chain = true;
        stripe.caps = zc::kCapTop | zc::kCapBot;
        stripe.page = kTilePupil;
        set_rgb(stripe, kOrange);
        for (const PupilStripeRing& sr : kStripe) {
          const zref::angle16 a{static_cast<uint16_t>(sr.angle_a16 & 0xFFFF)};
          const int32_t rise =
              static_cast<int32_t>((static_cast<int64_t>(eye_rz) * zref::fx_sin(a).raw) >> 16);
          const int32_t radial =
              static_cast<int32_t>((static_cast<int64_t>(eye_rx) * zref::fx_cos(a).raw) >> 16);
          zc::RingSpec rs;
          rs.y = fxm(kBodyY + rise);
          rs.radius = fxm(sr.half_width_mm);
          rs.segments = static_cast<uint8_t>(kPupilStripeSides);
          rs.cx = fxm(eye_x);
          rs.cz = fxm(flank * (radial + kPupilStripeSurfaceLiftMm));
          rs.rx = fxm(sr.half_width_mm);
          rs.rz = fxm(kPupilStripeDepthMm);
          if (sr.follow == 0) {
            rs.b0 = eye_bind.b0;
            rs.b1 = eye_bind.b1;
            rs.w0 = eye_bind.w0;
          } else if (sr.follow == 1) {
            rs.b0 = pupil_bone;
            rs.b1 = kBHead;
            rs.w0 = kPupilStripeShoulderFollow;
          } else {
            rs.b0 = pupil_bone;
            rs.b1 = pupil_bone;
            rs.w0 = 64;
          }
          stripe.rings.push_back(rs);
        }
        parts.push_back(stripe);
      }
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
        // LEAF profile (RUN 0757): rises 700 -> 1000 by t=300, then one long
        // straight-ish taper to the point -- the sheet's sliver, not the old
        // root-heavy paddle (k = 1000 - t^2/1000).
        const int32_t k = t < 300 ? 700 + t
                                  : (1000 * (1000 - t)) / 700;
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
    bank.clips.push_back(build_pupil_proof());
    // C2: the phase vocabulary, sliced from the local-body attack at shared
    // keys; the two authored phases start/end on the exact spear pose. The
    // declared seams below are ENFORCED by compile_creature -- a phase edit
    // that breaks a seam fails the whole creature compile.
    {
      const zc::Clip atk_local = build_attack(true);
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkCompress, 0, 17));
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkRelease, 17, 29));
      bank.clips.push_back(duplicate_pose_clip(atk_local, kSlotAtkCoil, 29));
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkUnroll, 52, 60));
      bank.clips.push_back(build_spear_flex());
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkStick, 74, 75));
      bank.clips.push_back(build_air_hit());
      bank.clips.push_back(slice_clip(atk_local, kSlotAtkRecover, 224, 239));
      bank.seams = {
          {kSlotAtkCompress, 17, kSlotAtkRelease, 0},
          {kSlotAtkRelease, 12, kSlotAtkCoil, 0},
          {kSlotAtkCoil, 0, kSlotAtkCoil, 1},
          {kSlotAtkCoil, 1, kSlotAtkUnroll, 0},
          {kSlotAtkUnroll, 8, kSlotAtkSpearFlex, 0},
          {kSlotAtkSpearFlex, 0, kSlotAtkSpearFlex, 9},
          {kSlotAtkSpearFlex, 0, kSlotAtkStick, 0},
          {kSlotAtkStick, 0, kSlotAtkStick, 1},
          {kSlotAtkStick, 1, kSlotAtkAirHit, 0},
          {kSlotAtkAirHit, 0, kSlotAtkAirHit, 11},
          {kSlotAtkAirHit, 11, kSlotAtkRecover, 0},
          {kSlotAtkRecover, 15, kSlotAtkCompress, 0},
      };
    }
    // run 0326: the vocabulary close-out (see ANIMATION-VOCABULARY.md)
    bank.clips.push_back(build_knock());
    bank.clips.push_back(build_getup());
    bank.clips.push_back(build_hitfloor());
    bank.clips.push_back(build_damage(kSlotDmgRight, 0));
    bank.clips.push_back(build_damage(kSlotDmgBack, 1));
    bank.clips.push_back(build_damage(kSlotDmgLeft, 2));
    bank.clips.push_back(build_damage(kSlotDmgTop, 3));
    bank.clips.push_back(build_run());
    bank.clips.push_back(build_death1());
    bank.clips.push_back(build_taunt());
    {
      // the corpse starts on the death's own final key, byte-for-byte
      const zc::Clip* d0 = nullptr;
      for (const zc::Clip& cc : bank.clips)
        if (cc.slot_id == 6) d0 = &cc;
      zc::Clip corpse = build_corpse(*d0);
      bank.clips.push_back(corpse);
    }
    // run 0326: the salto variations (attack1/attack2 -- the planner's payoff)
    bank.clips.push_back(build_attack_dummy());
    bank.clips.push_back(build_attack_fly());
    bank.clips.push_back(build_attack_six());
    bank.clips.push_back(build_jump_one());
    bank.clips.push_back(build_jump_multi());
    bank.clips.push_back(build_attack_nine());
    // RUN 0757: the vocabulary close-out (see the section above)
    bank.clips.push_back(build_stance2());
    bank.clips.push_back(build_tumble());
    bank.clips.push_back(build_death2());
    bank.clips.push_back(build_idle2());
    bank.clips.push_back(build_idle3());
    bank.clips.push_back(build_strike());
    bank.clips.push_back(build_notify());
    bank.clips.push_back(build_bow());
    bank.clips.push_back(build_talk());
    bank.clips.push_back(build_sorrow());
    // the knockdown chain's shared poses and the death->corpse handoff are
    // asset invariants: compile_creature byte-compares them and FAILS the
    // creature if an edit ever splits them
    bank.seams.push_back({kSlotKnock, kKnockKeys - 1, kSlotGetUp, 0});
    bank.seams.push_back({kSlotHitFloor, kHitFloorKeys - 1, kSlotGetUp, 0});
    bank.seams.push_back({6, kDeathKeys - 1, kSlotCorpse, 0});

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
