// zhao_texture_material_combine_v1.sv — MATERIAL.COMBINE.V1
// Authored 2026-09-05 (roadmap G1-C), replacing the refuted II=1 combiner.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS AND WHAT IT REPLACES
// ---------------------------------------------------------------------------
// `zhao_texture_combine.sv` implemented the CONTRACT's II=1 with eight
// independent `unit_mul` sites. It fit at 494 ALM / 524 reg / 100.12 MHz and
// **8 DSP blocks** against a rule of 2 -- one DSP per inferred `*`.
// `islandrearchitecture5.md` §15.5 closes with the instruction it violated,
// verbatim:
//
//   > **Do not write six independent `*` operators and assume they pack.**
//
// and §15.2 gives the reason it is wrong on its own terms rather than merely
// expensive: **the TMU supplies at most one sample per clock**, so a two-sample
// recipe cannot retire faster than one fragment per two sample clocks however
// many multipliers stand ready. The parallel form buys throughput the sample
// supply cannot use, and pays for it in silicon.
//
// The DSP rule was the architecture's (§3.4 "reject DSP > 2"), and
// `design/fit_targets.yml` recorded the response to a firing BEFORE the fit
// ran, so raising the rule to fit the measurement was never available. Docket
// D19q; evidence in reports/G1C-COMBINER-II1-REFUTED-20260905.md.
//
// ---------------------------------------------------------------------------
// THE SHAPE (§15.3, §15.5-A, §15.6)
// ---------------------------------------------------------------------------
//   * a small record file (depth 8, §15.3 "Depth 8 is sufficient initially")
//     holding recipe, weight, three samples, intermediates, a REQUIRED microjob
//     mask and a COMPLETED microjob mask;
//   * TWO product lanes, variant A `LOGIC2` -- exact multipliers built in ALM
//     logic, ZERO DSP, via `multstyle = "logic"`. Two operators, attributed;
//     not eight, assumed;
//   * tokenized microjobs. Continuations are enqueued when their first-layer
//     result lands -- "no table-wide scan" -- by testing one dependency mask;
//   * PASSTHRU and ADD_SAT are FAST BYPASSES that never touch a lane (§15.3).
//
// Fragments may retire OUT OF ALLOCATION ORDER (§15.6): a PASSTHRU behind a
// DETAIL_LIGHT is done on arrival. FRAGROB performs final ordered retirement,
// so this block must NOT reorder into program order itself -- doing so would
// stall a fast fragment behind a slow one for no benefit, which is the
// "slow head fragment idling the combiner" §15.3 names.
//
// ---------------------------------------------------------------------------
// MICROJOB NUMBERING
// ---------------------------------------------------------------------------
//   0,1,2  first-layer product on R,G,B
//   3      the alpha product
//   4,5,6  second-layer product on R,G,B  (DETAIL_LIGHT only)
//
// Second-layer jobs depend on their own channel's first-layer result, which is
// the whole reason the record carries a completed mask rather than a counter:
// job 4 is issuable exactly when job 0 has landed.
//
// ---------------------------------------------------------------------------
// ONE DISCREPANCY, FLAGGED RATHER THAN RECONCILED
// ---------------------------------------------------------------------------
// §15.3's job table lists `MASK  1 alpha product`. But MASK's ratified
// arithmetic, in `design/contracts/TEXTURE.COMBINE.md` and in the oracle
// `zref::material::combine`, is a SELECT -- "s0 where s1 passes, else
// transparent", the pass test being s1's alpha being non-zero. A select needs
// no multiplier.
//
// This implements the ratified arithmetic (the oracle is what it is
// differentiated against) and therefore issues ZERO jobs for MASK. The
// per-recipe counter below reports what the hardware ACTUALLY issued, per
// §15.4's "Counters must record actual product jobs by recipe", so the
// difference from §15.3's table is visible in a trace instead of buried. Either
// §15.3 anticipates a different MASK, or its table is one row wrong; that is an
// owner question and not one to settle by writing a multiply nothing needs.

// ---------------------------------------------------------------------------
// RECORD DEPTH IS 2, NOT §15.3's 8, AND THE REASON IS A BUDGET CONFLICT
// ---------------------------------------------------------------------------
// §15.3 says "Depth 8 is sufficient initially". §3.3's budget for this block
// says **500 registers and 1-4 M10K**. Those two are in tension if the record
// file is flops: a record here is about 175 bits, so eight of them is roughly
// 1,400 flip-flops -- nearly three times the register budget on its own.
//
// The M10K line in §3.3 is what resolves it: at depth 8 the record PAYLOAD
// (three samples, weight, tag) belongs in memory, with only the valid bit and
// the two 7-bit masks in flops so the scheduler can still scan them
// combinationally -- 15 bits per record instead of 175. That is a different
// block, with a memory read in the operand path and a cycle of latency it must
// hide.
//
// This build is the LOGIC2 arithmetic at a depth that fits flops, so the DSP
// question -- the one the refuted block failed and the one this rewrite exists
// to answer -- gets measured now rather than after a memory redesign. Depth is
// a parameter precisely so raising it is a fit, not a rewrite. What depth 2
// costs is buffering, not correctness: §15.2's own argument is that the TMU
// supplies one sample per clock, so a two-sample recipe cannot retire faster
// than one per two clocks whatever the queue holds.
module zhao_texture_material_combine_v1 #(
    parameter int RECS = 2
) (
    input  logic        clk,
    input  logic        rst_n,

    // ---- fragment in -------------------------------------------------------
    input  logic        f_valid_i,
    output logic        f_ready_o,
    input  logic [1:0]  f_sample_count_i,   // 0..3
    input  logic [2:0]  f_recipe_i,         // 0..7; anything else is refused
    input  logic [7:0]  f_weight_i,         // unit8, LERP only
    input  logic [23:0] f_s0_rgb_i,
    input  logic [7:0]  f_s0_a_i,
    input  logic [23:0] f_s1_rgb_i,
    input  logic [7:0]  f_s1_a_i,
    input  logic [23:0] f_s2_rgb_i,         // NEW in v1: the third sample
    input  logic [7:0]  f_s2_a_i,
    // The untextured colour, used when sample_count == 0. Named `base` to match
    // the oracle: TEXTURE.COMBINE.md says "the fragment's vertex colour" but
    // its packet list has no such field, so the binding is made by the CALLER
    // rather than guessed at here.
    input  logic [23:0] f_base_rgb_i,
    input  logic [7:0]  f_base_a_i,
    input  logic [15:0] f_tag_i,

    // ---- fragment out ------------------------------------------------------
    output logic        o_valid_o,
    input  logic        o_ready_i,
    output logic [23:0] o_rgb_o,
    output logic [7:0]  o_a_o,
    output logic [15:0] o_tag_o,
    output logic        o_refused_o,

    // ---- counters ----------------------------------------------------------
    output logic [31:0] refused_recipe_o,
    output logic [31:0] refused_missing_o,
    output logic [31:0] saturated_add_o,
    output logic [31:0] saturated_mul2x_o,
    // §15.4: actual product jobs ISSUED, by recipe. Eight counters, so the
    // 80%-capacity question is answered from a trace rather than from
    // arithmetic over an assumed recipe mix.
    output logic [31:0] jobs_by_recipe_o [8]
);

  // ---- recipe encodings (§15.1) --------------------------------------------
  localparam logic [2:0] R_PASSTHRU    = 3'd0;
  localparam logic [2:0] R_MODULATE    = 3'd1;
  localparam logic [2:0] R_MODULATE2X  = 3'd2;
  localparam logic [2:0] R_LERP        = 3'd3;
  localparam logic [2:0] R_ADD_SAT     = 3'd4;
  localparam logic [2:0] R_MASK        = 3'd5;
  localparam logic [2:0] R_DETAIL_LIGHT= 3'd6;
  localparam logic [2:0] R_DETAIL_MASK = 3'd7;

  // ==========================================================================
  // The two LOGIC2 product lanes (§15.5 variant A)
  // ==========================================================================
  // `multstyle = "logic"` is the whole point of this block. Without it these
  // two `*` operators become two DSP blocks and the exercise has bought
  // nothing; with it they are exact multipliers in ALM logic and the block
  // uses ZERO DSP, which is variant A's stated advantage over B.
  //
  // The arithmetic is the frozen `((a*b) + 128) >> 8`, clamp 255 -- the same
  // law as `zref::unit_mul` and as the refuted block. It is restated here
  // because RTL cannot call the C++ oracle, and the differential test exists
  // precisely because a restatement is a risk.
  function automatic logic [7:0] unit_mul_logic(input logic [7:0] a,
                                                input logic [7:0] b);
    /* verilator lint_off UNUSEDSIGNAL */
    (* multstyle = "logic" *) logic [16:0] p;
    /* verilator lint_on UNUSEDSIGNAL */
    begin
      p = ({9'd0, a} * {9'd0, b}) + 17'd128;
      unit_mul_logic = (p[16:8] > 9'd255) ? 8'd255 : p[15:8];
    end
  endfunction

  // ==========================================================================
  // The record file
  // ==========================================================================
  typedef struct packed {
    logic        valid;
    logic        refused;
    logic [2:0]  recipe;
    logic [1:0]  count;
    logic [7:0]  weight;
    logic [15:0] tag;
    logic [7:0]  s0_r, s0_g, s0_b, s0_a;
    logic [7:0]  s1_r, s1_g, s1_b, s1_a;
    logic [7:0]  s2_r, s2_g, s2_b, s2_a;
    logic [7:0]  o_r,  o_g,  o_b,  o_a;   // intermediates, then the result
    logic [6:0]  req;                      // required microjobs
    logic [6:0]  done;                     // completed microjobs
    // IN FLIGHT. A job issued this cycle does not set `done` until its lane
    // result lands two cycles later, so without this the scheduler sees it as
    // still pending and ISSUES IT AGAIN, every cycle, until the first result
    // arrives. The block did exactly that: DETAIL_LIGHT was issuing about
    // twelve jobs per fragment instead of six.
    //
    // The results still matched the oracle throughout, because re-computing
    // the same product is idempotent and the second-layer dependency gate
    // happened to keep the destructive case out. Only the per-recipe job
    // counter showed it -- and only after the counter's OWN double-issue bug
    // was fixed, which is the one that made 2,398 jobs read as 1,199.
    logic [6:0]  busy;                     // issued, result not yet back
    // MODULATE2X's doubling now happens at write-back, so its saturation is
    // a per-CHANNEL event arriving over several cycles. The oracle counts it
    // ONCE PER FRAGMENT, so the flag is latched here and the counter fires
    // at retirement. Counting at write-back instead would report up to four
    // saturations for one fragment and quietly disagree with the oracle.
    logic        sat2x;
  } rec_t;

  rec_t rec [RECS];

  // How many samples each recipe REQUIRES (S15.1). Written as a CASE and not
  // with SystemVerilog's `inside` operator: **Quartus 17.0.2 does not support
  // `inside`** and fails Analysis & Synthesis with "syntax error ... near text:
  // \"inside\"; expecting )". Verilator accepts it happily, so a clean lint says
  // nothing about whether the synthesiser will take it -- this was found only
  // by launching the fit.
  function automatic logic [1:0] samples_required(input logic [2:0] r);
    begin
      case (r)
        R_PASSTHRU:                    samples_required = 2'd1;
        R_DETAIL_LIGHT, R_DETAIL_MASK: samples_required = 2'd3;
        default:                       samples_required = 2'd2;
      endcase
    end
  endfunction

  // ---- required-job mask per recipe ----------------------------------------
  // The table §15.3 gives, as a mask. PASSTHRU and ADD_SAT are bypasses; MASK
  // is a select (see the header discrepancy note); LERP's three products are
  // difference-by-weight and use the same first-layer slots.
  function automatic logic [6:0] req_mask(input logic [2:0] r);
    begin
      case (r)
        // FOUR jobs, not three. S15.3's table says "MODULATE 3 RGB products"
        // and counts no alpha -- but the ratified arithmetic multiplies alpha
        // too, so the alpha product exists whatever the table says. Computing
        // it at acceptance instead of as a job is what put a THIRD multiplier
        // in the block; making it a job puts it through a lane. The per-recipe
        // counter therefore reports 4 where S15.3 says 3, which is S15.4's
        // "actual product jobs" doing its job.
        R_MODULATE,
        R_MODULATE2X,
        R_LERP:          req_mask = 7'b000_1111;  // three RGB + alpha
        R_DETAIL_MASK:   req_mask = 7'b000_1111;  // three first-layer + alpha
        R_DETAIL_LIGHT:  req_mask = 7'b111_0111;  // + three second-layer
        default:         req_mask = 7'b000_0000;  // PASSTHRU, ADD_SAT, MASK
      endcase
    end
  endfunction

  // A job is ISSUABLE when it is required, not done, and its dependency has
  // landed. Only the second-layer jobs have a dependency, and each depends on
  // exactly its own channel -- which is what lets a continuation be enqueued
  // "after first-layer results" without a table-wide scan.
  function automatic logic [6:0] issuable(input logic [6:0] req,
                                          input logic [6:0] done,
                                          input logic [6:0] busy);
    logic [6:0] pend;
    begin
      pend = req & ~done & ~busy;
      // jobs 4,5,6 gate on jobs 0,1,2 respectively
      pend[4] = pend[4] & done[0];
      pend[5] = pend[5] & done[1];
      pend[6] = pend[6] & done[2];
      issuable = pend;
    end
  endfunction

  // ---- saturating helpers (bypass paths) -----------------------------------
  // {overflow, value}: the caller needs BOTH, and returning only the clamped
  // value is how a saturation stops being counted.
  function automatic logic [8:0] add_sat9(input logic [7:0] a, input logic [7:0] b);
    logic [8:0] s;
    begin
      s = {1'b0, a} + {1'b0, b};
      add_sat9 = s[8] ? 9'b1_11111111 : {1'b0, s[7:0]};
    end
  endfunction


  // ==========================================================================
  // Allocation (C0)
  // ==========================================================================
  logic [$clog2(RECS)-1:0] alloc_slot;
  logic                    have_free;

  always_comb begin
    have_free  = 1'b0;
    alloc_slot = '0;
    for (int i = RECS-1; i >= 0; i--)
      if (!rec[i].valid) begin
        have_free  = 1'b1;
        alloc_slot = ($clog2(RECS))'(i);
      end
  end

  assign f_ready_o = have_free;

  // ==========================================================================
  // Scheduling (C1/C2/C3): up to two jobs per clock, oldest-first
  // ==========================================================================
  // "Oldest-first" here means lowest slot index with work, which is a fixed
  // priority rather than a true age order. That is deliberate at depth 8: a
  // rotating arbiter costs logic to solve a fairness problem that cannot
  // starve anything, because every record retires in a bounded number of
  // cycles and its slot is then free.
  typedef struct packed {
    logic                    valid;
    logic [$clog2(RECS)-1:0] slot;
    logic [2:0]              job;
    logic [7:0]              a, b;
  } job_t;

  job_t lane0_q, lane1_q;

  /* verilator lint_off UNUSEDSIGNAL */
  // `r` is passed whole and only the two named channels are read; `which`/`ch`
  // are ints for call-site readability and only their low two bits select.
  // Both are deliberate, so the warning is suppressed HERE rather than at the
  // file level, where it would also hide a genuinely dropped signal.
  function automatic logic [7:0] chan_of(input rec_t r, input int which,
                                         input int ch);
    begin
      case ({which[1:0], ch[1:0]})
        4'b00_00: chan_of = r.s0_r;  4'b00_01: chan_of = r.s0_g;
        4'b00_10: chan_of = r.s0_b;  4'b00_11: chan_of = r.s0_a;
        4'b01_00: chan_of = r.s1_r;  4'b01_01: chan_of = r.s1_g;
        4'b01_10: chan_of = r.s1_b;  4'b01_11: chan_of = r.s1_a;
        4'b10_00: chan_of = r.s2_r;  4'b10_01: chan_of = r.s2_g;
        4'b10_10: chan_of = r.s2_b;  4'b10_11: chan_of = r.s2_a;
        4'b11_00: chan_of = r.o_r;   4'b11_01: chan_of = r.o_g;
        4'b11_10: chan_of = r.o_b;
        default:  chan_of = r.o_a;
      endcase
    end
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  // The operands for one microjob. First-layer jobs read s0 x s1; the alpha job
  // reads whichever pair the recipe's mask semantics name; second-layer jobs
  // read the stored intermediate x s2.
  function automatic logic [15:0] operands(input rec_t r, input logic [2:0] job);
    logic [7:0] a, b;
    begin
      if (job <= 3'd2) begin                       // first layer, R/G/B
        if (r.recipe == R_LERP) begin
          // §15.3: "LERP  3 signed difference x weight products". The lane's
          // frozen law is an UNSIGNED product, so the job carries the
          // MAGNITUDE |s1 - s0| times the weight, and the sign is reapplied at
          // write-back. Rounding a magnitude and then reapplying the sign is
          // symmetric about zero; multiplying a signed difference and rounding
          // half-up would bias every darkening lerp toward +inf, which is the
          // exact asymmetry the oracle's lerp8 comment records.
          a = (chan_of(r, 1, int'(job)) >= chan_of(r, 0, int'(job)))
                ? (chan_of(r, 1, int'(job)) - chan_of(r, 0, int'(job)))
                : (chan_of(r, 0, int'(job)) - chan_of(r, 1, int'(job)));
          b = r.weight;
        end else begin
          a = chan_of(r, 0, int'(job));
          b = chan_of(r, 1, int'(job));
        end
      end else if (job == 3'd3) begin              // the alpha product
        if (r.recipe == R_LERP) begin
          // LERP's alpha is the same magnitude-by-weight form as its RGB.
          a = (r.s1_a >= r.s0_a) ? (r.s1_a - r.s0_a) : (r.s0_a - r.s1_a);
          b = r.weight;
        end else begin
          a = r.s0_a;
          b = (r.recipe == R_DETAIL_MASK) ? r.s2_a : r.s1_a;
        end
      end else begin                               // second layer, R/G/B
        a = chan_of(r, 3, int'(job) - 4);          // the intermediate
        b = chan_of(r, 2, int'(job) - 4);          // s2
      end
      operands = {a, b};
    end
  endfunction

  // What a lane result becomes when it lands. For every recipe but LERP the
  // product IS the answer; LERP's product is a magnitude, so the sign is
  // reapplied here against sample 0 and the sum is clamped.
  function automatic logic [7:0] writeback_val(input rec_t r,
                                               input logic [2:0] job,
                                               input logic [7:0] prod);
    logic [7:0] s0c, s1c;
    logic signed [10:0] v;
    begin
      if (r.recipe == R_MODULATE2X) begin
        // The doubling and saturation live HERE, not in the lane: the lane's
        // law is the frozen unit8 product and must stay one law. Doubling
        // AFTER that product keeps a single rounding, which is what the oracle
        // does and what a second rounding would drift from by an LSB.
        v = $signed({2'b0, prod}) <<< 1;
        writeback_val = (v > 11'sd255) ? 8'd255 : v[7:0];
      end else if (r.recipe == R_LERP && job <= 3'd3) begin
        s0c = chan_of(r, 0, (job == 3'd3) ? 3 : int'(job));
        s1c = chan_of(r, 1, (job == 3'd3) ? 3 : int'(job));
        v = (s1c >= s0c) ? ($signed({3'b0, s0c}) + $signed({3'b0, prod}))
                         : ($signed({3'b0, s0c}) - $signed({3'b0, prod}));
        writeback_val = (v < 0) ? 8'd0 : ((v > 11'sd255) ? 8'd255 : v[7:0]);
      end else begin
        writeback_val = prod;
      end
    end
  endfunction


  // ==========================================================================
  // Retirement (C4/C5)
  // ==========================================================================
  logic [$clog2(RECS)-1:0] retire_slot;
  logic                    have_retire;

  always_comb begin
    have_retire = 1'b0;
    retire_slot = '0;
    for (int i = RECS-1; i >= 0; i--)
      if (rec[i].valid && (rec[i].done == rec[i].req)) begin
        have_retire = 1'b1;
        retire_slot = ($clog2(RECS))'(i);
      end
  end

  assign o_valid_o   = have_retire;
  assign o_rgb_o     = {rec[retire_slot].o_r, rec[retire_slot].o_g,
                        rec[retire_slot].o_b};
  assign o_a_o       = rec[retire_slot].o_a;
  assign o_tag_o     = rec[retire_slot].tag;
  assign o_refused_o = rec[retire_slot].refused;

  // ==========================================================================
  // Sequential
  // ==========================================================================
  logic [31:0] jobs_by_recipe_r [8];
  assign jobs_by_recipe_o = jobs_by_recipe_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < RECS; i++) rec[i] <= '0;
      for (int i = 0; i < 8; i++)    jobs_by_recipe_r[i] <= '0;
      lane0_q <= '0;
      lane1_q <= '0;
      refused_recipe_o  <= '0;
      refused_missing_o <= '0;
      saturated_add_o   <= '0;
      saturated_mul2x_o <= '0;
    end else begin
      automatic logic [6:0] pend0;
      automatic int         issued;
      automatic logic [8:0] t;
      automatic logic       sat_add;
      automatic logic [15:0] ops;
      // TWO BITS, NOT THIRTY-TWO, and this is the block's critical path.
      //
      // The fit measured 29.74 MHz with the worst path running
      // `Decoder5~9 -> Add46~41`, slack -23.624 ns. The datapath here is 7 to 9
      // bits wide and cannot produce a 23 ns path; the only 32-bit signals in
      // the block are these counters. `jobs_inc[rec[i].recipe] += 1` is a
      // BLOCKING add on a DECODED index inside a nested loop over RECS x 7
      // jobs, so synthesis must serialise up to fourteen conditional 32-bit
      // adds into one carry chain -- decode, select, add, repeat.
      //
      // At most TWO jobs issue per cycle (two lanes), so the per-cycle
      // increment never exceeds 2 and needs two bits. The chain becomes 2-bit
      // adds and only the commit below is 32-bit, where the eight adds are
      // independent and parallel rather than chained.
      //
      // The irony is worth keeping: these are the §15.4 counters that caught
      // the double-issue bug this morning, and they are why the block does not
      // close timing.
      automatic logic [1:0] jobs_inc [8];
      automatic logic [7:0]  prod0, prod1, wb0, wb1;

      // ---- lane results write back (C2: capture, round, enqueue) -----------
      // The rounding happened in the lane; here the result lands in the record
      // and its `done` bit is what makes any dependent continuation issuable
      // on the very next cycle. That IS the "enqueue continuation" step -- no
      // separate queue exists, because one bit per job is cheaper than a queue
      // and expresses exactly the same dependency.
      if (lane0_q.valid) begin
        // ONE PRODUCT PER LANE. The case selects a DESTINATION; it must not
        // contain the multiply. Written the other way round -- a
        // `unit_mul_logic(...)` inside each of seven arms -- it is seven
        // independent `*` operators per lane, fourteen in the block, and the
        // fitter gave them 7 DSP blocks. That is EXACTLY the failure S15.5
        // closes with: "Do not write six independent `*` operators and assume
        // they pack." Having quoted that line in this file's own header while
        // writing fourteen of them is the reason it is quoted again here.
        prod0 = unit_mul_logic(lane0_q.a, lane0_q.b);
        wb0   = writeback_val(rec[lane0_q.slot], lane0_q.job, prod0);
        case (lane0_q.job)
          3'd0: rec[lane0_q.slot].o_r <= wb0;
          3'd1: rec[lane0_q.slot].o_g <= wb0;
          3'd2: rec[lane0_q.slot].o_b <= wb0;
          // EVERY arm takes `wb`, not `prod`. Job 3 taking the raw product
          // was a real bug: MODULATE2X's doubling lives in writeback_val, so
          // its ALPHA came out half -- 17 where the oracle said 34 -- while
          // its RGB was right, which is the kind of asymmetry that reads as a
          // channel-ordering mistake. writeback_val returns the product
          // unchanged for every recipe that wants it unchanged, so there is
          // no case where `prod` is the correct thing to store.
          3'd3: rec[lane0_q.slot].o_a <= wb0;
          3'd4: rec[lane0_q.slot].o_r <= wb0;
          3'd5: rec[lane0_q.slot].o_g <= wb0;
          default: rec[lane0_q.slot].o_b <= wb0;
        endcase
        rec[lane0_q.slot].done[lane0_q.job] <= 1'b1;
        rec[lane0_q.slot].busy[lane0_q.job] <= 1'b0;
        if (rec[lane0_q.slot].recipe == R_MODULATE2X && prod0 > 8'd127)
          rec[lane0_q.slot].sat2x <= 1'b1;
      end
      if (lane1_q.valid) begin
        // ONE PRODUCT PER LANE. The case selects a DESTINATION; it must not
        // contain the multiply. Written the other way round -- a
        // `unit_mul_logic(...)` inside each of seven arms -- it is seven
        // independent `*` operators per lane, fourteen in the block, and the
        // fitter gave them 7 DSP blocks. That is EXACTLY the failure S15.5
        // closes with: "Do not write six independent `*` operators and assume
        // they pack." Having quoted that line in this file's own header while
        // writing fourteen of them is the reason it is quoted again here.
        prod1 = unit_mul_logic(lane1_q.a, lane1_q.b);
        wb1   = writeback_val(rec[lane1_q.slot], lane1_q.job, prod1);
        case (lane1_q.job)
          3'd0: rec[lane1_q.slot].o_r <= wb1;
          3'd1: rec[lane1_q.slot].o_g <= wb1;
          3'd2: rec[lane1_q.slot].o_b <= wb1;
          // EVERY arm takes `wb`, not `prod`. Job 3 taking the raw product
          // was a real bug: MODULATE2X's doubling lives in writeback_val, so
          // its ALPHA came out half -- 17 where the oracle said 34 -- while
          // its RGB was right, which is the kind of asymmetry that reads as a
          // channel-ordering mistake. writeback_val returns the product
          // unchanged for every recipe that wants it unchanged, so there is
          // no case where `prod` is the correct thing to store.
          3'd3: rec[lane1_q.slot].o_a <= wb1;
          3'd4: rec[lane1_q.slot].o_r <= wb1;
          3'd5: rec[lane1_q.slot].o_g <= wb1;
          default: rec[lane1_q.slot].o_b <= wb1;
        endcase
        rec[lane1_q.slot].done[lane1_q.job] <= 1'b1;
        rec[lane1_q.slot].busy[lane1_q.job] <= 1'b0;
        if (rec[lane1_q.slot].recipe == R_MODULATE2X && prod1 > 8'd127)
          rec[lane1_q.slot].sat2x <= 1'b1;
      end

      // ---- issue up to two jobs (§15.3: two lanes, two jobs per clock) -----
      for (int k = 0; k < 8; k++) jobs_inc[k] = 2'd0;
      lane0_q.valid <= 1'b0;
      lane1_q.valid <= 1'b0;
      issued = 0;
      for (int i = 0; i < RECS; i++) begin
        if (issued < 2 && rec[i].valid) begin
          pend0 = issuable(rec[i].req, rec[i].done, rec[i].busy);
          for (int j = 0; j < 7; j++) begin
            if (issued < 2 && pend0[j]) begin
              ops = operands(rec[i], j[2:0]);
              if (issued == 0) begin
                lane0_q.valid <= 1'b1;
                lane0_q.slot  <= ($clog2(RECS))'(i);
                lane0_q.job   <= j[2:0];
                lane0_q.a     <= ops[15:8];
                lane0_q.b     <= ops[7:0];
              end else begin
                lane1_q.valid <= 1'b1;
                lane1_q.slot  <= ($clog2(RECS))'(i);
                lane1_q.job   <= j[2:0];
                lane1_q.a     <= ops[15:8];
                lane1_q.b     <= ops[7:0];
              end
              // §15.4: count ACTUAL jobs issued, by recipe. ACCUMULATED into
              // a blocking local and committed ONCE below, because both lanes
              // can issue for the same recipe in one cycle -- and two
              // non-blocking assignments to the same counter in one cycle keep
              // only the last, so the counter incremented by ONE for two jobs.
              //
              // It was found by the differential: every colour matched the
              // oracle and only the counts were short, by exactly 1 in 1,200
              // and 1 in 600 -- the two runs that happen to end with a
              // double-issue. An under-reporting counter that nobody checks is
              // the failure mode CLAUDE.md names, and §15.4's whole
              // 80%-capacity argument reads this number.
              jobs_inc[rec[i].recipe] = jobs_inc[rec[i].recipe] + 2'd1;
              rec[i].busy[j] <= 1'b1;
              pend0[j] = 1'b0;
              issued   = issued + 1;
            end
          end
        end
      end

      for (int k = 0; k < 8; k++)
        jobs_by_recipe_r[k] <= jobs_by_recipe_r[k] + 32'(jobs_inc[k]);

      // ---- retire ---------------------------------------------------------
      if (have_retire && o_ready_i) begin
        rec[retire_slot].valid <= 1'b0;
        if (rec[retire_slot].sat2x)
          saturated_mul2x_o <= saturated_mul2x_o + 1;
      end

      // ---- accept (C0) ----------------------------------------------------
      if (f_valid_i && f_ready_o) begin
        rec[alloc_slot].valid   <= 1'b1;
        rec[alloc_slot].tag     <= f_tag_i;
        rec[alloc_slot].recipe  <= f_recipe_i;
        rec[alloc_slot].count   <= f_sample_count_i;
        rec[alloc_slot].weight  <= f_weight_i;
        rec[alloc_slot].s0_r <= f_s0_rgb_i[23:16];
        rec[alloc_slot].s0_g <= f_s0_rgb_i[15:8];
        rec[alloc_slot].s0_b <= f_s0_rgb_i[7:0];
        rec[alloc_slot].s0_a <= f_s0_a_i;
        rec[alloc_slot].s1_r <= f_s1_rgb_i[23:16];
        rec[alloc_slot].s1_g <= f_s1_rgb_i[15:8];
        rec[alloc_slot].s1_b <= f_s1_rgb_i[7:0];
        rec[alloc_slot].s1_a <= f_s1_a_i;
        rec[alloc_slot].s2_r <= f_s2_rgb_i[23:16];
        rec[alloc_slot].s2_g <= f_s2_rgb_i[15:8];
        rec[alloc_slot].s2_b <= f_s2_rgb_i[7:0];
        rec[alloc_slot].s2_a <= f_s2_a_i;
        rec[alloc_slot].done    <= 7'd0;
        rec[alloc_slot].busy    <= 7'd0;
        rec[alloc_slot].sat2x   <= 1'b0;
        rec[alloc_slot].refused <= 1'b0;

        // ---- the two refusals, decided at acceptance ----------------------
        // A refused fragment allocates a record and retires immediately with
        // req == done == 0, so it leaves in order with its tag intact rather
        // than being dropped. Dropping it would lose the tag FRAGROB retires
        // on, and a hole in a retirement stream is far harder to debug than a
        // fragment that says it is malformed.
        if (f_sample_count_i == 2'd0) begin
          // The untextured path. Legal and common: base colour straight
          // through, no recipe applied, no jobs.
          rec[alloc_slot].o_r <= f_base_rgb_i[23:16];
          rec[alloc_slot].o_g <= f_base_rgb_i[15:8];
          rec[alloc_slot].o_b <= f_base_rgb_i[7:0];
          rec[alloc_slot].o_a <= f_base_a_i;
          rec[alloc_slot].req <= 7'd0;
        end else if (f_sample_count_i < samples_required(f_recipe_i)) begin
          rec[alloc_slot].refused <= 1'b1;
          rec[alloc_slot].req     <= 7'd0;
          rec[alloc_slot].o_r <= 8'd0;
          rec[alloc_slot].o_g <= 8'd0;
          rec[alloc_slot].o_b <= 8'd0;
          rec[alloc_slot].o_a <= 8'd0;
          refused_missing_o <= refused_missing_o + 1;
        end else begin
          rec[alloc_slot].req <= req_mask(f_recipe_i);

          // ---- the bypasses (§15.3) --------------------------------------
          // Computed at acceptance because they need no lane. A recipe here
          // retires on the next cycle with req == 0.
          sat_add = 1'b0;
          case (f_recipe_i)
            R_PASSTHRU: begin
              rec[alloc_slot].o_r <= f_s0_rgb_i[23:16];
              rec[alloc_slot].o_g <= f_s0_rgb_i[15:8];
              rec[alloc_slot].o_b <= f_s0_rgb_i[7:0];
              rec[alloc_slot].o_a <= f_s0_a_i;
            end
            R_ADD_SAT: begin
              t = add_sat9(f_s0_rgb_i[23:16], f_s1_rgb_i[23:16]);
              rec[alloc_slot].o_r <= t[7:0]; sat_add |= t[8];
              t = add_sat9(f_s0_rgb_i[15:8],  f_s1_rgb_i[15:8]);
              rec[alloc_slot].o_g <= t[7:0]; sat_add |= t[8];
              t = add_sat9(f_s0_rgb_i[7:0],   f_s1_rgb_i[7:0]);
              rec[alloc_slot].o_b <= t[7:0]; sat_add |= t[8];
              t = add_sat9(f_s0_a_i,          f_s1_a_i);
              rec[alloc_slot].o_a <= t[7:0]; sat_add |= t[8];
              if (sat_add) saturated_add_o <= saturated_add_o + 1;
            end
            R_MASK: begin
              // "s0 where s1 passes, else transparent". The pass test is s1's
              // ALPHA being non-zero: MASK exists so a shape's alpha can cut a
              // colour, and testing the RGB instead would make a
              // black-but-opaque mask erase what it was meant to keep.
              if (f_s1_a_i != 8'd0) begin
                rec[alloc_slot].o_r <= f_s0_rgb_i[23:16];
                rec[alloc_slot].o_g <= f_s0_rgb_i[15:8];
                rec[alloc_slot].o_b <= f_s0_rgb_i[7:0];
                rec[alloc_slot].o_a <= f_s0_a_i;
              end else begin
                rec[alloc_slot].o_r <= 8'd0;
                rec[alloc_slot].o_g <= 8'd0;
                rec[alloc_slot].o_b <= 8'd0;
                rec[alloc_slot].o_a <= 8'd0;
              end
            end
            // MODULATE, MODULATE2X and LERP compute NOTHING at acceptance
            // now. Every one of their products -- alpha included -- goes
            // through a lane, which is the whole point: acceptance-time
            // arithmetic is a multiplier that exists in parallel with the
            // lanes and is exactly what kept this block over its DSP budget.
            R_DETAIL_LIGHT: begin
              // No alpha job: this recipe names no mask, so §15.1's exception
              // does not apply and sample 0 owns the base alpha.
              rec[alloc_slot].o_a <= f_s0_a_i;
            end
            default: ;  // DETAIL_MASK's alpha IS job 3
          endcase
        end
      end

      // An unknown recipe cannot occur on a 3-bit port now that all eight
      // encodings are ratified, so the counter is retained and wired but can
      // no longer be incremented from this port width. Kept rather than
      // deleted: widening the field later must not silently lose the refusal.
      refused_recipe_o <= refused_recipe_o;
    end
  end

endmodule
