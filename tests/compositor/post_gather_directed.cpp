// post_gather_directed.cpp — does the effect gather obey R5, including the
// clauses that are about ANOTHER block?
//
// ---------------------------------------------------------------------------
// THE PROPERTIES WORTH CHECKING, AND WHY THEY ARE THESE
// ---------------------------------------------------------------------------
// 1. IT NEVER BACKPRESSURES RESOLVE. There is no `ready` on the fragment port
//    at all, so the check is that a fragment offered on every clock, forever,
//    is always accounted for. An interface with no ready cannot stall; what it
//    CAN do is drop, and that is what this tests.
//
// 2. ACCUMULATE, DO NOT AVERAGE. One very bright fragment must light the cell.
//    A cell fed one 255 and fifteen zeros must not read as 16.
//
// 3. THE CLAMPS ARE ANOTHER BLOCK'S BOUND. X to [-8,+8] and Y to [-4,+4] are
//    what POST.COMPOSITE's nine-line ring is built against. Widening them here
//    would break a block that is not this one, and no test in THAT block would
//    catch it — so the bound is asserted here, at the place that could break
//    it, with the reason attached.
//
// 4. EVERY TILE WRITES ALL SIXTEEN CELLS, INCLUDING ZEROS. That is what
//    removes the frame reset pass. A flush that skipped empty cells would
//    leave last frame's light in them, and would look correct on any tile that
//    happened to be busy.
//
// 5. ONE ROUNDING. Glow accumulates in u16 and is packed to RGB565 exactly
//    once, at flush.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_post_gather.h"

#include "zhao_sim.hpp"
#include "zref/zref_post.hpp"

namespace {

uint32_t rnd(uint32_t* s) {
  *s = *s * 1664525u + 1013904223u;
  return (*s >> 8);
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_post_gather top;

  auto idle = [&]() {
    top.f_valid_i = 0;
    top.tile_start_i = 0;
    top.tile_flush_i = 0;
  };

  auto reset = [&]() {
    idle();
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);
  };

  auto frag = [&](int x, int y, int r, int g, int b, int dx, int dy, bool ink) {
    top.f_valid_i = 1;
    top.f_x_i = x; top.f_y_i = y;
    top.f_glow_r_i = r; top.f_glow_g_i = g; top.f_glow_b_i = b;
    top.f_disp_x_i = dx; top.f_disp_y_i = dy;
    top.f_ink_i = ink ? 1 : 0;
    zhao::tick(top);
    top.f_valid_i = 0;
  };

  struct Cell { int glow, dx, dy, ink; bool seen; };

  auto flush = [&]() {
    std::vector<Cell> out(16, Cell{0, 0, 0, 0, false});
    top.tile_flush_i = 1;
    zhao::tick(top);
    top.tile_flush_i = 0;
    for (int c = 0; c < 40; ++c) {
      top.eval();
      if (top.c_valid_o) {
        Cell& e = out[top.c_index_o];
        e.glow = top.c_glow_o;
        e.dx = static_cast<int8_t>(top.c_disp_x_o);
        e.dy = static_cast<int8_t>(top.c_disp_y_o);
        e.ink = top.c_ink_o;
        e.seen = true;
      }
      zhao::tick(top);
    }
    return out;
  };

  // ---- 1: a fragment every clock, forever, and nothing is lost ------------
  {
    reset();
    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;

    uint32_t s = 0x5EED77u;
    int offered = 0;
    for (int c = 0; c < 4000; ++c) {
      const int x = static_cast<int>(rnd(&s) & 15u);
      const int y = static_cast<int>(rnd(&s) & 15u);
      frag(x, y, 1, 0, 0, 0, 0, false);
      ++offered;
    }
    zhao::check(top.fragments_o == static_cast<uint32_t>(offered),
                "4,000 fragments offered on 4,000 consecutive clocks are all "
                "accounted for -- there is no ready line to stall, so the only "
                "failure available is dropping one",
                offered, static_cast<int>(top.fragments_o));
  }

  // ---- 2: ACCUMULATE, DO NOT AVERAGE -------------------------------------
  {
    reset();
    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;

    // one very bright fragment, then fifteen dark ones, all in cell 0
    frag(0, 0, 255, 255, 255, 0, 0, false);
    for (int i = 0; i < 15; ++i) frag(1, 1, 0, 0, 0, 0, 0, false);

    top.tile_start_i = 1;   // swap so the flush reads what we just filled
    zhao::tick(top);
    top.tile_start_i = 0;
    const auto out = flush();

    // 255 in every channel saturates the 5/6/5 pack to white.
    // The expected value comes from the oracle, not from a constant typed
    // here: 0xFFFF happens to be right and would also be right for a block
    // that packed a different way.
    uint16_t acc = zref::post::glow_accumulate(0, 255);
    for (int i = 0; i < 15; ++i) acc = zref::post::glow_accumulate(acc, 0);
    const uint16_t want = zref::post::glow_pack565(acc, acc, acc);
    zhao::check(out[0].glow == want,
                "one bright fragment among fifteen dark ones LIGHTS the cell, "
                "packed exactly as zref::post::glow_pack565 packs it -- "
                "averaging would have diluted it to nothing",
                want, out[0].glow);
  }

  // ---- 3: THE CLAMPS ARE POST.COMPOSITE'S BOUND --------------------------
  // R5 fixes X to [-8,+8] and Y to [-4,+4] because the compositor keeps nine
  // source lines and reaches +/-8 horizontally. Nothing in POST.COMPOSITE can
  // detect a gather that exceeds them; this is the place that can.
  {
    reset();
    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;

    // 8.8 format: 100 pixels right and 100 down, far past both bounds
    frag(0, 0, 0, 0, 0, 100 * 256, 100 * 256, false);
    frag(4, 0, 0, 0, 0, -100 * 256, -100 * 256, false);

    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;
    const auto out = flush();

    bool cx = false, cy = false;
    const int wx = zref::post::disp_to_pixels(static_cast<int16_t>(100 * 256),
                                              zref::post::kDispMaxX, &cx);
    const int wy = zref::post::disp_to_pixels(static_cast<int16_t>(100 * 256),
                                              zref::post::kDispMaxY, &cy);
    zhao::check(out[0].dx == wx && out[0].dy == wy && cx && cy,
                "a huge positive displacement clamps exactly as "
                "zref::post::disp_to_pixels clamps it", 1,
                (out[0].dx == wx && out[0].dy == wy) ? 1 : 0);
    zhao::check(out[0].dx == 8 && out[0].dy == 4,
                "a huge positive displacement clamps to +8 in X and +4 in Y -- "
                "the bound POST.COMPOSITE's nine-line ring is built against",
                1, (out[0].dx == 8 && out[0].dy == 4) ? 1 : 0);
    zhao::check(out[1].dx == -8 && out[1].dy == -4,
                "and a huge negative one clamps to -8 and -4", 1,
                (out[1].dx == -8 && out[1].dy == -4) ? 1 : 0);
    zhao::check(top.disp_clamps_o >= 2,
                "and the clamps are counted -- a frame that clamps a lot is a "
                "frame asking for a displacement the compositor cannot serve",
                1, top.disp_clamps_o >= 2 ? 1 : 0);
  }

  // ---- 4: EVERY CELL IS WRITTEN, INCLUDING THE EMPTY ONES ----------------
  {
    reset();
    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;
    frag(0, 0, 200, 200, 200, 0, 0, true);   // exactly one cell touched

    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;
    const auto out = flush();

    int unseen = 0, nonzero = 0;
    for (int i = 0; i < 16; ++i) {
      if (!out[i].seen) ++unseen;
      if (i != 0 && (out[i].glow != 0 || out[i].dx != 0 || out[i].dy != 0 ||
                     out[i].ink != 0))
        ++nonzero;
    }
    zhao::check(unseen == 0,
                "all sixteen cells are written including the fifteen that "
                "received nothing -- this is what removes the frame reset pass",
                0, unseen);
    zhao::check(nonzero == 0,
                "and the untouched fifteen are written as ZERO, not left "
                "holding the previous tile",
                0, nonzero);
    zhao::check(out[0].ink == 1, "and the ink bit ORs into its own cell", 1,
                out[0].ink);
  }

  // ---- 5: the banks really do ping-pong ----------------------------------
  // Accumulating into one bank while the other flushes is the whole reason
  // resolve never stalls. If they were one bank, a flush would have to block.
  {
    reset();
    top.tile_start_i = 1;
    zhao::tick(top);
    top.tile_start_i = 0;
    frag(0, 0, 255, 0, 0, 0, 0, false);      // tile A: red in cell 0

    top.tile_start_i = 1;                     // swap: A becomes flushable
    zhao::tick(top);
    top.tile_start_i = 0;
    frag(0, 0, 0, 0, 255, 0, 0, false);      // tile B: blue in cell 0

    // flushing A must give A's red, not B's blue, while B keeps accumulating
    const auto a = flush();
    const int r5 = (a[0].glow >> 11) & 0x1F;
    const int b5 = a[0].glow & 0x1F;
    zhao::check(r5 == 31 && b5 == 0,
                "flushing one bank reports what THAT bank accumulated while the "
                "other was still being written -- the ping-pong is why resolve "
                "never has to wait",
                1, (r5 == 31 && b5 == 0) ? 1 : 0);
  }

  std::printf("  %u fragments, %u glow saturations, %u displacement clamps, "
              "%u cells flushed\n",
              top.fragments_o, top.glow_saturations_o, top.disp_clamps_o,
              top.cells_flushed_o);

  return zhao::report_and_exit("post_gather_directed");
}
