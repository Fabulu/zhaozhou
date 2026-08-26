// Zixxtrixx — the first Upheaval creature.
//
// Built from S. Hofer's two concept sheets (Upheaval/creature/Zixxtrixx/
// Concept/{Front,Side}.png). Nothing here is copied from the donor; the laws
// it obeys live in spec/creature_rules.md and zref_creature.hpp, and this file
// is pure AUTHORING — which rings, which bones, which keys.
//
// ---------------------------------------------------------------------------
// HOW TO TURN THE KNOBS
//
//   1. edit a constant in the KNOBS block below
//   2. cmake --build build --target zhao-reel
//   3. build\tools\zhao-reel.exe <out-dir> zixxtrixx-slither
//
// Everything Fabian is likely to want to change is a named constant in one
// place. Nothing below the KNOBS block invents a number.
// ---------------------------------------------------------------------------
//
// THREE DELIBERATE DEPARTURES FROM THE CONCEPT ART, all requested:
//   * the eyes are much bigger than drawn (kEyeR vs the head's kHeadRMax)
//   * the three tail prongs are BLOCKY — kProngSegs = 4, a square cross
//     section, instead of a round one
//   * the head is exaggerated: kHeadRMax is over 1.5x the body's kBodyR0 and
//     the neck PINCHES into it (kNeckR1 < kNeckR0) so the head reads as a
//     separate object at 240p rather than a bulge in a tube.
//
// STRUCTURAL NOTES (read before editing, they are not obvious):
//   * Bone rest rotations are IDENTITY — Bone is {parent, tx, ty, tz}, a pure
//     translation chain. There is no bind rotation, so the prong splay is
//     baked into every frame of every clip. That is why kProngSplay appears in
//     the clip builders and not in the skeleton.
//   * Ring stacks must stay ASCENDING in local y. The zipper fixes winding
//     from ring order, so a descending stack builds inside-out. Direction
//     comes from the quarter turns instead: {pitch_q=1,yaw_q=1} = +X,
//     {1,3} = -X, {1,0} = +Z, {1,2} = -Z, {0,0} = +Y.
//   * One part = one bone bounds how a part BENDS, not how many parts a bone
//     may carry. compile_creature only checks `bone < bone_count`. That is
//     what makes the eyes, the rims, the ridge and the two-tone prongs
//     affordable: they cost meshlets, not bones, and bones are the scarce
//     resource for a serpent.

#ifndef ZHAO_REEL_ZIXXTRIXX_H
#define ZHAO_REEL_ZIXXTRIXX_H

namespace zixx {

// ============================== KNOBS ======================================
// Lengths in millimetres (fxm converts to fx16). Angles in angle16 units:
// 65536 = one full turn, so 182 units ~ 1 degree.

// -- overall scale ----------------------------------------------------------
// Fabian: "a smaller scale creature, but that still means relatively big in
// Sacrifice terms." The donor's posed peasant is 1.70 units. Zixxtrixx is
// ~3.9 m nose to prong-tip and carries its head ~0.5 m up, so it is long
// rather than tall — a large low-tier creature, not a tier-8 one.
constexpr int32_t kSegLen = 270;    // one body segment
constexpr int kBodySegs = 10;       // segments == bend smoothness (bones!)
constexpr int32_t kBodyR0 = 205;    // body radius at the neck end
constexpr int32_t kBodyR1 = 84;     // body radius at the fork end
constexpr int32_t kBodyY = 232;     // body axis height above ground
constexpr int kBodySegsRound = 8;   // ring segments around the body

// -- neck and head ----------------------------------------------------------
constexpr int32_t kNeckLen = 340;   // root to head bone
constexpr int32_t kHeadRise = 132;   // how much higher the head axis sits
constexpr int32_t kNeckR0 = 202;    // neck radius at the body
constexpr int32_t kNeckR1 = 168;    // neck radius at the head — PINCHED
constexpr int32_t kHeadRMax = 335;  // widest point of the skull
constexpr int32_t kHeadBack = 150;  // skull extent behind the head bone
constexpr int32_t kHeadFwd = 415;   // skull extent in front of the head bone

// -- the pink dorsal cap on the skull --------------------------------------
constexpr int32_t kCapX = 10;       // cap bone, forward of the head bone
constexpr int32_t kCapY = 236;      // cap bone, above the head bone
constexpr int32_t kCapR = 150;      // widest point of the cap

// -- eyes (deliberately oversized) -----------------------------------------
constexpr int32_t kEyeX = 238;       // forward of the head bone
constexpr int32_t kEyeY = 96;       // above the head bone
constexpr int32_t kEyeZ = 172;      // out to the side
constexpr int32_t kEyeR = 132;      // the yellow eyeball
constexpr int32_t kEyeRimR = 158;   // the orange rim behind it
constexpr int32_t kEyeBulge = 196;  // how far the eyeball stands out

// -- mouth ------------------------------------------------------------------
// REMOVED. The concept sheet has a mouth slit; at 384x240 it was two pixels
// the skull swallowed, and it cost both a bone and a whole shading band
// against the 256-colour law. The oversized eyes carry the face instead. If
// the creature is ever shot in close-up, put it back.

// -- the pink dorsal ridge along the body ----------------------------------
// One extra bone per ridged segment. kRidgeSegs < kBodySegs so the crest fades
// out where the body gets thin, which is also what the concept sheet shows.
constexpr int kRidgeSegs = 8;
constexpr int32_t kRidgeLiftNum = 62;   // ridge bone height = r * 62/100
constexpr int32_t kRidgeRNum = 44;      // ridge radius   = r * 44/100

// -- the three tail prongs (blocky) ----------------------------------------
constexpr int kProngSegs = 4;         // 4 = square cross section = BLOCKY
constexpr int32_t kProngLen = 830;    // the two long prongs
constexpr int32_t kProngLenMid = 400; // the short middle one
constexpr int32_t kProngR0 = 96;
constexpr int32_t kProngR1 = 18;
constexpr int32_t kProngTipFrom = 60;  // % of the prong that is the pink tip
constexpr int32_t kProngSplay = 4200;  // +-23 degrees, in angle16 units
constexpr int32_t kProngSplayMid = 900;

// -- colours, sampled from the concept scans -------------------------------
// Median RGB over named regions of both sheets, paper and ink excluded. The
// two sheets agree to within a few counts, which is why these are used raw.
constexpr uint8_t kGreen[3] = {116, 205, 147};   // flank
constexpr uint8_t kPink[3] = {228, 146, 194};    // dorsal, SATURATED
// from the sheet's 226,203,221: at 240p under the scene light the raw
// pencil pink resolved to near-white and the crest read as a grey helmet.
// Hue kept, saturation pushed. Set it back to 226,203,221 to see why.
constexpr uint8_t kBlue[3] = {20, 163, 213};     // head
constexpr uint8_t kYellow[3] = {250, 226, 92};   // eye, SATURATED
// from the sheet's 246,236,167, for the same reason as kPink: the raw
// pencil yellow came out olive under the scene light and the eye stopped
// reading as an eye.
constexpr uint8_t kOrange[3] = {212, 121, 96};   // eye rim
// The concept blends blue into green under the chin. That transition cost a
// whole shading band against the 256-colour ceiling and was invisible at
// 240p, so the neck simply carries the head's blue.

// -- animation --------------------------------------------------------------
// A key is held 2 sim ticks, so REEL FRAMES = KEYS x 2 at step 1. Both clip
// lengths are chosen against the donor medians (locomotion 34 keys, melee 45)
// AND against the orbit: the subject's frame count must be a whole number of
// cycles or the GIF will not loop.
constexpr int kSlitherKeys = 32;   // 64 frames per cycle, 1.07 s
constexpr int kAttackKeys = 48;    // 96 frames, 1.60 s — donor melee median 45
constexpr int kSlitherWaves = 2;   // Fabian: two waves
constexpr int32_t kSlitherAmp = 3750;   // per-joint lateral yaw, angle16
constexpr int32_t kSlitherHeadHold = 62;  // % of the wave the head cancels
constexpr int32_t kAttackArc = 2900;    // per-joint tail pitch at full arc
constexpr int32_t kAttackHead = 2600;   // head pitch authority
constexpr uint16_t kAttackStrikeKey = 23;  // where kEvAttack fires

// -- how it travels ---------------------------------------------------------
// The orbit camera yaws the WORLD about the origin, so a creature that
// travels must cross the origin or it swings out of frame. Zixxtrixx starts
// kSlitherStartBack behind it and slithers through.
constexpr int32_t kSlitherSpeed = 14;       // mm per reel frame
constexpr int32_t kSlitherStartBack = 900; // mm behind the origin at frame 0

// ============================ END KNOBS ====================================

// Rotation about a principal axis, as a quat16. `a` is a full angle in
// angle16 units; the quaternion takes the HALF angle (the watchdog's
// convention, and the reason every amplitude above is doubled relative to the
// visible swing).
inline zc::quat16 quat_x(int32_t a) {
  const zref::angle16 h{static_cast<uint16_t>((a >> 1) & 0xFFFF)};
  return zc::quat16_axis_angle(zref::fx16{1 << 16}, zref::fx16{0}, zref::fx16{0}, zref::fx_sin(h),
                               zref::fx_cos(h));
}
inline zc::quat16 quat_y(int32_t a) {
  const zref::angle16 h{static_cast<uint16_t>((a >> 1) & 0xFFFF)};
  return zc::quat16_axis_angle(zref::fx16{0}, zref::fx16{1 << 16}, zref::fx16{0}, zref::fx_sin(h),
                               zref::fx_cos(h));
}
inline zc::quat16 quat_z(int32_t a) {
  const zref::angle16 h{static_cast<uint16_t>((a >> 1) & 0xFFFF)};
  return zc::quat16_axis_angle(zref::fx16{0}, zref::fx16{0}, zref::fx16{1 << 16}, zref::fx_sin(h),
                               zref::fx_cos(h));
}

// Piecewise-linear keyed curve in thousandths. Integer throughout: the value
// between two keys is a rounded linear blend, and it clamps at both ends.
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
      const int d = f - k[i].f;
      return k[i].v + ((k[i + 1].v - k[i].v) * d + span / 2) / span;
    }
  }
  return k[n - 1].v;
}

// ------------------------------------------------------------- bone map ----
// Parent-before-child is REQUIRED and validated at bake, so the order here is
// load-bearing, not cosmetic.
enum : uint8_t {
  kBRoot = 0,   // at the front of the body, on the body axis
  kBHead = 1,   // child of root: forward and raised by kHeadRise
  kBSpine0 = 2, // kBodySegs spine joints, kBSpine0 + j
  kBFork = static_cast<uint8_t>(kBSpine0 + kBodySegs),  // 12
  kBProngA = static_cast<uint8_t>(kBFork + 1),          // 13 upper
  kBProngB = static_cast<uint8_t>(kBFork + 2),          // 14 lower
  kBProngC = static_cast<uint8_t>(kBFork + 3),          // 15 middle, short
  kBCap = static_cast<uint8_t>(kBFork + 4),             // 16 skull cap
  kBEyeL = static_cast<uint8_t>(kBFork + 5),            // 17
  kBEyeR = static_cast<uint8_t>(kBFork + 6),            // 18
  kBRidge0 = static_cast<uint8_t>(kBFork + 7),          // 19, kRidgeSegs of them
  kBoneCount = static_cast<uint8_t>(kBRidge0 + kRidgeSegs)  // 28
};
static_assert(kBoneCount <= 32, "creature_rules 1.2: <= 32 bones");

// Body radius at spine joint j (linear taper; taper is free in this format).
inline int32_t body_r(int j) {
  if (j <= 0) return kBodyR0;
  if (j >= kBodySegs) return kBodyR1;
  return kBodyR0 + ((kBodyR1 - kBodyR0) * j + kBodySegs / 2) / kBodySegs;
}

inline void set_rgb(zc::RingPart& p, const uint8_t c[3]) {
  p.r = c[0];
  p.g = c[1];
  p.b = c[2];
}

// ---------------------------------------------------------------- clips ----

// Slither: a travelling lateral wave. Each spine joint yaws about Y on the
// same sinusoid, phase-shifted down the body so kSlitherWaves complete waves
// live on the creature at once. The head cancels most of its own joint's
// sway so it stays pointed along the direction of travel instead of whipping.
inline zc::Clip build_slither() {
  zc::Clip c;
  c.slot_id = 1;
  c.frame_count = static_cast<uint16_t>(kSlitherKeys);
  c.root.assign(static_cast<size_t>(kSlitherKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kSlitherKeys) * kBoneCount, zc::quat16_identity());

  const int32_t per_key = 65536 / kSlitherKeys;
  const int32_t per_seg = (kSlitherWaves * 65536) / kBodySegs;

  for (int f = 0; f < kSlitherKeys; ++f) {
    const size_t base = static_cast<size_t>(f) * kBoneCount;
    int32_t head_sway = 0;
    for (int j = 0; j < kBodySegs; ++j) {
      const zref::angle16 ph{static_cast<uint16_t>((f * per_key - j * per_seg) & 0xFFFF)};
      const int32_t a = (zref::fx_sin(ph).raw * kSlitherAmp) >> 16;
      c.quats[base + kBSpine0 + j] = quat_y(a);
      if (j == 0) head_sway = a;
    }
    // the head holds its line against the first joint's sway
    c.quats[base + kBHead] = quat_y(-(head_sway * kSlitherHeadHold) / 100);
    // a gentle roll on the fork so the prongs are not a dead stick
    const zref::angle16 fp{static_cast<uint16_t>((f * per_key * 2) & 0xFFFF)};
    c.quats[base + kBFork] = quat_y((zref::fx_sin(fp).raw * 900) >> 16);
    // the prong splay is baked in: rest rotations are identity, so there is
    // nowhere else for it to live
    c.quats[base + kBProngA] = quat_z(kProngSplay);
    c.quats[base + kBProngB] = quat_z(-kProngSplay);
    c.quats[base + kBProngC] = quat_z(kProngSplayMid);
  }
  c.events = {{0, zc::kEvFoot, 0},
              {static_cast<uint16_t>(kSlitherKeys / 2), zc::kEvFoot, 1}};
  return c;
}

// Attack: Zixxtrixx does not bite. It rears, throws its TAIL up and over its
// own head, and drives the prongs down into whatever is in front of it —
// Fabian: "it attacks by stabbing things in the head with its tail and driving
// them to the ground." So the tail is the weapon and the head is the pivot.
//
// The arc is per-joint pitch about Z, weighted toward the rear of the body, so
// the far end of the animal travels furthest. The head ducks as the tail
// passes over it, then presses down with the strike.
inline zc::Clip build_attack() {
  zc::Clip c;
  c.slot_id = 2;
  c.frame_count = static_cast<uint16_t>(kAttackKeys);
  c.root.assign(static_cast<size_t>(kAttackKeys) * 3, 0);
  c.quats.assign(static_cast<size_t>(kAttackKeys) * kBoneCount, zc::quat16_identity());

  // tail arc, thousandths: negative pitch lifts the tail (a point at -X
  // rotated about +Z by a negative angle rises)
  static const Key kArc[] = {{0, 0},    {6, 150},   {14, -700}, {20, -1000}, {23, 480},
                             {27, 660}, {34, 590},  {41, -140}, {47, 0}};
  // head pitch: rears up, then ducks under the passing tail and presses down
  static const Key kHead[] = {{0, 0},     {8, -300},  {16, -540}, {20, -570}, {23, -60},
                              {26, 430},  {34, 390},  {42, -70},  {47, 0}};
  // lateral coil: the body gathers before the throw and unwinds after
  static const Key kCoil[] = {{0, 1000}, {10, 420}, {20, 240}, {27, 300}, {40, 720}, {47, 1000}};
  const int nA = static_cast<int>(sizeof(kArc) / sizeof(Key));
  const int nH = static_cast<int>(sizeof(kHead) / sizeof(Key));
  const int nC = static_cast<int>(sizeof(kCoil) / sizeof(Key));

  const int32_t per_key = 65536 / kAttackKeys;
  const int32_t per_seg = (kSlitherWaves * 65536) / kBodySegs;

  for (int f = 0; f < kAttackKeys; ++f) {
    const size_t base = static_cast<size_t>(f) * kBoneCount;
    const int arc = curve(kArc, nA, f);
    const int head = curve(kHead, nH, f);
    const int coil = curve(kCoil, nC, f);

    for (int j = 0; j < kBodySegs; ++j) {
      // rear-weighted: joint 0 barely moves, the fork end carries the throw
      const int32_t w = (j * 1000 + kBodySegs / 2) / kBodySegs;
      const int32_t pitch = static_cast<int32_t>(
          (static_cast<int64_t>(kAttackArc) * arc / 1000) * w / 1000);
      // the residual lateral wave keeps it alive during the wind-up
      const zref::angle16 ph{static_cast<uint16_t>((f * per_key - j * per_seg) & 0xFFFF)};
      const int32_t lat = (((zref::fx_sin(ph).raw * kSlitherAmp) >> 16) * coil) / 1000;
      // one axis per bone: the pitch owns the throw, so the lateral term only
      // takes the joints the throw has not claimed
      c.quats[base + kBSpine0 + j] =
          (pitch > 600 || pitch < -600) ? quat_z(pitch) : quat_y(lat);
    }
    c.quats[base + kBHead] = quat_z((kAttackHead * head) / 1000);
    c.quats[base + kBFork] = quat_z((kAttackArc * arc) / 1000);
    c.quats[base + kBProngA] = quat_z(kProngSplay);
    c.quats[base + kBProngB] = quat_z(-kProngSplay);
    c.quats[base + kBProngC] = quat_z(kProngSplayMid);
  }
  c.events = {{kAttackStrikeKey, zc::kEvAttack, 0}};
  return c;
}

// ------------------------------------------------------------ the build ----

inline const zc::CreatureType& type() {
  static const zc::CreatureType t = [] {
    zc::Skeleton sk;
    sk.bone_count = kBoneCount;

    sk.bones[kBRoot] = zc::Bone{kBRoot, 0, fxm(kBodyY), 0};
    sk.bones[kBHead] = zc::Bone{kBRoot, fxm(kNeckLen), fxm(kHeadRise), 0};
    // spine joint 0 sits ON the root: it is the first BENDABLE joint, and
    // giving it a zero offset means the body starts where the neck ends
    // instead of a segment behind it.
    for (int j = 0; j < kBodySegs; ++j) {
      sk.bones[kBSpine0 + j] =
          zc::Bone{static_cast<uint8_t>(j == 0 ? kBRoot : (kBSpine0 + j - 1)),
                   j == 0 ? 0 : -fxm(kSegLen), 0, 0};
    }
    sk.bones[kBFork] =
        zc::Bone{static_cast<uint8_t>(kBSpine0 + kBodySegs - 1), -fxm(kSegLen), 0, 0};
    sk.bones[kBProngA] = zc::Bone{kBFork, 0, fxm(28), 0};
    sk.bones[kBProngB] = zc::Bone{kBFork, 0, -fxm(28), 0};
    sk.bones[kBProngC] = zc::Bone{kBFork, 0, 0, 0};
    sk.bones[kBCap] = zc::Bone{kBHead, fxm(kCapX), fxm(kCapY), 0};
    sk.bones[kBEyeL] = zc::Bone{kBHead, fxm(kEyeX), fxm(kEyeY), fxm(kEyeZ)};
    sk.bones[kBEyeR] = zc::Bone{kBHead, fxm(kEyeX), fxm(kEyeY), -fxm(kEyeZ)};
    for (int j = 0; j < kRidgeSegs; ++j) {
      sk.bones[kBRidge0 + j] = zc::Bone{static_cast<uint8_t>(kBSpine0 + j), 0,
                                        fxm(body_r(j) * kRidgeLiftNum / 100), 0};
    }

    std::vector<zc::RingPart> parts;

    // neck: root -> head, PINCHING inward so the skull reads as its own object
    {
      zc::RingPart p;
      p.rings = {{0, fxm(kNeckR0), kBodySegsRound},
                 {fxm(kNeckLen / 2), fxm((kNeckR0 + kNeckR1) / 2), kBodySegsRound},
                 {fxm(kNeckLen), fxm(kNeckR1), kBodySegsRound}};
      p.caps = zc::kCapBot;
      p.pitch_q = 1;
      p.yaw_q = 1;  // +X, forward
      p.bone = kBRoot;
      set_rgb(p, kBlue);
      parts.push_back(p);
    }

    // skull: five rings, widest just behind the eyes
    {
      zc::RingPart p;
      // proportional to kHeadRMax so that knob actually reshapes the skull
      p.rings = {{-fxm(kHeadBack), fxm(kHeadRMax * 34 / 100), kBodySegsRound},
                 {-fxm(48), fxm(kHeadRMax * 74 / 100), kBodySegsRound},
                 {fxm(70), fxm(kHeadRMax), kBodySegsRound},
                 {fxm(196), fxm(kHeadRMax * 93 / 100), kBodySegsRound},
                 {fxm(320), fxm(kHeadRMax * 60 / 100), kBodySegsRound},
                 {fxm(kHeadFwd), fxm(kHeadRMax * 27 / 100), kBodySegsRound}};
      p.caps = zc::kCapTop | zc::kCapBot;
      p.pitch_q = 1;
      p.yaw_q = 1;
      p.bone = kBHead;
      set_rgb(p, kBlue);
      parts.push_back(p);
    }

    // the pink cap over the skull (the concept's dorsal colour, as geometry —
    // there is no texture page pipeline yet, so a stripe is geometry or it is
    // nothing)
    {
      zc::RingPart p;
      p.rings = {{-fxm(196), fxm(kCapR * 60 / 100), kBodySegsRound},
                 {-fxm(70), fxm(kCapR * 92 / 100), kBodySegsRound},
                 {fxm(70), fxm(kCapR), kBodySegsRound},
                 {fxm(196), fxm(kCapR * 78 / 100), kBodySegsRound},
                 {fxm(292), fxm(kCapR * 34 / 100), kBodySegsRound}};
      p.caps = zc::kCapTop | zc::kCapBot;
      p.pitch_q = 1;
      p.yaw_q = 1;
      p.bone = kBCap;
      set_rgb(p, kPink);
      parts.push_back(p);
    }

    // eyes: orange rim first, yellow eyeball standing proud of it. Both ride
    // the same eye bone — parts share bones freely, which is the whole reason
    // an oversized eye costs nothing against the 32-bone cap.
    for (int side = 0; side < 2; ++side) {
      const uint8_t bone = side == 0 ? kBEyeL : kBEyeR;
      const uint8_t yq = side == 0 ? 0 : 2;  // +Z or -Z
      zc::RingPart rim;
      rim.rings = {{-fxm(70), fxm(kEyeRimR * 58 / 100), kBodySegsRound},
                   {fxm(10), fxm(kEyeRimR), kBodySegsRound},
                   {fxm(74), fxm(kEyeRimR * 66 / 100), kBodySegsRound}};
      rim.caps = zc::kCapBot;
      rim.pitch_q = 1;
      rim.yaw_q = yq;
      rim.bone = bone;
      set_rgb(rim, kOrange);
      parts.push_back(rim);

      zc::RingPart eye;
      eye.rings = {{-fxm(34), fxm(kEyeR * 58 / 100), kBodySegsRound},
                   {fxm(46), fxm(kEyeR), kBodySegsRound},
                   {fxm(116), fxm(kEyeR * 65 / 100), kBodySegsRound},
                   {fxm(kEyeBulge), fxm(kEyeR * 31 / 100), kBodySegsRound}};
      eye.caps = zc::kCapTop | zc::kCapBot;
      eye.pitch_q = 1;
      eye.yaw_q = yq;
      eye.bone = bone;
      set_rgb(eye, kYellow);
      parts.push_back(eye);
    }

    // body: one part per spine joint, each running backward from its own bone
    for (int j = 0; j < kBodySegs; ++j) {
      zc::RingPart p;
      p.rings = {{0, fxm(body_r(j)), kBodySegsRound},
                 {fxm(kSegLen), fxm(body_r(j + 1)), kBodySegsRound}};
      p.caps = j == kBodySegs - 1 ? zc::kCapTop : 0;
      p.pitch_q = 1;
      p.yaw_q = 3;  // -X, backward
      p.bone = static_cast<uint8_t>(kBSpine0 + j);
      // the concept blends the blue chin into the green flank over the first
      // segment
      set_rgb(p, kGreen);
      parts.push_back(p);
    }

    // the dorsal ridge: the concept's pink stripe, built as geometry
    for (int j = 0; j < kRidgeSegs; ++j) {
      zc::RingPart p;
      p.rings = {{0, fxm(body_r(j) * kRidgeRNum / 100), 6},
                 {fxm(kSegLen), fxm(body_r(j + 1) * kRidgeRNum / 100), 6}};
      p.caps = j == kRidgeSegs - 1 ? zc::kCapTop : 0;
      p.pitch_q = 1;
      p.yaw_q = 3;
      p.bone = static_cast<uint8_t>(kBRidge0 + j);
      set_rgb(p, kPink);
      parts.push_back(p);
    }

    // the three prongs. Each is TWO parts on ONE bone: a green shaft and a
    // pink tip, which is free because parts share bones.
    for (int k = 0; k < 3; ++k) {
      const uint8_t bone = k == 0 ? kBProngA : (k == 1 ? kBProngB : kBProngC);
      const int32_t len = k == 2 ? kProngLenMid : kProngLen;
      const int32_t split = len * kProngTipFrom / 100;
      const int32_t r_split = kProngR0 + (kProngR1 - kProngR0) * kProngTipFrom / 100;

      zc::RingPart shaft;
      shaft.rings = {{0, fxm(kProngR0), kProngSegs}, {fxm(split), fxm(r_split), kProngSegs}};
      shaft.caps = zc::kCapBot;
      shaft.pitch_q = 1;
      shaft.yaw_q = 3;
      shaft.bone = bone;
      set_rgb(shaft, kGreen);
      parts.push_back(shaft);

      zc::RingPart tip;
      tip.rings = {{fxm(split), fxm(r_split), kProngSegs}, {fxm(len), fxm(kProngR1), kProngSegs}};
      tip.caps = zc::kCapTop;
      tip.pitch_q = 1;
      tip.yaw_q = 3;
      tip.bone = bone;
      set_rgb(tip, kPink);
      parts.push_back(tip);
    }

    zc::ClipBank bank;
    bank.bone_count = kBoneCount;
    bank.clips.push_back(build_slither());
    bank.clips.push_back(build_attack());

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
