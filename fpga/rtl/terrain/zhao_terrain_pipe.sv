// zhao_terrain_pipe.sv — the terrain PROJECTION pipeline, composed: the
// tessellator (ModeVtx + ModeRef) -> the group sequencer -> the shared
// projector (client B) -> the terrain arena shell -> one projected triangle
// per clock, in GEOM.CLIP's packet, for both views.
//
//     job (ox, oz, level, neighbours, morph, surface, dual, src, view_mask, mats)
//       |
//       v
//   zhao_terrain_group_seq ---- job x2 (mode 1, mode 2) ----> zhao_terrain_tess
//       |   ^  vertices (81, view-independent)  <----------------'   |  lattice /
//       |   ^  triples  (one triangle per clock) <------------------'  cell-state
//       |                                                              ports UP
//       |-- client B: vertex x V views, rider {arena, index} --> zhao_proj_subsystem
//       |-- open / seal                                        (service + shell)
//       '-- references x V views, tagged {arena, gen, view}  --> ... -> triangles out
//
// THIS IS THE TOP THE PROJECTION-SUBSYSTEM FIT GATE MEASURES
// (reports/TERRAIN-PIPELINE-COMPOSITION-20260910.md §gate) and the block that
// would replace `zhao_terrain_project` in a composed terrain top. It is NOT
// adopted: design/prod_manifest.yml lists it not-yet-adopted, and the flip is
// one deliberate edit after the fit.
//
// WHAT IS COMPOSED HERE AND WHAT IS NOT, stated so the remaining list stays
// honest:
//   * composed: tess (modes 1 and 2), sequencer, service, shell. Client A
//     (geometry) is passed straight through, as in the subsystem.
//   * NOT composed: TERRAIN.NORMALS. The legacy chain is tess(mode 0) ->
//     normals -> project; in this architecture no world-coordinate triangle
//     stream exists (mode 1 is vertices, mode 2 is indices). The face normal
//     needs one of: a ModeTri pass (a third presentation, +456 clocks per
//     level-0 job -- does not fit two-view), a world-vertex arena beside the
//     projected one (a second 4x81 shell at 96 bits), or a per-cell normal
//     produced upstream. That is a design question the composition report
//     names and does not answer; the tess's `tri_*` port is tied off here.
//   * NOT composed: GEOM.DEPTHQUANT on `out_*w_o` (adoption §7 item 7).
//   * The lattice and cell-state read ports are the tess's and go UP: the
//     lattice is TERRAIN.PATCH's composed slot, not this block's.
//
// `sparse_fill_i` is exposed rather than tied: with VALID_MODE = 0 it is the
// §3 remedy (81/25/9/4 fills per level); with VALID_MODE = 1 it is a fault
// the differential fires on purpose (seal short, sticky). The composition
// that owns both knobs decides. Because sparse fill is a runtime input, an
// elaboration guard cannot reject that pairing; the shell's seal/refusal
// counters make the misuse explicit instead.
//
// Conservative SystemVerilog subset only (charter §2).
`default_nettype none

module zhao_terrain_pipe #(
    parameter int unsigned PAYLOAD_A_W   = 16,
    parameter int unsigned ROWS_PER_PASS = 3,
    parameter int unsigned MATW          = 32,
    parameter int unsigned ARENAS        = 4,
    parameter int unsigned DEPTH         = 81,
    parameter int unsigned GEN_W         = 8,
    parameter int unsigned VALID_MODE    = 1,
    parameter int unsigned IDX_W         = 7,
    parameter int unsigned INDEX_W       = $clog2(DEPTH) + 1,
    parameter int unsigned ARENA_W       = $clog2(ARENAS) + 1
) (
    input  wire clk,
    input  wire rst_n,

    // ---- ONE configuration bus: one matrix bank for both clients ------------
    input  wire        cfg_we_i,
    input  wire        cfg_view_i,
    input  wire [ 4:0] cfg_addr_i,
    input  wire [31:0] cfg_data_i,
    input  wire        en_i,

    // ---- client A: geometry, passed straight through --------------------------
    input  wire                     a_valid_i,
    output wire                     a_ready_o,
    input  wire signed [31:0]       a_vx_i,
    input  wire signed [31:0]       a_vy_i,
    input  wire signed [31:0]       a_vz_i,
    input  wire                     a_view_i,
    input  wire [PAYLOAD_A_W-1:0]   a_payload_i,
    output wire                     a_valid_o,
    output wire signed [20:0]       a_x_o,
    output wire signed [20:0]       a_y_o,
    output wire signed [31:0]       a_d_o,
    output wire        [30:0]       a_w_o,
    output wire                     a_behind_o,
    output wire                     a_view_o,
    output wire [PAYLOAD_A_W-1:0]   a_payload_o,

    // ---- one subpatch job -----------------------------------------------------
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
    input  wire [ 1:0] job_view_mask_i,
    input  wire [ 7:0] job_mat_a_i,
    input  wire [ 7:0] job_mat_b_i,
    input  wire [ 7:0] job_weight_i,
    input  wire        sparse_fill_i,

    // ---- the tess's memory ports, up to the composition --------------------------
    output wire               lat_req_o,
    output wire        [ 5:0] lat_vi_o,
    output wire        [ 5:0] lat_vj_o,
    output wire               lat_surface_o,
    input  wire signed [31:0] lat_h_i,
    input  wire signed [31:0] lat_wx_i,
    input  wire signed [31:0] lat_wz_i,
    output wire               cs_req_o,
    output wire        [ 4:0] cs_ci_o,
    output wire        [ 4:0] cs_cj_o,
    input  wire        [ 1:0] cs_substance_i,

    // ---- projected triangles: zhao_terrain_project's packet + w x3 ---------------
    output wire                     out_valid_o,
    input  wire                     out_ready_i,
    output wire signed [20:0]       out_ax_o,
    output wire signed [20:0]       out_ay_o,
    output wire signed [20:0]       out_bx_o,
    output wire signed [20:0]       out_by_o,
    output wire signed [20:0]       out_cx_o,
    output wire signed [20:0]       out_cy_o,
    output wire        [ 2:0]       out_behind_o,
    output wire        [15:0]       out_src_id_o,
    output wire signed [31:0]       out_ad_o,
    output wire signed [31:0]       out_bd_o,
    output wire signed [31:0]       out_cd_o,
    output wire        [30:0]       out_aw_o,
    output wire        [30:0]       out_bw_o,
    output wire        [30:0]       out_cw_o,
    output wire                     out_view_o,
    output wire        [ 7:0]       out_mat_a_o,
    output wire        [ 7:0]       out_mat_b_o,
    output wire        [ 7:0]       out_weight_o,
    output wire                     out_refused_o,
    output wire                     out_missed_o,

    // ---- observation ---------------------------------------------------------------
    output wire                     idle_o,
    output wire [ARENAS-1:0]        held_o,
    // the sequencer's
    output wire        [31:0]       jobs_accepted_o,
    output wire        [31:0]       jobs_no_view_o,
    output wire        [31:0]       jobs_rejected_o,
    output wire        [31:0]       jobs_empty_o,
    output wire        [31:0]       groups_opened_o,
    output wire        [31:0]       groups_released_o,
    output wire        [31:0]       fills_forwarded_o,
    output wire        [31:0]       fills_dropped_o,
    output wire        [31:0]       refs_forwarded_o,
    output wire        [31:0]       release_unsafe_o,
    // the tess's
    output wire        [31:0]       tess_vertices_o,
    output wire        [31:0]       tess_refs_o,
    output wire        [31:0]       tess_rejected_o,
    output wire        [31:0]       tess_lod_clamped_o,
    output wire        [31:0]       tess_mode_invalid_o,
    // the shell's and the service's
    output wire        [31:0]       replay_triangles_o,
    output wire        [31:0]       replay_refused_o,
    output wire        [31:0]       replay_missed_o,
    output wire        [31:0]       corner_hits_o,
    output wire        [31:0]       corner_refusals_o,
    output wire        [31:0]       corner_misses_o,
    output wire                     arena_overflow_o,
    output wire                     arena_seal_short_o,
    output wire        [31:0]       a_grants_o,
    output wire        [31:0]       b_grants_o,
    output wire        [31:0]       contended_o,
    output wire        [31:0]       mat_refused_o
);

  // ---- the seams -------------------------------------------------------------------------
  // sequencer -> tess job
  wire        t_job_valid, t_job_ready, t_job_reject;
  wire [ 1:0] t_job_mode;
  wire [ 5:0] t_job_ox, t_job_oz;
  wire [ 1:0] t_job_level, t_job_nz, t_job_pz, t_job_nx, t_job_px;
  wire [16:0] t_job_morph;
  wire        t_job_surface, t_job_dual;
  wire [15:0] t_job_src;
  // tess -> sequencer streams
  wire               t_vtx_valid, t_vtx_ready, t_vtx_stride;
  wire signed [31:0] t_vtx_x, t_vtx_y, t_vtx_z;
  wire [IDX_W-1:0]   t_vtx_index;
  wire               t_ref_valid, t_ref_ready;
  wire [IDX_W-1:0]   t_ref_ia, t_ref_ib, t_ref_ic;
  // sequencer -> subsystem
  wire               b_valid, b_ready, b_view, fill_landed;
  wire signed [31:0] b_vx, b_vy, b_vz;
  wire [ARENA_W-1:0] b_arena, fill_arena, open_arena, seal_arena, r_arena;
  wire [INDEX_W-1:0] b_index, r_ia, r_ib, r_ic;
  wire               open_v, seal_v, r_valid, r_ready, r_view;
  wire [GEN_W-1:0]   open_gen, r_gen;
  wire [15:0]        r_src;
  wire [7:0]         r_mat_a, r_mat_b, r_weight;
  // tess outputs this composition does not consume (ModeTri never runs here)
  wire               tri_valid_unused, tri_surface_unused;
  wire signed [31:0] tri_ax_unused, tri_ay_unused, tri_az_unused, tri_bx_unused, tri_by_unused,
                     tri_bz_unused, tri_cx_unused, tri_cy_unused, tri_cz_unused;
  wire [15:0]        tri_src_unused, vtx_src_unused, ref_src_unused;
  wire               vtx_surface_unused, ref_surface_unused, tess_idle, seq_busy, shell_idle, svc_busy;
  wire [31:0]        tess_tris_unused;

  // ---- the tessellator: modes 1 and 2 only --------------------------------------------------
  zhao_terrain_tess #(.IDX_W(IDX_W)) u_tess (
      .clk            (clk),
      .rst_n          (rst_n),
      .job_valid_i    (t_job_valid),
      .job_ready_o    (t_job_ready),
      .job_mode_i     (t_job_mode),
      .job_ox_i       (t_job_ox),
      .job_oz_i       (t_job_oz),
      .job_level_i    (t_job_level),
      .job_lvl_nz_i   (t_job_nz),
      .job_lvl_pz_i   (t_job_pz),
      .job_lvl_nx_i   (t_job_nx),
      .job_lvl_px_i   (t_job_px),
      .job_morph_i    (t_job_morph),
      .job_surface_i  (t_job_surface),
      .job_dual_i     (t_job_dual),
      .job_src_id_i   (t_job_src),
      .lat_req_o      (lat_req_o),
      .lat_vi_o       (lat_vi_o),
      .lat_vj_o       (lat_vj_o),
      .lat_surface_o  (lat_surface_o),
      .lat_h_i        (lat_h_i),
      .lat_wx_i       (lat_wx_i),
      .lat_wz_i       (lat_wz_i),
      .cs_req_o       (cs_req_o),
      .cs_ci_o        (cs_ci_o),
      .cs_cj_o        (cs_cj_o),
      .cs_substance_i (cs_substance_i),
      // ModeTri is never presented; the port is tied ready so nothing can
      // ever hold the block, and its outputs are named unused.
      .tri_valid_o    (tri_valid_unused),
      .tri_ready_i    (1'b1),
      .ax_o           (tri_ax_unused),
      .ay_o           (tri_ay_unused),
      .az_o           (tri_az_unused),
      .bx_o           (tri_bx_unused),
      .by_o           (tri_by_unused),
      .bz_o           (tri_bz_unused),
      .cx_o           (tri_cx_unused),
      .cy_o           (tri_cy_unused),
      .cz_o           (tri_cz_unused),
      .surface_o      (tri_surface_unused),
      .src_id_o       (tri_src_unused),
      .vtx_valid_o    (t_vtx_valid),
      .vtx_ready_i    (t_vtx_ready),
      .vtx_x_o        (t_vtx_x),
      .vtx_y_o        (t_vtx_y),
      .vtx_z_o        (t_vtx_z),
      .vtx_index_o    (t_vtx_index),
      .vtx_stride_o   (t_vtx_stride),
      .vtx_surface_o  (vtx_surface_unused),
      .vtx_src_id_o   (vtx_src_unused),
      .ref_valid_o    (t_ref_valid),
      .ref_ready_i    (t_ref_ready),
      .ref_ia_o       (t_ref_ia),
      .ref_ib_o       (t_ref_ib),
      .ref_ic_o       (t_ref_ic),
      .ref_surface_o  (ref_surface_unused),
      .ref_src_id_o   (ref_src_unused),
      .terrain_triangles_emitted_o(tess_tris_unused),
      .terrain_vertices_emitted_o (tess_vertices_o),
      .terrain_refs_emitted_o     (tess_refs_o),
      .mode_invalid_o             (tess_mode_invalid_o),
      .subpatch_rejected_o        (tess_rejected_o),
      .lod_clamped_o              (tess_lod_clamped_o),
      .job_reject_o   (t_job_reject),
      .idle_o         (tess_idle)
  );

  // ---- the group sequencer -------------------------------------------------------------------
  // The committed mutant (tests/mutants/zhao_terrain_group_seq_mutant.sv,
  // release on PRESENTATION rather than acceptance) is substituted here by
  // the inverted-polarity control build only; production never defines it.
`ifdef ZHAO_MUTANT_GROUP_SEQ
  zhao_terrain_group_seq_mutant #(
`else
  zhao_terrain_group_seq #(
`endif
      .ARENAS (ARENAS),
      .DEPTH  (DEPTH),
      .GEN_W  (GEN_W),
      .IDX_W  (IDX_W),
      .INDEX_W(INDEX_W),
      .ARENA_W(ARENA_W)
  ) u_seq (
      .clk              (clk),
      .rst_n            (rst_n),
      .job_valid_i      (job_valid_i),
      .job_ready_o      (job_ready_o),
      .job_ox_i         (job_ox_i),
      .job_oz_i         (job_oz_i),
      .job_level_i      (job_level_i),
      .job_lvl_nz_i     (job_lvl_nz_i),
      .job_lvl_pz_i     (job_lvl_pz_i),
      .job_lvl_nx_i     (job_lvl_nx_i),
      .job_lvl_px_i     (job_lvl_px_i),
      .job_morph_i      (job_morph_i),
      .job_surface_i    (job_surface_i),
      .job_dual_i       (job_dual_i),
      .job_src_id_i     (job_src_id_i),
      .job_view_mask_i  (job_view_mask_i),
      .job_mat_a_i      (job_mat_a_i),
      .job_mat_b_i      (job_mat_b_i),
      .job_weight_i     (job_weight_i),
      .sparse_fill_i    (sparse_fill_i),
      .t_job_valid_o    (t_job_valid),
      .t_job_ready_i    (t_job_ready),
      .t_job_mode_o     (t_job_mode),
      .t_job_ox_o       (t_job_ox),
      .t_job_oz_o       (t_job_oz),
      .t_job_level_o    (t_job_level),
      .t_job_lvl_nz_o   (t_job_nz),
      .t_job_lvl_pz_o   (t_job_pz),
      .t_job_lvl_nx_o   (t_job_nx),
      .t_job_lvl_px_o   (t_job_px),
      .t_job_morph_o    (t_job_morph),
      .t_job_surface_o  (t_job_surface),
      .t_job_dual_o     (t_job_dual),
      .t_job_src_id_o   (t_job_src),
      .t_job_reject_i   (t_job_reject),
      .t_vtx_valid_i    (t_vtx_valid),
      .t_vtx_ready_o    (t_vtx_ready),
      .t_vtx_x_i        (t_vtx_x),
      .t_vtx_y_i        (t_vtx_y),
      .t_vtx_z_i        (t_vtx_z),
      .t_vtx_index_i    (t_vtx_index),
      .t_vtx_stride_i   (t_vtx_stride),
      .t_ref_valid_i    (t_ref_valid),
      .t_ref_ready_o    (t_ref_ready),
      .t_ref_ia_i       (t_ref_ia),
      .t_ref_ib_i       (t_ref_ib),
      .t_ref_ic_i       (t_ref_ic),
      .b_valid_o        (b_valid),
      .b_ready_i        (b_ready),
      .b_vx_o           (b_vx),
      .b_vy_o           (b_vy),
      .b_vz_o           (b_vz),
      .b_view_o         (b_view),
      .b_arena_o        (b_arena),
      .b_index_o        (b_index),
      .fill_landed_i    (fill_landed),
      .fill_arena_i     (fill_arena),
      .open_o           (open_v),
      .open_arena_o     (open_arena),
      .open_gen_i       (open_gen),
      .seal_o           (seal_v),
      .seal_arena_o     (seal_arena),
      .r_valid_o        (r_valid),
      .r_ready_i        (r_ready),
      .r_arena_o        (r_arena),
      .r_gen_o          (r_gen),
      .r_ia_o           (r_ia),
      .r_ib_o           (r_ib),
      .r_ic_o           (r_ic),
      .r_src_id_o       (r_src),
      .r_view_o         (r_view),
      .r_mat_a_o        (r_mat_a),
      .r_mat_b_o        (r_mat_b),
      .r_weight_o       (r_weight),
      .held_o           (held_o),
      .busy_o           (seq_busy),
      .jobs_accepted_o  (jobs_accepted_o),
      .jobs_no_view_o   (jobs_no_view_o),
      .jobs_rejected_o  (jobs_rejected_o),
      .jobs_empty_o     (jobs_empty_o),
      .groups_opened_o  (groups_opened_o),
      .groups_released_o(groups_released_o),
      .fills_forwarded_o(fills_forwarded_o),
      .fills_dropped_o  (fills_dropped_o),
      .refs_forwarded_o (refs_forwarded_o),
      .release_unsafe_o (release_unsafe_o)
  );

  // ---- the projection subsystem: service + shell ----------------------------------------------
  zhao_proj_subsystem #(
      .PAYLOAD_A_W  (PAYLOAD_A_W),
      .ROWS_PER_PASS(ROWS_PER_PASS),
      .MATW         (MATW),
      .ARENAS       (ARENAS),
      .DEPTH        (DEPTH),
      .GEN_W        (GEN_W),
      .VALID_MODE   (VALID_MODE),
      .INDEX_W      (INDEX_W),
      .ARENA_W      (ARENA_W)
  ) u_sub (
      .clk               (clk),
      .rst_n             (rst_n),
      .cfg_we_i          (cfg_we_i),
      .cfg_view_i        (cfg_view_i),
      .cfg_addr_i        (cfg_addr_i),
      .cfg_data_i        (cfg_data_i),
      .en_i              (en_i),
      .a_valid_i         (a_valid_i),
      .a_ready_o         (a_ready_o),
      .a_vx_i            (a_vx_i),
      .a_vy_i            (a_vy_i),
      .a_vz_i            (a_vz_i),
      .a_view_i          (a_view_i),
      .a_payload_i       (a_payload_i),
      .a_valid_o         (a_valid_o),
      .a_x_o             (a_x_o),
      .a_y_o             (a_y_o),
      .a_d_o             (a_d_o),
      .a_w_o             (a_w_o),
      .a_behind_o        (a_behind_o),
      .a_view_o          (a_view_o),
      .a_payload_o       (a_payload_o),
      .b_valid_i         (b_valid),
      .b_ready_o         (b_ready),
      .b_vx_i            (b_vx),
      .b_vy_i            (b_vy),
      .b_vz_i            (b_vz),
      .b_view_i          (b_view),
      .b_arena_i         (b_arena),
      .b_index_i         (b_index),
      .fill_landed_o     (fill_landed),
      .fill_arena_o      (fill_arena),
      .open_i            (open_v),
      .open_arena_i      (open_arena),
      .open_gen_o        (open_gen),
      .seal_i            (seal_v),
      .seal_arena_i      (seal_arena),
      .ref_valid_i       (r_valid),
      .ref_ready_o       (r_ready),
      .ref_arena_i       (r_arena),
      .ref_gen_i         (r_gen),
      .ref_ia_i          (r_ia),
      .ref_ib_i          (r_ib),
      .ref_ic_i          (r_ic),
      .ref_src_id_i      (r_src),
      .ref_view_i        (r_view),
      .ref_mat_a_i       (r_mat_a),
      .ref_mat_b_i       (r_mat_b),
      .ref_weight_i      (r_weight),
      .out_valid_o       (out_valid_o),
      .out_ready_i       (out_ready_i),
      .out_ax_o          (out_ax_o),
      .out_ay_o          (out_ay_o),
      .out_bx_o          (out_bx_o),
      .out_by_o          (out_by_o),
      .out_cx_o          (out_cx_o),
      .out_cy_o          (out_cy_o),
      .out_behind_o      (out_behind_o),
      .out_src_id_o      (out_src_id_o),
      .out_ad_o          (out_ad_o),
      .out_bd_o          (out_bd_o),
      .out_cd_o          (out_cd_o),
      .out_aw_o          (out_aw_o),
      .out_bw_o          (out_bw_o),
      .out_cw_o          (out_cw_o),
      .out_view_o        (out_view_o),
      .out_mat_a_o       (out_mat_a_o),
      .out_mat_b_o       (out_mat_b_o),
      .out_weight_o      (out_weight_o),
      .out_refused_o     (out_refused_o),
      .out_missed_o      (out_missed_o),
      .replay_triangles_o(replay_triangles_o),
      .replay_refused_o  (replay_refused_o),
      .replay_missed_o   (replay_missed_o),
      .corner_hits_o     (corner_hits_o),
      .corner_refusals_o (corner_refusals_o),
      .corner_misses_o   (corner_misses_o),
      .arena_overflow_o  (arena_overflow_o),
      .arena_seal_short_o(arena_seal_short_o),
      .shell_idle_o      (shell_idle),
      .svc_busy_o        (svc_busy),
      .a_grants_o        (a_grants_o),
      .b_grants_o        (b_grants_o),
      .contended_o       (contended_o),
      .mat_refused_o     (mat_refused_o)
  );

  // Idle: nothing held anywhere. A reply in flight in the shell or a vertex in
  // the core IS work (svc_busy covers the core; the shell's own idle covers
  // its skid and landing).
  assign idle_o = tess_idle && !seq_busy && shell_idle && !svc_busy && !out_valid_o;

endmodule : zhao_terrain_pipe

`default_nettype wire
