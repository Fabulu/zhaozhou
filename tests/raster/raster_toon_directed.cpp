// raster_toon_directed.cpp — the cel band against rast.cpp's own ramp, on
// Zixxtrixx's shipped constants.
//
// ---------------------------------------------------------------------------
// WHAT CAN BE WRONG
// ---------------------------------------------------------------------------
// The toon ramp looks like three comparisons and a multiply. It is not, and
// each of the ways it differs is a way the creature stops looking like itself:
//
//   * THE RATIO. `r*q/mean` per channel preserves the Cool Cross chromatic
//     relationship. Replacing the light with `q` on all three lanes -- the
//     obvious "toon" implementation -- turns the cool blue fill into dark grey.
//     A test whose inputs are grey cannot tell those apart, so every case here
//     carries three DIFFERENT channel values.
//
//   * THE TRUNCATION. rast.cpp divides in C++, which truncates TOWARD ZERO.
//     zhao_raster_attrdiv rounds half AWAY from zero. Reusing the attribute
//     divider here would be off by one on most fragments -- invisible in a
//     screenshot, fatal to a capture CRC.
//
//   * THE SIGN. Light lanes are signed and an unlit fragment can carry a
//     negative one. Truncation and floor differ only on negatives, so the
//     negative cases are the ones that separate a correct implementation from
//     an arithmetic-shift one.
//
//   * THE MEAN'S OWN DIVISION. `(r+g+b)/3` also truncates toward zero, and it
//     decides the BAND. One code of drift there moves a band boundary, which is
//     a visible edge on the creature rather than a rounding difference.
//
//   * `mean <= 0`, where the reference short-circuits to a flat `q` because
//     there is no ratio to preserve.
//
// The oracle is rast.cpp's law restated, and the constants are the ones the
// shipped presentation actually uses.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_toon.h"

#include "zhao_sim.hpp"

namespace {

// ZIXX_EXP=celmain, the shipped Zixxtrixx presentation.
constexpr int32_t kThr0 = 43000;
constexpr int32_t kThr1 = 57000;
constexpr int32_t kLvl0 = 28000;
constexpr int32_t kLvl1 = 50000;
constexpr int32_t kLvl2 = 82000;

struct Rgb {
  int32_t r, g, b;
};

/** rast.cpp's apply_toon_ramp, restated. C++ division truncates toward zero. */
Rgb toon_ref(Rgb v, int bands) {
  if (bands == 0) return v;
  const int32_t mean = static_cast<int32_t>(
      (static_cast<int64_t>(v.r) + static_cast<int64_t>(v.g) + v.b) / 3);
  const int32_t q =
      bands <= 2 ? (mean < kThr0 ? kLvl0 : kLvl1)
                 : (mean < kThr0 ? kLvl0 : (mean < kThr1 ? kLvl1 : kLvl2));
  if (mean <= 0) return Rgb{q, q, q};
  return Rgb{static_cast<int32_t>(static_cast<int64_t>(v.r) * q / mean),
             static_cast<int32_t>(static_cast<int64_t>(v.g) * q / mean),
             static_cast<int32_t>(static_cast<int64_t>(v.b) * q / mean)};
}

struct Out {
  Rgb v;
  int band;
  int clocks;
};

Out run_one(Vzhao_raster_toon& t, Rgb in, int bands, uint16_t tag) {
  t.cfg_bands_i = static_cast<uint8_t>(bands);
  t.cfg_thr0_i = static_cast<uint32_t>(kThr0);
  t.cfg_thr1_i = static_cast<uint32_t>(kThr1);
  t.cfg_lvl0_i = static_cast<uint32_t>(kLvl0);
  t.cfg_lvl1_i = static_cast<uint32_t>(kLvl1);
  t.cfg_lvl2_i = static_cast<uint32_t>(kLvl2);
  t.r_i = static_cast<uint32_t>(in.r);
  t.g_i = static_cast<uint32_t>(in.g);
  t.b_i = static_cast<uint32_t>(in.b);
  t.tag_i = tag;
  t.v_valid_i = 1;
  int clocks = 0;
  for (;;) {
    t.eval();
    const bool taken = t.v_ready_o != 0;
    zhao::tick(t);
    ++clocks;
    if (taken) break;
    if (clocks > 2000) return {{0, 0, 0}, -1, -1};
  }
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  for (;;) {
    t.eval();
    if (t.r_valid_o) {
      const Out o{{static_cast<int32_t>(t.r_o), static_cast<int32_t>(t.g_o),
                   static_cast<int32_t>(t.b_o)},
                  static_cast<int>(t.band_o), clocks + 1};
      const bool tag_ok = (t.tag_o == tag);
      zhao::tick(t);
      if (!tag_ok) return {{0, 0, 0}, -2, o.clocks};
      return o;
    }
    zhao::tick(t);
    ++clocks;
    if (clocks > 2000) return {{0, 0, 0}, -1, -1};
  }
}

void reset(Vzhao_raster_toon& t) {
  t.rst_n = 0;
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  t.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_toon top;
  reset(top);

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: the three bands, on colours that are not grey ==\n");
  {
    // Deliberately chromatic: the Cool Cross rig produces a warm key and a
    // blue fill, so R, G and B are never equal on a lit creature. A grey input
    // would let a band-value-replacing implementation pass.
    const Rgb cases[] = {
        {20000, 22000, 30000},   // deep shadow, blue-dominant -> band 0
        {40000, 43000, 52000},   // just under threshold 0
        {43000, 43000, 43000},   // exactly at threshold 0
        {45000, 48000, 60000},   // mid band
        {56000, 57000, 58000},   // just under threshold 1
        {57000, 57000, 57000},   // exactly at threshold 1
        {70000, 65000, 60000},   // lit, warm-dominant -> band 2
        {120000, 90000, 70000},  // bright key
    };
    long bad = 0, checked = 0;
    int bands_seen[3] = {0, 0, 0};
    for (const Rgb& c : cases) {
      const Out got = run_one(top, c, 3, 0x11);
      const Rgb want = toon_ref(c, 3);
      if (got.band < 0) {
        printf("      (%d,%d,%d): protocol failure %d\n", c.r, c.g, c.b, got.band);
        ++bad;
        continue;
      }
      if (got.band >= 0 && got.band < 3) ++bands_seen[got.band];
      if (got.v.r != want.r || got.v.g != want.g || got.v.b != want.b) {
        if (bad < 4)
          printf("      (%d,%d,%d): want (%d,%d,%d) got (%d,%d,%d)\n", c.r, c.g, c.b, want.r,
                 want.g, want.b, got.v.r, got.v.g, got.v.b);
        ++bad;
      }
      ++checked;
    }
    zhao::check(bad == 0, "every band matches rast.cpp's ramp exactly", 0, (uint32_t)bad);
    printf("   MEASURED: %ld cases, bands hit %d/%d/%d\n", checked, bands_seen[0], bands_seen[1],
           bands_seen[2]);
    zhao::check(bands_seen[0] > 0 && bands_seen[1] > 0 && bands_seen[2] > 0,
                "and all three bands were actually reached", 1,
                (bands_seen[0] > 0 && bands_seen[1] > 0 && bands_seen[2] > 0) ? 1 : 0);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: the RATIO survives, which is the whole point ==\n");
  {
    // A ratio-preserving ramp keeps R:G:B; a band-replacing one flattens it.
    // Check the output is NOT grey for a chromatic input, and that it matches
    // the reference -- the second is the real test, the first says why.
    const Rgb in{30000, 45000, 90000};  // strongly blue
    const Out got = run_one(top, in, 3, 0x22);
    const Rgb want = toon_ref(in, 3);
    zhao::check(got.v.r == want.r && got.v.g == want.g && got.v.b == want.b,
                "a strongly chromatic fragment matches the reference", 1,
                (got.v.r == want.r && got.v.g == want.g && got.v.b == want.b) ? 1 : 0);
    printf("   MEASURED: (%d,%d,%d) -> (%d,%d,%d)\n", in.r, in.g, in.b, got.v.r, got.v.g, got.v.b);
    zhao::check(!(got.v.r == got.v.g && got.v.g == got.v.b),
                "and it did NOT collapse to three equal lanes", 1,
                (!(got.v.r == got.v.g && got.v.g == got.v.b)) ? 1 : 0);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: negatives, where truncation and floor part company ==\n");
  {
    // Truncation toward zero and floor agree on positives and differ on
    // negatives. An arithmetic-shift or floor implementation passes every
    // positive case above and fails here.
    const Rgb cases[] = {
        {-5000, 20000, 40000},
        {-30000, -20000, 90000},
        {70000, -1, 65000},
        {-1, -2, -3},          // mean < 0: the flat short circuit
        {1, 1, 1},             // mean 0 by truncation: also flat
        {0, 0, 0},             // mean exactly 0
        {2, 0, 0},             // mean 0 by truncation from a positive sum
        {-2, 0, 0},            // mean 0 by truncation from a negative sum
    };
    long bad = 0, flats = 0;
    for (const Rgb& c : cases) {
      const Out got = run_one(top, c, 3, 0x33);
      const Rgb want = toon_ref(c, 3);
      if (want.r == want.g && want.g == want.b) ++flats;
      if (got.band < 0 || got.v.r != want.r || got.v.g != want.g || got.v.b != want.b) {
        if (bad < 4)
          printf("      (%d,%d,%d): want (%d,%d,%d) got (%d,%d,%d)\n", c.r, c.g, c.b, want.r,
                 want.g, want.b, got.v.r, got.v.g, got.v.b);
        ++bad;
      }
    }
    zhao::check(bad == 0, "negative lanes truncate toward zero, as rast.cpp does", 0,
                (uint32_t)bad);
    zhao::check(flats >= 4, "and the mean <= 0 short circuit was actually exercised", 4,
                (uint32_t)flats);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: a wide random sweep ==\n");
  {
    uint64_t s = 0xCE1CE1ull;
    auto nxt = [&s]() {
      s ^= s << 13;
      s ^= s >> 7;
      s ^= s << 17;
      return s;
    };
    long bad = 0, checked = 0;
    for (int i = 0; i < 400; ++i) {
      // A range that straddles both thresholds and reaches negative.
      auto lane = [&]() { return static_cast<int32_t>(nxt() % 160000ull) - 20000; };
      const Rgb c{lane(), lane(), lane()};
      const Out got = run_one(top, c, 3, static_cast<uint16_t>(i));
      const Rgb want = toon_ref(c, 3);
      if (got.band < 0 || got.v.r != want.r || got.v.g != want.g || got.v.b != want.b) {
        if (bad < 3)
          printf("      (%d,%d,%d): want (%d,%d,%d) got (%d,%d,%d)\n", c.r, c.g, c.b, want.r,
                 want.g, want.b, got.v.r, got.v.g, got.v.b);
        ++bad;
      }
      ++checked;
    }
    zhao::check(bad == 0, "400 random fragments all match the reference", 0, (uint32_t)bad);
    printf("   MEASURED: %ld random fragments\n", checked);
  }

  // ------------------------------------------------------------------ 5 ---
  printf("== section 5: bands = 0 is a pass-through, and the rate ==\n");
  {
    // Every non-cel material sets bands = 0 and must not be touched OR slowed.
    const Rgb in{12345, -678, 90123};
    const Out got = run_one(top, in, 0, 0x55);
    zhao::check(got.v.r == in.r && got.v.g == in.g && got.v.b == in.b,
                "bands = 0 passes the light through untouched", 1,
                (got.v.r == in.r && got.v.g == in.g && got.v.b == in.b) ? 1 : 0);
    printf("   MEASURED: pass-through costs %d clocks\n", got.clocks);

    // THROUGHPUT, not latency. run_one() issues one fragment and waits for it,
    // which measures the 32-stage array's LATENCY -- the first version of this
    // section reported 39 clocks and called it the rate. The block carries four
    // fragments at once, so the number that matters is what a back-to-back
    // stream costs.
    reset(top);
    const int kN = 300;
    std::vector<Rgb> stream;
    for (int i = 0; i < kN; ++i)
      stream.push_back(Rgb{40000 + i * 37, 50000 - i * 11, 60000 + i * 53});

    top.cfg_bands_i = 3;
    top.cfg_thr0_i = static_cast<uint32_t>(kThr0);
    top.cfg_thr1_i = static_cast<uint32_t>(kThr1);
    top.cfg_lvl0_i = static_cast<uint32_t>(kLvl0);
    top.cfg_lvl1_i = static_cast<uint32_t>(kLvl1);
    top.cfg_lvl2_i = static_cast<uint32_t>(kLvl2);
    size_t next = 0;
    long done = 0, clocks = 0, wrong = 0;
    while (done < kN && clocks < 200000) {
      const bool offering = next < stream.size();
      if (offering) {
        top.r_i = static_cast<uint32_t>(stream[next].r);
        top.g_i = static_cast<uint32_t>(stream[next].g);
        top.b_i = static_cast<uint32_t>(stream[next].b);
        top.tag_i = static_cast<uint16_t>(next & 0xFFFF);
      }
      top.v_valid_i = offering ? 1 : 0;
      top.r_ready_i = 1;
      top.eval();
      const bool took = offering && top.v_ready_o;
      if (top.r_valid_o && top.r_ready_i) {
        const size_t idx = static_cast<size_t>(top.tag_o);
        const Rgb want = toon_ref(stream[idx], 3);
        if (static_cast<int32_t>(top.r_o) != want.r ||
            static_cast<int32_t>(top.g_o) != want.g ||
            static_cast<int32_t>(top.b_o) != want.b || idx != static_cast<size_t>(done))
          ++wrong;
        ++done;
      }
      zhao::tick(top);
      ++clocks;
      if (took) ++next;
    }
    top.v_valid_i = 0;
    top.eval();
    zhao::check(done == kN, "the streamed batch completes", (uint32_t)kN, (uint32_t)done);
    zhao::check(wrong == 0, "and every streamed fragment is exact AND in order", 0,
                (uint32_t)wrong);
    const double per_frag = static_cast<double>(clocks) / static_cast<double>(kN);
    const int64_t per_frame = static_cast<int64_t>(1666667.0 / per_frag);
    printf("   MEASURED: %.2f clocks a cel fragment streamed -> %lld a frame\n", per_frag,
           (long long)per_frame);
    printf("   AGAINST: 320000 pre-Early-Z stress, and only SURVIVING CEL fragments arrive\n");
    printf("   VERDICT: %s\n", per_frame >= 320000 ? "SUFFICIENT" : "SHORT -- needs pipelining");
    zhao::check(per_frag < 8.0, "and a streamed fragment costs under 8 clocks", 1,
                per_frag < 8.0 ? 1 : 0);
  }

  return zhao::report_and_exit("raster_toon_directed");
}
