// tb_packet_g_lease_cdc.sv -- connected Packet-G lease/publication/CDC gate.
`default_nettype none

`ifndef ZHAO_PACKET_G_CONNECTED_READY_VALID
`define ZHAO_PACKET_G_CONNECTED_READY_VALID(clean_ready, terminal_valid, terminal_publish) \
    (clean_ready)
`endif

// Test-only reset fanout intentionally reaches the two production async-reset
// interfaces and the CDC's synchronized-release input.
/* verilator lint_off SYNCASYNCNET */
module tb_packet_g_lease_cdc
  import zhao_pkg::*;
(
    input  logic        gpu_clk,
    input  logic        gpu_rst_n,
    input  logic        vid_clk,
    input  logic        vid_rst_n,

    input  logic        render_req_valid_i,
    output logic        render_req_ready_o,
    input  logic        render_req_slot_i,
    input  logic [1:0]  render_req_mode_i,
    input  logic        blit_req_valid_i,
    output logic        blit_req_ready_o,
    input  logic        blit_req_slot_i,
    input  logic [1:0]  blit_req_mode_i,

    output logic        rsp_valid_o,
    input  logic        rsp_ready_i,
    output logic        rsp_writer_o,
    output logic        rsp_granted_o,
    output logic        rsp_slot_o,
    output logic [15:0] rsp_generation_o,
    output logic [1:0]  rsp_mode_o,
    output logic [31:0] rsp_base_o,
    output logic [31:0] rsp_span_o,

    input  logic        fault_valid_i,
    output logic        fault_ready_o,
    input  logic        fault_writer_i,
    input  logic        fault_slot_i,
    input  logic [15:0] fault_generation_i,
    input  logic        term_valid_i,
    output logic        term_ready_o,
    input  logic        term_writer_i,
    input  logic        term_slot_i,
    input  logic [15:0] term_generation_i,
    input  logic        term_publish_i,
    input  logic        term_fault_i,

    input  logic        frame_start_i,
    input  logic        frame_boundary_i,
    output logic        frame_repeated_o,
    output logic        frame_tick_o,
    output logic        frame_swap_req_o,
    output logic        frame_swap_slot_o,
    output logic [63:0] deadline_faults_o,

    output logic        lease_valid_o,
    output logic        lease_writer_o,
    output logic        lease_slot_o,
    output logic [15:0] lease_generation_o,
    output logic [1:0]  lease_mode_o,
    output logic [31:0] lease_base_o,
    output logic [31:0] lease_span_o,
    output logic        lease_fault_o,
    output logic        displayed_valid_o,
    output logic        displayed_writer_o,
    output logic        displayed_slot_o,
    output logic [15:0] displayed_generation_o,
    output logic [1:0]  displayed_mode_o,
    output logic [31:0] displayed_base_o,
    output logic [31:0] displayed_span_o,
    output logic [1:0]  slot0_state_o,
    output logic [1:0]  slot1_state_o,

    input  logic        render_guard_valid_i,
    input  logic [26:0] render_guard_addr_i,
    input  logic [6:0]  render_guard_len_i,
    output logic        render_guard_ready_o,
    output logic        render_guard_ok_o,
    output logic        render_guard_violation_o,
    output logic        render_guard_arb_valid_o,
    input  logic        blit_guard_valid_i,
    input  logic [26:0] blit_guard_addr_i,
    input  logic [6:0]  blit_guard_len_i,
    output logic        blit_guard_ready_o,
    output logic        blit_guard_ok_o,
    output logic        blit_guard_violation_o,
    output logic        blit_guard_arb_valid_o,
    output logic        guard_arb_payload_observe_o,

    output logic        gpu_barrier_done_o,
    output logic        vid_barrier_done_o,
    output logic        video_pending_o,
    output logic        video_swap_hold_o,
    output logic [31:0] video_ready_captured_o,
    output logic [31:0] video_echoes_o,
    output logic [31:0] manager_publications_o,
    output logic [31:0] manager_releases_o,
    output logic [31:0] manager_ready_events_o,
    output logic [31:0] manager_swaps_o,
    output logic [31:0] manager_stale_events_o,
    output logic [31:0] cdc_ready_enqueued_o,
    output logic [31:0] cdc_ready_dequeued_o,
    output logic [31:0] cdc_swap_enqueued_o,
    output logic [31:0] cdc_swap_dequeued_o
);

  wire pair_rst_n = gpu_rst_n && vid_rst_n;

  logic manager_ready_valid_w, manager_ready_ready_w;
  logic manager_ready_writer_w, manager_ready_slot_w;
  logic [15:0] manager_ready_generation_w;
  logic [1:0] manager_ready_mode_w;
  logic [31:0] manager_ready_base_w, manager_ready_span_w;
  logic manager_swap_valid_w, manager_swap_ready_w;
  logic [83:0] manager_swap_tuple_w;
  logic [1:0] slot_state_w [0:1];

  wire [83:0] manager_ready_tuple_w = {
      manager_ready_writer_w, manager_ready_slot_w,
      manager_ready_generation_w, manager_ready_mode_w,
      manager_ready_base_w, manager_ready_span_w};
  wire seam_ready_valid_w = `ZHAO_PACKET_G_CONNECTED_READY_VALID(
      manager_ready_valid_w, term_valid_i, term_publish_i);

  logic [31:0] unused_manager_requests, unused_manager_responses;
  logic [31:0] unused_manager_grants, unused_manager_refusals;
  logic [31:0] unused_manager_faults, unused_manager_contentions;

  zhao_video_slotmgr_v2 u_slotmgr (
      .clk(gpu_clk), .rst_n(gpu_rst_n),
      .render_req_valid_i(render_req_valid_i),
      .render_req_ready_o(render_req_ready_o),
      .render_req_slot_i(render_req_slot_i),
      .render_req_mode_i(render_req_mode_i),
      .blit_req_valid_i(blit_req_valid_i),
      .blit_req_ready_o(blit_req_ready_o),
      .blit_req_slot_i(blit_req_slot_i),
      .blit_req_mode_i(blit_req_mode_i),
      .rsp_valid_o(rsp_valid_o), .rsp_ready_i(rsp_ready_i),
      .rsp_writer_o(rsp_writer_o), .rsp_granted_o(rsp_granted_o),
      .rsp_slot_o(rsp_slot_o), .rsp_generation_o(rsp_generation_o),
      .rsp_mode_o(rsp_mode_o), .rsp_base_o(rsp_base_o),
      .rsp_span_o(rsp_span_o),
      .lease_valid_o(lease_valid_o), .lease_writer_o(lease_writer_o),
      .lease_slot_o(lease_slot_o),
      .lease_generation_o(lease_generation_o), .lease_mode_o(lease_mode_o),
      .lease_base_o(lease_base_o), .lease_span_o(lease_span_o),
      .lease_fault_o(lease_fault_o),
      .fault_valid_i(fault_valid_i), .fault_ready_o(fault_ready_o),
      .fault_writer_i(fault_writer_i), .fault_slot_i(fault_slot_i),
      .fault_generation_i(fault_generation_i),
      .term_valid_i(term_valid_i), .term_ready_o(term_ready_o),
      .term_writer_i(term_writer_i), .term_slot_i(term_slot_i),
      .term_generation_i(term_generation_i),
      .term_publish_i(term_publish_i), .term_fault_i(term_fault_i),
      .ready_valid_o(manager_ready_valid_w),
      .ready_ready_i(manager_ready_ready_w),
      .ready_writer_o(manager_ready_writer_w),
      .ready_slot_o(manager_ready_slot_w),
      .ready_generation_o(manager_ready_generation_w),
      .ready_mode_o(manager_ready_mode_w), .ready_base_o(manager_ready_base_w),
      .ready_span_o(manager_ready_span_w),
      .swap_valid_i(manager_swap_valid_w), .swap_ready_o(manager_swap_ready_w),
      .swap_writer_i(manager_swap_tuple_w[83]),
      .swap_slot_i(manager_swap_tuple_w[82]),
      .swap_generation_i(manager_swap_tuple_w[81:66]),
      .swap_mode_i(manager_swap_tuple_w[65:64]),
      .swap_base_i(manager_swap_tuple_w[63:32]),
      .swap_span_i(manager_swap_tuple_w[31:0]),
      .displayed_valid_o(displayed_valid_o),
      .displayed_writer_o(displayed_writer_o),
      .displayed_slot_o(displayed_slot_o),
      .displayed_generation_o(displayed_generation_o),
      .displayed_mode_o(displayed_mode_o),
      .displayed_base_o(displayed_base_o),
      .displayed_span_o(displayed_span_o), .slot_state_o(slot_state_w),
      .requests_accepted_o(unused_manager_requests),
      .responses_accepted_o(unused_manager_responses),
      .leases_granted_o(unused_manager_grants),
      .leases_refused_o(unused_manager_refusals),
      .faults_latched_o(unused_manager_faults),
      .publications_o(manager_publications_o),
      .releases_o(manager_releases_o),
      .ready_events_o(manager_ready_events_o), .swaps_o(manager_swaps_o),
      .stale_events_o(manager_stale_events_o),
      .contentions_o(unused_manager_contentions));

  assign slot0_state_o = slot_state_w[0];
  assign slot1_state_o = slot_state_w[1];

  logic cdc_vid_ready_valid_w, cdc_vid_ready_ready_w;
  logic [83:0] cdc_vid_ready_tuple_w;
  logic cdc_vid_swap_valid_w, cdc_vid_swap_ready_w;
  logic [83:0] cdc_vid_swap_tuple_w;
  logic unused_gpu_protocol_fault, unused_vid_protocol_fault;
  logic [2:0] unused_ready_level, unused_swap_level;
  logic unused_gpu_idle, unused_vid_idle;

  zhao_fb_ready_cdc_v2 u_cdc (
      .gpu_clk(gpu_clk), .gpu_rst_n(gpu_rst_n),
      .vid_clk(vid_clk), .vid_rst_n(vid_rst_n),
      .gpu_ready_valid_i(seam_ready_valid_w),
      .gpu_ready_ready_o(manager_ready_ready_w),
      .gpu_ready_tuple_i(manager_ready_tuple_w),
      .vid_ready_valid_o(cdc_vid_ready_valid_w),
      .vid_ready_ready_i(cdc_vid_ready_ready_w),
      .vid_ready_tuple_o(cdc_vid_ready_tuple_w),
      .vid_swap_valid_i(cdc_vid_swap_valid_w),
      .vid_swap_ready_o(cdc_vid_swap_ready_w),
      .vid_swap_tuple_i(cdc_vid_swap_tuple_w),
      .gpu_swap_valid_o(manager_swap_valid_w),
      .gpu_swap_ready_i(manager_swap_ready_w),
      .gpu_swap_tuple_o(manager_swap_tuple_w),
      .gpu_barrier_done_o(gpu_barrier_done_o),
      .vid_barrier_done_o(vid_barrier_done_o),
      .gpu_protocol_fault_o(unused_gpu_protocol_fault),
      .vid_protocol_fault_o(unused_vid_protocol_fault),
      .ready_enqueued_o(cdc_ready_enqueued_o),
      .ready_dequeued_o(cdc_ready_dequeued_o),
      .swap_enqueued_o(cdc_swap_enqueued_o),
      .swap_dequeued_o(cdc_swap_dequeued_o),
      .ready_memory_level_o(unused_ready_level),
      .swap_memory_level_o(unused_swap_level),
      .gpu_idle_o(unused_gpu_idle), .vid_idle_o(unused_vid_idle));

  logic video_pending_q;
  logic [83:0] video_pending_tuple_q;
  logic video_swap_hold_q;
  logic [83:0] video_swap_tuple_q;
  wire video_swap_room_c = !video_swap_hold_q || cdc_vid_swap_ready_w;
  wire [1:0] frame_slot_ready_c =
      (video_pending_q && video_swap_room_c)
          ? (video_pending_tuple_q[82] ? 2'b10 : 2'b01) : 2'b00;
  assign cdc_vid_ready_ready_w = !video_pending_q;
  assign cdc_vid_swap_valid_w = video_swap_hold_q;
  assign cdc_vid_swap_tuple_w = video_swap_tuple_q;
  assign video_pending_o = video_pending_q;
  assign video_swap_hold_o = video_swap_hold_q;

  logic frame_swap_req_w, frame_swap_slot_w;
  logic [31:0] unused_frame_id, unused_deadline_margin;
  logic [63:0] unused_frame_cycles;
  zhao_frame_tick_t unused_gpu_tick;
  logic [0:0] unused_gpu_complete_slot;

  zhao_video_framectl u_framectl (
      .vid_clk(vid_clk), .rst_n(pair_rst_n),
      .x(16'd0), .y(16'd0), .vblank(frame_boundary_i),
      .vswap_dec(frame_boundary_i), .frame_start(frame_start_i),
      .mode(ZHAO_MODE_Z60), .slot_ready(frame_slot_ready_c),
      .deadline_cycles(32'd1000), .swap_req(frame_swap_req_w),
      .swap_slot(frame_swap_slot_w), .swap_ack(frame_swap_req_w),
      .frame_repeated(frame_repeated_o), .frame_tick(frame_tick_o),
      .frame_id(unused_frame_id), .frame_cycles(unused_frame_cycles),
      .deadline_faults(deadline_faults_o),
      .deadline_margin(unused_deadline_margin), .gpu_clk(gpu_clk),
      .gpu_tick(unused_gpu_tick),
      .gpu_complete_slot(unused_gpu_complete_slot));
  assign frame_swap_req_o = frame_swap_req_w;
  assign frame_swap_slot_o = frame_swap_slot_w;

  always_ff @(posedge vid_clk or negedge pair_rst_n) begin
    if (!pair_rst_n) begin
      video_pending_q <= 1'b0;
      video_pending_tuple_q <= '0;
      video_swap_hold_q <= 1'b0;
      video_swap_tuple_q <= '0;
      video_ready_captured_o <= 32'd0;
      video_echoes_o <= 32'd0;
    end else begin
      if (video_swap_hold_q && cdc_vid_swap_ready_w) begin
        video_swap_hold_q <= 1'b0;
        video_echoes_o <= video_echoes_o + 32'd1;
      end
      if (cdc_vid_ready_valid_w && cdc_vid_ready_ready_w) begin
        video_pending_q <= 1'b1;
        video_pending_tuple_q <= cdc_vid_ready_tuple_w;
        video_ready_captured_o <= video_ready_captured_o + 32'd1;
      end
      if (frame_swap_req_w && video_pending_q &&
          (frame_swap_slot_w == video_pending_tuple_q[82]) &&
          video_swap_room_c) begin
        video_pending_q <= 1'b0;
        video_swap_hold_q <= 1'b1;
        video_swap_tuple_q <= video_pending_tuple_q;
      end
    end
  end

  function automatic logic [63:0] request_mask(input logic [6:0] length);
    logic [63:0] value;
    begin
      value = '0;
      for (int index = 0; index < 64; index++)
        if (index < length) value[index] = 1'b1;
      request_mask = value;
    end
  endfunction

  zhao_guard_req_t render_guard_req_w, blit_guard_req_w;
  zhao_guard_rsp_t render_guard_rsp_w, blit_guard_rsp_w;
  zhao_arb_req_t render_arb_req_w, blit_arb_req_w;
  zhao_arb_rsp_t render_arb_rsp_w, blit_arb_rsp_w;
  logic unused_render_guard_pulse, unused_blit_guard_pulse;
  logic [31:0] unused_render_guard_count, unused_blit_guard_count;
  zhao_guard_req_t unused_render_guard_trace, unused_blit_guard_trace;

  always_comb begin
    render_guard_req_w = '0;
    render_guard_req_w.valid = render_guard_valid_i;
    render_guard_req_w.write = 1'b1;
    render_guard_req_w.client = ZHAO_CLIENT_ENGINE0;
    render_guard_req_w.addr = render_guard_addr_i;
    render_guard_req_w.len = render_guard_len_i;
    render_guard_req_w.be = request_mask(render_guard_len_i);
    blit_guard_req_w = '0;
    blit_guard_req_w.valid = blit_guard_valid_i;
    blit_guard_req_w.write = 1'b1;
    blit_guard_req_w.client = ZHAO_CLIENT_BLIT_DMA;
    blit_guard_req_w.addr = blit_guard_addr_i;
    blit_guard_req_w.len = blit_guard_len_i;
    blit_guard_req_w.be = request_mask(blit_guard_len_i);
    render_arb_rsp_w = '0;
    render_arb_rsp_w.grant = 1'b1;
    blit_arb_rsp_w = '0;
    blit_arb_rsp_w.grant = 1'b1;
  end

  zhao_mem_guard u_render_guard (
      .clk(gpu_clk), .rst_n(gpu_rst_n), .req(render_guard_req_w),
      .rsp(render_guard_rsp_w), .map_valid(lease_valid_o),
      .blit_slot(lease_slot_o), .blit_span(lease_span_o),
      .fb_writer(lease_writer_o), .arb_req(render_arb_req_w),
      .res_valid  (1'b0),   // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .res_base   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .res_span   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .arb_rsp(render_arb_rsp_w),
      .guard_violation(unused_render_guard_pulse),
      .guard_violations(unused_render_guard_count),
      .guard_violation_req(unused_render_guard_trace));
  zhao_mem_guard u_blit_guard (
      .clk(gpu_clk), .rst_n(gpu_rst_n), .req(blit_guard_req_w),
      .rsp(blit_guard_rsp_w), .map_valid(lease_valid_o),
      .blit_slot(lease_slot_o), .blit_span(lease_span_o),
      .fb_writer(lease_writer_o), .arb_req(blit_arb_req_w),
      .res_valid  (1'b0),   // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .res_base   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .res_span   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .arb_rsp(blit_arb_rsp_w),
      .guard_violation(unused_blit_guard_pulse),
      .guard_violations(unused_blit_guard_count),
      .guard_violation_req(unused_blit_guard_trace));

  assign render_guard_ready_o = render_guard_rsp_w.ready;
  assign render_guard_ok_o = render_guard_rsp_w.ok;
  assign render_guard_violation_o = render_guard_rsp_w.violation;
  assign render_guard_arb_valid_o = render_arb_req_w.valid;
  assign blit_guard_ready_o = blit_guard_rsp_w.ready;
  assign blit_guard_ok_o = blit_guard_rsp_w.ok;
  assign blit_guard_violation_o = blit_guard_rsp_w.violation;
  assign blit_guard_arb_valid_o = blit_arb_req_w.valid;
  assign guard_arb_payload_observe_o = ^render_arb_req_w ^ ^blit_arb_req_w;

endmodule : tb_packet_g_lease_cdc
/* verilator lint_on SYNCASYNCNET */

`undef ZHAO_PACKET_G_CONNECTED_READY_VALID
`default_nettype wire
