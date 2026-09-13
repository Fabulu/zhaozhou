// dual18_lane_swap_mutant.sv -- DELIBERATELY CROSSED RESULT-LANE CONTROL.
// NOT SHIPPED AND NEVER PART OF A CORRECT-DESIGN SOURCE LIST.
//
// All four operands and both physical result lanes remain live, but logical
// resulta_o is deliberately sourced by RESULTB and logical resultb_o by RESULTA.
// Only a bitwise mapped-cone check can distinguish this from the correct shape.

`default_nettype none

(* preserve_hierarchy *)
module dual18_lane_swap_mutant (
    input  var logic        clk_i,
    input  var logic        rst_i,
    input  var logic        ce_i,
    input  var logic        valid_i,
    input  var logic [31:0] tag_i,
    input  var logic [17:0] ax_i,
    input  var logic [17:0] ay_i,
    input  var logic [17:0] bx_i,
    input  var logic [17:0] by_i,
    output wire      [35:0] resulta_o,
    output wire      [35:0] resultb_o,
    output var logic        valid_o,
    output var logic [31:0] tag_o
);

  (* preserve *) logic [17:0] ax_q, ay_q, bx_q, by_q;
  (* preserve *) logic        operand_valid_q;
  (* preserve *) logic [31:0] operand_tag_q;
  logic [35:0] prod_a_c, prod_b_c;

  zhao_dual18_mul #(
      .AX_SIGNED(1'b0),
      .AY_SIGNED(1'b0),
      .BX_SIGNED(1'b0),
      .BY_SIGNED(1'b0)
  ) u_dual18 (
      .ax_i(ax_q),
      .ay_i(ay_q),
      .bx_i(bx_q),
      .by_i(by_q),
      .resulta_o(prod_a_c),
      .resultb_o(prod_b_c)
  );

  assign resulta_o = prod_b_c; // MUTATION: physical lane B crosses to A.
  assign resultb_o = prod_a_c; // MUTATION: physical lane A crosses to B.

  always_ff @(posedge clk_i) begin
    if (rst_i) begin
      ax_q            <= '0;
      ay_q            <= '0;
      bx_q            <= '0;
      by_q            <= '0;
      operand_valid_q <= 1'b0;
      operand_tag_q   <= '0;
      valid_o         <= 1'b0;
      tag_o           <= '0;
    end else if (ce_i) begin
      ax_q            <= ax_i;
      ay_q            <= ay_i;
      bx_q            <= bx_i;
      by_q            <= by_i;
      operand_valid_q <= valid_i;
      operand_tag_q   <= tag_i;
      valid_o         <= operand_valid_q;
      tag_o           <= operand_tag_q;
    end
  end
endmodule : dual18_lane_swap_mutant

`default_nettype wire
