// tb_terrain_prepwalk.sv -- a FLATTENING WRAPPER around zhao_terrain_prepwalk.
//
// IT HOLDS NO LOGIC. Every port is passed straight through; the only thing this
// file does is take the two packed HPS-bridge structs apart into plain vectors
// so the C++ bench can drive and read them by field.
//
// WHY A WRAPPER AND NOT STRUCT DECODING IN THE BENCH. `zhao_hps_burst_req_t`
// and `zhao_hps_burst_rsp_t` reach Verilated C++ as flat wide words whose bit
// offsets depend on `zhao_client_e`'s width -- a number no bench should have to
// know and which changes the day a client is added. A bench that hard-codes
// those offsets is a bench that goes silently wrong on an unrelated edit, in
// whichever direction the shifted bits happen to land. Here the packing is done
// by the tool that owns the typedef.
//
// `tools/budget/mutant_copy_drift.py` does not count this as a copy, correctly:
// a wrapper that INSTANTIATES the production module cannot drift away from it.
// If a port is added to `zhao_terrain_prepwalk` and not to this file, the
// elaboration fails loudly rather than passing quietly.

`default_nettype none

module tb_terrain_prepwalk
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    input var logic [2:0]  cfg_hps_client_i,
    input var logic [31:0] cfg_epoch_i,
    input var logic [31:0] cfg_arena_base_i,
    input var logic [31:0] cfg_arena_bytes_i,

    input  var logic        j_valid_i,
    output var logic        j_ready_o,
    input  var logic [31:0] j_epoch_i,
    input  var logic [31:0] j_list_off_i,
    input  var logic [31:0] j_list_bytes_i,
    input  var logic [31:0] j_list_crc_i,
    input  var logic [15:0] j_patch_count_i,

    input  var logic signed [7:0] frz_pitch_log2_i,
    input  var logic [31:0]       frz_tok_i,
    output var logic [31:0]       frz_tok_o,

    // ---- the HPS bridge, FLATTENED ---------------------------------------
    output var logic        hps_req_valid_o,
    output var logic        hps_req_write_o,
    output var logic [31:0] hps_req_addr_o,
    output var logic [ 6:0] hps_req_len_o,
    input  var logic        hps_req_grant_i,
    input  var logic        hps_rsp_beat_valid_i,
    input  var logic [63:0] hps_rsp_data_i,
    input  var logic        hps_rsp_last_i,
    input  var logic        hps_rsp_err_i,

    output var logic               lu_valid_o,
    input  var logic               lu_ready_i,
    output var logic [31:0]        lu_epoch_o,
    output var logic [31:0]        lu_island_o,
    output var logic signed [15:0] lu_ix_o,
    output var logic signed [15:0] lu_iz_o,
    input  var logic               lu_ans_valid_i,
    input  var logic               lu_ans_hit_i,
    input  var logic [9:0]         lu_ans_slot_i,
    input  var logic [7:0]         lu_ans_gen_i,

    output var logic               r_start_o,
    output var logic [9:0]         r_slot_o,
    input  var logic               r_ready_i,
    input  var logic               r_valid_i,
    output var logic               r_ready_o,
    input  var logic [3:0]         r_sp_i,
    input  var logic [23:0]        r_dev1_i,
    input  var logic [23:0]        r_dev2_i,
    input  var logic [23:0]        r_dev3_i,
    input  var logic signed [15:0] r_cy_i,
    input  var logic [1:0]         r_prev_level_i,
    input  var logic [16:0]        r_prev_morph_i,
    input  var logic [7:0]         r_hold_i,
    input  var logic               r_fresh_i,

    output var logic               sp_valid_o,
    input  var logic               sp_ready_i,
    output var logic signed [31:0] sp_cx_o,
    output var logic signed [31:0] sp_cy_o,
    output var logic signed [31:0] sp_cz_o,
    output var logic [23:0]        sp_dev1_o,
    output var logic [23:0]        sp_dev2_o,
    output var logic [23:0]        sp_dev3_o,
    output var logic [1:0]         sp_prev_level_o,
    output var logic [16:0]        sp_prev_morph_o,
    output var logic [7:0]         sp_hold_o,
    output var logic [15:0]        sp_src_id_o,
    output var logic signed [15:0] sp_ix_o,
    output var logic signed [15:0] sp_iz_o,
    output var logic [7:0]         sp_gen_o,
    output var logic [3:0]         sp_sub_o,

    output var logic prep_begin_o,
    input  var logic prep_gate_i,
    output var logic prep_done_o,
    output var logic prep_valid_o,
    output var logic restart_req_o,
    output var logic busy_o,
    output var logic idle_o,

    output var logic [31:0] walks_started_o,
    output var logic [31:0] walks_completed_o,
    output var logic [31:0] records_walked_o,
    output var logic [31:0] patches_prepared_o,
    output var logic [31:0] skipped_not_resident_o,
    output var logic [31:0] descriptors_emitted_o,
    output var logic [31:0] patches_unfresh_o,
    output var logic [31:0] list_crc_mismatch_o,
    output var logic [31:0] freeze_broken_o,
    output var logic [31:0] pitch_illegal_o,
    output var logic [31:0] jobs_refused_o,
    output var logic [31:0] place_range_o,
    output var logic [31:0] bridge_errs_o,
    output var logic [31:0] sub_order_bad_o,
    output var logic [31:0] store_wait_clocks_o,
    output var logic [31:0] list_wait_clocks_o,
    output var logic [31:0] list_bytes_read_o,
    output var logic [31:0] list_refetch_bytes_o
);

  zhao_hps_burst_req_t req_w;
  zhao_hps_burst_rsp_t rsp_w;

  assign hps_req_valid_o = req_w.valid;
  assign hps_req_write_o = req_w.write;
  assign hps_req_addr_o  = req_w.addr;
  assign hps_req_len_o   = req_w.len;

  always_comb begin
    rsp_w.beat_valid = hps_rsp_beat_valid_i;
    rsp_w.data       = hps_rsp_data_i;
    rsp_w.last       = hps_rsp_last_i;
    rsp_w.err        = hps_rsp_err_i;
  end

  zhao_terrain_prepwalk u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_hps_client_i (zhao_client_e'(cfg_hps_client_i)),
      .cfg_epoch_i      (cfg_epoch_i),
      .cfg_arena_base_i (cfg_arena_base_i),
      .cfg_arena_bytes_i(cfg_arena_bytes_i),

      .j_valid_i      (j_valid_i),
      .j_ready_o      (j_ready_o),
      .j_epoch_i      (j_epoch_i),
      .j_list_off_i   (j_list_off_i),
      .j_list_bytes_i (j_list_bytes_i),
      .j_list_crc_i   (j_list_crc_i),
      .j_patch_count_i(j_patch_count_i),

      .frz_pitch_log2_i(frz_pitch_log2_i),
      .frz_tok_i       (frz_tok_i),
      .frz_tok_o       (frz_tok_o),

      .hps_req_o      (req_w),
      .hps_req_grant_i(hps_req_grant_i),
      .hps_rsp_i      (rsp_w),

      .lu_valid_o    (lu_valid_o),
      .lu_ready_i    (lu_ready_i),
      .lu_epoch_o    (lu_epoch_o),
      .lu_island_o   (lu_island_o),
      .lu_ix_o       (lu_ix_o),
      .lu_iz_o       (lu_iz_o),
      .lu_ans_valid_i(lu_ans_valid_i),
      .lu_ans_hit_i  (lu_ans_hit_i),
      .lu_ans_slot_i (lu_ans_slot_i),
      .lu_ans_gen_i  (lu_ans_gen_i),

      .r_start_o      (r_start_o),
      .r_slot_o       (r_slot_o),
      .r_ready_i      (r_ready_i),
      .r_valid_i      (r_valid_i),
      .r_ready_o      (r_ready_o),
      .r_sp_i         (r_sp_i),
      .r_dev1_i       (r_dev1_i),
      .r_dev2_i       (r_dev2_i),
      .r_dev3_i       (r_dev3_i),
      .r_cy_i         (r_cy_i),
      .r_prev_level_i (r_prev_level_i),
      .r_prev_morph_i (r_prev_morph_i),
      .r_hold_i       (r_hold_i),
      .r_fresh_i      (r_fresh_i),

      .sp_valid_o     (sp_valid_o),
      .sp_ready_i     (sp_ready_i),
      .sp_cx_o        (sp_cx_o),
      .sp_cy_o        (sp_cy_o),
      .sp_cz_o        (sp_cz_o),
      .sp_dev1_o      (sp_dev1_o),
      .sp_dev2_o      (sp_dev2_o),
      .sp_dev3_o      (sp_dev3_o),
      .sp_prev_level_o(sp_prev_level_o),
      .sp_prev_morph_o(sp_prev_morph_o),
      .sp_hold_o      (sp_hold_o),
      .sp_src_id_o    (sp_src_id_o),
      .sp_ix_o        (sp_ix_o),
      .sp_iz_o        (sp_iz_o),
      .sp_gen_o       (sp_gen_o),
      .sp_sub_o       (sp_sub_o),

      .prep_begin_o (prep_begin_o),
      .prep_gate_i  (prep_gate_i),
      .prep_done_o  (prep_done_o),
      .prep_valid_o (prep_valid_o),
      .restart_req_o(restart_req_o),
      .busy_o       (busy_o),
      .idle_o       (idle_o),

      .walks_started_o       (walks_started_o),
      .walks_completed_o     (walks_completed_o),
      .records_walked_o      (records_walked_o),
      .patches_prepared_o    (patches_prepared_o),
      .skipped_not_resident_o(skipped_not_resident_o),
      .descriptors_emitted_o (descriptors_emitted_o),
      .patches_unfresh_o     (patches_unfresh_o),
      .list_crc_mismatch_o   (list_crc_mismatch_o),
      .freeze_broken_o       (freeze_broken_o),
      .pitch_illegal_o       (pitch_illegal_o),
      .jobs_refused_o        (jobs_refused_o),
      .place_range_o         (place_range_o),
      .bridge_errs_o         (bridge_errs_o),
      .sub_order_bad_o       (sub_order_bad_o),
      .store_wait_clocks_o   (store_wait_clocks_o),
      .list_wait_clocks_o    (list_wait_clocks_o),
      .list_bytes_read_o     (list_bytes_read_o),
      .list_refetch_bytes_o  (list_refetch_bytes_o)
  );

endmodule

`default_nettype wire
