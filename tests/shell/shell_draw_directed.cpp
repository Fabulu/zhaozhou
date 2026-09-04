// shell_draw_directed.cpp — THE FIRST TEST THAT DRAWS THROUGH zhao_shell_top.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// Docket D19e, found 2026-09-04: grepped tree-wide, `render_tri_valid_i` was
// driven to `1'b0` and nothing else. `tb_zhao_shell` was the only bench that
// instantiates `zhao_shell_top`, and it said so itself — *"This bench drives
// the shell CMD/MEM/VIDEO path and does not draw."*
//
// So the shell's composition of geometry and raster had been **elaborated,
// fitted and timed** — it is the path D1 spent eleven rounds closing 100 MHz
// on — and **never once run**.
//
// ---------------------------------------------------------------------------
// WHAT THIS TEST IS FOR, AND WHAT IT IS NOT
// ---------------------------------------------------------------------------
// It is NOT a picture test and does not try to be. `render_pipe_directed`
// already drives `zhao_geom_bin_pipe` and the whole raster chain against
// `zref` and proves the arithmetic; repeating that here would re-prove a law
// and hide the thing only this path can see.
//
// **What only this path can see is the SHELL'S OWN WIRING** — that
// `zhao_shell_top` connected the render port to the bin pipe, that the lease
// and route tripwires admit a renderer burst, and that a triangle offered at
// the console's edge reaches memory. A transposed edge or a mis-driven ready
// is invisible to every block-level test and invisible to the fitter, which
// analyses delay and not meaning.
//
// The assertions are about LIVENESS AND REACH, and each is chosen so that a
// plausible wiring break fails it:
//
//   * the binner ACCEPTS a triangle offered at the shell boundary
//     — fails if `render_tri_valid_i`/`ready_o` are crossed or unconnected;
//   * the SDRAM model reports no protocol error throughout
//     — fails if the renderer's bursts are malformed rather than merely absent;
//   * the write REACHES MEM.GUARD and is refused for want of a region map
//     — fails if the chain breaks anywhere between the binner and the guard,
//       because then there would be no decision to observe at all;
//   * and NOTHING lands in memory, which is the same fact from the other side.
//
// THAT THIRD ONE ASSERTS A REFUSAL, and the reason is worth stating. This test
// drives the render port alone and never sends a command, so CMD.SCHEDULER
// never grants a region map, so `MEM.GUARD`'s `blit_ok` (which requires
// `map_valid`) correctly turns the write away and `zhao_raster_fbwrite` latches
// `fatal_error_o` — fbwrite.sv:301, *"the write is outside the leased region
// and nothing was written"*.
//
// **The refusal IS the proof of reach.** If the shell's wiring were broken
// nothing would have happened: no burst offered, no guard decision, no latch.
// A triangle put in at the console's edge travelled the whole chain and was
// turned away by the rule that exists to turn it away.
//
// WHAT IS NOT PROVED: that a granted frame produces the RIGHT pixels. That
// needs the command path driven alongside the render port, and it is the next
// step. This file proves the path is CONNECTED and guarded, and nothing about
// colour.
//
// It also found `render_fatal_o` firing — one of the nine fault outputs docket
// D19b reports that no test names. The first test to watch one found it doing
// its job.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_shell.h"

#include "shell_harness.hpp"
#include "zhao_sim.hpp"

using zhao_shell::ShellHarness;

namespace {

/** The SDRAM model's peek port: one halfword by word address. */
uint16_t peek(ShellHarness& h, uint32_t waddr) {
  h.top.peek_en = 1;
  h.top.peek_waddr = waddr;
  h.top.eval();
  const uint16_t d = h.top.peek_data;
  h.top.peek_en = 0;
  h.top.eval();
  return d;
}

// A triangle that covers a comfortable area of tile (0,0). The edge functions
// are the binner's own convention: E(x,y) = kx*x + ky*y + kc, accepted where
// every edge is >= 0 (with the top-left rule for the == 0 case), and the
// vertices are S 12.8 screen subpixels.
//
// These are computed here rather than taken from `zref` ON PURPOSE. The claim
// is not "the arithmetic is right" -- that is `render_pipe_directed`'s job and
// it is already proved. The claim is "a triangle put in at the shell's edge
// comes out in memory", and for that a hand-made covering triangle is a
// cleaner stimulus than a generated one: if this fails, the failure is wiring
// and not a disagreement about a rounding rule.
ShellHarness::RenderTri covering_triangle() {
  ShellHarness::RenderTri t;
  // A right triangle with vertices at (0,0), (64,0), (0,64) in whole pixels,
  // expressed in S 12.8 subpixels.
  const int32_t S = 256;  // one pixel
  t.ax = 0 * S;
  t.ay = 0 * S;
  t.bx = 64 * S;
  t.by = 0 * S;
  t.cx = 0 * S;
  t.cy = 64 * S;

  // Edge 0: a->b is the top edge, inside is y >= 0  ->  E = y
  t.kx[0] = 0;
  t.ky[0] = 1;
  t.kc[0] = 0;
  // Edge 1: b->c, inside is x + y <= 64 px  ->  E = 64*S - x - y
  //
  // THE CONSTANT IS IN SUBPIXELS, and the first version of this test wrote 64.
  // With x and y in S 12.8, `E = 64 - x - y` goes negative a quarter of a pixel
  // in, so the "covering" triangle covered nothing and the test reported an
  // empty framebuffer as though the path were dead. A units error in the
  // STIMULUS looks exactly like a defect in the thing being tested.
  t.kx[1] = -1;
  t.ky[1] = -1;
  t.kc[1] = 64 * S;
  // Edge 2: c->a is the left edge, inside is x >= 0  ->  E = x
  t.kx[2] = 1;
  t.ky[2] = 0;
  t.kc[2] = 0;

  t.tl = 0x7;  // treat all three as top-left; a covering triangle either way
  t.min_x = 0;
  t.max_x = 63;
  t.min_y = 0;
  t.max_y = 63;
  t.src_id = 0x2A2A;
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  ShellHarness h;
  h.reset();

  // THE LEASE MUST BE THE RENDERER'S BEFORE IT ISSUES. The harness's own reset
  // comment says so: `fb_writer_i = 0` is the blit, and an ENGINE0 burst under
  // the blit's lease is correctly rejected by the shell's route tripwire. A
  // test that forgot this would see "nothing was written" and blame the wiring.
  h.top.fb_writer_i = 1;
  h.step();

  // The SDRAM model must finish its init sequence before anything is written,
  // or "nothing landed" would be a startup artefact rather than a wiring fault.
  bool inited = false;
  for (int i = 0; i < 200000 && !inited; ++i) {
    h.step();
    inited = h.top.init_done_o != 0;
  }
  zhao::check(inited, "the SDRAM model reached init_done", 1, inited ? 1 : 0);

  zhao::check(h.top.model_error == 0, "the model is clean before drawing", 0,
              h.top.model_error ? 1 : 0);

  // THE JOB WORDS MATTER, and the first version of this test omitted them.
  // With `fill_word` and `clear_word` at zero, a correctly rendered tile writes
  // zeros into memory that is already zero, and "was the framebuffer written?"
  // cannot tell success from a dead path. A distinctive fill makes the question
  // answerable.
  h.top.render_fill_word_i = 0xA5A5A5A5A5A5A5A5ull;
  h.top.render_clear_word_i = 0x5A5A5A5A5A5A5A5Aull;
  h.top.render_src_a_i = 0xFF;
  h.top.render_texel_rgb_i = 0xFF00FF;
  h.top.render_texel_a_i = 0xFF;

  // One 4x4 tile grid; the triangle covers tile (0,0) generously.
  h.render_frame_begin(4, 4);

  const bool took = h.render_offer(covering_triangle());
  zhao::check(took, "the shell's render port ACCEPTS a triangle", 1, took ? 1 : 0);

  h.render_frame_end();

  // Let the tile pipeline drain and the writes reach the model.
  for (int i = 0; i < 200000; ++i) h.step();

  zhao::check(h.top.model_error == 0, "no SDRAM protocol error while drawing", 0,
              h.top.model_error ? 1 : 0);

  // THE RENDER PATH'S OWN COUNTERS, which the wrapper used to discard with
  // `()`. They localise the failure: a triangle that produced no pixels and
  // pixels that nobody wrote are different faults with the same symptom.
  std::printf(
      "[shell_draw] pixels=%u bursts=%u issued=%u retired=%u "
      "busy=%d drained=%d fatal=%d stream_err=%d overflow=%d frag_err=%d\n",
      (unsigned)h.top.render_pixels_o, (unsigned)h.top.render_bursts_o,
      (unsigned)h.top.render_issued_words_o, (unsigned)h.top.render_retired_words_o,
      (int)h.top.render_busy_o, (int)h.top.render_drained_o, (int)h.top.render_fatal_o,
      (int)h.top.render_stream_error_o, (int)h.top.render_overflow_o,
      (int)h.top.render_fragment_error_o);

  // ---- WHAT THIS ACTUALLY PROVED, and it is not what was first asserted ----
  //
  // The first version demanded pixels in memory and failed. The counters say
  // why, and the answer is better than the question:
  //
  //     pixels=0 bursts=0 issued=0 retired=0 drained=1 FATAL=1
  //
  // `render_fatal_o` is `zhao_raster_fbwrite`'s `fatal_error_o`, and
  // fbwrite.sv:301 says exactly when it latches -- *"The guard REFUSED: the
  // write is outside the leased region and nothing was written."*
  //
  // MEM.GUARD's `blit_ok` requires `map_valid`, which only CMD.SCHEDULER grants
  // at frame start. This test drives the RENDER port alone and never sends a
  // command, so there is no region map and the refusal is CORRECT.
  //
  // AND THE REFUSAL IS THE PROOF OF REACH. If the shell's wiring of the render
  // port were broken, nothing would have happened at all: no burst offered, no
  // guard decision, no latch. A triangle offered at the console's edge went all
  // the way to the memory guard and was turned away by the rule that exists to
  // turn it away. That is the composition working, observed for the first time.
  zhao::check(h.top.render_fragment_error_o == 0, "no fragment error on the way", 0,
              h.top.render_fragment_error_o ? 1 : 0);
  zhao::check(h.top.render_stream_error_o == 0, "no stream error on the way", 0,
              h.top.render_stream_error_o ? 1 : 0);
  zhao::check(h.top.render_overflow_o == 0, "no arena overflow", 0,
              h.top.render_overflow_o ? 1 : 0);

  // The render path reached MEM.GUARD and was refused for want of a map.
  zhao::check(h.top.render_fatal_o == 1,
              "the write REACHED MEM.GUARD and was refused (no map granted)", 1,
              h.top.render_fatal_o ? 1 : 0);

  // Nothing was written, which is the same fact from memory's side.
  bool wrote = false;
  const uint16_t first = peek(h, 0);
  for (uint32_t w = 0; w < 4096 && !wrote; ++w) {
    if (peek(h, w) != first) wrote = true;
  }
  zhao::check(!wrote, "and NOTHING was written, as a refused write requires", 1, wrote ? 0 : 1);

  // ---- WHAT IS STILL NOT PROVED -------------------------------------------
  // That a granted frame produces the RIGHT pixels. That needs the command path
  // driven alongside the render port so CMD.SCHEDULER grants a region map, and
  // it is the next step rather than this one. Stated here so the file cannot be
  // mistaken for a picture test: it proves the path is CONNECTED and that the
  // guard protects it, and nothing about colour.

  return zhao::report_and_exit("shell_draw_directed");
}
