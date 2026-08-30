// raster_edgewalk_setupcost_directed.cpp — what does ruling 4 actually buy?
//
// ---------------------------------------------------------------------------
// THE CLAIM BEING PRICED
// ---------------------------------------------------------------------------
// reports/RENDERER_ARCHITECTURE.md ruling 4: "RASTER.EDGEWALK recomputes edge
// setup for every tile reference. A triangle touching 30 tiles pays setup 30
// times for coefficients that are tile-independent." The proposed fix is a
// TriangleContext: GEOM.SETUP writes the coefficients once, the binner stores a
// context id, and the drain fetches them instead of re-deriving them.
//
// That is a real cost and the fix is sound. What nobody has measured is HOW BIG
// -- and the fix is not free: the binner's per-triangle record would have to
// carry three edge equations (3 x (23 + 23 + 48) bits) plus the top-left flags,
// roughly TRIPLING it.
//
// I ESTIMATED THIS WRONG BEFORE MEASURING IT, which is why the file exists.
// Counting the setup states in the RTL suggests about four clocks, and four
// clocks against a 16-row walk is a rounding error nobody should restructure a
// binner for. The measurement says TWENTY-ONE, and for a thin sliver that is 21
// of the job's 26 clocks. Setup is not a preamble to the walk; on small
// triangles it IS the job.
//
// So this file measures the two numbers the decision needs and refuses to make
// the decision:
//
//   1. The SETUP COST: clocks from a job being accepted to its first coverage
//      beat. That is what a context cache would remove, and only that -- the
//      row walk is per-tile work either way.
//
//   2. The TOTAL per job, so the setup can be read as a fraction rather than as
//      a scary-looking absolute.
//
// Then it prices ruling 4 against the four scenes tools/render/count_bin_load
// measures, whose references-per-triangle span two orders of magnitude:
//
//      sky backdrop        2 tris,  396 refs  -> 198.0 refs/tri
//      terrain patch    2,048 tris, 4,080 refs ->   2.0 refs/tri
//      creature army   19,200 tris, 23,912 refs ->   1.2 refs/tri
//      giant near camera  126 tris, 25,704 refs -> 204.0 refs/tri
//
// A cache saves setup on every reference AFTER the first, so the saving is
// (refs - tris) x setup_clocks. On the army that is almost nothing; on the
// giant it is almost everything. An optimisation whose value swings by two
// orders of magnitude between two scenes in the same game is a decision, not a
// conclusion.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_raster_edgewalk.h"

#include "zhao_sim.hpp"

namespace {

constexpr int64_t kClocksPerFrame = 1666667;

struct Job {
  int32_t ax, ay, bx, by, cx, cy;
  int tile_x, tile_y;
};

struct Timing {
  int to_first;  // accept -> first coverage beat: the setup
  int total;     // accept -> job_done
  int rows;
  int covered;
};

Timing measure(Vzhao_raster_edgewalk& t, const Job& j) {
  t.rst_n = 0;
  t.job_valid_i = 0;
  t.cov_ready_i = 1;
  t.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();

  t.job_ax_i = static_cast<uint32_t>(j.ax);
  t.job_ay_i = static_cast<uint32_t>(j.ay);
  t.job_bx_i = static_cast<uint32_t>(j.bx);
  t.job_by_i = static_cast<uint32_t>(j.by);
  t.job_cx_i = static_cast<uint32_t>(j.cx);
  t.job_cy_i = static_cast<uint32_t>(j.cy);
  t.job_tile_x_i = static_cast<uint16_t>(j.tile_x) & 0xFFFu;
  t.job_tile_y_i = static_cast<uint16_t>(j.tile_y) & 0xFFFu;
  t.job_src_id_i = 0x1234;
  t.job_valid_i = 1;

  for (int i = 0; i < 20; ++i) {
    t.eval();
    if (t.job_ready_o) {
      zhao::tick(t);
      break;
    }
    zhao::tick(t);
  }
  t.job_valid_i = 0;

  Timing r{-1, -1, 0, 0};
  int clocks = 0;
  for (; clocks < 2000; ++clocks) {
    t.eval();
    if (t.cov_valid_o && t.cov_ready_i) {
      if (r.to_first < 0) r.to_first = clocks;
      ++r.rows;
      uint16_t m = t.cov_mask_o;
      while (m) {
        r.covered += m & 1;
        m >>= 1;
      }
    }
    if (t.job_done_o) {
      r.total = clocks + 1;
      zhao::tick(t);
      return r;
    }
    zhao::tick(t);
  }
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_edgewalk top;

  auto px = [](int v) { return v * 256; };

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: what setup costs, isolated from the row walk ==\n");
  int setup_clocks = -1;
  {
    // Three triangles over the SAME tile with very different coverage. Setup is
    // tile-independent and coverage-independent, so if the accept-to-first-beat
    // figure is the setup, it must be IDENTICAL across all three while the
    // totals differ. That is the measurement being self-checking rather than
    // just a number I labelled.
    const struct {
      Job j;
      const char* what;
    } cases[] = {
        {{px(0), px(0), px(64), px(0), px(0), px(64), 0, 0}, "covers the whole tile"},
        {{px(0), px(0), px(64), px(2), px(2), px(4), 0, 0}, "a thin sliver"},
        {{px(4), px(4), px(12), px(4), px(4), px(12), 0, 0}, "a small patch"},
    };

    int firsts[3] = {-1, -1, -1};
    int idx = 0;
    for (const auto& c : cases) {
      const Timing t = measure(top, c.j);
      printf("   %-24s setup %2d, total %3d, rows %2d, covered %3d\n", c.what, t.to_first, t.total,
             t.rows, t.covered);
      firsts[idx++] = t.to_first;
      zhao::check(t.total > 0, "the job completes", 1, t.total > 0 ? 1 : 0);
      zhao::check(t.covered > 0, "and covers something, so the walk really ran", 1,
                  t.covered > 0 ? 1 : 0);
    }
    setup_clocks = firsts[0];
    zhao::check(firsts[0] == firsts[1] && firsts[1] == firsts[2],
                "setup costs the same regardless of coverage, so it IS the setup", 1,
                (firsts[0] == firsts[1] && firsts[1] == firsts[2]) ? 1 : 0);
    printf("   MEASURED: setup = %d clocks a job\n", setup_clocks);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: and it is the same at any tile, which is the whole point ==\n");
  {
    // Ruling 4's claim is that the coefficients are TILE-INDEPENDENT. If the
    // setup cost varied with the tile, caching it would be wrong as well as
    // expensive. Same triangle, four different tiles.
    const Job base = {px(0), px(0), px(200), px(8), px(8), px(200), 0, 0};
    int firsts[4];
    for (int k = 0; k < 4; ++k) {
      Job j = base;
      j.tile_x = k * 4;
      j.tile_y = k * 3;
      const Timing t = measure(top, j);
      firsts[k] = t.to_first;
    }
    bool same = true;
    for (int k = 1; k < 4; ++k)
      if (firsts[k] != firsts[0]) same = false;
    zhao::check(same && firsts[0] == setup_clocks,
                "the same triangle pays the same setup at every tile", 1,
                (same && firsts[0] == setup_clocks) ? 1 : 0);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: pricing ruling 4 against the four measured scenes ==\n");
  {
    // A context cache removes setup on every reference AFTER the first, so the
    // saving is (refs - tris) * setup. These four scenes are
    // tools/render/count_bin_load's output, not estimates.
    const struct {
      const char* what;
      long tris, refs;
    } scenes[] = {
        {"sky backdrop", 2, 396},
        {"terrain patch 32x32", 2048, 4080},
        {"creature army", 19200, 23912},
        {"giant near camera", 126, 25704},
    };
    printf("   %-22s %8s %8s %8s %10s %8s\n", "scene", "tris", "refs", "refs/tri", "saved clk",
           "of frame");
    for (const auto& s : scenes) {
      const long saved = (s.refs - s.tris) * setup_clocks;
      printf("   %-22s %8ld %8ld %8.1f %10ld %7.2f%%\n", s.what, s.tris, s.refs,
             (double)s.refs / (double)s.tris, saved,
             100.0 * (double)saved / (double)kClocksPerFrame);
    }
    printf("   COST: the per-triangle record grows 285 bits (3 edge equations of\n");
    printf("         94 bits, plus 3 top-left flags) on a 142-bit record -- 3x.\n");
    printf("         At the shipped TRI_CAP = 128 that is 36 Kbit, which is nothing.\n");
    printf("         At the ~19,200 a real army needs it is 8.2 Mbit against 2.7 --\n");
    printf("         so the cost is trivial at the capacity we have and severe at\n");
    printf("         the capacity we do not, which is the arena decision again.\n");

    // The finding this file exists to make checkable: the value of ruling 4
    // swings by orders of magnitude between scenes in the same game, so it
    // cannot be settled by reasoning about one of them.
    const double army = (double)(23912 - 19200) / 19200.0;
    const double giant = (double)(25704 - 126) / 126.0;
    zhao::check(giant > army * 100,
                "the saving per triangle differs by over 100x between two real scenes", 1,
                (giant > army * 100) ? 1 : 0);
    zhao::check(setup_clocks > 0, "and the setup cost was actually measured", 1,
                setup_clocks > 0 ? 1 : 0);
  }

  return zhao::report_and_exit("raster_edgewalk_setupcost_directed");
}
