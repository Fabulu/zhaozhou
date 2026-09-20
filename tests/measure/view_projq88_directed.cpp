// view_projq88_directed.cpp -- the per-camera PROJECTION SCALE in Q8.8,
// derived under owner ruling R73 by `zhao_view_projq88`.
//
// WHAT THIS LANE WOULD CATCH:
//
//   1. THE RULING'S FORMULA, AGAINST THE CONTRACT'S OWN WORKED EXAMPLE.
//      `design/contracts/MEASURE.GOVERNOR.md` line 75 works the shipped Duo
//      canvas by hand: "VIDEO_DUO is two 256x192 canvases. At 60 degrees of
//      horizontal FOV the projection scale is (256/2)/tan 30 = 221.70 px per
//      unit tangent = Q8.8 raw 56755." R73 says the same number is
//      `rhu(kx_raw * viewport_w / 512)`. This case computes kx from the FOV,
//      feeds it, and checks the block lands on that value. Two independent
//      statements of one quantity agreeing is the point -- if the derivation
//      were wrong in the factor of 2, or in the fx16 scaling, it would miss by
//      a factor, not by an LSB.
//   2. ONE ROUNDING, AND IT IS ROUND-HALF-UP. A product whose remainder mod
//      512 is exactly 256 is the one input where round-half-up and truncation
//      differ, and it is constructed rather than hoped for.
//   3. SATURATION FIRES AND IS COUNTED. The Q8.8 port tops out at 255.996 px
//      per unit tangent, and the console's own single-view 512-pixel raster
//      exceeds it for any horizontal FOV narrower than 90 degrees. That is a
//      REAL operating point, not a synthetic one -- see FINDINGS-post3.md,
//      which raises the port width as a decision. Here it is simply asserted
//      that the block clamps once, does not wrap, and counts it.
//   4. THE TWO VIEWS ARE INDEPENDENT. View 1's camera must not move view 0's
//      output, which is the governor's law G3 one block upstream.
//   5. THE OPERANDS ARE LATCHED PER PASS. Changing the inputs while a pass is
//      in flight must never produce one camera's kx multiplied by another's
//      viewport width -- a value that is wrong and plausible at the same time.
#include "Vzhao_view_projq88.h"

#include <cmath>
#include <cstdio>

#include "zhao_sim.hpp"

namespace {

using zhao::check;

/** The law, restated here rather than imported from the RTL (R73). */
uint32_t want_proj(uint32_t kx_raw, uint32_t vw) {
  const uint64_t n = static_cast<uint64_t>(kx_raw) * vw + 256u;
  const uint64_t q = n >> 9;
  return q > 0xFFFFu ? 0xFFFFu : static_cast<uint32_t>(q);
}

/** fx16 (Q16.16) from a double. */
uint32_t fx16(double v) { return static_cast<uint32_t>(std::llround(v * 65536.0)); }

void reset_dut(Vzhao_view_projq88& dut) {
  dut.rst_n = 0;
  dut.clk = 0;
  dut.kx0_i = 0;
  dut.kx1_i = 0;
  dut.vw0_i = 0;
  dut.vw1_i = 0;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/** A pass is 14 clocks per view; 80 covers both views twice over. */
void settle(Vzhao_view_projq88& dut, int cycles = 80) {
  for (int i = 0; i < cycles; ++i) zhao::tick(dut);
}

void drive(Vzhao_view_projq88& dut, uint32_t kx0, uint32_t vw0, uint32_t kx1, uint32_t vw1) {
  dut.kx0_i = kx0;
  dut.vw0_i = static_cast<uint16_t>(vw0);
  dut.kx1_i = kx1;
  dut.vw1_i = static_cast<uint16_t>(vw1);
  dut.eval();
  settle(dut);
}

}  // namespace

int main() {
  Vzhao_view_projq88 dut;
  reset_dut(dut);

  // ---- case 1: the contract's shipped Duo camera -------------------------
  {
    const double kx = 1.0 / std::tan(30.0 * 3.14159265358979323846 / 180.0);
    const uint32_t kx_raw = fx16(kx);
    const uint32_t vw = 256;
    drive(dut, kx_raw, vw, kx_raw, vw);

    const uint32_t want = want_proj(kx_raw, vw);
    check(dut.proj0_o == want, "Duo 60deg: view 0 matches R73", want, dut.proj0_o);
    check(dut.proj1_o == want, "Duo 60deg: view 1 matches R73", want, dut.proj1_o);

    // The contract's hand-worked 56755 is the same number to within the LSB
    // its own text truncated. A mismatch by a FACTOR would be a wrong law; a
    // mismatch by one is a rounding convention, and R73 names round-half-up.
    const int32_t delta = static_cast<int32_t>(dut.proj0_o) - 56755;
    check(delta >= 0 && delta <= 1, "Duo 60deg: within 1 LSB of the contract's 56755",
          56755, dut.proj0_o);
    // And it is the physical quantity: 221.70 px per unit tangent.
    const double px = static_cast<double>(dut.proj0_o) / 256.0;
    check(px > 221.6 && px < 221.8, "Duo 60deg: reads as 221.7 px", 221,
          static_cast<uint64_t>(px));
  }

  // ---- case 2: the round-half-up tie -------------------------------------
  {
    // Choose kx_raw * vw with remainder exactly 256 mod 512. vw = 1 makes the
    // product kx_raw itself, so any kx_raw = k*512 + 256 is a tie.
    const uint32_t kx_raw = 512u * 1000u + 256u;
    drive(dut, kx_raw, 1, 0, 0);
    const uint32_t want = want_proj(kx_raw, 1);  // (n + 256) >> 9 rounds UP
    check(dut.proj0_o == want, "tie rounds half UP", want, dut.proj0_o);
    check(want == 1001, "the tie case is the one that distinguishes truncation", 1001, want);
  }

  // ---- case 3: saturation, at a real operating point ---------------------
  {
    const uint32_t sat_before = dut.saturations_o;
    // Single-view 512-pixel raster at 60 degrees horizontal FOV: proj = 443.4,
    // which the Q8.8 port cannot carry.
    const double kx = 1.0 / std::tan(30.0 * 3.14159265358979323846 / 180.0);
    const uint32_t kx_raw = fx16(kx);
    drive(dut, kx_raw, 512, 0, 0);
    check(dut.proj0_o == 0xFFFF, "512-wide 60deg clamps to the Q8.8 ceiling", 0xFFFF,
          dut.proj0_o);
    check(dut.saturations_o > sat_before, "saturations_o fired", sat_before + 1,
          dut.saturations_o);
    // It CLAMPED, it did not wrap. A wrap would have produced a small, entirely
    // plausible number and pegged every camera at the fine end of the ladder.
    check(want_proj(kx_raw, 512) == 0xFFFF, "the oracle agrees it saturates", 0xFFFF,
          want_proj(kx_raw, 512));
  }

  // ---- case 4: the two views are independent -----------------------------
  {
    const uint32_t kx0 = fx16(1.0);
    const uint32_t kx1 = fx16(0.25);
    drive(dut, kx0, 256, kx1, 256);
    check(dut.proj0_o == want_proj(kx0, 256), "view 0 has its own camera",
          want_proj(kx0, 256), dut.proj0_o);
    check(dut.proj1_o == want_proj(kx1, 256), "view 1 has its own camera",
          want_proj(kx1, 256), dut.proj1_o);
    check(dut.proj0_o != dut.proj1_o, "and they differ", 1,
          dut.proj0_o != dut.proj1_o ? 1 : 0);

    // Move view 1 only; view 0 must not budge by one LSB.
    const uint32_t before0 = dut.proj0_o;
    dut.kx1_i = fx16(0.5);
    dut.eval();
    settle(dut);
    check(dut.proj0_o == before0, "moving view 1 leaves view 0 alone", before0, dut.proj0_o);
    check(dut.proj1_o == want_proj(fx16(0.5), 256), "view 1 followed",
          want_proj(fx16(0.5), 256), dut.proj1_o);
  }

  // ---- case 5: zero and the low limits -----------------------------------
  {
    drive(dut, 0, 256, fx16(2.0), 0);
    check(dut.proj0_o == 0, "kx = 0 gives zero", 0, dut.proj0_o);
    check(dut.proj1_o == 0, "viewport width 0 gives zero", 0, dut.proj1_o);
  }

  // ---- case 6: the operands are latched per pass -------------------------
  {
    // Drive a settled pair, then change kx0 and vw0 together on every cycle
    // for a while. Whatever the block publishes must be SOME consistent pair,
    // never a cross of one camera's kx with another's width. The two candidate
    // pairs are chosen so that a crossed product is a different number from
    // either honest one.
    const uint32_t kxA = fx16(1.0), vwA = 256;   // -> 32768
    const uint32_t kxB = fx16(0.5), vwB = 128;   // -> 8192
    drive(dut, kxA, vwA, 0, 0);
    bool saw_bad = false;
    for (int i = 0; i < 400; ++i) {
      const bool a = (i % 2) == 0;
      dut.kx0_i = a ? kxA : kxB;
      dut.vw0_i = static_cast<uint16_t>(a ? vwA : vwB);
      dut.eval();
      zhao::tick(dut);
      const uint32_t p = dut.proj0_o;
      const bool ok = (p == want_proj(kxA, vwA)) || (p == want_proj(kxB, vwB));
      if (!ok) saw_bad = true;
    }
    check(!saw_bad, "every published value is one honest (kx, vw) pair", 0, saw_bad ? 1 : 0);
  }

  // ---- case 7: the pass counters fire for both views ---------------------
  check(dut.passes0_o > 0, "passes0_o fired", 1, dut.passes0_o);
  check(dut.passes1_o > 0, "passes1_o fired", 1, dut.passes1_o);

  zhao::exit_hard(zhao::report_and_exit("view_projq88_directed"));
}
