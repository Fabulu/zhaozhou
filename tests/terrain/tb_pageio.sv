// tb_pageio.sv -- TERRAIN.PAGEIO with a played page-pool fabric that serves
// READS AND WRITES, and the REAL MEM.GUARD watching from the side.
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
// every pass as a denial. `tb_pagestream.sv` established this bench shape; the
// one thing added here is a WRITE engine, because TERRAIN.PAGEIO is the first
// terrain block that both reads and writes the page pool.
//
// REAL: a second `zhao_mem_guard` instance watching the DUT's own request
// wires as an OBSERVER. It drives nothing. MEM.GUARD admits
// ZHAO_CLIENT_TERRAIN_BUILD on TERRAIN.PAGE_POOL in BOTH directions
// (`terrain_ok` requires req.write, `terrain_rd_ok` requires !req.write), so
// every request this block makes must PASS -- and the observer's count is what
// says so, against the real region logic rather than against the played
// model's blanket verdict. It is also what would catch a sparse `be`: the
// guard's `be_ok = (req.be == mask_of(req.len))` is the reason this block
// cannot use byte enables at all, and a drift into one would show up here as a
// violation rather than as a mystery.
//
// ---------------------------------------------------------------------------
// THE IMAGE IS READ BACK, NOT JUST WRITTEN
// ---------------------------------------------------------------------------
// `mr_addr`/`mr_data` let the C++ side read the page image AFTER the bake, and
// that is the whole acceptance test for section 4's read-modify-write: layers
// A, C and E must be BYTE-IDENTICAL, and nothing else in the machine reads
// them back, so nothing else would ever notice.
//
// ---------------------------------------------------------------------------
// EVERY STALL IS A KNOB
// ---------------------------------------------------------------------------
// How long the guard holds `ready` low after an accept, the latency to the
// first read beat, the gap between beats, the latency to write acceptance and
// the gap between accepted write beats. A sibling block's differential passed
// a 15,625-case sweep and still missed a dropped answer, because every phase
// held the consumer's ready high.
`default_nettype none

module tb_pageio
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- the page-pool image, a word at a time from the C++ side ----------
    input  var logic        mw_en,
    input  var logic [13:0] mw_addr,   // 64-bit word index within the image
    input  var logic [63:0] mw_data,
    input  var logic [13:0] mr_addr,
    output var logic [63:0] mr_data,

    input var logic [31:0] cfg_vram_window_base_i,  // image origin, byte address

    // ---- played timing ----------------------------------------------------
    input var logic [7:0] cfg_grant_hold_i,
    input var logic [7:0] cfg_rd_latency_i,
    input var logic [7:0] cfg_rd_gap_i,
    input var logic [7:0] cfg_wr_latency_i,
    input var logic [7:0] cfg_wr_gap_i,

    // ---- fault injection --------------------------------------------------
    input var logic        cfg_region_ok_i,   // played guard's blanket verdict
    input var logic        cfg_deny_mode_i,   // refuse request number N only
    input var logic [15:0] cfg_deny_idx_i,
    input var logic        cfg_short_mode_i,  // a read burst that ends early
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

    // ---- the BAKE face ----------------------------------------------------
    input  var logic [5:0]  nb_vi,
    input  var logic [5:0]  nb_vj,
    input  var logic        nb_req,
    output var logic        nb_out,

    input  var logic [5:0]  cell_ci,
    input  var logic [5:0]  cell_cj,
    output var logic        cell_valid,
    input  var logic        cell_ready,
    output var logic [7:0]  cell_state,

    input  var logic        sc_valid,
    output var logic        sc_ready,
    input  var logic [15:0] sc_scar,
    input  var logic [5:0]  sc_vi,
    input  var logic [5:0]  sc_vj,

    input  var logic        cs_valid,
    output var logic        cs_ready,
    input  var logic [7:0]  cs_state,
    input  var logic [5:0]  cs_ci,
    input  var logic [5:0]  cs_cj,

    input  var logic        dig_done,
    input  var logic        bake_done,

    // ---- the deformation mark ---------------------------------------------
    output var logic        dm_valid,
    input  var logic        dm_ready,
    output var logic [15:0] dm_slot,
    output var logic [ 7:0] dm_gen,
    output var logic [31:0] dm_epoch,
    output var logic        dm_bd,
    output var logic        dm_f,
    output var logic        dm_mips,

    // ---- completion -------------------------------------------------------
    output var logic        done_valid,
    input  var logic        done_ready,
    output var logic        done_ok,
    output var logic [ 3:0] done_verdict,
    output var logic [15:0] done_slot,
    output var logic [31:0] done_src_id,

    // ---- the DUT's counters ------------------------------------------------
    output var logic [31:0] c_pages_read,
    output var logic [31:0] c_pages_written,
    output var logic [31:0] c_guard_denied,
    output var logic [31:0] c_bursts_read,
    output var logic [31:0] c_bursts_written,
    output var logic [31:0] c_stale_gen,
    output var logic [31:0] c_marks,
    output var logic [31:0] c_refused,
    output var logic [31:0] c_nobake_mutated,
    output var logic [31:0] c_cell_refetch,
    output var logic        c_idle,

    // ---- what the BENCH saw -------------------------------------------------
    output var logic [31:0] greqs_seen,
    output var logic [31:0] rbeats_seen,
    output var logic [31:0] wbeats_seen,
    output var logic [31:0] wreqs_seen,

    // ---- what the REAL guard said about the DUT's requests -----------------
    output var logic [31:0] shadow_req,
    output var logic [31:0] shadow_ok,
    output var logic [31:0] shadow_viol,
    output var logic [31:0] shadow_fwd
);

  localparam int unsigned SLOTW = 11;
  localparam int unsigned GENW  = 8;

  // One page is 21,376 B = 2,672 words; four slots is enough for "the block
  // touched the slot it was told to and not its neighbour", which is the only
  // reason a second slot exists here at all.
  localparam int unsigned IMG_WORDS = 4 * 2672;
  localparam int unsigned VW = $clog2(IMG_WORDS);

  logic [63:0] vram_mem [IMG_WORDS];

  // ------------------------------------------------------ the DUT ----------
  zhao_guard_req_t guard_req;
  zhao_guard_rsp_t guard_rsp;
  logic            beat_valid;
  logic [63:0]     beat_data;
  logic            beat_last;
  logic [63:0]     wdata;
  logic            wvalid;
  logic            wready;
  logic            wlast;

  logic [SLOTW-1:0]   d_dm_slot, d_done_slot;
  logic signed [15:0] d_sc_scar;

  assign dm_slot   = {{(16-SLOTW){1'b0}}, d_dm_slot};
  assign done_slot = {{(16-SLOTW){1'b0}}, d_done_slot};
  assign d_sc_scar = sc_scar;

  zhao_terrain_pageio #(
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

      .guard_req_o   (guard_req),
      .guard_rsp_i   (guard_rsp),
      .beat_valid_i  (beat_valid),
      .beat_data_i   (beat_data),
      .beat_last_i   (beat_last),
      .guard_wdata_o (wdata),
      .guard_wvalid_o(wvalid),
      .guard_wready_i(wready),
      .guard_wlast_o (wlast),

      .nb_vi_i(nb_vi),
      .nb_vj_i(nb_vj),
      .nb_req_i(nb_req),
      .nb_o   (nb_out),

      .cell_ci_i   (cell_ci),
      .cell_cj_i   (cell_cj),
      .cell_valid_o(cell_valid),
      .cell_ready_i(cell_ready),
      .cell_state_o(cell_state),

      .sc_valid_i(sc_valid),
      .sc_ready_o(sc_ready),
      .sc_scar_i (d_sc_scar),
      .sc_vi_i   (sc_vi),
      .sc_vj_i   (sc_vj),

      .cs_valid_i(cs_valid),
      .cs_ready_o(cs_ready),
      .cs_state_i(cs_state),
      .cs_ci_i   (cs_ci),
      .cs_cj_i   (cs_cj),

      .dig_done_i (dig_done),
      .bake_done_i(bake_done),

      .dm_valid_o(dm_valid),
      .dm_ready_i(dm_ready),
      .dm_slot_o (d_dm_slot),
      .dm_gen_o  (dm_gen),
      .dm_epoch_o(dm_epoch),
      .dm_bd_o   (dm_bd),
      .dm_f_o    (dm_f),
      .dm_mips_o (dm_mips),

      .done_valid_o  (done_valid),
      .done_ready_i  (done_ready),
      .done_ok_o     (done_ok),
      .done_verdict_o(done_verdict),
      .done_slot_o   (d_done_slot),
      .done_src_id_o (done_src_id),

      .pages_read_o     (c_pages_read),
      .pages_written_o  (c_pages_written),
      .guard_denied_o   (c_guard_denied),
      .bursts_read_o    (c_bursts_read),
      .bursts_written_o (c_bursts_written),
      .stale_gen_o      (c_stale_gen),
      .marks_emitted_o  (c_marks),
      .jobs_refused_o   (c_refused),
      .nobake_mutated_o (c_nobake_mutated),
      .cell_refetch_o   (c_cell_refetch),
      .idle_o           (c_idle)
  );

  // ------------------------------------------------ the played guard -------
  logic       g_fwd;
  logic [7:0] g_hold;
  logic       g_ok_q, g_viol_q;

  logic          rd_busy;
  logic [7:0]    rd_wait;
  logic [2:0]    rd_beat;
  logic [VW-1:0] rd_word;
  logic [15:0]   rd_idx;

  logic          wr_busy;
  logic [7:0]    wr_wait;
  logic [2:0]    wr_beat;
  logic [VW-1:0] wr_word;

  assign guard_rsp.ready     = !g_fwd;
  assign guard_rsp.ok        = g_ok_q;
  assign guard_rsp.violation = g_viol_q;

  // THE WRITE READY IS A LEVEL, AND IT IS GATED ON `wr_busy` ALONE. The write
  // beat is consumed only when the DUT also offers `wvalid`, which is the
  // ordinary ready/valid rule -- a bench that counted a beat on `ready` alone
  // would count the cycles between the grant and the DUT reaching its beat
  // state, which is how `zhao_hps_bridge`'s missing `wr_ready` lost a client's
  // first beats silently.
  assign wready = wr_busy && (wr_wait == 8'd0);

  logic [VW-1:0] rd_word_idx, wr_word_idx;
  assign rd_word_idx = rd_word + {{(VW-3){1'b0}}, rd_beat};
  assign wr_word_idx = wr_word + {{(VW-3){1'b0}}, wr_beat};

  logic g_deny_this;
  assign g_deny_this = !cfg_region_ok_i
                     || (cfg_deny_mode_i && (greqs_seen[15:0] == cfg_deny_idx_i));

  logic short_here;
  assign short_here = cfg_short_mode_i && (rd_idx == cfg_short_idx_i)
                      && (rd_beat == cfg_short_beat_i);

  logic wbeat_take;
  assign wbeat_take = wr_busy && wready && wvalid;

  // ONE write port on the image, shared between the C++ loader and the played
  // fabric, with the loader winning. Two always_ff blocks writing one array is
  // two drivers; merging them here is what keeps it a RAM.
  always_ff @(posedge clk) begin
    if (mw_en)           vram_mem[mw_addr]     <= mw_data;
    else if (wbeat_take) vram_mem[wr_word_idx] <= wdata;
    mr_data <= vram_mem[mr_addr];
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g_fwd       <= 1'b0;
      g_hold      <= 8'd0;
      g_ok_q      <= 1'b0;
      g_viol_q    <= 1'b0;
      rd_busy     <= 1'b0;
      rd_wait     <= 8'd0;
      rd_beat     <= 3'd0;
      rd_word     <= '0;
      rd_idx      <= 16'd0;
      wr_busy     <= 1'b0;
      wr_wait     <= 8'd0;
      wr_beat     <= 3'd0;
      wr_word     <= '0;
      beat_valid  <= 1'b0;
      beat_data   <= 64'd0;
      beat_last   <= 1'b0;
      greqs_seen  <= 32'd0;
      wreqs_seen  <= 32'd0;
      rbeats_seen <= 32'd0;
      wbeats_seen <= 32'd0;
    end else begin
      // The clear is folded INTO the working branch, never made an else-if: an
      // else-if would freeze the played model for the cycle it fires, and a
      // model whose timing depends on the observer is not a model.
      if (stat_clear_i) begin
        greqs_seen  <= 32'd0;
        wreqs_seen  <= 32'd0;
        rbeats_seen <= 32'd0;
        wbeats_seen <= 32'd0;
      end
      g_ok_q     <= 1'b0;
      g_viol_q   <= 1'b0;
      beat_valid <= 1'b0;
      beat_last  <= 1'b0;

      if (g_fwd) begin
        if (g_hold != 8'd0) g_hold <= g_hold - 8'd1;
        else                g_fwd  <= 1'b0;
      end else if (guard_req.valid) begin
        greqs_seen <= (stat_clear_i ? 32'd0 : greqs_seen) + 32'd1;
        if (!g_deny_this) begin
          g_ok_q <= 1'b1;
          g_fwd  <= 1'b1;
          g_hold <= cfg_grant_hold_i;
          if (guard_req.write) begin
            wreqs_seen <= (stat_clear_i ? 32'd0 : wreqs_seen) + 32'd1;
            wr_busy    <= 1'b1;
            wr_wait    <= cfg_wr_latency_i;
            wr_beat    <= 3'd0;
            wr_word    <= VW'(({5'd0, guard_req.addr} - cfg_vram_window_base_i) >> 3);
          end else begin
            rd_busy <= 1'b1;
            rd_wait <= cfg_rd_latency_i;
            rd_beat <= 3'd0;
            rd_idx  <= greqs_seen[15:0];
            rd_word <= VW'(({5'd0, guard_req.addr} - cfg_vram_window_base_i) >> 3);
          end
        end else begin
          g_viol_q <= 1'b1;
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

      if (wr_busy) begin
        if (wr_wait != 8'd0) begin
          wr_wait <= wr_wait - 8'd1;
        end else if (wvalid) begin
          wbeats_seen <= (stat_clear_i ? 32'd0 : wbeats_seen) + 32'd1;
          wr_wait     <= cfg_wr_gap_i;
          if (wr_beat == 3'd7) wr_busy <= 1'b0;
          else                 wr_beat <= wr_beat + 3'd1;
        end
      end
    end
  end

  // ------------------------------------- the REAL guard, as an observer ----
  zhao_guard_req_t obs_req;
  /* verilator lint_off UNUSEDSIGNAL */
  zhao_guard_rsp_t obs_rsp;
  zhao_arb_req_t   obs_arb_req;
  logic [31:0]     obs_viol_total;
  zhao_guard_req_t obs_viol_req;
  logic            wlast_unused;
  /* verilator lint_on UNUSEDSIGNAL */
  zhao_arb_rsp_t   obs_arb_rsp;
  logic            obs_viol_pulse;

  assign wlast_unused        = wlast;
  assign obs_arb_rsp.grant   = 1'b1;
  assign obs_arb_rsp.credits = 8'd32;

  // ONE OBSERVATION PER REQUEST, NOT ONE PER STALLED CYCLE. The DUT holds
  // `guard_req.valid` high until the PLAYED guard says ready, which under a
  // nonzero `cfg_grant_hold_i` is several cycles.
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
      .res_valid  (1'b0),   // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .res_base   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
      .res_span   (32'd0),  // TIE: this client is not MEM.UPLOAD; the R32 resource-write arm names TERRAIN_BUILD alone
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
      if (obs_req.valid)  shadow_req  <= (stat_clear_i ? 32'd0 : shadow_req)  + 32'd1;
      if (obs_rsp.ok)     shadow_ok   <= (stat_clear_i ? 32'd0 : shadow_ok)   + 32'd1;
      // THE PULSE, NOT THE STRUCT FIELD -- `tb_pagestream.sv`'s reason.
      if (obs_viol_pulse) shadow_viol <= (stat_clear_i ? 32'd0 : shadow_viol) + 32'd1;
      // AND WHAT IT FORWARDED. A guard that passed every request and forwarded
      // none would satisfy `shadow_ok == shadow_req` and still be broken.
      if (obs_arb_req.valid && obs_arb_rsp.grant)
        shadow_fwd <= (stat_clear_i ? 32'd0 : shadow_fwd) + 32'd1;
    end
  end

endmodule
