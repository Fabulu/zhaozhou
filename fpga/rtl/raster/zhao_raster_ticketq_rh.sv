// zhao_raster_ticketq_rh.sv -- a ticket queue with a REGISTERED head.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// S01 §16.2 names the defect and this repository's own comment admits it:
//
//   > `zhao_raster_ticketq`'s commentary says a FIFO head is a register. Its
//   > actual output is `assign dout_o = mem_q[head_q];` That is a dynamically
//   > indexed combinational output from a flop array.
//
// Line 18 of that file really does say "A FIFO's head is a register", and line
// 64 really is the combinational index. The fit agrees: every worst path in
// `zhao_raster_rcp24_v3@v3-full` starts at `u_doneq|mem_q[..]` or
// `u_doneq|head_q[..]` and ends at `r_tok_o` / `r_o`, and the block reports
// 90.54 MHz against a core-to-core 129.18.
//
// §16.3 asks for "a real registered head and a spare/prefetch slot, with exact
// queue occupancy and context-credit conservation", and §16.2 says to "start
// with the DONE queue/output seam. Do not rewrite the multiplier first."
//
// A WRAPPER, NOT A REWRITE OF THE SHARED BODY. §16.4: "Do not replace every
// ticket queue blindly. FREE, NEW, CONTINUATION and DONE have different traffic
// and initialization." `zhao_raster_ticketq` is instantiated four times; only
// the DONE instance sits on the measured path. Wrapping leaves the other three
// byte-identical instead of asking a diff to prove they are unaffected.
//
// TWO HEADS, BECAUSE ONE WOULD COST THE THROUGHPUT GATE. With a single
// registered head a back-to-back pop finds nothing: the body read lands a cycle
// late. `raster_rcp24_v3_directed` asserts "a saturated V3 tile costs under 4.6
// clocks per reciprocal" and "not under four, which would mean a launch was
// skipped", and §16.1 says to retain the four-clock rate. The head/spare pair
// with a reserved pending-read destination is the structure that sustains one
// pop per clock -- the same shape as `zhao_texture_v3rq`, whose sustained rate
// was MEASURED on 2026-09-07 at zero bubbles in 400 cycles under continuous
// supply, with the bubble counter itself fire-tested.
//
// OCCUPANCY COUNTS THE HEADS. §5.3's law, learned the expensive way in the
// texture queue: "a body of 64 plus two heads must not silently advertise 66
// logical owner credits". `full_o` here is driven from a logical count that
// includes the body, both heads and the read in flight, so the wrapper never
// advertises more credit than the ring has.
`default_nettype none

module zhao_raster_ticketq_rh #(
    parameter int unsigned W = 8,
    parameter int unsigned D = 16,
    // Reset leaves the queue FULL, holding 0, 1, ... D-1 -- passed through to
    // the body, which is where preload actually happens.
    parameter bit PRELOAD = 1'b0
) (
    input var logic clk,
    input var logic rst_n,

    input var logic         push_i,
    input var logic [W-1:0] din_i,
    input var logic         pop_i,

    output var logic [W-1:0] dout_o,
    output var logic         empty_o,
    output var logic         full_o,
    output var logic         err_o
);

  localparam int unsigned PW = $clog2(D);

  // ---- the unmodified body -------------------------------------------------
  logic         b_pop_c;
  logic [W-1:0] b_dout_c;
  logic         b_empty_c, b_full_c, b_err_c;

  zhao_raster_ticketq #(
      .W      (W),
      .D      (D),
      .PRELOAD(PRELOAD)
  ) u_body (
      .clk    (clk),
      .rst_n  (rst_n),
      .push_i (push_i && !full_o),
      .din_i  (din_i),
      .pop_i  (b_pop_c),
      .dout_o (b_dout_c),
      .empty_o(b_empty_c),
      .full_o (b_full_c),
      .err_o  (b_err_c)
  );

  // ---- head and spare ------------------------------------------------------
  logic         h_v_q, s_v_q;
  logic [W-1:0] h_d_q, s_d_q;

  assign dout_o  = h_d_q;
  assign empty_o = !h_v_q;

  logic pop_taken_c;
  assign pop_taken_c = pop_i && h_v_q;

  // §5.4: "A body read may launch only if there is a head/staging position
  // reserved for its return. The reservation survives backpressure. Decide the
  // launch using the post-pop head occupancy, then reserve a position for the
  // new pending result."
  //
  // The body here is a COMBINATIONAL array, so its read returns in the same
  // cycle the pop is taken -- there is no separate in-flight registration to
  // count, which is the one way this differs from the texture queue.
  logic [1:0] reserved_c;
  assign reserved_c = {1'b0, h_v_q} + {1'b0, s_v_q};
  assign b_pop_c    = !b_empty_c && ((reserved_c - {1'b0, pop_taken_c}) < 2'd2);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      h_v_q <= 1'b0;
      s_v_q <= 1'b0;
      h_d_q <= '0;
      s_d_q <= '0;
    end else begin
      logic       n_h_v, n_s_v;
      logic [W-1:0] n_h_d, n_s_d;
      n_h_v = h_v_q;  n_h_d = h_d_q;
      n_s_v = s_v_q;  n_s_d = s_d_q;

      if (pop_taken_c) begin
        // the spare promotes into the head
        n_h_v = s_v_q;  n_h_d = s_d_q;
        n_s_v = 1'b0;
      end
      if (b_pop_c) begin
        if (!n_h_v) begin
          n_h_v = 1'b1;  n_h_d = b_dout_c;
        end else begin
          n_s_v = 1'b1;  n_s_d = b_dout_c;
        end
      end

      h_v_q <= n_h_v;  h_d_q <= n_h_d;
      s_v_q <= n_s_v;  s_d_q <= n_s_d;
    end
  end

  // ---- LOGICAL occupancy: body + heads -------------------------------------
  // §5.3, and the reason `full_o` is not the body's own flag: the heads hold
  // real tickets, so advertising the body's freedom would over-issue credit by
  // up to two.
  logic [PW+1:0] lcnt_q;
  logic          push_taken_c;
  assign push_taken_c = push_i && !full_o;
  assign full_o       = (lcnt_q >= (PW + 2)'(D));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) lcnt_q <= PRELOAD ? (PW + 2)'(D) : '0;
    else        lcnt_q <= lcnt_q + (PW + 2)'(push_taken_c) - (PW + 2)'(pop_taken_c);
  end

  // The body's own error flag plus this wrapper's contract violations.
  logic err_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) err_q <= 1'b0;
    else if ((push_i && full_o) || (pop_i && empty_o)) err_q <= 1'b1;
  end
  assign err_o = err_q || b_err_c;

  // Unused: the body's own full flag. The wrapper's logical count is the
  // authority, and sinking it explicitly keeps `-Wall` honest about the fact
  // that the two are deliberately different questions.
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_body_full;
  assign unused_body_full = b_full_c;
  /* verilator lint_on UNUSEDSIGNAL */

`ifndef SYNTHESIS
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q) begin
      // The spare may only be occupied when the head is.
      a_rh_head_before_spare : assert (!s_v_q || h_v_q);
      // §5.5: a counter crossing its legal range is a design error that must
      // fire a detector, not saturate.
      a_rh_lcnt_in_range : assert (lcnt_q <= (PW + 2)'(D));
    end
  end
`endif

endmodule : zhao_raster_ticketq_rh

`default_nettype wire
