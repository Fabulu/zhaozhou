// island_composed_directed.cpp — the composed texture island carries a
// fragment from end to end.
// Authored 2026-09-05 (roadmap G1-D). Fixture REBUILT 2026-09-06 (W10).
//
// ---------------------------------------------------------------------------
// WHY A FIT IS NOT ENOUGH
// ---------------------------------------------------------------------------
// The composed fit answers CAPACITY: how much silicon the island costs when its
// blocks are wired to each other instead of to pads. It cannot answer whether
// the wiring is right. A top that connects eleven blocks through mismatched
// handshakes still synthesises, still fits, and still reports an ALM count --
// it simply never moves a fragment.
//
// The roadmap is explicit that this is the trap:
//
//   > Each integration step needs a composed test that ACTUALLY DRAWS THROUGH
//   > THE ADDED HARDWARE.
//
// So this drives fragments in at the island's boundary and requires them to
// come out the other end, having visibly passed through every block on the way.
//
// ---------------------------------------------------------------------------
// W10: WHAT THIS FIXTURE USED TO CLAIM, AND WHY FOUR OF THOSE CLAIMS WERE VOID
// ---------------------------------------------------------------------------
// The owner's pre-fit brief (reports section 5.1-5.6) and its addendum (12.1
// to 12.4) took this file apart. Every finding was re-verified against the RTL
// before this rebuild, and every one of them held:
//
//   1. NO MODE RECORD EXISTED. `bind_mode_i` was the bare literal 0x6600 for
//      the whole run, and `zref::Tmu::Mode` -- the packer that exists for
//      exactly this -- was never included. 0x6600 decodes
//      (zhao_texture_tmu_plan.sv:229-252) as m_fmt=0 CLUT8, m_filter=0,
//      log2w=log2h=6, and `filter_eff = m_filter && !is_clut` is therefore 0.
//      So `acc_en_o = 4'b0001` (:465) -- ONE LANE -- and `fu/fv = 0` (:301) on
//      every transaction in the file.
//
//   2. THE "BILINEAR" HALF DID NOT REQUEST BILINEAR. It asked for the bilinear
//      CONSUMER by driving `frag_class_i = 2`, which routes on
//      `cache_smp_src[15:14]` (island_top:1096) and has nothing to do with the
//      planner's verdict. The island counts that disagreement today in
//      `err_class_mismatch_o`, and this file never read it. A real filtered
//      request needs a direct-colour format AND the filter bit -- 0x6609, not
//      0x6600 -- because the planner raises `err_c` on a filtered CLUT (:249).
//
//   3. THE TEXELS WERE UNIFORM. `fill_data_i = 0x8586` on every beat,
//      address-independent, and `mem.addr` was captured and never read. The
//      old comment claimed "all four texels come from the same memory pattern
//      ... so they are identical". They are NOT identical: with en=0001 lanes
//      1..3 are never fetched (cache_pipe:264 `c3_need_c = c2_en & ~c3_hit_c`,
//      fill masked by `fb_mask_r`), yet cache_pipe:543 copies ALL FOUR lanes
//      into the response word unconditionally, so taps 1..3 were untagged RAM.
//      The exact check passed only because fu=fv=0 makes the filter return t00.
//      A bilinear weighting bug and a nearest fetch produced the same answer.
//
//   4. THE EXPECTATIONS WERE NOT ADDRESS-SPECIFIC. The CLUT oracle accepted
//      `want(recipe, c_lo) OR want(recipe, c_hi)` -- either byte of one
//      constant halfword -- and nothing computed which texel THIS fragment
//      should have read. A sample fetched from the wrong address still passed.
//
// The rebuild below fixes all four, and the argument for each is written where
// the value is chosen rather than here.
//
// ---------------------------------------------------------------------------
// THE SHAPE OF THE REBUILD: TWO REAL PHASES, BECAUSE THE MODE IS A GLOBAL PORT
// ---------------------------------------------------------------------------
// `bind_mode_i` and `bind_base_i` are ISLAND-LEVEL ports read by the planner at
// its own T0 handshake (island_top:933), many clocks after the fragment was
// admitted. Writing them inside the submit loop does not attach a mode to a
// fragment -- it is a late ingress read, the exact class
// `tools/rtl/check_ingress_capture.py` exists to prevent, and the gate misses
// it because its contract watches the `frag_` prefix only.
//
// So a mode CANNOT travel per fragment on the present port. Mixing CLUT and
// RGB565 fragments in one stream would measure that bug rather than the
// filter. The fixture therefore runs FULLY DRAINED PHASES:
//
//   PHASE 1  CLUT8 / NEAREST   bind_mode 0x6600, every fragment CLS_CLUT
//   PHASE 2  RGB565 / BILINEAR bind_mode 0x6609, every fragment CLS_BIL
//   PHASE 3  the owner-credit and reorder-wrap stress, CLUT8 again
//
// In phases 1 and 2 the mode word and the class AGREE, so `err_class_mismatch_o`
// must be exactly zero -- which is the island's own comment at :1017-1030
// asking for precisely this ("nonzero today, and zero when the fixture drives a
// mode and a class that agree").
//
// ---------------------------------------------------------------------------
// HOW IT AVOIDS PASSING VACUOUSLY
// ---------------------------------------------------------------------------
// "A fragment came out" is a weak claim: FRAGROB could retire a fragment whose
// samples never reached the cache, and the picture would be wrong but the
// handshake would look healthy. So the acceptance condition is that **every
// block's counter moved** -- and, since W10, that every retired colour is the
// EXACT value `zref::Tmu::sample` and `zref::material::combine` compute for
// THAT fragment's own address, from a texture whose neighbouring texels are
// different by construction.
//
// The fixture also asserts its own SEPARATION properties: for every checked
// fragment it proves that nearest, correct-bilinear and wrong-weight-bilinear
// give three different answers, and that the four neighbouring addresses give
// four different answers. Those checks are what make a green run mean
// something -- they are the fixture testing itself.

// ---------------------------------------------------------------------------
// FIRE-TEST EVIDENCE: THIS FIXTURE HAS BEEN SHOWN TO GO RED
// ---------------------------------------------------------------------------
// "A detector that has not been shown to FIRE has not been tested." Four
// mutations were applied to the STIMULUS and the MODE RECORD -- never to the
// RTL, which is frozen for a running fit -- and the results are recorded here.
// Baseline for all four: 75/75 checks pass, and the island's own
// `err_class_mismatch_o` $warning fires ZERO times (it fired 396 times on the
// pre-W10 fixture, which is the RTL saying out loud that the test was lying).
//
// A. THE MODE. `mode_rgb565_bilinear()` with `m.bilinear = false`, i.e. the
//    original defect -- asking for a filter and never setting bit 3.
//    Packs to 0x6601. TEN checks fired:
//      "the RGB565/bilinear record packs to 0x6609 ...": expected 26121, got 26113
//      "and the RGB565+filter mode derives class BILINEAR": expected 2, got 1
//      "the bilinear mode plans FOUR cache lanes ...": expected 4, got 1
//      "and the filtered plan carries the fractions ...": expected 16528, got 0
//      "and the filter ran THREE JOBS PER SAMPLE ...": expected 288, got 0
//      "every BILINEAR fragment retires EXACTLY ...": expected 0, got 32
//      "for EVERY bilinear fragment the correct filtered value differs from tap 0":
//                                                       expected 0, got 32
//      "and differs from the same taps with fu and fv exchanged": expected 0, got 32
//      "BILERP ran filter jobs ...": expected 1, got 0
//      "and the nearest class was never used ...": expected 0, got 96
//    AND IT SURFACED AN RTL GAP. Every RGB565 sample retired 0xFF00FF -- the
//    island's SMP_ERR_RGB -- with `cnt_near_refused_o == 96`. CLS_NEAR has no
//    decode (island_top:1379 `near_ok_c = 1'b0`, and :1490), so an unfiltered
//    direct-colour texture ships magenta today. That is a real defect, not a
//    fixture artefact, and it was invisible while every request was CLUT8.
//
// B. THE TEXELS. `rgb565_at` returning the constant 0x8586 -- the exact
//    pre-W10 memory. SIX checks fired, and THE POINT IS WHICH ONES DID NOT:
//      "and no texel in the RGB565 sheet equals its neighbour either": got 8192
//      "for EVERY bilinear fragment ... differs from tap 0": expected 0, got 32
//      "and differs from the same taps with fu and fv exchanged": expected 0, got 32
//      "and differs from the taps in the wrong lane order": expected 0, got 32
//      "and the four taps of every footprint are four DIFFERENT texels": got 192
//      "a footprint fetched one texel away ... would have retired a different
//       colour": expected 0, got 128
//    while `P2 exact colour: checked 32, mismatched 0` STILL PASSED. That is
//    the old fixture reproduced exactly: its headline check was green on a
//    memory that made the filter unobservable. The separation guards are the
//    difference between a green run and evidence.
//
// C. THE ADDRESS. Both phases driven at texel X+1 while the expectation stays
//    at X. TWO checks fired, and they fired on EVERY fragment:
//      "every CLUT fragment retires EXACTLY the colour zref::Tmu::sample
//       fetches from ITS OWN planned address ...": expected 0, got 32
//      "every BILINEAR fragment retires EXACTLY ...": expected 0, got 32
//    e.g. P1 frag 0 got 0x216929, want 0x184184. 64 of 64 detected -- no
//    fragment slipped through on a wrong address, which is the property the
//    old "either of two colours" oracle could not state at all.
//
// D. THE WEIGHTS. Phase 2 driven with fu and fv EXCHANGED -- the same four
//    taps at the wrong weights. ONE check fired, on all 32:
//      "every BILINEAR fragment retires EXACTLY ...": expected 0, got 32
//      e.g. P2 frag 1 got 0x222026, want 0x161721
//    This is the mutation the pre-W10 file was structurally incapable of
//    detecting: with fu = fv = 0 there were no weights to get wrong.
//
// ---------------------------------------------------------------------------
// FIRE-TEST EVIDENCE FOR THE DEFECT (d) PHASES, 2026-09-06
// ---------------------------------------------------------------------------
// Phases 4, 5 and 6 and the checks around them were added with the shared
// format-controlled decode, so their mutations are applied to the RTL rather
// than to the stimulus -- no fit was running, and a stimulus mutation cannot
// show that a DECODE detector fires. Each was applied to
// `zhao_texture_island_top.sv` alone, built, run, and reverted; the file was
// byte-compared against its pre-mutation copy afterwards. Baseline for all
// three: 108/108 checks pass.
//
// E. THE ORIGINAL DEFECT, PUT BACK. `near_ok_c = 1'b0` -- the placeholder the
//    shared decode replaced. SIX checks fired:
//      "AND NOT ONE SAMPLE WAS REFUSED ... in phase RGB565/nearest":
//                                                      expected 0, got 96
//      "every RGB565/nearest fragment retires EXACTLY the colour
//       zref::Tmu::sample decodes ...":                expected 0, got 32
//      "AND NOT ONE SAMPLE WAS REFUSED ... in phase ARGB1555/nearest":
//                                                      expected 0, got 96
//      "every ARGB1555/nearest fragment retires EXACTLY ...": expected 0, got 32
//      "and every ARGB1555/nearest fragment's ALPHA matches the reference ...":
//                                                      expected 0, got 31
//      "and across the whole run NOT ONE sample was refused for want of a
//       nearest decode ...":                           expected 0, got 192
//    and the printed line read `NEAR REFUSED +96` for each nearest phase --
//    the same 96 the pre-repair island measured. The retired colours were the
//    error colour: `RGB565/nearest frag 0 recipe 0 texel(3,5): got 0xFF00FF,
//    want 0x211463`, and `frag 1 recipe 1: got 0xFE00FE` because MODULATE
//    scales even the magenta. THE BILINEAR PHASE WAS UNTOUCHED, which is the
//    point: this mutation is specific to the lane it disables.
//
// F. A FORMAT-BLIND DECODE. Both call sites pinned to `FMT_RGB565` -- the
//    island's behaviour before `sampmeta_m` carried a format, reproduced
//    exactly. SIX checks fired, and WHICH ONES DID NOT is the evidence:
//      "every ARGB1555/nearest fragment retires EXACTLY ...": expected 0, got 32
//      "and every ARGB1555/nearest fragment's ALPHA matches ...": expected 0, got 31
//      "and for EVERY ARGB1555/nearest fragment the decoded answer DIFFERS from
//       what the old hardwired RGB565 extraction would have produced ...":
//                                                      expected 32, got 0
//      "every ARGB4444/bilinear fragment retires EXACTLY ...": expected 0, got 32
//      "and every ARGB4444/bilinear fragment's ALPHA matches ...": expected 0, got 32
//      "and for EVERY ARGB4444/bilinear fragment the decoded answer DIFFERS
//       ...":                                          expected 32, got 0
//    Phases 1, 2 and 4 -- CLUT8 and the two RGB565 phases -- passed unchanged,
//    because RGB565 is the one format a format-blind decode gets right. That
//    asymmetry is the whole reason the ARGB phases exist, and it is why "the
//    suite is green" was true for months while `chan8()` had no format input.
//
// G. ALPHA BACK TO THE LITERAL. `fr_tmu_a = smp_err_c ? SMP_ERR_A : 8'hFF`,
//    with the colour decode left correct, so this isolates alpha from the
//    rest of the repair. FIVE checks fired:
//      "every ARGB1555/nearest fragment retires EXACTLY ...": expected 0, got 4
//      "and every ARGB1555/nearest fragment's ALPHA matches ...": expected 0, got 31
//      "and the ARGB1555/nearest alpha is LOAD-BEARING ...": expected 1, got 0
//      "and every ARGB4444/bilinear fragment's ALPHA matches ...": expected 0, got 32
//      "and the ARGB4444/bilinear alpha is LOAD-BEARING ...": expected 1, got 0
//    e.g. `ARGB4444/bilinear frag 0 recipe 0: alpha got 255, want 58` and
//    `ARGB1555/nearest frag 0 recipe 0: alpha got 255, want 0`.
//    FOUR ARGB1555 COLOURS MOVED TOO, which is not incidental: MASK gates on
//    `s1.a != 0`, so a hardwired opaque alpha does not merely report the wrong
//    transparency, it paints a shape that should have been cut away.

#include "zref/zref_material.hpp"
#include "zref/zref_texture.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "Vzhao_texture_island_top.h"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what, long long expected, long long got) {
  ++g_checks;
  if (!ok) {
    ++g_failed;
    std::printf("FAIL: %s: expected %lld, got %lld\n", what, expected, got);
  }
}

using Dut = Vzhao_texture_island_top;

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

// ===========================================================================
// THE TEXTURE POOL
// ===========================================================================
// One flat image behind the cache's fill port, laid out so both phases and the
// palette live in it and `zref::Tmu::sample` can be used as the oracle for
// both without the test re-deriving one line of addressing.
//
//   0x0010_0000  4096 B   CLUT8 texture, 64 x 64, one index per byte
//   0x0010_1000   512 B   the palette, 256 RGB565 entries, little-endian
//   0x0010_2000  8192 B   RGB565 texture, 64 x 64, one halfword per texel
//
// The palette region is never fetched by the island -- PALETTE_RES holds its
// own copy, uploaded through the load protocol below -- but `zref::Tmu::sample`
// reads a CLUT entry out of `TextureMemory` at `pal_base`, so keeping the two
// copies in one place is what lets the reference model the whole path.
constexpr uint32_t kImgBase = 0x0010'0000u;
constexpr uint32_t kBaseClut = 0x0010'0000u;
constexpr uint32_t kPalBase = 0x0010'1000u;
constexpr uint32_t kBaseRgb = 0x0010'2000u;
constexpr uint32_t kImgBytes = 0x4000u;

constexpr int kPalEntries = 256;

// ---------------------------------------------------------------------------
// WHY THESE NUMBERS AND NOT OTHERS. THIS IS THE POINT OF THE FIXTURE.
// ---------------------------------------------------------------------------
// The old memory answered 0x8586 for every beat. Uniform texels make a filter's
// fractions unobservable: bilerp(c,c,c,c,fu,fv) == c for ALL fu, fv, so a
// weighting bug, a lane-order bug and a nearest fetch all produce the identical
// answer. Nothing in the file could tell them apart.
//
// The replacement is built from PERMUTATION TABLES indexed by different linear
// combinations of (x, y). Each table holds 16 DISTINCT values, and each channel
// uses a different pair of odd multipliers:
//
//     r5 = kR5[(5x + 3y) & 15]
//     g6 = kG6[(3x + 7y) & 15]
//     b5 = kB5[(7x + 5y) & 15]
//
// Three consequences, all of them load-bearing and all provable by inspection
// rather than by hoping:
//
//   * STEP ONE TEXEL IN U. Every index moves by 5, 3 and 7 mod 16 -- all
//     non-zero -- so every table lookup lands on a DIFFERENT slot, and because
//     the tables are permutations a different slot is a different value. All
//     three channels change. The same argument for a step in V (3, 7, 5).
//     Therefore t00, t10, t01 and t11 differ from one another IN EVERY CHANNEL,
//     which is exactly what uniform texels destroyed.
//
//   * THE U- AND V-GRADIENTS DIFFER PER CHANNEL, because (5,3), (3,7) and
//     (7,5) are three different pairs. So swapping fu and fv changes the
//     answer -- a wrong-weight bilinear is distinguishable from a right one,
//     which it is not for any separable or symmetric pattern.
//
//   * NEIGHBOURING ADDRESSES ARE DIFFERENT COLOURS, which is what makes an
//     address-specific expectation possible at all.
//
// The VALUE RANGES are chosen too, and for a duller reason: the material
// recipes include MODULATE2X and ADD_SAT. If the texels were near full scale,
// those recipes would saturate to 255 for the right texel AND for the wrong
// one, and the separation the tables buy would be thrown away in the combiner.
// So r5 and b5 run over 1..16 and g6 over the 16 odd values 1..31 -- bright
// enough that nothing rounds to zero, dim enough that s0*s1*2 stays in range.
const uint8_t kR5[16] = {5, 12, 1, 16, 8, 3, 14, 10, 6, 15, 2, 11, 7, 13, 4, 9};
const uint8_t kG6[16] = {11, 27, 3, 19, 31, 7, 23, 15, 1, 29, 9, 25, 5, 21, 13, 17};
const uint8_t kB5[16] = {14, 2, 9, 6, 16, 11, 4, 13, 1, 15, 8, 3, 10, 7, 12, 5};

// The CLUT8 index is built the same way, from two permutations of 0..15 that
// become the high and low nibble. Stepping one texel in either axis moves both
// nibble indices (5,7 in u; 3,5 in v -- all non-zero mod 16), so EVERY
// neighbouring texel carries a different palette index. An off-by-one address
// cannot land on the same colour.
const uint8_t kHI[16] = {9, 2, 14, 5, 11, 0, 7, 13, 3, 15, 6, 10, 1, 12, 8, 4};
const uint8_t kLO[16] = {6, 13, 1, 10, 15, 4, 8, 2, 12, 7, 11, 0, 5, 14, 3, 9};

uint16_t rgb565_at(int x, int y) {
  const uint32_t r5 = kR5[(5 * x + 3 * y) & 15];
  const uint32_t g6 = kG6[(3 * x + 7 * y) & 15];
  const uint32_t b5 = kB5[(7 * x + 5 * y) & 15];
  return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

uint8_t clut_index_at(int x, int y) {
  return static_cast<uint8_t>((kHI[(5 * x + 3 * y) & 15] << 4) | kLO[(7 * x + 5 * y) & 15]);
}

// THE PALETTE. It was `0x0841 * (i + 1)`, which is injective but reaches full
// scale and therefore saturates MODULATE2X and ADD_SAT -- the same trap the
// texel ranges above avoid. This is injective for a stated reason instead:
// 7 is invertible mod 15, 11 mod 31 and 5 mod 16, so two indices collide only
// if they agree mod 15 AND mod 31 AND mod 16, i.e. mod 7440 > 256. Every entry
// is non-zero in all three channels, so "a fragment retired a non-zero colour"
// still means something.
//
// THE THREE MODULI ARE 15, 31 AND 16 AND THE LAST ONE WAS EARNED. The first
// version used 15, 31, 15 -- injective, and still not good enough: two indices
// 45 apart agree mod 15 TWICE, so they shared red AND blue and differed only in
// green. The address-separation check below found exactly one such pair
// (indices 194 and 149, texels (51,58) and (51,59)) whose green difference was
// then squashed by DETAIL_LIGHT's triple product, and reported it as one
// neighbour out of 128 that a wrong fetch could have passed on. Pairwise
// different moduli make a two-channel coincidence need agreement mod 240, 465
// or 496 -- none of which a 256-entry palette can reach twice.
uint16_t pal565(int i) {
  const uint32_t r5 = 1u + static_cast<uint32_t>((i * 7) % 15);
  const uint32_t g6 = 1u + static_cast<uint32_t>((i * 11) % 31);
  const uint32_t b5 = 1u + static_cast<uint32_t>((i * 5) % 16);
  return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

// The AUX sheet. Coordinate-DEPENDENT, which the old constant 0x22/0x88
// responder was not: with a constant sheet a wrong aux coordinate produces the
// right colour, so the whole aux coordinate path was unobservable. Same
// permutation-table construction, same argument.
//
// THE VALUES SIT HIGH, in 0x60..0xD0, and that range was also earned. They were
// 0x09..0x75 at first, and DETAIL_LIGHT is `unit_mul(unit_mul(s0, s1), s2)`:
// with a small third sample the product collapses toward zero and two visibly
// different texture samples retire the same colour. A sheet value near 0x20 is
// not a wrong number, it is a number that throws away the very difference this
// fixture exists to detect.
const uint8_t kSTAG[16] = {0x62, 0x9E, 0x77, 0xB3, 0x68, 0xC4, 0x85, 0xA9,
                           0x71, 0xBC, 0x66, 0x93, 0xAF, 0x7C, 0xC0, 0x8A};
const uint8_t kSSTR[16] = {0xA4, 0x6B, 0xC7, 0x82, 0x99, 0x74, 0xB8, 0x60,
                           0xAB, 0x8E, 0x65, 0xD0, 0x79, 0xBF, 0x96, 0x6F};

uint8_t sheet_tag_at(int u, int v) { return kSTAG[(5 * u + 3 * v) & 15]; }
uint8_t sheet_str_at(int u, int v) { return kSSTR[(3 * u + 7 * v) & 15]; }

zref::TextureMemory g_mem;

void build_memory() {
  g_mem.base = kImgBase;
  g_mem.bytes.assign(kImgBytes, 0);
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; ++x) {
      const uint32_t t = static_cast<uint32_t>(y) * 64u + static_cast<uint32_t>(x);
      g_mem.bytes[(kBaseClut - kImgBase) + t] = clut_index_at(x, y);
      const uint16_t h = rgb565_at(x, y);
      g_mem.bytes[(kBaseRgb - kImgBase) + 2 * t] = static_cast<uint8_t>(h & 0xFF);
      g_mem.bytes[(kBaseRgb - kImgBase) + 2 * t + 1] = static_cast<uint8_t>(h >> 8);
    }
  for (int i = 0; i < kPalEntries; ++i) {
    const uint16_t e = pal565(i);
    g_mem.bytes[(kPalBase - kImgBase) + 2 * i] = static_cast<uint8_t>(e & 0xFF);
    g_mem.bytes[(kPalBase - kImgBase) + 2 * i + 1] = static_cast<uint8_t>(e >> 8);
  }
}

// ===========================================================================
// THE MODE RECORDS, BUILT BY THE REFERENCE PACKER
// ===========================================================================
// `zref::Tmu::Mode::pack()` is the ABI. The literals below are asserted against
// it at run time rather than trusted, because a bare literal is exactly how
// this fixture spent its whole life asking for CLUT8/nearest while its comments
// said bilinear.
uint32_t mode_clut8_nearest() {
  zref::Tmu::Mode m;
  m.fmt = zref::Tmu::kClut8;
  m.bilinear = false;
  m.wrap_u = zref::Tmu::kRepeat;
  m.wrap_v = zref::Tmu::kRepeat;
  m.log2w = 6;
  m.log2h = 6;
  m.max_level = 0;
  m.mip_en = false;
  return m.pack();
}

uint32_t mode_rgb565_bilinear() {
  zref::Tmu::Mode m;
  m.fmt = zref::Tmu::kRgb565;
  m.bilinear = true;  // bit 3 -- the bit the old fixture never set
  m.wrap_u = zref::Tmu::kRepeat;
  m.wrap_v = zref::Tmu::kRepeat;
  m.log2w = 6;
  m.log2h = 6;
  m.max_level = 0;
  m.mip_en = false;
  return m.pack();
}

// The three direct-colour formats, nearest or filtered, from the same packer.
// The two named builders above are kept verbatim because their literals are
// asserted in main(); this one exists so the ARGB phases cannot drift from them
// by being hand-packed.
uint32_t mode_direct(uint8_t fmt, bool bilinear) {
  zref::Tmu::Mode m;
  m.fmt = fmt;
  m.bilinear = bilinear;
  m.wrap_u = zref::Tmu::kRepeat;
  m.wrap_v = zref::Tmu::kRepeat;
  m.log2w = 6;
  m.log2h = 6;
  m.max_level = 0;
  m.mip_en = false;
  return m.pack();
}

// The island's own class encoding (zhao_texture_island_top.sv:319-322) and its
// derivation `class_of(fmt, filter)` (:335-350). The fixture computes the class
// the SAME way the planner does, from the mode record, instead of choosing one
// independently -- which is the whole of defect (e).
constexpr uint8_t kClsClut = 0;
constexpr uint8_t kClsNear = 1;
constexpr uint8_t kClsBil = 2;

uint8_t class_of_mode(uint32_t mode) {
  const zref::Tmu::Mode m = zref::Tmu::Mode::unpack(mode);
  const bool clut = (m.fmt == zref::Tmu::kClut8) || (m.fmt == zref::Tmu::kClut4);
  if (clut) return kClsClut;
  return m.bilinear ? kClsBil : kClsNear;
}

// ===========================================================================
// A FRAGMENT, ITS STIMULUS AND ITS EXPECTATION
// ===========================================================================
struct FragSpec {
  uint16_t tag = 0;
  uint32_t depth = 0;
  uint32_t uow = 0, vow = 0;
  int32_t u = 0, v = 0;  // what the planner must see, S15.16 texture units
  uint8_t recipe = 0;
  uint8_t weight = 128;
  uint8_t cls = 0;
  bool aux = false;
  uint32_t wx = 0, wz = 0;  // aux world position, carried in the context word
  int X = 0, Y = 0;         // the texel this fragment must address
  uint8_t FU = 0, FV = 0;   // and the filter fractions it must produce

  // The expectation, computed by the reference before the DUT is run.
  uint32_t want_rgb = 0;
  uint8_t want_a = 0;
  bool want_refused = false;
};

struct PhaseOut {
  std::vector<uint16_t> tags;
  std::vector<uint32_t> rgb;
  std::vector<uint8_t> alpha;
  std::vector<uint8_t> refused;
  std::vector<int> sheet_u, sheet_v;
  uint32_t fills = 0;
  int submitted = 0, retired = 0;
};

// ---------------------------------------------------------------------------
// THE COORDINATE, EXACTLY: WHY THE DEPTHS ARE POWERS OF TWO
// ---------------------------------------------------------------------------
// The planner sees `u` after RCP24 and PERSPUV, not `frag_u_over_w_i`. Rather
// than re-implement that divide in the test -- a second renderer, which section
// 12.4 rules out -- the STIMULUS is chosen so the divide is exact and its
// result is known by inspection:
//
//   rcp24 normalises d to [2^23, 2^24) and returns (r, k) with k = e + 1
//     (zhao_raster_rcp24_svc.sv:118-120, :259). For d = 2^n the mantissa is the
//     pinned saturating case m = 2^23 -> r = 0xFF_FFFF (:224-225, and the same
//     pin in zref::rcp_u24_norm).
//   perspuv forms u = (num * r + (1 << (sh-1))) >> sh with sh = 32 - k
//     (zhao_raster_perspuv_svc.sv:275-278, :476).
//
// With d = 2^16 -> k = 8, sh = 24, and for 0 <= num <= 2^23:
//     (num*(2^24 - 1) + 2^23) >> 24  ==  num + floor((2^23 - num) / 2^24) == num.
// With d = 2^17 the numerator is doubled and the same algebra gives num again;
// with d = 2^18, quadrupled. So THREE different reciprocal exponents run
// through RCP24 and PERSPUV, the multiplier launches for every one of them, and
// the coordinate the planner receives is the one this test names.
//
// This is deliberately NOT a test of the divide -- raster_rcp24_svc_directed
// and raster_perspuv_svc_directed own that. It is the minimum needed to make
// the TEXTURE address knowable, and saying so is the difference between a
// chosen constraint and an accidental one.
const uint32_t kDepths[3] = {0x01'0000u, 0x02'0000u, 0x04'0000u};

// The texel a fragment addresses, and the fraction it lands on inside it.
//
// tu = u << log2w, and the planner subtracts a half texel when filtering
// (tmu_plan:296-297). So for a target texel X and target fraction F:
//     filtered:  tu = (X << 16) + 32768 + (F << 8)  ->  iu0 = X, fu = F
//     nearest :  tu = (X << 16) + 32768             ->  iu0 = X, fu = 0
// and u = tu >> 6 exactly, because every term is a multiple of 64.
int32_t coord_for(int texel, uint8_t frac, bool filtered) {
  const int32_t tu = (static_cast<int32_t>(texel) << 16) + 32768 +
                     (filtered ? (static_cast<int32_t>(frac) << 8) : 0);
  return tu >> 6;
}

// Eight texels per axis, spread across the sheet and including 63 so that the
// +1 tap WRAPS (REPEAT) and the wrap fold is exercised rather than assumed.
const int kXs[8] = {3, 17, 40, 62, 9, 28, 51, 63};
const int kYs[8] = {5, 22, 37, 58, 12, 44, 19, 61};
// Non-zero fractions, and FU != FV for every fragment -- checked below, because
// equal fractions are the one case where swapping them cannot be detected.
const uint8_t kFU[8] = {0x20, 0x60, 0xA0, 0xC0, 0x40, 0xE0, 0x80, 0x30};
const uint8_t kFV[8] = {0x90, 0x30, 0xB0, 0x50, 0xD0, 0x70, 0x10, 0xF0};

// The base colour every fragment carries. Moderate, for the saturation reason
// above.
zref::material::Sample base_sample() {
  zref::material::Sample b;
  b.r = 0x20;
  b.g = 0x40;
  b.b = 0x60;
  b.a = 255;
  return b;
}

// The texture sample the reference says this fragment must fetch.
zref::Tmu::Sample ref_sample(const FragSpec& f, uint32_t mode, uint32_t base) {
  zref::Tmu::Req rq;
  rq.u = f.u;
  rq.v = f.v;
  rq.base = base;
  rq.pal_base = kPalBase;
  rq.mode = mode;
  rq.lod = 0;
  rq.src_id = f.tag;
  return zref::Tmu::sample(rq, g_mem);
}

// The AUX sample the reference says this fragment must receive.
//
// `zref::aux::axis_texel(w, e0, e1)` is (w - e0) * 64 / (e1 - e0) clamped to
// 0..63, and the island wires the envelope to (0, 65536) on both axes
// (island_top:1608-1609), so the sheet coordinate is w >> 10. The RTL forms the
// same quantity as `(wx - x0) << 6` over `x1 - x0` in AUX_PIPE (:229-232).
// The island then presents the sheet's answer as {tag, strength, 0} with alpha
// 255 unless the envelope was degenerate (:1621-1623).
int aux_axis(uint32_t w) {
  const int t = static_cast<int>(w >> 10);
  return t < 0 ? 0 : (t > 63 ? 63 : t);
}

zref::material::Sample ref_aux(const FragSpec& f) {
  const int u = aux_axis(f.wx);
  const int v = aux_axis(f.wz);
  zref::material::Sample s;
  s.r = sheet_tag_at(u, v);
  s.g = sheet_str_at(u, v);
  s.b = 0;
  s.a = 255;
  return s;
}

// The whole expectation for one fragment, through the canonical components and
// nothing else: zref::Tmu::sample for the fetch and decode, the aux law above
// for the third sample when the fragment carries one, and
// zref::material::combine for the material arithmetic.
//
// THE LIMIT THIS COMMENT USED TO RECORD IS GONE, and the record of it stays
// because it names what the new phases are for. It said:
//
//   > `island_top` assigns `fr_tmu_a = 8'hFF` unconditionally for every texture
//   > sample. `zref::Tmu::sample` also returns 255 for CLUT8 and RGB565 -- so
//   > for THESE two modes the reference and the island agree honestly. They
//   > would NOT agree for ARGB1555 or ARGB4444, and the island has no decode for
//   > those at all. Non-opaque alpha therefore stays out of reach.
//
// That is exactly why phases 1 and 2 could not see defect (d): the two formats
// they drive are the two whose reference alpha happens to be 255, so a hardwire
// and a decode are indistinguishable. Phases 5 and 6 drive the two formats
// where they are NOT, which is the whole reason those phases exist rather than
// being more of the same.
//
// Alpha now comes from the island's shared `decode16` and is compared against
// `zref::Tmu::sample` like every other channel -- and, because agreement with
// an oracle is not the same as the value MATTERING, each ARGB phase also
// recomputes its expectation with alpha FORCED to the old 8'hFF and requires
// the two to differ.
void compute_expectation(FragSpec& f, uint32_t mode, uint32_t base) {
  const zref::Tmu::Sample ts = ref_sample(f, mode, base);
  zref::material::Sample tex;
  tex.r = ts.r;
  tex.g = ts.g;
  tex.b = ts.b;
  tex.a = ts.a;

  zref::material::Sample s[3] = {tex, tex, f.aux ? ref_aux(f) : tex};
  zref::material::Ledger led{};
  const zref::material::Out o =
      zref::material::combine(f.recipe, f.weight, s, /*count=*/3, base_sample(), f.tag, &led);
  f.want_rgb = (static_cast<uint32_t>(o.r) << 16) | (static_cast<uint32_t>(o.g) << 8) | o.b;
  f.want_a = o.a;
  f.want_refused = o.refused;
}

// THE SAME EXPECTATION WITH ONE INPUT REPLACED. Two counterfactuals, and each
// answers a question that "it matches the reference" does not:
//
//   * `expect_forced_alpha` recomputes the fragment with the texture sample's
//     alpha replaced by the old hardwired 255. If the retired value is the same
//     either way, the alpha decode is unobservable at this boundary and an
//     agreeing oracle proves nothing.
//   * `want_as_rgb565` recomputes it with the FORMAT replaced by RGB565 at the
//     same addresses and the same fractions. That is precisely what the island
//     shipped before defect (d) -- `chan8()` had no format input -- so a
//     difference here is the measure of what the repair changed.
void expect_forced_alpha(const FragSpec& f, uint32_t mode, uint32_t base, uint8_t force_a,
                         uint32_t* rgb, uint8_t* a) {
  const zref::Tmu::Sample ts = ref_sample(f, mode, base);
  zref::material::Sample tex;
  tex.r = ts.r;
  tex.g = ts.g;
  tex.b = ts.b;
  tex.a = force_a;
  zref::material::Sample s[3] = {tex, tex, f.aux ? ref_aux(f) : tex};
  zref::material::Ledger led{};
  const zref::material::Out o =
      zref::material::combine(f.recipe, f.weight, s, /*count=*/3, base_sample(), f.tag, &led);
  *rgb = (static_cast<uint32_t>(o.r) << 16) | (static_cast<uint32_t>(o.g) << 8) | o.b;
  *a = o.a;
}

uint32_t want_as_rgb565(const FragSpec& f, uint32_t base, bool filtered) {
  FragSpec g = f;
  compute_expectation(g, mode_direct(zref::Tmu::kRgb565, filtered), base);
  return g.want_rgb;
}

// One phase's stimulus. Phases 1 and 2 were written out longhand before there
// were more than two of them and are left alone; every phase added for defect
// (d) is built here, so a change to the stimulus cannot apply to one format and
// not another.
void build_frags(std::vector<FragSpec>& v, uint16_t tag_base, uint32_t mode, uint32_t base,
                 bool filtered) {
  for (int i = 0; i < static_cast<int>(v.size()); ++i) {
    FragSpec& f = v[i];
    f.tag = static_cast<uint16_t>(tag_base + i);
    f.X = kXs[i & 7];
    f.Y = kYs[(i >> 3) & 7];
    f.FU = filtered ? kFU[i & 7] : 0;
    f.FV = filtered ? kFV[(i * 3 + 1) & 7] : 0;
    f.u = coord_for(f.X, f.FU, filtered);
    f.v = coord_for(f.Y, f.FV, filtered);
    f.depth = kDepths[i % 3];
    const uint32_t scale = f.depth >> 16;
    f.uow = static_cast<uint32_t>(f.u) * scale;
    f.vow = static_cast<uint32_t>(f.v) * scale;
    f.recipe = static_cast<uint8_t>(i % 8);
    f.weight = 128;
    f.cls = class_of_mode(mode);
    f.aux = (i % 3) == 0;
    f.wx = f.tag;
    f.wz = static_cast<uint32_t>(0x0400 + i * 0x0700);
    compute_expectation(f, mode, base);
  }
}

// ===========================================================================
// THE PHASE RUNNER
// ===========================================================================
// One mode, one class, one texture base, drained to completion before the next
// phase starts. The fill responder now serves the ACTUAL image at the ACTUAL
// requested address; the sheet responder answers as a function of the
// coordinate it was handed. Both are memories, not oracles -- neither is
// allowed to decide what the island SHOULD produce.
struct FillModel {
  static constexpr int kBeats = 8;  // LINE_BYTES / 2
  uint32_t addr = 0;
  int delay = 0;
  int beats_left = 0;
  uint32_t served = 0;
};

void run_phase(Dut& d, uint32_t mode, uint32_t base, const std::vector<FragSpec>& frags,
               PhaseOut& out) {
  FillModel mem;
  const int kN = static_cast<int>(frags.size());
  int submitted = 0, retired = 0;
  int gap = 0;

  d.out_ready_i = 1;

  for (int cyc = 0; cyc < 400000; ++cyc) {
    // A DELIBERATE GAP after every fourth fragment, so the poison below lands
    // while earlier fragments are still in flight rather than only during the
    // final drain.
    const bool present = (gap == 0) && (submitted < kN);
    if (gap > 0) --gap;

    // The binding is a global port and is held for the whole phase. It is
    // written every cycle, INCLUDING the poison cycles, precisely because it is
    // not a per-fragment attribute: changing it mid-stream would be the late
    // ingress read this phase structure exists to avoid.
    d.bind_base_i = base;
    d.bind_mode_i = mode;

    if (present) {
      const FragSpec& f = frags[submitted];
      d.frag_valid_i = 1;
      d.frag_depth_i = f.depth;
      d.frag_u_over_w_i = f.uow;
      d.frag_v_over_w_i = f.vow;
      d.frag_sample_count_i = 3;
      d.frag_binding_i = 1;
      d.frag_lod_i = 0;
      d.frag_recipe_i = f.recipe;
      d.frag_weight_i = f.weight;
      // The context word carries the caller's tag in its low 16 bits and the
      // aux world position in the two halves AUX_PIPE reads
      // (island_top:1605-1606: wx = ctx[31:0], wz = ctx[63:32]).
      d.frag_ctx_i = (static_cast<uint64_t>(f.wz) << 32) | static_cast<uint64_t>(f.wx);
      d.frag_aux_i = f.aux ? 1 : 0;
      d.frag_class_i = f.cls;
      d.frag_base_rgb_i = 0x204060u;
      d.frag_base_a_i = 255;
      d.frag_pal_slot_i = 0;
      d.frag_pal_gen_i = 1;
    } else {
      // ================ INVALID-INPUT POISON ================================
      // Owner recovery architecture v2, priority 3: "A single-fragment
      // invalid-input-poison test should precede elaborate RAM or bit-OR
      // theories."
      //
      // This is the BEHAVIOURAL counterpart to
      // tools/rtl/check_ingress_capture.py. That gate reads the source and
      // proves nobody WROTE a late read; this proves nobody PERFORMS one,
      // which also covers reads its contract does not know to look for.
      //
      // Poison is applied only while `frag_valid_i` is LOW, so no accept can
      // occur on these cycles and the values are pure don't-care. A consumer
      // that samples its attributes late reads THESE instead of its own
      // fragment's, and every exact colour in the phase moves.
      //
      // It ALTERNATES rather than holding a constant. A constant poison makes
      // the recipe constant too, and a wrong-but-constant recipe still yields
      // a tidy-looking histogram. `frag_pal_slot_i` is never 0, so any late
      // read of the palette binding lands on an unloaded slot and goes stale.
      const uint32_t k = static_cast<uint32_t>(cyc);
      d.frag_valid_i = 0;
      d.frag_u_over_w_i = 0xDEAD0000u ^ (k * 2654435761u);
      d.frag_v_over_w_i = 0xBEEF0000u ^ (k * 40503u);
      d.frag_depth_i = 0x00FF0000u ^ (k * 97u);
      d.frag_recipe_i = static_cast<uint8_t>((k * 5u + 3u) & 7u);
      d.frag_weight_i = static_cast<uint8_t>(k * 31u);
      d.frag_sample_count_i = static_cast<uint8_t>(1 + (k & 1u));
      d.frag_class_i = static_cast<uint8_t>((k & 1u) ? 0 : 2);
      d.frag_ctx_i = static_cast<uint64_t>(0xF00DF00Du) ^ k;
      d.frag_aux_i = (k & 1u) != 0;
      d.frag_binding_i = static_cast<uint8_t>(0x50 + (k & 7u));
      d.frag_lod_i = static_cast<uint8_t>(k & 3u);
      d.frag_base_rgb_i = 0xFF00FFu ^ (k << 3);
      d.frag_base_a_i = static_cast<uint8_t>(k * 17u);
      d.frag_pal_slot_i = static_cast<uint8_t>(1 + (k & 1u));  // never slot 0
      d.frag_pal_gen_i = static_cast<uint8_t>(0x20 + (k & 15u));
    }

    // ---- the aux sheet, as a COORDINATE-DEPENDENT memory --------------------
    // ECHO THE REQUEST'S TOKEN. AUX_PIPE matches a sheet response to the
    // request that asked for it; answering with a constant 0 makes every aux
    // result carry a wrong identity, FRAGROB rejects them all, and because it
    // retires in allocation order a single aux fragment at the head stalls the
    // whole island -- 48 samples fetched and 0 fragments out.
    d.sheet_rvalid_i = d.sheet_valid_o;
    if (d.sheet_valid_o) {
      out.sheet_u.push_back(d.sheet_u_o);
      out.sheet_v.push_back(d.sheet_v_o);
    }
    d.sheet_tag_i = sheet_tag_at(d.sheet_u_o, d.sheet_v_o);
    d.sheet_str_i = sheet_str_at(d.sheet_u_o, d.sheet_v_o);
    d.sheet_rtok_i = d.sheet_tok_o;

    // ---- memory behind the cache -------------------------------------------
    // IT MUST STREAM A WHOLE LINE. `zhao_texture_cache_pipe` counts beats and
    // only marks the line valid on the last one:
    //
    //     if (fb_beat_r == BEAT_W'(HW_PL - 1)) ... fb_busy_r <= 1'b0;
    //
    // with `HW_PL = LINE_BYTES / 2 = 8`. The first version of this model
    // answered each fill with ONE halfword, so the cache waited forever for
    // beats 2..8, back-pressured the planner, and nothing ever reached the
    // dispatcher -- "cache miss 1, dispatch 0".
    //
    // THE DATA NOW COMES FROM THE ADDRESS. This is the single most important
    // change in the W10 rebuild: `mem.addr` used to be captured and never read,
    // and every beat answered 0x8586. A misrouted fill was indistinguishable
    // from a correct one.
    if (d.fill_valid_o && d.fill_ready_i && mem.beats_left == 0 && mem.delay == 0) {
      mem.addr = d.fill_addr_o;
      mem.delay = 2;  // a two-cycle memory, so the cache must actually wait
    }
    d.fill_data_valid_i = 0;
    if (mem.delay > 0) {
      if (--mem.delay == 0) mem.beats_left = FillModel::kBeats;
    } else if (mem.beats_left > 0) {
      const int beat = FillModel::kBeats - mem.beats_left;
      d.fill_data_valid_i = 1;
      d.fill_data_i = g_mem.halfword(mem.addr + 2u * static_cast<uint32_t>(beat));
      --mem.beats_left;
      ++mem.served;
    }

    d.eval();
    const bool accepted = d.frag_valid_i && d.frag_ready_o;
    if (d.out_valid_o && d.out_ready_i) {
      out.tags.push_back(d.out_tag_o);
      out.rgb.push_back(d.out_rgb_o);
      out.alpha.push_back(d.out_a_o);
      out.refused.push_back(d.out_refused_o ? 1 : 0);
      ++retired;
    }
    tick(d);
    if (accepted) {
      ++submitted;
      if ((submitted % 4) == 0) gap = 3;
    }
    if (submitted >= kN && retired >= kN) break;
  }

  out.fills = mem.served;
  out.submitted = submitted;
  out.retired = retired;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  build_memory();

  // ==========================================================================
  // THE MODE RECORDS ARE REAL RECORDS, AND THEY ARE CHECKED
  // ==========================================================================
  const uint32_t kModeClut = mode_clut8_nearest();
  const uint32_t kModeBil = mode_rgb565_bilinear();
  std::printf("  modes: CLUT8/nearest 0x%08X, RGB565/bilinear 0x%08X\n", kModeClut, kModeBil);

  check(kModeClut == 0x0000'6600u,
        "the CLUT8/nearest mode record packs to the word the RTL decodes as "
        "fmt=CLUT8, filter=0, 64x64 -- m_fmt[2:0], m_filter[3], m_log2w[11:8], "
        "m_log2h[15:12]",
        0x6600, kModeClut);
  check(kModeBil == 0x0000'6609u,
        "and the RGB565/bilinear record packs to 0x6609 -- fmt=RGB565 AND THE "
        "FILTER BIT SET. The old fixture asked for 0x6600 and called it "
        "bilinear; that word cannot enable a filter at all, because "
        "`filter_eff = m_filter && !is_clut` is zero for every CLUT",
        0x6609, kModeBil);
  check(class_of_mode(kModeClut) == kClsClut,
        "the CLUT mode derives sample class CLUT, the same function the island "
        "computes at :335-350",
        kClsClut, class_of_mode(kModeClut));
  check(class_of_mode(kModeBil) == kClsBil, "and the RGB565+filter mode derives class BILINEAR",
        kClsBil, class_of_mode(kModeBil));

  // And the reference agrees about what those words MEAN before a single
  // fragment is driven: four lanes and a live filter for one, one lane and no
  // filter for the other. This is the claim the old fixture made in a comment.
  {
    zref::Tmu::Req rq;
    rq.u = coord_for(10, 0x40, true);
    rq.v = coord_for(20, 0x90, true);
    rq.base = kBaseRgb;
    rq.pal_base = kPalBase;
    rq.mode = kModeBil;
    const zref::Tmu::Plan pb = zref::Tmu::plan(rq);
    rq.mode = kModeClut;
    rq.base = kBaseClut;
    rq.u = coord_for(10, 0, false);
    rq.v = coord_for(20, 0, false);
    const zref::Tmu::Plan pc = zref::Tmu::plan(rq);

    check(pb.bilinear && pb.lanes == 4 && !pb.mode_error,
          "the bilinear mode plans FOUR cache lanes with the filter live and no "
          "mode error -- the RTL's `acc_en_o = t3_filt ? 4'b1111 : 4'b0001`",
          4, pb.lanes);
    check(!pc.bilinear && pc.lanes == 1 && !pc.mode_error,
          "and the CLUT mode plans exactly ONE lane, nearest", 1, pc.lanes);
    check(pb.fu == 0x40 && pb.fv == 0x90,
          "and the filtered plan carries the fractions this fixture asked for, "
          "so the coordinate construction lands where it says it does",
          0x4090, (pb.fu << 8) | pb.fv);
    check(pc.fu == 0 && pc.fv == 0, "while the nearest plan carries none", 0,
          (pc.fu << 8) | pc.fv);
  }

  // ==========================================================================
  // THE PALETTE AND THE TEXTURE MUST THEMSELVES BE CAPABLE OF DISTINGUISHING
  // ==========================================================================
  // A fixture whose memory contains repeats cannot detect a wrong address,
  // however exact its comparison looks. So the memory's own separating property
  // is asserted rather than argued.
  {
    int pal_collisions = 0;
    for (int i = 0; i < kPalEntries; ++i)
      for (int j = i + 1; j < kPalEntries; ++j)
        if (pal565(i) == pal565(j)) ++pal_collisions;
    check(pal_collisions == 0,
          "no two palette indices share a colour, so a wrong CLUT index is "
          "always a wrong colour",
          0, pal_collisions);

    int idx_nb = 0, tex_nb = 0;
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x) {
        const uint8_t c = clut_index_at(x, y);
        if (c == clut_index_at((x + 1) & 63, y)) ++idx_nb;
        if (c == clut_index_at(x, (y + 1) & 63)) ++idx_nb;
        const uint16_t t = rgb565_at(x, y);
        if (t == rgb565_at((x + 1) & 63, y)) ++tex_nb;
        if (t == rgb565_at(x, (y + 1) & 63)) ++tex_nb;
      }
    check(idx_nb == 0,
          "NO texel in the CLUT sheet equals its u- or v-neighbour -- the "
          "permutation-table construction guarantees it, and this is the check "
          "that says so out loud",
          0, idx_nb);
    check(tex_nb == 0, "and no texel in the RGB565 sheet equals its neighbour either", 0, tex_nb);
  }

  Dut d;
  d.rst_n = 0;
  d.frag_valid_i = 0;
  d.out_ready_i = 1;
  d.fill_ready_i = 1;
  d.fill_data_valid_i = 0;
  d.sheet_ready_i = 1;
  d.sheet_rvalid_i = 0;
  d.pal_ld_valid_i = 0;
  d.bind_base_i = kBaseClut;
  d.bind_mode_i = kModeClut;
  for (int i = 0; i < 8; ++i) tick(d);
  d.rst_n = 1;

  // ---- load a palette, FOLLOWING THE LOAD PROTOCOL ------------------------
  // This loop used to send op == 1 (LD_WRITE) sixty-four times and nothing
  // else. PALETTE_RES requires BEGIN -> every entry -> END:
  //
  //   * LD_WRITE is ignored unless `loading_r`, which only LD_BEGIN sets, so
  //     not one of those writes landed;
  //   * LD_END makes the slot resident only if ALL `PAL_ENTRIES` arrived and
  //     the CRC is good, so 64 of 256 would not have sufficed either.
  //
  // The palette was therefore never resident and `gen_r[0]` stayed 0 against a
  // lookup generation of 1. MEASURED: 96 lookups, 96 STALE, 0 cold -- every
  // CLUT fragment retired black while the lookup counter moved and the path
  // looked busy and healthy.
  //
  // Worth recording because the first diagnosis of that blackness asserted
  // "it is NOT the palette being unloaded", on the grounds that the entry
  // VALUES written here are all non-zero. That checked the data and not the
  // protocol -- the values were irrelevant because no write was accepted. A
  // negative claim needs the same evidence as a positive one.
  //
  // THE VALUES ARE THE SAME ONES `g_mem` HOLDS at kPalBase, so the reference's
  // CLUT lookup and the island's are the same table by construction rather
  // than by two transcriptions agreeing.
  d.pal_ld_valid_i = 1;
  d.pal_ld_slot_i = 0;
  d.pal_ld_gen_i = 1;
  d.pal_ld_crc_ok_i = 1;
  d.pal_ld_op_i = 0;  // LD_BEGIN
  d.pal_ld_idx_i = 0;
  tick(d);
  for (int i = 0; i < kPalEntries; ++i) {
    d.pal_ld_op_i = 1;  // LD_WRITE
    d.pal_ld_idx_i = static_cast<uint16_t>(i);
    d.pal_ld_rgb565_i = pal565(i);
    tick(d);
  }
  d.pal_ld_op_i = 2;  // LD_END
  tick(d);
  d.pal_ld_valid_i = 0;
  tick(d);

  // ==========================================================================
  // PHASE 1 -- CLUT8, NEAREST, 32 FRAGMENTS
  // ==========================================================================
  const int kPhaseN = 32;
  std::vector<FragSpec> p1(kPhaseN);
  for (int i = 0; i < kPhaseN; ++i) {
    FragSpec& f = p1[i];
    f.tag = static_cast<uint16_t>(0x1000 + i);
    f.X = kXs[i & 7];
    f.Y = kYs[(i >> 3) & 7];
    f.FU = 0;
    f.FV = 0;
    f.u = coord_for(f.X, 0, /*filtered=*/false);
    f.v = coord_for(f.Y, 0, /*filtered=*/false);
    f.depth = kDepths[i % 3];
    const uint32_t scale = f.depth >> 16;
    f.uow = static_cast<uint32_t>(f.u) * scale;
    f.vow = static_cast<uint32_t>(f.v) * scale;
    // Recipes cycle so the combiner's bypass, product and continuation paths
    // are all exercised inside the composition.
    f.recipe = static_cast<uint8_t>(i % 8);
    f.weight = 128;
    f.cls = class_of_mode(kModeClut);
    f.aux = (i % 3) == 0;
    // The context word: the caller's tag in the low 16 bits, which is also the
    // aux world X, and an independent world Z above it. Both stay inside the
    // envelope so the sheet coordinate sweeps rather than sitting still.
    f.wx = f.tag;
    f.wz = static_cast<uint32_t>(0x0400 + i * 0x0700);
    compute_expectation(f, kModeClut, kBaseClut);
  }

  PhaseOut o1;
  run_phase(d, kModeClut, kBaseClut, p1, o1);

  std::printf(
      "  PHASE 1 (CLUT8/nearest): submitted %d, retired %d, fills %u\n"
      "    rcp %u | persp %u | plan %u | cache hit %u miss %u | dispatch %u\n"
      "    bilerp %u | palette %u | mosaic %u | aux %u | fragrob %u | id errors %u\n",
      o1.submitted, o1.retired, o1.fills, d.cnt_rcp_completed_o, d.cnt_persp_fragments_o,
      d.cnt_plan_accepted_o, d.cnt_cache_hits_o, d.cnt_cache_misses_o, d.cnt_dispatch_accepted_o,
      d.cnt_bilerp_jobs_o, d.cnt_palette_lookups_o, d.cnt_mosaic_samples_o, d.cnt_aux_accepted_o,
      d.cnt_fragments_o, d.cnt_fragrob_id_errors_o);

  const uint32_t bilerp_after_p1 = d.cnt_bilerp_jobs_o;
  const uint32_t palette_after_p1 = d.cnt_palette_lookups_o;

  check(o1.retired == kPhaseN, "phase 1 retired every fragment it submitted", kPhaseN, o1.retired);
  check(d.err_class_mismatch_o == 0,
        "PHASE 1 DROVE A MODE AND A CLASS THAT AGREE: the planner's derived "
        "class equals the one carried in the source id on every transaction. "
        "The old fixture drove 0x6600 with half its fragments tagged BILINEAR "
        "and this counter would have read 96",
        0, static_cast<long>(d.err_class_mismatch_o));
  check(d.err_plan_mode_o == 0,
        "and the planner never raised its own mode error -- no reserved bit, no "
        "filtered palette, no format above ARGB4444",
        0, static_cast<int>(d.err_plan_mode_o));
  check(bilerp_after_p1 == 0,
        "AND THE BILINEAR LANE RAN NOT ONE JOB. A CLUT request is nearest by "
        "law (`filter_eff = m_filter && !is_clut`), so a single bilerp job in "
        "this phase would mean the island filtered a palette index",
        0, static_cast<long>(bilerp_after_p1));
  check(palette_after_p1 == static_cast<uint32_t>(kPhaseN * 3),
        "and every CLUT sample performed its palette lookup", kPhaseN * 3,
        static_cast<long>(palette_after_p1));
  check(d.cnt_palette_stale_o == 0,
        "every lookup RESOLVES -- no lookup is answered stale, which is the "
        "miss indication that used to be mistaken for a working path",
        0, static_cast<int>(d.cnt_palette_stale_o));
  check(d.cnt_palette_cold_o == 0, "and none is answered cold", 0,
        static_cast<int>(d.cnt_palette_cold_o));

  // ---- PHASE 1: THE EXACT, ADDRESS-SPECIFIC COLOUR ------------------------
  // The old check accepted either of two palette colours derived from one
  // constant halfword. This one computes THE colour, from THIS fragment's own
  // planned address, and compares it.
  {
    std::vector<int> byidx(kPhaseN, -1);
    for (std::size_t i = 0; i < o1.tags.size(); ++i) {
      const int ix = static_cast<int>(o1.tags[i]) - 0x1000;
      if (ix >= 0 && ix < kPhaseN) byidx[ix] = static_cast<int>(i);
    }
    int mismatched = 0, missing = 0, checked = 0, alpha_bad = 0;
    for (int i = 0; i < kPhaseN; ++i) {
      if (byidx[i] < 0) {
        ++missing;
        continue;
      }
      ++checked;
      const uint32_t got = o1.rgb[byidx[i]];
      if (got != p1[i].want_rgb) {
        if (mismatched < 4)
          std::printf(
              "    P1 frag %d recipe %u texel(%d,%d) aux=%d: got 0x%06X, want 0x%06X\n", i,
              p1[i].recipe, p1[i].X, p1[i].Y, p1[i].aux ? 1 : 0, got, p1[i].want_rgb);
        ++mismatched;
      }
      if (o1.alpha[byidx[i]] != p1[i].want_a) ++alpha_bad;
    }
    std::printf("    P1 exact colour: checked %d, mismatched %d, missing %d, alpha wrong %d\n",
                checked, mismatched, missing, alpha_bad);
    check(checked == kPhaseN, "every phase-1 fragment was compared", kPhaseN, checked);
    check(mismatched == 0,
          "every CLUT fragment retires EXACTLY the colour zref::Tmu::sample "
          "fetches from ITS OWN planned address and zref::material::combine "
          "makes of it -- address, byte select, palette lookup, 565-to-888 "
          "replication, the AUX third sample and the material arithmetic, all "
          "exact and all address-specific",
          0, mismatched);
    check(alpha_bad == 0, "and its alpha matches the reference too", 0, alpha_bad);

    // ---- PROOF THAT THE ADDRESS MATTERED --------------------------------
    // For every fragment, the expectation is recomputed at the four
    // neighbouring texels. If any of them produced the SAME retired colour, a
    // wrong fetch would have passed -- so this is the property that makes the
    // check above address-specific rather than merely exact.
    int weak = 0;
    for (int i = 0; i < kPhaseN; ++i) {
      static const int dx[4] = {1, -1, 0, 0};
      static const int dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; ++k) {
        FragSpec g = p1[i];
        g.X = (p1[i].X + dx[k] + 64) & 63;
        g.Y = (p1[i].Y + dy[k] + 64) & 63;
        g.u = coord_for(g.X, 0, false);
        g.v = coord_for(g.Y, 0, false);
        compute_expectation(g, kModeClut, kBaseClut);
        if (g.want_rgb == p1[i].want_rgb) ++weak;
      }
    }
    std::printf("    P1 address separation: %d of %d neighbours indistinguishable\n", weak,
                kPhaseN * 4);
    check(weak == 0,
          "and a sample fetched from ANY of the four neighbouring texel "
          "addresses would have produced a DIFFERENT retired colour, so the "
          "check above could not have passed on a wrong address",
          0, weak);
  }

  // ==========================================================================
  // PHASE 2 -- RGB565, GENUINELY BILINEAR, 32 FRAGMENTS
  // ==========================================================================
  std::vector<FragSpec> p2(kPhaseN);
  for (int i = 0; i < kPhaseN; ++i) {
    FragSpec& f = p2[i];
    f.tag = static_cast<uint16_t>(0x2000 + i);
    f.X = kXs[i & 7];
    f.Y = kYs[(i >> 3) & 7];
    f.FU = kFU[i & 7];
    f.FV = kFV[(i * 3 + 1) & 7];
    f.u = coord_for(f.X, f.FU, /*filtered=*/true);
    f.v = coord_for(f.Y, f.FV, /*filtered=*/true);
    f.depth = kDepths[i % 3];
    const uint32_t scale = f.depth >> 16;
    f.uow = static_cast<uint32_t>(f.u) * scale;
    f.vow = static_cast<uint32_t>(f.v) * scale;
    f.recipe = static_cast<uint8_t>(i % 8);
    f.weight = 128;
    f.cls = class_of_mode(kModeBil);
    f.aux = (i % 3) == 0;
    f.wx = f.tag;
    f.wz = static_cast<uint32_t>(0x0300 + i * 0x0680);
    compute_expectation(f, kModeBil, kBaseRgb);
  }

  // THE FRACTIONS ARE DISTINCT, PER FRAGMENT. Equal fu and fv is the one case
  // where swapping them cannot be detected, and a fixture that silently drifted
  // into it would lose the wrong-weight property without any check going red.
  {
    int equal_fr = 0, zero_fr = 0;
    for (int i = 0; i < kPhaseN; ++i) {
      if (p2[i].FU == p2[i].FV) ++equal_fr;
      if (p2[i].FU == 0 || p2[i].FV == 0) ++zero_fr;
    }
    check(equal_fr == 0, "no bilinear fragment has fu == fv, so a swapped-weight filter is visible",
          0, equal_fr);
    check(zero_fr == 0,
          "and none has a zero fraction, so no fragment degenerates to the "
          "nearest tap -- which is what 0x6600 made EVERY fragment do",
          0, zero_fr);
  }

  PhaseOut o2;
  run_phase(d, kModeBil, kBaseRgb, p2, o2);

  std::printf(
      "  PHASE 2 (RGB565/bilinear): submitted %d, retired %d, fills %u\n"
      "    cache hit %u miss %u | dispatch %u | bilerp %u (+%u) | palette %u (+%u)\n",
      o2.submitted, o2.retired, o2.fills, d.cnt_cache_hits_o, d.cnt_cache_misses_o,
      d.cnt_dispatch_accepted_o, d.cnt_bilerp_jobs_o, d.cnt_bilerp_jobs_o - bilerp_after_p1,
      d.cnt_palette_lookups_o, d.cnt_palette_lookups_o - palette_after_p1);

  check(o2.retired == kPhaseN, "phase 2 retired every fragment it submitted", kPhaseN, o2.retired);
  check(d.err_class_mismatch_o == 0,
        "phase 2's mode and class agree as well -- an RGB565 request with the "
        "filter bit set derives class BILINEAR, which is what its fragments "
        "carry",
        0, static_cast<long>(d.err_class_mismatch_o));
  check(d.err_plan_mode_o == 0, "and the planner raised no mode error on the filtered request", 0,
        static_cast<int>(d.err_plan_mode_o));
  check(d.cnt_palette_lookups_o == palette_after_p1,
        "THE PALETTE WAS NOT CONSULTED ONCE in the RGB565 phase -- a direct "
        "colour has no index, and a lookup here would mean the format decode "
        "went to the wrong consumer",
        static_cast<long>(palette_after_p1), static_cast<long>(d.cnt_palette_lookups_o));
  // FOUR, NOT THREE, AND THE CHANGE IS DEFECT (d). Alpha became a filtered
  // channel when the shared decode landed, because with ARGB4444 the four taps
  // carry four different alphas and tap 0's alpha is not the sample's. This
  // number was 288 before that and is 384 now; it is written as `4` rather
  // than edited to `384` so the reason stays attached to the arithmetic.
  check(d.cnt_bilerp_jobs_o - bilerp_after_p1 == static_cast<uint32_t>(kPhaseN * 3 * 4),
        "and the filter ran FOUR JOBS PER SAMPLE -- R, G, B and A sequenced "
        "independently -- for all 96 samples. This is the number that proves "
        "`filter_eff` came out of the mode word: it was ZERO for every "
        "fragment while the fixture asked for 0x6600",
        kPhaseN * 3 * 4, static_cast<long>(d.cnt_bilerp_jobs_o - bilerp_after_p1));
  check(d.err_bil_chan_o == 0,
        "and the lane retired R, G, B, A in order every time, so no sample's "
        "red was paired with another's blue",
        0, static_cast<int>(d.err_bil_chan_o));

  // ---- PHASE 2: THE EXACT FILTERED COLOUR, AND THE THREE-WAY SEPARATION ----
  {
    std::vector<int> byidx(kPhaseN, -1);
    for (std::size_t i = 0; i < o2.tags.size(); ++i) {
      const int ix = static_cast<int>(o2.tags[i]) - 0x2000;
      if (ix >= 0 && ix < kPhaseN) byidx[ix] = static_cast<int>(i);
    }
    int mismatched = 0, checked = 0, alpha_bad = 0, grey = 0;
    for (int i = 0; i < kPhaseN; ++i) {
      if (byidx[i] < 0) continue;
      ++checked;
      const uint32_t got = o2.rgb[byidx[i]];
      if (got != p2[i].want_rgb) {
        if (mismatched < 4)
          std::printf(
              "    P2 frag %d recipe %u texel(%d,%d) fu=%02X fv=%02X aux=%d: got 0x%06X, "
              "want 0x%06X\n",
              i, p2[i].recipe, p2[i].X, p2[i].Y, p2[i].FU, p2[i].FV, p2[i].aux ? 1 : 0, got,
              p2[i].want_rgb);
        ++mismatched;
      }
      if (o2.alpha[byidx[i]] != p2[i].want_a) ++alpha_bad;
      const uint8_t r = static_cast<uint8_t>(got >> 16), g = static_cast<uint8_t>(got >> 8),
                    b = static_cast<uint8_t>(got);
      if (r == g && g == b) ++grey;
    }
    std::printf("    P2 exact colour: checked %d, mismatched %d, alpha wrong %d, grey %d\n",
                checked, mismatched, alpha_bad, grey);
    check(checked == kPhaseN, "every phase-2 fragment was compared", kPhaseN, checked);
    check(mismatched == 0,
          "every BILINEAR fragment retires EXACTLY what zref::Tmu::sample "
          "computes from its four planned tap addresses at its own fractions, "
          "put through zref::material::combine -- four distinct texels, "
          "per-channel decode, the filter weights and the material arithmetic",
          0, mismatched);
    check(alpha_bad == 0, "and its alpha matches", 0, alpha_bad);
    check(grey == 0,
          "and NONE of them is grey -- before three-channel sequencing the "
          "island filtered one channel and replicated it, so every "
          "direct-colour fragment had r == g == b",
          0, grey);

    // =====================================================================
    // THE SEPARATION PROPERTY -- THE WHOLE POINT OF THE REBUILT TEXELS
    // =====================================================================
    // For every fragment, four candidate answers are computed from the SAME
    // four taps:
    //
    //   NEAREST        tap 0 alone, which is what the old 0x6600 fixture
    //                  actually measured while calling itself bilinear;
    //   CORRECT        bilerp(t00, t10, t01, t11, fu, fv);
    //   SWAPPED        the same taps with fu and fv exchanged -- a plausible
    //                  wrong-weight filter;
    //   TRANSPOSED     the taps permuted (t01, t00, t11, t10) -- a plausible
    //                  wrong lane order.
    //
    // All four must differ. If they do not, the fixture cannot tell a working
    // filter from a broken one, and the exact check above is worth nothing --
    // which is precisely the state this file was in with uniform texels.
    int no_sep_nearest = 0, no_sep_swap = 0, no_sep_perm = 0;
    for (int i = 0; i < kPhaseN; ++i) {
      zref::Tmu::Req rq;
      rq.u = p2[i].u;
      rq.v = p2[i].v;
      rq.base = kBaseRgb;
      rq.pal_base = kPalBase;
      rq.mode = kModeBil;
      const zref::Tmu::Plan pl = zref::Tmu::plan(rq);

      uint8_t cr[4], cg[4], cb[4];
      for (int k = 0; k < 4; ++k) {
        const uint16_t h = g_mem.halfword(pl.addr[k]);
        const uint32_t r5 = (h >> 11) & 0x1F, g6 = (h >> 5) & 0x3F, b5 = h & 0x1F;
        cr[k] = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
        cg[k] = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
        cb[k] = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
      }
      const uint8_t cor[3] = {zref::Tmu::bilerp(cr[0], cr[1], cr[2], cr[3], pl.fu, pl.fv),
                              zref::Tmu::bilerp(cg[0], cg[1], cg[2], cg[3], pl.fu, pl.fv),
                              zref::Tmu::bilerp(cb[0], cb[1], cb[2], cb[3], pl.fu, pl.fv)};
      const uint8_t nea[3] = {cr[0], cg[0], cb[0]};
      const uint8_t swp[3] = {zref::Tmu::bilerp(cr[0], cr[1], cr[2], cr[3], pl.fv, pl.fu),
                              zref::Tmu::bilerp(cg[0], cg[1], cg[2], cg[3], pl.fv, pl.fu),
                              zref::Tmu::bilerp(cb[0], cb[1], cb[2], cb[3], pl.fv, pl.fu)};
      const uint8_t prm[3] = {zref::Tmu::bilerp(cr[2], cr[3], cr[0], cr[1], pl.fu, pl.fv),
                              zref::Tmu::bilerp(cg[2], cg[3], cg[0], cg[1], pl.fu, pl.fv),
                              zref::Tmu::bilerp(cb[2], cb[3], cb[0], cb[1], pl.fu, pl.fv)};
      auto same = [](const uint8_t* a, const uint8_t* b) {
        return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
      };
      if (same(cor, nea)) ++no_sep_nearest;
      if (same(cor, swp)) ++no_sep_swap;
      if (same(cor, prm)) ++no_sep_perm;
    }
    std::printf(
        "    P2 separation: nearest-indistinguishable %d, swapped-fractions %d, "
        "permuted-taps %d (of %d)\n",
        no_sep_nearest, no_sep_swap, no_sep_perm, kPhaseN);
    check(no_sep_nearest == 0,
          "for EVERY bilinear fragment the correct filtered value differs from "
          "tap 0 -- so a filter that returned nearest would fail. With uniform "
          "texels this number was 32 of 32 and the old check could not fail",
          0, no_sep_nearest);
    check(no_sep_swap == 0,
          "and differs from the same taps with fu and fv exchanged -- a "
          "wrong-weight filter is detectable, which needs the per-channel "
          "u- and v-gradients to differ",
          0, no_sep_swap);
    check(no_sep_perm == 0,
          "and differs from the taps in the wrong lane order -- a transposed "
          "cache lane mapping is detectable",
          0, no_sep_perm);

    // AND THE TAPS THEMSELVES ARE FOUR DIFFERENT TEXELS. The old comment
    // asserted "all four texels come from the same memory pattern, so they are
    // identical" and treated that as a convenience. It is the defect.
    int degenerate_taps = 0;
    for (int i = 0; i < kPhaseN; ++i) {
      zref::Tmu::Req rq;
      rq.u = p2[i].u;
      rq.v = p2[i].v;
      rq.base = kBaseRgb;
      rq.pal_base = kPalBase;
      rq.mode = kModeBil;
      const zref::Tmu::Plan pl = zref::Tmu::plan(rq);
      uint16_t h[4];
      for (int k = 0; k < 4; ++k) h[k] = g_mem.halfword(pl.addr[k]);
      for (int a = 0; a < 4; ++a)
        for (int b = a + 1; b < 4; ++b)
          if (h[a] == h[b]) ++degenerate_taps;
    }
    check(degenerate_taps == 0,
          "and the four taps of every footprint are four DIFFERENT texels, so "
          "no fragment's filter is the identity on its own inputs",
          0, degenerate_taps);

    // ---- AND THE FILTERED PATH IS ADDRESS-SPECIFIC TOO -------------------
    // The same argument as phase 1, applied to the whole footprint: move it
    // one texel in u or v and the retired colour must change. A filter reading
    // the right four texels at the right weights from the WRONG place is
    // exactly as wrong as any other misfetch, and nothing above would have
    // noticed it.
    int weak2 = 0;
    for (int i = 0; i < kPhaseN; ++i) {
      static const int dx[4] = {1, -1, 0, 0};
      static const int dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; ++k) {
        FragSpec g = p2[i];
        g.X = (p2[i].X + dx[k] + 64) & 63;
        g.Y = (p2[i].Y + dy[k] + 64) & 63;
        g.u = coord_for(g.X, g.FU, true);
        g.v = coord_for(g.Y, g.FV, true);
        compute_expectation(g, kModeBil, kBaseRgb);
        if (g.want_rgb == p2[i].want_rgb) ++weak2;
      }
    }
    std::printf("    P2 address separation: %d of %d neighbours indistinguishable\n", weak2,
                kPhaseN * 4);
    check(weak2 == 0,
          "a footprint fetched one texel away in ANY direction would have "
          "retired a different colour, so the exact bilinear check is "
          "address-specific and not merely exact",
          0, weak2);
  }

  // ==========================================================================
  // THE CHAIN, BLOCK BY BLOCK
  // ==========================================================================
  // Named individually so a break is located, not merely detected. A single
  // "the island works" assertion would report the same failure whether the
  // reciprocal never started or the combiner never retired.
  {
    struct Link {
      const char* name;
      uint32_t value;
    };
    const Link chain[] = {
        {"RCP24 completed a reciprocal", d.cnt_rcp_completed_o},
        {"PERSPUV produced fragments", d.cnt_persp_fragments_o},
        {"FRAGROB accepted fragments", d.cnt_fragments_o},
        {"TMU_PLAN accepted sample requests", d.cnt_plan_accepted_o},
        {"CACHE_PIPE was consulted (hits + misses)", d.cnt_cache_hits_o + d.cnt_cache_misses_o},
        {"RSP_DISPATCH routed responses", d.cnt_dispatch_accepted_o},
        {"MOSAIC saw texture samples", d.cnt_mosaic_samples_o},
        {"PALETTE_RES was looked up -- the CLUT path is no longer idle",
         d.cnt_palette_lookups_o},
        {"BILERP ran filter jobs -- the direct-colour path is no longer idle",
         d.cnt_bilerp_jobs_o},
        {"AUX_PIPE accepted requests", d.cnt_aux_accepted_o},
    };
    for (const Link& l : chain) check(l.value > 0, l.name, 1, l.value > 0 ? 1 : 0);
  }

  // ---- THE AUX SHEET WAS ADDRESSED, AND ADDRESSED CORRECTLY ---------------
  // The sheet responder is now a function of the coordinate it is handed, so a
  // wrong aux coordinate is a wrong colour and the exact checks above already
  // cover it. This adds the direct statement: the multiset of coordinates the
  // island requested is the multiset the fragments' world positions imply.
  {
    std::vector<int> want_u, want_v, got_u = o1.sheet_u, got_v = o1.sheet_v;
    got_u.insert(got_u.end(), o2.sheet_u.begin(), o2.sheet_u.end());
    got_v.insert(got_v.end(), o2.sheet_v.begin(), o2.sheet_v.end());
    for (const FragSpec& f : p1)
      if (f.aux) {
        want_u.push_back(aux_axis(f.wx));
        want_v.push_back(aux_axis(f.wz));
      }
    for (const FragSpec& f : p2)
      if (f.aux) {
        want_u.push_back(aux_axis(f.wx));
        want_v.push_back(aux_axis(f.wz));
      }
    // The sheet port holds one request until it is taken, so the same
    // coordinate can be observed on several cycles; compare the SETS of pairs.
    auto sorted_pairs = [](const std::vector<int>& u, const std::vector<int>& v) {
      std::vector<int> k;
      for (std::size_t i = 0; i < u.size(); ++i) k.push_back(u[i] * 64 + v[i]);
      std::sort(k.begin(), k.end());
      k.erase(std::unique(k.begin(), k.end()), k.end());
      return k;
    };
    const std::vector<int> want = sorted_pairs(want_u, want_v);
    const std::vector<int> got = sorted_pairs(got_u, got_v);
    std::printf("    aux sheet coordinates: %d distinct requested, %d distinct expected\n",
                static_cast<int>(got.size()), static_cast<int>(want.size()));
    check(got == want,
          "the island asked the sheet for exactly the coordinates the "
          "fragments' world positions imply -- the aux coordinate path is "
          "computed, not constant. The old responder answered 0x22/0x88 to "
          "everything, so a wrong coordinate produced the right colour",
          static_cast<long>(want.size()), static_cast<long>(got.size()));
    check(want.size() > 4,
          "and the coordinates SWEEP -- a single point would make the sheet "
          "constant again by the back door",
          5, static_cast<long>(want.size()));
  }

  // ---- PER-FRAGMENT RECIPE IDENTITY --------------------------------------
  // The two phases cycle all eight recipes across 64 fragments, so each runs
  // eight times. If the recipe did NOT travel with its fragment -- the fault
  // this island top had, wiring the combiner straight from the input ports --
  // then every fragment would combine with whichever recipe happened to be
  // arriving and exactly ONE counter would move.
  //
  // THE LESSON, which is the one CLAUDE.md already states: three rounds of
  // reasoning about aggregate counts produced three wrong answers, and one
  // measurement of per-fragment IDENTITY produced the right one immediately.
  // When a count is wrong, measure the identity of the things being counted
  // before theorising about the count.
  //
  // `product_jobs()` in zref_material.hpp is the same table the oracle uses, so
  // this asserts the hardware against the reference rather than against itself.
  {
    std::printf("  combine refused(missing samples) = %u, jobs by recipe:",
                d.cnt_combine_refused_o);
    for (int r = 0; r < 8; ++r) std::printf(" %u", d.cnt_combine_jobs_o[r]);
    std::printf("\n");
    int kJobsPerFrag[8];
    for (int r = 0; r < 8; ++r)
      kJobsPerFrag[r] = zref::material::product_jobs(static_cast<uint8_t>(r));
    int wrong_recipe = -1;
    for (int r = 0; r < 8; ++r)
      if (static_cast<int>(d.cnt_combine_jobs_o[r]) != kJobsPerFrag[r] * 8) {
        wrong_recipe = r;
        break;
      }
    if (wrong_recipe >= 0)
      std::printf("  recipe %d issued %d jobs, expected %d\n", wrong_recipe,
                  static_cast<int>(d.cnt_combine_jobs_o[wrong_recipe]),
                  kJobsPerFrag[wrong_recipe] * 8);
    check(wrong_recipe < 0,
          "every recipe issued EXACTLY the jobs its eight fragments call for -- "
          "the recipe, and every other per-fragment attribute, travels with its "
          "fragment instead of being read off the input pin twelve clocks late",
          -1, wrong_recipe);
    check(d.cnt_combine_jobs_o[6] > d.cnt_combine_jobs_o[1],
          "and DETAIL_LIGHT issued more than MODULATE, in the ratio 6:4 the "
          "architecture's job table gives -- the counts follow the RECIPES, "
          "not the arrival order",
          1, d.cnt_combine_jobs_o[6] > d.cnt_combine_jobs_o[1] ? 1 : 0);
    check(d.cnt_combine_jobs_o[0] == 0, "PASSTHRU issued none, as a bypass must", 0,
          d.cnt_combine_jobs_o[0]);
  }

  // ======================= INGRESS-TO-EGRESS IDENTITY ======================
  // THE CHECK THAT WOULD HAVE CAUGHT THE CARRIAGE BUG IN ONE RUN.
  //
  // Every count-based check is an aggregate, and aggregates are exactly what
  // that bug hid behind: 64 fragments went in and 64 came out, every block's
  // counter moved, and the totals looked plausible while only 25 distinct
  // fragments existed and one of them was delivered 24 times.
  //
  // MUTATION EVIDENCE -- this gate is known to DETECT, not merely to pass.
  // Two mutations were applied to zhao_texture_island_top.sv and the results
  // recorded here:
  //
  //   `fr_f_ctx = fr_f_ctx_in` -- the EXACT historical bug, reading the
  //   boundary tap instead of the token-indexed store:
  //       38 missing, 38 duplicated, 63 out of order
  //
  //   `fc_rp = fc_wp` -- reading live ingress at the second read point:
  //       63 missing, 11 duplicated, 52 foreign
  //
  // THE POINT: under the first mutation the OLD aggregate checks still pass.
  // The identity gate fails immediately and unambiguously.
  {
    for (int phase = 0; phase < 2; ++phase) {
      const PhaseOut& o = phase ? o2 : o1;
      const int tag0 = phase ? 0x2000 : 0x1000;
      int missing = 0, duplicated = 0, out_of_order = 0, foreign = 0;
      std::vector<int> seen(kPhaseN, 0);
      for (std::size_t i = 0; i < o.tags.size(); ++i) {
        const int ix = static_cast<int>(o.tags[i]) - tag0;
        if (ix < 0 || ix >= kPhaseN) {
          ++foreign;
          continue;
        }
        if (seen[ix]++ > 0) ++duplicated;
        if (static_cast<int>(i) != ix) ++out_of_order;
      }
      for (int i = 0; i < kPhaseN; ++i)
        if (seen[i] == 0) ++missing;
      std::printf("  phase %d identity: %d missing, %d duplicated, %d out of order, %d foreign\n",
                  phase + 1, missing, duplicated, out_of_order, foreign);
      check(foreign == 0, "every retired tag is one this test SUBMITTED", 0, foreign);
      check(duplicated == 0, "no fragment is retired TWICE", 0, duplicated);
      check(missing == 0, "no submitted fragment is LOST", 0, missing);
      check(out_of_order == 0,
            "the island returns fragments in STRICT SUBMISSION ORDER at its "
            "boundary -- not within a bound, exactly",
            0, out_of_order);
    }
    // AND THE REORDERING REALLY HAPPENED. Without this the check above passes
    // just as well on a workload that never reordered, and an ordering boundary
    // that was never exercised is not evidence of anything.
    std::printf("  fragments that completed early and WAITED at the boundary: %u\n",
                d.cnt_reorder_held_o);
    check(d.cnt_reorder_held_o > 0,
          "and the boundary was actually exercised -- fragments did complete "
          "out of order inside and were held, so strict order out is a "
          "restoration rather than a workload that never disturbed it",
          1, d.cnt_reorder_held_o > 0 ? 1 : 0);
  }

  // ==========================================================================
  // PHASES 4, 5 AND 6 -- THE THREE DIRECT FORMATS, THROUGH THE SHARED DECODE
  // ==========================================================================
  // WHAT THESE EXIST TO CATCH, stated as the defect rather than as a feature.
  // Before the shared decode landed the island had three separate holes that
  // were all the same missing block, and phases 1 and 2 could see NONE of them:
  //
  //   * `near_ok_c` was tied to `1'b0`, so EVERY unfiltered direct-colour
  //     sample completed with SMP_ERR_RGB and was counted in
  //     `cnt_near_refused_o`. Phase 1 is CLUT8, which routes to CLS_CLUT, and
  //     phase 2 is filtered, which routes to CLS_BIL -- so between them they
  //     never sent one response down the nearest lane. Measured with an
  //     RGB565/nearest stimulus: 96 samples, 96 magenta, refused 96.
  //   * `chan8()` had no format input and extracted 5:6:5 fields unconditionally,
  //     so a bilinear fetch of an ARGB4444 sheet was filtered on the wrong bits.
  //     Phase 2 is RGB565, the one format for which that is accidentally right.
  //   * `fr_tmu_a` was the literal 8'hFF. CLUT8 and RGB565 have no alpha and the
  //     reference returns 255 for both, so phases 1 and 2 agree with a hardwire
  //     for a real reason and could never tell it from a decode.
  //
  // THE MEMORY IS NOT REBUILT FOR THIS, AND THAT IS THE POINT. All three phases
  // read the SAME halfwords at `kBaseRgb` that phase 2 reads. Only the mode word
  // changes, so the bytes fetched are identical and every difference in the
  // retired colour is the DECODE and nothing else -- no new texture whose
  // layout could be wrong, no second memory model to disagree with the first.
  //
  // HOW EACH PHASE PROVES THE NEAREST STATION ACTUALLY ANSWERED. "Refused zero"
  // is exactly the shape of a broken instrument -- a lane that is never used
  // refuses nothing either. So the nearest phases also require that the palette
  // performed NO lookup and the filter ran NO job, while the dispatcher routed
  // all 96 responses and all 32 fragments retired the reference's exact colour.
  // With the other two consumers provably idle, the only block that can have
  // produced those colours is the nearest decode station.
  {
    struct DirectPhase {
      const char* name;
      uint8_t fmt;
      bool filtered;
      uint16_t tag_base;
      bool argb;  // does this format carry alpha at all?
    };
    const DirectPhase kDirect[3] = {
        {"RGB565/nearest", zref::Tmu::kRgb565, false, 0x4000, false},
        {"ARGB1555/nearest", zref::Tmu::kArgb1555, false, 0x5000, true},
        {"ARGB4444/bilinear", zref::Tmu::kArgb4444, true, 0x6000, true},
    };

    for (const DirectPhase& dp : kDirect) {
      const uint32_t mode = mode_direct(dp.fmt, dp.filtered);
      std::vector<FragSpec> ph(kPhaseN);
      build_frags(ph, dp.tag_base, mode, kBaseRgb, dp.filtered);

      const uint32_t refused_before = d.cnt_near_refused_o;
      const uint32_t bilerp_before = d.cnt_bilerp_jobs_o;
      const uint32_t palette_before = d.cnt_palette_lookups_o;
      const uint32_t dispatch_before = d.cnt_dispatch_accepted_o;

      PhaseOut o;
      run_phase(d, mode, kBaseRgb, ph, o);

      const uint32_t refused = d.cnt_near_refused_o - refused_before;
      const uint32_t bilerp = d.cnt_bilerp_jobs_o - bilerp_before;
      const uint32_t palette = d.cnt_palette_lookups_o - palette_before;
      const uint32_t dispatch = d.cnt_dispatch_accepted_o - dispatch_before;

      std::printf(
          "  PHASE %s (mode 0x%08X, class %u): submitted %d, retired %d\n"
          "    dispatch +%u | bilerp +%u | palette +%u | NEAR REFUSED +%u\n",
          dp.name, mode, class_of_mode(mode), o.submitted, o.retired, dispatch, bilerp, palette,
          refused);

      char msg[600];
      std::snprintf(msg, sizeof msg, "phase %s retired every fragment it submitted", dp.name);
      check(o.retired == kPhaseN, msg, kPhaseN, o.retired);

      std::snprintf(msg, sizeof msg,
                    "phase %s drove a mode and a class that agree, so the planner's "
                    "derived class equals the one in the source id on every transaction",
                    dp.name);
      check(d.err_class_mismatch_o == 0, msg, 0, static_cast<long>(d.err_class_mismatch_o));

      std::snprintf(msg, sizeof msg,
                    "and the planner raised no mode error on %s -- the format is one it "
                    "names, not one above FMT_ARGB4444",
                    dp.name);
      check(d.err_plan_mode_o == 0, msg, 0, static_cast<int>(d.err_plan_mode_o));

      std::snprintf(msg, sizeof msg,
                    "THE PALETTE WAS NOT CONSULTED ONCE in phase %s -- a direct colour "
                    "has no index, and a lookup here would mean the decode went to the "
                    "wrong consumer",
                    dp.name);
      check(palette == 0, msg, 0, static_cast<long>(palette));

      std::snprintf(msg, sizeof msg,
                    "and the dispatcher routed all %d sample responses in phase %s, so "
                    "the traffic the checks below rest on actually existed",
                    kPhaseN * 3, dp.name);
      check(dispatch == static_cast<uint32_t>(kPhaseN * 3), msg, kPhaseN * 3,
            static_cast<long>(dispatch));

      if (dp.filtered) {
        std::snprintf(msg, sizeof msg,
                      "and phase %s ran FOUR filter jobs per sample -- R, G, B and A -- "
                      "for all %d samples",
                      dp.name, kPhaseN * 3);
        check(bilerp == static_cast<uint32_t>(kPhaseN * 3 * 4), msg, kPhaseN * 3 * 4,
              static_cast<long>(bilerp));
      } else {
        std::snprintf(msg, sizeof msg,
                      "AND THE FILTER RAN NOT ONE JOB in phase %s. With the palette idle "
                      "too, no consumer but the NEAREST decode station can have produced "
                      "the colours checked below -- which is what makes 'refused zero' "
                      "evidence rather than the signature of an unused lane",
                      dp.name);
        check(bilerp == 0, msg, 0, static_cast<long>(bilerp));

        std::snprintf(msg, sizeof msg,
                      "AND NOT ONE SAMPLE WAS REFUSED FOR WANT OF A NEAREST DECODE in "
                      "phase %s. This counter read %d on exactly this stimulus while "
                      "`near_ok_c` was tied to 1'b0, and every one of those samples "
                      "retired SMP_ERR_RGB",
                      dp.name, kPhaseN * 3);
        check(refused == 0, msg, 0, static_cast<long>(refused));
      }

      // ---- the exact, address-specific colour and the exact alpha ----------
      std::vector<int> byidx(kPhaseN, -1);
      for (std::size_t i = 0; i < o.tags.size(); ++i) {
        const int ix = static_cast<int>(o.tags[i]) - static_cast<int>(dp.tag_base);
        if (ix >= 0 && ix < kPhaseN) byidx[ix] = static_cast<int>(i);
      }
      int checked = 0, mismatched = 0, alpha_bad = 0;
      int as565_same = 0, as565_differs = 0;
      int alpha_matters = 0, alpha_unobservable = 0;
      for (int i = 0; i < kPhaseN; ++i) {
        if (byidx[i] < 0) continue;
        ++checked;
        const uint32_t got = o.rgb[byidx[i]];
        const uint8_t got_a = o.alpha[byidx[i]];
        if (got != ph[i].want_rgb) {
          if (mismatched < 4)
            std::printf("    %s frag %d recipe %u texel(%d,%d): got 0x%06X, want 0x%06X\n",
                        dp.name, i, ph[i].recipe, ph[i].X, ph[i].Y, got, ph[i].want_rgb);
          ++mismatched;
        }
        if (got_a != ph[i].want_a) {
          if (alpha_bad < 4)
            std::printf("    %s frag %d recipe %u: alpha got %u, want %u\n", dp.name, i,
                        ph[i].recipe, got_a, ph[i].want_a);
          ++alpha_bad;
        }

        // COUNTERFACTUAL 1 -- THE OLD HARDWIRED RGB565 DECODE, at the same
        // addresses and the same fractions. This is literally what the island
        // shipped for this stimulus before the repair.
        const uint32_t as565 = want_as_rgb565(ph[i], kBaseRgb, dp.filtered);
        if (as565 == ph[i].want_rgb)
          ++as565_same;
        else {
          ++as565_differs;
          if (got == as565) ++as565_same;  // retired the OLD answer: not repaired
        }

        // COUNTERFACTUAL 2 -- THE OLD HARDWIRED 8'hFF ALPHA.
        uint32_t frgb = 0;
        uint8_t fa = 0;
        expect_forced_alpha(ph[i], mode, kBaseRgb, 255, &frgb, &fa);
        if (fa != ph[i].want_a || frgb != ph[i].want_rgb) {
          ++alpha_matters;
          if (got_a == fa && got == frgb) ++alpha_unobservable;
        }
      }

      std::printf(
          "    %s: checked %d, colour wrong %d, alpha wrong %d | vs RGB565 decode: %d "
          "differ | alpha load-bearing on %d fragments\n",
          dp.name, checked, mismatched, alpha_bad, as565_differs, alpha_matters);

      std::snprintf(msg, sizeof msg, "every phase-%s fragment was compared", dp.name);
      check(checked == kPhaseN, msg, kPhaseN, checked);

      std::snprintf(msg, sizeof msg,
                    "every %s fragment retires EXACTLY the colour zref::Tmu::sample "
                    "decodes from ITS OWN planned address and zref::material::combine "
                    "makes of it -- the format-controlled channel fields, the 5/6/4/1-bit "
                    "replications and the material arithmetic, all exact",
                    dp.name);
      check(mismatched == 0, msg, 0, mismatched);

      std::snprintf(msg, sizeof msg,
                    "and every %s fragment's ALPHA matches the reference -- decoded, not "
                    "the literal 8'hFF the island used to assign unconditionally",
                    dp.name);
      check(alpha_bad == 0, msg, 0, alpha_bad);

      if (dp.argb) {
        // The two ARGB formats are the ones the old path could not express at
        // all, so these two checks are the measure of the repair. Both are
        // stated in the direction that fails if the repair did NOT happen.
        std::snprintf(msg, sizeof msg,
                      "and for EVERY %s fragment the decoded answer DIFFERS from what the "
                      "old hardwired RGB565 extraction would have produced from the same "
                      "halfwords at the same addresses -- so this phase cannot pass on a "
                      "format-blind decode",
                      dp.name);
        check(as565_differs == kPhaseN && as565_same == 0, msg, kPhaseN,
              as565_differs - as565_same);

        std::snprintf(msg, sizeof msg,
                      "and the %s alpha is LOAD-BEARING -- on fragments where the decoded "
                      "alpha changes the retired result, the island retired the decoded "
                      "one and not the 8'hFF one. A phase where no fragment could tell "
                      "the two apart would make the check above vacuous",
                      dp.name);
        check(alpha_matters > 0 && alpha_unobservable == 0, msg, 1,
              (alpha_matters > 0 && alpha_unobservable == 0) ? 1 : 0);
      }
    }
  }

  check(d.cnt_fragrob_id_errors_o == 0, "FRAGROB rejected no sample response", 0,
        static_cast<long>(d.cnt_fragrob_id_errors_o));
  check(d.err_rsp_dropped_o == 0,
        "no sample response was produced and dropped -- the merger's "
        "unconditional-consumer assumption still holds",
        0, static_cast<int>(d.err_rsp_dropped_o));
  check(d.err_unknown_class_o == 0, "no response was popped with an unroutable class", 0,
        static_cast<long>(d.err_unknown_class_o));
  check(d.err_class_invalid_o == 0, "and no fragment arrived with an invalid class at the pin", 0,
        static_cast<long>(d.err_class_invalid_o));
  check(d.err_palette_unusable_o == 0, "and no palette lookup shipped the error colour", 0,
        static_cast<long>(d.err_palette_unusable_o));
  // THIS CHECK CHANGED MEANING, WHICH IS THE HEADLINE OF DEFECT (d). It used
  // to be true because CLS_NEAR was never exercised -- a zero that measured the
  // fixture's coverage and not the island. Phases 4 and 5 now push 192 samples
  // through the nearest lane, so it is a statement about the DECODE: 192 direct
  // samples were decoded and not one was refused. Reading zero here while the
  // per-phase dispatch counts above are zero would mean the lane went idle
  // again, which is why those are checked beside it rather than instead of it.
  check(d.cnt_near_refused_o == 0,
        "and across the whole run NOT ONE sample was refused for want of a "
        "nearest decode -- 192 of them went down a lane that answered every "
        "request with SMP_ERR_RGB before the shared format-controlled decode "
        "replaced `near_ok_c = 1'b0`",
        0, static_cast<long>(d.cnt_near_refused_o));

  // ======================= THE OWNER CREDIT, UNDER A STALLED SINK ==========
  // Owner recovery brief, prerequisite 1. The reorder buffer claimed it could
  // not overflow "because FRAGROB admits at most FCTXN fragments". The brief's
  // counterexample is exact and the claim was false:
  //
  //     hold the sink not-ready; admit and complete sequences 0..63; entry 0
  //     still holds sequence 0; INTERNAL CONTEXTS HAVE BEEN RELEASED AND ADMIT
  //     MORE WORK; sequence 64 completes and overwrites entry 0.
  //
  // A 64-FRAGMENT TEST CANNOT REACH THE WRAP, which is why every check above
  // passes either way and this phase exists. It runs LAST, after all cumulative
  // assertions, because it deliberately admits far more work than they account
  // for.
  //
  // It runs in the CLUT8 mode with the matching class, so the class-mismatch
  // counter stays a live assertion through it rather than being abandoned for
  // the stress phase.
  {
    const int kWant = 200;
    int p3_submitted = 0, p3_retired = 0;
    std::vector<uint16_t> p3_tags;
    uint32_t accepted_while_stalled = 0;
    FillModel mem;
    const int kStallUntil = 4000;

    for (int cyc = 0; cyc < 400000; ++cyc) {
      const bool sink_open = (cyc >= kStallUntil);
      d.out_ready_i = sink_open ? 1 : 0;

      // Submit as fast as the island will take it -- no gaps. Backpressure is
      // the property under test, so the test must not supply its own.
      d.frag_valid_i = (p3_submitted < kWant) ? 1 : 0;
      d.frag_depth_i = kDepths[p3_submitted % 3];
      {
        const uint32_t scale = kDepths[p3_submitted % 3] >> 16;
        const int32_t u = coord_for(kXs[p3_submitted & 7], 0, false);
        const int32_t v = coord_for(kYs[(p3_submitted >> 3) & 7], 0, false);
        d.frag_u_over_w_i = static_cast<uint32_t>(u) * scale;
        d.frag_v_over_w_i = static_cast<uint32_t>(v) * scale;
      }
      d.frag_sample_count_i = 3;
      d.frag_binding_i = 1;
      d.frag_lod_i = 0;
      d.frag_recipe_i = static_cast<uint8_t>(1 + (p3_submitted % 3));
      d.frag_weight_i = 128;
      d.frag_aux_i = 0;
      d.frag_class_i = class_of_mode(kModeClut);
      d.frag_base_rgb_i = 0x204060u;
      d.frag_base_a_i = 255;
      d.frag_pal_slot_i = 0;
      d.frag_pal_gen_i = 1;
      d.frag_ctx_i = 0x3000u + static_cast<uint32_t>(p3_submitted);
      d.bind_base_i = kBaseClut;
      d.bind_mode_i = kModeClut;

      d.fill_data_valid_i = 0;
      if (d.fill_valid_o && d.fill_ready_i && mem.beats_left == 0 && mem.delay == 0) {
        mem.addr = d.fill_addr_o;
        mem.delay = 2;
      } else if (mem.delay > 0) {
        if (--mem.delay == 0) mem.beats_left = FillModel::kBeats;
      } else if (mem.beats_left > 0) {
        const int beat = FillModel::kBeats - mem.beats_left;
        d.fill_data_valid_i = 1;
        d.fill_data_i = g_mem.halfword(mem.addr + 2u * static_cast<uint32_t>(beat));
        --mem.beats_left;
        ++mem.served;
      }
      d.sheet_rvalid_i = d.sheet_valid_o;
      d.sheet_tag_i = sheet_tag_at(d.sheet_u_o, d.sheet_v_o);
      d.sheet_str_i = sheet_str_at(d.sheet_u_o, d.sheet_v_o);
      d.sheet_rtok_i = d.sheet_tok_o;

      d.eval();
      const bool accepted = d.frag_valid_i && d.frag_ready_o;
      if (accepted && !sink_open) ++accepted_while_stalled;
      if (d.out_valid_o && d.out_ready_i) {
        p3_tags.push_back(d.out_tag_o);
        ++p3_retired;
      }
      tick(d);
      if (accepted) ++p3_submitted;
      if (p3_submitted >= kWant && p3_retired >= p3_submitted) break;
    }

    std::printf(
        "  credit phase: submitted %d, retired %d, accepted while the sink was "
        "SHUT %u, live peak %u of 64\n",
        p3_submitted, p3_retired, accepted_while_stalled, d.cnt_live_peak_o);

    check(accepted_while_stalled == 64,
          "with the sink held SHUT, the island admitted exactly 64 fragments "
          "and then stopped -- the owner credit is what makes the reorder "
          "buffer's index identify a live fragment rather than a fragment "
          "modulo 64",
          64, static_cast<long>(accepted_while_stalled));
    check(d.cnt_live_peak_o == 64,
          "reaching OWNER_DEPTH exactly, so the ceiling was TESTED rather than "
          "merely not breached",
          64, static_cast<long>(d.cnt_live_peak_o));
    check(p3_retired == p3_submitted,
          "every admitted fragment came back out: nothing was lost to a wrap", p3_submitted,
          p3_retired);
    check(p3_submitted == kWant, "and all 200 were admitted once the sink opened", kWant,
          p3_submitted);

    int p3_bad = 0;
    for (std::size_t i = 0; i < p3_tags.size(); ++i)
      if (p3_tags[i] != static_cast<uint16_t>(0x3000u + i)) ++p3_bad;
    check(p3_bad == 0,
          "and all 200 retired in STRICT submission order across three wraps "
          "of the 64-entry buffer -- the sequence's low bits identify a live "
          "fragment because the credit bounds how many are live",
          0, p3_bad);

    check(d.err_class_mismatch_o == 0,
          "and the mode and class agreed for the whole stress phase too, so no "
          "part of this run measures the class-disagreement defect instead of "
          "the property it names",
          0, static_cast<long>(d.err_class_mismatch_o));
  }

  if (g_failed) {
    std::printf("[island_composed_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[island_composed_directed] %d checks passed\n", g_checks);
  return 0;
}
