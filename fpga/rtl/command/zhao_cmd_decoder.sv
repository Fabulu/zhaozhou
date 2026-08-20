// zhao_cmd_decoder.sv — CMD.DECODER: the STREAMING packet validator.
//
// Contract: design/contracts/CMD.DECODER.md
// Reference: zref::cmd::validate (reference/include/zref/zref_cmd.hpp), a thin
// view onto zhao::zhao_frame_validate — the same function zhao_stub_top, the
// capture tooling and the 19 committed goldens in tests/abi/golden/ have always
// agreed with.
//
// WHY THIS BLOCK EXISTS, given that a validator already runs. zhao_stub_top
// hands a whole frame slot to zhao_frame_validate as a FUNCTION CALL, and its
// own header says why that is not the answer: "silicon validates STREAMING
// byte-by-byte". Silicon has no slot to hand over. CMD.DMA emits verified bytes
// one at a time and this block must reach the identical verdict having seen
// each byte exactly once, holding no packet buffer.
//
// Not holding one is the point. CMD.DMA's blit_buf was 1.97 Mbit and made the
// composed shell unsynthesizable; a decoder that buffered a frame slot would be
// 8 Mbit. This block's whole state is a header capture, two running CRCs, a
// record cursor and three counters.
//
// ---------------------------------------------------------------------------
// THE VALIDATION ORDER IS NORMATIVE (spec/capture_format.md §3.2)
// ---------------------------------------------------------------------------
// Every check runs BEFORE any payload field is consumed; on any error the frame
// aborts with no partial consumption. `bytes_consumed_o` is part of the law and
// is easy to get wrong: 36 on a header-level abort (checks 1-3), otherwise
// 40 + N — the whole packet is consumed before the verdict even when a later
// check fails.
//
// COVERED HERE, all streaming: 1, 2, 3, 4, 5, the record-header half of 6, 9
// and 10.
//
// DEFERRED, AND IT IS A TOOLING GAP RATHER THAN A CHOICE:
//   6  payload PAD BYTES zero        -> ZH_ABI_RESERVED_FIELD
//   7  enum ranges / bitfield widths -> ZH_ABI_BAD_VALUE
// The generated package knows both laws already — zhao_record_pad_nonzero and
// zhao_record_enum_bad — but both take `input logic [7:0] p []`, an OPEN ARRAY.
// Those are exactly the "verification-only open-array helpers" the shell
// project excludes from Quartus with QUARTUS_SYNTHESIS=1, so they cannot be
// called from synthesizable RTL, and a streaming decoder could not hand them a
// whole record anyway.
//
// Doing these properly needs SW.TOOLS.ABIDOC to emit a SYNTHESIZABLE per-opcode
// table of (pad offsets, enum offsets, legal ranges) that a streaming walker
// consults by offset as each byte arrives. Hand-writing that table here would
// break this contract's own rule that layouts come EXCLUSIVELY from the
// generated package. So it is recorded, not skipped: a packet this block calls
// OK may still fail zref's check 6 or 7.
//   ENFORCED-BY: tests/command/cmd_decoder_directed.cpp — its differential
//   asserts the deferred pair is the ONLY source of disagreement with zref, so
//   the day the generator emits that table this test goes red until this block
//   consumes it.
//
//   8  handle generations vs resource_epoch: spec §3.2 records "no v1 field",
//      so there is nothing to check until DrawSky activates it in wave 8.
// ---------------------------------------------------------------------------
module zhao_cmd_decoder
  import zhao_abi_pkg::*;
(
    input  logic clk,
    input  logic rst_n,

    // ---- sealed packet byte stream, from CMD.DMA ---------------------------
    input  logic        pkt_valid_i,
    output logic        pkt_ready_o,
    input  logic [ 7:0] pkt_byte_i,
    input  logic [31:0] pkt_len_i,    // CMD.DMA's verified total, 40 + N

    // ---- decoded record headers -------------------------------------------
    // THE CONSUMER MUST NOT RETIRE THESE until decode_done_o with
    // decode_error_o == ZH_ABI_OK. The payload CRC (4) and the count laws (9)
    // cannot conclude until the last byte, so a record emitted earlier belongs
    // to a packet that may still be rejected. The contract argues this choice
    // and records the rejected alternative (buffer and replay), which would
    // re-introduce exactly the storage this block exists to avoid.
    output logic        rec_valid_o,
    input  logic        rec_ready_i,
    output logic [15:0] rec_opcode_o,
    output logic [15:0] rec_bytes_o,
    output logic [31:0] rec_source_id_o,
    output logic [31:0] rec_index_o,

    // ---- verdict -----------------------------------------------------------
    output logic        decode_done_o,     // one-cycle pulse: verdict valid
    output logic [ 7:0] decode_error_o,    // zhao_abi_error
    output logic [31:0] bytes_consumed_o,  // 36 on header abort, else 40+N
    output logic [31:0] commands_o         // records walked
);

  // The fail-safe order means the FIRST failing check is the reported one, so
  // every later assignment is suppressed by err_set.
  `define ZHAO_FAIL(code) if (!err_set) begin err <= (code); err_set <= 1'b1; end

  typedef enum logic [2:0] {
    S_HDR,    // consuming bytes [0,36)
    S_CHECK,  // header decided; consumes no byte
    S_REC,    // walking the command stream
    S_PCRC,   // the trailing payload CRC word
    S_DONE
  } st_e;
  st_e st;

  logic [31:0] pos, total;
  logic [31:0] h_magic, h_cmd_count, h_cmd_bytes, h_hdr_crc;
  logic [15:0] h_abi_ver, h_flags;

  // Two CRC accumulators, not one: [0,32) and [36, 36+N) never overlap, so a
  // single accumulator would need reinitialising mid-stream, which is a state
  // bug waiting to happen.
  logic [31:0] crc_hdr, crc_pay;
  // 24 bits, not 32: the final byte of each word is compared straight from
  // pkt_byte_i, so only three bytes of history are ever read back.
  logic [23:0] pcrc_hist;

  logic [31:0] rec_sum, rec_idx;
  logic [15:0] rec_off, cur_opcode, cur_bytes;
  logic [31:0] cur_source, cur_flags;
  logic [23:0] cur_rsv0;   // same three-byte history as pcrc_hist

  logic [7:0] err;
  logic       err_set, hdr_abort, done, hdr_bad;

  // RECORD-LEVEL ERRORS ARE HELD SEPARATELY, and this is forced by streaming.
  //
  // spec 3.2's order puts the payload CRC (check 4) BEFORE the per-record
  // checks (5, 6, 10). A streaming decoder discovers them the other way round:
  // a record header is complete long before the CRC over the whole stream is.
  // Latching a record error into `err` directly therefore reported
  // ZH_ABI_RESERVED_FIELD where the oracle reports ZH_ABI_BAD_PAYLOAD_CRC --
  // caught by cmd_decoder_directed on its first run, ten checks in two cases.
  //
  // So the record verdict waits here and is only promoted at the end, after
  // the CRC has had its turn. The normative order is preserved even though the
  // discovery order cannot be.
  logic [7:0] rec_err_l;
  logic       rec_err_set;

  assign decode_error_o   = err;
  assign bytes_consumed_o = hdr_abort ? 32'd36 : total;
  assign commands_o       = rec_idx;
  assign decode_done_o    = done;
  assign rec_opcode_o     = cur_opcode;
  assign rec_bytes_o      = cur_bytes;
  assign rec_source_id_o  = cur_source;
  assign rec_index_o      = rec_idx;

  // Bytes are accepted while this packet is still being consumed, EXCEPT while
  // a presented record has not been taken.
  //
  // An earlier draft claimed a stalled consumer should not stall the stream,
  // "because the record port is only a report". That was wrong and Verilator
  // caught it as an unused rec_ready_i: a one-cycle rec_valid_o that nobody
  // accepts is a DROPPED record, and the ledger says this block's backpressure
  // is ready_valid. So the handshake is real — hold the byte stream until the
  // record is taken. CMD.DMA already buffers on its own side, so the stall
  // propagates cleanly upstream rather than losing anything.
  assign pkt_ready_o = ((st == S_HDR) || (st == S_REC) || (st == S_PCRC))
                    && !(rec_valid_o && !rec_ready_i);

  logic take;
  assign take = pkt_valid_i && pkt_ready_o;

  // `total` is latched on the FIRST byte with a non-blocking assignment, so it
  // still reads zero during that byte. Anything comparing against the length in
  // the same cycle must use this instead -- the first version of the
  // short-packet exit did not, and `1 >= 0` sent every packet straight to
  // S_CHECK with an empty header.
  logic [31:0] eff_total;
  assign eff_total = (pos == '0) ? pkt_len_i : total;

  // A record's declared size, from the GENERATED table. Unknown opcodes return
  // zero, which is how check 5 tells "not in the ABI" from "wrong size".
  logic [31:0] known_sz;
  assign known_sz = 32'(zhao_opcode_record_bytes(cur_opcode));

  // Checks 1-3 as one combinational verdict, so S_CHECK both reports the first
  // failure and knows whether to abort without re-listing the conditions.
  logic [7:0] hdr_err;
  always_comb begin
    hdr_err = ZH_ABI_OK;
    if      (total < 32'd36)                             hdr_err = ZH_ABI_BAD_LENGTH;
    else if (h_magic != 32'h314B_505A)                   hdr_err = ZH_ABI_BAD_MAGIC;
    else if (h_abi_ver != 16'(ZHAO_ABI_VERSION))         hdr_err = ZH_ABI_BAD_ABI_VERSION;
    else if (|h_flags[15:1])                             hdr_err = ZH_ABI_RESERVED_FLAG;
    else if ((32'd40 + h_cmd_bytes) > FRAME_SLOT_BYTES)  hdr_err = ZH_ABI_BAD_LENGTH;
    else if (|h_cmd_bytes[3:0])                          hdr_err = ZH_ABI_BAD_LENGTH;
    // NOT `(h_cmd_count << 4) > h_cmd_bytes`. That shift OVERFLOWS 32 bits: a
    // command_count of 0x10000000 shifts to 0x1_0000_0000, truncates to zero,
    // and the check silently passes -- the packet then fell through to the
    // header CRC and reported BAD_HEADER_CRC where the oracle says BAD_LENGTH.
    // Found by the random lane at iteration 599, a single bit flipped in byte
    // 27. Dividing the other side instead cannot overflow, and the two forms
    // agree exactly because command_bytes is already a multiple of 16 (checked
    // on the line above).
    else if (h_cmd_count > (h_cmd_bytes >> 4))           hdr_err = ZH_ABI_BAD_LENGTH;
    else if (total != (32'd40 + h_cmd_bytes))            hdr_err = ZH_ABI_BAD_LENGTH;
    else if ((~crc_hdr) != h_hdr_crc)                    hdr_err = ZH_ABI_BAD_HEADER_CRC;
  end
  assign hdr_bad = (hdr_err != ZH_ABI_OK);

  // The record-header verdict, likewise combinational at its decision instant.
  // `nrsv` completes on the very cycle this is evaluated, so it is formed here
  // rather than read from the register.
  logic [7:0]  rec_err;
  logic [31:0] nrsv;
  always_comb begin
    nrsv = {pkt_byte_i, cur_rsv0};
    rec_err = ZH_ABI_OK;
    if      (|cur_bytes[3:0] || (cur_bytes < 16'd16))    rec_err = ZH_ABI_BAD_LENGTH;
    else if ((rec_sum + 32'(cur_bytes)) > h_cmd_bytes)   rec_err = ZH_ABI_BAD_LENGTH;
    else if (known_sz == 32'd0)                          rec_err = ZH_ABI_UNKNOWN_OPCODE;
    else if (32'(cur_bytes) != known_sz)                 rec_err = ZH_ABI_BAD_LENGTH;
    else if (nrsv != 32'd0)                              rec_err = ZH_ABI_RESERVED_FIELD;
    else if (cur_flags != 32'd0)                         rec_err = ZH_ABI_RESERVED_FLAG;
    else if ((cur_opcode >= ZHAO_DEBUG_OPCODE_LO)
          && (cur_opcode <= ZHAO_DEBUG_OPCODE_HI)
          && !h_flags[0])                                rec_err = ZH_ABI_DEBUG_FLAG_REQUIRED;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st <= S_HDR; pos <= '0; total <= '0;
      h_magic <= '0; h_cmd_count <= '0; h_cmd_bytes <= '0; h_hdr_crc <= '0;
      h_abi_ver <= '0; h_flags <= '0;
      crc_hdr <= 32'hFFFF_FFFF; crc_pay <= 32'hFFFF_FFFF; pcrc_hist <= '0;
      rec_sum <= '0; rec_idx <= '0; rec_off <= '0;
      cur_opcode <= '0; cur_bytes <= '0; cur_source <= '0;
      cur_flags <= '0; cur_rsv0 <= '0;
      err <= ZH_ABI_OK; err_set <= 1'b0; hdr_abort <= 1'b0; done <= 1'b0;
      rec_err_l <= ZH_ABI_OK; rec_err_set <= 1'b0;
      rec_valid_o <= 1'b0;
    end else begin
      done <= 1'b0;
      // HOLD rec_valid_o until it is accepted. Clearing it unconditionally was
      // a bug: pkt_ready_o stalls the byte stream while a record is unaccepted,
      // so S_REC does not run, so an unconditional clear dropped exactly the
      // record the stall existed to protect. valid must persist until ready.
      if (rec_valid_o && rec_ready_i) rec_valid_o <= 1'b0;

      unique case (st)

        // ---- bytes [0,36): capture the header, CRC over [0,32) -------------
        S_HDR: if (take) begin
          if (pos == '0) total <= pkt_len_i;
          if (pos < 32'd32) crc_hdr <= zhao_crc32c_step(crc_hdr, pkt_byte_i);
          if      (pos < 32'd4)  h_magic     <= {pkt_byte_i, h_magic[31:8]};
          else if (pos < 32'd6)  h_abi_ver   <= {pkt_byte_i, h_abi_ver[15:8]};
          else if (pos < 32'd8)  h_flags     <= {pkt_byte_i, h_flags[15:8]};
          else if (pos < 32'd24) ;  // frame_id, sequence, epoch, deadline: not
                                    // validated here, not needed downstream
          else if (pos < 32'd28) h_cmd_count <= {pkt_byte_i, h_cmd_count[31:8]};
          else if (pos < 32'd32) h_cmd_bytes <= {pkt_byte_i, h_cmd_bytes[31:8]};
          else                   h_hdr_crc   <= {pkt_byte_i, h_hdr_crc[31:8]};

          pos <= pos + 32'd1;
          // Normally the header ends at byte 35. But a packet SHORTER than the
          // 36-byte header must still reach a verdict (BAD_LENGTH, check 1),
          // and it never delivers a 36th byte -- so leaving on pos == 35 alone
          // hangs forever waiting for a byte that does not exist. Found by
          // cmd_decoder_directed's "shorter than a header" case, which timed
          // out rather than mismatching.
          if ((pos == 32'd35) || ((pos + 32'd1) >= eff_total)) st <= S_CHECK;
        end

        // ---- checks 1-3, consuming nothing ---------------------------------
        // A separate state precisely because these must be decidable when the
        // stream STOPS: a 36-byte packet that fails check 1 never delivers
        // another byte, so a check waiting for one would hang forever.
        S_CHECK: begin
          if (hdr_bad) begin
            `ZHAO_FAIL(hdr_err)
            hdr_abort <= 1'b1;   // consumed exactly 36 bytes (spec §3.2)
            done      <= 1'b1;
            st        <= S_DONE;
          end else if (h_cmd_bytes == '0) begin
            st <= S_PCRC;        // a legal empty stream still carries its CRC
          end else begin
            st <= S_REC;
          end
        end

        // ---- the command stream --------------------------------------------
        S_REC: if (take) begin
          crc_pay <= zhao_crc32c_step(crc_pay, pkt_byte_i);
          pos     <= pos + 32'd1;

          if (rec_off < 16'd16) begin
            if      (rec_off < 16'd2)  cur_opcode <= {pkt_byte_i, cur_opcode[15:8]};
            else if (rec_off < 16'd4)  cur_bytes  <= {pkt_byte_i, cur_bytes[15:8]};
            else if (rec_off < 16'd8)  cur_source <= {pkt_byte_i, cur_source[31:8]};
            else if (rec_off < 16'd12) cur_flags  <= {pkt_byte_i, cur_flags[31:8]};
            else                       cur_rsv0   <= {pkt_byte_i, cur_rsv0[23:8]};
          end

          // The record header completes on its sixteenth byte. Checks 5, 6 and
          // 10 run here: the earliest instant every field they need exists, and
          // before one payload byte has been consumed.
          if (rec_off == 16'd15) begin
            if (rec_err != ZH_ABI_OK) begin
              // HELD, not reported: the payload CRC outranks this (see above).
              if (!rec_err_set) begin
                rec_err_l   <= rec_err;
                rec_err_set <= 1'b1;
              end
            end else begin
              rec_valid_o <= 1'b1;   // only a record that passed is reported
            end
          end

          // Record boundary. cur_bytes is trustworthy from offset 3 onward and
          // the minimum record is 16 bytes, so this is never compared against
          // an incomplete value.
          if ((rec_off >= 16'd15) && ((rec_off + 16'd1) >= cur_bytes)) begin
            rec_off <= '0;
            rec_sum <= rec_sum + 32'(cur_bytes);
            rec_idx <= rec_idx + 32'd1;
          end else begin
            rec_off <= rec_off + 16'd1;
          end

          if ((pos + 32'd1) == (32'd36 + h_cmd_bytes)) st <= S_PCRC;
        end

        // ---- the trailing payload CRC word ---------------------------------
        S_PCRC: if (take) begin
          pcrc_hist <= {pkt_byte_i, pcrc_hist[23:8]};
          pos       <= pos + 32'd1;
          if ((pos + 32'd1) == total) begin
            // 4 and 9 are decidable only here, which is exactly why records
            // cannot be retired before this instant.
            // THE NORMATIVE ORDER, restored at the one instant every input to
            // it exists: 4 (payload CRC), then the held record verdict from
            // 5/6/10, then 9 (the count laws).
            if ((~crc_pay) != {pkt_byte_i, pcrc_hist}) begin
              `ZHAO_FAIL(ZH_ABI_BAD_PAYLOAD_CRC)
            end else if (rec_err_set) begin
              `ZHAO_FAIL(rec_err_l)
            end else if (rec_sum != h_cmd_bytes) begin
              `ZHAO_FAIL(ZH_ABI_TRUNCATED)
            end else if (rec_idx != h_cmd_count) begin
              `ZHAO_FAIL(ZH_ABI_COUNT_MISMATCH)
            end
            done <= 1'b1;
            st   <= S_DONE;
          end
        end

        S_DONE: ;  // hold the verdict until reset

        default: st <= S_DONE;
      endcase
    end
  end

  `undef ZHAO_FAIL

endmodule : zhao_cmd_decoder
