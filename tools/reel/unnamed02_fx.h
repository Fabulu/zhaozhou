// Unnamed02 — the effects: TEN mana kinds, the FX.LIGHTNING bolt, the
// centre glow. This file IS the effects knob block: every rate, life,
// speed, size and colour below is an owner knob.
//
// Consumer contract: include after `namespace zc = zref::creature;` with the
// zref headers available. Everything here is reel-side AUTHORING over
// exported engine primitives — no engine change, no interaction with the
// ≤2 suns/flares caps.
//
// COLOUR LAW (proven at spike S3): additive colours must sit UNDER the
// channel ceiling or they whiten on the pink pigment — every additive kind
// below is authored calm (Direction 30's lesson, on particles).
//
// THE BOLT follows reports/ADDLIGHTNING.md's FX.LIGHTNING recurrence
// exactly — P_i = lerp(start, end, i/N) + perp1·jitter(seed, phase, i) +
// perp2·jitter(seed², phase, i), ≤24 segments, ≤2 bounded branches — so
// this authoring migrates unchanged onto the FORGE.PRIM ribbon evaluator
// the day it lands. Today each segment is one additive tri bead through
// the Population path (the executable route the direction confirms).

#ifndef ZHAO_REEL_UNNAMED02_FX_H
#define ZHAO_REEL_UNNAMED02_FX_H

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
};

// ---- the ten kinds: named knobs -------------------------------------------
// slot counts are LIVE particle caps per kind; a clip runs only its set.
constexpr int kMotesN = 18;          // 1 cyan orbit shimmer — all clips
constexpr int kSparksN = 12;         // 2 white-cyan hinge bursts — startle, channel
constexpr int kWispsN = 10;          // 3 magenta crown risers — hover, rest
constexpr int kRingN = 16;           // 4 violet release torus — channel release
constexpr int kHelixN = 14;          // 5 teal counter-helices — channel, pirouette
constexpr int kDropletsN = 8;        // 6 deep-blue OPAQUE drips — hover, drift
constexpr int kDrainN = 16;          // 7 green inward streamers — channel draw-in
constexpr int kGlintsN = 6;          // 8 cyan TRI star pops — all clips, sparse
constexpr int kBoltSegs = 12;        // 9 the main bolt (hinge B -> belly)
constexpr int kBolt2Segs = 8;        //   the branch bolt (hinge A -> hinge C)
constexpr int kShieldN = 10;         // 10 gold-amber equatorial band — channel, curious

constexpr int32_t kBoltJitterMm = 130;
constexpr int kBoltRehashFrames = 3;   // the strike clock: a new jagged path
constexpr uint32_t kBoltSeed = 0xC0DA11CEu;

// calm additive colours (the ceiling law) — {r,g,b} per kind
constexpr uint8_t kColMotes[3] = {30, 120, 140};
constexpr uint8_t kColSparks[3] = {95, 120, 130};
constexpr uint8_t kColWisps[3] = {150, 55, 115};
constexpr uint8_t kColRing[3] = {115, 55, 165};
constexpr uint8_t kColHelix[3] = {40, 140, 125};
constexpr uint8_t kColDroplets[3] = {45, 70, 165};  // OPAQUE: full-value blue
constexpr uint8_t kColDrain[3] = {60, 150, 70};
constexpr uint8_t kColGlints[3] = {60, 180, 190};
constexpr uint8_t kColBolt[3] = {225, 205, 250};    // the one deliberately hot kind
constexpr uint8_t kColShield[3] = {175, 125, 45};

// ---- helpers --------------------------------------------------------------
inline void fx_push(zref::render::Population& pop, int32_t x, int32_t y, int32_t z,
                    uint8_t size, const uint8_t c[3]) {
  pop.parts.push_back(zref::render::Particle{x, y, z, size, c[0], c[1], c[2]});
}
inline int32_t lerp32(int32_t a, int32_t b, int32_t num, int32_t den) {
  return a + static_cast<int32_t>((static_cast<int64_t>(b - a) * num) / den);
}
inline int32_t fx_sin16(uint32_t ph) {  // Q16.16 sin of a 0..65535 phase
  return zref::fx_sin(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
}
inline int32_t fx_cos16(uint32_t ph) {
  return zref::fx_cos(zref::angle16{static_cast<uint16_t>(ph & 0xFFFF)}).raw;
}

/**
 * FX.LIGHTNING position evaluator (the ADDLIGHTNING recurrence, software
 * preview): deterministic jagged polyline start->end, re-hashed every
 * kBoltRehashFrames (the strike clock), jitter in the two perpendiculars.
 * Emits one additive tri bead per segment; the rehash frame flashes larger.
 */
inline void bolt_beads(zref::render::Population& pop, const int32_t s[3], const int32_t e[3],
                       int segs, uint32_t frame, uint32_t seed) {
  const uint32_t phase = frame / kBoltRehashFrames;
  const bool flash = (frame % kBoltRehashFrames) == 0;
  // axis + two crude perpendiculars (the loop plane is X-Y, so perp1 = the
  // in-plane normal and perp2 = Z; exact orthonormality is not the point —
  // bounded deterministic jag is)
  const int64_t ax = e[0] - s[0], ay = e[1] - s[1];
  (void)ax;
  (void)ay;
  for (int i = 1; i < segs; ++i) {
    const uint32_t h1 = fx_hash(seed, phase, static_cast<uint32_t>(i));
    const uint32_t h2 = fx_hash(seed * seed | 1u, phase, static_cast<uint32_t>(i));
    int32_t p[3];
    for (int k = 0; k < 3; ++k) p[k] = lerp32(s[k], e[k], i, segs);
    // perp1: in the loop plane, perpendicular-ish to the run: jitter x/y
    p[0] += fxu(fx_jit(h1, kBoltJitterMm));
    p[1] += fxu(fx_jit(h1 >> 11, kBoltJitterMm / 2));
    // perp2: across the plane
    p[2] += fxu(fx_jit(h2, kBoltJitterMm * 2 / 3));
    fx_push(pop, p[0], p[1], p[2], flash ? 76 : 52, kColBolt);
  }
}

/**
 * Fill the frame's populations for one conduit. `frame` is the presentation
 * frame within the clip loop (loop_frames long). Additive kinds go to
 * add_pop (drawn flags 0x0007), the droplets to opq_pop (0x0003).
 * Kind sets per clip follow the plan's table; channel is the only clip
 * running everything.
 */
inline void fx_fill(int clip_slot, uint32_t frame, uint32_t loop_frames,
                    const FxAnchors& A, zref::render::Population& add_pop,
                    zref::render::Population& tri_pop,
                    zref::render::Population& opq_pop, int solo = -1,
                    bool crackle = false) {
  const bool ch = clip_slot == 2;
  const uint32_t lf = loop_frames > 0 ? loop_frames : 1;
  // solo >= 0: the fx-tour — ONE kind at a time, forced active regardless of
  // clip. crackle: the ADDLIGHTNING "lightning version" — bolts every frame.
  const auto on = [&](int kind, bool clip_cond) {
    if (solo >= 0) return solo == kind;
    return clip_cond;
  };

  // 1 motes — every clip: the baseline shimmer, slow orbit about the body
  if (on(0, true))
  for (int i = 0; i < kMotesN; ++i) {
    const uint32_t h = fx_hash(1, 0, static_cast<uint32_t>(i));
    const int32_t rad = fxu(720 + static_cast<int32_t>(h % 300));
    const uint32_t ph = frame * 65536 / 240 + i * 65536 / kMotesN + (h & 0xFFFF);
    const int32_t y = A.body[1] + fxu(150) + fxu(fx_jit(h >> 7, 300)) +
                      static_cast<int32_t>((static_cast<int64_t>(fxu(140)) *
                                            fx_sin16(frame * 65536 / 120 + i * 9000)) >> 16);
    fx_push(add_pop, A.body[0] + static_cast<int32_t>((static_cast<int64_t>(rad) * fx_cos16(ph)) >> 16),
            y, A.body[2] + static_cast<int32_t>((static_cast<int64_t>(rad) * fx_sin16(ph)) >> 16),
            40, kColMotes);
  }

  // 2 sparks — startle + channel: fast radial hinge bursts, short life
  if (on(1, clip_slot == 4 || ch || crackle)) {
    for (int i = 0; i < kSparksN; ++i) {
      const uint32_t h = fx_hash(2, static_cast<uint32_t>(i), 7u);
      const uint32_t cycle = 78;
      const uint32_t t = (frame + h % cycle) % cycle;
      if (t >= 26) continue;  // duty 1/3
      const int32_t* hinge = i % 3 == 0 ? A.hinge_a : i % 3 == 1 ? A.hinge_b : A.hinge_c;
      const uint32_t burst = (frame + h % cycle) / cycle;  // re-aim per burst
      const uint32_t hd = fx_hash(2, static_cast<uint32_t>(i), burst);
      const int32_t dist = fxu(static_cast<int32_t>(26 * t));
      const uint32_t dir = hd & 0xFFFF;
      fx_push(add_pop,
              hinge[0] + static_cast<int32_t>((static_cast<int64_t>(dist) * fx_cos16(dir)) >> 16),
              hinge[1] + static_cast<int32_t>((static_cast<int64_t>(dist / 2) *
                                               fx_sin16(dir * 3)) >> 16),
              hinge[2] + static_cast<int32_t>((static_cast<int64_t>(dist) * fx_sin16(dir)) >> 16),
              static_cast<uint8_t>(40 - t), kColSparks);
    }
  }

  // 3 wisps — hover + rest: magenta risers off the crown, fading as they climb
  if (on(2, clip_slot == 0 || clip_slot == 5)) {
    for (int i = 0; i < kWispsN; ++i) {
      const uint32_t h = fx_hash(3, static_cast<uint32_t>(i), 3u);
      const uint32_t t = (frame + h % 150) % 150;
      const int32_t y = A.crown[1] + fxu(static_cast<int32_t>(8 * t));
      const int32_t lat = fxu(fx_jit(h >> 5, 120)) +
                          static_cast<int32_t>((static_cast<int64_t>(fxu(90)) *
                                                fx_sin16(t * 1200 + (h & 0xFFFF))) >> 16);
      const uint8_t size = static_cast<uint8_t>(48 - t * 28 / 150);
      fx_push(add_pop, A.crown[0] + lat, y, A.crown[2] + fx_jit(h >> 9, 3) * 40, size, kColWisps);
    }
  }

  // 4 ring-pulse — the channel's release beat: an expanding equatorial torus
  if (on(3, ch && frame >= 280)) {
    const uint32_t rt = (frame >= 280 ? frame - 280 : frame) % 70;
    for (int i = 0; i < kRingN; ++i) {
      const uint32_t ph = i * 65536 / kRingN + rt * 300;
      const int32_t rad = fxu(500 + static_cast<int32_t>(rt * 24));
      fx_push(add_pop,
              A.body[0] + static_cast<int32_t>((static_cast<int64_t>(rad) * fx_cos16(ph)) >> 16),
              A.body[1],
              A.body[2] + static_cast<int32_t>((static_cast<int64_t>(rad) * fx_sin16(ph)) >> 16),
              static_cast<uint8_t>(44 - rt / 4), kColRing);
    }
  }

  // 5 helix-stream — channel + pirouette: two counter-rotating bead helices
  if (on(4, ch || clip_slot == 6)) {
    for (int i = 0; i < kHelixN; ++i) {
      const int dir = (i & 1) ? 1 : -1;
      const uint32_t ph = static_cast<uint32_t>(
          static_cast<int64_t>(dir) * frame * 500 + i * 65536 / kHelixN * 2);
      const int32_t y = A.body[1] - fxu(430) +
                        fxu(static_cast<int32_t>((frame * 6 + i * 157) % 1300));
      const int32_t rad = fxu(520);
      fx_push(add_pop,
              A.body[0] + static_cast<int32_t>((static_cast<int64_t>(rad) * fx_cos16(ph)) >> 16),
              y,
              A.body[2] + static_cast<int32_t>((static_cast<int64_t>(rad) * fx_sin16(ph)) >> 16),
              36, kColHelix);
    }
  }

  // 6 droplets — hover + drift: OPAQUE deep-blue drips off the loop underside
  if (on(5, clip_slot == 0 || clip_slot == 1)) {
    for (int i = 0; i < kDropletsN; ++i) {
      const uint32_t h = fx_hash(6, static_cast<uint32_t>(i), 11u);
      const uint32_t t = (frame + h % 90) % 90;
      int32_t sp[3];
      for (int k = 0; k < 3; ++k)
        sp[k] = lerp32(A.hinge_a[k], A.hinge_c[k], static_cast<int32_t>(h % 64), 64);
      const int32_t drop = fxu(static_cast<int32_t>((6 * t * t) / 10));
      if (sp[1] - drop < fxu(120)) continue;  // the despawn floor line
      fx_push(opq_pop, sp[0] + fx_jit(h >> 8, 2) * 30, sp[1] - drop, sp[2], 32, kColDroplets);
    }
  }

  // 7 drain-streamers — the channel's draw-in: spawn on a far shell, pulled in
  if (on(6, ch && frame < 130)) {
    const uint32_t T = 90;
    for (int i = 0; i < kDrainN; ++i) {
      const uint32_t h = fx_hash(7, static_cast<uint32_t>(i), 5u);
      const uint32_t t = (frame + h % T) % T;
      const uint32_t dir = h & 0xFFFF;
      const int32_t el = static_cast<int32_t>((h >> 16) % 20000) - 10000;
      // dist = shell * (1 - (t/T)^2): the pull ACCELERATES inward
      const int64_t tt = static_cast<int64_t>(t) * t * 65536 / (T * T);
      const int32_t dist = static_cast<int32_t>(
          (static_cast<int64_t>(fxu(1125)) * (65536 - tt)) >> 16);
      fx_push(add_pop,
              A.body[0] + static_cast<int32_t>((static_cast<int64_t>(dist) * fx_cos16(dir)) >> 16),
              A.body[1] + static_cast<int32_t>((static_cast<int64_t>(dist) * el) >> 16),
              A.body[2] + static_cast<int32_t>((static_cast<int64_t>(dist) * fx_sin16(dir)) >> 16),
              36, kColDrain);
    }
  }

  // 8 star-glints — all clips, sparse: TRI pops at loop stations echoing the
  // pupil star; each holds 32 frames (>= the 16-frame register floor)
  if (on(7, true))
  for (int i = 0; i < kGlintsN; ++i) {
    const uint32_t epoch = frame / 32;
    const uint32_t h = fx_hash(8, static_cast<uint32_t>(i), epoch);
    if (h % 3 != 0) continue;  // sparse
    const uint32_t st = h % 128;
    int32_t p[3];
    if (st < 64) {
      for (int k = 0; k < 3; ++k) p[k] = lerp32(A.hinge_a[k], A.hinge_b[k], st, 64);
    } else {
      for (int k = 0; k < 3; ++k) p[k] = lerp32(A.hinge_b[k], A.hinge_c[k], st - 64, 64);
    }
    fx_push(tri_pop, p[0], p[1] + fxu(fx_jit(h >> 9, 60)), p[2] + fxu(fx_jit(h >> 13, 40)), 52,
            kColGlints);
  }

  // 9 bolt-beads — the channel's blaze: the FX.LIGHTNING bead-chain. The
  // main bolt arcs hinge B -> the body CROWN (visible above the silhouette;
  // a centre-aimed bolt hid entirely behind the body's own depth) and the
  // branch crosses the loop's open window hinge A -> hinge C, against the
  // bloomed sky where additive reads hottest.
  if (on(8, (ch && frame >= 112 && frame < 280) || crackle)) {
    bolt_beads(tri_pop, A.hinge_b, A.crown, kBoltSegs, frame, kBoltSeed);
    if (crackle || (frame >= 160 && frame < 240))  // the branch (<= 2 branches)
      bolt_beads(tri_pop, A.hinge_a, A.hinge_c, kBolt2Segs, frame, kBoltSeed * 2654435761u);
  }

  // 10 shield-orbit — channel + curious: the slow gold equatorial band
  if (on(9, ch || clip_slot == 3)) {
    for (int i = 0; i < kShieldN; ++i) {
      const uint32_t ph = frame * 65536 / 300 + i * 65536 / kShieldN;
      const int32_t rad = fxu(720);
      const int32_t y = A.body[1] +
                        static_cast<int32_t>((static_cast<int64_t>(fxu(90)) *
                                              fx_sin16(ph * 2 + 8000)) >> 16);
      fx_push(add_pop,
              A.body[0] + static_cast<int32_t>((static_cast<int64_t>(rad) * fx_cos16(ph)) >> 16),
              y,
              A.body[2] + static_cast<int32_t>((static_cast<int64_t>(rad) * fx_sin16(ph)) >> 16),
              56, kColShield);
    }
  }
  (void)lf;
}

// ---- the centre glow (S5) -------------------------------------------------
//
// ONE baked radial CLUT8 sprite (the engine's §4 halo_atmo corona bake) +
// ONE 64-entry ramp built per FRAME (not per instance). Two layers: the
// outer aura BEFORE the compose, depth-tested against the conduit centre's
// own 1/w (paints over sky AND the terrain behind it); the small core AFTER
// the compose with no depth test — the light lives in the belly.

struct GlowAssets {
  zref::star::Sprite8 sprite;  // baked once per process
  bool baked = false;
};

struct GlowFrame {
  uint8_t pal[64][3];  // built once per frame from the knob colours
};

inline void glow_bake(GlowAssets& g) {
  if (g.baked) return;
  g.sprite = zref::star::corona_sprite(0);  // §4 halo_atmo profile
  g.baked = true;
}

/** 64-entry ramp: lo -> mid over [0,32), mid -> hi over [32,64). [0] stays
 *  black (the additive identity the corona bake's exterior relies on). */
inline void glow_build_ramp(GlowFrame& f, const uint8_t lo[3], const uint8_t mid[3],
                            const uint8_t hi[3], int gain_pm) {
  for (int i = 0; i < 64; ++i) {
    const uint8_t* a = i < 32 ? lo : mid;
    const uint8_t* b = i < 32 ? mid : hi;
    const int t = (i & 31) * 2 + 1;  // 1..63 of 64
    for (int c = 0; c < 3; ++c) {
      int v = (a[c] * (64 - t) + b[c] * t) / 64;
      v = v * gain_pm / 1000;
      if (v > 255) v = 255;
      f.pal[i][c] = static_cast<uint8_t>(v);
    }
  }
  f.pal[0][0] = f.pal[0][1] = f.pal[0][2] = 0;  // additive identity
}

/** One additive glow splat at canvas (cx,cy), half-size r px, depth-tested
 *  against the given centre depth (Q16.16 1/w), never writing depth. */
inline void glow_splat(uint8_t* rgb, int32_t* depth, uint32_t w, uint32_t h,
                       const GlowAssets& g, const GlowFrame& f, int32_t cx, int32_t cy,
                       int32_t r, int32_t centre_d, bool depth_test = true) {
  if (r <= 0 || !g.baked) return;
  const int32_t qx0 = cx - r, qy0 = cy - r;
  int32_t x0 = qx0, y0 = qy0, x1 = cx + r, y1 = cy + r;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > static_cast<int32_t>(w)) x1 = static_cast<int32_t>(w);
  if (y1 > static_cast<int32_t>(h)) y1 = static_cast<int32_t>(h);
  const int64_t wq = 2 * static_cast<int64_t>(r);
  for (int32_t y = y0; y < y1; ++y) {
    const int32_t sy = static_cast<int32_t>((static_cast<int64_t>(y - qy0) * g.sprite.h) / wq);
    for (int32_t x = x0; x < x1; ++x) {
      const size_t idx = static_cast<size_t>(y) * w + x;
      if (depth_test && !(centre_d > depth[idx])) continue;  // occluded by nearer surface
      const int32_t sx = static_cast<int32_t>((static_cast<int64_t>(x - qx0) * g.sprite.w) / wq);
      const uint8_t t = g.sprite.pix[static_cast<size_t>(sy) * g.sprite.w + sx];
      if (t == 0) continue;
      const size_t ri = idx * 3;
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
