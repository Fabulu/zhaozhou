// tb_post_lease_top.sv -- the post lease exactly as the shell composes it, and
// nothing else: `zhao_post_lease` with the REAL `zhao_raster_fbwrite` on its
// requester-0 socket and on its post-phase pixel port, wired the way
// `zhao_shell_top_v2` wires them (FBWRITE's retirement comes THROUGH the lease;
// its `drained_o` is the lease's `fbw_drained_i`).
//
// Bench-only. Driven by tests/compositor/post_lease_directed.cpp, which plays
// the compositor on one side and an IN-ORDER memory system (guard, arbiter and
// controller, with credits for reads and writes) on the other.
//
// There is no raster here: the bench is about the post phase, so the raster's
// pixel port is idle and `raster_quiet_i` is the bench's to drive.
`default_nettype none

module tb_post_lease_top
  import zhao_pkg::*;
(
    input  var logic clk,
    input  var logic rst_n,

    input  var logic          lease_live_i,
    input  var logic          frame_admit_i,
    input  var logic          frame_end_i,
    input  var logic          raster_quiet_i,
    input  var logic [ZHAO_VRAM_ADDR_BITS-1:0] fb_base_i,
    input  var logic [15:0]   fb_stride_i,
    input  var logic [8:0]    frame_w_i,
    input  var logic [7:0]    frame_h_i,
    input  var logic          duo_i,

    output var logic          pass_start_o,
    output var logic          view_o,
    output var logic          src_valid_o,
    input  var logic          src_ready_i,
    output var logic [15:0]   src_rgb_o,
    input  var logic          out_valid_i,
    output var logic          out_ready_o,
    input  var logic [15:0]   out_rgb_i,
    input  var logic [8:0]    out_x_i,
    input  var logic [7:0]    out_y_i,
    input  var logic          out_last_i,
    input  var logic          echo_valid_i,
    input  var logic [15:0]   echo_rgb_i,

    output var logic          phase_post_o,

    output var zhao_guard_req_t e0_req_o,
    input  var zhao_guard_rsp_t e0_rsp_i,
    output var logic [63:0]     e0_wdata_o,
    output var logic            e0_wvalid_o,
    input  var logic            e0_wready_i,
    output var logic            e0_wlast_o,
    input  var logic            e0_beat_valid_i,
    input  var logic [63:0]     e0_beat_data_i,
    input  var logic            e0_beat_last_i,
    input  var logic [7:0]      e0_credits_i,

    output var logic          busy_o,
    output var logic [31:0]   passes_o,
    output var logic [31:0]   frames_o,
    output var logic          fault_o,
    output var logic [31:0]   src_reads_o,
    output var logic [31:0]   src_pixels_o,
    output var logic [31:0]   retire_unowned_o,
    output var logic [31:0]   share_contention_o,
    output var logic [31:0]   echo_passes_complete_o,
    output var logic [31:0]   echo_passes_torn_o,
    output var logic [31:0]   echo_pixels_written_o,
    output var logic [31:0]   echo_pixels_dropped_o,
    output var logic          echo_fault_o,
    output var logic          fbw_drained_o,
    output var logic          fbw_fatal_o,
    output var logic          fbw_stream_err_o,
    output var logic [31:0]   fbw_issued_words_o,
    output var logic [31:0]   fbw_retired_words_o
);

  logic               ppx_valid, ppx_ready, ppx_last;
  logic        [15:0] ppx_rgb;
  logic signed [11:0] ppx_x, ppx_y;

  zhao_guard_req_t fbw_req;
  zhao_guard_rsp_t fbw_rsp;
  logic [63:0]     fbw_wdata;
  logic            fbw_wvalid, fbw_wready, fbw_wlast;
  logic [7:0]      fbw_retire;
  logic [31:0]     fbw_pixels_unused, fbw_bursts_unused, fbw_stall_unused;
  logic            fbw_busy_unused;

  zhao_raster_fbwrite u_fbw (
    .clk(clk), .rst_n(rst_n),
    .fb_base_i(fb_base_i), .fb_stride_i(fb_stride_i),
    .px_valid_i(ppx_valid), .px_ready_o(ppx_ready),
    .px_rgb565_i(ppx_rgb), .px_x_i(ppx_x), .px_y_i(ppx_y), .px_last_i(ppx_last),
    .frame_end_i(frame_end_i),
    .retire_words_i(fbw_retire),
    .guard_req_o(fbw_req), .guard_rsp_i(fbw_rsp),
    .guard_wdata_o(fbw_wdata), .guard_wvalid_o(fbw_wvalid),
    .guard_wready_i(fbw_wready), .guard_wlast_o(fbw_wlast),
    .pixels_written_o(fbw_pixels_unused), .bursts_issued_o(fbw_bursts_unused),
    .stall_clocks_o(fbw_stall_unused), .stream_error_o(fbw_stream_err_o),
    .issued_words_o(fbw_issued_words_o), .retired_words_o(fbw_retired_words_o),
    .drained_o(fbw_drained_o), .fatal_error_o(fbw_fatal_o), .busy_o(fbw_busy_unused)
  );

  zhao_post_lease #(.XW(9), .YW(8)) u_lease (
    .clk(clk), .rst_n(rst_n),
    .lease_live_i(lease_live_i), .frame_admit_i(frame_admit_i), .frame_end_i(frame_end_i),
    .raster_quiet_i(raster_quiet_i), .raster_px_i(1'b0), .fbw_drained_i(fbw_drained_o),
    .fb_base_i(fb_base_i), .fb_stride_i(fb_stride_i),
    .frame_w_i(frame_w_i), .frame_h_i(frame_h_i), .duo_i(duo_i),
    .pass_start_o(pass_start_o), .view_o(view_o),
    .src_valid_o(src_valid_o), .src_ready_i(src_ready_i), .src_rgb_o(src_rgb_o),
    .out_valid_i(out_valid_i), .out_ready_o(out_ready_o), .out_rgb_i(out_rgb_i),
    .out_x_i(out_x_i), .out_y_i(out_y_i), .out_last_i(out_last_i),
    .echo_valid_i(echo_valid_i), .echo_rgb_i(echo_rgb_i),
    .phase_post_o(phase_post_o),
    .fbw_px_valid_o(ppx_valid), .fbw_px_ready_i(ppx_ready), .fbw_px_rgb_o(ppx_rgb),
    .fbw_px_x_o(ppx_x), .fbw_px_y_o(ppx_y), .fbw_px_last_o(ppx_last),
    .fbw_req_i(fbw_req), .fbw_rsp_o(fbw_rsp), .fbw_wdata_i(fbw_wdata),
    .fbw_wvalid_i(fbw_wvalid), .fbw_wready_o(fbw_wready), .fbw_wlast_i(fbw_wlast),
    .fbw_retire_o(fbw_retire),
    .e0_req_o(e0_req_o), .e0_rsp_i(e0_rsp_i),
    .e0_wdata_o(e0_wdata_o), .e0_wvalid_o(e0_wvalid_o), .e0_wready_i(e0_wready_i),
    .e0_wlast_o(e0_wlast_o),
    .e0_beat_valid_i(e0_beat_valid_i), .e0_beat_data_i(e0_beat_data_i),
    .e0_beat_last_i(e0_beat_last_i), .e0_credits_i(e0_credits_i),
    .busy_o(busy_o), .passes_o(passes_o), .frames_o(frames_o), .fault_o(fault_o),
    .src_reads_o(src_reads_o), .src_pixels_o(src_pixels_o),
    .retire_unowned_o(retire_unowned_o), .share_contention_o(share_contention_o),
    .echo_passes_complete_o(echo_passes_complete_o),
    .echo_passes_torn_o(echo_passes_torn_o),
    .echo_pixels_written_o(echo_pixels_written_o),
    .echo_pixels_dropped_o(echo_pixels_dropped_o),
    .echo_fault_o(echo_fault_o)
  );

  logic unused_c;
  assign unused_c = ^{fbw_pixels_unused, fbw_bursts_unused, fbw_stall_unused, fbw_busy_unused};

endmodule : tb_post_lease_top

`default_nettype wire
