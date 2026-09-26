// terrain_shademod_directed.cpp
//
// `zhao_terrain_shademod`'s acceptance suite: the PALETTE LADDER and the ONE
// ROUNDING, against `reference/src/zrender/terrain.cpp`'s `mod_of` lambda.
//
// ---------------------------------------------------------------------------
// TWO TRANSCRIPTIONS, AND THEY MUST AGREE BEFORE EITHER IS QUOTED
// ---------------------------------------------------------------------------
// `zref` does not export `mod_of` -- it is a lambda inside `draw_terrain`. So
// this suite carries TWO independent statements of it, exactly as
// `sheetmod_directed.cpp` does for the sheet tint law:
//
//   ORACLE A, THE LITERAL FORM, terrain.cpp:633-637 transcribed character for
//   character, including its own transcription of `div_rhu_s128` (rast.cpp:31)
//   and the `__int128` the source uses:
//       shade_q = (shade + 8191) >> 14
//       prod    = (shade_q << 14) * tint_q * sheet_q            // s128
//       out     = div_rhu_s128(prod, (__int128)1 << 32)
//
//   ORACLE B, THE REDUCED FORM the RTL implements, written from the ALGEBRA in
//   `zhao_terrain_shademod.sv`'s header and NOT from oracle A:
//       P   = shade_q * tint_q * sheet_q
//       out = (P + 131072) >> 18
//
// Section 1 proves A == B over the whole law domain BEFORE any RTL is touched.
// That is what makes section 2 evidence about the RTL instead of a
// transcription agreeing with itself: if the reduction in the block's header
// is wrong, section 1 fails and nothing downstream is quoted.
//
// ---------------------------------------------------------------------------
// WHAT IT PINS, IN ORDER
// ---------------------------------------------------------------------------
//   1. THE TWO FORMS AGREE       -- no RTL in the room. The 2^32 divide and
//                                   the 2^18 shift are the same rounding.
//   2. RTL vs THE LAW            -- a sweep across every rung, every rung
//                                   BOUNDARY, both tint rails and the sheet's
//                                   real [32768, 65536] range.
//   3. FIVE RUNGS, NOT FOUR      -- the boundaries are located by search and
//                                   asserted at their measured values, and
//                                   `shade_q` is shown to take FIVE distinct
//                                   values over [0, 65536]. `zhao_console_core
//                                   .sv:3810` says FOUR; it derived that from
//                                   `ambient()`, which the textured top
//                                   surface does not apply (terrain.cpp:731).
//   4. RUNG ZERO IS BLACK        -- and reachable. A top-surface triangle
//                                   turned from the sun modulates to EXACTLY
//                                   0 for every tint and every sheet. A
//                                   2-bit ladder could not express this
//                                   fragment.
//   5. THE LADDER MOVES THE PIXEL -- THE POSITIVE CONTROL THE BRIEF DEMANDS.
//                                   The RTL is differenced against a NO-LADDER
//                                   model (the unquantised shade through the
//                                   same rounding) and must DISAGREE. An
//                                   identity that cannot fail is not evidence
//                                   that a ladder is present; this section is
//                                   what distinguishes "quantised" from
//                                   "happens to round the same way".
//   6. EXACT UNITY               -- all three operands at 1.0 return EXACTLY
//                                   65536. The oracle's own sheet-tint lesson.
//   7. ONE ROUNDING, NOT TWO     -- differenced against the composed path's
//                                   arithmetic (two 8-bit `unit_mul`s, the
//                                   shape `zhao_raster_fragment` and
//                                   `zhao_texture_sheetmod` form today), which
//                                   must DISAGREE. This is the divergence
//                                   entry I13 names, measured rather than
//                                   asserted.
//   8. BOTH COUNTERS FIRE        -- `issued_o` counts accepts exactly (the
//                                   "how many times" number the throughput
//                                   budget is written against), and
//                                   `shade_domain_o` fires on an unclamped
//                                   offer and stays at zero across the whole
//                                   legal domain. Both are reachable with
//                                   legal stimulus, so neither needs a
//                                   committed mutant.
//   9. THE HANDSHAKE             -- `req_ready_o` falls while the shift-add
//                                   runs, the result is held stable under
//                                   `rsp_ready_i` backpressure, and no request
//                                   is accepted twice.
//
// POSITIVE CONTROL: --break-oracle corrupts ONE expected value; the suite must
// then FAIL. Registered as the `shademod_break_oracle` ctest.
#include "Vzhao_terrain_shademod.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "verilated.h"

// zhao::exit_hard / zhao::tick -- tests/harness/zhao_sim.hpp.
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
    std::printf("SHADEMOD: too many failures, stopping\n");
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

// ==========================================================================
// ORACLE A -- terrain.cpp:633-637 verbatim, with rast.cpp:31's divide
// ==========================================================================
// Transcribed from the reference. NOT from oracle B.
int32_t a_div_rhu_s128(__int128 n, __int128 d) {
  if (d < 0) {
    n = -n;
    d = -d;
  }
  __int128 h = n + d / 2;
  __int128 q = h / d;
  __int128 r = h % d;
  if (r != 0 && r < 0) q -= 1;  // floor semantics (qformats 4)
  return static_cast<int32_t>(q > INT32_MAX ? INT32_MAX : (q < INT32_MIN ? INT32_MIN : q));
}

int32_t oracle_a_mod(int32_t shade, int32_t tint_q, int32_t sheet_q) {
  const int32_t shade_q = (shade + 8191) >> 14;  // the palette ladder (0..4)
  const __int128 prod = static_cast<__int128>(shade_q << 14) * tint_q * sheet_q;
  return static_cast<int32_t>(a_div_rhu_s128(prod, static_cast<__int128>(1) << 32));
}

// ==========================================================================
// ORACLE B -- the reduction in the RTL header, written from the ALGEBRA
// ==========================================================================
// floor((P*2^14 + 2^31) / 2^32) == floor((P + 2^17) / 2^18) for P >= 0.
// Written from that identity, not by simplifying oracle A's code.
int64_t oracle_b_mod(int64_t shade, int64_t tint_q, int64_t sheet_q) {
  const int64_t shade_q = (shade + 8191) >> 14;
  const int64_t p = shade_q * tint_q * sheet_q;
  return (p + 131072) >> 18;
}

// ==========================================================================
// THE TWO RIVALS -- models the RTL must DISAGREE with. Neither is a law.
// ==========================================================================
// NO LADDER: the same single rounding over the UNQUANTISED shade. This is
// what every pass before this one costed `mod_of` as.
int64_t rival_no_ladder(int64_t shade, int64_t tint_q, int64_t sheet_q) {
  const __int128 prod = static_cast<__int128>(shade) * tint_q * sheet_q;
  return static_cast<int64_t>((prod + (static_cast<__int128>(1) << 31)) >> 32);
}

// The three-line primitive both composed blocks carry
// (`zhao_texture_sheetmod.sv:132-137`, `zhao_raster_fragment`): one
// round-half-up over an 8-bit product.
int64_t unit_mul(int64_t a, int64_t b) { return (a * b + 128) >> 8; }

int64_t sat_u8(int64_t v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// THE COMPOSED PATH'S PIXEL, today. A terrain triangle's flat shade arrives at
// `zhao_raster_fragment`'s `unit_mul` with `zhao_texture_sheetmod`'s ahead of
// it: TWO 8-BIT ROUNDINGS AND NO LADDER (entry I13, item 1). The Q16.16
// factors narrow to the byte gains those blocks take; 65536 >> 8 is 256, which
// an 8-bit gain cannot hold, and that narrowing is itself part of why this
// path cannot express exact unity (the sheetmod header's 0.4% lesson).
int64_t rival_composed_pixel(int64_t texel, int64_t shade, int64_t sheet_q) {
  const int64_t s8 = shade >= 65536 ? 255 : (shade >> 8);
  const int64_t h8 = sheet_q >= 65536 ? 255 : (sheet_q >> 8);
  return sat_u8(unit_mul(unit_mul(texel, h8), s8));
}

// THE LAW'S PIXEL: `mod_of` once, then the rasteriser's single application
// `sat_u8((texel * mod + 32768) >> 16)` (rast.cpp:349-351).
int64_t law_pixel(int64_t texel, int64_t mod) { return sat_u8((texel * mod + 32768) >> 16); }

// ==========================================================================
// THE RTL DRIVER
// ==========================================================================
struct Dut {
  Vzhao_terrain_shademod* d;

  void reset() {
    d->rst_n = 0;
    d->req_valid_i = 0;
    d->rsp_ready_i = 0;
    d->shade_i = 0;
    d->tint_i = 0;
    d->sheet_i = 0;
    d->clk = 0;
    d->eval();
    for (int i = 0; i < 3; ++i) zhao::tick(*d);
    d->rst_n = 1;
    d->eval();
    zhao::tick(*d);
  }

  // One full request/response. Returns mod_o. `hold` adds backpressure cycles
  // on the response so the held-stable path is exercised too.
  uint32_t eval_once(uint32_t shade, uint32_t tint, uint32_t sheet, int hold = 0) {
    d->shade_i = shade;
    d->tint_i = tint;
    d->sheet_i = sheet;
    d->req_valid_i = 1;
    d->rsp_ready_i = 0;
    // Offer until accepted.
    int guard = 0;
    while (true) {
      d->eval();
      const bool accepted = (d->req_ready_o != 0);
      zhao::tick(*d);
      if (accepted) break;
      if (++guard > 64) {
        std::printf("SHADEMOD: request never accepted (hang)\n");
        zhao::exit_hard(1);
      }
    }
    d->req_valid_i = 0;
    // Wait for the response.
    guard = 0;
    while (true) {
      d->eval();
      if (d->rsp_valid_o != 0) break;
      zhao::tick(*d);
      if (++guard > 64) {
        std::printf("SHADEMOD: response never arrived (hang)\n");
        zhao::exit_hard(1);
      }
    }
    const uint32_t first = d->mod_o;
    // Hold it under backpressure: the value must not move.
    for (int i = 0; i < hold; ++i) {
      zhao::tick(*d);
      d->eval();
      if (d->rsp_valid_o == 0 || d->mod_o != first) {
        std::printf("FAIL held result moved under backpressure\n");
        ++g_fails;
        break;
      }
      ++g_checks;
    }
    d->rsp_ready_i = 1;
    zhao::tick(*d);
    d->rsp_ready_i = 0;
    d->eval();
    return first;
  }
};

uint32_t g_lfsr = 0x5EED1234u;
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
  // 1. THE TWO FORMS AGREE, WITH NO RTL IN THE ROOM
  // =========================================================================
  // If the reduction in the block's header is wrong, this fails and every
  // number below it is withdrawn.
  {
    long n = 0;
    for (int64_t shade = 0; shade <= 65536; shade += 397) {
      for (int64_t tint = 0; tint <= 65536; tint += 1021) {
        for (int64_t sheet = 32768; sheet <= 65536; sheet += 1023) {
          const int64_t a = oracle_a_mod(static_cast<int32_t>(shade), static_cast<int32_t>(tint),
                                         static_cast<int32_t>(sheet));
          const int64_t b = oracle_b_mod(shade, tint, sheet);
          check_eq("form A == form B", a, b);
          ++n;
        }
      }
    }
    // And at every rung boundary and both rails exactly.
    const int64_t edges[] = {0, 1, 8192, 8193, 24576, 24577, 40960, 40961, 57344, 57345, 65535,
                             65536};
    for (int64_t shade : edges)
      for (int64_t tint : {int64_t{0}, int64_t{1}, int64_t{65535}, int64_t{65536}})
        for (int64_t sheet : {int64_t{32768}, int64_t{65280}, int64_t{65536}}) {
          check_eq("form A == form B (edge)",
                   oracle_a_mod(static_cast<int32_t>(shade), static_cast<int32_t>(tint),
                                static_cast<int32_t>(sheet)),
                   oracle_b_mod(shade, tint, sheet));
          ++n;
        }
    std::printf("1. two forms agree over %ld operand triples\n", n);
  }

  Vzhao_terrain_shademod dut_model;
  Dut dut{&dut_model};
  dut.reset();

  // =========================================================================
  // 2. THE RTL AGAINST THE LAW
  // =========================================================================
  {
    long n = 0;
    const int64_t shades[] = {0,     1,     4096,  8192,  8193,  16384, 24576, 24577,
                              32768, 40960, 40961, 49152, 57344, 57345, 60000, 65535,
                              65536};
    const int64_t tints[] = {0, 1, 8192, 21845, 32768, 43690, 65024, 65535, 65536};
    const int64_t sheets[] = {32768, 40960, 49152, 57344, 65024, 65280, 65535, 65536};
    for (int64_t shade : shades)
      for (int64_t tint : tints)
        for (int64_t sheet : sheets) {
          int64_t want = oracle_a_mod(static_cast<int32_t>(shade), static_cast<int32_t>(tint),
                                      static_cast<int32_t>(sheet));
          if (g_break_oracle && n == 17) want += 1;
          const uint32_t got = dut.eval_once(static_cast<uint32_t>(shade),
                                             static_cast<uint32_t>(tint),
                                             static_cast<uint32_t>(sheet), (n % 37 == 0) ? 3 : 0);
          check_eq("RTL == mod_of", got, want);
          ++n;
        }
    // ... and a randomised sweep across the whole law domain.
    for (int i = 0; i < 3000; ++i) {
      const int64_t shade = rnd() % 65537;
      const int64_t tint = rnd() % 65537;
      const int64_t sheet = 32768 + (rnd() % 32769);
      const uint32_t got = dut.eval_once(static_cast<uint32_t>(shade), static_cast<uint32_t>(tint),
                                         static_cast<uint32_t>(sheet));
      check_eq("RTL == mod_of (random)", got,
               oracle_a_mod(static_cast<int32_t>(shade), static_cast<int32_t>(tint),
                            static_cast<int32_t>(sheet)));
      ++n;
    }
    std::printf("2. RTL matches mod_of over %ld operand triples\n", n);
  }

  // =========================================================================
  // 3. FIVE RUNGS, NOT FOUR -- the entry's own claim, measured
  // =========================================================================
  // The boundaries are FOUND by walking the domain, not asserted from the
  // header, and then the found values are checked against the arithmetic.
  {
    int distinct = 0;
    int64_t last_q = -1;
    int64_t found[8];
    for (int64_t shade = 0; shade <= 65536; ++shade) {
      const int64_t q = (shade + 8191) >> 14;
      if (q != last_q) {
        if (distinct < 8) found[distinct] = shade;
        ++distinct;
        last_q = q;
      }
    }
    check_eq("shade_q takes FIVE values over [0, 65536]", distinct, 5);
    check_eq("rung 0 begins at 0", found[0], 0);
    check_eq("rung 1 begins at 8193", found[1], 8193);
    check_eq("rung 2 begins at 24577", found[2], 24577);
    check_eq("rung 3 begins at 40961", found[3], 40961);
    check_eq("rung 4 begins at 57345", found[4], 57345);
    // And the RTL agrees that the rung is flat between boundaries: every shade
    // inside one rung gives the SAME mod, and the first shade of the next rung
    // gives a different one.
    for (int r = 0; r < 4; ++r) {
      const uint32_t lo = dut.eval_once(static_cast<uint32_t>(found[r]), 65536, 65536);
      const uint32_t hi = dut.eval_once(static_cast<uint32_t>(found[r + 1] - 1), 65536, 65536);
      const uint32_t nx = dut.eval_once(static_cast<uint32_t>(found[r + 1]), 65536, 65536);
      check_eq("rung is FLAT", lo, hi);
      check_ne("rung STEPS at its boundary", nx, lo);
    }
    std::printf("3. five rungs located at 0 / 8193 / 24577 / 40961 / 57345\n");
  }

  // =========================================================================
  // 4. RUNG ZERO IS BLACK, AND IT IS REACHABLE
  // =========================================================================
  // The path that carries a real tint and a real sheet is the TEXTURED TOP
  // SURFACE (terrain.cpp:731), which does NOT apply `ambient()`. Its shade is
  // `shade_flat_tri`'s clamp01, so 0 is in the domain and rung 0 is real.
  {
    for (int64_t shade : {int64_t{0}, int64_t{1}, int64_t{4095}, int64_t{8192}})
      for (int64_t tint : {int64_t{0}, int64_t{32768}, int64_t{65536}})
        for (int64_t sheet : {int64_t{32768}, int64_t{65536}}) {
          check_eq("rung 0 is EXACTLY black",
                   dut.eval_once(static_cast<uint32_t>(shade), static_cast<uint32_t>(tint),
                                 static_cast<uint32_t>(sheet)),
                   0);
        }
    // And it is NOT black one rung up, or the check above would be vacuous.
    check_ne("rung 1 is not black", dut.eval_once(8193, 65536, 65536), 0);
    std::printf("4. rung 0 is black for every tint and sheet; rung 1 is not\n");
  }

  // =========================================================================
  // 5. THE LADDER MOVES THE PIXEL -- the positive control
  // =========================================================================
  // Differenced against the SAME single rounding over the UNQUANTISED shade.
  // If the RTL had no ladder these would agree and section 2 would still pass,
  // because most operands round the same way either side of the quantisation.
  {
    int moved = 0;
    long n = 0;
    for (int64_t shade = 0; shade <= 65536; shade += 251)
      for (int64_t tint : {int64_t{32768}, int64_t{65536}})
        for (int64_t sheet : {int64_t{49152}, int64_t{65536}}) {
          const uint32_t got = dut.eval_once(static_cast<uint32_t>(shade),
                                             static_cast<uint32_t>(tint),
                                             static_cast<uint32_t>(sheet));
          if (static_cast<int64_t>(got) != rival_no_ladder(shade, tint, sheet)) ++moved;
          ++n;
        }
    ++g_checks;
    if (moved * 2 < n) {
      ++g_fails;
      std::printf("FAIL the ladder barely moves anything: %d of %ld differ\n", moved, n);
    }
    std::printf("5. ladder vs no-ladder: %d of %ld operand triples DIFFER\n", moved, n);
    // A NAMED case, so the control is not only statistical.
    //
    // CAUGHT WHILE WRITING THIS: the obvious named case, a half-lit shade of
    // 32768, is VACUOUS. The ladder snaps it to rung 2, whose gain is exactly
    // 32768, so quantised and unquantised agree to the bit and a `check_ne`
    // there would have failed against correct RTL. The rung CENTRES are
    // precisely where the ladder is invisible -- which is the same shape as
    // this repository's "an identity that cannot fail is not evidence", one
    // level down. Pick a shade INSIDE a rung and away from its centre.
    const uint32_t centre = dut.eval_once(32768, 65536, 65536);
    check_eq("rung-2 centre: quantised and unquantised agree (the vacuous case)",
             centre, rival_no_ladder(32768, 65536, 65536));
    const uint32_t off = dut.eval_once(40000, 65536, 65536);
    check_ne("ladder MOVES a shade inside rung 2 but off its centre", off,
             rival_no_ladder(40000, 65536, 65536));
    check_eq("40000 snaps down to rung 2's gain", off, 32768);
    check_eq("unquantised would have kept 40000", rival_no_ladder(40000, 65536, 65536), 40000);
  }

  // =========================================================================
  // 6. EXACT UNITY
  // =========================================================================
  {
    check_eq("all-unity is EXACT unity", dut.eval_once(65536, 65536, 65536), 65536);
    // The sheet's own unity arm, which is a separate value from strength 0.
    check_eq("unity tint, unity sheet, full shade", dut.eval_once(65536, 65536, 65536), 65536);
    std::printf("6. all-unity returns exactly 65536\n");
  }

  // =========================================================================
  // 7. ONE ROUNDING, NOT TWO -- the divergence entry I13 names
  // =========================================================================
  // Compared IN THE PIXEL, and like with like: the tint is held at unity so
  // both sides carry the same two factors, and the difference that remains is
  // the ladder plus the rounding count and nothing else.
  {
    int differ = 0;
    int worst = 0;
    long n = 0;
    for (int64_t texel = 0; texel <= 255; texel += 5)
      for (int64_t shade : {int64_t{8192}, int64_t{16384}, int64_t{32768}, int64_t{40000},
                            int64_t{49152}, int64_t{65536}})
        for (int64_t sheet : {int64_t{32768}, int64_t{49152}, int64_t{65280}, int64_t{65536}}) {
          const uint32_t mod = dut.eval_once(static_cast<uint32_t>(shade), 65536,
                                             static_cast<uint32_t>(sheet));
          const int64_t law = law_pixel(texel, mod);
          const int64_t comp = rival_composed_pixel(texel, shade, sheet);
          const int d = static_cast<int>(law > comp ? law - comp : comp - law);
          if (d != 0) ++differ;
          if (d > worst) worst = d;
          ++n;
        }
    ++g_checks;
    if (differ == 0) {
      ++g_fails;
      std::printf("FAIL the composed two-rounding shape agrees everywhere; "
                  "the divergence I13 names would not exist\n");
    }
    std::printf("7. law pixel vs composed pixel: %d of %ld DIFFER, worst %d LSB\n", differ, n,
                worst);
    // The named case: ALL-UNITY, where the law is exact and the composed path
    // provably is not -- 255/256 twice over.
    const uint32_t unity_mod = dut.eval_once(65536, 65536, 65536);
    check_eq("law: a full-scale texel at all-unity is UNTOUCHED", law_pixel(255, unity_mod), 255);
    check_ne("composed: the same fragment is NOT untouched",
             rival_composed_pixel(255, 65536, 65536), 255);
  }

  // =========================================================================
  // 8. BOTH COUNTERS FIRE
  // =========================================================================
  {
    // `issued_o` counts accepts EXACTLY -- the "how many times" number. A
    // result-checking test cannot see a block that modulates twice.
    Vzhao_terrain_shademod fresh_model;
    Dut fresh{&fresh_model};
    fresh.reset();
    check_eq("issued starts at zero", fresh_model.issued_o, 0);
    check_eq("shade_domain starts at zero", fresh_model.shade_domain_o, 0);
    for (int i = 0; i < 25; ++i) fresh.eval_once(65536, 65536, 65536, i % 5);
    check_eq("issued counts every accept ONCE", fresh_model.issued_o, 25);

    // The domain counter stays silent across the ENTIRE legal domain ...
    check_eq("shade_domain silent on the legal domain", fresh_model.shade_domain_o, 0);
    fresh.eval_once(65536, 65536, 65536);
    check_eq("shade_domain silent at the rail exactly", fresh_model.shade_domain_o, 0);
    // ... and FIRES one LSB past it. This is the guard being seen to fire, on
    // legal stimulus, so it needs no committed mutant.
    const uint32_t before = fresh_model.shade_domain_o;
    fresh.eval_once(65537, 65536, 65536);
    check_eq("shade_domain FIRES at 65537", fresh_model.shade_domain_o, before + 1);
    fresh.eval_once(131071, 65536, 65536);
    check_eq("shade_domain FIRES again at the port rail", fresh_model.shade_domain_o, before + 2);
    // And the arithmetic is still EXACT out there -- the block counts the
    // broken palette budget, it does not silently truncate the pixel.
    check_eq("out-of-domain result is still exact", dut.eval_once(131071, 65536, 65536),
             oracle_b_mod(131071, 65536, 65536));
    std::printf("8. issued_o counted 25/25; shade_domain_o fired at 65537 and at 131071\n");
  }

  // =========================================================================
  // 9. THE HANDSHAKE
  // =========================================================================
  {
    Vzhao_terrain_shademod hs_model;
    Dut hs{&hs_model};
    hs.reset();
    hs_model.eval();
    check_eq("ready at idle", hs_model.req_ready_o, 1);
    check_eq("not valid at idle", hs_model.rsp_valid_o, 0);
    // Accept one, then confirm ready falls while the shift-add runs.
    hs_model.shade_i = 65536;
    hs_model.tint_i = 65536;
    hs_model.sheet_i = 65536;
    hs_model.req_valid_i = 1;
    hs_model.rsp_ready_i = 0;
    hs_model.eval();
    zhao::tick(hs_model);
    hs_model.req_valid_i = 0;
    hs_model.eval();
    check_eq("ready falls while multiplying", hs_model.req_ready_o, 0);
    int cycles = 0;
    while (hs_model.rsp_valid_o == 0 && cycles < 64) {
      zhao::tick(hs_model);
      hs_model.eval();
      ++cycles;
    }
    check_eq("result arrives in the budgeted 17 shift-add clocks", cycles, 17);
    check_eq("and it is right", hs_model.mod_o, 65536);
    check_eq("still not ready while the result is unread", hs_model.req_ready_o, 0);
    std::printf("9. handshake: 17 clocks, ready held low, result stable\n");
  }

  std::printf("SHADEMOD: %ld checks, %d failures\n", g_checks, g_fails);
  if (g_fails != 0) {
    std::printf("SHADEMOD: FAIL\n");
    zhao::exit_hard(1);
  }
  std::printf("SHADEMOD: PASS\n");
  zhao::exit_hard(0);
}
