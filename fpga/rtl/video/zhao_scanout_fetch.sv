// zhao_scanout_fetch.sv — VIDEO.SCANOUT fetch client (gpu domain)
// (plan W2.2; law: spec/video_rules.md §3-§4, decision D7 item 1; contract
// VIDEO.SCANOUT "Input and output packet layouts" / "Memory ownership").
//
// The STRICT-PRIORITY guaranteed arbiter client (D3): it reads the displayed
// slot READ-ONLY through MEM.GUARD, 64-B bursts (8 beats of 4 RGB565
// pixels), one display line at a time, streaming each beat as one 64-bit
// word into the ping-pong line buffers.
//
// READ-DATA RETURN CHANNEL: the frozen guard response (zhao_guard_rsp_t)
// carries only admission {ready, ok, violation}; read beats return on the
// separate {beat_valid, beat_data} channel — the W2.5 seam (arbiter/SDRAM
// beat return). The W2.2 Verilator harness drives it with the frozen sim
// profile (fixed latency, injectable starvation).
//
// Frame re-arm law (spec/video_rules.md §4 swap timing): the swap decision
// happens at vswap_dec (raster (0, 244)); fetch re-arms THERE — crossed to
// the gpu domain — so a full vblank (18 lines) of prefetch margin exists
// before the frame's first active line. Geometry (mode/slot) is taken from
// the PENDING mode at dec (mode_next) and re-checked at frame_start against
// the authoritative latched mode; a mode_we landing inside the dec ->
// frame_start vblank window flushes the partial prefetch and refetches the
// frame under the new mode (starvation-visible, never torn — deterministic
// on both sides of the differential).
//
// Geometry (framebuffer layout, spec/video_rules.md §3):
//   Z60    line y: [base + y*768,  +768)  — 1 segment (12 x 64 B)
//   Storm  line y: [base + y*640,  +640)  — 1 segment (10 x 64 B)
//   Duo    line y in [24, 216):
//            view0 [base + (y-24)*512,        +512)   (8 x 64 B)
//            view1 [base + 0x18000 + (y-24)*512, +512) (8 x 64 B)
//          — 2 segments written into the SAME 128-word buffer (view1
//            follows view0); border lines fetch NOTHING (the serializer
//            emits the black border locally, spec §3.1).
//
// Guard violation (impossible in Phase 2 — scanout owns both slots
// read-only): the partial buffer is discarded (fill_abort, never full) and
// the line is retried; the missed display counts starvation (contract
// "Overflow and malformed-input behaviour").
//
// Conservative SystemVerilog subset only (charter §2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

module zhao_scanout_fetch
  import zhao_pkg::*;
(
  input  logic        gpu_clk,
  input  logic        rst_n,

  // MEM.GUARD read client port (gpu domain)
  output zhao_guard_req_t guard_req,
  input  zhao_guard_rsp_t  guard_rsp,
  // read-beat return channel (W2.5 seam; harness-driven in W2.2)
  input  logic        beat_valid,
  input  logic [63:0] beat_data,
  input  logic        beat_last,    // conformance only (8 beats per request)

  // line-buffer fill port (gpu side of zhao_scanout_linebuf)
  output logic        fill_buf,
  output logic [6:0]  fill_addr,
  output logic [63:0] fill_data,
  output logic        fill_we,
  output logic        fill_line_done,
  output logic [1:0]  fill_abort,
  input  logic [1:0]  buf_empty,

  // control crossings from the vid domain (synchronized at the top level)
  input  logic        dec_sync,           // vswap_dec re-arm (next frame)
  input  logic        frame_start_sync,   // frame_start (mode re-check)
  input  logic [0:0]  display_slot_sync,  // slot to fetch (post-swap)
  input  zhao_mode_e  mode_next_sync,     // pending mode at dec (prefetch)
  input  zhao_mode_e  mode_sync,          // latched mode (authoritative)

  output logic        req_active          // trace: a burst is in flight
);

  // ---------------------------------------------------------------- FSM ---
  typedef enum logic [2:0] {
    F_ARM      = 3'd0,  // per-line arm (skips Duo border lines)
    F_WAIT     = 3'd1,  // wait for the EMPTY credit on the target buffer
    F_REQ      = 3'd2,  // offer a 64-B guard read; wait ready
    F_BEATS    = 3'd3,  // collect 8 beats -> 8 words into the line buffer
    F_PARK     = 3'd4   // frame fully fetched; park until the next dec_sync
  } fetch_state_e;

  fetch_state_e state;

  zhao_mode_e   fetch_mode;     // geometry this frame's fetch runs under
  logic [0:0]   fetch_slot;
  logic [7:0]   fetch_line;     // DISPLAY line index 0..239 being filled
  logic         seg_idx;        // 0/1: Duo view segment
  logic [3:0]   req_idx;        // 64-B request within the segment
  logic [2:0]   beat_cnt;       // beat within the request (0..7)
  logic [7:0]   fill_words;     // words written into the buffer this line

  logic         fill_line_buf;  // 0/1 ping-pong select (gpu-domain view)

  // ------------------------------------------------------ geometry math ---
  logic [26:0] base_addr;
  // ---- the per-line base is REGISTERED, because it changes ONCE A LINE ----
  // Round 14's worst path, and the one bro predicted by name:
  //
  //     u_fetch|fetch_line[0] -> Add0  +1.27 ns   fetch_line * 768
  //                           -> Sel5  +0.25
  //                           -> Add6  +1.42      + req_idx * 64
  //                           -> u_guard_scan|Add0 +1.33
  //                           -> LessThan68 +0.59 -> fwd_active +0.51
  //                           -> mem_guard|fwd_req.addr[11]
  //
  // Three adder chains and a compare, ACROSS TWO MODULES, in one cycle.
  // `fetch_line * 768` sat at the head of it, recomputed every cycle from a
  // value that changes once per SCANLINE -- roughly every 800 cycles.
  //
  // Same shape as the four EDGEWALK faults this pass: arithmetic on a value
  // that is stable for the whole operation, left combinational because it
  // looks too cheap to bother registering, and standing at the head of a long
  // path.
  //
  // SAFE BY THE FSM, NOT BY ASSERTION. `line_base_r` lags `fetch_line` by one
  // cycle, so it must not be read in the cycle after fetch_line moves.
  // fetch_line is assigned in exactly two running states (the Duo border skip
  // in F_ARM, and the last segment in F_BEATS) and BOTH set state <= F_ARM.
  // The route to the next request is F_ARM -> F_WAIT -> F_REQ, so seg_addr is
  // never consumed less than two cycles after the change. The reset paths
  // also land in F_ARM.
  //
  // Only the constant multiply is registered. `base_addr` and the Duo
  // `seg_idx` term stay combinational: they are an add and a mux, and seg_idx
  // moves per segment rather than per line, so lagging it would NOT be safe.
  logic [31:0] line_base_c, line_base_r;
  logic [31:0] seg_addr;
  logic [3:0]  seg_reqs;
  logic        line_real, line_last;
  logic [7:0]  line_next;

  always_comb begin
    base_addr   = 27'(fetch_slot ? ZHAO_FB_SLOT1_BASE : ZHAO_FB_SLOT0_BASE);
    line_base_c = 32'd0;
    seg_addr    = 32'd0;
    seg_reqs  = 4'd0;
    line_real = 1'b1;
    line_last = 1'b0;
    line_next = 8'd0;
    unique case (fetch_mode)
      ZHAO_MODE_Z60: begin
        line_base_c = 32'(fetch_line) * 32'd768;
        seg_addr  = {5'b00000, base_addr} + line_base_r;
        seg_reqs  = 4'd12;
        line_last = (fetch_line == 8'd239);
        line_next = fetch_line + 8'd1;
      end
      ZHAO_MODE_STORM: begin
        line_base_c = 32'(fetch_line) * 32'd640;
        seg_addr  = {5'b00000, base_addr} + line_base_r;
        seg_reqs  = 4'd10;
        line_last = (fetch_line == 8'd239);
        line_next = fetch_line + 8'd1;
      end
      ZHAO_MODE_DUO: begin
        if (fetch_line < 8'd24) begin
          line_real = 1'b0;                 // top border: skip to view 0
          line_next = 8'd24;
        end else if (fetch_line >= 8'd216) begin
          line_real = 1'b0;                 // bottom border (park path)
          line_next = 8'd24;
        end else begin
          line_base_c = 32'(fetch_line - 8'd24) * 32'd512;
          seg_addr  = {5'b00000, base_addr}
                    + line_base_r
                    + (seg_idx ? 32'h0001_8000 : 32'd0);
          seg_reqs  = 4'd8;
          line_last = (fetch_line == 8'd215);
          line_next = fetch_line + 8'd1;
        end
      end
      default: begin
        line_real = 1'b0;   // unreachable: 2-bit enum, three declared values
        line_last = 1'b1;
        line_next = 8'd0;
      end
    endcase
  end

  // current request (read-only, all-ones byte enables, 64 B)
  assign guard_req.valid  = (state == F_REQ);
  assign guard_req.write  = 1'b0;
  assign guard_req.client = ZHAO_CLIENT_SCANOUT;
  assign guard_req.addr   = 27'(seg_addr + 32'(req_idx) * 32'd64);
  assign guard_req.len    = 7'd64;
  assign guard_req.be     = 64'hFFFF_FFFF_FFFF_FFFF;

  // beat -> word stream into the line buffer (one 64-bit word per beat)
  assign fill_buf  = fill_line_buf;
  assign fill_addr = fill_words[6:0];
  assign fill_data = beat_data;
  assign fill_we   = (state == F_BEATS) && beat_valid;
  assign req_active= (state == F_REQ) || (state == F_BEATS);

  // ---- completion / abort are COMBINATIONAL levels of the completing
  // cycle so they always carry the CURRENT fill_line_buf (the select only
  // advances at the same NBA edge — a registered pulse would name the
  // NEXT buffer: the classic ping-pong off-by-one, law: never torn).
  logic beat_eob;        // this beat is the last of its 64-B request
  logic seg_complete;    // this beat completes the segment
  logic line_complete;   // this beat completes the display line
  logic flush_now;       // frame re-arm or vblank mode flush fires now
  logic violation_now;   // the guard denies the in-flight request now

  assign beat_eob      = (state == F_BEATS) && beat_valid && (beat_cnt == 3'd7);
  assign seg_complete  = beat_eob && (req_idx == (seg_reqs - 4'd1));
  assign line_complete = seg_complete &&
                         ((fetch_mode != ZHAO_MODE_DUO) || (seg_idx == 1'b1));
  assign flush_now     = dec_sync ||
                         (frame_start_sync && (mode_sync != fetch_mode));
  assign violation_now = (state == F_REQ) && guard_rsp.ready &&
                         guard_rsp.violation;

  assign fill_line_done = line_complete;
  // frame re-arm / mode flush discards BOTH buffers (the per-frame
  // re-alignment law — see zhao_scanout_linebuf.sv); a guard violation
  // retries just the line's own buffer
  assign fill_abort = (flush_now || violation_now)
                    ? ((flush_now && !violation_now) ? 2'b11
                                                     : (fill_line_buf ? 2'b10 : 2'b01))
                    : 2'b00;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      line_base_r   <= 32'd0;
      state         <= F_ARM;
      fetch_mode    <= ZHAO_MODE_Z60;   // free-run from reset (contract:
      fetch_slot    <= 1'b0;            //  raster repeats black until the
      fetch_line    <= 8'd0;            //  first complete frame is fetched;
      seg_idx       <= 1'b0;            //  defaults slot 0 / Z60)
      req_idx       <= 4'd0;
      beat_cnt      <= 3'd0;
      fill_words    <= 8'd0;
      fill_line_buf <= 1'b0;
    end else begin
      // Tracks fetch_line by one cycle, which the FSM makes safe: every
      // assignment to fetch_line also sets state <= F_ARM, and the route
      // to the next request is F_ARM -> F_WAIT -> F_REQ.
      line_base_r <= line_base_c;

      if (dec_sync) begin
        // ---- frame re-arm at the swap decision (vswap_dec crossed) -----
        // Abort any in-flight burst (state leaves F_BEATS: further beats
        // are ignored), discard the partial buffer WITHOUT marking it full.
        state         <= F_ARM;
        fetch_mode    <= mode_next_sync;
        fetch_slot    <= display_slot_sync;
        fetch_line    <= 8'd0;
        seg_idx       <= 1'b0;
        req_idx       <= 4'd0;
        beat_cnt      <= 3'd0;
        fill_words    <= 8'd0;
        fill_line_buf <= 1'b0;
      end else if (frame_start_sync && (mode_sync != fetch_mode)) begin
        // ---- a mode_we landed inside the dec->frame_start vblank window:
        // flush the prefetch, refetch the frame under the latched mode ----
        state         <= F_ARM;
        fetch_mode    <= mode_sync;
        fetch_line    <= 8'd0;
        seg_idx       <= 1'b0;
        req_idx       <= 4'd0;
        beat_cnt      <= 3'd0;
        fill_words    <= 8'd0;
        fill_line_buf <= 1'b0;
      end else begin
        unique case (state)
          F_ARM: begin
            // per-line arm: reset the word counter; skip border lines
            fill_words <= 8'd0;
            seg_idx    <= 1'b0;
            req_idx    <= 4'd0;
            if (line_real) begin
              state <= F_WAIT;
            end else begin
              fetch_line <= line_next;        // Duo border -> view 0
              state      <= F_ARM;
            end
          end

          F_WAIT: begin
            if (buf_empty[fill_line_buf]) begin
              state <= F_REQ;
            end
          end

          F_REQ: begin
            if (guard_rsp.ready) begin
              if (guard_rsp.violation) begin
                // denied (impossible in Phase 2): discard + retry the line
                state        <= F_ARM;
                seg_idx      <= 1'b0;
                req_idx      <= 4'd0;
                fill_words   <= 8'd0;
              end else begin
                state    <= F_BEATS;
                beat_cnt <= 3'd0;
              end
            end
          end

          F_BEATS: begin
            if (beat_valid) begin
              fill_words <= fill_words + 8'd1;
              if (beat_cnt == 3'd7) begin
                beat_cnt <= 3'd0;
                if (req_idx == (seg_reqs - 4'd1)) begin
                  if ((fetch_mode != ZHAO_MODE_DUO) || (seg_idx == 1'b1)) begin
                    // ---- LINE complete: buffer -> FULL (combinationally
                    // ---- above), ping-pong select + line advance here
                    fill_line_buf <= ~fill_line_buf;
                    req_idx       <= 4'd0;
                    seg_idx       <= 1'b0;
                    fetch_line    <= line_next;
                    state         <= line_last ? F_PARK : F_ARM;
                  end else begin
                    seg_idx <= 1'b1;           // Duo: view1 follows in-buffer
                    req_idx <= 4'd0;
                    state   <= F_REQ;
                  end
                end else begin
                  req_idx <= req_idx + 4'd1;
                  state   <= F_REQ;
                end
              end else begin
                beat_cnt <= beat_cnt + 3'd1;
              end
            end
          end

          F_PARK: begin
            state <= F_PARK;    // frame fetched; wait for the next dec_sync
          end

          default: state <= F_ARM;
        endcase
      end
    end
  end

  /* verilator lint_off UNUSEDSIGNAL */
  logic unused_conformance;
  assign unused_conformance = beat_last ^ guard_rsp.ok; // conformance +
  /* verilator lint_on UNUSEDSIGNAL */   // ok is checked by the responder

endmodule : zhao_scanout_fetch
