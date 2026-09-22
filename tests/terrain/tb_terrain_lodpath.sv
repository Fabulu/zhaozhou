// tb_terrain_lodpath.sv -- the PAGE-LOAD DEVIATION PATH, end to end.
//
// TERRAIN.MIPFEED's fine stream -> zhao_terrain_lodfeed (buffer + the R8 walk)
// -> zhao_terrain_devstore (the R24/R59/R242 per-page store) -> SDRAM -> a read.
//
// It is a wrapper and not a second implementation of anything: every wire below
// is a port-to-port connection, and the two blocks are the production files.
// The point of testing them together is that the thing to prove is a VALUE
// TRAVERSING -- a height that entered as a page-load sample coming back out as
// the deviation zref computes -- and neither block alone can show that.
//
// ===========================================================================
// WHAT OWNER RULING R242 ADDED HERE, 2026-09-22
// ===========================================================================
// The store's records no longer live in M10K; they live in local SDRAM behind
// a MEM.GUARD socket.  So the traversal this bench exists to prove now runs
// THROUGH THE FABRIC, and the fabric has to be here for it to mean anything.
//
// PLAYED: the guard's accept/beat engine, transcribed from `zhao_mem_guard.sv`
// and `zhao_vram_arbiter.sv`'s timing -- `ready` as a LEVEL, `ok` pulsed the
// cycle after the accept, eight 64-bit beats with `last` on the eighth.  It is
// played rather than composed so the bench can make the DUT meet a DENIAL, a
// SHORT BURST and a STRAY BEAT, none of which a correct fabric produces.
//
// REAL: a second `zhao_mem_guard` instance watching the DUT's own requests and
// answering nobody.  The played guard says yes to whatever the bench tells it
// to; the REAL one says whether the request was legal, so "the store stays
// inside TERRAIN.DEVSTORE" is checked by the block that owns that question
// rather than by a range test written twice.  This is `tb_pageio.sv`'s
// arrangement, copied deliberately.
//
// THE MEMORY IS A WINDOW, not the whole map: 16 slots x 320 B = 640 words,
// based at the store's REGION_BASE.  A bench image the size of the real region
// would be 40,960 words of nothing.
`default_nettype none

module tb_terrain_lodpath
  import zhao_pkg::*;
#(
    parameter int unsigned SLOTW = 4,          // 16 slots is plenty for a bench
    parameter int unsigned GENW  = 8,
    parameter bit DEV_INCLUDE_BOUNDARY = 1'b1  // owner ruling R22
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the mip pass, played by the bench --------------------------------
    input  var logic               f_start_i,
    input  var logic [SLOTW-1:0]   f_slot_i,
    input  var logic [GENW-1:0]    f_gen_i,
    input  var logic [31:0]        f_epoch_i,
    input  var logic [15:0]        f_src_id_i,
    input  var logic               f_valid_i,
    input  var logic signed [15:0] f_h_i,

    // ---- THE DIRECTORY, PLAYED BY THE BENCH -------------------------------
    // `zhao_terrain_residency_v2` is not instantiated here on purpose.  What
    // has to be shown is that the FEED asks with the handle it walked under
    // and acts on the verdict; a real directory would make the verdict an
    // outcome of a page-eviction sequence and bury the thing under test.  The
    // directory's own answer is proved by `terrain_residency_directed`.
    output var logic               chk_valid_o,
    output var logic [SLOTW-1:0]   chk_slot_o,
    output var logic [GENW-1:0]    chk_gen_o,
    output var logic [31:0]        chk_epoch_o,
    input  var logic               chk_valid_i,
    input  var logic               chk_stale_i,

    output var logic [31:0] handles_checked_o,
    output var logic [31:0] handles_stale_o,
    output var logic [31:0] chk_unanswered_clocks_o,
    output var logic [31:0] chk_overrun_o,
    output var logic [31:0] chk_stray_o,

    // ---- the store's read side, driven by the bench -----------------------
    input  var logic             r_start_i,
    input  var logic [SLOTW-1:0] r_slot_i,
    output var logic             r_ready_o,
    output var logic             r_valid_o,
    input  var logic             r_ready_i,
    output var logic [3:0]       r_sp_o,
    output var logic [23:0]      r_dev1_o,
    output var logic [23:0]      r_dev2_o,
    output var logic [23:0]      r_dev3_o,
    output var logic signed [15:0] r_cy_o,
    output var logic [1:0]       r_prev_level_o,
    output var logic [16:0]      r_prev_morph_o,
    output var logic [7:0]       r_hold_o,
    output var logic             r_fresh_o,

    // ---- the history writeback, driven by the bench -----------------------
    input  var logic        h_valid_i,
    output var logic        h_ready_o,
    input  var logic [1:0]  h_level_i,
    input  var logic [16:0] h_morph_i,
    input  var logic [7:0]  h_hold_i,

    // ---- evidence ---------------------------------------------------------
    output var logic [31:0] lattices_seen_o,
    output var logic [31:0] lattices_walked_o,
    output var logic [31:0] lattices_dropped_o,
    output var logic [31:0] surface1_samples_o,
    output var logic [31:0] stray_samples_o,
    output var logic [31:0] walk_clocks_o,
    output var logic [31:0] dev_records_o,
    output var logic [31:0] dev_clipped_o,
    output var logic [31:0] dev_vertices_o,
    output var logic [31:0] dev_lattice_reads_o,
    output var logic        feed_busy_o,

    // `zhao_terrain_lodfeed` gained `w_src_id_o` on 2026-09-20 (ruling R70).
    // The store keys on the SLOT and has no src_id input, so this wrapper
    // carries it out to the bench rather than leaving the pin empty: a new
    // port left unconnected here is the PINMISSING the packet protocol warns
    // about, and an empty pin says nothing about whether the value is right.
    output var logic [15:0] w_src_id_o,

    output var logic [31:0] records_written_o,
    output var logic [31:0] patches_committed_o,
    output var logic [31:0] patches_read_o,
    output var logic [31:0] read_unwritten_o,
    output var logic [31:0] hist_step_bad_o,
    output var logic [31:0] invalidations_o,
    output var logic        store_busy_o,

    // ---- THE PLAYED FABRIC'S KNOBS (owner ruling R242) --------------------
    // Every one of these exists to construct a case a CORRECT fabric never
    // produces.  Defaults: region_ok = 1, everything else 0, which is a
    // zero-latency always-granting memory.
    input  var logic        cfg_region_ok_i,    // blanket verdict
    input  var logic        cfg_deny_mode_i,    // deny exactly one request
    input  var logic [15:0] cfg_deny_idx_i,
    input  var logic [7:0]  cfg_rd_latency_i,   // grant -> first read beat
    input  var logic [7:0]  cfg_rd_gap_i,       // between read beats
    input  var logic [7:0]  cfg_wr_latency_i,   // grant -> write accept
    input  var logic [7:0]  cfg_grant_hold_i,   // how long `ready` stays low
    input  var logic        cfg_short_mode_i,   // truncate one burst
    input  var logic [15:0] cfg_short_idx_i,
    input  var logic [2:0]  cfg_short_beat_i,
    // A beat offered with NO read in flight.  The share broadcasts beat data
    // and demuxes only `beat_valid`, so this is the wrong-demux case, and the
    // bench has to make it because nothing else can.
    input  var logic        cfg_stray_beat_i,

    // ---- what the played fabric saw --------------------------------------
    output var logic [31:0] greqs_seen_o,
    output var logic [31:0] wreqs_seen_o,
    output var logic [31:0] rbeats_seen_o,
    output var logic [31:0] wbeats_seen_o,

    // ---- what the REAL guard said about the DUT's requests ----------------
    output var logic [31:0] shadow_req_o,
    output var logic [31:0] shadow_viol_o,
    output var logic [31:0] shadow_fwd_o,

    // ---- the store's R242 counters ---------------------------------------
    output var logic [31:0] hist_unwritten_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] short_burst_o,
    output var logic [31:0] stray_beat_o,
    output var logic [31:0] slot_addr_bad_o,
    output var logic [31:0] bursts_read_o,
    output var logic [31:0] bursts_written_o,
    output var logic [31:0] rd_wait_clocks_o
);

  logic               w_valid, w_ready;
  logic [SLOTW-1:0]   w_slot;
  logic [3:0]         w_sp;
  logic [23:0]        w_dev1, w_dev2, w_dev3;
  logic signed [15:0] w_cy;
  /* verilator lint_off UNUSEDSIGNAL */
  logic               store_patch_done_w;   // the store's own commit pulse; the
                                            // bench reads patches_committed_o
  /* verilator lint_on UNUSEDSIGNAL */
  logic               inv_valid;
  logic [SLOTW-1:0]   inv_slot;

  zhao_terrain_lodfeed #(
    .SLOTW               (SLOTW),
    .EDGE                (33),
    .GENW                (GENW),
    .DEV_INCLUDE_BOUNDARY(DEV_INCLUDE_BOUNDARY)
  ) u_feed (
    .clk  (clk),
    .rst_n(rst_n),

    .f_start_i (f_start_i),
    .f_slot_i  (f_slot_i),
    .f_gen_i   (f_gen_i),
    .f_epoch_i (f_epoch_i),
    .f_src_id_i(f_src_id_i),
    .f_valid_i (f_valid_i),
    .f_h_i     (f_h_i),

    .w_valid_o(w_valid),
    .w_ready_i(w_ready),
    .w_slot_o (w_slot),
    .w_sp_o   (w_sp),
    .w_dev1_o (w_dev1),
    .w_dev2_o (w_dev2),
    .w_dev3_o (w_dev3),
    .w_cy_o   (w_cy),
    .w_src_id_o(w_src_id_o),

    .inv_valid_o(inv_valid),
    .inv_slot_o (inv_slot),

    .chk_valid_o(chk_valid_o),
    .chk_slot_o (chk_slot_o),
    .chk_gen_o  (chk_gen_o),
    .chk_epoch_o(chk_epoch_o),
    .chk_valid_i(chk_valid_i),
    .chk_stale_i(chk_stale_i),

    .handles_checked_o      (handles_checked_o),
    .handles_stale_o        (handles_stale_o),
    .chk_unanswered_clocks_o(chk_unanswered_clocks_o),
    .chk_overrun_o          (chk_overrun_o),
    .chk_stray_o            (chk_stray_o),

    .lattices_seen_o    (lattices_seen_o),
    .lattices_walked_o  (lattices_walked_o),
    .lattices_dropped_o (lattices_dropped_o),
    .surface1_samples_o (surface1_samples_o),
    .stray_samples_o    (stray_samples_o),
    .walk_clocks_o      (walk_clocks_o),
    .dev_records_o      (dev_records_o),
    .dev_clipped_o      (dev_clipped_o),
    .dev_vertices_o     (dev_vertices_o),
    .dev_lattice_reads_o(dev_lattice_reads_o),
    .busy_o             (feed_busy_o)
  );

  // ---- the store's guard socket -------------------------------------------
  zhao_guard_req_t guard_req;
  zhao_guard_rsp_t guard_rsp;
  logic            beat_valid, beat_last;
  logic [63:0]     beat_data;
  logic [63:0]     wdata;
  logic            wvalid, wready, wlast;

  // THE MUTANT SEAM.  A plain `ifdef selecting the module name, which a -D
  // does reach (CLAUDE.md: a function-like `define does not, silently).
  // `tests/terrain/terrain_devstore_addrmut.cpp` is built BOTH ways from this
  // one wrapper -- with the macro it requires `slot_addr_bad_o` to FIRE,
  // without it it requires SILENCE -- and that pair is what shows the selector
  // engaged rather than compiling production twice.
`ifdef ZHAO_DEVSTORE_ADDRLATCH_MUT
  zhao_terrain_devstore_addrlatch_mutant #(
`else
  zhao_terrain_devstore #(
`endif
    .SLOTS (1 << SLOTW),
    .SLOTW (SLOTW),
    .DEVW  (24),
    .MORPHW(17)
  ) u_store (
    .clk  (clk),
    .rst_n(rst_n),

    .cfg_vram_client_i(ZHAO_CLIENT_TERRAIN_BUILD),

    .guard_req_o   (guard_req),
    .guard_rsp_i   (guard_rsp),
    .beat_valid_i  (beat_valid),
    .beat_data_i   (beat_data),
    .beat_last_i   (beat_last),
    .guard_wdata_o (wdata),
    .guard_wvalid_o(wvalid),
    .guard_wready_i(wready),
    .guard_wlast_o (wlast),

    .w_valid_i     (w_valid),
    .w_ready_o     (w_ready),
    .w_slot_i      (w_slot),
    .w_sp_i        (w_sp),
    .w_dev1_i      (w_dev1),
    .w_dev2_i      (w_dev2),
    .w_dev3_i      (w_dev3),
    .w_cy_i        (w_cy),
    .w_patch_done_o(store_patch_done_w),

    .inv_valid_i(inv_valid),
    .inv_slot_i (inv_slot),

    .r_start_i     (r_start_i),
    .r_slot_i      (r_slot_i),
    .r_ready_o     (r_ready_o),
    .r_valid_o     (r_valid_o),
    .r_ready_i     (r_ready_i),
    .r_sp_o        (r_sp_o),
    .r_dev1_o      (r_dev1_o),
    .r_dev2_o      (r_dev2_o),
    .r_dev3_o      (r_dev3_o),
    .r_cy_o        (r_cy_o),
    .r_prev_level_o(r_prev_level_o),
    .r_prev_morph_o(r_prev_morph_o),
    .r_hold_o      (r_hold_o),
    .r_fresh_o     (r_fresh_o),

    .h_valid_i(h_valid_i),
    .h_ready_o(h_ready_o),
    .h_level_i(h_level_i),
    .h_morph_i(h_morph_i),
    .h_hold_i (h_hold_i),

    .records_written_o  (records_written_o),
    .patches_committed_o(patches_committed_o),
    .patches_read_o     (patches_read_o),
    .read_unwritten_o   (read_unwritten_o),
    .hist_unwritten_o   (hist_unwritten_o),
    .hist_step_bad_o    (hist_step_bad_o),
    .invalidations_o    (invalidations_o),
    .guard_denied_o     (guard_denied_o),
    .short_burst_o      (short_burst_o),
    .stray_beat_o       (stray_beat_o),
    .slot_addr_bad_o    (slot_addr_bad_o),
    .bursts_read_o      (bursts_read_o),
    .bursts_written_o   (bursts_written_o),
    .rd_wait_clocks_o   (rd_wait_clocks_o),
    .busy_o             (store_busy_o)
  );

  // ==========================================================================
  // THE PLAYED FABRIC
  // ==========================================================================
  // 16 slots x (256 B of records + 64 B of history) = 5,120 B = 640 words.
  localparam int unsigned IMG_WORDS = (1 << SLOTW) * (256 + 64) / 8;
  localparam int unsigned VW        = $clog2(IMG_WORDS);
  localparam logic [26:0] WIN_BASE  = ZHAO_TERRAIN_DEVSTORE_BASE[26:0];

  logic [63:0] vram_mem [IMG_WORDS];

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

  logic          pb_valid, pb_last;
  logic [63:0]   pb_data;

  assign guard_rsp.ready     = !g_fwd;
  assign guard_rsp.ok        = g_ok_q;
  assign guard_rsp.violation = g_viol_q;

  // THE INJECTED BEAT IS OR-ED IN AT THE PORT, not inside the played engine.
  // Putting it in the engine would make the engine's own beat counters count
  // it, and a stimulus that corrupts the instrument measuring it proves
  // nothing.
  assign beat_valid = pb_valid || cfg_stray_beat_i;
  assign beat_last  = pb_last;
  assign beat_data  = pb_data;

  // THE WRITE READY IS A LEVEL GATED ON `wr_busy` ALONE.  The beat is consumed
  // only when the DUT also offers `wvalid`, which is the ordinary ready/valid
  // rule -- a bench that counted a beat on `ready` alone would count the
  // cycles between the grant and the DUT reaching its beat state.
  assign wready = wr_busy && (wr_wait == 8'd0);

  logic [VW-1:0] rd_word_idx, wr_word_idx;
  assign rd_word_idx = rd_word + {{(VW-3){1'b0}}, rd_beat};
  assign wr_word_idx = wr_word + {{(VW-3){1'b0}}, wr_beat};

  logic g_deny_this;
  assign g_deny_this = !cfg_region_ok_i
                     || (cfg_deny_mode_i && (greqs_seen_o[15:0] == cfg_deny_idx_i));

  logic short_here;
  assign short_here = cfg_short_mode_i && (rd_idx == cfg_short_idx_i)
                      && (rd_beat == cfg_short_beat_i);

  logic wbeat_take;
  assign wbeat_take = wr_busy && wready && wvalid;

  /* verilator lint_off UNUSEDSIGNAL */
  logic wlast_unused;
  /* verilator lint_on UNUSEDSIGNAL */
  assign wlast_unused = wlast;

  always_ff @(posedge clk) begin
    if (wbeat_take) vram_mem[wr_word_idx] <= wdata;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g_fwd    <= 1'b0;
      g_hold   <= 8'd0;
      g_ok_q   <= 1'b0;
      g_viol_q <= 1'b0;
      rd_busy  <= 1'b0;
      rd_wait  <= 8'd0;
      rd_beat  <= 3'd0;
      rd_word  <= '0;
      rd_idx   <= 16'd0;
      wr_busy  <= 1'b0;
      wr_wait  <= 8'd0;
      wr_beat  <= 3'd0;
      wr_word  <= '0;
      pb_valid <= 1'b0;
      pb_data  <= 64'd0;
      pb_last  <= 1'b0;
      greqs_seen_o  <= 32'd0;
      wreqs_seen_o  <= 32'd0;
      rbeats_seen_o <= 32'd0;
      wbeats_seen_o <= 32'd0;
    end else begin
      g_ok_q   <= 1'b0;
      g_viol_q <= 1'b0;
      pb_valid <= 1'b0;
      pb_last  <= 1'b0;

      if (g_fwd) begin
        if (g_hold != 8'd0) g_hold <= g_hold - 8'd1;
        else                g_fwd  <= 1'b0;
      end else if (guard_req.valid) begin
        greqs_seen_o <= greqs_seen_o + 32'd1;
        if (!g_deny_this) begin
          g_ok_q <= 1'b1;
          g_fwd  <= 1'b1;
          g_hold <= cfg_grant_hold_i;
          if (guard_req.write) begin
            wreqs_seen_o <= wreqs_seen_o + 32'd1;
            wr_busy      <= 1'b1;
            wr_wait      <= cfg_wr_latency_i;
            wr_beat      <= 3'd0;
            wr_word      <= VW'((guard_req.addr - WIN_BASE) >> 3);
          end else begin
            rd_busy <= 1'b1;
            rd_wait <= cfg_rd_latency_i;
            rd_beat <= 3'd0;
            rd_idx  <= greqs_seen_o[15:0];
            rd_word <= VW'((guard_req.addr - WIN_BASE) >> 3);
          end
        end else begin
          g_viol_q <= 1'b1;
        end
      end

      if (rd_busy) begin
        if (rd_wait != 8'd0) begin
          rd_wait <= rd_wait - 8'd1;
        end else begin
          pb_valid <= 1'b1;
          pb_data  <= vram_mem[rd_word_idx];
          pb_last  <= (rd_beat == 3'd7) || short_here;
          rd_wait  <= cfg_rd_gap_i;
          rbeats_seen_o <= rbeats_seen_o + 32'd1;
          if ((rd_beat == 3'd7) || short_here) rd_busy <= 1'b0;
          else                                 rd_beat <= rd_beat + 3'd1;
        end
      end

      if (wr_busy) begin
        if (wr_wait != 8'd0) begin
          wr_wait <= wr_wait - 8'd1;
        end else if (wvalid) begin
          wbeats_seen_o <= wbeats_seen_o + 32'd1;
          if (wr_beat == 3'd7) wr_busy <= 1'b0;
          else                 wr_beat <= wr_beat + 3'd1;
        end
      end
    end
  end

  // ==========================================================================
  // THE REAL GUARD, AS AN OBSERVER
  // ==========================================================================
  // It answers nobody: its verdict is read as evidence about the DUT's
  // requests.  ONE OBSERVATION PER REQUEST, NOT ONE PER STALLED CYCLE -- the
  // DUT holds `valid` until the played guard says ready, which under a nonzero
  // `cfg_grant_hold_i` is several cycles.
  zhao_guard_req_t obs_req;
  /* verilator lint_off UNUSEDSIGNAL */
  zhao_guard_rsp_t obs_rsp;
  zhao_arb_req_t   obs_arb_req;
  zhao_guard_req_t obs_viol_req;
  /* verilator lint_on UNUSEDSIGNAL */
  zhao_arb_rsp_t   obs_arb_rsp;
  logic            obs_viol_pulse;
  logic [31:0]     obs_viol_total;

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
      // TIE: this client is not MEM.UPLOAD; R32's resource-write arm is not
      // what admits the deviation store, and leaving it live would let a
      // misconfigured region make an illegal store request look legal.
      .res_valid          (1'b0),
      .res_base           (32'd0),
      .res_span           (32'd0),
      .pb_lease_valid   (1'b0),  // TIE: this client is never ENGINE1; item 4's PARAMBUF window names ENGINE1 alone and stays shut here
      .pb_wr_view       (1'b0),  // TIE: this client is never ENGINE1; item 4's PARAMBUF window names ENGINE1 alone and stays shut here
      .pb_scratch_valid (1'b0),  // TIE: this client is never ENGINE1; item 4's PARAMBUF window names ENGINE1 alone and stays shut here
      .arb_req            (obs_arb_req),
      .arb_rsp            (obs_arb_rsp),
      .guard_violation    (obs_viol_pulse),
      .guard_violations   (obs_viol_total),
      .guard_violation_req(obs_viol_req)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      shadow_req_o  <= 32'd0;
      shadow_viol_o <= 32'd0;
      shadow_fwd_o  <= 32'd0;
    end else begin
      if (obs_req.valid)     shadow_req_o  <= shadow_req_o + 32'd1;
      if (obs_viol_pulse)    shadow_viol_o <= shadow_viol_o + 32'd1;
      if (obs_arb_req.valid && obs_arb_rsp.grant)
        shadow_fwd_o <= shadow_fwd_o + 32'd1;
    end
  end

endmodule

`default_nettype wire
