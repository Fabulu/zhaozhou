// dual18_ce_ignore_mutant.sv -- DELIBERATELY BROKEN CE-HOLD CONTROL.
// NOT SHIPPED AND NEVER PART OF A CORRECT-DESIGN SOURCE LIST.
//
// One substantive defect distinguishes this renamed copy from the correct
// explicit shell: tag_o advances from operand_tag_q even while ce_i is zero.
// Products, valids, operands, and the pending tag still hold.  The directed
// stall corpus changes external inputs and checks every output/tag, so its
// positive-control mode must observe this lone illegal advance.

`default_nettype none

module dual18_ce_ignore_mutant #(
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

  logic [17:0] ax_q, ay_q, bx_q, by_q;
  logic        operand_valid_q;
  logic [31:0] operand_tag_q;
  logic [35:0] prod_a_c, prod_b_c;

  zhao_dual18_mul #(
      .AX_SIGNED(AX_SIGNED),
      .AY_SIGNED(AY_SIGNED),
      .BX_SIGNED(BX_SIGNED),
      .BY_SIGNED(BY_SIGNED)
  ) u_dual18 (
      .ax_i(ax_q),
      .ay_i(ay_q),
      .bx_i(bx_q),
      .by_i(by_q),
      .resulta_o(prod_a_c),
      .resultb_o(prod_b_c)
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
    end else begin
      tag_o <= operand_tag_q;  // MUTANT: this assignment ignores ce_i.
      if (ce_i) begin
        ax_q            <= ax_i;
        ay_q            <= ay_i;
        bx_q            <= bx_i;
        by_q            <= by_i;
        operand_valid_q <= valid_i;
        operand_tag_q   <= tag_i;
        resulta_o       <= prod_a_c;
        resultb_o       <= prod_b_c;
        valid_o         <= operand_valid_q;
      end
    end
  end
endmodule : dual18_ce_ignore_mutant

`default_nettype wire
