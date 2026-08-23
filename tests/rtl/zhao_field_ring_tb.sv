// zhao_field_ring_tb.sv — TEST-ONLY harness wrapper. NEVER synthesis.
//
// WHY THIS FILE EXISTS. Under the DSP ruling of 2026-08-23 no production op unit
// in the Field engine keeps a private nonconstant multiplier: the whole engine
// shares ONE `zhao_field_mul`, and the shared resources live in
// `zhao_field_exec_shared`. That is the point of the rearchitecture, and it is
// also why `zhao_field_ring` can no longer be elaborated on its own —
// its arithmetic arrives through ports.
//
// So the block-level differential gets a harness that supplies exactly those
// shared resources and nothing else, and presents the op unit's ORIGINAL port
// list. The differential then keeps testing the same block against the same
// oracle, with the same per-lane saturation attribution it was written for.
//
// WHAT THIS FILE MUST NOT DO is restate any of the op's semantics. It contains
// no rounding, no saturation, no operand selection and no law — only the
// resources, wired straight through. The one exception is documented where it
// occurs, in zhao_field_alu_tb.sv.
//
// It is not in the production cone: nothing in fpga/rtl instantiates it, and it
// is listed only by the block's own test in tests/CMakeLists.txt.
module zhao_field_ring_tb (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic signed [31:0] d_i,
    input  logic signed [31:0] r0_i,
    input  logic signed [31:0] r1_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] result_o,
    output logic               sat_add_o,
    output logic               sat_mul_o,
    output logic               sat_rescale_o,
    output logic               sat_rcp_o,
    output logic               rcp0_o
);

  logic               mul_issue;
  logic signed [32:0] mul_a, mul_b;
  logic signed [65:0] mul_p;
  logic               mul_valid;

  zhao_field_mul u_mul (
      .clk      (clk),
      .rst_n    (rst_n),
      .issue_i  (mul_issue),
      .a_i      (mul_a),
      .b_i      (mul_b),
      .p_o      (mul_p),
      .p_valid_o(mul_valid)
  );

  // RING calls the shared reciprocal, which itself uses the shared lane. The
  // priority below is the same one `zhao_field_exec_shared` uses and for the
  // same reason: while RING waits on the reciprocal it issues nothing, so
  // there is exactly one requester at every instant.
  logic               rcp_mul_issue, rg_mul_issue;
  logic signed [32:0] rcp_mul_a, rcp_mul_b, rg_mul_a, rg_mul_b;

  always_comb begin
    if (rcp_mul_issue) begin
      mul_issue = 1'b1;
      mul_a     = rcp_mul_a;
      mul_b     = rcp_mul_b;
    end else begin
      mul_issue = rg_mul_issue;
      mul_a     = rg_mul_a;
      mul_b     = rg_mul_b;
    end
  end

  logic               rcp_valid, rcp_ready, rcp_rvalid, rcp_rready;
  logic signed [31:0] rcp_a, rcp_result;
  logic               rcp_sat, rcp_zero;

  zhao_field_rcp u_rcp (
      .clk        (clk),
      .rst_n      (rst_n),
      .v_valid_i  (rcp_valid),
      .v_ready_o  (rcp_ready),
      .a_i        (rcp_a),
      .r_valid_o  (rcp_rvalid),
      .r_ready_i  (rcp_rready),
      .result_o   (rcp_result),
      .sat_rcp_o  (rcp_sat),
      .rcp0_o     (rcp_zero),
      .mul_issue_o(rcp_mul_issue),
      .mul_a_o    (rcp_mul_a),
      .mul_b_o    (rcp_mul_b),
      .mul_p_i    (mul_p),
      .mul_valid_i(mul_valid)
  );

  zhao_field_ring u_ring (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (v_valid_i),
      .v_ready_o     (v_ready_o),
      .d_i           (d_i),
      .r0_i          (r0_i),
      .r1_i          (r1_i),
      .r_valid_o     (r_valid_o),
      .r_ready_i     (r_ready_i),
      .result_o      (result_o),
      .sat_add_o     (sat_add_o),
      .sat_mul_o     (sat_mul_o),
      .sat_rescale_o (sat_rescale_o),
      .sat_rcp_o     (sat_rcp_o),
      .rcp0_o        (rcp0_o),
      .rcp_valid_o   (rcp_valid),
      .rcp_ready_i   (rcp_ready),
      .rcp_a_o       (rcp_a),
      .rcp_rvalid_i  (rcp_rvalid),
      .rcp_rready_o  (rcp_rready),
      .rcp_result_i  (rcp_result),
      .rcp_sat_i     (rcp_sat),
      .rcp_zero_i    (rcp_zero),
      .mul_issue_o   (rg_mul_issue),
      .mul_a_o       (rg_mul_a),
      .mul_b_o       (rg_mul_b),
      .mul_p_i       (mul_p),
      .mul_valid_i   (mul_valid)
  );

endmodule : zhao_field_ring_tb
