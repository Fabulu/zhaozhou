// zhao_geom_loomfeed_mutant.sv -- THE POSITIVE CONTROL for
// `ret_overflow_o`, and nothing else.
//
// THIS IS A COPY of fpga/rtl/geometry/zhao_geom_loomfeed.sv with ONE
// SUBSTANTIVE LINE CHANGED. REGENERATE IT if that file changes shape: a copy of
// an old version is a positive control for a block that no longer exists
// (CLAUDE.md, 2026-09-16). `tools/budget/mutant_copy_drift.py` watches for it.
//
// THE CHANGE, in full:
//
//     wire ret_credit = (owed + r_used) < (RETW+1)'(RETQ);   // production
//     wire ret_credit = 1'b1;                                // here
//
// WHY IT HAS TO BE A COMMITTED FILE. Law 7 of the production block reserves a
// return slot when a post is CONSUMED, so the return queue cannot be full when
// the record is written. That makes `ret_overflow_o` unreachable by every legal
// stimulus, and "it can fire" would stay an argument forever -- the exact case
// CLAUDE.md's committed-mutant chapter is about. Removing the credit makes it
// reachable, and its driver
// (tests/geometry/geom_loomfeed_mutant_control.cpp) has INVERTED POLARITY: it
// passes when the counter FIRES.
//
// It is evidence about the INSTRUMENT, not about the design. The renamed module
// is why no source list can elaborate it by mistake.
`default_nettype none

module zhao_geom_loomfeed_mutant #(
    // GEOM.LOOM's node bound, mirrored so a count above it is refused HERE
    // rather than after a thousand nodes have been composed and dropped. A
    // literal rather than an expression for the reason `zhao_field_doorbell`'s
    // parameter list gives: `tools/quartus/gen_prod_top.py` cannot evaluate a
    // parameter expression when it sizes a port and SKIPS a module it cannot
    // size, silently.
    parameter int unsigned MAX_NODES = 1024,
    parameter int unsigned IDXW      = 10,
    // The posted mailbox, in streams. Two lets the ARM stage frame N+1 while
    // frame N plays, which is the whole point of a posted mailbox.
    parameter int unsigned POSTS     = 2,
    // Consumed posts that may still owe a return record.
    parameter int unsigned RETQ      = 4,
    // Consecutive refusals of ONE burst before the stream is abandoned. Named
    // and editable for the reason `zhao_part_hps` gives: it is the boundary
    // between "the bridge was busy" and "this burst will never be accepted",
    // and nothing in the protocol distinguishes them.
    parameter int unsigned ERR_RETRY_N = 4,
    // The header's magic. "LOOM" in ASCII, little-endian in beat 0's low word.
    parameter logic [31:0] STREAM_MAGIC = 32'h4D4F_4F4C,
    // The bridge client tag this traffic carries. A parameter so the composer
    // states it; see the console core for the choice.
    parameter zhao_pkg::zhao_client_e CLIENT = zhao_pkg::ZHAO_CLIENT_ENGINE1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- D0: the plan's epoch identity (HPS-owned, held, trace only) --------
    input  var logic [31:0] cfg_plan_base_i,

    // ---- D1: posts ----------------------------------------------------------
    input  var logic        post_valid_i,
    output var logic        post_ready_o,
    input  var logic [31:0] post_base_i,     // byte address of record 0
    input  var logic [31:0] post_ticket_i,

    // ---- D2: returns --------------------------------------------------------
    output var logic        ret_valid_o,
    input  var logic        ret_ready_i,
    output var logic [31:0] ret_ticket_o,
    output var logic        ret_ok_o,        // the stream reached the loom whole
    output var logic        ret_refused_o,   // laws 2/3: refused before any beat
    output var logic [2:0]  ret_reason_o,
    output var logic [2:0]  ret_loom_reason_o, // law 4: the loom's own reason
    output var logic [15:0] ret_nodes_o,     // nodes delivered under this ticket
    output var logic [31:0] ret_plan_o,

    // ---- MEM.HPS.BRIDGE client (read only -- this block never writes) -------
    output zhao_pkg::zhao_hps_burst_req_t hps_req_o,
    input  var logic                      hps_grant_i,
    input  zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- GEOM.LOOM's node stream, name for name ----------------------------
    output var logic               lm_valid_o,
    input  var logic               lm_ready_i,
    output var logic [IDXW-1:0]    lm_node_index_o,
    output var logic [IDXW-1:0]    lm_parent_index_o,
    output var logic [3:0]         lm_kind_o,
    output var logic signed [31:0] lm_param_o [12],
    output var logic [15:0]        lm_angle_o,
    output var logic [1:0]         lm_axis_o,
    output var logic               lm_bodypatch_o,
    output var logic [15:0]        lm_src_id_o,
    output var logic               lm_first_o,
    output var logic               lm_last_o,
    output var logic signed [31:0] lm_cam_basis_o [9],

    // ---- GEOM.LOOM's refusal, OBSERVED (law 4) -----------------------------
    input  var logic       lm_refuse_valid_i,
    input  var logic [2:0] lm_refuse_reason_i,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] posts_o,             // posts consumed, all outcomes
    output var logic [31:0] streams_o,           // streams delivered whole
    output var logic [31:0] nodes_o,             // node beats accepted by the loom
    output var logic [31:0] bursts_o,            // 64-B reads granted
    output var logic [31:0] posts_refused_align_o,
    output var logic [31:0] headers_refused_o,   // magic or count
    output var logic [31:0] streams_refused_o,   // the loom refused one
    output var logic [31:0] streams_faulted_o,   // the bridge refused one
    output var logic [31:0] streams_replayed_o,  // law 6
    output var logic [31:0] post_stalls_o,       // cycles a post was held
    output var logic [31:0] bridge_errs_o,       // `err` seen at the request
    output var logic [31:0] feed_wait_cycles_o,  // the loom ready, nothing to give
    output var logic [31:0] ret_overflow_o       // unreachable by credit; see the mutant
);

  // ---- return reasons ------------------------------------------------------
  localparam logic [2:0] RR_NONE   = 3'd0;
  localparam logic [2:0] RR_ALIGN  = 3'd1;   // law 2
  localparam logic [2:0] RR_MAGIC  = 3'd2;   // law 3
  localparam logic [2:0] RR_COUNT  = 3'd3;   // law 3
  localparam logic [2:0] RR_LOOM   = 3'd4;   // law 4
  localparam logic [2:0] RR_BRIDGE = 3'd5;   // law 5

  localparam int unsigned PTRW = (POSTS > 1) ? $clog2(POSTS) : 1;
  localparam int unsigned RETW = (RETQ  > 1) ? $clog2(RETQ)  : 1;
  localparam int unsigned ERW  = $clog2(ERR_RETRY_N + 1);

  // Quartus 17.0 rejects a bare module-scope `if`; and `--lint-only` does not
  // run these, so a clean lint says nothing whatever about them (CLAUDE.md,
  // 2026-09-08).
  initial begin
    if (POSTS < 1) $fatal(1, "zhao_geom_loomfeed_mutant: POSTS must be at least 1");
    if (RETQ  < 1) $fatal(1, "zhao_geom_loomfeed_mutant: RETQ must be at least 1");
    if (ERR_RETRY_N < 1) $fatal(1, "zhao_geom_loomfeed_mutant: ERR_RETRY_N must be at least 1");
    // THE FROZEN RECORD GIVES EACH INDEX TEN BITS, so a carrier built at any
    // other IDXW would be reading a different record -- and it would do it
    // SILENTLY, by truncating a node index into something that still looks
    // legal to the loom. This guard is what makes the coupling loud: if
    // GEOM.LOOM's IDXW ever moves, the record layout moves with it in
    // design/contracts/GEOM.LOOM.STREAM.md and this line moves with both.
    if (IDXW != 10) begin
      $fatal(1, "zhao_geom_loomfeed_mutant: IDXW must be 10 (the frozen record's index width), got %0d", IDXW);
    end
    if (MAX_NODES < 1 || MAX_NODES > 65535) begin
      $fatal(1, "zhao_geom_loomfeed_mutant: MAX_NODES must be 1..65535, got %0d", MAX_NODES);
    end
  end

  // ==========================================================================
  // THE POSTED MAILBOX
  // ==========================================================================
  logic [31:0] q_base   [0:POSTS-1];
  logic [31:0] q_ticket [0:POSTS-1];

  logic [PTRW:0] q_wr, q_rd;
  wire  [PTRW:0] q_used  = q_wr - q_rd;
  wire           q_full  = (q_used == (PTRW+1)'(POSTS));
  wire           q_empty = (q_wr == q_rd);
  wire [PTRW-1:0] q_wi = q_wr[PTRW-1:0];
  wire [PTRW-1:0] q_ri = q_rd[PTRW-1:0];

  assign post_ready_o = !q_full;

  // ==========================================================================
  // THE RETURN QUEUE, AND THE CREDIT THAT MAKES IT UNOVERFLOWABLE (law 7)
  // ==========================================================================
  logic [31:0] r_ticket [0:RETQ-1];
  logic        r_ok     [0:RETQ-1];
  logic        r_refused[0:RETQ-1];
  logic [2:0]  r_reason [0:RETQ-1];
  logic [2:0]  r_lreason[0:RETQ-1];
  logic [15:0] r_nodes  [0:RETQ-1];
  logic [31:0] r_plan   [0:RETQ-1];

  logic [RETW:0] r_wr, r_rd;
  wire  [RETW:0] r_used  = r_wr - r_rd;
  wire           r_empty = (r_wr == r_rd);
  wire [RETW-1:0] r_wi = r_wr[RETW-1:0];
  wire [RETW-1:0] r_ri = r_rd[RETW-1:0];

  // Consumed posts that have not yet written their return record. THE CREDIT.
  logic [RETW:0] owed;
  // MUTATED (the one substantive line): the credit is removed, so a post is
  // consumed with no reserved return slot and the queue can be overrun.
  wire           ret_credit = 1'b1;

  assign ret_valid_o       = !r_empty;
  assign ret_ticket_o      = r_ticket[r_ri];
  assign ret_ok_o          = r_ok[r_ri];
  assign ret_refused_o     = r_refused[r_ri];
  assign ret_reason_o      = r_reason[r_ri];
  assign ret_loom_reason_o = r_lreason[r_ri];
  assign ret_nodes_o       = r_nodes[r_ri];
  assign ret_plan_o        = r_plan[r_ri];

  // ==========================================================================
  // THE STREAM IN FLIGHT
  // ==========================================================================
  localparam logic [2:0] S_IDLE = 3'd0;  // waiting for a post with credit
  localparam logic [2:0] S_HDR  = 3'd1;  // reading and judging record 0
  localparam logic [2:0] S_READ = 3'd2;  // reading node record `rec_q`
  localparam logic [2:0] S_PLAY = 3'd3;  // offering the assembled node
  localparam logic [2:0] S_RET  = 3'd4;  // writing the return record

  logic [2:0]  s_q;
  logic [31:0] base_q;        // record 0's byte address
  logic [31:0] ticket_q;
  logic [31:0] plan_q;
  logic [15:0] count_q;       // nodes in this stream, from the header
  logic [15:0] rec_q;         // 1..count_q: the node record being fetched
  logic [15:0] done_q;        // nodes the loom has accepted
  logic [2:0]  reason_q;
  logic        ok_q;
  logic        refused_q;
  logic [2:0]  lreason_q;

  // Law 6: the loom may be holding a stream this block abandoned without a
  // `last`, in which case it is waiting for one -- either composing (S_RUN) or
  // swallowing (S_DRAIN). Both are released by a `last` and neither can be
  // released any other way.
  logic        stale_q;
  logic        flush_q;       // this PASS is the flush pass, not the real one
  // The loom refused this stream. LATCHED rather than acted on: the loom
  // drains to `last`, so the feed must not stop (law 4).
  logic        loom_ref_q;
  logic [15:0] ref_at_q;      // nodes delivered when the refusal arrived

  // The eight landed beats of the record being read.
  logic [63:0] w_q [0:7];
  logic [2:0]  beat_q;

  // The camera basis, loaded ONCE per stream from record 0 and never written
  // while S_PLAY runs. See the header: this is the anti-metadata-swap property.
  logic signed [31:0] cam_q [9];

  // ---- the one burst in flight --------------------------------------------
  localparam logic [1:0] B_IDLE = 2'd0;
  localparam logic [1:0] B_REQ  = 2'd1;   // request held until grant or err
  localparam logic [1:0] B_RD   = 2'd2;   // read beats landing

  logic [1:0]     b_q;
  logic [31:0]    b_addr_q;
  logic [ERW-1:0] err_run_q;
  // A burst this block still wants. Set with the address by the stream FSM,
  // cleared by the burst's last beat. It is what makes a TRANSIENT bridge
  // refusal re-offer rather than hang: B_REQ drops to B_IDLE on `err` so the
  // re-offer is a NEW request (the arbiter re-serves a held one), and B_IDLE
  // raises it again while this flag stands.
  logic           b_pending_q;

  assign hps_req_o.valid  = (b_q == B_REQ);
  assign hps_req_o.write  = 1'b0;
  assign hps_req_o.client = CLIENT;
  assign hps_req_o.addr   = b_addr_q;
  assign hps_req_o.len    = 7'd64;

  wire rbeat_c = (b_q == B_RD) && hps_rsp_i.beat_valid;
  // The record in `w_q` is complete and the engine is quiet.
  wire rec_landed_c = !b_pending_q && (b_q == B_IDLE);
  // LAW 5's permanent case, read by the stream FSM on the same edge the burst
  // engine sees the refusal.
  wire err_permanent_c = (b_q == B_REQ) && hps_rsp_i.err &&
                         (ERW'(err_run_q + ERW'(1)) >= ERW'(ERR_RETRY_N));

  // ---- the node the loom is being offered ---------------------------------
  // Combinational slices of the landed beats. Nothing is recomputed: every
  // field is a bit range of bytes the ARM wrote.
  wire [9:0] rec_node_c   = w_q[0][9:0];
  wire [9:0] rec_parent_c = w_q[0][19:10];

  assign lm_valid_o        = (s_q == S_PLAY);
  assign lm_node_index_o   = rec_node_c;
  assign lm_parent_index_o = rec_parent_c;
  assign lm_kind_o         = w_q[0][23:20];
  assign lm_axis_o         = w_q[0][25:24];
  assign lm_bodypatch_o    = w_q[0][26];
  assign lm_angle_o        = w_q[0][47:32];
  assign lm_src_id_o       = w_q[0][63:48];
  assign lm_first_o        = (rec_q == 16'd1);
  assign lm_last_o         = (rec_q == count_q);

  // Unrolled rather than looped: Quartus 17.0's subset is the constraint this
  // file is written to (charter 2), and twelve explicit lines cannot be read
  // two ways.
  assign lm_param_o[0]  = signed'(w_q[1][31:0]);
  assign lm_param_o[1]  = signed'(w_q[1][63:32]);
  assign lm_param_o[2]  = signed'(w_q[2][31:0]);
  assign lm_param_o[3]  = signed'(w_q[2][63:32]);
  assign lm_param_o[4]  = signed'(w_q[3][31:0]);
  assign lm_param_o[5]  = signed'(w_q[3][63:32]);
  assign lm_param_o[6]  = signed'(w_q[4][31:0]);
  assign lm_param_o[7]  = signed'(w_q[4][63:32]);
  assign lm_param_o[8]  = signed'(w_q[5][31:0]);
  assign lm_param_o[9]  = signed'(w_q[5][63:32]);
  assign lm_param_o[10] = signed'(w_q[6][31:0]);
  assign lm_param_o[11] = signed'(w_q[6][63:32]);

  assign lm_cam_basis_o[0] = cam_q[0];
  assign lm_cam_basis_o[1] = cam_q[1];
  assign lm_cam_basis_o[2] = cam_q[2];
  assign lm_cam_basis_o[3] = cam_q[3];
  assign lm_cam_basis_o[4] = cam_q[4];
  assign lm_cam_basis_o[5] = cam_q[5];
  assign lm_cam_basis_o[6] = cam_q[6];
  assign lm_cam_basis_o[7] = cam_q[7];
  assign lm_cam_basis_o[8] = cam_q[8];

  wire lm_fire_c = lm_valid_o && lm_ready_i;

  // The loom is ready and this block has nothing to give it. The number that
  // says whether the one-record-in-flight choice costs anything.
  wire feed_wait_c = lm_ready_i && !lm_valid_o &&
                     ((s_q == S_READ) || (s_q == S_HDR));

  // ---- the header's verdict (law 3) ---------------------------------------
  wire [15:0] hdr_count_c = w_q[0][47:32];
  wire        hdr_magic_ok_c = (w_q[0][31:0] == STREAM_MAGIC);
  wire        hdr_count_ok_c = (hdr_count_c != 16'd0) &&
                               (hdr_count_c <= 16'(MAX_NODES));

  // ---- the post's verdict (law 2) -----------------------------------------
  wire post_aligned_c = (q_base[q_ri][5:0] == 6'd0);

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      q_wr <= '0;
      q_rd <= '0;
      r_wr <= '0;
      r_rd <= '0;
      owed <= '0;
      s_q  <= S_IDLE;
      b_q  <= B_IDLE;
      base_q <= 32'd0;
      ticket_q <= 32'd0;
      plan_q <= 32'd0;
      count_q <= 16'd0;
      rec_q <= 16'd0;
      done_q <= 16'd0;
      reason_q <= RR_NONE;
      ok_q <= 1'b0;
      refused_q <= 1'b0;
      lreason_q <= 3'd0;
      stale_q <= 1'b0;
      flush_q <= 1'b0;
      loom_ref_q <= 1'b0;
      ref_at_q <= 16'd0;
      beat_q <= 3'd0;
      b_addr_q <= 32'd0;
      b_pending_q <= 1'b0;
      err_run_q <= '0;
      posts_o <= 32'd0;
      streams_o <= 32'd0;
      nodes_o <= 32'd0;
      bursts_o <= 32'd0;
      posts_refused_align_o <= 32'd0;
      headers_refused_o <= 32'd0;
      streams_refused_o <= 32'd0;
      streams_faulted_o <= 32'd0;
      streams_replayed_o <= 32'd0;
      post_stalls_o <= 32'd0;
      bridge_errs_o <= 32'd0;
      feed_wait_cycles_o <= 32'd0;
      ret_overflow_o <= 32'd0;
      for (i = 0; i < int'(POSTS); i = i + 1) begin
        q_base[i]   <= 32'd0;
        q_ticket[i] <= 32'd0;
      end
      for (i = 0; i < int'(RETQ); i = i + 1) begin
        r_ticket[i]  <= 32'd0;
        r_ok[i]      <= 1'b0;
        r_refused[i] <= 1'b0;
        r_reason[i]  <= RR_NONE;
        r_lreason[i] <= 3'd0;
        r_nodes[i]   <= 16'd0;
        r_plan[i]    <= 32'd0;
      end
      for (i = 0; i < 8; i = i + 1) w_q[i] <= 64'd0;
      for (i = 0; i < 9; i = i + 1) cam_q[i] <= 32'sd0;
    end else begin
      // ---- D1 intake (law 1) ------------------------------------------------
      if (post_valid_i && post_ready_o) begin
        q_base[q_wi]   <= post_base_i;
        q_ticket[q_wi] <= post_ticket_i;
        q_wr <= q_wr + 1'b1;
        if (posts_o != 32'hFFFF_FFFF) posts_o <= posts_o + 32'd1;
      end else if (post_valid_i && !post_ready_o) begin
        // HELD, not dropped. The count is cycles.
        if (post_stalls_o != 32'hFFFF_FFFF) post_stalls_o <= post_stalls_o + 32'd1;
      end

      // ---- D2 drain ---------------------------------------------------------
      if (ret_valid_o && ret_ready_i) r_rd <= r_rd + 1'b1;

      if (feed_wait_c && (feed_wait_cycles_o != 32'hFFFF_FFFF)) begin
        feed_wait_cycles_o <= feed_wait_cycles_o + 32'd1;
      end

      // ---- LAW 4: the loom's verdict, LATCHED --------------------------------
      // `zhao_geom_loom` accepts the offending beat (`in_ready_o` is a pure
      // state decode that never looks at the fault) and pulses `refuse_valid_o`
      // the cycle AFTER, from S_REF -- so the refusal always lands while this
      // block is between beats, in S_READ or S_PLAY, never on the fire edge.
      // It is latched rather than acted on because the loom then enters
      // S_DRAIN and SWALLOWS beats until the stream's own `last`. Stopping the
      // feed here would leave it draining, and the NEXT stream would be eaten
      // to ITS `last` with no refusal raised at all -- a whole frame lost
      // silently, which is strictly worse than the refusal being reported.
      // This is why law 4 reads "record, and play on".
      if (lm_refuse_valid_i && (s_q != S_IDLE) && (s_q != S_RET) && !loom_ref_q) begin
        loom_ref_q <= 1'b1;
        lreason_q  <= lm_refuse_reason_i;
        ref_at_q   <= done_q;
        if (!flush_q && (streams_refused_o != 32'hFFFF_FFFF)) begin
          streams_refused_o <= streams_refused_o + 32'd1;
        end
      end

      // ---- the burst engine -------------------------------------------------
      // Driven by the stream FSM, which sets B_REQ and the address together.
      case (b_q)
        B_IDLE: begin
          beat_q <= 3'd0;
          // RE-OFFER. A burst the FSM still wants and that the bridge refused
          // transiently comes back here as a NEW request.
          if (b_pending_q) b_q <= B_REQ;
        end

        // HELD UNTIL GRANTED OR REFUSED (law 5, owner ruling R54). The bridge
        // refuses a malformed burst or one that collides with a busy port with
        // `err` and NO grant. Every burst this block asks for is 64 bytes at a
        // 64-byte-aligned address -- law 2 refuses any base that is not -- so
        // the first cause is believed unreachable and the second is transient.
        // That belief is the reason `bridge_errs_o` is expected to read zero in
        // the console; it is NOT the handling, because `zhao_hps_arbiter_n`
        // re-serves a held request and a refusal this state could not see would
        // be an unbounded spin with every counter here frozen.
        B_REQ: begin
          if (hps_rsp_i.err) begin
            if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
            b_q <= B_IDLE;   // take the request DOWN, then re-offer
            if (ERW'(err_run_q + ERW'(1)) < ERW'(ERR_RETRY_N)) begin
              err_run_q <= ERW'(err_run_q + ERW'(1));
            end
            // The permanent case is handled by the stream FSM below, which owns
            // the return record; it reads `err_run_q` on the same edge.
          end else if (hps_grant_i) begin
            b_q       <= B_RD;
            err_run_q <= '0;
            beat_q    <= 3'd0;
            if (bursts_o != 32'hFFFF_FFFF) bursts_o <= bursts_o + 32'd1;
          end
        end

        // The bridge answers `err` only at the REQUEST, never once a burst is
        // granted, so a granted read runs to its last beat.
        B_RD: begin
          if (rbeat_c) begin
            w_q[beat_q] <= hps_rsp_i.data;
            beat_q <= beat_q + 3'd1;
            if (hps_rsp_i.last) begin
              b_q         <= B_IDLE;
              b_pending_q <= 1'b0;
            end
          end
        end

        default: b_q <= B_IDLE;
      endcase

      // ---- the stream FSM ---------------------------------------------------
      case (s_q)
        S_IDLE: begin
          if (!q_empty && ret_credit) begin
            ticket_q   <= q_ticket[q_ri];
            plan_q     <= cfg_plan_base_i;
            base_q     <= q_base[q_ri];
            done_q     <= 16'd0;
            rec_q      <= 16'd0;
            count_q    <= 16'd0;
            lreason_q  <= 3'd0;
            loom_ref_q <= 1'b0;
            ref_at_q   <= 16'd0;
            // LAW 6: a stream that starts while the loom holds an abandoned
            // one is played twice -- once to hand over the `last` the loom is
            // waiting for, once for real.
            flush_q    <= stale_q;
            owed       <= owed + 1'b1;
            q_rd       <= q_rd + 1'b1;
            if (!post_aligned_c) begin
              // LAW 2: refused before a burst is issued.
              ok_q      <= 1'b0;
              refused_q <= 1'b1;
              reason_q  <= RR_ALIGN;
              s_q       <= S_RET;
              if (posts_refused_align_o != 32'hFFFF_FFFF) begin
                posts_refused_align_o <= posts_refused_align_o + 32'd1;
              end
            end else begin
              b_addr_q    <= q_base[q_ri];
              b_pending_q <= 1'b1;
              b_q         <= B_REQ;
              err_run_q   <= '0;
              s_q         <= S_HDR;
            end
          end
        end

        // Record 0. The burst engine lands eight beats; judge them when the
        // engine is quiet again.
        S_HDR: begin
          if (err_permanent_c) begin
            // LAW 5: permanent. Nothing has reached the loom, so the loom is
            // NOT left mid-stream and `stale_q` is not set.
            ok_q        <= 1'b0;
            refused_q   <= 1'b0;
            reason_q    <= RR_BRIDGE;
            err_run_q   <= '0;
            b_pending_q <= 1'b0;
            b_q         <= B_IDLE;
            s_q         <= S_RET;
            if (streams_faulted_o != 32'hFFFF_FFFF) begin
              streams_faulted_o <= streams_faulted_o + 32'd1;
            end
          end else if (rec_landed_c) begin
            if (!hdr_magic_ok_c) begin
              ok_q      <= 1'b0;
              refused_q <= 1'b1;
              reason_q  <= RR_MAGIC;
              s_q       <= S_RET;
              if (headers_refused_o != 32'hFFFF_FFFF) begin
                headers_refused_o <= headers_refused_o + 32'd1;
              end
            end else if (!hdr_count_ok_c) begin
              ok_q      <= 1'b0;
              refused_q <= 1'b1;
              reason_q  <= RR_COUNT;
              s_q       <= S_RET;
              if (headers_refused_o != 32'hFFFF_FFFF) begin
                headers_refused_o <= headers_refused_o + 32'd1;
              end
            end else begin
              // The basis is loaded ONCE, here, by the act that starts the
              // stream. Nothing writes `cam_q` again until the next S_HDR.
              count_q  <= hdr_count_c;
              cam_q[0] <= signed'(w_q[1][31:0]);
              cam_q[1] <= signed'(w_q[1][63:32]);
              cam_q[2] <= signed'(w_q[2][31:0]);
              cam_q[3] <= signed'(w_q[2][63:32]);
              cam_q[4] <= signed'(w_q[3][31:0]);
              cam_q[5] <= signed'(w_q[3][63:32]);
              cam_q[6] <= signed'(w_q[4][31:0]);
              cam_q[7] <= signed'(w_q[4][63:32]);
              cam_q[8]    <= signed'(w_q[5][31:0]);
              rec_q       <= 16'd1;
              b_addr_q    <= base_q + 32'd64;
              b_pending_q <= 1'b1;
              b_q         <= B_REQ;
              s_q         <= S_READ;
            end
          end
        end

        // A node record is in flight.
        S_READ: begin
          if (err_permanent_c) begin
            // LAW 5. If any beat has already reached the loom, the stream was
            // opened without a `last`, so the loom IS left mid-stream and the
            // next stream pays law 6's single replay. If none has -- the fault
            // landed on record 1 -- nothing was opened and there is nothing to
            // drop, which is why `stale_q` follows `done_q` rather than being
            // set unconditionally. Getting that wrong would have thrown away a
            // good stream's first node for a stream that never started.
            ok_q        <= 1'b0;
            refused_q   <= 1'b0;
            reason_q    <= RR_BRIDGE;
            err_run_q   <= '0;
            stale_q     <= (done_q != 16'd0);
            b_pending_q <= 1'b0;
            b_q         <= B_IDLE;
            s_q         <= S_RET;
            if (streams_faulted_o != 32'hFFFF_FFFF) begin
              streams_faulted_o <= streams_faulted_o + 32'd1;
            end
          end else if (rec_landed_c) begin
            s_q <= S_PLAY;
          end
        end

        // The record is offered to the loom. The loom's verdict is RECORDED
        // above and never re-derived (law 4); the stream is played to its
        // `last` either way, because that is what releases the loom's S_DRAIN.
        S_PLAY: begin
          if (lm_fire_c) begin
            done_q <= done_q + 16'd1;
            if (nodes_o != 32'hFFFF_FFFF) nodes_o <= nodes_o + 32'd1;
            if (rec_q == count_q) begin
              // `last` went with this beat. Whatever the loom was doing --
              // composing this stream, DRAINING it after a refusal, or draining
              // an abandoned one -- a `last` ends it and the scrub follows, so
              // the loom is IDLE next. That is the property law 6 stands on.
              if (flush_q) begin
                // LAW 6. This pass existed only to hand the loom the `last` it
                // was waiting for. It proves nothing about this stream and is
                // not reported; play the stream again, for real, into a loom
                // that is now provably idle. Bounded by construction:
                // `stale_q` is cleared here and only a fresh abandonment sets
                // it, so a stream is flushed at most once. NOTHING IS
                // FABRICATED -- the same bytes, re-read from the same addresses.
                flush_q     <= 1'b0;
                stale_q     <= 1'b0;
                loom_ref_q  <= 1'b0;
                lreason_q   <= 3'd0;
                ref_at_q    <= 16'd0;
                done_q      <= 16'd0;
                rec_q       <= 16'd1;
                b_addr_q    <= base_q + 32'd64;
                b_pending_q <= 1'b1;
                b_q         <= B_REQ;
                err_run_q   <= '0;
                s_q         <= S_READ;
                if (streams_replayed_o != 32'hFFFF_FFFF) begin
                  streams_replayed_o <= streams_replayed_o + 32'd1;
                end
              end else if (loom_ref_q) begin
                ok_q      <= 1'b0;
                refused_q <= 1'b0;
                reason_q  <= RR_LOOM;
                stale_q   <= 1'b0;
                s_q       <= S_RET;
              end else begin
                ok_q      <= 1'b1;
                refused_q <= 1'b0;
                reason_q  <= RR_NONE;
                stale_q   <= 1'b0;
                s_q       <= S_RET;
                if (streams_o != 32'hFFFF_FFFF) streams_o <= streams_o + 32'd1;
              end
            end else begin
              rec_q       <= rec_q + 16'd1;
              b_addr_q    <= base_q + 32'({rec_q + 16'd1, 6'd0});
              b_pending_q <= 1'b1;
              b_q         <= B_REQ;
              err_run_q   <= '0;
              s_q         <= S_READ;
            end
          end
        end

        // The return record. The space was reserved when the post was consumed
        // (law 7), so this write cannot be refused; `ret_overflow_o` says so if
        // it ever is.
        S_RET: begin
          r_ticket[r_wi]  <= ticket_q;
          r_ok[r_wi]      <= ok_q;
          r_refused[r_wi] <= refused_q;
          r_reason[r_wi]  <= reason_q;
          r_lreason[r_wi] <= lreason_q;
          // HOW FAR IT GOT. On a clean stream that is every node; on a refused
          // one it is the count AT THE REFUSAL, not the larger number the loom
          // then swallowed into its drain -- reporting the drained total would
          // tell the ARM the stream nearly worked when the fault was at node 2.
          r_nodes[r_wi]   <= loom_ref_q ? ref_at_q : done_q;
          r_plan[r_wi]    <= plan_q;
          r_wr  <= r_wr + 1'b1;
          owed  <= owed - 1'b1;
          s_q   <= S_IDLE;
          if (r_used == (RETW+1)'(RETQ)) begin
            if (ret_overflow_o != 32'hFFFF_FFFF) begin
              ret_overflow_o <= ret_overflow_o + 32'd1;
            end
          end
        end

        default: s_q <= S_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
