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

// Draw the triangle once and return the framebuffer words.
std::vector<uint16_t> draw_once(bool setup_mode) {
  ShellHarness h;
  h.reset();
  h.top.fb_writer_i = 1;  // the renderer owns the lease, not the blit

  h.top.render_fb_base_i = kFbBase;
  h.top.render_fb_stride_i = kFbStride;
  h.top.render_fill_word_i = 0xA5A5A5A5A5A5A5A5ull;
  h.top.render_clear_word_i = 0x5A5A5A5A5A5A5A5Aull;

  zhao_geom::BinTri b;
  if (!the_triangle(kGridW, kGridH, &b)) {
    std::printf("the oracle refused the triangle\n");
    return std::vector<uint16_t>();
  }

  // THE BOUNDARY. In setup mode the coefficients are NOT supplied; the bench
  // hands over vertices and the composed GEOM.SETUP computes them. `area2` is
  // an input to SETUP in both cases -- it comes from the binner, upstream of
  // the boundary this step moves.
  h.top.setup_mode_i = setup_mode ? 1 : 0;
  h.top.setup_area2_i = static_cast<int64_t>(b.s.area2);

  ShellHarness::RenderTri t = from_bin_tri(b);
  if (setup_mode) {
    // Deliberately CLEARED, so a shell that ignored setup_mode and used these
    // would draw nothing and the comparison would fail loudly rather than pass
    // by accident. This is the difference between testing the new path and
    // merely running with it enabled.
    for (int e = 0; e < 3; ++e) {
      t.kx[e] = 0;
      t.ky[e] = 0;
      t.kc[e] = 0;
    }
    t.tl = 0;
  }

  h.render_frame_begin(kGridW, kGridH);
  h.render_offer(t);
  h.render_frame_end();
  for (int i = 0; i < 4000; ++i) h.step();

  std::vector<uint16_t> fb(kWords);
  for (uint32_t w = 0; w < kWords; ++w) fb[w] = peek(h, w);
  return fb;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  const std::vector<uint16_t> pre = draw_once(/*setup_mode=*/false);
  const std::vector<uint16_t> via = draw_once(/*setup_mode=*/true);

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
