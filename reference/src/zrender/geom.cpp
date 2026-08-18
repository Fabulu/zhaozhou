// geom.cpp — zref::Clip, zref::Setup, zref::Binner: the GEOM.CLIP /
// GEOM.SETUP / GEOM.BINNER oracles (phase 5, ZH-056/057/058).
//
// Law, reached by CALLING it rather than restating it (see the header):
//   · zref::EdgeWalk::area2 IS rast.cpp's orient() — the s64 §8 setup.
//   · zref::render::scan_bbox IS raster_tri's own scissored pixel-centre
//     bounding box, extracted from it for this increment.
//   · zref::fill_accept IS zhao_raster_fill.sv's expression.
// The near-plane law is likewise found, not invented: rast.cpp's
// project_vertex returns `in = false` for `clip.w.raw <= 0`, and every caller
// (terrain.cpp, sprites.cpp, render_frame.cpp) drops the WHOLE primitive —
// "whole-primitive near-plane rejection", spec/sky_and_beams.md §1.2
// projection corollary, restated at internal.hpp's ProjOut.

#include "zref/zref_geom.hpp"

#include "zref/zref_edgewalk.hpp"

#include "internal.hpp"

namespace zref {

Clip::Out Clip::clip(const In& t, const Viewport& vp, CullMode cull) {
  Out o;

  // 1. NEAR PLANE — whole-primitive rejection. Not a clip: no vertex is moved
  //    and no new triangle is produced. This is the documented Phase-3 model
  //    and it is what the guard band is FOR — a triangle that survives the
  //    near plane has already been clamped into +/-2048 px by to_screen_xy.
  if ((t.behind & 7u) != 0u) {
    o.verdict = kNearPlane;
    return o;
  }

  // 2. ZERO AREA — rast.cpp `if (area == 0) return;`, on the exact s64 2A.
  const EdgeWalk::Tri tri{t.ax, t.ay, t.bx, t.by, t.cx, t.cy};
  const int64_t area = EdgeWalk::area2(tri);
  if (area == 0) {
    o.verdict = kZeroArea;
    return o;
  }

  // 3. BACKFACE — the CHOSEN mode. kCullNone is rast.cpp exactly.
  if ((cull == kCullNegative && area < 0) || (cull == kCullPositive && area > 0)) {
    o.verdict = kBackface;
    return o;
  }

  // 4. WINDING — rast.cpp's double-sided flip, applied ONCE, here. After it
  //    2A > 0, so RASTER.EDGEWALK's own flip is a no-op on this triangle and
  //    the two agree by construction.
  o.ax = t.ax;
  o.ay = t.ay;
  if (area < 0) {
    o.bx = t.cx;
    o.by = t.cy;
    o.cx = t.bx;
    o.cy = t.by;
    o.area2 = -area;
  } else {
    o.bx = t.bx;
    o.by = t.by;
    o.cx = t.cx;
    o.cy = t.cy;
    o.area2 = area;
  }

  // 5. SCISSOR — raster_tri's own early return, by calling its own function.
  render::ScreenV a;
  render::ScreenV b;
  render::ScreenV c;
  a.x = o.ax;
  a.y = o.ay;
  b.x = o.bx;
  b.y = o.by;
  c.x = o.cx;
  c.y = o.cy;
  const render::Viewport rvp{static_cast<uint32_t>(vp.x0), static_cast<uint32_t>(vp.y0),
                             static_cast<uint32_t>(vp.w), static_cast<uint32_t>(vp.h)};
  const render::ScanBox bb = render::scan_bbox(a, b, c, rvp);
  if (bb.empty) {
    Out rej;
    rej.verdict = kOffscreen;
    return rej;
  }
  o.min_x = bb.min_x;
  o.max_x = bb.max_x;
  o.min_y = bb.min_y;
  o.max_y = bb.max_y;
  o.verdict = kAccept;
  return o;
}

Setup::Out Setup::setup(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy,
                        int64_t area2) {
  // orient(a,b,px,py) = (b.x-a.x)(py-a.y) - (b.y-a.y)(px-a.x)
  //                   = -(b.y-a.y)*px + (b.x-a.x)*py + (a.x*b.y - a.y*b.x)
  const int32_t vx[3] = {bx, cx, ax};  // edge i starts at vertex a_i
  const int32_t vy[3] = {by, cy, ay};
  const int32_t wx[3] = {cx, ax, bx};  // and ends at b_i
  const int32_t wy[3] = {cy, ay, by};

  Out o;
  o.area2 = area2;
  for (int i = 0; i < 3; ++i) {
    o.e[i].kx = -(wy[i] - vy[i]);
    o.e[i].ky = (wx[i] - vx[i]);
    o.e[i].kc = static_cast<int64_t>(vx[i]) * wy[i] - static_cast<int64_t>(vy[i]) * wx[i];
    // §8 top-left, evaluated on the winding-normalised triangle exactly as
    // RASTER.EDGEWALK evaluates it after its own flip.
    o.e[i].tl = (vy[i] == wy[i]) ? (vx[i] < wx[i]) : (vy[i] < wy[i]);
  }
  return o;
}

std::vector<Binner::Ref> Binner::bin(const Setup::Out& s, int32_t min_x, int32_t max_x,
                                     int32_t min_y, int32_t max_y) {
  std::vector<Ref> out;
  if (min_x > max_x || min_y > max_y) return out;

  int64_t base[3];
  bool nz[3];
  for (int i = 0; i < 3; ++i) {
    base[i] = ep_base(s.e[i]);
    nz[i] = rnz(s.e[i]);
  }

  const int32_t tx0 = min_x >> kTileLog2, tx1 = max_x >> kTileLog2;
  const int32_t ty0 = min_y >> kTileLog2, ty1 = max_y >> kTileLog2;

  for (int32_t ty = ty0; ty <= ty1; ++ty) {
    for (int32_t tx = tx0; tx <= tx1; ++tx) {
      const int64_t px = static_cast<int64_t>(tx) << kTileLog2;
      const int64_t py = static_cast<int64_t>(ty) << kTileLog2;
      bool keep = true;
      for (int i = 0; i < 3; ++i) {
        // E' at the tile's top-left pixel centre, then moved to the corner
        // that MAXIMISES it (E' is affine, so its max over the 16x16 block of
        // centres is at a corner). If the max still fails the §8 fill test,
        // no centre in the tile can pass it.
        int64_t ep = base[i] + static_cast<int64_t>(s.e[i].kx) * px +
                     static_cast<int64_t>(s.e[i].ky) * py;
        if (s.e[i].kx > 0) ep += static_cast<int64_t>(s.e[i].kx) * (kTile - 1);
        if (s.e[i].ky > 0) ep += static_cast<int64_t>(s.e[i].ky) * (kTile - 1);
        if (!fill_accept(ep, nz[i], s.e[i].tl)) {
          keep = false;
          break;
        }
      }
      if (keep) out.push_back(Ref{tx, ty});
    }
  }
  return out;
}

}  // namespace zref
