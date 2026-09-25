// sheetmod_directed.cpp
//
// TEXTURE.SHEETMOD's acceptance suite: `zhao_texture_sheetmod` against the two
// statements of charter section 12's sheet tint law that
// `reference/src/zrender/terrain.cpp` carries.
//
// ---------------------------------------------------------------------------
// TWO ORACLES, AND THEY MUST AGREE WITH EACH OTHER BEFORE EITHER IS QUOTED
// ---------------------------------------------------------------------------
// `zref` does NOT export this law -- it lives inline in `draw_terrain`, in two
// places that were written for two different profiles. So this suite carries
// BOTH, transcribed separately and never from each other, exactly as
// `terrain_uvlane_directed.cpp` does for the frozen UV law:
//
//   ORACLE A, the TEXTURED profile's Q16.16 form (terrain.cpp:638-640 and the
//   `mod_of` rounding at :633-636):
//       factor = (sheet == nullptr) ? 65536 : ((255 - (strength >> 1)) << 8)
//       out    = rescale_s(rgb * factor, 16) = (rgb * factor + 32768) >> 16
//
//   ORACLE B, the UNTEXTURED profile's unit8 form (terrain.cpp:718 and :756):
//       tint = 255 - (strength >> 1)
//       out  = (rgb * tint + 128) >> 8
//
// Section 1 proves A == B over all 256 x 256 operand pairs BEFORE any RTL is
// touched. That is what makes the RTL comparison in section 2 evidence about
// the RTL rather than a transcription agreeing with itself: if the algebra in
// the block's header ("the Q16.16 form and the unit8 form agree bit for bit")
// were wrong, section 1 fails and nothing downstream is quoted.
//
// What it pins, in order:
//   1. THE TWO ORACLES AGREE  -- exhaustive, 65,536 pairs, no RTL involved.
//   2. EXHAUSTIVE RTL         -- all 256 strengths x all 256 channel values,
//                                all three channels independently, against
//                                oracle A. 196,608 checks.
//   3. THE IDENTITY ARM       -- en_i low passes RGB through BIT FOR BIT for
//                                1,024 random words INCLUDING strengths that
//                                would have darkened it, and `applied_o` is
//                                low. This is the 0.4% trap the oracle names:
//                                255/256 is not 1, so "strength 0" is NOT the
//                                identity and only the enable is.
//   4. THE 0.4% DARKENING IS REAL -- with en_i HIGH and strength 0, every
//                                channel value >= 129 comes back SMALLER.
//                                Asserted as the correct behaviour, because
//                                the oracle's own comment says a sheet that
//                                exists tints even where it is unstamped.
//   5. THE BOUND              -- max attenuation is ~50% (charter 12's own
//                                words): strength 255 gives tint 128, so
//                                out = (rgb*128 + 128) >> 8, exactly half
//                                rounded up. Checked at the rails.
//   6. NO ALPHA, NO TAG       -- the port list carries neither. Asserted by
//                                construction: this file cannot drive what
//                                does not exist, and that is the check.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected value; the suite must
// then FAIL. Run it whenever you doubt the checker is alive.
#include "Vzhao_texture_sheetmod.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

// zhao::exit_hard -- tests/harness/zhao_sim.hpp: a plain return from a
// Verilated main can deadlock in VlThreadPool's destructor at ~0 CPU.
#include "../harness/zhao_sim.hpp"

double sc_time_stamp() { return 0.0; }

namespace {

bool g_break_oracle = false;
int g_checks = 0;
int g_fails = 0;

void fail(const char* what, long a, long b) {
  ++g_fails;
  std::printf("FAIL %s: got %ld expected %ld\n", what, a, b);
  if (g_fails > 20) {
    std::printf("SHEETMOD: too many failures, stopping\n");
    zhao::exit_hard(1);
  }
}

void check_eq(const char* what, long got, long want) {
  ++g_checks;
  if (got != want) fail(what, got, want);
}

// ---- ORACLE A: the TEXTURED profile, terrain.cpp:633-640 ------------------
// `sheet_factor` then `mod_of`'s single round-half-up over the product.
// Transcribed from the reference, not from oracle B.
int32_t oracle_a_factor(bool sheet_present, uint8_t strength) {
  if (!sheet_present) return 65536;
  return (255 - (strength >> 1)) << 8;
}

uint8_t oracle_a_channel(bool sheet_present, uint8_t strength, uint8_t v) {
  const int64_t factor = oracle_a_factor(sheet_present, strength);
  const int64_t prod = static_cast<int64_t>(v) * factor;
  const int64_t r = (prod + 32768) >> 16;
  return static_cast<uint8_t>(r);
}

// ---- ORACLE B: the UNTEXTURED profile, terrain.cpp:718 and :756 -----------
// `tint = 255 - (sheet_at >> 1)` then `(lit * tint + 128) >> 8`.
// Transcribed from the reference, not from oracle A.
uint8_t oracle_b_channel(bool sheet_present, uint8_t strength, uint8_t v) {
  if (!sheet_present) return v;
  const int32_t tint = 255 - (strength >> 1);
  return static_cast<uint8_t>((static_cast<int32_t>(v) * tint + 128) >> 8);
}

uint32_t eval(Vzhao_texture_sheetmod* d, bool en, uint8_t strength, uint32_t rgb) {
  d->en_i = en ? 1 : 0;
  d->strength_i = strength;
  d->rgb_i = rgb;
  d->eval();
  return d->rgb_o & 0xFFFFFFu;
}

uint32_t g_lfsr = 0xC0FFEEu;
uint32_t rnd() {
  g_lfsr ^= g_lfsr << 13;
  g_lfsr ^= g_lfsr >> 17;
  g_lfsr ^= g_lfsr << 5;
  return g_lfsr;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--break-oracle") == 0) g_break_oracle = true;

  // =========================================================================
  // 1. THE TWO ORACLES AGREE, EXHAUSTIVELY, WITH NO RTL IN THE ROOM
  // =========================================================================
  // 65,536 pairs. If this section fails, the block's header algebra is wrong
  // and nothing below may be quoted as evidence about the RTL.
  int agree = 0;
  for (int s = 0; s < 256; ++s) {
    for (int v = 0; v < 256; ++v) {
      const uint8_t a = oracle_a_channel(true, static_cast<uint8_t>(s),
                                         static_cast<uint8_t>(v));
      const uint8_t b = oracle_b_channel(true, static_cast<uint8_t>(s),
                                         static_cast<uint8_t>(v));
      ++g_checks;
      if (a != b) {
        fail("oracle A != oracle B", a, b);
      } else {
        ++agree;
      }
    }
  }
  if (agree != 65536) {
    std::printf("SHEETMOD: the two transcriptions of the section 12 law disagree\n");
    zhao::exit_hard(1);
  }
  std::printf("SHEETMOD: 1. the Q16.16 and unit8 statements of the law agree on all %d pairs\n",
              agree);

  Vzhao_texture_sheetmod* d = new Vzhao_texture_sheetmod;

  // =========================================================================
  // 2. EXHAUSTIVE RTL AGAINST ORACLE A, ALL THREE CHANNELS INDEPENDENTLY
  // =========================================================================
  // The channels are driven with DIFFERENT values in the same word, so a block
  // that applied red's tint to green would fail here rather than pass a test
  // that drove all three the same.
  for (int s = 0; s < 256; ++s) {
    for (int v = 0; v < 256; ++v) {
      const uint8_t r = static_cast<uint8_t>(v);
      const uint8_t g = static_cast<uint8_t>(255 - v);
      const uint8_t b = static_cast<uint8_t>((v * 7) & 0xFF);
      const uint32_t in = (static_cast<uint32_t>(r) << 16) |
                          (static_cast<uint32_t>(g) << 8) | b;
      const uint32_t out = eval(d, true, static_cast<uint8_t>(s), in);
      uint32_t want = (static_cast<uint32_t>(
                           oracle_a_channel(true, static_cast<uint8_t>(s), r))
                       << 16) |
                      (static_cast<uint32_t>(
                           oracle_a_channel(true, static_cast<uint8_t>(s), g))
                       << 8) |
                      oracle_a_channel(true, static_cast<uint8_t>(s), b);
      if (g_break_oracle && s == 77 && v == 33) want ^= 1u;
      check_eq("section 2 rgb", out, want);
      check_eq("section 2 applied_o", d->applied_o, 1);
    }
  }
  std::printf("SHEETMOD: 2. exhaustive 256 strengths x 256 values x 3 channels\n");

  // =========================================================================
  // 3. THE IDENTITY ARM IS THE ENABLE, NOT STRENGTH 0
  // =========================================================================
  for (int i = 0; i < 1024; ++i) {
    const uint32_t in = rnd() & 0xFFFFFFu;
    const uint8_t s = static_cast<uint8_t>(rnd() & 0xFF);
    const uint32_t out = eval(d, false, s, in);
    check_eq("section 3 passthrough", out, in);
    check_eq("section 3 applied_o low", d->applied_o, 0);
  }
  std::printf("SHEETMOD: 3. en_i low is bit-for-bit passthrough over 1024 words\n");

  // =========================================================================
  // 4. WITH THE SHEET PRESENT, STRENGTH 0 STILL DARKENS -- BY DESIGN
  // =========================================================================
  // terrain.cpp: "tint only when a sheet exists: even t = 255 would darken by
  // ~0.4% ((v*255+128)>>8 < v for v >= 129)". This asserts the CORRECT
  // behaviour of the tint arm, and section 3 above asserts that the arm is not
  // taken when no sheet exists. Both are needed; neither alone says anything.
  int darkened = 0;
  for (int v = 129; v < 256; ++v) {
    const uint32_t in = (static_cast<uint32_t>(v) << 16);
    const uint32_t out = eval(d, true, 0, in);
    const uint32_t got_r = (out >> 16) & 0xFF;
    ++g_checks;
    if (got_r >= static_cast<uint32_t>(v)) {
      fail("section 4 strength 0 did not darken", got_r, v);
    } else {
      ++darkened;
    }
  }
  check_eq("section 4 count", darkened, 127);
  std::printf("SHEETMOD: 4. with the sheet present, all %d values >= 129 darken at strength 0\n",
              darkened);

  // =========================================================================
  // 5. THE ~50% BOUND, charter section 12's own words
  // =========================================================================
  // strength 255 -> 255 >> 1 = 127 -> tint 128 -> out = (v*128 + 128) >> 8.
  for (int v = 0; v < 256; ++v) {
    const uint32_t in = (static_cast<uint32_t>(v) << 16) |
                        (static_cast<uint32_t>(v) << 8) | static_cast<uint32_t>(v);
    const uint32_t out = eval(d, true, 255, in);
    const uint32_t want_c = static_cast<uint32_t>((v * 128 + 128) >> 8);
    check_eq("section 5 half r", (out >> 16) & 0xFF, want_c);
    check_eq("section 5 half g", (out >> 8) & 0xFF, want_c);
    check_eq("section 5 half b", out & 0xFF, want_c);
  }
  // 254 and 255 both give 127, which is the shift's own law and not a rail.
  check_eq("section 5 tint at 254", (eval(d, true, 254, 0xFF0000u) >> 16) & 0xFF,
           static_cast<uint32_t>((255 * 128 + 128) >> 8));
  std::printf("SHEETMOD: 5. strength 255 is exactly the half-attenuation bound\n");

  // =========================================================================
  // 6. MONOTONE IN STRENGTH -- a property no single vector can state
  // =========================================================================
  // More scar is never brighter. This is what would catch a sign error or a
  // swapped operand that still round-tripped a spot value.
  for (int v = 1; v < 256; ++v) {
    uint32_t prev = 256;
    for (int s = 0; s < 256; s += 2) {
      const uint32_t out =
          eval(d, true, static_cast<uint8_t>(s), static_cast<uint32_t>(v) << 16);
      const uint32_t c = (out >> 16) & 0xFF;
      ++g_checks;
      if (c > prev) fail("section 6 not monotone in strength", c, prev);
      prev = c;
    }
  }
  std::printf("SHEETMOD: 6. the tint is monotone non-increasing in strength\n");

  delete d;

  std::printf("SHEETMOD: %d checks, %d failures%s\n", g_checks, g_fails,
              g_break_oracle ? "  (--break-oracle: a failure is the PASS)" : "");
  // THE POLARITY IS CTEST'S, NOT THIS FILE'S, and getting it backwards makes
  // the control read GREEN-WHEN-BROKEN. The registration marks the
  // --break-oracle run `WILL_FAIL TRUE`, so the control PASSES when this
  // process exits NON-ZERO. A corruption that was CAUGHT therefore exits
  // through the ordinary failure path below; the only special case is the
  // dangerous one -- a corruption that produced NO failure means the checker
  // is blind, and that must exit ZERO so the WILL_FAIL entry goes RED.
  if (g_break_oracle && (g_fails == 0)) {
    std::printf("SHEETMOD: --break-oracle produced NO failure -- THE CHECKER IS BLIND\n");
    zhao::exit_hard(0);
  }
  zhao::exit_hard(g_fails == 0 ? 0 : 1);
}
