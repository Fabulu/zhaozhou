// tb_pageloader.sv - TERRAIN.PAGELOADER's ports flattened, with a played
// MEM.HPS.BRIDGE, a played MEM.GUARD, and the REAL MEM.GUARD watching.
//
// ---------------------------------------------------------------------------
// WHY THE GUARD IS PLAYED **AND** INSTANTIATED
// ---------------------------------------------------------------------------
// `tools/rtl/check_guard_verdict.py` records what happened on 2026-09-06: two
// geometry fetchers treated every guard PASS as a denial for months, because
// every bench that played a guard raised `ready` and `ok` together -- "the RTL,
// the harnesses and the measurements all agreed with each other about a machine
// that does not exist."
//
// So the played guard here is written from `zhao_mem_guard.sv` line by line:
// `ready` is the LEVEL `!fwd_active`, and `ok` / `violation` are REGISTERED
// PULSES one cycle after the accept. They are never both high.
//
// And because a played model can still drift from the block it plays, the real
// `zhao_mem_guard` is instantiated alongside as a pure observer on the same
// request wires. It is not in the loader's answer path, but it sees every
// request the loader makes and it counts what it did with them.
//
// THAT OBSERVER USED TO MEASURE A REFUSAL. IT NOW MEASURES A PASS.
// Until 2026-09-06 the guard had no window that admitted a write to bank 2 for
// anybody, so `shadow_ok_seen` staying ZERO was this file's evidence for the
// amendment the contract asked for. The amendment landed: `zhao_pkg` has
// `ZHAO_CLIENT_TERRAIN_BUILD = 6` and the guard has TERRAIN.PAGE_POOL,
// write-only, for that client alone. So the observer's job flipped from "it
// never passed one" to "it passed EXACTLY these and no others", and the
// counters below are what makes the second sentence checkable: a sticky
// `shadow_ok_seen` can say a pass happened but never that the right NUMBER
// happened, and this bench's own header records that a machine doing its work
// twice produces byte-identical output.
//
// A SECOND REAL GUARD, DRIVEN BY THE BENCH, PROVES THE REFUSALS.
// The observer only ever sees legal requests, because the loader only makes
// legal ones -- so on its own it shows one direction of a two-directional
// claim. `u_probe_guard` is a third instance wired to bench-controlled request
// wires and to nothing else, so the C++ can ask the REAL block what it does
// with a different client in the pool, the ruled client outside it, and a READ
// where a write was granted. Its counters are deltas, not levels, so a check
// cannot pass on some earlier request's verdict.
//
// ---------------------------------------------------------------------------
// STALLS ARE FIRST-CLASS, NOT AN AFTERTHOUGHT
// ---------------------------------------------------------------------------
// A sibling block passed 21 checks over every input it had and still dropped
// answers, because every phase held the consumer's ready high. Every stall
// source this block can face is therefore a knob here: bridge first-beat
// latency, inter-beat gaps, how long the guard holds `ready` low after an
// accept, and how many idle cycles sit between accepted write beats.
`default_nettype none

module tb_pageloader
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- backing memories, written a word at a time by the C++ side --------
    input var logic        mw_en,
    input var logic        mw_sel,        // 0 = staged HPS page, 1 = VRAM image
    input var logic [15:0] mw_addr,       // 64-bit word index
    input var logic [63:0] mw_data,
    input var logic [15:0] mr_addr,
    output var logic [63:0] mr_data,      // VRAM image read-back

    // ---- where the played memories start ----------------------------------
    input var logic [31:0] cfg_hps_window_base_i,
    input var logic [31:0] cfg_vram_window_base_i,

    // ---- played-bridge / played-guard timing ------------------------------
    input var logic [7:0] cfg_req_latency_i,   // cycles from accept to 1st beat
    input var logic [7:0] cfg_beat_gap_i,      // idle cycles between read beats
    input var logic [7:0] cfg_grant_hold_i,    // guard `ready` low after accept
    input var logic [7:0] cfg_wready_gap_i,    // idle cycles between write beats
    input var logic       cfg_region_ok_i,     // played guard's verdict
    // 0 = never; 1 = `err` instead of granting burst N; 2 = `err` mid-beat of
    // burst N.
    input var logic [1:0]  cfg_err_mode_i,
    input var logic [15:0] cfg_err_burst_i,

    // Zeroes the BENCH's own observations (not the DUT's counters), so every
    // check below can be about ONE job rather than about a running total. A
    // running minimum is the specific trap: `first_wr_addr` over a whole run
    // reports the first slot ever written, which looks correct for as long as
    // every test happens to use slot 0.
    input var logic stat_clear_i,

    // ---- DUT configuration -------------------------------------------------
    input var logic [2:0]  cfg_vram_client_i,
    input var logic [2:0]  cfg_hps_client_i,
    input var logic [31:0] cfg_hps_arena_base_i,
    input var logic [31:0] cfg_hps_arena_bytes_i,
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
    input  var logic [63:0] j_hps_addr,
    input  var logic [31:0] j_expect_crc,
    input  var logic [31:0] j_src_id,

    // ---- completion --------------------------------------------------------
    output var logic        fin_valid,
    input  var logic        fin_ready,
    output var logic [15:0] fin_slot,
    output var logic [7:0]  fin_gen,
    output var logic [31:0] fin_epoch,
    output var logic        fin_ok,
    output var logic [31:0] fin_crc,
    output var logic [3:0]  fin_verdict,
    output var logic [31:0] fin_src_id,

    // ---- fault trace -------------------------------------------------------
    output var logic [31:0] fault_island,
    output var logic [31:0] fault_ix,
    output var logic [31:0] fault_iz,
    output var logic [31:0] fault_src_id,
    output var logic [3:0]  fault_verdict,
    output var logic [31:0] fault_crc_seen,
    output var logic [31:0] fault_crc_expect,

    // ---- counters ----------------------------------------------------------
    output var logic [31:0] pages_loaded,
    output var logic [31:0] pages_faulted,
    output var logic [31:0] pages_refused,
    output var logic [31:0] crc_fails,
    output var logic [31:0] hdr_ident_fails,
    output var logic [31:0] incomplete,
    output var logic [31:0] guard_denied,
    output var logic [31:0] bridge_errs,
    output var logic [31:0] load_bytes,

    // ---- what the BENCH saw (the how-many-times half) ----------------------
    output var logic [31:0] bursts_seen,     // HPS bursts the bridge accepted
    output var logic [31:0] greqs_seen,      // guard requests accepted
    output var logic [31:0] wbeats_seen,     // write beats retired
    output var logic [31:0] vram_oob,        // writes outside the played window
    output var logic [31:0] wlast_bad,       // `wlast` not exactly on beat 7
    output var logic [31:0] first_wr_addr,   // address of the first write beat
    output var logic [31:0] last_wr_addr,    // ...and of the last
    output var logic        shadow_ok_seen,  // the REAL guard ever passed one
    output var logic [31:0] shadow_ok_count,   // ...and how many times
    output var logic [31:0] shadow_fwd_count,  // ...and how many it FORWARDED
    output var logic [31:0] shadow_violations,

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

  // one page is 21,376 B = 2,672 beats; four of them fits two slots either side
  localparam int unsigned WORDS = 4 * 2672;

  logic [63:0] hps_mem  [WORDS];
  logic [63:0] vram_mem [WORDS];

  localparam int unsigned MW = $clog2(WORDS);   // 14

  // ONE DRIVING BLOCK FOR THE VRAM IMAGE. The C++ preload and the captured
  // write beats both land here; splitting them across two always_ff blocks is
  // the MULTIDRIVEN that Verilator refuses, and it would also let a preload and
  // a beat write the same word in one cycle with no defined winner.
  logic        wbeat_take;
  logic [31:0] wbyte_addr;
  logic [31:0] wword_idx;
  logic        w_in_window;

  always_ff @(posedge clk) begin
    if (mw_en && mw_sel)                vram_mem[mw_addr[MW-1:0]]   <= mw_data;
    else if (wbeat_take && w_in_window) vram_mem[wword_idx[MW-1:0]] <= guard_wdata;
    if (mw_en && !mw_sel)               hps_mem[mw_addr[MW-1:0]]    <= mw_data;
    mr_data <= vram_mem[mr_addr[MW-1:0]];
  end

  // ------------------------------------------------------------ DUT wires ---
  zhao_hps_burst_req_t hps_req;
  logic                hps_grant;
  zhao_hps_burst_rsp_t hps_rsp;

  zhao_guard_req_t guard_req;
  zhao_guard_rsp_t guard_rsp;
  logic [63:0]     guard_wdata;
  logic            guard_wvalid;
  logic            guard_wready;
  logic            guard_wlast;

  logic [10:0] dut_slot, dut_fin_slot;
  logic signed [15:0] dut_fault_ix, dut_fault_iz;

  assign dut_slot = j_slot[10:0];
  assign fin_slot = {5'd0, dut_fin_slot};
  assign fault_ix = {{16{dut_fault_ix[15]}}, dut_fault_ix};
  assign fault_iz = {{16{dut_fault_iz[15]}}, dut_fault_iz};

  zhao_terrain_pageloader u_dut (
      .clk(clk),
      .rst_n(rst_n),
      .cfg_vram_client_i(zhao_client_e'(cfg_vram_client_i)),
      .cfg_hps_client_i(zhao_client_e'(cfg_hps_client_i)),
      .cfg_hps_arena_base_i(cfg_hps_arena_base_i),
      .cfg_hps_arena_bytes_i(cfg_hps_arena_bytes_i),
      .cfg_epoch_i(cfg_epoch_i),
      .j_valid_i(j_valid),
      .j_ready_o(j_ready),
      .j_slot_i(dut_slot),
      .j_gen_i(j_gen),
      .j_epoch_i(j_epoch),
      .j_island_i(j_island),
      .j_ix_i(j_ix[15:0]),
      .j_iz_i(j_iz[15:0]),
      .j_hps_addr_i(j_hps_addr),
      .j_expect_crc_i(j_expect_crc),
      .j_src_id_i(j_src_id),
      .hps_req_o(hps_req),
      .hps_req_grant_i(hps_grant),
      .hps_rsp_i(hps_rsp),
      .guard_req_o(guard_req),
      .guard_rsp_i(guard_rsp),
      .guard_wdata_o(guard_wdata),
      .guard_wvalid_o(guard_wvalid),
      .guard_wready_i(guard_wready),
      .guard_wlast_o(guard_wlast),
      .fin_valid_o(fin_valid),
      .fin_ready_i(fin_ready),
      .fin_slot_o(dut_fin_slot),
      .fin_gen_o(fin_gen),
      .fin_epoch_o(fin_epoch),
      .fin_ok_o(fin_ok),
      .fin_crc_o(fin_crc),
      .fin_verdict_o(fin_verdict),
      .fin_src_id_o(fin_src_id),
      .fault_island_o(fault_island),
      .fault_ix_o(dut_fault_ix),
      .fault_iz_o(dut_fault_iz),
      .fault_src_id_o(fault_src_id),
      .fault_verdict_o(fault_verdict),
      .fault_crc_seen_o(fault_crc_seen),
      .fault_crc_expect_o(fault_crc_expect),
      .pages_loaded_o(pages_loaded),
      .pages_faulted_o(pages_faulted),
      .pages_refused_o(pages_refused),
      .crc_fails_o(crc_fails),
      .hdr_ident_fails_o(hdr_ident_fails),
      .incomplete_o(incomplete),
      .guard_denied_o(guard_denied),
      .bridge_errs_o(bridge_errs),
      .load_bytes_o(load_bytes)
  );

  // -------------------------------------------------- the played bridge -----
  // MEM.HPS.BRIDGE.md: a registered accept pulse, then the sim latency profile
  // (16 gpu cycles to the first beat, 1 beat/cycle after). Both numbers are
  // knobs here so a test can hold the block at its slowest AND at its fastest.
  logic        br_busy;
  logic [7:0]  br_wait;
  logic [2:0]  br_beat;
  logic [15:0] br_word;   // word index of the burst's first beat
  logic [15:0] rd_word_sum;
  logic [MW-1:0] rd_word_idx;
  assign rd_word_sum = br_word + {13'd0, br_beat};
  assign rd_word_idx = rd_word_sum[MW-1:0];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      br_busy     <= 1'b0;
      br_wait     <= 8'd0;
      br_beat     <= 3'd0;
      br_word     <= 16'd0;
      hps_grant   <= 1'b0;
      hps_rsp     <= '0;
      bursts_seen <= 32'd0;
    end else begin
      // The clear is folded INTO the working branch, never made an else-if:
      // an else-if would freeze the played bridge for the cycle it fires, and
      // a model that stops for one cycle when a counter is zeroed is a model
      // whose timing depends on the observer.
      if (stat_clear_i) bursts_seen <= 32'd0;
      hps_grant          <= 1'b0;
      hps_rsp.beat_valid <= 1'b0;
      hps_rsp.last       <= 1'b0;
      hps_rsp.err        <= 1'b0;

      if (!br_busy) begin
        if (hps_req.valid) begin
          if ((cfg_err_mode_i == 2'd1) && (bursts_seen[15:0] == cfg_err_burst_i)) begin
            hps_rsp.err <= 1'b1;
          end else begin
            br_busy   <= 1'b1;
            hps_grant <= 1'b1;
            br_wait   <= cfg_req_latency_i;
            br_beat   <= 3'd0;
            br_word   <= 16'((hps_req.addr - cfg_hps_window_base_i) >> 3);
            bursts_seen <= bursts_seen + 32'd1;
          end
        end
      end else if (br_wait != 8'd0) begin
        br_wait <= br_wait - 8'd1;
      end else if ((cfg_err_mode_i == 2'd2)
                   && ((bursts_seen - 32'd1) == {16'd0, cfg_err_burst_i})
                   && (br_beat == 3'd3)) begin
        hps_rsp.err <= 1'b1;
        br_busy     <= 1'b0;
      end else begin
        hps_rsp.beat_valid <= 1'b1;
        hps_rsp.data       <= hps_mem[rd_word_idx];
        hps_rsp.last       <= (br_beat == 3'd7);
        br_wait            <= cfg_beat_gap_i;
        if (br_beat == 3'd7) br_busy <= 1'b0;
        else                 br_beat <= br_beat + 3'd1;
      end
    end
  end

  // --------------------------------------------------- the played guard -----
  // Transcribed from zhao_mem_guard.sv:
  //     rsp.ready = !fwd_active;   // LEVEL
  //     rsp_ok_q <= 1'b1;          // PULSE, the cycle AFTER the accept
  // They are never high together, and a client that tests them in one arm reads
  // every pass as a denial.
  logic       g_fwd;
  logic [7:0] g_hold;
  logic       g_ok_q, g_viol_q;
  logic [26:0] g_addr;
  logic [2:0]  g_beat;
  logic [7:0]  wr_gap;

  assign guard_rsp.ready     = !g_fwd;
  assign guard_rsp.ok        = g_ok_q;
  assign guard_rsp.violation = g_viol_q;
  assign guard_wready        = (wr_gap >= cfg_wready_gap_i);

  assign wbeat_take  = guard_wvalid && guard_wready;
  assign wbyte_addr  = {5'd0, g_addr} + {26'd0, g_beat, 3'd0};
  assign wword_idx   = (wbyte_addr - cfg_vram_window_base_i) >> 3;
  assign w_in_window = (wbyte_addr >= cfg_vram_window_base_i) && (wword_idx < 32'(WORDS));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g_fwd         <= 1'b0;
      g_hold        <= 8'd0;
      g_ok_q        <= 1'b0;
      g_viol_q      <= 1'b0;
      g_addr        <= '0;
      g_beat        <= 3'd0;
      wr_gap        <= 8'd0;
      greqs_seen    <= 32'd0;
      wbeats_seen   <= 32'd0;
      vram_oob      <= 32'd0;
      wlast_bad     <= 32'd0;
      first_wr_addr <= 32'hFFFF_FFFF;
      last_wr_addr  <= 32'd0;
    end else begin
      if (stat_clear_i) begin
        greqs_seen    <= 32'd0;
        wbeats_seen   <= 32'd0;
        vram_oob      <= 32'd0;
        wlast_bad     <= 32'd0;
        first_wr_addr <= 32'hFFFF_FFFF;
        last_wr_addr  <= 32'd0;
      end
      g_ok_q   <= 1'b0;
      g_viol_q <= 1'b0;

      if (g_fwd) begin
        if (g_hold != 8'd0) g_hold <= g_hold - 8'd1;
        else                g_fwd  <= 1'b0;
      end else if (guard_req.valid) begin
        if (cfg_region_ok_i) begin
          g_ok_q     <= 1'b1;
          g_fwd      <= 1'b1;
          g_hold     <= cfg_grant_hold_i;
          g_addr     <= guard_req.addr;
          g_beat     <= 3'd0;
          greqs_seen <= greqs_seen + 32'd1;
        end else begin
          g_viol_q <= 1'b1;
        end
      end

      // write-beat pacing: `wr_gap` counts idle cycles since the last accepted
      // beat, so gap 0 is "always ready" and gap 3 stalls three cycles between
      // every beat.
      if (guard_wvalid && guard_wready) wr_gap <= 8'd0;
      else if (wr_gap != 8'hFF)         wr_gap <= wr_gap + 8'd1;

      if (wbeat_take) begin
        wbeats_seen <= wbeats_seen + 32'd1;
        if (guard_wlast != (g_beat == 3'd7)) wlast_bad <= wlast_bad + 32'd1;
        if (!w_in_window) vram_oob <= vram_oob + 32'd1;
        if (wbyte_addr < first_wr_addr) first_wr_addr <= wbyte_addr;
        if (wbyte_addr > last_wr_addr)  last_wr_addr  <= wbyte_addr;
        g_beat <= g_beat + 3'd1;
      end
    end
  end

  // ------------------------------------------- the REAL guard, observing ----
  // Same request wires, no influence on the answer path. `map_valid = 0` and no
  // lease -- and that is not a limitation any more, it is the POINT: the
  // terrain window consults NO map input, so a guard with no framebuffer lease
  // at all still admits a legal page write. If this observer only passed when
  // handed a lease, the window would be frame-scoped and the amendment would be
  // the wrong shape.
  //
  // `shadow_arb_rsp.grant` is tied high, so every forwarded request is taken on
  // the cycle it is offered and `shadow_fwd_count` counts FORWARDS, one per
  // request. That is the observable the no-escape theorem is about -- what
  // reached the arbiter port -- rather than the verdict bit beside it.
  zhao_guard_rsp_t shadow_rsp;
  zhao_arb_req_t   shadow_arb_req;
  zhao_arb_rsp_t   shadow_arb_rsp;
  logic            shadow_viol_pulse;
  zhao_guard_req_t shadow_viol_req;

  assign shadow_arb_rsp.grant   = 1'b1;
  assign shadow_arb_rsp.credits = 8'd32;

  // ONE OBSERVATION PER REQUEST, NOT ONE PER STALLED CYCLE.
  // The loader holds `guard_req.valid` high until the PLAYED guard says ready,
  // which under a nonzero `cfg_grant_hold_i` is several cycles. The observer
  // has its own ready (its downstream grant is tied high, so its forwarding
  // stage frees every cycle) and would therefore accept the SAME burst once per
  // stalled cycle -- inflating its counters with an artefact of the observer
  // arrangement rather than measuring the loader. Presenting the request only
  // on the cycle the played guard actually takes it makes the observer see
  // exactly the request stream the loader issued, at any stall profile.
  zhao_guard_req_t shadow_req;
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
      .guard_violations(shadow_violations),
      .guard_violation_req(shadow_viol_req)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      shadow_ok_seen   <= 1'b0;
      shadow_ok_count  <= 32'd0;
      shadow_fwd_count <= 32'd0;
    end else begin
      // stat_clear_i is folded in, not made an else-if, for the reason the
      // played bridge above states: a model that stops for the cycle a counter
      // is zeroed has timing that depends on the observer.
      if (stat_clear_i) begin
        shadow_ok_count  <= 32'd0;
        shadow_fwd_count <= 32'd0;
      end
      if (shadow_rsp.ok) begin
        shadow_ok_seen  <= 1'b1;
        shadow_ok_count <= (stat_clear_i ? 32'd0 : shadow_ok_count) + 32'd1;
      end
      if (shadow_arb_req.valid && shadow_arb_rsp.grant)
        shadow_fwd_count <= (stat_clear_i ? 32'd0 : shadow_fwd_count) + 32'd1;
    end
  end

  // ------------------------------------ the REAL guard, ASKED directly ------
  // A third instance, on bench-controlled wires. Same parameters as the
  // observer -- no lease, no map -- because the terrain window does not consult
  // either, and giving the probe a lease it does not need would let a future
  // regression hide behind the framebuffer arms.
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
      if (probe_rsp.ok)                                p_ok_count   <= p_ok_count + 32'd1;
      if (probe_arb_req.valid && probe_arb_rsp.grant)  p_fwd_count  <= p_fwd_count + 32'd1;
      if (probe_viol_pulse)                            p_viol_count <= p_viol_count + 32'd1;
    end
  end

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_shadow;
  assign unused_shadow = shadow_viol_pulse | (|shadow_arb_req) | (|shadow_viol_req)
                       | (|probe_viol_total) | (|probe_viol_req) | (|probe_arb_req)
                       | probe_rsp.violation
                       | (|j_ix[31:16]) | (|j_iz[31:16]) | (|j_slot[15:11])
                       | (|mw_addr[15:MW]) | (|mr_addr[15:MW]) | (|hps_req) | (|rd_word_sum[15:MW])
                       | shadow_rsp.ready | shadow_rsp.violation;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule

`default_nettype wire
