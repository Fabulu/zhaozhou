// warp_sparse_ordinal_directed.cpp -- THE ONE STIMULUS IN THE TREE THAT CAN
// TELL A CORRECTLY-WIRED WARP ADAPTER FROM A WRONGLY-WIRED ONE.
//
// Owner ruling R168, 2026-09-20, adopting packet W1's finding:
//
//   > "`zhao_field_warp_adapter`'s host-side ports match the OLD host
//   > name-for-name and the NEW host in MEANING. Wire it to the old host and
//   > it elaborates, runs, and passes every gate -- while reading window
//   > positions as ordinals. Nothing in either port list distinguishes correct
//   > from wrong."
//
// R168's first action, and it says it is not optional: *"The SPARSE case must
// become a committed, named test, not a case inside W1's bench. It is the only
// discriminator that exists and it must survive the packet that wrote it."*
// This file is that test.
//
// ---------------------------------------------------------------------------
// WHAT IT ASSERTS, AND WHAT IT DELIBERATELY DOES NOT
// ---------------------------------------------------------------------------
// It asserts the CORRECT behaviour -- ordinal 5 arrives from R21 -- and never
// the defect. CLAUDE.md: *"Do not write a test that asserts the bug. 'The
// counter fires on the swap' passes only while the defect exists."* The
// wrong-wiring half lives in its own inverted-polarity control,
// `warp_sparse_ordinal_winidx_mutant`, and the two are evidence only together.
//
// ---------------------------------------------------------------------------
// THE NEGATIVE CONTROL IS INSIDE THIS FILE, AND IT IS THE INTERESTING PART
// ---------------------------------------------------------------------------
// Case C runs the SAME experiment with a CONTIGUOUS program and asserts that
// the ordinal-5 value it produces is ALSO what a window-indexed read would
// produce. That is not a pass; it is a demonstration that the contiguous case
// is BLIND, recorded as a property of the tree rather than as a sentence in a
// comment. Every other Warp test here uses a contiguous program, so every other
// Warp test here is blind to R168 -- and that is why the defect reached the
// coordinator branch with every gate green.
//
// ENFORCES: fpga/rtl/field/zhao_field_warp_adapter.sv

#include <cstdint>
#include <cstdio>

#include "verilated.h"

#include "Vtb_warp_field_chain.h"
#include "warp_sparse_program.hpp"

namespace ws = warp_sparse;

using Dut = Vtb_warp_field_chain;

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut top;
  ws::resetAll(top);

  const ws::Prog SPARSE = ws::sparseProgram();
  const ws::Prog CONTIG = ws::contiguousProgram();
  const ws::Stim s = ws::sparseVertex();

  // =========================================================================
  // A. THE DISCRIMINATOR
  // =========================================================================
  {
    const bool loaded = ws::loadProg(top, 2, SPARSE);
    zhao::check(loaded, "A: SPARSE loaded into resident slot 2", 1, loaded ? 1 : 0);

    const std::uint32_t inc0 = top.h_out_incomplete_o;
    const std::uint32_t bad0 = top.h_bad_image_o;
    const std::uint32_t absent0 = top.a_absent_outputs_o;

    const ws::Outcome o = ws::runVertex(top, 2, s);
    const ws::gw::Result r = ws::softwareAnswer(SPARSE, s);

    zhao::check(SPARSE.decoded_ok, "A: the SPARSE image decoded through the real validator",
                1, SPARSE.decoded_ok ? 1 : 0);
    zhao::check(!r.poison(), "A: the software engine accepted this vertex", 0,
                r.poison() ? 1 : 0);
    zhao::check(o.published, "A: the chain published a coherent warped vertex", 1,
                o.published ? 1 : 0);

    // ---- THE HEADLINE ASSERTION -------------------------------------------
    // Ordinal 5 is nz'. It was computed into R21. Window lane 5 is R20 and
    // nothing ever wrote it. 99 is the ordinal-indexed answer; 0 is the
    // window-indexed one. There is no third possibility, and no contiguous
    // program can produce this pair of numbers.
    zhao::check(o.n[2] == 99,
                "A: ORDINAL 5 CAME FROM R21, NOT FROM THE UNWRITTEN WINDOW LANE 5. "
                "This is owner ruling R168's discriminator: 99 means the response is "
                "ordinal-indexed, 0 would mean it is window-indexed.",
                99, static_cast<std::uint32_t>(o.n[2]));

    // The software engine is an independent implementation of the same law, so
    // agreeing with it is a second, differently-derived vote for the same
    // number rather than a restatement of the first.
    for (int k = 0; k < 3; ++k) {
      zhao::check(o.n[k] == r.direction[k],
                  "A: the replacement normal agrees with the software engine",
                  static_cast<std::uint32_t>(r.direction[k]),
                  static_cast<std::uint32_t>(o.n[k]));
      zhao::check(o.p[k] == r.position[k],
                  "A: the warped position agrees with the software engine",
                  static_cast<std::uint32_t>(r.position[k]),
                  static_cast<std::uint32_t>(o.p[k]));
    }
    zhao::check(o.p[0] == 11 && o.p[1] == 22 && o.p[2] == 33,
                "A: the three displacement ordinals are unaffected by the sparse map", 11,
                static_cast<std::uint32_t>(o.p[0]));

    // ---- and the run was ordinary, which is what makes it dangerous --------
    zhao::check(top.h_out_incomplete_o == inc0,
                "A: the host produced ALL SIX declared ordinals -- this is a COMPLETE, "
                "SUCCESSFUL run, not a refusal somebody would have noticed",
                inc0, top.h_out_incomplete_o);
    zhao::check(top.h_bad_image_o == bad0,
                "A: and the sparse OUTPUT_MAP was not a load-time refusal either -- "
                "every source register lies inside the capture window",
                bad0, top.h_bad_image_o);

    // =======================================================================
    // B. R168's SECOND ACTION: THE ADAPTER CONSUMES `resp_present_o`
    // =======================================================================
    zhao::check(top.h_resp_present_o == 0x3F,
                "B: all six ORDINALS present -- an ordinal-indexed mask, not a window "
                "one. The adapter now READS this; before R168 it decided on a zero "
                "status alone, which cannot tell a value from a hole.",
                0x3F, top.h_resp_present_o);
    zhao::check(top.a_absent_outputs_o == absent0,
                "B: and absent_outputs_o stayed put on a whole record", absent0,
                top.a_absent_outputs_o);
    zhao::check(top.a_vertices_o >= 1,
                "B: the adapter counted a real Field result", 1,
                top.a_vertices_o >= 1 ? 1 : 0);

    std::printf("  SPARSE: window=0x%02X present=0x%02X count=%u, normal=(%d,%d,%d)\n",
                top.h_resp_window_o, top.h_resp_present_o, top.h_resp_count_o, o.n[0],
                o.n[1], o.n[2]);
  }

  // =========================================================================
  // C. THE BLINDNESS OF THE CONTIGUOUS CASE, MEASURED
  // =========================================================================
  // Not a pass. A demonstration that the rest of the Warp suite CANNOT see
  // R168's defect, recorded so that nobody has to take the claim on trust.
  //
  // For a contiguous map, ordinal k is register (out_base + k), which IS
  // window lane k. The two readings return the same word by construction. If
  // this ever stops being true the assertion below fails and the comment above
  // is wrong -- which is the only honest way to write down a negative result.
  {
    const bool loaded = ws::loadProg(top, 3, CONTIG);
    zhao::check(loaded, "C: CONTIGUOUS loaded into resident slot 3", 1, loaded ? 1 : 0);
    const ws::Outcome o = ws::runVertex(top, 3, s);
    zhao::check(o.published, "C: the contiguous program published too", 1,
                o.published ? 1 : 0);

    bool contiguous_is_blind = true;
    for (int k = 0; k < 6; ++k) {
      const int window_position = CONTIG.out_regs[k] - CONTIG.out_base();
      if (window_position != k) contiguous_is_blind = false;
    }
    zhao::check(contiguous_is_blind,
                "C: for the CONTIGUOUS map, ordinal k IS window lane k for all six "
                "ordinals -- so a contiguous program is STRUCTURALLY INCAPABLE of "
                "telling an ordinal-indexed response from a window-indexed one. Every "
                "other Warp test in this tree is contiguous. That is why R168's defect "
                "passed every gate.",
                1, contiguous_is_blind ? 1 : 0);

    bool sparse_discriminates = false;
    for (int k = 0; k < 6; ++k) {
      const int window_position = SPARSE.out_regs[k] - SPARSE.out_base();
      if (window_position != k) sparse_discriminates = true;
    }
    zhao::check(sparse_discriminates,
                "C: and for the SPARSE map at least one ordinal does NOT sit at its own "
                "window position, which is the entire reason this file exists", 1,
                sparse_discriminates ? 1 : 0);

    zhao::check(o.n[2] == 99, "C: the contiguous run is correct as well -- it is blind, "
                "not broken", 99, static_cast<std::uint32_t>(o.n[2]));
  }

  return zhao::report_and_exit("warp_sparse_ordinal_directed");
}
