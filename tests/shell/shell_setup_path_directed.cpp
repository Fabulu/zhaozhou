// shell_setup_path_directed.cpp — D22 step 1's evidence.
// Authored 2026-09-05 (roadmap G3).
//
// ---------------------------------------------------------------------------
// WHAT THIS PROVES, AND WHY THE OBVIOUS TEST WOULD NOT
// ---------------------------------------------------------------------------
// D22's staircase begins by moving the shell's input boundary backwards one
// block: from PRECOMPUTED EDGE EQUATIONS to VERTICES, with `zhao_geom_setup`
// computing the coefficients inside the composed design.
//
// The roadmap is explicit about the trap here:
//
//   > some earlier proposed "unchanged picture" evidence came from pure C++
//   > reference tests, while the old shell bench held its render input
//   > inactive. Those cannot prove the newly connected RTL renderer. Each
//   > integration step needs a composed test that ACTUALLY DRAWS THROUGH THE
//   > ADDED HARDWARE.
//
// So this does not compare the shell against an oracle. It draws THE SAME
// TRIANGLE TWICE THROUGH THE SAME SHELL -- once with the coefficients supplied
// from outside, once with only the vertices supplied and GEOM.SETUP computing
// them inside -- and requires the two framebuffers to be identical, byte for
// byte.
//
// A failure means the composed SETUP disagrees with the coefficients the
// console has been fed all along. A pass means the boundary moved and the
// picture did not.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_shell.h"

// Oracle-only mode: the CLIP+SETUP views without the Verilated DUTs geom_dev.hpp
// otherwise pulls in. The same defines shell_draw_directed uses -- copied rather
// than rediscovered, because without them BinTri is not declared.
#define ZHAO_GEOM_DEV_ORACLE_ONLY
#define ZHAO_GEOM_DEV_BINNER

#include "geom_dev.hpp"
#include "shell_harness.hpp"
#include "zref/zref_render.hpp"
#include "zhao_sim.hpp"

using zhao_shell::ShellHarness;

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

uint16_t peek(ShellHarness& h, uint32_t waddr) {
  h.top.peek_en = 1;
  h.top.peek_waddr = waddr;
  h.top.eval();
  const uint16_t d = h.top.peek_data;
  h.top.peek_en = 0;
  h.top.eval();
  return d;
}

constexpr int32_t px(int32_t whole) { return whole << 8; }  // S 12.8 subpixels

// The same shape shell_draw_directed builds, so a difference here is about the
// boundary move and nothing else. make_bin_tri returns a BOOL and fills an out
// parameter -- copied from the existing test rather than guessed at.
bool the_triangle(int grid_w, int grid_h, zhao_geom::BinTri* out) {
  zref::Clip::Viewport vp;
  vp.w = grid_w * 16;
  vp.h = grid_h * 16;
  return zhao_geom::make_bin_tri(px(4), px(4), px(grid_w * 16 - 4), px(8), px(8),
                                 px(grid_h * 16 - 4), vp, 0x2A2A, out);
}

ShellHarness::RenderTri from_bin_tri(const zhao_geom::BinTri& b) {
  ShellHarness::RenderTri t;
  for (int e = 0; e < 3; ++e) {
    t.kx[e] = b.s.e[e].kx;
    t.ky[e] = b.s.e[e].ky;
    t.kc[e] = b.s.e[e].kc;
  }
  t.tl = (uint8_t)((b.s.e[0].tl ? 1u : 0u) | (b.s.e[1].tl ? 2u : 0u) |
                   (b.s.e[2].tl ? 4u : 0u));
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

constexpr uint32_t kFbBase = 0;
constexpr uint32_t kFbStride = 64 * 2;   // bytes per row
constexpr uint8_t kGridW = 4, kGridH = 4;
constexpr uint32_t kWords = 64 * 64;     // words peeked back

// Draw the triangle once and return the slot's halfwords.
//
// The preconditions below are NOT incidental. An earlier version of this file
// omitted them and drew nothing at all -- which the anti-vacuity check caught,
// and which would otherwise have compared two blank framebuffers and "passed".
// They are taken from shell_draw_directed, which paid for each one:
//   * the SDRAM model must reach init_done, or "nothing landed" is a startup
//     artefact rather than a wiring fault;
//   * fb_base and fb_stride must be set, or every tile row resolves to the same
//     address (D19h);
//   * distinctive fill/clear words, or a correct render writes zeros into
//     memory that is already zero and success is indistinguishable from a dead
//     path;
//   * and a DISPLAY BLIT must grant a framebuffer lease (D19f) -- FBWRITE may
//     only write while one is live, and a mode packet alone grants nothing.
std::vector<uint16_t> draw_once(bool setup_mode, bool* drew_ok) {
  constexpr uint32_t kSlotHalfwords = 245760u / 2u;
  *drew_ok = false;

  ShellHarness h;
  h.reset();
  h.top.fb_writer_i = 1;  // the renderer owns the lease, not the blit
  h.step();

  bool inited = false;
  for (int i = 0; i < 200000 && !inited; ++i) {
    h.step();
    inited = h.top.init_done_o != 0;
  }
  if (!inited) return std::vector<uint16_t>(kSlotHalfwords, 0);

  h.top.render_fb_base_i = 0;
  h.top.render_fb_stride_i = kGridW * 16 * 2;
  h.top.render_fill_word_i = 0xA5A5A5A5A5A5A5A5ull;
  h.top.render_clear_word_i = 0x5A5A5A5A5A5A5A5Aull;
  h.top.render_src_a_i = 0xFF;
  h.top.render_texel_rgb_i = 0xFF00FF;
  h.top.render_texel_a_i = 0xFF;

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
    if (!h.publish(0, zhao_shell::build_packet(ps)))
      return std::vector<uint16_t>(kSlotHalfwords, 0);
  }

  int lease_opens = 0;
  bool was = false;
  for (int i = 0; i < 3000000; ++i) {
    h.step();
    const bool now = h.top.dbg_fb_lease_valid_o != 0;
    if (now && !was) ++lease_opens;
    was = now;
    if (lease_opens > 0 && !now) break;
  }
  if (lease_opens == 0) return std::vector<uint16_t>(kSlotHalfwords, 0);

  zhao_geom::BinTri b;
  if (!the_triangle(kGridW, kGridH, &b))
    return std::vector<uint16_t>(kSlotHalfwords, 0);

  // THE BOUNDARY. In setup mode the coefficients are not supplied; the bench
  // hands over vertices and the composed GEOM.SETUP computes them.
  h.top.setup_mode_i = setup_mode ? 1 : 0;
  h.top.setup_area2_i = (int64_t)b.s.area2;

  ShellHarness::RenderTri t = from_bin_tri(b);
  if (setup_mode) {
    // Deliberately CLEARED, so a shell that ignored setup_mode would draw
    // nothing and this would fail loudly rather than pass by accident.
    for (int e = 0; e < 3; ++e) {
      t.kx[e] = 0;
      t.ky[e] = 0;
      t.kc[e] = 0;
    }
    t.tl = 0;
  }

  h.render_frame_begin(kGridW, kGridH);
  const bool took = h.render_offer(t);

  // Wait for the shell to actually TAKE SETUP's output, rather than guessing a
  // cycle count. A fixed 40-cycle drain was far too short: the trace showed
  // su_valid rising at cycle 2 and holding, with the shell's ready low for
  // every one of those cycles -- and render_offer, which works in mode 0,
  // waits up to 200,000. "It did not happen in 40 cycles" is not the same
  // fact as "it does not happen".
  if (setup_mode) {
    bool consumed = false;
    for (int i = 0; i < 200000 && !consumed; ++i) {
      h.top.eval();
      if (h.top.dbg_su_out_valid_o && h.top.dbg_shell_tri_ready_o) consumed = true;
      h.step();
    }
    std::printf("    [setup] shell consumed SETUP's triangle: %d\n",
                consumed ? 1 : 0);
  }

  h.render_frame_end();
  for (int i = 0; i < 200000; ++i) h.step();

  std::vector<uint16_t> fb(kSlotHalfwords);
  for (uint32_t w = 0; w < kSlotHalfwords; ++w) fb[w] = peek(h, w);
  *drew_ok = took;
  return fb;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  bool pre_ok = false, via_ok = false;
  const std::vector<uint16_t> pre = draw_once(/*setup_mode=*/false, &pre_ok);
  const std::vector<uint16_t> via = draw_once(/*setup_mode=*/true, &via_ok);
  check(pre_ok, "the precomputed pass's triangle was ACCEPTED", 1, pre_ok ? 1 : 0);
  check(via_ok, "the setup pass's triangle was ACCEPTED", 1, via_ok ? 1 : 0);

  // Anti-vacuity FIRST. If the precomputed pass drew nothing, two identical
  // blank framebuffers would "match" and prove nothing at all -- which is
  // exactly the failure the roadmap warns about.
  int nonzero_pre = 0;
  for (uint16_t w : pre)
    if (w != 0) ++nonzero_pre;
  check(nonzero_pre > 0,
        "the PRECOMPUTED pass actually drew something -- otherwise this test "
        "compares two blank frames and proves nothing",
        1, nonzero_pre > 0 ? 1 : 0);

  int nonzero_via = 0;
  for (uint16_t w : via)
    if (w != 0) ++nonzero_via;
  check(nonzero_via > 0, "and so did the SETUP pass", 1, nonzero_via > 0 ? 1 : 0);

  // The comparison.
  int diff = 0;
  int first = -1;
  for (std::size_t i = 0; i < pre.size(); ++i) {
    if (pre[i] != via[i]) {
      ++diff;
      if (first < 0) first = static_cast<int>(i);
    }
  }
  check(diff == 0,
        "the same triangle drawn through GEOM.SETUP produces an IDENTICAL "
        "framebuffer",
        0, diff);
  if (diff) std::printf("  first differing word: %d\n", first);

  std::printf("[shell_setup_path_directed] %d non-zero words drawn; %d checks %s\n",
              nonzero_pre, g_checks, g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
