// tb_geom_binner_v2_pair.sv -- exact old/V2 binner differential wrapper.
//
// Both instances see the same triangle, token, frame, and drain traffic. Every
// observable old stream/status signal is exposed independently; Packet-D adds
// only the V2 metadata input/output. The C++ driver checks parity every clock.
`default_nettype none

module tb_geom_binner_v2_pair #(
  parameter int unsigned GRID_W     = 24,
  parameter int unsigned GRID_H     = 24,
  parameter int unsigned TILES      = GRID_W * GRID_H,
  parameter int unsigned TIDX_W     = 10,
  parameter int unsigned TRI_CAP    = 128,
  parameter int unsigned TRI_W      = 7,
  parameter int unsigned CHUNKS     = 256,
  parameter int unsigned CHUNK_W    = 8,
  parameter int unsigned CHUNK_REFS = 4,
  parameter int unsigned METAW      = 1157
) (
  input  logic clk,
  input  logic rst_n,

  input  logic               frame_begin_i,
  input  logic               frame_end_i,
  input  logic        [5:0]  grid_w_i,
  input  logic        [5:0]  grid_h_i,

  input  logic               tri_valid_i,
  output logic               old_tri_ready_o,
  output logic               v2_tri_ready_o,
  input  logic signed [22:0] tri_kx0_i,
  input  logic signed [22:0] tri_ky0_i,
  input  logic signed [47:0] tri_kc0_i,
  input  logic signed [22:0] tri_kx1_i,
  input  logic signed [22:0] tri_ky1_i,
  input  logic signed [47:0] tri_kc1_i,
  input  logic signed [22:0] tri_kx2_i,
  input  logic signed [22:0] tri_ky2_i,
  input  logic signed [47:0] tri_kc2_i,
  input  logic        [2:0]  tri_tl_i,
  input  logic signed [20:0] tri_ax_i,
  input  logic signed [20:0] tri_ay_i,
  input  logic signed [20:0] tri_bx_i,
  input  logic signed [20:0] tri_by_i,
  input  logic signed [20:0] tri_cx_i,
  input  logic signed [20:0] tri_cy_i,
  input  logic signed [11:0] tri_min_x_i,
  input  logic signed [11:0] tri_max_x_i,
  input  logic signed [11:0] tri_min_y_i,
  input  logic signed [11:0] tri_max_y_i,
  input  logic        [15:0] tri_src_id_i,
  input  logic       [METAW-1:0] tri_meta_i,

  output logic               old_tok_req_o,
  output logic               v2_tok_req_o,
  input  logic               tok_grant_i,

  output logic               old_job_valid_o,
  output logic               v2_job_valid_o,
  input  logic               job_ready_i,
  output logic signed [20:0] old_job_ax_o,
  output logic signed [20:0] old_job_ay_o,
  output logic signed [20:0] old_job_bx_o,
  output logic signed [20:0] old_job_by_o,
  output logic signed [20:0] old_job_cx_o,
  output logic signed [20:0] old_job_cy_o,
  output logic               old_job_first_o,
  output logic               old_job_last_o,
  output logic signed [11:0] old_job_tile_x_o,
  output logic signed [11:0] old_job_tile_y_o,
  output logic        [15:0] old_job_src_id_o,
  output logic signed [20:0] v2_job_ax_o,
  output logic signed [20:0] v2_job_ay_o,
  output logic signed [20:0] v2_job_bx_o,
  output logic signed [20:0] v2_job_by_o,
  output logic signed [20:0] v2_job_cx_o,
  output logic signed [20:0] v2_job_cy_o,
  output logic               v2_job_first_o,
  output logic               v2_job_last_o,
  output logic signed [11:0] v2_job_tile_x_o,
  output logic signed [11:0] v2_job_tile_y_o,
  output logic        [15:0] v2_job_src_id_o,
  output logic       [METAW-1:0] v2_job_meta_o,

  output logic               old_drain_busy_o,
  output logic               v2_drain_busy_o,
  output logic               old_drain_done_o,
  output logic               v2_drain_done_o,
  output logic        [31:0] old_tile_references_o,
  output logic        [31:0] v2_tile_references_o,
  output logic        [15:0] old_max_tile_list_depth_o,
  output logic        [15:0] v2_max_tile_list_depth_o,
  output logic        [31:0] old_triangles_culled_o,
  output logic        [31:0] v2_triangles_culled_o,
  output logic               old_overflow_o,
  output logic               v2_overflow_o,
  output logic               old_arena_full_o,
  output logic               v2_arena_full_o,
  output logic [CHUNK_W:0]   old_arena_used_o,
  output logic [CHUNK_W:0]   v2_arena_used_o
);

  zhao_geom_binner #(
    .GRID_W(GRID_W), .GRID_H(GRID_H), .TILES(TILES), .TIDX_W(TIDX_W),
    .TRI_CAP(TRI_CAP), .TRI_W(TRI_W), .CHUNKS(CHUNKS),
    .CHUNK_W(CHUNK_W), .CHUNK_REFS(CHUNK_REFS)
  ) u_old (
    .clk(clk), .rst_n(rst_n),
    .frame_begin_i(frame_begin_i), .frame_end_i(frame_end_i),
    .grid_w_i(grid_w_i), .grid_h_i(grid_h_i),
    .tri_valid_i(tri_valid_i), .tri_ready_o(old_tri_ready_o),
    .tri_kx0_i(tri_kx0_i), .tri_ky0_i(tri_ky0_i), .tri_kc0_i(tri_kc0_i),
    .tri_kx1_i(tri_kx1_i), .tri_ky1_i(tri_ky1_i), .tri_kc1_i(tri_kc1_i),
    .tri_kx2_i(tri_kx2_i), .tri_ky2_i(tri_ky2_i), .tri_kc2_i(tri_kc2_i),
    .tri_tl_i(tri_tl_i),
    .tri_ax_i(tri_ax_i), .tri_ay_i(tri_ay_i),
    .tri_bx_i(tri_bx_i), .tri_by_i(tri_by_i),
    .tri_cx_i(tri_cx_i), .tri_cy_i(tri_cy_i),
    .tri_min_x_i(tri_min_x_i), .tri_max_x_i(tri_max_x_i),
    .tri_min_y_i(tri_min_y_i), .tri_max_y_i(tri_max_y_i),
    .tri_src_id_i(tri_src_id_i),
    .tok_req_o(old_tok_req_o), .tok_grant_i(tok_grant_i),
    .job_valid_o(old_job_valid_o), .job_ready_i(job_ready_i),
    .job_ax_o(old_job_ax_o), .job_ay_o(old_job_ay_o),
    .job_bx_o(old_job_bx_o), .job_by_o(old_job_by_o),
    .job_cx_o(old_job_cx_o), .job_cy_o(old_job_cy_o),
    .job_first_o(old_job_first_o), .job_last_o(old_job_last_o),
    .job_tile_x_o(old_job_tile_x_o), .job_tile_y_o(old_job_tile_y_o),
    .job_src_id_o(old_job_src_id_o),
    .drain_busy_o(old_drain_busy_o), .drain_done_o(old_drain_done_o),
    .tile_references_o(old_tile_references_o),
    .max_tile_list_depth_o(old_max_tile_list_depth_o),
    .triangles_culled_o(old_triangles_culled_o), .overflow_o(old_overflow_o),
    .arena_full_o(old_arena_full_o), .arena_used_o(old_arena_used_o)
  );

  // Local, not a bench port: the V1 half has no counterpart, so adding a

  // port here would change the C++ harness interface for a signal the

  // differential does not compare.

  logic [1:0] v2_job_profile_bad_w;


  zhao_geom_binner_v2 #(
    .GRID_W(GRID_W), .GRID_H(GRID_H), .TILES(TILES), .TIDX_W(TIDX_W),
    .TRI_CAP(TRI_CAP), .TRI_W(TRI_W), .CHUNKS(CHUNKS),
    .CHUNK_W(CHUNK_W), .CHUNK_REFS(CHUNK_REFS), .METAW(METAW)
  ) u_v2 (
    .clk(clk), .rst_n(rst_n),
    .frame_begin_i(frame_begin_i), .frame_end_i(frame_end_i),
    .grid_w_i(grid_w_i), .grid_h_i(grid_h_i),
    .tri_valid_i(tri_valid_i), .tri_ready_o(v2_tri_ready_o),
    .tri_kx0_i(tri_kx0_i), .tri_ky0_i(tri_ky0_i), .tri_kc0_i(tri_kc0_i),
    .tri_kx1_i(tri_kx1_i), .tri_ky1_i(tri_ky1_i), .tri_kc1_i(tri_kc1_i),
    .tri_kx2_i(tri_kx2_i), .tri_ky2_i(tri_ky2_i), .tri_kc2_i(tri_kc2_i),
    .tri_tl_i(tri_tl_i),
    .tri_ax_i(tri_ax_i), .tri_ay_i(tri_ay_i),
    .tri_bx_i(tri_bx_i), .tri_by_i(tri_by_i),
    .tri_cx_i(tri_cx_i), .tri_cy_i(tri_cy_i),
    .tri_min_x_i(tri_min_x_i), .tri_max_x_i(tri_max_x_i),
    .tri_min_y_i(tri_min_y_i), .tri_max_y_i(tri_max_y_i),
    .tri_src_id_i(tri_src_id_i), .tri_meta_i(tri_meta_i),
    .tok_req_o(v2_tok_req_o), .tok_grant_i(tok_grant_i),
    .job_valid_o(v2_job_valid_o), .job_ready_i(job_ready_i),
    .job_ax_o(v2_job_ax_o), .job_ay_o(v2_job_ay_o),
    .job_bx_o(v2_job_bx_o), .job_by_o(v2_job_by_o),
    .job_cx_o(v2_job_cx_o), .job_cy_o(v2_job_cy_o),
    .job_first_o(v2_job_first_o), .job_last_o(v2_job_last_o),
    .job_tile_x_o(v2_job_tile_x_o), .job_tile_y_o(v2_job_tile_y_o),
    .job_src_id_o(v2_job_src_id_o), .job_meta_o(v2_job_meta_o),
      // The V1 binner has no profile verdict to compare against, so this
      // pair bench does not difference it. Connected explicitly rather than
      // left off: an omitted port is a PINMISSING the next fit discovers.
      .job_profile_bad_o(v2_job_profile_bad_w),
    // The serialise pass is NOT REQUESTED in this bench, deliberately. This
    // pair proves the V2 transport is the unversioned binner's, cycle for
    // cycle; with `ser_req_i` low, `ser_mode_r` can never set and the raster
    // drain is the only walk there is -- which is the property being asserted,
    // not a way of dodging the new port.
    .drain_busy_o(v2_drain_busy_o), .drain_done_o(v2_drain_done_o),
    .tile_references_o(v2_tile_references_o),
    .max_tile_list_depth_o(v2_max_tile_list_depth_o),
    .triangles_culled_o(v2_triangles_culled_o), .overflow_o(v2_overflow_o),
    .arena_full_o(v2_arena_full_o), .arena_used_o(v2_arena_used_o)
  );

endmodule : tb_geom_binner_v2_pair

`default_nettype wire
