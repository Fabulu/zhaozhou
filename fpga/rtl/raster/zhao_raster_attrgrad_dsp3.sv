// zhao_raster_attrgrad_dsp3.sv -- exact ATTR3 candidate for one tile plane.
//
// Public arithmetic, divider, pixel walk, counters, and handshakes match
// zhao_raster_attrgrad_v2.  Job-setup products use one local three-DSP worker:
// BASE_X and BASE_Y fold into the wrapping96 base, then OFFSET writes signed45.
// The divider remains unchanged and the fixed micro-calendar fits even its
// two-clock exceptional responses.
`default_nettype none

`ifndef ZHAO_ATTR_DSP3_TILE_SEED
`define ZHAO_ATTR_DSP3_TILE_SEED(row_q, tile_offset) \
    ($signed(row_q) + $signed(tile_offset))
`endif
`ifndef ZHAO_ATTR_DSP3_BASE_Y_RESULT
`define ZHAO_ATTR_DSP3_BASE_Y_RESULT(value) (value)
`endif
`ifndef ZHAO_ATTR_DSP3_OFFSET_READY_VALUE
`define ZHAO_ATTR_DSP3_OFFSET_READY_VALUE 1'b1
`endif
`ifndef ZHAO_ATTR_DSP3_IDLE_EXPR
`define ZHAO_ATTR_DSP3_IDLE_EXPR(value) (value)
`endif

(* preserve_hierarchy *)
module zhao_raster_attrgrad_dsp3 #(
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

  localparam logic [1:0] M_BASE_X = 2'd0;
  localparam logic [1:0] M_BASE_Y = 2'd1;
  localparam logic [1:0] M_OFFSET = 2'd2;

  logic [2:0] st_r;
  logic signed [95:0] dndx_r, dndy_r;
  logic signed [95:0] base_min_y0_r;
  logic        [46:0] area2_r;
  logic signed [11:0] min_x_r, tile_y_r;
  logic signed [12:0] tile_delta_r;

  logic signed [31:0] grad_x_r;
  logic               grad_sat_r, grad_err_r;
  logic signed [31:0] walk_q_r;
  logic               row_sat_r, row_err_r;
  logic        [15:0] mask_r;
  logic        [3:0]  row_r, col_r;
  logic               last_row_r;

  logic signed [95:0] dndx_in_c, dndy_in_c;
  logic signed [12:0] tile_delta_c;
  always_comb begin
    dndx_in_c = 96'(job_dndx_i);
    dndy_in_c = 96'(job_dndy_i);
    tile_delta_c = $signed({job_tile_x_i[11], job_tile_x_i})
                 - $signed({job_min_x_i[11], job_min_x_i});
  end

  logic signed [95:0] row_offset_c, row_num_c;
  always_comb begin
    row_offset_c = 96'sd0;
    if (row_r[0]) row_offset_c = row_offset_c + dndy_r;
    if (row_r[1]) row_offset_c = row_offset_c + (dndy_r <<< 1);
    if (row_r[2]) row_offset_c = row_offset_c + (dndy_r <<< 2);
    if (row_r[3]) row_offset_c = row_offset_c + (dndy_r <<< 3);
    row_num_c = base_min_y0_r + row_offset_c;
  end

  logic               dv_valid, dv_ready;
  logic signed [95:0] dv_num;
  logic               dv_rvalid, dv_rready;
  logic signed [31:0] dv_q;
  logic               dv_sat, dv_err;
  logic [31:0]        dv_divides, dv_saturations, dv_errors;
  logic [31:0]        unused_dv_busy;

  logic base_ready_r, offset_ready_r, offset_pending_r;
  logic [1:0] base_phase_r;

  always_comb begin
    dv_valid = 1'b0;
    dv_num   = 96'sd0;
    if (st_r == S_GRAD_REQ) begin
      dv_valid = 1'b1;
      dv_num   = dndx_r;
    end else if ((st_r == S_ROW_REQ) && base_ready_r) begin
      dv_valid = 1'b1;
      dv_num   = row_num_c;
    end
  end
  assign dv_rready = (st_r == S_GRAD_WAIT) ||
                     ((st_r == S_ROW_WAIT) && offset_ready_r);

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
      .busy_clocks_o  (unused_dv_busy)
  );

  assign divides_o       = dv_divides;
  assign saturations_o   = dv_saturations;
  assign divide_errors_o = dv_errors;

  // Fixed local micro-issue priority. BASE_X and BASE_Y occupy J+1/J+2;
  // OFFSET cannot become pending before the gradient commits at J+4.
  logic mul_in_valid_c;
  logic [1:0] mul_in_op_c;
  logic signed [71:0] mul_in_a_c;
  logic signed [12:0] mul_in_b_c;
  logic mul_out_valid_c, mul_idle_c;
  logic [1:0] mul_out_op_c;
  logic signed [84:0] mul_out_product_c;

  always_comb begin
    mul_in_valid_c = 1'b0;
    mul_in_op_c = M_BASE_X;
    mul_in_a_c = 72'sd0;
    mul_in_b_c = 13'sd0;
    if (base_phase_r == 2'd1) begin
      mul_in_valid_c = 1'b1;
      mul_in_op_c = M_BASE_X;
      mul_in_a_c = dndx_r[71:0];
      mul_in_b_c = $signed({min_x_r[11], min_x_r});
    end else if (base_phase_r == 2'd2) begin
      mul_in_valid_c = 1'b1;
      mul_in_op_c = M_BASE_Y;
      mul_in_a_c = dndy_r[71:0];
      mul_in_b_c = $signed({tile_y_r[11], tile_y_r});
    end else if (offset_pending_r) begin
      mul_in_valid_c = 1'b1;
      mul_in_op_c = M_OFFSET;
      mul_in_a_c = {{40{grad_x_r[31]}}, grad_x_r};
      mul_in_b_c = tile_delta_r;
    end
  end

  zhao_attr_mul72x13_dsp3 u_mul (
      .clk          (clk),
      .rst_n        (rst_n),
      .in_valid_i   (mul_in_valid_c),
      .in_op_i      (mul_in_op_c),
      .in_a_i       (mul_in_a_c),
      .in_b_i       (mul_in_b_c),
      .out_valid_o  (mul_out_valid_c),
      .out_op_o     (mul_out_op_c),
      .out_product_o(mul_out_product_c),
      .idle_o       (mul_idle_c)
  );

  logic signed [44:0] tile_offset_r;
  logic signed [45:0] row_q_ext_c, tile_offset_ext_c;
  logic signed [31:0] tile_seed_c;
  always_comb begin
    row_q_ext_c       = {{14{dv_q[31]}}, dv_q};
    tile_offset_ext_c = {{1{tile_offset_r[44]}}, tile_offset_r};
    tile_seed_c = 32'(`ZHAO_ATTR_DSP3_TILE_SEED(row_q_ext_c,
                                                tile_offset_ext_c));
  end

  logic no_more_cols_c;
  always_comb begin
    no_more_cols_c = 1'b1;
    for (int unsigned c = 0; c < 16; ++c)
      if ((c > {28'd0, col_r}) && mask_r[c]) no_more_cols_c = 1'b0;
  end

  assign job_ready_o = (st_r == S_IDLE) && mul_idle_c &&
                       (base_phase_r == 2'd0) && !offset_pending_r;
  assign cov_ready_o = (st_r == S_ROW);
  assign idle_o = `ZHAO_ATTR_DSP3_IDLE_EXPR(job_ready_o && !q_valid_o);
  assign q_valid_o = (st_r == S_WALK) && mask_r[col_r];
  assign q_o = walk_q_r;
  assign q_row_o = row_r;
  assign q_col_o = col_r;
  assign q_last_o = last_row_r && no_more_cols_c;
  assign q_saturated_o = grad_sat_r || row_sat_r;
  assign q_error_o = grad_err_r || row_err_r;

  logic signed [95:0] mul_product_ext_c;
  assign mul_product_ext_c = {{11{mul_out_product_c[84]}},
                              mul_out_product_c};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r             <= S_IDLE;
      dndx_r           <= 96'sd0;
      dndy_r           <= 96'sd0;
      base_min_y0_r    <= 96'sd0;
      area2_r          <= 47'd0;
      min_x_r          <= 12'sd0;
      tile_y_r         <= 12'sd0;
      tile_delta_r     <= 13'sd0;
      tile_offset_r    <= 45'sd0;
      base_phase_r     <= 2'd0;
      base_ready_r     <= 1'b0;
      offset_pending_r <= 1'b0;
      offset_ready_r   <= 1'b0;
      grad_x_r         <= 32'sd0;
      grad_sat_r       <= 1'b0;
      grad_err_r       <= 1'b0;
      walk_q_r         <= 32'sd0;
      row_sat_r        <= 1'b0;
      row_err_r        <= 1'b0;
      mask_r           <= 16'd0;
      row_r            <= 4'd0;
      col_r            <= 4'd0;
      last_row_r       <= 1'b0;
      pixels_o         <= 32'd0;
    end else begin
      // One writer per state field. The fixed issue/result calendar makes these
      // base updates mutually exclusive, and assertions below keep that true.
      if ((st_r == S_IDLE) && job_valid_i && job_ready_o) begin
        base_min_y0_r <= job_n0_i;
        base_phase_r <= 2'd1;
        base_ready_r <= 1'b0;
        offset_pending_r <= 1'b0;
        offset_ready_r <= 1'b0;
      end else if (base_phase_r == 2'd1) begin
        base_min_y0_r <= base_min_y0_r + (dndx_r >>> 1);
        base_phase_r <= 2'd2;
      end else if (base_phase_r == 2'd2) begin
        base_min_y0_r <= base_min_y0_r + (dndy_r >>> 1);
        base_phase_r <= 2'd0;
      end else if (mul_out_valid_c && (mul_out_op_c == M_BASE_X)) begin
        base_min_y0_r <= base_min_y0_r + mul_product_ext_c;
      end else if (mul_out_valid_c && (mul_out_op_c == M_BASE_Y)) begin
        base_min_y0_r <= base_min_y0_r +
            96'(`ZHAO_ATTR_DSP3_BASE_Y_RESULT(mul_product_ext_c));
        base_ready_r <= 1'b1;
      end

      if (mul_in_valid_c && (mul_in_op_c == M_OFFSET))
        offset_pending_r <= 1'b0;
      if (mul_out_valid_c && (mul_out_op_c == M_OFFSET)) begin
        tile_offset_r <= mul_out_product_c[44:0];
        offset_ready_r <= `ZHAO_ATTR_DSP3_OFFSET_READY_VALUE;
      end

      case (st_r)
        S_IDLE: begin
          if (job_valid_i && job_ready_o) begin
            dndx_r        <= dndx_in_c;
            dndy_r        <= dndy_in_c;
            area2_r       <= job_area2_i;
            min_x_r       <= job_min_x_i;
            tile_y_r      <= job_tile_y_i;
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
          if (dv_valid && dv_ready) st_r <= S_GRAD_WAIT;
        end

        S_GRAD_WAIT: begin
          if (dv_rvalid) begin
            grad_x_r <= dv_q;
            offset_pending_r <= 1'b1;
            grad_sat_r <= dv_sat;
            grad_err_r <= dv_err;
            st_r <= S_ROW;
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
          if (dv_rvalid && dv_rready) begin
            walk_q_r  <= tile_seed_c;
            row_sat_r <= dv_sat;
            row_err_r <= dv_err;
            st_r      <= S_WALK;
          end
        end

        S_WALK: begin
          if (!mask_r[col_r] || q_ready_i) begin
            if (q_valid_o) pixels_o <= pixels_o + 32'd1;
            if (col_r == 4'd15) begin
              st_r <= last_row_r ? S_IDLE : S_ROW;
            end else begin
              col_r    <= col_r + 4'd1;
              walk_q_r <= walk_q_r + grad_x_r;
            end
          end
        end

        default: st_r <= S_IDLE;
      endcase
    end
  end

  // synthesis translate_off
  logic [31:0] verify_mul_issued_q, verify_mul_retired_q;
  logic verify_offset_guard_fault_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      verify_mul_issued_q <= 32'd0;
      verify_mul_retired_q <= 32'd0;
      verify_offset_guard_fault_q <= 1'b0;
    end else begin
      if (mul_in_valid_c) verify_mul_issued_q <= verify_mul_issued_q + 32'd1;
      if (mul_out_valid_c) verify_mul_retired_q <= verify_mul_retired_q + 32'd1;
      if (dv_rvalid && (st_r == S_ROW_WAIT) && !offset_ready_r)
        verify_offset_guard_fault_q <= 1'b1;
`ifndef ZHAO_ATTR_DSP3_MUTANT_DISABLE_LANE_ASSERTIONS
      a_no_base_issue_collision : assert (!((base_phase_r != 2'd0) &&
          offset_pending_r));
      a_base_ready_before_row_div : assert (!((st_r == S_ROW_REQ) &&
          !base_ready_r));
      a_offset_ready_before_row_result : assert (!(dv_rvalid &&
          (st_r == S_ROW_WAIT) && !offset_ready_r));
      a_idle_has_no_micro_work : assert (!idle_o ||
          ((verify_mul_issued_q == verify_mul_retired_q) &&
           (base_phase_r == 2'd0) && !offset_pending_r));
`endif
    end
  end
  // synthesis translate_on

endmodule : zhao_raster_attrgrad_dsp3

`undef ZHAO_ATTR_DSP3_TILE_SEED
`undef ZHAO_ATTR_DSP3_BASE_Y_RESULT
`undef ZHAO_ATTR_DSP3_OFFSET_READY_VALUE
`undef ZHAO_ATTR_DSP3_IDLE_EXPR
`undef ZHAO_ATTR_DSP3_MUTANT_DISABLE_LANE_ASSERTIONS
`default_nettype wire
