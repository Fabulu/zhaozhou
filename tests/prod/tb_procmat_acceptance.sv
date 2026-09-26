// tb_procmat_acceptance.sv -- the arrangement owner completion ruling 2's
// acceptance test names, composed from the PRODUCTION modules, fed by REAL
// DrawProcedural BYTES and a REAL forge page in a memory model.
//
// ENFORCED-BY: tests/prod/procmat_acceptance.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THE RULING ASKED FOR, VERBATIM
// ---------------------------------------------------------------------------
//   "Acceptance: two records in one set, the same record ID in two sets, upper
//    handle-index bits, generation/residency cases governed by the existing
//    handle law, backpressure and alternating procedural draws. Compare against
//    the reference and ASSERT THAT THE CORRECT MATERIAL ACTUALLY REACHES THE
//    RASTER PATH."
//
// and, in the completion-report section:
//
//   "An otherwise green smoke whose upstream fixture never reaches the new path
//    does not prove the path."
//
// So every draw in `procmat_acceptance.cpp` enters as BYTES of a command
// packet, through the production decoder and executor, and every forge record
// enters as BYTES of a memory image that the production page bank reads through
// its own MEM.GUARD request port. The driver touches the packet stream, the
// memory image, MATERIAL.RESOLVE's answer and GEOM.CLIP's ready -- nothing
// inside.
//
// ---------------------------------------------------------------------------
// IT IS A WRAPPER, NOT A COPY
// ---------------------------------------------------------------------------
// Nine production modules are INSTANTIATED here, so this file cannot drift from
// them: a port either binds or the elaboration fails.
//
//   zhao_cmd_decoder      zhao_forge_prim_eval    zhao_geom_clipdoor
//   zhao_cmd_exec         zhao_forge_ring_eval    zhao_material_window
//   zhao_forge_pagebank   zhao_forge_assemble     zhao_forge_prim
//
// `tools/budget/mutant_copy_drift.py` excludes wrappers for exactly this
// reason. What this file adds is the FORK, a MEM.GUARD responder over a byte
// array, a FAKE PROJECTOR and R197's four-line gate, and nothing else.
//
// THE THREE THINGS THAT ARE COPIES, named so a reader knows where to look:
//
//   1. THE FORK. `pkt_ready_o` is the AND of both consumers' readies, which is
//      `zhao_console_core.sv`'s own expression. A tap instead of a fork lets one
//      consumer advance past a byte the other never saw, and every counter on
//      both sides still balances (`tb_cmd_exec_pair.sv` says this at length).
//
//   2. R197's UNTEXTURED GATE, four lines, because it lives in
//      `zhao_console_core`'s BODY rather than in a module of its own:
//
//          wire cl_in_untex_c  = cd_o_untex;
//          wire cl_in_refuse_c = cl_in_untex_c && (mw_pub_sample_count != 2'd0);
//          assign cl_in_valid  = mw_t_valid && !cl_in_refuse_c;
//          assign mw_t_ready   = cl_in_refuse_c || cl_in_ready;
//
//      (`zhao_console_core.sv`, the UNTEXTURED DOOR block. Search for
//      `cl_in_refuse_c`; there is exactly one definition.) It matters here
//      because a forge primitive DECLARES untextured by law, so this gate is
//      what a sampling material meets.
//
//   3. THE POSITION MUX between the two evaluators, which is a plain mux in the
//      composer because only one of them can ever be running.
//
// ---------------------------------------------------------------------------
// WHAT IS NOT HERE, AND WHY -- STATED RATHER THAN LEFT TO BE DISCOVERED
// ---------------------------------------------------------------------------
// * THE PROJECTOR IS FAKE. `zhao_part_project` is a three-owner front mux whose
//   other two clients are absent, so instantiating it here would measure an
//   arbitration this bench does not have. The fake is the one
//   `tb_forge_assemble.sv` uses -- a LAT-deep shift register with a result that
//   is a FUNCTION of the vertex, so a broken join cannot pass -- and the
//   geometry it produces is not what this bench asserts about.
//
// * THE DOOR HAS ONE CLIENT. `zhao_geom_clipdoor` is instantiated with
//   NCLIENT=1 rather than the console's three. GEOM.REPLAY and PART.CLIPFEED
//   are other subsystems' producers and `tb_partmat_acceptance.sv` already
//   arbitrates all three. What is asserted here is the FORGE arm's pair
//   surviving the door, not the arbitration.
//
// * MEM.UPLOAD's PUBLICATION IS THE DRIVER'S. `pub_valid_i` is a real port of
//   the page bank and in the console it comes from `u_mem_upload`. Here the
//   driver pulses it, because this bench is about what a published page does
//   and not about how it came to be published.
//
// * MATERIAL.RESOLVE IS THE DRIVER'S. The window's request port is the
//   measurement -- it is where the pair ARRIVES after the whole chain -- so
//   answering it from the driver is how the arrival is read, not a shortcut
//   around a producer. `tb_partmat_acceptance.sv` does the same.

`default_nettype none

module tb_procmat_acceptance #(
    parameter int unsigned ATTRS     = 7,
    parameter int unsigned IDW       = 16,
    // 8 KiB of page memory, byte addressed from zero. A FORGE_PROGRAM page is
    // 64 + 192*count bytes, so this holds up to 42 records -- far past what any
    // case here needs and past MAX_SCAN besides.
    parameter int unsigned MEM_BYTES = 8192,
    parameter int unsigned MAX_VERTS = 520,
    parameter int unsigned INFLIGHT  = 64,
    parameter int unsigned FAKE_LAT  = 36
) (
    input  var logic clk,
    input  var logic rst_n,
    // CMD.DECODER is ONE-SHOT: its S_DONE holds the verdict until reset. A
    // bench wanting a SECOND packet resets the decoder alone, which is what
    // this extra reset is for.
    input  var logic dec_rst_n,

    // ---- the sealed packet byte stream, as CMD.DMA presents it -------------
    input  var logic        pkt_valid_i,
    output var logic        pkt_ready_o,
    input  var logic [ 7:0] pkt_byte_i,
    input  var logic [31:0] pkt_len_i,
    output var logic        decode_done_o,
    output var logic [ 7:0] decode_error_o,
    output var logic [31:0] decode_commands_o,

    // ---- the page memory image, written by the driver before the run -------
    input  var logic        mem_we_i,
    input  var logic [15:0] mem_addr_i,
    input  var logic [ 7:0] mem_data_i,
    // Hold the MEM.GUARD responder off, so the page read can be back-pressured
    // while a draw is in flight. This is the "backpressure" the ruling names,
    // applied where it is hardest: between the draw's accept and the record.
    input  var logic        mem_stall_i,

    // ---- MEM.UPLOAD's publication (the driver's) ---------------------------
    input  var logic        pub_valid_i,
    input  var logic [ 7:0] pub_tag_i,
    input  var logic [31:0] pub_base_i,
    input  var logic [31:0] pub_extent_i,

    // ---- MATERIAL.RESOLVE, played by the driver ----------------------------
    // THIS IS THE MEASUREMENT. `req_material_set_o` / `req_material_id_o` are
    // the pair the window asks its resolver for, after the draw's bytes have
    // travelled the decoder, the executor, the page bank, the topology and
    // position evaluators, the assembler and the clip door.
    output var logic               req_valid_o,
    input  var logic               req_ready_i,
    output var logic        [31:0] req_material_set_o,
    output var logic        [15:0] req_material_id_o,
    input  var logic               rsp_valid_i,
    output var logic               rsp_ready_o,
    input  var logic               rsp_has_record_i,
    input  var logic        [ 1:0] rsp_sample_count_i,
    input  var logic        [ 2:0] rsp_material_recipe_i,
    input  var logic        [ 7:0] rsp_recipe_weight_i,
    input  var logic        [ 7:0] rsp_base_binding_i,
    input  var logic        [ 7:0] rsp_sample0_modes_i,

    // ---- what reaches GEOM.CLIP's input ------------------------------------
    output var logic               cl_in_valid_o,
    input  var logic               cl_in_ready_i,
    output var logic [IDW-1:0]     cl_src_id_o,
    output var logic               cl_untex_o,
    // The pair ON THE GRANTED BEAT. Read together with `cl_in_valid_o` this is
    // literally "the material that reached the raster path".
    output var logic        [31:0] cl_material_set_o,
    output var logic        [15:0] cl_material_id_o,
    output var logic        [ 1:0] cl_material_mode_o,
    output var logic        [ 1:0] pub_sample_count_o,
    output var logic        [ 1:0] pub_material_mode_o,
    output var logic               pub_valid_o,

    // ---- the span's other two disposal events, the driver's ----------------
    input  var logic               d_reject_i,
    input  var logic               d_leave_i,

    // ---- CMD.EXEC's decoded forge record, OBSERVED ON ITS OWN HANDSHAKE ----
    // Published so the executor's two same-bytes allocations can be asserted
    // from REAL RECORD BYTES rather than argued from the layout:
    //   * `material_id`, the field owner ruling 2 (2026-09-22) added;
    //   * `frame_tick`, R241 D-TICK-A's field, whose BEHAVIOUR that ruling
    //     requires PRESERVED -- so a test that only checked the new field
    //     would not have checked the clause that says the old one still works.
    // Sampled by the driver on `cmd_forge_take_o`, which is the accepted
    // handshake the pair is captured on.
    output var logic        cmd_forge_take_o,
    output var logic [31:0] cmd_forge_program_o,
    output var logic [31:0] cmd_forge_material_o,
    output var logic [15:0] cmd_forge_material_id_o,
    output var logic [15:0] cmd_forge_frame_tick_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] cmd_forges_issued_o,
    output var logic [31:0] cmd_forge_overflow_o,
    output var logic [31:0] cmd_unsupported_o,
    output var logic [31:0] pb_pages_o,
    output var logic [31:0] pb_draws_o,
    output var logic [31:0] pb_lookup_miss_o,
    output var logic [31:0] pb_refused_kind_o,
    output var logic [31:0] pb_refused_nopage_o,
    output var logic [31:0] pb_bad_record_o,
    output var logic [31:0] pb_denied_o,
    output var logic [31:0] asm_jobs_o,
    output var logic [31:0] asm_triangles_o,
    output var logic [31:0] asm_mat_skew_o,
    output var logic [31:0] mw_resolves_o,
    output var logic [31:0] mw_switches_o,
    output var logic [31:0] mw_no_record_o,
    output var logic [31:0] mw_mode_refused_o,
    output var logic [31:0] mw_err_unpublished_o,
    output var logic [31:0] mw_err_underflow_o,
    output var logic [31:0] geom_untex_refused_o,
    // The FORGE arm's own pair, at the assembler's output, before the door.
    // Published so a failing case can say WHERE the pair stopped being right.
    output var logic [31:0] fa_material_set_o,
    output var logic [15:0] fa_material_id_o
);

  import zhao_pkg::*;

  localparam int unsigned AW = ATTRS * 32;
  localparam int unsigned S_INVW = 0, S_UOW = 1, S_VOW = 2;
  localparam int unsigned S_R = 3, S_G = 4, S_B = 5, S_ALPHA = 6;

  // ==========================================================================
  // THE FORK -- `zhao_console_core.sv`'s own expression
  // ==========================================================================
  logic dec_ready, exe_ready;
  assign pkt_ready_o = dec_ready && exe_ready;

  /* verilator lint_off UNUSEDSIGNAL */
  logic        rec_valid_w;
  logic [15:0] rec_opcode_w, rec_bytes_w;
  logic [31:0] rec_src_w, rec_index_w, bytes_consumed_w;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_cmd_decoder u_dec (
      .clk  (clk),
      .rst_n(rst_n && dec_rst_n),
      .pkt_valid_i(pkt_valid_i),
      .pkt_ready_o(dec_ready),
      .pkt_byte_i (pkt_byte_i),
      .pkt_len_i  (pkt_len_i),
      .rec_valid_o    (rec_valid_w),
      .rec_ready_i    (1'b1),
      .rec_opcode_o   (rec_opcode_w),
      .rec_bytes_o    (rec_bytes_w),
      .rec_source_id_o(rec_src_w),
      .rec_index_o    (rec_index_w),
      .decode_done_o   (decode_done_o),
      .decode_error_o  (decode_error_o),
      .bytes_consumed_o(bytes_consumed_w),
      .commands_o      (decode_commands_o)
  );

  // ==========================================================================
  // CMD.EXEC -- every port, by name, through `.*`
  // ==========================================================================
  // The include below is `zhao_cmd_exec`'s port DECLARATION list. It was
  // written for `tb_zhao_twod_chain.sv` and it is NOT TWOD-SPECIFIC: it is that
  // block's whole port list minus the handful a harness drives itself. Sharing
  // it is deliberate -- a second copy is a copy, and a copy of a port list goes
  // stale silently, which is the failure this repository has a chapter about.
  // A port added to CMD.EXEC appears here as an ELABORATION ERROR rather than
  // as a PINMISSING reading zero.
  logic        pkt_fork_ready_i;
  assign pkt_fork_ready_i = pkt_ready_o;
  logic verdict_valid_i;
  logic [7:0] verdict_error_i;
  assign verdict_valid_i = decode_done_o;
  assign verdict_error_i = decode_error_o;

  // UNUSEDSIGNAL is suppressed ACROSS THE INCLUDE AND NOWHERE ELSE. The file
  // is CMD.EXEC's whole port list -- ~200 nets -- and this harness consumes one
  // arm of it, so every other net is legitimately unread. Silencing it here
  // rather than globally keeps the warning live on THIS file's own signals,
  // which is where `zhao_forge_pagebank`'s header says an unread record field
  // was once caught.
  /* verilator lint_off UNUSEDSIGNAL */
`include "tb_zhao_twod_chain_exec_nets.svh"
  /* verilator lint_on UNUSEDSIGNAL */

  // The consumers CMD.EXEC has in the console and this bench does not. Every
  // one is a READY and every one is high, EXCEPT `forge_ready_i`, which is the
  // page bank's own -- that is the whole point of this harness.
  assign proj_cfg_ready_i = 1'b1;
  assign stamp_ready_i    = 1'b1;
  assign draw_ready_i     = 1'b1;
  assign upl_ready_i      = 1'b1;
  // TERRAINMAT, 2026-09-26: `zhao_cmd_exec` gained two SetEnvironment
  // outputs for terrain's material identity. This bench binds it with
  // `.*`, which needs a declared net per port -- an implicit one is NOT
  // created for a `.*` connection, which is how this file failed to
  // elaborate the moment the ports landed. Declared and unread here: this
  // bench is about the 2-D compositor chain and has no terrain arm.
  logic [31:0] env_terr_mat_set_o;
  logic [15:0] env_terr_mat_id_o;
  assign env_ready_i      = 1'b1;
  assign pop_ready_i      = 1'b1;
  assign tfld_ready_i     = 1'b1;
  assign post_idle_i      = 1'b1;
  // The TWOD consumers CMD.EXEC also has. High for the same reason: a stalled
  // descriptor port would back-pressure the packet walk and make a forge result
  // depend on a block that is not in this harness.
  assign tpl_ready_i      = 1'b1;
  assign tsp_ready_i      = 1'b1;
  assign tld_ready_i      = 1'b1;

  assign cmd_forge_take_o        = forge_valid_o && forge_ready_i;
  assign cmd_forge_program_o     = forge_program_o;
  assign cmd_forge_material_o    = forge_material_o;
  assign cmd_forge_material_id_o = forge_material_id_o;
  assign cmd_forge_frame_tick_o  = forge_frame_tick_o;

  assign cmd_forges_issued_o  = forges_issued_o;
  assign cmd_forge_overflow_o = forge_overflow_o;
  assign cmd_unsupported_o    = unsupported_o;

  zhao_cmd_exec u_exec (.pkt_ready_o (exe_ready), .*);

  // ==========================================================================
  // THE MEM.GUARD RESPONDER over a byte array
  //
  // MEM.GUARD's protocol is TWO CYCLES, LEVEL THEN PULSE: `rsp.ready` is a
  // level the cycle the request is offered and `rsp.ok` a PULSE one cycle
  // later. `zhao_forge_pagebank` has a separate state for each and its header
  // says a block testing both on one cycle "would read every pass as a
  // denial", so this responder must get it right or nothing reads at all.
  // ==========================================================================
  zhao_guard_req_t fpb_req;
  zhao_guard_rsp_t fpb_rsp;
  logic        fpb_beat_valid;
  logic [63:0] fpb_beat_data;

  localparam int unsigned MEMW = $clog2(MEM_BYTES);
  logic [7:0] mem_q [0:MEM_BYTES-1];
  logic [1:0] rstate_q;
  logic [2:0] rbeat_q;
  logic [15:0] raddr_q;

  localparam logic [1:0] R_IDLE = 2'd0, R_OK = 2'd1, R_BEATS = 2'd2;

  always_comb begin
    fpb_rsp           = '0;
    fpb_rsp.ready     = (rstate_q == R_IDLE) && fpb_req.valid && !mem_stall_i;
    fpb_rsp.ok        = (rstate_q == R_OK);
    fpb_rsp.violation = 1'b0;
  end

  // LSB-first, which is the assembly `zhao_forge_pagebank` performs:
  // `line_q <= {g_beat_data_i, line_q[511:64]}`.
  logic [15:0] beat_base_c;
  assign beat_base_c = raddr_q + {10'd0, rbeat_q, 3'd0};
  always_comb begin
    fpb_beat_data = 64'd0;
    for (int b = 0; b < 8; b++) begin
      fpb_beat_data[b*8 +: 8] = mem_q[MEMW'(beat_base_c + 16'(b))];
    end
  end
  assign fpb_beat_valid = (rstate_q == R_BEATS);

  integer mi;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rstate_q <= R_IDLE;
      rbeat_q  <= 3'd0;
      raddr_q  <= 16'd0;
      for (mi = 0; mi < MEM_BYTES; mi = mi + 1) mem_q[mi] <= 8'd0;
    end else begin
      if (mem_we_i) mem_q[MEMW'(mem_addr_i)] <= mem_data_i;
      unique case (rstate_q)
        R_IDLE: if (fpb_req.valid && !mem_stall_i) begin
          raddr_q  <= fpb_req.addr[15:0];
          rbeat_q  <= 3'd0;
          rstate_q <= R_OK;
        end
        R_OK: rstate_q <= R_BEATS;
        R_BEATS: begin
          rbeat_q <= rbeat_q + 3'd1;
          if (rbeat_q == 3'd7) rstate_q <= R_IDLE;
        end
        default: rstate_q <= R_IDLE;
      endcase
    end
  end

  // ==========================================================================
  // THE FORGE CHAIN -- the composer's own wiring
  // ==========================================================================
  logic         fpb_p_valid, fpb_p_ready;
  logic [ 2:0]  fpb_p_family;
  logic [ 6:0]  fpb_p_segments;
  logic [ 3:0]  fpb_p_sides;
  logic [15:0]  fpb_p_material;
  logic [ 1:0]  fpb_p_view_mask;
  logic [15:0]  fpb_p_src_id;
  logic [31:0]  fpb_material_set;
  logic [15:0]  fpb_material_id;
  logic         fpb_a_valid, fpb_a_ready;

  logic         fpb_e_valid, fpb_e_ready;
  logic signed [31:0] fpb_e_sx, fpb_e_sy, fpb_e_sz, fpb_e_ex, fpb_e_ey, fpb_e_ez;
  logic signed [31:0] fpb_e_p1x, fpb_e_p1y, fpb_e_p1z;
  logic signed [31:0] fpb_e_p2x, fpb_e_p2y, fpb_e_p2z;
  logic signed [31:0] fpb_e_wx, fpb_e_wy, fpb_e_wz;
  logic signed [31:0] fpb_e_hw, fpb_e_bhw, fpb_e_amp, fpb_e_bamp;
  logic [31:0]  fpb_e_seed;
  logic [15:0]  fpb_e_phase;
  logic [ 6:0]  fpb_e_segments;
  logic [ 1:0]  fpb_e_brcount;
  logic [ 6:0]  fpb_e_b0att, fpb_e_b1att;
  logic [ 3:0]  fpb_e_b0seg, fpb_e_b1seg;
  logic signed [31:0] fpb_e_b0x, fpb_e_b0y, fpb_e_b0z;
  logic signed [31:0] fpb_e_b1x, fpb_e_b1y, fpb_e_b1z;
  logic [ 1:0]  fpb_e_vmask;
  logic [15:0]  fpb_e_src_id;

  logic         fpb_r_valid, fpb_r_ready;
  logic [ 2:0]  fpb_r_family;
  logic         fpb_r_sweep;
  logic [ 6:0]  fpb_r_segments;
  logic [ 3:0]  fpb_r_sides;
  logic signed [31:0] fpb_r_a0x, fpb_r_a0y, fpb_r_a0z;
  logic signed [31:0] fpb_r_a1x, fpb_r_a1y, fpb_r_a1z;
  logic signed [31:0] fpb_r_ux, fpb_r_uy, fpb_r_uz;
  logic signed [31:0] fpb_r_vx, fpb_r_vy, fpb_r_vz;
  logic signed [31:0] fpb_r_r0, fpb_r_r1;
  logic [ 1:0]  fpb_r_vmask;
  logic [15:0]  fpb_r_src_id;

  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] pb_truncated, pb_bad_magic, pb_page_overflow, pb_refused_cliff;
  logic        pb_busy;
  logic [31:0] fp_jobs, fp_tris, fp_ref_family, fp_ref_limit, fp_skip_view;
  logic [31:0] fe_jobs, fe_points, fe_verts, fe_ref_limit, fe_skip_view,
               fe_sat, fe_overrun;
  logic [31:0] fr_jobs, fr_rings, fr_verts, fr_ref_family, fr_ref_else,
               fr_ref_limit, fr_skip_view, fr_sat;
  logic [31:0] fa_vertices, fa_index_oor, fa_vtx_ovf, fa_slot_press,
               fa_dq_refused, fa_dq_stray, fa_proj_stray;
  logic [31:0] cd_granted, cd_switches, cd_idle_offered, cd_err_hold;
  logic        cd_owner;   // NCLIENT=1, so the door's owner field is one bit
  logic [31:0] mw_drain_stall, mw_answer_stall, mw_occ_max, mw_sel_ovf,
               mw_clut_unowned, mw_no_material_spans;
  logic [ 1:0] fe_v_poly;
  logic [15:0] fe_v_src_id, fr_v_src_id;
  logic [ 6:0] fr_v_ring;
  logic [ 4:0] fr_v_k;
  logic [ 2:0] cd_o_behind;
  logic [ 1:0] cd_o_cull_mode;
  logic [AW-1:0] cd_o_attr_a, cd_o_attr_b, cd_o_attr_c;
  logic signed [20:0] cd_o_ax, cd_o_ay, cd_o_bx, cd_o_by, cd_o_cx, cd_o_cy;
  logic [ 7:0] cd_o_quality_tier;
  /* verilator lint_on UNUSEDSIGNAL */

  // Both views asserted, exactly as `zhao_console_core` does it: DrawProcedural
  // carries no viewport_mask, so the PAGE's own mask governs alone.
  localparam logic [1:0] CMD_VIEW_MASK = 2'b11;
  wire [1:0] view_sel_c = 2'b01;   // view 0

  zhao_forge_pagebank #(
    .MAX_SCAN (64)
  ) u_pagebank (
    .clk   (clk),
    .rst_n (rst_n),

    .pub_valid_i (pub_valid_i),
    .pub_tag_i   (pub_tag_i),
    .pub_base_i  (pub_base_i),
    .pub_extent_i(pub_extent_i),

    .g_req_o       (fpb_req),
    .g_rsp_i       (fpb_rsp),
    .g_beat_valid_i(fpb_beat_valid),
    .g_beat_data_i (fpb_beat_data),

    // REAL: CMD.EXEC's ratified DrawProcedural, both halves of the material
    // reference from ONE queue entry.
    .d_valid_i      (forge_valid_o),
    .d_ready_o      (forge_ready_i),
    .d_program_i    (forge_program_o),
    .d_kind_i       (forge_kind_o),
    .d_material_i   (forge_material_o),
    .d_material_id_i(forge_material_id_o),
    .d_frame_tick_i (forge_frame_tick_o),
    .d_view_mask_i  (CMD_VIEW_MASK),
    .d_src_id_i     (forge_src_id_o),

    .asm_busy_i    (fa_busy),

    .p_valid_o    (fpb_p_valid),
    .p_ready_i    (fpb_p_ready),
    .p_family_o   (fpb_p_family),
    .p_segments_o (fpb_p_segments),
    .p_sides_o    (fpb_p_sides),
    .p_material_o (fpb_p_material),
    .p_view_mask_o(fpb_p_view_mask),
    .p_src_id_o   (fpb_p_src_id),
    .p_material_set_o(fpb_material_set),
    .p_material_id_o (fpb_material_id),
    .a_valid_o       (fpb_a_valid),
    .a_ready_i       (fpb_a_ready),

    .e_valid_o (fpb_e_valid),
    .e_ready_i (fpb_e_ready),
    .e_start_x_o(fpb_e_sx), .e_start_y_o(fpb_e_sy), .e_start_z_o(fpb_e_sz),
    .e_end_x_o  (fpb_e_ex), .e_end_y_o  (fpb_e_ey), .e_end_z_o  (fpb_e_ez),
    .e_perp1_x_o(fpb_e_p1x), .e_perp1_y_o(fpb_e_p1y), .e_perp1_z_o(fpb_e_p1z),
    .e_perp2_x_o(fpb_e_p2x), .e_perp2_y_o(fpb_e_p2y), .e_perp2_z_o(fpb_e_p2z),
    .e_waxis_x_o(fpb_e_wx), .e_waxis_y_o(fpb_e_wy), .e_waxis_z_o(fpb_e_wz),
    .e_half_width_o       (fpb_e_hw),
    .e_branch_half_width_o(fpb_e_bhw),
    .e_amp_o              (fpb_e_amp),
    .e_branch_amp_o       (fpb_e_bamp),
    .e_seed_o             (fpb_e_seed),
    .e_tick_phase_o       (fpb_e_phase),
    .e_segments_o         (fpb_e_segments),
    .e_branch_count_o     (fpb_e_brcount),
    .e_br0_attach_o  (fpb_e_b0att), .e_br0_segments_o(fpb_e_b0seg),
    .e_br0_end_x_o   (fpb_e_b0x), .e_br0_end_y_o(fpb_e_b0y), .e_br0_end_z_o(fpb_e_b0z),
    .e_br1_attach_o  (fpb_e_b1att), .e_br1_segments_o(fpb_e_b1seg),
    .e_br1_end_x_o   (fpb_e_b1x), .e_br1_end_y_o(fpb_e_b1y), .e_br1_end_z_o(fpb_e_b1z),
    .e_view_mask_o   (fpb_e_vmask),
    .e_src_id_o      (fpb_e_src_id),

    .r_valid_o   (fpb_r_valid),
    .r_ready_i   (fpb_r_ready),
    .r_family_o  (fpb_r_family),
    .r_sweep_o   (fpb_r_sweep),
    .r_segments_o(fpb_r_segments),
    .r_sides_o   (fpb_r_sides),
    .r_a0_x_o(fpb_r_a0x), .r_a0_y_o(fpb_r_a0y), .r_a0_z_o(fpb_r_a0z),
    .r_a1_x_o(fpb_r_a1x), .r_a1_y_o(fpb_r_a1y), .r_a1_z_o(fpb_r_a1z),
    .r_u_x_o (fpb_r_ux),  .r_u_y_o (fpb_r_uy),  .r_u_z_o (fpb_r_uz),
    .r_v_x_o (fpb_r_vx),  .r_v_y_o (fpb_r_vy),  .r_v_z_o (fpb_r_vz),
    .r_r0_o  (fpb_r_r0),  .r_r1_o  (fpb_r_r1),
    .r_view_mask_o(fpb_r_vmask),
    .r_src_id_o   (fpb_r_src_id),

    .pages_o         (pb_pages_o),
    .draws_o         (pb_draws_o),
    .bad_magic_o     (pb_bad_magic),
    .page_overflow_o (pb_page_overflow),
    .truncated_o     (pb_truncated),
    .lookup_miss_o   (pb_lookup_miss_o),
    .refused_kind_o  (pb_refused_kind_o),
    .refused_cliff_o (pb_refused_cliff),
    .bad_record_o    (pb_bad_record_o),
    .refused_nopage_o(pb_refused_nopage_o),
    .denied_o        (pb_denied_o),
    .busy_o          (pb_busy)
  );

  logic         fp_t_valid, fp_t_ready;
  logic [15:0]  fp_t_i0, fp_t_i1, fp_t_i2, fp_t_material, fp_t_src_id;
  logic         fp_t_last;

  zhao_forge_prim #(
    .MAX_SEGMENTS (64),
    .MAX_SIDES    (8)
  ) u_forge_prim (
    .clk   (clk),
    .rst_n (rst_n),
    .j_valid_i    (fpb_p_valid),
    .j_ready_o    (fpb_p_ready),
    .j_family_i   (fpb_p_family),
    .j_segments_i (fpb_p_segments),
    .j_sides_i    (fpb_p_sides),
    .j_material_i (fpb_p_material),
    .j_view_mask_i(fpb_p_view_mask),
    .j_src_id_i   (fpb_p_src_id),
    .view_sel_i   (view_sel_c),
    .t_valid_o   (fp_t_valid),
    .t_ready_i   (fp_t_ready),
    .t_i0_o      (fp_t_i0),
    .t_i1_o      (fp_t_i1),
    .t_i2_o      (fp_t_i2),
    .t_material_o(fp_t_material),
    .t_src_id_o  (fp_t_src_id),
    .t_last_o    (fp_t_last),
    .jobs_o           (fp_jobs),
    .triangles_o      (fp_tris),
    .refused_family_o (fp_ref_family),
    .refused_limit_o  (fp_ref_limit),
    .skipped_view_o   (fp_skip_view)
  );

  logic         fe_v_valid, fe_v_ready;
  logic signed [31:0] fe_v_x, fe_v_y, fe_v_z;
  logic         fe_v_last;
  logic         fr_v_valid, fr_v_ready;
  logic signed [31:0] fr_v_x, fr_v_y, fr_v_z;
  logic         fr_v_last;

  zhao_forge_prim_eval u_forge_prim_eval (
    .clk   (clk),
    .rst_n (rst_n),
    .j_valid_i  (fpb_e_valid),
    .j_ready_o  (fpb_e_ready),
    .j_start_x_i(fpb_e_sx), .j_start_y_i(fpb_e_sy), .j_start_z_i(fpb_e_sz),
    .j_end_x_i  (fpb_e_ex), .j_end_y_i  (fpb_e_ey), .j_end_z_i  (fpb_e_ez),
    .j_perp1_x_i(fpb_e_p1x), .j_perp1_y_i(fpb_e_p1y), .j_perp1_z_i(fpb_e_p1z),
    .j_perp2_x_i(fpb_e_p2x), .j_perp2_y_i(fpb_e_p2y), .j_perp2_z_i(fpb_e_p2z),
    .j_waxis_x_i(fpb_e_wx), .j_waxis_y_i(fpb_e_wy), .j_waxis_z_i(fpb_e_wz),
    .j_half_width_i       (fpb_e_hw),
    .j_branch_half_width_i(fpb_e_bhw),
    .j_amp_i              (fpb_e_amp),
    .j_branch_amp_i       (fpb_e_bamp),
    .j_seed_i             (fpb_e_seed),
    .j_tick_phase_i       (fpb_e_phase),
    .j_segments_i         (fpb_e_segments),
    .j_branch_count_i     (fpb_e_brcount),
    .j_br0_attach_i  (fpb_e_b0att), .j_br0_segments_i(fpb_e_b0seg),
    .j_br0_end_x_i   (fpb_e_b0x), .j_br0_end_y_i(fpb_e_b0y), .j_br0_end_z_i(fpb_e_b0z),
    .j_br1_attach_i  (fpb_e_b1att), .j_br1_segments_i(fpb_e_b1seg),
    .j_br1_end_x_i   (fpb_e_b1x), .j_br1_end_y_i(fpb_e_b1y), .j_br1_end_z_i(fpb_e_b1z),
    .j_view_mask_i   (fpb_e_vmask),
    .j_src_id_i      (fpb_e_src_id),
    .view_sel_i      (view_sel_c),
    .v_valid_o (fe_v_valid),
    .v_ready_i (fe_v_ready),
    .v_x_o     (fe_v_x),
    .v_y_o     (fe_v_y),
    .v_z_o     (fe_v_z),
    .v_poly_o  (fe_v_poly),
    .v_last_o  (fe_v_last),
    .v_src_id_o(fe_v_src_id),
    .jobs_o          (fe_jobs),
    .points_o        (fe_points),
    .vertices_o      (fe_verts),
    .refused_limit_o (fe_ref_limit),
    .skipped_view_o  (fe_skip_view),
    .sat_events_o    (fe_sat),
    .walk_overrun_o  (fe_overrun)
  );

  zhao_forge_ring_eval #(
    .MAX_SEGMENTS (64),
    .MAX_SIDES    (8)
  ) u_forge_ring_eval (
    .clk   (clk),
    .rst_n (rst_n),
    .j_valid_i   (fpb_r_valid),
    .j_ready_o   (fpb_r_ready),
    .j_family_i  (fpb_r_family),
    .j_sweep_i   (fpb_r_sweep),
    .j_segments_i(fpb_r_segments),
    .j_sides_i   (fpb_r_sides),
    .j_a0_x_i(fpb_r_a0x), .j_a0_y_i(fpb_r_a0y), .j_a0_z_i(fpb_r_a0z),
    .j_a1_x_i(fpb_r_a1x), .j_a1_y_i(fpb_r_a1y), .j_a1_z_i(fpb_r_a1z),
    .j_u_x_i (fpb_r_ux),  .j_u_y_i (fpb_r_uy),  .j_u_z_i (fpb_r_uz),
    .j_v_x_i (fpb_r_vx),  .j_v_y_i (fpb_r_vy),  .j_v_z_i (fpb_r_vz),
    .j_r0_i  (fpb_r_r0),  .j_r1_i  (fpb_r_r1),
    .j_view_mask_i(fpb_r_vmask),
    .j_src_id_i   (fpb_r_src_id),
    .view_sel_i   (view_sel_c),
    .v_valid_o (fr_v_valid),
    .v_ready_i (fr_v_ready),
    .v_x_o     (fr_v_x),
    .v_y_o     (fr_v_y),
    .v_z_o     (fr_v_z),
    .v_ring_o  (fr_v_ring),
    .v_k_o     (fr_v_k),
    .v_last_o  (fr_v_last),
    .v_src_id_o(fr_v_src_id),
    .jobs_o              (fr_jobs),
    .rings_o             (fr_rings),
    .vertices_o          (fr_verts),
    .refused_family_o    (fr_ref_family),
    .refused_elsewhere_o (fr_ref_else),
    .refused_limit_o     (fr_ref_limit),
    .skipped_view_o      (fr_skip_view),
    .sat_events_o        (fr_sat)
  );

  // THE POSITION MUX. A plain mux because only one evaluator can be running:
  // the bank issues to the ribbon evaluator OR the ring evaluator, never both,
  // and holds the next draw until the assembler retires the primitive. The
  // composer's own comment and the composer's own expression.
  wire fa_v_valid = fe_v_valid || fr_v_valid;
  wire fa_from_ring_c = fr_v_valid;
  wire signed [31:0] fa_v_x = fa_from_ring_c ? fr_v_x : fe_v_x;
  wire signed [31:0] fa_v_y = fa_from_ring_c ? fr_v_y : fe_v_y;
  wire signed [31:0] fa_v_z = fa_from_ring_c ? fr_v_z : fe_v_z;
  wire fa_v_last = fa_from_ring_c ? fr_v_last : fe_v_last;
  wire fa_v_ready;
  assign fe_v_ready = fa_v_ready && !fa_from_ring_c;
  assign fr_v_ready = fa_v_ready &&  fa_from_ring_c;

  // ==========================================================================
  // THE FAKE PROJECTOR -- `tb_forge_assemble.sv`'s, and it is a FUNCTION of the
  // vertex on purpose, so a broken join cannot pass by returning a constant.
  // ==========================================================================
  logic               fa_f_valid;
  logic signed [31:0] fa_f_vx, fa_f_vy, fa_f_vz;
  /* verilator lint_off UNUSEDSIGNAL */
  logic               fa_f_view;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [14:0]        fa_f_slot;

  logic               pv_q   [FAKE_LAT];
  logic signed [20:0] px_q   [FAKE_LAT];
  logic signed [20:0] py_q   [FAKE_LAT];
  logic [30:0]        pw_q   [FAKE_LAT];
  logic [14:0]        psl_q  [FAKE_LAT];

  integer fi;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (fi = 0; fi < FAKE_LAT; fi = fi + 1) begin
        pv_q[fi]  <= 1'b0;
        px_q[fi]  <= 21'sd0;
        py_q[fi]  <= 21'sd0;
        pw_q[fi]  <= 31'd0;
        psl_q[fi] <= 15'd0;
      end
    end else begin
      for (fi = FAKE_LAT - 1; fi > 0; fi = fi - 1) begin
        pv_q[fi]  <= pv_q[fi-1];
        px_q[fi]  <= px_q[fi-1];
        py_q[fi]  <= py_q[fi-1];
        pw_q[fi]  <= pw_q[fi-1];
        psl_q[fi] <= psl_q[fi-1];
      end
      pv_q[0]  <= fa_f_valid;
      px_q[0]  <= $signed(fa_f_vx[20:0]);
      py_q[0]  <= $signed(fa_f_vy[20:0]);
      pw_q[0]  <= {15'd0, fa_f_vz[15:0]} + 31'd65536;
      psl_q[0] <= fa_f_slot;
    end
  end

  logic         fa_o_valid, fa_o_ready;
  logic signed [20:0] fa_o_ax, fa_o_ay, fa_o_bx, fa_o_by, fa_o_cx, fa_o_cy;
  logic [ 2:0]  fa_o_behind;
  logic [15:0]  fa_o_src_id;
  logic         fa_o_untex;
  logic [ 1:0]  fa_o_cull_mode;
  logic [AW-1:0] fa_o_attr_a, fa_o_attr_b, fa_o_attr_c;
  logic [ 7:0]  fa_o_quality_tier;
  logic         fa_busy;

  zhao_forge_assemble #(
    .MAX_VERTS (MAX_VERTS),
    .IDW       (16),
    .ATTRS     (ATTRS),
    .INFLIGHT  (INFLIGHT),
    .SLOT_INVW (S_INVW),
    .SLOT_UOW  (S_UOW),
    .SLOT_VOW  (S_VOW),
    .SLOT_R    (S_R),
    .SLOT_G    (S_G),
    .SLOT_B    (S_B),
    .SLOT_ALPHA(S_ALPHA)
  ) u_forge_assemble (
    .clk   (clk),
    .rst_n (rst_n),
    .v_valid_i(fa_v_valid),
    .v_ready_o(fa_v_ready),
    .v_x_i    (fa_v_x),
    .v_y_i    (fa_v_y),
    .v_z_i    (fa_v_z),
    .v_last_i (fa_v_last),
    .t_valid_i   (fp_t_valid),
    .t_ready_o   (fp_t_ready),
    .t_i0_i      (fp_t_i0),
    .t_i1_i      (fp_t_i1),
    .t_i2_i      (fp_t_i2),
    .t_material_i(fp_t_material),
    .t_src_id_i  (fp_t_src_id),
    .t_last_i    (fp_t_last),
    // THE PAIR, both halves of ONE held sideband.
    .j_material_set_i(fpb_material_set),
    .j_material_id_i (fpb_material_id),
    // The per-job DECLARATION (SHADOWRIDE, 2026-09-23). This bench drives
    // FORGE.PRIM's, which is what it has always measured: MATERIAL_BACKED,
    // opaque, and the plain opaque write.
    .j_material_mode_i(FORGE_MATERIAL_MODE),
    .j_vertex_alpha_i (FORGE_VERTEX_ALPHA),
    .j_frag_state_i   (FORGE_FRAG_STATE),
    .j_valid_i       (fpb_a_valid),
    .j_ready_o       (fpb_a_ready),
    // The authored art values, at the console's own named seams.
    .art_r_i           (32'sh0000_C000),
    .art_g_i           (32'sh0000_A000),
    .art_b_i           (32'sh0000_8000),
    .art_alpha_i       (32'sh0001_0000),
    .art_quality_tier_i(8'h40),
    .art_cull_mode_i   (2'd0),
    .f_valid_o  (fa_f_valid),
    .f_ready_i  (1'b1),
    .f_vx_o     (fa_f_vx),
    .f_vy_o     (fa_f_vy),
    .f_vz_o     (fa_f_vz),
    .f_view_o   (fa_f_view),
    .f_slot_o   (fa_f_slot),
    .rs_valid_i (pv_q[FAKE_LAT-1]),
    .rs_x_i     (px_q[FAKE_LAT-1]),
    .rs_y_i     (py_q[FAKE_LAT-1]),
    .rs_w_i     (pw_q[FAKE_LAT-1]),
    .rs_behind_i(1'b0),
    .rs_profile_i(2'd0),
    .rs_slot_i   (psl_q[FAKE_LAT-1]),
    .view_sel_i (1'b0),
    .o_valid_o       (fa_o_valid),
    .o_ready_i       (fa_o_ready),
    .o_ax_o          (fa_o_ax),
    .o_ay_o          (fa_o_ay),
    .o_bx_o          (fa_o_bx),
    .o_by_o          (fa_o_by),
    .o_cx_o          (fa_o_cx),
    .o_cy_o          (fa_o_cy),
    .o_behind_o      (fa_o_behind),
    .o_src_id_o      (fa_o_src_id),
    .o_untex_o       (fa_o_untex),
    .o_cull_mode_o   (fa_o_cull_mode),
    .o_attr_a_o      (fa_o_attr_a),
    .o_attr_b_o      (fa_o_attr_b),
    .o_attr_c_o      (fa_o_attr_c),
    .o_material_set_o(fa_material_set_o),
    .o_material_id_o (fa_material_id_o),
    .o_material_mode_o(fa_o_material_mode),
    .o_vertex_alpha_o (fa_o_vertex_alpha),
    .o_frag_state_o   (fa_o_frag_state),
    .o_quality_tier_o(fa_o_quality_tier),
    .busy_o          (fa_busy),
    .jobs_o          (asm_jobs_o),
    .vertices_o      (fa_vertices),
    .triangles_o     (asm_triangles_o),
    .index_oor_o     (fa_index_oor),
    .vtx_overflow_o  (fa_vtx_ovf),
    .slot_pressure_o (fa_slot_press),
    .dq_refused_o    (fa_dq_refused),
    .dq_stray_o      (fa_dq_stray),
    .proj_stray_o    (fa_proj_stray),
    .mat_skew_o      (asm_mat_skew_o)
  );

  // ==========================================================================
  // THE DOOR -- ONE CLIENT. See the header for why.
  // ==========================================================================
  logic cd_o_valid, cd_o_ready, cd_o_untex;
  logic [31:0] cd_o_material_set;
  logic [15:0] cd_o_material_id;
  logic [ 1:0] cd_o_material_mode;
  logic [IDW-1:0] cd_o_src_id;

  // A forge primitive is MATERIAL_BACKED. It carries a real {set, id} and
  // expects it resolved; it is NOT the no-sampling arm owner ruling 1 gave
  // particles, and reusing that arm as a fallback is what the ruling forbids.
  localparam logic [1:0] FORGE_MATERIAL_MODE = 2'd0;
  // FORGE.PRIM's raster declaration, unchanged by the shadow's arrival:
  // opaque, and state zero is the plain opaque write.
  localparam logic [ 7:0] FORGE_VERTEX_ALPHA = 8'hFF;
  localparam logic [31:0] FORGE_FRAG_STATE   = 32'd0;
  logic [ 1:0] fa_o_material_mode;
  logic [ 7:0] fa_o_vertex_alpha;
  logic [31:0] fa_o_frag_state;
  logic [ 7:0] cd_o_vertex_alpha;
  logic [31:0] cd_o_frag_state;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [ 7:0] pub_vertex_alpha_w;
  logic [31:0] pub_frag_state_w;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_geom_clipdoor #(
    .NCLIENT (1),
    .ATTRS   (ATTRS),
    .IDW     (IDW)
  ) u_door (
    .clk   (clk),
    .rst_n (rst_n),
    .c_valid_i        (fa_o_valid),
    .c_ready_o        (fa_o_ready),
    .c_ax_i           (fa_o_ax),
    .c_ay_i           (fa_o_ay),
    .c_bx_i           (fa_o_bx),
    .c_by_i           (fa_o_by),
    .c_cx_i           (fa_o_cx),
    .c_cy_i           (fa_o_cy),
    .c_behind_i       (fa_o_behind),
    .c_src_id_i       (fa_o_src_id),
    .c_untex_i        (fa_o_untex),
    .c_cull_mode_i    (fa_o_cull_mode),
    .c_attr_a_i       (fa_o_attr_a),
    .c_attr_b_i       (fa_o_attr_b),
    .c_attr_c_i       (fa_o_attr_c),
    // ARENAID 2026-09-25: the identity half, tied -- this bench measures
    // the forge arm's material path, not the identity space.
    .c_key_a_i        ('0),
    .c_key_b_i        ('0),
    .c_key_c_i        ('0),
    .c_rider_i        ('0),
    .c_material_set_i (fa_material_set_o),
    .c_material_id_i  (fa_material_id_o),
    .c_material_mode_i(fa_o_material_mode),
    .c_vertex_alpha_i (fa_o_vertex_alpha),
    .c_frag_state_i   (fa_o_frag_state),
    .c_quality_tier_i (fa_o_quality_tier),
    .o_valid_o       (cd_o_valid),
    .o_ready_i       (cd_o_ready),
    .o_ax_o          (cd_o_ax),
    .o_ay_o          (cd_o_ay),
    .o_bx_o          (cd_o_bx),
    .o_by_o          (cd_o_by),
    .o_cx_o          (cd_o_cx),
    .o_cy_o          (cd_o_cy),
    .o_behind_o      (cd_o_behind),
    .o_src_id_o      (cd_o_src_id),
    .o_untex_o       (cd_o_untex),
    .o_cull_mode_o   (cd_o_cull_mode),
    .o_attr_a_o      (cd_o_attr_a),
    .o_attr_b_o      (cd_o_attr_b),
    .o_attr_c_o      (cd_o_attr_c),
    /* verilator lint_off PINCONNECTEMPTY */   // ARENAID: no consumer here
    .o_key_a_o       (),
    .o_key_b_o       (),
    .o_key_c_o       (),
    .o_rider_o       (),
    /* verilator lint_on PINCONNECTEMPTY */
    .o_material_set_o(cd_o_material_set),
    .o_material_id_o (cd_o_material_id),
    .o_material_mode_o(cd_o_material_mode),
    .o_vertex_alpha_o (cd_o_vertex_alpha),
    .o_frag_state_o   (cd_o_frag_state),
    .o_quality_tier_o(cd_o_quality_tier),
    .o_owner_o       (cd_owner),
    .granted_o       (cd_granted),
    .switches_o      (cd_switches),
    .idle_offered_o  (cd_idle_offered),
    .err_hold_broken_o(cd_err_hold)
  );

  assign cl_material_set_o  = cd_o_material_set;
  assign cl_material_id_o   = cd_o_material_id;
  assign cl_material_mode_o = cd_o_material_mode;

  logic mw_t_valid, mw_t_ready;
  /* verilator lint_off UNUSEDSIGNAL */
  // Published fields this bench does not assert about. They are named rather
  // than left as empty pin connections: an empty connection is invisible to a
  // reader asking what the window publishes.
  logic [ 7:0] mw_req_tier, mw_pub_weight, mw_pub_binding;
  logic [ 2:0] mw_pub_recipe;
  logic [ 1:0] mw_pub_class;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_material_window #(
    .TMU_MODE_CLASS (8'b11_10_01_00),
    .OCCW           (8)
  ) u_window (
    .clk   (clk),
    .rst_n (rst_n),
    .t_valid_i        (cd_o_valid),
    .t_ready_o        (cd_o_ready),
    .t_material_set_i (cd_o_material_set),
    .t_material_id_i  (cd_o_material_id),
    .t_material_mode_i(cd_o_material_mode),
    .t_vertex_alpha_i (cd_o_vertex_alpha),
    .t_frag_state_i   (cd_o_frag_state),
    .t_quality_tier_i (cd_o_quality_tier),
    .t_valid_o        (mw_t_valid),
    .t_ready_i        (mw_t_ready),

    .d_enter_i  (cl_in_valid_o && cl_in_ready_i),
    .d_reject_i (d_reject_i),
    .d_leave_i  (d_leave_i),

    .req_valid_o        (req_valid_o),
    .req_ready_i        (req_ready_i),
    .req_material_set_o (req_material_set_o),
    .req_material_id_o  (req_material_id_o),
    .req_quality_tier_o (mw_req_tier),
    .rsp_valid_i        (rsp_valid_i),
    .rsp_ready_o        (rsp_ready_o),
    .rsp_status_i       (3'd0),
    .rsp_has_record_i   (rsp_has_record_i),
    .rsp_sample_count_i (rsp_sample_count_i),
    .rsp_material_recipe_i  (rsp_material_recipe_i),
    .rsp_recipe_weight_i    (rsp_recipe_weight_i),
    .rsp_base_binding_i     (rsp_base_binding_i),
    .rsp_selector_overflow_i(1'b0),
    .rsp_sample0_modes_i    (rsp_sample0_modes_i),

    // ---- I20's fragment-state group, CONNECTED 2026-09-25 (EDGEPREP) ------
    // The second of the two benches left stale when `zhao_material_window`
    // gained eight ports at `eca5b8d3` earlier the same day. See
    // `tb_partmat_acceptance.sv` for the full note. `rsp_frag_declared_i` LOW
    // is the compatibility default: the material declares nothing and the
    // primitive's fragment state stays authoritative, so this bench's
    // behaviour is unchanged.
    .rsp_frag_declared_i (1'b0),
    .rsp_frag_state_i    (32'd0),
    .rsp_effect_tag_i    (8'd0),
    .rsp_stencil_ref_i   (8'd0),

    .pub_valid_o           (pub_valid_o),
    .pub_sample_count_o    (pub_sample_count_o),
    .pub_material_recipe_o (mw_pub_recipe),
    .pub_recipe_weight_o   (mw_pub_weight),
    .pub_base_binding_o    (mw_pub_binding),
    .pub_response_class_o  (mw_pub_class),
    .pub_material_mode_o   (pub_material_mode_o),
    .pub_vertex_alpha_o    (pub_vertex_alpha_w),
    .pub_frag_state_o      (pub_frag_state_w),

    // Explicitly OPEN rather than absent: an empty connection states that this
    // bench does not observe the port, where a MISSING one is a pin the next
    // port change hides inside a wall of warnings.
    .pub_frag_declared_o   (),
    .pub_mat_frag_state_o  (),
    .pub_effect_tag_o      (),
    .pub_stencil_ref_o     (),

    .resolves_o                (mw_resolves_o),
    .switches_o                (mw_switches_o),
    .drain_stall_cycles_o      (mw_drain_stall),
    .answer_stall_cycles_o     (mw_answer_stall),
    .occupancy_max_o           (mw_occ_max),
    .no_record_o               (mw_no_record_o),
    .selector_overflow_o       (mw_sel_ovf),
    .clut_unowned_o            (mw_clut_unowned),
    .no_material_spans_o       (mw_no_material_spans),
    .mode_refused_o            (mw_mode_refused_o),
    .err_unpublished_o         (mw_err_unpublished_o),
    .err_occupancy_underflow_o (mw_err_underflow_o)
  );

  // ==========================================================================
  // R197's UNTEXTURED GATE -- THE COPY. See the header.
  // ==========================================================================
  wire cl_in_untex_c  = cd_o_untex;
  wire cl_in_refuse_c = cl_in_untex_c && (pub_sample_count_o != 2'd0);
  assign cl_in_valid_o = mw_t_valid && !cl_in_refuse_c;
  assign mw_t_ready    = cl_in_refuse_c || cl_in_ready_i;
  assign cl_untex_o    = cl_in_untex_c;
  assign cl_src_id_o   = cd_o_src_id;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      geom_untex_refused_o <= 32'd0;
    end else if (mw_t_valid && cl_in_refuse_c &&
                 (geom_untex_refused_o != 32'hffff_ffff)) begin
      geom_untex_refused_o <= geom_untex_refused_o + 32'd1;
    end
  end

  // The lint sink. Every bit named here is one this harness deliberately does
  // not read: the guard request's write/client/len/be fields (the responder
  // answers every read the same way), the high bits of a vertex the fake
  // projector truncates exactly as the real service's 21-bit screen space
  // does, and the memory address bits above the model's 8 KiB.
  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused = &{1'b0, fpb_req, fa_f_vx, fa_f_vy, fa_f_vz, mem_addr_i, 1'b0};
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
