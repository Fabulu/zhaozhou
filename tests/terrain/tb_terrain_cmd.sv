// tb_terrain_cmd.sv -- TERRAIN.CMD with a played HPS bridge.
//
// ---------------------------------------------------------------------------
// THE PLAYED BRIDGE HONOURS `len`, AND THAT IS NOT A DETAIL
// ---------------------------------------------------------------------------
// The composed world bench's played bridge returns EIGHT beats for every
// request whatever `len` says, because every client in it reads whole 64-byte
// bursts. This block does not: a patch list is 32-byte granular, so an odd
// record count ends on a 32-byte tail burst, and a model that answered eight
// beats there would hand the DUT 32 bytes that are not in the list — folding
// them into the CRC and then reporting a mismatch the DUT did not cause.
//
// So this model derives its beat count from `len`, the way `zhao_hps_bridge`'s
// own byte counter does. It is the difference between testing the block and
// testing the bench.
//
// ---------------------------------------------------------------------------
// EVERY STALL IS A KNOB
// ---------------------------------------------------------------------------
// Grant latency, the gap between beats, and `rec_ready` / `done_ready` from the
// C++ side. The record port is where it matters most: TERRAIN.SEQ takes one
// record when it is ready and not before, and the DUT abandons the rest of a
// burst to hold one — so a bench that never stalls the record port never
// exercises the re-request path at all.
`default_nettype none

module tb_terrain_cmd
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- the HPS image, a word at a time from the C++ side ----------------
    input  var logic        mw_en,
    input  var logic [13:0] mw_addr,   // 64-bit word index
    input  var logic [63:0] mw_data,
    input  var logic [13:0] mr_addr,
    output var logic [63:0] mr_data,

    // ---- played timing ----------------------------------------------------
    input var logic [7:0] cfg_req_latency_i,  // grant -> first beat
    input var logic [7:0] cfg_beat_gap_i,     // idle cycles between beats
    // 0 = never; 1 = `err` instead of granting burst N; 2 = `err` mid-burst N.
    input var logic [1:0]  cfg_err_mode_i,
    input var logic [15:0] cfg_err_burst_i,

    input var logic stat_clear_i,

    // ---- DUT configuration ------------------------------------------------
    input var logic [2:0]  cfg_hps_client_i,
    input var logic [31:0] cfg_epoch_i,
    input var logic [31:0] cfg_arena_base_i,
    input var logic [31:0] cfg_arena_bytes_i,

    // ---- the command ------------------------------------------------------
    input  var logic        j_valid,
    output var logic        j_ready,
    input  var logic [31:0] j_epoch,
    input  var logic [31:0] j_list_off,
    input  var logic [31:0] j_list_bytes,
    input  var logic [31:0] j_list_crc,
    input  var logic [15:0] j_patch_count,
    input  var logic [31:0] j_sequence,
    input  var logic [31:0] j_src_id,

    // ---- the frame ring ---------------------------------------------------
    output var logic        fr_start,
    output var logic [31:0] fr_epoch,
    output var logic [15:0] fr_patch_count,
    output var logic [15:0] fr_sequence,

    // ---- the record stream ------------------------------------------------
    output var logic        rec_valid,
    input  var logic        rec_ready,
    output var logic [31:0] rec_island,
    output var logic [15:0] rec_ix,
    output var logic [15:0] rec_iz,
    output var logic [63:0] rec_hps_addr,
    output var logic [31:0] rec_crc,
    output var logic [15:0] rec_flags,
    output var logic [ 7:0] rec_view_mask,
    output var logic [ 7:0] rec_priority,
    output var logic [31:0] rec_src_id,

    // ---- completion -------------------------------------------------------
    output var logic        done_valid,
    input  var logic        done_ready,
    output var logic        done_ok,
    output var logic [ 3:0] done_verdict,
    output var logic [31:0] done_src_id,
    output var logic [31:0] done_crc_seen,

    // ---- counters ---------------------------------------------------------
    output var logic [31:0] c_accepted,
    output var logic [31:0] c_refused,
    output var logic [31:0] c_records,
    output var logic [31:0] c_bytes,
    output var logic [31:0] c_crc_fails,
    output var logic [31:0] c_bridge_errs,
    output var logic        c_idle,

    // ---- what the BENCH saw -----------------------------------------------
    output var logic [31:0] bursts_seen,
    output var logic [31:0] beats_seen,
    output var logic [31:0] first_addr,
    output var logic [31:0] last_addr
);

  localparam int unsigned IMG_WORDS = 16384;
  localparam int unsigned VW = $clog2(IMG_WORDS);

  logic [63:0] hps_mem [IMG_WORDS];

  always_ff @(posedge clk) begin
    if (mw_en) hps_mem[mw_addr] <= mw_data;
    mr_data <= hps_mem[mr_addr];
  end

  // The model reads `valid`, `addr` and `len`. `write` and `client` are the
  // BRIDGE's business and this is a model of the fabric, not of the bridge --
  // named as unused rather than left to look like an oversight.
  /* verilator lint_off UNUSEDSIGNAL */
  zhao_hps_burst_req_t hps_req;
  /* verilator lint_on UNUSEDSIGNAL */
  logic                hps_grant;
  zhao_hps_burst_rsp_t hps_rsp;

  logic signed [15:0] d_rec_ix, d_rec_iz;
  assign rec_ix = d_rec_ix;
  assign rec_iz = d_rec_iz;

  zhao_terrain_cmd u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_hps_client_i (zhao_client_e'(cfg_hps_client_i)),
      .cfg_epoch_i      (cfg_epoch_i),
      .cfg_arena_base_i (cfg_arena_base_i),
      .cfg_arena_bytes_i(cfg_arena_bytes_i),

      .j_valid_i      (j_valid),
      .j_ready_o      (j_ready),
      .j_epoch_i      (j_epoch),
      .j_list_off_i   (j_list_off),
      .j_list_bytes_i (j_list_bytes),
      .j_list_crc_i   (j_list_crc),
      .j_patch_count_i(j_patch_count),
      .j_sequence_i   (j_sequence),
      .j_src_id_i     (j_src_id),

      .hps_req_o      (hps_req),
      .hps_req_grant_i(hps_grant),
      .hps_rsp_i      (hps_rsp),

      .fr_start_o      (fr_start),
      .fr_epoch_o      (fr_epoch),
      .fr_patch_count_o(fr_patch_count),
      .fr_sequence_o   (fr_sequence),

      .rec_valid_o    (rec_valid),
      .rec_ready_i    (rec_ready),
      .rec_island_o   (rec_island),
      .rec_ix_o       (d_rec_ix),
      .rec_iz_o       (d_rec_iz),
      .rec_hps_addr_o (rec_hps_addr),
      .rec_crc_o      (rec_crc),
      .rec_flags_o    (rec_flags),
      .rec_view_mask_o(rec_view_mask),
      .rec_priority_o (rec_priority),
      .rec_src_id_o   (rec_src_id),

      .done_valid_o   (done_valid),
      .done_ready_i   (done_ready),
      .done_ok_o      (done_ok),
      .done_verdict_o (done_verdict),
      .done_src_id_o  (done_src_id),
      .done_crc_seen_o(done_crc_seen),

      .sets_accepted_o  (c_accepted),
      .sets_refused_o   (c_refused),
      .records_emitted_o(c_records),
      .list_bytes_read_o(c_bytes),
      .crc_fails_o      (c_crc_fails),
      .bridge_errs_o    (c_bridge_errs),
      .idle_o           (c_idle)
  );

  // ------------------------------------------- the played bridge -----------
  logic        br_busy;
  logic [7:0]  br_wait;
  logic [3:0]  br_beat;
  logic [3:0]  br_beats;     // how many beats this burst carries, from `len`
  logic [31:0] br_base;
  logic [15:0] br_idx;       // which burst this is, for fault injection
  logic [VW-1:0] br_word;

  // THE BEAT COUNT COMES FROM `len`. See the header: eight-beats-always would
  // hand the DUT bytes that are not in the list.
  logic [3:0] beats_of_c;
  assign beats_of_c = 4'(hps_req.len[6:3]);

  assign br_word = VW'((br_base >> 3) + {28'd0, br_beat});

  logic err_grant_c, err_mid_c;
  assign err_grant_c = (cfg_err_mode_i == 2'd1) && (bursts_seen[15:0] == cfg_err_burst_i);
  assign err_mid_c   = (cfg_err_mode_i == 2'd2) && (br_idx == cfg_err_burst_i)
                       && (br_beat == 4'd1);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      br_busy     <= 1'b0;
      br_wait     <= 8'd0;
      br_beat     <= 4'd0;
      br_beats    <= 4'd0;
      br_base     <= 32'd0;
      br_idx      <= 16'd0;
      hps_grant   <= 1'b0;
      hps_rsp     <= '0;
      bursts_seen <= 32'd0;
      beats_seen  <= 32'd0;
      first_addr  <= 32'hFFFF_FFFF;
      last_addr   <= 32'd0;
    end else begin
      if (stat_clear_i) begin
        bursts_seen <= 32'd0;
        beats_seen  <= 32'd0;
        first_addr  <= 32'hFFFF_FFFF;
        last_addr   <= 32'd0;
      end
      hps_grant         <= 1'b0;
      hps_rsp.beat_valid <= 1'b0;
      hps_rsp.last       <= 1'b0;
      hps_rsp.err        <= 1'b0;

      if (!br_busy) begin
        if (hps_req.valid) begin
          bursts_seen <= (stat_clear_i ? 32'd0 : bursts_seen) + 32'd1;
          if (hps_req.addr < first_addr) first_addr <= hps_req.addr;
          if (hps_req.addr > last_addr)  last_addr  <= hps_req.addr;
          if (err_grant_c) begin
            // "nothing issued" -- the bridge's own words. No grant, one err.
            hps_rsp.err <= 1'b1;
          end else begin
            br_busy   <= 1'b1;
            hps_grant <= 1'b1;
            br_wait   <= cfg_req_latency_i;
            br_beat   <= 4'd0;
            br_beats  <= beats_of_c;
            br_base   <= hps_req.addr;
            br_idx    <= bursts_seen[15:0];
          end
        end
      end else if (br_wait != 8'd0) begin
        br_wait <= br_wait - 8'd1;
      end else if (err_mid_c) begin
        hps_rsp.err <= 1'b1;
        br_busy     <= 1'b0;
      end else begin
        hps_rsp.beat_valid <= 1'b1;
        hps_rsp.data       <= hps_mem[br_word];
        hps_rsp.last       <= (br_beat + 4'd1 == br_beats);
        br_wait            <= cfg_beat_gap_i;
        beats_seen         <= (stat_clear_i ? 32'd0 : beats_seen) + 32'd1;
        if (br_beat + 4'd1 == br_beats) br_busy <= 1'b0;
        else                            br_beat <= br_beat + 4'd1;
      end
    end
  end

endmodule
