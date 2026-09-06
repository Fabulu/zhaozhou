// zhao_texture_aux_pipe.sv — AUX as a pipeline: II=1 accept, II=1 sheet request.
//
// BESIDE `zhao_texture_aux.sv`, which stays the block of record. Nothing
// instantiates this yet.
//
// ---------------------------------------------------------------------------
// WHY
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md:
//
//   > The current AUX FSM performs three restoring quotient bits in one state,
//   > three in another, issues one Surface Sheet request, waits for the answer,
//   > presents it, and only then accepts another fragment. Its nominal
//   > one-per-six rate is about 277,778/frame, almost exactly the 276,480
//   > estimate.
//
// "Almost exactly the estimate" is the problem. A block sized at 1.005x its
// own demand has no reserve for a cache miss, a queue bubble or a frame that
// is slightly busier than the model.
//
//   A0  accept, numerator/divisor, classify neg/sat/degenerate
//   A1..A6  the six restoring bits -- zhao_texture_aux_div6, ALREADY VERIFIED
//   A7  form texel index, enqueue Surface Sheet read
//   A8  capture registered sheet response
//   A9  enqueue tokenized AUX return
//
// ---------------------------------------------------------------------------
// TWO THINGS TRANSCRIBED RATHER THAN REINVENTED
// ---------------------------------------------------------------------------
// A0's arithmetic is copied from zhao_texture_aux.sv character for character:
//
//     degen = (env_x1 <= env_x0) || (env_z1 <= env_z0)
//     du    = env_x1 - env_x0                     (unsigned)
//     nu    = (NUM_W'(signed(wx)) - NUM_W'(signed(env_x0))) <<< 6
//     neg   = nu[NUM_W-1]
//     sat   = !neg && unsigned(nu) >= {2'b00, du, 6'b0}      i.e. nu >= 64*du
//     r_in  = (neg || sat) ? 0 : nu
//     tex   = sat ? 63 : quotient
//
// The two clamps stay OUTSIDE the divider, which is what lets the divide be
// exactly six steps -- the same contract zhao_texture_aux_div6 documents and
// its test enforces.
//
// AND THE DEGENERATE CASE STILL TRAVELS. The brief is explicit: "A degenerate
// envelope travels through the ordering machinery but emits no sheet read."
// Dropping it at A0 would be simpler and would silently lose a fragment's
// completion, so it walks the same queues as everything else -- see ORDERING
// below for what that does and does not promise.
//
// ---------------------------------------------------------------------------
// THE 2026-09-06 HANDSHAKE REPAIR -- WHAT WAS WRONG, AND WHAT REPLACED IT
// ---------------------------------------------------------------------------
// reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt section 4,
// and reports/TEXTURE-ISLAND-PREFIT-ADDENDUM-20260906.txt section 6. Three
// verified defects, all of them one mistake in three costumes: A FIXED-LATENCY
// PRODUCER WAS TREATED AS PROOF THAT ITS SINKS ARE INFINITE.
//
//   1. THE SHEET OFFER WAS NOT HELD. `sheet_ready_i` appeared in exactly one
//      place in this file -- an evidence counter -- and had no influence on
//      the A7 register or any part of its payload. A7 was an unconditional
//      one-cycle shadow of `div_ov`, so an offer's lifetime was one clock
//      whether or not the sheet took it. Three loss modes: the offer
//      EVAPORATES (and the FRAGROB slot then waits forever -- a hang); the
//      offer MUTATES while valid is high (a ready/valid protocol violation on
//      its own); or a degenerate arriving the next cycle drops `sheet_valid_o`
//      AND pushes itself to the return queue, completing ahead of the request
//      it just destroyed.
//
//      REPLACED BY a bounded holding FIFO whose HEAD IS THE OFFER. Valid and
//      payload are stable to the acceptance edge BY CONSTRUCTION -- the read
//      pointer is the only thing that can move them, and it moves only on an
//      accepted handshake. A held offer should not be a property maintained by
//      careful case analysis; it should be a property of the structure.
//
//   2. THE RETURN QUEUE HAD NO RESERVATION. It pushed unconditionally, with no
//      full term and no admission gate. Peak push is 2/cycle (a sheet response
//      and a degenerate together) against a pop of 1, and there is NO
//      `sheet_rready_o` port at all, so the leaf MUST sink every response.
//      Worse, `rqcnt` was 4 bits for an 8-deep queue: it could reach 15, wrap
//      on a +2 and land on 0 with entries still resident -- at which point
//      `out_valid_o` goes false and the queue silently stops delivering.
//
//      REPLACED BY a queue sized to the credit (so the bound is a counting
//      argument, not an average-occupancy hope), a count wide enough to hold
//      its whole range without wrapping, and an explicit admission gate in
//      which THE SHEET RESPONDER HAS ABSOLUTE PRIORITY for a free slot -- it
//      has no ready line, so it cannot be the one that waits. The degenerate
//      path is the one that waits, which it now CAN, because it sits in the
//      offer FIFO instead of in a register about to be overwritten.
//
//   3. `req_ready_o = 1'b1` reasoned from "the divider never stalls" to "the
//      input is always ready", skipping the three stages downstream that can.
//
//      REPLACED BY a credit, taken at admission and released ONLY at terminal
//      acceptance (`out_valid_o && out_ready_i`) -- never at the sheet
//      handshake, because a request handed to the sheet still owns an offer
//      slot, a return-queue slot and a side-channel entry. Every bounded
//      resource in the block is sized to that credit.
//
// ---------------------------------------------------------------------------
// ORDERING -- A CLAIM THIS FILE USED TO MAKE AND DOES NOT HOLD
// ---------------------------------------------------------------------------
// The old comment above the return queue said degenerates "enter the return
// queue directly, in the same order". THAT IS FALSE, and it was false before
// this repair. A degenerate completes on the cycle it reaches the offer head;
// a non-degenerate that has ALREADY BEEN ISSUED to the sheet waits an unbounded
// round trip. So a degenerate submitted later can, and routinely does, return
// first.
//
// What IS true, and is all that is claimed:
//   * offers reach the sheet in submission order -- one FIFO, one head;
//   * a degenerate never overtakes a request still WAITING to be offered,
//     because it queues behind that request in the same FIFO;
//   * every accepted request yields exactly one terminal result carrying its
//     own token.
// FRAGROB rejoins by token, so that is sufficient. If a consumer ever needs
// true submission order it has to be built and tested, not asserted in a
// comment.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_aux_pipe #(
    parameter int unsigned NUM_W = 40,
    parameter int unsigned DEN_W = 32,
    parameter int unsigned REM_W = 39,
    parameter int unsigned TOKW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- accept -------------------------------------------------------------
    input  var logic                     req_valid_i,
    output var logic                     req_ready_o,
    input  var logic signed [31:0]       req_wx_i,
    input  var logic signed [31:0]       req_wz_i,
    input  var logic signed [31:0]       req_env_x0_i,
    input  var logic signed [31:0]       req_env_x1_i,
    input  var logic signed [31:0]       req_env_z0_i,
    input  var logic signed [31:0]       req_env_z1_i,
    input  var logic [TOKW-1:0]          req_tok_i,

    // ---- Surface Sheet read -------------------------------------------------
    output var logic                     sheet_valid_o,
    input  var logic                     sheet_ready_i,
    output var logic [5:0]               sheet_u_o,
    output var logic [5:0]               sheet_v_o,
    output var logic [TOKW-1:0]          sheet_tok_o,
    input  var logic                     sheet_rvalid_i,
    input  var logic [7:0]               sheet_tag_i,
    input  var logic [7:0]               sheet_str_i,
    input  var logic [TOKW-1:0]          sheet_rtok_i,

    // ---- the tokenized AUX return -------------------------------------------
    output var logic                     out_valid_o,
    input  var logic                     out_ready_i,
    output var logic [TOKW-1:0]          out_tok_o,
    output var logic [7:0]               out_tag_o,
    output var logic [7:0]               out_str_o,
    output var logic                     out_degenerate_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]              accepted_o,
    output var logic [31:0]              sheet_reads_o,
    output var logic [31:0]              degenerate_o
);

  // =================================================== THE TRANSACTION CREDIT
  // ONE number bounds the whole block. A credit is taken when a request is
  // admitted and released when its result is ACCEPTED by the consumer, so a
  // single count covers, simultaneously:
  //
  //     admitted A0/A0b work                   (two stages)
  //     work inside the divider                (six stages, non-stallable)
  //     divider results awaiting an offer      (the offer FIFO)
  //     issued sheet reads awaiting a response (unbounded latency)
  //     finished results awaiting FRAGROB      (the return queue)
  //
  // Every one of those is sized to CREDIT, which is what turns "the
  // non-stallable divider cannot meet a full sink" into a counting argument.
  // The addendum asks for exactly this and is explicit that a smaller queue
  // would need a separate proof, and that average occupancy is not that proof.
  //
  // WHY 16 AND NOT 8. The minimum round trip with a one-clock sheet is ten
  // clocks (A0, A0b, six divider stages, offer head, handshake, response,
  // return queue, presented), so a credit of 8 would throttle acceptance below
  // the II=1 this block exists to provide. 16 keeps II=1 with six clocks of
  // slack.
  //
  // It also happens to equal SIDE_N, and that IS a coincidence -- see the note
  // at the side table, which is protected by the divider's issue rate and not
  // by this credit. Two numbers being equal is not two numbers being linked,
  // and writing "not a coincidence" over a coincidence is how a free variable
  // becomes an unmovable one.
  localparam int unsigned CREDIT   = 16;
  localparam int unsigned CREDIT_W = $clog2(CREDIT + 1);   // holds 0..CREDIT

  logic [CREDIT_W-1:0] credit_q;

  // ======================================================================= A0
  // ---- WHY THERE IS A REGISTER HERE THAT WAS NOT HERE BEFORE ---------------
  // MEASURED, 2026-09-03. This block fitted at 54.95 MHz -- the slowest thing
  // on the texture island by 7 MHz and 95 MHz below the 150 MHz leaf target --
  // and the setup report named the path rather than leaving it to be guessed:
  //
  //   req_env_x1_i[20] -> zhao_texture_aux_div6:u_div|ru_q[0][11]   -8.199 ns
  //
  // THE PATH STARTS AT AN INPUT PORT. Everything below used to be one
  // combinational cone from this block's boundary into the divider's first
  // flop: two 32-bit subtracts, a widening shift, a wide unsigned magnitude
  // compare and a mux. The block had five stages and no register at the seam
  // where it meets the world, which is the one place a leaf cannot control
  // what it is handed.
  //
  // The cone is now split in two by a register:
  //
  //   A0   ports -> subtract and shift        -> a0_* registers
  //   A0b  a0_*  -> clamp compare and mux     -> the divider's input flop
  //
  // It costs ONE clock of latency and nothing else: this is a pipeline stage
  // rather than a skid buffer, and II stays 1. What refuses a request when the
  // machine is full is the CREDIT above, not this register.
  logic                     degen_c;
  logic [DEN_W-1:0]         du_c, dv_c;
  logic signed [NUM_W-1:0]  nu_c, nv_c;

  always_comb begin
    degen_c = (req_env_x1_i <= req_env_x0_i) || (req_env_z1_i <= req_env_z0_i);

    du_c = $unsigned(req_env_x1_i) - $unsigned(req_env_x0_i);
    dv_c = $unsigned(req_env_z1_i) - $unsigned(req_env_z0_i);

    nu_c = (NUM_W'($signed(req_wx_i)) - NUM_W'($signed(req_env_x0_i))) <<< 6;
    nv_c = (NUM_W'($signed(req_wz_i)) - NUM_W'($signed(req_env_z0_i))) <<< 6;
  end

  logic                     a0_v_q, a0_degen_q;
  logic [DEN_W-1:0]         a0_du_q, a0_dv_q;
  logic signed [NUM_W-1:0]  a0_nu_q, a0_nv_q;
  logic [TOKW-1:0]          a0_tok_q;

  // ====================================================================== A0b
  // The clamps the divider relies on. Applied HERE, outside the divider, so
  // the quotient is in [0,63] by construction and six steps suffice -- the
  // rule is unchanged, only which side of a register it sits on.
  // ENFORCED-BY: tests/texture/texture_aux_pipe_directed.cpp
  logic negu_c, negv_c, satu_c, satv_c;
  always_comb begin
    negu_c = a0_nu_q[NUM_W-1];
    negv_c = a0_nv_q[NUM_W-1];
    satu_c = !negu_c && ($unsigned(a0_nu_q) >= {2'b00, a0_du_q, 6'b000000});
    satv_c = !negv_c && ($unsigned(a0_nv_q) >= {2'b00, a0_dv_q, 6'b000000});
  end

  // A side channel carries what the divider does not need. It is indexed by
  // the SAME token the divider echoes, so nothing has to be kept in step by
  // counting clocks -- that is the whole reason the divider carries a tag.
  //
  // ITS LIFETIME IS BOUNDED BY THE DIVIDER'S ISSUE RATE, NOT BY THE CREDIT.
  // Worth stating plainly, because "the credit protects it" is the
  // comfortable answer and it is the wrong one: a credit is released at
  // TERMINAL acceptance, and terminal acceptance is out of order (a degenerate
  // overtakes a request stuck at the sheet), so a freed credit says nothing
  // about which slot is safe. The real argument needs neither:
  //
  //   slot k is written when item k is ISSUED to the divider, at clock T_k;
  //   slot k is read at item k's divider output, at T_k + 6;
  //   slot k is rewritten when item k+SIDE_N is issued, and issues are at most
  //   one per clock and strictly in order, so that is no earlier than
  //   T_k + 16.
  //
  // 16 > 6 with ten clocks to spare, and the margin is against SIDE_N, not
  // against CREDIT. The addendum makes the same point from the other side --
  // the side table is not automatically the bug, and it should not be grown to
  // 64 merely because the island has 64 owners.
  localparam int unsigned SIDE_N = 16;
  logic            sd_degen [SIDE_N];
  logic            sd_satu  [SIDE_N];
  logic            sd_satv  [SIDE_N];
  logic [TOKW-1:0] sd_tok   [SIDE_N];

  logic [3:0] sd_wp;

  // ---- ACCEPT ONLY AGAINST A CREDIT ---------------------------------------
  // The line here used to be `assign req_ready_o = 1'b1;`, reasoning that "the
  // divider is a fixed-latency pipeline with no stalls, so this never depends
  // on the sheet or the output". The premise is true and the conclusion does
  // not follow: the sheet and the consumer are ready/valid interfaces that CAN
  // stall, and three stages of this block sit downstream of the divider.
  // Readiness is a statement about the whole machine, not about one stage.
  //
  // This depends on a REGISTER only, so no input reaches this output
  // combinationally and the boundary that cost 95 MHz stays clean.
  assign req_ready_o = (credit_q != CREDIT_W'(CREDIT));

  logic admit_c;
  assign admit_c = req_valid_i && req_ready_o;

  // ================================================================= A1..A6
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] nc_div_issued;
  logic [3:0]  nc_div_occ;
  /* verilator lint_on UNUSEDSIGNAL */

  logic            div_ov;
  logic [5:0]      div_qu, div_qv;
  logic [3:0]      div_tag;

  zhao_texture_aux_div6 #(
      .REM_W(REM_W),
      .DEN_W(DEN_W),
      .TAGW (4)
  ) u_div (
      .clk        (clk),
      .rst_n      (rst_n),
      .in_valid_i (a0_v_q),
      .in_ru_i    ((negu_c || satu_c) ? {REM_W{1'b0}} : REM_W'($unsigned(a0_nu_q))),
      .in_du_i    (a0_du_q),
      .in_rv_i    ((negv_c || satv_c) ? {REM_W{1'b0}} : REM_W'($unsigned(a0_nv_q))),
      .in_dv_i    (a0_dv_q),
      .in_tag_i   (sd_wp),
      .out_valid_o(div_ov),
      .out_qu_o   (div_qu),
      .out_qv_o   (div_qv),
      .out_tag_o  (div_tag),
      .issued_o   (nc_div_issued),
      .occupancy_o(nc_div_occ)
  );

  // ======================================================================= A7
  // The texel index, and the sheet read. A degenerate envelope produces NO
  // read but still occupies a return slot.
  logic [5:0] tex_u_c, tex_v_c;
  assign tex_u_c = sd_satu[div_tag] ? 6'd63 : div_qu;
  assign tex_v_c = sd_satv[div_tag] ? 6'd63 : div_qv;

  // ---- THE OFFER FIFO: ITS HEAD IS THE OFFER -------------------------------
  // Not "a register that is usually stable" -- a queue head. `sheet_valid_o`
  // and every `sheet_*_o` payload bit are functions of `off_rp` and the array
  // contents, and `off_rp` advances only when the head is consumed. Nothing
  // arriving from the divider can disturb an offer in flight, because the
  // divider writes at `off_wp`, which is never `off_rp` while the queue is
  // non-empty.
  //
  // DEGENERATES SIT IN THIS SAME FIFO. They emit no sheet read, but putting
  // them here is what gives them somewhere to WAIT when the return queue has
  // no room for them -- and it is what stops one from overtaking a request
  // that has not been offered yet.
  //
  // Sized to CREDIT: an entry here holds a credit, so the divider -- which
  // cannot be told to stall -- can never find this full.
  localparam int unsigned OFF_N  = CREDIT;
  localparam int unsigned OFF_AW = $clog2(OFF_N);
  localparam int unsigned OFF_CW = $clog2(OFF_N + 1);

  logic [5:0]        off_u   [OFF_N];
  logic [5:0]        off_v   [OFF_N];
  logic [TOKW-1:0]   off_tok [OFF_N];
  logic              off_deg [OFF_N];
  logic [OFF_AW-1:0] off_wp, off_rp;
  logic [OFF_CW-1:0] off_cnt;

  logic off_head_v, off_head_degen;
  assign off_head_v     = (off_cnt != OFF_CW'(0));
  assign off_head_degen = off_deg[off_rp];

  assign sheet_valid_o = off_head_v && !off_head_degen;
  assign sheet_u_o     = off_u[off_rp];
  assign sheet_v_o     = off_v[off_rp];
  assign sheet_tok_o   = off_tok[off_rp];

  // ==================================================================== A8/A9
  // THE RETURN QUEUE. Two producers, one consumer -- and the two producers are
  // NOT equals:
  //
  //   the sheet response has NO ready line (there is no `sheet_rready_o` port
  //   on this module at all), so the leaf is obliged to sink it on the cycle
  //   it arrives. It therefore gets ABSOLUTE PRIORITY for a free slot.
  //
  //   the degenerate bypass sits at the head of the offer FIFO and can simply
  //   not be popped. It is the one that waits -- possible only because of that
  //   FIFO; the old single register had nowhere to wait.
  //
  // Sized to CREDIT so the bound is arithmetic: every resident entry holds a
  // credit, so `rqcnt` can never exceed CREDIT and the gate below can never
  // actually have to refuse the sheet. The gate is written and asserted
  // anyway, because a bound that is only true by an argument made elsewhere is
  // exactly the bound that goes quietly wrong when someone changes CREDIT.
  //
  // AND THE COUNT IS WIDE ENOUGH. The old `rqcnt` was 4 bits for an 8-deep
  // queue: it could reach 15, wrap on a +2 and land on 0 with entries still
  // resident, dropping `out_valid_o` on a queue full of data. CREDIT_W holds
  // 0..CREDIT inclusive and cannot wrap.
  localparam int unsigned RQ_N  = CREDIT;
  localparam int unsigned RQ_AW = $clog2(RQ_N);
  localparam int unsigned RQ_CW = $clog2(RQ_N + 1);

  logic [TOKW-1:0]  rq_tok [RQ_N];
  logic [7:0]       rq_tag [RQ_N];
  logic [7:0]       rq_str [RQ_N];
  logic             rq_deg [RQ_N];
  logic [RQ_AW-1:0] rq_wp, rq_rp;
  logic [RQ_CW-1:0] rqcnt;

  assign out_valid_o      = (rqcnt != RQ_CW'(0));
  assign out_tok_o        = rq_tok[rq_rp];
  assign out_tag_o        = rq_tag[rq_rp];
  assign out_str_o        = rq_str[rq_rp];
  assign out_degenerate_o = rq_deg[rq_rp];

  // ---- the one arbitration point ------------------------------------------
  logic [RQ_CW-1:0] rq_free;
  logic             p_sheet, p_degen, pop_out, off_pop, sheet_fire;
  logic [1:0]       pushes;

  always_comb begin
    // Conservative: the concurrent pop is NOT counted as freeing a slot. The
    // queue is a full credit deep, so the slack costs nothing, and it keeps
    // the full term out of the consumer's path.
    rq_free = RQ_CW'(RQ_N) - rqcnt;

    // No ready line. Sinking this is not optional.
    p_sheet = sheet_rvalid_i;

    // The degenerate may take a slot only if one is left AFTER the sheet's.
    p_degen = off_head_v && off_head_degen &&
              (rq_free >= (p_sheet ? RQ_CW'(2) : RQ_CW'(1)));

    sheet_fire = sheet_valid_o && sheet_ready_i;
    off_pop    = sheet_fire || p_degen;
    pop_out    = out_valid_o && out_ready_i;
    pushes     = 2'(p_sheet) + 2'(p_degen);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sd_wp         <= 4'd0;
      a0_v_q        <= 1'b0;
      credit_q      <= CREDIT_W'(0);
      off_wp        <= OFF_AW'(0);
      off_rp        <= OFF_AW'(0);
      off_cnt       <= OFF_CW'(0);
      rq_wp         <= RQ_AW'(0);
      rq_rp         <= RQ_AW'(0);
      rqcnt         <= RQ_CW'(0);
      accepted_o    <= 32'd0;
      sheet_reads_o <= 32'd0;
      degenerate_o  <= 32'd0;
    end else begin
      // ---- the credit: taken at admission, released at TERMINAL acceptance -
      // NOT at the sheet handshake. A request handed to the sheet still owns
      // an offer slot until the head moves, a return-queue slot it has not
      // used yet, and a side-channel entry. Releasing there would let the
      // block admit work it has no room for -- which is precisely the shape of
      // the defect being repaired, one level in.
      credit_q <= credit_q + CREDIT_W'(admit_c) - CREDIT_W'(pop_out);

      // ---- A0 capture: the ports, one subtract deep ----------------------
      a0_v_q <= admit_c;
      if (admit_c) begin
        a0_degen_q <= degen_c;
        a0_du_q    <= du_c;
        a0_dv_q    <= dv_c;
        a0_nu_q    <= nu_c;
        a0_nv_q    <= nv_c;
        a0_tok_q   <= req_tok_i;
        accepted_o <= accepted_o + 32'd1;
        if (degen_c) degenerate_o <= degenerate_o + 32'd1;
      end

      // ---- A0b: the side channel, written where the divider is ISSUED -----
      // `sd_wp` is the divider's tag, so this write has to happen on the same
      // clock the divider takes the job -- which is now A0b, not A0. Splitting
      // the input cone moved the issue point by one clock and this moved with
      // it; leaving it at A0 would have written slot N's flags under slot
      // N+1's tag, and every saturated envelope would have come back clamped
      // on the wrong axis of the wrong request.
      if (a0_v_q) begin
        sd_degen[sd_wp] <= a0_degen_q;
        sd_satu[sd_wp]  <= satu_c;
        sd_satv[sd_wp]  <= satv_c;
        sd_tok[sd_wp]   <= a0_tok_q;
        sd_wp           <= sd_wp + 4'd1;
      end

      // ---- A7: push every divider result; pop ONLY on acceptance ----------
      if (div_ov) begin
        off_u[off_wp]   <= tex_u_c;
        off_v[off_wp]   <= tex_v_c;
        off_tok[off_wp] <= sd_tok[div_tag];
        off_deg[off_wp] <= sd_degen[div_tag];
        off_wp          <= off_wp + OFF_AW'(1);
      end
      if (off_pop) off_rp <= off_rp + OFF_AW'(1);
      off_cnt <= off_cnt + OFF_CW'(div_ov) - OFF_CW'(off_pop);

      if (sheet_fire) sheet_reads_o <= sheet_reads_o + 32'd1;

      // ---- A8/A9: the return queue's count moves ONCE --------------------
      // Two producers (a sheet response, and a degenerate bypass) and one
      // consumer. Counting each separately is the fault the perspective lane
      // had; the net is computed once, above, and both writers are gated on
      // the same reservation.
      if (p_sheet) begin
        rq_tok[rq_wp] <= sheet_rtok_i;
        rq_tag[rq_wp] <= sheet_tag_i;
        rq_str[rq_wp] <= sheet_str_i;
        rq_deg[rq_wp] <= 1'b0;
      end
      if (p_degen) begin
        automatic logic [RQ_AW-1:0] w = p_sheet ? (rq_wp + RQ_AW'(1)) : rq_wp;
        rq_tok[w] <= off_tok[off_rp];
        rq_tag[w] <= 8'd0;
        rq_str[w] <= 8'd0;
        rq_deg[w] <= 1'b1;
      end
      rq_wp <= rq_wp + RQ_AW'(pushes);
      rqcnt <= rqcnt + RQ_CW'(pushes) - RQ_CW'(pop_out);
      if (pop_out) rq_rp <= rq_rp + RQ_AW'(1);
    end
  end

`ifndef QUARTUS_SYNTHESIS
  // =========================================================== THE ASSERTIONS
  // Excluded from every Quartus fit (`tools/quartus/run_block_map.ps1` defines
  // QUARTUS_SYNTHESIS=1), present for lint, formal and simulation. Every
  // signal declared here is verification-only and never reaches the fabric.
  //
  // WHERE THESE ACTUALLY FIRE, stated rather than implied: Verilator
  // elaborates `assert` only when given `--assert`, and this block's directed
  // test does not currently pass it. So these are checked by lint and by
  // formal, and THE SAME INVARIANTS ARE CHECKED FROM OUTSIDE THE MODULE by
  // tests/texture/texture_aux_pipe_directed.cpp, which does have evidence of
  // firing. A detector that has not been shown to fire has not been tested;
  // the C++ ones are the ones carrying that weight today.
  logic f_past_valid;

  // A shadow of the offer, one clock old, so stability is a comparison rather
  // than a claim.
  logic            f_sv_q, f_sr_q;
  logic [5:0]      f_su_q, f_svc_q;
  logic [TOKW-1:0] f_stok_q;

  // Issued sheet reads still awaiting a response. A response for nothing is a
  // protocol fault, not a sample.
  logic [CREDIT_W-1:0] f_sh_out_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      f_past_valid <= 1'b0;
      f_sv_q       <= 1'b0;
      f_sr_q       <= 1'b0;
      f_su_q       <= 6'd0;
      f_svc_q      <= 6'd0;
      f_stok_q     <= {TOKW{1'b0}};
      f_sh_out_q   <= CREDIT_W'(0);
    end else begin
      f_past_valid <= 1'b1;
      f_sv_q       <= sheet_valid_o;
      f_sr_q       <= sheet_ready_i;
      f_su_q       <= sheet_u_o;
      f_svc_q      <= sheet_v_o;
      f_stok_q     <= sheet_tok_o;
      f_sh_out_q   <= f_sh_out_q + CREDIT_W'(sheet_fire) - CREDIT_W'(sheet_rvalid_i);
    end
  end

  // `f_past_valid` alone is the guard, deliberately: it is asynchronously
  // cleared by the same reset, so it already IMPLIES "out of reset, and there
  // was a previous clock to compare against". Adding `rst_n &&` here would say
  // nothing extra and would flop rst_n both synchronously and asynchronously
  // in one module -- a real lint finding (SYNCASYNCNET) that this block should
  // not be teaching people to waive.
  always_ff @(posedge clk) begin
    if (f_past_valid) begin
      // 1. THE OFFER IS HELD. Valid does not retract and no payload bit moves
      //    while the sheet has not taken it. This is the defect that hung a
      //    FRAGROB slot; it is structural now, and this states it.
      if (f_sv_q && !f_sr_q) begin
        a_offer_valid_held : assert (sheet_valid_o);
        a_offer_u_stable   : assert (sheet_u_o   == f_su_q);
        a_offer_v_stable   : assert (sheet_v_o   == f_svc_q);
        a_offer_tok_stable : assert (sheet_tok_o == f_stok_q);
      end

      // 2. THE RETURN QUEUE BOUND HOLDS. Never more pushes than free slots,
      //    the sheet is never the producer refused, and the count never leaves
      //    its range -- which is how the old 4-bit count wrapped to 0 on a
      //    queue that still held data.
      a_rq_admits_pushes : assert (RQ_CW'(pushes) <= rq_free);
      a_rq_sheet_room    : assert (!p_sheet || (rq_free != RQ_CW'(0)));
      a_rq_in_range      : assert (rqcnt <= RQ_CW'(RQ_N));

      // 3. THE NON-STALLABLE PRODUCER NEVER MEETS A FULL SINK. The divider
      //    cannot be told to wait, so its landing place must be provably free.
      a_off_no_overflow  : assert (!div_ov || (off_cnt != OFF_CW'(OFF_N)));
      a_off_in_range     : assert (off_cnt <= OFF_CW'(OFF_N));

      // 4. A RESPONSE ARRIVES ONLY FOR AN ISSUED TOKEN. Counted rather than
      //    tracked per token: a response with nothing outstanding is a fault
      //    whatever token it carries, and the count is the part that can be
      //    checked cheaply in fabric-shaped logic. The last term admits the
      //    zero-latency loopback the composed bench uses, where a response is
      //    presented on the same cycle as the handshake that earned it.
      a_response_was_issued : assert (!sheet_rvalid_i ||
                                      (f_sh_out_q != CREDIT_W'(0)) || sheet_fire);

      // 5. THE CREDIT IS THE BOUND ON EVERYTHING ELSE, so it had better hold.
      a_credit_in_range  : assert (credit_q   <= CREDIT_W'(CREDIT));
      a_sheet_out_bound  : assert (f_sh_out_q <= CREDIT_W'(CREDIT));
    end
  end
`endif

endmodule : zhao_texture_aux_pipe

`default_nettype wire
