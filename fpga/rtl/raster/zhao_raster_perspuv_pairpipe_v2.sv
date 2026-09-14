// zhao_raster_perspuv_pairpipe_v2.sv -- Packet-B observation-only successor
// to zhao_raster_perspuv_pairpipe.
//
// The paired arithmetic, credit ceiling, FIFO, held output, counters, and every
// ready/valid edge are unchanged.  idle_o is only a reduction of owned_q, which
// already counts every accepted pair until its external result acceptance.  It
// therefore covers all pipeline stages, terminal FIFO entries, and the held
// result without adding state or a ready path.
`default_nettype none

module zhao_raster_perspuv_pairpipe_v2 #(
    parameter int unsigned NTOK = 16,
    parameter int unsigned TAGW = 16
) (
    input var logic clk,
    input var logic rst_n,

    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] u_over_w_i,
    input  var logic signed [31:0] v_over_w_i,
    input  var logic        [23:0] r_mant_i,
    input  var logic        [ 5:0] r_k_i,
    input  var logic               depth_zero_i,
    input  var logic [TAGW-1:0]    tag_i,

    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] u_o,
    output var logic signed [31:0] v_o,
    output var logic [TAGW-1:0]    tag_o,
    output var logic               sat_o,
    output var logic               depth_zero_o,

    output var logic [31:0]        fragments_o,
    output var logic [31:0]        products_o,
    output var logic [31:0]        zero_products_o,
    output var logic [4:0]         occupancy_o,
    output var logic               idle_o
);

  localparam int unsigned CAP   = NTOK + 1;
  localparam int unsigned FIFOD = NTOK;
  localparam int unsigned FW    = $clog2(FIFOD);

  logic [4:0] owned_q;
  wire accept_c = v_valid_i && (owned_q < 5'(CAP));
  assign v_ready_o = (owned_q < 5'(CAP));
  assign occupancy_o = owned_q;
  assign idle_o = (owned_q == 5'd0);

  wire out_fire_c = r_valid_o && r_ready_i;

  logic               p0_v_q;
  logic signed [31:0] p0_nu_q, p0_nv_q;
  logic [23:0]        p0_mant_q;
  logic [5:0]         p0_k_q;
  logic [TAGW-1:0]    p0_tag_q;
  logic               p0_dz_q;

  logic               p1_v_q;
  logic signed [63:0] p1_pu_q, p1_pv_q;
  logic [5:0]         p1_k_q;
  logic [TAGW-1:0]    p1_tag_q;
  logic               p1_dz_q;

  logic               p2_v_q;
  logic signed [63:0] p2_su_q, p2_sv_q;
  logic [5:0]         p2_sh_q;
  logic [TAGW-1:0]    p2_tag_q;
  logic               p2_dz_q;

  logic               p3_v_q;
  logic signed [63:0] p3_ru_q, p3_rv_q;
  logic [TAGW-1:0]    p3_tag_q;
  logic               p3_dz_q;

  wire [5:0]         sh_c  = 6'd32 - p1_k_q;
  wire signed [63:0] rnd_c = $signed(64'd1 <<< (sh_c - 6'd1));
  wire signed [63:0] sum_u_c = $signed(p1_pu_q) + rnd_c;
  wire signed [63:0] sum_v_c = $signed(p1_pv_q) + rnd_c;

  wire signed [63:0] resc_u_c = $signed(p2_su_q) >>> p2_sh_q;
  wire signed [63:0] resc_v_c = $signed(p2_sv_q) >>> p2_sh_q;

  wire sat_u_c = (p3_ru_q > 64'sh0000_0000_7FFF_FFFF)
              || (p3_ru_q < -64'sh0000_0000_8000_0000);
  wire sat_v_c = (p3_rv_q > 64'sh0000_0000_7FFF_FFFF)
              || (p3_rv_q < -64'sh0000_0000_8000_0000);

  wire signed [31:0] q_u_c = sat_u_c ? (p3_ru_q[63] ? 32'sh8000_0000
                                                    : 32'sh7FFF_FFFF)
                                     : p3_ru_q[31:0];
  wire signed [31:0] q_v_c = sat_v_c ? (p3_rv_q[63] ? 32'sh8000_0000
                                                    : 32'sh7FFF_FFFF)
                                     : p3_rv_q[31:0];

  logic signed [31:0] fifo_u_q [FIFOD];
  logic signed [31:0] fifo_v_q [FIFOD];
  logic [TAGW-1:0]    fifo_tag_q [FIFOD];
  logic               fifo_sat_q [FIFOD];
  logic               fifo_dz_q [FIFOD];
  logic [FW:0]        fifo_wp_q, fifo_rp_q;

  wire fifo_empty_c = (fifo_wp_q == fifo_rp_q);

  logic               o_v_q;
  logic signed [31:0] o_u_q, o_v_val_q;
  logic [TAGW-1:0]    o_tag_q;
  logic               o_sat_q, o_dz_q;

  wire o_room_c = !o_v_q || r_ready_i;
  wire fifo_pop_c = o_room_c && !fifo_empty_c;

  assign r_valid_o    = o_v_q;
  assign u_o          = o_u_q;
  assign v_o          = o_v_val_q;
  assign tag_o        = o_tag_q;
  assign sat_o        = o_sat_q;
  assign depth_zero_o = o_dz_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      p0_v_q          <= 1'b0;
      p1_v_q          <= 1'b0;
      p2_v_q          <= 1'b0;
      p3_v_q          <= 1'b0;
      o_v_q           <= 1'b0;
      fifo_wp_q       <= '0;
      fifo_rp_q       <= '0;
      owned_q         <= 5'd0;
      fragments_o     <= 32'd0;
      products_o      <= 32'd0;
      zero_products_o <= 32'd0;
    end else begin
      owned_q <= owned_q + (accept_c ? 5'd1 : 5'd0) - (out_fire_c ? 5'd1 : 5'd0);

      p0_v_q <= accept_c;
      if (accept_c) begin
        p0_nu_q   <= u_over_w_i;
        p0_nv_q   <= v_over_w_i;
        p0_mant_q <= r_mant_i;
        p0_k_q    <= r_k_i;
        p0_tag_q  <= tag_i;
        p0_dz_q   <= depth_zero_i;
        fragments_o <= fragments_o + 32'd1;
        if (depth_zero_i) zero_products_o <= zero_products_o + 32'd1;
        else              products_o      <= products_o + 32'd2;
      end

      p1_v_q <= p0_v_q;
      if (p0_v_q) begin
        p1_pu_q  <= 64'(p0_nu_q) * $signed({40'd0, p0_mant_q});
        p1_pv_q  <= 64'(p0_nv_q) * $signed({40'd0, p0_mant_q});
        p1_k_q   <= p0_k_q;
        p1_tag_q <= p0_tag_q;
        p1_dz_q  <= p0_dz_q;
      end

      p2_v_q <= p1_v_q;
      if (p1_v_q) begin
        p2_su_q  <= sum_u_c;
        p2_sv_q  <= sum_v_c;
        p2_sh_q  <= sh_c;
        p2_tag_q <= p1_tag_q;
        p2_dz_q  <= p1_dz_q;
      end

      p3_v_q <= p2_v_q;
      if (p2_v_q) begin
        p3_ru_q  <= resc_u_c;
        p3_rv_q  <= resc_v_c;
        p3_tag_q <= p2_tag_q;
        p3_dz_q  <= p2_dz_q;
      end

      if (p3_v_q) begin
        fifo_u_q  [fifo_wp_q[FW-1:0]] <= p3_dz_q ? 32'sd0 : q_u_c;
        fifo_v_q  [fifo_wp_q[FW-1:0]] <= p3_dz_q ? 32'sd0 : q_v_c;
        fifo_tag_q[fifo_wp_q[FW-1:0]] <= p3_tag_q;
        fifo_sat_q[fifo_wp_q[FW-1:0]] <= !p3_dz_q && (sat_u_c || sat_v_c);
        fifo_dz_q [fifo_wp_q[FW-1:0]] <= p3_dz_q;
        fifo_wp_q <= fifo_wp_q + (FW+1)'(1);
      end

      if (o_room_c) begin
        o_v_q <= !fifo_empty_c;
        if (!fifo_empty_c) begin
          o_u_q     <= fifo_u_q  [fifo_rp_q[FW-1:0]];
          o_v_val_q <= fifo_v_q  [fifo_rp_q[FW-1:0]];
          o_tag_q   <= fifo_tag_q[fifo_rp_q[FW-1:0]];
          o_sat_q   <= fifo_sat_q[fifo_rp_q[FW-1:0]];
          o_dz_q    <= fifo_dz_q [fifo_rp_q[FW-1:0]];
        end
      end
      if (fifo_pop_c) fifo_rp_q <= fifo_rp_q + (FW+1)'(1);
    end
  end

endmodule : zhao_raster_perspuv_pairpipe_v2

`default_nettype wire
