// zhao_video_tb.sv — Verilator integration wrapper for the W2.2 VIDEO
// subsystem differential tests (plan W2.2; harness convention: tests/).
//
// The harness C++ drives BOTH clocks with a FIXED phase (risk R1): gpu_clk
// toggles every step and vid_clk = gpu_clk/2 (posedges on odd steps — the
// vid_test helpers in the .cpp files encode the same law the zref oracles
// mirror). Everything else (guard responder, slot-READY timelines, mode
// writes) is harness-owned; this file is pure wiring, flattened ports for
// clean C++ access (struct ports are flattened at this boundary only).
//
// TESTBENCH COMPONENT — excluded from synthesis; linted clean anyway.

module zhao_video_tb
  import zhao_pkg::*;
(
  // clocks + reset (harness-driven, fixed phase)
  input  logic        gpu_clk,
  input  logic        vid_clk,
  input  logic        rst_n,

  // mode register port (VID: zhao_video_mode)
  input  logic        mode_we,
  input  logic [1:0]  mode_in,

  // FRAMECTL stimulus (VID; deadline in gpu cycles, 0 = mode default)
  input  logic [1:0]  slot_ready,
  input  logic [31:0] deadline_cycles,

  // MEM.GUARD responder (GPU; harness answers admission + beats)
  input  logic        guard_ready,
  input  logic        guard_ok,
  input  logic        guard_violation,
  input  logic        beat_valid,
  input  logic [63:0] beat_data,
  input  logic        beat_last,

  // SCALER sink (VID)
  input  logic        px_out_ready,

  // ---- observability (flattened) ------------------------------------------
  // VIDEO.MODE raster
  output logic [15:0] o_x,
  output logic [15:0] o_y,
  output logic        o_hsync, o_vsync, o_hblank, o_vblank,
  output logic        o_frame_start, o_frame_end, o_vswap_dec,
  output logic [1:0]  o_mode, o_mode_next,

  // fetch client (GPU)
  output logic        o_req_valid,
  output logic        o_req_write,
  output logic [26:0] o_req_addr,
  output logic [6:0]  o_req_len,
  output logic [2:0]  o_req_client,
  output logic [63:0] o_req_be,

  // pixel stream (VID, after SCALER — the displayed stream)
  output logic        o_px_valid,
  output logic [15:0] o_px_rgb,
  output logic [9:0]  o_px_x,
  output logic [7:0]  o_px_y,
  output logic        o_px_hsync, o_px_vsync, o_px_hblank, o_px_vblank,
  output logic        o_scaler_violation,

  // FRAMECTL (VID)
  output logic        o_frame_tick,
  output logic [31:0] o_frame_id,
  output logic        o_repeated,
  output logic        o_swap_req,
  output logic [0:0]  o_swap_slot,
  output logic        o_swap_ack,
  output logic [63:0] o_deadline_faults,
  output logic [63:0] o_frame_cycles,
  output logic [31:0] o_deadline_margin,

  // FRAMECTL gpu broadcast (toggle+2FF crossed)
  output logic        o_gpu_tick,
  output logic [31:0] o_gpu_tick_frame_id,
  output logic        o_gpu_tick_repeated,
  output logic [0:0]  o_gpu_complete_slot,

  // SCANOUT (VID)
  output logic [63:0] o_starvation,

  // debug observability (differential bring-up only)
  output logic [3:0]  o_dbg_bstate,     // linebuf per-buffer state {b1,b0}
  output logic [1:0]  o_dbg_full_tog,   // gpu fill-completion toggles
  output logic [1:0]  o_dbg_cons_tog,   // vid consumption toggles
  output logic [1:0]  o_dbg_buf_fresh,  // vid freshness flags
  output logic        o_dbg_disp_buf,   // serializer ping-pong select
  output logic        o_dbg_line_fresh,
  output logic [1:0]  o_dbg_consume_start,
  output logic [1:0]  o_dbg_consume_done,
  output logic        o_dbg_dec_sync,
  output logic        o_dbg_fs_sync,
  output logic [1:0]  o_dbg_last_seen,
  output logic [1:0]  o_dbg_full_s2
);

  // ------------------------------------------------------------ internals --
  logic [15:0] x, y;
  logic        hsync, vsync, hblank, vblank, frame_start, frame_end, vswap_dec;
  zhao_mode_e  mode, mode_next;

  zhao_px_stream_t px_ser, px_out;

  logic        swap_req;
  logic [0:0]  swap_slot;
  logic        swap_ack;

  logic        frame_tick, repeated;
  logic [31:0] frame_id, deadline_margin;
  logic [63:0] deadline_faults, frame_cycles, starvation;

  zhao_frame_tick_t gpu_tick;

  zhao_guard_req_t guard_req;
  zhao_guard_rsp_t guard_rsp;
  assign guard_rsp.ready     = guard_ready;
  assign guard_rsp.ok        = guard_ok;
  assign guard_rsp.violation = guard_violation;

  // ------------------------------------------------------------ instances --
  zhao_video_mode u_mode (
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .mode_we     (mode_we),
    .mode_in     (mode_in),
    .x           (x),
    .y           (y),
    .hsync       (hsync),
    .vsync       (vsync),
    .hblank      (hblank),
    .vblank      (vblank),
    .frame_start (frame_start),
    .frame_end   (frame_end),
    .vswap_dec   (vswap_dec),
    .mode_out    (mode),
    .mode_next   (mode_next)
  );

  zhao_video_scanout u_scanout (
    .gpu_clk     (gpu_clk),
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .guard_req   (guard_req),
    .guard_rsp   (guard_rsp),
    .beat_valid  (beat_valid),
    .beat_data   (beat_data),
    .beat_last   (beat_last),
    .x           (x),
    .y           (y),
    .hsync       (hsync),
    .vsync       (vsync),
    .hblank      (hblank),
    .vblank      (vblank),
    .frame_start (frame_start),
    .vswap_dec   (vswap_dec),
    .mode        (mode),
    .mode_next   (mode_next),
    .swap_req    (swap_req),
    .swap_slot   (swap_slot),
    .swap_ack    (swap_ack),
    .px          (px_ser),
    .starvation_cycles (starvation)
  );

  zhao_video_scaler u_scaler (
    .vid_clk     (vid_clk),
    .rst_n       (rst_n),
    .in          (px_ser),
    .out         (px_out),
    .out_ready   (px_out_ready),
    .never_active(o_scaler_violation)
  );

  zhao_video_framectl u_framectl (
    .vid_clk        (vid_clk),
    .rst_n          (rst_n),
    .x              (x),
    .y              (y),
    .vblank         (vblank),
    .vswap_dec      (vswap_dec),
    .frame_start    (frame_start),
    .mode           (mode),
    .slot_ready     (slot_ready),
    .deadline_cycles(deadline_cycles),
    .swap_req       (swap_req),
    .swap_slot      (swap_slot),
    .swap_ack       (swap_ack),
    .frame_repeated (repeated),
    .frame_tick     (frame_tick),
    .frame_id       (frame_id),
    .frame_cycles   (frame_cycles),
    .deadline_faults(deadline_faults),
    .deadline_margin(deadline_margin),
    .gpu_clk        (gpu_clk),
    .gpu_tick       (gpu_tick),
    .gpu_complete_slot (o_gpu_complete_slot)
  );

  // ------------------------------------------------------------ flattened --
  assign o_x           = x;
  assign o_y           = y;
  assign o_hsync       = hsync;
  assign o_vsync       = vsync;
  assign o_hblank      = hblank;
  assign o_vblank      = vblank;
  assign o_frame_start = frame_start;
  assign o_frame_end   = frame_end;
  assign o_vswap_dec   = vswap_dec;
  assign o_mode        = mode;
  assign o_mode_next   = mode_next;

  assign o_req_valid   = guard_req.valid;
  assign o_req_write   = guard_req.write;
  assign o_req_addr    = guard_req.addr;
  assign o_req_len     = guard_req.len;
  assign o_req_client  = guard_req.client;
  assign o_req_be      = guard_req.be;

  assign o_px_valid    = px_out.valid;
  assign o_px_rgb      = px_out.rgb565;
  assign o_px_x        = px_out.x;
  assign o_px_y        = px_out.y;
  assign o_px_hsync    = px_out.hsync;
  assign o_px_vsync    = px_out.vsync;
  assign o_px_hblank   = px_out.hblank;
  assign o_px_vblank   = px_out.vblank;

  assign o_frame_tick  = frame_tick;
  assign o_frame_id    = frame_id;
  assign o_repeated    = repeated;
  assign o_swap_req    = swap_req;
  assign o_swap_slot   = swap_slot;
  assign o_swap_ack    = swap_ack;
  assign o_deadline_faults = deadline_faults;
  assign o_frame_cycles= frame_cycles;
  assign o_deadline_margin = deadline_margin;

  assign o_gpu_tick         = gpu_tick.pulse;
  assign o_gpu_tick_frame_id= gpu_tick.frame_id;
  assign o_gpu_tick_repeated= gpu_tick.repeated;

  assign o_starvation  = starvation;

  // debug taps (hierarchical; testbench-only)
  assign o_dbg_bstate = {u_scanout.u_linebuf.bstate[1],
                         u_scanout.u_linebuf.bstate[0]};
  assign o_dbg_full_tog = u_scanout.u_linebuf.full_toggle;
  assign o_dbg_cons_tog = u_scanout.u_linebuf.consumed_toggle;
  assign o_dbg_buf_fresh = u_scanout.u_linebuf.buf_fresh;
  assign o_dbg_disp_buf = u_scanout.u_ser.display_buf;
  assign o_dbg_line_fresh = u_scanout.u_ser.line_fresh;
  assign o_dbg_consume_start = u_scanout.u_ser.consume_start;
  assign o_dbg_consume_done = u_scanout.u_ser.consume_done;
  assign o_dbg_dec_sync = u_scanout.dec_sync;
  assign o_dbg_fs_sync  = u_scanout.frame_start_sync;
  assign o_dbg_last_seen = u_scanout.u_linebuf.last_seen;
  assign o_dbg_full_s2   = u_scanout.u_linebuf.full_s2;

endmodule : zhao_video_tb
