// shell_project_path_directed.cpp — D22 step 4's evidence.
// Authored 2026-09-05 (roadmap G3).
//
// ---------------------------------------------------------------------------
// THE STAIRCASE, COMPLETE TO HERE
// ---------------------------------------------------------------------------
//   step 1  precomputed edge coefficients  ->  GEOM.SETUP
//   step 2  precomputed invw24 depth       ->  GEOM.DEPTHQUANT
//   step 3  precomputed 2A and scan box    ->  GEOM.CLIP
//   step 4  precomputed SCREEN VERTICES    ->  GEOM.PROJECT
//
// Step 4 is the one that joins the front end up, because PROJECT feeds BOTH
// downstream blocks. Its own header is explicit about the second, and about
// why the obvious shortcut is wrong:
//
//   > clip.w itself, fx16 raw. GEOM.DEPTHQUANT consumes THIS and not out_d_o:
//   > the ratified depth law performs its own rcp_u24 on w, and the quotient
//   > has already lost the precision reconstruction would need.
//
// So in project mode the bench supplies CLIP-SPACE vertices and nothing else
// geometric: PROJECT produces the screen x/y CLIP consumes, the behind flags
// CLIP tests, and the w DEPTHQUANT quantises.
//
// ---------------------------------------------------------------------------
// THE COMPARISON IS AGAINST THE ORACLE, NOT AGAINST THE DUT
// ---------------------------------------------------------------------------
// The screen vertices fed to the precomputed pass come from
// `zref::render::project_vertex` -- the same oracle `geom_project_directed`
// uses -- computed in C++ from the same clip-space input and the same matrix.
// Reading `dbg_proj_ax_o` out of the DUT and feeding it back would make both
// passes agree whatever PROJECT computed, which is the shape of a test that
// cannot fail.
//
// ---------------------------------------------------------------------------
// AND THE SENSITIVITY IS MEASURED, because step 2 was insensitive
// ---------------------------------------------------------------------------
// Step 2's framebuffer comparison could not see the value it was testing until
// depth testing was switched on. Since then every step measures whether its
// own comparison is capable of failing rather than assuming it. Here a wrong
// projection moves the vertices, so it should be sensitive by construction --
// which is exactly the kind of "obviously fine" that step 2 was.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_shell.h"

#define ZHAO_GEOM_DEV_ORACLE_ONLY
#define ZHAO_GEOM_DEV_BINNER

#include "geom_dev.hpp"
#include "shell_harness.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_render.hpp"
#include "zrender/internal.hpp"

using zhao_shell::ShellHarness;
namespace zr = zref::render;

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

constexpr uint8_t kGridW = 4, kGridH = 4;
constexpr uint32_t kSlotHalfwords = 245760u / 2u;

// Three clip-space vertices, chosen so the projected triangle lands well inside
// the 64x64 canvas and covers a useful number of pixels. A triangle that
// projects to a sliver would compare two nearly-empty framebuffers.
struct ClipVtx {
  int32_t x, y, z;
};
const ClipVtx kTri[3] = {
    {-(1 << 15), -(1 << 15), 1 << 16},  // -0.5, -0.5, 1.0 in fx16
    {(3 << 14), -(1 << 14), 1 << 16},   // +0.75, -0.25
    {-(1 << 14), (3 << 14), 1 << 16},   // -0.25, +0.75
};

// The projection: a plain scale with w = 1, so the perspective divide is exact
// and the test is about the composed PATH rather than about rounding. The
// matrix is IDENTITY-scaled rather than a real perspective for the same reason
// step 2 chose a w well inside its clamps -- a step that only ever exercised a
// clamp or a divide edge would agree for the wrong reason.
zref::mat4fx the_matrix() {
  zref::mat4fx m{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) m.m[r][c] = zref::fx16{0};
  m.m[0][0] = zref::fx16{1 << 16};
  m.m[1][1] = zref::fx16{1 << 16};
  m.m[2][2] = zref::fx16{1 << 16};
  m.m[3][3] = zref::fx16{1 << 16};  // w = 1
  return m;
}

zr::Viewport the_viewport() {
  zr::Viewport vp;
  vp.x0 = 0;
  vp.y0 = 0;
  vp.w = kGridW * 16;
  vp.h = kGridH * 16;
  return vp;
}

struct Pass {
  std::vector<uint16_t> fb;
  bool took = false;
  bool ready = false;
  int32_t ax = 0, ay = 0;
  uint32_t w = 0;
  uint8_t behind = 0;
};

void write_cfg(ShellHarness& h, uint8_t addr, uint32_t data) {
  h.top.proj_cfg_we_i = 1;
  h.top.proj_cfg_view_i = 0;
  h.top.proj_cfg_addr_i = addr;
  h.top.proj_cfg_data_i = data;
  h.top.eval();
  h.step();
  h.top.proj_cfg_we_i = 0;
  h.top.eval();
}

// mode 0 = precomputed screen vertices (from the ORACLE)
// mode 1 = project + clip + setup
Pass draw_once(int mode, bool wrong_matrix) {
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

  // ---- configure PROJECT --------------------------------------------------
  zref::mat4fx m = the_matrix();
  if (wrong_matrix) m.m[0][0] = zref::fx16{1 << 15};  // half scale in x
  const zr::Viewport vp = the_viewport();
  for (int rr = 0; rr < 4; ++rr)
    for (int cc = 0; cc < 4; ++cc) write_cfg(h, (uint8_t)(rr * 4 + cc), (uint32_t)m.m[rr][cc].raw);
  write_cfg(h, 16, ((uint32_t)vp.y0 << 16) | ((uint32_t)vp.x0 & 0xFFFFu));
  write_cfg(h, 17, ((uint32_t)vp.h << 16) | ((uint32_t)vp.w & 0xFFFFu));

  {
    std::vector<uint8_t> canvas(zr::kSlotBytes, 0x11);
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

  // ---- THE ORACLE: project the three vertices in C++ ----------------------
  int32_t sx[3], sy[3];
  for (int k = 0; k < 3; ++k) {
    const auto p = zr::project_vertex(m, vp, zref::fx16{kTri[k].x}, zref::fx16{kTri[k].y},
                                      zref::fx16{kTri[k].z}, nullptr);
    sx[k] = p.s.x;
    sy[k] = p.s.y;
  }

  // Build the BinTri from the ORACLE's screen vertices, so the precomputed
  // pass draws what PROJECT is supposed to produce.
  zref::Clip::Viewport cvp;
  cvp.w = kGridW * 16;
  cvp.h = kGridH * 16;
  zhao_geom::BinTri b;
  if (!zhao_geom::make_bin_tri(sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], cvp, 0x2A2A, &b)) return r;

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

  h.top.setup_mode_i = (mode >= 1) ? 1 : 0;
  h.top.clip_mode_i = 0;
  h.top.project_mode_i = (mode >= 1) ? 1 : 0;
  h.top.setup_area2_i = (int64_t)b.s.area2;
  h.top.proj_ax_i = kTri[0].x;
  h.top.proj_ay_i = kTri[0].y;
  h.top.proj_az_i = kTri[0].z;
  h.top.proj_bx_i = kTri[1].x;
  h.top.proj_by_i = kTri[1].y;
  h.top.proj_bz_i = kTri[1].z;
  h.top.proj_cx_i = kTri[2].x;
  h.top.proj_cy_i = kTri[2].y;
  h.top.proj_cz_i = kTri[2].z;

  if (mode >= 1) {
    // Everything geometric CLEARED, as in every earlier step: a shell that
    // ignored project_mode_i draws nothing rather than the bench's triangle.
    for (int e = 0; e < 3; ++e) {
      t.kx[e] = 0;
      t.ky[e] = 0;
      t.kc[e] = 0;
    }
    t.tl = 0;
    t.ax = t.ay = t.bx = t.by = t.cx = t.cy = 0;
  }

  h.render_frame_begin(kGridW, kGridH);
  const bool took = h.render_offer(t);

  if (mode >= 1) {
    // Watch the WHOLE chain, not just PROJECT, and DO NOT BREAK EARLY.
    //
    // The first version broke out the instant `dbg_proj_ready_o` went high and
    // then called render_frame_end(), so the triangle never propagated through
    // CLIP -> SETUP -> raster and the pass drew nothing. PROJECT's numbers were
    // already exactly right; the test was ending the frame before the picture
    // existed.
    //
    // That is step 1's lesson repeated verbatim -- its own comment reads "a
    // fixed 40-cycle drain was far too short ... 'It did not happen in 40
    // cycles' is not the same fact as 'it does not happen'." A loop that stops
    // at the first interesting signal answers only about that signal.
    //
    // So this runs the full window and counts every hop, which also means a
    // future break is LOCATED rather than merely detected.
    int saw_proj = 0, saw_clip = 0, saw_setup = 0, saw_consume = 0;
    for (int i = 0; i < 200000; ++i) {
      h.top.eval();
      if (h.top.dbg_proj_ready_o) {
        if (!r.ready) {
          r.ready = true;
          r.ax = (int32_t)h.top.dbg_proj_ax_o;
          r.ay = (int32_t)h.top.dbg_proj_ay_o;
          r.w = h.top.dbg_proj_w_o;
          r.behind = (uint8_t)h.top.dbg_proj_behind_o;
        }
        ++saw_proj;
      }
      if (h.top.dbg_clip_valid_o) ++saw_clip;
      if (h.top.dbg_su_out_valid_o) ++saw_setup;
      if (h.top.dbg_su_out_valid_o && h.top.dbg_shell_tri_ready_o) ++saw_consume;
      h.step();
    }
    std::printf(
        "    [chain] proj_ready %d | clip_valid %d | setup_valid %d | "
        "shell consumed %d\n",
        saw_proj, saw_clip, saw_setup, saw_consume);
  }

  h.render_frame_end();
  for (int i = 0; i < 200000; ++i) h.step();

  for (uint32_t k = 0; k < kSlotHalfwords; ++k) r.fb[k] = peek(h, k);
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

  const zref::mat4fx m = the_matrix();
  const zr::Viewport vp = the_viewport();
  const auto oa = zr::project_vertex(m, vp, zref::fx16{kTri[0].x}, zref::fx16{kTri[0].y},
                                     zref::fx16{kTri[0].z}, nullptr);
  std::printf("  oracle vertex A -> screen (%d, %d), w = %d, in = %d\n", oa.s.x, oa.s.y, oa.w,
              oa.in ? 1 : 0);

  const Pass pre = draw_once(/*mode=*/0, false);
  const Pass via = draw_once(/*mode=*/1, false);

  check(pre.took, "the precomputed pass's triangle was ACCEPTED", 1, pre.took);
  check(via.ready,
        "GEOM.PROJECT produced all three vertices -- the collector waits for "
        "three, because offering early hands CLIP one new vertex and two stale "
        "ones, which is a wrong triangle that still draws",
        1, via.ready ? 1 : 0);

  std::printf("  project vertex A -> screen (%d, %d), w = %u, behind = %u\n", via.ax, via.ay, via.w,
              via.behind);

  check(via.ax == oa.s.x && via.ay == oa.s.y,
        "and its vertex A matches zref::render::project_vertex exactly -- the "
        "composed block agrees with the ratified law, not merely with itself",
        ((long long)oa.s.x << 32) | (uint32_t)oa.s.y, ((long long)via.ax << 32) | (uint32_t)via.ay);
  check(via.behind == 0, "no vertex is behind for a w = 1 projection", 0, via.behind);

  // `w` is checked too, and it is the reason step 4 exists in this shape:
  // DEPTHQUANT consumes PROJECT's clip.w, so a w that reached the fragment
  // pipe wrong would produce a wrong depth with everything else correct.
  // ProjOut gained this field on 2026-09-04 precisely because the port had no
  // oracle and "the test's expectation was an uninitialised member for a day".
  check((int32_t)via.w == oa.w, "and its w matches the oracle -- the field DEPTHQUANT quantises",
        oa.w, (int32_t)via.w);

  const int nz = nonzero(pre.fb);
  check(nz > 0,
        "the PRECOMPUTED pass actually drew something -- otherwise this "
        "compares two blank frames",
        1, nz > 0 ? 1 : 0);
  check(nonzero(via.fb) > 0, "and so did the PROJECT pass", 1, nonzero(via.fb) > 0 ? 1 : 0);
  check(differing(pre.fb, via.fb) == 0,
        "the same triangle drawn with GEOM.PROJECT supplying the screen "
        "vertices produces an IDENTICAL framebuffer",
        0, differing(pre.fb, via.fb));

  // ---- can the comparison fail? ------------------------------------------
  const Pass bad = draw_once(/*mode=*/1, /*wrong_matrix=*/true);
  const int diff = differing(via.fb, bad.fb);
  std::printf("  sensitivity: halving the matrix's x scale changes %d words\n", diff);
  check(diff > 0,
        "a wrong projection matrix MOVES the picture, so the match above is "
        "real evidence and not an insensitive comparison",
        1, diff > 0 ? 1 : 0);

  std::printf("[shell_project_path_directed] %d non-zero words drawn; %d checks %s\n", nz, g_checks,
              g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
