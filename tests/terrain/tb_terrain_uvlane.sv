// tb_terrain_uvlane.sv -- zhao_terrain_uvlane with its ports flattened, so
// tests/terrain/terrain_uvlane_directed.cpp names fields. Testbench only.
`default_nettype none

module tb_terrain_uvlane #(
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
    input  var logic [31:0] fill_vz,
    input  var logic        fill_surface,
    input  var logic [ 7:0] pitch_log2,

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

    output var logic        uv_valid,
    input  var logic        uv_ready,
    output var logic [31:0] uv_au,
    output var logic [31:0] uv_av,
    output var logic [31:0] uv_bu,
    output var logic [31:0] uv_bv,
    output var logic [31:0] uv_cu,
    output var logic [31:0] uv_cv,
    output var logic [15:0] uv_src,

    output var logic [31:0] refs_taken,
    output var logic [31:0] uvs_emitted,
    output var logic [31:0] stale_reads,
    output var logic [31:0] pitch_clamped,
    output var logic [31:0] pitch_illegal,
    output var logic        idle
);

  zhao_terrain_uvlane #(
      .ARENAS (ARENAS),
      .DEPTH  (DEPTH),
      .GEN_W  (8),
      .SRCW   (16)
  ) u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .fill_valid_i  (fill_valid),
      .fill_ready_i  (fill_ready),
      .fill_arena_i  (fill_arena),
      .fill_index_i  (fill_index),
      .fill_vx_i     (signed'(fill_vx)),
      .fill_vz_i     (signed'(fill_vz)),
      .fill_surface_i(fill_surface),
      .pitch_log2_i  (signed'(pitch_log2)),

      .open_i      (open_v),
      .open_arena_i(open_arena),
      .open_gen_i  (open_gen),

      .ref_valid_i  (ref_valid),
      .ref_ready_o  (ref_ready),
      .ref_arena_i  (ref_arena),
      .ref_gen_i    (ref_gen),
      .ref_ia_i     (ref_ia),
      .ref_ib_i     (ref_ib),
      .ref_ic_i     (ref_ic),
      .ref_src_id_i (ref_src),

      .uv_valid_o  (uv_valid),
      .uv_ready_i  (uv_ready),
      .uv_au_o     (uv_au),
      .uv_av_o     (uv_av),
      .uv_bu_o     (uv_bu),
      .uv_bv_o     (uv_bv),
      .uv_cu_o     (uv_cu),
      .uv_cv_o     (uv_cv),
      .uv_src_id_o (uv_src),

      .refs_taken_o   (refs_taken),
      .uvs_emitted_o  (uvs_emitted),
      .stale_reads_o  (stale_reads),
      .pitch_clamped_o(pitch_clamped),
      .pitch_illegal_o(pitch_illegal),
      .idle_o         (idle)
  );

endmodule

`default_nettype wire
