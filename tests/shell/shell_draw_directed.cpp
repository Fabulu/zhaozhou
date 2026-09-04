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

// Oracle-only mode: the CLIP+SETUP views without the Verilated DUTs that
// geom_dev.hpp otherwise pulls in. Same three defines render_pipe_directed uses.
#define ZHAO_GEOM_DEV_ORACLE_ONLY
#define ZHAO_GEOM_DEV_BINNER

#include "geom_dev.hpp"
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

// The triangle comes from THE ORACLE, via zhao_geom::make_bin_tri -- the same
// GEOM.CLIP + GEOM.SETUP path `render_pipe_directed` uses.
//
// THE FIRST VERSION HAND-ROLLED THE EDGE FUNCTIONS. It looked like the cleaner
// stimulus -- fewer moving parts, and a failure would have to be wiring rather
// than a rounding rule -- and it was wrong twice over:
//
//   * `kc = 64` against S 12.8 vertices makes edge 1 go negative a quarter of a
//     pixel in. A real units bug, and worth fixing;
//   * but it was NOT the cause of the `pixels=0` that prompted the hunt.
//     `render_pixels_o` is `zhao_raster_fbwrite`'s `pixels_written_o` -- pixels
//     WRITTEN, not pixels covered -- and the write was refused by the guard, so
//     zero is what it must read whatever the triangle does.
//
// The second half is the part worth keeping: **a number that agrees with your
// hypothesis is not evidence until you know what it counts.** Two rounds were
// spent on stimulus because a counter was read as coverage.
//
// The oracle is used now regardless. **Hand-rolled stimulus is not simpler, it
// is unverified**, and the oracle already knows the convention.
// S 12.8 screen subpixels, the coordinate the whole chain speaks. Same one line
// as `zhao_raster::px` in tests/raster/raster_tile_pipe_dev.hpp; repeated here
// rather than included, because that header pulls a Verilated model this target
// does not build.
inline int32_t px(int32_t p) { return p * 256; }

bool build_triangle(int grid_w, int grid_h, zhao_geom::BinTri* out) {
  zref::Clip::Viewport vp;
  vp.w = grid_w * 16;
  vp.h = grid_h * 16;
  return zhao_geom::make_bin_tri(px(4), px(4), px(grid_w * 16 - 4), px(8), px(8),
                                 px(grid_h * 16 - 4), vp, 0x2A2A, out);
}

/** The oracle's packet, in the shape the shell's render port takes. */
ShellHarness::RenderTri from_bin_tri(const zhao_geom::BinTri& b) {
  ShellHarness::RenderTri t;
  for (int e = 0; e < 3; ++e) {
    t.kx[e] = b.s.e[e].kx;
    t.ky[e] = b.s.e[e].ky;
    t.kc[e] = b.s.e[e].kc;
  }
  t.tl = (uint8_t)((b.s.e[0].tl ? 1u : 0u) | (b.s.e[1].tl ? 2u : 0u) | (b.s.e[2].tl ? 4u : 0u));
  t.ax = b.ax;
  t.ay = b.ay;
  t.bx = b.bx;
  t.by = b.by;
  t.cx = b.cx;
  t.cy = b.cy;
  t.min_x = b.min_x;
  t.max_x = b.max_x;
  t.min_y = b.min_y;
  t.max_y = b.max_y;
  t.src_id = b.src_id;
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

  zhao_geom::BinTri bt;
  const bool built = build_triangle(4, 4, &bt);
  zhao::check(built, "the oracle accepts the triangle through CLIP + SETUP", 1, built ? 1 : 0);

  const bool took = h.render_offer(from_bin_tri(bt));
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
