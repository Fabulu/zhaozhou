// tb_renderer_lease_v2.sv -- scalar-port harness for Packet-H H2/H3.
`default_nettype none

module tb_renderer_lease_v2 (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        lease_open_i,

    input  logic        frame_req_valid_i,
    output logic        frame_req_ready_o,
    input  logic [1:0]  frame_req_mode_i,

    input  logic        lease_valid_i,
    input  logic [1:0]  slot0_state_i,
    input  logic [1:0]  slot1_state_i,

    output logic        render_req_valid_o,
    input  logic        render_req_ready_i,
    output logic        render_req_slot_o,
    output logic [1:0]  render_req_mode_o,

    input  logic        rsp_valid_i,
    output logic        rsp_ready_o,
    input  logic        rsp_writer_i,
    input  logic        rsp_granted_i,
    input  logic        rsp_slot_i,
    input  logic [15:0] rsp_generation_i,
    input  logic [1:0]  rsp_mode_i,
    input  logic [31:0] rsp_base_i,
    input  logic [31:0] rsp_span_i,

    output logic        frame_fault_clear_valid_o,
    input  logic        frame_fault_clear_ready_i,

    output logic        frame_valid_o,
    input  logic        frame_ready_i,
    output logic        frame_writer_o,
    output logic        frame_slot_o,
    output logic [15:0] frame_generation_o,
    output logic [1:0]  frame_mode_o,
    output logic [31:0] frame_base_o,
    output logic [31:0] frame_span_o,
    output logic [15:0] frame_width_o,
    output logic [15:0] frame_height_o,
    output logic [15:0] frame_stride_o,
    output logic [15:0] frame_view1_y_o,
    output logic [31:0] frame_view1_offset_o
);

  logic [1:0] slot_state_w [0:1];
  assign slot_state_w[0] = slot0_state_i;
  assign slot_state_w[1] = slot1_state_i;

  zhao_renderer_lease_v2 u_dut (
      .clk(clk),
      .rst_n(rst_n),
      .lease_open_i(lease_open_i),
      .frame_req_valid_i(frame_req_valid_i),
      .frame_req_ready_o(frame_req_ready_o),
      .frame_req_mode_i(frame_req_mode_i),
      .lease_valid_i(lease_valid_i),
      .slot_state_i(slot_state_w),
      .render_req_valid_o(render_req_valid_o),
      .render_req_ready_i(render_req_ready_i),
      .render_req_slot_o(render_req_slot_o),
      .render_req_mode_o(render_req_mode_o),
      .rsp_valid_i(rsp_valid_i),
      .rsp_ready_o(rsp_ready_o),
      .rsp_writer_i(rsp_writer_i),
      .rsp_granted_i(rsp_granted_i),
      .rsp_slot_i(rsp_slot_i),
      .rsp_generation_i(rsp_generation_i),
      .rsp_mode_i(rsp_mode_i),
      .rsp_base_i(rsp_base_i),
      .rsp_span_i(rsp_span_i),
      .frame_fault_clear_valid_o(frame_fault_clear_valid_o),
      .frame_fault_clear_ready_i(frame_fault_clear_ready_i),
      .frame_valid_o(frame_valid_o),
      .frame_ready_i(frame_ready_i),
      .frame_writer_o(frame_writer_o),
      .frame_slot_o(frame_slot_o),
      .frame_generation_o(frame_generation_o),
      .frame_mode_o(frame_mode_o),
      .frame_base_o(frame_base_o),
      .frame_span_o(frame_span_o),
      .frame_width_o(frame_width_o),
      .frame_height_o(frame_height_o),
      .frame_stride_o(frame_stride_o),
      .frame_view1_y_o(frame_view1_y_o),
      .frame_view1_offset_o(frame_view1_offset_o));

endmodule : tb_renderer_lease_v2

`default_nettype wire
