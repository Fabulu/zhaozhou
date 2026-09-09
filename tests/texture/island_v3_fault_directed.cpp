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
// It WAS deliberately unregistered while it was red -- a red suite hides
// regressions, and that was a known, documented, dated hole rather than a
// surprise. The repairs landed, `add_test(NAME island_v3_fault_directed)` went
// in with them, and this paragraph used to still say "deliberately NOT
// registered ... yet". Stale by exactly the fault §3.1 below catches in itself:
// a comment describing an obligation the tree no longer has is as confidently
// wrong as a current file compared to an old measurement.
#include "Vzhao_texture_island_v3_top.h"
// The internal-scope headers, for the §3.1 C injection at the end of main. The
// expander's output register is reached as
// `rootp->zhao_texture_island_v3_top->u_expand__DOT__wq_overflow_o`, which
// exists only because the target is verilated with --public-flat-rw.
#include "Vzhao_texture_island_v3_top___024root.h"
#include "Vzhao_texture_island_v3_top_zhao_texture_island_v3_top.h"

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
  d.frag_class_i = 0;  // CLS_CLUT -- legal
  d.frag_pal_slot_i = 0;
  d.frag_pal_gen_i = 0;
  d.bind_base_i = 0;
  d.bind_mode_i = 0x00006600u;  // CLUT8 / nearest / 64x64
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
  zhao::check(clean_invalid == 0, "clean traffic leaves err_class_invalid_o clear", 0,
              clean_invalid);

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
              "AND err_class_invalid_o MOVED -- the observable the repair "
              "exists to produce",
              1, d.err_class_invalid_o > clean_invalid ? 1 : 0);

  // EXACTNESS, not just movement (post-fit brief §3.1). The counter delta must
  // equal the number of ACCEPTED illegal beats -- no more, no less.
  //
  // "It moved" is satisfied by a counter that fires once per illegal fragment,
  // and equally by one that fires on every cycle the pin happens to be 3, or
  // twice per beat, or once for the whole burst. Those are different circuits
  // and only one of them is the contract. This is the counters-see-what-
  // pictures-cannot law applied to a diagnostic instead of to a job count.
  zhao::check(d.err_class_invalid_o - clean_invalid == static_cast<uint32_t>(bad),
              "and it moved by EXACTLY the number of accepted illegal beats -- "
              "not once per cycle the pin was 3, not twice per fragment, and "
              "not once for the burst",
              bad, static_cast<long long>(d.err_class_invalid_o - clean_invalid));

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
    // RESET FIRST. Phases 1-3 offer sample_count=1 fragments and this harness
    // deliberately does not model the texture memory (`fill_data_valid_i` is
    // never raised), so those fragments are admitted and can never complete.
    // Running the zero-work probe behind them would measure a machine already
    // clogged by the harness, not the zero-work path -- which is exactly the
    // confounded reading this phase existed to avoid.
    d.rst_n = 0;
    tick(d);
    tick(d);
    d.rst_n = 1;
    tick(d);

    const int kWrap = 300;  // ~4.7x the 64-slot space
    int submitted = 0, retired = 0, dup = 0, foreign = 0;
    std::vector<int> seen(kWrap + 8, 0);

    for (int cyc = 0; cyc < 60000 && retired < kWrap; ++cyc) {
      d.frag_valid_i = (submitted < kWrap) ? 1 : 0;
      d.frag_class_i = 0;
      d.frag_sample_count_i = 0;  // ZERO-WORK: ready at admission
      d.frag_aux_i = 0;
      d.frag_ctx_i = static_cast<uint64_t>(submitted);
      d.out_ready_i = 1;
      d.eval();

      const bool acc = d.frag_valid_i && d.frag_ready_o;
      if (d.out_valid_o && d.out_ready_i) {
        const int tag = static_cast<int>(d.out_tag_o);
        if (tag < 0 || tag >= kWrap)
          ++foreign;
        else if (seen[tag]++)
          ++dup;
        ++retired;
      }
      tick(d);
      if (acc) ++submitted;
    }
    d.frag_valid_i = 0;
    d.frag_sample_count_i = 1;
    d.eval();

    std::printf(
        "  zero-work wrap: submitted %d, retired %d (slot space 64, "
        "so ~%dx wrap)\n",
        submitted, retired, submitted / 64);

    // WHERE DOES IT STOP? Counters localise a stall that a retired-count
    // cannot: each of these is a different stage of the same path, so the
    // first one that reads zero names the boundary.
    std::printf("    rcp %u | persp %u | plan %u | cache %u | dispatch %u\n", d.cnt_rcp_completed_o,
                d.cnt_persp_fragments_o, d.cnt_plan_accepted_o,
                d.cnt_cache_hits_o + d.cnt_cache_misses_o, d.cnt_dispatch_accepted_o);
    // `cnt_combine_jobs_o` is an ARRAY OF EIGHT -- one job count per recipe --
    // not a scalar. Printing it with %u formats the array's address, which is
    // how it produced 2,558,523,520 on one run and a different large number on
    // the next. I recorded that as "looks unreset or X-propagating"; it was
    // neither. Sum the eight.
    unsigned combine_jobs_total = 0;
    for (int r = 0; r < 8; ++r) combine_jobs_total += d.cnt_combine_jobs_o[r];
    std::printf("    expander frags %u | combine jobs %u | phases %u | refused %u | live peak %u\n",
                d.cnt_fragments_o, combine_jobs_total, d.cnt_combine_phases_o,
                d.cnt_combine_refused_o, d.cnt_live_peak_o);

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
    zhao::check(foreign == 0, "and every retired tag is one this phase submitted", 0, foreign);
  }

  // ---- PHASE 5: THE CONSUMER STALLS MID-FLIGHT (brief §3.2) ----------------
  // "consumer stalls immediately after a read reservation" is on the brief's
  // list of minimum additional composed cases. A sink that shuts while work is
  // in flight must not lose a fragment or retire one twice: the reservation is
  // a credit, and a credit that is spent while the consumer is closed is the
  // reservation-versus-acceptance defect M6 already cost this repository once.
  {
    d.rst_n = 0;
    tick(d);
    tick(d);
    d.rst_n = 1;
    tick(d);

    const int kN = 120;
    int submitted = 0, retired = 0, dup = 0, foreign = 0, stalled_accepts = 0;
    std::vector<int> seen(kN + 8, 0);

    for (int cyc = 0; cyc < 40000 && retired < kN; ++cyc) {
      // Shut the sink for a 40-cycle window in the middle of the burst, while
      // fragments are certainly in flight -- not at the drain, where a stall
      // proves nothing.
      const bool sink_open = !(cyc >= 400 && cyc < 440);
      d.out_ready_i = sink_open ? 1 : 0;

      d.frag_valid_i = (submitted < kN) ? 1 : 0;
      d.frag_class_i = 0;
      d.frag_sample_count_i = 0;
      d.frag_aux_i = 0;
      d.frag_ctx_i = static_cast<uint64_t>(submitted);
      d.eval();

      const bool acc = d.frag_valid_i && d.frag_ready_o;
      if (acc && !sink_open) ++stalled_accepts;
      if (d.out_valid_o && d.out_ready_i) {
        const int tag = static_cast<int>(d.out_tag_o);
        if (tag < 0 || tag >= kN)
          ++foreign;
        else if (seen[tag]++)
          ++dup;
        ++retired;
      }
      tick(d);
      if (acc) ++submitted;
    }
    d.frag_valid_i = 0;
    d.out_ready_i = 1;
    d.eval();

    std::printf("  consumer stall: submitted %d, retired %d, accepted while SHUT %d\n", submitted,
                retired, stalled_accepts);

    zhao::check(stalled_accepts > 0,
                "the island kept ACCEPTING while the sink was shut -- otherwise "
                "the stall never exercised the credit path and this phase is "
                "just a slower version of phase 4",
                1, stalled_accepts > 0 ? 1 : 0);
    zhao::check(retired == submitted, "every fragment survived the stall", submitted, retired);
    zhao::check(dup == 0, "and none was retired twice across it", 0, dup);
    zhao::check(foreign == 0, "and no foreign tag appeared", 0, foreign);
  }

  // ---- PHASE 6: RESET MID-FLIGHT, THEN A FRESH NAMESPACE (brief §5) --------
  // "the owner's local quiet cannot certify that external producers have
  // stopped delivering old responses. The current owner source explicitly
  // leaves those acknowledgment phases outside its interface."
  //
  // This drives the part that IS observable from the boundary: assert reset
  // while fragments are in flight, then run a fresh batch whose tags cannot
  // collide with the abandoned ones. Nothing from before the reset may retire
  // afterwards.
  {
    const int kPre = 40, kPost = 80;
    const int kTagBase = 500;  // disjoint from the pre-reset tags

    d.rst_n = 0;
    tick(d);
    tick(d);
    d.rst_n = 1;
    tick(d);
    d.out_ready_i = 1;

    // Offer pre-reset traffic and do NOT drain it.
    int pre = 0;
    for (int cyc = 0; cyc < 4000 && pre < kPre; ++cyc) {
      d.frag_valid_i = 1;
      d.frag_class_i = 0;
      d.frag_sample_count_i = 0;
      d.frag_ctx_i = static_cast<uint64_t>(pre);
      d.out_ready_i = 0;  // sink shut: keep them in the machine
      d.eval();
      if (d.frag_valid_i && d.frag_ready_o) ++pre;
      tick(d);
    }
    d.frag_valid_i = 0;
    d.eval();

    // RESET while they are in flight.
    d.rst_n = 0;
    tick(d);
    tick(d);
    d.rst_n = 1;
    tick(d);
    d.out_ready_i = 1;

    int post = 0, retired = 0, stale = 0;
    for (int cyc = 0; cyc < 40000 && retired < kPost; ++cyc) {
      d.frag_valid_i = (post < kPost) ? 1 : 0;
      d.frag_class_i = 0;
      d.frag_sample_count_i = 0;
      d.frag_ctx_i = static_cast<uint64_t>(kTagBase + post);
      d.eval();
      const bool acc = d.frag_valid_i && d.frag_ready_o;
      if (d.out_valid_o && d.out_ready_i) {
        const int tag = static_cast<int>(d.out_tag_o);
        if (tag < kTagBase) ++stale;  // a pre-reset fragment came back
        ++retired;
      }
      tick(d);
      if (acc) ++post;
    }
    d.frag_valid_i = 0;
    d.eval();

    std::printf("  reset schedule: pre %d (abandoned), post %d, retired %d, stale %d\n", pre, post,
                retired, stale);

    zhao::check(pre > 0,
                "pre-reset fragments were actually admitted and left in flight, "
                "so the reset had something to abandon",
                1, pre > 0 ? 1 : 0);
    zhao::check(post >= kPost,
                "and the island accepts a full fresh batch after reset -- it "
                "did not come back wedged",
                kPost, post);
    zhao::check(stale == 0,
                "NOTHING from before the reset retired afterwards. A tag below "
                "the fresh base can only be an abandoned fragment surviving a "
                "namespace it no longer belongs to",
                0, stale);
    zhao::check(retired == kPost, "and every post-reset fragment retired", kPost, retired);
  }

  // The metajoin shadow is asserted in island_v3_composed_directed, NOT here.
  // This probe drives sample_count = 0 and never serves texture memory, so no
  // response ever reaches the common stream and the shadow performs ZERO
  // comparisons. The non-vacuity check caught that on its first run -- without
  // it, "0 mismatches" would have been reported as agreement when it was zero
  // over zero.

  // ---- §3.1 B and C: BOTH DISCHARGED, and the old text was stale -----------
  // This block used to say "neither port has a defined event to inject:
  // `fr_wq_overflow` and `fr_id_error` have no drivers at all". That description
  // stopped being true when repairs B and C landed, and a test reporting an
  // obligation about RTL that no longer exists is the same fault as comparing a
  // current file to an old measurement -- confident, specific, and about
  // something else.
  //
  // B is covered and NAMED. `err_fragrob_id_error_o` is the OR of all SIX of
  // v3own's identity-error counters -- range, stale, unsolicited, duplicate,
  // issue, final -- listed by name at the driver. The brief's requirement was
  // that the port say which of the family it covers rather than summing an
  // unstated subset; it covers all six. (An earlier version summed three of six,
  // which is the omission the brief named.) Whether each of those six can
  // individually fire is v3own's own 541-check suite's business, not this
  // probe's.
  //
  // C is covered and the event is now DEMONSTRATED, not merely defined.
  // `err_fragrob_wq_overflow_o` is set from the expander's real
  // capacity-violation counter -- a queue holding more entries than it owns --
  // and explicitly NOT from `valid && !ready`, because conflating backpressure
  // with overflow is how the brief says a port gets tied to zero and called
  // preserved.
  //
  // That counter is unreachable by any legal stimulus, so it was demonstrated
  // with a committed mutant: `frag_expand_overflow_control` drives
  // `tests/mutants/zhao_texture_frag_expand_mutant.sv`, whose only substantive
  // change is `fq_full_c`'s `>=` becoming `>`, and watches the counter reach 36
  // with six entries accepted into a four-deep queue.
  //
  // The ISLAND-LEVEL propagation from that counter to this sticky bit used to
  // be recorded here as "STILL UNTESTED", on the grounds that it is a one-line
  // `if (exp_wq_overflow != 0)` and that arguing from its simplicity is the
  // move that hides defects. Correct as far as it went -- but "recorded as
  // untested" is a note, not a gate, and the hop is now exercised below.
  std::printf(
      "  §3.1 B DISCHARGED: err_fragrob_id_error_o ORs all six named "
      "v3own identity counters (range/stale/unsol/dup/issue/final)\n");
  std::printf(
      "  §3.1 C DISCHARGED: err_fragrob_wq_overflow_o reads the "
      "expander's real capacity-violation event, shown to fire in "
      "frag_expand_overflow_control (counter 36, 6 accepted into 4)\n");

  // ---- §3.1 C, SECOND HALF: the island-level hop, by INJECTION -------------
  //
  // The expander's capacity-violation counter cannot be moved by any legal
  // stimulus -- it needs a queue holding more entries than it owns, which the
  // correct full-guard forbids. That is why the counter itself was demonstrated
  // with a committed mutant rather than with input. The ISLAND hop needs the
  // counter nonzero without a mutated island, so it is injected: the expander's
  // output register is written directly through --public-flat-rw, which is a
  // verilate flag rather than a source edit (and therefore writable while
  // zhao_texture_island_v3_top.sv sits in a running fit's closure).
  //
  // `wq_overflow_o` is a flip-flop assigned only under its violation condition,
  // with no else arm, so an injected value HOLDS across ticks instead of being
  // recomputed away.
  {
    auto& expander = *d.rootp->zhao_texture_island_v3_top;

    // NON-VACUITY. If the sticky were already set, every check below would pass
    // with the injection doing nothing at all -- the exact shape of the
    // zero-over-zero agreement this file's §3.1 A comparison had to guard.
    zhao::check(d.err_fragrob_wq_overflow_o == 0,
                "sticky starts clear, so the injection below is not vacuous", 0,
                d.err_fragrob_wq_overflow_o);

    // NEGATIVE CONTROL, before the positive one. Injecting ZERO must leave the
    // bit clear. Without this the test cannot tell "the hop carries a nonzero
    // count" from "the bit latches on being looked at".
    expander.u_expand__DOT__wq_overflow_o = 0;
    tick(d);
    zhao::check(d.err_fragrob_wq_overflow_o == 0,
                "a zero counter does NOT set the sticky bit", 0,
                d.err_fragrob_wq_overflow_o);

    // THE HOP.
    expander.u_expand__DOT__wq_overflow_o = 1;
    tick(d);
    zhao::check(d.err_fragrob_wq_overflow_o == 1,
                "a nonzero capacity-violation count sets err_fragrob_wq_overflow_o",
                1, d.err_fragrob_wq_overflow_o);

    // AND IT IS STICKY. Clear the counter; the bit must HOLD. This asserts the
    // CORRECT behaviour rather than the fault -- "the counter fires" would stop
    // being assertable the moment anything upstream is repaired, which is the
    // trap CLAUDE.md names: do not write a test that asserts the bug.
    expander.u_expand__DOT__wq_overflow_o = 0;
    for (int i = 0; i < 8; ++i) tick(d);
    zhao::check(d.err_fragrob_wq_overflow_o == 1,
                "the sticky bit HOLDS after the counter returns to zero", 1,
                d.err_fragrob_wq_overflow_o);
    std::printf(
        "  §3.1 C SECOND HALF: island hop exercised by injection -- clear, "
        "zero-is-clear, set, and held\n");
  }

  return zhao::report_and_exit("island_v3_fault_directed");
}
