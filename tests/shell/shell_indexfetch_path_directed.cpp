// shell_indexfetch_path_directed.cpp -- D22 tread 9: the u8 INDEX STREAM.
//
// THE BOUNDARY MOVE, and it is the cheapest one in the staircase because the
// work was already being done. Tread 8 had GEOM.ASSETFETCH read the meshlet's
// whole footprint -- 24 beats, of which EIGHT were the index run -- and then
// throw the index result away, because `ix_req_i` was tied off and
// GEOM.ASSEMBLE still read the bench's flat `asm_index_stream_i`. This tread
// connects the port that was already being fed.
//
// WHY ASSETFETCH BUFFERS RATHER THAN CACHES, which this tread is the proof of:
// GEOM.ASSEMBLE's index port has `ix_req_o` and `ix_valid_i` and NO READY, by
// that block's own deliberate choice. A cache behind a port that cannot stall
// is not an optimisation, it is a protocol violation waiting for a miss. The
// whole-footprint prefetch is what makes an always-answerable port possible.
//
// THE BENCH'S INDEX STREAM IS POISON in the fetch pass. `asm_index_stream_i`
// is driven with a different triplet whenever the fetcher is supposed to be
// answering, so a shell that quietly kept reading it draws a different picture
// instead of accidentally agreeing -- the same device treads 7 and 8 used.
//
// WHAT REMAINS THE BENCH'S: the memory itself. The guard and the beats are
// still played here, and `dbg_af_beats_o` counts what was actually read so a
// fetcher that never fetched cannot pass.
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
    {(7 << 13), (7 << 13), 1 << 16},    // 1  the DECOY
    {(3 << 14), -(1 << 14), 1 << 16},   // 2
    {-(1 << 14), (3 << 14), 1 << 16},   // 3
};

// A 32-byte format-0 record, little-endian, offset 0 at bit 0:
//   off  0  12  position s32 x3 (fx16 S15.16)
//   off 12   3  normal s8 x3       off 15  1  w0, 1/64 quanta (64 == rigid)
//   off 16   4  UV 2 x s16         off 20  2  bone0    off 22  2  bone1
//   off 24   8  reserved, MUST BE ZERO -- a nonzero byte is REFUSED, not
//               flagged and emitted, so a sloppy record here would silently
//               drop a vertex and leave a hole rather than fail loudly.
// The pool is 32 aligned 64-bit beats. The four 32-byte records occupy the
// first 128 bytes -- beats 0..15 -- in vertex order, which is the layout
// GEOM.ASSETFETCH expects at `m_vertex_offset_i` = 0.
void make_record_words(uint32_t w[8], int32_t x, int32_t y, int32_t z);

void pack_pool(uint64_t pool[32], const ClipVtx tbl[4], const uint8_t idx[3]) {
  for (int i = 0; i < 32; ++i) pool[i] = 0;
  // The index run lives at byte offset 128 -- pool line 2, beat 16 -- because
  // that is what `m_index_offset_i` names. Triplet 0 is the low three bytes.
  pool[16] = static_cast<uint64_t>(idx[0]) | (static_cast<uint64_t>(idx[1]) << 8) |
             (static_cast<uint64_t>(idx[2]) << 16);
  for (int k = 0; k < 4; ++k) {
    uint32_t w[8];
    make_record_words(w, tbl[k].x, tbl[k].y, tbl[k].z);
    for (int j = 0; j < 4; ++j)
      pool[k * 4 + j] =
          static_cast<uint64_t>(w[2 * j]) | (static_cast<uint64_t>(w[2 * j + 1]) << 32);
  }
}

void make_record_words(uint32_t w[8], int32_t x, int32_t y, int32_t z) {
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
    {(1 << 15), (1 << 15), 1 << 16},
    {-(7 << 13), -(7 << 13), 1 << 16},
    {-(3 << 14), (1 << 14), 1 << 16},
    {(1 << 14), -(3 << 14), 1 << 16},
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
  uint32_t meshlets = 0, beats = 0, denied = 0, refused_fp = 0;
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

  // ---- the vertex table, and the bench's own choice of three --------------
  for (int k = 0; k < 4; ++k) {
    // In the VDECODE pass the table is POISON: the shell must be reading the
    // records it decoded, not this.
    const ClipVtx& t = (mode >= 1) ? kPoison[k] : kTable[k];
    h.top.asm_vtx_x_i[k] = t.x;
    h.top.asm_vtx_y_i[k] = t.y;
    h.top.asm_vtx_z_i[k] = t.z;
    // In the ASSETFETCH pass the bench's records are POISON: the fetcher is
    // supposed to be serving them out of the pool instead.
    uint32_t w[8];
    if (mode >= 1)
      make_record_words(w, kPoison[k].x, kPoison[k].y, kPoison[k].z);
    else
      make_record_words(w, kTable[k].x, kTable[k].y, kTable[k].z);
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
  // POISON in the fetch pass: a different, legal triplet, so a shell still
  // reading the bench's stream draws a DIFFERENT triangle rather than the
  // right one by accident.
  if (mode >= 1)
    h.top.asm_index_stream_i[0] =
        ((uint32_t)kOtherIdx[0]) | ((uint32_t)kOtherIdx[1] << 8) | ((uint32_t)kOtherIdx[2] << 16);
  else
    h.top.asm_index_stream_i[0] =
        ((uint32_t)idx[0]) | ((uint32_t)idx[1] << 8) | ((uint32_t)idx[2] << 16);
  h.top.asm_index_stream_i[1] = 0;
  h.top.asm_index_stream_i[2] = 0;

  h.top.setup_mode_i = 1;
  h.top.clip_mode_i = 0;
  h.top.project_mode_i = 1;
  // ASSEMBLE is on in BOTH passes: this tread moves where the vertex TABLE
  // comes from, not who names the triplet. Keeping ASSEMBLE constant is what
  // makes the difference between the passes attributable to VDECODE alone.
  h.top.assemble_mode_i = 1;
  // VDECODE is on in BOTH passes -- this tread moves where its RECORDS come
  // from, not whether it decodes. Holding it constant is what makes the
  // difference attributable to ASSETFETCH alone.
  h.top.vdecode_mode_i = 1;
  // ASSETFETCH is on in BOTH passes -- this tread moves only who answers the
  // INDEX port, so holding the record path constant is what makes the
  // difference attributable to the index route alone.
  h.top.assetfetch_mode_i = 1;
  h.top.indexfetch_mode_i = (mode >= 1) ? 1 : 0;
  {
    uint64_t pool[32];
    pack_pool(pool, kTable, idx);
    for (int i = 0; i < 32; ++i) h.top.af_pool_i[i] = pool[i];
  }

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

  r.meshlets = h.top.dbg_af_meshlets_o;
  r.beats = h.top.dbg_af_beats_o;
  r.denied = h.top.dbg_af_denied_o;
  r.refused_fp = h.top.dbg_af_refused_o;
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

  std::printf("  assetfetch: meshlets %u, beats %u, denied %u, refused %u\n", via.meshlets,
              via.beats, via.denied, via.refused_fp);
  std::printf(
      "  vdecode: decoded %u vertices, have=%d, refused=%d, "
      "format_bad=%u; assemble named (%u, %u, %u)\n",
      via.vertices, via.decoded ? 1 : 0, via.refused ? 1 : 0, via.format_bad, via.v0, via.v1,
      via.v2);

  check(pre.took, "the bench-table pass's triangle was ACCEPTED", 1, pre.took);
  check(via.meshlets > 0, "GEOM.ASSETFETCH fetched a meshlet footprint", 1,
        via.meshlets > 0 ? 1 : 0);
  check(via.v0 == kTriIdx[0] && via.v1 == kTriIdx[1] && via.v2 == kTriIdx[2],
        "and GEOM.ASSEMBLE named the triplet the FETCHED index run carried, "
        "not the poisoned one the bench was still offering",
        (kTriIdx[0] << 16) | (kTriIdx[1] << 8) | kTriIdx[2],
        (via.v0 << 16) | (via.v1 << 8) | via.v2);
  check(via.beats > 0,
        "and actually READ BEATS doing it, so 'it never fetched' cannot pass "
        "as success",
        1, via.beats > 0 ? 1 : 0);
  check(via.denied == 0, "with no guard denial", 0, via.denied);
  check(via.refused_fp == 0, "and no refused footprint", 0, via.refused_fp);
  check(via.decoded,
        "GEOM.VDECODE still filled the vertex table, now from records the "
        "SHELL fetched for itself",
        1, via.decoded ? 1 : 0);
  check(via.vertices == 4, "and decoded exactly the four records it was given", 4, via.vertices);
  check(!via.refused,
        "with no record REFUSED -- a malformed record is dropped, not flagged "
        "and emitted, so a refusal here would be a hole in the mesh",
        0, via.refused ? 1 : 0);
  check(via.format_bad == 0, "and none rejected for format", 0, via.format_bad);
  check(via.named,
        "GEOM.ASSEMBLE emitted a TriangleDescriptor -- it walked the index "
        "stream rather than the bench walking it",
        1, via.named ? 1 : 0);
  check(via.v0 == kTriIdx[0] && via.v1 == kTriIdx[1] && via.v2 == kTriIdx[2],
        "and named exactly the triplet the index stream carried, in order",
        (kTriIdx[0] << 16) | (kTriIdx[1] << 8) | kTriIdx[2],
        (via.v0 << 16) | (via.v1 << 8) | via.v2);
  check(via.triangles == 1, "one meshlet of one triangle produced one triangle", 1, via.triangles);

  const int nz = nonzero(pre.fb);
  check(nz > 0,
        "the bench-table pass actually drew something -- otherwise this "
        "compares two blank frames",
        1, nz > 0 ? 1 : 0);
  check(nonzero(via.fb) > 0, "and so did the VDECODE pass", 1, nonzero(via.fb) > 0 ? 1 : 0);
  check(differing(pre.fb, via.fb) == 0,
        "the same triangle drawn with the INDEX TRIPLET served out of the "
        "fetched footprint produces an IDENTICAL framebuffer -- and the "
        "bench's index stream was POISON throughout that pass, naming a "
        "different triangle, so this cannot be it leaking through",
        0, differing(pre.fb, via.fb));

  // ---- can the comparison fail? ------------------------------------------
  // A DIFFERENT triplet must draw a different picture. If it does not, the
  // index stream is not reaching the vertices and the match above is empty --
  // which is exactly what step 2's framebuffer check turned out to be.
  const Pass other = draw_once(/*mode=*/1, kOtherIdx);
  const int diff = differing(via.fb, other.fb);
  std::printf("  sensitivity: a different index triplet changes %d words\n", diff);
  check(diff > 0,
        "a different index triplet MOVES the picture, so the match above is "
        "real evidence and not two frames that agree because nothing reached "
        "them -- the habit step 2 forced, where the framebuffer check turned "
        "out to be blind to depth",
        1, diff > 0 ? 1 : 0);

  std::printf("[shell_indexfetch_path_directed] %d non-zero words drawn; %d checks %s\n", nz,
              g_checks, g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
