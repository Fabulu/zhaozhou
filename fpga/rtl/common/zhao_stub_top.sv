// zhao_stub_top.sv — Phase-1-gate stub top (charter 23 Phase 1: "one empty
// frame replays through ZRef and a stub RTL model").
//
// Consumes a sealed frame-packet byte stream over a ready/valid handshake,
// validates magic / abi_version / lengths against the (placeholder) frame
// package zhao_frame_pkg, then returns {status, completion_flags, counters}.
// It deliberately does NOTHING with the payload: it is the bus shell the
// W4 empty-frame replay drives; CMD.* semantics belong to later blocks.
//
// Import swap point (W4): replace `import zhao_frame_pkg::*;` with
// `import zhao_abi_pkg::*;` — the generated package provides the same
// constants. Everything else in this module is swap-invariant.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

module zhao_stub_top
  import zhao_frame_pkg::*;
(
  input  logic        clk,
  input  logic        rst_n,

  // sealed frame-packet byte stream (little-endian), ready/valid
  input  logic        in_valid,
  output logic        in_ready,
  input  logic [7:0]  in_data,

  // status / completion (codes and bits from zhao_frame_pkg)
  output logic [7:0]  status,
  output logic [7:0]  completion_flags,

  // observability counters (charter: counters over side effects)
  output logic [31:0] frames_accepted,
  output logic [31:0] frames_rejected,
  output logic [31:0] bytes_consumed,

  // parity over header fields the stub does not act on (frame_id, sequence,
  // resource_epoch, deadline, flags, header_crc32c) — an observation port so
  // the ignored fields stay in the lint-clean cone and debug-visible.
  output logic        hdr_parity
);

  // states: collecting header bytes, checking the completed header,
  // consuming payload bytes.
  typedef enum logic [1:0] {
    S_HDR     = 2'd0,
    S_CHECK   = 2'd1,
    S_PAYLOAD = 2'd2
  } state_e;

  state_e      state;
  logic [5:0]  hdr_idx;         // 0..ZHAO_HEADER_BYTES-1 (36 fits in 6 bits)
  logic [31:0] payload_left;

  logic [7:0]  hdr [0:ZHAO_HEADER_BYTES-1];

  // ---- header field decode (little-endian assembly) ----------------------
  logic [31:0] h_magic, h_frame_id, h_sequence, h_resource_epoch;
  logic [31:0] h_deadline, h_command_count, h_command_bytes, h_header_crc;
  logic [15:0] h_abi_version, h_flags;

  assign h_magic         = {hdr[OFF_MAGIC+3],         hdr[OFF_MAGIC+2],         hdr[OFF_MAGIC+1],         hdr[OFF_MAGIC]};
  assign h_abi_version   = {hdr[OFF_ABI_VERSION+1],   hdr[OFF_ABI_VERSION]};
  assign h_flags         = {hdr[OFF_FLAGS+1],         hdr[OFF_FLAGS]};
  assign h_frame_id      = {hdr[OFF_FRAME_ID+3],      hdr[OFF_FRAME_ID+2],      hdr[OFF_FRAME_ID+1],      hdr[OFF_FRAME_ID]};
  assign h_sequence      = {hdr[OFF_SEQUENCE+3],      hdr[OFF_SEQUENCE+2],      hdr[OFF_SEQUENCE+1],      hdr[OFF_SEQUENCE]};
  assign h_resource_epoch= {hdr[OFF_RESOURCE_EPOCH+3],hdr[OFF_RESOURCE_EPOCH+2],hdr[OFF_RESOURCE_EPOCH+1],hdr[OFF_RESOURCE_EPOCH]};
  assign h_deadline      = {hdr[OFF_DEADLINE+3],      hdr[OFF_DEADLINE+2],      hdr[OFF_DEADLINE+1],      hdr[OFF_DEADLINE]};
  assign h_command_count = {hdr[OFF_COMMAND_COUNT+3], hdr[OFF_COMMAND_COUNT+2], hdr[OFF_COMMAND_COUNT+1], hdr[OFF_COMMAND_COUNT]};
  assign h_command_bytes = {hdr[OFF_COMMAND_BYTES+3], hdr[OFF_COMMAND_BYTES+2], hdr[OFF_COMMAND_BYTES+1], hdr[OFF_COMMAND_BYTES]};
  assign h_header_crc    = {hdr[OFF_HEADER_CRC+3],    hdr[OFF_HEADER_CRC+2],    hdr[OFF_HEADER_CRC+1],    hdr[OFF_HEADER_CRC]};

  assign hdr_parity = ^{h_flags, h_frame_id, h_sequence,
                        h_resource_epoch, h_deadline, h_header_crc};

  // ---- validation (fail-safe order: magic, then abi, then lengths) --------
  logic bad_magic, bad_abi, bad_length;

  always_comb begin
    bad_magic = (h_magic != ZHAO_FRAME_MAGIC);
    bad_abi   = (h_abi_version != ZHAO_ABI_VERSION[15:0]);
    // payload must be command-aligned, at least one aligned record per
    // declared command, and fit the frame slot beside the header.
    bad_length = ((h_command_bytes & (32'(ZHAO_COMMAND_ALIGNMENT) - 32'd1)) != 32'd0)
              || (h_command_bytes > (ZHAO_FRAME_SLOT_BYTES - ZHAO_HEADER_BYTES))
              || (h_command_count > (h_command_bytes / 32'(ZHAO_COMMAND_ALIGNMENT)));
  end

  // ---- ready/valid --------------------------------------------------------
  assign in_ready = (state == S_HDR) || (state == S_PAYLOAD);

  // ---- sequential core ----------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state             <= S_HDR;
      hdr_idx           <= 6'd0;
      payload_left      <= 32'd0;
      status            <= STATUS_IDLE;
      completion_flags  <= 8'h00;
      frames_accepted   <= 32'd0;
      frames_rejected   <= 32'd0;
      bytes_consumed    <= 32'd0;
      for (int unsigned i = 0; i < ZHAO_HEADER_BYTES; i++) begin
        hdr[i] <= 8'h00;
      end
    end else begin
      if (in_valid && in_ready) begin
        bytes_consumed <= bytes_consumed + 32'd1;
      end

      unique case (state)
        S_HDR: begin
          if (in_valid && in_ready) begin
            if (hdr_idx == 6'd0) begin
              // a new frame clears the previous frame's status
              status           <= STATUS_IDLE;
              completion_flags <= 8'h00;
            end
            hdr[hdr_idx] <= in_data;
            if (hdr_idx == 6'(ZHAO_HEADER_BYTES - 1)) begin
              hdr_idx <= 6'd0;
              state   <= S_CHECK;
            end else begin
              hdr_idx <= hdr_idx + 6'd1;
            end
          end
        end

        S_CHECK: begin
          // one stall cycle: the fully written hdr[] is only visible now
          if (bad_magic) begin
            status            <= STATUS_ERR_MAGIC;
            completion_flags  <= ZHAO_COMPL_ERR;
            frames_rejected   <= frames_rejected + 32'd1;
            state             <= S_HDR;
            hdr_idx           <= 6'd0;
          end else if (bad_abi) begin
            status            <= STATUS_ERR_ABI;
            completion_flags  <= ZHAO_COMPL_ERR;
            frames_rejected   <= frames_rejected + 32'd1;
            state             <= S_HDR;
            hdr_idx           <= 6'd0;
          end else if (bad_length) begin
            status            <= STATUS_ERR_LENGTH;
            completion_flags  <= ZHAO_COMPL_ERR;
            frames_rejected   <= frames_rejected + 32'd1;
            state             <= S_HDR;
            hdr_idx           <= 6'd0;
          end else if (h_command_bytes == 32'd0) begin
            // empty frame (command_count must also be 0 by bad_length)
            status            <= STATUS_OK;
            completion_flags  <= ZHAO_COMPL_DONE;
            frames_accepted   <= frames_accepted + 32'd1;
            state             <= S_HDR;
            hdr_idx           <= 6'd0;
          end else begin
            payload_left <= h_command_bytes;
            state        <= S_PAYLOAD;
            hdr_idx      <= 6'd0;
          end
        end

        S_PAYLOAD: begin
          if (in_valid && in_ready) begin
            payload_left <= payload_left - 32'd1;
            if (payload_left == 32'd1) begin
              status           <= STATUS_OK;
              completion_flags <= ZHAO_COMPL_DONE;
              frames_accepted  <= frames_accepted + 32'd1;
              state            <= S_HDR;
              hdr_idx          <= 6'd0;
            end
          end
        end

        default: begin
          // unreachable; keeps the case total without a latch
          state   <= S_HDR;
          hdr_idx <= 6'd0;
        end
      endcase
    end
  end

endmodule : zhao_stub_top
