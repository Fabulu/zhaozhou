// warp_sparse_ordinal_mutant_driver.cpp -- AN INVERTED-POLARITY POSITIVE
// CONTROL. IT PASSES WHEN THE DEFECT IS DETECTED.
//
// THIS IS EVIDENCE ABOUT AN INSTRUMENT, NOT ABOUT THE DESIGN. It says nothing
// whatever about whether `zhao_field_warp_adapter` is correct. What it says is
// that `warp_sparse_ordinal_directed` would NOTICE if the adapter were reading
// window positions as ordinals -- which is the claim owner ruling R168 demands
// be demonstrated rather than argued, and which a test that passes both ways
// could never support.
//
// ---------------------------------------------------------------------------
// WHY THIS CONTROL HAD TO EXIST AT ALL
// ---------------------------------------------------------------------------
// R168's defect is not reachable with legal stimulus against production. The
// wrong wiring is a WIRING, not an input: you cannot present a vertex that
// makes an ordinal-indexed host behave like a window-indexed one. CLAUDE.md's
// rule for exactly this situation:
//
//   > "A guard you cannot reach with legal stimulus needs a COMMITTED MUTANT
//   > ... the break must not be a temporary edit to production RTL, because
//   > that is a live-tree hazard AND it leaves nothing behind: the next person
//   > inherits the same argument and no evidence."
//
// So the break is committed:
// `tests/mutants/zhao_field_host_v2_winidx_mutant.sv` publishes its results
// window-indexed, and `tests/mutants/tb_warp_field_chain_winidx_mutant.sv`
// instantiates it in place of the real host and is otherwise the production
// bench, substitution for substitution.
//
// ---------------------------------------------------------------------------
// THE STIMULUS IS NOT MERELY SIMILAR, IT IS THE SAME CODE
// ---------------------------------------------------------------------------
// The program, the vertex, the loader words and the software oracle all come
// from tests/field/warp_sparse_program.hpp, which
// `warp_sparse_ordinal_directed.cpp` also includes. CLAUDE.md's mismatched-pose
// lesson is the reason: a comparison between two arrangements that were set up
// separately measures the setup. Here the ONLY difference between the two runs
// is which host module is instantiated.
//
// ---------------------------------------------------------------------------
// WHAT THE MUTANT IS EXPECTED TO DO, AND WHY IT IS THE NASTY VERSION
// ---------------------------------------------------------------------------
// The mutant's COMPLETION RULE IS UNTOUCHED, so it still answers StOk. It does
// not refuse, it does not fault, it does not set `out_incomplete`. It returns a
// successful run whose ordinal 5 is the cleared zero from R20 instead of the
// program's nz' from R21 -- a whole, confident, wrong vertex. That is R168's
// defect in its actual shape, and a mutant that merely errored would have
// proven nothing about ordinals.
//
// Since R168's repair the adapter also reads `resp_present_i`, so there are now
// TWO independent teeth in the trap and this driver records which ones bit.
// Requiring only that the WRONG ANSWER DID NOT GET PUBLISHED is the honest
// assertion: demanding a particular mechanism would make this control fail the
// day the repair is improved.

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vtb_warp_field_chain_winidx_mutant.h"
#include "warp_sparse_program.hpp"

namespace ws = warp_sparse;

using Dut = Vtb_warp_field_chain_winidx_mutant;

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;
  ws::resetAll(top);

  const ws::Prog SPARSE = ws::sparseProgram();
  const ws::Prog CONTIG = ws::contiguousProgram();
  const ws::Stim s = ws::sparseVertex();

  // =========================================================================
  // 1. THE CONTIGUOUS NEGATIVE CONTROL, AND IT COMES FIRST ON PURPOSE
  // =========================================================================
  // The mutant must be INDISTINGUISHABLE from production under a contiguous
  // program. If it were not, this control would be firing on some incidental
  // difference between the two files -- a stale copy, a botched rename, a
  // mutation that broke the engine rather than the indexing -- and its verdict
  // on the sparse case would mean nothing.
  //
  // This is CLAUDE.md's macro-mutant lesson in a different costume: two
  // combiner mutants passed while measuring unmutated production, and the check
  // that separated them was compiling with the selector disengaged and
  // confirming the output DIFFERED. Same question here: is the mutation
  // actually engaged, and engaged only where it should be?
  {
    const bool loaded = ws::loadProg(top, 3, CONTIG);
    zhao::check(loaded, "1: CONTIGUOUS loaded into the mutant's slot 3", 1, loaded ? 1 : 0);
    const ws::Outcome o = ws::runVertex(top, 3, s);
    zhao::check(o.published,
                "1: the mutant publishes a vertex for a CONTIGUOUS program -- it is a "
                "working host, not a broken one", 1, o.published ? 1 : 0);
    zhao::check(o.n[2] == 99,
                "1: and its answer is CORRECT there. For a contiguous map ordinal k IS "
                "window lane k, so the mutation is invisible -- which is precisely why "
                "every contiguous Warp test in this tree missed R168.",
                99, static_cast<std::uint32_t>(o.n[2]));
    zhao::check(o.p[0] == 11 && o.p[1] == 22 && o.p[2] == 33,
                "1: the displacement ordinals are correct in the mutant too", 11,
                static_cast<std::uint32_t>(o.p[0]));
    std::printf("  mutant CONTIGUOUS: present=0x%02X normal=(%d,%d,%d)  <- indistinguishable\n",
                top.h_resp_present_o, o.n[0], o.n[1], o.n[2]);
  }

  // =========================================================================
  // 2. THE SPARSE CASE -- THE DISCRIMINATION, INVERTED
  // =========================================================================
  {
    const bool loaded = ws::loadProg(top, 2, SPARSE);
    zhao::check(loaded, "2: SPARSE loaded into the mutant's slot 2", 1, loaded ? 1 : 0);

    const std::uint32_t absent0 = top.a_absent_outputs_o;
    const std::uint32_t verts0 = top.a_vertices_o;
    const std::uint32_t inc0 = top.h_out_incomplete_o;

    const ws::Outcome o = ws::runVertex(top, 2, s);
    const ws::gw::Result r = ws::softwareAnswer(SPARSE, s);

    // The software engine is unaffected by the mutation: it is the same
    // reference both drivers consult, and it still says 99.
    zhao::check(r.direction[2] == 99,
                "2: the software engine still says ordinal 5 is 99 -- the reference is "
                "not what moved", 99, static_cast<std::uint32_t>(r.direction[2]));

    // ---- THE MUTANT SAID THE RUN SUCCEEDED --------------------------------
    // This is the assertion that makes the control worth having. The defect
    // does not announce itself.
    zhao::check(top.h_out_incomplete_o == inc0,
                "2: the mutant host reported NO incomplete result -- it believes this "
                "run succeeded. The defect is a plausible wrong number, not an error.",
                inc0, top.h_out_incomplete_o);
    zhao::check(top.h_resp_present_o != 0x3F,
                "2: but its ORDINAL present mask is short, because it is really a "
                "WINDOW mask -- window lane 5 (R20) was never written",
                0x1F, top.h_resp_present_o);

    // ---- THE INVERTED ASSERTION -------------------------------------------
    // PASSES WHEN THE WRONG VALUE IS DETECTED. Note what is required: that the
    // wrong answer was NOT PUBLISHED as a correct one. Which tooth caught it
    // -- the presence mask or the ordinal word -- is reported below but not
    // demanded, because pinning the mechanism would make this control fail the
    // day the repair is strengthened.
    const bool published_the_wrong_normal = o.published && (o.n[2] == 0);
    const bool published_the_right_normal = o.published && (o.n[2] == 99);

    zhao::check(!published_the_right_normal,
                "2 INVERTED: the window-indexed host did NOT get away with it. If this "
                "check fails, the SPARSE stimulus cannot tell an ordinal-indexed "
                "response from a window-indexed one, and "
                "`warp_sparse_ordinal_directed` is not evidence about R168 at all.",
                0, published_the_right_normal ? 1 : 0);

    zhao::check(!published_the_wrong_normal,
                "2 INVERTED: and it did not publish the cleared R20 zero as a normal "
                "component either -- W10's 'an absent output must not look like a zero "
                "result', which is the exact harm R168 describes",
                0, published_the_wrong_normal ? 1 : 0);

    const bool presence_tooth = (top.a_absent_outputs_o > absent0);
    const bool no_false_result = (top.a_vertices_o == verts0);
    zhao::check(no_false_result,
                "2 INVERTED: the adapter did NOT count a real Field result for a record "
                "it could not stand behind", verts0, top.a_vertices_o);

    std::printf("  mutant SPARSE: present=0x%02X normal=(%d,%d,%d) published=%d\n",
                top.h_resp_present_o, o.n[0], o.n[1], o.n[2], o.published ? 1 : 0);
    std::printf("  which tooth bit: resp_present_i refusal=%s (absent_outputs %u -> %u)\n",
                presence_tooth ? "YES" : "no", absent0, top.a_absent_outputs_o);
    std::printf("  NOTE: this run PASSING means the discriminator WORKS. It is not a\n"
                "        statement that the design is correct -- see\n"
                "        warp_sparse_ordinal_directed for that.\n");
  }

  return zhao::report_and_exit("warp_sparse_ordinal_winidx_mutant");
}
