// zref_particle.hpp — PART.EXPAND's reference model.
//
// The ledger declared `zref::ParticleExpand` and that symbol never existed: one
// of the twenty-five phantoms in reports/PHANTOM_REFERENCES.md. This one is
// KIND 1 — the law was already shipped, inline, inside a function that does more
// than this block does.
//
// `zref::render::draw_population` (reference/src/zrender/sprites.cpp) projects
// each particle and then, on its `tris` branch, expands it into a three-vertex
// screen-space fan and rasterises it. THIS block is that expansion and nothing
// else: the projection is GEOM.PROJECT's and the rasterisation is GEOM.SETUP's
// and RASTER's.
//
// ---------------------------------------------------------------------------
// EXTRACTING AN INLINE LAW IS THE RISKY KIND OF REFERENCE, SO IT IS CHECKED
// ---------------------------------------------------------------------------
// The forty `zref::fieldir::*` names could be forwarded to a real callable. This
// one cannot: `draw_population` computes the fan inside a loop body and then
// immediately rasterises it, so there is no function to forward to and the law
// has to be RESTATED here.
//
// A restated law is a second implementation, and a second implementation that
// only its author ever compares against the first is exactly the failure the
// phantom-reference rules exist to catch. So
// `tests/particles/part_expand_directed.cpp` does not merely check RTL against
// this header — it also renders the same particles BOTH ways, once through
// `draw_population` and once by feeding this function's vertices to the same
// `raster_tri`, and requires the two surfaces to be identical pixel for pixel.
// If this header ever drifts from sprites.cpp, that check fails.
//
// ---------------------------------------------------------------------------
// THE LAW, verbatim from the `tris` branch
// ---------------------------------------------------------------------------
//     side_sub = size << 4          // U 0.4.4 px -> S 12.8 subpixels
//     a  = { x,                    y - side_sub    }
//     b  = { x - (side_sub*3)/4,   y + side_sub/2  }
//     c  = { x + (side_sub*3)/4,   y + side_sub/2  }
//
// All three carry the particle's own depth `d` and its colour. Three things
// about it are load-bearing and none of them is obvious:
//
//   1. THE FAN IS NOT EQUILATERAL AND MUST NOT BE "CORRECTED". The half-width is
//      3/4 of a side and the drop is half a side. Making it a true equilateral
//      triangle would be better geometry and would change every particle on
//      screen.
//      (Both divisions are EXACT for every legal input, which is not obvious:
//      `side_sub` is `size << 4`, hence a multiple of 16, so neither the /4 nor
//      the /2 ever discards anything. A mutation that rounded instead of
//      truncating survives the whole suite because no input distinguishes them
//      -- an equivalent mutant, recorded so the next reader does not go looking
//      for the missing test.)
//   2. `side_sub` IS `size << 4`, NOT `size << 8`. Particle size is U 0.4.4
//      pixels (qformats §10), so the raw byte is sixteenths of a pixel; shifting
//      by 4 lands it in S 12.8 subpixels. Shifting by 8 would treat it as whole
//      pixels and make every particle sixteen times too big.
//   3. DEPTH IS TESTED, NEVER WRITTEN. `draw_population` sets
//      `depth_write = false` on its TriMode, with the comment "pass-7 law: test
//      only, no write". Particles occlude nothing behind them; a particle that
//      wrote depth would carve a hole in whatever drew after it.
//
// A particle BEHIND THE EYE is skipped entirely (`if (!c.in) continue`), which
// is why `expand_polygon` reports whether it produced anything rather than
// emitting a degenerate triangle.
#pragma once

#include <cstdint>

namespace zref {
namespace part {

/** One expanded screen-space vertex. Mirrors the fields `raster_tri` reads. */
struct ExpandedVertex {
  int32_t x = 0;  // S 12.8 canvas subpixels
  int32_t y = 0;
  int32_t d = 0;  // Q16.16 1/w, the particle's own depth
};

/** What one particle expands into. */
struct PolyExpand {
  bool emitted = false;  // false: the particle was behind the eye
  ExpandedVertex a, b, c;
  uint8_t r = 0, g = 0, b_ = 0;
  // The TriMode law: opaque fill, depth TESTED, depth NOT written.
  static constexpr bool kDepthTest = true;
  static constexpr bool kDepthWrite = false;
};

/**
 * Expand one PROJECTED particle into its three-vertex fan.
 *
 * `sx`/`sy`/`sd` are the projection's screen vertex; `in` is its
 * behind-the-eye verdict. Projection belongs to GEOM.PROJECT and is not redone
 * here — this function is the expansion alone.
 */
inline PolyExpand expand_polygon(bool in, int32_t sx, int32_t sy, int32_t sd, uint8_t size,
                                 uint8_t r, uint8_t g, uint8_t b) {
  PolyExpand o;
  if (!in) return o;  // skipped, exactly as draw_population `continue`s
  const int32_t side_sub = static_cast<int32_t>(size) << 4;
  o.emitted = true;
  o.a = ExpandedVertex{sx, sy - side_sub, sd};
  o.b = ExpandedVertex{sx - (side_sub * 3) / 4, sy + side_sub / 2, sd};
  o.c = ExpandedVertex{sx + (side_sub * 3) / 4, sy + side_sub / 2, sd};
  o.r = r;
  o.g = g;
  o.b_ = b;
  return o;
}

}  // namespace part
}  // namespace zref
