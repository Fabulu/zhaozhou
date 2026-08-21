// zref_particle_soft.hpp — PART.SOFT's reference model.
//
// The ledger declared `zref::SoftParticles`, which never existed: one of the
// twenty-five phantoms in reports/PHANTOM_REFERENCES.md. Like PART.EXPAND this
// is KIND 1 — the law is already shipped, inline, in
// `zref::render::draw_population`'s `points` branch and the
// `blit_pattern_block` it calls (reference/src/zrender/sprites.cpp).
//
// PART.EXPAND is the `tris` branch; this is the `points` branch. The ledger keeps
// them apart by architect ruling 1.D and they really are different laws: one
// produces a triangle for the setup stage, the other produces a scissored PIXEL
// RECTANGLE for the fragment stage.
//
// ---------------------------------------------------------------------------
// AGAIN A RESTATED LAW, AND AGAIN IT IS CHECKED AGAINST THE RENDERER
// ---------------------------------------------------------------------------
// `blit_pattern_block` computes the rectangle and immediately paints it, so
// there is no callable to forward to and the geometry has to be restated here.
// `tests/particles/part_soft_directed.cpp` therefore renders the same particles
// both ways — once through `draw_population`, once by painting THIS function's
// rectangle — and requires identical surfaces. Without that, a restated law is
// a second implementation nobody ever compared.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     side_sub = size << 4                  (U 0.4.4 px -> S 12.8 subpixels)
//     x0_sub   = sx - side_sub/2
//     y0_sub   = sy - side_sub/2
//
//     min_x = max( (x0_sub + 255) >> 8,           vp.x0 )
//     max_x = min( (x0_sub + side_sub) >> 8,      vp.x0 + vp.w - 1 )
//     min_y = max( (y0_sub + 255) >> 8,           vp.y0 )
//     max_y = min( (y0_sub + side_sub) >> 8,      vp.y0 + vp.h - 1 )
//
// **THE TWO EDGES ROUND DIFFERENTLY, AND THAT IS THE WHOLE RULE.** The low edge
// takes a CEILING (`+255 >> 8`) and the high edge takes a FLOOR (`>> 8`). That is
// the pixel-centre coverage convention: a pixel is inside iff its centre is
// inside the rectangle. Rounding both the same way is the obvious "tidy-up" and
// it makes every sprite a pixel too wide or a pixel too narrow, on one side only
// — an asymmetry that reads as a sprite that drifts as it moves.
//
// **A ZERO-OR-NEGATIVE EXTENT DRAWS NOTHING.** `blit_pattern_block` returns
// immediately on `w_sub <= 0 || h_sub <= 0`, before any clamping. A `size` of 0
// therefore produces no pixels at all rather than a one-pixel dot.
//
// **THE RECTANGLE CAN COME OUT EMPTY AFTER SCISSORING**, when `min > max` on
// either axis. The loops in the reference simply do not execute; this function
// reports it so the hardware does not emit a fragment span nobody wants.
//
// **DEPTH IS TESTED, NEVER WRITTEN** — `depth > surf.depth[idx]` guards the
// colour write and nothing writes depth. Charter §8 pass 7: particles never
// occlude. Same law as PART.EXPAND, for the same reason.
#pragma once

#include <cstdint>

namespace zref {
namespace part {

/** The scissored pixel rectangle a point sprite covers. */
struct SoftRect {
  bool covered = false;          // false: nothing to draw
  int32_t min_x = 0, max_x = 0;  // inclusive whole-pixel range
  int32_t min_y = 0, max_y = 0;
  // The mode law, mirrored from blit_pattern_block: depth decides, never moves.
  static constexpr bool kDepthTest = true;
  static constexpr bool kDepthWrite = false;
};

/** Ceiling of a subpixel coordinate to whole pixels: `(v + 255) >> 8`. */
inline int32_t ceil_px(int32_t v_sub) { return (v_sub + 255) >> 8; }

/** Floor of a subpixel coordinate to whole pixels. */
inline int32_t floor_px(int32_t v_sub) { return v_sub >> 8; }

/**
 * The rectangle one PROJECTED point-sprite particle covers.
 *
 * `sx`/`sy` are the projection's screen vertex in S 12.8; `in` is its
 * behind-the-eye verdict. Projection is GEOM.PROJECT's and is not redone here.
 * The viewport is the canvas-local one the particle is being drawn into.
 */
inline SoftRect soft_rect(bool in, int32_t sx, int32_t sy, uint8_t size, int32_t vp_x0,
                          int32_t vp_y0, int32_t vp_w, int32_t vp_h) {
  SoftRect o;
  if (!in) return o;  // skipped, exactly as draw_population `continue`s
  const int32_t side_sub = static_cast<int32_t>(size) << 4;
  if (side_sub <= 0) return o;  // blit_pattern_block's own early return
  const int32_t half_sub = side_sub >> 1;
  const int32_t x0_sub = sx - half_sub;
  const int32_t y0_sub = sy - half_sub;

  const int32_t lo_x = ceil_px(x0_sub);
  const int32_t hi_x = floor_px(x0_sub + side_sub);
  const int32_t lo_y = ceil_px(y0_sub);
  const int32_t hi_y = floor_px(y0_sub + side_sub);

  o.min_x = lo_x > vp_x0 ? lo_x : vp_x0;
  o.max_x = hi_x < (vp_x0 + vp_w - 1) ? hi_x : (vp_x0 + vp_w - 1);
  o.min_y = lo_y > vp_y0 ? lo_y : vp_y0;
  o.max_y = hi_y < (vp_y0 + vp_h - 1) ? hi_y : (vp_y0 + vp_h - 1);
  o.covered = (o.min_x <= o.max_x) && (o.min_y <= o.max_y);
  return o;
}

}  // namespace part
}  // namespace zref
