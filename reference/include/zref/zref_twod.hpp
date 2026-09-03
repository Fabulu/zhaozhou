// zref_twod.hpp — the 2D sprite law (owner ruling 2026-08-31 §3.2).
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// TWOD.SPRITE.md cited `zref::HudSprites` as its scalar reference and no such
// symbol had ever been written — the fifth phantom citation hit in a row while
// building blocks today, and the reason `reports/PHANTOM-CITATIONS-AUDIT.md`
// exists. The right answer to a phantom is to write the symbol.
//
// ---------------------------------------------------------------------------
// WHAT THE LAW IS, AND WHAT IT DELIBERATELY IS NOT
// ---------------------------------------------------------------------------
// The ruling is mostly deletions: no private HUD sampler, no text rasterizer,
// no glyph cache. Text is glyph sprites, windows are tiled sprites, cursors
// are sprites, and the game authors layout in software.
//
// So there is no font metric here, no kerning, no line breaking. What is left
// is an affine map from a pixel inside a sprite to a UV, and the rule about
// which descriptors are walked at all.
#pragma once

#include <cstdint>

namespace zref {
namespace twod {

// UV at pixel (px, py) of a descriptor. This is the CLOSED FORM; the hardware
// steps it with adds, and the two must agree exactly at every pixel.
//
// Stepping it the serpentine way — carrying the running coordinate from the
// last pixel of one row into the first of the next — is also just adds, also
// correct on a 1xN sprite, and wrong for every sprite wider than one pixel.
// The cross terms a01 and a10 are what tell the two apart.
inline int32_t sprite_u(int32_t u0, int32_t a00, int32_t a01, int px, int py) {
  return u0 + a00 * px + a01 * py;
}

inline int32_t sprite_v(int32_t v0, int32_t a10, int32_t a11, int px, int py) {
  return v0 + a10 * px + a11 * py;
}

// A descriptor with no area is REFUSED, not walked for zero pixels: the two
// are indistinguishable downstream and only one of them is a caller bug.
inline bool sprite_degenerate(uint16_t w, uint16_t h) {
  return w == 0 || h == 0;
}

// The two player HUD regions are a compositing concern. Here they are a mask,
// and a descriptor that does not name this view is skipped — which is normal,
// and is counted separately from a refusal, which is not.
inline bool sprite_for_view(uint8_t view_mask, uint8_t view_sel) {
  return (view_mask & view_sel) != 0;
}

}  // namespace twod
}  // namespace zref
