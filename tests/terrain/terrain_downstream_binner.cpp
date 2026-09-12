// terrain_downstream_binner.cpp — what GEOM.BINNER costs PER TERRAIN TRIANGLE.
//
// GEOM.BINNER.md records 2.83 clocks per emitted TILE REFERENCE, measured on
// one full-canvas triangle over 198 tiles. A terrain triangle is the other
// extreme: a level-0 cell a few pixels across touches ONE tile (sometimes two
// or four at a tile seam), and the block's structural cost is "1 accept + 2
// setup + 1 per rejected candidate tile + 2 per kept tile", with
// `tri_ready_o` low for the whole enumeration. So the per-TRIANGLE cost for
// terrain is dominated by the fixed part, and no number in the tree says
// what it is. This measures it, with the driver the binner's own directed
// test uses (tests/geometry/geom_dev.hpp::BinnerDev), on TRI_CAP = 128
// one-tile triangles -- the most the block bins in one frame.
//
// It is the third block downstream of the arena and it is the one whose rate
// decides whether one triangle per clock can be CONSUMED, which is the
// replica question in reports/TERRAIN-PIPELINE-COMPOSITION-20260910.md.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "verilated.h"

#define ZHAO_GEOM_DEV_BINNER
#include "geom_dev.hpp"
#include "zhao_sim.hpp"

using zhao::check;
using zhao_geom::BinJob;
using zhao_geom::BinStatus;
using zhao_geom::BinTri;
using zhao_geom::BinnerDev;

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  const int grid_w = 24, grid_h = 15;  // 384x240 in 16-px tiles
  const zref::Clip::Viewport vp{0, 0, 384, 240};

  // 128 one-tile triangles: 4-px cells well inside their 16-px tile.
  std::vector<BinTri> tris;
  for (int i = 0; i < 128; ++i) {
    const int tx = i % grid_w, ty = (i / grid_w) % grid_h;
    const int x0 = (tx * 16 + 4) * 256, y0 = (ty * 16 + 4) * 256;
    const int x1 = x0 + 4 * 256, y1 = y0 + 4 * 256;
    BinTri t;
    const bool ok = (i % 2 == 0)
                        ? zhao_geom::make_bin_tri(x0, y0, x1, y1, x1, y0, vp, static_cast<uint16_t>(i), &t)
                        : zhao_geom::make_bin_tri(x0, y0, x0, y1, x1, y1, vp, static_cast<uint16_t>(i), &t);
    check(ok, "binner: the oracle clip accepts a one-tile terrain triangle", 1, ok ? 1 : 0);
    tris.push_back(t);
  }

  BinnerDev dev;
  BinStatus st;
  std::string err;
  const std::vector<BinJob> jobs = dev.frame(tris, grid_w, grid_h, 0, &st, &err);
  if (!err.empty()) std::printf("  binner driver error: %s\n", err.c_str());
  check(err.empty(), "binner: driver ran clean", 1, err.empty() ? 1 : 0);
  check(jobs.size() == tris.size(), "binner: one tile reference per one-tile triangle", tris.size(),
        jobs.size());
  check(!st.overflow, "binner: no overflow at TRI_CAP", 0, st.overflow ? 1 : 0);

  const double per_tri = static_cast<double>(st.bin_cycles) / static_cast<double>(tris.size());
  std::printf("MEASURED GEOM.BINNER on %zu ONE-TILE terrain triangles: %llu bin clocks = %.2f clocks per "
              "triangle (drain %llu clocks); TRI_CAP = 128 triangles per frame\n",
              tris.size(), static_cast<unsigned long long>(st.bin_cycles), per_tri,
              static_cast<unsigned long long>(st.drain_cycles));
  // The structural floor for a one-tile triangle is 1 accept + 2 setup + 2
  // (evaluate + push) = 5; the contract's per-reference figure is 2.83 on a
  // big triangle. Hold the measurement to a wide bracket so it cannot drift
  // silently in either direction.
  check(per_tri >= 3.0, "binner: at least 3 clocks per one-tile triangle (structural floor)", 300,
        static_cast<uint64_t>(per_tri * 100));
  check(per_tri <= 8.0, "binner: at most 8 clocks per one-tile triangle", 800,
        static_cast<uint64_t>(per_tri * 100));

  return zhao::report_and_exit("terrain_downstream_binner");
}
