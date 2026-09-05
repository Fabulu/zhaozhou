// shell_meshfetch_path_directed.cpp — D22 step 6's evidence, the last tread.
// Authored 2026-09-05 (roadmap G3).
//
// ---------------------------------------------------------------------------
// THE WHOLE STAIRCASE
// ---------------------------------------------------------------------------
//   step 1  precomputed edge coefficients  ->  GEOM.SETUP
//   step 2  precomputed invw24 depth       ->  GEOM.DEPTHQUANT
//   step 3  precomputed 2A and scan box    ->  GEOM.CLIP
//   step 4  precomputed screen vertices    ->  GEOM.PROJECT
//   step 5  "here are three vertices"      ->  GEOM.ASSEMBLE names them
//   step 6  "here is a meshlet"            ->  GEOM.MESHFETCH reads it
//
// Step 5 still had the bench hand ASSEMBLE a meshlet: a vertex count, a
// triangle count, a material. Step 6 stops supplying it. MESHFETCH reads a
// 64-byte meshlet DESCRIPTOR out of memory, validates its format, CRC and
// generation, culls it, and emits the meshlet ASSEMBLE walks.
//
// ---------------------------------------------------------------------------
// WHAT THE BENCH PLAYS, AND WHY THAT LIMITS THE CLAIM
// ---------------------------------------------------------------------------
// `zhao_geom_meshfetch` is the ONLY `zhao_guard_req_t` client in the geometry
// subsystem, so the bench plays three interfaces for it: the memory guard, the
// beat stream carrying the descriptor bytes, and the cull service.
//
// So this proves the DESCRIPTOR PATH inside the composed shell. It does not
// prove the asset fetcher, and it does not prove culling — the cull answer is
// a constant "visible" here precisely so a cull failure cannot masquerade as a
// descriptor success. Docket D22 identified one asset fetcher over
// GEOM.ASSET_POOL serving three consumers as its remaining work, and
// `spec/memory_rules.md` §5f ruled the region for it; that fetcher is not this.
//
// ---------------------------------------------------------------------------
// THE ORDER THE BENCH MUST ANSWER IN
// ---------------------------------------------------------------------------
// Beats may only start AFTER the guard grants. The unit bench already paid for
// getting this wrong and its comment is worth carrying over, because the
// symptom points at the wrong thing entirely: feeding beats from cycle zero
// drops all eight while the block is still in S_REQ, it then waits forever for
// beats that already came and went, and every bound reads (0,0,0) r=0 —
// "which looks like broken arithmetic and is actually a testbench that
// answered out of order".

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vtb_zhao_shell.h"

#define ZHAO_GEOM_DEV_ORACLE_ONLY
#define ZHAO_GEOM_DEV_BINNER

#include "geom_dev.hpp"
#include "shell_harness.hpp"
#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_meshfetch.hpp"
#include "zref/zref_render.hpp"
#include "zrender/internal.hpp"

using zhao_shell::ShellHarness;
namespace zr = zref::render;
namespace mf = zref::meshfetch;

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
constexpr int32_t ONE = 65536;
constexpr uint8_t kFormat = 1;
constexpr uint16_t kGeneration = 1;

// The meshlet this descriptor describes: FOUR vertices, ONE triangle. Those
// two numbers are the whole point of the step -- in step 5 the bench asserted
// them and here MESHFETCH reads them out of memory.
constexpr uint8_t kVertexCount = 4;
constexpr uint8_t kTriangleCount = 1;
constexpr uint16_t kMaterial = 0x0777;

void wr16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
void wr32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

// The 64-byte descriptor, laid out exactly as geom_meshfetch_rtl_directed does
// so the two benches cannot drift into describing different formats.
struct Desc {
  uint8_t b[mf::kDescBytes];
  Desc() {
    std::memset(b, 0, sizeof b);
    b[0] = kFormat;
    b[2] = kVertexCount;
    b[3] = kTriangleCount;
    wr16(b + 4, kMaterial);
    wr32(b + 8, (uint32_t)(0 * ONE));   // bound centre x
    wr32(b + 12, (uint32_t)(0 * ONE));  // bound centre y
    wr32(b + 16, (uint32_t)(0 * ONE));  // bound centre z
    wr32(b + 20, (uint32_t)(4 * ONE));  // bound radius, non-zero
    wr32(b + 24, 0x0000);               // vertex offset
    wr32(b + 28, 0x0000);               // index offset
    wr16(b + 32, kGeneration);
    wr16(b + 34, 9);
    stamp();
  }
  void stamp() { wr32(b + mf::kCrcOff, zhao_abi::zhao_crc32c(0, b, mf::kCrcCovered)); }
};

struct ClipVtx {
  int32_t x, y, z;
};
const ClipVtx kTable[4] = {
    {-(1 << 15), -(1 << 15), 1 << 16},
    {(7 << 13), (7 << 13), 1 << 16},
    {(3 << 14), -(1 << 14), 1 << 16},
    {-(1 << 14), (3 << 14), 1 << 16},
};
const uint8_t kTriIdx[3] = {2, 0, 3};

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
  bool read = false;
  uint8_t vcount = 0, tcount = 0;
  uint16_t material = 0;
  uint8_t vis = 0;
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

// mode 0 = the bench asserts the meshlet (step 5)
// mode 1 = GEOM.MESHFETCH reads it (step 6)
Pass draw_once(int mode, bool corrupt_crc) {
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

  // ---- the descriptor, as eight 64-bit beats -----------------------------
  Desc d;
  for (int w = 0; w < 8; ++w) {
    uint64_t v = 0;
    for (int k = 0; k < 8; ++k) v |= (uint64_t)d.b[w * 8 + k] << (8 * k);
    h.top.mf_desc_i[w] = v;
  }
  h.top.mf_crc_ok_i = corrupt_crc ? 0 : 1;

  for (int k = 0; k < 4; ++k) {
    h.top.asm_vtx_x_i[k] = kTable[k].x;
    h.top.asm_vtx_y_i[k] = kTable[k].y;
    h.top.asm_vtx_z_i[k] = kTable[k].z;
  }
  h.top.proj_ax_i = kTable[kTriIdx[0]].x;
  h.top.proj_ay_i = kTable[kTriIdx[0]].y;
  h.top.proj_az_i = kTable[kTriIdx[0]].z;
  h.top.proj_bx_i = kTable[kTriIdx[1]].x;
  h.top.proj_by_i = kTable[kTriIdx[1]].y;
  h.top.proj_bz_i = kTable[kTriIdx[1]].z;
  h.top.proj_cx_i = kTable[kTriIdx[2]].x;
  h.top.proj_cy_i = kTable[kTriIdx[2]].y;
  h.top.proj_cz_i = kTable[kTriIdx[2]].z;

  h.top.asm_vertex_count_i = kVertexCount;
  h.top.asm_triangle_count_i = kTriangleCount;
  h.top.asm_index_stream_i[0] =
      ((uint32_t)kTriIdx[0]) | ((uint32_t)kTriIdx[1] << 8) | ((uint32_t)kTriIdx[2] << 16);
  h.top.asm_index_stream_i[1] = 0;
  h.top.asm_index_stream_i[2] = 0;

  h.top.setup_mode_i = 1;
  h.top.clip_mode_i = 0;
  h.top.project_mode_i = 1;
  h.top.assemble_mode_i = 1;
  h.top.meshfetch_mode_i = (mode >= 1) ? 1 : 0;

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

  int saw_greq = 0, saw_grant = 0, saw_cull = 0, maxbeat = 0;
  int saw_asm = 0, saw_proj = 0, saw_clip = 0, saw_setup = 0;
  for (int i = 0; i < 200000; ++i) {
    h.top.eval();
    if (h.top.dbg_mf_greq_o) ++saw_greq;
    if (h.top.dbg_mf_granted_o) ++saw_grant;
    if (h.top.dbg_mf_cull_tick_o) ++saw_cull;
    if ((int)h.top.dbg_mf_beat_o > maxbeat) maxbeat = h.top.dbg_mf_beat_o;
    if (h.top.dbg_asm_valid_o) ++saw_asm;
    if (h.top.dbg_proj_ready_o) ++saw_proj;
    if (h.top.dbg_clip_valid_o) ++saw_clip;
    if (h.top.dbg_su_out_valid_o) ++saw_setup;
    if (h.top.dbg_mf_valid_o && !r.read) {
      r.read = true;
      r.vcount = h.top.dbg_mf_vcount_o;
      r.tcount = h.top.dbg_mf_tcount_o;
      r.material = h.top.dbg_mf_material_o;
      r.vis = h.top.dbg_mf_vis_o;
    }
    h.step();
  }

  if (mode >= 1)
    std::printf(
        "    [mf] guard_req %d | granted %d | max beat %d | cull ticks %d | "
        "fetched %u denied %u refused %u\n"
        "    [chain] asm %d | proj %d | clip %d | setup %d\n",
        saw_greq, saw_grant, maxbeat, saw_cull, h.top.dbg_mf_fetched_o, h.top.dbg_mf_denied_o,
        h.top.dbg_mf_refused0_o, saw_asm, saw_proj, saw_clip, saw_setup);

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

  const Pass pre = draw_once(/*mode=*/0, false);
  const Pass via = draw_once(/*mode=*/1, false);

  std::printf("  meshfetch read: vertices %u, triangles %u, material 0x%04X, vis %u\n", via.vcount,
              via.tcount, via.material, via.vis);

  check(pre.took, "the bench-asserted pass's triangle was ACCEPTED", 1, pre.took);
  check(via.read,
        "GEOM.MESHFETCH emitted a meshlet -- it read and validated the "
        "descriptor rather than the bench asserting one",
        1, via.read ? 1 : 0);
  check(via.vcount == kVertexCount && via.tcount == kTriangleCount,
        "and the counts came out of the DESCRIPTOR BYTES, not the bench",
        (kVertexCount << 8) | kTriangleCount, (via.vcount << 8) | via.tcount);
  check(via.material == kMaterial, "as did the material id", kMaterial, via.material);

  const int nz = nonzero(pre.fb);
  check(nz > 0,
        "the bench-asserted pass actually drew something -- otherwise this "
        "compares two blank frames",
        1, nz > 0 ? 1 : 0);
  check(nonzero(via.fb) > 0, "and so did the MESHFETCH pass", 1, nonzero(via.fb) > 0 ? 1 : 0);
  check(differing(pre.fb, via.fb) == 0,
        "the same triangle drawn with GEOM.MESHFETCH supplying the meshlet "
        "produces an IDENTICAL framebuffer",
        0, differing(pre.fb, via.fb));

  // ---- the refusal, which is what a validating reader is FOR --------------
  // A descriptor whose CRC does not check must produce NO meshlet. This is the
  // sensitivity probe and the contract's own rule at once: "a descriptor that
  // is not trustworthy" must not become geometry.
  const Pass bad = draw_once(/*mode=*/1, /*corrupt_crc=*/true);
  std::printf("  crc-failed descriptor: read = %d, drew %d words\n", bad.read ? 1 : 0,
              nonzero(bad.fb));
  check(!bad.read,
        "a descriptor whose CRC does not check emits NO meshlet -- MESHFETCH "
        "validates rather than transports",
        0, bad.read ? 1 : 0);
  check(nonzero(bad.fb) == 0,
        "and nothing is drawn from it, so the comparison above is sensitive to "
        "the descriptor actually being read",
        0, nonzero(bad.fb));

  std::printf("[shell_meshfetch_path_directed] %d non-zero words drawn; %d checks %s\n", nz,
              g_checks, g_failed ? "FAILED" : "passed");
  return g_failed ? 1 : 0;
}
