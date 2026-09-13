// zhao_dual18_mul.sv -- exact combinational boundary for one Cyclone V
// variable-precision DSP block in two-independent-18x18 mode.
//
// This file is deliberately NOT selected by a default backend.  Calibration,
// reference simulation, and synthesis must each say which implementation they
// mean so an accidental behavioral synthesis cannot masquerade as packing.
//
// Exactly one of these macros is required:
//   ZHAO_DUAL18_BEHAVIORAL  portable bit-exact reference implementation
//   ZHAO_DUAL18_CYCLONEV    one direct Quartus-17 cyclonev_mac atom
//
// The boundary is stateless.  Callers own every clock, reset, enable, valid,
// tag, and pipeline decision.

`default_nettype none

(* preserve_hierarchy *)
module zhao_dual18_mul #(
    parameter bit AX_SIGNED = 1'b0,
    parameter bit AY_SIGNED = 1'b0,
    parameter bit BX_SIGNED = 1'b0,
    parameter bit BY_SIGNED = 1'b0
) (
    input  var logic [17:0] ax_i,
    input  var logic [17:0] ay_i,
    input  var logic [17:0] bx_i,
    input  var logic [17:0] by_i,
    output var logic [35:0] resulta_o,
    output var logic [35:0] resultb_o
);

`ifdef ZHAO_DUAL18_CYCLONEV
  `ifdef ZHAO_DUAL18_BEHAVIORAL
    // An unresolved module is intentional: conflicting backends must fail at
    // elaboration in both Quartus and Verilator rather than create a mux.
    zhao_dual18_backend_conflict MUST_NOT_ELABORATE();
  `endif

  // Keep the strings local and statically derived from the four elaboration-
  // time booleans.  Signedness is never transaction data.
  localparam string AX_SIGN_MODE = AX_SIGNED ? "true" : "false";
  localparam string AY_SIGN_MODE = AY_SIGNED ? "true" : "false";
  localparam string BX_SIGN_MODE = BX_SIGNED ? "true" : "false";
  localparam string BY_SIGN_MODE = BY_SIGNED ? "true" : "false";

  wire [0:0]  scanout_unused;
  wire [63:0] chainout_unused;
  wire        dftout_unused;

  // The parameter and port names below are from the installed Quartus Prime
  // Lite 17.0.2 cyclonev_mac declaration/XML, not from a newer online recipe.
  (* keep, preserve *)
  cyclonev_mac #(
      .ax_width(18),
      .ay_scan_in_width(18),
      .az_width(1),
      .bx_width(18),
      .by_width(18),
      .bz_width(1),
      .scan_out_width(1),
      .result_a_width(36),
      .result_b_width(36),
      .operation_mode("m18x18_full"),
      .mode_sub_location(0),
      .operand_source_max("input"),
      .operand_source_may("input"),
      .operand_source_mbx("input"),
      .operand_source_mby("input"),
      .preadder_subtract_a("false"),
      .preadder_subtract_b("false"),
      .signed_max(AX_SIGN_MODE),
      .signed_may(AY_SIGN_MODE),
      .signed_mbx(BX_SIGN_MODE),
      .signed_mby(BY_SIGN_MODE),
      .ay_use_scan_in("false"),
      .by_use_scan_in("false"),
      .delay_scan_out_ay("false"),
      .delay_scan_out_by("false"),
      .use_chainadder("false"),
      .enable_double_accum("false"),
      .load_const_value(6'b000000),
      .ax_clock("none"),
      .ay_scan_in_clock("none"),
      .az_clock("none"),
      .bx_clock("none"),
      .by_clock("none"),
      .bz_clock("none"),
      .coef_sel_a_clock("none"),
      .coef_sel_b_clock("none"),
      .sub_clock("none"),
      .negate_clock("none"),
      .accumulate_clock("none"),
      .load_const_clock("none"),
      .output_clock("none")
  ) u_dual18_mac (
      .ax(ax_i),
      .ay(ay_i),
      .az(1'b0),
      .coefsela(3'b000),
      .bx(bx_i),
      .by(by_i),
      .bz(1'b0),
      .coefselb(3'b000),
      .scanin(18'b0),
      .chainin(64'b0),
      .loadconst(1'b0),
      .accumulate(1'b0),
      .negate(1'b0),
      .sub(1'b0),
      .clk(3'b000),
      .ena(3'b111),
      .aclr(2'b00),
      .resulta(resulta_o),
      .resultb(resultb_o),
      .scanout(scanout_unused),
      .chainout(chainout_unused),
      .dftout(dftout_unused)
  );

`elsif ZHAO_DUAL18_BEHAVIORAL
  // Every physical 18-bit operand becomes one signed 19-bit mathematical
  // integer.  Unsigned operands get a leading zero; signed operands get their
  // sign bit.  The 19x19 expression is deliberately uniform across all four
  // static signedness combinations and does not rely on context casting.
  logic signed [18:0] ax_math_c;
  logic signed [18:0] ay_math_c;
  logic signed [18:0] bx_math_c;
  logic signed [18:0] by_math_c;
  logic signed [37:0] prod_a_c;
  logic signed [37:0] prod_b_c;

  generate
    if (AX_SIGNED) begin : g_ax_signed
      always_comb ax_math_c = $signed({ax_i[17], ax_i});
    end else begin : g_ax_unsigned
      always_comb ax_math_c = $signed({1'b0, ax_i});
    end
    if (AY_SIGNED) begin : g_ay_signed
      always_comb ay_math_c = $signed({ay_i[17], ay_i});
    end else begin : g_ay_unsigned
      always_comb ay_math_c = $signed({1'b0, ay_i});
    end
    if (BX_SIGNED) begin : g_bx_signed
      always_comb bx_math_c = $signed({bx_i[17], bx_i});
    end else begin : g_bx_unsigned
      always_comb bx_math_c = $signed({1'b0, bx_i});
    end
    if (BY_SIGNED) begin : g_by_signed
      always_comb by_math_c = $signed({by_i[17], by_i});
    end else begin : g_by_unsigned
      always_comb by_math_c = $signed({1'b0, by_i});
    end
  endgenerate

  always_comb begin
    prod_a_c = ax_math_c * ay_math_c;
    prod_b_c = bx_math_c * by_math_c;
    resulta_o = prod_a_c[35:0];
    resultb_o = prod_b_c[35:0];
  end

`ifndef SYNTHESIS
  // Legal 18-bit operands always fit the raw 36-bit product contract.  These
  // checks defend the truncation itself; the directed C++ oracle independently
  // checks every emitted bit.
  always_comb begin
    if (!$isunknown({ax_i, ay_i})) begin
      if (AX_SIGNED || AY_SIGNED)
        a_lane_a_fits_36 : assert (prod_a_c[37:36] == {2{prod_a_c[35]}});
      else
        a_lane_a_fits_36 : assert (prod_a_c[37:36] == 2'b00);
    end
    if (!$isunknown({bx_i, by_i})) begin
      if (BX_SIGNED || BY_SIGNED)
        a_lane_b_fits_36 : assert (prod_b_c[37:36] == {2{prod_b_c[35]}});
      else
        a_lane_b_fits_36 : assert (prod_b_c[37:36] == 2'b00);
    end
  end
`endif

`else
  // An unresolved module is intentional: a missing backend must fail at
  // elaboration rather than silently synthesize an inferred multiplier.
  zhao_dual18_backend_not_selected MUST_NOT_ELABORATE();
`endif

endmodule : zhao_dual18_mul

`default_nettype wire
