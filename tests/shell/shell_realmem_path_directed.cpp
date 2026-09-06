// shell_realmem_path_directed.cpp -- D22 tread 10: REAL MEMORY.
//
// THE LAST THING THE BENCH WAS STILL PLAYING. Treads 6 through 9 each took one
// input the harness supplied and gave it to a composed block -- setup, project,
// assemble, vdecode, assetfetch, the index stream -- and every one of them was
// measured against a memory that granted immediately and answered in one
// cycle, because the bench WAS the memory. `af_pool_i` is a register file the
// C++ writes and the SystemVerilog indexes; there is no arbitration in it, no
// refresh, no CAS latency and no other client.
//
// This tread removes it. In the real-memory pass GEOM.ASSETFETCH's guard
// request goes to `zhao_shell_top`'s own MEM.GUARD, the guard forwards to
// MEM.VRAM.ARBITER on client slot 3, the arbiter offers to MEM.SDRAM.CTRL, and
// the beats come back out of `zhao_sdram_model` -- through an ACTIVATE, a READ,
// three cycles of CAS latency, refresh cycles that close every open row, and a
// scanout client that is reading the framebuffer for the whole frame.
//
// THE PLAYED POOL IS POISON in that pass. `af_pool_i` is filled with records
// for the DECOY vertices, so a shell that quietly kept reading it draws a
// visibly different triangle instead of accidentally agreeing. That is the
// same device treads 7, 8 and 9 used and it is the only reason "the picture
// matched" means anything here: without it, an unconnected memory path and a
// working one produce the same frame.
//
// WHAT THIS TREAD DOES NOT DO, stated rather than left to be discovered.
// GEOM.MESHFETCH still has a played guard. `zhao_vram_arbiter` builds the
// controller's client tag by casting the SLOT INDEX --
// `ctrl_req.client = zhao_client_e'(offer_client)` -- so slot 3 is ENGINE1 and
// slot 4 is DEBUG, positionally; and `zhao_mem_guard` grants the asset-pool
// window to ENGINE1 alone, with DEBUG falling to `default: pass_ok = 1'b0`.
// A second geometry fetcher has no client identity that both the guard admits
// and the arbiter carries. That is a memory-law decision, not a wiring one.
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

// The write half of the same backdoor. It bypasses the SDRAM protocol entirely
// and exists to PLACE a fixture, never to model a write -- the write path has
// its own tread and its own tripwire.
void poke(ShellHarness& h, uint32_t waddr, uint16_t d) {
  h.top.poke_en = 1;
  h.top.poke_waddr = waddr;
  h.top.poke_data = d;
  h.step();
  h.top.poke_en = 0;
  h.top.eval();
}

constexpr uint8_t kGridW = 4, kGridH = 4;
constexpr uint32_t kSlotHalfwords = 245760u / 2u;

// zhao_pkg::ZHAO_GEOM_ASSET_BASE. The pool has to live where `zhao_mem_guard`
// admits a read -- `addr32 >= ZHAO_GEOM_ASSET_BASE` -- which is the first thing
// about this tread that the played responder could not have taught us, because
// a played responder never looks at the address at all.
constexpr uint32_t kAssetBase = 0x06A0'0000u;

struct ClipVtx {
  int32_t x, y, z;
};
const ClipVtx kTable[4] = {
    {-(1 << 15), -(1 << 15), 1 << 16},  // 0
    {(7 << 13), (7 << 13), 1 << 16},    // 1  the DECOY
    {(3 << 14), -(1 << 14), 1 << 16},   // 2
    {-(1 << 14), (3 << 14), 1 << 16},   // 3
};

const ClipVtx kPoison[4] = {
    {(1 << 15), (1 << 15), 1 << 16},
    {-(7 << 13), -(7 << 13), 1 << 16},
    {-(3 << 14), (1 << 14), 1 << 16},
    {(1 << 14), -(3 << 14), 1 << 16},
};

const uint8_t kTriIdx[3] = {2, 0, 3};

void make_record_words(uint32_t w[8], int32_t x, int32_t y, int32_t z) {
  w[0] = static_cast<uint32_t>(x);
  w[1] = static_cast<uint32_t>(y);
  w[2] = static_cast<uint32_t>(z);
  w[3] = (0u) | (0u << 8) | (127u << 16) | (64u << 24);
  w[4] = 0;
  w[5] = 0;
  w[6] = 0;
  w[7] = 0;
}

// The 32-beat pool: four 32-byte vertex records at offset 0, the index triplet
// at offset 128. Identical layout to tread 9's -- what moves is WHERE it lives.
void pack_pool(uint64_t pool[32], const ClipVtx tbl[4], const uint8_t idx[3]) {
  for (int i = 0; i < 32; ++i) pool[i] = 0;
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
  uint32_t vertices = 0;
  uint32_t format_bad = 0;
  uint32_t meshlets = 0, beats = 0, denied = 0, refused_fp = 0, stalls = 0;
  uint32_t geom_beats = 0;
  uint32_t guard_violations = 0;
  uint32_t route_err = 0;
  uint16_t pool_word0 = 0;
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

// mode 0 = the played pool (tread 9's terminal state)
// mode 1 = REAL MEMORY, played pool poisoned
Pass draw_once(int mode) {
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
  std::printf("    [mode %d] inited=%d\n", mode, inited?1:0);
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

  // ---- PLACE THE POOL IN REAL MEMORY --------------------------------------
  // 32 beats of 64 bits = 128 sixteen-bit words, word 0 in the LOW half of the
  // beat, because that is the order the shell's read packer rebuilds them in
  // (`pack_lo[16*pack_cnt]`, low group first). Getting this backwards would
  // produce a decoded record whose x and y are the halves of somebody else's
  // coordinate -- which is exactly the kind of wrongness a picture comparison
  // catches and a beat counter does not.
  {
    uint64_t pool[32];
    pack_pool(pool, kTable, kTriIdx);
    const uint32_t base_w = kAssetBase >> 1;
    for (int i = 0; i < 32; ++i)
      for (int j = 0; j < 4; ++j)
        poke(h, base_w + (uint32_t)(i * 4 + j),
             (uint16_t)((pool[i] >> (16 * j)) & 0xFFFFu));
    r.pool_word0 = peek(h, base_w);
  }

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
    const bool pub = h.publish(0, zhao_shell::build_packet(ps));
    std::printf("    [mode %d] published=%d\n", mode, pub?1:0);
    if (!pub) return r;
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
  std::printf("    [mode %d] lease_opens=%d\n", mode, lease_opens);
  if (lease_opens == 0) return r;

  // Everything the bench used to supply is POISON in the real-memory pass.
  for (int k = 0; k < 4; ++k) {
    h.top.asm_vtx_x_i[k] = kPoison[k].x;
    h.top.asm_vtx_y_i[k] = kPoison[k].y;
    h.top.asm_vtx_z_i[k] = kPoison[k].z;
    uint32_t w[8];
    make_record_words(w, kPoison[k].x, kPoison[k].y, kPoison[k].z);
    for (int j = 0; j < 8; ++j) h.top.vd_rec_i[k][j] = w[j];
  }
  h.top.proj_ax_i = 0;
  h.top.proj_ay_i = 0;
  h.top.proj_az_i = 0;
  h.top.proj_bx_i = 0;
  h.top.proj_by_i = 0;
  h.top.proj_bz_i = 0;
  h.top.proj_cx_i = 0;
  h.top.proj_cy_i = 0;
  h.top.proj_cz_i = 0;

  h.top.asm_vertex_count_i = 4;
  h.top.asm_triangle_count_i = 1;
  h.top.asm_index_stream_i[0] = 0x030001u;  // a legal but WRONG triplet
  h.top.asm_index_stream_i[1] = 0;
  h.top.asm_index_stream_i[2] = 0;

  h.top.setup_mode_i = 1;
  h.top.clip_mode_i = 0;
  h.top.project_mode_i = 1;
  h.top.assemble_mode_i = 1;
  h.top.vdecode_mode_i = 1;
  h.top.assetfetch_mode_i = 1;
  h.top.indexfetch_mode_i = 1;
  // The one bit that separates the two passes.
  h.top.realmem_mode_i = (mode >= 1) ? 1 : 0;

  // THE PLAYED POOL. Correct in mode 0, DECOY RECORDS in mode 1 -- so a shell
  // that never left the register file draws vertex 1's coordinates and the
  // frame comparison fails loudly instead of passing by coincidence.
  {
    uint64_t pool[32];
    pack_pool(pool, (mode >= 1) ? kPoison : kTable, kTriIdx);
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
  r.took = h.render_offer(t);

  // Real memory is SLOWER, and by an amount nobody should be guessing at: the
  // whole point of the tread is that the fetch now waits behind arbitration,
  // refresh and CAS. So the window is generous and the counters -- not the
  // loop bound -- are what the assertions read.
  for (int i = 0; i < 600000; ++i) {
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
  for (int i = 0; i < 400000; ++i) h.step();

  r.meshlets = h.top.dbg_af_meshlets_o;
  r.beats = h.top.dbg_af_beats_o;
  r.denied = h.top.dbg_af_denied_o;
  r.refused_fp = h.top.dbg_af_refused_o;
  r.stalls = h.top.dbg_af_stall_o;
  r.vertices = h.top.dbg_vd_vertices_o;
  r.format_bad = h.top.dbg_vd_format_bad_o;
  r.geom_beats = h.top.dbg_geom_grants_o;
  r.guard_violations = h.top.guard_violations_o;
  r.route_err = h.top.shell_err_route_o;
  for (uint32_t k = 0; k < kSlotHalfwords; ++k) r.fb[k] = peek(h, k);
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

  const Pass played = draw_once(/*mode=*/0);
  const Pass real = draw_once(/*mode=*/1);

  std::printf("  played: beats %u, meshlets %u, stalls %u, painted %d\n", played.beats,
              played.meshlets, played.stalls, nonzero(played.fb));
  std::printf("  REAL:   beats %u, meshlets %u, stalls %u, painted %d\n", real.beats,
              real.meshlets, real.stalls, nonzero(real.fb));
  std::printf("  REAL:   64-bit beats out of the DRAM %u; guard violations %u; route_err %u\n",
              real.geom_beats, real.guard_violations, real.route_err);
  std::printf("  REAL:   footprints refused %u, guard denials %u\n", real.refused_fp, real.denied);

  // ---- the fixture is really in memory ------------------------------------
  // Checked before anything else, because every assertion below is meaningless
  // if the pool is not where the guard expects it. Word 0 is vertex 0's x,
  // low half: -(1 << 15) as a u32 is 0xFFFF8000, low word 0x8000.
  check(real.pool_word0 == 0x8000u,
        "the asset pool is in real memory at ZHAO_GEOM_ASSET_BASE and reads "
        "back through the peek port -- the poke backdoor placed it, and a "
        "fixture that was never written would make every check below vacuous",
        0x8000, real.pool_word0);

  // ---- the traffic actually happened --------------------------------------
  // COUNTED, not inferred from the mode bit. A test that asserted
  // `realmem_mode_i` would pass with this entire path disconnected.
  check(real.geom_beats > 0,
        "64-bit beats came back to GEOM.ASSETFETCH FROM THE SDRAM MODEL -- "
        "through the shell's own guard, arbiter and controller, not from the "
        "bench's register file",
        1, real.geom_beats > 0 ? 1 : 0);
  check(played.geom_beats == 0,
        "and the played pass drove NO shell read traffic, so the counter is "
        "measuring this tread rather than something the shell always did",
        0, played.geom_beats);
  check(real.beats == played.beats,
        "GEOM.ASSETFETCH read the same number of beats either way -- the same "
        "footprint, fetched from somewhere else",
        played.beats, real.beats);

  // ---- and it was LEGAL ----------------------------------------------------
  check(real.guard_violations == 0,
        "MEM.GUARD refused nothing: the fetcher presented ENGINE1 and an "
        "address inside the asset window, which the played responder never "
        "checked because a played responder does not read those fields",
        0, real.guard_violations);
  check(real.route_err == 0,
        "and the shell's own burst-owner tripwire stayed quiet -- the guard "
        "was taught a new legal reader and so was the check below it, which "
        "is the mistake the write side already made once",
        0, real.route_err);

  // ---- the picture ---------------------------------------------------------
  check(played.took && real.took, "both passes ACCEPTED the triangle", 1,
        (played.took && real.took) ? 1 : 0);
  check(nonzero(real.fb) > 0,
        "the real-memory pass PAINTED something -- a silent empty frame is the "
        "failure mode a beat count cannot see",
        1, nonzero(real.fb) > 0 ? 1 : 0);
  check(real.vertices == played.vertices, "GEOM.VDECODE decoded the same vertex count",
        played.vertices, real.vertices);
  check(real.format_bad == 0,
        "with no malformed records -- bytes reassembled in the wrong word order "
        "would land in the reserved field and be REFUSED",
        0, real.format_bad);
  check(real.v0 == played.v0 && real.v1 == played.v1 && real.v2 == played.v2,
        "GEOM.ASSEMBLE named the same three vertices from an index run that "
        "came out of the DRAM",
        (long long)played.v0, (long long)real.v0);

  // THE ONE THAT MATTERS. Byte-identical, with the played pool holding decoy
  // records throughout the real pass.
  const int diff = differing(played.fb, real.fb);
  check(diff == 0,
        "and the FRAME IS BYTE-IDENTICAL to the played-memory pass, while the "
        "bench's pool held the decoy records the whole time -- so the picture "
        "was rebuilt out of real memory rather than out of the register file "
        "nobody disconnected",
        0, diff);

  std::printf("[shell_realmem_path_directed] %d checks, %d failed\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
