// zhao_texture_island_v3_top.sv — the composed texture island, V3 OWNERSHIP.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS, AND WHY IT IS A SECOND TOP RATHER THAN AN EDIT
// ---------------------------------------------------------------------------
// P0-C Stage C. This composition replaces `zhao_texture_fragrob`'s ownership and
// lifetime machinery with `zhao_texture_v3own` plus the fragment expander
// `zhao_texture_frag_expand`, and deletes the top-level ordering and result
// state the replacement makes redundant.
//
// `zhao_texture_island_top.sv` IS UNTOUCHED AND STAYS THAT WAY. It is the
// end-to-end ORACLE for this stage's paired run -- the recovery brief §6.5:
// "first keep the current island behavior as an end-to-end oracle". Two tops
// driven with identical stimulus, retired streams compared byte for byte on
// rgb/a/tag/refused and ORDER. Editing the old top destroys the comparison that
// makes this stage checkable at all.
//
// `zhao_texture_v3own.sv` IS ALSO NOT EDITED. Its 541-check adversarial suite
// reads internal probes (`c3t_we_q`, `cmt_q`, the `vgen_q` shadow) through
// `verilator public` markers, and that suite passing on the UNMODIFIED file is
// gate 1 of three. Every adapter lives here or in the expander. If the
// integration turns out to need v3own changed, that is a FINDING about the
// architecture -- not a licence to edit.
//
// Plan:      reports/P0C-RESTRUCTURE-ARCHITECTURE-20260907.md
// Inventory: reports/P0C-STAGEC-PORT-INVENTORY-20260908.md
// Expander:  reports/P0C-STAGEB-EXPANDER-CONTRACT-20260908.md
//
// ---------------------------------------------------------------------------
// BUILD STATE: INCOMPLETE -- REFERENCED BY NOTHING
// ---------------------------------------------------------------------------
// Built incrementally and deliberately wired into nothing until finished: no
// `design/fit_targets.yml` entry, no `tests/CMakeLists.txt` target. A half-built
// top that nothing instantiates cannot break a build, a fit or a golden, which
// is why it can land in reviewable pieces instead of one unreadable commit.
//
// DONE:  seeded from the oracle so the ELEVEN carried instantiations are
//        verbatim (rcp24_svc, perspuv_svc, mosaic, tmu_plan, cache_pipe,
//        rsp_dispatch, bilerp_lane, palette_res, aux_pipe, combine_v2);
//        module renamed.
// TO DO: delete the fragrob instance and its glue; instantiate
//        zhao_texture_frag_expand and zhao_texture_v3own; re-key identity to
//        v3own's 14-bit handle; widen SRCW 16 -> 18 through plan/cache/dispatch;
//        delete the ROB pool, `fseq_m`, `live_r`/`tok_r` and the side tables
//        the deletion ledger names.
//
// The ORACLE'S OWN HEADER IS NOT CARRIED OVER. It is ~88 lines describing
// fragrob's reorder buffer, its DEPTH=16 admission model and the SRCW-16 id
// constraint -- all true of the oracle and none of it true here once the TO DO
// list above is worked. Copying it would have produced a header describing
// hardware this file does not contain, which is precisely the "vertex RGB: lit,
// tinted and ALREADY FOGGED" defect this repository paid for earlier today: a
// confident description of a stage that did not exist.
//
// What the oracle's header says about the CARRIED blocks is still true of them,
// and `zhao_texture_island_top.sv` remains its home. This file documents what it
// actually contains, as it comes to contain it.

module zhao_texture_island_v3_top #(
    // THE MIGRATION LABORATORY, and whether this elaboration carries it.
    //
    // Decrufter D1/§4.2: one functional source, two elaborations. The lab
    // profile keeps every shadow comparator; the production profile must not
    // elaborate the reference table, the comparison cones or their counters.
    //
    // The brief is explicit that this is NOT permission to disable a checker
    // and call the result healthy, which is why `shadow_present_o` exists: a
    // test must assert the CAPABILITY before it may believe a shadow counter's
    // zero. Default 1, so every existing test elaborates unchanged.
    parameter bit MIGRATION_SHADOWS = 1'b1,
    parameter int unsigned DEPTH   = 16,   // FRAGROB reorder depth
    parameter int unsigned CTXW    = 64,
    parameter int unsigned BINDW   = 8,
    // LODW IS 8 BECAUSE THE PLANNER'S LOD IS Q4.4, NOT AN INTEGER LEVEL.
    // `zhao_texture_tmu_plan` selects the level with `t0_lod[7:4]` and leaves
    // [3:0] as the FRACTION. This was 4, and the connection below padded it
    // into the LOW nibble -- so every possible value became 0x00..0x0F, the
    // planner's integer level was ALWAYS ZERO, and enabling MIP_EN could not
    // produce a non-zero mip however the binding was set. Everything the
    // planner already implements -- clamping, selected-level UV scaling,
    // packed mip-chain offsets -- was unreachable through that wiring.
    //
    // Inert at this commit: the composed test drives `bind_mode_i = 0`, so
    // MIP_EN is low and `lvl_req` is 0 either way. The fix is here because the
    // FRACTION it recovers is the blend weight the two-level mip blend needs,
    // so the defect and that feature share one repair (docket D23).
    parameter int unsigned LODW    = 8,
    parameter int unsigned GENW    = 8,
    parameter int unsigned LANES   = 4,    // CACHE_PIPE lanes
    // SRCW 16 -> 18: {class[1:0], slot[5:0], sidx[1:0], gen[7:0]}. The oracle
    // keeps 16, where the slot is 4 bits and the pad term is exactly zero.
    parameter int unsigned SRCW    = 18,
    parameter int unsigned DATAW   = 64,   // RSP_DISPATCH payload = LANES*16
    // TOKW 16 -> 18 UNDER P0-C. This is the island's ROUTING token
    // {class[1:0], slot[5:0], sidx[1:0], gen[7:0]}, and every net declared
    // `[TOKW-1:0]` follows it -- the dispatch's four per-lane outputs, the
    // response token, the bilerp and palette lanes. The oracle keeps 16.
    //
    // §1.1 named three consumers to widen; the parameter is how they stay
    // consistent instead of four `[17:0]` literals drifting apart.
    parameter int unsigned TOKW    = 18,
    parameter int unsigned PAL_SLOTS   = 4,
    parameter int unsigned PAL_ENTRIES = 256,
    // AUX_TOKW CARRIES THE WHOLE IDENTITY OR IT CARRIES A BUG.
    //
    // The oracle learned this the expensive way: AUX_PIPE's `TOKW` defaults to
    // 8, the identity it had to round-trip was $clog2(DEPTH)+GENW = 12, and
    // every aux response came back with the slot sitting where the generation
    // should be. FRAGROB counted exactly 7 ID errors against exactly 7 aux
    // requests, which is how it was found. The token is a parameter; it just
    // had to be told how wide the identity IS.
    //
    // UNDER P0-C THE IDENTITY IS v3own's OWNER HANDLE, so the same rule gives a
    // different number: OWNERW = SLOTW + GENW = 6 + 8 = **14**, not 12. Setting
    // it from the oracle's `$clog2(DEPTH) + GENW` would reproduce the original
    // defect exactly — a token one field too narrow, and an identity that
    // cannot round-trip.
    //
    // THIS IS AN ISLAND BOUNDARY PORT (`sheet_tok_o`, `sheet_rtok_i`), which
    // makes it the first P0-C change the paired-run oracle can SEE. The SRCW
    // widening could default to 16 and leave every existing instantiation
    // bit-identical; this one cannot, because a boundary width is the composed
    // top's contract with whatever drives it. Recorded in the architecture as
    // §1.2b, with the measurement that `zhao_texture_aux_pipe.sv` is
    // parameterised throughout and needs NO leaf change.
    parameter int unsigned AUX_TOKW = 6 + GENW
) (
    input  var logic        clk,
    input  var logic        rst_n,

    // ======================= island boundary: fragments in ==================
    input  var logic        frag_valid_i,
    output var logic        frag_ready_o,
    input  var logic [23:0] frag_depth_i,       // w, for the reciprocal
    input  var logic [31:0] frag_u_over_w_i,
    input  var logic [31:0] frag_v_over_w_i,
    input  var logic [1:0]  frag_sample_count_i,
    input  var logic [BINDW-1:0] frag_binding_i,
    input  var logic [LODW-1:0]  frag_lod_i,
    input  var logic [2:0]  frag_recipe_i,
    input  var logic [7:0]  frag_weight_i,
    input  var logic [CTXW-1:0] frag_ctx_i,
    input  var logic        frag_aux_i,
    input  var logic [23:0] frag_base_rgb_i,
    input  var logic [7:0]  frag_base_a_i,
    // THE SAMPLE CLASS, per fragment. It was a hardcoded CLASS_BILINEAR, which
    // left the palette path wired and permanently idle -- `palette 0` in every
    // composed run. The class belongs at the boundary because in the machine it
    // comes from the material binding, which is upstream of this island.
    input  var logic [1:0]  frag_class_i,

    // THE FRAGMENT'S PALETTE BINDING -- which CLUT it samples and which upload
    // generation it expects. This is a MATERIAL BINDING and belongs to the
    // fragment. It used to be taken from the response routing token, which is
    // a different namespace entirely; see the palette wiring below.
    input  var logic [$clog2(PAL_SLOTS)-1:0] frag_pal_slot_i,
    input  var logic [GENW-1:0]              frag_pal_gen_i,

    // ======================= island boundary: binding table =================
    input  var logic [31:0] bind_base_i,
    input  var logic [31:0] bind_mode_i,

    // ======================= island boundary: memory fill ===================
    output var logic        fill_valid_o,
    input  var logic        fill_ready_i,
    output var logic [31:0] fill_addr_o,
    input  var logic        fill_data_valid_i,
    input  var logic [15:0] fill_data_i,

    // ======================= island boundary: palette upload ================
    input  var logic        pal_ld_valid_i,
    input  var logic [1:0]  pal_ld_op_i,
    input  var logic [$clog2(PAL_SLOTS)-1:0]   pal_ld_slot_i,
    input  var logic [GENW-1:0]                pal_ld_gen_i,
    input  var logic [$clog2(PAL_ENTRIES)-1:0] pal_ld_idx_i,
    input  var logic [15:0] pal_ld_rgb565_i,
    input  var logic        pal_ld_crc_ok_i,

    // ======================= island boundary: aux sheet =====================
    input  var logic        sheet_rvalid_i,
    input  var logic [7:0]  sheet_tag_i,
    input  var logic [7:0]  sheet_str_i,
    input  var logic [AUX_TOKW-1:0] sheet_rtok_i,
    output var logic        sheet_valid_o,
    input  var logic        sheet_ready_i,
    output var logic [5:0]  sheet_u_o,
    output var logic [5:0]  sheet_v_o,
    // THE SHEET REQUEST'S TOKEN, which the first draft left
    // unconnected -- `.sheet_tok_o()`. AUX_PIPE matches a sheet response
    // to its request by this token, so with it dangling the responder
    // had nothing to echo, every aux result carried a wrong identity,
    // and FRAGROB rejected all of them. Because FRAGROB retires in
    // ALLOCATION ORDER, one aux fragment stuck at the head blocked the
    // entire island: 48 samples fetched, 0 fragments out.
    output var logic [AUX_TOKW-1:0] sheet_tok_o,

    // ======================= island boundary: fragments out =================
    output var logic        out_valid_o,
    input  var logic        out_ready_i,
    output var logic [23:0] out_rgb_o,
    output var logic [7:0]  out_a_o,
    output var logic [15:0] out_tag_o,
    output var logic        out_refused_o,
    // Times a fragment finished ahead of an earlier one and had to wait at the
    // boundary (audit R6). Zero on a run means the ordering guarantee was
    // never TESTED, not that it holds.
    // ---- TRIPWIRES THAT WERE DANGLING -------------------------------------
    // fr_wq_overflow, fr_id_error and aux_degenerate were declared, connected
    // to their submodule's output port, and READ BY NOTHING -- each name
    // appears exactly twice in this file, the declaration and the connection.
    // A tripwire nobody can see is decoration: the block dutifully raised it
    // and synthesis dutifully deleted it.
    //
    // V3 section 0 point G says to preserve these, and preserving something
    // requires being able to observe it, so they reach the boundary now rather
    // than after the rewrite -- otherwise "preserved" would mean "still
    // invisible, in a new file".
    //
    // STICKY, not pass-through. Each is a fault, and a fault that is true for
    // one clock in a hundred thousand is exactly the one a sampling consumer
    // misses; latching means a test that looks once at the end still sees it.
    // ENFORCED-BY: tests/texture/island_composed_directed.cpp
    output var logic err_fragrob_wq_overflow_o,
    output var logic err_fragrob_id_error_o,
    output var logic err_aux_degenerate_o,
    output var logic [31:0] cnt_reorder_held_o,
    // High-water mark of live owner credits. A run whose peak never approached
    // OWNER_DEPTH has not tested the ceiling, whatever else it proved.
    output var logic [31:0] cnt_live_peak_o,

    // ======================= one summary counter per block ==================
    // Deliberately NOT every counter every block owns. Each output is a pin,
    // and 60 counter pins would add I/O registers that inflate the very number
    // this file exists to measure honestly. One per block keeps each block's
    // counter logic alive -- so it is not optimised away and the measurement
    // stays truthful -- without paying for the whole census.
    output var logic [31:0] cnt_fragments_o,
    output var logic [31:0] cnt_cache_hits_o,
    output var logic [31:0] cnt_cache_misses_o,
    output var logic [31:0] cnt_palette_lookups_o,
    output var logic [31:0] cnt_bilerp_jobs_o,
    output var logic [31:0] cnt_mosaic_samples_o,
    output var logic [31:0] cnt_aux_accepted_o,
    output var logic [31:0] cnt_combine_refused_o,
    // V2 counts PHASE LAUNCHES, not fragments. A recipe costing two phases
    // must show two here; a combiner quietly re-issuing work shows it as a
    // phase count that outruns the schedule, which is the one thing a
    // colour-checking test cannot see.
    output var logic [31:0] cnt_combine_phases_o,
    output var logic [31:0] cnt_rcp_completed_o,
    output var logic [31:0] cnt_persp_fragments_o,
    output var logic [31:0] cnt_dispatch_accepted_o,
    output var logic [31:0] cnt_plan_accepted_o,
    // FRAGROB rejects a sample response whose slot/sidx/generation does
    // not match what it issued. Exposed because a composed island that
    // accepts fragments and retires none is either starved or REJECTING,
    // and those need different fixes.
    output var logic [31:0] cnt_fragrob_id_errors_o,
    // Packet C step 1: the metadata bank's shadow falsifier. Must stay ZERO.
    // THE CAPABILITY CONTRACT. Constant, not a counter: it says whether the
    // shadow machinery is present at all. Every zero read from the counters
    // below is meaningless unless this is 1, and the production build asserts
    // it is 0 and skips those checks BY THAT EVIDENCE rather than by
    // assumption. That distinction is the whole of §4.2.
    output var logic        shadow_present_o,
    output var logic [31:0] meta_shadow_mismatch_o,
    // How many responses the shadow actually compared. A mismatch count of
    // zero over zero comparisons is not evidence.
    output var logic [31:0] meta_shadow_reads_o,
    // Packet C step 2: is the QUEUED metadata the right response's?
    output var logic [31:0] meta_align_err_o,
    output var logic [31:0] meta_align_chk_o,
    output var logic [31:0] meta_bil_err_o,
    output var logic [31:0] meta_bil_chk_o,
    output var logic [31:0] meta_near_err_o,
    output var logic [31:0] meta_near_chk_o,
    output var logic [20:0] meta_bil_first_q_o,
    output var logic [20:0] meta_bil_first_t_o,
    output var logic [17:0] meta_bil_first_tok_o,
    // The bank's own alignment check: did a response name a generation the
    // row was not written for? That is slot recycling, and it decides which
    // side of a fraction disagreement is the stale one.
    output var logic [31:0] meta_genmis_o,
    // COMBINE.V1's per-recipe product-job counts, at the island boundary.
    //
    // §15.4 requires actual product jobs recorded by recipe, and until now they
    // stopped inside the combiner. Bringing them out is what lets a composed
    // test prove PER-FRAGMENT RECIPE IDENTITY: if the recipe did not travel
    // with its fragment, every fragment would combine with whichever recipe
    // arrived last and exactly one counter would move. That gap was recorded
    // against the ENFORCED-BY note below and this closes it.
    output var logic [31:0] cnt_combine_jobs_o [8],

    // PALETTE RESIDENCY OUTCOME. These two are counters and not diagnostics
    // because "the lookup happened" and "the lookup found its palette" are
    // different facts, and only the first was observable. Every CLUT fragment
    // retired black for a whole pass while `cnt_palette_lookups_o` moved
    // healthily; the fault was 96 lookups all STALE, and nothing exposed that.
    // A path that answers with a miss indication is doing no work.
    output var logic [31:0] cnt_palette_stale_o,
    output var logic [31:0] cnt_palette_cold_o,

    // STICKY: a sample response was produced and nobody took it. One bit and
    // not a count, because the question is "did this ever happen", and one
    // occurrence already means a fragment waits forever. See the completion
    // merger below for why this is currently unreachable and why that is an
    // assumption rather than a property.
    output var logic        err_rsp_dropped_o,

    // Sticky: the bilinear lane retired a channel out of order. The three-
    // channel accumulator pairs R, G and B by ARRIVAL ORDER, so a reordering
    // would silently combine one sample's red with another's blue and every
    // direct-colour pixel would be wrong with no counter moving.
    output var logic        err_bil_chan_o,

    // ======================= TYPED SAMPLE COMPLETION EVIDENCE ===============
    // Pre-fit brief section 5A. Every one of these counts a sample that used
    // to VANISH -- and a vanished sample is not a wrong pixel, it is a hang:
    // `zhao_texture_fragrob.sv:470-472` retires only when arrivals exactly
    // equal requests, there is no timeout in that file, and retirement is
    // allocation ordered, so ONE lost sample parks the head forever and the
    // whole island stops. Each counter below therefore marks the conversion of
    // a deadlock into a visible, retiring error pixel.
    //
    // These are counts and not stickies because the interesting question is
    // "how much of the frame was wrong", and because a single occurrence no
    // longer means the island is dead.

    // (a) Class-1 (direct nearest) responses given a terminal REFUSAL
    // completion. Refusal and not colour, because the island has no
    // format-controlled decode yet -- see the merger below.
    output var logic [31:0] cnt_near_refused_o,

    // (b) Responses whose class was 3. Routed to an error completion by
    // RSP_DISPATCH instead of being popped into nothing.
    output var logic [31:0] err_unknown_class_o,

    // (b, boundary half) Fragments admitted with `frag_class_i == 3`. The
    // class is sanitised at the island pin so a class 3 cannot reach the
    // planner at all; this counts how often that sanitisation fired.
    output var logic [31:0] err_class_invalid_o,

    // (c) Palette lookups the LEAF ITSELF marked unusable -- stale or
    // non-resident -- which used to be shipped as ordinary colour.
    output var logic [31:0] err_palette_unusable_o,

    // (e) Requests where the class carried in the source id disagreed with the
    // class implied by the planner's own `acc_fmt_o`/`acc_filter_o`
    // resolution. Two authorities, no comparison, until now.
    output var logic [31:0] err_class_mismatch_o,

    // (e, second half) Sticky: the planner raised `acc_err_o` on an accepted
    // request. That output was connected and never read by anything, so a
    // sanitised or rejected mode was completely invisible at the island
    // boundary.
    output var logic        err_plan_mode_o
);

  // ==========================================================================
  // THE SAMPLE CLASS AND FORMAT ENCODINGS, NAMED ONCE
  // ==========================================================================
  // These were literals scattered across the file, which is how (e) survived:
  // the island routed on `2'd0/2'd2` bit patterns and the planner resolved
  // `FMT_*` symbols, and nothing put the two vocabularies in the same place
  // where they could be compared.
  //
  // SOURCE OF TRUTH for the class encoding is `zhao_texture_rsp_dispatch.sv`
  // (CLS_CLUT/CLS_NEAR/CLS_BIL); for the format encoding it is
  // `zhao_texture_tmu_plan.sv` (FMT_CLUT8..FMT_ARGB4444). Neither exports a
  // package, so these are copies; they are grouped here so a divergence is one
  // diff rather than a hunt.
  localparam logic [1:0] CLS_CLUT = 2'd0;
  localparam logic [1:0] CLS_NEAR = 2'd1;
  localparam logic [1:0] CLS_BIL  = 2'd2;
  localparam logic [1:0] CLS_ERR  = 2'd3;   // not a legal request class

  localparam logic [2:0] FMT_CLUT8    = 3'd0;
  localparam logic [2:0] FMT_RGB565   = 3'd1;
  localparam logic [2:0] FMT_CLUT4    = 3'd2;
  localparam logic [2:0] FMT_ARGB1555 = 3'd3;
  localparam logic [2:0] FMT_ARGB4444 = 3'd4;

  // THE DERIVATION THE ISLAND SHOULD HAVE BEEN ROUTING ON. Class is a FUNCTION
  // of {format, filter} and was never an independent fact; see (e) below.
  // EVERY FORMAT LISTED, none folded into a default, so that adding one to the
  // planner and forgetting it here is an unroutable CLS_ERR and a counted
  // mismatch rather than a silent reuse of whatever the last arm decided.
  function automatic logic [1:0] class_of(input logic [2:0] fmt,
                                          input logic       filt);
    case (fmt)
      FMT_CLUT8, FMT_CLUT4:
        // A palette is never filtered -- the planner forces `filter_eff` low
        // for a CLUT and raises `acc_err_o` if one was asked for, so `filt` is
        // deliberately not consulted on this arm.
        class_of = CLS_CLUT;
      FMT_RGB565, FMT_ARGB1555, FMT_ARGB4444:
        class_of = filt ? CLS_BIL : CLS_NEAR;
      default:
        // Above FMT_ARGB4444. This is the planner's own `fmt_bad`, and it
        // cannot route anywhere; it exists so the comparison below reports it.
        class_of = CLS_ERR;
    endcase
  endfunction

  // ==========================================================================
  // DEFECT (d), FIRST HALF: THE ONE SHARED FORMAT-CONTROLLED DECODE
  // ==========================================================================
  // ONE BLOCK, NOT THREE. Section 5A.7 item 5. Before this, the island had two
  // half-decodes and one refusal: `chan8()` extracted RGB565 channels with no
  // format input, the nearest station returned `near_ok_c = 1'b0` because it
  // had nothing to decode with, and alpha was the literal 8'hFF. All three are
  // the SAME missing law, so all three are now this one function.
  //
  // IT IS NOT A NEW LAW. This is `decode16` from `zhao_texture_tmu.sv` and
  // `zhao_texture_tmu_pipe.sv`, transcribed unchanged, and it is the same
  // arithmetic as the reference oracle in `reference/src/zrender/texture.cpp`
  // (the direct-colour arm of `zref::Tmu::sample`, with `exp5`/`exp4`) --
  // which is what the composed test compares against, so a divergence here
  // shows up as a colour mismatch rather than as a passing run.
  //
  //   5 -> 8 and 6 -> 8 are the FROZEN expansions (spec/stars_and_flares.md
  //   section 2, `zref::sky::rgb565::to_rgb888`): replicate the high bits, so
  //   31 -> 255 and 63 -> 255 exactly. Zero-fill caps every channel below full
  //   scale and is the AUDIT R5/D23 defect repaired for the palette below.
  //
  //   4 -> 8 is `{n, n}` and 1 -> 8 is 0 or 255. Those two laws are UNWRITTEN
  //   in the spec; bit replication is the consistent extension of the two that
  //   are written -- the unique expansion that round-trips both 0 and full
  //   scale -- and the reference implements exactly this, so the choice is
  //   recorded in two places that are checked against each other.
  //
  // THE RETURN IS {a, r, g, b}, alpha in the HIGH byte, so `[23:0]` is the
  // colour the response port already carries and `[31:24]` is the alpha it
  // never had.
  //
  // RGB565 IS THE `default` ARM ON PURPOSE. A format that reaches here and is
  // not a direct one is a routing fault, not a colour: it is REFUSED before
  // this value is used (see `fmt_is_direct` and `near_ok_c`), so the arm keeps
  // the function total rather than defining a decode for CLUT8.
  function automatic logic [31:0] decode16(input logic [15:0] h, input logic [2:0] fmt);
    logic [7:0] a_, r_, g_, b_;
    begin
      case (fmt)
        FMT_ARGB1555: begin
          a_ = h[15] ? 8'd255 : 8'd0;
          r_ = {h[14:10], h[14:12]};
          g_ = {h[9:5],   h[9:7]};
          b_ = {h[4:0],   h[4:2]};
        end
        FMT_ARGB4444: begin
          a_ = {h[15:12], h[15:12]};
          r_ = {h[11:8],  h[11:8]};
          g_ = {h[7:4],   h[7:4]};
          b_ = {h[3:0],   h[3:0]};
        end
        default: begin  // FMT_RGB565
          a_ = 8'd255;
          r_ = {h[15:11], h[15:13]};
          g_ = {h[10:5],  h[10:9]};
          b_ = {h[4:0],   h[4:2]};
        end
      endcase
      decode16 = {a_, r_, g_, b_};
    end
  endfunction

  // IS THIS FORMAT ONE THIS DECODE CAN ANSWER? Every direct-colour format is
  // listed by name and nothing is folded into a default, for the same reason
  // `class_of` above lists them all: adding a format to the planner and
  // forgetting it here must be a COUNTED REFUSAL, not a silent reuse of the
  // RGB565 arm. A CLUT format arriving at a direct-colour station is a routing
  // fault and lands here as `0`, which is what `cnt_near_refused_o` counts.
  function automatic logic fmt_is_direct(input logic [2:0] fmt);
    case (fmt)
      FMT_RGB565, FMT_ARGB1555, FMT_ARGB4444: fmt_is_direct = 1'b1;
      default:                                fmt_is_direct = 1'b0;
    endcase
  endfunction

  // THE ERROR COLOUR EVERY REFUSED OR FAILED SAMPLE COMPLETES WITH.
  //
  // LOUD ON PURPOSE. The alternative -- black, or alpha 0 -- makes a failed
  // sample look like a legitimately dark or transparent one, and the entire
  // point of the typed-completion repair is that a failure stops being
  // indistinguishable from ordinary output. Magenta at full alpha is visible in
  // one glance at a frame and cannot be mistaken for art.
  //
  // It is a NAMED CONSTANT and not a literal because the owner must be able to
  // change it: a debug build wants it screaming, a shipping build may want it
  // to match the surrounding material. That choice belongs to whoever is
  // looking at the picture.
  localparam logic [23:0] SMP_ERR_RGB = 24'hFF00FF;
  localparam logic [7:0]  SMP_ERR_A   = 8'hFF;

  // ==========================================================================
  // RCP24 -> PERSPUV
  // ==========================================================================
  logic        rcp_r_valid, rcp_r_ready;
  logic [23:0] rcp_r;
  logic [5:0]  rcp_k;
  logic        rcp_dzero;
  logic [13:0] rcp_tok;   // v3own's {slot[5:0], generation[7:0]}
  logic        rcp_v_ready;
  logic [31:0] rcp_accepted, rcp_mul_busy;
  logic [3:0]  rcp_occ;

  // The island's own fragment counter, used as the token so a response can be
  // matched to its request. Eight bits is RCP24's TOKW.
  // `tok_r` DELETED: it stamped an ingress identity v3own now assigns, and the
  // RCP carries the owner handle itself (`.v_tok_i(own_adm_owner)`).

  // ==========================================================================
  // THE END-TO-END OWNER CREDIT (owner recovery brief, prerequisite 1)
  //
  // MY OVERFLOW PROOF WAS WRONG, and the brief's counterexample is exact:
  //
  //     hold the sink not-ready; admit and complete sequences 0..63; entry 0
  //     still holds sequence 0; INTERNAL CONTEXTS HAVE BEEN RELEASED AND ADMIT
  //     MORE WORK; sequence 64 completes and overwrites entry 0.
  //
  // The reorder buffer's comment claimed it "cannot overflow: FRAGROB admits at
  // most FCTXN fragments, so at most FCTXN sequence numbers are live". FRAGROB
  // releases a context at ITS retirement, which is upstream of MATERIAL.COMBINE
  // and upstream of this buffer. What is parked at the output is therefore NOT
  // bounded by what FRAGROB is holding, and a 64-fragment test cannot reach the
  // wrap. A false "cannot overflow" is worse than the bug: it tells the next
  // reader not to check.
  //
  // The credit is the fix the brief specifies. Reserve one owner slot at
  // ADMISSION; return it ONLY at final external output acceptance -- not at
  // RCP or PERSPUV completion, not at FRAGROB retirement, not at COMBINE
  // completion, and not on transfer into any output stage.
  //
  // BOTH SIDES ARE GATED. `frag_ready_o` alone is not enough: RCP's own
  // `v_valid_i` must be gated too, or RCP accepts a job the caller was told did
  // not handshake -- a phantom that owns no slot and retires into somebody
  // else's.
  //
  // NO SAME-CYCLE BYPASS of the full condition using the outgoing result. A
  // conservative one-cycle bubble at full-to-free avoids a cyclic ready path
  // and a same-slot clear/write hazard; the brief says to optimise it only
  // after the invariant is proven, and it is right.
  // ==========================================================================
  // (c1) ADMISSION MOVES TO THE ISLAND BOUNDARY, AND live_r IS DELETED
  // ==========================================================================
  // The architecture's central decision: "admit at the island boundary and make
  // v3own's 14-bit owner handle the island's single identity namespace end to
  // end -- the island's existing `live_r` credit, `tok_r`, `fseq_m` and output
  // ROB are partial implementations of v3own's lifetime and are DELETED, not
  // wrapped."
  //
  // The oracle's own credit counter is the clearest case. It counts admitted
  // minus emitted against OWNER_DEPTH, with a deliberate one-cycle bubble at
  // full-to-free to avoid a cyclic ready path. That is exactly v3own's live
  // window (§6.1's `live(t) = unsigned_14(t - retire_ticket) < used`), computed
  // a second time from different signals. Two counters that must agree are two
  // counters that can disagree, and the whole point of a single identity
  // namespace is that there is one.
  //
  // So `credit_available` becomes v3own's `adm_ready_o`, and the ceiling is
  // v3own's OWNERS=64 rather than a local OWNER_DEPTH. `live_r` and
  // `live_peak_r` survive ONLY as the continuity counters the composed test
  // asserts (`cnt_live_peak_o == 64`); they are now OBSERVERS of v3own's
  // admission rather than the authority for it.
  localparam int unsigned OWNER_DEPTH = FCTXN;

  wire credit_available = own_adm_ready;

  wire admit_c = frag_valid_i && frag_ready_o;
  wire emit_c  = out_valid_o && out_ready_i;

  // THE ADMISSION BEAT ITSELF. `adm_req_i` is v3own's required-source mask:
  // one bit per sample lane plus the AUX bit, which is what makes a zero-sample
  // fragment a legal zero-work owner (§9.1) rather than a fragment that never
  // completes. It is built from the SAME sample count the expander uses, so the
  // two cannot drift: fragrob's table, one place.
  // ADMISSION MUST FIRE EXACTLY WHEN THE FRAGMENT ENTERS, NOT WHENEVER ONE IS
  // OFFERED. `frag_ready_o` is `rcp_v_ready && credit_available`, so a fragment
  // is taken only when the RCP has room AND v3own has an owner. Asserting
  // `adm_valid_i` on `frag_valid_i` alone allocates an owner every cycle the
  // caller offers one -- including cycles the RCP refuses -- and those owners
  // are never issued, never complete and never retire. The ring fills with
  // fragments that do not exist and admission stops.
  //
  // That is what gate 2 was showing: 27 admitted, 3 retired, then nothing. Two
  // admission decisions that must agree, made from different signals -- the
  // same defect class as the duplicate credit counter `live_r` was, and the
  // reason §0 says one identity namespace rather than two.
  assign own_adm_valid_c = frag_valid_i && rcp_v_ready;
  assign own_adm_ctx_c   = frag_ctx_i;
  always_comb begin
    unique case (frag_sample_count_i)
      2'd0:    own_adm_req_c = 4'b0000;
      2'd1:    own_adm_req_c = 4'b0001;
      2'd2:    own_adm_req_c = 4'b0011;
      default: own_adm_req_c = 4'b0111;
    endcase
    if (frag_aux_i) own_adm_req_c[3] = 1'b1;
  end

  logic [FCTXW:0] live_r;                      // OBSERVER, not the authority
  logic [FCTXW:0] live_peak_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) live_r <= '0;
    // One combined case per clock. Written as a single update because admit and
    // emit can land together and two separate statements would race.
    else if (admit_c && !emit_c) live_r <= live_r + 1'b1;
    else if (emit_c && !admit_c) live_r <= live_r - 1'b1;
  end

  // OBSERVABILITY: the high-water mark, so a run that never approached the
  // ceiling cannot be read as evidence that the ceiling works.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) live_peak_r <= '0;
    else if (live_r > live_peak_r) live_peak_r <= live_r;
  end
  assign cnt_live_peak_o = {{(32-FCTXW-1){1'b0}}, live_peak_r};

  // TOKW 8 -> 14: THE RCP CARRIES THE OWNER HANDLE, NOT AN INGRESS TOKEN.
  // Third width question of this restructure and the third different answer.
  // `zhao_raster_rcp24_svc` is parameterised throughout (`TOKW` at :59, every
  // use `[TOKW-1:0]`, no literals), so like `aux_pipe` and unlike `cache_pipe`
  // the leaf needs NO change -- only the number here. Measured, not assumed.
  zhao_raster_rcp24_svc #(.NCTX(8), .TOKW(14)) u_rcp (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(frag_valid_i && credit_available), .v_ready_o(rcp_v_ready),
      // The token IS v3own's handle now. `tok_r`, the island's own ingress
      // counter, is one of the "partial implementations of v3own's lifetime"
      // §0 names -- it stamped an identity the owner already assigns.
      .d_i(frag_depth_i), .v_tok_i(own_adm_owner),
      .r_valid_o(rcp_r_valid), .r_ready_i(rcp_r_ready),
      .r_o(rcp_r), .k_o(rcp_k), .d_zero_o(rcp_dzero), .r_tok_o(rcp_tok),
      .accepted_o(rcp_accepted), .completed_o(cnt_rcp_completed_o),
      .mul_busy_o(rcp_mul_busy), .occupancy_o(rcp_occ));

  assign frag_ready_o = rcp_v_ready && credit_available;

  always_ff @(posedge clk or negedge rst_n) begin
  end

  // ==========================================================================
  // PER-FRAGMENT ATTRIBUTE CARRIAGE
  // ==========================================================================
  // EVERY per-fragment attribute below used to be tapped straight off this
  // module's input pins at the point it was CONSUMED. That is wrong, and it was
  // wrong for ten separate signals. A fragment spends ~12 clocks in RCP24 and
  // PERSPUV before FRAGROB accepts it, so by the time the tap was read the
  // boundary was presenting a DIFFERENT fragment -- and when the boundary
  // stalled or went idle it presented the same one repeatedly.
  //
  // Measured, in `island_composed_directed`: of 64 fragments submitted, the
  // first one to reach FRAGROB arrived carrying fragment 12's tag, only 25
  // distinct fragments were ever seen, 39 were lost outright, and the last
  // fragment's attributes were delivered 24 times because the input pins simply
  // held their final value. FRAGROB's own slot allocation and ordered retire
  // were PERFECT throughout -- the head slot walked 0..15 exactly four times.
  // The reorder buffer was innocent; it was handed the wrong data.
  //
  // The token needed to fix it already existed and was already carried end to
  // end: `tok_r` is stamped on admission, RCP24 returns it as `r_tok_o`, and
  // PERSPUV carries it through as `tag_o`. Nothing consulted it. So the fix is
  // to store each fragment's attributes at admission and read them back at the
  // two points where its token reappears.
  //
  // Depth 64 against at most RCP24's 8 contexts plus PERSPUV's 16 tokens in
  // flight, so a token cannot wrap onto a live entry. The reads are
  // combinational (MLAB/LUT-RAM, not M10K) because PERSPUV's input handshake is
  // combinational off RCP24's output, and inserting a cycle here would need a
  // skid buffer for no gain.
  localparam int unsigned FCTXN = 64;
  localparam int unsigned FCTXW = 6;
  // The combiner tag carries the caller's 16-bit tag AND the submission
  // sequence, so the island can restore order after a block that retires out
  // of order by design.
  // ENFORCED-BY: tests/texture/island_composed_directed.cpp
  localparam int unsigned ROBTAGW = 16 + FCTXW;

  logic [63:0]           uvw_m   [FCTXN];   // {u_over_w, v_over_w}
  logic [CTXW-1:0]       fctx_m  [FCTXN];   // tag + token + material fields
  logic [31:0]           fbase_m [FCTXN];   // {base_rgb, base_a}
  // ONE NAMED ARRAY PER FIELD, and deliberately not one packed word.
  //
  // The packed form costs nothing to write and one whole defect to find: the
  // palette lookup in this file took its slot and generation from two
  // hand-written slices of the same token that OVERLAPPED, so the "slot" was
  // the low bits of the "generation" and every CLUT fragment came out black
  // while the lookup counter looked healthy. Named fields cannot overlap.
  // ==========================================================================
  // DEFECT (b), THE BOUNDARY HALF: VALIDATE frag_class_i AT THE ISLAND PIN
  // ==========================================================================
  // THE DEFECT. `frag_class_i` is a 2-bit island input and NOTHING validated
  // it. Its value was stored verbatim, travelled to `class_m`, was packed into
  // the top two bits of the planner's source id, echoed by CACHE_PIPE and used
  // by RSP_DISPATCH to route. A caller supplying 3 -- a legal bit pattern on an
  // unconstrained pin -- reached the dispatcher's `default:` arm, which popped
  // the response and destroyed it.
  //
  // THE EVIDENCE. The composed test drives only 0 and 2, which is why this has
  // never fired. That is not safety; that is an untested trapdoor. Section 5A.8
  // of the brief requires acceptance to drive ALL FOUR values including 3.
  //
  // THE CONSEQUENCE WITHOUT THE FIX. FRAGROB's `head_done_c` never reaches
  // `arr_q == req_q` for that fragment; retirement is allocation ordered; the
  // island stops. One bad pin value from a caller kills the frame.
  //
  // BOTH HALVES ARE IMPLEMENTED, DELIBERATELY. The dispatcher now completes an
  // unknown class with an error instead of destroying it (defence in depth,
  // against corruption between here and there), and the class cannot enter as
  // 3 in the first place (this). Either alone leaves the other's failure mode
  // open: without the dispatcher fix a corrupted id still hangs the island;
  // without this fix every class-3 fragment burns an error completion and a
  // magenta pixel for a fault that could have been caught at the pin.
  //
  // SANITISED TO CLS_NEAR, NOT DROPPED, because the nearest lane is the one
  // that already terminates in a counted refusal (defect (a) below). The
  // fragment therefore gets a defined error colour and RETIRES, which is the
  // section 5A.7 invariant: an error must be as retirable as a success. Since
  // the refusal is counted separately from `err_class_invalid_o`, the two are
  // still distinguishable in the evidence.
  //
  // WHERE THE SANITISATION LIVES, AND WHY NOT HERE. The obvious shape is a
  // combinational `frag_class_san_c` beside the pin. That is a LIVE-INGRESS
  // ALIAS, and `tools/rtl/check_ingress_capture.py` exists to forbid exactly
  // that pattern: a wire built from an ingress pin is a late read laundered
  // through one extra wire, which is the historical `fr_f_ctx_in` defect the
  // gate was written for. It went red on the first draft of this repair, which
  // is the gate doing its job.
  //
  // So the pin is STORED VERBATIM at the capture event, exactly as before, and
  // both the sanitisation and its counter happen at the READ point, on
  // captured data -- see `f_class_c` / `f_class_bad_c` below. That also keeps
  // the raw value recoverable for diagnosis instead of destroying it at the
  // door.

  localparam int unsigned PSW = $clog2(PAL_SLOTS);
  // THREE ARRAYS AND THEIR ALIASES LIVED HERE, and they were dead DUPLICATES
  // rather than merely unread: `fpsl_m`, `fpgn_m` and `frec_m` held the palette
  // pair and the recipe keyed by FCTXN slot, while the LIVE copies of the same
  // state sit in `palslot_m`, `palgen_m` and `mat_m` keyed by OWNER slot -- and
  // only the owner-keyed ones are read. Their `_c` aliases had no consumers at
  // all, so alias and array went together.
  //
  // They are an abandoned earlier attempt at CARRIAGE, left in place when the
  // sidecar won. §0 calls that a partial implementation of v3own's lifetime, and
  // Packet 3 is the same idea done properly -- the pair travelling WITH the
  // request, so no lookup can be keyed on an identity that has been recycled.
  //
  // Deleted in Packet 1 rather than Packet 2 deliberately: removing dead state
  // and adding the descriptor bank in one packet would make FIT GATE 2's delta
  // uninterpretable, because a shrink could be either.
  logic [BINDW-1:0] fbind_m [FCTXN];
  logic [LODW-1:0]  flod_m  [FCTXN];
  logic [1:0]       fcls_m  [FCTXN];
  logic             faux_m  [FCTXN];
  logic [1:0]       fsc_m   [FCTXN];
  logic [7:0]       fwt_m   [FCTXN];
  // THE SUBMISSION SEQUENCE NUMBER (audit R6). Stamped at admission and
  // carried out through the combiner so the island can restore the caller's
  // order at its own boundary.
  //
  // It is a counter and not the token, because the token comes from a POOL:
  // a slot is reused as soon as its fragment retires, so token order is
  // allocation order only while nothing has retired yet. A free-running count
  // is submission order by construction.
  //
  // FCTXW bits is exact, not a guess. FRAGROB admits at most FCTXN fragments,
  // so two in flight never differ by FCTXN or more, and the low FCTXW bits
  // ENFORCED-BY: tests/texture/island_composed_directed.cpp
  // therefore distinguish every live fragment. The counter is allowed to wrap.
  // `fseq_m` DELETED: v3own's cursor IS the sequence. A second one is the
  // "partial implementation of v3own's lifetime" §0 forbids keeping.


  // THE ATTRIBUTE TABLES ARE KEYED BY THE OWNER SLOT, like everything else.
  // They were keyed by `tok_r`, the deleted ingress counter. The slot is the
  // handle's HIGH six bits, and taking the low ones would key on the
  // GENERATION -- the same silent aliasing `uvw_m`'s index nearly had.
  wire [FCTXW-1:0] fc_wp = own_adm_owner[13:8];

  // THE CALLER'S CONTEXT IS STORED VERBATIM AND NEVER REPACKED.
  //
  // An earlier version of this capture wrote the recipe, weight, sample count
  // and token into bits [34:16] of the caller's own context word. That is not
  // free space -- the caller interprets that word as its own data, and the
  // owner's recovery architecture v2 (2.3) rules it out directly: "Packing
  // recipe bits into that word is not a valid way to retain an independently
  // opaque context and world X/Z ... Do not silently overwrite caller-owned
  // bits."
  //
  // The island's own fields live in the named arrays above, and the token
  // travels through FRAGROB as a TYPED FIELD beside the context rather than
  // inside it, so nothing the caller owns is touched.
  // HONEST LIMIT: this island consumes only the low 16 bits of the context,
  // as the fragment tag behind out_tag_o. The remaining bits are stored and
  // carried through FRAGROB intact, but nothing downstream reads them and no
  // port exposes them, so NO TEST CAN OBSERVE that they survive. What is
  // established is the weaker and still necessary property that the island no
  // longer OVERWRITES them. Surfacing them would cost 64 output pins on a
  // block whose timing is already boundary-sensitive, so it waits for a
  // consumer that actually needs them.
  logic [CTXW-1:0] fr_f_ctx_in;
  assign fr_f_ctx_in = frag_ctx_i;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
    end else if (frag_valid_i && frag_ready_o) begin
    end
  end

  always_ff @(posedge clk) begin
    if (frag_valid_i && frag_ready_o) begin
      uvw_m[fc_wp]   <= {frag_u_over_w_i, frag_v_over_w_i};
      fctx_m[fc_wp]  <= fr_f_ctx_in;
      fbase_m[fc_wp] <= {frag_base_rgb_i, frag_base_a_i};
      fbind_m[fc_wp] <= frag_binding_i;
      flod_m [fc_wp] <= frag_lod_i;
      fcls_m [fc_wp] <= frag_class_i;
      faux_m [fc_wp] <= frag_aux_i;
      fsc_m  [fc_wp] <= frag_sample_count_i;
      fwt_m  [fc_wp] <= frag_weight_i;
    end
  end

  // ==========================================================================
  // PACKET 2 / D2: THE TYPED EARLY DESCRIPTOR BANK
  // ==========================================================================
  // Written ONCE, on the owner's admission handshake, from the input pins of
  // that same beat. Read back synchronously when PERSPUV presents that owner's
  // result, through `zhao_texture_uv_join` below.
  //
  // §5.2 is specific about the write event: "Write the row once on the EXACT
  // owner admission handshake, using that owner's slot. Do not use a local
  // rolling pointer that can drift from owner admission." `own_adm_accept` IS
  // that handshake -- v3own's `adm_accept_o` -- and the slot comes from the owner
  // it allocated on this beat, not from `fc_wp`.
  //
  // The `fc_wp`-indexed arrays above are the thing this replaces. They are
  // written on `frag_valid_i && frag_ready_o`, which is algebraically the SAME
  // beat (both reduce to `frag_valid_i && rcp_v_ready && own_adm_ready`, proven
  // in the roadmap) -- so the two write events agree today. The difference is
  // that the descriptor is keyed by the identity the response comes back under,
  // and the FCTXN arrays are keyed by a pointer that has to be carried
  // separately and read at the right moment by whoever needs it.
  //
  // THE MOSAIC BYTE SLICE, verified rather than assumed: `fbase_m` stores
  // `{frag_base_rgb_i, frag_base_a_i}` and Mosaic reads `fbase_rd[31:24]` and
  // `[23:16]`, which are `frag_base_rgb_i[23:16]` and `[15:8]`. Those exact two
  // bytes are what the descriptor carries. A one-byte shift here is the
  // five-stale-slices defect reborn.
  logic [63:0] ed_ctx_c;
  logic [7:0]  ed_lod_c, ed_mosa_c, ed_mosb_c, ed_mosw_c, ed_bsel_c;
  logic [1:0]  ed_class_c, ed_count_c, ed_pslot_c;
  logic        ed_aux_c, ed_rvalid_c;
  logic [GENW-1:0] ed_pgen_c, ed_ogen_c;
  logic        ed_rd_valid_c;
  logic [5:0]  ed_rd_slot_c;
  logic [GENW-1:0] ed_rd_gen_c;

  zhao_texture_early_desc #(
      .SLOTW(6), .GENW(GENW), .SLICEW(40)
  ) u_early_desc (
      .clk(clk), .rst_n(rst_n),
      .wr_valid_i        (own_adm_accept),
      .wr_slot_i         (own_adm_owner[13:8]),
      .wr_owner_gen_i    (own_adm_owner[7:0]),
      .wr_aux_context_i  (frag_ctx_i),
      .wr_lod_q4_4_i     (frag_lod_i),
      .wr_raw_class_i    (frag_class_i),
      .wr_needs_aux_i    (frag_aux_i),
      .wr_sample_count_i (frag_sample_count_i),
      .wr_palette_slot_i (frag_pal_slot_i),
      .wr_palette_gen_i  (frag_pal_gen_i),
      .wr_mosaic_mat_a_i (frag_base_rgb_i[23:16]),
      .wr_mosaic_mat_b_i (frag_base_rgb_i[15:8]),
      .wr_mosaic_weight_i(frag_weight_i),
      .wr_binding_sel_i  (frag_binding_i),

      .rd_valid_i        (ed_rd_valid_c),
      .rd_slot_i         (ed_rd_slot_c),
      .rd_owner_gen_i    (ed_rd_gen_c),

      .rd_result_valid_o (ed_rvalid_c),
      .rd_owner_gen_o    (ed_ogen_c),
      .rd_aux_context_o  (ed_ctx_c),
      .rd_lod_q4_4_o     (ed_lod_c),
      .rd_raw_class_o    (ed_class_c),
      .rd_needs_aux_o    (ed_aux_c),
      .rd_sample_count_o (ed_count_c),
      .rd_palette_slot_o (ed_pslot_c),
      .rd_palette_gen_o  (ed_pgen_c),
      .rd_mosaic_mat_a_o (ed_mosa_c),
      .rd_mosaic_mat_b_o (ed_mosb_c),
      .rd_mosaic_weight_o(ed_mosw_c),
      .rd_binding_sel_o  (ed_bsel_c),

      // Instruments deliberately unconnected in this packet. Wiring them would
      // add island output PORTS, and the composed test shares one file between
      // this top and the oracle on the strength of their port lists matching.
      // That is a separate, guarded step; it is not worth coupling to the rewire.
      .writes_o          (),
      .reads_o           (),
      .rd_gen_mismatch_o ()
  );

  // Read point 1: RCP24's answer, for PERSPUV's numerators.
  //
  // ---------------------------------------------------------------------------
  // P0-C STAGE A: THIS READ IS REGISTERED, AND THAT IS WHY uvw_m IS A RAM
  // ---------------------------------------------------------------------------
  // It used to be `wire [63:0] uvw_rd = uvw_m[rcp_tok[FCTXW-1:0]];` -- a
  // dynamically indexed COMBINATIONAL read out of a 64x64 array. Quartus said so
  // in as many words:
  //
  //     Info (276007): RAM logic "uvw_m" is uninferred due to ASYNCHRONOUS READ
  //     LOGIC
  //
  // and the array therefore cost **4,096 flip-flops** in fabric -- the single
  // largest uninferred structure at this level and a majority of the top's
  // ~6,981 registers above its member blocks (P0-E-THE-GLUE-POOL-MEASURED).
  //
  // Registering the read is what lets it become an M10K, and it is the same
  // defect §16.2 named in the DONE queue: "a dynamically indexed combinational
  // output from a flop array".
  //
  // THE COST IS A CYCLE, SO IT NEEDS A SKID. A registered read returns its data
  // one clock after the address, so the RCP beat cannot be passed straight
  // through to PERSPUV any more. This one-deep stage holds the whole beat --
  // the RAM data and every scalar that travelled beside it -- and presents it
  // together. `zhao_raster_perspuv_svc`'s `v_ready_o` is `(free_cnt_q != '0)`,
  // a pure register read, so `rcp_r_ready` below cannot form a combinational
  // loop through it.
  logic        px_v_q;
  logic [63:0] px_uvw_q;
  logic [23:0] px_r_q;
  logic [5:0]  px_k_q;
  logic        px_dz_q;
  logic [13:0] px_tok_q;

  logic px_in_ready_c;   // PERSPUV's own ready, one name for the two uses below

  assign rcp_r_ready = !px_v_q || px_in_ready_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      px_v_q <= 1'b0;
    end else begin
      if (px_v_q && px_in_ready_c) px_v_q <= 1'b0;
      if (rcp_r_valid && rcp_r_ready) begin
        px_v_q <= 1'b1;
        // THE REGISTERED READ. Written inside the clocked block against a
        // dynamic index, which is the form Quartus infers as RAM.
        // INDEXED BY THE OWNER SLOT, not by an ingress token: the handle's
        // high six bits ARE the slot, and the slot is what v3own allocates
        // one-per-live-fragment. `rcp_tok[FCTXW-1:0]` would take the low bits
        // of the handle, which are the GENERATION -- a silent aliasing of
        // every fragment sharing a generation onto one entry.
        px_uvw_q <= uvw_m[rcp_tok[13:8]];
        px_r_q   <= rcp_r;
        px_k_q   <= rcp_k;
        px_dz_q  <= rcp_dzero;
        px_tok_q <= rcp_tok;
      end
    end
  end

  logic        pu_valid, pu_ready;
  logic [31:0] pu_u, pu_v;
  logic [15:0] pu_tag;
  logic        pu_sat, pu_dzero;
  logic [31:0] pu_products;
  // [4:0], not [3:0]: the pair-pipe's occupancy_o is 5 bits where svc's was 4.
  // `owned_q` counts everything accepted and not yet emitted -- the pipeline
  // stages, the terminal FIFO and the item in the output -- so it can exceed
  // NTOK=16 and needs the extra bit. Found 2026-09-09 by verilate refusing the
  // swap: WIDTHEXPAND, "expects 5 bits ... generates 4 bits".
  //
  // WHICH CORRECTS A CLAIM I MADE: the pair-pipe's ports are a superset by
  // NAME, not by WIDTH, so the swap is not the pure module-name change I
  // reported. My comparison script extracted port names and discarded widths.
  // Nothing reads pu_occ in either island top -- it is declared, driven and
  // never consumed -- so widening it is safe.
  logic [4:0]  pu_occ;
  // The pair-pipe's one NEW port. Verilator refuses an unconnected output
  // here (PINMISSING as an error), so it must be connected even to be
  // simulated -- which is the third way the swap is not the "module-name
  // change" I first reported. It counts depth-zero fragments that produce
  // no product, and perspuv_pairpipe_directed asserts it against an
  // independently counted zero_accepts, so it is a TESTED instrument.
  // Landed on a local here; production should carry it to a debug counter
  // port rather than drop it on the floor.
  logic [31:0] pu_zero_products;

  zhao_raster_perspuv_pairpipe #(.NTOK(16), .TAGW(16)) u_persp (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(px_v_q), .v_ready_o(px_in_ready_c),
      .u_over_w_i(px_uvw_q[63:32]), .v_over_w_i(px_uvw_q[31:0]),
      .r_mant_i(px_r_q), .r_k_i(px_k_q), .depth_zero_i(px_dz_q),
      .tag_i({2'd0, px_tok_q}),   // 16-bit tag, 14-bit handle: it fits
      .r_valid_o(pu_valid), .r_ready_i(pu_ready),
      .u_o(pu_u), .v_o(pu_v), .tag_o(pu_tag), .sat_o(pu_sat),
      .depth_zero_o(pu_dzero),
      .fragments_o(cnt_persp_fragments_o), .products_o(pu_products),
      .occupancy_o(pu_occ), .zero_products_o(pu_zero_products));

  // Read point 2: PERSPUV's answer, for everything that consumes a fragment
  // once its texture coordinates exist.
  // ...and read by the same slot, arriving in PERSPUV's tag. `pu_tag[FCTXW-1:0]`
  // would again be the generation; `[13:8]` is the slot.
  wire [FCTXW-1:0]      fc_rp    = pu_tag[13:8];
  wire [CTXW-1:0]       fctx_rd  = fctx_m[fc_rp];
  wire [31:0]           fbase_rd = fbase_m[fc_rp];
  wire [BINDW-1:0] f_binding_c  = fbind_m[fc_rp];
  wire [LODW-1:0]  f_lod_c      = flod_m [fc_rp];
  // DEFECT (b), THE BOUNDARY HALF, APPLIED HERE ON CAPTURED DATA.
  // `fcls_m` holds the pin verbatim; this is where an out-of-range class stops
  // being able to travel. `f_class_bad_c` is the raw verdict, kept so the
  // counter below can see it after the value has been replaced.
  wire [1:0]       f_class_raw_c = fcls_m [fc_rp];
  wire             f_class_bad_c = (f_class_raw_c == CLS_ERR);
  wire [1:0]       f_class_c    = f_class_bad_c ? CLS_NEAR : f_class_raw_c;
  wire             f_aux_c      = faux_m [fc_rp];
  wire [1:0]       f_scount_c   = fsc_m  [fc_rp];
  wire [7:0]       f_weight_c   = fwt_m  [fc_rp];

  // ==========================================================================
  // MOSAIC, on the pre-TMU u/v path
  // ==========================================================================
  // It observes the same u/v the TMU planner will use and picks a tile. It is
  // fed rather than bypassed so its logic is real in this measurement; its
  // pick is consumed by the counter only, because the tile selection's
  // consumer (the plan's base address) is a binding-table concern that this
  // composition does not yet own.
  logic mos_req_ready, mos_pick_valid;
  logic [7:0] mos_tile;
  logic [5:0] mos_tx, mos_ty;
  logic [15:0] mos_src;
  logic mos_idle;

  zhao_texture_mosaic u_mosaic (
      .clk(clk), .rst_n(rst_n),
      // PACKET 2: Mosaic now takes the JOIN's branch, and its ready is part of
      // the fork's handshake instead of being ignored. The old form was
      // `req_valid_i(pu_valid && pu_ready)` with `mos_req_ready` going nowhere --
      // exactly what §5.3 warns about: "a future functional Mosaic branch cannot
      // drop requests merely because its ready was ignored".
      //
      // u/v/src come from the join's F-branch outputs and that is CORRECT rather
      // than cross-branch borrowing: `f_u_o`, `f_v_o` and `f_owner_o` are the same
      // held record (`r_u_q`/`r_v_val_q`/`r_tag_q`) the m_* fields come from -- one
      // enable loaded them all. `{2'd0, f_owner_o}` reproduces the old
      // `req_src_id_i(pu_tag)` bit for bit, because pu_tag is driven
      // `{2'd0, px_tok_q}`.
      .req_valid_i(jn_m_valid_c), .req_ready_o(mos_req_ready),
      .req_u_i(jn_u_c), .req_v_i(jn_v_c),
      .req_mat_a_i(jn_m_mata_c), .req_mat_b_i(jn_m_matb_c),
      .req_weight_i(jn_m_wt_c), .req_mosaic_i(1'b1),
      .req_src_id_i({2'd0, jn_owner_c}),
      .pick_valid_o(mos_pick_valid), .pick_ready_i(1'b1),
      .pick_tile_o(mos_tile), .pick_tx_o(mos_tx), .pick_ty_o(mos_ty),
      .pick_src_id_o(mos_src), .idle_o(mos_idle),
      .texture_samples_o(cnt_mosaic_samples_o));

  // ==========================================================================
  // FRAGROB — the hub
  // ==========================================================================
  logic        fr_f_ready;
  logic        fr_tmu_valid, fr_tmu_ready;
  logic [31:0] fr_tmu_u, fr_tmu_v;
  logic [BINDW-1:0] fr_tmu_binding;
  logic [LODW-1:0]  fr_tmu_lod;
  logic [$clog2(DEPTH)-1:0] fr_tmu_slot;
  logic [1:0]  fr_tmu_sidx;
  logic [GENW-1:0] fr_tmu_gen;
  logic        fr_tmu_rvalid, fr_tmu_rready;
  logic [23:0] fr_tmu_rgb;
  logic [7:0]  fr_tmu_a;
  logic [$clog2(DEPTH)-1:0] fr_tmu_rslot;
  logic [1:0]  fr_tmu_rsidx;
  logic [GENW-1:0] fr_tmu_rgen;

  logic        fr_aux_valid, fr_aux_ready;
  logic [CTXW-1:0] fr_aux_ctx;
  logic [$clog2(DEPTH)-1:0] fr_aux_slot;
  logic [GENW-1:0] fr_aux_gen;
  logic        fr_aux_rvalid, fr_aux_rready;
  logic [23:0] fr_aux_rgb;
  logic [7:0]  fr_aux_a;
  logic [$clog2(DEPTH)-1:0] fr_aux_rslot;
  logic [GENW-1:0] fr_aux_rgen;

  logic        fr_o_valid, fr_o_ready;
  logic [$clog2(DEPTH)-1:0] fr_alloc_slot;

  logic [CTXW-1:0] fr_o_ctx;
  logic [FCTXW-1:0] fr_o_tok;
  logic [23:0] fr_o_rgb, fr_o_aux_rgb;
  logic [7:0]  fr_o_a, fr_o_aux_a;
  logic        fr_o_has_aux, fr_o_uv_sat;
  logic [23:0] fr_o_s_rgb [3];
  logic [7:0]  fr_o_s_a   [3];
  logic [31:0] fr_samples, fr_full_clocks;
  // `fr_wq_overflow`, `fr_id_error`, `fr_combiner_unfrozen` and
  // `fr_alloc_valid` are GONE with the FRAGROB instance that drove them. They
  // survived its deletion as declarations feeding live logic, which is how
  // three fault ports became constant zero without a single tool complaining.


  // DELETED: the oracle drove `pu_ready` from fragrob's `f_ready_o`. The
  // expander is that consumer now and drives it through its own `f_ready_o`
  // port, so this leftover made PERSPUV's ready MULTIDRIVEN -- caught by
  // elaboration, which is what elaboration is for.

  // FRAGROB takes u/v/binding/lod PER SAMPLE -- three of each. The island's
  // boundary supplies one u/v pair (the fragment's) and one binding/lod; the
  // per-sample variation is a binding-table concern this composition does not
  // own yet, so all three samples are given the same coordinates and are
  // distinguished by their binding index. That is a real limitation of the
  // composition, not of FRAGROB, and it is why the binding is offset per
  // sample rather than replicated: three identical bindings would make the
  // cache serve one line three times and understate the miss traffic.
  // THE RECIPE TRAVELS WITH THE FRAGMENT, in FRAGROB's context word.
  //
  // The first version wired the combiner's recipe/weight/sample_count straight
  // from the island's INPUT ports. With a reorder buffer in between, a fragment
  // retiring after N others was combined with whatever recipe happened to be
  // arriving at that moment -- and the composed test still passed every
  // handshake check, because a wrong recipe is a wrong picture, not a stall.
  //
  // ENFORCED-BY: fpga/rtl/texture/zhao_texture_fragrob.sv -- its `ctx_m` array
  // is written at allocation and read at `head_slot_c` on retirement, so the
  // context word leaves FRAGROB with the fragment it was allocated for. This
  // top relies on that and does not re-derive it; the packing below and the
  // unpacking at the combiner instance are the two halves of one layout and
  // are deliberately adjacent in this file so they cannot drift apart.
  //
  // ENFORCED-BY: tests/texture/island_composed_directed.cpp -- it cycles all
  // eight recipes and asserts the per-recipe job counts `cnt_combine_jobs_o`
  // reports. If the recipe did not travel with its fragment, every fragment
  // would combine with whichever recipe arrived last and exactly one counter
  // would move. This was an open gap until the counters were brought out to
  // the boundary in the same pass that re-fits the island.
  //
  // CTXW is 64 and the low 16 bits are the tag, so the material fields ride
  // above it.
  // The context that reaches FRAGROB is the caller's own, stored verbatim at
  // admission and recovered by this fragment's token.
  logic [CTXW-1:0] fr_f_ctx;
  assign fr_f_ctx = fctx_rd;

  logic signed [31:0] fr_f_u [3];
  logic signed [31:0] fr_f_v [3];
  logic [BINDW-1:0]   fr_f_binding [3];
  logic [LODW-1:0]    fr_f_lod [3];
  always_comb begin
    for (int s = 0; s < 3; s++) begin
      fr_f_u[s]       = pu_u;
      fr_f_v[s]       = pu_v;
      fr_f_binding[s] = f_binding_c + BINDW'(s);
      fr_f_lod[s]     = f_lod_c;
    end
  end

  // ---------------------------------------------------------------------------
  // FRAGROB IS DELETED HERE. This is the whole point of Stage C.
  // ---------------------------------------------------------------------------
  // The oracle instantiates `zhao_texture_fragrob` at this position (its
  // island_top.sv:941) with DEPTH/CTXW/TOKW_F/BINDW/LODW/GENW, and that one
  // instance carries SIX distinct jobs:
  //
  //   1. fragment admission and slot allocation      -> zhao_texture_v3own
  //   2. fragment-to-sample request expansion        -> zhao_texture_frag_expand
  //   3. per-source issued/claimed/committed masks   -> v3own (its bitplanes)
  //   4. result banks for the returned samples       -> v3own (zhao_texture_v3bank)
  //   5. output reorder and retirement ordering      -> v3own (its cursors)
  //   6. owner context storage                       -> v3own (OWNER_CONTEXT)
  //
  // Only (2) needs a new home; the other five are what v3own already IS. That
  // asymmetry is why the deletion ledger in the architecture expects the swap
  // to ADD area on its own (v3own 3,348 ALM against fragrob 1,676) and to pay
  // for itself only through the top-level ordering and result state it makes
  // deletable -- a claim recorded as UNMEASURED there and still unmeasured here.
  //
  // TO DO, in this order, so each is separately reviewable:
  //   a. instantiate zhao_texture_frag_expand  (job 2)
  //   b. instantiate zhao_texture_v3own        (jobs 1,3,4,5,6)
  //   c. re-key the identity namespace to the 14-bit owner handle
  //   d. delete the ROB pool, fseq_m, live_r/tok_r and the named side tables
  //
  // Until (b) lands this file does not elaborate, which is why it is
  // referenced by no fit target and no test.

  // ==========================================================================
  // (a) THE FRAGMENT EXPANDER -- fragrob's job 2, and only job 2
  // ==========================================================================
  // Contract: reports/P0C-STAGEB-EXPANDER-CONTRACT-20260908.md, traced out of
  // fragrob rather than remembered. Leaf-verified 2026-09-08: 10 checks, 96
  // requests matching the model element for element, and fire-tested against
  // the architecture's own falsifier (an issue pulse on INTENT rather than on
  // the accepted handshake trips `a_iss_only_on_fire`).
  //
  // ITS INPUT IS PERSPUV'S OUTPUT, NOT A NEW ADMISSION PORT. The oracle runs
  // `pu_valid`/`pu_u`/`pu_v` into fragrob's sample walk; here the same beat
  // enters the expander, which is why `u`/`v` arrive WITH the fragment and the
  // expander needs no descriptor RAM of its own (contract §4).
  //
  // `exp_owner_c` IS A PLACEHOLDER UNTIL STEP (c). Under the re-key it is
  // v3own's 14-bit {slot,generation} handle, carried through RCP and PERSPUV in
  // the existing 16-bit tag -- which has room, 16 >= 14, so no widening is
  // needed on that path. It is written as an explicit unresolved signal rather
  // than quietly tied to `pu_tag[13:0]`, because a placeholder that looks like
  // a real connection is how a wrong number acquires a provenance line.
  // RESOLVED: the handle admitted by v3own, carried through RCP's token and
  // PERSPUV's tag, arrives back here. Three blocks, one identity, no lookup.
  wire [13:0] exp_owner_c = pu_tag[13:0];

  logic        exp_req_valid, exp_req_ready;
  logic signed [31:0] exp_req_u, exp_req_v;
  logic [7:0]  exp_req_lod;
  logic [17:0] exp_req_src_id;
  logic [1:0]  exp_req_pal_slot;
  logic [7:0]  exp_req_pal_gen;
  logic        exp_aux_valid, exp_aux_ready;
  logic [13:0] exp_aux_owner;
  logic [CTXW-1:0] exp_aux_ctx;
  logic        exp_iss_tmu_valid, exp_iss_aux_valid;
  logic [15:0] exp_iss_tmu_handle;
  logic [13:0] exp_iss_aux_owner;
  logic [31:0] exp_fragments, exp_requests, exp_zero_frags, exp_aux_requests;
  logic [31:0] exp_wq_overflow;

  // ==========================================================================
  // PACKET 2 / D2: THE POST-PERSPUV JOIN
  // ==========================================================================
  // §5.3: one bounded, stall-safe join between PERSPUV and the expander,
  // capturing {owner identity, computed U/V, flags, synchronous early
  // descriptor} and advancing all of it together.
  //
  // The alignment is STRUCTURAL, not a timing argument. The join drives the
  // bank's `rd_valid_i` with the same enable that registers its own PERSPUV
  // fields, and the bank's output register is gated on an actual read (the D0
  // hold law). So the descriptor and the U/V it travels with move on the same
  // clocks and cannot come from different transactions.
  //
  // This is what replaces the `fc_rp`-indexed reads below. Those took each
  // attribute out of a separate array at whatever moment the consumer happened
  // to look; the join hands over one record.
  logic               jn_f_valid_c, jn_f_ready_c;
  logic [13:0]        jn_owner_c;
  logic signed [31:0] jn_u_c, jn_v_c;
  logic [7:0]         jn_binding_c, jn_lod_c;
  logic [1:0]         jn_count_c, jn_class_c;
  logic               jn_aux_c, jn_sat_c, jn_dz_c;
  logic [CTXW-1:0]    jn_ctx_c;
  logic [1:0]         jn_pal_slot_c;
  logic [GENW-1:0]    jn_pal_gen_c;
  logic               jn_m_valid_c;
  logic [7:0]         jn_m_mata_c, jn_m_matb_c, jn_m_wt_c;

  zhao_texture_uv_join #(
      .TAGW(14), .SLOTW(6), .GENW(GENW), .CTXW(CTXW),
      // Behaviour-preserving default: the descriptor's own sample count is used
      // and the depth-zero flag is reported rather than acted on. Making a
      // depth-zero fragment issue no samples is a POLICY and belongs to the
      // island, not to a default.
      .DZ_FORCES_ZERO_SAMPLES(1'b0)
  ) u_uv_join (
      .clk(clk), .rst_n(rst_n),
      .p_valid_i(pu_valid), .p_ready_o(pu_ready),
      .p_u_i(pu_u), .p_v_i(pu_v), .p_tag_i(pu_tag[13:0]),
      .p_sat_i(pu_sat), .p_dz_i(pu_dzero),

      .d_rd_valid_o(ed_rd_valid_c),
      .d_rd_slot_o(ed_rd_slot_c),
      .d_rd_owner_gen_o(ed_rd_gen_c),

      .d_aux_context_i(ed_ctx_c),
      .d_lod_i(ed_lod_c),
      .d_raw_class_i(ed_class_c),
      .d_needs_aux_i(ed_aux_c),
      .d_sample_count_i(ed_count_c),
      .d_binding_sel_i(ed_bsel_c),
      .d_mosaic_mat_a_i(ed_mosa_c),
      .d_mosaic_mat_b_i(ed_mosb_c),
      .d_mosaic_weight_i(ed_mosw_c),
      .d_owner_gen_i(ed_ogen_c),
      .d_palette_slot_i(ed_pslot_c),
      .d_palette_gen_i(ed_pgen_c),

      .f_valid_o(jn_f_valid_c), .f_ready_i(jn_f_ready_c),
      .f_owner_o(jn_owner_c), .f_u_o(jn_u_c), .f_v_o(jn_v_c),
      .f_binding_o(jn_binding_c), .f_lod_o(jn_lod_c),
      .f_count_o(jn_count_c), .f_aux_o(jn_aux_c),
      .f_class_o(jn_class_c), .f_ctx_o(jn_ctx_c),
      .f_sat_o(jn_sat_c), .f_depth_zero_o(jn_dz_c),
      .f_pal_slot_o(jn_pal_slot_c), .f_pal_gen_o(jn_pal_gen_c),

      .m_valid_o(jn_m_valid_c), .m_ready_i(mos_req_ready),
      .m_mat_a_o(jn_m_mata_c), .m_mat_b_o(jn_m_matb_c),
      .m_weight_o(jn_m_wt_c),

      // As with the bank: instruments unconnected in this packet rather than
      // coupling the rewire to an island port-list change.
      .joined_o(), .saturated_o(), .depth_zero_o(), .gen_mismatch_o()
  );

  // The class SANITISATION lives here, at the read point, applied to the
  // CAPTURED class. The join passes raw class through by design -- its header
  // says the rule belongs in one place, and this is that place.
  wire [1:0] jn_class_sane_c = (jn_class_c == CLS_ERR) ? CLS_NEAR : jn_class_c;

  zhao_texture_frag_expand #(
      .FQD (4),      // the architecture's stated starting point, not a measured
                     // optimum; the composed test shows starvation if it is low
      .SRCW(18)      // {class[1:0], sample_handle[15:0]} -- §1.1's widening
  ) u_expand (
      .clk(clk), .rst_n(rst_n),
      .f_valid_i(jn_f_valid_c), .f_ready_o(jn_f_ready_c),
      .f_owner_i(jn_owner_c),
      .f_u_i(jn_u_c), .f_v_i(jn_v_c),
      // PACKET 3: the palette pair, carried from the descriptor rather than
      // looked up from palslot_m/palgen_m by owner slot at the far end.
      .f_pal_slot_i(jn_pal_slot_c), .f_pal_gen_i(jn_pal_gen_c),
      .f_binding_i(jn_binding_c), .f_lod_i(jn_lod_c),
      .f_count_i(jn_count_c), .f_aux_i(jn_aux_c),
      .f_class_i(jn_class_sane_c),
      // `fr_f_ctx`, NOT `frag_ctx_i`. Every other attribute here -- `pu_u`,
      // `pu_v`, `f_binding_c`, `f_lod_c` -- has travelled through PERSPUV with
      // its fragment; taking the context off the raw input pin instead reads
      // whatever the CALLER is presenting now, which is a later fragment. That
      // is the exact defect this island's own test names in the check text
      // "travels with its fragment instead of being read off the input pin
      // twelve clocks late", and the queue that fixes it (`fctx_m`, indexed by
      // `fc_wp`/`fc_rp`) was already here and already correct.
      .f_ctx_i(jn_ctx_c),
      .req_valid_o(exp_req_valid), .req_ready_i(exp_req_ready),
      .req_u_o(exp_req_u), .req_v_o(exp_req_v), .req_lod_o(exp_req_lod),
      .req_src_id_o(exp_req_src_id),
      .req_pal_slot_o(exp_req_pal_slot), .req_pal_gen_o(exp_req_pal_gen),
      .aux_valid_o(exp_aux_valid), .aux_ready_i(exp_aux_ready),
      .aux_owner_o(exp_aux_owner), .aux_ctx_o(exp_aux_ctx),
      .iss_tmu_valid_o(exp_iss_tmu_valid),
      .iss_tmu_handle_o(exp_iss_tmu_handle),
      .iss_aux_valid_o(exp_iss_aux_valid),
      .iss_aux_owner_o(exp_iss_aux_owner),
      .fragments_o(exp_fragments), .requests_o(exp_requests),
      .zero_sample_fragments_o(exp_zero_frags),
      .aux_requests_o(exp_aux_requests),
      .wq_overflow_o(exp_wq_overflow));

  // ==========================================================================
  // (b) THE V3 OWNER -- fragrob's jobs 1, 3, 4, 5 and 6
  // ==========================================================================
  // `zhao_texture_v3own.sv` IS NOT EDITED BY THIS INTEGRATION. Its 541-check
  // adversarial suite reads internal probes through `verilator public` markers,
  // and that suite passing on the unmodified file is gate 1 of Stage C's three.
  // Every adapter is here.
  //
  // Post-T2 measurement (V31-T2-OWNER-FIT-20260907): 3,348 ALM, 3,953 registers,
  // 17 M10K, 20,640 bits, 0 DSP, at 952 virtual pins. Against fragrob's 1,676
  // ALM the swap ADDS ~1,672 on leaf prices -- and per docket M4 a leaf price is
  // provisional in both directions, so the composed cost is genuinely unknown
  // until Stage C's fit.
  //
  // WHAT IS WIRED HERE, and what is still owed:
  //   * ISSUE notifications come from the expander, which pulses them on the
  //     ACCEPTED handshake (§11.1 event 3, not the intent). Those are real
  //     connections and they are made below.
  //   * ADMISSION, the RETURN lanes, COMBINE and the OUTPUT are the identity
  //     re-key's business -- step (c) -- because each carries an owner handle
  //     that today's signals do not have. They are left as named unresolved
  //     signals rather than plausible-looking ties, for the same reason
  //     `exp_owner_c` is.
  logic              own_adm_valid_c, own_adm_ready;
  logic [CTXW-1:0]   own_adm_ctx_c;
  logic [3:0]        own_adm_req_c;
  logic [13:0]       own_adm_owner;
  logic              own_adm_accept;

  logic              own_tmu_rvalid_c, own_tmu_rready;
  logic [15:0]       own_tmu_rhandle_c;
  logic [39:0]       own_tmu_rresult_c;
  logic              own_aux_rvalid_c, own_aux_rready;
  logic [13:0]       own_aux_rowner_c;
  logic [39:0]       own_aux_rresult_c;

  logic              own_cmb_valid, own_cmb_ready_c;
  logic [13:0]       own_cmb_owner;
  logic [39:0]       own_cmb_s0, own_cmb_s1, own_cmb_s2, own_cmb_aux;

  logic              own_fin_valid_c, own_fin_ready;
  logic [13:0]       own_fin_owner_c;
  logic [39:0]       own_fin_result_c;

  logic              own_out_valid, own_out_ready_c;
  logic [13:0]       own_out_owner;
  logic [39:0]       own_out_result;
  logic [CTXW-1:0]   own_out_ctx;

  logic [31:0] own_ev_admitted, own_ev_emitted, own_ev_commits, own_ev_tickets;
  logic [31:0] own_ev_err_range, own_ev_err_stale, own_ev_err_unsol;
  logic [31:0] own_ev_err_dup, own_ev_err_final, own_ev_err_issue;
  logic [31:0] own_ev_wrap_drains;
  logic [6:0]  own_ev_live, own_ev_live_peak;
  logic        own_ev_quiet;

  zhao_texture_v3own #(
      .OWNERS(64), .SLOTW(6), .GENW(8), .RESW(40), .CTXW(CTXW),
      .OUTQD(4), .CMBQD(4)
  ) u_own (
      .clk(clk), .rst_n(rst_n),
      // ---- admission: STEP (c) ----
      .adm_valid_i(own_adm_valid_c), .adm_ready_o(own_adm_ready),
      .adm_ctx_i(own_adm_ctx_c), .adm_req_i(own_adm_req_c),
      .adm_owner_o(own_adm_owner), .adm_accept_o(own_adm_accept),
      // ---- ISSUE: real, from the expander's accepted handshakes ----
      .iss_tmu_valid_i(exp_iss_tmu_valid),
      .iss_tmu_handle_i(exp_iss_tmu_handle),
      .iss_aux_valid_i(exp_iss_aux_valid),
      .iss_aux_owner_i(exp_iss_aux_owner),
      // ---- returns: STEP (c) ----
      .tmu_rvalid_i(own_tmu_rvalid_c), .tmu_rready_o(own_tmu_rready),
      .tmu_rhandle_i(own_tmu_rhandle_c), .tmu_rresult_i(own_tmu_rresult_c),
      .aux_rvalid_i(own_aux_rvalid_c), .aux_rready_o(own_aux_rready),
      .aux_rowner_i(own_aux_rowner_c), .aux_rresult_i(own_aux_rresult_c),
      // ---- COMBINE: STEP (c) ----
      .cmb_valid_o(own_cmb_valid), .cmb_ready_i(own_cmb_ready_c),
      .cmb_owner_o(own_cmb_owner),
      .cmb_s0_o(own_cmb_s0), .cmb_s1_o(own_cmb_s1), .cmb_s2_o(own_cmb_s2),
      .cmb_aux_o(own_cmb_aux),
      .fin_valid_i(own_fin_valid_c), .fin_ready_o(own_fin_ready),
      .fin_owner_i(own_fin_owner_c), .fin_result_i(own_fin_result_c),
      // ---- ordered output: STEP (c) ----
      .out_valid_o(own_out_valid), .out_ready_i(own_out_ready_c),
      .out_owner_o(own_out_owner), .out_result_o(own_out_result),
      .out_ctx_o(own_out_ctx),
      // ---- evidence ----
      .ev_admitted_o(own_ev_admitted), .ev_emitted_o(own_ev_emitted),
      .ev_commits_o(own_ev_commits), .ev_tickets_o(own_ev_tickets),
      .ev_err_range_o(own_ev_err_range), .ev_err_stale_o(own_ev_err_stale),
      .ev_err_unsol_o(own_ev_err_unsol), .ev_err_dup_o(own_ev_err_dup),
      .ev_err_final_o(own_ev_err_final), .ev_err_issue_o(own_ev_err_issue),
      .ev_wrap_drains_o(own_ev_wrap_drains),
      .ev_live_o(own_ev_live), .ev_live_peak_o(own_ev_live_peak),
      .ev_quiet_o(own_ev_quiet));

  // ==========================================================================
  // FRAGROB -> TMU_PLAN -> CACHE_PIPE
  // ==========================================================================
  // GLUE 3: the response CLASS. See the header. The source id carries the
  // request's identity so a sample can be returned to the right FRAGROB slot;
  // its top two bits carry the class RSP_DISPATCH routes on, because
  // CACHE_PIPE has no class lane of its own.
  // THE CLASS IS KEYED BY SLOT, not read off the input pin. A TMU request
  // carries `fr_tmu_slot` and nothing else that identifies its fragment, and
  // requests are issued long after admission, so the pin holds a later
  // fragment's class. Written when the OWNER is allocated, and keyed by the
  // owner slot -- the oracle said "when FRAGROB reports where the fragment
  // landed", and v3own's `adm_accept_o` with `adm_owner_o` is that same moment
  // and that same answer.
  // The palette binding is keyed the same way and for the same reason: a
  // sample response identifies its fragment by SLOT and nothing else.
  // `f_class_in_c` lived here: the input-stage class, sanitised by the same
  // rule the planner applies. Its ONLY consumer was `class_m`, so deleting that
  // write orphaned it -- transitively dead, found by grepping for readers after
  // the deletion rather than by assuming the deletion was self-contained.
  //
  // The reasoning it carried is still true and still enforced: an input-stage
  // copy that skipped the clamp would widen what an out-of-range class can
  // reach. Nothing takes such a copy now, and `f_class_bad_c` still stops a bad
  // class travelling at the planner. Recorded rather than silently dropped,
  // because the comment was the reason and the reason outlived the wire.

  // ---- THE PALETTE SIDECAR IS NOW LABORATORY-ONLY -------------------------
  // Packet 3 moved the palette pair onto CARRIAGE: it is captured into the
  // early descriptor at admission and travels with its fragment through the
  // join, the expander and every planner stage to the metajoin write. Nothing
  // in the production path reads these two tables any more.
  //
  // They are NOT deleted, and that is the better answer than the plan called
  // for. Their remaining readers are the shadow comparator and the CLUT
  // alignment check -- so keeping them inside the laboratory turns them into
  // exactly the migration proof D3 needs: the shadow now demonstrates that the
  // CARRIED pair equals what the sidecar lookup would have said, on live
  // traffic. Delete them and that proof goes with them.
  //
  // Declared at module scope for the same reason as `sampmeta_m`: the readers
  // are hundreds of lines away in another generate block, and hierarchical
  // references into a generate are not something to try on Quartus 17. With
  // MIGRATION_SHADOWS=0 they have no writer and no reader, and synthesis
  // removes them.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [$clog2(PAL_SLOTS)-1:0] palslot_m [64];
  logic [GENW-1:0]              palgen_m  [64];
  /* verilator lint_on UNUSEDSIGNAL */
  generate
  if (MIGRATION_SHADOWS) begin : g_pal_sidecar
  always_ff @(posedge clk) begin
    if (own_adm_accept) begin
      // ALL THREE ARE INPUT-STAGE VALUES, because admission is an input-stage
      // event: `own_adm_valid_c` is `frag_valid_i && rcp_v_ready`, and the
      // owner being written here is the one allocated for the fragment on the
      // input pins this cycle.
      //
      // `palslot_m`/`palgen_m` were taking `f_pal_slot_c`/`f_pal_gen_c`, which
      // are `fpsl_m[fc_rp]`/`fpgn_m[fc_rp]` -- the PLANNER stage's values,
      // belonging to a fragment several cycles older. `mat_m` in the same file
      // was already written from the input ports, so the two per-owner tables
      // disagreed about which fragment they were describing.
      //
      // It showed as only THREE stale palette lookups because a phase holds one
      // palette slot for most of its fragments: the misalignment is invisible
      // wherever the old value and the new one happen to be equal. A defect
      // that is mostly masked by uniform stimulus is not a small defect.
      palslot_m[own_adm_owner[13:8]] <= frag_pal_slot_i;
      palgen_m [own_adm_owner[13:8]] <= frag_pal_gen_i;
    end
  end
  end
  endgenerate

  // DEFECT (b) BOUNDARY EVIDENCE. Counted at FRAGROB ALLOCATION, which happens
  // exactly once per admitted fragment, so this is "fragments that arrived with
  // an impossible class" and not "clocks during which the pin was 3". It reads
  // the captured `f_class_bad_c`, never the pin, so the ingress-capture gate
  // stays clean -- see the note at the capture site.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) err_class_invalid_o <= 32'd0;
    // REPAIR A (owner brief 3.1). This counted `fr_alloc_valid && f_class_bad_c`
    // -- and `fr_alloc_valid` lost its only driver when the FRAGROB instance
    // was deleted, so the port was a constant zero while a healthy-run check
    // asserting it stayed clear passed happily.
    //
    // The brief's form: the ACTUAL ingress admission and THAT SAME BEAT's raw
    // class. Not `f_class_bad_c`, which is the planner-stage verdict on a
    // fragment several cycles older -- pairing an ingress event with a
    // planner-stage value is the identical mistake that made palslot_m stale.
    else if (own_adm_accept && (frag_class_i == CLS_ERR))
      err_class_invalid_o <= err_class_invalid_o + 32'd1;
  end

  // THE RESPONSE ROUTING TOKEN'S FIELDS, named once. The palette wiring below
  // used to slice this word by hand with two overlapping ranges, so nothing
  // caught that they overlapped.
  localparam int unsigned SRC_GEN_LO  = 0;
  // THE TOKEN LAYOUT, RE-DERIVED FOR THE OWNER HANDLE.
  // The oracle's slot field is $clog2(DEPTH) = 4 bits wide because its identity
  // was a FRAGROB slot. The identity is now v3own's, whose slot is SLOTW = 6 --
  // so SRC_SLOT_HI must follow the OWNER's slot width, not the reorder depth.
  // Deriving it from DEPTH here would silently take four of the six slot bits
  // and alias four owners onto every `sampmeta_m` row.
  localparam int unsigned OWN_SLOTW   = 6;
  localparam int unsigned SRC_SIDX_LO = GENW;
  localparam int unsigned SRC_SLOT_LO = GENW + 2;
  localparam int unsigned SRC_SLOT_HI = SRC_SLOT_LO + OWN_SLOTW - 1;

  // `plan_src_id` IS DELETED. The oracle built the routing token here, out of
  // fragrob's slot/sidx/gen plus a class lookup and a pad. THE EXPANDER BUILDS
  // IT NOW -- `{cls, owner_slot, sidx, owner_gen}`, at the point the request is
  // formed, from the identity it already holds.
  //
  // Its pad term is the one this restructure kept tripping over:
  // `SRCW-2-$clog2(DEPTH)-2-GENW` evaluated to EXACTLY ZERO at slot width 4,
  // which is why a 6-bit slot forced SRCW to 18 and why `cache_pipe` needed a
  // width parameter at all. The expression is gone with the construction.

  // THE CLUT BYTE SELECT, CARRIED PER SAMPLE. AUDIT R5 / docket D23.
  //
  // For CLUT8 the planner's address is BYTE granular -- `t3_base + total_c[k]`,
  // one byte per texel -- while CACHE_PIPE returns 16-bit halfwords. Which of
  // the two texels in that halfword was asked for is therefore `addr[0]`, and
  // the palette lookup read `disp_clut_data[7:0]` unconditionally: ALWAYS the
  // low byte, so every odd texel decoded its neighbour's palette index.
  //
  // The selector cannot simply be read at the palette. That lookup happens in
  // the RESPONSE path, keyed by the routing token, long after the request; and
  // `plan_acc_addr` at that moment belongs to whatever request the planner is
  // emitting now. Reading it there would be the late-read defect this island
  // was already repaired for once, and `check_ingress_capture.py` exists
  // because of it.
  //
  // So it travels, keyed by the SAMPLE's identity -- FRAGROB slot and sample
  // index -- exactly as the sample class and the palette binding already do. It
  // is per SAMPLE and not per fragment, because a fragment's three samples have
  // three different coordinates and so three different byte positions.
  //
  // The response token has no spare bits ({class 2, slot 4, sidx 2, gen 8} is
  // exactly SRCW = 16), which is why this is a side table rather than another
  // field in the token.
  // {fv, fu, bytesel} per sample. The byte select was the first field to need
  // carrying; the bilinear FRACTIONS need it for the same reason and were the
  // "additional source-level risk" the audit named without testing:
  //
  //   .fu_i(plan_acc_fu), .fv_i(plan_acc_fv)
  //
  // read the planner's CURRENT fractions while the sample data arrives through
  // cache and dispatch latency. Whatever request the planner happens to be
  // emitting when a response lands supplies that response's weights. With one
  // fragment in flight and uniform coordinates nothing moves; with varying
  // coordinates and a cache miss the filter weights belong to a different
  // texel, and every colour is subtly wrong in a way no counter shows.
  //
  // Same fix as the class, the palette binding and the byte select: store at
  // REQUEST, keyed by the identity the response returns under.
  // {fmt, fv, fu, bytesel} per sample.
  //
  // THE FORMAT JOINS THE TABLE, and it is here for exactly the reason the byte
  // select and the fractions are: the decode happens in the RESPONSE path, keyed
  // by the routing token, many clocks after the request, and `plan_acc_fmt` at
  // that moment belongs to whatever request the planner is emitting NOW. Reading
  // it there is the late-ingress defect this table exists to prevent.
  //
  // THREE BITS, AND THAT WIDTH IS THE MODE WORD'S OWN. `m_fmt` is
  // `t0_mode[2:0]` (`zhao_texture_tmu_plan.sv`), so three bits is the full
  // field the caller can name -- including the codes above FMT_ARGB4444 that
  // the planner flags as `fmt_bad`. Narrowing it to the two bits the five legal
  // formats need would silently alias a malformed format onto a legal one and
  // turn a countable routing fault into a wrong colour, which is the whole
  // failure mode `fmt_is_direct` and `cnt_near_refused_o` exist to make
  // visible.
  // 21 bits: {nib[1], fmt[3], fv[8], fu[8], bytesel[1]}. The nibble is APPENDED
  // at the top rather than inserted, so every existing bit keeps its index and
  // the readers below did not have to be renumbered -- a renumbering is exactly
  // the kind of edit that silently moves one consumer and not another.
  // 64 ROWS, NOT DEPTH=16. It is keyed by the OWNER SLOT now, and v3own has 64
  // owners. Sized by DEPTH it would drop the top two slot bits at the index and
  // three quarters of the fragments would share a row -- the same aliasing the
  // slot-width correction above prevents at the slice.
  // THE PER-SLOT TABLES ARE 64 ROWS, NOT DEPTH.
  // `class_m`, `palslot_m`, `palgen_m` and `sampmeta_m` are all keyed by the
  // token's SLOT field, and that field is v3own's 6-bit owner slot now. Sized
  // by the oracle's DEPTH=16 they would drop the top two index bits and alias
  // four owners onto every row -- a silent data corruption with no error, the
  // same shape as the `uvw_m` index and the `fc_wp`/`fc_rp` keys.
  //
  // The linter caught these as index-width truncations. It could NOT have
  // caught the slice errors, because those were the right WIDTH at the wrong
  // OFFSET -- which is why they needed traffic to find.
  // The shadow reference table. DECLARED at module scope, WRITTEN and READ only
  // inside `MIGRATION_SHADOWS` generate blocks -- so in the production profile
  // it has no writer and no reader, and synthesis removes it entirely. That is
  // what the MapOnly gate checks for.
  //
  // It stays at module scope rather than inside a generate block because its
  // readers are a thousand lines away in a different block, which would force
  // hierarchical `g_shadows.sampmeta_m` references. Quartus 17 is not the tool
  // to try that on: it rejects a bare module-scope `if` and an implicit
  // generate, both measured on 2026-09-08.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [20:0] sampmeta_m [64][3];
  /* verilator lint_on UNUSEDSIGNAL */

  logic        plan_req_ready, plan_acc_valid, plan_acc_ready;
  logic [3:0]  plan_acc_en;
  logic [127:0] plan_acc_addr;
  logic [17:0] plan_acc_src;
  logic [1:0]  plan_acc_pslot;
  logic [GENW-1:0] plan_acc_pgen;   // SRCW 18 under P0-C

  // Written on the request handshake, indexed by the identity the response will
  // come back under, so the read below cannot pick up a different sample's bit.
  generate
    if (MIGRATION_SHADOWS) begin : g_shadow_write
      always_ff @(posedge clk) begin
        if (plan_acc_valid && plan_acc_ready)
          sampmeta_m[plan_acc_src[SRC_SLOT_HI:SRC_SLOT_LO]]
                    [plan_acc_src[SRC_SIDX_LO+1:SRC_SIDX_LO]] <=
              {plan_acc_nib, plan_acc_fmt, plan_acc_fv, plan_acc_fu, plan_acc_addr[0]};
      end
    end
  endgenerate
  logic        plan_acc_filter, plan_acc_err;
  logic [7:0]  plan_acc_fu, plan_acc_fv;
  logic [2:0]  plan_acc_fmt;
  // Lane 0's nibble is the one that matters: a CLUT is never filtered, so the
  // planner drives acc_en = 4'b0001 for every palette request and only lane 0
  // is fetched. The other three are carried anyway rather than dropped at the
  // port, so that if a filtered palette ever became legal this would be a
  // widening rather than a re-plumbing.
  logic [3:0]  plan_acc_nib_lanes;
  wire         plan_acc_nib = plan_acc_nib_lanes[0];
  logic [3:0]  plan_occ;

  assign fr_tmu_ready = plan_req_ready;

  // THE PLANNER IS DRIVEN BY THE EXPANDER, not by fragrob's request port.
  //
  // THIS CONNECTION WAS MISSING AND GATE 2 FOUND IT. The expander's outputs were
  // declared and its instantiation was correct, but `u_plan` still named
  // `fr_tmu_valid` -- fragrob's port, whose only driver was deleted. An UNDRIVEN
  // signal reads as zero, so the composition ELABORATED CLEAN and then admitted
  // 24 fragments and jammed: no request ever reached the planner, nothing
  // retired, credit never returned, and admission stopped.
  //
  // Worth stating plainly because it is the argument for gate 2 existing: a lint
  // that reports zero diagnostics is not evidence that the blocks are connected
  // to each other. Only traffic is.
  zhao_texture_tmu_plan #(.SRCW(18), .PAL_CARRY(1'b1)) u_plan (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(exp_req_valid), .req_ready_o(exp_req_ready),
      .req_u_i(exp_req_u), .req_v_i(exp_req_v),
      .req_base_i(bind_base_i), .req_mode_i(bind_mode_i),
      .req_lod_i(exp_req_lod),   // already Q4.4; see LODW above
      .req_src_id_i(exp_req_src_id),
      .req_pal_slot_i(exp_req_pal_slot), .req_pal_gen_i(exp_req_pal_gen),
      .acc_valid_o(plan_acc_valid), .acc_ready_i(plan_acc_ready),
      .acc_en_o(plan_acc_en), .acc_addr_o(plan_acc_addr),
      .acc_pal_slot_o(plan_acc_pslot), .acc_pal_gen_o(plan_acc_pgen),
      .acc_src_id_o(plan_acc_src), .acc_filter_o(plan_acc_filter),
      .acc_err_o(plan_acc_err), .acc_nib_o(plan_acc_nib_lanes),
      .acc_fu_o(plan_acc_fu), .acc_fv_o(plan_acc_fv),
      .acc_fmt_o(plan_acc_fmt),
      .accepted_o(cnt_plan_accepted_o), .occupancy_o(plan_occ));

  // ==========================================================================
  // DEFECT (e): TWO AUTHORITIES DECIDE THE SAME THING AND NOBODY COMPARED THEM
  // ==========================================================================
  // THE DEFECT. TMU_PLAN resolves format and filter out of `bind_mode_i` and
  // publishes `acc_fmt_o`, `acc_filter_o` and `acc_err_o`. All three are
  // connected above and, before this block, ALL THREE WERE READ BY NOTHING --
  // a whole-file grep found only the declarations and the port connections.
  // The island instead routes on `frag_class_i`, a separate island input, and
  // the class is a FUNCTION of {format, filter}, so there is only ever one
  // right answer and two places claiming to hold it.
  //
  // THE EVIDENCE THAT IT FIRES TODAY, not latently. The composed test drives
  // `bind_mode_i = 0x6600`, which resolves to CLUT8, nearest, `acc_en_o =
  // 4'b0001` -- ONE enabled lane -- while tagging half its fragments CLS_BIL.
  //
  // THE CONSEQUENCE. For every one of those fragments:
  //   * only lane 0 was enabled, but `zhao_texture_cache_pipe.sv:453-455`
  //     writes the result slot from ALL FOUR lanes unconditionally while
  //     classifying hit/miss on enabled lanes only. Lanes 1-3 return
  //     UNTAG-CHECKED RAM contents at addresses the planner computed anyway --
  //     potentially another texture entirely;
  //   * those four words are then fed to `chan8()` as RGB565 when lane 0 in
  //     fact holds a pair of CLUT8 palette INDICES.
  // The output is a palette index byte and three stale words interpreted as
  // colour channels, bilinearly filtered, and written as the fragment's colour.
  // Not "approximately right".
  //
  // WHAT THIS PASS DOES, AND WHAT IT DELIBERATELY DOES NOT. The full repair is
  // section 5A.7's resolved sample descriptor: derive the class from the
  // planner and delete the independent input. That is a data-path change
  // touching the token layout, the metadata table and the request path, and it
  // is NOT attempted here. What is done here is to MAKE THE DISAGREEMENT
  // VISIBLE -- because right now the island is silently wrong and reads as
  // healthy, and a counted disagreement is the thing that turns "we believe the
  // fixture is testing bilinear" into a number. `frag_class_i` still routes.
  //
  // WHAT BREAKS WITHOUT THIS. Nothing new breaks -- it is already broken. What
  // breaks is the ability to KNOW: the next composed run reports colours, the
  // counters all move, and there is no signal anywhere that half the fragments
  // decoded another texture's bytes as colour.
  // ENFORCED-BY: tests/texture/island_composed_directed.cpp
  //
  // That test now READS this counter rather than only the colours: its phase 2
  // asserts err_class_mismatch_o == 0, which is what turned the 396 silent
  // warnings of the pre-W10 fixture into a check that can fail.
  wire [1:0] plan_class_carried_c = plan_acc_src[SRCW-1 -: 2];
  wire [1:0] plan_class_derived_c = class_of(plan_acc_fmt, plan_acc_filter);
  wire       plan_class_mismatch_c =
      plan_acc_valid && plan_acc_ready &&
      (plan_class_carried_c != plan_class_derived_c);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      err_class_mismatch_o <= 32'd0;
      err_plan_mode_o      <= 1'b0;
    end else begin
      if (plan_class_mismatch_c)
        err_class_mismatch_o <= err_class_mismatch_o + 32'd1;
      // `acc_err_o` is the planner's own verdict that the mode word was
      // reserved-bit-dirty, asked for a filtered palette, or named a format
      // above ARGB4444. It was connected and never read. Sticky, because one
      // occurrence already means the request that went to the cache is not the
      // request the caller described.
      if (plan_acc_valid && plan_acc_ready && plan_acc_err)
        err_plan_mode_o <= 1'b1;
    end
  end

`ifndef SYNTHESIS
  // IT REPORTS; IT DOES NOT HALT, and that is a deliberate choice rather than a
  // softened assertion.
  //
  // The disagreement this watches for is REAL AND PRESENT: the composed fixture
  // drives bind_mode 0x6600, which the planner resolves as CLUT8 / NEAREST,
  // while tagging half its fragments as the bilinear class through an
  // independent input. That is the defect, and repairing it is the fixture
  // rebuild (W10) -- not something the island can fix by stopping.
  //
  // A halting assert here would break the suite on a known, scheduled defect,
  // and a suite that is red for a scheduled reason teaches people to run it
  // with assertions off. So the event is COUNTED in err_class_mismatch_o, which
  // the composed test asserts an exact value against: nonzero today, and zero
  // when the fixture drives a mode and a class that agree. The number is the
  // evidence either way.
  //
  // The message was also wrong on its first outing -- the two string literals
  // were concatenated with {} , which Verilog evaluates as a numeric
  // concatenation and printed a 250-digit integer instead of the text. One
  // literal now.
  always_ff @(posedge clk) begin
    if (rst_n && plan_acc_valid && plan_acc_ready &&
        (plan_class_carried_c != plan_class_derived_c)) begin
      $warning("island (e): sample class disagreement -- source id says %0d, planner says %0d (fmt=%0d filter=%0b)",
               plan_class_carried_c, plan_class_derived_c, plan_acc_fmt, plan_acc_filter);
    end
  end
`endif

  logic        cache_smp_valid, cache_smp_ready;
  logic [LANES*16-1:0] cache_smp_data;
  logic [17:0] cache_smp_src;  // SRCW 18 under P0-C
  logic [31:0] cache_fills, cache_multicast, cache_replays;

  zhao_texture_cache_pipe #(
      .LANES(LANES), .LINES(16), .LINE_BYTES(16), .REQN(4), .SRCW(18)
  ) u_cache (
      .clk(clk), .rst_n(rst_n),
      .acc_valid_i(plan_acc_valid), .acc_ready_o(plan_acc_ready),
      .acc_en_i(plan_acc_en), .acc_addr_i(plan_acc_addr),
      .acc_src_id_i(plan_acc_src),
      .smp_valid_o(cache_smp_valid), .smp_ready_i(cache_smp_ready),
      .smp_data_o(cache_smp_data), .smp_src_id_o(cache_smp_src),
      .fill_valid_o(fill_valid_o), .fill_ready_i(fill_ready_i),
      .fill_addr_o(fill_addr_o),
      .fill_data_valid_i(fill_data_valid_i), .fill_data_i(fill_data_i),
      .cache_hits_o(cnt_cache_hits_o), .cache_misses_o(cnt_cache_misses_o),
      .fills_o(cache_fills), .multicast_o(cache_multicast),
      .replays_o(cache_replays));

  // ==========================================================================
  // CACHE_PIPE -> RSP_DISPATCH -> {BILERP, PALETTE}
  // ==========================================================================
  logic        disp_rsp_ready;
  logic        disp_clut_valid, disp_clut_ready;
  logic [DATAW-1:0] disp_clut_data;
  logic [TOKW-1:0]  disp_clut_tok;
  logic        disp_near_valid, disp_near_ready;
  // ALL THREE ARE READ NOW, AND THE ORDER THEY BECAME SO IS THE HISTORY OF THIS
  // LANE. Originally the VALID, the DATA and the TOKEN were all unread, which is
  // why the sample vanished and the island hung. Defect (a) gave the valid and
  // the token a real terminal completion, leaving the PAYLOAD unread because
  // decoding it needed a format -- a refused sample, counted and retiring and
  // visible, where an unread token had been a deadlock. Defect (d) supplied the
  // format, so the payload is now decoded: see `near_dec` below.
  logic [DATAW-1:0] disp_near_data;
  logic [TOKW-1:0]  disp_near_tok;
  logic        disp_bil_valid, disp_bil_ready;
  logic [DATAW-1:0] disp_bil_data;
  logic [TOKW-1:0]  disp_bil_tok;
  // DEFECT (b): the dispatcher's terminal path for a class it cannot route.
  // Only the VALID and the TOKEN are taken. The token is the whole point --
  // it carries the FRAGROB slot, sample index and generation, which is what
  // makes the failed sample completable instead of a hole. The data is left
  // unconnected because it is undecodable by construction: what went wrong is
  // the routing, so there is no format under which those 64 bits mean
  // anything. It stays a dispatcher output for a future diagnostic port.
  // ENFORCED-BY: tests/texture/island_composed_directed.cpp
  logic        disp_err_valid, disp_err_ready;
  logic [TOKW-1:0]  disp_err_tok;
  logic [31:0] disp_hol;
  logic [2:0]  disp_occ;

  // ==========================================================================
  // PACKET C: THE CREDITED READ JOIN
  // ==========================================================================
  // The brief's §5 pipeline, and the step that everything else was waiting on:
  //
  //   cache response -> RESERVE destination capacity -> synchronous metadata
  //   read -> capture data + metadata + matching identity -> dispatcher
  //
  // WHY IT IS NEEDED, established the hard way. `zhao_texture_metajoin` answers
  // ONE CYCLE after its read is launched. The dispatcher was previously handed
  // the live response and the bank's late answer together, so every queued
  // record paired a response with the PREVIOUS one's metadata.
  //
  // That fault was masked. The dispatcher itself also captured metadata from
  // the wrong point -- the current input rather than the FIFO entry being
  // dispatched -- and the two errors cancelled whenever the raw FIFO was empty,
  // which is most of the time. Fixing the dispatcher alone (leaf test: 239/240
  // wrong -> 5/5 passing) made the composed island WORSE: bilinear 32 -> 256,
  // nearest 0 -> 4, gate 2 5/122 failing.
  //
  // TWO ERRORS WERE CANCELLING, and neither could be repaired alone.
  //
  // THE RESERVATION IS THE POINT, not the register. A response is accepted from
  // the cache only when this stage can hand on what it already holds, so the
  // bank read is never launched for a response the dispatcher cannot take. The
  // brief is explicit that reserving late "admits more preparation records than
  // the context store owns"; here it would drop a response whose metadata had
  // already left the bank.
  logic                r1_v_q;
  logic [LANES*16-1:0] r1_d_q;
  logic [17:0]         r1_t_q;

  // The credit: room for the item in hand, or the item in hand is leaving.
  wire r1_room_c = !r1_v_q || disp_rsp_ready;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r1_v_q <= 1'b0;
    end else if (r1_room_c) begin
      r1_v_q <= cache_smp_valid;
      r1_d_q <= cache_smp_data;
      r1_t_q <= cache_smp_src;
    end
  end

  // The cache is held off unless the join stage has room -- the reservation.
  assign cache_smp_ready = r1_room_c;

  zhao_texture_rsp_dispatch #(
      // TOKW 18: the routing token the dispatch routes on is the widened
      // SRCW, per §1.1's third named consumer.
      .RAWN(4), .CHN(4), .DATAW(DATAW), .TOKW(18)
  , .META_EN(1'b1), .METAW(40)) u_dispatch (
      .clk(clk), .rst_n(rst_n),
      // The DELAYED response, now co-timed with the bank's answer for it.
      .rsp_valid_i(r1_v_q), .rsp_ready_o(disp_rsp_ready),
      // PACKET C step 2: the metadata rides the class queues. The bank's read
      // result is still SHADOW-ONLY -- no downstream reader consumes
      // `*_meta_o` yet -- so the island's behaviour is unchanged and gate 3
      // must still match byte for byte. Moving the readers over is the next
      // step, and it needs the credited alignment the brief's J1 models,
      // because the bank answers one cycle after the response arrives.
      .rsp_meta_i(mj_meta_packed_c),
      .clut_meta_o(disp_clut_meta), .near_meta_o(disp_near_meta),
      .bil_meta_o(disp_bil_meta),   .err_meta_o(disp_err_meta),
      .rsp_data_i(r1_d_q), .rsp_tok_i(r1_t_q),
      // THE CLASS MOVED WITH THE WIDENING. It is the token's TOP two bits, and
      // the token is now 18 -- so [17:16], not [15:14]. Slicing the old
      // position takes the slot's high bits instead and every response is
      // routed to the wrong lane: gate 2 reported 48 class disagreements,
      // which is what that looks like from the outside.
      //
      // The FOURTH instance tonight of a slice that was correct until the
      // meaning of the bits under it changed -- after `uvw_m`, `fc_wp` and
      // `fc_rp`. Written as `[TOKW-1 -: 2]` so it follows the parameter and
      // cannot go stale again.
      .rsp_class_i(r1_t_q[TOKW-1 -: 2]),
      .clut_valid_o(disp_clut_valid), .clut_ready_i(disp_clut_ready),
      .clut_data_o(disp_clut_data), .clut_tok_o(disp_clut_tok),
      // DEFECT (a) REPAIRED. This was `.near_ready_i(1'b1)` with `near_data_o`
      // and `near_tok_o` going to nets that a whole-file grep found read by
      // NOTHING -- three declarations and this instantiation, no decode, no
      // completion, not even a counter. Tied high, the class-1 pop fired
      // whenever the queue was non-empty and drained one entry per clock INTO
      // OPEN AIR. `near_ready_i` is now driven by the completion merger, so a
      // nearest response is popped only when something takes it.
      .near_valid_o(disp_near_valid), .near_ready_i(disp_near_ready),
      .near_data_o(disp_near_data), .near_tok_o(disp_near_tok),
      .bil_valid_o(disp_bil_valid), .bil_ready_i(disp_bil_ready),
      .bil_data_o(disp_bil_data), .bil_tok_o(disp_bil_tok),
      .err_valid_o(disp_err_valid), .err_ready_i(disp_err_ready),
      .err_data_o(), .err_tok_o(disp_err_tok),
      .accepted_o(cnt_dispatch_accepted_o), .hol_stall_o(disp_hol),
      .err_unknown_class_o(err_unknown_class_o),
      .occupancy_o(disp_occ));

  // GLUE 2: texel -> channel. The dispatcher hands four RGB565 texels; the
  // serial bilinear lane consumes four 8-bit channel values. This extracts ONE
  // channel (the low byte of each texel). The three-channel sequencing the
  // lane is designed for -- "serial bilinear CHANNEL engine" -- is NOT built,
  // so the lane's job counter under-reports by a factor of three until it is.
  // Written here rather than silently: an under-reporting counter that nobody
  // has flagged is worse than a missing one.
  logic        bil_out_valid, bil_out_ready;
  logic        bil_lane_valid, bil_lane_ready;
  logic [7:0]  bil_out, bil_lane_out;
  logic [23:0] bil_rgb;
  logic [7:0]  bil_a;    // the filtered alpha, defect (d)
  logic [1:0]  bil_expect_r;
  logic [TOKW-1:0] bil_lane_tok;
  logic [TOKW-1:0] bil_out_tok;
  logic [1:0]  bil_out_chan;
  logic        bil_job_ready;
  logic [1:0]  bil_occ;

  // The metadata belonging to the sample whose texels just arrived.
  // STAYS ON THE TABLE, and now for a MEASURED reason rather than a
  // precautionary one. The per-queue falsifier reports
  // **bil 768 checked, 32 wrong** -- 4.2% of bilinear responses carry
  // metadata that is not their own. The nearest queue beside it is 192/0 and
  // its reader HAS moved, so this is specific to the bilinear path, not a
  // property of the queue mechanism.
  //
  // Originally reverted after gate 2 failed:
  // ARGB4444/bilinear alpha wrong on 3 fragments. The alignment falsifier I
  // built validated the CLUT queue only -- 792 responses, zero
  // disagreements -- and I moved the BILINEAR and NEAREST readers on the
  // strength of evidence that never covered them.
  //
  // The bilinear lane sequences four channels, so a fragment's taps arrive
  // as several responses; whether each carries its own correct metadata
  // through that queue is a different question from the CLUT case, and it
  // is now an open one rather than an assumed one.
  wire [20:0] bil_meta = meta21(disp_bil_meta);
  // THE FORMAT THIS SAMPLE WAS REQUESTED UNDER, recovered the same way its
  // fractions are. Before defect (d) landed, the four taps below were decoded
  // as RGB565 whatever the binding said, so a bilinear fetch of an ARGB4444
  // sheet was filtered on the wrong bit fields and retired a confident, wrong
  // colour with no counter moving.
  wire [2:0]  bil_fmt  = bil_meta[19:17];

  // ==========================================================================
  // FOUR-CHANNEL BILINEAR SEQUENCING. AUDIT R5, EXTENDED BY DEFECT (d).
  // ==========================================================================
  // R5 MADE IT THREE; DEFECT (d) MAKES IT FOUR. Alpha is a filtered channel
  // like any other and the reference bilerps it -- `zref::Tmu::sample` runs
  // the same `bilerp` over `ca[0..3]` that it runs over r, g and b. With
  // ARGB4444 the four taps carry four DIFFERENT alphas, so taking tap 0's
  // alpha -- or the old literal 8'hFF -- is a wrong answer, not a
  // simplification.
  //
  // IT COSTS NO SILICON. The lane is one channel wide by charter and already
  // carries a 2-bit `chan_i`/`out_chan_o`; a fourth phase is one more CLOCK
  // per sample through the same multiplier, not a fourth lane. What it does
  // change is `cnt_bilerp_jobs_o`, which is now FOUR per filtered sample.
  // This issued ONE job with `chan_i = 2'd0`, filtered the LOW BYTE of each of
  // the four texel halfwords, and replicated the single result to all three
  // output channels: `{bil_out, bil_out, bil_out}`. Every direct-colour
  // fragment therefore came out grey, and no exact-colour oracle could be
  // written for the bilinear half at all -- which is why R5 could only say the
  // composed test checks "nonzero colours".
  //
  // The lane was always built for this: it carries `chan_i` in and
  // `out_chan_o` out, and never used them. What was missing was the
  // SEQUENCER around it.
  //
  // THE SHAPE. Each cache response carries four RGB565 texels, one per lane.
  // A channel is filtered independently, so one response becomes THREE jobs --
  // R, then G, then B -- each with that channel extracted from all four texels
  // and expanded to 8 bits by REPLICATION, the same law the palette path uses
  // and the one zref_texture.hpp names. The dispatch entry is held until the
  // third job is accepted, so a response is consumed exactly once.
  //
  // COLLECTION IS IN ORDER, and that is a property of the lane rather than an
  // assumption: jobs for one sample are issued back to back and the lane
  // retires in order, so R, G and B return in the order they were issued and
  // carry `out_chan_o` to prove it. The channel is CHECKED on arrival rather
  // than inferred from a counter -- see `err_bil_chan_o`.
  logic [1:0] bil_phase_r;

  // The four texels, as halfwords rather than as their low bytes.
  wire [15:0] btx [4];
  assign btx[0] = disp_bil_data[15:0];
  assign btx[1] = disp_bil_data[31:16];
  assign btx[2] = disp_bil_data[47:32];
  assign btx[3] = disp_bil_data[63:48];

  // Channel extraction, THROUGH THE ONE SHARED DECODE. This used to hold its
  // own hardwired 5:6:5 field positions with no format input -- the second
  // half of defect (d). It now selects a byte out of `decode16`, so the
  // bilinear taps and the nearest station cannot drift apart: there is one
  // law and two readers of it.
  function automatic logic [7:0] chan8(input logic [15:0] h, input logic [2:0] fmt,
                                       input logic [1:0] c);
    logic [31:0] dc;
    begin
      dc = decode16(h, fmt);
      case (c)
        2'd0:    chan8 = dc[23:16];   // red
        2'd1:    chan8 = dc[15:8];    // green
        2'd2:    chan8 = dc[7:0];     // blue
        default: chan8 = dc[31:24];   // alpha
      endcase
    end
  endfunction

  // Hold the response until its FOURTH job is taken.
  assign disp_bil_ready = bil_job_ready && (bil_phase_r == 2'd3);

  // The phase is 2 bits and the sequence is 0,1,2,3, so the wrap is the
  // counter's own. It is still written as an explicit compare rather than a
  // bare increment, because a width change would otherwise silently turn the
  // sequence into something else.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) bil_phase_r <= 2'd0;
    else if (disp_bil_valid && bil_job_ready)
      bil_phase_r <= (bil_phase_r == 2'd3) ? 2'd0 : (bil_phase_r + 2'd1);
  end

  zhao_texture_bilerp_lane #(.TOKW(TOKW)) u_bilerp (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(disp_bil_valid), .job_ready_o(bil_job_ready),
      .t00_i(chan8(btx[0], bil_fmt, bil_phase_r)),
      .t10_i(chan8(btx[1], bil_fmt, bil_phase_r)),
      .t01_i(chan8(btx[2], bil_fmt, bil_phase_r)),
      .t11_i(chan8(btx[3], bil_fmt, bil_phase_r)),
      // THE SAMPLE'S OWN fractions, recovered by the identity the response
      // came back under -- not whatever the planner is emitting right now.
      .fu_i(bil_meta[8:1]), .fv_i(bil_meta[16:9]),
      .tok_i(disp_bil_tok), .chan_i(bil_phase_r),
      .out_valid_o(bil_lane_valid), .out_ready_i(bil_lane_ready),
      .out_o(bil_lane_out), .out_tok_o(bil_lane_tok), .out_chan_o(bil_out_chan),
      .jobs_o(cnt_bilerp_jobs_o), .occupancy_o(bil_occ));

  // ---- collect R, G, B, A into one sample -----------------------------------
  // Only the LAST result presents a fragment sample downstream; the earlier
  // channels are absorbed into the accumulator. So the response port still
  // sees one answer per sample and nothing downstream had to change -- the
  // only difference from R5 is WHICH channel is last, and it is now alpha.
  logic [7:0] bil_r_r, bil_g_r, bil_b_r;

  assign bil_lane_ready = (bil_out_chan != 2'd3) ? 1'b1 : bil_out_ready;
  assign bil_out_valid  = bil_lane_valid && (bil_out_chan == 2'd3);
  assign bil_out        = bil_lane_out;
  assign bil_out_tok    = bil_lane_tok;
  assign bil_rgb        = {bil_r_r, bil_g_r, bil_b_r};
  assign bil_a          = bil_lane_out;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bil_r_r <= 8'd0;
      bil_g_r <= 8'd0;
      bil_b_r <= 8'd0;
      err_bil_chan_o <= 1'b0;
    end else if (bil_lane_valid && bil_lane_ready) begin
      case (bil_out_chan)
        2'd0: bil_r_r <= bil_lane_out;
        2'd1: bil_g_r <= bil_lane_out;
        2'd2: bil_b_r <= bil_lane_out;
        default: ;  // alpha is used combinationally above
      endcase
      // THE ORDER IS CHECKED, NOT ASSUMED. If the lane ever retires out of
      // order, the accumulator would silently pair one sample's red with
      // another's blue and every direct-colour pixel would be quietly wrong.
      if (bil_out_chan != bil_expect_r) err_bil_chan_o <= 1'b1;
      bil_expect_r <= (bil_out_chan == 2'd3) ? 2'd0 : (bil_out_chan + 2'd1);
    end
  end

  logic        pal_lu_valid_o;
  logic [15:0] pal_lu_rgb565;
  logic        pal_lu_stale, pal_lu_resident;
  logic [31:0] pal_stale, pal_cold, pal_e0, pal_e1, pal_e2, pal_e3, pal_ok;

  assign disp_clut_ready = 1'b1;  // the palette lookup is unconditional

  zhao_texture_palette_res #(
      .SLOTS(PAL_SLOTS), .ENTRIES(PAL_ENTRIES), .GENW(GENW)
  ) u_palette (
      .clk(clk), .rst_n(rst_n),
      .ld_valid_i(pal_ld_valid_i), .ld_ready_o(),
      .ld_op_i(pal_ld_op_i), .ld_slot_i(pal_ld_slot_i), .ld_gen_i(pal_ld_gen_i),
      .ld_idx_i(pal_ld_idx_i), .ld_rgb565_i(pal_ld_rgb565_i),
      .ld_crc_ok_i(pal_ld_crc_ok_i),
      .lu_valid_i(disp_clut_valid),
      // THE PALETTE'S SLOT AND GENERATION ARE THE FRAGMENT'S BINDING, not the
      // response's routing token. These two ports used to read
      // `disp_clut_tok[$clog2(PAL_SLOTS)-1:0]` and `disp_clut_tok[GENW-1:0]`
      // -- OVERLAPPING slices of the same word, so the "slot" was the low two
      // bits of the "generation", and the generation was FRAGROB's residency
      // counter, which has nothing to do with a palette upload.
      //
      // MEASURED before the repair: 96 lookups, 96 STALE, 0 cold. The slot was
      // resident and the generation never matched, so every CLUT fragment
      // retired black while the lookup counter moved and looked healthy.
      // PACKET C STEP 3: THE FIRST READERS MOVE OFF THE TABLES.
      //
      // These two were `palslot_m[disp_clut_tok[...]]` and `palgen_m[...]` --
      // two ASYNCHRONOUS 64-entry selections indexed by the queued token, and
      // they are the "64-owner palette binding selection" named in the
      // island's worst INTERNAL path:
      //
      //   rsp_dispatch|cq_rp[0][0] -> palette_res|cold_o[26]   -2.093 ns
      //
      // They now come from the class queue's REGISTERED metadata payload,
      // which travelled with this very response. Two of packet C's five
      // asynchronous response-side reads are gone, and they are the two the
      // 21-bit subset would have left behind.
      //
      // Behaviour-preserving, and measured rather than argued: the alignment
      // falsifier compared these fields against the live tables over 792 CLUT
      // responses with zero disagreements BEFORE this swap, and gate 3 must
      // still report 392 byte-identical records after it.
  // REVERTED to the live table. The dispatcher's raw-FIFO fix is CORRECT
  // (its leaf test went 239/240 wrong -> 5/5 passing), and applying it
  // exposed that the ISLAND's feed was wrong in a compensating direction:
  // `rsp_meta_i` carries the bank's output, which answers ONE CYCLE AFTER
  // `cache_smp_valid`. Capturing metadata late, at dispatch, partly
  // cancelled that latency. Capturing it correctly at input acceptance
  // does not, so the island got worse: bilinear 32 -> 256 wrong, nearest
  // 0 -> 4, gate 2 5/122 failing.
  //
  // TWO ERRORS WERE CANCELLING. Fixing one alone is a regression, which
  // is why the composed suite must be re-run after every leaf repair and
  // not only after the last one.
  //
  // The remaining work is the brief's credited reservation: reserve
  // destination capacity, then read the bank, then capture data, metadata
  // and identity together as ONE record. Until that exists the readers
  // stay on the tables and the bank stays a shadow.
      // PACKET C COMPLETE. The credited read join aligned the metadata on
      // every class queue -- bilinear 768/0, nearest 192/0, CLUT 792/0 --
      // so all five asynchronous response-side reads move together.
      //
      // These two are the '64-owner palette binding selection' on the
      // island's worst INTERNAL path. They are now registered queue
      // payload that travelled with this response.
      .lu_slot_i(disp_clut_meta[31:30]),
      .lu_gen_i (disp_clut_meta[29:22]),
      // THE ADDRESSED BYTE, not always the low one -- and for CLUT4 the
      // addressed NIBBLE of that byte. A CLUT4 address is `total >> 1`, so one
      // fetched byte carries TWO texels; taking the whole byte gave every odd
      // texel its neighbour's palette index, and nothing counted it because the
      // lookup itself succeeded. The nibble is zero-extended: CLUT4 addresses
      // 16 entries of the 256-entry palette.
      .lu_idx_i(clut_idx_c),
      .lu_valid_o(pal_lu_valid_o), .lu_rgb565_o(pal_lu_rgb565),
      .lu_stale_o(pal_lu_stale), .lu_resident_o(pal_lu_resident),
      .lookups_o(cnt_palette_lookups_o),
      .stale_o(cnt_palette_stale_o), .cold_o(cnt_palette_cold_o),
      .err_write_outside_o(pal_e0), .err_same_gen_o(pal_e1),
      .err_incomplete_o(pal_e2), .err_crc_o(pal_e3), .loads_ok_o(pal_ok));

  // The palette index, byte-selected then nibble-selected. Written out rather
  // than nested in the port map so the CLUT4 arm is visible to a reader and to
  // a grep, and so the format that decides it is named at the point of use.
  // PACKET C: the third and last sampmeta reader moves onto the queue. All
  // FIVE of the asynchronous response-side reads this packet targeted are
  // now gone -- three sampmeta selections and the palette binding pair.
  // REVERTED with the palette binding above -- same compensating-error
  // finding.
  wire [20:0] clut_meta_c = meta21(disp_clut_meta);
  wire [7:0]  clut_byte_c = clut_meta_c[0] ? disp_clut_data[15:8]
                                           : disp_clut_data[7:0];
  wire [7:0]  clut_idx_c  =
      (clut_meta_c[19:17] == FMT_CLUT4)
        ? (clut_meta_c[20] ? {4'd0, clut_byte_c[7:4]}
                           : {4'd0, clut_byte_c[3:0]})
        : clut_byte_c;

  // ---- sample responses back into FRAGROB ---------------------------------
  // The bilinear lane's byte becomes the sample's luminance-carrying channel
  // and the palette's RGB565 is expanded. Which of the two answers a given
  // request is decided by the class the request was tagged with, which is the
  // same two bits GLUE 3 carries.
  // THE RESPONSE IDENTITY MUST COME FROM WHICHEVER PATH ANSWERED.
  //
  // This derived slot/sidx/generation from `bil_out_tok` unconditionally, so a
  // palette answer carried the BILINEAR path's identity. FRAGROB rejected it
  // and nothing retired. The bug survived every earlier composed run because
  // the island tagged every request bilinear, leaving the CLUT path wired and
  // permanently idle -- `palette 0` in the trace, which read as "expected" and
  // was in fact "never tested".
  //
  // The palette's answer arrives a cycle after its request, so its token is
  // latched rather than read live.
  logic [TOKW-1:0] clut_tok_r;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)               clut_tok_r <= '0;
    else if (disp_clut_valid) clut_tok_r <= disp_clut_tok;
  end

  // ==========================================================================
  // DEFECT (c): THE PALETTE'S OWN REFUSAL WAS IGNORED
  // ==========================================================================
  // THE DEFECT. `pal_lu_stale` and `pal_lu_resident` appeared at EXACTLY TWO
  // places in this file: their declaration and their connection to the leaf.
  // The merger below formed `fr_tmu_rvalid` and `fr_tmu_rgb` from
  // `pal_lu_valid_o` ALONE, so a lookup the leaf had explicitly marked unusable
  // was shipped as an ordinary sample colour.
  //
  // THE LEAF IS NOT AT FAULT AND MUST NOT BE "FIXED". `zhao_texture_palette_res
  // .sv:87-91` says it in the port comment -- "The caller must take the cold
  // path; the colour returned is NOT usable" -- and it is right to say so: it
  // forwards a same-clock BEGIN and advances the generation AT begin, precisely
  // so a mid-load lookup cannot answer. The defect is entirely caller-side.
  //
  // WHAT THE SHIPPED WORD ACTUALLY WAS. Not a defined black. `l1_data_q` is
  // loaded unconditionally from `pal_m`, an array DELIBERATELY never reset
  // (palette_res:112-129, and reset-at-allocation is the exact thing section 7
  // of the pre-fit brief forbids for M10K). So the colour is the PREVIOUS
  // palette's entry under a live binding to a different palette -- the
  // "creature briefly wearing another creature's colours" case the leaf's own
  // header names -- or a half-written entry, or uninitialised memory.
  //
  // IT FIRES TODAY. `cnt_palette_stale_o` and `cnt_palette_cold_o` are already
  // wired to the island boundary. The island was counting the event and
  // shipping the colour anyway. A measured 96 lookups / 96 stale run is on
  // record above.
  //
  // THE REPAIR, AND THE INVARIANT IT PRESERVES. The ordinary completion is now
  // gated on all three bits; the complement is an EXPLICIT, COUNTED ERROR
  // COMPLETION that still asserts `fr_tmu_rvalid` with the same identity. That
  // last part is the whole design: FRAGROB increments `arr_q` on any accepted
  // response, so an error completion advances `arr_q == req_q` exactly as a
  // success does and the fragment RETIRES. An error must be as retirable as a
  // success -- otherwise "refuse the bad colour" turns a wrong pixel into a
  // deadlock, which is strictly worse.
  //
  // WHAT BREAKS WITHOUT THE FIX. Every stale or cold lookup paints a texel with
  // whatever bits happen to be in the palette RAM, with no way to tell it from
  // an intended colour, on a path the counters report as healthy.
  wire pal_ok_c  = pal_lu_valid_o && !pal_lu_stale && pal_lu_resident;
  wire pal_bad_c = pal_lu_valid_o && (pal_lu_stale || !pal_lu_resident);

  // ==========================================================================
  // DEFECT (a): THE NEAREST PATH RETURNED TO NOBODY
  // ==========================================================================
  // THE DEFECT. `.near_ready_i(1'b1)` above, and a whole-file grep for
  // `disp_near` returning FOUR lines: three declarations and the instantiation.
  // The data and token nets were read by nothing -- no decode, no completion,
  // not even a counter -- so the class-1 pop fired whenever the queue was
  // non-empty and drained one entry per clock into open air. `fr_tmu_rvalid`
  // was formed from the other two lanes only.
  //
  // LATENT, NOT FIRING -- WHICH IS THE PROBLEM. The composed test drives
  // `frag_class_i` as 0 or 2 only, so class 1 has never been exercised. It is a
  // trapdoor on a 2-bit island input, and the first real nearest binding would
  // have hung the island rather than drawn a wrong pixel: FRAGROB has no
  // timeout, retires in allocation order, and one un-arrived sample parks the
  // head permanently.
  //
  // WHAT THIS PASS DOES, STATED PLAINLY BECAUSE IT IS A REDUCTION. A nearest
  // response now receives a REAL TERMINAL COMPLETION carrying its own identity
  // -- but a REFUSAL completion, not a colour. It is counted in
  // `cnt_near_refused_o` and it retires the fragment. It is never silently
  // popped.
  //
  // WHY IT WAS A REFUSAL AND NOT THE COLOUR, AND WHY IT IS THE COLOUR NOW.
  // Decoding the texel needs the format, and until defect (d) landed the island
  // had NO format-controlled decode: its only extractor, `chan8()`, was
  // hardwired RGB565 with no format input, and the sample metadata table
  // carried no format field, so nothing here could tell an RGB565 texel from an
  // ARGB1555 or ARGB4444 one. Guessing RGB565 would have shipped a confidently
  // wrong colour on two of the three direct formats, indistinguishable from a
  // correct one, so `near_ok_c` was tied to `1'b0` and every nearest sample
  // completed with SMP_ERR_RGB.
  //
  // THAT PLACEHOLDER HAS BEEN REPLACED, and it had to be: `near_ok_c = 1'b0`
  // made this whole station DEAD CODE. Synthesis strips dead code, so a fit run
  // against it under-reports the design's area by an entire decode station --
  // and the fit this island is queued for exists to attack an area pathology.
  // A placeholder that hides silicon from the tool measuring silicon is worse
  // than a wrong colour.
  //
  // MEASURED BEFORE THE REPAIR, with an RGB565/nearest fixture: 96 nearest
  // samples, 96 retiring 0xFF00FF, `cnt_near_refused_o == 96`. It was invisible
  // for as long as every request in the suite was CLUT8, because CLUT8 routes
  // to CLS_CLUT and never reaches this lane at all.
  //
  // WHAT IT IS NOW. The sample's own format, recovered from `sampmeta_m` by the
  // identity the response came back under -- never from `plan_acc_fmt`, which
  // belongs to whatever request the planner is emitting this clock -- through
  // the ONE shared `decode16` the bilinear taps also use. Section 5A.7 item 5:
  // one block, not three.
  // PACKET C: moved onto the queue, now that a falsifier for THIS queue
  // exists and passes -- `meta_near_*` reports 192 checked, 0 wrong. The
  // fourth of five asynchronous response-side reads is gone.
  // REVERTED -- same finding. It measured 0/192 before the dispatcher fix
  // and 4/192 after, which is the compensation disappearing.
  wire [20:0] near_meta = meta21(disp_near_meta);
  wire [2:0]  near_fmt  = near_meta[19:17];

  // ONE TEXEL, LANE 0. A nearest request plans `acc_en_o = 4'b0001` -- the
  // planner's `filter_eff` is low, so lanes 1..3 are never fetched and the
  // words the cache copies into them are untagged RAM. Reading anything but
  // `[15:0]` here would be reading another texture.
  wire [31:0] near_dec  = decode16(disp_near_data[15:0], near_fmt);

  // A REAL PREDICATE, AND IT CAN STILL SAY NO. `cnt_near_refused_o` must read
  // zero on ordinary traffic -- that is the evidence the decode landed -- and
  // must still count a genuine fault. Two faults reach here: a CLUT format at a
  // direct-colour station, which is a routing error, and a format above
  // FMT_ARGB4444, which is the planner's own `fmt_bad`. Both are undecodable,
  // both complete with the error colour, and both are counted.
  wire near_ok_c = fmt_is_direct(near_fmt);

  // ==========================================================================
  // THE COMPLETION MERGER
  // ==========================================================================
  // FOUR SOURCES, ONE PORT. The priority order is fixed and it is not a
  // fairness choice:
  //
  //   1. PALETTE -- because it CANNOT WAIT. PALETTE_RES has no `lu_ready_i`;
  //      its answer is valid for exactly one clock and cannot be held. Both an
  //      ok and a refused palette lookup occupy this slot, because they are the
  //      same one-clock event with a different verdict.
  //   2. NEAREST refusal, 3. UNKNOWN-CLASS error -- both come out of the
  //      dispatcher's per-class queues and can be back-pressured.
  //   4. BILINEAR -- last because its lane holds its result until taken.
  //
  // NO STARVATION, and the argument is short: the dispatcher pops at most ONE
  // raw entry per clock, so the combined arrival rate into all class queues is
  // at most one per clock, while this merger retires one per clock. Bilinear
  // therefore cannot be held off indefinitely by traffic that had to arrive
  // through the same single port.
  wire [TOKW-1:0] rsp_tok = pal_lu_valid_o  ? clut_tok_r
                          : disp_near_valid ? disp_near_tok
                          : disp_err_valid  ? disp_err_tok
                                            : bil_out_tok;

  // FOUR SOURCES NOW, ONE PORT, SO THREE MUST WAIT. The history below was
  // written when there were two, and it is kept verbatim because it is the
  // argument that fixes the priority ORDER -- the two lanes added since,
  // nearest and unknown-class, both come out of dispatcher queues that CAN
  // wait, so they slot in below the palette and above the bilinear lane
  // without changing a word of it.
  //
  // The bilinear lane and the palette both answer into FRAGROB's single sample
  // response port. `bil_out_ready = fr_tmu_rready` accepted the bilinear answer
  // whenever FRAGROB was ready -- including cycles where the palette also
  // answered and the mux chose the palette. The bilinear result was then
  // consumed by nobody while its producer believed it had been taken, and the
  // fragment waiting on it never completed.
  //
  // This could not happen while every request was tagged bilinear, because the
  // two paths were never active at once. It appeared the moment the class was
  // allowed to vary, which is the second bug in this response path that being
  // permanently idle had hidden.
  //
  // The palette has no ready of its own -- it is a fixed-latency lookup -- so
  // it wins and the lane back-pressures.
  //
  // THIS IS SAFE ONLY BECAUSE `zhao_texture_fragrob.tmu_rready_o` IS TIED
  // HIGH. That is a cross-module invariant, it is load-bearing, and nothing
  // stated it until now:
  //
  //   * PALETTE_RES has no `lu_ready_i`. Its answer is valid for exactly one
  //     clock and cannot be held.
  //   * `disp_clut_ready` is tied high, so a lookup is issued without asking
  //     whether its answer can be taken.
  //   * So if FRAGROB ever stops being unconditionally ready, the palette
  //     answer is DROPPED. The fragment waiting on that sample never
  //     completes, and because FRAGROB retires in allocation order, one stuck
  //     head blocks the whole island -- the exact signature the aux-token bug
  //     produced earlier ("48 samples fetched, 0 fragments out").
  //
  // The strict priority is therefore not a fairness choice; it is the only
  // safe order given one lane cannot wait. Making the merger round-robin
  // WITHOUT first giving the palette a holding register would introduce the
  // very drop this flag watches for.
  //
  // Unreachable today, which is why it is a flag and not a repair: a tripwire
  // on an assumption, so the day the assumption changes it is reported instead
  // of being discovered as a hang.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)                                err_rsp_dropped_o <= 1'b0;
    else if (pal_lu_valid_o && !fr_tmu_rready) err_rsp_dropped_o <= 1'b1;
  end

  // The strict priority chain, written once. Each lane may fire only when no
  // higher-priority lane is presenting this clock.
  assign disp_near_ready = fr_tmu_rready && !pal_lu_valid_o;
  assign disp_err_ready  = fr_tmu_rready && !pal_lu_valid_o && !disp_near_valid;
  assign bil_out_ready   = fr_tmu_rready && !pal_lu_valid_o && !disp_near_valid
                                         && !disp_err_valid;

  // EVERY LANE COMPLETES. This used to be `bil_out_valid || pal_lu_valid_o`;
  // the two missing terms are defects (a) and (b), and each missing term was a
  // sample that never arrived at FRAGROB and therefore a fragment that never
  // retired and therefore a stalled island.
  assign fr_tmu_rvalid  = pal_lu_valid_o || disp_near_valid || disp_err_valid
                                         || bil_out_valid;

  // IS THIS COMPLETION AN ERROR? True for a palette lookup the leaf refused,
  // for a nearest response we have no decode for, and for a response whose
  // class was not routable. All three still complete; they just complete with
  // the error colour instead of a sample colour.
  wire smp_err_c = pal_bad_c
                || (!pal_lu_valid_o && disp_near_valid && !near_ok_c)
                || (!pal_lu_valid_o && !disp_near_valid && disp_err_valid);

  // ---- the counted evidence for (a) and (c) --------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      err_palette_unusable_o <= 32'd0;
      cnt_near_refused_o     <= 32'd0;
    end else begin
      // Counted at COMPLETION, not at lookup, so the number is "samples that
      // shipped an error", which is the thing a frame's worth of magenta
      // should be divisible by. `cnt_palette_stale_o`/`cnt_palette_cold_o`
      // already count the leaf's verdict; this counts what the island did
      // about it, and before this repair the two would have disagreed
      // completely -- the leaf refusing and the island shipping.
      if (pal_bad_c && fr_tmu_rready)
        err_palette_unusable_o <= err_palette_unusable_o + 32'd1;
      if (disp_near_valid && disp_near_ready && !near_ok_c)
        cnt_near_refused_o <= cnt_near_refused_o + 32'd1;
    end
  end

  // RGB565 -> RGB888 BY REPLICATION, not by zero-fill. AUDIT R5/D23.
  //
  // This appended zeros: {r5, 3'b000}. The ABI is written down --
  // zref_texture.hpp names `zref::sky::rgb565::to_rgb888` as THE expansion, and
  // that function replicates the high bits:
  //
  //     r = (r5 << 3) | (r5 >> 2);      31 -> 255
  //     g = (g6 << 2) | (g6 >> 4);      63 -> 255
  //     b = (b5 << 3) | (b5 >> 2);
  //
  // Zero-fill caps every channel below its intended maximum: 31 became 248, so
  // full white returned 248, 252, 248. The error is exactly zero at the bottom
  // of each channel and worst at the top, which is why it survived every
  // "did anything paint" check the composed test had.
  //
  // It also made the reference unusable as an oracle for this path: every
  // palette pixel differed from zref by this amount, so a per-texel comparison
  // could not have been written against it until this matched.
  wire [7:0] pal_r8 = {pal_lu_rgb565[15:11], pal_lu_rgb565[15:13]};
  wire [7:0] pal_g8 = {pal_lu_rgb565[10:5],  pal_lu_rgb565[10:9]};
  wire [7:0] pal_b8 = {pal_lu_rgb565[4:0],   pal_lu_rgb565[4:2]};

  // THE COLOUR, IN THE SAME PRIORITY ORDER AS THE TOKEN ABOVE. An errored
  // completion overrides everything: the identity still comes from whichever
  // lane answered, but the payload is the declared error colour.
  // THE NEAREST LANE IS IN THE CHAIN NOW, in the same priority order as
  // `rsp_tok` and `smp_err_c` above: palette, nearest, unknown-class, bilinear.
  // It needs no `!pal_lu_valid_o` guard of its own -- a valid palette lookup is
  // either `pal_ok_c` and taken one arm earlier, or `pal_bad_c` and already
  // folded into `smp_err_c` -- but the ORDER is what makes that true, so the
  // two chains must be edited together.
  assign fr_tmu_rgb     = smp_err_c       ? SMP_ERR_RGB
                        : pal_ok_c        ? {pal_r8, pal_g8, pal_b8}
                        : disp_near_valid ? near_dec[23:0]
                          // THREE CHANNELS, not one replicated three times.
                                          : bil_rgb;

  // ==========================================================================
  // DEFECT (d) -- ALPHA AND FORMAT DECODE. THE RECORD OF WHAT WAS MISSING IS
  // KEPT VERBATIM BELOW; WHAT CLOSED IT IS STATED AFTER IT.
  // ==========================================================================
  // The four holes are left in their original words rather than deleted,
  // because three of them are closed by a change spread across this file and
  // the fourth is still open -- and a reader who cannot see what the gap WAS
  // cannot tell which is which. The assignment they describe was:
  //
  //     assign fr_tmu_a = smp_err_c ? SMP_ERR_A : 8'hFF;
  //
  // `8'hFF` for BOTH branches, unconditionally. That is a gap and not a
  // convention, and the proof is one screen down: the AUX path at `fr_aux_a`
  // forms a REAL alpha from `aux_out_degenerate`. So the island can express a
  // non-opaque sample; the texture path simply never did.
  //
  // CONSEQUENCE: every texture sample is opaque. Any ARGB texture and any
  // index-based transparency is silently lost, and the combiner always receives
  // 255. No counter moves, because there is nothing to count -- the information
  // was discarded before it was ever formed.
  //
  // THE FOUR SEPARATE HOLES, each of which needs its own repair:
  //
  //   1. ALPHA. There is no alpha anywhere on this path. It cannot be
  //      recovered from an RGB565 word (there is none) and must come from the
  //      decode of ARGB1555/ARGB4444, or from an index-transparency rule
  //      applied to the raw CLUT index BEFORE the palette lookup replaces it.
  //
  //   2. DIRECT-COLOUR EXTRACTION IS HARDWIRED RGB565. `chan8()` above takes
  //      a halfword and a channel and has NO FORMAT INPUT. Every direct texel
  //      in this island is decoded as RGB565 whatever the binding asked for.
  //
  //   3. CLUT4'S NIBBLE IS UNRECOVERABLE DOWNSTREAM -- this is the one that
  //      forces the repair upstream, not here. The planner carries
  //      `t1_clut4`/`t2_clut4`/`t3_clut4` through every stage and emits
  //      `FMT_CLUT4`, and this island never consumes it: the palette lookup
  //      above selects a BYTE (`sampmeta_m[...][0] ? data[15:8] : data[7:0]`)
  //      and never a nibble. It CANNOT be fixed at this line, because the
  //      texel-to-byte divide in the planner has ALREADY DISCARDED the
  //      sub-byte position by the time an address exists. The nibble select
  //      must be captured at PLANNING and carried in `sampmeta_m` beside the
  //      byte select and the fractions -- which is precisely the resolved
  //      sample descriptor of section 5A.7 item 1.
  //
  //   4. ARGB1555 AND ARGB4444 HAVE NO DECODE ANYWHERE IN THE ISLAND. Not a
  //      wrong decode -- an absent one. `sampmeta_m` (declared above as
  //      `{fv, fu, bytesel}`) carries no format field and no nibble select, so
  //      even a correct decoder here would have nothing to switch on.
  //
  // ==========================================================================
  // WHAT DEFECT (d) CLOSED, AND WHAT IT DID NOT
  // ==========================================================================
  // CLOSED. Items 1, 2 and 4 above. There is one shared `decode16`; the
  // bilinear taps and the nearest station both go through it; alpha is a real
  // decoded value on both paths, filtered as a fourth channel where the sample
  // is filtered; and `sampmeta_m` carries the format so the decode can switch
  // on it without a late ingress read.
  //
  // STILL OPEN -- ITEM 3, CLUT4'S NIBBLE. It cannot be fixed at this line for
  // the reason written above: the planner's texel-to-byte divide has already
  // discarded the sub-byte position by the time an address exists, so the
  // nibble select must be captured at PLANNING alongside the byte select. The
  // metadata table now has the shape for it -- adding a `nibble` bit beside
  // `bytesel` is one more field -- but the capture is in `zhao_texture_tmu_plan`
  // and this pass did not open that file. FMT_CLUT4 therefore still reaches the
  // palette as a BYTE, which is the wrong index for every odd texel.
  //
  // ALSO STILL OPEN: index transparency. A CLUT8 sample's alpha is 255 below,
  // and that is HONEST rather than a leftover hardwire -- a palette entry is an
  // RGB565 word and carries no alpha, so the reference returns 255 for CLUT8
  // and RGB565 too and the two agree for a real reason. An index-transparency
  // rule (index 0 is clear, or a per-binding key) would have to be applied to
  // the raw index BEFORE the palette lookup replaces it, and no such rule is
  // written down anywhere yet. When one is, it belongs on the `pal_ok_c` arm.
  assign fr_tmu_a       = smp_err_c       ? SMP_ERR_A
                          // A palette entry is RGB565: opaque, and the
                          // reference says 255 here as well.
                        : pal_ok_c        ? 8'hFF
                        : disp_near_valid ? near_dec[31:24]
                                          : bil_a;
  assign fr_tmu_rslot   = rsp_tok[$clog2(DEPTH)+2+GENW-1 -: $clog2(DEPTH)];
  assign fr_tmu_rsidx   = rsp_tok[GENW+1 -: 2];
  assign fr_tmu_rgen    = rsp_tok[GENW-1:0];

  // ==========================================================================
  // (c2) THE TMU RETURN LANE INTO v3own
  // ==========================================================================
  // The completion merger above is CARRIED OVER WITH ITS PRIORITY LAW INTACT --
  // palette first because PALETTE_RES has no `lu_ready_i` and its answer lives
  // exactly one clock, then nearest, unknown-class, bilinear. Every lane
  // completes; the architecture's §1.5 is explicit that this survives, and the
  // three missing terms it once had were each "a sample that never arrived and
  // therefore a fragment that never retired".
  //
  // THE HANDLE FALLS OUT OF THE TOKEN WIDENING, which is the whole reason the
  // SRCW 16 -> 18 prerequisite landed first. Under P0-C the routing token is
  // {class[1:0], slot[5:0], sidx[1:0], gen[7:0]}, so the low SIXTEEN bits ARE
  // v3own's sample handle -- SMPW = SLOTW + 2 + GENW = 6 + 2 + 8 = 16. No
  // re-packing, no adapter, no place for a field to be mis-sliced.
  assign own_tmu_rvalid_c  = fr_tmu_rvalid;
  assign own_tmu_rhandle_c = rsp_tok[15:0];

  // result40 = STATUS8 | alpha8 | RGB888 (v3own Appendix B.1). The status byte
  // carries the merger's OWN error verdict rather than a second opinion:
  // `smp_err_c` is already the one place that decides a completion shipped the
  // error colour, and duplicating that decision here would be the "two
  // arithmetics that agree until they do not" defect on a status field.
  assign own_tmu_rresult_c = {7'd0, smp_err_c, fr_tmu_a, fr_tmu_rgb};
  assign fr_tmu_rready     = own_tmu_rready;

  // ---- the AUX return, AND A WIDTH THE ARCHITECTURE DID NOT NAME -----------
  // AUX is keyed by OWNER, not by sample: v3own's `aux_rowner_i` is the 14-bit
  // handle, because an AUX result belongs to the fragment rather than to one of
  // its samples.
  //
  // **FINDING, 2026-09-08: `AUX_TOKW` MUST WIDEN 12 -> 14 AND IT IS A TOP-LEVEL
  // PORT.** Today `AUX_TOKW = $clog2(DEPTH) + GENW` = 4 + 8 = **12**, and it
  // leaves the island on `sheet_tok_o` / returns on `sheet_rtok_i` (:149,:161).
  // v3own's OWNERW is SLOTW + GENW = 6 + 8 = **14**. The architecture's §1.1
  // widening analysis covered the SRCW routing token through plan/cache/dispatch
  // and named `cache_pipe` as the hard one; it did NOT name the AUX sheet token,
  // which is a different path and crosses the island BOUNDARY.
  //
  // That makes it a bigger change than the SRCW one, not a smaller one: an
  // island-boundary port width is the composed top's contract with whatever
  // drives it, so it cannot be defaulted away the way `cache_pipe`'s SRCW was.
  // It is recorded here at the point of discovery and belongs in the
  // architecture's §1.1 before (c3) is planned in detail.
  //
  // Until then this lane uses the oracle's ACTUAL signals at their ACTUAL
  // widths, zero-extended, so the file states what it really does rather than
  // pretending the re-key has happened:
  assign own_aux_rvalid_c  = fr_aux_rvalid;
  // THE AUX TOKEN IS THE OWNER HANDLE, WHOLE. It used to be re-assembled from
  // `fr_aux_rslot`/`fr_aux_rgen`, which slice `aux_out_tok` at the ORACLE's
  // layout -- `[AUX_TOKW-1 -: $clog2(DEPTH)]` takes four bits where the slot is
  // now six. Fifth instance tonight of a slice that survived a change in what
  // the bits mean.
  //
  // Nothing needs re-assembling: the expander sent the handle
  // (`aux_owner_o`), AUX_PIPE echoes its token opaquely, so what comes back IS
  // the handle.
  assign own_aux_rowner_c  = aux_out_tok;
  assign own_aux_rresult_c = {8'd0, fr_aux_a, fr_aux_rgb};
  assign fr_aux_rready     = own_aux_rready;

  // ==========================================================================
  // (c3) THE COMBINE SEAM -- where the oracle's shape and v3own's differ most
  // ==========================================================================
  // NOT WIRED YET, AND THE REASON IS A REAL STRUCTURAL DIFFERENCE rather than
  // remaining effort. Recorded here at the point where the difference is
  // visible, because it changes what (c3) actually is.
  //
  // THE ORACLE presents a fragment to COMBINE by INDEXING TOP-LEVEL TABLES with
  // the retiring token (:2228-2240):
  //
  //     .f_sample_count_i(fsc_m [fr_o_tok])   .f_recipe_i(frec_m[fr_o_tok])
  //     .f_weight_i      (fwt_m [fr_o_tok])   .f_base_*  (fbase_m[fr_o_tok])
  //     .f_tag_i({fseq_m[fr_o_tok], fr_o_ctx[15:0]})
  //
  // Five of the twenty side tables, read at COMBINE time, keyed by a token the
  // ROB supplies -- plus `fseq_m`, the sequence number the ROB assigns.
  //
  // V3OWN PRESENTS A WHOLE PACKET INSTEAD: `cmb_owner_o` with `cmb_s0_o`,
  // `cmb_s1_o`, `cmb_s2_o`, `cmb_aux_o` -- four result40 lanes already gathered,
  // already in retirement order, already carrying identity. There is no token to
  // index a side table with, because the packet IS the fragment.
  //
  // So (c3) is not "connect the ports". It is:
  //   * the RECIPE/WEIGHT/BASE fields must reach COMBINE some other way. They
  //     are per-fragment attributes known at ADMISSION, so they belong either in
  //     v3own's OWNER_CONTEXT (`adm_ctx_i` is CTXW=64 wide and already carried
  //     to `out_ctx_o`) or in a small table keyed by the OWNER SLOT rather than
  //     by a ROB token. Which one is a real design choice with a measurable
  //     cost, and it is the deletion ledger's `fctx_m` row that pays for it.
  //   * `f_tag_i`'s `fseq_m` half disappears with the ROB -- v3own's ordered
  //     output IS the sequence, so a separate sequence number is exactly the
  //     "partial implementation of v3own's lifetime" the architecture says to
  //     delete rather than wrap.
  //
  // THIS IS THE FIRST PLACE STAGE C STOPS BEING MECHANICAL. Everything before it
  // -- admission, the return lanes, the expander -- had a one-to-one shape in
  // the oracle. This does not, and wiring it by analogy would produce a
  // composition that elaborates and is wrong in a way no port check would catch.
  //
  // ---- THE MATERIAL PLANE, ruled by the architect as §1.7b -----------------
  // The per-fragment attributes COMBINE needs -- base colour, weight, recipe,
  // sample count -- are written ONCE at admission and read ONCE at combine.
  // One writer, one reader, keyed by the owner SLOT.
  //
  // WHY NOT IN v3own's OWNER_CONTEXT, which was the obvious alternative. It
  // fails twice, and the second reason is the decisive one:
  //
  //   * POLICY. `zhao_texture_island_top.sv`:677-686 records that an earlier
  //     island packed recipe/weight/count into bits [34:16] of the CALLER'S
  //     context word, and the owner's recovery architecture v2 §2.3 ruled it
  //     out: "Packing recipe bits into that word is not a valid way to retain
  //     an independently opaque context ... Do not silently overwrite
  //     caller-owned bits."
  //   * STRUCTURE. v3own's COMBINE interface carries NO context at all
  //     (v3own.sv:191-198 is owner + four result40 lanes), and `out_ctx_o`
  //     appears only in the ordered-output group (:211) -- which is AFTER the
  //     combiner needed the fields. Verified by reading both.
  //
  // SLOT-ONLY KEYING IS SAFE, and not by assumption: v3own re-verifies the
  // presented generation at combine admission itself --
  //     assign cmb_gen_ok_c = (win_gen_of_slot(cmb_owner_o[..slot..])
  //                            == cmb_owner_o[GENW-1:0]);   v3own.sv:1128
  // so a stale slot cannot read a live plane entry. The plane does not need to
  // carry a generation because the thing that reads it already checks one.
  //
  // This is P0-E's prescribed one-writer/one-reader bank rather than a
  // violation of §0's delete-don't-wrap: it is `fbase_m`/`frec_m`/`fwt_m`/
  // `fsc_m` re-keyed from a ROB token to the owner slot and merged under one
  // name -- four side tables becoming one, not a fifth being added.
  // THE RULING SAID 45 BITS; IT IS 46, AND THE EXTRA BIT IS `has_aux`.
  // Recorded rather than silently absorbed. §1.7b enumerates
  // {base_rgb24, base_a8, weight8, recipe3, sc2} = 45, but `u_combine`'s S2 lane
  // is a MUX, not a plain sample (:2341-2342 in the oracle):
  //
  //     .f_s2_rgb_i(fr_o_has_aux ? fr_o_aux_rgb : fr_o_s_rgb[2])
  //
  // so the combiner needs to know whether this fragment used AUX. In the oracle
  // that arrives as `fr_o_has_aux` on fragrob's output beat. v3own's COMBINE
  // packet does not carry it -- `cmb_aux_o` is a result lane, not a validity --
  // and the fact is known at ADMISSION (`frag_aux_i`), which is exactly the
  // plane's charter: per-fragment attributes written once at admission and read
  // once at combine.
  //
  // One bit, and it is the ruling's own logic applied to a field the ruling did
  // not enumerate rather than a departure from it.
  localparam int unsigned MATW = 24 + 8 + 8 + 3 + 2 + 1;   // = 46
  logic [MATW-1:0] mat_m [64];

  always_ff @(posedge clk) begin
    if (own_adm_accept)
      mat_m[own_adm_owner[13:8]] <= {frag_base_rgb_i, frag_base_a_i,
                                     frag_weight_i, frag_recipe_i,
                                     frag_sample_count_i, frag_aux_i};
  end

  // Registered read, addressed by the owner slot v3own presents at COMBINE.
  // REGISTERED because P0-E's whole finding was that an asynchronous read of a
  // 64-entry array costs its width in flip-flops -- `uvw_m` was 4,096 of them,
  // and Stage A's -4,092 is what registering one read is worth.
  // A REGISTERED READ IS A CYCLE LATE, AND THE PACKET MUST WAIT FOR IT.
  // This is the cost P0-E's finding buys: `uvw_m` cost 4,096 flip-flops for an
  // asynchronous read and Stage A recovered them by registering it. The same
  // trade here means `mat_rd_q` holds the PREVIOUS address's fields on the
  // cycle `own_cmb_valid` first rises -- and a combiner that accepted then
  // would take a different fragment's recipe, weight and base.
  //
  // v3own HOLDS `cmb_valid_o` until `cmb_ready_i`, so the address is stable and
  // the read settles after one cycle. `mat_aligned_c` says it has: the address
  // captured in `mat_addr_q` is the address being presented now, and since
  // `mat_rd_q` is loaded at the same edge as `mat_addr_q`, that compare is
  // exactly "the material I am holding belongs to this owner". The combiner is
  // offered the packet only then, and the SAME term gates its ready.
  //
  // The cost is one cycle per COMBINE admission, not per sample, and it is
  // taken deliberately rather than paid back by reintroducing the async read.
  logic [MATW-1:0]  mat_rd_q;
  logic [13:0]      mat_addr_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      mat_addr_q <= 14'd0;
    end else begin
      mat_rd_q   <= mat_m[own_cmb_owner[13:8]];
      mat_addr_q <= own_cmb_owner;
    end
  end

  // Field extraction, named once so no consumer re-derives a bit position.
  // ONE alignment term, used by BOTH halves of the handshake.
  //
  // There were two, and that was the bug: `f_valid_i` carried a registered
  // `mat_rdy_q` while `cmb_ready_i` carried this compare. A valid and a ready
  // computed from different notions of "the material is here" cannot agree,
  // and the transfer simply stopped happening -- 32 retired became 0. The
  // stale flag is deleted rather than fixed, because the failure was having a
  // second source of truth at all.
  wire        mat_aligned_c  = (mat_addr_q == own_cmb_owner);

  wire        mat_has_aux_c  = mat_rd_q[0];
  wire [1:0]  mat_scount_c   = mat_rd_q[2:1];
  wire [2:0]  mat_recipe_c   = mat_rd_q[5:3];
  wire [7:0]  mat_weight_c   = mat_rd_q[13:6];
  wire [7:0]  mat_base_a_c   = mat_rd_q[21:14];
  wire [23:0] mat_base_rgb_c = mat_rd_q[45:22];

  // Registered read, addressed by the owner slot v3own presents at COMBINE.
  // REGISTERED because P0-E's whole finding was that an asynchronous read of a
  // 64-entry array costs its width in flip-flops -- `uvw_m` was 4,096 of them,
  // and Stage A's -4,092 is what registering one read is worth.


  // `fseq_m` IS NOT RE-KEYED, IT IS DELETED. v3own's ordered output IS the
  // sequence (its cursor E), so a separate sequence number is exactly the
  // "partial implementation of v3own's lifetime" §0 says to delete rather than
  // wrap. COMBINE's TAGW therefore drops 22 -> 14: the owner handle alone.
  //
  // STILL UNRESOLVED, and named rather than tied: `own_cmb_ready_c`,
  // `own_fin_valid_c`, `own_fin_owner_c`, `own_fin_result_c`, `own_out_ready_c`
  // -- the COMBINE handshake and ordered output, which need `u_combine`'s
  // instantiation rewritten against `mat_rd_q` instead of the five table reads.

  // ==========================================================================
  // AUX
  // ==========================================================================
  logic        aux_req_ready, aux_out_valid;
  logic [AUX_TOKW-1:0] aux_out_tok;
  logic [7:0]  aux_out_tag, aux_out_str;
  logic        aux_out_degenerate;
  logic [31:0] aux_sheet_reads, aux_degenerate;

  assign fr_aux_ready = aux_req_ready;

  zhao_texture_aux_pipe #(.TOKW(AUX_TOKW)) u_aux (
      .clk(clk), .rst_n(rst_n),
      // AUX IS DRIVEN BY THE EXPANDER TOO -- the same undriven-port defect gate 2
      // caught on the TMU side, found by sweeping for it rather than by waiting
      // for a second stall. `fr_aux_valid`'s only driver was fragrob.
      //
      // The world coordinates come from the owner's CONTEXT, which is where the
      // caller put them and which v3own carries untouched -- §1.4's "the AUX
      // path also derives geometry from the opaque context", kept verbatim at
      // first integration, limitation and all.
      .req_valid_i(exp_aux_valid), .req_ready_o(exp_aux_ready),
      // THE FRAGMENT'S OWN CONTEXT, carried by the expander -- not
      // `own_out_ctx`, which belongs to whatever the OUTPUT stage is emitting
      // this cycle and is a different fragment entirely. See the expander's
      // `f_ctx_i` comment: this was asking the sheet about fragment M's world
      // position while issuing fragment N's aux request.
      .req_wx_i(exp_aux_ctx[31:0]), .req_wz_i(exp_aux_ctx[63:32]),
      .req_env_x0_i(32'sd0), .req_env_x1_i(32'sd65536),
      .req_env_z0_i(32'sd0), .req_env_z1_i(32'sd65536),
      // THE OTHER HALF OF THE AUX WIDENING. `AUX_TOKW` was widened to 14 at
      // the parameter, but this REQUEST still sourced `{fr_aux_slot,
      // fr_aux_gen}` = 4+8 = 12 -- the oracle's fragrob-shaped identity. The
      // token must BE the owner handle, which is what the return lane already
      // decodes it as. Elaboration caught the mismatch; the parameter change
      // alone was not the whole job.
      .req_tok_i(exp_aux_owner),
      .sheet_valid_o(sheet_valid_o), .sheet_ready_i(sheet_ready_i),
      .sheet_u_o(sheet_u_o), .sheet_v_o(sheet_v_o), .sheet_tok_o(sheet_tok_o),
      .sheet_rvalid_i(sheet_rvalid_i), .sheet_tag_i(sheet_tag_i),
      .sheet_str_i(sheet_str_i), .sheet_rtok_i(sheet_rtok_i),
      .out_valid_o(aux_out_valid), .out_ready_i(own_aux_rready),
      .out_tok_o(aux_out_tok), .out_tag_o(aux_out_tag),
      .out_str_o(aux_out_str), .out_degenerate_o(aux_out_degenerate),
      .accepted_o(cnt_aux_accepted_o), .sheet_reads_o(aux_sheet_reads),
      .degenerate_o(aux_degenerate));

  // ---- THE TWO PORTS FRAGROB USED TO DRIVE ---------------------------------
  // Found by SWEEPING the module's 42 outputs for ones with no driver, after
  // the same defect had already cost gate 2 once on the planner and AUX sides.
  // An undriven output does not fail elaboration and does not fail lint; it
  // reads as a clean zero, and a counter that reads zero looks exactly like a
  // stage that is quiet rather than a wire that was never connected.
  //
  // Both ports stay, because gate 3 compares the two tops through the SAME port
  // contract -- deleting a port here would make the oracle uncomparable.
  //
  // `cnt_fragments_o` is a straight re-host: the expander accepts fragments
  // where fragrob used to.
  assign cnt_fragments_o = exp_fragments;

  // `cnt_fragrob_id_errors_o` is a RE-INTERPRETATION and not an identity, which
  // is worth saying rather than hiding behind a matching port name. fragrob
  // counted responses whose id matched no outstanding fragment. v3own splits
  // that same family across three named counters -- unsolicited, stale
  // generation, duplicate -- so the sum is the closest honest reading, and it
  // must be zero in a healthy run for the same reason the original was.
  // All SIX categories, matching the sticky above. This summed three.
  assign cnt_fragrob_id_errors_o =
      own_ev_err_unsol + own_ev_err_stale + own_ev_err_dup +
      own_ev_err_range + own_ev_err_issue + own_ev_err_final;


  // ==========================================================================
  // PACKET C, STEP 1: THE METADATA BANK RUNS AS A SHADOW
  // ==========================================================================
  // Post-fit brief §5 asks for one synchronous join on the common response
  // stream, carrying sample metadata AND palette identity AND the descriptor's
  // owner generation -- 40 bits, replacing five asynchronous reads across three
  // tables.
  //
  // THIS IS NOT THAT YET. This instantiates the bank, writes it from the same
  // event that writes `sampmeta_m`, reads it from the same address on the
  // common stream, and COMPARES. Nothing downstream consumes it and no existing
  // read is removed, so the island's behaviour is unchanged by construction.
  //
  // Why a shadow rather than the swap: every integration defect this session
  // produced -- five stale slices, two stage misalignments, four undriven
  // outputs -- came from wiring a verified, never-composed block into a top and
  // trusting it. `zhao_texture_metajoin` passes its own 7-check leaf suite. So
  // did v3own's 541. A leaf suite does not know what address the composed design
  // will hand it.
  //
  // `meta_shadow_mismatch_o` is the falsifier: if the bank ever returns
  // something the live table would not have, it counts, and the composed tests
  // assert it stays zero. When it has stayed zero across the full 119-check
  // suite, the readers can move over one at a time.
  logic [1:0]  mj_rd_pal_slot;
  logic [7:0]  mj_rd_pal_gen;
  logic [2:0]  mj_rd_format;
  logic [7:0]  mj_rd_frac_u, mj_rd_frac_v;
  logic        mj_rd_byte_sel, mj_rd_nibble, mj_rd_valid;
  logic [31:0] mj_writes, mj_illegal;
  logic [GENW-1:0] mj_rd_owner_gen;
  logic [39:0] disp_clut_meta, disp_near_meta, disp_bil_meta, disp_err_meta;
  // ---- THE 21-BIT SAMPMETA VIEW OF A 40-BIT RECORD -------------------------
  // The three response-side readers below want exactly the fields `sampmeta_m`
  // held: {nibble, format, frac_v, frac_u, byte_select}. The queued record
  // carries them plus palette identity and the owner generation.
  //
  // This is a FUNCTION, not three hand-written slices, because M11 is one
  // screen of docket about what happens when field offsets are typed: all five
  // in `zhao_texture_metajoin` came out off by one, lint-clean, in the file
  // written to prevent exactly that. Extracting in one place means the three
  // call sites cannot disagree with each other or with the bank.
  function automatic logic [20:0] meta21(input logic [39:0] m);
    // format[21:19], frac_v[18:11], frac_u[10:3], byte_sel[2], nibble[1]
    return {m[1], m[21:19], m[18:11], m[10:3], m[2]};
  endfunction

  // The bank's registered result, repacked into the record layout the queue
  // carries. One cycle late relative to the response it belongs to -- which is
  // exactly the alignment the next step must close, and why nothing reads it.
  // PACKET 3 / D0d: the top eight bits were a literal `8'd0`. They now carry the
  // generation the BANK holds for the row it just returned, so the queued 40-bit
  // record's story is honest end to end -- the identity the bank validated
  // travels with the data it validated.
  //
  // It is only sound because of the D0 repair. Before the read register was gated,
  // `rd_gen_q` followed whatever address was being offered, and this field would
  // have carried a generation belonging to a different response.
  wire [39:0] mj_meta_packed_c = {mj_rd_owner_gen, mj_rd_pal_slot, mj_rd_pal_gen,
                                  mj_rd_format, mj_rd_frac_v, mj_rd_frac_u,
                                  mj_rd_byte_sel, mj_rd_nibble, 1'b0};

  wire [5:0] mj_wr_slot_c = plan_acc_src[SRC_SLOT_HI:SRC_SLOT_LO];
  wire [1:0] mj_wr_sidx_c = plan_acc_src[SRC_SIDX_LO+1:SRC_SIDX_LO];
  wire [5:0] mj_rd_slot_c = cache_smp_src[SRC_SLOT_HI:SRC_SLOT_LO];
  wire [1:0] mj_rd_sidx_c = cache_smp_src[SRC_SIDX_LO+1:SRC_SIDX_LO];

  zhao_texture_metajoin #(
      .SLOTW(6), .SIDXW(2), .GENW(GENW), .PALSW($clog2(PAL_SLOTS)), .METAW(40)
  ) u_metajoin (
      .clk(clk), .rst_n(rst_n),
      .wr_valid_i    (plan_acc_valid && plan_acc_ready),
      .wr_slot_i     (mj_wr_slot_c),
      .wr_sidx_i     (mj_wr_sidx_c),
      // The owner generation the row is written FOR. The planner's token low
      // byte IS that generation -- {class, slot, sidx, gen}.
      .wr_owner_gen_i(plan_acc_src[GENW-1:0]),
      // The palette binding, read at the OWNER slot exactly as the live path
      // does. Carrying these is the whole reason the record is 40 bits and not
      // 21: they are the fields on the measured critical family.
      // PACKET 3 / §6, THE POINT OF THE WHOLE CHAIN. This was
      // `palslot_m[mj_wr_slot_c]` / `palgen_m[...]` -- a sidecar lookup keyed by
      // owner slot, performed at the far end of the pipeline, on an identity that
      // may already have been recycled by the time the sample is planned. It is
      // D0's hazard class: a record assembled from parts that were never
      // guaranteed to belong together.
      //
      // The pair now arrives WITH the request, having been captured into the
      // descriptor at admission and carried through the join, the expander and
      // every planner stage under the same enables as the source token. It cannot
      // be stale, because it never leaves its fragment.
      .wr_pal_slot_i (plan_acc_pslot),
      .wr_pal_gen_i  (plan_acc_pgen),
      .wr_format_i   (plan_acc_fmt),
      .wr_frac_u_i   (plan_acc_fu),
      .wr_frac_v_i   (plan_acc_fv),
      .wr_byte_sel_i (plan_acc_addr[0]),
      .wr_nibble_i   (plan_acc_nib),
      // Launched only on an ACCEPTED beat, so the bank never answers for a
      // response the join stage did not take.
      .rd_valid_i    (cache_smp_valid && r1_room_c),
      .rd_slot_i     (mj_rd_slot_c),
      .rd_sidx_i     (mj_rd_sidx_c),
      .rd_owner_gen_i(cache_smp_src[GENW-1:0]),
      .rd_result_valid_o(mj_rd_valid),
      .rd_pal_slot_o (mj_rd_pal_slot),
      .rd_pal_gen_o  (mj_rd_pal_gen),
      .rd_format_o   (mj_rd_format),
      .rd_frac_u_o   (mj_rd_frac_u),
      .rd_frac_v_o   (mj_rd_frac_v),
      .rd_byte_sel_o (mj_rd_byte_sel),
      .rd_nibble_o   (mj_rd_nibble),
      .rd_owner_gen_o(mj_rd_owner_gen),
      .writes_o          (mj_writes),
      .reads_o           (meta_shadow_reads_o),
      .rd_illegal_sidx_o (mj_illegal),
      .rd_gen_mismatch_o (meta_genmis_o));

  // The live tables, sampled at the SAME address and delayed by one cycle so
  // the comparison is cycle-aligned with the bank's synchronous read. Without
  // this delay the shadow would compare a registered value against a
  // combinational one and disagree for a reason that is not a defect -- which
  // is the stage-misalignment mistake, and it would be the third time.
  // ==========================================================================
  // THE MIGRATION LABORATORY (D1/§4.2)
  // ==========================================================================
  // Everything inside this generate is APPARATUS, not the machine: the
  // reference table's readers, the three comparators, their counters and the
  // first-error captures. It exists to prove the metadata bank reproduces what
  // the live tables held, and it stops being needed once that proof is
  // accepted.
  //
  // The `else` arm ties the shadow counters to zero -- exactly the move the
  // brief warns about: "it does not permit disabling a checker, tying its
  // output to zero, and calling that healthy". `shadow_present_o` is what makes
  // it legitimate rather than a lie. A zero with the capability HIGH is
  // evidence; a zero with it LOW is silence, and the two are distinguishable
  // from outside the module.
  //
  // Explicit `generate`/`endgenerate`: Quartus 17 rejects the implicit form,
  // measured 2026-09-08 at the cost of a fit.
  assign shadow_present_o = MIGRATION_SHADOWS;

  generate
    if (MIGRATION_SHADOWS) begin : g_shadows
    logic [20:0] mj_ref_q;
    logic [1:0]  mj_ref_pslot_q;
    logic [7:0]  mj_ref_pgen_q;
    logic        mj_ref_v_q;
    always_ff @(posedge clk or negedge rst_n) begin
      if (!rst_n) begin
        mj_ref_v_q               <= 1'b0;
        meta_shadow_mismatch_o   <= 32'd0;
      end else begin
        mj_ref_q       <= sampmeta_m[mj_rd_slot_c][mj_rd_sidx_c];
        mj_ref_pslot_q <= palslot_m[mj_rd_slot_c];
        mj_ref_pgen_q  <= palgen_m [mj_rd_slot_c];
        // Gated exactly like the bank's read, or the shadow compares a
        // reference taken on a beat the bank never saw.
        mj_ref_v_q     <= cache_smp_valid && r1_room_c && (mj_rd_sidx_c != 2'd3);

        if (mj_ref_v_q && mj_rd_valid) begin
          if ({mj_rd_nibble, mj_rd_format, mj_rd_frac_v, mj_rd_frac_u,
               mj_rd_byte_sel} != mj_ref_q
              || mj_rd_pal_slot != mj_ref_pslot_q
              || mj_rd_pal_gen  != mj_ref_pgen_q)
            meta_shadow_mismatch_o <= meta_shadow_mismatch_o + 32'd1;
        end
      end
    end


    // ---- IS THE QUEUED METADATA THE RIGHT RESPONSE'S? -------------------------
    // `rsp_meta_i` is fed the bank's REGISTERED output, which answers one cycle
    // after `cache_smp_valid`. The dispatcher enqueues into a class queue via a
    // RAW fifo, so the enqueue is itself some cycles later, and whether the two
    // line up is a property of that fifo's occupancy -- not something to assume.
    //
    // This checks it where it can be checked: when a CLUT response is presented,
    // the palette fields in its queued metadata must equal what the live tables
    // hold for THAT response's own token. If the metadata belongs to a different
    // response, this counts.
    //
    // It is a falsifier for the wiring, not for the bank -- the bank itself is
    // already proven by the shadow. Nothing reads `*_meta_o`, so a nonzero count
    // here breaks nothing; it says the alignment step is still outstanding.
    always_ff @(posedge clk or negedge rst_n) begin
      if (!rst_n) begin
        meta_align_err_o  <= 32'd0;
        meta_align_chk_o  <= 32'd0;
      end else if (disp_clut_valid) begin
        meta_align_chk_o <= meta_align_chk_o + 32'd1;
        if (disp_clut_meta[29:22] != palgen_m [disp_clut_tok[SRC_SLOT_HI:SRC_SLOT_LO]]
            || disp_clut_meta[31:30] != palslot_m[disp_clut_tok[SRC_SLOT_HI:SRC_SLOT_LO]])
          meta_align_err_o <= meta_align_err_o + 32'd1;
      end
    end


    // ---- THE SAME QUESTION, ASKED OF THE OTHER TWO QUEUES ---------------------
    // The CLUT check above validated ONE class queue, and I moved three readers
    // on it -- two of which belonged to other queues. Gate 2 caught that in one
    // run. These are the checks that should have existed first.
    //
    // Each compares the queued metadata's sampmeta fields against the live table
    // at THAT queue's own token, so a pass licenses moving THAT reader and no
    // other.
    always_ff @(posedge clk or negedge rst_n) begin
      if (!rst_n) begin
        meta_bil_err_o  <= 32'd0;
        meta_bil_first_q_o   <= 21'd0;
        meta_bil_first_t_o   <= 21'd0;
        meta_bil_first_tok_o <= 18'd0;
        meta_bil_chk_o  <= 32'd0;
        meta_near_err_o <= 32'd0;
        meta_near_chk_o <= 32'd0;
      end else begin
        if (disp_bil_valid) begin
          meta_bil_chk_o <= meta_bil_chk_o + 32'd1;
          if (meta21(disp_bil_meta) !=
              sampmeta_m[disp_bil_tok[SRC_SLOT_HI:SRC_SLOT_LO]]
                        [disp_bil_tok[SRC_SIDX_LO+1:SRC_SIDX_LO]]) begin
            meta_bil_err_o <= meta_bil_err_o + 32'd1;
            // CAPTURE THE FIRST ONE. A count says how often; it does not say
            // WHAT differs, and guessing which field from a count is how the last
            // three wrong diagnoses in this session were reached.
            if (meta_bil_err_o == 32'd0) begin
              meta_bil_first_q_o <= meta21(disp_bil_meta);
              meta_bil_first_t_o <= sampmeta_m[disp_bil_tok[SRC_SLOT_HI:SRC_SLOT_LO]]
                                              [disp_bil_tok[SRC_SIDX_LO+1:SRC_SIDX_LO]];
              meta_bil_first_tok_o <= disp_bil_tok;
            end
          end
        end
        if (disp_near_valid) begin
          meta_near_chk_o <= meta_near_chk_o + 32'd1;
          if (meta21(disp_near_meta) !=
              sampmeta_m[disp_near_tok[SRC_SLOT_HI:SRC_SLOT_LO]]
                        [disp_near_tok[SRC_SIDX_LO+1:SRC_SIDX_LO]])
            meta_near_err_o <= meta_near_err_o + 32'd1;
        end
      end
    end
    end else begin : g_no_shadows
      // Not "healthy" -- ABSENT. `shadow_present_o` reads 0 here, and every
      // test that reads a counter below must consult that first.
      assign meta_shadow_mismatch_o = 32'd0;
      assign meta_align_err_o       = 32'd0;
      assign meta_align_chk_o       = 32'd0;
      assign meta_bil_err_o         = 32'd0;
      assign meta_bil_chk_o         = 32'd0;
      assign meta_near_err_o        = 32'd0;
      assign meta_near_chk_o        = 32'd0;
      assign meta_bil_first_q_o     = 21'd0;
      assign meta_bil_first_t_o     = 21'd0;
      assign meta_bil_first_tok_o   = 18'd0;
    end
  endgenerate

  // The sticky tripwire latches. `aux_degenerate` is a 32-bit COUNT rather
  // than a flag, so its tripwire is "the count ever moved" -- taken as
  // non-zero rather than as an edge, because a count that is already non-zero
  // at the first look has still tripped.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      err_fragrob_wq_overflow_o <= 1'b0;
      err_fragrob_id_error_o    <= 1'b0;
      err_aux_degenerate_o      <= 1'b0;
    end else begin
      // REPAIR C. `fr_wq_overflow` has no driver. The expander now exposes a
      // REAL capacity violation -- a fragment accepted while its queue is
      // full -- which is unreachable if `f_ready_o` is correct and is
      // therefore a tripwire whose zero means something. Backpressure
      // (`valid && !ready`) is NOT counted here; the brief is explicit that
      // conflating the two is how a port gets tied to zero and called
      // preserved.
      if (exp_wq_overflow != 32'd0)  err_fragrob_wq_overflow_o <= 1'b1;
      // REPAIR B. `fr_id_error` has no driver. v3own splits the identity-error
      // family across SIX named counters, and the brief requires naming which
      // are covered rather than summing an unstated subset:
      //
      //   range      -- a sample index outside the requested set
      //   stale      -- a return naming a superseded generation
      //   unsolicited-- a return matching no outstanding request
      //   duplicate  -- a second return for a source already committed
      //   issue      -- an illegal issue notification
      //   final      -- an unauthorized FINAL, before real COMBINE acceptance
      //
      // All six. `cnt_fragrob_id_errors_o` below summed only the first four
      // minus range -- three of six -- which is the omission the brief names.
      if (own_ev_err_range != 32'd0 || own_ev_err_stale != 32'd0 ||
          own_ev_err_unsol != 32'd0 || own_ev_err_dup   != 32'd0 ||
          own_ev_err_issue != 32'd0 || own_ev_err_final != 32'd0)
        err_fragrob_id_error_o <= 1'b1;
      if (aux_degenerate != 32'd0) err_aux_degenerate_o      <= 1'b1;
    end
  end

  assign fr_aux_rvalid = aux_out_valid;
  assign fr_aux_rgb    = {aux_out_tag, aux_out_str, 8'd0};
  assign fr_aux_a      = aux_out_degenerate ? 8'd0 : 8'hFF;
  assign fr_aux_rslot  = aux_out_tok[AUX_TOKW-1 -: $clog2(DEPTH)];
  assign fr_aux_rgen   = aux_out_tok[GENW-1:0];

  // ==========================================================================
  // FRAGROB -> MATERIAL.COMBINE.V1
  // ==========================================================================
  // GLUE 1 IS GONE. The first draft of this file held the last two retirements
  // in a shift register here and called it a sample bank, because FRAGROB
  // appeared to expose only one colour. It banks all three internally --
  // `res_rgb_m [3][DEPTH]` -- and its retire read simply took `res_rgb_m[0]`
  // and dropped the rest. So the missing piece was two output ports, not a
  // buffer in the top, and the combiner now reads REAL per-sample results.
  //
  // Worth stating plainly because the wrong version would have measured fine:
  // a shift register of past fragments has a size, fits, and reports numbers.
  // It would have composed an island whose combiner blended a fragment with
  // its two predecessors and called that a three-sample material.

  logic comb_f_ready;
  assign fr_o_ready = comb_f_ready;

  logic [31:0] comb_refused_recipe, comb_sat_add, comb_sat_2x;
  logic [31:0] comb_jobs [8];
  assign cnt_combine_jobs_o = comb_jobs;

  // ==========================================================================
  // THE ORDERING BOUNDARY (audit R6)
  //
  // The audit's finding, and it was a fair one: the composed test asserted a
  // BOUNDED displacement -- no fragment more than 8 places from where it was
  // submitted -- and a bounded defect is not an ordering guarantee. Raster and
  // blend semantics need the boundary itself.
  //
  // The reordering is not a fault to be removed. TMU responses come back out of
  // order because the memory does, and `zhao_texture_material_combine_v1`
  // retires out of order on purpose -- its own leaf test exercises that, because
  // a one-sample recipe genuinely finishes before a three-sample one that
  // started earlier and holding it back would cost throughput for nothing. So
  // the machine reorders INSIDE and orders at its EDGE, which is where the
  // caller's semantics live.
  //
  // A REORDER BUFFER, NOT A CAM AND NOT A STALL. The sequence stamped at
  // admission indexes an FCTXN-entry table directly; completed fragments are
  // written wherever they land and the head pointer walks forward as entries
  // become present.
  //
  // IT CANNOT OVERFLOW BECAUSE OF THE OWNER CREDIT ABOVE, and for no other
  // reason. This comment used to argue that FRAGROB's capacity bounded it. That
  // was wrong -- FRAGROB releases a context upstream of here, so admissions are
  // not bounded by what is parked at the output -- and the owner's recovery
  // brief supplied the counterexample. The credit is what makes the sequence's
  // low FCTXW bits identify a live fragment uniquely; without it they identify
  // a fragment modulo 64, which is not the same thing.
  //
  // The combiner's `o_ready_i` is tied high on that basis: a completing
  // fragment always has a reserved slot, and downstream back-pressure parks in
  // the buffer instead of stalling the pipe.
  logic        comb_o_valid, comb_o_ready;
  logic [23:0] comb_o_rgb;
  logic [7:0]  comb_o_a;
  // 14, not ROBTAGW's 22: the tag is the owner handle alone now that `fseq_m`
  // is deleted. Declared at the width the instantiation actually produces.
  logic [13:0] comb_o_tag;
  logic        comb_o_refused;

  // COMBINE V2, the paired-phase combiner. V1 put `unit_mul_logic(...)` inside
  // every arm of two seven-arm case statements -- about fourteen multiplier
  // sites against an architecture whose own sentence allows two -- and the
  // previous composed fit's worst path (-5.737 ns) sat inside it. V2 issues
  // each recipe as PHASES through two registered product sites instead.
  //
  // NCTX is not RECS. V1's RECS was how many sample records one job carried;
  // NCTX is how many material jobs are in flight, and the brief is explicit
  // that it is "a latency-hiding choice, not the global fragment capacity" --
  // the island's OWNER CREDIT (OWNER_DEPTH = FCTXN) is what bounds fragments.
  // Wiring FCTXN in here would conflate the two bounds and silently give the
  // combiner 64 contexts' worth of storage to hold two numbers.
  // ==========================================================================
  // COMBINE, DRIVEN BY v3own's PACKET AND THE MATERIAL PLANE
  // ==========================================================================
  // TAGW GOES 22 -> 14. The oracle's tag is `{fseq_m[tok], ctx[15:0]}` -- a ROB
  // sequence number concatenated with sixteen context bits. Under v3own the
  // ordered output IS the sequence (its cursor E), so `fseq_m` is deleted rather
  // than re-keyed, and the tag becomes the owner handle alone. That is §0's
  // "delete, not wrap" applied to the one piece of state most tempting to keep.
  //
  // THE FOUR TABLE READS BECOME ONE PLANE READ. The oracle indexes `fsc_m`,
  // `frec_m`, `fwt_m` and `fbase_m` with `fr_o_tok`; here every one of those
  // fields arrives in `mat_rd_q`, addressed by the owner slot v3own itself
  // presents and whose generation v3own itself re-checks (`cmb_gen_ok_c`).
  //
  // THE SAMPLE LANES COME FROM THE PACKET. result40 is
  // {status8, alpha8, rgb24} (v3own Appendix B.1), so rgb is [23:0] and alpha
  // [31:24] of each lane. No bank read here: v3own already gathered them.
  zhao_texture_material_combine_v2 #(.NCTX(8), .TAGW(14)) u_combine (
      .clk(clk), .rst_n(rst_n),
      .f_valid_i(own_cmb_valid && mat_aligned_c), .f_ready_o(comb_f_ready),
      .f_sample_count_i(mat_scount_c), .f_recipe_i(mat_recipe_c),
      .f_weight_i(mat_weight_c),
      .f_s0_rgb_i(own_cmb_s0[23:0]), .f_s0_a_i(own_cmb_s0[31:24]),
      .f_s1_rgb_i(own_cmb_s1[23:0]), .f_s1_a_i(own_cmb_s1[31:24]),
      // AUX, when the fragment has it, genuinely IS the third sample -- that
      // is what the aux pipeline computes. Sample bank 2 is the fallback for
      // fragments that do not. `mat_has_aux_c` is the plane's 46th bit, added
      // because this mux is the one consumer that needs it and v3own's packet
      // does not carry it.
      .f_s2_rgb_i(mat_has_aux_c ? own_cmb_aux[23:0]  : own_cmb_s2[23:0]),
      .f_s2_a_i  (mat_has_aux_c ? own_cmb_aux[31:24] : own_cmb_s2[31:24]),
      .f_base_rgb_i(mat_base_rgb_c),
      .f_base_a_i(mat_base_a_c),
      .f_tag_i(own_cmb_owner),
      .o_valid_o(comb_o_valid), .o_ready_i(comb_o_ready),
      .o_rgb_o(comb_o_rgb), .o_a_o(comb_o_a), .o_tag_o(comb_o_tag),
      .o_refused_o(comb_o_refused),
      .refused_recipe_o(comb_refused_recipe),
      .refused_missing_o(cnt_combine_refused_o),
      .saturated_add_o(comb_sat_add), .saturated_mul2x_o(comb_sat_2x),
      .jobs_by_recipe_o(comb_jobs),
      .phases_issued_o(cnt_combine_phases_o));

  // v3own's COMBINE handshake completes here: the packet is taken when the
  // combiner accepts it. §11.1's event 3 -- actual acceptance, not the
  // reservation -- which is the distinction mutation §22.10-8 exists to protect
  // and which four M6 checks catch if it is confused.
  // ...and the acceptance must be gated the same way, or v3own would see its
  // packet taken on a cycle the combiner was not actually offered it.
  // THE ALIGNMENT GUARD IS COMBINATIONAL, and `mat_rdy_q` is not it.
  //
  // `mat_rd_q` is loaded from `mat_m[own_cmb_owner]` at the same edge that
  // loads `mat_addr_q <= own_cmb_owner`, so the registered material ALWAYS
  // corresponds to `mat_addr_q`. That makes the correct question "does the
  // material I am holding belong to the owner being offered right now", which
  // is a compare of those two -- not a flag sampled a cycle earlier.
  //
  // The registered flag was wrong in one specific place: the cycle after a
  // fire. `mat_rdy_q` was computed at the previous edge, while the owner was
  // still A; COMBINE takes A; the next cycle offers B with `mat_rdy_q` still
  // high and `mat_rd_q` still A's material -- so B combines with A's base
  // colour, weight and recipe. It only bites when COMBINE accepts back to
  // back, which is why it corrupted a MINORITY of fragments and left the rest
  // exact. A stall between every pair would have hidden it completely.
  assign own_cmb_ready_c = comb_f_ready && mat_aligned_c;

  // -------- reorder buffer --------------------------------------------------
  // (d3) THE ROB STORAGE IS GONE. `rob_m[64]x33`, `rob_full_m[64]`,
  // `rob_tag_m[64]x16` and the `seq_head_r` cursor -- 3,138 bits plus a cursor
  // -- deleted because v3own's ordered output implements the same guarantee
  // once instead of twice. `comb_seq` goes with them: it sliced a sequence
  // number out of the tag, and TAGW is now 14, the owner handle alone.
  //
  // `comb_o_ready` is no longer a constant either. The oracle could assert it
  // permanently because the ROB had one slot per live fragment and so could
  // never refuse. Now the combiner's answer must be ACCEPTED BY v3own as a
  // FINAL, and v3own can legitimately not be ready -- so the handshake is real,
  // and it is `own_fin_ready` (assigned at (d2) below).

  // HEAD-OF-LINE STALL IS DELIBERATE, and it is the honest cost of this
  // boundary. If a fragment is admitted and never completes -- the condition
  // `err_rsp_dropped_o` counts -- the head stops and the island stops emitting,
  // rather than quietly skipping it and shipping a reordered stream. A lost
  // fragment was already a fault; this makes it a LOUD one. A timeout that
  // released the head would turn a stuck machine back into a silently
  // out-of-order one, which is the thing this block exists to prevent.
  // ==========================================================================
  // (d1) THE ORDERED OUTPUT: v3own's, NOT A ROB
  // ==========================================================================
  // The oracle's output IS a reorder buffer -- `rob_m[64]`, `rob_tag_m[64]`,
  // `rob_full_m[64]` and a `seq_head_r` cursor, with `rob_hit` gating emission
  // on the head being present. All of it is deleted here, because v3own's
  // ordered output is the same guarantee implemented once instead of twice:
  // its cursor E emits in admission order and its `out_valid_o` is the head
  // being present. §0's "partial implementations of v3own's lifetime" names
  // this pool first.
  //
  // THE HEAD-OF-LINE PROPERTY SURVIVES, AND MUST. The oracle's comment is worth
  // carrying because the reasoning is the design, not the implementation:
  //
  //   "If a fragment is admitted and never completes the head stops and the
  //    island stops emitting, rather than quietly skipping it and shipping a
  //    reordered stream. A lost fragment was already a fault; this makes it a
  //    LOUD one. A timeout that released the head would turn a stuck machine
  //    back into a silently out-of-order one."
  //
  // v3own behaves the same way by construction -- an owner that never completes
  // is never retired and the cursor does not advance past it -- so this is a
  // property PRESERVED by deleting the ROB, not one traded away for area.
  //
  // ENFORCED-BY: fpga/rtl/texture/zhao_texture_v3own.sv:a_out_in_order
  //
  // That assertion is the machine-resolvable enforcer, not this paragraph.
  // The mechanism it guards: `emit_q` advances ONLY on `out_fire_c`, so an
  // owner that never reaches the output queue never advances the cursor and
  // every later owner waits behind it. The stall is the property.
  //
  // It is also exercised rather than merely asserted:
  // tests/texture/island_v3_fault_directed.cpp phase 5 shuts the consumer
  // mid-flight and requires every fragment to survive, none duplicated and
  // no foreign tag -- with a non-vacuity check that the island kept
  // ACCEPTING while the sink was shut, so the ordering guarantee is put
  // under real pressure rather than a workload that never disturbs it.
  //
  // COLOUR AND STATUS COME FROM result40: {status8, alpha8, rgb24}, so refused
  // is the status byte's low bit, matching what the TMU return lane packs at
  // (c2). One layout, written once, read once.
  assign out_valid_o   = own_out_valid;
  assign out_rgb_o     = own_out_result[23:0];
  assign out_a_o       = own_out_result[31:24];
  assign out_refused_o = own_out_result[32];
  // THE OUTPUT TAG IS THE CALLER'S, NOT THE OWNER'S. I had this wrong and gate
  // 2 said so precisely: "every retired tag is one this test SUBMITTED:
  // expected 0, got 3".
  //
  // `out_tag_o` is an island BOUNDARY port -- the caller's own identifier for
  // the fragment, which the oracle carries as `f_tag_i({fseq_m, ctx[15:0]})`
  // and returns through the ROB. Emitting v3own's owner handle instead
  // substitutes an INTERNAL identity for an EXTERNAL contract: the numbers are
  // the right width and mean something, just not what the caller asked about.
  //
  // v3own carries the context untouched to `out_ctx_o` -- which is exactly the
  // opaque-context law the material-plane ruling turned on -- so the caller's
  // sixteen bits come back from there.
  assign out_tag_o     = own_out_ctx[15:0];
  assign own_out_ready_c = out_ready_i;

  // OBSERVABILITY, because an ordering boundary that works silently is
  // indistinguishable from one that was never needed. `cnt_reorder_held_o`
  // counts cycles in which a completed fragment was waiting behind an earlier
  // one; a run where it stays at zero has not exercised this at all, and a
  // test asserting strict order on such a run proves nothing.
  // ==========================================================================
  // (d2) THE ROB POOL IS DELETED. COMBINE's RESULT GOES BACK TO v3own.
  // ==========================================================================
  // What stood here was the oracle's reorder write side: `rob_m[comb_seq]`,
  // `rob_tag_m`, `rob_full_m`, the `seq_head_r` cursor advanced on emission,
  // and `rob_held_r` counting restorations. Roughly 3,300 bits plus a cursor,
  // implementing ordering the v3 owner already implements.
  //
  // Under v3own the combiner's answer is not filed into a buffer to be
  // re-ordered later; it is RETURNED TO THE OWNER as the fragment's final
  // result, and the owner emits in admission order because that is what its
  // cursor does. FINAL is a lifecycle event with a rule attached: §M6 /
  // case 22 -- a final may only be authorised by ACTUAL COMBINE ACCEPTANCE,
  // never by a reservation, and mutation §22.10-8 exists to prove the four M6
  // checks catch the confusion.
  assign own_fin_valid_c  = comb_o_valid;
  assign own_fin_owner_c  = comb_o_tag[13:0];
  assign own_fin_result_c = {7'd0, comb_o_refused, comb_o_a, comb_o_rgb};
  assign comb_o_ready     = own_fin_ready;

  // `cnt_reorder_held_o` KEEPS ITS MEANING, and this is not cosmetic. The
  // oracle counts "how many times did order have to be restored" -- a completed
  // fragment arriving while an earlier one is still missing. The paired run
  // compares colour and ORDER byte for byte but explicitly does NOT require
  // counter identity where semantics legitimately refine, and this counter is
  // on that documented list. What must not happen is it silently becoming zero:
  // a run where it stays at zero has not exercised ordering at all, and a test
  // asserting strict order on such a run proves nothing. So it is counted here
  // at the same event, from v3own's own view of it.
  logic [31:0] rob_held_r;
  assign cnt_reorder_held_o = rob_held_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rob_held_r <= 32'd0;
    end else begin
      // A final arriving for an owner that is NOT the one about to retire is
      // the same event the oracle counted: order had to be restored.
      if (own_fin_valid_c && own_fin_ready
          && (own_fin_owner_c != own_out_owner))
        rob_held_r <= rob_held_r + 32'd1;
    end
  end

endmodule : zhao_texture_island_v3_top
