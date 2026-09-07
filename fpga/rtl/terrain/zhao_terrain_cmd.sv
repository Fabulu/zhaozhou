// zhao_terrain_cmd.sv -- SubmitTerrainSet, turned into a frame.
//
// ===========================================================================
// THE LAST OF DOCKET D4
// ===========================================================================
// `reports/DOCKET.md` D4 names four things missing from the 8 km world: the
// world pager, the patch-residency manager, the composed-height cache, and the
// COMMAND -> TERRAIN PIPELINE. The first three are built and unit-verified and
// the composed suite runs them together. The fourth was nothing at all: the
// bench played `fr_start`, `fr_epoch`, `fr_patch_count` and a `rec_*` stream it
// constructed in C++, and no command a game could issue made any of it happen.
//
// This block is that pipeline's middle. Given a SubmitTerrainSet's fields, it
// fetches the sealed patch list out of the HPS staging arena, verifies it, and
// drives TERRAIN.SEQ's frame ring with the records the list contains.
//
// ===========================================================================
// THE LIST IS CAPTURE DATA, WHICH DECIDES ALMOST EVERYTHING HERE
// ===========================================================================
// Ruling T5: "The sealed list is capture data -- replay does not rerun the HPS
// visibility walk." `zref::swstream` did the walk, unioned the views, applied
// T7's prefetch policy, sorted by `canonical_less` and sealed the result.
//
// So this block does not sort, does not merge, does not deduplicate, does not
// filter by view mask and does not decide what is required. It READS BYTES IN
// ORDER. Any of those verbs here would be a second source of truth for the
// thing the determinism ledger is anchored on, and its first divergence would
// be a replay that renders differently on the machine that recorded it.
//
// ===========================================================================
// TWO PASSES, BECAUSE "VERIFY BEFORE ACTING" IS A REAL CONSTRAINT
// ===========================================================================
// T5 gives the list a CRC and the whole point of it is that a corrupt list must
// not be acted on. Acting on record 0 and discovering at record 200 that the
// CRC is wrong is not verification -- 200 loads have already been issued and
// 200 slots claimed.
//
// The alternatives are to BUFFER the list or to READ IT TWICE. Buffering is
// 32 bytes per record and T6 permits 256 composed patches, so the visible set
// can be 8,192 bytes = 65,536 bits, which is seven M10K held for one command.
// Reading twice costs 16 KB of HPS traffic per frame against the 684 KB that
// T7's 32 whole pages already cost -- 2.4% more, for a block that is not on the
// frame's critical path because the sequencer never waits on a load.
//
// So: pass one folds the CRC and emits nothing; pass two emits and folds
// nothing. A mismatch after pass one refuses the command with no record having
// been offered to anybody.
//
// ===========================================================================
// A RECORD IS EXACTLY FOUR BEATS, AND THAT IS ARITHMETIC
// ===========================================================================
// `zref::swstream::encode_record` lays the 32 bytes out little-endian in T5's
// field order, by hand rather than by memcpy -- because a struct's padding is
// the compiler's business and a capture that replays only on the machine that
// made it is not capture data. The result:
//
//     byte  0..3   island_id             u32     beat 0 [31:0]
//     byte  4..5   patch_ix              i16     beat 0 [47:32]
//     byte  6..7   patch_iz              i16     beat 0 [63:48]
//     byte  8..15  hps_page_addr         u64     beat 1
//     byte 16..19  expected_page_crc32c  u32     beat 2 [31:0]
//     byte 20..21  flags                 u16     beat 2 [47:32]
//     byte 22      view_mask             u8      beat 2 [55:48]
//     byte 23      priority              u8      beat 2 [63:56]
//     byte 24..27  source_id             u32     beat 3 [31:0]
//     byte 28..31  reserved              u32     beat 3 [63:32]
//
// 32 is four whole 64-bit beats and every field sits inside one of them. No
// field straddles a beat and no record straddles a burst, provided the list
// starts 8-byte aligned -- which is checked, because an unaligned list would
// shear every field in it and still produce plausible terrain.
//
// ===========================================================================
// WHAT IT DOES NOT DO
// ===========================================================================
// It does not decide residency, does not load pages, does not compose, and does
// not walk visibility. It does not interpret `flags`, `view_mask` or
// `priority`: those ride the record to TERRAIN.SEQ, which owns what they mean.
//
// AND IT DOES NOT DECODE THE COMMAND. Its job port takes the fields already
// unpacked, because the command seam is an open question that is not this
// block's to answer: the shell's record framer carries SIXTEEN payload bytes
// (`zhao_shell_top.sv:1712`) and SubmitTerrainSet's payload is thirty-two, so
// `patch_count` -- T5's seal -- does not reach the scheduler today. That is a
// command-path limit rather than a terrain one; `TerrainField 0x0200` is a
// 112-byte record and is truncated the same way. See
// `reports/TERRAIN-COMMAND-PIPELINE-20260907.md`. Solving it inside this block
// would be the terrain lane working around a defect that belongs to the
// command lane.
// ===========================================================================
`default_nettype none

module zhao_terrain_cmd
  import zhao_pkg::*;
#(
    // 32 bytes, ruling T5 and `zref::swstream::kRecordBytes`. A parameter so
    // the elaboration checks below have something to check, NOT a knob: a
    // different record is a different ABI.
    parameter int unsigned REC_BYTES   = 32,
    parameter int unsigned BURST_BYTES = 64,
    // T6 permits 256 live composed patches and the visible set may name more
    // than it composes. The ceiling here is a REFUSAL, not a clamp: a list
    // longer than this is reported and dropped rather than truncated into a
    // frame that looks complete.
    parameter int unsigned MAX_PATCHES = 1024
) (
    input var logic clk,
    input var logic rst_n,

    // ---- configuration -------------------------------------------------------
    input var zhao_client_e cfg_hps_client_i,
    input var logic [31:0]  cfg_epoch_i,
    input var logic [31:0]  cfg_arena_base_i,
    input var logic [31:0]  cfg_arena_bytes_i,

    // ---- the command, already unpacked ---------------------------------------
    input  var logic        j_valid_i,
    output var logic        j_ready_o,
    input  var logic [31:0] j_epoch_i,       // resource_epoch
    input  var logic [31:0] j_list_off_i,    // byte offset within the arena
    input  var logic [31:0] j_list_bytes_i,  // must be REC_BYTES * patch_count
    input  var logic [31:0] j_list_crc_i,
    input  var logic [15:0] j_patch_count_i,
    // NO `view_mask` OR `flags` PORT, AND THAT IS A FINDING RATHER THAN A
    // TRIM. T5 gives SubmitTerrainSet a SET-LEVEL `view_mask` and `flags`, and
    // nothing downstream consumes either: TERRAIN.SEQ's frame ring takes
    // {epoch, patch_count, sequence} and every record carries its OWN
    // `view_mask`. Accepting them here and dropping them would look like they
    // were handled; giving them an output nobody reads would be worse. They
    // are named in reports/TERRAIN-COMMAND-PIPELINE-20260907.md as work with
    // no consumer, which is what they are.
    input  var logic [31:0] j_sequence_i,
    input  var logic [31:0] j_src_id_i,

    // ---- MEM.HPS.BRIDGE read client ------------------------------------------
    output var zhao_hps_burst_req_t hps_req_o,
    input  var logic                hps_req_grant_i,
    input  var zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- TERRAIN.SEQ's frame ring --------------------------------------------
    output var logic        fr_start_o,
    output var logic [31:0] fr_epoch_o,
    output var logic [15:0] fr_patch_count_o,
    output var logic [15:0] fr_sequence_o,

    // ---- TERRAIN.SEQ's record port -------------------------------------------
    output var logic               rec_valid_o,
    input  var logic               rec_ready_i,
    output var logic [31:0]        rec_island_o,
    output var logic signed [15:0] rec_ix_o,
    output var logic signed [15:0] rec_iz_o,
    output var logic [63:0]        rec_hps_addr_o,
    output var logic [31:0]        rec_crc_o,
    output var logic [15:0]        rec_flags_o,
    output var logic [ 7:0]        rec_view_mask_o,
    output var logic [ 7:0]        rec_priority_o,
    output var logic [31:0]        rec_src_id_o,

    // ---- completion -----------------------------------------------------------
    // ONE COMMAND, ONE COMPLETION, ALWAYS -- the rule TERRAIN.PAGELOADER's
    // contract states. A refusal that produced silence would leave whoever
    // issued the command waiting on a frame that will never start.
    output var logic        done_valid_o,
    input  var logic        done_ready_i,
    output var logic        done_ok_o,
    output var logic [ 3:0] done_verdict_o,
    output var logic [31:0] done_src_id_o,
    output var logic [31:0] done_crc_seen_o,

    // ---- counters -------------------------------------------------------------
    output var logic [31:0] sets_accepted_o,
    output var logic [31:0] sets_refused_o,
    output var logic [31:0] records_emitted_o,
    output var logic [31:0] list_bytes_read_o,
    output var logic [31:0] crc_fails_o,
    output var logic [31:0] bridge_errs_o,
    output var logic        idle_o
);

  localparam int unsigned BEATS_PER_REC = REC_BYTES / 8;          // 4
  localparam int unsigned BEATS_PER_BST = BURST_BYTES / 8;        // 8

  localparam logic [3:0] V_OK        = 4'd0;
  localparam logic [3:0] V_EPOCH     = 4'd1;   // resource_epoch != live epoch
  localparam logic [3:0] V_LEN       = 4'd2;   // list_bytes != 32 * patch_count
  localparam logic [3:0] V_COUNT     = 4'd3;   // patch_count > MAX_PATCHES
  localparam logic [3:0] V_ALIGN     = 4'd4;   // list not 8-byte aligned
  localparam logic [3:0] V_UNREACH   = 4'd5;   // list runs outside the arena
  localparam logic [3:0] V_CRC       = 4'd6;   // list_crc32c mismatch
  localparam logic [3:0] V_BRIDGE    = 4'd7;   // bridge reported err
  localparam logic [3:0] V_EMPTY     = 4'd8;   // patch_count == 0
  // T5's `sequence` is a u32 and TERRAIN.SEQ's frame ring carries SIXTEEN
  // bits. Truncating would put two different frames on the same ring sequence
  // and the determinism ledger would never see it, so a sequence that does not
  // fit is REFUSED. Which of the two widths is wrong is not this block's to
  // decide; it is listed as an open ruling.
  localparam logic [3:0] V_SEQ       = 4'd9;

`ifndef SYNTHESIS
  initial begin
    if (REC_BYTES != 32)
      $fatal(1, "terrain_cmd: T5's record is 32 bytes; REC_BYTES=%0d is a different ABI", REC_BYTES);
    if ((BURST_BYTES % REC_BYTES) != 0)
      $fatal(1, "terrain_cmd: a burst must hold a whole number of records");
    // The beat assembler indexes records with a 2-bit counter, so a record that
    // is not four beats would wrap it silently and shear every field.
    if (BEATS_PER_REC != 4)
      $fatal(1, "terrain_cmd: a record must be four 64-bit beats, not %0d", BEATS_PER_REC);
  end
`endif

  // ---- the job, captured at acceptance -------------------------------------
  logic [31:0] job_off_q, job_bytes_q, job_crc_q, job_src_q;
  // SIXTEEN BITS, because that is what TERRAIN.SEQ's ring carries, and the
  // upper half of T5's u32 is refused at S_CHECK rather than stored and
  // ignored. A 32-bit register here whose top half nothing reads is how a
  // truncation gets called a field.
  logic [15:0] job_seq_q;
  logic [15:0] job_count_q;
  logic [31:0] job_epoch_q;

  // ---- the walk ------------------------------------------------------------
  logic        pass_q;          // 0 = fold the CRC, 1 = emit the records
  logic [31:0] byte_q;          // bytes consumed so far in THIS pass
  logic [31:0] crc_q;
  logic [15:0] emitted_q;
  logic [ 1:0] rbeat_q;         // which beat of the current record
  logic [63:0] rec_b0_q, rec_b1_q, rec_b2_q;
  logic [ 3:0] verdict_q;
  logic [31:0] crc_seen_q;

  logic [$clog2(BEATS_PER_BST+1)-1:0] beat_q;

  typedef enum logic [3:0] {
    S_IDLE,
    S_CHECK,
    S_REQ,
    S_BEAT,
    S_HOLD,     // a record is assembled and waiting for TERRAIN.SEQ
    S_PASS2,
    S_FRAME,    // fr_start pulse
    S_DONE
  } state_e;

  state_e state_q;

  // ---- CRC folding ---------------------------------------------------------
  logic [31:0] fold_out;
  zhao_crc32c_fold u_fold (
      .c_i(crc_q),
      .d_i(hps_rsp_i.data),
      .n_i(4'd8),
      .c_o(fold_out)
  );

  // ---- pre-checks, in a fixed order ----------------------------------------
  // ORDER IS PART OF THE CONTRACT, the same way TERRAIN.PAGELOADER's pre-verdict
  // order is: a command that is both stale and misaligned must report ONE
  // verdict and always the same one, or a test cannot assert against it.
  logic pre_epoch_bad_c, pre_empty_c, pre_count_bad_c, pre_len_bad_c;
  logic pre_align_bad_c, pre_unreach_c, pre_seq_bad_c;
  logic [63:0] end_off_c;

  assign pre_epoch_bad_c = (j_epoch_i != cfg_epoch_i);
  assign pre_empty_c     = (j_patch_count_i == 16'd0);
  assign pre_count_bad_c = (32'({16'd0, j_patch_count_i}) > 32'(MAX_PATCHES));
  // 32 * patch_count with no multiplier: REC_BYTES is 32, so it is a shift.
  assign pre_len_bad_c   = (j_list_bytes_i != ({16'd0, j_patch_count_i} << 5));
  assign pre_align_bad_c = (j_list_off_i[2:0] != 3'd0);
  assign end_off_c       = {32'd0, j_list_off_i} + {32'd0, j_list_bytes_i};
  assign pre_unreach_c   = (end_off_c > {32'd0, cfg_arena_bytes_i});
  assign pre_seq_bad_c   = (j_sequence_i[31:16] != 16'd0);

  // ---- the bridge request --------------------------------------------------
  // The remaining bytes of this pass, capped at one burst. The tail burst is
  // SHORT rather than over-reading: the list is 32-byte granular and a burst is
  // 64, so a list with an odd record count ends on a 32-byte burst. Reading the
  // extra 32 bytes would fold bytes that are not in the list into the CRC.
  logic [31:0] left_c;
  // SEVEN BITS, which is the width of the bridge's `len` field. Carrying a
  // 32-bit "bytes this burst" and then truncating it at the port is how a
  // burst length silently becomes 0 for a 128-byte remainder.
  logic [6:0]  this_bytes_c;
  assign left_c       = job_bytes_q - byte_q;
  assign this_bytes_c = (left_c > 32'(BURST_BYTES)) ? 7'(BURST_BYTES) : 7'(left_c);

  always_comb begin
    hps_req_o        = '0;
    hps_req_o.valid  = (state_q == S_REQ);
    hps_req_o.write  = 1'b0;
    hps_req_o.client = cfg_hps_client_i;
    hps_req_o.addr   = cfg_arena_base_i + job_off_q + byte_q;
    hps_req_o.len    = this_bytes_c;
  end

  assign j_ready_o = (state_q == S_IDLE);
  assign idle_o    = (state_q == S_IDLE);

  // fr_start is a ONE-CYCLE PULSE and it fires BEFORE the first record, not
  // with it: TERRAIN.SEQ latches the frame's epoch, count and sequence on the
  // pulse and only then opens `rec_ready_o`.
  assign fr_start_o       = (state_q == S_FRAME);
  assign fr_epoch_o       = job_epoch_q;
  assign fr_patch_count_o = job_count_q;
  assign fr_sequence_o    = job_seq_q;

  assign rec_valid_o     = (state_q == S_HOLD);
  assign rec_island_o    = rec_b0_q[31:0];
  assign rec_ix_o        = signed'(rec_b0_q[47:32]);
  assign rec_iz_o        = signed'(rec_b0_q[63:48]);
  assign rec_hps_addr_o  = rec_b1_q;
  assign rec_crc_o       = rec_b2_q[31:0];
  assign rec_flags_o     = rec_b2_q[47:32];
  assign rec_view_mask_o = rec_b2_q[55:48];
  assign rec_priority_o  = rec_b2_q[63:56];
  // Beat 3's low word, taken straight off the bridge on the cycle the record
  // completes -- it is the last beat, so there is nothing to register it into
  // that would not be read on the same cycle anyway.
  logic [31:0] rec_src_q;
  assign rec_src_id_o    = rec_src_q;

  assign done_valid_o    = (state_q == S_DONE);
  assign done_ok_o       = (verdict_q == V_OK);
  assign done_verdict_o  = verdict_q;
  assign done_src_id_o   = job_src_q;
  assign done_crc_seen_o = crc_seen_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q           <= S_IDLE;
      pass_q            <= 1'b0;
      byte_q            <= 32'd0;
      crc_q             <= 32'hFFFF_FFFF;
      crc_seen_q        <= 32'd0;
      emitted_q         <= 16'd0;
      rbeat_q           <= 2'd0;
      rec_b0_q          <= 64'd0;
      rec_b1_q          <= 64'd0;
      rec_b2_q          <= 64'd0;
      rec_src_q         <= 32'd0;
      beat_q            <= '0;
      verdict_q         <= V_OK;
      job_off_q         <= 32'd0;
      job_bytes_q       <= 32'd0;
      job_crc_q         <= 32'd0;
      job_seq_q         <= 16'd0;
      job_src_q         <= 32'd0;
      job_count_q       <= 16'd0;
      job_epoch_q       <= 32'd0;
      sets_accepted_o   <= 32'd0;
      sets_refused_o    <= 32'd0;
      records_emitted_o <= 32'd0;
      list_bytes_read_o <= 32'd0;
      crc_fails_o       <= 32'd0;
      bridge_errs_o     <= 32'd0;
    end else begin
      unique case (state_q)

        S_IDLE: begin
          if (j_valid_i) begin
            job_epoch_q <= j_epoch_i;
            job_off_q   <= j_list_off_i;
            job_bytes_q <= j_list_bytes_i;
            job_crc_q   <= j_list_crc_i;
            job_count_q <= j_patch_count_i;
            job_seq_q   <= j_sequence_i[15:0];
            job_src_q   <= j_src_id_i;
            pass_q      <= 1'b0;
            byte_q      <= 32'd0;
            crc_q       <= 32'hFFFF_FFFF;
            emitted_q   <= 16'd0;
            rbeat_q     <= 2'd0;
            state_q     <= S_CHECK;
          end
        end

        // Every refusal happens HERE, before a byte is read. A command that is
        // going to be refused must not have issued a bridge burst: otherwise
        // `bridge_errs_o` and `list_bytes_read_o` measure this block's
        // bookkeeping instead of the fabric.
        S_CHECK: begin
          if (pre_epoch_bad_c) begin
            verdict_q      <= V_EPOCH;
            sets_refused_o <= sets_refused_o + 32'd1;
            state_q        <= S_DONE;
          end else if (pre_empty_c) begin
            verdict_q      <= V_EMPTY;
            sets_refused_o <= sets_refused_o + 32'd1;
            state_q        <= S_DONE;
          end else if (pre_count_bad_c) begin
            verdict_q      <= V_COUNT;
            sets_refused_o <= sets_refused_o + 32'd1;
            state_q        <= S_DONE;
          end else if (pre_len_bad_c) begin
            verdict_q      <= V_LEN;
            sets_refused_o <= sets_refused_o + 32'd1;
            state_q        <= S_DONE;
          end else if (pre_align_bad_c) begin
            verdict_q      <= V_ALIGN;
            sets_refused_o <= sets_refused_o + 32'd1;
            state_q        <= S_DONE;
          end else if (pre_unreach_c) begin
            verdict_q      <= V_UNREACH;
            sets_refused_o <= sets_refused_o + 32'd1;
            state_q        <= S_DONE;
          end else if (pre_seq_bad_c) begin
            verdict_q      <= V_SEQ;
            sets_refused_o <= sets_refused_o + 32'd1;
            state_q        <= S_DONE;
          end else begin
            verdict_q <= V_OK;
            state_q   <= S_REQ;
          end
        end

        S_REQ: begin
          // `err` ON THE REQUEST CHANNEL, WHICH THE FIRST VERSION IGNORED.
          // The bridge's own comment is "malformed burst / bridge error:
          // NOTHING ISSUED" -- so `err` here is a refusal of the request, not a
          // fault in a transfer, and it arrives with no grant. Watching for it
          // only in S_BEAT meant the pulse landed while this state was still
          // waiting, was missed, and the bench's next cycle granted normally:
          // the command completed happily and the directed suite reported eight
          // records where it wanted none. Found by the test, not by reading the
          // contract, which is the argument for firing every verdict.
          if (hps_rsp_i.err) begin
            bridge_errs_o <= bridge_errs_o + 32'd1;
            verdict_q     <= V_BRIDGE;
            state_q       <= S_DONE;
          end else if (hps_req_grant_i) begin
            beat_q  <= '0;
            state_q <= S_BEAT;
          end
        end

        S_BEAT: begin
          if (hps_rsp_i.err) begin
            bridge_errs_o <= bridge_errs_o + 32'd1;
            verdict_q     <= V_BRIDGE;
            state_q       <= S_DONE;
          end else if (hps_rsp_i.beat_valid) begin
            list_bytes_read_o <= list_bytes_read_o + 32'd8;

            if (!pass_q) begin
              // PASS ONE: fold, emit nothing.
              crc_q <= fold_out;
            end else begin
              // PASS TWO: assemble. `rbeat_q` says which beat of the record
              // this is; beat 3 completes it and the record is offered.
              unique case (rbeat_q)
                2'd0: rec_b0_q <= hps_rsp_i.data;
                2'd1: rec_b1_q <= hps_rsp_i.data;
                2'd2: rec_b2_q <= hps_rsp_i.data;
                default: rec_src_q <= hps_rsp_i.data[31:0];
              endcase
            end

            byte_q <= byte_q + 32'd8;

            if (pass_q && (rbeat_q == 2'd3)) begin
              rbeat_q <= 2'd0;
              // THE BURST STOPS FOR THE CONSUMER. TERRAIN.SEQ may not be ready,
              // and the bridge has no backpressure on its beat stream -- so the
              // record is held and the REST OF THE BURST IS ABANDONED, then
              // re-requested from the byte after this record. Wasteful by one
              // record's worth of beats at most, and the alternative is a skid
              // buffer for a stall that only happens at a record boundary.
              state_q <= S_HOLD;
            end else begin
              if (pass_q) rbeat_q <= rbeat_q + 2'd1;

              if (hps_rsp_i.last) begin
                if ((byte_q + 32'd8) >= job_bytes_q) begin
                  // END OF THE PASS.
                  if (!pass_q) begin
                    state_q <= S_PASS2;
                  end else begin
                    sets_accepted_o <= sets_accepted_o + 32'd1;
                    state_q         <= S_DONE;
                  end
                end else begin
                  state_q <= S_REQ;
                end
              end else begin
                beat_q <= beat_q + ($clog2(BEATS_PER_BST+1))'(1);
              end
            end
          end
        end

        S_HOLD: begin
          if (rec_ready_i) begin
            records_emitted_o <= records_emitted_o + 32'd1;
            emitted_q         <= emitted_q + 16'd1;
            if (byte_q >= job_bytes_q) begin
              sets_accepted_o <= sets_accepted_o + 32'd1;
              state_q         <= S_DONE;
            end else begin
              state_q <= S_REQ;
            end
          end
        end

        // THE VERIFICATION, and the only place the command can still be
        // refused after a byte has been read. Nothing has been offered to
        // TERRAIN.SEQ at this point -- that is the whole reason for two passes.
        S_PASS2: begin
          crc_seen_q <= ~crc_q;
          if ((~crc_q) != job_crc_q) begin
            crc_fails_o    <= crc_fails_o + 32'd1;
            sets_refused_o <= sets_refused_o + 32'd1;
            verdict_q      <= V_CRC;
            state_q        <= S_DONE;
          end else begin
            pass_q  <= 1'b1;
            byte_q  <= 32'd0;
            rbeat_q <= 2'd0;
            state_q <= S_FRAME;
          end
        end

        S_FRAME: state_q <= S_REQ;

        S_DONE: if (done_ready_i) state_q <= S_IDLE;

        default: state_q <= S_IDLE;
      endcase
    end
  end

`ifndef SYNTHESIS
  // ENFORCED-BY: tests/terrain/terrain_cmd_rtl_directed.cpp
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_no_record_before_frame :
      assert (!(rec_valid_o && !pass_q))
      else $error("terrain_cmd: offered a record during the CRC pass");

      a_frame_once :
      assert (!(fr_start_o && (emitted_q != 16'd0)))
      else $error("terrain_cmd: frame started after records had gone out");

      a_count_bounded :
      assert (emitted_q <= job_count_q)
      else $error("terrain_cmd: emitted %0d records for a count of %0d",
                  emitted_q, job_count_q);
    end
  end
`endif

endmodule
