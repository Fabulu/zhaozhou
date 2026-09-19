// MANAFOLD — THE EXPERIMENTAL MANA REEL (Owner Direction 6). LANE-ONLY.
//
// This header ships NOTHING. It exists so the experimental reel can vary the
// FOLD MECHANISM without editing one shipped constant in manafold_art.h or
// manafold_fx.h. Everything here is additive: a variant table, a forked
// fold, a forked antenna-knead layer, and one lab clip.
//
// Direction 6 §2, the instruction that matters most:
//   "I think the mana folding will be the hardest part of the project, so
//    use this experimental part to try out some different ways to do it."
// The shipped mechanism (24 motes at fixed MVC weights over six posed
// antenna anchors) has failed to read TWICE. Ten knob settings would be an
// eleventh failure. So at least half of these variants change the mechanism
// by which a shape is DRAWN, not how brightly it is drawn.
//
// WHY A FORK AND NOT A PATCH. `mana_fold()` is the shipping fold and the
// pass-6 implementer is editing its neighbourhood right now. Forking it into
// `lab_fold()` means (a) the shipping pass is provably unaffected, (b) the
// control variant can call the shipping path VERBATIM so it really is a
// control, and (c) a mechanism can be replaced outright rather than smuggled
// in behind an if. The cost is that a later fix to `mana_fold` does not
// reach this file -- which is correct, because this file is a research
// instrument with a finish date, not a second implementation.

#ifndef ZHAO_REEL_MANAFOLD_LAB_H
#define ZHAO_REEL_MANAFOLD_LAB_H

#include "manafold_fx.h"

namespace u02 {
namespace lab {

// ======================= THE CHOREOGRAPHY (Direction 6 §1) =================
//
// The owner's shot list, and each beat isolates exactly one property:
//   1 STAND      -- the mana's RESTING character (density, colour, how the
//                   strands sit in the loop window). Stationary.
//   2 TRAVERSE   -- the SMEAR. The smear plane is screen-space, so the trail
//                   only separates from the creature when the creature
//                   travels ACROSS FRAME. Camera fixed; a tracking camera
//                   cancels screen motion and kills the trail by
//                   construction (measured: hasty +1.90 px/frame net drift,
//                   channel +0.01).
//   3 FOLD       -- a shape forming, HOLDING long enough to be nameable, and
//                   being kneaded into the next. The hold is where a shape
//                   becomes a name; it is not cut short here.
//
// Keys; frames on screen = 2x. Direction 18 lengthens the comparison reel so
// each large topology morph gets roughly a 4.2 s C2 handoff and each result
// holds long enough to read at native resolution.
constexpr int kLabKeys = 720;
constexpr int kLabStandEndKeys = 60;      // beat 1 ends (frame 120, 2.0 s)
constexpr int kLabTraverseEndKeys = 180;  // beat 2 ends (frame 360, 6.0 s)
constexpr int kLabGatherKeys = 25;        // the opening gather, inside beat 1
constexpr int kLabReleaseKeys = 120;      // 4 s C2 shape/root return to opener
// Direction 18 gives 900 pm of each fold cycle to the transition. In the
// 720-key reel that is roughly 126 keys / 252 presentation frames for
// four-shape rows, retaining a 14-key / 28-frame held read between shapes.
constexpr int kLabKneadFracPm = 900;
constexpr int kLabHoldLeashFollowFrames = 16;  // smooth leash correction, never a snap

// THE TRAVERSE AXIS -- fixed after looking at the first render.
//
// Iteration 1 travelled along world +X for 3.6 m. Measured on those frames:
// a real +1.38 px/frame net drift, 221 px of net traverse (against channel's
// +0.01) -- so the traverse itself worked. But the camera sits at yaw 0x2000
// and +X is HALF LATERAL AND HALF TOWARD THE CAMERA: the creature grew from
// 80 to 116 px wide across the beat (+45%), and finished at x1=358 of 384
// with 26 px of margin. A beat that changes the subject's scale cannot be
// compared with the beat before it, and the fold beat ended jammed against
// the right edge with no room for a shape.
//
// So the traverse now runs PERPENDICULAR to the view: +X with an equal and
// opposite Z, which is pure lateral screen motion at constant depth. And it
// ends at the WORLD ORIGIN (measured to sit within ~9 px of frame centre),
// so the fold beat plays in the middle of the frame with the most room.
// Iteration 2 fixed the axis (width held 133-140 px across all three beats,
// against 80->116 before) and gave +1.03 px/frame with 160 px of net drift.
// It also put the STAND beat half off the left edge -- x0 = 0, x1 = 58 on a
// 137 px creature -- which is the one beat whose whole job is showing the
// mana's resting character. So iteration 3 slides the whole path right: the
// creature stops SHORT of frame centre by kLabTraverseEndMm, which buys the
// stand beat its left margin without shortening the traverse.
// Calibrating this took three renders, and the first two calibrations were
// wrong the SAME way: they read the start cx off a frame whose left edge was
// CLIPPED, so the centroid was biased right and the scale came out ~15% low.
// A clipped bbox does not have a centroid. Iteration 3's two ends, both
// measured unclipped where possible, give 185.5 px per 1474 mm = 0.1259
// px/mm at kLabCamK, and put the world origin at cx 179.
// Target: start cx ~100 (x0 ~31), end cx ~250 (x1 ~319).
constexpr int32_t kLabTraverseMm = 1191;   // lateral distance travelled
constexpr int32_t kLabTraverseEndMm = 562; // where it stops, past the origin
constexpr int32_t kLabTraverseXPm = 707;   // (1,0,-1)/sqrt(2): the perpendicular
constexpr int32_t kLabTraverseZPm = -707;

// The camera. The house u02 camera is 240000; pass 6's architecture DECIDED
// 360000 ("the axis that gains on every count is PIXELS PER CREATURE"), and
// the fold must be judged at the size it will ship at, or a mechanism that
// would have read at 360k gets rejected here at 240k. 340000 is the lab's
// value: nearly all of the pass-6 gain, with enough window left that a real
// traverse fits inside it. This is a LAB constant and ships nothing.
constexpr int32_t kLabCamK = 340000;

// The outline's stroke: thin enough that the shape it draws is thicker than
// the stroke (the mote lesson, inverted -- a 7-10 px mote drawing a 56 px
// figure fused; a 4 px line drawing the same figure cannot).
constexpr int32_t kLabEdgeCoreRPx = 4;
constexpr int32_t kLabEdgeHaloRPx = 8;

// ========================= THE VARIANT TABLE ===============================
//
// Named by MECHANISM, never by number, so the owner picks a direction rather
// than a row (Direction 6 §2).
struct LabVariant {
  const char* name;        // subject suffix: manalab-<name>
  const char* mechanism;   // one line, for the report

  // --- how many elements draw the shape, and how big ---
  int mote_count;          // total motes (shape + wander); 0 = no motes
  int wander_count;        // of mote_count: the odd drifters
  int32_t halo_min_px, halo_max_px;
  int halo_gain_pm;        // the ADDITIVE halo gain. The ceiling is hit by
                           // OVERLAP, not count -- this is the wrong knob to
                           // raise and only `past-the-wall` raises it.
  int32_t cloud_spread_mm; // low-grip relax offsets: BREADTH, the right knob
  int32_t stencil_scale_mm;// the shape's size in the pocket
  bool few_elements;       // draw the shape from its CHARACTERISTIC points
                           // only (vertices, tips, ends) instead of evenly
                           // spaced stations

  // --- draw the EDGE instead of the area ---
  bool edge_strands;       // stamp the shape's OUTLINE with the lightning
                           // strand primitive (already approved, already a
                           // line, already gap-free by construction)
  int edge_core_pm, edge_halo_pm;
  int32_t edge_jitter_mm;

  // --- persistence: let the shape hold still while the creature moves ---
  bool hold_in_world;
  int32_t hold_leash_mm;   // how far the held shape may fall behind before it
                           // is dragged along (it must stay on screen)

  // --- transition emphasis: smooth scatter during one persistent morph ---
  bool snap;                // historical field name/subject slug; never a hard cut
  int32_t snap_scatter_mm;  // extra agitation amplitude during the C2 handoff

  // --- the shape as a HOLE: dark silhouette on a bright plate ---
  bool negative;
  int32_t plate_r_px;
  int plate_gain_pm;
  int void_gain_pm;        // opaque gain: LOW = near-black (gain scales the
                           // palette and `opaque` writes it straight out)

  // --- the lightning ---
  int strand_count;        // 1 = the shipping strand verbatim; >1 adds more

  // --- the timeline ---
  int shape_count;         // how many shapes across the fold beat (2..4)
  uint8_t shapes[4];

  // --- the smear rung (kSmearPresets index) and the colour ---
  int smear_rung;
  uint8_t ramp;
};

// Stencil ids now live in manafold_fx.h (pass 8: the shipping edge needs them).
// This fork uses them from there.

// The four most distinct silhouettes, in the order they read best.
#define LAB_SHAPES_4 4, {kShRing, kShTriangle, kShBar, kShCrescent}
#define LAB_SHAPES_2 2, {kShRing, kShTriangle, 0, 0}

constexpr LabVariant kLabVariants[] = {
    // ---------------------------------------------------------------- 1 --
    // THE CONTROL. The shipping `channel` treatment, unmodified, through the
    // lab choreography. Every other row is compared against this and nothing
    // else. Its mote path is the shipping constants verbatim.
    {"control-channel",
     "CONTROL: the shipping channel -- 24 motes at MVC weights + 1 strand",
     kMoteCount, kWanderCount, kMoteHaloRPxMin, kMoteHaloRPxMax,
     kMoteHaloGainPm, kCloudSpreadMm, kStencilScaleMm, false,
     false, 0, 0, 0,
     false, 0,
     false, 0,
     false, 0, 0, 0,
     1, LAB_SHAPES_4, 1, kRampAqua},

    // ---------------------------------------------------------------- 2 --
    // INTENSITY, done the way the measurement says: BREADTH, not depth.
    // "More lightning, more particles, more smear" with the halo gain HELD
    // -- mana-stack draws 3.2x the density at the identical 22.1% clamp
    // fraction but 21% LESS hue spread, so depth buys nothing but white.
    {"breadth-more",
     "INTENSITY by breadth: 40 motes, wider spread, 2 strands, torn smear",
     40, 5, kMoteHaloRPxMin, kMoteHaloRPxMax,
     kMoteHaloGainPm, 520, 340, false,
     false, 0, 0, 0,
     false, 0,
     false, 0,
     false, 0, 0, 0,
     2, LAB_SHAPES_4, 3, kRampAqua},

    // ---------------------------------------------------------------- 3 --
    // PAST THE WALL, ON PURPOSE AND LABELLED. Everything the measurement
    // says not to do, at once: count trebled, halo gain raised past the
    // ceiling, radii into the merge band, four strands at ~1,330 near-white
    // px each into a 10-15 px pocket, the broken-buffer smear rung. Knowing
    // where the wall is has been worth more here than any single value.
    {"past-the-wall",
     "DELIBERATELY TOO FAR: 72 motes, gain over the ceiling, 4 strands, rung 4",
     72, 8, 12, 16,
     520, 620, 360, false,
     false, 0, 0, 0,
     false, 0,
     false, 0,
     false, 0, 0, 0,
     4, LAB_SHAPES_4, 4, kRampAqua},

    // ---------------------------------------------------------------- 4 --
    // MECHANISM: FEWER, LARGER ELEMENTS. 24 motes at 7-10 px halos into a
    // ~56 px figure fuse into blobs (73-99% of the bright core merges). A
    // triangle made of THREE strong elements may read where twenty-four soft
    // ones cannot -- so the shape is drawn from its CHARACTERISTIC points
    // (vertices, star tips, bar corners), not from evenly spaced stations.
    {"three-stones",
     "MECHANISM: the shape drawn from 3-5 LARGE elements at its own vertices",
     0, 0, 19, 26,
     560, 260, 340, /*few_elements=*/true,
     false, 0, 0, 0,
     false, 0,
     false, 0,
     false, 0, 0, 0,
     1, 4, {kShTriangle, kShBar, kShStar, kShCrescent}, 1, kRampAqua},

    // ---------------------------------------------------------------- 5 --
    // MECHANISM: DRAW THE EDGE, NOT THE AREA. A shape is recognised by its
    // OUTLINE. The lightning strand is already a line primitive, already
    // gap-free by construction (kBoltStampMm under one core radius), and
    // already approved by the owner. So the outline is stamped as strand,
    // and the mote cloud is thinned to a garnish instead of being the shape.
    {"edge-strands",
     "MECHANISM: the shape's OUTLINE stamped with the lightning strand primitive",
     10, 3, kMoteHaloRPxMin, kMoteHaloRPxMax,
     kMoteHaloGainPm, 420, 340, false,
     /*edge_strands=*/true, 420, 300, 26,
     false, 0,
     false, 0,
     false, 0, 0, 0,
     1, LAB_SHAPES_4, 1, kRampAqua},

    // ---------------------------------------------------------------- 6 --
    // MECHANISM: LET THE SHAPE HOLD STILL WHILE THE CREATURE MOVES.
    // Legibility needs persistence; a shape that never stops changing never
    // resolves. During every HOLD the anchor set is FROZEN in world space,
    // so the shape stands there and the creature travels past it -- leashed
    // so it cannot leave frame. This is also the strongest possible read of
    // the traverse beat: the shape and the creature visibly separate.
    {"held-still",
     "MECHANISM: the shape FREEZES in world space through every hold",
     28, 3, kMoteHaloRPxMin, kMoteHaloRPxMax,
     kMoteHaloGainPm, 380, 340, false,
     false, 0, 0, 0,
     /*hold_in_world=*/true, 900,
     false, 0,
     false, 0, 0, 0,
     1, LAB_SHAPES_4, 3, kRampAqua},

    // ---------------------------------------------------------------- 7 --
    // HISTORICAL SNAP STUDY, RETIRED BY DIRECTION 18. The subject slug remains
    // stable for archive links, but the rendered mechanism is now one persistent
    // C2 identity with a stronger smooth scatter through the handoff.
    {"snap-states",
     "MECHANISM: persistent C2 shape handoff with a strong smooth scatter",
     28, 3, kMoteHaloRPxMin, kMoteHaloRPxMax,
     kMoteHaloGainPm, 380, 340, false,
     false, 0, 0, 0,
     false, 0,
     /*snap=*/true, 300,
     false, 0, 0, 0,
     1, LAB_SHAPES_4, 3, kRampAqua},

    // ---------------------------------------------------------------- 8 --
    // MECHANISM: THE SHAPE AS A HOLE. The loop window is a clean empty
    // region. A bright plate fills it and the shape is punched out of it as
    // a DARK silhouette -- the one read on this creature that cannot whiten,
    // because it is subtractive by construction and the whitening element
    // (the strand) is switched off. Needs no new ramp: `opaque` writes the
    // gain-scaled palette straight out, so a low gain IS near-black.
    {"negative-void",
     "MECHANISM: a bright plate in the loop window, the shape punched out DARK",
     22, 0, 13, 17,
     300, 300, 330, false,
     false, 0, 0, 0,
     false, 0,
     false, 0,
     /*negative=*/true, 54, 470, 70,
     0, LAB_SHAPES_4, 0, kRampAqua},

    // ---------------------------------------------------------------- 9 --
    // MECHANISM: FEWER SHAPES, BETTER. Six that read beats twenty that do
    // not. TWO shapes across the whole clip, a 5.8 s hold on the second, and
    // the stencil grown 1.5x so the figure is big enough to have an inside
    // and an outside at native.
    {"two-shapes-long",
     "MECHANISM: only TWO shapes, a 5.8 s hold, stencil grown 1.5x",
     30, 2, kMoteHaloRPxMin, kMoteHaloRPxMax,
     kMoteHaloGainPm, 340, 450, false,
     false, 0, 0, 0,
     false, 0,
     false, 0,
     false, 0, 0, 0,
     1, LAB_SHAPES_2, 1, kRampAqua},

    // --------------------------------------------------------------- 10 --
    // THE SYNTHESIS: outline strands + smooth scatter handoffs + world-held
    // shapes. The slug remains stable; Direction 18 forbids hard replacement.
    {"edge-snap-held",
     "SYNTHESIS: outline strands + smooth scatter morphs + world-held shapes",
     10, 2, kMoteHaloRPxMin, kMoteHaloRPxMax,
     kMoteHaloGainPm, 380, 380, false,
     /*edge_strands=*/true, 430, 300, 24,
     /*hold_in_world=*/true, 900,
     /*snap=*/true, 320,
     false, 0, 0, 0,
     1, LAB_SHAPES_2, 2, kRampAqua},
};
constexpr int kLabVariantCount =
    static_cast<int>(sizeof(kLabVariants) / sizeof(kLabVariants[0]));
constexpr int kLabTraceMaxMotes = 72;
constexpr int kLabPersistentFewCount = 5;

struct LabContinuityTrace {
  FoldPhase fold{};
  int station_count = 0;
  int32_t station[kStencilPts][3]{};
  int edge_link_count = 0;
  int32_t edge_energy_pm = 0;
  int32_t edge_presence_pm[kStencilPts]{};
  int32_t edge_layer_stamp_count[kStencilPts][2]{};  // halo, core
  int32_t edge_layer_energy_pm[kStencilPts][2]{};
  int mote_count = 0;
  uint8_t mote_role[kLabTraceMaxMotes]{};
  int32_t mote_visibility_pm[kLabTraceMaxMotes]{};
  int32_t mote_position[kLabTraceMaxMotes][3]{};
};

// The mana-candidate ids the lab occupies. `mana_fill` owns 0..9; the lab
// takes 100+ so a lab id can never collide with a shipping candidate and the
// call-site branch is a single comparison.
constexpr int kLabCandBase = 100;

// =================== THE LAB TIMELINE (authored, not hashed) ===============
//
// The shipping timeline (`fold_phase`) hashes its segment durations, which is
// right for a bank of clips that must never visibly repeat -- and wrong for a
// comparison reel, where every variant must hit the same beat at the same
// frame or the ten are not comparable. So the lab authors its schedule
// outright, and the antenna layer and the mote system read the SAME function
// in the same key domain (kq4 = key * 16; the fx lane passes frame * 8).
inline FoldPhase lab_phase(const LabVariant& V, int32_t kq4) {
  FoldPhase ph{};
  const int32_t gather_end = kLabGatherKeys * 16;
  const int32_t fold_start = kLabTraverseEndKeys * 16;
  const int32_t release_at = (kLabKeys - kLabReleaseKeys) * 16;
  const int n_trans = V.shape_count > 1 ? V.shape_count - 1 : 1;
  const int32_t cycle = (release_at - fold_start) / n_trans;
  const int32_t knead_len = cycle * kLabKneadFracPm / 1000;

  // beat 3 has ended: ease the whole layer home so the loop seam is clean
  if (kq4 >= release_at) {
    ph.seg = kSegRelease;
    ph.shape_from = V.shapes[V.shape_count - 1];
    ph.shape_to = V.shapes[0];
    const int32_t release_span_q4 = kLabReleaseKeys * 16 - 8;
    int32_t t = (kq4 - release_at) * 1000 /
                (release_span_q4 > 0 ? release_span_q4 : 1);
    if (t > 1000) t = 1000;
    ph.morph_pm = motion_c2_ease(t);
    ph.amp_pm = 1000 - motion_c2_ease(t);
    return ph;
  }
  // beat 1a: the opening gather, rising from zero (matches the release tail,
  // so the always-playing loop carries no pop at the wrap)
  if (kq4 < gather_end) {
    ph.seg = kSegGather;
    ph.shape_from = ph.shape_to = V.shapes[0];
    ph.amp_pm = motion_c2_ease(kq4 * 1000 / gather_end);
    return ph;
  }
  // beats 1b + 2: ONE long hold of the opening shape, across the whole stand
  // and the whole traverse. The resting character and the smear are read
  // against a shape that is not changing -- one variable at a time.
  if (kq4 < fold_start) {
    ph.seg = kSegHold;
    ph.amp_pm = 1000;
    ph.shape_from = ph.shape_to = V.shapes[0];
    return ph;
  }
  // beat 3: n_trans repetitions of [knead -> long hold]
  const int32_t into = kq4 - fold_start;
  int idx = static_cast<int>(into / cycle);
  if (idx >= n_trans) idx = n_trans - 1;
  const int32_t within = into - static_cast<int32_t>(idx) * cycle;
  ph.shape_from = V.shapes[idx];
  ph.shape_to = V.shapes[idx + 1 < V.shape_count ? idx + 1 : idx];
  ph.amp_pm = 1000;
  if (within < knead_len) {
    ph.seg = kSegKnead;
    const int32_t t = within * 1000 / (knead_len > 0 ? knead_len : 1);
    ph.agit_pm = t < 250 ? motion_c2_ease(t * 4)
                         : t > 750 ? motion_c2_ease((1000 - t) * 4) : 1000;
    // Direction 18: all variants, including the labelled historical SNAP rung,
    // traverse one persistent C2 identity. Scatter may change amplitude, never
    // replace the topology on a frame boundary.
    ph.morph_pm = motion_c2_ease(t);
    return ph;
  }
  ph.seg = kSegHold;
  ph.shape_from = ph.shape_to = ph.shape_to;  // the knead has landed
  ph.morph_pm = 0;
  ph.agit_pm = 0;
  return ph;
}

// ================== THE PER-VARIANT STENCIL WEIGHT TABLE ===================
//
// `fold_weights()` bakes ONE table at kStencilScaleMm. A variant that grows
// the stencil needs its own, so the lab builds one per variant, once, from
// the same authored stencils and the same integer MVC.
inline const FoldWeights& lab_weights(int vi) {
  static FoldWeights fw[kLabVariantCount];
  static bool built[kLabVariantCount] = {};
  if (!built[vi]) {
    const int32_t scale = kLabVariants[vi].stencil_scale_mm;
    // PASS 12: the figure count is kFoldStencilCount now, not a literal 6.
    // The lab is a lane-only fork that ships nothing, but it compiles into the
    // reel, so it follows the table it borrows.
    const StencilPt(&st)[kFoldStencilCount][kStencilPts] = fold_stencils();
    for (int sh = 0; sh < kFoldStencilCount; ++sh)
      for (int i = 0; i < kStencilPts; ++i) {
        const int32_t pu =
            kStencilCentreUMm + static_cast<int32_t>(st[sh][i].u_pm) * scale / 1000;
        const int32_t pv =
            kStencilCentreVMm + static_cast<int32_t>(st[sh][i].v_pm) * scale / 1000;
        fold_mvc(pu, pv, fw[vi].w[sh][i]);
      }
    built[vi] = true;
  }
  return fw[vi];
}

// ============ THE SHAPE'S OWN CHARACTERISTIC POINTS AND OUTLINE ============
//
// Two tables that encode what each authored stencil actually IS, so the
// edge and few-element mechanisms draw the shape rather than a sampling of
// it. Both are read straight off the stencil constructions in
// manafold_fx.h::fold_stencils() -- they are documentation of that code, and
// they must be re-derived if it changes.

/** The stations that carry a shape's identity: a triangle's three vertices,
 *  a star's centre and four tips, a bar's four corners. Count in [0]. */
inline const int8_t* lab_few_stations(uint8_t shape, int& n) {
  //                          RING            STAR (centre + 4 tips)
  static const int8_t ring[] = {0, 3, 6, 9, 12, 15};
  static const int8_t star[] = {0, 5, 9, 13, 17};
  static const int8_t bar[] = {0, 8, 9, 17};              // the two passes' ends
  static const int8_t crescent[] = {0, 4, 9, 13, 17};
  static const int8_t triangle[] = {0, 6, 12};            // per = 18/3, vertices
  static const int8_t curl[] = {0, 5, 10, 17};
  switch (shape) {
    case kShStar: n = 5; return star;
    case kShBar: n = 4; return bar;
    case kShCrescent: n = 5; return crescent;
    case kShTriangle: n = 3; return triangle;
    case kShCurl: n = 4; return curl;
    default: n = 6; return ring;
  }
}

/** Does the shape's outline run from station i to station i+1 (mod 18)?
 *  RING and TRIANGLE close; CRESCENT and S-CURL are open arcs; the BAR is
 *  two separate passes so its seam is skipped; the STAR is four spokes, so
 *  segments run only WITHIN an arm and from the centre out. */
inline bool lab_edge_link(uint8_t shape, int i) {
  switch (shape) {
    case kShRing:
    case kShTriangle:
      return true;  // closed: 17 -> 0 included
    case kShCrescent:
    case kShCurl:
      return i < kStencilPts - 1;  // open arc: no wrap
    case kShBar:
      return i < kStencilPts - 1 && i != (kStencilPts / 2) - 1;  // skip the seam
    case kShStar:
      // stations 2..17 are four arms of four, running outward. Link inside an
      // arm only; the centre pair (0,1) links to each arm's innermost.
      if (i < 2) return false;
      return ((i - 2) % 4) != 3 && i < kStencilPts - 1;
    default:
      return false;
  }
}

// ============================ THE LAB FOLD =================================

/** Per-conduit lab state. Deterministic: reset at frame 0. Holds only what
 *  the forked mechanisms need beyond FoldState. */
struct LabState {
  bool holding = false;      // a HOLD segment is in progress
  int32_t held[6][3] = {};   // the anchor set frozen at that hold's first frame
};
inline LabState& lab_state(int conduit) {
  static LabState s[3];
  return s[conduit < 0 ? 0 : (conduit > 2 ? 2 : conduit)];
}

/** THE EDGE STAMPER. `bolt_stamp` hard-codes a kRampWhite core, which is
 *  right for lightning and wrong for an outline: measured on the first lab
 *  render, `edge-strands` put 366 near-white px on screen and its saturation
 *  fell to 108.9 against the control's 142.1. A white outline reads as a
 *  glitch; an AQUA outline reads as mana that has been folded into a shape.
 *  So the edge is stamped in the variant's own ramp, and its core carries NO
 *  depth test -- an outline chopped into beads wherever it passes behind the
 *  antenna is not an outline (the pulsar-core law, already on the record).
 *  The halo stays depth-tested so the line still sits in the world. */
inline void lab_edge_stamp(std::vector<ManaSplat>& out, const int32_t pts[][3],
                           int segs, uint8_t ramp, int core_pm, int halo_pm,
                           int32_t core_r, int32_t halo_r, uint32_t frame,
                           int edge_id, int32_t* stamp_count = nullptr,
                           int32_t* halo_energy_pm = nullptr,
                           int32_t* core_energy_pm = nullptr) {
  const int max_count = segs * 24;
  for (int i = 0; i < segs; ++i) {
    int n = path_segment_stamp_count(pts[i], pts[i + 1], kBoltStampMm, 24);
    if (g_u02_fx_continuity_fault == FxContinuityFault::kStampCountPop &&
        edge_id == 0 && i == 0 && (frame & 1u) == 0u)
      n = 24;
    if (stamp_count != nullptr) *stamp_count += n;
    if (halo_energy_pm != nullptr)
      *halo_energy_pm += stamp_energy_pm(n, halo_pm, halo_r, max_count,
                                         1200, halo_r);
    if (core_energy_pm != nullptr)
      *core_energy_pm += stamp_energy_pm(n, core_pm, core_r, max_count,
                                         1200, core_r);
    for (int t = 0; t < n; ++t) {
      const int32_t x = lerp32(pts[i][0], pts[i + 1][0], t, n);
      const int32_t y = lerp32(pts[i][1], pts[i + 1][1], t, n);
      const int32_t z = lerp32(pts[i][2], pts[i + 1][2], t, n);
      mana_push(out, x, y, z, halo_r, ramp, halo_pm, true, false);
      mana_push(out, x, y, z, core_r, ramp, core_pm, false, false);
    }
  }
}

/** Extra lightning strands beyond the shipping one. The shipping strand is
 *  drawn by `mana_lightning()` UNCHANGED so the control is a real control;
 *  this only adds breadth. Each strand is ~1,330 px of near-white into a
 *  10-15 px pocket -- the whitening element, and the reason `past-the-wall`
 *  takes four of them. */
inline void lab_extra_strands(uint32_t frame, uint32_t slot, int keys,
                              const FxAnchors& A, int extra,
                              std::vector<ManaSplat>& out,
                              FxContinuityTrace* trace = nullptr) {
  if (extra <= 0) return;
  for (int i = 1; i <= extra && i < kFxTraceMaxStrands; ++i) {
    const FreeLightningPath path = free_lightning_path(frame, slot, keys, i, A);
    if (trace != nullptr) {
      for (int p = 0; p <= kBoltSegs; ++p) {
        const int id = i * (kBoltSegs + 1) + p;
        for (int k = 0; k < 3; ++k)
          trace->free_point[id][k] = path.pts[p][k];
      }
      trace->free_point_count = (i + 1) * (kBoltSegs + 1);
    }
    const int32_t core_gain = kBoltCoreGainPm * path.flick_pm / 1000;
    const int32_t halo_gain = kBoltHaloGainPm * path.flick_pm / 1000;
    if (trace != nullptr && i < kFxTraceMaxStrands) {
      const int count = path_stamp_count(path.pts, kBoltSegs, kBoltStampMm, 24);
      trace->free_strand_stamp_count[i][0] = count;
      trace->free_strand_stamp_count[i][1] = count;
      constexpr int kMaxCount = kBoltSegs * 24;
      trace->free_strand_energy_pm[i][0] =
          stamp_energy_pm(count, halo_gain, kBoltHaloRPx,
                          kMaxCount, 1200, kBoltHaloRPx);
      trace->free_strand_energy_pm[i][1] =
          stamp_energy_pm(count, core_gain, kBoltCoreRPx,
                          kMaxCount, 1200, kBoltCoreRPx);
    }
    bolt_stamp(out, path.pts, kBoltSegs, core_gain, halo_gain);
  }
}

/** THE FORKED FOLD. Same law as `mana_fold` -- every element position is a
 *  fixed-weight sum over the POSED anchors, and no proximity, collision or
 *  distance term exists anywhere (manafold_clips.h:501) -- with the drawing
 *  MECHANISM swapped per variant. Returns agitation 0..1000. */
inline int32_t lab_fold(int vi, uint32_t frame, int conduit, const FxAnchors& A,
                        FoldState& stfx, std::vector<ManaSplat>& out,
                        LabContinuityTrace* trace = nullptr) {
  const LabVariant& V = kLabVariants[vi];
  LabState& L = lab_state(conduit);
  const int32_t* anchors[6] = {A.junction_f, A.neck, A.hinge_a,
                               A.hinge_b,    A.hinge_c, A.junction_b};

  // ---- rig-derived scalars (joint state ONLY) ----------------------------
  int32_t rel[6][3];
  for (int i = 0; i < 6; ++i)
    for (int k = 0; k < 3; ++k) rel[i][k] = anchors[i][k] - A.body[k];
  if (frame == 0) {
    stfx = FoldState{};
    L = LabState{};
  }
  if (!stfx.init) {
    for (int i = 0; i < 6; ++i)
      for (int k = 0; k < 3; ++k) stfx.prev_rel[i][k] = rel[i][k];
    for (auto& v : stfx.dragbuf) v[0] = v[1] = v[2] = 0;
    stfx.knead_smooth = 0;
    stfx.drag_idx = 0;
    stfx.init = true;
    L.holding = false;
  }
  int64_t raw = 0;
  for (int i = 0; i < 6; ++i)
    for (int k = 0; k < 3; ++k) {
      const int64_t dd = rel[i][k] - stfx.prev_rel[i][k];
      raw += dd < 0 ? -dd : dd;
    }
  const int32_t raw_mm = static_cast<int32_t>((raw * 1000) >> 16);
  stfx.knead_smooth += (raw_mm - stfx.knead_smooth) / 4;
  if (stfx.knead_slow == 0) stfx.knead_slow = raw_mm;
  stfx.knead_slow += (raw_mm - stfx.knead_slow) / 64;
  const int32_t excess = stfx.knead_smooth - stfx.knead_slow * 12 / 10;
  const int32_t agit = excess <= 0 ? 0
                       : excess >= kKneadVelRefMm ? 1000
                                                  : excess * 1000 / kKneadVelRefMm;
  stfx.dragbuf[stfx.drag_idx & 7][0] = rel[3][0] - stfx.prev_rel[3][0];
  stfx.dragbuf[stfx.drag_idx & 7][1] = rel[3][1] - stfx.prev_rel[3][1];
  stfx.dragbuf[stfx.drag_idx & 7][2] = rel[3][2] - stfx.prev_rel[3][2];
  ++stfx.drag_idx;
  for (int i = 0; i < 6; ++i)
    for (int k = 0; k < 3; ++k) stfx.prev_rel[i][k] = rel[i][k];

  // GRIP: the anchor polygon's area against its rest area
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
    live_area2 = isqrt64(sx * sx + sy * sy + sz * sz);
  }
  static const int64_t rest_area2 = [] {
    int64_t cx = 0, cy = 0, cz = 0;
    for (int i = 0; i < 6; ++i) {
      cx += kFoldAnchorRestMm[i][0]; cy += kFoldAnchorRestMm[i][1];
      cz += kFoldAnchorRestMm[i][2];
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

  const FoldPhase ph = lab_phase(V, static_cast<int32_t>(frame) * 8);
  // `lab_phase` already authors gather/release with one C2 envelope. Consume it
  // directly here: a second ease compresses the transition and moves its
  // acceleration/jerk peaks away from the authored timeline.
  coh = lerp32(kCohBasePm, coh, ph.amp_pm, 1000);
  if (trace != nullptr) trace->fold = ph;
  g_u02_fold_release_pm = ph.seg == kSegRelease ? ph.amp_pm : 1000;

  // ---- MECHANISM: hold the shape still in world space --------------------
  // On the first frame of a HOLD, freeze the anchor set. The shape then
  // stands where it was folded and the creature travels past it. LEASHED:
  // once the live ring has walked further than hold_leash_mm from the frozen
  // one, the frozen set is dragged along, so the shape can lag hard but can
  // never leave the frame (an off-screen shape teaches nothing).
  const int32_t* pos_anchors[6];
  for (int i = 0; i < 6; ++i) pos_anchors[i] = anchors[i];
  int32_t leashed[6][3];
  int32_t live_follow_pm = 1000;
  if (V.hold_in_world) {
    if (ph.seg == kSegHold) {
      live_follow_pm = 0;
      if (!L.holding) {
        for (int i = 0; i < 6; ++i)
          for (int k = 0; k < 3; ++k) L.held[i][k] = anchors[i][k];
        L.holding = true;
      }
      // the leash, applied to the whole frozen set as one rigid offset
      int32_t hc[3] = {0, 0, 0};
      for (int i = 0; i < 6; ++i)
        for (int k = 0; k < 3; ++k) hc[k] += L.held[i][k] / 6;
      int32_t d[3];
      for (int k = 0; k < 3; ++k) d[k] = A.ring[k] - hc[k];
      const int64_t mag = isqrt64(static_cast<int64_t>(d[0]) * d[0] +
                                  static_cast<int64_t>(d[1]) * d[1] +
                                  static_cast<int64_t>(d[2]) * d[2]);
      const int64_t leash = fxu(V.hold_leash_mm);
      if (mag > leash && mag > 0) {
        const int64_t pull = mag - leash;
        for (int i = 0; i < 6; ++i)
          for (int k = 0; k < 3; ++k)
            L.held[i][k] += static_cast<int32_t>(
                static_cast<int64_t>(d[k]) * pull /
                (mag * kLabHoldLeashFollowFrames));
      }
      for (int i = 0; i < 6; ++i) {
        for (int k = 0; k < 3; ++k) leashed[i][k] = L.held[i][k];
        pos_anchors[i] = leashed[i];
      }
    } else if (L.holding) {
      // Direction 18: a world-held figure cannot be dropped back onto the live
      // rig in one frame. The following knead/release already owns a C2 morph;
      // use that same envelope to return every anchor to the carrier.
      const int32_t handoff_pm =
          (ph.seg == kSegKnead || ph.seg == kSegRelease) ? ph.morph_pm : 1000;
      live_follow_pm = handoff_pm;
      if (handoff_pm < 1000) {
        for (int i = 0; i < 6; ++i) {
          for (int k = 0; k < 3; ++k)
            leashed[i][k] = lerp32(L.held[i][k], anchors[i][k], handoff_pm, 1000);
          pos_anchors[i] = leashed[i];
        }
      } else {
        L.holding = false;
      }
    }
  }
  const bool held_ring_now = V.hold_in_world && L.holding;

  const FoldWeights& fw = lab_weights(vi);

  // ---- the station -> world sum ------------------------------------------
  // The one law: a fixed-weight sum over the POSED anchors. Everything below
  // only chooses WHICH stations are drawn and WITH WHAT.
  const auto bary = [&](uint8_t shape_id, int stn, int32_t q[3]) {
    const int32_t* wt = fw.w[shape_id][stn];
    for (int k = 0; k < 3; ++k) {
      int64_t acc = 0;
      for (int i = 0; i < 6; ++i) acc += static_cast<int64_t>(wt[i]) * pos_anchors[i][k];
      q[k] = static_cast<int32_t>(acc >> 12);
    }
  };
  // the pocket centre the stencil offsets rotate about; frozen with the rest
  int32_t ring[3];
  if (held_ring_now) {
    for (int k = 0; k < 3; ++k) {
      ring[k] = 0;
      for (int i = 0; i < 6; ++i) ring[k] += pos_anchors[i][k] / 6;
    }
  } else {
    for (int k = 0; k < 3; ++k) ring[k] = A.ring[k];
  }

  /** One station's drawn world position, with the authored face yaw applied
   *  to the OFFSET (never the anchors: the folding law stays a rig sum). */
  const auto station_pos = [&](int stn, int32_t P[3]) {
    int32_t Pf[3], Pt[3];
    const int source_stn = fold_canonical_station(ph.shape_from, stn);
    const int dest_stn = fold_canonical_station(ph.shape_to, stn);
    bary(ph.shape_from, source_stn, Pf);
    bary(ph.shape_to, dest_stn, Pt);
    // `lab_phase` already authors the shared C2 envelope. Applying the easing a
    // second time here made the lab use a different transition law.
    const int32_t mp = ph.morph_pm;
    for (int k = 0; k < 3; ++k) P[k] = lerp32(Pf[k], Pt[k], mp, 1000);
    const int32_t sn = fx_sin16(static_cast<uint32_t>(kStencilFaceYawA16));
    const int32_t cs = fx_cos16(static_cast<uint32_t>(kStencilFaceYawA16));
    const int32_t ox = P[0] - ring[0], oz = P[2] - ring[2];
    P[0] = ring[0] + static_cast<int32_t>(((static_cast<int64_t>(ox) * cs) >> 16) -
                                          ((static_cast<int64_t>(oz) * sn) >> 16));
    P[2] = ring[2] + static_cast<int32_t>(((static_cast<int64_t>(ox) * sn) >> 16) +
                                          ((static_cast<int64_t>(oz) * cs) >> 16));
  };

  // ---- MECHANISM: the shape as a HOLE ------------------------------------
  // Drawn FIRST so the dark shape lands on top of it: post splats composite
  // in vector order.
  if (V.negative) {
    // PRE: the plate composites BEFORE the creature, so the creature and its
    // antenna occlude it. Looked at on the first render, a post plate was a
    // big soft fog lying over the whole antenna -- it read as weather, not as
    // a surface. Behind the creature it does the one job it has: it makes the
    // loop window a clean bright region for a dark shape to sit in.
    mana_push(out, ring[0], ring[1], ring[2], V.plate_r_px, V.ramp,
              V.plate_gain_pm, true, /*pre=*/true);
    mana_push(out, ring[0], ring[1], ring[2], V.plate_r_px * 620 / 1000, V.ramp,
              1000, true, /*pre=*/true, /*opaque=*/true);
  }

  // Evaluate the persistent station domain for every variant, even when its
  // drawing mechanism omits edge strands. The production gate consumes this
  // exact position field rather than reconstructing a private proxy.
  int32_t S[kStencilPts][3];
  for (int i = 0; i < kStencilPts; ++i) {
    station_pos(i, S[i]);
    if (trace != nullptr)
      for (int k = 0; k < 3; ++k) trace->station[i][k] = S[i][k];
  }
  if (trace != nullptr) trace->station_count = kStencilPts;

  // ---- MECHANISM: draw the EDGE, not the area ----------------------------
  if (V.edge_strands) {
    int32_t pts[5][3];
    for (int i = 0; i < kStencilPts; ++i) {
      const bool source_link = lab_edge_link(
          ph.shape_from, fold_canonical_station(ph.shape_from, i));
      const bool dest_link = lab_edge_link(
          ph.shape_to, fold_canonical_station(ph.shape_to, i));
      if (!source_link && !dest_link) continue;
      const int32_t edge_pm = source_link == dest_link
                                  ? (source_link ? 1000 : 0)
                                  : (source_link ? 1000 - ph.morph_pm
                                                 : ph.morph_pm);
      if (trace != nullptr) trace->edge_presence_pm[i] = edge_pm;
      if (edge_pm <= 0) continue;
      if (trace != nullptr) {
        ++trace->edge_link_count;
        trace->edge_energy_pm += edge_pm;
      }
      const int j = (i + 1) % kStencilPts;
      bolt_path_morph(S[i], S[j], 4, frame,
                      static_cast<uint32_t>(vi * 101 + i + 1), kLabKeys,
                      kFoldEdgeMorphFrames,
                      kBoltSeed ^ (0x5EDu * (i + 1)), pts,
                      V.edge_jitter_mm);
      int32_t stamp_count = 0, halo_energy = 0, core_energy = 0;
      lab_edge_stamp(out, pts, 4, V.ramp,
                     V.edge_core_pm * edge_pm / 1000,
                     V.edge_halo_pm * edge_pm / 1000,
                     kLabEdgeCoreRPx, kLabEdgeHaloRPx, frame, i,
                     &stamp_count, &halo_energy, &core_energy);
      if (trace != nullptr) {
        trace->edge_layer_stamp_count[i][0] = stamp_count;
        trace->edge_layer_stamp_count[i][1] = stamp_count;
        trace->edge_layer_energy_pm[i][0] = halo_energy;
        trace->edge_layer_energy_pm[i][1] = core_energy;
      }
    }
  }

  // ---- the elements ------------------------------------------------------
  int n_motes = V.few_elements ? kLabPersistentFewCount : V.mote_count;
  int n_wander = V.few_elements ? 0 : V.wander_count;
  int n_shape = n_motes - n_wander;
  if (n_shape < 0) n_shape = 0;
  n_motes = continuity_mote_count(frame, n_motes);
  if (n_motes > kLabTraceMaxMotes) n_motes = kLabTraceMaxMotes;
  if (n_shape > n_motes) n_shape = n_motes;
  if (trace != nullptr) trace->mote_count = n_motes;

  // Stable canonical IDs for the few-element mechanism. Every shape uses the
  // same five entities; canonical station mapping moves them continuously and
  // may co-locate IDs at a triangle corner rather than popping count.
  static constexpr int kFewStation[kLabPersistentFewCount] = {0, 4, 8, 12, 16};

  for (int m = 0; m < n_motes; ++m) {
    const uint32_t hm = fx_hash(0xF01Du, static_cast<uint32_t>(m), 0xA7u);
    const uint8_t normal_role = m >= n_shape ? 1u : 0u;
    const uint8_t mote_role = continuity_mote_role(frame, m, normal_role);
    int32_t P[3];
    if (mote_role == 1u) {
      // the wanderers: slow hashed walks that leave the pocket, quantised to
      // whole cycles over the clip so the loop seam does not pop
      const int per = 240 + static_cast<int>(hm % 200u);
      const uint32_t ph1 = hm & 0xFFFFu;
      const int32_t r1 =
          fxu(kWanderEscapeMm * (600 + static_cast<int32_t>((hm >> 4) % 400u)) / 1000);
      const int32_t r2 = r1 * 2 / 3;
      const int ft = kLabKeys * 2;
      int cycles = (ft + per / 2) / per;
      if (cycles < 1) cycles = 1;
      const uint32_t fmod = frame % static_cast<uint32_t>(ft);
      const uint32_t th = static_cast<uint32_t>(
          (static_cast<uint64_t>(fmod) * static_cast<uint32_t>(cycles) << 16) /
          static_cast<uint32_t>(ft));
      P[0] = A.ring[0] + static_cast<int32_t>((static_cast<int64_t>(r1) * fx_cos16(th + ph1)) >> 16);
      P[1] = A.ring[1] + static_cast<int32_t>((static_cast<int64_t>(r2) *
                                               fx_sin16(th + (ph1 ^ 0x9A00u))) >> 16);
      P[2] = A.ring[2] + static_cast<int32_t>((static_cast<int64_t>(r2) *
                                               fx_sin16(th + ph1 + 0x4000u)) >> 16);
    } else {
      const int stn = V.few_elements
                          ? kFewStation[m % kLabPersistentFewCount]
                          : m * kStencilPts / (n_shape > 0 ? n_shape : 1);
      int32_t Pst[3];
      station_pos(stn, Pst);
      // the relax cloud + one slow consistent orbit, both quantised to whole
      // cycles over the clip. A few-element variant gets NEITHER: three big
      // stones that wander are three big stones, not a triangle.
      int32_t orb[3] = {0, 0, 0}, cloud_off[3] = {0, 0, 0};
      if (!V.few_elements) {
        const int per = kMoteOrbitPeriodMinF +
            static_cast<int>((hm >> 8) % static_cast<uint32_t>(kMoteOrbitPeriodMaxF -
                                                               kMoteOrbitPeriodMinF));
        const int ft = kLabKeys * 2;
        int cycles = (ft + per / 2) / per;
        if (cycles < 1) cycles = 1;
        const uint32_t th = static_cast<uint32_t>(
            (static_cast<uint64_t>(frame % static_cast<uint32_t>(ft)) *
             static_cast<uint32_t>(cycles) << 16) / static_cast<uint32_t>(ft));
        const int32_t orad = fxu(kMoteOrbitRMinMm +
            static_cast<int32_t>((hm >> 16) % static_cast<uint32_t>(kMoteOrbitRMaxMm -
                                                                    kMoteOrbitRMinMm)));
        const uint32_t oph = (hm >> 3) & 0xFFFFu;
        orb[0] = static_cast<int32_t>((static_cast<int64_t>(orad) * fx_cos16(th + oph)) >> 16);
        orb[1] = static_cast<int32_t>((static_cast<int64_t>(orad * 3 / 4) * fx_sin16(th + oph)) >> 16);
        orb[2] = static_cast<int32_t>((static_cast<int64_t>(orad / 2) *
                                       fx_sin16(th + oph + 0x3800u)) >> 16);
        cloud_off[0] = fxu(fx_jit(hm, V.cloud_spread_mm));
        cloud_off[1] = fxu(fx_jit(hm >> 7, V.cloud_spread_mm * 3 / 4));
        cloud_off[2] = fxu(fx_jit(hm >> 13, V.cloud_spread_mm / 2));
      }
      for (int k = 0; k < 3; ++k) {
        const int32_t cloud = Pst[k] + cloud_off[k] + orb[k];
        const int32_t tight = Pst[k] + orb[k] / 4;
        P[k] = lerp32(cloud, tight, coh, 1000);
      }
    }
    // Direction 18: even the historical SNAP rung retains persistent particle
    // IDs and smooth paths; its larger scatter is amplitude, never a reseed.
    const int32_t jit_mm = V.snap ? V.snap_scatter_mm * ph.agit_pm / 1000
                                  : kKneadJitterMm * agit / 1000;
    if (jit_mm > 0) {
      int32_t jo[3];
      mote_knead_offset(frame, kLabKeys, m, jit_mm, jo);
      for (int k = 0; k < 3; ++k) P[k] += jo[k];
    }
    // DRAG: the lagged pull along the antenna's sweep (the iron-filings
    // read), clamped by magnitude. A world-frozen shape takes NO drag -- it
    // is standing still, and a drag term would be it standing still while
    // being pulled, which is nothing.
    if (live_follow_pm > 0) {
      const int lag = kDragLagFrames + static_cast<int>((hm >> 21) % 4u);
      const uint32_t bi = stfx.drag_idx + 8u - static_cast<uint32_t>(lag);
      int32_t dsp[3];
      for (int k = 0; k < 3; ++k) {
        const int64_t v = static_cast<int64_t>(stfx.dragbuf[bi & 7][k]) +
                          stfx.dragbuf[(bi - 1u) & 7][k] + stfx.dragbuf[(bi - 2u) & 7][k];
        dsp[k] = static_cast<int32_t>(
            v * kDragGainPm / 1000 * live_follow_pm / 1000 * ph.amp_pm / 1000);
      }
      const int64_t mag = isqrt64(static_cast<int64_t>(dsp[0]) * dsp[0] +
                                  static_cast<int64_t>(dsp[1]) * dsp[1] +
                                  static_cast<int64_t>(dsp[2]) * dsp[2]);
      const int64_t cap = fxu(kDragMaxMm);
      if (mag > cap && mag > 0)
        for (int k = 0; k < 3; ++k) dsp[k] = static_cast<int32_t>(dsp[k] * cap / mag);
      for (int k = 0; k < 3; ++k) P[k] += dsp[k];
    }
    int32_t visibility_pm = 1000;
    if (g_u02_fx_continuity_fault == FxContinuityFault::kMoteVisibilityPop &&
        m == 0 && (frame & 1u) == 0u)
      visibility_pm = 0;
    if (trace != nullptr) {
      trace->mote_role[m] = mote_role;
      trace->mote_visibility_pm[m] = visibility_pm;
      for (int k = 0; k < 3; ++k) trace->mote_position[m][k] = P[k];
    }
    if (visibility_pm <= 0) continue;
    const int32_t halo = V.halo_min_px +
        static_cast<int32_t>((hm >> 5) %
                             static_cast<uint32_t>(V.halo_max_px - V.halo_min_px + 1));
    if (V.negative) {
      // the shape is a HOLE: one opaque body at a gain low enough to read as
      // dark against the plate. No additive halo -- an additive halo would
      // put light back exactly where the hole is. Drawn PRE, with the plate,
      // so the creature occludes both and the dark shape sits INSIDE the loop
      // window instead of being painted over the antenna.
      mana_push(out, P[0], P[1], P[2], halo, V.ramp,
                V.void_gain_pm * visibility_pm / 1000, true,
                /*pre=*/true, /*opaque=*/true);
    } else if (V.few_elements) {
      // FEWER, LARGER: the first render drew 429 mana px from four 19-26 px
      // stones -- LESS than the 24-mote control's 495. Three of the four were
      // behind the antenna. A shape made of three elements cannot afford to
      // lose one, so the stones' glow carries NO depth test and shines
      // through the arm (the pulsar-core law); the opaque heart stays
      // depth-tested so the stone still sits in the world where it is
      // visible.
      // Iteration 2 on this variant: taking the depth test off the GLOW was
      // not enough -- at gain 300 the shine-through was below the point of
      // being seen, and three of four stones stayed invisible behind the
      // arm (450 mana px, still under the 24-mote control's 495). The stone
      // is now a solid body of mana in FRONT of the antenna, glow and heart
      // both. That is a real authored choice and not a cheat: it is exactly
      // what the caged pulsar's core already does, and a three-element shape
      // that loses an element is not a shape.
      mana_push(out, P[0], P[1], P[2], halo * kMoteCoreOfHaloPm / 1000, V.ramp,
                visibility_pm, false, false, /*opaque=*/true);
      mana_push(out, P[0], P[1], P[2], halo, V.ramp,
                V.halo_gain_pm * visibility_pm / 1000, false, false);
    } else {
      mana_push(out, P[0], P[1], P[2], halo * kMoteCoreOfHaloPm / 1000, V.ramp,
                visibility_pm, true, false, /*opaque=*/true);
      mana_push(out, P[0], P[1], P[2], halo, V.ramp,
                V.halo_gain_pm * visibility_pm / 1000, true, false);
    }
  }
  return agit;
}

/** The lab's `mana_fill`. `cand` is kLabCandBase + variant index. */
inline void lab_fill(int cand, uint32_t frame, int conduit, int keys,
                     const FxAnchors& A, FoldState& stfx,
                     std::vector<ManaSplat>& out, int32_t* agit_out,
                     LabContinuityTrace* lab_trace = nullptr,
                     FxContinuityTrace* fx_trace = nullptr) {
  const int vi = cand - kLabCandBase;
  if (vi < 0 || vi >= kLabVariantCount) return;
  const LabVariant& V = kLabVariants[vi];
  const int32_t ag = lab_fold(vi, frame, conduit, A, stfx, out, lab_trace);
  if (agit_out) *agit_out = ag;
  // the shipping strand VERBATIM (so the control really is the control),
  // then breadth on top; both share the production continuity trace.
  const uint32_t lightning_slot = 15u + static_cast<uint32_t>(conduit) * 37u;
  if (V.strand_count >= 1)
    mana_lightning(frame, lightning_slot, keys, A, out, 1000, fx_trace);
  lab_extra_strands(frame, lightning_slot, keys, A, V.strand_count - 1, out,
                    fx_trace);
}

// ==================== THE LAB CLIP (slot 15) ===============================

/** The forked antenna layer: identical to `antenna_knead` except that it
 *  reads the LAB timeline, so the bones and the motes agree about which beat
 *  it is. Gain is the lab's own knob, not a bank entry. */
constexpr int kLabKneadGainPm = 900;  // channel's own value: the clip that
                                      // reads best has the second-highest
                                      // knead gain in the bank
inline void lab_antenna_knead(Rig& g, const LabVariant& V, int f) {
  const FoldPhase ph = lab_phase(V, f * 16);
  const auto a = [&](int32_t base, int32_t env_pm) {
    return static_cast<int32_t>(static_cast<int64_t>(base) * env_pm / 1000 *
                                kLabKneadGainPm / 1000);
  };
  const int32_t grip = ph.amp_pm;
  // Direction 18: the old hold-only tremor entered/exited at an arbitrary sine
  // value. The lab uses the same continuous grip/wag vocabulary as production;
  // do not restore a private hard-gated shake here.
  g.q[kBJunctionF] = quat_mul(g.q[kBJunctionF], quat_z(a(kKneadGripJfA16, grip)));
  g.q[kBNeck] = quat_mul(g.q[kBNeck], quat_z(a(kKneadGripNeckA16, grip)));
  g.q[kBHingeA] = quat_mul(g.q[kBHingeA], quat_z(a(kKneadGripAA16, grip)));
  g.q[kBHingeB] = quat_mul(g.q[kBHingeB], quat_z(a(kKneadGripBA16, grip)));
  g.q[kBHingeC] = quat_mul(g.q[kBHingeC], quat_z(a(kKneadGripCA16, grip)));
  if (ph.agit_pm > 0) {
    const int cyc = kLabKeys / kKneadWagPeriodKeys;
    const int32_t w1 = sinp(f, kLabKeys, cyc);
    const int32_t w2 = sinp(f, kLabKeys, cyc, 0x4000);
    g.q[kBJunctionF] = quat_mul(g.q[kBJunctionF], quat_z(static_cast<int32_t>(
        (static_cast<int64_t>(a(kKneadWagJfA16, ph.agit_pm)) * w1) >> 16)));
    g.q[kBHingeC] = quat_mul(g.q[kBHingeC], quat_z(-static_cast<int32_t>(
        (static_cast<int64_t>(a(kKneadWagCA16, ph.agit_pm)) * w1) >> 16)));
    g.q[kBNeck] = quat_mul(g.q[kBNeck], quat_x(static_cast<int32_t>(
        (static_cast<int64_t>(a(kKneadWagNeckA16, ph.agit_pm)) * w2) >> 16)));
    g.q[kBHingeB] = quat_mul(g.q[kBHingeB], quat_z(static_cast<int32_t>(
        (static_cast<int64_t>(a(kKneadWagBA16, ph.agit_pm)) * w2) >> 16)));
    g.q[kBLoopBase2] = quat_mul(g.q[kBLoopBase2], quat_z(-static_cast<int32_t>(
        (static_cast<int64_t>(a(kKneadWagB2A16, ph.agit_pm)) * w2) >> 16)));
  }
}

/** THE LAB CLIP, slot 15. One clip, ten mana configurations -- the sameness
 *  everywhere else is what makes the ten comparable.
 *
 *  The traverse is authored as a straight X translation with an eased start
 *  and stop, giving REAL NET DRIFT rather than an oscillation that averages
 *  to zero. The camera does not track (the subject sets no orbit), because a
 *  tracking camera cancels screen-space motion and the smear plane is
 *  screen-space: the trail would be killed by construction. */
inline zc::Clip build_manalab() {
  const int K = kLabKeys;
  zc::Clip c = clip_shell(15, K, kHoverHeightMm);
  // the timeline the bones follow is the FIRST variant's -- every variant
  // shares shape_count 4 or 2, and the bone layer only reads amp/agit, which
  // are beat-aligned for all of them. (A variant with a different
  // shape_count kneads on its own schedule; the mote system reads its own
  // phase, so the mana is always right. Named here so it is not a surprise.)
  const LabVariant& VB = kLabVariants[0];
  Rig g;
  for (int f = 0; f < K; ++f) {
    g.reset();
    lab_antenna_knead(g, VB, f);
    loop_alive(g, f, K, K / 22, kAntennaSwayPm, kCompressAmpPm, K / 26);
    face_rest(g);
    apply_gaze(g, 0, kGazeLiftMaxA16 / 4);
    apply_squint(g, blink_at(f, 7));
    g.write(c, f);
    // ---- root: stand at the left, traverse laterally, stand at centre ----
    // `t` runs 0 (start of the stand) -> 1000 (the traverse has landed), and
    // the distance travelled is (1000 - t) BEHIND the world origin along the
    // perpendicular, so the clip finishes at the origin = frame centre.
    int32_t t;
    if (f < kLabStandEndKeys) {
      t = 0;
    } else if (f < kLabTraverseEndKeys) {
      // eased, so the start and stop are not a step (a step reads as a
      // teleport, and the smear plane records it as one)
      t = motion_c2_ease((f - kLabStandEndKeys) * 1000 /
                    (kLabTraverseEndKeys - kLabStandEndKeys));
    } else if (f < K - kLabReleaseKeys) {
      t = 1000;
    } else {
      // Direction 18: return the comparison traverse on a long C2 tail rather
      // than teleporting from centre back to the left at the loop seam.
      const int start = K - kLabReleaseKeys;
      const int den = K - 1 - start;
      const int32_t home = motion_c2_ease(
          (f - start) * 1000 / (den > 0 ? den : 1));
      t = 1000 - home;
    }
    const int32_t back = fxu(kLabTraverseMm) * (1000 - t) / 1000 -
                         fxu(kLabTraverseEndMm);
    c.root[static_cast<size_t>(f) * 3 + 0] =
        -static_cast<int32_t>(static_cast<int64_t>(back) * kLabTraverseXPm / 1000);
    c.root[static_cast<size_t>(f) * 3 + 2] =
        -static_cast<int32_t>(static_cast<int64_t>(back) * kLabTraverseZPm / 1000);
    c.root[static_cast<size_t>(f) * 3 + 1] =
        hover_at(f, K, kHoverHeightMm, kBobAmpAMm, kBobAmpBMm, K / 40, K / 64);
    c.deform[static_cast<size_t>(f)] = compress_at(f, K, K / 40, kCompressAmpPm);
  }
  return c;
}

}  // namespace lab
}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_LAB_H
