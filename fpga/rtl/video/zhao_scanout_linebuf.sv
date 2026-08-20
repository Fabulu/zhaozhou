// zhao_scanout_linebuf.sv — VIDEO.SCANOUT line buffers + the documented
// gpu->video pixel-domain bridge (plan W2.2; law: spec/video_rules.md §4,
// decision D7 item 2; contract VIDEO.SCANOUT "Clock and reset semantics").
//
// 2 x 512 x RGB565 ping-pong line buffers (one full line of margin by
// construction: prefetch line N+1 while line N displays), organized as
// 2 x 128 words of 64 b (4 pixels per word = one beat of the fetch client).
// ENFORCED-BY: tests/formal/video_scanout_linebuf.sby
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
// Handshake law (gray-coded toggles, one bit each way per buffer — a
// single-bit code is gray by definition, there is no multi-bit skew to
// encode away):
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
//     ENFORCED-BY: tests/formal/video_scanout_linebuf.sby
//
// KNOWN CDC HAZARD, enforced by SYSTEM timing, not by this module (found
// 2026-08-16 when the formal stimulus became genuinely free; an earlier
// header claimed the abort was toggle-free "by construction", which was
// FALSE for the FULL case): aborting a FULL buffer un-does its completion
// toggle, and an un-toggle IS a toggle — the resulting pulse can be
// sampled by the vid-side 2FF chain as a stale buf_fresh for up to ~2 vid
// cycles while this side already holds the buffer EMPTY (and the fetch may
// refill it): a torn read IF a freshness decision lands in that window.
// ENFORCED-BY: fpga/rtl/video/zhao_scanout_fetch.sv:fill_abort
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
  // Read request, ONE VID CYCLE AHEAD of the pixel that needs it. The RAM read
  // is registered (zhao_dc_sdp_ram), so the consumer issues the address for
  // pixel n during pixel n-1 and `rd_word` carries it when pixel n is current.
  // `rd_en` low means NO read is issued at all, which is what keeps the video
  // side off a buffer the GPU may be refilling.
  input  logic         rd_en,
  input  logic         rd_req_buf,
  input  logic [6:0]   rd_req_addr,
  output logic [63:0]  rd_word,
  output logic [1:0]   buf_fresh      // displayable now (synced_full != seen)
);

  // ------------------------------------------------------------ storage ---
  // 2 x 128 x 64 b = 16,384 bits, held in zhao_dc_sdp_ram as ONE flat address
  // space with the buffer selector as the high address bit. That is what the
  // hardware is: 256 words of 64 bits, one gpu-clock write port, one vid-clock
  // registered read port.
  //
  // It used to be `logic [63:0] mem [0:1][0:127]` with `assign rd_word =
  // mem[rd_buf][rd_addr]`, and the composed synthesis named that asynchronous
  // read as the reason inference failed:
  //   Info (276007): RAM logic "...linebuf:u_linebuf|mem" is uninferred due to
  //                  asynchronous read logic
  // The array is not the mistake -- a line buffer is exactly what block RAM is
  // for -- only the read-port description was.
  //
  // NO CAPACITY CLAIM HERE. The old comment asserted "one M10K"; that was a
  // guess and guesses of this kind are how this class of defect survived.
  // 16,384 bits is the logical payload; what Quartus actually places is
  // recorded in reports/synthesis after it has placed it.
  logic [7:0]  ram_wr_addr, ram_rd_addr;
  assign ram_wr_addr = {fill_buf, fill_addr};
  assign ram_rd_addr = {rd_req_buf, rd_req_addr};

  logic ram_we;

  zhao_dc_sdp_ram #(.DATA_W(64), .ADDR_W(8)) u_ram (
    .wr_clk  (gpu_clk),
    .wr_en   (ram_we),
    .wr_addr (ram_wr_addr),
    .wr_data (fill_data),
    .rd_clk  (vid_clk),
    .rd_en   (rd_en),
    .rd_addr (ram_rd_addr),
    .rd_data (rd_word)
  );

`ifdef FORMAL
  // FORMAL-ONLY storage init, matching the documented Verilator-profile
  // semantics above (canonical 0 before first fill). The silicon M10K
  // powers up undefined — which is why the formal harness ALSO tracks
  // written-ness and only claims equality for addresses the fill wrote —
  // but without this init the solver reasons about a fully symbolic
  // 16-Kbit initial array and the never-torn BMC stalls for tens of
  // minutes per step (boolector AND yices, measured 2026-08-16).
  // Structurally absent outside `ifdef FORMAL (the W2.6 precedent).
  initial begin
    for (int unsigned fj = 0; fj < 256; fj++) begin
      u_ram.mem[fj] = 64'd0;
    end
  end
`endif

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
  assign ram_we       = fill_we && fill_we_ok;
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
        // the word itself is written by u_ram, gated by the same condition
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
  // rd_word is driven by u_ram's registered read. Nothing here reads the array.
  //
  // The write enable is the SAME predicate the ownership process uses, lifted
  // out so the RAM sees it directly: a write only happens to a buffer that is
  // EMPTY or FILLING, which is what makes a legal read and a legal write
  // unable to target the same buffer generation. That is the collision
  // argument zhao_dc_sdp_ram requires of its users, and it is discharged by
  // the ping-pong ownership protocol already in this file rather than by any
  // arbitration in the RAM.

endmodule : zhao_scanout_linebuf
