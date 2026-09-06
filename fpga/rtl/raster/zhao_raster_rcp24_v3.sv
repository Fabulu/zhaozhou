// zhao_raster_rcp24_v3.sv — the same reciprocal, QUEUED instead of scanned, and
// with the 32-by-64 multiply replaced by an exact 32x32 product.
//
// ENFORCED-BY: tests/raster/raster_rcp24_v3_directed.cpp:main
//
// A STANDALONE TILE. TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt S26.1: "In
// parallel, the RCP arithmetic tile can establish its own exact-tool packing.
// Do not merge those changes into the composition until the shared record and
// credit contracts are fixed." So nothing instantiates this, and it deliberately
// keeps the OLD block's request/result ports rather than adopting owner14 /
// RCP_RESULT / sample-owner tickets. Those belong to the composition contract
// and adopting them here would make this tile un-diffable against the shipped
// oracle, which is the only reason it can be trusted at all.
//
// ---------------------------------------------------------------------------
// WHAT CHANGES, AND WHAT MAY NOT
// ---------------------------------------------------------------------------
// S10.1: "The reference law is unchanged: normalize a 24-bit denominator, use
// the existing RCP24 seed ROM, perform MW0, MX0, MW1, MX1, then perform the
// final rounded rescale and 24-bit clamp. Preserve zero-input behavior and the
// exponent convention." And: "Bit-exact equivalence includes truncation and
// wraparound, not just an error bound."
//
// So the ROM is the same instance, k is still leading-zero-count + 1, r is
// still `(x + 64) >> 7` pinned at 0xFF_FFFF, and d == 0 still terminates with
// d_zero_o raised instead of shifting forever.
//
// TWO THINGS CHANGE:
//
//   1. THE SCANS BECOME QUEUES. `zhao_raster_rcp24_svc` walks its context table
//      once for a free slot, once for a pending job, once for a done token,
//      every clock. S10.2: "There is no context-wide free scan, ready scan, or
//      completion scan." A free-context FIFO, a NEW queue, a CONTINUATION queue
//      and a DONE queue replace all three, and the context count rises from 8
//      to 16 because the explicit product pipeline has a longer feedback loop.
//
//   2. THE MULTIPLY BECOMES EXACT AND NARROW. `zhao_raster_rcp24_mul` carries
//      the S10.5 identity. That file owns the proof and the four multiplier
//      sites; this one owns the schedule.
//
// ---------------------------------------------------------------------------
// THE FEEDBACK LOOP IS TEN CLOCKS, WHICH IS WHY THERE ARE SIXTEEN CONTEXTS
// ---------------------------------------------------------------------------
// S10.8's stages, as built:
//
//   Q  arbitrate NEW vs CONT, pop one ticket            -> s1
//   R  read payload/scratch by context index            -> s2
//   O  select a/b/corr, register operands               -> mul O
//   M  four parallel 16x16 partial products             -> mul M
//   X  cross sum                                        -> mul X
//   L  low sum and carry                                -> mul L
//   H  high sum                                         -> mul H
//   C  high-half negative correction                    -> mul C
//   E  MW extraction or MX rounded 32-bit rescale       -> mul E
//   W  scratch write, continuation push, or terminal    -> this file
//
// Ten clocks from pop to re-pop. Sixteen contexts therefore OFFER 16 tickets
// every 10 clocks -- 1.6 per clock against one launch per clock -- so the
// multiplier saturates and a nonzero reciprocal costs its four launches and
// nothing idle. S10.2 asks for this to be measured rather than asserted, so the
// launch counters below are outputs and the test divides them.
//
// S10.8: "16 contexts are a defensible starting point, not a measured optimum."
// NCTX is a parameter for exactly that reason.
//
// ---------------------------------------------------------------------------
// ZERO IS A SCHEDULED PHASE, NOT A BYPASS
// ---------------------------------------------------------------------------
// S10.8: zero-input handling "does not create an independent unqueued bypass
// writer that can collide with a normal RCP final write. Its product counter
// explicitly counts zero jobs, while a separate scheduled-phase counter
// includes this phase, rather than pretending four products occurred."
//
// PH_ZERO therefore rides the same ten stages with a = b = 0, lands in the same
// terminal register and the same DONE queue, and is counted in `zero_jobs_o`
// and `phase_jobs_o` but not in `mul_jobs_o`.
//
// ---------------------------------------------------------------------------
// THE SCRATCH IS INITIALISED BY MW0's WRITEBACK, NOT BY ADMISSION
// ---------------------------------------------------------------------------
// S10.2: "Phase MW0 ignores old scratch and initializes the whole scratch row
// through its ordinary writeback. Admission must not also initialize scratch."
// So MW0 reads the seed from the read-only payload plane, and its writeback is
// the only thing that ever writes both scratch words at once. A context's
// scratch row holds whatever the previous occupant left until then, and that is
// intentional -- if MW0's initialisation is wrong, the test sees garbage from
// the last tenant rather than a zero that happens to be harmless.
`default_nettype none

module zhao_raster_rcp24_v3 #(
    parameter int unsigned NCTX = 16,
    parameter int unsigned TOKW = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- request -------------------------------------------------------------
    input  var logic            v_valid_i,
    output var logic            v_ready_o,
    input  var logic [23:0]     d_i,
    input  var logic [TOKW-1:0] v_tok_i,

    // ---- result, IN COMPLETION ORDER ----------------------------------------
    output var logic            r_valid_o,
    input  var logic            r_ready_i,
    output var logic [23:0]     r_o,
    output var logic [5:0]      k_o,
    output var logic            d_zero_o,
    output var logic [TOKW-1:0] r_tok_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] accepted_o,
    output var logic [31:0] completed_o,
    // Product launches. Four per nonzero reciprocal, by law.
    output var logic [31:0] mul_jobs_o,
    // Zero phases. One per zero request, and NOT counted as products.
    output var logic [31:0] zero_jobs_o,
    // Every scheduled phase, products and zeros alike.
    output var logic [31:0] phase_jobs_o,
    // MX phases that took the negative high-word correction. Zero over the
    // reciprocal's own domain -- see zhao_raster_rcp24_mul's header -- so this
    // counter exists to say so honestly rather than to look busy.
    output var logic [31:0] negcorr_jobs_o,
    output var logic [5:0]  occupancy_o,
    // Sticky: any queue overflowed or underflowed. Must stay low.
    output var logic        qerr_o
);

  localparam int unsigned CW = $clog2(NCTX);

  localparam logic [2:0] PH_MW0  = 3'd0;
  localparam logic [2:0] PH_MX0  = 3'd1;
  localparam logic [2:0] PH_MW1  = 3'd2;
  localparam logic [2:0] PH_MX1  = 3'd3;
  localparam logic [2:0] PH_ZERO = 3'd4;

  // Ticket = {phase, context}, and the multiplier's tag is the same thing. The
  // negative-correction flag does NOT ride along: it is counted where it is
  // decided, at operand selection, and carrying a bit that only a counter would
  // read is dead logic in a block whose whole point is area.
  localparam int unsigned TKW  = 3 + CW;
  localparam int unsigned TAGW = TKW;

  // ------------------------------------------------------------- planes -----
  // Payload: written once at admission, read-only afterwards.
  logic [23:0]     p_m_q    [NCTX];
  logic [31:0]     p_x0_q   [NCTX];
  logic [5:0]      p_k_q    [NCTX];
  logic            p_zero_q [NCTX];
  logic [TOKW-1:0] p_tok_q  [NCTX];
  // Scratch: written only by phase writeback.
  logic [31:0]     s_x_q    [NCTX];
  logic [31:0]     s_w_q    [NCTX];
  // The finished mantissa, held until the caller takes it.
  logic [23:0]     res_q    [NCTX];

  // ---------------------------------------------------------- free queue ----
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

  // ----------------------------------------------------------- NEW queue ----
  // S10.2: "Admission and a returning phase may both enqueue in one clock, so
  // NEW and CONTINUATION have separate single writers."
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

  // ---------------------------------------------------- CONTINUATION queue --
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

  // ---------------------------------------------------------- DONE queue ----
  // The completion scan, replaced. A context reaches this queue exactly once.
  logic          done_push, done_pop, done_empty, done_full, done_err;
  logic [CW-1:0] done_din, done_dout;
  zhao_raster_ticketq #(
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

  // ======================================================== ADMISSION =======
  // S10.3: "Reserve an execution context before admitting work into a
  // nonstallable seed pipeline. The ROM result is not allowed to appear without
  // a payload write slot." The context is popped from the free queue in the
  // same clock the request is taken, so the two stages below can never find
  // themselves without a row to write.
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

  // Normalisation, transcribed from the serial block priority scan and all, so
  // the exponent convention cannot drift. S10.3 permits a balanced leading-zero
  // network but requires the same zero/nonzero convention; the balanced version
  // is a later, measured change and not a free one.
  logic [4:0]  e_c;
  logic [23:0] m_c;
  always_comb begin
    e_c = 5'd0;
    for (int unsigned b = 0; b < 24; ++b) begin
      if (a0_d_q[23-b] && (e_c == 5'd0) && !a0_d_q[23]) e_c = 5'(b);
    end
    m_c = a0_d_q << e_c;
  end

  // The same generated T24 the serial block and the Field engine use.
  logic [30:0] seed_c;
  zhao_field_rcp24_rom u_rom (
      .idx_i (a1_idx_q),
      .seed_o(seed_c)
  );

  // ========================================================= SCHEDULE =======
  // A small fair arbiter. Alternating priority rather than a fixed one: NEW is
  // bounded by the free queue and CONT is bounded by the contexts, so neither
  // can starve the other for long, but a fixed priority would still let a burst
  // of admissions sit behind a saturated continuation stream (or the reverse)
  // for the whole burst.
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

  // ---- s1: the popped ticket ------------------------------------------------
  logic           s1_v_q;
  logic [CW-1:0]  s1_ctx_q;
  logic [2:0]     s1_ph_q;

  // ---- s2: the payload/scratch read ----------------------------------------
  logic           s2_v_q;
  logic [CW-1:0]  s2_ctx_q;
  logic [2:0]     s2_ph_q;
  logic [23:0]    s2_m_q;
  logic [31:0]    s2_x_q;
  logic [31:0]    s2_w_q;

  // ---- O: operand selection, the only place the phase picks anything -------
  logic is_mw_c, is_mx_c, is_zero_c, neg_c;
  assign is_mw_c   = (s2_ph_q == PH_MW0) || (s2_ph_q == PH_MW1);
  assign is_mx_c   = (s2_ph_q == PH_MX0) || (s2_ph_q == PH_MX1);
  assign is_zero_c = (s2_ph_q == PH_ZERO);
  // S10.5: "The boundary w=2^31 is not negative and gives b32=0."
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
      // S10.5: b32 = (2^31 - w) mod 2^32, unsigned. w = 0 gives 2^31 and must
      // NOT be read as a negative signed 32-bit value.
      b_c = 32'h8000_0000 - s2_w_q;
      if (neg_c) corr_c = s2_x_q;
    end
  end

  logic [TAGW-1:0] tag_c;
  assign tag_c = {s2_ph_q, s2_ctx_q};

  // ---- the exact product unit ----------------------------------------------
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

  // ---- terminal rescale ----------------------------------------------------
  // S10.8: "For a terminal MX1, register x_next before the final (x_next+64)>>7
  // and clamp." x_next IS a register (the product unit's E stage), so the cone
  // below starts at a flop. 33 bits and not 32, per S10.6's "use sufficient
  // width for x+64, rather than accidentally wrapping at 32 bits".
  logic [32:0] resc7_c;
  assign resc7_c = {1'b0, mul_x_c} + 33'd64;

  logic [23:0] term_r_c;
  assign term_r_c = (w_ph_c == PH_ZERO)             ? 24'd0
                  : ((resc7_c >> 7) > 33'h00FF_FFFF) ? 24'hFF_FFFF
                  :                                    resc7_c[30:7];

  logic          t1_v_q;
  logic [CW-1:0] t1_ctx_q;
  logic [23:0]   t1_r_q;

  // ---- output --------------------------------------------------------------
  assign r_valid_o = !done_empty;
  assign r_o       = res_q[done_dout];
  assign k_o       = p_k_q[done_dout];
  assign d_zero_o  = p_zero_q[done_dout];
  assign r_tok_o   = p_tok_q[done_dout];

  logic retire_c;
  assign retire_c = r_valid_o && r_ready_i;

  // ---- queue drives --------------------------------------------------------
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
      default: cont_din = {PH_MX1, w_ctx_c};  // PH_MW1
    endcase
  end

  assign done_push = t1_v_q;
  assign done_din  = t1_ctx_q;
  assign done_pop  = retire_c;

  assign qerr_o = free_err | new_err | cont_err | done_err;

  // ======================================================= SEQUENTIAL =======
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
      // ---- accept ---------------------------------------------------------
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

      // ---- A0: normalise and index ----------------------------------------
      a1_v_q <= a0_v_q;
      if (a0_v_q) begin
        a1_ctx_q  <= a0_ctx_q;
        a1_m_q    <= m_c;
        a1_idx_q  <= 8'((m_c - 24'h80_0000) >> 15);
        a1_k_q    <= (a0_d_q == 24'd0) ? 6'd0 : (6'({1'b0, e_c}) + 6'd1);
        a1_zero_q <= (a0_d_q == 24'd0);
        a1_tok_q  <= a0_tok_q;
      end

      // ---- A1: ROM capture, payload write, NEW ticket ----------------------
      if (a1_v_q) begin
        p_m_q[a1_ctx_q]    <= a1_zero_q ? 24'd0 : a1_m_q;
        p_x0_q[a1_ctx_q]   <= a1_zero_q ? 32'd0 : {1'b0, seed_c};
        p_k_q[a1_ctx_q]    <= a1_k_q;
        p_zero_q[a1_ctx_q] <= a1_zero_q;
        p_tok_q[a1_ctx_q]  <= a1_tok_q;
      end

      // ---- Q: the granted ticket ------------------------------------------
      s1_v_q <= grant_new_c || grant_cont_c;
      if (grant_new_c || grant_cont_c) begin
        s1_ctx_q <= ticket_c[CW-1:0];
        s1_ph_q  <= ticket_c[CW+2:CW];
      end
      if (!new_empty && !cont_empty) pri_q <= !pri_q;

      // ---- R: payload and scratch read ------------------------------------
      s2_v_q <= s1_v_q;
      if (s1_v_q) begin
        s2_ctx_q <= s1_ctx_q;
        s2_ph_q  <= s1_ph_q;
        s2_m_q   <= p_m_q[s1_ctx_q];
        // MW0 reads the SEED; every later phase reads the scratch it wrote.
        s2_x_q   <= (s1_ph_q == PH_MW0) ? p_x0_q[s1_ctx_q] : s_x_q[s1_ctx_q];
        s2_w_q   <= s_w_q[s1_ctx_q];
      end

      // ---- O: launch counters ---------------------------------------------
      if (s2_v_q) begin
        phase_jobs_o <= phase_jobs_o + 32'd1;
        if (is_zero_c) zero_jobs_o <= zero_jobs_o + 32'd1;
        else           mul_jobs_o  <= mul_jobs_o + 32'd1;
        if (neg_c)     negcorr_jobs_o <= negcorr_jobs_o + 32'd1;
      end

      // ---- W: writeback ----------------------------------------------------
      t1_v_q <= mul_v_c && ((w_ph_c == PH_MX1) || (w_ph_c == PH_ZERO));
      if (mul_v_c) begin
        case (w_ph_c)
          PH_MW0: begin
            // The ONLY writeback that initialises the whole scratch row.
            s_w_q[w_ctx_c] <= mul_w_c;
            s_x_q[w_ctx_c] <= p_x0_q[w_ctx_c];
          end
          PH_MX0:  s_x_q[w_ctx_c] <= mul_x_c;
          PH_MW1:  s_w_q[w_ctx_c] <= mul_w_c;
          default: begin  // PH_MX1 and PH_ZERO are terminal
            t1_ctx_q <= w_ctx_c;
            t1_r_q   <= term_r_c;
          end
        endcase
      end

      // ---- terminal commit -------------------------------------------------
      if (t1_v_q) res_q[t1_ctx_q] <= t1_r_q;

      // ---- retire ----------------------------------------------------------
      if (retire_c) completed_o <= completed_o + 32'd1;
    end
  end

endmodule : zhao_raster_rcp24_v3

`default_nettype wire
