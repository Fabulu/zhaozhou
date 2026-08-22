// zhao_hps_bridge.sv — functional HPS-DDR burst bridge (plan W2.5, D10).
// Law: spec/memory_rules.md §3-§4; contract design/contracts/MEM.HPS.BRIDGE.md.
//
// The FPGA-side core is lane-portable: a generic burst request/response
// engine against the harness (in Verilator the C++ harness IS the HPS and
// answers bursts with the frozen sim latency profile — 16 gpu cycles to the
// first beat, 1 beat/cycle after) or the framework-AXI adapter (the hardware
// seam; no AXI RTL in Phase 2). The bridge itself adds exactly one register
// stage per direction (deterministic, mirrored by zref::HpsBridge).
//
//   * Bursts are 64-B aligned, 1..64 bytes, read or write; beats are 64-bit.
//   * One burst in flight per client (the busy law; a request from a busy
//     client is a protocol violation: answered with err, nothing issued).
//   * Malformed bursts (len 0 / len > 64 / misaligned address) are rejected
//     at the port with a single err|last response pulse, counted, and
//     NOTHING is issued to the HPS side — never a wild DRAM access.
//   * hps_ddr_bytes_by_client += len (both directions) at burst completion;
//     shadows latch on frame_tick (D9).
//
// Write data: the client streams wr_valid/wr_data beats after its request
// is accepted (rsp.grant); the bridge forwards them registered to the HPS
// side; wr_last terminates. Read data: hps_rd beats are forwarded registered
// to the client port rsp (beat_valid/data/last). The client response stream
// carries no backpressure in Phase 2 (clients accept at 1 beat/cycle).
//
// Conservative SystemVerilog subset only (charter §2). Lint: clean under
// `verilator --lint-only -Wall` (lint_mem_hps_bridge CTest).

module zhao_hps_bridge
  import zhao_pkg::*;
(
  input  logic clk,
  input  logic rst_n,

  // client burst port
  input  zhao_hps_burst_req_t req,
  output logic                req_grant,   // registered accept pulse
  input  logic                wr_valid,    // write beats (after grant)
  input  logic [63:0]         wr_data,
  input  logic                wr_last,
  output zhao_hps_burst_rsp_t rsp,         // read beats / err (registered)

  // HPS side (harness in sim; framework adapter seam in hardware)
  output logic        hps_req_valid,
  output logic        hps_req_write,
  output logic [31:0] hps_req_addr,
  output logic [6:0]  hps_req_len,
  input  logic        hps_req_grant,
  output logic        hps_wr_valid,
  output logic [63:0] hps_wr_data,
  output logic        hps_wr_last,
  input  logic        hps_rd_valid,
  input  logic [63:0] hps_rd_data,
  input  logic        hps_rd_last,

  // counters (D9) and status
  input  logic             frame_tick,
  output logic [4:0][31:0] hps_bytes,
  output logic [4:0][31:0] hps_bytes_shadow,
  output logic [31:0]      hps_err_count
);

  // one burst in flight (single client port in Phase 2; the per-client busy
  // law reduces to one bridge-wide burst here — client ids are carried for
  // byte accounting only)
  logic        busy;
  logic        busy_write;
  logic [2:0]  busy_client;
  logic [6:0]  busy_len;
  logic        issued;       // HPS accepted the request

  logic malformed;
  assign malformed = (req.len == 7'd0) || (req.len > 7'd64) || (req.addr[5:0] != 6'd0);

  logic accept;
  assign accept = req.valid && !busy && !malformed;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy             <= 1'b0;
      busy_write       <= 1'b0;
      busy_client      <= 3'd0;
      busy_len         <= 7'd0;
      issued           <= 1'b0;
      req_grant        <= 1'b0;
      hps_req_valid    <= 1'b0;
      hps_req_write    <= 1'b0;
      hps_req_addr     <= 32'd0;
      hps_req_len      <= 7'd0;
      hps_wr_valid     <= 1'b0;
      hps_wr_data      <= 64'd0;
      hps_wr_last      <= 1'b0;
      rsp              <= '0;
      hps_bytes        <= '0;
      hps_bytes_shadow <= '0;
      hps_err_count    <= 32'd0;
    end else begin
      // defaults: one-cycle pulses / pass-through registers
      req_grant    <= 1'b0;
      rsp.beat_valid <= 1'b0;
      rsp.err      <= 1'b0;
      rsp.last     <= 1'b0;
      hps_wr_valid <= 1'b0;
      hps_wr_last  <= 1'b0;

      if (frame_tick) hps_bytes_shadow <= hps_bytes;

      // ---- malformed / protocol-violation answers ------------------------
      if (req.valid && (malformed || busy)) begin
        rsp.err  <= 1'b1;
        rsp.last <= 1'b1;      // single-pulse err response, nothing issued
        hps_err_count <= hps_err_count + 32'd1;
      end

      // ---- request acceptance --------------------------------------------
      if (accept) begin
        req_grant      <= 1'b1;
        busy           <= 1'b1;
        busy_write     <= req.write;
        busy_client    <= req.client;
        busy_len       <= req.len;
        issued         <= 1'b0;
        hps_req_valid  <= 1'b1;
        hps_req_write  <= req.write;
        hps_req_addr   <= req.addr;
        hps_req_len    <= req.len;
      end else if (hps_req_valid && hps_req_grant) begin
        hps_req_valid <= 1'b0;
        issued        <= 1'b1;
      end

      // ---- write beats: client -> HPS (registered) -----------------------
      if (busy && busy_write && issued && wr_valid) begin
        hps_wr_valid <= 1'b1;
        hps_wr_data  <= wr_data;
        hps_wr_last  <= wr_last;
        if (wr_last) begin
          busy <= 1'b0;   // burst complete: count and free the port
          // ENFORCED-BY: tests/formal/sat_add.sby
          hps_bytes[busy_client] <=
            zhao_pkg::zhao_sat_add32(hps_bytes[busy_client], {25'b0, busy_len});
        end
      end

      // ---- read beats: HPS -> client (registered) ------------------------
      if (busy && !busy_write && issued && hps_rd_valid) begin
        rsp.beat_valid <= 1'b1;
        rsp.data       <= hps_rd_data;
        rsp.last       <= hps_rd_last;
        if (hps_rd_last) begin
          busy <= 1'b0;
          // ENFORCED-BY: tests/formal/sat_add.sby
          hps_bytes[busy_client] <=
            zhao_pkg::zhao_sat_add32(hps_bytes[busy_client], {25'b0, busy_len});
        end
      end
    end
  end

endmodule
