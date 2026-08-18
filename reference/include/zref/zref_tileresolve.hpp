// zref_tileresolve.hpp — RASTER.RESOLVE reference model (phase 4, ZH-024).
//
// The scalar oracle named by design/blocks.yml (`reference_model:
// zref::TileResolve`) and design/contracts/RASTER.RESOLVE.md.
//
// IT IS NOT A SECOND IMPLEMENTATION OF THE DITHER. The body calls
// zref::render::resolve_rgb565 (reference/src/zrender/resolve.cpp) — the
// frozen 4x4 Bayer matrix, the three per-channel quantizers with green's
// doubled amplitude, and the 2026-08-16 white-rail clamps — and reads the
// resolved halfwords back out. There is exactly one dither law in this
// repository and this is a view onto it, so "RTL == zref::TileResolve" is
// literally "RTL == the charter 8 resolve".
//
// PHASE TRANSLATION. resolve_rgb565 indexes the Bayer matrix by the pixel's
// position in the SURFACE it is handed, i.e. (y & 3, x & 3) relative to that
// buffer's origin. A tile whose top-left pixel is (tile_x, tile_y) must use
// the ABSOLUTE phase ((tile_y + row) & 3, (tile_x + col) & 3). The model
// therefore resolves a buffer padded by (tile_x & 3, tile_y & 3) on the top
// and left and reads the 16x16 sub-rect back: the pad shifts every tile pixel
// to buffer position (tile_x&3 + col, tile_y&3 + row), whose low two bits are
// exactly (tile_x + col) & 3 and (tile_y + row) & 3. Same matrix, same
// quantizer, same clamps, right phase — and NO dither arithmetic here.
// (The same shape of trick as zref::EdgeWalk's tile translation.)
//
// The RTL is handed the untranslated tile plus the origin, so a bug in its
// phase arithmetic still shows up as a pixel mismatch.

#pragma once

#include <cstdint>

#include "zref/zref_tilestore.hpp"

namespace zref {

/** Ordered-dither RGB565 resolve of one finished 16x16 tile, plus its CRC. */
struct TileResolve {
  static constexpr int kTile = TileStore::kTile;
  static constexpr int kPixels = TileStore::kWords;

  /** The resolved tile: the fb_tiles stream and the tile_crc handoff. */
  struct Out {
    uint16_t rgb565[kPixels] = {};  // raster order, index = row*16 + col
    uint8_t tag[kPixels] = {};      // the effect tag, NEVER dithered
    uint32_t crc32c = 0;            // CRC-32C over the 512 framebuffer bytes

    bool operator==(const Out& o) const {
      if (crc32c != o.crc32c) return false;
      for (int i = 0; i < kPixels; ++i)
        if (rgb565[i] != o.rgb565[i] || tag[i] != o.tag[i]) return false;
      return true;
    }
    bool operator!=(const Out& o) const { return !(*this == o); }
  };

  /**
   * Resolve one tile. `words` is 256 packed TileStore words in raster order;
   * (tile_x, tile_y) is the tile's top-left PIXEL in the surface, which sets
   * the Bayer phase. The CRC is CRC-32C (capture_format.md 2) over the 512
   * resolved framebuffer bytes in raster order, little-endian halfwords
   * (video_rules.md 3) — the same parameter set and byte order the displayed
   * frame CRC uses, not a new variant.
   */
  static Out tile(const uint64_t* words, int32_t tile_x, int32_t tile_y);
};

}  // namespace zref
