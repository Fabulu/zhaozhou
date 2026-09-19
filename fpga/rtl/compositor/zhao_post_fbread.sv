// zhao_post_fbread.sv -- POST.COMPOSITE's SOURCE: the completed back buffer,
// read back in RASTER order through the render engine's own lease.
//
// ENFORCED-BY: tests/compositor/post_fbread_directed.cpp
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS (core header entry I15, closed 2026-09-19)
// ---------------------------------------------------------------------------
// `zhao_post_composite`'s source port `s_*` is ADDRESSLESS: the Nth pixel it
// accepts IS frame pixel (N mod W, N div W), and its nine-line ring is built on
// that. RASTER.RESOLVE is TILE-ordered (16x16, and only the tiles a triangle
// touched), so the reorder buffer between the two is a whole frame -- and the
// frame store that already exists is the framebuffer. POST.COMPOSITE.md names
// the arrangement: "an exclusive framebuffer read/write lease after resolve and
// before publication". This block is the READ half of that lease: it walks the
// leased slot row by row, 64 bytes at a time, and hands the compositor one
// RGB565 pixel per accepted beat, in exactly the order the compositor counts.
//
// It is the shape `zhao_scanout_fetch` already has (the core's I15 note named
// it), re-armed on RENDER DRAIN rather than on the video swap and pointed at the
// BACK buffer rather than the displayed one. It is a separate block rather than
// a mode of that one because every one of the three differences is in the
// fetcher's control half, and the fetcher's control half is the display pipe.
//
// ---------------------------------------------------------------------------
// THE LAWS THIS FILE OWNS
// ---------------------------------------------------------------------------
// * BYTE ORDER: `spec/video_rules.md` 3 -- RGB565 little-endian halfwords,
//   row-major, no row padding beyond `stride_i`. The shell's read packer
//   assembles a beat as {w3, w2, w1, w0} with w0 the LOWEST address, so pixel k
//   of a beat is `beat[16*k +: 16]`. No colour arithmetic: a pixel leaves this
//   block with the bits it was stored with.
// * ONE LOGICAL REQUEST AT A TIME. A request is at most 64 bytes (32 px) and
//   never crosses a row; the last request of a row is shorter when the width is
//   not a multiple of 32. Widths must be a multiple of 4 px (one beat) -- every
//   mode is (384, 320, 256) -- and a width that is not is REFUSED at start with
//   `fault_o`, never silently truncated.
// * CREDIT BEFORE REQUEST. A request is offered only when the beat queue has
//   room for every beat it will return, counting beats already owed. A read
//   that could overflow the queue is not issued, so there is no overflow path
//   to get wrong -- and `overflow_o` is the tripwire that says so.
// * THE GUARD'S TWO-CYCLE LAW: `ready` is a level, the verdict is a pulse one
//   cycle (or, behind a share, more) later. R_VERD WAITS for `ok` or
//   `violation` and never reads silence as either (check_guard_verdict.py).
// * A REFUSED READ IS FATAL TO THE PASS. `fault_o` latches, no further request
//   is issued, and the pixels already queued still drain -- the compositor then
//   waits for pixels that never come, which is correct: a frame whose source
//   could not be read must not be published as if it had been.
//
// ---------------------------------------------------------------------------
// COST, STATED BEFORE ANY FIT (none has been run)
// ---------------------------------------------------------------------------
// The beat queue is FIFO_BEATS x 64 bits with a REGISTERED read, written so it
// infers a simple dual-port memory: at 16 beats that is two M10K at the 32x64
// shape rather than 1,024 flops (~256 ALM of registers plus a 16:1 x 64 mux).
// The owner ruling of 2026-09-18 prefers M10K to ALMs, and this is that trade.
// The rest is two row counters, an address accumulator (NO multiplier: the row
// base advances by `stride_i` per row) and a 4:1 halfword select. Estimate:
// ~120 ALM, 0 DSP, 2 M10K. UNMEASURED.
`default_nettype none

module zhao_post_fbread
  import zhao_pkg::*;
#(
    parameter int unsigned XW         = 9,    // pixel x within the view (<= 511)
    parameter int unsigned YW         = 8,    // pixel y within the view (<= 255)
    parameter int unsigned FIFO_BEATS = 16    // power of two, >= 8
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the pass -----------------------------------------------------------
    input  var logic                           start_i,   // one pulse per pass
    input  var logic [ZHAO_VRAM_ADDR_BITS-1:0] origin_i,  // byte addr of view (0,0)
    input  var logic [15:0]                    stride_i,  // bytes per row
    input  var logic [XW-1:0]                  w_i,
    input  var logic [YW-1:0]                  h_i,

    // ---- MEM.GUARD read master (client ENGINE0, reads only) ----------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    input  var logic            beat_valid_i,
    input  var logic [63:0]     beat_data_i,

    // ---- to POST.COMPOSITE's `s_*` ------------------------------------------
    output var logic        px_valid_o,
    input  var logic        px_ready_i,
    output var logic [15:0] px_rgb_o,

    // ---- evidence -----------------------------------------------------------
    output var logic        busy_o,        // a pass is in progress
    output var logic        done_o,        // one pulse: the pass's last pixel left
    output var logic        fault_o,       // sticky per pass: refused read / bad width
    output var logic [31:0] reads_o,       // requests the guard passed
    output var logic [31:0] pixels_o,      // pixels handed to the compositor
    output var logic [31:0] overflow_o     // tripwire: a beat with no room (unreachable)
);

  localparam int unsigned QW = $clog2(FIFO_BEATS);

  initial begin
    if ((FIFO_BEATS < 8) || ((FIFO_BEATS & (FIFO_BEATS - 1)) != 0))
      $fatal(1, "zhao_post_fbread: FIFO_BEATS must be a power of two >= 8 (got %0d)", FIFO_BEATS);
    if (XW > 11)
      $fatal(1, "zhao_post_fbread: XW %0d leaves no room for the byte offset", XW);
  end

  // ==========================================================================
  // REQUEST SIDE
  // ==========================================================================
  typedef enum logic [1:0] {
    R_IDLE = 2'd0,   // no pass, or the pass's last request is out
    R_WAIT = 2'd1,   // waiting for queue room
    R_REQ  = 2'd2,   // offering one read to the guard
    R_VERD = 2'd3    // the guard's verdict, one or more cycles later
  } rstate_e;

  rstate_e       r_st_q;
  logic [XW-1:0] rx_q;                             // next pixel x to request
  logic [YW-1:0] ry_q;                             // row being requested
  logic [ZHAO_VRAM_ADDR_BITS-1:0] row_base_q;      // origin + ry * stride
  logic [XW-1:0] w_q;
  logic [YW-1:0] h_q;
  logic [15:0]   stride_q;

  // pixels in the next request: min(32, w - rx)
  logic [XW-1:0] rem_c;
  logic [5:0]    npx_c;
  assign rem_c = w_q - rx_q;
  assign npx_c = (rem_c >= XW'(32)) ? 6'd32 : 6'(rem_c);

  logic [3:0]    nbeats_c;                         // 1..8
  assign nbeats_c = 4'(npx_c >> 2);

  // Beats already owed to the queue by passed requests, plus the queue's own
  // occupancy: a request is offered only when all of its beats fit.
  logic [QW:0]   q_count_q;                        // beats resident in the queue
  logic [QW:0]   owed_q;                           // beats passed, not yet arrived
  logic          room_c;
  assign room_c = ((QW+1)'(FIFO_BEATS) - q_count_q - owed_q) >= (QW+1)'(nbeats_c);

  logic [3:0]    req_beats_q;                      // beats of the request in flight

  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (r_st_q == R_REQ);
    guard_req_o.write  = 1'b0;
    guard_req_o.client = ZHAO_CLIENT_ENGINE0;
    guard_req_o.addr   = row_base_q + ZHAO_VRAM_ADDR_BITS'({rx_q, 1'b0});
    guard_req_o.len    = 7'({npx_c, 1'b0});        // bytes = pixels * 2
    // The guard wants the FULL contiguous mask over [addr, addr+len).
    guard_req_o.be     = (npx_c == 6'd32) ? '1
                         : ((64'd1 << {npx_c, 1'b0}) - 64'd1);
  end

  // ==========================================================================
  // THE BEAT QUEUE -- simple dual port, registered read (infers memory)
  // ==========================================================================
  logic [63:0]   q_mem [0:FIFO_BEATS-1];
  logic [QW-1:0] q_wp_q, q_rp_q;
  logic [63:0]   head_q;                           // the beat being unpacked
  logic          head_v_q;
  logic [1:0]    idx_q;                            // halfword within head_q
  logic          rd_en_c, px_fire_c, head_done_c;

  assign px_fire_c   = px_valid_o && px_ready_i;
  assign head_done_c = px_fire_c && (idx_q == 2'd3);
  // Load the next beat into the head exactly when the head empties, so a steady
  // stream presents a pixel every cycle with no bubble at beat boundaries.
  assign rd_en_c     = (q_count_q != '0) && (!head_v_q || head_done_c);

  always_ff @(posedge clk) begin
    if (beat_valid_i) q_mem[q_wp_q] <= beat_data_i;
    if (rd_en_c)      head_q        <= q_mem[q_rp_q];
  end

  assign px_valid_o = head_v_q;
  assign px_rgb_o   = head_q[{idx_q, 4'b0000} +: 16];

  // ==========================================================================
  // THE PASS LEDGER
  // ==========================================================================
  // Pixels handed out this pass, counted as (x, y) so `done_o` is the last
  // pixel of the view and never a product that needs a multiplier.
  logic [XW-1:0] ox_q;
  logic [YW-1:0] oy_q;
  logic          pass_q;                           // a pass is live on the output side

  assign busy_o = pass_q;

  // A width that is not a whole number of beats cannot be walked without
  // inventing pixels, and a zero-sized view has no last pixel.
  logic bad_geom_c;
  assign bad_geom_c = (w_i[1:0] != 2'b00) || (w_i == '0) || (h_i == '0);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r_st_q      <= R_IDLE;
      rx_q        <= '0;
      ry_q        <= '0;
      row_base_q  <= '0;
      w_q         <= '0;
      h_q         <= '0;
      stride_q    <= '0;
      req_beats_q <= '0;
      q_count_q   <= '0;
      owed_q      <= '0;
      q_wp_q      <= '0;
      q_rp_q      <= '0;
      head_v_q    <= 1'b0;
      idx_q       <= 2'd0;
      ox_q        <= '0;
      oy_q        <= '0;
      pass_q      <= 1'b0;
      done_o      <= 1'b0;
      fault_o     <= 1'b0;
      reads_o     <= '0;
      pixels_o    <= '0;
      overflow_o  <= '0;
    end else begin
      done_o <= 1'b0;

      // ---- queue occupancy: one assignment, so a push and a pop together
      //      are both counted -------------------------------------------------
      q_count_q <= q_count_q + ((beat_valid_i) ? (QW+1)'(1) : '0)
                             - ((rd_en_c)      ? (QW+1)'(1) : '0);
      if (beat_valid_i) begin
        q_wp_q <= q_wp_q + QW'(1);
        // Structurally unreachable: a request is issued only with room for
        // every beat it returns. Counted rather than asserted so it ships.
        if (q_count_q == (QW+1)'(FIFO_BEATS)) overflow_o <= overflow_o + 32'd1;
      end
      if (rd_en_c) q_rp_q <= q_rp_q + QW'(1);

      // ---- owed beats: + at the verdict, - per arriving beat ---------------
      begin
        automatic logic [QW:0] add = '0;
        if ((r_st_q == R_VERD) && guard_rsp_i.ok) add = (QW+1)'(req_beats_q);
        owed_q <= owed_q + add - ((beat_valid_i && (owed_q != '0)) ? (QW+1)'(1) : '0);
      end

      // ---- the head and the pixel walk --------------------------------------
      if (rd_en_c) begin
        head_v_q <= 1'b1;
        idx_q    <= 2'd0;
      end else if (head_done_c) begin
        head_v_q <= 1'b0;
      end
      if (px_fire_c) begin
        if (!rd_en_c) idx_q <= idx_q + 2'd1;
        pixels_o <= pixels_o + 32'd1;
        if (ox_q == (w_q - XW'(1))) begin
          ox_q <= '0;
          oy_q <= oy_q + YW'(1);
          if (oy_q == (h_q - YW'(1))) begin
            pass_q <= 1'b0;
            done_o <= 1'b1;
          end
        end else begin
          ox_q <= ox_q + XW'(1);
        end
      end

      // ---- the request walk -------------------------------------------------
      case (r_st_q)
        R_IDLE: begin
          if (start_i) begin
            w_q        <= w_i;
            h_q        <= h_i;
            stride_q   <= stride_i;
            row_base_q <= origin_i;
            rx_q       <= '0;
            ry_q       <= '0;
            ox_q       <= '0;
            oy_q       <= '0;
            fault_o    <= bad_geom_c;
            // A new pass starts from an EMPTY queue. The sequencer starts one
            // only after the previous pass finished or faulted, so nothing a
            // flush could drop is still owed to it.
            q_rp_q     <= q_wp_q;
            q_count_q  <= '0;
            owed_q     <= '0;
            head_v_q   <= 1'b0;
            pass_q     <= !bad_geom_c;
            r_st_q     <= bad_geom_c ? R_IDLE : R_WAIT;
          end
        end

        R_WAIT: begin
          if (room_c) begin
            req_beats_q <= nbeats_c;
            r_st_q      <= R_REQ;
          end
        end

        // The guard's ready is a LEVEL; its verdict follows.
        R_REQ: begin
          if (guard_rsp_i.ready) r_st_q <= R_VERD;
        end

        R_VERD: begin
          if (guard_rsp_i.ok) begin
            reads_o <= reads_o + 32'd1;
            if (({1'b0, rx_q} + (XW+1)'(npx_c)) >= {1'b0, w_q}) begin
              rx_q <= '0;
              if (ry_q == (h_q - YW'(1))) begin
                r_st_q <= R_IDLE;                  // the last request is out
              end else begin
                ry_q       <= ry_q + YW'(1);
                row_base_q <= row_base_q + ZHAO_VRAM_ADDR_BITS'(stride_q);
                r_st_q     <= R_WAIT;
              end
            end else begin
              rx_q   <= rx_q + XW'(npx_c);
              r_st_q <= R_WAIT;
            end
          end else if (guard_rsp_i.violation) begin
            fault_o <= 1'b1;
            r_st_q  <= R_IDLE;
          end
          // Neither yet: WAIT. Silence is not an answer.
        end

        default: r_st_q <= R_IDLE;
      endcase
    end
  end

endmodule : zhao_post_fbread

`default_nettype wire
