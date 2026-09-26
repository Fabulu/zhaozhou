// zhao_shell_paired_diff.sv -- GENERATED, do not edit.
//
// Regenerate: python tools/design/gen_shell_paired_diff.py
// Checked by: packet_h_paired_diff_fresh
//
// The Packet-H gate clause "unaffected behaviour matches under
// paired traffic", as a machine. One stimulus, two shells, and a
// mismatch bit per compared output.
//
// 59 shared inputs drive both. 91 outputs must match. 4 are
// declared divergent, each with its reason in the generator --
// every name on that list is a claim WITHDRAWN, which is why it
// is short and why it is argued rather than discovered.
//
// 73 inputs exist only on the sibling. They get harness ports of
// their own so a test can exercise the new lifecycle without
// disturbing the paired comparison.

module zhao_shell_paired_diff_mut
  // The same import the two shells carry. Some ports are typedefs
  // -- `geom_guard_req_i` is a 103-bit `zhao_guard_req_t` -- and
  // without the package they are either unresolvable or, worse,
  // silently one bit wide. ONE comma-separated clause:
  // QUARTUS_GOTCHAS 20 says two consecutive `import` statements
  // lint clean here and are rejected by Quartus 17.0.
  import zhao_pkg::*, zhao_abi_pkg::*;
(
  input  logic gpu_clk,
  input  logic vid_clk,
  input  logic audio_clk,
  input  logic rst_n,
  input  logic [1:0] hps_state_i [0:2],
  input  logic [31:0] hps_byte_len_i [0:2],
  input  logic ring_wr_ready_i,
  input  logic hps_req_grant_i,
  input  logic hps_rd_valid_i,
  input  logic [63:0] hps_rd_data_i,
  input  logic hps_rd_last_i,
  input  logic [3:0] pad_present_i,
  input  logic [31:0] pad_buttons_i [0:3],
  input  logic [15:0] pad_lx_i [0:3],
  input  logic [15:0] pad_ly_i [0:3],
  input  logic [15:0] pad_rx_i [0:3],
  input  logic [15:0] pad_ry_i [0:3],
  input  logic aud_wr_valid_i,
  input  logic [15:0] aud_wr_l_i,
  input  logic [15:0] aud_wr_r_i,
  input  logic cnt_snap_ready_i,
  input  logic render_frame_begin_i,
  input  logic render_frame_end_i,
  input  logic [5:0] render_grid_w_i,
  input  logic [5:0] render_grid_h_i,
  input  logic render_tri_valid_i,
  input  zhao_guard_req_t geom_guard_req_i,
  input  logic [22:0] render_kx0_i,
  input  logic [22:0] render_ky0_i,
  input  logic [47:0] render_kc0_i,
  input  logic [22:0] render_kx1_i,
  input  logic [22:0] render_ky1_i,
  input  logic [47:0] render_kc1_i,
  input  logic [22:0] render_kx2_i,
  input  logic [22:0] render_ky2_i,
  input  logic [47:0] render_kc2_i,
  input  logic [ 2:0] render_tl_i,
  input  logic [20:0] render_ax_i,
  input  logic [20:0] render_ay_i,
  input  logic [20:0] render_bx_i,
  input  logic [20:0] render_by_i,
  input  logic [20:0] render_cx_i,
  input  logic [20:0] render_cy_i,
  input  logic [11:0] render_min_x_i,
  input  logic [11:0] render_max_x_i,
  input  logic [11:0] render_min_y_i,
  input  logic [11:0] render_max_y_i,
  input  logic [15:0] render_src_id_i,
  input  logic [63:0] render_fill_word_i,
  input  logic [63:0] render_clear_word_i,
  input  logic [31:0] render_state_i,
  input  logic [ 7:0] render_src_a_i,
  input  logic [23:0] render_texel_rgb_i,
  input  logic [ 7:0] render_texel_a_i,
  input  logic [ 7:0] render_texel_idx_i,
  input  logic [26:0] render_fb_base_i,
  input  logic [15:0] render_fb_stride_i,
  input  logic fb_writer_i,
  input  logic [15:0] phy_dq_i,
  input  logic cfg_valid_i,
  input  logic [1:0] cfg_op_i,
  input  logic [7:0] cfg_page_generation_i,
  input  logic [7:0] cfg_selector_i,
  input  logic [74:0] cfg_row_i,
  input  logic [31:0] cfg_crc32_i,
  input  logic cfg_rsp_ready_i,
  input  logic pal_load_valid_i,
  input  logic [1:0] pal_load_op_i,
  input  logic [1:0] pal_load_slot_i,
  input  logic [7:0] pal_load_gen_i,
  input  logic [7:0] pal_load_idx_i,
  input  logic [15:0] pal_load_rgb565_i,
  input  logic pal_load_crc_ok_i,
  input  logic [46:0] tri_area2_i,
  input  logic [239:0] tri_invw_plane_i,
  input  logic [239:0] tri_u_over_w_plane_i,
  input  logic [239:0] tri_v_over_w_plane_i,
  input  logic [239:0] tri_r_plane_i,
  input  logic [239:0] tri_g_plane_i,
  input  logic [239:0] tri_b_plane_i,
  input  logic [297:0] tri_flat_request_i,
  input  logic [47:0] tri_continuation_tail_i,
  input  logic [31:0] tri_fragment_state_i,
  input  logic fill_req_ready_i,
  input  logic fill_data_valid_i,
  input  logic [15:0] fill_data_i,
  input  logic fill_refused_i,
  input  logic [63:0] frame_clear_word_i,
  input  logic sheet_req_ready_i,
  input  logic pg_valid_i,
  input  logic [1:0] pg_op_i,
  input  logic [1:0] pg_status_i,
  input  logic [7:0] pg_tag_i,
  input  logic [7:0] pg_strength_i,
  input  logic [15:0] pg_src_id_i,
  input  logic blank_cmd_i,
  input  logic scanout_ack_i,
  input  logic frame_swap_valid_i,
  input  logic frame_swap_slot_i,
  input  logic [63:0] geom_wdata_i,
  input  logic geom_wvalid_i,
  input  logic geom_wlast_i,
  input  logic geom_pb_lease_i,
  input  logic geom_pb_wr_view_i,
  input  logic geom_pb_scratch_i,
  input  zhao_guard_req_t build_guard_req_i,
  input  logic [63:0] build_wdata_i,
  input  logic build_wvalid_i,
  input  logic build_wlast_i,
  input  zhao_hps_burst_req_t [1-1:0] build_hps_req_i,
  input  logic [1-1:0] build_hps_wr_valid_i,
  input  logic [1-1:0][63:0] build_hps_wr_data_i,
  input  logic [1-1:0] build_hps_wr_last_i,
  input  logic build_res_valid_i,
  input  logic [31:0] build_res_base_i,
  input  logic [31:0] build_res_span_i,
  input  logic [8:0] post_frame_w_i,
  input  logic [7:0] post_frame_h_i,
  input  logic post_duo_i,
  input  logic post_echo_arm_i,
  input  logic post_look_hold_i,
  input  logic post_src_ready_i,
  input  logic post_out_valid_i,
  input  logic [15:0] post_out_rgb_i,
  input  logic [8:0] post_out_x_i,
  input  logic [7:0] post_out_y_i,
  input  logic post_out_last_i,
  input  logic post_echo_valid_i,
  input  logic [15:0] post_echo_rgb_i,
  input  logic cmd_pkt_ready_i,
  input  logic render_ser_req_i,
  input  logic render_ser_ready_i,
  output logic v1_ring_wr_valid_o,
  output logic v2_ring_wr_valid_o,
  output logic [1:0] v1_ring_wr_slot_o,
  output logic [1:0] v2_ring_wr_slot_o,
  output logic [1:0] v1_ring_wr_state_o,
  output logic [1:0] v2_ring_wr_state_o,
  output logic v1_hps_req_valid_o,
  output logic v2_hps_req_valid_o,
  output logic v1_hps_req_write_o,
  output logic v2_hps_req_write_o,
  output logic [31:0] v1_hps_req_addr_o,
  output logic [31:0] v2_hps_req_addr_o,
  output logic [6:0] v1_hps_req_len_o,
  output logic [6:0] v2_hps_req_len_o,
  output logic v1_hps_wr_valid_o,
  output logic v2_hps_wr_valid_o,
  output logic [63:0] v1_hps_wr_data_o,
  output logic [63:0] v2_hps_wr_data_o,
  output logic v1_hps_wr_last_o,
  output logic v2_hps_wr_last_o,
  output logic v1_aud_wr_ready_o,
  output logic v2_aud_wr_ready_o,
  output logic v1_aud_refill_req_o,
  output logic v2_aud_refill_req_o,
  output logic [11:0] v1_aud_occupancy_o,
  output logic [11:0] v2_aud_occupancy_o,
  output logic v1_pcm_valid_o,
  output logic v2_pcm_valid_o,
  output logic [15:0] v1_pcm_l_o,
  output logic [15:0] v2_pcm_l_o,
  output logic [15:0] v1_pcm_r_o,
  output logic [15:0] v2_pcm_r_o,
  output logic v1_underrun_status_o,
  output logic v2_underrun_status_o,
  output logic [31:0] v1_audio_underruns_o,
  output logic [31:0] v2_audio_underruns_o,
  output logic v1_px_valid_o,
  output logic v2_px_valid_o,
  output logic [15:0] v1_px_rgb_o,
  output logic [15:0] v2_px_rgb_o,
  output logic [9:0] v1_px_x_o,
  output logic [9:0] v2_px_x_o,
  output logic [7:0] v1_px_y_o,
  output logic [7:0] v2_px_y_o,
  output logic v1_px_hsync_o,
  output logic v2_px_hsync_o,
  output logic v1_px_vsync_o,
  output logic v2_px_vsync_o,
  output logic v1_px_hblank_o,
  output logic v2_px_hblank_o,
  output logic v1_px_vblank_o,
  output logic v2_px_vblank_o,
  output logic v1_scaler_violation_o,
  output logic v2_scaler_violation_o,
  output logic [31:0] v1_crc_frame_o,
  output logic [31:0] v2_crc_frame_o,
  output logic v1_crc_valid_o,
  output logic v2_crc_valid_o,
  output logic [31:0] v1_crc_bytes_o,
  output logic [31:0] v2_crc_bytes_o,
  output logic v1_crc_size_err_o,
  output logic v2_crc_size_err_o,
  output logic v1_gpu_tick_o,
  output logic v2_gpu_tick_o,
  output logic [31:0] v1_gpu_tick_frame_id_o,
  output logic [31:0] v2_gpu_tick_frame_id_o,
  output logic v1_gpu_tick_repeated_o,
  output logic v2_gpu_tick_repeated_o,
  output logic [0:0] v1_gpu_complete_slot_o,
  output logic [0:0] v2_gpu_complete_slot_o,
  output logic [63:0] v1_deadline_faults_o,
  output logic [63:0] v2_deadline_faults_o,
  output logic [63:0] v1_frame_cycles_o,
  output logic [63:0] v2_frame_cycles_o,
  output logic [2:0] v1_slot_state_o [0:2],
  output logic [2:0] v2_slot_state_o [0:2],
  output logic v1_fence_valid_o,
  output logic v2_fence_valid_o,
  output logic [1:0] v1_fence_slot_o,
  output logic [1:0] v2_fence_slot_o,
  output logic v1_fence_ok_o,
  output logic v2_fence_ok_o,
  output logic [7:0] v1_fence_status_o,
  output logic [7:0] v2_fence_status_o,
  output logic [1:0] v1_mode_act_o,
  output logic [1:0] v2_mode_act_o,
  output logic v1_dma_done_o,
  output logic v2_dma_done_o,
  output logic [7:0] v1_dma_status_o,
  output logic [7:0] v2_dma_status_o,
  output logic v1_blit_done_o,
  output logic v2_blit_done_o,
  output logic [7:0] v1_blit_status_o,
  output logic [7:0] v2_blit_status_o,
  output logic [639:0] v1_pad_frame_flat_o,
  output logic [639:0] v2_pad_frame_flat_o,
  output logic [15:0] v1_pad_sequence_o [0:3],
  output logic [15:0] v2_pad_sequence_o [0:3],
  output logic [63:0] v1_input_gaps_o,
  output logic [63:0] v2_input_gaps_o,
  output logic [7:0] v1_rumble_duty_o [0:3],
  output logic [7:0] v2_rumble_duty_o [0:3],
  output logic [3:0] v1_rumble_active_o,
  output logic [3:0] v2_rumble_active_o,
  output logic [3:0] v1_rumble_pwm_o,
  output logic [3:0] v2_rumble_pwm_o,
  output logic [63:0] v1_rumble_drops_o,
  output logic [63:0] v2_rumble_drops_o,
  output logic v1_cnt_snap_valid_o,
  output logic v2_cnt_snap_valid_o,
  output logic [15:0] v1_cnt_snap_id_o,
  output logic [15:0] v2_cnt_snap_id_o,
  output logic [63:0] v1_cnt_snap_value_o,
  output logic [63:0] v2_cnt_snap_value_o,
  output logic v1_cnt_window_open_o,
  output logic v2_cnt_window_open_o,
  output logic v1_cnt_cat_violation_o,
  output logic v2_cnt_cat_violation_o,
  output logic [31:0] v1_guard_violations_o,
  output logic [31:0] v2_guard_violations_o,
  output logic [63:0] v1_starvation_o,
  output logic [63:0] v2_starvation_o,
  output logic v1_init_done_o,
  output logic v2_init_done_o,
  output logic [31:0] v1_refresh_stalls_o,
  output logic [31:0] v2_refresh_stalls_o,
  output logic [31:0] v1_bank_conflicts_o,
  output logic [31:0] v2_bank_conflicts_o,
  output logic [31:0] v1_scanout_preempted_o,
  output logic [31:0] v2_scanout_preempted_o,
  output logic [31:0] v1_hps_err_count_o,
  output logic [31:0] v2_hps_err_count_o,
  output logic v1_shell_err_wfifo_o,
  output logic v2_shell_err_wfifo_o,
  output logic v1_shell_err_route_o,
  output logic v2_shell_err_route_o,
  output logic v1_shell_err_cdc_o,
  output logic v2_shell_err_cdc_o,
  output logic v1_shell_err_framer_o,
  output logic v2_shell_err_framer_o,
  output logic v1_render_tri_ready_o,
  output logic v2_render_tri_ready_o,
  output zhao_guard_rsp_t v1_geom_guard_rsp_o,
  output zhao_guard_rsp_t v2_geom_guard_rsp_o,
  output logic v1_geom_beat_valid_o,
  output logic v2_geom_beat_valid_o,
  output logic [63:0] v1_geom_beat_data_o,
  output logic [63:0] v2_geom_beat_data_o,
  output logic v1_geom_beat_last_o,
  output logic v2_geom_beat_last_o,
  output logic v1_render_drain_done_o,
  output logic v2_render_drain_done_o,
  output logic v1_render_busy_o,
  output logic v2_render_busy_o,
  output logic [31:0] v1_render_pixels_o,
  output logic [31:0] v2_render_pixels_o,
  output logic [31:0] v1_render_bursts_o,
  output logic [31:0] v2_render_bursts_o,
  output logic v1_render_stream_error_o,
  output logic v2_render_stream_error_o,
  output logic v1_render_drained_o,
  output logic v2_render_drained_o,
  output logic v1_render_fatal_o,
  output logic v2_render_fatal_o,
  output logic [31:0] v1_render_issued_words_o,
  output logic [31:0] v2_render_issued_words_o,
  output logic [31:0] v1_render_retired_words_o,
  output logic [31:0] v2_render_retired_words_o,
  output logic v1_render_overflow_o,
  output logic v2_render_overflow_o,
  output logic v1_render_fragment_error_o,
  output logic v2_render_fragment_error_o,
  output logic v1_phy_cs_n_o,
  output logic v2_phy_cs_n_o,
  output logic v1_phy_ras_n_o,
  output logic v2_phy_ras_n_o,
  output logic v1_phy_cas_n_o,
  output logic v2_phy_cas_n_o,
  output logic v1_phy_we_n_o,
  output logic v2_phy_we_n_o,
  output logic [12:0] v1_phy_a_o,
  output logic [12:0] v2_phy_a_o,
  output logic [1:0] v1_phy_ba_o,
  output logic [1:0] v2_phy_ba_o,
  output logic [15:0] v1_phy_dq_o,
  output logic [15:0] v2_phy_dq_o,
  output logic v1_phy_dq_oe_o,
  output logic v2_phy_dq_oe_o,
  output logic [1:0] v1_phy_dqm_o,
  output logic [1:0] v2_phy_dqm_o,
  output logic mismatch_any_o,
  output logic mismatch_sticky_o,
  output logic [31:0] mismatch_count_o,
  output logic [31:0] mismatch_first_o,
  output logic [31:0] toggled_count_o
);

  // ---- the two shells, one stimulus ------------------------
  zhao_shell_top u_v1 (
    .gpu_clk(gpu_clk),
    .vid_clk(vid_clk),
    .audio_clk(audio_clk),
    .rst_n(rst_n),
    .hps_state_i(hps_state_i),
    .hps_byte_len_i(hps_byte_len_i),
    .ring_wr_valid_o(v1_ring_wr_valid_o),
    .ring_wr_slot_o(v1_ring_wr_slot_o),
    .ring_wr_state_o(v1_ring_wr_state_o),
    .ring_wr_ready_i(ring_wr_ready_i),
    .hps_req_valid_o(v1_hps_req_valid_o),
    .hps_req_write_o(v1_hps_req_write_o),
    .hps_req_addr_o(v1_hps_req_addr_o),
    .hps_req_len_o(v1_hps_req_len_o),
    .hps_req_grant_i(hps_req_grant_i),
    .hps_wr_valid_o(v1_hps_wr_valid_o),
    .hps_wr_data_o(v1_hps_wr_data_o),
    .hps_wr_last_o(v1_hps_wr_last_o),
    .hps_rd_valid_i(hps_rd_valid_i),
    .hps_rd_data_i(hps_rd_data_i),
    .hps_rd_last_i(hps_rd_last_i),
    .pad_present_i(pad_present_i),
    .pad_buttons_i(pad_buttons_i),
    .pad_lx_i(pad_lx_i),
    .pad_ly_i(pad_ly_i),
    .pad_rx_i(pad_rx_i),
    .pad_ry_i(pad_ry_i),
    .aud_wr_valid_i(aud_wr_valid_i),
    .aud_wr_l_i(aud_wr_l_i),
    .aud_wr_r_i(aud_wr_r_i),
    .aud_wr_ready_o(v1_aud_wr_ready_o),
    .aud_refill_req_o(v1_aud_refill_req_o),
    .aud_occupancy_o(v1_aud_occupancy_o),
    .pcm_valid_o(v1_pcm_valid_o),
    .pcm_l_o(v1_pcm_l_o),
    .pcm_r_o(v1_pcm_r_o),
    .underrun_status_o(v1_underrun_status_o),
    .audio_underruns_o(v1_audio_underruns_o),
    .px_valid_o(v1_px_valid_o),
    .px_rgb_o(v1_px_rgb_o),
    .px_x_o(v1_px_x_o),
    .px_y_o(v1_px_y_o),
    .px_hsync_o(v1_px_hsync_o),
    .px_vsync_o(v1_px_vsync_o),
    .px_hblank_o(v1_px_hblank_o),
    .px_vblank_o(v1_px_vblank_o),
    .scaler_violation_o(v1_scaler_violation_o),
    .crc_frame_o(v1_crc_frame_o),
    .crc_valid_o(v1_crc_valid_o),
    .crc_bytes_o(v1_crc_bytes_o),
    .crc_size_err_o(v1_crc_size_err_o),
    .gpu_tick_o(v1_gpu_tick_o),
    .gpu_tick_frame_id_o(v1_gpu_tick_frame_id_o),
    .gpu_tick_repeated_o(v1_gpu_tick_repeated_o),
    .gpu_complete_slot_o(v1_gpu_complete_slot_o),
    .deadline_faults_o(v1_deadline_faults_o),
    .frame_cycles_o(v1_frame_cycles_o),
    .slot_state_o(v1_slot_state_o),
    .fence_valid_o(v1_fence_valid_o),
    .fence_slot_o(v1_fence_slot_o),
    .fence_ok_o(v1_fence_ok_o),
    .fence_status_o(v1_fence_status_o),
    .mode_act_o(v1_mode_act_o),
    .dma_done_o(v1_dma_done_o),
    .dma_status_o(v1_dma_status_o),
    .blit_done_o(v1_blit_done_o),
    .blit_status_o(v1_blit_status_o),
    .pad_frame_flat_o(v1_pad_frame_flat_o),
    .pad_sequence_o(v1_pad_sequence_o),
    .input_gaps_o(v1_input_gaps_o),
    .rumble_duty_o(v1_rumble_duty_o),
    .rumble_active_o(v1_rumble_active_o),
    .rumble_pwm_o(v1_rumble_pwm_o),
    .rumble_drops_o(v1_rumble_drops_o),
    .cnt_snap_ready_i(cnt_snap_ready_i),
    .cnt_snap_valid_o(v1_cnt_snap_valid_o),
    .cnt_snap_id_o(v1_cnt_snap_id_o),
    .cnt_snap_value_o(v1_cnt_snap_value_o),
    .cnt_window_open_o(v1_cnt_window_open_o),
    .cnt_cat_violation_o(v1_cnt_cat_violation_o),
    .guard_violations_o(v1_guard_violations_o),
    .starvation_o(v1_starvation_o),
    .init_done_o(v1_init_done_o),
    .refresh_stalls_o(v1_refresh_stalls_o),
    .bank_conflicts_o(v1_bank_conflicts_o),
    .scanout_preempted_o(v1_scanout_preempted_o),
    .hps_err_count_o(v1_hps_err_count_o),
    .shell_err_wfifo_o(v1_shell_err_wfifo_o),
    .shell_err_route_o(v1_shell_err_route_o),
    .shell_err_cdc_o(v1_shell_err_cdc_o),
    .shell_err_framer_o(v1_shell_err_framer_o),
    .render_frame_begin_i(render_frame_begin_i),
    .render_frame_end_i(render_frame_end_i),
    .render_grid_w_i(render_grid_w_i),
    .render_grid_h_i(render_grid_h_i),
    .render_tri_valid_i(render_tri_valid_i),
    .render_tri_ready_o(v1_render_tri_ready_o),
    .geom_guard_req_i(geom_guard_req_i),
    .geom_guard_rsp_o(v1_geom_guard_rsp_o),
    .geom_beat_valid_o(v1_geom_beat_valid_o),
    .geom_beat_data_o(v1_geom_beat_data_o),
    .geom_beat_last_o(v1_geom_beat_last_o),
    .render_kx0_i(render_kx0_i),
    .render_ky0_i(render_ky0_i),
    .render_kc0_i(render_kc0_i),
    .render_kx1_i(render_kx1_i),
    .render_ky1_i(render_ky1_i),
    .render_kc1_i(render_kc1_i),
    .render_kx2_i(render_kx2_i),
    .render_ky2_i(render_ky2_i),
    .render_kc2_i(render_kc2_i),
    .render_tl_i(render_tl_i),
    .render_ax_i(render_ax_i),
    .render_ay_i(render_ay_i),
    .render_bx_i(render_bx_i),
    .render_by_i(render_by_i),
    .render_cx_i(render_cx_i),
    .render_cy_i(render_cy_i),
    .render_min_x_i(render_min_x_i),
    .render_max_x_i(render_max_x_i),
    .render_min_y_i(render_min_y_i),
    .render_max_y_i(render_max_y_i),
    .render_src_id_i(render_src_id_i),
    .render_fill_word_i(render_fill_word_i),
    .render_clear_word_i(render_clear_word_i),
    .render_state_i(render_state_i),
    .render_src_a_i(render_src_a_i),
    .render_texel_rgb_i(render_texel_rgb_i),
    .render_texel_a_i(render_texel_a_i),
    .render_texel_idx_i(render_texel_idx_i),
    .render_fb_base_i(render_fb_base_i),
    .render_fb_stride_i(render_fb_stride_i),
    .fb_writer_i(fb_writer_i),
    .render_drain_done_o(v1_render_drain_done_o),
    .render_busy_o(v1_render_busy_o),
    .render_pixels_o(v1_render_pixels_o),
    .render_bursts_o(v1_render_bursts_o),
    .render_stream_error_o(v1_render_stream_error_o),
    .render_drained_o(v1_render_drained_o),
    .render_fatal_o(v1_render_fatal_o),
    .render_issued_words_o(v1_render_issued_words_o),
    .render_retired_words_o(v1_render_retired_words_o),
    .render_overflow_o(v1_render_overflow_o),
    .render_fragment_error_o(v1_render_fragment_error_o),
    .phy_cs_n_o(v1_phy_cs_n_o),
    .phy_ras_n_o(v1_phy_ras_n_o),
    .phy_cas_n_o(v1_phy_cas_n_o),
    .phy_we_n_o(v1_phy_we_n_o),
    .phy_a_o(v1_phy_a_o),
    .phy_ba_o(v1_phy_ba_o),
    .phy_dq_o(v1_phy_dq_o),
    .phy_dq_oe_o(v1_phy_dq_oe_o),
    .phy_dqm_o(v1_phy_dqm_o),
    .phy_dq_i(phy_dq_i)
  );

  // The sibling has 88 outputs the historical shell never had
  // -- the v2_* lifecycle counters and the new lease surface.
  // They are left unconnected ON PURPOSE: this harness exists to
  // compare the SHARED surface, and a V2-only output has nothing
  // to be compared against.
  /* verilator lint_off PINCONNECTEMPTY */
  zhao_shell_top_v2 u_v2 (
    .gpu_clk(gpu_clk),
    .vid_clk(vid_clk),
    .audio_clk(audio_clk),
    .rst_n(rst_n),
    .cfg_valid_i(cfg_valid_i),
    .cfg_ready_o(),
    .cfg_op_i(cfg_op_i),
    .cfg_page_generation_i(cfg_page_generation_i),
    .cfg_selector_i(cfg_selector_i),
    .cfg_row_i(cfg_row_i),
    .cfg_crc32_i(cfg_crc32_i),
    .cfg_rsp_valid_o(),
    .cfg_rsp_ready_i(cfg_rsp_ready_i),
    .cfg_rsp_op_o(),
    .cfg_rsp_status_o(),
    .cfg_rsp_page_generation_o(),
    .active_page_generation_o(),
    .pal_load_valid_i(pal_load_valid_i),
    .pal_load_ready_o(),
    .pal_load_op_i(pal_load_op_i),
    .pal_load_slot_i(pal_load_slot_i),
    .pal_load_gen_i(pal_load_gen_i),
    .pal_load_idx_i(pal_load_idx_i),
    .pal_load_rgb565_i(pal_load_rgb565_i),
    .pal_load_crc_ok_i(pal_load_crc_ok_i),
    .tri_area2_i(tri_area2_i),
    .tri_invw_plane_i(tri_invw_plane_i),
    .tri_u_over_w_plane_i(tri_u_over_w_plane_i),
    .tri_v_over_w_plane_i(tri_v_over_w_plane_i),
    .tri_r_plane_i(tri_r_plane_i),
    .tri_g_plane_i(tri_g_plane_i),
    .tri_b_plane_i(tri_b_plane_i),
    .tri_flat_request_i(tri_flat_request_i),
    .tri_continuation_tail_i(tri_continuation_tail_i),
    .tri_fragment_state_i(tri_fragment_state_i),
    .fill_req_ready_i(fill_req_ready_i),
    .fill_req_valid_o(),
    .fill_req_addr_o(),
    .fill_data_valid_i(fill_data_valid_i),
    .fill_data_i(fill_data_i),
    .fill_refused_i(fill_refused_i),
    .frame_clear_word_i(frame_clear_word_i),
    .sheet_req_ready_i(sheet_req_ready_i),
    .sheet_req_valid_o(),
    .sheet_req_op_o(),
    .sheet_req_handle_o(),
    .sheet_req_texel_o(),
    .sheet_req_src_id_o(),
    .pg_valid_i(pg_valid_i),
    .pg_ready_o(),
    .pg_op_i(pg_op_i),
    .pg_status_i(pg_status_i),
    .pg_tag_i(pg_tag_i),
    .pg_strength_i(pg_strength_i),
    .pg_src_id_i(pg_src_id_i),
    .blank_cmd_i(blank_cmd_i),
    .scanout_ack_i(scanout_ack_i),
    .frame_swap_valid_i(frame_swap_valid_i),
    .frame_swap_slot_i(frame_swap_slot_i),
    .blank_ack_o(),
    .blank_active_o(),
    .lease_open_o(),
    .frame_slot_ready_o(),
    .v2_requests_accepted_o(),
    .v2_responses_accepted_o(),
    .v2_leases_granted_o(),
    .v2_leases_refused_o(),
    .v2_faults_latched_o(),
    .v2_publications_o(),
    .v2_releases_o(),
    .v2_ready_events_o(),
    .v2_swaps_o(),
    .v2_contentions_o(),
    .v2_clear_handshakes_o(),
    .v2_frames_admitted_o(),
    .v2_blit_leases_acquired_o(),
    .v2_blit_leases_refused_o(),
    .hps_state_i(hps_state_i),
    .hps_byte_len_i(hps_byte_len_i),
    .ring_wr_valid_o(v2_ring_wr_valid_o),
    .ring_wr_slot_o(v2_ring_wr_slot_o),
    .ring_wr_state_o(v2_ring_wr_state_o),
    .ring_wr_ready_i(ring_wr_ready_i),
    .hps_req_valid_o(v2_hps_req_valid_o),
    .hps_req_write_o(v2_hps_req_write_o),
    .hps_req_addr_o(v2_hps_req_addr_o),
    .hps_req_len_o(v2_hps_req_len_o),
    .hps_req_grant_i(hps_req_grant_i),
    .hps_wr_valid_o(v2_hps_wr_valid_o),
    .hps_wr_data_o(v2_hps_wr_data_o),
    .hps_wr_last_o(v2_hps_wr_last_o),
    .hps_rd_valid_i(hps_rd_valid_i),
    .hps_rd_data_i(hps_rd_data_i),
    .hps_rd_last_i(hps_rd_last_i),
    .pad_present_i(pad_present_i),
    .pad_buttons_i(pad_buttons_i),
    .pad_lx_i(pad_lx_i),
    .pad_ly_i(pad_ly_i),
    .pad_rx_i(pad_rx_i),
    .pad_ry_i(pad_ry_i),
    .aud_wr_valid_i(aud_wr_valid_i),
    .aud_wr_l_i(aud_wr_l_i),
    .aud_wr_r_i(aud_wr_r_i),
    .aud_wr_ready_o(v2_aud_wr_ready_o),
    .aud_refill_req_o(v2_aud_refill_req_o),
    .aud_occupancy_o(v2_aud_occupancy_o),
    .pcm_valid_o(v2_pcm_valid_o),
    .pcm_l_o(v2_pcm_l_o),
    .pcm_r_o(v2_pcm_r_o),
    .underrun_status_o(v2_underrun_status_o),
    .audio_underruns_o(v2_audio_underruns_o),
    .px_valid_o(v2_px_valid_o),
    .px_rgb_o(v2_px_rgb_o),
    .px_x_o(v2_px_x_o),
    .px_y_o(v2_px_y_o),
    .px_hsync_o(v2_px_hsync_o),
    .px_vsync_o(v2_px_vsync_o),
    .px_hblank_o(v2_px_hblank_o),
    .px_vblank_o(v2_px_vblank_o),
    .scaler_violation_o(v2_scaler_violation_o),
    .crc_frame_o(v2_crc_frame_o),
    .crc_valid_o(v2_crc_valid_o),
    .crc_bytes_o(v2_crc_bytes_o),
    .crc_size_err_o(v2_crc_size_err_o),
    .gpu_tick_o(v2_gpu_tick_o),
    .gpu_tick_frame_id_o(v2_gpu_tick_frame_id_o),
    .gpu_tick_repeated_o(v2_gpu_tick_repeated_o),
    .gpu_complete_slot_o(v2_gpu_complete_slot_o),
    .deadline_faults_o(v2_deadline_faults_o),
    .frame_cycles_o(v2_frame_cycles_o),
    .slot_state_o(v2_slot_state_o),
    .fence_valid_o(v2_fence_valid_o),
    .fence_slot_o(v2_fence_slot_o),
    .fence_ok_o(v2_fence_ok_o),
    .fence_status_o(v2_fence_status_o),
    .mode_act_o(v2_mode_act_o),
    .dma_done_o(v2_dma_done_o),
    .dma_status_o(v2_dma_status_o),
    .blit_done_o(v2_blit_done_o),
    .blit_status_o(v2_blit_status_o),
    .pad_frame_flat_o(v2_pad_frame_flat_o),
    .pad_sequence_o(v2_pad_sequence_o),
    .input_gaps_o(v2_input_gaps_o),
    .rumble_duty_o(v2_rumble_duty_o),
    .rumble_active_o(v2_rumble_active_o),
    .rumble_pwm_o(v2_rumble_pwm_o),
    .rumble_drops_o(v2_rumble_drops_o),
    .cnt_snap_ready_i(cnt_snap_ready_i),
    .cnt_snap_valid_o(v2_cnt_snap_valid_o),
    .cnt_snap_id_o(v2_cnt_snap_id_o),
    .cnt_snap_value_o(v2_cnt_snap_value_o),
    .cnt_window_open_o(v2_cnt_window_open_o),
    .cnt_cat_violation_o(v2_cnt_cat_violation_o),
    .guard_violations_o(v2_guard_violations_o),
    .starvation_o(v2_starvation_o),
    .init_done_o(v2_init_done_o),
    .refresh_stalls_o(v2_refresh_stalls_o),
    .bank_conflicts_o(v2_bank_conflicts_o),
    .scanout_preempted_o(v2_scanout_preempted_o),
    .hps_err_count_o(v2_hps_err_count_o),
    .shell_err_wfifo_o(v2_shell_err_wfifo_o),
    .shell_err_route_o(v2_shell_err_route_o),
    .shell_err_cdc_o(v2_shell_err_cdc_o),
    .shell_err_framer_o(v2_shell_err_framer_o),
    .render_frame_begin_i(render_frame_begin_i),
    .render_frame_end_i(render_frame_end_i),
    .render_grid_w_i(render_grid_w_i),
    .render_grid_h_i(render_grid_h_i),
    .render_tri_valid_i(render_tri_valid_i),
    .render_tri_ready_o(v2_render_tri_ready_o),
    .geom_guard_req_i(geom_guard_req_i),
    .geom_guard_rsp_o(v2_geom_guard_rsp_o),
    .geom_beat_valid_o(v2_geom_beat_valid_o),
    .geom_beat_data_o(v2_geom_beat_data_o),
    .geom_beat_last_o(v2_geom_beat_last_o),
    .geom_wdata_i(geom_wdata_i),
    .geom_wvalid_i(geom_wvalid_i),
    .geom_wready_o(),
    .geom_wlast_i(geom_wlast_i),
    .geom_retire_words_o(),
    .geom_pb_lease_i(geom_pb_lease_i),
    .geom_pb_wr_view_i(geom_pb_wr_view_i),
    .geom_pb_scratch_i(geom_pb_scratch_i),
    .build_guard_req_i(build_guard_req_i),
    .build_guard_rsp_o(),
    .build_wdata_i(build_wdata_i),
    .build_wvalid_i(build_wvalid_i),
    .build_wready_o(),
    .build_wlast_i(build_wlast_i),
    .build_retire_words_o(),
    .build_beat_valid_o(),
    .build_beat_data_o(),
    .build_beat_last_o(),
    .build_hps_req_i(build_hps_req_i),
    .build_hps_grant_o(),
    .build_hps_rsp_o(),
    .build_hps_wait_o(),
    .build_hps_wr_valid_i(build_hps_wr_valid_i),
    .build_hps_wr_data_i(build_hps_wr_data_i),
    .build_hps_wr_last_i(build_hps_wr_last_i),
    .build_hps_wr_ready_o(),
    .build_res_valid_i(build_res_valid_i),
    .build_res_base_i(build_res_base_i),
    .build_res_span_i(build_res_span_i),
    .render_kx0_i(render_kx0_i),
    .render_ky0_i(render_ky0_i),
    .render_kc0_i(render_kc0_i),
    .render_kx1_i(render_kx1_i),
    .render_ky1_i(render_ky1_i),
    .render_kc1_i(render_kc1_i),
    .render_kx2_i(render_kx2_i),
    .render_ky2_i(render_ky2_i),
    .render_kc2_i(render_kc2_i),
    .render_tl_i(render_tl_i),
    .render_ax_i(render_ax_i),
    .render_ay_i(render_ay_i),
    .render_bx_i(render_bx_i),
    .render_by_i(render_by_i),
    .render_cx_i(render_cx_i),
    .render_cy_i(render_cy_i),
    .render_min_x_i(render_min_x_i),
    .render_max_x_i(render_max_x_i),
    .render_min_y_i(render_min_y_i),
    .render_max_y_i(render_max_y_i),
    .render_src_id_i(render_src_id_i),
    .render_fill_word_i(render_fill_word_i),
    .render_clear_word_i(render_clear_word_i),
    .render_state_i(render_state_i),
    .render_src_a_i(render_src_a_i),
    .render_texel_rgb_i(render_texel_rgb_i),
    .render_texel_a_i(render_texel_a_i),
    .render_texel_idx_i(render_texel_idx_i),
    .render_fb_base_i(render_fb_base_i),
    .render_fb_stride_i(render_fb_stride_i),
    .fb_writer_i(fb_writer_i),
    .render_drain_done_o(v2_render_drain_done_o),
    .render_busy_o(v2_render_busy_o),
    .render_pixels_o(v2_render_pixels_o),
    .render_bursts_o(v2_render_bursts_o),
    .render_stream_error_o(v2_render_stream_error_o),
    .render_drained_o(v2_render_drained_o),
    .render_fatal_o(v2_render_fatal_o),
    .render_issued_words_o(v2_render_issued_words_o),
    .render_retired_words_o(v2_render_retired_words_o),
    .render_overflow_o(v2_render_overflow_o),
    .render_fragment_error_o(v2_render_fragment_error_o),
    .render_texture_fragments_o(),
    .render_texture_cache_hits_o(),
    .render_texture_cache_misses_o(),
    .render_texture_palette_lookups_o(),
    .render_texture_plan_accepted_o(),
    .render_texture_dispatch_accepted_o(),
    .render_texture_combine_refused_o(),
    .render_texture_samples_o(),
    .gth_valid_o(),
    .gth_rgb565_o(),
    .gth_tag_o(),
    .gth_addr_o(),
    .gth_x_o(),
    .gth_y_o(),
    .gth_last_o(),
    .post_frame_w_i(post_frame_w_i),
    .post_frame_h_i(post_frame_h_i),
    .post_duo_i(post_duo_i),
    .post_echo_arm_i(post_echo_arm_i),
    .post_look_hold_i(post_look_hold_i),
    .post_pass_start_o(),
    .post_view_o(),
    .post_src_valid_o(),
    .post_src_ready_i(post_src_ready_i),
    .post_src_rgb_o(),
    .post_out_valid_i(post_out_valid_i),
    .post_out_ready_o(),
    .post_out_rgb_i(post_out_rgb_i),
    .post_out_x_i(post_out_x_i),
    .post_out_y_i(post_out_y_i),
    .post_out_last_i(post_out_last_i),
    .post_echo_valid_i(post_echo_valid_i),
    .post_echo_rgb_i(post_echo_rgb_i),
    .post_busy_o(),
    .post_passes_o(),
    .post_frames_o(),
    .post_fault_o(),
    .post_src_reads_o(),
    .post_src_pixels_o(),
    .post_retire_unowned_o(),
    .post_share_contention_o(),
    .echo_passes_complete_o(),
    .echo_passes_torn_o(),
    .echo_pixels_written_o(),
    .echo_pixels_dropped_o(),
    .echo_fault_o(),
    .phy_cs_n_o(v2_phy_cs_n_o),
    .phy_ras_n_o(v2_phy_ras_n_o),
    .phy_cas_n_o(v2_phy_cas_n_o),
    .phy_we_n_o(v2_phy_we_n_o),
    .phy_a_o(v2_phy_a_o),
    .phy_ba_o(v2_phy_ba_o),
    .phy_dq_o(v2_phy_dq_o),
    .phy_dq_oe_o(v2_phy_dq_oe_o),
    .phy_dqm_o(v2_phy_dqm_o),
    .phy_dq_i(phy_dq_i),
    .cmd_pkt_valid_o(),
    .cmd_pkt_byte_o(),
    .cmd_pkt_len_o(),
    .cmd_pkt_ready_i(cmd_pkt_ready_i),
    .render_ser_req_i(render_ser_req_i),
    .render_ser_busy_o(),
    .render_ser_done_o(),
    .render_ser_valid_o(),
    .render_ser_ready_i(render_ser_ready_i),
    .render_ser_tri_id_o(),
    .render_ser_tile_o(),
    .render_ser_first_o(),
    .render_ser_last_o()
  );
  /* verilator lint_on PINCONNECTEMPTY */

  // ---- the verdict ----------------------------------------
  // One bit per compared output, OR-ed. A sticky copy because a
  // one-cycle divergence that the next cycle repairs is still a
  // divergence, and a test sampling at its own convenience would
  // miss it -- this repository has a chapter about sampling
  // uniformly and finding the typical frame.
  wire [90:0] mm_c;
  assign mm_c[0] = (v1_ring_wr_valid_o !== v2_ring_wr_valid_o);
  assign mm_c[1] = (v1_ring_wr_slot_o !== v2_ring_wr_slot_o);
  assign mm_c[2] = (v1_ring_wr_state_o !== v2_ring_wr_state_o);
  assign mm_c[3] = (v1_hps_req_valid_o !== v2_hps_req_valid_o);
  assign mm_c[4] = (v1_hps_req_write_o !== v2_hps_req_write_o);
  assign mm_c[5] = (v1_hps_req_addr_o !== v2_hps_req_addr_o);
  assign mm_c[6] = (v1_hps_req_len_o !== v2_hps_req_len_o);
  assign mm_c[7] = (v1_hps_wr_valid_o !== v2_hps_wr_valid_o);
  assign mm_c[8] = (v1_hps_wr_data_o !== v2_hps_wr_data_o);
  assign mm_c[9] = (v1_hps_wr_last_o !== v2_hps_wr_last_o);
  assign mm_c[10] = (v1_aud_wr_ready_o !== v2_aud_wr_ready_o);
  assign mm_c[11] = (v1_aud_refill_req_o !== v2_aud_refill_req_o);
  assign mm_c[12] = (v1_aud_occupancy_o !== v2_aud_occupancy_o);
  assign mm_c[13] = (v1_pcm_valid_o !== v2_pcm_valid_o);
  assign mm_c[14] = (v1_pcm_l_o !== v2_pcm_l_o);
  assign mm_c[15] = (v1_pcm_r_o !== v2_pcm_r_o);
  assign mm_c[16] = (v1_underrun_status_o !== v2_underrun_status_o);
  assign mm_c[17] = (v1_audio_underruns_o !== v2_audio_underruns_o);
  // MUTATED: the V2 side is negated, so this comparison
  // must report a divergence that is not there. If the
  // differential still passes, it is not comparing.
  assign mm_c[18] = (v1_px_valid_o !== ~v2_px_valid_o);
  assign mm_c[19] = (v1_px_rgb_o !== v2_px_rgb_o);
  assign mm_c[20] = (v1_px_x_o !== v2_px_x_o);
  assign mm_c[21] = (v1_px_y_o !== v2_px_y_o);
  assign mm_c[22] = (v1_px_hsync_o !== v2_px_hsync_o);
  assign mm_c[23] = (v1_px_vsync_o !== v2_px_vsync_o);
  assign mm_c[24] = (v1_px_hblank_o !== v2_px_hblank_o);
  assign mm_c[25] = (v1_px_vblank_o !== v2_px_vblank_o);
  assign mm_c[26] = (v1_scaler_violation_o !== v2_scaler_violation_o);
  assign mm_c[27] = (v1_crc_frame_o !== v2_crc_frame_o);
  assign mm_c[28] = (v1_crc_valid_o !== v2_crc_valid_o);
  assign mm_c[29] = (v1_crc_bytes_o !== v2_crc_bytes_o);
  assign mm_c[30] = (v1_crc_size_err_o !== v2_crc_size_err_o);
  assign mm_c[31] = (v1_gpu_tick_o !== v2_gpu_tick_o);
  assign mm_c[32] = (v1_gpu_tick_frame_id_o !== v2_gpu_tick_frame_id_o);
  assign mm_c[33] = (v1_gpu_tick_repeated_o !== v2_gpu_tick_repeated_o);
  assign mm_c[34] = (v1_gpu_complete_slot_o !== v2_gpu_complete_slot_o);
  assign mm_c[35] = (v1_deadline_faults_o !== v2_deadline_faults_o);
  assign mm_c[36] = (v1_frame_cycles_o !== v2_frame_cycles_o);
  assign mm_c[37] = (v1_slot_state_o[0] !== v2_slot_state_o[0]) ||
                     (v1_slot_state_o[1] !== v2_slot_state_o[1]) ||
                     (v1_slot_state_o[2] !== v2_slot_state_o[2]);
  assign mm_c[38] = (v1_fence_valid_o !== v2_fence_valid_o);
  assign mm_c[39] = (v1_fence_slot_o !== v2_fence_slot_o);
  assign mm_c[40] = (v1_fence_ok_o !== v2_fence_ok_o);
  assign mm_c[41] = (v1_fence_status_o !== v2_fence_status_o);
  assign mm_c[42] = (v1_mode_act_o !== v2_mode_act_o);
  assign mm_c[43] = (v1_dma_done_o !== v2_dma_done_o);
  assign mm_c[44] = (v1_dma_status_o !== v2_dma_status_o);
  assign mm_c[45] = (v1_blit_done_o !== v2_blit_done_o);
  assign mm_c[46] = (v1_blit_status_o !== v2_blit_status_o);
  assign mm_c[47] = (v1_pad_frame_flat_o !== v2_pad_frame_flat_o);
  assign mm_c[48] = (v1_pad_sequence_o[0] !== v2_pad_sequence_o[0]) ||
                     (v1_pad_sequence_o[1] !== v2_pad_sequence_o[1]) ||
                     (v1_pad_sequence_o[2] !== v2_pad_sequence_o[2]) ||
                     (v1_pad_sequence_o[3] !== v2_pad_sequence_o[3]);
  assign mm_c[49] = (v1_input_gaps_o !== v2_input_gaps_o);
  assign mm_c[50] = (v1_rumble_duty_o[0] !== v2_rumble_duty_o[0]) ||
                     (v1_rumble_duty_o[1] !== v2_rumble_duty_o[1]) ||
                     (v1_rumble_duty_o[2] !== v2_rumble_duty_o[2]) ||
                     (v1_rumble_duty_o[3] !== v2_rumble_duty_o[3]);
  assign mm_c[51] = (v1_rumble_active_o !== v2_rumble_active_o);
  assign mm_c[52] = (v1_rumble_pwm_o !== v2_rumble_pwm_o);
  assign mm_c[53] = (v1_rumble_drops_o !== v2_rumble_drops_o);
  assign mm_c[54] = (v1_cnt_snap_valid_o !== v2_cnt_snap_valid_o);
  assign mm_c[55] = (v1_cnt_snap_id_o !== v2_cnt_snap_id_o);
  assign mm_c[56] = (v1_cnt_snap_value_o !== v2_cnt_snap_value_o);
  assign mm_c[57] = (v1_cnt_window_open_o !== v2_cnt_window_open_o);
  assign mm_c[58] = (v1_cnt_cat_violation_o !== v2_cnt_cat_violation_o);
  assign mm_c[59] = (v1_guard_violations_o !== v2_guard_violations_o);
  assign mm_c[60] = (v1_starvation_o !== v2_starvation_o);
  assign mm_c[61] = (v1_init_done_o !== v2_init_done_o);
  assign mm_c[62] = (v1_refresh_stalls_o !== v2_refresh_stalls_o);
  assign mm_c[63] = (v1_bank_conflicts_o !== v2_bank_conflicts_o);
  assign mm_c[64] = (v1_scanout_preempted_o !== v2_scanout_preempted_o);
  assign mm_c[65] = (v1_hps_err_count_o !== v2_hps_err_count_o);
  assign mm_c[66] = (v1_shell_err_wfifo_o !== v2_shell_err_wfifo_o);
  assign mm_c[67] = (v1_shell_err_route_o !== v2_shell_err_route_o);
  assign mm_c[68] = (v1_shell_err_cdc_o !== v2_shell_err_cdc_o);
  assign mm_c[69] = (v1_shell_err_framer_o !== v2_shell_err_framer_o);
  assign mm_c[70] = (v1_geom_guard_rsp_o !== v2_geom_guard_rsp_o);
  assign mm_c[71] = (v1_geom_beat_valid_o !== v2_geom_beat_valid_o);
  assign mm_c[72] = (v1_geom_beat_data_o !== v2_geom_beat_data_o);
  assign mm_c[73] = (v1_geom_beat_last_o !== v2_geom_beat_last_o);
  assign mm_c[74] = (v1_render_busy_o !== v2_render_busy_o);
  assign mm_c[75] = (v1_render_pixels_o !== v2_render_pixels_o);
  assign mm_c[76] = (v1_render_bursts_o !== v2_render_bursts_o);
  assign mm_c[77] = (v1_render_stream_error_o !== v2_render_stream_error_o);
  assign mm_c[78] = (v1_render_drained_o !== v2_render_drained_o);
  assign mm_c[79] = (v1_render_fatal_o !== v2_render_fatal_o);
  assign mm_c[80] = (v1_render_issued_words_o !== v2_render_issued_words_o);
  assign mm_c[81] = (v1_render_retired_words_o !== v2_render_retired_words_o);
  assign mm_c[82] = (v1_phy_cs_n_o !== v2_phy_cs_n_o);
  assign mm_c[83] = (v1_phy_ras_n_o !== v2_phy_ras_n_o);
  assign mm_c[84] = (v1_phy_cas_n_o !== v2_phy_cas_n_o);
  assign mm_c[85] = (v1_phy_we_n_o !== v2_phy_we_n_o);
  assign mm_c[86] = (v1_phy_a_o !== v2_phy_a_o);
  assign mm_c[87] = (v1_phy_ba_o !== v2_phy_ba_o);
  assign mm_c[88] = (v1_phy_dq_o !== v2_phy_dq_o);
  assign mm_c[89] = (v1_phy_dq_oe_o !== v2_phy_dq_oe_o);
  assign mm_c[90] = (v1_phy_dqm_o !== v2_phy_dqm_o);
  assign mismatch_any_o = |mm_c;

  // THE INDEX OF THE FIRST OUTPUT TO DIVERGE, latched. A harness
  // that reports only "they differ" sends the reader hunting
  // through %d signals; the index names one. The generator
  // prints the table below so the number resolves to a port.
  logic sticky_q;
  logic [31:0] count_q, first_q;
  logic [31:0] first_index_c;
  always_comb begin
    first_index_c = 32'hFFFF_FFFF;
    for (int unsigned mmi = 0; mmi < 91; mmi++)
      if (mm_c[mmi] && (first_index_c == 32'hFFFF_FFFF))
        first_index_c = mmi;
  end
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      sticky_q <= 1'b0;
      count_q  <= 32'd0;
      first_q  <= 32'hFFFF_FFFF;
    end else if (mismatch_any_o) begin
      sticky_q <= 1'b1;
      count_q  <= count_q + 32'd1;
      if (!sticky_q) first_q <= first_index_c;
    end
  end
  assign mismatch_sticky_o = sticky_q;
  assign mismatch_count_o = count_q;
  assign mismatch_first_o = first_q;

  // ---- THE ACTIVITY WITNESS -------------------------------
  // Two shells that produce NOTHING agree perfectly. Without
  // this, a stimulus that never reaches the compared outputs --
  // a clock that does not tick, a reset never released, a domain
  // nothing drives -- passes the differential completely and
  // means nothing by it. "A gate that cannot reach the state is
  // not evidence about the state."
  //
  // So: one bit per compared output, set the first time V1's
  // copy of it CHANGES. The test asserts a floor on the count,
  // and the count is the honest measure of how much of this
  // comparison was actually exercised.
  //
  // ARMED ONE CYCLE LATE, and that detail is the difference
  // between a witness and a decoration. The delayed copies hold
  // x until they are first loaded, so an unguarded `!==`
  // compares every output against x on the first edge and marks
  // ALL of them toggled -- a witness that reports full coverage
  // of a run that has not started.
  logic  v1_ring_wr_valid_o_q;
  logic [1:0] v1_ring_wr_slot_o_q;
  logic [1:0] v1_ring_wr_state_o_q;
  logic  v1_hps_req_valid_o_q;
  logic  v1_hps_req_write_o_q;
  logic [31:0] v1_hps_req_addr_o_q;
  logic [6:0] v1_hps_req_len_o_q;
  logic  v1_hps_wr_valid_o_q;
  logic [63:0] v1_hps_wr_data_o_q;
  logic  v1_hps_wr_last_o_q;
  logic  v1_aud_wr_ready_o_q;
  logic  v1_aud_refill_req_o_q;
  logic [11:0] v1_aud_occupancy_o_q;
  logic  v1_pcm_valid_o_q;
  logic [15:0] v1_pcm_l_o_q;
  logic [15:0] v1_pcm_r_o_q;
  logic  v1_underrun_status_o_q;
  logic [31:0] v1_audio_underruns_o_q;
  logic  v1_px_valid_o_q;
  logic [15:0] v1_px_rgb_o_q;
  logic [9:0] v1_px_x_o_q;
  logic [7:0] v1_px_y_o_q;
  logic  v1_px_hsync_o_q;
  logic  v1_px_vsync_o_q;
  logic  v1_px_hblank_o_q;
  logic  v1_px_vblank_o_q;
  logic  v1_scaler_violation_o_q;
  logic [31:0] v1_crc_frame_o_q;
  logic  v1_crc_valid_o_q;
  logic [31:0] v1_crc_bytes_o_q;
  logic  v1_crc_size_err_o_q;
  logic  v1_gpu_tick_o_q;
  logic [31:0] v1_gpu_tick_frame_id_o_q;
  logic  v1_gpu_tick_repeated_o_q;
  logic [0:0] v1_gpu_complete_slot_o_q;
  logic [63:0] v1_deadline_faults_o_q;
  logic [63:0] v1_frame_cycles_o_q;
  logic [2:0] v1_slot_state_o_q[0:2];
  logic  v1_fence_valid_o_q;
  logic [1:0] v1_fence_slot_o_q;
  logic  v1_fence_ok_o_q;
  logic [7:0] v1_fence_status_o_q;
  logic [1:0] v1_mode_act_o_q;
  logic  v1_dma_done_o_q;
  logic [7:0] v1_dma_status_o_q;
  logic  v1_blit_done_o_q;
  logic [7:0] v1_blit_status_o_q;
  logic [639:0] v1_pad_frame_flat_o_q;
  logic [15:0] v1_pad_sequence_o_q[0:3];
  logic [63:0] v1_input_gaps_o_q;
  logic [7:0] v1_rumble_duty_o_q[0:3];
  logic [3:0] v1_rumble_active_o_q;
  logic [3:0] v1_rumble_pwm_o_q;
  logic [63:0] v1_rumble_drops_o_q;
  logic  v1_cnt_snap_valid_o_q;
  logic [15:0] v1_cnt_snap_id_o_q;
  logic [63:0] v1_cnt_snap_value_o_q;
  logic  v1_cnt_window_open_o_q;
  logic  v1_cnt_cat_violation_o_q;
  logic [31:0] v1_guard_violations_o_q;
  logic [63:0] v1_starvation_o_q;
  logic  v1_init_done_o_q;
  logic [31:0] v1_refresh_stalls_o_q;
  logic [31:0] v1_bank_conflicts_o_q;
  logic [31:0] v1_scanout_preempted_o_q;
  logic [31:0] v1_hps_err_count_o_q;
  logic  v1_shell_err_wfifo_o_q;
  logic  v1_shell_err_route_o_q;
  logic  v1_shell_err_cdc_o_q;
  logic  v1_shell_err_framer_o_q;
  zhao_guard_rsp_t v1_geom_guard_rsp_o_q;
  logic  v1_geom_beat_valid_o_q;
  logic [63:0] v1_geom_beat_data_o_q;
  logic  v1_geom_beat_last_o_q;
  logic  v1_render_busy_o_q;
  logic [31:0] v1_render_pixels_o_q;
  logic [31:0] v1_render_bursts_o_q;
  logic  v1_render_stream_error_o_q;
  logic  v1_render_drained_o_q;
  logic  v1_render_fatal_o_q;
  logic [31:0] v1_render_issued_words_o_q;
  logic [31:0] v1_render_retired_words_o_q;
  logic  v1_phy_cs_n_o_q;
  logic  v1_phy_ras_n_o_q;
  logic  v1_phy_cas_n_o_q;
  logic  v1_phy_we_n_o_q;
  logic [12:0] v1_phy_a_o_q;
  logic [1:0] v1_phy_ba_o_q;
  logic [15:0] v1_phy_dq_o_q;
  logic  v1_phy_dq_oe_o_q;
  logic [1:0] v1_phy_dqm_o_q;
  logic [90:0] toggled_q;
  logic armed_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      toggled_q <= '0;
      armed_q   <= 1'b0;
    end else begin
      armed_q <= 1'b1;
      if (armed_q) begin
        if (v1_ring_wr_valid_o !== v1_ring_wr_valid_o_q) toggled_q[0] <= 1'b1;
        if (v1_ring_wr_slot_o !== v1_ring_wr_slot_o_q) toggled_q[1] <= 1'b1;
        if (v1_ring_wr_state_o !== v1_ring_wr_state_o_q) toggled_q[2] <= 1'b1;
        if (v1_hps_req_valid_o !== v1_hps_req_valid_o_q) toggled_q[3] <= 1'b1;
        if (v1_hps_req_write_o !== v1_hps_req_write_o_q) toggled_q[4] <= 1'b1;
        if (v1_hps_req_addr_o !== v1_hps_req_addr_o_q) toggled_q[5] <= 1'b1;
        if (v1_hps_req_len_o !== v1_hps_req_len_o_q) toggled_q[6] <= 1'b1;
        if (v1_hps_wr_valid_o !== v1_hps_wr_valid_o_q) toggled_q[7] <= 1'b1;
        if (v1_hps_wr_data_o !== v1_hps_wr_data_o_q) toggled_q[8] <= 1'b1;
        if (v1_hps_wr_last_o !== v1_hps_wr_last_o_q) toggled_q[9] <= 1'b1;
        if (v1_aud_wr_ready_o !== v1_aud_wr_ready_o_q) toggled_q[10] <= 1'b1;
        if (v1_aud_refill_req_o !== v1_aud_refill_req_o_q) toggled_q[11] <= 1'b1;
        if (v1_aud_occupancy_o !== v1_aud_occupancy_o_q) toggled_q[12] <= 1'b1;
        if (v1_pcm_valid_o !== v1_pcm_valid_o_q) toggled_q[13] <= 1'b1;
        if (v1_pcm_l_o !== v1_pcm_l_o_q) toggled_q[14] <= 1'b1;
        if (v1_pcm_r_o !== v1_pcm_r_o_q) toggled_q[15] <= 1'b1;
        if (v1_underrun_status_o !== v1_underrun_status_o_q) toggled_q[16] <= 1'b1;
        if (v1_audio_underruns_o !== v1_audio_underruns_o_q) toggled_q[17] <= 1'b1;
        if (v1_px_valid_o !== v1_px_valid_o_q) toggled_q[18] <= 1'b1;
        if (v1_px_rgb_o !== v1_px_rgb_o_q) toggled_q[19] <= 1'b1;
        if (v1_px_x_o !== v1_px_x_o_q) toggled_q[20] <= 1'b1;
        if (v1_px_y_o !== v1_px_y_o_q) toggled_q[21] <= 1'b1;
        if (v1_px_hsync_o !== v1_px_hsync_o_q) toggled_q[22] <= 1'b1;
        if (v1_px_vsync_o !== v1_px_vsync_o_q) toggled_q[23] <= 1'b1;
        if (v1_px_hblank_o !== v1_px_hblank_o_q) toggled_q[24] <= 1'b1;
        if (v1_px_vblank_o !== v1_px_vblank_o_q) toggled_q[25] <= 1'b1;
        if (v1_scaler_violation_o !== v1_scaler_violation_o_q) toggled_q[26] <= 1'b1;
        if (v1_crc_frame_o !== v1_crc_frame_o_q) toggled_q[27] <= 1'b1;
        if (v1_crc_valid_o !== v1_crc_valid_o_q) toggled_q[28] <= 1'b1;
        if (v1_crc_bytes_o !== v1_crc_bytes_o_q) toggled_q[29] <= 1'b1;
        if (v1_crc_size_err_o !== v1_crc_size_err_o_q) toggled_q[30] <= 1'b1;
        if (v1_gpu_tick_o !== v1_gpu_tick_o_q) toggled_q[31] <= 1'b1;
        if (v1_gpu_tick_frame_id_o !== v1_gpu_tick_frame_id_o_q) toggled_q[32] <= 1'b1;
        if (v1_gpu_tick_repeated_o !== v1_gpu_tick_repeated_o_q) toggled_q[33] <= 1'b1;
        if (v1_gpu_complete_slot_o !== v1_gpu_complete_slot_o_q) toggled_q[34] <= 1'b1;
        if (v1_deadline_faults_o !== v1_deadline_faults_o_q) toggled_q[35] <= 1'b1;
        if (v1_frame_cycles_o !== v1_frame_cycles_o_q) toggled_q[36] <= 1'b1;
        if ((v1_slot_state_o[0] !== v1_slot_state_o_q[0]) ||
            (v1_slot_state_o[1] !== v1_slot_state_o_q[1]) ||
            (v1_slot_state_o[2] !== v1_slot_state_o_q[2])) toggled_q[37] <= 1'b1;
        if (v1_fence_valid_o !== v1_fence_valid_o_q) toggled_q[38] <= 1'b1;
        if (v1_fence_slot_o !== v1_fence_slot_o_q) toggled_q[39] <= 1'b1;
        if (v1_fence_ok_o !== v1_fence_ok_o_q) toggled_q[40] <= 1'b1;
        if (v1_fence_status_o !== v1_fence_status_o_q) toggled_q[41] <= 1'b1;
        if (v1_mode_act_o !== v1_mode_act_o_q) toggled_q[42] <= 1'b1;
        if (v1_dma_done_o !== v1_dma_done_o_q) toggled_q[43] <= 1'b1;
        if (v1_dma_status_o !== v1_dma_status_o_q) toggled_q[44] <= 1'b1;
        if (v1_blit_done_o !== v1_blit_done_o_q) toggled_q[45] <= 1'b1;
        if (v1_blit_status_o !== v1_blit_status_o_q) toggled_q[46] <= 1'b1;
        if (v1_pad_frame_flat_o !== v1_pad_frame_flat_o_q) toggled_q[47] <= 1'b1;
        if ((v1_pad_sequence_o[0] !== v1_pad_sequence_o_q[0]) ||
            (v1_pad_sequence_o[1] !== v1_pad_sequence_o_q[1]) ||
            (v1_pad_sequence_o[2] !== v1_pad_sequence_o_q[2]) ||
            (v1_pad_sequence_o[3] !== v1_pad_sequence_o_q[3])) toggled_q[48] <= 1'b1;
        if (v1_input_gaps_o !== v1_input_gaps_o_q) toggled_q[49] <= 1'b1;
        if ((v1_rumble_duty_o[0] !== v1_rumble_duty_o_q[0]) ||
            (v1_rumble_duty_o[1] !== v1_rumble_duty_o_q[1]) ||
            (v1_rumble_duty_o[2] !== v1_rumble_duty_o_q[2]) ||
            (v1_rumble_duty_o[3] !== v1_rumble_duty_o_q[3])) toggled_q[50] <= 1'b1;
        if (v1_rumble_active_o !== v1_rumble_active_o_q) toggled_q[51] <= 1'b1;
        if (v1_rumble_pwm_o !== v1_rumble_pwm_o_q) toggled_q[52] <= 1'b1;
        if (v1_rumble_drops_o !== v1_rumble_drops_o_q) toggled_q[53] <= 1'b1;
        if (v1_cnt_snap_valid_o !== v1_cnt_snap_valid_o_q) toggled_q[54] <= 1'b1;
        if (v1_cnt_snap_id_o !== v1_cnt_snap_id_o_q) toggled_q[55] <= 1'b1;
        if (v1_cnt_snap_value_o !== v1_cnt_snap_value_o_q) toggled_q[56] <= 1'b1;
        if (v1_cnt_window_open_o !== v1_cnt_window_open_o_q) toggled_q[57] <= 1'b1;
        if (v1_cnt_cat_violation_o !== v1_cnt_cat_violation_o_q) toggled_q[58] <= 1'b1;
        if (v1_guard_violations_o !== v1_guard_violations_o_q) toggled_q[59] <= 1'b1;
        if (v1_starvation_o !== v1_starvation_o_q) toggled_q[60] <= 1'b1;
        if (v1_init_done_o !== v1_init_done_o_q) toggled_q[61] <= 1'b1;
        if (v1_refresh_stalls_o !== v1_refresh_stalls_o_q) toggled_q[62] <= 1'b1;
        if (v1_bank_conflicts_o !== v1_bank_conflicts_o_q) toggled_q[63] <= 1'b1;
        if (v1_scanout_preempted_o !== v1_scanout_preempted_o_q) toggled_q[64] <= 1'b1;
        if (v1_hps_err_count_o !== v1_hps_err_count_o_q) toggled_q[65] <= 1'b1;
        if (v1_shell_err_wfifo_o !== v1_shell_err_wfifo_o_q) toggled_q[66] <= 1'b1;
        if (v1_shell_err_route_o !== v1_shell_err_route_o_q) toggled_q[67] <= 1'b1;
        if (v1_shell_err_cdc_o !== v1_shell_err_cdc_o_q) toggled_q[68] <= 1'b1;
        if (v1_shell_err_framer_o !== v1_shell_err_framer_o_q) toggled_q[69] <= 1'b1;
        if (v1_geom_guard_rsp_o !== v1_geom_guard_rsp_o_q) toggled_q[70] <= 1'b1;
        if (v1_geom_beat_valid_o !== v1_geom_beat_valid_o_q) toggled_q[71] <= 1'b1;
        if (v1_geom_beat_data_o !== v1_geom_beat_data_o_q) toggled_q[72] <= 1'b1;
        if (v1_geom_beat_last_o !== v1_geom_beat_last_o_q) toggled_q[73] <= 1'b1;
        if (v1_render_busy_o !== v1_render_busy_o_q) toggled_q[74] <= 1'b1;
        if (v1_render_pixels_o !== v1_render_pixels_o_q) toggled_q[75] <= 1'b1;
        if (v1_render_bursts_o !== v1_render_bursts_o_q) toggled_q[76] <= 1'b1;
        if (v1_render_stream_error_o !== v1_render_stream_error_o_q) toggled_q[77] <= 1'b1;
        if (v1_render_drained_o !== v1_render_drained_o_q) toggled_q[78] <= 1'b1;
        if (v1_render_fatal_o !== v1_render_fatal_o_q) toggled_q[79] <= 1'b1;
        if (v1_render_issued_words_o !== v1_render_issued_words_o_q) toggled_q[80] <= 1'b1;
        if (v1_render_retired_words_o !== v1_render_retired_words_o_q) toggled_q[81] <= 1'b1;
        if (v1_phy_cs_n_o !== v1_phy_cs_n_o_q) toggled_q[82] <= 1'b1;
        if (v1_phy_ras_n_o !== v1_phy_ras_n_o_q) toggled_q[83] <= 1'b1;
        if (v1_phy_cas_n_o !== v1_phy_cas_n_o_q) toggled_q[84] <= 1'b1;
        if (v1_phy_we_n_o !== v1_phy_we_n_o_q) toggled_q[85] <= 1'b1;
        if (v1_phy_a_o !== v1_phy_a_o_q) toggled_q[86] <= 1'b1;
        if (v1_phy_ba_o !== v1_phy_ba_o_q) toggled_q[87] <= 1'b1;
        if (v1_phy_dq_o !== v1_phy_dq_o_q) toggled_q[88] <= 1'b1;
        if (v1_phy_dq_oe_o !== v1_phy_dq_oe_o_q) toggled_q[89] <= 1'b1;
        if (v1_phy_dqm_o !== v1_phy_dqm_o_q) toggled_q[90] <= 1'b1;
      end
    end
  end
  always_ff @(posedge gpu_clk) begin
    v1_ring_wr_valid_o_q <= v1_ring_wr_valid_o;
    v1_ring_wr_slot_o_q <= v1_ring_wr_slot_o;
    v1_ring_wr_state_o_q <= v1_ring_wr_state_o;
    v1_hps_req_valid_o_q <= v1_hps_req_valid_o;
    v1_hps_req_write_o_q <= v1_hps_req_write_o;
    v1_hps_req_addr_o_q <= v1_hps_req_addr_o;
    v1_hps_req_len_o_q <= v1_hps_req_len_o;
    v1_hps_wr_valid_o_q <= v1_hps_wr_valid_o;
    v1_hps_wr_data_o_q <= v1_hps_wr_data_o;
    v1_hps_wr_last_o_q <= v1_hps_wr_last_o;
    v1_aud_wr_ready_o_q <= v1_aud_wr_ready_o;
    v1_aud_refill_req_o_q <= v1_aud_refill_req_o;
    v1_aud_occupancy_o_q <= v1_aud_occupancy_o;
    v1_pcm_valid_o_q <= v1_pcm_valid_o;
    v1_pcm_l_o_q <= v1_pcm_l_o;
    v1_pcm_r_o_q <= v1_pcm_r_o;
    v1_underrun_status_o_q <= v1_underrun_status_o;
    v1_audio_underruns_o_q <= v1_audio_underruns_o;
    v1_px_valid_o_q <= v1_px_valid_o;
    v1_px_rgb_o_q <= v1_px_rgb_o;
    v1_px_x_o_q <= v1_px_x_o;
    v1_px_y_o_q <= v1_px_y_o;
    v1_px_hsync_o_q <= v1_px_hsync_o;
    v1_px_vsync_o_q <= v1_px_vsync_o;
    v1_px_hblank_o_q <= v1_px_hblank_o;
    v1_px_vblank_o_q <= v1_px_vblank_o;
    v1_scaler_violation_o_q <= v1_scaler_violation_o;
    v1_crc_frame_o_q <= v1_crc_frame_o;
    v1_crc_valid_o_q <= v1_crc_valid_o;
    v1_crc_bytes_o_q <= v1_crc_bytes_o;
    v1_crc_size_err_o_q <= v1_crc_size_err_o;
    v1_gpu_tick_o_q <= v1_gpu_tick_o;
    v1_gpu_tick_frame_id_o_q <= v1_gpu_tick_frame_id_o;
    v1_gpu_tick_repeated_o_q <= v1_gpu_tick_repeated_o;
    v1_gpu_complete_slot_o_q <= v1_gpu_complete_slot_o;
    v1_deadline_faults_o_q <= v1_deadline_faults_o;
    v1_frame_cycles_o_q <= v1_frame_cycles_o;
    v1_slot_state_o_q <= v1_slot_state_o;
    v1_fence_valid_o_q <= v1_fence_valid_o;
    v1_fence_slot_o_q <= v1_fence_slot_o;
    v1_fence_ok_o_q <= v1_fence_ok_o;
    v1_fence_status_o_q <= v1_fence_status_o;
    v1_mode_act_o_q <= v1_mode_act_o;
    v1_dma_done_o_q <= v1_dma_done_o;
    v1_dma_status_o_q <= v1_dma_status_o;
    v1_blit_done_o_q <= v1_blit_done_o;
    v1_blit_status_o_q <= v1_blit_status_o;
    v1_pad_frame_flat_o_q <= v1_pad_frame_flat_o;
    v1_pad_sequence_o_q <= v1_pad_sequence_o;
    v1_input_gaps_o_q <= v1_input_gaps_o;
    v1_rumble_duty_o_q <= v1_rumble_duty_o;
    v1_rumble_active_o_q <= v1_rumble_active_o;
    v1_rumble_pwm_o_q <= v1_rumble_pwm_o;
    v1_rumble_drops_o_q <= v1_rumble_drops_o;
    v1_cnt_snap_valid_o_q <= v1_cnt_snap_valid_o;
    v1_cnt_snap_id_o_q <= v1_cnt_snap_id_o;
    v1_cnt_snap_value_o_q <= v1_cnt_snap_value_o;
    v1_cnt_window_open_o_q <= v1_cnt_window_open_o;
    v1_cnt_cat_violation_o_q <= v1_cnt_cat_violation_o;
    v1_guard_violations_o_q <= v1_guard_violations_o;
    v1_starvation_o_q <= v1_starvation_o;
    v1_init_done_o_q <= v1_init_done_o;
    v1_refresh_stalls_o_q <= v1_refresh_stalls_o;
    v1_bank_conflicts_o_q <= v1_bank_conflicts_o;
    v1_scanout_preempted_o_q <= v1_scanout_preempted_o;
    v1_hps_err_count_o_q <= v1_hps_err_count_o;
    v1_shell_err_wfifo_o_q <= v1_shell_err_wfifo_o;
    v1_shell_err_route_o_q <= v1_shell_err_route_o;
    v1_shell_err_cdc_o_q <= v1_shell_err_cdc_o;
    v1_shell_err_framer_o_q <= v1_shell_err_framer_o;
    v1_geom_guard_rsp_o_q <= v1_geom_guard_rsp_o;
    v1_geom_beat_valid_o_q <= v1_geom_beat_valid_o;
    v1_geom_beat_data_o_q <= v1_geom_beat_data_o;
    v1_geom_beat_last_o_q <= v1_geom_beat_last_o;
    v1_render_busy_o_q <= v1_render_busy_o;
    v1_render_pixels_o_q <= v1_render_pixels_o;
    v1_render_bursts_o_q <= v1_render_bursts_o;
    v1_render_stream_error_o_q <= v1_render_stream_error_o;
    v1_render_drained_o_q <= v1_render_drained_o;
    v1_render_fatal_o_q <= v1_render_fatal_o;
    v1_render_issued_words_o_q <= v1_render_issued_words_o;
    v1_render_retired_words_o_q <= v1_render_retired_words_o;
    v1_phy_cs_n_o_q <= v1_phy_cs_n_o;
    v1_phy_ras_n_o_q <= v1_phy_ras_n_o;
    v1_phy_cas_n_o_q <= v1_phy_cas_n_o;
    v1_phy_we_n_o_q <= v1_phy_we_n_o;
    v1_phy_a_o_q <= v1_phy_a_o;
    v1_phy_ba_o_q <= v1_phy_ba_o;
    v1_phy_dq_o_q <= v1_phy_dq_o;
    v1_phy_dq_oe_o_q <= v1_phy_dq_oe_o;
    v1_phy_dqm_o_q <= v1_phy_dqm_o;
  end

  always_comb begin
    toggled_count_o = 32'd0;
    for (int unsigned mmi = 0; mmi < 91; mmi++)
      if (toggled_q[mmi]) toggled_count_o = toggled_count_o + 1;
  end

  // The compared outputs, in bit order, so `mismatch_first_o`
  // resolves to a name without re-running the generator:
  //     0  ring_wr_valid_o
  //     1  ring_wr_slot_o
  //     2  ring_wr_state_o
  //     3  hps_req_valid_o
  //     4  hps_req_write_o
  //     5  hps_req_addr_o
  //     6  hps_req_len_o
  //     7  hps_wr_valid_o
  //     8  hps_wr_data_o
  //     9  hps_wr_last_o
  //    10  aud_wr_ready_o
  //    11  aud_refill_req_o
  //    12  aud_occupancy_o
  //    13  pcm_valid_o
  //    14  pcm_l_o
  //    15  pcm_r_o
  //    16  underrun_status_o
  //    17  audio_underruns_o
  //    18  px_valid_o
  //    19  px_rgb_o
  //    20  px_x_o
  //    21  px_y_o
  //    22  px_hsync_o
  //    23  px_vsync_o
  //    24  px_hblank_o
  //    25  px_vblank_o
  //    26  scaler_violation_o
  //    27  crc_frame_o
  //    28  crc_valid_o
  //    29  crc_bytes_o
  //    30  crc_size_err_o
  //    31  gpu_tick_o
  //    32  gpu_tick_frame_id_o
  //    33  gpu_tick_repeated_o
  //    34  gpu_complete_slot_o
  //    35  deadline_faults_o
  //    36  frame_cycles_o
  //    37  slot_state_o
  //    38  fence_valid_o
  //    39  fence_slot_o
  //    40  fence_ok_o
  //    41  fence_status_o
  //    42  mode_act_o
  //    43  dma_done_o
  //    44  dma_status_o
  //    45  blit_done_o
  //    46  blit_status_o
  //    47  pad_frame_flat_o
  //    48  pad_sequence_o
  //    49  input_gaps_o
  //    50  rumble_duty_o
  //    51  rumble_active_o
  //    52  rumble_pwm_o
  //    53  rumble_drops_o
  //    54  cnt_snap_valid_o
  //    55  cnt_snap_id_o
  //    56  cnt_snap_value_o
  //    57  cnt_window_open_o
  //    58  cnt_cat_violation_o
  //    59  guard_violations_o
  //    60  starvation_o
  //    61  init_done_o
  //    62  refresh_stalls_o
  //    63  bank_conflicts_o
  //    64  scanout_preempted_o
  //    65  hps_err_count_o
  //    66  shell_err_wfifo_o
  //    67  shell_err_route_o
  //    68  shell_err_cdc_o
  //    69  shell_err_framer_o
  //    70  geom_guard_rsp_o
  //    71  geom_beat_valid_o
  //    72  geom_beat_data_o
  //    73  geom_beat_last_o
  //    74  render_busy_o
  //    75  render_pixels_o
  //    76  render_bursts_o
  //    77  render_stream_error_o
  //    78  render_drained_o
  //    79  render_fatal_o
  //    80  render_issued_words_o
  //    81  render_retired_words_o
  //    82  phy_cs_n_o
  //    83  phy_ras_n_o
  //    84  phy_cas_n_o
  //    85  phy_we_n_o
  //    86  phy_a_o
  //    87  phy_ba_o
  //    88  phy_dq_o
  //    89  phy_dq_oe_o
  //    90  phy_dqm_o

endmodule : zhao_shell_paired_diff_mut
