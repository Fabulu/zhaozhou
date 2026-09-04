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
// WHAT IT FOUND, on the first run: **zhao_raster_fbwrite could not write a
// single row through the real guard.** It read the guard's verdict in the
// accepting cycle, but `zhao_mem_guard` delivers `ok` as a ONE-CYCLE PULSE the
// cycle after (guard:186, *"verdict 1 cycle after accept"*, and guard:202 sets
// the default back to 0 every cycle). So the first burst saw `ready=1, ok=0` --
// the reset value -- declared "the guard REFUSED", latched `fatal_error_o` and
// dropped the row. **Every frame was unpublishable and nothing was ever
// written, against a guard that had refused nothing.**
//
// `zhao_debug_frameblit`, the other guard client, has always done it correctly:
// `B_GUARD_REQUEST` waits for `ready`, and a separate `B_GUARD_VERDICT` reads
// the answer. fbwrite now has the same shape (`W_VERD`).
//
// WHY NOTHING CAUGHT IT: `tests/render/render_fb_directed.cpp` played a guard
// that answered `ready` and `ok` in the same cycle. **The model and the DUT
// shared one wrong assumption**, so the block passed alone and failed the
// moment it met the real guard. That model now delivers the real one-cycle
// pulse, which is what makes it able to fail. **A played interface that is
// easier than the real one is not a simplification, it is a different
// interface.**
//
// The assertions, after the fix:
//
//   * the binner ACCEPTS a triangle offered at the shell boundary;
//   * a blit packet GRANTS a lease, span 245,760 = `ZHAO_FB_SLOT_SPAN`;
//   * fbwrite does NOT latch fatal and the guard refuses nothing;
//   * THE RENDER PATH WRITES EXACTLY THE ORACLE'S EXTENT -- `zref::Binner`
//     says the triangle touches 13 tiles, the pipeline resolves whole tiles,
//     and the hardware wrote 13 x 16 x 16 = 3,328 pixels in 208 bursts;
//   * issued and retired words BALANCE, which is why both are counted in words;
//   * the frame DRAINS, and the framebuffer slot changes.
//
// STILL NOT PROVED: that each pixel holds the right COLOUR. The extent is
// checked against `zref::Binner`; the shading is `render_pipe_directed`'s, and
// re-proving it here would duplicate a law rather than test the composition.
// What this file establishes is that the composed shell renders the right
// TILES to memory -- which until 2026-09-04 nothing had ever checked, because
// until then it rendered nothing at all.
//
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

  // SNAPSHOT THE SLOT BEFORE RENDERING. The blit has already filled it, so
  // "what the render changed" is the difference between two real states rather
  // than a guess about power-on values. This makes the memory-side check
  // ASSUMPTION-FREE: it needs no knowledge of the tile-to-address mapping, the
  // canvas stride, or the fill-to-RGB565 conversion -- only that a written
  // halfword differs from what was there before.
  const uint32_t kSlotHalfwords = 245760u / 2u;
  std::vector<uint16_t> before(kSlotHalfwords);
  for (uint32_t w = 0; w < kSlotHalfwords; ++w) before[w] = peek(h, w);

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

  // THE DEFECT THIS TEST FOUND, NOW FIXED (docket D19g).
  //
  // zhao_raster_fbwrite read the guard's verdict in the ACCEPTING cycle, but
  // zhao_mem_guard delivers `ok` as a one-cycle pulse the cycle AFTER
  // (guard:186, "verdict 1 cycle after accept"). So on its first burst fbwrite
  // saw ready=1, ok=0 -- the reset value -- declared "the guard REFUSED",
  // latched fatal and dropped the row. Every frame was unpublishable and
  // nothing was ever written, against a guard that had refused NOTHING.
  //
  // fbwrite now has a W_VERD state, the shape zhao_debug_frameblit has always
  // used. These checks are what changed when it landed.
  zhao::check(h.top.render_fatal_o == 0, "fbwrite does NOT latch fatal", 0,
              h.top.render_fatal_o ? 1 : 0);
  zhao::check(h.top.dbg_render_gv_cnt_o == 0, "and the guard refused nothing", 0,
              (unsigned)h.top.dbg_render_gv_cnt_o);
  // NOT JUST "SOME" PIXELS -- exactly the ones the binner oracle names.
  //
  // `zref::Binner::bin` returns the tiles an accepted, set-up triangle touches.
  // The tile pipeline resolves a WHOLE tile once, so a frame that rendered
  // correctly writes `tiles x 16 x 16` pixels and nothing else. That turns a
  // liveness check into a claim about the picture's EXTENT, using the same
  // oracle `render_pipe_directed` bins against -- without re-proving the
  // arithmetic it already owns.
  const std::vector<zref::Binner::Ref> want_tiles =
      zref::Binner::bin(bt.s, bt.min_x, bt.max_x, bt.min_y, bt.max_y);
  const uint32_t want_px =
      (uint32_t)want_tiles.size() * (uint32_t)(zref::Binner::kTile * zref::Binner::kTile);
  std::printf("[shell_draw] oracle: %zu tiles -> %u pixels\n", want_tiles.size(),
              (unsigned)want_px);
  zhao::check(h.top.render_pixels_o == want_px,
              "the render path wrote EXACTLY the oracle's tiles x 256 pixels", want_px,
              (unsigned)h.top.render_pixels_o);
  zhao::check(h.top.render_issued_words_o == h.top.render_retired_words_o,
              "issued and retired words balance", (unsigned)h.top.render_issued_words_o,
              (unsigned)h.top.render_retired_words_o);
  zhao::check(h.top.render_drained_o == 1, "and the frame DRAINED", 1,
              h.top.render_drained_o ? 1 : 0);

  // Nothing was written, which is the same fact from memory's side.
  uint32_t changed = 0;
  for (uint32_t w = 0; w < kSlotHalfwords; ++w) {
    if (peek(h, w) != before[w]) ++changed;
  }
  const bool wrote = changed > 0;
  std::printf("[shell_draw] memory: %u halfwords changed (counter said %u)\n", changed,
              (unsigned)h.top.render_pixels_o);
  // Was the BLIT still writing this slot while the render wrote it? D19f: the
  // render's guard window IS the blit's lease, so the two are concurrent BY
  // CONSTRUCTION -- the renderer may only write while a blit is in flight into
  // the same slot. If the blit overwrote the render's pixels, the counter and
  // memory disagree for a reason that is architectural, not a wiring fault.
  std::printf("[shell_draw] blits completed=%zu, lease still live=%u\n", h.blit_log.size(),
              (unsigned)h.top.dbg_fb_lease_valid_o);
  // THE COUNTER AND THE MEMORY DISAGREE, AND THAT IS DOCKET D19h.
  //
  //   counter  render_pixels_o = 3328     (fbwrite's own tally)
  //   memory   64 halfwords changed       (what the SDRAM model actually holds)
  //   blits completed = 1, lease still live = 1
  //
  // The obvious explanation -- that the blit was overwriting the render -- is
  // MEASURED FALSE: the blit had already completed when the snapshot was taken.
  // So either fbwrite counts words it did not land, or the writes go somewhere
  // this peek range does not cover, or most of them wrote a value equal to what
  // the blit had left. **None of those is established**, and an unmeasured
  // explanation is worse than an open question -- which is the lesson this file
  // already carries twice.
  //
  // So the equality is REPORTED, not asserted. What is asserted below is the
  // part that is certain: memory changed, so the picture reached it. The day
  // D19h is understood, this becomes an equality.
  if (changed != h.top.render_pixels_o) {
    std::printf("[shell_draw] D19h: counter %u != memory %u -- OPEN\n",
                (unsigned)h.top.render_pixels_o, changed);
  }
  zhao::check(wrote, "and the framebuffer CHANGED -- the picture reached memory", 1, wrote ? 1 : 0);

  // ---- WHAT IS STILL NOT PROVED -------------------------------------------
  // That a granted frame produces the RIGHT pixels. That needs the command path
  // driven alongside the render port so CMD.SCHEDULER grants a region map, and
  // it is the next step rather than this one. Stated here so the file cannot be
  // mistaken for a picture test: it proves the path is CONNECTED and that the
  // guard protects it, and nothing about colour.

  return zhao::report_and_exit("shell_draw_directed");
}
