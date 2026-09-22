// zhao_terrain_jobissue.sv — TERRAIN.JOBISSUE: the SUBPATCH JOB ISSUER, the
// block `zhao_console_core.sv` entry I21 says does not exist.
//
// Law, in citation order:
//   fpga/rtl/prod/zhao_console_core.sv, entry I21 — the statement of the
//       absence, in its own words: "`zhao_terrain_lod.sv` ... its own header
//       says its output is EXACTLY `zhao_terrain_tess`'s job port. It is not
//       the sequencer's job port: the sequencer additionally needs
//       `job_view_mask`, `job_mat_a`, `job_mat_b` and `job_weight`, and
//       TERRAIN.LOD emits none of the four. Taking its 12 matching fields and
//       inventing the other 4 here is the hidden-adapter failure."
//       AND, of the compose cache's retirement pulse: "The block that knows
//       when a patch is finished is the block that issued its subpatch jobs
//       and counted them home -- the same one this entry is about."
//       Both halves are this block. It is not an adapter in the core; it is a
//       file with a contract, a test and counters that discriminate.
//   OWNER RULING R13 (reports/OWNER-RULINGS-20260919-EVENING.md) — "Join PER
//       TRIANGLE, by the triangle's cell. The per-cell layer-E value is read at
//       tessellation, where the triangle's cell is known, and travels with the
//       triangle. **The job port is not widened to carry a subpatch-uniform
//       value that is not true.**"  So this block has NO `mat_a`, `mat_b` or
//       `weight` port, and that is a ruling obeyed rather than a field
//       forgotten. See WHAT IS DELIBERATELY NOT HERE.
//   spec/video_rules.md §3.1 (ratified 2026-08-15, MAJOR-3) — View 0 is P1 and
//       View 1 is P2. With ruling T5's `view_mask:u8` on `SubmitTerrainSet`
//       0x0230 ("which views this set was unioned for") that makes the door's
//       mask and `zhao_terrain_group_seq.job_view_mask_i` THE SAME QUANTITY at
//       two widths. Packet gz/viewmask settled this 2026-09-21 from five
//       independent sources; there is no player mask anywhere in the tree.
//   fpga/rtl/terrain/zhao_terrain_group_seq.sv — the consumer: one job in, one
//       arena group per view out, `job_ready_o = (st == StIdle)`, so a rising
//       `job_ready_i` after a job means that job has drained completely.
//   fpga/rtl/terrain/zhao_terrain_lod.sv — the producer of the decision: laws
//       5 ("the history rides the packet ... for the caller to store") and the
//       emit order, sixteen subpatches, each followed immediately by its
//       underside on a dual page: (0,top),(0,bot),(1,top)...(15,bot).
//   fpga/rtl/terrain/zhao_terrain_compcache_front.sv — `serve_valid_o` /
//       `serve_src_id_o` / `serve_release_i`: one patch per rising edge, and
//       the cache's own header records what a wrong reading of the release
//       already cost once (a whole patch retired without one vertex read).
//   fpga/rtl/terrain/zhao_terrain_devstore.sv — the history writeback port
//       `h_valid_i`/`h_level_i`/`h_morph_i`/`h_hold_i`, which is where
//       TERRAIN.LOD's `out_hold_o` is required to land.
//
// ENFORCED-BY: tests/terrain/terrain_jobissue_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE ONE IDEA: A JOB IS A DECISION JOINED TO ITS PATCH'S DRAW CONTEXT, AND
// THE JOIN IS CHECKED RATHER THAN ASSUMED
// ---------------------------------------------------------------------------
// Entry I21 refused, correctly, to drive `job_view_mask` from the compose
// door's `tis_view_mask` while the other job fields arrived from outside the
// core: that "joins two things that move independently". The objection is
// about INDEPENDENT MOVEMENT, not about the mask, and the remedy is not a
// wider port -- it is to make the two things move together and to be able to
// SEE when they do not.
//
// So the draw context ({view_mask, sparse_fill}, keyed by `src_id`) is CAPTURED
// at the compose door when the patch is handed to the streamer, queued, and
// popped when THAT patch reaches the cache's serve side. The popped context's
// `src_id` is then differenced against the serve port's own `src_id`.
//
// **AND THE TWO SIDES OF THAT COMPARISON ARE CLOCKED BY DIFFERENT ENABLES**,
// which is the whole reason the check is worth having. `CLAUDE.md`'s
// metadata-swap chapter is exactly this shape: a bank whose captured generation
// was loaded by the SAME ungated assignment as the row it guarded could never
// fire, because on a swap both operands moved to the new record together. Here
// the queued `src_id` is written by `ctx_valid_i && ctx_ready_o` (the door) and
// the compared `src_id` arrives on `serve_src_id_i` (the cache's fill side,
// many hundreds of clocks later and through a different path). A skew between
// them moves ONE operand. `ctx_src_mismatch_o` can therefore fire, and the
// directed test fires it deliberately before its silence is quoted anywhere.
//
// ---------------------------------------------------------------------------
// THE LIFETIME OF A PATCH
// ---------------------------------------------------------------------------
//   (door)    `ctx_valid_i` pushes {src_id, view_mask, sparse_fill}. The queue
//             is CTXD deep with a real `ctx_ready_o`; a context offered to a
//             full queue is REFUSED and counted, never overwritten.
//   ARM       `serve_valid_i` with the head context's src_id: pop it, latch it
//             as the active patch, and expect its decisions.
//             A serve with an EMPTY queue is counted (`serve_no_ctx_o`) and
//             nothing is issued.
//             A serve whose src_id does not match the head is counted
//             (`ctx_src_mismatch_o`), the patch is DROPPED, and the release is
//             pulsed anyway -- see THE DROP LAW below.
//   ISSUE     each `lod_target` packet is forked to (a) the sequencer's job
//             port with the active context's `view_mask` and `sparse_fill`
//             attached, and (b) the deviation store's history writeback. The
//             packet is consumed only when BOTH have taken it.
//             The expected count is latched from the FIRST packet's
//             `lod_dual_i`: 32 on a dual page, 16 otherwise, which is
//             TERRAIN.LOD's own emit law and not a number chosen here.
//   DRAIN     when the last job has been ACCEPTED, wait for `job_ready_i` to
//             return high. By the sequencer's own exit rule that is the cycle
//             its arenas are released and every reference has been accepted by
//             the shell -- so it is the earliest instant at which "TESS is
//             finished with the served patch" is TRUE rather than likely.
//   RETIRE    one-cycle `serve_release_o`. `patches_retired_o` counts it.
//
// THE DROP LAW, stated because it gives up function on a fault path. On a
// context mismatch the block issues NOTHING and retires the patch. The
// alternative -- issue with the head context anyway -- projects a patch into
// the wrong player's view, silently. The other alternative -- refuse to
// release -- wedges the compose engine with no timeout and no counter, which
// is the `zhao_geom_vattr` stall owner ruling R88 says "deserves a counter
// now ... nobody will be debugging shadows when it fires". A counted drop is
// the only one of the three that is visible. `patches_dropped_o` is that
// record and it is a SEPARATE counter from the mismatch, so a drop for any
// later reason cannot hide inside the mismatch's number.
//
// ---------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT HERE
// ---------------------------------------------------------------------------
//   * NO `mat_a` / `mat_b` / `weight`. Ruling R13 rules these three the WRONG
//     CARRIER: layer E is per CELL and a subpatch job covers 64 of them, so any
//     value this block put there would be a look law invented in the composer.
//     R13's honest closure for the sequencer's three ports is their REMOVAL
//     once a per-triangle layer-E path exists inside TESS -- a function MOVE,
//     and nothing may leave until the replacement lands. Until then the three
//     remain the core's declared boundary and this block does not pretend to
//     feed them. **The issuer owes thirteen fields, not sixteen.**
//   * NO TIMEOUT on a patch whose decisions never arrive, and NO FAULT FLAG
//     for a decision that arrives early. A timeout is a retirement POLICY and
//     the cache's header is explicit about what a wrong one cost; an
//     early-decision flag fires on ordinary backpressure. The two CLOCK
//     instruments stand in their place: `issue_clocks_o` climbs while a patch
//     is armed and starved, `decision_wait_clocks_o` climbs while a decision
//     waits for a patch to arm. Either one rising with `patches_retired_o`
//     FLAT is a wedge, readable from the counter block. Those are
//     measurements, not policies.
//   * NO devstore READ orchestration (`r_start_i`/`r_slot_i`). The store is
//     addressed by page SLOT and this block is keyed by `src_id`; inventing the
//     mapping here would be the second hidden adapter in a file written to
//     refuse the first. It belongs to whoever composes the store.
//   * NO reordering, no buffering of decisions, and no second patch in flight.
//     The compose cache is a two-buffer FRONT that serves one patch at a time.
//
// Conservative SystemVerilog subset only (charter §2); no package deps; the
// elaboration guards live in `initial begin ... end` because Quartus 17.0
// rejects a bare module-scope `if` ("syntax error near text: `if`; expecting
// `endmodule`"), and `--lint-only` does not run them.
`default_nettype none

module zhao_terrain_jobissue #(
    // The draw-context queue. The compose engine holds at most two patches
    // (one filling, one served) and the door may be one job ahead of the
    // streamer, so 4 is two clear of the composition that uses it. It is a
    // KNOB, not a derivation: a deeper door pipeline raises it.
    parameter int unsigned CTXD = 4,
    // TERRAIN.LOD's emit law: sixteen subpatches per patch, each followed by
    // its underside on a dual page. Named so the count this block waits for
    // cannot drift from the count that block emits.
    parameter int unsigned SUBPATCHES = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the patch DRAW CONTEXT, captured at the compose door ---------------
    // `view_mask` is T5's `SubmitTerrainSet.view_mask`, ALL EIGHT BITS, as the
    // page's own record carries it.
    //
    // THE NARROWING MOVED HERE 2026-09-22 (gz/terrclose), and the sentence it
    // replaces is kept because the reason it was written is the reason it had
    // to move. It read: "The narrowing is performed by the COMPOSER, at the
    // door, with its refusal counted there -- not here, where the field has
    // already lost its high bits." That was correct while no producer carried
    // the mask: the composer was the only place eight bits existed. Now
    // `zhao_terrain_pagestream` forwards `view_mask:u8` beside `flags:u16`
    // (core entry I21), so eight bits arrive HERE, and the block that consumes
    // the value is the block that should own the discard.
    //
    // AND THE DECIDING ARGUMENT IS NOT TIDINESS, IT IS THAT A COUNTER IN THE
    // COMPOSER CANNOT BE FIRED. The console smoke fails the CRC of every
    // terrain page it plays, so the compose door never opens in it and no
    // legal stimulus reaches a counter placed there; it would read zero for
    // ever and the zero would mean nothing. `terrain_jobissue_directed` drives
    // this port directly and fires `view_mask_high_o` on a record that sets a
    // bit this console has no view for, with a negative control beside it.
    //
    // THE DISCARD IS IGNORE, NOT REFUSE, and that is a decision entry I21
    // explicitly left to whoever composed the consumer. Bits [7:2] are not a
    // wider mask being truncated: `spec/commands.zidl`, ruling T5 and
    // `spec/video_rules.md` 3.1 all describe TWO views, `zref_sw_stream.hpp`
    // accumulates two bits and tests `== 0x3`, and `zhao_geom_group_seq`
    // $fatals unless NVIEWS == 2. Refusing a patch over bits no ratified
    // document gives a meaning to would drop legal content from a legal
    // stream. They are dropped and counted instead, so that the day a third
    // view is ratified the machine says so rather than a reviewer noticing.
    input  var logic        ctx_valid_i,
    output var logic        ctx_ready_o,
    input  var logic [15:0] ctx_src_id_i,
    input  var logic [ 7:0] ctx_view_mask_i,
    input  var logic        ctx_sparse_fill_i,

    // ---- TERRAIN.COMPCACHE's serve side -------------------------------------
    input  var logic        serve_valid_i,     // LEVEL: a patch is served
    input  var logic [15:0] serve_src_id_i,
    output var logic        serve_release_o,   // PULSE: one patch per edge

    // ---- TERRAIN.LOD's `lod_target` stream ----------------------------------
    input  var logic        lod_valid_i,
    output var logic        lod_ready_o,
    input  var logic [ 5:0] lod_ox_i,
    input  var logic [ 5:0] lod_oz_i,
    input  var logic [ 1:0] lod_level_i,
    input  var logic [ 1:0] lod_lvl_nz_i,
    input  var logic [ 1:0] lod_lvl_pz_i,
    input  var logic [ 1:0] lod_lvl_nx_i,
    input  var logic [ 1:0] lod_lvl_px_i,
    input  var logic [16:0] lod_morph_i,
    input  var logic        lod_surface_i,
    input  var logic        lod_dual_i,
    input  var logic [15:0] lod_src_id_i,
    input  var logic [ 7:0] lod_hold_i,

    // ---- TERRAIN.GROUP_SEQ's job port: THIRTEEN fields ----------------------
    output var logic        job_valid_o,
    input  var logic        job_ready_i,
    output var logic [ 5:0] job_ox_o,
    output var logic [ 5:0] job_oz_o,
    output var logic [ 1:0] job_level_o,
    output var logic [ 1:0] job_lvl_nz_o,
    output var logic [ 1:0] job_lvl_pz_o,
    output var logic [ 1:0] job_lvl_nx_o,
    output var logic [ 1:0] job_lvl_px_o,
    output var logic [16:0] job_morph_o,
    output var logic        job_surface_o,
    output var logic        job_dual_o,
    output var logic [15:0] job_src_id_o,
    output var logic [ 1:0] job_view_mask_o,
    output var logic        sparse_fill_o,

    // ---- TERRAIN.DEVSTORE's history writeback -------------------------------
    output var logic        h_valid_o,
    input  var logic        h_ready_i,
    output var logic [ 1:0] h_level_o,
    output var logic [16:0] h_morph_o,
    output var logic [ 7:0] h_hold_o,

    // ---- counters -----------------------------------------------------------
    output var logic [31:0] jobs_issued_o,
    output var logic [31:0] patches_retired_o,
    output var logic [31:0] patches_dropped_o,
    output var logic [31:0] ctx_refused_o,
    output var logic [31:0] serve_no_ctx_o,
    output var logic [31:0] ctx_src_mismatch_o,
    output var logic [31:0] lod_src_mismatch_o,
    // Contexts whose record set a view bit this console cannot project into.
    // Expected zero on ratified content; see the port comment above for why it
    // is a counter rather than a refusal, and `terrain_jobissue_directed` for
    // the stimulus that fires it.
    output var logic [31:0] view_mask_high_o,
    output var logic [31:0] decision_wait_clocks_o,
    output var logic [31:0] issue_clocks_o,
    output var logic        busy_o
);

  // --------------------------------------------------------------------------
  // Elaboration guards. Inside `initial begin ... end`: Quartus 17.0 rejects a
  // bare module-scope `if`, and a clean `--lint-only` says nothing about these.
  // --------------------------------------------------------------------------
  // verilator lint_off IGNOREDRETURN
  initial begin
    if (CTXD < 2) begin
      $fatal(1, "zhao_terrain_jobissue: CTXD must be at least 2 (fill + serve)");
    end
    if (SUBPATCHES != 16) begin
      $fatal(1, "zhao_terrain_jobissue: SUBPATCHES must be 16 -- zhao_terrain_lod emits sixteen");
    end
  end
  // verilator lint_on IGNOREDRETURN

  localparam int unsigned CNTW  = $clog2(CTXD) + 1;
  localparam int unsigned IDXW  = $clog2(CTXD);
  // 16 or 32 decisions per patch; 6 bits holds 32 and its terminal compare.
  localparam int unsigned EXPW  = 6;

  // --------------------------------------------------------------------------
  // THE DRAW-CONTEXT QUEUE
  // --------------------------------------------------------------------------
  logic [15:0]      cq_src   [CTXD];
  logic [ 1:0]      cq_mask  [CTXD];
  logic             cq_sparse[CTXD];
  logic [IDXW-1:0]  cq_wr_q, cq_rd_q;
  logic [CNTW-1:0]  cq_cnt_q;

  wire cq_full_c  = (cq_cnt_q == CNTW'(CTXD));
  wire cq_empty_c = (cq_cnt_q == '0);

  assign ctx_ready_o = !cq_full_c;

  wire cq_push_c = ctx_valid_i && ctx_ready_o;

  // --------------------------------------------------------------------------
  // THE PATCH FSM
  // --------------------------------------------------------------------------
  typedef enum logic [1:0] {StIdle, StIssue, StDrain} state_e;
  state_e st_q;

  logic [15:0]     act_src_q;
  logic [ 1:0]     act_mask_q;
  logic            act_sparse_q;
  logic [EXPW-1:0] act_expect_q;   // 0 until the first decision sets it
  logic [EXPW-1:0] act_count_q;
  logic            act_sized_q;    // `act_expect_q` is valid

  // The fork: a decision is consumed only when BOTH sinks have taken it.
  // `job_valid_o` and `h_valid_o` depend on no `*_ready_i`, so there is no
  // combinational valid<->ready loop with either consumer.
  logic job_taken_q, h_taken_q;

  wire issuing_c = (st_q == StIssue);

  wire job_fire_c = issuing_c && lod_valid_i && !job_taken_q;
  wire h_fire_c   = issuing_c && lod_valid_i && !h_taken_q;

  wire job_done_c = job_taken_q || (job_fire_c && job_ready_i);
  wire h_done_c   = h_taken_q   || (h_fire_c   && h_ready_i);

  wire dec_take_c = issuing_c && lod_valid_i && job_done_c && h_done_c;

  assign lod_ready_o = issuing_c && job_done_c && h_done_c;

  assign job_valid_o     = job_fire_c;
  assign job_ox_o        = lod_ox_i;
  assign job_oz_o        = lod_oz_i;
  assign job_level_o     = lod_level_i;
  assign job_lvl_nz_o    = lod_lvl_nz_i;
  assign job_lvl_pz_o    = lod_lvl_pz_i;
  assign job_lvl_nx_o    = lod_lvl_nx_i;
  assign job_lvl_px_o    = lod_lvl_px_i;
  assign job_morph_o     = lod_morph_i;
  assign job_surface_o   = lod_surface_i;
  assign job_dual_o      = lod_dual_i;
  assign job_src_id_o    = lod_src_id_i;
  // THE TWO FIELDS THAT ARE THIS BLOCK'S OWN, and the reason it exists: they
  // come from the patch's OWN context, popped for this patch's own src_id, so
  // they move with the job instead of beside it.
  assign job_view_mask_o = act_mask_q;
  assign sparse_fill_o   = act_sparse_q;

  assign h_valid_o = h_fire_c;
  assign h_level_o = lod_level_i;
  assign h_morph_o = lod_morph_i;
  assign h_hold_o  = lod_hold_i;

  // The count this patch owes, from TERRAIN.LOD's own emit law.
  wire [EXPW-1:0] expect_c =
      lod_dual_i ? EXPW'(SUBPATCHES * 2) : EXPW'(SUBPATCHES);

  wire last_dec_c = dec_take_c && act_sized_q &&
                    (act_count_q + EXPW'(1) == act_expect_q);

  // Edge detection for the two "offered and refused" counters, so a level held
  // for a thousand clocks is one event and not a thousand.
  logic serve_seen_q;     // this serve level has already been judged

  // A pop happens on the ONE cycle a served patch is judged against the head
  // of the queue -- whichever way the judgement goes, because leaving a
  // mismatched context at the head would mis-key every patch after it too.
  // Declared once, here, so the count and the read pointer cannot disagree:
  // an earlier draft updated `cq_cnt_q` inside four separate case arms and one
  // of them (a push landing in the same cycle as a serve with an EMPTY queue)
  // dropped the increment.
  wire cq_pop_c = (st_q == StIdle) && serve_valid_i && !serve_seen_q && !cq_empty_c;

  assign busy_o = (st_q != StIdle) || !cq_empty_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      cq_wr_q  <= '0;
      cq_rd_q  <= '0;
      cq_cnt_q <= '0;

      st_q         <= StIdle;
      act_src_q    <= '0;
      act_mask_q   <= '0;
      act_sparse_q <= 1'b0;
      act_expect_q <= '0;
      act_count_q  <= '0;
      act_sized_q  <= 1'b0;
      job_taken_q  <= 1'b0;
      h_taken_q    <= 1'b0;

      serve_release_o <= 1'b0;
      serve_seen_q    <= 1'b0;

      jobs_issued_o      <= '0;
      patches_retired_o  <= '0;
      patches_dropped_o  <= '0;
      ctx_refused_o      <= '0;
      serve_no_ctx_o     <= '0;
      ctx_src_mismatch_o <= '0;
      lod_src_mismatch_o <= '0;
      view_mask_high_o   <= '0;
      decision_wait_clocks_o <= '0;
      issue_clocks_o     <= '0;
    end else begin
      serve_release_o <= 1'b0;

      // ---- the context queue, accounted in ONE place --------------------
      if (cq_push_c) begin
        cq_src[cq_wr_q]    <= ctx_src_id_i;
        cq_mask[cq_wr_q]   <= ctx_view_mask_i[1:0];
        if ((ctx_view_mask_i[7:2] != 6'd0) &&
            (view_mask_high_o != 32'hFFFF_FFFF))
          view_mask_high_o <= view_mask_high_o + 32'd1;
        cq_sparse[cq_wr_q] <= ctx_sparse_fill_i;
        cq_wr_q            <= (cq_wr_q == IDXW'(CTXD - 1)) ? '0 : cq_wr_q + IDXW'(1);
      end
      if (cq_pop_c) begin
        cq_rd_q <= (cq_rd_q == IDXW'(CTXD - 1)) ? '0 : cq_rd_q + IDXW'(1);
      end
      if (cq_push_c && !cq_pop_c)      cq_cnt_q <= cq_cnt_q + CNTW'(1);
      else if (cq_pop_c && !cq_push_c) cq_cnt_q <= cq_cnt_q - CNTW'(1);

      if (ctx_valid_i && !ctx_ready_o && (ctx_refused_o != 32'hFFFF_FFFF)) begin
        ctx_refused_o <= ctx_refused_o + 32'd1;
      end

      // ---- the DECISION-WAIT instrument --------------------------------
      // Clocks spent with a decision OFFERED and no patch armed. It is a
      // MEASUREMENT, like `issue_clocks_o`, and deliberately not a fault
      // counter -- which is a correction this block's own directed test
      // forced, twice, and it is the more useful half of what was built here.
      //
      // The first draft counted the EVENT and called it `stray_decision_o`.
      // It fired on a perfectly clean patch, because TERRAIN.LOD has patch
      // N+1's first decision ready before the compose cache serves patch N+1,
      // and because a producer that is valid in the very cycle the patch ARMS
      // is early by zero clocks. Both are ordinary backpressure. **A counter
      // that is non-zero in normal operation cannot be read as a fault**, and
      // narrowing the window until it stayed silent would have been tuning an
      // instrument to its own test rather than to the machine.
      //
      // There is no locally detectable fault here at all: this block cannot
      // know whether a patch will EVER be armed for a decision it is holding.
      // So the honest instrument is the duration, and the pair to read is
      // `decision_wait_clocks_o` climbing while `patches_retired_o` is flat --
      // the same tell as `issue_clocks_o`, from the other side of the join.
      if (lod_valid_i && (st_q == StIdle) &&
          (decision_wait_clocks_o != 32'hFFFF_FFFF)) begin
        decision_wait_clocks_o <= decision_wait_clocks_o + 32'd1;
      end

      // ---- the serve level's edge --------------------------------------
      if (!serve_valid_i) serve_seen_q <= 1'b0;

      if (st_q == StIssue) begin
        if (issue_clocks_o != 32'hFFFF_FFFF) issue_clocks_o <= issue_clocks_o + 32'd1;
      end

      case (st_q)
        StIdle: begin
          if (serve_valid_i && !serve_seen_q) begin
            serve_seen_q <= 1'b1;
            if (cq_empty_c) begin
              // A patch reached the serve side with no draw context behind it.
              // Nothing is issued and nothing is retired: the composition that
              // can produce a patch without a context is the fault, and
              // retiring here would hide it behind a moving `patches_served_o`.
              if (serve_no_ctx_o != 32'hFFFF_FFFF) serve_no_ctx_o <= serve_no_ctx_o + 32'd1;
            end else begin
              // The pop itself is `cq_pop_c`, above. Only the JUDGEMENT is here.
              if (cq_src[cq_rd_q] == serve_src_id_i) begin
                act_src_q    <= serve_src_id_i;
                act_mask_q   <= cq_mask[cq_rd_q];
                act_sparse_q <= cq_sparse[cq_rd_q];
                act_expect_q <= '0;
                act_count_q  <= '0;
                act_sized_q  <= 1'b0;
                job_taken_q  <= 1'b0;
                h_taken_q    <= 1'b0;
                st_q         <= StIssue;
              end else begin
                // THE DROP LAW. See the header.
                if (ctx_src_mismatch_o != 32'hFFFF_FFFF) begin
                  ctx_src_mismatch_o <= ctx_src_mismatch_o + 32'd1;
                end
                if (patches_dropped_o != 32'hFFFF_FFFF) begin
                  patches_dropped_o <= patches_dropped_o + 32'd1;
                end
                serve_release_o <= 1'b1;
              end
            end
          end
        end

        StIssue: begin
          // Size the patch from its first decision, then hold it.
          if (lod_valid_i && !act_sized_q) begin
            act_expect_q <= expect_c;
            act_sized_q  <= 1'b1;
          end

          // The fork's per-sink acknowledgements.
          if (job_fire_c && job_ready_i && !dec_take_c) job_taken_q <= 1'b1;
          if (h_fire_c   && h_ready_i   && !dec_take_c) h_taken_q   <= 1'b1;

          if (dec_take_c) begin
            job_taken_q <= 1'b0;
            h_taken_q   <= 1'b0;
            act_count_q <= act_count_q + EXPW'(1);
            if (jobs_issued_o != 32'hFFFF_FFFF) jobs_issued_o <= jobs_issued_o + 32'd1;

            // The decision's own src_id against the patch we armed for. Two
            // independently clocked operands: `act_src_q` came from the cache's
            // serve port at ARM, `lod_src_id_i` rides the decision out of
            // TERRAIN.LOD from the deviation store's record.
            if ((lod_src_id_i != act_src_q) && (lod_src_mismatch_o != 32'hFFFF_FFFF)) begin
              lod_src_mismatch_o <= lod_src_mismatch_o + 32'd1;
            end

            if (last_dec_c) st_q <= StDrain;
          end
        end

        default: begin  // StDrain
          // `job_ready_i` high is zhao_terrain_group_seq back in StIdle, which
          // by its own exit proof is the cycle every reference of the last job
          // has been accepted by the shell and its arenas released.
          if (job_ready_i) begin
            serve_release_o <= 1'b1;
            if (patches_retired_o != 32'hFFFF_FFFF) patches_retired_o <= patches_retired_o + 32'd1;
            st_q <= StIdle;
          end
        end
      endcase
    end
  end

endmodule : zhao_terrain_jobissue

`default_nettype wire
