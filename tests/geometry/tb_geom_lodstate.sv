// tb_geom_lodstate.sv -- the WHOLE R68 chain, every block REAL:
//
//   zhao_view_projscale  (the camera's kx and viewport width, snooped)
//   zhao_geom_ladderbank (the four constants, loaded from a CREATURE_FORM page)
//   zhao_geom_lodstate   (the per-instance ladder state)
//     |- zhao_geom_projradius  (the projected bound radius)
//     '- zhao_geom_lod         (the ladder itself)
//
// The driver supplies only what genuinely comes from OUTSIDE this chain: the
// projector configuration bus, the asset read window, the job tap, the shared
// projector's `w` for the instance centre (owner ruling R3 keeps client A a
// time-multiplex, so the multiplexer is the composer's), and the governor's
// per-camera threshold.
//
// Composing them here rather than driving `zhao_geom_lodstate`'s bank and
// radius ports from C++ is the point: a bank that answered the wrong row, a
// snooper that read the wrong matrix element, or a radius that used the wrong
// view's viewport would all pass a test that handed them the answer.
`default_nettype none

module tb_geom_lodstate
  import zhao_pkg::*;
#(
    parameter int unsigned INSTANCES = 8,
    parameter int unsigned ROWS = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the projector configuration bus ------------------------------------
    input  var logic        cfg_we_i,
    input  var logic        cfg_view_i,
    input  var logic [ 4:0] cfg_addr_i,
    input  var logic [31:0] cfg_data_i,

    // ---- MEM.UPLOAD's publication, into the ladder bank ---------------------
    input  var logic        pub_valid_i,
    input  var logic [ 7:0] pub_tag_i,
    input  var logic [31:0] pub_base_i,
    input  var logic [31:0] pub_extent_i,

    // ---- the asset read window the bank reads through -----------------------
    output var logic        req_valid_o,
    output var logic [26:0] req_addr_o,
    output var logic [ 6:0] req_len_o,
    input  var logic        rsp_ready_i,
    input  var logic        rsp_ok_i,
    input  var logic        rsp_violation_i,
    input  var logic        beat_valid_i,
    input  var logic [63:0] beat_data_i,

    output var logic [31:0] bank_pages_o,
    output var logic [31:0] bank_records_o,

    // ---- the frame boundary and the job tap ---------------------------------
    input  var logic               frame_i,
    input  var logic               j_fire_i,
    input  var logic        [15:0] j_instance_id_i,
    input  var logic        [23:0] j_form_index_i,
    input  var logic signed [31:0] j_cx_i,
    input  var logic signed [31:0] j_cy_i,
    input  var logic signed [31:0] j_cz_i,
    // OWNER RULING R74 / D-LADDER-A: the two-view MASK, as zhao_geom_drawjob
    // emits it. A job with both bits set is evaluated twice, once per camera.
    input  var logic        [ 1:0] j_view_mask_i,

    // ---- the instance centre's projection, from client A --------------------
    output var logic               pr_valid_o,
    input  var logic               pr_ready_i,
    output var logic signed [31:0] pr_x_o,
    output var logic signed [31:0] pr_y_o,
    output var logic signed [31:0] pr_z_o,
    output var logic               pr_view_o,
    input  var logic               pr_ans_valid_i,
    input  var logic        [30:0] pr_w_i,
    input  var logic               pr_behind_i,

    // ---- the governor's per-camera threshold --------------------------------
    input  var logic signed [31:0] thresh0_i,
    input  var logic signed [31:0] thresh1_i,

    // ---- the caster out -----------------------------------------------------
    output var logic               c_valid_o,
    input  var logic               c_ready_i,
    output var logic        [15:0] c_instance_id_o,
    output var logic signed [31:0] c_x_o,
    output var logic signed [31:0] c_z_o,
    output var logic signed [31:0] c_radius_o,
    output var logic        [ 1:0] c_rung_o,
    output var logic               c_view_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] ticks_o,
    output var logic [31:0] skipped_repeat_o,
    output var logic [31:0] bank_miss_o,
    output var logic [31:0] no_radius_o,
    output var logic [31:0] dropped_o,
    output var logic [31:0] out_of_range_o,
    output var logic [31:0] rung0_o,
    output var logic [31:0] rung1_o,
    output var logic [31:0] rung2_o,
    output var logic [31:0] rung3_o,
    output var logic [31:0] rad_evaluations_o,
    output var logic [31:0] rad_behind_o,
    output var logic [31:0] rad_bad_bound_o,
    output var logic [31:0] rad_saturated_o,
    output var logic        busy_o,
    output var logic [31:0] kx0_o,
    output var logic [11:0] vw0_o
);

  // ---- the snooper ---------------------------------------------------------
  logic [31:0] kx0_w, kx1_w;
  logic [11:0] vw0_w, vw1_w;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] abs_sat_w;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_view_projscale u_scale (
      .clk  (clk),
      .rst_n(rst_n),
      .cfg_we_i  (cfg_we_i),
      .cfg_view_i(cfg_view_i),
      .cfg_addr_i(cfg_addr_i),
      .cfg_data_i(cfg_data_i),
      .kx0_o(kx0_w),
      .kx1_o(kx1_w),
      .vw0_o(vw0_w),
      .vw1_o(vw1_w),
      .abs_saturated_o(abs_sat_w)
  );

  assign kx0_o = kx0_w;
  assign vw0_o = vw0_w;

  // ---- the ladder bank -----------------------------------------------------
  zhao_guard_req_t bank_req_c;
  zhao_guard_rsp_t bank_rsp_c;

  assign req_valid_o = bank_req_c.valid;
  assign req_addr_o  = bank_req_c.addr;
  assign req_len_o   = bank_req_c.len;

  always_comb begin
    bank_rsp_c           = '0;
    bank_rsp_c.ready     = rsp_ready_i;
    bank_rsp_c.ok        = rsp_ok_i;
    bank_rsp_c.violation = rsp_violation_i;
  end

  logic               q_valid_w;
  logic [23:0]        q_form_w;
  logic               a_valid_w, a_hit_w;
  logic signed [31:0] a_bound_w, a_micro_w, a_splat_w, a_glint_w;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] bank_dropped_w, bank_bad_magic_w, bank_trunc_w, bank_badrec_w;
  logic [31:0] bank_over_w, bank_denied_w, bank_miss2_w;
  logic        bank_busy_w;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_geom_ladderbank #(
      .ROWS(ROWS)
  ) u_bank (
      .clk  (clk),
      .rst_n(rst_n),

      .pub_valid_i (pub_valid_i),
      .pub_tag_i   (pub_tag_i),
      .pub_base_i  (pub_base_i),
      .pub_extent_i(pub_extent_i),

      .g_req_o       (bank_req_c),
      .g_rsp_i       (bank_rsp_c),
      .g_beat_valid_i(beat_valid_i),
      .g_beat_data_i (beat_data_i),

      .q_valid_i(q_valid_w),
      .q_form_i (q_form_w),
      .a_valid_o(a_valid_w),
      .a_hit_o  (a_hit_w),
      .a_bound_o(a_bound_w),
      .a_micro_o(a_micro_w),
      .a_splat_o(a_splat_w),
      .a_glint_o(a_glint_w),

      .pages_o        (bank_pages_o),
      .records_o      (bank_records_o),
      .pages_dropped_o(bank_dropped_w),
      .bad_magic_o    (bank_bad_magic_w),
      .truncated_o    (bank_trunc_w),
      .bad_record_o   (bank_badrec_w),
      .overflow_o     (bank_over_w),
      .denied_o       (bank_denied_w),
      .lookup_miss_o  (bank_miss2_w),
      .busy_o         (bank_busy_w)
  );

  // ---- the ladder state owner ---------------------------------------------
  logic [31:0] rungs_w [4];
  assign rung0_o = rungs_w[0];
  assign rung1_o = rungs_w[1];
  assign rung2_o = rungs_w[2];
  assign rung3_o = rungs_w[3];

  zhao_geom_lodstate #(
      .INSTANCES(INSTANCES)
  ) u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .frame_i(frame_i),

      .j_fire_i       (j_fire_i),
      .j_instance_id_i(j_instance_id_i),
      .j_form_index_i (j_form_index_i),
      .j_cx_i         (j_cx_i),
      .j_cy_i         (j_cy_i),
      .j_cz_i         (j_cz_i),
      .j_view_mask_i  (j_view_mask_i),

      .q_valid_o(q_valid_w),
      .q_form_o (q_form_w),
      .a_valid_i(a_valid_w),
      .a_hit_i  (a_hit_w),
      .a_bound_i(a_bound_w),
      .a_micro_i(a_micro_w),
      .a_splat_i(a_splat_w),
      .a_glint_i(a_glint_w),

      .pr_valid_o    (pr_valid_o),
      .pr_ready_i    (pr_ready_i),
      .pr_x_o        (pr_x_o),
      .pr_y_o        (pr_y_o),
      .pr_z_o        (pr_z_o),
      .pr_view_o     (pr_view_o),
      .pr_ans_valid_i(pr_ans_valid_i),
      .pr_w_i        (pr_w_i),
      .pr_behind_i   (pr_behind_i),

      .kx0_i(kx0_w),
      .kx1_i(kx1_w),
      .vw0_i(vw0_w),
      .vw1_i(vw1_w),

      .thresh0_i(thresh0_i),
      .thresh1_i(thresh1_i),

      .c_valid_o      (c_valid_o),
      .c_ready_i      (c_ready_i),
      .c_instance_id_o(c_instance_id_o),
      .c_x_o          (c_x_o),
      .c_z_o          (c_z_o),
      .c_radius_o     (c_radius_o),
      .c_rung_o       (c_rung_o),
      .c_view_o       (c_view_o),

      .ticks_o          (ticks_o),
      .skipped_repeat_o (skipped_repeat_o),
      .bank_miss_o      (bank_miss_o),
      .no_radius_o      (no_radius_o),
      .dropped_o        (dropped_o),
      .out_of_range_o   (out_of_range_o),
      .rung_counts_o    (rungs_w),
      .rad_evaluations_o(rad_evaluations_o),
      .rad_behind_o     (rad_behind_o),
      .rad_bad_bound_o  (rad_bad_bound_o),
      .rad_saturated_o  (rad_saturated_o),
      .busy_o           (busy_o)
  );

endmodule : tb_geom_lodstate

`default_nettype wire
