// tb_video_terminal_adapter_v2.sv -- scalar harness for Packet-H terminal join.
`default_nettype none

module tb_video_terminal_adapter_v2 (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        blit_publish_valid_i,
    input  logic        blit_publish_slot_i,
    input  logic [15:0] blit_publish_generation_i,
    input  logic        blit_release_valid_i,
    input  logic        blit_release_slot_i,
    input  logic [15:0] blit_release_generation_i,
    output logic        blit_refused_o,
    output logic [31:0] blit_events_refused_o,
    input  logic        renderer_term_valid_i,
    output logic        renderer_term_ready_o,
    input  logic        renderer_term_slot_i,
    input  logic [15:0] renderer_term_generation_i,
    input  logic        renderer_term_publish_i,
    input  logic        renderer_term_fault_i,
    output logic        term_valid_o,
    input  logic        term_ready_i,
    output logic        term_writer_o,
    output logic        term_slot_o,
    output logic [15:0] term_generation_o,
    output logic        term_publish_o,
    output logic        term_fault_o,
    output logic        occupied_o,
    output logic        idle_o,
    output logic [31:0] source_events_captured_o,
    output logic [31:0] manager_terms_accepted_o
);

  zhao_video_terminal_adapter_v2 u_dut (.*);

endmodule : tb_video_terminal_adapter_v2

`default_nettype wire
