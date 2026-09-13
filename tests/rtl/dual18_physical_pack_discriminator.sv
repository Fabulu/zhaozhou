// dual18_physical_pack_discriminator.sv -- functional and MapOnly shells for
// the explicit Cyclone V dual-18 packing calibration.
//
// The three mapping revisions select these tops independently:
//   dual18_inferred_pair
//   dual18_explicit_pair
//   dual18_s32x18_exact
//
// A fourth top, dual18_s32xu12_projector, is functional-only.  It exercises the
// viewport factorization named by the architecture without contaminating any
// MapOnly total.
//
// Every top has the same transaction law: synchronous active-high reset has
// priority over one common CE; an enabled edge accepts the new operand/valid/tag
// registers and emits the previous registered operands' result/valid/tag.  A
// disabled edge holds ALL state while external pins may continue to change.

`default_nettype none

/* verilator lint_off DECLFILENAME */

(* preserve_hierarchy *)
module dual18_inferred_pair #(
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

  // Keep the MapOnly default as two ordinary unsigned 18x18 expressions.  The
  // other static branches make the same shell useful for the complete signed
  // functional corpus; mixed sign needs a 19-bit mathematical representation
  // because unsigned 18-bit maximum is not representable as signed18.
  generate
    if (!AX_SIGNED && !AY_SIGNED) begin : g_auu
      always_comb prod_a_c = $unsigned(ax_q) * $unsigned(ay_q);
    end else if (AX_SIGNED && AY_SIGNED) begin : g_ass
      always_comb prod_a_c = $signed(ax_q) * $signed(ay_q);
    end else if (AX_SIGNED) begin : g_asu
      logic signed [18:0] x_c, y_c;
      always_comb begin
        x_c = $signed({ax_q[17], ax_q});
        y_c = $signed({1'b0, ay_q});
        prod_a_c = x_c * y_c;
      end
    end else begin : g_aus
      logic signed [18:0] x_c, y_c;
      always_comb begin
        x_c = $signed({1'b0, ax_q});
        y_c = $signed({ay_q[17], ay_q});
        prod_a_c = x_c * y_c;
      end
    end

    if (!BX_SIGNED && !BY_SIGNED) begin : g_buu
      always_comb prod_b_c = $unsigned(bx_q) * $unsigned(by_q);
    end else if (BX_SIGNED && BY_SIGNED) begin : g_bss
      always_comb prod_b_c = $signed(bx_q) * $signed(by_q);
    end else if (BX_SIGNED) begin : g_bsu
      logic signed [18:0] x_c, y_c;
      always_comb begin
        x_c = $signed({bx_q[17], bx_q});
        y_c = $signed({1'b0, by_q});
        prod_b_c = x_c * y_c;
      end
    end else begin : g_bus
      logic signed [18:0] x_c, y_c;
      always_comb begin
        x_c = $signed({1'b0, bx_q});
        y_c = $signed({by_q[17], by_q});
        prod_b_c = x_c * y_c;
      end
    end
  endgenerate

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
endmodule : dual18_inferred_pair


(* preserve_hierarchy *)
module dual18_explicit_pair #(
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
endmodule : dual18_explicit_pair


(* preserve_hierarchy *)
module dual18_s32x18_exact (
    input  var logic        clk_i,
    input  var logic        rst_i,
    input  var logic        ce_i,
    input  var logic        valid_i,
    input  var logic [31:0] tag_i,
    input  var logic [31:0] a_i,
    input  var logic [17:0] b_i,
    output var logic [49:0] result_o,
    output var logic        valid_o,
    output var logic [31:0] tag_o
);

  (* preserve *) logic [31:0] a_q;
  (* preserve *) logic [17:0] b_q;
  (* preserve *) logic        operand_valid_q;
  (* preserve *) logic [31:0] operand_tag_q;

  logic [17:0] lo_limb_c, hi_limb_c;
  logic [35:0] p_lo_c, p_hi_c;
  logic signed [49:0] p_lo_ext_c, p_hi_ext_c, product_c;

  always_comb begin
    lo_limb_c = {2'b00, a_q[15:0]};
    hi_limb_c = {{2{a_q[31]}}, a_q[31:16]};
    p_lo_ext_c = $signed({{14{p_lo_c[35]}}, p_lo_c});
    p_hi_ext_c = $signed({{14{p_hi_c[35]}}, p_hi_c});
    product_c = p_lo_ext_c + (p_hi_ext_c <<< 16);
  end

  zhao_dual18_mul #(
      .AX_SIGNED(1'b0),
      .AY_SIGNED(1'b1),
      .BX_SIGNED(1'b1),
      .BY_SIGNED(1'b1)
  ) u_dual18 (
      .ax_i(lo_limb_c),
      .ay_i(b_q),
      .bx_i(hi_limb_c),
      .by_i(b_q),
      .resulta_o(p_lo_c),
      .resultb_o(p_hi_c)
  );

  always_ff @(posedge clk_i) begin
    if (rst_i) begin
      a_q             <= '0;
      b_q             <= '0;
      operand_valid_q <= 1'b0;
      operand_tag_q   <= '0;
      result_o        <= '0;
      valid_o         <= 1'b0;
      tag_o           <= '0;
    end else if (ce_i) begin
      a_q             <= a_i;
      b_q             <= b_i;
      operand_valid_q <= valid_i;
      operand_tag_q   <= tag_i;
      result_o        <= product_c;
      valid_o         <= operand_valid_q;
      tag_o           <= operand_tag_q;
    end
  end
endmodule : dual18_s32x18_exact


// Functional-only check of N*V followed by the exact existing <<15.  No QSF
// generated by gen_calib.py names this top.
(* preserve_hierarchy *)
module dual18_s32xu12_projector (
    input  var logic        clk_i,
    input  var logic        rst_i,
    input  var logic        ce_i,
    input  var logic        valid_i,
    input  var logic [31:0] tag_i,
    input  var logic [31:0] n_i,
    input  var logic [11:0] viewport_i,
    output var logic [63:0] result_o,
    output var logic        valid_o,
    output var logic [31:0] tag_o
);

  (* preserve *) logic [31:0] n_q;
  (* preserve *) logic [11:0] viewport_q;
  (* preserve *) logic        operand_valid_q;
  (* preserve *) logic [31:0] operand_tag_q;

  logic [17:0] lo_limb_c, hi_limb_c, viewport18_c;
  logic [35:0] p_lo_c, p_hi_c;
  logic signed [43:0] p_lo_ext_c, p_hi_ext_c, product44_c;
  logic signed [63:0] product64_c;

  always_comb begin
    lo_limb_c = {2'b00, n_q[15:0]};
    hi_limb_c = {{2{n_q[31]}}, n_q[31:16]};
    viewport18_c = {6'b000000, viewport_q};
    p_lo_ext_c = $signed({8'b00000000, p_lo_c});
    p_hi_ext_c = $signed({{8{p_hi_c[35]}}, p_hi_c});
    product44_c = p_lo_ext_c + (p_hi_ext_c <<< 16);
    product64_c = $signed({{20{product44_c[43]}}, product44_c}) <<< 15;
  end

  zhao_dual18_mul #(
      .AX_SIGNED(1'b0),
      .AY_SIGNED(1'b0),
      .BX_SIGNED(1'b1),
      .BY_SIGNED(1'b0)
  ) u_dual18 (
      .ax_i(lo_limb_c),
      .ay_i(viewport18_c),
      .bx_i(hi_limb_c),
      .by_i(viewport18_c),
      .resulta_o(p_lo_c),
      .resultb_o(p_hi_c)
  );

  always_ff @(posedge clk_i) begin
    if (rst_i) begin
      n_q             <= '0;
      viewport_q      <= '0;
      operand_valid_q <= 1'b0;
      operand_tag_q   <= '0;
      result_o        <= '0;
      valid_o         <= 1'b0;
      tag_o           <= '0;
    end else if (ce_i) begin
      n_q             <= n_i;
      viewport_q      <= viewport_i;
      operand_valid_q <= valid_i;
      operand_tag_q   <= tag_i;
      result_o        <= product64_c;
      valid_o         <= operand_valid_q;
      tag_o           <= operand_tag_q;
    end
  end
endmodule : dual18_s32xu12_projector

/* verilator lint_on DECLFILENAME */

`default_nettype wire
