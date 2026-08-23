// surface_sq_directed.cpp — the shared sequential squarer, on its own.
//
// WHY THIS FILE EXISTS, and it is the mutation sweep's answer rather than a
// hunch. `zhao_surface_sq` is the arithmetic core the 28 DSP blocks were
// traded for, and until this file it had no test of its own — it was checked
// only THROUGH `zhao_surface_stamp`'s coverage test. That turns out to be a
// systematically weak place to check it:
//
//   `covered = !(d2 > r_outer2 || d2 < r_inner2)`
//
// and `d2`, `r_outer2` and `r_inner2` all come out of the SAME instance of this
// module. **A comparison is scale-invariant**, so any mutation that multiplies
// this module's output by a constant is invisible through coverage — `2*m^2`
// and `m^2/2` order exactly as `m^2` does. The sweep of 2026-08-23 found three
// such mutants alive against the whole stamp suite:
//
//   S02  the sign is read from bit 34 instead of bit 35
//   S05  each chain link's place value is one too high      (uniform x2)
//   S12  the addend is loaded pre-doubled                   (uniform x2)
//
// None is a defect the stamp can observe; all three are defects in the MODULE.
// Checking `sq_o` against the arithmetic directly is what distinguishes them,
// and it is the same argument `zhao_surface_blend` already won: factor the
// arithmetic out, then check it where its value is visible instead of where it
// has been reduced to a boolean.
//
// EXHAUSTIVE IS NOT AVAILABLE HERE. The blend has 22 free input bits and is
// proved TOTAL. This module has 36 plus a sequential state, so the input space
// is 2^36 and a proof would be bounded rather than total. The cases below are
// therefore CHOSEN, and each is chosen against a specific way of being wrong:
// the sign rails, the magnitude rails, every single-bit magnitude (which is the
// only thing that distinguishes one chain link's place value from its
// neighbour's), the parity cases the coverage test could never see, and the
// actual operand ranges `zhao_surface_stamp` presents.
//
// THE ORACLE IS THE C++ MULTIPLY, deliberately, and truncated the same way:
// `zhao_surface_stamp` used to compute `64'(dx) * 64'(dx)` and keep 64 bits, so
// `(uint64_t)v * (uint64_t)v` in two's complement IS the law this module has to
// reproduce — for every input, in the stated +-4,096 m domain and outside it.

#include "Vzhao_surface_sq.h"
#include "verilated.h"

#include "zhao_sim.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using zhao::check;

// The RTL's parameters. ZHAO_SQ_RADIX is set per target so the frontier builds
// check their own elaboration; MAG_W is fixed at 36 by zhao_surface_stamp.
#ifndef ZHAO_SQ_RADIX
#define ZHAO_SQ_RADIX 1
#endif
constexpr int kMagW = 36;
constexpr int kSteps = (kMagW + ZHAO_SQ_RADIX - 1) / ZHAO_SQ_RADIX;

void tick(Vzhao_surface_sq& dut) {
  dut.clk = 0;
  dut.eval();
  dut.clk = 1;
  dut.eval();
}

void reset(Vzhao_surface_sq& dut) {
  dut.rst_n = 0;
  dut.start_i = 0;
  dut.a_i = 0;
  tick(dut);
  tick(dut);
  dut.rst_n = 1;
  tick(dut);
}

// Sign-extend a 36-bit two's-complement value held in a uint64_t, so the
// oracle multiplies the same number the RTL squares.
int64_t sext36(uint64_t raw) {
  const uint64_t m = raw & ((1ULL << kMagW) - 1);
  return (m & (1ULL << (kMagW - 1))) ? static_cast<int64_t>(m | ~((1ULL << kMagW) - 1))
                                     : static_cast<int64_t>(m);
}

// Drive one square and return sq_o, asserting the handshake shape on the way.
uint64_t square(Vzhao_surface_sq& dut, uint64_t raw, int* latency_out = nullptr) {
  dut.a_i = raw;
  dut.start_i = 1;
  tick(dut);
  dut.start_i = 0;
  // vld_o must be LOW the cycle after a start, whatever it was before: a stale
  // result surviving into a new square is what mutant S11 is.
  const bool vld_after_start = dut.vld_o != 0;
  int cycles = 0;
  while (dut.vld_o == 0 && cycles < 4 * kMagW) {
    tick(dut);
    ++cycles;
  }
  if (latency_out) *latency_out = vld_after_start ? -1 : cycles;
  return dut.sq_o;
}

}  // namespace

int main() {
  Vzhao_surface_sq dut;
  reset(dut);

  // ---- 1. the cases, each chosen against a way of being wrong -------------
  std::vector<int64_t> vals;

  // zero, and the smallest magnitudes — the only place where the difference
  // between m^2 and (m^2 - m)/2 is a single bit.
  for (int64_t v = -8; v <= 8; ++v) vals.push_back(v);

  // EVERY single-bit magnitude, positive and negative. One set bit means the
  // whole result is one partial product, so this is the only family that can
  // tell chain link b from chain link b+1 -- which is exactly mutant S05.
  for (int b = 0; b < kMagW - 1; ++b) {
    vals.push_back(static_cast<int64_t>(1LL << b));
    vals.push_back(-static_cast<int64_t>(1LL << b));
  }

  // The signed rails of the 36-bit lane. -2^35 is the input where a naive
  // signed negation gives back a NEGATIVE magnitude, and where the true square
  // (2^70) leaves the 64-bit lane entirely -- both forms must give 0.
  vals.push_back(-(1LL << 35));
  vals.push_back((1LL << 35) - 1);
  vals.push_back(-((1LL << 35) - 1));

  // The bits the STAMP cannot reach but the MODULE must still get right.
  // zhao_surface_stamp bounds |dx| below 2^34 for any int32 input, so bits
  // 35:34 are always equal there and mutant S02 is invisible from above.
  // Driving them here is the point of testing the leaf.
  vals.push_back((1LL << 34) + 12345);
  vals.push_back(-((1LL << 34) + 12345));
  vals.push_back((3LL << 33) + 7);

  // The operand ranges zhao_surface_stamp actually presents: dx/dz at the
  // +-4,096 m domain edge, a radius, an r_inner that reaches 2^32 because it is
  // max(r - rw, 0) on two unconstrained int32 words.
  vals.push_back(1LL << 30);
  vals.push_back(-(1LL << 30));
  vals.push_back((1LL << 28) * 3 / 2);
  vals.push_back(1LL << 32);
  vals.push_back((1LL << 32) - 1);

  // ODD magnitudes at scale. The stamp suite drove none of these before
  // 2026-08-23, because every envelope, radius and translation in it is a whole
  // number of metres or a binary fraction of one. That accident kept two
  // halving mutants alive; parity is now checked at the source.
  vals.push_back(12345);
  vals.push_back(-12345);
  vals.push_back(0x1FFFFFFFFLL);  // 33 ones
  vals.push_back(-0x1FFFFFFFFLL);
  vals.push_back(0xAAAAAAAABLL);
  vals.push_back(0x555555555LL);

  uint64_t mismatches = 0;
  int64_t first_bad = 0;
  uint64_t first_want = 0, first_got = 0;
  for (int64_t v : vals) {
    const uint64_t raw = static_cast<uint64_t>(v) & ((1ULL << kMagW) - 1);
    const int64_t sv = sext36(raw);
    // The law: the low 64 bits of the two's-complement product, which is what
    // `64'(dx) * 64'(dx)` gave before the farm came out.
    const uint64_t want = static_cast<uint64_t>(sv) * static_cast<uint64_t>(sv);
    const uint64_t got = square(dut, raw);
    if (got != want) {
      if (mismatches == 0) {
        first_bad = sv;
        first_want = want;
        first_got = got;
      }
      ++mismatches;
    }
  }
  if (mismatches != 0)
    std::printf("surface_sq_directed: first mismatch at v=%lld want=%llu got=%llu\n",
                static_cast<long long>(first_bad), static_cast<unsigned long long>(first_want),
                static_cast<unsigned long long>(first_got));
  check(mismatches == 0, "every directed value squares to the low 64 bits of v*v", 0, mismatches);
  check(vals.size() > 80, "the directed set is not accidentally empty", 80,
        static_cast<uint64_t>(vals.size()));

  // ---- 2. the handshake shape --------------------------------------------
  // `Steps` ticks after the start tick, `vld_o` is observable — i.e. the last
  // accumulate registers at the end of the Steps-th step cycle and is readable
  // from the cycle after. In zhao_surface_stamp's numbering that is Steps + 1
  // cycles from the start pulse, and its pass costs one more for the GStartX
  // cycle itself: Steps + 2. At the default that is 38, which is exactly the
  // 158,162-cycle full-cover stamp the directed suite measures
  // (64 rows * 65 passes * 38). **The two conventions differ by one and this
  // comment exists so the next reader does not "fix" the RTL to reconcile them.**
  //
  // The counting is law rather than observation: the block's whole initiation
  // interval, and therefore its texels/frame against the 20,000 demand, is
  // built on it.
  int lat = 0;
  square(dut, 0x123456789ULL, &lat);
  check(lat == kSteps, "vld_o is observable exactly Steps ticks after the start tick", kSteps,
        static_cast<uint64_t>(lat));

  // A second start must clear the previous result's valid, or the parent reads
  // a stale square as if it were this texel's.
  int lat2 = 0;
  square(dut, 7, &lat2);
  check(lat2 >= 0, "a fresh start clears the previous vld_o", 1, lat2 >= 0 ? 1 : 0);

  // vld_o must HOLD after it rises — the parent stalls on backpressure and
  // reads d2 many cycles later.
  const uint64_t held = dut.sq_o;
  for (int i = 0; i < 20; ++i) tick(dut);
  check(dut.vld_o != 0, "vld_o holds until the next start", 1, dut.vld_o ? 1 : 0);
  check(dut.sq_o == held, "sq_o holds with it", held, dut.sq_o);

  // ---- 3. reset ------------------------------------------------------------
  reset(dut);
  check(dut.vld_o == 0, "reset clears vld_o", 0, dut.vld_o ? 1 : 0);
  check(dut.sq_o == 0, "reset clears sq_o", 0, dut.sq_o);

  std::printf("surface_sq_directed: SQ_RADIX=%d, %d steps, %d values\n", ZHAO_SQ_RADIX, kSteps,
              static_cast<int>(vals.size()));
  dut.final();
  return zhao::report_and_exit("surface_sq_directed");
}
