// tb_terrainaux_acceptance.sv -- THE SURFACE SHEET'S LOOP, CLOSED, FROM THE
// PRODUCTION MODULES.
//
// ENFORCED-BY: tests/prod/terrainaux_acceptance.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS BENCH EXISTS TO PROVE, AND WHY A COUNTER WOULD NOT HAVE DONE
// ---------------------------------------------------------------------------
// Before 2026-09-25 the console could WRITE layer F and could not SEE it.
// SURFACE.STAMP stamped a scar into `zhao_surface_sheet`; the composed texture
// island's `zhao_texture_aux_pipe_v2` knew how to ask for it; and between the
// two sat two separate refusals, each of which looked local and reasonable:
//
//   * `zhao_shell_top_v2` TIED the response to zero -- "TIE: page-generation
//     residency is its own clause and its own packet; no producer exists in
//     this shell yet" -- while the REQUEST half left the shell, the core AND
//     the board as a dangling top-level output group;
//   * and `zhao_texture_material_combine_v3` read only the AUX plane's STATUS,
//     because both texture contracts said in terms that "tag and strength are
//     reserved for a later visible terrain-effect composition".
//
// So this bench is not "did a port get driven". It is: STAMP A SCAR, SAMPLE IT
// THROUGH THE REAL ARBITER AND THE REAL STORE, AND WATCH A COLOUR CHANGE.
//
// ---------------------------------------------------------------------------
// EVERY MODULE HERE IS INSTANTIATED, NOTHING IS COPIED
// ---------------------------------------------------------------------------
//   zhao_surface_sheet        the real layer-F store, its real write port
//   zhao_surface_sheetshare   the real three-client arbiter (A, B, C)
//   zhao_texture_aux_pipe_v2  the real AUX adapter, on CLIENT C, exactly as
//                             `zhao_console_core` wires it since TERRAINAUX
//   zhao_texture_sheetmod     the visible effect
//
// A port either binds or the elaboration fails, so this bench cannot drift from
// the composition it measures. What it does NOT instantiate is the island and
// the combiner: the island's fragment protocol is `island_composed_directed`'s
// subject and the combiner is untouched by this packet. What this bench adds is
// the SEAM the island makes -- aux result to colour -- and it makes it with the
// island's own two operands.
//
// ---------------------------------------------------------------------------
// CLIENT A IS PRESENT AND BUSY ON PURPOSE
// ---------------------------------------------------------------------------
// A three-client arbiter measured with one client asking is a wire. The driver
// runs SURFACE.STAMP-shaped traffic on client A while the AUX reads go through
// client C, so `a_reqs_o` and `c_reqs_o` both move, `switches` happen, and the
// claim "round robin bounds both at one beat" is exercised rather than quoted.
// Client B (TERRAIN.SHEETSEAM) is idle here and busy in `tb_sheetseam`; between
// the two benches every pair is covered.
`default_nettype none

module tb_terrainaux_acceptance (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the layer-F WRITE port: the bench plays SURFACE.STAMP -------------
    input  var logic        wr_valid_i,
    output var logic        wr_ready_o,
    input  var logic [31:0] wr_handle_i,
    input  var logic [11:0] wr_texel_i,
    input  var logic [ 7:0] wr_tag_i,
    input  var logic [ 7:0] wr_strength_i,
    input  var logic        wr_we_tag_i,
    input  var logic        wr_we_strength_i,
    output var logic        wr_miss_o,

    // ---- CLIENT A: SURFACE.STAMP's own request stream ----------------------
    input  var logic        a_req_valid_i,
    output var logic        a_req_ready_o,
    input  var logic [ 1:0] a_req_op_i,
    input  var logic [31:0] a_req_handle_i,
    input  var logic [11:0] a_req_texel_i,
    input  var logic [15:0] a_req_src_id_i,
    output var logic        a_pg_valid_o,
    input  var logic        a_pg_ready_i,

    // ---- CLIENT C: the AUX job, at the island's own port shape -------------
    input  var logic               job_valid_i,
    output var logic               job_ready_o,
    input  var logic signed [31:0] job_wx_i,
    input  var logic signed [31:0] job_wz_i,
    input  var logic signed [31:0] job_env_x0_i,
    input  var logic signed [31:0] job_env_x1_i,
    input  var logic signed [31:0] job_env_z0_i,
    input  var logic signed [31:0] job_env_z1_i,
    input  var logic        [31:0] job_sheet_handle_i,
    input  var logic        [13:0] job_owner_i,
    input  var logic               job_force_refuse_i,

    output var logic               out_valid_o,
    input  var logic               out_ready_i,
    output var logic        [13:0] out_owner_o,
    output var logic        [47:0] out_result_o,

    // ---- THE VISIBLE EFFECT, driven from the returned AUX plane ------------
    // `sm_en_i` is the MATERIAL's declaration, exactly as the island takes it
    // from `frag_aux_i`; `sm_rgb_i` is the colour the recipe produced. The
    // strength is NOT a bench value: it is `out_result_o`'s own byte, taken
    // from the plane the store answered with.
    input  var logic               sm_en_i,
    input  var logic        [23:0] sm_rgb_i,
    output var logic        [23:0] sm_rgb_o,
    output var logic               sm_applied_o,
    // The strength the modulation actually used, so the driver checks the
    // value it is shading with rather than the value it hoped for.
    output var logic        [ 7:0] sm_strength_o,

    // ---- evidence ----------------------------------------------------------
    output var logic        [31:0] share_a_reqs_o,
    output var logic        [31:0] share_b_reqs_o,
    output var logic        [31:0] share_c_reqs_o,
    output var logic        [31:0] share_pg_orphan_o,
    output var logic        [31:0] share_pg_op_mismatch_o,
    output var logic        [ 1:0] share_owner_o,
    output var logic               share_busy_o,

    output var logic        [31:0] aux_accepted_o,
    output var logic        [31:0] aux_sheet_reads_o,
    output var logic        [31:0] aux_completed_o,
    output var logic        [31:0] aux_degenerate_o,
    output var logic        [31:0] aux_hits_o,
    output var logic        [31:0] aux_misses_o,
    output var logic        [31:0] aux_wrong_op_o,
    output var logic        [31:0] aux_wrong_status_o,
    output var logic        [31:0] aux_wrong_src_o,
    output var logic        [31:0] aux_unsolicited_o,
    output var logic        [31:0] aux_local_refused_o,
    output var logic        [31:0] aux_credit_fault_o,
    output var logic               aux_frame_fault_o,
    output var logic               aux_idle_o,
    output var logic               aux_sheet_rsp_owed_o,

    output var logic        [ 1:0] sheet_res_occupancy_o,
    output var logic               sheet_res_overflow_o,
    output var logic        [31:0] sheet_texels_touched_o,
    output var logic               sheet_idle_o
);

  // ---- the shared store request/response ----------------------------------
  logic        s_req_valid, s_req_ready;
  logic [ 1:0] s_req_op;
  logic [31:0] s_req_handle;
  logic [11:0] s_req_texel;
  logic [15:0] s_req_src_id;
  logic        s_pg_valid, s_pg_ready;
  logic [ 1:0] s_pg_op, s_pg_status;
  logic [ 7:0] s_pg_tag, s_pg_strength;
  logic [15:0] s_pg_src_id;

  // ---- client C, between the AUX pipe and the share ------------------------
  logic        c_req_valid, c_req_ready;
  logic [ 1:0] c_req_op;
  logic [31:0] c_req_handle;
  logic [11:0] c_req_texel;
  logic [15:0] c_req_src_id;
  logic        c_pg_valid, c_pg_ready;

  logic [ 4:0] aux_credit;
  logic        aux_issue_valid;
  logic [13:0] aux_issue_owner;
  logic        aux_refuse_valid;

  // =========================================================================
  // THE STORE
  // =========================================================================
  zhao_surface_sheet #(.Slots(2)) u_sheet (
      .clk(clk),
      .rst_n(rst_n),

      .req_valid_i (s_req_valid),
      .req_ready_o (s_req_ready),
      .req_op_i    (s_req_op),
      .req_handle_i(s_req_handle),
      .req_texel_i (s_req_texel),
      .req_src_id_i(s_req_src_id),

      .pg_valid_o   (s_pg_valid),
      .pg_ready_i   (s_pg_ready),
      .pg_op_o      (s_pg_op),
      .pg_status_o  (s_pg_status),
      .pg_tag_o     (s_pg_tag),
      .pg_strength_o(s_pg_strength),
      .pg_src_id_o  (s_pg_src_id),

      .wr_valid_i      (wr_valid_i),
      .wr_ready_o      (wr_ready_o),
      .wr_handle_i     (wr_handle_i),
      .wr_texel_i      (wr_texel_i),
      .wr_tag_i        (wr_tag_i),
      .wr_strength_i   (wr_strength_i),
      .wr_we_tag_i     (wr_we_tag_i),
      .wr_we_strength_i(wr_we_strength_i),
      .wr_src_id_i     (16'd0),
      .wr_miss_o       (wr_miss_o),
      .wr_miss_src_id_o(),

      .res_occupancy_o(sheet_res_occupancy_o),
      .res_busy_o     (),
      .res_overflow_o (sheet_res_overflow_o),

      .surface_texels_touched_o(sheet_texels_touched_o),
      .idle_o                  (sheet_idle_o)
  );

  // =========================================================================
  // THE THREE-CLIENT SHARE, wired exactly as `zhao_console_core` wires it
  // =========================================================================
  zhao_surface_sheetshare u_share (
      .clk(clk),
      .rst_n(rst_n),

      .a_req_valid_i (a_req_valid_i),
      .a_req_ready_o (a_req_ready_o),
      .a_req_op_i    (a_req_op_i),
      .a_req_handle_i(a_req_handle_i),
      .a_req_texel_i (a_req_texel_i),
      .a_req_src_id_i(a_req_src_id_i),
      .a_pg_valid_o  (a_pg_valid_o),
      .a_pg_ready_i  (a_pg_ready_i),

      // CLIENT B -- TERRAIN.SHEETSEAM, idle here and busy in tb_sheetseam.
      .b_req_valid_i (1'b0),
      .b_req_ready_o (),
      .b_req_op_i    (2'd0),
      .b_req_handle_i(32'd0),
      .b_req_texel_i (12'd0),
      .b_req_src_id_i(16'd0),
      .b_pg_valid_o  (),
      .b_pg_ready_i  (1'b1),

      .c_req_valid_i (c_req_valid),
      .c_req_ready_o (c_req_ready),
      .c_req_op_i    (c_req_op),
      .c_req_handle_i(c_req_handle),
      .c_req_texel_i (c_req_texel),
      .c_req_src_id_i(c_req_src_id),
      .c_pg_valid_o  (c_pg_valid),
      .c_pg_ready_i  (c_pg_ready),

      .s_req_valid_o (s_req_valid),
      .s_req_ready_i (s_req_ready),
      .s_req_op_o    (s_req_op),
      .s_req_handle_o(s_req_handle),
      .s_req_texel_o (s_req_texel),
      .s_req_src_id_o(s_req_src_id),
      .s_pg_valid_i  (s_pg_valid),
      .s_pg_ready_o  (s_pg_ready),
      .s_pg_op_i     (s_pg_op),

      .busy_o           (share_busy_o),
      .owner_o          (share_owner_o),
      .a_reqs_o         (share_a_reqs_o),
      .b_reqs_o         (share_b_reqs_o),
      .c_reqs_o         (share_c_reqs_o),
      .pg_orphan_o      (share_pg_orphan_o),
      .pg_op_mismatch_o (share_pg_op_mismatch_o)
  );

  // =========================================================================
  // THE AUX ADAPTER -- the island's own instance parameters
  // =========================================================================
  zhao_texture_aux_pipe_v2 #(.OWNERW(14), .CREDIT(16)) u_aux (
      .clk(clk),
      .rst_n(rst_n),
      .frame_fault_clear_i(1'b0),

      .job_valid_i       (job_valid_i),
      .job_ready_o       (job_ready_o),
      .job_wx_i          (job_wx_i),
      .job_wz_i          (job_wz_i),
      .job_env_x0_i      (job_env_x0_i),
      .job_env_x1_i      (job_env_x1_i),
      .job_env_z0_i      (job_env_z0_i),
      .job_env_z1_i      (job_env_z1_i),
      .job_sheet_handle_i(job_sheet_handle_i),
      .job_owner_i       (job_owner_i),
      .job_force_refuse_i(job_force_refuse_i),

      .issue_valid_o(aux_issue_valid),
      .issue_owner_o(aux_issue_owner),

      .req_valid_o (c_req_valid),
      .req_ready_i (c_req_ready),
      .req_op_o    (c_req_op),
      .req_handle_o(c_req_handle),
      .req_texel_o (c_req_texel),
      .req_src_id_o(c_req_src_id),

      .pg_valid_i   (c_pg_valid),
      .pg_ready_o   (c_pg_ready),
      .pg_op_i      (s_pg_op),
      .pg_status_i  (s_pg_status),
      .pg_tag_i     (s_pg_tag),
      .pg_strength_i(s_pg_strength),
      .pg_src_id_i  (s_pg_src_id),

      .out_valid_o(out_valid_o),
      .out_ready_i(out_ready_i),
      .out_owner_o(out_owner_o),
      .out_result_o(out_result_o),

      .refuse_valid_o  (aux_refuse_valid),
      .sheet_rsp_owed_o(aux_sheet_rsp_owed_o),
      .idle_o          (aux_idle_o),

      .accepted_o              (aux_accepted_o),
      .sheet_reads_o           (aux_sheet_reads_o),
      .local_refused_o         (aux_local_refused_o),
      .completed_o             (aux_completed_o),
      .degenerate_o            (aux_degenerate_o),
      .sheet_hits_o            (aux_hits_o),
      .sheet_misses_o          (aux_misses_o),
      .sheet_rsp_wrong_op_o    (aux_wrong_op_o),
      .sheet_rsp_wrong_status_o(aux_wrong_status_o),
      .sheet_rsp_wrong_src_o   (aux_wrong_src_o),
      .sheet_rsp_unsolicited_o (aux_unsolicited_o),
      .credit_fault_o          (aux_credit_fault_o),
      .frame_fault_o           (aux_frame_fault_o),
      .credit_in_use_o         (aux_credit)
  );

  // =========================================================================
  // THE VISIBLE EFFECT
  // =========================================================================
  // THE STRENGTH IS HELD, AND THAT IS THE ISLAND'S OWN SHAPE RATHER THAN A
  // BENCH CONVENIENCE. `out_result_o` is valid only on its handshake; the
  // colour it tints is produced later, by a different block, for the same
  // fragment. `zhao_texture_island_v3_top` solves that with `sheet_m`, a
  // per-OWNER-SLOT record written on the AUX return and read back with the
  // owner the combiner returns. This bench holds the LAST retired plane,
  // which is the same discipline with one slot -- and the driver runs one AUX
  // job at a time, so one slot is exactly right here and would not be in the
  // island.
  logic [7:0] sm_strength_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) sm_strength_q <= 8'd0;
    else if (out_valid_o && out_ready_i)
      sm_strength_q <= out_result_o[31:24];
  end
  assign sm_strength_o = sm_strength_q;

  zhao_texture_sheetmod u_sheetmod (
      .en_i      (sm_en_i),
      // THE STRENGTH IS THE PLANE'S OWN BYTE. `zhao_texture_aux_pipe_v2`'s
      // header fixes the typed owner plane at
      // "[47:40] status, [39:32] tag, [31:24] strength, [23:0] zero", which is
      // `TEXTURE_RESULT_ALPHA_HI:LO` -- the same slice `zhao_texture_island_v3_top`
      // takes. A bench constant here would make the whole test a test of the
      // bench.
      .strength_i(sm_strength_q),
      .rgb_i     (sm_rgb_i),
      .rgb_o     (sm_rgb_o),
      .applied_o (sm_applied_o)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  wire _unused = &{1'b0, aux_credit, aux_issue_valid, aux_issue_owner,
                   aux_refuse_valid};
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
