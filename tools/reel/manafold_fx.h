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
  int32_t neck[3];        // pass 4: the NEW mid-tube neck hinge
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
// PASS 4 (R4, Direction 4 §2: "fewer lightning LINES -- just have them be
// particles"): ONE strand (the owner knob; the channel may carry 2), and
// the retired strands' budget converts into SURGE MOTES that flow along
// the strand's path and burst at its ends -- energised particles with one
// hot filament, not a filament cloud.
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
// PASS 4 (R6, Direction 4 §2 "glitchier than the others you made"): a
// FOURTH live rung past the long/glitchy one -- keep higher, steps longer
// and chunkier, jitter wider -- and the ROW TEAR: on hashed frames a
// horizontal band of the plane composites with a 1-2 cell x-offset, the
// VHS tear of a genuinely broken buffer. Tear rides the two glitchier
// rungs only (tear=1); an index offset at composite, near-free.
struct SmearTear { int rows, frames, cells; };
constexpr SmearTear kSmearTear = {5, 46, 2};  // kSmearTearRows/Frames/Cells
constexpr SmearPreset kSmearPresets[5] = {
    {0, 1, 0, 1, 0, 0},          // 0: no smear
    {620, 2, 40, 90, 420, 0},    // 1: SHORT/CLEAN — a readable tail, tidy
    {820, 4, 90, 260, 520, 0},   // 2: MID/GLITCHY — the pass-3 lead rung
    {900, 6, 160, 430, 520, 1},  // 3: LONG/GLITCHIER — the shipping fold rung
    {940, 8, 260, 560, 540, 1},  // 4: BROKEN-BUFFER — the new far end (cyan)
};
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

constexpr int kStencilPts = 18;

// The six shape stencils, authored in pocket coordinates (u across the
// hole, v up; per-mille of kStencilScaleMm). Chosen for legibility with
// blobby strokes at ~37 px: RING (the opener), FOUR-POINT STAR (the
// identity, rhymes with the pupil), BAR (max contrast), CRESCENT (the
// Description sheet's rear view), TRIANGLE, S-CURL.
struct StencilPt { int16_t u_pm, v_pm; };
inline const StencilPt (&fold_stencils())[6][kStencilPts] {
  static StencilPt st[6][kStencilPts];
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
  uint16_t w[6][kStencilPts][6];
};
inline const FoldWeights& fold_weights() {
  static FoldWeights fw;
  static bool built = false;
  if (!built) {
    const StencilPt(&st)[6][kStencilPts] = fold_stencils();
    for (int sh = 0; sh < 6; ++sh)
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
  const FoldPhase ph = fold_phase(slot, keys, static_cast<int32_t>(frame) * 8);
  g_u02_fold_release_pm = ph.seg == kSegRelease ? ph.amp_pm : 1000;
  const FoldWeights& fw = fold_weights();
  if (g_u02_fold_debug)
    std::fprintf(stderr,
                 "fold f=%u seg=%d amp=%d agit_env=%d morph=%d %d->%d | "
                 "area_pm=%d ema=%d coh=%d knead_mm=%d agit=%d\n",
                 frame, static_cast<int>(ph.seg), ph.amp_pm, ph.agit_pm,
                 ph.morph_pm, ph.shape_from, ph.shape_to, area_pm, stfx.area_ema_pm, coh,
                 stfx.knead_smooth, agit);
  // ---- the motes ---------------------------------------------------------
  int n_motes = kMoteCount * crowd_pm / 1000;
  if (n_motes < 6) n_motes = 6;
  const int n_wander = kWanderCount;
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
      // the authored face yaw (kStencilFaceYawA16): rotate the stencil
      // OFFSET about the vertical axis so the shape survives the house
      // camera's foreshortening; the anchor sum itself is untouched
      {
        const int32_t sn = fx_sin16(static_cast<uint32_t>(kStencilFaceYawA16));
        const int32_t cs = fx_cos16(static_cast<uint32_t>(kStencilFaceYawA16));
        const int32_t ox = Pst[0] - A.ring[0];
        const int32_t oz = Pst[2] - A.ring[2];
        Pst[0] = A.ring[0] + static_cast<int32_t>(
            ((static_cast<int64_t>(ox) * cs) >> 16) - ((static_cast<int64_t>(oz) * sn) >> 16));
        Pst[2] = A.ring[2] + static_cast<int32_t>(
            ((static_cast<int64_t>(ox) * sn) >> 16) + ((static_cast<int64_t>(oz) * cs) >> 16));
      }
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
    // DRAG: the lagged pull along the antenna's sweep (iron filings)
    if (!g_u02_fold_lock) {
      const int lag = kDragLagFrames + static_cast<int>((hm >> 21) % 4u);  // 2..5
      const uint32_t bi = stfx.drag_idx + 8u - static_cast<uint32_t>(lag);
      for (int k = 0; k < 3; ++k) {
        const int64_t v = static_cast<int64_t>(stfx.dragbuf[bi & 7][k]) +
                          stfx.dragbuf[(bi - 1u) & 7][k] + stfx.dragbuf[(bi - 2u) & 7][k];
        P[k] += static_cast<int32_t>(v * kDragGainPm / 1000);
      }
    }
    // draw: an opaque heart under an additive halo (R7 -- filled and BIG)
    const int32_t halo = kMoteHaloRPxMin +
        static_cast<int32_t>((hm >> 5) % static_cast<uint32_t>(kMoteHaloRPxMax - kMoteHaloRPxMin + 1));
    mana_push(out, P[0], P[1], P[2], halo * kMoteCoreOfHaloPm / 1000, ramp, 1000,
              true, false, /*opaque=*/true);
    mana_push(out, P[0], P[1], P[2], halo, ramp, kMoteHaloGainPm, true, false);
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
      for (int k = 0; k < 3; ++k) {
        int cc = c[k] * 3 / 2;  // the cell's own hue, kept vivid
        if (cc > 255) cc = 255;
        px[k] = static_cast<uint8_t>((px[k] * (1000 - a) + cc * a) / 1000);
      }
    }
  }
}

}  // namespace u02

#endif  // ZHAO_REEL_MANAFOLD_FX_H
