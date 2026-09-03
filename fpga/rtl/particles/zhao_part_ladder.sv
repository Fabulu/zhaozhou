// zhao_part_ladder.sv — which representation is this particle, in this camera,
// this frame?
//
// ---------------------------------------------------------------------------
// THE LADDER, FROZEN (owner ruling 2026-08-31 §2.5)
// ---------------------------------------------------------------------------
//     meshlet -> triangle/shard -> ribbon/streak -> soft sprite -> glint
//             -> culled
//
// And the answer that shapes the whole block:
//
//   > Yes: a particle may change representation while it is alive. The
//   > simulation state never changes because of the representation. Selection
//   > is PER CAMERA and PER FRAME, so the same particle may be a triangle in
//   > one Duo view and a glint in the other.
//
// Both halves matter. "May change" is why there is hysteresis at all. "The
// simulation never changes" is why this block is DOWNSTREAM OF EVERYTHING and
// feeds back into nothing -- it cannot reseed, respawn or perturb a particle,
// and it has no port with which to try.
//
// ---------------------------------------------------------------------------
// IT OWNS NO STORAGE, WHICH IS THE CONTRACT'S DECISION AND NOT A SHORTCUT
// ---------------------------------------------------------------------------
// The hold state is per (particle, camera): 32,768 particles x 2 cameras is
// 64 KiB, and the contract is explicit that this does not belong in M10K beside
// the renderer -- "a design that put the hold state on chip would be choosing
// 64 KiB of M10K to avoid a few bytes per particle of DDR traffic, and that
// trade should be measured, not assumed."
//
// So `prev_rung` and `hold_count` come IN and go OUT. The block is a function.
//
// ---------------------------------------------------------------------------
// OVERLAPPING BANDS RESOLVE BY THE LADDER'S OWN ORDER
// ---------------------------------------------------------------------------
// The provisional thresholds overlap on purpose -- triangle is ~6-18 px and
// soft sprite is ~2-8, so a 7 px particle satisfies both. The ladder is an
// ORDER, so the resolution is to take the FIRST rung whose threshold is met,
// walking coarse to fine. That is deterministic, needs no tie-break table, and
// matches how the ladder is written down.
//
// Every threshold is a named parameter. They are Class B -- evidence-driven
// defaults, not ABI -- and a value that came from a measurement still has to
// stay the owner's to move.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_part_ladder #(
    // Projected size in U 8.8 pixels, so 1.0 px is 256.
    parameter int unsigned MESHLET_MIN = 18 * 256,   // >= 18 px diameter
    parameter int unsigned SHARD_MIN   =  6 * 256,   // ~6-18 px
    parameter int unsigned RIBBON_MIN  =  4 * 256,   // trail >= 4 px
    parameter int unsigned SPRITE_MIN  =  2 * 256,   // ~2-8 px
    parameter int unsigned GLINT_MIN   =      128,   // 0.5 px
    // Frames a new rung must be selected before the change is committed.
    parameter int unsigned HOLD_FRAMES = 3
) (
    input var logic clk,
    input var logic rst_n,

    input  var logic        v_valid_i,
    output var logic        v_ready_o,

    input  var logic [15:0] p_size_i,        // projected size, U 8.8 px
    input  var logic [15:0] p_trail_i,       // trail length, U 8.8 px
    input  var logic        p_narrow_i,      // species: reads as a line
    input  var logic        p_protected_i,   // species: never culled
    input  var logic [2:0]  p_gov_floor_i,   // governor: coarsest allowed rung
    input  var logic [2:0]  p_prev_rung_i,
    input  var logic [3:0]  p_hold_i,
    input  var logic        p_first_i,       // no valid hold state yet

    output var logic        r_valid_o,
    input  var logic        r_ready_i,
    output var logic [2:0]  r_rung_o,
    output var logic [3:0]  r_hold_o,
    output var logic        r_changed_o,

    output var logic [31:0] decisions_o,
    output var logic [31:0] changes_o,
    output var logic [31:0] held_o,          // a change the hysteresis suppressed
    output var logic [31:0] gov_forced_o     // the governor coarsened the choice
);

  // The rungs, coarse to fine. The numbering IS the ladder order, so "coarser"
  // is "numerically smaller" and the governor floor is a max().
  localparam logic [2:0] RUNG_MESHLET = 3'd0;
  localparam logic [2:0] RUNG_SHARD   = 3'd1;
  localparam logic [2:0] RUNG_RIBBON  = 3'd2;
  localparam logic [2:0] RUNG_SPRITE  = 3'd3;
  localparam logic [2:0] RUNG_GLINT   = 3'd4;
  localparam logic [2:0] RUNG_CULLED  = 3'd5;

  // ---- the raw choice: first rung whose threshold is met ------------------
  logic [2:0] raw_c;
  always_comb begin
    if (p_size_i >= 16'(MESHLET_MIN))                       raw_c = RUNG_MESHLET;
    else if (p_size_i >= 16'(SHARD_MIN))                    raw_c = RUNG_SHARD;
    // A streak is chosen on its TRAIL, not on its size, and only if the
    // species says it reads as a line. A round particle with a long trail is
    // not a streak, it is a round particle that moved.
    else if (p_narrow_i && p_trail_i >= 16'(RIBBON_MIN))    raw_c = RUNG_RIBBON;
    else if (p_size_i >= 16'(SPRITE_MIN))                   raw_c = RUNG_SPRITE;
    else if (p_size_i >= 16'(GLINT_MIN))                    raw_c = RUNG_GLINT;
    else                                                    raw_c = RUNG_CULLED;
  end

  // ---- the governor, and this reading of it is AN INTERPRETATION ----------
  // The contract says this block consumes "governor targets" and does not say
  // by what mechanism. Taken here as a FLOOR on coarseness: the governor may
  // force a particle coarser than its size asks for, and may never force it
  // finer. That is the only direction that saves work, which is what a
  // governor is for.
  //
  // Flagged rather than assumed silently, and separated into its own counter,
  // so if the owner meant something else it is one line and the evidence
  // already says how often it mattered.
  logic [2:0] govd_c;
  logic       gov_hit_c;
  always_comb begin
    govd_c    = (p_gov_floor_i > raw_c) ? p_gov_floor_i : raw_c;
    gov_hit_c = (p_gov_floor_i > raw_c);
  end

  // ---- semantic protection ------------------------------------------------
  // "Semantically protected is a SPECIES FLAG, not an inference" -- a tiny but
  // important particle stays visible because its asset says so. It is applied
  // AFTER the governor: a governor that could cull a protected particle would
  // make the flag a suggestion.
  logic [2:0] want_c;
  always_comb begin
    want_c = (p_protected_i && govd_c == RUNG_CULLED) ? RUNG_GLINT : govd_c;
  end

  // ---- hysteresis ---------------------------------------------------------
  // A change is committed only after the new rung has been wanted for
  // HOLD_FRAMES consecutive frames. `p_first_i` bypasses it: a particle with
  // no hold state yet has nothing to be continuous with, and making its first
  // frame wait would show every new particle at the wrong rung for three
  // frames.
  logic [2:0] rung_c;
  logic [3:0] hold_c;
  logic       changed_c, held_c;
  always_comb begin
    changed_c = 1'b0;
    held_c    = 1'b0;
    if (p_first_i) begin
      rung_c    = want_c;
      hold_c    = 4'd0;
      changed_c = 1'b1;
    end else if (want_c == p_prev_rung_i) begin
      rung_c = p_prev_rung_i;
      hold_c = 4'd0;                      // agreement resets the count
    end else if (p_hold_i + 4'd1 >= 4'(HOLD_FRAMES)) begin
      rung_c    = want_c;
      hold_c    = 4'd0;
      changed_c = 1'b1;
    end else begin
      rung_c = p_prev_rung_i;             // keep the old one, and remember
      hold_c = p_hold_i + 4'd1;
      held_c = 1'b1;
    end
  end

  assign v_ready_o = !r_valid_o || r_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // Reset clears the hold state, and the contract calls that out as a
      // VISIBLE consequence: the first frame after a reset may show changes
      // the hold would normally suppress. The alternative is stale hold state
      // deciding against a camera that no longer exists.
      r_valid_o   <= 1'b0;
      decisions_o <= '0;
      changes_o   <= '0;
      held_o      <= '0;
      gov_forced_o<= '0;
    end else begin
      if (!r_valid_o || r_ready_i) begin
        r_valid_o <= v_valid_i;
        if (v_valid_i) begin
          r_rung_o    <= rung_c;
          r_hold_o    <= hold_c;
          r_changed_o <= changed_c;

          decisions_o <= decisions_o + 32'd1;
          if (changed_c) changes_o    <= changes_o + 32'd1;
          if (held_c)    held_o       <= held_o + 32'd1;
          if (gov_hit_c) gov_forced_o <= gov_forced_o + 32'd1;
        end
      end
    end
  end

endmodule : zhao_part_ladder

`default_nettype wire
