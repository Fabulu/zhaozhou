// zhao_terrain_group_seq_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists only to prove `release_unsafe_o` can fire. The production
// sequencer releases a group after the tess job returns idle, which means its
// final reference has completed every view's shell handshake. This renamed
// copy changes one substantive condition:
//
//     StRef && t_done_c  ->  StRef && t_ref_valid_i
//
// It therefore releases on reference presentation before the ModeRef job has
// drained. The inverted-polarity mutant test deliberately uses one view, so
// the presented triple can complete its entire fan-out in that cycle: the
// detector must still see that later triples remain. The test passes only when
// the synthesizable counter becomes nonzero. The module is renamed and
// lives under tests/ so it cannot enter a production closure by accident.
// Regenerate this copy whenever the production sequencer changes shape.

// zhao_terrain_group_seq.sv — the terrain GROUP sequencer: one subpatch job in,
// one arena GROUP per view out -- allocated, opened, filled through the shared
// projector, sealed, replayed, released -- with the tessellator presented the
// job once in ModeVtx and once in ModeRef.
//
// Law (in citation order):
//   fpga/rtl/terrain/zhao_terrain_tess.sv — the producer: `job_mode_i` (1 =
//       the 81 window vertices, VIEW-INDEPENDENT; 2 = index triples, one
//       triangle per clock), law 7 (a rejected job is rejected in every mode;
//       the reject arrives at the FIRST presentation), and the exit rule: the
//       block returns to StIdle only when every output register has DRAINED,
//       so `job_ready_o` rising after a job means every vertex/reference of
//       that job has been ACCEPTED by this block.
//   fpga/rtl/geometry/zhao_vertex_arena.sv — the primitive: a lookup is
//       answered from the metadata of the cycle it is PRESENTED, the reply is
//       registered at that edge (READ-OLD on a same-address write), an
//       unsealed or re-generated arena REFUSES deterministically. Dense seal:
//       refused unless the fill count is exactly DEPTH.
//   fpga/rtl/terrain/zhao_terrain_wcache.sv — the shell: three copies, one
//       fill broadcast, one reference in -> one triangle out; it holds
//       PAYLOADS in its skid, never handles.
//   fpga/rtl/common/zhao_project_service.sv — client B: `b_ready_o` is the
//       arbiter's grant, combinational from `b_valid_i`; the result lands 36
//       cycles later with the rider intact.
//   reports/PROJECTION-ADOPTION-20260910.md §7 items 3, 4, 6; §1 ARENAS.
//   reports/TERRAIN-TESS-VERTEX-MODE-20260910.md §5, §11 — "present each job
//       in mode 1 once (view-independent) and mode 2 once, honour job_reject_o
//       at the first presentation, and either present the 81 vertices to
//       client B once per view or open one arena per view."
//
// ENFORCED-BY: tests/terrain/terrain_pipe_differential.cpp:main
//
// ---------------------------------------------------------------------------
// THE SHAPE: ONE TESSELLATION, V FILLS, V REPLAYS
// ---------------------------------------------------------------------------
// The tessellation is world coordinates and knows nothing of views; only the
// projection is per view. So a job with view_mask = 2'b11 is tessellated ONCE
// in ModeVtx and each vertex is presented to client B TWICE -- view 0 into
// arena a0 with rider {a0, k}, then view 1 into arena a1 with {a1, k} -- from
// a one-slot fan-out with no buffer: the tess's vertex is held (vtx_ready low)
// until its last view is accepted. Client B is one vertex per clock and the
// fill phase is bounded below by 81 x V projections whatever the buffering,
// so a buffer would buy nothing here; it would buy overlap of the NEXT job's
// tessellation, which needs a second tess instance and is priced, not built
// (report §5).
//
// The references are view-independent too. The job is presented ONCE in
// ModeRef and every triple is fanned out to the V arenas with each slot's
// {arena, gen, view} -- two shell clocks per triangle for a dual-view job,
// which is exactly the shell's output rate (one triangle per clock) at two
// views. The riders `src_id`, `mat_a/b`, `weight` are the job's; `view` is
// the slot's.
//
// ---------------------------------------------------------------------------
// THE LIFETIME OF A GROUP, AND WHY THE RELEASE IS PROVABLE
// ---------------------------------------------------------------------------
//   ALLOC   pick the lowest arena with held[a] == 0; open it (gen = open_gen_i
//           sampled on the open edge); held[a] <= 1; landed[a] <= 0.
//   FILL    ModeVtx job; fan each vertex out to the V slots; count ACCEPTS per
//           slot (`sent`) and LANDINGS per arena (`landed`, from the result
//           port's rider).
//   SEAL    only when landed[a_v] == sent[v] for the slot -- the core is 36
//           cycles deep and a dense seal on the accept count would be REFUSED
//           as short and stick on arena_seal_short_o. The ModeRef job is
//           presented to the tess meanwhile (its cell-state scan runs); no
//           reference is forwarded until every slot is sealed.
//   REPLAY  fan each triple out to the V slots; the shell's credit is honoured.
//   RELEASE held[a_v] <= 0 for every slot, in the cycle the tess reports
//           `job_ready_o` after the ModeRef job.
//
// THE PROOF that a released arena has no live reader:
//   (1) The tess leaves StIdle only for our job and returns to it only when
//       its reference register has drained (`!r_valid` in its exit condition),
//       i.e. after THIS block accepted its last triple -- and this block
//       accepts a triple from the tess only in the cycle the LAST slot's
//       reference is accepted by the shell (`t_ref_ready_o = ref_take &&
//       last_slot`). So at release, every reference of every slot has been
//       ACCEPTED by the shell.
//   (2) An accepted lookup is answered from the metadata of ITS cycle, and its
//       payload is captured into the primitive's reply register at that edge
//       (READ-OLD). Nothing that happens in a later cycle -- an open, a fill
//       for the new generation -- can change either. The shell's skid holds
//       the REPLY (payloads), not the handle.
//   (3) Therefore an open on the cycle after release (the earliest possible)
//       cannot disturb any reference of the released group. There is nothing
//       to reference-count. `release_unsafe_o` compares the release event
//       against independently driven obligations: a successful fill still
//       needs replay, and a ModeRef job stays live until the tess confirms that
//       every triple has drained. The committed mutant tests/mutants/zhao_terrain_group_seq_mutant.sv
//       releases on reference PRESENTATION instead of job completion and an
//       inverted-polarity test proves this detector can fire.
//   A rejected or EMPTY job (a legacy page's underside emits nothing in every
//   mode) never issued a reference; its arenas are released unsealed. An
//   unsealed arena refuses every lookup, so a stale handle to it is refused,
//   not misread; and its next open bumps the generation as usual.
//
// The brief this was built to said "the arena's reference counter exists
// precisely so a group cannot be reopened under a live reader". It does not
// exist: zhao_terrain_wcache's header says "It does not reference-count
// groups" and zhao_terrain_topo's says "HOLD, NOT A REFERENCE COUNT". The
// counter was the zhao_proj_arena3 design study's, deliberately not carried,
// because the shell's consumer receives payloads and retains no handle.
// The release rule above is what stands in its place, and (1)-(3) is why it
// needs no counter.
//
// ---------------------------------------------------------------------------
// SPARSE FILL -- the §3 remedy, as one AND gate
// ---------------------------------------------------------------------------
// `sparse_fill_i` = 1 drops every vertex the tess flags `vtx_stride_o = 0` (a
// filler no triangle of the job references, law 6) instead of projecting it:
// 81/25/9/4 fills per level instead of 81. LEGAL ONLY WITH A VALID_MODE = 0
// (bitmap) SHELL: a dense shell refuses the seal as short, sticky, and the
// group's replay is then refused -- counted, never silent, and the positive
// control in the differential does exactly that on purpose. This block
// cannot see the shell's mode; the composition that instantiates both is
// where the two must agree (zhao_terrain_pipe ties the knob to its own
// VALID_MODE unless told otherwise).
//
// ---------------------------------------------------------------------------
// WHAT IS NOT HERE, named
// ---------------------------------------------------------------------------
//   * No second tess instance and no vertex buffer: the fill of job N+1
//     cannot overlap the replay of job N through one tess. With one instance
//     ARENAS = 2 is sufficient (two slots, released before the next alloc);
//     ARENAS = 4 is what the two-instance pipeline would need. Both fit the
//     same six M10K per copy (324 or 162 rows of 512), so the larger is kept.
//   * No normals leg. ModeVtx/ModeRef carry no world triangle; TERRAIN.NORMALS
//     consumes one. Options are priced in the composition report; none is
//     taken here.
//   * No per-slot seal acknowledgement: the shell has no seal-accept port. A
//     refused seal shows up as replay refusals, which this block does not
//     read; the counters do.
//
// Conservative SystemVerilog subset only (charter §2); explicit generate is
// not needed (no generate blocks); the $fatal guards live in `initial begin`
// (Quartus 17). `--lint-only` does not run them.
`default_nettype none

module zhao_terrain_group_seq_mutant #(
    parameter int unsigned ARENAS  = 4,
    parameter int unsigned DEPTH   = 81,
    parameter int unsigned GEN_W   = 8,
    parameter int unsigned IDX_W   = 7,                   // the tess's window index width
    parameter int unsigned INDEX_W = $clog2(DEPTH) + 1,   // the shell's (carries a refusal bit)
    parameter int unsigned ARENA_W = $clog2(ARENAS) + 1
) (
    input  wire clk,
    input  wire rst_n,

    // ---- one subpatch job: the tess's job fields + the sequencing riders ----
    input  wire        job_valid_i,
    output wire        job_ready_o,
    input  wire [ 5:0] job_ox_i,
    input  wire [ 5:0] job_oz_i,
    input  wire [ 1:0] job_level_i,
    input  wire [ 1:0] job_lvl_nz_i,
    input  wire [ 1:0] job_lvl_pz_i,
    input  wire [ 1:0] job_lvl_nx_i,
    input  wire [ 1:0] job_lvl_px_i,
    input  wire [16:0] job_morph_i,
    input  wire        job_surface_i,
    input  wire        job_dual_i,
    input  wire [15:0] job_src_id_i,
    input  wire [ 1:0] job_view_mask_i,   // bit v = project into view v
    input  wire [ 7:0] job_mat_a_i,
    input  wire [ 7:0] job_mat_b_i,
    input  wire [ 7:0] job_weight_i,

    input  wire        sparse_fill_i,     // drop fillers (VALID_MODE = 0 shells only)

    // ---- the tessellator's job port -------------------------------------------
    output wire        t_job_valid_o,
    input  wire        t_job_ready_i,
    output wire [ 1:0] t_job_mode_o,
    output wire [ 5:0] t_job_ox_o,
    output wire [ 5:0] t_job_oz_o,
    output wire [ 1:0] t_job_level_o,
    output wire [ 1:0] t_job_lvl_nz_o,
    output wire [ 1:0] t_job_lvl_pz_o,
    output wire [ 1:0] t_job_lvl_nx_o,
    output wire [ 1:0] t_job_lvl_px_o,
    output wire [16:0] t_job_morph_o,
    output wire        t_job_surface_o,
    output wire        t_job_dual_o,
    output wire [15:0] t_job_src_id_o,
    input  wire        t_job_reject_i,

    // ---- the tessellator's ModeVtx stream ---------------------------------------
    input  wire               t_vtx_valid_i,
    output wire               t_vtx_ready_o,
    input  wire signed [31:0] t_vtx_x_i,
    input  wire signed [31:0] t_vtx_y_i,
    input  wire signed [31:0] t_vtx_z_i,
    input  wire [IDX_W-1:0]   t_vtx_index_i,
    input  wire               t_vtx_stride_i,

    // ---- the tessellator's ModeRef stream ---------------------------------------
    input  wire               t_ref_valid_i,
    output wire               t_ref_ready_o,
    input  wire [IDX_W-1:0]   t_ref_ia_i,
    input  wire [IDX_W-1:0]   t_ref_ib_i,
    input  wire [IDX_W-1:0]   t_ref_ic_i,

    // ---- client B of the projection service -------------------------------------
    output wire               b_valid_o,
    input  wire               b_ready_i,
    output wire signed [31:0] b_vx_o,
    output wire signed [31:0] b_vy_o,
    output wire signed [31:0] b_vz_o,
    output wire               b_view_o,
    output wire [ARENA_W-1:0] b_arena_o,
    output wire [INDEX_W-1:0] b_index_o,
    input  wire               fill_landed_i,   // the result port's valid
    input  wire [ARENA_W-1:0] fill_arena_i,    // ... and its rider's arena

    // ---- the arena lifetime ------------------------------------------------------
    output wire               open_o,
    output wire [ARENA_W-1:0] open_arena_o,
    input  wire [GEN_W-1:0]   open_gen_i,
    output wire               seal_o,
    output wire [ARENA_W-1:0] seal_arena_o,

    // ---- tagged references to the shell -------------------------------------------
    output wire               r_valid_o,
    input  wire               r_ready_i,
    output wire [ARENA_W-1:0] r_arena_o,
    output wire [GEN_W-1:0]   r_gen_o,
    output wire [INDEX_W-1:0] r_ia_o,
    output wire [INDEX_W-1:0] r_ib_o,
    output wire [INDEX_W-1:0] r_ic_o,
    output wire        [15:0] r_src_id_o,
    output wire               r_view_o,
    output wire        [ 7:0] r_mat_a_o,
    output wire        [ 7:0] r_mat_b_o,
    output wire        [ 7:0] r_weight_o,

    // ---- observation -----------------------------------------------------------------
    output wire [ARENAS-1:0]  held_o,           // arena a is allocated to a live group
    output wire               busy_o,
    output logic [31:0]       jobs_accepted_o,  // jobs taken off the job port
    output logic [31:0]       jobs_no_view_o,   // ... of which view_mask == 0 (dropped)
    output logic [31:0]       jobs_rejected_o,  // the tess rejected the job (law 3)
    output logic [31:0]       jobs_empty_o,     // the tess emitted nothing (legacy underside)
    output logic [31:0]       groups_opened_o,  // arenas opened
    output logic [31:0]       groups_released_o,// arenas released
    output logic [31:0]       fills_forwarded_o,// vertices accepted by client B
    output logic [31:0]       fills_dropped_o,  // fillers dropped under sparse_fill_i
    output logic [31:0]       refs_forwarded_o,// references accepted by the shell
    output logic [31:0]       release_unsafe_o // release while fill replay or ModeRef work remains
);

  localparam int unsigned CNT_W = $clog2(DEPTH + 1);
  localparam int unsigned AW    = $clog2(ARENAS);

  localparam logic [1:0] ModeVtx = 2'd1;
  localparam logic [1:0] ModeRef = 2'd2;

  // Quartus 17 requires these inside `initial begin`; `--lint-only` does not
  // run them (CLAUDE.md, 2026-09-09).
  initial begin
    if (ARENAS < 2)
      $fatal(1, "zhao_terrain_group_seq: ARENAS (%0d) must be >= 2 -- a dual-view job holds two arenas at once",
             ARENAS);
    if (DEPTH != 81)
      $fatal(1, "zhao_terrain_group_seq: DEPTH (%0d) must be the tess's 9x9 window (81) for a dense seal to close",
             DEPTH);
    if (INDEX_W < IDX_W)
      $fatal(1, "zhao_terrain_group_seq: INDEX_W (%0d) cannot carry the tess's IDX_W (%0d) index", INDEX_W, IDX_W);
  end

  // ---- state ---------------------------------------------------------------------
  localparam logic [2:0] StIdle     = 3'd0;
  localparam logic [2:0] StAlloc    = 3'd1;
  localparam logic [2:0] StFill     = 3'd2;
  localparam logic [2:0] StSealWait = 3'd3;
  localparam logic [2:0] StRef      = 3'd4;

  logic [2:0] st;

  // the held job
  logic [ 5:0] j_ox, j_oz;
  logic [ 1:0] j_level, j_nz, j_pz, j_nx, j_px;
  logic [16:0] j_morph;
  logic        j_surface, j_dual;
  logic [15:0] j_src;
  logic [ 7:0] j_mat_a, j_mat_b, j_weight;

  // the slots: slot 0 is the lowest view in the mask, slot 1 the other
  logic               nslots2_q;          // 1 = two slots
  logic               slot_view_q [0:1];
  logic [ARENA_W-1:0] slot_arena_q[0:1];
  logic [GEN_W-1:0]   slot_gen_q  [0:1];
  logic [CNT_W-1:0]   sent_q      [0:1];  // fills accepted on client B, per slot
  logic               sealed_q    [0:1];

  // per arena
  logic [ARENAS-1:0]  held_q;
  logic [CNT_W-1:0]   landed_q[0:ARENAS-1];

  logic alloc_slot_q;    // the slot being allocated / sealed
  logic vs_q;            // the fan-out slot for the current vertex / reference
  logic t_taken_q;       // the tess accepted the current presentation
  logic t_busy_q;        // ... and has not yet returned to idle
  logic rejected_q;
  logic [1:0] mode_q;

  // ---- the free arena, lowest index first ------------------------------------------
  logic          any_free_c;
  logic [AW-1:0] free_c;
  integer fi;
  always_comb begin
    any_free_c = 1'b0;
    free_c     = '0;
    for (fi = ARENAS - 1; fi >= 0; fi = fi - 1) begin
      if (!held_q[fi]) begin
        any_free_c = 1'b1;
        free_c     = AW'(fi);
      end
    end
  end

  // ---- the tess's job port ------------------------------------------------------------
  wire present_c = ((st == StFill) || (st == StSealWait) || (st == StRef)) && !t_taken_q;
  assign t_job_valid_o   = present_c;
  assign t_job_mode_o    = mode_q;
  assign t_job_ox_o      = j_ox;
  assign t_job_oz_o      = j_oz;
  assign t_job_level_o   = j_level;
  assign t_job_lvl_nz_o  = j_nz;
  assign t_job_lvl_pz_o  = j_pz;
  assign t_job_lvl_nx_o  = j_nx;
  assign t_job_lvl_px_o  = j_px;
  assign t_job_morph_o   = j_morph;
  assign t_job_surface_o = j_surface;
  assign t_job_dual_o    = j_dual;
  assign t_job_src_id_o  = j_src;

  wire t_take_c = present_c && t_job_ready_i;
  // The tess has returned to idle after OUR job: every output of that job has
  // been accepted (its exit rule). Never true in the accept cycle itself,
  // because t_busy_q is set on that edge.
  wire t_done_c = t_busy_q && t_job_ready_i;

  // ---- the vertex fan-out (StFill) --------------------------------------------------------
  wire last_slot_c = (vs_q == nslots2_q);
  wire skip_c      = sparse_fill_i && !t_vtx_stride_i;
  wire vtx_here_c  = (st == StFill) && t_vtx_valid_i;

  assign b_valid_o = vtx_here_c && !skip_c;
  assign b_vx_o    = t_vtx_x_i;
  assign b_vy_o    = t_vtx_y_i;
  assign b_vz_o    = t_vtx_z_i;
  assign b_view_o  = slot_view_q[vs_q];
  assign b_arena_o = slot_arena_q[vs_q];
  assign b_index_o = INDEX_W'(t_vtx_index_i);

  wire vtx_take_c = b_valid_o && b_ready_i;
  wire vtx_drop_c = vtx_here_c && skip_c;
  assign t_vtx_ready_o = vtx_drop_c || (vtx_take_c && last_slot_c);

  // ---- the reference fan-out (StRef, only once every slot is sealed) ---------------------
  wire all_sealed_c = sealed_q[0] && (sealed_q[1] || !nslots2_q);
  assign r_valid_o  = (st == StRef) && t_ref_valid_i && all_sealed_c;
  assign r_arena_o  = slot_arena_q[vs_q];
  assign r_gen_o    = slot_gen_q[vs_q];
  assign r_ia_o     = INDEX_W'(t_ref_ia_i);
  assign r_ib_o     = INDEX_W'(t_ref_ib_i);
  assign r_ic_o     = INDEX_W'(t_ref_ic_i);
  assign r_src_id_o = j_src;
  assign r_view_o   = slot_view_q[vs_q];
  assign r_mat_a_o  = j_mat_a;
  assign r_mat_b_o  = j_mat_b;
  assign r_weight_o = j_weight;

  wire ref_take_c = r_valid_o && r_ready_i;
  assign t_ref_ready_o = ref_take_c && last_slot_c;

  // ---- open / seal ---------------------------------------------------------------------------
  wire open_c = (st == StAlloc) && any_free_c;
  assign open_o       = open_c;
  assign open_arena_o = ARENA_W'(free_c);

  wire [ARENA_W-1:0] seal_arena_c  = slot_arena_q[alloc_slot_q];
  wire               seal_landed_c = (landed_q[seal_arena_c[AW-1:0]] == sent_q[alloc_slot_q]);
  wire               seal_c        = (st == StSealWait) && !sealed_q[alloc_slot_q] && seal_landed_c;
  assign seal_o       = seal_c;
  assign seal_arena_o = seal_arena_c;

  assign job_ready_o = (st == StIdle);
  assign busy_o      = (st != StIdle);
  assign held_o      = held_q;

  // ---- counters --------------------------------------------------------------------------------
  function automatic logic [31:0] sat_inc(input logic [31:0] a);
    sat_inc = (a == 32'hFFFF_FFFF) ? a : a + 32'd1;
  endfunction

  // ---- the machine ------------------------------------------------------------------------------
  wire fill_ok_c = (st == StFill) && t_done_c && !rejected_q && !t_job_reject_i && (sent_q[0] != '0);
  wire release_c = ((st == StFill) && t_done_c && !fill_ok_c) || // rejected or empty
                   ((st == StRef)  && t_ref_valid_i);             // MUTANT: release on presentation
  // These obligations are driven independently from release_c: a successful
  // ModeVtx pass still needs seal+replay, and a ModeRef job remains live until
  // the tessellator reports that every triple has drained. Watching the whole
  // job, rather than only the currently presented triple, also detects an
  // erroneous release on the last-slot handshake of a non-final triple. The
  // committed mutant changes release_c alone, so this comparison is not
  // corrupted in lockstep.
  wire group_needs_replay_c = (st == StFill) && fill_ok_c;
  wire ref_job_inflight_c   = (st == StRef) && !t_done_c;
  wire release_unsafe_c     = release_c && (group_needs_replay_c || ref_job_inflight_c);

  integer ai;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st           <= StIdle;
      j_ox         <= '0;
      j_oz         <= '0;
      j_level      <= '0;
      j_nz         <= '0;
      j_pz         <= '0;
      j_nx         <= '0;
      j_px         <= '0;
      j_morph      <= '0;
      j_surface    <= 1'b0;
      j_dual       <= 1'b0;
      j_src        <= '0;
      j_mat_a      <= '0;
      j_mat_b      <= '0;
      j_weight     <= '0;
      nslots2_q    <= 1'b0;
      slot_view_q[0]  <= 1'b0;
      slot_view_q[1]  <= 1'b0;
      slot_arena_q[0] <= '0;
      slot_arena_q[1] <= '0;
      slot_gen_q[0]   <= '0;
      slot_gen_q[1]   <= '0;
      sent_q[0]       <= '0;
      sent_q[1]       <= '0;
      sealed_q[0]     <= 1'b0;
      sealed_q[1]     <= 1'b0;
      held_q       <= '0;
      for (ai = 0; ai < ARENAS; ai = ai + 1) landed_q[ai] <= '0;
      alloc_slot_q <= 1'b0;
      vs_q         <= 1'b0;
      t_taken_q    <= 1'b0;
      t_busy_q     <= 1'b0;
      rejected_q   <= 1'b0;
      mode_q       <= ModeVtx;
      jobs_accepted_o   <= '0;
      jobs_no_view_o    <= '0;
      jobs_rejected_o   <= '0;
      jobs_empty_o      <= '0;
      groups_opened_o   <= '0;
      groups_released_o <= '0;
      fills_forwarded_o <= '0;
      fills_dropped_o   <= '0;
      refs_forwarded_o  <= '0;
      release_unsafe_o  <= '0;
    end else begin
      // ---- landings, every cycle, whichever state -------------------------------
      // A landing for an arena being OPENED this cycle cannot happen (a free
      // arena has no fill in flight), so the open's clear below wins safely.
      // Compare the complete carries-refusal arena field. An invalid code must
      // never alias a legal arena merely because their AW low bits match.
      if (fill_landed_i) begin
        for (ai = 0; ai < ARENAS; ai = ai + 1) begin
          if (fill_arena_i == ARENA_W'(ai))
            landed_q[ai] <= landed_q[ai] + CNT_W'(1);
        end
      end

      // ---- the tess presentation bookkeeping --------------------------------------
      if (t_take_c) begin
        t_taken_q <= 1'b1;
        t_busy_q  <= 1'b1;
      end
      if (t_job_reject_i) rejected_q <= 1'b1;

      // ---- fan-out accounting -----------------------------------------------------------
      if (vtx_take_c) begin
        sent_q[vs_q] <= sent_q[vs_q] + CNT_W'(1);
        fills_forwarded_o <= sat_inc(fills_forwarded_o);
        vs_q <= last_slot_c ? 1'b0 : 1'b1;
      end
      if (vtx_drop_c) fills_dropped_o <= sat_inc(fills_dropped_o);
      if (ref_take_c) begin
        refs_forwarded_o <= sat_inc(refs_forwarded_o);
        vs_q <= last_slot_c ? 1'b0 : 1'b1;
      end

      case (st)
        StIdle: begin
          if (job_valid_i) begin
            jobs_accepted_o <= sat_inc(jobs_accepted_o);
            j_ox      <= job_ox_i;
            j_oz      <= job_oz_i;
            j_level   <= job_level_i;
            j_nz      <= job_lvl_nz_i;
            j_pz      <= job_lvl_pz_i;
            j_nx      <= job_lvl_nx_i;
            j_px      <= job_lvl_px_i;
            j_morph   <= job_morph_i;
            j_surface <= job_surface_i;
            j_dual    <= job_dual_i;
            j_src     <= job_src_id_i;
            j_mat_a   <= job_mat_a_i;
            j_mat_b   <= job_mat_b_i;
            j_weight  <= job_weight_i;
            // slot 0 = the lowest view present; slot 1 = view 1 when both
            nslots2_q      <= (job_view_mask_i == 2'b11);
            slot_view_q[0] <= (job_view_mask_i == 2'b10);
            slot_view_q[1] <= 1'b1;
            sent_q[0]      <= '0;
            sent_q[1]      <= '0;
            sealed_q[0]    <= 1'b0;
            sealed_q[1]    <= 1'b0;
            alloc_slot_q   <= 1'b0;
            vs_q           <= 1'b0;
            t_taken_q      <= 1'b0;
            t_busy_q       <= 1'b0;
            rejected_q     <= 1'b0;
            mode_q         <= ModeVtx;
            if (job_view_mask_i == 2'b00) begin
              jobs_no_view_o <= sat_inc(jobs_no_view_o);
              st <= StIdle;     // consumed, nothing to do
            end else begin
              st <= StAlloc;
            end
          end
        end

        StAlloc: begin
          if (open_c) begin
            held_q[free_c]      <= 1'b1;
            landed_q[free_c]    <= '0;
            slot_arena_q[alloc_slot_q] <= ARENA_W'(free_c);
            slot_gen_q[alloc_slot_q]   <= open_gen_i;
            groups_opened_o     <= sat_inc(groups_opened_o);
            if (alloc_slot_q == nslots2_q) begin
              alloc_slot_q <= 1'b0;
              st <= StFill;   // t_taken_q is 0: the ModeVtx job is presented from here
            end else begin
              alloc_slot_q <= 1'b1;
            end
          end
        end

        StFill: begin
          if (t_done_c) begin
            t_busy_q  <= 1'b0;
            t_taken_q <= 1'b0;
            if (fill_ok_c) begin
              mode_q <= ModeRef;   // present the reference pass while the fills land
              st     <= StSealWait;
            end else begin
              // rejected (law 3/7) or empty (a legacy page's underside): no
              // reference will ever be issued; release the unsealed arenas.
              if (rejected_q || t_job_reject_i) jobs_rejected_o <= sat_inc(jobs_rejected_o);
              else                              jobs_empty_o    <= sat_inc(jobs_empty_o);
              st <= StIdle;
            end
          end
        end

        StSealWait: begin
          // A ModeRef job that emits NO triangle (a level-3 job whose one
          // run-cell covers a void cell -- legal, 6,144 of the probe's jobs)
          // returns the tess to idle while we are still here. `t_busy_q`
          // stays set, so StRef sees `t_done_c` on its first cycle and
          // releases with zero references. Deliberate, not accidental.
          if (seal_c) begin
            sealed_q[alloc_slot_q] <= 1'b1;
            if (alloc_slot_q == nslots2_q) begin
              alloc_slot_q <= 1'b0;
              st <= StRef;
            end else begin
              alloc_slot_q <= 1'b1;
            end
          end
        end

        StRef: begin
          if (t_done_c) begin
            t_busy_q  <= 1'b0;
            t_taken_q <= 1'b0;
            // Law 7 says a job accepted in ModeVtx is never rejected in
            // ModeRef; if it were, nothing was replayed and the group is
            // released like any other. Counted as a rejection, not hidden.
            if (rejected_q || t_job_reject_i) jobs_rejected_o <= sat_inc(jobs_rejected_o);
            st <= StIdle;
          end
        end

        default: st <= StIdle;
      endcase

      // ---- RELEASE: the one place held_q is cleared ------------------------------------
      if (release_c) begin
        held_q[slot_arena_q[0][AW-1:0]] <= 1'b0;
        if (nslots2_q) held_q[slot_arena_q[1][AW-1:0]] <= 1'b0;
        groups_released_o <= nslots2_q ? sat_inc(sat_inc(groups_released_o)) : sat_inc(groups_released_o);
        if (release_unsafe_c) release_unsafe_o <= sat_inc(release_unsafe_o);
      end
    end
  end

endmodule : zhao_terrain_group_seq_mutant

`default_nettype wire
