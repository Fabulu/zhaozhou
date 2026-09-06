// island_composed_directed.cpp — the composed texture island carries a
// fragment from end to end.
// Authored 2026-09-05 (roadmap G1-D).
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
// HOW IT AVOIDS PASSING VACUOUSLY
// ---------------------------------------------------------------------------
// "A fragment came out" is a weak claim: FRAGROB could retire a fragment whose
// samples never reached the cache, and the picture would be wrong but the
// handshake would look healthy. So the acceptance condition is that **every
// block's counter moved** -- the reciprocal completed, perspective produced
// fragments, the planner accepted, the cache was consulted, the dispatcher
// routed, the bilinear lane ran jobs, the palette was looked up, aux accepted,
// and the combiner retired. A zero anywhere in that list means the chain is cut
// at that point, and the test names WHICH counter is zero rather than reporting
// a generic failure.
//
// This is the same discipline that caught two vacuous tests in this tree
// already: a 900-tick wizards replay that killed nobody, and a shell test that
// compared two blank framebuffers and would have called them equal.

#include "zref/zref_material.hpp"
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

// A trivial memory behind the cache's fill port.
//
// IT MUST STREAM A WHOLE LINE. `zhao_texture_cache_pipe` counts beats and only
// marks the line valid on the last one:
//
//     if (fb_beat_r == BEAT_W'(HW_PL - 1)) ... fb_busy_r <= 1'b0;
//
// with `HW_PL = LINE_BYTES / 2 = 8`. The first version of this model answered
// each fill with ONE halfword, so the cache waited forever for beats 2..8, back-
// pressured the planner, and nothing ever reached the dispatcher. The composed
// test reported exactly that -- "cache miss 1, dispatch 0" -- which is why the
// chain is checked block by block instead of with one pass/fail.
//
// The data is derived from the address so a misrouted fill produces a wrong
// colour rather than a plausible one.
struct FillModel {
  static constexpr int kBeats = 8;  // LINE_BYTES / 2
  uint32_t addr = 0;
  int delay = 0;  // cycles before the first beat: the memory is not instant
  int beats_left = 0;
  uint32_t served = 0;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  Dut d;
  d.rst_n = 0;
  d.frag_valid_i = 0;
  d.out_ready_i = 1;
  d.fill_ready_i = 1;
  d.fill_data_valid_i = 0;
  d.sheet_ready_i = 1;
  d.sheet_rvalid_i = 0;
  d.pal_ld_valid_i = 0;
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
  const int kPalEntries = 256;
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
    d.pal_ld_rgb565_i = static_cast<uint16_t>(0x0841 * (i + 1));
    tick(d);
  }
  d.pal_ld_op_i = 2;  // LD_END
  tick(d);
  d.pal_ld_valid_i = 0;
  tick(d);

  // ---- drive fragments ----------------------------------------------------
  FillModel mem;
  const int kFrags = 64;
  int submitted = 0, retired = 0;
  int gap = 0;  // poisoned cycles deliberately inserted mid-stream
  uint32_t first_rgb = 0;
  int nonzero_rgb = 0;
  std::vector<uint32_t> g_retired_tags;
  std::vector<uint32_t> g_clut_rgb;  // exact retired colour, CLUT fragments only
  std::vector<int> g_clut_idx;       // and which fragment each came from
  std::vector<uint32_t> g_bil_rgb;   // and the same for the BILINEAR half
  std::vector<int> g_bil_idx;
  int g_nz_by_cls[2] = {0, 0};
  bool saw_rgb = false;

  for (int cyc = 0; cyc < 400000; ++cyc) {
    // fragment in
    // A DELIBERATE GAP after every fourth fragment, so the poison below lands
    // while earlier fragments are still in flight rather than only during the
    // final drain. Without it `frag_valid_i` is high continuously until the
    // last fragment and there is no idle cycle to poison.
    const bool present = (gap == 0) && (submitted < kFrags);
    if (gap > 0) --gap;

    if (present) {
      d.frag_valid_i = 1;
      d.frag_depth_i = 0x010000u + static_cast<uint32_t>(submitted * 37);
      d.frag_u_over_w_i = 0x00020000u + static_cast<uint32_t>(submitted * 131);
      d.frag_v_over_w_i = 0x00030000u + static_cast<uint32_t>(submitted * 197);
      d.frag_sample_count_i = 3;
      d.frag_binding_i = 1;
      d.frag_lod_i = 0;
      // Cycle the recipes so the combiner's bypass, product and continuation
      // paths are all exercised inside the composition -- not just the one
      // that happens to be cheapest.
      d.frag_recipe_i = static_cast<uint8_t>(submitted % 8);
      d.frag_weight_i = 128;
      d.frag_ctx_i = static_cast<uint64_t>(0x1000 + submitted);
      d.frag_aux_i = (submitted % 3) == 0;
      // ALTERNATE THE SAMPLE CLASS. Every fragment was tagged bilinear before,
      // which left the palette wired and permanently idle -- `palette 0` in
      // every run. Class 0 is the CLUT path, 2 the bilinear one.
      d.frag_class_i = (submitted % 2) ? 2 : 0;
      d.frag_base_rgb_i = 0x204060u;
      d.frag_base_a_i = 255;
      // the fragment names its own palette binding: slot 0, generation 1,
      // which is what the upload loop above wrote
      d.frag_pal_slot_i = 0;
      d.frag_pal_gen_i = 1;
      d.bind_base_i = 0x0010'0000u;
      d.bind_mode_i = 0;
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
      // fragment's, and the job histogram, the sample-class split, the aux
      // count and the retired colours all move.
      //
      // It ALTERNATES rather than holding a constant. A constant poison makes
      // the recipe constant too, and a wrong-but-constant recipe still yields
      // a tidy-looking histogram -- which is how the original defect survived
      // being looked at. `frag_pal_slot_i` is never 0, so any late read of the
      // palette binding lands on an unloaded slot and goes stale.
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

    // aux sheet responder
    // ECHO THE REQUEST'S TOKEN. AUX_PIPE matches a sheet response to the
    // request that asked for it; answering with a constant 0 makes every aux
    // result carry a wrong identity, FRAGROB rejects them all, and because it
    // retires in allocation order a single aux fragment at the head stalls the
    // whole island -- 48 samples fetched and 0 fragments out.
    d.sheet_rvalid_i = d.sheet_valid_o;
    d.sheet_tag_i = 0x22;
    d.sheet_str_i = 0x88;
    d.sheet_rtok_i = d.sheet_tok_o;

    // memory behind the cache
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
      // EVERY HALFWORD IS 0xAABB, so the two CLUT8 texels inside it are
      // DISTINCT and both are known. That is what makes an exact colour check
      // possible without the test re-deriving the planner's addresses:
      // whichever texel a fragment asked for, its palette index is either 0xBB
      // or 0xAA and nothing else.
      //
      // A hash of the address -- what this was -- gives a different byte per
      // line and makes the expected colour unknowable without modelling the
      // address maths, which is why the test could only ever check "non-zero".
      // A CONSTANT would be worse still: both bytes equal, so the byte select
      // stops mattering and the defect it exists to catch becomes invisible.
      // 0x8586: the two CLUT8 texels are indices 0x86 and 0x85, chosen because
      // both LARGE (so the material arithmetic does not round them to zero) and
      // whose palette entries distinguish the two expansion laws in ALL
      // THREE channels -- 0x5A47 has r5=11, g6=18, b5=7, so replication and
      // zero-fill differ everywhere.
      //
      // THIS MATTERS AND WAS FOUND THE HARD WAY. The first pattern here was
      // 0xAABB, whose index 0xBB maps to palette entry 0x0FBC with r5 = 1 --
      // and 1 has no low bits to replicate, so zero-fill and replication agree.
      // Mutating the red channel back to zero-fill produced ZERO mismatches:
      // the exact-colour check was insensitive to the very defect it was
      // written to cover. A check that cannot fail is not evidence, however
      // exact it looks.
      d.fill_data_i = 0x8586u;
      --mem.beats_left;
      ++mem.served;
    }

    d.eval();
    const bool accepted = d.frag_valid_i && d.frag_ready_o;
    if (d.out_valid_o && d.out_ready_i) {
      if (!saw_rgb) {
        first_rgb = d.out_rgb_o;
        saw_rgb = true;
      }
      g_retired_tags.push_back(d.out_tag_o);
      {
        const int ix = static_cast<int>(d.out_tag_o) - 0x1000;
        if (ix >= 0 && ix < 64 && (ix % 2) == 0) {
          g_clut_rgb.push_back(d.out_rgb_o);
          g_clut_idx.push_back(ix);
        } else if (ix >= 0 && ix < 64) {
          g_bil_rgb.push_back(d.out_rgb_o);
          g_bil_idx.push_back(ix);
        }
      }
      if (d.out_rgb_o != 0) ++nonzero_rgb;
      {
        const int ix = static_cast<int>(d.out_tag_o) - 0x1000;
        if (ix >= 0 && ix < 64 && d.out_rgb_o != 0) g_nz_by_cls[ix % 2]++;
      }
      ++retired;
    }
    tick(d);
    if (accepted) {
      ++submitted;
      if ((submitted % 4) == 0) gap = 3;
    }
    if (submitted >= kFrags && retired >= kFrags) break;
  }

  std::printf(
      "  submitted %d, retired %d, fills served %u\n"
      "  rcp %u | persp %u | plan %u | cache hit %u miss %u | dispatch %u\n"
      "  bilerp %u | palette %u | mosaic %u | aux %u | fragrob %u\n"
      "  fragrob ID ERRORS %u\n",
      submitted, retired, mem.served, d.cnt_rcp_completed_o, d.cnt_persp_fragments_o,
      d.cnt_plan_accepted_o, d.cnt_cache_hits_o, d.cnt_cache_misses_o, d.cnt_dispatch_accepted_o,
      d.cnt_bilerp_jobs_o, d.cnt_palette_lookups_o, d.cnt_mosaic_samples_o, d.cnt_aux_accepted_o,
      d.cnt_fragments_o, d.cnt_fragrob_id_errors_o);

  check(submitted > 0, "the island ACCEPTED fragments at its boundary", 1, submitted > 0 ? 1 : 0);

  // ---- the chain, block by block -----------------------------------------
  // Named individually so a break is located, not merely detected. A single
  // "the island works" assertion would report the same failure whether the
  // reciprocal never started or the combiner never retired.
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
      {"PALETTE_RES was looked up -- the CLUT path is no longer idle", d.cnt_palette_lookups_o},
      {"AUX_PIPE accepted requests", d.cnt_aux_accepted_o},
  };
  for (const Link& l : chain) check(l.value > 0, l.name, 1, l.value > 0 ? 1 : 0);

  // ---- PER-FRAGMENT RECIPE IDENTITY --------------------------------------
  // The test cycles all eight recipes across 64 fragments, so each runs eight
  // times. If the recipe did NOT travel with its fragment -- the fault this
  // island top had, wiring the combiner straight from the input ports -- then
  // every fragment would combine with whichever recipe happened to be arriving
  // and exactly ONE counter would move. Several moving, in the right
  // proportions, is what says each fragment kept its own.
  {
    std::printf("  combine refused(missing samples) = %u", d.cnt_combine_refused_o);
    std::printf("  combine jobs by recipe:");
    for (int r = 0; r < 8; ++r) std::printf(" %u", d.cnt_combine_jobs_o[r]);
    std::printf("\n");
    int moved = 0;
    for (int r = 0; r < 8; ++r)
      if (d.cnt_combine_jobs_o[r] > 0) ++moved;
    // The threshold is TWO, and that is what this check can honestly claim.
    //
    // Before the context word was actually wired to FRAGROB -- it was passing
    // `frag_ctx_i` where it meant `fr_f_ctx`, so the packing existed and was
    // never connected -- every one of these counters was ZERO and the combiner
    // took its `sample_count == 0` base-passthrough path for all 64 fragments.
    // Nothing noticed, because no test read them. More than one counter moving
    // is what proves the recipe now arrives with its fragment.
    //
    // RESOLVED, and the resolution is worth more than the anomaly was.
    //
    // The distribution used to read 0 0 0 4 0 0 24 112 against an expected
    // 8 x (0+4+4+4+0+0+6+4) = 176. It now reads 0 32 32 32 0 0 48 32, which is
    // that expectation exactly, and the check below asserts it.
    //
    // HOW IT WAS FOUND, because the route matters. Three hypotheses were
    // written down confidently and all three were wrong:
    //
    //   1. "the recipe field arrives OR-ed together" -- refuted by driving a
    //      FIXED recipe, which gave exact counts (64 x 4, 64 x 6). A corrupted
    //      field cannot produce exact counts.
    //   2. "fragments are mis-associated with their neighbour's recipe" --
    //      refuted by varying how OFTEN the recipe changes: the total moved to
    //      140, 180 and 232 around the expected 176, and mere re-association
    //      cannot inflate a total.
    //   3. "jobs are being issued more than once" -- refuted by the measurement
    //      that finally settled it.
    //
    // What settled it was recording the TAG of every retired fragment instead
    // of reasoning about the counts. `out_tag_o` was already exposed, so this
    // cost one run: of 64 fragments submitted, only 25 distinct ones ever came
    // out, 39 were lost, 7 were duplicated, and the tail was fragment 63
    // delivered 24 times. The jobs counted then matched what that ACTUAL
    // retired set predicts -- 140 = 140, exactly -- which exonerated the
    // combiner completely. Logging the same tags one stage earlier showed the
    // identical sequence arriving at FRAGROB, and logging the slot each
    // fragment was written into showed FRAGROB's own allocation and ordered
    // retire were PERFECT: the head slot walked 0..15 exactly four times. The
    // reorder buffer was innocent and had been handed the wrong data.
    //
    // THE BUG: the island read every per-fragment attribute off its own INPUT
    // PINS at the point each was consumed -- ten separate signals, including
    // PERSPUV's u/w and v/w numerators. A fragment spends about twelve clocks
    // in RCP24 and PERSPUV, so each tap sampled whatever fragment happened to
    // be at the boundary twelve clocks later, and once submission stopped the
    // pins simply held fragment 63. The token needed to fix it already existed
    // and was already carried end to end (`tok_r` -> RCP24 `r_tok_o` ->
    // PERSPUV `tag_o`); nothing consulted it. Attributes are now stored at
    // admission and read back by that token.
    //
    // THE LESSON, which is the one CLAUDE.md already states: three rounds of
    // reasoning about aggregate counts produced three wrong answers, and one
    // measurement of per-fragment IDENTITY produced the right one immediately.
    // The counters said "something is wrong" and could not say what; they
    // aggregate away the very field that was broken. When a count is wrong,
    // measure the identity of the things being counted before theorising about
    // the count.
    //
    // The check is now the EXACT distribution, not a threshold. While the
    // carriage bug was live this could only be stated as "more than one recipe
    // moved", because the real numbers were unexplained -- and a threshold is
    // exactly what lets a wrong distribution keep passing. `product_jobs()` in
    // zref_material.hpp is the same table the oracle uses, so this asserts the
    // hardware against the reference rather than against itself.
    // THE ORACLE'S OWN TABLE, not a copy of it. AUDIT R15 observed that this
    // hardcoded its own array while its comment claimed to reference
    // zref_material -- so the two could disagree, and did: product_jobs said
    // MASK costs one product and this said zero. Calling the function removes
    // the possibility rather than correcting one instance of it.
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
    check(moved >= 2,
          "and more than one recipe issued product jobs at all -- before the "
          "context word was wired, ALL EIGHT counters were zero and every "
          "fragment took the untextured path",
          2, moved);
    check(d.cnt_combine_jobs_o[6] > d.cnt_combine_jobs_o[1],
          "and DETAIL_LIGHT issued more than MODULATE, in the ratio 6:4 the "
          "architecture's job table gives -- the counts follow the RECIPES, "
          "not the arrival order",
          1, d.cnt_combine_jobs_o[6] > d.cnt_combine_jobs_o[1] ? 1 : 0);
    check(d.cnt_combine_jobs_o[0] == 0, "PASSTHRU issued none, as a bypass must", 0,
          d.cnt_combine_jobs_o[0]);
  }

  check(retired > 0,
        "and a fragment came OUT of the combiner at the far end -- the chain is "
        "connected end to end, not eleven blocks sharing a clock",
        1, retired > 0 ? 1 : 0);

  // Anti-vacuity on the colour itself. A chain that runs but returns black for
  // every fragment has proved the handshakes and nothing else.
  // ANTI-VACUITY, counted over EVERY retired fragment rather than the first.
  // It used to test `first_rgb`, which passed only because the attribute-tap
  // bug made fragment 12 arrive first. With the fragments in their real order
  // the first is fragment 0, so the guard silently depended on which fragment
  // happened to lead -- and fixing a genuine bug broke a check that was never
  // about the first fragment. Both halves are now asserted; the CLUT half was
  // black for a whole pass and is checked separately below.
  std::printf("  palette: %u lookups, %u stale, %u cold\n", d.cnt_palette_lookups_o,
              d.cnt_palette_stale_o, d.cnt_palette_cold_o);
  std::printf("  non-zero colour by class -- CLUT %d, bilinear %d (of 32 each)\n", g_nz_by_cls[0],
              g_nz_by_cls[1]);
  std::printf("  retires carrying a non-zero colour: %d of %d\n", nonzero_rgb, retired);
  check(g_nz_by_cls[1] == 32, "every BILINEAR fragment carries a NON-ZERO colour", 32,
        g_nz_by_cls[1]);

  // ======================= EXACT COLOUR, THROUGH THE ORACLE ================
  // AUDIT R5: "it checks nonzero colours rather than exact expected per-fragment
  // RGBA. A counter moving or every colour being nonzero cannot establish that
  // the fetched texels, weights, channel expansion, or material output are
  // correct."
  //
  // Now it does, for the CLUT half, and against zref rather than against a
  // second copy of the arithmetic.
  //
  // WHAT MAKES IT COMPUTABLE. Every halfword the memory returns is 0x8586, so a
  // fragment's palette index is 0x86 or 0x85 and nothing else -- whichever texel
  // of the pair it addressed. The island gives all three samples of a fragment
  // the SAME u/v, so all three resolve to the same palette colour. The palette
  // was uploaded as `0x0841 * (index + 1)` truncated to 16 bits, and the island
  // expands 565 to 888 by replication. So both admissible sample colours are
  // known here exactly, and the expected OUTPUT is whatever
  // zref::material::combine makes of them under that fragment's recipe.
  //
  // A FIRST VERSION OF THIS CHECK COMPARED AGAINST THE SAMPLE COLOUR DIRECTLY
  // and reported 24 of 32 fragments "unexpected". That was the check being
  // naive, not the island being wrong: CLUT fragments carry recipes 0, 2, 4 and
  // 6, and only PASSTHRU returns a sample unchanged. Exactly the 8 PASSTHRU
  // fragments matched. Recorded because "24 unexpected" looked like a defect for
  // about a minute, and the difference between a wrong expectation and a wrong
  // machine is the whole job.
  {
    auto expand = [](uint16_t p) -> zref::material::Sample {
      const uint32_t r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
      zref::material::Sample s;
      s.r = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
      s.g = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
      s.b = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
      s.a = 255;  // the island drives fr_tmu_a = 8'hFF
      return s;
    };
    const zref::material::Sample c_lo = expand(static_cast<uint16_t>(0x0841u * (0x86u + 1u)));
    const zref::material::Sample c_hi = expand(static_cast<uint16_t>(0x0841u * (0x85u + 1u)));

    zref::material::Sample base;
    base.r = 0x20;
    base.g = 0x40;
    base.b = 0x60;
    base.a = 255;

    auto want = [&](uint8_t recipe, const zref::material::Sample& c) -> uint32_t {
      zref::material::Sample s[3] = {c, c, c};
      zref::material::Ledger led{};
      const zref::material::Out o =
          zref::material::combine(recipe, /*weight=*/128, s, /*count=*/3, base,
                                  /*frag_tag=*/0, &led);
      return (static_cast<uint32_t>(o.r) << 16) | (static_cast<uint32_t>(o.g) << 8) | o.b;
    };

    // AUX FRAGMENTS ARE EXCLUDED, and the reason is not convenience. A
    // fragment with `frag_aux_i` set takes its third sample from the AUX sheet
    // responder (str 0x88), not from the palette, so "all three samples are the
    // same palette colour" -- the premise that makes this expectation
    // computable at all -- is false for them. The test drives aux on
    // `(i % 3) == 0`.
    //
    // A first version did not exclude them and reported 3 mismatches, all
    // recipe 6, at fragments 6, 30 and 54 -- every one of them a multiple of
    // three. That was the expectation being incomplete, not the island being
    // wrong, and it is the second time in this check that a naive model looked
    // like a defect. Modelling the aux path here would mean re-deriving the
    // sheet arithmetic in the test, which is the duplication R15 was about.
    int matched_lo = 0, matched_hi = 0, mismatched = 0, skipped_aux = 0;
    for (std::size_t i = 0; i < g_clut_rgb.size(); ++i) {
      if ((g_clut_idx[i] % 3) == 0) {
        ++skipped_aux;
        continue;
      }
      const uint8_t recipe = static_cast<uint8_t>(g_clut_idx[i] % 8);
      const uint32_t got = g_clut_rgb[i];
      if (got == want(recipe, c_lo))
        ++matched_lo;
      else if (got == want(recipe, c_hi))
        ++matched_hi;
      else {
        if (mismatched < 3)
          std::printf("    fragment %d recipe %u: got 0x%06X, want 0x%06X or 0x%06X\n",
                      g_clut_idx[i], recipe, got, want(recipe, c_lo), want(recipe, c_hi));
        ++mismatched;
      }
    }

    std::printf(
        "  CLUT exact vs zref::material::combine: low-byte %d, high-byte %d, "
        "mismatched %d, aux-skipped %d\n",
        matched_lo, matched_hi, mismatched, skipped_aux);

    check(mismatched == 0,
          "every CLUT fragment retires EXACTLY what zref::material::combine "
          "makes of its palette colour under its own recipe -- fetch, byte "
          "select, palette lookup, 565-to-888 expansion and the material "
          "arithmetic are all exact, not merely non-zero",
          0, mismatched);
    check(matched_lo > 0,
          "and at least one fragment's colour was actually compared, so this is "
          "not vacuously satisfied by an empty set",
          1, matched_lo > 0 ? 1 : 0);

    // AN HONEST GAP, PRINTED RATHER THAN ASSERTED. This workload addresses only
    // EVEN texels -- `matched_hi` is 0 -- so it never asks for the high byte of
    // a halfword and therefore does not exercise the byte-select repair at all.
    //
    // Asserting `matched_hi > 0` here would be asserting a property of the
    // COORDINATES this test happens to generate, not of the island, and it
    // would fail for a reason that has nothing to do with the machine. Forcing
    // odd texels needs control over the coordinate after the perspective divide
    // and the planner's scaling, which this test does not have.
    //
    // So the byte-select repair is established by the RTL argument in D23 and
    // by its own mutation, NOT by this check. Said out loud because a reader
    // seeing "exact colour" pass could otherwise assume it covers that too.
    if (matched_hi == 0)
      std::printf(
          "  NOTE: no fragment addressed an odd texel, so the byte "
          "select is NOT exercised by this workload\n");
  }

  // ======================= AND THE BILINEAR HALF, EXACTLY ==================
  // R5's remaining half. This could not be written at all until three-channel
  // sequencing existed: the island filtered ONE channel and replicated it to
  // all three outputs, so every direct-colour fragment came out grey and there
  // was nothing to compare against but "non-zero".
  //
  // WHAT MAKES IT COMPUTABLE. All four texels of a bilinear sample come from
  // the same memory pattern 0x8586, so they are identical -- and a bilinear
  // filter of four equal values is that value, whatever the fractions are. The
  // sample colour is therefore 0x8586 unpacked as RGB565 and expanded by
  // replication, exactly as the lane now does per channel:
  //
  //     r5 = 16 -> 132,  g6 = 44 -> 178,  b5 = 6 -> 49
  //
  // Note that this is deliberately NOT a test of the filter weights -- equal
  // texels make the fractions unobservable. It tests channel EXTRACTION,
  // expansion, ordering and the material arithmetic. The fractions carrying
  // correctly is a separate property and this test does not establish it.
  {
    zref::material::Sample c;
    c.r = 132;
    c.g = 178;
    c.b = 49;
    c.a = 255;

    zref::material::Sample base;
    base.r = 0x20;
    base.g = 0x40;
    base.b = 0x60;
    base.a = 255;

    auto want = [&](uint8_t recipe) -> uint32_t {
      zref::material::Sample s3[3] = {c, c, c};
      zref::material::Ledger led{};
      const zref::material::Out o =
          zref::material::combine(recipe, /*weight=*/128, s3, /*count=*/3, base,
                                  /*frag_tag=*/0, &led);
      return (static_cast<uint32_t>(o.r) << 16) | (static_cast<uint32_t>(o.g) << 8) | o.b;
    };

    int matched = 0, mismatched = 0, skipped_aux = 0, grey = 0;
    for (std::size_t i = 0; i < g_bil_rgb.size(); ++i) {
      if ((g_bil_idx[i] % 3) == 0) {
        ++skipped_aux;
        continue;
      }
      const uint32_t got = g_bil_rgb[i];
      const uint8_t r = static_cast<uint8_t>(got >> 16);
      const uint8_t g = static_cast<uint8_t>(got >> 8);
      const uint8_t b = static_cast<uint8_t>(got);
      if (r == g && g == b) ++grey;
      if (got == want(static_cast<uint8_t>(g_bil_idx[i] % 8)))
        ++matched;
      else {
        if (mismatched < 3)
          std::printf("    bilinear fragment %d recipe %u: got 0x%06X, want 0x%06X\n", g_bil_idx[i],
                      g_bil_idx[i] % 8, got, want(static_cast<uint8_t>(g_bil_idx[i] % 8)));
        ++mismatched;
      }
    }

    std::printf(
        "  BILINEAR exact vs zref::material::combine: matched %d, "
        "mismatched %d, aux-skipped %d, grey %d\n",
        matched, mismatched, skipped_aux, grey);

    check(mismatched == 0,
          "every BILINEAR fragment retires EXACTLY what zref::material::combine "
          "makes of its filtered texel under its own recipe -- channel "
          "extraction, replication expansion, three-channel ordering and the "
          "material arithmetic are all exact",
          0, mismatched);
    check(grey == 0,
          "and NONE of them is grey -- before three-channel sequencing the "
          "island filtered one channel and replicated it, so every "
          "direct-colour fragment had r == g == b and this check could not "
          "have been written",
          0, grey);
  }

  // ======================= INGRESS-TO-EGRESS IDENTITY ======================
  // THE CHECK THAT WOULD HAVE CAUGHT THE CARRIAGE BUG IN ONE RUN.
  //
  // Every other check in this file is an aggregate: a count of jobs, of
  // lookups, of samples. Aggregates are exactly what the bug hid behind --
  // 64 fragments went in and 64 came out, every block's counter moved, and
  // the totals looked plausible while only 25 distinct fragments existed and
  // one of them was delivered 24 times. A histogram cannot see that. Identity
  // can, and it costs one vector.
  //
  // Each fragment carries a unique tag (0x1000 + i). The island must return
  // each one EXACTLY ONCE, and -- because FRAGROB retires in allocation order
  // -- in submission order. That is the whole contract, and it is asserted
  // per fragment rather than as a population statistic.
  // MUTATION EVIDENCE -- this gate is known to DETECT, not merely to pass.
  // Two mutations were applied to zhao_texture_island_top.sv and the results
  // recorded here, because a green check is not evidence until it has been
  // shown to go red on the fault it claims to cover:
  //
  //   `fr_f_ctx = fr_f_ctx_in` -- the EXACT historical bug, reading the
  //   boundary tap instead of the token-indexed store:
  //       38 missing, 38 duplicated, 63 out of order
  //       jobs by recipe 0 0 4 4 0 0 30 112
  //
  //   `fc_rp = fc_wp` -- reading live ingress at the second read point:
  //       63 missing, 11 duplicated, 52 foreign
  //       jobs by recipe all zero
  //
  // THE POINT: under the first mutation the OLD aggregate checks still pass.
  // `moved >= 2` sees four counters moved and is satisfied; the old
  // single-fragment colour check samples a fragment that happens to be
  // non-zero. The identity gate fails immediately and unambiguously. That
  // difference is the entire argument for it.
  {
    int missing = 0, duplicated = 0, out_of_order = 0, foreign = 0;
    std::vector<int> seen(kFrags, 0);
    for (size_t i = 0; i < g_retired_tags.size(); ++i) {
      const int ix = static_cast<int>(g_retired_tags[i]) - 0x1000;
      if (ix < 0 || ix >= kFrags) {
        ++foreign;
        continue;
      }
      if (seen[ix]++ > 0) ++duplicated;
      if (static_cast<int>(i) != ix) ++out_of_order;
    }
    for (int i = 0; i < kFrags; ++i)
      if (seen[i] == 0) ++missing;

    std::printf(
        "  identity: %d missing, %d duplicated, %d out of order, "
        "%d foreign\n",
        missing, duplicated, out_of_order, foreign);

    int max_disp = 0;
    for (size_t i = 0; i < g_retired_tags.size(); ++i) {
      int dd = static_cast<int>(g_retired_tags[i]) - 0x1000 - static_cast<int>(i);
      if (dd < 0) dd = -dd;
      if (dd > max_disp) max_disp = dd;
    }
    std::printf("  max displacement from submission order: %d\n", max_disp);

    check(foreign == 0, "every retired tag is one this test SUBMITTED", 0, foreign);
    check(duplicated == 0, "no fragment is retired TWICE", 0, duplicated);
    check(missing == 0, "no submitted fragment is LOST", 0, missing);

    // ===================== THE ORDERING BOUNDARY (audit R6) ================
    // This used to be an OPEN DEFECT with a regression guard. The audit was
    // right that a bounded defect is not an ordering guarantee:
    //
    //     ... 48 49 50 51 56 60 57 61 62 63 58 59 52 53 54 55
    //
    // 10 fragments out of place, maximum displacement 8, and the test asserted
    // `max_disp <= 8` so it could not silently worsen. That is a guard, not a
    // boundary, and raster and blend semantics need the boundary.
    //
    // WHAT WAS ACTUALLY WRONG, and it is not what the old comment guessed. The
    // permutation is real and it is not FRAGROB's: FRAGROB retires strictly in
    // allocation order, so the disorder is already present at its input, from
    // the variable-latency services ahead of it -- and then MATERIAL.COMBINE
    // reorders again, deliberately, because a one-sample recipe finishes before
    // a three-sample one that started earlier and holding it back would cost
    // throughput for nothing.
    //
    // So the fix is NOT to make the interior ordered. It is to ORDER AT THE
    // EDGE: the island stamps a submission sequence at admission, carries it
    // through the combiner beside the caller's tag, and restores order in a
    // reorder buffer at its own output. The buffer cannot overflow -- FRAGROB
    // admits at most 64 fragments, so at most 64 sequence numbers are live and
    // each has its own slot -- so the combiner never stalls for it.
    //
    // The old comment attributed the defect to the "misplaced ordering
    // boundary" of the recovery architecture and said repairing it was that
    // rearchitecture rather than a patch. That was too pessimistic by one
    // idea: moving the boundary does not require moving the allocation.
    std::printf("  fragments out of submission order at the boundary: %d\n", out_of_order);
    check(out_of_order == 0,
          "the island returns fragments in STRICT SUBMISSION ORDER at its "
          "boundary -- not within a bound, exactly",
          0, out_of_order);
    check(max_disp == 0, "so no fragment is displaced at all", 0, max_disp);

    // AND THE REORDERING REALLY HAPPENED. Without this the check above passes
    // just as well on a workload that never reordered, and an ordering boundary
    // that was never exercised is not evidence of anything. The counter is the
    // number of times a fragment completed while an earlier one was still
    // missing; the old measurement says that is at least ten.
    const uint32_t held = d.cnt_reorder_held_o;
    std::printf("  fragments that completed early and WAITED at the boundary: %u\n", held);
    check(held > 0,
          "and the boundary was actually exercised -- fragments did complete "
          "out of order inside and were held, so strict order out is a "
          "restoration rather than a workload that never disturbed it",
          1, held > 0 ? 1 : 0);
  }

  // ======================= THE CLUT PATH, RESOLVED =========================
  // Every CLUT-class fragment used to retire BLACK while every bilinear one did
  // not, with `cnt_palette_lookups_o` moving healthily the whole time. Two
  // independent faults, both invisible to a lookup count:
  //
  //   1. THE TEST never loaded a palette. It sent LD_WRITE 64 times and no
  //      LD_BEGIN, so `loading_r` was never set and not one write landed; and
  //      LD_END grants residency only if all 256 entries arrived, so 64 would
  //      not have been enough either.
  //
  //   2. THE ISLAND asked the palette the wrong question. `lu_slot_i` and
  //      `lu_gen_i` were OVERLAPPING slices of the response routing token --
  //      the "slot" was the low two bits of the "generation", and the
  //      generation was FRAGROB's residency counter, which has nothing to do
  //      with a palette upload. A palette slot and generation are a MATERIAL
  //      BINDING; they now travel with the fragment.
  //
  // Measured, before and after: 96 lookups / 96 stale / 0 cold, every CLUT
  // fragment black -> 96 lookups / 0 stale / 0 cold, all 32 non-zero.
  //
  // Asserted as residency, not just as colour, because "the lookup happened"
  // and "the lookup found its palette" are different facts and only the first
  // was ever observable.
  check(g_nz_by_cls[0] == 32, "every CLUT fragment carries a NON-ZERO colour", 32, g_nz_by_cls[0]);
  check(d.cnt_palette_lookups_o == 96, "the CLUT path performs every lookup its fragments ask for",
        96, static_cast<int>(d.cnt_palette_lookups_o));
  check(d.cnt_palette_stale_o == 0,
        "and every one of them RESOLVES -- no lookup is answered stale, which "
        "is the miss indication that used to be mistaken for a working path",
        0, static_cast<int>(d.cnt_palette_stale_o));
  check(d.cnt_palette_cold_o == 0, "and none is answered cold", 0,
        static_cast<int>(d.cnt_palette_cold_o));

  // The completion merger gives the palette strict priority because PALETTE_RES
  // cannot be back-pressured -- its answer is valid for one clock. That is safe
  // only while FRAGROB's response ready is tied high. This flag is the tripwire
  // on that cross-module assumption: if a palette answer is ever produced and
  // not taken, a fragment waits forever and the island wedges in allocation
  // order, which is a hang rather than a wrong pixel.
  check(d.err_rsp_dropped_o == 0,
        "no sample response was produced and dropped -- the merger's "
        "unconditional-consumer assumption still holds",
        0, static_cast<int>(d.err_rsp_dropped_o));

  if (g_failed) {
    std::printf("[island_composed_directed] %d/%d checks FAILED\n", g_failed, g_checks);
    return 1;
  }
  std::printf("[island_composed_directed] %d checks passed\n", g_checks);
  return 0;
}
