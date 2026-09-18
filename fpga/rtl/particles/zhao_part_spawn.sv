// zhao_part_spawn.sv -- PART.SPAWN: deterministic child spawn.
//
// Law, in citation order:
//   design/contracts/PART.SPAWN.md      -- the block contract, owner ZH-064, phase 10.
//   Owner ruling 2026-08-31 SS2.4       -- the four events and the 16-child bound, FROZEN.
//   Owner ruling 2026-08-31 SS2.1       -- the 128-bit record layout, FROZEN.
//   reference/include/zref/zref_particle.hpp
//                                       -- particle128 v1 codec (qformats SS10,
//                                          amendment C2 / ruling R3). THE field offsets
//                                          and flag bits below are that header's, not
//                                          this file's. Do not re-derive them here; that
//                                          is the duplicate-arithmetic failure CLAUDE.md
//                                          has a chapter about.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS
// ---------------------------------------------------------------------------
// A parent particle arrives with a bitmask of which of its four events fired.
// For each fired event, in event order, the species descriptor says what species
// to spawn and how many. This block emits that many child records into
// PART.STATE's child channel, in the exact order
//
//     parent stream order  ->  event order  ->  child index 0..N-1
//
// and that ordering IS the determinism contract. It is enforced structurally --
// one parent, then one event, then one ascending index -- rather than by a
// priority comparison that could be got wrong.
//
// ---------------------------------------------------------------------------
// WHAT IT DELIBERATELY DOES NOT DO, AND WHY
// ---------------------------------------------------------------------------
// It does NOT compute child physics. The derivation here is IDENTITY, behind two
// named knobs (CHILD_POS_FROM_PARENT / CHILD_VEL_FROM_PARENT).
//
// CORRECTED 2026-09-18. This header first justified that by quoting the
// contract -- child position and velocity are "subject to the same open Class-C
// scale question recorded in PART.UPDATE" -- and concluding the scale is
// unruled. **The scale is ruled.** Amendment C2 (2026-09-02, QFMT_VERSION 2->3,
// owner ruling R3) replaced spec/qformats.md SS10 whole and pins position to
// s18 S 9.8 m and velocity to s11 S 2.8 m/tick. PART.SPAWN.md and PART.UPDATE.md
// were both written before that and still describe the pre-C2 status;
// PART.EXPAND.md and PART.SOFT.md carry a SUPERSEDED banner and these two do not.
//
// Identity is still correct, for a DIFFERENT and better reason: the contract
// specifies no derivation at all. It says the child's own behaviour is its
// species descriptor's and that "this block only places it into the stream".
// A scale being ruled does not tell anyone what offset a child gets from its
// parent, so inventing one here would still be authoring physics in the block
// the contract says does not own it. The knobs stay because when a derivation IS
// ruled it belongs in one named place.
//
// It owns NO memory (contract: "None. Children are appended into PART.STATE's
// write stream; that block owns the buffers. The species table is read as a
// port."). The standing owner direction to spend M10K rather than ALMs does not
// apply here: the contract's ceiling is 1,200 ALM / 2 DSP / **0 M10K**, and the
// one table this block would want lives behind spc_*.
//
// It uses NO multipliers. The child hash is logic, per the ceiling's own note:
// "the hash is logic, not arithmetic. If this block grows DSPs, child behaviour
// has migrated in from PART.UPDATE, where it belongs."
//
// ---------------------------------------------------------------------------
// ONE GENERATION PER TICK
// ---------------------------------------------------------------------------
// "A child may spawn on a later tick, never recursively in the same one. That is
// what bounds the work: without it, a single tick could cascade unboundedly and
// no fixed-latency stage could contain it."
//
// Here that is structural and not a check: children are emitted, never fed back
// into par_*. This block has no path from its own output to its own input, so a
// same-tick recursion is not prevented -- it is unrepresentable.
//
// ---------------------------------------------------------------------------
// REFUSAL IS WHOLE-GROUP, AND THAT IS NOT A SIMPLIFICATION
// ---------------------------------------------------------------------------
// "child count > 16 | refuse the group, count it. Do not emit the first 16 -- a
// truncated burst is a different effect, silently."
//
// So the refusal decision is taken BEFORE the first child is emitted, in S_EVAL,
// and a refused group emits nothing at all. The three reasons are kept apart in
// groups_refused_by_reason_o because "a budget overrun shows as a shape".
//
// Capacity is the one that is NOT whole-group, and the contract says so in
// different words: "later children are dropped deterministically". So
// cap_full_i drops from the END of the group, by index, never by arrival time.
//
// Conservative SystemVerilog subset only (charter SS2).
// ---------------------------------------------------------------------------

`default_nettype none

module zhao_part_spawn #(
    parameter int unsigned REC_W     = 128,
    parameter int unsigned SPECIES_N = 128,  // 7 bits of species, per SS2.1
    parameter int unsigned PID_W     = 16,   // parent id, the hash's first seed word
    parameter int unsigned TICK_W    = 32,   // tick counter, the hash's last seed word
    // The ruling's bound. A parameter so a test can make refusal reachable
    // cheaply, NOT so production can raise it -- SS2.4 froze 16.
    parameter int unsigned MAX_CHILD = 16,
    // Class-C scale placeholders. Identity today; see the header.
    parameter bit CHILD_POS_FROM_PARENT = 1'b1,
    parameter bit CHILD_VEL_FROM_PARENT = 1'b1
) (
    input var logic clk,
    input var logic rst_n,

    // tick boundary: clears the per-tick content-bound watermark only
    input  wire                    tick_start_i,

    // ---- parent stream in ---------------------------------------------------
    input  wire                    par_valid_i,
    output wire                    par_ready_o,
    input  wire [REC_W-1:0]        par_record_i,
    input  wire [PID_W-1:0]        par_id_i,
    // one bit per event, in EVENT ORDER: 0 birth, 1 age marker, 2 collision, 3 death
    input  wire [3:0]              par_events_i,
    input  wire [TICK_W-1:0]       tick_i,

    // ---- species descriptor, read as a port (this block owns no table) ------
    output wire [6:0]              spc_species_o,    // the PARENT's species
    output wire [1:0]              spc_event_o,      // which event we are asking about
    input  wire                    spc_known_i,      // descriptor resolves at all
    input  wire [6:0]              spc_child_spc_i,  // species of the child to spawn
    input  wire [4:0]              spc_count_i,      // 0..16 legal; 17..31 refuses

    // ---- child stream out, into PART.STATE's chl_* channel ------------------
    output wire                    chl_valid_o,
    input  wire                    chl_ready_i,
    output wire [REC_W-1:0]        chl_record_o,

    // ---- capacity backstop --------------------------------------------------
    // PART.STATE knows when the generation is full. Survivors outrank children,
    // so a full generation drops children from the END of the group.
    input  wire                    cap_full_i,

    // ---- counters (contract "Counters and traces") --------------------------
    output var logic [31:0]        children_requested_o,
    output var logic [31:0]        children_emitted_o,
    output var logic [31:0]        children_refused_o,
    output var logic [31:0]        spawn_by_event0_o,
    output var logic [31:0]        spawn_by_event1_o,
    output var logic [31:0]        spawn_by_event2_o,
    output var logic [31:0]        spawn_by_event3_o,
    output var logic [31:0]        refused_count_gt_max_o,   // reason 0
    output var logic [31:0]        refused_unknown_species_o,// reason 1
    output var logic [31:0]        refused_capacity_o,       // reason 2
    output var logic [31:0]        max_children_in_tick_o
);

  // ---- the ratified record layout. zref_particle.hpp is the source. --------
  localparam int unsigned OFF_POS = 0;    localparam int unsigned W_POS = 18;
  localparam int unsigned OFF_VEL = 54;   localparam int unsigned W_VEL = 11;
  localparam int unsigned OFF_AGE = 87;   localparam int unsigned W_AGE = 10;
  localparam int unsigned OFF_SPC = 97;   localparam int unsigned W_SPC = 7;
  // size (6 bits @ 104) is inherited from the parent unchanged -- this block has
  // no licence over child behaviour, so it is not named as a field it rewrites.
  localparam int unsigned OFF_SPN = 110;  localparam int unsigned W_SPN = 6;
  localparam int unsigned OFF_FLG = 116;  localparam int unsigned W_FLG = 4;
  localparam int unsigned OFF_VAR = 120;  localparam int unsigned W_VAR = 8;

  // Ratified flag bits, zref_particle.hpp. kPartFlagReserved (0x8) is
  // "zero in, preserved zero" and is therefore NOT set here.
  localparam logic [3:0] FLG_BORN_THIS_TICK = 4'h4;

  // Elaboration guards. Quartus 17.0 needs these inside initial begin/end --
  // a bare module-scope `if` is a syntax error there however clean the lint.
  // And --lint-only does not run initial blocks, so a clean lint says nothing
  // whatever about these firing.
  initial begin
    if (REC_W != 128)
      $fatal(1, "zhao_part_spawn: REC_W must be 128; the record layout is FROZEN (ruling 2026-08-31 SS2.1)");
    if (OFF_VAR + W_VAR != REC_W)
      $fatal(1, "zhao_part_spawn: field map does not tile the record exactly");
    if (SPECIES_N > (1 << W_SPC))
      $fatal(1, "zhao_part_spawn: SPECIES_N exceeds the 7-bit species field");
    if (MAX_CHILD < 1 || MAX_CHILD > 16)
      $fatal(1, "zhao_part_spawn: MAX_CHILD outside 1..16; owner ruling 2026-08-31 SS2.4 froze 16");
  end

  // ---- state --------------------------------------------------------------
  localparam logic [1:0] S_IDLE = 2'd0;  // no parent held
  localparam logic [1:0] S_EVAL = 2'd1;  // descriptor presented, decide the group
  localparam logic [1:0] S_EMIT = 2'd2;  // walking child index 0..N-1

  logic [1:0]        st_q;
  logic [REC_W-1:0]  par_q;
  logic [PID_W-1:0]  pid_q;
  logic [3:0]        ev_left_q;   // events still to consider, event order = bit order
  logic [1:0]        ev_q;        // the event being evaluated/emitted
  logic [4:0]        n_q;         // children in this group
  logic [4:0]        idx_q;       // next child index to emit
  logic [6:0]        cspc_q;      // child species for this group
  logic [TICK_W-1:0] tick_q;
  logic [31:0]       tick_children_q;  // watermark accumulator for this tick

  // ---- ONE event-advance, used by every exit path -------------------------
  // The first draft open-coded this priority encoder four times, once per way a
  // group can end. Four copies of the same decision is how the copies disagree
  // later, and the ordering they implement IS the determinism contract.
  wire [3:0] ev_rem_c = ev_left_q & ~(4'b0001 << ev_q);
  logic [1:0] ev_pick_c;
  always_comb begin
    ev_pick_c = 2'd0;
    if      (ev_rem_c[0]) ev_pick_c = 2'd0;
    else if (ev_rem_c[1]) ev_pick_c = 2'd1;
    else if (ev_rem_c[2]) ev_pick_c = 2'd2;
    else if (ev_rem_c[3]) ev_pick_c = 2'd3;
  end

  assign spc_species_o = par_q[OFF_SPC +: W_SPC];
  assign spc_event_o   = ev_q;

  // ---- the group decision, taken BEFORE any child is emitted --------------
  wire bad_count_c   = (spc_count_i > 5'(MAX_CHILD));
  // WIDER THAN THE FIELD, deliberately. Written first as spc_child_spc_i >=
  // 7'(SPECIES_N), which at the default SPECIES_N = 128 truncates to 7'd0 and
  // makes the comparison constant TRUE -- every group refused, every child
  // lost, and the counters would have said so in a way that looked like the
  // species table was wrong. Verilator's UNSIGNED warning caught it. Compare in
  // 8 bits so SPECIES_N = 128 means "no 7-bit species is out of range".
  wire bad_species_c = (!spc_known_i) || ({1'b0, spc_child_spc_i} >= 8'(SPECIES_N));
  wire empty_c       = (spc_count_i == 5'd0);
  wire refuse_c      = bad_count_c || bad_species_c;

  // ---- the stateless child hash -------------------------------------------
  // Seeded by {parent_id, event, child_index, tick} EXACTLY, per the contract:
  // "never from a running counter. A counter would make a child's identity
  // depend on how many particles were processed before it, which is the
  // iteration-order dependence PART.STATE's contract forbids."
  //
  // Pure combinational avalanche, no multiplier, no state. It mixes in 32 bits
  // and RETURNS ONLY THE 14 IT OWNS -- 8 of variation and 6 of spin -- so the
  // unused half cannot sit in the design looking like a field nobody wired.
  function automatic logic [13:0] child_hash(input logic [PID_W-1:0]  pid,
                                             input logic [1:0]        ev,
                                             input logic [4:0]        idx,
                                             input logic [TICK_W-1:0] tk);
    logic [31:0] h;
    begin
      h = 32'h9E37_79B9;
      h = h ^ {{(32 - PID_W){1'b0}}, pid};
      h = h ^ tk[31:0];
      h = h ^ {25'd0, idx, ev};
      h = h ^ (h << 13);
      h = h ^ (h >> 17);
      h = h ^ (h << 5);
      h = h ^ (h >> 11);
      h = h ^ (h << 9);
      return h[13:0];
    end
  endfunction

  wire [13:0] hash_c = child_hash(pid_q, ev_q, idx_q, tick_q);

  // ---- the child record ---------------------------------------------------
  // Everything NOT named here is inherited from the parent unchanged, size
  // included -- the contract gives this block no licence over child behaviour.
  logic [REC_W-1:0] child_c;
  always_comb begin
    child_c = par_q;
    if (!CHILD_POS_FROM_PARENT) child_c[OFF_POS +: 3*W_POS] = '0;
    if (!CHILD_VEL_FROM_PARENT) child_c[OFF_VEL +: 3*W_VEL] = '0;
    child_c[OFF_AGE +: W_AGE] = '0;                       // a child is age zero
    child_c[OFF_SPC +: W_SPC] = cspc_q;                   // the descriptor's species
    child_c[OFF_FLG +: W_FLG] = FLG_BORN_THIS_TICK;       // reserved bit stays zero
    child_c[OFF_VAR +: W_VAR] = hash_c[7:0];              // identity, stateless
    child_c[OFF_SPN +: W_SPN] = hash_c[13:8];             // spin, stateless
  end

  assign par_ready_o  = (st_q == S_IDLE);
  assign chl_valid_o  = (st_q == S_EMIT) && !cap_full_i;
  assign chl_record_o = child_c;

  wire chl_fire_c = chl_valid_o && chl_ready_i;

  // one place where a group ends, so every exit advances identically
  task automatic end_group;
    begin
      ev_left_q <= ev_rem_c;
      ev_q      <= ev_pick_c;
      st_q      <= (ev_rem_c != 4'd0) ? S_EVAL : S_IDLE;
    end
  endtask

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // "Reset abandons the parent in flight and any children it had queued but
      // not emitted." The counters are diagnostics and are cleared with it.
      st_q                      <= S_IDLE;
      par_q                     <= '0;
      pid_q                     <= '0;
      ev_left_q                 <= 4'd0;
      ev_q                      <= 2'd0;
      n_q                       <= 5'd0;
      idx_q                     <= 5'd0;
      cspc_q                    <= 7'd0;
      tick_q                    <= '0;
      tick_children_q           <= 32'd0;
      children_requested_o      <= 32'd0;
      children_emitted_o        <= 32'd0;
      children_refused_o        <= 32'd0;
      spawn_by_event0_o         <= 32'd0;
      spawn_by_event1_o         <= 32'd0;
      spawn_by_event2_o         <= 32'd0;
      spawn_by_event3_o         <= 32'd0;
      refused_count_gt_max_o    <= 32'd0;
      refused_unknown_species_o <= 32'd0;
      refused_capacity_o        <= 32'd0;
      max_children_in_tick_o    <= 32'd0;
    end else begin
      if (tick_start_i) tick_children_q <= 32'd0;

      case (st_q)
        S_IDLE: begin
          if (par_valid_i) begin
            par_q     <= par_record_i;
            pid_q     <= par_id_i;
            tick_q    <= tick_i;
            ev_left_q <= par_events_i;
            idx_q     <= 5'd0;
            st_q      <= (par_events_i == 4'd0) ? S_IDLE : S_EVAL;
            // ev_q must hold the FIRST fired event before S_EVAL presents it
            if      (par_events_i[0]) ev_q <= 2'd0;
            else if (par_events_i[1]) ev_q <= 2'd1;
            else if (par_events_i[2]) ev_q <= 2'd2;
            else                      ev_q <= 2'd3;
          end
        end

        S_EVAL: begin
          children_requested_o <= children_requested_o + {27'd0, spc_count_i};
          if (refuse_c) begin
            // Refuse the WHOLE group. Nothing is emitted, on purpose: "do not
            // emit the first 16 -- a truncated burst is a different effect".
            children_refused_o <= children_refused_o + {27'd0, spc_count_i};
            if (bad_count_c)   refused_count_gt_max_o    <= refused_count_gt_max_o + 1;
            if (bad_species_c) refused_unknown_species_o <= refused_unknown_species_o + 1;
            end_group();
          end else if (empty_c) begin
            end_group();
          end else begin
            n_q    <= spc_count_i;
            cspc_q <= spc_child_spc_i;
            idx_q  <= 5'd0;
            st_q   <= S_EMIT;
          end
        end

        S_EMIT: begin
          if (cap_full_i) begin
            // Capacity: later children dropped, by INDEX and never by arrival
            // time. Survivors outrank children, so this is the correct loser.
            children_refused_o <= children_refused_o + {27'd0, (n_q - idx_q)};
            refused_capacity_o <= refused_capacity_o + 1;
            end_group();
          end else if (chl_fire_c) begin
            children_emitted_o <= children_emitted_o + 1;
            tick_children_q    <= tick_children_q + 1;
            if (max_children_in_tick_o < tick_children_q + 1)
              max_children_in_tick_o <= tick_children_q + 1;
            case (ev_q)
              2'd0: spawn_by_event0_o <= spawn_by_event0_o + 1;
              2'd1: spawn_by_event1_o <= spawn_by_event1_o + 1;
              2'd2: spawn_by_event2_o <= spawn_by_event2_o + 1;
              default: spawn_by_event3_o <= spawn_by_event3_o + 1;
            endcase
            if (idx_q + 5'd1 >= n_q) end_group();
            else                     idx_q <= idx_q + 5'd1;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

  // The ordering property is structural (one parent, one event, one ascending
  // index at a time), so there is no assertion here pretending to check it.
  // The directed test checks the emitted stream, which is where it is visible.

endmodule

`default_nettype wire
