// raster_rcp24_directed.cpp — the raster reciprocal against the frozen
// full-domain hash, not against a sample.
//
// ---------------------------------------------------------------------------
// WHY THIS TEST GETS TO BE ABSOLUTE
// ---------------------------------------------------------------------------
// Almost nothing in this console can be checked exhaustively. This can:
// `rcp_u24` takes one u24 and spec/qformats.md §6.1 publishes a frozen FNV-1a-64
// hash over the results of ALL 16,777,215 nonzero inputs --
//
//     RCP24_FULL_HASH = 0xd624beb8659baf83
//
// -- so the RTL is not compared against a handful of values, or against a
// re-derivation, or against the same table it was built from. It is walked over
// its entire input domain and reduced to one number that was frozen before this
// block existed. There is no sampling decision to get wrong and no corner left
// for a bug to sit in.
//
// That matters more than usual here because the failure mode is quiet. A
// reciprocal that is off by one LSB on some inputs produces texture coordinates
// that are off by a fraction of a texel: no crash, no seam, no obviously wrong
// picture, just a capture CRC that no longer matches and no way to tell which
// of a dozen blocks moved it.
//
// ---------------------------------------------------------------------------
// AND THE ONE WIDTH THIS BLOCK GUESSED AT
// ---------------------------------------------------------------------------
// The reference computes `2^31 - w` in uint64, which WRAPS if w exceeds 2^31,
// and the RTL reproduces that at 64 bits rather than assuming it cannot happen.
// Section 3 measures the actual maximum of w across the whole domain -- from the
// reference's own arithmetic, since w is internal to the block -- so the width
// can be narrowed later against evidence instead of against confidence.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_raster_rcp24.h"

#include "zhao_sim.hpp"
#include "zref/zref_rcp.hpp"

namespace {

constexpr int64_t kClocksPerFrame = 1666667;

struct Res {
  uint32_t r;
  int k;
  bool zero;
  int clocks;
};

Res one(Vzhao_raster_rcp24& t, uint32_t d) {
  t.d_i = d;
  t.v_valid_i = 1;
  int clocks = 0;
  for (;;) {
    t.eval();
    const bool taken = t.v_ready_o != 0;
    zhao::tick(t);
    ++clocks;
    if (taken) break;
    if (clocks > 200) return {0, 0, false, -1};
  }
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  for (;;) {
    t.eval();
    if (t.r_valid_o) {
      const Res res{static_cast<uint32_t>(t.r_o), static_cast<int>(t.k_o), t.d_zero_o != 0,
                    clocks + 1};
      zhao::tick(t);
      return res;
    }
    zhao::tick(t);
    ++clocks;
    if (clocks > 200) return {0, 0, false, -1};
  }
}

void reset(Vzhao_raster_rcp24& t) {
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
  Vzhao_raster_rcp24 top;
  reset(top);

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: the boundaries of the law ==\n");
  {
    // Each of these is a place the law says something specific, so a block that
    // is merely close fails here before the sweep even starts.
    const uint32_t cases[] = {
        1u,             // maximal normalisation shift, k = 24
        2u,             //
        0x7FFFFFu,      // just below the normalised range
        0x800000u,      // m == 2^23: THE pinned input, the only one that saturates
        0x800001u,      // one above the pin
        0xFFFFFFu,      // no shift at all, k = 1
        0xFFFFFEu,      //
        0xABCDEFu,      // nothing special, which is also worth one case
    };
    long bad = 0;
    for (uint32_t d : cases) {
      const Res got = one(top, d);
      const zref::rcp24_result want = zref::rcp_u24(d);
      if (got.clocks < 0 || got.r != want.r || got.k != want.k || got.zero) {
        printf("      d=0x%06X: want r=0x%06X k=%d, got r=0x%06X k=%d%s\n", d, want.r, want.k,
               got.r, got.k, got.zero ? " ZERO" : "");
        ++bad;
      }
    }
    zhao::check(bad == 0, "every boundary input matches zref exactly", 0, (uint32_t)bad);

    // The pin, said out loud: m == 2^23 is the single input whose result
    // saturates, and it is law rather than overflow.
    const Res pin = one(top, 0x800000u);
    zhao::check(pin.r == 0xFFFFFFu, "the pinned input saturates to 0xFFFFFF", 0xFFFFFFu, pin.r);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: THE WHOLE DOMAIN, against the frozen hash ==\n");
  {
    // FNV-1a-64 over the 3 little-endian bytes of each r, d ascending from 1.
    uint64_t h = 14695981039346656037ull;
    const uint64_t prime = 1099511628211ull;
    long bad = 0;
    long first_bad_d = -1;

    for (uint32_t d = 1; d <= 0xFFFFFFu; ++d) {
      const Res got = one(top, d);
      if (got.clocks < 0) {
        if (first_bad_d < 0) first_bad_d = d;
        ++bad;
        break;
      }
      for (int b = 0; b < 3; ++b) {
        h ^= (got.r >> (8 * b)) & 0xFFu;
        h *= prime;
      }
      // The k lane is not in the hash, so it is checked directly. A block with a
      // perfect mantissa and a wrong exponent would sail through the hash and
      // put every texture coordinate off by a power of two.
      if (got.k != zref::rcp_u24(d).k) {
        if (first_bad_d < 0) first_bad_d = d;
        ++bad;
      }
    }

    if (first_bad_d >= 0) printf("      first divergence at d=%ld\n", first_bad_d);
    zhao::check(bad == 0, "the exponent k matches zref on every input", 0, (uint32_t)bad);
    printf("   MEASURED: hash over all 16777215 inputs = 0x%016llx\n", (unsigned long long)h);
    zhao::check(h == zref::RCP24_FULL_HASH,
                "and the mantissas hash to the frozen RCP24_FULL_HASH", 1,
                h == zref::RCP24_FULL_HASH ? 1 : 0);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: the initiation interval, and the width that was guessed ==\n");
  {
    // The rate. This block sits on the per-surviving-pixel texture path, so its
    // II is a frame budget, not a curiosity.
    reset(top);
    int worst = 0, best = 1 << 20;
    for (uint32_t i = 0; i < 2000; ++i) {
      const uint32_t d = (i * 8389u + 12345u) & 0xFFFFFFu;
      const Res got = one(top, d ? d : 1u);
      if (got.clocks > worst) worst = got.clocks;
      if (got.clocks < best) best = got.clocks;
    }
    printf("   MEASURED: %d..%d clocks a reciprocal\n", best, worst);
    const int64_t per_frame = kClocksPerFrame / worst;
    printf("   THROUGHPUT: %lld reciprocals a frame from ONE unit\n", (long long)per_frame);
    printf("   AGAINST: 276480 terrain-primary pixels, each needing one after early-Z\n");
    printf("   VERDICT: %s\n", per_frame >= 276480 ? "SUFFICIENT for terrain alone"
                                                   : "SHORT -- this needs a UNITS sweep too");
    zhao::check(worst == best, "the interval is fixed, so the budget is a division", 1,
                worst == best ? 1 : 0);

    // THE WIDTH. w is internal, so it is measured from the reference's own
    // arithmetic over the whole domain -- the same law the RTL implements.
    // If this never reaches 2^31 the 64-bit subtract and product can be
    // narrowed later, with this number as the reason.
    // EXHAUSTIVE, not sampled. A width claim from a stride of 7 would be a
    // guess with a decimal point on it, and the loop is pure arithmetic.
    uint64_t max_w = 0;
    for (uint32_t d = 1; d <= 0xFFFFFFu; ++d) {
      uint32_t m = d;
      while ((m & (1u << 23)) == 0) m <<= 1;
      uint32_t x = zref::gen::RCP24_T0[(m - (1u << 23)) >> 15];
      for (int step = 0; step < 2; ++step) {
        const uint64_t w = (static_cast<uint64_t>(m) * x) >> 24;
        if (w > max_w) max_w = w;
        x = static_cast<uint32_t>(
            ((static_cast<uint64_t>(x) * ((2ull << 30) - w)) + (1ull << 29)) >> 30);
      }
    }
    printf("   MEASURED: max w over ALL 16777215 inputs = 0x%llx, 2^31 = 0x%llx\n",
           (unsigned long long)max_w, (unsigned long long)(1ull << 31));
    printf("   %s\n", max_w <= (1ull << 31)
                          ? "   w never exceeds 2^31: the 64-bit lanes are narrowable, with evidence"
                          : "   w EXCEEDS 2^31: the 64-bit wrap is load-bearing, keep the width");
    zhao::check(max_w > 0, "the width measurement actually ran", 1, max_w > 0 ? 1 : 0);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: a zero is reported, not looped on ==\n");
  {
    // The normalising loop shifts until bit 23 is set. Given zero it would never
    // stop, so zero is refused at accept -- the same choice RASTER.ATTRDIV makes
    // for a zero area.
    reset(top);
    const Res z = one(top, 0);
    zhao::check(z.clocks > 0, "d == 0 terminates", 1, z.clocks > 0 ? 1 : 0);
    zhao::check(z.zero, "and is reported as a caller bug rather than answered", 1,
                z.zero ? 1 : 0);

    // And the block still works afterwards, which a state machine that latched
    // an error state would fail.
    const Res after = one(top, 0x123456u);
    zhao::check(after.r == zref::rcp_u24(0x123456u).r && !after.zero,
                "and the block is usable again straight after", 1,
                (after.r == zref::rcp_u24(0x123456u).r && !after.zero) ? 1 : 0);
  }

  return zhao::report_and_exit("raster_rcp24_directed");
}
