// raster_tile_pipe_random.cpp — randomized differential test for the composed
// phase-4/5 tile pipe (fpga/rtl/raster/zhao_raster_tile_pipe.sv):
// RASTER.EDGEWALK -> RASTER.EARLYZ -> RASTER.FRAGMENT -> RASTER.TILESTORE ->
// RASTER.RESOLVE.
//
// THE FRAGMENT STATE IS PART OF THE RANDOM DRAW, and it has to be. When
// RASTER.EARLYZ and RASTER.FRAGMENT replaced the flat write path, this lane
// still drove `state == 0` for every job — the plain opaque write, with the
// depth test off, blend REPLACE and no alpha test. A mutation sweep caught it
// immediately: an INVERTED DEPTH COMPARISON in RASTER.FRAGMENT was caught by
// both of that block's own lanes and by the composed DIRECTED lane, and this
// one stayed green, because at state 0 there is no depth comparison to
// invert. So `make_job` now draws a legal random state for half its jobs and
// the phase-4 state 0 for the other half — the second half is what keeps the
// bit-for-bit phase-4 compatibility evidence, and the first half is what
// makes this lane able to fail.
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
#include "zref/zref_fragment.hpp"
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
uint32_t g_state_zero = 0;        // jobs at the phase-4 opaque write
uint32_t g_state_rand = 0;        // jobs at a random legal recipe
uint32_t g_ez_rejects = 0;        // early-Z rejects the oracle predicted
uint32_t g_frag_killed = 0;       // candidates the fragment tests killed
uint32_t g_blended = 0;           // surviving fragments that really blended
uint32_t g_depth_tie = 0;         // fragment depth EXACTLY at the tile clear depth
uint32_t g_depth_just_above = 0;  // ...and exactly one LSB above it

/**
 * A legal random fragment state. The 32-bit encoding has no reserved holes,
 * so every draw is a state the chain must handle and nothing is filtered out.
 * The depth test is forced ON far more often than a uniform bit would give
 * it, because a state with the test off exercises neither RASTER.EARLYZ's
 * reject nor RASTER.FRAGMENT's comparison — which is exactly the hole that
 * made this lane blind before.
 */
uint32_t random_state(Prng& rng) {
  const uint32_t r = rng.draw();
  zref::FragmentPipeline::State s;
  s.z_test_en = (r & 3u) != 0u;  // on 3 times in 4
  s.z_write_dis = (r >> 2) & 1u;
  s.z_force_far = (r >> 3) & 1u;
  s.blend = static_cast<uint8_t>((r >> 4) & 3u);
  s.shade_mod = (r >> 6) & 1u;
  s.alpha_mod = (r >> 7) & 1u;
  s.atest_en = (r >> 8) & 1u;
  s.atest_ref = ((r >> 9) & 1u) ? 0 : static_cast<uint8_t>(rng.draw());
  s.sten_func = static_cast<uint8_t>((r >> 10) & 3u);
  s.sten_op = static_cast<uint8_t>((r >> 12) & 3u);
  s.tag_write_dis = (r >> 14) & 1u;
  s.tag_from_texel = (r >> 15) & 1u;
  s.tag_channel = static_cast<uint8_t>((r >> 16) & 3u);
  s.sten_mask = ((r >> 18) & 1u) ? 0xFF : static_cast<uint8_t>(rng.draw());
  return s.pack();
}

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

  // Half the jobs keep the phase-4 opaque write (state 0), which is what
  // makes "the pre-RASTER.FRAGMENT behaviour is preserved bit for bit" a
  // measured claim rather than an assertion; the other half draw a legal
  // recipe, which is what lets this lane fail at all.
  if ((rng.draw() & 1u) != 0u) {
    j.state = 0u;
    ++g_state_zero;
  } else {
    j.state = random_state(rng);
    ++g_state_rand;
  }
  j.src_a = static_cast<uint8_t>(rng.draw());
  j.texel_rgb = rng.draw() & 0xFFFFFFu;
  j.texel_a = static_cast<uint8_t>(rng.draw());
  // Index 0 a quarter of the time: it is the ratified alpha-test sentinel.
  j.texel_idx = ((rng.draw() & 3u) == 0u) ? 0 : static_cast<uint8_t>(rng.draw());

  // THE DEPTH IS DRAWN IN A NARROW WINDOW AROUND THE CLEAR DEPTH. A uniform
  // 24-bit draw makes `fragment depth == tile clear depth (+/- 1)` a
  // 1-in-16-million event, and those three values are the ONLY ones that
  // separate a correct early-Z boundary from an off-by-one: this composition
  // is flat-shaded, so every fragment of a tile carries the same depth and
  // the reject decision is all-or-nothing per tile. A mutation sweep proved
  // the point - widening RASTER.EARLYZ's reject by one LSB survived this lane
  // untouched until the window below was added.
  const uint32_t clear_depth = rng.draw() & 0xFFFFFFu;
  uint32_t frag_depth = rng.draw() & 0xFFFFFFu;
  if ((rng.draw() & 1u) != 0u) {
    const int32_t off = rng.span(-2, 3);
    int64_t d = static_cast<int64_t>(clear_depth) + off;
    if (d < 0) d = 0;
    if (d > 0xFFFFFF) d = 0xFFFFFF;
    frag_depth = static_cast<uint32_t>(d);
    if (frag_depth == clear_depth) ++g_depth_tie;
    if (frag_depth == clear_depth + 1u) ++g_depth_just_above;
  }

  j.fill = pipe_word(static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                     static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()), frag_depth,
                     static_cast<uint8_t>(rng.draw()));
  j.clear = pipe_word(static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                      static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                      clear_depth, static_cast<uint8_t>(rng.draw()));
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
  uint32_t want_rejects = 0;
  uint32_t want_blended = 0;
  for (size_t i = 0; i < jobs.size(); ++i) {
    const PipeExpect want = pipe_oracle(store, ez, jobs[i]);
    want_rejects += want.rejects;
    want_blended += want.blended;
    if (want.degenerate)
      ++g_degenerate;
    else if (want.count == 0)
      ++g_empty;
    else if (want.count == static_cast<uint32_t>(kPipePixels))
      ++g_full;
    else
      ++g_partial;

    g_ez_rejects += want.rejects;
    g_frag_killed += want.candidates - want.writes;
    g_blended += want.blended;

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

  // THE TWO NEW BLOCKS' COUNTERS, ON EVERY CASE. The composed picture cannot
  // see everything the chain does: RASTER.EARLYZ is a CONSERVATIVE reject, so
  // an early-Z that passes a fragment RASTER.FRAGMENT then kills produces the
  // identical tile - by design, since the block is allowed to be pessimistic
  // and only forbidden to be optimistic. What it is NOT allowed to do is
  // disagree with its contract about how many it rejected: `early_z_rejects`
  // is a budgeted counter, and a reject that quietly stopped happening is a
  // performance regression nothing else here would notice. So the counters
  // are diffed against the composed oracle's own prediction on EVERY case,
  // not only in the counters case. (A mutation sweep found this: relaxing the
  // reject's tie survived both composed lanes until this comparison existed.)
  if (dev.early_z_rejects() != want_rejects || dev.blended_fragments() != want_blended ||
      dev.saw_fragment_error()) {
    ok = false;
    ++g_failures;
    if (g_saved < 6) {
      std::printf("  %s[%u]: counters rejects %u/%u blended %u/%u error %d\n", lane, iter,
                  want_rejects, dev.early_z_rejects(), want_blended, dev.blended_fragments(),
                  static_cast<int>(dev.saw_fragment_error()));
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

  // The fragment chain has to have DONE something, or this lane is once again
  // only testing the phase-4 write path with extra steps.
  std::printf(
      "raster_tile_pipe_random recipes: %u jobs at state 0, %u at a random recipe; "
      "%u early-Z rejects, %u fragments killed by the tests, %u blended\n",
      g_state_zero, g_state_rand, g_ez_rejects, g_frag_killed, g_blended);
  check(g_state_zero > 0 && g_state_rand > 0,
        "lane A: both the phase-4 state 0 and random recipes were driven", 1,
        (g_state_zero > 0 && g_state_rand > 0) ? 1 : 0);
  check(g_ez_rejects > 0, "lane A: RASTER.EARLYZ actually rejected fragments", 1,
        g_ez_rejects > 0 ? 1 : 0);
  check(g_frag_killed > 0, "lane A: RASTER.FRAGMENT's tests actually killed fragments", 1,
        g_frag_killed > 0 ? 1 : 0);
  check(g_blended > 0, "lane A: fragments were actually BLENDED into the tile", 1,
        g_blended > 0 ? 1 : 0);
  std::printf("raster_tile_pipe_random depth boundary: %u ties at the clear depth, %u one above\n",
              g_depth_tie, g_depth_just_above);
  check(g_depth_tie > 0 && g_depth_just_above > 0,
        "lane A: the early-Z boundary (the tie, and one LSB above) was actually reached", 1,
        (g_depth_tie > 0 && g_depth_just_above > 0) ? 1 : 0);
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
