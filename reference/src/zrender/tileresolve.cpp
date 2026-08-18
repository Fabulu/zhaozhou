// tileresolve.cpp — zref::TileResolve, the RASTER.RESOLVE oracle.
//
// Law: charter 8 resolve, reached by CALLING the frozen dither
// (zref::render::resolve_rgb565) rather than restating it — see the header
// (reference/include/zref/zref_tileresolve.hpp) for why, and for the
// Bayer-phase translation argument behind the padded surface.

#include "zref/zref_tileresolve.hpp"

#include <vector>

#include "internal.hpp"
#include "zhao_abi.h"

namespace zref {

TileResolve::Out TileResolve::tile(const uint64_t* words, int32_t tile_x, int32_t tile_y) {
  Out out;

  // The Bayer phase of the tile's top-left pixel. The RTL takes the low two
  // bits of a two's-complement origin; C++17 leaves `%` on a negative operand
  // truncating toward zero, so the phase is normalised explicitly here rather
  // than relying on a representation detail.
  const int pad_x = static_cast<int>(((tile_x % 4) + 4) % 4);
  const int pad_y = static_cast<int>(((tile_y % 4) + 4) % 4);

  const int w = pad_x + kTile;
  const int h = pad_y + kTile;

  std::vector<uint8_t> rgb888(static_cast<size_t>(w) * h * 3, 0);
  for (int row = 0; row < kTile; ++row) {
    for (int col = 0; col < kTile; ++col) {
      const TileStore::Word px = TileStore::Word::unpack(words[row * kTile + col]);
      const size_t i = (static_cast<size_t>(pad_y + row) * w + (pad_x + col)) * 3;
      rgb888[i + 0] = px.r;
      rgb888[i + 1] = px.g;
      rgb888[i + 2] = px.b;
      out.tag[row * kTile + col] = px.tag;  // never dithered (stars_and_flares 1)
    }
  }

  std::vector<uint8_t> out565(static_cast<size_t>(w) * h * 2, 0);
  zref::render::resolve_rgb565(rgb888.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                               out565.data());

  // Read the 16x16 sub-rect back out, and lay the framebuffer bytes down in
  // raster order, little-endian halfwords (video_rules.md 3) for the CRC.
  std::vector<uint8_t> fb(static_cast<size_t>(kPixels) * 2, 0);
  for (int row = 0; row < kTile; ++row) {
    for (int col = 0; col < kTile; ++col) {
      const size_t src = (static_cast<size_t>(pad_y + row) * w + (pad_x + col)) * 2;
      const int i = row * kTile + col;
      out.rgb565[i] = static_cast<uint16_t>(out565[src] | (out565[src + 1] << 8));
      fb[static_cast<size_t>(i) * 2 + 0] = out565[src];
      fb[static_cast<size_t>(i) * 2 + 1] = out565[src + 1];
    }
  }

  out.crc32c = zhao_abi::zhao_crc32c(0, fb.data(), fb.size());
  return out;
}

}  // namespace zref
