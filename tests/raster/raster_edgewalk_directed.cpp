// raster_edgewalk_directed.cpp — RASTER.EDGEWALK directed vectors
// (design/contracts/RASTER.EDGEWALK.md "Directed tests"; law
// spec/qformats.md §8; oracle reference/src/zrender/rast.cpp).
//
// Every case runs the Verilated zhao_raster_edgewalk AND the frozen software
// raster over the same triangle/tile and requires the 16 row masks, the
// covered-pixel count and the zero-area verdict to be IDENTICAL. On top of
// that each case asserts its own law:
//
//   1. degenerate      — zero area (coincident, collinear, repeated vertex):
//                        no coverage at all, job_degenerate_o set
//   2. windings        — all 6 vertex permutations of one triangle (both
//                        windings × 3 rotations) give the SAME coverage
//                        (the double-sided flip of rast.cpp)
//   3. outside         — triangles wholly left/right/above/below the tile,
//                        and one a guard band away: zero coverage, and NOT
//                        degenerate (a culled triangle is not a bad one)
//   4. tile borders    — edges exactly on the tile's boundary lines and
//                        exactly through pixel centres
//   5. shared edge     — a tile-sized quad split on its diagonal: every
//                        pixel covered EXACTLY once, no holes, no doubles,
//                        and every centre ON the diagonal claimed by one side
//   6. seam sweep      — the 2026-08-15 defect class: a vertical and a
//                        horizontal seam at every one of the 256 subpixel
//                        fractions, four triangles per fraction, exactly-once
//                        coverage of the whole tile
//   7. guard band      — vertices at the ±2048 px extremes of the S 12.8
//                        guard band, tiles at both ends of the screen
//   8. full tile       — a triangle that swallows the tile: 16 rows of
//                        0xFFFF, count 256
//   9. subpixel        — triangles smaller than one pixel, on and off a
//                        pixel centre
//  10. backpressure    — the same jobs with cov_ready_i gated by a PCG bit
//                        stream: identical masks, beats held stable while
//                        stalled, no beat after cov_last_o
//  11. tile origins    — negative and far-positive tile origins

#include "raster_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using zhao::check;
using zhao_raster::kGuard;
using zhao_raster::kTile;
using zhao_raster::oracle_cover;
using zhao_raster::RtlDev;
using zhao_raster::TileCov;
using zhao_raster::Tri;

namespace {

// One Verilated model for the whole suite. A function-local static, NOT a
// global pointer to main's local — the latter is a dangling-lifetime trap one
// refactor away (cppcheck danglingLifetime, and it is right). Its destructor
// never runs: zhao::report_and_exit finishes with _Exit, which is the
// deliberate house workaround for the Verilator/libwinpthread exit deadlock.
RtlDev& dev() {
  static RtlDev d;
  return d;
}

uint16_t g_src = 0;

// The differential core: RTL vs rast.cpp for one triangle × one tile.
TileCov diff(const Tri& t, int32_t tx, int32_t ty, const char* what, uint32_t stall_seed = 0) {
  std::string err;
  const TileCov got = dev().run(t, tx, ty, ++g_src, stall_seed, &err);
  const TileCov want = oracle_cover(t, tx, ty);
  check(err.empty(), (std::string(what) + ": protocol clean").c_str(), 0, err.empty() ? 0 : 1);
  if (!err.empty()) std::printf("    protocol: %s\n", err.c_str());
  const bool eq = (got == want);
  check(eq, (std::string(what) + ": RTL == rast.cpp").c_str(), want.count, got.count);
  if (!eq) {
    const std::string body = zhao_raster::describe(t, tx, ty, want, got);
    std::printf("    %s\n", body.c_str());
    zhao::save_failing_vector(std::string("raster_edgewalk_") + what,
                              zhao_raster::serialize(t, tx, ty), "oracle", body);
  }
  return got;
}

// subpixel helper: whole pixels -> S 12.8
constexpr int32_t px(int32_t p) { return p << 8; }

// ---- 1. degenerate ---------------------------------------------------------
void test_degenerate() {
  const Tri cases[] = {
      {px(4), px(4), px(4), px(4), px(4), px(4)},                // one point
      {px(2), px(2), px(8), px(8), px(14), px(14)},              // collinear diagonal
      {px(1), px(5), px(15), px(5), px(7), px(5)},               // collinear horizontal
      {px(3), px(0), px(3), px(9), px(3), px(15)},               // collinear vertical
      {px(2), px(3), px(9), px(3), px(2), px(3)},                // repeated vertex
      {px(0) + 1, px(0), px(8) + 1, px(8), px(16) + 1, px(16)},  // collinear, subpixel
  };
  int i = 0;
  for (const Tri& t : cases) {
    char name[48];
    std::snprintf(name, sizeof(name), "degenerate_%d", i++);
    const TileCov c = diff(t, 0, 0, name);
    check(c.degenerate, "degenerate: job_degenerate_o set", 1, c.degenerate ? 1 : 0);
    check(c.count == 0, "degenerate: no coverage", 0, c.count);
  }
}

// ---- 2. all six vertex permutations ---------------------------------------
void test_windings() {
  const Tri base{px(2) + 37, px(1) + 200, px(13) + 91, px(4), px(6), px(14) + 13};
  const Tri perms[6] = {
      {base.ax, base.ay, base.bx, base.by, base.cx, base.cy},
      {base.bx, base.by, base.cx, base.cy, base.ax, base.ay},
      {base.cx, base.cy, base.ax, base.ay, base.bx, base.by},
      {base.ax, base.ay, base.cx, base.cy, base.bx, base.by},  // reversed winding
      {base.cx, base.cy, base.bx, base.by, base.ax, base.ay},
      {base.bx, base.by, base.ax, base.ay, base.cx, base.cy},
  };
  TileCov first;
  for (int i = 0; i < 6; ++i) {
    char name[32];
    std::snprintf(name, sizeof(name), "winding_%d", i);
    const TileCov c = diff(perms[i], 0, 0, name);
    if (i == 0) {
      first = c;
      check(c.count > 0, "winding: the base triangle covers something", 1, c.count > 0 ? 1 : 0);
    } else {
      check(c == first, "winding: every vertex permutation covers identically", first.count,
            c.count);
    }
  }
}

// ---- 3. wholly outside the tile -------------------------------------------
void test_outside() {
  const struct {
    const char* name;
    Tri t;
  } cases[] = {
      {"outside_left", {px(-40), px(2), px(-20), px(3), px(-30), px(12)}},
      {"outside_right", {px(40), px(2), px(60), px(3), px(50), px(12)}},
      {"outside_above", {px(2), px(-40), px(9), px(-30), px(4), px(-20)}},
      {"outside_below", {px(2), px(40), px(9), px(50), px(4), px(60)}},
      {"outside_guard",
       {kGuard - px(3), kGuard - px(3), kGuard, kGuard - px(1), kGuard - px(1), kGuard}},
      {"outside_diag", {px(-30), px(-30), px(-10), px(-28), px(-20), px(-12)}},
  };
  for (const auto& c : cases) {
    const TileCov r = diff(c.t, 0, 0, c.name);
    check(r.count == 0, "outside: zero coverage", 0, r.count);
    check(!r.degenerate, "outside: culled by the edge functions, not by zero area", 0,
          r.degenerate ? 1 : 0);
  }
}

// ---- 4. exactly on the tile borders ---------------------------------------
void test_tile_borders() {
  // Edges exactly on the tile boundary lines (x = 0, x = 16, y = 0, y = 16),
  // i.e. half a pixel from every pixel centre — and edges exactly THROUGH
  // pixel centres (x = 8*256 + 128), where the top-left rule decides.
  diff({px(0), px(0), px(16), px(0), px(16), px(16)}, 0, 0, "border_full_upper");
  diff({px(0), px(0), px(16), px(16), px(0), px(16)}, 0, 0, "border_full_lower");
  diff({px(8) + 128, px(0), px(16), px(0), px(16), px(16)}, 0, 0, "border_centre_x");
  diff({px(0), px(8) + 128, px(16), px(8) + 128, px(16), px(16)}, 0, 0, "border_centre_y");
  diff({px(-1), px(-1), px(17), px(-1), px(17), px(17)}, 0, 0, "border_overhang");
  // one-pixel-wide slivers hugging each border
  diff({px(0), px(0), px(1), px(0), px(1), px(16)}, 0, 0, "border_sliver_left");
  diff({px(15), px(0), px(16), px(0), px(16), px(16)}, 0, 0, "border_sliver_right");
  diff({px(0), px(15), px(16), px(15), px(16), px(16)}, 0, 0, "border_sliver_bottom");
}

// ---- 5. shared edge: exactly once ------------------------------------------
void test_shared_edge() {
  // A quad covering the whole tile, split on its diagonal. Both halves keep
  // the same winding, so neither is flipped. Every pixel centre must be
  // claimed by EXACTLY one half — the D3D top-left rule's whole point.
  const int32_t lo = px(0), hi = px(16);
  const TileCov t1 = diff({lo, lo, hi, lo, hi, hi}, 0, 0, "shared_upper");
  const TileCov t2 = diff({lo, lo, hi, hi, lo, hi}, 0, 0, "shared_lower");

  uint32_t holes = 0, doubles = 0, once = 0, diag = 0;
  for (int y = 0; y < kTile; ++y) {
    for (int x = 0; x < kTile; ++x) {
      const int n = (t1.covered(x, y) ? 1 : 0) + (t2.covered(x, y) ? 1 : 0);
      if (n == 0) ++holes;
      if (n > 1) ++doubles;
      if (n == 1) ++once;
      if (x == y && n >= 1) ++diag;
    }
  }
  check(holes == 0, "shared edge: no holes", 0, holes);
  check(doubles == 0, "shared edge: no double fill", 0, doubles);
  check(once == 256, "shared edge: all 256 pixels covered exactly once", 256, once);
  check(diag == 16, "shared edge: every centre ON the diagonal is claimed once", 16, diag);
}

// ---- 6. subpixel seam sweep (the 2026-08-15 defect class) -----------------
//
// Two quads meeting at a seam whose subpixel fraction sweeps 0..255. With a
// strict `>` the seam column is dropped by both sides; with the bias applied
// to the FLOORED E' the columns whose exact edge value lands in [1,255] are
// dropped by both sides. Either bug shows up here as holes.
void test_seam_sweep(bool vertical) {
  uint32_t worst_holes = 0, worst_doubles = 0;
  int32_t worst_frac = -1;
  for (int32_t frac = 0; frac < 256; ++frac) {
    const int32_t seam = px(8) + frac;
    const int32_t lo = px(-2), hi = px(18);
    Tri quads[4];
    if (vertical) {
      quads[0] = {lo, lo, seam, lo, seam, hi};
      quads[1] = {lo, lo, seam, hi, lo, hi};
      quads[2] = {seam, lo, hi, lo, hi, hi};
      quads[3] = {seam, lo, hi, hi, seam, hi};
    } else {
      quads[0] = {lo, lo, hi, lo, hi, seam};
      quads[1] = {lo, lo, hi, seam, lo, seam};
      quads[2] = {lo, seam, hi, seam, hi, hi};
      quads[3] = {lo, seam, hi, hi, lo, hi};
    }
    int cover[kTile][kTile] = {};
    for (int q = 0; q < 4; ++q) {
      std::string err;
      const TileCov got = dev().run(quads[q], 0, 0, ++g_src, 0, &err);
      const TileCov want = oracle_cover(quads[q], 0, 0);
      if (!err.empty() || got != want) {
        char name[64];
        std::snprintf(name, sizeof(name), "seam_%s_%d_%d", vertical ? "v" : "h", frac, q);
        check(false, name, want.count, got.count);
        std::printf("    %s\n", zhao_raster::describe(quads[q], 0, 0, want, got).c_str());
      }
      for (int y = 0; y < kTile; ++y)
        for (int x = 0; x < kTile; ++x) cover[y][x] += got.covered(x, y) ? 1 : 0;
    }
    uint32_t holes = 0, doubles = 0;
    for (int y = 0; y < kTile; ++y)
      for (int x = 0; x < kTile; ++x) {
        if (cover[y][x] == 0) ++holes;
        if (cover[y][x] > 1) ++doubles;
      }
    if (holes + doubles > worst_holes + worst_doubles) {
      worst_holes = holes;
      worst_doubles = doubles;
      worst_frac = frac;
    }
  }
  const char* what = vertical
                         ? "seam sweep (vertical): no crack at any of 256 subpixel fractions"
                         : "seam sweep (horizontal): no crack at any of 256 subpixel fractions";
  check(worst_holes == 0 && worst_doubles == 0, what, 0, worst_holes + worst_doubles);
  if (worst_holes + worst_doubles != 0)
    std::printf("    worst fraction %d: %u holes, %u doubles\n", worst_frac, worst_holes,
                worst_doubles);
}

// ---- 7. guard band extremes ------------------------------------------------
void test_guard_band() {
  // A triangle whose vertices sit at the ±2048 px guard band corners: the
  // s64 setup must not overflow and the tile-local saturation must keep the
  // sign. Tiles at both ends of the screen see the same huge triangle.
  const Tri big{-kGuard, -kGuard, kGuard - 1, -kGuard, kGuard - 1, kGuard - 1};
  diff(big, 0, 0, "guard_big_origin");
  diff(big, 2032, 2032, "guard_big_far");
  diff(big, -2048, -2048, "guard_big_near");
  diff(big, 100, -2048, "guard_big_top");

  // a thin guard-band sliver crossing the tile: the largest edge deltas the
  // format can produce, with the tile in the middle
  diff({-kGuard, -kGuard, kGuard - 1, kGuard - 1, -kGuard, -kGuard + px(1)}, 0, 0, "guard_sliver");
  diff({-kGuard, kGuard - 1, kGuard - 1, -kGuard, px(8), px(8)}, 0, 0, "guard_cross");
}

// ---- 8. full tile ----------------------------------------------------------
void test_full_tile() {
  const TileCov c = diff({px(-100), px(-100), px(200), px(-100), px(50), px(200)}, 0, 0, "full");
  check(c.count == 256, "full tile: 256 covered pixels", 256, c.count);
  bool all = true;
  for (int y = 0; y < kTile; ++y) all = all && (c.row[y] == 0xFFFFu);
  check(all, "full tile: every row mask is 0xFFFF", 1, all ? 1 : 0);
}

// ---- 9. subpixel triangles -------------------------------------------------
void test_subpixel() {
  // strictly inside one pixel, straddling the centre -> covers that pixel
  diff({px(4) + 100, px(4) + 100, px(4) + 160, px(4) + 100, px(4) + 160, px(4) + 160}, 0, 0,
       "subpixel_on_centre");
  // strictly inside one pixel, missing the centre -> covers nothing
  const TileCov miss = diff({px(4) + 4, px(4) + 4, px(4) + 40, px(4) + 4, px(4) + 40, px(4) + 40},
                            0, 0, "subpixel_off_centre");
  check(miss.count == 0, "subpixel: a triangle that misses every centre covers nothing", 0,
        miss.count);
  // one-subpixel-wide needle through a column of centres
  diff({px(6) + 128, px(0), px(6) + 129, px(0), px(6) + 128, px(16)}, 0, 0, "subpixel_needle");
}

// ---- 10. backpressure ------------------------------------------------------
void test_backpressure() {
  const Tri t{px(1) + 77, px(2), px(14), px(3) + 200, px(5), px(15) + 40};
  std::string err;
  const TileCov free_run = dev().run(t, 0, 0, 0x1234, 0, &err);
  check(err.empty(), "backpressure: free-running protocol clean", 0, err.empty() ? 0 : 1);
  for (uint32_t seed = 1; seed <= 8; ++seed) {
    std::string e2;
    const TileCov stalled = dev().run(t, 0, 0, 0x5678, seed * 2654435761u, &e2);
    check(e2.empty(), "backpressure: stalled protocol clean", 0, e2.empty() ? 0 : 1);
    if (!e2.empty()) std::printf("    protocol: %s\n", e2.c_str());
    check(stalled == free_run, "backpressure: stalls do not change coverage", free_run.count,
          stalled.count);
  }
}

// ---- 11. tile origins ------------------------------------------------------
void test_tile_origins() {
  // The SAME triangle shape at four tile origins, translated to match: the
  // coverage must be identical (orient() is translation invariant, and the
  // walker's tile-origin arithmetic must be too).
  const Tri shape{px(1) + 33, px(2) + 5, px(14) + 90, px(4), px(7), px(15) + 201};
  const struct {
    int32_t tx, ty;
  } origins[] = {{0, 0}, {16, 32}, {-48, -16}, {2032, -2048}, {-2048, 2032}};
  TileCov first;
  int i = 0;
  for (const auto& o : origins) {
    const Tri t{shape.ax + o.tx * 256, shape.ay + o.ty * 256, shape.bx + o.tx * 256,
                shape.by + o.ty * 256, shape.cx + o.tx * 256, shape.cy + o.ty * 256};
    char name[40];
    std::snprintf(name, sizeof(name), "origin_%d", i);
    const TileCov c = diff(t, o.tx, o.ty, name);
    if (i == 0)
      first = c;
    else
      check(c == first, "tile origin: translation invariant", first.count, c.count);
    ++i;
  }
}

}  // namespace

int main() {
  test_degenerate();
  test_windings();
  test_outside();
  test_tile_borders();
  test_shared_edge();
  test_seam_sweep(true);
  test_seam_sweep(false);
  test_guard_band();
  test_full_tile();
  test_subpixel();
  test_backpressure();
  test_tile_origins();

  return zhao::report_and_exit("raster_edgewalk_directed");
}
