// zhao_terrain_loadq.sv -- the load queue between TERRAIN.SEQ and
// TERRAIN.PAGELOADER.
//
// WHY IT EXISTS, MEASURED RATHER THAN ARGUED
// ------------------------------------------
// TERRAIN.SEQ's law is that a miss is SKIPPED, never waited on. That law holds
// for load COMPLETION and was defeated by load ACCEPTANCE: TERRAIN.PAGELOADER
// drives `j_ready_o = (state == S_IDLE)`, so it accepts one job and holds ready
// LOW for the whole 21,376-byte transfer, and SEQ sat in S_LOAD for all of it.
//
// The composed terrain test measured what that costs: 53,806 clocks for eight
// misses, 6,726 each, with the sequencer blocked -- and every patch that WAS
// resident waiting behind them. This queue accepts the job and lets the
// sequencer walk on. It is the only thing in the chain whose whole purpose is
// to make a `ready` mean "recorded" instead of "finished".
//
// DEPTH IS 32, AND THE FIRST VERSION OF THIS FILE GOT IT WRONG
// ------------------------------------------------------------
// That version was eight deep, and carried a confident paragraph explaining
// why 32 -- T7's per-frame page budget -- was "a budget for a different
// question": the loader still moves one page at a time, so a deeper queue
// supposedly bought nothing but registers.
//
// The bench was then pointed at a LEGAL FULL FRAME and the paragraph did not
// survive contact with it:
//
//     8 misses,  depth 8 :     67 cycles      8 per miss    (803x better)
//    32 misses,  depth 8 : 176,768 cycles  5,524 per miss   (1.2x better)
//
// 10.6% of a 1,666,667-clock frame, with the sequencer stalled on a full queue
// for 176,509 of those cycles. The depth is not covering "the acceptance
// stall", it is covering THE WHOLE FRAME'S MISS LIST, and T7's 32 is exactly
// the number that matters because it is the number of pages a frame may ask
// for. Eight looked sufficient only because the frame that measured it asked
// for eight.
//
// This is the house failure mode in its usual costume: a plausible argument
// that reads like a reason, sitting where a measurement belongs. It survived
// one green test run, because the test run asked the easy question.
//
// WHY THE RECORD LIVES IN AN M10K, AND HOW IT FITS IN ONE
// -------------------------------------------------------
// A job is 243 bits. Thirty-two of them in flops is 7,776 registers, which on
// its own would blow the composed island's 9,000-register redline for a FIFO.
// So the record is SERIALISED into a single M10K instead:
//
//     WW  = 40 bits per word          M10K's widest supported configuration
//     WPJ = 8 words per job           320 bits of room for a 243-bit record
//     32 jobs x 8 words = 256 words   256 x 40 = 10,240 bits = exactly one M10K
//
// Address is `{job_index[4:0], word[2:0]}` -- a plain concatenation, no adder,
// no modulo. Serialising costs eight cycles to write a job and eight to read
// one; the loader spends 6,726 cycles per page, so those sixteen cycles are
// free in a way that is worth stating plainly rather than hoping about.
//
// The array is read with a REGISTERED ADDRESS straight into a REGISTER, with
// nothing combinational in between -- QUARTUS_GOTCHAS section 14: logic between
// the array read and the first register is what stops Quartus absorbing the
// memory, and a "FIFO" that infers 7,776 flops instead of one M10K is the same
// budget disaster wearing the fix's name.
//
// WHAT IT DOES NOT DO
// -------------------
// It does not reorder, does not merge, does not drop, and does not look inside
// a job. T4's writeback-before-load barrier lives in TERRAIN.SEQ and must stay
// there: a queue that could reorder loads relative to writebacks would break a
// barrier it cannot see. This is a FIFO for exactly that reason -- the cheapest
// structure that cannot invent an ordering.

`default_nettype none

module zhao_terrain_loadq #(
    parameter int unsigned DEPTH = 32,  // T7's per-frame page budget, not a guess
    parameter int unsigned SLOTW = 11,
    parameter int unsigned GENW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- from TERRAIN.SEQ ---------------------------------------------------
    input  var logic               j_valid_i,
    output var logic               j_ready_o,
    input  var logic [SLOTW-1:0]   j_slot_i,
    input  var logic [GENW-1:0]    j_gen_i,
    input  var logic [31:0]        j_epoch_i,
    input  var logic [31:0]        j_island_i,
    input  var logic signed [15:0] j_ix_i,
    input  var logic signed [15:0] j_iz_i,
    input  var logic [63:0]        j_hps_addr_i,
    input  var logic [31:0]        j_expect_crc_i,
    input  var logic [31:0]        j_src_id_i,

    // ---- to TERRAIN.PAGELOADER ----------------------------------------------
    output var logic               q_valid_o,
    input  var logic               q_ready_i,
    output var logic [SLOTW-1:0]   q_slot_o,
    output var logic [GENW-1:0]    q_gen_o,
    output var logic [31:0]        q_epoch_o,
    output var logic [31:0]        q_island_o,
    output var logic signed [15:0] q_ix_o,
    output var logic signed [15:0] q_iz_o,
    output var logic [63:0]        q_hps_addr_o,
    output var logic [31:0]        q_expect_crc_o,
    output var logic [31:0]        q_src_id_o,

    // ---- drain --------------------------------------------------------------
    // A T6 frame fault abandons the rest of the list, and jobs already queued
    // belong to that list. Draining is explicit rather than a reset so the
    // count of what was thrown away survives. Whether a fault SHOULD abandon
    // them is an owner ruling nobody has made; this port is the mechanism, not
    // the policy, and nothing in the tree asserts it yet.
    input var logic drain_i,

    // ---- counters -----------------------------------------------------------
    output var logic [31:0] accepted_o,   // jobs taken off the sequencer
    output var logic [31:0] issued_o,     // ...handed on to the loader
    output var logic [31:0] drained_o,    // ...thrown away on a drain
    output var logic [31:0] refused_o,    // CYCLES offered while it could not take one
    output var logic [31:0] level_o,      // jobs committed to the store right now
    // EVERYTHING THE BLOCK IS HOLDING, which is not the same number. A job
    // spends eight cycles in the write serialiser before it is committed and
    // sits in the output register after it leaves the store, so `level_o`
    // alone can read zero while two jobs are still inside. Anything asking
    // "is this queue done?" -- a settle, a drain's tally, a frame boundary --
    // wants THIS one; `level_o` is for judging DEPTH and nothing else.
    output var logic [31:0] inflight_o,
    output var logic [31:0] high_water_o  // the deepest the store ever got
);

  // ---- the shapes ---------------------------------------------------------
  localparam int unsigned WW   = 40;                 // M10K's widest word
  localparam int unsigned WPJ  = 8;                  // words per job
  localparam int unsigned RECW = SLOTW + GENW + 32 + 32 + 16 + 16 + 64 + 32 + 32;
  localparam int unsigned PADW = (WPJ * WW) - RECW;  // 320 - 243 = 77 with the defaults

  localparam int unsigned PTRW = $clog2(DEPTH);
  localparam int unsigned CNTW = $clog2(DEPTH + 1);
  localparam int unsigned WSEL = $clog2(WPJ);
  localparam int unsigned AW   = PTRW + WSEL;

  // A compile-time refusal rather than a truncation at 3 a.m. Widening a field
  // past the room in eight words must be a build error, not a job that arrives
  // at the loader with its CRC sheared off.
  //
  // IT LIVES IN AN `initial`, NOT IN A BARE GENERATE-IF, and the first version
  // did the latter. Verilator accepted it; Quartus 17.0.2 did not, and the fit
  // came back `incomplete:failed:quartus_map` in 57 seconds with
  //
  //     Error (10170): Verilog HDL syntax error ... near text: "if";
  //                    expecting "endmodule"
  //
  // A guard that stops the synthesiser reading the file is worse than no guard:
  // it fails the build it was meant to protect, for a condition that is not
  // true. The simulator's elaboration is where this check belongs, and the lint
  // gate runs on every build.
`ifndef SYNTHESIS
  initial begin
    if (RECW > WPJ * WW)
      $fatal(1, "zhao_terrain_loadq: record %0d bits is wider than WPJ*WW = %0d",
             RECW, WPJ * WW);
  end
`endif

  logic [WW-1:0] mem_q [DEPTH * WPJ];

  // ---- the write side -----------------------------------------------------
  logic [RECW-1:0]  wrec_q;
  logic [WSEL-1:0]  wword_q;
  logic             wbusy_q;
  logic [PTRW-1:0]  wp_q;

  // ---- the read side ------------------------------------------------------
  // FULL WORD WIDTH, not RECW. The deserialiser writes whole 40-bit words, and
  // the last one runs past the record's 243 bits into the pad. A vector sized
  // to RECW would make that final part-select fall off the end. The pad bits
  // are written and never read, which is the one thing here that genuinely IS
  // unused rather than accidentally so.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [(WPJ*WW)-1:0] orec_q;   // the fully deserialised job the loader sees
  /* verilator lint_on UNUSEDSIGNAL */
  logic             ovalid_q;
  logic [WSEL-1:0]  rword_q;
  logic             rbusy_q;
  logic [PTRW-1:0]  rp_q;
  logic [WW-1:0]    rdata_q;
  logic             rdvalid_q;   // rdata_q holds the word raddr_q named
  logic [WSEL-1:0]  rdword_q;    // ...and which word of the job it is

  logic [CNTW-1:0]  lvl_q;
  logic [CNTW-1:0]  hw_q;

  wire full_c  = (lvl_q == CNTW'(DEPTH));
  wire empty_c = (lvl_q == CNTW'(0));

  // READY MEANS "I CAN START ONE NOW", not "I have room somewhere". The write
  // serialiser is busy for WPJ cycles per job, so a ready that ignored it would
  // accept a second job on top of the one being written.
  assign j_ready_o = !full_c && !wbusy_q;
  assign q_valid_o = ovalid_q;

  assign level_o      = {{(32-CNTW){1'b0}}, lvl_q};
  assign inflight_o   = {{(32-CNTW){1'b0}}, lvl_q}
                        + (wbusy_q  ? 32'd1 : 32'd0)
                        + (ovalid_q ? 32'd1 : 32'd0);
  assign high_water_o = {{(32-CNTW){1'b0}}, hw_q};

  wire push_c = j_valid_i && j_ready_o;
  wire pop_c  = ovalid_q && q_ready_i;

  wire [RECW-1:0] in_rec_c = {j_slot_i, j_gen_i, j_epoch_i, j_island_i,
                              j_ix_i, j_iz_i, j_hps_addr_i, j_expect_crc_i,
                              j_src_id_i};

  assign {q_slot_o, q_gen_o, q_epoch_o, q_island_o,
          q_ix_o, q_iz_o, q_hps_addr_o, q_expect_crc_o,
          q_src_id_o} = orec_q[RECW-1:0];

  // The record, padded to WPJ words, as a flat vector the word select indexes.
  // Two of them: the captured copy for words 1..WPJ-1, and the LIVE record for
  // word 0, which is stored on the acceptance cycle itself. Indexing a
  // concatenation directly is not legal, so both get a name.
  wire [(WPJ*WW)-1:0] wflat_c = {{PADW{1'b0}}, wrec_q};
  // Word 0 only, so no padding is involved: the record is 243 bits and the
  // word is 40, so the low word is always entirely record.
  wire [WW-1:0] in_word0_c = in_rec_c[0 +: WW];

  // Start a read whenever the store holds a job and the output register is
  // free. `rbusy_q` covers the WPJ cycles the deserialiser needs.
  wire rd_start_c = !rbusy_q && !ovalid_q && !empty_c;

  logic [AW-1:0]   waddr_c;
  logic [WW-1:0]   wdata_c;
  logic            wen_c;
  logic [AW-1:0]   raddr_c;
  logic            ren_c;
  logic [WSEL-1:0] rword_c;

  always_comb begin
    // The write serialiser sources its first word from the incoming record on
    // the acceptance cycle and from the captured copy after that -- otherwise
    // word 0 would need a whole extra cycle before it could be stored.
    wen_c   = push_c || wbusy_q;
    waddr_c = push_c ? {wp_q, {WSEL{1'b0}}} : {wp_q, wword_q};
    wdata_c = push_c ? in_word0_c
                     : wflat_c[(32'(wword_q) * 32'(WW)) +: WW];

    ren_c   = rd_start_c || rbusy_q;
    rword_c = rd_start_c ? {WSEL{1'b0}} : rword_q;
    raddr_c = {rp_q, rword_c};
  end

  // =========================================================================
  // THE STORE. One always_ff, registered address in, registered data out, and
  // nothing combinational between the array and either register.
  // =========================================================================
  always_ff @(posedge clk) begin
    if (wen_c) mem_q[waddr_c] <= wdata_c;
    rdata_q  <= mem_q[raddr_c];
    rdword_q <= rword_c;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wrec_q     <= '0;
      wword_q    <= '0;
      wbusy_q    <= 1'b0;
      wp_q       <= '0;
      orec_q     <= '0;
      ovalid_q   <= 1'b0;
      rword_q    <= '0;
      rbusy_q    <= 1'b0;
      rp_q       <= '0;
      rdvalid_q  <= 1'b0;
      lvl_q      <= '0;
      hw_q       <= '0;
      accepted_o <= 32'd0;
      issued_o   <= 32'd0;
      drained_o  <= 32'd0;
      refused_o  <= 32'd0;
    end else if (drain_i) begin
      // EVERYTHING IN FLIGHT GOES, AND ALL OF IT IS COUNTED. A drain that
      // silently empties cannot be told from a queue that was never filled,
      // and the T6 fault path is exactly where somebody will later ask "did
      // those loads happen?". The three places a job can be sitting are the
      // store, the write serialiser and the output register, so all three are
      // in the tally -- a drain that counted only the store would under-report
      // by up to two every time.
      drained_o <= drained_o + 32'({{(32-CNTW){1'b0}}, lvl_q})
                             + (wbusy_q  ? 32'd1 : 32'd0)
                             + (ovalid_q ? 32'd1 : 32'd0);
      wp_q      <= '0;
      rp_q      <= '0;
      lvl_q     <= '0;
      wbusy_q   <= 1'b0;
      wword_q   <= '0;
      rbusy_q   <= 1'b0;
      rword_q   <= '0;
      rdvalid_q <= 1'b0;
      ovalid_q  <= 1'b0;
    end else begin
      rdvalid_q <= ren_c;

      // ---- offered while it could not take one --------------------------
      // COUNTED IN CYCLES, not in jobs, and the name of the counter is the
      // only place that can say so. A blocked sequencer holds `j_valid_i` up
      // for as long as it is blocked, so this number IS the stall, which is
      // the thing worth reading -- 176,509 of a frame's 1,666,667 cycles at
      // the wrong depth.
      if (j_valid_i && !j_ready_o) refused_o <= refused_o + 32'd1;

      // ---- write side ----------------------------------------------------
      if (push_c) begin
        wrec_q     <= in_rec_c;
        wword_q    <= WSEL'(1);          // word 0 went in on this very cycle
        wbusy_q    <= 1'b1;
        accepted_o <= accepted_o + 32'd1;
      end else if (wbusy_q) begin
        if (wword_q == WSEL'(WPJ - 1)) begin
          // The last word lands this cycle, so the job is committed now.
          wbusy_q <= 1'b0;
          wword_q <= '0;
          wp_q    <= (wp_q == PTRW'(DEPTH - 1)) ? '0 : wp_q + PTRW'(1);
        end else begin
          wword_q <= wword_q + WSEL'(1);
        end
      end

      // ---- read side -----------------------------------------------------
      if (rd_start_c) begin
        rbusy_q <= 1'b1;
        rword_q <= WSEL'(1);
      end else if (rbusy_q) begin
        if (rword_q != WSEL'(WPJ - 1)) rword_q <= rword_q + WSEL'(1);
      end

      // Words arrive one cycle behind the address. `rdword_q` says which one,
      // so the deserialiser never has to reason about the pipeline's depth.
      if (rdvalid_q && rbusy_q) begin
        orec_q[(32'(rdword_q) * 32'(WW)) +: WW] <= rdata_q;
        if (rdword_q == WSEL'(WPJ - 1)) begin
          rbusy_q  <= 1'b0;
          rword_q  <= '0;
          ovalid_q <= 1'b1;
          rp_q     <= (rp_q == PTRW'(DEPTH - 1)) ? '0 : rp_q + PTRW'(1);
        end
      end

      if (pop_c) begin
        ovalid_q <= 1'b0;
        issued_o <= issued_o + 32'd1;
      end

      // ---- the level, in JOBS --------------------------------------------
      // A job joins the store when its LAST word is written and leaves when
      // the deserialiser has taken its last word -- not on the handshakes,
      // which are eight cycles away from both.
      unique case ({(wbusy_q && (wword_q == WSEL'(WPJ - 1))),
                    (rdvalid_q && rbusy_q && (rdword_q == WSEL'(WPJ - 1)))})
        2'b10:   lvl_q <= lvl_q + CNTW'(1);
        2'b01:   lvl_q <= lvl_q - CNTW'(1);
        default: lvl_q <= lvl_q;
      endcase

      if ((wbusy_q && (wword_q == WSEL'(WPJ - 1)))
          && !(rdvalid_q && rbusy_q && (rdword_q == WSEL'(WPJ - 1)))
          && ((lvl_q + CNTW'(1)) > hw_q))
        hw_q <= lvl_q + CNTW'(1);
    end
  end

`ifndef SYNTHESIS
  // ENFORCED-BY: tests/terrain/world_composed_directed.cpp phase L
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_level_bounded :
      assert (lvl_q <= CNTW'(DEPTH))
      else $error("loadq: level %0d exceeds depth %0d", lvl_q, DEPTH);

      a_no_push_when_full :
      assert (!(push_c && full_c))
      else $error("loadq: accepted a job with a full store");

      a_no_push_when_writing :
      assert (!(push_c && wbusy_q))
      else $error("loadq: accepted a job while still serialising the last one");

      a_no_pop_when_empty :
      assert (!(pop_c && !ovalid_q))
      else $error("loadq: popped with no job in the output register");
    end
  end
`endif

endmodule
