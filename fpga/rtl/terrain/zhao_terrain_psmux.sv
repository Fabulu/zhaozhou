// zhao_terrain_psmux.sv -- TERRAIN.PAGESTREAM'S TWO-CLIENT SHARE.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS, AND WHAT IT IS NOT
// ---------------------------------------------------------------------------
// Two different jobs in the terrain spine need the SAME page read back out of
// the pool, and they are separate passes over the same bytes:
//
//   * THE COMPOSE PASS. TERRAIN.SEQ issues a patch, TERRAIN.PAGESTREAM streams
//     its 33x33 lattice, TERRAIN.PLACE places it and TERRAIN.PATCH composes it.
//   * THE MIP PASS. TERRAIN.RESIDENCY sets `mips_stale` on EVERY claim, so a
//     loaded page reaches ST_MIPGEN and stops there until a SECOND completion
//     arrives. TERRAIN.MIPFEED produces that completion, and to do it it must
//     stream the page again and hand the heights to TERRAIN.MIPGEN.
//
// `tests/terrain/tb_terrain_world.sv` gives the mip pass ITS OWN streamer and
// says why: "A SECOND PLAYED READ ENGINE, not a shared one ... teaching it to
// arbitrate two would put a scheduler in the bench, and a bench that schedules
// is a bench whose timing is its own invention."  That is the right call for a
// BENCH and the wrong one for the console: a second `zhao_terrain_pagestream`
// is a MEASURED 1,649 ALM (`reports/synthesis/zhao_block_fit.json`, clean tree)
// for a block that is idle most of the time in both roles.  Against an ALM
// budget already breached (47,582 measured, 41,910 budgeted) that is the whole
// point of this file.
//
// SO THIS IS A SCHEDULER, AND IT IS A BLOCK RATHER THAN COMPOSER GLUE for the
// reason `zhao_console_core.sv` gives about every arbiter it refuses to invent:
// arbitration is state, state belongs in a file with a contract and a test, and
// a mux written inline in a composer is an arbiter nobody can point at.  The
// pattern it follows is `zhao_part_project.sv`, which time-multiplexes the
// shared projector's client A rather than adding a second projector.
//
// WHAT IT CONTAINS: one busy bit, one owner bit, one round-robin bit and four
// counters.  NO STORAGE OF THE STREAM.  The job fields are muxed, and the
// vertex and completion streams are DEMUXED BY HANDSHAKE ONLY -- the data wires
// are broadcast, and each client reads them in the cycles its own `valid` is
// high.  Nothing is buffered, nothing is reordered, and nothing is renamed.
//
// ---------------------------------------------------------------------------
// WHY ROUND ROBIN AND NOT PRIORITY
// ---------------------------------------------------------------------------
// Strict priority to the compose door is the tempting choice and it is the
// dangerous one.  The compose door can only issue a patch the directory calls
// RESIDENT; a page only becomes RESIDENT when the MIP pass completes.  So
// starving client B starves the thing client A depends on, and the failure
// would present as A MACHINE THAT STOPS rather than one that is slow -- with
// every handshake legal and every counter balanced, which is this repository's
// named worst case.
//
// The rule is therefore: when both ask, the turn goes to whoever was NOT
// granted last.  `last_q` is one flip-flop and it makes starvation structurally
// impossible in either direction.  When only one asks it takes the streamer
// immediately, so the fair rule costs nothing when there is no contention.
//
// THE GRANT READS THE CLIENTS' `valid` COMBINATIONALLY, which is ordinary for
// an arbiter and is safe here because both clients drive a REGISTERED valid:
// TERRAIN.SEQ holds `is_valid_o` from a state bit until its ready comes, and
// TERRAIN.MIPFEED drives `ps_valid_o` out of its own FSM.  Neither derives
// `valid` from `ready`, so there is no combinational loop across this seam --
// and if one ever appeared, Verilator's UNOPTFLAT would say so rather than the
// design quietly oscillating.
//
// ---------------------------------------------------------------------------
// THE ONE THING A SHARE CAN GET WRONG, AND THE COUNTER THAT WATCHES IT
// ---------------------------------------------------------------------------
// A demux keyed on a captured owner is exactly the shape CLAUDE.md's
// "detector wired to two operands that move together" chapter is about, so the
// detectors here are deliberately NOT differences of two things this block
// latched together.  `stray_v_o` and `stray_done_o` compare the STREAMER'S OWN
// output against this block's `busy_q` -- one side is upstream state, the other
// is ours, and they are clocked by different events (the streamer's internal
// FSM against our job/completion handshakes).  A beat that arrives with no job
// outstanding is a real fault and this is the one place it can be seen.
//
// They read zero under correct upstream behaviour, which makes them a claim.
// They are FIRED BY LEGAL STIMULUS in `tests/terrain/terrain_psmux_directed.cpp`
// case 5, by asserting `p_v_valid_i` / `p_done_valid_i` with no job granted --
// legal at this module's port, because these are inputs and a bench owns them.
// No mutant is needed: the state is reachable from outside.
//
// `p_v_ready_o` and `p_done_ready_o` are HIGH when idle, deliberately.  A stray
// beat is consumed and counted rather than left to wedge the streamer for ever,
// because a silent stall is worse evidence than a counted anomaly.
//
// Conservative SystemVerilog subset only (charter S2); no package deps.

module zhao_terrain_psmux #(
    // The streamer's slot width.  Both clients present a slot of this width;
    // any narrowing or widening is the COMPOSER's, made at its own seam and
    // annotated there, because a width step hidden inside an arbiter is a
    // width step nobody can find.
    parameter int unsigned SLOTW = 11,
    parameter int unsigned GENW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // -----------------------------------------------------------------------
    // CLIENT A -- the compose pass (TERRAIN.SEQ's issue door)
    // -----------------------------------------------------------------------
    input  var logic             a_j_valid_i,
    output var logic             a_j_ready_o,
    input  var logic [SLOTW-1:0] a_j_slot_i,
    input  var logic [GENW-1:0]  a_j_gen_i,
    input  var logic [31:0]      a_j_epoch_i,
    input  var logic [31:0]      a_j_src_id_i,
    input  var logic [15:0]      a_j_flags_i,

    output var logic             a_v_valid_o,
    input  var logic             a_v_ready_i,
    output var logic             a_done_valid_o,
    input  var logic             a_done_ready_i,

    // -----------------------------------------------------------------------
    // CLIENT B -- the mip pass (TERRAIN.MIPFEED)
    // -----------------------------------------------------------------------
    input  var logic             b_j_valid_i,
    output var logic             b_j_ready_o,
    input  var logic [SLOTW-1:0] b_j_slot_i,
    input  var logic [GENW-1:0]  b_j_gen_i,
    input  var logic [31:0]      b_j_epoch_i,
    input  var logic [31:0]      b_j_src_id_i,
    input  var logic [15:0]      b_j_flags_i,

    output var logic             b_v_valid_o,
    input  var logic             b_v_ready_i,
    output var logic             b_done_valid_o,
    input  var logic             b_done_ready_i,

    // -----------------------------------------------------------------------
    // THE SHARED TERRAIN.PAGESTREAM
    // -----------------------------------------------------------------------
    output var logic             p_j_valid_o,
    input  var logic             p_j_ready_i,
    output var logic [SLOTW-1:0] p_j_slot_o,
    output var logic [GENW-1:0]  p_j_gen_o,
    output var logic [31:0]      p_j_epoch_o,
    output var logic [31:0]      p_j_src_id_o,
    output var logic [15:0]      p_j_flags_o,

    input  var logic             p_v_valid_i,
    output var logic             p_v_ready_o,
    input  var logic             p_done_valid_i,
    output var logic             p_done_ready_o,

    // -----------------------------------------------------------------------
    // evidence (spec/counters.md S4: saturate, never wrap)
    // -----------------------------------------------------------------------
    output var logic        busy_o,
    output var logic        owner_o,       // 0 = A, 1 = B; read while busy_o
    output var logic [31:0] a_jobs_o,
    output var logic [31:0] b_jobs_o,
    output var logic [31:0] stray_v_o,
    output var logic [31:0] stray_done_o
);

  logic busy_q;
  logic owner_q;
  logic last_q;    // who took the streamer last; 0 = A, 1 = B

  // ---- the grant ----------------------------------------------------------
  // While busy the selection is the CAPTURED owner and nothing may change it.
  // While idle: both asking -> the turn goes to !last_q; otherwise whoever asks.
  logic sel_b_c;
  logic grant_c;

  always_comb begin
    if (busy_q) begin
      sel_b_c = owner_q;
    end else if (a_j_valid_i && b_j_valid_i) begin
      sel_b_c = !last_q;
    end else begin
      sel_b_c = b_j_valid_i;
    end
    grant_c = !busy_q && (sel_b_c ? b_j_valid_i : a_j_valid_i);
  end

  assign p_j_valid_o  = grant_c;
  assign p_j_slot_o   = sel_b_c ? b_j_slot_i   : a_j_slot_i;
  assign p_j_gen_o    = sel_b_c ? b_j_gen_i    : a_j_gen_i;
  assign p_j_epoch_o  = sel_b_c ? b_j_epoch_i  : a_j_epoch_i;
  assign p_j_src_id_o = sel_b_c ? b_j_src_id_i : a_j_src_id_i;
  assign p_j_flags_o  = sel_b_c ? b_j_flags_i  : a_j_flags_i;

  assign a_j_ready_o = grant_c && !sel_b_c && p_j_ready_i;
  assign b_j_ready_o = grant_c &&  sel_b_c && p_j_ready_i;

  // ---- the vertex stream, demuxed by the captured owner -------------------
  assign a_v_valid_o = p_v_valid_i && busy_q && !owner_q;
  assign b_v_valid_o = p_v_valid_i && busy_q &&  owner_q;
  assign p_v_ready_o = busy_q ? (owner_q ? b_v_ready_i : a_v_ready_i) : 1'b1;

  // ---- the completion, demuxed the same way -------------------------------
  assign a_done_valid_o = p_done_valid_i && busy_q && !owner_q;
  assign b_done_valid_o = p_done_valid_i && busy_q &&  owner_q;
  assign p_done_ready_o = busy_q ? (owner_q ? b_done_ready_i : a_done_ready_i) : 1'b1;

  assign busy_o  = busy_q;
  assign owner_o = owner_q;

  function automatic logic [31:0] sat_inc(input logic [31:0] a);
    sat_inc = (a == 32'hFFFF_FFFF) ? a : a + 32'd1;
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_q       <= 1'b0;
      owner_q      <= 1'b0;
      last_q       <= 1'b0;
      a_jobs_o     <= 32'd0;
      b_jobs_o     <= 32'd0;
      stray_v_o    <= 32'd0;
      stray_done_o <= 32'd0;
    end else begin
      // A grant and a completion can never be in the same cycle: the grant
      // requires !busy_q and the completion requires busy_q.  The ordering
      // below is therefore documentation, not a priority.
      if (grant_c && p_j_ready_i) begin
        busy_q  <= 1'b1;
        owner_q <= sel_b_c;
        last_q  <= sel_b_c;
        if (sel_b_c) b_jobs_o <= sat_inc(b_jobs_o);
        else         a_jobs_o <= sat_inc(a_jobs_o);
      end else if (busy_q && p_done_valid_i && p_done_ready_o) begin
        busy_q <= 1'b0;
      end

      // COUNTED ON THE HANDSHAKE, NOT ON THE LEVEL.  `p_v_valid_i` and
      // `p_done_valid_i` are held until their ready comes, so counting the
      // level would count CYCLES OF WAITING and read six orders of magnitude
      // high -- the exact defect `zhao_console_core.sv` records at
      // `terr_pl_slot_overflow_o`.  Both readies are high while idle, so each
      // stray beat is consumed once and counted once.
      if (p_v_valid_i && p_v_ready_o && !busy_q)
        stray_v_o <= sat_inc(stray_v_o);
      if (p_done_valid_i && p_done_ready_o && !busy_q)
        stray_done_o <= sat_inc(stray_done_o);
    end
  end

endmodule : zhao_terrain_psmux
