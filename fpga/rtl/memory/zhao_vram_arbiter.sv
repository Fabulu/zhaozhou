// zhao_vram_arbiter.sv — guaranteed-liveness VRAM arbiter (plan W2.5, D3).
// Law: spec/memory_rules.md §2; contract design/contracts/MEM.VRAM.ARBITER.md.
//
// Policy (D3), evaluated at every SDRAM-edge acceptance boundary:
//   1. AGING OVERRIDE — a guaranteed client (scanout/blit/engine0/engine1)
//      whose pending bursts have waited >= AGING_OVERRIDE cycles is served
//      first (oldest wins, lowest id breaks ties). This is what makes the
//      liveness bound finite at all: without it, strict scanout priority
//      starves the RR class forever.
//
//      CORRECTED 2026-08-16 (ratified; see zhao_pkg.sv and
//      spec/memory_rules.md §2.1). This comment used to claim B = 40 was a
//      structural theorem because "one in-flight burst (18) plus one
//      competitor burst (18) = 36 < 40". That argument is wrong twice over:
//      the competitor gets TWO bursts, not one (a 64-B request is four
//      bursts and the override only closes the window after
//      ceil(AGING_OVERRIDE / MAX_BURST_SPAN) = 2 of them), and 40 was itself
//      computed with MAX_BURST = 14 rather than the true worst span 18. The
//      proven, tight, refresh-free bounds are 34 (scanout) and 52 (RR); the
//      operational bounds add a 13-cycle refresh steal => 47 and 65.
//      An override does raise hold_refresh, so an urgent (not hard) refresh
//      cannot push past it — that part of the old comment stands.
//   2. SCANOUT — strict priority (isochronous display fetch). It preempts
//      the RR class at BURST BOUNDARIES only (never mid-burst: the ctrl
//      executes whole bursts and re-arbitrates at each acceptance edge).
//   3. ROUND-ROBIN among {blit_dma, engine0, engine1} (pointer advances
//      past the served member).
//   4. DEBUG — best-effort class, served only when nothing else is pending
//      (empty in Phase-2 traffic; the port is the reservation).
//
// Credits (credit-based edge law): each client holds a 32-word pool (one max
// 64-B request). A granted request deducts its word count up front; the
// controller reissues credits burst-by-burst as bursts retire
// (ctrl_rsp.credits, routed to the issuing client). One outstanding request
// per client: a request held while pending[].active is ignored (clients are
// internal and follow the law).
//
// Byte accounting: vram_bytes_by_client += len (bytes, saturating u32) at
// request grant; shadows latch on frame_tick (D9). scanout_preempted counts
// boundaries where a non-scanout burst was served while scanout was eligible
// — the bandwidth-budget test asserts ZERO (spec/memory_rules.md §2).
//
// client_req[].len is BYTES (1..64, zhao_pkg law); ctrl_req.len at the SDRAM
// edge is WORDS (1..8, 0 encodes 8) — the arbiter is the converter. A burst
// never crosses a 2048-word row boundary (open-page law).
//
// Conservative SystemVerilog subset only (charter §2). Lint: clean under
// `verilator --lint-only -Wall` (lint_mem_vram_arbiter CTest).

module zhao_vram_arbiter
  import zhao_pkg::*, zhao_sdram_params_pkg::*;
(
  input  logic clk,
  input  logic rst_n,

  // client ports (zhao_client_e order: scanout, blit, engine0, engine1, debug)
  input  zhao_arb_req_t [4:0] client_req,
  output zhao_arb_rsp_t [4:0] client_rsp,

  // SDRAM edge (the credit port of zhao_sdram_ctrl)
  output zhao_arb_req_t ctrl_req,
  output logic          hold_refresh,   // override boundary: refresh defers
  input  zhao_arb_rsp_t ctrl_rsp,

  // counter snapshot law (D9)
  input  logic             frame_tick,
  output logic [4:0][31:0] vram_bytes,
  output logic [4:0][31:0] vram_bytes_shadow,
  output logic [31:0]      scanout_preempted
);

  localparam int unsigned AGING_OVERRIDE = 20;  // > one MAX_BURST_SPAN wait
  localparam int unsigned CREDIT_INIT    = 32;  // words (one 64-B request)
  localparam logic [2:0]  SCANOUT_ID = 3'(ZHAO_CLIENT_SCANOUT);
  localparam logic [2:0]  DEBUG_ID   = 3'(ZHAO_CLIENT_DEBUG);

  // -------------------------------------------------------------- per-client
  logic [4:0]  pend_active;
  logic [4:0]  pend_write;
  logic [26:0] pend_addr  [0:4];   // next burst byte address
  logic [5:0]  pend_words [0:4];   // words not yet accepted by the ctrl
  logic [5:0]  credits    [0:4];
  logic [5:0]  age        [0:4];   // cycles eligible-but-unserved (saturating)
  logic [2:0]  rr_ptr;             // next RR member in {1,2,3}
  logic [2:0]  last_issuer;        // owner of the in-flight ctrl burst

  // request word counts (len bytes 1..64 -> words 1..32); 0/overlong -> 0
  function automatic logic [5:0] words_of(input logic [6:0] len_b);
    if (len_b == 7'd0 || len_b > 7'd64) words_of = 6'd0;
    else begin
      words_of = len_b[6:1];              // len <= 64: <= 32, fits 6 bits
      if (len_b[0]) words_of = words_of + 6'd1;
    end
  endfunction

  // ------------------------------------------------------------- eligibility
  logic [4:0] eligible;
  always_comb begin
    for (int k = 0; k < 5; k++) eligible[k] = pend_active[k] && (pend_words[k] != 6'd0);
  end

  // burst a client may issue now: min(remaining, 8, row tail) in words
  // (rows are 2048 words; 8 divides the row so a tail burst never crosses)
  function automatic logic [3:0] burst_words(input logic [5:0] rem,
                                              input logic [10:0] col);
    logic [11:0] row_tail;
    row_tail = 12'd2048 - {1'b0, col};
    if (row_tail >= {6'b0, rem}) begin
      if (rem >= 6'd8) burst_words = 4'd8;
      else             burst_words = rem[3:0];
    end else begin
      if (row_tail >= 12'd8) burst_words = 4'd8;
      else                   burst_words = row_tail[3:0];
    end
  endfunction

  // ------------------------------------------------------------ selection ---
  logic       any_override;
  logic [2:0] ov_client;
  always_comb begin
    any_override = 1'b0;
    ov_client    = 3'd0;
    for (int k = 0; k < 4; k++) begin   // guaranteed clients 0..3
      if (!any_override && eligible[k] && (age[k] >= AGING_OVERRIDE[5:0])) begin
        any_override = 1'b1;           // FIRST threshold hit wins (lowest id)
        ov_client    = 3'(k);          // — keeps the B-bound argument local
      end
    end
  end

  // RR pick starting at rr_ptr (members 1..3 only)
  // RR scan: members 1..3 in pointer order (7 = none eligible)
  function automatic logic [2:0] rr_next(input logic [2:0] p);
    rr_next = (p == 3'd3) ? 3'd1 : (p + 3'd1);
  endfunction
  logic [2:0] rr_pick;
  always_comb begin
    logic [2:0] c;
    rr_pick = 3'd7;
    c       = rr_ptr;
    for (int k = 0; k < 3; k++) begin
      if (rr_pick == 3'd7 && eligible[c]) rr_pick = c;
      c = rr_next(c);
    end
  end

  logic       sel_valid;
  logic [2:0] sel;
  logic       sel_override;
  always_comb begin
    sel_valid    = 1'b0;
    sel          = 3'd0;
    sel_override = 1'b0;
    if (any_override) begin
      sel_valid    = 1'b1;
      sel          = ov_client;
      sel_override = 1'b1;
    end else if (eligible[SCANOUT_ID]) begin
      sel_valid = 1'b1;
      sel       = SCANOUT_ID;
    end else if (rr_pick != 3'd7) begin
      sel_valid = 1'b1;
      sel       = rr_pick;
    end else if (eligible[DEBUG_ID]) begin
      sel_valid = 1'b1;
      sel       = DEBUG_ID;
    end
  end


  // ------------------------------------------------------------- the offer ---
  // The offered burst is REGISTERED and held STABLE until the controller
  // accepts it (grant reads high one cycle after acceptance). A purely
  // combinational presentation would let a newly-latched port request
  // change ctrl_req between the acceptance edge and the grant pulse — the
  // controller would execute one burst while the bookkeeping decremented
  // another. The offer latch closes that window by construction.
  // ENFORCED-BY: tests/memory/mem_random.cpp:VramArbiter
  // (a torn offer diverges the grant order / per-client byte counts from
  // the zref::VramArbiter oracle in the three-way random differential)
  logic        offer_valid;
  logic        offer_write;
  logic [2:0]  offer_client;
  logic [26:0] offer_addr;
  logic [3:0]  offer_words;   // 1..8
  logic        offer_hold;    // the offer was an aging-override selection
  logic        offer_sup;     // scanout was ELIGIBLE when this offer was
                             // latched (a grant of it is a strict-priority
                             // violation; a stale pre-scanout offer is not)

  // ---- every client's burst length, computed BEFORE the winner is known ---
  // The seed-3 fit made this the design's worst path at -0.425 ns:
  //
  //     pend_words[1][2] -> eligible[1]  +0.587
  //                      -> always1~2    +0.572   the override/RR/priority
  //                      -> sel[0]~2     +0.413   arbitration
  //                      -> Mux32~2/~3   +0.974   5-way mux of pend_words+addr
  //                      -> Add2~29      +0.932   row_tail = 2048 - col
  //                      -> LessThan4~2  +0.495   row_tail >= rem
  //                      -> LessThan4~4  +0.560   row_tail >= 8
  //                      -> sel_bw[0]~2  +0.503
  //                      -> offer_words
  //
  // Eligibility, a three-way priority arbitration, a 5-way mux of 17 bits,
  // a 12-bit subtract and two 12-bit compares, all in one cycle -- and the
  // arithmetic sat AFTER the arbitration, waiting to learn which client won.
  //
  // It never needed to wait. `burst_words` is a pure function of ONE client's
  // own `pend_words` and `pend_addr`, so all five results can be computed
  // while the arbiter is still deciding, and the winner selects among the
  // ANSWERS instead of among the inputs. A 17-bit mux feeding a subtract and
  // two compares becomes a 4-bit mux fed by them.
  //
  // Identical by construction: bw_all[sel] IS burst_words(pend_words[sel],
  // pend_addr[sel][11:1]), evaluated for every k rather than just the winner.
  //
  // Costs five copies of a 12-bit subtract and two compares instead of one.
  // Same shape as the Early-Z floor fix in round 12 -- when a select feeds
  // arithmetic, computing every branch and selecting the RESULT is shorter
  // than selecting the operand and then computing.
  logic [3:0] bw_all [0:4];
  always_comb begin
    for (int k = 0; k < 5; k++)
      bw_all[k] = burst_words(pend_words[k], pend_addr[k][11:1]);
  end

  logic [3:0] sel_bw;
  assign sel_bw = sel_valid ? bw_all[sel] : 4'd0;

  always_comb begin
    ctrl_req         = '0;
    ctrl_req.valid   = offer_valid;
    ctrl_req.write   = offer_write;
    ctrl_req.client  = offer_valid ? zhao_client_e'(offer_client)
                                   : ZHAO_CLIENT_NONE;
    ctrl_req.addr    = offer_addr;
    ctrl_req.len     = (offer_words == 4'd8) ? 7'd0 : {4'b0, offer_words[2:0]};
  end

  assign hold_refresh = offer_valid && offer_hold;

  // ctrl_rsp.grant reads high during cycle G (the burst's first command
  // cycle); credits for the PREVIOUS burst read high one cycle earlier, so
  // last_issuer (updated on grants) always names the retiring owner.
  logic ctrl_grant, ctrl_retire;
  logic [4:0] ctrl_retire_words;
  assign ctrl_grant        = ctrl_rsp.grant;
  assign ctrl_retire       = (ctrl_rsp.credits != 8'd0);
  assign ctrl_retire_words = ctrl_rsp.credits[4:0];

  // ------------------------------------------------------- port handshake ---
  // grant_k: the window accepts client k's request at this edge (one
  // outstanding per client, credit-covered, legal length)
  logic [4:0] port_grant;
  always_comb begin
    for (int k = 0; k < 5; k++) begin
      logic [5:0] w_k;
      logic [5:0] ret_words_k;
      w_k         = words_of(client_req[k].len);
      ret_words_k = (ctrl_retire && (last_issuer == 3'(k)))
                    ? {1'b0, ctrl_retire_words} : 6'd0;
      port_grant[k] = client_req[k].valid && !pend_active[k] && (w_k != 6'd0)
                      && ((credits[k] + ret_words_k) >= w_k);
    end
  end

  // ------------------------------------------------------------- seq core ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pend_active       <= 5'b0;
      pend_write        <= 5'b0;
      rr_ptr            <= 3'd1;
      last_issuer       <= 3'd0;
      offer_valid       <= 1'b0;
      offer_write       <= 1'b0;
      offer_client      <= 3'd0;
      offer_addr        <= 27'd0;
      offer_words       <= 4'd0;
      offer_hold        <= 1'b0;
      offer_sup         <= 1'b0;
      scanout_preempted <= 32'd0;
      vram_bytes_shadow <= '0;
      for (int k = 0; k < 5; k++) begin
        pend_addr[k]  <= 27'd0;
        pend_words[k] <= 6'd0;
        credits[k]    <= 6'(CREDIT_INIT);
        age[k]        <= 6'd0;
        vram_bytes[k] <= 32'd0;
      end
    end else begin
      // ---- D9: shadow latch on frame_tick --------------------------------
      if (frame_tick) vram_bytes_shadow <= vram_bytes;

      for (int k = 0; k < 5; k++) begin
        logic [5:0] w_k;
        logic [5:0] ret_words_k;
        w_k         = words_of(client_req[k].len);
        ret_words_k = (ctrl_retire && (last_issuer == 3'(k)))
                      ? {1'b0, ctrl_retire_words} : 6'd0;

        // credit bookkeeping: retirements return, granted requests deduct
        // (a retirement and a grant may land on one client in one cycle —
        // both apply in the single expression below)
        credits[k] <= credits[k] + ret_words_k - (port_grant[k] ? w_k : 6'd0);

        if (port_grant[k]) begin
          // a fresh request fully re-latches the window slot
          pend_active[k] <= 1'b1;
          pend_write[k]  <= client_req[k].write;
          pend_addr[k]   <= client_req[k].addr;
          pend_words[k]  <= w_k;
          // saturating byte counter (catalog vram_bytes_by_client)
          // ENFORCED-BY: tests/formal/sat_add.sby
          vram_bytes[k]  <= zhao_pkg::zhao_sat_add32(vram_bytes[k],
                                                     {25'b0, client_req[k].len});
        end else if (ctrl_grant && offer_valid && (offer_client == 3'(k))) begin
          // the ctrl accepted the OFFERED burst (the port grant arm
          // dominates: a same-edge new request replaces the old one whole)
          pend_words[k] <= pend_words[k] - {2'b0, offer_words};
          pend_addr[k]  <= pend_addr[k] + {22'b0, offer_words, 1'b0};
          if (pend_words[k] == {2'b0, offer_words}) pend_active[k] <= 1'b0;
        end
      end

      // ---- the offer latch -------------------------------------------------
      if (ctrl_grant && offer_valid) begin
        // the controller took the offered burst: retire the offer. A fresh
        // selection latches at the NEXT edge from the UPDATED pend state —
        // latching at this same edge would duplicate the consumed burst
        // (the pre-edge pend still shows it) and underflow the word count.
        // The one-cycle gap costs nothing: the ctrl is busy >= 9 cycles.
        offer_valid <= 1'b0;
        last_issuer <= offer_client;
        if (offer_client >= 3'd1 && offer_client <= 3'd3)
          rr_ptr <= rr_next(offer_client);
        // strict-priority violation witness: a NON-override, non-scanout
        // grant of an offer that was LATCHED while scanout was eligible
        // (an aging override is the liveness law working, not starvation;
        // an offer latched before scanout arrived is legal burst-boundary
        // preemption — at most one stale offer, bounded by MAX_BURST_SPAN)
        if (offer_client != SCANOUT_ID && !offer_hold && offer_sup)
          scanout_preempted <= scanout_preempted + 32'd1;
      end
      if (!offer_valid && !(ctrl_grant && offer_valid) && sel_valid) begin
        offer_valid  <= 1'b1;
        offer_write  <= pend_write[sel];
        offer_client <= sel;
        offer_addr   <= pend_addr[sel];
        offer_words  <= sel_bw;
        offer_hold   <= sel_override;
        offer_sup    <= sel_override ? 1'b0
                        : (sel != SCANOUT_ID) && eligible[SCANOUT_ID];
      end

      // ---- aging (saturating; resets when served or idle) ----------------
      for (int k = 0; k < 5; k++) begin
        if (eligible[k] && !(ctrl_grant && offer_valid && (offer_client == 3'(k)))) begin
          if (age[k] != 6'd63) age[k] <= age[k] + 6'd1;
        end else begin
          age[k] <= 6'd0;
        end
      end
    end
  end

  // client responses: registered grant pulse; credit returns pass through
  genvar gj;
  generate
    for (gj = 0; gj < 5; gj++) begin : g_rsp
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) client_rsp[gj].grant <= 1'b0;
        else        client_rsp[gj].grant <= port_grant[gj];
      end
      assign client_rsp[gj].credits =
        (ctrl_retire && (last_issuer == 3'(gj))) ? ctrl_rsp.credits : 8'd0;
    end
  endgenerate

endmodule
