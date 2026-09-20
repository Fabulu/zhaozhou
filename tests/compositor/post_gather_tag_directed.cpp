// post_gather_tag_directed.cpp -- does the RTL carry owner ruling R195's law
// EXACTLY, and do its four counters actually fire?
//
// ---------------------------------------------------------------------------
// WHY THIS SWEEPS THE WHOLE INPUT SPACE INSTEAD OF SAMPLING IT
// ---------------------------------------------------------------------------
// The law's input is one 8-bit tag and one 16-bit colour: 2^24 = 16,777,216
// pairs, and every one of them is independent of every other -- the block is
// combinational plus one register. A random sample of such a space is a
// WEAKER instrument than a sweep and costs more to argue about, so this walks
// ALL of it. That was a strided sample of 4,094 points until the full walk was
// TIMED at 2.3 SECONDS -- the sample was not even the cheap option, it was a
// guess about cost nobody had measured.
//
// WHAT A SPOT CHECK WOULD MISS, concretely. The model computes
// `unit_mul(unit_mul(channel, gain), tint)` -- TWO roundings, in that order.
// Folding `gain` and `tint` together first is algebraically identical and
// ARITHMETICALLY DIFFERENT, because round-half-up does not commute across a
// product. The two agree on most inputs and differ by one LSB on some.
//
// THAT IS ALSO THIS SWEEP'S POSITIVE CONTROL, and it is not a rhetorical
// point. `mismatches == 0` over 16.7 million points is exactly the shape
// CLAUDE.md calls a broken instrument until proven otherwise, so the same walk
// scores the FOLDED law too and REQUIRES it to differ. The count is printed;
// if it were zero, either the fold is harmless or the comparison is not
// looking at the subject, and the sweep's own zero would be worthless.
//
// ---------------------------------------------------------------------------
// THE FOUR COUNTERS ARE A PARTITION, AND EACH IS FIRED DELIBERATELY
// ---------------------------------------------------------------------------
// CLAUDE.md: a detector that has not been shown to FIRE has not been tested,
// and a counter reading zero is the claim to check hardest. All four of these
// are reachable with LEGAL stimulus at this block's own ports -- there is no
// unreachable guard here, so no committed mutant is owed:
//
//   frag_untagged_o     channel 0b00                     tag = 0x00
//   frag_below_knee_o   GLOW at or below the knee        tag = 0x40 | 24
//   frag_lit_o          GLOW above the knee              tag = 0x40 | 63
//   reserved_channel_o  0b10 / 0b11, R195 decision 3     tag = 0x80, 0xC0
//
// And the PARTITION is asserted, not just the four firings: the sum must equal
// the number of accepted fragments. A partition is a much stronger instrument
// than four tallies, because one wrong branch breaks the sum and no counter
// read on its own could say so.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vzhao_post_gather_tag.h"

#include "zhao_sim.hpp"
#include "zref/zref_post.hpp"

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vzhao_post_gather_tag top;

  auto idle = [&]() {
    top.f_valid_i = 0;
    top.f_tag_i = 0;
    top.f_rgb565_i = 0;
    top.f_x_i = 0;
    top.f_y_i = 0;
    top.tile_start_i = 0;
    top.tile_flush_i = 0;
  };

  idle();
  top.rst_n = 0;
  for (int i = 0; i < 4; ++i) zhao::tick(top);
  top.rst_n = 1;
  zhao::tick(top);

  // One fragment in, one beat out, one clock later.
  auto present = [&](uint8_t tag, uint16_t rgb) {
    top.f_valid_i = 1;
    top.f_tag_i = tag;
    top.f_rgb565_i = rgb;
    zhao::tick(top);
    top.f_valid_i = 0;
  };

  // ======================================================================
  // 1. THE LAW, AGAINST zref::post::gather, OVER THE WHOLE INPUT SPACE
  // ======================================================================
  // ALL 2^24 PAIRS. This was a strided sample of 4,094 until it was TIMED:
  // the full walk takes 2.3 seconds. A sample was never the right instrument
  // here and was not even the cheap one -- it was a guess about cost that
  // nobody had measured, which is this repository's own recurring shape.
  uint64_t swept = 0;
  uint64_t mismatches = 0;
  uint32_t first_bad_tag = 0, first_bad_rgb = 0;
  // The NEGATIVE CONTROL rides along in the same walk (see below).
  uint64_t folded_differs = 0;

  for (uint64_t i = 0; i < (1u << 24); ++i) {
    const uint8_t tag = static_cast<uint8_t>(i >> 16);
    const uint16_t rgb = static_cast<uint16_t>(i & 0xFFFFu);
    present(tag, rgb);
    const zref::post::gather::Fragment want =
        zref::post::gather::tag_to_fragment(tag, rgb);
    ++swept;
    const bool ok = (top.g_valid_o == 1) && (top.g_glow_r_o == want.glow_r) &&
                    (top.g_glow_g_o == want.glow_g) &&
                    (top.g_glow_b_o == want.glow_b) &&
                    // R195 decision 3: these are ZERO under this law, and the
                    // model says so too. Checking them is checking that the
                    // ruling is implemented, not that a wire is idle.
                    (top.g_disp_x_o == 0) && (top.g_disp_y_o == 0) &&
                    (top.g_ink_o == 0);
    if (!ok) {
      if (mismatches == 0) {
        first_bad_tag = tag;
        first_bad_rgb = rgb;
      }
      ++mismatches;
    }

    // ---- THE POSITIVE CONTROL FOR THE SWEEP ITSELF --------------------
    // CLAUDE.md: "a number that is exactly zero is a broken instrument until
    // proven otherwise", and `mismatches == 0` over 16.7 million points is
    // precisely that shape. So the same walk also compares the RTL against
    // the WRONG law -- the fold this file's header warns about, where `gain`
    // and `tint` are combined before the channel instead of after. It is
    // algebraically identical and arithmetically different, because
    // round-half-up does not commute across a product.
    //
    // If `folded_differs` came back ZERO, either the fold is harmless (and
    // the header's warning is wrong) or the comparison is not looking at the
    // subject (and `mismatches == 0` means nothing). Either way the sweep
    // would have to be rewritten rather than believed.
    // AND IT IS SCORED ON ALL THREE CHANNELS, which is not fussiness -- the
    // first version of this control scored only RED and came back ZERO,
    // failing itself. The reason is specific and would have been invisible in
    // prose: `kGlowTint[0]` is 255, and `unit_mul(x, 255) == x` for every x
    // the ramp can produce (the ramp saturates at 68, and the identity only
    // breaks near 255). So the fold IS harmless on red, and the one channel
    // picked to demonstrate it was the one channel that could not. G and B
    // carry 236 and 224 and differ.
    if (zref::post::gather::tag_channel(tag) ==
        zref::post::gather::kChannelGlow) {
      const uint8_t gain = zref::post::gather::unit_mul(
          zref::post::gather::glow_gain(zref::post::gather::tag_strength(tag)),
          zref::post::gather::kGlowMaster);
      const uint8_t ch[3] = {zref::post::gather::exp5((rgb >> 11) & 0x1F),
                             zref::post::gather::exp6((rgb >> 5) & 0x3F),
                             zref::post::gather::exp5(rgb & 0x1F)};
      const uint8_t got[3] = {want.glow_r, want.glow_g, want.glow_b};
      for (int c = 0; c < 3; ++c) {
        const uint8_t folded = zref::post::gather::unit_mul(
            ch[c], zref::post::gather::unit_mul(
                       gain, zref::post::gather::kGlowTint[c]));
        if (folded != got[c]) ++folded_differs;
      }
    }
  }
  zhao::check(mismatches == 0,
              "R195's law is bit-exact against zref::post::gather over ALL "
              "2^24 (tag, rgb565) pairs -- including the ROUNDING ORDER, "
              "which is the part that looks right when it is folded",
              0, mismatches);
  if (mismatches != 0)
    std::printf("  first mismatch at tag=0x%02X rgb565=0x%04X\n", first_bad_tag,
                first_bad_rgb);
  zhao::check(folded_differs > 0,
              "POSITIVE CONTROL: the FOLDED law -- unit_mul(c, unit_mul(gain, "
              "tint)) instead of unit_mul(unit_mul(c, gain), tint) -- differs "
              "from the model on a real number of inputs. So the rounding "
              "order is load-bearing AND the sweep above can tell one law "
              "from another; a zero here would make its zero worthless",
              1, folded_differs > 0 ? 1 : 0);

  // ======================================================================
  // 2. THE KNEE IS A KNEE, which is decision 2 and the one the picture is
  //    most sensitive to. Below it a texel is LIT, not a LIGHT.
  // ======================================================================
  {
    // Strength exactly AT the knee must contribute nothing: the model's
    // comparison is `<=`, and an off-by-one here is the difference between
    // "stars glow" and "the whole image hazes".
    present(static_cast<uint8_t>(0x40 | zref::post::gather::kGlowKnee), 0xFFFF);
    const bool at_knee_dark =
        (top.g_glow_r_o == 0) && (top.g_glow_g_o == 0) && (top.g_glow_b_o == 0);
    zhao::check(at_knee_dark,
                "a white fragment at exactly kGlowKnee contributes NO glow -- "
                "the knee is inclusive, and R195 measured what the other "
                "direction costs (knee 16: 907 of 5,760 cells against 74)",
                1, at_knee_dark ? 1 : 0);

    present(static_cast<uint8_t>(0x40 | (zref::post::gather::kGlowKnee + 1)),
            0xFFFF);
    const bool above_lit = (top.g_glow_r_o != 0);
    zhao::check(above_lit,
                "one strength step ABOVE the knee does contribute, so the "
                "ramp is a ramp and not a dead band",
                1, above_lit ? 1 : 0);
  }

  // ======================================================================
  // 3. THE GLOW BORROWS THE FRAGMENT'S OWN COLOUR (decision 1), and the
  //    TINT is a real per-channel weight rather than a decoration.
  // ======================================================================
  {
    present(0x40 | 63, 0xF800);  // full red, maximum strength
    const bool red_only = (top.g_glow_r_o != 0) && (top.g_glow_g_o == 0) &&
                          (top.g_glow_b_o == 0);
    zhao::check(red_only,
                "a red star's halo is RED -- the bloom borrows the resolved "
                "colour instead of inventing a second palette",
                1, red_only ? 1 : 0);

    present(0x40 | 63, 0xFFFF);  // white, maximum strength
    const int wr = top.g_glow_r_o, wg = top.g_glow_g_o, wb = top.g_glow_b_o;
    zhao::check(wr >= wg && wg >= wb && wb < wr,
                "the tint's warm bias survives to the output: R is left alone "
                "and G and B are pulled back, which is what 255/236/224 says",
                1, (wr >= wg && wg >= wb && wb < wr) ? 1 : 0);
    std::printf("  white at full strength -> (%d, %d, %d)\n", wr, wg, wb);
  }

  // ======================================================================
  // 4. THE TILE PULSES ARE DELAYED BY EXACTLY AS MUCH AS THE DATA
  // ======================================================================
  // If the data were delayed and the pulses were not, every tile's last
  // fragment would land in the next tile's bank -- a halo displaced by four
  // pixels at every tile edge, sixteen times a row. One clock, both.
  {
    idle();
    zhao::tick(top);
    top.tile_start_i = 1;
    top.tile_flush_i = 0;
    zhao::tick(top);
    top.tile_start_i = 0;
    const bool start_seen = (top.g_tile_start_o == 1);
    zhao::check(start_seen,
                "tile_start arrives at the output exactly one clock later -- "
                "the same one clock the fragment data takes",
                1, start_seen ? 1 : 0);
    zhao::tick(top);
    zhao::check(top.g_tile_start_o == 0,
                "and it is a PULSE, not a level: a held start would clear the "
                "accumulating bank on every clock of a tile", 0,
                top.g_tile_start_o);
  }

  // ======================================================================
  // 5. THE COUNTERS. FIRE ALL FOUR, THEN ASSERT THE PARTITION.
  // ======================================================================
  {
    idle();
    top.rst_n = 0;
    for (int i = 0; i < 4; ++i) zhao::tick(top);
    top.rst_n = 1;
    zhao::tick(top);

    const uint32_t base_u = top.frag_untagged_o;
    const uint32_t base_k = top.frag_below_knee_o;
    const uint32_t base_l = top.frag_lit_o;
    const uint32_t base_r = top.reserved_channel_o;
    zhao::check(base_u == 0 && base_k == 0 && base_l == 0 && base_r == 0,
                "all four counters reset to zero, so what follows measures "
                "this stimulus and not the sweep above",
                0, base_u + base_k + base_l + base_r);

    present(0x00, 0xFFFF);                                      // untagged
    present(0x40 | 0, 0xFFFF);                                  // GLOW, str 0
    present(0x40 | zref::post::gather::kGlowKnee, 0xFFFF);      // GLOW, at knee
    present(0x40 | 63, 0xFFFF);                                 // GLOW, lit
    present(0x40 | 40, 0x07E0);                                 // GLOW, lit
    present(0x80, 0xFFFF);                                      // reserved 0b10
    present(0xC0 | 31, 0xFFFF);                                 // reserved 0b11
    zhao::tick(top);

    zhao::check(top.frag_untagged_o == 1,
                "frag_untagged_o FIRES on channel 0b00", 1,
                top.frag_untagged_o);
    zhao::check(top.frag_below_knee_o == 2,
                "frag_below_knee_o FIRES on GLOW at and below the knee", 2,
                top.frag_below_knee_o);
    zhao::check(top.frag_lit_o == 2, "frag_lit_o FIRES on GLOW above the knee",
                2, top.frag_lit_o);
    // THE ONE R195 DECISION 3 EXISTS FOR. The day stars_and_flares.md
    // allocates a refraction or ink channel, this counter is what says
    // whether anything was already drawing it.
    zhao::check(top.reserved_channel_o == 2,
                "reserved_channel_o FIRES on BOTH unallocated channels -- this "
                "is the instrument R195 decision 3 is built around",
                2, top.reserved_channel_o);

    const uint32_t sum = top.frag_untagged_o + top.frag_below_knee_o +
                         top.frag_lit_o + top.reserved_channel_o;
    zhao::check(sum == 7,
                "the four counters PARTITION the stream: every accepted "
                "fragment lands in exactly one, so the sum is the count. One "
                "wrong branch breaks this and no counter read alone could say "
                "so",
                7, sum);
  }

  // ======================================================================
  // 6. AN UNACCEPTED BEAT CONTRIBUTES NOTHING
  // ======================================================================
  // POST.GATHER has no `ready` by R5, so the ACCEPTED-beat gate lives on the
  // producer side and this block must honour `f_valid_i` strictly. A block
  // that counted or lit on an idle clock would make one bright fragment read
  // as many -- the exact failure the shell's `rpx_valid && rpx_ready` exists
  // to prevent, checked from the other end.
  {
    const uint32_t before = top.frag_untagged_o + top.frag_below_knee_o +
                            top.frag_lit_o + top.reserved_channel_o;
    top.f_valid_i = 0;
    top.f_tag_i = 0x40 | 63;   // a LIT tag, held on the wires
    top.f_rgb565_i = 0xFFFF;
    for (int i = 0; i < 8; ++i) zhao::tick(top);
    const uint32_t after = top.frag_untagged_o + top.frag_below_knee_o +
                           top.frag_lit_o + top.reserved_channel_o;
    zhao::check(before == after,
                "eight idle clocks with a LIT tag held on the input wires "
                "contribute nothing -- valid is the gate, not the data",
                before, after);
    const bool dark = (top.g_glow_r_o == 0) && (top.g_glow_g_o == 0) &&
                      (top.g_glow_b_o == 0);
    zhao::check(dark,
                "and the glow output is ZERO on an invalid beat, so the "
                "accumulator downstream cannot add a stale value",
                1, dark ? 1 : 0);
  }

  std::printf(
      "  swept %llu (tag, rgb565) pairs, %llu mismatch(es); the FOLDED law "
      "differs from the model on %llu of them\n",
      static_cast<unsigned long long>(swept),
      static_cast<unsigned long long>(mismatches),
      static_cast<unsigned long long>(folded_differs));

  return zhao::report_and_exit("post_gather_tag_directed");
}
