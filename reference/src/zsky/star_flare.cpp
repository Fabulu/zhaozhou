// star_flare.cpp — the lens-flare chain: baked sprites, the per-light law,
// the ghost chain, and the bounded POST glow splat + tinted composite
// (spec/stars_and_flares.md §5, §9 placement).
//
// Law:
//   stars_and_flares.md §5
//     sprites: burst12/burst4 on a 512² canvas (streak 768×128), spoke i of
//       n at screen-fixed angle i·180/n, symmetric, half-length
//       L_i = R_canvas·SPOKE_LEN_SEQ[i mod 8]/54, 1-px additive Bresenham
//       lines value 32 saturating, 8× box downsample (burst 64×64,
//       streak 96×16). SPOKE_LEN_SEQ = {16,24,36,54,36,24,16,10}/16 —
//       the ×1.5 zig-zag frozen (deterministic, no shimmer); the streak is
//       the single c=0 survivor (the iconic anamorphic line).
//     per-light law: k = clamp(d/r, 5, 384); b = clamp(floor(log2(d/r))−2,
//       0, 7); sprite = b≤3 burst12 / b≤6 burst4 / streak
//     splat table: burst at +256/256 alpha 255; ghosts at −26/−77/−230
//       (Q8.8 of the light pos relative to the view centre), half-sizes
//       k·26/256, k·102/256, k·410/256, alpha 64; ghosts reuse the burst
//     glow += rescale_u(sprite_u8 · a, 8), saturating u16; tint at the
//       upscale-composite; flare_texels ≤ 16384/view/frame — a splat that
//       would exceed the remaining budget is dropped WHOLE (deterministic)
//   §26 note (D9): everything here is frozen-table sprite splats into the
//     bounded low-res glow plane — no live line rasteriser (refused), no
//     fragment program, no framebuffer sampling.
//   qformats.md §4 rounding (rescale_s32 / div_rhu_s64), §7.1 sin table for
//     the bake-time spoke directions.
//
// Bresenham note: the ONLY line rasteriser in this file runs at BAKE time
// into a private canvas (spec §5 explicitly bakes the fan; the §26-refused
// "live Bresenham flare fan" is per-frame framebuffer work, which this is
// not). Each canvas pixel of a line is visited once.

#include "zref/zref_star.hpp"

namespace zref {
namespace flare {

const uint8_t kSpokeLenSeq[8] = {16, 24, 36, 54, 36, 24, 16, 10};
const int16_t kGhostPos[3] = {-26, -77, -230};
const int16_t kGhostSize[3] = {26, 102, 410};

namespace {

// saturating write at one canvas pixel. Lines draw 255 in the supersampled
// canvas so the 8× mean downsample lands a 1-px spoke at the spec's 32/255
// arm brightness (implementation clarification 2 in stars_and_flares.md —
// drawing 32 up here yields 4/255 arms, an invisible flare).
inline void plot32(std::vector<uint8_t>& c, int w, int h, int x, int y) {
  if (x < 0 || y < 0 || x >= w || y >= h) return;
  c[static_cast<size_t>(y) * w + x] = 255;
}

// classic integer Bresenham, each pixel visited once, saturating
void line32(std::vector<uint8_t>& c, int w, int h, int x0, int y0, int x1, int y1) {
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
  const int dy = y1 > y0 ? y1 - y0 : y0 - y1;
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  int x = x0, y = y0;
  for (;;) {
    plot32(c, w, h, x, y);
    if (x == x1 && y == y1) break;
    const int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }
}

// 8× box downsample (mean, round_half_up/64) — §5
star::Sprite8 downsample8(const std::vector<uint8_t>& c, int w, int h) {
  star::Sprite8 out;
  out.w = static_cast<uint16_t>(w / 8);
  out.h = static_cast<uint16_t>(h / 8);
  out.pix.assign(static_cast<size_t>(out.w) * out.h, 0);
  for (int y = 0; y < out.h; ++y) {
    for (int x = 0; x < out.w; ++x) {
      int32_t sum = 0;
      for (int j = 0; j < 8; ++j)
        for (int i = 0; i < 8; ++i) sum += c[static_cast<size_t>(y * 8 + j) * w + x * 8 + i];
      out.pix[static_cast<size_t>(y) * out.w + x] = static_cast<uint8_t>((sum + 32) >> 6);
    }
  }
  return out;
}

// n-spoke burst on a square canvas: spoke i at angle i·180/n (angle16
// i·0x8000/n), endpoints at ±L_i along the table direction, drawn once
star::Sprite8 bake_burst(int n) {
  const int W = 512, H = 512, cx = 256, cy = 256, R = 256;
  std::vector<uint8_t> canvas(static_cast<size_t>(W) * H, 0);
  for (int i = 0; i < n; ++i) {
    const angle16 a{static_cast<uint16_t>((static_cast<uint32_t>(i) * 0x8000u) / n)};
    const int32_t cdir = fx_cos(a).raw, sdir = fx_sin(a).raw;
    const int32_t L = static_cast<int32_t>(
        detail::div_rhu_s64(static_cast<int64_t>(R) * kSpokeLenSeq[i % 8], 54));
    const int32_t ex = rescale_s32(static_cast<int64_t>(cdir) * L, 16, nullptr);
    const int32_t ey = rescale_s32(static_cast<int64_t>(sdir) * L, 16, nullptr);
    line32(canvas, W, H, cx - ex, cy - ey, cx + ex, cy + ey);
  }
  return downsample8(canvas, W, H);
}

star::Sprite8 bake_streak() {
  // the single c=0 survivor: one horizontal spoke on 768×128, R = 384
  const int W = 768, H = 128, cx = 384, cy = 64;
  std::vector<uint8_t> canvas(static_cast<size_t>(W) * H, 0);
  const int32_t L =
      static_cast<int32_t>(detail::div_rhu_s64(static_cast<int64_t>(384) * kSpokeLenSeq[0], 54));
  line32(canvas, W, H, cx - L, cy, cx + L, cy);
  return downsample8(canvas, W, H);
}

}  // namespace

const Sprites& sprites() {
  // baked once per process from frozen constants — byte-identical always
  static const Sprites s{bake_burst(12), bake_burst(4), bake_streak()};
  return s;
}

LightLaw light_law(int64_t d_milli, int64_t r_milli) {
  LightLaw law;
  if (r_milli <= 0) r_milli = 1;
  int64_t q = d_milli / r_milli;
  if (q < 0) q = 0;
  law.k = static_cast<int32_t>(q < 5 ? 5 : (q > 384 ? 384 : q));
  int b = 0;
  if (q >= 1) {
    int bits = 0;  // floor(log2(q))
    for (int64_t t = q; t > 1; t >>= 1) ++bits;
    b = bits - 2;
    if (b < 0) b = 0;
    if (b > 7) b = 7;
  }
  law.bucket = static_cast<uint8_t>(b);
  law.sprite = b <= 3 ? 0 : (b <= 6 ? 1 : 2);
  return law;
}

int emit(int32_t lx_px, int32_t ly_px, int32_t cx_px, int32_t cy_px, const LightLaw& law,
         Splat out[4]) {
  const Sprites& sp = sprites();
  const star::Sprite8& s = law.sprite == 0 ? sp.burst12 : (law.sprite == 1 ? sp.burst4 : sp.streak);
  // half_y follows the sprite aspect (only the streak is non-square)
  const auto aspect_y = [&](int32_t hx) {
    return static_cast<int32_t>(detail::div_rhu_s64(static_cast<int64_t>(hx) * s.h, s.w));
  };
  int n = 0;
  // burst at the light itself (+256/256), alpha 255
  out[n].cx_px = lx_px;
  out[n].cy_px = ly_px;
  out[n].half_x_px = law.k;
  out[n].half_y_px = aspect_y(law.k);
  out[n].alpha = 255;
  out[n].sprite = law.sprite;
  ++n;
  // the ghost chain, mirrored through the view centre; ONE round_half_up
  // per axis/size (rescale(g·delta, 8) — the §13 flare_ghost_anchor law)
  const int32_t dx = lx_px - cx_px;
  const int32_t dy = ly_px - cy_px;
  for (int i = 0; i < 3; ++i) {
    const int32_t hx = rescale_s32(static_cast<int64_t>(law.k) * kGhostSize[i], 8, nullptr);
    if (hx <= 0) continue;  // zero-size ghost dropped
    out[n].cx_px = cx_px + rescale_s32(static_cast<int64_t>(kGhostPos[i]) * dx, 8, nullptr);
    out[n].cy_px = cy_px + rescale_s32(static_cast<int64_t>(kGhostPos[i]) * dy, 8, nullptr);
    out[n].half_x_px = hx;
    out[n].half_y_px = aspect_y(hx);
    out[n].alpha = 64;
    out[n].sprite = law.sprite;
    ++n;
  }
  return n;
}

}  // namespace flare

namespace post {

int32_t flare_splat(GlowBuffer& g, const star::Sprite8& sprite, int32_t cx, int32_t cy,
                    int32_t half_x, int32_t half_y, uint8_t alpha, uint32_t budget_left) {
  if (half_x <= 0 || half_y <= 0 || alpha == 0) return 0;
  // clip the splat rect to the buffer, count the texels it will touch
  int32_t x0 = cx - half_x, x1 = cx + half_x;  // [x0, x1)
  int32_t y0 = cy - half_y, y1 = cy + half_y;
  const int32_t ox0 = x0, oy0 = y0;  // unclipped origin for sprite addressing
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > g.w) x1 = g.w;
  if (y1 > g.h) y1 = g.h;
  if (x0 >= x1 || y0 >= y1) return 0;
  const uint32_t texels = static_cast<uint32_t>(x1 - x0) * static_cast<uint32_t>(y1 - y0);
  if (texels > budget_left) return -1;  // §5 bound: dropped WHOLE
  const int32_t wx = 2 * half_x, wy = 2 * half_y;
  for (int32_t y = y0; y < y1; ++y) {
    const int32_t sy = static_cast<int32_t>((static_cast<int64_t>(y - oy0) * sprite.h) / wy);
    for (int32_t x = x0; x < x1; ++x) {
      const int32_t sx = static_cast<int32_t>((static_cast<int64_t>(x - ox0) * sprite.w) / wx);
      const uint8_t s8 = sprite.pix[static_cast<size_t>(sy) * sprite.w + sx];
      if (s8 == 0) continue;
      // glow += rescale_u(sprite·a, 8), saturating u16 (§5)
      const uint32_t add = (static_cast<uint32_t>(s8) * alpha + 128) >> 8;
      uint16_t& t = g.v[static_cast<size_t>(y) * g.w + x];
      const uint32_t v = t + add;
      t = static_cast<uint16_t>(v > 0xFFFF ? 0xFFFF : v);
    }
  }
  return static_cast<int32_t>(texels);
}

void glow_composite(uint8_t* rgb888, uint32_t w, uint32_t h, uint32_t vx0, uint32_t vy0,
                    uint32_t vw, uint32_t vh, const GlowBuffer& g, uint8_t tint_r, uint8_t tint_g,
                    uint8_t tint_b) {
  (void)h;
  const uint8_t tint[3] = {tint_r, tint_g, tint_b};
  for (uint32_t y = 0; y < vh; ++y) {
    const uint32_t gy = y >> 2;
    if (gy >= g.h) break;
    for (uint32_t x = 0; x < vw; ++x) {
      const uint32_t gx = x >> 2;
      if (gx >= g.w) break;
      uint32_t glow = g.v[static_cast<size_t>(gy) * g.w + gx];
      if (glow == 0) continue;
      if (glow > 255) glow = 255;
      uint8_t* dst = &rgb888[(static_cast<size_t>(vy0 + y) * w + vx0 + x) * 3];
      for (int c = 0; c < 3; ++c) {
        // rgb = sat(rgb + rescale_u(glow·tint, 8)) — §5 tinted composite
        const uint32_t v = dst[c] + ((glow * tint[c] + 128) >> 8);
        dst[c] = static_cast<uint8_t>(v > 255 ? 255 : v);
      }
    }
  }
}

}  // namespace post
}  // namespace zref
