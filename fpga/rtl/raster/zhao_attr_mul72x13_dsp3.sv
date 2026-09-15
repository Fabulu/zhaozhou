// zhao_attr_mul72x13_dsp3.sv -- exact three-DSP signed72x13 worker.
//
// One fixed pipeline serves BASE_X, BASE_Y, and OFFSET micro-operations inside
// one attribute lane.  Three 27x27 leaves evaluate the low/middle/high pieces
// simultaneously; no opcode owns a private multiplier cone.
`default_nettype none

`ifndef ZHAO_ATTR_DSP3_MIDDLE_EXT
`define ZHAO_ATTR_DSP3_MIDDLE_EXT(value) $signed({3'b000, value[47:24]})
`endif
`ifndef ZHAO_ATTR_DSP3_RECOMBINE
`define ZHAO_ATTR_DSP3_RECOMBINE(s01, high) \
    ($signed({{23{s01[61]}}, s01}) + \
     ($signed({{48{high[36]}}, high}) <<< 48))
`endif

(* preserve_hierarchy *)
module zhao_attr_mul72x13_dsp3 (
    input  var logic               clk,
    input  var logic               rst_n,
    input  var logic               in_valid_i,
    input  var logic        [1:0]  in_op_i,
    input  var logic signed [71:0] in_a_i,
    input  var logic signed [12:0] in_b_i,
    output var logic               out_valid_o,
    output var logic        [1:0]  out_op_o,
    output var logic signed [84:0] out_product_o,
    output var logic               idle_o
);

  logic signed [26:0] a0_c, a1_c, ah_c, b_c;
  logic signed [53:0] p0_full_c, p1_full_c, ph_full_c;

  assign a0_c = $signed({3'b000, in_a_i[23:0]});
  assign a1_c = `ZHAO_ATTR_DSP3_MIDDLE_EXT(in_a_i);
  assign ah_c = $signed({{3{in_a_i[71]}}, in_a_i[71:48]});
  assign b_c  = $signed({{14{in_b_i[12]}}, in_b_i});

  zhao_mul27_exact u_p0 (.a_i(a0_c), .b_i(b_c), .p_o(p0_full_c));
  zhao_mul27_exact u_p1 (.a_i(a1_c), .b_i(b_c), .p_o(p1_full_c));
  zhao_mul27_exact u_ph (.a_i(ah_c), .b_i(b_c), .p_o(ph_full_c));

  logic v0_q, v1_q, v2_q;
  logic [1:0] op0_q, op1_q, op2_q;
  logic signed [37:0] p0_q, p1_q;
  logic signed [36:0] ph0_q, ph1_q;
  logic signed [61:0] s01_q;
  logic signed [84:0] product_q;
  logic signed [61:0] p0_ext_c, p1_ext_c;
  logic signed [84:0] product_c;

  always_comb begin
    p0_ext_c = {{24{p0_q[37]}}, p0_q};
    p1_ext_c = {{24{p1_q[37]}}, p1_q};
    product_c = `ZHAO_ATTR_DSP3_RECOMBINE(s01_q, ph1_q);
  end

  assign out_valid_o = v2_q;
  assign out_op_o = op2_q;
  assign out_product_o = product_q;
  assign idle_o = !(v0_q || v1_q || v2_q);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      v0_q <= 1'b0;
      v1_q <= 1'b0;
      v2_q <= 1'b0;
    end else begin
      v0_q <= in_valid_i;
      v1_q <= v0_q;
      v2_q <= v1_q;

      if (in_valid_i) begin
        p0_q  <= p0_full_c[37:0];
        p1_q  <= p1_full_c[37:0];
        ph0_q <= ph_full_c[36:0];
        op0_q <= in_op_i;
      end
      if (v0_q) begin
        s01_q <= p0_ext_c + (p1_ext_c <<< 24);
        ph1_q <= ph0_q;
        op1_q <= op0_q;
      end
      if (v1_q) begin
        product_q <= product_c;
        op2_q <= op1_q;
      end
    end
  end

  // synthesis translate_off
`ifndef ZHAO_ATTR_DSP3_MUTANT_DISABLE_ASSERTIONS
  always_ff @(posedge clk) begin
    if (rst_n && in_valid_i) begin
      a_p0_narrow_exact : assert (
          p0_full_c[53:38] == {16{p0_full_c[37]}});
      a_p1_narrow_exact : assert (
          p1_full_c[53:38] == {16{p1_full_c[37]}});
      a_ph_narrow_exact : assert (
          ph_full_c[53:37] == {17{ph_full_c[36]}});
    end
    if (rst_n && out_valid_o && (out_op_o == 2'd2)) begin
      a_offset_narrow_exact : assert (
          out_product_o[84:45] == {40{out_product_o[44]}});
    end
  end
`endif
  // synthesis translate_on

endmodule : zhao_attr_mul72x13_dsp3

`undef ZHAO_ATTR_DSP3_MIDDLE_EXT
`undef ZHAO_ATTR_DSP3_RECOMBINE
`default_nettype wire
