// zhao_proj_subsystem.sv — the projection subsystem COMPOSED: one shared
// projector, terrain as its client B, the terrain arena shell fed straight
// off the result port, and the level-0 walker replaying sealed groups.
//
//     client A (geometry) ----\
//                              zhao_project_service  -- ONE zhao_project_core
//     client B (terrain) -----/          |
//        {arena,index} rider              | b_*_o  (x, y, d, w, behind, rider)
//                                         v
//                             zhao_terrain_wcache.fill  (3 copies, broadcast)
//                                         ^
//               zhao_terrain_topo ------> ref  (three corners per clock)
//                                         |
//                                         v
//                             one projected triangle per clock (+ w x3)
//
// THIS IS THE WIRING THE ADOPTION IS ABOUT, in RTL rather than in a test
// harness, and it is the top the projection-subsystem FIT GATE measures
// (reports/PROJECTION-ADOPTION-20260910.md, "the one gate"). The service's
// client-B result port drives the shell's fill port directly: the {arena,
// index} the producer put into the rider comes back on `b_payload_o` and IS
// the fill address. No adapter, no FIFO, no shadow state -- so the fill can
// never drift from the vertex it describes, which is the property the
// differential (tests/terrain/terrain_wcache_differential.cpp) leans on.
//
// WHAT IT EXPOSES, AND WHY EACH THING IS STILL A PORT:
//   * client A raw -- the geometry producer (GEOM.SKIN/WARP into GEOM.WCACHE)
//     is not composed anywhere in this tree; when it is, it plugs in here and
//     nothing in this file changes.
//   * client B as VERTICES with an {arena, index} rider, plus open/seal --
//     the terrain vertex producer is the tessellator's future vertex mode
//     (today the harness); until it exists this is the seam it will fill.
//   * the walker's job port -- level-0 unstitched only; stitched and coarse
//     topologies come from the tessellator (zhao_terrain_topo header).
//   * the triangle output -- zhao_terrain_project's packet plus three `w`,
//     which is what GEOM.CLIP consumes today and what GEOM.DEPTHQUANT needs.
//
// Nothing here has arithmetic: three instances and wires. The service, the
// shell and the walker each carry their own law and their own tests; this
// file's one claim is that the wires are right, and the differential holds
// that claim triangle for triangle against the retained legacy projector.
//
// Conservative SystemVerilog subset only (charter §2).
`default_nettype none

module zhao_proj_subsystem #(
    parameter int unsigned PAYLOAD_A_W   = 16,
    parameter int unsigned ROWS_PER_PASS = 3,
    parameter int unsigned MATW          = 32,
    parameter int unsigned ARENAS        = 4,
    parameter int unsigned DEPTH         = 81,
    parameter int unsigned GEN_W         = 8,
    parameter int unsigned VALID_MODE    = 1,
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

    // ---- client B: terrain lattice vertices with their arena address ---------
    input  wire                     b_valid_i,
    output wire                     b_ready_o,
    input  wire signed [31:0]       b_vx_i,
    input  wire signed [31:0]       b_vy_i,
    input  wire signed [31:0]       b_vz_i,
    input  wire                     b_view_i,
    input  wire [ARENA_W-1:0]       b_arena_i,
    input  wire [INDEX_W-1:0]       b_index_i,
    output wire                     fill_landed_o,   // one terrain fill wrote the arena

    // ---- the arena lifetime: open / seal ----------------------------------------
    input  wire                     open_i,
    input  wire [ARENA_W-1:0]       open_arena_i,
    output wire [GEN_W-1:0]         open_gen_o,
    input  wire                     seal_i,
    input  wire [ARENA_W-1:0]       seal_arena_i,

    // ---- the walker's job: one sealed level-0 group -----------------------------
    input  wire                     job_valid_i,
    output wire                     job_ready_o,
    input  wire [ARENA_W-1:0]       job_arena_i,
    input  wire [GEN_W-1:0]         job_gen_i,
    input  wire                     job_surface_i,
    input  wire        [15:0]       job_src_id_i,
    input  wire                     job_view_i,
    input  wire        [ 7:0]       job_mat_a_i,
    input  wire        [ 7:0]       job_mat_b_i,
    input  wire        [ 7:0]       job_weight_i,
    output wire                     hold_o,
    output wire [ARENA_W-1:0]       hold_arena_o,
    output wire                     done_o,
    output wire        [31:0]       jobs_done_o,

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
    output wire        [31:0]       replay_triangles_o,
    output wire        [31:0]       replay_refused_o,
    output wire        [31:0]       replay_missed_o,
    output wire        [31:0]       corner_hits_o,
    output wire        [31:0]       corner_refusals_o,
    output wire        [31:0]       corner_misses_o,
    output wire                     arena_overflow_o,
    output wire                     arena_seal_short_o,
    output wire                     shell_idle_o,
    output wire                     svc_busy_o,
    output wire        [31:0]       a_grants_o,
    output wire        [31:0]       b_grants_o,
    output wire        [31:0]       contended_o,
    output wire        [31:0]       mat_refused_o
);

  localparam int unsigned RIDE_B_W = ARENA_W + INDEX_W;

  // ---- the ONE core -------------------------------------------------------------------
  wire                  b_valid_o;
  wire signed [20:0]    b_x_o, b_y_o;
  wire signed [31:0]    b_d_o;
  wire        [30:0]    b_w_o;
  wire                  b_behind_o, b_view_o;
  wire [RIDE_B_W-1:0]   b_payload_o;

  zhao_project_service #(
      .PAYLOAD_A_W  (PAYLOAD_A_W),
      .PAYLOAD_B_W  (RIDE_B_W),
      .ROWS_PER_PASS(ROWS_PER_PASS),
      .MATW         (MATW)
  ) u_svc (
      .clk          (clk),
      .rst_n        (rst_n),
      .cfg_we_i     (cfg_we_i),
      .cfg_view_i   (cfg_view_i),
      .cfg_addr_i   (cfg_addr_i),
      .cfg_data_i   (cfg_data_i),
      .en_i         (en_i),
      .a_valid_i    (a_valid_i),
      .a_ready_o    (a_ready_o),
      .a_vx_i       (a_vx_i),
      .a_vy_i       (a_vy_i),
      .a_vz_i       (a_vz_i),
      .a_view_i     (a_view_i),
      .a_payload_i  (a_payload_i),
      .a_valid_o    (a_valid_o),
      .a_x_o        (a_x_o),
      .a_y_o        (a_y_o),
      .a_d_o        (a_d_o),
      .a_w_o        (a_w_o),
      .a_behind_o   (a_behind_o),
      .a_view_o     (a_view_o),
      .a_payload_o  (a_payload_o),
      .b_valid_i    (b_valid_i),
      .b_ready_o    (b_ready_o),
      .b_vx_i       (b_vx_i),
      .b_vy_i       (b_vy_i),
      .b_vz_i       (b_vz_i),
      .b_view_i     (b_view_i),
      .b_payload_i  ({b_arena_i, b_index_i}),
      .b_valid_o    (b_valid_o),
      .b_x_o        (b_x_o),
      .b_y_o        (b_y_o),
      .b_d_o        (b_d_o),
      .b_w_o        (b_w_o),
      .b_behind_o   (b_behind_o),
      .b_view_o     (b_view_o),
      .b_payload_o  (b_payload_o),
      .busy_o       (svc_busy_o),
      .a_grants_o   (a_grants_o),
      .b_grants_o   (b_grants_o),
      .contended_o  (contended_o),
      .mat_refused_o(mat_refused_o)
  );

  assign fill_landed_o = b_valid_o;

  // The view rides in the walker's JOB, not in the fill: a group holds one
  // view's results (the shell header), so the result's view tag has no
  // reader here. Named, not left dangling.
  wire b_view_unused = b_view_o;

  // ---- the terrain shell, fed straight off client B ------------------------------------
  wire                fill_ready_unused;   // the primitive never stalls a fill
  wire                ref_valid, ref_ready;
  wire [ARENA_W-1:0]  ref_arena;
  wire [GEN_W-1:0]    ref_gen;
  wire [INDEX_W-1:0]  ref_ia, ref_ib, ref_ic;
  wire [15:0]         ref_src;
  wire                ref_view;
  wire [7:0]          ref_mat_a, ref_mat_b, ref_weight;

  zhao_terrain_wcache #(
      .ARENAS    (ARENAS),
      .DEPTH     (DEPTH),
      .GEN_W     (GEN_W),
      .VALID_MODE(VALID_MODE),
      .INDEX_W   (INDEX_W),
      .ARENA_W   (ARENA_W)
  ) u_wc (
      .clk               (clk),
      .rst_n             (rst_n),
      .open_i            (open_i),
      .open_arena_i      (open_arena_i),
      .open_gen_o        (open_gen_o),
      .fill_valid_i      (b_valid_o),
      .fill_ready_o      (fill_ready_unused),
      .fill_arena_i      (b_payload_o[RIDE_B_W-1 -: ARENA_W]),
      .fill_index_i      (b_payload_o[INDEX_W-1:0]),
      .fill_x_i          (b_x_o),
      .fill_y_i          (b_y_o),
      .fill_d_i          (b_d_o),
      .fill_w_i          (b_w_o),
      .fill_behind_i     (b_behind_o),
      .seal_i            (seal_i),
      .seal_arena_i      (seal_arena_i),
      .ref_valid_i       (ref_valid),
      .ref_ready_o       (ref_ready),
      .ref_arena_i       (ref_arena),
      .ref_gen_i         (ref_gen),
      .ref_ia_i          (ref_ia),
      .ref_ib_i          (ref_ib),
      .ref_ic_i          (ref_ic),
      .ref_src_id_i      (ref_src),
      .ref_view_i        (ref_view),
      .ref_mat_a_i       (ref_mat_a),
      .ref_mat_b_i       (ref_mat_b),
      .ref_weight_i      (ref_weight),
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
      .idle_o            (shell_idle_o)
  );

  // ---- the level-0 walker --------------------------------------------------------------
  zhao_terrain_topo #(
      .ARENAS (ARENAS),
      .GEN_W  (GEN_W),
      .INDEX_W(INDEX_W),
      .ARENA_W(ARENA_W)
  ) u_topo (
      .clk          (clk),
      .rst_n        (rst_n),
      .job_valid_i  (job_valid_i),
      .job_ready_o  (job_ready_o),
      .job_arena_i  (job_arena_i),
      .job_gen_i    (job_gen_i),
      .job_surface_i(job_surface_i),
      .job_src_id_i (job_src_id_i),
      .job_view_i   (job_view_i),
      .job_mat_a_i  (job_mat_a_i),
      .job_mat_b_i  (job_mat_b_i),
      .job_weight_i (job_weight_i),
      .ref_valid_o  (ref_valid),
      .ref_ready_i  (ref_ready),
      .ref_arena_o  (ref_arena),
      .ref_gen_o    (ref_gen),
      .ref_ia_o     (ref_ia),
      .ref_ib_o     (ref_ib),
      .ref_ic_o     (ref_ic),
      .ref_src_id_o (ref_src),
      .ref_view_o   (ref_view),
      .ref_mat_a_o  (ref_mat_a),
      .ref_mat_b_o  (ref_mat_b),
      .ref_weight_o (ref_weight),
      .hold_o       (hold_o),
      .hold_arena_o (hold_arena_o),
      .done_o       (done_o),
      .jobs_done_o  (jobs_done_o)
  );

endmodule : zhao_proj_subsystem

`default_nettype wire
