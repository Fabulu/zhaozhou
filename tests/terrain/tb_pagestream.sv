// tb_pagestream.sv -- TERRAIN.PAGESTREAM with a played page-pool fabric and the
// REAL MEM.GUARD watching from the side.
//
// ---------------------------------------------------------------------------
// WHAT IS PLAYED AND WHAT IS REAL
// ---------------------------------------------------------------------------
// PLAYED: the guard's accept/beat engine, transcribed from `zhao_mem_guard.sv`
// rather than paraphrased --
//
//     rsp.ready = !fwd_active;   // LEVEL
//     rsp_ok_q <= 1'b1;          // PULSE, the cycle AFTER the accept
//
// They are never high together, and a client that tests them in one arm reads
// every pass as a denial. The DUT was built against that protocol; this bench
// is where the claim is checkable.
//
// REAL: a second `zhao_mem_guard` instance watching the DUT's own request wires
// as an OBSERVER. TERRAIN.PAGE_POOL gained a READ arm on 2026-09-06 with
// TERRAIN.WRITEBACK, so every request this block makes must PASS -- and the
// observer's count is what says so. `shadow_ok == shadow_req` with
// `shadow_viol == 0` is this bench's evidence that the block is reading inside
// the window the ruling gave it, checked against the real region logic rather
// than against the played model's blanket verdict.
//
// ---------------------------------------------------------------------------
// EVERY STALL IS A KNOB
// ---------------------------------------------------------------------------
// A sibling block's differential passed a 15,625-case sweep and still missed a
// dropped answer, because every phase held the consumer's ready high. So: how
// long the guard holds `ready` low after an accept, the latency to the first
// beat, the gap between beats, and `v_ready`/`done_ready` from the C++ side.
`default_nettype none

module tb_pagestream
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- the page-pool image, a word at a time from the C++ side ----------
    input  var logic        mw_en,
    // 14 bits, which is exactly the image. A 16-bit port here would carry two
    // bits nothing can use, and "the top of the address is ignored" is how a
    // bench quietly reads slot 0 for every slot.
    input  var logic [13:0] mw_addr,   // 64-bit word index within the image
    input  var logic [63:0] mw_data,
    input  var logic [13:0] mr_addr,
    output var logic [63:0] mr_data,

    input var logic [31:0] cfg_vram_window_base_i,  // image origin, byte address

    // ---- played timing ----------------------------------------------------
    input var logic [7:0] cfg_grant_hold_i,
    input var logic [7:0] cfg_rd_latency_i,
    input var logic [7:0] cfg_rd_gap_i,

    // ---- fault injection --------------------------------------------------
    input var logic        cfg_region_ok_i,   // played guard's blanket verdict
    input var logic        cfg_deny_mode_i,   // refuse request number N only
    input var logic [15:0] cfg_deny_idx_i,
    // A burst that ends early. `beat_last` on beat cfg_short_beat_i of request
    // cfg_short_idx_i, which is the fabric giving up mid-transfer.
    input var logic        cfg_short_mode_i,
    input var logic [15:0] cfg_short_idx_i,
    input var logic [2:0]  cfg_short_beat_i,

    input var logic stat_clear_i,

    // ---- DUT configuration ------------------------------------------------
    input var logic [2:0]  cfg_vram_client_i,
    input var logic [31:0] cfg_epoch_i,

    // ---- job --------------------------------------------------------------
    input  var logic        j_valid,
    output var logic        j_ready,
    input  var logic [10:0] j_slot,
    input  var logic [ 7:0] j_gen,
    input  var logic [31:0] j_epoch,
    input  var logic [31:0] j_src_id,
    input  var logic [15:0] j_flags,

    // ---- the lattice out --------------------------------------------------
    output var logic        v_valid,
    input  var logic        v_ready,
    output var logic [15:0] v_base,
    output var logic [15:0] v_scar,
    output var logic [15:0] v_bottom,
    output var logic [ 5:0] v_vi,
    output var logic [ 5:0] v_vj,
    output var logic        v_first,
    output var logic        v_last,
    output var logic [15:0] v_slot,
    output var logic [ 7:0] v_gen,
    output var logic [31:0] v_epoch,
    output var logic [31:0] v_src_id,
    output var logic [15:0] v_flags,

    // ---- completion -------------------------------------------------------
    output var logic        done_valid,
    input  var logic        done_ready,
    output var logic [15:0] done_slot,
    output var logic [ 7:0] done_gen,
    output var logic [31:0] done_epoch,
    output var logic        done_ok,
    output var logic [ 3:0] done_verdict,
    output var logic [31:0] done_src_id,

    // ---- the DUT's counters ------------------------------------------------
    output var logic [31:0] c_lattices,
    output var logic [31:0] c_refused,
    output var logic [31:0] c_vertices,
    output var logic [31:0] c_bursts,
    output var logic [31:0] c_guard_denied,
    output var logic [31:0] c_incomplete,
    output var logic        c_idle,

    // ---- what the BENCH saw -------------------------------------------------
    output var logic [31:0] greqs_seen,
    output var logic [31:0] rbeats_seen,
    output var logic [31:0] first_rd_addr,
    output var logic [31:0] last_rd_addr,

    // ---- what the REAL guard said about the DUT's requests -----------------
    output var logic [31:0] shadow_req,
    output var logic [31:0] shadow_ok,
    output var logic [31:0] shadow_viol,
    output var logic [31:0] shadow_fwd
);

  localparam int unsigned SLOTW = 11;
  localparam int unsigned GENW  = 8;

  // The page-pool image. One page is 21,376 B = 2,672 words; four slots is
  // enough for "the block read the slot it was told to and not its neighbour",
  // which is the only reason a second slot exists here at all.
  localparam int unsigned IMG_WORDS = 4 * 2672;
  localparam int unsigned VW = $clog2(IMG_WORDS);

  logic [63:0] vram_mem [IMG_WORDS];

  always_ff @(posedge clk) begin
    if (mw_en) vram_mem[mw_addr] <= mw_data;
    mr_data <= vram_mem[mr_addr];
  end

  // ------------------------------------------------------ the DUT ----------
  zhao_guard_req_t guard_req;
  zhao_guard_rsp_t guard_rsp;
  logic            beat_valid;
  logic [63:0]     beat_data;
  logic            beat_last;

  logic [SLOTW-1:0]   d_v_slot, d_done_slot;
  logic signed [15:0] d_v_base, d_v_scar, d_v_bottom;

  assign v_slot    = {{(16-SLOTW){1'b0}}, d_v_slot};
  assign done_slot = {{(16-SLOTW){1'b0}}, d_done_slot};
  assign v_base    = d_v_base;
  assign v_scar    = d_v_scar;
  assign v_bottom  = d_v_bottom;

  zhao_terrain_pagestream #(
      .REGION_BASE (27'h400_0000),
      .REGION_SLOTS(1024),
      .SLOTW       (SLOTW),
      .GENW        (GENW)
  ) u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_vram_client_i(zhao_client_e'(cfg_vram_client_i)),
      .cfg_epoch_i      (cfg_epoch_i),

      .j_valid_i (j_valid),
      .j_ready_o (j_ready),
      .j_slot_i  (j_slot),
      .j_gen_i   (j_gen),
      .j_epoch_i (j_epoch),
      .j_src_id_i(j_src_id),
      .j_flags_i (j_flags),

      .guard_req_o (guard_req),
      .guard_rsp_i (guard_rsp),
      .beat_valid_i(beat_valid),
      .beat_data_i (beat_data),
      .beat_last_i (beat_last),

      .v_valid_o (v_valid),
      .v_ready_i (v_ready),
      .v_base_o  (d_v_base),
      .v_scar_o  (d_v_scar),
      .v_bottom_o(d_v_bottom),
      .v_vi_o    (v_vi),
      .v_vj_o    (v_vj),
      .v_first_o (v_first),
      .v_last_o  (v_last),
      .v_slot_o  (d_v_slot),
      .v_gen_o   (v_gen),
      .v_epoch_o (v_epoch),
      .v_src_id_o(v_src_id),
      .v_flags_o (v_flags),

      .done_valid_o  (done_valid),
      .done_ready_i  (done_ready),
      .done_slot_o   (d_done_slot),
      .done_gen_o    (done_gen),
      .done_epoch_o  (done_epoch),
      .done_ok_o     (done_ok),
      .done_verdict_o(done_verdict),
      .done_src_id_o (done_src_id),

      .lattices_streamed_o(c_lattices),
      .lattices_refused_o (c_refused),
      .vertices_streamed_o(c_vertices),
      .bursts_read_o      (c_bursts),
      .guard_denied_o     (c_guard_denied),
      .incomplete_o       (c_incomplete),
      .idle_o             (c_idle)
  );

  // ------------------------------------------------ the played guard -------
  logic       g_fwd;
  logic [7:0] g_hold;
  logic       g_ok_q, g_viol_q;

  logic          rd_busy;
  logic [7:0]    rd_wait;
  logic [2:0]    rd_beat;
  logic [VW-1:0] rd_word;
  logic [VW-1:0] rd_word_sum;
  logic [VW-1:0] rd_word_idx;
  logic [15:0]   rd_idx;          // which request this burst belongs to

  assign guard_rsp.ready     = !g_fwd;
  assign guard_rsp.ok        = g_ok_q;
  assign guard_rsp.violation = g_viol_q;

  assign rd_word_sum = rd_word + {{(VW-3){1'b0}}, rd_beat};
  assign rd_word_idx = rd_word_sum;

  logic g_deny_this;
  assign g_deny_this = !cfg_region_ok_i
                     || (cfg_deny_mode_i && (greqs_seen[15:0] == cfg_deny_idx_i));

  logic short_here;
  assign short_here = cfg_short_mode_i && (rd_idx == cfg_short_idx_i)
                      && (rd_beat == cfg_short_beat_i);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g_fwd         <= 1'b0;
      g_hold        <= 8'd0;
      g_ok_q        <= 1'b0;
      g_viol_q      <= 1'b0;
      rd_busy       <= 1'b0;
      rd_wait       <= 8'd0;
      rd_beat       <= 3'd0;
      rd_word       <= '0;
      rd_idx        <= 16'd0;
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
          rd_idx     <= greqs_seen[15:0];
          rd_word    <= VW'(({5'd0, guard_req.addr} - cfg_vram_window_base_i) >> 3);
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
          beat_last   <= (rd_beat == 3'd7) || short_here;
          rd_wait     <= cfg_rd_gap_i;
          rbeats_seen <= (stat_clear_i ? 32'd0 : rbeats_seen) + 32'd1;
          if ((rd_beat == 3'd7) || short_here) rd_busy <= 1'b0;
          else                                 rd_beat <= rd_beat + 3'd1;
        end
      end
    end
  end

  // ------------------------------------- the REAL guard, as an observer ----
  // It watches the DUT's own request wires and never drives anything.
  //
  // ONE OBSERVATION PER REQUEST, NOT ONE PER STALLED CYCLE. The DUT holds
  // `guard_req.valid` high until the PLAYED guard says ready, which under a
  // nonzero `cfg_grant_hold_i` is several cycles; the observer has its own ready
  // and would otherwise accept the SAME request once per stalled cycle,
  // inflating its counters with an artefact of the arrangement.
  zhao_guard_req_t obs_req;
  // The observer's `ready` is meaningless here -- nothing waits on it, because
  // the request is presented for exactly one cycle. Named as unused rather than
  // left to look like an oversight.
  /* verilator lint_off UNUSEDSIGNAL */
  zhao_guard_rsp_t obs_rsp;
  /* verilator lint_on UNUSEDSIGNAL */
  // The observer's forwarded request is read only for its `valid`, and its
  // latched violating request and running total are the block's own
  // bookkeeping, which this bench counts itself so a check can be about ONE job
  // rather than a running level. Named as unused rather than left to look like
  // an oversight.
  /* verilator lint_off UNUSEDSIGNAL */
  zhao_arb_req_t   obs_arb_req;
  logic [31:0]     obs_viol_total;
  zhao_guard_req_t obs_viol_req;
  /* verilator lint_on UNUSEDSIGNAL */
  zhao_arb_rsp_t   obs_arb_rsp;
  logic            obs_viol_pulse;

  assign obs_arb_rsp.grant   = 1'b1;
  assign obs_arb_rsp.credits = 8'd32;

  always_comb begin
    obs_req       = guard_req;
    obs_req.valid = guard_req.valid && guard_rsp.ready;
  end

  zhao_mem_guard u_real_guard (
      .clk                (clk),
      .rst_n              (rst_n),
      .req                (obs_req),
      .rsp                (obs_rsp),
      .map_valid          (1'b0),
      .blit_slot          (1'b0),
      .blit_span          (32'd0),
      .fb_writer          (1'b0),
      .arb_req            (obs_arb_req),
      .arb_rsp            (obs_arb_rsp),
      .guard_violation    (obs_viol_pulse),
      .guard_violations   (obs_viol_total),
      .guard_violation_req(obs_viol_req)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      shadow_req  <= 32'd0;
      shadow_ok   <= 32'd0;
      shadow_viol <= 32'd0;
      shadow_fwd  <= 32'd0;
    end else begin
      if (stat_clear_i) begin
        shadow_req  <= 32'd0;
        shadow_ok   <= 32'd0;
        shadow_viol <= 32'd0;
        shadow_fwd  <= 32'd0;
      end
      if (obs_req.valid)    shadow_req  <= (stat_clear_i ? 32'd0 : shadow_req)  + 32'd1;
      if (obs_rsp.ok)       shadow_ok   <= (stat_clear_i ? 32'd0 : shadow_ok)   + 32'd1;
      // THE PULSE, NOT THE STRUCT FIELD. `guard_violation` is the block's own
      // one-cycle event; taking it from the response struct instead would work
      // today and would stop working the moment the guard grows a second
      // refusal path that does not go through `rsp`.
      if (obs_viol_pulse)   shadow_viol <= (stat_clear_i ? 32'd0 : shadow_viol) + 32'd1;
      // AND WHAT IT FORWARDED. A guard that passed every request and forwarded
      // none would satisfy `shadow_ok == shadow_req` and still be broken.
      if (obs_arb_req.valid && obs_arb_rsp.grant)
        shadow_fwd <= (stat_clear_i ? 32'd0 : shadow_fwd) + 32'd1;
    end
  end

endmodule
