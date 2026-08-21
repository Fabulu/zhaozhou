// cmd_dma_crc_gate_harness.sv — formal harness for tests/formal/
// cmd_dma_crc_gate.sby (plan W2.6 / plan 4). Testbench component, NEVER
// synthesis or the Verilator ctests.
//
// The harness instantiates the REAL zhao_cmd_dma with a SHRUNK staging
// buffer (40 B slot — a minimal header-only packet: the staging bound
// forces command_bytes = 0, the smallest packet the ABI allows; the
// committed 4096 default is exercised by the ctest lane cmd_dma_directed).
// Every DUT input is a free formal input — in particular the bridge
// response beats are ARBITRARY, which is exactly the adversarial model for
// the gate: can ANY beat pattern push a byte downstream before the CRC
// gates open?
//
// The properties themselves live in the DUT under `ifdef FORMAL (read
// with -DFORMAL):
//   (a) pkt_valid_o implies hdr_gate && pay_gate (no verified-packet byte
//       before BOTH CRC checks passed)
//   (c) reset leaves no partial handoff
//
// PROPERTY (b) IS GONE, with the blit engine it guarded (step 6). It said
// no VRAM write is offered before the blit payload CRC passed; this module
// no longer has a MEM.GUARD client to offer one. The law moved to
// DEBUG.FRAMEBLIT and tests/formal/debug_frameblit_safety.sby.
//
// Recorded because it is the opposite of reassuring: (b) was VACUOUS until
// this harness gained FORMAL_BLIT_LEN, since the smallest lawful canvas is
// 153,600 B and no tractable BMC depth could open the gate. The property
// that had to be rescued from vacuity is the one now deleted.

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

  input  zhao_pkg::zhao_frame_tick_t frame_tick_i
);

  // The blit engine and its BLIT_BUF_BYTES / FORMAL_BLIT_LEN parameters were
  // removed with step 6; the blit is DEBUG.FRAMEBLIT's, and its own gate is
  // tests/formal/debug_frameblit_safety.sby. Only the slot shrink remains.
  zhao_cmd_dma #(
    .SLOT_BUF_BYTES (40)
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
    .frame_tick_i      (frame_tick_i),
    .snap_cmds_o       (),
    .snap_bytes_o      (),
    .snap_drops_o      ()
  );

  // ---- SELF-ASSERTING SCOPE GUARD (ledger rule V19; the arbiter
  // a_horizon_is_refresh_free / linebuf a_scope_four_sessions pattern) ----
  // The gate assertions are proven at bmc depth 24 UNDER THE STAGING
  // SHRINK above (40-B slot => header-only packets with command_bytes = 0;
  // header-only packets). That fact scopes the depth: 24 steps suffice
  // BECAUSE the packets are minimal. Raising `depth` does NOT extend the
  // proof to multi-record packets
  // lengths — the buffer parameters have to be re-derived together with
  // the depth — so this guard PINS the proven window and FIRES the moment
  // the depth is raised, forcing that re-derivation instead of a silent
  // re-scope of what "PASS" means.
  logic [5:0] f_steps = 6'd0;
  always_ff @(posedge clk) begin
    if (f_steps != 6'h3F) f_steps <= f_steps + 6'd1;
  end
  always_comb begin
    a_scope_header_only_window : assert (f_steps <= 6'd24);
  end

endmodule : zhao_cmd_dma_crc_gate_harness
