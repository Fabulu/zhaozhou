// render_fb_directed.cpp — the console draws a triangle into memory.
//
// ---------------------------------------------------------------------------
// WHAT IS NEW HERE
// ---------------------------------------------------------------------------
// Every rasterizer test in this tree ends at a pixel STREAM. The binner is
// checked against `zref::Binner`, the tile pipe against the five raster oracles
// composed, and the seam between them against both in series -- but all three
// stop at `fb_rgb565_o` and none of them asks where the pixel GOES.
//
// This file asks. `zhao_probe_render_fb` is the binner, the tile pipe and
// `zhao_raster_fbwrite` joined, and the harness models MEM.GUARD and a VRAM
// behind it. So a triangle described in screen coordinates comes out as BYTES
// AT ADDRESSES, and the check is a whole framebuffer compared against one built
// by the oracles.
//
// That is the step between "the rasterizer is verified" and "the console
// draws". The shell integrates CMD, MEM, VIDEO, INPUT, AUDIO and DEBUG and has
// never had a render path; this is the piece it was missing, tested before it
// is wired in.
//
// ---------------------------------------------------------------------------
// THE ORACLE IS THE EXISTING ONES, PLUS AN ADDRESS
// ---------------------------------------------------------------------------
// `zref::Binner` says which tiles a triangle touches and in what order.
// `pipe_oracle` says what one tile looks like after a job. Those are unchanged
// and un-re-derived. The only new arithmetic is where a tile's 256 pixels land:
//
//     byte address = fb_base + (ty*16 + row) * stride + (tx*16 + col) * 2
//
// and it is written out here rather than copied from the RTL, so the two are
// independent statements of the same law.
//
// ONE RESOLVE PER JOB, SO A SHARED TILE IS WRITTEN TWICE. The tile pipe
// declares it and the seam test measures it; in a FRAMEBUFFER it becomes
// "the later write wins", which is what the expected image below models by
// applying the jobs in order. That is also the first place in this project
// where that property is visible as a PICTURE rather than as a beat count.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "verilated.h"

#include "Vzhao_probe_render_fb.h"

#include "zhao_sim.hpp"
#include "zref/zref_geom.hpp"

#define ZHAO_PIPE_DEV_ORACLE_ONLY 1
#define ZHAO_GEOM_DEV_ORACLE_ONLY 1
#define ZHAO_GEOM_DEV_BINNER 1
#include "../geometry/geom_dev.hpp"
#include "../raster/raster_tile_pipe_dev.hpp"

namespace {

using zhao_geom::BinTri;
using zhao_geom::make_bin_tri;
using zhao_raster::kPipePixels;
using zhao_raster::kPipeTile;
using zhao_raster::pipe_oracle;
using zhao_raster::PipeExpect;
using zhao_raster::PipeJob;
using zhao_raster::px;

// A small canvas: big enough that a triangle crosses many tiles, small enough
// that a whole-framebuffer comparison prints usefully when it fails.
constexpr int kGridW = 8;
constexpr int kGridH = 6;
constexpr int kFbW = kGridW * kPipeTile;   // 128 px
constexpr int kFbH = kGridH * kPipeTile;   // 96 px
constexpr uint32_t kStride = kFbW * 2;     // bytes per row
constexpr uint32_t kFbBase = 0x0010'0000;  // somewhere unremarkable in VRAM
constexpr size_t kFbBytes = static_cast<size_t>(kStride) * kFbH;

// `zhao_guard_req_t` is 103 bits packed, so Verilator hands it over as a word
// array rather than a scalar. The field offsets are read off the struct's
// declaration order in zhao_pkg -- first field is the MSB -- and written out
// here once rather than open-coded at each use:
//
//     [102]    valid      [101]   write     [100:98] client
//     [97:71]  addr       [70:64] len       [63:0]   be
//
// Stated as a decoder because a wrong shift here would silently read a
// plausible address and the test would then agree with itself.
uint64_t field(const VlWide<4>& w, int lsb, int bits) {
  uint64_t v = 0;
  for (int i = 0; i < bits; ++i) {
    const int b = lsb + i;
    if ((w[b >> 5] >> (b & 31)) & 1u) v |= (uint64_t)1 << i;
  }
  return v;
}
constexpr int kReqBeLsb = 0, kReqLenLsb = 64, kReqAddrLsb = 71, kReqValidLsb = 102;

struct Look {
  uint64_t fill = 0;
  uint64_t clear = 0;
  uint32_t state = 0;
  uint8_t src_a = 0xFF;
  uint32_t texel_rgb = 0;
  uint8_t texel_a = 0;
  uint8_t texel_idx = 0;
};

/**
 * The harness IS the memory system: MEM.GUARD's region check and a VRAM behind
 * it. Modelled rather than instantiated because the shipping guard is already
 * integrated in `zhao_shell_top`, and a second copy here would be testing the
 * copy.
 *
 * The region is exactly the framebuffer, so a write outside it is refused and
 * counted -- which is how this file can assert that the render path never
 * addresses a byte it was not granted, without needing the real guard to say so.
 */
struct Vram {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(kFbBytes + 0x1000, 0);
  uint32_t refused = 0;
  uint32_t beats = 0;
  // Set non-zero to make memory slow: `stall_mask` gates write-data readiness
  // on a counter, so the burst -- and through it the whole render path -- has
  // to wait.
  uint32_t stall_mask = 0;
  uint32_t tick = 0;

  bool in_region(uint32_t addr, uint32_t len) const {
    return addr >= kFbBase && (uint64_t)addr + len <= (uint64_t)kFbBase + kFbBytes;
  }
  void store(uint32_t addr, uint16_t v) {
    const size_t off = addr - kFbBase;
    if (off + 1 < bytes.size()) {
      bytes[off] = (uint8_t)(v & 0xFF);
      bytes[off + 1] = (uint8_t)(v >> 8);
    }
  }
  uint16_t load(uint32_t addr) const {
    const size_t off = addr - kFbBase;
    if (off + 1 >= bytes.size()) return 0;
    return (uint16_t)(bytes[off] | ((uint16_t)bytes[off + 1] << 8));
  }
};

/** Drives the probe and plays the memory system on the other side. */
struct Dut {
  Vzhao_probe_render_fb& t;
  Vram mem;
  long clocks = 0;

  // The burst in flight, as the modelled guard sees it.
  bool active = false;
  uint32_t base = 0;
  uint32_t len = 0;
  uint32_t got = 0;  // bytes accepted so far

  explicit Dut(Vzhao_probe_render_fb& top) : t(top) {}

  void reset(const Look& lk) {
    t.rst_n = 0;
    t.frame_begin_i = 0;
    t.frame_end_i = 0;
    t.grid_w_i = kGridW;
    t.grid_h_i = kGridH;
    t.tri_valid_i = 0;
    t.tok_grant_i = 1;
    t.job_fill_word_i = lk.fill;
    t.job_clear_word_i = lk.clear;
    t.job_state_i = lk.state;
    t.job_src_a_i = lk.src_a;
    t.job_texel_rgb_i = lk.texel_rgb;
    t.job_texel_a_i = lk.texel_a;
    t.job_texel_idx_i = lk.texel_idx;
    t.fb_base_i = kFbBase;
    t.fb_stride_i = kStride;
    t.guard_rsp_i = 0;
    t.guard_wready_i = 0;
    t.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    zhao::tick(t);
    mem = Vram{};
    clocks = 0;
  }

  /** One clock, with the modelled guard and VRAM answering. */
  void step() {
    // ---- the guard's request port ----------------------------------------
    // `ready` is asserted whenever no burst is in flight; `ok` is the region
    // check. A refused request is DROPPED and nothing is written, exactly as
    // zhao_mem_guard's contract states.
    uint8_t rsp = 0;
    t.eval();
    const bool req_v = field(t.guard_req_o, kReqValidLsb, 1) != 0;
    if (req_v && !active) {
      const uint32_t a = (uint32_t)field(t.guard_req_o, kReqAddrLsb, 27);
      const uint32_t l = (uint32_t)field(t.guard_req_o, kReqLenLsb, 7);
      const bool ok = mem.in_region(a, l);
      // zhao_guard_rsp_t is {ready, ok, violation} declared MSB-first, so
      // ready is bit 2, ok is bit 1 and violation is bit 0. The first version
      // of this line put ready at bit 0 and the render path was never told its
      // request had been accepted: zero bursts, zero pixels, and a frame that
      // compared equal to an all-black expectation.
      rsp = (uint8_t)(4u | (ok ? 2u : 1u));
      if (ok) {
        active = true;
        base = a;
        len = l;
        got = 0;
      } else {
        ++mem.refused;
      }
    }
    t.guard_rsp_i = rsp;

    // ---- the write-data port ---------------------------------------------
    ++mem.tick;
    const bool ready = active && ((mem.stall_mask == 0) || ((mem.tick & mem.stall_mask) != 0));
    t.guard_wready_i = ready ? 1 : 0;
    t.eval();
    if (active && ready && t.guard_wvalid_o) {
      for (int k = 0; k < 4; ++k) {
        const uint32_t a = base + got + (uint32_t)k * 2u;
        if (a + 1 < base + len) {
          const uint16_t v = (uint16_t)((t.guard_wdata_o >> (16 * k)) & 0xFFFFu);
          mem.store(a, v);
        }
      }
      got += 8;
      ++mem.beats;
      if (t.guard_wlast_o) active = false;
    }

    zhao::tick(t);
    ++clocks;
  }

  bool offer(const BinTri& b, int guard_max = 40000) {
    t.tri_valid_i = 1;
    t.tri_kx0_i = (uint32_t)b.s.e[0].kx;
    t.tri_ky0_i = (uint32_t)b.s.e[0].ky;
    t.tri_kc0_i = (uint64_t)b.s.e[0].kc;
    t.tri_kx1_i = (uint32_t)b.s.e[1].kx;
    t.tri_ky1_i = (uint32_t)b.s.e[1].ky;
    t.tri_kc1_i = (uint64_t)b.s.e[1].kc;
    t.tri_kx2_i = (uint32_t)b.s.e[2].kx;
    t.tri_ky2_i = (uint32_t)b.s.e[2].ky;
    t.tri_kc2_i = (uint64_t)b.s.e[2].kc;
    t.tri_tl_i =
        (uint8_t)((b.s.e[0].tl ? 1u : 0u) | (b.s.e[1].tl ? 2u : 0u) | (b.s.e[2].tl ? 4u : 0u));
    t.tri_ax_i = (uint32_t)b.ax;
    t.tri_ay_i = (uint32_t)b.ay;
    t.tri_bx_i = (uint32_t)b.bx;
    t.tri_by_i = (uint32_t)b.by;
    t.tri_cx_i = (uint32_t)b.cx;
    t.tri_cy_i = (uint32_t)b.cy;
    t.tri_min_x_i = (uint32_t)b.min_x;
    t.tri_max_x_i = (uint32_t)b.max_x;
    t.tri_min_y_i = (uint32_t)b.min_y;
    t.tri_max_y_i = (uint32_t)b.max_y;
    t.tri_src_id_i = b.src_id;
    int guard = 0;
    for (;;) {
      t.eval();
      const bool taken = t.tri_ready_o != 0;
      step();
      if (taken) break;
      if (++guard >= guard_max) return false;
    }
    t.tri_valid_i = 0;
    return true;
  }

  bool drain(int guard_max = 400000) {
    t.frame_end_i = 1;
    step();
    t.frame_end_i = 0;
    int guard = 0;
    for (;;) {
      t.eval();
      const bool done = t.drain_done_o != 0;
      step();
      if (done) break;
      if (++guard >= guard_max) return false;
    }
    // Let the last tile resolve and its last burst retire.
    for (int i = 0; i < 8192; ++i) step();
    return true;
  }
};

/**
 * The framebuffer the two oracles say this frame must contain.
 *
 * Tiles row-major, triangles within a tile in submission order, ONE resolve per
 * job -- and each resolve's 256 pixels written to their surface addresses, so a
 * tile two triangles share is written twice and the later write wins. That is a
 * framebuffer, and it is the first time this project has expressed the tile
 * pipe's one-job-one-resolve rule as an image.
 */
std::vector<uint16_t> expect_frame(const std::vector<BinTri>& tris, const Look& lk) {
  std::vector<std::vector<zref::Binner::Ref>> refs;
  refs.reserve(tris.size());
  for (const BinTri& t : tris)
    refs.push_back(zref::Binner::bin(t.s, t.min_x, t.max_x, t.min_y, t.max_y));

  // Untouched framebuffer bytes stay zero, matching the modelled VRAM's reset.
  std::vector<uint16_t> fb((size_t)kFbW * kFbH, 0);

  zref::TileStore store;
  zref::EarlyZ ez;
  for (int ty = 0; ty < kGridH; ++ty) {
    for (int tx = 0; tx < kGridW; ++tx) {
      for (size_t i = 0; i < tris.size(); ++i) {
        for (const zref::Binner::Ref& r : refs[i]) {
          if (r.tx != tx || r.ty != ty) continue;
          PipeJob j;
          j.tri.ax = tris[i].ax;
          j.tri.ay = tris[i].ay;
          j.tri.bx = tris[i].bx;
          j.tri.by = tris[i].by;
          j.tri.cx = tris[i].cx;
          j.tri.cy = tris[i].cy;
          j.tx = tx * kPipeTile;
          j.ty = ty * kPipeTile;
          j.fill = lk.fill;
          j.clear = lk.clear;
          j.index = (uint16_t)(((ty & 0x3F) << 6) | (tx & 0x3F));
          j.src = tris[i].src_id;
          j.state = lk.state;
          j.src_a = lk.src_a;
          j.texel_rgb = lk.texel_rgb;
          j.texel_a = lk.texel_a;
          j.texel_idx = lk.texel_idx;
          const PipeExpect e = pipe_oracle(store, ez, j);
          for (int row = 0; row < kPipeTile; ++row)
            for (int col = 0; col < kPipeTile; ++col) {
              const int sx = tx * kPipeTile + col;
              const int sy = ty * kPipeTile + row;
              fb[(size_t)sy * kFbW + sx] = e.res.rgb565[row * kPipeTile + col];
            }
        }
      }
    }
  }
  return fb;
}

/** Compare the modelled VRAM against the expected image, byte-exact. */
uint32_t compare(const Dut& d, const std::vector<uint16_t>& want, const char* name) {
  uint32_t wrong = 0;
  int first_x = -1, first_y = -1;
  for (int y = 0; y < kFbH; ++y)
    for (int x = 0; x < kFbW; ++x) {
      const uint32_t a = kFbBase + (uint32_t)y * kStride + (uint32_t)x * 2u;
      const uint16_t got = d.mem.load(a);
      if (got != want[(size_t)y * kFbW + x]) {
        if (wrong == 0) {
          first_x = x;
          first_y = y;
        }
        ++wrong;
      }
    }
  if (wrong != 0)
    printf("      %s: %u pixels differ, first at (%d,%d) got %04X want %04X\n", name, wrong,
           first_x, first_y,
           d.mem.load(kFbBase + (uint32_t)first_y * kStride + (uint32_t)first_x * 2u),
           want[(size_t)first_y * kFbW + first_x]);
  return wrong;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_probe_render_fb top;

  zref::Clip::Viewport vp;
  vp.w = kFbW;
  vp.h = kFbH;

  Look lk;
  // THE FILL WORD IS A TILESTORE WORD, AND ITS COLOUR IS THE TOP 24 BITS.
  // RASTER.TILESTORE's layout (its own header, charter section 8 order, MSB
  // first) is [63:40] RGB, [39:32] tag, [31:8] depth, [7:0] stencil. The seam
  // test upstream uses 0x000000FF008040C0, whose RGB field is 0x000000 --
  // BLACK, on a clear word that is also black. That is invisible in a CRC
  // comparison and fatal in a picture: the first run of this file passed all
  // seventeen checks against an expected framebuffer that was entirely zero,
  // because both sides agreed on nothing at all.
  //
  // So the fill carries a colour that cannot be confused with the clear, and
  // the checks below refuse a frame that wrote no pixels.
  lk.fill = (0xC04080ull << 40) | (0x11ull << 32) | (0x001000ull << 8) | 0x00ull;
  lk.clear = 0ull;
  lk.state = 0;
  lk.src_a = 0xFF;

  printf("== section 1: one triangle becomes bytes at addresses ==\n");
  {
    std::vector<BinTri> tris;
    BinTri b;
    const bool ok =
        make_bin_tri(px(8), px(8), px(kFbW - 8), px(12), px(20), px(kFbH - 8), vp, 1, &b);
    zhao::check(ok, "the triangle survives CLIP", 1, ok ? 1 : 0);
    if (ok) tris.push_back(b);

    Dut d(top);
    d.reset(lk);
    top.frame_begin_i = 1;
    d.step();
    top.frame_begin_i = 0;
    for (const BinTri& t : tris) zhao::check(d.offer(t), "the binner takes the triangle", 1, 1);
    zhao::check(d.drain(), "the frame drains", 1, 1);

    const std::vector<uint16_t> want = expect_frame(tris, lk);
    // ANTI-VACUITY, FIRST. A framebuffer comparison that passes because both
    // sides are empty is the easiest wrong green in this project, and this
    // file already produced one.
    uint32_t lit = 0;
    for (uint16_t v : want)
      if (v != 0) ++lit;
    zhao::check(lit > 1000, "the expected frame actually contains a triangle", 1,
                lit > 1000 ? 1 : 0);
    zhao::check(top.pixels_written_o > 0, "and the render path wrote pixels at all", 1,
                top.pixels_written_o > 0 ? 1 : 0);
    zhao::check(compare(d, want, "section 1") == 0,
                "every pixel of the frame is in memory, at the right address", 1, 1);
    zhao::check(top.fb_stream_error_o == 0, "the pixel stream was contiguous within every row", 0,
                (uint32_t)top.fb_stream_error_o);
    zhao::check(d.mem.refused == 0, "no write was addressed outside the granted framebuffer", 0,
                d.mem.refused);
    zhao::check(top.fragment_error_o == 0, "no fragment error", 0, (uint32_t)top.fragment_error_o);
    zhao::check(top.overflow_o == 0, "the arena did not overflow", 0, (uint32_t)top.overflow_o);
    printf("   MEASURED: %u pixels in %u bursts, %ld clocks, %u job stalls, %u fb stalls\n",
           (uint32_t)top.pixels_written_o, (uint32_t)top.bursts_issued_o, d.clocks,
           (uint32_t)top.job_stall_clocks_o, (uint32_t)top.fb_stall_clocks_o);
    printf("   SEAM: %u pixel-clocks offered, %u taken\n", (uint32_t)top.px_offered_o,
           (uint32_t)top.px_taken_o);
  }

  printf("== section 2: two overlapping triangles, and the later write wins ==\n");
  {
    std::vector<BinTri> tris;
    BinTri a, b;
    if (make_bin_tri(px(4), px(4), px(90), px(10), px(10), px(70), vp, 1, &a)) tris.push_back(a);
    if (make_bin_tri(px(20), px(20), px(100), px(30), px(30), px(80), vp, 2, &b)) tris.push_back(b);
    zhao::check(tris.size() == 2, "both triangles survive CLIP", 2, (uint32_t)tris.size());

    Dut d(top);
    d.reset(lk);
    top.frame_begin_i = 1;
    d.step();
    top.frame_begin_i = 0;
    for (const BinTri& t : tris) zhao::check(d.offer(t), "the binner takes it", 1, 1);
    zhao::check(d.drain(), "the frame drains", 1, 1);

    const std::vector<uint16_t> want = expect_frame(tris, lk);
    zhao::check(compare(d, want, "section 2") == 0,
                "the composed image matches the oracles applied in job order", 1, 1);
    zhao::check(top.fb_stream_error_o == 0, "the stream stayed contiguous", 0,
                (uint32_t)top.fb_stream_error_o);
    printf("   MEASURED: %u pixels in %u bursts, %u jobs\n", (uint32_t)top.pixels_written_o,
           (uint32_t)top.bursts_issued_o, (uint32_t)top.jobs_taken_o);
  }

  printf("== section 3: slow memory stalls the whole render path, and loses nothing ==\n");
  {
    // The reason the seam is a ready/valid wire. FBWRITE holds `px_ready_o` low
    // while a burst is in flight, which stalls RESOLVE, the tile store, the
    // fragment path and finally the binner's drain. Neither block's own bench
    // can produce that: each faces a partner that is always ready.
    std::vector<BinTri> tris;
    BinTri b;
    if (make_bin_tri(px(6), px(6), px(110), px(20), px(24), px(84), vp, 7, &b)) tris.push_back(b);

    Dut d(top);
    d.reset(lk);
    d.mem.stall_mask = 3;  // memory refuses write data 1 clock in 4
    top.frame_begin_i = 1;
    d.step();
    top.frame_begin_i = 0;
    for (const BinTri& t : tris) (void)d.offer(t);
    zhao::check(d.drain(), "the frame drains even with memory stalling", 1, 1);

    const std::vector<uint16_t> want = expect_frame(tris, lk);
    zhao::check(compare(d, want, "section 3") == 0, "and every pixel still landed correctly", 1, 1);
    zhao::check(top.fb_stream_error_o == 0, "with the stream still contiguous", 0,
                (uint32_t)top.fb_stream_error_o);
    printf("   MEASURED: %u pixels in %u bursts, %ld clocks, %u fb stalls\n",
           (uint32_t)top.pixels_written_o, (uint32_t)top.bursts_issued_o, d.clocks,
           (uint32_t)top.fb_stall_clocks_o);
  }

  return zhao::report_and_exit("render_fb_directed");
}
