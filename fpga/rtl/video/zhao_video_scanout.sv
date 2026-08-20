// zhao_video_scanout.sv — VIDEO.SCANOUT top: fetch + line buffers +
// serializer + the in-vblank swap execution (plan W2.2; law:
// spec/video_rules.md §3-§4, decision D7; contract VIDEO.SCANOUT).
//
// Decomposition (D7, one file per submodule beside this top):
//   fpga/rtl/video/zhao_scanout_fetch.sv      gpu-domain VRAM read client
//   fpga/rtl/video/zhao_scanout_linebuf.sv    2x512 RGB565 ping-pong + bridge
//   fpga/rtl/video/zhao_scanout_serializer.sv vid-domain pixel emitter
//   fpga/rtl/video/zhao_video_scanout.sv      this top: wiring + crossings
//
// Domain picture (CDC documented per plan R1; ratio vid=gpu/2 frozen, D1):
//
//   gpu domain                          vid domain
//   ---------                          -----------
//   zhao_scanout_fetch ----wr----> zhao_scanout_linebuf <----rd---- serializer
//        ^  \empty(full_toggle<)----/ (2FF gray toggles, 1 bit each way)
//        |  dec_sync / frame_start_sync <== toggle+2FF+edge <= vswap_dec /
//        |                                          frame_start (from MODE)
//        |  display_slot_sync / mode_next_sync / mode_sync <= 2FF <= regs
//   swap execution: FRAMECTL pulses swap_req{slot} DURING the vswap_dec
//   cycle (combinational decision — contract FRAMECTL latency note); this
//   top registers display_slot at that edge, so the 2FF-crossed copy is
//   settled BEFORE the dec_sync re-arm pulse reaches the fetch side
//   (toggle+edge crossing is one gpu cycle slower than the data 2FF —
//   checked by the video_scanout_directed differential).
//
// The raster never stalls (D1): if the line buffers are not fresh the
// serializer re-emits the last valid pixel and counts starvation; if no
// slot was READY at the decision the previous frame's slot is retained
// (the repeat half of the 60 Hz law — FRAMECTL owns the decision, this
// module only executes the swap in vblank).
//
// Conservative SystemVerilog subset only (charter §2).
// Lint: clean under `verilator_bin --lint-only -Wall` (CTest
// lint_zhao_video_scanout).

module zhao_video_scanout
  import zhao_pkg::*;
(
  input  logic        gpu_clk,
  input  logic        vid_clk,
  input  logic        rst_n,

  // MEM.GUARD read client port + beat return (gpu domain, W2.5 seam)
  output zhao_guard_req_t guard_req,
  input  zhao_guard_rsp_t  guard_rsp,
  input  logic        beat_valid,
  input  logic [63:0] beat_data,
  input  logic        beat_last,

  // raster from VIDEO.MODE (vid domain)
  input  logic [15:0] x,
  input  logic [15:0] y,
  input  logic        hsync,
  input  logic        vsync,
  input  logic        hblank,
  input  logic        vblank,
  input  logic        frame_start,
  input  logic        vswap_dec,
  input  zhao_mode_e  mode,
  input  zhao_mode_e  mode_next,

  // FRAMECTL swap handshake (vid domain, vblank only)
  input  logic        swap_req,
  input  logic [0:0]  swap_slot,
  output logic        swap_ack,

  // pixel stream out (to VIDEO.SCALER) + starvation counter
  output zhao_px_stream_t px,
  output logic [63:0] starvation_cycles
);

  // ------------------------------------------------- swap execution (vid) --
  logic [0:0] display_slot;

  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      display_slot <= 1'b0;
      swap_ack     <= 1'b0;
    end else begin
      swap_ack <= swap_req;   // ack one cycle after the command
      if (swap_req) begin
        display_slot <= swap_slot;
      end
    end
  end

  // --------------------------------------------- vid -> gpu control pulses --
  // toggle + 2-flop + edge: one gpu cycle slower than a plain 2FF, which
  // the settled-before-sample rule above relies on.
  logic dec_tog, dec_s1, dec_s2, dec_s2q, dec_sync;
  logic fs_tog,  fs_s1,  fs_s2,  fs_s2q,  frame_start_sync;

  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      dec_tog <= 1'b0;
      fs_tog  <= 1'b0;
    end else begin
      if (vswap_dec)    dec_tog <= ~dec_tog;
      if (frame_start)  fs_tog  <= ~fs_tog;
    end
  end

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      dec_s1 <= 1'b0; dec_s2 <= 1'b0; dec_s2q <= 1'b0; dec_sync <= 1'b0;
      fs_s1  <= 1'b0; fs_s2  <= 1'b0; fs_s2q  <= 1'b0; frame_start_sync <= 1'b0;
    end else begin
      dec_s1 <= dec_tog;
      dec_s2 <= dec_s1;
      dec_s2q<= dec_s2;
      dec_sync <= (dec_s2 != dec_s2q);
      fs_s1  <= fs_tog;
      fs_s2  <= fs_s1;
      fs_s2q <= fs_s2;
      frame_start_sync <= (fs_s2 != fs_s2q);
    end
  end

  // ------------------------------------------------ vid -> gpu data (2FF) --
  logic [0:0]  slot_s1, slot_s2;
  zhao_mode_e  mnext_s1, mnext_s2, mode_s1, mode_s2;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      slot_s1 <= 1'b0; slot_s2 <= 1'b0;
      mnext_s1 <= ZHAO_MODE_Z60; mnext_s2 <= ZHAO_MODE_Z60;
      mode_s1  <= ZHAO_MODE_Z60; mode_s2  <= ZHAO_MODE_Z60;
    end else begin
      slot_s1 <= display_slot;  slot_s2 <= slot_s1;
      mnext_s1<= mode_next;     mnext_s2<= mnext_s1;
      mode_s1 <= mode;          mode_s2 <= mode_s1;
    end
  end

  // ------------------------------------------------------------ instances --
  logic        fill_buf, fill_we, fill_line_done;
  logic [1:0]  fill_abort;
  logic [6:0]  fill_addr;
  logic [63:0] fill_data;
  logic [1:0]  buf_empty;

  logic [1:0]  consume_start, consume_done, buf_fresh;
  logic        rd_en;
  logic        rd_req_buf;
  logic [6:0]  rd_req_addr;
  logic [63:0] rd_word;

  logic        req_active;

  zhao_scanout_fetch u_fetch (
    .gpu_clk            (gpu_clk),
    .rst_n              (rst_n),
    .guard_req          (guard_req),
    .guard_rsp          (guard_rsp),
    .beat_valid         (beat_valid),
    .beat_data          (beat_data),
    .beat_last          (beat_last),
    .fill_buf           (fill_buf),
    .fill_addr          (fill_addr),
    .fill_data          (fill_data),
    .fill_we            (fill_we),
    .fill_line_done     (fill_line_done),
    .fill_abort         (fill_abort),
    .buf_empty          (buf_empty),
    .dec_sync           (dec_sync),
    .frame_start_sync   (frame_start_sync),
    .display_slot_sync  (slot_s2),
    .mode_next_sync     (mnext_s2),
    .mode_sync          (mode_s2),
    .req_active         (req_active)
  );

  zhao_scanout_linebuf u_linebuf (
    .gpu_clk        (gpu_clk),
    .rst_n          (rst_n),
    .fill_buf       (fill_buf),
    .fill_addr      (fill_addr),
    .fill_data      (fill_data),
    .fill_we        (fill_we),
    .fill_line_done (fill_line_done),
    .fill_abort     (fill_abort),
    .buf_empty      (buf_empty),
    .vid_clk        (vid_clk),
    .consume_start  (consume_start),
    .consume_done   (consume_done),
    .rd_en          (rd_en),
    .rd_req_buf     (rd_req_buf),
    .rd_req_addr    (rd_req_addr),
    .rd_word        (rd_word),
    .buf_fresh      (buf_fresh)
  );

  zhao_scanout_serializer u_ser (
    .vid_clk            (vid_clk),
    .rst_n              (rst_n),
    .x                  (x),
    .y                  (y),
    .hsync              (hsync),
    .vsync              (vsync),
    .hblank             (hblank),
    .vblank             (vblank),
    .frame_start        (frame_start),
    .mode               (mode),
    .mode_next          (mode_next),
    .buf_fresh          (buf_fresh),
    .consume_start      (consume_start),
    .consume_done       (consume_done),
    .rd_en              (rd_en),
    .rd_req_buf         (rd_req_buf),
    .rd_req_addr        (rd_req_addr),
    .rd_word            (rd_word),
    .px                 (px),
    .starvation_cycles  (starvation_cycles)
  );

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_top;
  assign unused_top = req_active;   // trace pin, observed by the harness
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : zhao_video_scanout
