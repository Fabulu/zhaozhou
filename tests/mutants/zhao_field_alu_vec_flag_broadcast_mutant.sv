// zhao_field_alu_vec_flag_broadcast_mutant.sv
//
// A DELIBERATELY BROKEN COPY of fpga/rtl/field/zhao_field_alu_vec.sv.
// IT IS NOT PRODUCTION RTL AND NOTHING SHIPS IT. It is renamed so that no
// source list can elaborate it by mistake.
//
// ---------------------------------------------------------------------------
// WHAT WAS CHANGED, AND WHY THIS MUTANT EXISTS
// ---------------------------------------------------------------------------
// ONE substantive change, three lines:
//
//   production:  assign sat_add_lane_o = l_sadd & lane_live_i;
//   MUTANT:      assign sat_add_lane_o = {LANES{|(l_sadd & lane_live_i)}}
//                                        & lane_live_i;
//
// and the same for `sat_mul_lane_o` and `sat_rescale_lane_o`.
//
// This BROADCASTS the group OR back into every lane -- it is the pre-FH11
// semantics wearing the post-FH11 interface. One lane saturating now labels all
// of its live neighbours, which is exactly the defect FH11 was written to
// remove (owner directive 2026-09-20 §2.8, §8.3, and its own mutant class list
// at line 2668: "SIMD flags ORed back into every point").
//
// ---------------------------------------------------------------------------
// WHY A MUTANT IS OWED HERE AT ALL
// ---------------------------------------------------------------------------
// The per-lane ports are a NEW instrument, and this repository's standing rule
// is that a detector which has not been seen to fire has not been tested. The
// subtlety is that the broadcast defect is INVISIBLE to every aggregate check:
// `sat_add_o` is identical under production and under this mutant, because the
// OR of a broadcast OR is the same OR. A suite that only ever asserted the
// group flag would pass against both files and could not tell them apart.
//
// So this mutant is the positive control for the ATTRIBUTION specifically, and
// the only checks that separate it from production are the ones that name a
// lane.
//
// ---------------------------------------------------------------------------
// HOW IT IS DRIVEN, AND THE NEGATIVE CONTROL
// ---------------------------------------------------------------------------
// `tests/differential/field_alu_vec_flag_attribution_control.cpp` is ONE driver
// compiled TWICE, selected by the C++ object-like macros `ZHAO_ALUVEC_TOP_HEADER`
// and `ZHAO_ALUVEC_TOP_TYPE`:
//
//   field_alu_vec_flag_attribution        -> against PRODUCTION, must PASS
//   field_alu_vec_flag_broadcast_control  -> against THIS FILE,  must FAIL
//                                            (registered WILL_FAIL TRUE)
//
// Both polarities come from the SAME asserted expectations, so the pair cannot
// drift apart, and the difference in outcome IS the proof that the selector
// engaged. There is no Verilog `-D` involved and therefore none of the
// function-like-macro trap CLAUDE.md records: the two builds compile two
// different FILES declaring two different MODULE names, so a selector that
// failed to apply would not link rather than silently measuring production.
//
// REGENERATE THIS FILE if zhao_field_alu_vec.sv changes shape. It is a COPY,
// and a copy of an old version is a positive control for a block that no
// longer exists. `tools/budget/mutant_copy_drift.py` watches for exactly that.
`default_nettype none

module zhao_field_alu_vec_flag_broadcast_mutant #(
    parameter int LANES = 4
) (
    // Shared control: one instruction across the whole quad.
    input var logic [ 7:0] op_i,
    input var logic [31:0] imm_i,

    input var logic [LANES-1:0] lane_live_i,

    // Per-lane operands, packed. Lane l occupies bits [32*l +: 32].
    input var logic signed [32*LANES-1:0] a0_i, a1_i, a2_i,
    input var logic signed [32*LANES-1:0] b0_i, b1_i, b2_i,
    input var logic signed [32*LANES-1:0] c_i,

    input var logic signed [66*LANES-1:0] prod_ab_i,
    input var logic signed [66*LANES-1:0] dot2_i,
    input var logic signed [66*LANES-1:0] dot3_i,

    output var logic signed [32*LANES-1:0] result_o,

    // Opcode properties: identical in every lane, taken from lane 0.
    output var logic is_end_o,
    output var logic writes_o,
    output var logic op_unsupported_o,

    output var logic [LANES-1:0] sat_add_lane_o,
    output var logic [LANES-1:0] sat_mul_lane_o,
    output var logic [LANES-1:0] sat_rescale_lane_o,

    output var logic sat_add_o,
    output var logic sat_mul_o,
    output var logic sat_rescale_o,

    output var logic lane_desync_o
);

  logic [LANES-1:0] l_end, l_writes, l_unsup;
  logic [LANES-1:0] l_sadd, l_smul, l_srescale;

  genvar gl;
  generate
  for (gl = 0; gl < LANES; gl++) begin : gen_lane
    zhao_field_alu u_alu (
        .op_i (op_i),
        .imm_i(imm_i),
        .a0_i (a0_i[32*gl+:32]),
        .a1_i (a1_i[32*gl+:32]),
        .a2_i (a2_i[32*gl+:32]),
        .b0_i (b0_i[32*gl+:32]),
        .b1_i (b1_i[32*gl+:32]),
        .b2_i (b2_i[32*gl+:32]),
        .c_i  (c_i[32*gl+:32]),
        .prod_ab_i(prod_ab_i[66*gl+:66]),
        .dot2_i   (dot2_i[66*gl+:66]),
        .dot3_i   (dot3_i[66*gl+:66]),
        .result_o (result_o[32*gl+:32]),
        .is_end_o        (l_end[gl]),
        .writes_o        (l_writes[gl]),
        .op_unsupported_o(l_unsup[gl]),
        .sat_add_o    (l_sadd[gl]),
        .sat_mul_o    (l_smul[gl]),
        .sat_rescale_o(l_srescale[gl])
    );
  end
  endgenerate

  assign is_end_o         = l_end[0];
  assign writes_o         = l_writes[0];
  assign op_unsupported_o = l_unsup[0];

  // ===========================================================================
  // THE MUTATION -- three lines, and the only substantive difference from
  // fpga/rtl/field/zhao_field_alu_vec.sv.
  //
  // Production publishes the NARROW flags. This broadcasts the group OR into
  // every live lane, destroying the attribution while leaving every aggregate
  // bit-identical.
  // ===========================================================================
  assign sat_add_lane_o     = {LANES{|(l_sadd     & lane_live_i)}} & lane_live_i;
  assign sat_mul_lane_o     = {LANES{|(l_smul     & lane_live_i)}} & lane_live_i;
  assign sat_rescale_lane_o = {LANES{|(l_srescale & lane_live_i)}} & lane_live_i;

  assign sat_add_o     = |sat_add_lane_o;
  assign sat_mul_o     = |sat_mul_lane_o;
  assign sat_rescale_o = |sat_rescale_lane_o;

  always_comb begin
    lane_desync_o = 1'b0;
    for (int l = 1; l < LANES; l++)
      if ((l_end[l] != l_end[0]) || (l_writes[l] != l_writes[0]) ||
          (l_unsup[l] != l_unsup[0]))
        lane_desync_o = 1'b1;
  end

endmodule : zhao_field_alu_vec_flag_broadcast_mutant

`default_nettype wire
