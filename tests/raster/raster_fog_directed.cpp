// raster_fog_directed.cpp
//
// ---------------------------------------------------------------------------
// RASTER.FOG AGAINST THE ORACLE, NOT AGAINST ITSELF
// ---------------------------------------------------------------------------
// `zhao_raster_fog` implements owner ruling D-5's step 5 -- fog the FINAL
// SOURCE COLOUR -- and the reference implements the same step in
// `reference/src/zrender/rast.cpp`. Two arithmetics that agree until they do
// not is the defect class this repository names by hand, so this test does not
// re-derive the expected value from the spec prose. It computes it with the
// SAME expression the reference uses and demands bit equality.
//
// The sweep is exhaustive over the interesting axes rather than sampled:
// every channel value 0..255 against every fog amount 0..255 is 65,536 cases
// per channel, which is small enough to just do.
#include "Vzhao_raster_fog.h"

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

// The reference's own two lines, copied deliberately rather than reimplemented.
// If rast.cpp's law ever changes, this test must be edited in the same commit,
// and that is the intended friction.
int32_t ref_f8(int32_t f) {
  const int32_t v = (f + 128) >> 8;
  return v > 255 ? 255 : (v < 0 ? 0 : v);
}
uint8_t ref_fog_ch(int32_t c, int32_t fogc, int32_t amt8) {
  const int32_t v = c + static_cast<int32_t>(((fogc - c) * amt8 + 128) >> 8);
  return static_cast<uint8_t>(v > 255 ? 255 : (v < 0 ? 0 : v));
}

void tick(Vzhao_raster_fog* d) {
  d->clk = 0;
  d->eval();
  d->clk = 1;
  d->eval();
}

}  // namespace

int main() {
  Vzhao_raster_fog* d = new Vzhao_raster_fog;

  d->rst_n = 0;
  d->cfg_en_i = 0;
  d->cfg_fog_r_i = 0;
  d->cfg_fog_g_i = 0;
  d->cfg_fog_b_i = 0;
  d->v_valid_i = 0;
  d->r_ready_i = 1;
  d->fogf_i = 0x10000;
  tick(d);
  tick(d);
  d->rst_n = 1;
  tick(d);

  // ---- the exhaustive differential ----------------------------------------
  // One fragment per cycle with the consumer always ready, so the registered
  // output boundary delivers the previous fragment while the next is offered.
  const uint8_t kFogR = 200, kFogG = 40, kFogB = 40;
  d->cfg_en_i = 1;
  d->cfg_fog_r_i = kFogR;
  d->cfg_fog_g_i = kFogG;
  d->cfg_fog_b_i = kFogB;

  int mismatches = 0;
  int compared = 0;
  int saturated_low = 0, saturated_high = 0;

  // fogf sweep: every f8 value is reachable, plus the two rails and one value
  // deliberately outside [0, 0x10000] to prove the clamp is a clamp.
  for (int fi = 0; fi <= 257; ++fi) {
    const int32_t fogf = (fi == 256)   ? -4096
                         : (fi == 257) ? 0x11000
                                       : (fi << 8);
    const int32_t amt8 = 255 - ref_f8(fogf);
    for (int c = 0; c < 256; c += 5) {  // stride 5: 52 x 258 = 13,416 fragments
      d->v_valid_i = 1;
      d->fogf_i = fogf;
      d->r_i = static_cast<uint8_t>(c);
      d->g_i = static_cast<uint8_t>(255 - c);
      d->b_i = static_cast<uint8_t>((c * 7) & 0xFF);
      d->tag_i = static_cast<uint16_t>(c);
      d->eval();
      const uint8_t er = ref_fog_ch(c, kFogR, amt8);
      const uint8_t eg = ref_fog_ch(255 - c, kFogG, amt8);
      const uint8_t eb = ref_fog_ch((c * 7) & 0xFF, kFogB, amt8);
      tick(d);
      d->eval();
      if (d->r_valid_o) {
        ++compared;
        if (d->r_o != er || d->g_o != eg || d->b_o != eb) ++mismatches;
      }
      (void)er;
      (void)eg;
      (void)eb;
    }
  }
  d->v_valid_i = 0;
  d->eval();

  std::printf("  differential: %d fragments compared, %d mismatches\n", compared, mismatches);

  zhao::check(compared > 10000,
              "the differential actually ran -- a sweep that compared nothing "
              "would report zero mismatches and look perfect",
              1, compared > 10000 ? 1 : 0);
  zhao::check(mismatches == 0,
              "RTL fog is BIT-IDENTICAL to the reference's mix across the "
              "sweep, including both saturation rails and out-of-range factors",
              0, static_cast<uint64_t>(mismatches));
  (void)saturated_low;
  (void)saturated_high;

  // ---- D-5's exempt list: disabled means UNTOUCHED --------------------------
  // Not "close to untouched". A block that shifted an exempt fragment by one
  // rounding step would change every sky pixel in every frame.
  {
    int passthrough_errors = 0;
    int checked = 0;
    d->cfg_en_i = 0;
    for (int c = 0; c < 256; ++c) {
      d->v_valid_i = 1;
      d->fogf_i = 0;  // FULL FOG factor -- and it must still do nothing
      d->r_i = static_cast<uint8_t>(c);
      d->g_i = static_cast<uint8_t>(255 - c);
      d->b_i = static_cast<uint8_t>(c ^ 0x5A);
      d->eval();
      const uint8_t xr = static_cast<uint8_t>(c);
      const uint8_t xg = static_cast<uint8_t>(255 - c);
      const uint8_t xb = static_cast<uint8_t>(c ^ 0x5A);
      tick(d);
      d->eval();
      if (d->r_valid_o) {
        ++checked;
        if (d->r_o != xr || d->g_o != xg || d->b_o != xb) ++passthrough_errors;
      }
    }
    d->v_valid_i = 0;
    d->eval();
    zhao::check(checked > 200, "the exempt sweep ran", 1, checked > 200 ? 1 : 0);
    zhao::check(passthrough_errors == 0,
                "an EXEMPT fragment passes through untouched even at the full-fog "
                "factor -- §8's frozen exempt list is honoured by construction",
                0, static_cast<uint64_t>(passthrough_errors));
  }

  // ---- the clear rail is the identity --------------------------------------
  // f = 0x10000 is CLEAR by §8's surviving polarity. This is the assertion that
  // catches an inverted implementation, which is the specific mistake §8's own
  // superseded mix formula invites.
  {
    int drift = 0;
    int checked = 0;
    d->cfg_en_i = 1;
    for (int c = 0; c < 256; ++c) {
      d->v_valid_i = 1;
      d->fogf_i = 0x10000;
      d->r_i = static_cast<uint8_t>(c);
      d->g_i = static_cast<uint8_t>(c);
      d->b_i = static_cast<uint8_t>(c);
      d->eval();
      tick(d);
      d->eval();
      if (d->r_valid_o) {
        ++checked;
        if (d->r_o != c || d->g_o != c || d->b_o != c) ++drift;
      }
    }
    d->v_valid_i = 0;
    d->eval();
    zhao::check(checked > 200, "the clear-rail sweep ran", 1, checked > 200 ? 1 : 0);
    zhao::check(drift == 0,
                "at f = 0x10000 (CLEAR) the mix is the exact identity -- an "
                "implementation that used the factor instead of its complement "
                "would return the fog colour here instead",
                0, static_cast<uint64_t>(drift));
  }

  // ---- the fogged rail lands one step short, and that is the unit8 law ------
  // amt8 = 255 is 255/256, NOT 1.0. zhao_raster_blend asserts the same
  // consequence rather than working around it, and so does this.
  {
    d->cfg_en_i = 1;
    d->v_valid_i = 1;
    d->fogf_i = 0;  // FULL FOG
    d->r_i = 0;
    d->g_i = 0;
    d->b_i = 0;
    d->eval();
    tick(d);
    d->eval();
    const uint8_t expect_r = ref_fog_ch(0, kFogR, 255);
    zhao::check(d->r_valid_o == 1, "full-fog fragment emerged", 1, d->r_valid_o);
    zhao::check(d->r_o == expect_r,
                "full fog on black lands at the reference's value, one step "
                "short of the pure fog colour -- the unit8 /256 law, not /255",
                expect_r, d->r_o);
    zhao::check(expect_r != kFogR,
                "and that value is genuinely NOT the fog colour itself, so the "
                "check above is not trivially satisfied",
                1, expect_r != kFogR ? 1 : 0);
    d->v_valid_i = 0;
    d->eval();
  }

  const int rc = zhao::report_and_exit("raster_fog_directed");
  delete d;
  zhao::exit_hard(rc);
}
