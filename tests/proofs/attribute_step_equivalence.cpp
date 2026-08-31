// attribute_step_equivalence.cpp — can the per-pixel divide be replaced by an
// exact quotient/remainder recurrence, and does the proposed formula match the
// shipped law?
//
// OWNER RULING 2026-08-31 (reports/OWNER-RULINGS-20260831.md, ruling 2): do not
// freeze a large per-pixel ATTRDIV farm. First prove and prototype exact
// quotient/remainder stepping, keeping the divider as oracle and fallback.
//
// The ruling states the target as
//
//     q(x,y) = floor( (N(x,y) + floor(A/2)) / A )
//
// and calls it "precisely the signed round-half-up law already used by
// ATTRDIV". This file checks that before anything is built on it, because a
// stepping scheme that is exact for the WRONG law is worse than no scheme.
//
// ---------------------------------------------------------------------------
// FINDING 1: THE STATED FORMULA IS NOT THE SHIPPED LAW
// ---------------------------------------------------------------------------
// rast.cpp rounds half AWAY FROM ZERO, symmetric:
//
//     n >= 0 :   (2n + A) / (2A)
//     n <  0 :  -((-2n + A) / (2A))
//
// `floor((n + floor(A/2))/A)` rounds half UP, toward +infinity. The two agree
// everywhere except at NEGATIVE EXACT HALVES, where they differ by one:
//
//     n = -1, A = 2:   shipped -1,   floor-form 0
//
// Section 1 measures exactly where and how often. This is not a small caveat:
// RASTER.ATTRDIV's own directed test drives 45 exact halves deliberately,
// because that is the one place three plausible roundings disagree. Adopting
// the floor form silently would move every golden capture CRC -- a Class C
// change, not an implementation detail.
//
// ---------------------------------------------------------------------------
// FINDING 2: A SIGN-AWARE RECURRENCE IS EXACT FOR THE LAW WE HAVE
// ---------------------------------------------------------------------------
// The fix is small, and it keeps every rendered bit. Write the shipped law as
// two floor divisions, one per sign of N:
//
//     N >= 0 :  q =  floor(M  / D),  M  =  2N + A,   M  steps by  2*Dx
//     N <  0 :  q = -floor(M' / D),  M' = -2N + A,   M' steps by -2*Dx
//
// with D = 2A. Within one sign region each is a plain floor division of an
// integer that advances by a constant, so the Euclidean recurrence
//
//     q += qd;  r += rd;  if (r >= D) { r -= D; ++q; }
//
// is exact, where (qd, rd) is the Euclidean decomposition of the step. N itself
// is stepped as an exact integer add -- RASTER.INTERP already does that -- so
// the sign is known for free, and a crossing simply reseeds the (q, r) pair.
//
// A row is 16 pixels and N is linear, so a row crosses zero AT MOST ONCE. The
// cost is therefore one divide per row per attribute, plus at most one more at
// a crossing: sixteen to eight times fewer than one divide per pixel.
//
// Section 3 puts the crossing INSIDE the row deliberately, because a proof that
// never reseeds has not tested the only interesting case.
//
//   g++ -std=c++17 -O2 tests/proofs/attribute_step_equivalence.cpp -o attrstep

#include <cstdint>
#include <cstdio>

namespace {

using i128 = __int128;

/** The shipped law, restated from rast.cpp rather than included. */
i128 div_rhu(i128 n, i128 A) {
  return (n >= 0) ? ((2 * n + A) / (2 * A)) : -((-2 * n + A) / (2 * A));
}

/** The formula the ruling states. Python-style floor division. */
i128 floor_form(i128 n, i128 A) {
  const i128 num = n + A / 2;
  i128 q = num / A;
  if ((num % A) != 0 && ((num < 0) != (A < 0))) --q;  // C++ truncates; we want floor
  return q;
}

/** Euclidean division: 0 <= r < D for D > 0. */
void edivmod(i128 M, i128 D, i128* q, i128* r) {
  i128 qq = M / D;
  i128 rr = M - qq * D;
  if (rr < 0) {
    --qq;
    rr += D;
  }
  *q = qq;
  *r = rr;
}

struct Stats {
  long divides = 0;
  long reseeds = 0;
};

/**
 * Walk `width` pixels of one attribute with the sign-aware recurrence.
 * Writes the quotients into `out`. This is the algorithm a RASTER.ATTRSTEP
 * block would implement: one add and one compare-subtract per pixel, plus a
 * reseed when N changes sign.
 */
void step_row(i128 N0, i128 Dx, i128 A, int width, i128* out, Stats* st) {
  const i128 D = 2 * A;
  i128 N = N0;
  i128 q = 0, r = 0;
  int sign = -1;  // no branch seeded yet
  for (int i = 0; i < width; ++i) {
    const int s = (N >= 0) ? 0 : 1;
    if (s != sign) {
      const i128 M = (s == 0) ? (2 * N + A) : (-2 * N + A);
      edivmod(M, D, &q, &r);
      ++st->divides;
      if (sign != -1) ++st->reseeds;
      sign = s;
    }
    out[i] = (s == 0) ? q : -q;

    // Advance to the next pixel in the SAME branch; a sign change is caught at
    // the top of the next iteration and reseeds.
    N += Dx;
    const i128 dM = (s == 0) ? (2 * Dx) : (-2 * Dx);
    i128 qd, rd;
    edivmod(dM, D, &qd, &rd);
    q += qd;
    r += rd;
    if (r >= D) {
      r -= D;
      ++q;
    }
  }
}

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  /** A value with a random magnitude up to `bits`, random sign. */
  i128 wide(int bits) {
    const int b = 1 + static_cast<int>(next() % static_cast<uint64_t>(bits));
    i128 v = 0;
    for (int i = 0; i < 3; ++i) v = (v << 32) ^ static_cast<i128>(next() & 0xFFFFFFFFu);
    if (b < 127) v &= ((static_cast<i128>(1) << b) - 1);
    return (next() & 1) ? -v : v;
  }
  i128 positive(int bits) {
    i128 v = wide(bits);
    if (v < 0) v = -v;
    return v ? v : 1;
  }
};

}  // namespace

int main() {
  int failures = 0;

  // ------------------------------------------------------------------ 1 ---
  std::printf("== 1. is floor((N + floor(A/2))/A) the shipped law? ==\n");
  {
    long mism = 0, checked = 0, neg = 0, halves = 0;
    for (i128 A = 1; A <= 40; ++A)
      for (i128 n = -200; n <= 200; ++n) {
        ++checked;
        if (div_rhu(n, A) != floor_form(n, A)) {
          ++mism;
          if (n < 0) ++neg;
          // an exact half is |n|/A == k + 1/2, i.e. 2|n| mod 2A == A
          const i128 an = n < 0 ? -n : n;
          if ((2 * an) % (2 * A) == A) ++halves;
        }
      }
    std::printf("   %ld of %ld small-domain cases DISAGREE\n", mism, checked);
    std::printf("   of those, %ld have n < 0 and %ld are exact halves\n", neg, halves);
    if (mism == 0) {
      std::printf("   FAIL: expected a disagreement; the finding may be stale\n");
      ++failures;
    } else if (neg != mism || halves != mism) {
      std::printf("   FAIL: the disagreement is not confined to negative exact halves\n");
      ++failures;
    } else {
      std::printf("   CONFIRMED: they differ EXACTLY on negative exact halves.\n");
      std::printf("   Adopting the floor form would move every golden capture CRC.\n");
    }
  }

  // ------------------------------------------------------------------ 2 ---
  std::printf("== 2. the sign-aware recurrence, over wide random planes ==\n");
  long total_px = 0, total_div = 0;
  {
    Rng rng(0x5EED20260831ull);
    Stats st;
    long bad = 0, checked = 0;
    i128 out[16];
    for (int t = 0; t < 20000; ++t) {
      const i128 A = rng.positive(46);  // 2A after winding normalisation
      const i128 N0 = rng.wide(78);     // the numerator plane's origin
      const i128 Dx = rng.wide(60);     // one pixel of step
      step_row(N0, Dx, A, 16, out, &st);
      for (int i = 0; i < 16; ++i) {
        const i128 want = div_rhu(N0 + static_cast<i128>(i) * Dx, A);
        ++checked;
        if (out[i] != want) {
          if (bad < 3) std::printf("   MISMATCH at pixel %d\n", i);
          ++bad;
        }
      }
    }
    total_px += checked;
    total_div += st.divides;
    std::printf("   %ld pixel-attributes, %ld mismatches\n", checked, bad);
    std::printf("   %ld divides for %ld pixels = %.3f per pixel\n", st.divides, checked,
                static_cast<double>(st.divides) / static_cast<double>(checked));
    if (bad) ++failures;
  }

  // ------------------------------------------------------------------ 3 ---
  std::printf("== 3. with the zero crossing INSIDE the row ==\n");
  {
    // ANTI-VACUITY. Section 2's planes mostly keep one sign across a 16-pixel
    // row, so the reseed path may never fire. Here the row is constructed so
    // that N passes through zero, which is the only case the recurrence has to
    // think about.
    Rng rng(0xC0FFEE31ull);
    Stats st;
    long bad = 0, checked = 0;
    i128 out[16];
    for (int t = 0; t < 20000; ++t) {
      const i128 A = rng.positive(46);
      i128 Dx = rng.positive(40);
      if (rng.next() & 1) Dx = -Dx;
      const int k = 1 + static_cast<int>(rng.next() % 15);
      const i128 jitter = static_cast<i128>(rng.next() % 8) - 4;
      const i128 N0 = -Dx * k + jitter;
      step_row(N0, Dx, A, 16, out, &st);
      for (int i = 0; i < 16; ++i) {
        const i128 want = div_rhu(N0 + static_cast<i128>(i) * Dx, A);
        ++checked;
        if (out[i] != want) {
          if (bad < 3) std::printf("   MISMATCH at pixel %d\n", i);
          ++bad;
        }
      }
    }
    total_px += checked;
    total_div += st.divides;
    std::printf("   %ld pixel-attributes, %ld mismatches\n", checked, bad);
    std::printf("   %ld reseeds actually fired -- the crossing path IS exercised\n", st.reseeds);
    std::printf("   %ld divides for %ld pixels = %.3f per pixel\n", st.divides, checked,
                static_cast<double>(st.divides) / static_cast<double>(checked));
    if (bad) ++failures;
    if (st.reseeds < 1000) {
      std::printf("   FAIL: too few reseeds; section 3 did not test what it claims\n");
      ++failures;
    }
  }

  // ------------------------------------------------------------------ 4 ---
  std::printf("== 4. what it costs ==\n");
  {
    const double per_px = static_cast<double>(total_div) / static_cast<double>(total_px);
    std::printf("   OVERALL: %.3f divides per pixel-attribute, against 1.000 today\n", per_px);
    std::printf("   = %.1fx fewer divides, with every rendered bit unchanged\n", 1.0 / per_px);
    std::printf("   A row is 16 pixels and N is linear, so a row crosses zero at most\n");
    std::printf("   once: one seed per row, plus at most one reseed.\n");
    if (per_px > 0.2) {
      std::printf("   FAIL: the recurrence is not buying what it should\n");
      ++failures;
    }
  }

  std::printf("%s\n", failures == 0 ? "attribute_step_equivalence: PASS"
                                    : "attribute_step_equivalence: FAILURES ABOVE");
  return failures != 0;
}
