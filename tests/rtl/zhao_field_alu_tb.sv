// zhao_field_alu_tb.sv — TEST-ONLY harness wrapper. NEVER synthesis.
//
// WHY THIS FILE EXISTS. Under the DSP ruling of 2026-08-23 no production op unit
// in the Field engine keeps a private nonconstant multiplier: the whole engine
// shares ONE `zhao_field_mul`, and the shared resources live in
// `zhao_field_exec_shared`. That is the point of the rearchitecture, and it is
// also why `zhao_field_alu` can no longer be elaborated on its own —
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
module zhao_field_alu_tb (
    input  logic        [ 7:0] op_i,
    input  logic        [31:0] imm_i,
    input  logic signed [31:0] a0_i,
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,
    input  logic signed [31:0] b0_i,
    input  logic signed [31:0] b1_i,
    input  logic signed [31:0] b2_i,
    input  logic signed [31:0] c_i,

    output logic signed [31:0] result_o,
    output logic               is_end_o,
    output logic               writes_o,
    output logic               op_unsupported_o,
    output logic               sat_add_o,
    output logic               sat_mul_o,
    output logic               sat_rescale_o
);

  // THE ONE RESTATEMENT IN THIS FILE, AND IT IS DELIBERATE. `zhao_field_alu` no
  // longer states that lane k of `a` multiplies lane k of `b`; in production
  // that pairing is the sequencer's read-address walk, `a + k` against `b + k`,
  // and it is proven end to end by field_seq_directed against `zfield::interpret`.
  // Here the pairing has to be restated to feed the block its products.
  //
  // That is acceptable ONLY because the production pairing is proven elsewhere
  // and by a different oracle. If this were the only place either version were
  // checked, the two could agree with each other and disagree with the software
  // forever, which is exactly the failure `abs` had.
  function automatic logic signed [65:0] ext32(input logic signed [31:0] v);
    ext32 = $signed({{34{v[31]}}, v});
  endfunction

  logic signed [65:0] prod_ab, dot2, dot3;
  assign prod_ab = ext32(a0_i) * ext32(b0_i);
  assign dot2    = prod_ab + ext32(a1_i) * ext32(b1_i);
  assign dot3    = dot2 + ext32(a2_i) * ext32(b2_i);

  zhao_field_alu u_alu (
      .op_i             (op_i),
      .imm_i            (imm_i),
      .a0_i             (a0_i),
      .a1_i             (a1_i),
      .a2_i             (a2_i),
      .b0_i             (b0_i),
      .b1_i             (b1_i),
      .b2_i             (b2_i),
      .c_i              (c_i),
      .prod_ab_i        (prod_ab),
      .dot2_i           (dot2),
      .dot3_i           (dot3),
      .result_o         (result_o),
      .is_end_o         (is_end_o),
      .writes_o         (writes_o),
      .op_unsupported_o (op_unsupported_o),
      .sat_add_o        (sat_add_o),
      .sat_mul_o        (sat_mul_o),
      .sat_rescale_o    (sat_rescale_o)
  );

endmodule : zhao_field_alu_tb
