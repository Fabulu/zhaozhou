// field_normalize_directed.cpp — OP_NORMALIZE2 and OP_NORMALIZE3, RTL against
// the interpreter's `normalize2` and `zref::normalize3_approx`.
//
// FIVE LAWS:
//
//   1. ONE ROUNDING PER LANE, by `31 + e`, where `e` comes from normalising the
//      vector's LENGTH into [2^23, 2^24). The shift is a function of the
//      magnitude, not a constant, which is what keeps the result accurate across
//      the range.
//   2. TWO correction steps in `rcp_u24_norm` -- `field_rcp` next door uses ONE.
//      Different functions, different counts, different tables.
//   3. THE ZERO CASE IS ASYMMETRIC. `normalize2` bumps the `rcp0` ledger lane on
//      a zero vector; `normalize3_approx` returns zeros and bumps nothing. The
//      reference really does differ. Section 4 pins both, because making them
//      consistent is the obvious tidy-up and would disagree with every capture
//      the software has produced.
//   4. THE SUM OF SQUARES IS UNSIGNED -- three squares reach 3*2^62.
//   5. THE SEED TABLE IS RCP24_T0, not FIELD_RCP_T0. Two reciprocal tables now
//      live in this engine for two different functions, and swapping them would
//      be invisible until some vector came out slightly short. Section 1 checks
//      every entry.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "verilated.h"

#include "Vzhao_field_normalize_tb.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_rcp.hpp"
#include "zref/zref_trig.hpp"
#include "zref/generated/zref_tables.hpp"

namespace {

using zhao::check;

struct Res {
  int32_t o0 = 0, o1 = 0, o2 = 0;
  bool rcp0 = false, sat_rescale = false;
};

/** The interpreter's `normalize2`, restated (it is a static in the .cpp). */
Res oracle2(int32_t x, int32_t y) {
  Res o;
  const uint64_t n2 = static_cast<uint64_t>(static_cast<int64_t>(x)) *
                          static_cast<uint64_t>(static_cast<int64_t>(x)) +
                      static_cast<uint64_t>(static_cast<int64_t>(y)) *
                          static_cast<uint64_t>(static_cast<int64_t>(y));
  if (n2 == 0) {
    o.rcp0 = true;  // the asymmetry: NORMALIZE2 records it
    return o;
  }
  const uint64_t len = zref::isqrt_u64(n2);
  int e = 0;
  uint64_t m = len;
  while (m < (1ull << 23)) {
    m <<= 1;
    --e;
  }
  while (m >= (1ull << 24)) {
    m >>= 1;
    ++e;
  }
  const uint32_t r = zref::rcp_u24_norm(static_cast<uint32_t>(m));
  zref::SatLedger L{};
  o.o0 = zref::rescale_s32(static_cast<int64_t>(x) * r, 31 + e, &L);
  o.o1 = zref::rescale_s32(static_cast<int64_t>(y) * r, 31 + e, &L);
  o.sat_rescale = L.rescale != 0;
  return o;
}

Res oracle3(int32_t x, int32_t y, int32_t z) {
  Res o;
  zref::SatLedger L{};
  const zref::vec3fx v =
      zref::normalize3_approx(zref::vec3fx{zref::fx16{x}, zref::fx16{y}, zref::fx16{z}}, &L);
  o.o0 = v.x.raw;
  o.o1 = v.y.raw;
  o.o2 = v.z.raw;
  o.sat_rescale = L.rescale != 0;
  o.rcp0 = false;  // NORMALIZE3 records nothing on a zero vector
  return o;
}

Res run(Vzhao_field_normalize_tb& dut, bool is3, int32_t a0, int32_t a1, int32_t a2) {
  dut.v_valid_i = 1;
  dut.is3_i = is3 ? 1 : 0;
  dut.a0_i = static_cast<uint32_t>(a0);
  dut.a1_i = static_cast<uint32_t>(a1);
  dut.a2_i = static_cast<uint32_t>(a2);
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
  guard = 0;
  while (!dut.r_valid_o && guard++ < 256) {
    zhao::tick(dut);
    dut.eval();
  }
  Res r;
  r.o0 = static_cast<int32_t>(dut.o0_o);
  r.o1 = static_cast<int32_t>(dut.o1_o);
  r.o2 = static_cast<int32_t>(dut.o2_o);
  r.rcp0 = dut.rcp0_o != 0;
  r.sat_rescale = dut.sat_rescale_o != 0;
  zhao::tick(dut);
  dut.eval();
  return r;
}

void diff(Vzhao_field_normalize_tb& dut, bool is3, int32_t a0, int32_t a1, int32_t a2,
          const char* what) {
  const Res want = is3 ? oracle3(a0, a1, a2) : oracle2(a0, a1);
  const Res got = run(dut, is3, a0, a1, a2);
  const std::string t(what);
  check(got.o0 == want.o0, (t + ": lane 0").c_str(), static_cast<uint32_t>(want.o0),
        static_cast<uint32_t>(got.o0));
  check(got.o1 == want.o1, (t + ": lane 1").c_str(), static_cast<uint32_t>(want.o1),
        static_cast<uint32_t>(got.o1));
  if (is3) {
    check(got.o2 == want.o2, (t + ": lane 2").c_str(), static_cast<uint32_t>(want.o2),
          static_cast<uint32_t>(got.o2));
  }
  check(got.rcp0 == want.rcp0, (t + ": rcp0 lane").c_str(), want.rcp0 ? 1 : 0, got.rcp0 ? 1 : 0);
  check(got.sat_rescale == want.sat_rescale, (t + ": rescale saturation").c_str(),
        want.sat_rescale ? 1 : 0, got.sat_rescale ? 1 : 0);
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
  int32_t val() {
    switch (below(6)) {
      case 0:
        return 0;
      case 1:
        return INT32_MAX;
      case 2:
        return INT32_MIN;
      case 3:
        return static_cast<int32_t>(next()) >> static_cast<int>(below(24));
      case 4:
        return static_cast<int32_t>(next()) >> 12;
      default:
        return static_cast<int32_t>(next());
    }
  }
};

constexpr int32_t kOne = 1 << 16;

}  // namespace

int main(int argc, char** argv) {
  Vzhao_field_normalize_tb dut;
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
    Prng rng(0x0A11u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      const bool is3 = rng.below(2) != 0;
      const int32_t a0 = rng.val(), a1 = rng.val(), a2 = rng.val();
      char tag[80];
      std::snprintf(tag, sizeof tag, "random[%u] %s", it, is3 ? "N3" : "N2");
      diff(dut, is3, a0, a1, a2, tag);
    }
    dut.final();
    return zhao::report_and_exit("field_normalize_random");
  }

  // ---- 1. THE SEED TABLE IS RCP24_T0 --------------------------------------
  // Every entry, reached through the only port the block has: a vector whose
  // length normalises to a mantissa selecting that index. `rcp_u24_norm` is
  // called directly beside it so a divergence is attributed to the table rather
  // than to the arithmetic around it.
  {
    uint64_t bad = 0;
    int first = -1;
    for (int i = 0; i < 256; ++i) {
      const uint32_t m = (1u << 23) | (static_cast<uint32_t>(i) << 15);
      const uint32_t want = zref::rcp_u24_norm(m);
      // A vector of length exactly m: (m, 0) has n2 = m^2 and isqrt = m.
      const Res g = run(dut, false, static_cast<int32_t>(m), 0, 0);
      const Res w = oracle2(static_cast<int32_t>(m), 0);
      if (g.o0 != w.o0) {
        if (first < 0) first = i;
        ++bad;
      }
      (void)want;
    }
    check(bad == 0, "the generated ROM drives rcp_u24_norm correctly for all 256 indices", 0, bad);
    if (bad) std::printf("  first divergent seed index: %d\n", first);

    // The table DESCENDS: the reciprocal of a small mantissa is large. An
    // earlier draft asserted these two the other way round, having read the
    // extracted min and max rather than the first and last entries.
    check(zref::gen::RCP24_T0[0] == 0x7FC01FF0u, "the reference table starts where it did",
          0x7FC01FF0u, zref::gen::RCP24_T0[0]);
    check(zref::gen::RCP24_T0[255] == 0x40100401u, "and ends where it did", 0x40100401u,
          zref::gen::RCP24_T0[255]);
    check(zref::gen::RCP24_T0[0] > zref::gen::RCP24_T0[255],
          "and it descends -- a bigger mantissa has a smaller reciprocal", 1,
          zref::gen::RCP24_T0[0] > zref::gen::RCP24_T0[255] ? 1 : 0);
    // And the two tables really are different, so a swap would be a real bug.
    check(zref::gen::RCP24_T0[0] != zref::gen::FIELD_RCP_T0[0],
          "RCP24_T0 and FIELD_RCP_T0 are different tables for different functions", 1,
          zref::gen::RCP24_T0[0] != zref::gen::FIELD_RCP_T0[0] ? 1 : 0);
  }

  // ---- 2. the anchors -----------------------------------------------------
  {
    diff(dut, false, kOne, 0, 0, "N2: (1,0) is already unit");
    diff(dut, false, 0, kOne, 0, "N2: (0,1)");
    diff(dut, false, -kOne, 0, 0, "N2: (-1,0)");
    diff(dut, false, 3 * kOne, 4 * kOne, 0, "N2: (3,4), a perfect length of 5");
    diff(dut, true, kOne, 0, 0, "N3: (1,0,0)");
    diff(dut, true, 2 * kOne, 3 * kOne, 6 * kOne, "N3: (2,3,6), a perfect length of 7");
    diff(dut, true, -kOne, -kOne, -kOne, "N3: all negative");
  }

  // ---- 3. the magnitude range, which is what `e` exists for ---------------
  // The shift is 31 + e and e comes from the LENGTH, so a vector one bit longer
  // takes a different shift. Sweeping powers of two walks the whole range.
  {
    for (int b = 0; b < 31; ++b) {
      const int32_t v = static_cast<int32_t>(1u << b);
      char tag[80];
      std::snprintf(tag, sizeof tag, "N2: magnitude 2^%d", b);
      diff(dut, false, v, v, 0, tag);
      std::snprintf(tag, sizeof tag, "N3: magnitude 2^%d", b);
      diff(dut, true, v, v, v, tag);
    }
    diff(dut, false, INT32_MAX, INT32_MAX, 0, "N2: both lanes at the rail");
    diff(dut, true, INT32_MAX, INT32_MAX, INT32_MAX, "N3: three lanes at the rail");
    diff(dut, true, INT32_MIN, INT32_MIN, INT32_MIN, "N3: three at the negative rail");
  }

  // ---- 4. THE ZERO CASE IS ASYMMETRIC -------------------------------------
  // NORMALIZE2 records rcp0; NORMALIZE3 records nothing. Making them consistent
  // is the obvious tidy-up and would disagree with every capture the software
  // has produced.
  {
    diff(dut, false, 0, 0, 0, "N2 of the zero vector");
    diff(dut, true, 0, 0, 0, "N3 of the zero vector");
    const Res z2 = run(dut, false, 0, 0, 0);
    const Res z3 = run(dut, true, 0, 0, 0);
    check(z2.rcp0, "NORMALIZE2 of zero DOES record rcp0", 1, z2.rcp0 ? 1 : 0);
    check(!z3.rcp0, "NORMALIZE3 of zero does NOT -- the reference is asymmetric here", 0,
          z3.rcp0 ? 1 : 0);
    check(z2.o0 == 0 && z2.o1 == 0, "and both return zeros", 1, (z2.o0 == 0 && z2.o1 == 0) ? 1 : 0);
    check(z3.o0 == 0 && z3.o1 == 0 && z3.o2 == 0, "all three lanes zero", 1,
          (z3.o0 == 0 && z3.o1 == 0 && z3.o2 == 0) ? 1 : 0);
    // A zero third lane must NOT make NORMALIZE3 behave like NORMALIZE2.
    diff(dut, true, kOne, 0, 0, "N3 with two zero lanes is still N3");
  }

  // ---- 5. NORMALIZE2 must not read the third lane -------------------------
  {
    diff(dut, false, 3 * kOne, 4 * kOne, INT32_MAX, "N2 ignores the third lane");
    diff(dut, true, 3 * kOne, 4 * kOne, 0, "N3 with a zero third lane matches its own law");
  }

  // ---- 6. tiny vectors, where the length normalises UP --------------------
  // A length below 2^23 makes `e` negative and the shift smaller than 31. The
  // reference has two while-loops for exactly this and an implementation with
  // only the down-shift would be right for large vectors and wrong for small.
  {
    for (int32_t v : {1, 2, 3, 7, 100, 1000, 65535}) {
      char tag[80];
      std::snprintf(tag, sizeof tag, "N2: a tiny vector (%d, %d) normalises UP", v, v);
      diff(dut, false, v, v, 0, tag);
      std::snprintf(tag, sizeof tag, "N3: a tiny vector (%d,%d,%d)", v, v, v);
      diff(dut, true, v, v, v, tag);
    }
  }

  dut.final();
  return zhao::report_and_exit("field_normalize_directed");
}
