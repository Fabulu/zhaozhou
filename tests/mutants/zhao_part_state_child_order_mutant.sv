// zhao_part_state_child_order_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// ---------------------------------------------------------------------------
// WHAT THIS EXISTS TO PROVE
// ---------------------------------------------------------------------------
// `zhao_part_state.sv` claims its ordering law -- survivors compacted first in
// stream order, children appended strictly after -- is STRUCTURAL:
//
//     "It is enforced structurally here -- the write stream cannot emit a child
//      until the survivor pass has ended -- rather than by a priority
//      comparison that could be got wrong."
//
// and its `ZHAO_ASSERT` block says out loud that the ordering deliberately has
// NO assertion, because the first draft's assertion ended in `&& 1'b0`.
//
// So the ONLY thing standing between that law and silence is
// `tests/particles/part_state_directed.cpp` comparing the emitted stream
// against an expected stream. That test passed on its first run, which under
// this tree's rules is a claim and not evidence: a test that would pass on a
// block with the ordering broken is not testing the ordering.
//
// This copy breaks the ordering and nothing else, and
// `tests/particles/part_state_child_order_control.cpp` runs it with INVERTED
// POLARITY -- it passes when the ordering is seen to be WRONG. It is evidence
// about the instrument, not about the design.
//
// ---------------------------------------------------------------------------
// THE MUTATION, EXACTLY
// ---------------------------------------------------------------------------
// ONE added block inside `S_SURVIVE`, marked `MUTATION` below and appearing
// nowhere in production:
//
//     if (!chl_empty_c && wr_room_c && !(vrd_valid_i && vrd_ready_o)) begin
//       wr_rec_q  <= chl_m[chl_rp_q[CHILD_PW-1:0]];
//       wr_v_q    <= 1'b1;
//       written_q <= written_q + CNT_W'(1);
//       children_written_o <= children_written_o + 32'd1;
//       chl_rp_q  <= chl_rp_q + (CHILD_PW+1)'(1);
//     end
//
// It is the production `S_APPEND` drain body, lifted verbatim into `S_SURVIVE`
// and guarded so it never collides with the verdict's own write. That is
// precisely "children become appendable BEFORE the survivors are drained" --
// the one sentence the structural argument rests on.
//
// WHY THIS PARTICULAR BREAK IS THE DISCRIMINATING ONE. It leaves the record
// COUNT unchanged. The same four survivors and the same three children are
// written; only their ORDER changes, because the children are spent during the
// survivor pass instead of after it. So a test that checked only
// `written.size()`, `survivors_o` and `children_written_o` -- three numbers
// that all still balance -- would pass on this mutant. Only the per-position
// record comparison catches it, and that is the check this control is here to
// prove does its job.
//
// It also leaves every counter in the block balanced, which is the ledger's
// "two operands that move together" shape one level out: the accounting is
// self-consistent and the stream is wrong.
//
// ---------------------------------------------------------------------------
// WHY A FILE AND NOT AN EDIT
// ---------------------------------------------------------------------------
// A temporary edit to production RTL is a live-tree hazard while a fit reads
// the working tree, and it leaves nothing behind: the next person inherits the
// same argument and no evidence. The module is RENAMED so no source list can
// elaborate it in place of the real one, and it lives under `tests/` where a
// production closure check will not find it.
//
// REGENERATE IT if `fpga/rtl/particles/zhao_part_state.sv` changes shape: this
// is a COPY, and a copy of an old version is a positive control for a block
// that no longer exists. `tools/budget/mutant_copy_drift.py` watches for that.
// Cut from zhao_part_state.sv at tree revision e1c03cc1, 2026-09-18.
//
// Everything below this line is zhao_part_state.sv verbatim except for the
// module name, the `endmodule` label, and the block marked MUTATION.
// ---------------------------------------------------------------------------

// zhao_part_state.sv — PART.STATE: the particle state stream.
//
// Law, in citation order:
//   design/contracts/PART.STATE.md — the block contract, owner ZH-042, phase 10.
//   Owner ruling 2026-08-31 §2.1 — the 128-bit record layout, FROZEN.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND WHAT IT IS NOT
// ---------------------------------------------------------------------------
// It is a STREAMER. Per tick it reads the previous generation from one HPS DDR
// buffer, offers each record to PART.UPDATE, writes the survivors back to the
// other buffer densely and in stream order, and then appends children.
//
// It does NOT interpret the record. The contract is explicit that position and
// velocity scales are an unruled Class-C decision, so this block "carries the
// bits without interpreting them" and the arithmetic lives in PART.UPDATE. The
// one thing it does assert is that the packing is exact: a record read and
// written back untouched is bit-identical, which is testable today and does not
// wait on the scale ruling.
//
// It owns no on-chip memory. The two 512 KiB buffers are in HPS DDR at the
// required tier of 32,768 particles, and the DDR crossing belongs to
// MEM.HPS.BRIDGE.
//
// ---------------------------------------------------------------------------
// THE ORDERING IS THE CONTRACT, NOT A CHOICE
// ---------------------------------------------------------------------------
//   1. survivors are compacted first, in stream order;
//   2. children are appended after survivors;
//   3. on exhaustion, survivors always outrank new children.
//
// That order is what makes the stream dense: interleaving children would need a
// second pass or a gap list. It is enforced structurally here — the write
// stream cannot emit a child until the survivor pass has ended — rather than by
// a priority comparison that could be got wrong.
//
// ---------------------------------------------------------------------------
// WHY CHILDREN ARE STAGED, AND WHY THE DROP IS DETERMINISTIC
// ---------------------------------------------------------------------------
// PART.SPAWN produces children WHILE the survivor pass is running, but they
// must be written AFTER it. So they are staged in a bounded FIFO.
//
// Both drop paths are by ARRIVAL ORDER and never by timing:
//
//   * staging full  -> the child is refused and counted. It is refused at the
//                      handshake, so the producer sees it; nothing is silently
//                      swallowed.
//   * tick capacity -> later children dropped, survivors retained.
//
// The contract's reason is worth keeping in front of the reader: dropping under
// backpressure in a timing-dependent way "would make the result depend on
// memory timing, which is the same determinism failure as an evolving seed".
// Every drop here is a function of the input sequence alone.
//
// ---------------------------------------------------------------------------
// RESET
// ---------------------------------------------------------------------------
// "Reset abandons the stream in flight. The ping-pong buffers are NOT cleared —
// they hold the last committed generation, and clearing them would destroy the
// simulation rather than restart it. What resets is the pointer state, the
// compaction cursor and the in-flight tags."
//
// Here that is literal: the buffers are off-chip and this block cannot clear
// them even by accident. What resets is exactly the list the contract names.
//
// Conservative SystemVerilog subset only (charter §2).
`default_nettype none

module zhao_part_state_child_order_mutant #(
    // The required tier. The stretch tier of 65,536 is gated by the ruling on
    // board bandwidth, not on this block, so it is a parameter and not a
    // decision taken here.
    parameter int unsigned CAPACITY   = 32768,
    // Bounded child staging. Deep enough that a normal tick never refuses,
    // small enough that the block stays a streamer.
    parameter int unsigned CHILD_D    = 64,
    parameter int unsigned SPECIES_N  = 128,   // 7 bits of species, per §2.1

    parameter int unsigned REC_W      = 128,
    parameter int unsigned CNT_W      = $clog2(CAPACITY) + 1,
    parameter int unsigned CHILD_PW   = (CHILD_D <= 1) ? 1 : $clog2(CHILD_D)
) (
    input var logic clk,
    input var logic rst_n,

    // ---- tick control --------------------------------------------------------
    input  wire                  tick_start_i,
    output wire                  tick_busy_o,
    output wire                  tick_done_o,     // one-cycle pulse

    // ---- the previous generation, from MEM.HPS.BRIDGE ------------------------
    input  wire                  rd_valid_i,
    output wire                  rd_ready_o,
    input  wire [REC_W-1:0]      rd_record_i,
    input  wire                  rd_last_i,       // final record of the generation

    // ---- offered to PART.UPDATE ----------------------------------------------
    output wire                  prt_valid_o,
    input  wire                  prt_ready_i,
    output wire [REC_W-1:0]      prt_record_o,

    // ---- the verdict, from PART.UPDATE ---------------------------------------
    // `vrd_record_i` is what gets written: UPDATE may have advanced the record.
    // A survivor written back UNCHANGED must be bit-identical, which is the
    // packing assertion this block owes.
    input  wire                  vrd_valid_i,
    output wire                  vrd_ready_o,
    input  wire                  vrd_survive_i,
    input  wire [REC_W-1:0]      vrd_record_i,

    // ---- children, from PART.SPAWN -------------------------------------------
    input  wire                  chl_valid_i,
    output wire                  chl_ready_o,
    input  wire [REC_W-1:0]      chl_record_i,

    // ---- the next generation, to MEM.HPS.BRIDGE ------------------------------
    output wire                  wr_valid_o,
    input  wire                  wr_ready_i,
    output wire [REC_W-1:0]      wr_record_o,

    // ---- counters. Faults leave the module; see the ledger's reading of a
    // ---- counter that is asserted zero and never seen to move.
    output var logic [31:0]      survivors_o,
    output var logic [31:0]      children_written_o,
    output var logic [31:0]      children_dropped_capacity_o,
    output var logic [31:0]      children_refused_staging_o,
    output var logic [31:0]      species_refused_o
);

  // THE RECORD LAYOUT, FROZEN BY OWNER RULING 2026-08-31 §2.1.
  //
  // Stated as offsets rather than a struct on purpose: a packed struct would
  // make the field order a compiler's business, and this layout is capture
  // visible. 54 + 33 + 10 + 7 + 6 + 6 + 4 + 8 = 128.
  // Most of these are reported unused, and they are -- deliberately. The
  // block "carries the bits without interpreting them", so only `species` is
  // read and only the total is checked. The offsets stay because the frozen
  // layout belongs in the RTL that carries it, and because the elaboration
  // check below is arithmetic over them rather than a comment about them.
  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned OFF_POS  = 0;    localparam int unsigned W_POS  = 54;
  localparam int unsigned OFF_VEL  = 54;   localparam int unsigned W_VEL  = 33;
  localparam int unsigned OFF_AGE  = 87;   localparam int unsigned W_AGE  = 10;
  localparam int unsigned OFF_SPC  = 97;   localparam int unsigned W_SPC  = 7;
  localparam int unsigned OFF_SIZ  = 104;  localparam int unsigned W_SIZ  = 6;
  localparam int unsigned OFF_SPN  = 110;  localparam int unsigned W_SPN  = 6;
  localparam int unsigned OFF_FLG  = 116;  localparam int unsigned W_FLG  = 4;
  localparam int unsigned OFF_VAR  = 120;  localparam int unsigned W_VAR  = 8;
  /* verilator lint_on UNUSEDPARAM */

  // The layout must ADD UP, and a comment is not a check. If a future field
  // edit breaks the packing the elaboration fails instead of the capture.
  initial begin
    if (OFF_VAR + W_VAR != REC_W)
      $fatal(1, "zhao_part_state: record layout sums to %0d bits, not %0d",
             OFF_VAR + W_VAR, REC_W);
    if (SPECIES_N > (1 << W_SPC))
      $fatal(1, "zhao_part_state: SPECIES_N=%0d exceeds the %0d the frozen 7-bit species field can address",
             SPECIES_N, (1 << W_SPC));
    if (CHILD_D < 2)
      $fatal(1, "zhao_part_state: CHILD_D=%0d; staging needs at least 2 entries", CHILD_D);
  end

  // ---- tick phases ----------------------------------------------------------
  // SURVIVORS cannot emit a child and APPEND cannot emit a survivor. The
  // contract's ordering is therefore a property of the state machine rather
  // than of a comparison someone could invert.
  localparam logic [1:0] S_IDLE    = 2'd0;
  localparam logic [1:0] S_SURVIVE = 2'd1;
  localparam logic [1:0] S_APPEND  = 2'd2;
  localparam logic [1:0] S_DONE    = 2'd3;

  logic [1:0]        st_q;
  logic              rd_done_q;      // the generation's last record was consumed
  logic [CNT_W-1:0]  written_q;      // survivors + children written this tick

  // ---- child staging --------------------------------------------------------
  logic [REC_W-1:0]  chl_m [CHILD_D];
  logic [CHILD_PW:0] chl_wp_q, chl_rp_q;
  wire  [CHILD_PW:0] chl_occ_c  = chl_wp_q - chl_rp_q;
  wire               chl_full_c = (chl_occ_c >= (CHILD_PW+1)'(CHILD_D));
  wire               chl_empty_c = (chl_occ_c == '0);

  // ---- the offered record ---------------------------------------------------
  logic [REC_W-1:0]  prt_rec_q;
  logic              prt_v_q;

  // ---- the write channel ----------------------------------------------------
  logic [REC_W-1:0]  wr_rec_q;
  logic              wr_v_q;

  wire wr_fire_c  = wr_v_q && wr_ready_i;
  wire wr_room_c  = !wr_v_q || wr_ready_i;
  wire room_left_c = (written_q < CNT_W'(CAPACITY));

  // A record whose species index is out of range is REFUSED, not fabricated.
  wire [W_SPC-1:0] vrd_species_c = vrd_record_i[OFF_SPC +: W_SPC];
  wire species_bad_c = (SPECIES_N < (1 << W_SPC)) &&
                       ({{(32-W_SPC){1'b0}}, vrd_species_c} >= 32'(SPECIES_N));

  // ---- handshakes -----------------------------------------------------------
  assign rd_ready_o  = (st_q == S_SURVIVE) && !prt_v_q;
  assign prt_valid_o = prt_v_q;
  assign prt_record_o = prt_rec_q;

  // A verdict is taken only when the write channel can accept what it implies.
  assign vrd_ready_o = (st_q == S_SURVIVE) && wr_room_c;

  // Children are accepted whenever staging has room, in ANY phase: SPAWN
  // produces them during the survivor pass and they must not be lost to a
  // phase boundary. Refusal is at the handshake so the producer sees it.
  assign chl_ready_o = !chl_full_c && (st_q != S_IDLE);

  assign wr_valid_o  = wr_v_q;
  assign wr_record_o = wr_rec_q;

  assign tick_busy_o = (st_q != S_IDLE);

  logic tick_done_q;
  assign tick_done_o = tick_done_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // Exactly the contract's list: pointer state, the compaction cursor, the
      // in-flight tags. There is no buffer here to clear even by mistake.
      st_q        <= S_IDLE;
      rd_done_q   <= 1'b0;
      written_q   <= '0;
      chl_wp_q    <= '0;
      chl_rp_q    <= '0;
      prt_v_q     <= 1'b0;
      prt_rec_q   <= '0;
      wr_v_q      <= 1'b0;
      wr_rec_q    <= '0;
      tick_done_q <= 1'b0;
      survivors_o                 <= 32'd0;
      children_written_o          <= 32'd0;
      children_dropped_capacity_o <= 32'd0;
      children_refused_staging_o  <= 32'd0;
      species_refused_o           <= 32'd0;
    end else begin
      tick_done_q <= 1'b0;

      // A child refused for staging space is counted where it is refused.
      if (chl_valid_i && chl_full_c && (st_q != S_IDLE))
        children_refused_staging_o <= children_refused_staging_o + 32'd1;

      if (chl_valid_i && chl_ready_o) begin
        chl_m[chl_wp_q[CHILD_PW-1:0]] <= chl_record_i;
        chl_wp_q <= chl_wp_q + (CHILD_PW+1)'(1);
      end

      if (wr_fire_c) wr_v_q <= 1'b0;

      case (st_q)
        S_IDLE: begin
          if (tick_start_i) begin
            st_q      <= S_SURVIVE;
            rd_done_q <= 1'b0;
            written_q <= '0;
            chl_wp_q  <= '0;
            chl_rp_q  <= '0;
          end
        end

        S_SURVIVE: begin
          // offer the next record
          if (rd_valid_i && rd_ready_o) begin
            prt_rec_q <= rd_record_i;
            prt_v_q   <= 1'b1;
            if (rd_last_i) rd_done_q <= 1'b1;
          end
          if (prt_v_q && prt_ready_i) prt_v_q <= 1'b0;

          // ------------------------------------------------------------------
          // MUTATION -- THE ONLY SUBSTANTIVE DIFFERENCE FROM PRODUCTION.
          //
          // The S_APPEND drain body, lifted verbatim into S_SURVIVE. Production
          // has NO path from the child FIFO to the write channel in this state,
          // and that absence IS the ordering law. Here a child is written
          // whenever the FIFO is non-empty and the verdict is not itself using
          // the write channel this cycle, so children interleave with the
          // survivors instead of following them.
          //
          // Counts are untouched: the same records come out, in the wrong
          // order. That is what makes it the discriminating mutation.
          // ------------------------------------------------------------------
          if (!chl_empty_c && wr_room_c && !(vrd_valid_i && vrd_ready_o)) begin
            wr_rec_q  <= chl_m[chl_rp_q[CHILD_PW-1:0]];
            wr_v_q    <= 1'b1;
            written_q <= written_q + CNT_W'(1);
            children_written_o <= children_written_o + 32'd1;
            chl_rp_q  <= chl_rp_q + (CHILD_PW+1)'(1);
          end

          // take the verdict and, if it survives, write it back densely
          if (vrd_valid_i && vrd_ready_o) begin
            if (species_bad_c) begin
              species_refused_o <= species_refused_o + 32'd1;
            end else if (vrd_survive_i && room_left_c) begin
              wr_rec_q    <= vrd_record_i;
              wr_v_q      <= 1'b1;
              written_q   <= written_q + CNT_W'(1);
              survivors_o <= survivors_o + 32'd1;
            end
            // a survivor with no capacity left cannot happen: the read
            // generation is at most CAPACITY records, so survivors alone can
            // never exhaust it. The guard is kept because "cannot happen"
            // should still not corrupt the stream if a future tier changes.

            if (rd_done_q && !(prt_v_q && !prt_ready_i)) st_q <= S_APPEND;
          end
        end

        S_APPEND: begin
          if (!chl_empty_c && wr_room_c) begin
            if (room_left_c) begin
              wr_rec_q  <= chl_m[chl_rp_q[CHILD_PW-1:0]];
              wr_v_q    <= 1'b1;
              written_q <= written_q + CNT_W'(1);
              children_written_o <= children_written_o + 32'd1;
            end else begin
              // Capacity exhausted: survivors already have their places, so the
              // later children go, in stream order, exactly as the ruling says.
              children_dropped_capacity_o <= children_dropped_capacity_o + 32'd1;
            end
            chl_rp_q <= chl_rp_q + (CHILD_PW+1)'(1);
          end
          if (chl_empty_c && !wr_v_q) st_q <= S_DONE;
        end

        S_DONE: begin
          tick_done_q <= 1'b1;
          st_q        <= S_IDLE;
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

`ifdef ZHAO_ASSERT
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q) begin
      // THE ORDERING IS STRUCTURAL and deliberately has NO assertion.
      //
      // The first draft had one, and it ended in `&& 1'b0` -- a condition that
      // can never be true, which is a check that can never fire dressed as a
      // guarantee. This file would rather say the property is structural than
      // carry a green assertion that proves nothing: S_SURVIVE contains no path
      // from the child FIFO to the write channel, and S_APPEND contains no path
      // from the verdict, so "survivors before children" is a fact about the
      // state machine and not about a comparison.
      //
      // What CAN be checked is below, and each of these can fail.

      // Never more than the tier, whatever the input does.
      a_capacity_respected: assert (written_q <= CNT_W'(CAPACITY));

      // The block must not invent a write out of nothing.
      a_write_implies_tick: assert (!wr_v_q || (st_q != S_IDLE));
    end
  end
`endif

endmodule : zhao_part_state_child_order_mutant

`default_nettype wire
