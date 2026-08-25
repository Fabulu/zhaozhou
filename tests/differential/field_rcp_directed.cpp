// field_rcp_directed.cpp — OP_RCP, RTL against `zref::field_rcp`.
//
// The reciprocal is the first Field IR op that needs a table, and the table is
// GENERATED into `zhao_field_rcp_rom.sv` from `zref_tables.hpp`. Two things
// therefore have to be true, and this file checks both:
//
//   1. THE ROM IS THE TABLE. All 256 entries, compared against
//      `zref::gen::FIELD_RCP_T0`. A generated file can go stale as easily as a
//      transcribed one -- the generator is not run by the build -- and a wrong
//      seed does not fail loudly. It makes some reciprocals slightly wrong.
//   2. THE ARITHMETIC IS THE REFERENCE'S. Differential on the result and on both
//      ledger lanes.
//
// FOUR LAWS:
//
//   * `1/0` is the PINNED 0x7FFFFFFF and is not an error. qformats §6.2. The
//     `rcp0` lane records it; the program runs on.
//   * ONE Newton correction, not two. Two would be more accurate and would
//     disagree with every reciprocal the reference has produced.
//   * The sign is reapplied LAST, to an unsigned magnitude -- because
//     `-INT32_MIN` does not fit s32 and taking the absolute value early loses
//     exactly that input.
//   * Saturation records in the `rcp` lane, not `mul` or `rescale`.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "verilated.h"

#include "Vzhao_field_rcp_tb.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_rcp.hpp"
#include "zref/generated/zref_tables.hpp"

namespace {

using zhao::check;

struct Res {
  int32_t value = 0;
  bool sat_rcp = false, rcp0 = false;
};

Res oracle(int32_t a) {
  zref::SatLedger L{};
  Res o;
  o.value = zref::field_rcp(zref::fx16{a}, &L).raw;
  o.sat_rcp = L.rcp != 0;
  o.rcp0 = L.rcp0 != 0;
  return o;
}

// THE BLOCK IS READY/VALID NOW, NOT COMBINATIONAL. Under the DSP ruling of
// 2026-08-23 `zhao_field_rcp` no longer owns the two multipliers it used to
// stand on: both products walk the engine's one shared lane, so the reciprocal
// took a handshake and about seven clocks. The ANSWER is unchanged and this
// test is the proof of that -- same oracle, same vectors, same three lanes.
Res run(Vzhao_field_rcp_tb& dut, int32_t a) {
  dut.v_valid_i = 1;
  dut.a_i = static_cast<uint32_t>(a);
  dut.r_ready_i = 1;
  dut.eval();

  int guard = 0;
  while (!dut.v_ready_o && guard++ < 128) {
    zhao::tick(dut);
    dut.eval();
  }
  zhao::tick(dut);
  dut.v_valid_i = 0;
  dut.eval();

  int cycles = 0;
  while (!dut.r_valid_o && cycles++ < 256) {
    zhao::tick(dut);
    dut.eval();
  }

  Res r;
  r.value = static_cast<int32_t>(dut.result_o);
  r.sat_rcp = dut.sat_rcp_o != 0;
  r.rcp0 = dut.rcp0_o != 0;
  zhao::tick(dut);  // the result is taken
  dut.eval();
  return r;
}

void diff(Vzhao_field_rcp_tb& dut, int32_t a, const char* what) {
  const Res want = oracle(a);
  const Res got = run(dut, a);
  const std::string t(what);
  check(got.value == want.value, (t + ": value").c_str(), static_cast<uint32_t>(want.value),
        static_cast<uint32_t>(got.value));
  check(got.sat_rcp == want.sat_rcp, (t + ": SatLedger::rcp").c_str(), want.sat_rcp ? 1 : 0,
        got.sat_rcp ? 1 : 0);
  check(got.rcp0 == want.rcp0, (t + ": SatLedger::rcp0").c_str(), want.rcp0 ? 1 : 0,
        got.rcp0 ? 1 : 0);
}

struct Prng {
  uint64_t s;
  explicit Prng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
  uint32_t next() {
    const uint64_t v0 = s;
    s = v0 * 6364136223846793005ULL + 1442695040888963407ULL;
    const uint32_t w = static_cast<uint32_t>(((v0 >> 22) ^ v0) >> 29);
    const uint32_t v = (static_cast<uint32_t>(v0 >> 27) ^ w) * 277803737u;
    return (v >> 22) ^ v;
  }
  uint32_t below(uint32_t n) { return n ? (next() % n) : 0u; }
};

constexpr int32_t kOne = 1 << 16;

}  // namespace

int main(int argc, char** argv) {
  Vzhao_field_rcp_tb dut;
  dut.rst_n = 0;
  dut.v_valid_i = 0;
  dut.r_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();

  bool random_mode = false;
  uint32_t iters = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--random") == 0 && (i + 1) < argc) {
      random_mode = true;
      iters = static_cast<uint32_t>(std::atoi(argv[i + 1]));
    }
  }

  if (random_mode) {
    Prng rng(0x12C0u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      int32_t a;
      switch (rng.below(5)) {
        // Small magnitudes are where the reciprocal saturates, and they are a
        // vanishing fraction of a uniform draw -- so they get their own lane.
        case 0:
          a = static_cast<int32_t>(rng.below(64)) - 32;
          break;
        case 1:
          a = static_cast<int32_t>(rng.next()) >> 20;
          break;
        case 2:
          a = static_cast<int32_t>(rng.next()) >> 8;
          break;
        default:
          a = static_cast<int32_t>(rng.next());
          break;
      }
      char tag[64];
      std::snprintf(tag, sizeof tag, "random[%u] a=%d", it, a);
      diff(dut, a, tag);
    }
    dut.final();
    return zhao::report_and_exit("field_rcp_random");
  }

  // ---- 1. THE ROM IS THE TABLE --------------------------------------------
  // Every entry. The generator is not run by the build, so a stale ROM is a
  // real possibility and a wrong seed only shows up as slightly wrong answers.
  {
    // The RTL exposes the ROM only through the reciprocal, so the check drives
    // the index by choosing an `a` whose normalised mantissa selects it: a
    // value with bit 31 set and the eight bits below it equal to the index.
    uint64_t bad = 0;
    int first = -1;
    for (int i = 0; i < 256; ++i) {
      const uint32_t n = 0x8000'0000u | (static_cast<uint32_t>(i) << 23);
      // Feed it as a positive fx16 whose magnitude normalises to exactly n.
      const int32_t a = static_cast<int32_t>(n >> 1);  // bit 30 set: one left shift
      const Res want = oracle(a);
      const Res got = run(dut, a);
      if (got.value != want.value) {
        if (first < 0) first = i;
        ++bad;
      }
    }
    check(bad == 0, "the generated ROM agrees with the reference for all 256 seed indices", 0, bad);
    if (bad) std::printf("  first divergent seed index: %d\n", first);

    // And the table itself, read directly, so a drift is attributed to the ROM
    // rather than to the arithmetic around it.
    check(zref::gen::FIELD_RCP_T0[0] == 0xFF80u, "the reference table starts where it did", 0xFF80u,
          zref::gen::FIELD_RCP_T0[0]);
    check(zref::gen::FIELD_RCP_T0[255] == 0x8020u, "and ends where it did", 0x8020u,
          zref::gen::FIELD_RCP_T0[255]);
  }

  // ---- 2. 1/0 IS PINNED, AND IS NOT AN ERROR ------------------------------
  {
    diff(dut, 0, "1/0");
    const Res r = run(dut, 0);
    check(r.value == 0x7FFF'FFFF, "1/0 is the pinned 0x7FFFFFFF", 0x7FFFFFFFu,
          static_cast<uint32_t>(r.value));
    check(r.rcp0, "and it records in the sticky rcp0 lane", 1, r.rcp0 ? 1 : 0);
    check(!r.sat_rcp, "but NOT in the saturation lane -- it is pinned, not clamped", 0,
          r.sat_rcp ? 1 : 0);
  }

  // ---- 3. the exact values a reader can check by hand ----------------------
  {
    diff(dut, kOne, "1/1.0");
    const Res one = run(dut, kOne);
    check(one.value == kOne, "1/1.0 is exactly 1.0", static_cast<uint32_t>(kOne),
          static_cast<uint32_t>(one.value));
    diff(dut, 2 * kOne, "1/2.0");
    diff(dut, 4 * kOne, "1/4.0");
    diff(dut, kOne / 2, "1/0.5");
    diff(dut, kOne / 4, "1/0.25");
    diff(dut, -kOne, "1/-1.0");
    diff(dut, -2 * kOne, "1/-2.0");
  }

  // ---- 4. THE SIGN IS REAPPLIED LAST --------------------------------------
  // Every positive input and its negation must give exactly negated results.
  // An implementation that took the absolute value in signed arithmetic loses
  // INT32_MIN, whose negation does not fit.
  {
    for (int32_t a : {1, 2, 3, 100, kOne, 3 * kOne, 0x0100'0000, 0x7FFF'FFFF}) {
      const Res pos = run(dut, a);
      const Res neg = run(dut, -a);
      char tag[96];
      std::snprintf(tag, sizeof tag, "sign symmetry at a=%d", a);
      // Exact negation holds only where NEITHER side saturated. At the rail the
      // reference is deliberately asymmetric -- `neg ? INT32_MIN : INT32_MAX` --
      // and INT32_MIN is one larger in magnitude than -INT32_MAX. An earlier
      // draft of this check asserted symmetry unconditionally and failed on
      // a = 1 and a = 2, which are exactly the inputs that saturate.
      if (!pos.sat_rcp && !neg.sat_rcp) {
        check(neg.value == -pos.value, tag, static_cast<uint32_t>(-pos.value),
              static_cast<uint32_t>(neg.value));
      } else {
        check(pos.value == INT32_MAX && neg.value == INT32_MIN,
              (std::string(tag) + " (both rails, asymmetric by law)").c_str(), 1,
              (pos.value == INT32_MAX && neg.value == INT32_MIN) ? 1 : 0);
      }
      diff(dut, a, tag);
      diff(dut, -a, tag);
    }
    diff(dut, INT32_MIN, "1/INT32_MIN -- the input whose negation does not fit");
  }

  // ---- 5. THE SATURATION RAIL ---------------------------------------------
  // Small magnitudes give huge reciprocals. The rail records in `rcp`, and the
  // sign survives it.
  {
    for (int32_t a : {1, 2, 3, 7, 15, 16, 100, 1000}) {
      char tag[80];
      std::snprintf(tag, sizeof tag, "1/%d -- small magnitude, large reciprocal", a);
      diff(dut, a, tag);
      std::snprintf(tag, sizeof tag, "1/-%d", a);
      diff(dut, -a, tag);
    }
    const Res r1 = run(dut, 1);
    check(r1.sat_rcp, "1/one-ulp saturates and says so", 1, r1.sat_rcp ? 1 : 0);
    const Res rn1 = run(dut, -1);
    check(rn1.value == INT32_MIN, "and the negative rail is INT32_MIN, not INT32_MAX",
          static_cast<uint32_t>(INT32_MIN), static_cast<uint32_t>(rn1.value));
  }

  // ---- 6. every power of two, both signs ----------------------------------
  // The normalisation shift is a different amount for each, so this sweeps the
  // whole `e` range.
  //
  // What it CANNOT show, recorded so nobody looks for the missing case: e == 0
  // occurs only for |a| == 1, and 1/(1/65536) is 2^32, which saturates whatever
  // the final shift did. A mutation removing the e == 0 identity guard survives
  // the entire suite -- an equivalent mutant. The guard is still right; its
  // value simply never escapes.
  {
    for (int b = 0; b < 31; ++b) {
      const int32_t a = static_cast<int32_t>(1u << b);
      char tag[80];
      std::snprintf(tag, sizeof tag, "1/2^%d", b);
      diff(dut, a, tag);
      std::snprintf(tag, sizeof tag, "1/-2^%d", b);
      diff(dut, -a, tag);
    }
    diff(dut, INT32_MAX, "1/INT32_MAX -- the largest magnitude, e at its smallest");
  }

  dut.final();
  // ---- MEASURED INITIATION INTERVAL ---------------------------------------
  // reports/FIELD_V2_MODEL.md closes Earth60 at 94.1% of the reserved budget
  // and BINDS ON THIS UNIT. Its II is listed as 9, DERIVED by subtracting a
  // 7-clock front-end walk from a 16-clock total instruction latency, with a
  // stated +/-2 error bar -- and at II=10 the configuration does NOT close.
  // A load-bearing number that nobody has measured is exactly what this
  // project keeps getting caught by, so it is measured here.
  //
  // At the UNIT boundary, not through the sequencer: the sequencer drains every
  // multi-cycle op before fetching another, so anything measured through it
  // reports latency and calls it II. v_valid_i is held high with r_ready_i
  // high and the clocks between successive ACCEPTS are counted. The gap is
  // also required to be STEADY across five accepts -- a unit that accepted
  // quickly once and then settled slower would otherwise report the
  // flattering figure. NORMALIZE3 got the same treatment and its derivation
  // turned out two clocks pessimistic.
  {
    dut.v_valid_i = 1;
    dut.a_i = static_cast<uint32_t>(3 << 16);
    dut.r_ready_i = 1;
    dut.eval();

    int accepts = 0, gap = 0, first_gap = -1, worst_gap = 0;
    for (int c = 0; c < 4096 && accepts < 5; ++c) {
      const bool accept = dut.v_valid_i && dut.v_ready_o;
      if (accept) {
        if (accepts > 0) {
          if (first_gap < 0) first_gap = gap;
          if (gap > worst_gap) worst_gap = gap;
        }
        ++accepts;
        gap = 0;
      }
      zhao::tick(dut);
      dut.eval();
      ++gap;
    }
    dut.v_valid_i = 0;
    dut.eval();

    std::printf("  RCP measured II = %d clocks (accepts seen: %d)\n", first_gap, accepts);
    check(accepts >= 2, "the unit accepts repeatedly with valid held high", 2, accepts);
    check(first_gap > 0 && first_gap < 512, "the measured II is a sane positive figure", 1,
          (first_gap > 0 && first_gap < 512) ? 1 : 0);
    check(first_gap == worst_gap, "the initiation interval is STEADY, not a first-shot figure",
          first_gap, worst_gap);
  }

  return zhao::report_and_exit("field_rcp_directed");
}
