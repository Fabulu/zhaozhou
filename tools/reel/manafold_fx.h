// Unnamed02 — the effects: THE MANA MENU (pass 2), the FX.LIGHTNING bolt,
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

#ifndef ZHAO_REEL_UNNAMED02_FX_H
#define ZHAO_REEL_UNNAMED02_FX_H

#include <vector>

#include "unnamed02_art.h"

namespace u02 {

// ---- deterministic hash (the crackle's clock; zlib-free, integer) ---------
inline uint32_t fx_hash(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 0x9E3779B9u ^ b * 0x85EBCA6Bu ^ c * 0xC2B2AE35u;
  h ^= h >> 15;
  h *= 0x2545F491u;
  h ^= h >> 13;
  return h;
}
inline int32_t fx_jit(uint32_t h, int32_t amp_mm) {
  if (amp_mm <= 0) return 0;
  return static_cast<int32_t>(h % static_cast<uint32_t>(2 * amp_mm + 1)) - amp_mm;
}

// ---- the anchors (posed bone origins, world fx16) -------------------------
struct FxAnchors {
  int32_t body[3];    // kBRoot origin (the belly light)
  int32_t crown[3];   // the body's top pole
  int32_t neck[3];    // the neck exit (pass 3: pulls the ring centre DOWN)
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
constexpr uint8_t kManaSeaGreenMid[3] = {26, 158, 92};
constexpr uint8_t kManaSeaGreenHi[3] = {150, 240, 180};
constexpr uint8_t kManaDeepBlueMid[3] = {28, 62, 195};
constexpr uint8_t kManaDeepBlueHi[3] = {130, 175, 255};
constexpr uint8_t kManaWhiteMid[3] = {160, 160, 190};
constexpr uint8_t kManaWhiteHi[3] = {245, 240, 255};
constexpr uint8_t kManaDripMid[3] = {40, 62, 150};   // the opaque deep blue
constexpr uint8_t kManaDripHi[3] = {90, 130, 220};
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
  kRampCount
};

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
constexpr int32_t kBoltHaloRPx = 7;
constexpr int kStreakGainPm = 420;         // the anamorphic strike flash
constexpr int32_t kStreakSpanPx = 46;
// PASS 3 (R9): lightning is 2-3 CONTINUOUS strands that BUZZ across the
// ring pocket's middle — not periodic strikes that vanish. Each strand is
// a two-layer stamped path (hot near-white core over a calm additive
// halo), re-hashed every kBoltRehashFrames so it visibly buzzes, ghosting
// through the smear plane so old paths decay instead of blinking out.
// Stamp spacing is the checkable gate: zero visible gaps at native.
constexpr int kStrandCount = 3;
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
constexpr SmearPreset kSmearPresets[4] = {
    {0, 1, 0, 1, 0},            // 0: no smear
    {620, 2, 40, 90, 420},      // 1: SHORT/CLEAN — a readable tail, tidy
    {820, 4, 90, 260, 520},     // 2: MID/GLITCHY — the lead (aqua) rung
    {900, 6, 160, 430, 520},    // 3: LONG/GLITCHIER — the far end (cyan)
};
constexpr int kSmearW = 96, kSmearH = 60;  // quarter-res: the fill budget

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
};

inline void mana_push(std::vector<ManaSplat>& out, int32_t x, int32_t y, int32_t z,
                      int32_t r_px, uint8_t ramp, int gain_pm, bool depth_test,
                      bool pre, bool opaque = false) {
  if (gain_pm <= 0 || r_px <= 0) return;
  out.push_back(ManaSplat{x, y, z, r_px, ramp, static_cast<int16_t>(gain_pm),
                          depth_test, opaque, pre});
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
    const int flick = 880 + static_cast<int>(hf % 240u);
    bolt_stamp(out, pts, kBoltSegs, kBoltCoreGainPm * flick / 1000,
               kBoltHaloGainPm * flick / 1000);
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

/** Fill the frame's mana splats for one conduit. `cand` selects the menu
 *  candidate (0 = none). PASS 3 (R13): drip is CUT (dead in 579 of 600
 *  frames); the menu is 1 pulsar, 2 filled deep blue, 3 aquamarine
 *  smeared plasma (the lead), 4 lightning strands (the channel's blaze —
 *  identity, not a menu item), 5 the boil CENTRE grown, 6 cyan smeared
 *  plasma (the long/glitchier smear rung), 7 filled sea-green, 8 THE
 *  STACK (pulsar + strands + aqua smear — the likely shipping stack,
 *  judged assembled). Every body is filled (R7) and centre-anchored (R8). */
inline void mana_fill(int cand, uint32_t frame, const FxAnchors& A,
                      std::vector<ManaSplat>& out) {
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
    case 3:  // aquamarine smeared plasma — THE LEAD (R13 #1)
      mana_bullets(frame, A, kRampAqua, out);
      break;
    case 6:  // cyan smeared plasma — the long/glitchier rung (R13 #2)
      mana_bullets(frame, A, kRampCyan, out);
      break;
    case 4:
      mana_lightning(frame, A, out);
      break;
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
    case 8: {  // THE STACK: caged pulsar + buzzing strands + aqua smear
      mana_fill(1, frame, A, out);
      mana_lightning(frame, A, out);
      mana_bullets(frame, A, kRampAqua, out);
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
  glow_build_ramp(ramps[kRampAqua], kBlack, kManaAquaMid, kManaAquaHi, 1000);
  glow_build_ramp(ramps[kRampSeaGreen], kBlack, kManaSeaGreenMid, kManaSeaGreenHi, 1000);
  glow_build_ramp(ramps[kRampDeepBlue], kBlack, kManaDeepBlueMid, kManaDeepBlueHi, 1000);
}

/** One additive glow splat at canvas (cx,cy), half-size r px, depth-tested
 *  against the given centre depth (Q16.16 1/w), never writing depth.
 *  `bloom` selects the Lorentzian plasma profile; `opaque` writes the ramp
 *  colour instead of adding (the drip's solid read) where the sprite is
 *  bright enough to be a body rather than a fringe. */
inline void glow_splat(uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h,
                       const GlowAssets& g, const GlowFrame& f, int32_t cx, int32_t cy,
                       int32_t r, int32_t centre_d, bool depth_test = true,
                       bool bloom = false, bool opaque = false) {
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

/** The smear plane's per-frame decay/glitch update (R6). Call ONCE per
 *  frame before feeding: applies the quantised decay step, the per-cell
 *  retention jitter, and the staggered bounded hard clear. */
inline void smear_update(uint8_t* buf, uint32_t frame, const SmearPreset& sp) {
  if (sp.gain_pm <= 0) return;
  const bool step = sp.step_frames <= 1 || (frame % static_cast<uint32_t>(sp.step_frames)) == 0;
  for (int i = 0; i < kSmearW * kSmearH; ++i) {
    uint8_t* c = buf + static_cast<size_t>(i) * 3;
    // the bounded hard clear, staggered per cell: PROOF the buffer resets
    if (sp.hard_clear_frames > 1) {
      const uint32_t hc = fx_hash(0x5EEDC1EAu, static_cast<uint32_t>(i), 3u);
      if ((frame + hc) % static_cast<uint32_t>(sp.hard_clear_frames) == 0) {
        c[0] = c[1] = c[2] = 0;
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
inline void smear_feed(uint8_t* buf, const GlowAssets& g, const GlowFrame& f,
                       int32_t cx, int32_t cy, int32_t r) {
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
 *  over the bright sky (R7 for the trails). No depth test: a persistence
 *  plane remembers pixels, not geometry. */
inline void smear_composite(const uint8_t* buf, uint8_t* rgb, uint32_t w, uint32_t h,
                            int gain_pm) {
  if (gain_pm <= 0) return;
  for (uint32_t y = 0; y < h; ++y) {
    const uint8_t* row = buf + (static_cast<size_t>(y / 4) * kSmearW) * 3;
    for (uint32_t x = 0; x < w; ++x) {
      const uint8_t* c = row + static_cast<size_t>(x / 4) * 3;
      int m = c[0];
      if (c[1] > m) m = c[1];
      if (c[2] > m) m = c[2];
      if (m < 8) continue;  // fully decayed: gone
      int a = m * gain_pm * 6 / 1000;
      if (a > kSmearAlphaMaxPm) a = kSmearAlphaMaxPm;
      uint8_t* px = rgb + (static_cast<size_t>(y) * w + x) * 3;
      for (int k = 0; k < 3; ++k) {
        int cc = c[k] * 3 / 2;  // the cell's own hue, kept vivid
        if (cc > 255) cc = 255;
        px[k] = static_cast<uint8_t>((px[k] * (1000 - a) + cc * a) / 1000);
      }
    }
  }
}

}  // namespace u02

#endif  // ZHAO_REEL_UNNAMED02_FX_H
