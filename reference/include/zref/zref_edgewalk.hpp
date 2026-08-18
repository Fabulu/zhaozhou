// zref_edgewalk.hpp — RASTER.EDGEWALK reference model (phase 4, ZH-022).
//
// The scalar oracle named by design/blocks.yml (`reference_model:
// zref::EdgeWalk`) and design/contracts/RASTER.EDGEWALK.md: given one
// triangle in S 12.8 screen subpixels and one 16×16 tile origin in pixels,
// the EXACT `spec/qformats.md` §8 coverage of that tile.
//
// IT IS NOT A SECOND IMPLEMENTATION OF THE FILL LAW. The body calls
// zref::render::raster_tri (reference/src/zrender/rast.cpp) — the frozen §8
// edge functions, the D3D top-left bias, the pixel-centre bounding box, the
// double-sided winding flip and the zero-area reject — and reads the coverage
// back out of the work surface. There is exactly one fill rule in this
// repository and this is a view onto it, so "RTL == zref::EdgeWalk" is
// literally "RTL == the §8 law".
//
// TILE TRANSLATION. raster_tri's Viewport is unsigned, so a tile at a
// negative or far screen origin cannot be expressed directly. orient(), the
// top-left predicate and the bbox law are all TRANSLATION INVARIANT — they
// use only coordinate differences, and 256·tile is a whole multiple of the
// 256-subpixel pixel pitch, so `(v + 127) >> 8` shifts by exactly `tile`.
// The model therefore rasterises the triangle translated by
// (−256·tile_x, −256·tile_y) into a 16×16 surface with viewport {0,0,16,16}:
// the same 256 pixel centres, in the same order, under the same law. The RTL
// under test is handed the UNtranslated triangle plus the tile origin, so a
// bug in its tile-origin arithmetic still shows up as a mask mismatch.

#pragma once

#include <cstdint>

namespace zref {

/** The 16×16 tile coverage oracle for RASTER.EDGEWALK. */
struct EdgeWalk {
  /** Tile edge in pixels (charter phase 4: "16×16 colour/Z/stencil tile"). */
  static constexpr int kTile = 16;

  /** ±2048 px guard band expressed in S 12.8 subpixels (qformats.md §8). */
  static constexpr int32_t kGuard = 524288;

  /** One triangle, S 12.8 screen subpixels. */
  struct Tri {
    int32_t ax = 0, ay = 0, bx = 0, by = 0, cx = 0, cy = 0;
  };

  /** Coverage of one tile: 16 row masks (bit i = tile column i). */
  struct Cov {
    uint16_t row[kTile] = {};
    uint32_t count = 0;       // covered pixel centres, 0..256
    bool degenerate = false;  // area == 0: the zero-area reject fired

    bool covered(int x, int y) const { return ((row[y] >> x) & 1u) != 0u; }
    bool operator==(const Cov& o) const {
      if (count != o.count || degenerate != o.degenerate) return false;
      for (int i = 0; i < kTile; ++i)
        if (row[i] != o.row[i]) return false;
      return true;
    }
    bool operator!=(const Cov& o) const { return !(*this == o); }
  };

  /**
   * 2A in subpixel² — the s64 setup rast.cpp performs before its
   * `if (area == 0) return;`. Exposed so callers can predict the zero-area
   * verdict without re-deriving the edge functions.
   */
  static int64_t area2(const Tri& t);

  /** Exact §8 coverage of the 16×16 tile whose top-left PIXEL is (tx, ty). */
  static Cov tile(const Tri& t, int32_t tx, int32_t ty);
};

}  // namespace zref
