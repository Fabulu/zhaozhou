// tb_field_loader.sv -- `zhao_field_loader`'s ports flattened, with a PLAYED
// MEM.HPS.BRIDGE holding the capsule bytes.
//
// ---------------------------------------------------------------------------
// WHY A WRAPPER AT ALL
// ---------------------------------------------------------------------------
// `hps_req_o` and `hps_rsp_i` are packed structs (`zhao_hps_burst_req_t`), and
// a C++ driver that pokes at a flattened struct word is one rename away from
// setting the wrong field silently. `tests/terrain/tb_pageloader.sv` solved
// this for the other bridge client and this file copies its shape: the bridge
// is played HERE, in SystemVerilog, against a backing memory the C++ fills a
// word at a time, and the C++ never sees a struct.
//
// ---------------------------------------------------------------------------
// THE BYTES GO IN THE LONG WAY, ON PURPOSE
// ---------------------------------------------------------------------------
// Owner directive section 20.5: "A testbench secretly writing the engine's
// private uop/table/scalar ports is not that path." So `mw_*` writes the
// PLAYED HPS MEMORY and nothing else. There is no port on this wrapper that
// reaches the loader's catalogue, its staging object or its CRC accumulator.
// The only way a capsule becomes a READY object is for the loader to ask the
// bridge for every one of its bytes.
//
// ---------------------------------------------------------------------------
// FAULTS ARE FIRST-CLASS
// ---------------------------------------------------------------------------
// FT052 needs an early `last`, a missing beat, an extra beat and an `err` to
// be four DISTINGUISHABLE stimuli, not one "the burst went wrong". Each is its
// own knob, each names the burst index it applies to, and `cfg_fault_burst_i`
// of -1 (all ones) disables it. FT053 needs a grant that never comes, which is
// `cfg_withhold_grant_i`.
//
// A NOTE ON WHAT THE PLAYED BRIDGE RETURNS OUTSIDE THE IMAGE: 8'hA5, not zero.
// A loader that read past the capsule and got zeros could pass a length check
// by accident; a distinctive poison byte makes an over-read show up in the CRC
// immediately. That is the same reasoning `tb_pageloader.sv` gives for poisoned
// scratch.
`default_nettype none

module tb_field_loader
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- the played HPS memory, written a 64-bit word at a time ------------
    input var logic        mw_en,
    input var logic [12:0] mw_addr,     // 64-bit word index
    input var logic [63:0] mw_data,
    input var logic [31:0] cfg_hps_window_base_i,

    // ---- played-bridge faults ---------------------------------------------
    input var logic       cfg_withhold_grant_i,
    input var logic [7:0] cfg_err_burst_i,         // 8'hFF = never
    input var logic [7:0] cfg_early_last_burst_i,  // 8'hFF = never
    input var logic [7:0] cfg_drop_beat_burst_i,   // 8'hFF = never
    input var logic [7:0] cfg_extra_beat_burst_i,  // 8'hFF = never

    // ---- loader configuration ---------------------------------------------
    input var logic [31:0] cfg_epoch_i,
    input var logic [31:0] cfg_stage_base_i,
    input var logic [31:0] cfg_stage_bytes_i,

    // ---- FH2 request -------------------------------------------------------
    input  var logic        fh2_valid_i,
    output var logic        fh2_ready_o,
    input  var logic [1:0]  fh2_kind_i,
    input  var logic [7:0]  fh2_verb_i,
    input  var logic [31:0] fh2_data0_i,   // data[31:0]
    input  var logic [31:0] fh2_data1_i,   // data[63:32]
    input  var logic [31:0] fh2_data2_i,   // data[95:64]
    input  var logic [31:0] fh2_hash_i,
    input  var logic [31:0] fh2_ticket_i,
    input  var logic [31:0] fh2_plan_i,

    // ---- FH2 reply ---------------------------------------------------------
    output var logic        fh2_resp_valid_o,
    input  var logic        fh2_resp_ready_i,
    output var logic        fh2_resp_ok_o,
    output var logic [3:0]  fh2_resp_verdict_o,
    output var logic [31:0] fh2_resp_ticket_o,
    output var logic [31:0] fh2_resp_plan_o,
    output var logic [31:0] fh2_resp_handle_o,
    output var logic [2:0]  fh2_resp_slot_o,
    output var logic        fh2_resp_evicted_o,

    // ---- publication -------------------------------------------------------
    output var logic [7:0]  pub_ready_o,
    output var logic [7:0]  pub_pinned_o,
    input  var logic [2:0]  pub_sel_i,
    output var logic [31:0] pub_handle_o,
    output var logic [31:0] pub_prog_hash_o,
    output var logic [7:0]  pub_gen_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] installs_ok_o,
    output var logic [31:0] installs_failed_o,
    output var logic [31:0] binds_ok_o,
    output var logic [31:0] binds_failed_o,
    output var logic [31:0] controls_ok_o,
    output var logic [31:0] bad_operation_o,
    output var logic [31:0] bad_envelope_o,
    output var logic [31:0] bad_range_o,
    output var logic [31:0] bad_section_o,
    output var logic [31:0] bad_crc_o,
    output var logic [31:0] bad_meta_o,
    output var logic [31:0] bridge_errs_o,
    output var logic [31:0] no_capacity_o,
    output var logic [31:0] evictions_o,
    output var logic [31:0] hint_overrides_o,
    output var logic [31:0] load_bytes_o,

    // ---- what the played bridge saw, so the C++ can cross-check ------------
    output var logic [31:0] tb_bursts_o,
    output var logic [31:0] tb_beats_o
);

  localparam int unsigned MEMW = 8192;   // 64-bit words: 64 KiB of staging
  localparam int unsigned BEATS = 8;     // 64-byte burst

  // ==========================================================================
  // THE PLAYED HPS MEMORY
  // ==========================================================================
  logic [63:0] mem [0:MEMW-1];

  always_ff @(posedge clk) begin
    if (mw_en) mem[mw_addr] <= mw_data;
  end

  // ==========================================================================
  // THE PLAYED BRIDGE
  // ==========================================================================
  // The played bridge reads `valid` and `addr`; `write`, `client` and `len`
  // are the loader's to get right and are checked by the LOADER's own lint,
  // not by this model. A wrapper that consumed them would be asserting on the
  // bench's opinion of the port rather than on the block.
  /* verilator lint_off UNUSEDSIGNAL */
  zhao_hps_burst_req_t req_w;
  /* verilator lint_on UNUSEDSIGNAL */
  logic                grant_c;
  zhao_hps_burst_rsp_t rsp_r;

  logic        in_burst;
  logic [3:0]  beat;
  logic [7:0]  burst_idx;
  logic [31:0] burst_addr;

  // Word index of the beat currently being served. The add is sized
  // explicitly: `beat` is 4 bits and the shift makes the operand 7, so an
  // unsized concatenation widens the sum to 33 and -Wall reports the truncation
  // rather than letting it happen quietly.
  wire [31:0] byte_addr = burst_addr + 32'({beat, 3'd0});
  wire [31:0] word_idx  = (byte_addr - cfg_hps_window_base_i) >> 3;
  wire        in_window = (byte_addr >= cfg_hps_window_base_i) &&
                          (word_idx < 32'(MEMW));
  // Poison outside the image: an over-read must be LOUD, not plausibly zero.
  wire [63:0] word_c = in_window ? mem[word_idx[12:0]] : 64'hA5A5_A5A5_A5A5_A5A5;

  assign grant_c = req_w.valid && !in_burst && !cfg_withhold_grant_i;

  wire this_err   = (cfg_err_burst_i        != 8'hFF) && (burst_idx == cfg_err_burst_i);
  wire this_early = (cfg_early_last_burst_i != 8'hFF) && (burst_idx == cfg_early_last_burst_i);
  wire this_drop  = (cfg_drop_beat_burst_i  != 8'hFF) && (burst_idx == cfg_drop_beat_burst_i);
  wire this_extra = (cfg_extra_beat_burst_i != 8'hFF) && (burst_idx == cfg_extra_beat_burst_i);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      in_burst   <= 1'b0;
      beat       <= 4'd0;
      burst_idx  <= 8'd0;
      burst_addr <= 32'd0;
      rsp_r      <= '0;
      tb_bursts_o <= 32'd0;
      tb_beats_o  <= 32'd0;
    end else begin
      rsp_r <= '0;

      // BURST INDEX IS PER-TRANSACTION, NOT PER-RESET. It was per-reset in the
      // first version of this file and that made every fault knob a no-op
      // after the first few installs: `cfg_err_burst_i = 2` matched burst 2 of
      // the SESSION, which had gone by long before. The tell was FT052
      // reporting a clean install under an injected fault -- a fault that
      // never fires reads exactly like a block that survived it, which is the
      // flattering direction.
      if (fh2_valid_i && fh2_ready_o) burst_idx <= 8'd0;

      if (grant_c) begin
        in_burst   <= 1'b1;
        beat       <= 4'd0;
        burst_addr <= req_w.addr;
        if (tb_bursts_o != 32'hFFFF_FFFF) tb_bursts_o <= tb_bursts_o + 32'd1;
      end else if (in_burst) begin
        if (this_err) begin
          // A BRIDGE ERROR: nothing was issued. No beat, no data.
          rsp_r.err <= 1'b1;
          in_burst  <= 1'b0;
          burst_idx <= burst_idx + 8'd1;
        end else if (this_drop && (beat == 4'd3)) begin
          // A MISSING BEAT: the bridge simply skips it. The burst then runs out
          // of beats before `last`, and the loader must not invent the gap.
          beat <= beat + 4'd1;
        end else begin
          rsp_r.beat_valid <= 1'b1;
          rsp_r.data       <= word_c;
          if (tb_beats_o != 32'hFFFF_FFFF) tb_beats_o <= tb_beats_o + 32'd1;

          if (this_early && (beat == 4'd2)) begin
            rsp_r.last <= 1'b1;
            in_burst   <= 1'b0;
            burst_idx  <= burst_idx + 8'd1;
          end else if (this_extra) begin
            // ONE BEAT TOO MANY: `last` is deferred past the burst's length.
            if (beat >= 4'(BEATS)) begin
              rsp_r.last <= 1'b1;
              in_burst   <= 1'b0;
              burst_idx  <= burst_idx + 8'd1;
            end
            beat <= beat + 4'd1;
          end else if (beat == 4'(BEATS - 1)) begin
            rsp_r.last <= 1'b1;
            in_burst   <= 1'b0;
            burst_idx  <= burst_idx + 8'd1;
          end else begin
            beat <= beat + 4'd1;
          end
        end
      end
    end
  end

  // ==========================================================================
  // THE DUT
  // ==========================================================================
  zhao_field_loader #(
      .OBJECTS(8),
      .OBJW(3),
      .GENW(8),
      .BURST_BYTES(64),
      .MAX_CAPSULE_BYTES(65536),
      .MAX_SECTIONS(24),
      .CHECK_EPOCH(1'b1)
  ) u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_hps_client_i (ZHAO_CLIENT_ENGINE1),
      .cfg_epoch_i      (cfg_epoch_i),
      .cfg_stage_base_i (cfg_stage_base_i),
      .cfg_stage_bytes_i(cfg_stage_bytes_i),

      .fh2_valid_i (fh2_valid_i),
      .fh2_ready_o (fh2_ready_o),
      .fh2_kind_i  (fh2_kind_i),
      .fh2_verb_i  (fh2_verb_i),
      .fh2_data_i  ({fh2_data2_i, fh2_data1_i, fh2_data0_i}),
      .fh2_hash_i  (fh2_hash_i),
      .fh2_ticket_i(fh2_ticket_i),
      .fh2_plan_i  (fh2_plan_i),

      .fh2_resp_valid_o  (fh2_resp_valid_o),
      .fh2_resp_ready_i  (fh2_resp_ready_i),
      .fh2_resp_ok_o     (fh2_resp_ok_o),
      .fh2_resp_verdict_o(fh2_resp_verdict_o),
      .fh2_resp_ticket_o (fh2_resp_ticket_o),
      .fh2_resp_plan_o   (fh2_resp_plan_o),
      .fh2_resp_handle_o (fh2_resp_handle_o),
      .fh2_resp_slot_o   (fh2_resp_slot_o),
      .fh2_resp_evicted_o(fh2_resp_evicted_o),

      .hps_req_o      (req_w),
      .hps_req_grant_i(grant_c),
      .hps_rsp_i      (rsp_r),

      .pub_ready_o    (pub_ready_o),
      .pub_pinned_o   (pub_pinned_o),
      .pub_sel_i      (pub_sel_i),
      .pub_handle_o   (pub_handle_o),
      .pub_prog_hash_o(pub_prog_hash_o),
      .pub_gen_o      (pub_gen_o),

      .installs_ok_o    (installs_ok_o),
      .installs_failed_o(installs_failed_o),
      .binds_ok_o       (binds_ok_o),
      .binds_failed_o   (binds_failed_o),
      .controls_ok_o    (controls_ok_o),
      .bad_operation_o  (bad_operation_o),
      .bad_envelope_o   (bad_envelope_o),
      .bad_range_o      (bad_range_o),
      .bad_section_o    (bad_section_o),
      .bad_crc_o        (bad_crc_o),
      .bad_meta_o       (bad_meta_o),
      .bridge_errs_o    (bridge_errs_o),
      .no_capacity_o    (no_capacity_o),
      .evictions_o      (evictions_o),
      .hint_overrides_o (hint_overrides_o),
      .load_bytes_o     (load_bytes_o)
  );

endmodule

`default_nettype wire
