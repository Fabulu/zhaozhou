// tb_raster_texture_stage_v3.sv -- private Packet-C composition harness.
//
// The executable path is exactly:
//   zhao_skid2(W=490) -> zhao_raster_texture_stage_v3
//     -> real zhao_raster_fragment -> external one-cycle tile model.
// Test-only gates pause the two observable boundaries without adding storage.
// Synthetic RELEASE/PUBLISH classification is deliberately here, not in the
// product stage: Packet H remains the real lease owner.  Likewise the test
// observes skid_cancel_i while occupancy is live, then separately asserts the
// private skid_rst_n_i to empty it; Packet D owns the eventual physical
// upstream cancellation path.
`default_nettype none

module tb_raster_texture_stage_v3 #(
    parameter bit MIGRATION_SHADOWS = 1'b0
) (
    input  logic clk,
    input  logic rst_n,

    input  logic         in_valid_i,
    output logic         in_ready_o,
    input  logic [490:0] in_data_i,
    input  logic         stage_admit_enable_i,
    input  logic         fragment_pause_i,
    input  logic         skid_rst_n_i,
    input  logic         skid_cancel_i,

    output logic [1:0]   skid_level_o,
    output logic         obs_cand_valid_o,
    output logic         obs_cand_ready_o,
    output logic [490:0] obs_cand_data_o,

    input  logic         frame_fault_clear_valid_i,
    output logic         frame_fault_clear_ready_o,
    output logic         frame_fault_o,
    output logic         lifetime_structural_fault_o,

    input  logic         cfg_valid_i,
    output logic         cfg_ready_o,
    input  logic [1:0]   cfg_op_i,
    input  logic [7:0]   cfg_page_generation_i,
    input  logic [7:0]   cfg_selector_i,
    input  logic [74:0]  cfg_row_i,
    input  logic [31:0]  cfg_crc32_i,
    output logic         cfg_rsp_valid_o,
    input  logic         cfg_rsp_ready_i,
    output logic [1:0]   cfg_rsp_op_o,
    output logic [3:0]   cfg_rsp_status_o,
    output logic [7:0]   cfg_rsp_page_generation_o,
    output logic [7:0]   active_page_generation_o,

    output logic         fill_req_valid_o,
    input  logic         fill_req_ready_i,
    output logic [31:0]  fill_req_addr_o,
    input  logic         fill_data_valid_i,
    input  logic [15:0]  fill_data_i,
    input  logic         fill_refused_i,

    input  logic         pal_load_valid_i,
    output logic         pal_load_ready_o,
    input  logic [1:0]   pal_load_op_i,
    input  logic [1:0]   pal_load_slot_i,
    input  logic [7:0]   pal_load_gen_i,
    input  logic [7:0]   pal_load_idx_i,
    input  logic [15:0]  pal_load_rgb565_i,
    input  logic         pal_load_crc_ok_i,

    output logic         sheet_req_valid_o,
    input  logic         sheet_req_ready_i,
    output logic [1:0]   sheet_req_op_o,
    output logic [31:0]  sheet_req_handle_o,
    output logic [11:0]  sheet_req_texel_o,
    output logic [15:0]  sheet_req_src_id_o,
    input  logic         pg_valid_i,
    output logic         pg_ready_o,
    input  logic [1:0]   pg_op_i,
    input  logic [1:0]   pg_status_i,
    input  logic [7:0]   pg_tag_i,
    input  logic [7:0]   pg_strength_i,
    input  logic [15:0]  pg_src_id_i,

    // External one-cycle tile-memory model.
    output logic         tile_rd_valid_o,
    input  logic         tile_rd_ready_i,
    output logic [7:0]   tile_rd_addr_o,
    output logic [15:0]  tile_rd_src_id_o,
    input  logic         tile_rd_valid_i,
    input  logic [63:0]  tile_rd_data_i,
    output logic         tile_wr_valid_o,
    input  logic         tile_wr_ready_i,
    output logic [7:0]   tile_wr_addr_o,
    output logic [63:0]  tile_wr_data_o,

    // Exact held Packet-C fragment beat before the real fragment leaf.
    output logic         obs_frag_valid_o,
    output logic         obs_frag_ready_o,
    output logic [7:0]   obs_frag_addr_o,
    output logic [23:0]  obs_frag_depth_o,
    output logic [31:0]  obs_frag_state_o,
    output logic [15:0]  obs_frag_src_id_o,
    output logic [23:0]  obs_frag_vert_rgb_o,
    output logic [7:0]   obs_frag_vert_a_o,
    output logic [7:0]   obs_frag_tag_o,
    output logic [7:0]   obs_frag_sten_ref_o,
    output logic [23:0]  obs_frag_texel_rgb_o,
    output logic [7:0]   obs_frag_texel_a_o,
    output logic [7:0]   obs_frag_texel_idx_o,
    output logic [7:0]   obs_frag_status_o,

    output logic         texture_quiet_o,
    output logic         fragment_idle_o,
    output logic         fragment_error_o,
    output logic [31:0]  covered_fragments_o,
    output logic [31:0]  blended_fragments_o,
    output logic         sequence_abort_o,
    output logic [31:0]  sequence_drop_count_o,
    output logic         sequence_mismatch_o,
    output logic [31:0]  admission_sequence_o,
    output logic [31:0]  expected_sequence_o,
    output logic [31:0]  returned_sequence_o,
    output logic         cand_fire_o,
    output logic         fragment_fire_o,
    output logic         drop_fire_o,
    output logic [31:0]  texture_fragments_o,
    output logic [31:0]  plan_accepted_o,
    output logic [31:0]  combine_refused_o,
    output logic         shadow_present_o,
    output logic [31:0]  meta_shadow_mismatch_o,
    output logic [31:0]  meta_shadow_reads_o,

    // Test-only terminal classifier.  classify_i is pulsed only after the
    // external fragment leaf is idle; these are evidence, never lease events.
    input  logic         classify_i,
    output logic         synthetic_release_o,
    output logic         synthetic_publish_o,
    output logic         skid_cancelled_o
);
  logic skid_valid_w;
  logic skid_ready_w;
  logic [490:0] skid_data_w;
  logic stage_cand_valid_w;
  logic stage_cand_ready_w;

  logic stage_frag_valid_w;
  logic stage_frag_ready_w;
  logic fragment_input_valid_w;
  logic fragment_input_ready_w;

  logic unused_err_fragrob_wq_overflow;
  logic unused_err_fragrob_id_error;
  logic unused_err_aux_degenerate;
  logic unused_err_rcp_q;
  logic [31:0] unused_cnt_reorder_held;
  logic [31:0] unused_cnt_live_peak;
  logic [31:0] unused_cnt_cache_hits;
  logic [31:0] unused_cnt_cache_misses;
  logic [31:0] unused_cnt_palette_lookups;
  logic [31:0] unused_cnt_bilerp_jobs;
  logic [31:0] unused_cnt_mosaic_samples;
  logic [31:0] unused_cnt_texture_samples;
  logic [31:0] unused_cnt_aux_accepted;
  logic [31:0] unused_cnt_combine_phases;
  logic [31:0] unused_cnt_rcp_completed;
  logic [31:0] unused_cnt_persp_fragments;
  logic [31:0] unused_cnt_dispatch_accepted;
  logic [31:0] unused_cnt_fragrob_id_errors;
  logic [31:0] unused_meta_align_err;
  logic [31:0] unused_meta_align_chk;
  logic [31:0] unused_meta_bil_err;
  logic [31:0] unused_meta_bil_chk;
  logic [31:0] unused_meta_near_err;
  logic [31:0] unused_meta_near_chk;
  logic [20:0] unused_meta_bil_first_q;
  logic [20:0] unused_meta_bil_first_t;
  logic [17:0] unused_meta_bil_first_tok;
  logic [31:0] unused_meta_genmis;
  logic [31:0] unused_cnt_combine_jobs [0:7];
  logic [31:0] unused_cnt_palette_stale;
  logic [31:0] unused_cnt_palette_cold;
  logic unused_err_rsp_dropped;
  logic unused_err_bil_chan;
  logic [31:0] unused_cnt_near_refused;
  logic [31:0] unused_err_unknown_class;
  logic [31:0] unused_err_class_invalid;
  logic [31:0] unused_err_palette_unusable;
  logic [31:0] unused_err_class_mismatch;
  logic unused_err_plan_mode;

  assign stage_cand_valid_w = skid_valid_w && stage_admit_enable_i;
  assign skid_ready_w = stage_cand_ready_w && stage_admit_enable_i;

  assign obs_cand_valid_o = skid_valid_w;
  assign obs_cand_ready_o = skid_ready_w;
  assign obs_cand_data_o = skid_data_w;

  assign fragment_input_valid_w = stage_frag_valid_w && !fragment_pause_i;
  assign stage_frag_ready_w = fragment_input_ready_w && !fragment_pause_i;
  assign obs_frag_valid_o = stage_frag_valid_w;
  assign obs_frag_ready_o = stage_frag_ready_w;

  assign synthetic_release_o = classify_i && texture_quiet_o &&
      fragment_idle_o && frame_fault_o;
  assign synthetic_publish_o = classify_i && texture_quiet_o &&
      fragment_idle_o && !frame_fault_o;
  assign skid_cancelled_o = skid_cancel_i && (skid_level_o != 2'd0);

  zhao_skid2 #(.W(491)) u_candidate_skid (
      .clk(clk),
      .rst_n(skid_rst_n_i),
      .up_valid_i(in_valid_i),
      .up_ready_o(in_ready_o),
      .up_data_i(in_data_i),
      .dn_valid_o(skid_valid_w),
      .dn_ready_i(skid_ready_w),
      .dn_data_o(skid_data_w),
      .level_o(skid_level_o)
  );

  zhao_raster_texture_stage_v3 #(
      .MIGRATION_SHADOWS(MIGRATION_SHADOWS)
  ) u_stage (
      .clk(clk),
      .rst_n(rst_n),
      .cand_valid_i(stage_cand_valid_w),
      .cand_ready_o(stage_cand_ready_w),
      .cand_data_i(skid_data_w),
      .frame_fault_clear_valid_i(frame_fault_clear_valid_i),
      .frame_fault_clear_ready_o(frame_fault_clear_ready_o),
      .frame_fault_o(frame_fault_o),
      .lifetime_structural_fault_o(lifetime_structural_fault_o),
      .cfg_valid_i(cfg_valid_i),
      .cfg_ready_o(cfg_ready_o),
      .cfg_op_i(cfg_op_i),
      .cfg_page_generation_i(cfg_page_generation_i),
      .cfg_selector_i(cfg_selector_i),
      .cfg_row_i(cfg_row_i),
      .cfg_crc32_i(cfg_crc32_i),
      .cfg_rsp_valid_o(cfg_rsp_valid_o),
      .cfg_rsp_ready_i(cfg_rsp_ready_i),
      .cfg_rsp_op_o(cfg_rsp_op_o),
      .cfg_rsp_status_o(cfg_rsp_status_o),
      .cfg_rsp_page_generation_o(cfg_rsp_page_generation_o),
      .active_page_generation_o(active_page_generation_o),
      .fill_req_valid_o(fill_req_valid_o),
      .fill_req_ready_i(fill_req_ready_i),
      .fill_req_addr_o(fill_req_addr_o),
      .fill_data_valid_i(fill_data_valid_i),
      .fill_data_i(fill_data_i),
      .fill_refused_i(fill_refused_i),
      .pal_load_valid_i(pal_load_valid_i),
      .pal_load_ready_o(pal_load_ready_o),
      .pal_load_op_i(pal_load_op_i),
      .pal_load_slot_i(pal_load_slot_i),
      .pal_load_gen_i(pal_load_gen_i),
      .pal_load_idx_i(pal_load_idx_i),
      .pal_load_rgb565_i(pal_load_rgb565_i),
      .pal_load_crc_ok_i(pal_load_crc_ok_i),
      .sheet_req_valid_o(sheet_req_valid_o),
      .sheet_req_ready_i(sheet_req_ready_i),
      .sheet_req_op_o(sheet_req_op_o),
      .sheet_req_handle_o(sheet_req_handle_o),
      .sheet_req_texel_o(sheet_req_texel_o),
      .sheet_req_src_id_o(sheet_req_src_id_o),
      .pg_valid_i(pg_valid_i),
      .pg_ready_o(pg_ready_o),
      .pg_op_i(pg_op_i),
      .pg_status_i(pg_status_i),
      .pg_tag_i(pg_tag_i),
      .pg_strength_i(pg_strength_i),
      .pg_src_id_i(pg_src_id_i),
      .frag_valid_o(stage_frag_valid_w),
      .frag_ready_i(stage_frag_ready_w),
      .frag_addr_o(obs_frag_addr_o),
      .frag_depth_o(obs_frag_depth_o),
      .frag_state_o(obs_frag_state_o),
      .frag_src_id_o(obs_frag_src_id_o),
      .frag_vert_rgb_o(obs_frag_vert_rgb_o),
      .frag_vert_a_o(obs_frag_vert_a_o),
      .frag_tag_o(obs_frag_tag_o),
      .frag_sten_ref_o(obs_frag_sten_ref_o),
      .frag_texel_rgb_o(obs_frag_texel_rgb_o),
      .frag_texel_a_o(obs_frag_texel_a_o),
      .frag_texel_idx_o(obs_frag_texel_idx_o),
      .frag_status_o(obs_frag_status_o),
      .quiet_o(texture_quiet_o),
      .sequence_abort_o(sequence_abort_o),
      .sequence_drop_count_o(sequence_drop_count_o),
      .sequence_mismatch_o(sequence_mismatch_o),
      .admission_sequence_o(admission_sequence_o),
      .expected_sequence_o(expected_sequence_o),
      .returned_sequence_o(returned_sequence_o),
      .cand_fire_o(cand_fire_o),
      .fragment_fire_o(fragment_fire_o),
      .drop_fire_o(drop_fire_o),
      .err_fragrob_wq_overflow_o(unused_err_fragrob_wq_overflow),
      .err_fragrob_id_error_o(unused_err_fragrob_id_error),
      .err_aux_degenerate_o(unused_err_aux_degenerate),
      .err_rcp_q_o(unused_err_rcp_q),
      .cnt_reorder_held_o(unused_cnt_reorder_held),
      .cnt_live_peak_o(unused_cnt_live_peak),
      .cnt_fragments_o(texture_fragments_o),
      .cnt_cache_hits_o(unused_cnt_cache_hits),
      .cnt_cache_misses_o(unused_cnt_cache_misses),
      .cnt_palette_lookups_o(unused_cnt_palette_lookups),
      .cnt_bilerp_jobs_o(unused_cnt_bilerp_jobs),
      .cnt_mosaic_samples_o(unused_cnt_mosaic_samples),
      .cnt_texture_samples_o(unused_cnt_texture_samples),
      .cnt_aux_accepted_o(unused_cnt_aux_accepted),
      .cnt_combine_refused_o(combine_refused_o),
      .cnt_combine_phases_o(unused_cnt_combine_phases),
      .cnt_rcp_completed_o(unused_cnt_rcp_completed),
      .cnt_persp_fragments_o(unused_cnt_persp_fragments),
      .cnt_dispatch_accepted_o(unused_cnt_dispatch_accepted),
      .cnt_plan_accepted_o(plan_accepted_o),
      .cnt_fragrob_id_errors_o(unused_cnt_fragrob_id_errors),
      .shadow_present_o(shadow_present_o),
      .meta_shadow_mismatch_o(meta_shadow_mismatch_o),
      .meta_shadow_reads_o(meta_shadow_reads_o),
      .meta_align_err_o(unused_meta_align_err),
      .meta_align_chk_o(unused_meta_align_chk),
      .meta_bil_err_o(unused_meta_bil_err),
      .meta_bil_chk_o(unused_meta_bil_chk),
      .meta_near_err_o(unused_meta_near_err),
      .meta_near_chk_o(unused_meta_near_chk),
      .meta_bil_first_q_o(unused_meta_bil_first_q),
      .meta_bil_first_t_o(unused_meta_bil_first_t),
      .meta_bil_first_tok_o(unused_meta_bil_first_tok),
      .meta_genmis_o(unused_meta_genmis),
      .cnt_combine_jobs_o(unused_cnt_combine_jobs),
      .cnt_palette_stale_o(unused_cnt_palette_stale),
      .cnt_palette_cold_o(unused_cnt_palette_cold),
      .err_rsp_dropped_o(unused_err_rsp_dropped),
      .err_bil_chan_o(unused_err_bil_chan),
      .cnt_near_refused_o(unused_cnt_near_refused),
      .err_unknown_class_o(unused_err_unknown_class),
      .err_class_invalid_o(unused_err_class_invalid),
      .err_palette_unusable_o(unused_err_palette_unusable),
      .err_class_mismatch_o(unused_err_class_mismatch),
      .err_plan_mode_o(unused_err_plan_mode)
  );

  zhao_raster_fragment u_fragment (
      .clk(clk),
      .rst_n(rst_n),
      .frag_valid_i(fragment_input_valid_w),
      .frag_ready_o(fragment_input_ready_w),
      .frag_addr_i(obs_frag_addr_o),
      .frag_depth_i(obs_frag_depth_o),
      .frag_state_i(obs_frag_state_o),
      .frag_src_id_i(obs_frag_src_id_o),
      .frag_vert_rgb_i(obs_frag_vert_rgb_o),
      .frag_vert_a_i(obs_frag_vert_a_o),
      .frag_tag_i(obs_frag_tag_o),
      .frag_sten_ref_i(obs_frag_sten_ref_o),
      .frag_texel_rgb_i(obs_frag_texel_rgb_o),
      .frag_texel_a_i(obs_frag_texel_a_o),
      .frag_texel_idx_i(obs_frag_texel_idx_o),
      .rd_valid_o(tile_rd_valid_o),
      .rd_ready_i(tile_rd_ready_i),
      .rd_addr_o(tile_rd_addr_o),
      .rd_src_id_o(tile_rd_src_id_o),
      .rd_valid_i(tile_rd_valid_i),
      .rd_data_i(tile_rd_data_i),
      .wr_valid_o(tile_wr_valid_o),
      .wr_ready_i(tile_wr_ready_i),
      .wr_addr_o(tile_wr_addr_o),
      .wr_data_o(tile_wr_data_o),
      .fragment_error_o(fragment_error_o),
      .idle_o(fragment_idle_o),
      .covered_fragments_o(covered_fragments_o),
      .blended_fragments_o(blended_fragments_o)
  );

endmodule : tb_raster_texture_stage_v3

`default_nettype wire
