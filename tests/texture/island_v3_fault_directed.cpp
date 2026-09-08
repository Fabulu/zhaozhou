// island_v3_fault_directed.cpp
//
// ---------------------------------------------------------------------------
// POSITIVE FAULT TESTS FOR THE V3 TOP'S EVIDENCE PORTS
// ---------------------------------------------------------------------------
// Owner brief, 2026-09-08, §3.2:
//
//   "For every retained error port: first show clean traffic leaves it clear;
//    then inject the specific condition and show that exact observable changes;
//    finally restore legitimate traffic and show continued operation. ...
//    A permanently-zero flag must not pass a healthy-run test and thereby be
//    declared implemented."
//
// That last sentence describes something already in this repository.
// `island_composed_directed.cpp` asserts
//
//     check(d.err_class_invalid_o == 0,
//           "and no fragment arrived with an invalid class at the pin", ...)
//
// and it PASSES -- because `err_class_invalid_o` increments on
// `fr_alloc_valid && f_class_bad_c`, and `fr_alloc_valid` lost its only driver
// when the FRAGROB instance was deleted. The port is wired to a constant zero.
// A check that the flag stayed clear cannot tell that apart from a flag that
// works.
//
// So this file does the other half. It is written to FAIL against the current
// RTL and to pass after the §3.1 repairs land, which is the only ordering that
// proves the test can see the thing it claims to test.
//
// It is deliberately NOT registered with add_test() yet: a red suite hides
// regressions, and this is a known, documented, dated hole rather than a
// surprise. `add_test` goes in with the repair, in the same commit.
#include "Vzhao_texture_island_v3_top.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#include "verilated.h"

#include "../harness/zhao_sim.hpp"

namespace {

using Dut = Vzhao_texture_island_v3_top;

void tick(Dut& d) {
  d.clk = 0;
  d.eval();
  d.clk = 1;
  d.eval();
}

// Everything the top needs held at a benign value, so the only thing under
// test is the one field each phase perturbs.
void quiesce(Dut& d) {
  d.frag_valid_i = 0;
  d.frag_depth_i = 0x010000;
  d.frag_u_over_w_i = 0x00008000;
  d.frag_v_over_w_i = 0x00008000;
  d.frag_sample_count_i = 1;
  d.frag_binding_i = 0;
  d.frag_lod_i = 0;
  d.frag_recipe_i = 0;
  d.frag_weight_i = 0xFF;
  d.frag_ctx_i = 0;
  d.frag_aux_i = 0;
  d.frag_base_rgb_i = 0x808080;
  d.frag_base_a_i = 0xFF;
  d.frag_class_i = 0;          // CLS_CLUT -- legal
  d.frag_pal_slot_i = 0;
  d.frag_pal_gen_i = 0;
  d.bind_base_i = 0;
  d.bind_mode_i = 0x00006600u; // CLUT8 / nearest / 64x64
  d.fill_ready_i = 1;
  d.fill_data_valid_i = 0;
  d.fill_data_i = 0;
  d.pal_ld_valid_i = 0;
  d.pal_ld_op_i = 0;
  d.pal_ld_slot_i = 0;
  d.pal_ld_gen_i = 0;
  d.pal_ld_idx_i = 0;
  d.pal_ld_rgb565_i = 0;
  d.pal_ld_crc_ok_i = 1;
  d.sheet_rvalid_i = 0;
  d.sheet_tag_i = 0;
  d.sheet_str_i = 0;
  d.sheet_rtok_i = 0;
  d.sheet_ready_i = 1;
  d.out_ready_i = 1;
}

// Offer `n` fragments at `cls`, letting the machine breathe. Returns how many
// were actually accepted -- a phase that admitted nothing proves nothing.
int offer(Dut& d, int n, uint8_t cls, int budget = 4000) {
  int sent = 0;
  for (int cyc = 0; cyc < budget && sent < n; ++cyc) {
    d.frag_valid_i = 1;
    d.frag_class_i = cls;
    d.frag_ctx_i = static_cast<uint64_t>(0x1000 + sent);
    d.eval();
    if (d.frag_valid_i && d.frag_ready_o) ++sent;
    tick(d);
  }
  d.frag_valid_i = 0;
  d.eval();
  for (int i = 0; i < 200; ++i) tick(d);
  return sent;
}

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Dut d;

  d.clk = 0;
  quiesce(d);
  d.rst_n = 0;
  tick(d);
  tick(d);
  d.rst_n = 1;
  tick(d);

  // ---- PHASE 1: clean traffic leaves every flag clear ----------------------
  const int clean = offer(d, 8, /*CLS_CLUT=*/0);
  zhao::check(clean > 0,
              "clean traffic was actually admitted -- a phase that admits "
              "nothing makes every check below vacuous",
              1, clean > 0 ? 1 : 0);
  const uint32_t clean_invalid = d.err_class_invalid_o;
  zhao::check(clean_invalid == 0,
              "clean traffic leaves err_class_invalid_o clear",
              0, clean_invalid);

  // ---- PHASE 2: INJECT the fault, and require the observable to MOVE -------
  // CLS_ERR (2'd3) is not a legal request class. §3.1 repair A: the counter
  // must see the actual ingress admission and THAT beat's raw class.
  const int bad = offer(d, 8, /*CLS_ERR=*/3);
  zhao::check(bad > 0,
              "the invalid-class fragments were actually offered and accepted "
              "at ingress -- otherwise 'the counter did not move' would just "
              "mean 'nothing arrived'",
              1, bad > 0 ? 1 : 0);
  zhao::check(d.err_class_invalid_o > clean_invalid,
              "AND err_class_invalid_o MOVED. It counts on `fr_alloc_valid && "
              "f_class_bad_c`, and `fr_alloc_valid` lost its only driver when "
              "FRAGROB was deleted -- so today this port is a constant zero and "
              "this check FAILS. That is the point: the healthy-run check in "
              "island_composed_directed passes against exactly this defect",
              1, d.err_class_invalid_o > clean_invalid ? 1 : 0);

  // ---- PHASE 3: legitimate traffic still works afterwards ------------------
  // A block that reacts to a fault by wedging is not fault-tolerant, it is just
  // differently broken.
  const int after = offer(d, 8, /*CLS_CLUT=*/0);
  zhao::check(after > 0,
              "and legitimate traffic is still admitted after the fault -- the "
              "island did not wedge on an invalid class",
              1, after > 0 ? 1 : 0);

  // ---- PHASE 4: THE ZERO-WORK LIFETIME PROBE (brief §5.1) ------------------
  // The brief classifies this as a SOURCE-PROVEN mismatch between two
  // completion domains, with reachability explicitly NOT established:
  //
  //   "owner may externally retire -> no legitimate front-end reader for it
  //    remains" ... is not established by the inspected composition.
  //
  // v3own makes an owner with `adm_req_i == 0` ready at admission -- correct for
  // its leaf contract, since it owes no TMU or AUX source. But the composed top
  // still sends that fragment through RCP, PERSPUV and the expander, which hold
  // its token and can read owner-keyed sidecars.
  //
  // So: drive far more than OWNERS(64) fragments with sample_count 0, forcing
  // the slot space to WRAP repeatedly while front-end stages are mid-flight. If
  // an owner can be freed and re-admitted while the expander still references
  // it, tags come back wrong or duplicated. This does not prove the implication
  // holds -- absence of a trace is not a proof -- but a FOUND trace would be a
  // reproduced defect, which is what the brief says nobody has yet.
  {
    const int kWrap = 300;              // ~4.7x the 64-slot space
    int submitted = 0, retired = 0, dup = 0, foreign = 0;
    std::vector<int> seen(kWrap + 8, 0);

    for (int cyc = 0; cyc < 60000 && retired < kWrap; ++cyc) {
      d.frag_valid_i = (submitted < kWrap) ? 1 : 0;
      d.frag_class_i = 0;
      d.frag_sample_count_i = 0;        // ZERO-WORK: ready at admission
      d.frag_aux_i = 0;
      d.frag_ctx_i = static_cast<uint64_t>(submitted);
      d.out_ready_i = 1;
      d.eval();

      const bool acc = d.frag_valid_i && d.frag_ready_o;
      if (d.out_valid_o && d.out_ready_i) {
        const int tag = static_cast<int>(d.out_tag_o);
        if (tag < 0 || tag >= kWrap) ++foreign;
        else if (seen[tag]++) ++dup;
        ++retired;
      }
      tick(d);
      if (acc) ++submitted;
    }
    d.frag_valid_i = 0;
    d.frag_sample_count_i = 1;
    d.eval();

    std::printf("  zero-work wrap: submitted %d, retired %d (slot space 64, "
                "so ~%dx wrap)
", submitted, retired, submitted / 64);

    zhao::check(submitted >= 200,
                "the zero-work burst actually wrapped the 64-slot owner space "
                "several times -- a run that never wrapped could not reach the "
                "reuse schedule this phase exists to probe",
                1, submitted >= 200 ? 1 : 0);
    zhao::check(retired == submitted,
                "every zero-work fragment retired -- none was lost to a slot "
                "recycled underneath it",
                submitted, retired);
    zhao::check(dup == 0,
                "and none was retired TWICE, which is what a slot freed while a "
                "front-end stage still held it would produce",
                0, dup);
    zhao::check(foreign == 0,
                "and every retired tag is one this phase submitted",
                0, foreign);
  }

  // ---- The other two ports, stated as obligations --------------------------
  // §3.1 B and C. These are not yet testable by injection because neither port
  // has a defined event to inject: `fr_wq_overflow` and `fr_id_error` have no
  // drivers at all. Asserting they stay zero would be the very mistake the
  // brief names, so this records the obligation instead of faking coverage.
  std::printf("  OBLIGATION (brief 3.1 B): err_fragrob_id_error_o must name "
              "which of range/stale/unsol/dup/issue/final it covers\n");
  std::printf("  OBLIGATION (brief 3.1 C): err_fragrob_wq_overflow_o needs a "
              "real capacity-violation event or a versioned retirement; "
              "valid && !ready is backpressure, not overflow\n");

  return zhao::report_and_exit("island_v3_fault_directed");
}
