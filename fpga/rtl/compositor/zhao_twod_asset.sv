// zhao_twod_asset.sv -- the TWOD sampler's page and palette loader: a real
// asset read, from the arena the game staged, driven by a real command.
//
// ===========================================================================
// WHY THIS BLOCK EXISTS, IN THE RULING'S OWN WORDS
// ===========================================================================
// Owner completion ruling 2026-09-22, item 3:
//
//   "This includes any required legitimate asset/palette loading producer. AN
//    OPCODE PLUS DESCRIPTORS REFERRING TO DATA THAT ONLY THE TESTBENCH CAN
//    INJECT IS NOT COMPLETION. Reuse PublishResource and the existing validated
//    resource mechanisms wherever applicable."
//
// `zhao_twod_sampler`'s page store is sixteen kilobytes of on-chip memory with
// a plain write port, `ld_page_*` / `ld_pal_*`. Until this block existed that
// port left `zhao_console_core.sv` as eleven tied-off inputs, so every texel a
// TWOD descriptor could name had to be poked in from outside the chip. A
// command that draws a sprite from texels nobody can load is not a producer.
//
// ===========================================================================
// IT READS THE HPS ARENA, NOT VRAM, AND THAT IS I17's OWN WALL
// ===========================================================================
// `zhao_console_core.sv` entry I17: "there is no TEXEL PAGE STORE in the tree
// that a (u, v) can walk into without a VRAM fill agent ... That is a real wall
// and it is why the new block carries a page store of its own rather than a
// client." The sampler owns its texels because nothing would fill them from
// SDRAM; so the loader reads the bytes where `PublishResource` already points
// -- `hps_addr_lo/hi`, the staged bytes in the HPS arena -- over the SAME
// `zhao_hps_arbiter_n` socket `MEM.UPLOAD`, `TERRAIN.PAGELOADER`, PART.STATE,
// GEOM.LOOM and FIELD already share, as a READ-ONLY client.
//
// The alternative -- a VRAM read client -- needs an arbiter index and a window
// that `spec/memory_rules.md` does not give the compositor, and would move the
// same sixteen kilobytes twice. This is the shorter road and it uses the
// mechanisms that are already validated.
//
// ===========================================================================
// EVERY PublishResource LAW IS THE EXISTING ONE
// ===========================================================================
//   * `length` a multiple of 64 -- a refusal, never a pad;
//   * `crc32c` over the staged bytes, `zhao_crc32c_fold`'s law, init all ones
//     and a final complement, exactly as TERRAIN.PAGELOADER folds it;
//   * `hps_addr` above 4 GiB REFUSED rather than narrowed -- MEM.UPLOAD's
//     `kUploadSourceUnreachable`, at the same width and for the same reason;
//   * the base 64-byte aligned, because the bridge's burst is;
//   * `epoch` checked against the open resource epoch.
//
// The one thing this block adds is a DESTINATION. `spec/cartridge.md` §4f:
// `dst_slot` 0..7 is a page-store slot of `PAGE_WORDS/8` words, 16..19 is a
// palette slot of 256 entries, and anything else is refused and counted. A
// transfer longer than its slot is refused WHOLE -- it does not walk into the
// neighbour, and it is not truncated to fit.
//
// ===========================================================================
// A FAILED CRC ZEROES WHAT IT WROTE. IT DOES NOT PUBLISH HALF A PAGE.
// ===========================================================================
// The CRC is only known at the last beat, and buffering sixteen kilobytes to
// hold the write back would cost more memory than the store it protects. So the
// words are written as they land and a BAD CRC IS FOLLOWED BY A ZEROING PASS
// over exactly the words this transfer wrote -- bounded by the slot, at most
// 1,024 writes, once, on a fault.
//
// That is the fail-safe direction and it is chosen deliberately over the two
// alternatives. Leaving the partial page would make a corrupt transfer LOOK
// LIKE ART -- "an absence that looks like a result", which is R221's exact
// refusal. Leaving the PREVIOUS page would be worse: the frame would draw
// last level's HUD with a counter quietly saying the new one failed. A zeroed
// region samples as colour 0, which is visibly nothing, and
// `crc_fails_o`/`regions_zeroed_o` say why.
//
// ===========================================================================
// THE WRITE IS NOT FRAME-GATED, AND THAT IS DECLARED RATHER THAN DISCOVERED
// ===========================================================================
// A load lands whenever the bridge answers. If a descriptor in the frame that
// is drawing samples the region being written, it sees a mix of the old and new
// words for that frame. The store is a simple dual-port memory and no cheap
// interlock exists that would not also be able to starve the loader.
//
// `loads_during_pass_o` MEASURES it -- the count of transfers that overlapped a
// live TWOD pass -- so the exposure is a number rather than a sentence. The
// safe use is the ordinary one for every asset system ever built: publish
// before the frame that draws with it. Its two operands are the loader's own
// write enable and the sampler's `frame_active_i`, which the compositor drives;
// nothing clocks both, so it is not blind in the way CLAUDE.md's metadata-bank
// detector was.

`default_nettype none

module zhao_twod_asset
  import zhao_pkg::*;
#(
    // The sampler's store, verbatim -- give this block the SAME numbers or the
    // destination map and the memory disagree.
    parameter int unsigned PAGE_WORDS = 8192,
    parameter int unsigned PAL_SLOTS  = 4,
    // Page-store slots. `spec/cartridge.md` §4f's map: eight of them.
    parameter int unsigned PAGE_SLOTS = 8,
    // The HPS client identity this block borrows. TERRAIN_BUILD is the
    // BACKGROUND class (owner ruling T3), which is what an asset install is.
    parameter zhao_client_e CLIENT = ZHAO_CLIENT_TERRAIN_BUILD
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the open resource epoch (CMD.EXEC's own `epoch` law) --------------
    input  var logic [15:0]  cfg_epoch_i,

    // ---- the request, from CMD.EXEC's PublishResource arm, kind 15 ---------
    // Field for field the same group `upl_*` carries to MEM.UPLOAD for every
    // other kind. CMD.EXEC forks by `kind`; nothing about the record changes.
    input  var logic         j_valid_i,
    output var logic         j_ready_o,
    input  var logic [23:0]  j_index_i,        // handle32[31:8]: the directory key
    input  var logic [63:0]  j_hps_addr_i,
    input  var logic [31:0]  j_len_i,          // bytes, a multiple of 64
    input  var logic [31:0]  j_crc_i,
    input  var logic [15:0]  j_epoch_i,
    input  var logic [ 7:0]  j_dst_slot_i,

    // ---- the HPS read burst -------------------------------------------------
    output var zhao_hps_burst_req_t hps_req_o,
    input  var logic                hps_grant_i,
    input  var zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- the sampler's asset write port -------------------------------------
    output var logic                              ld_page_we_o,
    output var logic [$clog2(PAGE_WORDS)-1:0]     ld_page_addr_o,
    output var logic [15:0]                       ld_page_data_o,
    output var logic                              ld_pal_we_o,
    output var logic [$clog2(PAL_SLOTS*256)-1:0]  ld_pal_addr_o,
    output var logic [15:0]                       ld_pal_data_o,

    // ---- the compositor's pass, for the overlap instrument ------------------
    input  var logic         pass_active_i,

    // ---- evidence -------------------------------------------------------------
    output var logic [31:0]  loads_started_o,
    output var logic [31:0]  loads_done_o,
    output var logic [31:0]  words_written_o,
    output var logic [31:0]  slot_refused_o,     // dst_slot outside the map
    output var logic [31:0]  len_refused_o,      // 0, not a multiple of 64, or past the slot
    output var logic [31:0]  addr_refused_o,     // above 4 GiB, or not 64-B aligned
    output var logic [31:0]  epoch_refused_o,    // a closed epoch
    output var logic [31:0]  crc_fails_o,
    output var logic [31:0]  regions_zeroed_o,
    output var logic [31:0]  bridge_errs_o,
    output var logic [31:0]  loads_during_pass_o,
    output var logic [31:0]  bursts_o
);

  localparam int unsigned PAW      = $clog2(PAGE_WORDS);
  localparam int unsigned PALAW    = $clog2(PAL_SLOTS * 256);
  localparam int unsigned SLOT_W   = PAGE_WORDS / PAGE_SLOTS;   // words per page slot
  localparam int unsigned PAL_W    = 256;                       // entries per palette slot

  // Quartus 17.0 requires an elaboration check inside `initial begin ... end`,
  // and `--lint-only` does NOT run one -- so a clean lint says nothing about
  // any of these.
  // synthesis translate_off
  initial begin
    if ((PAGE_WORDS % PAGE_SLOTS) != 0)
      $fatal(1, "zhao_twod_asset: PAGE_WORDS must divide into PAGE_SLOTS whole slots");
    if (SLOT_W < 32)
      $fatal(1, "zhao_twod_asset: a page slot below 32 words cannot hold a 64-byte burst");
    if (PAL_SLOTS < 1)
      $fatal(1, "zhao_twod_asset: PAL_SLOTS must be >= 1");
  end
  // synthesis translate_on

  // ==========================================================================
  // THE REQUEST'S VERDICT -- every clause is an existing PublishResource law
  // ==========================================================================
  logic        dst_is_pal_c;
  logic [ 7:0] dst_idx_c;
  logic        dst_ok_c;
  assign dst_is_pal_c = (j_dst_slot_i >= 8'd16);
  assign dst_idx_c    = dst_is_pal_c ? (j_dst_slot_i - 8'd16) : j_dst_slot_i;
  assign dst_ok_c     = dst_is_pal_c ? (32'(dst_idx_c) < 32'(PAL_SLOTS))
                                     : (32'(dst_idx_c) < 32'(PAGE_SLOTS));

  // Words, not bytes: the store is 16 bits wide and the record is a run of
  // little-endian u16.
  logic [31:0] words_c, cap_c;
  assign words_c = j_len_i >> 1;
  assign cap_c   = dst_is_pal_c ? 32'(PAL_W) : 32'(SLOT_W);

  logic len_ok_c, addr_ok_c, epoch_ok_c;
  assign len_ok_c   = (j_len_i != 32'd0)
                   && (j_len_i[5:0] == 6'd0)        // multiple of 64
                   && (words_c <= cap_c);
  assign addr_ok_c  = (j_hps_addr_i[63:32] == 32'd0)
                   && (j_hps_addr_i[5:0]   == 6'd0);
  assign epoch_ok_c = (j_epoch_i == cfg_epoch_i);

  // ==========================================================================
  // THE BURST ENGINE
  // ==========================================================================
  // A BURST IS COLLECTED WHOLE AND THEN DRAINED, and that is not a stylistic
  // choice. The bridge delivers one 64-bit beat PER CYCLE once granted and
  // answers `err` only at the request, never mid-burst -- so a granted read
  // runs to its last beat whatever the consumer is doing. Writing the store
  // takes four cycles per beat (four 16-bit words), so a design that drained
  // while the beats landed would DROP THREE BEATS IN FOUR with every counter
  // still balancing. The 512-bit landing buffer is the same shape
  // `zhao_geom_loomfeed` uses on the same socket, for the same reason.
  typedef enum logic [2:0] {
    S_IDLE, S_REQ, S_RD, S_DRAIN, S_FIN, S_ZERO
  } state_e;
  state_e state_q;

  logic [31:0] addr_q;        // next burst's HPS byte address
  logic [31:0] left_q;        // bytes still to READ (not yet drained)
  logic [PAW-1:0]   wbase_q;  // first page word of the destination region
  logic [PALAW-1:0] pbase_q;  // first palette entry of the destination region
  logic             is_pal_q;
  logic [31:0] widx_q;        // words written so far
  logic [31:0] wcount_q;      // words this transfer will write
  logic [31:0] crc_q;
  logic [31:0] want_crc_q;
  logic [63:0] w_q [0:7];     // the landed burst
  logic [ 2:0] beat_q;        // which beat is landing
  logic [ 4:0] drain_q;       // 0..31: which 16-bit word of the burst
  logic        overlapped_q;

  logic [31:0] fold_out_c;
  zhao_crc32c_fold u_fold (
      .c_i(crc_q),
      .d_i(hps_rsp_i.data),
      .n_i(4'd8),
      .c_o(fold_out_c)
  );

  assign hps_req_o.valid  = (state_q == S_REQ);
  assign hps_req_o.write  = 1'b0;
  assign hps_req_o.client = CLIENT;
  assign hps_req_o.addr   = addr_q;
  assign hps_req_o.len    = 7'd64;

  // A request is taken only when this block is idle. The handshake WAITS rather
  // than queues: CMD.EXEC's upload arm already owns a pending queue, and a
  // second one here would be two places a request can be waiting with no single
  // answer to "where is it".
  assign j_ready_o = (state_q == S_IDLE);

  // The drained word: beat `drain_q[4:2]`, half `drain_q[1:0]`, low half first
  // because the record is a run of LITTLE-ENDIAN u16.
  logic [63:0] drain_beat_c;
  logic [15:0] word_c;
  assign drain_beat_c = w_q[drain_q[4:2]];
  always_comb begin
    case (drain_q[1:0])
      2'd0:    word_c = drain_beat_c[15:0];
      2'd1:    word_c = drain_beat_c[31:16];
      2'd2:    word_c = drain_beat_c[47:32];
      default: word_c = drain_beat_c[63:48];
    endcase
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q <= S_IDLE;
      addr_q <= 32'd0; left_q <= 32'd0;
      wbase_q <= '0; pbase_q <= '0; is_pal_q <= 1'b0;
      widx_q <= 32'd0; wcount_q <= 32'd0;
      crc_q <= 32'hFFFF_FFFF; want_crc_q <= 32'd0;
      beat_q <= 3'd0; drain_q <= 5'd0;
      overlapped_q <= 1'b0;
      for (int i = 0; i < 8; i++) w_q[i] <= 64'd0;
      ld_page_we_o <= 1'b0; ld_page_addr_o <= '0; ld_page_data_o <= 16'd0;
      ld_pal_we_o  <= 1'b0; ld_pal_addr_o  <= '0; ld_pal_data_o  <= 16'd0;
      loads_started_o <= '0; loads_done_o <= '0; words_written_o <= '0;
      slot_refused_o <= '0; len_refused_o <= '0; addr_refused_o <= '0;
      epoch_refused_o <= '0; crc_fails_o <= '0; regions_zeroed_o <= '0;
      bridge_errs_o <= '0; loads_during_pass_o <= '0; bursts_o <= '0;
    end else begin
      ld_page_we_o <= 1'b0;
      ld_pal_we_o  <= 1'b0;

      if (pass_active_i && (state_q != S_IDLE)) overlapped_q <= 1'b1;

      case (state_q)
        // ---- accept, or refuse on one of the existing laws -----------------
        S_IDLE: begin
          if (j_valid_i) begin
            // THE ORDER IS FAIL-SAFE AND IT IS THE RECORD'S OWN: the
            // destination, then the extent, then the source, then the epoch.
            // Each counter names ONE clause, so a refusal reads back to the
            // FIELD that caused it rather than to "the loader said no".
            if (!dst_ok_c) begin
              slot_refused_o <= slot_refused_o + 32'd1;
            end else if (!len_ok_c) begin
              len_refused_o <= len_refused_o + 32'd1;
            end else if (!addr_ok_c) begin
              addr_refused_o <= addr_refused_o + 32'd1;
            end else if (!epoch_ok_c) begin
              epoch_refused_o <= epoch_refused_o + 32'd1;
            end else begin
              addr_q     <= j_hps_addr_i[31:0];
              left_q     <= j_len_i;
              is_pal_q   <= dst_is_pal_c;
              wbase_q    <= PAW'(32'(dst_idx_c) * 32'(SLOT_W));
              pbase_q    <= PALAW'(32'(dst_idx_c) * 32'(PAL_W));
              widx_q     <= 32'd0;
              wcount_q   <= words_c;
              crc_q      <= 32'hFFFF_FFFF;
              want_crc_q <= j_crc_i;
              beat_q     <= 3'd0;
              drain_q    <= 5'd0;
              overlapped_q <= pass_active_i;
              state_q    <= S_REQ;
              loads_started_o <= loads_started_o + 32'd1;
            end
          end
        end

        // ---- one 64-byte burst, held until granted or refused --------------
        S_REQ: begin
          if (hps_rsp_i.err) begin
            // The bridge refuses a malformed or colliding burst with `err` and
            // no grant. Every burst here is 64 bytes at a 64-byte-aligned
            // address -- `addr_ok_c` refuses any base that is not -- so the
            // first cause is unreachable and the second is transient. The
            // request is taken DOWN and re-offered on the next cycle rather
            // than held, because a held request the arbiter re-serves would
            // spin with every counter here frozen.
            bridge_errs_o <= bridge_errs_o + 32'd1;
          end else if (hps_grant_i) begin
            beat_q   <= 3'd0;
            state_q  <= S_RD;
            bursts_o <= bursts_o + 32'd1;
          end
        end

        // ---- the beats land, one per cycle, and are FOLDED as they land ----
        S_RD: begin
          if (hps_rsp_i.beat_valid) begin
            w_q[beat_q] <= hps_rsp_i.data;
            crc_q       <= fold_out_c;
            beat_q      <= beat_q + 3'd1;
            if (hps_rsp_i.last) begin
              drain_q <= 5'd0;
              state_q <= S_DRAIN;
            end
          end
        end

        // ---- thirty-two words out of the landed burst, one per cycle -------
        S_DRAIN: begin
          if (widx_q < wcount_q) begin
            if (is_pal_q) begin
              ld_pal_we_o   <= 1'b1;
              ld_pal_addr_o <= pbase_q + PALAW'(widx_q);
              ld_pal_data_o <= word_c;
            end else begin
              ld_page_we_o   <= 1'b1;
              ld_page_addr_o <= wbase_q + PAW'(widx_q);
              ld_page_data_o <= word_c;
            end
            widx_q          <= widx_q + 32'd1;
            words_written_o <= words_written_o + 32'd1;
          end
          if (drain_q == 5'd31) begin
            if (left_q <= 32'd64) begin
              state_q <= S_FIN;
            end else begin
              left_q  <= left_q - 32'd64;
              addr_q  <= addr_q + 32'd64;
              state_q <= S_REQ;
            end
          end else begin
            drain_q <= drain_q + 5'd1;
          end
        end

        // ---- the verdict ----------------------------------------------------
        S_FIN: begin
          if (overlapped_q) loads_during_pass_o <= loads_during_pass_o + 32'd1;
          if ((~crc_q) != want_crc_q) begin
            crc_fails_o      <= crc_fails_o + 32'd1;
            regions_zeroed_o <= regions_zeroed_o + 32'd1;
            widx_q           <= 32'd0;
            state_q          <= S_ZERO;
          end else begin
            loads_done_o <= loads_done_o + 32'd1;
            state_q      <= S_IDLE;
          end
        end

        // ---- the zeroing pass, bounded by what this transfer wrote ----------
        S_ZERO: begin
          if (is_pal_q) begin
            ld_pal_we_o   <= 1'b1;
            ld_pal_addr_o <= pbase_q + PALAW'(widx_q);
            ld_pal_data_o <= 16'd0;
          end else begin
            ld_page_we_o   <= 1'b1;
            ld_page_addr_o <= wbase_q + PAW'(widx_q);
            ld_page_data_o <= 16'd0;
          end
          if (widx_q + 32'd1 >= wcount_q) state_q <= S_IDLE;
          else                            widx_q  <= widx_q + 32'd1;
        end

        default: state_q <= S_IDLE;
      endcase
    end
  end

  /* verilator lint_off UNUSEDSIGNAL */
  // The handle's 24-bit directory INDEX travels with the record and no port in
  // this path consumes it: the destination is `dst_slot`, by cartridge 4f.
  // Carrying it to nothing would be a wire, not a check -- the same sentence
  // CMD.EXEC's upload arm makes about the handle's generation byte. It is taken
  // as a port so the seam matches `upl_*` field for field and a future
  // directory-keyed destination does not have to re-cut it.
  wire _unused_index = |j_index_i;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : zhao_twod_asset

`default_nettype wire
