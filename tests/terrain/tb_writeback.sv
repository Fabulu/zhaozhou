// tb_writeback.sv - TERRAIN.WRITEBACK's ports flattened, with a played MEM.GUARD
// read path, a played MEM.HPS.BRIDGE write path, and the REAL MEM.GUARD both
// watching and being asked.
//
// ---------------------------------------------------------------------------
// THE REAL GUARD REFUSES THIS BLOCK TODAY, AND THAT IS THE EVIDENCE
// ---------------------------------------------------------------------------
// `zhao_mem_guard` gives TERRAIN.BUILD one window -- TERRAIN.PAGE_POOL,
// WRITE-ONLY UNTIL 2026-09-06 -- and this block READS that pool. The read arm
// (`terrain_rd_ok`) landed with this block, so the observer's counts INVERTED:
// it now passes all 130 and refuses none, where it refused all 130 before.
// So `u_real_guard`, watching the
// DUT's own request wires, must refuse every single one, and
// `shadow_viol_count == shadow_req_count` with `shadow_ok_count == 0` is this
// bench's evidence for the amendment the contract asks for. It is the exact
// shape `tb_pageloader.sv` used before the write window landed: "until 2026-09-06
// the guard had no window that admitted a write to bank 2 for anybody, so
// `shadow_ok_seen` staying ZERO was this file's evidence for the amendment."
//
// When the read arm lands, these two checks invert -- and that inversion is the
// acceptance test for the amendment, which is why they are counts and not a
// sticky bit. A sticky "it refused one" can say a refusal happened but never
// that the RIGHT NUMBER happened, and this tree's own record is that a machine
// doing its work twice produces byte-identical output.
//
// ---------------------------------------------------------------------------
// WHY THE GUARD IS PLAYED **AND** INSTANTIATED
// ---------------------------------------------------------------------------
// `tools/rtl/check_guard_verdict.py` records what happened on 2026-09-06: two
// geometry fetchers treated every guard PASS as a denial for months, because
// every bench that played a guard raised `ready` and `ok` together. So the
// played guard here is written from `zhao_mem_guard.sv` line by line: `ready` is
// the LEVEL `!fwd_active`, and `ok` / `violation` are REGISTERED PULSES one cycle
// after the accept. They are never both high.
//
// ---------------------------------------------------------------------------
// STALLS ARE FIRST-CLASS, NOT AN AFTERTHOUGHT
// ---------------------------------------------------------------------------
// A sibling block passed 21 checks over every input it had and still dropped
// answers, because every phase held the consumer's ready high. Every stall this
// block can face is a knob here: how long the guard holds `ready` low after an
// accept, the latency to the first read beat and the gap between them, the
// bridge's grant latency, and the pacing of accepted write beats. `wb_ready`
// and `done_ready` are driven from the C++ side and can be held low for as long
// as a test likes.
`default_nettype none

module tb_writeback
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- backing memories, written a word at a time by the C++ side --------
    input  var logic        mw_en,
    input  var logic        mw_sel,    // 0 = the page-pool image, 1 = the journal
    input  var logic [15:0] mw_addr,   // 64-bit word index
    input  var logic [63:0] mw_data,
    input  var logic        mr_sel,
    input  var logic [15:0] mr_addr,
    output var logic [63:0] mr_data,

    // ---- where the played memories start ----------------------------------
    input var logic [31:0] cfg_vram_window_base_i,   // page-pool image origin
    input var logic [31:0] cfg_hps_window_base_i,    // journal image origin

    // ---- played timing -----------------------------------------------------
    input var logic [7:0] cfg_grant_hold_i,   // guard `ready` low after accept
    input var logic [7:0] cfg_rd_latency_i,   // accept -> first read beat
    input var logic [7:0] cfg_rd_gap_i,       // idle cycles between read beats
    input var logic [7:0] cfg_wr_latency_i,   // bridge grant -> first wready
    input var logic [7:0] cfg_wr_gap_i,       // idle cycles between write beats

    // ---- fault injection ---------------------------------------------------
    input var logic        cfg_region_ok_i,   // played guard's blanket verdict
    // 0 = never; 1 = refuse guard request number cfg_deny_idx_i only.
    input var logic        cfg_deny_mode_i,
    input var logic [15:0] cfg_deny_idx_i,
    // 0 = never; 1 = `err` instead of granting burst N; 2 = `err` mid-beat of N.
    input var logic [1:0]  cfg_err_mode_i,
    input var logic [15:0] cfg_err_burst_i,

    // Zeroes the BENCH's own observations (not the DUT's counters), so a check
    // can be about ONE job rather than a running total. A running minimum is the
    // specific trap: `first_wr_addr` over a whole run reports the first entry
    // ever written, which looks correct for as long as every test uses entry 0.
    input var logic stat_clear_i,

    // ---- DUT configuration -------------------------------------------------
    input var logic [2:0]  cfg_vram_client_i,
    input var logic [2:0]  cfg_hps_client_i,
    input var logic [31:0] cfg_journal_base_i,
    input var logic [31:0] cfg_journal_bytes_i,
    input var logic [31:0] cfg_epoch_i,

    // ---- job ---------------------------------------------------------------
    input  var logic        j_valid,
    output var logic        j_ready,
    input  var logic [15:0] j_slot,
    input  var logic [7:0]  j_gen,
    input  var logic [31:0] j_epoch,
    input  var logic [31:0] j_island,
    input  var logic [31:0] j_ix,
    input  var logic [31:0] j_iz,
    input  var logic [63:0] j_journal_addr,
    input  var logic [31:0] j_seq,
    input  var logic [31:0] j_src_id,

    // ---- the journal acknowledgement ---------------------------------------
    input  var logic        ack_valid,
    output var logic        ack_ready,
    input  var logic [31:0] ack_seq,
    input  var logic        ack_ok,

    // ---- the barrier release -----------------------------------------------
    output var logic        wb_valid,
    input  var logic        wb_ready,
    output var logic [15:0] wb_slot,
    output var logic [7:0]  wb_gen,
    output var logic [31:0] wb_epoch,

    // ---- completion --------------------------------------------------------
    output var logic        done_valid,
    input  var logic        done_ready,
    output var logic [15:0] done_slot,
    output var logic [7:0]  done_gen,
    output var logic [31:0] done_epoch,
    output var logic        done_ok,
    output var logic [3:0]  done_verdict,
    output var logic [31:0] done_seq,
    output var logic [31:0] done_src_id,

    // ---- fault trace -------------------------------------------------------
    output var logic [31:0] fault_island,
    output var logic [31:0] fault_ix,
    output var logic [31:0] fault_iz,
    output var logic [31:0] fault_seq,
    output var logic [31:0] fault_src_id,
    output var logic [3:0]  fault_verdict,

    // ---- counters ----------------------------------------------------------
    output var logic [31:0] sheets_written,
    output var logic [31:0] sheets_refused,
    output var logic [31:0] sheets_faulted,
    output var logic [31:0] hdr_ident_fails,
    output var logic [31:0] guard_denied,
    output var logic [31:0] bridge_errs,
    output var logic [31:0] acks_ok,
    output var logic [31:0] acks_nak,
    output var logic [31:0] acks_unmatched,
    output var logic [31:0] acks_after_epoch,
    output var logic [31:0] acks_overdue,
    output var logic [31:0] seq_conflicts,
    output var logic [31:0] wb_bytes,
    output var logic [31:0] outstanding_hwm,
    output var logic [31:0] ack_wait_max_cycles,
    output var logic [31:0] jobs_stall_cycles,

    // ---- what the BENCH saw (the how-many-times half) ----------------------
    output var logic [31:0] greqs_seen,      // guard requests accepted
    output var logic [31:0] rbeats_seen,     // read beats delivered
    output var logic [31:0] bursts_seen,     // bridge write bursts granted
    output var logic [31:0] wbeats_seen,     // write beats retired
    // NEVER CLEARED, and that is its job. `wb_bytes_o` counts every beat that
    // actually retired -- including the beats of a sheet the bridge later
    // aborted, because those bytes really did land in the journal. A scalar
    // model cannot know how many of them there were (that is exactly what
    // `transfer_complete` abstracts away), so the DUT's byte counter is checked
    // against the BENCH's independent beat count instead of against the oracle.
    output var logic [31:0] wbeats_total,
    output var logic [31:0] jnl_oob,         // write beats outside the journal image
    output var logic [31:0] wlast_bad,       // `wlast` not exactly on beat 7
    output var logic [31:0] first_wr_addr,   // address of the first write beat
    output var logic [31:0] last_wr_addr,    // ...and of the last
    output var logic [31:0] first_rd_addr,   // address of the first guard request
    output var logic [31:0] last_rd_addr,

    // ---- the REAL guard, observing the DUT's own requests ------------------
    output var logic [31:0] shadow_req_count,
    output var logic [31:0] shadow_ok_count,
    output var logic [31:0] shadow_fwd_count,
    output var logic [31:0] shadow_viol_count,

    // ---- the bench-driven probe into a third real MEM.GUARD ----------------
    input  var logic        p_valid,
    input  var logic        p_write,
    input  var logic [2:0]  p_client,
    input  var logic [26:0] p_addr,
    input  var logic [6:0]  p_len,
    input  var logic [63:0] p_be,
    output var logic        p_ready,
    output var logic [31:0] p_ok_count,
    output var logic [31:0] p_fwd_count,
    output var logic [31:0] p_viol_count
);

  // two whole pages, so "the neighbouring slot is untouched" is a real check
  localparam int unsigned VWORDS = 2 * 2672;   // 5,344
  localparam int unsigned JWORDS = 4 * 1024;   // four journal entries
  localparam int unsigned VW = $clog2(VWORDS);
  localparam int unsigned JW = $clog2(JWORDS);

  logic [63:0] vram_mem [VWORDS];
  logic [63:0] jnl_mem  [JWORDS];

  // ------------------------------------------------------------ DUT wires ---
  zhao_guard_req_t guard_req;
  zhao_guard_rsp_t guard_rsp;
  logic            beat_valid;
  logic [63:0]     beat_data;
  logic            beat_last;

  zhao_hps_burst_req_t hps_req;
  logic                hps_grant;
  zhao_hps_burst_rsp_t hps_rsp;
  logic [63:0]         hps_wdata;
  logic                hps_wvalid;
  logic                hps_wready;
  logic                hps_wlast;

  logic [10:0]        dut_slot, dut_wb_slot, dut_done_slot;
  logic signed [15:0] dut_fault_ix, dut_fault_iz;

  assign dut_slot  = j_slot[10:0];
  assign wb_slot   = {5'd0, dut_wb_slot};
  assign done_slot = {5'd0, dut_done_slot};
  assign fault_ix  = {{16{dut_fault_ix[15]}}, dut_fault_ix};
  assign fault_iz  = {{16{dut_fault_iz[15]}}, dut_fault_iz};

  // ACK_DEADLINE_CYCLES IS OVERRIDDEN SO THE WATCHDOG IS REACHABLE. At the
  // block's 100,000-cycle default the overdue counter could only be exercised by
  // a test that idles for longer than the rest of the suite takes. 20,000 is
  // still far above any wait this bench produces by accident -- the slowest
  // stalled sheet here retires in about 15,000 -- so the counter fires only in
  // the case written for it.
  zhao_terrain_writeback #(
      .ACK_SLOTS(4),
      .ACK_DEADLINE_CYCLES(20000)
  ) u_dut (
      .clk(clk),
      .rst_n(rst_n),
      .cfg_vram_client_i(zhao_client_e'(cfg_vram_client_i)),
      .cfg_hps_client_i(zhao_client_e'(cfg_hps_client_i)),
      .cfg_journal_base_i(cfg_journal_base_i),
      .cfg_journal_bytes_i(cfg_journal_bytes_i),
      .cfg_epoch_i(cfg_epoch_i),
      .j_valid_i(j_valid),
      .j_ready_o(j_ready),
      .j_slot_i(dut_slot),
      .j_gen_i(j_gen),
      .j_epoch_i(j_epoch),
      .j_island_i(j_island),
      .j_ix_i(j_ix[15:0]),
      .j_iz_i(j_iz[15:0]),
      .j_journal_addr_i(j_journal_addr),
      .j_seq_i(j_seq),
      .j_src_id_i(j_src_id),
      .guard_req_o(guard_req),
      .guard_rsp_i(guard_rsp),
      .beat_valid_i(beat_valid),
      .beat_data_i(beat_data),
      .beat_last_i(beat_last),
      .hps_req_o(hps_req),
      .hps_req_grant_i(hps_grant),
      .hps_rsp_i(hps_rsp),
      .hps_wdata_o(hps_wdata),
      .hps_wvalid_o(hps_wvalid),
      .hps_wready_i(hps_wready),
      .hps_wlast_o(hps_wlast),
      .ack_valid_i(ack_valid),
      .ack_ready_o(ack_ready),
      .ack_seq_i(ack_seq),
      .ack_ok_i(ack_ok),
      .wb_valid_o(wb_valid),
      .wb_ready_i(wb_ready),
      .wb_slot_o(dut_wb_slot),
      .wb_gen_o(wb_gen),
      .wb_epoch_o(wb_epoch),
      .done_valid_o(done_valid),
      .done_ready_i(done_ready),
      .done_slot_o(dut_done_slot),
      .done_gen_o(done_gen),
      .done_epoch_o(done_epoch),
      .done_ok_o(done_ok),
      .done_verdict_o(done_verdict),
      .done_seq_o(done_seq),
      .done_src_id_o(done_src_id),
      .fault_island_o(fault_island),
      .fault_ix_o(dut_fault_ix),
      .fault_iz_o(dut_fault_iz),
      .fault_seq_o(fault_seq),
      .fault_src_id_o(fault_src_id),
      .fault_verdict_o(fault_verdict),
      .sheets_written_o(sheets_written),
      .sheets_refused_o(sheets_refused),
      .sheets_faulted_o(sheets_faulted),
      .hdr_ident_fails_o(hdr_ident_fails),
      .guard_denied_o(guard_denied),
      .bridge_errs_o(bridge_errs),
      .acks_ok_o(acks_ok),
      .acks_nak_o(acks_nak),
      .acks_unmatched_o(acks_unmatched),
      .acks_after_epoch_o(acks_after_epoch),
      .acks_overdue_o(acks_overdue),
      .seq_conflicts_o(seq_conflicts),
      .wb_bytes_o(wb_bytes),
      .outstanding_hwm_o(outstanding_hwm),
      .ack_wait_max_cycles_o(ack_wait_max_cycles),
      .jobs_stall_cycles_o(jobs_stall_cycles)
  );

  // ------------------------------------------------- the played guard -------
  // Transcribed from zhao_mem_guard.sv:
  //     rsp.ready = !fwd_active;   // LEVEL
  //     rsp_ok_q <= 1'b1;          // PULSE, the cycle AFTER the accept
  // They are never high together, and a client that tests them in one arm reads
  // every pass as a denial.
  logic        g_fwd;
  logic [7:0]  g_hold;
  logic        g_ok_q, g_viol_q;

  // read-return engine: one accepted request becomes eight beats
  logic        rd_busy;
  logic [7:0]  rd_wait;
  logic [2:0]  rd_beat;
  logic [15:0] rd_word;
  logic [15:0] rd_word_sum;
  logic [VW-1:0] rd_word_idx;

  assign guard_rsp.ready     = !g_fwd;
  assign guard_rsp.ok        = g_ok_q;
  assign guard_rsp.violation = g_viol_q;

  assign rd_word_sum = rd_word + {13'd0, rd_beat};
  assign rd_word_idx = rd_word_sum[VW-1:0];

  logic g_deny_this;
  assign g_deny_this = !cfg_region_ok_i
                     || (cfg_deny_mode_i && (greqs_seen[15:0] == cfg_deny_idx_i));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g_fwd         <= 1'b0;
      g_hold        <= 8'd0;
      g_ok_q        <= 1'b0;
      g_viol_q      <= 1'b0;
      rd_busy       <= 1'b0;
      rd_wait       <= 8'd0;
      rd_beat       <= 3'd0;
      rd_word       <= 16'd0;
      beat_valid    <= 1'b0;
      beat_data     <= 64'd0;
      beat_last     <= 1'b0;
      greqs_seen    <= 32'd0;
      rbeats_seen   <= 32'd0;
      first_rd_addr <= 32'hFFFF_FFFF;
      last_rd_addr  <= 32'd0;
    end else begin
      // The clear is folded INTO the working branch, never made an else-if: an
      // else-if would freeze the played model for the cycle it fires, and a
      // model whose timing depends on the observer is not a model.
      if (stat_clear_i) begin
        greqs_seen    <= 32'd0;
        rbeats_seen   <= 32'd0;
        first_rd_addr <= 32'hFFFF_FFFF;
        last_rd_addr  <= 32'd0;
      end
      g_ok_q     <= 1'b0;
      g_viol_q   <= 1'b0;
      beat_valid <= 1'b0;
      beat_last  <= 1'b0;

      if (g_fwd) begin
        if (g_hold != 8'd0) g_hold <= g_hold - 8'd1;
        else                g_fwd  <= 1'b0;
      end else if (guard_req.valid) begin
        if (!g_deny_this) begin
          g_ok_q     <= 1'b1;
          g_fwd      <= 1'b1;
          g_hold     <= cfg_grant_hold_i;
          rd_busy    <= 1'b1;
          rd_wait    <= cfg_rd_latency_i;
          rd_beat    <= 3'd0;
          rd_word    <= 16'(({5'd0, guard_req.addr} - cfg_vram_window_base_i) >> 3);
          greqs_seen <= (stat_clear_i ? 32'd0 : greqs_seen) + 32'd1;
          if ({5'd0, guard_req.addr} < first_rd_addr) first_rd_addr <= {5'd0, guard_req.addr};
          if ({5'd0, guard_req.addr} > last_rd_addr)  last_rd_addr  <= {5'd0, guard_req.addr};
        end else begin
          g_viol_q   <= 1'b1;
          greqs_seen <= (stat_clear_i ? 32'd0 : greqs_seen) + 32'd1;
        end
      end

      if (rd_busy) begin
        if (rd_wait != 8'd0) begin
          rd_wait <= rd_wait - 8'd1;
        end else begin
          beat_valid  <= 1'b1;
          beat_data   <= vram_mem[rd_word_idx];
          beat_last   <= (rd_beat == 3'd7);
          rd_wait     <= cfg_rd_gap_i;
          rbeats_seen <= (stat_clear_i ? 32'd0 : rbeats_seen) + 32'd1;
          if (rd_beat == 3'd7) rd_busy <= 1'b0;
          else                 rd_beat <= rd_beat + 3'd1;
        end
      end
    end
  end

  // ------------------------------------------------ the played bridge -------
  // MEM.HPS.BRIDGE.md: a registered accept pulse, then a beat stream the client
  // sources. The bridge has no `wr_ready` today and this block asks for one; the
  // model plays the acceptance level the bridge already computes internally
  // (`busy && busy_write && issued`) so the requested amendment is exercised
  // rather than assumed away.
  logic        br_busy;
  logic [7:0]  br_wait;
  logic [2:0]  br_beat;
  logic [7:0]  wr_gap;
  logic [31:0] wr_base;

  logic        wbeat_take;
  logic [31:0] wbyte_addr;
  logic [31:0] wword_idx;
  logic        w_in_window;

  assign hps_wready  = br_busy && (br_wait == 8'd0) && (wr_gap >= cfg_wr_gap_i);
  assign wbeat_take  = hps_wvalid && hps_wready;
  assign wbyte_addr  = wr_base + {26'd0, br_beat, 3'd0};
  assign wword_idx   = (wbyte_addr - cfg_hps_window_base_i) >> 3;
  assign w_in_window = (wbyte_addr >= cfg_hps_window_base_i) && (wword_idx < 32'(JWORDS));

  // ONE DRIVING BLOCK PER MEMORY. The C++ preload and the captured write beats
  // both land in `jnl_mem`; splitting them across two always_ff blocks is the
  // MULTIDRIVEN Verilator refuses, and it would let a preload and a beat write
  // the same word in one cycle with no defined winner.
  always_ff @(posedge clk) begin
    if (mw_en && !mw_sel)               vram_mem[mw_addr[VW-1:0]] <= mw_data;
    if (mw_en && mw_sel)                jnl_mem[mw_addr[JW-1:0]]  <= mw_data;
    else if (wbeat_take && w_in_window) jnl_mem[wword_idx[JW-1:0]] <= hps_wdata;
    mr_data <= mr_sel ? jnl_mem[mr_addr[JW-1:0]] : vram_mem[mr_addr[VW-1:0]];
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      br_busy       <= 1'b0;
      br_wait       <= 8'd0;
      br_beat       <= 3'd0;
      wr_gap        <= 8'd0;
      wr_base       <= 32'd0;
      hps_grant     <= 1'b0;
      hps_rsp       <= '0;
      bursts_seen   <= 32'd0;
      wbeats_seen   <= 32'd0;
      wbeats_total  <= 32'd0;
      jnl_oob       <= 32'd0;
      wlast_bad     <= 32'd0;
      first_wr_addr <= 32'hFFFF_FFFF;
      last_wr_addr  <= 32'd0;
    end else begin
      if (stat_clear_i) begin
        bursts_seen   <= 32'd0;
        wbeats_seen   <= 32'd0;
        jnl_oob       <= 32'd0;
        wlast_bad     <= 32'd0;
        first_wr_addr <= 32'hFFFF_FFFF;
        last_wr_addr  <= 32'd0;
      end
      hps_grant          <= 1'b0;
      hps_rsp.beat_valid <= 1'b0;
      hps_rsp.last       <= 1'b0;
      hps_rsp.err        <= 1'b0;

      if (!br_busy) begin
        if (hps_req.valid) begin
          if ((cfg_err_mode_i == 2'd1) && (bursts_seen[15:0] == cfg_err_burst_i)) begin
            hps_rsp.err <= 1'b1;
          end else begin
            br_busy     <= 1'b1;
            hps_grant   <= 1'b1;
            br_wait     <= cfg_wr_latency_i;
            br_beat     <= 3'd0;
            wr_base     <= hps_req.addr;
            wr_gap      <= 8'hFF;
            bursts_seen <= (stat_clear_i ? 32'd0 : bursts_seen) + 32'd1;
          end
        end
      end else if (br_wait != 8'd0) begin
        br_wait <= br_wait - 8'd1;
      end else if ((cfg_err_mode_i == 2'd2)
                   && ((bursts_seen - 32'd1) == {16'd0, cfg_err_burst_i})
                   && (br_beat == 3'd3)) begin
        hps_rsp.err <= 1'b1;
        br_busy     <= 1'b0;
      end

      // write-beat pacing: `wr_gap` counts idle cycles since the last accepted
      // beat, so gap 0 is "always ready" and gap 3 stalls three cycles between
      // every beat.
      if (wbeat_take)           wr_gap <= 8'd0;
      else if (wr_gap != 8'hFF) wr_gap <= wr_gap + 8'd1;

      if (wbeat_take) begin
        wbeats_seen  <= (stat_clear_i ? 32'd0 : wbeats_seen) + 32'd1;
        wbeats_total <= wbeats_total + 32'd1;
        if (hps_wlast != (br_beat == 3'd7)) wlast_bad <= wlast_bad + 32'd1;
        if (!w_in_window) jnl_oob <= jnl_oob + 32'd1;
        if (wbyte_addr < first_wr_addr) first_wr_addr <= wbyte_addr;
        if (wbyte_addr > last_wr_addr)  last_wr_addr  <= wbyte_addr;
        if (br_beat == 3'd7) br_busy <= 1'b0;
        else                 br_beat <= br_beat + 3'd1;
      end
    end
  end

  // ------------------------------------------- the REAL guard, observing ----
  // Same request wires, no influence on the answer path. `map_valid = 0` and no
  // lease -- the terrain window consults no map input, so a guard with no
  // framebuffer lease still judges a terrain access on its own terms.
  //
  // ONE OBSERVATION PER REQUEST, NOT ONE PER STALLED CYCLE. The DUT holds
  // `guard_req.valid` high until the PLAYED guard says ready, which under a
  // nonzero `cfg_grant_hold_i` is several cycles; the observer has its own ready
  // and would otherwise accept the SAME request once per stalled cycle,
  // inflating its counters with an artefact of the arrangement.
  zhao_guard_rsp_t shadow_rsp;
  zhao_arb_req_t   shadow_arb_req;
  zhao_arb_rsp_t   shadow_arb_rsp;
  logic            shadow_viol_pulse;
  logic [31:0]     shadow_viol_total;
  zhao_guard_req_t shadow_viol_req;
  zhao_guard_req_t shadow_req;

  assign shadow_arb_rsp.grant   = 1'b1;
  assign shadow_arb_rsp.credits = 8'd32;

  always_comb begin
    shadow_req       = guard_req;
    shadow_req.valid = guard_req.valid && guard_rsp.ready;
  end

  zhao_mem_guard u_real_guard (
      .clk(clk),
      .rst_n(rst_n),
      .req(shadow_req),
      .rsp(shadow_rsp),
      .map_valid(1'b0),
      .blit_slot(1'b0),
      .blit_span(32'd0),
      .fb_writer(1'b0),
      .arb_req(shadow_arb_req),
      .arb_rsp(shadow_arb_rsp),
      .guard_violation(shadow_viol_pulse),
      .guard_violations(shadow_viol_total),
      .guard_violation_req(shadow_viol_req)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      shadow_req_count  <= 32'd0;
      shadow_ok_count   <= 32'd0;
      shadow_fwd_count  <= 32'd0;
      shadow_viol_count <= 32'd0;
    end else begin
      if (stat_clear_i) begin
        shadow_req_count  <= 32'd0;
        shadow_ok_count   <= 32'd0;
        shadow_fwd_count  <= 32'd0;
        shadow_viol_count <= 32'd0;
      end
      if (shadow_req.valid)
        shadow_req_count <= (stat_clear_i ? 32'd0 : shadow_req_count) + 32'd1;
      if (shadow_rsp.ok)
        shadow_ok_count <= (stat_clear_i ? 32'd0 : shadow_ok_count) + 32'd1;
      if (shadow_rsp.violation)
        shadow_viol_count <= (stat_clear_i ? 32'd0 : shadow_viol_count) + 32'd1;
      if (shadow_arb_req.valid && shadow_arb_rsp.grant)
        shadow_fwd_count <= (stat_clear_i ? 32'd0 : shadow_fwd_count) + 32'd1;
    end
  end

  // ------------------------------------ the REAL guard, ASKED directly ------
  // A third instance on bench-controlled wires, so the C++ can ask the REAL
  // block what it does with the read this DUT needs, with a different client in
  // the pool, and with the ruled client outside it. Its counters are deltas, not
  // levels, so a check cannot pass on some earlier request's verdict.
  zhao_guard_req_t probe_req;
  zhao_guard_rsp_t probe_rsp;
  zhao_arb_req_t   probe_arb_req;
  zhao_arb_rsp_t   probe_arb_rsp;
  logic            probe_viol_pulse;
  logic [31:0]     probe_viol_total;
  zhao_guard_req_t probe_viol_req;

  assign probe_req.valid  = p_valid;
  assign probe_req.write  = p_write;
  assign probe_req.client = zhao_client_e'(p_client);
  assign probe_req.addr   = p_addr;
  assign probe_req.len    = p_len;
  assign probe_req.be     = p_be;
  assign probe_arb_rsp.grant   = 1'b1;
  assign probe_arb_rsp.credits = 8'd32;
  assign p_ready = probe_rsp.ready;

  zhao_mem_guard u_probe_guard (
      .clk(clk),
      .rst_n(rst_n),
      .req(probe_req),
      .rsp(probe_rsp),
      .map_valid(1'b0),
      .blit_slot(1'b0),
      .blit_span(32'd0),
      .fb_writer(1'b0),
      .arb_req(probe_arb_req),
      .arb_rsp(probe_arb_rsp),
      .guard_violation(probe_viol_pulse),
      .guard_violations(probe_viol_total),
      .guard_violation_req(probe_viol_req)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      p_ok_count   <= 32'd0;
      p_fwd_count  <= 32'd0;
      p_viol_count <= 32'd0;
    end else begin
      if (stat_clear_i) begin
        p_ok_count   <= 32'd0;
        p_fwd_count  <= 32'd0;
        p_viol_count <= 32'd0;
      end
      if (probe_rsp.ok)        p_ok_count   <= (stat_clear_i ? 32'd0 : p_ok_count)   + 32'd1;
      if (probe_rsp.violation) p_viol_count <= (stat_clear_i ? 32'd0 : p_viol_count) + 32'd1;
      if (probe_arb_req.valid && probe_arb_rsp.grant)
        p_fwd_count <= (stat_clear_i ? 32'd0 : p_fwd_count) + 32'd1;
    end
  end

  // A BENCH IS HELD TO THE SAME -Wall AS THE BLOCK, so the bits nothing reads are
  // folded here deliberately rather than waived file-wide. Everything below is
  // either a frozen struct field this bench has no use for (the arbiter port's
  // client/len, a violation trace it counts instead of decoding) or the top of a
  // C++-side port that is wider than the DUT's own signal -- `j_slot` is 16 bits
  // here and 11 in the block, which is exactly the width difference that makes
  // the out-of-pool refusal reachable.
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_bench;
  assign unused_bench = shadow_viol_pulse ^ (^shadow_viol_total) ^ (^shadow_viol_req)
                      ^ probe_viol_pulse ^ (^probe_viol_total) ^ (^probe_viol_req)
                      ^ (^shadow_arb_req) ^ (^probe_arb_req) ^ (^shadow_rsp)
                      ^ (^mw_addr) ^ (^mr_addr) ^ (^j_slot) ^ (^j_ix) ^ (^j_iz)
                      ^ (^hps_req) ^ (^rd_word_sum);
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
