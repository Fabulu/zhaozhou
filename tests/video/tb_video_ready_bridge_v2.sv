// tb_video_ready_bridge_v2.sv -- scalar Verilator wrapper for Packet-H bridge.
`default_nettype none

// Reset-release witnesses intentionally expose nets that also asynchronously
// reset the bridge state banks.
/* verilator lint_off SYNCASYNCNET */
module tb_video_ready_bridge_v2
  import zhao_pkg::*;
(
    input  logic        gpu_clk,
    input  logic        gpu_rst_n,
    input  logic        vid_clk,
    input  logic        vid_rst_n,
    input  logic        gpu_barrier_done_i,
    input  logic        vid_barrier_done_i,
    input  logic        blank_cmd_i,
    output logic        blank_ack_o,
    output logic        lease_open_o,
    input  logic        cdc_ready_valid_i,
    output logic        cdc_ready_ready_o,
    input  logic [83:0] cdc_ready_tuple_i,
    output logic        cdc_swap_valid_o,
    input  logic        cdc_swap_ready_i,
    output logic [83:0] cdc_swap_tuple_o,
    output logic [1:0]  frame_slot_ready_o,
    input  logic        frame_swap_valid_i,
    input  logic        frame_swap_slot_i,
    input  logic        scanout_ack_i,

    input  logic        scanout_valid_i,
    input  logic [15:0] scanout_rgb_i,
    input  logic [9:0]  scanout_x_i,
    input  logic [7:0]  scanout_y_i,
    input  logic        scanout_hsync_i,
    input  logic        scanout_vsync_i,
    input  logic        scanout_hblank_i,
    input  logic        scanout_vblank_i,
    output logic        output_valid_o,
    output logic [15:0] output_rgb_o,
    output logic [9:0]  output_x_o,
    output logic [7:0]  output_y_o,
    output logic        output_hsync_o,
    output logic        output_vsync_o,
    output logic        output_hblank_o,
    output logic        output_vblank_o,

    output logic        gpu_reset_released_o,
    output logic        vid_reset_released_o,
    output logic        pending_o,
    output logic [83:0] pending_tuple_o,
    output logic        echo_hold_o,
    output logic        blank_active_o,
    output logic        scanout_wait_o,
    output logic        unblank_candidate_o,
    output logic [83:0] unblank_tuple_o,
    output logic        unblank_echo_seen_o,
    output logic        unblank_scanout_seen_o
);

  zhao_px_stream_t scanout_px_w;
  zhao_px_stream_t output_px_w;

  always_comb begin
    scanout_px_w = '0;
    scanout_px_w.valid = scanout_valid_i;
    scanout_px_w.rgb565 = scanout_rgb_i;
    scanout_px_w.x = scanout_x_i;
    scanout_px_w.y = scanout_y_i;
    scanout_px_w.hsync = scanout_hsync_i;
    scanout_px_w.vsync = scanout_vsync_i;
    scanout_px_w.hblank = scanout_hblank_i;
    scanout_px_w.vblank = scanout_vblank_i;
  end

  assign output_valid_o = output_px_w.valid;
  assign output_rgb_o = output_px_w.rgb565;
  assign output_x_o = output_px_w.x;
  assign output_y_o = output_px_w.y;
  assign output_hsync_o = output_px_w.hsync;
  assign output_vsync_o = output_px_w.vsync;
  assign output_hblank_o = output_px_w.hblank;
  assign output_vblank_o = output_px_w.vblank;

  zhao_video_ready_bridge_v2 u_dut (
      .gpu_clk(gpu_clk),
      .gpu_rst_n(gpu_rst_n),
      .vid_clk(vid_clk),
      .vid_rst_n(vid_rst_n),
      .gpu_barrier_done_i(gpu_barrier_done_i),
      .vid_barrier_done_i(vid_barrier_done_i),
      .blank_cmd_i(blank_cmd_i),
      .blank_ack_o(blank_ack_o),
      .lease_open_o(lease_open_o),
      .cdc_ready_valid_i(cdc_ready_valid_i),
      .cdc_ready_ready_o(cdc_ready_ready_o),
      .cdc_ready_tuple_i(cdc_ready_tuple_i),
      .cdc_swap_valid_o(cdc_swap_valid_o),
      .cdc_swap_ready_i(cdc_swap_ready_i),
      .cdc_swap_tuple_o(cdc_swap_tuple_o),
      .frame_slot_ready_o(frame_slot_ready_o),
      .frame_swap_valid_i(frame_swap_valid_i),
      .frame_swap_slot_i(frame_swap_slot_i),
      .scanout_ack_i(scanout_ack_i),
      .scanout_px_i(scanout_px_w),
      .output_px_o(output_px_w),
      .gpu_reset_released_o(gpu_reset_released_o),
      .vid_reset_released_o(vid_reset_released_o),
      .pending_o(pending_o),
      .pending_tuple_o(pending_tuple_o),
      .echo_hold_o(echo_hold_o),
      .blank_active_o(blank_active_o),
      .scanout_wait_o(scanout_wait_o),
      .unblank_candidate_o(unblank_candidate_o),
      .unblank_tuple_o(unblank_tuple_o),
      .unblank_echo_seen_o(unblank_echo_seen_o),
      .unblank_scanout_seen_o(unblank_scanout_seen_o));

endmodule : tb_video_ready_bridge_v2
/* verilator lint_on SYNCASYNCNET */

`default_nettype wire
