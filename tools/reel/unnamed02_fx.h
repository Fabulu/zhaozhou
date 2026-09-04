// Unnamed02 — the effects: the centre glow (S5), later the ten emitter
// tables and the bolt polyline.
//
// Consumer contract: include after `namespace zc = zref::creature;` with the
// zref headers available (zref_star.hpp included by the consumer for the
// corona bake). Everything here is reel-side AUTHORING over exported engine
// primitives — no engine change, no interaction with the ≤2 suns/flares
// caps (the glow is not a ComposeLight; it never enters the flare slots).

#ifndef ZHAO_REEL_UNNAMED02_FX_H
#define ZHAO_REEL_UNNAMED02_FX_H

#include "unnamed02_art.h"

namespace u02 {

// ---- the centre glow (S5) -------------------------------------------------
//
// ONE baked radial CLUT8 sprite (the engine's §4 halo_atmo corona bake,
// core16 = 0: soft, no hard core) + ONE 64-entry ramp built per FRAME (not
// per instance) — N conduits share both, which is what makes "a big effect
// at its centre" affordable at several on screen. Drawn additively at each
// projected body centre with the CENTRE'S OWN DEPTH: the star path's
// kStarDepth law paints only where depth == 0 (sky), but a conduit's glow
// must also paint over the terrain BEHIND it — so this splat depth-tests
// against the creature centre's 1/w instead. Drawn BEFORE the creature
// compose, so the body occludes the glow core: the light reads as coming
// from INSIDE the belly.

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

/** 64-entry ramp: lo -> mid over [0,32), mid -> hi over [32,64). Same shape
 *  as the planet-sky ramp; the halo palette keeps [0] black (the additive
 *  identity), which the corona bake's index-0 exterior relies on. */
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
