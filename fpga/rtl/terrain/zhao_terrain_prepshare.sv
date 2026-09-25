// zhao_terrain_prepshare.sv -- THE TWO PORTS THE PREPARE PASS BORROWS FROM THE
// EMIT SPINE, arbitrated at TRANSACTION granularity.
//
// Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt sections 2 and 7
//      design/contracts/TERRAIN.EDGERECON.md, "WHAT P4 STILL OWES" item 7
//      reports/OWNER-RULINGS-BUILDABILITY-20260902.md T3 (the background class)
//
// ===========================================================================
// THE DECISION THIS BLOCK IS
// ===========================================================================
// QUESTION. `zhao_terrain_prepwalk` masters two ports that already have an
// owner in the composed console: `zhao_terrain_devstore`'s SINGLE read port
// (owned by `zhao_terrain_spdesc`, the EMIT-side assembler) and
// `zhao_terrain_residency_v2`'s SINGLE lookup port (owned by
// `zhao_terrain_seq`, the pager). The two passes are NOT disjoint in time --
// the compose spine serves patches on its own schedule and a PREPARE walk can
// be running while the cache serves -- so "PREPARE first, then EMIT" is not a
// structural guarantee and must not be assumed.
//
// CHOSEN OPTION. One named share block holding two independent
// transaction-granular arbiters, neither of which can starve either requester.
// NOT composer wires and NOT a mux on a phase bit: a mux that changed owner
// mid-transaction would hand one requester's beats to the other with every
// counter balancing, which is this repository's documented record-swapping
// shape.
//
// REASON / ALTERNATIVES.
//   * A second devstore read port: doubles the block's read FSM and its SDRAM
//     socket, against a directive that says to SHARE an existing route.
//   * Muxing on the walker's `busy_o`: that level moves inside a transaction.
//   * Two separate share files: the two arbiters have one owner, one law and
//     one reason to exist. Splitting them would put PREPARE's access policy in
//     two places.
//
// CONSTRAINTS / COST. Two owner registers, two selectors, nine counters.
// 0 DSP, 0 M10K, no memory port of its own.
//
// CONSEQUENCES. `zhao_terrain_devstore`, `zhao_terrain_residency_v2`,
// `zhao_terrain_spdesc`, `zhao_terrain_seq` and `zhao_terrain_prepwalk` are
// all UNCHANGED -- not one port moves. Only `zhao_console_core.sv` rewires.
//
// ===========================================================================
// SECTION 7, AND THE CLIENT THAT IS NOT SPENT
// ===========================================================================
// The directive: "Prefer sharing an appropriate existing guaranteed read route
// or using the currently reserved client slot IF THE LIVE DESIGN STILL HAS IT
// AVAILABLE. Do not spend a specific numeric client ID based on an old
// comment."
//
// IT DOES NOT HAVE IT AVAILABLE, re-measured in this packet rather than
// inherited: `zhao_vram_arbiter.sv:353` forces `port_grant[5]` low, `:246`
// records that it appears in no selector arm, and `zhao_mem_guard`'s `default`
// arm grants it nothing. So the shared route is taken, and it is taken in the
// cheapest possible way: **PREPARE's devstore reads ARE devstore's OWN reads.**
// They go out on requester 4 of `u_build_share` under
// ZHAO_CLIENT_TERRAIN_BUILD, on the identical socket, with the identical
// scoped guard permission, because they are literally the same port. PREPARE
// therefore spends NO new memory client, needs NO change to
// `zhao_vram_arbiter` or `zhao_mem_guard`, and
// `tests/formal/mem_guard_no_escape.sby` does not move.
//
// What PREPARE does spend is TIME on that port, and the blocked-clock counters
// below are the instrument for it -- `spec/memory_rules.md:396-402` named the
// unit ("the record READ is frame-critical ... a frame whose page loads
// saturate the socket therefore DELAYS LOD decisions rather than corrupting
// them") and `zhao_terrain_prepwalk`'s own `store_wait_clocks_o` is the other
// half. Do not quote a burst count for this question.
//
// ===========================================================================
// WHY THE PAYLOAD IS NOT ROUTED THROUGH THIS BLOCK
// ===========================================================================
// `r_sp_o`, `r_dev1_o`..`r_fresh_o` are combinational outputs of
// `zhao_terrain_devstore` that are meaningful ONLY while `r_valid_o` is high,
// and the same is true of the directory's `lu_hit_o`/`lu_slot_o`/`lu_gen_o`
// against `lu_valid_o`. This block routes the QUALIFYING BITS; the composer
// wires the payload to both requesters directly. That is a deliberate and
// stated choice rather than an omission: passing twenty fields through a
// register-free passthrough would add a place for an adapter to hide, and the
// bit that can be wrong is the one routed here where it can be reasoned about.
// `d_r_sp_i` IS taken, because this block needs it to know where a devstore
// transaction ends.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_terrain_prepshare #(
    parameter int unsigned SLOTW = 10,
    parameter int unsigned CW    = 32,
    // The last subpatch index of a devstore read transaction. A parameter so
    // the elaboration check below has something to check, NOT a knob: sixteen
    // subpatches is TERRAIN.SPDESC's and TERRAIN.DEVSTORE's shared law.
    parameter int unsigned LAST_SP = 15
) (
    input var logic clk,
    input var logic rst_n,

    // =======================================================================
    // ARBITER 1 -- `zhao_terrain_devstore`'s READ port
    // =======================================================================
    // Requester A: the EMIT assembler (`zhao_terrain_spdesc`).
    input  var logic             a_r_start_i,
    input  var logic [SLOTW-1:0] a_r_slot_i,
    output var logic             a_r_ready_o,
    output var logic             a_r_valid_o,
    input  var logic             a_r_ready_i,

    // Requester B: the PREPARE walker (`zhao_terrain_prepwalk`).
    input  var logic             b_r_start_i,
    input  var logic [SLOTW-1:0] b_r_slot_i,
    output var logic             b_r_ready_o,
    output var logic             b_r_valid_o,
    input  var logic             b_r_ready_i,

    // Downstream: the store itself.
    output var logic             d_r_start_o,
    output var logic [SLOTW-1:0] d_r_slot_o,
    input  var logic             d_r_ready_i,   // devstore `r_ready_o`: R_IDLE
    input  var logic             d_r_valid_i,   // devstore `r_valid_o`: R_STREAM
    output var logic             d_r_ready_o,
    input  var logic [3:0]       d_r_sp_i,      // devstore `r_sp_o`, 0..15

    // =======================================================================
    // ARBITER 2 -- `zhao_terrain_residency_v2`'s LOOKUP port
    // =======================================================================
    // Requester A: the pager (`zhao_terrain_seq`).
    input  var logic               a_lu_valid_i,
    output var logic               a_lu_ready_o,
    input  var logic [31:0]        a_lu_epoch_i,
    input  var logic [31:0]        a_lu_island_i,
    input  var logic signed [15:0] a_lu_ix_i,
    input  var logic signed [15:0] a_lu_iz_i,
    output var logic               a_lu_ans_valid_o,

    // Requester B: the PREPARE walker.
    input  var logic               b_lu_valid_i,
    output var logic               b_lu_ready_o,
    input  var logic [31:0]        b_lu_epoch_i,
    input  var logic [31:0]        b_lu_island_i,
    input  var logic signed [15:0] b_lu_ix_i,
    input  var logic signed [15:0] b_lu_iz_i,
    output var logic               b_lu_ans_valid_o,

    // Downstream: the directory.
    output var logic               d_lu_valid_o,
    input  var logic               d_lu_ready_i,
    output var logic [31:0]        d_lu_epoch_o,
    output var logic [31:0]        d_lu_island_o,
    output var logic signed [15:0] d_lu_ix_o,
    output var logic signed [15:0] d_lu_iz_o,
    input  var logic               d_lu_ans_valid_i,

    // =======================================================================
    // counters
    // =======================================================================
    output var logic [CW-1:0] a_dev_grants_o,
    output var logic [CW-1:0] b_dev_grants_o,
    // CLOCKS, and the names say so because the unit differs from the grants
    // above. THIS IS THE COST OF SHARING, CHARGED TO BOTH SIDES:
    //   * `a_dev_blocked_clocks_o` -- cycles the EMIT assembler held `r_start`
    //     and was not offered the port. Directive section 7 says to charge the
    //     complete cost, and this is the frame-critical half of it.
    //   * `b_dev_blocked_clocks_o` -- cycles PREPARE held the offer and the
    //     port was unavailable.
    //
    // AND NOTE WHY THE SYMMETRIC FORM IS NOT USED FOR B.
    // `zhao_terrain_prepwalk:538` drives
    // `r_start_o = (state_q == P_START) && r_ready_i`, so its start is HIGH
    // ONLY WHEN IT IS ALREADY READY. A counter written as
    // `b_r_start_i && !b_r_ready_o` is therefore exactly CLAUDE.md's detector
    // wired to two operands that move together: structurally incapable of
    // firing, reading zero for ever, and looking like the right instrument.
    // The offer bit and the port's availability come from DIFFERENT state, so
    // the form actually used below can fire -- and is shown firing.
    output var logic [CW-1:0] a_dev_blocked_clocks_o,
    output var logic [CW-1:0] b_dev_blocked_clocks_o,
    output var logic [CW-1:0] a_lu_grants_o,
    output var logic [CW-1:0] b_lu_grants_o,
    output var logic [CW-1:0] a_lu_blocked_clocks_o,
    output var logic [CW-1:0] b_lu_blocked_clocks_o,
    // THE DETECTOR, and it differences two quantities loaded by two DIFFERENT
    // enables. The owner register is written on the GRANT handshake
    // (`d_lu_valid_o && d_lu_ready_i`); the answer arrives on the directory's
    // own pipeline (`d_lu_ans_valid_i`), later and on a different enable
    // entirely. An answer with no owner therefore means the directory answered
    // something this block did not ask, or answered twice -- the one fault a
    // lookup arbiter can commit, and the one that would otherwise hand PREPARE
    // the pager's slot.
    output var logic [CW-1:0] lu_ans_unowned_o,
    // BOTH devstore requesters asking in one cycle. Not a fault -- it is the
    // arbiter doing its job -- and counted because a run in which it never
    // moves never exercised the contention this block exists for, so quoting
    // the blocked-clock counters' silence would be quoting an untested
    // arbiter. It is this block's POSITIVE CONTROL.
    output var logic [CW-1:0] dev_contended_o,

    output var logic busy_o
);

  localparam logic [1:0] OWN_NONE = 2'd0;
  localparam logic [1:0] OWN_A    = 2'd1;
  localparam logic [1:0] OWN_B    = 2'd2;

  // =========================================================================
  // ARBITER 1 -- the devstore read port. A ROTATING OFFER, NOT A PRIORITY.
  // =========================================================================
  // THE OFFER IS A REGISTER AND THAT IS FORCED, NOT STYLISTIC.
  // `zhao_terrain_prepwalk` gates its own `r_start_o` on `r_ready_i`, so an
  // arbiter whose READY depends on that requester's START is a COMBINATIONAL
  // LOOP. Verilator calls it UNOPTFLAT and Quartus infers a latch; both are
  // right. So the offer is HELD in `dev_sel_q`, rotates on every idle cycle,
  // and neither ready looks at either start.
  //
  // The cost is at most ONE idle clock of hand-off latency; the benefit is
  // that neither requester can be starved -- directive section 7's "give other
  // admitted clients bounded service", made structural rather than argued.
  logic [1:0] dev_own_q;
  logic       dev_sel_q;   // 0 = the offer is A's, 1 = the offer is B's

  wire dev_free_c = (dev_own_q == OWN_NONE) && d_r_ready_i;

  assign a_r_ready_o = dev_free_c && !dev_sel_q;
  assign b_r_ready_o = dev_free_c &&  dev_sel_q;

  wire dev_take_a_c = a_r_ready_o && a_r_start_i;
  wire dev_take_b_c = b_r_ready_o && b_r_start_i;

  assign d_r_start_o = dev_take_a_c || dev_take_b_c;
  assign d_r_slot_o  = dev_sel_q ? b_r_slot_i : a_r_slot_i;

  assign a_r_valid_o = d_r_valid_i && (dev_own_q == OWN_A);
  assign b_r_valid_o = d_r_valid_i && (dev_own_q == OWN_B);
  assign d_r_ready_o = (dev_own_q == OWN_A) ? a_r_ready_i :
                       (dev_own_q == OWN_B) ? b_r_ready_i : 1'b0;

  wire dev_last_beat_c = d_r_valid_i && d_r_ready_o && (d_r_sp_i == 4'(LAST_SP));

  // =========================================================================
  // ARBITER 2 -- the residency lookup port.
  // =========================================================================
  // Both requesters here drive an UNGATED valid -- `zhao_terrain_seq:345`
  // `lu_valid_o = (st == S_LOOKUP)` and `zhao_terrain_prepwalk:541`
  // `lu_valid_o = (state_q == P_LOOKUP)` -- so this arbiter may look at the
  // requests without closing a loop, and is written as a preference with a
  // round-robin tie-break rather than a rotating offer. The asymmetry with
  // arbiter 1 is driven by the requesters' shapes, not by taste; BOTH forms
  // are recorded so that the next reader does not "tidy" one into the other
  // and reintroduce the loop.
  logic [1:0] lu_own_q;
  logic       lu_last_a_q;

  wire lu_free_c   = (lu_own_q == OWN_NONE);
  wire lu_pick_b_c = b_lu_valid_i && (!a_lu_valid_i || lu_last_a_q);
  wire lu_sel_b_c  = lu_free_c && lu_pick_b_c;
  wire lu_sel_a_c  = lu_free_c && a_lu_valid_i && !lu_pick_b_c;

  assign d_lu_valid_o  = lu_sel_a_c || lu_sel_b_c;
  assign d_lu_epoch_o  = lu_sel_b_c ? b_lu_epoch_i  : a_lu_epoch_i;
  assign d_lu_island_o = lu_sel_b_c ? b_lu_island_i : a_lu_island_i;
  assign d_lu_ix_o     = lu_sel_b_c ? b_lu_ix_i     : a_lu_ix_i;
  assign d_lu_iz_o     = lu_sel_b_c ? b_lu_iz_i     : a_lu_iz_i;
  assign a_lu_ready_o  = lu_sel_a_c && d_lu_ready_i;
  assign b_lu_ready_o  = lu_sel_b_c && d_lu_ready_i;

  assign a_lu_ans_valid_o = d_lu_ans_valid_i && (lu_own_q == OWN_A);
  assign b_lu_ans_valid_o = d_lu_ans_valid_i && (lu_own_q == OWN_B);

  assign busy_o = (dev_own_q != OWN_NONE) || (lu_own_q != OWN_NONE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dev_own_q              <= OWN_NONE;
      dev_sel_q              <= 1'b0;
      lu_own_q               <= OWN_NONE;
      lu_last_a_q            <= 1'b0;
      a_dev_grants_o         <= '0;
      b_dev_grants_o         <= '0;
      a_dev_blocked_clocks_o <= '0;
      b_dev_blocked_clocks_o <= '0;
      a_lu_grants_o          <= '0;
      b_lu_grants_o          <= '0;
      a_lu_blocked_clocks_o  <= '0;
      b_lu_blocked_clocks_o  <= '0;
      lu_ans_unowned_o       <= '0;
      dev_contended_o        <= '0;
    end else begin
      // ---- devstore ----
      if (dev_take_a_c) begin
        dev_own_q <= OWN_A;
        dev_sel_q <= 1'b1;               // the other side gets the next offer
        if (a_dev_grants_o != {CW{1'b1}}) a_dev_grants_o <= a_dev_grants_o + CW'(1);
      end else if (dev_take_b_c) begin
        dev_own_q <= OWN_B;
        dev_sel_q <= 1'b0;
        if (b_dev_grants_o != {CW{1'b1}}) b_dev_grants_o <= b_dev_grants_o + CW'(1);
      end else begin
        if (dev_last_beat_c) dev_own_q <= OWN_NONE;
        // ROTATE THE OFFER while nobody is using it. This is what makes the
        // scheme starvation-free without either ready reading either start.
        if (dev_free_c) dev_sel_q <= !dev_sel_q;
      end

      if (a_r_start_i && !a_r_ready_o) begin
        if (a_dev_blocked_clocks_o != {CW{1'b1}})
          a_dev_blocked_clocks_o <= a_dev_blocked_clocks_o + CW'(1);
      end
      if (dev_sel_q && !dev_free_c) begin
        if (b_dev_blocked_clocks_o != {CW{1'b1}})
          b_dev_blocked_clocks_o <= b_dev_blocked_clocks_o + CW'(1);
      end
      if (a_r_start_i && b_r_start_i) begin
        if (dev_contended_o != {CW{1'b1}}) dev_contended_o <= dev_contended_o + CW'(1);
      end

      // ---- residency lookup ----
      if (lu_sel_a_c && d_lu_ready_i) begin
        lu_own_q    <= OWN_A;
        lu_last_a_q <= 1'b1;
        if (a_lu_grants_o != {CW{1'b1}}) a_lu_grants_o <= a_lu_grants_o + CW'(1);
      end else if (lu_sel_b_c && d_lu_ready_i) begin
        lu_own_q    <= OWN_B;
        lu_last_a_q <= 1'b0;
        if (b_lu_grants_o != {CW{1'b1}}) b_lu_grants_o <= b_lu_grants_o + CW'(1);
      end else if (d_lu_ans_valid_i) begin
        lu_own_q <= OWN_NONE;
      end

      if (a_lu_valid_i && !a_lu_ready_o) begin
        if (a_lu_blocked_clocks_o != {CW{1'b1}})
          a_lu_blocked_clocks_o <= a_lu_blocked_clocks_o + CW'(1);
      end
      if (b_lu_valid_i && !b_lu_ready_o) begin
        if (b_lu_blocked_clocks_o != {CW{1'b1}})
          b_lu_blocked_clocks_o <= b_lu_blocked_clocks_o + CW'(1);
      end
      if (d_lu_ans_valid_i && (lu_own_q == OWN_NONE)) begin
        if (lu_ans_unowned_o != {CW{1'b1}}) lu_ans_unowned_o <= lu_ans_unowned_o + CW'(1);
      end
    end
  end

  // Elaboration guards. `initial begin ... end` and NOT a module-scope `if`:
  // Quartus 17.0 rejects the latter. `--lint-only` does not run these.
  // synthesis translate_off
  initial begin
    if (LAST_SP != 15)
      $fatal(1, "zhao_terrain_prepshare: LAST_SP=%0d; TERRAIN.DEVSTORE streams exactly 16 records and this arbiter ends a transaction on the last one", LAST_SP);
    if (SLOTW == 0)
      $fatal(1, "zhao_terrain_prepshare: SLOTW must be positive");
    if (CW < 8)
      $fatal(1, "zhao_terrain_prepshare: CW=%0d is too narrow to be a census", CW);
  end
  // synthesis translate_on

endmodule

`default_nettype wire
