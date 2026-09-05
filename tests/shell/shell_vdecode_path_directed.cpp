// shell_vdecode_path_directed.cpp -- D22 tread 7: GEOM.VDECODE.
//
// THE BOUNDARY MOVE. Every earlier tread took something away from the bench and
// gave it to the shell. This one takes away the DECODED VERTEX: the bench stops
// supplying `asm_vtx_{x,y,z}_i` and supplies the four 32-byte format-0 RECORDS
// those coordinates were written into. `zhao_geom_vdecode` turns bytes into the
// vertex inside the composed shell, and the same triangle must come out
// byte-identical.
//
// HOW THE TWO PASSES ARE MADE THE SAME TRIANGLE. The record's position field is
// s32 fx16 S15.16 -- the same format and width as the table entry it replaces --
// so the records are authored to hold exactly the clip-space values the bench
// would otherwise have handed over. Any difference in the framebuffer is the
// boundary move and nothing else.
//
// WHAT THIS TREAD DELIBERATELY DOES NOT PROVE, so it is not read as more than
// it is:
//
//   * THE TRANSFORM. A record holds a MODEL-space position and the table held
//     CLIP-space. There is no transform block in this shell, so the transform
//     here is IDENTITY BY CONSTRUCTION. That is the device step 6 used when it
//     answered the cull with a constant VISIBLE -- hold the neighbour at a
//     known value so the tread under test is the only thing that can fail.
//   * THE BATCH ENGINE. `zhao_geom_vdecode`'s own header is explicit that it is
//     the record leaf and NOT GEOM.VDECODE's batch engine: vertex_count,
//     addressing across burst boundaries and the all-or-nothing batch rule are
//     not in it. The ledger entry stays SPECIFIED and this tread does not
//     advance it.
//   * ANY FORMAT BUT 0. Formats 1 and 2 are bake-off gated.
//
// THE TABLE IS POISONED IN THE VDECODE PASS. `asm_vtx_*_i` is driven with
// deliberately wrong coordinates whenever the shell is decoding for itself, so
// a shell that quietly kept reading the bench's table draws a different picture
// instead of accidentally agreeing with it. Without that, a dead VDECODE path
// and a working one are indistinguishable.
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

// A 32-byte format-0 record, little-endian, offset 0 at bit 0:
//   off  0  12  position s32 x3 (fx16 S15.16)
//   off 12   3  normal s8 x3       off 15  1  w0, 1/64 quanta (64 == rigid)
//   off 16   4  UV 2 x s16         off 20  2  bone0    off 22  2  bone1
//   off 24   8  reserved, MUST BE ZERO -- a nonzero byte is REFUSED, not
//               flagged and emitted, so a sloppy record here would silently
//               drop a vertex and leave a hole rather than fail loudly.
void make_record(uint32_t w[8], int32_t x, int32_t y, int32_t z) {
  w[0] = static_cast<uint32_t>(x);
  w[1] = static_cast<uint32_t>(y);
  w[2] = static_cast<uint32_t>(z);
  // normal (0,0,127) and w0 = 64, the rigid quantum. w0 > 64 is refused.
  w[3] = (0u) | (0u << 8) | (127u << 16) | (64u << 24);
  w[4] = 0;  // UV
  w[5] = 0;  // bone0, bone1
  w[6] = 0;  // reserved
  w[7] = 0;  // reserved
}

// Coordinates the shell must NOT draw. Driven into the bench's table during the
// VDECODE pass, so reading the table instead of the decoded record is visible.
const ClipVtx kPoison[4] = {
    { (1 << 15),  (1 << 15), 1 << 16},
    {-(7 << 13), -(7 << 13), 1 << 16},
    {-(3 << 14),  (1 << 14), 1 << 16},
    { (1 << 14), -(3 << 14), 1 << 16},
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
  bool decoded = false;
  uint32_t vertices = 0;
  bool refused = false;
  uint32_t format_bad = 0;
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
    // In the VDECODE pass the table is POISON: the shell must be reading the
    // records it decoded, not this.
    const ClipVtx& t = (mode >= 1) ? kPoison[k] : kTable[k];
    h.top.asm_vtx_x_i[k] = t.x;
    h.top.asm_vtx_y_i[k] = t.y;
    h.top.asm_vtx_z_i[k] = t.z;
    // The records always carry the REAL table, in every mode. They are simply
    // ignored unless vdecode_mode_i is set.
    uint32_t w[8];
    make_record(w, kTable[k].x, kTable[k].y, kTable[k].z);
    for (int j = 0; j < 8; ++j) h.top.vd_rec_i[k][j] = w[j];
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
  // 96 bits wide (4 triplets x 24), so Verilator exposes it as VlWide<3> and
  // it must be written a word at a time. Only triplet 0 is used; the rest are
  // zeroed rather than left indeterminate, because an ASSEMBLE that walked
  // past its triangle_count would then read a defined zero and the overrun
  // would be visible instead of random.
  h.top.asm_index_stream_i[0] = ((uint32_t)idx[0]) | ((uint32_t)idx[1] << 8) |
                                ((uint32_t)idx[2] << 16);
  h.top.asm_index_stream_i[1] = 0;
  h.top.asm_index_stream_i[2] = 0;

  h.top.setup_mode_i = 1;
  h.top.clip_mode_i = 0;
  h.top.project_mode_i = 1;
  // ASSEMBLE is on in BOTH passes: this tread moves where the vertex TABLE
  // comes from, not who names the triplet. Keeping ASSEMBLE constant is what
  // makes the difference between the passes attributable to VDECODE alone.
  h.top.assemble_mode_i = 1;
  h.top.vdecode_mode_i = (mode >= 1) ? 1 : 0;

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

  r.decoded = h.top.dbg_vd_have_o != 0;
  r.vertices = h.top.dbg_vd_vertices_o;
  r.refused = h.top.dbg_vd_refused_o != 0;
  r.format_bad = h.top.dbg_vd_format_bad_o;
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

  std::printf("  vdecode: decoded %u vertices, have=%d, refused=%d, "
              "format_bad=%u; assemble named (%u, %u, %u)\n",
              via.vertices, via.decoded ? 1 : 0, via.refused ? 1 : 0,
              via.format_bad, via.v0, via.v1, via.v2);

  check(pre.took, "the bench-table pass's triangle was ACCEPTED", 1, pre.took);
  check(via.decoded,
        "GEOM.VDECODE filled the vertex table from RECORDS -- the shell turned "
        "bytes into vertices instead of being handed coordinates",
        1, via.decoded ? 1 : 0);
  check(via.vertices == 4,
        "and decoded exactly the four records it was given", 4, via.vertices);
  check(!via.refused,
        "with no record REFUSED -- a malformed record is dropped, not flagged "
        "and emitted, so a refusal here would be a hole in the mesh",
        0, via.refused ? 1 : 0);
  check(via.format_bad == 0, "and none rejected for format", 0,
        via.format_bad);
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
        "the bench-table pass actually drew something -- otherwise this "
        "compares two blank frames",
        1, nz > 0 ? 1 : 0);
  check(nonzero(via.fb) > 0, "and so did the VDECODE pass", 1,
        nonzero(via.fb) > 0 ? 1 : 0);
  check(differing(pre.fb, via.fb) == 0,
        "the same triangle drawn from RECORDS the shell decoded produces an "
        "IDENTICAL framebuffer -- and the bench's table was POISON throughout "
        "that pass, so this cannot be the table leaking through",
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
        "real evidence and not two frames that agree because nothing reached "
        "them -- the habit step 2 forced, where the framebuffer check turned "
        "out to be blind to depth",
        1, diff > 0 ? 1 : 0);

  std::printf("[shell_vdecode_path_directed] %d non-zero words drawn; %d checks %s\n",
              nz, g_checks, g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
