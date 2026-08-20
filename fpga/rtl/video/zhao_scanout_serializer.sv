// zhao_scanout_serializer.sv — VIDEO.SCANOUT serializer (vid domain)
// (plan W2.2; law: spec/video_rules.md §3-§4, decision D7 item 3).
//
// One RGB565 pixel per vid cycle during H active, free-running with the
// raster (never stalls — the raster owns time, D1). Per display line:
//
//   * real line (Z60/Storm y<240; Duo y in [24,216)): the freshness of the
//     target ping-pong buffer is taken at the edge ENDING the PREVIOUS
//     raster line (x == h_total-1, including the vblank wrap edge — see
//     "line boundary law" below). Fresh -> the buffer's content displays.
//     Not fresh -> the line is STARVED: the serializer re-emits the LAST
//     VALID pixel (a held register — the last fetched pixel actually
//     displayed; black before the first fresh pixel) for every starved
//     cycle, and every starved ACTIVE vid cycle increments
//     scanout_starvation_cycles (ZHAO_CNT_SCANOUT_STARVE). The starved line
//     NEVER reads a buffer the fetch side may be refilling -> structurally
//     never torn. Border lines (Duo y<24 / y>=216) emit 16'h0000 locally
//     (spec §3.1) — no fetch, no freshness, no starvation.
//   * at the last cycle of a consumed line the EMPTY credit returns to the
//     fetch side (consume_done); the ping-pong select advances on every
//     REAL line (fresh or starved), in lockstep with the fetch side's fill
//     alternation, and re-anchors to buffer 0 at the frame wrap edge (the
//     fetch side re-anchors at its dec re-arm 18 lines earlier — spec §4).
//
// LINE BOUNDARY LAW: the freshness decision for line N happens at the edge
// ending line N-1, so line_fresh is already correct during x==0 of line N.
// A decision taken at x==0 instead would let the first pixel display with
// the PREVIOUS line's freshness flag — a torn-read hazard (not just a
// counter off-by-one); the zref::VideoSys mirror reproduces this exactly.
//
// The output is the zhao_px_stream_t the SCALER (pass-through) and the
// displayed-stream CRC consume.
//
// Conservative SystemVerilog subset only (charter §2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

module zhao_scanout_serializer
  import zhao_pkg::*;
(
  input  logic         vid_clk,
  input  logic         rst_n,

  // raster from VIDEO.MODE (vid domain)
  input  logic [15:0]  x,
  input  logic [15:0]  y,
  input  logic         hsync,
  input  logic         vsync,
  input  logic         hblank,
  input  logic         vblank,
  input  logic         frame_start,
  input  zhao_mode_e   mode,
  input  zhao_mode_e   mode_next,   // pending mode (wrap-edge decode only)

  // line-buffer read port (vid side of zhao_scanout_linebuf)
  input  logic [1:0]   buf_fresh,
  output logic [1:0]   consume_start,
  output logic [1:0]   consume_done,
  output logic         rd_en,
  output logic         rd_req_buf,
  output logic [6:0]   rd_req_addr,
  input  logic [63:0]  rd_word,

  // pixel stream out (to VIDEO.SCALER)
  output zhao_px_stream_t px,

  // starvation counter (ZHAO_CNT_SCANOUT_STARVE; saturating, never wraps)
  output logic [63:0]  starvation_cycles
);

  logic        display_buf;   // ping-pong select for the line being displayed
  logic        line_fresh;    // the current real line consumed fresh data
  logic [15:0] last_px;       // last valid (fetched) pixel — starve hold
  logic [63:0] starve_q;

  // line class of the CURRENT raster line
  logic line_active;   // raster inside the active frame area (y < v_active)
  logic line_real;     // this active line needs fetched data (no border)
  assign line_active = (y < ZHAO_TIMING[mode].v_active);
  assign line_real   = (mode != ZHAO_MODE_DUO)
                     ? line_active
                     : ((y >= ZHAO_DUO_VIEW_Y) &&
                        (y < (ZHAO_DUO_VIEW_Y + ZHAO_DUO_VIEW_H)));

  // ---- line boundary decode ------------------------------------------------
  logic line_last;      // the LAST cycle of the current raster line
  logic last_of_frame;  // ... and the frame wraps at this edge
  assign line_last     = (x == (ZHAO_TIMING[mode].h_total - 16'd1));
  assign last_of_frame = line_last &&
                         (y == (ZHAO_TIMING[mode].v_total - 16'd1));

  // next-line class (effective after the edge ending THIS line). At the
  // frame wrap the next frame runs under the PENDING mode (the latch fires
  // at this very edge, spec §1.1): the border/real decision must use
  // mode_next there, or a Z60->Duo switch makes border line 0 look "real"
  // and wrongly takes a buffer's freshness (and vice versa).
  logic [15:0] y_next;
  logic next_active, next_real;
  logic        next_buf;     // buffer the next real line displays from
  logic        next_fresh;
  zhao_mode_e  mode_of_next;
  assign y_next       = last_of_frame ? 16'd0 : (y + 16'd1);
  assign mode_of_next = last_of_frame ? mode_next : mode;
  assign next_active  = (y_next < ZHAO_TIMING[mode_of_next].v_active);
  assign next_real    = (mode_of_next != ZHAO_MODE_DUO)
                     ? next_active
                     : ((y_next >= ZHAO_DUO_VIEW_Y) &&
                        (y_next < (ZHAO_DUO_VIEW_Y + ZHAO_DUO_VIEW_H)));
  assign next_buf    = last_of_frame ? 1'b0   // frame re-anchor (spec §4)
                    : (line_real ? ~display_buf : display_buf);
  assign next_fresh  = buf_fresh[next_buf];

  // ---- read request, ONE CYCLE AHEAD -------------------------------------
  //
  // The line buffer's RAM read is registered, so the word for pixel n has to be
  // requested during pixel n-1. Nothing is added to the video pipeline: the
  // latency is absorbed entirely by asking early.
  //
  // The line-boundary case is the one that matters and the one a naive output
  // register gets wrong. On the LAST cycle of a line the next cycle is x=0 of
  // the NEXT line, so the request must already use that line's buffer and word
  // zero. This module already decides `next_buf` and `next_fresh` at exactly
  // that edge for the ownership handover, so the lookahead needs no new state.
  //
  // `rd_en` is false for a starved or border line, which means NO read is
  // issued at all. That is deliberate: with a genuine dual-clock RAM, reading a
  // buffer the GPU may be refilling is the same-address read-during-write
  // ambiguity Quartus warned about in AUDIO.FIFO. A starved line displays
  // `last_px` and touches the memory not at all.
  // 16 bits because x is; only [8:2] selects the word. The low two bits are
  // the lane within the word and belong to the CURRENT pixel, not the
  // lookahead, and the high bits cannot be reached inside a 512-pixel line.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [15:0] x_ahead;
  /* verilator lint_on UNUSEDSIGNAL */
  assign x_ahead = x + 16'd1;

  always_comb begin
    if (line_last) begin
      rd_en       = next_real && next_fresh;
      rd_req_buf  = next_buf;
      rd_req_addr = 7'd0;
    end else begin
      rd_en       = line_real && line_fresh;
      rd_req_buf  = display_buf;
      rd_req_addr = x_ahead[8:2];
    end
  end

  // pixel mux: border black vs buffer word (pixel = 16-bit lane of the word)
  // UNCHANGED. The lane selector still uses the CURRENT x; only the address
  // generation moved a cycle earlier.
  logic [15:0] px_buf;
  always_comb begin
    unique case (x[1:0])                      // lane select
      2'b00:   px_buf = rd_word[15:0];
      2'b01:   px_buf = rd_word[31:16];
      2'b10:   px_buf = rd_word[47:32];
      default: px_buf = rd_word[63:48];
    endcase
  end

  logic px_valid;
  assign px_valid = line_active && !hblank;    // an ACTIVE pixel cycle

  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      display_buf <= 1'b0;
      line_fresh  <= 1'b0;
      last_px     <= 16'h0000;   // canonical black until the first fresh px
      starve_q    <= 64'd0;
      consume_start <= 2'b00;
      consume_done  <= 2'b00;
    end else begin
      consume_start <= 2'b00;
      consume_done  <= 2'b00;

      // starvation accounting: an active real pixel cycle whose line did
      // not consume fresh data (border lines never count)
      if (px_valid && line_real && !line_fresh) begin
        starve_q <= (starve_q == 64'hFFFF_FFFF_FFFF_FFFF)
                  ? starve_q : starve_q + 64'd1;
      end

      // the hold value tracks every fetched pixel actually displayed
      if (px_valid && line_real && line_fresh) begin
        last_px <= px_buf;
      end

      if (line_last) begin
        // close the CURRENT real line: credit its buffer back to the fetch
        if (line_real) begin
          display_buf <= next_buf;
          if (line_fresh) begin
            consume_done[display_buf] <= 1'b1;
          end
        end
        if (last_of_frame) begin
          display_buf <= 1'b0;   // re-anchor at the wrap, even from vblank
        end
        // open the NEXT real line: take its freshness now (the boundary law)
        if (next_real) begin
          line_fresh <= next_fresh;
          if (next_fresh) begin
            consume_start[next_buf] <= 1'b1;
          end
        end else begin
          line_fresh <= 1'b0;   // border/vblank next: no fetched data
        end
      end
    end
  end

  assign starvation_cycles = starve_q;

  // ------------------------------------------------------ stream output ---
  // Starved cycles re-emit the LAST VALID pixel (never a buffer being
  // refilled -> never torn); border cycles emit the border colour locally.
  assign px.valid  = px_valid;
  assign px.rgb565 = !line_active          ? 16'h0000
                   : !line_real            ? 16'(ZHAO_DUO_BORDER_RGB565)
                   : line_fresh            ? px_buf
                   : last_px;
  assign px.x      = 10'(x);
  assign px.y      = 8'(y);
  assign px.hsync  = hsync;
  assign px.vsync  = vsync;
  assign px.hblank = hblank;
  assign px.vblank = vblank;

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_serializer;
  assign unused_serializer = frame_start ^ vsync ^ vblank;
  /* verilator lint_on UNUSEDSIGNAL */

endmodule : zhao_scanout_serializer
