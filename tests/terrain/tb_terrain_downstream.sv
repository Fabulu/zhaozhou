// tb_terrain_downstream.sv — GEOM.CLIP chained into GEOM.SETUP, no adapter,
// for the ONE question adoption report §7 item 4 asks and nothing measured:
// can the blocks downstream of the terrain arena take one triangle per clock?
//
// The terrain shell (`zhao_terrain_wcache`) emits `zhao_terrain_project`'s
// packet, which IS `zhao_geom_clip`'s input packet (TERRAIN.PROJECT.md: "its
// output packet IS zhao_geom_clip's input packet"), and GEOM.CLIP's output IS
// GEOM.SETUP's input. This bench wires exactly those two, drives the front
// port from C++ at one triangle per clock, and counts. Both contracts CLAIM
// "one triangle per clock, met"; tests/terrain/terrain_downstream_rate.cpp
// turns the claim into a number.
//
// ATTRS = 1: the attribute lanes ride through GEOM.CLIP untouched except for
// the winding swap and have no bearing on rate; one lane keeps the bench port
// list short and still exercises the swap path.
//
// GEOM.BINNER is deliberately NOT in this bench: its rate is per TILE
// REFERENCE, it holds `tri_ready_o` low for a whole enumeration, and it has a
// frame protocol (frame_begin / tokens / drain). It is measured by its own
// driver in terrain_downstream_binner.cpp.

`default_nettype none

module tb_terrain_downstream (
    input  wire clk,
    input  wire rst_n,

    // ---- GEOM.CLIP's input: the terrain shell's packet ------------------
    input  wire               tri_valid_i,
    output wire               tri_ready_o,
    input  wire signed [20:0] tri_ax_i,
    input  wire signed [20:0] tri_ay_i,
    input  wire signed [20:0] tri_bx_i,
    input  wire signed [20:0] tri_by_i,
    input  wire signed [20:0] tri_cx_i,
    input  wire signed [20:0] tri_cy_i,
    input  wire        [ 2:0] tri_behind_i,
    input  wire        [15:0] tri_src_id_i,
    input  wire        [31:0] tri_attr_a_i,
    input  wire        [31:0] tri_attr_b_i,
    input  wire        [31:0] tri_attr_c_i,
    input  wire        [11:0] vp_x0_i,
    input  wire        [11:0] vp_y0_i,
    input  wire        [11:0] vp_w_i,
    input  wire        [11:0] vp_h_i,
    input  wire        [ 1:0] cull_mode_i,

    // ---- GEOM.CLIP's verdict stream (every triangle retires here) --------
    output wire               ret_valid_o,
    output wire        [ 2:0] ret_verdict_o,

    // ---- GEOM.SETUP's output: what GEOM.BINNER would consume --------------
    output wire               out_valid_o,
    input  wire               out_ready_i,
    output wire        [15:0] out_src_id_o,
    output wire signed [47:0] out_area2_o,

    // ---- counters ---------------------------------------------------------
    output wire        [31:0] clip_submitted_o,
    output wire        [31:0] clip_clipped_o,
    output wire        [31:0] clip_culled_o,
    output wire        [31:0] setup_submitted_o
);

  // ---- the seam: GEOM.CLIP out == GEOM.SETUP in ---------------------------
  wire               c_valid, c_ready;
  wire signed [20:0] c_ax, c_ay, c_bx, c_by, c_cx, c_cy;
  wire signed [47:0] c_area2;
  wire signed [11:0] c_min_x, c_max_x, c_min_y, c_max_y;
  wire        [15:0] c_src;
  wire        [31:0] c_attr_a_unused, c_attr_b_unused, c_attr_c_unused;
  wire               c_flip_unused;

  zhao_geom_clip #(.ATTRS(1)) u_clip (
      .clk                  (clk),
      .rst_n                (rst_n),
      .tri_valid_i          (tri_valid_i),
      .tri_ready_o          (tri_ready_o),
      .tri_ax_i             (tri_ax_i),
      .tri_ay_i             (tri_ay_i),
      .tri_bx_i             (tri_bx_i),
      .tri_by_i             (tri_by_i),
      .tri_cx_i             (tri_cx_i),
      .tri_cy_i             (tri_cy_i),
      .tri_behind_i         (tri_behind_i),
      .tri_src_id_i         (tri_src_id_i),
      .tri_attr_a_i         (tri_attr_a_i),
      .tri_attr_b_i         (tri_attr_b_i),
      .tri_attr_c_i         (tri_attr_c_i),
      .vp_x0_i              (vp_x0_i),
      .vp_y0_i              (vp_y0_i),
      .vp_w_i               (vp_w_i),
      .vp_h_i               (vp_h_i),
      .cull_mode_i          (cull_mode_i),
      .out_valid_o          (c_valid),
      .out_ready_i          (c_ready),
      .out_ax_o             (c_ax),
      .out_ay_o             (c_ay),
      .out_bx_o             (c_bx),
      .out_by_o             (c_by),
      .out_cx_o             (c_cx),
      .out_cy_o             (c_cy),
      .out_area2_o          (c_area2),
      .out_min_x_o          (c_min_x),
      .out_max_x_o          (c_max_x),
      .out_min_y_o          (c_min_y),
      .out_max_y_o          (c_max_y),
      .out_src_id_o         (c_src),
      .out_attr_a_o         (c_attr_a_unused),
      .out_attr_b_o         (c_attr_b_unused),
      .out_attr_c_o         (c_attr_c_unused),
      .out_flip_o           (c_flip_unused),
      .ret_valid_o          (ret_valid_o),
      .ret_verdict_o        (ret_verdict_o),
      .triangles_submitted_o(clip_submitted_o),
      .triangles_clipped_o  (clip_clipped_o),
      .triangles_culled_o   (clip_culled_o)
  );

  wire signed [22:0] s_kx0_unused, s_ky0_unused, s_kx1_unused, s_ky1_unused, s_kx2_unused, s_ky2_unused;
  wire signed [47:0] s_kc0_unused, s_kc1_unused, s_kc2_unused;
  wire        [ 2:0] s_tl_unused;
  wire signed [20:0] s_ax_unused, s_ay_unused, s_bx_unused, s_by_unused, s_cx_unused, s_cy_unused;
  wire signed [11:0] s_min_x_unused, s_max_x_unused, s_min_y_unused, s_max_y_unused;

  zhao_geom_setup u_setup (
      .clk                  (clk),
      .rst_n                (rst_n),
      .tri_valid_i          (c_valid),
      .tri_ready_o          (c_ready),
      .tri_ax_i             (c_ax),
      .tri_ay_i             (c_ay),
      .tri_bx_i             (c_bx),
      .tri_by_i             (c_by),
      .tri_cx_i             (c_cx),
      .tri_cy_i             (c_cy),
      .tri_area2_i          (c_area2),
      .tri_min_x_i          (c_min_x),
      .tri_max_x_i          (c_max_x),
      .tri_min_y_i          (c_min_y),
      .tri_max_y_i          (c_max_y),
      .tri_src_id_i         (c_src),
      .out_valid_o          (out_valid_o),
      .out_ready_i          (out_ready_i),
      .out_kx0_o            (s_kx0_unused),
      .out_ky0_o            (s_ky0_unused),
      .out_kc0_o            (s_kc0_unused),
      .out_kx1_o            (s_kx1_unused),
      .out_ky1_o            (s_ky1_unused),
      .out_kc1_o            (s_kc1_unused),
      .out_kx2_o            (s_kx2_unused),
      .out_ky2_o            (s_ky2_unused),
      .out_kc2_o            (s_kc2_unused),
      .out_tl_o             (s_tl_unused),
      .out_area2_o          (out_area2_o),
      .out_ax_o             (s_ax_unused),
      .out_ay_o             (s_ay_unused),
      .out_bx_o             (s_bx_unused),
      .out_by_o             (s_by_unused),
      .out_cx_o             (s_cx_unused),
      .out_cy_o             (s_cy_unused),
      .out_min_x_o          (s_min_x_unused),
      .out_max_x_o          (s_max_x_unused),
      .out_min_y_o          (s_min_y_unused),
      .out_max_y_o          (s_max_y_unused),
      .out_src_id_o         (out_src_id_o),
      .triangles_submitted_o(setup_submitted_o)
  );

endmodule : tb_terrain_downstream

`default_nettype wire
