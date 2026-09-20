// zhao_measure_starve.sv -- the per-frame, per-view STARVATION VERDICT:
// MEASURE.TOKENS' one-cycle denial port turned into the held, per-frame,
// per-view bit MEASURE.GOVERNOR's `starved0/1_i` asks for.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS AT ALL
// ---------------------------------------------------------------------------
// `zhao_console_core.sv`'s I18 entry names this block, in these words, as the
// reason MEASURE.GOVERNOR cannot be composed:
//
//   > Its `starved0/1_i` would come from TOKENS' `den_*`, which is a one-cycle
//   > REGISTERED denial while the governor wants a per-frame per-view verdict:
//   > THE LATCH BETWEEN THEM IS STATE, AND STATE BELONGS IN A FILE WITH A
//   > CONTRACT AND A TEST.
//
// This is that file. It is deliberately NOT written inside the composer, for
// exactly the reason the entry gives: a composer may write a join whose two
// sides are the same cycle's wires, and may not write a register.
//
// ---------------------------------------------------------------------------
// THE LAW IS READ, NOT CHOSEN
// ---------------------------------------------------------------------------
// `zhao_measure_governor.sv`'s own `starved0/1_i` port comment states the law:
//
//   > MEASURE.TOKENS is the natural producer: a view was starved iff it had a
//   > request denied AGAINST ITS OWN GUARANTEED POOL.
//
// `zhao_measure_tokens.sv:303-305` is the reason encoding, and it already
// separates exactly that:
//
//   REASON_LOW_PRIORITY (0)  "private short, not essential"
//   REASON_EXHAUSTED    (1)  "essential, but shared short too"
//   REASON_RELOAD       (2)  "a budget load landed this cycle"
//   3                        unused and never presented
//
// Reasons 0 and 1 are both denials in which THE VIEW'S OWN PRIVATE POOL WAS
// SHORT -- that is what "private short" and "essential, but shared short too"
// each say. Reason 2 is law T9's protocol refusal: the request arrived in the
// cycle a budget load landed, and the block refuses it rather than dropping it
// silently. T9's own comment says "the frame-boundary protocol makes that
// unreachable in real traffic; it is defined anyway". A RELOAD denial is
// therefore a statement about a COLLISION, not about a budget, and counting it
// as starvation would let a budget load degrade a view that was never short.
// That would be a governor responding to its own frame boundary.
//
// So the classification is LIFTED from MEASURE.TOKENS' encoding rather than
// invented here. It is still a knob, because CLAUDE.md rule 6 says a value
// belongs in a named, editable constant even when it was derived: see
// `STARVE_REASON_MASK`.
//
// ---------------------------------------------------------------------------
// THE FRAME BOUNDARY, AND WHY THE OUTPUT IS THE ACCUMULATOR ITSELF
// ---------------------------------------------------------------------------
// The governor and this block take THE SAME `frame_i` pulse. The governor's
// port comment says `frame_i` is "a one-cycle pulse at the frame boundary:
// decide now", and its `starved0/1_i` is "the per-view verdict for THE FRAME
// JUST ENDED". Both of those must be true on the same cycle.
//
// So `starved0/1_o` ARE the accumulators, presented continuously, and the
// clear happens on the frame pulse as a non-blocking assignment. On the pulse
// cycle the governor therefore samples everything this frame accumulated, and
// the accumulator is empty from the next cycle on. A design that registered
// `starved_o <= acc` at the pulse would hand the governor the PREVIOUS frame's
// verdict -- one frame late, and late in the flattering direction, because a
// view that has just stopped being starved would still be degraded.
//
// A DENIAL IN THE SAME CYCLE AS THE PULSE BELONGS TO THE NEW FRAME. The pulse
// is the boundary; the verdict the governor reads is everything strictly
// before it. The alternative (fold it into the ending frame) would make the
// bit the governor is sampling change in the cycle it samples it, which is the
// join this repository's own "detector wired to two operands that move
// together" chapter is about. Named here rather than left to be re-derived.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT
// ---------------------------------------------------------------------------
// It is not a policy. It does not know what a rung is, it never looks at
// `den_class_o`, `den_rep_o`, `den_cost_o` or `den_src_id_o`, and it has no
// opinion about what the governor should do with the bit. Law G3 of the
// governor -- each view's degrade is a function of that view's own pressure
// and nothing else -- is preserved STRUCTURALLY here too: there is no path in
// this file from a view-1 denial to `starved0_o`.
//
// Conservative SystemVerilog subset only (charter section 2). No function-call
// result is indexed anywhere in this file (Quartus 17.0 rejects `f(x)[7:0]`).
`default_nettype none

module zhao_measure_starve #(
    // Which of MEASURE.TOKENS' four reason codes count as STARVATION, as a
    // one-bit-per-reason mask. NAMED AND EDITABLE (CLAUDE.md rule 6) even
    // though it is derived: bit 0 = REASON_LOW_PRIORITY, bit 1 =
    // REASON_EXHAUSTED, bit 2 = REASON_RELOAD, bit 3 = the unused code.
    //
    // The default 4'b0011 is the reading argued in the header. If the owner
    // decides a RELOAD collision IS budget pressure, this is a one-line change
    // and `reload_ignored_o` already says how often it would have mattered --
    // which is the whole reason that counter exists separately.
    parameter logic [3:0] STARVE_REASON_MASK = 4'b0011
) (
    input var logic clk,
    input var logic rst_n,

    // -----------------------------------------------------------------------
    // The frame boundary. The SAME pulse MEASURE.GOVERNOR takes.
    // -----------------------------------------------------------------------
    input var logic frame_i,

    // -----------------------------------------------------------------------
    // MEASURE.TOKENS' denial port, OBSERVED. There is no `ready` and nothing
    // here is a term in that block's handshake: `den_valid_o` is a registered
    // one-cycle pulse (its ledger's `latency: fixed:1`) and this block is a
    // listener. It cannot stall the token pools, which is the same property
    // POST.GATHER's R5 rule states for the resolve path.
    // -----------------------------------------------------------------------
    input var logic       den_valid_i,
    input var logic       den_view_i,
    input var logic [1:0] den_reason_i,

    // -----------------------------------------------------------------------
    // The held verdict. Valid EVERY cycle; the governor samples it on the
    // frame pulse (see the header).
    // -----------------------------------------------------------------------
    output var logic starved0_o,
    output var logic starved1_o,

    // -----------------------------------------------------------------------
    // Evidence. Every one of these is reachable with legal stimulus -- there
    // is no guard here that needs a committed mutant.
    // -----------------------------------------------------------------------
    output var logic [31:0] denials_seen_o,     // every den_valid_i, any reason
    output var logic [31:0] starving_denials_o, // those the mask counts
    output var logic [31:0] reload_ignored_o,   // those the mask deliberately drops
    output var logic [31:0] frames_o,           // frame_i pulses
    output var logic [31:0] starved_frames0_o,  // frames presented starved, view 0
    output var logic [31:0] starved_frames1_o
);

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  // MEASURE.TOKENS' encoding, restated here so the mask above is readable.
  // These are NOT a second definition of the law: they are that block's
  // localparams, cited by file and line in the header, and the elaboration
  // check below is what keeps the mask honest about their width.
  localparam logic [1:0] REASON_RELOAD = 2'd2;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint
  // (CLAUDE.md, "Verilator lint-clean is not Quartus-synthesizable"). And
  // `--lint-only` does not run this block, so a clean lint is NOT evidence
  // about it -- it is fired by parameter override in the directed test.
  initial begin
    if (STARVE_REASON_MASK == 4'b0000)
      $fatal(1, "zhao_measure_starve: STARVE_REASON_MASK is zero; the verdict could never fire");
  end

  // ---- the two accumulators (the governor's law G3, structurally) ----------
  // Two registers, never cross-wired. There is deliberately no array and no
  // loop: a `for` over views is how a view index becomes a variable, and a
  // variable view index is how one player's denial reaches the other player's
  // rung.
  logic acc0_r, acc1_r;

  assign starved0_o = acc0_r;
  assign starved1_o = acc1_r;

  // Does THIS cycle's denial count? One decode, shared by both views, because
  // the reason encoding is not per view.
  logic counts_c;
  always_comb begin
    counts_c = den_valid_i && STARVE_REASON_MASK[den_reason_i];
  end

  logic hit0_c, hit1_c;
  always_comb begin
    hit0_c = counts_c && !den_view_i;
    hit1_c = counts_c && den_view_i;
  end

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      acc0_r <= 1'b0;
      acc1_r <= 1'b0;
    end else if (frame_i) begin
      // The boundary: the ending frame's verdict has just been read on this
      // same cycle, so the accumulator restarts holding only what arrived NOW.
      acc0_r <= hit0_c;
      acc1_r <= hit1_c;
    end else begin
      acc0_r <= acc0_r | hit0_c;
      acc1_r <= acc1_r | hit1_c;
    end
  end

  // ---- evidence ------------------------------------------------------------
  // Saturating, per house style: a counter that wraps reports a smaller number
  // than the truth, which is the flattering direction.
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      denials_seen_o     <= 32'd0;
      starving_denials_o <= 32'd0;
      reload_ignored_o   <= 32'd0;
      frames_o           <= 32'd0;
      starved_frames0_o  <= 32'd0;
      starved_frames1_o  <= 32'd0;
    end else begin
      if (den_valid_i && denials_seen_o != CNT_MAX)
        denials_seen_o <= denials_seen_o + 32'd1;

      if (counts_c && starving_denials_o != CNT_MAX)
        starving_denials_o <= starving_denials_o + 32'd1;

      // The deliberate exclusion, counted so its silence is visible. If this
      // reads zero forever it is because law T9's collision never happens in
      // real traffic, which is what T9 itself predicts -- but "predicted zero"
      // and "measured zero" are different claims and this is the second one.
      if (den_valid_i && den_reason_i == REASON_RELOAD
          && !STARVE_REASON_MASK[REASON_RELOAD] && reload_ignored_o != CNT_MAX)
        reload_ignored_o <= reload_ignored_o + 32'd1;

      if (frame_i) begin
        if (frames_o != CNT_MAX) frames_o <= frames_o + 32'd1;
        // Sampled at the pulse: the verdict the governor is reading THIS
        // cycle, which is the accumulator before the clear above takes effect.
        if (acc0_r && starved_frames0_o != CNT_MAX)
          starved_frames0_o <= starved_frames0_o + 32'd1;
        if (acc1_r && starved_frames1_o != CNT_MAX)
          starved_frames1_o <= starved_frames1_o + 32'd1;
      end
    end
  end

endmodule

`default_nettype wire
