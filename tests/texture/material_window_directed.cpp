// material_window_directed.cpp -- does MATERIAL.RESOLVE's answer belong to the
// triangle it is published for?
//
// ---------------------------------------------------------------------------
// WHAT IS AT RISK HERE, AND WHAT IS NOT
// ---------------------------------------------------------------------------
// The RESOLVE is not at risk and is not re-checked here.
// `material_resolve_rtl_directed.cpp` differences `zhao_material_resolve`
// against `zref::material::Resolver` in 91 checks and owns that question.
// This driver plays the resolver's ROLE and tests the thing the window adds,
// which is a JOIN -- and a join is a timing property, so the checks below are
// written about timing and never about arithmetic.
//
// The three faults that matter, each of which leaves every handshake healthy:
//
//   * A LATE PUBLICATION. The window lets a triangle of material B through
//     while B's answer is still in flight, so B's triangles are shaded with
//     A's record. Every counter balances, because no counter looks at the
//     field that moved -- CLAUDE.md's metadata-swap chapter, one block over.
//   * AN EARLY PUBLICATION. The window switches to B while A's triangles are
//     still between GEOM.CLIP and the shell's door. Same consequence, opposite
//     edge, and equally invisible downstream.
//   * A SPURIOUS RESOLVE. The window re-resolves a material it already holds.
//     The picture is byte-identical -- a resolve is idempotent -- and the
//     meshlet loop's clock budget is quietly spent twice. This is the
//     "counters see what pictures cannot" law, and it is why `resolves_o` is
//     asserted EXACTLY rather than as "at least one".
//
// ---------------------------------------------------------------------------
// HOW THE JOIN IS OBSERVED WITHOUT ASSERTING THE BUG
// ---------------------------------------------------------------------------
// The driver models the downstream span itself: every triangle the window
// emits is pushed into a queue with the material that was PUBLISHED at the
// moment it was emitted, and is later disposed of through `d_leave_i` (the
// door) or `d_reject_i` (GEOM.CLIP's non-ACCEPT verdict). At each disposal the
// driver compares the published material AT THAT INSTANT against the one the
// triangle carried. That is the correct-behaviour assertion -- "the record
// holds" -- not "the detector fires", so it keeps its value after the defect
// is gone.
//
// `err_unpublished_o` and `err_occupancy_underflow_o` are the block's two
// structural guards and BOTH ARE FIRED HERE with legal stimulus at the block's
// own ports: the disposal events are inputs, so a departure that never had an
// arrival is a perfectly legal thing for a bench to present. That is the
// `t_ack_i` shape and not the `wq_overflow_o` shape -- neither guard owes a
// committed mutant, and the reason is worth stating because the difference is
// what decides whether one is needed.

#include <cstdint>
#include <cstdio>
#include <deque>

#include "verilated.h"

#include "Vzhao_material_window.h"

#include "zhao_sim.hpp"

namespace {

int fails = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    ++fails;
    std::printf("FAIL: %s\n", what);
  }
}

struct Answer {
  bool has_record = true;
  uint8_t sample_count = 1;
  uint8_t recipe = 0;
  uint8_t weight = 0x5A;
  uint8_t binding = 1;
  uint8_t modes = 0x01;   // tmu_mode 1 -> class NEAR under the default mapping
  bool selector_overflow = false;
  int latency = 3;        // clocks between the request's acceptance and the answer
};

// What a triangle was shaded with, recorded at the instant the window emitted
// it. The queue is the driver's model of GEOM.CLIP..the door.
struct InFlight {
  uint8_t sample_count;
  uint8_t recipe;
  uint8_t weight;
  uint8_t binding;
  uint8_t response_class;
};

class Bench {
 public:
  Bench() {
    top_.clk = 0;
    top_.rst_n = 0;
    top_.t_valid_i = 0;
    top_.t_material_set_i = 0;
    top_.t_material_id_i = 0;
    top_.t_quality_tier_i = 0;
    top_.t_ready_i = 1;
    top_.d_enter_i = 0;
    top_.d_reject_i = 0;
    top_.d_leave_i = 0;
    top_.req_ready_i = 1;
    top_.rsp_valid_i = 0;
    top_.rsp_status_i = 0;
    top_.rsp_has_record_i = 0;
    top_.rsp_sample_count_i = 0;
    top_.rsp_material_recipe_i = 0;
    top_.rsp_recipe_weight_i = 0;
    top_.rsp_base_binding_i = 0;
    top_.rsp_selector_overflow_i = 0;
    top_.rsp_sample0_modes_i = 0;
    top_.eval();
    for (int i = 0; i < 4; ++i) tick();
    top_.rst_n = 1;
    tick();
  }

  Vzhao_material_window& top() { return top_; }
  uint64_t clocks() const { return clocks_; }
  int mismatches() const { return mismatches_; }
  int disposed() const { return disposed_; }

  void set_answer(const Answer& a) { answer_ = a; }

  // One clock, with the resolver model and the downstream-span model both
  // running inside it. The span drains one triangle per clock once it has any,
  // which is deliberately SLOWER than the window can emit -- a drain that
  // completes instantly would never exercise ST_DRAIN at all.
  void tick() {
    // ---- the resolver model, on the settled pre-edge values --------------
    top_.eval();
    bool req_fire = top_.req_valid_o && top_.req_ready_i;
    bool rsp_fire = top_.rsp_valid_i && top_.rsp_ready_o;

    // ---- the downstream span model ---------------------------------------
    bool emit = top_.t_valid_o && top_.t_ready_i;
    InFlight carried{};
    if (emit) {
      carried.sample_count = top_.pub_sample_count_o;
      carried.recipe = top_.pub_material_recipe_o;
      carried.weight = top_.pub_recipe_weight_o;
      carried.binding = top_.pub_base_binding_o;
      carried.response_class = top_.pub_response_class_o;
      // A triangle emitted with nothing published is the fault this whole
      // block exists to make impossible; record it so the summary is loud.
      if (!top_.pub_valid_o) ++emitted_unpublished_;
    }

    // The span retires one triangle every third clock, which is SLOWER than
    // the window can emit. That is deliberate twice over: it keeps
    // `occupancy_max_o` measurably above one, and it makes a material change
    // wait for a real drain instead of finding the span already empty and
    // reporting a drain cost of zero for a reason that has nothing to do with
    // the block. A model that retired only at depth two would instead DEADLOCK
    // the drain with one triangle stranded -- a bench bug that reads exactly
    // like a hung block.
    bool dispose = !span_.empty() && ((clocks_ % 3) == 0);
    if (dispose) {
      const InFlight& t = span_.front();
      // THE CORRECT-BEHAVIOUR ASSERTION. Not "the detector fired" -- "the
      // record held".
      if (t.sample_count != top_.pub_sample_count_o ||
          t.recipe != top_.pub_material_recipe_o ||
          t.weight != top_.pub_recipe_weight_o ||
          t.binding != top_.pub_base_binding_o ||
          t.response_class != top_.pub_response_class_o) {
        ++mismatches_;
      }
      ++disposed_;
    }

    top_.d_enter_i = emit ? 1 : 0;
    // Retire alternately through the door and through a GEOM.CLIP rejection,
    // so both decrement paths are exercised by ordinary traffic.
    top_.d_leave_i = (dispose && ((disposed_ & 1) == 1)) ? 1 : 0;
    top_.d_reject_i = (dispose && ((disposed_ & 1) == 0)) ? 1 : 0;

    // THE ANSWER IS A ONE-CLOCK PULSE, and the reason is worth writing down
    // because the first version of this model deadlocked on it. The window
    // raises `rsp_ready_o` only in ST_WAIT and LEAVES ST_WAIT on the same edge
    // that takes the answer -- so a model that waits to observe
    // `rsp_valid_i && rsp_ready_o` on a LATER clock never sees the acceptance,
    // holds `rsp_valid_i` high forever, and the next resolve is answered
    // instantly with the PREVIOUS record's fields. Every handshake still looks
    // healthy; the data is a clock late for the rest of the run.
    if (rsp_fire || answered_) {
      top_.rsp_valid_i = 0;
      answered_ = false;
      pending_ = -1;
    } else if (req_fire) {
      pending_ = answer_.latency;
    } else if (pending_ > 0) {
      --pending_;
    } else if (pending_ == 0) {
      top_.rsp_valid_i = 1;
      top_.rsp_has_record_i = answer_.has_record ? 1 : 0;
      top_.rsp_sample_count_i = answer_.sample_count;
      top_.rsp_material_recipe_i = answer_.recipe;
      top_.rsp_recipe_weight_i = answer_.weight;
      top_.rsp_base_binding_i = answer_.binding;
      top_.rsp_sample0_modes_i = answer_.modes;
      top_.rsp_selector_overflow_i = answer_.selector_overflow ? 1 : 0;
      answered_ = true;
    }

    top_.eval();
    top_.clk = 1; top_.eval();
    top_.clk = 0; top_.eval();
    ++clocks_;

    if (emit) span_.push_back(carried);
    if (dispose) span_.pop_front();
    top_.d_enter_i = 0;
    top_.d_leave_i = 0;
    top_.d_reject_i = 0;
  }

  // Offer `n` triangles of one material and run until they are all accepted.
  int offer(uint32_t set, uint16_t id, uint8_t tier, int n, int budget = 4000) {
    top_.t_material_set_i = set;
    top_.t_material_id_i = id;
    top_.t_quality_tier_i = tier;
    top_.t_valid_i = 1;
    int taken = 0;
    int spent = 0;
    while (taken < n && spent < budget) {
      top_.eval();
      if (top_.t_valid_o && top_.t_ready_i) ++taken;
      tick();
      ++spent;
    }
    top_.t_valid_i = 0;
    tick();
    return taken;
  }

  void drain(int budget = 4000) {
    int spent = 0;
    while (!span_.empty() && spent < budget) { tick(); ++spent; }
    for (int i = 0; i < 8; ++i) tick();
  }

  int emitted_unpublished() const { return emitted_unpublished_; }

 private:
  Vzhao_material_window top_;
  Answer answer_{};
  std::deque<InFlight> span_;
  int pending_ = -1;
  bool answered_ = false;
  uint64_t clocks_ = 0;
  int mismatches_ = 0;
  int disposed_ = 0;
  int emitted_unpublished_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);

  int checks = 0;

  // ==========================================================================
  // CASE 1 -- the ordinary life of one material.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.sample_count = 1; a.recipe = 0; a.weight = 0x5A; a.binding = 1;
    a.modes = 0x01;                  // tmu_mode 1
    b.set_answer(a);

    const int taken = b.offer(0xABCD'0001u, 7, 0x20, 12);
    b.drain();

    check(taken == 12, "case1: every offered triangle was eventually passed"); ++checks;
    check(b.top().resolves_o == 1,
          "case1: ONE resolve for twelve triangles of one material -- not twelve"); ++checks;
    check(b.top().switches_o == 1, "case1: exactly one material switch"); ++checks;
    check(b.top().pub_valid_o == 1, "case1: a material is published at rest"); ++checks;
    check(b.top().pub_sample_count_o == 1, "case1: sample_count is the record's"); ++checks;
    check(b.top().pub_recipe_weight_o == 0x5A, "case1: recipe_weight is the record's"); ++checks;
    check(b.top().pub_base_binding_o == 1, "case1: base_binding_selector is the record's"); ++checks;
    check(b.top().pub_response_class_o == 1,
          "case1: tmu_mode 1 maps to response class NEAR under the default mapping"); ++checks;
    check(b.top().no_record_o == 0, "case1: no fault material was published"); ++checks;
    check(b.top().clut_unowned_o == 0, "case1: a NEAR class is not the unowned CLUT case"); ++checks;
    check(b.mismatches() == 0,
          "case1: every disposed triangle was shaded with the material it was emitted with"); ++checks;
    check(b.disposed() >= 10, "case1: the span actually carried triangles"); ++checks;
    check(b.emitted_unpublished() == 0,
          "case1: no triangle was ever emitted with nothing published"); ++checks;
    check(b.top().err_unpublished_o == 0, "case1: the structural guard is silent"); ++checks;
    check(b.top().err_occupancy_underflow_o == 0,
          "case1: the occupancy guard is silent"); ++checks;
    check(b.top().occupancy_max_o >= 2, "case1: the span was measurably occupied"); ++checks;
    check(b.top().drain_stall_cycles_o == 0,
          "case1: the FIRST material costs no drain -- the span starts empty"); ++checks;
    check(b.top().answer_stall_cycles_o > 0,
          "case1: the first material did cost an answer wait"); ++checks;
  }

  // ==========================================================================
  // CASE 2 -- a material CHANGE, which is the join the block exists for.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.sample_count = 1; a.recipe = 0; a.weight = 0x11; a.binding = 3;
    a.modes = 0x02; a.latency = 7;   // tmu_mode 2 -> BIL
    b.set_answer(a);
    b.offer(0x1000'0000u, 1, 0, 6);

    Answer c;
    c.sample_count = 2; c.recipe = 1; c.weight = 0x22; c.binding = 9;
    c.modes = 0x01; c.latency = 11;
    b.set_answer(c);
    const int taken2 = b.offer(0x1000'0000u, 2, 0, 6);
    b.drain();

    check(taken2 == 6, "case2: the second material's triangles all passed"); ++checks;
    check(b.top().resolves_o == 2, "case2: exactly two resolves for two materials"); ++checks;
    check(b.top().switches_o == 2, "case2: exactly two switches"); ++checks;
    check(b.top().drain_stall_cycles_o > 0,
          "case2: the SECOND material paid a drain -- the span had to empty first"); ++checks;
    check(b.mismatches() == 0,
          "case2: no triangle was disposed of under the other material's record"); ++checks;
    check(b.top().pub_base_binding_o == 9, "case2: the second record is the one held"); ++checks;
    check(b.top().pub_recipe_weight_o == 0x22, "case2: the second weight is held"); ++checks;
    check(b.top().pub_response_class_o == 1, "case2: the second class is held"); ++checks;
    check(b.top().err_unpublished_o == 0, "case2: the structural guard is silent"); ++checks;
  }

  // ==========================================================================
  // CASE 3 -- the SAME {set, id} offered again costs NOTHING.
  // ==========================================================================
  {
    Bench b;
    Answer a; a.latency = 5;
    b.set_answer(a);
    b.offer(0x7777'0000u, 0x0042, 0, 4);
    const uint32_t r1 = b.top().resolves_o;
    const uint32_t d1 = b.top().drain_stall_cycles_o;
    b.offer(0x7777'0000u, 0x0042, 0, 4);
    b.drain();
    check(b.top().resolves_o == r1,
          "case3: re-offering the same material issued NO second resolve"); ++checks;
    check(b.top().drain_stall_cycles_o == d1,
          "case3: and cost no drain -- the common case is free"); ++checks;
    check(b.top().switches_o == 1, "case3: one switch, not two"); ++checks;
  }

  // ==========================================================================
  // CASE 4 -- a DIFFERENT SET with the SAME ID is a different material.
  // The set is half the key, and a window that keyed on the id alone would
  // serve draw A's record to draw B and pass every other check in this file.
  // ==========================================================================
  {
    Bench b;
    Answer a; a.binding = 5; a.latency = 2;
    b.set_answer(a);
    b.offer(0xAAAA'0000u, 0x0099, 0, 3);
    Answer c; c.binding = 6; c.latency = 2;
    b.set_answer(c);
    b.offer(0xBBBB'0000u, 0x0099, 0, 3);
    b.drain();
    check(b.top().resolves_o == 2,
          "case4: the same material_id under a different material_set re-resolved"); ++checks;
    check(b.top().pub_base_binding_o == 6, "case4: the second set's record is held"); ++checks;
    check(b.mismatches() == 0, "case4: the join held across the set change"); ++checks;
  }

  // ==========================================================================
  // CASE 5 -- R20's defined fault material. A resolve that finds no record
  // must PUBLISH, be COUNTED, and never stop the stream.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.has_record = false; a.sample_count = 3; a.recipe = 7; a.binding = 0xFF;
    a.latency = 4;
    b.set_answer(a);
    const int taken = b.offer(0x3333'0000u, 0x0001, 0, 5);
    b.drain();
    check(taken == 5, "case5: a denied/missing record does NOT stall the triangle stream"); ++checks;
    check(b.top().no_record_o == 1, "case5: the fault was counted once"); ++checks;
    check(b.top().pub_valid_o == 1, "case5: the fault material is PUBLISHED, not withheld"); ++checks;
    check(b.top().pub_sample_count_o == 0,
          "case5: the fault material takes no sample -- and does not inherit the"
          " response's stale fields"); ++checks;
    check(b.top().pub_base_binding_o == 0, "case5: nor its binding selector"); ++checks;
  }

  // ==========================================================================
  // CASE 6 -- the two counted ABSENCES: a CLUT class has no palette-identity
  // producer in this console, and a 16-bit binding slot narrowed to 8 is a
  // selector overflow. Both are loud rather than silent.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.modes = 0x00;                  // tmu_mode 0 -> CLUT
    a.selector_overflow = true;
    a.latency = 3;
    b.set_answer(a);
    b.offer(0x5555'0000u, 0x0007, 0, 3);
    b.drain();
    check(b.top().pub_response_class_o == 0, "case6: tmu_mode 0 maps to CLUT"); ++checks;
    check(b.top().clut_unowned_o == 1,
          "case6: a CLUT material is COUNTED -- its palette identity has no producer"); ++checks;
    check(b.top().selector_overflow_o == 1,
          "case6: the resolver's selector overflow is carried and counted here too"); ++checks;
  }

  // ==========================================================================
  // CASE 7 -- BOTH STRUCTURAL GUARDS, FIRED. Legal stimulus at the block's own
  // ports: a departure that never had an arrival. A guard that has not been
  // seen to fire is a claim, not an instrument.
  // ==========================================================================
  {
    Bench b;
    // Nothing has been published yet and nothing has entered the span.
    check(b.top().pub_valid_o == 0, "case7: nothing is published before the first resolve"); ++checks;
    b.top().d_leave_i = 1;
    b.top().eval();
    b.top().clk = 1; b.top().eval();
    b.top().clk = 0; b.top().eval();
    b.top().d_leave_i = 0;
    b.top().eval();
    check(b.top().err_unpublished_o == 1,
          "case7: err_unpublished_o FIRED on a departure with nothing published"); ++checks;
    check(b.top().err_occupancy_underflow_o == 1,
          "case7: err_occupancy_underflow_o FIRED on a departure with an empty span"); ++checks;

    // And the negative control the same law demands: with an arrival on the
    // same clock the two cancel and NEITHER counter moves again.
    b.top().d_enter_i = 1;
    b.top().d_leave_i = 1;
    b.top().eval();
    b.top().clk = 1; b.top().eval();
    b.top().clk = 0; b.top().eval();
    b.top().d_enter_i = 0;
    b.top().d_leave_i = 0;
    b.top().eval();
    check(b.top().err_occupancy_underflow_o == 1,
          "case7: an arrival and a departure on one clock cancel -- no false underflow"); ++checks;
  }

  std::printf("material_window_directed: %d check(s), %d failure(s)\n", checks, fails);
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
