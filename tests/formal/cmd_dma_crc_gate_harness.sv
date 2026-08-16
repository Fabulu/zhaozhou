// cmd_dma_crc_gate_harness.sv — formal harness for tests/formal/
// cmd_dma_crc_gate.sby (plan W2.6 / plan 4). Testbench component, NEVER
// synthesis or the Verilator ctests.
//
// The harness instantiates the REAL zhao_cmd_dma with SHRUNK staging
// buffers (40 B slot / 64 B blit — a minimal header-only packet: the
// staging bound forces command_bytes = 0, the smallest packet the ABI
// allows) and FORMAL_BLIT_LEN = 64 so the blit path — and with it the
// blit CRC gate of assertion (b) — is genuinely in the cone (see the
// parameter note at the instantiation; the committed 4096/245,760
// defaults and the full-size blit-length law are exercised by the ctest
// lane cmd_dma_directed). Every DUT input is a free formal input —
// in particular the bridge response beats are ARBITRARY, which is exactly
// the adversarial model for the gate: can ANY beat pattern push a byte
// downstream before the CRC gates open?
//
// The properties themselves live in the DUT under `ifdef FORMAL (read
// with -DFORMAL):
//   (a) pkt_valid_o implies hdr_gate && pay_gate (no verified-packet byte
//       before BOTH CRC checks passed)
//   (b) guard_req_o.valid implies blit_gate (no VRAM write before the blit
//       payload CRC passed)
//   (c) reset leaves no partial handoff

module zhao_cmd_dma_crc_gate_harness (
  input  logic clk,
  input  logic rst_n,

  input  logic        fetch_req_valid_i,
  input  logic [1:0]  fetch_slot_i,
  input  logic [31:0] fetch_addr_i,
  input  logic [31:0] fetch_byte_len_i,
  input  logic [31:0] fetch_epoch_i,

  input  zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,

  input  logic pkt_ready_i,

  input  logic        blit_req_valid_i,
  input  logic [7:0]  blit_dst_slot_i,
  input  logic [7:0]  blit_mode_i,
  input  logic [31:0] blit_src_i,
  input  logic [31:0] blit_len_i,
  input  logic [31:0] blit_crc_i,

  input  zhao_pkg::zhao_guard_rsp_t guard_rsp_i,

  input  zhao_pkg::zhao_frame_tick_t frame_tick_i
);

  zhao_cmd_dma #(
    .SLOT_BUF_BYTES (40),
    .BLIT_BUF_BYTES (64),
    // FORMAL-ONLY override of the blit-length law (byte_len ==
    // canvas_bytes(mode)): the smallest lawful canvas is 153,600 B, which
    // no tractable BMC depth can fetch — without this the blit CRC gate
    // NEVER opened in the formal cone and assertion (b) was vacuous (the
    // original header's claim that both gates were in the cone was wrong).
    // The full-size length law is exercised by the ctest lane instead.
    .FORMAL_BLIT_LEN (64)
  ) dut (
    .clk               (clk),
    .rst_n             (rst_n),
    .fetch_req_valid_i (fetch_req_valid_i),
    .fetch_req_ready_o (),
    .fetch_slot_i      (fetch_slot_i),
    .fetch_addr_i      (fetch_addr_i),
    .fetch_byte_len_i  (fetch_byte_len_i),
    .fetch_epoch_i     (fetch_epoch_i),
    .dma_done_o        (),
    .dma_slot_o        (),
    .dma_status_o      (),
    .dma_bytes_consumed_o (),
    .dma_cmds_consumed_o (),
    .hps_req_o         (),
    .hps_rsp_i         (hps_rsp_i),
    .pkt_valid_o       (),
    .pkt_ready_i       (pkt_ready_i),
    .pkt_byte_o        (),
    .pkt_len_o         (),
    .blit_req_valid_i  (blit_req_valid_i),
    .blit_req_ready_o  (),
    .blit_dst_slot_i   (blit_dst_slot_i),
    .blit_mode_i       (blit_mode_i),
    .blit_src_i        (blit_src_i),
    .blit_len_i        (blit_len_i),
    .blit_crc_i        (blit_crc_i),
    .blit_done_o       (),
    .blit_status_o     (),
    .guard_req_o       (),
    .guard_rsp_i       (guard_rsp_i),
    .guard_wdata_o     (),
    .frame_tick_i      (frame_tick_i),
    .snap_cmds_o       (),
    .snap_bytes_o      (),
    .snap_drops_o      ()
  );

endmodule : zhao_cmd_dma_crc_gate_harness
