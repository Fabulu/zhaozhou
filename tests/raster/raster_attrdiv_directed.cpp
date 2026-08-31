// raster_attrdiv_directed.cpp — the attribute divide against the oracle's own
// rounding, including the cases that only differ by one LSB.
//
// ---------------------------------------------------------------------------
// WHAT IS AT RISK
// ---------------------------------------------------------------------------
// This block sets the throughput of the entire textured path -- one divide per
// attribute per pixel -- but throughput is not what can be WRONG about it. What
// can be wrong is the rounding, and the ways it can be wrong are invisible:
//
//   * floor instead of round-half-up: off by one on most non-exact quotients;
//   * round-half-to-even instead of half-up: off by one on exact halves only;
//   * arithmetic shift instead of magnitude-then-sign: off by one on negative
//     values only, and correct on every positive one.
//
// A test that divides a handful of pleasant numbers passes all three. So this
// file drives EXACT HALVES deliberately, drives the same magnitudes with both
// signs, and sweeps a pseudo-random spread wide enough to hit the carries.
//
// The reference's law, restated rather than included, because comparing two
// independent statements of it is the point:
//
//     n >= 0 :   (2n + d) / (2d)
//     n <  0 :  -((-2n + d) / (2d))

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_raster_attrdiv.h"

#include "zhao_sim.hpp"

namespace {

int64_t div_rhu(__int128 n, int64_t d) {
  const __int128 dd = d;
  return static_cast<int64_t>((n >= 0) ? ((2 * n + dd) / (2 * dd)) : -((-2 * n + dd) / (2 * dd)));
}

void put96(Vzhao_raster_attrdiv& t, __int128 v) {
  for (int i = 0; i < 3; ++i) t.num_i[i] = static_cast<uint32_t>((v >> (32 * i)) & 0xFFFFFFFFu);
}

/** One divide, start to finish. Returns the clocks it took. */
int run_one(Vzhao_raster_attrdiv& t, __int128 num, uint64_t area, int64_t* q, bool* ovf) {
  put96(t, num);
  t.area_i = area;
  t.v_valid_i = 1;
  int clocks = 0;
  // accept
  for (;;) {
    t.eval();
    const bool taken = t.v_ready_o != 0;
    zhao::tick(t);
    ++clocks;
    if (taken) break;
    if (clocks > 200) return -1;
  }
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  for (;;) {
    t.eval();
    if (t.r_valid_o) {
      *q = static_cast<int32_t>(t.q_o);
      *ovf = t.r_valid_o && t.q_overflow_o;
      zhao::tick(t);
      ++clocks;
      return clocks;
    }
    zhao::tick(t);
    ++clocks;
    if (clocks > 200) return -1;
  }
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_attrdiv top;

  top.rst_n = 0;
  top.v_valid_i = 0;
  top.r_ready_i = 1;
  top.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(top);
  top.rst_n = 1;
  top.eval();

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: exact halves, where half-up and half-even disagree ==\n");
  {
    // n/d == k + 1/2 exactly, for both signs. Half-up gives k+1; half-even
    // gives k or k+1 depending on parity, and floor gives k. Only one of the
    // three matches the reference on all of them.
    const int64_t areas[] = {2, 6, 10, 1024, 65536};
    int bad = 0, cases = 0;
    for (int64_t d : areas)
      for (int64_t k = -4; k <= 4; ++k) {
        // n = d*k + d/2 gives exactly k + 1/2 when d is even.
        const __int128 n = static_cast<__int128>(d) * k + d / 2;
        int64_t q = 0;
        bool ovf = false;
        if (run_one(top, n, static_cast<uint64_t>(d), &q, &ovf) < 0) {
          ++bad;
          continue;
        }
        const int64_t want = div_rhu(n, d);
        if (q != want || ovf) {
          if (bad < 4)
            printf("      d=%lld k=%lld: want %lld got %lld%s\n", (long long)d, (long long)k,
                   (long long)want, (long long)q, ovf ? " OVERFLOW" : "");
          ++bad;
        }
        ++cases;
      }
    zhao::check(bad == 0, "exact halves round the way the reference rounds", 0, (uint32_t)bad);
    printf("   MEASURED: %d exact-half cases\n", cases);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: the same magnitude with both signs ==\n");
  {
    // The failure this catches is an arithmetic shift or a floor: correct on
    // every positive value and off by one on the negatives.
    // Magnitudes chosen so the QUOTIENT stays inside 32 bits, which is the
    // block's stated precondition -- a quotient of 733 * 2^31 is the ceiling
    // here. My first version used 2^60, whose quotient needs 51 bits; the block
    // correctly reported overflow instead of truncating, and the test was what
    // was wrong. The overflow path gets its own case in section 4.
    const __int128 mags[] = {1, 7, 12345, (__int128)1 << 30, ((__int128)1 << 40) + 12345};
    int bad = 0;
    for (__int128 m : mags)
      for (int s = 0; s < 2; ++s) {
        const __int128 n = s ? -m : m;
        const uint64_t d = 733;
        int64_t q = 0;
        bool ovf = false;
        if (run_one(top, n, d, &q, &ovf) < 0) {
          ++bad;
          continue;
        }
        const int64_t want = div_rhu(n, static_cast<int64_t>(d));
        if (q != want) {
          printf("      sign=%d: want %lld got %lld\n", s, (long long)want, (long long)q);
          ++bad;
        }
      }
    zhao::check(bad == 0, "negative numerators are magnitude-then-sign, not a shift", 0,
                (uint32_t)bad);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: a wide pseudo-random sweep ==\n");
  {
    uint64_t s = 0xC0FFEEull;
    auto nxt = [&s]() {
      s ^= s << 13;
      s ^= s >> 7;
      s ^= s << 17;
      return s;
    };
    int bad = 0, cases = 0;
    int64_t worst = 0, best = 1 << 30;
    for (int i = 0; i < 400; ++i) {
      const uint64_t d = (nxt() % 100000ull) + 1ull;
      // A numerator whose quotient stays inside 32 bits, which is the block's
      // stated precondition.
      const __int128 q_want = static_cast<int64_t>(nxt() % 2000000ull) - 1000000;
      const __int128 n = q_want * static_cast<__int128>(d) + static_cast<int64_t>(nxt() % d);
      int64_t q = 0;
      bool ovf = false;
      const int clocks = run_one(top, n, d, &q, &ovf);
      if (clocks < 0) {
        ++bad;
        continue;
      }
      if (clocks > worst) worst = clocks;
      if (clocks < best) best = clocks;
      const int64_t want = div_rhu(n, static_cast<int64_t>(d));
      if (q != want || ovf) {
        if (bad < 4)
          printf("      d=%llu: want %lld got %lld%s\n", (unsigned long long)d, (long long)want,
                 (long long)q, ovf ? " OVERFLOW" : "");
        ++bad;
      }
      ++cases;
    }
    zhao::check(bad == 0, "every random divide matches the reference exactly", 0, (uint32_t)bad);
    printf("   MEASURED: %d divides, %lld..%lld clocks each\n", cases, (long long)best,
           (long long)worst);
    printf("   THROUGHPUT: at %lld clocks a divide, one divider sustains %lld attribute-pixels\n",
           (long long)worst, (long long)(1666667 / worst));
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: a broken precondition is REPORTED, not guessed ==\n");
  {
    int64_t q = 0;
    bool ovf = false;
    // area 0 is a degenerate triangle; GEOM.CLIP rejects it, but the block must
    // not divide forever if one ever arrives.
    const int c = run_one(top, 12345, 0, &q, &ovf);
    zhao::check(c > 0, "an area of zero still terminates", 1, c > 0 ? 1 : 0);
    zhao::check(ovf, "and is reported as an overflow rather than answered", 1, ovf ? 1 : 0);

    // A quotient too large for 32 bits is the other broken precondition, and it
    // is the one a caller reaches by accident. It must be REPORTED, because a
    // truncated attribute is a plausible wrong colour rather than a visible
    // failure.
    const int c2 = run_one(top, ((__int128)1 << 60) + 12345, 733, &q, &ovf);
    zhao::check(c2 > 0, "an over-large quotient still terminates", 1, c2 > 0 ? 1 : 0);
    zhao::check(ovf, "and is reported rather than silently truncated", 1, ovf ? 1 : 0);
  }

  // ------------------------------------------------------------------ 5 ---
  printf("== section 5: a result that is not consumed is not overwritten ==\n");
  {
    // Every case above holds r_ready_i high, so none of them can see what
    // happens when the consumer is BUSY -- which is the normal case behind the
    // service, where one divider's answer waits for its turn in issue order.
    // The block returns to idle in the same clock it raises r_valid_o, so a
    // ready that looked only at the state would accept a second divide here and
    // clobber the first answer 34 clocks later, under a raised valid.
    top.rst_n = 0;
    top.v_valid_i = 0;
    top.r_ready_i = 0;
    top.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(top);
    top.rst_n = 1;
    top.eval();

    // First divide, answer deliberately left unconsumed.
    const __int128 n1 = 1234567;
    const uint64_t d1 = 89;
    put96(top, n1);
    top.area_i = d1;
    top.v_valid_i = 1;
    for (int i = 0; i < 60; ++i) {
      top.eval();
      if (top.v_ready_o) {
        zhao::tick(top);
        break;
      }
      zhao::tick(top);
    }
    top.v_valid_i = 0;
    int waited = 0;
    for (; waited < 100; ++waited) {
      top.eval();
      if (top.r_valid_o) break;
      zhao::tick(top);
    }
    zhao::check(waited < 100, "the first divide answers", 1, waited < 100 ? 1 : 0);
    const int64_t held = static_cast<int32_t>(top.q_o);

    // Now offer a SECOND, different divide for 80 clocks with the answer still
    // unconsumed. It must be refused for every one of them, and the held answer
    // must still be the first one's.
    put96(top, 999999999);
    top.area_i = 7;
    top.v_valid_i = 1;
    int accepted = 0;
    for (int i = 0; i < 80; ++i) {
      top.eval();
      if (top.v_ready_o) ++accepted;
      zhao::tick(top);
    }
    top.v_valid_i = 0;
    top.eval();
    zhao::check(accepted == 0, "no second divide is accepted while the answer waits", 0,
                (uint32_t)accepted);
    zhao::check(top.r_valid_o == 1, "the answer is still being offered", 1,
                (uint32_t)top.r_valid_o);
    const int64_t want1 = div_rhu(n1, static_cast<int64_t>(d1));
    zhao::check(static_cast<int32_t>(top.q_o) == want1 && held == want1,
                "and it is still the FIRST divide's value, not overwritten", 1,
                (static_cast<int32_t>(top.q_o) == want1 && held == want1) ? 1 : 0);

    // Consume it; the block must then take work again.
    top.r_ready_i = 1;
    zhao::tick(top);
    top.eval();
    zhao::check(top.v_ready_o == 1, "consuming the answer reopens the block", 1,
                (uint32_t)top.v_ready_o);
  }

  // ------------------------------------------------------------------ 6 ---
  printf("== section 6: the remainder, which the stepping path seeds from ==\n");
  {
    // tests/proofs/attribute_step_equivalence.cpp replaces the per-pixel divide
    // with a quotient/remainder recurrence, and that recurrence SEEDS from this
    // block's remainder. A port nobody checks is a port that will be wrong.
    //
    // The block divides (2|n| + d) by 2d, so the remainder must satisfy
    //     2|n| + d  ==  q_mag * 2d + rem,   0 <= rem < 2d
    // with q_mag the magnitude of the quotient. That identity is the whole
    // contract, and it is checked rather than a recomputed expectation.
    top.rst_n = 0;
    top.v_valid_i = 0;
    top.r_ready_i = 1;
    top.eval();
    for (int i = 0; i < 3; ++i) zhao::tick(top);
    top.rst_n = 1;
    top.eval();

    uint64_t s = 0xBEEF77ull;
    auto nxt = [&s]() {
      s ^= s << 13;
      s ^= s >> 7;
      s ^= s << 17;
      return s;
    };
    long bad = 0, range_bad = 0, cases = 0;
    for (int i = 0; i < 300; ++i) {
      const uint64_t d = (nxt() % 100000ull) + 1ull;
      const __int128 qw = static_cast<int64_t>(nxt() % 2000000ull) - 1000000;
      const __int128 n = qw * static_cast<__int128>(d) + static_cast<int64_t>(nxt() % d);
      int64_t q = 0;
      bool ovf = false;
      if (run_one(top, n, d, &q, &ovf) < 0 || ovf) continue;
      const __int128 rem = static_cast<__int128>(top.rem_o);
      const __int128 D = 2 * static_cast<__int128>(d);
      const __int128 an = (n < 0) ? -n : n;
      const __int128 M = 2 * an + static_cast<__int128>(d);
      const __int128 qmag = (q < 0) ? -static_cast<__int128>(q) : static_cast<__int128>(q);
      if (rem < 0 || rem >= D) ++range_bad;
      if (M != qmag * D + rem) {
        if (bad < 3) printf("      d=%llu: M != q*D + rem\n", (unsigned long long)d);
        ++bad;
      }
      ++cases;
    }
    printf("   MEASURED: %ld divides checked against M == q*D + rem\n", cases);
    zhao::check(bad == 0, "the remainder closes the division identity exactly", 0, (uint32_t)bad);
    zhao::check(range_bad == 0, "and it is Euclidean: 0 <= rem < 2*area", 0, (uint32_t)range_bad);
    zhao::check(cases > 200, "over enough divides to mean something", 1, cases > 200 ? 1 : 0);
  }

  return zhao::report_and_exit("raster_attrdiv_directed");
}
