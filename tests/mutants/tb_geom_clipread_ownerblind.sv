// tb_geom_clipread_ownerblind.sv -- the flattening bench for
// `zhao_geom_clipread_ownerblind_mutant`, the owner ruling of 2026-09-21's
// test J.
//
// It is NOT a copy of `tests/geometry/tb_geom_clipread.sv`: it carries only
// the ports the inverted-polarity driver reads, so it cannot go stale in the
// flattering direction by holding an old view of a port this fire test never
// looks at. The two packed-struct ports are flattened for the same reason the
// production bench flattens them -- a driver that pokes `g_req_o[97:71]` still
// compiles after somebody inserts a field.
`default_nettype none

module tb_geom_clipread_ownerblind
  import zhao_pkg::*;
(
    input  var logic clk,
    input  var logic rst_n,

    input  var logic        pub_valid_i,
    input  var logic [ 7:0] pub_tag_i,
    input  var logic [23:0] pub_index_i,
    input  var logic [15:0] pub_generation_i,
    input  var logic [31:0] pub_base_i,
    input  var logic [31:0] pub_extent_i,

    input  var logic        p_valid_i,
    output var logic        p_ready_o,
    input  var logic [23:0] p_form_idx_i,
    input  var logic [15:0] p_clip_id_i,
    input  var logic [15:0] p_frame_no_i,

    output var logic        req_valid_o,
    output var logic [26:0] req_addr_o,
    input  var logic        rsp_ready_i,
    input  var logic        rsp_ok_i,
    input  var logic        rsp_violation_i,
    input  var logic        beat_valid_i,
    input  var logic [63:0] beat_data_i,

    output var logic        fill_we_o,
    output var logic        fill_sel_o,
    output var logic [ 4:0] fill_bone_o,
    output var logic [ 3:0] fill_word_o,
    output var logic [63:0] fill_data_o,

    output var logic        src_req_o,
    output var logic [ 5:0] src_bone_count_o,
    input  var logic        src_ready_i,

    output var logic [23:0] res_body_owner_o,
    output var logic [23:0] res_clip_owner_o,

    output var logic [31:0] bodies_o,
    output var logic [31:0] clips_o,
    output var logic [31:0] frames_o,
    output var logic [31:0] bone_mismatch_o,
    output var logic [31:0] not_resident_o,
    output var logic [31:0] owner_mismatch_o,
    output var logic        busy_o
);

  zhao_guard_req_t req_c;
  zhao_guard_rsp_t rsp_c;

  assign req_valid_o = req_c.valid;
  assign req_addr_o  = req_c.addr;

  always_comb begin
    rsp_c           = '0;
    rsp_c.ready     = rsp_ready_i;
    rsp_c.ok        = rsp_ok_i;
    rsp_c.violation = rsp_violation_i;
  end

  // Everything this bench does not read is left unconnected deliberately; the
  // fire test asks exactly one question.
  /* verilator lint_off PINCONNECTEMPTY */
  zhao_geom_clipread_ownerblind_mutant u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .pub_valid_i     (pub_valid_i),
      .pub_tag_i       (pub_tag_i),
      .pub_index_i     (pub_index_i),
      .pub_generation_i(pub_generation_i),
      .pub_base_i      (pub_base_i),
      .pub_extent_i    (pub_extent_i),

      .p_valid_i   (p_valid_i),
      .p_ready_o   (p_ready_o),
      .p_form_idx_i(p_form_idx_i),
      .p_clip_id_i (p_clip_id_i),
      .p_frame_no_i(p_frame_no_i),

      .g_req_o       (req_c),
      .g_rsp_i       (rsp_c),
      .g_beat_valid_i(beat_valid_i),
      .g_beat_data_i (beat_data_i),

      .fill_we_o  (fill_we_o),
      .fill_sel_o (fill_sel_o),
      .fill_bone_o(fill_bone_o),
      .fill_word_o(fill_word_o),
      .fill_data_o(fill_data_o),

      .src_req_o       (src_req_o),
      .src_bone_count_o(src_bone_count_o),
      .src_ready_i     (src_ready_i),

      .root_dx_o(),
      .root_dy_o(),
      .root_dz_o(),

      .res_body_index_o(),
      .res_body_gen_o  (),
      .res_clip_index_o(),
      .res_clip_gen_o  (),
      .res_body_owner_o(res_body_owner_o),
      .res_clip_owner_o(res_clip_owner_o),

      .bodies_o        (bodies_o),
      .clips_o         (clips_o),
      .frames_o        (frames_o),
      .pages_dropped_o (),
      .bad_magic_o     (),
      .truncated_o     (),
      .misaligned_o    (),
      .bad_bone_count_o(),
      .bone_mismatch_o (bone_mismatch_o),
      .overflow_o      (),
      .not_rigid_o     (),
      .reserved_nz_o   (),
      .denied_o        (),
      .clip_miss_o     (),
      .frame_oob_o     (),
      .not_resident_o  (not_resident_o),
      .owner_mismatch_o(owner_mismatch_o),
      .busy_o          (busy_o)
  );
  /* verilator lint_on PINCONNECTEMPTY */

endmodule : tb_geom_clipread_ownerblind

`default_nettype wire
