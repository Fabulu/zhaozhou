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
  int32_t hinge_a[3];
  int32_t hinge_b[3];
  int32_t hinge_c[3];
  int32_t ring[3];    // the ring-pocket centre (hinge centroid) — the mana
                      // moves when the hinges play: the rig and the effect
                      // are one performance (Direction 2 §4)
};

// ---- the bolt (FX.LIGHTNING recurrence — kept verbatim) -------------------
constexpr int kBoltSegs = 16;          // the main bolt (hinge B -> crown)
constexpr int kBolt2Segs = 8;          // the branch (hinge A -> hinge C)
constexpr int32_t kBoltJitterMm = 175;
constexpr int kBoltStrikeFrames = 14;  // one strike lives this long
constexpr uint32_t kBoltSeed = 0xC0DA11CEu;
constexpr int32_t kBoltStampMm = 45;   // stamp spacing along a segment (~2 px)

// ---- the mana ramps (lo MUST be black: a floor above zero rims every
// blob — the edge-free law, 09-ENGINE-GOTCHAS §11) --------------------------
constexpr uint8_t kManaBlueMid[3] = {40, 85, 215};
constexpr uint8_t kManaBlueHi[3] = {150, 215, 255};
constexpr uint8_t kManaVioletMid[3] = {125, 45, 205};
constexpr uint8_t kManaVioletHi[3] = {225, 175, 255};
constexpr uint8_t kManaGoldMid[3] = {175, 120, 35};
constexpr uint8_t kManaGoldHi[3] = {255, 225, 150};
constexpr uint8_t kManaCyanMid[3] = {35, 125, 160};
constexpr uint8_t kManaCyanHi[3] = {195, 250, 255};
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
constexpr int kPlasmaGainPm = 480;
constexpr int32_t kPlasmaOrbitMm = 620;
constexpr int kPlasmaOrbitFrames = 300;
constexpr int kBulletsN = 10;
constexpr int kBulletLifeFrames = 48;
constexpr int32_t kBulletRPx = 8;
constexpr int kBulletGainPm = 780;
constexpr int kBulletGhosts = 3;           // smear route 2: stamped ghosts
constexpr int kBulletGhostStepFrames = 2;
constexpr int32_t kBulletSpeedMmPerFrame = 42;
constexpr int kBoltCoreGainPm = 1000;
constexpr int32_t kBoltCoreRPx = 3;
constexpr int kBoltHaloGainPm = 290;
constexpr int32_t kBoltHaloRPx = 9;
constexpr int kBoltGhostGainPm = 240;      // the previous strike, decaying
constexpr int kStreakGainPm = 420;         // the anamorphic strike flash
constexpr int32_t kStreakSpanPx = 46;
constexpr int32_t kBoilCorePx = 30;
constexpr int32_t kBoilOuterPx = 48;
constexpr int kBoilCoreGainPm = 520;
constexpr int kBoilOuterGainPm = 380;
constexpr int kBoilRotDiv = 3;             // CLUT rotation: churn, zero pixels
constexpr int kDripN = 4;
constexpr int kDripLifeFrames = 60;
constexpr int32_t kDripR0Px = 12, kDripR1Px = 7;   // shrinks as it falls

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
                      uint32_t seed, int32_t pts[][3]) {
  for (int i = 0; i <= segs; ++i) {
    for (int k = 0; k < 3; ++k) pts[i][k] = lerp32(s[k], e[k], i, segs);
    if (i == 0 || i == segs) continue;  // the anchors stay anchored
    const uint32_t h1 = fx_hash(seed, phase, static_cast<uint32_t>(i));
    const uint32_t h2 = fx_hash(seed * seed | 1u, phase, static_cast<uint32_t>(i));
    pts[i][0] += fxu(fx_jit(h1, kBoltJitterMm));
    pts[i][1] += fxu(fx_jit(h1 >> 11, kBoltJitterMm / 2));
    pts[i][2] += fxu(fx_jit(h2, kBoltJitterMm * 2 / 3));
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
      mana_push(out, x, y, z, kBoltCoreRPx, kRampWhite, gain_core_pm, true, false);
    }
  }
}

/** LIGHTNING (candidate 4 / the channel's blaze): the current strike at
 *  full heat decaying over its life, the PREVIOUS strike as a fading ghost
 *  (a bolt that vanishes reads as noise; one that decays reads as a
 *  strike), and the anamorphic flash bar on the strike frame. */
inline void mana_lightning(uint32_t frame, const FxAnchors& A,
                           std::vector<ManaSplat>& out) {
  const uint32_t phase = frame / kBoltStrikeFrames;
  const int age = static_cast<int>(frame % kBoltStrikeFrames);
  int32_t pts[kBoltSegs + 1][3];
  // the current strike: heat falls over the strike's life
  const int heat = 1000 - age * 900 / kBoltStrikeFrames;
  bolt_path(A.hinge_b, A.crown, kBoltSegs, phase, kBoltSeed, pts);
  bolt_stamp(out, pts, kBoltSegs, kBoltCoreGainPm * heat / 1000,
             kBoltHaloGainPm * heat / 1000);
  // the previous strike, ghosting out (smear: a decaying afterimage)
  if (phase > 0) {
    bolt_path(A.hinge_b, A.crown, kBoltSegs, phase - 1, kBoltSeed, pts);
    bolt_stamp(out, pts, kBoltSegs, 0, kBoltGhostGainPm * (1000 - age * 110) / 1000);
  }
  // the branch, every third strike
  if (phase % 3 == 0) {
    int32_t bp[kBolt2Segs + 1][3];
    bolt_path(A.hinge_a, A.hinge_c, kBolt2Segs, phase, kBoltSeed * 2654435761u, bp);
    bolt_stamp(out, bp, kBolt2Segs, kBoltCoreGainPm * heat / 1400,
               kBoltHaloGainPm * heat / 1400);
  }
  // the anamorphic flash bar through the strike midpoint, strike frame only
  if (age == 0) {
    const int32_t mx = (A.hinge_b[0] + A.crown[0]) / 2;
    const int32_t my = (A.hinge_b[1] + A.crown[1]) / 2;
    const int32_t mz = (A.hinge_b[2] + A.crown[2]) / 2;
    for (int i = -3; i <= 3; ++i) {
      const int32_t r = 8 - (i < 0 ? -i : i) * 2;  // 2..8..2 px
      // ~kStreakSpanPx of horizontal spread expressed in world mm (~12.3/px)
      mana_push(out, mx + fxu(i * kStreakSpanPx * 25 / 12), my, mz, r, kRampWhite,
                kStreakGainPm, false, false);
    }
  }
}

/** Fill the frame's mana splats for one conduit. `cand` selects the menu
 *  candidate (1..6, 0 = none). The showcase channel/crackle clips run
 *  candidate 4 — the Description sheet says lightning IS this creature. */
inline void mana_fill(int cand, uint32_t frame, const FxAnchors& A,
                      std::vector<ManaSplat>& out) {
  switch (cand) {
    case 1: {  // the caged pulsar
      const int32_t breathe = static_cast<int32_t>(
          (static_cast<int64_t>(kPulsarHaloMaxPx - kPulsarHaloMinPx) *
           ((65536 + fx_sin16(frame * 65536 / kPulsarBreathFrames)) / 2)) >> 16);
      mana_push(out, A.ring[0], A.ring[1], A.ring[2], kPulsarHaloMinPx + breathe,
                kRampCyan, kPulsarHaloGainPm, true, true);  // pre: arms occlude it
      mana_push(out, A.ring[0], A.ring[1], A.ring[2], kPulsarCorePx, kRampCyan,
                kPulsarCoreGainPm, false, false);  // no depth test: through the blade
      break;
    }
    case 2: {  // big plasma blobs — blue, violet, gold, drifting slowly
      const uint32_t ph = frame * 65536 / kPlasmaOrbitFrames;
      const uint8_t ramps[3] = {kRampBlue, kRampViolet, kRampGold};
      for (int i = 0; i < 3; ++i) {
        const uint32_t p = ph + static_cast<uint32_t>(i) * 65536 / 3;
        const int32_t ox = static_cast<int32_t>(
            (static_cast<int64_t>(fxu(kPlasmaOrbitMm)) * fx_cos16(p)) >> 16);
        const int32_t oz = static_cast<int32_t>(
            (static_cast<int64_t>(fxu(kPlasmaOrbitMm)) * fx_sin16(p)) >> 16);
        const int32_t oy = static_cast<int32_t>(
            (static_cast<int64_t>(fxu(260)) * fx_sin16(p * 2 + 9000)) >> 16);
        mana_push(out, A.body[0] + ox, A.body[1] + fxu(150) + oy, A.body[2] + oz,
                  kPlasmaRPx[i], ramps[i], kPlasmaGainPm, true, true);
      }
      break;
    }
    case 3: {  // smeared plasma bullets out of the ring
      for (int i = 0; i < kBulletsN; ++i) {
        const uint32_t h = fx_hash(31, static_cast<uint32_t>(i), 7u);
        const uint32_t life = kBulletLifeFrames;
        for (int gstep = 0; gstep <= kBulletGhosts; ++gstep) {
          const int64_t fghost =
              static_cast<int64_t>(frame) - gstep * kBulletGhostStepFrames;
          if (fghost < 0) continue;
          const uint32_t t = static_cast<uint32_t>((fghost + h % life) % life);
          const uint32_t burst = static_cast<uint32_t>((fghost + h % life) / life);
          const uint32_t hd = fx_hash(31, static_cast<uint32_t>(i), burst);
          const uint32_t dir = hd & 0xFFFF;
          const int32_t el = static_cast<int32_t>((hd >> 16) % 26000) - 6000;
          const int32_t dist = fxu(static_cast<int32_t>(kBulletSpeedMmPerFrame * t));
          const int32_t droop = fxu(static_cast<int32_t>((2 * t * t) / 10));
          const int gain = kBulletGainPm * (gstep == 0 ? 1000
                                            : gstep == 1 ? 480
                                            : gstep == 2 ? 240
                                                         : 110) / 1000;
          mana_push(out,
                    A.ring[0] + static_cast<int32_t>(
                        (static_cast<int64_t>(dist) * fx_cos16(dir)) >> 16),
                    A.ring[1] + static_cast<int32_t>(
                        (static_cast<int64_t>(dist) * el / 32768)) - droop,
                    A.ring[2] + static_cast<int32_t>(
                        (static_cast<int64_t>(dist) * fx_sin16(dir)) >> 16),
                    kBulletRPx, kRampBlue, gain, true, false);
        }
      }
      break;
    }
    case 4:
      mana_lightning(frame, A, out);
      break;
    case 5: {  // two-tone boil: the churn is CLUT rotation (see ramp build)
      mana_push(out, A.ring[0], A.ring[1], A.ring[2], kBoilOuterPx, kRampViolet,
                kBoilOuterGainPm, true, true);
      mana_push(out, A.ring[0], A.ring[1], A.ring[2], kBoilCorePx, kRampBlue,
                kBoilCoreGainPm, false, false);
      break;
    }
    case 6: {  // the drip: large opaque droplets off the loop underside
      for (int i = 0; i < kDripN; ++i) {
        const uint32_t h = fx_hash(66, static_cast<uint32_t>(i), 11u);
        const uint32_t t = (frame + h % kDripLifeFrames) % kDripLifeFrames;
        int32_t sp[3];
        for (int k = 0; k < 3; ++k)
          sp[k] = lerp32(A.hinge_a[k], A.hinge_c[k], static_cast<int32_t>(h % 64), 64);
        const int32_t drop = fxu(static_cast<int32_t>((8 * t * t) / 10));
        if (sp[1] - drop < fxu(160)) continue;  // despawn above the dirt
        const int32_t r = kDripR0Px - static_cast<int32_t>(
            (kDripR0Px - kDripR1Px) * static_cast<int32_t>(t) /
            static_cast<int32_t>(kDripLifeFrames));
        mana_push(out, sp[0] + fx_jit(h >> 8, 2) * 30, sp[1] - drop, sp[2], r,
                  kRampDrip, 1000, true, false, /*opaque=*/true);
      }
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

}  // namespace u02

#endif  // ZHAO_REEL_UNNAMED02_FX_H
