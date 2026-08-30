// zhao_raster_fbwrite.sv — RASTER.FBWRITE: the rasterizer's pixel stream
// becomes VRAM writes.
//
// ENFORCED-BY: tests/render/render_fb_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS: THE CONSOLE HAS NEVER DRAWN A PIXEL
// ---------------------------------------------------------------------------
// `zhao_shell_top` integrates CMD (scheduler + DMA), MEM (guards + arbiter +
// SDRAM controller), VIDEO (mode + scanout + scaler + framectl), INPUT, AUDIO
// and DEBUG. It does not integrate the render path at all. The rasterizer is
// verified — `zhao_geom_bin_pipe` drives GEOM.BINNER straight into
// `zhao_raster_tile_pipe` and both halves are green — but its output is a pixel
// STREAM that nothing in the shell consumes, so no triangle this machine
// rasterizes has ever reached memory or a screen.
//
// This is the missing link, and it is deliberately the SMALLEST one that closes
// the loop: it does no rasterizing, no filtering, no blending and no address
// arithmetic beyond a framebuffer origin. It turns
//
//     RASTER.RESOLVE's {rgb565, x, y, last}
//
// into bursts on the memory guard, which is the same port `zhao_debug_frameblit`
// already writes pixels through. The shell then needs one client tie-off
// replaced, not a new subsystem.
//
// ---------------------------------------------------------------------------
// THE PORT IT WRITES THROUGH WAS RESERVED FOR IT
// ---------------------------------------------------------------------------
// `zhao_pkg`'s client enum has `ZHAO_CLIENT_ENGINE0 = 3'd2`, commented
// "reserved guaranteed slot", and `zhao_shell_top` ties `client_req[2]`,
// `[3]` and `[4]` to zero. So the arbiter has always had a guaranteed port
// waiting for the renderer; this block is what finally drives it. Nothing about
// the arbiter, the guard or the SDRAM controller changes.
//
// ---------------------------------------------------------------------------
// THE ONE LAW THIS FILE OWNS: A TILE ROW IS EXACTLY ONE BURST
// ---------------------------------------------------------------------------
// RASTER.RESOLVE streams a tile in raster order — `fb_addr_o` is `{row, col}`
// and the surface coordinate is the tile origin plus that — so sixteen
// consecutive beats share a `y` and have `x`, `x+1` ... `x+15`. At two bytes a
// pixel that is **thirty-two contiguous bytes**, and the guard's burst limit is
// sixty-four, so one tile row is one burst and a 16x16 tile is sixteen of them.
//
//     16 pixels x 2 B = 32 B = 4 beats of the guard's 64-bit write data
//
// That is why this block buffers a ROW and not a tile: a row is the largest
// unit that is guaranteed contiguous in memory, and buffering the whole tile
// would need 512 bytes of storage to produce exactly the same sixteen bursts.
//
// CONTIGUITY IS CHECKED, NOT ASSUMED. `stream_error_o` rises if a pixel arrives
// that is not the immediate successor of the one before it within a row. The
// alternative — trusting the producer and computing the address from the FIRST
// pixel of the row — would write sixteen right-looking pixels to a wrong place
// on any upstream reordering, and a framebuffer full of plausible garbage is
// the hardest possible thing to debug. A wrong stream stops instead.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT DO
// ---------------------------------------------------------------------------
// No clipping against the framebuffer bounds — the guard owns the region check
// and will refuse and report a write outside the granted span, which is the one
// place that decision belongs. No double buffering or page flipping: it writes
// where `fb_base_i` says. No format conversion: RESOLVE has already produced
// RGB565, which is the scanout's format. No tile scheduling and no notion of a
// frame beyond `frame_end_i` for the counters.
`default_nettype none

module zhao_raster_fbwrite
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- the framebuffer this frame is being written into -----------------
    // Byte address of pixel (0,0) and the byte distance between rows. Held
    // stable while a frame is in flight; sampled per burst, so a change
    // between tiles is honoured and a change mid-row is the caller's fault.
    input var logic [ZHAO_VRAM_ADDR_BITS-1:0] fb_base_i,
    input var logic [15:0]                    fb_stride_i,

    // ---- RASTER.RESOLVE's pixel stream ------------------------------------
    input  var logic        px_valid_i,
    output var logic        px_ready_o,
    input  var logic [15:0] px_rgb565_i,
    input  var logic signed [11:0] px_x_i,
    input  var logic signed [11:0] px_y_i,
    input  var logic        px_last_i,   // last pixel of a tile

    input  var logic        frame_end_i,

    // ---- retirement, which is the only thing that means "the write landed" --
    // The arbiter reissues credits as bursts RETIRE. Accepting a beat means the
    // write queue took it; it says nothing about whether SDRAM has it. A
    // framebuffer slot must never be published on an acceptance count, and this
    // block used to expose nothing else -- `frame_end_i` merely reset counters.
    //
    // This is DEBUG.FRAMEBLIT's already-ratified transaction law, applied to
    // the renderer: accepted is not retired; failure stops new side effects;
    // issued traffic drains; a dirty inactive slot is acceptable; publication
    // requires every intended byte to have retired.
    //
    // ENFORCED-BY: tests/render/render_fb_directed.cpp:main
    input  var logic [ 7:0] retire_words_i,

    // ---- MEM.GUARD, exactly as zhao_debug_frameblit drives it -------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    output var logic [63:0]     guard_wdata_o,
    output var logic            guard_wvalid_o,
    input  var logic            guard_wready_i,
    output var logic            guard_wlast_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] pixels_written_o,
    output var logic [31:0] bursts_issued_o,
    output var logic [31:0] stall_clocks_o,   // pixel offered, not accepted
    output var logic        stream_error_o,   // sticky: a non-contiguous pixel

    // ---- the frame transaction ---------------------------------------------
    output var logic [31:0] issued_words_o,   // 16-bit words handed to the guard
    output var logic [31:0] retired_words_o,  // ...that the arbiter has retired
    // Every issued word retired and nothing in flight. THIS is the signal a
    // frame controller may publish a slot on; `busy_o` is not.
    output var logic        drained_o,
    // Sticky. A guard denial or a broken pixel stream means the frame's picture
    // is wrong, so the slot must be released dirty rather than published. The
    // old behaviour -- drop the refused row and carry on -- is right for a
    // standalone seam test and wrong for a frame.
    output var logic        fatal_error_o,
    output var logic        busy_o
);

  // ---- the row buffer ------------------------------------------------------
  localparam int unsigned ROW_PX    = 16;             // one tile row
  localparam int unsigned ROW_BYTES = ROW_PX * 2;     // 32
  localparam int unsigned BEATS     = ROW_BYTES / 8;  // 4 beats of 64 bits

  logic [15:0] row_r [ROW_PX];
  logic [4:0]  fill_r;                     // 0..16 pixels buffered
  logic signed [11:0] row_x0_r, row_y_r;   // where this row starts
  logic        row_open_r;                 // a row is being collected

  // ---- the burst -----------------------------------------------------------
  localparam logic [1:0] W_IDLE = 2'd0;
  localparam logic [1:0] W_REQ  = 2'd1;
  localparam logic [1:0] W_DATA = 2'd2;

  logic [1:0]  w_state_r;
  logic [2:0]  beat_r;
  logic [15:0] out_r [ROW_PX];
  logic [4:0]  out_n_r;                    // pixels in the burst being sent
  logic [ZHAO_VRAM_ADDR_BITS-1:0] out_addr_r;

  // A row's byte address. The multiply is the only one in the block and it is
  // a 12x16 — the framebuffer stride is not a power of two (384 px RGB565 is
  // 768 B) so a shift will not do, and precomputing per-row bases upstream
  // would put framebuffer geometry in a block that has no business knowing it.
  // The product is 32 bits and the address space is 27; the top bits are
  // dropped DELIBERATELY, because an address past 128 MB is not this block's
  // to reject -- MEM.GUARD owns the region check and reports the violation.
  // Sunk explicitly rather than by a lint waiver.
  logic [31:0] yoff_c;
  logic [ZHAO_VRAM_ADDR_BITS-1:0] row_addr_c;
  logic        addr_high_unused;
  assign yoff_c = 32'(unsigned'({20'd0, row_y_r})) * 32'({16'd0, fb_stride_i});
  assign addr_high_unused = |yoff_c[31:ZHAO_VRAM_ADDR_BITS];
  assign row_addr_c = fb_base_i + yoff_c[ZHAO_VRAM_ADDR_BITS-1:0]
                    + ZHAO_VRAM_ADDR_BITS'({19'd0, unsigned'(row_x0_r)} << 1);

  // The row is flushed when it is full, or when the tile ends on a short row.
  logic take_px_c, flush_c;
  assign take_px_c = px_valid_i && px_ready_o;
  assign flush_c   = take_px_c && ((fill_r == 5'(ROW_PX - 1)) || px_last_i);

  // A pixel is accepted only while there is somewhere to put it: the buffer has
  // room AND the burst engine is not still sending the previous row. Holding
  // `px_ready_o` low is the backpressure that reaches all the way to the
  // binner's drain, which is the whole point of the ready/valid chain.
  assign px_ready_o = (w_state_r == W_IDLE) && (fill_r != 5'(ROW_PX));

  assign busy_o = (w_state_r != W_IDLE) || row_open_r;

  // `guard_rsp_i.violation` is the guard's own pulse for its own counter; this
  // block acts on `ok` and needs no second copy of the same fact. Sunk so the
  // linter's "bits not used" is answered by a statement rather than a waiver.
  logic rsp_violation_unused;
  assign rsp_violation_unused = guard_rsp_i.violation;

  // ---- guard master --------------------------------------------------------
  // `valid` is a function of registers only, never of `guard_rsp_i.ready`.
  always_comb begin
    guard_req_o         = '0;
    guard_req_o.valid   = (w_state_r == W_REQ);
    guard_req_o.write   = 1'b1;
    guard_req_o.client  = ZHAO_CLIENT_ENGINE0;
    guard_req_o.addr    = out_addr_r;
    guard_req_o.len     = 7'({2'd0, out_n_r} << 1);   // bytes = pixels * 2
    // Byte enables, bit i = addr + i. A full row is 32 bytes; a short tail row
    // enables only the bytes it actually carries, which is what keeps a partial
    // burst from writing over a neighbour's pixels.
    guard_req_o.be      = (64'd1 << ({2'd0, out_n_r} << 1)) - 64'd1;
  end

  assign guard_wvalid_o = (w_state_r == W_DATA);
  assign guard_wlast_o  = (w_state_r == W_DATA) && (beat_r == 3'(BEATS - 1));
  always_comb begin
    guard_wdata_o = 64'd0;
    for (int unsigned k = 0; k < 4; k++) begin
      automatic logic [4:0] idx = 5'({beat_r, 2'd0}) + 5'(k);
      guard_wdata_o[16*k +: 16] = (idx < out_n_r) ? out_r[idx[3:0]] : 16'd0;
    end
  end

  // ---- sequential ----------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fill_r           <= 5'd0;
      row_open_r       <= 1'b0;
      row_x0_r         <= 12'sd0;
      row_y_r          <= 12'sd0;
      w_state_r        <= W_IDLE;
      beat_r           <= 3'd0;
      out_n_r          <= 5'd0;
      out_addr_r       <= '0;
      pixels_written_o <= 32'd0;
      bursts_issued_o  <= 32'd0;
      stall_clocks_o   <= 32'd0;
      stream_error_o   <= 1'b0;
      issued_words_o   <= 32'd0;
      retired_words_o  <= 32'd0;
      fatal_error_o    <= 1'b0;
      for (int unsigned k = 0; k < ROW_PX; k++) begin
        row_r[k] <= 16'd0;
        out_r[k] <= 16'd0;
      end
    end else begin
      if (frame_end_i) begin
        // Counters are per-frame windows for the harness; the stream error and
        // the fatal latch are NOT cleared here, because a frame that produced a
        // bad address is not made good by ending. The word ledger is not
        // cleared either -- it has to balance ACROSS the drain that follows
        // frame_end, which is exactly when publication is decided.
        pixels_written_o <= 32'd0;
        bursts_issued_o  <= 32'd0;
        stall_clocks_o   <= 32'd0;
      end

      // The arbiter's credit stream. This is the only statement in the block
      // that means a write actually landed.
      retired_words_o <= retired_words_o + 32'({24'd0, retire_words_i});

      // A broken pixel stream is fatal to the frame, not just to a row.
      if (stream_error_o) fatal_error_o <= 1'b1;

      if (px_valid_i && !px_ready_o) stall_clocks_o <= stall_clocks_o + 32'd1;

      // ---- collect ---------------------------------------------------------
      if (take_px_c) begin
        row_r[fill_r[3:0]] <= px_rgb565_i;
        if (!row_open_r || (fill_r == 5'd0)) begin
          row_x0_r   <= px_x_i;
          row_y_r    <= px_y_i;
          row_open_r <= 1'b1;
        end else begin
          // THE CONTIGUITY CHECK. Within a row every pixel must be the
          // immediate successor of the last, on the same scanline. Anything
          // else and the address computed from the row's FIRST pixel is a
          // lie about the fifteen behind it.
          if ((px_y_i != row_y_r) ||
              (px_x_i != (row_x0_r + 12'sd1 * $signed({7'd0, fill_r}))))
            stream_error_o <= 1'b1;
        end

        if (flush_c) begin
          for (int unsigned k = 0; k < ROW_PX; k++)
            out_r[k] <= (5'(k) == fill_r) ? px_rgb565_i : row_r[k];
          out_n_r    <= fill_r + 5'd1;
          out_addr_r <= row_addr_c;
          fill_r     <= 5'd0;
          row_open_r <= 1'b0;
          beat_r     <= 3'd0;
          w_state_r  <= W_REQ;
        end else begin
          fill_r <= fill_r + 5'd1;
        end
      end

      // ---- the burst -------------------------------------------------------
      case (w_state_r)
        W_REQ: begin
          if (guard_rsp_i.ready) begin
            if (guard_rsp_i.ok) begin
              w_state_r <= W_DATA;
              beat_r    <= 3'd0;
              bursts_issued_o <= bursts_issued_o + 32'd1;
              // Words, not bytes: the arbiter's credits are 16-bit words and
              // the two ledgers have to be in the same unit to balance.
              issued_words_o  <= issued_words_o + 32'({27'd0, out_n_r});
            end else begin
              // The guard REFUSED: the write is outside the leased region and
              // nothing was written. Dropping the row is still correct -- the
              // guard has counted and latched the violation, and a retry would
              // write it somewhere it does not belong -- but the FRAME is now
              // unpublishable, and that is what the latch below says.
              w_state_r     <= W_IDLE;
              fatal_error_o <= 1'b1;
            end
          end
        end

        W_DATA: begin
          if (guard_wready_i) begin
            if (beat_r == 3'(BEATS - 1)) begin
              w_state_r        <= W_IDLE;
              pixels_written_o <= pixels_written_o + 32'({27'd0, out_n_r});
            end else begin
              beat_r <= beat_r + 3'd1;
            end
          end
        end

        // W_IDLE IS AN EXPLICIT NO-OP, AND THE DEFAULT IS NOT.
        //
        // The collect block above sets `w_state_r <= W_REQ` when a row fills.
        // This case runs LATER in the same always_ff and sees the OLD state,
        // which is W_IDLE -- so a `default: w_state_r <= W_IDLE;` that catches
        // W_IDLE silently overwrites the flush transition on every single row.
        // The block then consumed 7,936 pixels, issued zero bursts, and held
        // `px_ready_o` high throughout, so nothing upstream stalled and nothing
        // looked wrong from either side of the seam.
        //
        // The default now catches only states that cannot occur, which is what
        // a default is for.
        W_IDLE: ;
        default: w_state_r <= W_IDLE;
      endcase
    end
  end

  // DRAINED: every word this block handed to the guard has been retired by the
  // arbiter, and nothing is in flight. A frame controller publishes on THIS.
  //
  // `busy_o` is not the same claim and must not be used for it: busy falls when
  // the last beat is ACCEPTED, which is several stages before the byte is in
  // SDRAM.
  assign drained_o = !busy_o && (w_state_r == W_IDLE)
                   && (issued_words_o == retired_words_o);

endmodule : zhao_raster_fbwrite

`default_nettype wire
