// tb_material_combine_v3_copy_readlate.sv -- paired copy/read-late R9 fixture.
//
// Test-only.  One atomic input is accepted by both real V3 combiners.  The copy
// instance captures typed planes on its pins; the READ_LATE instance receives
// the same planes from four real zhao_texture_v3bank instances addressed by its
// source request.  Independent output ready inputs exercise each held edge.
`default_nettype none

module tb_material_combine_v3_copy_readlate #(
    parameter int NCTX = 8,
    parameter int TAGW = 14,
    parameter int SLOTW = 6,
    parameter int PLANE_DEPTH = 64
) (
    input  var logic clk,
    input  var logic rst_n,

    input  var logic f_valid_i,
    output var logic f_ready_o,
    input  var logic [1:0] f_sample_count_i,
    input  var logic [2:0] f_recipe_i,
    input  var logic [7:0] f_weight_i,
    input  var logic f_aux_required_i,
    input  var logic [23:0] f_base_rgb_i,
    input  var logic [7:0] f_base_a_i,
    input  var logic [TAGW-1:0] f_tag_i,
    input  var logic [SLOTW-1:0] f_slot_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] f_s0_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] f_s1_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] f_s2_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] f_aux_i,

    // Test-side owner-plane writer: one selected plane per cycle.
    input  var logic pw_en_i,
    input  var logic [1:0] pw_lane_i,
    input  var logic [SLOTW-1:0] pw_slot_i,
    input  var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] pw_data_i,

    output var logic copy_valid_o,
    input  var logic copy_ready_i,
    output var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] copy_result_o,
    output var logic [TAGW-1:0] copy_tag_o,
    output var logic copy_idle_o,

    output var logic late_valid_o,
    input  var logic late_ready_i,
    output var logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0] late_result_o,
    output var logic [TAGW-1:0] late_tag_o,
    output var logic late_idle_o,
    output var logic late_src_rd_valid_o,
    output var logic [SLOTW-1:0] late_src_rd_slot_o,

    output var logic [31:0] copy_jobs_accepted_o,
    output var logic [31:0] copy_jobs_completed_o,
    output var logic [31:0] copy_phases_issued_o,
    output var logic [31:0] copy_phases_completed_o,
    output var logic [31:0] copy_saturated_add_o,
    output var logic [31:0] copy_saturated_mul2x_o,
    output var logic [31:0] copy_jobs_by_recipe_o [8],

    output var logic [31:0] late_jobs_accepted_o,
    output var logic [31:0] late_jobs_completed_o,
    output var logic [31:0] late_phases_issued_o,
    output var logic [31:0] late_phases_completed_o,
    output var logic [31:0] late_saturated_add_o,
    output var logic [31:0] late_saturated_mul2x_o,
    output var logic [31:0] late_jobs_by_recipe_o [8]
);

  import zhao_render_texture_pkg::*;

  logic copy_input_ready, late_input_ready;
  wire pair_accept_c = f_valid_i && f_ready_o;
  assign f_ready_o = copy_input_ready && late_input_ready;
  wire copy_input_valid_c = f_valid_i && late_input_ready;
  wire late_input_valid_c = f_valid_i && copy_input_ready;

  // Registered bank writes preserve the owner's production write-enable shape.
  logic [3:0] pw_we_q;
  logic [SLOTW-1:0] pw_slot_q;
  logic [TEXTURE_RESULT_W-1:0] pw_data_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) pw_we_q <= 4'b0000;
    else pw_we_q <= pw_en_i ? (4'b0001 << pw_lane_i) : 4'b0000;
  end
  always_ff @(posedge clk) begin
    pw_slot_q <= pw_slot_i;
    pw_data_q <= pw_data_i;
  end

  logic [TEXTURE_RESULT_W-1:0] plane_rd [4];
  logic [SLOTW-1:0] late_rd_slot;
  generate
    genvar p;
    for (p = 0; p < 4; p++) begin : g_plane
      zhao_texture_v3bank #(
          .WIDTH(TEXTURE_RESULT_W), .DEPTH(PLANE_DEPTH), .AW(SLOTW)
      ) u_plane (
          .clk(clk), .wr_en_i(pw_we_q[p]), .wr_addr_i(pw_slot_q),
          .wr_data_i(pw_data_q), .rd_addr_i(late_rd_slot),
          .rd_data_o(plane_rd[p]));
    end
  endgenerate

  /* verilator lint_off UNUSEDSIGNAL */
  logic copy_src_valid_unused;
  logic [SLOTW-1:0] copy_src_slot_unused;
  logic [23:0] copy_rgb_unused, late_rgb_unused;
  logic [7:0] copy_a_unused, copy_index_unused, copy_status_unused;
  logic [7:0] late_a_unused, late_index_unused, late_status_unused;
  logic copy_refused_unused, late_refused_unused;
  logic [31:0] copy_refused_material_unused, late_refused_material_unused;
  logic [31:0] copy_recipe_alias_unused [8];
  logic [31:0] late_recipe_alias_unused [8];
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_texture_material_combine_v3 #(
      .NCTX(NCTX), .TAGW(TAGW), .READ_LATE(0), .SLOTW(SLOTW)
  ) u_copy (
      .clk(clk), .rst_n(rst_n),
      .f_valid_i(copy_input_valid_c), .f_ready_o(copy_input_ready),
      .f_sample_count_i(f_sample_count_i), .f_recipe_i(f_recipe_i),
      .f_weight_i(f_weight_i), .f_aux_required_i(f_aux_required_i),
      .f_base_rgb_i(f_base_rgb_i), .f_base_a_i(f_base_a_i),
      .f_tag_i(f_tag_i), .f_s0_i(f_s0_i), .f_s1_i(f_s1_i),
      .f_s2_i(f_s2_i), .f_aux_i(f_aux_i), .f_slot_i('0),
      .src_s0_i('0), .src_s1_i('0), .src_s2_i('0), .src_aux_i('0),
      .src_rd_valid_o(copy_src_valid_unused),
      .src_rd_slot_o(copy_src_slot_unused),
      .o_valid_o(copy_valid_o), .o_ready_i(copy_ready_i),
      .o_result_o(copy_result_o), .o_rgb_o(copy_rgb_unused),
      .o_a_o(copy_a_unused), .o_raw_index_o(copy_index_unused),
      .o_status_o(copy_status_unused), .o_tag_o(copy_tag_o),
      .o_refused_o(copy_refused_unused), .idle_o(copy_idle_o),
      .refused_material_o(copy_refused_material_unused),
      .saturated_add_o(copy_saturated_add_o),
      .saturated_mul2x_o(copy_saturated_mul2x_o),
      .jobs_by_recipe_o(copy_jobs_by_recipe_o),
      .jobs_recipe_0_o(copy_recipe_alias_unused[0]),
      .jobs_recipe_1_o(copy_recipe_alias_unused[1]),
      .jobs_recipe_2_o(copy_recipe_alias_unused[2]),
      .jobs_recipe_3_o(copy_recipe_alias_unused[3]),
      .jobs_recipe_4_o(copy_recipe_alias_unused[4]),
      .jobs_recipe_5_o(copy_recipe_alias_unused[5]),
      .jobs_recipe_6_o(copy_recipe_alias_unused[6]),
      .jobs_recipe_7_o(copy_recipe_alias_unused[7]),
      .jobs_accepted_o(copy_jobs_accepted_o),
      .jobs_completed_o(copy_jobs_completed_o),
      .phases_issued_o(copy_phases_issued_o),
      .phases_completed_o(copy_phases_completed_o));

  zhao_texture_material_combine_v3 #(
      .NCTX(NCTX), .TAGW(TAGW), .READ_LATE(1), .SLOTW(SLOTW)
  ) u_late (
      .clk(clk), .rst_n(rst_n),
      .f_valid_i(late_input_valid_c), .f_ready_o(late_input_ready),
      .f_sample_count_i(f_sample_count_i), .f_recipe_i(f_recipe_i),
      .f_weight_i(f_weight_i), .f_aux_required_i(f_aux_required_i),
      .f_base_rgb_i(f_base_rgb_i), .f_base_a_i(f_base_a_i),
      .f_tag_i(f_tag_i), .f_s0_i('0), .f_s1_i('0),
      .f_s2_i('0), .f_aux_i('0), .f_slot_i(f_slot_i),
      .src_s0_i(plane_rd[0]), .src_s1_i(plane_rd[1]),
      .src_s2_i(plane_rd[2]), .src_aux_i(plane_rd[3]),
      .src_rd_valid_o(late_src_rd_valid_o),
      .src_rd_slot_o(late_rd_slot),
      .o_valid_o(late_valid_o), .o_ready_i(late_ready_i),
      .o_result_o(late_result_o), .o_rgb_o(late_rgb_unused),
      .o_a_o(late_a_unused), .o_raw_index_o(late_index_unused),
      .o_status_o(late_status_unused), .o_tag_o(late_tag_o),
      .o_refused_o(late_refused_unused), .idle_o(late_idle_o),
      .refused_material_o(late_refused_material_unused),
      .saturated_add_o(late_saturated_add_o),
      .saturated_mul2x_o(late_saturated_mul2x_o),
      .jobs_by_recipe_o(late_jobs_by_recipe_o),
      .jobs_recipe_0_o(late_recipe_alias_unused[0]),
      .jobs_recipe_1_o(late_recipe_alias_unused[1]),
      .jobs_recipe_2_o(late_recipe_alias_unused[2]),
      .jobs_recipe_3_o(late_recipe_alias_unused[3]),
      .jobs_recipe_4_o(late_recipe_alias_unused[4]),
      .jobs_recipe_5_o(late_recipe_alias_unused[5]),
      .jobs_recipe_6_o(late_recipe_alias_unused[6]),
      .jobs_recipe_7_o(late_recipe_alias_unused[7]),
      .jobs_accepted_o(late_jobs_accepted_o),
      .jobs_completed_o(late_jobs_completed_o),
      .phases_issued_o(late_phases_issued_o),
      .phases_completed_o(late_phases_completed_o));

  assign late_src_rd_slot_o = late_rd_slot;

  // The pair event is named for waveform/debug use; both valid equations make
  // it impossible for either instance to accept without the other.
  /* verilator lint_off UNUSEDSIGNAL */
  wire pair_accept_unused = pair_accept_c;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : tb_material_combine_v3_copy_readlate

`default_nettype wire
