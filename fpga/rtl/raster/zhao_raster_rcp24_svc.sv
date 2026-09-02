// zhao_raster_rcp24_svc.sv — the same reciprocal, scheduled instead of serial.
//
// BESIDE `zhao_raster_rcp24.sv`, which stays the golden implementation and the
// ORACLE. Nothing instantiates this yet.
//
// ---------------------------------------------------------------------------
// WHY
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md, endorsed by reports/Addendum:
//
//   > zhao_raster_rcp24 performs four dependent multiply jobs for one
//   > reciprocal before accepting another request. [...] One multiplier
//   > launches one micro-job per clock. Dependencies exist within a token, but
//   > other tokens occupy the intervening clocks.
//
// The dependencies are real and are not being removed. MW1 genuinely needs
// MX0's result. What is wrong today is that the multiplier IDLES through those
// dependencies instead of serving somebody else, so one reciprocal owns the
// unit for its whole latency and capacity lands at ~151,515 fragments a frame
// against a 276,480 terrain estimate.
//
// The addendum's phrasing of the fix is the whole design: "don't clone
// reciprocal units; interleave dependent micro-jobs from multiple tokens so one
// multiplier stays occupied."
//
//     4 multiplier jobs per reciprocal, one launch per clock
//     => one reciprocal every 4 clocks => 416,666/frame at 100 MHz
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS THE OLD ONE, INSTRUCTION FOR INSTRUCTION
// ---------------------------------------------------------------------------
// The brief says "Keep the exact existing arithmetic and ROM", so every line
// below is transcribed from `zhao_raster_rcp24.sv` rather than rederived:
//
//     seed  x = {1'b0, rom(m)}
//     MW    w = (m * x) >> 24              m zero-extended, product 64-bit
//     MX    x = (x * (2^31 - w) + 2^29) >> 30, TRUNCATED to 32 bits
//     twice, then
//     r     = (x + 64) >> 7, clamped to 24'hFF_FFFF, or 0 when the input was 0
//
// Two details in the original are law rather than accident and are preserved
// deliberately, because "fixing" either would silently change results:
//
//   * `t = 2^31 - w` WRAPS at 64 bits, matching the reference's uint64.
//   * the Newton iterate is stored back into 32 bits, so the top of the
//     rescaled value is DISCARDED BY LAW, not by a width that happened to fit.
//
// The old block is the oracle in the paired test, so any drift fails loudly
// rather than being argued about.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_raster_rcp24_svc #(
    // Eight contexts, as the brief specifies. Four micro-jobs each and a
    // three-clock turnaround means three tokens would nominally saturate the
    // multiplier; eight leaves margin for the accept and complete clocks
    // without pretending more is free.
    parameter int unsigned NCTX = 8,
    parameter int unsigned TOKW = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- request -------------------------------------------------------------
    // The RAW denominator, as zhao_raster_rcp24 takes it. Normalisation, the
    // seed index and the zero case are done HERE for the same reason they are
    // done there -- moving them to the caller would change the block's contract
    // while claiming to keep its arithmetic.
    input  var logic            v_valid_i,
    output var logic            v_ready_o,
    input  var logic [23:0]     d_i,
    input  var logic [TOKW-1:0] v_tok_i,

    // ---- result, IN COMPLETION ORDER (not request order) ---------------------
    // Reordering belongs to the caller's join, which already has token identity.
    // Imposing it here would rebuild a queue the consumer already owns.
    output var logic            r_valid_o,
    input  var logic            r_ready_i,
    output var logic [23:0]     r_o,
    output var logic [5:0]      k_o,
    output var logic            d_zero_o,
    output var logic [TOKW-1:0] r_tok_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]     accepted_o,
    output var logic [31:0]     completed_o,
    output var logic [31:0]     mul_busy_o,   // clocks the multiplier launched
    output var logic [3:0]      occupancy_o   // live contexts
);

  localparam int CW = $clog2(NCTX);

  // Phase within a token: the four micro-jobs the brief names.
  localparam logic [1:0] PH_MW0 = 2'd0;
  localparam logic [1:0] PH_MX0 = 2'd1;
  localparam logic [1:0] PH_MW1 = 2'd2;
  localparam logic [1:0] PH_MX1 = 2'd3;

  // ------------------------------------------------------------- contexts ---
  logic            c_val  [NCTX];
  logic            c_pend [NCTX];   // has an un-launched micro-job
  logic [1:0]      c_ph   [NCTX];
  logic [23:0]     c_m    [NCTX];
  logic [31:0]     c_x    [NCTX];
  logic [63:0]     c_w    [NCTX];
  logic [5:0]      c_k    [NCTX];
  logic            c_zero [NCTX];
  logic [TOKW-1:0] c_tok  [NCTX];

  // ---- normalise: shift d left until bit 23 is set -------------------------
  // Transcribed from the serial block, priority scan and all. d == 0 never
  // reaches the iteration; it is captured as `zero` at accept.
  logic [4:0]  e_c;
  logic [23:0] m_c;
  always_comb begin
    e_c = 5'd0;
    for (int unsigned b = 0; b < 24; ++b) begin
      if (d_i[23-b] && (e_c == 5'd0) && !d_i[23]) e_c = 5'(b);
    end
    m_c = d_i << e_c;
  end

  // ---- the shared T24 seed table, the same ROM the serial block uses -------
  logic [7:0]  idx_c;
  logic [30:0] seed_c;
  assign idx_c = 8'((m_c - 24'h80_0000) >> 15);
  zhao_field_rcp24_rom u_rom (
      .idx_i (idx_c),
      .seed_o(seed_c)
  );

  logic v_zero_c;
  assign v_zero_c = (d_i == 24'd0);

  // ------------------------------------------------------------- allocate ---
  logic          free_v;
  logic [CW-1:0] free_i;
  always_comb begin
    free_v = 1'b0;
    free_i = '0;
    for (int i = 0; i < NCTX; i++) begin
      if (!free_v && !c_val[i]) begin
        free_v = 1'b1;
        free_i = CW'(i);
      end
    end
  end
  assign v_ready_o = free_v;

  // --------------------------------------------------- micro-job selection --
  // One launch per clock. Round-robin from a rotating pointer so a token that
  // becomes ready early cannot monopolise the multiplier.
  logic [CW-1:0] rr_q;
  logic          pick_v;
  logic [CW-1:0] pick_i;
  always_comb begin
    pick_v = 1'b0;
    pick_i = '0;
    for (int n = 0; n < NCTX; n++) begin
      automatic logic [CW-1:0] idx = CW'((int'(rr_q) + n) % NCTX);
      if (!pick_v && c_val[idx] && c_pend[idx]) begin
        pick_v = 1'b1;
        pick_i = idx;
      end
    end
  end

  // ---- M0: operands, exactly the serial block's selection ------------------
  logic [31:0] mul_a_c;
  logic [63:0] mul_b_c, t_c;
  always_comb begin
    // 2^31 - w, wrapping at 64 bits exactly as the reference's uint64 does.
    t_c     = 64'h0000_0000_8000_0000 - c_w[pick_i];
    // MW phases multiply m by x; MX phases multiply x by (2^31 - w).
    mul_a_c = (c_ph[pick_i] == PH_MW0 || c_ph[pick_i] == PH_MW1)
                  ? {8'd0, c_m[pick_i]} : c_x[pick_i];
    mul_b_c = (c_ph[pick_i] == PH_MW0 || c_ph[pick_i] == PH_MW1)
                  ? {32'd0, c_x[pick_i]} : t_c;
  end

  // ---- M1: the registered product ------------------------------------------
  logic          m1_v_q;
  logic [CW-1:0] m1_i_q;
  logic [1:0]    m1_ph_q;
  logic [63:0]   m1_p_q;

  // ---- M2 update -----------------------------------------------------------
  // Truncating at 64 bits is the point, not an accident: the reference's
  // product is a uint64 and its overflow is part of the law.
  // rescale_u(v, k) = (v + 2^(k-1)) >> k, round-half-up.
  // The reference stores the Newton iterate back into a uint32, so the top of
  // the rescaled value is DISCARDED BY LAW rather than by width choice. The
  // serial block says exactly this and suppresses the same warning; saying it
  // here too is the difference between reproducing the arithmetic and
  // accidentally matching it.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [63:0] resc30_c;
  /* verilator lint_on UNUSEDSIGNAL */
  assign resc30_c = (m1_p_q + 64'h0000_0000_2000_0000) >> 30;

  // The final rescale reads the context's x AFTER the last MX has written it,
  // so it is computed at completion rather than in the multiplier path.
  logic [63:0] resc7_c;
  logic [CW-1:0] done_i;
  assign resc7_c = ({32'd0, c_x[done_i]} + 64'd64) >> 7;

  // ---------------------------------------------------------- completion ----
  logic          done_v;
  always_comb begin
    done_v = 1'b0;
    done_i = '0;
    for (int i = 0; i < NCTX; i++) begin
      // A context is finished when it is valid, has no pending job and none in
      // flight — which the phase wrap records by parking at PH_MX1 with
      // c_pend low.
      if (!done_v && c_val[i] && !c_pend[i] && c_ph[i] == PH_MX1 && !(m1_v_q && m1_i_q == CW'(i))) begin
        done_v = 1'b1;
        done_i = CW'(i);
      end
    end
  end

  assign r_valid_o = done_v;
  assign r_o       = c_zero[done_i] ? 24'd0
                   : ((resc7_c > 64'h00FF_FFFF) ? 24'hFF_FFFF : resc7_c[23:0]);
  assign k_o       = c_k[done_i];
  assign d_zero_o  = c_zero[done_i];
  assign r_tok_o   = c_tok[done_i];

  always_comb begin
    occupancy_o = 4'd0;
    for (int i = 0; i < NCTX; i++) occupancy_o = occupancy_o + 4'(c_val[i]);
  end

  // ------------------------------------------------------------ sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rr_q        <= '0;
      m1_v_q      <= 1'b0;
      accepted_o  <= 32'd0;
      completed_o <= 32'd0;
      mul_busy_o  <= 32'd0;
      for (int i = 0; i < NCTX; i++) begin
        c_val[i]  <= 1'b0;
        c_pend[i] <= 1'b0;
        c_ph[i]   <= PH_MW0;
      end
    end else begin
      // ---- accept ---------------------------------------------------------
      if (v_valid_i && v_ready_o) begin
        c_val[free_i]  <= 1'b1;
        c_pend[free_i] <= 1'b1;
        c_ph[free_i]   <= PH_MW0;
        c_m[free_i]    <= v_zero_c ? 24'd0 : m_c;
        c_x[free_i]    <= v_zero_c ? 32'd0 : {1'b0, seed_c};
        c_w[free_i]    <= 64'd0;
        c_k[free_i]    <= v_zero_c ? 6'd0 : (6'({1'b0, e_c}) + 6'd1);
        c_zero[free_i] <= v_zero_c;
        c_tok[free_i]  <= v_tok_i;
        accepted_o     <= accepted_o + 32'd1;
      end

      // ---- M0 launch ------------------------------------------------------
      m1_v_q <= pick_v;
      if (pick_v) begin
        m1_i_q     <= pick_i;
        m1_ph_q    <= c_ph[pick_i];
        m1_p_q     <= 64'(mul_a_c) * mul_b_c;
        c_pend[pick_i] <= 1'b0;          // in flight; not re-launchable
        rr_q       <= (pick_i == CW'(NCTX - 1)) ? '0 : pick_i + CW'(1);
        mul_busy_o <= mul_busy_o + 32'd1;
      end

      // ---- M2 writeback ---------------------------------------------------
      if (m1_v_q) begin
        case (m1_ph_q)
          PH_MW0: begin
            c_w[m1_i_q]    <= m1_p_q >> 24;
            c_ph[m1_i_q]   <= PH_MX0;
            c_pend[m1_i_q] <= 1'b1;
          end
          PH_MX0: begin
            c_x[m1_i_q]    <= resc30_c[31:0];
            c_ph[m1_i_q]   <= PH_MW1;
            c_pend[m1_i_q] <= 1'b1;
          end
          PH_MW1: begin
            c_w[m1_i_q]    <= m1_p_q >> 24;
            c_ph[m1_i_q]   <= PH_MX1;
            c_pend[m1_i_q] <= 1'b1;
          end
          default: begin  // PH_MX1 — the last iterate; the token is now done
            c_x[m1_i_q]    <= resc30_c[31:0];
            c_ph[m1_i_q]   <= PH_MX1;
            c_pend[m1_i_q] <= 1'b0;
          end
        endcase
      end

      // ---- retire ---------------------------------------------------------
      if (done_v && r_ready_i) begin
        c_val[done_i] <= 1'b0;
        completed_o   <= completed_o + 32'd1;
      end
    end
  end

endmodule : zhao_raster_rcp24_svc

`default_nettype wire
