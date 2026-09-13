// dual18_lane_collapse_mutant.sv -- DELIBERATELY COLLAPSED RESULT-LANE CONTROL.
// NOT SHIPPED AND NEVER PART OF A CORRECT-DESIGN SOURCE LIST.
//
// One real cyclonev_mac still receives all four operands.  Logical top outputs
// A and B are both deliberately driven directly from RESULTA, while RESULTB is
// routed directly to an independently observable wrong-sink signature output.
// No preserve attribute props up a fanout-free lane: a real map must retain both
// result lanes, and the post-map origin detector must reject logical output B.
// This renamed mutant cannot be elaborated accidentally as production RTL.

`default_nettype none

(* preserve_hierarchy *)
module dual18_lane_collapse_mutant (
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
    output wire      [35:0] resultb_wrong_sink_o,
    output var logic        valid_o,
    output var logic [31:0] tag_o
);

  (* preserve *) logic [17:0] ax_q, ay_q, bx_q, by_q;
  (* preserve *) logic        operand_valid_q;
  (* preserve *) logic [31:0] operand_tag_q;
  logic [35:0] prod_a_c, prod_b_wrong_sink_c;

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
      .resultb_o(prod_b_wrong_sink_c)
  );

  assign resulta_o            = prod_a_c;
  assign resultb_o            = prod_a_c;              // MUTATION: B collapses to A.
  assign resultb_wrong_sink_o = prod_b_wrong_sink_c;   // Keeps RESULTB independently live.

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
endmodule : dual18_lane_collapse_mutant

`default_nettype wire
