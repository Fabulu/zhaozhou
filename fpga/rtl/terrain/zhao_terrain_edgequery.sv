// zhao_terrain_edgequery.sv -- THE EMIT-SIDE QUERY DRIVER. It asks
// TERRAIN.EDGERECON for the served patch's four neighbour border rows, holds
// the answer across the patch job, and GATES the descriptor stream so that no
// subpatch of a patch can be decided before that patch's edges are known.
//
// Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt section 2
//      design/contracts/TERRAIN.EDGERECON.md -- "the EMIT-side query driver:
//        the time-share files during PREPARE, but who QUERIES
//        `zhao_terrain_edgerecon` during EMIT, with which `q_ix_i`/`q_iz_i`,
//        is still the caller's business -- this contract's original exclusion
//        stands." (WHAT P4 STILL OWES, item 6)
//      design/contracts/TERRAIN.LOD.md -- `edge_*` "must be held stable across
//        a patch job"
//
// ===========================================================================
// THE DECISION THIS BLOCK IS
// ===========================================================================
// QUESTION. TERRAIN.EDGERECON answers a query keyed on a PATCH COORDINATE.
// The EMIT spine has no patch coordinate anywhere near TERRAIN.LOD:
// `zhao_terrain_spdesc` carries `src_id` and fifteen other fields and no
// `ix`/`iz`, and `zhao_terrain_lod` has no coordinate port at all. So
// SOMETHING must carry the coordinate from where the console knows it (the
// page header beat) to where the query must be asked (the serve edge), AND
// must guarantee the answer is published before the first descriptor of that
// patch is decided -- because an `edge_*` that changes mid-patch is a crack
// whose cause is a handshake, which is precisely the defect TERRAIN.EDGERECON's
// own case 12 exists for one level down.
//
// CHOSEN OPTION. A named block on the EMIT descriptor path, holding a door
// queue of `{ix, iz, src_id}` pushed by the SAME pulse that pushes
// `zhao_terrain_spdesc`'s door and popped by the SAME serve edge, a seven-clock
// query FSM, and a GATE on the descriptor stream. The gate is what turns "the
// query is much faster than the devstore read, so it will always be ready" from
// an argument into a structure.
//
// REASON / ALTERNATIVES.
//   * Adding `ix`/`iz` to `zhao_terrain_spdesc`'s door and `sp_*` stream: a
//     port change on a composed, proven block plus a regeneration of three
//     generated tops, to carry a field only this consumer wants. Refused; the
//     door-queue pair is this file's established answer to "two keyings, one
//     event" and it already has a fired detector.
//   * Querying on the door push instead of the serve edge: the door is depth
//     four, so up to four patches can be queued while the ladder decides one,
//     and the answer would be for the wrong patch.
//   * No gate, relying on the timing margin: the query is 7 clocks against a
//     devstore read of five SDRAM bursts, so it "always" wins -- and "always"
//     that rests on a bandwidth figure is the shape this campaign has been
//     burned by repeatedly. The gate costs a comparator.
//
// CONSTRAINTS / COST. A 4-entry x 48-bit queue, a 3-state FSM, one 36-bit
// answer register, nine counters. 0 DSP, 0 M10K.
//
// CONSEQUENCES. No existing block changes. `zhao_terrain_spdesc`,
// `zhao_terrain_lodshare`, `zhao_terrain_lod` and `zhao_terrain_edgerecon`
// keep every port they have.
//
// ===========================================================================
// THE SIDE QUEUE, AND THE DETECTOR THAT MAKES IT HONEST
// ===========================================================================
// "Metadata travels WITH the record" is the rule, and a side queue is exactly
// the shape that breaks it. The mitigation is the one `zhao_console_core.sv`
// already argues for its DOOR/CTX pair: TWO QUEUES OF EQUAL DEPTH, PUSHED BY
// ONE EVENT AND POPPED BY ONE EVENT, hold equal occupancy at every instant.
// This block's queue is the third of that set, with the same depth, the same
// push (`zhao_terrain_compcache`'s `fill_accept_o`) and the same pop (the
// rising edge of `serve_valid_i`, `zhao_terrain_jobissue`'s law).
//
// AND IT IS NOT LEFT AS AN ARGUMENT. `serve_src_mismatch_o` differences the
// POPPED `src_id` -- written into the queue on the door pulse -- against
// `serve_src_id_i`, which is combinational off the compose cache's own serve
// parity. TWO VALUES, TWO ENABLES, TWO PORTS. That is the same detector
// `zhao_terrain_spdesc`'s `door_src_mismatch_o` is, deliberately, because a
// run in which this block's queue desyncs and that block's does not is a
// defect in the composition and the pair is what says so.
//
// ===========================================================================
// WHY A FALLBACK IS ALWAYS AVAILABLE AND NEVER A DEADLOCK
// ===========================================================================
// `8'h00` on all four edges is the console's EXISTING behaviour and therefore
// always a safe answer -- conservative in triangles, not in cracks, which is
// entry I21 item (d)'s correction and the reason the real producer was built.
// This block takes it whenever the bank cannot be trusted:
//   * `bank_emit_i` low -- the reconciler is sweeping or preparing;
//   * `prep_valid_i` low -- the walk did not complete, or its freeze broke;
//   * the door queue was empty at a serve (`serve_no_door_o`);
//   * `bank_emit_i` dropped while a query was in flight (`query_abandoned_o`)
//     -- a frame boundary landing mid-patch, which is a real event and not a
//     fault;
//   * a descriptor arrived with no armed answer and no query pending
//     (`descriptor_unarmed_o`) -- the safety valve that makes the gate
//     incapable of deadlocking. It is a fault and it is counted as one.
// Each has its own counter, so "everything fell back" is never one number.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_terrain_edgequery #(
    parameter int unsigned DEVW   = 24,
    parameter int unsigned MORPHW = 17,
    // The door queue's depth. It MUST equal `zhao_terrain_spdesc`'s `DOORD`
    // and `zhao_terrain_jobissue`'s `CTXD`: the equal-occupancy argument in
    // the header is the whole safety case, and it is an equality, not an
    // inequality. The elaboration guard below refuses a value that is not a
    // power of two, and `zhao_console_core.sv` passes one named constant to
    // all three.
    parameter int unsigned DOORD = 4,
    parameter int unsigned CW    = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the ADMITTED RECORD, as the page's compose job is accepted --------
    // `{ix, iz, src_id}` straight off TERRAIN.SEQ's issue port, captured on
    // the cycle TERRAIN.HDRREAD accepts that job. THIS IS THE SEALED LIST'S
    // OWN COORDINATE, which is the same field `zhao_terrain_prepwalk` files
    // the bank under -- not the page header's copy, which the header reader
    // only CHECKS against it. A query keyed on a different field from the
    // filing would miss every time and fall back in silence.
    input  var logic               rec_valid_i,
    input  var logic signed [15:0] rec_ix_i,
    input  var logic signed [15:0] rec_iz_i,
    input  var logic        [15:0] rec_src_id_i,

    // ---- the compose door, the SAME pulse TERRAIN.SPDESC's door takes ------
    // It carries only the identity; the coordinate is LOOKED UP by it in the
    // record hold above. See "WHY THE COORDINATE IS MATCHED AND NOT HELD".
    input  var logic               door_valid_i,
    output var logic               door_ready_o,
    input  var logic        [15:0] door_src_id_i,

    // ---- the serve edge ----------------------------------------------------
    input  var logic        serve_valid_i,     // LEVEL: a patch is served
    input  var logic [15:0] serve_src_id_i,

    // ---- the EMIT descriptor stream, IN from TERRAIN.SPDESC ----------------
    input  var logic               e_sp_valid_i,
    output var logic               e_sp_ready_o,
    input  var logic signed [31:0] e_sp_cx_i,
    input  var logic signed [31:0] e_sp_cy_i,
    input  var logic signed [31:0] e_sp_cz_i,
    input  var logic [DEVW-1:0]    e_sp_dev1_i,
    input  var logic [DEVW-1:0]    e_sp_dev2_i,
    input  var logic [DEVW-1:0]    e_sp_dev3_i,
    input  var logic [1:0]         e_sp_prev_level_i,
    input  var logic [MORPHW-1:0]  e_sp_prev_morph_i,
    input  var logic [7:0]         e_sp_hold_i,
    input  var logic [15:0]        e_sp_src_id_i,

    // ---- and OUT to TERRAIN.LODSHARE's EMIT client -------------------------
    // A PURE PASSTHROUGH of every payload field. Only the handshake is gated,
    // and the payload is wired straight across rather than registered so that
    // nothing can be reordered relative to the handshake it rides.
    output var logic               o_sp_valid_o,
    input  var logic               o_sp_ready_i,
    output var logic signed [31:0] o_sp_cx_o,
    output var logic signed [31:0] o_sp_cy_o,
    output var logic signed [31:0] o_sp_cz_o,
    output var logic [DEVW-1:0]    o_sp_dev1_o,
    output var logic [DEVW-1:0]    o_sp_dev2_o,
    output var logic [DEVW-1:0]    o_sp_dev3_o,
    output var logic [1:0]         o_sp_prev_level_o,
    output var logic [MORPHW-1:0]  o_sp_prev_morph_o,
    output var logic [7:0]         o_sp_hold_o,
    output var logic [15:0]        o_sp_src_id_o,

    // ---- TERRAIN.EDGERECON's QUERY port ------------------------------------
    output var logic               q_valid_o,
    input  var logic               q_ready_i,
    output var logic signed [15:0] q_ix_o,
    output var logic signed [15:0] q_iz_o,
    input  var logic               q_done_i,     // PULSE: the four words are new
    input  var logic [7:0]         q_edge_nz_i,
    input  var logic [7:0]         q_edge_pz_i,
    input  var logic [7:0]         q_edge_nx_i,
    input  var logic [7:0]         q_edge_px_i,
    input  var logic [3:0]         q_edge_real_i,

    // ---- the bank's trustworthiness ----------------------------------------
    input  var logic bank_emit_i,    // TERRAIN.EDGERECON `phase_o == 2`
    input  var logic prep_valid_i,   // TERRAIN.PREPWALK `prep_valid_o`

    // ---- the answer, HELD, to TERRAIN.LOD ----------------------------------
    output var logic [7:0] edge_nz_o,
    output var logic [7:0] edge_pz_o,
    output var logic [7:0] edge_nx_o,
    output var logic [7:0] edge_px_o,

    // ---- counters -----------------------------------------------------------
    output var logic [CW-1:0] patches_queued_o,
    output var logic [CW-1:0] door_refused_o,          // the queue was full at a push
    // THE DOOR'S IDENTITY DID NOT MATCH EITHER HELD RECORD. The entry is still
    // pushed, with a coordinate no patch can carry, so the patch falls back on
    // all four seams and its neighbours fall back symmetrically -- correct,
    // and loud.
    output var logic [CW-1:0] door_src_unknown_o,
    output var logic [CW-1:0] serve_no_door_o,         // a serve with an empty queue
    output var logic [CW-1:0] serve_src_mismatch_o,    // THE DETECTOR
    output var logic [CW-1:0] queries_issued_o,
    output var logic [CW-1:0] queries_answered_o,
    output var logic [CW-1:0] edges_real_o,            // lanes answered from a decision
    output var logic [CW-1:0] fallback_patches_o,      // patches given 8'h00 on all four
    output var logic [CW-1:0] query_abandoned_o,       // the bank left EMIT mid-query
    output var logic [CW-1:0] descriptor_unarmed_o,    // the safety valve fired
    output var logic [CW-1:0] gate_wait_clocks_o,      // CLOCKS a descriptor waited
    output var logic          busy_o
);

  localparam int unsigned DPTRW = (DOORD <= 1) ? 1 : $clog2(DOORD);
  // THE OCCUPANCY COUNTER IS ONE BIT WIDER THAN THE POINTERS, and it needs its
  // own named width rather than `DPTRW'(DOORD)`. At DOORD=4, DPTRW is 2 and
  // `DPTRW'(4)` is 2'd0 -- so the full-guard would read "empty" and the queue
  // would overwrite itself with `door_refused_o` flat beside it. Caught here
  // rather than in a bench, and named so it cannot come back.
  localparam int unsigned CNTW = DPTRW + 1;

  // ---- the door queue -------------------------------------------------------
  logic signed [15:0] dq_ix  [DOORD];
  logic signed [15:0] dq_iz  [DOORD];
  logic        [15:0] dq_src [DOORD];
  logic [DPTRW-1:0]   dq_wr_q, dq_rd_q;
  logic [CNTW-1:0]    dq_cnt_q;

  wire dq_full_c  = (dq_cnt_q == CNTW'(DOORD));
  wire dq_empty_c = (dq_cnt_q == '0);

  // The door NEVER gates the push, exactly as TERRAIN.SPDESC's and
  // TERRAIN.JOBISSUE's do not: `door_valid_i` is the compose cache ACCEPTING a
  // fill -- a fact that has already happened, not a request this block may
  // decline. A full queue REFUSES the entry and COUNTS it.
  assign door_ready_o = !dq_full_c;

  wire dq_push_c = door_valid_i && !dq_full_c;

  // ===========================================================================
  // WHY THE COORDINATE IS MATCHED AND NOT HELD
  // ===========================================================================
  // The obvious wiring is one register holding `{ix, iz}`, loaded on the job
  // accept, read at the door pulse -- which is what `bkr_pg_ix_q` does a few
  // thousand lines away in `zhao_console_core.sv`, with an argument that reads
  // exactly right: "the block takes one job at a time, so the pair held here
  // always belongs to the job now being forwarded".
  //
  // THAT ARGUMENT IS ABOUT THE FORWARD, AND THIS BLOCK'S PULSE IS LATER THAN
  // THE FORWARD. TERRAIN.HDRREAD returns to idle once TERRAIN.PAGESTREAM takes
  // the job; the compose cache's `fill_accept` comes later still, on the first
  // vertex beat. In the window between them the header reader may accept and
  // emit the NEXT job, and a single held register would then describe the
  // wrong patch -- a query against a coordinate the frame does not contain,
  // answered with the fallback, in total silence.
  //
  // So TWO records are held and the door SELECTS BY IDENTITY. Two is the
  // bound, not a guess: the header reader holds one job (`j_ready_o` is low
  // while it is busy) and the streamer holds one, so at most two admitted
  // records can be in flight between the issue port and a fill acceptance. A
  // door whose `src_id` matches neither is pushed with `UNKNOWN_IX` -- a
  // coordinate no patch can carry, because the island extent is 125 and the
  // direct-mapped bank's tag compare fails on it exactly as an off-island
  // neighbour's `16'hFFFF` does -- and counted on `door_src_unknown_o`.
  //
  // This is strictly stronger than the precedent it departs from, and the
  // departure is deliberate: a timing argument becomes an identity match.
  localparam logic signed [15:0] UNKNOWN_IX = 16'sh4000;

  logic signed [15:0] rh_ix  [2];
  logic signed [15:0] rh_iz  [2];
  logic        [15:0] rh_src [2];
  logic        [1:0]  rh_v;
  logic               rh_wr_q;

  wire rh_hit0_c = rh_v[0] && (rh_src[0] == door_src_id_i);
  wire rh_hit1_c = rh_v[1] && (rh_src[1] == door_src_id_i);
  wire rh_miss_c = !rh_hit0_c && !rh_hit1_c;

  wire signed [15:0] push_ix_c = rh_hit0_c ? rh_ix[0] : rh_hit1_c ? rh_ix[1] : UNKNOWN_IX;
  wire signed [15:0] push_iz_c = rh_hit0_c ? rh_iz[0] : rh_hit1_c ? rh_iz[1] : UNKNOWN_IX;

  // ---- the serve edge -------------------------------------------------------
  // ONE POP PER RISING `serve_valid_i`, which is `zhao_terrain_spdesc:394`'s
  // law verbatim so the two queues advance together.
  logic serve_seen_q;
  wire  serve_edge_c = serve_valid_i && !serve_seen_q;
  wire  dq_pop_c     = serve_edge_c && !dq_empty_c;

  // ---- the query FSM --------------------------------------------------------
  localparam logic [1:0] Q_IDLE = 2'd0;
  localparam logic [1:0] Q_REQ  = 2'd1;
  localparam logic [1:0] Q_WAIT = 2'd2;

  logic [1:0]         qst_q;
  logic signed [15:0] q_ix_q, q_iz_q;
  logic               armed_q;     // the four words below describe THIS patch
  logic [7:0]         e_nz_q, e_pz_q, e_nx_q, e_px_q;

  assign q_valid_o = (qst_q == Q_REQ);
  assign q_ix_o    = q_ix_q;
  assign q_iz_o    = q_iz_q;

  assign edge_nz_o = e_nz_q;
  assign edge_pz_o = e_pz_q;
  assign edge_nx_o = e_nx_q;
  assign edge_px_o = e_px_q;

  assign busy_o = (qst_q != Q_IDLE) || !dq_empty_c;

  // ---- the gate -------------------------------------------------------------
  // The payload is a straight passthrough; only the handshake is qualified.
  assign o_sp_valid_o      = e_sp_valid_i && armed_q;
  assign e_sp_ready_o      = o_sp_ready_i && armed_q;
  assign o_sp_cx_o         = e_sp_cx_i;
  assign o_sp_cy_o         = e_sp_cy_i;
  assign o_sp_cz_o         = e_sp_cz_i;
  assign o_sp_dev1_o       = e_sp_dev1_i;
  assign o_sp_dev2_o       = e_sp_dev2_i;
  assign o_sp_dev3_o       = e_sp_dev3_i;
  assign o_sp_prev_level_o = e_sp_prev_level_i;
  assign o_sp_prev_morph_o = e_sp_prev_morph_i;
  assign o_sp_hold_o       = e_sp_hold_i;
  assign o_sp_src_id_o     = e_sp_src_id_i;

  // THE SAFETY VALVE. A descriptor is waiting, nothing is armed, and there is
  // no query in flight and no serve edge arriving to start one. Arming with
  // the conservative fallback keeps the gate incapable of deadlocking; the
  // counter is what stops it being a silent degradation.
  wire stuck_c = e_sp_valid_i && !armed_q && (qst_q == Q_IDLE) && !serve_edge_c;

  // How many of the four lanes the bank answered from a real decision. Counted
  // rather than exported per patch: `edge_real_o` is a per-query fact and this
  // module's consumer is a counter catalogue.
  wire [2:0] real_lanes_c = {2'd0, q_edge_real_i[0]} + {2'd0, q_edge_real_i[1]} +
                            {2'd0, q_edge_real_i[2]} + {2'd0, q_edge_real_i[3]};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dq_wr_q              <= '0;
      dq_rd_q              <= '0;
      dq_cnt_q             <= '0;
      rh_v                 <= 2'b00;
      rh_wr_q              <= 1'b0;
      serve_seen_q         <= 1'b0;
      qst_q                <= Q_IDLE;
      q_ix_q               <= '0;
      q_iz_q               <= '0;
      // RESET INTO THE FALLBACK, ARMED. An empty bank answers `8'h00` and that
      // is the console's existing behaviour, so it is the safe state to come
      // out of reset into -- the same argument TERRAIN.EDGERECON makes for its
      // own sweep.
      armed_q              <= 1'b1;
      e_nz_q               <= 8'h00;
      e_pz_q               <= 8'h00;
      e_nx_q               <= 8'h00;
      e_px_q               <= 8'h00;
      patches_queued_o     <= '0;
      door_refused_o       <= '0;
      door_src_unknown_o   <= '0;
      serve_no_door_o      <= '0;
      serve_src_mismatch_o <= '0;
      queries_issued_o     <= '0;
      queries_answered_o   <= '0;
      edges_real_o         <= '0;
      fallback_patches_o   <= '0;
      query_abandoned_o    <= '0;
      descriptor_unarmed_o <= '0;
      gate_wait_clocks_o   <= '0;
    end else begin
      // ---- the record hold, two deep --------------------------------------
      if (rec_valid_i) begin
        rh_ix [rh_wr_q] <= rec_ix_i;
        rh_iz [rh_wr_q] <= rec_iz_i;
        rh_src[rh_wr_q] <= rec_src_id_i;
        rh_v[rh_wr_q]   <= 1'b1;
        rh_wr_q         <= !rh_wr_q;
      end

      // ---- the door -------------------------------------------------------
      if (dq_push_c) begin
        dq_ix [dq_wr_q] <= push_ix_c;
        dq_iz [dq_wr_q] <= push_iz_c;
        if (rh_miss_c && (door_src_unknown_o != {CW{1'b1}}))
          door_src_unknown_o <= door_src_unknown_o + CW'(1);
        dq_src[dq_wr_q] <= door_src_id_i;
        dq_wr_q <= (dq_wr_q == DPTRW'(DOORD - 1)) ? '0 : (dq_wr_q + DPTRW'(1));
        if (patches_queued_o != {CW{1'b1}}) patches_queued_o <= patches_queued_o + CW'(1);
      end else if (door_valid_i) begin
        if (door_refused_o != {CW{1'b1}}) door_refused_o <= door_refused_o + CW'(1);
      end

      if (dq_pop_c) begin
        dq_rd_q <= (dq_rd_q == DPTRW'(DOORD - 1)) ? '0 : (dq_rd_q + DPTRW'(1));
      end

      if (dq_push_c && !dq_pop_c)      dq_cnt_q <= dq_cnt_q + 1'b1;
      else if (dq_pop_c && !dq_push_c) dq_cnt_q <= dq_cnt_q - 1'b1;

      // ---- the serve edge -------------------------------------------------
      if (!serve_valid_i) serve_seen_q <= 1'b0;
      else if (serve_edge_c) serve_seen_q <= 1'b1;

      if (serve_edge_c) begin
        // The answer stops describing the outgoing patch the instant a new one
        // is served, whatever happens next.
        armed_q <= 1'b0;
        if (dq_empty_c) begin
          if (serve_no_door_o != {CW{1'b1}}) serve_no_door_o <= serve_no_door_o + CW'(1);
          // Nothing names the patch, so nothing can be asked. Fall back.
          e_nz_q  <= 8'h00;
          e_pz_q  <= 8'h00;
          e_nx_q  <= 8'h00;
          e_px_q  <= 8'h00;
          armed_q <= 1'b1;
          if (fallback_patches_o != {CW{1'b1}})
            fallback_patches_o <= fallback_patches_o + CW'(1);
        end else begin
          // THE DETECTOR. The popped id was written on the door pulse; the
          // served id is combinational off the cache's serve parity.
          if (dq_src[dq_rd_q] != serve_src_id_i) begin
            if (serve_src_mismatch_o != {CW{1'b1}})
              serve_src_mismatch_o <= serve_src_mismatch_o + CW'(1);
          end
          q_ix_q <= dq_ix[dq_rd_q];
          q_iz_q <= dq_iz[dq_rd_q];
          if (bank_emit_i && prep_valid_i) begin
            qst_q <= Q_REQ;
          end else begin
            e_nz_q  <= 8'h00;
            e_pz_q  <= 8'h00;
            e_nx_q  <= 8'h00;
            e_px_q  <= 8'h00;
            armed_q <= 1'b1;
            if (fallback_patches_o != {CW{1'b1}})
              fallback_patches_o <= fallback_patches_o + CW'(1);
          end
        end
      end

      // ---- the query FSM --------------------------------------------------
      case (qst_q)
        Q_REQ: begin
          if (!bank_emit_i) begin
            // A frame boundary landed mid-patch and swept the bank. Real, not
            // a fault, and the answer for this patch is now the fallback.
            qst_q   <= Q_IDLE;
            e_nz_q  <= 8'h00;
            e_pz_q  <= 8'h00;
            e_nx_q  <= 8'h00;
            e_px_q  <= 8'h00;
            armed_q <= 1'b1;
            if (query_abandoned_o != {CW{1'b1}})
              query_abandoned_o <= query_abandoned_o + CW'(1);
            if (fallback_patches_o != {CW{1'b1}})
              fallback_patches_o <= fallback_patches_o + CW'(1);
          end else if (q_ready_i) begin
            qst_q <= Q_WAIT;
            if (queries_issued_o != {CW{1'b1}})
              queries_issued_o <= queries_issued_o + CW'(1);
          end
        end
        Q_WAIT: begin
          if (q_done_i) begin
            qst_q   <= Q_IDLE;
            e_nz_q  <= q_edge_nz_i;
            e_pz_q  <= q_edge_pz_i;
            e_nx_q  <= q_edge_nx_i;
            e_px_q  <= q_edge_px_i;
            armed_q <= 1'b1;
            if (queries_answered_o != {CW{1'b1}})
              queries_answered_o <= queries_answered_o + CW'(1);
            if (edges_real_o != {CW{1'b1}})
              edges_real_o <= edges_real_o + CW'(real_lanes_c);
            if ((q_edge_real_i == 4'h0) && (fallback_patches_o != {CW{1'b1}}))
              fallback_patches_o <= fallback_patches_o + CW'(1);
          end else if (!bank_emit_i) begin
            qst_q   <= Q_IDLE;
            e_nz_q  <= 8'h00;
            e_pz_q  <= 8'h00;
            e_nx_q  <= 8'h00;
            e_px_q  <= 8'h00;
            armed_q <= 1'b1;
            if (query_abandoned_o != {CW{1'b1}})
              query_abandoned_o <= query_abandoned_o + CW'(1);
            if (fallback_patches_o != {CW{1'b1}})
              fallback_patches_o <= fallback_patches_o + CW'(1);
          end
        end
        default: begin
          // Q_IDLE, and the safety valve.
          if (stuck_c) begin
            e_nz_q  <= 8'h00;
            e_pz_q  <= 8'h00;
            e_nx_q  <= 8'h00;
            e_px_q  <= 8'h00;
            armed_q <= 1'b1;
            if (descriptor_unarmed_o != {CW{1'b1}})
              descriptor_unarmed_o <= descriptor_unarmed_o + CW'(1);
          end
        end
      endcase

      // ---- the gate's cost -------------------------------------------------
      if (e_sp_valid_i && !armed_q) begin
        if (gate_wait_clocks_o != {CW{1'b1}})
          gate_wait_clocks_o <= gate_wait_clocks_o + CW'(1);
      end
    end
  end

  // Elaboration guards. `initial begin ... end` and NOT a module-scope `if`:
  // Quartus 17.0 rejects the latter with "syntax error near text: `if`;
  // expecting `endmodule`". `--lint-only` does not run these, so a clean lint
  // says nothing whatever about them.
  // synthesis translate_off
  initial begin
    if ((DOORD < 2) || ((DOORD & (DOORD - 1)) != 0))
      $fatal(1, "zhao_terrain_edgequery: DOORD=%0d must be a power of two >= 2; the wrap arithmetic and the equal-occupancy argument against TERRAIN.SPDESC's door both depend on it", DOORD);
    if (CW < 8)
      $fatal(1, "zhao_terrain_edgequery: CW=%0d is too narrow to be a census", CW);
    if (DEVW == 0 || MORPHW == 0)
      $fatal(1, "zhao_terrain_edgequery: DEVW and MORPHW must be positive");
  end
  // synthesis translate_on

endmodule

`default_nettype wire
