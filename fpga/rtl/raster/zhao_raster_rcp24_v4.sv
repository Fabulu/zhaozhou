// zhao_raster_rcp24_v4.sv -- Packet-B observation-only successor to
// zhao_raster_rcp24_v3.
//
// Arithmetic, queues, scheduling, counters, ready/valid behavior, and the
// completion-order result are intentionally unchanged.  The sole interface
// addition is idle_o.  It is a combinational observation of the existing
// accepted-but-not-retired occupancy and adds no state, credit, lifecycle edge,
// or ready path.  That occupancy spans reciprocal ingress, multiplier phases,
// ticket queues, completed contexts, and the held result.
`default_nettype none

module zhao_raster_rcp24_v4 #(
    parameter int unsigned NCTX = 16,
    parameter int unsigned TOKW = 8
) (
    input var logic clk,
    input var logic rst_n,

    input  var logic            v_valid_i,
    output var logic            v_ready_o,
    input  var logic [23:0]     d_i,
    input  var logic [TOKW-1:0] v_tok_i,

    output var logic            r_valid_o,
    input  var logic            r_ready_i,
    output var logic [23:0]     r_o,
    output var logic [5:0]      k_o,
    output var logic            d_zero_o,
    output var logic [TOKW-1:0] r_tok_o,

    output var logic [31:0] accepted_o,
    output var logic [31:0] completed_o,
    output var logic [31:0] mul_jobs_o,
    output var logic [31:0] zero_jobs_o,
    output var logic [31:0] phase_jobs_o,
    output var logic [31:0] negcorr_jobs_o,
    output var logic [5:0]  occupancy_o,
    output var logic        qerr_o,
    output var logic        idle_o
);

  localparam int unsigned CW = $clog2(NCTX);

  localparam logic [2:0] PH_MW0  = 3'd0;
  localparam logic [2:0] PH_MX0  = 3'd1;
  localparam logic [2:0] PH_MW1  = 3'd2;
  localparam logic [2:0] PH_MX1  = 3'd3;
  localparam logic [2:0] PH_ZERO = 3'd4;

  localparam int unsigned TKW  = 3 + CW;
  localparam int unsigned TAGW = TKW;

  logic [23:0]     p_m_q    [NCTX];
  logic [31:0]     p_x0_q   [NCTX];
  logic [5:0]      p_k_q    [NCTX];
  logic            p_zero_q [NCTX];
  logic [TOKW-1:0] p_tok_q  [NCTX];
  logic [31:0]     s_x_q    [NCTX];
  logic [31:0]     s_w_q    [NCTX];
  logic [23:0]     res_q    [NCTX];

  logic          free_push, free_pop, free_empty, free_full, free_err;
  logic [CW-1:0] free_din, free_dout;
  zhao_raster_ticketq #(
      .W      (CW),
      .D      (NCTX),
      .PRELOAD(1'b1)
  ) u_freeq (
      .clk    (clk),
      .rst_n  (rst_n),
      .push_i (free_push),
      .din_i  (free_din),
      .pop_i  (free_pop),
      .dout_o (free_dout),
      .empty_o(free_empty),
      .full_o (free_full),
      .err_o  (free_err)
  );

  logic           new_push, new_pop, new_empty, new_full, new_err;
  logic [TKW-1:0] new_din, new_dout;
  zhao_raster_ticketq #(
      .W(TKW),
      .D(NCTX)
  ) u_newq (
      .clk    (clk),
      .rst_n  (rst_n),
      .push_i (new_push),
      .din_i  (new_din),
      .pop_i  (new_pop),
      .dout_o (new_dout),
      .empty_o(new_empty),
      .full_o (new_full),
      .err_o  (new_err)
  );

  logic           cont_push, cont_pop, cont_empty, cont_full, cont_err;
  logic [TKW-1:0] cont_din, cont_dout;
  zhao_raster_ticketq #(
      .W(TKW),
      .D(NCTX)
  ) u_contq (
      .clk    (clk),
      .rst_n  (rst_n),
      .push_i (cont_push),
      .din_i  (cont_din),
      .pop_i  (cont_pop),
      .dout_o (cont_dout),
      .empty_o(cont_empty),
      .full_o (cont_full),
      .err_o  (cont_err)
  );

  logic          done_push, done_pop, done_empty, done_full, done_err;
  logic [CW-1:0] done_din, done_dout;
  zhao_raster_ticketq_rh #(
      .W(CW),
      .D(NCTX)
  ) u_doneq (
      .clk    (clk),
      .rst_n  (rst_n),
      .push_i (done_push),
      .din_i  (done_din),
      .pop_i  (done_pop),
      .dout_o (done_dout),
      .empty_o(done_empty),
      .full_o (done_full),
      .err_o  (done_err)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_full;
  assign unused_full = free_full | new_full | cont_full | done_full;
  /* verilator lint_on UNUSEDSIGNAL */

  assign v_ready_o = !free_empty;

  logic            a0_v_q;
  logic [CW-1:0]   a0_ctx_q;
  logic [23:0]     a0_d_q;
  logic [TOKW-1:0] a0_tok_q;

  logic            a1_v_q;
  logic [CW-1:0]   a1_ctx_q;
  logic [23:0]     a1_m_q;
  logic [7:0]      a1_idx_q;
  logic [5:0]      a1_k_q;
  logic            a1_zero_q;
  logic [TOKW-1:0] a1_tok_q;

  logic [4:0]  e_c;
  logic [23:0] m_c;
  always_comb begin
    e_c = 5'd0;
    for (int unsigned b = 0; b < 24; ++b) begin
      if (a0_d_q[23-b] && (e_c == 5'd0) && !a0_d_q[23]) e_c = 5'(b);
    end
    m_c = a0_d_q << e_c;
  end

  logic [30:0] seed_c;
  zhao_field_rcp24_rom u_rom (
      .idx_i (a1_idx_q),
      .seed_o(seed_c)
  );

  logic pri_q;
  logic grant_new_c, grant_cont_c;
  always_comb begin
    grant_new_c  = 1'b0;
    grant_cont_c = 1'b0;
    if (!new_empty && !cont_empty) begin
      grant_new_c  = pri_q;
      grant_cont_c = !pri_q;
    end else if (!new_empty) begin
      grant_new_c = 1'b1;
    end else if (!cont_empty) begin
      grant_cont_c = 1'b1;
    end
  end
  assign new_pop  = grant_new_c;
  assign cont_pop = grant_cont_c;

  logic [TKW-1:0] ticket_c;
  assign ticket_c = grant_new_c ? new_dout : cont_dout;

  logic           s1_v_q;
  logic [CW-1:0]  s1_ctx_q;
  logic [2:0]     s1_ph_q;

  logic           s2_v_q;
  logic [CW-1:0]  s2_ctx_q;
  logic [2:0]     s2_ph_q;
  logic [23:0]    s2_m_q;
  logic [31:0]    s2_x_q;
  logic [31:0]    s2_w_q;

  logic is_mw_c, is_mx_c, is_zero_c, neg_c;
  assign is_mw_c   = (s2_ph_q == PH_MW0) || (s2_ph_q == PH_MW1);
  assign is_mx_c   = (s2_ph_q == PH_MX0) || (s2_ph_q == PH_MX1);
  assign is_zero_c = (s2_ph_q == PH_ZERO);
  assign neg_c     = is_mx_c && (s2_w_q > 32'h8000_0000);

  logic [31:0] a_c, b_c, corr_c;
  always_comb begin
    a_c    = 32'd0;
    b_c    = 32'd0;
    corr_c = 32'd0;
    if (is_mw_c) begin
      a_c = {8'd0, s2_m_q};
      b_c = s2_x_q;
    end else if (is_mx_c) begin
      a_c = s2_x_q;
      b_c = 32'h8000_0000 - s2_w_q;
      if (neg_c) corr_c = s2_x_q;
    end
  end

  logic [TAGW-1:0] tag_c;
  assign tag_c = {s2_ph_q, s2_ctx_q};

  logic            mul_v_c;
  logic [TAGW-1:0] mul_tag_c;
  logic [31:0]     mul_w_c, mul_x_c;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0]     mul_phi_c, mul_plo_c;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_raster_rcp24_mul #(
      .TAGW(TAGW)
  ) u_mul (
      .clk     (clk),
      .rst_n   (rst_n),
      .valid_i (s2_v_q),
      .a_i     (a_c),
      .b_i     (b_c),
      .corr_i  (corr_c),
      .tag_i   (tag_c),
      .valid_o (mul_v_c),
      .tag_o   (mul_tag_c),
      .p_hi_o  (mul_phi_c),
      .p_lo_o  (mul_plo_c),
      .w_next_o(mul_w_c),
      .x_next_o(mul_x_c)
  );

  logic [CW-1:0] w_ctx_c;
  logic [2:0]    w_ph_c;
  assign w_ctx_c = mul_tag_c[CW-1:0];
  assign w_ph_c  = mul_tag_c[CW+2:CW];

  logic [32:0] resc7_c;
  assign resc7_c = {1'b0, mul_x_c} + 33'd64;

  logic [23:0] term_r_c;
  assign term_r_c = (w_ph_c == PH_ZERO)              ? 24'd0
                  : ((resc7_c >> 7) > 33'h00FF_FFFF) ? 24'hFF_FFFF
                  :                                     resc7_c[30:7];

  logic          t1_v_q;
  logic [CW-1:0] t1_ctx_q;
  logic [23:0]   t1_r_q;

  assign r_valid_o = !done_empty;
  assign r_o       = res_q[done_dout];
  assign k_o       = p_k_q[done_dout];
  assign d_zero_o  = p_zero_q[done_dout];
  assign r_tok_o   = p_tok_q[done_dout];

  logic retire_c;
  assign retire_c = r_valid_o && r_ready_i;

  assign free_pop  = v_valid_i && v_ready_o;
  assign free_push = retire_c;
  assign free_din  = done_dout;

  assign new_push = a1_v_q;
  assign new_din  = {(a1_zero_q ? PH_ZERO : PH_MW0), a1_ctx_q};

  logic cont_more_c;
  assign cont_more_c = mul_v_c && (w_ph_c != PH_MX1) && (w_ph_c != PH_ZERO);
  assign cont_push   = cont_more_c;
  always_comb begin
    cont_din = {PH_MX0, w_ctx_c};
    case (w_ph_c)
      PH_MW0:  cont_din = {PH_MX0, w_ctx_c};
      PH_MX0:  cont_din = {PH_MW1, w_ctx_c};
      default: cont_din = {PH_MX1, w_ctx_c};
    endcase
  end

  assign done_push = t1_v_q;
  assign done_din  = t1_ctx_q;
  assign done_pop  = retire_c;

  assign qerr_o = free_err | new_err | cont_err | done_err;

  // Packet-B observation only.  occupancy_o is reserved at request acceptance
  // and released only at external result acceptance, so zero covers every
  // internal stage and held result without peeking into child-private state.
  assign idle_o = (occupancy_o == 6'd0);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      a0_v_q         <= 1'b0;
      a1_v_q         <= 1'b0;
      s1_v_q         <= 1'b0;
      s2_v_q         <= 1'b0;
      t1_v_q         <= 1'b0;
      pri_q          <= 1'b0;
      accepted_o     <= 32'd0;
      completed_o    <= 32'd0;
      mul_jobs_o     <= 32'd0;
      zero_jobs_o    <= 32'd0;
      phase_jobs_o   <= 32'd0;
      negcorr_jobs_o <= 32'd0;
      occupancy_o    <= 6'd0;
    end else begin
      a0_v_q <= v_valid_i && v_ready_o;
      if (v_valid_i && v_ready_o) begin
        a0_ctx_q    <= free_dout;
        a0_d_q      <= d_i;
        a0_tok_q    <= v_tok_i;
        accepted_o  <= accepted_o + 32'd1;
        occupancy_o <= occupancy_o + 6'd1 - 6'(retire_c);
      end else if (retire_c) begin
        occupancy_o <= occupancy_o - 6'd1;
      end

      a1_v_q <= a0_v_q;
      if (a0_v_q) begin
        a1_ctx_q  <= a0_ctx_q;
        a1_m_q    <= m_c;
        a1_idx_q  <= 8'((m_c - 24'h80_0000) >> 15);
        a1_k_q    <= (a0_d_q == 24'd0) ? 6'd0 : (6'({1'b0, e_c}) + 6'd1);
        a1_zero_q <= (a0_d_q == 24'd0);
        a1_tok_q  <= a0_tok_q;
      end

      if (a1_v_q) begin
        p_m_q[a1_ctx_q]    <= a1_zero_q ? 24'd0 : a1_m_q;
        p_x0_q[a1_ctx_q]   <= a1_zero_q ? 32'd0 : {1'b0, seed_c};
        p_k_q[a1_ctx_q]    <= a1_k_q;
        p_zero_q[a1_ctx_q] <= a1_zero_q;
        p_tok_q[a1_ctx_q]  <= a1_tok_q;
      end

      s1_v_q <= grant_new_c || grant_cont_c;
      if (grant_new_c || grant_cont_c) begin
        s1_ctx_q <= ticket_c[CW-1:0];
        s1_ph_q  <= ticket_c[CW+2:CW];
      end
      if (!new_empty && !cont_empty) pri_q <= !pri_q;

      s2_v_q <= s1_v_q;
      if (s1_v_q) begin
        s2_ctx_q <= s1_ctx_q;
        s2_ph_q  <= s1_ph_q;
        s2_m_q   <= p_m_q[s1_ctx_q];
        s2_x_q   <= (s1_ph_q == PH_MW0) ? p_x0_q[s1_ctx_q] : s_x_q[s1_ctx_q];
        s2_w_q   <= s_w_q[s1_ctx_q];
      end

      if (s2_v_q) begin
        phase_jobs_o <= phase_jobs_o + 32'd1;
        if (is_zero_c) zero_jobs_o <= zero_jobs_o + 32'd1;
        else           mul_jobs_o  <= mul_jobs_o + 32'd1;
        if (neg_c)     negcorr_jobs_o <= negcorr_jobs_o + 32'd1;
      end

      t1_v_q <= mul_v_c && ((w_ph_c == PH_MX1) || (w_ph_c == PH_ZERO));
      if (mul_v_c) begin
        case (w_ph_c)
          PH_MW0: begin
            s_w_q[w_ctx_c] <= mul_w_c;
            s_x_q[w_ctx_c] <= p_x0_q[w_ctx_c];
          end
          PH_MX0:  s_x_q[w_ctx_c] <= mul_x_c;
          PH_MW1:  s_w_q[w_ctx_c] <= mul_w_c;
          default: begin
            t1_ctx_q <= w_ctx_c;
            t1_r_q   <= term_r_c;
          end
        endcase
      end

      if (t1_v_q) res_q[t1_ctx_q] <= t1_r_q;
      if (retire_c) completed_o <= completed_o + 32'd1;
    end
  end

endmodule : zhao_raster_rcp24_v4

`default_nettype wire
