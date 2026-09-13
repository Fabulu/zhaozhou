// GENERATED FILE -- DO NOT EDIT.
// Generator: tools/quartus/gen_shell_fit_top.py
// shell-declaration-sha256: a243647455d61afa653a14127a5bc489c7361f767f7e36c880254d81a46e1739
// policy-sha256: e153ed6e4f19ef9855a0f97784886d9ea323245ce4999f3da8696f1178ceb91a
// generator-sha256: f34a4609e362f4052a78bcc74f3414ef7e06a1c26da0abafab33abdfb22e73b7
// parser-sha256: d4da568555ca520691fec7db7dd2893d7a34bc5b5a0aeda879e9d407fc4572a9
// packet-rom-sha256: bf1363eb06c8a58cb63e6a82608b1321279a4dc4178fd9497b9b90ed31942b51
// Traffic is deterministic legal-ish characterization stimulus, not an HPS/SDRAM model.

module zhao_shell_fit_top
  import zhao_pkg::*, zhao_abi_pkg::*;
(
  input  logic       gpu_clk,
  input  logic       vid_clk,
  input  logic       audio_clk,
  input  logic       rst_n,
  output logic [2:0] fit_signature_o,
  output logic [2:0] fit_epoch_o
);

  // Exact shell boundary aliases. Inputs are registered in u_stimulus;
  // outputs feed native-domain capture banks for signatures and accounting.
  // Renderer ready additionally feeds its same-edge ready/valid decision directly.
  (* keep = "true" *) logic [1:0] shell_hps_state_i [0:2];
  (* keep = "true" *) logic [31:0] shell_hps_byte_len_i [0:2];
  (* keep = "true" *) logic shell_ring_wr_valid_o;
  (* keep = "true" *) logic [1:0] shell_ring_wr_slot_o;
  (* keep = "true" *) logic [1:0] shell_ring_wr_state_o;
  (* keep = "true" *) logic shell_ring_wr_ready_i;
  (* keep = "true" *) logic shell_hps_req_valid_o;
  (* keep = "true" *) logic shell_hps_req_write_o;
  (* keep = "true" *) logic [31:0] shell_hps_req_addr_o;
  (* keep = "true" *) logic [6:0] shell_hps_req_len_o;
  (* keep = "true" *) logic shell_hps_req_grant_i;
  (* keep = "true" *) logic shell_hps_wr_valid_o;
  (* keep = "true" *) logic [63:0] shell_hps_wr_data_o;
  (* keep = "true" *) logic shell_hps_wr_last_o;
  (* keep = "true" *) logic shell_hps_rd_valid_i;
  (* keep = "true" *) logic [63:0] shell_hps_rd_data_i;
  (* keep = "true" *) logic shell_hps_rd_last_i;
  (* keep = "true" *) logic [3:0] shell_pad_present_i;
  (* keep = "true" *) logic [31:0] shell_pad_buttons_i [0:3];
  (* keep = "true" *) logic [15:0] shell_pad_lx_i [0:3];
  (* keep = "true" *) logic [15:0] shell_pad_ly_i [0:3];
  (* keep = "true" *) logic [15:0] shell_pad_rx_i [0:3];
  (* keep = "true" *) logic [15:0] shell_pad_ry_i [0:3];
  (* keep = "true" *) logic shell_aud_wr_valid_i;
  (* keep = "true" *) logic [15:0] shell_aud_wr_l_i;
  (* keep = "true" *) logic [15:0] shell_aud_wr_r_i;
  (* keep = "true" *) logic shell_aud_wr_ready_o;
  (* keep = "true" *) logic shell_aud_refill_req_o;
  (* keep = "true" *) logic [11:0] shell_aud_occupancy_o;
  (* keep = "true" *) logic shell_pcm_valid_o;
  (* keep = "true" *) logic [15:0] shell_pcm_l_o;
  (* keep = "true" *) logic [15:0] shell_pcm_r_o;
  (* keep = "true" *) logic shell_underrun_status_o;
  (* keep = "true" *) logic [31:0] shell_audio_underruns_o;
  (* keep = "true" *) logic shell_px_valid_o;
  (* keep = "true" *) logic [15:0] shell_px_rgb_o;
  (* keep = "true" *) logic [9:0] shell_px_x_o;
  (* keep = "true" *) logic [7:0] shell_px_y_o;
  (* keep = "true" *) logic shell_px_hsync_o;
  (* keep = "true" *) logic shell_px_vsync_o;
  (* keep = "true" *) logic shell_px_hblank_o;
  (* keep = "true" *) logic shell_px_vblank_o;
  (* keep = "true" *) logic shell_scaler_violation_o;
  (* keep = "true" *) logic [31:0] shell_crc_frame_o;
  (* keep = "true" *) logic shell_crc_valid_o;
  (* keep = "true" *) logic [31:0] shell_crc_bytes_o;
  (* keep = "true" *) logic shell_crc_size_err_o;
  (* keep = "true" *) logic shell_gpu_tick_o;
  (* keep = "true" *) logic [31:0] shell_gpu_tick_frame_id_o;
  (* keep = "true" *) logic shell_gpu_tick_repeated_o;
  (* keep = "true" *) logic [0:0] shell_gpu_complete_slot_o;
  (* keep = "true" *) logic [63:0] shell_deadline_faults_o;
  (* keep = "true" *) logic [63:0] shell_frame_cycles_o;
  (* keep = "true" *) logic [2:0] shell_slot_state_o [0:2];
  (* keep = "true" *) logic shell_fence_valid_o;
  (* keep = "true" *) logic [1:0] shell_fence_slot_o;
  (* keep = "true" *) logic shell_fence_ok_o;
  (* keep = "true" *) logic [7:0] shell_fence_status_o;
  (* keep = "true" *) logic [1:0] shell_mode_act_o;
  (* keep = "true" *) logic shell_dma_done_o;
  (* keep = "true" *) logic [7:0] shell_dma_status_o;
  (* keep = "true" *) logic shell_blit_done_o;
  (* keep = "true" *) logic [7:0] shell_blit_status_o;
  (* keep = "true" *) logic [639:0] shell_pad_frame_flat_o;
  (* keep = "true" *) logic [15:0] shell_pad_sequence_o [0:3];
  (* keep = "true" *) logic [63:0] shell_input_gaps_o;
  (* keep = "true" *) logic [7:0] shell_rumble_duty_o [0:3];
  (* keep = "true" *) logic [3:0] shell_rumble_active_o;
  (* keep = "true" *) logic [3:0] shell_rumble_pwm_o;
  (* keep = "true" *) logic [63:0] shell_rumble_drops_o;
  (* keep = "true" *) logic shell_cnt_snap_ready_i;
  (* keep = "true" *) logic shell_cnt_snap_valid_o;
  (* keep = "true" *) logic [15:0] shell_cnt_snap_id_o;
  (* keep = "true" *) logic [63:0] shell_cnt_snap_value_o;
  (* keep = "true" *) logic shell_cnt_window_open_o;
  (* keep = "true" *) logic shell_cnt_cat_violation_o;
  (* keep = "true" *) logic [31:0] shell_guard_violations_o;
  (* keep = "true" *) logic [63:0] shell_starvation_o;
  (* keep = "true" *) logic shell_init_done_o;
  (* keep = "true" *) logic [31:0] shell_refresh_stalls_o;
  (* keep = "true" *) logic [31:0] shell_bank_conflicts_o;
  (* keep = "true" *) logic [31:0] shell_scanout_preempted_o;
  (* keep = "true" *) logic [31:0] shell_hps_err_count_o;
  (* keep = "true" *) logic shell_shell_err_wfifo_o;
  (* keep = "true" *) logic shell_shell_err_route_o;
  (* keep = "true" *) logic shell_shell_err_cdc_o;
  (* keep = "true" *) logic shell_shell_err_framer_o;
  (* keep = "true" *) logic shell_render_frame_begin_i;
  (* keep = "true" *) logic shell_render_frame_end_i;
  (* keep = "true" *) logic [5:0] shell_render_grid_w_i;
  (* keep = "true" *) logic [5:0] shell_render_grid_h_i;
  (* keep = "true" *) logic shell_render_tri_valid_i;
  (* keep = "true" *) logic shell_render_tri_ready_o;
  (* keep = "true" *) zhao_guard_req_t shell_geom_guard_req_i;
  (* keep = "true" *) zhao_guard_rsp_t shell_geom_guard_rsp_o;
  (* keep = "true" *) logic shell_geom_beat_valid_o;
  (* keep = "true" *) logic [63:0] shell_geom_beat_data_o;
  (* keep = "true" *) logic shell_geom_beat_last_o;
  (* keep = "true" *) logic signed [22:0] shell_render_kx0_i;
  (* keep = "true" *) logic signed [22:0] shell_render_ky0_i;
  (* keep = "true" *) logic signed [47:0] shell_render_kc0_i;
  (* keep = "true" *) logic signed [22:0] shell_render_kx1_i;
  (* keep = "true" *) logic signed [22:0] shell_render_ky1_i;
  (* keep = "true" *) logic signed [47:0] shell_render_kc1_i;
  (* keep = "true" *) logic signed [22:0] shell_render_kx2_i;
  (* keep = "true" *) logic signed [22:0] shell_render_ky2_i;
  (* keep = "true" *) logic signed [47:0] shell_render_kc2_i;
  (* keep = "true" *) logic [2:0] shell_render_tl_i;
  (* keep = "true" *) logic signed [20:0] shell_render_ax_i;
  (* keep = "true" *) logic signed [20:0] shell_render_ay_i;
  (* keep = "true" *) logic signed [20:0] shell_render_bx_i;
  (* keep = "true" *) logic signed [20:0] shell_render_by_i;
  (* keep = "true" *) logic signed [20:0] shell_render_cx_i;
  (* keep = "true" *) logic signed [20:0] shell_render_cy_i;
  (* keep = "true" *) logic signed [11:0] shell_render_min_x_i;
  (* keep = "true" *) logic signed [11:0] shell_render_max_x_i;
  (* keep = "true" *) logic signed [11:0] shell_render_min_y_i;
  (* keep = "true" *) logic signed [11:0] shell_render_max_y_i;
  (* keep = "true" *) logic [15:0] shell_render_src_id_i;
  (* keep = "true" *) logic [63:0] shell_render_fill_word_i;
  (* keep = "true" *) logic [63:0] shell_render_clear_word_i;
  (* keep = "true" *) logic [31:0] shell_render_state_i;
  (* keep = "true" *) logic [7:0] shell_render_src_a_i;
  (* keep = "true" *) logic [23:0] shell_render_texel_rgb_i;
  (* keep = "true" *) logic [7:0] shell_render_texel_a_i;
  (* keep = "true" *) logic [7:0] shell_render_texel_idx_i;
  (* keep = "true" *) logic [26:0] shell_render_fb_base_i;
  (* keep = "true" *) logic [15:0] shell_render_fb_stride_i;
  (* keep = "true" *) logic shell_fb_writer_i;
  (* keep = "true" *) logic shell_render_drain_done_o;
  (* keep = "true" *) logic shell_render_busy_o;
  (* keep = "true" *) logic [31:0] shell_render_pixels_o;
  (* keep = "true" *) logic [31:0] shell_render_bursts_o;
  (* keep = "true" *) logic shell_render_stream_error_o;
  (* keep = "true" *) logic shell_render_drained_o;
  (* keep = "true" *) logic shell_render_fatal_o;
  (* keep = "true" *) logic [31:0] shell_render_issued_words_o;
  (* keep = "true" *) logic [31:0] shell_render_retired_words_o;
  (* keep = "true" *) logic shell_render_overflow_o;
  (* keep = "true" *) logic shell_render_fragment_error_o;
  (* keep = "true" *) logic shell_phy_cs_n_o;
  (* keep = "true" *) logic shell_phy_ras_n_o;
  (* keep = "true" *) logic shell_phy_cas_n_o;
  (* keep = "true" *) logic shell_phy_we_n_o;
  (* keep = "true" *) logic [12:0] shell_phy_a_o;
  (* keep = "true" *) logic [1:0] shell_phy_ba_o;
  (* keep = "true" *) logic [15:0] shell_phy_dq_o;
  (* keep = "true" *) logic shell_phy_dq_oe_o;
  (* keep = "true" *) logic [1:0] shell_phy_dqm_o;
  (* keep = "true" *) logic [15:0] shell_phy_dq_i;

  logic [1694:0] gpu_payload_c;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [1694:0] gpu_capture_bus;
  /* verilator lint_on UNUSEDSIGNAL */
  always_comb begin
    gpu_payload_c = '0;
    gpu_payload_c[0 +: 1] = shell_ring_wr_valid_o;
    gpu_payload_c[1 +: 2] = shell_ring_wr_slot_o;
    gpu_payload_c[3 +: 2] = shell_ring_wr_state_o;
    gpu_payload_c[5 +: 1] = shell_hps_req_valid_o;
    gpu_payload_c[6 +: 1] = shell_hps_req_write_o;
    gpu_payload_c[7 +: 32] = shell_hps_req_addr_o;
    gpu_payload_c[39 +: 7] = shell_hps_req_len_o;
    gpu_payload_c[46 +: 1] = shell_hps_wr_valid_o;
    gpu_payload_c[47 +: 64] = shell_hps_wr_data_o;
    gpu_payload_c[111 +: 1] = shell_hps_wr_last_o;
    gpu_payload_c[112 +: 1] = shell_aud_wr_ready_o;
    gpu_payload_c[113 +: 1] = shell_aud_refill_req_o;
    gpu_payload_c[114 +: 12] = shell_aud_occupancy_o;
    gpu_payload_c[126 +: 32] = shell_crc_frame_o;
    gpu_payload_c[158 +: 1] = shell_crc_valid_o;
    gpu_payload_c[159 +: 32] = shell_crc_bytes_o;
    gpu_payload_c[191 +: 1] = shell_crc_size_err_o;
    gpu_payload_c[192 +: 1] = shell_gpu_tick_o;
    gpu_payload_c[193 +: 32] = shell_gpu_tick_frame_id_o;
    gpu_payload_c[225 +: 1] = shell_gpu_tick_repeated_o;
    gpu_payload_c[226 +: 1] = shell_gpu_complete_slot_o;
    gpu_payload_c[227 +: 3] = shell_slot_state_o[0];
    gpu_payload_c[230 +: 3] = shell_slot_state_o[1];
    gpu_payload_c[233 +: 3] = shell_slot_state_o[2];
    gpu_payload_c[236 +: 1] = shell_fence_valid_o;
    gpu_payload_c[237 +: 2] = shell_fence_slot_o;
    gpu_payload_c[239 +: 1] = shell_fence_ok_o;
    gpu_payload_c[240 +: 8] = shell_fence_status_o;
    gpu_payload_c[248 +: 2] = shell_mode_act_o;
    gpu_payload_c[250 +: 1] = shell_dma_done_o;
    gpu_payload_c[251 +: 8] = shell_dma_status_o;
    gpu_payload_c[259 +: 1] = shell_blit_done_o;
    gpu_payload_c[260 +: 8] = shell_blit_status_o;
    gpu_payload_c[268 +: 640] = shell_pad_frame_flat_o;
    gpu_payload_c[908 +: 16] = shell_pad_sequence_o[0];
    gpu_payload_c[924 +: 16] = shell_pad_sequence_o[1];
    gpu_payload_c[940 +: 16] = shell_pad_sequence_o[2];
    gpu_payload_c[956 +: 16] = shell_pad_sequence_o[3];
    gpu_payload_c[972 +: 64] = shell_input_gaps_o;
    gpu_payload_c[1036 +: 8] = shell_rumble_duty_o[0];
    gpu_payload_c[1044 +: 8] = shell_rumble_duty_o[1];
    gpu_payload_c[1052 +: 8] = shell_rumble_duty_o[2];
    gpu_payload_c[1060 +: 8] = shell_rumble_duty_o[3];
    gpu_payload_c[1068 +: 4] = shell_rumble_active_o;
    gpu_payload_c[1072 +: 4] = shell_rumble_pwm_o;
    gpu_payload_c[1076 +: 64] = shell_rumble_drops_o;
    gpu_payload_c[1140 +: 1] = shell_cnt_snap_valid_o;
    gpu_payload_c[1141 +: 16] = shell_cnt_snap_id_o;
    gpu_payload_c[1157 +: 64] = shell_cnt_snap_value_o;
    gpu_payload_c[1221 +: 1] = shell_cnt_window_open_o;
    gpu_payload_c[1222 +: 1] = shell_cnt_cat_violation_o;
    gpu_payload_c[1223 +: 32] = shell_guard_violations_o;
    gpu_payload_c[1255 +: 64] = shell_starvation_o;
    gpu_payload_c[1319 +: 1] = shell_init_done_o;
    gpu_payload_c[1320 +: 32] = shell_refresh_stalls_o;
    gpu_payload_c[1352 +: 32] = shell_bank_conflicts_o;
    gpu_payload_c[1384 +: 32] = shell_scanout_preempted_o;
    gpu_payload_c[1416 +: 32] = shell_hps_err_count_o;
    gpu_payload_c[1448 +: 1] = shell_shell_err_wfifo_o;
    gpu_payload_c[1449 +: 1] = shell_shell_err_route_o;
    gpu_payload_c[1450 +: 1] = shell_shell_err_cdc_o;
    gpu_payload_c[1451 +: 1] = shell_shell_err_framer_o;
    gpu_payload_c[1452 +: 1] = shell_render_tri_ready_o;
    gpu_payload_c[1453 +: 3] = shell_geom_guard_rsp_o;
    gpu_payload_c[1456 +: 1] = shell_geom_beat_valid_o;
    gpu_payload_c[1457 +: 64] = shell_geom_beat_data_o;
    gpu_payload_c[1521 +: 1] = shell_geom_beat_last_o;
    gpu_payload_c[1522 +: 1] = shell_render_drain_done_o;
    gpu_payload_c[1523 +: 1] = shell_render_busy_o;
    gpu_payload_c[1524 +: 32] = shell_render_pixels_o;
    gpu_payload_c[1556 +: 32] = shell_render_bursts_o;
    gpu_payload_c[1588 +: 1] = shell_render_stream_error_o;
    gpu_payload_c[1589 +: 1] = shell_render_drained_o;
    gpu_payload_c[1590 +: 1] = shell_render_fatal_o;
    gpu_payload_c[1591 +: 32] = shell_render_issued_words_o;
    gpu_payload_c[1623 +: 32] = shell_render_retired_words_o;
    gpu_payload_c[1655 +: 1] = shell_render_overflow_o;
    gpu_payload_c[1656 +: 1] = shell_render_fragment_error_o;
    gpu_payload_c[1657 +: 1] = shell_phy_cs_n_o;
    gpu_payload_c[1658 +: 1] = shell_phy_ras_n_o;
    gpu_payload_c[1659 +: 1] = shell_phy_cas_n_o;
    gpu_payload_c[1660 +: 1] = shell_phy_we_n_o;
    gpu_payload_c[1661 +: 13] = shell_phy_a_o;
    gpu_payload_c[1674 +: 2] = shell_phy_ba_o;
    gpu_payload_c[1676 +: 16] = shell_phy_dq_o;
    gpu_payload_c[1692 +: 1] = shell_phy_dq_oe_o;
    gpu_payload_c[1693 +: 2] = shell_phy_dqm_o;
  end

  logic [167:0] video_payload_c;
  always_comb begin
    video_payload_c = '0;
    video_payload_c[0 +: 1] = shell_px_valid_o;
    video_payload_c[1 +: 16] = shell_px_rgb_o;
    video_payload_c[17 +: 10] = shell_px_x_o;
    video_payload_c[27 +: 8] = shell_px_y_o;
    video_payload_c[35 +: 1] = shell_px_hsync_o;
    video_payload_c[36 +: 1] = shell_px_vsync_o;
    video_payload_c[37 +: 1] = shell_px_hblank_o;
    video_payload_c[38 +: 1] = shell_px_vblank_o;
    video_payload_c[39 +: 1] = shell_scaler_violation_o;
    video_payload_c[40 +: 64] = shell_deadline_faults_o;
    video_payload_c[104 +: 64] = shell_frame_cycles_o;
  end

  logic [65:0] audio_payload_c;
  always_comb begin
    audio_payload_c = '0;
    audio_payload_c[0 +: 1] = shell_pcm_valid_o;
    audio_payload_c[1 +: 16] = shell_pcm_l_o;
    audio_payload_c[17 +: 16] = shell_pcm_r_o;
    audio_payload_c[33 +: 1] = shell_underrun_status_o;
    audio_payload_c[34 +: 32] = shell_audio_underruns_o;
  end

  zhao_shell_fit_stimulus u_stimulus (
    .gpu_clk(gpu_clk),
    .rst_n(rst_n),
    .ring_wr_valid_o_captured_i(gpu_capture_bus[0 +: 1]),
    .ring_wr_slot_o_captured_i(gpu_capture_bus[1 +: 2]),
    .ring_wr_state_o_captured_i(gpu_capture_bus[3 +: 2]),
    .hps_req_valid_o_captured_i(gpu_capture_bus[5 +: 1]),
    .hps_req_write_o_captured_i(gpu_capture_bus[6 +: 1]),
    .hps_req_addr_o_captured_i(gpu_capture_bus[7 +: 32]),
    .hps_req_len_o_captured_i(gpu_capture_bus[39 +: 7]),
    .hps_wr_valid_o_captured_i(gpu_capture_bus[46 +: 1]),
    .hps_wr_last_o_captured_i(gpu_capture_bus[111 +: 1]),
    .blit_done_o_captured_i(gpu_capture_bus[259 +: 1]),
    .blit_status_o_captured_i(gpu_capture_bus[260 +: 8]),
    .aud_wr_ready_o_captured_i(gpu_capture_bus[112 +: 1]),
    .cnt_snap_valid_o_captured_i(gpu_capture_bus[1140 +: 1]),
    .cnt_snap_id_o_captured_i(gpu_capture_bus[1141 +: 16]),
    .init_done_o_captured_i(gpu_capture_bus[1319 +: 1]),
    .render_tri_ready_o_native_i(shell_render_tri_ready_o),
    .render_drained_o_captured_i(gpu_capture_bus[1589 +: 1]),
    .render_issued_words_o_captured_i(gpu_capture_bus[1591 +: 32]),
    .render_retired_words_o_captured_i(gpu_capture_bus[1623 +: 32]),
    .geom_guard_rsp_o_captured_i(zhao_guard_rsp_t'(gpu_capture_bus[1453 +: 3])),
    .geom_beat_valid_o_captured_i(gpu_capture_bus[1456 +: 1]),
    .geom_beat_last_o_captured_i(gpu_capture_bus[1521 +: 1]),
    .phy_cs_n_o_captured_i(gpu_capture_bus[1657 +: 1]),
    .phy_ras_n_o_captured_i(gpu_capture_bus[1658 +: 1]),
    .phy_cas_n_o_captured_i(gpu_capture_bus[1659 +: 1]),
    .phy_we_n_o_captured_i(gpu_capture_bus[1660 +: 1]),
    .phy_a_o_captured_i(gpu_capture_bus[1661 +: 13]),
    .phy_ba_o_captured_i(gpu_capture_bus[1674 +: 2]),
    .phy_dq_o_captured_i(gpu_capture_bus[1676 +: 16]),
    .phy_dq_oe_o_captured_i(gpu_capture_bus[1692 +: 1]),
    .phy_dqm_o_captured_i(gpu_capture_bus[1693 +: 2]),
    .hps_state_i(shell_hps_state_i),
    .hps_byte_len_i(shell_hps_byte_len_i),
    .ring_wr_ready_i(shell_ring_wr_ready_i),
    .hps_req_grant_i(shell_hps_req_grant_i),
    .hps_rd_valid_i(shell_hps_rd_valid_i),
    .hps_rd_data_i(shell_hps_rd_data_i),
    .hps_rd_last_i(shell_hps_rd_last_i),
    .pad_present_i(shell_pad_present_i),
    .pad_buttons_i(shell_pad_buttons_i),
    .pad_lx_i(shell_pad_lx_i),
    .pad_ly_i(shell_pad_ly_i),
    .pad_rx_i(shell_pad_rx_i),
    .pad_ry_i(shell_pad_ry_i),
    .aud_wr_valid_i(shell_aud_wr_valid_i),
    .aud_wr_l_i(shell_aud_wr_l_i),
    .aud_wr_r_i(shell_aud_wr_r_i),
    .cnt_snap_ready_i(shell_cnt_snap_ready_i),
    .render_frame_begin_i(shell_render_frame_begin_i),
    .render_frame_end_i(shell_render_frame_end_i),
    .render_grid_w_i(shell_render_grid_w_i),
    .render_grid_h_i(shell_render_grid_h_i),
    .render_tri_valid_i(shell_render_tri_valid_i),
    .geom_guard_req_i(shell_geom_guard_req_i),
    .render_kx0_i(shell_render_kx0_i),
    .render_ky0_i(shell_render_ky0_i),
    .render_kc0_i(shell_render_kc0_i),
    .render_kx1_i(shell_render_kx1_i),
    .render_ky1_i(shell_render_ky1_i),
    .render_kc1_i(shell_render_kc1_i),
    .render_kx2_i(shell_render_kx2_i),
    .render_ky2_i(shell_render_ky2_i),
    .render_kc2_i(shell_render_kc2_i),
    .render_tl_i(shell_render_tl_i),
    .render_ax_i(shell_render_ax_i),
    .render_ay_i(shell_render_ay_i),
    .render_bx_i(shell_render_bx_i),
    .render_by_i(shell_render_by_i),
    .render_cx_i(shell_render_cx_i),
    .render_cy_i(shell_render_cy_i),
    .render_min_x_i(shell_render_min_x_i),
    .render_max_x_i(shell_render_max_x_i),
    .render_min_y_i(shell_render_min_y_i),
    .render_max_y_i(shell_render_max_y_i),
    .render_src_id_i(shell_render_src_id_i),
    .render_fill_word_i(shell_render_fill_word_i),
    .render_clear_word_i(shell_render_clear_word_i),
    .render_state_i(shell_render_state_i),
    .render_src_a_i(shell_render_src_a_i),
    .render_texel_rgb_i(shell_render_texel_rgb_i),
    .render_texel_a_i(shell_render_texel_a_i),
    .render_texel_idx_i(shell_render_texel_idx_i),
    .render_fb_base_i(shell_render_fb_base_i),
    .render_fb_stride_i(shell_render_fb_stride_i),
    .fb_writer_i(shell_fb_writer_i),
    .phy_dq_i(shell_phy_dq_i)
  );

  zhao_shell_top u_shell (
    .gpu_clk(gpu_clk),
    .vid_clk(vid_clk),
    .audio_clk(audio_clk),
    .rst_n(rst_n),
    .hps_state_i(shell_hps_state_i),
    .hps_byte_len_i(shell_hps_byte_len_i),
    .ring_wr_valid_o(shell_ring_wr_valid_o),
    .ring_wr_slot_o(shell_ring_wr_slot_o),
    .ring_wr_state_o(shell_ring_wr_state_o),
    .ring_wr_ready_i(shell_ring_wr_ready_i),
    .hps_req_valid_o(shell_hps_req_valid_o),
    .hps_req_write_o(shell_hps_req_write_o),
    .hps_req_addr_o(shell_hps_req_addr_o),
    .hps_req_len_o(shell_hps_req_len_o),
    .hps_req_grant_i(shell_hps_req_grant_i),
    .hps_wr_valid_o(shell_hps_wr_valid_o),
    .hps_wr_data_o(shell_hps_wr_data_o),
    .hps_wr_last_o(shell_hps_wr_last_o),
    .hps_rd_valid_i(shell_hps_rd_valid_i),
    .hps_rd_data_i(shell_hps_rd_data_i),
    .hps_rd_last_i(shell_hps_rd_last_i),
    .pad_present_i(shell_pad_present_i),
    .pad_buttons_i(shell_pad_buttons_i),
    .pad_lx_i(shell_pad_lx_i),
    .pad_ly_i(shell_pad_ly_i),
    .pad_rx_i(shell_pad_rx_i),
    .pad_ry_i(shell_pad_ry_i),
    .aud_wr_valid_i(shell_aud_wr_valid_i),
    .aud_wr_l_i(shell_aud_wr_l_i),
    .aud_wr_r_i(shell_aud_wr_r_i),
    .aud_wr_ready_o(shell_aud_wr_ready_o),
    .aud_refill_req_o(shell_aud_refill_req_o),
    .aud_occupancy_o(shell_aud_occupancy_o),
    .pcm_valid_o(shell_pcm_valid_o),
    .pcm_l_o(shell_pcm_l_o),
    .pcm_r_o(shell_pcm_r_o),
    .underrun_status_o(shell_underrun_status_o),
    .audio_underruns_o(shell_audio_underruns_o),
    .px_valid_o(shell_px_valid_o),
    .px_rgb_o(shell_px_rgb_o),
    .px_x_o(shell_px_x_o),
    .px_y_o(shell_px_y_o),
    .px_hsync_o(shell_px_hsync_o),
    .px_vsync_o(shell_px_vsync_o),
    .px_hblank_o(shell_px_hblank_o),
    .px_vblank_o(shell_px_vblank_o),
    .scaler_violation_o(shell_scaler_violation_o),
    .crc_frame_o(shell_crc_frame_o),
    .crc_valid_o(shell_crc_valid_o),
    .crc_bytes_o(shell_crc_bytes_o),
    .crc_size_err_o(shell_crc_size_err_o),
    .gpu_tick_o(shell_gpu_tick_o),
    .gpu_tick_frame_id_o(shell_gpu_tick_frame_id_o),
    .gpu_tick_repeated_o(shell_gpu_tick_repeated_o),
    .gpu_complete_slot_o(shell_gpu_complete_slot_o),
    .deadline_faults_o(shell_deadline_faults_o),
    .frame_cycles_o(shell_frame_cycles_o),
    .slot_state_o(shell_slot_state_o),
    .fence_valid_o(shell_fence_valid_o),
    .fence_slot_o(shell_fence_slot_o),
    .fence_ok_o(shell_fence_ok_o),
    .fence_status_o(shell_fence_status_o),
    .mode_act_o(shell_mode_act_o),
    .dma_done_o(shell_dma_done_o),
    .dma_status_o(shell_dma_status_o),
    .blit_done_o(shell_blit_done_o),
    .blit_status_o(shell_blit_status_o),
    .pad_frame_flat_o(shell_pad_frame_flat_o),
    .pad_sequence_o(shell_pad_sequence_o),
    .input_gaps_o(shell_input_gaps_o),
    .rumble_duty_o(shell_rumble_duty_o),
    .rumble_active_o(shell_rumble_active_o),
    .rumble_pwm_o(shell_rumble_pwm_o),
    .rumble_drops_o(shell_rumble_drops_o),
    .cnt_snap_ready_i(shell_cnt_snap_ready_i),
    .cnt_snap_valid_o(shell_cnt_snap_valid_o),
    .cnt_snap_id_o(shell_cnt_snap_id_o),
    .cnt_snap_value_o(shell_cnt_snap_value_o),
    .cnt_window_open_o(shell_cnt_window_open_o),
    .cnt_cat_violation_o(shell_cnt_cat_violation_o),
    .guard_violations_o(shell_guard_violations_o),
    .starvation_o(shell_starvation_o),
    .init_done_o(shell_init_done_o),
    .refresh_stalls_o(shell_refresh_stalls_o),
    .bank_conflicts_o(shell_bank_conflicts_o),
    .scanout_preempted_o(shell_scanout_preempted_o),
    .hps_err_count_o(shell_hps_err_count_o),
    .shell_err_wfifo_o(shell_shell_err_wfifo_o),
    .shell_err_route_o(shell_shell_err_route_o),
    .shell_err_cdc_o(shell_shell_err_cdc_o),
    .shell_err_framer_o(shell_shell_err_framer_o),
    .render_frame_begin_i(shell_render_frame_begin_i),
    .render_frame_end_i(shell_render_frame_end_i),
    .render_grid_w_i(shell_render_grid_w_i),
    .render_grid_h_i(shell_render_grid_h_i),
    .render_tri_valid_i(shell_render_tri_valid_i),
    .render_tri_ready_o(shell_render_tri_ready_o),
    .geom_guard_req_i(shell_geom_guard_req_i),
    .geom_guard_rsp_o(shell_geom_guard_rsp_o),
    .geom_beat_valid_o(shell_geom_beat_valid_o),
    .geom_beat_data_o(shell_geom_beat_data_o),
    .geom_beat_last_o(shell_geom_beat_last_o),
    .render_kx0_i(shell_render_kx0_i),
    .render_ky0_i(shell_render_ky0_i),
    .render_kc0_i(shell_render_kc0_i),
    .render_kx1_i(shell_render_kx1_i),
    .render_ky1_i(shell_render_ky1_i),
    .render_kc1_i(shell_render_kc1_i),
    .render_kx2_i(shell_render_kx2_i),
    .render_ky2_i(shell_render_ky2_i),
    .render_kc2_i(shell_render_kc2_i),
    .render_tl_i(shell_render_tl_i),
    .render_ax_i(shell_render_ax_i),
    .render_ay_i(shell_render_ay_i),
    .render_bx_i(shell_render_bx_i),
    .render_by_i(shell_render_by_i),
    .render_cx_i(shell_render_cx_i),
    .render_cy_i(shell_render_cy_i),
    .render_min_x_i(shell_render_min_x_i),
    .render_max_x_i(shell_render_max_x_i),
    .render_min_y_i(shell_render_min_y_i),
    .render_max_y_i(shell_render_max_y_i),
    .render_src_id_i(shell_render_src_id_i),
    .render_fill_word_i(shell_render_fill_word_i),
    .render_clear_word_i(shell_render_clear_word_i),
    .render_state_i(shell_render_state_i),
    .render_src_a_i(shell_render_src_a_i),
    .render_texel_rgb_i(shell_render_texel_rgb_i),
    .render_texel_a_i(shell_render_texel_a_i),
    .render_texel_idx_i(shell_render_texel_idx_i),
    .render_fb_base_i(shell_render_fb_base_i),
    .render_fb_stride_i(shell_render_fb_stride_i),
    .fb_writer_i(shell_fb_writer_i),
    .render_drain_done_o(shell_render_drain_done_o),
    .render_busy_o(shell_render_busy_o),
    .render_pixels_o(shell_render_pixels_o),
    .render_bursts_o(shell_render_bursts_o),
    .render_stream_error_o(shell_render_stream_error_o),
    .render_drained_o(shell_render_drained_o),
    .render_fatal_o(shell_render_fatal_o),
    .render_issued_words_o(shell_render_issued_words_o),
    .render_retired_words_o(shell_render_retired_words_o),
    .render_overflow_o(shell_render_overflow_o),
    .render_fragment_error_o(shell_render_fragment_error_o),
    .phy_cs_n_o(shell_phy_cs_n_o),
    .phy_ras_n_o(shell_phy_ras_n_o),
    .phy_cas_n_o(shell_phy_cas_n_o),
    .phy_we_n_o(shell_phy_we_n_o),
    .phy_a_o(shell_phy_a_o),
    .phy_ba_o(shell_phy_ba_o),
    .phy_dq_o(shell_phy_dq_o),
    .phy_dq_oe_o(shell_phy_dq_oe_o),
    .phy_dqm_o(shell_phy_dqm_o),
    .phy_dq_i(shell_phy_dq_i)
  );

  zhao_shell_fit_gpu_sink u_gpu_sink (
    .clk(gpu_clk),
    .rst_n(rst_n),
    .payload_i(gpu_payload_c),
    .capture_o(gpu_capture_bus),
    .signature_o(fit_signature_o[0]),
    .epoch_o(fit_epoch_o[0])
  );

  zhao_shell_fit_video_sink u_video_sink (
    .clk(vid_clk),
    .rst_n(rst_n),
    .payload_i(video_payload_c),
    .signature_o(fit_signature_o[1]),
    .epoch_o(fit_epoch_o[1])
  );

  zhao_shell_fit_audio_sink u_audio_sink (
    .clk(audio_clk),
    .rst_n(rst_n),
    .payload_i(audio_payload_c),
    .signature_o(fit_signature_o[2]),
    .epoch_o(fit_epoch_o[2])
  );

endmodule

/* verilator lint_off DECLFILENAME */
module zhao_shell_fit_stimulus
  import zhao_pkg::*, zhao_abi_pkg::*;
(
  input logic gpu_clk,
  input logic rst_n,
  input var logic ring_wr_valid_o_captured_i,
  input var logic [1:0] ring_wr_slot_o_captured_i,
  input var logic [1:0] ring_wr_state_o_captured_i,
  input var logic hps_req_valid_o_captured_i,
  input var logic hps_req_write_o_captured_i,
  input var logic [31:0] hps_req_addr_o_captured_i,
  input var logic [6:0] hps_req_len_o_captured_i,
  input var logic hps_wr_valid_o_captured_i,
  input var logic hps_wr_last_o_captured_i,
  input var logic blit_done_o_captured_i,
  input var logic [7:0] blit_status_o_captured_i,
  input var logic aud_wr_ready_o_captured_i,
  input var logic cnt_snap_valid_o_captured_i,
  input var logic [15:0] cnt_snap_id_o_captured_i,
  input var logic init_done_o_captured_i,
  input var logic render_tri_ready_o_native_i,
  input var logic render_drained_o_captured_i,
  input var logic [31:0] render_issued_words_o_captured_i,
  input var logic [31:0] render_retired_words_o_captured_i,
  input var zhao_guard_rsp_t geom_guard_rsp_o_captured_i,
  input var logic geom_beat_valid_o_captured_i,
  input var logic geom_beat_last_o_captured_i,
  input var logic phy_cs_n_o_captured_i,
  input var logic phy_ras_n_o_captured_i,
  input var logic phy_cas_n_o_captured_i,
  input var logic phy_we_n_o_captured_i,
  input var logic [12:0] phy_a_o_captured_i,
  input var logic [1:0] phy_ba_o_captured_i,
  input var logic [15:0] phy_dq_o_captured_i,
  input var logic phy_dq_oe_o_captured_i,
  input var logic [1:0] phy_dqm_o_captured_i,
  (* preserve *) output var logic [1:0] hps_state_i [0:2],
  (* preserve *) output var logic [31:0] hps_byte_len_i [0:2],
  (* preserve *) output var logic ring_wr_ready_i,
  (* preserve *) output var logic hps_req_grant_i,
  (* preserve *) output var logic hps_rd_valid_i,
  (* preserve *) output var logic [63:0] hps_rd_data_i,
  (* preserve *) output var logic hps_rd_last_i,
  (* preserve *) output var logic [3:0] pad_present_i,
  (* preserve *) output var logic [31:0] pad_buttons_i [0:3],
  (* preserve *) output var logic [15:0] pad_lx_i [0:3],
  (* preserve *) output var logic [15:0] pad_ly_i [0:3],
  (* preserve *) output var logic [15:0] pad_rx_i [0:3],
  (* preserve *) output var logic [15:0] pad_ry_i [0:3],
  (* preserve *) output var logic aud_wr_valid_i,
  (* preserve *) output var logic [15:0] aud_wr_l_i,
  (* preserve *) output var logic [15:0] aud_wr_r_i,
  (* preserve *) output var logic cnt_snap_ready_i,
  (* preserve *) output var logic render_frame_begin_i,
  (* preserve *) output var logic render_frame_end_i,
  (* preserve *) output var logic [5:0] render_grid_w_i,
  (* preserve *) output var logic [5:0] render_grid_h_i,
  (* preserve *) output var logic render_tri_valid_i,
  (* preserve *) output var zhao_guard_req_t geom_guard_req_i,
  (* preserve *) output var logic signed [22:0] render_kx0_i,
  (* preserve *) output var logic signed [22:0] render_ky0_i,
  (* preserve *) output var logic signed [47:0] render_kc0_i,
  (* preserve *) output var logic signed [22:0] render_kx1_i,
  (* preserve *) output var logic signed [22:0] render_ky1_i,
  (* preserve *) output var logic signed [47:0] render_kc1_i,
  (* preserve *) output var logic signed [22:0] render_kx2_i,
  (* preserve *) output var logic signed [22:0] render_ky2_i,
  (* preserve *) output var logic signed [47:0] render_kc2_i,
  (* preserve *) output var logic [2:0] render_tl_i,
  (* preserve *) output var logic signed [20:0] render_ax_i,
  (* preserve *) output var logic signed [20:0] render_ay_i,
  (* preserve *) output var logic signed [20:0] render_bx_i,
  (* preserve *) output var logic signed [20:0] render_by_i,
  (* preserve *) output var logic signed [20:0] render_cx_i,
  (* preserve *) output var logic signed [20:0] render_cy_i,
  (* preserve *) output var logic signed [11:0] render_min_x_i,
  (* preserve *) output var logic signed [11:0] render_max_x_i,
  (* preserve *) output var logic signed [11:0] render_min_y_i,
  (* preserve *) output var logic signed [11:0] render_max_y_i,
  (* preserve *) output var logic [15:0] render_src_id_i,
  (* preserve *) output var logic [63:0] render_fill_word_i,
  (* preserve *) output var logic [63:0] render_clear_word_i,
  (* preserve *) output var logic [31:0] render_state_i,
  (* preserve *) output var logic [7:0] render_src_a_i,
  (* preserve *) output var logic [23:0] render_texel_rgb_i,
  (* preserve *) output var logic [7:0] render_texel_a_i,
  (* preserve *) output var logic [7:0] render_texel_idx_i,
  (* preserve *) output var logic [26:0] render_fb_base_i,
  (* preserve *) output var logic [15:0] render_fb_stride_i,
  (* preserve *) output var logic fb_writer_i,
  (* preserve *) output var logic [15:0] phy_dq_i
);
  localparam logic [2:0] HPS_IDLE = 3'd0;
  localparam logic [2:0] HPS_GRANT_WAIT = 3'd1;
  localparam logic [2:0] HPS_READ_LATENCY = 3'd2;
  localparam logic [2:0] HPS_READ_BEATS = 3'd3;
  localparam logic [2:0] HPS_WRITE_DATA = 3'd4;
  localparam logic [2:0] RENDER_WAIT_INIT = 3'd0;
  localparam logic [2:0] RENDER_BEGIN = 3'd1;
  localparam logic [2:0] RENDER_OFFER = 3'd2;
  localparam logic [2:0] RENDER_END = 3'd3;
  localparam logic [2:0] RENDER_DRAIN = 3'd4;

  logic [9:0] release_q;
  logic run_c;
  (* preserve *) logic [63:0] lfsr_q;
  logic [63:0] cycle_q;
  logic [2047:0] entropy_c;
  logic [2:0] hps_state_q;
  logic [2:0] render_state_q;
  logic [31:0] hps_addr_q;
  logic [6:0] hps_len_q;
  logic hps_write_q;
  logic [6:0] hps_wait_q;
  logic [6:0] hps_beat_q;
  logic [7:0] hps_first_beat_age_q;
  logic hps_first_beat_pending_q;
  logic hps_timing_fault_hold_q;
  logic blit_source_read_q;
  logic render_enable_q;
  logic ring_ready_capture_q;
  logic [7:0] ring_delay_q [0:2];
  logic [7:0] audio_pause_q;
  logic [1:0] pad_phase_q;
  logic cnt_ready_capture_q;
  logic [15:0] counter_next_id_q;
  logic [6:0] counter_entries_q;
  logic render_width_cover_q;
  logic render_job_entropy_q;
  logic render_profile_done_q;
  logic render_offer_stalled_q;
  logic render_offer_mutated_q;
  logic [738:0] render_offer_payload_c;
  logic [738:0] render_offer_snapshot_q;
  logic [738:0] render_entropy_offer_snapshot_q /* verilator public_flat_rd */;
  logic [738:0] render_accepted_payload_q /* verilator public_flat_rd */;
  logic [738:0] render_entropy_accepted_payload_q /* verilator public_flat_rd */;
  logic [281:0] render_triangle_coefficients_c;
  logic [125:0] render_triangle_vertices_c;
  logic [47:0] render_triangle_bounds_c;
  logic [18:0] render_triangle_identity_c;
  logic [281:0] render_directed_accepted_coefficients_q /* verilator public_flat_rd */;
  logic [125:0] render_directed_accepted_vertices_q /* verilator public_flat_rd */;
  logic [47:0] render_directed_accepted_bounds_q /* verilator public_flat_rd */;
  logic [18:0] render_directed_accepted_identity_q /* verilator public_flat_rd */;
  logic [281:0] render_entropy_accepted_coefficients_q /* verilator public_flat_rd */;
  logic [125:0] render_entropy_accepted_vertices_q /* verilator public_flat_rd */;
  logic [47:0] render_entropy_accepted_bounds_q /* verilator public_flat_rd */;
  logic [18:0] render_entropy_accepted_identity_q /* verilator public_flat_rd */;
  logic [15:0] render_wait_q;
  logic [31:0] render_issued_start_q;
  logic [31:0] render_retired_start_q;
  logic render_work_seen_q;
  logic render_retirement_seen_q;
  logic guard_negative_q;
  logic guard_response_pending_q;
  logic [15:0] guard_verdict_wait_q;
  logic guard_beat_pending_q;
  logic [3:0] guard_beat_count_q;
  logic guard_frame_owned_q;
  logic guard_denial_watch_q;
  logic guard_preownership_watch_q;
  logic [3:0] guard_extra_fault_delay_q;
  logic [3:0] guard_verdict_extra_fault_delay_q;
  logic [3:0] guard_denial_extra_fault_delay_q;
  logic [15:0] guard_beat_wait_q;
  logic [7:0] guard_gap_q;
  logic [3:0] sdr_read_wait_q;
  logic [3:0] sdr_read_beat_q;
  logic [28:0] sdr_row_q;
  logic sdr_write_pending_q;
  logic sdr_write_active_q;
  logic [14:0] sdr_write_command_q;
  // Safe synthesis default; the generated smoke monitor alone overrides this
  // internal simulation hook hierarchically to fire each protocol detector.
  /* verilator lint_off MULTIDRIVEN */
  logic [3:0] protocol_fault_i = 4'd0;
  /* verilator lint_on MULTIDRIVEN */
  logic [1:0] ring_state_checked_c;
  logic hps_req_write_checked_c;
  logic [15:0] counter_id_checked_c;
  logic guard_verdict_ok_checked_c;
  logic guard_verdict_violation_checked_c;
  logic guard_beat_valid_checked_c;
  logic guard_beat_last_checked_c;
  logic sdr_dq_oe_checked_c;

  // Positive progress witnesses used only by the deterministic smoke test.
  logic [31:0] ring_transactions_q;
  logic [31:0] ring_done_posts_q;
  logic [31:0] ring_done_frees_q;
  logic [31:0] ring_done_dwell_q;
  logic [31:0] ring_sequence_errors_q;
  logic [31:0] hps_read_completions_q;
  logic [31:0] hps_write_watchdog_faults_q;
  logic [31:0] hps_first_beat_timing_witnesses_q;
  logic [31:0] hps_first_beat_timing_errors_q;
  logic [31:0] blit_successes_q;
  logic [31:0] blit_failures_q;
  logic [3:0] pad_directed_phases_q /* verilator public_flat_rd */;
  logic [31:0] audio_accepts_q;
  logic [31:0] counter_accepts_q;
  logic [31:0] counter_windows_q;
  logic [31:0] counter_sequence_errors_q;
  logic [39:0] counter_selector_seen_q /* verilator public_flat_rd */;
  logic [31:0] render_accepts_q;
  logic [31:0] render_directed_accepts_q;
  logic [31:0] render_entropy_accepts_q;
  logic [31:0] render_entropy_completions_q;
  logic [31:0] render_backpressure_witnesses_q;
  logic [31:0] render_entropy_backpressure_witnesses_q;
  logic [31:0] render_backpressure_stability_errors_q;
  logic [31:0] render_accept_class_errors_q;
  logic [31:0] render_entropy_accepted_mode_q /* verilator public_flat_rd */;
  logic [63:0] render_entropy_accepted_fill_q /* verilator public_flat_rd */;
  logic [63:0] render_entropy_accepted_clear_q /* verilator public_flat_rd */;
  logic [31:0] render_drains_q;
  logic [31:0] render_timeouts_q;
  logic [31:0] guard_accepts_q;
  logic [31:0] guard_rejects_q;
  logic [31:0] guard_last_beats_q;
  logic [31:0] guard_exact_frames_q;
  logic [31:0] guard_beat_timeouts_q;
  logic [31:0] guard_verdict_timeouts_q;
  logic [31:0] guard_early_last_errors_q;
  logic [31:0] guard_late_last_errors_q;
  logic [31:0] guard_extra_beat_errors_q;
  logic [31:0] guard_verdict_extra_witnesses_q /* verilator public_flat_rd */;
  logic [31:0] guard_post_denial_extra_witnesses_q /* verilator public_flat_rd */;
  logic [2:0] guard_preownership_fault_windows_q /* verilator public_flat_rd */;
  logic [31:0] sdr_read_responses_q;
  logic [31:0] sdr_write_transactions_q;
  logic [31:0] sdr_write_observations_q;
  logic [31:0] sdr_write_phase_errors_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) release_q <= '0;
    else release_q <= {release_q[8:0], 1'b1};
  end
  assign run_c = release_q[9]; // 2 sync edges + 8 warm-up edges
  assign entropy_c = {32{lfsr_q ^ cycle_q}};
  // Fault controls perturb observed protocol operands; the unchanged detector
  // arms below must then report the deliberately malformed transaction.
  assign ring_state_checked_c = (protocol_fault_i == 4'd1)
                                  ? 2'd2 : ring_wr_state_o_captured_i;
  assign hps_req_write_checked_c = hps_req_write_o_captured_i ||
                                     (protocol_fault_i == 4'd2);
  assign counter_id_checked_c = cnt_snap_id_o_captured_i ^
                                ((protocol_fault_i == 4'd3) ? 16'd1 : 16'd0);
  assign guard_verdict_ok_checked_c = geom_guard_rsp_o_captured_i.ok &&
                                         (protocol_fault_i != 4'd7);
  assign guard_verdict_violation_checked_c = geom_guard_rsp_o_captured_i.violation &&
                                                (protocol_fault_i != 4'd7);
  assign guard_beat_valid_checked_c =
      (protocol_fault_i == 4'd5) ? 1'b0 :
      (geom_beat_valid_o_captured_i ||
       ((protocol_fault_i == 4'd10) && guard_frame_owned_q &&
        !guard_beat_pending_q && (guard_extra_fault_delay_q == 4'd1)) ||
       ((protocol_fault_i == 4'd13) && guard_frame_owned_q &&
        guard_response_pending_q &&
        (guard_verdict_extra_fault_delay_q == 4'd1)) ||
       ((protocol_fault_i == 4'd14) && guard_denial_watch_q &&
        !guard_response_pending_q && !guard_beat_pending_q &&
        (guard_denial_extra_fault_delay_q == 4'd1)) ||
       ((protocol_fault_i == 4'd15) && guard_preownership_watch_q &&
        !guard_beat_pending_q &&
        ((!guard_response_pending_q && !geom_guard_req_i.valid &&
          !guard_preownership_fault_windows_q[0]) ||
         (!guard_response_pending_q && geom_guard_req_i.valid &&
          !guard_preownership_fault_windows_q[1]) ||
         (guard_response_pending_q &&
          !guard_verdict_ok_checked_c &&
          !guard_verdict_violation_checked_c &&
          !guard_preownership_fault_windows_q[2]))));
  assign guard_beat_last_checked_c =
      ((protocol_fault_i == 4'd8) && geom_beat_valid_o_captured_i &&
       (guard_beat_count_q == 4'd0)) ? 1'b1 :
      ((protocol_fault_i == 4'd9) && geom_beat_valid_o_captured_i &&
       (guard_beat_count_q == 4'd7)) ? 1'b0 :
      geom_beat_last_o_captured_i;
  assign sdr_dq_oe_checked_c = phy_dq_oe_o_captured_i;
  assign render_offer_payload_c = {render_grid_w_i, render_grid_h_i, render_fill_word_i, render_clear_word_i, render_state_i, render_src_a_i, render_texel_rgb_i, render_texel_a_i, render_texel_idx_i, render_fb_base_i, render_fb_stride_i, fb_writer_i, render_kx0_i, render_ky0_i, render_kc0_i, render_kx1_i, render_ky1_i, render_kc1_i, render_kx2_i, render_ky2_i, render_kc2_i, render_tl_i, render_ax_i, render_ay_i, render_bx_i, render_by_i, render_cx_i, render_cy_i, render_min_x_i, render_max_x_i, render_min_y_i, render_max_y_i, render_src_id_i};
  assign render_triangle_coefficients_c = {render_kx0_i, render_ky0_i, render_kc0_i, render_kx1_i, render_ky1_i, render_kc1_i, render_kx2_i, render_ky2_i, render_kc2_i};
  assign render_triangle_vertices_c = {render_ax_i, render_ay_i, render_bx_i, render_by_i, render_cx_i, render_cy_i};
  assign render_triangle_bounds_c = {render_min_x_i, render_max_x_i, render_min_y_i, render_max_y_i};
  assign render_triangle_identity_c = {render_tl_i, render_src_id_i};

  function automatic logic [7:0] packet_byte(input logic [31:0] address);
    begin
      case (address)
        32'h00001000: packet_byte = 8'h5a;
        32'h00001001: packet_byte = 8'h50;
        32'h00001002: packet_byte = 8'h4b;
        32'h00001003: packet_byte = 8'h31;
        32'h00001004: packet_byte = 8'h03;
        32'h00001005: packet_byte = 8'h00;
        32'h00001006: packet_byte = 8'h01;
        32'h00001007: packet_byte = 8'h00;
        32'h00001008: packet_byte = 8'h01;
        32'h00001009: packet_byte = 8'h00;
        32'h0000100a: packet_byte = 8'h00;
        32'h0000100b: packet_byte = 8'h00;
        32'h0000100c: packet_byte = 8'h00;
        32'h0000100d: packet_byte = 8'h00;
        32'h0000100e: packet_byte = 8'h00;
        32'h0000100f: packet_byte = 8'h00;
        32'h00001010: packet_byte = 8'h00;
        32'h00001011: packet_byte = 8'h00;
        32'h00001012: packet_byte = 8'h00;
        32'h00001013: packet_byte = 8'h00;
        32'h00001014: packet_byte = 8'h00;
        32'h00001015: packet_byte = 8'h00;
        32'h00001016: packet_byte = 8'h00;
        32'h00001017: packet_byte = 8'h00;
        32'h00001018: packet_byte = 8'h04;
        32'h00001019: packet_byte = 8'h00;
        32'h0000101a: packet_byte = 8'h00;
        32'h0000101b: packet_byte = 8'h00;
        32'h0000101c: packet_byte = 8'h80;
        32'h0000101d: packet_byte = 8'h00;
        32'h0000101e: packet_byte = 8'h00;
        32'h0000101f: packet_byte = 8'h00;
        32'h00001020: packet_byte = 8'h3c;
        32'h00001021: packet_byte = 8'h76;
        32'h00001022: packet_byte = 8'hca;
        32'h00001023: packet_byte = 8'h87;
        32'h00001024: packet_byte = 8'h01;
        32'h00001025: packet_byte = 8'h00;
        32'h00001026: packet_byte = 8'h20;
        32'h00001027: packet_byte = 8'h00;
        32'h00001028: packet_byte = 8'h01;
        32'h00001029: packet_byte = 8'h00;
        32'h0000102a: packet_byte = 8'h01;
        32'h0000102b: packet_byte = 8'h50;
        32'h0000102c: packet_byte = 8'h00;
        32'h0000102d: packet_byte = 8'h00;
        32'h0000102e: packet_byte = 8'h00;
        32'h0000102f: packet_byte = 8'h00;
        32'h00001030: packet_byte = 8'h00;
        32'h00001031: packet_byte = 8'h00;
        32'h00001032: packet_byte = 8'h00;
        32'h00001033: packet_byte = 8'h00;
        32'h00001034: packet_byte = 8'h00;
        32'h00001035: packet_byte = 8'h00;
        32'h00001036: packet_byte = 8'h00;
        32'h00001037: packet_byte = 8'h00;
        32'h00001038: packet_byte = 8'h00;
        32'h00001039: packet_byte = 8'h00;
        32'h0000103a: packet_byte = 8'h00;
        32'h0000103b: packet_byte = 8'h00;
        32'h0000103c: packet_byte = 8'h00;
        32'h0000103d: packet_byte = 8'h00;
        32'h0000103e: packet_byte = 8'h00;
        32'h0000103f: packet_byte = 8'h00;
        32'h00001040: packet_byte = 8'h00;
        32'h00001041: packet_byte = 8'h00;
        32'h00001042: packet_byte = 8'h00;
        32'h00001043: packet_byte = 8'h00;
        32'h00001044: packet_byte = 8'h00;
        32'h00001045: packet_byte = 8'h00;
        32'h00001046: packet_byte = 8'h10;
        32'h00001047: packet_byte = 8'h00;
        32'h00001048: packet_byte = 8'h00;
        32'h00001049: packet_byte = 8'h00;
        32'h0000104a: packet_byte = 8'h01;
        32'h0000104b: packet_byte = 8'h50;
        32'h0000104c: packet_byte = 8'h00;
        32'h0000104d: packet_byte = 8'h00;
        32'h0000104e: packet_byte = 8'h00;
        32'h0000104f: packet_byte = 8'h00;
        32'h00001050: packet_byte = 8'h00;
        32'h00001051: packet_byte = 8'h00;
        32'h00001052: packet_byte = 8'h00;
        32'h00001053: packet_byte = 8'h00;
        32'h00001054: packet_byte = 8'h02;
        32'h00001055: packet_byte = 8'hf0;
        32'h00001056: packet_byte = 8'h30;
        32'h00001057: packet_byte = 8'h00;
        32'h00001058: packet_byte = 8'h10;
        32'h00001059: packet_byte = 8'h00;
        32'h0000105a: packet_byte = 8'h01;
        32'h0000105b: packet_byte = 8'h50;
        32'h0000105c: packet_byte = 8'h00;
        32'h0000105d: packet_byte = 8'h00;
        32'h0000105e: packet_byte = 8'h00;
        32'h0000105f: packet_byte = 8'h00;
        32'h00001060: packet_byte = 8'h00;
        32'h00001061: packet_byte = 8'h00;
        32'h00001062: packet_byte = 8'h00;
        32'h00001063: packet_byte = 8'h00;
        32'h00001064: packet_byte = 8'h00;
        32'h00001065: packet_byte = 8'h00;
        32'h00001066: packet_byte = 8'h00;
        32'h00001067: packet_byte = 8'h00;
        32'h00001068: packet_byte = 8'h00;
        32'h00001069: packet_byte = 8'h00;
        32'h0000106a: packet_byte = 8'h20;
        32'h0000106b: packet_byte = 8'h00;
        32'h0000106c: packet_byte = 8'h00;
        32'h0000106d: packet_byte = 8'hd0;
        32'h0000106e: packet_byte = 8'h02;
        32'h0000106f: packet_byte = 8'h00;
        32'h00001070: packet_byte = 8'h73;
        32'h00001071: packet_byte = 8'hb1;
        32'h00001072: packet_byte = 8'h1d;
        32'h00001073: packet_byte = 8'h61;
        32'h00001074: packet_byte = 8'h00;
        32'h00001075: packet_byte = 8'h00;
        32'h00001076: packet_byte = 8'h00;
        32'h00001077: packet_byte = 8'h00;
        32'h00001078: packet_byte = 8'h00;
        32'h00001079: packet_byte = 8'h00;
        32'h0000107a: packet_byte = 8'h00;
        32'h0000107b: packet_byte = 8'h00;
        32'h0000107c: packet_byte = 8'h00;
        32'h0000107d: packet_byte = 8'h00;
        32'h0000107e: packet_byte = 8'h00;
        32'h0000107f: packet_byte = 8'h00;
        32'h00001080: packet_byte = 8'h00;
        32'h00001081: packet_byte = 8'h00;
        32'h00001082: packet_byte = 8'h00;
        32'h00001083: packet_byte = 8'h00;
        32'h00001084: packet_byte = 8'h02;
        32'h00001085: packet_byte = 8'h00;
        32'h00001086: packet_byte = 8'h20;
        32'h00001087: packet_byte = 8'h00;
        32'h00001088: packet_byte = 8'h02;
        32'h00001089: packet_byte = 8'h00;
        32'h0000108a: packet_byte = 8'h01;
        32'h0000108b: packet_byte = 8'h50;
        32'h0000108c: packet_byte = 8'h00;
        32'h0000108d: packet_byte = 8'h00;
        32'h0000108e: packet_byte = 8'h00;
        32'h0000108f: packet_byte = 8'h00;
        32'h00001090: packet_byte = 8'h00;
        32'h00001091: packet_byte = 8'h00;
        32'h00001092: packet_byte = 8'h00;
        32'h00001093: packet_byte = 8'h00;
        32'h00001094: packet_byte = 8'h00;
        32'h00001095: packet_byte = 8'h00;
        32'h00001096: packet_byte = 8'h00;
        32'h00001097: packet_byte = 8'h00;
        32'h00001098: packet_byte = 8'h00;
        32'h00001099: packet_byte = 8'h00;
        32'h0000109a: packet_byte = 8'h00;
        32'h0000109b: packet_byte = 8'h00;
        32'h0000109c: packet_byte = 8'h00;
        32'h0000109d: packet_byte = 8'h00;
        32'h0000109e: packet_byte = 8'h00;
        32'h0000109f: packet_byte = 8'h00;
        32'h000010a0: packet_byte = 8'h00;
        32'h000010a1: packet_byte = 8'h00;
        32'h000010a2: packet_byte = 8'h00;
        32'h000010a3: packet_byte = 8'h00;
        32'h000010a4: packet_byte = 8'h85;
        32'h000010a5: packet_byte = 8'hf9;
        32'h000010a6: packet_byte = 8'h36;
        32'h000010a7: packet_byte = 8'h9e;
        default: packet_byte = address[7:0] ^ address[15:8] ^ address[23:16] ^ address[31:24] ^ 8'h5a;
      endcase
    end
  endfunction

  function automatic logic [63:0] packet_word(
      input logic [31:0] address);
    integer byte_index;
    begin
      for (byte_index = 0; byte_index < 8; byte_index = byte_index + 1)
        packet_word[byte_index*8 +: 8] = packet_byte(address + byte_index);
    end
  endfunction

  integer slot;
  integer pad;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      lfsr_q <= 64'h9e37_79b9_7f4a_7c15;
      cycle_q <= '0;
      hps_state_q <= HPS_IDLE;
      render_state_q <= RENDER_WAIT_INIT;
      hps_addr_q <= '0;
      hps_len_q <= '0;
      hps_write_q <= 1'b0;
      hps_wait_q <= '0;
      hps_beat_q <= '0;
      hps_first_beat_age_q <= '0;
      hps_first_beat_pending_q <= 1'b0;
      hps_timing_fault_hold_q <= 1'b0;
      blit_source_read_q <= 1'b0;
      render_enable_q <= 1'b0;
      ring_ready_capture_q <= 1'b0;
      audio_pause_q <= '0;
      pad_phase_q <= '0;
      cnt_ready_capture_q <= 1'b0;
      counter_next_id_q <= '0;
      counter_entries_q <= '0;
      render_width_cover_q <= 1'b0;
      render_job_entropy_q <= 1'b0;
      render_profile_done_q <= 1'b0;
      render_offer_stalled_q <= 1'b0;
      render_offer_mutated_q <= 1'b0;
      render_offer_snapshot_q <= '0;
      render_entropy_offer_snapshot_q <= '0;
      render_accepted_payload_q <= '0;
      render_entropy_accepted_payload_q <= '0;
      render_directed_accepted_coefficients_q <= '0;
      render_directed_accepted_vertices_q <= '0;
      render_directed_accepted_bounds_q <= '0;
      render_directed_accepted_identity_q <= '0;
      render_entropy_accepted_coefficients_q <= '0;
      render_entropy_accepted_vertices_q <= '0;
      render_entropy_accepted_bounds_q <= '0;
      render_entropy_accepted_identity_q <= '0;
      render_wait_q <= '0;
      render_issued_start_q <= '0;
      render_retired_start_q <= '0;
      render_work_seen_q <= 1'b0;
      render_retirement_seen_q <= 1'b0;
      guard_negative_q <= 1'b0;
      guard_response_pending_q <= 1'b0;
      guard_verdict_wait_q <= '0;
      guard_beat_pending_q <= 1'b0;
      guard_beat_count_q <= '0;
      guard_frame_owned_q <= 1'b0;
      guard_denial_watch_q <= 1'b0;
      guard_preownership_watch_q <= 1'b1;
      guard_extra_fault_delay_q <= '0;
      guard_verdict_extra_fault_delay_q <= '0;
      guard_denial_extra_fault_delay_q <= '0;
      guard_beat_wait_q <= '0;
      guard_gap_q <= '0;
      sdr_read_wait_q <= '0;
      sdr_read_beat_q <= '0;
      sdr_row_q <= '0;
      sdr_write_pending_q <= 1'b0;
      sdr_write_active_q <= 1'b0;
      sdr_write_command_q <= '0;
      ring_transactions_q <= '0;
      ring_done_posts_q <= '0;
      ring_done_frees_q <= '0;
      ring_done_dwell_q <= '0;
      ring_sequence_errors_q <= '0;
      hps_read_completions_q <= '0;
      hps_write_watchdog_faults_q <= '0;
      hps_first_beat_timing_witnesses_q <= '0;
      hps_first_beat_timing_errors_q <= '0;
      blit_successes_q <= '0;
      blit_failures_q <= '0;
      pad_directed_phases_q <= '0;
      audio_accepts_q <= '0;
      counter_accepts_q <= '0;
      counter_windows_q <= '0;
      counter_sequence_errors_q <= '0;
      counter_selector_seen_q <= '0;
      render_accepts_q <= '0;
      render_directed_accepts_q <= '0;
      render_entropy_accepts_q <= '0;
      render_entropy_completions_q <= '0;
      render_backpressure_witnesses_q <= '0;
      render_entropy_backpressure_witnesses_q <= '0;
      render_backpressure_stability_errors_q <= '0;
      render_accept_class_errors_q <= '0;
      render_entropy_accepted_mode_q <= '0;
      render_entropy_accepted_fill_q <= '0;
      render_entropy_accepted_clear_q <= '0;
      render_drains_q <= '0;
      render_timeouts_q <= '0;
      guard_accepts_q <= '0;
      guard_rejects_q <= '0;
      guard_last_beats_q <= '0;
      guard_exact_frames_q <= '0;
      guard_beat_timeouts_q <= '0;
      guard_verdict_timeouts_q <= '0;
      guard_early_last_errors_q <= '0;
      guard_late_last_errors_q <= '0;
      guard_extra_beat_errors_q <= '0;
      guard_verdict_extra_witnesses_q <= '0;
      guard_post_denial_extra_witnesses_q <= '0;
      guard_preownership_fault_windows_q <= '0;
      sdr_read_responses_q <= '0;
      sdr_write_transactions_q <= '0;
      sdr_write_observations_q <= '0;
      sdr_write_phase_errors_q <= '0;
      hps_state_i[0] <= '0;
      hps_state_i[1] <= '0;
      hps_state_i[2] <= '0;
      hps_byte_len_i[0] <= '0;
      hps_byte_len_i[1] <= '0;
      hps_byte_len_i[2] <= '0;
      ring_wr_ready_i <= '0;
      hps_req_grant_i <= '0;
      hps_rd_valid_i <= '0;
      hps_rd_data_i <= '0;
      hps_rd_last_i <= '0;
      pad_present_i <= '0;
      pad_buttons_i[0] <= '0;
      pad_buttons_i[1] <= '0;
      pad_buttons_i[2] <= '0;
      pad_buttons_i[3] <= '0;
      pad_lx_i[0] <= '0;
      pad_lx_i[1] <= '0;
      pad_lx_i[2] <= '0;
      pad_lx_i[3] <= '0;
      pad_ly_i[0] <= '0;
      pad_ly_i[1] <= '0;
      pad_ly_i[2] <= '0;
      pad_ly_i[3] <= '0;
      pad_rx_i[0] <= '0;
      pad_rx_i[1] <= '0;
      pad_rx_i[2] <= '0;
      pad_rx_i[3] <= '0;
      pad_ry_i[0] <= '0;
      pad_ry_i[1] <= '0;
      pad_ry_i[2] <= '0;
      pad_ry_i[3] <= '0;
      aud_wr_valid_i <= '0;
      aud_wr_l_i <= '0;
      aud_wr_r_i <= '0;
      cnt_snap_ready_i <= '0;
      render_frame_begin_i <= '0;
      render_frame_end_i <= '0;
      render_grid_w_i <= '0;
      render_grid_h_i <= '0;
      render_tri_valid_i <= '0;
      geom_guard_req_i <= '0;
      render_kx0_i <= '0;
      render_ky0_i <= '0;
      render_kc0_i <= '0;
      render_kx1_i <= '0;
      render_ky1_i <= '0;
      render_kc1_i <= '0;
      render_kx2_i <= '0;
      render_ky2_i <= '0;
      render_kc2_i <= '0;
      render_tl_i <= '0;
      render_ax_i <= '0;
      render_ay_i <= '0;
      render_bx_i <= '0;
      render_by_i <= '0;
      render_cx_i <= '0;
      render_cy_i <= '0;
      render_min_x_i <= '0;
      render_max_x_i <= '0;
      render_min_y_i <= '0;
      render_max_y_i <= '0;
      render_src_id_i <= '0;
      render_fill_word_i <= '0;
      render_clear_word_i <= '0;
      render_state_i <= '0;
      render_src_a_i <= '0;
      render_texel_rgb_i <= '0;
      render_texel_a_i <= '0;
      render_texel_idx_i <= '0;
      render_fb_base_i <= '0;
      render_fb_stride_i <= '0;
      fb_writer_i <= '0;
      phy_dq_i <= '0;
      for (slot = 0; slot < 3; slot = slot + 1) ring_delay_q[slot] <= 8'(slot * 17);
    end else if (run_c) begin
      lfsr_q <= {lfsr_q[62:0], ^(lfsr_q & 64'hd800_0000_0000_0000)};
      cycle_q <= cycle_q + 64'd1;
      ring_ready_capture_q <= ring_wr_ready_i;
      cnt_ready_capture_q <= cnt_snap_ready_i;

      // Defaults for one-cycle protocol pulses and don't-care payload motion.
      hps_req_grant_i <= 1'b0;
      hps_rd_valid_i <= 1'b0;
      hps_rd_last_i <= 1'b0;
      hps_rd_data_i <= entropy_c[127 +: 64];
      render_frame_begin_i <= 1'b0;
      render_frame_end_i <= 1'b0;
      cnt_snap_ready_i <= (cycle_q[3:0] != 4'hf);
      ring_wr_ready_i <= (cycle_q[2:0] != 3'h7);

      // The first accepted blit-source request proves that BeginFrame installed
      // a live shared framebuffer lease. Its registered response is held while
      // the one-shot renderer owns and retires work; completion hands the same
      // lease to blit before any source data can launch a blit write.

      // Three coherent FRAME_RING slots. FPGA-posted DONE remains HPS-visible
      // until the scheduler's later DONE -> FREE write is accepted.
      for (slot = 0; slot < 3; slot = slot + 1) begin
        if (hps_state_i[slot] == 2'd3)
          ring_done_dwell_q <= ring_done_dwell_q + 32'd1;
        if (ring_delay_q[slot] != 0) begin
          ring_delay_q[slot] <= ring_delay_q[slot] - 8'd1;
        end else if (hps_state_i[slot] == 2'd0) begin
          hps_state_i[slot] <= 2'd1;
          hps_byte_len_i[slot] <= 32'd168;
        end else if (hps_state_i[slot] == 2'd1) begin
          hps_state_i[slot] <= 2'd2;
        end
      end
      if (ring_wr_valid_o_captured_i && ring_ready_capture_q) begin
        ring_transactions_q <= ring_transactions_q + 32'd1;
        if (ring_state_checked_c == 2'd3) begin
          hps_state_i[ring_wr_slot_o_captured_i] <= 2'd3;
          ring_done_posts_q <= ring_done_posts_q + 32'd1;
        end else if (ring_state_checked_c == 2'd0) begin
          if (hps_state_i[ring_wr_slot_o_captured_i] == 2'd3)
            ring_done_frees_q <= ring_done_frees_q + 32'd1;
          else
            ring_sequence_errors_q <= ring_sequence_errors_q + 32'd1;
          hps_state_i[ring_wr_slot_o_captured_i] <= 2'd0;
          hps_byte_len_i[ring_wr_slot_o_captured_i] <= 32'd0;
          ring_delay_q[ring_wr_slot_o_captured_i] <= 8'd23;
        end else begin
          ring_sequence_errors_q <= ring_sequence_errors_q + 32'd1;
        end
      end
      if (blit_done_o_captured_i) begin
        if (blit_status_o_captured_i == 8'd0)
          blit_successes_q <= blit_successes_q + 32'd1;
        else
          blit_failures_q <= blit_failures_q + 32'd1;
      end

      // Registered HPS responder: 1..4 grant cycles and exactly the established
      // 16 idle edges followed by first data on the next edge. The first-beat
      // age witness is independent of the response state transition.
      case (hps_state_q)
        HPS_IDLE: if (hps_req_valid_o_captured_i) begin
          hps_addr_q <= hps_req_addr_o_captured_i;
          hps_len_q <= hps_req_len_o_captured_i;
          hps_write_q <= hps_req_write_checked_c;
          blit_source_read_q <= !hps_req_write_checked_c &&
                                hps_req_addr_o_captured_i[31:20] == 12'h002;
          if (!hps_req_write_checked_c &&
              hps_req_addr_o_captured_i[31:20] == 12'h002 &&
              !render_profile_done_q && render_timeouts_q == 0) begin
            render_enable_q <= 1'b1;
            fb_writer_i <= 1'b1;
          end
          hps_wait_q <= {5'd0, cycle_q[1:0]} + 7'd1;
          hps_beat_q <= '0;
          hps_state_q <= HPS_GRANT_WAIT;
        end
        HPS_GRANT_WAIT: if (hps_wait_q != 0) begin
          hps_wait_q <= hps_wait_q - 7'd1;
        end else begin
          hps_req_grant_i <= 1'b1;
          hps_wait_q <= hps_write_q ? 7'd127 : 7'd16;
          hps_first_beat_age_q <= '0;
          hps_first_beat_pending_q <= !hps_write_q && !blit_source_read_q;
          hps_timing_fault_hold_q <= 1'b0;
          hps_state_q <= hps_write_q ? HPS_WRITE_DATA : HPS_READ_LATENCY;
        end
        HPS_READ_LATENCY: if (hps_wait_q != 0) begin
          hps_wait_q <= hps_wait_q - 7'd1;
          if (hps_first_beat_pending_q)
            hps_first_beat_age_q <= hps_first_beat_age_q + 8'd1;
        end else if ((protocol_fault_i == 4'd11) &&
                     hps_first_beat_pending_q && !hps_timing_fault_hold_q) begin
          hps_timing_fault_hold_q <= 1'b1;
          hps_first_beat_age_q <= hps_first_beat_age_q + 8'd1;
        end else if (blit_source_read_q && !render_profile_done_q &&
                     render_timeouts_q == 0) begin
          hps_state_q <= HPS_READ_BEATS;
        end else begin
          hps_rd_valid_i <= 1'b1;
          hps_rd_data_i <= packet_word(hps_addr_q);
          hps_rd_last_i <= (7'd8 >= hps_len_q);
          if (hps_first_beat_pending_q) begin
            if (hps_first_beat_age_q == 8'd16)
              hps_first_beat_timing_witnesses_q <= hps_first_beat_timing_witnesses_q + 32'd1;
            else
              hps_first_beat_timing_errors_q <= hps_first_beat_timing_errors_q + 32'd1;
            hps_first_beat_pending_q <= 1'b0;
          end
          if (7'd8 >= hps_len_q) begin
            blit_source_read_q <= 1'b0;
            hps_read_completions_q <= hps_read_completions_q + 32'd1;
            hps_state_q <= HPS_IDLE;
          end else begin
            hps_beat_q <= 7'd1;
            hps_state_q <= HPS_READ_BEATS;
          end
        end
        HPS_READ_BEATS: begin
          if (blit_source_read_q && !render_profile_done_q &&
              render_timeouts_q == 0) begin
            // Keep registered response controls idle through both render jobs.
            hps_rd_valid_i <= 1'b0;
            hps_rd_last_i <= 1'b0;
          end else begin
            hps_rd_valid_i <= 1'b1;
            hps_rd_data_i <= packet_word(hps_addr_q + {22'd0, hps_beat_q, 3'b000});
            hps_rd_last_i <= ((hps_beat_q + 7'd1) * 8 >= hps_len_q);
            if ((hps_beat_q + 7'd1) * 8 >= hps_len_q) begin
              blit_source_read_q <= 1'b0;
              hps_read_completions_q <= hps_read_completions_q + 32'd1;
              hps_state_q <= HPS_IDLE;
            end else begin
              hps_beat_q <= hps_beat_q + 7'd1;
            end
          end
        end
        HPS_WRITE_DATA: begin
          if (hps_wr_valid_o_captured_i && hps_wr_last_o_captured_i) begin
            hps_wait_q <= '0;
            hps_state_q <= HPS_IDLE;
          end else if (hps_wait_q != 0) begin
            hps_wait_q <= hps_wait_q - 7'd1;
          end else begin
            hps_write_watchdog_faults_q <= hps_write_watchdog_faults_q + 32'd1;
            hps_state_q <= HPS_IDLE;
          end
        end
        default: hps_state_q <= HPS_IDLE;
      endcase

      // Directed pad phases cover center, signed INT_MIN/INT_MAX, then entropy.
      if (cycle_q[5:0] == 6'd0) begin
        pad_directed_phases_q[pad_phase_q] <= 1'b1;
        case (pad_phase_q)
          2'd0: begin
            pad_present_i <= 4'h0;
            for (pad = 0; pad < 4; pad = pad + 1) begin
              pad_buttons_i[pad] <= 32'h0000_0000;
              pad_lx_i[pad] <= 16'h0000;
              pad_ly_i[pad] <= 16'h0000;
              pad_rx_i[pad] <= 16'h0000;
              pad_ry_i[pad] <= 16'h0000;
            end
          end
          2'd1: begin
            pad_present_i <= 4'hf;
            for (pad = 0; pad < 4; pad = pad + 1) begin
              pad_buttons_i[pad] <= 32'h5555_5555;
              pad_lx_i[pad] <= 16'h8000;
              pad_ly_i[pad] <= 16'h8000;
              pad_rx_i[pad] <= 16'h8000;
              pad_ry_i[pad] <= 16'h8000;
            end
          end
          2'd2: begin
            pad_present_i <= 4'hf;
            for (pad = 0; pad < 4; pad = pad + 1) begin
              pad_buttons_i[pad] <= 32'haaaa_aaaa;
              pad_lx_i[pad] <= 16'h7fff;
              pad_ly_i[pad] <= 16'h7fff;
              pad_rx_i[pad] <= 16'h7fff;
              pad_ry_i[pad] <= 16'h7fff;
            end
          end
          default: begin
            pad_present_i <= cycle_q[9:6];
            for (pad = 0; pad < 4; pad = pad + 1) begin
              pad_buttons_i[pad] <= entropy_c[256 + pad*32 +: 32];
              pad_lx_i[pad] <= entropy_c[384 + pad*16 +: 16];
              pad_ly_i[pad] <= entropy_c[448 + pad*16 +: 16];
              pad_rx_i[pad] <= entropy_c[512 + pad*16 +: 16];
              pad_ry_i[pad] <= entropy_c[576 + pad*16 +: 16];
            end
          end
        endcase
        pad_phase_q <= pad_phase_q + 2'd1;
      end

      // Ready/valid stereo producer; samples remain stable during backpressure.
      if (aud_wr_valid_i && aud_wr_ready_o_captured_i) begin
        audio_accepts_q <= audio_accepts_q + 32'd1;
        if (audio_accepts_q[2:0] == 3'h7) begin
          aud_wr_valid_i <= 1'b0;
          audio_pause_q <= 8'd255;
        end else begin
          aud_wr_l_i <= entropy_c[640 +: 16];
          aud_wr_r_i <= entropy_c[672 +: 16];
        end
      end else if (!aud_wr_valid_i) begin
        if (audio_pause_q != 0) audio_pause_q <= audio_pause_q - 8'd1;
        else begin
          aud_wr_valid_i <= 1'b1;
          aud_wr_l_i <= entropy_c[704 +: 16];
          aud_wr_r_i <= entropy_c[736 +: 16];
        end
      end
      // The captured read-window beat is paired with the ready level from
      // the same shell edge. Require one exact ascending 0..39 window.
      if (cnt_snap_valid_o_captured_i && cnt_ready_capture_q) begin
        counter_accepts_q <= counter_accepts_q + 32'd1;
        if (counter_id_checked_c < 16'd40) begin
          counter_selector_seen_q[counter_id_checked_c[5:0]] <= 1'b1;
          if (counter_id_checked_c != counter_next_id_q)
            counter_sequence_errors_q <= counter_sequence_errors_q + 32'd1;
          if (counter_id_checked_c == 16'd39) begin
            if (counter_next_id_q == 16'd39 && counter_entries_q == 7'd39)
              counter_windows_q <= counter_windows_q + 32'd1;
            else
              counter_sequence_errors_q <= counter_sequence_errors_q + 32'd1;
            counter_next_id_q <= '0;
            counter_entries_q <= '0;
          end else begin
            counter_next_id_q <= counter_id_checked_c + 16'd1;
            counter_entries_q <= counter_entries_q + 7'd1;
          end
        end else begin
          counter_sequence_errors_q <= counter_sequence_errors_q + 32'd1;
        end
      end

      // Two lease-protected jobs are mandatory: a legal directed retirement,
      // then a distinct entropy-width transaction held through clear backpressure.
      case (render_state_q)
        RENDER_WAIT_INIT: if (init_done_o_captured_i && render_enable_q) begin
          render_issued_start_q <= render_issued_words_o_captured_i;
          render_retired_start_q <= render_retired_words_o_captured_i;
          render_work_seen_q <= 1'b0;
          render_retirement_seen_q <= 1'b0;
          render_job_entropy_q <= render_width_cover_q;
          render_width_cover_q <= ~render_width_cover_q;
          render_grid_w_i <= 6'd4;
          render_grid_h_i <= 6'd4;
          render_fill_word_i <= render_width_cover_q ? entropy_c[896 +: 64] : 64'ha5a5_a5a5_a5a5_a5a5;
          render_clear_word_i <= render_width_cover_q ? entropy_c[960 +: 64] : 64'h5a5a_5a5a_5a5a_5a5a;
          render_state_i <= render_width_cover_q ? 32'ha5e0_0068 : 32'h0000_0000;
          render_src_a_i <= render_width_cover_q ? entropy_c[1056 +: 8] : 8'hff;
          render_texel_rgb_i <= render_width_cover_q ? entropy_c[1088 +: 24] : 24'hff00ff;
          render_texel_a_i <= render_width_cover_q ? entropy_c[1120 +: 8] : 8'hff;
          render_texel_idx_i <= entropy_c[1152 +: 8];
          render_fb_base_i <= 27'd0;
          render_fb_stride_i <= 16'd128;
            render_kx0_i <= render_width_cover_q ? -23'sd1536 : -23'sd13312;
            render_ky0_i <= render_width_cover_q ? 23'sd17920 : -23'sd13312;
            render_kc0_i <= render_width_cover_q ? 48'sd15597568 : 48'sd231735296;
            render_kx1_i <= render_width_cover_q ? -23'sd15360 : 23'sd14336;
            render_ky1_i <= render_width_cover_q ? -23'sd17152 : -23'sd1024;
            render_kc1_i <= render_width_cover_q ? 48'sd256507904 : -48'sd13631488;
            render_kx2_i <= render_width_cover_q ? 23'sd16896 : -23'sd1024;
            render_ky2_i <= render_width_cover_q ? -23'sd768 : 23'sd14336;
            render_kc2_i <= render_width_cover_q ? 48'sd29491200 : -48'sd13631488;
            render_tl_i <= render_width_cover_q ? 3'b011 : 3'b101;
            render_ax_i <= render_width_cover_q ? -21'sd1024 : 21'sd1024;
            render_ay_i <= render_width_cover_q ? 21'sd15872 : 21'sd1024;
            render_bx_i <= render_width_cover_q ? -21'sd1792 : 21'sd15360;
            render_by_i <= render_width_cover_q ? -21'sd1024 : 21'sd2048;
            render_cx_i <= render_width_cover_q ? 21'sd16128 : 21'sd2048;
            render_cy_i <= render_width_cover_q ? 21'sd512 : 21'sd15360;
            render_min_x_i <= render_width_cover_q ? 12'sd0 : 12'sd4;
            render_max_x_i <= render_width_cover_q ? 12'sd63 : 12'sd60;
            render_min_y_i <= render_width_cover_q ? 12'sd0 : 12'sd4;
            render_max_y_i <= render_width_cover_q ? 12'sd62 : 12'sd60;
            render_src_id_i <= render_width_cover_q ? 16'hd5e7 : 16'h2a2a;
          render_frame_begin_i <= 1'b1;
          render_offer_stalled_q <= 1'b0;
          render_offer_mutated_q <= 1'b0;
          render_wait_q <= '0;
          render_state_q <= RENDER_BEGIN;
        end
        RENDER_BEGIN: begin
          // Offer the installed mode and payload immediately after frame_begin.
          // The binner's frame-clear phase supplies genuine ready backpressure.
          render_tri_valid_i <= 1'b1;
          render_state_q <= RENDER_OFFER;
        end
        RENDER_OFFER: begin
          if (render_tri_valid_i && !render_tri_ready_o_native_i) begin
            if (!render_offer_stalled_q) begin
              render_offer_snapshot_q <= render_offer_payload_c;
              if (render_job_entropy_q)
                render_entropy_offer_snapshot_q <= render_offer_payload_c;
              render_offer_stalled_q <= 1'b1;
            end else if (render_offer_payload_c != render_offer_snapshot_q) begin
              render_backpressure_stability_errors_q <=
                  render_backpressure_stability_errors_q + 32'd1;
            end
            // Positive control 12 changes a real offered field after the snapshot.
            if ((protocol_fault_i == 4'd12) && render_offer_stalled_q &&
                !render_offer_mutated_q) begin
              render_fill_word_i <= render_fill_word_i ^ 64'h1;
              render_offer_mutated_q <= 1'b1;
            end
          end
          if (render_tri_valid_i && render_tri_ready_o_native_i) begin
            render_tri_valid_i <= 1'b0;
            render_accepts_q <= render_accepts_q + 32'd1;
            render_accepted_payload_q <= render_offer_payload_c;
            if (render_offer_stalled_q) begin
              if (render_offer_payload_c == render_offer_snapshot_q) begin
                render_backpressure_witnesses_q <= render_backpressure_witnesses_q + 32'd1;
                if (render_job_entropy_q)
                  render_entropy_backpressure_witnesses_q <=
                      render_entropy_backpressure_witnesses_q + 32'd1;
              end else
                render_backpressure_stability_errors_q <=
                    render_backpressure_stability_errors_q + 32'd1;
            end
            if (render_job_entropy_q) begin
              // Classify using values sampled on the actual acceptance edge,
              // independently from the label that launched the transaction.
              render_entropy_accepted_payload_q <= render_offer_payload_c;
              render_entropy_accepted_mode_q <= render_state_i;
              render_entropy_accepted_fill_q <= render_fill_word_i;
              render_entropy_accepted_clear_q <= render_clear_word_i;
              render_entropy_accepted_coefficients_q <= render_triangle_coefficients_c;
              render_entropy_accepted_vertices_q <= render_triangle_vertices_c;
              render_entropy_accepted_bounds_q <= render_triangle_bounds_c;
              render_entropy_accepted_identity_q <= render_triangle_identity_c;
              if ((render_state_i == 32'ha5e0_0068) &&
                  (render_fill_word_i != 64'ha5a5_a5a5_a5a5_a5a5) &&
                  (render_clear_word_i != 64'h5a5a_5a5a_5a5a_5a5a) &&
                  ((render_triangle_coefficients_c == {-23'sd1536, 23'sd17920, 48'sd15597568, -23'sd15360, -23'sd17152, 48'sd256507904, 23'sd16896, -23'sd768, 48'sd29491200}) && (render_triangle_vertices_c == {-21'sd1024, 21'sd15872, -21'sd1792, -21'sd1024, 21'sd16128, 21'sd512}) && (render_triangle_bounds_c == {12'sd0, 12'sd63, 12'sd0, 12'sd62}) && (render_triangle_identity_c == {3'b011, 16'hd5e7})))
                render_entropy_accepts_q <= render_entropy_accepts_q + 32'd1;
              else
                render_accept_class_errors_q <= render_accept_class_errors_q + 32'd1;
            end else begin
              render_directed_accepted_coefficients_q <= render_triangle_coefficients_c;
              render_directed_accepted_vertices_q <= render_triangle_vertices_c;
              render_directed_accepted_bounds_q <= render_triangle_bounds_c;
              render_directed_accepted_identity_q <= render_triangle_identity_c;
              if ((render_state_i == 32'h0000_0000) &&
                  (render_fill_word_i == 64'ha5a5_a5a5_a5a5_a5a5) &&
                  (render_clear_word_i == 64'h5a5a_5a5a_5a5a_5a5a) &&
                  ((render_triangle_coefficients_c == {-23'sd13312, -23'sd13312, 48'sd231735296, 23'sd14336, -23'sd1024, -48'sd13631488, -23'sd1024, 23'sd14336, -48'sd13631488}) && (render_triangle_vertices_c == {21'sd1024, 21'sd1024, 21'sd15360, 21'sd2048, 21'sd2048, 21'sd15360}) && (render_triangle_bounds_c == {12'sd4, 12'sd60, 12'sd4, 12'sd60}) && (render_triangle_identity_c == {3'b101, 16'h2a2a})))
                render_directed_accepts_q <= render_directed_accepts_q + 32'd1;
              else
                render_accept_class_errors_q <= render_accept_class_errors_q + 32'd1;
            end
            render_frame_end_i <= 1'b1;
            render_state_q <= RENDER_END;
          end
        end
        RENDER_END: begin
          render_wait_q <= '0;
          render_state_q <= RENDER_DRAIN;
        end
        RENDER_DRAIN: begin
          render_wait_q <= render_wait_q + 16'd1;
          if (render_issued_words_o_captured_i > render_issued_start_q)
            render_work_seen_q <= 1'b1;
          if (render_retired_words_o_captured_i > render_retired_start_q)
            render_retirement_seen_q <= 1'b1;
          if ((protocol_fault_i != 4'd4) && !render_job_entropy_q &&
              (render_work_seen_q ||
               render_issued_words_o_captured_i > render_issued_start_q) &&
              (render_retirement_seen_q ||
               render_retired_words_o_captured_i > render_retired_start_q) &&
              render_drained_o_captured_i &&
              render_issued_words_o_captured_i == render_retired_words_o_captured_i) begin
            render_state_q <= RENDER_WAIT_INIT;
          end else if ((protocol_fault_i != 4'd4) && render_job_entropy_q &&
                       (render_work_seen_q ||
                        render_issued_words_o_captured_i > render_issued_start_q) &&
                       (render_retirement_seen_q ||
                        render_retired_words_o_captured_i > render_retired_start_q) &&
                       render_drained_o_captured_i &&
                       render_issued_words_o_captured_i == render_retired_words_o_captured_i) begin
            render_entropy_completions_q <= render_entropy_completions_q + 32'd1;
            render_drains_q <= render_drains_q + 32'd1;
            render_profile_done_q <= 1'b1;
            render_enable_q <= 1'b0;
            fb_writer_i <= 1'b0;
            render_state_q <= RENDER_WAIT_INIT;
          end else if (&render_wait_q ||
                       ((protocol_fault_i == 4'd4) && &render_wait_q[7:0])) begin
            render_timeouts_q <= render_timeouts_q + 32'd1;
            render_enable_q <= 1'b0;
            fb_writer_i <= 1'b0;
            render_state_q <= RENDER_WAIT_INIT;
          end
        end
        default: render_state_q <= RENDER_WAIT_INIT;
      endcase

      // A legal geometry read owns exactly eight accepted beats. Before the
      // first legal verdict, reset-active quarantine owns every unsolicited
      // beat through initial idle, request wait, and response pending. Completed
      // ownership persists through the next verdict; a denied verdict then
      // starts quarantine through idle/request/pending states until a later
      // legal verdict begins new ownership.
      if (guard_frame_owned_q && !guard_beat_pending_q &&
          (guard_extra_fault_delay_q != 0))
        guard_extra_fault_delay_q <= guard_extra_fault_delay_q - 4'd1;
      if (guard_frame_owned_q && guard_response_pending_q &&
          (guard_verdict_extra_fault_delay_q != 0))
        guard_verdict_extra_fault_delay_q <=
            guard_verdict_extra_fault_delay_q - 4'd1;
      if (guard_denial_watch_q && !guard_response_pending_q &&
          !guard_beat_pending_q && (guard_denial_extra_fault_delay_q != 0))
        guard_denial_extra_fault_delay_q <=
            guard_denial_extra_fault_delay_q - 4'd1;
      if (guard_beat_pending_q) begin
        if (guard_beat_valid_checked_c) begin
          guard_beat_wait_q <= '0;
          if (guard_beat_count_q < 4'd7) begin
            if (guard_beat_last_checked_c) begin
              guard_early_last_errors_q <= guard_early_last_errors_q + 32'd1;
              guard_beat_pending_q <= 1'b0;
              guard_frame_owned_q <= 1'b0;
              guard_gap_q <= 8'd11;
            end else begin
              guard_beat_count_q <= guard_beat_count_q + 4'd1;
            end
          end else if (guard_beat_count_q == 4'd7) begin
            guard_beat_pending_q <= 1'b0;
            if (guard_beat_last_checked_c) begin
              guard_last_beats_q <= guard_last_beats_q + 32'd1;
              guard_exact_frames_q <= guard_exact_frames_q + 32'd1;
              guard_gap_q <= 8'd11;
              guard_extra_fault_delay_q <= 4'd5;
            end else begin
              guard_late_last_errors_q <= guard_late_last_errors_q + 32'd1;
              guard_frame_owned_q <= 1'b0;
              guard_gap_q <= 8'd11;
            end
          end else begin
            guard_extra_beat_errors_q <= guard_extra_beat_errors_q + 32'd1;
            guard_beat_pending_q <= 1'b0;
            guard_frame_owned_q <= 1'b0;
            guard_gap_q <= 8'd11;
          end
        end else if (&guard_beat_wait_q ||
                     ((protocol_fault_i == 4'd5) && &guard_beat_wait_q[7:0])) begin
          guard_beat_pending_q <= 1'b0;
          guard_frame_owned_q <= 1'b0;
          guard_beat_timeouts_q <= guard_beat_timeouts_q + 32'd1;
          guard_gap_q <= 8'd11;
        end else begin
          guard_beat_wait_q <= guard_beat_wait_q + 16'd1;
        end
      end else if (guard_response_pending_q) begin
        // A legal verdict starts new ownership before its optional first beat
        // is classified. Otherwise old-frame, pre-ownership, or post-denial
        // quarantine owns every unsolicited beat through verdict resolution.
        if (guard_verdict_ok_checked_c) begin
          guard_accepts_q <= guard_accepts_q + 32'd1;
          guard_response_pending_q <= 1'b0;
          guard_verdict_extra_fault_delay_q <= '0;
          guard_denial_watch_q <= 1'b0;
          guard_preownership_watch_q <= 1'b0;
          guard_denial_extra_fault_delay_q <= '0;
          guard_verdict_wait_q <= '0;
          guard_beat_wait_q <= '0;
          if (guard_beat_valid_checked_c) begin
            if (guard_beat_last_checked_c) begin
              guard_early_last_errors_q <= guard_early_last_errors_q + 32'd1;
              guard_beat_pending_q <= 1'b0;
              guard_frame_owned_q <= 1'b0;
              guard_gap_q <= 8'd11;
            end else begin
              guard_beat_pending_q <= 1'b1;
              guard_frame_owned_q <= 1'b1;
              guard_beat_count_q <= 4'd1;
            end
          end else begin
            guard_beat_pending_q <= 1'b1;
            guard_frame_owned_q <= 1'b1;
            guard_beat_count_q <= '0;
          end
        end else begin
          if ((guard_frame_owned_q || guard_denial_watch_q ||
               (guard_preownership_watch_q &&
                ((protocol_fault_i == 4'd0) ||
                 (protocol_fault_i == 4'd15))) ||
               guard_verdict_violation_checked_c) &&
              guard_beat_valid_checked_c) begin
            guard_extra_beat_errors_q <= guard_extra_beat_errors_q + 32'd1;
            if ((protocol_fault_i == 4'd13) && guard_frame_owned_q &&
                (guard_verdict_extra_fault_delay_q == 4'd1))
              guard_verdict_extra_witnesses_q <=
                  guard_verdict_extra_witnesses_q + 32'd1;
            if ((protocol_fault_i == 4'd15) &&
                guard_preownership_watch_q &&
                !guard_verdict_violation_checked_c &&
                !guard_preownership_fault_windows_q[2])
              guard_preownership_fault_windows_q[2] <= 1'b1;
            guard_verdict_extra_fault_delay_q <= '0;
          end
          if (guard_verdict_violation_checked_c) begin
            guard_rejects_q <= guard_rejects_q + 32'd1;
            guard_response_pending_q <= 1'b0;
            guard_frame_owned_q <= 1'b0;
            if ((protocol_fault_i == 4'd0) ||
                (protocol_fault_i == 4'd14)) begin
              guard_denial_watch_q <= 1'b1;
              guard_denial_extra_fault_delay_q <= 4'd1;
            end else begin
              // Keep unrelated positive controls single-arm; fault hooks are
              // simulation-only and synthesize at the normal zero value.
              guard_denial_watch_q <= 1'b0;
              guard_denial_extra_fault_delay_q <= '0;
            end
            guard_verdict_extra_fault_delay_q <= '0;
            guard_verdict_wait_q <= '0;
            guard_gap_q <= 8'd11;
          end else if (&guard_verdict_wait_q ||
                       ((protocol_fault_i == 4'd7) && &guard_verdict_wait_q[7:0])) begin
            guard_response_pending_q <= 1'b0;
            guard_frame_owned_q <= 1'b0;
            guard_verdict_extra_fault_delay_q <= '0;
            guard_verdict_timeouts_q <= guard_verdict_timeouts_q + 32'd1;
            guard_gap_q <= 8'd11;
          end else begin
            guard_verdict_wait_q <= guard_verdict_wait_q + 16'd1;
          end
        end
      end else begin
        if ((guard_frame_owned_q || guard_denial_watch_q ||
             (guard_preownership_watch_q &&
              ((protocol_fault_i == 4'd0) ||
               (protocol_fault_i == 4'd15)))) &&
            guard_beat_valid_checked_c) begin
          guard_extra_beat_errors_q <= guard_extra_beat_errors_q + 32'd1;
          if ((protocol_fault_i == 4'd14) && guard_denial_watch_q &&
              (guard_denial_extra_fault_delay_q == 4'd1))
            guard_post_denial_extra_witnesses_q <=
                guard_post_denial_extra_witnesses_q + 32'd1;
          if ((protocol_fault_i == 4'd15) &&
              guard_preownership_watch_q) begin
            if (geom_guard_req_i.valid &&
                !guard_preownership_fault_windows_q[1])
              guard_preownership_fault_windows_q[1] <= 1'b1;
            else if (!geom_guard_req_i.valid &&
                     !guard_preownership_fault_windows_q[0])
              guard_preownership_fault_windows_q[0] <= 1'b1;
          end
          guard_denial_extra_fault_delay_q <= '0;
          guard_gap_q <= 8'd11;
        end
        if (geom_guard_req_i.valid && geom_guard_rsp_o_captured_i.ready) begin
          geom_guard_req_i.valid <= 1'b0;
          guard_response_pending_q <= 1'b1;
          guard_extra_fault_delay_q <= '0;
          guard_verdict_extra_fault_delay_q <=
              guard_frame_owned_q ? 4'd1 : 4'd0;
          guard_verdict_wait_q <= '0;
        end else if (!geom_guard_req_i.valid) begin
          if (guard_gap_q != 0) guard_gap_q <= guard_gap_q - 8'd1;
          else begin
            geom_guard_req_i.valid <= 1'b1;
            guard_negative_q <= ~guard_negative_q;
            if (!guard_negative_q) begin
              geom_guard_req_i.write <= 1'b0;
              geom_guard_req_i.client <= ZHAO_CLIENT_ENGINE1;
              geom_guard_req_i.addr <= 27'h6a00000;
              geom_guard_req_i.len <= 7'd64;
              geom_guard_req_i.be <= 64'hffff_ffff_ffff_ffff;
            end else begin
              geom_guard_req_i.write <= lfsr_q[0];
              geom_guard_req_i.client <= zhao_client_e'(lfsr_q[3:1]);
              geom_guard_req_i.addr <= {1'b0, lfsr_q[25:6], 6'b0};
              geom_guard_req_i.len <= {1'b0, lfsr_q[5:0]};
              geom_guard_req_i.be <= lfsr_q;
            end
          end
        end
      end

      // Compact SDR responder. Captured READ at R schedules first DQ at R+3.
      if (sdr_read_beat_q != 0) begin
        phy_dq_i <= sdr_row_q[15:0] ^ {3'b0, sdr_row_q[28:16]} ^ {12'h0, sdr_read_beat_q};
        sdr_read_beat_q <= sdr_read_beat_q - 4'd1;
      end else if (sdr_read_wait_q != 0) begin
        sdr_read_wait_q <= sdr_read_wait_q - 4'd1;
        if (sdr_read_wait_q == 4'd1) begin
          phy_dq_i <= sdr_row_q[15:0] ^ {3'b0, sdr_row_q[28:16]};
          sdr_read_beat_q <= 4'd7;
          sdr_read_responses_q <= sdr_read_responses_q + 32'd1;
        end
      end else begin
        phy_dq_i <= entropy_c[1888 +: 16];
        if (!phy_cs_n_o_captured_i && phy_ras_n_o_captured_i &&
            !phy_cas_n_o_captured_i && phy_we_n_o_captured_i) begin
          sdr_row_q <= {phy_ba_o_captured_i, phy_a_o_captured_i, cycle_q[13:0]};
          sdr_read_wait_q <= 4'd1;
        end
      end
      // WRITE command and DQ data are distinct SDR phases. Hold the decoded
      // command identity until output-enable proves the delayed data phase.
      if (sdr_dq_oe_checked_c && !sdr_write_active_q)
        sdr_write_phase_errors_q <= sdr_write_phase_errors_q + 32'd1;
      if (!phy_cs_n_o_captured_i && phy_ras_n_o_captured_i &&
          !phy_cas_n_o_captured_i && !phy_we_n_o_captured_i &&
          protocol_fault_i != 4'd6) begin
        sdr_write_pending_q <= 1'b1;
        sdr_write_active_q <= 1'b1;
        sdr_write_command_q <= {phy_ba_o_captured_i, phy_a_o_captured_i};
      end
      if (sdr_write_active_q && phy_dq_oe_o_captured_i) begin
        if (sdr_write_pending_q) begin
          sdr_write_transactions_q <= sdr_write_transactions_q + 32'd1;
          sdr_write_pending_q <= 1'b0;
        end
        sdr_write_observations_q <= {sdr_write_observations_q[30:0],
                                     sdr_write_observations_q[31]}
                                  ^ {17'd0, sdr_write_command_q}
                                  ^ {14'd0, phy_dqm_o_captured_i,
                                     phy_dq_o_captured_i};
      end else if (sdr_write_active_q && !sdr_write_pending_q) begin
        sdr_write_active_q <= 1'b0;
      end
    end
  end
endmodule

module zhao_shell_fit_gpu_sink (
  input  logic clk,
  input  logic rst_n,
  input  logic [1694:0] payload_i,
  output logic [1694:0] capture_o,
  output logic signature_o,
  output logic epoch_o
);

  // Immediate native-domain endpoint for every shell output bit.
  (* preserve *) logic [1694:0] capture_q;
  always_ff @(posedge clk) capture_q <= payload_i;
  assign capture_o = capture_q;

  logic [9:0] release_q;
  logic run_c;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) release_q <= '0;
    else release_q <= {release_q[8:0], 1'b1};
  end
  assign run_c = release_q[9]; // 2 sync edges + 8 warm-up edges

  (* preserve *) logic [63:0] activity_lfsr_q;
  logic [5:0] chunk_index_q, selected_index_q;
  logic selected_valid_q;
  logic [31:0] selected_c, selected_q;
  (* preserve *) logic [31:0] misr_q;
  (* preserve *) logic [31:0] snapshot_q;
  logic [4:0] serial_index_q;
  logic serializer_busy_q;
  logic [31:0] chunk_wraps_q;
  logic [31:0] snapshots_q;
  logic [31:0] misr_next_c;

  always_comb begin
    selected_c = 32'h0;
    case (chunk_index_q)
      6'd0: selected_c = capture_q[0 +: 32];
      6'd1: selected_c = capture_q[32 +: 32];
      6'd2: selected_c = capture_q[64 +: 32];
      6'd3: selected_c = capture_q[96 +: 32];
      6'd4: selected_c = capture_q[128 +: 32];
      6'd5: selected_c = capture_q[160 +: 32];
      6'd6: selected_c = capture_q[192 +: 32];
      6'd7: selected_c = capture_q[224 +: 32];
      6'd8: selected_c = capture_q[256 +: 32];
      6'd9: selected_c = capture_q[288 +: 32];
      6'd10: selected_c = capture_q[320 +: 32];
      6'd11: selected_c = capture_q[352 +: 32];
      6'd12: selected_c = capture_q[384 +: 32];
      6'd13: selected_c = capture_q[416 +: 32];
      6'd14: selected_c = capture_q[448 +: 32];
      6'd15: selected_c = capture_q[480 +: 32];
      6'd16: selected_c = capture_q[512 +: 32];
      6'd17: selected_c = capture_q[544 +: 32];
      6'd18: selected_c = capture_q[576 +: 32];
      6'd19: selected_c = capture_q[608 +: 32];
      6'd20: selected_c = capture_q[640 +: 32];
      6'd21: selected_c = capture_q[672 +: 32];
      6'd22: selected_c = capture_q[704 +: 32];
      6'd23: selected_c = capture_q[736 +: 32];
      6'd24: selected_c = capture_q[768 +: 32];
      6'd25: selected_c = capture_q[800 +: 32];
      6'd26: selected_c = capture_q[832 +: 32];
      6'd27: selected_c = capture_q[864 +: 32];
      6'd28: selected_c = capture_q[896 +: 32];
      6'd29: selected_c = capture_q[928 +: 32];
      6'd30: selected_c = capture_q[960 +: 32];
      6'd31: selected_c = capture_q[992 +: 32];
      6'd32: selected_c = capture_q[1024 +: 32];
      6'd33: selected_c = capture_q[1056 +: 32];
      6'd34: selected_c = capture_q[1088 +: 32];
      6'd35: selected_c = capture_q[1120 +: 32];
      6'd36: selected_c = capture_q[1152 +: 32];
      6'd37: selected_c = capture_q[1184 +: 32];
      6'd38: selected_c = capture_q[1216 +: 32];
      6'd39: selected_c = capture_q[1248 +: 32];
      6'd40: selected_c = capture_q[1280 +: 32];
      6'd41: selected_c = capture_q[1312 +: 32];
      6'd42: selected_c = capture_q[1344 +: 32];
      6'd43: selected_c = capture_q[1376 +: 32];
      6'd44: selected_c = capture_q[1408 +: 32];
      6'd45: selected_c = capture_q[1440 +: 32];
      6'd46: selected_c = capture_q[1472 +: 32];
      6'd47: selected_c = capture_q[1504 +: 32];
      6'd48: selected_c = capture_q[1536 +: 32];
      6'd49: selected_c = capture_q[1568 +: 32];
      6'd50: selected_c = capture_q[1600 +: 32];
      6'd51: selected_c = capture_q[1632 +: 32];
      6'd52: begin
        selected_c[30:0] = capture_q[1664 +: 31];
      end
      default: selected_c = 32'h0;
    endcase
  end

  assign misr_next_c = {misr_q[30:0], 1'b0}
                     ^ (misr_q[31] ? 32'h0040_0007 : 32'h0)
                     ^ selected_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      activity_lfsr_q <= 64'hd1b54a32d192ed03;
      chunk_index_q <= '0;
      selected_index_q <= '0;
      selected_valid_q <= 1'b0;
      selected_q <= '0;
      misr_q <= '0;
      snapshot_q <= '0;
      serial_index_q <= '0;
      serializer_busy_q <= 1'b0;
      signature_o <= 1'b0;
      epoch_o <= 1'b0;
      chunk_wraps_q <= '0;
      snapshots_q <= '0;
    end else if (!run_c) begin
      activity_lfsr_q <= 64'hd1b54a32d192ed03;
      chunk_index_q <= '0;
      selected_index_q <= '0;
      selected_valid_q <= 1'b0;
      selected_q <= '0;
      misr_q <= '0;
      snapshot_q <= '0;
      serial_index_q <= '0;
      serializer_busy_q <= 1'b0;
      signature_o <= 1'b0;
      epoch_o <= 1'b0;
      chunk_wraps_q <= '0;
      snapshots_q <= '0;
    end else begin
      activity_lfsr_q <= {activity_lfsr_q[62:0],
                          ^(activity_lfsr_q & 64'hd800_0000_0000_0000)};
      selected_q <= selected_c;
      selected_index_q <= chunk_index_q;
      selected_valid_q <= 1'b1;
      if (chunk_index_q == 6'd52) begin
        chunk_index_q <= '0;
      end else begin
        chunk_index_q <= chunk_index_q + 6'd1;
      end

      if (selected_valid_q) begin
        misr_q <= misr_next_c;
        if (selected_index_q == 6'd52) begin
          chunk_wraps_q <= chunk_wraps_q + 32'd1;
          if (!serializer_busy_q) begin
            snapshot_q <= misr_next_c;
            signature_o <= misr_next_c[0];
            epoch_o <= ~epoch_o;
            serial_index_q <= 5'd1;
            serializer_busy_q <= 1'b1;
            snapshots_q <= snapshots_q + 32'd1;
          end
        end
      end

      if (serializer_busy_q) begin
        signature_o <= snapshot_q[serial_index_q];
        if (serial_index_q == 5'd31) begin
          serializer_busy_q <= 1'b0;
          serial_index_q <= '0;
        end else begin
          serial_index_q <= serial_index_q + 5'd1;
        end
      end
    end
  end
endmodule

module zhao_shell_fit_video_sink (
  input  logic clk,
  input  logic rst_n,
  input  logic [167:0] payload_i,
  output logic signature_o,
  output logic epoch_o
);

  // Immediate native-domain endpoint for every shell output bit.
  (* preserve *) logic [167:0] capture_q;
  always_ff @(posedge clk) capture_q <= payload_i;

  logic [9:0] release_q;
  logic run_c;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) release_q <= '0;
    else release_q <= {release_q[8:0], 1'b1};
  end
  assign run_c = release_q[9]; // 2 sync edges + 8 warm-up edges

  (* preserve *) logic [63:0] activity_lfsr_q;
  logic [2:0] chunk_index_q, selected_index_q;
  logic selected_valid_q;
  logic [31:0] selected_c, selected_q;
  (* preserve *) logic [31:0] misr_q;
  (* preserve *) logic [31:0] snapshot_q;
  logic [4:0] serial_index_q;
  logic serializer_busy_q;
  logic [31:0] chunk_wraps_q;
  logic [31:0] snapshots_q;
  logic [31:0] misr_next_c;

  always_comb begin
    selected_c = 32'h0;
    case (chunk_index_q)
      3'd0: selected_c = capture_q[0 +: 32];
      3'd1: selected_c = capture_q[32 +: 32];
      3'd2: selected_c = capture_q[64 +: 32];
      3'd3: selected_c = capture_q[96 +: 32];
      3'd4: selected_c = capture_q[128 +: 32];
      3'd5: begin
        selected_c[7:0] = capture_q[160 +: 8];
      end
      default: selected_c = 32'h0;
    endcase
  end

  assign misr_next_c = {misr_q[30:0], 1'b0}
                     ^ (misr_q[31] ? 32'h0040_0007 : 32'h0)
                     ^ selected_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      activity_lfsr_q <= 64'h94d049bb133111eb;
      chunk_index_q <= '0;
      selected_index_q <= '0;
      selected_valid_q <= 1'b0;
      selected_q <= '0;
      misr_q <= '0;
      snapshot_q <= '0;
      serial_index_q <= '0;
      serializer_busy_q <= 1'b0;
      signature_o <= 1'b0;
      epoch_o <= 1'b0;
      chunk_wraps_q <= '0;
      snapshots_q <= '0;
    end else if (!run_c) begin
      activity_lfsr_q <= 64'h94d049bb133111eb;
      chunk_index_q <= '0;
      selected_index_q <= '0;
      selected_valid_q <= 1'b0;
      selected_q <= '0;
      misr_q <= '0;
      snapshot_q <= '0;
      serial_index_q <= '0;
      serializer_busy_q <= 1'b0;
      signature_o <= 1'b0;
      epoch_o <= 1'b0;
      chunk_wraps_q <= '0;
      snapshots_q <= '0;
    end else begin
      activity_lfsr_q <= {activity_lfsr_q[62:0],
                          ^(activity_lfsr_q & 64'hd800_0000_0000_0000)};
      selected_q <= selected_c;
      selected_index_q <= chunk_index_q;
      selected_valid_q <= 1'b1;
      if (chunk_index_q == 3'd5) begin
        chunk_index_q <= '0;
      end else begin
        chunk_index_q <= chunk_index_q + 3'd1;
      end

      if (selected_valid_q) begin
        misr_q <= misr_next_c;
        if (selected_index_q == 3'd5) begin
          chunk_wraps_q <= chunk_wraps_q + 32'd1;
          if (!serializer_busy_q) begin
            snapshot_q <= misr_next_c;
            signature_o <= misr_next_c[0];
            epoch_o <= ~epoch_o;
            serial_index_q <= 5'd1;
            serializer_busy_q <= 1'b1;
            snapshots_q <= snapshots_q + 32'd1;
          end
        end
      end

      if (serializer_busy_q) begin
        signature_o <= snapshot_q[serial_index_q];
        if (serial_index_q == 5'd31) begin
          serializer_busy_q <= 1'b0;
          serial_index_q <= '0;
        end else begin
          serial_index_q <= serial_index_q + 5'd1;
        end
      end
    end
  end
endmodule

module zhao_shell_fit_audio_sink (
  input  logic clk,
  input  logic rst_n,
  input  logic [65:0] payload_i,
  output logic signature_o,
  output logic epoch_o
);

  // Immediate native-domain endpoint for every shell output bit.
  (* preserve *) logic [65:0] capture_q;
  always_ff @(posedge clk) capture_q <= payload_i;

  logic [9:0] release_q;
  logic run_c;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) release_q <= '0;
    else release_q <= {release_q[8:0], 1'b1};
  end
  assign run_c = release_q[9]; // 2 sync edges + 8 warm-up edges

  (* preserve *) logic [63:0] activity_lfsr_q;
  logic [1:0] chunk_index_q, selected_index_q;
  logic selected_valid_q;
  logic [31:0] selected_c, selected_q;
  (* preserve *) logic [31:0] misr_q;
  (* preserve *) logic [31:0] snapshot_q;
  logic [4:0] serial_index_q;
  logic serializer_busy_q;
  logic [31:0] chunk_wraps_q;
  logic [31:0] snapshots_q;
  logic [31:0] misr_next_c;

  always_comb begin
    selected_c = 32'h0;
    case (chunk_index_q)
      2'd0: selected_c = capture_q[0 +: 32];
      2'd1: selected_c = capture_q[32 +: 32];
      2'd2: begin
        selected_c[1:0] = capture_q[64 +: 2];
      end
      default: selected_c = 32'h0;
    endcase
  end

  assign misr_next_c = {misr_q[30:0], 1'b0}
                     ^ (misr_q[31] ? 32'h0040_0007 : 32'h0)
                     ^ selected_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      activity_lfsr_q <= 64'hbf58476d1ce4e5b9;
      chunk_index_q <= '0;
      selected_index_q <= '0;
      selected_valid_q <= 1'b0;
      selected_q <= '0;
      misr_q <= '0;
      snapshot_q <= '0;
      serial_index_q <= '0;
      serializer_busy_q <= 1'b0;
      signature_o <= 1'b0;
      epoch_o <= 1'b0;
      chunk_wraps_q <= '0;
      snapshots_q <= '0;
    end else if (!run_c) begin
      activity_lfsr_q <= 64'hbf58476d1ce4e5b9;
      chunk_index_q <= '0;
      selected_index_q <= '0;
      selected_valid_q <= 1'b0;
      selected_q <= '0;
      misr_q <= '0;
      snapshot_q <= '0;
      serial_index_q <= '0;
      serializer_busy_q <= 1'b0;
      signature_o <= 1'b0;
      epoch_o <= 1'b0;
      chunk_wraps_q <= '0;
      snapshots_q <= '0;
    end else begin
      activity_lfsr_q <= {activity_lfsr_q[62:0],
                          ^(activity_lfsr_q & 64'hd800_0000_0000_0000)};
      selected_q <= selected_c;
      selected_index_q <= chunk_index_q;
      selected_valid_q <= 1'b1;
      if (chunk_index_q == 2'd2) begin
        chunk_index_q <= '0;
      end else begin
        chunk_index_q <= chunk_index_q + 2'd1;
      end

      if (selected_valid_q) begin
        misr_q <= misr_next_c;
        if (selected_index_q == 2'd2) begin
          chunk_wraps_q <= chunk_wraps_q + 32'd1;
          if (!serializer_busy_q) begin
            snapshot_q <= misr_next_c;
            signature_o <= misr_next_c[0];
            epoch_o <= ~epoch_o;
            serial_index_q <= 5'd1;
            serializer_busy_q <= 1'b1;
            snapshots_q <= snapshots_q + 32'd1;
          end
        end
      end

      if (serializer_busy_q) begin
        signature_o <= snapshot_q[serial_index_q];
        if (serial_index_q == 5'd31) begin
          serializer_busy_q <= 1'b0;
          serial_index_q <= '0;
        end else begin
          serial_index_q <= serial_index_q + 5'd1;
        end
      end
    end
  end
endmodule

/* verilator lint_on DECLFILENAME */
