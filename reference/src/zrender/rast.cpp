// rast.cpp — projection + the §8 edge-function raster core.
//
// Law: spec/qformats.md
//   §2  mat4fx x vec4: four 32x32 products summed EXACTLY in s128 per row,
//       then ONE rescale(.,16) + saturate (zref::mat4_vec4 — called, never
//       re-derived)
//   §3  fx_div_exact / fx_mad single rounding
//   §4  round_half_up signed division (div_rhu_s128 below)
//   §8  screenXY: S 12.8, rescale(.,8) + clamp to the ±2048 px guard band;
//       edge functions: s64 setup in subpixel^2 (Giesen bound 2^43-2 at p=21),
//       E' = E0 >> 8 evaluated at pixel centres (low 8 bits constant per
//       edge), D3D top-left fill convention (bias 0 / -1),
//       exact incremental stepping; depth test strict-greater, clear 0.
//       The fill comparison is `E0 + bias >= 0` on the EXACT s64 edge value —
//       see the note at the test below; a strict `>` drops every shared-edge
//       pixel on BOTH sides, and biasing the floored E' drops sub-unit
//       strictly-interior pixels on both sides.
//       The scan bbox is the pixel-CENTRE range, not ceil/floor of the vertex
//       extent — see the note at the bbox; getting that wrong opens a 1px
//       crack at every shared vertical/horizontal seam.
// The depth lane is Q16.16 1/w (plan W3.5/D7 — NOT the Phase-4 invw24
// pipeline; the switch is a frozen one-line change in project_vertex).

#include "internal.hpp"

#include <algorithm>

namespace zref {
namespace render {

int32_t div_rhu_s128(__int128 n, __int128 d) {
  if (d < 0) {
    n = -n;
    d = -d;
  }
  __int128 h = n + d / 2;
  __int128 q = h / d;
  __int128 r = h % d;
  if (r != 0 && r < 0) q -= 1;  // floor semantics (§4)
  return static_cast<int32_t>(q > INT32_MAX ? INT32_MAX : (q < INT32_MIN ? INT32_MIN : q));
}

ProjOut project_vertex(const mat4fx& vp, const Viewport& vp_px, fx16 x, fx16 y, fx16 z,
                       SatLedger* L) {
  ProjOut o;
  const vec4fx clip = mat4_vec4(vp, vec4fx{x, y, z, fx16{1 << 16}}, L);
  if (clip.w.raw <= 0) return o;  // behind the eye — Phase-3 cull
  const fx16 ndc_x = fx_div_exact(clip.x, clip.w, L);
  const fx16 ndc_y = fx_div_exact(clip.y, clip.w, L);
  // viewport: ndc -1..+1 -> [x0, x0+w); +Y NDC = +Y canvas row (top-left
  // origin, video_rules.md §2). fx_mad = ONE rounding (§3).
  const int32_t hw = static_cast<int32_t>(vp_px.w) * (1 << 15);  // (w/2)<<16
  const int32_t hh = static_cast<int32_t>(vp_px.h) * (1 << 15);
  const int32_t cx = (static_cast<int32_t>(vp_px.x0) + static_cast<int32_t>(vp_px.w >> 1)) << 16;
  const int32_t cy = (static_cast<int32_t>(vp_px.y0) + static_cast<int32_t>(vp_px.h >> 1)) << 16;
  o.s.x = to_screen_xy(fx_mad(ndc_x, fx16{hw}, fx16{cx}, L), L);
  o.s.y = to_screen_xy(fx_mad(ndc_y, fx16{hh}, fx16{cy}, L), L);
  o.s.d = fx_div_exact(fx16{1 << 16}, clip.w, L).raw;  // Q16.16 1/w (D7)
  o.in = true;
  return o;
}

namespace {

// §8 fill convention: for a positive-area (clockwise, y-down) triangle, an
// edge is top-left iff horizontal going right (top) or going down (left).
inline bool edge_top_left(const ScreenV& p, const ScreenV& q) {
  return (p.y == q.y) ? (p.x < q.x) : (p.y < q.y);
}

inline int64_t orient(const ScreenV& a, const ScreenV& b, int64_t px, int64_t py) {
  return (static_cast<int64_t>(b.x) - a.x) * (py - a.y) -
         (static_cast<int64_t>(b.y) - a.y) * (px - a.x);
}

inline uint8_t sat_u8(int32_t v) { return static_cast<uint8_t>(v > 255 ? 255 : (v < 0 ? 0 : v)); }

}  // namespace

// The §8 scan box: exactly the pixels whose CENTRE can lie in the triangle,
// scissored to the viewport (declaration + rationale: internal.hpp).
//
// The centre of pixel p sits at 256*p + 128 subpixels (§8). So the first
// candidate column is the smallest p with 256p + 128 >= v_min, i.e.
// p = ceil((v_min - 128)/256) = (v_min + 127) >> 8, and the last is the
// largest p with 256p + 128 <= v_max, i.e. p = (v_max - 128) >> 8.
//
// DEFECT FIXED 2026-08-15: this used to be ceil(v_min/256) = (v_min+255)>>8,
// which is one column too far right whenever v_min's subpixel fraction lies
// in (0,128] — the pixel centre is still inside, but the loop never visits
// it. On a shared vertical seam between two quads (sky drum columns, terrain
// grid columns) the left quad's edge functions correctly reject that pixel
// and the right quad's bbox refuses to test it, so NOBODY covers it and the
// clear colour shows through as a full-height 1px crack. Same on Y.
// Instrumented proof: sky drum seam at subpixel x = 9547 (px 37.293), pixel
// (37,60) — the right-hand quads' edge test says INSIDE (E' = 6189/26206/403
// against biases 0/0/-1) but the old min_x = 38 excluded the column; the
// left-hand quads say outside. Tests: render_directed.cpp
// test_seam_subpixel_sweep / test_no_seam_cracks_drum / _terrain.
// The max side was conservative rather than wrong (a centre strictly right
// of every vertex is always rejected by an edge function); it is tightened
// to the same law so the bbox is exactly "pixel centres in [v_min, v_max]".
ScanBox scan_bbox(const ScreenV& A, const ScreenV& B, const ScreenV& C, const Viewport& vp) {
  ScanBox b;
  b.min_x = std::max<int32_t>((std::min({A.x, B.x, C.x}) + 127) >> 8, static_cast<int32_t>(vp.x0));
  b.max_x = std::min<int32_t>((std::max({A.x, B.x, C.x}) - 128) >> 8,
                              static_cast<int32_t>(vp.x0 + vp.w) - 1);
  b.min_y = std::max<int32_t>((std::min({A.y, B.y, C.y}) + 127) >> 8, static_cast<int32_t>(vp.y0));
  b.max_y = std::min<int32_t>((std::max({A.y, B.y, C.y}) - 128) >> 8,
                              static_cast<int32_t>(vp.y0 + vp.h) - 1);
  b.empty = (b.min_x > b.max_x) || (b.min_y > b.max_y);
  return b;
}

void raster_tri(WorkSurface& s, const Viewport& vp, const ScreenV& A0, const ScreenV& B0,
                const ScreenV& C0, uint8_t r, uint8_t g, uint8_t b, const TriMode& m,
                const TextureSpan* tex) {
  ScreenV A = A0, B = B0, C = C0;
  int64_t area = orient(A, B, C.x, C.y);  // 2A in subpixel^2 (s64 setup, §8)
  if (area == 0) return;
  if (area < 0) {  // double-sided: flip the winding (Phase-3, see header)
    ScreenV t = B;
    B = C;
    C = t;
    area = -area;
  }

  // bbox in whole pixels, scissored to the viewport — scan_bbox() below, the
  // ONE site of this law (GEOM.CLIP's viewport test calls the same function).
  const ScanBox bb = scan_bbox(A, B, C, vp);
  if (bb.empty) return;
  const int32_t min_x = bb.min_x;
  const int32_t max_x = bb.max_x;
  const int32_t min_y = bb.min_y;
  const int32_t max_y = bb.max_y;

  // per-pixel edge deltas (one pixel = 256 subpixel units, §8)
  const int64_t dw0_dx = -(static_cast<int64_t>(C.y) - B.y) * 256;
  const int64_t dw1_dx = -(static_cast<int64_t>(A.y) - C.y) * 256;
  const int64_t dw2_dx = -(static_cast<int64_t>(B.y) - A.y) * 256;

  // top-left biases (§8): E0(p) + bias >= 0, bias 0 / -1, in subpixel^2 units
  const int64_t bias0 = edge_top_left(B, C) ? 0 : -1;
  const int64_t bias1 = edge_top_left(C, A) ? 0 : -1;
  const int64_t bias2 = edge_top_left(A, B) ? 0 : -1;

  // affine scanline gradients for depth and (optional) alpha: ONE
  // round_half_up division per attribute at setup, exact s32 stepping
  // afterwards (the §8 "stepped" model — deterministic, and what the RTL
  // tile walker does). Row starts re-evaluate the full barycentric form.
  int32_t d_grad_x = 0;
  int32_t a_grad_x = 0;
  if (!m.use_fixed_depth) {
    d_grad_x =
        div_rhu_s128(static_cast<__int128>(dw0_dx) * A.d + static_cast<__int128>(dw1_dx) * B.d +
                         static_cast<__int128>(dw2_dx) * C.d,
                     area);
  }
  if (m.interp_alpha) {
    a_grad_x =
        div_rhu_s128(static_cast<__int128>(dw0_dx) * A.a + static_cast<__int128>(dw1_dx) * B.a +
                         static_cast<__int128>(dw2_dx) * C.a,
                     area);
  }
  // affine UV gradients (terrain texturing, terrain_rules §6.2): the SAME
  // one-rounding plane setup as depth/alpha; Phase-5 brings the
  // perspective-correct divide (charter §8 build order 7 — affine within a
  // 2 m cell is the documented Phase-3 stand-in)
  int32_t u_grad_x = 0, v_grad_x = 0;
  if (tex != nullptr) {
    u_grad_x =
        div_rhu_s128(static_cast<__int128>(dw0_dx) * A.u + static_cast<__int128>(dw1_dx) * B.u +
                         static_cast<__int128>(dw2_dx) * C.u,
                     area);
    v_grad_x =
        div_rhu_s128(static_cast<__int128>(dw0_dx) * A.v + static_cast<__int128>(dw1_dx) * B.v +
                         static_cast<__int128>(dw2_dx) * C.v,
                     area);
  }

  for (int32_t py = min_y; py <= max_y; ++py) {
    const int64_t cy = (static_cast<int64_t>(py) << 8) + 128;  // pixel centre
    const int64_t cx0 = (static_cast<int64_t>(min_x) << 8) + 128;
    int64_t w0 = orient(B, C, cx0, cy);
    int64_t w1 = orient(C, A, cx0, cy);
    int64_t w2 = orient(A, B, cx0, cy);
    // row-start interpolated values: full bary eval (one §4 division each),
    // then exact s32 steps (d/alpha are affine; w_i step exactly per pixel)
    int32_t d = 0, a = 0, u = 0, v = 0;
    if (tex != nullptr) {
      u = div_rhu_s128(static_cast<__int128>(w0) * A.u + static_cast<__int128>(w1) * B.u +
                           static_cast<__int128>(w2) * C.u,
                       area);
      v = div_rhu_s128(static_cast<__int128>(w0) * A.v + static_cast<__int128>(w1) * B.v +
                           static_cast<__int128>(w2) * C.v,
                       area);
    }
    if (!m.use_fixed_depth) {
      d = div_rhu_s128(static_cast<__int128>(w0) * A.d + static_cast<__int128>(w1) * B.d +
                           static_cast<__int128>(w2) * C.d,
                       area);
    }
    if (m.interp_alpha) {
      a = div_rhu_s128(static_cast<__int128>(w0) * A.a + static_cast<__int128>(w1) * B.a +
                           static_cast<__int128>(w2) * C.a,
                       area);
    }
    for (int32_t px = min_x; px <= max_x; ++px) {
      // §8: inside ⟺ E0 + bias >= 0, bias 0 (top-left) / -1, applied to the
      // EXACT s64 edge value in subpixel^2 units. That is literally the D3D
      // convention: accept E0 > 0 always, and accept E0 == 0 only on a
      // top-left edge (bias -1 turns `>= 0` into `>= 1` for the other side).
      //
      // DEFECT FIXED 2026-08-15: the bias used to be applied to the FLOORED
      // E' = E0 >> 8. Flooring merges "strictly inside by less than one
      // subpixel^2 unit" (E0 in [1,255] -> E' = 0) with "exactly on the edge"
      // (E0 = 0 -> E' = 0), and merges "strictly outside" (E0 in [-255,-1] ->
      // E' = -1) with a full unit outside. On a shared edge the two sides see
      // (E0, -E0), i.e. (E', -E'-1): for E0 in [1,255] the non-top-left owner
      // rejects E' = 0 (bias -1) and the top-left side sees E' = -1 and also
      // rejects — a hole on a pixel that is strictly inside one triangle.
      // Found by tests/render/render_directed.cpp test_seam_subpixel_sweep
      // (seam fraction 89/256, pixel (14,8), E0 = -128/+128).
      //
      // The RTL freeze keeps its s32 tile stepping: the low 8 bits of E0 are
      // constant per edge (§8), so with r = E0 & 255, `E0 >= 0` is `E' >= 0`
      // and `E0 >= 1` is `E' > 0 || (E' == 0 && r != 0)` — one extra constant
      // bit per edge, no wider datapath.
      const bool inside = (w0 + bias0 >= 0) && (w1 + bias1 >= 0) && (w2 + bias2 >= 0);
      if (inside) {
        const int32_t dz = m.use_fixed_depth ? m.fixed_depth : d;
        size_t idx = static_cast<size_t>(py) * s.w + px;
        if (!m.depth_test || dz > s.depth[idx]) {
          uint8_t* dst = &s.rgb[idx * 3];
          switch (m.blend) {
            case BlendMode::kOpaque:
              if (tex != nullptr && tex->ts != nullptr) {
                // ONE primary sample (charter §15/§26): mirrored-repeat fold
                // to the texel (zref::terrain::mirror_texel — the §6.2 frozen
                // law), optional per-texel Mosaic pick between the cell's two
                // candidates, then the per-primitive modulation with ONE
                // rounding per channel
                const int32_t tx = terrain::mirror_texel(u);
                const int32_t ty = terrain::mirror_texel(v);
                const uint8_t tile = tex->mosaic
                                         ? terrain::mosaic_pick(tex->tile_a, tex->tile_b,
                                                                tex->weight, u >> 10, v >> 10)
                                         : tex->tile_a;
                const uint8_t ci8 =
                    tex->ts->tiles[tile][(static_cast<size_t>(ty) << 6) + static_cast<size_t>(tx)];
                const uint16_t c565 = tex->ts->palette[ci8];
                const uint32_t r5 = (c565 >> 11) & 0x1F, g6 = (c565 >> 5) & 0x3F, b5 = c565 & 0x1F;
                dst[0] = sat_u8((((r5 * 255 + 15) / 31) * tex->mod_r + 32768) >> 16);
                dst[1] = sat_u8((((g6 * 255 + 31) / 63) * tex->mod_g + 32768) >> 16);
                dst[2] = sat_u8((((b5 * 255 + 15) / 31) * tex->mod_b + 32768) >> 16);
              } else {
                dst[0] = r;
                dst[1] = g;
                dst[2] = b;
              }
              break;
            case BlendMode::kAlpha: {
              // sky_cloud_fade (sky_and_beams.md §1.1):
              // out = dst*(1-a) + src*a, a = Q16.16, one rounding
              const int32_t ia = 65536 - a;
              dst[0] = sat_u8((dst[0] * ia + r * a + 32768) >> 16);
              dst[1] = sat_u8((dst[1] * ia + g * a + 32768) >> 16);
              dst[2] = sat_u8((dst[2] * ia + b * a + 32768) >> 16);
              break;
            }
            case BlendMode::kAdditive: {
              // sun_additive (§1.1): dst = sat(dst + src*a)
              dst[0] = sat_u8(dst[0] + ((r * a + 32768) >> 16));
              dst[1] = sat_u8(dst[1] + ((g * a + 32768) >> 16));
              dst[2] = sat_u8(dst[2] + ((b * a + 32768) >> 16));
              break;
            }
          }
          if (m.depth_write) s.depth[idx] = dz;
        }
      }
      w0 += dw0_dx;
      w1 += dw1_dx;
      w2 += dw2_dx;
      d += d_grad_x;
      a += a_grad_x;
      if (tex != nullptr) {
        u += u_grad_x;
        v += v_grad_x;
      }
    }
  }
}

}  // namespace render
}  // namespace zref
