// zhao_cmd_dma.sv — CMD.DMA: sealed-frame-packet fetch + the debug-blit
// DMA engine (plan W2.6, decisions D8/D10).
//
// Law (in citation order):
//   design/contracts/CMD.DMA.md  — the block contract (this file matures
//       the stub to real fetch: CRC gate BEFORE the first byte, payload CRC
//       at the end, epoch check, fail-safe drop order)
//   spec/capture_format.md 3/3.2 — the frame packet layout, the fail-safe
//       validation order and the bytes_consumed law (36 on header-level
//       abort, else 40+N). Checks 6 (pad-zero) / 7 (enum range) / 8
//       (handles) are RECORD-SEMANTIC checks owned by CMD.DECODER (wave 3);
//       this module owns the packet-level gate: 1,2,3,4,5,9,10 + epoch.
//   spec/memory_rules.md 3/4.1/4.3/5 — bridge burst law (64-B aligned,
//       1..64 B, one in flight per client), the FRAME_RING fetch, the blit
//       source (address/length/expected CRC from the DebugFrameBlit record)
//       and the region law: the blit writes exactly dst_slot's FB region,
//       exactly canvas_bytes(mode) bytes, any other length rejected BEFORE
//       the first byte.
//   spec/counters.md (D9) — commands / hps_ddr_bytes_by_client /
//       deadline_faults (drops) owned here; frame_tick latches shadows.
//
// D10 (harness-as-HPS): the bridge port is the generic burst channel; in
// simulation the harness C++ answers reads with the frozen sim profile
// (16 gpu cycles to first beat, 1 beat/cycle, `last` on the final beat of
// the burst). The RTL is latency-agnostic: it holds the request and counts
// beats until last.
//
// THE GATE (formal cmd_dma_crc_gate): NOTHING is emitted downstream —
// neither a verified packet byte (pkt_valid_o) nor a VRAM write
// (guard_req_o) — before the corresponding header-CRC / blit-CRC check
// passed. The architecture is buffer-then-release: bytes land in an
// internal staging buffer during fetch, are verified in full, and only a
// fully verified packet/stream is released. No partial delivery, ever.
//
// Status space: 0..14 = zhao_abi_error (ABI law); 15+ are MODULE-LOCAL
// extensions shared with zref_cmd2.hpp (never on the wire):
//   15  epoch mismatch (slot epoch != current epoch, dropped before the
//       first payload byte)
//   17  bridge error (rsp.err — malformed burst, nothing issued)
//   18  blit rejected (byte_len != canvas_bytes(mode) or bad dst_slot —
//       rejected before the first byte, zero guard writes)
//
// Client ids (frozen zhao_pkg enum; memory_rules.md 2 maps command DMA to
// the reserved engine RR slot): packet fetch = ZHAO_CLIENT_ENGINE0,
// blit = ZHAO_CLIENT_BLIT_DMA (both feed hps_ddr_bytes_by_client).
//
// VRAM write-data seam (CORRECTED for W2.7 shell composition): the frozen
// zhao_guard_req_t carries the transaction {client, write, addr, len, be}
// but no data lane. As shipped by W2.6 the guard_wdata_o sideband carried
// ONLY the first 8 bytes of each 64-byte write request — the other 56 bytes
// existed in blit_buf and on no wire, and cmd_dma_directed's "bit-exact"
// check only ever compared those first 8 bytes. Composing the real SDRAM
// datapath exposed the gap. The seam is now a BEAT STREAM: after the guard
// accepts a write request (guard_rsp_i.ready with guard_req_o.valid), this
// module streams ceil(len/8) beats of guard_wdata_o, one per cycle, marked
// by guard_wvalid_o — beat k carries bytes [k*8, k*8+8) of the request.
// The shell's write-data queue consumes exactly these beats and feeds the
// SDRAM controller's wr_beat pace. W2.5's guard owns the request-path
// shape; this data seam is recorded in the contract notes.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_cmd_dma).

module zhao_cmd_dma
#(
  // staging sizes. Parameters exist so the FORMAL harness can shrink the
  // buffers for tractability; committed defaults are the Phase-2 law
  // (SLOT_BUF: largest per-frame command packet; BLIT_BUF: the largest
  // canvas, Duo 245,760 B — the M10K mapping is the synthesis lane).
  parameter int unsigned SLOT_BUF_BYTES = 4096,
  parameter int unsigned BLIT_BUF_BYTES = 245760
`ifdef FORMAL
  // FORMAL-ONLY (structurally absent outside `ifdef FORMAL, so synthesis
  // and the Verilator ctest lane always run the law): a nonzero value
  // overrides the blit-length law `byte_len == canvas_bytes(mode)` so the
  // blit path fits a tractable BMC depth. Without it the blit CRC gate is
  // UNREACHABLE in the shrunk formal harness — the smallest lawful canvas
  // is 153,600 B (≈19,200 fetch cycles), so assertion (b) below was
  // vacuous while the harness header claimed both gates were in the cone.
  // The full-size length law itself is exercised by cmd_dma_directed.
  , parameter int unsigned FORMAL_BLIT_LEN = 0
`endif
) (
  input  logic clk,
  input  logic rst_n,

  // ---- packet fetch request (from CMD.SCHEDULER's claim) -------------------
  input  logic        fetch_req_valid_i,
  output logic        fetch_req_ready_o,
  input  logic [1:0]  fetch_slot_i,
  input  logic [31:0] fetch_addr_i,      // HPS byte address of the slot body
  input  logic [31:0] fetch_byte_len_i,  // sealed length from the descriptor
  input  logic [31:0] fetch_epoch_i,     // the CURRENT resource epoch

  // ---- the verdict (one pulse per fetch) -----------------------------------
  output logic        dma_done_o,
  output logic [1:0]  dma_slot_o,
  output logic [7:0]  dma_status_o,      // zhao_abi_error / local 15/17/18
  output logic [31:0] dma_bytes_consumed_o,  // 36 | 40+N (capture_format 3.2)
  output logic [31:0] dma_cmds_consumed_o,   // records walked (on success)

  // ---- HPS bridge port (D10; the harness C++ is the HPS in sim) ------------
  output zhao_pkg::zhao_hps_burst_req_t hps_req_o,
  input  zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,

  // ---- verified packet byte stream (decoder-facing) ------------------------
  output logic        pkt_valid_o,
  input  logic        pkt_ready_i,
  output logic [7:0]  pkt_byte_o,
  output logic [31:0] pkt_len_o,         // verified length (40+N)

  // ---- blit engine (Phase-2 dispatch sink of CMD.SCHEDULER) ----------------
  input  logic        blit_req_valid_i,
  output logic        blit_req_ready_o,
  input  logic [7:0]  blit_dst_slot_i,
  input  logic [7:0]  blit_mode_i,       // ABI video_mode byte
  input  logic [31:0] blit_src_i,        // HPS source address
  input  logic [31:0] blit_len_i,        // must equal canvas_bytes(mode)
  input  logic [31:0] blit_crc_i,        // expected CRC-32C over the payload
  output logic        blit_done_o,       // one pulse per blit request
  output logic [7:0]  blit_status_o,     // 0 = committed to VRAM

  // ---- VRAM commit via MEM.GUARD (client BLIT_DMA) -------------------------
  output zhao_pkg::zhao_guard_req_t guard_req_o,
  /* verilator lint_off UNUSEDSIGNAL */
  input  zhao_pkg::zhao_guard_rsp_t guard_rsp_i,
  /* verilator lint_on UNUSEDSIGNAL */
  // write-data beat stream (see header: the frozen req struct has no data
  // lane; ceil(len/8) beats per accepted write request, one per cycle)
  output logic [63:0] guard_wdata_o,
  output logic        guard_wvalid_o,

  // ---- frame boundary (D9 shadow latch) ------------------------------------
  /* verilator lint_off UNUSEDSIGNAL */
  input  zhao_pkg::zhao_frame_tick_t frame_tick_i,
  /* verilator lint_on UNUSEDSIGNAL */

  // ---- D9 counters: owned here, shadows latched at frame_tick --------------
  output zhao_pkg::zhao_counter_snap_t snap_cmds_o,    // id 2  commands
  output zhao_pkg::zhao_counter_snap_t snap_bytes_o,   // id 29 hps bytes
  output zhao_pkg::zhao_counter_snap_t snap_drops_o    // id 1  deadline_faults
);

  // ---- status codes (ABI error codes verbatim; locals documented above) ----
  localparam logic [7:0] ST_OK              = 8'd0;
  localparam logic [7:0] ST_BAD_MAGIC       = 8'd1;
  localparam logic [7:0] ST_BAD_ABI_VER     = 8'd2;
  localparam logic [7:0] ST_RESERVED_FLAG   = 8'd3;
  localparam logic [7:0] ST_BAD_LENGTH      = 8'd4;
  localparam logic [7:0] ST_BAD_HEADER_CRC  = 8'd5;
  localparam logic [7:0] ST_BAD_PAYLOAD_CRC = 8'd6;
  localparam logic [7:0] ST_UNKNOWN_OPCODE  = 8'd7;
  localparam logic [7:0] ST_TRUNCATED       = 8'd11;
  localparam logic [7:0] ST_DEBUG_FLAG      = 8'd12;
  localparam logic [7:0] ST_COUNT_MISMATCH  = 8'd13;
  localparam logic [7:0] ST_EPOCH           = 8'd15;  // module-local (zref)
  localparam logic [7:0] ST_BRIDGE_ERR      = 8'd17;  // module-local (zref)
  localparam logic [7:0] ST_BLIT_REJECT     = 8'd18;  // module-local (zref)

  // ---- record size table (generated LayoutIR sizes — never hand-derived) ---
  function automatic logic [15:0] rec_size(input logic [15:0] op);
    case (op)
      zhao_abi_pkg::ZHAO_OP_NOP:                       rec_size = 16'd16;
      zhao_abi_pkg::ZHAO_OP_BEGIN_FRAME:               rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_END_FRAME:                 rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_SET_VIEW:                  rec_size = 16'd96;
      zhao_abi_pkg::ZHAO_OP_SET_PRESENTATION_CONTRACT: rec_size = 16'd48;
      zhao_abi_pkg::ZHAO_OP_TERRAIN_FIELD:             rec_size = 16'd112;
      zhao_abi_pkg::ZHAO_OP_SURFACE_STAMP:             rec_size = 16'd64;
      zhao_abi_pkg::ZHAO_OP_DRAW_FORM:                 rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_DRAW_POPULATION:           rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_DRAW_PROCEDURAL:           rec_size = 16'd64;
      zhao_abi_pkg::ZHAO_OP_DRAW_SKY:                  rec_size = 16'd176;
      zhao_abi_pkg::ZHAO_OP_EMIT_AUDIO_EVENT:          rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_DEBUG_BOOTSTRAP:           rec_size = 16'd64;
      zhao_abi_pkg::ZHAO_OP_DEBUG_FRAME_BLIT:          rec_size = 16'd48;
      zhao_abi_pkg::ZHAO_OP_DEBUG_RUMBLE:              rec_size = 16'd32;
      default:                                         rec_size = 16'd0;
    endcase
  endfunction

  function automatic logic [31:0] canvas_bytes(input logic [7:0] m);
    canvas_bytes = zhao_pkg::zhao_canvas_bytes(zhao_pkg::zhao_mode_from_abi(m));
  endfunction

  // the mode's lawful blit length. Under `ifdef FORMAL the harness may
  // override it downward (FORMAL_BLIT_LEN != 0) so the blit CRC gate is
  // reachable within the BMC depth; everywhere else this IS canvas_bytes.
  function automatic logic [31:0] blit_law_len(input logic [7:0] m);
`ifdef FORMAL
    blit_law_len = (FORMAL_BLIT_LEN != 0) ? 32'(FORMAL_BLIT_LEN)
                                          : canvas_bytes(m);
`else
    blit_law_len = canvas_bytes(m);
`endif
  endfunction

  // ------------------------------------------------------------ state -----
  typedef enum logic [3:0] {
    M_IDLE,
    M_HDR_REQ, M_HDR_WAIT, M_HDR_CHK,
    M_PAY_REQ, M_PAY_WAIT,
    M_WALK,
    M_PKT_DONE, M_STREAM,
    M_BLIT_CHK,
    M_BLIT_REQ, M_BLIT_WAIT, M_BLIT_COMMIT, M_BLIT_DATA
  } dma_state_e;

  /* verilator lint_off PROCASSINIT */
  dma_state_e m = M_IDLE;

  logic [7:0]  slot_buf [0:SLOT_BUF_BYTES-1] = '{default: 8'h00};
  // BLIT STAGING, ONE 64-BIT WORD PER ENTRY, not 245,760 bytes.
  //
  // This was `logic [7:0] blit_buf [0:BLIT_BUF_BYTES-1]`, and that shape is
  // what made this block unsynthesizable. MEASURED 2026-08-20, the first time
  // synthesis ever saw it: elaborating THIS MODULE ALONE peaked at 16.2 GB and
  // had not finished in seven minutes, while zhao_sdram_ctrl and
  // zhao_video_mode each finish in 0.26 GB. It is also the whole reason the
  // composed shell fit committed 28.4 GB and thrashed.
  //
  // The byte granularity was never used: both sides move aligned 8-byte groups.
  // ENFORCED-BY: tests/command/cmd_dma_directed.cpp — its blit cases drive
  // whole canvases through fetch and commit and compare the emitted beat
  // stream byte-for-byte against the oracle, so an unaligned move shows up as
  // wrong data rather than as a passing test. If either side ever moved an
  // unaligned group the word index would drop the low three bits and address
  // the wrong word silently, which is why this is the load-bearing claim here.
  // Reinforced by tests/command/cmd_random.cpp and tests/formal/
  // cmd_dma_crc_gate.sby, whose CRC covers the same bytes this indexing picks.
  //
  // The shapes themselves:
  //   - write: `wr_off <= wr_off + 32'd8` from zero, one hps_rsp_i.data word
  //   - read:  `wdata_off = b_commit + (wbeat << 3)`, and b_commit advances by
  //            glen_q which is a 64-byte multiple for a canvas blit
  // So this is a 64-bit memory that was described as a byte array, and the
  // description cost 245,760 elaboration entries instead of 30,720.
  //
  // Still NOT an M10K, and the contract says why: the write lives in an
  // async-reset process and the read is combinational, and an M10K has no
  // reset port and a registered read. Fixing that is a protocol change (the
  // beat stream needs a one-cycle read lead) and is deliberately not done
  // here. This commit makes the description honest and the block measurable;
  // whether a 1.97 Mbit on-chip buffer should exist at all is the open design
  // question in STATUS.md.
  localparam int unsigned BLIT_BUF_WORDS = BLIT_BUF_BYTES / 8;
  localparam int unsigned BLIT_IDX_W      = $clog2(BLIT_BUF_WORDS);  // 15
  logic [63:0] blit_buf [0:BLIT_BUF_WORDS-1];

  // fetch latches
  logic [1:0]  f_slot = 2'd0;
  logic [31:0] f_addr = 32'd0;
  logic [31:0] f_len = 32'd0;   // descriptor byte_len
  logic [31:0] f_epoch = 32'd0;

  // parsed header (latched at the header check)
  logic [31:0] cb = 32'd0;      // command_bytes
  logic [31:0] cc = 32'd0;      // command_count
  logic        h_debug = 1'b0;  // frame header flags bit0 (debug umbrella)

  // burst machinery
  logic [31:0] fetched = 32'd0;    // packet bytes landed in slot_buf
  logic [31:0] need_total = 32'd0; // 40 + cb
  logic [31:0] wr_off = 32'd0;
  logic [31:0] crc_pay_r = 32'hFFFF_FFFF;

  // record walk
  logic [31:0] walk_off = 32'd0;   // offset within the command stream
  logic [31:0] walk_cnt = 32'd0;

  // verified-packet stream
  logic [31:0] rd_off = 32'd0;
  logic        pkt_v = 1'b0;
  logic [31:0] pkt_len_r = 32'd0;

  // verdict pipeline (every outcome shaped identically)
  logic        done_v = 1'b0;
  logic [1:0]  done_slot = 2'd0;
  logic [7:0]  done_status = 8'd0;
  logic [31:0] done_bytes = 32'd36;
  logic [31:0] done_cmds = 32'd0;

  // blit registers
  logic [7:0]  b_dst = 8'd0;
  logic [7:0]  b_mode = 8'd0;
  logic [31:0] b_src = 32'd0;
  logic [31:0] b_len = 32'd0;
  logic [31:0] b_crc = 32'd0;
  logic [31:0] b_fetched = 32'd0;
  logic [31:0] b_commit = 32'd0;
  logic        blit_done_v = 1'b0;
  logic [7:0]  blit_status_r = 8'd0;
  // write-data beat streaming (M_BLIT_DATA): beat index + the accepted
  // request's length, latched at the guard acceptance edge
  logic [2:0]  wbeat = 3'd0;
  logic [6:0]  glen_q = 7'd0;

  // bridge request registers
  logic        hps_req_v = 1'b0;
  logic [31:0] hps_addr = 32'd0;
  logic [6:0]  hps_len = 7'd0;
  logic        hps_blit = 1'b0;

  // gate flags (formal anchors): set ONLY by the matching CRC comparison
  // (consumed by the `ifdef FORMAL properties — lint anchor note)
  /* verilator lint_off UNUSEDSIGNAL */
  logic hdr_gate = 1'b0;   // header checks + header CRC passed
  logic pay_gate = 1'b0;   // payload CRC + record walk passed
  logic blit_gate = 1'b0;
  /* verilator lint_on UNUSEDSIGNAL */  // blit payload CRC passed

  // D9 counters (u64, saturate never wrap)
  logic [63:0] live_cmds = 64'd0;
  logic [63:0] live_bytes = 64'd0;
  logic [63:0] live_drops = 64'd0;
  logic [63:0] sh_cmds = 64'd0;
  logic [63:0] sh_bytes = 64'd0;
  logic [63:0] sh_drops = 64'd0;
  logic        snap_v = 1'b0;
  /* verilator lint_on PROCASSINIT */

  // saturating add (spec/counters.md 4)
  function automatic logic [63:0] sat_add(input logic [63:0] a,
                                          input logic [63:0] b);
    sat_add = (a > 64'hFFFF_FFFF_FFFF_FFFF - b) ? 64'hFFFF_FFFF_FFFF_FFFF
                                                : (a + b);
  endfunction

  // header field readers (little-endian, capture_format.md 3)
  function automatic logic [15:0] hget16(input int unsigned off);
    hget16 = {slot_buf[off + 1], slot_buf[off]};
  endfunction

  function automatic logic [31:0] hget32(input int unsigned off);
    hget32 = {slot_buf[off + 3], slot_buf[off + 2], slot_buf[off + 1],
              slot_buf[off]};
  endfunction

  // CRC-32C (finalized) over the 32-byte frame header (capture_format.md 3:
  // header_crc32c covers bytes [0,32) — the ONLY use of this function, so
  // the loop bound is the constant 32; a parameter-bounded loop would put
  // SLOT_BUF_BYTES muxed CRC steps in the formal cone for nothing)
  function automatic logic [31:0] crc_final();
    logic [31:0] c;
    c = 32'hFFFF_FFFF;
    for (int unsigned i = 0; i < 32; i++) begin
      c = zhao_abi_pkg::zhao_crc32c_step(c, slot_buf[i]);
    end
    crc_final = ~c;
  endfunction

  // next burst length: multiple of 8, capped at 64, covering `left` bytes
  function automatic logic [6:0] burst_len(input logic [31:0] left);
    logic [31:0] n8;
    n8 = ((left + 32'd7) >> 3) << 3;
    burst_len = (n8 > 32'd64) ? 7'd64 : 7'(n8);
  endfunction

  // ------------------------------------------------------ ready/valid -------
  assign fetch_req_ready_o = (m == M_IDLE);
  assign blit_req_ready_o = (m == M_IDLE) && !fetch_req_valid_i;

  always_comb begin
    hps_req_o.valid  = hps_req_v;
    hps_req_o.write  = 1'b0;
    hps_req_o.client = hps_blit ? zhao_pkg::ZHAO_CLIENT_BLIT_DMA
                                : zhao_pkg::ZHAO_CLIENT_ENGINE0;
    hps_req_o.addr   = hps_addr;
    hps_req_o.len    = hps_len;
  end

  // VRAM commit requests: one guard write per 64 B (tail carried by len);
  // the request's DATA follows as guard_wvalid_o beats (M_BLIT_DATA)
  logic [31:0] g_base;
  logic [31:0] g_left;
  assign g_base = (b_dst == 8'd0) ? zhao_pkg::ZHAO_FB_SLOT0_BASE
                                  : zhao_pkg::ZHAO_FB_SLOT1_BASE;
  assign g_left = b_len - b_commit;
  always_comb begin
    guard_req_o.valid  = (m == M_BLIT_COMMIT) && blit_gate;
    guard_req_o.write  = 1'b1;
    guard_req_o.client = zhao_pkg::ZHAO_CLIENT_BLIT_DMA;
    guard_req_o.addr   = 27'(g_base + b_commit);  // bases < 2^27: exact
    guard_req_o.len    = (g_left >= 32'd64) ? 7'd64 : 7'(g_left);
    guard_req_o.be     = 64'hFFFF_FFFF_FFFF_FFFF;
  end

  // write-data beat stream: beat `wbeat` of the accepted request during
  // M_BLIT_DATA (the acceptance-cycle view shows beat 0; consumers must
  // sample on guard_wvalid_o beats only)
  // Kept 32 bits wide because it is an address arithmetic result; only bits
  // [BLIT_IDX_W+2:3] index the word array, since the low three are the
  // always-zero byte offset and everything above the index width cannot be
  // reached inside a canvas-sized buffer.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] wdata_off;
  /* verilator lint_on UNUSEDSIGNAL */
  assign wdata_off = b_commit
                   + ((m == M_BLIT_DATA) ? ({29'd0, wbeat} << 3) : 32'd0);
  // One aligned word, not eight scattered bytes. Identical bits: byte i of the
  // old read was blit_buf[wdata_off + i], and the word at wdata_off >> 3 holds
  // exactly those eight bytes in the same order.
  // The low three bits of wdata_off are the byte offset inside the word and
  // are always zero here (both sides move aligned 8-byte groups, argued at the
  // declaration). Slicing from bit 3 up gives exactly the index width the array
  // needs; taking the whole word offset would be a 29-bit index into a
  // 30,720-entry array.
  assign guard_wdata_o = blit_buf[wdata_off[BLIT_IDX_W+2:3]];
  assign guard_wvalid_o = (m == M_BLIT_DATA);

  assign pkt_valid_o = pkt_v;
  assign pkt_byte_o = slot_buf[rd_off];
  assign pkt_len_o = pkt_len_r;

  assign dma_done_o = done_v;
  assign dma_slot_o = done_slot;
  assign dma_status_o = done_status;
  assign dma_bytes_consumed_o = done_bytes;
  assign dma_cmds_consumed_o = done_cmds;

  assign blit_done_o = blit_done_v;
  assign blit_status_o = blit_status_r;

  // ------------------------------------------------------ sequential ------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      m <= M_IDLE;
      f_slot <= 2'd0; f_addr <= 32'd0; f_len <= 32'd0; f_epoch <= 32'd0;
      cb <= 32'd0; cc <= 32'd0; h_debug <= 1'b0;
      fetched <= 32'd0; need_total <= 32'd0; wr_off <= 32'd0;
      crc_pay_r <= 32'hFFFF_FFFF;
      walk_off <= 32'd0; walk_cnt <= 32'd0;
      rd_off <= 32'd0; pkt_v <= 1'b0; pkt_len_r <= 32'd0;
      done_v <= 1'b0; done_slot <= 2'd0; done_status <= 8'd0;
      b_dst <= 8'd0; b_mode <= 8'd0; b_src <= 32'd0; b_len <= 32'd0;
      b_crc <= 32'd0; b_fetched <= 32'd0; b_commit <= 32'd0;
      blit_done_v <= 1'b0; blit_status_r <= 8'd0;
      wbeat <= 3'd0; glen_q <= 7'd0;
      hps_req_v <= 1'b0; hps_addr <= 32'd0; hps_len <= 7'd0; hps_blit <= 1'b0;
      hdr_gate <= 1'b0; pay_gate <= 1'b0; blit_gate <= 1'b0;
      live_cmds <= 64'd0; live_bytes <= 64'd0; live_drops <= 64'd0;
      sh_cmds <= 64'd0; sh_bytes <= 64'd0; sh_drops <= 64'd0;
      snap_v <= 1'b0;
    end else begin
      // pulse defaults
      done_v <= 1'b0;
      blit_done_v <= 1'b0;
      snap_v <= 1'b0;
      hps_req_v <= 1'b0;

      // fetched-byte accounting (every accepted bridge beat, both engines)
      if (hps_rsp_i.beat_valid && !hps_rsp_i.err) begin
        live_bytes <= sat_add(live_bytes, 64'd8);
      end

      case (m)
        // ------------------------------------------------ packet fetch -----
        M_IDLE: begin
          hdr_gate <= 1'b0;
          pay_gate <= 1'b0;
          blit_gate <= 1'b0;
          if (fetch_req_valid_i) begin
            f_slot <= fetch_slot_i;
            f_addr <= fetch_addr_i;
            f_len <= fetch_byte_len_i;
            f_epoch <= fetch_epoch_i;
            fetched <= 32'd0;
            walk_off <= 32'd0;
            walk_cnt <= 32'd0;
            if (fetch_byte_len_i < 32'd36) begin
              // fail-safe #1: cannot even hold the 36-byte header
              done_v <= 1'b1; done_slot <= fetch_slot_i;
              done_status <= ST_BAD_LENGTH; done_bytes <= 32'd36;
              live_drops <= sat_add(live_drops, 64'd1);
              m <= M_IDLE;  // nothing fetched: return immediately
            end else begin
              m <= M_HDR_REQ;
            end
          end else if (blit_req_valid_i) begin
            crc_pay_r <= 32'hFFFF_FFFF;  // reused as the blit CRC register
            b_dst <= blit_dst_slot_i;
            b_mode <= blit_mode_i;
            b_src <= blit_src_i;
            b_len <= blit_len_i;
            b_crc <= blit_crc_i;
            b_fetched <= 32'd0;
            b_commit <= 32'd0;
            m <= M_BLIT_CHK;
          end
        end

        M_HDR_REQ: begin
          hps_req_v <= 1'b1;
          hps_blit <= 1'b0;
          hps_addr <= f_addr;
          hps_len <= burst_len(f_len);
          wr_off <= 32'd0;
          m <= M_HDR_WAIT;
        end

        M_HDR_WAIT: begin
          if (hps_rsp_i.err) begin
            done_v <= 1'b1; done_slot <= f_slot; done_status <= ST_BRIDGE_ERR;
            done_bytes <= 32'd36; done_cmds <= 32'd0;
            live_drops <= sat_add(live_drops, 64'd1);
            m <= M_IDLE;
          end else if (hps_rsp_i.beat_valid) begin
            for (int i = 0; i < 8; i++) begin
              if ((wr_off + 32'(i)) < SLOT_BUF_BYTES) begin
                slot_buf[wr_off + 32'(i)] <= hps_rsp_i.data[8*i +: 8];
              end
            end
            wr_off <= wr_off + 32'd8;
            fetched <= fetched + 32'd8;
            if (hps_rsp_i.last) m <= M_HDR_CHK;
          end
        end

        M_HDR_CHK: begin
          // fail-safe order, checks 1-3 + epoch (capture_format.md 3.2).
          // Blocking locals first: one ladder, first failure wins.
          logic [31:0] cb_v;
          logic [31:0] cc_v;
          logic [15:0] fl_v;
          logic [7:0]  st_v;
          logic        ok_v;
          cb_v = hget32(zhao_abi_pkg::ZHAO_OFF_COMMAND_BYTES);
          cc_v = hget32(zhao_abi_pkg::ZHAO_OFF_COMMAND_COUNT);
          fl_v = hget16(zhao_abi_pkg::ZHAO_OFF_FLAGS);
          ok_v = 1'b1;
          st_v = ST_OK;
          if (fetched < 32'd36) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // truncated header burst
          end else if (hget32(0) != zhao_abi_pkg::ZHAO_FRAME_MAGIC) begin
            ok_v = 1'b0; st_v = ST_BAD_MAGIC;
          end else if (hget16(zhao_abi_pkg::ZHAO_OFF_ABI_VERSION)
                       != 16'(zhao_abi_pkg::ZHAO_ABI_VERSION)) begin
            ok_v = 1'b0; st_v = ST_BAD_ABI_VER;
          end else if (|(fl_v & 16'hFFFE)) begin
            ok_v = 1'b0; st_v = ST_RESERVED_FLAG;
          end else if ((cb_v & 32'd15) != 32'd0) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // N % 16
          end else if (cc_v > (cb_v >> 5'd4)) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // count*16 <= command_bytes
          end else if ((32'd40 + cb_v) > f_len) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // packet == descriptor len
          end else if ((32'd40 + cb_v) > SLOT_BUF_BYTES) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // staging bound
          end else if ((32'd40 + cb_v)
                       > (zhao_abi_pkg::FRAME_SLOT_BYTES - 32'd40)) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // FRAME_SLOT_BYTES law
          end else if (crc_final() != hget32(zhao_abi_pkg::ZHAO_OFF_HEADER_CRC)) begin
            ok_v = 1'b0; st_v = ST_BAD_HEADER_CRC;  // THE header gate
          end else if (hget32(zhao_abi_pkg::ZHAO_OFF_RESOURCE_EPOCH) != f_epoch)
          begin
            ok_v = 1'b0; st_v = ST_EPOCH;           // drop before payload
          end
          if (!ok_v) begin
            done_v <= 1'b1; done_slot <= f_slot; done_status <= st_v;
            done_bytes <= 32'd36; done_cmds <= 32'd0;  // header-level abort
            live_drops <= sat_add(live_drops, 64'd1);
            m <= M_IDLE;
          end else begin
            hdr_gate <= 1'b1;
            cb <= cb_v;
            cc <= cc_v;
            h_debug <= fl_v[0];
            need_total <= 32'd40 + cb_v;
            // seed the payload CRC over the payload bytes already fetched
            // (bytes [36, min(fetched, 40+N)) of burst 1)
            begin
              logic [31:0] seed_end;
              logic [31:0] cseed;
              seed_end = fetched;
              if (seed_end > (32'd36 + cb_v)) seed_end = 32'd36 + cb_v;
              cseed = 32'hFFFF_FFFF;
              for (int unsigned k = 36; k < 192; k++) begin
                if (k < seed_end) begin
                  cseed = zhao_abi_pkg::zhao_crc32c_step(cseed, slot_buf[k]);
                end
              end
              crc_pay_r <= cseed;
            end
            if (fetched >= (32'd40 + cb_v)) begin
              m <= M_WALK;  // tiny packet: one burst covered everything
            end else begin
              m <= M_PAY_REQ;
            end
          end
        end

        M_PAY_REQ: begin
          hps_req_v <= 1'b1;
          hps_blit <= 1'b0;
          hps_addr <= f_addr + fetched;
          hps_len <= burst_len(need_total - fetched);
          wr_off <= fetched;
          m <= M_PAY_WAIT;
        end

        M_PAY_WAIT: begin
          if (hps_rsp_i.err) begin
            done_v <= 1'b1; done_slot <= f_slot; done_status <= ST_BRIDGE_ERR;
            done_bytes <= 32'd36; done_cmds <= 32'd0;
            live_drops <= sat_add(live_drops, 64'd1);
            m <= M_IDLE;
          end else if (hps_rsp_i.beat_valid) begin
            for (int i = 0; i < 8; i++) begin
              if ((wr_off + 32'(i)) < SLOT_BUF_BYTES) begin
                slot_buf[wr_off + 32'(i)] <= hps_rsp_i.data[8*i +: 8];
              end
            end
            // payload CRC over this beat's bytes inside [36, 40+cb)
            begin
              logic [31:0] cnext;
              cnext = crc_pay_r;
              for (int i = 0; i < 8; i++) begin
                if (((wr_off + 32'(i)) >= 32'd36)
                    && ((wr_off + 32'(i)) < (32'd36 + cb))) begin
                  cnext = zhao_abi_pkg::zhao_crc32c_step(
                      cnext, hps_rsp_i.data[8*i +: 8]);
                end
              end
              crc_pay_r <= cnext;
            end
            wr_off <= wr_off + 32'd8;
            fetched <= fetched + 32'd8;
            if (hps_rsp_i.last) begin
              if ((fetched + 32'd8) >= need_total) m <= M_WALK;
              else m <= M_PAY_REQ;
            end
          end
        end

        M_WALK: begin
          // payload CRC (check 4) then the record walk (5/9/10)
          if ({~crc_pay_r} != hget32(36 + cb)) begin
            done_v <= 1'b1; done_slot <= f_slot;
            done_status <= ST_BAD_PAYLOAD_CRC;
            done_bytes <= need_total; done_cmds <= 32'd0;
            live_drops <= sat_add(live_drops, 64'd1);
            m <= M_IDLE;
          end else if (walk_off >= cb) begin
            if (walk_cnt != cc) begin
              done_v <= 1'b1; done_slot <= f_slot;
              done_status <= ST_COUNT_MISMATCH;
              done_bytes <= need_total; done_cmds <= walk_cnt;
              live_drops <= sat_add(live_drops, 64'd1);
              m <= M_IDLE;
            end else begin
              pay_gate <= 1'b1;  // fully verified: release the packet
              done_v <= 1'b1; done_slot <= f_slot; done_status <= ST_OK;
              done_bytes <= need_total; done_cmds <= walk_cnt;
              live_cmds <= sat_add(live_cmds, {32'd0, walk_cnt});
              pkt_v <= 1'b1;          // the verified stream starts now
              pkt_len_r <= need_total;
              rd_off <= 32'd0;
              m <= M_STREAM;
            end
          end else begin
            logic [15:0] op_v;
            logic [15:0] rb_v;
            logic [7:0]  wst_v;
            logic        wok_v;
            op_v = hget16(36 + walk_off);
            rb_v = hget16(36 + walk_off + 2);
            wok_v = 1'b1;
            wst_v = ST_OK;
            if ((rb_v & 16'h000F) != 16'd0 || rb_v < 16'd16) begin
              wok_v = 1'b0; wst_v = ST_BAD_LENGTH;
            end else if (rec_size(op_v) == 16'd0) begin
              wok_v = 1'b0; wst_v = ST_UNKNOWN_OPCODE;
            end else if (rec_size(op_v) != rb_v) begin
              wok_v = 1'b0; wst_v = ST_BAD_LENGTH;
            end else if ((walk_off + 32'(rb_v)) > cb) begin
              wok_v = 1'b0; wst_v = ST_TRUNCATED;
            end else if ((op_v >= zhao_abi_pkg::ZHAO_DEBUG_OPCODE_LO)
                         && (op_v <= zhao_abi_pkg::ZHAO_DEBUG_OPCODE_HI)
                         && !h_debug) begin
              wok_v = 1'b0; wst_v = ST_DEBUG_FLAG;
            end
            if (!wok_v) begin
              done_v <= 1'b1; done_slot <= f_slot; done_status <= wst_v;
              done_bytes <= need_total; done_cmds <= walk_cnt;
              live_drops <= sat_add(live_drops, 64'd1);
              m <= M_IDLE;
            end else begin
              walk_off <= walk_off + 32'(rb_v);
              walk_cnt <= walk_cnt + 32'd1;
            end
          end
        end

        M_STREAM: begin
          if (pkt_ready_i) begin
            if ((rd_off + 32'd1) >= pkt_len_r) begin
              pkt_v <= 1'b0;
              rd_off <= 32'd0;
              m <= M_IDLE;
            end else begin
              rd_off <= rd_off + 32'd1;
            end
          end
        end

        // ------------------------------------------------ blit engine ------
        M_BLIT_CHK: begin
          // memory_rules.md 5: byte_len must equal canvas_bytes(mode) and
          // dst_slot must be 0/1 — anything else rejected BEFORE any byte
          // (blit_law_len == canvas_bytes outside `ifdef FORMAL)
          if ((b_dst > 8'd1) || (b_len != blit_law_len(b_mode))
              || (b_len == 32'd0) || (b_len > BLIT_BUF_BYTES)) begin
            blit_done_v <= 1'b1;
            blit_status_r <= ST_BLIT_REJECT;
            m <= M_IDLE;
          end else begin
            m <= M_BLIT_REQ;
          end
        end

        M_BLIT_REQ: begin
          hps_req_v <= 1'b1;
          hps_blit <= 1'b1;
          hps_addr <= b_src + b_fetched;
          hps_len <= burst_len(b_len - b_fetched);
          wr_off <= b_fetched;
          m <= M_BLIT_WAIT;
        end

        M_BLIT_WAIT: begin
          if (hps_rsp_i.err) begin
            blit_done_v <= 1'b1;
            blit_status_r <= ST_BRIDGE_ERR;
            m <= M_IDLE;  // zero guard writes: never entered M_BLIT_COMMIT
          end else if (hps_rsp_i.beat_valid) begin
            begin
              logic [31:0] cnext;
              cnext = crc_pay_r;
              if (wr_off < BLIT_BUF_BYTES) begin
                blit_buf[wr_off[BLIT_IDX_W+2:3]] <= hps_rsp_i.data;
              end
              for (int i = 0; i < 8; i++) begin
                cnext = zhao_abi_pkg::zhao_crc32c_step(
                    cnext, hps_rsp_i.data[8*i +: 8]);
              end
              crc_pay_r <= cnext;
            end
            wr_off <= wr_off + 32'd8;
            b_fetched <= b_fetched + 32'd8;
            if (hps_rsp_i.last) begin
              if ((b_fetched + 32'd8) >= b_len) m <= M_BLIT_COMMIT;
              else m <= M_BLIT_REQ;
            end
          end
        end

        M_BLIT_COMMIT: begin
          // first cycle here: the blit payload CRC gate. On mismatch ZERO
          // bytes commit to VRAM (memory_rules.md 4.3) and the blit faults.
          if (!blit_gate && (b_commit == 32'd0)
              && ({~crc_pay_r} != b_crc)) begin
            blit_done_v <= 1'b1;
            blit_status_r <= ST_BAD_PAYLOAD_CRC;
            m <= M_IDLE;
          end else if (!blit_gate) begin
            blit_gate <= 1'b1;  // CRC passed: commit may start next cycle
          end else if (guard_rsp_i.ready) begin
            // request accepted: stream its ceil(len/8) data beats before
            // advancing (the write-data seam law, header note)
            glen_q <= guard_req_o.len;
            wbeat  <= 3'd0;
            m      <= M_BLIT_DATA;
          end
        end

        M_BLIT_DATA: begin
          // one guard_wdata_o beat per cycle (guard_wvalid_o high);
          // beats = ceil(glen_q / 8), glen_q is 8..64 here (canvas bytes
          // are 64-B multiples; the tail guard keeps the general law)
          if ((4'(wbeat) + 4'd1) >= 4'((glen_q + 7'd7) >> 3)) begin
            if ((b_commit + {25'd0, glen_q}) >= b_len) begin
              blit_done_v <= 1'b1;
              blit_status_r <= ST_OK;
              m <= M_IDLE;
            end else begin
              b_commit <= b_commit + {25'd0, glen_q};
              m <= M_BLIT_COMMIT;
            end
          end else begin
            wbeat <= wbeat + 3'd1;
          end
        end

        default: m <= M_IDLE;
      endcase

      // D9 shadow latch at the frame boundary
      if (frame_tick_i.pulse) begin
        sh_cmds <= live_cmds;
        sh_bytes <= live_bytes;
        sh_drops <= live_drops;
        snap_v <= 1'b1;
      end
    end
  end

  // ------------------------------------------------------- snapshots -------
  always_comb begin
    snap_cmds_o.valid      = snap_v;
    snap_cmds_o.counter_id = zhao_pkg::ZHAO_CNT_COMMANDS;
    snap_cmds_o.value      = sh_cmds;
    snap_bytes_o.valid      = snap_v;
    snap_bytes_o.counter_id = zhao_pkg::ZHAO_CNT_HPS_BYTES;
    snap_bytes_o.value      = sh_bytes;
    snap_drops_o.valid      = snap_v;
    snap_drops_o.counter_id = zhao_pkg::ZHAO_CNT_DEADLINE_FAULTS;
    snap_drops_o.value      = sh_drops;
  end

  // ------------------------------------------------------- formal ---------
  // cmd_dma_crc_gate (plan 4 / capture_format.md 3.2 / memory_rules.md 4.3):
  //   (a) no verified-packet byte is offered before the header CRC passed
  //       (hdr_gate) AND the payload CRC + walk passed (pay_gate)
  //   (b) no VRAM write is offered before the blit payload CRC passed
  //   (c) reset leaves no partial handoff (all valids low after reset)
  // The formal harness (cmd_dma_crc_gate_harness.sv) shrinks the staging
  // buffers for tractability; the committed defaults are verified by the
  // ctest lane (cmd_dma_directed).
`ifdef FORMAL
  logic f_past_valid = 1'b0;
  always_ff @(posedge clk) f_past_valid <= 1'b1;
  always_ff @(posedge clk) begin
    if (f_past_valid && $past(rst_n)) assume(rst_n);
  end
  always_ff @(posedge clk) begin
    if (f_past_valid && !$past(rst_n)) begin
      assert(!pkt_valid_o);
      assert(!guard_req_o.valid);
      assert(!hps_req_o.valid);
    end
  end
  always_ff @(posedge clk) begin
    if (rst_n) begin
      if (pkt_valid_o) begin
        assert(hdr_gate && pay_gate);
      end
      if (guard_req_o.valid) begin
        assert(blit_gate);
      end
    end
  end

  // ---- non-vacuity covers (V16: covers must prove the antecedents) --------
  // Both assertions above are implications; a model that cannot raise
  // pkt_valid_o or guard_req_o.valid satisfies them while proving nothing
  // (the MEM.GUARD failure shape). c_guard is reachable ONLY because the
  // harness sets FORMAL_BLIT_LEN — see the parameter note; without it the
  // blit gate never opened in the formal cone and (b) was vacuous.
  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      c_pkt:      cover (pkt_valid_o);                 // (a) antecedent
      c_gates:    cover (hdr_gate && pay_gate);
      c_guard:    cover (guard_req_o.valid);           // (b) antecedent
      c_blitgate: cover (blit_gate);
      c_hdr_bad:  cover (dma_done_o
                         && (dma_status_o == ST_BAD_HEADER_CRC));
      c_blit_ok:  cover (blit_done_o && (blit_status_o == ST_OK));
      c_blit_bad: cover (blit_done_o
                         && (blit_status_o == ST_BAD_PAYLOAD_CRC));
    end
  end
`endif

endmodule : zhao_cmd_dma
