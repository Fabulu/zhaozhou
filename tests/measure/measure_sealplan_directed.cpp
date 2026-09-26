// measure_sealplan_directed.cpp -- MEASURE.SEALPLAN: the per-view pre-frame
// admission plan, validated in hardware and turned into GEOM.PARAMBUF's quota
// seal. Console entry I56; owner vacation directive 2026-09-23 section 5.
//
// THE DIRECTIVE NAMES THE CASES, so this file is organised by its sentence
// rather than by the block's ports:
//
//   > "The quota producer, encoded maximum values, absent-giant case,
//   >  reservation under pressure, and oversized plans all need real execution
//   >  and failure tests."
//
// WHAT THIS LANE WOULD CATCH:
//
//   1. THE SEAL CARRYING A CONSTANT. Case 1 publishes a plan whose numbers are
//      NOT the capacities and requires those numbers at `seal_*_o`. Red if the
//      block ever falls back to capacity, which is the exact state the
//      directive names -- "A constant equal to arena capacity is not an
//      admission plan."
//   2. THE 14x UNIT CONFUSION, IN BOTH DIRECTIONS AND BY DIFFERENT RULES.
//      Cases 8 and 9. A plan reserving 2,341 REFERENCES for the giant is a
//      giant paid for at a fourteenth of its cost and must be refused; a plan
//      writing 32,768 into the CHUNK field must be refused too. Case 10 is the
//      one that matters most: a plan that WOULD PASS under the confusion --
//      ordinary chunks sized against a 2,341-reference reserve -- and fails.
//   3. A RESERVATION THAT IS ASSERTED RATHER THAN ENFORCED. Cases 5 and 6 sit
//      on both sides of the line in both units. Red if the block admits a plan
//      whose ordinary demand plus the reservation exceeds capacity by one.
//   4. A REFUSAL THAT CLAMPS INSTEAD OF REFUSING. Every refusal case asserts
//      `seal_valid_o` stayed LOW for the whole frame edge. A block that
//      silently reduced an illegal plan to a legal one would pass a test that
//      only read the counters.
//   5. A SELECTOR THAT IS NOT A STREAMING MAX. Cases 11 to 13: highest weight
//      wins regardless of arrival order, lowest instance breaks a tie, and the
//      first draw of a frame is taken unconditionally (the reset-value trap: a
//      first draw of weight 0 must not lose to nothing).
//   6. A MISMATCH DETECTOR THAT CANNOT FIRE. Case 14 seals a giant the frame
//      then fails to draw, and case 15 draws a heavier one than was paid for.
//      Both move `giant_mismatch_o`. Case 16 is the negative control.
//   7. A PLAN RENEGOTIATING A SEALED FRAME. Case 17 publishes a second plan
//      mid-frame and requires the sealed record not to move.
//   8. A GENERATION CHECK WITH OPERANDS THAT MOVE TOGETHER. Cases 18 and 19
//      bump the console's own generation after the plan was priced and require
//      the refusal. This is the one CLAUDE.md's metadata-bank law is about:
//      the counter that reads zero forever because both sides moved.

#include "Vzhao_measure_sealplan.h"

#include <cstdio>
#include <string>

#include "zhao_sim.hpp"

using zhao::check;

namespace {

// The composed console's numbers, restated so a reader need not chase them.
// MAX_REFS is the BINNER's (`zhao_shell_top_v2`: 8192 chunks x 4 refs); the
// other three are the arena's.
constexpr uint32_t kMaxVerts = 65536;
constexpr uint32_t kMaxTris = 16384;
constexpr uint32_t kMaxChunks = 16384;
constexpr uint32_t kMaxRefs = 32768;
constexpr uint32_t kChunkIds = 14;
constexpr uint32_t kGiantRefs = 32768;
// ceil(32768/14). Written as the division rather than as 2341 so a reader can
// see WHICH arithmetic produced it -- the whole subject of this file.
constexpr uint32_t kGiantChunks = (kGiantRefs + kChunkIds - 1) / kChunkIds;

// The refusal reasons, in the block's order.
enum Reason : uint8_t {
  kNone = 0,
  kVertsCap = 1,
  kTrisCap = 2,
  kChunksCap = 3,
  kRefsCap = 4,
  kGiantTrim = 5,
  kFlagsMbz = 6,
  kViewId = 7,
  kChunkResv = 8,
  kRefResv = 9,
  kResGen = 10,
  kViewGen = 11,
  kGiantStray = 12,
};

struct Plan {
  uint8_t view = 0;
  uint8_t flags = 0;  // bit0 giant_present
  uint16_t res_gen = 0;
  uint16_t view_gen = 0;
  uint16_t giant_inst = 0;
  uint32_t verts = 0;
  uint32_t tris = 0;
  uint32_t chunks = 0;
  uint32_t refs = 0;
  uint32_t giant_refs = 0;
};

/** A plan that fits with no giant: the capacities, exactly. */
Plan legal_no_giant() {
  Plan p;
  p.verts = kMaxVerts;
  p.tris = kMaxTris;
  p.chunks = kMaxChunks;
  p.refs = kMaxRefs;
  p.giant_refs = 0;
  return p;
}

/**
 * A plan that fits WITH the guaranteed giant: ordinary demand exactly at the
 * line in both reserved units.
 *
 * Note `refs = 0`, and it is not a shortcut. MAX_REFS == GIANT_REFS on the
 * composed console, so a frame declaring the giant has NO ordinary reference
 * budget at all. That is the ruled number enforced honestly and it is also a
 * measurement about the machine -- see the block header.
 */
Plan legal_with_giant() {
  Plan p;
  p.flags = 1;
  p.giant_inst = 7;
  p.verts = kMaxVerts;
  p.tris = kMaxTris;
  p.chunks = kMaxChunks - kGiantChunks;  // 14,043
  p.refs = 0;
  p.giant_refs = kGiantRefs;
  return p;
}

void reset(Vzhao_measure_sealplan& dut) {
  dut.rst_n = 0;
  dut.plan_valid_i = 0;
  dut.plan_view_i = 0;
  dut.plan_flags_i = 0;
  dut.plan_res_gen_i = 0;
  dut.plan_view_gen_i = 0;
  dut.plan_giant_inst_i = 0;
  dut.plan_verts_i = 0;
  dut.plan_tris_i = 0;
  dut.plan_chunks_i = 0;
  dut.plan_refs_i = 0;
  dut.plan_giant_refs_i = 0;
  dut.res_bump_i = 0;
  dut.view_bump_i = 0;
  dut.dw_valid_i = 0;
  dut.dw_weight_i = 0;
  dut.dw_inst_i = 0;
  dut.frame_begin_i = 0;
  dut.frame_end_i = 0;
  dut.seal_ready_i = 1;
  dut.eval();
  for (int i = 0; i < 3; ++i) zhao::tick(dut);
  dut.rst_n = 1;
  dut.eval();
  zhao::tick(dut);
}

/** Publish one plan record: a one-cycle pulse with the fields beside it. */
void publish(Vzhao_measure_sealplan& dut, const Plan& p) {
  dut.plan_valid_i = 1;
  dut.plan_view_i = p.view;
  dut.plan_flags_i = p.flags;
  dut.plan_res_gen_i = p.res_gen;
  dut.plan_view_gen_i = p.view_gen;
  dut.plan_giant_inst_i = p.giant_inst;
  dut.plan_verts_i = p.verts;
  dut.plan_tris_i = p.tris;
  dut.plan_chunks_i = p.chunks;
  dut.plan_refs_i = p.refs;
  dut.plan_giant_refs_i = p.giant_refs;
  zhao::tick(dut);
  dut.plan_valid_i = 0;
  dut.eval();
}

/** What the block offered the arena on the frame's begin edge. */
struct Offered {
  bool valid;
  uint32_t verts, tris, chunks, refs, giant_refs, giant_chunks;
  uint16_t frame_gen;
};

/**
 * One frame begin edge, read the way the ARENA reads it: the offered record is
 * whatever the wires carry ON the pulse cycle, before the clock edge that
 * latches it. That is not a convenience -- the arena is combinational over
 * `seal_valid_i` and these fields on exactly this cycle.
 */
Offered frame_begin(Vzhao_measure_sealplan& dut) {
  dut.frame_begin_i = 1;
  dut.eval();
  Offered o{};
  o.valid = dut.seal_valid_o != 0;
  o.verts = dut.seal_verts_o;
  o.tris = dut.seal_tris_o;
  o.chunks = dut.seal_chunks_o;
  o.refs = dut.seal_refs_o;
  o.giant_refs = dut.seal_giant_refs_o;
  o.giant_chunks = dut.seal_giant_chunks_o;
  o.frame_gen = dut.seal_frame_gen_o;
  zhao::tick(dut);
  dut.frame_begin_i = 0;
  // ONE IDLE CYCLE WITH THE LEVEL LOW, and it is not padding. The block seals
  // on the RISING EDGE of `frame_begin_i` (see case 22 and the block's own
  // comment), so a helper that never clocks the signal low would never re-arm
  // the edge detector and every frame after the first would silently do
  // nothing. The composed console cannot present two frame begins with no gap
  // either -- `render_frame_begin_i` is a lease request with a real ready.
  zhao::tick(dut);
  dut.eval();
  return o;
}

void frame_end(Vzhao_measure_sealplan& dut) {
  dut.frame_end_i = 1;
  zhao::tick(dut);
  dut.frame_end_i = 0;
  dut.eval();
}

void draw(Vzhao_measure_sealplan& dut, uint8_t weight, uint16_t inst) {
  dut.dw_valid_i = 1;
  dut.dw_weight_i = weight;
  dut.dw_inst_i = inst;
  zhao::tick(dut);
  dut.dw_valid_i = 0;
  dut.eval();
}

/**
 * Publish a plan, take the frame edge, and require a REFUSAL with this reason.
 *
 * The `seal_valid_o` check is the load-bearing half and is easy to leave out:
 * a block that CLAMPED an illegal plan to a legal one would move no refusal
 * counter and would seal a frame nobody authorised. "Illegal/overflowing plans
 * are refused before any partial publication" is the directive's wording and
 * the absence of the seal is what "before" means.
 */
void expect_refusal(Vzhao_measure_sealplan& dut, const Plan& p, uint8_t reason,
                    const char* what) {
  const uint32_t before = dut.plans_refused_o;
  publish(dut, p);
  const Offered o = frame_begin(dut);
  check(!o.valid, (std::string(what) + ": NO seal was offered").c_str(), 0, o.valid ? 1 : 0);
  check(dut.plans_refused_o == before + 1, (std::string(what) + ": the refusal counted").c_str(),
        before + 1, dut.plans_refused_o);
  check(dut.refuse_reason_o == reason, (std::string(what) + ": the reason names the rule").c_str(),
        reason, dut.refuse_reason_o);
}

}  // namespace

int main() {
  Vzhao_measure_sealplan dut;

  // =========================================================================
  // 1. THE QUOTA PRODUCER. A real per-view number reaches the seal.
  // =========================================================================
  // The numbers here are deliberately NOT the capacities and not round: if the
  // block ever reverted to sealing at capacity, or to sealing a constant of any
  // kind, every one of these six comparisons moves.
  {
    reset(dut);
    Plan p = legal_no_giant();
    p.verts = 40000;
    p.tris = 9000;
    p.chunks = 5000;
    p.refs = 21000;
    publish(dut, p);
    check(dut.plans_staged_o == 1, "case1: the record staged", 1, dut.plans_staged_o);

    const Offered o = frame_begin(dut);
    check(o.valid, "case1: the plan was sealed", 1, o.valid ? 1 : 0);
    check(o.verts == 40000, "case1: VERTICES are the plan's, not the capacity", 40000, o.verts);
    check(o.tris == 9000, "case1: TRIANGLES are the plan's", 9000, o.tris);
    check(o.chunks == 5000, "case1: CHUNKS are the plan's", 5000, o.chunks);
    check(o.refs == 21000, "case1: TILE REFERENCES are the plan's", 21000, o.refs);
    // The four sealed numbers are all different from the four capacities. That
    // sentence IS the acceptance test the directive wrote.
    check(o.verts != kMaxVerts && o.tris != kMaxTris && o.chunks != kMaxChunks &&
              o.refs != kMaxRefs,
          "case1: not one sealed field equals its capacity", 1, 1);
    check(dut.plans_sealed_o == 1, "case1: counted as a PLAN seal", 1, dut.plans_sealed_o);
    check(dut.default_seals_o == 0, "case1: and not as a default seal", 0, dut.default_seals_o);
  }

  // =========================================================================
  // 2. ENCODED MAXIMUM VALUES. Exactly at capacity fits; one past does not.
  // =========================================================================
  {
    reset(dut);
    Plan p = legal_no_giant();  // every field AT its capacity
    publish(dut, p);
    const Offered o = frame_begin(dut);
    check(o.valid, "case2: a plan at exactly the capacities is sealed", 1, o.valid ? 1 : 0);
    check(o.verts == kMaxVerts, "case2: MAX_VERTS survives the 18-bit field", kMaxVerts, o.verts);
    check(o.refs == kMaxRefs, "case2: MAX_REFS survives it too", kMaxRefs, o.refs);

    // One past, in each unit, one at a time. Four separate refusals with four
    // separate reasons: a single "it does not fit" would not distinguish the
    // unit, and distinguishing the unit is this block's whole job.
    Plan v = legal_no_giant(); v.verts = kMaxVerts + 1;
    expect_refusal(dut, v, kVertsCap, "case2a: VERTICES one past capacity");
    Plan t = legal_no_giant(); t.tris = kMaxTris + 1;
    expect_refusal(dut, t, kTrisCap, "case2b: TRIANGLES one past capacity");
    Plan c = legal_no_giant(); c.chunks = kMaxChunks + 1;
    expect_refusal(dut, c, kChunksCap, "case2c: CHUNKS one past capacity");
    Plan r = legal_no_giant(); r.refs = kMaxRefs + 1;
    expect_refusal(dut, r, kRefsCap, "case2d: REFERENCES one past capacity");
  }

  // =========================================================================
  // 3. THE ABSENT-GIANT CASE. The reservation is RELEASED before sealing.
  // =========================================================================
  //   > "A frame explicitly containing no guaranteed giant may release that
  //   >  reservation BEFORE sealing."
  {
    reset(dut);
    Plan p = legal_no_giant();
    publish(dut, p);
    const Offered o = frame_begin(dut);
    check(o.valid, "case3: a no-giant plan at full capacity is sealed", 1, o.valid ? 1 : 0);
    check(o.giant_refs == 0, "case3: the REFERENCE reservation is released", 0, o.giant_refs);
    check(o.giant_chunks == 0, "case3: the CHUNK reservation is released", 0, o.giant_chunks);
    check(o.chunks == kMaxChunks, "case3: ordinary allocation gets the whole arena", kMaxChunks,
          o.chunks);
  }

  // =========================================================================
  // 4. THE RESERVATION, SEALED. A giant frame is sealed BELOW capacity.
  // =========================================================================
  {
    reset(dut);
    publish(dut, legal_with_giant());
    const Offered o = frame_begin(dut);
    check(o.valid, "case4: a plan with the guaranteed giant is sealed", 1, o.valid ? 1 : 0);
    check(o.giant_refs == kGiantRefs, "case4: the seal NAMES the reservation in REFERENCES",
          kGiantRefs, o.giant_refs);
    check(o.giant_chunks == kGiantChunks, "case4: and in CHUNKS, ceil(32768/14)", kGiantChunks,
          o.giant_chunks);
    check(kGiantChunks == 2341, "case4: which is 2,341 and not 32,768", 2341, kGiantChunks);
    // THE RESERVATION, AS THE ARENA WILL SEE IT. The ordinary chunk quota is
    // strictly below capacity by exactly the reservation, so the arena's top
    // 2,341 chunks are budget the ordinary stream is never given.
    check(o.chunks == kMaxChunks - kGiantChunks,
          "case4: the ordinary CHUNK quota is capacity minus the reservation",
          kMaxChunks - kGiantChunks, o.chunks);
    check(o.chunks + o.giant_chunks == kMaxChunks,
          "case4: and the two together are exactly the arena", kMaxChunks,
          o.chunks + o.giant_chunks);
  }

  // =========================================================================
  // 5. RESERVATION UNDER PRESSURE -- CHUNKS. One past the line is refused.
  // =========================================================================
  {
    reset(dut);
    Plan p = legal_with_giant();
    p.chunks = kMaxChunks - kGiantChunks + 1;  // 14,044: legal alone, illegal beside the giant
    // Note what makes this the interesting case: 14,044 is FAR below
    // MAX_CHUNKS, so `bad_chunks_c` does not fire. The only thing that refuses
    // it is the reservation, which is the property under test.
    expect_refusal(dut, p, kChunkResv, "case5: ordinary CHUNKS eating the reserve by one");
  }

  // =========================================================================
  // 6. RESERVATION UNDER PRESSURE -- REFERENCES.
  // =========================================================================
  {
    reset(dut);
    Plan p = legal_with_giant();
    p.refs = 1;  // one single ordinary tile reference
    expect_refusal(dut, p, kRefResv, "case6: ONE ordinary reference beside the giant");
    // The measurement behind that, stated as a check so it cannot rot: the
    // composed binner's whole reference capacity IS the giant's reservation.
    check(kMaxRefs == kGiantRefs,
          "case6: MAX_REFS equals GIANT_REFS, so a giant frame has no ordinary references",
          kGiantRefs, kMaxRefs);
  }

  // =========================================================================
  // 7. OVERSIZED PLANS. Illegal in two units at once presents ONE reason, and
  //    it is the deterministic one.
  // =========================================================================
  {
    reset(dut);
    Plan p = legal_no_giant();
    p.verts = kMaxVerts + 1;
    p.tris = kMaxTris + 1;
    expect_refusal(dut, p, kVertsCap, "case7: two capacity breaches present the first");
    // And the refused plan is CONSUMED: an illegal record must not re-offer
    // itself every frame forever, or one bad packet disables the console.
    const uint32_t refused = dut.plans_refused_o;
    const Offered o = frame_begin(dut);
    check(o.valid, "case7: the NEXT frame seals the default, not the refused plan", 1,
          o.valid ? 1 : 0);
    check(dut.plans_refused_o == refused, "case7: and does not refuse it a second time", refused,
          dut.plans_refused_o);
    check(dut.default_seals_o == 1, "case7: counted as a default seal", 1, dut.default_seals_o);
  }

  // =========================================================================
  // 8. THE 14x CONFUSION, FLATTERING DIRECTION: 2,341 as the REFERENCE reserve.
  // =========================================================================
  // This is the one that looks right. A planner that computed the giant's cost
  // in chunks and wrote it into the reference field has reserved a fourteenth
  // of the giant and every number in the plan looks small and reasonable.
  {
    reset(dut);
    Plan p = legal_with_giant();
    p.giant_refs = kGiantChunks;  // 2,341 REFERENCES -- 14x short
    expect_refusal(dut, p, kGiantTrim, "case8: a giant reserved in the wrong unit is TRIMMED");
  }

  // =========================================================================
  // 9. THE 14x CONFUSION, LOUD DIRECTION: 32,768 in the CHUNK field.
  // =========================================================================
  {
    reset(dut);
    Plan p = legal_no_giant();
    p.chunks = kGiantRefs;  // 200% of MAX_CHUNKS
    expect_refusal(dut, p, kChunksCap, "case9: references written into the CHUNK field");
  }

  // =========================================================================
  // 10. THE CASE THAT WOULD PASS UNDER THE CONFUSION, AND FAILS.
  // =========================================================================
  // A planner that believes the giant costs 2,341 of SOMETHING, and sizes the
  // ordinary reference budget against that belief, asks for
  // 32,768 - 2,341 = 30,427 ordinary references beside a guaranteed giant.
  // Under the 14x confusion that is a frame with comfortable headroom. In
  // truth the giant has taken every reference the binner owns, so the correct
  // answer is a refusal -- and the reason names REFERENCES, not chunks, which
  // is how the reader learns which unit was wrong.
  {
    reset(dut);
    Plan p = legal_with_giant();
    p.refs = kMaxRefs - kGiantChunks;  // 30,427
    expect_refusal(dut, p, kRefResv, "case10: ordinary references sized against a CHUNK reserve");
    check(p.refs == 30427, "case10: the confused planner's number, for the record", 30427, p.refs);
  }

  // A no-giant plan may not smuggle a reservation in: "released" must have a
  // consequence, or the flag is decoration.
  {
    reset(dut);
    Plan p = legal_no_giant();
    p.giant_refs = kGiantRefs;  // reservation without the flag
    expect_refusal(dut, p, kGiantStray, "case10b: a reservation with no giant declared");
    Plan q = legal_no_giant();
    q.flags = 0x02;  // a reserved flag bit
    expect_refusal(dut, q, kFlagsMbz, "case10c: a reserved flag bit is REFUSED, never masked");
    Plan r = legal_no_giant();
    r.view = 2;  // this console has views 0 and 1
    expect_refusal(dut, r, kViewId, "case10d: a plan for a view this console does not have");
  }

  // =========================================================================
  // 11-13. THE SELECTOR: a streaming max on {semantic_weight, instance_id}.
  // =========================================================================
  {
    reset(dut);
    publish(dut, legal_with_giant());  // giant_inst = 7
    frame_begin(dut);
    // Heaviest LAST: a selector that kept the first would pick 3.
    draw(dut, 10, 3);
    draw(dut, 200, 7);
    draw(dut, 50, 9);
    frame_end(dut);
    check(dut.draws_seen_o == 3, "case11: every draw reached the selector", 3, dut.draws_seen_o);
    check(dut.giant_mismatch_o == 0, "case11: the stream's max IS the sealed giant", 0,
          dut.giant_mismatch_o);
  }
  {
    reset(dut);
    Plan p = legal_with_giant();
    p.giant_inst = 4;  // the LOWER of the two tied instances
    publish(dut, p);
    frame_begin(dut);
    draw(dut, 200, 9);  // equal weight, higher id -- arrives first
    draw(dut, 200, 4);  // equal weight, lower id -- must win
    frame_end(dut);
    check(dut.giant_mismatch_o == 0, "case12: a tie breaks on the LOWEST instance identity", 0,
          dut.giant_mismatch_o);
  }
  {
    // THE RESET-VALUE TRAP. With {weight,instance} reset to {0,0}, a first draw
    // of weight 0 and instance 5 would lose to a comparison against nothing and
    // the frame would select instance 0, which never drew.
    reset(dut);
    Plan p = legal_with_giant();
    p.giant_inst = 5;
    publish(dut, p);
    frame_begin(dut);
    draw(dut, 0, 5);
    frame_end(dut);
    check(dut.giant_mismatch_o == 0, "case13: the first draw of a frame is taken unconditionally",
          0, dut.giant_mismatch_o);
  }

  // =========================================================================
  // 14-16. THE MISMATCH DETECTOR, FIRED TWICE AND THEN SILENCED.
  // =========================================================================
  // Its two operands are enabled by DIFFERENT things -- the selector by the
  // draw stream, the sealed identity by the frame edge -- which is why it can
  // fire at all. CLAUDE.md's metadata-bank law is about the version of this
  // check where both move together and the counter reads zero forever.
  {
    reset(dut);
    publish(dut, legal_with_giant());  // giant_inst = 7
    frame_begin(dut);
    frame_end(dut);  // the frame drew NOTHING
    check(dut.giant_mismatch_o == 1, "case14: a sealed giant the frame never drew", 1,
          dut.giant_mismatch_o);
  }
  {
    reset(dut);
    publish(dut, legal_with_giant());  // paid for instance 7
    frame_begin(dut);
    draw(dut, 250, 11);  // a HEAVIER instance than the one paid for
    frame_end(dut);
    check(dut.giant_mismatch_o == 1, "case15: the frame's heaviest draw is not the sealed giant",
          1, dut.giant_mismatch_o);
  }
  {
    // THE NEGATIVE CONTROL. A frame with NO declared giant draws whatever it
    // likes and the detector must stay silent -- otherwise case 14's fire is
    // not evidence, it is the counter's normal state.
    reset(dut);
    publish(dut, legal_no_giant());
    frame_begin(dut);
    draw(dut, 250, 11);
    frame_end(dut);
    check(dut.giant_mismatch_o == 0, "case16: no declared giant, nothing to disagree with", 0,
          dut.giant_mismatch_o);
  }

  // =========================================================================
  // 17. A SEALED FRAME IS NOT RENEGOTIATED.
  // =========================================================================
  //   > "Do not renegotiate a sealed frame or borrow from the giant's
  //   >  reservation because an early primitive happens to fit."
  {
    reset(dut);
    Plan a = legal_no_giant();
    a.chunks = 5000;
    publish(dut, a);
    const Offered first = frame_begin(dut);
    check(first.chunks == 5000, "case17: the frame sealed at the first plan's number", 5000,
          first.chunks);

    // A second plan arrives while the frame is open. It must stage for the NEXT
    // frame and touch nothing.
    Plan b = legal_no_giant();
    b.chunks = 9000;
    publish(dut, b);
    dut.eval();
    check(dut.seal_chunks_o == 9000,
          "case17: the OFFERED record follows the staged plan between frames", 9000,
          dut.seal_chunks_o);
    check(dut.seal_valid_o == 0, "case17: but no seal is offered outside a frame edge", 0,
          dut.seal_valid_o);
    frame_end(dut);

    const Offered second = frame_begin(dut);
    check(second.chunks == 9000, "case17: the NEXT frame takes the second plan", 9000,
          second.chunks);
    check(second.frame_gen != first.frame_gen,
          "case17: and no two consecutive frames share a generation stamp", 1,
          second.frame_gen != first.frame_gen ? 1 : 0);
  }

  // =========================================================================
  // 18-19. THE GENERATIONS, AND A CHECK WHOSE SIDES MOVE SEPARATELY.
  // =========================================================================
  {
    reset(dut);
    Plan p = legal_no_giant();
    p.res_gen = 0;  // priced against the console's current resource generation
    publish(dut, p);
    // ...and then a resource is published. The plan is now priced against a
    // resource set that no longer exists.
    dut.res_bump_i = 1;
    zhao::tick(dut);
    dut.res_bump_i = 0;
    dut.eval();
    const uint32_t before = dut.plans_refused_o;
    const Offered o = frame_begin(dut);
    check(!o.valid, "case18: a plan priced before a resource publication is REFUSED", 0,
          o.valid ? 1 : 0);
    check(dut.refuse_reason_o == kResGen, "case18: and the reason names the resource generation",
          kResGen, dut.refuse_reason_o);
    check(dut.plans_refused_o == before + 1, "case18: counted", before + 1, dut.plans_refused_o);

    // The positive control: the SAME plan, priced against the new generation.
    Plan q = legal_no_giant();
    q.res_gen = 1;
    publish(dut, q);
    const Offered o2 = frame_begin(dut);
    check(o2.valid, "case18b: repriced against the new generation, it seals", 1, o2.valid ? 1 : 0);
  }
  {
    reset(dut);
    Plan p = legal_no_giant();
    publish(dut, p);
    dut.view_bump_i = 1;
    zhao::tick(dut);
    dut.view_bump_i = 0;
    dut.eval();
    const Offered o = frame_begin(dut);
    check(!o.valid, "case19: a plan priced before a view matrix write is REFUSED", 0,
          o.valid ? 1 : 0);
    check(dut.refuse_reason_o == kViewGen, "case19: and the reason names the view generation",
          kViewGen, dut.refuse_reason_o);
  }

  // =========================================================================
  // 20. THE ARENA NOT READY. The edge is lost and the plan is KEPT.
  // =========================================================================
  // The plan described this view and is still the right plan for the next
  // edge, so it must not be consumed by an edge that did nothing.
  {
    reset(dut);
    Plan p = legal_no_giant();
    p.chunks = 7777;
    publish(dut, p);
    dut.seal_ready_i = 0;
    const Offered o = frame_begin(dut);
    check(o.valid, "case20: the request IS offered even when the arena cannot take it", 1,
          o.valid ? 1 : 0);
    check(dut.seal_lost_o == 1, "case20: and the lost edge is counted", 1, dut.seal_lost_o);
    check(dut.plans_sealed_o == 0, "case20: nothing was sealed", 0, dut.plans_sealed_o);
    dut.seal_ready_i = 1;
    dut.eval();
    const Offered o2 = frame_begin(dut);
    check(o2.chunks == 7777, "case20: the plan survived the lost edge", 7777, o2.chunks);
    check(dut.plans_sealed_o == 1, "case20: and sealed on the next one", 1, dut.plans_sealed_o);
  }

  // =========================================================================
  // 21. THE DEFAULT PLAN, WITH NOTHING PUBLISHED.
  // =========================================================================
  // It is a NO-GIANT plan at capacity and it is COUNTED, so a console that
  // never publishes a plan can be seen never to have published one. That
  // visibility is the difference between this and the constant it replaced.
  {
    reset(dut);
    const Offered o = frame_begin(dut);
    check(o.valid, "case21: a console with no published plan still seals", 1, o.valid ? 1 : 0);
    check(dut.default_seals_o == 1, "case21: and it is counted as a DEFAULT seal", 1,
          dut.default_seals_o);
    check(dut.plans_sealed_o == 0, "case21: not as a plan seal", 0, dut.plans_sealed_o);
    check(o.giant_refs == 0, "case21: with the reservation released, per the directive", 0,
          o.giant_refs);
    check(dut.plans_staged_o == 0, "case21: nothing was ever staged", 0, dut.plans_staged_o);
  }

  // =========================================================================
  // 22. A HELD `frame_begin_i` SEALS EXACTLY ONCE.
  // =========================================================================
  // THIS IS A REPAIR, NOT A PROPERTY THAT WAS ALWAYS TRUE, and the case exists
  // because the defect was live in the composition this block replaced.
  //
  // `render_frame_begin_i` is `zhao_renderer_lease_v2`'s `frame_req_valid_i`,
  // a ready/valid request, and `tb_zhao_console_core_smoke.sv` HOLDS IT until
  // the lease admits a frame -- measured at 2,531 cycles. The previous
  // composition drove `seal_valid_i` from that level directly, and
  // `zhao_geom_paramarena`'s `seal_fire_c` FLIPS THE VIEW and zeroes the three
  // allocation cursors. So one frame re-sealed the arena 2,531 times.
  //
  // It was invisible because that bench releases its draws one line AFTER the
  // level drops, so nothing had been allocated to lose. A console that let
  // geometry flow while the lease was still being granted would have lost it
  // silently and reported a clean, short frame.
  //
  // Sixty cycles here rather than 2,531: the property is "once per edge", and
  // it does not get more true with more cycles.
  {
    reset(dut);
    Plan p = legal_no_giant();
    p.chunks = 1234;
    publish(dut, p);

    dut.frame_begin_i = 1;
    dut.eval();
    const uint32_t gen_at_edge = dut.seal_frame_gen_o;
    for (int i = 0; i < 60; ++i) zhao::tick(dut);
    dut.frame_begin_i = 0;
    dut.eval();

    check(dut.plans_sealed_o == 1, "case22: a level held for 60 cycles sealed ONCE", 1,
          dut.plans_sealed_o);
    check(dut.default_seals_o == 0, "case22: and not as a default", 0, dut.default_seals_o);
    // The generation advanced by exactly one. Under the old arrangement it
    // advanced once per held cycle, which is the same defect read from the
    // other side: no two consecutive FRAMES shared a stamp because no two
    // consecutive CYCLES did.
    check(dut.seal_frame_gen_o == gen_at_edge + 1,
          "case22: and the frame generation advanced by exactly one", gen_at_edge + 1,
          dut.seal_frame_gen_o);
    check(dut.seal_valid_o == 0, "case22: the request is released once it has fired", 0,
          dut.seal_valid_o);

    // A SECOND frame, so "once per edge" is not "once ever".
    frame_end(dut);
    Plan q = legal_no_giant();
    q.chunks = 4321;
    publish(dut, q);
    dut.frame_begin_i = 1;
    dut.eval();
    for (int i = 0; i < 20; ++i) zhao::tick(dut);
    dut.frame_begin_i = 0;
    dut.eval();
    check(dut.plans_sealed_o == 2, "case22: the next edge seals again, once", 2,
          dut.plans_sealed_o);
  }

  std::printf("[measure_sealplan_directed] GIANT_REFS=%u  GIANT_CHUNKS=%u  "
              "MAX_REFS=%u  MAX_CHUNKS=%u  ordinary chunk quota with a giant=%u\n",
              kGiantRefs, kGiantChunks, kMaxRefs, kMaxChunks, kMaxChunks - kGiantChunks);

  zhao::exit_hard(zhao::report_and_exit("measure_sealplan_directed"));
}
