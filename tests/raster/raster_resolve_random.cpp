// raster_resolve_random.cpp — RASTER.RESOLVE randomized differential test
// (design/contracts/RASTER.RESOLVE.md "Randomized differential tests"; law
// reference/src/zrender/resolve.cpp; oracle zref::TileResolve).
//
// Two lanes, both fully deterministic from fixed seeds (the PCG shape used by
// every other random lane in this tree — audio_fifo_random.cpp):
//
//   LANE A — resolve differential. PCG tiles across four populations (uniform
//     random, near-rail, low-contrast bands, and a per-pixel gradient) at PCG
//     origins that are deliberately NOT all 16-aligned, so the absolute Bayer
//     phase is exercised rather than the 16-aligned special case. Half the
//     tiles run with PCG-gated `tr_ready_i` and `fb_ready_i`. All 256 RGB565
//     pixels, all 256 tags and the tile CRC must equal zref::TileResolve.
//
//   LANE B — the phase and stall invariants. For each PCG tile:
//     · resolving it at origin (x, y) and at (x + 4k, y + 4m) must give the
//       IDENTICAL picture — the dither phase depends on the origin mod 4 and
//       on nothing else; and
//     · resolving it with three different stall patterns must give the
//       identical picture AND the identical CRC — backpressure may cost
//       cycles and may never cost or change a pixel.
//     A tile-local Bayer phase fails the first; a CRC accumulated on
//     production rather than on acceptance fails the second.
//
// Modes: default = 1,200 tiles for lane A and 300 for lane B (CTest fast);
// --nightly = 18,000 / 4,000. Failing vectors are saved (charter §29-17).

#include "raster_resolve_dev.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

using zhao::check;
using zhao_raster::kPixels;
using zhao_raster::oracle_resolve;
using zhao_raster::pack_px;
using zhao_raster::Resolved;
using zhao_raster::ResolveDev;

namespace {

// PCG RXS-M-XS — the committed test PRNG shape (qformats.md §7.5 constants).
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

// Four tile populations. Uniform random finds nothing near the rails; the
// near-rail and low-contrast bands are where the rounding term and the clamp
// actually decide something.
void make_tile(Prng& rng, uint64_t* w, uint32_t population) {
  switch (population) {
    case 0:  // uniform random
      for (int i = 0; i < kPixels; ++i)
        w[i] = pack_px(static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                       static_cast<uint8_t>(rng.draw()), static_cast<uint8_t>(rng.draw()),
                       rng.draw() & 0xFFFFFFu, static_cast<uint8_t>(rng.draw()));
      break;
    case 1: {  // near the rails: 0..7 and 248..255, where the clamps live
      for (int i = 0; i < kPixels; ++i) {
        auto rail = [&]() {
          const uint32_t v = rng.below(8u);
          return static_cast<uint8_t>((rng.draw() & 1u) ? (248u + v) : v);
        };
        w[i] = pack_px(rail(), rail(), rail(), static_cast<uint8_t>(rng.draw()));
      }
      break;
    }
    case 2: {  // low contrast: a narrow band, where the dither is the picture
      const uint32_t base = rng.below(248u);
      for (int i = 0; i < kPixels; ++i) {
        auto band = [&]() { return static_cast<uint8_t>(base + rng.below(8u)); };
        w[i] = pack_px(band(), band(), band(), static_cast<uint8_t>(rng.draw()));
      }
      break;
    }
    default: {  // a smooth gradient — the classic ordered-dither workload
      const uint32_t dr = rng.below(4u) + 1u, dg = rng.below(4u) + 1u, db = rng.below(4u) + 1u;
      for (int i = 0; i < kPixels; ++i)
        w[i] = pack_px(static_cast<uint8_t>(i * dr), static_cast<uint8_t>(i * dg),
                       static_cast<uint8_t>(i * db), static_cast<uint8_t>(i));
      break;
    }
  }
}

Resolved one(ResolveDev& dev, const uint64_t* w, int32_t tx, int32_t ty, uint32_t stall,
             const char* lane, uint32_t i, bool* ok) {
  std::string err;
  const Resolved got =
      dev.run(w, tx, ty, static_cast<uint16_t>(i), static_cast<uint16_t>(i * 7u), stall, &err);
  const Resolved want = oracle_resolve(w, tx, ty);
  *ok = err.empty() && (got == want);
  if (!*ok) {
    if (g_failures < 6) {
      const std::string body =
          err.empty() ? zhao_raster::describe(tx, ty, want, got) : ("protocol: " + err);
      std::printf("  %s[%u]: RTL != zref::TileResolve\n    %s\n", lane, i, body.c_str());
      char name[64];
      std::snprintf(name, sizeof(name), "raster_resolve_%s_%u", lane, i);
      zhao::save_failing_vector(name, zhao_raster::serialize(w, tx, ty), "zref::TileResolve", body);
    }
    ++g_failures;
  }
  return got;
}

// --------------------------------------------------------------------- A ---
void lane_a(ResolveDev& dev, uint32_t iters) {
  Prng rng(0x5EED'0024u);
  uint32_t unaligned = 0;
  uint64_t w[kPixels];
  for (uint32_t i = 0; i < iters; ++i) {
    make_tile(rng, w, i % 4u);
    // Deliberately NOT all 16-aligned: half the origins are arbitrary, so the
    // absolute Bayer phase carries the test instead of coinciding with the
    // tile-local one.
    const int32_t tx = (rng.draw() & 1u) ? (rng.span(-128, 128) * 16) : rng.span(-2048, 2047);
    const int32_t ty = (rng.draw() & 1u) ? (rng.span(-128, 128) * 16) : rng.span(-2048, 2047);
    if (((tx | ty) & 3) != 0) ++unaligned;
    const uint32_t stall = (i & 1u) ? (0x9000u + i * 13u) : 0u;
    bool ok = false;
    one(dev, w, tx, ty, stall, "laneA", i, &ok);
  }
  std::printf("raster_resolve_random lane A: %u tiles, %u at an unaligned Bayer phase\n", iters,
              unaligned);
  check(unaligned > iters / 4, "lane A: unaligned origins are well represented", 1,
        unaligned > iters / 4 ? 1 : 0);
}

// --------------------------------------------------------------------- B ---
void lane_b(ResolveDev& dev, uint32_t iters) {
  Prng rng(0x5EED'0025u);
  uint32_t phase_drift = 0;
  uint32_t stall_drift = 0;
  uint64_t w[kPixels];

  for (uint32_t i = 0; i < iters; ++i) {
    make_tile(rng, w, i % 4u);
    const int32_t tx = rng.span(-512, 512);
    const int32_t ty = rng.span(-512, 512);

    bool ok = false;
    const Resolved base = one(dev, w, tx, ty, 0u, "laneB", i, &ok);

    // (1) the phase depends on the origin mod 4 and on nothing else
    const int32_t dx = rng.span(-8, 8) * 4;
    const int32_t dy = rng.span(-8, 8) * 4;
    const Resolved shifted = one(dev, w, tx + dx, ty + dy, 0u, "laneB_shift", i, &ok);
    if (shifted != base) ++phase_drift;

    // (2) backpressure costs cycles, never pixels — and never the CRC
    for (uint32_t s = 1; s <= 3; ++s) {
      const Resolved st = one(dev, w, tx, ty, 0xA000u * s + i, "laneB_stall", i, &ok);
      if (st != base) ++stall_drift;
    }
  }

  std::printf("raster_resolve_random lane B: %u tiles x (1 shift + 3 stall patterns)\n", iters);
  check(phase_drift == 0, "lane B: a whole-multiple-of-4 origin shift changes nothing", 0,
        phase_drift);
  check(stall_drift == 0, "lane B: backpressure changes neither a pixel nor the CRC", 0,
        stall_drift);
}

}  // namespace

int main(int argc, char** argv) {
  bool nightly = false;
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--nightly") == 0) nightly = true;

  static ResolveDev dev;
  lane_a(dev, nightly ? 18000u : 1200u);
  lane_b(dev, nightly ? 4000u : 300u);

  check(g_failures == 0, "randomized differential: RTL == zref::TileResolve on every tile", 0,
        g_failures);
  return zhao::report_and_exit("raster_resolve_random");
}
