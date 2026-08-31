// zhao_raster_toon.sv — RASTER.TOON: the cel band, applied to interpolated
// light without flattening its colour.
//
// ENFORCED-BY: tests/raster/raster_toon_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THE CREATURE ACTUALLY ASKS FOR
// ---------------------------------------------------------------------------
// Zixxtrixx's shipped presentation (`ZIXX_EXP=celmain`) is NOT faceted shading
// and NOT a per-pixel normal. It keeps coherent vertex normals, interpolates
// ordinary Gouraud RGB, and then quantises the INTERPOLATED light into three
// authored bands. That last step is the only thing the raster is missing, and
// it is this block.
//
//     thresholds  {43000, 57000}
//     levels      {28000, 50000, 82000}
//
// ---------------------------------------------------------------------------
// THE RATIO IS THE POINT
// ---------------------------------------------------------------------------
// A naive toon ramp replaces the light with the band value, giving three greys
// and throwing the Cool Cross rig away. The shipped law does not:
//
//     mean = (r + g + b) / 3
//     q    = mean < t0 ? l0 : (mean < t1 ? l1 : l2)
//     r' = r*q/mean,  g' = g*q/mean,  b' = b*q/mean
//
// so the BAND changes and the CHROMATIC RELATIONSHIP survives. The cool blue
// fill stays blue in shadow instead of becoming dark grey. That is why this
// block divides three times instead of muxing a constant.
//
// ---------------------------------------------------------------------------
// THE DIVISION IS TRUNCATING, AND THAT IS NOT THE DIVIDER WE ALREADY HAVE
// ---------------------------------------------------------------------------
// `rast.cpp` computes `(int64_t)r * q / mean` in C++, which truncates TOWARD
// ZERO. `zhao_raster_attrdiv` rounds half AWAY from zero. They are different
// laws and they differ on most non-exact quotients, so this block does its own
// division rather than reusing the attribute divider.
//
// `mean` is likewise `(r + g + b) / 3` with C++ truncation, and r/g/b are
// SIGNED — an unlit fragment can carry a negative lane — so the sign handling
// is part of the law, not a guard.
//
// `mean <= 0` short-circuits to a flat `q` on all three lanes. That is the one
// case where the ratio genuinely cannot be preserved, and the reference says
// so explicitly.
//
// ---------------------------------------------------------------------------
// ONE DIVIDE LANE, THREE JOBS, ONE FRAGMENT EVERY THREE CLOCKS
// ---------------------------------------------------------------------------
// A fully pipelined lane accepting one channel a clock retires a cel fragment
// every three:
//
//     1,666,667 / 3 = 555,555 cel fragments a frame
//
// against a 320,000 pre-Early-Z cross-mode stress profile — and only SURVIVING
// CEL fragments reach here at all. Terrain, sky and every non-cel object
// consume none of it. Three parallel dividers would triple the area to buy
// headroom on a resource that is already 1.7x over its own worst case.
//
// THE RATE, AND WHAT IT COST TO GET THERE. MEASURED, streamed:
//
//     4.11 clocks a cel fragment  ->  405,515 a frame, against 320,000
//
// It did not start there, and each step was a different mistake:
//
//   201 clocks   a 64-iteration sequential divider walked three times. The law
//                was right and the rate was 38x short. Correct first, fast
//                second -- a wrong fast block would have been worse.
//    39 clocks   the divider became a 32-stage pipelined array. But one
//                fragment was in flight at a time, so this measured its
//                LATENCY and called it the rate.
//  9.29 clocks   four slots, so fragments overlap. Still latency-bound: a slot
//                frees only when its fragment RETIRES, 32 stages later, so
//                four slots cap the rate at latency/4.
//  4.11 clocks   SIXTEEN slots, which covers 32/3 ~ 11 fragments in flight and
//                lets the three-clock issue be the limit.
//
// The lesson worth keeping is the second one: a block that issues one job and
// waits measures how long a job takes, not how many it can do. Only a streamed
// batch answers the question the frame budget asks.
//
// Every one of those four versions produced IDENTICAL attributes. The test
// compares against rast.cpp's own apply_toon_ramp and checks the streamed batch
// is exact AND in order, so the speed work could not quietly cost a band edge.
//
`default_nettype none

module zhao_raster_toon (
    input var logic clk,
    input var logic rst_n,

    // ---- the ramp, per material ---------------------------------------------
    // 0 disables the block entirely and the fragment passes through untouched,
    // which is what every non-cel material does.
    input var logic [1:0]         cfg_bands_i,
    input var logic signed [31:0] cfg_thr0_i,
    input var logic signed [31:0] cfg_thr1_i,
    input var logic signed [31:0] cfg_lvl0_i,
    input var logic signed [31:0] cfg_lvl1_i,
    input var logic signed [31:0] cfg_lvl2_i,

    // ---- one fragment's interpolated light ----------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] r_i,
    input  var logic signed [31:0] g_i,
    input  var logic signed [31:0] b_i,
    input  var logic        [15:0] tag_i,

    output logic               r_valid_o,
    input  var logic               r_ready_i,
    output logic signed [31:0] r_o,
    output logic signed [31:0] g_o,
    output logic signed [31:0] b_o,
    output logic        [15:0] tag_o,
    // The band this fragment landed in, 0..2. Not needed to shade, but it is
    // what a capture wants when a band boundary moves by one pixel.
    output logic        [1:0]  band_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] fragments_o,
    output var logic [31:0] flat_fragments_o,   // mean <= 0, the ratio-less case
    output var logic [31:0] busy_clocks_o,
    // A channel quotient did not fit 32 bits. Its lane is not usable, and
    // silence would look like an art bug rather than a refusal.
    output var logic [31:0] overflow_o
);



  // ---- the mean: (r + g + b) / 3, TRUNCATING toward zero -------------------
  // A 34-bit sum cannot overflow three 32-bit signed lanes. Division by three
  // is a real divide by a constant; it is done once per fragment, not per
  // channel, so a 34-bit restoring step would cost more than it saves. The
  // magnitude-then-sign form below is what makes it truncate rather than floor.
  logic signed [33:0] sum_c;
  logic [33:0]        sum_mag_c;
  // Only the low 31 bits are consumed: a mean of three 32-bit signed lanes
  // divided by three cannot exceed 2^31-1 in magnitude.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [33:0]        mean_mag_c;
  /* verilator lint_on UNUSEDSIGNAL */
  logic signed [31:0] mean_c;
  always_comb begin
    sum_c      = 34'(r_i) + 34'(g_i) + 34'(b_i);
    sum_mag_c  = sum_c[33] ? unsigned'(-sum_c) : unsigned'(sum_c);
    mean_mag_c = sum_mag_c / 34'd3;
    mean_c     = sum_c[33] ? 32'(-$signed({1'b0, mean_mag_c[30:0]}))
                           : 32'($signed({1'b0, mean_mag_c[30:0]}));
  end

  // ---- the band ------------------------------------------------------------
  logic [1:0]         band_c;
  logic signed [31:0] q_c;
  always_comb begin
    if (cfg_bands_i <= 2'd2) begin
      band_c = (mean_c < cfg_thr0_i) ? 2'd0 : 2'd1;
      q_c    = (mean_c < cfg_thr0_i) ? cfg_lvl0_i : cfg_lvl1_i;
    end else begin
      if (mean_c < cfg_thr0_i) begin
        band_c = 2'd0;
        q_c    = cfg_lvl0_i;
      end else if (mean_c < cfg_thr1_i) begin
        band_c = 2'd1;
        q_c    = cfg_lvl1_i;
      end else begin
        band_c = 2'd2;
        q_c    = cfg_lvl2_i;
      end
    end
  end

  // ---- the channel divide: lane * q / mean, TRUNCATING ---------------------
  // Magnitude then sign, which is what gives truncation toward zero.
  //
  // SEVERAL FRAGMENTS ARE IN FLIGHT AT ONCE, and that is the difference between
  // a block that works and one that ships. The array is 32 stages deep, so a
  // design that issued one fragment and waited measured 39 clocks a fragment --
  // the LATENCY, not the throughput, and 42,735 a frame. Carrying SLOTS lets
  // the next fragment enter while the previous one is still in the pipe, and
  // the cost falls to the three clocks its three channels actually occupy.
  //
  // SLOTS MUST COVER THE LATENCY, not merely exceed the channel count. With
  // four slots the block measured 9.29 clocks a fragment: issue costs three
  // clocks, but a slot is only freed when its fragment RETIRES, 32 stages
  // later, so four slots cap the rate at latency/4. Sixteen slots cover
  // 32 / 3 ~ 11 fragments in flight and let the three-clock issue be the limit.
  localparam int unsigned SLOTS = 16;
  localparam int unsigned SLOTW = 4;

  logic signed [31:0] sl_lane_r [SLOTS][3];
  logic signed [31:0] sl_q_r    [SLOTS];
  logic [1:0]         sl_band_r [SLOTS];
  logic [15:0]        sl_tag_r  [SLOTS];
  logic [1:0]         sl_got_r  [SLOTS];
  logic               sl_ovf_r  [SLOTS];
  logic               sl_flat_r [SLOTS];   // bypassed: no divide was issued
  logic [SLOTW-1:0]   wr_r, rd_r;
  logic [SLOTW:0]     inflight_r;

  logic full_c, empty_c;
  assign full_c  = (inflight_r == 5'd16);
  assign empty_c = (inflight_r == 5'd0);

  // ---- issue: one channel a clock -----------------------------------------
  logic [SLOTW-1:0] iss_slot_r;
  logic [1:0]       iss_ch_r;
  logic        issuing_r;
  logic [31:0] iss_den_r;

  logic signed [63:0] prod_c;
  logic [63:0]        prod_mag_c;
  logic               prod_neg_c;
  always_comb begin
    prod_c     = 64'(sl_lane_r[iss_slot_r][iss_ch_r]) * 64'(sl_q_r[iss_slot_r]);
    prod_neg_c = prod_c[63];
    prod_mag_c = prod_neg_c ? unsigned'(-prod_c) : unsigned'(prod_c);
  end

  logic       dq_valid, dq_neg, dq_ovf;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] dq_quo;   // bit 31 unread: a signed lane cannot hold 2^31
  /* verilator lint_on UNUSEDSIGNAL */
  logic [SLOTW+1:0] dq_tag;
  zhao_raster_toon_div #(.QBITS(32), .TAGW(SLOTW + 2)) u_div (
      .clk       (clk),
      .rst_n     (rst_n),
      .v_valid_i (issuing_r),
      .num_i     (prod_mag_c),
      .den_i     (iss_den_r),
      .neg_i     (prod_neg_c),
      .tag_i     ({iss_slot_r, iss_ch_r}),
      .r_valid_o (dq_valid),
      .quo_o     (dq_quo),
      .neg_o     (dq_neg),
      .overflow_o(dq_ovf),
      .tag_o     (dq_tag)
  );

  // A fragment is accepted whenever there is a slot AND the issuer is free.
  assign v_ready_o = !full_c && !issuing_r;

  // ---- retire, in slot order ----------------------------------------------
  logic head_done_c;
  assign head_done_c = !empty_c && (sl_flat_r[rd_r] || (sl_got_r[rd_r] == 2'd3));

  assign r_valid_o = head_done_c && !out_held_r;
  assign r_o       = sl_lane_r[rd_r][0];
  assign g_o       = sl_lane_r[rd_r][1];
  assign b_o       = sl_lane_r[rd_r][2];
  assign tag_o     = sl_tag_r[rd_r];
  assign band_o    = sl_band_r[rd_r];

  logic out_held_r;   // never set; the output is combinational from the head

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wr_r             <= SLOTW'(0);
      rd_r             <= SLOTW'(0);
      inflight_r       <= 5'd0;
      iss_slot_r       <= SLOTW'(0);
      iss_ch_r         <= 2'd0;
      issuing_r        <= 1'b0;
      iss_den_r        <= 32'd0;
      out_held_r       <= 1'b0;
      fragments_o      <= 32'd0;
      flat_fragments_o <= 32'd0;
      busy_clocks_o    <= 32'd0;
      overflow_o       <= 32'd0;
      for (int unsigned i = 0; i < SLOTS; ++i) begin
        sl_q_r[i]    <= 32'sd0;
        sl_band_r[i] <= 2'd0;
        sl_tag_r[i]  <= 16'd0;
        sl_got_r[i]  <= 2'd0;
        sl_ovf_r[i]  <= 1'b0;
        sl_flat_r[i] <= 1'b0;
        for (int unsigned c = 0; c < 3; ++c) sl_lane_r[i][c] <= 32'sd0;
      end
    end else begin
      if (!empty_c) busy_clocks_o <= busy_clocks_o + 32'd1;

      // ---- accept ---------------------------------------------------------
      if (v_valid_i && v_ready_o) begin
        sl_lane_r[wr_r][0] <= r_i;
        sl_lane_r[wr_r][1] <= g_i;
        sl_lane_r[wr_r][2] <= b_i;
        sl_tag_r[wr_r]     <= tag_i;
        sl_band_r[wr_r]    <= band_c;
        sl_q_r[wr_r]       <= q_c;
        sl_got_r[wr_r]     <= 2'd0;
        sl_ovf_r[wr_r]     <= 1'b0;
        wr_r               <= wr_r + SLOTW'(1);
        inflight_r         <= inflight_r + 5'd1;

        if (cfg_bands_i == 2'd0) begin
          // Not a cel material: the light passes through untouched, and no
          // divide is issued at all.
          sl_flat_r[wr_r] <= 1'b1;
          sl_band_r[wr_r] <= 2'd0;
        end else if (mean_c <= 32'sd0) begin
          // The reference's own short circuit: with no light to take a ratio
          // OF, all three lanes become the band value.
          sl_flat_r[wr_r]    <= 1'b1;
          sl_lane_r[wr_r][0] <= q_c;
          sl_lane_r[wr_r][1] <= q_c;
          sl_lane_r[wr_r][2] <= q_c;
          flat_fragments_o   <= flat_fragments_o + 32'd1;
        end else begin
          sl_flat_r[wr_r] <= 1'b0;
          issuing_r       <= 1'b1;
          iss_slot_r      <= wr_r;
          iss_ch_r        <= 2'd0;
          iss_den_r       <= unsigned'(mean_c);
        end
      end

      // ---- issue the three channels ---------------------------------------
      if (issuing_r) begin
        if (iss_ch_r == 2'd2) issuing_r <= 1'b0;
        else iss_ch_r <= iss_ch_r + 2'd1;
      end

      // ---- collect --------------------------------------------------------
      if (dq_valid) begin
        sl_lane_r[dq_tag[SLOTW+1:2]][dq_tag[1:0]] <=
            dq_neg ? 32'(-$signed({1'b0, dq_quo[30:0]}))
                   : 32'($signed({1'b0, dq_quo[30:0]}));
        sl_got_r[dq_tag[SLOTW+1:2]] <= sl_got_r[dq_tag[SLOTW+1:2]] + 2'd1;
        if (dq_ovf) sl_ovf_r[dq_tag[SLOTW+1:2]] <= 1'b1;
      end

      // ---- retire ---------------------------------------------------------
      if (r_valid_o && r_ready_i) begin
        rd_r        <= rd_r + SLOTW'(1);
        inflight_r  <= inflight_r - 5'd1 +
                       ((v_valid_i && v_ready_o) ? 5'd1 : 5'd0);
        fragments_o <= fragments_o + 32'd1;
        if (sl_ovf_r[rd_r]) overflow_o <= overflow_o + 32'd1;
      end
    end
  end

endmodule : zhao_raster_toon

`default_nettype wire
