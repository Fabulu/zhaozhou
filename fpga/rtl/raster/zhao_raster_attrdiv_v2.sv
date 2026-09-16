// zhao_raster_attrdiv_v2.sv -- current rast.cpp signed attribute divide.
//
// q = sat_s32(floor((num + floor(area/2)) / area)), area > 0.
// Unlike the retained unversioned candidate, a negative exact half therefore
// rounds toward positive infinity.  Saturation is a valid oracle result and is
// reported separately from the terminal zero-area error. A dedicated preparation
// clock reuses the dividend bank to separate rounding from magnitude formation;
// it adds one clock without adding a second wide register bank.
//
// ENFORCED-BY: tests/raster/raster_attrgrad_v2_directed.cpp
`default_nettype none

// Test-only selector hooks are defined by the committed mutant source compiled
// immediately before this file.  Production gets the identity law below.
`ifndef ZHAO_ATTR_V2_ROUND_NUM
`define ZHAO_ATTR_V2_ROUND_NUM(num, area_ext) \
    ($signed(num) + ($signed(area_ext) >>> 1))
`endif

module zhao_raster_attrdiv_v2 #(
    parameter int unsigned RADIX = 2
) (
    input  var logic               clk,
    input  var logic               rst_n,

    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [95:0] num_i,
    input  var logic        [46:0] area_i,

    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] q_o,
    output var logic               q_saturated_o,
    output var logic               q_error_o,

    output var logic [31:0]        divides_o,
    output var logic [31:0]        saturations_o,
    output var logic [31:0]        errors_o,
    output var logic [31:0]        busy_clocks_o
);

  localparam int unsigned RBITS = (RADIX == 4) ? 2 : 1;
  // A leading zero makes the walk an even number of bits at radix four.
  localparam int unsigned QPOS  = 98;
  localparam int unsigned STEPS = QPOS / RBITS;

  localparam logic [1:0] D_IDLE = 2'd0;
  localparam logic [1:0] D_RUN  = 2'd1;
  localparam logic [1:0] D_DONE = 2'd2;
  localparam logic [1:0] D_PREP = 2'd3;

  logic [1:0] st_r;
  logic [6:0] iter_r;
  logic       neg_r;

  logic [97:0] dividend_r;
  logic [97:0] qmag_r;
  logic [48:0] rem_r;
  logic [46:0] den_r;

  logic signed [31:0] final_q_r;
  logic               final_sat_r;
  logic               final_err_r;

  logic signed [96:0] area_ext_c;
  logic signed [96:0] rounded_num_c;
  logic signed [96:0] pos_sat_limit_c;
  logic signed [96:0] neg_sat_limit_c;
  logic               sat_pos_c, sat_neg_c;
  logic        [97:0] magnitude_c;

  always_comb begin
    area_ext_c     = $signed({50'd0, area_i});
    // Exact law leaf.  The committed negative-half mutant selects only this
    // expression; it does not copy the divider or its transport.
    rounded_num_c  = `ZHAO_ATTR_V2_ROUND_NUM(num_i, area_ext_c);
    pos_sat_limit_c = area_ext_c <<< 31; // (INT32_MAX + 1) * area
    neg_sat_limit_c = -(area_ext_c <<< 31); // INT32_MIN * area
    sat_pos_c = (area_i != 47'd0) && (rounded_num_c >= pos_sat_limit_c);
    sat_neg_c = (area_i != 47'd0) && (rounded_num_c <  neg_sat_limit_c);
  end

  // The fitted G8A path formerly combined input rounding with this 98-bit
  // magnitude/bias formation on the dividend register's input. D_IDLE now
  // captures the signed rounded value in dividend_r; D_PREP reuses that same
  // bank to form the unsigned long-division dividend one clock later.
  // ONE 98-BIT CARRY CHAIN, NOT THREE. The negative branch used to materialize
  // a negate, then add the denominator, then subtract one -- three dependent
  // 98-bit additions in the D_PREP cone, which the Timing3 census reports as
  // the ATTR3/divider family (dividend_r[0] -> dividend_r[96]).
  //
  // The saving is an exact bit-vector identity, not an approximation. In two's
  // complement -x is (~x)+1, so modulo 2^98:
  //
  //     (-x + d - 1)  ==  ((~x) + 1 + d - 1)  ==  ((~x) + d)
  //
  // The explicit -1 is precisely what cancels the negate's carry-in, so the
  // complement can be added to the denominator directly. This holds for every
  // 98-bit x including the most negative value, where an explicit negate would
  // have wrapped anyway; the truncating modulo arithmetic is unchanged.
  //
  // Same D_PREP edge, same register bank, same state sequence, no extra clock
  // and no DSP. Do not "simplify" this back into a materialized negate.
  always_comb begin
    if (neg_r)
      magnitude_c = (~dividend_r) + 98'({51'd0, den_r});
    else
      magnitude_c = dividend_r;
  end

  assign v_ready_o = (st_r == D_IDLE) && !r_valid_o;

  logic [48:0] rem_shift_c;
  logic [48:0] d1_c, d2_c, d3_c;
  logic [1:0]  digit_c;
  logic [48:0] rem_next_c;
  logic [97:0] dividend_next_c;
  logic [97:0] qmag_next_c;

  always_comb begin
    d1_c = {2'd0, den_r};
    d2_c = d1_c << 1;
    d3_c = d1_c + d2_c;
    digit_c = 2'd0;

    if (RADIX == 4) begin
      rem_shift_c = (rem_r << 2) | 49'({47'd0, dividend_r[97:96]});
      if (rem_shift_c >= d3_c) begin
        digit_c    = 2'd3;
        rem_next_c = rem_shift_c - d3_c;
      end else if (rem_shift_c >= d2_c) begin
        digit_c    = 2'd2;
        rem_next_c = rem_shift_c - d2_c;
      end else if (rem_shift_c >= d1_c) begin
        digit_c    = 2'd1;
        rem_next_c = rem_shift_c - d1_c;
      end else begin
        rem_next_c = rem_shift_c;
      end
    end else begin
      rem_shift_c = (rem_r << 1) | 49'({48'd0, dividend_r[97]});
      if (rem_shift_c >= d1_c) begin
        digit_c    = 2'd1;
        rem_next_c = rem_shift_c - d1_c;
      end else begin
        rem_next_c = rem_shift_c;
      end
    end

    dividend_next_c = dividend_r << RBITS;
    qmag_next_c = (qmag_r << RBITS) | 98'(digit_c);
  end

  initial begin
    if ((RADIX != 2) && (RADIX != 4))
      $fatal(1, "zhao_raster_attrdiv_v2 RADIX must be 2 or 4");
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r           <= D_IDLE;
      iter_r         <= 7'd0;
      neg_r          <= 1'b0;
      dividend_r     <= 98'd0;
      qmag_r         <= 98'd0;
      rem_r          <= 49'd0;
      den_r          <= 47'd0;
      final_q_r      <= 32'sd0;
      final_sat_r    <= 1'b0;
      final_err_r    <= 1'b0;
      r_valid_o      <= 1'b0;
      q_o            <= 32'sd0;
      q_saturated_o  <= 1'b0;
      q_error_o      <= 1'b0;
      divides_o      <= 32'd0;
      saturations_o  <= 32'd0;
      errors_o       <= 32'd0;
      busy_clocks_o  <= 32'd0;
    end else begin
      if (st_r != D_IDLE) busy_clocks_o <= busy_clocks_o + 32'd1;

      case (st_r)
        D_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            // Reuse dividend_r as the preparation register: sign-extend the
            // exact rounded numerator now, then overwrite it with magnitude in
            // D_PREP. This avoids a second 97-bit bank in all three lanes.
            dividend_r   <= {rounded_num_c[96], rounded_num_c};
            neg_r         <= rounded_num_c[96];
            final_sat_r   <= sat_pos_c || sat_neg_c;
            final_err_r   <= (area_i == 47'd0);
            den_r         <= area_i;
            rem_r         <= 49'd0;
            qmag_r        <= 98'd0;
            st_r           <= D_PREP;
          end
        end

        D_PREP: begin
          if (final_err_r) begin
            final_q_r <= 32'sd0;
            st_r      <= D_DONE;
          end else if (final_sat_r && !neg_r) begin
            final_q_r <= 32'sh7fff_ffff;
            st_r      <= D_DONE;
          end else if (final_sat_r) begin
            final_q_r <= -32'sh8000_0000;
            st_r      <= D_DONE;
          end else begin
            // A negative floor is -ceil(abs/area), hence area-1.
            dividend_r <= magnitude_c;
            iter_r     <= 7'(STEPS);
            st_r       <= D_RUN;
          end
        end

        D_RUN: begin
          dividend_r <= dividend_next_c;
          qmag_r     <= qmag_next_c;
          rem_r      <= rem_next_c;
          if (iter_r == 7'd1) begin
            final_q_r <= neg_r
                ? 32'(-$signed({1'b0, qmag_next_c[31:0]}))
                : 32'($signed({1'b0, qmag_next_c[31:0]}));
            final_sat_r <= 1'b0;
            final_err_r <= 1'b0;
            st_r        <= D_DONE;
          end else begin
            iter_r <= iter_r - 7'd1;
          end
        end

        D_DONE: begin
          q_o           <= final_q_r;
          q_saturated_o <= final_sat_r;
          q_error_o     <= final_err_r;
          r_valid_o     <= 1'b1;
          divides_o     <= divides_o + 32'd1;
          if (final_sat_r) saturations_o <= saturations_o + 32'd1;
          if (final_err_r) errors_o <= errors_o + 32'd1;
          st_r <= D_IDLE;
        end

        default: st_r <= D_IDLE;
      endcase

      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
    end
  end

endmodule : zhao_raster_attrdiv_v2

`undef ZHAO_ATTR_V2_ROUND_NUM
`default_nettype wire
