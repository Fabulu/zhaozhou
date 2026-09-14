// zhao_texture_island_v3_packet_b_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-B top mutation selectors.  NOT SHIPPED.
// This file declares no design module and is absent from every production source
// list.  A private inverse build compiles this file immediately before the real
// zhao_texture_island_v3_top.sv and defines exactly one selector below.  The
// selected macro replaces one production seam in that real top; no copied toy
// implementation can drift from the machine it is meant to falsify.
//
// Ordinary builds define none of these selectors.  The top supplies the correct
// default for every hook and undefines it after its module body.
`default_nettype none

// Dispatcher/index route miswire: route-token low byte replaces immutable raw
// sample-0 index while status/alpha/RGB remain unchanged.
`ifdef ZHAO_PACKET_B_MUTANT_DISPATCH_INDEX_ROUTE
  `define ZHAO_PACKET_B_TMU_RESULT(tuple) \
      {tuple[47:40], tuple[55:48], tuple[31:0]}
`endif

// Superseded old-island law: typed AUX is wired to the combiner as sample 2.
`ifdef ZHAO_PACKET_B_MUTANT_AUX_AS_SAMPLE2
  `define ZHAO_PACKET_B_COMBINE_S2(sample2, aux) aux
`endif

// Retirement truncation: preserve only the low 64 retirement bits and zero the
// upper 96 instead of returning all 160 owner-carried bits.
`ifdef ZHAO_PACKET_B_MUTANT_RETIRE_CONTEXT_TRUNCATION
  `define ZHAO_PACKET_B_RETIRE_CONTEXT(context) \
      {96'd0, context[CTXW +: 64]}
`endif

// Clear-over-fault priority inversion.
`ifdef ZHAO_PACKET_B_MUTANT_FRAME_CLEAR_OVER_FAULT
  `define ZHAO_PACKET_B_RECOVERABLE_NEXT(fault_set, clear_fire, current) \
      (clear_fire ? 1'b0 : (fault_set ? 1'b1 : current))
`endif

// Owner-mask generation corruption: the only authority becomes invalid and must
// enter reset-lifetime recovery rather than letting descriptor/material vote.
`ifdef ZHAO_PACKET_B_MUTANT_OWNER_MASK_GENERATION
  `define ZHAO_PACKET_B_OWNER_MASK_GENERATION(generation) (generation ^ 8'h01)
`endif

// Impossible cache return: rewrite a real route token to sample index 3 before
// the metadata join.  The production top must consume/drop and latch lifetime.
`ifdef ZHAO_PACKET_B_MUTANT_CACHE_SIDX3
  `define ZHAO_PACKET_B_CACHE_TOKEN(token) \
      {token[17:10], 2'b11, token[7:0]}
`endif

// Corrupt one observed bilerp lane token.  The original token remains the return
// identity; the result must become typed loud refusal rather than clean wrong RGB.
`ifdef ZHAO_PACKET_B_MUTANT_BILERP_IDENTITY
  `define ZHAO_PACKET_B_BILERP_OBS_TOKEN(token, channel) \
      ((channel == 0) ? (token ^ 18'h00001) : token)
`endif

// Corrupt only the laboratory shadow image. Functional metadata remains exact;
// the first NEAR comparison must increment both global and NEAR mismatch counts.
`ifdef ZHAO_PACKET_B_MUTANT_SHADOW_ONE_SIDED
  `define ZHAO_PACKET_B_SHADOW_WRITE(metadata) (metadata ^ 40'h0000000008)
`endif

// Cache response hold violation: the producer's observed payload changes when
// the metadata read enters its pending cycle, while valid remains stalled. The
// shipped sticky detector must fire; functional class data remains untouched.
`ifdef ZHAO_PACKET_B_MUTANT_RSP_DROP_OBSERVATION
  `define ZHAO_PACKET_B_RSP_OBS_DATA(data, pending) \
      (data ^ {{63{1'b0}}, pending})
`endif

// ---------------------------------------------------------------------------
// Packet-E top selectors. Exactly one may be active. Each hook is consumed by
// the exact production top immediately after this file; no copied top exists.
// ---------------------------------------------------------------------------

// Historical pre-E behavior: fill denial is withheld from the cache and latches
// a reset-lifetime admission barrier. It is test-only and absent from ordinary
// and synthesis elaboration.
`ifdef ZHAO_PACKET_E_MUTANT_PRE_E_FILL_LIFETIME
  `ifdef ZHAO_PACKET_E_TOP_MUTANT_SELECTED
    `define ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_PACKET_E_TOP_MUTANT_SELECTED
  `endif
`endif

// Route every held cache refusal to physical ERR while preserving its original
// token bits. The unchanged dispatcher must count the class mismatch.
`ifdef ZHAO_PACKET_E_MUTANT_RELABEL_REFUSAL_ERR
  `ifdef ZHAO_PACKET_E_TOP_MUTANT_SELECTED
    `define ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_PACKET_E_TOP_MUTANT_SELECTED
  `endif
  `define ZHAO_PACKET_E_REFUSAL_CLASS(token) 2'd3
`endif

// Defeat the status bypass so a denied cache record enters Packet-B metadata and
// native arithmetic, losing the typed refusal result instead of terminating it.
`ifdef ZHAO_PACKET_E_MUTANT_REFUSAL_ENTERS_NATIVE
  `ifdef ZHAO_PACKET_E_TOP_MUTANT_SELECTED
    `define ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_PACKET_E_TOP_MUTANT_SELECTED
  `endif
  `define ZHAO_PACKET_E_CACHE_STATUS_IS_REFUSAL(status) 1'b0
`endif

// Native-only merge: the held refusal remains queued forever even when the
// dispatcher is ready. This is the dropped/backpressured-refusal inverse.
`ifdef ZHAO_PACKET_E_MUTANT_DROP_HELD_REFUSAL
  `ifdef ZHAO_PACKET_E_TOP_MUTANT_SELECTED
    `define ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_PACKET_E_TOP_MUTANT_SELECTED
  `endif
  `define ZHAO_PACKET_E_REFUSAL_MERGE_VALID(valid) 1'b0
`endif

`ifdef ZHAO_PACKET_E_TOP_MUTANT_SELECTED
  `undef ZHAO_PACKET_E_TOP_MUTANT_SELECTED
`endif

// The omitted-quiet-operand control is an exact source mutation rather than an
// RTL helper: the private static driver replaces the one production term
//     && !q_sheet_rsp_owed
// with
//     && 1'b1
// in a TEMP-only copy, requires audit_quiet_contract() to reject it, and then
// deletes the copy.  The same driver omits each of the 71 named detector
// operands plus the `data_quiet` bridge (72 literal equation-term mutations).
// Flattening `q_combine_idle` into its two independently tested sources also
// yields 72 physical source leaves; those are distinct accounting statements.
// No generated mutant is ever placed in a source list.

`default_nettype wire
