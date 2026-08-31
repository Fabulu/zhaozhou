// zhao_raster_blend_prod -- one half of the shipping blend. See zhao_raster_blend.sv, which
// wires both halves together and is what the formal proof targets.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

// ---------------------------------------------------------------------------
// F2 -- THE PRODUCT HALF. One multiply, nothing else.
//
// `mul_left` selects the signed operand (ALPHA takes src-dst, every other mode
// takes src) and the product is formed at full 18-bit width. Deliberately NO
// rounding here: the +128 and the shift belong to the finish half, because
// splitting between the multiply and its rounding is what puts the DSP alone
// in one stage.
// ---------------------------------------------------------------------------
module zhao_raster_blend_prod (
  input  logic [1:0]          mode_i,
  input  logic [7:0]          dst_i,
  input  logic [7:0]          src_i,
  input  logic [7:0]          a_i,
  output logic signed [17:0]  prod_o
);

  localparam logic [1:0] BL_ALPHA = 2'd1;

  logic signed [17:0] mul_left, alpha_x;
  always_comb begin
    mul_left = (mode_i == BL_ALPHA)
                 ? ($signed({10'd0, src_i}) - $signed({10'd0, dst_i}))
                 : $signed({10'd0, src_i});
    alpha_x  = $signed({10'd0, a_i});
    prod_o   = mul_left * alpha_x;
  end

endmodule : zhao_raster_blend_prod
