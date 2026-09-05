// shell_clip_path_directed.cpp — D22 step 3's evidence.
// Authored 2026-09-05 (roadmap G3).
//
// ---------------------------------------------------------------------------
// THE STAIRCASE SO FAR
// ---------------------------------------------------------------------------
//   step 1  precomputed edge coefficients  ->  GEOM.SETUP computes them
//   step 2  precomputed invw24 depth       ->  GEOM.DEPTHQUANT computes it
//   step 3  precomputed 2A and scan box    ->  GEOM.CLIP computes them
//
// Step 3 moves more than two numbers. GEOM.CLIP also NORMALISES WINDING: if a
// triangle has 2A < 0 it swaps B and C so the area comes out positive, and it
// swaps their attributes with them. Its own header explains why that swap must
// live beside the decision:
//
//   > Swapping the positions and not the attributes produces a triangle that is
//   > geometrically correct and shaded wrong, on exactly the back-facing half
//   > of the scene -- so it survives any test whose triangles are all wound one
//   > way.
//
// This test therefore draws BOTH windings. A step-3 test that only ever fed
// counter-clockwise triangles would exercise the flip zero times and pass with
// the whole normalisation path dead, which is precisely the failure that
// sentence is warning about.
//
// ---------------------------------------------------------------------------
// WHY THE EVIDENCE IS STRONGER HERE THAN IN STEP 2
// ---------------------------------------------------------------------------
// Step 2's framebuffer comparison was initially EMPTY: with depth testing off
// the colour buffer cannot see a depth value, so identical framebuffers proved
// nothing until depth testing was turned on. That trap does not exist here --
// 2A and the scan box decide which pixels are covered at all, so a wrong value
// changes the picture by construction. The sensitivity probe below measures
// that rather than asserting it, because step 2 taught that the assumption is
// worth one extra draw.

#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_shell.h"

#define ZHAO_GEOM_DEV_ORACLE_ONLY
#define ZHAO_GEOM_DEV_BINNER

#include "geom_dev.hpp"
#include "shell_harness.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_render.hpp"

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

constexpr int32_t px(int32_t whole) { return whole << 8; }
constexpr uint8_t kGridW = 4, kGridH = 4;
constexpr uint32_t kSlotHalfwords = 245760u / 2u;

// THE ONE TRIANGLE. The clockwise variant is NOT made here.
//
// `make_bin_tri` is the ORACLE, and the oracle normalises winding itself --
// handing it B and C swapped returns the same positively-wound BinTri, so the
// first version of this test fed CLIP two identical triangles, saw flip=0
// twice, and would have reported the normalisation path exercised when it had
// never run. The swap has to happen AFTER the oracle, on the vertices actually
// presented to the hardware.
bool the_triangle(zhao_geom::BinTri* out) {
  zref::Clip::Viewport vp;
  vp.w = kGridW * 16;
  vp.h = kGridH * 16;
  return zhao_geom::make_bin_tri(px(4), px(4), px(kGridW * 16 - 4), px(8), px(8),
                                 px(kGridH * 16 - 4), vp, 0x2A2A, out);
}

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

struct Pass {
  std::vector<uint16_t> fb;
  bool took = false;
  bool flipped = false;
  int64_t area2 = 0;
};

// mode 0 = precomputed (bench supplies 2A and the scan box)
// mode 1 = setup only  (step 1)
// mode 2 = clip + setup (step 3)
Pass draw_once(int mode, bool swap_bc, bool corrupt_box) {
  Pass r;
  r.fb.assign(kSlotHalfwords, 0);

  ShellHarness h;
  h.reset();
  h.top.fb_writer_i = 1;
  h.step();

  bool inited = false;
  for (int i = 0; i < 200000 && !inited; ++i) {
    h.step();
    inited = h.top.init_done_o != 0;
  }
  if (!inited) return r;

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
    ps.mode = 2;
    ps.has_blit = true;
    ps.blit_dst = 0;
    ps.blit_src = arena;
    ps.blit_len = (uint32_t)canvas.size();
    ps.blit_crc = zhao_abi::zhao_crc32c(0, canvas.data(), canvas.size());
    if (!h.publish(0, zhao_shell::build_packet(ps))) return r;
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
  if (lease_opens == 0) return r;

  zhao_geom::BinTri b;
  if (!the_triangle(&b)) return r;

  h.top.setup_mode_i = (mode >= 1) ? 1 : 0;
  h.top.clip_mode_i = (mode == 2) ? 1 : 0;
  h.top.setup_area2_i = (int64_t)b.s.area2;

  ShellHarness::RenderTri t = from_bin_tri(b);
  if (swap_bc) {
    // Present the SAME triangle wound the other way. CLIP must flip it back.
    std::swap(t.bx, t.cx);
    std::swap(t.by, t.cy);
  }
  if (mode >= 1) {
    // Coefficients cleared, as in step 1: a shell ignoring the mode draws
    // nothing rather than quietly drawing the bench's triangle.
    for (int e = 0; e < 3; ++e) {
      t.kx[e] = 0;
      t.ky[e] = 0;
      t.kc[e] = 0;
    }
    t.tl = 0;
  }
  if (corrupt_box) {
    // The sensitivity probe: a scan box that excludes most of the triangle.
    // Only meaningful in the PRECOMPUTED mode, where the bench's box is the
    // one that is used.
    t.min_x = t.max_x;
    t.min_y = t.max_y;
  }

  h.render_frame_begin(kGridW, kGridH);
  const bool took = h.render_offer(t);

  if (mode == 2) {
    bool consumed = false;
    for (int i = 0; i < 200000 && !consumed; ++i) {
      h.top.eval();
      if (h.top.dbg_clip_valid_o) {
        r.flipped = h.top.dbg_clip_flip_o != 0;
        r.area2 = (int64_t)h.top.dbg_clip_area2_o;
      }
      if (h.top.dbg_su_out_valid_o && h.top.dbg_shell_tri_ready_o) consumed = true;
      h.step();
    }
  }

  h.render_frame_end();
  for (int i = 0; i < 200000; ++i) h.step();

  for (uint32_t w = 0; w < kSlotHalfwords; ++w) r.fb[w] = peek(h, w);
  r.took = took;
  return r;
}

int nonzero(const std::vector<uint16_t>& v) {
  int n = 0;
  for (uint16_t w : v)
    if (w != 0) ++n;
  return n;
}

int differing(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b) {
  int n = 0;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i] != b[i]) ++n;
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  const Pass pre = draw_once(/*mode=*/0, /*swap_bc=*/false, false);
  const Pass via = draw_once(/*mode=*/2, /*swap_bc=*/false, false);

  check(pre.took, "the precomputed pass's triangle was ACCEPTED", 1, pre.took);
  check(via.took, "the clip pass's triangle was ACCEPTED", 1, via.took);

  const int nz = nonzero(pre.fb);
  check(nz > 0,
        "the PRECOMPUTED pass actually drew something -- otherwise this "
        "compares two blank frames",
        1, nz > 0 ? 1 : 0);
  check(nonzero(via.fb) > 0, "and so did the CLIP pass", 1, nonzero(via.fb) > 0 ? 1 : 0);

  check(differing(pre.fb, via.fb) == 0,
        "the same triangle drawn with GEOM.CLIP supplying 2A and the scan box "
        "produces an IDENTICAL framebuffer",
        0, differing(pre.fb, via.fb));

  // ---- is the comparison capable of failing? ------------------------------
  // Step 2's framebuffer check turned out to be insensitive to the value it
  // was meant to be testing. That is now measured every time rather than
  // assumed: a wrong scan box must move the picture.
  const Pass badbox = draw_once(/*mode=*/0, /*swap_bc=*/false, true);
  const int box_diff = differing(pre.fb, badbox.fb);
  std::printf("  sensitivity: a collapsed scan box changes %d framebuffer words\n", box_diff);
  check(box_diff > 0,
        "a wrong scan box MOVES the picture, so the match above is real "
        "evidence and not an insensitive comparison",
        1, box_diff > 0 ? 1 : 0);

  // ---- the winding flip, which is the part that is not plumbing -----------
  const Pass cw = draw_once(/*mode=*/2, /*swap_bc=*/true, false);
  std::printf("  clip: 2A = %lld, flip = %d (ccw) | 2A = %lld, flip = %d (cw)\n",
              (long long)via.area2, via.flipped ? 1 : 0, (long long)cw.area2, cw.flipped ? 1 : 0);

  check(cw.flipped,
        "the CLOCKWISE triangle was FLIPPED by GEOM.CLIP -- without this the "
        "normalisation path is never exercised and this whole file passes with "
        "it dead",
        1, cw.flipped ? 1 : 0);
  check(!via.flipped, "and the counter-clockwise one was not", 0, via.flipped ? 1 : 0);
  check(via.area2 > 0 && cw.area2 > 0, "both windings leave CLIP with a POSITIVE 2A", 1,
        (via.area2 > 0 && cw.area2 > 0) ? 1 : 0);
  check(differing(via.fb, cw.fb) == 0,
        "and both windings draw the SAME picture -- the flip is a "
        "normalisation, not a different triangle",
        0, differing(via.fb, cw.fb));

  std::printf("[shell_clip_path_directed] %d non-zero words drawn; %d checks %s\n", nz, g_checks,
              g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
