// MANAFOLD (creature 02) — the effects: THE MANA MENU (pass 2), the FX.LIGHTNING bolt,
// the centre glow. This file IS the effects knob block: every rate, life,
// size and colour below is an owner knob.
//
// Direction 2 §3 AXED the ten-emitter particle set ("too tiny and too many.
// We want cheaper, easier, but more impressive and way more good looking")
// and asked for MANY examples to pick from. This header therefore carries
// SIX NAMED CANDIDATES, each a variant on the proven glow-splat machinery
// (big soft additive splats — a particle caps at 15.9 px of flat colour and
// can never be a soft blob, 09-ENGINE-GOTCHAS §11):
//
//   1 caged pulsar   — a core inside the ring pocket, a breathing halo the
//                      arms silhouette against (the pulse is 30 px of halo
//                      swing, not a 9-count palette flicker)
//   2 big plasma     — three large Lorentzian blobs, blue/violet/gold
//   3 plasma bullets — mini splats launched from the ring, each trailing
//                      stamped ghosts (smear route 2: the deliberately
//                      steppy "dropped frame buffer" read)
//   4 LIGHTNING      — the FX.LIGHTNING recurrence kept verbatim, drawn as
//                      a CONTINUOUS two-layer path (stamps along segments,
//                      hot core over calm halo), decaying ghost of the
//                      previous strike, an anamorphic flash on the strike
//                      frame. Direction 2 §0: actual lightning.
//   5 two-tone boil  — blue core / violet outer on counter-rotating ramps
//                      (CLUT rotation: churn for zero pixel cost)
//   6 the drip       — a few LARGE opaque droplets, the one non-additive
//                      read (kept from the old set's only good idea)
//
// Consumer contract: include after `namespace zc = zref::creature;` with the
// zref headers available. Everything here is reel-side AUTHORING over
// exported engine primitives — no engine change.
//
// COLOUR LAW (proven at spike S3): additive colours must sit UNDER the
// channel ceiling or they whiten on the pink pigment.
//
// THE BOLT follows reports/ADDLIGHTNING.md's FX.LIGHTNING recurrence
// exactly — P_i = lerp(start, end, i/N) + perp1·jitter(seed, phase, i) +
// perp2·jitter(seed², phase, i), ≤24 segments, ≤2 bounded branches — so
// this authoring migrates unchanged onto the FORGE.PRIM ribbon evaluator
// the day it lands (the hardware ask is on the record in zhaozhou/reports/).

#ifndef ZHAO_REEL_MANAFOLD_FX_H
#define ZHAO_REEL_MANAFOLD_FX_H

#include <cstring>  // std::memset -- the mist plane's shift clears vacated cells
#include <vector>

#include "manafold_clips.h"

namespace u02 {

// (fx_hash / fx_jit moved to manafold_art.h -- shared with the fold
// choreography timeline in manafold_clips.h.)

// ---- the anchors (posed bone origins, world fx16) -------------------------
struct FxAnchors {
  int32_t body[3];        // kBRoot origin (the belly light)
  int32_t crown[3];       // the body's top pole
  int32_t junction_f[3];  // the FRONT JUNCTION (pass 4: the old neck bind;
                          // keeps the pass-3 ring centring bone-for-bone)
  int32_t neck[3];        // the neck hinge. PASS 11 (QA §7.9): NOT "mid-tube"
                          // -- Direction 7 §9.1 moved it onto the front
                          // junction and manafold_rig.h has said so since
                          // pass 9.
  int32_t junction_b[3];  // the BACK JUNCTION (kBLoopBase2 posed origin)
  int32_t hinge_a[3];
  int32_t hinge_b[3];
  int32_t hinge_c[3];
  int32_t ring[3];    // the ring-pocket centre. PASS 3 (R8/Direction 3 §6c
                      // "they sit at the edge of the circle"): the pass-2
                      // A/B/C centroid sat ~120 mm from ball B — near the
                      // hole's TOP edge, which is exactly the owner's
                      // complaint. Now the NECK EXIT joins the centroid so
                      // the anchor lands in the hole's middle; it still
                      // moves with hinge play (one performance).
};

// ---- the bolt (FX.LIGHTNING recurrence — kept verbatim) -------------------
constexpr int kBoltSegs = 16;          // the main bolt (hinge B -> crown)
constexpr int kBolt2Segs = 8;          // the branch (hinge A -> hinge C)
constexpr int32_t kBoltJitterMm = 175;
constexpr int kBoltStrikeFrames = 14;  // one strike lives this long
constexpr uint32_t kBoltSeed = 0xC0DA11CEu;
constexpr int32_t kBoltStampMm = 22;   // stamp spacing: under one core
                                       // radius at ~12.3 mm/px — the
                                       // gap-free-at-native law (R9)

// ---- the mana ramps (lo MUST be black: a floor above zero rims every
// blob — the edge-free law, 09-ENGINE-GOTCHAS §11) --------------------------
constexpr uint8_t kManaBlueMid[3] = {40, 85, 215};
constexpr uint8_t kManaBlueHi[3] = {150, 215, 255};
constexpr uint8_t kManaVioletMid[3] = {125, 45, 205};
constexpr uint8_t kManaVioletHi[3] = {225, 175, 255};
constexpr uint8_t kManaGoldMid[3] = {175, 120, 35};
constexpr uint8_t kManaGoldHi[3] = {255, 225, 150};
// PASS 3 (R7, Direction 3 §6d.3 "more aquamarine, more cyan" + 6b "filled
// blues and greens"): the owner's named colour family. The cyan is
// deepened toward teal (the eye recon's pigment finding); aquamarine and
// sea-green join. Chosen by eye at native on the lit shipping path.
constexpr uint8_t kManaCyanMid[3] = {24, 138, 148};
constexpr uint8_t kManaCyanHi[3] = {160, 245, 250};
constexpr uint8_t kManaAquaMid[3] = {28, 190, 172};
constexpr uint8_t kManaAquaHi[3] = {175, 255, 236};
// PASS 8 (pass-7 by-eye fault 2: "the mote cores are still white steam").
// THE MEASURED FACT: restricted to the pixels the effect actually dominates,
// the motes were 48.7% hue-neutral at mean RGB (215,229,236). THE MECHANISM,
// traced through the code rather than guessed: each mote is an OPAQUE heart
// under an ADDITIVE halo. The opaque heart WRITES f.pal[t] -- and at a corona
// sprite's centre t is at the ramp's top, so the heart writes kManaAquaHi
// (175,255,236), which is already only 31% saturated. The halo then adds
// kManaAquaHi * kMoteHaloGainPm/1000 = (59,86,80) ON TOP of it, and
// (234,255,255) is white. The number and the code agree exactly, which is the
// corroboration 09-ENGINE-GOTCHAS §16 asks for before acting on a measurement.
//
// The fix is NOT a gain (the lab is explicit: kMoteHaloGainPm never moves, and
// the ceiling is reached by OVERLAP, not by count). It is that an opaque body
// and an additive fringe want DIFFERENT ends of the ramp, and pass 7 gave them
// the same one. The heart gets its own ramp, deliberately dark and saturated,
// authored so that AFTER the halo's known (59,86,80) add it lands near
// (85,255,236) -- about 67% saturation instead of 8%. The halo keeps
// kManaAquaHi: an additive fringe SHOULD reach for white at its centre, that
// is what makes it read as glow.
// Authored by eye at native, twice. The first values (10,120,100)/(26,186,156)
// were chosen from the arithmetic alone -- dark enough that the halo's known
// add would land on a saturated result -- and rendering them showed hard dark
// green specks under white blobs, which is worse than what it replaced. That is
// the art law: the arithmetic was right about the sum and wrong about the READ.
// These are bright enough to be a glowing body in their own right and still
// 65% saturated, and the halo is now drawn UNDER them.
// Third authoring pass, and the last two both failed for the same reason: the
// ramp's LOWER HALF is what most of an opaque disc is made of. A ramp that runs
// from black gives a disc whose interior is nearly black, so the mote read as a
// dark green pea inside a white glow. This ramp does NOT start at black -- LO
// and MID are the same bright aqua, so the body is flat and bright across its
// whole face and only brightens toward the middle. It is the one place on this
// creature where a ramp SHOULD have a floor: 09-ENGINE-GOTCHAS §11's rule
// ("the property that makes a blob look edgeless is the ramp reaching black at
// its bottom") is about SOFT blobs, and this one is deliberately a body.
constexpr uint8_t kManaAquaCoreMid[3] = {70, 215, 190};
constexpr uint8_t kManaAquaCoreHi[3] = {120, 250, 228};
constexpr uint8_t kManaSeaGreenMid[3] = {26, 158, 92};
constexpr uint8_t kManaSeaGreenHi[3] = {150, 240, 180};
constexpr uint8_t kManaDeepBlueMid[3] = {28, 62, 195};
constexpr uint8_t kManaDeepBlueHi[3] = {130, 175, 255};
constexpr uint8_t kManaWhiteMid[3] = {160, 160, 190};
constexpr uint8_t kManaWhiteHi[3] = {245, 240, 255};
constexpr uint8_t kManaDripMid[3] = {40, 62, 150};   // the opaque deep blue
constexpr uint8_t kManaDripHi[3] = {90, 130, 220};

// ---- OWNER DIRECTION 10 (2026-09-09): THE DEEP DARK BLUE ------------------
// "...connected by white lightning lines surrounded by a deep dark blue. You
// know, like actual lightning."
//
// ⚠ THIS IS A DARK HALO, NOT A GLOW, AND IT IS THE FIRST OF ITS KIND HERE.
// Every mana element on this creature is ADDITIVE, and additive can only
// LIGHTEN -- so every previous lightning attempt was pale-on-pale by
// construction, no matter what the core gain was. What the owner is
// describing is a bright core wrapped in something DARKER THAN THE SKY,
// which is how real lightning reads against a night. The machinery for it
// already exists and had never been pointed at this: glow_splat's
// `opaque + soft` mode blends toward the ramp colour by the sprite's own
// intensity, so a ramp whose entries are darker than the backdrop SUBTRACTS.
//
// LO == MID, the kRampAquaCore idiom: a flat dark body, not a fade from
// black -- a fade from black would go INVISIBLE at the rim, which is exactly
// where a surround has to be present.
//
// The values are a STARTING POINT for the rung plate, not a shipped choice
// (Direction 10 §5: "it is an experiment ... the deliverable is the AXIS").
// They are dark enough to sit under `channel`'s violet night ({14,10,46} deep,
// {70,44,132} mid) and blue enough to read as storm rather than as ink.
// ---- PASS 15, OWNER DIRECTION 11 §4: THE SURROUND WAS BLACK BY ARITHMETIC -
//
// D11 sharpens D10 from "surrounded by a deep dark blue" to "**connected by
// white lines of light with BLUE SHIMMER surrounding them**", and pass 14's own
// by-eye review had already found the shipped halo reads BLACK, not blue.
//
// ⚠ AND THE NUMBERS PREDICTED IT EXACTLY, WHICH IS WHY NO RUNG COULD HAVE
// FIXED IT. {8,12,46} scaled by kFoldStrandDarkGainPm = 300 is (2,4,14).
// Eight counts of blue is not blue at 240p; two is not a colour at all. The
// pass-14 ladder swept RADIUS and GAIN and **never swept HUE**, so every rung
// on it was a different size of black. 10-GATE §0: a gate — or a ladder —
// cannot report a fault nobody thought to bound.
//
// THE FIX IS THE RAMP, NOT THE GAIN, and that is the plan's own instruction.
// kRampStorm is drawn opaque+soft, so it BLENDS TOWARD its ramp colour: the
// ramp entry IS the pixel wherever the sprite dominates. Author the navy here,
// directly, in scene, and leave the gain at unity so the knob nobody can read
// backwards stops existing. These are a deep storm navy — darker than
// `channel`'s violet night ({14,10,46} deep, {70,44,132} mid) in red and green
// and BLUER than it, so it reads as weather rather than as ink, and plainly
// dark against the pale sunset the other 25 clips carry.
constexpr uint8_t kManaStormMid[3] = {12, 20, 64};
constexpr uint8_t kManaStormHi[3] = {26, 44, 112};
// ---- THE SHIMMER: the layer that has never existed -----------------------
// "connected by white lines of light with blue shimmer surrounding them. You
// know, like actual lightning bolts."
//
// Every previous attempt built TWO layers — a white core and a dark surround —
// and the owner has now asked, twice, for a thing that is neither: light that
// is BLUE. A shimmer is not a dark band, it is not a black outline, and it
// cannot be made out of a subtractive element no matter how it is tuned. So it
// is its own layer, ADDITIVE (it is light: 09-ENGINE-GOTCHAS §4), sitting
// between the white core and the navy backing.
//
// The colour sits under the channel ceiling (the S3 colour law) so it can
// stack a little without whitening, and it is BLUE-dominant rather than cyan:
// the creature's own mana family is aqua, and the lightning must not read as
// more of the same fold. Authored by eye on the rung plate, both backdrops.
constexpr uint8_t kManaShimmerMid[3] = {36, 116, 235};
constexpr uint8_t kManaShimmerHi[3] = {150, 205, 255};
// The bolt's own OPAQUE heart, for the lightning MOTES (D11: "we want the
// other particles folded into shapes"). LO == MID, the kRampAquaCore idiom and
// for the pass-8 reason: a ramp that starts at black gives an opaque disc a
// near-black interior, and a lightning mote with a dark middle is a hole.
constexpr uint8_t kManaBoltCoreMid[3] = {200, 228, 255};
constexpr uint8_t kManaBoltCoreHi[3] = {255, 255, 255};
// ---- D11 §4: the green/aqua fold he LIKES, one rebalance rung ------------
// "Right now you're folding the green ones. It actually looks neat. Experiment
// some and make it look better." — so this is an experiment row, NOT a
// replacement, and the shipped aqua stays the default. The known mechanism is
// an aqua ramp whose G sits above its B (G−B = +18 at mid, +19 at hi), which
// the additive pile-up then exaggerates into green. This rung swaps them so
// B ≥ G at both stops and nothing else moves. Off unless ZHAO_U02_AQUA_BAL=1.
constexpr uint8_t kManaAquaBalMid[3] = {28, 172, 196};
constexpr uint8_t kManaAquaBalHi[3] = {175, 236, 255};
enum ManaRamp : uint8_t {
  kRampGlow = 0,   // the shipped centre-glow ramp (kGlowLo/Mid/Hi)
  kRampBlue,
  kRampViolet,
  kRampGold,
  kRampCyan,
  kRampWhite,
  kRampDrip,
  kRampAqua,      // pass 3: the owner's aquamarine lead
  kRampSeaGreen,  // pass 3: the "try greens" ask
  kRampDeepBlue,  // pass 3: the filled deep blue
  kRampAquaCore,  // pass 8: the mote HEART -- opaque, dark, saturated
  kRampStorm,     // D10: the DEEP DARK BLUE surround. Darker than either sky
                  // on purpose -- it is the only ramp here that subtracts.
  kRampShimmer,   // D11: the BLUE SHIMMER. Additive, and the whole point of it
                  // is that it is LIGHT -- the layer two passes did not have.
  kRampBoltCore,  // D11: the lightning MOTE's opaque heart. LO == MID.
  kRampCount
};
/** PASS 8: which ramp an OPAQUE heart should write, given the ramp its
 *  additive halo uses. Identity for every family that has not been given a
 *  core of its own, so adding one later is a one-line change and nothing
 *  else moves. Named as a function rather than inlined at the call site so
 *  the mapping is greppable and so the mote path and any future opaque body
 *  cannot drift apart. */
inline uint8_t mana_core_ramp(uint8_t halo_ramp) {
  if (halo_ramp == kRampAqua) return static_cast<uint8_t>(kRampAquaCore);
  // D11: the lightning mote is the same idiom one family over -- an additive
  // blue shimmer around a hot opaque heart. Registered HERE rather than at the
  // call site for the reason the comment above gives: the mapping stays
  // greppable and the two families cannot drift apart.
  if (halo_ramp == kRampShimmer) return static_cast<uint8_t>(kRampBoltCore);
  return halo_ramp;
}


// ---- the candidate knobs --------------------------------------------------
constexpr int32_t kPulsarCorePx = 13;      // inside the ~10-15 px ring pocket
constexpr int32_t kPulsarHaloMinPx = 60;   // the halo BREATHES — this is the
constexpr int32_t kPulsarHaloMaxPx = 90;   // pulse, 30 px of swing at ~4 Hz
constexpr int kPulsarBreathFrames = 15;    // 60 fps / 15 = 4 Hz
constexpr int kPulsarCoreGainPm = 620;
constexpr int kPulsarHaloGainPm = 310;
constexpr int32_t kPlasmaRPx[3] = {46, 36, 28};
constexpr int kPlasmaGainPm = 320;  // pass 3 retune: under the ceiling
constexpr int32_t kPlasmaOrbitMm = 620;
constexpr int kPlasmaOrbitFrames = 300;
constexpr int kBulletsN = 7;      // pass 3 retune: 10 halos stacked white
constexpr int kBulletLifeFrames = 48;
constexpr int32_t kBulletRPx = 9;
constexpr int kBulletGainPm = 300;  // under the ceiling: the HUE survives
constexpr int kBulletGhosts = 3;           // smear route 2: stamped ghosts
constexpr int kBulletGhostStepFrames = 2;
constexpr int32_t kBulletSpeedMmPerFrame = 42;
constexpr int kBoltCoreGainPm = 1000;
constexpr int32_t kBoltCoreRPx = 3;        // the hot core: 3 px fuses the
                                           // 22 mm stamps into one solid
                                           // filament at native (2 px read
                                           // as dots against the ghost)
constexpr int kBoltHaloGainPm = 260;
// PASS 7 widened this 7 -> 9 to buy properly-saturated cyan area around the
// hot core. PASS 12 TAKES IT BACK to 6, because D9 §3 changed what the pocket
// is for: it now has to hold a READABLE SHAPE, and 9 px of additive halo every
// 22 mm along a jagged diameter is not breadth, it is a slab.
//
// ⚠ THE MEASUREMENT THAT WOULD HAVE LIED, recorded because it nearly did.
// The obvious lever for "too white" is the gain, so a gain ladder was rendered
// first: kBoltCoreGainPm 1000 / 700 / 480 / 300, all four at native in the
// knead window. THE FOUR RUNGS ARE VISUALLY INDISTINGUISHABLE. Cutting the
// per-stamp gain by 3.3x changed almost nothing, because the white is not
// gain-driven -- it is the CHANNEL CEILING. Enough additive stamps overlap
// that the sum clamps whatever each one contributes, and a knob that feeds a
// clamped sum is a knob that does nothing. Only ablating the strand entirely
// cleared it, which is how the cause was pinned.
//
// So the lever is OVERLAP, not gain: fewer stamps in the pocket, and less area
// each. Laddered by eye at native (strands x halo: 2x9 shipped, 1x9, 2x6, 1x6,
// and 0 as the deliberate too-far rung). 1x6 is the pick -- the strand reads as
// a jagged FILAMENT with its beads visible instead of a fused white mass, and
// the folded shape's aqua outline reads underneath it for the first time.
constexpr int32_t kBoltHaloRPx = 6;
constexpr int kStreakGainPm = 420;         // the anamorphic strike flash
constexpr int32_t kStreakSpanPx = 46;
// PASS 3 (R9): lightning is 2-3 CONTINUOUS strands that BUZZ across the
// ring pocket's middle — not periodic strikes that vanish. Each strand is
// a two-layer stamped path (hot near-white core over a calm additive
// halo), re-hashed every kBoltRehashFrames so it visibly buzzes, ghosting
// through the smear plane so old paths decay instead of blinking out.
// Stamp spacing is the checkable gate: zero visible gaps at native.
// PASS 4 (R4, Direction 4 §2: "fewer lightning LINES -- just have them be
// particles"): ONE strand (the owner knob; the channel may carry 2), and
// the retired strands' budget converts into SURGE MOTES that flow along
// the strand's path and burst at its ends -- energised particles with one
// hot filament, not a filament cloud.
// PASS 6 E.3 (Direction 5 0-BIS: "some more lightning particles ... but don't
// go overboard"). 1 -> 2; the header's own note already allowed two. The THIRD
// is rejected on the record rather than untried: each strand pushes ~1,330 px
// of near-white core with depth-test off into a 10-15 px pocket, and the
// whitening element is the strand, not the motes. Three would hue-neutralise
// the pocket -- the exact "goes white and erases its own colour" failure
// 08-LIGHTING documents and the clause the owner attached to the request.
// ---- DIRECTION 9 §3: THE MOTE CLOUD IS A GARNISH, NOT THE SHAPE ---------
// "we no longer have the super awesome shapes of lightning being made, we have
// a spazzy green cloud that sucks donkey balls trying to make shapes. Take that
// out almost entirely."
//
// The owner's two sentences are ONE mechanism, and that is the pass-12 finding:
// the cloud is what buried the lightning. The shape is drawn TWICE over -- once
// as an edge stamped with the bolt primitive, and once as a mote field parked
// on the same stations. The motes win, because there are 38 of them at 7-10 px
// halos inside a ~40 px pocket, and a fused field of soft balls is exactly "a
// spazzy green cloud trying to make shapes".
//
// The mana lab already proved the fix and the owner already approved it by eye:
// `edge-strands` (manafold_lab.h, the row D7 §2/§8 picked -- "Edge drawn, not
// held reads perfectly as shapes") drew the outline with the strand primitive
// and ran TEN motes, "the mote cloud thinned to a garnish instead of being the
// shape". Shipping ran 24 then, and has since grown to 38.
//
// ⚠ This scales `kMoteCount` rather than replacing it. kMoteCount stays the
// owner's knob for how populous the conduit is; this is the separate question
// of how much of that population the FOLD draws while the edge carries the
// shape. Two knobs because they are two questions -- and both are read, so
// neither is a dead knob (manafold_art.h §kFoldEdgeCoreGainPm is what a dead
// one costs).
//
// It lives HERE and not in manafold_art.h only because pass 12 ran two
// implementers over disjoint file sets and art.h belongs to the other lane.
// Said out loud so the next pass can move it home rather than wonder.
constexpr int kFoldMoteGarnishPm = 300;   // 38 -> 11, the lab's approved ten
// ---- D11 §4: "and we want MORE of them. These are the lightning particles."
//
// The garnish above is the number D9 §3 cut the AQUA CLOUD to, and it was the
// right cut: a fused field of soft green balls was burying the shape. D11 asks
// for the opposite of that in a different material -- the motes are to BE the
// lightning, sitting on the figure's stations as its vertices, connected by
// the white line. A hot 3 px bolt mote is not the 10 px soft aqua ball the cut
// was about, so the count that was wrong for one is not wrong for the other.
//
// ⚠ AND THE PASS-5 CEILING IS BOUNDED, NOT IGNORED: more count needs more AREA
// (`manalab-breadth-more` recorded the overlap regression). This buys count
// while the LIGHTNING mote's radii stay the small hot ones, so the pocket
// gains vertices without gaining fill. Laddered anyway -- ZHAO_U02_MOTES.
constexpr int kFoldStrandMoteGarnishPm = 720;  // 38 -> 27 when the bolt folds
// The green/aqua fold he likes keeps its own count untouched: this only
// applies while kFoldStrandOn, which is the whole "keep them SEPARATE" clause.

// ---- PASS 12: THE STAMP SPACING FOLLOWS THE CORE IT DRAWS WITH -----------
// A CONSEQUENCE OF THE OWNER-ORDERED K1 RADII THAT NOBODY COSTED. The edge is
// drawn by stamping discs along the outline, and it is continuous only while
// consecutive stamps overlap -- `kBoltStampMm = 22` is commented, correctly,
// "stamp spacing: under one core", and it was sized against the STRAND's
// kBoltCoreRPx of 3. D9 §10.1 takes the FOLD EDGE's core to 2 without touching
// the spacing, so the same 22 mm now steps further than one core and the
// outline breaks into a dotted line. Rendered and looked at: the aqua outline
// IS present at K1 and it IS beaded, which is why thinning alone did not read
// as a shape.
//
// So the edge gets its own spacing, scaled with its own core, and the strand
// keeps kBoltStampMm -- they are two primitives with two core radii and one
// shared number was always a coincidence rather than a rule.
constexpr int32_t kFoldEdgeStampMm = 14;   // 22 * (2/3), one core again

// ---- OWNER DIRECTION 10: THE EDGE BECOMES THE LIGHTNING -------------------
//
// THE PICTURE THAT SETTLED IT. `channel` f001-f009 at 3x, baseline build
// 12ca4ca3 (pass14-plates/r6-channel-base-3x.png): the figure's outline IS
// being drawn -- a faint dark-teal DOTTED arc is plainly visible through the
// pocket -- and it is drawn at kFoldEdgeCoreRPx = 2 px while the motes that
// carry no shape at all are drawn at kMoteHaloRPxMin..Max = 7..10 px.
//
// **The connection is drawn at a fifth of the size of the things it connects.**
// That is the whole of "no shapes, can't see them", it is four passes old, and
// no gain ladder could ever have found it: the edge was never dim, it was
// SMALL, and it was small in the one dimension nobody had a knob pointed at.
//
// Direction 10 and the pass-14 architect's `edge-strands` finding are the same
// sentence from two directions: the motes were never the picture, they are its
// VERTICES -- and the shape motes already sit exactly on the stencil stations
// the outline runs between (`stn = m * kStencilPts / n_shape`, and `place()` is
// shared by both), so "connect the particles" needs no new placement at all.
// It needs the line to become the loud thing.
//
// THE THREE PARTS, EACH ITS OWN KNOB, because Direction 10 is an EXPERIMENT
// and the deliverable is the axis rather than a point on it:
//
//   1. CONNECTED -- kFoldStrandPerSeg sets how many stamps subdivide each
//      or the line is beads again (pass 12 learned this the hard way one
//      constant above).
//   2. WHITE -- kRampWhite, additive, depth test OFF (the pulsar-core law:
//      energy reads over flesh).
//      ⚠ THIS DELIBERATELY REVERSES A STANDING LAW. manafold_art.h records the
//      lab's finding that a white outline "put 366 near-white px on screen and
//      dropped saturation to 108.9" and concludes "a white outline reads as a
//      glitch; an aqua one reads as folded mana". That measurement was sound
//      and its conclusion is now OVERRULED BY THE OWNER IN HIS OWN WORDS --
//      "connected by white lightning lines". Recorded rather than quietly
//      flipped, because the next pass will find that comment and wonder.
//      The reason the measurement was not wrong AND the owner is right: the
//      lab's white outline had no dark surround, so it whitened a pale field.
//      Part 3 is what makes part 2 survivable.
//   3. SURROUNDED BY A DEEP DARK BLUE -- kRampStorm, opaque+soft, DEPTH TESTED
//      (a surround is grounded; it is not energy). Drawn FIRST so the white
//      core lands on top of it and the dark reads as a wrapping, not a veil.
//
// ⚠ AND THE TRAP DIRECTION 10 NAMES ITSELF: a deep dark blue may vanish on
// the violet night and dominate the sunset the other 25 clips use. So every
// value below is swept from ONE binary through the env overrides in
// zhao_reel.cpp, on BOTH backdrops, and the plate is the deliverable.
// ⚠ DEFAULT OFF, AND THAT IS DELIBERATE. Direction 10 SS5 calls this an
// EXPERIMENT and says the deliverable is "the AXIS, not the best point on it"
// -- so the bank keeps shipping pass 13 until the owner picks a rung off
// pass14-plates-reel/. Verified, not assumed: with this false, `channel`
// renders sequence_crc32c 0xD554CC12, the same value the pre-change binary
// produced. Flip to true (or ZHAO_U02_STRAND=1) for the D10 read.
// ⚠ PASS 15 FLIPS THIS TO TRUE, AND THAT IS AN ORDER, NOT AN EXPERIMENT.
// Direction 11 §4: "for STANDARD, we want the other particles folded into
// shapes... THEY NEED TO MAKE THE SHAPES." D10 called the look an experiment
// and the correct answer then was a ladder; D11 names the STANDARD. The
// architect flipped the flag on `channel` f363 under the shipping env and it
// draws an unmistakable connected spiral where the shipped frame shows a mote
// swarm (pass15-arch-plates/D-strand-vs-shipped-3x.png) -- so the mechanism
// landed in pass 14 and only the SWITCH never did. 10-GATE item 39: a flag
// that exists and is off is not a fix, and this one had been off for a pass.
// What stays an EXPERIMENT is the COLOUR -- the shimmer, the navy, the mote
// count -- which is what the rung plates are for.
// ⚠ This changes `channel`, whose lightning is on the protected list. It is
// changed DELIBERATELY, by name, on the owner's own instruction, and the
// identity gate is re-run so the change is declared rather than discovered.
constexpr bool kFoldStrandOn = true;         // D11: the lightning IS the figure
constexpr int32_t kFoldStrandCoreRPx = 3;    // the white hot core
constexpr int kFoldStrandCoreGainPm = 1000;
// PASS 15: 12 -> 9. D11's composite is THREE layers, and the navy is now the
// BACKING for a blue shimmer rather than the only thing wrapping the white. A
// 12 px opaque-soft disc every few millimetres along an 18-station figure was
// filling the pocket with bruise; the shimmer needs room between the core and
// the backing's rim, which is where it lives.
constexpr int32_t kFoldStrandDarkRPx = 9;    // the navy BACKING, now thin
// ⚠ THIS KNOB IS NO LONGER BACKWARDS, AND THE PARAGRAPH THAT SAID IT WAS IS
// DELETED ON PURPOSE. It used to read "LOWER IS DARKER", which was true only
// because the ramp was near-black and the gain was scaling it toward nothing.
// Pass 15 authored the navy INTO kManaStormMid/Hi where it belongs (see the
// ramp block: the plan's own instruction, "the ramp entries are the knob, not
// the gain") and put this back to unity. So the ramp is the colour, this is a
// plain scale on it, and higher is lighter like everywhere else in this file.
// A knob nobody can read the direction of is a knob the owner does not have.
constexpr int kFoldStrandDarkGainPm = 1000;
// ---- D11: THE BLUE SHIMMER LAYER ---------------------------------------
// Between the white core (3 px) and the navy backing (9 px), additive, depth
// test OFF like the core -- it is light coming off the bolt, and light reads
// over flesh (the pulsar-core law). Radius is deliberately close to the
// core's: a shimmer HUGS its bolt. Make it wide and it becomes a blue cloud,
// which is the pass-12 "spazzy cloud" failure wearing a new colour.
constexpr int32_t kFoldShimmerRPx = 6;
constexpr int kFoldShimmerGainPm = 620;
// ...and the thing that makes it SHIMMER rather than sit: a per-stamp,
// per-frame flicker of ±this many per-mille on the gain. Off the frame clock
// directly, not off kBoltRehashFrames -- the path RE-HASHES every 7 frames
// (that is the strike), and a shimmer is the faster thing living on top of it.
// ⚠ 09-ENGINE-GOTCHAS §9: "clearly visible" in a comment is not evidence, and
// the crayon grain was clipped to a range narrower than the rig's own and went
// invisible. 380 pm is ±38% of the gain, which is wider than anything else
// modulating these pixels; if the plate says otherwise the plate wins.
constexpr int kFoldShimmerFlickerPm = 380;
// ⚠ THE CONNECTEDNESS KNOB IS A COUNT, NOT A DISTANCE, AND THAT IS THE WHOLE
// FINDING. The first strand build set a spacing in MILLIMETRES, like every
// other stamped primitive here, and it did nothing whatsoever: rungs at 9 mm
// and at 3 mm rendered BYTE-IDENTICAL (md5 91a66310, sequence_crc32c
// 0xB60B0DFA both times), and so did raising the per-segment cap from 40 to
// 300. Three knobs, three no-ops, and the plate LOOKED slightly different to
// me at 4x -- which is checklist item 41 earning its place: the crop found a
// candidate and only the md5 could confirm it.
//
// The cause: the figure is kStencilPts = 18 stations around a pocket a couple
// of hundred millimetres across, each link cut into kFoldEdgeSegs = 4, so a
// sub-segment is about THREE MILLIMETRES long. `nst = manhattan / stamp_mm`
// then floors to 0 for every stamp value above ~3 and is rescued to 1 by the
// `nst < 1` guard -- so the strand drew exactly ONE stamp per sub-segment,
// 18 * 4 = 72 beads, no matter what the constant said. An integer millimetre
// cannot subdivide a three-millimetre segment, so the unit itself was the bug.
//
// Expressed as a COUNT it is monotonic, resolution-independent, and it is
// precisely the owner's axis: how connected. Spacing follows the figure
// instead of fighting it.
constexpr int kFoldStrandPerSeg = 6;         // stamps per sub-segment: the LINE
// THE PICTURE THAT FORCED THE NEXT TWO CONSTANTS (pass14-plates/d10-*.png,
// binary f11a8268): with the three above at 3 px / 9 px / 9 mm, `channel` f363
// draws the figure as a NECKLACE OF ONE-PIXEL WHITE DOTS. Measured through a
// dot: the core is exactly one pixel of (255,255,255) -- saturated, so gain is
// not the lever AGAIN -- and the dark surround's mean drop is 30 lum out of
// ~700, i.e. 4%. That is the crayon grain verbatim: mathematically present and
// visually invisible.
// AND THE SPACING WAS THE PART WITH NO KNOB. I gave myself overrides for the
// core radius, the halo radius and the halo gain -- three axes that were
// roughly fine -- and none for the one the owner named FIRST ("the particles
// are actually CONNECTED"). That is the gain-ladder error committed a second
// time: sweeping the axis I had a knob on instead of the axis that carries
// the fault.
// TWO things set the spacing and only one was a constant. `nst` is the stamp
// count along a sub-segment; it is computed from a MANHATTAN length (adx+ady+adz,
// up to sqrt(3) longer than the true one) divided by the stamp, and then
// CLAMPED. Whichever of the two binds is the one choosing the spacing, and
// nobody has ever established which. So both are knobs now and the ladder
// decides it by looking. (It did, and the answer was neither: see
// kFoldStrandPerSeg below -- the UNIT was wrong, so both knobs were no-ops.)
constexpr int kFoldStrandCapN = 64;          // stamps per sub-segment ceiling
// The free cross-pocket strand (`mana_lightning`) inside the fold window. It
// traces nothing -- 09-ENGINE-GOTCHAS §18's "an independent, full-brightness
// white strand composited over the same pocket with no knowledge of the
// stencil" -- and once the EDGE is a white strand it is the same primitive
// drawing a figure instead of a diameter. Off by default under D10; one flip
// restores it, and the ablation is one env away.
// ⚠ TRUE, AND IT WAS false FOR A WHILE, AND A BASELINE BUILD CAUGHT IT.
// Turning the free strand off UNCONDITIONALLY silently changed `channel` even
// with the D10 strand off: my tree rendered sequence_crc32c 0x39676A53 where
// origin/main renders 0x241D7382. `channel`'s lightning is on the PROTECTED
// list, and I had been about to write "reverts to pass 13 byte for byte" in a
// comment on the strength of reading the code. Reading said inert; a baseline
// binary said otherwise. BUILD THE BASELINE -- it cost one build and it was
// the only thing in this pass that could have caught it.
// The free strand is redundant only WHEN THE EDGE IS ITSELF A STRAND -- then
// it is the same primitive drawing a diameter across a figure. So it now
// FOLLOWS the strand instead of being switched independently, and
// ZHAO_U02_FREE_STRAND still forces either answer outright.
constexpr bool kFoldFreeStrandOn = true;

// PASS 12 (D9 §3, "bring back the super awesome shapes of lightning"): 2 -> 1.
// This RESTORES the pass-4 value, which Direction 4 §2 asked for in as many
// words -- "fewer lightning LINES -- just have them be particles" -- and which
// pass 6 raised to 2 under D5 0-BIS's "some more lightning particles ... but
// don't go overboard". D9 supersedes D5, and the pass-6 note above got the
// mechanism exactly right while stopping one strand short of acting on it:
// "each strand pushes ~1,330 px of near-white core with depth-test off into a
// 10-15 px pocket, and the whitening element is the strand, not the motes."
// Two strands do to the pocket what it correctly predicted three would.
//
// The lightning is NOT reduced by this -- it is relocated. The SHAPE is itself
// drawn with the bolt primitive (the fold edge, `bolt_path` + kFoldEdgeStampMm
// below), so "shapes of lightning" is what the edge makes; this free strand is
// the crackle across the pocket, and one of it is a filament while two is a
// slab that buries the shape the owner is asking to see.
constexpr int kStrandCount = 1;
constexpr int kSurgeMotes = 5;             // flowing along the strand
constexpr int kSurgeFlowFrames = 26;       // one end-to-end trip
constexpr int32_t kSurgeRPx = 7;
constexpr int kSurgeGainPm = 420;
constexpr int32_t kSurgeBurstRPx = 11;     // the endpoint bursts
constexpr int kSurgeBurstGainPm = 520;
constexpr int32_t kStrandJitterMm = 60;    // jag SMALL against the ~50 mm
                                           // segment — a filament, not a
                                           // scribble (the dot-cloud fix)
constexpr int kBoltRehashFrames = 7;       // the buzz cadence
constexpr int32_t kStrandSpanMm = 820;     // endpoint spread across the pocket
constexpr int32_t kStrandEndJitMm = 240;   // endpoint scatter per re-hash
constexpr int32_t kBoilCorePx = 30;
constexpr int32_t kBoilOuterPx = 48;
constexpr int kBoilCoreGainPm = 520;
constexpr int kBoilOuterGainPm = 380;
constexpr int kBoilRotDiv = 3;             // CLUT rotation: churn, zero pixels
// PASS 3 (R13 #5, Direction 3 §6e): the boil-CENTRE variant — the middle
// grown ~1.6x, the outer ring removed.
constexpr int32_t kBoilCentrePx = 48;
// PASS 3 (R8, Direction 3 §6c): EVERYTHING anchors in the middle of the
// ring and stays there — position law = ring centre + a bounded integer
// Lissajous wobble well inside the pocket. Bullets stop being ballistic:
// they orbit/jiggle with kBulletSpreadMm bounding their excursion.
constexpr int32_t kCentreWobbleMm = 130;   // the shared centre wobble bound
constexpr int32_t kBulletSpreadMm = 360;   // "a little spray is fine" — small
constexpr int32_t kBulletCoreRPx = 7;      // the opaque FILLED heart (R7)
constexpr int32_t kPlasmaSpreadMm = 240;   // the filled blobs' wobble bound
// PASS 3 (R7): "filled" means an OPAQUE, saturated, non-additive core
// under the additive halo — additive alone can never read solid over the
// bright peach sky. Core radius as per-mille of its halo radius.
constexpr int kCoreOfHaloPm = 640;

// ===================== THE SHELL (DIRECTION 9 §7 + §14) ===================
//
// "We need the outer part of the creature to be translucent like in the very
//  first version. Go look it up. Just less translucent. Only a bit see
//  through, not almost completely invisible."                        (D9 §7)
//
// "you should make the fog go outside the lines of the creature a bit, makes
//  more sense to me. Should get denser the closer it gets to the inside."
//                                                                    (D9 §14)
//
// ---- WHY THIS IS NEW CODE AND NOT A NEW VALUE ---------------------------
// THE SHELL WAS NEVER A SHELL. `kFogThicknessPm` -- the constant four passes
// have argued over, set to 1000, 2000, 4500 and 1200, and which D9 §7 calls
// "the THIRD setting of this value" -- is a global multiplier on the SMEAR
// PLANE's composite gain. It is one number applied to a screen-space
// persistence buffer. It has no idea where the creature is.
//
// That is why the value kept being wrong in a way nobody could fix by moving
// it: §14 asks for a density that varies with distance from the silhouette and
// that extends past the ink, and a scalar on an unrelated plane is
// STRUCTURALLY INCAPABLE of either. The owner has now described the mechanism
// precisely enough that the mismatch is unmissable, so the shell gets built as
// the thing it is: a band around the creature's own silhouette.
//
// (kFogThicknessPm survives untouched, because it does do something real to the
// smear. Its own comment now says what it actually is. It is no longer the
// shell, and no future pass should re-answer a shell question by moving it.)
//
// ---- THE MECHANISM ------------------------------------------------------
// Pass 10 already built the exact input this needs and used it for one job:
// `u02_cover`, the creature's per-pixel coverage INCLUDING the cel ink ring,
// which the mist reads to exclude the animal. The shell reads the same mask
// and measures DISTANCE FROM ITS EDGE, in both directions:
//
//      outside  <-- kShellOutReachPx --|-- kShellInReachPx -->  inside
//                                 the ink line
//
// Density is HIGHEST AT THE LINE and falls off both ways. That satisfies both
// of the owner's sentences at once: it "goes outside the lines a bit", and out
// there it "gets denser the closer it gets to the inside". Inward it fades too,
// over kShellInReachPx, which is D5 §3's original picture -- "the gassy outside
// inside the black line before you get to the real body" -- so the body's
// middle stays clean and only its rim is hazed.
//
// A LIMB-BRIGHTENED shell (densest at the rim, thinning toward the body's
// centre) is what a real translucent sphere does, and it is the OPPOSITE of one
// possible reading of §14. It is not the opposite of what the owner asked: he
// wrote that sentence about the part that goes OUTSIDE, where "closer to the
// inside" means "closer to the animal". Both readings agree on everything
// outside the line and differ only on whether the haze keeps thickening deeper
// into the body -- which would fog the creature rather than wrap it. Written
// down because the next pass will read §14 again.
//
// ---- WHAT MUST SURVIVE --------------------------------------------------
// §14.1's acceptance test is THE INK. Pass 10 spent a whole item restoring the
// outline after the mist erased it, so the shell paints over ink pixels at
// kShellOverInkPm of its density -- a knob, authored by eye, whose only job is
// that the line still reads plainly with gas on both sides of it.
//
// §14.3: THIS IS NOT THE MIST. The mist's silhouette exclusion is untouched by
// everything here and stays at 0 creature pixels. The shell crossing the line
// does not license the mist to.
// ---- PASS 15: THE BAND BECOMES AN ANNULUS -------------------------------
//
// OWNER DIRECTION 11 s2.3: "Once the outer body part is made of a thick fog
// (that gets less thick the nearer to the outside it goes), it no longer looks
// like clipping, just going into fog."
//
// > "You need to get to the bottom of why this didn't work before."
//
// THE BOTTOM, AND IT IS A MECHANISM, NOT A VALUE. Everything above this line
// describes a band 3 px OUTSIDE and 6 px INSIDE the silhouette, densest AT the
// ink. The pass-15 architect rendered its whole ladder under the shipping env
// (pass15-arch-plates/B-shell-ladder-3x.png, `hover` f72/f300 at alpha 0 / 440
// / 900) and the result is unambiguous:
//
//     440 (shipped)  ~7,000 px changed, and it reads as EDGE ANTI-ALIASING
//     900 (too far)  it reads as a white outer GLOW, and it greys the ink
//
// Neither is fog, and no rung between them can be, because the eye/body
// intersection the owner is complaining about sits TENS OF PIXELS interior to
// the silhouette -- where a 6 px inward band is IDENTICALLY ZERO. Four passes
// argued about a value on a mechanism that could not express the request:
// 09-ENGINE-GOTCHAS s18 exactly, one level up from the kFogThicknessPm case
// this file already documents. THE THIRD PASS ON AN ITEM ATTACKS THE
// MECHANISM.
//
// ---- WHAT THE OWNER ACTUALLY DESCRIBED ----------------------------------
// Not a rim. A BODY WHOSE OUTER LAYER IS GAS: a volume occupying a sizeable
// fraction of the ball's own radius, denser the deeper in you go, so that
// anything entering it is PROGRESSIVELY ABSORBED rather than cut. So the
// reaches stop being pixels and become FRACTIONS OF THE BODY'S OWN APPARENT
// RADIUS -- which is also the only way one number can be right on a close-up
// and on a wide shot (09-ENGINE-GOTCHAS s1: this projection is anisotropic and
// world size is not screen size, so a screen effect must be sized on screen).
//
// ---- AND THE PROFILE IS NOT A GUESS: THREE THINGS AGREE ON IT -----------
// Take the ball as a screen disc of radius R whose outer shell, from R down to
// R_core, is fog. The fog you SEE at a pixel is the column integral along the
// ray. Work it out and the column:
//
//   * is ZERO at the silhouette and rises steeply inward,
//   * PEAKS where the ray goes tangent to the clear core -- which is at screen
//     depth (R - R_core), i.e. exactly the INNER EDGE of the annulus,
//   * then drops to roughly half the peak and HOLDS FLAT across the whole
//     interior, because there you are looking through the front layer only.
//
// That is the optics. It is also the owner's sentence, verbatim -- thick fog
// getting "less thick the nearer to the outside it goes". And it is the
// profile that makes the eye REMEDY work: as the bounce pushes the lens
// deeper, the depth under it rises, the fog over it thickens, and the lens
// FADES instead of showing a cut. Three independent readings, one curve; that
// is the corroboration s16 asks for before acting on anything.
//
// THE OUTWARD PART IS THE SAME CURVE, NOT A SECOND FEATURE. D9 s14 asked for
// the fog to go "outside the lines of the creature a bit". So the fog's own
// outer edge is kShellOutReachPm past the cover mask, and the profile's zero
// sits THERE -- one continuous rise from the fog's edge, through the ink line,
// on into the body. The previous build had a maximum at the ink and a fade in
// both directions, which is why "outside" and "denser inward" fought each
// other and neither read.
//
// EVERY NUMBER BELOW IS A NAMED OWNER KNOB WITH AN ENV OVERRIDE (the ladder
// comes from ONE BINARY, 10-GATE item 26), and the ladder MUST carry a
// deliberately-too-far rung where the animal drowns (item 4).

// The annulus depth, as per-mille of the creature's own apparent radius R --
// R being the deepest interior point of the cover mask this frame, so the fog
// breathes with the bounce instead of being a fixed pixel count over a body
// that changes size.
constexpr int kShellFogDepthPm = 380;
// A floor, so a small or distant subject still gets a band rather than a
// rounding error. NOT a substitute for the fraction: the fraction is the thing.
constexpr int32_t kShellFogDepthMinPx = 5;
// How far past the cover mask the gas reaches, per-mille of R (D9 s14's "a
// bit"), with its own floor. This is where the profile's ZERO now lives.
constexpr int kShellOutReachPm = 55;
constexpr int32_t kShellOutReachMinPx = 2;
// What the fog does DEEPER than the annulus: it falls from the peak to this
// fraction of it, over one more annulus depth, and then holds. That plateau is
// the front layer you look through over the clear core -- and it is the knob
// that decides whether the body wears gas or the body is WASHED OUT. D8 s4
// ("we thickened too much... I want to revert that") is what lives at the top
// of this knob's range.
constexpr int kShellCoreFloorPm = 340;
// The shape of the inward rise. 1000 linear, 2000 fully quadratic (holds the
// gas close to the annulus's inner edge and lets the outer skirt stay thin).
constexpr int kShellRiseGamma = 1600;
// LEGACY, KEPT AS A DEAD REFERENCE AND NOTHING ELSE. These were the pass-12
// band's reaches; they are what the paragraph at the top of this block is
// about. Nothing reads them. They stay named so a future pass grepping for
// "why was the shell 3 and 6" finds this comment rather than a git blame.
constexpr int32_t kShellOutReachPx_legacy_p12 = 3;
constexpr int32_t kShellInReachPx_legacy_p12 = 6;
// Authored by eye off the ladder in pass12-plates/B4-shell-*.png -- 0 / 200 /
// 340 / 440 / 520 / 900, ONE binary via U02_SHELL_ALPHA, with 900 as the
// deliberately-too-far rung (10-GATE-CHECKLIST item 4: ask where the ceiling is
// BEFORE shipping an increase). At 900 the gas reads as a white glow and the
// ink greys out, which is the failure s14.1 names. 440 is where the gas is
// plainly present on both sides of the line, the outward gradient is visible,
// and the ink is still solid black at 6x.
// PASS 15: THAT LADDER WAS RUN ON THE 3/6 PX BAND AND ITS PICK DOES NOT
// TRANSFER. 440 was "the most you can push a fringe before it glows"; this
// knob is now the PEAK OF A VOLUME and it is re-laddered from scratch.
constexpr int kShellAlphaMaxPm = 440;     // the peak, at the annulus inner edge
// v1's shell read as a faint whitish-pink haze over the pink body -- looked at
// in archive-2026-09-04-u02-hover.webm f60 and -channel.webm f180, as s7
// ordered ("go look it up" is an instruction to look). This is that colour at
// the presence s7 asks for. The VALUE is chosen by eye in scene and never
// sampled off the archive: a pale rose that reads pink on one ground reads grey
// on another (CLAUDE.md, the dorsal-pink lesson).
constexpr uint8_t kShellTint[3] = {255, 214, 232};
constexpr int kShellOverInkPm = 260;      // the ink must survive being crossed
// The live values the compositor reads; the constants above are the shipping
// defaults and stay the named owner knobs. U02_SHELL_* overrides them for the
// by-eye ladder ONLY, so the ladder comes from ONE BINARY (10-GATE-CHECKLIST
// item 21: a comparison split across two builds measures the builds). Never a
// shipping setting.
inline int g_u02_shell_alpha_pm = kShellAlphaMaxPm;
inline int g_u02_shell_depth_pm = kShellFogDepthPm;
inline int g_u02_shell_out_pm = kShellOutReachPm;
inline int g_u02_shell_floor_pm = kShellCoreFloorPm;
inline int g_u02_shell_gamma = kShellRiseGamma;

/** The falloff profile. `t_pm` is 1000 at the profile's peak and 0 at the gas's
 *  outer edge. Linear at gamma 1000, quadratic at 2000, blended between so the
 *  knob is continuous. */
inline int shell_profile_pm(int t_pm, int gamma_pm) {
  if (t_pm <= 0) return 0;
  if (t_pm > 1000) t_pm = 1000;
  int k = gamma_pm - 1000;
  if (k < 0) k = 0;
  if (k > 1000) k = 1000;
  const int quad = t_pm * t_pm / 1000;
  return (t_pm * (1000 - k) + quad * k) / 1000;
}

/** Paint the creature's outer layer as a GAS ANNULUS scaled to its own radius.
 *
 *  `cover` is pass 10's per-pixel creature coverage (body + cel ink); `ink` is
 *  the ink ring alone, or nullptr. Both come from the renderer's OWN masks --
 *  the shell never re-derives where the creature is, because a second
 *  derivation is how two things that must agree stop agreeing.
 *
 *  Runs AFTER the cel ink pass and BEFORE the mana, so the mana glows over the
 *  shell exactly as it glows over the creature.
 *
 *  Three steps, and the middle one is the pass-15 change:
 *    1. an UNCAPPED inward distance transform of the cover mask -- how deep
 *       into the animal each pixel is, in pixels;
 *    2. R = the deepest of those, i.e. the body's own apparent radius THIS
 *       FRAME, which is what every reach is then a fraction of;
 *    3. one continuous profile from the gas's outer edge inward: rise to a
 *       peak at the annulus's inner boundary, decay to a plateau, hold.
 *
 *  Cost is one BFS pass over the frame with each pixel enqueued once -- about
 *  740k integer ops at 384x240, which is nothing next to the composite that
 *  follows. (Arithmetic, not measurement: 09-ENGINE-GOTCHAS s5.)
 */
inline void shell_paint(uint8_t* rgb, uint32_t w, uint32_t h,
                        const uint8_t* cover, const uint8_t* ink,
                        int alpha_max_pm) {
  if (cover == nullptr || alpha_max_pm <= 0) return;
  const size_t n = static_cast<size_t>(w) * h;
  const int iw = static_cast<int>(w), ih = static_cast<int>(h);

  // ---- 1. the inward distance transform, UNCAPPED -----------------------
  // depth[i] = 0 outside the cover, 1 on the first interior ring, and so on.
  // The IMAGE BORDER counts as outside: a creature cropped by the frame edge
  // is not thereby infinitely deep, and without this a close-up would inflate
  // R and fog the whole animal. (Named, because it is a decision.)
  //
  // ⚠ THE BUFFERS ARE REUSED ACROSS FRAMES ON PURPOSE, AND IT IS NOT A
  // MICRO-OPTIMISATION. The first version of this allocated and zeroed four
  // frame-sized vectors per frame and materialised every EXTERIOR pixel as a
  // BFS seed -- about 85,000 of them at 384x240, each fanning out to eight
  // neighbours. Measured on the ladder, it cost roughly 1.8x the reel's whole
  // render throughput (about 200 frames/min down to about 112), which across a
  // 28-clip bank is half an hour added to every publish wave. Cost is
  // arithmetic here, not measurement (09-ENGINE-GOTCHAS §5) -- but a throughput
  // change that large is visible on a wall clock and was.
  //
  // The exterior never needed to be enumerated: level 1 is exactly "a cover
  // pixel touching a non-cover pixel, or the frame edge", which is ONE scan.
  // And `depth` carries the skirt too, as NEGATIVE distances, so the second
  // and third frame-sized buffers stop existing.
  static std::vector<int32_t> depth;
  depth.assign(n, 0);
  std::vector<size_t> cur;
  cur.reserve(n / 16);
  for (int y = 0; y < ih; ++y) {
    for (int x = 0; x < iw; ++x) {
      const size_t i = static_cast<size_t>(y) * w + static_cast<uint32_t>(x);
      if (!cover[i]) continue;
      bool edge = (x == 0 || y == 0 || x == iw - 1 || y == ih - 1);
      if (!edge) {
        for (int dy = -1; dy <= 1 && !edge; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const size_t j = static_cast<size_t>(y + dy) * w +
                             static_cast<uint32_t>(x + dx);
            if (!cover[j]) { edge = true; break; }
          }
      }
      if (edge) { depth[i] = 1; cur.push_back(i); }
    }
  }
  int32_t r_px = cur.empty() ? 0 : 1;
  std::vector<size_t> next;
  for (int32_t d = 2; !cur.empty(); ++d) {
    next.clear();
    next.reserve(cur.size());
    for (const size_t i : cur) {
      const int x = static_cast<int>(i % w), y = static_cast<int>(i / w);
      for (int dy = -1; dy <= 1; ++dy) {
        const int ny = y + dy;
        if (ny < 0 || ny >= ih) continue;
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          const int nx = x + dx;
          if (nx < 0 || nx >= iw) continue;
          const size_t j =
              static_cast<size_t>(ny) * w + static_cast<uint32_t>(nx);
          if (!cover[j] || depth[j] != 0) continue;
          depth[j] = d;
          next.push_back(j);
        }
      }
    }
    if (!next.empty()) r_px = d;
    cur.swap(next);
  }
  if (r_px <= 0) return;   // nothing of the creature on screen this frame

  // ---- 2. the reaches, as fractions of THIS FRAME'S radius --------------
  int32_t out_px = static_cast<int32_t>(
      static_cast<int64_t>(r_px) * g_u02_shell_out_pm / 1000);
  if (out_px < kShellOutReachMinPx) out_px = kShellOutReachMinPx;
  int32_t ann_px = static_cast<int32_t>(
      static_cast<int64_t>(r_px) * g_u02_shell_depth_pm / 1000);
  if (ann_px < kShellFogDepthMinPx) ann_px = kShellFogDepthMinPx;
  // Where the profile peaks, measured from the gas's own outer edge.
  const int32_t peak_at = out_px + ann_px;
  int floor_pm = g_u02_shell_floor_pm;
  if (floor_pm < 0) floor_pm = 0;
  if (floor_pm > 1000) floor_pm = 1000;

  // ---- 3. one profile, outer edge inward --------------------------------
  // alpha_at(dist), dist measured from the gas's outer edge:
  //   dist <= peak_at        rise, gamma-shaped, 0 -> alpha_max
  //   peak_at .. +ann_px     decay, alpha_max -> alpha_max * floor
  //   deeper                 hold at the plateau
  const auto alpha_at = [&](int32_t dist) -> int {
    if (dist <= 0) return 0;
    if (dist <= peak_at) {
      const int t = static_cast<int>(
          static_cast<int64_t>(dist) * 1000 / (peak_at > 0 ? peak_at : 1));
      return alpha_max_pm * shell_profile_pm(t, g_u02_shell_gamma) / 1000;
    }
    int32_t over = dist - peak_at;
    if (over > ann_px) over = ann_px;
    const int fall = static_cast<int>(
        static_cast<int64_t>(over) * 1000 / (ann_px > 0 ? ann_px : 1));
    const int scale = 1000 - fall * (1000 - floor_pm) / 1000;
    return alpha_max_pm * scale / 1000;
  };

  // The exterior skirt: grow off the cover, out_px rings. Ring r is r steps
  // out, so it sits at dist = out_px - r + 1 from the gas's own outer edge --
  // the ring just outside the ink is the DENSEST of the skirt and the last one
  // is the thinnest, which is the "denser the closer to the inside" half of
  // D9 s14 read outside the line.
  //
  // It rides in `depth` as NEGATIVE distances -- a skirt pixel is never a cover
  // pixel, so the two readings cannot collide, and the frame-sized `seen` and
  // `outring` buffers the first version used stop existing.
  cur.clear();
  for (int32_t r = 1; r <= out_px; ++r) {
    next.clear();
    // Ring 1 grows off the cover itself; later rings grow off the ring before.
    if (r == 1) {
      for (size_t i = 0; i < n; ++i)
        if (cover[i]) cur.push_back(i);
    }
    if (cur.empty()) break;
    const int32_t mark = -(out_px - r + 1);   // dist from the gas's outer edge
    for (const size_t i : cur) {
      const int x = static_cast<int>(i % w), y = static_cast<int>(i / w);
      for (int dy = -1; dy <= 1; ++dy) {
        const int ny = y + dy;
        if (ny < 0 || ny >= ih) continue;
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          const int nx = x + dx;
          if (nx < 0 || nx >= iw) continue;
          const size_t j =
              static_cast<size_t>(ny) * w + static_cast<uint32_t>(nx);
          if (cover[j] || depth[j] != 0) continue;
          depth[j] = mark;
          next.push_back(j);
        }
      }
    }
    cur.swap(next);
  }

  // ---- composite ---------------------------------------------------------
  for (size_t i = 0; i < n; ++i) {
    const int32_t d = depth[i];
    if (d == 0) continue;
    const int32_t dist = cover[i] ? out_px + d : -d;
    if (dist <= 0) continue;
    int a = alpha_at(dist);
    if (ink != nullptr && ink[i]) a = a * kShellOverInkPm / 1000;
    if (a <= 0) continue;
    if (a > 1000) a = 1000;
    uint8_t* px = rgb + i * 3;
    for (int k = 0; k < 3; ++k)
      px[k] = static_cast<uint8_t>(
          (px[k] * (1000 - a) + kShellTint[k] * a) / 1000);
  }
}

// ============================ THE SMEAR PLANE ==============================
// PASS 3 (R6, Direction 3 §6d as amended): the smear is a DECAYING,
// GLITCHY persistence — "never clears is too much, but longer than usual
// in games. A bit glitchy." A persistent 96x60 RGB accumulation plane fed
// by everything the mana draws, decayed per frame, composited additively
// at chunky 4x nearest — the reel-side emulation of the glow_persist
// hardware ask. A smooth exponential fade is the NAMED FAILURE (an
// ordinary motion trail); the glitch is the point:
//   kSmearKeepPm         per-frame retention (the decay length)
//   kSmearStepFrames     decay lands in DISCRETE steps every N frames —
//                        the trail visibly stutters down (this IS the glitch)
//   kSmearJitterPm       per-cell retention jitter (uneven decay)
//   kSmearHardClearFrames a bounded, per-cell-staggered interval after
//                        which a cell fully clears (the "it does decay"
//                        guarantee — the buffer provably resets)
// Presets ship as owner-pickable variants (short/clean vs long/glitchy,
// with the lead's mid rung between them).
struct SmearPreset {
  int keep_pm;            // retention applied at each decay step
  int step_frames;        // frames between decay steps
  int jitter_pm;          // per-cell retention jitter (+/-)
  int hard_clear_frames;  // full per-cell reset interval
  int gain_pm;            // composite gain onto the frame
  int tear;               // pass 4 (R6): the row-tear glitch rides this rung
};
// what fraction of each splat's ramp feeds the plane per frame: with keep
// near 1 the plane integrates many frames, so a full-strength feed
// saturates to white in a dozen frames (measured on the first render).
constexpr int kSmearFeedPm = 520;
// THE COMPOSITE IS A BLEND, NOT AN ADD. Additive over the bright peach sky
// can only whiten (the first two renders proved it at effect scale) — but
// the owner's image is the solitaire win: a broken framebuffer leaves OLD
// PIXELS DRAWN, solid, over whatever is behind. So a cell paints its own
// hue with an opacity that follows its remaining brightness — fresh cells
// are near-solid saturated blobs, decayed cells thin out and dissolve.
constexpr int kSmearAlphaMaxPm = 780;
// PASS 8 (pass-7 by-eye fault 3: "the smear takes its colour from what is
// behind it" -- glorious aqua on the night clips, flat khaki-grey blocks on the
// daylight ones, one plane producing the pass's best AND worst pixels purely as
// a function of the sky). The mechanism is the composite below: it LERPS the
// cell over the frame at alpha `a`, so at any alpha under about half the result
// is mostly background, and a saturated aqua lerped 25% over a peach sky is a
// slightly-cool peach -- which is exactly what "dirt or compression blocks"
// describes.
//
// The fix is a CHROMA FLOOR, not more alpha. More alpha would widen the read of
// an already-too-wide cloud (fault 7, the traverse framing) and the lab's
// stopping rule is that the mana must not start deleting the creature. Instead
// the cell's own colour DIFFERENCE FROM ITS GREY is added after the blend: for
// an aqua cell that lifts G and B and PULLS R DOWN, which no lerp can do, so
// the block tilts teal against a warm sky at unchanged brightness and unchanged
// coverage. It needs no extra decay handling: a decaying cell's stored colour
// shrinks toward zero, so its chroma shrinks with it by construction.
//
// 0 restores pass 7 exactly, which is the point of it being a knob.
constexpr int kSmearChromaFloorPm = 600;
constexpr int kSmearVividPm = 1500;
// ---- DIRECTION 7 §4: THE SMEAR SCALES WITH SPEED -------------------------
// "we said no frame buffer glitch smear when standing still. I'd like to
// qualify that. We want some smear, just not as much. And the fall definitely
// needs the full smear treatment. The smear in hasty looks great for when
// there's movement. Probably have a base smear and increase smear with movement
// speed."
//
// The owner gave the mechanism himself, so this is it: ONE continuous knob
// driven by the creature's own posed root speed, replacing the per-clip
// intensity judgement. That also retires a whole bug class -- a per-clip preset
// table with an off-by-one bound has now bitten three passes.
//
// The preset table SURVIVES, because a rung is an IDENTITY (decay length, step
// quantisation, jitter, tear) and not an intensity. Speed multiplies the
// composite's opacity; the rung still says what kind of smear it is.
//
// ⚠ The plane is SCREEN-SPACE, so a trail only SEPARATES when the creature
// travels across frame. Speed-driven intensity lines up with that naturally,
// but a clip that is fast without traversing gets intensity and no trail. That
// is why `fall` -- which moves vertically -- is checked by eye, not assumed.
//
// Measured from the posed root anchor (fa.body), which carries the clip's own
// root displacement, NOT from the instance placement, which does not move.
constexpr int kSmearSpeedBasePm = 380;   // standing still: never zero
// The speed at which the multiplier reaches full. `hasty` travels 8330 mm over
// 120 keys = 69 mm/key = ~34 mm/frame at the shipped two-frames-per-key, and
// the owner names hasty as the reference for the moving end -- so hasty is
// exactly full and everything else is read against it. `fall` drops 3600 mm
// over 130 keys with a t^2 curve, so its peak is ~2*3600/260 = 28 mm/frame:
// most of the way to full, at the moment of the catch, which is where the
// treatment should peak.
constexpr int kSmearSpeedFullMmPerFrame = 34;
// PASS 8: the composite's "keep the cell vivid" step was `c[k] * 3 / 2` with a
// per-channel clamp at 255, and 09-ENGINE-GOTCHAS §9 is exactly about comments
// like that one. A cell at (142,208,192) becomes (213,255,255) -- the two high
// channels clip, the low one does not, and the hue the feed went to such trouble
// to preserve is destroyed AT THE LAST STEP. The scale is now reduced when it
// would clip, so the ratio survives at any brightness -- the same hue-preserving
// law smear_feed already uses one function up. 1500 is the old 3/2.
// PASS 7 (Direction 5 §3): "the gassy outside inside the black line before
// you get to the real body is actually a cool idea, but it wasn't in the
// specs. Instead of removing it, let's thicken that fog by a lot, so it's
// still see through, but way less so. It should be very visible."
//
// THIS IS ITS OWN KNOB, deliberately separate from each SmearPreset's own
// gain_pm above. gain_pm carries a rung's IDENTITY (short/clean vs
// long/glitchy vs broken-buffer) and is tuned against that rung's own
// decay/tear timing; folding "how thick does the fog read" into the same
// number is exactly the kBellyGlowGainPm mistake on record (one knob doing
// two jobs, so fixing one silently breaks the other). kFogThicknessPm scales
// EVERY rung's composite gain by the same factor at the one call site
// (zhao_reel.cpp's smear_composite call) and nothing else touches it: no
// rung's own gain_pm, decay, jitter, hard-clear or tear timing changes, so
// every rung keeps the identity it was tuned for and simply reads solidly
// thicker. 1000 = untouched (pass 6 behaviour).
//
// ⚠ FALSE-COMMENT CORRECTION (pass 11, P.3). This block used to end "...2000
// was the first value where the gassy shell read as deliberately thick fog"
// while the constant shipped 4500. That is the 10-GATE-CHECKLIST's own cited
// example of a rationale outliving its number, and it stood through two more
// passes. The history, in full, because both halves of it are the owner's:
//
//   pass 6  1000  the v1 read -- "you could barely see there was some red mist
//                 around the creature"
//   pass 7  2000  Direction 5 §3: "thicken that fog by a lot ... way less
//                 see-through, very visible"
//   pass ?  4500  the same instruction pushed further
//   pass 11 1200  DIRECTION 8 §4 REVERSES IT: "we thickened too much and now
//                 the outer layer is completely opaque. I want to revert that."
//
// Direction 8 §4 contains its own correction and it is why the revert does not
// land back on 1000: "It was too much, you could barely see there was some red
// mist around the creature. But the concept was cool. I wanted that." He is
// describing v1 as too faint AND saying it was the good version. So the target
// is v1's read with a hair more presence -- 1200, authored by eye at native
// against the shipped 4500 and against a bare leg, never fitted to a number.
// The acceptance sentence is his: present, felt, not read as a surface.
// ⚠ PASS 12 NAME CORRECTION -- read this before moving this number again.
// THIS IS NOT THE SHELL. Everything above is the history of four passes
// arguing about "the outer layer" through a constant that is a global
// multiplier on the SMEAR PLANE's composite gain, at one call site, on a
// screen-space persistence buffer that has no idea where the creature is.
// D9 §14 asks the shell to be denser near the body and to reach past the ink,
// and this scalar is structurally incapable of either -- which is why the value
// kept being wrong in a way nobody could fix by moving it.
//
// The shell is now its own thing: kShellAlphaMaxPm / kShellOutReachPx /
// kShellInReachPx / kShellFalloffGamma and u02::shell_paint, above. This
// constant KEEPS ITS JOB and its value, because thickening the smear is a real
// effect the owner has asked for and liked; it simply is not the answer to a
// shell question, and no future pass should re-answer one by moving it.
constexpr int kFogThicknessPm = 1200;
// PASS 11 (Direction 8 §4): THE SHELL GOES BACK TO A WHISPER, and the ladder
// that chose it comes from ONE BINARY. g_u02_fog_thickness_pm is the live
// value the compositor reads; kFogThicknessPm above is its shipping default
// and remains the named, editable owner knob. U02_FOG_THICKNESS=<pm> overrides
// it for the by-eye ladder only -- it is a measurement lever, never a shipping
// setting, exactly like U02_MIST_NO_EXCLUDE (10-GATE-CHECKLIST item 21: a
// comparison split across two builds measures the builds).
inline int g_u02_fog_thickness_pm = kFogThicknessPm;
// PASS 4 (R6, Direction 4 §2 "glitchier than the others you made"): a
// FOURTH live rung past the long/glitchy one -- keep higher, steps longer
// and chunkier, jitter wider -- and the ROW TEAR: on hashed frames a
// horizontal band of the plane composites with a 1-2 cell x-offset, the
// VHS tear of a genuinely broken buffer. Tear rides the two glitchier
// rungs only (tear=1); an index offset at composite, near-free.
struct SmearTear { int rows, frames, cells; };
constexpr SmearTear kSmearTear = {5, 46, 2};  // kSmearTearRows/Frames/Cells
// PASS 6 E.5: a SIXTH rung, "SHORT/TORN". Two facts drove it, both measured:
//   * The plateau. taunt2 opens at 126 white px and climbs to a 700-1100 px
//     haze that NEVER comes back down, which is why the best frames of every
//     clip are its first ten (protected item 11) -- the persistence plane
//     saturates and buries the read it started with.
//   * The tear IS the glitch. hasty's approved "broken frame buffer" look is
//     rung 3 PLUS the row tear; channel has never had the tear at all.
// So this rung is rung 1's keep/step/jitter/clear/gain -- the accumulation
// behaviour channel already ships and the owner already likes -- with tear=1
// added. Stationary clips get the owner's "some of that glitchy smear" WITHOUT
// the accumulation that plateaus. Travelling clips keep rung 3, which is the
// approved reference look and is not touched.
constexpr SmearPreset kSmearPresets[] = {
    {0, 1, 0, 1, 0, 0},          // 0: no smear
    {620, 2, 40, 90, 420, 0},    // 1: SHORT/CLEAN — a readable tail, tidy
    {820, 4, 90, 260, 520, 0},   // 2: MID/GLITCHY — the pass-3 lead rung
    {900, 6, 160, 430, 520, 1},  // 3: LONG/GLITCHIER — the travelling rung
    {940, 8, 260, 560, 540, 1},  // 4: BROKEN-BUFFER — the far end (cyan)
    {620, 2, 40, 90, 420, 1},    // 5: SHORT/TORN — rung 1 + the row tear
};
// PASS 7: named, not a bare "3" at the one call site that must skip the fog
// knob (kFogThicknessPm) -- the travelling clips' approved reference trail.
constexpr int kU02SmearTravellingRung = 3;
// PASS 7: the bound is DERIVED FROM THE ARRAY, never written by hand. Pass 5
// shipped `< 14` on a 15-entry table; pass 6 fixed that one and then shipped
// `< 5` on this six-entry one, which silently routed rung 5 -- and therefore
// TWELVE of fifteen clips, `channel` included -- to index 0, "no smear".
// Anything that indexes this table clamps against kSmearPresetCount.
constexpr int kSmearPresetCount =
    static_cast<int>(sizeof(kSmearPresets) / sizeof(kSmearPresets[0]));
static_assert(kSmearPresetCount == 6,
              "kSmearPresets grew or shrank: check every clip's rung assignment");
constexpr int kSmearW = 96, kSmearH = 60;  // quarter-res: the fill budget
// PASS 4 (R5, Direction 4 §2 "the smear needs to be properly hidden
// whenever the creature is in front of it"): the plane carries ONE DEPTH
// VALUE PER CELL — the nearest (largest 1/w) contributing splat depth,
// recorded at feed, zeroed by the hard clear, untouched by decay. The
// composite then applies exactly glow_splat's own test at cell
// granularity: a pixel whose surface is nearer than the cell's remembered
// depth keeps the surface. The 4-px blocky occlusion edge this produces
// is PART of the broken-framebuffer aesthetic — accepted, stated, judged
// by eye. Storage: 96x60 int32 (+11.5 KB-equivalent in the reel).

// =============================== THE MIST PLANE =============================
// DIRECTION 7 §8: "besides the particles there should be some of that mist that
// leaves the wrong frame buffer ghost image. That's missing. That doesn't even
// need shape, we have enough shape from the particles. But bring it back."
// ...and §8's second message: "and it needs to move with the creature. Leaving
// stuff hanging in space just looks like a glitch."
//
// THIS IS A SECOND PLANE, NOT A CHANGE TO THE SMEAR. That is deliberate and it
// is the whole risk control of this pass. Pass 8 fixed a real fault in the
// smear's colour (motes 48.7% -> 22.8% hue-neutral, smear 0.0% neutral at
// saturation 168) and the owner has praised hasty's rung-3 trail by name. Both
// survive here BY CONSTRUCTION: not one smear constant, buffer or code path is
// touched. The mist is additional, exactly as the direction words it --
// "BESIDES the particles" -- and it can be switched off with one flag without
// disturbing anything the last pass earned.
//
// WHAT MAKES IT A MIST RATHER THAN A TRAIL, and why each knob exists:
//   * NO SHAPE OF ITS OWN. kMistFeedOfHaloPm is ~4.5x the smear's fed radius,
//     so a mote's contribution lands as a broad soft wash instead of the
//     smear's tight core-sized dot. The direction is explicit that the mist
//     "doesn't even need shape" because the particles already carry it.
//   * CHUNKIER BLOCKS. 48x30 composited at 8x nearest, against the smear's
//     96x60 at 4x. The broken-frame-buffer read comes from big, obviously
//     wrong blocks; at the smear's resolution a soft wash just looks blurry.
//   * LONGER PERSISTENCE. Keep/step near the "BROKEN-BUFFER" rung, because a
//     ghost is by definition an image that outstayed its frame.
//   * SEE-THROUGH. kMistAlphaMaxPm sits well under the smear's 780: Direction
//     5 §3's fog is "still see through, way less so, very visible".
//
// ---- AND THE PART THAT IS ACTUALLY NEW: IT IS CREATURE-RELATIVE -----------
// Direction 6 §0-TER listed three options for the stationary-trail problem and
// option 3 -- "make the smear plane creature-relative so a ghost trails from
// the creature's own motion rather than screen motion" -- was declared OUT OF
// SCOPE as new machinery. The owner has now asked for it directly, so it is in
// scope and this is it.
//
// The mechanism is one idea: EACH FRAME THE PLANE IS SHIFTED BY THE CREATURE'S
// OWN SCREEN-SPACE DISPLACEMENT, scaled by kMistFollowPm.
//
//   kMistFollowPm = 1000  the plane rides exactly with the creature. No streak
//                         at all -- a halo pinned to the animal.
//   kMistFollowPm =    0  the plane is pinned to the SCREEN. This is the old
//                         behaviour, and it is the thing the owner rejected:
//                         "leaving stuff hanging in space just looks like a
//                         glitch."
//   in between            the plane follows, but LAGS. The lag is the streak.
//
// That middle case is the whole point, and it is the direction's own
// distinction made mechanical: "anchor the mist to the creature and let its lag
// produce the streak, so the trail always points back to its source." A ghost
// that comes OFF the animal is a smear; the same pixels standing still while
// the animal departs are debris. Because the shift is driven by the creature's
// posed screen position, the residue can never be left behind in the world --
// it is structurally incapable of the fault, rather than tuned away from it.
//
// The shift is integer cells with a FIXED-POINT RESIDUAL carried across frames
// (kMistShiftFxBits), so a slow drift of a third of a cell per frame still
// moves the plane every third frame instead of rounding to zero forever. That
// rounding-to-zero is exactly how a "creature-relative" plane silently becomes
// a screen-space one again on the gentle clips, which are the clips this
// feature exists for.
constexpr int kMistW = 48, kMistH = 30;  // 8x blocks: chunkier than the smear
constexpr int kMistBlock = 8;            // composite magnification
static_assert(kMistW * kMistBlock == 384 && kMistH * kMistBlock == 240,
              "the mist plane must tile the 384x240 frame exactly");
// How much of the creature's screen motion the plane follows. See above: this
// is the one knob that decides "attached smear" vs "debris left in space".
// Authored by eye: 820 keeps the haze plainly on the animal while the 18% it
// gives up per frame accumulates into a visible tail behind a travelling one.
constexpr int kMistFollowPm = 820;
constexpr int kMistShiftFxBits = 8;  // sub-cell residual precision
// PASS 10, STAGE A. The mist composites AROUND the creature, never over it --
// the pass's spine. See mist_composite for the mechanism and the 134-degree
// measurement it removes. Default TRUE; false reproduces pass 9 exactly and is
// kept as the A/B leg and as the proved-failable input for the colour gate.
// U02_MIST_NO_EXCLUDE=1 flips it at runtime, so the comparison comes from ONE
// binary (10-GATE-CHECKLIST item 21).
constexpr bool kMistExcludeSilhouette = true;
// persistence: PASS 11 (Direction 8 §1) cut 930 -> 420, and this is the knob
// that did the work. Walking alpha_max_pm down from `rich`'s 380 to 60 thinned
// the cloud and LEFT ITS FOOTPRINT UNCHANGED; walking feed_of_halo_pm 1300 ->
// 450 barely moved the footprint either. Both were plated and looked at. The
// reason is that the plane integrates for hundreds of frames and saturates
// against cell_cap_pm, so at steady state the per-frame feed decides almost
// nothing and how long a cell REMEMBERS decides almost everything.
//
// That is what "the effect covers the screen" was: not opacity, EXTENT. And it
// is why the fix is the persistence rather than the density -- hasty's praised
// "chunky glitchy haze" is a TRAIL, and a trail is a short memory. 420 keeps
// the chunky broken-buffer blocks (they are still plainly blocks; the read the
// owner likes survives) while the haze hugs the mana instead of spilling into
// clean sky. Authored by eye at native on rest f120 and f300, against `rich`
// and against a bare leg -- no number chose it.
constexpr int kMistKeepPm = 420;
constexpr int kMistStepFrames = 5;
constexpr int kMistJitterPm = 190;
constexpr int kMistHardClearFrames = 520;
// the feed. BREADTH is the point -- this is what makes it a mist and not a
// second trail. The smear feeds at kSmearFeedOfHaloPm = 420 (deliberately
// SHRUNK to the core's footprint, pass 5); the mist feeds WIDER than the halo.
constexpr int kMistFeedOfHaloPm = 1300;  // PASS 11: the `smidgen` rung
constexpr int kMistFeedPm = 220;   // per-frame contribution (it integrates)
constexpr int kMistCellCapPm = 110;  // hue-preserving cap, under the smear's 208
// the composite
constexpr int kMistGainPm = 1000;
// ⚠ FALSE-COMMENT CORRECTION (pass 11, P.3). This block said "300 is the `mid`
// row" while the constant shipped 380 -- pass 10 promoted `rich` and did not
// touch the prose. QA caught it; it is item 12 on QA §7's list and the one that
// pass 10 CREATED. Corrected here, in the same commit that moves the number,
// which is the only arrangement that does not rot again.
//
// PASS 11 (Direction 8 §1): the shipping defaults are now the `smidgen` row.
// The owner looked at `rich` on the published page and said "the green cloud is
// totally out of control. I wanted to have a tiny smidgen of it, now the effect
// covers the screen." Two honest gates had confirmed `rich` by measurement --
// the by-eye reviewer called it "the right call" and QA found the whitening in
// band. Every number was inside every band and the read was wrong: the bands
// described the pixels, not the READ. So `rich` is a CEILING PROVED TOO HIGH,
// and nothing here is defended with a number.
//
// Chosen by eye at native off the ladder plates in pass11-plates/, against
// hasty's praised "chunky glitchy haze" -- which is a TRAIL, not a FIELD.
constexpr int kMistAlphaMaxPm = 180;  // a smidgen, not a shell
constexpr int kMistVividPm = 1500;    // same hue-preserving vivify as the smear
constexpr int kMistChromaFloorPm = 600;  // pass 8's fix, so the mist is mana
                                         // -coloured against ANY sky, not just
                                         // the night ones
// the tear, on the mist's own coarser grid. "The tear IS the glitch" (pass 6
// E.5) and the wrong-frame-buffer read is what the direction asked for by name.
constexpr SmearTear kMistTear = {4, 38, 2};
// ---- THE MIST'S TUNABLES, AS A LIVE STRUCT --------------------------------
// Every one of these is an owner knob (CLAUDE.md: "never remove the owner's
// control in the name of fidelity"), and the constants above are its SHIPPING
// DEFAULTS -- they remain the named, editable values. The struct exists so the
// experiment lane can vary the mist without a second copy of the code, which is
// how the smear ended up with a preset table and how the fold ended up with a
// variant table. Nothing outside the mist reads it.
struct MistCfg {
  int alpha_max_pm = kMistAlphaMaxPm;
  int feed_pm = kMistFeedPm;
  int feed_of_halo_pm = kMistFeedOfHaloPm;
  int cell_cap_pm = kMistCellCapPm;
  int follow_pm = kMistFollowPm;
  int gain_pm = kMistGainPm;
  int vivid_pm = kMistVividPm;
  int chroma_floor_pm = kMistChromaFloorPm;
  // STAGE A. Last, and with a default, so every existing aggregate initialiser
  // in kMistVariants[] keeps its exact meaning and every variant inherits the
  // exclusion (the fault it fixes is independent of density -- `sparing`
  // rotated the band's hue as badly as `mid`).
  bool exclude_silhouette = kMistExcludeSilhouette;
  // PASS 11 M.2. THE PERSISTENCE, promoted to a knob because it turned out to
  // be THE knob. Direction 8 §1 asks for "a tiny smidgen" measured against
  // hasty's praised "chunky glitchy haze" -- which is a TRAIL, not a FIELD.
  // Walking alpha down from rich to 60 thinned the cloud and left its FOOTPRINT
  // untouched, and walking feed_of_halo_pm 1300 -> 450 changed the footprint
  // almost not at all: the plane integrates for hundreds of frames and
  // saturates against cell_cap_pm, so at steady state the per-frame feed hardly
  // decides anything. What decides the footprint is how long a cell REMEMBERS.
  // Same position as the other fields, last so every existing aggregate
  // initialiser in kMistVariants[] keeps its exact meaning.
  int keep_pm = kMistKeepPm;
};
inline MistCfg g_u02_mist;
// STAGE A's A/B lever. It lives OUTSIDE MistCfg on purpose: the reel assigns
// g_u02_mist wholesale from kMistVariants[] per subject and resets it between
// subjects, so a field inside the struct would be silently overwritten every
// time a variant leg ran -- which is exactly the leg the A/B needs it for.
inline bool g_u02_mist_force_no_exclude = false;
/** The effective STAGE A predicate. One place, so the compositor, the reel's
 *  missing-mask warning and any gate all ask the same question. */
inline bool mist_excludes_silhouette() {
  return g_u02_mist.exclude_silhouette && !g_u02_mist_force_no_exclude;
}
// ---- THE MIST VARIANT TABLE (Direction 7 §8, §10.2, §10.3) ----------------
// The owner asked for the mist back (§8), then said the new one is "a bit too
// sparing" (§10.2), then licensed an experiment: "for some experiments let's
// not care if mist goes white. Might look cool and intended" (§10.3). All three
// are answered by rendering variants and letting him pick, which is the format
// that produced the edge-drawn shapes.
//
// ⚠ THE WHITE VARIANT IS AN EXPERIMENT, NOT A DEFAULT. §10.3 is a licence for
// ONE element in experiments and explicitly does not generalise: the channel
// ceiling is still a real fault everywhere else, and pass 8's saturation gains
// on the MOTES are protected and untouched by every row here.
//
// `parked` is the CONTROL, and it is the reason this table earns its keep. It
// sets follow to zero, which is the OLD screen-space behaviour -- the thing the
// owner rejected as "leaving stuff hanging in space". Rendering it beside the
// others proves the follow does something, on screen, rather than asserting it
// in a comment (09-ENGINE-GOTCHAS §9: a property nobody has measured on screen
// is a property nobody has seen).
struct MistVariant { const char* name; MistCfg cfg; const char* note; };
// ⚠ PASS 11: every PRE-PASS-11 row now carries its pass-10 persistence (930)
// EXPLICITLY, because pass 11 changed the DEFAULT to 420. Without that, adding
// a field would have silently redefined what `rich` means -- the comparison row
// on the owner's own sheet would have stopped reproducing the thing he
// rejected, and the sheet would have been quietly lying about its control.
// A variant row states its own identity; it does not inherit one that moves.
inline const MistVariant kMistVariants[] = {
    // PASS 11 (Direction 8 §1): THE ROWS BELOW `sparing`. The owner looked at
    // the shipped `rich` page and said "the green cloud is totally out of
    // control. I wanted to have a tiny smidgen of it, now the effect covers
    // the screen." Two honest gates had confirmed `rich` by measurement --
    // every number inside every band -- so the bands described the pixels and
    // not the READ. `rich` is therefore a CEILING THAT HAS BEEN PROVED TOO
    // HIGH, never a starting point to trim, and nothing below is defended with
    // a number. The reference the pick is authored against is the one thing he
    // has consistently praised: hasty's "chunky glitchy haze", which is a
    // TRAIL, not a FIELD.
    //
    // `trace` is the thin bracket of the ladder, kept on the sheet so the owner
    // picks a rung rather than accepting one (owner question 1c, "nearly none").
    {"trace", {120, 180, 1300, 90, kMistFollowPm, 1000, kMistVividPm,
               kMistChromaFloorPm, kMistExcludeSilhouette, 330},
     "the thin bracket: tighter and fainter still -- owner question 1(c)"},
    {"smidgen", {180, 220, 1300, 110, kMistFollowPm, 1000, kMistVividPm,
                 kMistChromaFloorPm, kMistExcludeSilhouette, 420},
     "PASS 11 SHIPPING DEFAULT: a tiny smidgen -- a trail, not a field"},
    {"sparing", {200, 210, 1500, 110, kMistFollowPm, 1000, kMistVividPm,
                 kMistChromaFloorPm, kMistExcludeSilhouette, 930},
     "thin veil -- the antenna reads through it everywhere"},
    {"mid", {300, 260, 1700, 135, kMistFollowPm, 1000, kMistVividPm,
             kMistChromaFloorPm, kMistExcludeSilhouette, 930},
     "the candidate default: visible haze, loop still legible"},
    // PASS 10 A.2: the rung between mid and thick. It exists because STAGE A
    // moved the wall: the mana-lab stopping rule was "too far is when the mana
    // starts eating the animal", and with the silhouette excluded the mist
    // CANNOT eat the animal any more -- so the honest answer to the owner's
    // standing "give the mana some more of our mist" is a re-judgement, not the
    // old ceiling. Judged by eye on BOTH sky types and on rest AND hover, which
    // is the pass-9 mistake (one clip, one sheet) being repaired rather than
    // repeated.
    {"rich", {380, 280, 1800, 152, kMistFollowPm, 1000, kMistVividPm,
              kMistChromaFloorPm, kMistExcludeSilhouette, 930},
     "PASS 10: one rung up from mid, the density the exclusion paid for"},
    {"thick", {460, 300, 1900, 170, kMistFollowPm, 1000, kMistVividPm,
               kMistChromaFloorPm, kMistExcludeSilhouette, 930},
     "the first authored value -- ate the antenna BEFORE STAGE A; re-judge it"},
    {"white", {520, 420, 1900, 255, kMistFollowPm, 1100, 2600, 0,
               kMistExcludeSilhouette, 930},
     "DELIBERATELY BLOWN (§10.3): no cell cap, no chroma floor, hard vivify"},
    {"parked", {300, 260, 1700, 135, 0, 1000, kMistVividPm,
                kMistChromaFloorPm, kMistExcludeSilhouette, 930},
     "CONTROL: follow=0, the rejected screen-space behaviour"},
};
inline constexpr int kMistVariantCount =
    static_cast<int>(sizeof(kMistVariants) / sizeof(kMistVariants[0]));
// ---- DIRECTION 9 §3: THE MIST IS A TRAIL, AND A TRAIL NEEDS MOTION -------
// "Take that out almost entirely, just leave a tiny bit of green mist to leave
// a bit of a trail."
//
// This SUPERSEDES D7 §8, which is where the 750 floor came from: §8 asked for
// the mist AT REST, so the floor was set at three quarters and the gate did
// almost nothing -- 750..1000 is not a speed gate, it is a rounding error with
// a speed term attached. A stationary clip got 75% of a travelling clip's mist
// and the plane then integrated it for hundreds of frames into a field.
//
// The floor is now a WHISPER, and the gate does the work the owner's sentence
// describes: standing still leaves nearly nothing, moving lays a trail, and
// `hasty`/`fall` -- the travelling clips whose trails the owner has praised
// twice -- sit at the TOP of the gate and are therefore the best case rather
// than an exemption that has to be carved out. That is the whole reason to cut
// the mist with speed rather than with a flat reduction: the flat cut would
// have taken the praised trails down with the rejected field.
// ⚠ THE GATE COMPOUNDS, and it is said out loud rather than left to be
// rediscovered: mist_speed_mul_pm is now applied TWICE -- once on the feed
// (zhao_reel.cpp, new this pass) and once on the composite gain (D7 §4, already
// there). So a standing clip lands near 0.18 x 0.18, not 0.18. That is the
// authored result -- rendered on `rest` f120/f300 and on `hasty` mid-traverse
// and chosen by looking, exactly as the ladder plates were -- but the NUMBER in
// this constant is not the fraction you see on screen, and anyone reading it as
// one will move it the wrong distance.
constexpr int kMistSpeedBasePm = 180;
inline int mist_speed_mul_pm(int32_t speed_mm) {
  if (speed_mm <= 0) return kMistSpeedBasePm;
  int32_t t = speed_mm * 1000 / kSmearSpeedFullMmPerFrame;
  if (t > 1000) t = 1000;
  return kMistSpeedBasePm + (1000 - kMistSpeedBasePm) * t / 1000;
}

/** One mana splat, in world space; the reel projects and composes it.
 *  pre=true draws BEFORE the creature compose (a pool the creature and its
 *  arms occlude — the caged-pulsar read); post splats depth-test against
 *  their own projected depth. */
struct ManaSplat {
  int32_t x, y, z;      // world fx16
  int32_t r_px;
  uint8_t ramp;
  int16_t gain_pm;
  bool depth_test;
  bool opaque;          // the drip: writes colour instead of adding
  bool pre;
  // PASS 8: an opaque body with a SOFT edge -- blend toward the ramp colour by
  // the sprite's own intensity instead of replacing outright. `opaque` alone
  // cuts hard at t < 20, which at mote scale is a 5 px turquoise SQUARE; and a
  // purely additive body over this creature's pink sky can only whiten
  // (09-ENGINE-GOTCHAS §4). Soft is the third option and the one a glowing mana
  // core actually wants: saturated where it dominates, feathered at the rim,
  // and incapable of stacking to white because it is a blend, not an add.
  bool soft;
};

inline void mana_push(std::vector<ManaSplat>& out, int32_t x, int32_t y, int32_t z,
                      int32_t r_px, uint8_t ramp, int gain_pm, bool depth_test,
                      bool pre, bool opaque = false, bool soft = false) {
  if (gain_pm <= 0 || r_px <= 0) return;
  out.push_back(ManaSplat{x, y, z, r_px, ramp, static_cast<int16_t>(gain_pm),
                          depth_test, opaque, pre, soft});
}

inline int32_t fx_sin16(uint32_t ph) {
  return zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
}
inline int32_t fx_cos16(uint32_t ph) {
  return zref::fx_cos(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
}
inline int32_t lerp32(int32_t a, int32_t b, int32_t num, int32_t den) {
  return a + static_cast<int32_t>((static_cast<int64_t>(b - a) * num) / den);
}

/** The FX.LIGHTNING path evaluator (the ADDLIGHTNING recurrence, verbatim):
 *  deterministic jagged polyline start->end for one strike phase. Fills
 *  `pts` with segs+1 world positions. This authoring migrates unchanged
 *  onto the FORGE.PRIM ribbon evaluator when the block is built. */
inline void bolt_path(const int32_t s[3], const int32_t e[3], int segs, uint32_t phase,
                      uint32_t seed, int32_t pts[][3],
                      int32_t jitter_mm = kBoltJitterMm) {
  // PASS 3: the jitter is a PARAMETER — ±175 mm per vertex on a strand
  // whose segments are ~50 mm folded the path into a scribble that read
  // as a dot cloud, not a line (looked at, twice). Lightning jag must be
  // small against the segment length to read as a filament.
  for (int i = 0; i <= segs; ++i) {
    for (int k = 0; k < 3; ++k) pts[i][k] = lerp32(s[k], e[k], i, segs);
    if (i == 0 || i == segs) continue;  // the anchors stay anchored
    const uint32_t h1 = fx_hash(seed, phase, static_cast<uint32_t>(i));
    const uint32_t h2 = fx_hash(seed * seed | 1u, phase, static_cast<uint32_t>(i));
    pts[i][0] += fxu(fx_jit(h1, jitter_mm));
    pts[i][1] += fxu(fx_jit(h1 >> 11, jitter_mm / 2));
    pts[i][2] += fxu(fx_jit(h2, jitter_mm * 2 / 3));
  }
}

/** Stamp one bolt path as a CONTINUOUS two-layer chain of splats: a hot
 *  narrow core over a calm wide halo, every ~kBoltStampMm along each
 *  segment (beads at the vertices alone leave visible gaps — the shipped
 *  crackle read as disconnected triangles). */
inline void bolt_stamp(std::vector<ManaSplat>& out, const int32_t pts[][3], int segs,
                       int gain_core_pm, int gain_halo_pm) {
  for (int i = 0; i < segs; ++i) {
    // segment length in mm (fx16 -> mm)
    int64_t dx = (pts[i + 1][0] - pts[i][0]) >> 16;
    int64_t dy = (pts[i + 1][1] - pts[i][1]) >> 16;
    int64_t dz = (pts[i + 1][2] - pts[i][2]) >> 16;
    int64_t len = dx * dx + dy * dy + dz * dz;
    // integer sqrt via float-free approx: step count from the dominant axis
    int64_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, adz = dz < 0 ? -dz : dz;
    int64_t approx = adx + ady + adz;  // upper bound on length
    (void)len;
    int n = static_cast<int>(approx / kBoltStampMm);
    if (n < 1) n = 1;
    if (n > 24) n = 24;
    for (int t = 0; t < n; ++t) {
      const int32_t x = lerp32(pts[i][0], pts[i + 1][0], t, n);
      const int32_t y = lerp32(pts[i][1], pts[i + 1][1], t, n);
      const int32_t z = lerp32(pts[i][2], pts[i + 1][2], t, n);
      mana_push(out, x, y, z, kBoltHaloRPx, kRampCyan, gain_halo_pm, true, false);
      // the hot core SHINES THROUGH the blade (the pulsar-core law): with
      // the depth test on, every place a strand wove behind the neck tube
      // chopped the filament into beads — looked at on the channel's
      // hottest frames. Energy reads over flesh; the halo stays grounded.
      mana_push(out, x, y, z, kBoltCoreRPx, kRampWhite, gain_core_pm, false, false);
    }
  }
}

/** The bounded centre wobble (R8): a small integer Lissajous well inside
 *  the ring pocket — three incommensurate integer periods so nothing
 *  metronomes, salted per element so a crowd never moves in lockstep. */
inline void centre_wobble(uint32_t frame, uint32_t salt, int32_t bound_mm,
                          int32_t o[3]) {
  const uint32_t h = fx_hash(0xC3A7u, salt, 17u);
  const uint32_t p1 = h & 0xFFFFu, p2 = (h >> 16) & 0xFFFFu;
  const int32_t b = fxu(bound_mm);
  o[0] = static_cast<int32_t>((static_cast<int64_t>(b) *
                               fx_cos16(frame * 65536u / 97u + p1)) >> 16);
  o[1] = static_cast<int32_t>((static_cast<int64_t>(b / 2) *
                               fx_sin16(frame * 65536u / 61u + p2)) >> 16);
  o[2] = static_cast<int32_t>((static_cast<int64_t>(b) *
                               fx_sin16(frame * 65536u / 113u + p1)) >> 16);
}

/** One FILLED mana body (R7): a saturated OPAQUE non-additive core (the
 *  drip's draw mode, depth-tested) under the additive halo that keeps the
 *  glow. The core is what "filled" means over the bright peach sky. */
inline void mana_filled(std::vector<ManaSplat>& out, int32_t x, int32_t y, int32_t z,
                        int32_t halo_r_px, uint8_t ramp, int halo_gain_pm) {
  mana_push(out, x, y, z, halo_r_px * kCoreOfHaloPm / 1000, ramp, 1000, true,
            false, /*opaque=*/true);
  mana_push(out, x, y, z, halo_r_px, ramp, halo_gain_pm, true, false);
}

/** LIGHTNING (candidate 4 / the channel's blaze). PASS 3 (R9): 2-3
 *  CONTINUOUS strands buzzing across the ring pocket's middle — each a
 *  two-layer stamped path (hot ~2 px near-white core over a calm wider
 *  halo), endpoints re-hashed on the kBoltRehashFrames cadence so they
 *  visibly BUZZ, never vanishing: old paths decay through the smear
 *  plane. Gap-free at native is the gate (kBoltStampMm under one core
 *  radius). */
inline void mana_lightning(uint32_t frame, const FxAnchors& A,
                           std::vector<ManaSplat>& out) {
  const uint32_t phase = frame / kBoltRehashFrames;
  int32_t pts[kBoltSegs + 1][3];
  for (int i = 0; i < kStrandCount; ++i) {
    const uint32_t h = fx_hash(kBoltSeed, phase, 0x51A0u + static_cast<uint32_t>(i));
    // endpoints across the pocket, through the middle: a hashed diameter
    const uint32_t ang = h & 0xFFFFu;
    const int32_t half = fxu(kStrandSpanMm / 2);
    const int32_t dx = static_cast<int32_t>(
        (static_cast<int64_t>(half) * fx_cos16(ang)) >> 16);
    const int32_t dy = static_cast<int32_t>(
        (static_cast<int64_t>(half) * fx_sin16(ang)) >> 16);
    int32_t s0[3] = {A.ring[0] + dx, A.ring[1] + dy, A.ring[2]};
    int32_t e0[3] = {A.ring[0] - dx, A.ring[1] - dy, A.ring[2]};
    s0[0] += fxu(fx_jit(h >> 8, kStrandEndJitMm));
    s0[1] += fxu(fx_jit(h >> 13, kStrandEndJitMm));
    e0[0] += fxu(fx_jit(h >> 18, kStrandEndJitMm));
    e0[1] += fxu(fx_jit(h >> 23, kStrandEndJitMm));
    bolt_path(s0, e0, kBoltSegs, phase * 3u + static_cast<uint32_t>(i),
              kBoltSeed + static_cast<uint32_t>(i) * 0x9E37u, pts, kStrandJitterMm);
    // constant presence with a per-frame flicker — a buzz, not a strobe
    const uint32_t hf = fx_hash(kBoltSeed ^ 0xF11Cu, frame, static_cast<uint32_t>(i));
    // pass 4 (R4): the brightness FLOOR is raised -- the median frame must
    // read as lightning, not glitter (the review's own sampling law)
    const int flick = 950 + static_cast<int>(hf % 130u);
    bolt_stamp(out, pts, kBoltSegs, kBoltCoreGainPm * flick / 1000,
               kBoltHaloGainPm * flick / 1000);
  }
  // pass 4 (R4): the SURGE MOTES -- the retired strands' energy, flowing
  // along the live strand's path and bursting at its ends. Positions are
  // re-derived from the same bolt path (deterministic; the path is the
  // rig-anchored diameter), so the surge follows every re-hash.
  {
    const uint32_t h0 = fx_hash(kBoltSeed, phase, 0x51A0u);
    const uint32_t ang = h0 & 0xFFFFu;
    const int32_t half = fxu(kStrandSpanMm / 2);
    const int32_t dx = static_cast<int32_t>((static_cast<int64_t>(half) * fx_cos16(ang)) >> 16);
    const int32_t dy = static_cast<int32_t>((static_cast<int64_t>(half) * fx_sin16(ang)) >> 16);
    int32_t s0[3] = {A.ring[0] + dx, A.ring[1] + dy, A.ring[2]};
    int32_t e0[3] = {A.ring[0] - dx, A.ring[1] - dy, A.ring[2]};
    for (int m = 0; m < kSurgeMotes; ++m) {
      const uint32_t hm = fx_hash(0x5069u, static_cast<uint32_t>(m), 0x11u);
      const int32_t t = static_cast<int32_t>(
          ((frame + hm % 97u) % static_cast<uint32_t>(kSurgeFlowFrames)) * 1000 /
          static_cast<uint32_t>(kSurgeFlowFrames));
      int32_t q[3];
      for (int k = 0; k < 3; ++k) q[k] = lerp32(s0[k], e0[k], t, 1000);
      q[0] += fxu(fx_jit(hm >> 3, 90));
      q[1] += fxu(fx_jit(hm >> 9, 90));
      mana_push(out, q[0], q[1], q[2], kSurgeRPx, kRampCyan, kSurgeGainPm, true, false);
      mana_push(out, q[0], q[1], q[2], kSurgeRPx * 55 / 100, kRampCyan, 1000, true,
                false, /*opaque=*/true);
    }
    // the endpoint bursts breathe on the re-hash cadence
    const int32_t bt = static_cast<int32_t>(frame % static_cast<uint32_t>(kBoltRehashFrames));
    const int32_t br = kSurgeBurstRPx - bt;
    if (br > 3) {
      mana_push(out, s0[0], s0[1], s0[2], br, kRampCyan, kSurgeBurstGainPm, true, false);
      mana_push(out, e0[0], e0[1], e0[2], br, kRampCyan, kSurgeBurstGainPm, true, false);
    }
  }
  // a small anamorphic glint at the pocket centre on each re-hash frame
  if (frame % kBoltRehashFrames == 0) {
    for (int i = -2; i <= 2; ++i) {
      const int32_t r = 6 - (i < 0 ? -i : i) * 2;
      mana_push(out, A.ring[0] + fxu(i * kStreakSpanPx * 25 / 24), A.ring[1],
                A.ring[2], r, kRampWhite, kStreakGainPm / 2, false, false);
    }
  }
}

/** The FILLED, CENTRE-ANCHORED bullet cloud (the §6d candidate rebuilt on
 *  R6/R7/R8): kBulletsN bodies that ORBIT AND JIGGLE around the ring
 *  centre — never ballistic escapees — each an opaque core under an
 *  additive halo, excursion bounded by kBulletSpreadMm ("a little spray
 *  is fine" = this knob, small). The smear plane grows their tails. */
inline void mana_bullets(uint32_t frame, const FxAnchors& A, uint8_t ramp,
                         std::vector<ManaSplat>& out) {
  for (int i = 0; i < kBulletsN; ++i) {
    const uint32_t h = fx_hash(31u, static_cast<uint32_t>(i), 7u);
    // per-bullet orbit radius (bounded), speed and plane phases
    const int32_t rad = fxu(150 + static_cast<int32_t>(h % static_cast<uint32_t>(
                                      kBulletSpreadMm - 150)));
    const uint32_t w = 65536u / (70u + (h >> 8) % 90u);  // period 70..159 frames
    const uint32_t p0 = h & 0xFFFFu;
    const int32_t ox = static_cast<int32_t>(
        (static_cast<int64_t>(rad) * fx_cos16(frame * w + p0)) >> 16);
    const int32_t oy = static_cast<int32_t>(
        (static_cast<int64_t>(rad * 3 / 4) *
         fx_sin16(frame * w * 2u + (h >> 16))) >> 16);
    const int32_t oz = static_cast<int32_t>(
        (static_cast<int64_t>(rad / 3) * fx_sin16(frame * w + p0 + 0x4000u)) >> 16);
    int32_t wob[3];
    centre_wobble(frame, 500u + static_cast<uint32_t>(i), kCentreWobbleMm / 2, wob);
    const int32_t x = A.ring[0] + ox + wob[0];
    const int32_t y = A.ring[1] + oy + wob[1];
    const int32_t z = A.ring[2] + oz + wob[2];
    mana_push(out, x, y, z, kBulletCoreRPx, ramp, 1000, true, false, /*opaque=*/true);
    mana_push(out, x, y, z, kBulletRPx, ramp, kBulletGainPm, true, false);
  }
  // the pocket's shared ambience: one soft pre-compose pool at the centre
  int32_t wob[3];
  centre_wobble(frame, 9u, kCentreWobbleMm, wob);
  mana_push(out, A.ring[0] + wob[0], A.ring[1] + wob[1], A.ring[2] + wob[2], 36,
            ramp, 130, true, true);
}

// ======================= THE FOLDING (pass 4 centrepiece) ==================
//
// The owner: "fold the mana into recognizable shapes. Then knead it into
// new shapes... it needs to look like they really affect the particles
// with their movement, not necessarily by touching them." The mechanism:
// each shape is a STENCIL of fat glow motes at FIXED generalized
// barycentric weights (mean-value coordinates, integer) over the six posed
// antenna anchors -- so the shape folds because the RIG folds, by
// construction; there is no other position law (R1: never collision, never
// proximity). GRIP (anchor-polygon area vs rest) drives coherence, KNEAD
// (anchor velocity) drives agitation, and DRAG (hinge B's lagged velocity)
// pulls the whole mass across the gap a beat late -- the iron-filings read.

// ---- PASS 12 / WAVE 2b: the two knobs the theatrical clips add here -------
// The lasso must READ as a loop for its whole flight, so its edge is held
// above the fold's ordinary draw threshold: the area-derived coherence is
// measuring the ANTENNA's pocket, which is not where the thrown ring is.
constexpr int32_t kLassoCohPm = 1000;
// A dying conduit drops the lightning STRAND at this life level rather than
// fading it -- a bolt at low strength reads as a glitch, not as an ebb.
constexpr int32_t kFoldLightningCutPm = 620;

constexpr int kStencilPts = 18;
// Stencil ids, for readability: 0 RING, 1 STAR, 2 BAR, 3 CRESCENT,
// 4 TRIANGLE, 5 S-CURL. Moved here from the lane-only lab header by pass 8,
// because the shipping edge (Direction 7 §2) needs them and the shipping path
// must not depend on a fork that ships nothing.
constexpr uint8_t kShRing = 0, kShStar = 1, kShBar = 2, kShCrescent = 3,
                  kShTriangle = 4, kShCurl = 5;
// ---- PASS 12: THREE MORE (D7 §3's lost half; inventory B7) ----------------
// "We also want more shapes, and shapes should not be the standard look, they
//  should happen intermittently."
//
// D7 §3 was implemented as its SECOND half only -- the intermittency landed in
// pass 7 (kDriftKeys*, the open-handed segment) and "more shapes" was never
// done. The inventory found it among the seven items that fell off every
// tracked list. Three figures, chosen to be nameable at ~37 px with a blobby
// stroke and to be UNLIKE the six already there:
//
//   BOLT   a zigzag. It rhymes with the creature's own identity -- this is the
//          animal whose mana is lightning -- and it is the only figure whose
//          silhouette is unmistakable even half-formed.
//   COIL   a tight spiral. The S-CURL is 1.5 lazy turns; this is 2.5 tight
//          ones, so it reads as WOUND rather than as a loose squiggle.
//   CROSS  an X. Two straight strokes, maximum contrast against the round
//          figures, and the only one with a centre crossing.
//
// The 18 stations are FIXED (kStencilPts is hard-wired into the weight tables
// and the mote path); re-authoring the station count is a separate lab and is
// deliberately out of scope. Every new figure is drawn inside the same 18.
constexpr uint8_t kShBolt = 6, kShCoil = 7, kShCross = 8;
// THE ONE AUTHORITY ON HOW MANY FIGURES EXIST.
//
// ⚠ IT IS NOT WIRED TO THE PICKER YET, ON PURPOSE, AND THIS IS THE HAND-OFF.
// `kFoldShapeCount` -- the modulus that decides which figure a clip draws --
// lives in manafold_clips.h, which belongs to the OTHER implementer this pass.
// The vocabulary is therefore split across a lane boundary: the table is here,
// the count is there. So these three are authored, weighted, edge-linked and
// rendered-ready, and the picker still selects from the first six.
//
// ONE LINE completes it: manafold_clips.h's `kFoldShapeCount = 6` becomes
// `= u02::kFoldStencilCount`. The static_assert below makes the mismatch
// impossible to ship silently in the other direction -- a count that outruns
// the table is a read off the end of it.
constexpr int kFoldStencilCount = 9;
// The pin's live value; -1 (the default, and the only shipping value) means
// the picker decides. Set by U02_FOLD_SHAPE in zhao_reel.cpp.
inline int g_u02_fold_shape_pin = -1;

// The six shape stencils, authored in pocket coordinates (u across the
// hole, v up; per-mille of kStencilScaleMm). Chosen for legibility with
// blobby strokes at ~37 px: RING (the opener), FOUR-POINT STAR (the
// identity, rhymes with the pupil), BAR (max contrast), CRESCENT (the
// Description sheet's rear view), TRIANGLE, S-CURL.
struct StencilPt { int16_t u_pm, v_pm; };
inline const StencilPt (&fold_stencils())[kFoldStencilCount][kStencilPts] {
  static StencilPt st[kFoldStencilCount][kStencilPts];
  static bool built = false;
  if (!built) {
    const auto scp = [](int i, int n, int32_t r_pm, int32_t ph16, int16_t& u, int16_t& v) {
      const uint16_t a = static_cast<uint16_t>((static_cast<int64_t>(i) * 65536 / n + ph16) & 0xFFFF);
      u = static_cast<int16_t>((static_cast<int64_t>(r_pm) * zref::fx_cos(zref::angle16{a}).raw) >> 16);
      v = static_cast<int16_t>((static_cast<int64_t>(r_pm) * zref::fx_sin(zref::angle16{a}).raw) >> 16);
    };
    for (int i = 0; i < kStencilPts; ++i) {
      // 0 RING: a full circle
      scp(i, kStencilPts, 1000, 0, st[0][i].u_pm, st[0][i].v_pm);
      // 1 FOUR-POINT STAR, drawn as SPOKES (iter 5: the outline read as a
      // wobbly ring; motes along four radial arms + a centre pair read as
      // the pupil star's own iconography)
      {
        if (i < 2) {
          st[1][i].u_pm = static_cast<int16_t>(i == 0 ? 0 : 90);
          st[1][i].v_pm = static_cast<int16_t>(i == 0 ? 0 : -90);
        } else {
          const int arm = (i - 2) / 4;          // 4 arms x 4 stations
          const int stn = (i - 2) % 4;
          static const int16_t r_of[4] = {320, 620, 900, 1150};
          const uint16_t a = static_cast<uint16_t>(0x2000 + arm * 0x4000);
          const int32_t r = r_of[stn];
          st[1][i].u_pm = static_cast<int16_t>((static_cast<int64_t>(r) * zref::fx_cos(zref::angle16{a}).raw) >> 16);
          st[1][i].v_pm = static_cast<int16_t>((static_cast<int64_t>(r) * zref::fx_sin(zref::angle16{a}).raw) >> 16);
        }
      }
      // 2 BAR: a thick diagonal stroke (two passes along the length)
      {
        const int half = kStencilPts / 2;
        const int j = i % half;
        const int32_t t = -1000 + 2000 * j / (half - 1);
        const int32_t off = i < half ? 190 : -190;  // stroke thickness
        st[2][i].u_pm = static_cast<int16_t>(t * 707 / 1000 - off * 707 / 1000);
        st[2][i].v_pm = static_cast<int16_t>(t * 707 / 1000 + off * 707 / 1000);
      }
      // 3 CRESCENT: a 260-degree open arc (the rear-view sheet's moon)
      {
        const int32_t span = 47000;  // ~260 deg in angle16
        const uint16_t a = static_cast<uint16_t>((0x5000 + static_cast<int64_t>(i) * span / (kStencilPts - 1)) & 0xFFFF);
        st[3][i].u_pm = static_cast<int16_t>((static_cast<int64_t>(950) * zref::fx_cos(zref::angle16{a}).raw) >> 16);
        st[3][i].v_pm = static_cast<int16_t>((static_cast<int64_t>(950) * zref::fx_sin(zref::angle16{a}).raw) >> 16);
      }
      // 4 TRIANGLE: three straight strokes
      {
        const int per = kStencilPts / 3;
        const int e = i / per, j = i % per;
        static const int16_t vx[4][2] = {{0, 1000}, {-870, -500}, {870, -500}, {0, 1000}};
        st[4][i].u_pm = static_cast<int16_t>(vx[e][0] + (vx[e + 1][0] - vx[e][0]) * j / per);
        st[4][i].v_pm = static_cast<int16_t>(vx[e][1] + (vx[e + 1][1] - vx[e][1]) * j / per);
      }
      // 5 S-CURL: a lazy spiral, 1.5 turns, radius decaying
      {
        const uint16_t a = static_cast<uint16_t>((static_cast<int64_t>(i) * 98304 / (kStencilPts - 1)) & 0xFFFF);
        const int32_t r = 1000 - 780 * i / (kStencilPts - 1);
        st[5][i].u_pm = static_cast<int16_t>((static_cast<int64_t>(r) * zref::fx_cos(zref::angle16{a}).raw) >> 16);
        st[5][i].v_pm = static_cast<int16_t>((static_cast<int64_t>(r) * zref::fx_sin(zref::angle16{a}).raw) >> 16);
      }
      // 6 BOLT: the zigzag. FOUR strokes down a descending staircase, the
      // classic lightning glyph. Authored as a polyline through five vertices
      // so the edge stamps run along real straight runs -- a bolt drawn as a
      // sampled curve reads as a wiggle, and the whole point of this figure is
      // that its corners are HARD.
      {
        static const int16_t bx[5][2] = {
            {-380, 1000}, {180, 210}, {-260, 130}, {330, -580}, {-120, -1000}};
        const int per = kStencilPts / 4;          // 4 runs x 4 stations = 16
        int e = i / per, j = i % per;
        if (e > 3) { e = 3; j = per - 1; }        // the 2 spare stations pile
                                                  // on the tail, which is the
                                                  // end a bolt should thicken
        st[6][i].u_pm = static_cast<int16_t>(bx[e][0] + (bx[e + 1][0] - bx[e][0]) * j / per);
        st[6][i].v_pm = static_cast<int16_t>(bx[e][1] + (bx[e + 1][1] - bx[e][1]) * j / per);
      }
      // 7 COIL: 2.5 TIGHT turns. The S-CURL next door is 1.5 lazy ones and
      // decays to 22% of its radius; this holds a fuller radius longer and then
      // winds in hard, so the two do not read as the same squiggle at 37 px --
      // which was the risk with adding a second spiral at all.
      {
        const uint16_t a = static_cast<uint16_t>((static_cast<int64_t>(i) * 163840 / (kStencilPts - 1)) & 0xFFFF);
        const int32_t r = 1000 - 880 * i * i / ((kStencilPts - 1) * (kStencilPts - 1));
        st[7][i].u_pm = static_cast<int16_t>((static_cast<int64_t>(r) * zref::fx_cos(zref::angle16{a}).raw) >> 16);
        st[7][i].v_pm = static_cast<int16_t>((static_cast<int64_t>(r) * zref::fx_sin(zref::angle16{a}).raw) >> 16);
      }
      // 8 CROSS: two straight strokes through the centre. Maximum contrast
      // against the five round figures, and the only one with a crossing.
      {
        const int half = kStencilPts / 2;
        const int j = i % half;
        const int32_t t = -1000 + 2000 * j / (half - 1);
        // first stroke runs "/" , second "\" -- the two diagonals
        const int32_t sgn = i < half ? 1 : -1;
        st[8][i].u_pm = static_cast<int16_t>(t * 707 / 1000);
        st[8][i].v_pm = static_cast<int16_t>(t * 707 / 1000 * sgn);
      }
    }
    built = true;
  }
  return st;
}

/** Mean-value coordinates of point p (mm, rest-uv space) over the six rest
 *  anchors (x,y of kFoldAnchorRestMm, a convex CCW hexagon). Integer-only;
 *  weights come back normalized Q12. Points are clamped toward the pocket
 *  centre until inside so every weight is non-negative -- the shape mass
 *  is bounded by construction (R8). */
/** DIRECTION 7 S2: does the shape's outline run from station i to station
 *  i+1 (mod 18)? RING and TRIANGLE close; CRESCENT and S-CURL are open arcs;
 *  the BAR is two separate passes so its seam is skipped; the STAR is four
 *  spokes, so segments run only WITHIN an arm. Ported verbatim from the
 *  experimental reel's lab_edge_link, which is where the owner saw it.
 *  Duplicated rather than shared because manafold_lab.h is a lane-only fork
 *  that ships nothing, and the shipping path must not depend on it. */
inline bool fold_edge_link(uint8_t shape, int i) {
  switch (shape) {
    case kShRing:
    case kShTriangle:
      return true;  // closed: 17 -> 0 included
    case kShCrescent:
    case kShCurl:
      return i < kStencilPts - 1;  // open arc: no wrap
    case kShBar:
    case kShCross:
      // two separate strokes: the seam between them is not an edge. The CROSS
      // shares the BAR's construction exactly, so it shares its link rule --
      // written as a fallthrough rather than a copy, because two rules that
      // must agree and are stated twice are two rules that will stop agreeing.
      return i < kStencilPts - 1 && i != (kStencilPts / 2) - 1;  // skip the seam
    case kShBolt:
    case kShCoil:
      return i < kStencilPts - 1;  // open polylines: no wrap
    case kShStar:
      if (i < 2) return false;
      return ((i - 2) % 4) != 3 && i < kStencilPts - 1;
    default:
      return false;
  }
}

inline void fold_mvc(int32_t pu, int32_t pv, uint16_t w[6]) {
  const int32_t cu = kStencilCentreUMm, cv = kStencilCentreVMm;
  for (int shrink = 0; shrink < 12; ++shrink) {
    int64_t d[6], dx[6], dy[6];
    for (int i = 0; i < 6; ++i) {
      dx[i] = kFoldAnchorRestMm[i][0] - pu;
      dy[i] = kFoldAnchorRestMm[i][1] - pv;
      d[i] = isqrt64(dx[i] * dx[i] + dy[i] * dy[i]);
      if (d[i] < 2) {  // on an anchor: all weight there
        for (int k = 0; k < 6; ++k) w[k] = 0;
        w[i] = 4096;
        return;
      }
    }
    int64_t t[6];
    bool outside = false;
    for (int i = 0; i < 6; ++i) {
      const int j = (i + 1) % 6;
      const int64_t cross = dx[i] * dy[j] - dy[i] * dx[j];
      const int64_t dot = dx[i] * dx[j] + dy[i] * dy[j];
      if (cross <= 0) {  // outside (or on) this edge: clamp inward, retry
        outside = true;
        break;
      }
      t[i] = ((d[i] * d[j] - dot) << 12) / cross;  // tan(half angle), Q12
    }
    if (outside) {
      pu = cu + (pu - cu) * 9 / 10;
      pv = cv + (pv - cv) * 9 / 10;
      continue;
    }
    int64_t wsum = 0, wq[6];
    for (int i = 0; i < 6; ++i) {
      const int h = (i + 5) % 6;
      wq[i] = ((t[h] + t[i]) << 12) / d[i];
      wsum += wq[i];
    }
    for (int i = 0; i < 6; ++i)
      w[i] = static_cast<uint16_t>(wsum > 0 ? (wq[i] * 4096) / wsum : 682);
    return;
  }
  for (int k = 0; k < 6; ++k) w[k] = 682;  // degenerate: centroid-ish
}

/** The per-mote weight tables: [shape][station] -> Q12 weights over the six
 *  anchors, computed ONCE from the authored stencils in the rest layout. */
struct FoldWeights {
  // [shape][station] -> Q12 weights over the SIX ANCHORS. The two sixes used
  // to look like the same number and were not: the first is the figure count
  // and the second is the hexagon's corner count. Only the first one moved.
  uint16_t w[kFoldStencilCount][kStencilPts][6];
};
// The picker's modulus must never outrun the table it indexes: kFoldShapeCount
// lives in manafold_clips.h and kFoldStencilCount here, and a mismatch in this
// direction is a read off the end of the weight tables. Bumping the picker to
// nine is safe the moment this holds; bumping it to ten stops the build.
static_assert(kFoldShapeCount == kFoldStencilCount,
              "the fold's shape picker indexes more figures than are authored");

inline const FoldWeights& fold_weights() {
  static FoldWeights fw;
  static bool built = false;
  if (!built) {
    const StencilPt(&st)[kFoldStencilCount][kStencilPts] = fold_stencils();
    for (int sh = 0; sh < kFoldStencilCount; ++sh)
      for (int i = 0; i < kStencilPts; ++i) {
        const int32_t pu = kStencilCentreUMm +
            static_cast<int32_t>(st[sh][i].u_pm) * kStencilScaleMm / 1000;
        const int32_t pv = kStencilCentreVMm +
            static_cast<int32_t>(st[sh][i].v_pm) * kStencilScaleMm / 1000;
        fold_mvc(pu, pv, fw.w[sh][i]);
      }
    built = true;
  }
  return fw;
}

/** Per-conduit, per-subject fold state: previous-frame anchors (the knead
 *  velocity), the drag ring buffer (hinge B's lagged relative velocity),
 *  and the smoothed agitation. Deterministic: reset at frame 0. */
struct FoldState {
  bool init = false;
  int32_t prev_rel[6][3];   // anchors relative to the body, fx16
  int32_t knead_smooth = 0; // smoothed anchor speed, mm/frame (~4-frame EMA)
  int32_t knead_slow = 0;   // the slow baseline (~64-frame EMA): the clip's
                            // own resting wobble, which must NOT read as
                            // kneading (iter 3 -- raw speed saturated at rest)
  int32_t dragbuf[8][3];    // rel hinge-B velocity ring (fx16/frame)
  uint32_t drag_idx = 0;
  int32_t area_ema_pm = 1000;  // ~16-frame smoothed area: the wobble's own
                               // 46/102-frame waves must not flap coherence
};

// Diagnostic gates (env, default off): U02_FOLD_LOCK=1 forces full
// coherence with no cloud/drag/jitter -- a stencil X-RAY that shows the
// pure barycentric shape; U02_FOLD_DEBUG=1 prints the per-frame scalars.
inline int g_u02_fold_lock = 0;
inline int g_u02_fold_debug = 0;
// D10 rung overrides, set from zhao_reel.cpp's env block. -1 == "use the
// constant". They exist so the owner's plate is a ladder from ONE BINARY
// (10-GATE-CHECKLIST item 26), and so he can move a value without a rebuild.
inline int g_u02_strand_on = -1;
inline int g_u02_strand_core_r = -1;
inline int g_u02_strand_dark_r = -1;
inline int g_u02_strand_dark_gain = -1;
inline int g_u02_strand_core_gain = -1;
inline int g_u02_free_strand = -1;
inline int g_u02_strand_perseg = -1;
inline int g_u02_strand_cap = -1;
// PASS 15 / D11 rung overrides: the shimmer's three axes, the mote count, and
// the aqua rebalance. Same contract -- -1 means "use the constant", so an
// unset environment renders the shipping bytes.
inline int g_u02_shimmer_r = -1;
inline int g_u02_shimmer_gain = -1;
inline int g_u02_shimmer_flicker = -1;
inline int g_u02_strand_motes = -1;
inline int g_u02_aqua_bal = 0;      // 0 = the shipped aqua; 1 = the B>=G rung
inline bool u02_strand_on() {
  return g_u02_strand_on < 0 ? kFoldStrandOn : g_u02_strand_on != 0;
}
inline int32_t u02_strand_core_r() {
  return g_u02_strand_core_r < 0 ? kFoldStrandCoreRPx : g_u02_strand_core_r;
}
inline int32_t u02_strand_dark_r() {
  return g_u02_strand_dark_r < 0 ? kFoldStrandDarkRPx : g_u02_strand_dark_r;
}
inline int u02_strand_dark_gain() {
  return g_u02_strand_dark_gain < 0 ? kFoldStrandDarkGainPm : g_u02_strand_dark_gain;
}
/** PASS 15: the white core's gain got no override in pass 14, and that is the
 *  "sweeping the axis I had a knob on instead of the axis that carries the
 *  fault" error this file names twice. It is here now whether or not it bites:
 *  a clamped additive sum makes gain a no-op, and whether THIS sum clamps is a
 *  question for the plate, not for me. */
inline int u02_strand_core_gain() {
  return g_u02_strand_core_gain < 0 ? kFoldStrandCoreGainPm : g_u02_strand_core_gain;
}
inline int u02_strand_perseg() {
  return g_u02_strand_perseg < 1 ? kFoldStrandPerSeg : g_u02_strand_perseg;
}
inline int u02_strand_cap() {
  return g_u02_strand_cap < 1 ? kFoldStrandCapN : g_u02_strand_cap;
}
inline int32_t u02_shimmer_r() {
  return g_u02_shimmer_r < 0 ? kFoldShimmerRPx : g_u02_shimmer_r;
}
inline int u02_shimmer_gain() {
  return g_u02_shimmer_gain < 0 ? kFoldShimmerGainPm : g_u02_shimmer_gain;
}
inline int u02_shimmer_flicker() {
  return g_u02_shimmer_flicker < 0 ? kFoldShimmerFlickerPm : g_u02_shimmer_flicker;
}
/** The mote garnish. The lightning read carries D11's larger population; the
 *  green/aqua fold keeps the count D9 §3 cut it to. One function so the two
 *  populations cannot be confused at a call site -- 09-ENGINE-GOTCHAS §14, one
 *  knob serving two features is a knob the owner has for neither. */
inline int u02_mote_garnish_pm() {
  if (!u02_strand_on()) return kFoldMoteGarnishPm;
  return g_u02_strand_motes < 0 ? kFoldStrandMoteGarnishPm : g_u02_strand_motes;
}
inline bool u02_free_strand_on() {
  if (g_u02_free_strand >= 0) return g_u02_free_strand != 0;  // env wins
  // Default: present exactly when the D10 strand is NOT. Strand off ==
  // pass 13 byte for byte; strand on == the owner's read, with no stray
  // diameter drawn across the figure it is trying to draw.
  return kFoldFreeStrandOn && !u02_strand_on();
}
// U02_FOLD_FREEZE=1 (pass 5; replaces the retired U02_ABLATE_KNEAD): the
// bones keep animating, and ONLY the field's anchor input is frozen at
// the rest layout. The mana must go static/limp while the antenna keeps
// working; if it still tracks the antenna, the coupling is decorative and
// the feature has failed. This isolates what the old ablation could not:
// zeroing the choreography moved the bones themselves, so its A/B could
// never separate field-follows-rig from rig-moved-so-projection-moved.
inline int g_u02_fold_freeze = 0;
// PASS 5 (loop seam): the fold's RELEASE amp, exported for the smear feed.
// During the release tail the feed fades with the amp, so the trail plane
// has decayed to near-empty by the wrap and the always-playing loop does
// not pop from "trails" to "no trails". 1000 everywhere else.
inline int32_t g_u02_fold_release_pm = 1000;

/** THE CENTREPIECE: place the folded motes for one conduit. `keys` = the
 *  clip's key count (the shared timeline domain); `crowd_pm` scales the
 *  mote count when several conduits share the frame. Returns the agitation
 *  (0..1000) so the caller can raise the smear feed with it. */
inline int32_t mana_fold(uint32_t frame, uint32_t slot, int keys, const FxAnchors& A,
                         FoldState& stfx, uint8_t ramp, int crowd_pm,
                         std::vector<ManaSplat>& out) {
  const int32_t* anchors[6] = {A.junction_f, A.neck, A.hinge_a,
                               A.hinge_b,    A.hinge_c, A.junction_b};
  // U02_FOLD_FREEZE: substitute the rest layout (body-relative) for the
  // posed anchors. Everything downstream -- KNEAD, GRIP, DRAG, the stencil
  // sum -- then sees a rig that never moves, while the drawn creature's
  // bones keep animating. See the comment at g_u02_fold_freeze.
  int32_t frozen[6][3];
  if (g_u02_fold_freeze) {
    for (int i = 0; i < 6; ++i) {
      for (int k = 0; k < 3; ++k)
        frozen[i][k] = A.body[k] + fxu(kFoldAnchorRestMm[i][k]);
      anchors[i] = frozen[i];
    }
  }
  // ---- rig-derived scalars (joint state ONLY -- R1) ----------------------
  int32_t rel[6][3];
  for (int i = 0; i < 6; ++i)
    for (int k = 0; k < 3; ++k) rel[i][k] = anchors[i][k] - A.body[k];
  if (frame == 0 || !stfx.init) {
    for (int i = 0; i < 6; ++i)
      for (int k = 0; k < 3; ++k) stfx.prev_rel[i][k] = rel[i][k];
    for (auto& v : stfx.dragbuf) v[0] = v[1] = v[2] = 0;
    stfx.knead_smooth = 0;
    stfx.drag_idx = 0;
    stfx.init = true;
  }
  // KNEAD: summed anchor speed (mm/frame), smoothed over ~4 frames
  int64_t raw = 0;
  for (int i = 0; i < 6; ++i) {
    for (int k = 0; k < 3; ++k) {
      const int64_t dd = rel[i][k] - stfx.prev_rel[i][k];
      raw += dd < 0 ? -dd : dd;
    }
  }
  const int32_t raw_mm = static_cast<int32_t>((raw * 1000) >> 16);
  stfx.knead_smooth += (raw_mm - stfx.knead_smooth) / 4;
  if (stfx.knead_slow == 0) stfx.knead_slow = raw_mm;  // warm start
  stfx.knead_slow += (raw_mm - stfx.knead_slow) / 64;
  // agitation is the EXCESS over the slow baseline: the resting wobble
  // cancels itself out; only a genuinely faster gesture churns the mana
  const int32_t excess = stfx.knead_smooth - stfx.knead_slow * 12 / 10;
  const int32_t agit = excess <= 0 ? 0
                       : excess >= kKneadVelRefMm
                           ? 1000
                           : excess * 1000 / kKneadVelRefMm;
  // DRAG: hinge B's relative velocity, ring-buffered for the per-mote lag
  {
    stfx.dragbuf[stfx.drag_idx & 7][0] = rel[3][0] - stfx.prev_rel[3][0];
    stfx.dragbuf[stfx.drag_idx & 7][1] = rel[3][1] - stfx.prev_rel[3][1];
    stfx.dragbuf[stfx.drag_idx & 7][2] = rel[3][2] - stfx.prev_rel[3][2];
    ++stfx.drag_idx;
  }
  for (int i = 0; i < 6; ++i)
    for (int k = 0; k < 3; ++k) stfx.prev_rel[i][k] = rel[i][k];
  // GRIP: the anchor polygon's area vs its rest area (closed form)
  int64_t live_area2;
  {
    int64_t cx = 0, cy = 0, cz = 0;
    int32_t rmm[6][3];
    for (int i = 0; i < 6; ++i) {
      for (int k = 0; k < 3; ++k)
        rmm[i][k] = static_cast<int32_t>((static_cast<int64_t>(rel[i][k]) * 1000) >> 16);
      cx += rmm[i][0]; cy += rmm[i][1]; cz += rmm[i][2];
    }
    cx /= 6; cy /= 6; cz /= 6;
    int64_t sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < 6; ++i) {
      const int j = (i + 1) % 6;
      const int64_t ax = rmm[i][0] - cx, ay = rmm[i][1] - cy, az = rmm[i][2] - cz;
      const int64_t bx = rmm[j][0] - cx, by = rmm[j][1] - cy, bz = rmm[j][2] - cz;
      sx += ay * bz - az * by;
      sy += az * bx - ax * bz;
      sz += ax * by - ay * bx;
    }
    live_area2 = isqrt64(sx * sx + sy * sy + sz * sz);  // 2x area, mm^2
  }
  static const int64_t rest_area2 = [] {
    int64_t cx = 0, cy = 0, cz = 0;
    for (int i = 0; i < 6; ++i) {
      cx += kFoldAnchorRestMm[i][0]; cy += kFoldAnchorRestMm[i][1]; cz += kFoldAnchorRestMm[i][2];
    }
    cx /= 6; cy /= 6; cz /= 6;
    int64_t sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < 6; ++i) {
      const int j = (i + 1) % 6;
      const int64_t ax = kFoldAnchorRestMm[i][0] - cx, ay = kFoldAnchorRestMm[i][1] - cy,
                    az = kFoldAnchorRestMm[i][2] - cz;
      const int64_t bx = kFoldAnchorRestMm[j][0] - cx, by = kFoldAnchorRestMm[j][1] - cy,
                    bz = kFoldAnchorRestMm[j][2] - cz;
      sx += ay * bz - az * by;
      sy += az * bx - ax * bz;
      sz += ax * by - ay * bx;
    }
    const int64_t a2 = isqrt64(sx * sx + sy * sy + sz * sz);
    return a2 > 0 ? a2 : 1;
  }();
  const int32_t area_pm = static_cast<int32_t>((live_area2 * 1000) / rest_area2);
  stfx.area_ema_pm += (area_pm - stfx.area_ema_pm) / 16;
  int32_t coh = kCohBasePm + (1000 - stfx.area_ema_pm) * kGripGamma;
  if (coh < kCohMinPm) coh = kCohMinPm;
  if (coh > 1000) coh = 1000;
  if (g_u02_fold_lock) coh = 1000;
  // ---- the shared timeline (shape choice + morph; key = frame / 2) -------
  FoldPhase ph = fold_phase(slot, keys, static_cast<int32_t>(frame) * 8);
  // PASS 12 / WAVE 2b (Direction 9 SS11.2): "the mana must respond -- a dead
  // conduit should not keep folding shapes." fold_life_pm() is 1000 for every
  // slot in the bank except the two deaths, so this scales nothing anywhere
  // else; on a death it takes the coherence, the agitation and the mote count
  // down together and then stops the fold outright at the settle key.
  const int32_t life_pm = fold_life_pm(slot, keys, static_cast<int32_t>(frame) * 8);
  if (life_pm <= 0) return 0;  // eternal rest: no shape, no motes, no edge
  if (life_pm < 1000) {
    coh = coh * life_pm / 1000;
    crowd_pm = crowd_pm * life_pm / 1000;
    ph.agit_pm = ph.agit_pm * life_pm / 1000;
    ph.amp_pm = ph.amp_pm * life_pm / 1000;
  }
  // PASS 12 / WAVE 2b (Direction 9 SS15): THE MANA LASSO. Not a new primitive
  // -- the throw PINS the fold's figure to stencil 0, THE RING, and then
  // translates, scales and spins the shape the fold already draws. The
  // substance, the edge, the motes and the knead are the ordinary ones, which
  // is what makes the lasso read as the creature's own mana rather than as a
  // prop. lasso_at() is inert for every other slot.
  const LassoState lasso = lasso_at(slot, keys, static_cast<int32_t>(frame) * 8);
  if (lasso.active) {
    ph.shape_from = 0;  // RING
    ph.shape_to = 0;
    ph.morph_pm = 0;
    // a thrown loop must READ as a loop for the whole flight, so the edge is
    // held above its draw threshold rather than left to the area-derived
    // coherence, which is measuring the ANTENNA and not the flying ring
    if (coh < kLassoCohPm) coh = kLassoCohPm;
  }
  // PASS 12 DIAGNOSTIC (default off): U02_FOLD_SHAPE=<id> pins the figure so a
  // single stencil can be looked at on its own, through the SHIPPING draw path
  // -- the same morph, the same edge, the same knead. It exists because the
  // three figures added this pass (BOLT, COIL, CROSS) cannot be reached by the
  // picker yet, and a figure nobody has rendered is a figure nobody has
  // judged. It pins shape_from AND shape_to, so the morph holds still and the
  // plate shows the figure rather than a blend of two.
  if (g_u02_fold_shape_pin >= 0) {
    ph.shape_from = static_cast<uint8_t>(g_u02_fold_shape_pin);
    ph.shape_to = static_cast<uint8_t>(g_u02_fold_shape_pin);
  }
  g_u02_fold_release_pm = ph.seg == kSegRelease ? ph.amp_pm : 1000;
  const FoldWeights& fw = fold_weights();
  if (g_u02_fold_debug)
    std::fprintf(stderr,
                 "fold f=%u seg=%d amp=%d agit_env=%d morph=%d %d->%d | "
                 "area_pm=%d ema=%d coh=%d knead_mm=%d agit=%d\n",
                 frame, static_cast<int>(ph.seg), ph.amp_pm, ph.agit_pm,
                 ph.morph_pm, ph.shape_from, ph.shape_to, area_pm, stfx.area_ema_pm, coh,
                 stfx.knead_smooth, agit);
  // ---- DIRECTION 7 S2: the shape is PLACED, then DRAWN AS AN EDGE --------
  //
  // `place()` is the ONE place the stencil's offset from the pocket centre is
  // rotated and kneaded, so the outline and the motes cannot disagree about
  // where the shape is -- the same defect class as pass 5's star and its
  // separately authored ring.
  //
  // ROTATE ON ALL AXES. Pass 7 had a single authored yaw (kStencilFaceYawA16)
  // whose job is to face the shape at the house camera. The owner asks for the
  // fold to "rotate the mana on all axis", so two slow, incommensurate turns
  // about X and Z ride on top of that fixed yaw. The yaw itself stays fixed and
  // authored: it is the thing that keeps the shape facing the camera at all.
  //
  // MALLEABLE. "the shapes should look a bit malleable like they're being
  // knead" -- the stencil offset is scaled ANISOTROPICALLY by the knead
  // agitation the antenna is already producing, one axis out and the other in,
  // so the shape changes PROPORTION rather than size. A uniform scale would
  // read as the shape moving toward the camera, not as being squeezed. Driven
  // by `agit`, the same signal the mote jitter uses, so a shape only goes soft
  // while the creature is actually kneading it.
  const uint32_t rot_ph = frame;
  const int32_t rx_a16 = static_cast<int32_t>(
      (static_cast<int64_t>(kStencilRotXAmpA16) *
       fx_sin16(rot_ph * 65536u / static_cast<uint32_t>(kStencilRotXFrames))) >> 16);
  const int32_t rz_a16 = static_cast<int32_t>(
      (static_cast<int64_t>(kStencilRotZAmpA16) *
       fx_sin16(rot_ph * 65536u / static_cast<uint32_t>(kStencilRotZFrames) + 0x3000u)) >> 16);
  const int32_t knead_pm = kStencilKneadAmpPm * agit / 1000;
  const auto place = [&](int32_t P[3]) {
    int32_t o[3] = {P[0] - A.ring[0], P[1] - A.ring[1], P[2] - A.ring[2]};
    if (knead_pm != 0) {
      const int32_t kq = static_cast<int32_t>(
          (static_cast<int64_t>(knead_pm) *
           fx_sin16(rot_ph * 65536u /
                    static_cast<uint32_t>(kStencilKneadFrames))) >> 16);
      o[0] = static_cast<int32_t>((static_cast<int64_t>(o[0]) * (1000 + kq)) / 1000);
      o[1] = static_cast<int32_t>((static_cast<int64_t>(o[1]) * (1000 - kq)) / 1000);
    }
    const auto turn = [](int32_t& a, int32_t& b, int32_t ang) {
      const int32_t sn = fx_sin16(static_cast<uint32_t>(ang));
      const int32_t cs = fx_cos16(static_cast<uint32_t>(ang));
      const int32_t na = static_cast<int32_t>(
          ((static_cast<int64_t>(a) * cs) >> 16) - ((static_cast<int64_t>(b) * sn) >> 16));
      const int32_t nb = static_cast<int32_t>(
          ((static_cast<int64_t>(a) * sn) >> 16) + ((static_cast<int64_t>(b) * cs) >> 16));
      a = na;
      b = nb;
    };
    turn(o[0], o[2], kStencilFaceYawA16);  // Y: the authored facing
    turn(o[1], o[2], rx_a16);              // X
    turn(o[0], o[1], rz_a16);              // Z
    // THE LASSO (D9 SS15): the ring OPENS as it flies and CINCHES as it is
    // reeled home, and it spins about the throw axis on the way. Applied here,
    // inside the one place the shape's offset is transformed, so the outline
    // and the motes cannot disagree about where the lasso is -- the same
    // defect class pass 5's separately-authored star and ring produced.
    if (lasso.active) {
      for (int li = 0; li < 3; ++li)
        o[li] = static_cast<int32_t>((static_cast<int64_t>(o[li]) * lasso.scale_pm) / 1000);
      turn(o[0], o[1], lasso.spin_a16);
    }
    // "Shapes are clipping into the antennae a bit though so you have to switch
    // them about": MOVE THE SHAPES, NOT THE ANTENNAE. One declared offset of
    // the whole shape, out of the antenna band's own plane.
    // ...and the THROW itself: the whole shape is carried out along the
    // authored flight and back. IT TRAVELS, so its RETURN IS AUTHORED (SS7.8:
    // "leaving stuff hanging in space just looks like a glitch") -- lasso_at()
    // brings the offset to exactly zero and the scale to exactly 1000 at
    // kLassoHomeKey, so the hand-back to the ordinary fold has no step in it.
    P[0] = A.ring[0] + o[0] + fxu(kStencilClearXMm) + fxu(lasso.off_mm[0]);
    P[1] = A.ring[1] + o[1] + fxu(kStencilClearYMm) + fxu(lasso.off_mm[1]);
    P[2] = A.ring[2] + o[2] + fxu(kStencilClearZMm) + fxu(lasso.off_mm[2]);
  };

  // THE EDGE. Drawn only while there IS a shape: below kFoldEdgeCohMinPm the
  // cloud has dispersed into the standard channel look (S3's drift) and an
  // outline over it would be a scribble rather than a shape.
  if (coh >= kFoldEdgeCohMinPm) {
    const int32_t lit = (coh - kFoldEdgeCohMinPm) * 1000 /
                        (1000 - kFoldEdgeCohMinPm > 0 ? 1000 - kFoldEdgeCohMinPm : 1);
    int32_t S[kStencilPts][3];
    for (int i = 0; i < kStencilPts; ++i) {
      const uint16_t* wf = fw.w[ph.shape_from][i];
      const uint16_t* wt = fw.w[ph.shape_to][i];
      for (int k = 0; k < 3; ++k) {
        int64_t af = 0, at = 0;
        for (int j = 0; j < 6; ++j) {
          af += static_cast<int64_t>(wf[j]) * anchors[j][k];
          at += static_cast<int64_t>(wt[j]) * anchors[j][k];
        }
        S[i][k] = lerp32(static_cast<int32_t>(af >> 12),
                         static_cast<int32_t>(at >> 12), ph.morph_pm, 1000);
      }
      place(S[i]);
    }
    const uint32_t ph_e = frame / 3u;  // the outline BUZZES, it does not crawl
    // OWNER DIRECTION 10: the edge is drawn as LIGHTNING -- a white hot line
    // wrapped in a deep dark blue -- or, with kFoldStrandOn false, as pass
    // 13's dim aqua outline. Read once per call so a rung sweep cannot
    // change identity between two stations of the same figure.
    const bool strand = u02_strand_on();
    const int32_t stamp_mm = kFoldEdgeStampMm;   // the pass-13 path only
    const int per_seg = u02_strand_perseg();
    const int cap_n = strand ? u02_strand_cap() : 24;
    const int32_t core_r = u02_strand_core_r();
    const int32_t dark_r = u02_strand_dark_r();
    const int dark_gain = u02_strand_dark_gain();
    const int core_gain = u02_strand_core_gain();
    int32_t pts[kFoldEdgeSegs + 1][3];
    // THE PICTURE THAT FORCED THE TWO-PASS DRAW (pass14-plates/d10-size.png):
    // the first strand build stamped dark-then-white PER BEAD, and raising the
    // radii to core 7 / dark 24 to make the halo legible made the pocket
    // LIGHTER and mushier instead -- the opposite of the ask. The reason is
    // ordering, not scale: with the two interleaved, every bead's ADDITIVE
    // white lands on the PREVIOUS bead's dark and cancels it, so the halo can
    // never be more than a bruise between beads and the whites just accumulate.
    //
    // "Surrounded by a deep dark blue" is a property of the WHOLE LINE, not of
    // each bead. So the strand is drawn in two passes over the same stations:
    // pass 0 lays the entire dark surround, pass 1 lays every white core on top
    // of the finished dark. bolt_path is deterministic in (i, ph_e, seed), so
    // pass 1 walks exactly the path pass 0 darkened -- no cached point buffer,
    // and no chance of the two passes disagreeing about where the figure is.
    // PASS 15: THREE passes, not two, and the new middle one is D11's whole
    // sentence. The two-pass ordering finding above stands and is the reason
    // this is a pass rather than a third mana_push beside the others: the
    // shimmer is a property of the WHOLE LINE too, so it must land on the
    // finished navy and under the finished white. Order is
    // navy -> shimmer -> white, outermost first, exactly as the layers stack.
    const int passes = strand ? 3 : 1;
    const int32_t shim_r = u02_shimmer_r();
    const int shim_gain = u02_shimmer_gain();
    const int shim_flick = u02_shimmer_flicker();
    for (int pass = 0; pass < passes; ++pass)
    for (int i = 0; i < kStencilPts; ++i) {
      if (!fold_edge_link(ph.shape_to, i)) continue;
      const int j = (i + 1) % kStencilPts;
      bolt_path(S[i], S[j], kFoldEdgeSegs, ph_e, kBoltSeed ^ (0x5EDu * (i + 1)),
                pts, kFoldEdgeJitterMm);
      for (int sgi = 0; sgi < kFoldEdgeSegs; ++sgi) {
        int64_t dx = (pts[sgi + 1][0] - pts[sgi][0]) >> 16;
        int64_t dy = (pts[sgi + 1][1] - pts[sgi][1]) >> 16;
        int64_t dz = (pts[sgi + 1][2] - pts[sgi][2]) >> 16;
        const int64_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy,
                      adz = dz < 0 ? -dz : dz;
        // The strand subdivides by COUNT (see kFoldStrandPerSeg); the
        // pass-13 outline keeps its millimetre spacing byte for byte, so
        // ZHAO_U02_STRAND=0 still reproduces the shipped picture exactly.
        int nst = strand ? per_seg
                         : static_cast<int>((adx + ady + adz) / stamp_mm);
        if (nst < 1) nst = 1;
        if (nst > cap_n) nst = cap_n;
        for (int t = 0; t < nst; ++t) {
          const int32_t x = lerp32(pts[sgi][0], pts[sgi + 1][0], t, nst);
          const int32_t y = lerp32(pts[sgi][1], pts[sgi + 1][1], t, nst);
          const int32_t z = lerp32(pts[sgi][2], pts[sgi + 1][2], t, nst);
          if (strand) {
            if (pass == 0) {
              // LAYER 1 (drawn first, sits under): THE NAVY BACKING.
              // opaque+soft, so it BLENDS TOWARD a
              // colour darker than either sky -- the only subtractive element
              // in this creature's whole mana path. Depth tested: a surround
              // is grounded, it is not energy shining through the animal.
              // It rides `lit` like everything else, so a figure that is not
              // gripping does not stamp a dark bruise on the sky.
              mana_push(out, x, y, z, dark_r, kRampStorm,
                        dark_gain * lit / 1000, true, false,
                        /*opaque=*/true, /*soft=*/true);
            } else if (pass == 1) {
              // LAYER 2: THE BLUE SHIMMER (D11) -- the layer this creature has
              // never had. ADDITIVE, because it is light and light adds;
              // depth test OFF, with the white core, because they are one
              // object. It sits on the finished navy, so the blue has
              // something dark to be bright against instead of the pale
              // sunset it would otherwise wash into (08-LIGHTING: measure
              // what an additive effect is drawn AGAINST before tuning it).
              //
              // The flicker is what makes it a shimmer rather than a blue
              // tube: a per-stamp, per-FRAME hash on the gain, so the light
              // crawls along the bolt between strikes instead of holding
              // still. It is deliberately wider than any other modulation on
              // these pixels -- the crayon-grain lesson is that a variation
              // narrower than what is already moving is not there.
              const uint32_t sh = fx_hash(
                  kBoltSeed ^ 0x5B1Eu, frame,
                  static_cast<uint32_t>((i * 97 + sgi * 13 + t) | 1));
              const int span = shim_flick * 2 + 1;
              const int mod = 1000 - shim_flick +
                              static_cast<int>(sh % static_cast<uint32_t>(span));
              mana_push(out, x, y, z, shim_r, kRampShimmer,
                        shim_gain * lit / 1000 * mod / 1000, false, false);
            } else {
              // LAYER 3: THE WHITE LINE, over the finished navy AND its
              // shimmer -- the hot centre of the bolt. Additive, depth
              // test OFF (the pulsar-core law: energy reads over flesh). It
              // lands inside a surround the whole figure already has, which
              // is the difference between this and the lab's rejected white
              // outline -- and, per the plate above, between a line and a
              // string of pale blobs.
              mana_push(out, x, y, z, core_r, kRampWhite,
                        core_gain * lit / 1000, false, false);
            }
          } else {
            mana_push(out, x, y, z, kFoldEdgeHaloRPx, ramp,
                      kFoldEdgeHaloGainPm * lit / 1000, true, false);
            // The core is pass 8's SOFT body, in the fold's own ramp. The lab
            // measured that an outline stamped with the lightning primitive's
            // hard-coded white core put 366 near-white px on screen and
            // dropped saturation to 108.9 against a 142.1 control -- true, and
            // superseded by Direction 10 above, which gives the white a dark
            // surround the lab's version never had.
            mana_push(out, x, y, z, kFoldEdgeCoreRPx, mana_core_ramp(ramp),
                      kFoldEdgeCoreGainPm, false, false, /*opaque=*/true,
                      /*soft=*/true);
          }
        }
      }
    }
  }

  // ---- the motes ---------------------------------------------------------
  // D11 SS4: "we want the other particles folded into shapes, and we want MORE
  // of them. These are the lightning particles." So while the bolt is drawing
  // the figure, the SHAPE motes stop being aqua garnish and become the bolt's
  // own vertices -- a hot white heart in a blue shimmer, at the count
  // u02_mote_garnish_pm() carries for that read. The green/aqua fold he likes
  // is the strand-OFF path and keeps every number it shipped with: "it
  // actually looks neat" is an instruction to leave it alone, not to unify it.
  const bool bolt_motes = u02_strand_on();
  const int garnish_pm = u02_mote_garnish_pm();
  int n_motes = kMoteCount * crowd_pm / 1000;
  n_motes = n_motes * garnish_pm / 1000;
  if (n_motes < 6) n_motes = 6;
  // ⚠ THE WANDERERS SCALE WITH THE GARNISH, AND NOT SCALING THEM IS WHY
  //  CUTTING THE MOTE COUNT DID NOT REMOVE THE FLOATING ORBS.
  //
  //  kWanderCount's own comment says "of kMoteCount" -- it was authored as a
  //  FRACTION of 38. It was then subtracted as an ABSOLUTE after the garnish
  //  scaling, so when pass 12 cut the cloud 38 -> 11 the shape motes went
  //  32 -> 5 and the wanderers stayed at 6:
  //
  //      before the cut   38 motes,  6 wander  -> 15% of the cloud
  //      after  the cut   11 motes,  6 wander  -> 54% of the cloud
  //
  //  The cut did not thin the drifters, it PROMOTED them to the majority --
  //  which is exactly the pass-12 review's item 3, "large soft round aqua
  //  motes sitting in the sky detached from the creature", still there after
  //  the count was cut. 09-ENGINE-GOTCHAS §18: the knob moved a population
  //  that was not the one being judged, so a second cut would have made the
  //  orbs worse again.
  //
  //  Scaled by the same garnish now, so the drifters stay the ~15% seasoning
  //  they were authored as whatever the cloud size becomes. At least one
  //  survives: D2's "drift off in weird ways" is still in the look, it is
  //  simply not the look any more.
  int n_wander = kWanderCount * crowd_pm / 1000 * garnish_pm / 1000;
  if (n_wander < 1) n_wander = 1;
  if (n_wander > n_motes - 2) n_wander = n_motes - 2 > 1 ? n_motes - 2 : 1;
  const int n_shape = n_motes - n_wander;
  for (int m = 0; m < n_motes; ++m) {
    const uint32_t hm = fx_hash(0xF01Du, static_cast<uint32_t>(m), 0xA7u);
    int32_t P[3];
    if (m >= n_shape) {
      // WANDER: slow hashed walks that leave the pocket and curve off oddly
      // (the owner's "drift off in weird ways"); the smear traces them.
      // PASS 5 (loop seam): the walk's two frequencies are quantised to
      // whole cycles over the clip, same law as the shape-mote orbits, so
      // the wanderers are back where they started at the wrap.
      const int per = 240 + static_cast<int>(hm % 200u);
      const uint32_t ph1 = hm & 0xFFFFu;
      const int32_t r1 = fxu(kWanderEscapeMm * (600 + static_cast<int32_t>((hm >> 4) % 400u)) / 1000);
      const int32_t r2 = r1 * 2 / 3;
      const int frames_total = keys * 2 > 0 ? keys * 2 : 1;
      int cycles = (frames_total + per / 2) / per;
      if (cycles < 1) cycles = 1;
      int cycles_slow = cycles / 3;
      if (cycles_slow < 1) cycles_slow = 1;
      const uint32_t fmod = frame % static_cast<uint32_t>(frames_total);
      const uint32_t th = static_cast<uint32_t>(
          (static_cast<uint64_t>(fmod) * static_cast<uint32_t>(cycles) << 16) /
          static_cast<uint32_t>(frames_total));
      const uint32_t th_slow = static_cast<uint32_t>(
          (static_cast<uint64_t>(fmod) * static_cast<uint32_t>(cycles_slow) << 16) /
          static_cast<uint32_t>(frames_total));
      P[0] = A.ring[0] + static_cast<int32_t>((static_cast<int64_t>(r1) * fx_cos16(th + ph1)) >> 16);
      P[1] = A.ring[1] +
             static_cast<int32_t>((static_cast<int64_t>(r2) * fx_sin16(th + (ph1 ^ 0x9A00u))) >> 16) +
             static_cast<int32_t>((static_cast<int64_t>(fxu(220)) * fx_sin16(th_slow + ph1)) >> 16);
      P[2] = A.ring[2] + static_cast<int32_t>((static_cast<int64_t>(r2) * fx_sin16(th + ph1 + 0x4000u)) >> 16);
    } else {
      const int stn = m * kStencilPts / (n_shape > 0 ? n_shape : 1);
      const auto bary = [&](uint8_t shape_id, int32_t q[3]) {
        const uint16_t* wt = fw.w[shape_id][stn];
        for (int k = 0; k < 3; ++k) {
          int64_t acc = 0;
          for (int i = 0; i < 6; ++i) acc += static_cast<int64_t>(wt[i]) * anchors[i][k];
          q[k] = static_cast<int32_t>(acc >> 12);
        }
      };
      int32_t Pf[3], Pt[3];
      bary(ph.shape_from, Pf);
      bary(ph.shape_to, Pt);
      // per-mote staggered, eased morph (a deterministic path each)
      const int32_t stag = static_cast<int32_t>(hm % 300u);
      int32_t mp = ph.morph_pm <= stag ? 0 : (ph.morph_pm - stag) * 1000 / (1000 - stag);
      mp = fold_ease(mp);
      int32_t Pst[3];
      for (int k = 0; k < 3; ++k) Pst[k] = lerp32(Pf[k], Pt[k], mp, 1000);
      // DIRECTION 7 S2: the SAME placement the edge uses -- authored facing
      // yaw, the two slow turns, the knead's malleability, the clearance
      // offset. Shared so the outline and the motes cannot disagree.
      place(Pst);
      // the cloud relax position: hashed offset + ONE slow consistent orbit
      // (R7: a single angular velocity per mote, long period, no doubling).
      // PASS 5 (the hover loop-seam, reviewer item 8): the hashed period is
      // QUANTISED to a whole number of orbits over the clip, so every
      // mote's orbit phase is identical at frame 0 and at the wrap -- the
      // release tail zeroed the fold amp but the orbits used to land
      // mid-turn, and the always-playing loop popped by ~2.4x the house
      // seam norm. The period only shifts within its own hashed band.
      const int per = kMoteOrbitPeriodMinF +
          static_cast<int>((hm >> 8) % static_cast<uint32_t>(kMoteOrbitPeriodMaxF - kMoteOrbitPeriodMinF));
      const int frames_total = keys * 2 > 0 ? keys * 2 : 1;
      int cycles = (frames_total + per / 2) / per;
      if (cycles < 1) cycles = 1;
      const uint32_t th = static_cast<uint32_t>(
          (static_cast<uint64_t>(frame % static_cast<uint32_t>(frames_total)) *
               static_cast<uint32_t>(cycles) << 16) /
          static_cast<uint32_t>(frames_total));
      const int32_t orad = fxu(kMoteOrbitRMinMm +
          static_cast<int32_t>((hm >> 16) % static_cast<uint32_t>(kMoteOrbitRMaxMm - kMoteOrbitRMinMm)));
      const uint32_t oph = (hm >> 3) & 0xFFFFu;
      int32_t orb[3];
      orb[0] = static_cast<int32_t>((static_cast<int64_t>(orad) * fx_cos16(th + oph)) >> 16);
      orb[1] = static_cast<int32_t>((static_cast<int64_t>(orad * 3 / 4) * fx_sin16(th + oph)) >> 16);
      orb[2] = static_cast<int32_t>((static_cast<int64_t>(orad / 2) * fx_sin16(th + oph + 0x3800u)) >> 16);
      int32_t cloud_off[3];
      cloud_off[0] = fxu(fx_jit(hm, kCloudSpreadMm));
      cloud_off[1] = fxu(fx_jit(hm >> 7, kCloudSpreadMm * 3 / 4));
      cloud_off[2] = fxu(fx_jit(hm >> 13, kCloudSpreadMm / 2));
      // coherence blends the mote from its relaxed cloud onto the stencil
      for (int k = 0; k < 3; ++k) {
        const int32_t cloud = Pst[k] + cloud_off[k] + orb[k];
        const int32_t tight = Pst[k] + orb[k] / 4;
        P[k] = lerp32(cloud, tight, coh, 1000);
      }
    }
    // KNEAD agitation: per-mote jitter that churns with fast gestures
    if (agit > 0 && !g_u02_fold_lock) {
      const uint32_t hj = fx_hash(frame / 2u, static_cast<uint32_t>(m), 0x177u);
      const int32_t jmm = kKneadJitterMm * agit / 1000;
      P[0] += fxu(fx_jit(hj, jmm));
      P[1] += fxu(fx_jit(hj >> 9, jmm));
      P[2] += fxu(fx_jit(hj >> 17, jmm));
    }
    // DRAG: the lagged pull along the antenna's sweep (iron filings).
    // PASS 5: clamped by MAGNITUDE (kDragMaxMm) so a violent stationary
    // gesture cannot fling the motes across the loop -- see the constant.
    if (!g_u02_fold_lock) {
      const int lag = kDragLagFrames + static_cast<int>((hm >> 21) % 4u);  // 2..5
      const uint32_t bi = stfx.drag_idx + 8u - static_cast<uint32_t>(lag);
      int32_t dsp[3];
      for (int k = 0; k < 3; ++k) {
        const int64_t v = static_cast<int64_t>(stfx.dragbuf[bi & 7][k]) +
                          stfx.dragbuf[(bi - 1u) & 7][k] + stfx.dragbuf[(bi - 2u) & 7][k];
        dsp[k] = static_cast<int32_t>(v * kDragGainPm / 1000);
      }
      const int64_t mag = isqrt64(static_cast<int64_t>(dsp[0]) * dsp[0] +
                                  static_cast<int64_t>(dsp[1]) * dsp[1] +
                                  static_cast<int64_t>(dsp[2]) * dsp[2]);
      const int64_t cap = fxu(kDragMaxMm);
      if (mag > cap && mag > 0) {
        for (int k = 0; k < 3; ++k)
          dsp[k] = static_cast<int32_t>(dsp[k] * cap / mag);
      }
      for (int k = 0; k < 3; ++k) P[k] += dsp[k];
    }
    // draw: an opaque heart under an additive halo (R7 -- filled and BIG)
    const int32_t halo = kMoteHaloRPxMin +
        static_cast<int32_t>((hm >> 5) % static_cast<uint32_t>(kMoteHaloRPxMax - kMoteHaloRPxMin + 1));
    // PASS 8 -- THE ORDER IS THE FIX, and it took looking to find it.
    //
    // The first pass-8 attempt gave the opaque heart its own dark saturated
    // ramp and the motes came back 50.5% hue-neutral at mean (217,233,239) --
    // MORE white than the 48.7% it was meant to cure. Rendering the motes-only
    // ablation and looking at it said why: the white blobs are ~7 px across and
    // the heart is ~3, so the white was never mostly the heart. It is the
    // ADDITIVE HALO, and 09-ENGINE-GOTCHAS §4 already names the law -- additive
    // over bright pink can only whiten. kManaAquaHi * kMoteHaloGainPm/1000 =
    // (59,86,80) added to a (200,140,150) sky is (255,226,230): the red clips,
    // the other two lift, and the spread collapses to hue-neutral. No colour
    // choice fixes that, because the arithmetic is the background's.
    //
    // What fixes it is that the halo was drawn LAST, so it also whitened the
    // heart. Push the halo FIRST and the opaque heart SECOND and the mote ends
    // as a SOLID SATURATED BODY with an additive glow around it, instead of a
    // saturated body buried under an additive glow. Nothing else moves: not the
    // count, not the radii, not the spread, and NOT kMoteHaloGainPm -- the lab
    // measured that raising any of those buys overlap and loses the hue, which
    // is the fault being fixed here. This is a draw-order change and a radius.
    // D11: the SHAPE motes join the lightning; the wanderers do not. A
    // wanderer is the drift garnish that leaves the pocket -- it is not folded
    // into anything, so it is not one of "the other particles" and it keeps
    // the fold's own colour. (09-ENGINE-GOTCHAS SS14, the other way round: two
    // populations that read differently need two decisions, not one knob.)
    const uint8_t mramp = (bolt_motes && m < n_shape)
                              ? static_cast<uint8_t>(kRampShimmer)
                              : ramp;
    mana_push(out, P[0], P[1], P[2], halo, mramp, kMoteHaloGainPm, true, false);
    mana_push(out, P[0], P[1], P[2], halo * kMoteCoreOfHaloPm / 1000,
              mana_core_ramp(mramp), 1000,
              true, false, /*opaque=*/true, /*soft=*/true);
  }
  return agit;
}

/** Fill the frame's mana splats for one conduit. `cand` selects the menu
 *  candidate (0 = none). PASS 3 (R13): drip is CUT (dead in 579 of 600
 *  frames); the menu is 1 pulsar, 2 filled deep blue, 3 aquamarine
 *  smeared plasma (the lead), 4 lightning strands (the channel's blaze —
 *  identity, not a menu item), 5 the boil CENTRE grown, 6 cyan smeared
 *  plasma (the long/glitchier smear rung), 7 filled sea-green, 8 THE
 *  STACK (pulsar + strands + aqua smear — the likely shipping stack,
 *  judged assembled). Every body is filled (R7) and centre-anchored (R8). */
inline void mana_fill(int cand, uint32_t frame, uint32_t slot, int keys,
                      const FxAnchors& A, FoldState& stfx, int crowd_pm,
                      std::vector<ManaSplat>& out, int32_t* agit_out = nullptr) {
  switch (cand) {
    case 1: {  // the caged pulsar — now with a FILLED heart
      const int32_t breathe = static_cast<int32_t>(
          (static_cast<int64_t>(kPulsarHaloMaxPx - kPulsarHaloMinPx) *
           ((65536 + fx_sin16(frame * 65536 / kPulsarBreathFrames)) / 2)) >> 16);
      int32_t wob[3];
      centre_wobble(frame, 1u, kCentreWobbleMm, wob);
      const int32_t x = A.ring[0] + wob[0], y = A.ring[1] + wob[1],
                    z = A.ring[2] + wob[2];
      mana_push(out, x, y, z, kPulsarHaloMinPx + breathe, kRampCyan,
                kPulsarHaloGainPm, true, true);  // pre: arms occlude it
      mana_push(out, x, y, z, kPulsarCorePx * 2 / 3, kRampCyan, 1000, true,
                false, /*opaque=*/true);  // the solid heart (R7)
      mana_push(out, x, y, z, kPulsarCorePx, kRampCyan, kPulsarCoreGainPm,
                false, false);  // no depth test: shines through the blade
      break;
    }
    case 2:    // filled deep blue blobs, minimal smear (R13 #3)
    case 7: {  // filled sea-green (the "try greens" ask, R13 #4)
      const uint8_t ramp = cand == 2 ? kRampDeepBlue : kRampSeaGreen;
      const int32_t r_px[3] = {40, 33, 27};
      for (int i = 0; i < 3; ++i) {
        int32_t wob[3];
        centre_wobble(frame, 40u + static_cast<uint32_t>(cand) * 8u +
                          static_cast<uint32_t>(i), kPlasmaSpreadMm, wob);
        mana_filled(out, A.ring[0] + wob[0], A.ring[1] + wob[1],
                    A.ring[2] + wob[2], r_px[i], ramp, kPlasmaGainPm);
      }
      break;
    }
    case 3: {  // THE FOLD, aquamarine — pass 4: the shipping mana. The
               // antenna folds the motes into shapes and kneads them
               // (bullets retired; the fold IS the plasma now).
      const int32_t ag = mana_fold(frame, slot, keys, A, stfx, kRampAqua, crowd_pm, out);
      if (agit_out) *agit_out = ag;
      break;
    }
    case 6: {  // THE FOLD, cyan — the long/glitchier smear rung
      const int32_t ag = mana_fold(frame, slot, keys, A, stfx, kRampCyan, crowd_pm, out);
      if (agit_out) *agit_out = ag;
      break;
    }
    case 4:
      mana_lightning(frame, A, out);
      break;
    case 9: {  // the CHANNEL stack: the fold + the lightning strand
      const int32_t ag = mana_fold(frame, slot, keys, A, stfx, kRampAqua, crowd_pm, out);
      // PASS 12 / WAVE 2b: the strand is the loudest thing the mana does, so a
      // dying conduit loses it FIRST and outright rather than fading it -- a
      // lightning bolt at 12% strength reads as a rendering fault, not as a
      // creature running out. 1000 for every slot but the two deaths.
      // OWNER DIRECTION 10: the free cross-pocket strand is OFF by default now.
      // It is a full-brightness white filament composited over the same pocket
      // with no knowledge of the stencil -- so under D10, where the EDGE is a
      // white strand, it is the same primitive drawing a diameter on top of a
      // figure. `kFoldFreeStrandOn` (and ZHAO_U02_FREE_STRAND) restores it.
      if (u02_free_strand_on() &&
          fold_life_pm(slot, keys, static_cast<int32_t>(frame) * 8) >=
          kFoldLightningCutPm)
        mana_lightning(frame, A, out);
      if (agit_out) *agit_out = ag;
      break;
    }
    case 5: {  // the boil CENTRE, grown 1.6x, outer removed (R13 #5)
      int32_t wob[3];
      centre_wobble(frame, 5u, kCentreWobbleMm, wob);
      const int32_t x = A.ring[0] + wob[0], y = A.ring[1] + wob[1],
                    z = A.ring[2] + wob[2];
      mana_push(out, x, y, z, kBoilCentrePx * kCoreOfHaloPm / 1000, kRampBlue,
                1000, true, false, /*opaque=*/true);
      mana_push(out, x, y, z, kBoilCentrePx, kRampBlue, kBoilCoreGainPm,
                false, false);  // the churning CLUT rotation lives in the ramp
      break;
    }
    case 8: {  // THE STACK: caged pulsar + strand + the aqua fold
      mana_fill(1, frame, slot, keys, A, stfx, crowd_pm, out);
      mana_lightning(frame, A, out);
      const int32_t ag = mana_fold(frame, slot, keys, A, stfx, kRampAqua, crowd_pm, out);
      if (agit_out) *agit_out = ag;
      break;
    }
    default:
      break;
  }
}


// ---- the centre glow (S5) + the mana ramp/splat machinery -----------------
//
// ONE baked radial CLUT8 sprite (the engine's §4 halo_atmo corona bake) +
// the unratified Lorentzian bloom (the PLASMA profile — tight saturated
// core, a skirt that never quite reaches zero), and one 64-entry ramp per
// ramp id per FRAME. Layers: pre-compose splats before the creature (the
// pools it occludes), post-compose splats after (depth-tested against
// their own projected 1/w, or not — the belly core).

struct GlowAssets {
  zref::star::Sprite8 sprite;   // §4 halo_atmo linear cone
  zref::star::Sprite8 bloom;    // corona_sprite_bloom(24) — the plasma one
  bool baked = false;
};

struct GlowFrame {
  uint8_t pal[64][3];  // built once per frame from the knob colours
};

inline void glow_bake(GlowAssets& g) {
  if (g.baked) return;
  g.sprite = zref::star::corona_sprite(0);  // §4 halo_atmo profile
  g.bloom = zref::star::corona_sprite_bloom(24);
  g.baked = true;
}

/** 64-entry ramp: lo -> mid over [0,32), mid -> hi over [32,64). [0] stays
 *  black (the additive identity the corona bake's exterior relies on).
 *  `rot` rotates indices 1..63 (the boil's churn — zero pixel cost). */
inline void glow_build_ramp(GlowFrame& f, const uint8_t lo[3], const uint8_t mid[3],
                            const uint8_t hi[3], int gain_pm, int rot = 0) {
  for (int i = 0; i < 64; ++i) {
    const int j = i == 0 ? 0 : 1 + (i - 1 + rot) % 63;
    const uint8_t* a = j < 32 ? lo : mid;
    const uint8_t* b = j < 32 ? mid : hi;
    const int t = (j & 31) * 2 + 1;  // 1..63 of 64
    for (int c = 0; c < 3; ++c) {
      int v = (a[c] * (64 - t) + b[c] * t) / 64;
      v = v * gain_pm / 1000;
      if (v > 255) v = 255;
      f.pal[i][c] = static_cast<uint8_t>(v);
    }
  }
  f.pal[0][0] = f.pal[0][1] = f.pal[0][2] = 0;  // additive identity
}

/** Build the frame's mana ramps. `frame` drives the boil's counter-rotating
 *  CLUT churn. Index by ManaRamp. */
inline void mana_build_ramps(GlowFrame ramps[kRampCount], uint32_t frame) {
  constexpr uint8_t kBlack[3] = {0, 0, 0};
  glow_build_ramp(ramps[kRampGlow], kGlowLo, kGlowMid, kGlowHi, 1000);
  const int rot = static_cast<int>((frame / kBoilRotDiv) % 63);
  glow_build_ramp(ramps[kRampBlue], kBlack, kManaBlueMid, kManaBlueHi, 1000, rot);
  glow_build_ramp(ramps[kRampViolet], kBlack, kManaVioletMid, kManaVioletHi, 1000,
                  63 - rot);
  glow_build_ramp(ramps[kRampGold], kBlack, kManaGoldMid, kManaGoldHi, 1000);
  glow_build_ramp(ramps[kRampCyan], kBlack, kManaCyanMid, kManaCyanHi, 1000);
  glow_build_ramp(ramps[kRampWhite], kBlack, kManaWhiteMid, kManaWhiteHi, 1000);
  glow_build_ramp(ramps[kRampDrip], kManaDripMid, kManaDripMid, kManaDripHi, 1000);
  // D11 SS4, the one rebalance rung for the fold he likes: B >= G at both
  // stops. Default OFF -- the shipped aqua is what he called neat, and this is
  // an experiment row, not a replacement (ZHAO_U02_AQUA_BAL=1).
  glow_build_ramp(ramps[kRampAqua], kBlack,
                  g_u02_aqua_bal ? kManaAquaBalMid : kManaAquaMid,
                  g_u02_aqua_bal ? kManaAquaBalHi : kManaAquaHi, 1000);
  glow_build_ramp(ramps[kRampSeaGreen], kBlack, kManaSeaGreenMid, kManaSeaGreenHi, 1000);
  glow_build_ramp(ramps[kRampDeepBlue], kBlack, kManaDeepBlueMid, kManaDeepBlueHi, 1000);
  // LO == MID on purpose: a flat bright body, not a fade from black.
  glow_build_ramp(ramps[kRampAquaCore], kManaAquaCoreMid, kManaAquaCoreMid,
                  kManaAquaCoreHi, 1000);
  // D10: same LO == MID idiom, and for the same reason -- a surround that
  // fades to black at its rim is a surround that is not there at its rim.
  glow_build_ramp(ramps[kRampStorm], kManaStormMid, kManaStormMid,
                  kManaStormHi, 1000);
  // D11: THE BLUE SHIMMER. Additive, so it starts at black like every other
  // additive family here -- the exterior of a corona sprite must be the
  // additive identity or every blob wears a hard rim (09-ENGINE-GOTCHAS SS11).
  glow_build_ramp(ramps[kRampShimmer], kBlack, kManaShimmerMid,
                  kManaShimmerHi, 1000);
  // D11: the lightning mote's OPAQUE heart. LO == MID, the kRampAquaCore
  // idiom, for the pass-8 reason -- an opaque disc is mostly its ramp's lower
  // half, and a ramp from black gives a hot mote a dark middle.
  glow_build_ramp(ramps[kRampBoltCore], kManaBoltCoreMid, kManaBoltCoreMid,
                  kManaBoltCoreHi, 1000);
}


/** One additive glow splat at canvas (cx,cy), half-size r px, depth-tested
 *  against the given centre depth (Q16.16 1/w), never writing depth.
 *  `bloom` selects the Lorentzian plasma profile; `opaque` writes the ramp
 *  colour instead of adding (the drip's solid read) where the sprite is
 *  bright enough to be a body rather than a fringe. */
inline void glow_splat(uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h,
                       const GlowAssets& g, const GlowFrame& f, int32_t cx, int32_t cy,
                       int32_t r, int32_t centre_d, bool depth_test = true,
                       bool bloom = false, bool opaque = false, bool soft = false) {
  if (r <= 0 || !g.baked) return;
  const zref::star::Sprite8& sp = bloom ? g.bloom : g.sprite;
  const int32_t qx0 = cx - r, qy0 = cy - r;
  int32_t x0 = qx0, y0 = qy0, x1 = cx + r, y1 = cy + r;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > static_cast<int32_t>(w)) x1 = static_cast<int32_t>(w);
  if (y1 > static_cast<int32_t>(h)) y1 = static_cast<int32_t>(h);
  const int64_t wq = 2 * static_cast<int64_t>(r);
  for (int32_t y = y0; y < y1; ++y) {
    const int32_t sy = static_cast<int32_t>((static_cast<int64_t>(y - qy0) * sp.h) / wq);
    for (int32_t x = x0; x < x1; ++x) {
      const size_t idx = static_cast<size_t>(y) * w + x;
      if (depth_test && !(centre_d > depth[idx])) continue;  // occluded by nearer surface
      const int32_t sx = static_cast<int32_t>((static_cast<int64_t>(x - qx0) * sp.w) / wq);
      const uint8_t t = sp.pix[static_cast<size_t>(sy) * sp.w + sx];
      if (t == 0) continue;
      const size_t ri = idx * 3;
      if (opaque) {
        if (t < 20) continue;  // the fringe stays additive-free: hard droplet
        if (soft) {
          // PASS 8: blend by the sprite's own intensity. t runs 1..63 on this
          // bake, so t/63 is the natural alpha and needs no knob.
          const int a = static_cast<int>(t) * 1000 / 63;
          for (int k = 0; k < 3; ++k)
            rgb[ri + k] = static_cast<uint8_t>(
                (static_cast<int>(rgb[ri + k]) * (1000 - a) +
                 static_cast<int>(f.pal[t][k]) * a) / 1000);
          continue;
        }
        rgb[ri] = f.pal[t][0];
        rgb[ri + 1] = f.pal[t][1];
        rgb[ri + 2] = f.pal[t][2];
        continue;
      }
      const auto add = [](uint8_t d, uint8_t s) {
        const int v = d + s;
        return static_cast<uint8_t>(v > 255 ? 255 : v);
      };
      rgb[ri] = add(rgb[ri], f.pal[t][0]);
      rgb[ri + 1] = add(rgb[ri + 1], f.pal[t][1]);
      rgb[ri + 2] = add(rgb[ri + 2], f.pal[t][2]);
    }
  }
}

/** PASS 4 (the reviewer's palette-rebuild note made a fix): a per-frame
 *  cache of gain-scaled ramps. Before this, every splat with gain != 1000
 *  rebuilt a 64x3 palette -- TWICE per splat when the smear is on. */
struct GlowFrameCache {
  uint32_t frame = 0xFFFFFFFFu;
  int n = 0;
  struct E { uint8_t ramp; int16_t gain; int16_t boost; GlowFrame gf; } e[40];
};
inline const GlowFrame& glow_frame_cached(GlowFrameCache& c, uint32_t frame,
                                          const GlowFrame ramps[], uint8_t ramp,
                                          int gain_pm, int boost_pm = 1000) {
  if (c.frame != frame) {
    c.frame = frame;
    c.n = 0;
  }
  for (int i = 0; i < c.n; ++i)
    if (c.e[i].ramp == ramp && c.e[i].gain == gain_pm && c.e[i].boost == boost_pm)
      return c.e[i].gf;
  GlowFrame gf = ramps[ramp];
  const int scale = gain_pm * boost_pm / 1000;
  if (scale != 1000) {
    for (int i = 0; i < 64; ++i)
      for (int ch = 0; ch < 3; ++ch) {
        const int v = gf.pal[i][ch] * scale / 1000;
        gf.pal[i][ch] = static_cast<uint8_t>(v > 255 ? 255 : v);
      }
  }
  if (c.n < 40) {
    c.e[c.n] = GlowFrameCache::E{ramp, static_cast<int16_t>(gain_pm),
                                 static_cast<int16_t>(boost_pm), gf};
    return c.e[c.n++].gf;
  }
  static GlowFrame overflow;
  overflow = gf;
  return overflow;
}

/** DIRECTION 7 §4: posed-root speed (mm this frame) -> composite multiplier.
 *  Clamped to [kSmearSpeedBasePm, 1000]. A pure function so it can be reasoned
 *  about and, if it ever needs to be, tested without a render. */
inline int smear_speed_mul_pm(int32_t speed_mm) {
  if (speed_mm <= 0) return kSmearSpeedBasePm;
  int32_t t = speed_mm * 1000 / kSmearSpeedFullMmPerFrame;
  if (t > 1000) t = 1000;
  return kSmearSpeedBasePm + (1000 - kSmearSpeedBasePm) * t / 1000;
}

/** The smear plane's per-frame decay/glitch update (R6). Call ONCE per
 *  frame before feeding: applies the quantised decay step, the per-cell
 *  retention jitter, and the staggered bounded hard clear. */
inline void smear_update(uint8_t* buf, int32_t* dbuf, uint32_t frame,
                         const SmearPreset& sp) {
  if (sp.gain_pm <= 0) return;
  const bool step = sp.step_frames <= 1 || (frame % static_cast<uint32_t>(sp.step_frames)) == 0;
  for (int i = 0; i < kSmearW * kSmearH; ++i) {
    uint8_t* c = buf + static_cast<size_t>(i) * 3;
    // the bounded hard clear, staggered per cell: PROOF the buffer resets
    // (the remembered depth resets with it; decay leaves depth alone)
    if (sp.hard_clear_frames > 1) {
      const uint32_t hc = fx_hash(0x5EEDC1EAu, static_cast<uint32_t>(i), 3u);
      if ((frame + hc) % static_cast<uint32_t>(sp.hard_clear_frames) == 0) {
        c[0] = c[1] = c[2] = 0;
        dbuf[i] = 0;
        continue;
      }
    }
    if (!step || (c[0] | c[1] | c[2]) == 0) continue;
    const uint32_t hj = fx_hash(0x1177E44Du, static_cast<uint32_t>(i),
                                frame / static_cast<uint32_t>(sp.step_frames > 0 ? sp.step_frames : 1));
    int keep = sp.keep_pm + fx_jit(hj, sp.jitter_pm);
    if (keep < 0) keep = 0;
    if (keep > 1000) keep = 1000;
    for (int k = 0; k < 3; ++k) {
      const int v = c[k];
      int nv = v * keep / 1000;
      if (nv == v && v > 0) nv = v - 1;  // decay always reaches zero
      c[k] = static_cast<uint8_t>(nv);
    }
  }
}

/** Feed one projected splat into the plane (quarter-res, always additive —
 *  the plane remembers EVERYTHING the mana draws, cores included). */
inline void smear_feed(uint8_t* buf, int32_t* dbuf, const GlowAssets& g,
                       const GlowFrame& f, int32_t cx, int32_t cy, int32_t r,
                       int32_t splat_d) {
  if (!g.baked) return;
  const int32_t qx = cx / 4, qy = cy / 4;
  int32_t qr = r / 4;
  if (qr < 1) qr = 1;
  const zref::star::Sprite8& sp = g.sprite;
  const int32_t qx0 = qx - qr, qy0 = qy - qr;
  const int64_t wq = 2 * static_cast<int64_t>(qr);
  for (int32_t y = qy0 < 0 ? 0 : qy0; y < qy + qr && y < kSmearH; ++y) {
    const int32_t sy = static_cast<int32_t>((static_cast<int64_t>(y - qy0) * sp.h) / wq);
    for (int32_t x = qx0 < 0 ? 0 : qx0; x < qx + qr && x < kSmearW; ++x) {
      const int32_t sx = static_cast<int32_t>((static_cast<int64_t>(x - qx0) * sp.w) / wq);
      const uint8_t t = sp.pix[static_cast<size_t>(sy) * sp.w + sx];
      if (t == 0) continue;
      uint8_t* c = buf + (static_cast<size_t>(y) * kSmearW + x) * 3;
      // remember the NEAREST contributing splat depth (largest 1/w)
      int32_t& cd = dbuf[static_cast<size_t>(y) * kSmearW + x];
      if (splat_d > cd) cd = splat_d;
      // HUE-PRESERVING accumulation: the first build let cells saturate
      // all three channels and the trail's centre went white. The add is
      // scaled so no channel passes 208 — the cell keeps the ramp's own
      // colour ratio at any accumulation depth.
      int av[3], am = 0, cm = 0;
      for (int k = 0; k < 3; ++k) {
        av[k] = f.pal[t][k] * kSmearFeedPm / 1000;
        if (av[k] > am) am = av[k];
        if (c[k] > cm) cm = c[k];
      }
      if (am == 0 || cm >= 208) continue;
      const int room = 208 - cm;
      const int sc = am > room ? room * 1000 / am : 1000;
      for (int k = 0; k < 3; ++k) {
        const int v = c[k] + av[k] * sc / 1000;
        c[k] = static_cast<uint8_t>(v > 255 ? 255 : v);
      }
    }
  }
}

/** Composite the plane onto the frame: an OPAQUE-LEANING BLEND at chunky
 *  4x nearest — the quarter-res blocks ARE part of the broken-framebuffer
 *  read, and blending (never adding) is what keeps the blobs' hue solid
 *  over the bright sky (R7 for the trails). PASS 4 (R5): depth-correct via
 *  the per-cell remembered splat depth against the frame's depth buffer —
 *  the owner rejected draw-on-top ("properly hidden whenever the creature
 *  is in front of it"). */
inline void smear_composite(const uint8_t* buf, const int32_t* dbuf, uint8_t* rgb,
                            const int32_t* frame_depth, uint32_t w, uint32_t h,
                            int gain_pm, uint32_t frame = 0, int tear = 0) {
  if (gain_pm <= 0) return;
  // pass 4 (R6): the row tear -- on hashed frames a horizontal band of the
  // plane reads with an x offset. Pure index arithmetic.
  int tear_y0 = -1, tear_y1 = -1, tear_dx = 0;
  if (tear) {
    const uint32_t ht = fx_hash(0x7EA2u, frame / 2u, 0x33u);
    if ((ht % static_cast<uint32_t>(kSmearTear.frames)) < 6u) {
      tear_y0 = static_cast<int>((ht >> 8) % static_cast<uint32_t>(kSmearH - kSmearTear.rows));
      tear_y1 = tear_y0 + kSmearTear.rows;
      tear_dx = 1 + static_cast<int>((ht >> 20) % static_cast<uint32_t>(kSmearTear.cells));
      if (ht & 0x40000000u) tear_dx = -tear_dx;
    }
  }
  for (uint32_t y = 0; y < h; ++y) {
    const int cy = static_cast<int>(y / 4);
    const uint8_t* row = buf + (static_cast<size_t>(cy) * kSmearW) * 3;
    const int32_t* drow = dbuf + static_cast<size_t>(cy) * kSmearW;
    const bool torn = cy >= tear_y0 && cy < tear_y1;
    for (uint32_t x = 0; x < w; ++x) {
      int cxi = static_cast<int>(x / 4);
      if (torn) {
        cxi += tear_dx;
        if (cxi < 0) cxi += kSmearW;
        if (cxi >= kSmearW) cxi -= kSmearW;
      }
      const uint8_t* c = row + static_cast<size_t>(cxi) * 3;
      int m = c[0];
      if (c[1] > m) m = c[1];
      if (c[2] > m) m = c[2];
      if (m < 8) continue;  // fully decayed: gone
      // R5: the depth test — exactly glow_splat's own comparison, at cell
      // granularity. A surface nearer than the remembered splat depth
      // keeps the surface: the creature occludes its own trail.
      const int32_t cell_d = drow[cxi];
      if (!(cell_d > frame_depth[static_cast<size_t>(y) * w + x])) continue;
      int a = m * gain_pm * 6 / 1000;
      if (a > kSmearAlphaMaxPm) a = kSmearAlphaMaxPm;
      uint8_t* px = rgb + (static_cast<size_t>(y) * w + x) * 3;
      int cc[3];
      int cc_sum = 0;
      // hue-preserving vivify: never let a channel clip, scale all three
      const int vivid = m * kSmearVividPm > 255000 ? 255000 / m : kSmearVividPm;
      for (int k = 0; k < 3; ++k) {
        cc[k] = c[k] * vivid / 1000;
        if (cc[k] > 255) cc[k] = 255;
        cc_sum += cc[k];
      }
      const int cc_grey = cc_sum / 3;
      for (int k = 0; k < 3; ++k) {
        int v = (px[k] * (1000 - a) + cc[k] * a) / 1000;
        // PASS 8, the chroma floor (kSmearChromaFloorPm). Signed on purpose:
        // the channels the cell is WEAK in come down. That is the half a lerp
        // cannot do, and it is the half that makes a block read as gas rather
        // than as dirt.
        v += (cc[k] - cc_grey) * kSmearChromaFloorPm / 1000;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        px[k] = static_cast<uint8_t>(v);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// THE MIST PLANE'S THREE-AND-A-HALF FUNCTIONS
// Modelled on the smear's, deliberately NOT shared with them. A shared
// implementation would put the smear -- whose colour pass 8 fixed and whose
// rung-3 trail the owner praised by name -- one edit away from every mist
// tweak, and 09-ENGINE-GOTCHAS 14 is exactly the story of one knob quietly
// serving two features. Two planes, two sets of knobs, two blast radii.
// ---------------------------------------------------------------------------

/** THE HALF: shift the plane by whole cells, so it rides with the creature.
 *  This is the creature-relative mechanism (Direction 6 0-TER option 3, asked
 *  for directly in Direction 7 8). `dx`/`dy` are CELL counts, already scaled
 *  by kMistFollowPm and already accumulated through the caller's residual.
 *  Vacated cells clear rather than wrap: a wrap would drag the trail's own tail
 *  back in on the far side, which reads as a second creature. */
/**
 * PASS 10, 0.1 -- THE FOLLOW STEP, EXTRACTED SO A GATE CAN REACH IT.
 *
 * This arithmetic lived INLINE in zhao_reel.cpp (~3335-3352), and that is
 * precisely why the owner's named "weapon" had no gate: there was nothing to
 * call. `kMistFollowPm = 0` -- the behaviour he rejected in his own words,
 * "leaving stuff hanging in space just looks like a glitch" -- passed every
 * gate on this project.
 *
 * It is moved verbatim, not rewritten. The reel now has no private copy to
 * diverge from, so the gate in manafold_probe.cpp calls THE SAME FUNCTION THE
 * REEL CALLS and cannot be satisfied by a re-implementation of itself.
 *
 * The residual is carried in fixed point because a bob of a third of a cell per
 * frame would otherwise round to zero every frame forever, and a
 * "creature-relative" plane that never actually shifts is a screen-space plane
 * with a better comment.
 *
 * Returns true when a shift was computed (i.e. there was a previous frame to
 * difference against) -- the caller shifts exactly when the inline code did, so
 * the refactor is behaviour-preserving.
 */
struct MistFollowState {
  int32_t prev_px[2] = {0, 0};  // the creature's last screen position
  int32_t res[2] = {0, 0};      // sub-cell shift residual (fx)
  bool valid = false;
};

inline bool mist_follow_step(MistFollowState& st, int32_t bx, int32_t by,
                             int follow_pm, int* dxc, int* dyc) {
  const bool had_prev = st.valid;
  *dxc = 0;
  *dyc = 0;
  if (had_prev) {
    const int32_t mvx = bx - st.prev_px[0];
    const int32_t mvy = by - st.prev_px[1];
    // pixels -> sub-cell fixed point, scaled by the follow fraction
    st.res[0] += static_cast<int32_t>(
        (static_cast<int64_t>(mvx) << kMistShiftFxBits) * follow_pm / 1000 / kMistBlock);
    st.res[1] += static_cast<int32_t>(
        (static_cast<int64_t>(mvy) << kMistShiftFxBits) * follow_pm / 1000 / kMistBlock);
    *dxc = st.res[0] >> kMistShiftFxBits;
    *dyc = st.res[1] >> kMistShiftFxBits;
    st.res[0] -= *dxc << kMistShiftFxBits;
    st.res[1] -= *dyc << kMistShiftFxBits;
  }
  st.prev_px[0] = bx;
  st.prev_px[1] = by;
  st.valid = true;
  return had_prev;
}

inline void mist_shift(uint8_t* buf, int32_t* dbuf, int dx, int dy) {
  if (dx == 0 && dy == 0) return;
  if (dx <= -kMistW || dx >= kMistW || dy <= -kMistH || dy >= kMistH) {
    std::memset(buf, 0, static_cast<size_t>(kMistW) * kMistH * 3);
    std::memset(dbuf, 0, static_cast<size_t>(kMistW) * kMistH * sizeof(int32_t));
    return;
  }
  // Walk in the direction that keeps source ahead of destination, so the copy
  // is safe in place without a scratch buffer.
  const int y_begin = dy > 0 ? kMistH - 1 : 0;
  const int y_end   = dy > 0 ? -1 : kMistH;
  const int y_step  = dy > 0 ? -1 : 1;
  const int x_begin = dx > 0 ? kMistW - 1 : 0;
  const int x_end   = dx > 0 ? -1 : kMistW;
  const int x_step  = dx > 0 ? -1 : 1;
  for (int y = y_begin; y != y_end; y += y_step) {
    const int sy = y - dy;
    for (int x = x_begin; x != x_end; x += x_step) {
      const int sx = x - dx;
      uint8_t* d = buf + (static_cast<size_t>(y) * kMistW + x) * 3;
      int32_t& dd = dbuf[static_cast<size_t>(y) * kMistW + x];
      if (sx < 0 || sx >= kMistW || sy < 0 || sy >= kMistH) {
        d[0] = d[1] = d[2] = 0;
        dd = 0;
        continue;
      }
      const uint8_t* s = buf + (static_cast<size_t>(sy) * kMistW + sx) * 3;
      d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
      dd = dbuf[static_cast<size_t>(sy) * kMistW + sx];
    }
  }
}

/** Per-frame decay. Same glitch grammar as the smear -- quantised step,
 *  per-cell retention jitter, staggered bounded hard clear -- on the mist's
 *  own longer constants. */
inline void mist_update(uint8_t* buf, int32_t* dbuf, uint32_t frame) {
  const bool step = kMistStepFrames <= 1 ||
                    (frame % static_cast<uint32_t>(kMistStepFrames)) == 0;
  for (int i = 0; i < kMistW * kMistH; ++i) {
    uint8_t* c = buf + static_cast<size_t>(i) * 3;
    if (kMistHardClearFrames > 1) {
      const uint32_t hc = fx_hash(0x9A17015Du, static_cast<uint32_t>(i), 5u);
      if ((frame + hc) % static_cast<uint32_t>(kMistHardClearFrames) == 0) {
        c[0] = c[1] = c[2] = 0;
        dbuf[i] = 0;
        continue;
      }
    }
    if (!step || (c[0] | c[1] | c[2]) == 0) continue;
    const uint32_t hj = fx_hash(0x31157EEDu, static_cast<uint32_t>(i),
                                frame / static_cast<uint32_t>(kMistStepFrames));
    int keep = g_u02_mist.keep_pm + fx_jit(hj, kMistJitterPm);
    if (keep < 0) keep = 0;
    if (keep > 1000) keep = 1000;
    for (int k = 0; k < 3; ++k) {
      const int v = c[k];
      int nv = v * keep / 1000;
      if (nv == v && v > 0) nv = v - 1;  // decay always reaches zero
      c[k] = static_cast<uint8_t>(nv);
    }
  }
}

/** Feed one projected splat into the mist, BROAD and soft. Same hue-preserving
 *  accumulation as the smear (a cell that saturates all three channels goes
 *  white, and white mist is the fault pass 8 spent itself on), at the mist's
 *  own lower cell cap so the haze stays thin enough to see through. */
inline void mist_feed(uint8_t* buf, int32_t* dbuf, const GlowAssets& g,
                      const GlowFrame& f, int32_t cx, int32_t cy, int32_t r,
                      int32_t splat_d, int feed_mul_pm) {
  if (!g.baked) return;
  const int32_t qx = cx / kMistBlock, qy = cy / kMistBlock;
  int32_t qr = r / kMistBlock;
  if (qr < 1) qr = 1;
  const zref::star::Sprite8& sp = g.sprite;
  const int32_t qx0 = qx - qr, qy0 = qy - qr;
  const int64_t wq = 2 * static_cast<int64_t>(qr);
  for (int32_t y = qy0 < 0 ? 0 : qy0; y < qy + qr && y < kMistH; ++y) {
    const int32_t sy = static_cast<int32_t>((static_cast<int64_t>(y - qy0) * sp.h) / wq);
    for (int32_t x = qx0 < 0 ? 0 : qx0; x < qx + qr && x < kMistW; ++x) {
      const int32_t sx = static_cast<int32_t>((static_cast<int64_t>(x - qx0) * sp.w) / wq);
      const uint8_t t = sp.pix[static_cast<size_t>(sy) * sp.w + sx];
      if (t == 0) continue;
      uint8_t* c = buf + (static_cast<size_t>(y) * kMistW + x) * 3;
      int32_t& cd = dbuf[static_cast<size_t>(y) * kMistW + x];
      if (splat_d > cd) cd = splat_d;
      int av[3], am = 0, cm = 0;
      for (int k = 0; k < 3; ++k) {
        av[k] = f.pal[t][k] * g_u02_mist.feed_pm / 1000 * feed_mul_pm
                / 1000;
        if (av[k] > am) am = av[k];
        if (c[k] > cm) cm = c[k];
      }
      if (am == 0 || cm >= g_u02_mist.cell_cap_pm) continue;
      const int room = g_u02_mist.cell_cap_pm - cm;
      const int sc = am > room ? room * 1000 / am : 1000;
      for (int k = 0; k < 3; ++k) {
        const int v = c[k] + av[k] * sc / 1000;
        c[k] = static_cast<uint8_t>(v > 255 ? 255 : v);
      }
    }
  }
}

/** Composite the mist onto the frame at chunky kMistBlock nearest, with the
 *  same depth test, hue-preserving vivify and chroma floor the smear uses --
 *  the parts of pass 8's colour work that are LAWS here, not preferences
 *  (09-ENGINE-GOTCHAS 4: additive over bright pink can only whiten, so this
 *  blends; and a plane with no chroma floor takes its colour from the sky). */
/**
 * PASS 10, STAGE A -- THE SPINE: THE MIST COMPOSITES AROUND THE SILHOUETTE.
 *
 * `cover` is the creature's own per-pixel coverage (body + cel ink) for this
 * frame, or nullptr for the pass-9 behaviour. A covered pixel NEVER receives
 * mist.
 *
 * WHY THIS IS THE WHOLE FIX. The only rejection this function used to make was
 * the depth test below -- so every creature pixel whose remembered splat sat
 * nearer got repainted. Measured on the antenna band at `rest` f200, the
 * animal's own skin rotated 134 degrees of hue: RGB 144,46,94 (hue 331,
 * magenta) became 88,136,154 (hue 197, cyan). `sparing` at alpha 200 does it
 * too, which is the proof that NO DENSITY VALUE CAN FIX IT -- the fault is the
 * composite reaching creature pixels at all, not how strongly it lands. Do not
 * let this be traded for a lower alpha.
 *
 * What survives, by construction: the loop window, the sky, the terrain and
 * the trail behind a travelling creature are all NON-creature pixels, so the
 * haze in the pocket and the streak on `hasty`/`fall` are untouched. The gas
 * is outside the animal and the ink line is crisp under it, which is
 * Direction 5 section 3 delivered.
 *
 * The SMEAR is deliberately not given this treatment: its over-creature lerp
 * is part of the praised halo/trail and sits on the reviewer's protected list.
 */
inline void mist_composite(const uint8_t* buf, const int32_t* dbuf, uint8_t* rgb,
                           const int32_t* frame_depth, uint32_t w, uint32_t h,
                           int gain_pm, uint32_t frame, const uint8_t* cover) {
  if (gain_pm <= 0) return;
  if (!mist_excludes_silhouette()) cover = nullptr;  // the A/B, one binary
  int tear_y0 = -1, tear_y1 = -1, tear_dx = 0;
  {
    const uint32_t ht = fx_hash(0x6C057A1Bu, frame / 2u, 0x5Bu);
    if ((ht % static_cast<uint32_t>(kMistTear.frames)) < 6u) {
      tear_y0 = static_cast<int>((ht >> 8) % static_cast<uint32_t>(kMistH - kMistTear.rows));
      tear_y1 = tear_y0 + kMistTear.rows;
      tear_dx = 1 + static_cast<int>((ht >> 20) % static_cast<uint32_t>(kMistTear.cells));
      if (ht & 0x40000000u) tear_dx = -tear_dx;
    }
  }
  for (uint32_t y = 0; y < h; ++y) {
    const int cy = static_cast<int>(y / kMistBlock);
    if (cy >= kMistH) break;
    const uint8_t* row = buf + (static_cast<size_t>(cy) * kMistW) * 3;
    const int32_t* drow = dbuf + static_cast<size_t>(cy) * kMistW;
    const bool torn = cy >= tear_y0 && cy < tear_y1;
    for (uint32_t x = 0; x < w; ++x) {
      int cxi = static_cast<int>(x / kMistBlock);
      if (cxi >= kMistW) break;
      if (torn) {
        cxi += tear_dx;
        if (cxi < 0) cxi += kMistW;
        if (cxi >= kMistW) cxi -= kMistW;
      }
      // STAGE A: the creature's own pixels are not sky. Rejected before
      // anything else is computed for them.
      if (cover != nullptr && cover[static_cast<size_t>(y) * w + x]) continue;
      const uint8_t* c = row + static_cast<size_t>(cxi) * 3;
      int m = c[0];
      if (c[1] > m) m = c[1];
      if (c[2] > m) m = c[2];
      if (m < 6) continue;  // fully decayed: gone
      const int32_t cell_d = drow[cxi];
      if (!(cell_d > frame_depth[static_cast<size_t>(y) * w + x])) continue;
      int a = m * gain_pm * 6 / 1000;
      if (a > g_u02_mist.alpha_max_pm) a = g_u02_mist.alpha_max_pm;
      uint8_t* px = rgb + (static_cast<size_t>(y) * w + x) * 3;
      int cc[3];
      int cc_sum = 0;
      const int vivid = m * g_u02_mist.vivid_pm > 255000 ? 255000 / m
                                                          : g_u02_mist.vivid_pm;
      for (int k = 0; k < 3; ++k) {
        cc[k] = c[k] * vivid / 1000;
        if (cc[k] > 255) cc[k] = 255;
        cc_sum += cc[k];
      }
      const int cc_grey = cc_sum / 3;
      for (int k = 0; k < 3; ++k) {
        int v = (px[k] * (1000 - a) + cc[k] * a) / 1000;
        v += (cc[k] - cc_grey) * g_u02_mist.chroma_floor_pm / 1000;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        px[k] = static_cast<uint8_t>(v);
      }
    }
  }
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_FX_H
