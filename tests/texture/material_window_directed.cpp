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
  uint32_t palette_base = 0;   // MaterialRecord.palette_base, the palette's name
};

// What TEXTURE.PALETTELOAD answers when the window asks for a CLUT material's
// palette identity.  `owned == false` is the fail-safe outcome -- a null,
// misaligned or out-of-VRAM base, or a denied fetch -- and it must publish ZERO
// rather than the previous span's pair.
struct PaletteAnswer {
  bool owned = true;
  uint8_t slot = 2;
  uint8_t generation = 7;
  int latency = 2;
};

// What a triangle was shaded with, recorded at the instant the window emitted
// it. The queue is the driver's model of GEOM.CLIP..the door.
struct InFlight {
  uint8_t sample_count;
  uint8_t recipe;
  uint8_t weight;
  uint8_t binding;
  uint8_t response_class;
  // ADDED 2026-09-22 (owner ruling 1). Without this field the in-flight
  // comparison below would be structurally unable to see a mode that changed
  // under a triangle -- which is the fault the ruling's "in-flight triangles
  // retain their own profile" clause is about. A checker that cannot see the
  // field that moved is this repository's own worst defect.
  uint8_t material_mode;
  // ADDED 2026-09-26 (I13CLOSE), for the reason the field above it was added:
  // a checker that cannot see the field that moved is this repository's own
  // worst defect, and the palette pair is a field of the published record that
  // is latched two states later than the rest of it.
  uint8_t palette_slot;
  uint8_t palette_generation;
};

class Bench {
 public:
  Bench() {
    top_.clk = 0;
    top_.rst_n = 0;
    top_.t_valid_i = 0;
    top_.t_material_set_i = 0;
    top_.t_material_id_i = 0;
    top_.t_material_mode_i = 0;   // MATMODE_BACKED
    // The primitive's RASTER declaration (SHADOWRIDE, 2026-09-23): opaque and
    // the plain opaque write, which is what every producer but FORGE.SHADOW
    // declares and is the value the whole existing suite runs under.
    top_.t_vertex_alpha_i = 0xFF;
    top_.t_frag_state_i = 0;
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
    top_.rsp_palette_base_i = 0;
    top_.pal_req_ready_i = 1;
    top_.pal_rsp_valid_i = 0;
    top_.pal_rsp_owned_i = 0;
    top_.pal_rsp_slot_i = 0;
    top_.pal_rsp_gen_i = 0;
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
  void set_palette(const PaletteAnswer& p) { palette_ = p; }
  uint32_t palette_base_seen() const { return palette_base_seen_; }
  int palette_asks() const { return palette_asks_; }

  // One clock, with the resolver model and the downstream-span model both
  // running inside it. The span drains one triangle per clock once it has any,
  // which is deliberately SLOWER than the window can emit -- a drain that
  // completes instantly would never exercise ST_DRAIN at all.
  void tick() {
    // ---- the resolver model, on the settled pre-edge values --------------
    top_.eval();
    bool req_fire = top_.req_valid_o && top_.req_ready_i;
    bool rsp_fire = top_.rsp_valid_i && top_.rsp_ready_o;
    bool pal_req_fire = top_.pal_req_valid_o && top_.pal_req_ready_i;
    bool pal_rsp_fire = top_.pal_rsp_valid_i && top_.pal_rsp_ready_o;

    // ---- the downstream span model ---------------------------------------
    bool emit = top_.t_valid_o && top_.t_ready_i;
    InFlight carried{};
    if (emit) {
      carried.sample_count = top_.pub_sample_count_o;
      carried.recipe = top_.pub_material_recipe_o;
      carried.weight = top_.pub_recipe_weight_o;
      carried.binding = top_.pub_base_binding_o;
      carried.response_class = top_.pub_response_class_o;
      carried.material_mode = top_.pub_material_mode_o;
      carried.palette_slot = top_.pub_palette_slot_o;
      carried.palette_generation = top_.pub_palette_generation_o;
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
          t.response_class != top_.pub_response_class_o ||
          t.material_mode != top_.pub_material_mode_o ||
          t.palette_slot != top_.pub_palette_slot_o ||
          t.palette_generation != top_.pub_palette_generation_o) {
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
      top_.rsp_palette_base_i = answer_.palette_base;
      answered_ = true;
    }

    // ---- TEXTURE.PALETTELOAD's model, the same one-clock-pulse shape ------
    // The window leaves ST_PAL on the same edge that takes the answer, so the
    // pulse must be withdrawn exactly as the resolver's is; a held valid would
    // answer the NEXT CLUT span instantly with this one's pair, which is the
    // stale-join fault this whole file is about.
    if (pal_rsp_fire || pal_answered_) {
      top_.pal_rsp_valid_i = 0;
      pal_answered_ = false;
      pal_pending_ = -1;
    } else if (pal_req_fire) {
      pal_pending_ = palette_.latency;
      palette_base_seen_ = top_.pal_req_base_o;
      ++palette_asks_;
    } else if (pal_pending_ > 0) {
      --pal_pending_;
    } else if (pal_pending_ == 0) {
      top_.pal_rsp_valid_i = 1;
      top_.pal_rsp_owned_i = palette_.owned ? 1 : 0;
      top_.pal_rsp_slot_i = palette_.slot;
      top_.pal_rsp_gen_i = palette_.generation;
      pal_answered_ = true;
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
  int offer(uint32_t set, uint16_t id, uint8_t tier, int n, int budget = 4000,
            uint8_t mode = 0, uint8_t valpha = 0xFF, uint32_t fstate = 0) {
    top_.t_material_set_i = set;
    top_.t_material_id_i = id;
    top_.t_material_mode_i = mode;
    top_.t_vertex_alpha_i = valpha;
    top_.t_frag_state_i = fstate;
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

  // Offer `n` beats that the window is expected to REFUSE. `offer()` above
  // counts `t_valid_o && t_ready_i` -- what PASSED -- and a refused beat never
  // raises `t_valid_o`, so it would spin its whole budget. This counts what was
  // CONSUMED (`t_valid_i && t_ready_o`) instead, which is the handshake a
  // refusal actually completes.
  int offer_refused(uint32_t set, uint16_t id, uint8_t mode, int n,
                    int budget = 400) {
    top_.t_material_set_i = set;
    top_.t_material_id_i = id;
    top_.t_material_mode_i = mode;
    top_.t_quality_tier_i = 0;
    top_.t_valid_i = 1;
    int taken = 0;
    int spent = 0;
    while (taken < n && spent < budget) {
      top_.eval();
      if (top_.t_ready_o) ++taken;
      tick();
      ++spent;
    }
    top_.t_valid_i = 0;
    top_.t_material_mode_i = 0;
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
  PaletteAnswer palette_{};
  std::deque<InFlight> span_;
  int pending_ = -1;
  bool answered_ = false;
  int pal_pending_ = -1;
  bool pal_answered_ = false;
  uint32_t palette_base_seen_ = 0;
  int palette_asks_ = 0;
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
  // CASE 6 -- THE PALETTE IDENTITY, PRODUCED. Rewritten 2026-09-26 (I13CLOSE):
  // it used to assert `clut_unowned_o == 1` for EVERY CLUT material, because
  // none of them had an identity -- which was a true statement about the
  // console and a test that pinned the absence. There is a producer now, so
  // what is asserted is that the window ASKS it, on the record's own
  // `palette_base`, and PUBLISHES what it answers.
  //
  // A 16-bit binding slot narrowed to 8 is still a selector overflow and is
  // still counted here, unchanged.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.modes = 0x00;                  // tmu_mode 0 -> CLUT
    a.selector_overflow = true;
    a.latency = 3;
    a.palette_base = 0x0040'C000u;
    b.set_answer(a);
    PaletteAnswer p;
    p.owned = true;
    p.slot = 3;
    p.generation = 0x2A;
    b.set_palette(p);
    b.offer(0x5555'0000u, 0x0007, 0, 3);
    b.drain();
    check(b.top().pub_response_class_o == 0, "case6: tmu_mode 0 maps to CLUT"); ++checks;
    check(b.palette_asks() == 1,
          "case6: ONE palette ask for one CLUT span -- not one per triangle"); ++checks;
    check(b.palette_base_seen() == 0x0040'C000u,
          "case6: the ask carries the RECORD's palette_base, not a constant"); ++checks;
    check(b.top().pub_palette_slot_o == 3 && b.top().pub_palette_generation_o == 0x2A,
          "case6: the published pair is the one the producer answered"); ++checks;
    check(b.top().clut_owned_o == 1 && b.top().clut_unowned_o == 0,
          "case6: an owned CLUT palette moves the owned counter and not the fault"); ++checks;
    check(b.mismatches() == 0,
          "case6: every triangle was disposed under the palette pair it was emitted with");
    ++checks;
    check(b.top().selector_overflow_o == 1,
          "case6: the resolver's selector overflow is carried and counted here too"); ++checks;
  }

  // ==========================================================================
  // CASE 6b -- GENERATION ZERO IS AN ORDINARY ANSWER, and slot zero with it.
  // `zhao_texture_palette_load` allocates generations from a counter that
  // resets to zero, so the FIRST palette a console ever loads is {slot 0,
  // generation 0} -- and until 2026-09-26 that was exactly the pair this
  // console could not distinguish from "no identity at all", because it was
  // also the composer's constant. The distinction now lives in `owned`, which
  // is a separate wire, so a real {0, 0} is published and COUNTED AS OWNED.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.modes = 0x00;
    a.latency = 2;
    a.palette_base = 0x0040'0000u;
    b.set_answer(a);
    PaletteAnswer p;
    p.owned = true;
    p.slot = 0;
    p.generation = 0;
    b.set_palette(p);
    b.offer(0x6666'0000u, 0x0001, 0, 4);
    b.drain();
    check(b.top().pub_palette_slot_o == 0 && b.top().pub_palette_generation_o == 0,
          "case6b: a real {slot 0, generation 0} is published"); ++checks;
    check(b.top().clut_owned_o == 1,
          "case6b: and it is counted as OWNED, not as the unowned case it looks like");
    ++checks;
    check(b.top().clut_unowned_o == 0, "case6b: the fault counter did not move"); ++checks;
  }

  // ==========================================================================
  // CASE 6c -- THE FAULT ARM, FIRED. A palette the producer could not make
  // resident publishes ZERO and is counted; it does NOT hold the stream, and
  // it does NOT inherit the previous span's pair -- which is the fault that
  // would be invisible downstream, because a wrong palette draws confident
  // wrong colours through an entirely healthy handshake.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.modes = 0x00;
    a.latency = 2;
    a.palette_base = 0x0040'C000u;
    b.set_answer(a);
    PaletteAnswer good;
    good.owned = true;
    good.slot = 2;
    good.generation = 0x11;
    b.set_palette(good);
    b.offer(0x7777'0000u, 0x0001, 0, 3);
    b.drain();
    check(b.top().pub_palette_slot_o == 2 && b.top().pub_palette_generation_o == 0x11,
          "case6c: the first CLUT span published a real pair"); ++checks;

    PaletteAnswer bad;
    bad.owned = false;
    bad.slot = 2;          // the producer still names a slot; `owned` is the verdict
    bad.generation = 0x11;
    b.set_palette(bad);
    const int taken = b.offer(0x7777'0000u, 0x0002, 0, 3);
    b.drain();
    check(taken == 3, "case6c: an unowned palette does NOT stall the triangle stream");
    ++checks;
    check(b.top().pub_palette_slot_o == 0 && b.top().pub_palette_generation_o == 0,
          "case6c: an unowned palette publishes ZERO, not the previous span's pair");
    ++checks;
    check(b.top().clut_unowned_o == 1 && b.top().clut_owned_o == 1,
          "case6c: one owned and one unowned -- both arms of the decision moved");
    ++checks;
    check(b.mismatches() == 0,
          "case6c: no triangle was disposed under a palette pair it was not emitted with");
    ++checks;
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


  // ==========================================================================
  // CASE 8 -- NO_MATERIAL IS A LAWFUL MODE (owner ruling 1, 2026-09-22).
  //
  // The ruling's three requirements, each asserted as its own check:
  //   * "select a defined no-sampling profile"       -> sample_count 0 and the
  //                                                     mode published as NONE;
  //   * "issue no material/texture/palette
  //      resolution request"                          -> `resolves_o` DOES NOT
  //                                                     MOVE;
  //   * "increment no missing-material fault counter" -> `no_record_o` DOES NOT
  //                                                     MOVE.
  //
  // The last two are the ones worth having, and they are NOT restatements of
  // the first: `resolves_o` counts a handshake with MATERIAL.RESOLVE and
  // `no_record_o` counts a response, so both are driven by enables the
  // no-material arm never reaches. Three independent quantities, not one said
  // three ways -- which is the whole of CLAUDE.md's checker law.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.sample_count = 2; a.recipe = 3; a.weight = 0x77; a.binding = 4;
    a.modes = 0x02;
    b.set_answer(a);

    const int mesh = b.offer(0x1111'0000u, 5, 0x10, 6);
    b.drain();
    const uint32_t resolves_after_mesh = b.top().resolves_o;
    check(mesh == 6, "case8: the textured mesh drew"); ++checks;
    check(resolves_after_mesh == 1, "case8: the mesh cost exactly one resolve"); ++checks;

    // The particle batch. Set and id are ZERO, which is what the mode REQUIRES.
    const int parts = b.offer(0, 0, 0, 9, 4000, 1 /* MATMODE_NONE */);
    b.drain();

    check(parts == 9, "case8: every polygon particle DREW -- the whole point"); ++checks;
    check(b.top().no_material_spans_o == 1,
          "case8: one NO_MATERIAL span was published for the batch"); ++checks;
    check(b.top().pub_material_mode_o == 1,
          "case8: the published mode is NO_MATERIAL"); ++checks;
    check(b.top().pub_sample_count_o == 0,
          "case8: the no-sampling profile takes zero texture samples"); ++checks;
    check(b.top().pub_base_binding_o == 0,
          "case8: the no-sampling profile binds nothing"); ++checks;
    check(b.top().pub_response_class_o == 0,
          "case8: the no-sampling profile has no response class"); ++checks;
    check(b.top().resolves_o == resolves_after_mesh,
          "case8: NO resolve was issued for the particle batch"); ++checks;
    check(b.top().no_record_o == 0,
          "case8: NO missing-material fault was counted -- the ruling's own sentence"); ++checks;
    check(b.top().mode_refused_o == 0,
          "case8: a well-formed NO_MATERIAL declaration is not a refusal"); ++checks;
    check(b.top().err_unpublished_o == 0,
          "case8: nothing left the span unpublished"); ++checks;
  }

  // ==========================================================================
  // CASE 9 -- BACKED -> NONE -> BACKED, THROUGH THE EXISTING DRAIN.
  //
  // The ruling: "Material mode is part of the span's identity. Changing
  // MATERIAL_BACKED -> NO_MATERIAL -> MATERIAL_BACKED must honor the existing
  // drain/ordering mechanism so that in-flight triangles retain their own
  // profile."
  //
  // `mismatches()` is the assertion. The driver models the span and compares
  // what each triangle CARRIED against what is published when it retires, and
  // `InFlight` now carries the MODE -- so a mode that moved under an in-flight
  // triangle is visible to the comparison rather than invisible to it.
  //
  // This is deliberately the CORRECT-BEHAVIOUR assertion ("the record holds"),
  // not "a detector fired". It keeps its value after the defect is gone.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.sample_count = 1; a.recipe = 1; a.weight = 0x11; a.binding = 1;
    a.modes = 0x01;
    b.set_answer(a);

    const int meshA = b.offer(0xAAAA'0000u, 1, 0x10, 5);

    Answer bmat = a;
    bmat.sample_count = 2; bmat.recipe = 4; bmat.weight = 0x99; bmat.binding = 7;
    bmat.modes = 0x02;

    const int parts = b.offer(0, 0, 0, 5, 4000, 1);
    b.set_answer(bmat);
    const int meshB = b.offer(0xBBBB'0000u, 2, 0x10, 5);
    b.drain();

    check(meshA == 5, "case9: mesh A drew"); ++checks;
    check(parts == 5, "case9: the particle batch drew between the two meshes"); ++checks;
    check(meshB == 5, "case9: mesh B drew"); ++checks;
    check(b.mismatches() == 0,
          "case9: EVERY in-flight triangle retired under the profile it carried, "
          "across both mode changes"); ++checks;
    check(b.disposed() == 15, "case9: all fifteen triangles were disposed of"); ++checks;
    check(b.top().switches_o == 3,
          "case9: three switches -- A, the batch, B -- and not one more"); ++checks;
    // TWO resolves for THREE spans. That difference is the measurement: the
    // particle span asked MATERIAL.RESOLVE nothing at all.
    check(b.top().resolves_o == 2,
          "case9: two resolves for three spans -- the particle span issued none"); ++checks;
    check(b.top().no_material_spans_o == 1, "case9: exactly one no-material span"); ++checks;
    check(b.top().no_record_o == 0, "case9: no missing-material fault across the whole run"); ++checks;
    check(b.top().pub_material_mode_o == 0,
          "case9: the run ends back in MATERIAL_BACKED"); ++checks;
    check(b.top().pub_base_binding_o == 7,
          "case9: mesh B's own record is published at the end, not mesh A's"); ++checks;
    check(b.emitted_unpublished() == 0, "case9: nothing was emitted unpublished"); ++checks;
  }

  // ==========================================================================
  // CASE 10 -- CONTRADICTORY DECLARATIONS ARE REFUSED, NOT REPAIRED.
  //
  // The ruling: "Reject internally contradictory declarations rather than
  // silently repairing them." Two shapes, both refused and counted, and the
  // NEGATIVE CONTROL beside them -- the same block, the same clock budget, a
  // well-formed declaration, counter still zero. A counter that fires is only
  // an instrument if it is also silent on legal stimulus (R95).
  // ==========================================================================
  {
    Bench b;
    Answer a;
    b.set_answer(a);

    // Legal traffic first, so the negative control is measured on a live block
    // rather than on one that has never run.
    const int mesh = b.offer(0x2222'0000u, 3, 0, 4);
    b.drain();
    check(mesh == 4, "case10: legal mesh traffic passed"); ++checks;
    check(b.top().mode_refused_o == 0,
          "case10: NEGATIVE CONTROL -- a MATERIAL_BACKED beat is not refused"); ++checks;

    const int legal_parts = b.offer(0, 0, 0, 3, 4000, 1);
    b.drain();
    check(legal_parts == 3, "case10: a well-formed NO_MATERIAL batch passed"); ++checks;
    check(b.top().mode_refused_o == 0,
          "case10: NEGATIVE CONTROL -- a well-formed NO_MATERIAL beat is not refused"); ++checks;

    // FAULT 1: NO_MATERIAL with a material identity it expects resolved.
    const int contra = b.offer_refused(0xDEAD'BEEFu, 9, 1, 4);
    check(contra == 4, "case10: the contradictory beats were CONSUMED, not stalled"); ++checks;
    check(b.top().mode_refused_o == 4,
          "case10: mode_refused_o FIRED once per contradictory beat"); ++checks;

    const uint32_t after_contra = b.top().mode_refused_o;
    const uint32_t resolves_before = b.top().resolves_o;
    const uint32_t spans_before = b.top().no_material_spans_o;

    // FAULT 2: an undefined mode encoding. 2'd2 and 2'd3 are reserved.
    const int undef2 = b.offer_refused(0, 0, 2, 3);
    const int undef3 = b.offer_refused(0, 0, 3, 3);
    check(undef2 == 3 && undef3 == 3,
          "case10: undefined mode encodings were consumed"); ++checks;
    check(b.top().mode_refused_o == after_contra + 6,
          "case10: both reserved encodings are refused and counted"); ++checks;

    // AND THE REFUSALS DID NOTHING ELSE. A refused beat must not resolve, must
    // not publish and must not enter the span -- so the three quantities that
    // would have moved if it had are checked to have stood still.
    check(b.top().resolves_o == resolves_before,
          "case10: a refused beat issued no resolve"); ++checks;
    check(b.top().no_material_spans_o == spans_before,
          "case10: a refused beat published no span"); ++checks;
    check(b.top().err_unpublished_o == 0,
          "case10: a refused beat never entered the span"); ++checks;
    check(b.top().err_occupancy_underflow_o == 0,
          "case10: the drain accounting never saw a refused beat"); ++checks;

    // And legal traffic still flows afterwards -- a refusal is not a wedge.
    const int after = b.offer(0x3333'0000u, 1, 0, 3);
    b.drain();
    check(after == 3, "case10: the stream runs again after the refusals"); ++checks;
  }

  // ==========================================================================
  // case11: THE PRIMITIVE'S RASTER DECLARATION IS PART OF THE SPAN'S IDENTITY.
  //
  // SHADOWRIDE, 2026-09-23. `t_vertex_alpha_i` and `t_frag_state_i` arrive on
  // the granted beat with the material pair and the mode, and are published
  // with them by the SAME enable. Three things have to hold:
  //
  //   * the published alpha and state are the SPAN'S, not a constant;
  //   * a primitive that disagrees about EITHER is a `match_c` miss, so it
  //     gets its own span and cannot be painted with the previous one's
  //     opacity -- the same law `t_material_mode_i` obeys and for the same
  //     stated reason: "a mode change is a match_c miss, and a match_c miss is
  //     the existing mechanism";
  //   * and a run that agrees about all of them is still ONE span, so adding
  //     two fields to the identity has not turned every triangle into a switch.
  // ==========================================================================
  {
    Bench b;
    Answer a;
    a.sample_count = 1; a.recipe = 0; a.weight = 0x33; a.binding = 2;
    a.modes = 0x01;
    b.set_answer(a);
    const uint32_t sw0 = b.top().switches_o;
    const int n1 = b.offer(0x51500000u, 7, 0, 3, 4000, 0, 0xFF, 0);
    b.drain();
    check(n1 == 3, "case11: the opaque run passed"); ++checks;
    check(b.top().pub_vertex_alpha_o == 0xFF,
          "case11: the span publishes ITS alpha"); ++checks;
    check(b.top().pub_frag_state_o == 0u,
          "case11: and ITS raster state"); ++checks;
    const uint32_t sw1 = b.top().switches_o;
    check(sw1 == sw0 + 1,
          "case11: three agreeing triangles are ONE span"); ++checks;

    // Same material pair, same mode, DIFFERENT alpha and state: a new span.
    const int n2 = b.offer(0x51500000u, 7, 0, 2, 4000, 0, 0x60, 0x0000000Bu);
    b.drain();
    check(n2 == 2, "case11: the transparent run passed"); ++checks;
    check(b.top().pub_vertex_alpha_o == 0x60,
          "case11: the new span publishes the NEW alpha, not the held one");
    ++checks;
    check(b.top().pub_frag_state_o == 0x0000000Bu,
          "case11: and BLEND=ALPHA + Z_TEST_EN + Z_WRITE_DIS"); ++checks;
    check(b.top().switches_o == sw1 + 1,
          "case11: the declaration change cost exactly ONE switch"); ++checks;
  }

  std::printf("material_window_directed: %d check(s), %d failure(s)\n", checks, fails);
  zhao::exit_hard(fails == 0 ? 0 : 1);
}
