// zhao_post_echo.sv -- POST.ECHO: the composited frame, post-ink and pre-HUD,
// echoed into a capture buffer once per post pass.
//
// Contract: design/contracts/POST.ECHO.md (written 2026-09-19 with this file).
// Law:      zref::post::echo (reference/include/zref/zref_post.hpp).
// ENFORCED-BY: tests/compositor/post_echo_directed.cpp
//
// ---------------------------------------------------------------------------
// WHY IT EXISTS NOW
// ---------------------------------------------------------------------------
// Owner ruling R7, 2026-09-19: POST.ECHO is mandatory and is BUILT. The block
// that used to be "first on the cut list" with every behavioural section
// "deliberately unwritten" now has a contract, a reference and this. What the
// old contract DID say survives unchanged, because it was right: the echo sits
// "after step 8 and before HUD", it is "observational", and it must "drop the
// echo rather than stall or fault the frame".
//
// ---------------------------------------------------------------------------
// WHAT IT IS MADE OF -- one existing engine and one queue
// ---------------------------------------------------------------------------
//   tap (accepted composited beat) --> chunk admission --> skid (M10K)
//        --> zhao_raster_fbwrite (capture base, stride w*2) --> MEM.GUARD
//
// The writer is RASTER.FBWRITE, instantiated again rather than rewritten: a
// raster-ordered, contiguous, 16-pixel-row stream into bursts with retirement
// accounting is EXACTLY what that block is, and its contiguity check is the
// thing that proves an admitted chunk landed where the address law says.
//
// ---------------------------------------------------------------------------
// THE THREE LAWS THIS FILE OWNS (and zref::post::echo states)
// ---------------------------------------------------------------------------
// 1. WHERE: addr(view, x, y) = BASE + ((view*h + y)*w + x)*2. Views are
//    STACKED, so view 1 is `fbwrite.px_y = y + h` and the stride stays w*2 --
//    no multiplier here at all.
// 2. NEVER STALL, DROP WHOLE CHUNKS. There is no ready on the tap. At a chunk's
//    first pixel (x mod 16 == 0) the chunk is admitted only if the skid has
//    room for all sixteen; otherwise all sixteen are dropped and the pass is
//    TORN. Dropping single pixels would break FBWRITE's contiguity check and
//    latch a fatal error; dropping chunks leaves 32-byte holes and nothing else.
// 3. A CAPTURE IS WHOLE OR IT IS NOT A CAPTURE. `pass_complete_o` pulses, and
//    `passes_complete_o` counts, only when every pixel of the pass was admitted
//    AND every issued word has RETIRED at the arbiter (FBWRITE's `drained_o`).
//    Anything less is counted in `passes_torn_o` and never reported complete.
//
// The tap MUST be qualified by the compositor's output handshake (the core
// does it): POST.COMPOSITE holds `echo_valid_o` through a downstream stall, so
// an unqualified tap would repeat a stalled pixel once per stall cycle.
//
// ---------------------------------------------------------------------------
// COST (estimated, UNMEASURED -- no fit has been run)
// ---------------------------------------------------------------------------
// One more zhao_raster_fbwrite (~300 ALM, its 12x16 row multiply may take a
// DSP), a 256 x (16 + XW + YW + 1) = 256 x 34-bit skid with registered read
// written to infer ONE M10K (the 256x40 shape) rather than ~8,700 flops, and
// the pass counters. ~400 ALM, 0-1 DSP, 1 M10K.
//
// WHY 256 AND NOT 32: the depth is free up to one M10K, and it buys slack. A
// RASTER.FBWRITE sustains 16 pixels per ~22 clocks (16 collect + request +
// verdict + four data beats) on a fast memory -- the SAME engine that paces the
// compositor's own write-back, so in the composition the tap arrives at that
// rate and the echo keeps up. What the skid absorbs is the difference in when
// the two writers win ENGINE0's round-robin: sixteen chunks of it. A tap that
// outruns a FBWRITE for longer than that is starved by definition and drops
// whole chunks, which is the contract's behaviour, not a fault.
`default_nettype none

module zhao_post_echo
  import zhao_pkg::*;
#(
    parameter int unsigned XW   = 9,     // view-local x (<= 511)
    parameter int unsigned YW   = 8,     // view-local y (<= 255)
    parameter int unsigned SKID = 256    // power of two, >= 32 (two chunks)
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the pass: the compositor's own frame_start and geometry ------------
    input  var logic          pass_start_i,
    input  var logic          view_i,
    input  var logic [XW-1:0] w_i,
    input  var logic [YW-1:0] h_i,

    // ---- the tap, one beat per ACCEPTED composited pixel --------------------
    input  var logic          tap_valid_i,
    input  var logic [15:0]   tap_rgb_i,
    input  var logic [XW-1:0] tap_x_i,
    input  var logic [YW-1:0] tap_y_i,

    // ---- MEM.GUARD write master (client ENGINE0), exactly as FBWRITE drives it
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    output var logic [63:0]     guard_wdata_o,
    output var logic            guard_wvalid_o,
    input  var logic            guard_wready_i,
    output var logic            guard_wlast_o,
    input  var logic [7:0]      retire_words_i,   // THIS requester's retired words

    // ---- evidence -----------------------------------------------------------
    output var logic          busy_o,              // a pass is open or draining
    output var logic          pass_complete_o,     // one pulse: a WHOLE capture landed
    output var logic [31:0]   passes_complete_o,
    output var logic [31:0]   passes_torn_o,
    output var logic [31:0]   pixels_written_o,    // pixels handed to the writer
    output var logic [31:0]   pixels_dropped_o,
    output var logic          fault_o              // sticky: a refused burst or a
                                                   // broken stream (FBWRITE's latch)
);

  localparam int unsigned QW = $clog2(SKID);
  localparam int unsigned CW = YW + 1;             // capture row: v*h + y
  localparam int unsigned EW = 16 + XW + CW;       // one skid entry

  initial begin
    if ((SKID < 32) || ((SKID & (SKID - 1)) != 0))
      $fatal(1, "zhao_post_echo: SKID must be a power of two >= 32 (got %0d)", SKID);
    if ((XW > 11) || (CW > 11))
      $fatal(1, "zhao_post_echo: coordinates wider than FBWRITE's signed 12 bits");
  end

  // ==========================================================================
  // PASS STATE
  // ==========================================================================
  logic          open_q;        // tap pixels of this pass are still arriving
  logic          drain_q;       // the last pixel arrived; waiting for retirement
  logic          torn_q;        // this pass dropped a chunk or was refused
  logic          view_q;
  logic [XW-1:0] w_q;
  logic [YW-1:0] h_q;
  logic          keep_q;        // the current chunk was admitted

  // ==========================================================================
  // THE SKID -- simple dual port, registered read, one M10K
  // ==========================================================================
  logic [EW-1:0] sk_mem [0:SKID-1];
  logic [QW-1:0] sk_wp_q, sk_rp_q;
  logic [QW:0]   sk_count_q;
  logic [EW-1:0] head_q;
  logic          head_v_q;

  logic          fbw_ready;
  logic          push_c, rd_en_c, head_take_c;
  logic [EW-1:0] push_data_c;

  assign head_take_c = head_v_q && fbw_ready;
  assign rd_en_c     = (sk_count_q != '0) && (!head_v_q || head_take_c);

  always_ff @(posedge clk) begin
    if (push_c)  sk_mem[sk_wp_q] <= push_data_c;
    if (rd_en_c) head_q          <= sk_mem[sk_rp_q];
  end

  // ==========================================================================
  // CHUNK ADMISSION -- decided at the chunk's first pixel, held for sixteen
  // ==========================================================================
  logic          chunk_open_c, room_c, admit_c;
  logic [CW-1:0] crow_c;
  assign chunk_open_c = (tap_x_i[3:0] == 4'd0);
  // Room for all sixteen pixels NOW, assuming the writer takes none of them
  // while they arrive: the conservative reading, and the one that makes a
  // partial chunk impossible rather than unlikely.
  assign room_c  = ((QW+1)'(SKID) - sk_count_q) >= (QW+1)'(16);
  assign admit_c = chunk_open_c ? room_c : keep_q;
  assign crow_c  = view_q ? (CW'(tap_y_i) + CW'(h_q)) : CW'(tap_y_i);

  assign push_c      = tap_valid_i && open_q && admit_c;
  assign push_data_c = {tap_rgb_i, tap_x_i, crow_c};

  logic last_px_c;
  assign last_px_c = (tap_x_i == (w_q - XW'(1))) && (tap_y_i == (h_q - YW'(1)));

  // ==========================================================================
  // THE WRITER -- RASTER.FBWRITE, pointed at the capture window
  // ==========================================================================
  logic [15:0]        px_rgb;
  logic [XW-1:0]      px_x;
  logic [CW-1:0]      px_row;
  assign {px_rgb, px_x, px_row} = head_q;

  logic signed [11:0] fbw_x, fbw_y;
  assign fbw_x = 12'($signed({1'b0, px_x}));
  assign fbw_y = 12'($signed({1'b0, px_row}));

  // A row ends every sixteen pixels by construction (w is a multiple of 16), so
  // FBWRITE flushes on a full row buffer. `px_last_i` is still driven with the
  // true row end, which is the same event, so a short row could never linger.
  logic fbw_last;
  assign fbw_last = (px_x == (w_q - XW'(1)));

  logic [31:0] fbw_pixels_unused, fbw_bursts_unused, fbw_stall_unused;
  logic [31:0] fbw_issued_unused, fbw_retired_unused;
  logic        fbw_stream_err, fbw_fatal, fbw_drained, fbw_busy;

  zhao_raster_fbwrite u_capture_writer (
    .clk(clk), .rst_n(rst_n),
    .fb_base_i  (ZHAO_VRAM_ADDR_BITS'(ZHAO_POST_ECHO_BASE)),
    .fb_stride_i(16'({w_q, 1'b0})),
    .px_valid_i (head_v_q), .px_ready_o(fbw_ready),
    .px_rgb565_i(px_rgb), .px_x_i(fbw_x), .px_y_i(fbw_y), .px_last_i(fbw_last),
    // The writer's per-frame counter window is this block's pass.
    .frame_end_i(pass_start_i),
    .retire_words_i(retire_words_i),
    .guard_req_o(guard_req_o), .guard_rsp_i(guard_rsp_i),
    .guard_wdata_o(guard_wdata_o), .guard_wvalid_o(guard_wvalid_o),
    .guard_wready_i(guard_wready_i), .guard_wlast_o(guard_wlast_o),
    .pixels_written_o(fbw_pixels_unused),
    .bursts_issued_o (fbw_bursts_unused),
    .stall_clocks_o  (fbw_stall_unused),
    .stream_error_o  (fbw_stream_err),
    .issued_words_o  (fbw_issued_unused),
    .retired_words_o (fbw_retired_unused),
    .drained_o       (fbw_drained),
    .fatal_error_o   (fbw_fatal),
    .busy_o          (fbw_busy)
  );

  // The window counters are the writer's own business; this block counts its
  // own events. Sunk by a statement rather than a waiver.
  logic unused_c;
  assign unused_c = ^{fbw_pixels_unused, fbw_bursts_unused, fbw_stall_unused,
                      fbw_issued_unused, fbw_retired_unused, fbw_busy};

  assign fault_o = fbw_fatal || fbw_stream_err;
  assign busy_o  = open_q || drain_q;

  // ==========================================================================
  // THE ONE SEQUENTIAL BLOCK
  // ==========================================================================
  logic bad_geom_c;
  assign bad_geom_c = (w_i[3:0] != 4'd0) || (w_i == '0) || (h_i == '0);

  logic settle_c;
  assign settle_c = drain_q && (sk_count_q == '0) && !head_v_q && fbw_drained;

  // Torn events: a pass settling torn, a pass abandoned by a new start, and a
  // new pass refused for its geometry. At most two coincide.
  logic [1:0] torn_inc_c;
  assign torn_inc_c = 2'((settle_c && (torn_q || fault_o)) ? 1 : 0)
                    + 2'((pass_start_i && (open_q || (drain_q && !settle_c))) ? 1 : 0)
                    + 2'((pass_start_i && bad_geom_c) ? 1 : 0);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      open_q            <= 1'b0;
      drain_q           <= 1'b0;
      torn_q            <= 1'b0;
      view_q            <= 1'b0;
      w_q               <= '0;
      h_q               <= '0;
      keep_q            <= 1'b0;
      sk_wp_q           <= '0;
      sk_rp_q           <= '0;
      sk_count_q        <= '0;
      head_v_q          <= 1'b0;
      pass_complete_o   <= 1'b0;
      passes_complete_o <= '0;
      passes_torn_o     <= '0;
      pixels_written_o  <= '0;
      pixels_dropped_o  <= '0;
    end else begin
      pass_complete_o <= 1'b0;

      // ---- the skid, one occupancy assignment ------------------------------
      sk_count_q <= sk_count_q + (push_c  ? (QW+1)'(1) : '0)
                               - (rd_en_c ? (QW+1)'(1) : '0);
      if (push_c)  sk_wp_q <= sk_wp_q + QW'(1);
      if (rd_en_c) sk_rp_q <= sk_rp_q + QW'(1);
      if (rd_en_c)          head_v_q <= 1'b1;
      else if (head_take_c) head_v_q <= 1'b0;
      if (head_take_c) pixels_written_o <= pixels_written_o + 32'd1;

      // ---- tap bookkeeping --------------------------------------------------
      if (tap_valid_i) begin
        if (!open_q || !admit_c) pixels_dropped_o <= pixels_dropped_o + 32'd1;
        if (open_q) begin
          if (chunk_open_c) keep_q <= room_c;
          if (!admit_c)     torn_q <= 1'b1;
          if (last_px_c) begin
            open_q  <= 1'b0;
            drain_q <= 1'b1;
          end
        end
      end

      // ---- a pass settles: whole, or torn -----------------------------------
      if (settle_c) begin
        drain_q <= 1'b0;
        if (!(torn_q || fault_o)) begin
          passes_complete_o <= passes_complete_o + 32'd1;
          pass_complete_o   <= 1'b1;
        end
      end
      // Every torn event this cycle, summed ONCE: two nonblocking increments of
      // one counter keep only the last, which is how a counter under-reports.
      if (torn_inc_c != 2'd0) passes_torn_o <= passes_torn_o + 32'(torn_inc_c);

      // ---- a new pass. One that arrives over an unsettled pass tears it: the
      //      previous capture cannot be called whole if its tail never came. --
      if (pass_start_i) begin
        view_q  <= view_i;
        w_q     <= w_i;
        h_q     <= h_i;
        keep_q  <= 1'b0;
        drain_q <= 1'b0;
        // A refused geometry opens no pass: every pixel of it is a drop, and
        // the refusal is a torn pass so it cannot read as "nothing happened".
        open_q  <= !bad_geom_c;
        torn_q  <= bad_geom_c;
      end
    end
  end

endmodule : zhao_post_echo

`default_nettype wire
