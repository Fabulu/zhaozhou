// raster_edgewalk_random.cpp — RASTER.EDGEWALK randomized differential test
// (design/contracts/RASTER.EDGEWALK.md "Randomized differential tests"; law
// spec/qformats.md §8; oracle reference/src/zrender/rast.cpp).
//
// Two lanes, both fully deterministic from one base seed (the PCG shape used
// by the other random lanes in this tree — audio_fifo_random.cpp):
//
//   LANE A — coverage differential. PCG triangles across five populations
//     (tile-local, guard-band-wide, slivers, pixel-centre-aligned, exactly
//     degenerate) at PCG tile origins, half of them with cov_ready_i gated
//     by a second PCG stream. The 16 row masks, the covered-pixel count and
//     the zero-area verdict must equal rast.cpp's EXACTLY.
//
//   LANE B — shared edges, exactly once. A PCG rectangle with independent
//     subpixel corners is split on BOTH diagonals. Every triangle is diffed
//     against the oracle, and then:
//       · neither split double-covers any pixel (the top-left rule owns
//         each shared-edge centre exactly once), and
//       · the two splits cover the IDENTICAL pixel set — the seam is not
//         allowed to move when the diagonal does.
//     A strict `>` or a bias on the floored E' fails this immediately.
//
// Modes: default = 4,000 iterations per lane (CTest fast); --nightly =
// 60,000 (CTest nightly). Every failing vector is saved (charter §29-17).

#include "raster_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

using zhao::check;
using zhao_raster::kGuard;
using zhao_raster::kTile;
using zhao_raster::oracle_cover;
using zhao_raster::RtlDev;
using zhao_raster::TileCov;
using zhao_raster::Tri;

namespace {

// PCG RXS-M-XS — the committed test PRNG shape (qformats.md §7.5 constants).
uint32_t pcg_perm(uint32_t s) {
  const uint32_t w = static_cast<uint32_t>(((s >> ((s >> 28) + 4)) ^ s) * 277803737u);
  return (w >> 22) ^ w;
}

struct Prng {
  uint32_t state;
  explicit Prng(uint32_t seed) : state(seed) {}
  uint32_t draw() {
    state = state * 747796405u + 2891336453u;
    return pcg_perm(state);
  }
  uint32_t lane(uint32_t lo, uint32_t hi) { return lo + (draw() % (hi - lo + 1)); }
  int32_t span(int32_t lo, int32_t hi) {
    return lo + static_cast<int32_t>(draw() % static_cast<uint32_t>(hi - lo + 1));
  }
};

int failures = 0;

// One differential job. Returns the RTL coverage; records at most a handful
// of failing vectors so a broken build does not write a gigabyte of them.
TileCov one(RtlDev& dev, const Tri& t, int32_t tx, int32_t ty, uint32_t stall, const char* lane,
            uint32_t iter) {
  std::string err;
  const TileCov got = dev.run(t, tx, ty, static_cast<uint16_t>(iter), stall, &err);
  const TileCov want = oracle_cover(t, tx, ty);
  if (!err.empty() || got != want) {
    ++failures;
    if (failures <= 8) {
      char name[64];
      std::snprintf(name, sizeof(name), "raster_edgewalk_%s_%u", lane, iter);
      const std::string body = zhao_raster::describe(t, tx, ty, want, got) +
                               (err.empty() ? std::string() : ("\n  protocol: " + err));
      std::printf("FAIL %s\n    %s\n", name, body.c_str());
      zhao::save_failing_vector(name, zhao_raster::serialize(t, tx, ty), "rast.cpp", body);
    }
  }
  return got;
}

// LANE A populations. All coordinates are S 12.8 subpixels inside the ±2048
// px guard band (§8); the tile origin is a signed pixel coordinate.
Tri draw_triangle(Prng& rng, int32_t tx, int32_t ty) {
  const int32_t ox = tx * 256, oy = ty * 256;
  auto clampg = [](int32_t v) { return v < -kGuard ? -kGuard : (v > kGuard - 1 ? kGuard - 1 : v); };
  Tri t;
  switch (rng.lane(0, 4)) {
    case 0: {  // tile-local: vertices within a few pixels of the tile
      t.ax = clampg(ox + rng.span(-6 * 256, 22 * 256));
      t.ay = clampg(oy + rng.span(-6 * 256, 22 * 256));
      t.bx = clampg(ox + rng.span(-6 * 256, 22 * 256));
      t.by = clampg(oy + rng.span(-6 * 256, 22 * 256));
      t.cx = clampg(ox + rng.span(-6 * 256, 22 * 256));
      t.cy = clampg(oy + rng.span(-6 * 256, 22 * 256));
      break;
    }
    case 1: {  // guard-band wide: the biggest edge deltas the format allows
      t.ax = rng.span(-kGuard, kGuard - 1);
      t.ay = rng.span(-kGuard, kGuard - 1);
      t.bx = rng.span(-kGuard, kGuard - 1);
      t.by = rng.span(-kGuard, kGuard - 1);
      t.cx = rng.span(-kGuard, kGuard - 1);
      t.cy = rng.span(-kGuard, kGuard - 1);
      break;
    }
    case 2: {  // sliver: two vertices within a pixel of each other
      t.ax = clampg(ox + rng.span(0, 16 * 256));
      t.ay = clampg(oy + rng.span(0, 16 * 256));
      t.bx = clampg(t.ax + rng.span(-255, 255));
      t.by = clampg(t.ay + rng.span(-255, 255));
      t.cx = clampg(ox + rng.span(-4 * 256, 20 * 256));
      t.cy = clampg(oy + rng.span(-4 * 256, 20 * 256));
      break;
    }
    case 3: {  // pixel-centre aligned: exact-zero edge values are common
      auto ctr = [&](int32_t o) {
        return o + (rng.span(-4, 20) << 8) + (rng.lane(0, 1) ? 128 : 0);
      };
      t.ax = clampg(ctr(ox));
      t.ay = clampg(ctr(oy));
      t.bx = clampg(ctr(ox));
      t.by = clampg(ctr(oy));
      t.cx = clampg(ctr(ox));
      t.cy = clampg(ctr(oy));
      break;
    }
    default: {  // exactly degenerate: C on the line AB (collinear by
                // construction with an integer multiplier, so area == 0)
      t.ax = clampg(ox + rng.span(-4 * 256, 20 * 256));
      t.ay = clampg(oy + rng.span(-4 * 256, 20 * 256));
      const int32_t dx = rng.span(-512, 512), dy = rng.span(-512, 512);
      const int32_t k1 = rng.span(-4, 4), k2 = rng.span(-4, 4);
      t.bx = clampg(t.ax + k1 * dx);
      t.by = clampg(t.ay + k1 * dy);
      t.cx = clampg(t.ax + k2 * dx);
      t.cy = clampg(t.ay + k2 * dy);
      break;
    }
  }
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  const bool nightly = (argc > 1 && std::strcmp(argv[1], "--nightly") == 0);
  const uint32_t iters = nightly ? 60000u : 4000u;

  RtlDev dev;

  // ---- LANE A: coverage differential --------------------------------------
  {
    Prng rng(0x5EEDA57Au);
    uint32_t degenerate = 0, empty = 0, full = 0, partial = 0;
    for (uint32_t i = 0; i < iters; ++i) {
      const int32_t tx = rng.span(-2048, 2032);
      const int32_t ty = rng.span(-2048, 2032);
      const Tri t = draw_triangle(rng, tx, ty);
      const uint32_t stall = (rng.lane(0, 1) != 0) ? (rng.draw() | 1u) : 0u;
      const TileCov c = one(dev, t, tx, ty, stall, "laneA", i);
      if (c.degenerate)
        ++degenerate;
      else if (c.count == 0)
        ++empty;
      else if (c.count == 256)
        ++full;
      else
        ++partial;
    }
    std::printf(
        "raster_edgewalk_random lane A: %u tiles (%u degenerate, %u empty, %u full, "
        "%u partial)\n",
        iters, degenerate, empty, full, partial);
    check(degenerate > 0, "lane A: the zero-area population was exercised", 1, degenerate > 0);
    check(partial > iters / 20, "lane A: most draws produce partial coverage", 1,
          partial > iters / 20);
  }

  // ---- LANE B: shared edges, exactly once ----------------------------------
  {
    Prng rng(0x5EEDB00Bu);
    uint32_t doubles = 0, split_diff = 0, shared_pixels = 0;
    for (uint32_t i = 0; i < iters; ++i) {
      const int32_t tx = rng.span(-2048, 2032);
      const int32_t ty = rng.span(-2048, 2032);
      const int32_t ox = tx * 256, oy = ty * 256;
      // an axis-aligned rectangle with independent SUBPIXEL corners — always
      // convex, so both diagonals are legal splits
      int32_t x0 = ox + rng.span(-2 * 256, 14 * 256);
      int32_t x1 = ox + rng.span(2 * 256, 18 * 256);
      int32_t y0 = oy + rng.span(-2 * 256, 14 * 256);
      int32_t y1 = oy + rng.span(2 * 256, 18 * 256);
      if (x1 <= x0) x1 = x0 + 1;
      if (y1 <= y0) y1 = y0 + 1;

      // corners 0..3 clockwise in the y-down canvas
      const int32_t qx[4] = {x0, x1, x1, x0};
      const int32_t qy[4] = {y0, y0, y1, y1};
      auto tri = [&](int a, int b, int c) { return Tri{qx[a], qy[a], qx[b], qy[b], qx[c], qy[c]}; };

      const TileCov s1a = one(dev, tri(0, 1, 2), tx, ty, 0, "laneB", i);
      const TileCov s1b = one(dev, tri(0, 2, 3), tx, ty, 0, "laneB", i);
      const TileCov s2a = one(dev, tri(0, 1, 3), tx, ty, 0, "laneB", i);
      const TileCov s2b = one(dev, tri(1, 2, 3), tx, ty, 0, "laneB", i);

      for (int y = 0; y < kTile; ++y) {
        const uint16_t u1 = static_cast<uint16_t>(s1a.row[y] | s1b.row[y]);
        const uint16_t u2 = static_cast<uint16_t>(s2a.row[y] | s2b.row[y]);
        if ((s1a.row[y] & s1b.row[y]) != 0) ++doubles;
        if ((s2a.row[y] & s2b.row[y]) != 0) ++doubles;
        if (u1 != u2) ++split_diff;
        for (int x = 0; x < kTile; ++x) shared_pixels += (u1 >> x) & 1u;
      }
    }
    std::printf("raster_edgewalk_random lane B: %u rectangles, %u covered pixels\n", iters,
                shared_pixels);
    check(doubles == 0, "lane B: no shared-edge pixel is covered twice", 0, doubles);
    check(split_diff == 0, "lane B: both diagonal splits cover the identical pixel set", 0,
          split_diff);
    check(shared_pixels > 0, "lane B: the rectangles actually covered pixels", 1,
          shared_pixels > 0);
  }

  check(failures == 0, "randomized differential: RTL == rast.cpp on every tile", 0, failures);
  return zhao::report_and_exit("raster_edgewalk_random");
}
