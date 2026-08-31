// raster_perspuv_directed.cpp — is the perspective divide the RIGHT divide, and
// is it transcribed exactly?
//
// ---------------------------------------------------------------------------
// TWO DIFFERENT QUESTIONS, AND THEY NEED DIFFERENT CHECKS
// ---------------------------------------------------------------------------
// spec/qformats.md §8 gives the law as `u = rescale((s64)u_over_w *
// rcp_u24(invw24))` WITHOUT the rescale's k. The block derives k = 32 - k_rcp
// from the three published Q-formats (S 8.24 in, U 0.0.24 depth, S 15.16 out).
// That derivation could be wrong, and checking the RTL against a C++ copy of it
// would only prove the two agree -- a circle.
//
// So there are two checks and they catch different things:
//
//   SEMANTIC (section 1). Against EXACT RATIONAL arithmetic: u is literally
//   u_over_w / invw24, and the answer in S 15.16 is that times 65536. Compared
//   with a tolerance, because the reciprocal is allowed ~1 LSB of relative
//   error. A shift that is off by one is a factor of two -- this catches it by
//   six orders of magnitude, and it does not care what the derivation said.
//
//   EXACT (section 2). Against the stated law computed with zref::rcp_u24.
//   Bit-for-bit, because capture CRCs are. This catches a transcription slip in
//   the rounding or the width that the tolerance would forgive.
//
// Section 1 without section 2 would let a rounding bug through; section 2
// without section 1 would happily certify a self-consistently wrong shift.
//
// ---------------------------------------------------------------------------
// AND THE SHIFT IS DATA DEPENDENT, WHICH IS WHY SECTION 5 EXISTS
// ---------------------------------------------------------------------------
// k_rcp runs 1..24 with the magnitude of the depth, so the shift runs 8..31. A
// block that hard-coded ANY single shift would be exactly right at one depth
// and wrong everywhere else. That failure is only visible if the cases actually
// span the range -- so section 5 counts the distinct exponents the sweep
// exercised and fails if the coverage was too narrow to have caught it.

#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

#include "verilated.h"

#include "Vzhao_raster_perspuv.h"

#include "zhao_sim.hpp"
#include "zref/zref_rcp.hpp"

namespace {

constexpr int64_t kClocksPerFrame = 1666667;

struct Res {
  int32_t u, v;
  uint16_t tag;
  bool sat, zero;
  int clocks;
};

Res one(Vzhao_raster_perspuv& t, int32_t uow, int32_t vow, uint32_t d, uint16_t tag) {
  t.u_over_w_i = static_cast<uint32_t>(uow);
  t.v_over_w_i = static_cast<uint32_t>(vow);
  t.invw24_i = d;
  t.tag_i = tag;
  t.v_valid_i = 1;
  int clocks = 0;
  for (;;) {
    t.eval();
    const bool taken = t.v_ready_o != 0;
    zhao::tick(t);
    ++clocks;
    if (taken) break;
    if (clocks > 400) return {0, 0, 0, false, false, -1};
  }
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  for (;;) {
    t.eval();
    if (t.r_valid_o) {
      const Res r{static_cast<int32_t>(t.u_o),
                  static_cast<int32_t>(t.v_o),
                  static_cast<uint16_t>(t.tag_o),
                  t.sat_o != 0,
                  t.depth_zero_o != 0,
                  clocks + 1};
      zhao::tick(t);
      return r;
    }
    zhao::tick(t);
    ++clocks;
    if (clocks > 400) return {0, 0, 0, false, false, -1};
  }
}

void reset(Vzhao_raster_perspuv& t) {
  t.rst_n = 0;
  t.v_valid_i = 0;
  t.r_ready_i = 1;
  t.eval();
  for (int i = 0; i < 4; ++i) zhao::tick(t);
  t.rst_n = 1;
  t.eval();
}

/** The stated law, computed independently: rescale_s(u_over_w * r, 32 - k). */
int64_t law(int32_t uow, uint32_t d) {
  const zref::rcp24_result rc = zref::rcp_u24(d);
  const int sh = 32 - rc.k;
  const __int128 p = static_cast<__int128>(uow) * rc.r;
  const __int128 q = (p + (static_cast<__int128>(1) << (sh - 1))) >> sh;
  if (q > INT32_MAX) return INT32_MAX;
  if (q < INT32_MIN) return INT32_MIN;
  return static_cast<int64_t>(q);
}

/** The EXACT value, as a rational: (u_over_w / d) * 65536, rounded. */
__int128 exact(int32_t uow, uint32_t d) {
  const __int128 num = static_cast<__int128>(uow) * 65536;
  const __int128 dd = d;
  // round-half-away, symmetric, so the tolerance below is about the reciprocal
  // and not about a rounding convention.
  return (num >= 0) ? (2 * num + dd) / (2 * dd) : -((-2 * num + dd) / (2 * dd));
}

struct Case {
  int32_t uow, vow;
  uint32_t d;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_raster_perspuv top;
  reset(top);

  // Build cases whose recovered coordinates mostly LAND in S 15.16: pick a
  // target texture coordinate and a depth, then form u_over_w = u_tex * d,
  // which is the direction the geometry pipeline actually produces them.
  std::vector<Case> cases;
  {
    const double targets[] = {0.0, 1.0, -1.0, 3.5, -12.25, 100.0, -100.0, 0.001};
    // Depths spanning the whole U 0.0.24 range, so k_rcp spans 1..24.
    for (int e = 0; e < 24; ++e) {
      const uint32_t d = (1u << (23 - e)) | ((e * 37u) & ((1u << (23 - e)) - 1u));
      for (double tu : targets) {
        const int64_t uow = static_cast<int64_t>(tu * static_cast<double>(d));
        const int64_t vow = static_cast<int64_t>(-tu * 0.5 * static_cast<double>(d));
        if (uow > INT32_MAX || uow < INT32_MIN || vow > INT32_MAX || vow < INT32_MIN) continue;
        cases.push_back({static_cast<int32_t>(uow), static_cast<int32_t>(vow), d});
      }
    }
  }

  std::set<int> exponents;
  for (const Case& c : cases) exponents.insert(zref::rcp_u24(c.d).k);

  // ------------------------------------------------------------------ 1 ---
  printf("== section 1: the answer is the actual quotient, checked against exact rationals ==\n");
  {
    long bad = 0, checked = 0, railed = 0;
    for (const Case& c : cases) {
      const Res got = one(top, c.uow, c.vow, c.d, 0x1234);
      if (got.clocks < 0) {
        ++bad;
        continue;
      }
      const __int128 wu = exact(c.uow, c.d), wv = exact(c.vow, c.d);
      if (wu > INT32_MAX || wu < INT32_MIN || wv > INT32_MAX || wv < INT32_MIN) {
        // Genuinely out of range; section 3 owns the railing behaviour.
        ++railed;
        continue;
      }
      // The reciprocal carries about 1 LSB of relative error, so the tolerance
      // is relative. It is still ~2^-20 -- a shift off by one is a factor of
      // two and misses this by six orders of magnitude.
      const __int128 diff = (got.u > wu) ? (got.u - wu) : (wu - got.u);
      const __int128 diffv = (got.v > wv) ? (got.v - wv) : (wv - got.v);
      const __int128 tol_u = 2 + ((wu < 0 ? -wu : wu) >> 20);
      const __int128 tol_v = 2 + ((wv < 0 ? -wv : wv) >> 20);
      if (diff > tol_u || diffv > tol_v) {
        if (bad < 4)
          printf("      uow=%d d=0x%06X: exact u=%lld got %d\n", c.uow, c.d, (long long)wu, got.u);
        ++bad;
      }
      ++checked;
    }
    zhao::check(bad == 0, "every in-range fragment matches the exact quotient", 0, (uint32_t)bad);
    printf("   MEASURED: %ld in-range cases, %ld deliberately out of range\n", checked, railed);
    zhao::check(checked > 60, "and enough of them were in range to mean something", 1,
                checked > 60 ? 1 : 0);
  }

  // ------------------------------------------------------------------ 2 ---
  printf("== section 2: bit-exact against the stated law ==\n");
  {
    long bad = 0;
    for (const Case& c : cases) {
      const Res got = one(top, c.uow, c.vow, c.d, 0x4321);
      if (got.clocks < 0 || got.u != law(c.uow, c.d) || got.v != law(c.vow, c.d)) {
        if (bad < 4)
          printf("      uow=%d d=0x%06X: law u=%lld got %d\n", c.uow, c.d,
                 (long long)law(c.uow, c.d), got.u);
        ++bad;
      }
      if (got.tag != 0x4321) ++bad;
    }
    zhao::check(bad == 0, "u, v and the tag are exactly the law's, on every case", 0,
                (uint32_t)bad);
  }

  // ------------------------------------------------------------------ 3 ---
  printf("== section 3: the horizon rails, and says so ==\n");
  {
    reset(top);
    // A large numerator over a tiny depth: the true coordinate is far outside
    // S 15.16. Saturating means the texture stops moving; wrapping would tear
    // it across the whole surface, which is why this is a rail and not a mask.
    const Res hi = one(top, 2000000000, -2000000000, 1u, 0x7);
    zhao::check(hi.u == INT32_MAX, "a huge positive coordinate rails to +max", (uint32_t)INT32_MAX,
                (uint32_t)hi.u);
    zhao::check(hi.v == INT32_MIN, "and a huge negative one to -min", (uint32_t)INT32_MIN,
                (uint32_t)hi.v);
    zhao::check(hi.sat, "and the rail is reported", 1, hi.sat ? 1 : 0);

    // A fragment that does NOT rail must not claim it did -- otherwise the
    // counter measures nothing.
    const Res ok = one(top, 1 << 20, -(1 << 20), 1u << 23, 0x8);
    zhao::check(!ok.sat, "an in-range fragment does not report a rail", 0, ok.sat ? 1 : 0);
    printf("   MEASURED: %u fragments, %u of them railed\n", (unsigned)top.fragments_o,
           (unsigned)top.sat_fragments_o);
    zhao::check(top.sat_fragments_o >= 1 && top.sat_fragments_o < top.fragments_o,
                "the rail counter counts rails and not everything", 1,
                (top.sat_fragments_o >= 1 && top.sat_fragments_o < top.fragments_o) ? 1 : 0);
  }

  // ------------------------------------------------------------------ 4 ---
  printf("== section 4: one reciprocal for both coordinates, and the rate ==\n");
  {
    reset(top);
    int worst = 0, best = 1 << 20;
    for (int i = 0; i < 500; ++i) {
      const uint32_t d = (static_cast<uint32_t>(i) * 33331u + 7u) & 0xFFFFFFu;
      const Res got = one(top, 1 << 22, -(1 << 21), d ? d : 1u, 0);
      if (got.clocks > worst) worst = got.clocks;
      if (got.clocks < best) best = got.clocks;
    }
    printf("   MEASURED: %d..%d clocks a fragment\n", best, worst);
    const int64_t per_frame = kClocksPerFrame / worst;
    printf("   THROUGHPUT: %lld textured survivors a frame from ONE unit\n", (long long)per_frame);
    printf("   VERDICT: %s against 276480 terrain-primary pixels\n",
           per_frame >= 276480 ? "SUFFICIENT" : "SHORT -- this needs a UNITS sweep");

    // THE SHARING, stated as a count rather than as a claim in a comment: u and
    // v divide by the same depth, so 500 fragments must have cost 500
    // reciprocals, not 1000.
    printf("   MEASURED: %u fragments cost %u reciprocals\n", (unsigned)top.fragments_o,
           (unsigned)top.rcp_recips_o);
    zhao::check(top.rcp_recips_o == top.fragments_o,
                "one reciprocal per fragment, shared between u and v", (uint32_t)top.fragments_o,
                (uint32_t)top.rcp_recips_o);
  }

  // ------------------------------------------------------------------ 5 ---
  printf("== section 5: the shift really did move ==\n");
  {
    // ANTI-VACUITY, and the most important check in the file. The shift is
    // 32 - k and k is data dependent; a block that hard-coded one shift passes
    // every case at that one depth. Sections 1 and 2 only rule that out if the
    // cases spanned the exponent range.
    printf("   MEASURED: %zu distinct reciprocal exponents across the cases\n", exponents.size());
    zhao::check(exponents.size() >= 12,
                "the cases span a wide range of exponents, so a fixed shift would fail", 12,
                (uint32_t)exponents.size());

    // A zero depth is a caller bug: early-Z should never pass a fragment at
    // infinity. Reported, not divided, and the block still works after.
    reset(top);
    const Res z = one(top, 12345, 6789, 0, 0x55);
    zhao::check(z.clocks > 0 && z.zero, "a zero depth is reported rather than divided", 1,
                (z.clocks > 0 && z.zero) ? 1 : 0);
    const Res after = one(top, 1 << 20, 1 << 20, 1u << 23, 0x56);
    zhao::check(!after.zero && after.u == law(1 << 20, 1u << 23),
                "and the block is usable again straight after", 1,
                (!after.zero && after.u == law(1 << 20, 1u << 23)) ? 1 : 0);
  }

  return zhao::report_and_exit("raster_perspuv_directed");
}
