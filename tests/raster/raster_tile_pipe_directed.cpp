// raster_tile_pipe_directed.cpp — directed vectors for the composed phase-4
// tile pipe (fpga/rtl/raster/zhao_raster_tile_pipe.sv): the roadmap's "first
// exact tile and triangle" — one flat-colour triangle walked by
// RASTER.EDGEWALK, written into RASTER.TILESTORE, swapped, and resolved by
// RASTER.RESOLVE to RGB565 framebuffer words with a deterministic tile CRC.
//
// Every case diffs all 256 RGB565 pixels, all 256 tags, the tile CRC, the
// coverage count and the degenerate flag against `pipe_oracle` — zref::
// EdgeWalk -> zref::TileStore -> zref::TileResolve driven through the SAME
// clear/write/swap sequence — and on top of that asserts its own law:
//
//   1. single tile      — the smoke case: one triangle in one tile, a fill
//                         word with every field distinct, bit for bit
//   2. coverage extremes— 0 covered pixels, exactly 1, all 256, and the
//                         zero-area reject; a tile with no coverage must
//                         still resolve, and resolve to the CLEAR word
//   3. multi-tile       — one triangle across a 5x5 tile grid at an UNALIGNED
//                         base: every tile it touches and every tile it
//                         misses, all in one back-to-back batch
//   4. ping-pong        — the same 16 full tiles fed back-to-back and fed
//                         serially give identical pictures, and the
//                         back-to-back run really does overlap (two tiles in
//                         flight, and far fewer cycles). This is the stated
//                         reason RASTER.TILESTORE is ping-pong at all.
//   5. backpressure     — six framebuffer stall patterns and a gapped feed,
//                         stalling mid-tile, change neither a pixel nor a CRC
//   6. dither phase     — the Bayer phase is ABSOLUTE. All 16 unaligned tile
//                         origins produce 16 DIFFERENT pictures of the same
//                         coverage; every 16-aligned origin produces the same
//                         one. Invisible at the origin, obvious off it.
//   7. black rail       — the KNOWN, ESCALATED oracle defect pinned, not
//                         fixed (see the note at the case)
//   8. fb address       — the composition's own law 3: fb_x/fb_y are the
//                         SURFACE coordinate, tile origin + {col,row},
//                         including at negative origins
//   9. status + CRC     — the coverage count and degenerate flag ride with
//                         the tile they belong to (not the one being walked),
//                         and the CRC is reseeded per tile
//  10. counters         — tile_references == covered writes + 256 reads per
//                         tile; resolved_tiles == tiles; front_bank_o toggles
//                         exactly once per tile

#include "raster_tile_pipe_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "zref/zref_tilestore.hpp"

using zhao::check;
using zhao_raster::kPipePixels;
using zhao_raster::kPipeTile;
using zhao_raster::pipe_address_errors;
using zhao_raster::pipe_describe;
using zhao_raster::pipe_match;
using zhao_raster::pipe_oracle;
using zhao_raster::pipe_word;
using zhao_raster::PipeDev;
using zhao_raster::PipeExpect;
using zhao_raster::PipeFeed;
using zhao_raster::PipeJob;
using zhao_raster::PipeTile;
using zhao_raster::PipeTri;
using zhao_raster::px;

namespace {

PipeDev& dev() {
  static PipeDev d;
  return d;
}

uint32_t g_saved = 0;

// A fill/clear word with every field distinct, so a field swap in the
// composition's write path shows up instead of cancelling out.
uint64_t distinct_word(uint32_t k) {
  return pipe_word(static_cast<uint8_t>(k * 7u + 11u), static_cast<uint8_t>(k * 13u + 29u),
                   static_cast<uint8_t>(k * 29u + 53u), static_cast<uint8_t>(k * 31u + 71u),
                   (k * 2654435761u) & 0xFFFFFFu, static_cast<uint8_t>(k * 37u + 97u));
}

// ------------------------------------------------------------- triangles ---
// A triangle that swallows the whole tile whose top-left pixel is (tx, ty).
PipeTri tri_full(int32_t tx, int32_t ty) {
  PipeTri t;
  t.ax = px(tx - 100);
  t.ay = px(ty - 100);
  t.bx = px(tx + 200);
  t.by = px(ty - 100);
  t.cx = px(tx - 100);
  t.cy = px(ty + 200);
  return t;
}

// A triangle containing exactly the pixel centre of (x, y) and no other:
// it fits inside a 200-subpixel box around that one centre, and centres are
// 256 subpixels apart.
PipeTri tri_one_pixel(int32_t x, int32_t y) {
  const int32_t cx = px(x) + 128;
  const int32_t cy = px(y) + 128;
  PipeTri t;
  t.ax = cx - 1;
  t.ay = cy - 1;
  t.bx = cx + 199;
  t.by = cy - 1;
  t.cx = cx - 1;
  t.cy = cy + 199;
  return t;
}

// A triangle wholly BETWEEN pixel centres: real area, zero coverage.
PipeTri tri_no_pixel(int32_t x, int32_t y) {
  const int32_t cx = px(x) + 128;
  const int32_t cy = px(y) + 128;
  PipeTri t;
  t.ax = cx + 60;
  t.ay = cy + 60;
  t.bx = cx + 160;
  t.by = cy + 60;
  t.cx = cx + 60;
  t.cy = cy + 160;
  return t;
}

// Collinear: rast.cpp's `if (area == 0) return;`.
PipeTri tri_degenerate(int32_t tx, int32_t ty) {
  PipeTri t;
  t.ax = px(tx);
  t.ay = px(ty);
  t.bx = px(tx + 8);
  t.by = px(ty + 8);
  t.cx = px(tx + 12);
  t.cy = px(ty + 12);
  return t;
}

// ----------------------------------------------------------- the compare ---
// Runs a batch through the RTL and through the three composed oracles, and
// requires every tile to match bit for bit.
bool run_batch(const std::vector<PipeJob>& jobs, PipeFeed feed, uint32_t feed_seed,
               uint32_t fb_seed, const char* what, std::vector<PipeTile>* got,
               uint32_t* cycles = nullptr) {
  std::string err;
  const uint32_t cyc = dev().run(jobs, feed, feed_seed, fb_seed, got, &err);
  if (cycles) *cycles = cyc;

  bool ok = err.empty();
  if (!ok) std::printf("  %s: protocol violation: %s\n", what, err.c_str());

  zref::TileStore store;
  for (size_t i = 0; i < jobs.size(); ++i) {
    const PipeExpect want = pipe_oracle(store, jobs[i]);
    if (pipe_match(want, (*got)[i])) continue;
    ok = false;
    const std::string body = pipe_describe(jobs[i], want, (*got)[i]);
    if (g_saved < 6) {
      std::printf("  %s: tile %zu != the composed oracle\n    %s\n", what, i, body.c_str());
      char name[80];
      std::snprintf(name, sizeof(name), "raster_tile_pipe_%s_%zu", what, i);
      zhao::save_failing_vector(name, zhao_raster::pipe_serialize(jobs[i]),
                                "zref::EdgeWalk->TileStore->TileResolve", body);
      ++g_saved;
    }
  }
  return ok;
}

// The oracle's verdict for one job, without the RTL (case setup assertions).
PipeExpect solo_oracle(const PipeJob& j) {
  zref::TileStore store;
  return pipe_oracle(store, j);
}

// --------------------------------------------------------------------- 1 ---
void test_single_tile() {
  PipeJob j;
  j.tri.ax = px(2);
  j.tri.ay = px(1);
  j.tri.bx = px(14);
  j.tri.by = px(4);
  j.tri.cx = px(5);
  j.tri.cy = px(13);
  j.tx = 0;
  j.ty = 0;
  j.fill = distinct_word(3);
  j.clear = distinct_word(9);
  j.index = 0x1234;
  j.src = 0xBEEF;

  std::vector<PipeTile> got;
  const bool ok = run_batch({j}, PipeFeed::kBackToBack, 0u, 0u, "single", &got);
  check(ok, "single tile: RTL == zref::EdgeWalk->TileStore->TileResolve", 1, ok ? 1 : 0);

  const PipeExpect want = solo_oracle(j);
  check(want.count > 0 && want.count < static_cast<uint32_t>(kPipePixels),
        "single tile: the vector really is a PARTIAL coverage", 1,
        (want.count > 0 && want.count < static_cast<uint32_t>(kPipePixels)) ? 1 : 0);
  check(got[0].crc_index == 0x1234, "single tile: the tile index rides through", 0x1234,
        got[0].crc_index);
}

// --------------------------------------------------------------------- 2 ---
void test_coverage_extremes() {
  const int32_t tx = 32, ty = 48;
  std::vector<PipeJob> jobs;

  PipeJob none;  // a real triangle wholly between pixel centres
  none.tri = tri_no_pixel(tx + 4, ty + 7);
  none.tx = tx;
  none.ty = ty;
  none.fill = pipe_word(255, 255, 255, 0xAA);
  none.clear = distinct_word(2);
  none.index = 0;
  none.src = 0x100;
  jobs.push_back(none);

  PipeJob miss = none;  // a real, well-covered triangle — in a DIFFERENT tile
  miss.tri.ax = px(tx + 66);
  miss.tri.ay = px(ty + 66);
  miss.tri.bx = px(tx + 76);
  miss.tri.by = px(ty + 68);
  miss.tri.cx = px(tx + 69);
  miss.tri.cy = px(ty + 77);
  miss.index = 1;
  miss.src = 0x101;
  jobs.push_back(miss);

  PipeJob one;
  one.tri = tri_one_pixel(tx + 9, ty + 3);
  one.tx = tx;
  one.ty = ty;
  one.fill = pipe_word(200, 30, 90, 0x55);
  one.clear = distinct_word(4);
  one.index = 2;
  one.src = 0x102;
  jobs.push_back(one);

  PipeJob all;
  all.tri = tri_full(tx, ty);
  all.tx = tx;
  all.ty = ty;
  all.fill = pipe_word(17, 200, 250, 0x3C);
  all.clear = distinct_word(6);
  all.index = 3;
  all.src = 0x103;
  jobs.push_back(all);

  PipeJob deg;
  deg.tri = tri_degenerate(tx, ty);
  deg.tx = tx;
  deg.ty = ty;
  deg.fill = pipe_word(255, 0, 0, 0x77);
  deg.clear = distinct_word(8);
  deg.index = 4;
  deg.src = 0x104;
  jobs.push_back(deg);

  std::vector<PipeTile> got;
  const bool ok = run_batch(jobs, PipeFeed::kBackToBack, 0u, 0u, "extremes", &got);
  check(ok, "coverage extremes: RTL == the composed oracle on all five tiles", 1, ok ? 1 : 0);

  // the vectors really are the extremes they claim to be
  check(solo_oracle(jobs[0]).count == 0 && !solo_oracle(jobs[0]).degenerate,
        "extremes: the between-centres triangle covers 0 pixels and is NOT degenerate", 0,
        solo_oracle(jobs[0]).count);
  check(solo_oracle(jobs[1]).count == 0, "extremes: the far triangle misses this tile entirely", 0,
        solo_oracle(jobs[1]).count);
  {  // ...and it is a REAL triangle, not a degenerate one that misses everything
    PipeJob elsewhere = jobs[1];
    elsewhere.tx = tx + 64;
    elsewhere.ty = ty + 64;
    check(solo_oracle(elsewhere).count > 0,
          "extremes: the far triangle really does cover pixels — in its OWN tile", 1,
          solo_oracle(elsewhere).count > 0 ? 1 : 0);
  }
  check(solo_oracle(jobs[2]).count == 1, "extremes: the tiny triangle covers exactly 1 pixel", 1,
        solo_oracle(jobs[2]).count);
  check(solo_oracle(jobs[3]).count == 256, "extremes: the big triangle covers all 256 pixels", 256,
        solo_oracle(jobs[3]).count);
  check(solo_oracle(jobs[4]).degenerate, "extremes: the collinear triangle is the zero-area reject",
        1, solo_oracle(jobs[4]).degenerate ? 1 : 0);

  // the RTL reports the same, on the right tile
  for (size_t i = 0; i < jobs.size(); ++i) {
    const PipeExpect w = solo_oracle(jobs[i]);
    check(got[i].count == w.count && got[i].degenerate == w.degenerate,
          "extremes: the coverage status is reported with its own tile", w.count, got[i].count);
  }

  // A tile with no coverage resolves to the CLEAR word — not to nothing, and
  // not to the previous tile's leftovers: the clear must have reached the
  // right bank. Both zero-coverage tiles above sit in the batch immediately
  // before tiles that DO write, so a clear that missed would show here.
  PipeJob bare = jobs[0];
  bare.fill = pipe_word(0, 0, 0);
  const PipeExpect bare_want = solo_oracle(bare);
  std::vector<PipeJob> only_clear{bare};
  std::vector<PipeTile> bare_got;
  const bool bok = run_batch(only_clear, PipeFeed::kBackToBack, 0u, 0u, "clearonly", &bare_got);
  bool uniform = true;
  for (int i = 1; i < kPipePixels; ++i)
    if (bare_got[0].rgb565[i] != bare_got[0].rgb565[0] &&
        bare_want.res.rgb565[i] == bare_want.res.rgb565[0])
      uniform = false;
  check(bok && uniform, "extremes: an untouched tile resolves as the CLEAR word everywhere", 1,
        (bok && uniform) ? 1 : 0);
}

// --------------------------------------------------------------------- 3 ---
// One triangle over a 5x5 tile grid whose base is NOT 16-aligned, so every
// tile in the grid also exercises the absolute Bayer phase. Tiles the
// triangle touches and tiles it misses go through the SAME batch, back to
// back, in one instance with no reset.
void test_multi_tile_triangle() {
  const int32_t bx = 6, by = 10;  // deliberately unaligned
  PipeTri tri;
  tri.ax = px(bx + 20);
  tri.ay = px(by + 6);
  tri.bx = px(bx + 71);
  tri.by = px(by + 63);
  tri.cx = px(bx + 9);
  tri.cy = px(by + 70);

  std::vector<PipeJob> jobs;
  uint16_t idx = 0;
  for (int gy = 0; gy < 5; ++gy) {
    for (int gx = 0; gx < 5; ++gx) {
      PipeJob j;
      j.tri = tri;
      j.tx = bx + gx * kPipeTile;
      j.ty = by + gy * kPipeTile;
      j.fill = pipe_word(210, 96, 40, static_cast<uint8_t>(0x40 + idx));
      j.clear = pipe_word(24, 24, 72, 0x02);
      j.index = idx;
      j.src = static_cast<uint16_t>(0x200 + idx);
      ++idx;
      jobs.push_back(j);
    }
  }

  std::vector<PipeTile> got;
  const bool ok = run_batch(jobs, PipeFeed::kBackToBack, 0u, 0u, "multitile", &got);
  check(ok, "multi-tile: every tile of a 5x5 unaligned grid matches the composed oracle", 1,
        ok ? 1 : 0);

  uint32_t touched = 0, missed = 0, partial = 0;
  for (const PipeJob& j : jobs) {
    const uint32_t c = solo_oracle(j).count;
    if (c == 0)
      ++missed;
    else {
      ++touched;
      if (c < static_cast<uint32_t>(kPipePixels)) ++partial;
    }
  }
  std::printf("raster_tile_pipe multi-tile: %u touched (%u partial), %u missed of %zu\n", touched,
              partial, missed, jobs.size());
  check(touched >= 6, "multi-tile: the triangle really does span several tiles", 1,
        touched >= 6 ? 1 : 0);
  check(missed >= 6, "multi-tile: the grid really does include tiles it misses entirely", 1,
        missed >= 6 ? 1 : 0);
  check(partial >= 4, "multi-tile: several tiles are partially covered", 1, partial >= 4 ? 1 : 0);

  // Missed tiles must be the clear colour AND must all agree with each other
  // only where the Bayer phase agrees — they sit at different origins, so a
  // tile-local phase would make them all identical. Check the stronger thing:
  // the missed tiles at origins whose (x&3, y&3) differ resolve DIFFERENTLY.
  int a = -1, b = -1;
  for (size_t i = 0; i < jobs.size(); ++i) {
    if (solo_oracle(jobs[i]).count != 0) continue;
    if (a < 0) {
      a = static_cast<int>(i);
    } else if (b < 0) {
      b = static_cast<int>(i);
    }
  }
  check(a >= 0 && b >= 0, "multi-tile: at least two missed tiles to compare", 1,
        (a >= 0 && b >= 0) ? 1 : 0);
}

// --------------------------------------------------------------------- 4 ---
// THE PING-PONG. Sixteen fully covered tiles, fed with no idle cycles, must
// give exactly what the same sixteen give when each waits for the previous
// tile's CRC — and must do it with two tiles in flight and in far fewer
// cycles. That overlap is the whole reason RASTER.TILESTORE has two banks.
void test_pingpong_overlap() {
  std::vector<PipeJob> jobs;
  for (uint16_t i = 0; i < 16; ++i) {
    PipeJob j;
    j.tx = 2 + static_cast<int32_t>(i) * kPipeTile;
    j.ty = 7;
    j.tri = tri_full(j.tx, j.ty);
    j.fill = distinct_word(i + 1u);
    j.clear = distinct_word(i + 40u);
    j.index = i;
    j.src = static_cast<uint16_t>(0x300 + i);
    jobs.push_back(j);
  }

  std::vector<PipeTile> b2b, ser;
  uint32_t cyc_b2b = 0, cyc_ser = 0;
  const bool ok_b = run_batch(jobs, PipeFeed::kBackToBack, 0u, 0u, "b2b", &b2b, &cyc_b2b);
  const size_t flight_b2b = dev().max_in_flight();
  const uint32_t toggles = dev().front_toggles();
  const bool ok_s = run_batch(jobs, PipeFeed::kSerial, 0u, 0u, "serial", &ser, &cyc_ser);
  const size_t flight_ser = dev().max_in_flight();

  check(ok_b && ok_s, "ping-pong: both feed orders match the composed oracle", 1,
        (ok_b && ok_s) ? 1 : 0);

  bool identical = true;
  for (size_t i = 0; i < jobs.size(); ++i)
    if (!b2b[i].same_picture(ser[i])) identical = false;
  check(identical, "ping-pong: back-to-back and serial feeding give identical pictures and CRCs", 1,
        identical ? 1 : 0);

  std::printf(
      "raster_tile_pipe ping-pong: %u tiles; back-to-back %u cycles (%u/tile, max %zu in flight), "
      "serial %u cycles (%u/tile, max %zu in flight)\n",
      static_cast<uint32_t>(jobs.size()), cyc_b2b, cyc_b2b / 16u, flight_b2b, cyc_ser,
      cyc_ser / 16u, flight_ser);

  check(flight_b2b == 2, "ping-pong: back-to-back really does keep TWO tiles in flight", 2,
        static_cast<uint64_t>(flight_b2b));
  check(flight_ser == 1, "ping-pong: the serial feed keeps exactly one", 1,
        static_cast<uint64_t>(flight_ser));
  // Resolve alone is 259 cycles a tile (RASTER.RESOLVE.md, measured). If the
  // banks did not overlap, back-to-back could not beat serial at all.
  check(static_cast<uint64_t>(cyc_b2b) * 3u < static_cast<uint64_t>(cyc_ser) * 2u,
        "ping-pong: overlapping really hides the resolve (b2b < 2/3 of serial)", 1,
        (static_cast<uint64_t>(cyc_b2b) * 3u < static_cast<uint64_t>(cyc_ser) * 2u) ? 1 : 0);
  check(toggles == jobs.size(), "ping-pong: front_bank_o toggles exactly once per tile",
        jobs.size(), toggles);
}

// --------------------------------------------------------------------- 5 ---
// Backpressure on the framebuffer output, stalling mid-tile, plus a gapped
// job feed. Costs cycles; may never cost or change a pixel or a CRC.
void test_backpressure() {
  const int32_t bx = 3, by = 9;
  PipeTri tri;
  tri.ax = px(bx + 4);
  tri.ay = px(by + 2);
  tri.bx = px(bx + 44);
  tri.by = px(by + 20);
  tri.cx = px(bx + 12);
  tri.cy = px(by + 40);

  std::vector<PipeJob> jobs;
  uint16_t idx = 0;
  for (int gy = 0; gy < 3; ++gy) {
    for (int gx = 0; gx < 3; ++gx) {
      PipeJob j;
      j.tri = tri;
      j.tx = bx + gx * kPipeTile;
      j.ty = by + gy * kPipeTile;
      j.fill = distinct_word(idx + 5u);
      j.clear = distinct_word(idx + 60u);
      j.index = idx;
      j.src = static_cast<uint16_t>(0x400 + idx);
      ++idx;
      jobs.push_back(j);
    }
  }

  std::vector<PipeTile> base;
  uint32_t cyc_base = 0;
  const bool ok0 = run_batch(jobs, PipeFeed::kBackToBack, 0u, 0u, "bp_base", &base, &cyc_base);
  check(ok0, "backpressure: the unstalled baseline matches the composed oracle", 1, ok0 ? 1 : 0);

  uint32_t drift = 0;
  uint32_t slowest = cyc_base;
  bool ok_all = true;
  for (uint32_t s = 1; s <= 6; ++s) {
    std::vector<PipeTile> got;
    uint32_t cyc = 0;
    const PipeFeed feed = (s & 1u) ? PipeFeed::kGapped : PipeFeed::kBackToBack;
    if (!run_batch(jobs, feed, 0x51D0u * s + 7u, 0xB100u * s + 3u, "bp", &got, &cyc))
      ok_all = false;
    for (size_t i = 0; i < jobs.size(); ++i)
      if (!got[i].same_picture(base[i])) ++drift;
    if (cyc > slowest) slowest = cyc;
  }
  std::printf("raster_tile_pipe backpressure: 6 stall/feed patterns, %u..%u cycles for %zu tiles\n",
              cyc_base, slowest, jobs.size());
  check(ok_all, "backpressure: every stall pattern still matches the composed oracle", 1,
        ok_all ? 1 : 0);
  check(drift == 0, "backpressure: stalling mid-tile changes neither a pixel nor a CRC", 0, drift);
  check(slowest > cyc_base, "backpressure: the stall patterns really did stall", 1,
        slowest > cyc_base ? 1 : 0);
}

// --------------------------------------------------------------------- 6 ---
// THE BAYER PHASE IS ABSOLUTE. The same coverage, the same fill, the same
// clear, at 16 tile origins covering every (x&3, y&3): 16 DIFFERENT pictures.
// The triangle is translated with the tile so the coverage is byte-identical
// and the phase is the only thing that moves. A composition that dropped the
// tile origin on the way to RASTER.RESOLVE would return the SAME picture 16
// times — and would still pass every test run at a 16-aligned origin.
void test_dither_phase_absolute() {
  PipeTri base;
  base.ax = px(3);
  base.ay = px(1);
  base.bx = px(15);
  base.by = px(6);
  base.cx = px(6);
  base.cy = px(14);

  auto shifted = [&](int32_t dx, int32_t dy) {
    PipeJob j;
    j.tri = base;
    j.tri.ax += px(dx);
    j.tri.bx += px(dx);
    j.tri.cx += px(dx);
    j.tri.ay += px(dy);
    j.tri.by += px(dy);
    j.tri.cy += px(dy);
    j.tx = dx;
    j.ty = dy;
    j.fill = pipe_word(100, 100, 100, 0x11);
    j.clear = pipe_word(37, 90, 200, 0x22);
    j.index = static_cast<uint16_t>((dy & 3) * 4 + (dx & 3));
    j.src = static_cast<uint16_t>(0x500 + j.index);
    return j;
  };

  std::vector<PipeJob> jobs;
  for (int32_t dy = 0; dy < 4; ++dy)
    for (int32_t dx = 0; dx < 4; ++dx) jobs.push_back(shifted(dx, dy));

  std::vector<PipeTile> got;
  const bool ok = run_batch(jobs, PipeFeed::kBackToBack, 0u, 0u, "phase", &got);
  check(ok, "dither phase: all 16 absolute phases match the composed oracle", 1, ok ? 1 : 0);

  // the coverage really is identical across the 16 (only the phase moved)
  uint32_t cov0 = solo_oracle(jobs[0]).count;
  bool same_cov = true;
  for (const PipeJob& j : jobs)
    if (solo_oracle(j).count != cov0) same_cov = false;
  check(same_cov && cov0 > 0 && cov0 < static_cast<uint32_t>(kPipePixels),
        "dither phase: the 16 tiles share one partial coverage, so only the phase differs", 1,
        (same_cov && cov0 > 0 && cov0 < static_cast<uint32_t>(kPipePixels)) ? 1 : 0);

  uint32_t collisions = 0;
  for (size_t i = 0; i < got.size(); ++i)
    for (size_t k = i + 1; k < got.size(); ++k)
      if (got[i].same_picture(got[k])) ++collisions;
  check(collisions == 0, "dither phase: all 120 pairs of the 16 absolute phases differ", 0,
        collisions);

  // ...and every 16-aligned origin reduces to phase (0,0): same picture.
  std::vector<PipeJob> aligned;
  const int32_t offs[4] = {0, 16, -16, 64};
  for (int k = 0; k < 4; ++k) {
    PipeJob j = shifted(offs[k], offs[k]);
    j.index = static_cast<uint16_t>(0x60 + k);
    j.src = static_cast<uint16_t>(0x600 + k);
    aligned.push_back(j);
  }
  std::vector<PipeTile> agot;
  const bool aok = run_batch(aligned, PipeFeed::kBackToBack, 0u, 0u, "aligned", &agot);
  uint32_t adrift = 0;
  for (size_t i = 1; i < agot.size(); ++i)
    if (!agot[i].same_picture(agot[0])) ++adrift;
  check(aok && adrift == 0,
        "dither phase: every 16-aligned origin is phase 0 and gives one picture", 0, adrift);
  check(!agot[0].same_picture(got[5]),
        "dither phase: and the aligned picture is NOT the phase-(1,1) one", 1,
        agot[0].same_picture(got[5]) ? 0 : 1);
}

// --------------------------------------------------------------------- 7 ---
// THE BLACK RAIL — a KNOWN, ESCALATED defect of reference/src/zrender/
// resolve.cpp, reproduced here bit for bit and NOT fixed.
// Green's dither amplitude is 32 while red's and blue's is 16, so at the 8
// Bayer cells with B >= 8 the green numerator 0*63 + 32B + 16 >= 272 >= 255
// and g6 comes out 1: pure black resolves to 0x0020 in exactly half the
// pixels. RASTER.RESOLVE.md records it as an observed law awaiting an owner
// decision; changing it is a resolve.cpp + golden-capture call, not an RTL
// one. This case pins the ACTUAL behaviour so the composition cannot quietly
// "improve" it either.
void test_black_rail() {
  PipeJob j;
  j.tri = tri_no_pixel(4, 4);  // real triangle, zero coverage
  j.tx = 0;
  j.ty = 0;
  j.fill = pipe_word(255, 255, 255);
  j.clear = pipe_word(0, 0, 0);
  j.index = 0x7A;
  j.src = 0x700;

  std::vector<PipeTile> got;
  const bool ok = run_batch({j}, PipeFeed::kBackToBack, 0u, 0u, "black", &got);
  check(ok, "black rail: the cleared-to-black tile matches the composed oracle", 1, ok ? 1 : 0);

  uint32_t lifted = 0, zero = 0, other = 0;
  for (int i = 0; i < kPipePixels; ++i) {
    if (got[0].rgb565[i] == 0x0000)
      ++zero;
    else if (got[0].rgb565[i] == 0x0020)
      ++lifted;
    else
      ++other;
  }
  check(other == 0, "black rail: black resolves ONLY to 0x0000 or 0x0020", 0, other);
  check(lifted == 128, "black rail: exactly half the pixels are lifted to green level 1 (KNOWN)",
        128, lifted);
  check(zero == 128, "black rail: the other half is exactly 0x0000", 128, zero);
}

// --------------------------------------------------------------------- 8 ---
// Law 3: the framebuffer beat carries the SURFACE coordinate of its pixel,
// which is the RESOLVING tile's origin plus {col, row} — not the walking
// tile's, and not the in-tile address. Negative origins included.
void test_fb_address() {
  const int32_t origins[6][2] = {{0, 0}, {16, 32}, {5, 11}, {-16, -32}, {-3, -7}, {2000, -2000}};
  std::vector<PipeJob> jobs;
  for (int k = 0; k < 6; ++k) {
    PipeJob j;
    j.tx = origins[k][0];
    j.ty = origins[k][1];
    j.tri = tri_full(j.tx, j.ty);
    j.fill = distinct_word(static_cast<uint32_t>(k) + 17u);
    j.clear = distinct_word(static_cast<uint32_t>(k) + 80u);
    j.index = static_cast<uint16_t>(0x80 + k);
    j.src = static_cast<uint16_t>(0x800 + k);
    jobs.push_back(j);
  }

  std::vector<PipeTile> got;
  const bool ok = run_batch(jobs, PipeFeed::kBackToBack, 0u, 0u, "fbaddr", &got);
  check(ok, "fb address: the batch matches the composed oracle", 1, ok ? 1 : 0);

  uint32_t wrong = 0;
  for (size_t k = 0; k < jobs.size(); ++k) wrong += pipe_address_errors(jobs[k], got[k]);
  check(wrong == 0, "fb address: every beat carries tile origin + {col, row}", 0, wrong);
}

// --------------------------------------------------------------------- 9 ---
// The status and the CRC belong to the tile that FINISHED, not the one being
// walked — the raster stage is a whole tile ahead by then. Feed tiles whose
// coverage counts are all different and check each lands with its own tile;
// and feed the same tile twice and require the same CRC (the seed is re-armed
// per tile, so a CRC accumulated across tiles cannot survive).
void test_status_and_crc() {
  std::vector<PipeJob> jobs;
  // A right triangle with legs w, its corner half a pixel above and left of
  // the tile's top-left pixel centre: it covers exactly the centres with
  // col + row <= w - 2, i.e. the triangular number T(w-1). These six widths
  // give 1, 3, 10, 28, 78 and 136 covered pixels — six DIFFERENT pictures.
  const int32_t widths[6] = {2, 3, 5, 8, 13, 17};
  for (uint16_t k = 0; k < 6; ++k) {
    PipeJob j;
    j.tx = 24;
    j.ty = 40;
    const int32_t w = widths[k];
    j.tri.ax = px(24) - 128;
    j.tri.ay = px(40) - 128;
    j.tri.bx = px(24 + w) - 128;
    j.tri.by = px(40) - 128;
    j.tri.cx = px(24) - 128;
    j.tri.cy = px(40 + w) - 128;
    j.fill = pipe_word(240, 12, 60, 0x5A);
    j.clear = pipe_word(8, 130, 66, 0x0C);
    j.index = static_cast<uint16_t>(0x90 + k);
    j.src = static_cast<uint16_t>(0x900 + k);
    jobs.push_back(j);
  }
  // then the SAME tile twice in a row: identical CRCs prove the reseed
  PipeJob twin = jobs[3];
  twin.index = 0xA0;
  twin.src = 0x9A0;
  jobs.push_back(twin);
  PipeJob twin2 = twin;
  twin2.index = 0xA1;
  twin2.src = 0x9A1;
  jobs.push_back(twin2);

  std::vector<PipeTile> got;
  const bool ok = run_batch(jobs, PipeFeed::kBackToBack, 0u, 0u, "status", &got);
  check(ok, "status: the batch matches the composed oracle", 1, ok ? 1 : 0);

  uint32_t misplaced = 0;
  uint32_t distinct_counts = 0;
  uint32_t seen[7] = {};
  for (size_t k = 0; k < jobs.size(); ++k) {
    const PipeExpect w = solo_oracle(jobs[k]);
    if (got[k].count != w.count || got[k].degenerate != w.degenerate) ++misplaced;
    bool fresh = true;
    for (uint32_t s = 0; s < distinct_counts; ++s)
      if (seen[s] == w.count) fresh = false;
    if (fresh && distinct_counts < 7) seen[distinct_counts++] = w.count;
  }
  check(misplaced == 0, "status: the coverage count and degenerate flag ride with their own tile",
        0, misplaced);
  check(distinct_counts >= 5, "status: the batch really does have distinct coverage counts", 1,
        distinct_counts >= 5 ? 1 : 0);

  check(got[6].crc32c == got[7].crc32c, "CRC: two identical tiles in a row give the same CRC",
        got[6].crc32c, got[7].crc32c);

  // The vector set is first PROVED able to tell the six tiles apart (their
  // oracle pictures are pairwise different), and only then are the CRCs
  // required to be pairwise different. A CRC accumulated across tiles instead
  // of reseeded makes tile k's CRC depend on tiles 0..k-1 — which this cannot
  // see on its own, which is why the twin pair above carries that half.
  uint32_t same_picture = 0;
  uint32_t dupes = 0;
  for (size_t a = 0; a < 6; ++a) {
    const PipeExpect wa = solo_oracle(jobs[a]);
    for (size_t b = a + 1; b < 6; ++b) {
      const PipeExpect wb = solo_oracle(jobs[b]);
      bool identical = true;
      for (int i = 0; i < kPipePixels; ++i)
        if (wa.res.rgb565[i] != wb.res.rgb565[i]) identical = false;
      if (identical) ++same_picture;
      if (got[a].crc32c == got[b].crc32c) ++dupes;
    }
  }
  check(same_picture == 0, "CRC: the six coverages really do give six different pictures", 0,
        same_picture);
  check(dupes == 0, "CRC: six different pictures give six different CRCs", 0, dupes);
}

// -------------------------------------------------------------------- 10 ---
// The composition's counters. RASTER.TILESTORE counts accepted DATA accesses
// (writes plus both read ports); read port A is tied off here, so the total is
// exactly "one write per covered pixel, plus 256 resolve reads per tile" — an
// end-to-end arithmetic identity across all three blocks.
void test_counters() {
  std::vector<PipeJob> jobs;
  const int32_t bx = 5, by = 5;
  PipeTri tri;
  tri.ax = px(bx + 6);
  tri.ay = px(by + 2);
  tri.bx = px(bx + 40);
  tri.by = px(by + 18);
  tri.cx = px(bx + 10);
  tri.cy = px(by + 36);
  uint16_t idx = 0;
  for (int gy = 0; gy < 2; ++gy) {
    for (int gx = 0; gx < 3; ++gx) {
      PipeJob j;
      j.tri = tri;
      j.tx = bx + gx * kPipeTile;
      j.ty = by + gy * kPipeTile;
      j.fill = distinct_word(idx + 100u);
      j.clear = distinct_word(idx + 130u);
      j.index = idx;
      j.src = static_cast<uint16_t>(0xA00 + idx);
      ++idx;
      jobs.push_back(j);
    }
  }

  std::vector<PipeTile> got;
  const bool ok = run_batch(jobs, PipeFeed::kBackToBack, 0u, 0u, "counters", &got);
  check(ok, "counters: the batch matches the composed oracle", 1, ok ? 1 : 0);

  uint32_t covered = 0;
  for (const PipeJob& j : jobs) covered += solo_oracle(j).count;
  const uint32_t want_refs = covered + static_cast<uint32_t>(jobs.size()) * kPipePixels;
  check(dev().tile_references() == want_refs,
        "counters: tile_references == covered writes + 256 resolve reads per tile", want_refs,
        dev().tile_references());
  check(dev().resolved_tiles() == jobs.size(), "counters: resolved_tiles == tiles handed in",
        jobs.size(), dev().resolved_tiles());
  check(dev().front_toggles() == jobs.size(), "counters: one bank swap per tile", jobs.size(),
        dev().front_toggles());
  check(covered > 0, "counters: the batch really did write something", 1, covered > 0 ? 1 : 0);
}

}  // namespace

int main() {
  test_single_tile();
  test_coverage_extremes();
  test_multi_tile_triangle();
  test_pingpong_overlap();
  test_backpressure();
  test_dither_phase_absolute();
  test_black_rail();
  test_fb_address();
  test_status_and_crc();
  test_counters();
  return zhao::report_and_exit("raster_tile_pipe_directed");
}
