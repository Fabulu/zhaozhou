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
// The fix is sound. What matters is HOW BIG, because it is not free -- the
// binner's per-triangle record would have to carry three edge equations
// (3 x (23 + 23 + 48) bits) plus the top-left flags, roughly tripling it.
//
// ---------------------------------------------------------------------------
// I MEASURED THE WRONG THING FIRST, AND IT LOOKED LIKE EVIDENCE
// ---------------------------------------------------------------------------
// The obvious probe is "clocks from job accept to the first coverage beat", and
// it reports a confident, stable 21. It is wrong, and its stability is what
// makes it convincing rather than what makes it right.
//
// `zhao_raster_edgewalk` walks ALL SIXTEEN ROWS into registers in S_WALK before
// S_DRAIN emits a single beat. So that 21 is setup PLUS the whole row walk --
// and the row walk is per-tile work that a context cache does not remove. Worse,
// my "self-check" was that the figure is identical for a full tile, a thin
// sliver and a small patch, which I read as proof it was setup alone. It is
// identical because THE WALK IS ALWAYS 16 ROWS regardless of coverage. The
// check could not have failed, so it confirmed nothing.
//
// That first version priced ruling 4 at up to 32% of a frame. The real figure
// is a quarter of that. Same block, same clocks, a probe pointed at the wrong
// interval.
//
// ---------------------------------------------------------------------------
// WHAT THIS VERSION DOES INSTEAD
// ---------------------------------------------------------------------------
// Setup is not directly observable from the ports -- there is no signal that
// says "coefficients ready". So it is DERIVED and the derivation is made to
// carry risk:
//
//     total = accept + SETUP + 16 (the walk) + drain_beats + 1
//     =>  SETUP = total - 16 - drain_beats - 1
//
// `drain_beats` varies from 4 to 16 across the cases, so if the model were
// wrong the derived SETUP would move with coverage. It does not, and that is a
// check that could have failed.
//
// It is then cross-checked against a completely different path: a DEGENERATE
// triangle leaves at S_W0, as soon as the area is known, and never walks at
// all. Its total is a direct, walk-free measurement of the front of the same
// setup sequence, and it must be smaller than the derived SETUP and larger than
// zero. Two independent routes to one number.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_raster_edgewalk.h"

#include "zhao_sim.hpp"

namespace {

constexpr int64_t kClocksPerFrame = 1666667;
// S_WALK always runs the full 16 rows into registers before S_DRAIN emits.
constexpr int kWalkRows = 16;

struct Job {
  int32_t ax, ay, bx, by, cx, cy;
  int tile_x, tile_y;
};

struct Timing {
  int to_first;  // accept -> first coverage beat: setup AND the walk
  int total;     // accept -> job_done
  int rows;      // coverage beats actually drained
  int covered;
  bool degenerate;
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

  Timing r{-1, -1, 0, 0, false};
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
      r.degenerate = t.job_degenerate_o != 0;
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
  printf("== section 1: setup, DERIVED, with the derivation carrying risk ==\n");
  int setup_clocks = -1;
  {
    // Three triangles over the same tile with very different coverage, so
    // `drain_beats` moves from 4 to 16. If the model total = setup + 16 + beats
    // + 1 were wrong, the derived setup would move with it.
    const struct {
      Job j;
      const char* what;
    } cases[] = {
        {{px(0), px(0), px(64), px(0), px(0), px(64), 0, 0}, "covers the whole tile"},
        {{px(0), px(0), px(64), px(2), px(2), px(4), 0, 0}, "a thin sliver"},
        {{px(4), px(4), px(12), px(4), px(4), px(12), 0, 0}, "a small patch"},
    };

    int derived[3] = {-1, -1, -1};
    int idx = 0;
    for (const auto& c : cases) {
      const Timing t = measure(top, c.j);
      const int setup = t.total - kWalkRows - t.rows - 1;
      printf("   %-24s total %3d, beats %2d, to-first %2d  ->  setup %2d\n", c.what, t.total,
             t.rows, t.to_first, setup);
      derived[idx++] = setup;
      zhao::check(t.total > 0, "the job completes", 1, t.total > 0 ? 1 : 0);
      zhao::check(t.covered > 0, "and covers something, so the walk really ran", 1,
                  t.covered > 0 ? 1 : 0);
    }
    setup_clocks = derived[0];
    zhao::check(derived[0] == derived[1] && derived[1] == derived[2],
                "the derived setup is the same though the drain length is not", 1,
                (derived[0] == derived[1] && derived[1] == derived[2]) ? 1 : 0);
    printf("   MEASURED: setup = %d clocks a job (NOT the 21 that accept-to-first-beat\n",
           setup_clocks);
    printf("             reports -- that figure contains the whole 16-row walk)\n");
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: cross-checked on a path that never walks at all ==\n");
  {
    // A zero-area triangle is rejected at S_W0, the moment the area lands, and
    // never enters S_WALK. Its total is a direct measurement of the front of the
    // setup sequence with no walk in it -- a completely different route to the
    // same region of the state machine.
    const Job degen = {px(10), px(10), px(40), px(10), px(70), px(10), 0, 0};
    const Timing d = measure(top, degen);
    printf("   MEASURED: a degenerate triangle retires in %d clocks, no walk\n", d.total);
    zhao::check(d.degenerate, "the degenerate case really was rejected as degenerate", 1,
                d.degenerate ? 1 : 0);
    zhao::check(d.rows == 0, "and emitted no coverage", 0, (uint32_t)d.rows);
    zhao::check(d.total > 0 && d.total <= setup_clocks,
                "and its cost sits inside the derived setup, as the state order says", 1,
                (d.total > 0 && d.total <= setup_clocks) ? 1 : 0);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: and the setup really is tile-independent ==\n");
  {
    // Ruling 4's premise. If it varied with the tile, caching would be wrong as
    // well as expensive.
    const Job base = {px(0), px(0), px(200), px(8), px(8), px(200), 0, 0};
    int derived[4];
    for (int k = 0; k < 4; ++k) {
      Job j = base;
      j.tile_x = k * 4;
      j.tile_y = k * 3;
      const Timing t = measure(top, j);
      derived[k] = t.total - kWalkRows - t.rows - 1;
    }
    bool same = true;
    for (int k = 1; k < 4; ++k)
      if (derived[k] != derived[0]) same = false;
    zhao::check(same && derived[0] == setup_clocks,
                "the same triangle pays the same setup at every tile", 1,
                (same && derived[0] == setup_clocks) ? 1 : 0);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: pricing ruling 4 against the four measured scenes ==\n");
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
    printf("   NOTE: the 16-row walk is per-tile work no cache removes, and at %d\n",
           kWalkRows);
    printf("         clocks it is larger than the setup. If the drain is ever the\n");
    printf("         thing to shorten, THAT is where the clocks are.\n");

    // The finding that makes this undecidable from any one scene.
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
