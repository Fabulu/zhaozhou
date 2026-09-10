// tb_terrain_wcache.sv — zhao_proj_subsystem (the projection subsystem composed
// in RTL) beside the retained legacy projector, for the differential:
//
//     client A (geometry noise) --\
//                                  zhao_project_service --> client B results
//     client B (terrain vertices)-/          |                    |
//                                            |         {arena,index} rider
//                                            v                    v
//                                     one zhao_project_core   zhao_terrain_wcache.fill
//                                                                  ^
//                                          zhao_terrain_topo --> ref (3 corners/clk)
//                                                                  |
//                                                                  v
//                                                          projected triangles
//
//     zhao_terrain_project (the retained legacy oracle, its OWN cfg bus)
//
// The wiring -- the service's client-B result port driving the shell's fill
// port directly, the {arena, index} rider coming back as the fill address --
// lives ONCE, in fpga/rtl/common/zhao_proj_subsystem.sv. This bench adds the
// legacy `zhao_terrain_project` beside it and nothing else.
//
// The legacy projector has a SEPARATE cfg bus so the mid-frame reconfiguration
// case can hold the oracle at the fill-time matrix while the service moves on
// (or the reverse). In every other case the driver writes both buses
// identically.
//
// Parameters passed through so one file elaborates every variant the
// differential needs: ROWS_PER_PASS (3 = default, 1 = one vertex per three
// clocks) and VALID_MODE (1 = dense, 0 = bitmap -- the miss-path control).

`default_nettype none

module tb_terrain_wcache #(
    parameter int unsigned ROWS_PER_PASS = 3,
    parameter int unsigned VALID_MODE    = 1,
    parameter int unsigned ARENAS        = 4,
    parameter int unsigned DEPTH         = 81,
    parameter int unsigned GEN_W         = 8
) (
    input wire clk,
    input wire rst_n,

    // ---- service cfg bus (shared by both clients: one matrix bank) ---------
    input  wire        cfg_we_i,
    input  wire        cfg_view_i,
    input  wire [ 4:0] cfg_addr_i,
    input  wire [31:0] cfg_data_i,
    input  wire        en_i,

    // ---- client A: geometry, driven raw ------------------------------------
    input  wire               a_valid_i,
    output wire               a_ready_o,
    input  wire signed [31:0] a_vx_i,
    input  wire signed [31:0] a_vy_i,
    input  wire signed [31:0] a_vz_i,
    input  wire               a_view_i,
    input  wire        [15:0] a_payload_i,
    output wire               a_valid_o,
    output wire signed [20:0] a_x_o,
    output wire signed [20:0] a_y_o,
    output wire signed [31:0] a_d_o,
    output wire        [30:0] a_w_o,
    output wire               a_behind_o,
    output wire               a_view_o,
    output wire        [15:0] a_payload_o,

    // ---- client B: terrain vertices with an {arena, index} rider -----------
    input  wire               b_valid_i,
    output wire               b_ready_o,
    input  wire signed [31:0] b_vx_i,
    input  wire signed [31:0] b_vy_i,
    input  wire signed [31:0] b_vz_i,
    input  wire               b_view_i,
    input  wire [$clog2(ARENAS):0] b_arena_i,
    input  wire [$clog2(DEPTH):0]  b_index_i,
    output wire               fill_seen_o,     // = b_valid_o: one fill landed

    // ---- the shell's producer control --------------------------------------
    input  wire               open_i,
    input  wire [$clog2(ARENAS):0] open_arena_i,
    output wire [GEN_W-1:0]   open_gen_o,
    input  wire               seal_i,
    input  wire [$clog2(ARENAS):0] seal_arena_i,

    // ---- the walker's job port ---------------------------------------------
    input  wire               job_valid_i,
    output wire               job_ready_o,
    input  wire [$clog2(ARENAS):0] job_arena_i,
    input  wire [GEN_W-1:0]   job_gen_i,
    input  wire               job_surface_i,
    input  wire        [15:0] job_src_id_i,
    input  wire               job_view_i,
    input  wire        [ 7:0] job_mat_a_i,
    input  wire        [ 7:0] job_mat_b_i,
    input  wire        [ 7:0] job_weight_i,
    output wire               hold_o,
    output wire [$clog2(ARENAS):0] hold_arena_o,
    output wire               done_o,
    output wire        [31:0] jobs_done_o,

    // ---- the shell's triangle output ---------------------------------------
    output wire               out_valid_o,
    input  wire               out_ready_i,
    output wire signed [20:0] out_ax_o,
    output wire signed [20:0] out_ay_o,
    output wire signed [20:0] out_bx_o,
    output wire signed [20:0] out_by_o,
    output wire signed [20:0] out_cx_o,
    output wire signed [20:0] out_cy_o,
    output wire        [ 2:0] out_behind_o,
    output wire        [15:0] out_src_id_o,
    output wire signed [31:0] out_ad_o,
    output wire signed [31:0] out_bd_o,
    output wire signed [31:0] out_cd_o,
    output wire        [30:0] out_aw_o,
    output wire        [30:0] out_bw_o,
    output wire        [30:0] out_cw_o,
    output wire               out_view_o,
    output wire        [ 7:0] out_mat_a_o,
    output wire        [ 7:0] out_mat_b_o,
    output wire        [ 7:0] out_weight_o,
    output wire               out_refused_o,
    output wire               out_missed_o,

    // ---- counters ----------------------------------------------------------
    output wire        [31:0] replay_triangles_o,
    output wire        [31:0] replay_refused_o,
    output wire        [31:0] replay_missed_o,
    output wire        [31:0] corner_hits_o,
    output wire        [31:0] corner_refusals_o,
    output wire        [31:0] corner_misses_o,
    output wire               arena_overflow_o,
    output wire               arena_seal_short_o,
    output wire               shell_idle_o,
    output wire               svc_busy_o,
    output wire        [31:0] a_grants_o,
    output wire        [31:0] b_grants_o,
    output wire        [31:0] contended_o,
    output wire        [31:0] mat_refused_o,

    // ---- the legacy oracle: its own cfg bus, its own triangle stream -------
    input  wire        ocfg_we_i,
    input  wire        ocfg_view_i,
    input  wire [ 4:0] ocfg_addr_i,
    input  wire [31:0] ocfg_data_i,
    input  wire               tri_valid_i,
    output wire               tri_ready_o,
    input  wire signed [31:0] ax_i,
    input  wire signed [31:0] ay_i,
    input  wire signed [31:0] az_i,
    input  wire signed [31:0] bx_i,
    input  wire signed [31:0] by_i,
    input  wire signed [31:0] bz_i,
    input  wire signed [31:0] cx_i,
    input  wire signed [31:0] cy_i,
    input  wire signed [31:0] cz_i,
    input  wire        [15:0] src_id_i,
    input  wire               view_i,
    input  wire        [ 7:0] mat_a_i,
    input  wire        [ 7:0] mat_b_i,
    input  wire        [ 7:0] weight_i,
    output wire               o_valid_o,
    input  wire               o_ready_i,
    output wire signed [20:0] o_ax_o,
    output wire signed [20:0] o_ay_o,
    output wire signed [20:0] o_bx_o,
    output wire signed [20:0] o_by_o,
    output wire signed [20:0] o_cx_o,
    output wire signed [20:0] o_cy_o,
    output wire        [ 2:0] o_behind_o,
    output wire        [15:0] o_src_id_o,
    output wire signed [31:0] o_ad_o,
    output wire signed [31:0] o_bd_o,
    output wire signed [31:0] o_cd_o,
    output wire               o_view_o,
    output wire        [ 7:0] o_mat_a_o,
    output wire        [ 7:0] o_mat_b_o,
    output wire        [ 7:0] o_weight_o,
    output wire        [31:0] o_triangles_o,
    output wire               o_idle_o,
    output wire        [31:0] o_mat_refused_o
);

  // ---- the composed subsystem: service + terrain shell + walker, in RTL ------------
  // The wiring under test lives ONCE, in fpga/rtl/common/zhao_proj_subsystem.sv;
  // this bench adds only the legacy oracle beside it.
  zhao_proj_subsystem #(
      .PAYLOAD_A_W  (16),
      .ROWS_PER_PASS(ROWS_PER_PASS),
      .ARENAS       (ARENAS),
      .DEPTH        (DEPTH),
      .GEN_W        (GEN_W),
      .VALID_MODE   (VALID_MODE)
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
      .b_valid_i         (b_valid_i),
      .b_ready_o         (b_ready_o),
      .b_vx_i            (b_vx_i),
      .b_vy_i            (b_vy_i),
      .b_vz_i            (b_vz_i),
      .b_view_i          (b_view_i),
      .b_arena_i         (b_arena_i),
      .b_index_i         (b_index_i),
      .fill_landed_o     (fill_seen_o),
      .open_i            (open_i),
      .open_arena_i      (open_arena_i),
      .open_gen_o        (open_gen_o),
      .seal_i            (seal_i),
      .seal_arena_i      (seal_arena_i),
      .job_valid_i       (job_valid_i),
      .job_ready_o       (job_ready_o),
      .job_arena_i       (job_arena_i),
      .job_gen_i         (job_gen_i),
      .job_surface_i     (job_surface_i),
      .job_src_id_i      (job_src_id_i),
      .job_view_i        (job_view_i),
      .job_mat_a_i       (job_mat_a_i),
      .job_mat_b_i       (job_mat_b_i),
      .job_weight_i      (job_weight_i),
      .hold_o            (hold_o),
      .hold_arena_o      (hold_arena_o),
      .done_o            (done_o),
      .jobs_done_o       (jobs_done_o),
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
      .shell_idle_o      (shell_idle_o),
      .svc_busy_o        (svc_busy_o),
      .a_grants_o        (a_grants_o),
      .b_grants_o        (b_grants_o),
      .contended_o       (contended_o),
      .mat_refused_o     (mat_refused_o)
  );

  // ---- the retained oracle ---------------------------------------------------------------
  zhao_terrain_project u_legacy (
      .clk         (clk),
      .rst_n       (rst_n),
      .cfg_we_i    (ocfg_we_i),
      .cfg_view_i  (ocfg_view_i),
      .cfg_addr_i  (ocfg_addr_i),
      .cfg_data_i  (ocfg_data_i),
      .tri_valid_i (tri_valid_i),
      .tri_ready_o (tri_ready_o),
      .ax_i        (ax_i),
      .ay_i        (ay_i),
      .az_i        (az_i),
      .bx_i        (bx_i),
      .by_i        (by_i),
      .bz_i        (bz_i),
      .cx_i        (cx_i),
      .cy_i        (cy_i),
      .cz_i        (cz_i),
      .src_id_i    (src_id_i),
      .view_i      (view_i),
      .mat_a_i     (mat_a_i),
      .mat_b_i     (mat_b_i),
      .weight_i    (weight_i),
      .out_valid_o (o_valid_o),
      .out_ready_i (o_ready_i),
      .out_ax_o    (o_ax_o),
      .out_ay_o    (o_ay_o),
      .out_bx_o    (o_bx_o),
      .out_by_o    (o_by_o),
      .out_cx_o    (o_cx_o),
      .out_cy_o    (o_cy_o),
      .out_behind_o(o_behind_o),
      .out_src_id_o(o_src_id_o),
      .out_ad_o    (o_ad_o),
      .out_bd_o    (o_bd_o),
      .out_cd_o    (o_cd_o),
      .out_view_o  (o_view_o),
      .out_mat_a_o (o_mat_a_o),
      .out_mat_b_o (o_mat_b_o),
      .out_weight_o(o_weight_o),
      .terrain_triangles_emitted_o(o_triangles_o),
      .idle_o      (o_idle_o),
      .mat_refused_o(o_mat_refused_o)
  );

endmodule : tb_terrain_wcache

`default_nettype wire
