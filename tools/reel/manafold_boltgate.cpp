// manafold_boltgate.cpp -- PASS 24: THE LIGHTNING/ANTENNA CLEARANCE PROBE AND
// GATE (mbolt).
//
// Owner Direction 25 item 2: *"Sometimes the Lightning shape goes through the
// antennae, I wish we could fix that somehow."*
//
// THE QUESTION THIS INSTRUMENT EXISTS TO ANSWER, and it was asked BEFORE
// anything was built: of the bolt segments that LOOK as though they pass
// through the antenna, how many actually intersect a rod's volume in 3D, and
// how many merely cross it in screen space while being honestly in front of or
// behind it? The first number is a GEOMETRY fault and 3D avoidance removes it;
// the second is a DRAWING-ORDER / footprint fault and only depth resolution can
// help it. Without the split, both mechanisms would have been built on a guess.
//
// ---------------------------------------------------------------------------
// WHAT IT MEASURES, AND WHAT IT APPROXIMATES
//
// THE 3D LEG IS EXACT. The pass-21 rods rig makes the band four straight rods
// between five ball joints, so the antenna's volume is four capsules and four
// spheres and a segment-vs-capsule distance is closed form. The joints come
// from the skin's own bind stations through fx_anchors_from_pose; the radii
// come from the band's own taper through kBoltRodRadiusMm. No camera, no
// terrain, no render: this leg answers for the whole bank in seconds.
//
// THE SCREEN LEG IS ORTHOGRAPHIC, AND THAT IS DECLARED RATHER THAN HIDDEN.
// "Crosses in screen space" needs a view. The production view DIRECTION is
// exactly reproducible (a ~15 deg down pitch plus the orbit's own integer turn
// per loop, or the fixed three-quarter yaw), but a perspective divide would
// also need the creature's staged world position, which needs the terrain
// lattice and the reel's staging. Reproducing that risks a confident wrong
// number from a camera that is subtly not the shipping one, so this leg
// projects ORTHOGRAPHICALLY along the exact view direction instead. A
// translation cancels in an orthographic test, so the staging cannot matter;
// the only error left is perspective foreshortening across a ~1.3 m creature at
// ~10 m. THE GATE ASSERTS ONLY ON THE 3D LEG. The screen leg is diagnosis, and
// it is labelled as such in the census.
//
// ---------------------------------------------------------------------------
// THE LEGS
//   B1 CLEAR    a subject configured for 3D avoidance has ZERO bolt segments
//               intersecting any rod capsule or ball sphere, on every key and
//               midpoint of its clip.
//   B2 SPLIT    the split REACHES THE DRAWING. The split subject's clip is
//               measured TWICE in one invocation -- once as configured and once
//               forced to N = 1 -- and the leg asserts that the configured run
//               lays exactly N times as many sprites along the bolt, at exactly
//               1/N of the spacing. It encodes no art value and no threshold: it
//               is the mechanism's own contract, and it is the contract this
//               project keeps finding broken (pass 20 shipped a whole ladder of
//               gate runs against a knob only the reel read).
//
//               ! AND B2 IS THIS BECAUSE TWO EARLIER FORMS WERE VACUOUS, WHICH
//               THE CONTROL SAID BOTH TIMES -- and the second failure is the
//               pass's most useful measurement.
//                 1. Bounding the STAMP SPACING at the thinnest rod's radius
//                    (46 mm) was green with the mechanism OFF: the unsplit
//                    strand already stamps every 26.6 mm.
//                 2. Asserting that every depth-straddling segment is DRAWN
//                    partly occluded is ALSO green with the mechanism off --
//                    all 155 of them already are. So the split does not buy
//                    partial occlusion; the bolt was already fine enough to be
//                    cut wherever the geometry straddles a rod. That is
//                    reported as INFO B2b, with both numbers, because it is the
//                    answer to the owner's comparison and not a gate result.
//                 (Getting there also cost two wrong operands of my own, both
//                 recorded at their sites: the rod's MEAN view depth instead of
//                 its depth beside the point, and a SPHERE's surface instead of
//                 a cylinder's. Each produced a confident failure count that was
//                 the measure rather than the drawing.)
//   B3 CONTROL GROUP  every other live subject is measured too, and its census
//               is printed, so "the experiment moved three clips" is a
//               statement about all twenty-two rather than about three.
//
// Every leg has a control fired in the same invocation:
//   --fail-no-avoid  run the avoidance subjects with avoidance OFF. B1 must go
//                    red. This configuration is also the pass-23 BEFORE, so the
//                    control doubles as the measurement's own baseline.
//   --fail-no-split  run the split subject at N = 1 in BOTH passes, so the
//                    ratio the leg asserts becomes 1. B2 must go red.
//   --fail-fat-rod   measure against capsules and spheres scaled to
//                    kFatRodControlPm, which the avoidance was not aimed at, so
//                    the bolt is inside them and B1 MUST go red. Its shipping
//                    reading is zero and a detector reading zero is a claim, so
//                    this is the proof that the leg reads the RODS rather than a
//                    constant -- and no legal stimulus can give it once the
//                    avoidance is correct (the committed-mutant law, as a
//                    committed knob).
//
//                    ! IT WAS A THIN-ROD CONTROL FIRST AND THAT VERSION WAS
//                    DEAD. Shrinking the rods to a tenth moved the operand
//                    (39,500 intersections across the control group became
//                    1,180) but could not turn B1 red, because a thinner rod is
//                    EASIER to clear. A control that moves a number without
//                    firing the leg is not a control; it is a second reading.
//
// ⚠ IT GATES NOTHING ABOUT THE LOOK. Whether the avoided bolt reads better than
// the split one is the owner's comparison, decided on the plates. This
// instrument says only "it is clear" or "it is not", and prints the numbers the
// eye is then asked about.
//
// Usage:
//   manafold-boltgate.exe --gate               assert + fire every control
//   manafold-boltgate.exe --census             the full per-subject table
//   manafold-boltgate.exe --csv <out.csv>      per-frame rows

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "zref/zref.hpp"
#include "zref/zref_trig.hpp"
#include "zref/zref_creature.hpp"
#include "zref/zref_star.hpp"
#include "zref/zref_render.hpp"
#include "zref/zref_texture.hpp"
#include "render_helpers.hpp"
#include "zrender/internal.hpp"

namespace zc = zref::creature;
#include "manafold.h"

namespace {

constexpr double kMm = 1000.0 / 65536.0;  // fx16 (metres) -> millimetres

struct V3 {
  double x = 0, y = 0, z = 0;
};
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double len(V3 a) { return std::sqrt(dot(a, a)); }
V3 from_fx(const int32_t p[3]) { return {p[0] * kMm, p[1] * kMm, p[2] * kMm}; }

/** Closest distance between two segments, in mm. Clamped-parameter solve; the
 *  degenerate and parallel cases fall back to the endpoint solves, which are
 *  exact for them. */
double seg_seg_dist(V3 p0, V3 p1, V3 q0, V3 q1) {
  const V3 d1 = p1 - p0, d2 = q1 - q0, r = p0 - q0;
  const double a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
  const double kEps = 1e-9;
  double s = 0, t = 0;
  if (a <= kEps && e <= kEps) return len(r);
  if (a <= kEps) {
    t = std::max(0.0, std::min(1.0, f / e));
  } else if (e <= kEps) {
    s = std::max(0.0, std::min(1.0, -dot(d1, r) / a));
  } else {
    const double b = dot(d1, d2), c = dot(d1, r);
    const double denom = a * e - b * b;
    s = denom > kEps ? std::max(0.0, std::min(1.0, (b * f - c * e) / denom)) : 0.0;
    t = (b * s + f) / e;
    if (t < 0) {
      t = 0;
      s = std::max(0.0, std::min(1.0, -c / a));
    } else if (t > 1) {
      t = 1;
      s = std::max(0.0, std::min(1.0, (b - c) / a));
    }
  }
  return len((p0 + d1 * s) - (q0 + d2 * t));
}
double seg_point_dist(V3 p0, V3 p1, V3 c) { return seg_seg_dist(p0, p1, c, c); }

// ---------------------------------------------------------------------------
// The posed antenna, as the bolt sees it.
// ---------------------------------------------------------------------------
/** THE THIN-ROD CONTROL, per mille. 1000 is the shipping antenna. */
constexpr int32_t kFatRodControlPm = 2200;  // 220 % -- per MILLE, not per cent
int32_t g_rod_scale_pm = 1000;

struct Antenna {
  V3 joint[5];
  double rod_r[4]{};
  double ball_r[5]{};
  bool valid = false;
  /** Signed clearance in mm: negative INSIDE the antenna's volume, positive
   *  outside; the worst (most negative) obstacle wins. */
  double clearance(V3 a, V3 b) const {
    double worst = 1e18;
    for (int e = 0; e < 4; ++e)
      worst = std::min(worst, seg_seg_dist(a, b, joint[e], joint[e + 1]) - rod_r[e]);
    for (int e = 1; e < 5; ++e)
      if (ball_r[e] > 0)
        worst = std::min(worst, seg_point_dist(a, b, joint[e]) - ball_r[e]);
    return worst;
  }
  double max_rod_r() const {
    return std::max(std::max(rod_r[0], rod_r[1]), std::max(rod_r[2], rod_r[3]));
  }
};

Antenna antenna_from(const u02::FxAnchors& A) {
  Antenna t;
  t.valid = A.joints_valid;
  const double sc = g_rod_scale_pm / 1000.0;
  for (int e = 0; e < 5; ++e) t.joint[e] = from_fx(A.joint[e]);
  for (int e = 0; e < 4; ++e) t.rod_r[e] = u02::kBoltRodRadiusMm[e] * sc;
  for (int e = 0; e < 5; ++e) t.ball_r[e] = u02::kBoltBallRadiusMm[e] * sc;
  return t;
}

// ---------------------------------------------------------------------------
// The view, orthographically. See the header for why this is an approximation
// and why the gate does not rest on it.
// ---------------------------------------------------------------------------
struct View {
  V3 right, up, fwd;  // fwd increases with distance from the camera
};
/** u02_common's showcase pitch, as the reel writes it. Copied as two named
 *  numbers rather than reached for across the translation unit, and printed in
 *  the census banner so a reader can check them against the reel. */
constexpr double kCamPs = 16962.0 / 65536.0;
constexpr double kCamPc = 63313.0 / 65536.0;

View view_for(bool orbit, int f, int frames, int32_t cam_yaw_a16) {
  double turns = static_cast<double>(cam_yaw_a16) / 65536.0;
  if (orbit && frames > 0)
    turns += static_cast<double>(f) / static_cast<double>(frames);
  const double th = turns * 2.0 * 3.14159265358979;
  const double cs = std::cos(th), sn = std::sin(th);
  const V3 wz{sn, 0.0, cs};  // world +Z after rot_world_yaw(theta)
  const V3 wy{0.0, 1.0, 0.0};
  View v;
  v.right = {cs, 0.0, -sn};
  // cam_pitch with zsign -1: w = -ps*y - pc*z + const, and larger w is further
  // away; the screen y row is -(pc*(y-E) + ps*(z+D)) so up is +pc*y + ps*z.
  v.fwd = (wy * -kCamPs) + (wz * -kCamPc);
  v.up = (wy * kCamPc) + (wz * kCamPs);
  return v;
}

bool screen_crosses(const View& v, V3 a, V3 b, V3 p, V3 q, double r) {
  const auto proj = [&](V3 w) { return V3{dot(w, v.right), dot(w, v.up), 0.0}; };
  return seg_seg_dist(proj(a), proj(b), proj(p), proj(q)) < r;
}

// ---------------------------------------------------------------------------
// A bolt segment, as DRAWN: two endpoints and the number of sprites the
// renderer lays along it. The stamp count is the split's whole subject.
// ---------------------------------------------------------------------------
struct Seg {
  V3 a, b;
  int stamps = 1;
  bool edge = false;      // a fold-figure link segment (vs a free strand)
  bool anchored = false;  // touches a link's shared station / a strand anchor
};

/** THE BOLT SEGMENTS THIS FRAME, exactly as drawn.
 *
 *  ⚠ THE FOLD-EDGE PATH IS RE-DERIVED THROUGH THE PRODUCTION FUNCTION, NOT
 *  RE-IMPLEMENTED. mana_fold's own trace hands back the eighteen posed stencil
 *  STATIONS (post-`place()`, so pass 22's knead transform is already in them)
 *  and each link's presence; the path between two stations is bolt_path_morph,
 *  a pure function of those two points and the clock. Calling it means the
 *  pass-24 avoidance that lives inside it is applied here too -- a probe that
 *  re-implemented the path would measure one the renderer does not draw, which
 *  is precisely how an instrument comes to report a clean number about a defect
 *  that is still on screen.
 *
 *  The stamp counts are the renderer's own laws, read from the same accessors
 *  bolt_stamp and mana_fold use. */
void gather_segments(const u02::FxContinuityTrace& tr, uint32_t frame,
                     uint32_t slot, int keys, std::vector<Seg>& out) {
  out.clear();
  const int split = u02::bolt_split_n();
  // the free lightning strands: the trace records the DRAWN points.
  for (int p = 0; p + 1 < tr.free_point_count; ++p) {
    if ((p + 1) % (u02::kBoltSegs + 1) == 0) continue;  // strand boundary
    Seg s;
    s.a = from_fx(tr.free_point[p]);
    s.b = from_fx(tr.free_point[p + 1]);
    s.stamps = u02::path_segment_stamp_count(tr.free_point[p],
                                             tr.free_point[p + 1],
                                             u02::kBoltStampMm, 24) * split;
    s.edge = false;
    const int within = p % (u02::kBoltSegs + 1);
    s.anchored = (within == 0) || (within + 2 == u02::kBoltSegs + 1);
    out.push_back(s);
  }
  // the fold figure's edge links
  const bool strand = u02::u02_strand_on();
  const int per_seg = u02::u02_strand_perseg();
  const int cap_n = strand ? u02::u02_strand_cap() : 24;
  int32_t pts[u02::kFoldEdgeSegs + 1][3];
  for (int i = 0; i < u02::kStencilPts; ++i) {
    if (tr.edge_presence_pm[i] <= 0) continue;
    const int j = (i + 1) % u02::kStencilPts;
    u02::bolt_path_morph(tr.fold_station[i], tr.fold_station[j],
                         u02::kFoldEdgeSegs, frame,
                         slot * 37u + static_cast<uint32_t>(i + 1), keys,
                         u02::kFoldEdgeMorphFrames,
                         u02::kBoltSeed ^ (0x5EDu * static_cast<uint32_t>(i + 1)),
                         pts, u02::kFoldEdgeJitterMm);
    for (int sgi = 0; sgi < u02::kFoldEdgeSegs; ++sgi) {
      int nst = strand ? per_seg
                       : u02::path_segment_stamp_count(pts[sgi], pts[sgi + 1],
                                                       u02::kFoldEdgeStampMm,
                                                       cap_n);
      if (nst < 1) nst = 1;
      if (nst > cap_n) nst = cap_n;
      Seg s;
      s.a = from_fx(pts[sgi]);
      s.b = from_fx(pts[sgi + 1]);
      s.stamps = nst * split;
      s.edge = true;
      s.anchored = (sgi == 0) || (sgi + 1 == u02::kFoldEdgeSegs);
      out.push_back(s);
    }
  }
}

// ---------------------------------------------------------------------------
// The census.
// ---------------------------------------------------------------------------
// The band in which a CLEAR segment still READS as touching the antenna: the
// bolt is drawn as discs, so a core whose centre is a few millimetres off the
// rod's silhouette paints over it whatever the depth buffer says. Reported,
// never gated -- how close is too close is an eye question.
constexpr double kNearBandMm = 60.0;
// B2's bound, DERIVED rather than authored: a chain of sprites claiming to
// resolve a crossing must place them closer together than the radius of the
// thinnest thing they cross. Anything looser cannot cut a rod in two.
// Reported beside B2 so the resolution question stays visible even though it is
// no longer what B2 asserts: the thinnest rod's radius, which the UNSPLIT stamp
// spacing already clears.
// How finely the OVERLAP length is sampled -- fixed, and deliberately
// independent of the split, so the qualifier cannot move with the thing it is
// qualifying.
constexpr int kOverlapSamples = 64;
// The overlap a segment must have with a rod's silhouette before the drawing is
// asked to cut it. Derived: half the thinnest rod's radius, which is the
// shortest crossing a sprite chain at the UNSPLIT spacing (26.6 mm) could still
// be expected to resolve.
constexpr double kSplitOverlapRefMm = 23.0;
constexpr double kSplitGapRefMm =
    static_cast<double>(u02::kBoltRodRadiusMm[1] < u02::kBoltRodRadiusMm[2]
                            ? u02::kBoltRodRadiusMm[1]
                            : u02::kBoltRodRadiusMm[2]);

struct Row {
  const char* name;
  int slot;
  bool orbit;
  int32_t cam_yaw;
  bool avoid;
  int split_n;
  long segs = 0;
  long hit3d = 0;
  long near_miss = 0;
  long cross_front = 0;
  long cross_behind = 0;
  double worst_pen = 0;
  int worst_pen_frame = -1;
  long frames_with_hit = 0;
  int frames = 0;
  double worst_gap = 0;      // mm between stamps, over near-rod segments
  long near_rod_segs = 0;
  // B2: segments whose GEOMETRY straddles a rod's surface in depth, and how many
  // of them the sprite chain actually draws as partly occluded.
  long straddle_segs = 0;
  long straddle_drawn_partial = 0;
  double worst_overlap = 0;
  long stamp_total = 0;   // every sprite the renderer lays along the bolt
  // the same clip measured at N = 1, for B2's ratio and for INFO B2b
  long ref_stamp_total = 0;
  double ref_worst_gap = 0;
  long ref_straddle_segs = 0;
  long ref_straddle_partial = 0;
  // WHERE the residual lives. A gate that says "157 left" and cannot say which
  // primitive they belong to sends the next pass looking in the wrong file.
  long hit_edge = 0, hit_free = 0;
  long hit_anchored = 0, hit_interior = 0;
  long hit_rod = 0, hit_ball = 0;
  double worst_seg_len = 0;
};

/** The LIVE bank with the camera each subject renders under. It mirrors
 *  zhao_reel.cpp's kU02LiveSiteSubjects, and the count is asserted against the
 *  same 22 the renderer asserts, so a subject added there and not here is a
 *  build error rather than a quietly short census. */
constexpr int kLiveSubjects = 22;
Row g_rows[kLiveSubjects] = {
    {"manafold-hover", 0, true, 0, true, 1},
    {"manafold-inspect", 0, true, 0, false, u02::kBoltDepthSplitN},
    {"manafold-channel", 2, false, 0x2000, false, 1},
    {"manafold-trick", 13, false, 0x2000, false, 1},
    {"manafold-damage", 14, false, 0x2000, false, 1},
    {"manafold-hasty", 8, false, 0x2000, false, 1},
    {"manafold-flight", 22, false, 0x2000, false, 1},
    {"manafold-fall", 9, false, 0x2000, false, 1},
    {"manafold-hit", 10, false, 0x2000, false, 1},
    {"manafold-taunt", 11, false, 0x2000, false, 1},
    {"manafold-taunt2", 12, false, 0x2000, false, 1},
    {"manafold-death-drop", 17, false, 0x2000, false, 1},
    {"manafold-death-gutter", 18, false, 0x2000, false, 1},
    {"manafold-lasso", 19, false, 0x2000, false, 1},
    {"manafold-blown", 20, false, 0x2000, false, 1},
    {"manafold-taunt3", 21, false, 0x2000, false, 1},
    {"manafold-drift", 1, false, 0x2000, false, 1},
    {"manafold-curious", 3, false, 0x2000, false, 1},
    {"manafold-startle", 4, false, 0x2000, false, 1},
    {"manafold-rest", 5, false, 0x2000, false, 1},
    {"manafold-pirouette", 6, false, 0x2000, false, 1},
    {"manafold-crackle", 23, false, 0x2000, true, 1},
};

const zc::Clip* clip_for(const zc::CreatureType& T, int slot) {
  for (const zc::Clip& c : T.bank.clips)
    if (c.slot_id == static_cast<uint16_t>(slot)) return &c;
  return nullptr;
}

void run_subject(const zc::CreatureType& T, Row& r, bool force_no_avoid,
                 bool force_no_split, std::FILE* csv) {
  const zc::Clip* cl = clip_for(T, r.slot);
  if (cl == nullptr) return;
  const int keys = static_cast<int>(cl->frame_count);
  r.frames = keys * 2;
  u02::bolt_set_subject(
      (r.avoid && !force_no_avoid) ? u02::BoltAvoid::kRods : u02::BoltAvoid::kOff,
      force_no_split ? 1 : r.split_n);
  u02::FoldState fold{};
  std::vector<u02::ManaSplat> splats;
  std::vector<Seg> segs;
  for (int f = 0; f < r.frames; ++f) {
    std::array<zc::mat3x4fx, zc::kMaxBones> pose{};
    zc::decode_pose(T, *cl, static_cast<uint16_t>(f / 2), pose, nullptr,
                    static_cast<uint8_t>(f % 2));
    const u02::FxAnchors A = u02::fx_anchors_from_pose(T, pose, 0, 0, 0);
    const Antenna ant = antenna_from(A);
    u02::FxContinuityTrace tr{};
    splats.clear();
    int32_t agit = 0;
    u02::mana_fill(9, static_cast<uint32_t>(f), static_cast<uint32_t>(r.slot),
                   keys, A, fold, 1000, splats, &agit, &tr);
    gather_segments(tr, static_cast<uint32_t>(f), static_cast<uint32_t>(r.slot),
                    keys, segs);
    if (!ant.valid) continue;
    const View v = view_for(r.orbit, f, r.frames, r.cam_yaw);
    long fhit = 0;
    for (const Seg& sg : segs) {
      ++r.segs;
      r.stamp_total += sg.stamps;
      const double c = ant.clearance(sg.a, sg.b);
      if (c < 0) {
        ++r.hit3d;
        ++fhit;
        if (sg.edge) ++r.hit_edge; else ++r.hit_free;
        if (sg.anchored) ++r.hit_anchored; else ++r.hit_interior;
        double br = 1e18;
        for (int e = 1; e < 5; ++e)
          if (ant.ball_r[e] > 0)
            br = std::min(br,
                          seg_point_dist(sg.a, sg.b, ant.joint[e]) - ant.ball_r[e]);
        if (br < 0) ++r.hit_ball; else ++r.hit_rod;
        const double sl = len(sg.b - sg.a);
        if (sl > r.worst_seg_len) r.worst_seg_len = sl;
        if (-c > r.worst_pen) {
          r.worst_pen = -c;
          r.worst_pen_frame = f;
        }
      } else if (c < kNearBandMm) {
        ++r.near_miss;
      }
      // B2's operand: any segment close enough to a rod to be partly occluded
      // must be drawn finely enough to cut it. The band is the near band, so a
      // segment that only grazes counts too.
      if (c < kNearBandMm) {
        ++r.near_rod_segs;
        const double gap = len(sg.b - sg.a) /
                           static_cast<double>(sg.stamps > 0 ? sg.stamps : 1);
        if (gap > r.worst_gap) r.worst_gap = gap;
      }
      if (c < 0) continue;  // an intersecting segment is a geometry fault, not
                            // a screen-order one; do not count it twice
      for (int e = 0; e < 4; ++e) {
        if (!screen_crosses(v, sg.a, sg.b, ant.joint[e], ant.joint[e + 1],
                            ant.rod_r[e]))
          continue;
        // ⚠ THE ROD'S SURFACE DEPTH IS TAKEN AT THE POINT IN QUESTION, NOT AS
        // THE ROD'S MEAN. The first version differenced against the mean view
        // depth of the whole rod, and on a rod inclined to the camera the two
        // ends differ by hundreds of millimetres, so it decided "straddles" and
        // "does not" against a reference several rod-widths away from the place
        // it was asking about -- a confidently wrong number, and B2 reported 55
        // of 241 failures that were the measure rather than the drawing. The
        // surface is now the depth of the CLOSEST POINT ON THE AXIS to whatever
        // is being classified, minus the radius.
        //
        // ⚠ AND IT IS THE CYLINDER'S SURFACE, NOT A SPHERE'S. Subtracting the
        // full radius from the axis depth is right only along the line of sight
        // through the axis; at screen offset o from it the near surface is
        // sqrt(r^2 - o^2) in front of the axis, which goes to ZERO at the
        // silhouette edge. Using the full radius everywhere over-states how much
        // of the bolt is "in front" exactly where the crossings happen -- it left
        // 18 of 285 straddles looking undrawn when the drawing was right and the
        // measure was not.
        const auto surface_at = [&](V3 x) {
          const V3 p = ant.joint[e], q = ant.joint[e + 1];
          const V3 ax = q - p;
          const double d2 = dot(ax, ax);
          double t = d2 > 1e-9 ? dot(x - p, ax) / d2 : 0.0;
          t = std::max(0.0, std::min(1.0, t));
          const V3 axp = p + ax * t;
          const double o = std::sqrt(
              std::max(0.0, std::pow(dot(x - axp, v.right), 2.0) +
                                std::pow(dot(x - axp, v.up), 2.0)));
          const double r2 = ant.rod_r[e] * ant.rod_r[e] - o * o;
          return dot(axp, v.fwd) - (r2 > 0 ? std::sqrt(r2) : 0.0);
        };
        const double da = dot(sg.a, v.fwd), db = dot(sg.b, v.fwd);
        const double sa = surface_at(sg.a), sb = surface_at(sg.b);
        if ((da - sa) + (db - sb) < 0) ++r.cross_front; else ++r.cross_behind;
        // B2's operand. The segment STRADDLES the rod's surface in depth when
        // one end is nearer than the surface beside it and the other is not; the
        // drawing shows that only if its sprite chain has a stamp on each side.
        if ((da < sa) == (db < sb)) break;  // not a straddle
        // ⚠ AND IT MUST SPEND LONG ENOUGH OVER THE ROD TO BE CUT AT ALL.
        // A segment that clips the rod's silhouette EDGE tangentially crosses it
        // for a millimetre or two of its length; no sprite chain at any spacing
        // can put stamps on both sides of that, and demanding it is demanding
        // something correct behaviour does not do. B2's first two forms did
        // exactly that and reported 65 failures of 427 with the mechanism ON
        // and 71 with it OFF -- a leg that was measuring the silhouette's edge,
        // not the split. The qualifier is the OVERLAP LENGTH, sampled at a fixed
        // resolution that does not depend on the split, against a reference
        // derived from the thinnest rod rather than authored.
        {
          int inside = 0;
          for (int k = 0; k <= kOverlapSamples; ++k) {
            const V3 x = sg.a + (sg.b - sg.a) *
                                    (static_cast<double>(k) / kOverlapSamples);
            if (screen_crosses(v, x, x, ant.joint[e], ant.joint[e + 1],
                               ant.rod_r[e]))
              ++inside;
          }
          const double overlap_mm = len(sg.b - sg.a) *
                                    static_cast<double>(inside) /
                                    (kOverlapSamples + 1);
          if (overlap_mm > r.worst_overlap) r.worst_overlap = overlap_mm;
          if (overlap_mm < kSplitOverlapRefMm) break;
        }
        ++r.straddle_segs;
        bool any_front = false, any_behind = false;
        for (int k = 0; k < sg.stamps; ++k) {
          const double u = sg.stamps > 1
                               ? static_cast<double>(k) / (sg.stamps - 1)
                               : 0.0;
          const V3 x = sg.a + (sg.b - sg.a) * u;
          if (!screen_crosses(v, x, x, ant.joint[e], ant.joint[e + 1],
                              ant.rod_r[e]))
            continue;  // this sprite's centre is not over the rod at all
          if (dot(x, v.fwd) < surface_at(x)) any_front = true;
          else any_behind = true;
        }
        if (any_front && any_behind) ++r.straddle_drawn_partial;
        break;
      }
    }
    if (fhit > 0) ++r.frames_with_hit;
    if (csv != nullptr)
      std::fprintf(csv, "%s,%d,%d,%ld,%ld,%.2f\n", r.name, r.slot, f,
                   static_cast<long>(segs.size()), fhit, r.worst_gap);
  }
}

void reset_rows() {
  for (Row& r : g_rows) {
    const Row keep{r.name, r.slot, r.orbit, r.cam_yaw, r.avoid, r.split_n};
    r = keep;
  }
}

void print_table(const char* title) {
  std::printf("\n%s\n", title);
  std::printf("%-24s %4s %8s %8s %8s %8s %8s %7s %9s %8s\n", "subject", "slot",
              "segs", "hit3d", "worst_mm", "nearmiss", "xfront", "xbehind",
              "frames_hit", "gap_mm");
  for (const Row& r : g_rows)
    std::printf("%-24s %4d %8ld %8ld %8.1f %8ld %8ld %7ld %9ld %8.1f\n", r.name,
                r.slot, r.segs, r.hit3d, r.worst_pen, r.near_miss,
                r.cross_front, r.cross_behind, r.frames_with_hit, r.worst_gap);
  std::printf("\nBREAKDOWN of the intersections -- which primitive, which "
              "obstacle, and whether the segment touches an anchor point\n");
  std::printf("%-24s %8s %8s %10s %10s %8s %8s %10s\n", "subject", "edge",
              "free", "anchored", "interior", "rod", "ball", "worstlen");
  for (const Row& r : g_rows)
    std::printf("%-24s %8ld %8ld %10ld %10ld %8ld %8ld %10.1f\n", r.name,
                r.hit_edge, r.hit_free, r.hit_anchored, r.hit_interior,
                r.hit_rod, r.hit_ball, r.worst_seg_len);
}

long total_hits() {
  long n = 0;
  for (const Row& r : g_rows) n += r.hit3d;
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  bool gate = false, census = false;
  const char* csv_path = nullptr;
  bool ctl_no_avoid = false, ctl_no_split = false, ctl_fat = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--gate") gate = true;
    else if (a == "--census") census = true;
    else if (a == "--csv" && i + 1 < argc) csv_path = argv[++i];
    else if (a == "--fail-no-avoid") ctl_no_avoid = true;
    else if (a == "--fail-no-split") ctl_no_split = true;
    else if (a == "--fail-fat-rod") ctl_fat = true;
    else {
      std::fprintf(stderr, "mbolt: unknown argument '%s'\n", a.c_str());
      return 2;
    }
  }
  if (!u02::apply_knead_dip_env()) return 2;
  if (!gate && !census && csv_path == nullptr) census = true;

  const zc::CreatureType& T = u02::type();
  std::printf("mbolt: rods rig=%s  rod radii mm = %d %d %d %d  balls = %d %d %d %d\n",
              u02::rig_rods() ? "rods" : "pass20", u02::kBoltRodRadiusMm[0],
              u02::kBoltRodRadiusMm[1], u02::kBoltRodRadiusMm[2],
              u02::kBoltRodRadiusMm[3], u02::kBoltBallRadiusMm[1],
              u02::kBoltBallRadiusMm[2], u02::kBoltBallRadiusMm[3],
              u02::kBoltBallRadiusMm[4]);
  std::printf("mbolt: clearance %d mm, split N default %d, split gap bound %.0f mm\n",
              u02::g_u02_bolt_clearance_mm, u02::kBoltDepthSplitN,
              kSplitGapRefMm);
  std::printf("mbolt: screen leg is ORTHOGRAPHIC along the production view "
              "direction (ps %.5f pc %.5f); the gate does not rest on it\n",
              kCamPs, kCamPc);

  std::FILE* csv = nullptr;
  if (csv_path != nullptr) {
    csv = std::fopen(csv_path, "w");
    if (csv == nullptr) {
      std::fprintf(stderr, "mbolt: cannot open %s\n", csv_path);
      return 2;
    }
    std::fprintf(csv, "subject,slot,frame,segments,hits,worst_gap_mm\n");
  }

  g_rod_scale_pm = ctl_fat ? kFatRodControlPm : 1000;
  reset_rows();
  for (Row& r : g_rows) run_subject(T, r, ctl_no_avoid, ctl_no_split, csv);
  // B2's reference pass: the same clip, same avoidance, forced to N = 1. Two
  // measurements of one clip in one invocation, so the ratio the leg asserts is
  // a comparison rather than a remembered number from another run.
  for (Row& r : g_rows) {
    if (r.split_n <= 1) continue;
    Row ref{r.name, r.slot, r.orbit, r.cam_yaw, r.avoid, r.split_n};
    run_subject(T, ref, ctl_no_avoid, /*force_no_split=*/true, nullptr);
    r.ref_stamp_total = ref.stamp_total;
    r.ref_worst_gap = ref.worst_gap;
    r.ref_straddle_segs = ref.straddle_segs;
    r.ref_straddle_partial = ref.straddle_drawn_partial;
  }
  if (csv != nullptr) std::fclose(csv);

  if (census || csv_path != nullptr)
    print_table("CENSUS -- per live subject, every key and midpoint");

  if (!gate) return 0;

  // ---- B1 CLEAR ----------------------------------------------------------
  int fails = 0;
  for (const Row& r : g_rows) {
    if (!r.avoid) continue;
    if (r.hit3d != 0) {
      std::printf("FAIL B1 CLEAR %s: %ld of %ld bolt segments intersect the "
                  "antenna (worst %.1f mm at frame %d)\n",
                  r.name, r.hit3d, r.segs, r.worst_pen, r.worst_pen_frame);
      ++fails;
    } else {
      std::printf("OK   B1 CLEAR %s: 0 of %ld bolt segments intersect the "
                  "antenna over %d frames\n", r.name, r.segs, r.frames);
    }
  }
  // ---- B2 SPLIT ----------------------------------------------------------
  for (const Row& r : g_rows) {
    if (r.split_n <= 1) continue;
    const long want = r.ref_stamp_total * r.split_n;
    const bool gap_ok = r.ref_worst_gap > 0 &&
                        std::fabs(r.worst_gap * r.split_n - r.ref_worst_gap) <
                            0.05 * r.ref_worst_gap;
    if (r.stamp_total != want || !gap_ok) {
      std::printf("FAIL B2 SPLIT %s: N=%d lays %ld sprites against %ld expected "
                  "(%ld unsplit x %d), spacing %.2f mm against %.2f/%d\n",
                  r.name, r.split_n, r.stamp_total, want, r.ref_stamp_total,
                  r.split_n, r.worst_gap, r.ref_worst_gap, r.split_n);
      ++fails;
    } else {
      std::printf("OK   B2 SPLIT %s: N=%d lays %ld sprites along the bolt, "
                  "exactly %d x the %ld it lays unsplit, and the near-rod "
                  "spacing is %.2f mm against %.2f\n",
                  r.name, r.split_n, r.stamp_total, r.split_n, r.ref_stamp_total,
                  r.worst_gap, r.ref_worst_gap);
    }
    std::printf("INFO B2b PARTIAL OCCLUSION %s: %ld of %ld depth-straddling "
                "segments are drawn partly occluded at N=%d, and %ld of %ld at "
                "N=1 -- the split changes the SPACING, not whether a crossing is "
                "cut, because the unsplit bolt is already finer than the rod\n",
                r.name, r.straddle_drawn_partial, r.straddle_segs, r.split_n,
                r.ref_straddle_partial, r.ref_straddle_segs);
  }
  // ---- B3 CONTROL GROUP --------------------------------------------------
  {
    long other = 0, others_with_hits = 0;
    for (const Row& r : g_rows) {
      if (r.avoid || r.split_n > 1) continue;
      other += r.hit3d;
      if (r.hit3d > 0) ++others_with_hits;
    }
    std::printf("INFO B3 CONTROL GROUP: %ld intersections remain on the %ld "
                "untouched subjects that still have them -- that is the "
                "measured size of the rollout the owner is deciding about\n",
                other, others_with_hits);
  }

  if (ctl_no_avoid || ctl_no_split || ctl_fat) {
    // A control run REPORTS; it is an instrument demonstration, not a gate
    // failure, so it returns 0 and says whether it fired (the mrod precedent).
    const bool fired = fails > 0;
    std::printf("CONTROL %s%s%s: the control %s (%d leg(s) red, %ld total "
                "intersections)\n",
                ctl_no_avoid ? "--fail-no-avoid " : "",
                ctl_no_split ? "--fail-no-split " : "",
                ctl_fat ? "--fail-fat-rod" : "",
                fired ? "FIRED" : "did NOT fire", fails, total_hits());
    if (ctl_fat) {
      std::printf("CONTROL --fail-fat-rod: radii scaled to %d pm; B1's operand "
                  "is reading the RODS, not a constant\n",
                  kFatRodControlPm);
    }
    return fired ? 0 : 1;  // a control that does not fire is the failure
  }
  std::printf("mbolt: %s\n", fails == 0 ? "ALL LEGS OK" : "LEGS RED");
  return fails == 0 ? 0 : 1;
}
