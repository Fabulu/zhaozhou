// zhao_field_rcp_tb.sv — TEST-ONLY harness wrapper. NEVER synthesis.
//
// WHY THIS FILE EXISTS. Under the DSP ruling of 2026-08-23 no production op unit
// in the Field engine keeps a private nonconstant multiplier: the whole engine
// shares ONE `zhao_field_mul`, and the shared resources live in
// `zhao_field_exec_shared`. That is the point of the rearchitecture, and it is
// also why `zhao_field_rcp` can no longer be elaborated on its own —
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
module zhao_field_rcp_tb (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic signed [31:0] a_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] result_o,
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

  zhao_field_rcp u_rcp (
      .clk        (clk),
      .rst_n      (rst_n),
      .v_valid_i  (v_valid_i),
      .v_ready_o  (v_ready_o),
      .a_i        (a_i),
      .r_valid_o  (r_valid_o),
      .r_ready_i  (r_ready_i),
      .result_o   (result_o),
      .sat_rcp_o  (sat_rcp_o),
      .rcp0_o     (rcp0_o),
      .mul_issue_o(mul_issue),
      .mul_a_o    (mul_a),
      .mul_b_o    (mul_b),
      .mul_p_i    (mul_p),
      .mul_valid_i(mul_valid)
  );

endmodule : zhao_field_rcp_tb
