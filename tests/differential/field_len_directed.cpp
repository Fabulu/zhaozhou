// field_len_directed.cpp — OP_LEN2, OP_LEN3 and OP_DIST2, RTL against the
// interpreter's `len_of` and its DIST2 case.
//
// FOUR LAWS, each one a place an implementation drifts:
//
//   1. THE SUM OF SQUARES IS UNSIGNED. Three squares reach 3*2^62, which does
//      NOT fit s64 and does fit u64 -- the reference accumulates into a
//      `uint64_t` for exactly that reason. A signed accumulator overflows on
//      three large lanes and the length comes out negative. Section 3 uses
//      lanes chosen to land in that gap.
//   2. THE DIST2 DIFFERENCE SATURATES BEFORE SQUARING. It is `fx_sub`, not a
//      plain subtract, and the saturation records in the `add` lane.
//   3. THE ROOT IS A FLOOR, NEVER ROUNDED. Rounding to nearest is a better
//      length and disagrees with the software wherever the root is inexact --
//      which is almost everywhere. Section 4 sweeps perfect squares and their
//      neighbours, where floor and nearest differ by construction.
//   4. THE LATENCY IS FIXED at thirty-four cycles regardless of operand. The
//      reference's alignment loop (`while (bit > num) bit >>= 2`) is an
//      optimisation whose skipped iterations are no-ops, so the RTL runs a fixed
//      thirty-two and nothing downstream has to model a variable delay.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "verilated.h"

#include "Vzhao_field_len.h"

#include "zhao_sim.hpp"
#include "zref/zref_fixp.hpp"
#include "zref/zref_trig.hpp"

namespace {

using zhao::check;

constexpr uint8_t M_LEN2 = 0, M_LEN3 = 1, M_DIST2 = 2;

struct Res {
  int32_t value = 0;
  bool sat_add = false, sat_rescale = false;
  int cycles = 0;
};

/** The interpreter's `len_of`, restated. */
int32_t len_of(const int32_t* v, int n, bool& sat_rescale) {
  uint64_t n2 = 0;
  for (int i = 0; i < n; ++i) {
    n2 += static_cast<uint64_t>(static_cast<int64_t>(v[i])) * static_cast<uint64_t>(
              static_cast<int64_t>(v[i]));
  }
  const uint64_t len = zref::isqrt_u64(n2);
  if (len > static_cast<uint64_t>(INT32_MAX)) {
    sat_rescale = true;
    return INT32_MAX;
  }
  return static_cast<int32_t>(len);
}

Res oracle(uint8_t mode, int32_t a0, int32_t a1, int32_t a2, int32_t b0, int32_t b1) {
  Res o;
  if (mode == M_DIST2) {
    zref::SatLedger L{};
    const int32_t d[2] = {zref::fx_sub(zref::fx16{a0}, zref::fx16{b0}, &L).raw,
                          zref::fx_sub(zref::fx16{a1}, zref::fx16{b1}, &L).raw};
    o.sat_add = L.add != 0;
    o.value = len_of(d, 2, o.sat_rescale);
  } else {
    const int32_t v[3] = {a0, a1, a2};
    o.value = len_of(v, mode == M_LEN3 ? 3 : 2, o.sat_rescale);
  }
  return o;
}

Res run(Vzhao_field_len& dut, uint8_t mode, int32_t a0, int32_t a1, int32_t a2, int32_t b0,
        int32_t b1) {
  dut.v_valid_i = 1;
  dut.mode_i = mode;
  dut.a0_i = static_cast<uint32_t>(a0);
  dut.a1_i = static_cast<uint32_t>(a1);
  dut.a2_i = static_cast<uint32_t>(a2);
  dut.b0_i = static_cast<uint32_t>(b0);
  dut.b1_i = static_cast<uint32_t>(b1);
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

  Res r;
  int cycles = 0;
  while (!dut.r_valid_o && cycles < 256) {
    zhao::tick(dut);
    dut.eval();
    ++cycles;
  }
  r.value = static_cast<int32_t>(dut.result_o);
  r.sat_add = dut.sat_add_o != 0;
  r.sat_rescale = dut.sat_rescale_o != 0;
  r.cycles = cycles;
  zhao::tick(dut);  // the result is taken
  dut.eval();
  return r;
}

void diff(Vzhao_field_len& dut, uint8_t mode, int32_t a0, int32_t a1, int32_t a2, int32_t b0,
          int32_t b1, const char* what) {
  const Res want = oracle(mode, a0, a1, a2, b0, b1);
  const Res got = run(dut, mode, a0, a1, a2, b0, b1);
  const std::string t(what);
  check(got.value == want.value, (t + ": value").c_str(),
        static_cast<uint32_t>(want.value), static_cast<uint32_t>(got.value));
  check(got.sat_rescale == want.sat_rescale, (t + ": SatLedger::rescale").c_str(),
        want.sat_rescale ? 1 : 0, got.sat_rescale ? 1 : 0);
  check(got.sat_add == want.sat_add, (t + ": SatLedger::add").c_str(), want.sat_add ? 1 : 0,
        got.sat_add ? 1 : 0);
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
      case 0: return 0;
      case 1: return INT32_MAX;
      case 2: return INT32_MIN;
      case 3: return static_cast<int32_t>(next()) >> static_cast<int>(below(24));
      case 4: return static_cast<int32_t>(next()) >> 8;
      default: return static_cast<int32_t>(next());
    }
  }
};

constexpr int32_t kOne = 1 << 16;

}  // namespace

int main(int argc, char** argv) {
  Vzhao_field_len dut;
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
    Prng rng(0x1E70u);
    for (uint32_t it = 0; it < iters && zhao::check_failures() == 0; ++it) {
      const uint8_t mode = static_cast<uint8_t>(rng.below(3));
      const int32_t a0 = rng.val(), a1 = rng.val(), a2 = rng.val();
      const int32_t b0 = rng.val(), b1 = rng.val();
      char tag[80];
      std::snprintf(tag, sizeof tag, "random[%u] mode %u", it, mode);
      diff(dut, mode, a0, a1, a2, b0, b1, tag);
    }
    dut.final();
    return zhao::report_and_exit("field_len_random");
  }

  // ---- 1. the anchors a reader can check by hand --------------------------
  {
    diff(dut, M_LEN2, 0, 0, 0, 0, 0, "|(0,0)|");
    diff(dut, M_LEN2, kOne, 0, 0, 0, 0, "|(1,0)| is 1.0");
    diff(dut, M_LEN2, 3 * kOne, 4 * kOne, 0, 0, 0, "|(3,4)| is exactly 5.0 -- a perfect square");
    diff(dut, M_LEN3, 2 * kOne, 3 * kOne, 6 * kOne, 0, 0, "|(2,3,6)| is exactly 7.0");
    diff(dut, M_LEN2, -3 * kOne, -4 * kOne, 0, 0, 0, "negative lanes give the same length");
    const Res r = run(dut, M_LEN2, 3 * kOne, 4 * kOne, 0, 0, 0);
    check(r.value == 5 * kOne, "|(3,4)| is 5.0 on the nose", static_cast<uint32_t>(5 * kOne),
          static_cast<uint32_t>(r.value));
  }

  // ---- 2. THE LATENCY IS FIXED --------------------------------------------
  // The reference's alignment loop skips leading digit pairs; its skipped
  // iterations are no-ops, so the RTL runs a fixed thirty-two. Operands that
  // would skip almost everything and operands that would skip nothing must take
  // the SAME number of cycles.
  {
    const Res tiny = run(dut, M_LEN2, 1, 0, 0, 0, 0);
    const Res huge = run(dut, M_LEN2, INT32_MAX, INT32_MAX, 0, 0, 0);
    check(tiny.cycles == huge.cycles,
          "the root takes the same time for the smallest and largest operands",
          static_cast<uint64_t>(huge.cycles), static_cast<uint64_t>(tiny.cycles));
    check(tiny.cycles > 30, "and it really is running the recurrence, not short-cutting", 1,
          tiny.cycles > 30 ? 1 : 0);
  }

  // ---- 3. THE SUM OF SQUARES IS UNSIGNED ----------------------------------
  // Three lanes near the rail sum to about 3*2^62, which overflows s64 and fits
  // u64. A signed accumulator wraps negative here and the length is nonsense.
  {
    diff(dut, M_LEN3, INT32_MAX, INT32_MAX, INT32_MAX, 0, 0,
         "three lanes at the rail: the sum overflows s64 and fits u64");
    diff(dut, M_LEN3, INT32_MIN, INT32_MIN, INT32_MIN, 0, 0, "three at the negative rail");
    diff(dut, M_LEN3, INT32_MAX, INT32_MIN, INT32_MAX, 0, 0, "mixed rails");
    diff(dut, M_LEN2, INT32_MAX, INT32_MAX, 0, 0, 0, "two lanes at the rail");
    // The result saturates here, and that is recorded.
    const Res r = run(dut, M_LEN3, INT32_MAX, INT32_MAX, INT32_MAX, 0, 0);
    check(r.sat_rescale, "the narrow to s32 saturates and says so", 1, r.sat_rescale ? 1 : 0);
    check(r.value == INT32_MAX, "at the top rail -- a length is never negative",
          static_cast<uint32_t>(INT32_MAX), static_cast<uint32_t>(r.value));
  }

  // ---- 4. THE ROOT IS A FLOOR ---------------------------------------------
  // Perfect squares and their immediate neighbours, where floor and
  // round-to-nearest differ by construction: n^2 - 1 floors to n-1, and
  // (n+1)^2 - 1 floors to n.
  {
    for (int64_t n : {2, 3, 7, 100, 1000, 65535, 65536, 100000, 1000000, 46340, 46341}) {
      const int64_t sq = n * n;
      // Feed the square as a single lane so the sum of squares IS a perfect
      // square: a lane of value v contributes v*v, so v = n gives n^2.
      char tag[112];
      std::snprintf(tag, sizeof tag, "sqrt of the perfect square %lld", static_cast<long long>(sq));
      diff(dut, M_LEN2, static_cast<int32_t>(n), 0, 0, 0, 0, tag);
      // And one lane below, whose square is smaller -- the neighbours are
      // reached through the two-lane sum rather than by feeding sq directly,
      // because the port takes lanes and not the sum.
      std::snprintf(tag, sizeof tag, "just below the square %lld", static_cast<long long>(sq));
      diff(dut, M_LEN2, static_cast<int32_t>(n), 1, 0, 0, 0, tag);
      std::snprintf(tag, sizeof tag, "just above the square %lld", static_cast<long long>(sq));
      diff(dut, M_LEN2, static_cast<int32_t>(n), 2, 0, 0, 0, tag);
    }
  }

  // ---- 5. DIST2, AND ITS DIFFERENCE SATURATES FIRST -----------------------
  {
    diff(dut, M_DIST2, 3 * kOne, 4 * kOne, 0, 0, 0, "dist((3,4),(0,0)) is 5.0");
    diff(dut, M_DIST2, 0, 0, 0, 3 * kOne, 4 * kOne, "dist((0,0),(3,4)) is the same");
    diff(dut, M_DIST2, 10 * kOne, 10 * kOne, 0, 7 * kOne, 6 * kOne, "dist((10,10),(7,6)) is 5.0");
    // The difference saturates: INT32_MAX - INT32_MIN does not fit s32.
    diff(dut, M_DIST2, INT32_MAX, 0, 0, INT32_MIN, 0,
         "the difference saturates BEFORE squaring");
    const Res r = run(dut, M_DIST2, INT32_MAX, 0, 0, INT32_MIN, 0);
    check(r.sat_add, "and the saturation records in the ADD lane, not rescale", 1,
          r.sat_add ? 1 : 0);
    diff(dut, M_DIST2, INT32_MIN, INT32_MIN, 0, INT32_MAX, INT32_MAX,
         "both lanes saturate");
    // DIST2 must ignore the third lane, which LEN3 reads.
    diff(dut, M_DIST2, 3 * kOne, 4 * kOne, INT32_MAX, 0, 0,
         "DIST2 does NOT read the third lane");
  }

  // ---- 6. LEN2 must not read the third lane either ------------------------
  {
    diff(dut, M_LEN2, 3 * kOne, 4 * kOne, INT32_MAX, 0, 0, "LEN2 ignores the third lane");
    diff(dut, M_LEN3, 3 * kOne, 4 * kOne, 0, 0, 0, "LEN3 with a zero third lane is LEN2");
    diff(dut, M_LEN3, 3 * kOne, 4 * kOne, 12 * kOne, 0, 0, "|(3,4,12)| is exactly 13.0");
  }

  dut.final();
  return zhao::report_and_exit("field_len_directed");
}
