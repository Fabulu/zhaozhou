// zhao_terrain_compose_seam.sv -- TERRAIN.PAGESTREAM wired to TERRAIN.PATCH.
// Authored 2026-09-07.
//
// ===========================================================================
// WHY THIS FILE EXISTS: A LEAF FIT CANNOT ANSWER THE QUESTION IT RAISED
// ===========================================================================
// zhao_terrain_pagestream's standalone fit came back at 93.91 MHz against a
// 100 MHz product clock -- its `min_fmax_mhz` rule's first firing, on the first
// block that carried it. Splitting all 2,000 summarised paths by where they
// END says the failure is entirely at the boundary:
//
//   366 paths end at an output port    worst -0.648  -> 93.91 MHz
//   1634 paths end inside the design   worst +0.687  -> 107.38 MHz
//   23 negative-slack paths, EVERY ONE of them ending at a port.
//
// The comfortable reading is "virtual-pin artefact, the block is fine", and
// CLAUDE.md is explicit that the explanation which absolves the design is the
// one to check hardest. Checked, it is NOT that simple in either direction:
//
//   * ARGUING IT IS AN ARTEFACT: the port paths carry -6.29 ns of reported
//     clock skew against -0.5 ns internally. That skew is the leaf fit's
//     imaginary clock network to an imaginary pad. It is not real silicon.
//   * ARGUING IT IS NOT: the failing paths carry 4.30 ns of DATA DELAY out of
//     a 10 ns period, because `v_base_o`/`v_scar_o`/`v_bottom_o` are
//     COMBINATIONAL -- `assign v_base_o = sample_of(2'd0)`, a byte-lane read of
//     the staging buffer indexed by `vidx_q`. That was a deliberate choice
//     (`design/fit_targets.yml` gates it with `max_m10k: 0`, because a memory
//     here would cost a cycle per vertex, 1,089 per lattice) and its price is
//     combinational delay EXPORTED ACROSS THE SEAM.
//   * AND THE RECEIVER SPENDS MORE: zhao_terrain_patch puts `fx_add_sat` -- a
//     33-bit saturating add -- plus two 32-bit clamp comparators on those exact
//     inputs before its first flop (patch.sv:299-308). Neither leaf fit sees
//     the other half. PAGESTREAM measures its 4.3 ns terminating at a pad;
//     PATCH measures its adder starting from one.
//
// So the standalone rows are wrong in BOTH directions at once -- the skew is
// pessimistic, the split path is optimistic -- and no amount of re-reading
// either row resolves it. WIRING THE TWO TOGETHER DOES. That is all this file
// is: the vertex seam, made real, so it can be fitted.
//
// ===========================================================================
// THE THREE PLACES THE BLOCKS DO NOT MEET, MARKED `GLUE:` AT THEIR SITES
// ===========================================================================
// Named rather than papered over, exactly as zhao_texture_island_top does.
// Every one is a live item on the owner-ruling list, and every one currently
// exists ONLY in a testbench -- which is the finding.
//
//   1. PLACEMENT. `wx_i`/`wz_i` are the vertex's world position and NOTHING IN
//      fpga/rtl COMPUTES THEM. The composed bench does it with a multiply per
//      vertex, and that multiply lands combinationally on PATCH's coverage
//      comparators. Reproduced here so the fit measures the seam that exists,
//      with the multiply's cost visible rather than assumed away.
//   2. THE DUAL FLAG. `dual_i` is bit 3 of the flags PAGESTREAM carries whole
//      and uninterpreted. Somebody has to interpret it; today it is a bench.
//   3. THE SOURCE-ID NARROWING. PAGESTREAM carries T5's 32-bit `source_id`;
//      PATCH takes 16. Open ruling, and the seam checker missed it for a day
//      because its stemmer gave the two ports different stems.
//
// WHAT THIS IS NOT: it is not the terrain island. COMPCACHE, TESS and NORMALS
// are the rest of the chain and are not here. This measures ONE SEAM, because
// one seam is what the timing evidence pointed at.
// ===========================================================================
`default_nettype none

module zhao_terrain_compose_seam
  import zhao_pkg::*;
#(
    parameter int unsigned SLOTW = 11,
    parameter int unsigned GENW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- PAGESTREAM configuration and job in --------------------------------
    input  var zhao_client_e     cfg_vram_client_i,
    input  var logic [31:0]      cfg_epoch_i,
    input  var logic             j_valid_i,
    output var logic             j_ready_o,
    input  var logic [SLOTW-1:0] j_slot_i,
    input  var logic [GENW-1:0]  j_gen_i,
    input  var logic [31:0]      j_epoch_i,
    input  var logic [31:0]      j_src_id_i,
    input  var logic [15:0]      j_flags_i,

    // ---- MEM.GUARD read client ----------------------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    input  var logic            beat_valid_i,
    input  var logic [63:0]     beat_data_i,
    input  var logic            beat_last_i,

    // ---- GLUE 1's knobs, at the island boundary because placement is not this
    //      pair's to own. Inputs, not localparams, so the multiply cannot be
    //      constant-folded away and report a seam cheaper than the one that
    //      exists.
    input var logic signed [31:0] cfg_x0_i,
    input var logic signed [31:0] cfg_z0_i,
    input var logic signed [31:0] cfg_step_i,

    // ---- PATCH's field list port --------------------------------------------
    input  var logic               list_clear_i,
    input  var logic [15:0]        patch_id_i,
    input  var logic               fld_add_valid_i,
    output var logic               fld_add_ready_o,
    input  var logic signed [31:0] fld_add_x0_i,
    input  var logic signed [31:0] fld_add_z0_i,
    input  var logic signed [31:0] fld_add_x1_i,
    input  var logic signed [31:0] fld_add_z1_i,
    input  var logic [31:0]        fld_add_hash_i,
    input  var logic [15:0]        fld_add_cmd_i,
    output var logic               fld_add_accept_o,
    output var logic               fld_add_reject_o,
    output var logic [4:0]         fields_active_o,
    output var logic [15:0]        trace_patch_id_o,
    output var logic [31:0]        trace_hash_o,
    output var logic [15:0]        trace_cmd_o,
    output var logic [31:0]        programs_rejected_o,

    // ---- PATCH's per-vertex field lanes -------------------------------------
    input  var logic               fld_valid_i,
    output var logic               fld_ready_o,
    input  var logic signed [31:0] fld_height_i,
    output var logic               fld_covers_o,

    // ---- composed patch_state out -------------------------------------------
    output var logic               st_valid_o,
    input  var logic               st_ready_i,
    output var logic signed [31:0] top_o,
    output var logic signed [31:0] bottom_o,
    output var logic signed [31:0] compose_top_o,
    output var logic               st_dirty_o,
    output var logic [15:0]        st_src_id_o,
    output var logic [15:0]        subpatch_dirty_o,

    // ---- PAGESTREAM completion ----------------------------------------------
    output var logic             done_valid_o,
    input  var logic             done_ready_i,
    output var logic [SLOTW-1:0] done_slot_o,
    output var logic [GENW-1:0]  done_gen_o,
    output var logic [31:0]      done_epoch_o,
    output var logic             done_ok_o,
    output var logic [3:0]       done_verdict_o,
    output var logic [31:0]      done_src_id_o,

    // ---- counters, both blocks ----------------------------------------------
    output var logic [31:0] lattices_streamed_o,
    output var logic [31:0] lattices_refused_o,
    output var logic [31:0] vertices_streamed_o,
    output var logic [31:0] bursts_read_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] incomplete_o,
    output var logic [31:0] terrain_samples_evaluated_o,
    output var logic        idle_o
);

  // THE SEAM ITSELF. Every wire below is a real connection between the two
  // blocks -- no LFSR standing in for a neighbour, which is the distinction
  // zhao_prod_top cannot make.
  logic               ps_v_valid, ps_v_ready;
  logic signed [15:0] ps_base, ps_scar, ps_bottom;
  logic [5:0]         ps_vi, ps_vj;
  // The upper half of `ps_src_id` is DELIBERATELY UNREAD, and that is GLUE 3
  // stated as a lint waiver rather than hidden by one: TERRAIN.PAGESTREAM
  // carries T5's 32-bit source_id and TERRAIN.PATCH takes 16, so composing the
  // pair DROPS SIXTEEN BITS OF PROVENANCE. Waived here with its reason so the
  // seam fit can be built, NOT because the narrowing is settled -- it is an
  // open ruling, and a silent `lint_off` would have made it invisible again.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0]        ps_src_id;
  /* verilator lint_on UNUSEDSIGNAL */
  // The identity PAGESTREAM carries that this PAIR has no consumer for. They
  // are read into dead wires rather than left empty so "nothing downstream
  // wants this yet" is written down instead of looking like an oversight --
  // COMPCACHE is the block that wants slot and generation, and it is not here.
  logic [SLOTW-1:0]   ps_slot_unused;
  logic [GENW-1:0]    ps_gen_unused;
  logic [31:0]        ps_epoch_unused;
  logic [15:0]        ps_flags;
  logic               ps_first, ps_last;
  logic               ps_idle, pt_idle;

  // GLUE 2: which bit of the record's flags means DUAL. The bench's constant,
  // reproduced because there is nowhere else it lives.
  localparam int unsigned FLAG_DUAL_BIT = 3;

  // GLUE 1: placement. wx FOLLOWS vi (the COLUMN) and wz FOLLOWS vj (the ROW);
  // it was the other way round for one run and only a downstream consumer that
  // INTERPRETS the pair could see it. The multiply is on the vertex's own
  // cycle, so it is part of the seam being measured, not a bench convenience --
  // and if it is what costs the clock, an accumulator is the answer and this
  // fit is what says so.
  logic signed [31:0] wx_c, wz_c;
  //
  // `$signed(...)` AND NOT `signed'(...)`, WHICH IS NOT PEDANTRY. The composed
  // bench writes the cast form; Quartus 17.0.2 has already killed two fits of
  // this lane on cast syntax it does not accept (QUARTUS_GOTCHAS 4b), 57 s and
  // 30 s in. The bench form is never synthesised, so it never had to survive
  // this. Same value, same width: the concatenation is exactly 32 bits.
  assign wx_c = cfg_x0_i + (cfg_step_i * $signed({26'd0, ps_vi}));
  assign wz_c = cfg_z0_i + (cfg_step_i * $signed({26'd0, ps_vj}));

  assign idle_o = ps_idle && pt_idle;

  zhao_terrain_pagestream #(
      .SLOTW(SLOTW),
      .GENW (GENW)
  ) u_pagestream (
      .clk              (clk),
      .rst_n            (rst_n),
      .cfg_vram_client_i(cfg_vram_client_i),
      .cfg_epoch_i      (cfg_epoch_i),
      .j_valid_i        (j_valid_i),
      .j_ready_o        (j_ready_o),
      .j_slot_i         (j_slot_i),
      .j_gen_i          (j_gen_i),
      .j_epoch_i        (j_epoch_i),
      .j_src_id_i       (j_src_id_i),
      .j_flags_i        (j_flags_i),
      .guard_req_o      (guard_req_o),
      .guard_rsp_i      (guard_rsp_i),
      .beat_valid_i     (beat_valid_i),
      .beat_data_i      (beat_data_i),
      .beat_last_i      (beat_last_i),
      .v_valid_o        (ps_v_valid),
      .v_ready_i        (ps_v_ready),
      .v_base_o         (ps_base),
      .v_scar_o         (ps_scar),
      .v_bottom_o       (ps_bottom),
      .v_vi_o           (ps_vi),
      .v_vj_o           (ps_vj),
      .v_first_o        (ps_first),
      .v_last_o         (ps_last),
      .v_slot_o         (ps_slot_unused),
      .v_gen_o          (ps_gen_unused),
      .v_epoch_o        (ps_epoch_unused),
      .v_src_id_o       (ps_src_id),
      .v_flags_o        (ps_flags),
      .done_valid_o     (done_valid_o),
      .done_ready_i     (done_ready_i),
      .done_slot_o      (done_slot_o),
      .done_gen_o       (done_gen_o),
      .done_epoch_o     (done_epoch_o),
      .done_ok_o        (done_ok_o),
      .done_verdict_o   (done_verdict_o),
      .done_src_id_o    (done_src_id_o),
      .lattices_streamed_o(lattices_streamed_o),
      .lattices_refused_o (lattices_refused_o),
      .vertices_streamed_o(vertices_streamed_o),
      .bursts_read_o      (bursts_read_o),
      .guard_denied_o     (guard_denied_o),
      .incomplete_o       (incomplete_o),
      .idle_o             (ps_idle)
  );

  zhao_terrain_patch u_patch (
      .clk             (clk),
      .rst_n           (rst_n),
      .list_clear_i    (list_clear_i),
      .patch_id_i      (patch_id_i),
      .fld_add_valid_i (fld_add_valid_i),
      .fld_add_ready_o (fld_add_ready_o),
      .fld_add_x0_i    (fld_add_x0_i),
      .fld_add_z0_i    (fld_add_z0_i),
      .fld_add_x1_i    (fld_add_x1_i),
      .fld_add_z1_i    (fld_add_z1_i),
      .fld_add_hash_i  (fld_add_hash_i),
      .fld_add_cmd_i   (fld_add_cmd_i),
      .fld_add_accept_o(fld_add_accept_o),
      .fld_add_reject_o(fld_add_reject_o),
      .fields_active_o (fields_active_o),
      .trace_patch_id_o(trace_patch_id_o),
      .trace_hash_o    (trace_hash_o),
      .trace_cmd_o     (trace_cmd_o),
      .programs_rejected_o(programs_rejected_o),

      .vtx_valid_i(ps_v_valid),
      .vtx_ready_o(ps_v_ready),
      .base_i     (ps_base),
      .scar_i     (ps_scar),
      .bottom_i   (ps_bottom),
      .dual_i     (ps_flags[FLAG_DUAL_BIT]),
      .wx_i       (wx_c),
      .wz_i       (wz_c),
      .vi_i       (ps_vi),
      .vj_i       (ps_vj),
      // GLUE 3: the 32 -> 16 narrowing, written as an explicit slice so it is
      // visible in the source rather than happening silently at the port. An
      // open ruling; the low half is what the composed bench carried.
      .src_id_i   (ps_src_id[15:0]),

      .fld_valid_i (fld_valid_i),
      .fld_ready_o (fld_ready_o),
      .fld_height_i(fld_height_i),
      .fld_covers_o(fld_covers_o),

      .st_valid_o     (st_valid_o),
      .st_ready_i     (st_ready_i),
      .top_o          (top_o),
      .bottom_o       (bottom_o),
      .compose_top_o  (compose_top_o),
      .st_dirty_o     (st_dirty_o),
      .st_src_id_o    (st_src_id_o),
      .subpatch_dirty_o(subpatch_dirty_o),
      .terrain_samples_evaluated_o(terrain_samples_evaluated_o),
      .idle_o         (pt_idle)
  );

  // `v_first_o`/`v_last_o` have no consumer in this pair -- PATCH is
  // per-vertex and stateless across the lattice. Read into a dead wire rather
  // than left dangling so the intent is stated and lint stays clean.
  logic unused_ok;
  assign unused_ok = ps_first ^ ps_last ^ (^ps_slot_unused) ^ (^ps_gen_unused)
                   ^ (^ps_epoch_unused);

endmodule

`default_nettype wire
