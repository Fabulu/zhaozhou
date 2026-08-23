// zhao_field_rot_tb.sv — TEST-ONLY harness wrapper. NEVER synthesis.
//
// WHY THIS FILE EXISTS. Under the DSP ruling of 2026-08-23 no production op unit
// in the Field engine keeps a private nonconstant multiplier: the whole engine
// shares ONE `zhao_field_mul`, and the shared resources live in
// `zhao_field_exec_shared`. That is the point of the rearchitecture, and it is
// also why `zhao_field_rot` can no longer be elaborated on its own —
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
module zhao_field_rot_tb (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic               is_rot3_i,
    input  logic        [ 1:0] axis_i,
    input  logic signed [31:0] ang_i,
    input  logic signed [31:0] a0_i,
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] o0_o,
    output logic signed [31:0] o1_o,
    output logic signed [31:0] o2_o,
    output logic               sat_add_o,
    output logic               sat_mul_o
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

  logic        [15:0] sin_angle;
  logic               sin_is_cos;
  logic signed [31:0] sin_result;

  zhao_field_sin u_sin (
      .angle_i (sin_angle),
      .is_cos_i(sin_is_cos),
      .result_o(sin_result)
  );

  zhao_field_rot u_rot (
      .clk          (clk),
      .rst_n        (rst_n),
      .v_valid_i    (v_valid_i),
      .v_ready_o    (v_ready_o),
      .is_rot3_i    (is_rot3_i),
      .axis_i       (axis_i),
      .ang_i        (ang_i),
      .a0_i         (a0_i),
      .a1_i         (a1_i),
      .a2_i         (a2_i),
      .r_valid_o    (r_valid_o),
      .r_ready_i    (r_ready_i),
      .o0_o         (o0_o),
      .o1_o         (o1_o),
      .o2_o         (o2_o),
      .sat_add_o    (sat_add_o),
      .sat_mul_o    (sat_mul_o),
      .sin_angle_o  (sin_angle),
      .sin_is_cos_o (sin_is_cos),
      .sin_result_i (sin_result),
      .mul_issue_o  (mul_issue),
      .mul_a_o      (mul_a),
      .mul_b_o      (mul_b),
      .mul_p_i      (mul_p),
      .mul_valid_i  (mul_valid)
  );

endmodule : zhao_field_rot_tb
