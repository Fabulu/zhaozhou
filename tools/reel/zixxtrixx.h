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
// The body TAPER is not a knob: it is generated from the concept sheet by
// Upheaval/creature/Zixxtrixx/tools/gen_profile.py into zixxtrixx_profile.h.
// ---------------------------------------------------------------------------
//
// WHAT CHANGED FROM THE FIRST MODEL
//
// * THE BODY IS ONE CONTINUOUS CHAIN PART. It used to be ten independent rigid
//   cylinders, which is why the attack opened a measured 61 mm hole (5.3 px on
//   a 19 px body). Rings now carry their own {b0,b1,w0} and blend across every
//   joint, so the skin cannot come apart however hard it is bent.
// * IT IS NOT A 100-TRIANGLE SNAKE. 20 sides, 57 stations. The donor roster
//   runs 1,600-10,500 vertices; the old model was ~450 and six-sided, and six
//   sides were chosen to appease a GIF palette that has no business shaping
//   geometry.
// * THE TAPER IS MEASURED and NOT monotonic: it pinches hard at the neck,
//   swells again through the body, and only then tapers away.
// * SECTIONS ARE ELLIPTICAL. Measured 1.19:1 body, 1.12:1 head, and the tail
//   blades are FLAT. A rotationally symmetric stack cannot say either.
// * COLOURS ARE THE SHEET'S OWN PIGMENT, orange included. See PALETTE.md.
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

// the measured taper, generated from the concept sheet
#include "zixxtrixx_profile.h"

// ============================== KNOBS ======================================
// Millimetres unless noted. Angles in angle16: 65536 = one turn, 182 ~ 1 deg.

constexpr int kSides = 20;       // sides around the body at LOD0
constexpr int kSpineBones = 20;  // chain bones nose -> fork
// Bind height of the body axis. This is the HEAD height, not the body height:
// bone 0 is the nose and the reel ground-snaps the ROOT, so at 300 mm the
// animal was pinned to the terrain by its face and reared backwards. The
// stance dips the middle down to the ground from here.
constexpr int32_t kBodyY = 1240;

// section ellipticity, measured: wider than tall
constexpr int32_t kHeadWideNum = 112;  // head 1.12 : 1
constexpr int32_t kBodyWideNum = 119;  // body 1.19 : 1
constexpr int kHeadStations = 9;       // how many stations read as head

// eyes: enormous, ~45% of head height in the drawing
constexpr int32_t kEyeR = 118;
constexpr int32_t kEyeRimR = 146;
constexpr int kEyeStation = 4;
constexpr int32_t kEyeUp = 92;
constexpr int32_t kEyeOut = 150;
constexpr int32_t kEyeBulge = 150;

// the dorsal crest: geometry, because there is no texture page pipeline yet
constexpr int32_t kCrestNum = 46;   // crest half-width = body half-width * n/100
constexpr int32_t kCrestLift = 104;  // crest centre, as a % of body half-height

// the tail: two big FLAT POINTY blades far apart left and right, plus a tiny
// middle spike (Fabian, 2026-08-26)
constexpr int kBladeSides = 8;
constexpr int kBladeRings = 9;
constexpr int32_t kBladeLen = 1180;
constexpr int32_t kBladeW0 = 205;      // half-width at the root (LATERAL)
constexpr int32_t kBladeThick0 = 40;   // half-thickness at the root (VERTICAL)
constexpr int32_t kBladeSplay = 6900;  // ~38 deg apart, about the vertical axis
constexpr int32_t kBladeRise = 1500;   // lifted, so they read against the sky
constexpr int32_t kSpikeLen = 330;
constexpr int32_t kSpikeR = 52;

// -- colours: MEASURED from the sheets, not chosen -------------------------
// Method and evidence: Upheaval/creature/Zixxtrixx/PALETTE.md and
// PALETTE-PROOF.png. The first pass took the MEDIAN of each crayon region,
// which is the pigment lightened by however much paper showed through, then
// hand-saturated to compensate. These are the pigment.
constexpr uint8_t kGreen[3] = {120, 184, 68};    // flank
constexpr uint8_t kPink[3] = {233, 188, 206};    // dorsal band
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

// The canonical S, as joint pitch in angle16.
constexpr int32_t kStanceRear = 5600;    // how hard the front rears up
constexpr int32_t kStanceDip = 3600;     // how deep the middle dips
constexpr int32_t kStanceTailUp = 4400;  // how high the tail is carried

// The idle is RELAXED.
constexpr int32_t kIdleBreathe = 150;    // +-15% of stance authority
constexpr int32_t kIdleLift = 52;        // mm the whole animal bobs
constexpr int32_t kIdleGirth = 42;       // girth swing, 1/1000 of scale
constexpr int32_t kIdleTailSway = 2200;  // lazy left-right tail sway

// caterpillar locomotion: VERTICAL and longitudinal, not lateral
constexpr int32_t kWalkArch = 4300;     // travelling arch amplitude
constexpr int32_t kWalkWaves = 1;       // one arch on the body at a time
constexpr int32_t kWalkHeadBob = 2600;  // head and neck bob (Fabian)
constexpr int32_t kWalkSway = 700;      // secondary lateral life only
constexpr int32_t kWalkSpeed = 26;      // mm per reel frame

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
  kBEyeL = static_cast<uint8_t>(kSpineBones + 5),
  kBEyeR = static_cast<uint8_t>(kSpineBones + 6),
  kBoneCount = static_cast<uint8_t>(kSpineBones + 7)
};
static_assert(kBoneCount <= 32, "creature_rules 1.2: <= 32 bones");

// station -> world distance back from the nose
inline int32_t station_x(int i) {
  return static_cast<int32_t>((static_cast<int64_t>(kBodyLenMm) * i) / (kProfileStations - 1));
}
// station -> half-thickness in mm, straight off the measured profile
inline int32_t station_r(int i) {
  return static_cast<int32_t>((static_cast<int64_t>(kHeadHalfMm) * kProfile[i]) / 1000);
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

// The canonical S: front reared, middle dipped, tail carried high. Spread
// along the chain so the WHOLE animal makes the shape, not three joints.
// `authority` in 1/1000 scales the whole pose, which is how the idle breathes
// and how the attack lets go of the S and becomes rigid.
inline void apply_stance(Rig& g, int32_t authority) {
  for (int k = 0; k < kSpineBones - 1; ++k) {
    const int t = (k * 1000) / (kSpineBones - 1);
    int32_t pitch;
    // Walk BACKWARD from the nose and think about where the body has to go.
    // Bone 0 is the nose and it is the root, so a rotation here moves the body
    // behind the head, not the head itself. To REAR THE HEAD the neck must
    // descend away from it; then the belly levels out along the ground; then
    // the tail rises again. Getting these two signs backwards made the animal
    // dive head-first with its tail in the air.
    if (t < 300) {
      pitch = kStanceRear;  // neck descends: the head is left carried high
    } else if (t < 620) {
      pitch = -kStanceDip;  // the belly levels out along the ground
    } else {
      pitch = -kStanceTailUp;  // and the tail rises behind
    }
    pitch = static_cast<int32_t>((static_cast<int64_t>(pitch) * authority) / 1000);
    g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(pitch / 3));
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
  c.frame_count = static_cast<uint16_t>(kIdleKeys);
  c.root.assign(static_cast<size_t>(kIdleKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kIdleKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kIdleKeys;
  for (int f = 0; f < kIdleKeys; ++f) {
    const int32_t ph = f * per_key;
    const int32_t s = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
    // the breath lags the bob, and the tail lags again: nothing is in phase
    const int32_t sl =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph - 9000) & 0xFFFF)}).raw;
    const int32_t st =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2 - 21000) & 0xFFFF)}).raw;

    Rig g;
    g.reset();
    // 1. the whole S gathers and relaxes with the breath
    apply_stance(g, 1000 + ((s * kIdleBreathe) >> 16));
    // the head lifts a little after the body does, so it is not rigid
    g.q[kBSpine0] = quat_mul(g.q[kBSpine0], quat_z(-((sl * 800) >> 16)));
    g.q[kBSpine0 + 1] = quat_mul(g.q[kBSpine0 + 1], quat_z(-((sl * 520) >> 16)));

    // 4. the tail plays: a lazy left-right sway spread over the rear third, so
    // the whole back half carries it rather than the fork alone
    const int32_t sway = (st * kIdleTailSway) >> 16;
    for (int k = (kSpineBones * 2) / 3; k < kSpineBones; ++k) {
      const int reach = ((k - (kSpineBones * 2) / 3) * 1000) / (kSpineBones / 3 + 1);
      g.q[kBSpine0 + k] = quat_mul(
          g.q[kBSpine0 + k],
          quat_y(static_cast<int32_t>((static_cast<int64_t>(sway) * (300 + reach)) / 1000)));
    }
    g.tail_rest(kBladeSplay + ((sl * 900) >> 16), kBladeRise + ((s * 500) >> 16));
    g.write(c, f);
    // 2. the vertical bob
    c.root[f * 3 + 1] = (s * kIdleLift) >> 16;
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
  c.frame_count = static_cast<uint16_t>(kWalkKeys);
  c.root.assign(static_cast<size_t>(kWalkKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kWalkKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kWalkKeys;
  for (int f = 0; f < kWalkKeys; ++f) {
    Rig g;
    g.reset();
    apply_stance(g, 1000);
    for (int k = 0; k < kSpineBones - 1; ++k) {
      const int32_t ph = f * per_key - (k * kWalkWaves * 65536) / (kSpineBones - 1);
      const int32_t s = zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
      const int32_t arch = (s * kWalkArch) >> 16;  // THE gait: pitch in X/Y
      const int32_t sway =
          (zref::fx_sin(zref::angle16{static_cast<uint16_t>((ph * 2) & 0xFFFF)}).raw * kWalkSway) >>
          16;
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_mul(quat_z(arch), quat_y(sway)));
    }
    const int32_t hb =
        (zref::fx_sin(zref::angle16{static_cast<uint16_t>((f * per_key) & 0xFFFF)}).raw *
         kWalkHeadBob) >>
        16;
    g.q[kBSpine0] = quat_mul(g.q[kBSpine0], quat_z(hb));
    g.q[kBSpine0 + 1] = quat_mul(g.q[kBSpine0 + 1], quat_z(hb / 2));
    g.tail_rest();
    g.write(c, f);
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
  c.frame_count = static_cast<uint16_t>(kAttackKeys);
  c.root.assign(static_cast<size_t>(kAttackKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kAttackKeys) * kBoneCount, zc::quat16_identity());

  // accumulated turn in 1/1000 of a full rotation: 3000 = three somersaults
  static const Key kSpin[] = {{0, 0},     {8, -160},  {18, 850},  {28, 1900},
                              {36, 3000}, {44, 3000}, {56, 3000}, {71, 3000}};
  // 1000 = fully coiled and relaxed, 0 = a rigid straight spear
  static const Key kCurl[] = {{0, 1000}, {8, 1000}, {18, 950}, {28, 800}, {36, 400},
                              {41, 60},  {45, 0},   {54, 0},   {60, 400}, {71, 1000}};
  // the descent: 0 airborne, 1000 driven into the ground
  static const Key kDrive[] = {{0, 0},     {38, 0},   {42, 200}, {45, 1000},
                               {54, 1000}, {60, 300}, {71, 0}};
  const int nS = static_cast<int>(sizeof(kSpin) / sizeof(Key));
  const int nC = static_cast<int>(sizeof(kCurl) / sizeof(Key));
  const int nD = static_cast<int>(sizeof(kDrive) / sizeof(Key));

  for (int f = 0; f < kAttackKeys; ++f) {
    Rig g;
    g.reset();
    const int spin = curve(kSpin, nS, f);
    const int curl = curve(kCurl, nC, f);
    const int drive = curve(kDrive, nD, f);
    // the S relaxes out of the animal as it commits
    apply_stance(g, curl);
    // the somersault: the accumulated turn spread over the whole chain
    const int32_t per_joint =
        static_cast<int32_t>((static_cast<int64_t>(spin) * 65536) / (1000 * (kSpineBones - 1)));
    for (int k = 0; k < kSpineBones - 1; ++k) {
      // while curled the rear carries more of the turn; as it straightens the
      // turn spreads evenly, and THAT is what makes the spear look rigid
      const int w = 1000 + (((k * 1000) / (kSpineBones - 1) - 500) * curl) / 1000;
      const int32_t a = static_cast<int32_t>((static_cast<int64_t>(per_joint) * w) / 1000);
      g.q[kBSpine0 + k] = quat_mul(g.q[kBSpine0 + k], quat_z(-a));
    }
    // the blades close to the spear line, then flare on impact
    const int32_t bl = (kBladeSplay * curl) / 1000 + (kBladeSplay * 2 * drive) / 3000;
    g.tail_rest(bl, (kBladeRise * curl) / 1000);
    g.write(c, f);
    c.root[f * 3 + 1] = -(drive * 260) / 1000;
  }
  c.events = {{45, zc::kEvAttack, 0}};
  return c;
}

// Slot 4 - FALLING FLAIL. Panicked and uncontrolled: the body corkscrews, head
// and tail counter-rotate, the blades flap, and nothing is a clean sine.
inline zc::Clip build_fall() {
  zc::Clip c;
  c.slot_id = 4;
  c.frame_count = static_cast<uint16_t>(kFallKeys);
  c.root.assign(static_cast<size_t>(kFallKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kFallKeys) * kBoneCount, zc::quat16_identity());
  const int32_t per_key = 65536 / kFallKeys;
  for (int f = 0; f < kFallKeys; ++f) {
    Rig g;
    g.reset();
    for (int k = 0; k < kSpineBones - 1; ++k) {
      // three incommensurate terms so the loop never reads as one sine, and
      // the head end counter-rotates against the tail end
      const int32_t p1 = f * per_key * 2 - k * (65536 / 7);
      const int32_t p2 = f * per_key * 3 + k * (65536 / 5);
      const int32_t p3 = f * per_key - k * (65536 / 11);
      const int32_t s1 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p1 & 0xFFFF)}).raw;
      const int32_t s2 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p2 & 0xFFFF)}).raw;
      const int32_t s3 = zref::fx_sin(zref::angle16{static_cast<uint16_t>(p3 & 0xFFFF)}).raw;
      const int sign = k < kSpineBones / 2 ? 1 : -1;
      g.q[kBSpine0 + k] =
          quat_mul(quat_z((s1 * 3100) >> 16),
                   quat_mul(quat_y((s2 * 2300 * sign) >> 16), quat_x((s3 * 1700) >> 16)));
    }
    const int32_t fl =
        zref::fx_sin(zref::angle16{static_cast<uint16_t>((f * per_key * 4) & 0xFFFF)}).raw;
    g.tail_rest(kBladeSplay + ((fl * 2600) >> 16), kBladeRise + ((fl * 1200) >> 16));
    g.write(c, f);
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
    const int32_t ex = station_x(kEyeStation);
    sk.bones[kBEyeL] = zc::Bone{kBSpine0, -fxm(ex), fxm(kEyeUp), fxm(kEyeOut)};
    sk.bones[kBEyeR] = zc::Bone{kBSpine0, -fxm(ex), fxm(kEyeUp), -fxm(kEyeOut)};

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
      set_rgb(p, kGreen);
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
        const Bind bd = station_bind(i);
        zc::RingSpec rs;
        rs.y = fxm(station_x(i));
        rs.radius = fxm(r);
        rs.segments = static_cast<uint8_t>(kSides);
        rs.b0 = bd.b0;
        rs.b1 = bd.b1;
        rs.w0 = bd.w0;
        rs.rx = fxm(r * station_wide(i) / 100);
        rs.rz = fxm(r);
        rs.cz = -fxm(kBodyY);
        p.rings.push_back(rs);
      }
      set_rgb(p, kBlue);
      parts.push_back(p);
    }

    // ---- DORSAL CREST: a third chain, same bones, riding on the back -----
    // The concept's pink runs ALONG the animal; a ring part's texture runs
    // AROUND it, so until the texture lane lands the stripe is geometry. It
    // shares the body's binds, which is what keeps it welded through any bend.
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
        rs.radius = fxm(r * kCrestNum / 100);
        rs.segments = 8;
        rs.b0 = bd.b0;
        rs.b1 = bd.b1;
        rs.w0 = bd.w0;
        rs.rx = fxm(r * kCrestNum / 100);
        rs.rz = fxm(r * kCrestNum / 140);
        rs.cz = -fxm(kBodyY + r * kCrestLift / 100);  // UP is NEGATIVE cz
        p.rings.push_back(rs);
      }
      set_rgb(p, kPink);
      parts.push_back(p);
    }

    // ---- EYES: enormous, orange-ringed -----------------------------------
    for (int side = 0; side < 2; ++side) {
      const uint8_t bone = side == 0 ? kBEyeL : kBEyeR;
      const uint8_t yq = side == 0 ? 0 : 2;  // +Z or -Z
      zc::RingPart rim;
      rim.rings = {{-fxm(58), fxm(kEyeRimR * 60 / 100), 12},
                   {fxm(8), fxm(kEyeRimR), 12},
                   {fxm(60), fxm(kEyeRimR * 66 / 100), 12}};
      rim.caps = zc::kCapBot;
      rim.pitch_q = 1;
      rim.yaw_q = yq;
      rim.bone = bone;
      set_rgb(rim, kOrange);
      parts.push_back(rim);

      zc::RingPart eye;
      eye.rings = {{-fxm(30), fxm(kEyeR * 58 / 100), 12},
                   {fxm(40), fxm(kEyeR), 12},
                   {fxm(100), fxm(kEyeR * 66 / 100), 12},
                   {fxm(kEyeBulge), fxm(kEyeR * 30 / 100), 12}};
      eye.caps = zc::kCapTop | zc::kCapBot;
      eye.pitch_q = 1;
      eye.yaw_q = yq;
      eye.bone = bone;
      set_rgb(eye, kYellow);
      parts.push_back(eye);
    }

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
      set_rgb(p, kGreen);
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
    return type;
  }();
  return t;
}

}  // namespace zixx

#endif  // ZHAO_REEL_ZIXXTRIXX_H
