// zhao_texture_bilerp_lane_dsp2.sv -- exact two-DSP bilerp candidate.
//
// Public arithmetic and the B0/B1/B2 elastic schedule are cycle-identical to
// zhao_texture_bilerp_lane_v2.  Only B0's simultaneous horizontal products are
// packed into one all-signed zhao_dual18_mul; the vertical product remains in B1.
// This is an unselected DSPR2 candidate until mapping and connected-fit gates pass.
`default_nettype none

`ifndef ZHAO_BIL2_RESULTB
`define ZHAO_BIL2_RESULTB(resulta, resultb) resultb
`endif

(* preserve_hierarchy *)
module zhao_texture_bilerp_lane_dsp2 #(
    parameter int unsigned TOKW = 18
) (
    input  var logic             clk,
    input  var logic             rst_n,

    input  var logic             job_valid_i,
    output var logic             job_ready_o,
    input  var logic [7:0]       t00_i,
    input  var logic [7:0]       t10_i,
    input  var logic [7:0]       t01_i,
    input  var logic [7:0]       t11_i,
    input  var logic [7:0]       fu_i,
    input  var logic [7:0]       fv_i,
    input  var logic [TOKW-1:0]  tok_i,
    input  var logic [1:0]       chan_i,

    output var logic             out_valid_o,
    input  var logic             out_ready_i,
    output var logic [7:0]       out_o,
    output var logic [TOKW-1:0]  out_tok_o,
    output var logic [1:0]       out_chan_o,

    output var logic             idle_o,
    output var logic [31:0]      jobs_o,
    output var logic [1:0]       occupancy_o
);

  // B0: accepted texels and fractions.
  logic            b0_valid_q;
  logic [7:0]      b0_t00_q;
  logic [7:0]      b0_t10_q;
  logic [7:0]      b0_t01_q;
  logic [7:0]      b0_t11_q;
  logic [7:0]      b0_fu_q;
  logic [7:0]      b0_fv_q;
  logic [TOKW-1:0] b0_tok_q;
  logic [1:0]      b0_chan_q;

  // B1: exact U-direction intermediates; no rounding has occurred.
  logic               b1_valid_q;
  logic signed [17:0] b1_a_q;
  logic signed [17:0] b1_b_q;
  logic [7:0]         b1_fv_q;
  logic [TOKW-1:0]    b1_tok_q;
  logic [1:0]         b1_chan_q;

  // B2: held terminal byte.
  logic            b2_valid_q;
  logic [7:0]      b2_out_q;
  logic [TOKW-1:0] b2_tok_q;
  logic [1:0]      b2_chan_q;

  logic b0_ready_c;
  logic b1_ready_c;
  logic b2_ready_c;

  always_comb begin
    b2_ready_c = !b2_valid_q || out_ready_i;
    b1_ready_c = !b1_valid_q || b2_ready_c;
    b0_ready_c = !b0_valid_q || b1_ready_c;

    job_ready_o = b0_ready_c;
    out_valid_o = b2_valid_q;
    out_o       = b2_out_q;
    out_tok_o   = b2_tok_q;
    out_chan_o  = b2_chan_q;
    occupancy_o = 2'(b0_valid_q) + 2'(b1_valid_q) + 2'(b2_valid_q);
    idle_o      = !b0_valid_q && !b1_valid_q && !b2_valid_q;
  end

  logic signed [8:0]  du0_c;
  logic signed [8:0]  du1_c;
  logic signed [17:0] du0_18_c;
  logic signed [17:0] du1_18_c;
  logic signed [17:0] fu_18_c;
  logic        [35:0] pu0_raw_c;
  logic        [35:0] pu1_raw_c;
  logic        [35:0] pu1_selected_c;
  logic signed [17:0] pu0_c;
  logic signed [17:0] pu1_c;
  logic signed [17:0] a_c;
  logic signed [17:0] b_c;

  always_comb begin
    du0_c = $signed({1'b0, b0_t10_q}) - $signed({1'b0, b0_t00_q});
    du1_c = $signed({1'b0, b0_t11_q}) - $signed({1'b0, b0_t01_q});
    du0_18_c = $signed({{9{du0_c[8]}}, du0_c});
    du1_18_c = $signed({{9{du1_c[8]}}, du1_c});
    fu_18_c  = $signed({10'b0, b0_fu_q});
    pu1_selected_c = `ZHAO_BIL2_RESULTB(pu0_raw_c, pu1_raw_c);
    pu0_c = $signed(pu0_raw_c[17:0]);
    pu1_c = $signed(pu1_selected_c[17:0]);
    a_c = $signed({2'b00, b0_t00_q, 8'd0}) + pu0_c;
    b_c = $signed({2'b00, b0_t01_q, 8'd0}) + pu1_c;
  end

  // One physical dual18 target, two independent live horizontal products.
  zhao_dual18_mul #(
      .AX_SIGNED(1'b1), .AY_SIGNED(1'b1),
      .BX_SIGNED(1'b1), .BY_SIGNED(1'b1)
  ) u_horizontal_pair (
      .ax_i(du0_18_c), .ay_i(fu_18_c),
      .bx_i(du1_18_c), .by_i(fu_18_c),
      .resulta_o(pu0_raw_c), .resultb_o(pu1_raw_c)
  );

  logic signed [8:0]  fv_s_c;
  logic signed [17:0] dv_c;
  logic signed [26:0] pv_c;
  logic signed [26:0] a_ext_c;
  logic signed [26:0] sum_c;
  logic        [7:0]  filtered_c;

  always_comb begin
    fv_s_c     = $signed({1'b0, b1_fv_q});
    dv_c       = b1_b_q - b1_a_q;
    pv_c       = 27'(dv_c * fv_s_c);
    a_ext_c    = 27'(b1_a_q);
    sum_c      = (a_ext_c <<< 8) + pv_c;
    filtered_c = 8'((sum_c + 27'sd32768) >>> 16);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      b0_valid_q <= 1'b0;
      b1_valid_q <= 1'b0;
      b2_valid_q <= 1'b0;
      jobs_o     <= 32'd0;
    end else begin
      if (b0_ready_c) begin
        b0_valid_q <= job_valid_i;
        if (job_valid_i) begin
          b0_t00_q  <= t00_i;
          b0_t10_q  <= t10_i;
          b0_t01_q  <= t01_i;
          b0_t11_q  <= t11_i;
          b0_fu_q   <= fu_i;
          b0_fv_q   <= fv_i;
          b0_tok_q  <= tok_i;
          b0_chan_q <= chan_i;
          jobs_o    <= jobs_o + 32'd1;
        end
      end

      if (b1_ready_c) begin
        b1_valid_q <= b0_valid_q;
        if (b0_valid_q) begin
          b1_a_q    <= a_c;
          b1_b_q    <= b_c;
          b1_fv_q   <= b0_fv_q;
          b1_tok_q  <= b0_tok_q;
          b1_chan_q <= b0_chan_q;
        end
      end

      if (b2_ready_c) begin
        b2_valid_q <= b1_valid_q;
        if (b1_valid_q) begin
          b2_out_q  <= filtered_c;
          b2_tok_q  <= b1_tok_q;
          b2_chan_q <= b1_chan_q;
        end
      end
    end
  end

`ifndef SYNTHESIS
  // The reachable signed9-by-nonnegative9 products fit signed18 exactly.
  always_comb begin
    if (b0_valid_q && !$isunknown({pu0_raw_c, pu1_raw_c})) begin
      a_pu0_fits_s18 : assert (pu0_raw_c[35:18] == {18{pu0_raw_c[17]}});
      a_pu1_fits_s18 : assert (pu1_raw_c[35:18] == {18{pu1_raw_c[17]}});
    end
  end
`endif

endmodule : zhao_texture_bilerp_lane_dsp2

`undef ZHAO_BIL2_RESULTB
`default_nettype wire
