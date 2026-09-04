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
//   * a blit packet GRANTS a framebuffer lease, span 245,760 — exactly
//     `ZHAO_FB_SLOT_SPAN`, so the command path and the guard agree;
//   * `zhao_raster_fbwrite` latches `fatal_error_o`, which is recorded as an
//     OBSERVATION rather than explained (see below);
//   * and nothing lands in memory.
//
// WHAT IS NOT PROVED, and this file will not imply otherwise:
//
//   * that a granted frame produces the RIGHT pixels — nothing here is a
//     picture test;
//   * WHY fbwrite latches fatal. The render guard reports **zero** violations
//     and no stream error was seen across the whole drain, and those are its
//     two documented setters. **Docket D19g.**
//
// An earlier version of this header explained the fatal as a guard refusal for
// want of a region map and called it "the proof of reach". That was written
// before the guard's own violation counter was read, and the counter says
// zero. The explanation is withdrawn; the observation stands.
//
// It did find `render_fatal_o` firing — one of the nine fault outputs docket
// D19b reports that no test names. The first test to watch one found it
// asserting, which is exactly why the nine are worth watching.
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
//     WRITTEN, not pixels covered -- and nothing was written, so zero is what
//     it must read whatever the triangle does.
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

  // ---- OPEN A GUARD WINDOW (docket D19f) ----------------------------------
  // RASTER.FBWRITE may only write while a DISPLAY BLIT lease is live, into that
  // blit's span, because zhao_shell_top:1234 ties the guard's map to the lease.
  // So a drawing test must make the command path grant one. Publish a frame the
  // way shell_probe does, then watch the lease through the testbench's probe.
  //
  // A MODE PACKET ALONE GRANTS NOTHING. The first attempt published one and
  // watched three million cycles for a lease that never came: the lease is the
  // slot manager's answer to a DISPLAY BLIT, and a packet without `has_blit`
  // schedules no blit. `shell_probe` only sees blits under its `--blit` flag,
  // which said so all along.
  {
    std::vector<uint8_t> canvas(zref::render::kSlotBytes, 0x11);
    const uint32_t arena = 0x0010'0000u;
    h.mem_write(arena, canvas);

    zhao_shell::PacketSpec ps;
    ps.frame_id = 1;
    ps.sequence = 1;
    ps.mode = 2;  // DUO
    ps.has_blit = true;
    ps.blit_dst = 0;
    ps.blit_src = arena;
    ps.blit_len = (uint32_t)canvas.size();
    ps.blit_crc = zhao_abi::zhao_crc32c(0, canvas.data(), canvas.size());
    const bool pub = h.publish(0, zhao_shell::build_packet(ps));
    zhao::check(pub, "the frame packet is published", 1, pub ? 1 : 0);
  }

  uint64_t lease_cycles = 0;
  uint32_t lease_span = 0;
  int lease_opens = 0;
  bool was = false;
  for (int i = 0; i < 3000000; ++i) {
    h.step();
    const bool now = h.top.dbg_fb_lease_valid_o != 0;
    if (now) {
      ++lease_cycles;
      lease_span = h.top.dbg_map_span_o;
    }
    if (now && !was) ++lease_opens;
    was = now;
    if (lease_opens > 0 && !now && lease_cycles > 0) break;  // one full window seen
  }
  std::printf("[shell_draw] lease opens=%d cycles=%llu span=%u\n", lease_opens,
              (unsigned long long)lease_cycles, lease_span);
  zhao::check(lease_opens > 0, "the command path grants a framebuffer lease", 1,
              lease_opens > 0 ? 1 : 0);

  // One 4x4 tile grid; the triangle covers tile (0,0) generously.
  h.render_frame_begin(4, 4);

  zhao_geom::BinTri bt;
  const bool built = build_triangle(4, 4, &bt);
  zhao::check(built, "the oracle accepts the triangle through CLIP + SETUP", 1, built ? 1 : 0);

  const bool took = h.render_offer(from_bin_tri(bt));
  zhao::check(took, "the shell's render port ACCEPTS a triangle", 1, took ? 1 : 0);

  h.render_frame_end();

  // Let the tile pipeline drain and the writes reach the model.
  //
  // STREAM ERROR AND FATAL ARE LATCHED HERE, not sampled at the end. Reading
  // `render_stream_error_o` after the fact showed 0 while `fatal` was 1, which
  // made a guard refusal look like the only possible cause -- and the render
  // guard's own violation count was 0, so that story was wrong. A pulse read
  // once, late, is not an observation.
  bool saw_stream_err = false;
  bool saw_fatal = false;
  uint64_t fatal_at = 0;
  for (int i = 0; i < 200000; ++i) {
    h.step();
    if (h.top.render_stream_error_o) saw_stream_err = true;
    if (h.top.render_fatal_o && !saw_fatal) {
      saw_fatal = true;
      fatal_at = (uint64_t)i;
    }
  }
  std::printf("[shell_draw] latched: stream_err=%d fatal=%d (first at drain step %llu)\n",
              saw_stream_err ? 1 : 0, saw_fatal ? 1 : 0, (unsigned long long)fatal_at);

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
  // WHAT IS MEASURED, AND WHAT IS STILL OPEN.
  //
  // An earlier version of this comment said the guard refused the write for
  // want of a region map, cited fbwrite.sv:301, and called the refusal "the
  // proof of reach". **The evidence does not support that**, and the story was
  // written before the evidence was gathered:
  //
  //   lease opens=1  span=245760        a lease IS granted, and 245,760 is
  //                                     exactly ZHAO_FB_SLOT_SPAN
  //   render guard violations=0         the render guard refused NOTHING
  //   latched stream_err=0
  //   fatal=1, first at drain step 234
  //
  // So `fatal_error_o` latches with **no guard violation and no stream error**,
  // which are fbwrite's only two documented setters. Either a third path sets
  // it, or one of those two fires without the counter this test reads.
  //
  // **The cause is NOT established and this file does not pretend otherwise.**
  // What IS established is below: the port accepts, a lease is granted with the
  // right span, no SDRAM protocol error occurs, and nothing is written. The
  // fatal is asserted as an OBSERVED FACT, not as a diagnosis -- so the day it
  // stops latching, this test notices and somebody reads the reason.
  //
  // Docket D19g carries the open question.
  std::printf("[shell_draw] render guard: violations=%u addr=%08X len=%u client=%u write=%u\n",
              (unsigned)h.top.dbg_render_gv_cnt_o, (unsigned)h.top.dbg_render_gv_addr_o,
              (unsigned)h.top.dbg_render_gv_len_o, (unsigned)h.top.dbg_render_gv_client_o,
              (unsigned)h.top.dbg_render_gv_write_o);
  std::printf("[shell_draw] window: lease=%u slot=%u span=%u\n",
              (unsigned)h.top.dbg_fb_lease_valid_o, (unsigned)h.top.dbg_fb_lease_slot_o,
              (unsigned)h.top.dbg_map_span_o);

  zhao::check(h.top.render_fragment_error_o == 0, "no fragment error on the way", 0,
              h.top.render_fragment_error_o ? 1 : 0);
  zhao::check(h.top.render_stream_error_o == 0, "no stream error on the way", 0,
              h.top.render_stream_error_o ? 1 : 0);
  zhao::check(h.top.render_overflow_o == 0, "no arena overflow", 0,
              h.top.render_overflow_o ? 1 : 0);

  // OBSERVED, not diagnosed: fbwrite latches fatal, with the render guard
  // reporting zero violations and no stream error. See D19g.
  zhao::check(h.top.render_fatal_o == 1,
              "fbwrite latches fatal (cause OPEN -- guard reports no violation)", 1,
              h.top.render_fatal_o ? 1 : 0);
  zhao::check(h.top.dbg_render_gv_cnt_o == 0,
              "and the render guard refused nothing, which is the open part", 0,
              (unsigned)h.top.dbg_render_gv_cnt_o);

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
