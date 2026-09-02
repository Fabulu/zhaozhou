// zhao_texture_bilerp_lane.sv — one bilinear channel job per clock.
//
// BESIDE `zhao_texture_bilerp.sv`, which stays the arithmetic of record and is
// instantiated here UNCHANGED for the combinational halves. Nothing uses this
// lane yet.
//
// ---------------------------------------------------------------------------
// WHY
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md:
//
//   > Retain one channel lane initially. The known workload contains roughly
//   > 180,000 filtered channel jobs versus more than half a million cache
//   > accesses, so four lanes would spend DSPs on a unit that is not presently
//   > the maximum term. The filter accepts one channel job per clock from a
//   > footprint FIFO, carries token and channel, and writes the ROB directly.
//
// ONE lane, deliberately. The temptation with a filter is to build four --
// one per channel -- and it would look faster on a diagram. The brief costs it
// out instead: 180,000 filtered jobs against 540,000+ cache accesses means the
// filter is not the maximum term, and four lanes would spend DSPs solving a
// problem the design does not have. That is the same discipline the raster
// pass learned when `gi * sx` inferred twelve DSPs and cost 18 MHz.
//
// ---------------------------------------------------------------------------
// WHAT CHANGES, AND WHAT MUST NOT
// ---------------------------------------------------------------------------
// `zhao_texture_bilerp` is purely combinational: two multiply levels in
// series, u-direction then v-direction. As one expression that is a long path
// and it filters one job at a time.
//
// This lane splits it at a register between the two levels. Three stages,
// initiation interval 1, and the arithmetic is BIT-IDENTICAL by construction
// rather than by assertion: `a_s` and `b_s` are EXACT 18-bit intermediates, so
// registering them cannot change a rounding. The only rounding in the whole
// function is the final `(s_w + 32768) >>> 16`, and that is untouched.
//
// The paired test instantiates the shipped combinational block alongside and
// compares every result, so "bit-identical by construction" is checked rather
// than believed.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_bilerp_lane #(
    parameter int unsigned TOKW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one channel job from the footprint FIFO ----------------------------
    input  var logic            job_valid_i,
    output var logic            job_ready_o,
    input  var logic [7:0]      t00_i,
    input  var logic [7:0]      t10_i,
    input  var logic [7:0]      t01_i,
    input  var logic [7:0]      t11_i,
    input  var logic [7:0]      fu_i,
    input  var logic [7:0]      fv_i,
    input  var logic [TOKW-1:0] tok_i,
    input  var logic [1:0]      chan_i,     // which channel this job is

    // ---- straight to the ROB ------------------------------------------------
    output var logic            out_valid_o,
    input  var logic            out_ready_i,
    output var logic [7:0]      out_o,
    output var logic [TOKW-1:0] out_tok_o,
    output var logic [1:0]      out_chan_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]     jobs_o,
    output var logic [1:0]      occupancy_o
);

  // ---- B0: accept -----------------------------------------------------------
  logic            b0_v_q;
  logic [7:0]      b0_t00_q, b0_t10_q, b0_t01_q, b0_t11_q, b0_fu_q, b0_fv_q;
  logic [TOKW-1:0] b0_tok_q;
  logic [1:0]      b0_ch_q;

  // ---- B1: the u-direction half, registered as EXACT intermediates ---------
  logic               b1_v_q;
  logic signed [17:0] b1_a_q, b1_b_q;
  logic [7:0]         b1_fv_q;
  logic [TOKW-1:0]    b1_tok_q;
  logic [1:0]         b1_ch_q;

  // ---- B2: the v-direction half and the single rounding --------------------
  logic            b2_v_q;
  logic [7:0]      b2_out_q;
  logic [TOKW-1:0] b2_tok_q;
  logic [1:0]      b2_ch_q;

  // Backwards ready, one stage at a time. Accepting a job depends on B0's
  // occupancy alone, never on out_ready_i directly.
  logic b0_rdy, b1_rdy, b2_rdy;
  assign b2_rdy = !b2_v_q || out_ready_i;
  assign b1_rdy = !b1_v_q || b2_rdy;
  assign b0_rdy = !b0_v_q || b1_rdy;
  assign job_ready_o = b0_rdy;

  // ---- u-direction, transcribed from zhao_texture_bilerp -------------------
  logic signed [8:0]  fu_s, du0, du1;
  logic signed [17:0] pu0, pu1, a_c, b_c;
  always_comb begin
    fu_s = $signed({1'b0, b0_fu_q});
    du0  = $signed({1'b0, b0_t10_q}) - $signed({1'b0, b0_t00_q});
    du1  = $signed({1'b0, b0_t11_q}) - $signed({1'b0, b0_t01_q});
    pu0  = du0 * fu_s;
    pu1  = du1 * fu_s;
    a_c  = $signed({2'b00, b0_t00_q, 8'd0}) + pu0;
    b_c  = $signed({2'b00, b0_t01_q, 8'd0}) + pu1;
  end

  // ---- v-direction and the ONE rounding ------------------------------------
  logic signed [8:0]  fv_s;
  logic signed [17:0] dv_c;
  logic signed [26:0] pv_c, a_ext_c, s_w_c;
  logic [7:0]         out_c;
  always_comb begin
    fv_s    = $signed({1'b0, b1_fv_q});
    dv_c    = b1_b_q - b1_a_q;
    pv_c    = 27'(dv_c * fv_s);
    a_ext_c = 27'(b1_a_q);
    s_w_c   = (a_ext_c <<< 8) + pv_c;
    out_c   = 8'((s_w_c + 27'sd32768) >>> 16);
  end

  assign out_valid_o = b2_v_q;
  assign out_o       = b2_out_q;
  assign out_tok_o   = b2_tok_q;
  assign out_chan_o  = b2_ch_q;

  always_comb occupancy_o = 2'(b0_v_q) + 2'(b1_v_q) + 2'(b2_v_q);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      b0_v_q <= 1'b0;
      b1_v_q <= 1'b0;
      b2_v_q <= 1'b0;
      jobs_o <= 32'd0;
    end else begin
      if (b0_rdy) begin
        b0_v_q <= job_valid_i;
        if (job_valid_i) begin
          b0_t00_q <= t00_i;  b0_t10_q <= t10_i;
          b0_t01_q <= t01_i;  b0_t11_q <= t11_i;
          b0_fu_q  <= fu_i;   b0_fv_q  <= fv_i;
          b0_tok_q <= tok_i;  b0_ch_q  <= chan_i;
          jobs_o   <= jobs_o + 32'd1;
        end
      end

      if (b1_rdy) begin
        b1_v_q <= b0_v_q;
        if (b0_v_q) begin
          b1_a_q   <= a_c;
          b1_b_q   <= b_c;
          b1_fv_q  <= b0_fv_q;
          b1_tok_q <= b0_tok_q;
          b1_ch_q  <= b0_ch_q;
        end
      end

      if (b2_rdy) begin
        b2_v_q <= b1_v_q;
        if (b1_v_q) begin
          b2_out_q <= out_c;
          b2_tok_q <= b1_tok_q;
          b2_ch_q  <= b1_ch_q;
        end
      end
    end
  end

endmodule : zhao_texture_bilerp_lane

`default_nettype wire
