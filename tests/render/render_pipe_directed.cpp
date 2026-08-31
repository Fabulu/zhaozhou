// render_pipe_directed.cpp — the GEOMETRY -> RASTER seam, end to end.
//
// ---------------------------------------------------------------------------
// WHAT IS ACTUALLY AT RISK
// ---------------------------------------------------------------------------
// Both halves are already green on their own. `zhao_geom_binner` has directed
// and random tests against `zref::Binner`; `zhao_raster_tile_pipe` has the same
// against the five raster oracles composed. Neither has ever been driven by the
// other.
//
// So this file is not re-testing binning or rasterising. It is testing the two
// things only a composition can be wrong about:
//
//   1. EVERY JOB THE BINNER EMITS IS RENDERED, in the order it emitted them.
//      A job dropped at the seam is invisible to both block tests -- the binner
//      would still have emitted it and the pipe would still render whatever it
//      was given.
//
//   2. BACK-PRESSURE IS HONOURED IN BOTH DIRECTIONS. Alone, each block faced a
//      bench that was always ready, or that refused on a schedule the bench
//      chose. Wired together the binner stalls because the pipe is genuinely
//      busy with a tile, which is the only configuration that exercises that
//      path at all.
//
// The Field engine has already paid for this distinction three times over: a
// composed machine found a silently dropped register write, a stale-operand
// hand-over and a counter that could not observe the thing it counted -- every
// one of them in blocks whose own tallies were green.
//
// ---------------------------------------------------------------------------
// THE ORACLE IS THE TWO EXISTING ONES, IN SERIES
// ---------------------------------------------------------------------------
// `zref::Binner` says which tiles a triangle touches and in what order.
// `pipe_oracle` says what one tile must look like once a job has been applied
// to it. Running the second over the first's answer is the whole expectation,
// and neither is re-derived here -- a composition that invented its own
// arithmetic would be testing that invention rather than the machine.
//
// ---------------------------------------------------------------------------
// WHAT BRINGING THEM UP TOGETHER ACTUALLY FOUND
// ---------------------------------------------------------------------------
// Three things, none of them visible from either bench, all of them found by
// the first execution of this file rather than by reading either block:
//
// * THE TILE COORDINATE IS A PIXEL ORIGIN, NOT A COLUMN. `job_tile_x_o` is the
//   tile's top-left PIXEL and both blocks agree about that, so the seam itself
//   is right -- but any CRC index a composition FORMS from it is in the wrong
//   units unless it shifts first. `zhao_geom_bin_pipe` gets this right and
//   always has; the trap was found in a duplicate composition written for this
//   test before its author checked whether one already existed, and that
//   duplicate has been deleted. Recorded because the next composition will meet
//   the same units, not because the console ever shipped it wrong.
//
// * THE TILE LEDGER IS NOT READY AT THE LAST PIXEL. `tile_crc_o` and its index
//   become the finishing tile's on `tile_done_o`, which trails `fb_last_o`.
//   Sampling them at the last framebuffer beat pairs THIS tile's pixels with
//   the PREVIOUS tile's ledger, and because the coverage count is re-derived
//   per tile it still looked right -- only the index was one behind, on 30 of
//   31 tiles.
//
// * ONE RESOLVE PER JOB, SO A SHARED TILE IS RENDERED TWICE. The pipe declares
//   it ("one job = one clear + one triangle + one resolve", no multi-triangle
//   accumulation), and the consequence only appears once a binner is upstream:
//   two triangles over one tile produce two full 256-pixel framebuffer streams,
//   and the second stream's CLEAR word overwrites the first triangle wherever
//   the second does not cover it. Section 2 measures that, and it is stated in
//   reports/RENDER_SEAM_FINDINGS.md rather than silently accepted, because it
//   is a property of the FRAME that neither block owns.
//
// The composition itself is `zhao_geom_bin_pipe`, which has joined these two
// since phase 5. This file did not build a new one -- it added five counters to
// that one (`jobs_taken_o`, `job_stall_clocks_o`, `arena_full_o`,
// `early_z_rejects_o`, `fragment_error_o`) and drives it with three frames its
// own directed test does not: many tiles, OVERLAPPING triangles, and a
// framebuffer sink that refuses.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_geom_bin_pipe.h"

#include "zhao_sim.hpp"

// The tile pipe's oracle without its model: this target's top is the
// composition, and only one Verilated model exists per target.
#define ZHAO_PIPE_DEV_ORACLE_ONLY
#define ZHAO_GEOM_DEV_ORACLE_ONLY
#define ZHAO_GEOM_DEV_BINNER
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
using zhao_raster::PipeTile;
using zhao_raster::px;  // 8 fractional bits: the screen coordinate the whole chain speaks

constexpr int kGridW = 8;
constexpr int kGridH = 6;

/** Appearance the command stream will own. Held constant for the whole frame
 *  here, because inventing a command front end inside a seam test would be
 *  inventing game behaviour -- and that is the owner's call. */
struct Look {
  uint64_t fill = 0;
  uint64_t clear = 0;
  uint32_t state = 0;
  uint8_t src_a = 0xFF;
  uint32_t texel_rgb = 0;
  uint8_t texel_a = 0xFF;
  uint8_t texel_idx = 0;
};

struct Dut {
  Vzhao_geom_bin_pipe& t;
  std::vector<PipeTile> tiles;
  PipeTile cur;
  int beat = 0;
  bool pixels_done = false;
  long clocks = 0;

  explicit Dut(Vzhao_geom_bin_pipe& top) : t(top) {}

  void reset(const Look& lk) {
    t.rst_n = 0;
    t.frame_begin_i = 0;
    t.frame_end_i = 0;
    t.grid_w_i = kGridW;
    t.grid_h_i = kGridH;
    t.tri_valid_i = 0;
    t.tok_grant_i = 1;
    t.fb_ready_i = 1;
    t.job_fill_word_i = lk.fill;
    t.job_clear_word_i = lk.clear;
    t.job_state_i = lk.state;
    t.job_src_a_i = lk.src_a;
    t.job_texel_rgb_i = lk.texel_rgb;
    t.job_texel_a_i = lk.texel_a;
    t.job_texel_idx_i = lk.texel_idx;
    t.eval();
    for (int i = 0; i < 4; ++i) zhao::tick(t);
    t.rst_n = 1;
    t.eval();
    zhao::tick(t);
    tiles.clear();
    beat = 0;
    pixels_done = false;
    clocks = 0;
  }

  /** One clock. The framebuffer stream is collected here and nowhere else, so
   *  a beat cannot be counted twice or missed by a second tick site. */
  void step() {
    t.eval();
    if (t.fb_valid_o && t.fb_ready_i) {
      if (beat < kPipePixels) {
        cur.rgb565[beat] = (uint16_t)t.fb_rgb565_o;
        cur.tag[beat] = (uint8_t)t.fb_tag_o;
        cur.x[beat] = (int32_t)(int16_t)t.fb_x_o;
        cur.y[beat] = (int32_t)(int16_t)t.fb_y_o;
      }
      ++beat;
      if (t.fb_last_o) {
        pixels_done = true;
        beat = 0;
      }
    }
    // THE LEDGER IS NOT READY AT THE LAST PIXEL. `tile_crc_o`, its index and
    // the coverage count are registered by RASTER.RESOLVE and only become this
    // tile's on `tile_done_o`, which trails `fb_last_o`. Reading them at the
    // last beat gives the PREVIOUS tile's ledger against this tile's pixels --
    // and because the count happens to be re-derived per tile it looked
    // right, while every index in the frame was one tile behind.
    if (t.tile_done_o && pixels_done) {
      cur.crc32c = (uint32_t)t.tile_crc_o;
      cur.crc_index = (uint16_t)t.tile_crc_index_o;
      cur.count = (uint32_t)t.tile_cov_count_o;
      cur.degenerate = t.tile_degenerate_o != 0;
      tiles.push_back(cur);
      cur = PipeTile{};
      pixels_done = false;
    }
    zhao::tick(t);
    ++clocks;
  }

  /** Offer one triangle, waiting for the binner's ready. */
  bool offer(const BinTri& b, int guard_max = 20000) {
    t.tri_valid_i = 1;
    // The three edges as GEOM.SETUP emits them, and the top-left mask packed
    // one bit per edge in the same order -- which is the order the binner and
    // the edge walker both read it in.
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
      const bool took = t.tri_ready_o != 0;
      step();
      if (took) break;
      if (++guard >= guard_max) {
        t.tri_valid_i = 0;
        return false;
      }
    }
    t.tri_valid_i = 0;
    t.eval();
    return true;
  }

  /** Run the frame out: end, then drain until the binner says it is done. */
  bool drain(int guard_max = 200000) {
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
    // Let the last tile finish resolving out of the pipe.
    for (int i = 0; i < 4096; ++i) step();
    return true;
  }
};

/**
 * `zhao_geom_bin_pipe`'s law 2, restated where the expectation is built: the
 * CRC index is the tile's own grid position PACKED as {row[5:0], col[5:0]},
 * not a row-major product and not a counter. Deriving it here from the tile
 * coordinates -- rather than copying the RTL's expression -- keeps the test an
 * independent statement of the same law.
 */
uint16_t tile_index(int tx, int ty) { return (uint16_t)(((ty & 0x3F) << 6) | (tx & 0x3F)); }

/**
 * What the two oracles, in series, say the frame must be.
 *
 * ONE RESOLVE PER JOB, NOT PER TILE. `zhao_raster_tile_pipe` declares it at
 * its own head -- "no multi-triangle accumulation into one tile (one job =
 * one clear + one triangle + one resolve)" -- so a tile two triangles both
 * reference is rendered TWICE and streamed to the framebuffer twice, and the
 * second stream's clear word overwrites the first triangle's pixels wherever
 * the second does not cover them. That is the shipped contract, and this
 * function models it rather than the tile-accumulating machine one might
 * expect: collapsing the jobs per tile is what made the first run of this gate
 * report 40 beats against 29, which was the ORACLE being wrong about the
 * design, not the design being wrong.
 *
 * Recorded as a seam property in reports/, because it is invisible from either
 * block's own bench: the binner does not know a tile gets two jobs and the
 * pipe does not know the two jobs are the same tile.
 *
 * The store and the early-Z model are carried across the WHOLE frame, one of
 * each, because that is how many the RTL has. A fresh model per tile would
 * hide a swap or a depth-floor that failed to reset.
 */
std::vector<PipeTile> expect_frame(const std::vector<BinTri>& tris, const Look& lk) {
  std::vector<std::vector<zref::Binner::Ref>> refs;
  refs.reserve(tris.size());
  for (const BinTri& t : tris)
    refs.push_back(zref::Binner::bin(t.s, t.min_x, t.max_x, t.min_y, t.max_y));

  zref::TileStore store;
  zref::EarlyZ ez;
  std::vector<PipeTile> out;

  // The binner's declared drain order: tiles row-major over the grid, and
  // within one tile the triangles in submission order.
  for (int ty = 0; ty < kGridH; ++ty) {
    for (int tx = 0; tx < kGridW; ++tx) {
      // ONE RESOLVE PER TILE. The pipe clears the bank on the tile's FIRST
      // reference and resolves on its LAST, so a tile several triangles share
      // is rendered once with all of them in it. Collect the tile's references
      // before running any, because the oracle needs to know which is last.
      std::vector<size_t> mine;
      for (size_t i = 0; i < tris.size(); ++i)
        for (const zref::Binner::Ref& r : refs[i])
          if (r.tx == tx && r.ty == ty) mine.push_back(i);
      if (mine.empty()) continue;

      uint32_t tile_cov = 0;
      for (size_t k = 0; k < mine.size(); ++k) {
        {
          const size_t i = mine[k];
          const bool first = (k == 0);
          const bool last = (k + 1 == mine.size());
          PipeJob j;
          // The edge walker takes VERTICES, not the setup coefficients -- it
          // re-derives its own edges. BinTri carries the winding-normalised
          // triangle GEOM.CLIP emitted, which is the one SETUP was run on.
          j.tri.ax = tris[i].ax;
          j.tri.ay = tris[i].ay;
          j.tri.bx = tris[i].bx;
          j.tri.by = tris[i].by;
          j.tri.cx = tris[i].cx;
          j.tri.cy = tris[i].cy;
          // Pixel origin, matching the binner's port and the pipe's input --
          // the tile COLUMN is only ever used to form the CRC index.
          j.tx = tx * kPipeTile;
          j.ty = ty * kPipeTile;
          j.fill = lk.fill;
          j.clear = lk.clear;
          j.index = tile_index(tx, ty);
          j.src = tris[i].src_id;
          j.state = lk.state;
          j.src_a = lk.src_a;
          j.texel_rgb = lk.texel_rgb;
          j.texel_a = lk.texel_a;
          j.texel_idx = lk.texel_idx;
          const PipeExpect e = pipe_oracle(store, ez, j, first, last);
          tile_cov += e.count;
          if (!last) continue;

          PipeTile pt;
          for (int p = 0; p < kPipePixels; ++p) {
            pt.rgb565[p] = e.res.rgb565[p];
            pt.tag[p] = e.res.tag[p];  // never dithered, and compared
          }
          pt.crc32c = e.res.crc32c;
          pt.crc_index = j.index;
          // The tile pipe reports the coverage of the TILE, accumulated over
          // its triangles, not of the last one -- so the expectation sums the
          // same way.
          pt.count = tile_cov;
          pt.degenerate = e.degenerate;
          out.push_back(pt);
        }
      }
    }
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_geom_bin_pipe top;

  zref::Clip::Viewport vp;
  vp.w = kGridW * 16;
  vp.h = kGridH * 16;

  Look lk;
  lk.fill = 0x0000'00FF'0080'40C0ull;  // a recognisable source word
  lk.clear = 0ull;
  lk.state = 0;
  lk.src_a = 0xFF;

  printf("== section 1: one triangle spanning several tiles ==\n");
  {
    std::vector<BinTri> tris;
    BinTri b;
    const bool ok = make_bin_tri(px(8), px(8), px(kGridW * 16 - 8), px(12), px(20),
                                 px(kGridH * 16 - 8), vp, 1, &b);
    zhao::check(ok, "the triangle survives CLIP", 1, ok ? 1 : 0);
    if (ok) tris.push_back(b);

    Dut d(top);
    d.reset(lk);
    top.frame_begin_i = 1;
    d.step();
    top.frame_begin_i = 0;
    for (const BinTri& t : tris) zhao::check(d.offer(t), "the binner takes the triangle", 1, 1);
    zhao::check(d.drain(), "the frame drains", 1, 1);

    const std::vector<PipeTile> want = expect_frame(tris, lk);
    zhao::check(d.tiles.size() == want.size(), "every referenced tile is rendered, and no others",
                (uint32_t)want.size(), (uint32_t)d.tiles.size());

    const size_t n = d.tiles.size() < want.size() ? d.tiles.size() : want.size();
    uint32_t wrong = 0;
    for (size_t i = 0; i < n; ++i) {
      if (!d.tiles[i].same_picture(want[i])) ++wrong;
      if (d.tiles[i].crc_index != want[i].crc_index) ++wrong;
    }
    if (wrong && getenv("ZHAO_RENDER_DUMP")) {
      for (size_t i = 0; i < n; ++i)
        printf("      [%2zu] rtl idx=%3u count=%3u crc=%08X | want idx=%3u count=%3u crc=%08X%s\n",
               i, d.tiles[i].crc_index, d.tiles[i].count, d.tiles[i].crc32c, want[i].crc_index,
               want[i].count, want[i].crc32c,
               d.tiles[i].same_picture(want[i]) ? "" : "  <-- picture");
    }
    zhao::check(wrong == 0, "each tile matches the two oracles in series, in order", 0, wrong);
    zhao::check(top.fragment_error_o == 0, "no fragment error", 0, (uint32_t)top.fragment_error_o);
    zhao::check(top.overflow_o == 0, "the arena did not overflow", 0, (uint32_t)top.overflow_o);
    printf("   MEASURED: %zu tiles in %ld clocks, %u job stalls\n", d.tiles.size(), d.clocks,
           (uint32_t)top.job_stall_clocks_o);
    // THE ARENA, WHICH NOTHING WAS READING. GEOM.BINNER reports its own
    // high-water marks and every one of them was tied off. The measured frame
    // in reports/BINNER_CAPACITY_FOR_8KM_MAPS.md is 150x the triangle budget
    // and 25x the reference arena, so these stop being trivia the moment the
    // capacities move -- and a capacity change nobody measures here is a
    // capacity change nobody checked.
    printf("   ARENA: %u references, deepest tile list %u, %u triangles culled\n",
           (uint32_t)top.tile_references_o, (uint32_t)top.max_tile_list_depth_o,
           (uint32_t)top.triangles_culled_o);
    zhao::check(top.triangles_culled_o == 0, "no triangle was culled, so the arena held this frame",
                0, (uint32_t)top.triangles_culled_o);
  }

  printf("== section 2: overlapping triangles share tiles, and ORDER decides the picture ==\n");
  {
    // Two triangles over the same ground. If the seam reordered them -- or
    // dropped one -- the depth and blend results would differ, which a
    // per-tile CRC catches and a per-block test cannot.
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

    const std::vector<PipeTile> want = expect_frame(tris, lk);
    zhao::check(d.tiles.size() == want.size(), "tile count matches", (uint32_t)want.size(),
                (uint32_t)d.tiles.size());
    const size_t n = d.tiles.size() < want.size() ? d.tiles.size() : want.size();
    uint32_t wrong = 0;
    for (size_t i = 0; i < n; ++i)
      if (!d.tiles[i].same_picture(want[i]) || d.tiles[i].crc_index != want[i].crc_index) ++wrong;
    zhao::check(wrong == 0, "overlapping tiles match, so the ORDER survived the seam", 0, wrong);
    printf("   MEASURED: %zu tiles, %u jobs taken, %u job stalls\n", d.tiles.size(),
           (uint32_t)top.jobs_taken_o, (uint32_t)top.job_stall_clocks_o);
  }

  printf("== section 3: a slow consumer must not lose a job ==\n");
  {
    // The framebuffer sink refuses on a pseudo-random schedule, so the resolve
    // stage back-pressures the tile store, which back-pressures the pipe, which
    // back-pressures the binner. That chain is the entire reason to compose
    // these two, and with a bench on each side it cannot be built at all.
    std::vector<BinTri> tris;
    BinTri b;
    if (make_bin_tri(px(6), px(6), px(110), px(20), px(24), px(84), vp, 7, &b)) tris.push_back(b);

    Dut d(top);
    d.reset(lk);
    top.frame_begin_i = 1;
    d.step();
    top.frame_begin_i = 0;
    for (const BinTri& t : tris) (void)d.offer(t);

    // Drain by hand so the sink can stutter.
    top.frame_end_i = 1;
    d.step();
    top.frame_end_i = 0;
    uint32_t lfsr = 0xACE1u;
    int guard = 0;
    for (;;) {
      // Unsigned negation of the tap bit, NOT `-(int32_t)(lfsr & 1u)`. The
      // signed form makes 0 or -1 and then relies on the int32 bit pattern,
      // which cppcheck flags as a signed overflow; `0u - bit` is defined
      // wraparound and yields the identical 0x00000000 / 0xFFFFFFFF mask, so
      // the sequence this drives is bit-for-bit the same.
      lfsr = (uint32_t)((lfsr >> 1) ^ ((0u - (lfsr & 1u)) & 0xB400u));
      top.fb_ready_i = (lfsr & 3u) != 0;
      top.eval();
      const bool done = top.drain_done_o != 0;
      d.step();
      if (done) break;
      if (++guard >= 400000) break;
    }
    top.fb_ready_i = 1;
    for (int i = 0; i < 8192; ++i) d.step();

    const std::vector<PipeTile> want = expect_frame(tris, lk);
    zhao::check(d.tiles.size() == want.size(), "a stuttering sink loses no tile",
                (uint32_t)want.size(), (uint32_t)d.tiles.size());
    const size_t n = d.tiles.size() < want.size() ? d.tiles.size() : want.size();
    uint32_t wrong = 0;
    for (size_t i = 0; i < n; ++i)
      if (!d.tiles[i].same_picture(want[i])) ++wrong;
    zhao::check(wrong == 0, "and changes no pixel of any of them", 0, wrong);
    printf("   MEASURED: %u job stalls under a refusing sink\n", (uint32_t)top.job_stall_clocks_o);
  }

  return zhao::report_and_exit("render_pipe_directed");
}
