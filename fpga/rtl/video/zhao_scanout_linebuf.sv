// zhao_scanout_linebuf.sv — VIDEO.SCANOUT line buffers + the documented
// gpu->video pixel-domain bridge (plan W2.2; law: spec/video_rules.md §4,
// decision D7 item 2; contract VIDEO.SCANOUT "Clock and reset semantics").
//
// 2 x 512 x RGB565 ping-pong line buffers (one full line of margin by
// construction: prefetch line N+1 while line N displays), organized as
// 2 x 128 words of 64 b (4 pixels per word = one beat of the fetch client).
// The WRITE port lives in the gpu domain (zhao_scanout_fetch), the READ
// port in the vid domain (zhao_scanout_serializer); the storage itself is
// the documented asynchronous bridge between the two (ratio 2:1 frozen,
// plan D1/R1).
//
// Per-buffer state machine (gpu domain view):
//
//        fill_we(first)      fill_line_done          consume_done (credit,
//   EMPTY --------------> FILLING ------------> FULL   vid edge observed)
//     ^  <---- fill_abort ----/    |                     |
//     |------------------------------------------------| credit
//
// Handshake law (gray-coded toggles, one bit each way per buffer — a single
// bit toggling is gray by construction):
//
//   full path   (gpu->vid): completing a fill XORs full_toggle[i]; the vid
//               side 2-flop-synchronizes it. A buffer is FRESH for display
//               iff synced_full_toggle[i] != last_seen[i].
//   empty path  (vid->gpu): finishing the display of a FRESH line XORs
//               consumed_toggle[i]; the gpu side 2-flop-synchronizes it and
//               returns the buffer to EMPTY (the refill credit).
//
// Safety (formal property video_scanout_linebuf, checked on this module in
// isolation by the SBY harness):
//   * writes are accepted only in EMPTY (first beat) or FILLING (rest) —
//     a FULL or credit-in-flight buffer is structurally never overwritten,
//     so the serializer never sees a torn line;
//   * FRESH rises only on fill_line_done, and a fresh buffer's content is
//     stable from fill_line_done until its consumption completes;
//   * fill_abort returns a FILLING buffer to EMPTY without ever toggling
//     full — partial data is unobservable by construction.
//
// KNOWN CDC HAZARD, enforced by SYSTEM timing, not by this module (found
// 2026-08-16 when the formal stimulus became genuinely free; an earlier
// header claimed the abort was toggle-free "by construction", which was
// FALSE for the FULL case): aborting a FULL buffer un-does its completion
// toggle, and an un-toggle IS a toggle — the resulting pulse can be
// sampled by the vid-side 2FF chain as a stale buf_fresh for up to ~2 vid
// cycles while this side already holds the buffer EMPTY (and the fetch may
// refill it): a torn read IF a freshness decision lands in that window.
// The ENFORCER is zhao_scanout_fetch's abort schedule: fill_abort of a
// FULL buffer fires only at the vswap_dec frame re-arm or the frame_start
// mode flush — both in/at vblank, with the serializer's next freshness
// decision (consume_start, taken at line-end edges only) at least a full
// raster line away, orders of magnitude beyond the 2FF window. The formal
// harness assumes exactly that spacing (video_linebuf_fv.sv, 4-cycle
// cooldown); any future integration that aborts OUTSIDE vblank must
// re-establish it or redesign this crossing.
//
// Conservative SystemVerilog subset only (charter §2). Storage reads as 0
// before its first fill on the Verilator profile (canonical black) — the
// starved-line law reads stale (possibly never-filled) content, which is
// deterministic on both sides of the differential.
//
// Lint: clean under `verilator_bin --lint-only -Wall` (CTest
// lint_zhao_video_scanout).

module zhao_scanout_linebuf
  import zhao_pkg::*;
(
  // fill side (gpu domain)
  input  logic         gpu_clk,
  input  logic         rst_n,
  input  logic         fill_buf,      // 0/1: buffer this write targets
  input  logic [6:0]   fill_addr,     // word index 0..127 (4 px each)
  input  logic [63:0]  fill_data,
  input  logic         fill_we,       // write ONE 64-bit word (4 px)
  input  logic         fill_line_done,// pulse: buffer fill_buf is COMPLETE
  input  logic [1:0]   fill_abort,    // pulse mask: discard buffer(s)
  output logic [1:0]   buf_empty,     // per-buffer refill credit (EMPTY)

  // read side (vid domain; same reset net — frozen 2:1 profile, plan R1)
  input  logic         vid_clk,
  input  logic [1:0]   consume_start, // line start: freshness of buf i taken
  input  logic [1:0]   consume_done,  // line end: buf i consumed -> credit
  input  logic         rd_buf,
  input  logic [6:0]   rd_addr,
  output logic [63:0]  rd_word,
  output logic [1:0]   buf_fresh      // displayable now (synced_full != seen)
);

  // ------------------------------------------------------------ storage ---
  // 2 x 128 x 64 b = 16 Kbit (one M10K). No reset (size); canonical 0 read
  // before first fill on the Verilator profile.
  logic [63:0] mem [0:1][0:127];

  // per-buffer state, gpu domain: 0=EMPTY 1=FILLING 2=FULL
  localparam logic [1:0] LB_EMPTY   = 2'd0;
  localparam logic [1:0] LB_FILLING = 2'd1;
  localparam logic [1:0] LB_FULL    = 2'd2;

  logic [1:0] bstate [0:1];

  // vid->gpu consumption-edge decode (declared before use; the crossing
  // chain itself lives below)
  logic [1:0] cons_s1, cons_s2, cons_s2q;
  logic [1:0] cons_edge;
  assign cons_edge = cons_s2 ^ cons_s2q;

  logic [1:0] full_toggle;       // gpu -> vid (fill completion)
  logic [1:0] consumed_toggle;   // vid -> gpu (consumption completion)

  logic       fill_we_ok, fill_done_ok;
  assign fill_we_ok   = (bstate[fill_buf] == LB_EMPTY)
                     || (bstate[fill_buf] == LB_FILLING);
  assign fill_done_ok = (bstate[fill_buf] == LB_FILLING);

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      bstate[0]    <= LB_EMPTY;
      bstate[1]    <= LB_EMPTY;
      full_toggle  <= 2'b00;
    end else begin
      // write port: accepted only in EMPTY/FILLING (never over a FULL buf)
      if (fill_we && fill_we_ok) begin
        mem[fill_buf][fill_addr] <= fill_data;
      end

      // state transitions (priority: abort > done > we > credit)
      for (int unsigned i = 0; i < 2; i++) begin
        if (fill_abort[i]) begin
          bstate[i] <= LB_EMPTY;               // discard; a discarded FULL
          if (bstate[i] == LB_FULL) begin       // fill also un-does its
            full_toggle[i] <= full_toggle[i] ^ 1'b1;  // completion toggle,
          end                                   // or the 1-bit freshness
        end else if (fill_line_done && (fill_buf == i[0]) && fill_done_ok) begin
          bstate[i]      <= LB_FULL;
          full_toggle[i] <= full_toggle[i] ^ 1'b1;
        end else if (fill_we && (fill_buf == i[0]) && fill_we_ok
                     && (bstate[i] == LB_EMPTY)) begin
          bstate[i] <= LB_FILLING;
        end else if (cons_edge[i]) begin
          bstate[i] <= LB_EMPTY;               // display credit -> refill
        end
      end
    end
  end

  assign buf_empty = {(bstate[1] == LB_EMPTY), (bstate[0] == LB_EMPTY)};

  // ------------------------------------------------ vid -> gpu crossing ---

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      cons_s1  <= 2'b00;
      cons_s2  <= 2'b00;
      cons_s2q <= 2'b00;
    end else begin
      cons_s1  <= consumed_toggle;
      cons_s2  <= cons_s1;
      cons_s2q <= cons_s2;
    end
  end

  // ------------------------------------------------ gpu -> vid crossing ---
  logic [1:0] full_s1, full_s2, last_seen;

  always_ff @(posedge vid_clk or negedge rst_n) begin
    if (!rst_n) begin
      consumed_toggle <= 2'b00;
      full_s1         <= 2'b00;
      full_s2         <= 2'b00;
      last_seen       <= 2'b00;
    end else begin
      full_s1 <= full_toggle;
      full_s2 <= full_s1;
      for (int unsigned i = 0; i < 2; i++) begin
        if (consume_start[i]) begin
          last_seen[i] <= full_s2[i];   // freshness taken OFF the table now
        end
        if (consume_done[i]) begin
          consumed_toggle[i] <= consumed_toggle[i] ^ 1'b1;  // -> credit
        end
      end
    end
  end

  assign buf_fresh = full_s2 ^ last_seen;

  // ------------------------------------------------------------ read port --
  assign rd_word = mem[rd_buf][rd_addr];

endmodule : zhao_scanout_linebuf
