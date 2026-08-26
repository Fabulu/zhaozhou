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
constexpr int32_t kHeadHalfMm = 270;   // half-thickness at the skull

// THE TAPER, HAND-SET (Fabian, 2026-08-26: author visually, render, look,
// compare, adjust -- the drawing-derived profile measured a projection, not
// the animal). {position along the body in 1/1000, half-thickness in 1/1000
// of kHeadHalfMm}. Bulbous skull, a deep neck pinch, a full easy mid-body,
// then one long clean taper out to the fork.
struct TaperKey {
  int t, r;
};
constexpr TaperKey kTaper[] = {
    {0, 1000},  {50, 990},  {100, 950}, {150, 810}, {205, 640},  // skull + pinch
    {280, 730}, {400, 790}, {520, 760}, {620, 680},              // the full body
    {720, 560}, {820, 420}, {900, 300}, {960, 210}, {1000, 170}  // out to the fork
};
constexpr int kTaperKeys = static_cast<int>(sizeof(kTaper) / sizeof(TaperKey));
// Bind height of the body axis. This is the HEAD height, not the body height:
// bone 0 is the nose and the reel ground-snaps the ROOT, so at 300 mm the
// animal was pinned to the terrain by its face and reared backwards. The
// stance arches the body UP behind the head (the sheets' apex is above the
// skull) and brings the middle down to the ground from here. TUNED against
// the pose probe (scratchpad zixx_check): belly rides 0..25 mm.
constexpr int32_t kBodyY = 540;

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
constexpr int32_t kEyeBulgeNum = 34; // extra lateral half-width, % of the ring

// the dorsal crest: geometry, because there is no texture page pipeline yet
constexpr int32_t kCrestNum = 46;   // crest half-width = body half-width * n/100
constexpr int32_t kCrestLift = 104;  // crest centre, as a % of body half-height

// the tail: two SMALL flat pointy blades left and right, plus a tiny middle
// spike. Fabian, 2026-08-26: "The fins are gargantuan while on the reference
// sketch they are small." The first sizing (1180 mm on a 3050 mm body) put a
// blade at 39% of the animal; the sketch's slivers are a sixth of that mass.
constexpr int kBladeSides = 8;
constexpr int kBladeRings = 7;
constexpr int32_t kBladeLen = 480;
constexpr int32_t kBladeW0 = 70;       // half-width at the root (LATERAL)
constexpr int32_t kBladeThick0 = 16;   // half-thickness at the root (VERTICAL)
constexpr int32_t kBladeSplay = 6900;  // ~38 deg apart, about the vertical axis
constexpr int32_t kBladeRise = 1500;   // lifted, so they read against the sky
constexpr int32_t kSpikeLen = 190;
constexpr int32_t kSpikeR = 26;

// -- colours: hue from the sheets, saturation and value ART-DIRECTED --------
// The measured pigment (PALETTE.md) is a HUE REFERENCE, not the answer: the
// scanner flattens, and a value that reads as pink on white paper reads as
// grey-white on dark ochre ground at 240p. Shipped colours are chosen by
// LOOKING at renders. These are the flat-colour fallbacks; the texture page's
// copies live at the top of tools/pack/mkcreaturepage.py -- keep them in step.
constexpr uint8_t kGreen[3] = {120, 184, 68};    // flank
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
constexpr int kAttackKeys = 72;
constexpr int kFallKeys = 48;

// THE CANONICAL S, as a slope table. d[k] is the direction of body segment k
// (walking BACKWARD from the nose) in angle16, POSITIVE = DESCENDING (the
// sign convention positive joint pitch established). The sheets' S is: the
// neck arches UP behind the head to an apex ABOVE the skull (Front.png's
// "big bulgey head" is this arch seen end-on -- Fabian's own correction),
// sweeps down to the ground, runs flat along it, and the tail rises behind.
// Joint pitch is the DIFFERENCE of adjacent slopes, so the whole chain makes
// the shape and no single joint carries a corner.
constexpr int kStanceSlopes = kSpineBones - 1;  // 19 segments
constexpr int32_t kStanceSlope[kStanceSlopes] = {
    // neck: a LONG easy rise, the head carried well forward of the arch
    -3000, -5200, -6600, -6200, -4300,
    // the great arch: over the apex and down to the ground
    1500, 5200, 8600, 10800, 11600, 10200, 6600, 2400,
    // grounded run (the caterpillar's working span)
    0, 0, 0,
    // the tail rises behind
    -3300, -6200, -8700};
// which slope entries the descent lobe occupies (breathing deepens these)
constexpr int kStanceDescend0 = 5;
constexpr int kStanceDescend1 = 12;
// and which are the grounded run (the walk's hump travels here)
constexpr int kStanceGround0 = 13;
constexpr int kStanceGround1 = 15;

// The idle is RELAXED. The breath DEEPENS the descent lobe while the root
// (the nose) rises to match, so the head and the arch visibly bob while the
// grounded belly stays put -- the only way to bob a creature whose root is
// ground-snapped by the nose without floating or burying it.
constexpr int32_t kIdleDeepen = 90;       // 1/1000 extra descent authority
constexpr int32_t kIdleBobComp = 46;      // mm of root rise at full breath, TUNED
constexpr int32_t kIdleGirth = 42;        // girth swing, 1/1000 of scale
constexpr int32_t kIdleTailSway = 2200;   // lazy left-right tail sway

// CATERPILLAR walk: the S holds, the head glides high, and ONLY the grounded
// run carries a travelling hump. The hump is authored as a HEIGHT field and
// converted to joint pitches by second difference, so it is a bump above the
// ground line by construction and can never reach below it.
constexpr int32_t kWalkHumpMm = 95;     // hump height
constexpr int32_t kWalkSway = 500;      // secondary lateral life only
constexpr int32_t kWalkSpeed = 11;      // mm per reel frame
constexpr int32_t kWalkBob = 26;        // mm of breath-bob while walking

// ATTACK geometry: the body coils into a near-circle of this radius
// (kBodyLenMm / 2 pi), spins about the coil's centre, and the tail-first
// spear dive needs the root this high for the tail tip to just reach ground.
constexpr int32_t kCoilR = 485;
constexpr int32_t kSpearPitch = 23300;  // ~128 deg: tail points forward-down

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
  kTileBody = 0,   // flank, with the dorsal band painted at U=192
  kTileHead = 1,   // head and throat
  kTileEye = 2,    // eye
  kTileRim = 3,    // eye rim / pupil
  kTileBlade = 4,  // tail blade
  kTileCrest = 5   // crest / blade edging
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
inline void apply_stance(Rig& g, int32_t authority, int32_t deepen = 0) {
  int32_t prev = 0;
  for (int k = 0; k < kStanceSlopes; ++k) {
    int64_t d = kStanceSlope[k];
    if (k >= kStanceDescend0 && k <= kStanceDescend1) {
      d += (d * deepen) / 1000;
    }
    d = (d * authority) / 1000;
    const int32_t pitch = static_cast<int32_t>(d) - prev;
    prev = static_cast<int32_t>(d);
    g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch));
  }
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
    // 1+2. THE BREATH IS THE BOB. breath in 0..1000: the descent lobe deepens
    // by breath * kIdleDeepen while the root rises breath * kIdleBobComp mm,
    // so the head and the whole arch rise and settle together while the
    // grounded belly stays exactly where it is. Verified by the pose probe:
    // belly excursion across the loop is millimetres.
    const int32_t breath = ((s + 65536) * 500) >> 16;  // 0..1000
    apply_stance(g, 1000, (breath * kIdleDeepen) / 1000);

    // 3. the tail plays: a lazy left-right sway spread over the rear third, so
    // the whole back half carries it rather than the fork alone
    const int32_t sway = (st * kIdleTailSway) >> 16;
    for (int k = (kSpineBones * 2) / 3; k < kSpineBones; ++k) {
      const int reach = ((k - (kSpineBones * 2) / 3) * 1000) / (kSpineBones / 3 + 1);
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k],
          quat_y(static_cast<int32_t>((static_cast<int64_t>(sway) * (300 + reach)) / 1000)));
    }
    g.tail_rest(kBladeSplay + ((st * 900) >> 16), kBladeRise + ((s * 500) >> 16));
    g.write(c, f);
    // the root rise that pairs with the deepened descent (see above).
    // ROOT CHANNEL UNITS ARE fx16 METRES -- the first pass wrote plain mm
    // here, which is 1/65536 of a mm once decoded: the "bob" never existed.
    c.root[f * 3 + 1] = fxm((breath * kIdleBobComp) / 1000);
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
  // hump window: one bone either side of the grounded run, so the hump can
  // fade in from the arch side and out into the tail side without a corner
  const int32_t bLo = kStanceGround0 - 1;                     // first bone of the window
  const int32_t bHi = kStanceGround1 + 2;                     // last bone of the window
  const int32_t span1000 = (bHi - bLo) * 1000;                // window width, milli-bones
  for (int f = 0; f < kWalkKeys; ++f) {
    Rig g;
    g.reset();
    // gentle breath while walking: same deepen+root pairing as the idle so
    // the belly never floats or digs (the ratio deepen:root-mm is calibrated)
    const int32_t bs =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((f * per_key * 2) & 0xFFFF)}).raw;
    const int32_t breath = ((bs + 65536) * 500) >> 16;  // 0..1000
    apply_stance(g, 1000,
                 (breath * kIdleDeepen * kWalkBob) / (kIdleBobComp * 1000));

    // THE CATERPILLAR HUMP, as a height field converted to joint pitches by
    // second difference. h[b] >= 0 by construction, so the belly line can arch
    // UP off the ground but never through it. The hump travels the grounded
    // window once per loop and its amplitude fades in and out at the window's
    // ends (sin envelope), so the loop closes without a pop.
    int32_t h[kSpineBones] = {};
    const int32_t c1000 = bLo * 1000 + (f * span1000) / kWalkKeys;  // hump centre
    const int32_t env = zref::fx_sin(zref::angle16{static_cast<uint16_t>(
                            ((c1000 - bLo * 1000) * 32768 / span1000) & 0xFFFF)})
                            .raw;  // 0..65536 over the traverse
    for (int b = 0; b < kSpineBones; ++b) {
      const int32_t d = b * 1000 - c1000;  // milli-bones from the hump centre
      if (d <= -2000 || d >= 2000) continue;
      // cos^2 bump, half-width two bones
      const int32_t ca =
          zref::fx_cos(zref::angle16{static_cast<uint16_t>(((d * 16384) / 2000) & 0xFFFF)}).raw;
      const int64_t bump = (static_cast<int64_t>(ca) * ca) >> 16;  // 0..65536
      h[b] = static_cast<int32_t>((bump * env >> 16) * kWalkHumpMm >> 16);
    }
    // pitch delta at joint k = change of slope: second difference of h, in
    // angle16 (~65 angle16 per mm over a 160 mm segment, small-angle)
    for (int k = 1; k < kSpineBones - 1; ++k) {
      const int32_t dd = h[k + 1] - 2 * h[k] + h[k - 1];
      if (dd != 0) g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(-dd * 65));
    }

    // secondary lateral life on the mid-body only, a fraction of the gait
    for (int k = 5; k <= 11; ++k) {
      const int32_t ph = f * per_key + k * 5000;
      const int32_t sw =
          (zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw * kWalkSway) >> 16;
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_y(sw));
    }
    g.tail_rest();
    g.write(c, f);
    c.root[f * 3 + 1] = fxm((breath * kWalkBob) / 1000);
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

  // WHAT HAPPENS, in order (Fabian, 2026-08-26): "It should roll up, the
  // whole body should rotate three times, then it quickly unrolls, becomes
  // straight, and tail first flies towards the enemy - or the ground. Don't
  // make it go through the ground, no clipping."
  //
  //   1. the S gathers and the body ROLLS UP into a near-circle (curl)
  //   2. the WHOLE coil somersaults three times about its own centre: the
  //      spin lives on BONE 0 (which rotates the entire skeleton) and the
  //      root displacement disp = c - R(theta)*c re-pivots that rotation from
  //      the nose to the coil centre c = (0, kCoilR) above it
  //   3. the spin runs PAST three turns to kSpearPitch while the curl snaps
  //      to zero: the body unrolls straight with its tail pointing
  //      forward-and-down -- the spear, tail first
  //   4. the root drops until the tail tip just reaches the ground (the
  //      contact height is geometry: kBodyLenMm * sin(spear angle) - kBodyY,
  //      checked by the pose probe -- clipping is not permitted to exist)
  //   5. the spin completes the fourth turn as the animal falls back to the
  //      ground and the S re-gathers, so the loop closes clean.

  // 1000 = rolled into the coil, 0 = straight
  static const Key kCurl[] = {{0, 0}, {8, 350}, {16, 1000}, {44, 1000}, {50, 0}, {71, 0}};
  // how much of the canonical S remains
  static const Key kAuth[] = {{0, 1000}, {8, 450}, {16, 0}, {58, 0}, {66, 650}, {71, 1000}};
  // accumulated turn of the WHOLE BODY in 1/1000 of a full rotation. 3000 =
  // the three somersaults; 3394 = spear (kSpearPitch further); 4000 = the
  // fourth turn that lands it. The -40 at key 10 is the wind-up.
  static const Key kSpin[] = {{0, 0},     {10, -40},  {16, 0},    {22, 600},
                              {30, 1500}, {38, 2500}, {44, 3000}, {52, 3394},
                              {56, 3394}, {62, 3720}, {68, 4000}, {71, 4000}};
  // root lift in mm: the leap, the high unroll, the dive PAST surface
  // contact. The spear's true tip is the BLADE tips, so the reach is
  // kBodyLenMm + kBladeLen = 3530: surface contact at the 142-deg spear is
  // 3530*sin(142) - kBodyY = ~1550, and 1350 buries the blades 200 mm --
  // the strike must BITE: deep, localised, brief, AUTHORED penetration.
  // Keys 46..51 track the swing where the tail hangs nearest vertical
  // (need = 3530*sin(phi) - 630, phi = (spin-3000)*0.36 deg).
  static const Key kLift[] = {{0, 0},     {8, 40},    {16, 700},  {30, 1150},
                              {44, 950},  {46, 1460}, {48, 2750}, {49, 2950},
                              {50, 2870}, {51, 2400}, {52, 1350}, {56, 1350},
                              {60, 900},  {66, 300},  {71, 0}};
  // root forward drive in mm during the dive, returned to zero by the wrap
  static const Key kFwd[] = {{0, 0},   {44, 0},   {48, 220}, {52, 520},
                             {57, 520}, {66, 160}, {71, 0}};
  const int nC = static_cast<int>(sizeof(kCurl) / sizeof(Key));
  const int nA = static_cast<int>(sizeof(kAuth) / sizeof(Key));
  const int nS = static_cast<int>(sizeof(kSpin) / sizeof(Key));
  const int nL = static_cast<int>(sizeof(kLift) / sizeof(Key));
  const int nF = static_cast<int>(sizeof(kFwd) / sizeof(Key));

  // rolling up: 360 degrees spread over the 18 interior joints
  const int32_t coil_pitch = -(65536 / (kSpineBones - 2));

  for (int f = 0; f < kAttackKeys; ++f) {
    Rig g;
    g.reset();
    const int curl = curve(kCurl, nC, f);
    const int auth = curve(kAuth, nA, f);
    const int spin = curve(kSpin, nS, f);
    const int lift = curve(kLift, nL, f);
    const int fwd = curve(kFwd, nF, f);

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
  c.events = {{52, zc::kEvAttack, 0}};
  return c;
}

// Slot 4 - FALLING FLAIL. Fabian, 2026-08-26: "it should look more
// distressed, more frantic flailing with the face. Even here, have it keep
// its S shape. The S shape should be signature everywhere."
//
// So: the canonical S is applied at FULL authority every frame and the panic
// rides on top of it -- a whole-body tumble wobble on bone 0 (airborne, so
// swinging the world about the nose is exactly the flailing physics wants),
// a fast frantic flail on the head joints, a writhe through the mid-body
// small enough to leave the S legible, a tail whip, and the blades beating.
// The animal is AIRBORNE for the whole loop (kFallLift), no contact at all.
constexpr int32_t kFallLift = 900;  // mm of air under the S
inline zc::Clip build_fall() {
  zc::Clip c;
  c.slot_id = 4;
  // 60 Hz presentation interpolation. Keys stay authored at 30 Hz and every
  // event frame is untouched; only the shown pose is blended at the half
  // tick. Without it the somersault and the flail visibly step.
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

    // whole-body tumble: three slow incommensurate axes on the root bone
    const int32_t t1 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
    const int32_t t2 =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2 + 17000) & 0xFFFF)}).raw;
    const int32_t t3 =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 3 + 40000) & 0xFFFF)}).raw;
    g.q[kBSpine0] = quat_mul(
        quat_mul(quat_z((t1 * 2100) >> 16), quat_x((t2 * 2600) >> 16)),
        quat_mul(quat_y((t3 * 1700) >> 16), g.q[kBSpine0]));

    // FRANTIC FACE: fast, large, multi-axis flail on the first joints. 5 and
    // 7 cycles per loop, different phases per joint, so the head never
    // repeats a gesture inside the loop.
    for (int k = 1; k <= 3; ++k) {
      const int32_t pf1 = ph * 5 + k * 21000;
      const int32_t pf2 = ph * 7 + k * 9000;
      const int32_t s1 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(pf1 & 0xFFFF)}).raw;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(pf2 & 0xFFFF)}).raw;
      const int32_t amp = 3400 - k * 700;  // strongest right at the head
      g.q[kBSpine0 + k] =
          quat_mul(g.q[kBSpine0 + k],
                   quat_mul(quat_z((s1 * amp) >> 16), quat_y((s2 * (amp * 2 / 3)) >> 16)));
    }
    // a small writhe through the middle -- distress, not enough to erase the S
    for (int k = 5; k <= 13; ++k) {
      const int32_t pw = ph * 3 - k * (65536 / 9);
      const int32_t sw = zref::fx_sin(zref::angle16{static_cast<uint16_t>(pw & 0xFFFF)}).raw;
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_x((sw * 900) >> 16));
    }
    // the tail whips harder than the middle
    for (int k = 15; k < kSpineBones - 1; ++k) {
      const int32_t pt = ph * 4 + k * 15000;
      const int32_t st = zref::fx_sin(zref::angle16{static_cast<uint16_t>(pt & 0xFFFF)}).raw;
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_y((st * 2200) >> 16));
    }
    // the blades beat fast
    const int32_t fl =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 6) & 0xFFFF)}).raw;
    g.tail_rest(kBladeSplay + ((fl * 2600) >> 16), kBladeRise + ((fl * 1400) >> 16));
    g.write(c, f);
    // airborne the whole loop, with a slow drift so the fall reads as motion
    c.root[f * 3 + 1] = fxm(kFallLift + ((t2 * 90) >> 16));
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
      // the sheets colour the two blades differently: one neon pink, one
      // green (Front.png: left green, right pink)
      p.page = side == 0 ? kTileCrest : kTileBlade;
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
      p.page = kTileCrest;
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
