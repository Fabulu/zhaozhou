// shell_assemble_path_directed.cpp — D22 step 5's evidence.
// Authored 2026-09-05 (roadmap G3).
//
// ---------------------------------------------------------------------------
// STEP 5 CROSSES A DIFFERENT KIND OF LINE
// ---------------------------------------------------------------------------
//   step 1  precomputed edge coefficients  ->  GEOM.SETUP
//   step 2  precomputed invw24 depth       ->  GEOM.DEPTHQUANT
//   step 3  precomputed 2A and scan box    ->  GEOM.CLIP
//   step 4  precomputed screen vertices    ->  GEOM.PROJECT
//   step 5  "here are three vertices"      ->  GEOM.ASSEMBLE names them
//
// Steps 1 to 4 moved per-triangle ARITHMETIC backwards. Step 5 moves the
// triangle's VERTEX SELECTION: the bench hands over a meshlet — a vertex
// count, a triangle count, a material, a raster state — and a u8 local index
// stream, and GEOM.ASSEMBLE walks it three indices at a time and emits one
// TriangleDescriptor per triangle carrying VERTEX IDS.
//
// ---------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT MOVED
// ---------------------------------------------------------------------------
// The bench still owns the vertex TABLE and looks the IDs up in it. Turning
// 32-byte vertex records into coordinates is GEOM.VDECODE's job — its contract
// says "32 bytes per vertex, naturally aligned", and `zhao_geom_assemble` does
// not decode anything. Step 5 moves the SELECTION and not the DECODE, and
// saying so is the difference between scaffolding and a block quietly stood in
// for.
//
// ---------------------------------------------------------------------------
// THE TEST THAT WOULD PASS WITH ASSEMBLE DEAD
// ---------------------------------------------------------------------------
// If the index stream were the identity — triplet (0, 1, 2) into a table whose
// entries are already in triangle order — then a shell that ignored ASSEMBLE
// entirely and used vertices 0, 1, 2 in order would draw the same picture. The
// check would be green and the block would never have run.
//
// So the stream here is a PERMUTATION: the triangle is (2, 0, 3) out of a
// four-entry table, and entry 1 is a decoy placed far off to one side. Drawing
// it would move the picture visibly. The sensitivity probe feeds a different
// permutation and requires the framebuffer to change, which is the same
// discipline steps 3 and 4 use and which step 2 taught by failing to have it.

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

// FOUR clip-space vertices. Index 1 is the decoy: far enough off that a
// triangle using it instead of the intended one draws a visibly different
// shape rather than a subtly different one.
struct ClipVtx {
  int32_t x, y, z;
};
const ClipVtx kTable[4] = {
    {-(1 << 15), -(1 << 15), 1 << 16},  // 0
    { (7 << 13),  (7 << 13), 1 << 16},  // 1  the DECOY
    { (3 << 14), -(1 << 14), 1 << 16},  // 2
    {-(1 << 14),  (3 << 14), 1 << 16},  // 3
};

// The triangle is vertices 2, 0, 3 -- a permutation, so an implementation that
// ignored ASSEMBLE and took 0, 1, 2 in order would draw something else.
const uint8_t kTriIdx[3] = {2, 0, 3};
const uint8_t kOtherIdx[3] = {1, 0, 3};  // the sensitivity probe

zref::mat4fx the_matrix() {
  zref::mat4fx m{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) m.m[r][c] = zref::fx16{0};
  m.m[0][0] = zref::fx16{1 << 16};
  m.m[1][1] = zref::fx16{1 << 16};
  m.m[2][2] = zref::fx16{1 << 16};
  m.m[3][3] = zref::fx16{1 << 16};
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
  bool named = false;
  uint16_t v0 = 0, v1 = 0, v2 = 0;
  uint32_t triangles = 0;
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

// mode 0 = bench names the vertices (steps 1-4 only)
// mode 1 = GEOM.ASSEMBLE names them
Pass draw_once(int mode, const uint8_t idx[3]) {
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

  const zref::mat4fx m = the_matrix();
  const zr::Viewport vp = the_viewport();
  for (int rr = 0; rr < 4; ++rr)
    for (int cc = 0; cc < 4; ++cc)
      write_cfg(h, (uint8_t)(rr * 4 + cc), (uint32_t)m.m[rr][cc].raw);
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

  // ---- the vertex table, and the bench's own choice of three --------------
  for (int k = 0; k < 4; ++k) {
    h.top.asm_vtx_x_i[k] = kTable[k].x;
    h.top.asm_vtx_y_i[k] = kTable[k].y;
    h.top.asm_vtx_z_i[k] = kTable[k].z;
  }
  // In mode 0 the bench names the same three directly, so the two passes are
  // the SAME TRIANGLE by construction and any difference is the boundary move.
  h.top.proj_ax_i = kTable[idx[0]].x;
  h.top.proj_ay_i = kTable[idx[0]].y;
  h.top.proj_az_i = kTable[idx[0]].z;
  h.top.proj_bx_i = kTable[idx[1]].x;
  h.top.proj_by_i = kTable[idx[1]].y;
  h.top.proj_bz_i = kTable[idx[1]].z;
  h.top.proj_cx_i = kTable[idx[2]].x;
  h.top.proj_cy_i = kTable[idx[2]].y;
  h.top.proj_cz_i = kTable[idx[2]].z;

  // one meshlet, one triangle, one triplet
  h.top.asm_vertex_count_i = 4;
  h.top.asm_triangle_count_i = 1;
  h.top.asm_index_stream_i = ((uint32_t)idx[0]) | ((uint32_t)idx[1] << 8) |
                             ((uint32_t)idx[2] << 16);

  h.top.setup_mode_i = 1;
  h.top.clip_mode_i = 0;
  h.top.project_mode_i = 1;
  h.top.assemble_mode_i = (mode >= 1) ? 1 : 0;

  ShellHarness::RenderTri t;
  for (int e = 0; e < 3; ++e) {
    t.kx[e] = 0;
    t.ky[e] = 0;
    t.kc[e] = 0;
  }
  t.tl = 0;
  t.ax = t.ay = t.bx = t.by = t.cx = t.cy = 0;
  t.min_x = 0;
  t.max_x = (int16_t)(kGridW * 16 - 1);
  t.min_y = 0;
  t.max_y = (int16_t)(kGridH * 16 - 1);
  t.src_id = 0x2A2A;

  h.render_frame_begin(kGridW, kGridH);
  const bool took = h.render_offer(t);

  // Run the WHOLE window and count every hop. Step 4's first version broke out
  // at the first interesting signal and ended the frame before the picture
  // existed; that mistake is not worth repeating in the file that follows it.
  for (int i = 0; i < 200000; ++i) {
    h.top.eval();
    if (h.top.dbg_asm_valid_o && !r.named) {
      r.named = true;
      r.v0 = h.top.dbg_asm_v0_o;
      r.v1 = h.top.dbg_asm_v1_o;
      r.v2 = h.top.dbg_asm_v2_o;
    }
    h.step();
  }
  r.triangles = h.top.dbg_asm_triangles_o;

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

  const Pass pre = draw_once(/*mode=*/0, kTriIdx);
  const Pass via = draw_once(/*mode=*/1, kTriIdx);

  std::printf("  assemble named vertices (%u, %u, %u); triangles = %u\n", via.v0,
              via.v1, via.v2, via.triangles);

  check(pre.took, "the bench-named pass's triangle was ACCEPTED", 1, pre.took);
  check(via.named,
        "GEOM.ASSEMBLE emitted a TriangleDescriptor -- it walked the index "
        "stream rather than the bench walking it",
        1, via.named ? 1 : 0);
  check(via.v0 == kTriIdx[0] && via.v1 == kTriIdx[1] && via.v2 == kTriIdx[2],
        "and named exactly the triplet the index stream carried, in order",
        (kTriIdx[0] << 16) | (kTriIdx[1] << 8) | kTriIdx[2],
        (via.v0 << 16) | (via.v1 << 8) | via.v2);
  check(via.triangles == 1, "one meshlet of one triangle produced one triangle",
        1, via.triangles);

  const int nz = nonzero(pre.fb);
  check(nz > 0,
        "the bench-named pass actually drew something -- otherwise this "
        "compares two blank frames",
        1, nz > 0 ? 1 : 0);
  check(nonzero(via.fb) > 0, "and so did the ASSEMBLE pass", 1,
        nonzero(via.fb) > 0 ? 1 : 0);
  check(differing(pre.fb, via.fb) == 0,
        "the same triangle drawn with GEOM.ASSEMBLE naming the vertices "
        "produces an IDENTICAL framebuffer",
        0, differing(pre.fb, via.fb));

  // ---- can the comparison fail? ------------------------------------------
  // A DIFFERENT triplet must draw a different picture. If it does not, the
  // index stream is not reaching the vertices and the match above is empty --
  // which is exactly what step 2's framebuffer check turned out to be.
  const Pass other = draw_once(/*mode=*/1, kOtherIdx);
  const int diff = differing(via.fb, other.fb);
  std::printf("  sensitivity: a different index triplet changes %d words\n",
              diff);
  check(diff > 0,
        "a different index triplet MOVES the picture, so the match above is "
        "real evidence -- the identity triplet (0,1,2) would have passed with "
        "ASSEMBLE dead, which is why the stream is a permutation",
        1, diff > 0 ? 1 : 0);

  std::printf("[shell_assemble_path_directed] %d non-zero words drawn; %d checks %s\n",
              nz, g_checks, g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
