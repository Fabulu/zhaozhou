// tb_zhao_shell.sv — TESTBENCH WRAPPER (W2.7): zhao_shell_top (the whole
// Phase-2 console) + the behavioural SDRAM model (sim/models/, testbench-
// only, D2). Pure wiring: every shell port passes straight through; the
// model hangs off the PHY pins and adds its peek/error surface.
//
// TESTBENCH COMPONENT — excluded from synthesis and from every lint target
// (the model is non-synthesizable by design).

module tb_zhao_shell (
  // clocks + reset (harness-driven, fixed phase: vid=gpu/2, audio=gpu/4)
  input  logic gpu_clk,
  input  logic vid_clk,
  input  logic audio_clk,
  input  logic rst_n,

  // FRAME_RING view (harness = HPS)
  input  logic [1:0]  hps_state_i [0:2],
  input  logic [31:0] hps_byte_len_i [0:2],
  output logic        ring_wr_valid_o,
  output logic [1:0]  ring_wr_slot_o,
  output logic [1:0]  ring_wr_state_o,
  input  logic        ring_wr_ready_i,

  // HPS bridge harness side
  output logic        hps_req_valid_o,
  output logic        hps_req_write_o,
  output logic [31:0] hps_req_addr_o,
  output logic [6:0]  hps_req_len_o,
  input  logic        hps_req_grant_i,
  output logic        hps_wr_valid_o,
  output logic [63:0] hps_wr_data_o,
  output logic        hps_wr_last_o,
  input  logic        hps_rd_valid_i,
  input  logic [63:0] hps_rd_data_i,
  input  logic        hps_rd_last_i,

  // pads
  input  logic [3:0]  pad_present_i,
  input  logic [31:0] pad_buttons_i [0:3],
  input  logic [15:0] pad_lx_i [0:3],
  input  logic [15:0] pad_ly_i [0:3],
  input  logic [15:0] pad_rx_i [0:3],
  input  logic [15:0] pad_ry_i [0:3],

  // audio
  input  logic        aud_wr_valid_i,
  input  logic [15:0] aud_wr_l_i,
  input  logic [15:0] aud_wr_r_i,
  output logic        aud_wr_ready_o,
  output logic        aud_refill_req_o,
  output logic [11:0] aud_occupancy_o,
  output logic        pcm_valid_o,
  output logic [15:0] pcm_l_o,
  output logic [15:0] pcm_r_o,
  output logic        underrun_status_o,
  output logic [31:0] audio_underruns_o,

  // displayed pixel stream
  output logic        px_valid_o,
  output logic [15:0] px_rgb_o,
  output logic [9:0]  px_x_o,
  output logic [7:0]  px_y_o,
  output logic        px_hsync_o,
  output logic        px_vsync_o,
  output logic        px_hblank_o,
  output logic        px_vblank_o,
  output logic        scaler_violation_o,

  // DEBUG.CRC
  output logic [31:0] crc_frame_o,
  output logic        crc_valid_o,
  output logic [31:0] crc_bytes_o,
  output logic        crc_size_err_o,

  // frame boundary
  output logic        gpu_tick_o,
  output logic [31:0] gpu_tick_frame_id_o,
  output logic        gpu_tick_repeated_o,
  output logic [0:0]  gpu_complete_slot_o,
  output logic [63:0] deadline_faults_o,
  output logic [63:0] frame_cycles_o,

  // CMD observability
  output logic [2:0]  slot_state_o [0:2],
  output logic        fence_valid_o,
  output logic [1:0]  fence_slot_o,
  output logic        fence_ok_o,
  output logic [7:0]  fence_status_o,
  output logic [1:0]  mode_act_o,
  output logic        dma_done_o,
  output logic [7:0]  dma_status_o,
  output logic        blit_done_o,
  output logic [7:0]  blit_status_o,

  // INPUT observability
  output logic [639:0] pad_frame_flat_o,
  output logic [15:0]  pad_sequence_o [0:3],
  output logic [63:0]  input_gaps_o,
  output logic [7:0]   rumble_duty_o [0:3],
  output logic [3:0]   rumble_active_o,
  output logic [3:0]   rumble_pwm_o,
  output logic [63:0]  rumble_drops_o,

  // counters window
  input  logic        cnt_snap_ready_i,
  output logic        cnt_snap_valid_o,
  output logic [15:0] cnt_snap_id_o,
  output logic [63:0] cnt_snap_value_o,
  output logic        cnt_window_open_o,
  output logic        cnt_cat_violation_o,

  // MEM observability + shell tripwires
  output logic [31:0] guard_violations_o,
  output logic [63:0] starvation_o,
  output logic        init_done_o,
  output logic [31:0] refresh_stalls_o,
  output logic [31:0] bank_conflicts_o,
  output logic [31:0] scanout_preempted_o,
  output logic [31:0] hps_err_count_o,
  output logic        shell_err_wfifo_o,
  output logic        shell_err_route_o,
  output logic        shell_err_cdc_o,
  output logic        shell_err_framer_o,

  // SDRAM model peek + errors
  input  logic        peek_en,
  input  logic [25:0] peek_waddr,
  output logic [15:0] peek_data,
  output logic        model_error,
  output logic [5:0]  model_err_kind   // {mrs,protocol,refresh,trc,trp,trcd}
);

  logic        phy_cs_n, phy_ras_n, phy_cas_n, phy_we_n, phy_dq_oe;
  logic [12:0] phy_a;
  logic [1:0]  phy_ba, phy_dqm;
  logic [15:0] phy_dq_o, phy_dq_i;

  zhao_shell_top u_shell (
    .gpu_clk, .vid_clk, .audio_clk, .rst_n,
    .hps_state_i, .hps_byte_len_i,
    .ring_wr_valid_o, .ring_wr_slot_o, .ring_wr_state_o, .ring_wr_ready_i,
    .hps_req_valid_o, .hps_req_write_o, .hps_req_addr_o, .hps_req_len_o,
    .hps_req_grant_i,
    .hps_wr_valid_o, .hps_wr_data_o, .hps_wr_last_o,
    .hps_rd_valid_i, .hps_rd_data_i, .hps_rd_last_i,
    .pad_present_i, .pad_buttons_i, .pad_lx_i, .pad_ly_i, .pad_rx_i, .pad_ry_i,
    .aud_wr_valid_i, .aud_wr_l_i, .aud_wr_r_i,
    .aud_wr_ready_o, .aud_refill_req_o, .aud_occupancy_o,
    .pcm_valid_o, .pcm_l_o, .pcm_r_o, .underrun_status_o, .audio_underruns_o,
    .px_valid_o, .px_rgb_o, .px_x_o, .px_y_o,
    .px_hsync_o, .px_vsync_o, .px_hblank_o, .px_vblank_o, .scaler_violation_o,
    .crc_frame_o, .crc_valid_o, .crc_bytes_o, .crc_size_err_o,
    .gpu_tick_o, .gpu_tick_frame_id_o, .gpu_tick_repeated_o,
    .gpu_complete_slot_o,
    .deadline_faults_o, .frame_cycles_o,
    .slot_state_o, .fence_valid_o, .fence_slot_o, .fence_ok_o, .fence_status_o,
    .mode_act_o, .dma_done_o, .dma_status_o, .blit_done_o, .blit_status_o,
    .pad_frame_flat_o, .pad_sequence_o, .input_gaps_o,
    .rumble_duty_o, .rumble_active_o, .rumble_pwm_o, .rumble_drops_o,
    .cnt_snap_ready_i, .cnt_snap_valid_o, .cnt_snap_id_o, .cnt_snap_value_o,
    .cnt_window_open_o, .cnt_cat_violation_o,
    .guard_violations_o, .starvation_o,
    .init_done_o, .refresh_stalls_o, .bank_conflicts_o, .scanout_preempted_o,
    .hps_err_count_o,
    .shell_err_wfifo_o, .shell_err_route_o, .shell_err_cdc_o,
    .shell_err_framer_o,
    .phy_cs_n_o (phy_cs_n),
    .phy_ras_n_o(phy_ras_n),
    .phy_cas_n_o(phy_cas_n),
    .phy_we_n_o (phy_we_n),
    .phy_a_o    (phy_a),
    .phy_ba_o   (phy_ba),
    .phy_dq_o   (phy_dq_o),
    .phy_dq_oe_o(phy_dq_oe),
    .phy_dqm_o  (phy_dqm),
    // ---- RENDER, tied off ------------------------------------------------
    // This bench drives the shell CMD/MEM/VIDEO path and does not draw. The
    // render port is held quiet rather than left dangling, so a future bench
    // that DOES draw has to name every pin it uses instead of inheriting an X.
    .render_frame_begin_i(1'b0), .render_frame_end_i(1'b0),
    .render_grid_w_i(6'd0), .render_grid_h_i(6'd0),
    .render_tri_valid_i(1'b0), .render_tri_ready_o(),
    .render_kx0_i(23'sd0), .render_ky0_i(23'sd0), .render_kc0_i(48'sd0),
    .render_kx1_i(23'sd0), .render_ky1_i(23'sd0), .render_kc1_i(48'sd0),
    .render_kx2_i(23'sd0), .render_ky2_i(23'sd0), .render_kc2_i(48'sd0),
    .render_tl_i(3'd0),
    .render_ax_i(21'sd0), .render_ay_i(21'sd0),
    .render_bx_i(21'sd0), .render_by_i(21'sd0),
    .render_cx_i(21'sd0), .render_cy_i(21'sd0),
    .render_min_x_i(12'sd0), .render_max_x_i(12'sd0),
    .render_min_y_i(12'sd0), .render_max_y_i(12'sd0),
    .render_src_id_i(16'd0),
    .render_fill_word_i(64'd0), .render_clear_word_i(64'd0),
    .render_state_i(32'd0), .render_src_a_i(8'd0),
    .render_texel_rgb_i(24'd0), .render_texel_a_i(8'd0), .render_texel_idx_i(8'd0),
    .render_fb_base_i(27'd0), .render_fb_stride_i(16'd0),
    // The lease names the blit, which is what this bench drives.
    .fb_writer_i(1'b0),
    .render_drain_done_o(), .render_busy_o(),
    .render_pixels_o(), .render_bursts_o(),
    .render_stream_error_o(), .render_overflow_o(), .render_fragment_error_o(),

    .phy_dq_i   (phy_dq_i)
  );

  zhao_sdram_model u_model (
    .clk (gpu_clk),
    .phy_cs_n, .phy_ras_n, .phy_cas_n, .phy_we_n,
    .phy_a, .phy_ba,
    .phy_dq_o, .phy_dq_oe, .phy_dqm, .phy_dq_i,
    .peek_en, .peek_waddr, .peek_data,
    .err_trcd (model_err_kind[0]), .err_trp (model_err_kind[1]),
    .err_trc (model_err_kind[2]),
    .err_refresh_interval (model_err_kind[3]), .err_protocol (model_err_kind[4]),
    .err_mrs (model_err_kind[5]),
    .model_error
  );

endmodule : tb_zhao_shell
