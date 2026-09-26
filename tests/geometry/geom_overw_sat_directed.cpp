// geom_overw_sat_directed.cpp
//
// `zhao_geom_overw_sat`'s acceptance suite: the S8.24 bound that
// `spec/qformats.md:75` has mandated all along, built and demonstrated
// saturating -- with a negative control that proves the block it does NOT
// correct.
//
// ---------------------------------------------------------------------------
// THE CONTROL STRUCTURE, AND WHY IT IS IN THIS ORDER
// ---------------------------------------------------------------------------
// The risk this suite exists to kill is not "the multiply is wrong". It is
// that a WIDE restatement of ratified arithmetic quietly becomes a RIVAL law
// -- the failure `design/contracts/GEOM.LIGHT.md` names and TERRAIN.SHADE's
// header records shipping once. So:
//
//   1. THE WIDE ORACLE IS AN EXTENSION, not a rival. `zref::geom_over_w_wide`
//      is differenced against the ratified `zref::geom_over_w` over ALL 65,536
//      s16 coordinates x a sweep of `invw24`. No RTL in the room. If this
//      fails, the wide form is a second law and nothing below it is quoted.
//
//   2. THE NEGATIVE CONTROL. The RTL over that same entire s16 domain: exact
//      against the oracle AND `sat_o` LOW on every single one. This is the
//      measured form of `zhao_geom_vattr.sv:58-60`'s claim -- "|u| < 2^15 and
//      invw24 < 2^24 bound the product below 2^39 ... NO SATURATION CASE
//      EXISTS". That sentence is now proven rather than quoted, and it is the
//      reason this block does not touch GEOM.VATTR.
//
//   3. THE POSITIVE CONTROL. Terrain's coordinate is `signed [31:0]`
//      (`zhao_terrain_uvlane`), so the product reaches 2^55 and the rounded
//      result reaches 2^39 -- seven bits outside the s32 the type is stored
//      in. The RTL must saturate, `sat_o` must rise, and the value must be the
//      rail.
//
//   4. THE EDGE IS FOUND, NOT ASSERTED. The exact coordinate at which the
//      saturate engages is located by binary search on the RTL and then
//      checked against the arithmetic, in both signs. An off-by-one here is
//      the whole defect.
//
//   5. WHAT THE MISSING SATURATE COSTS. The correct (saturated) answer is
//      asserted; the WRAPPED answer a 32-bit truncation would give is computed
//      beside it and asserted to be DIFFERENT and of the WRONG SIGN. This is
//      evidence that the saturate does work -- it is not a test that asserts
//      the bug, because after the repair the assertion is still about correct
//      behaviour (the rail) and the wrap is only the quantity being ruled out.
//
//   6. ROUNDING. Round-half-up at exact ties, on both signs, per qformats 4.
//
//   7. `sat_o` IS A DECLARATION, NOT A DIFFERENCE. A coordinate whose exact
//      result lands ON a rail is not a saturation and must not be reported as
//      one.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected value; the suite must
// then FAIL. Registered as the `geom_overw_sat_break_oracle` ctest.
#include "Vzhao_geom_overw_sat.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"
#include "zref/zref_depth.hpp"

#include "../harness/zhao_sim.hpp"

double sc_time_stamp() { return 0.0; }

namespace {

bool g_break_oracle = false;
long g_checks = 0;
int g_fails = 0;

void fail(const char* what, long long a, long long b) {
  ++g_fails;
  std::printf("FAIL %s: got %lld expected %lld\n", what, a, b);
  if (g_fails > 20) {
    std::printf("OVERW_SAT: too many failures, stopping\n");
    zhao::exit_hard(1);
  }
}

void check_eq(const char* what, long long got, long long want) {
  ++g_checks;
  if (got != want) fail(what, got, want);
}

void check_ne(const char* what, long long got, long long unwanted) {
  ++g_checks;
  if (got == unwanted) {
    ++g_fails;
    std::printf("FAIL %s: got %lld, which must NOT equal %lld\n", what, got, unwanted);
    if (g_fails > 20) zhao::exit_hard(1);
  }
}

// THE WRAP: what a 32-bit truncation with no saturate would have produced.
// Not a law, and never asserted as correct -- it is the quantity section 5
// rules OUT.
int32_t wrapped(int32_t uv, uint32_t invw24) {
  const int64_t p = static_cast<int64_t>(uv) * static_cast<int64_t>(invw24 & 0xFFFFFFu);
  const int64_t r = (p + (int64_t{1} << 15)) >> 16;
  return static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(r) & 0xFFFFFFFFull));
}

struct Dut {
  Vzhao_geom_overw_sat* d;
  int32_t over_w = 0;
  bool sat = false;

  void eval(int32_t uv, uint32_t invw24) {
    d->uv_i = static_cast<uint32_t>(uv);
    d->invw24_i = invw24 & 0xFFFFFFu;
    d->eval();
    over_w = static_cast<int32_t>(d->over_w_o);
    sat = d->sat_o != 0;
  }
};

uint32_t g_lfsr = 0x1B0CCA7Eu;
uint32_t rnd() {
  g_lfsr ^= g_lfsr << 13;
  g_lfsr ^= g_lfsr >> 17;
  g_lfsr ^= g_lfsr << 5;
  return g_lfsr;
}

// A sweep of invw24 values: both rails, the pinned m = 2^23 of qformats 8,
// and a spread between.
const uint32_t kInvw[] = {0u,        1u,        2u,        0x000100u, 0x001000u, 0x010000u,
                          0x400000u, 0x7FFFFFu, 0x800000u, 0x800001u, 0xAAAAAAu, 0xC00000u,
                          0xF00000u, 0xFFFFFEu, 0xFFFFFFu};
const int kNInvw = static_cast<int>(sizeof(kInvw) / sizeof(kInvw[0]));

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--break-oracle") == 0) g_break_oracle = true;

  // =========================================================================
  // 1. THE WIDE ORACLE IS AN EXTENSION OF THE RATIFIED ONE. NO RTL.
  // =========================================================================
  {
    long n = 0;
    for (int32_t uv = -32768; uv <= 32767; ++uv) {
      for (int k = 0; k < kNInvw; ++k) {
        bool sat = true;  // seeded WRONG so a function that never writes it fails
        const int32_t wide = zref::geom_over_w_wide(uv, kInvw[k], &sat);
        const int32_t narrow = zref::geom_over_w(static_cast<int16_t>(uv), kInvw[k]);
        check_eq("wide == narrow on the s16 domain", wide, narrow);
        check_eq("wide never saturates on the s16 domain", sat ? 1 : 0, 0);
        ++n;
      }
    }
    std::printf("1. geom_over_w_wide == geom_over_w over %ld s16 x invw24 pairs, 0 saturations\n",
                n);
  }

  Vzhao_geom_overw_sat model;
  Dut dut{&model};

  // =========================================================================
  // 2. THE NEGATIVE CONTROL -- GEOM.VATTR's "no saturation case exists",
  //    measured over its entire coordinate domain rather than quoted.
  // =========================================================================
  {
    long n = 0;
    int sats = 0;
    for (int32_t uv = -32768; uv <= 32767; ++uv) {
      for (int k = 0; k < kNInvw; ++k) {
        dut.eval(uv, kInvw[k]);
        int32_t want = zref::geom_over_w(static_cast<int16_t>(uv), kInvw[k]);
        if (g_break_oracle && n == 9) want += 1;
        check_eq("RTL == geom_over_w (s16 domain)", dut.over_w, want);
        if (dut.sat) ++sats;
        ++n;
      }
    }
    check_eq("sat_o is LOW on every s16 coordinate", sats, 0);
    std::printf("2. negative control: %ld s16 pairs exact, %d saturations\n", n, sats);
  }

  // =========================================================================
  // 3. THE POSITIVE CONTROL -- terrain's s32 coordinate saturates
  // =========================================================================
  {
    int hi = 0, lo = 0;
    long n = 0;
    const int32_t wide_uvs[] = {INT32_MAX,   INT32_MIN,   1 << 30,      -(1 << 30),
                                1 << 24,     -(1 << 24),  100 << 16,    -(100 << 16),
                                128 << 16,   -(128 << 16), 4096 << 16,  -(4096 << 16)};
    for (int32_t uv : wide_uvs)
      for (int k = 0; k < kNInvw; ++k) {
        dut.eval(uv, kInvw[k]);
        bool want_sat = false;
        const int32_t want = zref::geom_over_w_wide(uv, kInvw[k], &want_sat);
        check_eq("RTL == geom_over_w_wide (wide domain)", dut.over_w, want);
        check_eq("RTL sat_o == oracle saturation", dut.sat ? 1 : 0, want_sat ? 1 : 0);
        if (dut.sat && dut.over_w > 0) ++hi;
        if (dut.sat && dut.over_w < 0) ++lo;
        ++n;
      }
    ++g_checks;
    if (hi == 0 || lo == 0) {
      ++g_fails;
      std::printf("FAIL the saturate did not fire in BOTH directions: hi=%d lo=%d\n", hi, lo);
    }
    std::printf("3. positive control: %ld wide pairs, %d saturated high, %d low\n", n, hi, lo);

    // And a randomised wide sweep, so the fixed list is not the whole evidence.
    long rn = 0;
    int rsat = 0;
    for (int i = 0; i < 200000; ++i) {
      const int32_t uv = static_cast<int32_t>(rnd());
      const uint32_t iw = rnd() & 0xFFFFFFu;
      dut.eval(uv, iw);
      bool want_sat = false;
      const int32_t want = zref::geom_over_w_wide(uv, iw, &want_sat);
      check_eq("RTL == geom_over_w_wide (random wide)", dut.over_w, want);
      check_eq("RTL sat_o == oracle (random wide)", dut.sat ? 1 : 0, want_sat ? 1 : 0);
      if (dut.sat) ++rsat;
      ++rn;
    }
    std::printf("   random wide sweep: %ld pairs, %d saturated\n", rn, rsat);
  }

  // =========================================================================
  // 4. THE EDGE IS FOUND, NOT ASSERTED
  // =========================================================================
  // At a pinned invw24, binary-search the RTL for the first coordinate whose
  // saturate engages, then check that value against the arithmetic. The bound
  // is |value| < 128 in S8.24, i.e. |uv * invw24| >= 2^47 after rounding.
  {
    const uint32_t iw = 0x800000u;  // 0.5 in U0.24, qformats 8's pinned m
    // Positive edge.
    int64_t lo_uv = 0, hi_uv = INT32_MAX;
    dut.eval(static_cast<int32_t>(hi_uv), iw);
    check_eq("the search bracket saturates at its top", dut.sat ? 1 : 0, 1);
    dut.eval(0, iw);
    check_eq("and not at its bottom", dut.sat ? 1 : 0, 0);
    while (lo_uv + 1 < hi_uv) {
      const int64_t mid = (lo_uv + hi_uv) / 2;
      dut.eval(static_cast<int32_t>(mid), iw);
      if (dut.sat) {
        hi_uv = mid;
      } else {
        lo_uv = mid;
      }
    }
    // hi_uv is the first saturating coordinate. Check it against the law.
    bool s_at = false, s_below = false;
    (void)zref::geom_over_w_wide(static_cast<int32_t>(hi_uv), iw, &s_at);
    (void)zref::geom_over_w_wide(static_cast<int32_t>(lo_uv), iw, &s_below);
    check_eq("RTL's first saturating coordinate saturates in the oracle too", s_at ? 1 : 0, 1);
    check_eq("and the one below it does not", s_below ? 1 : 0, 0);
    // At invw24 = 2^23 the rounded raw is uv * 2^7, so the first saturating
    // coordinate is the smallest uv with uv * 128 > 2^31 - 1, i.e. 2^24. The
    // search is what establishes it; this line records what it found so a
    // future change that moves the edge is visible in the diff.
    check_eq("the edge is at 2^24 for invw24 = 2^23", hi_uv, 16777216);
    dut.eval(static_cast<int32_t>(hi_uv), iw);
    check_eq("RTL rails high at the edge", dut.over_w, INT32_MAX);
    dut.eval(static_cast<int32_t>(lo_uv), iw);
    check_eq("RTL is exact one LSB below it", dut.over_w,
             zref::geom_over_w_wide(static_cast<int32_t>(lo_uv), iw, nullptr));
    std::printf("4. edge found by search at uv = %lld (invw24 = 0x800000)\n",
                static_cast<long long>(hi_uv));

    // The negative edge, the same way: nlo saturates, nhi does not.
    int64_t nlo = INT32_MIN, nhi = 0;
    dut.eval(static_cast<int32_t>(nlo), iw);
    check_eq("the negative bracket saturates at its bottom", dut.sat ? 1 : 0, 1);
    while (nlo + 1 < nhi) {
      const int64_t mid = (nlo + nhi) / 2;  // truncation toward zero stays inside
      dut.eval(static_cast<int32_t>(mid), iw);
      if (dut.sat) {
        nlo = mid;
      } else {
        nhi = mid;
      }
    }
    dut.eval(static_cast<int32_t>(nlo), iw);
    check_eq("RTL rails low at the negative edge", dut.over_w, INT32_MIN);
    check_eq("and declares it", dut.sat ? 1 : 0, 1);
    dut.eval(static_cast<int32_t>(nhi), iw);
    check_eq("and is exact one LSB above it", dut.sat ? 1 : 0, 0);
    std::printf("   negative edge at uv = %lld\n", static_cast<long long>(nlo));
  }

  // =========================================================================
  // 5. WHAT THE MISSING SATURATE COSTS -- the correct answer is the assertion
  // =========================================================================
  // A terrain patch 300 tiles from the origin at w = 2, so
  // value(u_over_w) = 300 * 0.5 = 150 -- past the S8.24 bound of 128 and not
  // by much, which is the point: this is a plausible island, not a contrived
  // rail. The ASSERTION is that the block rails correctly; the wrap is
  // computed only to be ruled out, and the SIGN FLIP is what makes the silent
  // failure so expensive -- the patch would sample from the far side of the
  // atlas with every gate in the repository passing.
  {
    const int32_t uv = 300 << 16;       // 300 tiles out, Q16.16 tile units
    const uint32_t iw = 0x800000u;      // invw24 = 2^23, i.e. w = 2
    dut.eval(uv, iw);
    bool want_sat = false;
    const int32_t want = zref::geom_over_w_wide(uv, iw, &want_sat);
    check_eq("far patch: the block RAILS, which is the law", dut.over_w, want);
    check_eq("far patch: and says so", dut.sat ? 1 : 0, 1);
    check_eq("far patch: the rail is INT32_MAX", dut.over_w, INT32_MAX);
    const int32_t w = wrapped(uv, iw);
    check_ne("far patch: the rail is NOT what a 32-bit truncation gives", dut.over_w, w);
    ++g_checks;
    if (!(w < 0)) {
      ++g_fails;
      std::printf("FAIL the chosen far-patch case does not demonstrate a sign flip; "
                  "pick a coordinate past the bound\n");
    }
    std::printf("5. far patch: block rails to %d; a truncation would have given %d\n", dut.over_w,
                w);
  }

  // =========================================================================
  // 6. ROUNDING -- round-half-up at exact ties, both signs (qformats 4)
  // =========================================================================
  {
    // p + 2^15 exactly a multiple of 2^16 means p is an exact half-LSB.
    // invw24 = 2^16 makes p = uv * 2^16, so uv odd ... pick invw24 = 2^15,
    // p = uv * 2^15, and uv odd gives p = (odd)*2^15, a half of 2^16.
    const uint32_t iw = 0x008000u;  // 2^15
    for (int32_t uv : {int32_t{1}, int32_t{3}, int32_t{-1}, int32_t{-3}, int32_t{12345},
                       int32_t{-12345}}) {
      dut.eval(uv, iw);
      const int64_t p = static_cast<int64_t>(uv) * static_cast<int64_t>(iw);
      const int64_t want = (p + 32768) >> 16;  // arithmetic shift: FLOOR, ties up
      check_eq("round-half-up at an exact tie", dut.over_w, want);
    }
    // Ties round toward +infinity, so -1 * 2^15 = -2^15 rounds to 0, not -1.
    dut.eval(-1, iw);
    check_eq("a negative exact tie rounds toward +infinity", dut.over_w, 0);
    dut.eval(1, iw);
    check_eq("and a positive one to 1", dut.over_w, 1);
    std::printf("6. ties round half-up on both signs\n");
  }

  // =========================================================================
  // 7. `sat_o` IS A DECLARATION, NOT A DIFFERENCE
  // =========================================================================
  // Land the exact result ON the positive rail and confirm the block does NOT
  // call it a saturation. invw24 = 2^16 gives r = uv exactly, so uv =
  // INT32_MAX lands on the rail by arithmetic and not by clamping.
  {
    dut.eval(INT32_MAX, 0x010000u);
    check_eq("an exact result ON the rail is still exact", dut.over_w, INT32_MAX);
    check_eq("and is NOT reported as a saturation", dut.sat ? 1 : 0, 0);
    dut.eval(INT32_MIN, 0x010000u);
    check_eq("the same on the low rail", dut.over_w, INT32_MIN);
    check_eq("and not reported", dut.sat ? 1 : 0, 0);
    std::printf("7. sat_o separates 'saturated' from 'landed on the rail'\n");
  }

  std::printf("OVERW_SAT: %ld checks, %d failures\n", g_checks, g_fails);
  if (g_fails != 0) {
    std::printf("OVERW_SAT: FAIL\n");
    zhao::exit_hard(1);
  }
  std::printf("OVERW_SAT: PASS\n");
  zhao::exit_hard(0);
}
