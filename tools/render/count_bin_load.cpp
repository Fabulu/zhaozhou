// count_bin_load.cpp — how much GEOM.BINNER a real frame actually asks for.
//
// reports/BINNER_CAPACITY_FOR_8KM_MAPS.md estimated that the shipped arena
// (TRI_CAP = 128 triangles, CHUNKS x CHUNK_REFS = 1,024 references) is about two
// orders of magnitude short for an 8 km map with hundreds of textured creatures.
// That was arithmetic on paper. This is the measurement, and it needs no RTL:
// GEOM.BINNER's binning law IS `zref::Binner`, so counting (triangle, tile)
// pairs through the shipped oracle counts exactly what the hardware would store.
//
// It answers three questions the capacity decision rests on:
//
//   1. how many triangles a representative frame submits;
//   2. how many REFERENCES they generate, which is the arena's real currency
//      and is always larger;
//   3. how deep the worst single tile's list gets, which is what a chunked
//      list has to walk.
//
// WHAT IT DOES NOT DO: invent a scene. There is no camera, no visibility and no
// LOD here, so the numbers are an UPPER bound for the geometry described and a
// LOWER bound for a real frame, which also has sky, stars, effects and objects.
// Both directions are stated at each figure rather than averaged into a single
// misleading one.
//
//   g++ -std=c++17 -O2 -I reference/include tools/render/count_bin_load.cpp \
//       -o count_bin_load -L build/reference -lzhao_zref

#include <cstdint>
#include <cstdio>
#include <vector>

#include "zref/zref_geom.hpp"

namespace {

constexpr int kTile = 16;

// Screen coordinates carry EIGHT fractional bits everywhere in this chain
// (zhao_raster::px is p * 256). Getting this wrong silently shrinks every
// triangle by 16x, which is a mistake this project has already made once.
int32_t px(double p) { return static_cast<int32_t>(p * 256.0); }

struct Load {
  long triangles = 0;  // survived CLIP
  long submitted = 0;  // offered
  long refs = 0;       // (triangle, tile) pairs
  int max_list = 0;    // deepest single tile list
  int tiles_touched = 0;
};

/**
 * Push one triangle through the shipped CLIP -> SETUP -> BINNER chain and
 * accumulate what the arena would have had to hold.
 */
void add_tri(Load* L, std::vector<int>* per_tile, int grid_w, const zref::Clip::Viewport& vp,
             int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t cx, int32_t cy) {
  ++L->submitted;
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
  for (const zref::Binner::Ref& r : refs) {
    const size_t k =
        static_cast<size_t>(r.ty) * static_cast<size_t>(grid_w) + static_cast<size_t>(r.tx);
    if (k < per_tile->size()) ++(*per_tile)[k];
  }
}

void report(const char* what, const Load& L, int tri_cap, int ref_cap) {
  const double tri_over = static_cast<double>(L.triangles) / tri_cap;
  const double ref_over = static_cast<double>(L.refs) / ref_cap;
  std::printf("%-34s tris %7ld (%6.1fx cap)  refs %8ld (%6.1fx cap)  deepest tile %4d\n", what,
              L.triangles, tri_over, L.refs, ref_over, L.max_list);
}

}  // namespace

int main() {
  // Z60: 384x240 at 16x16 tiles.
  constexpr int kW = 384, kH = 240;
  constexpr int kGridW = kW / kTile;  // 24
  constexpr int kGridH = kH / kTile;  // 15
  zref::Clip::Viewport vp;
  vp.w = kW;
  vp.h = kH;

  // The shipped arena, from zhao_geom_binner's parameters.
  constexpr int kTriCap = 128;
  constexpr int kRefCap = 256 * 4;  // CHUNKS * CHUNK_REFS

  std::printf("GEOM.BINNER load against the shipped arena: TRI_CAP=%d, references=%d\n", kTriCap,
              kRefCap);
  std::printf("Z60 %dx%d, %dx%d tiles of %d\n\n", kW, kH, kGridW, kGridH, kTile);

  // --------------------------------------------------------------- 1 ---
  // A FULL-CANVAS BACKDROP. Two triangles, and the reason references are the
  // arena's real currency rather than triangles: it touches every tile.
  {
    Load L;
    std::vector<int> per(static_cast<size_t>(kGridW) * kGridH, 0);
    add_tri(&L, &per, kGridW, vp, px(0), px(0), px(kW), px(0), px(0), px(kH));
    add_tri(&L, &per, kGridW, vp, px(kW), px(0), px(kW), px(kH), px(0), px(kH));
    for (int v : per) {
      if (v > L.max_list) L.max_list = v;
      if (v) ++L.tiles_touched;
    }
    report("sky backdrop (2 triangles)", L, kTriCap, kRefCap);
  }

  // --------------------------------------------------------------- 2 ---
  // ONE TERRAIN PATCH, tessellated as the spec describes: 32x32 cells over a
  // 33x33 lattice, two triangles a cell. Drawn filling the screen, which is
  // what a patch under the camera does.
  {
    Load L;
    std::vector<int> per(static_cast<size_t>(kGridW) * kGridH, 0);
    constexpr int kCells = 32;
    const double sx = static_cast<double>(kW) / kCells;
    const double sy = static_cast<double>(kH) / kCells;
    for (int cy = 0; cy < kCells; ++cy)
      for (int cx = 0; cx < kCells; ++cx) {
        const double x0 = cx * sx, x1 = (cx + 1) * sx;
        const double y0 = cy * sy, y1 = (cy + 1) * sy;
        add_tri(&L, &per, kGridW, vp, px(x0), px(y0), px(x1), px(y0), px(x0), px(y1));
        add_tri(&L, &per, kGridW, vp, px(x1), px(y0), px(x1), px(y1), px(x0), px(y1));
      }
    for (int v : per) {
      if (v > L.max_list) L.max_list = v;
      if (v) ++L.tiles_touched;
    }
    report("one terrain patch, 32x32 cells", L, kTriCap, kRefCap);
  }

  // --------------------------------------------------------------- 3 ---
  // A CREATURE ARMY. Charter 15 caps a meshlet at 96-126 triangles; this draws
  // 200 creatures at 96, each a small cluster on screen. Deliberately small on
  // screen -- an army is many SMALL things, which is the case that stresses
  // triangle count rather than reference count.
  {
    Load L;
    std::vector<int> per(static_cast<size_t>(kGridW) * kGridH, 0);
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
        add_tri(&L, &per, kGridW, vp, px(ox + jx), px(oy + jy), px(ox + jx + 3), px(oy + jy),
                px(ox + jx), px(oy + jy + 3));
      }
    }
    for (int v : per) {
      if (v > L.max_list) L.max_list = v;
      if (v) ++L.tiles_touched;
    }
    report("creature army, 200 x 96 triangles", L, kTriCap, kRefCap);
  }

  // --------------------------------------------------------------- 4 ---
  // A GIANT FILLING THE VIEW. Few triangles, each enormous: the other extreme,
  // where references explode and triangle count does not.
  {
    Load L;
    std::vector<int> per(static_cast<size_t>(kGridW) * kGridH, 0);
    constexpr int kTris = 126;
    for (int t = 0; t < kTris; ++t) {
      const double f = static_cast<double>(t) / kTris;
      add_tri(&L, &per, kGridW, vp, px(kW * f * 0.5), px(0), px(kW), px(kH * (0.3 + 0.7 * f)),
              px(0), px(kH));
    }
    for (int v : per) {
      if (v > L.max_list) L.max_list = v;
      if (v) ++L.tiles_touched;
    }
    report("giant near camera, 126 big triangles", L, kTriCap, kRefCap);
  }

  std::printf(
      "\nREADING THESE. There is no camera, visibility or LOD here, so each figure is an\n"
      "UPPER bound for the geometry described and a LOWER bound for a real frame, which\n"
      "also carries sky, stars, effects and objects at the same time.\n");
  return 0;
}
