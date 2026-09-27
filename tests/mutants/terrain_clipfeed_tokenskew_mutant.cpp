// terrain_clipfeed_tokenskew_mutant.cpp -- THE INVERTED-POLARITY DRIVER for
// `tests/mutants/zhao_terrain_clipfeed_tokenskew_mutant.sv`.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS EVIDENCE ABOUT
// ---------------------------------------------------------------------------
// It is evidence about the INSTRUMENT, not about the design.
//
// `terrain_clipfeed_mat_directed` section 7 asserts that I34's per-triangle
// material token arrives at GEOM.CLIP's door WITH ITS OWN TRIANGLE. Against
// unmutated production that assertion cannot fail: the block latches the token
// by the same register enable as the triangle's corners, so there is no
// alignment invariant to break. A check that cannot fail is a claim, and
// CLAUDE.md is blunt about it -- "a detector that has not been shown to FIRE
// has not been tested", and "a guard you cannot reach with legal stimulus needs
// a COMMITTED MUTANT".
//
// So this run puts the SAME bench against a wrapper whose only change is to
// hand the production block the PREVIOUS accepted triangle's token, and
// **PASSES WHEN THE EQUALITY FAILS**.
//
// ---------------------------------------------------------------------------
// WHY THIS PARTICULAR MUTATION, AND NOT A CONVENIENT ONE
// ---------------------------------------------------------------------------
// It is the `u_geom_tidq` fault, reproduced in this arm. That queue sat
// permanently one behind and bound 74 of 75 triangles to their predecessor's
// arena descriptor -- and it survived because the ids stayed IN RANGE and
// DECODED CLEANLY, so every range guard passed and every gate stayed green.
// `reports/DECISION-20260927-I34-MATERIAL-CARRIER.md` chose to widen the
// record rather than run an aligned side FIFO for exactly that reason.
//
// The skewed token here is a perfectly well-formed v1 token with a correct
// tag. The triangle count, the emitted count, `src_id_mismatch_o`,
// `mat_backed_o` and `mat_id_orphan_o` are all identical to the clean run --
// ASSERTED BELOW, not assumed -- so this proves section 7 catches a fault that
// nothing else in the block's instrumentation can see. That is the point.
//
// ---------------------------------------------------------------------------
// I DO NOT ASSERT THE BUG
// ---------------------------------------------------------------------------
// CLAUDE.md: "Do not write a test that asserts the bug ... Assert the CORRECT
// behaviour and keep the detector's positive control separate." That is why
// this lives in its own file with its own ctest and does not touch section 7.
// Section 7 asserts the record HOLDS; this asserts that section 7's comparison
// is capable of noticing when it does not. Repairing the mutant is never a
// goal -- if production ever made this pass with zero mismatches, the check
// would have gone blind and this file is what says so.
//
// THE NEGATIVE CONTROL is the same bench without `ZHAO_MUT_TOKEN_SKEW`:
// `terrain_clipfeed_mat_directed`, which must report ZERO mismatches. The two
// builds differ in one preprocessor symbol and nothing else.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

// Its OWN verilate PREFIX, so this target cannot share the clean run's
// object directory -- the trap PACKET-PROTOCOL names for every new switch.
#include "Vtb_terrain_clipfeed_skew.h"

using SkewDut = Vtb_terrain_clipfeed_skew;

#include "zhao_sim.hpp"

using zhao::check;

namespace {

constexpr uint32_t kW = 0x0010'0000u;
constexpr uint32_t kShade = 32768u;

// `zhao_material_token_pkg::ZMT_TAG_V1`, and the same asymmetric stimulus law
// section 7 uses. Kept identical on purpose: a control that varied the
// stimulus as well as the DUT would not isolate the DUT.
constexpr uint32_t kZmtTagV1 = 0xE1u;

uint32_t token_of(int k) {
  const uint8_t a = static_cast<uint8_t>(0x11u + 0x27u * static_cast<uint32_t>(k));
  const uint8_t b = static_cast<uint8_t>(0x93u - 0x3Du * static_cast<uint32_t>(k));
  const uint8_t w = static_cast<uint8_t>(0x05u + 0x5Bu * static_cast<uint32_t>(k));
  return (kZmtTagV1 << 24) | (static_cast<uint32_t>(a) << 16) |
         (static_cast<uint32_t>(b) << 8) | static_cast<uint32_t>(w);
}

uint16_t src_of(int k) { return static_cast<uint16_t>(0x1000 + k); }

void reset(SkewDut& d) {
  d.rst_n = 0;
  d.t_valid = 0;
  d.l_valid = 0;
  d.u_valid = 0;
  d.t_src = 0;
  d.t_w = kW;
  d.l_shade = kShade;
  d.l_degenerate = 0;
  d.mat_set = 0;
  d.mat_id = 0;
  d.t_material_token = 0;
  d.o_ready = 1;
  d.eval();
  for (int i = 0; i < 8; ++i) zhao::tick(d);
  d.rst_n = 1;
  d.eval();
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  SkewDut dv;
  reset(dv);

  // A legal terrain material, so the arm behaves exactly as it does in the
  // clean run and the ONLY difference is the skew.
  dv.mat_set = 0x1234'5678u;
  dv.mat_id = 0x9ABCu;
  dv.eval();

  constexpr int kN = 12;
  uint32_t seen_tok = 0;
  uint16_t seen_src = 0;
  uint32_t seen_by_k[kN] = {0};
  uint32_t mismatches = 0;
  uint32_t emitted_expected = 0;

  for (int k = 0; k < kN; ++k) {
    // Offer triangle k with token k.
    dv.t_material_token = token_of(k);
    dv.t_valid = 1;
    dv.l_valid = 1;
    dv.u_valid = 1;
    dv.t_src = src_of(k);
    dv.t_w = kW;
    dv.l_shade = kShade;
    dv.l_degenerate = 0;
    for (int i = 0; i < 64; ++i) {
      dv.eval();
      if (dv.t_valid && dv.t_ready && dv.l_ready && dv.u_ready) {
        zhao::tick(dv);
        break;
      }
      zhao::tick(dv);
    }
    dv.t_valid = 0;
    dv.l_valid = 0;
    dv.u_valid = 0;

    ++emitted_expected;
    bool granted = false;
    for (int i = 0; i < 4000; ++i) {
      dv.eval();
      if (dv.o_valid && dv.o_ready) {
        seen_tok = dv.o_material_token;
        seen_src = static_cast<uint16_t>(dv.o_src_id);
        granted = true;
      }
      if (dv.emitted >= emitted_expected) break;
      zhao::tick(dv);
    }
    if (!granted) {
      check(false, "the mutant still EMITS every triangle -- a mutation that dropped "
                   "beats would not be a fair control for a MIS-ATTRIBUTION",
            1u, 0u);
      break;
    }
    seen_by_k[k] = seen_tok;
    if (seen_tok != token_of(k) || seen_src != src_of(k)) ++mismatches;
  }

  // THE PRECISE SIGNATURE, not just a total. A count of mismatches would also
  // be produced by a mutation that SCRAMBLED the token, and that is a different
  // and much easier fault to catch. This asserts the token each triangle
  // received is exactly its PREDECESSOR's -- the u_geom_tidq shape.
  uint32_t one_behind = 0;
  for (int k = 1; k < kN; ++k) {
    if (seen_by_k[k] == token_of(k - 1)) ++one_behind;
  }

  // =========================================================================
  // THE INVERTED ASSERTION
  // =========================================================================
  check(mismatches == static_cast<uint32_t>(kN),
        "MUTANT (inverted polarity): the per-triangle token equality FIRES on EVERY one "
        "of the twelve triangles -- so section 7's silence against production is a "
        "measurement and not a claim",
        static_cast<uint32_t>(kN), mismatches);

  // Triangle 0 is counted above too, and its value is stated rather than waved
  // past: skew_q resets to zero, so triangle 0 receives 32'd0, which is not a
  // v1 token, so it mismatches as well. That is why the total is kN and not
  // kN-1. The signature below covers triangles 1..kN-1, where what arrives is a
  // REAL token belonging to the WRONG triangle.
  check(one_behind == static_cast<uint32_t>(kN) - 1u,
        "MUTANT: every triangle from the second on received EXACTLY its PREDECESSOR's "
        "token -- the u_geom_tidq signature, a well-formed value on the wrong triangle "
        "rather than a corruption",
        static_cast<uint32_t>(kN) - 1u, one_behind);

  check(mismatches >= 1u,
        "MUTANT: at least one mismatch was seen at all -- a zero here would mean the "
        "ifdef never selected the mutant and this run measured production, which is the "
        "exact silent failure CLAUDE.md records for macro-selected mutants",
        1u, mismatches >= 1u ? 1u : 0u);

  // =========================================================================
  // AND THE MUTATION IS INVISIBLE TO EVERY OTHER INSTRUMENT IN THE BLOCK
  // =========================================================================
  // This is the half that makes the control worth having. The fault class
  // section 7 exists to catch is the one that passes every range check, so
  // these must all read EXACTLY as they do in the clean run.
  check(dv.emitted == static_cast<uint32_t>(kN),
        "MUTANT: every triangle was still emitted -- the skew loses nothing",
        static_cast<uint32_t>(kN), dv.emitted);
  check(dv.triangles == dv.emitted,
        "MUTANT: nothing is stuck in the block", dv.emitted, dv.triangles);
  check(dv.src_id_mismatch == 0,
        "MUTANT: the three-way join reports NO disagreement -- the block's own "
        "alignment counter is BLIND to this fault, which is exactly why the "
        "per-triangle token equality is the instrument that matters",
        0u, dv.src_id_mismatch);
  check(dv.mat_id_orphan == 0,
        "MUTANT: the orphan rule is silent -- the skewed token is well-formed", 0u,
        dv.mat_id_orphan);
  check(dv.mat_backed == static_cast<uint32_t>(kN),
        "MUTANT: every triangle still declared its material -- the {set, id} identity "
        "is untouched by the mutation",
        static_cast<uint32_t>(kN), dv.mat_backed);
  check((seen_tok >> 24) == kZmtTagV1,
        "MUTANT: the token that arrived was a well-formed v1 token -- it is the WRONG "
        "triangle's, not a corrupt one, which is the whole fault class",
        kZmtTagV1, seen_tok >> 24);

  dv.final();
  return zhao::report_and_exit("terrain_clipfeed_tokenskew_mutant");
}
