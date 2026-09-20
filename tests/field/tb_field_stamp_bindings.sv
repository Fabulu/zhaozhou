// tb_field_stamp_bindings.sv -- the S profile's TWO NAMED BINDINGS, side by
// side, driven independently.
//
// WHY THIS WRAPPER EXISTS, AND WHY IT IS NOT A CONVENIENCE.
//
// Owner directive 15.2 requires `LEGACY_STAMP_BRUSH` and `CANONICAL_STAMP` to
// be two named application bindings, and says of them: **"The legacy bridge is
// not evidence that the full canonical Stamp binding works."**
//
// `STAMP_BINDING` is a PARAMETER, so a single verilated top can only ever be
// one of them. A directed test that elaborated one binding and reported "the
// stamp adapter passes" would be the exact instrument defect this campaign
// exists to remove -- a green that is structurally unable to see the other
// half. Two instances in one wrapper is the cheapest arrangement in which the
// two bindings CANNOT share a result:
//
//   * each has its OWN complete port set, its own stimulus and its own
//     assertions, so a case that drives `l_` leaves every `c_` counter where
//     it was, and the test can assert that it did;
//   * and the discriminating case drives BOTH with the SAME engine response
//     and shows the two delivering DIFFERENT records -- which is the only
//     shape of evidence that proves they are two contracts rather than one
//     contract with two names.
//
// Both are 64-wide on purpose: 15.2's worked example is stated for a 64-wide
// sheet -- "a 64-wide unit center (2*i+1)/128 maps exactly to fx16
// (2*i+1)*512" -- and a test on an 8-wide sheet would verify a different shift
// and agree with itself.
//
// This is a TEST wrapper. It is not in any production source list.

`default_nettype none

module tb_field_stamp_bindings (
    input  var logic clk,
    input  var logic rst_n,

    // ---- LEGACY_STAMP_BRUSH ------------------------------------------------
    input  var logic         l_cmd_fire_i,
    input  var logic         l_cmd_field_en_i,
    input  var logic [2:0]   l_slot_i,
    input  var logic         l_slot_valid_i,
    output var logic         l_arm_ready_o,
    output var logic         l_fld_valid_o,
    input  var logic         l_fld_ready_i,
    output var logic [31:0]  l_fld_tag_op_o,
    output var logic [15:0]  l_fld_strength_o,
    output var logic [15:0]  l_fld_emissive_o,
    output var logic         l_req_valid_o,
    input  var logic         l_req_ready_i,
    output var logic [2:0]   l_req_slot_o,
    output var logic         l_req_noprog_o,
    output var logic [415:0] l_req_in_o,
    input  var logic         l_resp_valid_i,
    output var logic         l_resp_ready_o,
    input  var logic [223:0] l_resp_out_i,
    input  var logic [7:0]   l_resp_status_i,
    output var logic [31:0]  l_stamps_o,
    output var logic [31:0]  l_texels_o,
    output var logic [31:0]  l_faults_o,
    output var logic [31:0]  l_restarts_o,
    output var logic [31:0]  l_canon_clamps_o,
    output var logic         l_busy_o,

    // ---- CANONICAL_STAMP ---------------------------------------------------
    input  var logic         c_cmd_fire_i,
    input  var logic         c_cmd_field_en_i,
    input  var logic [2:0]   c_slot_i,
    input  var logic         c_slot_valid_i,
    output var logic         c_arm_ready_o,
    output var logic         c_fld_valid_o,
    input  var logic         c_fld_ready_i,
    output var logic [31:0]  c_fld_tag_op_o,
    output var logic [15:0]  c_fld_strength_o,
    output var logic [15:0]  c_fld_emissive_o,
    output var logic         c_req_valid_o,
    input  var logic         c_req_ready_i,
    output var logic [2:0]   c_req_slot_o,
    output var logic         c_req_noprog_o,
    output var logic [415:0] c_req_in_o,
    input  var logic         c_resp_valid_i,
    output var logic         c_resp_ready_o,
    input  var logic [223:0] c_resp_out_i,
    input  var logic [7:0]   c_resp_status_i,
    output var logic [31:0]  c_stamps_o,
    output var logic [31:0]  c_texels_o,
    output var logic [31:0]  c_faults_o,
    output var logic [31:0]  c_restarts_o,
    output var logic [31:0]  c_canon_clamps_o,
    output var logic         c_busy_o
);

  // Binding 1: the exact pre-2026-09-20 bridge. This is the configuration
  // `zhao_console_core.sv` composes today.
  zhao_field_stamp_adapter #(
    .SHEET_W      (64),
    .SHEET_H      (64),
    .SLOTW        (3),
    .IN_LANES     (13),
    .OUT_LANES    (7),
    .STAMP_BINDING(1)
  ) u_legacy (
    .clk(clk), .rst_n(rst_n),
    .cmd_fire_i(l_cmd_fire_i), .cmd_field_en_i(l_cmd_field_en_i),
    .slot_i(l_slot_i), .slot_valid_i(l_slot_valid_i), .arm_ready_o(l_arm_ready_o),
    .fld_valid_o(l_fld_valid_o), .fld_ready_i(l_fld_ready_i),
    .fld_tag_op_o(l_fld_tag_op_o), .fld_strength_o(l_fld_strength_o),
    .fld_emissive_o(l_fld_emissive_o),
    .req_valid_o(l_req_valid_o), .req_ready_i(l_req_ready_i),
    .req_slot_o(l_req_slot_o), .req_noprog_o(l_req_noprog_o), .req_in_o(l_req_in_o),
    .resp_valid_i(l_resp_valid_i), .resp_ready_o(l_resp_ready_o),
    .resp_out_i(l_resp_out_i), .resp_status_i(l_resp_status_i),
    .stamps_o(l_stamps_o), .texels_o(l_texels_o), .faults_o(l_faults_o),
    .restarts_o(l_restarts_o), .canon_clamps_o(l_canon_clamps_o), .busy_o(l_busy_o)
  );

  // Binding 2: the full eight-input / three-output S signature, with 15.2's
  // two written conversions.
  zhao_field_stamp_adapter #(
    .SHEET_W      (64),
    .SHEET_H      (64),
    .SLOTW        (3),
    .IN_LANES     (13),
    .OUT_LANES    (7),
    .STAMP_BINDING(2)
  ) u_canonical (
    .clk(clk), .rst_n(rst_n),
    .cmd_fire_i(c_cmd_fire_i), .cmd_field_en_i(c_cmd_field_en_i),
    .slot_i(c_slot_i), .slot_valid_i(c_slot_valid_i), .arm_ready_o(c_arm_ready_o),
    .fld_valid_o(c_fld_valid_o), .fld_ready_i(c_fld_ready_i),
    .fld_tag_op_o(c_fld_tag_op_o), .fld_strength_o(c_fld_strength_o),
    .fld_emissive_o(c_fld_emissive_o),
    .req_valid_o(c_req_valid_o), .req_ready_i(c_req_ready_i),
    .req_slot_o(c_req_slot_o), .req_noprog_o(c_req_noprog_o), .req_in_o(c_req_in_o),
    .resp_valid_i(c_resp_valid_i), .resp_ready_o(c_resp_ready_o),
    .resp_out_i(c_resp_out_i), .resp_status_i(c_resp_status_i),
    .stamps_o(c_stamps_o), .texels_o(c_texels_o), .faults_o(c_faults_o),
    .restarts_o(c_restarts_o), .canon_clamps_o(c_canon_clamps_o), .busy_o(c_busy_o)
  );

endmodule

`default_nettype wire
