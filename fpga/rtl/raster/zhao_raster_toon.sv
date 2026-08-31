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
// THE LANE HERE IS SEQUENTIAL, AND IT IS 38x TOO SLOW. MEASURED:
//
//     201 clocks a cel fragment  ->  8,291 a frame, against 320,000
//
// A 64-iteration restoring divider walked three times is ~195 clocks, and the
// arithmetic above assumed a PIPELINED lane taking one channel a clock. This
// implementation is correct and slow, deliberately: the law had to be pinned
// against rast.cpp before any effort went into speed, and a wrong fast block
// would have been worse than a right slow one.
//
// The block is therefore CORRECT AND NOT YET SHIPPABLE. What closes the gap,
// in the order worth trying:
//
//   1. Pipeline the lane -- one channel a clock, three clocks a fragment, which
//      is the 555,555 a frame the arithmetic above describes. This is what the
//      owner ruling actually specified and it is the expected answer.
//   2. Seed from the numerator's most significant bit instead of bit 63. The
//      shipped constants make |lane*q| about 34 bits, not 64, so most of the
//      iterations are shifting zeros.
//   3. Reciprocal-plus-correction: one reciprocal of `mean` shared by all three
//      channels, then a multiply and an exact correction step per lane. Cheaper
//      if the fit dislikes a 64-deep pipeline -- but it must stay differential
//      against apply_toon_ramp, because a reciprocal that is one LSB off moves
//      a band edge on the creature.
//
// `busy_clocks_o` and `fragments_o` are here so the improvement is measured
// rather than assumed.
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

    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] r_o,
    output var logic signed [31:0] g_o,
    output var logic signed [31:0] b_o,
    output var logic        [15:0] tag_o,
    // The band this fragment landed in, 0..2. Not needed to shade, but it is
    // what a capture wants when a band boundary moves by one pixel.
    output var logic        [1:0]  band_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] fragments_o,
    output var logic [31:0] flat_fragments_o,   // mean <= 0, the ratio-less case
    output var logic [31:0] busy_clocks_o
);

  localparam logic [2:0] T_IDLE = 3'd0;
  localparam logic [2:0] T_DIV  = 3'd1;
  localparam logic [2:0] T_NEXT = 3'd2;
  localparam logic [2:0] T_DONE = 3'd3;

  logic [2:0]         st_r;
  logic signed [31:0] lane_r [3];   // the three inputs, then the three results
  logic signed [31:0] q_r;
  logic [1:0]         band_r;
  logic [15:0]        tag_r;
  logic [1:0]         ch_r;         // which channel is dividing

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
  // Magnitude then sign, which is what gives truncation toward zero. The
  // product is 64 bits; the divisor is a positive mean (the mean <= 0 case
  // never reaches here). 64 restoring iterations, once per channel.
  logic [63:0] num_r;      // |lane * q|
  logic [63:0] rem_r;
  logic [63:0] quo_r;
  logic [31:0] den_r;      // |mean|, always > 0 on this path
  logic        neg_r;      // sign of the result
  logic [6:0]  iter_r;

  logic signed [63:0] prod_c;
  logic [63:0]        prod_mag_c;
  logic               prod_neg_c;
  always_comb begin
    prod_c     = 64'(lane_r[ch_r]) * 64'(q_r);
    prod_neg_c = prod_c[63];
    prod_mag_c = prod_neg_c ? unsigned'(-prod_c) : unsigned'(prod_c);
  end

  logic [63:0] rem_shift_c;
  logic        sub_ok_c;
  always_comb begin
    rem_shift_c = (rem_r << 1) | 64'(num_r[iter_r[5:0]]);
    sub_ok_c    = (rem_shift_c >= 64'({32'd0, den_r}));
  end

  assign v_ready_o = (st_r == T_IDLE) && !r_valid_o;
  assign band_o    = band_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r             <= T_IDLE;
      q_r              <= 32'sd0;
      band_r           <= 2'd0;
      tag_r            <= 16'd0;
      ch_r             <= 2'd0;
      num_r            <= 64'd0;
      rem_r            <= 64'd0;
      quo_r            <= 64'd0;
      den_r            <= 32'd0;
      neg_r            <= 1'b0;
      iter_r           <= 7'd0;
      r_valid_o        <= 1'b0;
      r_o              <= 32'sd0;
      g_o              <= 32'sd0;
      b_o              <= 32'sd0;
      tag_o            <= 16'd0;
      fragments_o      <= 32'd0;
      flat_fragments_o <= 32'd0;
      busy_clocks_o    <= 32'd0;
      for (int unsigned i = 0; i < 3; ++i) lane_r[i] <= 32'sd0;
    end else begin
      if (st_r != T_IDLE) busy_clocks_o <= busy_clocks_o + 32'd1;

      case (st_r)
        T_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            lane_r[0] <= r_i;
            lane_r[1] <= g_i;
            lane_r[2] <= b_i;
            tag_r     <= tag_i;
            q_r       <= q_c;
            band_r    <= band_c;
            ch_r      <= 2'd0;
            if (cfg_bands_i == 2'd0) begin
              // The ramp is off: this material is not cel and the fragment
              // passes through with its light untouched.
              r_o       <= r_i;
              g_o       <= g_i;
              b_o       <= b_i;
              tag_o     <= tag_i;
              band_r    <= 2'd0;
              r_valid_o <= 1'b1;
              fragments_o <= fragments_o + 32'd1;
            end else if (mean_c <= 32'sd0) begin
              // The reference's own short circuit: with no light to take a
              // ratio OF, all three lanes become the band value.
              r_o              <= q_c;
              g_o              <= q_c;
              b_o              <= q_c;
              tag_o            <= tag_i;
              r_valid_o        <= 1'b1;
              fragments_o      <= fragments_o + 32'd1;
              flat_fragments_o <= flat_fragments_o + 32'd1;
            end else begin
              den_r  <= unsigned'(mean_c);
              st_r   <= T_DIV;
              iter_r <= 7'd63;
              rem_r  <= 64'd0;
              quo_r  <= 64'd0;
              num_r  <= 64'd0;   // loaded in T_DIV's first pass below
            end
          end
        end

        T_DIV: begin
          // The first clock of each channel loads the product; the remaining
          // 64 walk it. `iter_r == 63` with a zero numerator is the load.
          if (num_r == 64'd0 && rem_r == 64'd0 && quo_r == 64'd0 && iter_r == 7'd63) begin
            num_r <= prod_mag_c;
            neg_r <= prod_neg_c;
          end else begin
            if (sub_ok_c) begin
              rem_r          <= rem_shift_c - 64'({32'd0, den_r});
              quo_r[iter_r[5:0]] <= 1'b1;
            end else begin
              rem_r <= rem_shift_c;
            end
            if (iter_r == 7'd0) st_r <= T_NEXT;
            else iter_r <= iter_r - 7'd1;
          end
        end

        T_NEXT: begin
          lane_r[ch_r] <= neg_r ? 32'(-$signed({1'b0, quo_r[30:0]}))
                                : 32'($signed({1'b0, quo_r[30:0]}));
          if (ch_r == 2'd2) begin
            st_r <= T_DONE;
          end else begin
            ch_r   <= ch_r + 2'd1;
            iter_r <= 7'd63;
            rem_r  <= 64'd0;
            quo_r  <= 64'd0;
            num_r  <= 64'd0;
            st_r   <= T_DIV;
          end
        end

        T_DONE: begin
          r_o         <= lane_r[0];
          g_o         <= lane_r[1];
          b_o         <= lane_r[2];
          tag_o       <= tag_r;
          r_valid_o   <= 1'b1;
          fragments_o <= fragments_o + 32'd1;
          st_r        <= T_IDLE;
        end

        default: st_r <= T_IDLE;
      endcase

      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
    end
  end

endmodule : zhao_raster_toon

`default_nettype wire
