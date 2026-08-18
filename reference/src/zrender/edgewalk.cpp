// edgewalk.cpp — zref::EdgeWalk, the RASTER.EDGEWALK tile-coverage oracle.
//
// Law: spec/qformats.md §8, reached by CALLING the frozen raster
// (zref::render::raster_tri) rather than restating it — see the header
// (reference/include/zref/zref_edgewalk.hpp) for why, and for the
// translation-invariance argument behind the tile origin handling.

#include "zref/zref_edgewalk.hpp"

#include "internal.hpp"

namespace zref {

int64_t EdgeWalk::area2(const Tri& t) {
  return (static_cast<int64_t>(t.bx) - t.ax) * (static_cast<int64_t>(t.cy) - t.ay) -
         (static_cast<int64_t>(t.by) - t.ay) * (static_cast<int64_t>(t.cx) - t.ax);
}

EdgeWalk::Cov EdgeWalk::tile(const Tri& t, int32_t tx, int32_t ty) {
  using namespace zref::render;

  Cov out;
  out.degenerate = (area2(t) == 0);

  const int32_t ox = tx * 256;
  const int32_t oy = ty * 256;
  auto sv = [&](int32_t x, int32_t y) {
    ScreenV v;
    v.x = x - ox;
    v.y = y - oy;
    return v;
  };

  // Flat white on black with depth off: the surface becomes a pure coverage
  // bitmap, and nothing in the depth/attribute path can perturb the verdict.
  WorkSurface s;
  s.reset(kTile, kTile, zref::sky::SkyColor{0, 0, 0});
  const Viewport vp{0, 0, kTile, kTile};
  TriMode m;
  m.depth_test = false;
  m.depth_write = false;
  m.use_fixed_depth = true;
  m.fixed_depth = 1;
  raster_tri(s, vp, sv(t.ax, t.ay), sv(t.bx, t.by), sv(t.cx, t.cy), 255, 255, 255, m);

  for (int y = 0; y < kTile; ++y) {
    for (int x = 0; x < kTile; ++x) {
      const size_t i = static_cast<size_t>(y) * kTile + x;
      if (s.rgb[i * 3] != 0) {
        out.row[y] = static_cast<uint16_t>(out.row[y] | (1u << x));
        ++out.count;
      }
    }
  }
  return out;
}

}  // namespace zref
