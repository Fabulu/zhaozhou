// count_fragment_load.cpp — how many FRAGMENTS a frame covers, and how much of
// that early-Z could ever remove.
//
// reports/PER_PIXEL_BUDGET.md measured what every per-pixel unit costs and then
// stopped at the one number it could not measure: `s`, the fraction of covered
// fragments that survive early-Z. Every figure in that report's demand table
// swings by a factor of four across the plausible range of `s`, so it is the
// most valuable unknown in the renderer.
//
// THIS TOOL DOES NOT MEASURE `s`, AND WILL NOT PRETEND TO. `s` depends on the
// depth of each triangle and on submission order, and the scenes in
// count_bin_load are screen-space geometry with no depth at all. Assigning them
// depths would be inventing a scene and then measuring the invention -- the
// exact failure this project has a written law about.
//
// What IS measurable, exactly and with no invention, is OVERDRAW: how many
// covered fragments the geometry produces against how many DISTINCT pixels it
// touches. That bounds `s` on both sides without assuming anything:
//
//     s = 1                       drawn back-to-front: early-Z saves nothing
//     s = distinct / covered      drawn perfectly front-to-back: only the
//                                 front-most fragment of each pixel survives
//
// A real frame is somewhere between, and no ordering can do better than the
// lower bound or worse than the upper. So the divide demand is bounded rather
// than guessed, and the width of that bound IS the value of getting the draw
// order right.
//
// THE SCENES ARE COUNT_BIN_LOAD'S, VERBATIM, and that matters more than it
// sounds: my first version of the giant was a different construction and
// reported 5,788 references where count_bin_load reports 25,704. Two tools that
// claim to describe the same four frames while disagreeing 4x on one of them
// are worse than one tool. They now agree exactly, which is itself a check --
// if a future edit breaks that, the reference counts stop matching.
//
// It uses the shipped oracles throughout -- zref::Clip, zref::Setup,
// zref::Binner for the tile references and zref::EdgeWalk for the exact §8
// coverage of each one -- so the fragment count is the hardware's, not a model
// of it.
//
//   g++ -std=c++17 -O2 -I reference/include tools/render/count_fragment_load.cpp \
//       build/reference/libzhao_zref.a -o count_fragment_load

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zref/zref_edgewalk.hpp"
#include "zref/zref_geom.hpp"

namespace {

constexpr int kTile = 16;
constexpr int kW = 384, kH = 240;
constexpr int kGridW = kW / kTile;  // 24
constexpr int64_t kClocksPerFrame = 1666667;

/** Screen coordinates carry EIGHT fractional bits. */
int32_t px(double p) { return static_cast<int32_t>(p * 256.0); }

struct Load {
  long triangles = 0;
  long refs = 0;
  long covered = 0;               // fragments, counting every overlap
  std::vector<uint8_t> touched;   // one byte a pixel: was it ever covered
  long distinct = 0;

  Load() : touched(static_cast<size_t>(kW) * kH, 0) {}
};

void add_tri(Load* L, const zref::Clip::Viewport& vp, int32_t ax, int32_t ay, int32_t bx,
             int32_t by, int32_t cx, int32_t cy) {
  zref::Clip::In in;
  in.ax = ax;
  in.ay = ay;
  in.bx = bx;
  in.by = by;
  in.cx = cx;
  in.cy = cy;
  const zref::Clip::Out c = zref::Clip::clip(in, vp, zref::Clip::kCullNone);
  if (c.verdict != zref::Clip::kAccept) return;
  ++L->triangles;

  const zref::Setup::Out s = zref::Setup::setup(c.ax, c.ay, c.bx, c.by, c.cx, c.cy, c.area2);
  const std::vector<zref::Binner::Ref> refs =
      zref::Binner::bin(s, c.min_x, c.max_x, c.min_y, c.max_y);
  L->refs += static_cast<long>(refs.size());

  zref::EdgeWalk::Tri t;
  t.ax = c.ax;
  t.ay = c.ay;
  t.bx = c.bx;
  t.by = c.by;
  t.cx = c.cx;
  t.cy = c.cy;

  for (const zref::Binner::Ref& r : refs) {
    const int tx = r.tx * kTile, ty = r.ty * kTile;
    const zref::EdgeWalk::Cov cov = zref::EdgeWalk::tile(t, tx, ty);
    if (cov.degenerate) continue;
    for (int row = 0; row < kTile; ++row) {
      uint16_t m = cov.row[row];
      for (int col = 0; m; ++col, m >>= 1) {
        if (!(m & 1)) continue;
        const int x = tx + col, y = ty + row;
        if (x < 0 || x >= kW || y < 0 || y >= kH) continue;
        ++L->covered;
        uint8_t& seen = L->touched[static_cast<size_t>(y) * kW + x];
        if (!seen) {
          seen = 1;
          ++L->distinct;
        }
      }
    }
  }
}

void report(const char* what, const Load& L) {
  const double overdraw =
      L.distinct ? static_cast<double>(L.covered) / static_cast<double>(L.distinct) : 0.0;
  const double s_lo = L.covered ? static_cast<double>(L.distinct) / static_cast<double>(L.covered)
                                : 1.0;
  std::printf("%-34s tris %7ld  refs %8ld  fragments %9ld  distinct %7ld  overdraw %5.2fx\n", what,
              L.triangles, L.refs, L.covered, L.distinct, overdraw);
  std::printf("%-34s   s in [%.3f, 1.000]  ->  early-Z can remove at most %.1f%% of the\n", "",
              s_lo, 100.0 * (1.0 - s_lo));
  std::printf("%-34s   per-survivor work, and nothing at all if drawn back to front\n", "");

  // What that means for the divide, which is the unit the budget report found
  // shortest. Every covered fragment pays one divide for invw24; a surviving
  // textured fragment pays two more, and four more again if Gouraud-lit.
  const long lo_tex = L.covered + 2 * static_cast<long>(s_lo * L.covered);
  const long hi_tex = L.covered + 2 * L.covered;
  const long lo_gou = L.covered + 6 * static_cast<long>(s_lo * L.covered);
  const long hi_gou = L.covered + 6 * L.covered;
  std::printf("%-34s   divides: textured %ld..%ld, +Gouraud %ld..%ld\n", "", lo_tex, hi_tex, lo_gou,
              hi_gou);
  // Against the measured service: radix 4 at UNITS = 8 delivers 658,978.
  std::printf("%-34s   radix-4 UNITS=8 delivers 658978 -> %s at best, %s at worst\n", "",
              lo_tex <= 658978 ? "FITS" : "SHORT", hi_gou <= 658978 ? "FITS" : "SHORT");
  std::printf("\n");
}

}  // namespace

int main() {
  zref::Clip::Viewport vp;
  vp.w = kW;
  vp.h = kH;

  std::printf("Fragment load and overdraw, Z60 %dx%d = %d pixels, tiles of %d\n", kW, kH, kW * kH,
              kTile);
  std::printf("Frame budget %lld clocks. Every figure below is the shipped oracle's.\n\n",
              (long long)kClocksPerFrame);

  // The same four scenes as count_bin_load, so the two tools describe the same
  // frames and their numbers can be read together.

  // 1. A full-canvas backdrop: two triangles, every tile, no overlap.
  {
    Load L;
    add_tri(&L, vp, px(0), px(0), px(kW), px(0), px(0), px(kH));
    add_tri(&L, vp, px(kW), px(0), px(kW), px(kH), px(0), px(kH));
    report("sky backdrop (2 triangles)", L);
  }

  // 2. One terrain patch, 32x32 cells over a 33x33 lattice, filling the screen.
  {
    Load L;
    constexpr int kCells = 32;
    const double sx = static_cast<double>(kW) / kCells;
    const double sy = static_cast<double>(kH) / kCells;
    for (int cy = 0; cy < kCells; ++cy)
      for (int cx = 0; cx < kCells; ++cx) {
        const double x0 = cx * sx, x1 = (cx + 1) * sx;
        const double y0 = cy * sy, y1 = (cy + 1) * sy;
        add_tri(&L, vp, px(x0), px(y0), px(x1), px(y0), px(x0), px(y1));
        add_tri(&L, vp, px(x1), px(y0), px(x1), px(y1), px(x0), px(y1));
      }
    report("one terrain patch, 32x32 cells", L);
  }

  // 3. A creature army: 200 creatures of 96 small triangles, scattered.
  {
    Load L;
    constexpr int kCreatures = 200, kTrisEach = 96;
    uint32_t s = 0x1234567u;
    auto rnd = [&s]() {
      s = s * 1664525u + 1013904223u;
      return static_cast<double>((s >> 16) & 0x7FFF) / 32768.0;
    };
    for (int n = 0; n < kCreatures; ++n) {
      const double ox = rnd() * (kW - 24), oy = rnd() * (kH - 24);
      for (int t = 0; t < kTrisEach; ++t) {
        const double jx = rnd() * 20.0, jy = rnd() * 20.0;
        add_tri(&L, vp, px(ox + jx), px(oy + jy), px(ox + jx + 3), px(oy + jy), px(ox + jx),
                px(oy + jy + 3));
      }
    }
    report("creature army, 200 x 96 triangles", L);
  }

  // 4. A giant filling the view: few triangles, each enormous. Copied VERBATIM
  //    from count_bin_load so the two tools describe the same frame -- my first
  //    version of this scene was a different one (126 nested triangles sharing a
  //    vertex, and a randomised angle computed and then never used), which gave
  //    5,788 references where count_bin_load reports 25,704. Two tools claiming
  //    to measure "the same four scenes" while disagreeing by 4x on one of them
  //    is worse than having only one tool.
  {
    Load L;
    constexpr int kTris = 126;
    for (int t = 0; t < kTris; ++t) {
      const double f = static_cast<double>(t) / kTris;
      add_tri(&L, vp, px(kW * f * 0.5), px(0), px(kW), px(kH * (0.3 + 0.7 * f)), px(0), px(kH));
    }
    report("giant near camera, 126 big triangles", L);
  }

  std::printf(
      "READING THESE. Overdraw is exact for the geometry described; `s` is bounded,\n"
      "not measured, because these scenes carry no depth. The width of each bound is\n"
      "what a correct front-to-back draw order is worth, and the WORST column is what\n"
      "the hardware must survive if the order is ever wrong.\n"
      "\n"
      "AND THE GIANT'S 66x IS NOT A GIANT. That scene is 126 triangles each spanning\n"
      "most of the canvas, stacked -- it was built to stress the BINNER, where it is a\n"
      "fair worst case, and it is a poor model of a creature. A closed surface drawn\n"
      "with backface culling has overdraw near 1; without culling, near 2. Read the\n"
      "giant row as 'what pathological geometry costs', never as 'what a giant costs'.\n"
      "The army at 2.2x and the two full-screen passes at exactly 1.0x are the rows\n"
      "that describe plausible frames.\n"
      "\n"
      "ONE COINCIDENCE WORTH CHECKING RATHER THAN BELIEVING. A full-screen textured\n"
      "pass needs 92,160 x 3 = 276,480 divides, which is exactly the figure ruling 7\n"
      "calls the 'terrain-primary estimate'. That may mean the two are the same\n"
      "quantity under different names, or it may be a numeric accident. Nothing here\n"
      "resolves it, and the budget report should not treat them as interchangeable\n"
      "until someone says which.\n");
  return 0;
}
