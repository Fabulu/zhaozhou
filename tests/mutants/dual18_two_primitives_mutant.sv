// dual18_two_primitives_mutant.sv -- DELIBERATELY UNPACKED MAP CONTROL.
// NOT SHIPPED AND NEVER PART OF A CORRECT-DESIGN SOURCE LIST.
//
// The correct explicit pair uses both live lanes of one zhao_dual18_mul.  This
// renamed mutant computes the same two logical products with TWO wrappers,
// using only lane A of each.  A real MapOnly report must therefore say two DSP
// blocks, and check_dual18_map.py's one-block detector must reject it.  The
// positive-control driver passes only after observing that rejection.

`default_nettype none

(* preserve_hierarchy *)
module dual18_two_primitives_mutant #(
    parameter bit AX_SIGNED = 1'b0,
    parameter bit AY_SIGNED = 1'b0,
    parameter bit BX_SIGNED = 1'b0,
    parameter bit BY_SIGNED = 1'b0
) (
    input  var logic        clk_i,
    input  var logic        rst_i,
    input  var logic        ce_i,
    input  var logic        valid_i,
    input  var logic [31:0] tag_i,
    input  var logic [17:0] ax_i,
    input  var logic [17:0] ay_i,
    input  var logic [17:0] bx_i,
    input  var logic [17:0] by_i,
    output var logic [35:0] resulta_o,
    output var logic [35:0] resultb_o,
    output var logic        valid_o,
    output var logic [31:0] tag_o
);

  (* preserve *) logic [17:0] ax_q, ay_q, bx_q, by_q;
  (* preserve *) logic        operand_valid_q;
  (* preserve *) logic [31:0] operand_tag_q;
  logic [35:0] prod_a_c, prod_b_c;
  wire [35:0] lane_a_unused_c, lane_b_unused_c;

  zhao_dual18_mul #(
      .AX_SIGNED(AX_SIGNED),
      .AY_SIGNED(AY_SIGNED),
      .BX_SIGNED(1'b0),
      .BY_SIGNED(1'b0)
  ) u_lane_a (
      .ax_i(ax_q),
      .ay_i(ay_q),
      .bx_i(18'b0),
      .by_i(18'b0),
      .resulta_o(prod_a_c),
      .resultb_o(lane_a_unused_c)
  );

  zhao_dual18_mul #(
      .AX_SIGNED(BX_SIGNED),
      .AY_SIGNED(BY_SIGNED),
      .BX_SIGNED(1'b0),
      .BY_SIGNED(1'b0)
  ) u_lane_b (
      .ax_i(bx_q),
      .ay_i(by_q),
      .bx_i(18'b0),
      .by_i(18'b0),
      .resulta_o(prod_b_c),
      .resultb_o(lane_b_unused_c)
  );

  always_ff @(posedge clk_i) begin
    if (rst_i) begin
      ax_q            <= '0;
      ay_q            <= '0;
      bx_q            <= '0;
      by_q            <= '0;
      operand_valid_q <= 1'b0;
      operand_tag_q   <= '0;
      resulta_o       <= '0;
      resultb_o       <= '0;
      valid_o         <= 1'b0;
      tag_o           <= '0;
    end else if (ce_i) begin
      ax_q            <= ax_i;
      ay_q            <= ay_i;
      bx_q            <= bx_i;
      by_q            <= by_i;
      operand_valid_q <= valid_i;
      operand_tag_q   <= tag_i;
      resulta_o       <= prod_a_c;
      resultb_o       <= prod_b_c;
      valid_o         <= operand_valid_q;
      tag_o           <= operand_tag_q;
    end
  end
endmodule : dual18_two_primitives_mutant

`default_nettype wire
