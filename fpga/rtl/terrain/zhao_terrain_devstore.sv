// zhao_terrain_devstore.sv -- TERRAIN.LOD's PER-PAGE DEVIATION AND HISTORY
// STORE.  Owner rulings R24 and R59.
//
// ===========================================================================
// WHAT IT IS FOR
// ===========================================================================
// `zhao_terrain_lod` needs three things per subpatch that it deliberately does
// not keep: the three stored coarse-level deviations (`sp_dev1/2/3_i`) and the
// previous frame's history (`sp_prev_level_i`, `sp_prev_morph_i`, `sp_hold_i`).
// Its own law 5 says so in as many words -- "THE HISTORY RIDES THE PACKET ...
// for the caller to store.  REJECTED: an internal history RAM" -- and the
// entry I21 in `zhao_console_core.sv` records the consequence: the only driver
// of `sp_dev1_i` in this tree was an LFSR in `zhao_prod_top`.
//
// `zhao_terrain_loddev` (owner ruling R8) is the producer of the deviations.
// This block is the store between them, and it is a store and not a wire for
// the reason ruling R24 gives: the deviations are computed AT PAGE LOAD and
// consumed PER FRAME, so something has to hold them across the gap.
//
// ===========================================================================
// RULING R24 -- WHEN, AND WHERE IT LIVES
// ===========================================================================
//   "At PAGE LOAD, stored alongside the page in M10K (the owner prefers M10K
//    over ALMs; deviations change only when the page changes or is baked).
//    BAKE re-triggers the recompute for the pages it dirties."
//
// So the key is the RESIDENCY PAGE SLOT.  That is the correct key and the
// alternatives are not merely smaller, they are wrong:
//
//   * keyed by COMPOSE slot -- a compose slot is reused by a different patch
//     from one frame to the next, so the HISTORY (which is what hysteresis is
//     made of) would be inherited by a stranger.  Ruling R24's own words tie
//     the lifetime to the page.
//   * recomputed per frame -- measured, not argued: `zhao_terrain_loddev`
//     spends about 13,700 clocks on one patch's one surface (16 subpatches x
//     {56, 72, 77} measured vertices x 4 clocks, plus the skipped slots at
//     one).  `spec/terrain_rules.md` 4.2 puts 256 live/visible patches in a
//     frame and the frame is 1.67M gpu clocks, so recomputing the visible set
//     is ~3.5M clocks -- twice the frame, before anything is drawn.
//
// ===========================================================================
// RULING R59 -- BOTH SIZES, AS ASKED
// ===========================================================================
// R59: "the packet must first show the EXACT same law in a smaller form ...
// and take the smaller one if it is bit-identical.  Report both sizes."
//
// THE NAIVE FORM, which is what a first pass writes: both surfaces, because
// `zhao_terrain_loddev` has a `start_surface_i` and the mip pass streams the
// page twice, once per surface, so two sets of records fall out.
//
//     1,024 slots x 2 surfaces x 16 subpatches x 3 levels x 24 bits
//       = 2,359,296 bits = 231 M10K of 553  (42%)
//   + history 1,024 x 16 x (2 + 17 + 8) = 442,368 bits = 44 M10K
//       = 275 M10K  (50%)
//
// THE SMALLER FORM, TAKEN HERE, AND WHY IT IS BIT-IDENTICAL: the underside's
// records are never read.  `zhao_terrain_lod` law 7 -- "THE UNDERSIDE TAKES
// THE TOP'S LEVEL" -- emits the underside job with the top's level, morph and
// neighbour levels, from the same descriptor.  Its `sp_*` port has no surface
// field at all.  So a second surface's deviations can be computed and thrown
// away with no effect on one output bit, which is the definition R59 asks for.
//
//     1,024 slots x 16 subpatches x 3 levels x 24 bits
//       = 1,179,648 bits = 116 M10K of 553  (21%)
//   + history 442,368 bits = 44 M10K
//       = 160 M10K  (29%)
//
// A FURTHER 29% WAS FOUND AND IS REFUSED, and it is refused for a reason worth
// keeping: every lattice height reaching `zhao_terrain_loddev` today is
// `height16 << 8` (qformats 9's exact up-conversion), so every deviation is a
// multiple of 128 and its low SEVEN BITS ARE PROVABLY ZERO -- 3 x 17 bits
// instead of 3 x 24 would be exact, 82 M10K instead of 116.  It is refused
// because the invariant is upstream, unenforced and invisible: the moment a
// composed height carries a FIELD delta (entry I34's lane, `zref::terrain`'s
// live-field arithmetic) the low bits stop being zero and every deviation
// silently quantises to 128ths with no counter able to see it.  A packing that
// is exact only while somebody else keeps a promise they never made is the
// broken-instrument shape, and 34 M10K is not worth it.
//
// AND A THIRD FORM IS AN OWNER DECISION, NOT TAKEN HERE.  These 160 M10K are
// per-page DERIVED data, which is exactly what `spec/memory_rules.md` 5b gives
// an SDRAM home to for the coarse-height mips (TERRAIN.RESIDENT_MIP_POOL,
// 1,024 x 1,536 B).  144 B per patch of deviations would fit the same pattern
// at 147 KB of SDRAM and 36 KB/frame of read bandwidth, and cost no M10K at
// all.  It is not taken here because ruling R24 says M10K in as many words and
// because it needs a new guarded region, which is an ABI act.  Recorded so the
// owner can spend the 160 M10K deliberately or move it.
//
// ===========================================================================
// THE SHAPE, AND WHY IT IS NOT THE OBVIOUS ONE
// ===========================================================================
// The obvious array is `mem[SLOTS*16]` of 72 bits -- one record per row.  On
// Cyclone V that is 16,384 deep, and an M10K's deepest mode is 8,192 x 1, so a
// 72-bit word costs 72 bit-planes x 2 depth banks = 144 M10K for 1,179,648
// bits of payload: 80% efficient, 28 M10K wasted on the shape alone.
//
// This array is EIGHT RECORDS PER ROW: 2,048 rows x 576 bits.  At 2,048 deep
// an M10K runs in its 2,048 x 5 mode, so 576 / 5 = 116 M10K and the payload
// packs at 99%.  The price is a 576-bit staging register on each side, about
// 1,200 flops, and ALMs are the binding constraint -- so it is stated rather
// than assumed: 28 M10K bought for ~1,200 flops is the wrong trade if the
// device is short of ALMs and the right one if it is short of M10K.  Both
// numbers are here so the next pass can reverse it with one localparam.
//
// The history is 1,024 x 432 (all sixteen records of one patch in one row),
// because it is read whole at the start of a patch and written whole at the
// end -- 1,024 deep runs in 1,024 x 10, so 432 / 10 = 44 M10K at 96%.
//
// ===========================================================================
// WHAT AN UNWRITTEN SLOT ANSWERS, AND WHY IT IS NOT ZERO
// ===========================================================================
// A patch can be served before its deviations exist -- the mip pass and the
// compose pass are independent, and ruling R24's BAKE re-trigger makes a
// resident page's records momentarily stale by design.  Zero is the WRONG
// answer: `dev = 0` passes every rung of `zhao_terrain_lod`'s ladder, so the
// patch would be drawn at level 3, the coarsest, and mush is indistinguishable
// from distance.  This block answers DEV_MAX instead (every ladder rung fails,
// the patch is drawn at full detail) and COUNTS it on `read_unwritten_o`.  The
// cost of the fail-safe is triangles; the cost of the other one is a wrong
// picture nobody can attribute.
//
// The per-slot valid bits are FLOPS (1,024 of them) and not a memory, because
// they are read and written on different cycles from the payload and a 1-bit
// M10K would cost a whole block for 1,024 bits.
//
// ===========================================================================
// THE TWO COUNTERS AND HOW THEY WERE FIRED
// ===========================================================================
// `read_unwritten_o` fires on any read of a slot with no committed records;
// the directed test reads slot 3 before writing it.
//
// `hist_step_bad_o` is the one CLAUDE.md's "detector wired to two operands
// that move together" rule is about.  The read side walks sp 0..15 off `rd_i`
// and the history writeback walks sp 0..15 off `h_ptr_q`, and they are
// SEPARATE counters advanced by SEPARATE handshakes -- so a history record
// arriving out of step with the descriptor it belongs to is visible.  If the
// two pointers were one register the check could not fail and would be worth
// nothing.  The directed test fires it by writing a seventeenth history record.
//
// Conservative SystemVerilog subset (charter 2); no package dependencies.
// Lint gate: lint_terrain_devstore.
`default_nettype none

module zhao_terrain_devstore #(
    // The residency's slot count.  T9/T10's 256 sets x 4 ways.
    parameter int unsigned SLOTS  = 1024,
    parameter int unsigned SLOTW  = 10,
    // `zhao_terrain_loddev`'s deviation width and `zhao_terrain_lod`'s morph.
    parameter int unsigned DEVW   = 24,
    parameter int unsigned MORPHW = 17
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- WRITE: `zhao_terrain_loddev`'s records, one patch at a time -------
    // Records arrive in subpatch order 0..15.  The row is committed on 7 and
    // 15; the slot becomes valid on 15.
    input  var logic             w_valid_i,
    output var logic             w_ready_o,
    input  var logic [SLOTW-1:0] w_slot_i,
    input  var logic [3:0]       w_sp_i,
    input  var logic [DEVW-1:0]  w_dev1_i,
    input  var logic [DEVW-1:0]  w_dev2_i,
    input  var logic [DEVW-1:0]  w_dev3_i,
    input  var logic signed [15:0] w_cy_i,     // subpatch centre height16
    output var logic             w_patch_done_o,   // pulse: sp 15 committed

    // ---- INVALIDATE: the page in this slot changed (claim, or a bake) ------
    input  var logic             inv_valid_i,
    input  var logic [SLOTW-1:0] inv_slot_i,

    // ---- READ: one patch's sixteen descriptors, in subpatch order ---------
    input  var logic             r_start_i,
    input  var logic [SLOTW-1:0] r_slot_i,
    output var logic             r_ready_o,        // a start may be taken
    output var logic             r_valid_o,
    input  var logic             r_ready_i,
    output var logic [3:0]       r_sp_o,
    output var logic [DEVW-1:0]  r_dev1_o,
    output var logic [DEVW-1:0]  r_dev2_o,
    output var logic [DEVW-1:0]  r_dev3_o,
    output var logic signed [15:0] r_cy_o,
    output var logic [1:0]       r_prev_level_o,
    output var logic [MORPHW-1:0] r_prev_morph_o,
    output var logic [7:0]       r_hold_o,
    output var logic             r_fresh_o,        // this patch had records

    // ---- HISTORY WRITEBACK: `zhao_terrain_lod`'s decisions, same order ----
    input  var logic              h_valid_i,
    output var logic              h_ready_o,
    input  var logic [1:0]        h_level_i,
    input  var logic [MORPHW-1:0] h_morph_i,
    input  var logic [7:0]        h_hold_i,

    // ---- evidence ---------------------------------------------------------
    output var logic [31:0] records_written_o,
    output var logic [31:0] patches_committed_o,
    output var logic [31:0] patches_read_o,
    output var logic [31:0] read_unwritten_o,   // served before its records
    output var logic [31:0] hist_step_bad_o,    // writeback out of step
    output var logic [31:0] invalidations_o,
    output var logic        busy_o
);

  localparam int unsigned SUBS   = 16;
  // A record is the three deviations plus the subpatch CENTRE HEIGHT.  The
  // centre is what zhao_terrain_lod measures its camera distance to, and
  // its x and z come free from TERRAIN.PLACE's placed column/row stream --
  // only the HEIGHT has to be remembered, because the lattice it was read
  // from is gone by the time the patch is served.  height16, 16 bits, the
  // page's own unit.
  localparam int unsigned RECW   = 3 * DEVW + 16;       // 88
  localparam int unsigned HALF   = SUBS / 2;            // 8 records per row
  localparam int unsigned ROWW   = HALF * RECW;         // 576
  localparam int unsigned ROWS   = 2 * SLOTS;           // 2,048
  // The accumulator holds the SEVEN records before the one that commits the
  // row; the eighth is merged on its own cycle rather than stored and read
  // back, so there are 72 fewer flops and one fewer state.
  localparam int unsigned ACCW   = ROWW - RECW;         // 504

  localparam int unsigned HISTW  = 2 + MORPHW + 8;      // 27
  localparam int unsigned HROWW  = SUBS * HISTW;        // 432

  localparam logic [DEVW-1:0] DEV_MAX = {DEVW{1'b1}};

  // Quartus 17.0 rejects a module-scope `if` guard; qualify inside `initial`.
  // synthesis translate_off
  initial begin
    if (SLOTS != (1 << SLOTW))
      $fatal(1, "zhao_terrain_devstore: SLOTS must be 1 << SLOTW");
    if (DEVW < 8 || DEVW > 32)
      $fatal(1, "zhao_terrain_devstore: DEVW out of range");
  end
  // synthesis translate_on

  // ---- the two memories ---------------------------------------------------
  // 2,048 x 576 deviations, 1,024 x 432 history.  Written and read on ONE
  // address each per cycle so both infer.
  logic [ROWW-1:0]  dev_mem  [ROWS];
  logic [HROWW-1:0] hist_mem [SLOTS];
  logic [SLOTS-1:0] slot_valid_q;

  // ---- WRITE SIDE ---------------------------------------------------------
  logic [ACCW-1:0]  w_acc_q;
  logic             w_slot_held_q;

  // Always ready: `zhao_terrain_loddev` emits at most one record per 800-odd
  // clocks and a back-pressured store would stall a walk for nothing.
  assign w_ready_o = 1'b1;

  // ---- READ SIDE ----------------------------------------------------------
  typedef enum logic [1:0] { R_IDLE, R_FETCH, R_STREAM } rstate_e;
  rstate_e          rstate_q;
  logic [ROWW-1:0]  r_row_q;
  logic [HROWW-1:0] r_hist_q;
  logic [HROWW-1:0] h_acc_q;
  logic [SLOTW-1:0] r_slot_q;
  logic [3:0]       r_ptr_q;      // 0..15, the descriptor being offered
  logic [3:0]       h_ptr_q;      // 0..15, the history record being taken
  logic             r_fresh_q;
  logic             r_second_q;   // the high row has been fetched

  assign r_ready_o = (rstate_q == R_IDLE);
  assign busy_o    = (rstate_q != R_IDLE) || w_slot_held_q;
  assign r_valid_o = (rstate_q == R_STREAM);
  assign r_sp_o    = r_ptr_q;
  assign r_fresh_o = r_fresh_q;

  // The record currently offered, sliced out of the 576-bit row.  `r_ptr_q[2:0]`
  // is the record within the row; bit 3 selects which row is loaded.
  logic [RECW-1:0] r_rec_c;
  logic [HISTW-1:0] r_hist_c;
  always_comb begin
    r_rec_c  = r_row_q[({4'd0, r_ptr_q[2:0]} * RECW) +: RECW];
    r_hist_c = r_hist_q[({4'd0, r_ptr_q} * HISTW) +: HISTW];
  end

  assign r_dev1_o = r_fresh_q ? r_rec_c[0        +: DEVW] : DEV_MAX;
  assign r_dev2_o = r_fresh_q ? r_rec_c[DEVW     +: DEVW] : DEV_MAX;
  assign r_dev3_o = r_fresh_q ? r_rec_c[2 * DEVW +: DEVW] : DEV_MAX;
  // The centre height is answered even for a slot with no records: a wrong
  // distance to a patch drawn at full detail is a smaller fault than a wrong
  // level, and zero is the island datum rather than a poison value.
  assign r_cy_o   = r_fresh_q ? signed'(r_rec_c[3 * DEVW +: 16]) : 16'sd0;

  assign r_prev_level_o = r_hist_c[1:0];
  assign r_prev_morph_o = r_hist_c[2 +: MORPHW];
  assign r_hold_o       = r_hist_c[(2 + MORPHW) +: 8];

  assign h_ready_o = (rstate_q == R_STREAM);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      w_acc_q       <= '0;
      w_slot_held_q <= 1'b0;
      w_patch_done_o <= 1'b0;
      rstate_q      <= R_IDLE;
      r_row_q       <= '0;
      r_hist_q      <= '0;
      h_acc_q       <= '0;
      r_slot_q      <= '0;
      r_ptr_q       <= '0;
      h_ptr_q       <= '0;
      r_fresh_q     <= 1'b0;
      r_second_q    <= 1'b0;
      slot_valid_q  <= '0;
      records_written_o   <= '0;
      patches_committed_o <= '0;
      patches_read_o      <= '0;
      read_unwritten_o    <= '0;
      hist_step_bad_o     <= '0;
      invalidations_o     <= '0;
    end else begin
      w_patch_done_o <= 1'b0;

      // ---- invalidation.  A claim or a bake makes a slot's records stale,
      // and stale records are worse than none: they describe a lattice that
      // has been replaced.  R24's re-trigger arrives as a second mip pass;
      // until it commits, the slot reads DEV_MAX and is counted.
      if (inv_valid_i) begin
        slot_valid_q[inv_slot_i] <= 1'b0;
        invalidations_o <= invalidations_o + 32'd1;
      end

      // ---- write ----------------------------------------------------------
      if (w_valid_i) begin
        records_written_o <= records_written_o + 32'd1;
        w_slot_held_q <= 1'b1;
        // Place the record in the accumulator at its position within the row.
        // Record 7 never lands here -- it is merged on this same cycle below.
        if (w_sp_i[2:0] != 3'd7)
          w_acc_q[({4'd0, w_sp_i[2:0]} * RECW) +: RECW] <= {w_cy_i, w_dev3_i, w_dev2_i, w_dev1_i};

        if (w_sp_i[2:0] == 3'd7) begin
          // The eighth record of a row completes it.  The accumulator does not
          // hold this record, so it is merged here rather than read back.
          dev_mem[{w_slot_i, w_sp_i[3]}] <=
              {{w_cy_i, w_dev3_i, w_dev2_i, w_dev1_i}, w_acc_q};
          if (w_sp_i[3]) begin
            slot_valid_q[w_slot_i] <= 1'b1;
            patches_committed_o <= patches_committed_o + 32'd1;
            w_patch_done_o <= 1'b1;
            w_slot_held_q  <= 1'b0;
          end
        end
      end

      // ---- read -----------------------------------------------------------
      case (rstate_q)
        R_IDLE: if (r_start_i) begin
          r_slot_q   <= r_slot_i;
          r_fresh_q  <= slot_valid_q[r_slot_i];
          r_ptr_q    <= 4'd0;
          h_ptr_q    <= 4'd0;
          r_second_q <= 1'b0;
          r_row_q    <= dev_mem[{r_slot_i, 1'b0}];
          r_hist_q   <= hist_mem[r_slot_i];
          h_acc_q    <= hist_mem[r_slot_i];
          patches_read_o <= patches_read_o + 32'd1;
          if (!slot_valid_q[r_slot_i])
            read_unwritten_o <= read_unwritten_o + 32'd1;
          rstate_q   <= R_FETCH;
        end

        // One cycle for the row to arrive.  The memory read above is
        // registered into `r_row_q`, so this state exists to let it land
        // before the first descriptor is offered.
        R_FETCH: rstate_q <= R_STREAM;

        R_STREAM: begin
          // The history record for the descriptor being retired.
          if (h_valid_i) begin
            if (h_ptr_q != r_ptr_q) hist_step_bad_o <= hist_step_bad_o + 32'd1;
            h_acc_q[({4'd0, h_ptr_q} * HISTW) +: HISTW] <=
                {h_hold_i, h_morph_i, h_level_i};
            h_ptr_q <= h_ptr_q + 4'd1;
          end

          if (r_valid_o && r_ready_i) begin
            if (r_ptr_q == 4'd15) begin
              // The history row is written whole, with this cycle's record
              // merged in so the accumulator is not read back.
              hist_mem[r_slot_q] <= h_valid_i
                  ? { {h_hold_i, h_morph_i, h_level_i},
                      h_acc_q[0 +: (HROWW - HISTW)] }
                  : h_acc_q;
              rstate_q <= R_IDLE;
            end else begin
              r_ptr_q <= r_ptr_q + 4'd1;
              if (r_ptr_q == 4'd7 && !r_second_q) begin
                r_row_q    <= dev_mem[{r_slot_q, 1'b1}];
                r_second_q <= 1'b1;
              end
            end
          end
        end

        default: rstate_q <= R_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
