// sprites.cpp — DrawForm marker quads (the wizards) + DrawPopulation
// particle sprites (the flow demo subset).
//
// Law:
//   spec/commands.zidl  DrawForm 0x0300 — "marker/billboard quads"; payload
//                       flags b0 = billboard (face the view camera), b1 =
//                       screen-space size (marker law) vs world-space size;
//                       semantic_weight feeds the Measure (recorded, no
//                       degrade ladder at Phase 3). DrawPopulation 0x0301 —
//                       flags b0 = point sprites, b1 = triangle sprites.
//   plan W3.5           "fixed 8x8 pattern scaled, wall-clamped per the demo
//                       law lineage" — the wave-2 Duo marker trajectory law.
//   charter §8          pass 3 (opaque forms) / pass 7 (particles: never
//                       occlude — depth test only).
//   spec/qformats.md    §3 single-rounding division (fx_div_exact), §8
//                       screenXY S 12.8 + guard band. Particle size U 0.4.4
//                       px (§10).
//
// [w3.5-software] readings (flagged for W3.7):
//   * billboard (b0) is the DEFAULT geometry: the quad is emitted in screen
//     space around the projected centre (a camera-facing quad by
//     construction), so b0 only documents intent at Phase 3;
//   * world-space size (b1 clear) uses the perspective divide of the size
//     lane at projection scale 1 — exact for the demo's authored matrices;
//   * wall-clamp: the quad CENTRE clamps into [half, extent-half] per axis
//     (a marker at the frame edge stays fully visible, sliding along the
//     wall); a pattern larger than the view centres instead of vanishing.

#include "internal.hpp"

#include <algorithm>

namespace zref {
namespace render {
namespace {

inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// nearest-neighbour 8x8 pattern blit into the working surface, depth-tested
// per pixel at `depth` (Q16.16), scissored to the viewport.
void blit_pattern_8x8(WorkSurface& surf, const Viewport& vp, int32_t x0_px, int32_t y0_px,
                      int32_t w_px, int32_t h_px, const FormPattern& form, int32_t depth,
                      bool depth_write) {
  if (w_px <= 0 || h_px <= 0) return;
  const int32_t min_x = std::max(x0_px, static_cast<int32_t>(vp.x0));
  const int32_t max_x = std::min(x0_px + w_px - 1, static_cast<int32_t>(vp.x0 + vp.w) - 1);
  const int32_t min_y = std::max(y0_px, static_cast<int32_t>(vp.y0));
  const int32_t max_y = std::min(y0_px + h_px - 1, static_cast<int32_t>(vp.y0 + vp.h) - 1);
  for (int32_t py = min_y; py <= max_y; ++py) {
    // texel row: floor((py - y0) * 8 / h) — nearest, deterministic
    const int32_t ty = static_cast<int32_t>((static_cast<int64_t>(py - y0_px) * 8) / h_px);
    for (int32_t px = min_x; px <= max_x; ++px) {
      const int32_t tx = static_cast<int32_t>((static_cast<int64_t>(px - x0_px) * 8) / w_px);
      if (form.mask[ty * 8 + tx] == 0) continue;  // colour key transparent
      const size_t idx = static_cast<size_t>(py) * surf.w + px;
      if (depth > surf.depth[idx]) {
        surf.rgb[idx * 3 + 0] = form.rgb[(ty * 8 + tx) * 3 + 0];
        surf.rgb[idx * 3 + 1] = form.rgb[(ty * 8 + tx) * 3 + 1];
        surf.rgb[idx * 3 + 2] = form.rgb[(ty * 8 + tx) * 3 + 2];
        if (depth_write) surf.depth[idx] = depth;
      }
    }
  }
}

// solid subpixel-rect block blit (point sprites), depth TEST only —
// particles never occlude (charter §8 pass 7). Scissored to the viewport.
void blit_pattern_block(WorkSurface& surf, const Viewport& vp, int32_t x0_sub, int32_t y0_sub,
                        int32_t w_sub, int32_t h_sub, uint8_t r, uint8_t g, uint8_t b,
                        int32_t depth) {
  if (w_sub <= 0 || h_sub <= 0) return;
  const int32_t min_x = std::max((x0_sub + 255) >> 8, static_cast<int32_t>(vp.x0));
  const int32_t max_x = std::min((x0_sub + w_sub) >> 8, static_cast<int32_t>(vp.x0 + vp.w) - 1);
  const int32_t min_y = std::max((y0_sub + 255) >> 8, static_cast<int32_t>(vp.y0));
  const int32_t max_y = std::min((y0_sub + h_sub) >> 8, static_cast<int32_t>(vp.y0 + vp.h) - 1);
  for (int32_t py = min_y; py <= max_y; ++py) {
    for (int32_t px = min_x; px <= max_x; ++px) {
      const size_t idx = static_cast<size_t>(py) * surf.w + px;
      if (depth > surf.depth[idx]) {
        surf.rgb[idx * 3 + 0] = r;
        surf.rgb[idx * 3 + 1] = g;
        surf.rgb[idx * 3 + 2] = b;
      }
    }
  }
}

}  // namespace

void draw_form_marker(WorkSurface& surf, const Viewport& vpp, const mat4fx& vp,
                      const FormPattern& form, const FormTransform& xf, uint16_t flags,
                      SatLedger* L) {
  const ProjOut c = project_vertex(vp, vpp, fx16{xf.x}, fx16{xf.y}, fx16{xf.z}, L);
  if (!c.in) return;  // behind the eye

  // half-extent in S 12.8 subpixel units
  int32_t half_sub;
  if (flags & 0x0002) {
    // screen-space size (marker law): the fx16 lane IS the pixel half-extent
    half_sub = rescale_s32(static_cast<int64_t>(xf.size), 8, L);
  } else {
    // world-space size: perspective divide at projection scale 1. The depth
    // lane c.s.d IS 1/w (Q16.16), so the divide is already done — the screen
    // half-extent is size * (1/w), a MULTIPLY. Dividing by c.s.d computes
    // size * w instead, which makes markers GROW with distance; only ortho
    // matrices (w == 1) hide the inversion.
    const fx16 w_fx = fx_mul(fx16{xf.size}, fx16{c.s.d}, L);
    half_sub = rescale_s32(static_cast<int64_t>(w_fx.raw), 8, L);
  }
  if (half_sub < 0) half_sub = -half_sub;
  if (half_sub == 0) return;

  const int32_t half_px = (half_sub + 128) >> 8;  // round-half-up to pixels
  // wall-clamp (demo law lineage): the centre stays where the quad is fully
  // visible; a quad larger than the view centres on the wall axis
  int32_t cx_px = c.s.x >> 8;
  int32_t cy_px = c.s.y >> 8;
  // The wall is the VIEWPORT's wall, so the clamp bounds carry the viewport
  // origin. DEFECT FIXED 2026-08-15: they were viewport-RELATIVE while cx/cy
  // are canvas coordinates, so any view not at the canvas origin had its
  // markers clamped into the OTHER view's region and then scissored away —
  // in Duo, view 1's markers were silently invisible. Latent until now
  // because view 1 was the only viewport with a non-zero origin.
  const int32_t vw = static_cast<int32_t>(vpp.w);
  const int32_t vh = static_cast<int32_t>(vpp.h);
  const int32_t vx0 = static_cast<int32_t>(vpp.x0);
  const int32_t vy0 = static_cast<int32_t>(vpp.y0);
  cx_px =
      2 * half_px <= vw ? clamp_i32(cx_px, vx0 + half_px, vx0 + vw - 1 - half_px) : vx0 + vw / 2;
  cy_px =
      2 * half_px <= vh ? clamp_i32(cy_px, vy0 + half_px, vy0 + vh - 1 - half_px) : vy0 + vh / 2;

  blit_pattern_8x8(surf, vpp, cx_px - half_px, cy_px - half_px, half_px * 2, half_px * 2, form,
                   c.s.d, /*depth_write=*/true);
}

void draw_population(WorkSurface& surf, const Viewport& vpp, const mat4fx& vp,
                     const Population& pop, uint16_t flags, SatLedger* L) {
  const bool points = (flags & 0x0001) != 0;
  const bool tris = (flags & 0x0002) != 0;
  if (!points && !tris) return;  // ladder-free L1 chooses statically (zidl)
  for (const Particle& p : pop.parts) {
    const ProjOut c = project_vertex(vp, vpp, fx16{p.x}, fx16{p.y}, fx16{p.z}, L);
    if (!c.in) continue;
    // particle size U 0.4.4 px (qformats §10): side = raw/16 px -> S 12.8
    const int32_t side_sub = static_cast<int32_t>(p.size) << 4;
    const int32_t half_sub = side_sub >> 1;
    if (points) {
      blit_pattern_block(surf, vpp, c.s.x - half_sub, c.s.y - half_sub, side_sub, side_sub, p.r,
                         p.g, p.b, c.s.d);
    }
    if (tris) {
      // triangle sprite: equilateral-ish 3-vertex fan in screen space
      const ScreenV a{c.s.x, c.s.y - side_sub, c.s.d, 0};
      const ScreenV b{c.s.x - (side_sub * 3) / 4, c.s.y + side_sub / 2, c.s.d, 0};
      const ScreenV cc{c.s.x + (side_sub * 3) / 4, c.s.y + side_sub / 2, c.s.d, 0};
      const TriMode m;  // opaque fill, but pass-7 law: test only, no write
      TriMode tm = m;
      tm.depth_write = false;
      raster_tri(surf, vpp, a, b, cc, p.r, p.g, p.b, tm);
    }
  }
}

}  // namespace render
}  // namespace zref
