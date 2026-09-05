// shell_depth_path_directed.cpp — D22 step 2's evidence.
// Authored 2026-09-05 (roadmap G3).
//
// ---------------------------------------------------------------------------
// WHAT THIS STEP ACTUALLY IS
// ---------------------------------------------------------------------------
// Step 1 moved the shell's input boundary back from PRECOMPUTED EDGE EQUATIONS
// to VERTICES, with GEOM.SETUP computing the coefficients inside the composed
// design. Step 2 moves it again, one block further back, for DEPTH:
// `zhao_geom_depthquant` turns `w` into the canonical 24-bit `invw24` the
// fragment pipe tests against.
//
// FINDING THE DEPTH INPUT TOOK THREE WRONG ANSWERS, recorded in docket D19s
// because every one of them was simpler than the truth:
//
//   1. "The shell has no depth path"           -- from grepping two files about
//                                                 a four-level hierarchy.
//   2. "The path exists, the value can't get in" -- from reading the port list
//                                                 without reading what the
//                                                 ports carry.
//   3. The truth, `zhao_raster_tile_pipe.sv:446`:
//
//          assign frag_depth = fill_r[31:8];
//
// `render_fill_word_i` is not a colour. It is a flat-fragment record --
// [63:40] vertex RGB, [39:32] effect tag, [31:8] the 24-bit invw24 depth,
// [7:0] stencil reference -- and no port-name search finds a depth field
// packed inside a word named "fill".
//
// ---------------------------------------------------------------------------
// WHAT THIS TEST PROVES, AND WHAT IT DELIBERATELY DOES NOT
// ---------------------------------------------------------------------------
// It proves the COMPOSED DEPTH PATH: that DEPTHQUANT's answer reaches the
// fragment pipe unchanged and produces the identical picture the bench's
// hand-packed value produced.
//
// It does NOT re-prove DEPTHQUANT's arithmetic -- `geom_depthquant_directed`
// covers that against `zref::depth_of_raw` across the profile table.
//
// THE COMPARISON IS NOT TAUTOLOGICAL, and the difference matters. The obvious
// cheap version would read `dbg_dq_invw24_o` out of the DUT and feed that same
// value back as the "precomputed" one; the two passes would then agree no
// matter what DEPTHQUANT computed, including garbage. So the precomputed pass
// uses **`zref::depth_of_raw`**, computed in C++ from the same `w`. If
// DEPTHQUANT disagrees with the oracle by a single LSB the two framebuffers
// differ and this fails.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_shell.h"

#define ZHAO_GEOM_DEV_ORACLE_ONLY
#define ZHAO_GEOM_DEV_BINNER

#include "geom_dev.hpp"
#include "shell_harness.hpp"
#include "zref/generated/zref_depth.hpp"
#include "zref/zref_depth.hpp"
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

constexpr int32_t px(int32_t whole) { return whole << 8; }

constexpr uint8_t kGridW = 4, kGridH = 4;
constexpr uint32_t kProfile = 0;

// A w well inside the profile's [wmin, wmax] so the run exercises the ordinary
// path rather than a clamp. The clamps have their own checks in
// geom_depthquant_directed; a boundary-move test that only ever hit a clamp
// would agree for the wrong reason.
constexpr uint64_t kW = 4ull << 16;  // 4 m in fx16 raw

// The tile's clear depth, chosen to sit BELOW the depth `kW` produces
// (0x400000) so the correct fragment passes the strict `d_new > d_old` test,
// and ABOVE the wrong depth the sensitivity probe uses so that one fails.
// Both halves matter: a clear depth above the correct value would make the
// correct pass draw nothing, and two blank frames match.
constexpr uint32_t kClearDepth = 0x300000u;
constexpr uint32_t kWrongDepth = 0x100000u;

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

// Draw once. In depth mode the fill word's [31:8] is IGNORED by the bench and
// GEOM.DEPTHQUANT drives it from `kW`; otherwise the bench packs `invw24`.
std::vector<uint16_t> draw_once(bool depth_mode, uint32_t invw24, bool* drew_ok,
                                uint32_t* dq_seen) {
  constexpr uint32_t kSlotHalfwords = 245760u / 2u;
  *drew_ok = false;
  if (dq_seen) *dq_seen = 0;

  ShellHarness h;
  h.reset();
  h.top.fb_writer_i = 1;
  h.step();

  bool inited = false;
  for (int i = 0; i < 200000 && !inited; ++i) {
    h.step();
    inited = h.top.init_done_o != 0;
  }
  if (!inited) return std::vector<uint16_t>(kSlotHalfwords, 0);

  h.top.render_fb_base_i = 0;
  h.top.render_fb_stride_i = kGridW * 16 * 2;

  // THE FLAT-FRAGMENT RECORD, packed field by field rather than as a magic
  // constant: [63:40] vertex RGB, [39:32] effect tag, [31:8] invw24 depth,
  // [7:0] stencil reference. In depth mode the depth field is deliberately
  // filled with a WRONG value (all ones) so that a shell which ignored
  // `depth_mode_i` would draw a visibly different picture rather than passing
  // because the two happened to agree.
  const uint32_t depth_field = depth_mode ? kWrongDepth : (invw24 & 0xFFFFFFu);
  h.top.render_fill_word_i =
      ((uint64_t)0xA5A5A5u << 40) | ((uint64_t)0x11u << 32) | ((uint64_t)depth_field << 8) | 0x00u;
  // DEPTH TESTING IS ON, and that is what makes the picture evidence real.
  //
  // The first version of this test left the render state at 0 -- "depth test
  // off, depth written, blend REPLACE", zhao_raster_fragment's own description
  // of the zero state -- and a sensitivity probe then measured that a
  // deliberately wrong depth changed ZERO framebuffer words. The identical
  // framebuffers were true and empty.
  //
  // zhao_raster_fragment bit [0] is Z_TEST_EN and the test is the qformats §8
  // one, quoted in its header: "pass <=> d_new > d_old (strict; ties fail)".
  // So the tile is cleared to a depth BELOW the fragment's: the correct depth
  // passes and draws, and a wrong depth below the clear fails and draws
  // nothing. The comparison can now fail.
  h.top.render_state_i = 0x1;  // Z_TEST_EN
  h.top.render_clear_word_i = ((uint64_t)0x5A5A5Au << 40) | ((uint64_t)kClearDepth << 8);
  h.top.render_src_a_i = 0xFF;
  h.top.render_texel_rgb_i = 0xFF00FF;
  h.top.render_texel_a_i = 0xFF;

  h.top.depth_mode_i = depth_mode ? 1 : 0;
  h.top.depth_w_i = kW;
  h.top.depth_profile_i = kProfile;

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
  if (!the_triangle(&b)) return std::vector<uint16_t>(kSlotHalfwords, 0);

  h.render_frame_begin(kGridW, kGridH);

  // In depth mode DEPTHQUANT must produce its answer BEFORE the triangle is
  // offered, because the fill word has to hold still for the whole triangle.
  // The bench latches `dq_invw24_r` on `d_valid`; this waits for that latch
  // rather than assuming a cycle count -- the fixed-drain mistake that cost a
  // whole diagnosis in step 1.
  if (depth_mode) {
    h.top.render_tri_valid_i = 1;
    bool got = false;
    for (int i = 0; i < 200000 && !got; ++i) {
      h.top.eval();
      if (h.top.dbg_dq_valid_o) got = true;
      h.step();
    }
    h.top.render_tri_valid_i = 0;
    h.top.eval();
    if (dq_seen) *dq_seen = h.top.dbg_dq_invw24_o;
    std::printf("    [depth] DEPTHQUANT produced invw24 = 0x%06X (%s)\n", h.top.dbg_dq_invw24_o,
                got ? "valid" : "TIMED OUT");
  }

  const ShellHarness::RenderTri t = from_bin_tri(b);
  const bool took = h.render_offer(t);
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

  // THE ORACLE, not the DUT's own answer. See the header: reading invw24 out of
  // the DUT and feeding it back would make the two passes agree whatever
  // DEPTHQUANT computed.
  const uint32_t want_invw24 = zref::depth_of_raw(kW, zref::gen::DEPTH_PROFILES[kProfile]);
  std::printf("  zref::depth_of_raw(w=0x%llX, profile %u) = 0x%06X\n", (unsigned long long)kW,
              kProfile, want_invw24);

  bool pre_ok = false, via_ok = false;
  uint32_t dq_seen = 0;
  const std::vector<uint16_t> pre = draw_once(/*depth_mode=*/false, want_invw24, &pre_ok, nullptr);
  const std::vector<uint16_t> via = draw_once(/*depth_mode=*/true, 0, &via_ok, &dq_seen);

  check(pre_ok, "the precomputed pass's triangle was ACCEPTED", 1, pre_ok);
  check(via_ok, "the depth pass's triangle was ACCEPTED", 1, via_ok);

  check(dq_seen == want_invw24,
        "GEOM.DEPTHQUANT's invw24 equals zref::depth_of_raw -- the composed "
        "block agrees with the ratified law, not merely with itself",
        want_invw24, dq_seen);

  // Anti-vacuity FIRST: two blank framebuffers match and prove nothing.
  int nonzero_pre = 0;
  for (uint16_t w : pre)
    if (w != 0) ++nonzero_pre;
  check(nonzero_pre > 0,
        "the PRECOMPUTED pass actually drew something -- otherwise this "
        "compares two blank frames",
        1, nonzero_pre > 0 ? 1 : 0);

  int nonzero_via = 0;
  for (uint16_t w : via)
    if (w != 0) ++nonzero_via;
  check(nonzero_via > 0, "and so did the DEPTH pass", 1, nonzero_via > 0 ? 1 : 0);

  // ---- IS THE FRAMEBUFFER COMPARISON EVEN CAPABLE OF FAILING? -------------
  // The picture is a COLOUR buffer. If the render state has depth testing off
  // -- "the plain opaque write (depth test off, depth written, blend REPLACE)"
  // is zhao_raster_tile_pipe's own description of the default -- then the depth
  // VALUE cannot change a single colour byte, and comparing two framebuffers
  // would agree no matter what depth either pass used.
  //
  // So this draws a third time with a deliberately WRONG depth and asks whether
  // the picture moved. Whatever the answer, it is reported rather than assumed.
  bool third_ok = false;
  const std::vector<uint16_t> wrongdepth =
      draw_once(/*depth_mode=*/false, kWrongDepth, &third_ok, nullptr);
  int wrong_diff = 0;
  for (std::size_t i = 0; i < pre.size(); ++i)
    if (pre[i] != wrongdepth[i]) ++wrong_diff;
  std::printf(
      "  sensitivity: a deliberately wrong depth changes %d framebuffer words\n"
      "  -> the framebuffer comparison %s evidence about the depth value\n",
      wrong_diff, wrong_diff ? "IS" : "is NOT (depth test is off; see check below)");

  int diff = 0, first = -1;
  for (std::size_t i = 0; i < pre.size(); ++i)
    if (pre[i] != via[i]) {
      ++diff;
      if (first < 0) first = static_cast<int>(i);
    }
  check(diff == 0,
        "the same triangle drawn with GEOM.DEPTHQUANT supplying the depth "
        "produces an IDENTICAL framebuffer",
        0, diff);

  // The honest conclusion, stated as a check so it cannot be skimmed past. If
  // a wrong depth changes nothing, then the identical-framebuffer result above
  // is TRUE BUT EMPTY, and the only real evidence in this file is that
  // DEPTHQUANT's value equals the oracle's and reaches the pipe.
  if (wrong_diff == 0) {
    std::printf(
        "  NOTE: with this render state the colour buffer is insensitive to\n"
        "        depth, so the framebuffer match is necessary but not\n"
        "        sufficient. The load-bearing check is the invw24 comparison\n"
        "        against zref::depth_of_raw, which a wrong DEPTHQUANT fails.\n");
  } else {
    check(wrong_diff > 0,
          "and the comparison is SENSITIVE to depth -- a wrong value moves the "
          "picture, so the match above is real evidence",
          1, wrong_diff > 0 ? 1 : 0);
  }
  if (diff) std::printf("  first differing word: %d\n", first);

  std::printf("[shell_depth_path_directed] %d non-zero words drawn; %d checks %s\n", nonzero_pre,
              g_checks, g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
