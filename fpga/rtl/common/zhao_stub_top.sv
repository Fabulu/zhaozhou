// zhao_stub_top.sv — Phase-1-gate stub top (charter 23 Phase 1: "one empty
// frame replays through ZRef and a stub RTL model").
//
// Consumes a sealed frame-packet byte stream over a ready/valid handshake,
// collects the full packet (40 + N bytes, capture_format.md 3) into a frame
// slot, then hands it to the GENERATED validator zhao_frame_validate
// (fpga/rtl/generated/zhao_abi_pkg.sv) — the same fail-safe order as the C++
// (zref_frame) and TS (frame.ts) validators. Early header-level checks
// (magic / abi_version / reserved flags / bounds / header CRC) abort after
// exactly 36 consumed bytes and resync, mirroring the spec's
// bytes_consumed semantics (capture_format.md 3.2).
//
// W4: imports the real generated zhao_abi_pkg (the W1 placeholder
// zhao_frame_pkg was removed). status carries the generated zhao_abi_error
// codes; header_crc32c is genuinely validated (incremental CRC-32C over the
// first 32 header bytes).
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

module zhao_stub_top
  import zhao_abi_pkg::*;
(
  input  logic        clk,
  input  logic        rst_n,

  // sealed frame-packet byte stream (little-endian), ready/valid
  input  logic        in_valid,
  output logic        in_ready,
  input  logic [7:0]  in_data,

  // status = zhao_abi_error code of the last decided frame (ZH_ABI_OK on
  // accept); completion_flags = ZHAO_COMPL_DONE / ZHAO_COMPL_ERR
  output logic [7:0]  status,
  output logic [7:0]  completion_flags,

  // observability counters (charter: counters over side effects); the
  // definitions match zref_frame's executor so the differential replay can
  // compare every counter (bytes_consumed: 36 on header-level abort, else
  // the whole 40+N packet)
  output logic [31:0] frames_accepted,
  output logic [31:0] frames_rejected,
  output logic [31:0] bytes_consumed,
  output logic [31:0] commands_consumed,

  // parity over header fields the stub does not act on (frame_id, sequence,
  // deadline) — keeps them debug-visible and in the lint cone
  output logic        hdr_parity
);

  typedef enum logic [1:0] {
    S_HDR     = 2'd0,  // collecting the 36-byte sealed header
    S_CHECK   = 2'd1,  // header-level checks (early abort path)
    S_PAYLOAD = 2'd2,  // collecting stream + trailing payload CRC
    S_FINAL   = 2'd3   // full generated validation over the slot
  } state_e;

  state_e      state;
  logic [31:0] idx;          // byte offset within the CURRENT packet
  logic [31:0] left;         // stream + payload-CRC bytes still to collect
  logic [31:0] crc_run;      // incremental CRC-32C over header bytes [0,32)

  // frame slot: header + stream + payload CRC, bounded by FRAME_SLOT_BYTES
  logic [7:0]  slot [0:FRAME_SLOT_BYTES-1];

  // ---- header field decode (little-endian assembly from slot bytes) -------
  logic [31:0] h_magic, h_frame_id, h_sequence, h_resource_epoch;
  logic [31:0] h_deadline, h_command_count, h_command_bytes, h_header_crc;
  logic [15:0] h_abi_version, h_flags;

  assign h_magic         = {slot[ZHAO_OFF_MAGIC+3],         slot[ZHAO_OFF_MAGIC+2],         slot[ZHAO_OFF_MAGIC+1],         slot[ZHAO_OFF_MAGIC]};
  assign h_abi_version   = {slot[ZHAO_OFF_ABI_VERSION+1],   slot[ZHAO_OFF_ABI_VERSION]};
  assign h_flags         = {slot[ZHAO_OFF_FLAGS+1],         slot[ZHAO_OFF_FLAGS]};
  assign h_frame_id      = {slot[ZHAO_OFF_FRAME_ID+3],      slot[ZHAO_OFF_FRAME_ID+2],      slot[ZHAO_OFF_FRAME_ID+1],      slot[ZHAO_OFF_FRAME_ID]};
  assign h_sequence      = {slot[ZHAO_OFF_SEQUENCE+3],      slot[ZHAO_OFF_SEQUENCE+2],      slot[ZHAO_OFF_SEQUENCE+1],      slot[ZHAO_OFF_SEQUENCE]};
  assign h_resource_epoch= {slot[ZHAO_OFF_RESOURCE_EPOCH+3],slot[ZHAO_OFF_RESOURCE_EPOCH+2],slot[ZHAO_OFF_RESOURCE_EPOCH+1],slot[ZHAO_OFF_RESOURCE_EPOCH]};
  assign h_deadline      = {slot[ZHAO_OFF_DEADLINE+3],      slot[ZHAO_OFF_DEADLINE+2],      slot[ZHAO_OFF_DEADLINE+1],      slot[ZHAO_OFF_DEADLINE]};
  assign h_command_count = {slot[ZHAO_OFF_COMMAND_COUNT+3], slot[ZHAO_OFF_COMMAND_COUNT+2], slot[ZHAO_OFF_COMMAND_COUNT+1], slot[ZHAO_OFF_COMMAND_COUNT]};
  assign h_command_bytes = {slot[ZHAO_OFF_COMMAND_BYTES+3], slot[ZHAO_OFF_COMMAND_BYTES+2], slot[ZHAO_OFF_COMMAND_BYTES+1], slot[ZHAO_OFF_COMMAND_BYTES]};
  assign h_header_crc    = {slot[ZHAO_OFF_HEADER_CRC+3],    slot[ZHAO_OFF_HEADER_CRC+2],    slot[ZHAO_OFF_HEADER_CRC+1],    slot[ZHAO_OFF_HEADER_CRC]};

  // observation parity over header fields the stub does not act on
  // (frame_id, sequence, resource_epoch, deadline)
  assign hdr_parity = ^{h_frame_id, h_sequence, h_resource_epoch, h_deadline};

  // ---- full-packet validation (combinational, only meaningful in S_FINAL) --
  logic [31:0]      v_total;
  zhao_abi_error_e  v_err;
  int unsigned      v_cmds;

  always_comb begin
    v_total = 32'd0;
    v_err   = ZH_ABI_OK;
    v_cmds  = 32'd0;
    if (state == S_FINAL) begin
      v_total = 32'(ZHAO_FRAME_OVERHEAD) + h_command_bytes;
      v_err   = zhao_frame_validate(slot, v_total, FRAME_SLOT_BYTES, v_cmds);
    end
  end

  // ---- early header-level checks (order per capture_format.md 3.2) ---------
  logic bad_magic, bad_abi, bad_flags, bad_length, bad_header_crc, empty_frame;

  always_comb begin
    bad_magic      = (h_magic != ZHAO_FRAME_MAGIC);
    bad_abi        = (h_abi_version != ZHAO_ABI_VERSION[15:0]);
    bad_flags      = ((h_flags & ~ZHAO_FRAME_FLAG_CONTAINS_DEBUG) != 16'h0000);
    bad_length     = ((h_command_bytes % 32'(ZHAO_COMMAND_ALIGNMENT)) != 32'd0)
                  || ((32'(ZHAO_FRAME_OVERHEAD) + h_command_bytes) > 32'(FRAME_SLOT_BYTES))
                  || ((h_command_count * 32'd16) > h_command_bytes);
    bad_header_crc = (~crc_run != h_header_crc);
    empty_frame    = (h_command_bytes == 32'd0);
  end

  // ---- ready/valid ----------------------------------------------------------
  assign in_ready = (state == S_HDR) || (state == S_PAYLOAD);

  // ---- sequential core --------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state             <= S_HDR;
      idx               <= 32'd0;
      left              <= 32'd0;
      crc_run           <= 32'hFFFFFFFF;
      status            <= ZH_ABI_OK;
      completion_flags  <= 8'h00;
      frames_accepted   <= 32'd0;
      frames_rejected   <= 32'd0;
      bytes_consumed    <= 32'd0;
      commands_consumed <= 32'd0;
    end else begin
      if (in_valid && in_ready) begin
        bytes_consumed <= bytes_consumed + 32'd1;
      end

      unique case (state)
        S_HDR: begin
          if (in_valid && in_ready) begin
            if (idx == 32'd0) begin
              // a new frame clears the previous frame's verdict
              status           <= ZH_ABI_OK;
              completion_flags <= 8'h00;
            end
            slot[idx] <= in_data;
            if (idx < 32'd32) begin
              crc_run <= zhao_crc32c_step(crc_run, in_data);
            end
            if (idx == 32'(ZHAO_FRAME_HEADER_BYTES - 1)) begin
              state <= S_CHECK;
              idx   <= 32'd0;
            end else begin
              idx <= idx + 32'd1;
            end
          end
        end

        S_CHECK: begin
          // one stall cycle: the fully written slot[] is only visible now.
          // Header-level aborts consume exactly 36 bytes (spec 3.2) and
          // resync; the checks run in the spec's order.
          if (bad_magic) begin
            status           <= ZH_ABI_BAD_MAGIC;
            completion_flags <= ZHAO_COMPL_ERR;
            frames_rejected  <= frames_rejected + 32'd1;
            state            <= S_HDR;
            crc_run          <= 32'hFFFFFFFF;
          end else if (bad_abi) begin
            status           <= ZH_ABI_BAD_ABI_VERSION;
            completion_flags <= ZHAO_COMPL_ERR;
            frames_rejected  <= frames_rejected + 32'd1;
            state            <= S_HDR;
            crc_run          <= 32'hFFFFFFFF;
          end else if (bad_flags) begin
            status           <= ZH_ABI_RESERVED_FLAG;
            completion_flags <= ZHAO_COMPL_ERR;
            frames_rejected  <= frames_rejected + 32'd1;
            state            <= S_HDR;
            crc_run          <= 32'hFFFFFFFF;
          end else if (bad_length) begin
            status           <= ZH_ABI_BAD_LENGTH;
            completion_flags <= ZHAO_COMPL_ERR;
            frames_rejected  <= frames_rejected + 32'd1;
            state            <= S_HDR;
            crc_run          <= 32'hFFFFFFFF;
          end else if (bad_header_crc) begin
            status           <= ZH_ABI_BAD_HEADER_CRC;
            completion_flags <= ZHAO_COMPL_ERR;
            frames_rejected  <= frames_rejected + 32'd1;
            state            <= S_HDR;
            crc_run          <= 32'hFFFFFFFF;
          end else begin
            // header is trustworthy: collect exactly command_bytes + 4 more
            idx  <= 32'(ZHAO_FRAME_HEADER_BYTES);
            left <= h_command_bytes + 32'd4;
            if (empty_frame) begin
              state <= S_PAYLOAD;  // still consumes the 4 payload-CRC bytes
            end else begin
              state <= S_PAYLOAD;
            end
          end
        end

        S_PAYLOAD: begin
          if (in_valid && in_ready) begin
            slot[idx] <= in_data;
            idx       <= idx + 32'd1;
            left      <= left - 32'd1;
            if (left == 32'd1) begin
              state <= S_FINAL;
            end
          end
        end

        S_FINAL: begin
          // full generated validation over the collected slot (the same
          // capture_format.md 3.2 walk as the C++/TS validators), computed
          // combinationally in v_err/v_cmds below
          commands_consumed <= v_cmds;
          if (v_err == ZH_ABI_OK) begin
            status           <= ZH_ABI_OK;
            completion_flags <= ZHAO_COMPL_DONE;
            frames_accepted  <= frames_accepted + 32'd1;
          end else begin
            status           <= v_err;
            completion_flags <= ZHAO_COMPL_ERR;
            frames_rejected  <= frames_rejected + 32'd1;
          end
          state    <= S_HDR;
          idx      <= 32'd0;
          crc_run  <= 32'hFFFFFFFF;
        end

        default: begin
          // unreachable; keeps the case total without a latch
          state   <= S_HDR;
          idx     <= 32'd0;
          crc_run <= 32'hFFFFFFFF;
        end
      endcase
    end
  end

endmodule : zhao_stub_top
