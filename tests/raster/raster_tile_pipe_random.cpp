// raster_tile_pipe_random.cpp — randomized differential test for the composed
// phase-4 tile pipe (fpga/rtl/raster/zhao_raster_tile_pipe.sv):
// RASTER.EDGEWALK -> RASTER.TILESTORE -> RASTER.RESOLVE.
//
// Two lanes, both fully deterministic from fixed seeds (the PCG shape every
// other random lane in this tree uses):
//
//   LANE A — the end-to-end differential. PCG triangles over PCG tile origins
//     that are deliberately NOT all 16-aligned, in BATCHES fed through one
//     instance with no reset, with PCG-gated framebuffer backpressure and a
//     PCG-gapped job feed. All 256 RGB565 pixels, all 256 tags, the tile CRC,
//     the coverage count and the degenerate flag must equal
//     zref::EdgeWalk -> zref::TileStore -> zref::TileResolve driven through
//     the identical clear/write/swap sequence, and every beat's SURFACE
//     address must be its tile origin plus its in-tile {col, row}.
//
//   LANE B — the composition invariants, each of which fails a specific
//     composition defect that lane A alone could pass by luck:
//       · shifting BOTH the triangle and the tile origin by (4k, 4m) pixels
//         gives the identical picture and CRC — the dither phase depends on
//         the origin mod 4 and on nothing else (a tile-local phase fails it);
//       · three stall/feed patterns give the identical picture and CRC —
//         backpressure costs cycles, never pixels;
//       · feeding the batch back-to-back and feeding it serially give the
//         identical pictures — the ping-pong may hide the resolve and may not
//         change what it resolves (a bank swapped early or late fails it).
//
// Modes: default = 400 batches x 4 tiles for lane A and 150 x 3 for lane B
// (CTest fast); --nightly = 4,000 and 1,200. Failing vectors are saved
// (charter 29-17).

#include "raster_tile_pipe_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "zref/zref_earlyz.hpp"
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
using zhao_raster::px;

namespace {

// PCG RXS-M-XS — the committed test PRNG shape (qformats.md 7.5 constants).
uint32_t pcg_perm(uint32_t s) {
  const uint32_t w = static_cast<uint32_t>(((s >> ((s >> 28) + 4)) ^ s) * 277803737u);
  return (w >> 22) ^ w;
}

struct Prng {
  uint32_t state;
  explicit Prng(uint32_t seed) : state(seed) {}
  uint32_t draw() {
    state = state * 747796405u + 2891336453u;
    return pcg_perm(state);
  }
  uint32_t below(uint32_t n) { return draw() % n; }
  int32_t span(int32_t lo, int32_t hi) {
    return lo + static_cast<int32_t>(draw() % static_cast<uint32_t>(hi - lo + 1));
  }
};

uint32_t g_failures = 0;
uint32_t g_saved = 0;
uint32_t g_unaligned = 0;
uint32_t g_partial = 0;
uint32_t g_empty = 0;
uint32_t g_full = 0;
uint32_t g_degenerate = 0;

// One job: a triangle around a tile, in four populations.
//   0 — a triangle a few pixels across, mostly partial coverage
//   1 — a triangle far larger than the tile: mostly full or mostly missed
//   2 — sub-pixel scale: the zero-coverage and one-pixel corner
//   3 — collinear: the zero-area reject
PipeJob make_job(Prng& rng, uint32_t population, uint16_t index) {
  PipeJob j;
  // Half the origins are 16-aligned, half arbitrary — the absolute Bayer
  // phase carries the test instead of coinciding with the tile-local one.
  j.tx = (rng.draw() & 1u) ? (rng.span(-60, 60) * kPipeTile) : rng.span(-1024, 1024);
  j.ty = (rng.draw() & 1u) ? (rng.span(-60, 60) * kPipeTile) : rng.span(-1024, 1024);
  if (((j.tx | j.ty) & 3) != 0) ++g_unaligned;

  const int32_t ox = px(j.tx);
  const int32_t oy = px(j.ty);
  switch (population) {
    case 0:
      j.tri.ax = ox + rng.span(-8, 24) * 256 + rng.span(0, 255);
      j.tri.ay = oy + rng.span(-8, 24) * 256 + rng.span(0, 255);
      j.tri.bx = ox + rng.span(-8, 24) * 256 + rng.span(0, 255);
      j.tri.by = oy + rng.span(-8, 24) * 256 + rng.span(0, 255);
      j.tri.cx = ox + rng.span(-8, 24) * 256 + rng.span(0, 255);
      j.tri.cy = oy + rng.span(-8, 24) * 256 + rng.span(0, 255);
      break;
    case 1:
      j.tri.ax = ox + rng.span(-48, 56) * 256;
      j.tri.ay = oy + rng.span(-48, 56) * 256;
      j.tri.bx = ox + rng.span(-48, 56) * 256;
      j.tri.by = oy + rng.span(-48, 56) * 256;
      j.tri.cx = ox + rng.span(-48, 56) * 256;
      j.tri.cy = oy + rng.span(-48, 56) * 256;
      break;
    case 2: {
      const int32_t cx = ox + rng.span(0, 15) * 256 + 128;
      const int32_t cy = oy + rng.span(0, 15) * 256 + 128;
      j.tri.ax = cx + rng.span(-160, 160);
      j.tri.ay = cy + rng.span(-160, 160);
      j.tri.bx = cx + rng.span(-160, 160);
      j.tri.by = cy + rng.span(-160, 160);
      j.tri.cx = cx + rng.span(-160, 160);
      j.tri.cy = cy + rng.span(-160, 160);
      break;
    }
    default: {  // collinear, at a random slope
      const int32_t sx = rng.span(-4, 4);
      const int32_t sy = rng.span(-4, 4);
      const int32_t k0 = rng.span(-8, 8);
      const int32_t k1 = rng.span(-8, 8);
      const int32_t k2 = rng.span(-8, 8);
      j.tri.ax = ox + sx * k0 * 256;
      j.tri.ay = oy + sy * k0 * 256;
      j.tri.bx = ox + sx * k1 * 256;
      j.tri.by = oy + sy * k1 * 256;
      j.tri.cx = ox + sx * k2 * 256;
      j.tri.cy = oy + sy * k2 * 256;
      break;
    }
  }

  j.fill = pipe_word(static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                     static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                     rng.draw() & 0xFFFFFFu, static_cast<uint8_t>(rng.draw()));
  j.clear = pipe_word(static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                      static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                      rng.draw() & 0xFFFFFFu, static_cast<uint8_t>(rng.draw()));
  j.index = index;
  j.src = static_cast<uint16_t>(index * 7u + 1u);
  return j;
}

// Runs one batch and diffs every tile against the five composed oracles.
bool one_batch(PipeDev& dev, const std::vector<PipeJob>& jobs, PipeFeed feed, uint32_t feed_seed,
               uint32_t fb_seed, const char* lane, uint32_t iter, std::vector<PipeTile>* got) {
  std::string err;
  dev.run(jobs, feed, feed_seed, fb_seed, got, &err);
  bool ok = err.empty();
  if (!ok) {
    if (g_saved < 6) std::printf("  %s[%u]: protocol violation: %s\n", lane, iter, err.c_str());
    ++g_failures;
  }

  zref::TileStore store;
  zref::EarlyZ ez;
  for (size_t i = 0; i < jobs.size(); ++i) {
    const PipeExpect want = pipe_oracle(store, ez, jobs[i]);
    if (want.degenerate)
      ++g_degenerate;
    else if (want.count == 0)
      ++g_empty;
    else if (want.count == static_cast<uint32_t>(kPipePixels))
      ++g_full;
    else
      ++g_partial;

    const uint32_t addr_bad = pipe_address_errors(jobs[i], (*got)[i]);
    if (addr_bad != 0) {
      if (g_saved < 6) {
        std::printf("  %s[%u] tile %zu: %u wrong surface addresses\n", lane, iter, i, addr_bad);
        ++g_saved;
      }
      ok = false;
      ++g_failures;
    }

    if (pipe_match(want, (*got)[i])) continue;
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      const std::string body = pipe_describe(jobs[i], want, (*got)[i]);
      std::printf("  %s[%u] tile %zu: RTL != the composed oracle\n    %s\n", lane, iter, i,
                  body.c_str());
      char name[80];
      std::snprintf(name, sizeof(name), "raster_tile_pipe_%s_%u_%zu", lane, iter, i);
      zhao::save_failing_vector(name, zhao_raster::pipe_serialize(jobs[i]),
                                "zref::EdgeWalk->TileStore->TileResolve", body);
      ++g_saved;
    }
  }
  return ok;
}

// --------------------------------------------------------------------- A ---
void lane_a(PipeDev& dev, uint32_t iters) {
  Prng rng(0x5EED'0031u);
  for (uint32_t i = 0; i < iters; ++i) {
    std::vector<PipeJob> jobs;
    for (uint32_t k = 0; k < 4; ++k)
      jobs.push_back(make_job(rng, (i + k) % 4u, static_cast<uint16_t>(i * 4u + k)));

    const uint32_t mode = rng.below(3u);
    const PipeFeed feed =
        (mode == 0) ? PipeFeed::kBackToBack : ((mode == 1) ? PipeFeed::kGapped : PipeFeed::kSerial);
    const uint32_t fb = (i & 1u) ? (0xC000u + i * 17u) : 0u;
    std::vector<PipeTile> got;
    one_batch(dev, jobs, feed, 0x2100u + i * 5u, fb, "laneA", i, &got);
  }
  std::printf(
      "raster_tile_pipe_random lane A: %u batches x 4 tiles; %u unaligned origins; coverage "
      "%u partial / %u empty / %u full / %u degenerate\n",
      iters, g_unaligned, g_partial, g_empty, g_full, g_degenerate);
  check(g_unaligned > iters, "lane A: unaligned tile origins are well represented", 1,
        g_unaligned > iters ? 1 : 0);
  check(g_partial > iters / 2, "lane A: partial coverage is well represented", 1,
        g_partial > iters / 2 ? 1 : 0);
  check(g_empty > 0 && g_full > 0 && g_degenerate > 0,
        "lane A: empty, full and degenerate tiles all occur", 1,
        (g_empty > 0 && g_full > 0 && g_degenerate > 0) ? 1 : 0);
}

// --------------------------------------------------------------------- B ---
void lane_b(PipeDev& dev, uint32_t iters) {
  Prng rng(0x5EED'0032u);
  uint32_t phase_drift = 0;
  uint32_t stall_drift = 0;
  uint32_t order_drift = 0;

  for (uint32_t i = 0; i < iters; ++i) {
    std::vector<PipeJob> jobs;
    for (uint32_t k = 0; k < 3; ++k)
      jobs.push_back(make_job(rng, (i + k) % 3u, static_cast<uint16_t>(i * 3u + k)));

    std::vector<PipeTile> base;
    one_batch(dev, jobs, PipeFeed::kBackToBack, 0u, 0u, "laneB", i, &base);

    // (1) a whole-multiple-of-4 shift of BOTH the tile and the triangle moves
    //     nothing: the Bayer phase is the origin mod 4 and nothing else.
    const int32_t dx = rng.span(-16, 16) * 4;
    const int32_t dy = rng.span(-16, 16) * 4;
    std::vector<PipeJob> moved = jobs;
    for (PipeJob& j : moved) {
      j.tx += dx;
      j.ty += dy;
      j.tri.ax += px(dx);
      j.tri.bx += px(dx);
      j.tri.cx += px(dx);
      j.tri.ay += px(dy);
      j.tri.by += px(dy);
      j.tri.cy += px(dy);
    }
    std::vector<PipeTile> shifted;
    one_batch(dev, moved, PipeFeed::kBackToBack, 0u, 0u, "laneB_shift", i, &shifted);
    for (size_t k = 0; k < jobs.size(); ++k)
      if (!shifted[k].same_picture(base[k])) ++phase_drift;

    // (2) backpressure costs cycles, never pixels — and never the CRC
    for (uint32_t s = 1; s <= 2; ++s) {
      std::vector<PipeTile> st;
      one_batch(dev, jobs, PipeFeed::kGapped, 0x7000u * s + i, 0xD000u * s + i, "laneB_stall", i,
                &st);
      for (size_t k = 0; k < jobs.size(); ++k)
        if (!st[k].same_picture(base[k])) ++stall_drift;
    }

    // (3) the ping-pong may hide the resolve; it may not change it
    std::vector<PipeTile> ser;
    one_batch(dev, jobs, PipeFeed::kSerial, 0u, 0u, "laneB_serial", i, &ser);
    for (size_t k = 0; k < jobs.size(); ++k)
      if (!ser[k].same_picture(base[k])) ++order_drift;
  }

  std::printf(
      "raster_tile_pipe_random lane B: %u batches x 3 tiles x (1 shift + 2 stalls + 1 "
      "serial)\n",
      iters);
  check(phase_drift == 0, "lane B: a whole-multiple-of-4 shift changes nothing", 0, phase_drift);
  check(stall_drift == 0, "lane B: backpressure changes neither a pixel nor the CRC", 0,
        stall_drift);
  check(order_drift == 0, "lane B: back-to-back and serial feeding resolve identically", 0,
        order_drift);
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  static PipeDev dev;
  lane_a(dev, nightly ? 4000u : 400u);
  lane_b(dev, nightly ? 1200u : 150u);

  check(g_failures == 0, "randomized differential: RTL == the composed oracle on every tile", 0,
        g_failures);
  return zhao::report_and_exit("raster_tile_pipe_random");
}
