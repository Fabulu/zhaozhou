// zhao_mem_share_wr.sv -- N logical requesters, READERS AND WRITERS, ONE
// permitted MEM.GUARD client with its write channel and its credit stream.
//
// Law: spec/memory_rules.md 5d / 5f (a client identity is a PRIVILEGE, not a slot)
//      zhao_mem_share2.sv (the share's arbitration and the guard's two-cycle law)
//      core entry I26 (THE TERRAIN.BUILD SOCKET's upstream sharing)
//
// ===========================================================================
// WHY THIS FILE EXISTS
// ===========================================================================
// `zhao_shell_top_v2` exposes ONE guard client on VRAM slot 6 -- the
// TERRAIN.BUILD socket -- and its own port comment says "upstream sharing of the
// one guard port is the composer's, through `zhao_mem_share_n` for readers".
// For READERS that is the whole story: `zhao_mem_share_n` records the owner and
// routes the beats. A socket with WRITERS on it needs two more things the read
// share does not have, and both are about the fact that a write carries data on
// a SEPARATE channel from its request:
//
//   1. WRITE-DATA ORDER. `zhao_mem_share_n` is idle again the cycle after a
//      write's verdict (a write returns no beats), so it can take a SECOND
//      writer's request while the first writer is still streaming its data. The
//      shell's slot-6 queue is one FIFO: two writers streaming at once would
//      interleave their words and the controller would write one client's bytes
//      at the other's address. So from the moment the share TAKES a write until
//      that write's LAST data beat (or its refusal), every other request is
//      withheld -- readers' too, for the fairness reason given at the mask.
//   2. RETIREMENT ATTRIBUTION. `zhao_vram_arbiter` returns credits per CLIENT,
//      and MEM.UPLOAD's publication waits on them ("the ONLY thing that means the
//      write landed"). With three requesters on one client, each would see the
//      others' credits -- and an upload that counted a page load's retirement as
//      its own would publish a mapping to bytes still in flight, the one thing
//      its retire wait exists to prevent. So a ledger of {requester, words} is
//      pushed at each passed verdict IN ISSUE ORDER, and the credit stream drains
//      its head. `retire_unowned_o` is the tripwire: a credit with no owner, or
//      more credit than the head is owed.
//
// THIS IS NOT A NEW PATTERN, and saying where it came from is the point.
// `zhao_post_lease` does exactly these two things, inline, for ENGINE0's three
// requesters (its "WRITE-DATA ORDER" and "RETIREMENT ATTRIBUTION" sections).
// This block is that logic lifted into one place with N a parameter, so the
// second user does not write a second copy of it. The post lease is another
// packet's file and was not edited; adopting this block there is a follow-up
// that would delete ~70 lines from it.
//
// ===========================================================================
// WHAT IT KEEPS FROM zhao_mem_share_n, UNCHANGED
// ===========================================================================
// It INSTANTIATES `zhao_mem_share_n` (FORCE_READ=0) rather than copying it, so
// one logical request in flight, the recorded owner, the guard's two-cycle law,
// `last` from each request's own length and the N-1 round-robin bound all come
// from the proved core. This file only MASKS offers into it and adds the two
// channels above.
//
// ===========================================================================
// BACKPRESSURE, NOT AN OVERFLOW COUNTER
// ===========================================================================
// The ledger is RQ entries deep. Rather than count an overflow nobody could
// fire, every offer is withheld while the ledger has fewer than TWO free
// entries: one for a request the share may already have taken but not yet
// verdicted, one for this. A full ledger therefore stalls the socket's
// requesters -- visibly, as the guard being busy -- and can never lose an
// owner.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_mem_share_wr
  import zhao_pkg::*;
#(
    parameter int unsigned N = 3,
    parameter int unsigned CLIENT_ID = 6,          // ZHAO_CLIENT_TERRAIN_BUILD
    parameter int unsigned RQ = 4                  // retire ledger depth, power of two
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the logical requesters --------------------------------------------
    input  var zhao_guard_req_t [N-1:0]       req_i,
    output var zhao_guard_rsp_t [N-1:0]       rsp_o,
    output var logic            [N-1:0]       beat_valid_o,
    output var logic            [63:0]        beat_data_o,   // one bus; valid routes it
    output var logic            [N-1:0]       beat_last_o,
    input  var logic            [N-1:0][63:0] wdata_i,
    input  var logic            [N-1:0]       wvalid_i,
    input  var logic            [N-1:0]       wlast_i,
    output var logic            [N-1:0]       wready_o,
    // each requester's share of the client's credit stream, in 16-bit words
    output var logic            [N-1:0][7:0]  retire_o,

    // ---- the one permitted client, downstream to MEM.GUARD ----------------
    output var zhao_guard_req_t m_req_o,
    input  var zhao_guard_rsp_t m_rsp_i,
    input  var logic            m_beat_valid_i,
    input  var logic [63:0]     m_beat_data_i,
    input  var logic            m_beat_last_i,
    output var logic [63:0]     m_wdata_o,
    output var logic            m_wvalid_o,
    output var logic            m_wlast_o,
    input  var logic            m_wready_i,
    input  var logic [7:0]      m_credits_i,     // client_rsp[slot].credits

    // ---- evidence ---------------------------------------------------------
    output var logic [N-1:0][31:0] jobs_o,
    output var logic [31:0]     denied_o,
    output var logic [31:0]     contention_o,
    output var logic [31:0]     err_short_o,
    output var logic [31:0]     err_long_o,
    output var logic [31:0]     err_unowned_o,
    // a credit nobody was owed, or more than the ledger's head was owed
    output var logic [31:0]     retire_unowned_o,
    // cycles a requester offered write data while it did not own the channel
    output var logic [31:0]     wbeat_unowned_o,
    // cycles every offer was withheld because the ledger was full
    output var logic [31:0]     ledger_full_o
);

  localparam int unsigned IW = (N > 1) ? $clog2(N) : 1;
  localparam int unsigned QW = (RQ > 1) ? $clog2(RQ) : 1;

  initial begin
    if (RQ < 2 || (RQ & (RQ - 1)) != 0) begin
      $fatal(1, "zhao_mem_share_wr: RQ must be a power of two >= 2 (got %0d)", RQ);
    end
  end

  // ==========================================================================
  // THE MASKED OFFERS
  // ==========================================================================
  logic          wpend_q;          // a taken write has not sent its last beat
  logic [IW-1:0] wown_q;           // ...and this requester owns the data channel
  logic [QW:0]   rq_n_q;           // ledger occupancy
  logic          ledger_tight_c;

  assign ledger_tight_c = (rq_n_q >= (QW+1)'(RQ - 1));

  zhao_guard_req_t [N-1:0] sh_req;
  zhao_guard_rsp_t [N-1:0] sh_rsp;

  // WHY EVERY OFFER, NOT ONLY WRITERS', IS WITHHELD WHILE A WRITE'S DATA IS
  // OUTSTANDING. The first version masked only writers and let readers through,
  // and its directed test starved writer 1 outright: the round-robin reaches
  // writer 1's turn exactly while writer 0 is streaming, finds it masked, picks
  // the reader, and rotates back to writer 0 -- every time. Masking everything
  // keeps the rotation's N-1 bound intact at the cost of a reader waiting out
  // one burst's data (eight beats), which is the cheap side of that trade.
  always_comb begin
    for (int i = 0; i < N; i++) begin
      sh_req[i] = req_i[i];
      if (ledger_tight_c || wpend_q) sh_req[i].valid = 1'b0;
    end
  end

  zhao_mem_share_n #(
      .N         (N),
      .CLIENT_ID (CLIENT_ID),
      .FORCE_READ(1'b0)
  ) u_share (
      .clk           (clk),
      .rst_n         (rst_n),
      .req_i         (sh_req),
      .rsp_o         (sh_rsp),
      .beat_valid_o  (beat_valid_o),
      .beat_data_o   (beat_data_o),
      .beat_last_o   (beat_last_o),
      .m_req_o       (m_req_o),
      .m_rsp_i       (m_rsp_i),
      .m_beat_valid_i(m_beat_valid_i),
      .m_beat_data_i (m_beat_data_i),
      .m_beat_last_i (m_beat_last_i),
      .jobs_o        (jobs_o),
      .denied_o      (denied_o),
      .contention_o  (contention_o),
      .err_short_o   (err_short_o),
      .err_long_o    (err_long_o),
      .err_unowned_o (err_unowned_o)
  );

  assign rsp_o = sh_rsp;

  // ==========================================================================
  // WRITE-DATA ORDER
  // ==========================================================================
  // Registered at the TAKE, which is early enough: the share takes one
  // requester per idle cycle and is then busy through the verdict, so no second
  // write can be taken before this mask is up.
  logic take_wr_c;
  logic [IW-1:0] take_idx_c;
  always_comb begin
    take_wr_c  = 1'b0;
    take_idx_c = '0;
    for (int i = 0; i < N; i++) begin
      if (sh_req[i].valid && sh_rsp[i].ready && sh_req[i].write) begin
        take_wr_c  = 1'b1;
        take_idx_c = IW'(i);
      end
    end
  end

  always_comb begin
    m_wdata_o  = wdata_i[wown_q];
    m_wvalid_o = wpend_q && wvalid_i[wown_q];
    m_wlast_o  = wpend_q && wlast_i[wown_q];
    for (int i = 0; i < N; i++) begin
      wready_o[i] = wpend_q && (wown_q == IW'(i)) && m_wready_i;
    end
  end

  logic wdone_c, wrefused_c;
  assign wdone_c    = m_wvalid_o && m_wready_i && m_wlast_o;
  assign wrefused_c = wpend_q && sh_rsp[wown_q].violation;

  logic wstray_c, any_offer_c;
  always_comb begin
    any_offer_c = 1'b0;
    for (int i = 0; i < N; i++) if (req_i[i].valid) any_offer_c = 1'b1;
  end
  always_comb begin
    wstray_c = 1'b0;
    for (int i = 0; i < N; i++) begin
      if (wvalid_i[i] && !(wpend_q && (wown_q == IW'(i)))) wstray_c = 1'b1;
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wpend_q         <= 1'b0;
      wown_q          <= '0;
      wbeat_unowned_o <= 32'd0;
      ledger_full_o   <= 32'd0;
    end else begin
      // Released by the owner's last data beat, or by its REFUSAL: a refused
      // write sends no data, and holding the mask would lock every other writer
      // out for good.
      if (wdone_c || wrefused_c) wpend_q <= 1'b0;
      if (take_wr_c) begin
        wpend_q <= 1'b1;
        wown_q  <= take_idx_c;
      end
      if (wstray_c) wbeat_unowned_o <= wbeat_unowned_o + 32'd1;
      if (ledger_tight_c && any_offer_c) ledger_full_o <= ledger_full_o + 32'd1;
    end
  end

  // ==========================================================================
  // RETIREMENT ATTRIBUTION -- in order, as the controller retires
  // ==========================================================================
  // The length each requester offered, captured when the share TOOK it,
  // because a requester may drop its request before the verdict.
  logic [N-1:0][6:0] take_len_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) take_len_q <= '0;
    else begin
      for (int i = 0; i < N; i++)
        if (sh_req[i].valid && sh_rsp[i].ready) take_len_q[i] <= sh_req[i].len;
    end
  end

  // `zhao_vram_arbiter`'s own rounding: a request of `len` bytes is
  // (len + 1) / 2 sixteen-bit words -- the same function the shell's write
  // gate uses (`build_words_of`), so the ledger owes exactly what the arbiter
  // will credit.
  function automatic logic [6:0] words_of(input logic [6:0] len_b);
    words_of = 7'((len_b + 7'd1) >> 1);
  endfunction

  logic          push_c;
  logic [IW-1:0] push_own_c;
  logic [6:0]    push_words_c;
  always_comb begin
    push_c       = 1'b0;
    push_own_c   = '0;
    push_words_c = 7'd0;
    for (int i = 0; i < N; i++) begin
      if (sh_rsp[i].ok) begin
        push_c       = 1'b1;
        push_own_c   = IW'(i);
        push_words_c = words_of(take_len_q[i]);
      end
    end
  end

  logic [IW-1:0] rq_own  [0:RQ-1];
  logic [6:0]    rq_left [0:RQ-1];
  logic [QW-1:0] rq_wp_q, rq_rp_q;

  logic          credit_c;
  logic [IW-1:0] head_own_c;
  assign credit_c   = (m_credits_i != 8'd0);
  assign head_own_c = rq_own[rq_rp_q];

  always_comb begin
    for (int i = 0; i < N; i++) begin
      retire_o[i] = (credit_c && (rq_n_q != '0) && (head_own_c == IW'(i)))
                    ? m_credits_i : 8'd0;
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rq_wp_q          <= '0;
      rq_rp_q          <= '0;
      rq_n_q           <= '0;
      retire_unowned_o <= 32'd0;
      for (int i = 0; i < RQ; i++) begin
        rq_own[i]  <= '0;
        rq_left[i] <= 7'd0;
      end
    end else begin
      automatic logic pop = 1'b0;
      if (credit_c) begin
        if (rq_n_q == '0) begin
          retire_unowned_o <= retire_unowned_o + 32'd1;
        end else if (7'(m_credits_i) >= rq_left[rq_rp_q]) begin
          pop = 1'b1;
          rq_rp_q <= rq_rp_q + QW'(1);
          // A burst's credits belong to ONE request. More than the head is
          // owed means a word was about to be attributed to the wrong owner.
          if (7'(m_credits_i) != rq_left[rq_rp_q])
            retire_unowned_o <= retire_unowned_o + 32'd1;
        end else begin
          rq_left[rq_rp_q] <= rq_left[rq_rp_q] - 7'(m_credits_i);
        end
      end
      if (push_c) begin
        rq_own[rq_wp_q]  <= push_own_c;
        rq_left[rq_wp_q] <= push_words_c;
        rq_wp_q          <= rq_wp_q + QW'(1);
      end
      rq_n_q <= rq_n_q + (push_c ? (QW+1)'(1) : '0) - (pop ? (QW+1)'(1) : '0);
    end
  end

endmodule

`default_nettype wire
