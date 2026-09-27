// zhao_geom_tilewalk -- GEOM.TILEWALK
//
// THE SEQUENCER ON `walk_valid_i`. Console entry I55, step 3 of the three
// `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md` and FINDINGS-doorcost
// name. It sweeps the frame's tiles, asks `zhao_geom_arenabin`'s head table
// which chunk each tile's list starts at, drives `zhao_geom_paramwalk` over
// that chain, and hands every triangle the walk returns to the time-multiplexed
// GEOM.SETUP / GEOM.ATTRPACK pair and then to the raster door.
//
// WHY IT IS NOT "LOGIC ADDED TO MAKE A COUNTER MOVE", which is the exact thing
// six packets refused. Those refusals were right, and they said so in the same
// words every time: with every `t_*` output dangling, a sequencer here would
// count triangles and DROP them. What changed is that the triangles now have a
// consumer -- `zhao_geom_bin_pipe_v2`'s `JOB_SRC = 1` door, which DOORCOST
// opened and priced at +983 comb ALUT / +0 DSP -- and TriangleDescriptor
// SCHEMA v2 gave the record the two fields (`area2` and the scissored box)
// GEOM.SETUP consumes and v1 could not carry. This block is the last link, and
// it drives a real back end that draws real pixels.
//
// ---------------------------------------------------------------------------
// ONE TRIANGLE IN FLIGHT AT A TIME, AND THAT IS A DECLARED CHOICE
// ---------------------------------------------------------------------------
//
// A triangle is taken from the walk, handed to the back end, and its job is
// offered to the raster door before the next triangle is accepted. No queue,
// no reordering, nothing to get out of step.
//
// THE COST IS REAL AND IT IS STATED RATHER THAN HIDDEN: the walk's next
// descriptor fetch does not overlap the back end's work on the current
// triangle, so a reference costs the walk's time PLUS the back end's instead of
// the larger of the two. Entry I55's own measurement is that the walk is
// 29.58 clocks per triangle against GEOM.ATTRPACK's 13, so the serial form is
// roughly a third slower than a pipelined one -- against a consumer side
// already measured at 7.18x the on-chip drain. That is a throughput number to
// improve later with a sideband queue, and it is not a correctness question.
//
// THE REASON TO TAKE IT ANYWAY: the alternative is a queue carrying
// (tile, first, last) alongside a triangle through a multi-stage back end, and
// a sideband that can slip by one entry relative to the record it describes is
// precisely this repository's metadata-swap defect -- "response A's data, A's
// token, and B's metadata" -- which shipped once with a live identity counter
// reading zero beside it. A serial handshake cannot slip, and it cannot slip
// without a counter to watch it.
//
// ---------------------------------------------------------------------------
// THE TILE INDEX AGREES WITH GEOM.ARENABIN BY CONSTRUCTION, NOT BY ARGUMENT
// ---------------------------------------------------------------------------
//
// `zhao_geom_arenabin` addresses its head table as `tidx(ty) + tx`, where
// `tidx(ty)` is `ty * GRID_W` -- the compile-time PARAMETER, not the runtime
// `grid_w_i`. So a sequencer that derived the index from `grid_w_i` would
// disagree with the producer on any frame whose grid is smaller than the
// parameter, and would walk a tile's list under another tile's coordinates:
// every guard downstream would still pass and the picture would be wrong.
//
// This block therefore sweeps the WHOLE index space, 0 .. TILES-1, with the
// same row stride, which is what that block's own A_CLEAR and A_FLUSH sweeps
// do (`sweep_r == TIDX_W'(TILES - 1)`). Empty tiles cost one query each and are
// skipped on `head_valid_i`; they are counted, because "most tiles were empty"
// and "the head table never answered" look identical from the outside.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT DO
// ---------------------------------------------------------------------------
//
// It does not compute `first` or `last`. `zhao_geom_paramwalk` publishes them,
// because only the walk knows whether a chain continues -- deriving them here
// would need a skid buffer holding a triangle while asking the walk for the
// next one, and the walk cannot answer without destroying the triangle being
// held. See that block's `t_first_o` / `t_last_o` comment.
//
// It does not touch GEOM.VERTID. The walk runs after the arena has SEALED and
// PUBLISHED, so the descriptor writer's work for the frame is finished; feeding
// it walk-sourced triangles would publish a second copy of every vertex into
// the next frame's build view. `active_o` is the select the composer uses, and
// the three-way fork it gates is left exactly as RASTERSWAP repaired it for the
// geometry phase, which is the phase that fork governs.

`default_nettype none

module zhao_geom_tilewalk #(
    // The tile index space, and BOTH must match `zhao_geom_arenabin`'s.
    parameter int unsigned TILES  = 576,
    parameter int unsigned GRID_W = 24,
    parameter int unsigned TIDX_W = 10
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the frame ---------------------------------------------------------
    // A LEVEL: the arena has published a frame. The rising edge starts a
    // sweep, detected here rather than in the composer so that adding this
    // block costs the composer no register of its own -- a stray flip-flop in
    // a 31,000-line composition file is a thing nobody finds again.
    //
    // The walk cannot start earlier than publication in any case:
    // `zhao_geom_paramwalk.walk_ready_o` is `(wstate_q == W_IDLE) &&
    // pub_valid_i`, so an offer made before it would simply never be taken.
    input  var logic        start_i,

    // ---- the head table, one clock of registered latency --------------------
    output var logic [TIDX_W-1:0] head_tile_o,
    input  var logic [31:0]       head_chunk_i,
    input  var logic              head_valid_i,

    // ---- the walk request ---------------------------------------------------
    output var logic        walk_valid_o,
    input  var logic        walk_ready_i,
    output var logic [31:0] walk_head_o,
    input  var logic        walk_done_i,
    input  var logic        walk_failed_i,

    // ---- the walk's triangle stream -----------------------------------------
    input  var logic        t_valid_i,
    output var logic        t_ready_o,
    input  var logic        t_first_i,
    input  var logic        t_last_i,

    // ---- the time-multiplexed geometry back end -----------------------------
    // `be_ready_i` is GEOM.SETUP's and GEOM.ATTRPACK's readys ANDed; the two
    // must take a triangle on one clock or neither does, exactly as the
    // geometry-phase fork requires of the same pair.
    output var logic        be_valid_o,
    input  var logic        be_ready_i,
    // Both back ends have produced their result for the triangle in flight.
    input  var logic        be_out_valid_i,
    output var logic        be_out_ready_o,

    // ---- the raster door, `zhao_geom_bin_pipe_v2`'s `walk_job_*` ------------
    output var logic        job_valid_o,
    input  var logic        job_ready_i,
    output var logic signed [11:0] job_tile_x_o,
    output var logic signed [11:0] job_tile_y_o,
    output var logic        job_first_o,
    output var logic        job_last_o,

    // ---- the select --------------------------------------------------------
    // High for the whole sweep. The composer uses it to feed GEOM.SETUP and
    // GEOM.ATTRPACK from the walk instead of from GEOM.CLIP.
    output var logic        active_o,
    output var logic        frame_done_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] tiles_walked_o,
    output var logic [31:0] tiles_empty_o,
    output var logic [31:0] jobs_issued_o,
    output var logic [31:0] walks_failed_o,
    output var logic [31:0] be_stall_clocks_o,
    // A triangle arrived from the walk while one was still in flight. It is
    // UNREACHABLE while the serial handshake below is correct -- `t_ready_o` is
    // low in every state but T_TAKE -- so it owes a committed mutant rather
    // than an argument, and `tests/mutants/zhao_geom_tilewalk_overlap_mutant.sv`
    // is it.
    output var logic [31:0] overlap_o
);

  // synthesis translate_off
  initial begin
    if (TIDX_W < $clog2(TILES))
      $fatal(1, "zhao_geom_tilewalk: TIDX_W too narrow for TILES");
    if (GRID_W == 0)
      $fatal(1, "zhao_geom_tilewalk: GRID_W must be non-zero");
    if (TILES % GRID_W != 0)
      $fatal(1, "zhao_geom_tilewalk: TILES is not a whole number of GRID_W rows -- the row stride and the sweep would disagree");
  end
  // synthesis translate_on

  typedef enum logic [2:0] {
    S_IDLE,
    S_QUERY,   // drive head_tile_o
    S_QWAIT,   // the head RAM's registered read lands
    S_START,   // offer the walk
    S_RUN,     // triangles
    S_NEXT,    // advance the cursor
    S_DONE
  } state_e;

  typedef enum logic [1:0] {
    T_TAKE,    // offer the walk's triangle to the back end
    T_WAIT,    // the back end is working
    T_JOB      // the result is up; offer the job
  } tstate_e;

  state_e  state_q;
  tstate_e tstate_q;

  logic [TIDX_W-1:0] tile_q;
  logic [11:0]       tx_q, ty_q;
  logic [31:0]       head_q;
  // `walk_done_i` pulses after the LAST emit was accepted, so it can land while
  // a triangle is still in flight through the back end. Latched rather than
  // acted on, or the sweep would advance to the next tile with a job still owed
  // -- and that job carries the `last` that RESOLVES the tile.
  logic              done_pend_q;
  // Publication is a LEVEL that stays high for the whole frame, so a sweep is
  // started by its EDGE. Without this the sweep would restart every clock the
  // frame stayed published, which is the shape that turns one walk into
  // thousands and reads downstream as a wedge rather than as a loop.
  logic              start_q;

  logic signed [11:0] job_tx_q, job_ty_q;
  logic               job_first_q, job_last_q;

  assign head_tile_o  = tile_q;
  assign walk_head_o  = head_q;
  assign walk_valid_o = (state_q == S_START);
  assign active_o     = (state_q != S_IDLE);

  // THE SERIAL HANDSHAKE. `t_ready_o` is high in exactly one state, which is
  // what makes `overlap_o` unreachable and what makes the (tile, first, last)
  // sideband incapable of slipping relative to the triangle it describes.
  assign t_ready_o      = (state_q == S_RUN) && (tstate_q == T_TAKE) && be_ready_i;
  assign be_valid_o     = (state_q == S_RUN) && (tstate_q == T_TAKE) && t_valid_i;
  assign job_valid_o    = (state_q == S_RUN) && (tstate_q == T_JOB);
  assign be_out_ready_o = job_valid_o && job_ready_i;

  assign job_tile_x_o = job_tx_q;
  assign job_tile_y_o = job_ty_q;
  assign job_first_o  = job_first_q;
  assign job_last_o   = job_last_q;

  wire take_c = be_valid_o && be_ready_i;
  wire job_c  = job_valid_o && job_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q     <= S_IDLE;
      tstate_q    <= T_TAKE;
      tile_q      <= '0;
      tx_q        <= 12'd0;
      ty_q        <= 12'd0;
      head_q      <= 32'd0;
      done_pend_q <= 1'b0;
      start_q     <= 1'b0;
      job_tx_q    <= 12'sd0;
      job_ty_q    <= 12'sd0;
      job_first_q <= 1'b0;
      job_last_q  <= 1'b0;
      frame_done_o      <= 1'b0;
      tiles_walked_o    <= '0;
      tiles_empty_o     <= '0;
      jobs_issued_o     <= '0;
      walks_failed_o    <= '0;
      be_stall_clocks_o <= '0;
      overlap_o         <= '0;
    end else begin
      frame_done_o <= 1'b0;
      start_q      <= start_i;

      // THE OVERLAP TRIPWIRE. A triangle offered by the walk while one is
      // still in flight would mean `t_ready_o` had been high in two states, or
      // that the walk ignored the handshake. Counted on the OFFER and not on a
      // take, because a take cannot happen in those states at all -- watching
      // the thing that is structurally impossible is the point of a tripwire.
      if ((state_q == S_RUN) && (tstate_q != T_TAKE) && t_valid_i && t_ready_o)
        overlap_o <= overlap_o + 32'd1;

      // The back end holding the line while a triangle waits to be handed over
      // or to come out. Time, not faults -- the throughput cost of the serial
      // handshake above, measured rather than estimated.
      if ((state_q == S_RUN) &&
          (((tstate_q == T_TAKE) && t_valid_i && !be_ready_i) ||
           ((tstate_q == T_WAIT) && !be_out_valid_i)))
        be_stall_clocks_o <= be_stall_clocks_o + 32'd1;

      case (state_q)
        S_IDLE: if (start_i && !start_q) begin
          tile_q      <= '0;
          tx_q        <= 12'd0;
          ty_q        <= 12'd0;
          done_pend_q <= 1'b0;
          tstate_q    <= T_TAKE;
          state_q     <= S_QUERY;
        end

        // `head_tile_o` is combinational off `tile_q`, which is already stable
        // here; this state exists so the query is presented for a whole clock
        // before S_QWAIT reads the registered answer.
        S_QUERY: state_q <= S_QWAIT;

        S_QWAIT: begin
          if (head_valid_i) begin
            head_q  <= head_chunk_i;
            state_q <= S_START;
          end else begin
            tiles_empty_o <= tiles_empty_o + 32'd1;
            state_q       <= S_NEXT;
          end
        end

        S_START: if (walk_ready_i) begin
          tiles_walked_o <= tiles_walked_o + 32'd1;
          done_pend_q    <= 1'b0;
          tstate_q       <= T_TAKE;
          state_q        <= S_RUN;
        end

        S_RUN: begin
          // The walk's end-of-chain pulse, held until the last job is owed no
          // longer. `walk_failed_i` is sampled with it, on the same pulse.
          if (walk_done_i) begin
            done_pend_q <= 1'b1;
            if (walk_failed_i) walks_failed_o <= walks_failed_o + 32'd1;
          end

          case (tstate_q)
            T_TAKE: begin
              if (take_c) begin
                // THE SIDEBAND IS LATCHED ON THE SAME EDGE THE BACK END TAKES
                // THE RECORD. One enable, one record, one set of brackets --
                // so the pair cannot be corrupted in lockstep by a stall,
                // which is the failure this repository names by name.
                // ---- THE UNITS ARE PIXELS, NOT TILE INDICES --------
                // `zhao_geom_binner_v2` supplies this port as
                // `$signed({2'd0, d_jx_r, 4'd0})` -- the tile index shifted
                // LEFT by four -- and its header states it in words:
                // "`job_tile_x_o` is the tile's top-left PIXEL".
                // `zhao_geom_bin_pipe_v2` agrees from the other end, deriving
                // the raster's tile index as `job_tile_x_w[9:4]`, and so does
                // GEOM.ARENABIN's `tile_of`, which is `p >>> 4`.
                //
                // EMITTING THE INDEX HERE WOULD HAVE BEEN SILENT. Every tile
                // would have landed inside the top-left 24x24 pixels, every
                // tile below sixteen would have carried tile index 0, and the
                // raster would have resolved one tile repeatedly -- with
                // `jobs_issued_o`, `walk_jobs_taken_o`, the `vread == 3*tris`
                // invariant and every range guard downstream still balancing.
                // The tile grid is non-negative by construction, so the cast
                // is a re-interpretation; the port is signed because a scan
                // box can be.
                job_tx_q    <= $signed(12'(tx_q << 4));
                job_ty_q    <= $signed(12'(ty_q << 4));
                job_first_q <= t_first_i;
                job_last_q  <= t_last_i;
                tstate_q    <= T_WAIT;
              end else if (done_pend_q && !t_valid_i) begin
                // The chain ended and nothing is in flight.
                state_q <= S_NEXT;
              end
            end

            T_WAIT: if (be_out_valid_i) tstate_q <= T_JOB;

            T_JOB: if (job_c) begin
              jobs_issued_o <= jobs_issued_o + 32'd1;
              tstate_q      <= T_TAKE;
            end

            default: tstate_q <= T_TAKE;
          endcase
        end

        S_NEXT: begin
          if (tile_q == TIDX_W'(TILES - 1)) begin
            state_q <= S_DONE;
          end else begin
            tile_q <= tile_q + TIDX_W'(1);
            // The row stride is the PARAMETER, exactly as GEOM.ARENABIN's
            // `tidx(ty) = ty * GRID_W`. Kept as two counters so the index and
            // the coordinates cannot be derived from each other wrongly.
            if (tx_q == 12'(GRID_W - 1)) begin
              tx_q <= 12'd0;
              ty_q <= ty_q + 12'd1;
            end else begin
              tx_q <= tx_q + 12'd1;
            end
            state_q <= S_QUERY;
          end
        end

        S_DONE: begin
          frame_done_o <= 1'b1;
          state_q      <= S_IDLE;
        end

        default: state_q <= S_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
