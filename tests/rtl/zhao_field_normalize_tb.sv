// zhao_field_normalize_tb.sv — TEST-ONLY harness wrapper. NEVER synthesis.
//
// WHY THIS FILE EXISTS. Under the DSP ruling of 2026-08-23 no production op unit
// in the Field engine keeps a private nonconstant multiplier: the whole engine
// shares ONE `zhao_field_mul`, and the shared resources live in
// `zhao_field_exec_shared`. That is the point of the rearchitecture, and it is
// also why `zhao_field_normalize` can no longer be elaborated on its own —
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
module zhao_field_normalize_tb (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic               is3_i,
    input  logic signed [31:0] a0_i,
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] o0_o,
    output logic signed [31:0] o1_o,
    output logic signed [31:0] o2_o,
    output logic               rcp0_o,
    output logic               sat_rescale_o
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

  logic        sqrt_valid, sqrt_ready, sqrt_rvalid, sqrt_rready;
  logic [63:0] sqrt_n, sqrt_r;

  zhao_field_isqrt u_isqrt (
      .clk      (clk),
      .rst_n    (rst_n),
      .n_valid_i(sqrt_valid),
      .n_ready_o(sqrt_ready),
      .n_i      (sqrt_n),
      .r_valid_o(sqrt_rvalid),
      .r_ready_i(sqrt_rready),
      .r_o      (sqrt_r)
  );

  zhao_field_normalize u_norm (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (v_valid_i),
      .v_ready_o     (v_ready_o),
      .is3_i         (is3_i),
      .a0_i          (a0_i),
      .a1_i          (a1_i),
      .a2_i          (a2_i),
      .r_valid_o     (r_valid_o),
      .r_ready_i     (r_ready_i),
      .o0_o          (o0_o),
      .o1_o          (o1_o),
      .o2_o          (o2_o),
      .rcp0_o        (rcp0_o),
      .sat_rescale_o (sat_rescale_o),
      .mul_issue_o   (mul_issue),
      .mul_a_o       (mul_a),
      .mul_b_o       (mul_b),
      .mul_p_i       (mul_p),
      .mul_valid_i   (mul_valid),
      .sqrt_valid_o  (sqrt_valid),
      .sqrt_ready_i  (sqrt_ready),
      .sqrt_n_o      (sqrt_n),
      .sqrt_rvalid_i (sqrt_rvalid),
      .sqrt_rready_o (sqrt_rready),
      .sqrt_r_i      (sqrt_r)
  );

endmodule : zhao_field_normalize_tb
