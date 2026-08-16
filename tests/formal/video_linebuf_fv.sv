// video_linebuf_fv.sv — formal harness for zhao_scanout_linebuf (tests/
// formal/video_scanout_linebuf.sby). FORMAL COMPONENT — never synthesized,
// never linted by the RTL lanes.
//
// An abstract FETCH driver (fill protocol as assumptions) and DISPLAY
// driver (consume protocol) exercise the ping-pong handshake; a shadow
// memory written exactly on the accepted fill beats proves the NEVER-TORN
// law: a fresh buffer's read port returns exactly its last completed fill.

// FREE STIMULUS MUST BE PORTS (soundness, found 2026-08-16 by the cover
// task): the salvaged harness declared the fetch/display stimulus as
// UNDRIVEN INTERNAL logic. read_slang ties undriven locals to 1'x, and
// yosys `prep`'s opt constant-folds `if (x)` branches away BEFORE sby's
// setundef can turn the leftovers into anyseq — so parts of the harness
// (and its assumptions) were silently deleted and the "free" stimulus was
// only free where optimization happened to keep it. Same failure mode as
// the recorded `(* anyseq *)`-on-locals trap (W2.5 ratification note),
// wearing plainer clothes. Top-level INPUT PORTS are genuinely free in
// this flow — the mode and framectl harnesses already did it this way.
module video_linebuf_fv
  import zhao_pkg::*;
(
  input logic clk,
  input logic rst_n,
  // free (constrained) fetch-side stimulus
  input logic        fill_we,
  input logic [6:0]  fill_addr,
  input logic [63:0] fill_data,
  input logic        fill_line_done,
  input logic [1:0]  fill_abort,
  // free (constrained) display-side stimulus
  input logic [1:0]  consume_start,
  input logic [1:0]  consume_done,
  input logic        rd_buf,
  input logic [6:0]  rd_addr,
  // symbolic WATCH ADDRESS for the never-torn law: a free input HELD
  // CONSTANT by assumption (the anyconst idiom, but as a port � the
  // recorded frontend trap ties attribute-carrying LOCALS to constants/x,
  // so the symbolic constant must enter through the port list too)
  input logic [6:0]  f_addr
);

  reg          fill_buf_q = 1'b0;   // fetch-side ping-pong select
  reg          filling_q = 1'b0;    // a fill session is open
  logic [1:0]  buf_fresh;
  logic [1:0]  buf_empty;
  logic [63:0] rd_word;

  zhao_scanout_linebuf dut (
    .gpu_clk(clk), .rst_n(rst_n),
    .fill_buf(fill_buf_q), .fill_addr(fill_addr), .fill_data(fill_data),
    .fill_we(fill_we), .fill_line_done(fill_line_done),
    .fill_abort(fill_abort), .buf_empty(buf_empty),
    .vid_clk(clk),
    .consume_start(consume_start), .consume_done(consume_done),
    .rd_buf(rd_buf), .rd_addr(rd_addr), .rd_word(rd_word),
    .buf_fresh(buf_fresh)
  );

  // ---- the fetch protocol (ASSUMED; zhao_scanout_fetch guarantees it) --
  // * a fill beat is only offered to a buffer that is EMPTY (first beat)
  //   or FILLING (the rest) — the module's own gate re-checks it
  // * line_done only after at least one fill beat of this line
  reg f_past_valid = 0;
  always @(posedge clk) begin
    f_past_valid <= 1;
    // reset discipline: the proof run starts in reset, released at most once
    if (!f_past_valid) assume(!rst_n);
    else assume(!$past(rst_n) || rst_n);   // release is monotonic
  end

  reg [1:0] beats_of_line = 2'd0;
  always @(posedge clk) begin   // harness trackers: synchronous reset
    if (!rst_n) begin
      beats_of_line <= 2'd0;
    end else begin
      if (fill_line_done) beats_of_line <= 2'd0;
      else if (fill_we && beats_of_line < 2'd3) beats_of_line <= beats_of_line + 2'd1;
    end
  end

  always @(posedge clk) begin
    if (rst_n) begin
      // never write a FULL buffer (fetch-side law)
      assume(!(fill_we && !buf_empty[fill_buf_q] && !filling_q));
      // line_done only for a line that actually filled beats this session
      assume(!(fill_line_done && beats_of_line == 2'd0));
      // abort and line_done never together
      assume(!(fill_abort != 2'b00 && fill_line_done));
      // WIDTH-REDUCTION ABSTRACTION (measured 2026-08-16): with all 64 data
      // bits free, boolector AND yices stall 15-45 min on a single BMC
      // assert query from step 8 on (solo box, zero-init storage — the cost
      // is the symbolic store/select chains x 64-bit equality). The
      // never-torn law is HANDSHAKE/ADDRESS-level: the datapath is a pure
      // 64-bit bus with no arithmetic, so any structurally torn read
      // returns a DIFFERENT word, and ONE free data bit suffices for the
      // solver to exhibit any such difference (it chooses distinguishing
      // values). Scope stated plainly: data equality is proven for the
      // 1-effective-bit alphabet; the bus carries the other 63 bits
      // untouched by construction (assign-through, no logic).
      assume(fill_data[63:1] == 63'd0);
    end
  end
  always @(posedge clk) begin   // harness tracker: synchronous reset
    if (!rst_n) filling_q <= 1'b0;
    else if (fill_line_done || (fill_abort[fill_buf_q] != 1'b0)) filling_q <= 1'b0;
    else if (fill_we && !filling_q) filling_q <= 1'b1;
  end

  // MERGE FIX (found by the c_fresh_both cover): the salvaged harness
  // declared fill_buf_q with init 0 and NEVER ASSIGNED it — the abstract
  // fetch driver could only ever fill buffer 0, so buffer 1's half of the
  // never-torn assertion was VACUOUS (buf_fresh[1] unreachable). Mirror
  // the real fetch alternation: the select toggles on line completion and
  // holds for a retry after an abort of the filling buffer.
  always @(posedge clk) begin
    if (!rst_n) fill_buf_q <= 1'b0;
    else if (fill_line_done) fill_buf_q <= ~fill_buf_q;
  end

  // the display protocol (ASSUMED; zhao_scanout_serializer guarantees it):
  // consume_start only on a FRESH buffer, consume_done only after its start
  reg [1:0] started = 2'b00;
  always @(posedge clk) begin   // harness tracker: synchronous reset
    if (!rst_n) begin
      started <= 2'b00;
    end else begin
      if (consume_start[0] && buf_fresh[0]) started[0] <= 1'b1;
      else if (consume_done[0]) started[0] <= 1'b0;
      if (consume_start[1] && buf_fresh[1]) started[1] <= 1'b1;
      else if (consume_done[1]) started[1] <= 1'b0;
    end
  end
  always @(posedge clk) begin
    if (rst_n) begin
      assume(!(consume_start[0] && !buf_fresh[0]));
      assume(!(consume_start[1] && !buf_fresh[1]));
      assume(!(consume_done[0] && !started[0]));
      assume(!(consume_done[1] && !started[1]));
    end
  end

  // ---- SYSTEM spacing law: no freshness decision inside the abort window --
  // Found 2026-08-16 by this property once the stimulus was genuinely free:
  // aborting a FULL buffer "un-does" its completion toggle, and an un-toggle
  // IS a toggle — a pulse the vid-side 2FF chain can sample as a stale
  // buf_fresh for ~2 vid cycles while the gpu side already holds the buffer
  // EMPTY and may be refilling it (a real torn-read CEX at depth 6). The
  // MODULE does not enforce safety here; the SYSTEM does: fill_abort fires
  // only at the frame re-arm / vblank mode flush (zhao_scanout_fetch), and
  // the serializer's next freshness decision (consume_start) is a full
  // raster line away — orders of magnitude beyond the 2FF window. That
  // spacing is assumed here (4 cycles covers the crossing), documented in
  // the zhao_scanout_linebuf.sv header, and MUST be re-established by any
  // future integration that aborts outside vblank.
  reg [2:0] abort_cooldown = 3'd0;
  always @(posedge clk) begin
    if (!rst_n)                     abort_cooldown <= 3'd0;
    else if (fill_abort != 2'b00)   abort_cooldown <= 3'd4;
    else if (abort_cooldown != 3'd0) abort_cooldown <= abort_cooldown - 3'd1;
  end
  always @(posedge clk) begin
    if (rst_n) begin
      assume(!(consume_start != 2'b00 &&
               (fill_abort != 2'b00 || abort_cooldown != 3'd0)));
      // ...and an abort never lands inside an OPEN consumption: aborts fire
      // at the vblank re-arm/mode flush; consume_done for the last real
      // line retired at that line's own end, one-plus cycles earlier.
      assume(!(fill_abort != 2'b00 && started != 2'b00));
    end
  end

  // ---- NEVER-TORN: symbolic-address shadow of the accepted fill beats ----
  // The watch's write gate MIRRORS the DUT's acceptance gate exactly
  // (hierarchical bstate view) — the salvaged harness approximated it with
  // its own session tracker, so shadow and mem could legally diverge and
  // the comparison meant less than it claimed.
  // Symbolic-address watch (replaces a full shadow memory + 128-bit written
  // mask whose barrel
  // shifter made the solver crawl: minutes per BMC step). f_addr is a free
  // constant; proving the law at ONE arbitrary address proves it at all.
  // The storage has NO reset (16-Kbit M10K; canonical-0 reads are a
  // Verilator-profile artefact � in formal it is anyinit garbage), so
  // equality is only claimable for addresses the fill session actually
  // WROTE; the every-word-written guarantee belongs to zhao_scanout_fetch
  // (seg_reqs x 8 beats per line, differential-verified), not here.
  always @(posedge clk) begin
    if (f_past_valid) assume(f_addr == $past(f_addr));  // held constant
  end

  reg [63:0] f_shadow [0:1];
  reg [1:0]  f_written = 2'b00;
  always @(posedge clk) begin
    if (!rst_n) begin
      f_written <= 2'b00;
    end else if (fill_we && (dut.bstate[fill_buf_q] == 2'd0 ||
                             dut.bstate[fill_buf_q] == 2'd1)) begin
      if (dut.bstate[fill_buf_q] == 2'd0) begin
        // first beat of a NEW session: previous session's words are stale
        f_written[fill_buf_q] <= (fill_addr == f_addr);
      end else if (fill_addr == f_addr) begin
        f_written[fill_buf_q] <= 1'b1;
      end
      if (fill_addr == f_addr) f_shadow[fill_buf_q] <= fill_data;
    end
  end

  // THE never-torn law, scoped to what the serializer actually does: from
  // consume_start (freshness taken) to consume_done, every read of an
  // address the completed fill WROTE returns exactly that fill's data — no
  // mid-refill word can ever surface into a line being DISPLAYED. (A
  // blip-window read WITHOUT a taken freshness is benign: the serializer
  // ignores rd_word unless it took the line's freshness — the abort-blip
  // hazard on buf_fresh itself is documented in the module header and
  // excluded by the spacing assumptions above, which mirror the system's
  // vblank-only aborts.)
  always @(posedge clk) begin
    if (rst_n && started[rd_buf] && rd_addr == f_addr && f_written[rd_buf]) begin
      assert(rd_word == f_shadow[rd_buf]);
    end
  end

  // ---- SELF-ASSERTING SCOPE GUARD (the arbiter a_horizon_is_refresh_free
  // pattern): the bmc task is bounded at depth 8, which admits AT MOST four
  // completed fill sessions (a session needs >= 2 cycles: a beat, then
  // line_done; the first can complete at step 2, then 4, 6, 8). Every CEX
  // class this property has ever produced lies inside that window (the
  // abort-blip torn read at step 6; the credit round-trip and both-fresh
  // at step 8). If anyone raises the depth past what was actually proven,
  // a FIFTH completion becomes reachable and this guard FIRES — the run
  // then fails loudly instead of silently re-scoping what "PASS" means,
  // and the depth/cost trade-off must be re-derived (measured 2026-08-16:
  // each step past 7 costs ~10 solver-minutes, solo box, boolector AND
  // yices; the killed-at-timeout history is in the run records).
  reg [3:0] f_sessions = 4'd0;
  always @(posedge clk) begin
    if (!rst_n) f_sessions <= 4'd0;
    else if (fill_line_done) f_sessions <= f_sessions + 4'd1;
  end
  always @(posedge clk) begin
    if (rst_n) begin
      a_scope_four_sessions: assert(f_sessions <= 4'd4);
    end
  end

  // ---- covers: the never-torn assertion's antecedent is reachable --------
  // (ledger rule V16; added at the merge — the salvaged harness had none.
  //  Without c_read_fresh the whole property could pass with freshness
  //  simply never rising: fill_line_done gated behind assumptions. The
  //  first cover run then caught c_fresh_both UNREACHABLE — the undriven
  //  fill_buf_q hole above.)
  // Trackers, not $past-of-free-inputs: $past(free_input, N) before step N
  // is unconstrained, so a cover written on it is satisfiable by garbage
  // and witnesses nothing.
  reg saw_done0 = 1'b0, saw_abort0 = 1'b0;
  always @(posedge clk) begin
    if (!rst_n) begin
      saw_done0  <= 1'b0;
      saw_abort0 <= 1'b0;
    end else begin
      if (consume_done[0] && started[0]) saw_done0 <= 1'b1;
      if (fill_abort[0])                 saw_abort0 <= 1'b1;
    end
  end
  always @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      c_read_fresh:  cover(buf_fresh[rd_buf]);          // THE antecedent
      c_fresh_both:  cover(buf_fresh[0] && buf_fresh[1]); // ping-pong overlap
      c_consumed:    cover(buf_fresh[0] && consume_start[0]); // display took it
      c_credit:      cover(saw_done0 && buf_empty[0]
                           && !buf_fresh[0]);           // full credit loop
      c_abort_seen:  cover(saw_abort0 && buf_empty[0]
                           && !buf_fresh[0]);           // discard path
      c_consume_after_abort: cover(saw_abort0 &&        // the spacing law
                           consume_start != 2'b00);     // still admits
                                                        // consumption
      c_read_written: cover(started[rd_buf] &&          // THE assert's full
                           rd_addr == f_addr &&
                           f_written[rd_buf]);          // antecedent
    end
  end

endmodule : video_linebuf_fv
