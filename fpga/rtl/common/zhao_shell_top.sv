// zhao_shell_top.sv — the Phase-2 CONSOLE SHELL (plan W2.7): the assembled
// Zhaozhou machine as ONE running composition — CMD front end (scheduler +
// DMA), MEM (guards + arbiter + SDRAM controller), VIDEO (mode + scanout +
// scaler + framectl), INPUT (snapshot + rumble), AUDIO (FIFO) and DEBUG
// (counters + displayed-stream CRC) — wired together with the cross-domain
// and cross-block glue this file owns. `Zhaozhou.sv` stays the framework
// glue stub; this is the Verilator integration top (the tb wrapper
// tests/shell/tb_zhao_shell.sv adds the behavioural SDRAM model).
//
// Law (in citation order):
//   spec/video_rules.md   — raster/mode-latch/swap/repeat/displayed-CRC law
//   spec/memory_rules.md  — guard region law, arbiter D3, bridge bursts,
//                           FRAME_RING/pixel-arena (harness = HPS, D10)
//   spec/counters.md      — §3 snapshot timing law (providers present their
//                           latched shadows ONE cycle after the tick), §5
//                           Phase-2 owner table (which block feeds which id)
//   spec/input_rules.md   — PadFrame latch law, rumble frame gating
//   spec/audio_rules.md   — FIFO D4 law
//
// ---------------------------------------------------------------------------
// GLUE THIS FILE OWNS (each a real seam the block wave never composed):
//
//  1. SDRAM WRITE-DATA QUEUE — the guard/arbiter request path carries no
//     data lane; CMD.DMA streams ceil(len/8) x 64-bit beats per accepted
//     write request (guard_wvalid_o, the corrected W2.7 seam) and this file
//     converts them to the controller's 16-bit wr_beat pace. Blit DMA is
//     the ONLY Phase-2 writer, so the queue is strictly ordered; sticky
//     tripwires (shell_err_wfifo_o) catch over/underflow instead of
//     trusting the occupancy argument.
//  2. SDRAM READ-BEAT PACKER — rdata 16-bit words -> 64-bit beats for the
//     scanout fetch (4 words/beat, little-endian ascending: beat byte i =
//     VRAM byte addr+i, the same mapping the write queue uses, so canvas
//     bytes round-trip exactly). Scanout is the only Phase-2 reader;
//     shell_err_route_o trips if any other client's burst appears.
//  3. RECORD FRAMER — the DMA's verified packet byte stream -> the
//     scheduler's {opcode, w0..w3} record port (w0..w3 = record bytes
//     [16,32), the payload dwords; ZhCmdHeader is 16 bytes). A small
//     record QUEUE decouples presentation from consumption: the scheduler
//     backpressures records while a blit dispatch is pending, and the blit
//     can only be accepted after the SAME packet's stream fully drains —
//     without the queue that is a composition DEADLOCK (found composing
//     W2.6's verified halves; neither block is wrong in isolation). A
//     packet may carry at most FRAMER_Q-1 records after its DebugFrameBlit;
//     shell_err_framer_o trips (sticky) if a packet violates that instead
//     of wedging silently.
//  4. SLOT-READY PENDING REGISTERS (vid domain) — DebugFrameBlit completion
//     (gpu) -> FRAMECTL's slot_ready level; cleared exactly when FRAMECTL
//     issues swap_req for that slot, which closes the re-commit race after
//     the vswap decision (set wins over a simultaneous clear — that pairing
//     means a NEW completion raced the swap of the SAME slot, impossible in
//     the alternating-slot cadence and harmless if it ever happens: the
//     slot re-displays its own fresher content).
//  5. FRAME-COMPLETION CORRELATOR — the scheduler's frame_complete needs
//     the RING slot whose packet produced the displayed frame; this file
//     correlates {the RUN slot, its blit's dst FB slot, gpu_complete_slot,
//     !repeated} at the gpu tick.
//  6. MODE CDC (gpu -> vid) — the scheduler's mode register (changes only
//     at the tick) crossed with a 2FF + 2-cycle stability filter before a
//     mode_we pulse; the filter removes the multi-bit skew hazard
//     (STORM->DUO flips two bits) entirely.
//  7. DISPLAYED-FRAME CRC HANDOFF (vid -> gpu, ONCE PER FRAME) — DEBUG.CRC
//     runs in vid_clk and consumes the post-scaler pixel stream natively
//     (one RGB565 pixel per vid cycle = two bytes, low byte first, §3 LE
//     law). expect_bytes is zhao_displayed_bytes(mode) — NOT
//     zhao_canvas_bytes: for Duo those differ (245,760 displayed vs 196,608
//     stored) and the canvas value here would be the documented silent-Duo
//     bug. Only the FINALISED 32-bit CRC crosses to gpu, on a toggle with
//     the value held stable beside it. Until 2026-08-22 this was a per-pixel
//     re-timing that RELIED ON THE FROZEN SIM PHASE (vid_clk = gpu_clk/2,
//     coincident posedges — plan R1) and produced the two `vid_clk ->
//     gpu_clk` hold violations measured under HIGH PERFORMANCE effort; the
//     owner ruled it be moved rather than re-timed (docs/OWNER_DOCKET.md).
//  8. COUNTER PROVIDER ADAPTERS — spec/counters.md §5 owner table: the
//     scheduler's 3 channels (ids 0/1/2) + AUDIO.FIFO (31) are native
//     zhao_counter_snap_t providers; vram_bytes (28, summed over clients),
//     hps_ddr_bytes (29, summed), scanout_starvation_cycles (30, vid-
//     domain value quiescent through vblank — tripwire shell_err_cdc_o),
//     input_sequence_gaps (35) and rumble_frames_dropped (36) are adapted
//     here with the §3 timing law (valid pulses ONE cycle after the tick).
//     CMD.DMA's snap channels are deliberately NOT wired: its ids 1/2
//     duplicate the scheduler's (§5 names the scheduler/decoder as owner)
//     and id 29's owner is MEM.HPS.BRIDGE.
//  9. RUMBLE EDGE CONVERTER — the scheduler's dispatch register is a LEVEL
//     held until the tick; INPUT.RUMBLE counts every command pulse as a
//     replace ("dropped" law), so the level is converted to one pulse per
//     new dispatch (rise or payload change).
// 10. BLIT PACER — even after the FB-slot bank split removed the row
//     thrash, free write interleave leaves the serial scanout fetch ~2%
//     short of Duo line rate (accumulating into a 2-of-4-line limp); the
//     blit client paces itself into scanout's quiet windows instead.
//
// Conservative SystemVerilog subset (charter §2); lint-clean -Wall
// (lint_shell_top CTest). The behavioural SDRAM model is NOT here — it is
// testbench-only (D2) and lives in the tb wrapper.

module zhao_shell_top
  import zhao_pkg::*, zhao_abi_pkg::*;
#(
  parameter int unsigned FRAMER_Q = 8,    // record-queue depth (glue 3)
  parameter int unsigned WFIFO_W  = 64    // write-data queue, 16-bit words
) (
  // ---- clocks + reset (harness-driven, frozen ratios: vid = gpu/2,
  // ---- audio = gpu/4, fixed phase — plan R1) -----------------------------
  input  logic gpu_clk,
  input  logic vid_clk,
  input  logic audio_clk,
  input  logic rst_n,

  // ---- FRAME_RING view (harness = HPS, D10; memory_rules.md 4.1) ---------
  input  logic [1:0]  hps_state_i [0:2],
  input  logic [31:0] hps_byte_len_i [0:2],
  output logic        ring_wr_valid_o,
  output logic [1:0]  ring_wr_slot_o,
  output logic [1:0]  ring_wr_state_o,
  input  logic        ring_wr_ready_i,

  // ---- HPS bridge, harness side (memory_rules.md 3) ----------------------
  output logic        hps_req_valid_o,
  output logic        hps_req_write_o,
  output logic [31:0] hps_req_addr_o,
  output logic [6:0]  hps_req_len_o,
  input  logic        hps_req_grant_i,
  output logic        hps_wr_valid_o,
  output logic [63:0] hps_wr_data_o,
  output logic        hps_wr_last_o,
  input  logic        hps_rd_valid_i,
  input  logic [63:0] hps_rd_data_i,
  input  logic        hps_rd_last_i,

  // ---- raw decoded pad state (input_rules.md 1/4) ------------------------
  input  logic [3:0]  pad_present_i,
  input  logic [31:0] pad_buttons_i [0:3],
  input  logic [15:0] pad_lx_i [0:3],
  input  logic [15:0] pad_ly_i [0:3],
  input  logic [15:0] pad_rx_i [0:3],
  input  logic [15:0] pad_ry_i [0:3],

  // ---- audio: ring-read client seam (pairs in) + PCM out -----------------
  input  logic        aud_wr_valid_i,
  input  logic [15:0] aud_wr_l_i,
  input  logic [15:0] aud_wr_r_i,
  output logic        aud_wr_ready_o,
  output logic        aud_refill_req_o,
  output logic [11:0] aud_occupancy_o,
  output logic        pcm_valid_o,
  output logic [15:0] pcm_l_o,
  output logic [15:0] pcm_r_o,
  output logic        underrun_status_o,
  output logic [31:0] audio_underruns_o,

  // ---- displayed pixel stream (vid domain, post-scaler) ------------------
  output logic        px_valid_o,
  output logic [15:0] px_rgb_o,
  output logic [9:0]  px_x_o,
  output logic [7:0]  px_y_o,
  output logic        px_hsync_o,
  output logic        px_vsync_o,
  output logic        px_hblank_o,
  output logic        px_vblank_o,
  output logic        scaler_violation_o,

  // ---- DEBUG.CRC (gpu domain): the displayed-stream CRC ------------------
  output logic [31:0] crc_frame_o,
  output logic        crc_valid_o,
  output logic [31:0] crc_bytes_o,
  output logic        crc_size_err_o,

  // ---- frame boundary observability --------------------------------------
  output logic        gpu_tick_o,
  output logic [31:0] gpu_tick_frame_id_o,
  output logic        gpu_tick_repeated_o,
  output logic [0:0]  gpu_complete_slot_o,
  output logic [63:0] deadline_faults_o,     // FRAMECTL (vid)
  output logic [63:0] frame_cycles_o,        // FRAMECTL (vid)

  // ---- CMD observability --------------------------------------------------
  output logic [2:0]  slot_state_o [0:2],
  output logic        fence_valid_o,
  output logic [1:0]  fence_slot_o,
  output logic        fence_ok_o,
  output logic [7:0]  fence_status_o,
  output logic [1:0]  mode_act_o,
  output logic        dma_done_o,
  output logic [7:0]  dma_status_o,
  output logic        blit_done_o,
  output logic [7:0]  blit_status_o,

  // ---- INPUT observability ------------------------------------------------
  output logic [639:0] pad_frame_flat_o,
  output logic [15:0]  pad_sequence_o [0:3],
  output logic [63:0]  input_gaps_o,
  output logic [7:0]   rumble_duty_o [0:3],
  output logic [3:0]   rumble_active_o,
  output logic [3:0]   rumble_pwm_o,
  output logic [63:0]  rumble_drops_o,

  // ---- DEBUG.COUNTERS read window ----------------------------------------
  input  logic        cnt_snap_ready_i,
  output logic        cnt_snap_valid_o,
  output logic [15:0] cnt_snap_id_o,
  output logic [63:0] cnt_snap_value_o,
  output logic        cnt_window_open_o,
  output logic        cnt_cat_violation_o,

  // ---- MEM observability + shell integrity tripwires ---------------------
  output logic [31:0] guard_violations_o,    // both guards, summed
  output logic [63:0] starvation_o,
  output logic        init_done_o,
  output logic [31:0] refresh_stalls_o,
  output logic [31:0] bank_conflicts_o,
  output logic [31:0] scanout_preempted_o,
  output logic [31:0] hps_err_count_o,
  output logic        shell_err_wfifo_o,     // write queue over/underflow
  output logic        shell_err_route_o,     // burst from an impossible client
  output logic        shell_err_cdc_o,       // starvation sample moved at tick
  output logic        shell_err_framer_o,    // record queue overflow (glue 3)

  // ---- SDR PHY pins (behavioural model in the tb wrapper; D2) ------------
  output logic        phy_cs_n_o,
  output logic        phy_ras_n_o,
  output logic        phy_cas_n_o,
  output logic        phy_we_n_o,
  output logic [12:0] phy_a_o,
  output logic [1:0]  phy_ba_o,
  output logic [15:0] phy_dq_o,
  output logic        phy_dq_oe_o,
  output logic [1:0]  phy_dqm_o,
  input  logic [15:0] phy_dq_i
);

  // ==========================================================================
  // VIDEO: mode + scanout + scaler + framectl (the zhao_video_tb wiring,
  // now against the real memory chain)
  // ==========================================================================
  logic [15:0] vx, vy;
  logic        vhsync, vvsync, vhblank, vvblank;
  logic        frame_start, frame_end, vswap_dec;
  zhao_mode_e  vmode, vmode_next;

  logic        mode_we;
  logic [1:0]  mode_in;

  zhao_px_stream_t px_ser, px_out;

  logic        swap_req;
  logic [0:0]  swap_slot;
  logic        swap_ack;

  logic        frame_tick_vid, frame_repeated_vid;
  logic [31:0] frame_id_vid, deadline_margin_vid;
  zhao_frame_tick_t gpu_tick;
  logic [0:0]  gpu_complete_slot;

  logic [1:0]  slot_ready_pending;   // glue 4 (vid domain)

  zhao_guard_req_t scan_guard_req;
  zhao_guard_rsp_t scan_guard_rsp;
  logic        scan_beat_valid;
  logic [63:0] scan_beat_data;

  zhao_video_mode u_mode (
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .mode_we     (mode_we),
    .mode_in     (mode_in),
    .x           (vx),
    .y           (vy),
    .hsync       (vhsync),
    .vsync       (vvsync),
    .hblank      (vhblank),
    .vblank      (vvblank),
    .frame_start (frame_start),
    .frame_end   (frame_end),
    .vswap_dec   (vswap_dec),
    .mode_out    (vmode),
    .mode_next   (vmode_next)
  );

  zhao_video_scanout u_scanout (
    .gpu_clk     (gpu_clk),
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .guard_req   (scan_guard_req),
    .guard_rsp   (scan_guard_rsp),
    .beat_valid  (scan_beat_valid),
    .beat_data   (scan_beat_data),
    .beat_last   (1'b0),             // conformance-only pin (fetch header)
    .x           (vx),
    .y           (vy),
    .hsync       (vhsync),
    .vsync       (vvsync),
    .hblank      (vhblank),
    .vblank      (vvblank),
    .frame_start (frame_start),
    .vswap_dec   (vswap_dec),
    .mode        (vmode),
    .mode_next   (vmode_next),
    .swap_req    (swap_req),
    .swap_slot   (swap_slot),
    .swap_ack    (swap_ack),
    .px          (px_ser),
    .starvation_cycles (starvation_o)
  );

  zhao_video_scaler u_scaler (
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .in          (px_ser),
    .out         (px_out),
    .out_ready   (1'b1),
    .never_active(scaler_violation_o)
  );

  zhao_video_framectl u_framectl (
    .vid_clk        (vid_clk),
    .rst_n          (rst_n),
    .x              (vx),
    .y              (vy),
    .vblank         (vvblank),
    .vswap_dec      (vswap_dec),
    .frame_start    (frame_start),
    .mode           (vmode),
    .slot_ready     (slot_ready_pending),
    .deadline_cycles(32'd0),          // mode-period default (D8)
    .swap_req       (swap_req),
    .swap_slot      (swap_slot),
    .swap_ack       (swap_ack),
    .frame_repeated (frame_repeated_vid),
    .frame_tick     (frame_tick_vid),
    .frame_id       (frame_id_vid),
    .frame_cycles   (frame_cycles_o),
    .deadline_faults(deadline_faults_o),
    .deadline_margin(deadline_margin_vid),
    .gpu_clk        (gpu_clk),
    .gpu_tick       (gpu_tick),
    .gpu_complete_slot (gpu_complete_slot)
  );

  assign px_valid_o  = px_out.valid;
  assign px_rgb_o    = px_out.rgb565;
  assign px_x_o      = px_out.x;
  assign px_y_o      = px_out.y;
  assign px_hsync_o  = px_out.hsync;
  assign px_vsync_o  = px_out.vsync;
  assign px_hblank_o = px_out.hblank;
  assign px_vblank_o = px_out.vblank;

  assign gpu_tick_o          = gpu_tick.pulse;
  assign gpu_tick_frame_id_o = gpu_tick.frame_id;
  assign gpu_tick_repeated_o = gpu_tick.repeated;
  assign gpu_complete_slot_o = gpu_complete_slot;

  // ==========================================================================
  // CMD: scheduler + DMA
  // ==========================================================================
  logic        fetch_req_valid, fetch_req_ready;
  logic [1:0]  fetch_slot;
  logic [31:0] fetch_addr, fetch_byte_len, fetch_epoch;

  logic        dma_done;
  logic [1:0]  dma_slot;
  logic [7:0]  dma_status;
  logic [31:0] dma_bytes_consumed, dma_cmds_consumed;

  zhao_hps_burst_req_t dma_hps_req;
  zhao_hps_burst_rsp_t dma_hps_rsp;

  // The HPS bridge now has an ARBITER in front of it. Client 1 is tied off in
  // this step: the point of inserting it before DEBUG.FRAMEBLIT exists is to
  // prove it does not disturb the path that already works, so that when the
  // blitter arrives a regression has only one possible cause.
  //
  // It also does real work already, because CMD.DMA and the bridge do not agree
  // on how long a request stays up: CMD.DMA PULSES `hps_req_v` for one cycle,
  // and the arbiter captures it on the edge it chooses an owner rather than
  // re-reading the port a cycle later.
  zhao_hps_burst_req_t arb_hps_req;
  zhao_hps_burst_rsp_t arb_hps_rsp;
  logic                arb_bridge_grant;
  logic                arb_wr_valid, arb_wr_last;
  logic [63:0]         arb_wr_data;
  logic                dma_hps_grant;
  logic                blit_hps_grant;
  zhao_hps_burst_req_t blit_hps_req;
  zhao_hps_burst_rsp_t blit_hps_rsp;
  logic [31:0]         hps_arb_c0_bursts, hps_arb_c1_bursts, hps_arb_c1_wait;

  logic        pkt_valid, pkt_ready;
  logic [7:0]  pkt_byte;
  logic [31:0] pkt_len;

  logic        dpy_blit_valid, blit_req_ready;
  logic [7:0]  dpy_blit_dst, dpy_blit_mode;
  logic [31:0] dpy_blit_src, dpy_blit_len, dpy_blit_crc;
  logic        blit_done;
  logic [7:0]  blit_status;

  logic        dpy_rumble_valid;
  logic [7:0]  dpy_rumble_pad, dpy_rumble_en, dpy_rumble_str;
  logic        dpy_snap_req;

  zhao_mode_e  sched_mode;

  zhao_guard_req_t blit_guard_req;
  zhao_guard_rsp_t blit_guard_rsp;
  logic [63:0] blit_wdata;
  logic        blit_wvalid;
  logic        blit_wready;   // the write queue can take a whole beat
  logic        blit_wlast;

  // ---- the new blit path -------------------------------------------------
  // DEBUG.FRAMEBLIT + VIDEO.SLOTMGR replace CMD.DMA's in-house blitter and the
  // ad-hoc `ready_tog` glue that stood in for slot ownership.
  logic        slot_lease_req, slot_lease_slot;
  logic        slot_lease_grant, slot_lease_refused;
  logic        fb_lease_valid, fb_lease_slot;
  logic [15:0] fb_lease_gen;
  logic        blit_pub_v, blit_pub_slot;
  logic [15:0] blit_pub_gen;
  logic        blit_rel_v, blit_rel_slot;
  logic [15:0] blit_rel_gen;
  logic [1:0]  slot_ready_gpu;
  logic        slot_displayed_v, slot_displayed_s;
  logic [1:0]  slot_state_gpu [0:1];
  logic [31:0] slot_leases_granted, slot_stale_events;
  logic        swap_gpu_valid, swap_gpu_slot;

  // The request as the shell latched it, held for the whole transaction.
  logic [ 7:0] r_blit_dst, r_blit_mode;
  logic [31:0] r_blit_src, r_blit_len, r_blit_crc;
  logic        nb_req_valid;
  logic        nb_req_ready;
  logic        nb_done;
  logic [ 7:0] nb_status;
  logic [31:0] blits_published, blits_rejected;

  zhao_counter_snap_t sched_snap_cycles, sched_snap_faults, sched_snap_cmds;
  zhao_counter_snap_t dma_snap_cmds, dma_snap_bytes, dma_snap_drops;

  // record framer -> scheduler (glue 3)
  logic        rec_valid, rec_ready;
  logic [15:0] rec_opcode;
  logic [31:0] rec_w0, rec_w1, rec_w2, rec_w3;

  // frame completion correlator (glue 5)
  logic        frame_complete;
  logic [1:0]  frame_complete_slot;

  zhao_cmd_scheduler u_sched (
    .clk                  (gpu_clk),
    .rst_n                (rst_n),
    .hps_state_i          (hps_state_i),
    .hps_byte_len_i       (hps_byte_len_i),
    .ring_wr_valid_o      (ring_wr_valid_o),
    .ring_wr_slot_o       (ring_wr_slot_o),
    .ring_wr_state_o      (ring_wr_state_o),
    .ring_wr_ready_i      (ring_wr_ready_i),
    .fetch_req_valid_o    (fetch_req_valid),
    .fetch_req_ready_i    (fetch_req_ready),
    .fetch_slot_o         (fetch_slot),
    .fetch_addr_o         (fetch_addr),
    .fetch_byte_len_o     (fetch_byte_len),
    .fetch_epoch_o        (fetch_epoch),
    .dma_done_i           (dma_done),
    .dma_slot_i           (dma_slot),
    .dma_status_i         (dma_status),
    .rec_valid_i          (rec_valid),
    .rec_ready_o          (rec_ready),
    .rec_opcode_i         (rec_opcode),
    .rec_w0_i             (rec_w0),
    .rec_w1_i             (rec_w1),
    .rec_w2_i             (rec_w2),
    .rec_w3_i             (rec_w3),
    .frame_tick_i         (gpu_tick),
    .frame_complete_i     (frame_complete),
    .frame_complete_slot_i(frame_complete_slot),
    .dpy_blit_valid_o     (dpy_blit_valid),
    .dpy_blit_ready_i     (blit_req_ready),
    .dpy_blit_dst_slot_o  (dpy_blit_dst),
    .dpy_blit_mode_o      (dpy_blit_mode),
    .dpy_blit_src_o       (dpy_blit_src),
    .dpy_blit_len_o       (dpy_blit_len),
    .dpy_blit_crc_o       (dpy_blit_crc),
    .dpy_rumble_valid_o   (dpy_rumble_valid),
    .dpy_rumble_pad_o     (dpy_rumble_pad),
    .dpy_rumble_en_o      (dpy_rumble_en),
    .dpy_rumble_str_o     (dpy_rumble_str),
    .dpy_snap_req_o       (dpy_snap_req),
    .mode_o               (sched_mode),
    .snap_cycles_o        (sched_snap_cycles),
    .snap_faults_o        (sched_snap_faults),
    .snap_cmds_o          (sched_snap_cmds),
    .fence_valid_o        (fence_valid_o),
    .fence_slot_o         (fence_slot_o),
    .fence_ok_o           (fence_ok_o),
    .fence_status_o       (fence_status_o),
    .slot_state_o         (slot_state_o)
  );

  zhao_cmd_dma u_dma (
    .clk                 (gpu_clk),
    .rst_n               (rst_n),
    .fetch_req_valid_i   (fetch_req_valid),
    .fetch_req_ready_o   (fetch_req_ready),
    .fetch_slot_i        (fetch_slot),
    .fetch_addr_i        (fetch_addr),
    .fetch_byte_len_i    (fetch_byte_len),
    .fetch_epoch_i       (fetch_epoch),
    .dma_done_o          (dma_done),
    .dma_slot_o          (dma_slot),
    .dma_status_o        (dma_status),
    .dma_bytes_consumed_o(dma_bytes_consumed),
    .dma_cmds_consumed_o (dma_cmds_consumed),
    .hps_req_o           (dma_hps_req),
    .hps_rsp_i           (dma_hps_rsp),
    .pkt_valid_o         (pkt_valid),
    .pkt_ready_i         (pkt_ready),
    .pkt_byte_o          (pkt_byte),
    .pkt_len_o           (pkt_len),
    // CMD.DMA has no blitter and no MEM.GUARD client any more: step 6
    // deleted the machinery, and with it the 1.97 Mbit whole-canvas staging
    // buffer that was the entire point of the redesign. The blit dispatch is
    // DEBUG.FRAMEBLIT's, and it stages 64 bytes.
    .frame_tick_i        (gpu_tick),
    .snap_cmds_o         (dma_snap_cmds),
    .snap_bytes_o        (dma_snap_bytes),
    .snap_drops_o        (dma_snap_drops)
  );

  assign mode_act_o    = sched_mode;
  assign dma_done_o    = dma_done;
  assign dma_status_o  = dma_status;
  assign blit_done_o   = blit_done;
  assign blit_status_o = blit_status;

  // DMA snap channels intentionally unconsumed (header note, glue 8):
  // spec/counters.md 5 names the scheduler (ids 1/2) and MEM.HPS.BRIDGE
  // (id 29) as the Phase-2 owners of the ids the DMA also latches.
  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_dma;
  assign unused_dma = ^dma_snap_cmds ^ ^dma_snap_bytes ^ ^dma_snap_drops
                    ^ ^dma_bytes_consumed ^ ^dma_cmds_consumed ^ ^dma_slot
                    ^ dpy_snap_req ^ ^frame_id_vid ^ ^deadline_margin_vid
                    ^ frame_repeated_vid ^ frame_tick_vid ^ frame_end
                    ^ starve_busy;
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // MEM: two guard instances (client field selects the region law), the
  // arbiter, the SDRAM controller, and the HPS bridge
  // ==========================================================================
  zhao_arb_req_t [4:0] client_req;
  zhao_arb_rsp_t [4:0] client_rsp;
  zhao_arb_req_t       ctrl_req;
  zhao_arb_rsp_t       ctrl_rsp;
  logic                hold_refresh;

  // guard (scanout, read-only both slots)
  zhao_arb_req_t scan_arb_req, blit_arb_req;
  logic       scan_gv, blit_gv;
  logic [31:0] scan_gv_cnt, blit_gv_cnt;
  zhao_guard_req_t scan_gv_req, blit_gv_req;   // trace-only (harness lane)
  logic        ctrl_refresh_pulse;
  // `bridge_req_grant` is gone: the bridge's accept now lands on the arbiter,
  // not here. The lint sink below absorbs the arbiter's client-side grants
  // instead -- DEBUG.FRAMEBLIT will consume its own once it is wired.
  logic        bridge_req_grant;
  assign bridge_req_grant = dma_hps_grant;

  zhao_mem_guard u_guard_scan (
    .clk        (gpu_clk),
    .rst_n      (rst_n),
    .req        (scan_guard_req),
    .rsp        (scan_guard_rsp),
    .map_valid  (1'b0),
    .blit_slot  (1'b0),
    .blit_span  (32'd0),
    .arb_req    (scan_arb_req),
    .arb_rsp    (client_rsp[0]),
    .guard_violation     (scan_gv),
    .guard_violations    (scan_gv_cnt),
    .guard_violation_req (scan_gv_req)
  );

  // guard (blit, write-only into the granted window — glue 4 grants it)
  logic        map_valid_q;   // driven from the lease, see GLUE 4 below
  logic [0:0]  map_slot_q;
  logic [31:0] map_span_q;

  zhao_mem_guard u_guard_blit (
    .clk        (gpu_clk),
    .rst_n      (rst_n),
    .req        (blit_guard_req),
    .rsp        (blit_guard_rsp),
    .map_valid  (map_valid_q),
    .blit_slot  (map_slot_q),
    .blit_span  (map_span_q),
    .arb_req    (blit_arb_req),
    .arb_rsp    (client_rsp[1]),
    .guard_violation     (blit_gv),
    .guard_violations    (blit_gv_cnt),
    .guard_violation_req (blit_gv_req)
  );

  assign guard_violations_o = scan_gv_cnt + blit_gv_cnt;

  // ---- GLUE 10: the blit pacer -------------------------------------------
  // Even with the FB-slot bank split (which removed the single-bank row
  // thrash — zhao_pkg ZHAO_FB_SLOT1_BASE note), free interleaving of blit
  // writes with the SERIAL scanout fetch leaves the fetch ~2% short of Duo
  // line rate: the deficit accumulates until the ping-pong limps (2 of
  // every 4 lines starved, measured). Starved lines re-emit held pixels,
  // which makes the displayed stream un-composable by zref — so the blit
  // client PACES itself (client pacing is lawful; the arbiter's D3 policy
  // is untouched): its arbiter request is offered only once scanout has
  // been quiet for BLIT_PACE_QUIET cycles, batching writes into the
  // line-fetch tail gaps, the Duo border lines and vblank. Measured:
  // starvation exactly zero at every cadence.
  localparam int unsigned BLIT_PACE_QUIET = 8;
  logic [3:0] scan_quiet_cnt;
  logic [7:0] scan_beats_pending;   // beats owed to the scanout fetch
  logic       scan_active;
  assign scan_active = scan_guard_req.valid || (scan_beats_pending != 8'd0);

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      scan_quiet_cnt     <= 4'd0;
      scan_beats_pending <= 8'd0;
    end else begin
      // one 64-B scanout request = 8 beats owed; each packed beat repays 1
      if (scan_guard_req.valid && scan_guard_rsp.ready
          && !scan_guard_rsp.violation) begin
        scan_beats_pending <= scan_beats_pending + 8'd8
                              - (scan_beat_valid ? 8'd1 : 8'd0);
      end else if (scan_beat_valid && (scan_beats_pending != 8'd0)) begin
        scan_beats_pending <= scan_beats_pending - 8'd1;
      end
      if (scan_active) scan_quiet_cnt <= 4'd0;
      else if (scan_quiet_cnt != 4'hF) scan_quiet_cnt <= scan_quiet_cnt + 4'd1;
    end
  end

  logic blit_gate_open;
  assign blit_gate_open = (scan_quiet_cnt >= 4'(BLIT_PACE_QUIET));

  always_comb begin
    client_req[1]       = blit_arb_req;
    client_req[1].valid = blit_arb_req.valid && blit_gate_open;
  end

  assign client_req[0] = scan_arb_req;
  assign client_req[2] = '0;
  assign client_req[3] = '0;
  assign client_req[4] = '0;


  logic [4:0][31:0] vram_bytes, vram_bytes_shadow;

  zhao_vram_arbiter u_arb (
    .clk               (gpu_clk),
    .rst_n             (rst_n),
    .client_req        (client_req),
    .client_rsp        (client_rsp),
    .ctrl_req          (ctrl_req),
    .hold_refresh      (hold_refresh),
    .ctrl_rsp          (ctrl_rsp),
    .frame_tick        (gpu_tick.pulse),
    .vram_bytes        (vram_bytes),
    .vram_bytes_shadow (vram_bytes_shadow),
    .scanout_preempted (scanout_preempted_o)
  );

  // write-data queue (glue 1): dma beats (4 words each) -> wr_beat pops
  logic [15:0] wfifo [0:WFIFO_W-1];
  logic [$clog2(WFIFO_W):0] wf_wp, wf_rp;
  logic wf_err;
  logic wr_beat_ctrl;
  logic [15:0] wdata_ctrl;
  logic [$clog2(WFIFO_W):0] wf_occ;
  assign wf_occ = wf_wp - wf_rp;
  assign wdata_ctrl = wfifo[wf_rp[$clog2(WFIFO_W)-1:0]];

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      wf_wp <= '0;
      wf_rp <= '0;
      wf_err <= 1'b0;
    end else begin
      // The queue now ANSWERS. It used to accept every beat and set a sticky
      // error afterwards if it had not really had room -- which is a report
      // that pixels were lost, not a mechanism for not losing them. One 64-bit
      // beat becomes four 16-bit words, so it is ready exactly when four fit.
      // `wf_err` stays as a tripwire; the overflow branch is now structurally
      // unreachable from the write side.
      if (blit_wvalid && blit_wready) begin
        if (wf_occ > ($bits(wf_occ))'(WFIFO_W - 4)) begin
          wf_err <= 1'b1;                      // overflow: beats dropped
        end else begin
          for (int j = 0; j < 4; j++) begin
            // power-of-two depth: the pointer's low bits ARE the index
            wfifo[($clog2(WFIFO_W))'(wf_wp + ($bits(wf_wp))'(j))]
              <= blit_wdata[16*j +: 16];
          end
          wf_wp <= wf_wp + ($bits(wf_wp))'(4);
        end
      end
      if (wr_beat_ctrl) begin
        if (wf_occ == '0) wf_err <= 1'b1;      // underflow: garbage word
        else wf_rp <= wf_rp + ($bits(wf_rp))'(1);
      end
    end
  end
  assign blit_wready = (wf_occ <= ($bits(wf_occ))'(WFIFO_W - 4));
  assign shell_err_wfifo_o = wf_err;

  // read-beat packer (glue 2): 4 rdata words -> one 64-bit scanout beat
  logic [15:0] ctrl_rdata;
  logic        ctrl_rdata_valid;
  logic [47:0] pack_lo;
  logic [1:0]  pack_cnt;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      pack_lo         <= '0;
      pack_cnt        <= 2'd0;
      scan_beat_valid <= 1'b0;
      scan_beat_data  <= '0;
    end else begin
      scan_beat_valid <= 1'b0;
      if (ctrl_rdata_valid) begin
        if (pack_cnt == 2'd3) begin
          scan_beat_data  <= {ctrl_rdata, pack_lo};
          scan_beat_valid <= 1'b1;
          pack_cnt        <= 2'd0;
        end else begin
          pack_lo[16*pack_cnt +: 16] <= ctrl_rdata;
          pack_cnt                   <= pack_cnt + 2'd1;
        end
      end
    end
  end

  // burst-owner tracking (integrity tripwire, glue 2): reads must be
  // scanout's, writes must be blit's — anything else is a routing bug
  logic route_err;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      route_err <= 1'b0;
    end else if (ctrl_rsp.grant) begin
      if (ctrl_req.write  && (ctrl_req.client != ZHAO_CLIENT_BLIT_DMA))
        route_err <= 1'b1;
      if (!ctrl_req.write && (ctrl_req.client != ZHAO_CLIENT_SCANOUT))
        route_err <= 1'b1;
    end
  end
  assign shell_err_route_o = route_err;

  zhao_sdram_ctrl u_ctrl (
    .clk           (gpu_clk),
    .rst_n         (rst_n),
    .req           (ctrl_req),
    .rsp           (ctrl_rsp),
    .hold_refresh  (hold_refresh),
    .wdata         (wdata_ctrl),
    .wr_beat       (wr_beat_ctrl),
    .rdata         (ctrl_rdata),
    .rdata_valid   (ctrl_rdata_valid),
    .phy_cs_n      (phy_cs_n_o),
    .phy_ras_n     (phy_ras_n_o),
    .phy_cas_n     (phy_cas_n_o),
    .phy_we_n      (phy_we_n_o),
    .phy_a         (phy_a_o),
    .phy_ba        (phy_ba_o),
    .phy_dq_o      (phy_dq_o),
    .phy_dq_oe     (phy_dq_oe_o),
    .phy_dqm       (phy_dqm_o),
    .phy_dq_i      (phy_dq_i),
    .init_done     (init_done_o),
    .refresh_stalls(refresh_stalls_o),
    .bank_conflicts(bank_conflicts_o),
    .refresh_pulse (ctrl_refresh_pulse)
  );

  // HPS bridge: the DMA's burst port through the verified bridge core
  logic [4:0][31:0] hps_bytes, hps_bytes_shadow;

  // ==========================================================================
  // THE BLIT PATH: DEBUG.FRAMEBLIT + VIDEO.SLOTMGR
  // ==========================================================================
  // What this replaces: CMD.DMA's in-house blitter, which carried a whole-canvas
  // staging buffer because the old rule said nothing may be written to VRAM
  // before the checksum passed -- and the `ready_tog` glue below, which stood in
  // for slot ownership with a bare toggle and no notion of which slot was on
  // screen.
  //
  // ---- THE ONE-CYCLE LAW THIS SEAM LIVES OR DIES BY ------------------------
  // DEBUG.FRAMEBLIT latches `fb_lease_generation_i` on the SAME edge it accepts
  // a request. So the lease must already be granted when the request arrives:
  // asking for the lease and handing over the request on one edge makes the
  // blitter latch the generation from BEFORE the grant, and every publication
  // is then refused as stale by a slot manager that is working perfectly.
  //
  // Hence the small sequencer below. It costs two cycles per blit and it is the
  // difference between a machine that works and one that quietly never shows a
  // frame.
  localparam logic [1:0] L_IDLE = 2'd0;   // ready for a dispatch
  localparam logic [1:0] L_LEASE = 2'd1;  // lease asked for; answer lands now
  localparam logic [1:0] L_ISSUE = 2'd2;  // hand the request to the blitter
  localparam logic [1:0] L_RUN = 2'd3;    // the blitter owns it

  logic [1:0] lseq;

  assign blit_req_ready = (lseq == L_IDLE);
  assign slot_lease_req = (lseq == L_IDLE) && dpy_blit_valid;
  assign slot_lease_slot = dpy_blit_dst[0];
  assign nb_req_valid = (lseq == L_ISSUE);

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      lseq <= L_IDLE;
      r_blit_dst <= 8'd0;
      r_blit_mode <= 8'd0;
      r_blit_src <= 32'd0;
      r_blit_len <= 32'd0;
      r_blit_crc <= 32'd0;
    end else begin
      case (lseq)
        L_IDLE: begin
          if (dpy_blit_valid) begin
            r_blit_dst  <= dpy_blit_dst;
            r_blit_mode <= dpy_blit_mode;
            r_blit_src  <= dpy_blit_src;
            r_blit_len  <= dpy_blit_len;
            r_blit_crc  <= dpy_blit_crc;
            lseq <= L_LEASE;
          end
        end
        // The grant (or the refusal) is a registered pulse, so it is visible
        // during this state. Either way the request goes to the blitter: a
        // refused lease is not the shell's to judge, and the blitter answers it
        // with ST_NO_LEASE, which is a status somebody can read.
        L_LEASE: lseq <= L_ISSUE;
        L_ISSUE: if (nb_req_ready) lseq <= L_RUN;
        default: if (nb_done) lseq <= L_IDLE;
      endcase
    end
  end

  zhao_video_slotmgr u_slotmgr (
    .clk                  (gpu_clk),
    .rst_n                (rst_n),
    .lease_req_valid_i    (slot_lease_req),
    .lease_req_slot_i     (slot_lease_slot),
    .lease_grant_o        (slot_lease_grant),
    .lease_refused_o      (slot_lease_refused),
    .fb_lease_valid_o     (fb_lease_valid),
    .fb_lease_slot_o      (fb_lease_slot),
    .fb_lease_generation_o(fb_lease_gen),
    .publish_valid_i      (blit_pub_v),
    .publish_slot_i       (blit_pub_slot),
    .publish_generation_i (blit_pub_gen),
    .release_valid_i      (blit_rel_v),
    .release_slot_i       (blit_rel_slot),
    .release_generation_i (blit_rel_gen),
    .swap_valid_i         (swap_gpu_valid),
    .swap_slot_i          (swap_gpu_slot),
    .slot_ready_o         (slot_ready_gpu),
    .displayed_valid_o    (slot_displayed_v),
    .displayed_slot_o     (slot_displayed_s),
    .slot_state_o         (slot_state_gpu),
    .leases_granted_o     (slot_leases_granted),
    .stale_events_o       (slot_stale_events)
  );

  zhao_debug_frameblit u_frameblit (
    .clk                  (gpu_clk),
    .rst_n                (rst_n),
    .req_valid_i          (nb_req_valid),
    .req_ready_o          (nb_req_ready),
    .req_dst_slot_i       (r_blit_dst),
    .req_mode_i           (r_blit_mode),
    .req_src_i            (r_blit_src),
    .req_len_i            (r_blit_len),
    .req_crc_i            (r_blit_crc),
    .fb_lease_valid_i     (fb_lease_valid),
    .fb_lease_slot_i      (fb_lease_slot),
    .fb_lease_generation_i(fb_lease_gen),
    .release_valid_o      (blit_rel_v),
    .release_slot_o       (blit_rel_slot),
    .release_generation_o (blit_rel_gen),
    .publish_valid_o      (blit_pub_v),
    .publish_slot_o       (blit_pub_slot),
    .publish_generation_o (blit_pub_gen),
    .hps_req_o            (blit_hps_req),
    .hps_req_grant_i      (blit_hps_grant),
    .hps_rsp_i            (blit_hps_rsp),
    .guard_req_o          (blit_guard_req),
    .guard_rsp_i          (blit_guard_rsp),
    .guard_wdata_o        (blit_wdata),
    .guard_wvalid_o       (blit_wvalid),
    .guard_wready_i       (blit_wready),
    .guard_wlast_o        (blit_wlast),
    // Retirement is the VRAM arbiter's credit stream for the blit client. This
    // is the ONLY thing that means "the write landed"; the block's own issue
    // count says nothing about it.
    .retire_words_i       (client_rsp[1].credits),
    .done_o               (nb_done),
    .status_o             (nb_status),
    .blits_published_o    (blits_published),
    .blits_rejected_o     (blits_rejected)
  );

  assign blit_done   = nb_done;
  assign blit_status = nb_status;

  zhao_hps_arbiter u_hps_arb (
    .clk            (gpu_clk),
    .rst_n          (rst_n),
    .c0_req_i       (dma_hps_req),
    .c0_req_grant_o (dma_hps_grant),
    .c0_wr_valid_i  (1'b0),
    .c0_wr_data_i   (64'd0),
    .c0_wr_last_i   (1'b0),
    .c0_rsp_o       (dma_hps_rsp),
    .c1_req_i       (blit_hps_req),
    .c1_req_grant_o (blit_hps_grant),
    .c1_wr_valid_i  (1'b0),
    .c1_wr_data_i   (64'd0),
    .c1_wr_last_i   (1'b0),
    .c1_rsp_o       (blit_hps_rsp),
    .b_req_o        (arb_hps_req),
    .b_req_grant_i  (arb_bridge_grant),
    .b_wr_valid_o   (arb_wr_valid),
    .b_wr_data_o    (arb_wr_data),
    .b_wr_last_o    (arb_wr_last),
    .b_rsp_i        (arb_hps_rsp),
    .c0_bursts_o      (hps_arb_c0_bursts),
    .c1_bursts_o      (hps_arb_c1_bursts),
    .c1_wait_cycles_o (hps_arb_c1_wait)
  );

  zhao_hps_bridge u_bridge (
    .clk           (gpu_clk),
    .rst_n         (rst_n),
    .req           (arb_hps_req),
    .req_grant     (arb_bridge_grant),
    .wr_valid      (arb_wr_valid),
    .wr_data       (arb_wr_data),
    .wr_last       (arb_wr_last),
    .rsp           (arb_hps_rsp),
    .hps_req_valid (hps_req_valid_o),
    .hps_req_write (hps_req_write_o),
    .hps_req_addr  (hps_req_addr_o),
    .hps_req_len   (hps_req_len_o),
    .hps_req_grant (hps_req_grant_i),
    .hps_wr_valid  (hps_wr_valid_o),
    .hps_wr_data   (hps_wr_data_o),
    .hps_wr_last   (hps_wr_last_o),
    .hps_rd_valid  (hps_rd_valid_i),
    .hps_rd_data   (hps_rd_data_i),
    .hps_rd_last   (hps_rd_last_i),
    .frame_tick    (gpu_tick.pulse),
    .hps_bytes     (hps_bytes),
    .hps_bytes_shadow (hps_bytes_shadow),
    .hps_err_count (hps_err_count_o)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_mem;
  assign unused_mem = ^vram_bytes ^ ^hps_bytes ^ scan_gv ^ blit_gv
                    ^ ^scan_gv_req ^ ^blit_gv_req ^ ctrl_refresh_pulse
                    ^ bridge_req_grant ^ blit_hps_grant ^ ^blit_hps_rsp
                    ^ ^hps_arb_c0_bursts ^ ^hps_arb_c1_bursts ^ ^hps_arb_c1_wait
                    ^ ^client_rsp[2] ^ ^client_rsp[3]
                    ^ ^client_rsp[4] ^ ^{client_rsp[0].credits}
                    ^ client_rsp[0].grant ^ client_rsp[1].grant
                    // `guard_wlast_o` marks the last beat of a guard request.
                    // The write queue does not need it: it enqueues four words
                    // per beat and the controller pops them on its own schedule,
                    // so nothing here has to know which beat was final.
                    ^ blit_wlast
                    // The slot manager's observability: exposed for tracing and
                    // for the counters, consumed by neither yet.
                    ^ slot_lease_grant ^ slot_lease_refused
                    ^ ^slot_ready_gpu ^ slot_displayed_v ^ slot_displayed_s
                    ^ ^slot_state_gpu[0] ^ ^slot_state_gpu[1]
                    ^ ^slot_leases_granted ^ ^slot_stale_events
                    ^ ^blits_published ^ ^blits_rejected;
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // GLUE 4 + 5: blit tracking, slot-ready pendings, completion correlator
  // ==========================================================================
  // THE GUARD WINDOW IS THE LEASE. It used to be opened at dispatch and closed
  // at done, which meant the guard's idea of who owned a slot and the machine's
  // idea of it were two separate pieces of bookkeeping that merely happened to
  // agree. Now there is one: while a lease is live the guard admits writes to
  // that slot, and when it ends the window shuts with it.
  assign map_valid_q = fb_lease_valid;
  assign map_slot_q  = fb_lease_slot;
  assign map_span_q  = r_blit_len;

  // Per-FB-slot completion toggles, now driven by a PUBLICATION the slot
  // manager accepted rather than by "the blitter said status zero". A blit that
  // finished and a blit whose slot is still ours are different facts, and only
  // the second one may put a frame on screen.
  logic [1:0] ready_tog;

  // Which FB slot the in-flight packet's blit targets. This is NOT the lease --
  // the lease ends when the blit publishes, while the completion correlator
  // fires at the frame tick, which is later. It is latched at dispatch exactly
  // as it always was, because the correlator's meaning has not changed.
  logic [0:0] run_blit_dst;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      ready_tog    <= 2'b00;
      run_blit_dst <= 1'b0;
    end else begin
      if (dpy_blit_valid && blit_req_ready) run_blit_dst <= dpy_blit_dst[0];
      if (blit_pub_v) ready_tog[blit_pub_slot] <= ~ready_tog[blit_pub_slot];
    end
  end

  // ---- the swap, coming back from the video domain -----------------------
  // Frame control decides the swap in `vid`; the slot manager owns slot state
  // in `gpu`. Only a single-bit toggle crosses, with the slot identity held
  // stable beside it from one swap to the next -- so the gpu side samples a
  // value that has been settled for far longer than the synchronizer depth.
  logic swap_tog_vid, swap_slot_vid;
  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      swap_tog_vid  <= 1'b0;
      swap_slot_vid <= 1'b0;
    end else if (swap_req) begin
      swap_tog_vid  <= ~swap_tog_vid;
      swap_slot_vid <= swap_slot;
    end
  end

  logic sw_g1, sw_g2, sw_g3, ss_g1, ss_g2;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      sw_g1 <= 1'b0; sw_g2 <= 1'b0; sw_g3 <= 1'b0;
      ss_g1 <= 1'b0; ss_g2 <= 1'b0;
    end else begin
      sw_g1 <= swap_tog_vid;
      sw_g2 <= sw_g1;
      sw_g3 <= sw_g2;
      ss_g1 <= swap_slot_vid;
      ss_g2 <= ss_g1;
    end
  end
  assign swap_gpu_valid = (sw_g2 != sw_g3);
  assign swap_gpu_slot  = ss_g2;

  // vid domain: pending registers (2FF per toggle, set on edge, cleared
  // when FRAMECTL consumes the slot at the swap decision; set wins)
  logic [1:0] rt_s1, rt_s2, rt_s3;
  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      rt_s1 <= 2'b00;
      rt_s2 <= 2'b00;
      rt_s3 <= 2'b00;
      slot_ready_pending <= 2'b00;
    end else begin
      rt_s1 <= ready_tog;
      rt_s2 <= rt_s1;
      rt_s3 <= rt_s2;
      for (int d = 0; d < 2; d++) begin
        if (rt_s2[d] != rt_s3[d]) begin
          slot_ready_pending[d] <= 1'b1;   // set wins (header note)
        end else if (swap_req && (swap_slot == 1'(d))) begin
          slot_ready_pending[d] <= 1'b0;
        end
      end
    end
  end

  // completion correlator (gpu): a fresh (non-repeat) tick displaying the
  // RUN slot's blit target completes that ring slot
  logic       any_run;
  logic [1:0] run_slot;
  always_comb begin
    any_run  = 1'b0;
    run_slot = 2'd0;
    for (int s = 0; s < 3; s++) begin
      if (slot_state_o[s] == 3'd3) begin   // S_RUN (charter encoding)
        any_run  = 1'b1;
        run_slot = 2'(s);
      end
    end
  end
  assign frame_complete = gpu_tick.pulse && !gpu_tick.repeated && any_run
                        && (gpu_complete_slot == run_blit_dst);
  assign frame_complete_slot = run_slot;

  // ==========================================================================
  // GLUE 6: mode CDC (gpu -> vid) with a 2-cycle stability filter
  // ==========================================================================
  logic [1:0] md_s1, md_s2, md_s3, md_cur;
  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      md_s1 <= 2'd0;
      md_s2 <= 2'd0;
      md_s3 <= 2'd0;
      md_cur <= 2'd0;
      mode_we <= 1'b0;
      mode_in <= 2'd0;
    end else begin
      md_s1 <= 2'(sched_mode);
      md_s2 <= md_s1;
      md_s3 <= md_s2;
      mode_we <= 1'b0;
      if ((md_s2 == md_s3) && (md_s2 != md_cur)) begin
        md_cur  <= md_s2;
        mode_in <= md_s2;
        mode_we <= 1'b1;
      end
    end
  end

  // ==========================================================================
  // GLUE 7: the displayed CRC, now ENTIRELY INSIDE vid_clk (2026-08-22)
  // ==========================================================================
  // WHAT USED TO BE HERE, and why it had to go. The CRC ran on gpu_clk, so
  // this file sampled the vid-domain pixel register (px_out.valid, .rgb565,
  // .x, .y) from GPU logic on a vid phase toggle and unpacked it into two
  // gpu-cycle bytes. Per-pixel state crossed the clock boundary on every
  // active pixel, and it was correct only because the SIMULATION freezes
  // vid_clk = gpu_clk/2 with coincident posedges (plan R1). TimeQuest
  // measured what that costs in silicon: the two hold violations reported on
  // `vid_clk -> gpu_clk` under the HIGH PERFORMANCE fitter effort were both
  // on this seam, and a hold violation is not a speed problem — no clock is
  // slow enough to fix data that arrives too early.
  //
  // Fabian's ruling (docs/OWNER_DOCKET.md, "RULED 2026-08-22 — BALANCED stays
  // authoritative"): move the displayed CRC into vid_clk rather than crossing
  // per-pixel state. That also settled a standing contradiction —
  // design/contracts/DEBUG.CRC.md had always said "`vid_clk` domain for the
  // displayed-stream lane", design/blocks.yml said `clock_domain: gpu`, and
  // the code had followed the ledger. The contract was right.
  //
  // What crosses now: nothing per-pixel. `zhao_debug_crc` sits in vid_clk and
  // consumes the serializer's stream natively — one pixel per vid clock, two
  // bytes folded in one XOR tree. Only the FINALISED result crosses, once per
  // displayed frame, as a toggle with its value held stable beside it for the
  // whole frame that follows: hundreds of thousands of clocks of settling
  // against a three-flop synchronizer, instead of sixteen data bits re-timed
  // every pixel.

  // ---- vid side: framing markers, from vid-domain signals only ------------
  logic [15:0] px_active_w;
  assign px_active_w = ZHAO_TIMING[vmode].h_active;

  logic px_is_sof, px_is_eof;
  assign px_is_sof = px_out.valid && (px_out.x == 10'd0) && (px_out.y == 8'd0);
  assign px_is_eof = px_out.valid && (px_out.x == 10'(px_active_w - 16'd1))
                     && (px_out.y == 8'd239);

  // expect_bytes is zhao_displayed_bytes(mode) — NOT zhao_canvas_bytes: for
  // Duo those differ (245,760 displayed vs 196,608 stored) and the canvas
  // value here would be the documented silent-Duo bug.
  logic [31:0] crc_expect_bytes;
  assign crc_expect_bytes = zhao_displayed_bytes(vmode);

  logic [31:0] crc_frame_vid, crc_bytes_vid;
  logic        crc_valid_vid, crc_err_vid;

  zhao_debug_crc u_crc (
    .clk               (vid_clk),
    .rst_n             (rst_n),
    .in_valid_i        (px_out.valid),
    .in_px_i           (px_out.rgb565),
    .in_sof_i          (px_is_sof),
    .in_eof_i          (px_is_eof),
    .expect_bytes_i    (crc_expect_bytes),
    .frame_crc_o       (crc_frame_vid),
    .frame_crc_valid_o (crc_valid_vid),
    .bytes_captured_o  (crc_bytes_vid),
    .size_err_evt_o    (crc_err_vid)
  );

  // ---- the only crossing left on this lane: vid -> gpu, once per frame ----
  // A publish and a size-error each toggle their own bit; the reported values
  // register beside them and then hold. Publish and size-error are mutually
  // exclusive in the block (a frame either finalizes or is flagged), but they
  // are carried separately so a burst of raster violations cannot be mistaken
  // for a published frame.
  logic        crc_pub_tog_vid, crc_err_tog_vid;
  logic [31:0] crc_frame_hold, crc_bytes_hold;
  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      crc_pub_tog_vid <= 1'b0;
      crc_err_tog_vid <= 1'b0;
      crc_frame_hold  <= 32'd0;
      crc_bytes_hold  <= 32'd0;
    end else begin
      if (crc_valid_vid) begin
        crc_pub_tog_vid <= ~crc_pub_tog_vid;
        crc_frame_hold  <= crc_frame_vid;
        crc_bytes_hold  <= crc_bytes_vid;
      end
      if (crc_err_vid) begin
        crc_err_tog_vid <= ~crc_err_tog_vid;
        crc_bytes_hold  <= crc_bytes_vid;
      end
    end
  end

  // gpu side: 3FF per toggle, edge-detected into a ONE-gpu-cycle pulse. The
  // held value is sampled at the same edge as the pulse it belongs to, so the
  // port pair {crc_valid_o, crc_frame_o} stays aligned for a consumer that
  // reads both after a gpu edge (which is what the harness does).
  logic cp_g1, cp_g2, cp_g3, ce_g1, ce_g2, ce_g3;
  logic crc_pub_edge, crc_err_edge;
  assign crc_pub_edge = (cp_g2 != cp_g3);
  assign crc_err_edge = (ce_g2 != ce_g3);

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      cp_g1 <= 1'b0; cp_g2 <= 1'b0; cp_g3 <= 1'b0;
      ce_g1 <= 1'b0; ce_g2 <= 1'b0; ce_g3 <= 1'b0;
      crc_frame_o    <= 32'd0;
      crc_bytes_o    <= 32'd0;
      crc_valid_o    <= 1'b0;
      crc_size_err_o <= 1'b0;
    end else begin
      cp_g1 <= crc_pub_tog_vid;
      cp_g2 <= cp_g1;
      cp_g3 <= cp_g2;
      ce_g1 <= crc_err_tog_vid;
      ce_g2 <= ce_g1;
      ce_g3 <= ce_g2;
      crc_valid_o    <= crc_pub_edge;
      crc_size_err_o <= crc_err_edge;
      if (crc_pub_edge) crc_frame_o <= crc_frame_hold;
      if (crc_pub_edge || crc_err_edge) crc_bytes_o <= crc_bytes_hold;
    end
  end

  // ==========================================================================
  // GLUE 3: record framer (packet byte stream -> scheduler record port)
  // ==========================================================================
  localparam int unsigned FQW = $clog2(FRAMER_Q);

  logic [31:0] f_pos;      // byte index within the packet
  logic [15:0] f_op;       // current record: opcode
  logic [15:0] f_len;      // current record: record_bytes
  logic [15:0] f_rpos;     // byte index within the current record
  logic [127:0] f_w;       // payload dwords w0..w3 (record bytes [16,32))

  logic [143:0] recq [0:FRAMER_Q-1];   // {op, w3, w2, w1, w0}
  logic [FQW:0] rq_wp, rq_rp;
  logic [FQW:0] rq_occ;
  assign rq_occ = rq_wp - rq_rp;

  logic rq_full, framer_err;
  assign rq_full = (rq_occ >= (FQW+1)'(FRAMER_Q));

  // the byte on the wires completes a record exactly when its position is
  // the record's last byte (f_len valid from byte 4 on; records are >=16 B)
  logic in_rec_region, rec_completes_now;
  // `pkt_len - 32'd4` USED TO BE COMPUTED HERE, in series with the comparison
  // that gates the record write. MEASURED on the composed shell fit of
  // 2026-08-24: the `f_pos -> recq[*]` family is the largest group of failing
  // setup endpoints on gpu_clk (~-0.765 ns at ~10.6 ns of data delay), and this
  // is a 32-bit subtract standing directly in front of a 32-bit compare whose
  // result is the write enable of a 144-bit array.
  //
  // EXACT, not approximately equal. `f_pos` resets to 0 whenever
  // `f_pos + 1 >= pkt_len`, so `f_pos < pkt_len` always holds, and
  // `in_rec_region` additionally requires `f_pos >= 36`. A packet therefore
  // cannot reach the region until 36 accepted bytes after its own first byte,
  // and a copy of `pkt_len - 4` that lags by ONE cycle has been correct for 35
  // cycles by the time anything consults it. The lag is only observable across
  // a packet boundary, which is exactly where the region test is false anyway.
  //
  // Underflow semantics are preserved deliberately: the registered expression
  // is the same expression, so a `pkt_len < 4` wraps identically to before.
  //
  // ENFORCED-BY: tests/shell/shell_golden.cpp:main
  logic [31:0] pkt_len_m4_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) pkt_len_m4_q <= 32'd0;
    else        pkt_len_m4_q <= pkt_len - 32'd4;
  end

  assign in_rec_region = (f_pos >= 32'd36) && (f_pos < pkt_len_m4_q);
  assign rec_completes_now = in_rec_region && (f_rpos >= 16'd4)
                           && (f_rpos + 16'd1 == f_len);

  // stall the LAST byte of a record while the queue is full (glue 3 law)
  assign pkt_ready = !(rec_completes_now && rq_full);

  // the record's payload dwords INCLUDING the byte on the wires this cycle
  logic [127:0] w_final;
  always_comb begin
    w_final = f_w;
    if ((f_rpos >= 16'd16) && (f_rpos < 16'd32))
      w_final[8*(f_rpos - 16'd16) +: 8] = pkt_byte;
  end

  assign rec_valid  = (rq_occ != '0);
  assign rec_opcode = recq[rq_rp[FQW-1:0]][143:128];
  assign rec_w0     = recq[rq_rp[FQW-1:0]][31:0];
  assign rec_w1     = recq[rq_rp[FQW-1:0]][63:32];
  assign rec_w2     = recq[rq_rp[FQW-1:0]][95:64];
  assign rec_w3     = recq[rq_rp[FQW-1:0]][127:96];

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      f_pos  <= 32'd0;
      f_op   <= 16'd0;
      f_len  <= 16'd0;
      f_rpos <= 16'd0;
      f_w    <= 128'd0;
      rq_wp  <= '0;
      rq_rp  <= '0;
      framer_err <= 1'b0;
    end else begin
      // pop (presentation side)
      if (rec_valid && rec_ready) rq_rp <= rq_rp + (FQW+1)'(1);

      // deadlock tripwire: the stream is stalled on a full queue while the
      // head cannot drain because the scheduler is backpressuring — a
      // packet shape the composition cannot execute (header, glue 3)
      if (pkt_valid && !pkt_ready && rec_valid && !rec_ready)
        framer_err <= 1'b1;

      // consume (byte side)
      if (pkt_valid && pkt_ready) begin
        if (in_rec_region) begin
          if (f_rpos == 16'd0) begin
            f_w        <= 128'd0;
            f_op[7:0]  <= pkt_byte;
          end else if (f_rpos == 16'd1) begin
            f_op[15:8] <= pkt_byte;
          end else if (f_rpos == 16'd2) begin
            f_len[7:0] <= pkt_byte;
          end else if (f_rpos == 16'd3) begin
            f_len[15:8] <= pkt_byte;
          end else if ((f_rpos >= 16'd16) && (f_rpos < 16'd32)) begin
            f_w[8*(f_rpos - 16'd16) +: 8] <= pkt_byte;
          end
          if (rec_completes_now) begin
            // push {op, w} — w_final patches THIS byte in when the final
            // byte itself lands inside [16,32) (exactly the 32-B records);
            // a 16-B record's w stays the zero-fill (NOP has no payload)
            recq[rq_wp[FQW-1:0]] <= {f_op, w_final};
            rq_wp  <= rq_wp + (FQW+1)'(1);
            f_rpos <= 16'd0;
          end else begin
            f_rpos <= f_rpos + 16'd1;
          end
        end
        // position bookkeeping (header/tail bytes just count)
        if (f_pos + 32'd1 >= pkt_len) begin
          f_pos  <= 32'd0;
          f_rpos <= 16'd0;
        end else begin
          f_pos <= f_pos + 32'd1;
        end
      end
    end
  end
  assign shell_err_framer_o = framer_err;

  // ==========================================================================
  // INPUT: snapshot + rumble (gpu domain, tick-latched)
  // ==========================================================================
  zhao_pad_frame_t pad_frame [0:3];
  logic [31:0] pad_frame_id;
  logic        input_gap_evt;

  zhao_input_snapshot u_snapshot (
    .clk         (gpu_clk),
    .rst_n       (rst_n),
    .pad_present (pad_present_i),
    .pad_buttons (pad_buttons_i),
    .pad_lx      (pad_lx_i),
    .pad_ly      (pad_ly_i),
    .pad_rx      (pad_rx_i),
    .pad_ry      (pad_ry_i),
    .frame_tick  (gpu_tick),
    .pad_frame   (pad_frame),
    .pad_frame_flat (pad_frame_flat_o),
    .pad_frame_id   (pad_frame_id),
    .pad_sequence   (pad_sequence_o),
    .input_sequence_gaps   (input_gaps_o),
    .input_sequence_gap_evt(input_gap_evt)
  );

  // rumble edge converter (glue 9): one pulse per NEW dispatch
  logic        rum_prev_v;
  logic [23:0] rum_prev_pl;
  logic        rum_pulse;
  assign rum_pulse = dpy_rumble_valid
                   && (!rum_prev_v
                       || (rum_prev_pl != {dpy_rumble_pad, dpy_rumble_en,
                                           dpy_rumble_str}));
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      rum_prev_v  <= 1'b0;
      rum_prev_pl <= 24'd0;
    end else begin
      rum_prev_v  <= dpy_rumble_valid;
      if (dpy_rumble_valid)
        rum_prev_pl <= {dpy_rumble_pad, dpy_rumble_en, dpy_rumble_str};
    end
  end

  zhao_input_rumble u_rumble (
    .clk              (gpu_clk),
    .rst_n            (rst_n),
    .rumble_cmd_valid (rum_pulse),
    .rumble_pad_index (dpy_rumble_pad),
    .rumble_enable    (dpy_rumble_en),
    .rumble_strength  (dpy_rumble_str),
    .frame_tick       (gpu_tick),
    .rumble_duty      (rumble_duty_o),
    .rumble_active    (rumble_active_o),
    .rumble_pwm       (rumble_pwm_o),
    .rumble_frames_dropped (rumble_drops_o)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_input;
  assign unused_input = input_gap_evt ^ ^pad_frame_id
                      ^ ^{pad_frame[0], pad_frame[1], pad_frame[2], pad_frame[3]};
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // AUDIO: the D4 FIFO (write side fed by the harness ring-read seam)
  // ==========================================================================
  zhao_counter_snap_t fifo_snap;

  zhao_audio_fifo u_fifo (
    .clk_gpu          (gpu_clk),
    .rst_gpu_n        (rst_n),
    .wr_valid_i       (aud_wr_valid_i),
    .wr_l_i           (aud_wr_l_i),
    .wr_r_i           (aud_wr_r_i),
    .wr_ready_o       (aud_wr_ready_o),
    .refill_req_o     (aud_refill_req_o),
    .occupancy_o      (aud_occupancy_o),
    .frame_tick_i     (gpu_tick.pulse),
    .cnt_snap_o       (fifo_snap),
    .clk_audio        (audio_clk),
    .rst_audio_n      (rst_n),
    .pcm_valid_o      (pcm_valid_o),
    .pcm_l_o          (pcm_l_o),
    .pcm_r_o          (pcm_r_o),
    .underrun_status_o(underrun_status_o),
    .audio_underruns_o(audio_underruns_o)
  );

  // ==========================================================================
  // GLUE 8: counter provider adapters + DEBUG.COUNTERS
  // ==========================================================================
  logic tick_d1;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) tick_d1 <= 1'b0;
    else        tick_d1 <= gpu_tick.pulse;
  end

  // ---- the starvation crossing, as a SNAPSHOT MAILBOX ---------------------
  // This used to sample the whole 64-bit vid_clk counter straight into gpu_clk
  // and check AFTERWARDS whether it had moved:
  //
  //     starve_samp <= starvation_o;
  //     if (tick_d1 && (starvation_o != starve_samp)) cdc_err <= 1'b1;
  //
  // The premise -- the counter only advances during active lines, the tick
  // lands in vblank -- was reasonable. The STRUCTURE was not: a value
  // comparison can notice that a bus moved, it cannot make a metastable sample
  // safe, and it says nothing about the 62 bits that did not move.
  //
  // MEASURED, and this is why it changed. Across FOUR composed fits that
  // touched nothing in this path, the crossing's hold slack read
  //
  //     -0.952 FAIL   +0.254 pass   +0.259 pass   -0.728 FAIL
  //
  // 1.2 ns of swing on placement alone, which made the shell's timing verdict
  // NONDETERMINISTIC -- worse than permanently red, because a verdict that
  // flips at random cannot be trusted in either direction and a real
  // regression arriving on a lucky fit is indistinguishable from luck. It had
  // already hidden one: the true worst synchronous path (-0.875 ns) sat behind
  // this crossing's -1.991.
  //
  // Published once per VID-DOMAIN frame, which is the rate the consumer wants:
  // `prov[6]` samples at `tick_d1`, once per frame.
  //
  // ENFORCED-BY: tests/formal/cdc_snapshot.sby
  logic [63:0] starve_snap;
  logic        starve_snap_valid;
  logic        starve_ovf;
  // Named rather than left empty: this lane forbids empty-by-name pin
  // connections, and the signal is genuinely observable -- it says the video
  // side is holding a snapshot the GPU side has not taken yet.
  logic        starve_busy;

  zhao_cdc_snapshot #(.W(64)) u_starve_mbx (
      .src_clk      (vid_clk),
      .src_rst_n    (rst_n),
      .src_publish_i(frame_tick_vid),
      .d_i          (starvation_o),
      .src_busy_o   (starve_busy),
      .ovf_o        (starve_ovf),
      .dst_clk      (gpu_clk),
      .dst_rst_n    (rst_n),
      .q_o          (starve_snap),
      .q_valid_o    (starve_snap_valid)
  );

  // The error bit now means something a person can act on: the GPU side failed
  // to collect a snapshot before the video side had another to publish. The
  // old bit meant "the counter moved while I happened to be looking", which is
  // a statement about luck.
  assign shell_err_cdc_o = starve_ovf;

  // summed byte counters (u64; each shadow saturates at u32 individually)
  logic [63:0] vram_total, hps_total;
  always_comb begin
    vram_total = 64'd0;
    hps_total  = 64'd0;
    for (int k = 0; k < 5; k++) begin
      vram_total = vram_total + {32'd0, vram_bytes_shadow[k]};
      hps_total  = hps_total  + {32'd0, hps_bytes_shadow[k]};
    end
  end

  zhao_counter_snap_t prov [0:8];
  always_comb begin
    prov[0] = sched_snap_cycles;                                  // id 0
    prov[1] = sched_snap_faults;                                  // id 1
    prov[2] = sched_snap_cmds;                                    // id 2
    prov[3] = fifo_snap;                                          // id 31
    prov[4] = '{valid: tick_d1, counter_id: ZHAO_CNT_VRAM_BYTES,
                value: vram_total};                               // id 28
    prov[5] = '{valid: tick_d1, counter_id: ZHAO_CNT_HPS_BYTES,
                value: hps_total};                                // id 29
    // READS THE SNAPSHOT, NOT THE CROSSING. This line used to take
    // `starvation_o` -- a vid_clk value -- directly on a gpu_clk tick, so the
    // counter itself sampled across the domain boundary and the tripwire
    // above only watched it happen. `starve_snap_valid` gates the first
    // frame, before any snapshot has been collected, to zero rather than to
    // an undefined mailbox.
    prov[6] = '{valid: tick_d1, counter_id: ZHAO_CNT_SCANOUT_STARVE,
                value: starve_snap_valid ? starve_snap : 64'd0};  // id 30
    prov[7] = '{valid: tick_d1, counter_id: ZHAO_CNT_INPUT_SEQ_GAPS,
                value: input_gaps_o};                             // id 35
    prov[8] = '{valid: tick_d1, counter_id: ZHAO_CNT_RUMBLE_DROPPED,
                value: rumble_drops_o};                           // id 36
  end

  zhao_counter_snap_t cnt_snap;

  zhao_debug_counters #(
    .PROV_N      (9),
    .CATALOG_IDS (40)
  ) u_counters (
    .clk             (gpu_clk),
    .rst_n           (rst_n),
    .frame_tick_i    (gpu_tick),
    .prov_i          (prov),
    .snap_valid_o    (cnt_snap_valid_o),
    .snap_ready_i    (cnt_snap_ready_i),
    .snap_o          (cnt_snap),
    .window_open_o   (cnt_window_open_o),
    .cat_violation_o (cnt_cat_violation_o)
  );

  assign cnt_snap_id_o    = cnt_snap.counter_id;
  assign cnt_snap_value_o = cnt_snap.value;

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_cnt;
  assign unused_cnt = cnt_snap.valid;   // window-open is the level law
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : zhao_shell_top
