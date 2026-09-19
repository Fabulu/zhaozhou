// tb_terrain_lightlane.sv -- zhao_terrain_lightlane with its ports flattened,
// so tests/terrain/terrain_lightlane_directed.cpp names fields. Testbench only.
`default_nettype none

module tb_terrain_lightlane #(
    parameter int unsigned ARENAS = 4,
    parameter int unsigned DEPTH  = 81
) (
    input  var logic clk,
    input  var logic rst_n,

    input  var logic        fill_valid,
    input  var logic        fill_ready,
    input  var logic [ 2:0] fill_arena,
    input  var logic [ 7:0] fill_index,
    input  var logic [31:0] fill_vx,
    input  var logic [31:0] fill_vy,
    input  var logic [31:0] fill_vz,

    input  var logic        open_v,
    input  var logic [ 2:0] open_arena,
    input  var logic [ 7:0] open_gen,

    input  var logic        ref_valid,
    output var logic        ref_ready,
    input  var logic [ 2:0] ref_arena,
    input  var logic [ 7:0] ref_gen,
    input  var logic [ 7:0] ref_ia,
    input  var logic [ 7:0] ref_ib,
    input  var logic [ 7:0] ref_ic,
    input  var logic [15:0] ref_src,

    input  var logic [31:0] sun_x,
    input  var logic [31:0] sun_y,
    input  var logic [31:0] sun_z,

    output var logic        light_valid,
    input  var logic        light_ready,
    output var logic [31:0] light_base,
    output var logic        light_degenerate,
    output var logic [15:0] light_src,

    output var logic [31:0] refs_taken,
    output var logic [31:0] lights_emitted,
    output var logic [31:0] stale_reads,
    output var logic [31:0] normals_evaluated,
    output var logic [31:0] triangles_shaded,
    output var logic [31:0] degenerate_count,
    output var logic [31:0] base_sat,
    output var logic [31:0] degen_mismatch,
    output var logic        lane_idle
);

  logic signed [31:0] base_s;
  assign light_base = base_s;

  zhao_terrain_lightlane #(
      .ARENAS (ARENAS),
      .DEPTH  (DEPTH)
  ) u_dut (
      .clk         (clk),
      .rst_n       (rst_n),
      .fill_valid_i(fill_valid),
      .fill_ready_i(fill_ready),
      .fill_arena_i(fill_arena),
      .fill_index_i(fill_index),
      .fill_vx_i   (signed'(fill_vx)),
      .fill_vy_i   (signed'(fill_vy)),
      .fill_vz_i   (signed'(fill_vz)),
      .open_i      (open_v),
      .open_arena_i(open_arena),
      .open_gen_i  (open_gen),
      .ref_valid_i (ref_valid),
      .ref_ready_o (ref_ready),
      .ref_arena_i (ref_arena),
      .ref_gen_i   (ref_gen),
      .ref_ia_i    (ref_ia),
      .ref_ib_i    (ref_ib),
      .ref_ic_i    (ref_ic),
      .ref_src_id_i(ref_src),
      .sun_x_i     (signed'(sun_x)),
      .sun_y_i     (signed'(sun_y)),
      .sun_z_i     (signed'(sun_z)),
      .light_valid_o     (light_valid),
      .light_ready_i     (light_ready),
      .light_base_o      (base_s),
      .light_degenerate_o(light_degenerate),
      .light_src_id_o    (light_src),
      .refs_taken_o       (refs_taken),
      .lights_emitted_o   (lights_emitted),
      .stale_reads_o      (stale_reads),
      .normals_evaluated_o(normals_evaluated),
      .triangles_shaded_o (triangles_shaded),
      .degenerate_count_o (degenerate_count),
      .base_sat_o         (base_sat),
      .degen_mismatch_o   (degen_mismatch),
      .idle_o             (lane_idle)
  );

endmodule

`default_nettype wire
