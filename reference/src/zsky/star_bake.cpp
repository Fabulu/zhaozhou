// star_bake.cpp — the ARM/compile-time celestial bakes: starface (QT-VR
// orthographic sphere resample), corona sprite, and the asin16 inverse of
// the frozen sin table (spec/stars_and_flares.md §3/§4, §9 placement).
//
// Law:
//   stars_and_flares.md §3  starface bake: cylindrical grid 256×64 of
//       (noise2_hash(x, y, texture_seed, 0) >> 26) & 0x3E, 3×3 box smooth
//       (wrap x / clamp y, 2 passes, 5 for compact classes), orthographic
//       resample through the QT-VR offset map into the 120-half-texel disc,
//       index 0 outside, intensity 1..63 inside
//   stars_and_flares.md §4  corona bake: rr = isqrt((2x−127)²+(2y−127)²),
//       fgm_h = round_half_up(R_h·core16, 16), k = round_half_up(63<<16,
//       R_h−fgm_h), LINEAR falloff pix = 63 − rescale_u((rr−fgm_h)·k, 16)
//   qformats.md §7.1  the ONE 257-entry sin table — asin16 below is a
//       binary search over fx_sin, NOT a second approximation (§29-6)
//   qformats.md §7.2  isqrt (exact floor), §7.5 noise2_hash, §4 rounding
//
// [phase3-preview]: the offset map is computed here at bake time in pure
// integers (the spec's "compiled QT-VR offset map" — compile-time per §9;
// a committed table would be byte-identical since every step below is
// deterministic integer arithmetic). Mip chains are asset-pack products;
// the preview samples level 0 nearest.

#include "zref/zref_star.hpp"

namespace zref {
namespace star {

int32_t asin16(int32_t y_fx_raw) {
  int32_t ay = y_fx_raw < 0 ? -y_fx_raw : y_fx_raw;
  if (ay > 65536) ay = 65536;
  // largest a in [0, 0x4000] with fx_sin(a).raw <= ay (fx_sin is monotone
  // on the first quarter turn; fx_sin(0x4000) = 0x10000 exactly)
  uint32_t lo = 0, hi = 0x4000;
  while (lo < hi) {
    const uint32_t mid = (lo + hi + 1) >> 1;
    if (fx_sin(angle16{static_cast<uint16_t>(mid)}).raw <= ay)
      lo = mid;
    else
      hi = mid - 1;
  }
  return y_fx_raw < 0 ? -static_cast<int32_t>(lo) : static_cast<int32_t>(lo);
}

namespace {

// §3 step 2: one 3×3 box-smooth pass over the 256×64 cylindrical grid —
// wrap in x (the cylinder seam), clamp in y (the poles), round_half_up/9.
void smooth_pass(uint8_t g[64][256]) {
  static uint8_t tmp[64][256];
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 256; ++x) {
      int32_t sum = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        int yy = y + dy;
        if (yy < 0) yy = 0;
        if (yy > 63) yy = 63;
        for (int dx = -1; dx <= 1; ++dx) {
          const int xx = (x + dx + 256) & 255;
          sum += g[yy][xx];
        }
      }
      tmp[y][x] = static_cast<uint8_t>((sum + 4) / 9);
    }
  }
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 256; ++x) g[y][x] = tmp[y][x];
}

}  // namespace

Sprite8 starface(uint32_t texture_seed, uint8_t smooth_passes) {
  // §3 step 1: the PCG grid (one hash discipline; the character comes from
  // the smoothing, not the generator)
  static uint8_t g[64][256];
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 256; ++x)
      g[y][x] = static_cast<uint8_t>(
          (noise2_hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), texture_seed, 0) >> 26) &
          0x3E);
  for (int p = 0; p < smooth_passes; ++p) smooth_pass(g);

  // §3 step 3: orthographic resample through the QT-VR offset map into the
  // 120-half-texel disc. For each texel the half-texel offsets from the
  // centre are dx = 2x−127, dy = 2y−127; inside ⟺ dx²+dy² < 120². Latitude
  // from dy, longitude from dx against the row chord rx = isqrt(120²−dy²);
  // both through asin16 (the front hemisphere maps to u ∈ [0,128] of the
  // 256-texel cylinder). Steps 1..2 are the grid; this is the "offset map"
  // applied — identical arithmetic to a committed compile-time table.
  Sprite8 out;
  out.w = out.h = 128;
  out.pix.assign(128 * 128, 0);
  for (int y = 0; y < 128; ++y) {
    const int32_t dy = 2 * y - 127;
    for (int x = 0; x < 128; ++x) {
      const int32_t dx = 2 * x - 127;
      if (dx * dx + dy * dy >= 120 * 120) continue;  // index 0 outside
      const int32_t vfx =
          static_cast<int32_t>(detail::div_rhu_s64(static_cast<int64_t>(dy) << 16, 120));
      const int32_t a_lat = asin16(vfx);
      int32_t v = ((a_lat + 0x4000) * 64) >> 15;
      if (v > 63) v = 63;
      const int32_t rx =
          static_cast<int32_t>(isqrt_u32(static_cast<uint32_t>(120 * 120 - dy * dy)));
      const int32_t ufx =
          static_cast<int32_t>(detail::div_rhu_s64(static_cast<int64_t>(dx) << 16, rx));
      const int32_t a_lon = asin16(ufx);
      const int32_t u = (((a_lon + 0x4000) * 128) >> 15) & 255;
      // §3 step 4: index 0 transparent; intensity 1..63 (grid is 0..62)
      out.pix[static_cast<size_t>(y) * 128 + x] = static_cast<uint8_t>(1 + g[v][u]);
    }
  }
  return out;
}

Sprite8 corona_sprite(uint8_t core16) {
  // §4 bake law, 128×128, half-texel radius R_h = 120, LINEAR falloff
  constexpr int32_t R_h = 120;
  const int32_t fgm_h =
      static_cast<int32_t>(detail::div_rhu_s64(static_cast<int64_t>(R_h) * core16, 16));
  const int32_t k =
      static_cast<int32_t>(detail::div_rhu_s64(static_cast<int64_t>(63) << 16, R_h - fgm_h));
  Sprite8 out;
  out.w = out.h = 128;
  out.pix.assign(128 * 128, 0);
  for (int y = 0; y < 128; ++y) {
    const int32_t dy = 2 * y - 127;
    for (int x = 0; x < 128; ++x) {
      const int32_t dx = 2 * x - 127;
      const int32_t rr = static_cast<int32_t>(isqrt_u32(static_cast<uint32_t>(dx * dx + dy * dy)));
      uint8_t pix = 0;
      if (rr < R_h) {
        if (rr <= fgm_h) {
          pix = 63;
        } else {
          // rescale_u(x, 16) = (x + 1<<15) >> 16 (qformats §4, unsigned)
          const int64_t drop = ((static_cast<int64_t>(rr - fgm_h) * k) + (1 << 15)) >> 16;
          pix = static_cast<uint8_t>(drop >= 63 ? 0 : 63 - drop);
        }
      }
      out.pix[static_cast<size_t>(y) * 128 + x] = pix;
    }
  }
  return out;
}

// ---- PROPOSED §4 amendment: the atmospheric bloom profile ------------------
//
// NOT RATIFIED. `corona_sprite` above is the frozen §4 law and is untouched;
// this is a second profile offered as evidence for an amendment, and nothing
// in the ledger cites it.
//
// WHY A SECOND PROFILE IS NEEDED. §4's falloff is LINEAR in radius, which
// draws a cone: a findable edge at the rim and a core that shrinks to a point.
// That is a star seen through nothing, and it is what every space class in the
// gamut wants. A sun seen from a planet's SURFACE through a thick atmosphere
// is a different object. It has no resolvable disc at all. It is a formless
// bloom, mostly sitting below the horizon, whose light bleeds a long way up
// into the sky with no boundary anywhere. A cone cannot express that, and
// scaling a cone up only produces a bigger cone.
//
// THE PROFILE. Intensity `63·a² / (a² + rr²)` — a Lorentzian, chosen because
// it holds a broad saturated core and then decays without ever reaching zero
// inside the sprite, which is exactly the "no findable edge" property. `half_h`
// is the half-intensity radius in half-texels: intensity is 63 at the centre,
// 32 at `half_h`, and still around 2 at the 120 half-texel rim.
//
// All integer, one rounding, no host floats (charter §29-7). The cost at
// runtime is unchanged and remains zero: §4's point is that the LUT IS the
// texture, so a different profile is bake time and nothing else.
Sprite8 corona_sprite_bloom(uint8_t half_h) {
  constexpr int32_t R_h = 120;
  const int32_t a2 = static_cast<int32_t>(half_h) * static_cast<int32_t>(half_h);
  Sprite8 out;
  out.w = out.h = 128;
  out.pix.assign(128 * 128, 0);
  for (int y = 0; y < 128; ++y) {
    const int32_t dy = 2 * y - 127;
    for (int x = 0; x < 128; ++x) {
      const int32_t dx = 2 * x - 127;
      const int32_t rr = static_cast<int32_t>(isqrt_u32(static_cast<uint32_t>(dx * dx + dy * dy)));
      uint8_t pix = 0;
      if (rr < R_h) {
        const int64_t den = static_cast<int64_t>(a2) + static_cast<int64_t>(rr) * rr;
        const int64_t v = (static_cast<int64_t>(63) * a2 + den / 2) / den;
        pix = static_cast<uint8_t>(v < 1 ? 1 : (v > 63 ? 63 : v));
      }
      out.pix[static_cast<size_t>(y) * 128 + x] = pix;
    }
  }
  return out;
}

}  // namespace star
}  // namespace zref
