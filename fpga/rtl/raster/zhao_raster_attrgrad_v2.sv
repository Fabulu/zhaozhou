// zhao_raster_attrgrad_v2.sv -- one current-oracle attribute plane per tile.
//
// One divide establishes grad_x.  Each accepted coverage row evaluates the
// plane at the GLOBAL scissored min_x pixel centre and divides once there.
// The result is then advanced into this tile by grad_x*(tile_x-min_x), and the
// signed 32-bit quotient is stepped (wrapping in two's complement) across the
// row. The existing global-to-tile product is captured with grad_x so row-divide
// return performs only the wrapping seed addition. A fresh divide at the tile
// edge is deliberately forbidden.
//
// ENFORCED-BY: tests/raster/raster_attrgrad_v2_directed.cpp
`default_nettype none

`ifndef ZHAO_ATTR_V2_TILE_SEED
`define ZHAO_ATTR_V2_TILE_SEED(row_q, tile_offset) \
    ($signed(row_q) + $signed(tile_offset))
`endif

module zhao_raster_attrgrad_v2 #(
    parameter int unsigned RADIX = 2
) (
    input  var logic               clk,
    input  var logic               rst_n,

    input  var logic               job_valid_i,
    output var logic               job_ready_o,
    input  var logic signed [95:0] job_n0_i,
    input  var logic signed [71:0] job_dndx_i,
    input  var logic signed [71:0] job_dndy_i,
    input  var logic        [46:0] job_area2_i,
    input  var logic signed [11:0] job_min_x_i,
    input  var logic signed [11:0] job_tile_x_i,
    input  var logic signed [11:0] job_tile_y_i,

    input  var logic               cov_valid_i,
    output var logic               cov_ready_o,
    input  var logic        [3:0]  cov_row_i,
    input  var logic        [15:0] cov_mask_i,
    input  var logic               cov_last_i,

    output var logic               q_valid_o,
    input  var logic               q_ready_i,
    output var logic signed [31:0] q_o,
    output var logic        [3:0]  q_row_o,
    output var logic        [3:0]  q_col_o,
    output var logic               q_last_o,
    output var logic               q_saturated_o,
    output var logic               q_error_o,

    output var logic               idle_o,
    output var logic [31:0]        pixels_o,
    output var logic [31:0]        divides_o,
    output var logic [31:0]        saturations_o,
    output var logic [31:0]        divide_errors_o
);

  localparam logic [2:0] S_IDLE      = 3'd0;
  localparam logic [2:0] S_GRAD_REQ  = 3'd1;
  localparam logic [2:0] S_GRAD_WAIT = 3'd2;
  localparam logic [2:0] S_ROW       = 3'd3;
  localparam logic [2:0] S_ROW_REQ   = 3'd4;
  localparam logic [2:0] S_ROW_WAIT  = 3'd5;
  localparam logic [2:0] S_WALK      = 3'd6;

  logic [2:0] st_r;

  logic signed [95:0] dndx_r, dndy_r;
  logic signed [95:0] base_min_y0_r;
  logic        [46:0] area2_r;
  logic signed [12:0] tile_delta_r;

  logic signed [31:0] grad_x_r;
  logic               grad_sat_r, grad_err_r;
  logic signed [31:0] walk_q_r;
  logic               row_sat_r, row_err_r;
  logic        [15:0] mask_r;
  logic        [3:0]  row_r, col_r;
  logic               last_row_r;

  // Pixel-centre plane value at (global min_x, tile_y).  This is the same
  // origin-anchored convention as the established attribute plane path.
  //
  // THE TWO MULTIPLIES ARE ON THEIR OWN EDGE, AND IT COSTS NOTHING.
  //
  // This was one expression evaluated combinationally and registered into
  // `base_min_y0_r` on job acceptance. The 2026-09-18 `@packet-h-uvw` fit made
  // it the machine's binding path: `Mult0~mult_h_mult_hlmac`, a DSP macro
  // feeding a long soft carry chain for the partial-product sum, **14.24 ns of
  // a 15.67 ns path** against a 10.000 ns period, for -5.144 ns of slack.
  //
  // `base_min_y0_r` is written on job acceptance in S_IDLE and is not read
  // until S_ROW_REQ -- through S_GRAD_REQ, S_GRAD_WAIT and the gradient divide,
  // which alone spends about fifty clocks in `zhao_raster_attrdiv_v2`'s D_RUN.
  // The value has enormous schedule slack, so producing it one edge later adds
  // no state, spends no cycle, and moves no output edge. The module's observable
  // behaviour is identical, which is also why `raster_attrgrad_dsp3_diff` --
  // which compares this module against `zhao_raster_attrgrad_dsp3` cycle by
  // cycle -- still holds.
  //
  // Bit-exactness: every operand is 96-bit and the result truncates to 96, so
  // holding each product in a 96-bit register preserves the arithmetic exactly
  // modulo 2**96. Nothing is rounded, widened or reassociated across the split.
  logic signed [95:0] dndx_in_c, dndy_in_c;
  logic signed [95:0] mul_x_c, mul_y_c;
  logic signed [95:0] mul_x_r, mul_y_r, n0_r;
  logic signed [95:0] base_min_y0_c;
  logic signed [12:0] tile_delta_c;
  always_comb begin
    dndx_in_c = 96'(job_dndx_i);
    dndy_in_c = 96'(job_dndy_i);
    // Edge one: the multiplies alone, straight off the job inputs.
    mul_x_c = dndx_in_c * 96'(job_min_x_i);
    mul_y_c = dndy_in_c * 96'(job_tile_y_i);
    // Edge two: the sum, entirely from registers.
    base_min_y0_c = n0_r + mul_x_r + mul_y_r
                  + (dndx_r >>> 1)
                  + (dndy_r >>> 1);
    tile_delta_c = $signed({job_tile_x_i[11], job_tile_x_i})
                 - $signed({job_min_x_i[11], job_min_x_i});
  end

  // `row_r * dndy_r` as a shift-and-add, kept out of the DSPs on purpose.
  //
  // WHY THE TERMS ARE SUMMED PAIRWISE RATHER THAN ACCUMULATED. Written as four
  // sequential `row_offset_c = row_offset_c + ...` statements this is a
  // dependency CHAIN: each add waits for the one above it, and the fifth add
  // for `row_num_c` waits for all four. Quartus builds it as written, so the
  // 2026-09-18 composed-shell fit measured five 96-bit carry chains back to
  // back and this became THE path that sets the whole machine's Fmax --
  // -8.477 ns of slack, 17.809 ns of data delay against a 10.000 ns period,
  // `row_r[0]` through `Add5`, `Add6`, ... into `attrdiv`'s `final_sat_r`.
  // 1/(10.000 + 8.477) ns = 54.12 MHz, exactly the figure the receipt reports.
  //
  // The four terms do not depend on each other, so the sum can be a TREE:
  // depth 3 for five terms instead of depth 5, which is the optimum
  // (ceil(log2(5))). Nothing else changes -- these are 96-bit two's-complement
  // adds, addition is associative modulo 2**96, and `row_r[N] ? term : 0`
  // reproduces the conditional accumulate exactly, including the all-zero case
  // when no bit of `row_r` is set. The result is bit-identical by construction.
  //
  // "By construction" is an argument, not evidence, so here is the evidence:
  // the differential below drives this module and `zhao_raster_attrgrad_dsp3`
  // from the same stimulus and compares them cycle by cycle. That module was
  // not touched by this change, so it is an independent second implementation
  // and not a restatement of the same reasoning.
  // ENFORCED-BY: tests/raster/raster_attrgrad_dsp3_diff.cpp
  logic signed [95:0] row_offset_c, row_num_c;
  logic signed [95:0] row_t0_c, row_t1_c, row_t2_c, row_t3_c;
  logic signed [95:0] row_pair0_c, row_pair1_c;
  always_comb begin
    row_t0_c = row_r[0] ? dndy_r         : 96'sd0;
    row_t1_c = row_r[1] ? (dndy_r <<< 1) : 96'sd0;
    row_t2_c = row_r[2] ? (dndy_r <<< 2) : 96'sd0;
    row_t3_c = row_r[3] ? (dndy_r <<< 3) : 96'sd0;

    row_pair0_c  = row_t0_c + row_t1_c;      // level 1
    row_pair1_c  = row_t2_c + row_t3_c;      // level 1
    row_offset_c = row_pair0_c + row_pair1_c; // level 2
    row_num_c    = base_min_y0_r + row_offset_c; // level 3
  end

  logic               dv_valid, dv_ready;
  logic signed [95:0] dv_num;
  logic               dv_rvalid, dv_rready;
  logic signed [31:0] dv_q;
  logic               dv_sat, dv_err;
  logic [31:0]        dv_divides, dv_saturations, dv_errors;
  logic [31:0]        unused_dv_busy;

  always_comb begin
    dv_valid = 1'b0;
    dv_num   = 96'sd0;
    if (st_r == S_GRAD_REQ) begin
      dv_valid = 1'b1;
      dv_num   = dndx_r;
    end else if (st_r == S_ROW_REQ) begin
      dv_valid = 1'b1;
      dv_num   = row_num_c;
    end
  end
  assign dv_rready = (st_r == S_GRAD_WAIT) || (st_r == S_ROW_WAIT);

  zhao_raster_attrdiv_v2 #(.RADIX(RADIX)) u_div (
      .clk            (clk),
      .rst_n          (rst_n),
      .v_valid_i      (dv_valid),
      .v_ready_o      (dv_ready),
      .num_i          (dv_num),
      .area_i         (area2_r),
      .r_valid_o      (dv_rvalid),
      .r_ready_i      (dv_rready),
      .q_o            (dv_q),
      .q_saturated_o  (dv_sat),
      .q_error_o      (dv_err),
      .divides_o      (dv_divides),
      .saturations_o  (dv_saturations),
      .errors_o       (dv_errors),
      .busy_clocks_o  (unused_dv_busy),
      // SUNK DELIBERATELY. This block consumes the QUOTIENT only -- it walks a
      // row by gradient adds, not by a Euclidean pair -- so the divider's
      // remainder and the width guard that watches it have no reader here.
      // The guard is read where the pair is actually used (ATTRDIV.SVC and
      // RASTER.ATTRSTEP), and its positive control is the committed mutant
      // tests/mutants/zhao_raster_attrdiv_v2_remwidth_mutant.sv.
      /* verilator lint_off PINCONNECTEMPTY */
      .rem_o           (),
      .rem_range_err_o ()
      /* verilator lint_on PINCONNECTEMPTY */
  );

  assign divides_o      = dv_divides;
  assign saturations_o  = dv_saturations;
  assign divide_errors_o = dv_errors;

  logic signed [44:0] tile_offset_r;
  logic signed [45:0] row_q_ext_c, tile_offset_ext_c;
  logic signed [31:0] tile_seed_c;
  always_comb begin
    row_q_ext_c       = {{14{dv_q[31]}}, dv_q};
    tile_offset_ext_c = {{1{tile_offset_r[44]}}, tile_offset_r};
    // Exact global-to-tile accumulation leaf.  The committed omit-min-X-offset
    // mutant selects only this expression and leaves the real divider/FSM untouched.
    tile_seed_c = 32'(`ZHAO_ATTR_V2_TILE_SEED(row_q_ext_c, tile_offset_ext_c));
  end

  logic no_more_cols_c;
  always_comb begin
    no_more_cols_c = 1'b1;
    for (int unsigned c = 0; c < 16; ++c)
      if ((c > {28'd0, col_r}) && mask_r[c]) no_more_cols_c = 1'b0;
  end

  assign job_ready_o    = (st_r == S_IDLE);
  assign cov_ready_o    = (st_r == S_ROW);
  assign idle_o         = (st_r == S_IDLE) && !q_valid_o;
  assign q_valid_o      = (st_r == S_WALK) && mask_r[col_r];
  assign q_o            = walk_q_r;
  assign q_row_o        = row_r;
  assign q_col_o        = col_r;
  assign q_last_o       = last_row_r && no_more_cols_c;
  assign q_saturated_o  = grad_sat_r || row_sat_r;
  assign q_error_o      = grad_err_r || row_err_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r          <= S_IDLE;
      dndx_r        <= 96'sd0;
      dndy_r        <= 96'sd0;
      base_min_y0_r <= 96'sd0;
      mul_x_r       <= 96'sd0;
      mul_y_r       <= 96'sd0;
      n0_r          <= 96'sd0;
      area2_r       <= 47'd0;
      tile_delta_r  <= 13'sd0;
      tile_offset_r <= 45'sd0;
      grad_x_r      <= 32'sd0;
      grad_sat_r    <= 1'b0;
      grad_err_r    <= 1'b0;
      walk_q_r      <= 32'sd0;
      row_sat_r     <= 1'b0;
      row_err_r     <= 1'b0;
      mask_r        <= 16'd0;
      row_r         <= 4'd0;
      col_r         <= 4'd0;
      last_row_r    <= 1'b0;
      pixels_o      <= 32'd0;
    end else begin
      case (st_r)
        S_IDLE: begin
          if (job_valid_i && job_ready_o) begin
            dndx_r        <= dndx_in_c;
            dndy_r        <= dndy_in_c;
            // The multiplies land here; their sum lands in S_GRAD_REQ, which
            // is still fifty-odd clocks before S_ROW_REQ reads the result.
            mul_x_r       <= mul_x_c;
            mul_y_r       <= mul_y_c;
            n0_r          <= 96'(job_n0_i);
            area2_r       <= job_area2_i;
            tile_delta_r  <= tile_delta_c;
            grad_x_r      <= 32'sd0;
            grad_sat_r    <= 1'b0;
            grad_err_r    <= 1'b0;
            row_sat_r     <= 1'b0;
            row_err_r     <= 1'b0;
            st_r          <= S_GRAD_REQ;
          end
        end

        S_GRAD_REQ: begin
          // The second edge of the split above. Unconditional because its
          // inputs are all registers loaded in S_IDLE and therefore stable for
          // as long as this state lasts; the result is consumed in S_ROW_REQ.
          base_min_y0_r <= base_min_y0_c;
          if (dv_valid && dv_ready) st_r <= S_GRAD_WAIT;
        end

        S_GRAD_WAIT: begin
          if (dv_rvalid) begin
            grad_x_r      <= dv_q;
            // Capture the existing global-to-tile product at gradient return.
            // The later row-divide return then performs only the wrapping seed
            // addition instead of multiplier-plus-add on one edge.
            tile_offset_r <= $signed(dv_q) * $signed(tile_delta_r);
            grad_sat_r <= dv_sat;
            grad_err_r <= dv_err;
            st_r       <= S_ROW;
          end
        end

        S_ROW: begin
          if (cov_valid_i && cov_ready_o) begin
            mask_r     <= cov_mask_i;
            row_r      <= cov_row_i;
            col_r      <= 4'd0;
            last_row_r <= cov_last_i;
            st_r       <= S_ROW_REQ;
          end
        end

        S_ROW_REQ: begin
          if (dv_valid && dv_ready) st_r <= S_ROW_WAIT;
        end

        S_ROW_WAIT: begin
          if (dv_rvalid) begin
            walk_q_r  <= tile_seed_c;
            row_sat_r <= dv_sat;
            row_err_r <= dv_err;
            st_r      <= S_WALK;
          end
        end

        S_WALK: begin
          // Uncovered columns advance without producing a beat.  A covered beat
          // advances only on acceptance, so every output field holds under stall.
          if (!mask_r[col_r] || q_ready_i) begin
            if (q_valid_o) pixels_o <= pixels_o + 32'd1;
            if (col_r == 4'd15) begin
              st_r <= last_row_r ? S_IDLE : S_ROW;
            end else begin
              col_r     <= col_r + 4'd1;
              walk_q_r  <= walk_q_r + grad_x_r; // deliberate s32 wrap
            end
          end
        end

        default: st_r <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_raster_attrgrad_v2

`undef ZHAO_ATTR_V2_TILE_SEED
`default_nettype wire
