// zhao_raster_perspuv_svc.sv — the perspective multiply lane, one product a clock.
//
// BESIDE `zhao_raster_perspuv.sv`, which stays the golden implementation.
// Nothing instantiates this yet.
//
// ---------------------------------------------------------------------------
// WHY
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md: "zhao_raster_perspuv then walks U and V through one
// shared multiplier while refusing another fragment." Two dependent products
// per fragment, and the block will not take another until both are done.
//
// The brief's replacement:
//
//   U0  enqueue U product / U1  multiply / U2  rescale, saturate, store
//   V0  enqueue V product / V1  multiply / V2  rescale, saturate, store
//   > One product launches per cycle. Two are needed per fragment, so this lane
//   > can support one completed perspective pair every two cycles -- well above
//   > the reciprocal engine's one-per-four rate.
//   > A direct-index table stores U and V by token and emits the perspective
//   > result once both have arrived.
//
// That last sentence is the design. U and V are INDEPENDENT given the
// reciprocal, so they never needed to be serial with each other -- only with
// the reciprocal that feeds them.
//
// ---------------------------------------------------------------------------
// SCOPE, STATED SO IT IS NOT MISTAKEN FOR MORE
// ---------------------------------------------------------------------------
// This is the MULTIPLY LANE only. It consumes a reciprocal result (mantissa and
// exponent) rather than instantiating a divider, where `zhao_raster_perspuv`
// contains its own `zhao_raster_rcp24`. Wiring this to `zhao_raster_rcp24_svc`
// is a separate integration step and a separate fit.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS TRANSCRIBED, NOT REDERIVED
// ---------------------------------------------------------------------------
// From `zhao_raster_perspuv.sv`, character for character:
//
//     prod = 64'(num) * 64'({40'd0, mant})
//     sh   = 32 - k
//     resc = (prod + (1 <<< (sh-1))) >>> sh          arithmetic, round-half-up
//     sat  = resc > 2^31-1 || resc < -2^31
//     q    = sat ? (resc<0 ? -2^31 : 2^31-1) : resc[31:0]
//
// The variable shift is the expensive part and it is kept variable. Replacing
// it with a fixed shift plus a correction would be a different function, and
// the gate that would catch that is an exponent boundary nobody thinks to
// write.
//
// ---------------------------------------------------------------------------
// TWO LANES, AND THE RESCALE PIPELINED AFTER THEM (ruling R7, 2026-09-03)
// ---------------------------------------------------------------------------
// ONE PRODUCT A CLOCK IS NOT ENOUGH, and the ruling does the arithmetic:
//
//     1,094,600 samples x 2 products = 2,189,200 products
//     design budget                  = 1,333,333 clocks
//
// The ~50% headroom this lane was written to claim is true for ONE UV pair per
// fragment. Three-sample materials need two products per sample, and one
// product a clock cannot deliver them. R7:
//
//   > instantiate two parallel product paths, U and V, so one complete pair
//   > starts each clock; pipeline variable rescale/saturation after both
//   > products
//
// Both halves are here, and the second half is also the timing fix. MEASURED
// at 62.67 MHz -- the slowest INTERNAL path on the whole texture island:
//
//     p1_prod_q[36] -> e_q[5][1][8]     slack -5.957, data delay 15.0 ns
//
// That was one combinational cone containing a 64-bit add, a 64-bit variable
// arithmetic shift, two 64-bit magnitude compares and a mux. It is now four
// registered stages:
//
//     P1  the product                     (DSP)
//     P2  + the round-half-up constant    (64-bit add)
//     P3  >>> sh                          (64-bit variable shift)
//     P4  saturate, select, write back    (compares and a mux)
//
// THE ARITHMETIC IS UNCHANGED. Every expression above is the same expression,
// evaluated in the same order, with registers between. The paired test drives
// this lane and the shipped `zhao_raster_perspuv` on identical stimulus and
// compares every U, V and saturation flag, so a transcription slip fails
// immediately rather than becoming a plausible wrong coordinate.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_raster_perspuv_svc #(
    parameter int unsigned NTOK = 16,
    parameter int unsigned TAGW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- a fragment whose reciprocal has already returned --------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] u_over_w_i,   // S 8.24
    input  var logic signed [31:0] v_over_w_i,   // S 8.24
    input  var logic        [23:0] r_mant_i,     // reciprocal mantissa
    input  var logic        [ 5:0] r_k_i,        // reciprocal exponent
    input  var logic               depth_zero_i, // invw24 == 0, a caller bug
    input  var logic        [TAGW-1:0] tag_i,

    // ---- the perspective pair, in completion order --------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] u_o,          // S 15.16
    output var logic signed [31:0] v_o,
    output var logic        [TAGW-1:0] tag_o,
    output var logic               sat_o,
    output var logic               depth_zero_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]        fragments_o,
    output var logic [31:0]        products_o,   // multiplier launches
    output var logic [3:0]         occupancy_o
);

  localparam int TW = $clog2(NTOK);

  // ------------------------------------------------------------- entries ----
  logic                  e_val  [NTOK];
  logic [1:0]            e_have [NTOK];
  // SPLIT PER AXIS, and the reason is PORT COUNT rather than tidiness.
  //
  // As `e_num [NTOK][2]` this needs TWO INDEPENDENT READ ADDRESSES in one
  // cycle: `ax` runs over both axes and the two axes are driven by separate
  // work queues, so `pk_i[0]` and `pk_i[1]` are unrelated. An M10K offers two
  // ports in total, so a 2R2W array cannot be one and 1,024 bits stayed in
  // flip-flops.
  //
  // The second dimension is not shared state -- axis 0 never reads axis 1's
  // entry -- so `[NTOK][2]` is two independent `[NTOK]` arrays wearing one
  // name. Split, each has exactly one read address and one write address,
  // which is what a simple dual-port memory does.
  //
  // Diagnosed in reports/PERSPUV-REGISTER-DIAGNOSIS-20260905.md as STATIC
  // ANALYSIS, explicitly not a measurement. The fit that follows this change
  // is what confirms or refutes it.
  logic signed [31:0]    e_num_u [NTOK];
  logic signed [31:0]    e_num_v [NTOK];
  logic [23:0]           e_mant [NTOK];
  logic [5:0]            e_k    [NTOK];
  logic signed [31:0]    e_q_u  [NTOK];
  logic signed [31:0]    e_q_v  [NTOK];
  logic                  e_sat  [NTOK];
  logic                  e_dz   [NTOK];
  logic [TAGW-1:0]       e_tag  [NTOK];

  logic [TW-1:0] head_q, tail_q;
  logic [TW:0]   free_cnt_q;

  // Ready from LOCAL storage, never from the consumer. Same property the
  // reciprocal service and TEXJOIN v2 rest on.
  assign v_ready_o = (free_cnt_q != '0);

  // ------------------------------------------------------- WORK QUEUES ------
  // ONE PER AXIS, so a complete pair can start on one clock.
  //
  // MEASURED, 2026-09-03. The two-lane rebuild took this block from 62.67 to
  // 99.14 MHz internal and its new worst path named what was left:
  //
  //     pk_i~5_OTERM777 -> p1_prod_q[0][36]_OTERM139
  //
  // That is the 16-entry PRIORITY SCAN feeding the multiplier. Each axis walked
  // all sixteen entries from the retire head looking for one that still needed
  // its product -- sixteen sequentially dependent comparisons whose result then
  // selects a 32-bit numerator out of a sixteen-deep table.
  //
  // It is the same structure the TEXJOIN rebuild replaced, and it was left in
  // place there deliberately: the measured wall was the rescale cone, and
  // fixing the thing that is not the limiter is how a block gets rewritten for
  // a wire. The rescale is fixed; this is now the limiter; so it goes.
  //
  // A scan asks "who still needs work?" every clock. A queue is told once, at
  // accept, and never asks. The ORDER IS IDENTICAL -- allocation order, oldest
  // first, because that is the order things are pushed in.
  //
  // Capacity is NTOK per axis and cannot overflow: one entry is pushed per
  // accepted fragment per axis, and a fragment cannot be accepted without a
  // free slot. NTOK is a power of two so the occupancy subtraction counts.
  logic [TW-1:0] wq   [2][NTOK];
  logic [TW:0]   wq_wp [2], wq_rp [2];

  logic          pk_v [2];
  logic [TW-1:0] pk_i [2];
  always_comb begin
    for (int unsigned ax = 0; ax < 2; ax++) begin
      pk_v[ax] = (wq_wp[ax] != wq_rp[ax]);
      pk_i[ax] = wq[ax][wq_rp[ax][TW-1:0]];
    end
  end

  // ---- P1..P4, one set per axis --------------------------------------------
  // The stages are per-lane arrays rather than two copies of the same names,
  // so the U and V paths cannot drift apart by an edit that touches one.
  logic               p1_v_q [2];
  logic [TW-1:0]      p1_i_q [2];
  logic signed [63:0] p1_prod_q [2];
  logic [5:0]         p1_k_q [2];

  logic               p2_v_q [2];
  logic [TW-1:0]      p2_i_q [2];
  logic signed [63:0] p2_sum_q [2];
  logic [5:0]         p2_sh_q [2];

  logic               p3_v_q [2];
  logic [TW-1:0]      p3_i_q [2];
  logic signed [63:0] p3_resc_q [2];

  // ---- the combinational pieces, each now BETWEEN two registers ------------
  logic [5:0]         sh_c   [2];
  logic signed [63:0] sum_c  [2];
  logic signed [63:0] resc_c [2];
  logic               sat_c  [2];
  logic signed [31:0] q_c    [2];
  always_comb begin
    for (int unsigned ax = 0; ax < 2; ax++) begin
      // P2: the round-half-up constant, added where the original adds it.
      sh_c[ax]  = 6'd32 - p1_k_q[ax];
      sum_c[ax] = $signed(p1_prod_q[ax]) + $signed(64'd1 <<< (sh_c[ax] - 6'd1));
      // P3: the variable arithmetic shift, still variable.
      resc_c[ax] = $signed(p2_sum_q[ax]) >>> p2_sh_q[ax];
      // P4: saturate and select, exactly the original's expression.
      sat_c[ax] = (p3_resc_q[ax] > 64'sh0000_0000_7FFF_FFFF)
               || (p3_resc_q[ax] < -64'sh0000_0000_8000_0000);
      q_c[ax]   = sat_c[ax] ? (p3_resc_q[ax][63] ? 32'sh8000_0000 : 32'sh7FFF_FFFF)
                            : p3_resc_q[ax][31:0];
    end
  end

  // ---------------------------------------------------------- retirement ----
  // Allocation order. A pair is done when BOTH axes have landed.
  logic head_done;
  assign head_done = e_val[head_q] && (e_have[head_q] == 2'b11);

  assign r_valid_o    = head_done;
  assign u_o          = e_q_u[head_q];
  assign v_o          = e_q_v[head_q];
  assign tag_o        = e_tag[head_q];
  assign sat_o        = e_sat[head_q];
  assign depth_zero_o = e_dz[head_q];

  always_comb begin
    occupancy_o = 4'd0;
    for (int i = 0; i < NTOK; i++) occupancy_o = occupancy_o + 4'(e_val[i]);
  end

  // ------------------------------------------------------------ sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      head_q      <= '0;
      tail_q      <= '0;
      free_cnt_q  <= (TW + 1)'(NTOK);
      for (int ax = 0; ax < 2; ax++) begin
        wq_wp[ax] <= '0;
        wq_rp[ax] <= '0;
      end
      for (int ax = 0; ax < 2; ax++) begin
        p1_v_q[ax] <= 1'b0;
        p2_v_q[ax] <= 1'b0;
        p3_v_q[ax] <= 1'b0;
      end
      fragments_o <= 32'd0;
      products_o  <= 32'd0;
      for (int i = 0; i < NTOK; i++) begin
        e_val[i]  <= 1'b0;
        e_have[i] <= 2'b00;
      end
    end else begin
      // ---- the occupancy counter moves ONCE ------------------------------
      // Accept and retire can happen in the SAME clock, and two non-blocking
      // assignments to free_cnt_q in one always_ff do not add up -- the last
      // one wins and the other is silently lost. That produced a count which
      // grew without bound, v_ready_o stuck high, and tail_q wrapping over
      // live entries: the lane answered 193 of 335 fragments and then hung.
      //
      // Computing the NET delta once is the fix. The first draft had the two
      // increments in their respective branches, which reads correctly and is
      // wrong, and only a test that stalled the output found it -- with
      // r_ready_i always high the two events never coincide.
      begin
        automatic logic acc = v_valid_i && v_ready_o;
        automatic logic ret = head_done && r_ready_i;
        if (acc && !ret)      free_cnt_q <= free_cnt_q - 1'b1;
        else if (!acc && ret) free_cnt_q <= free_cnt_q + 1'b1;
      end

      // ---- accept ---------------------------------------------------------
      if (v_valid_i && v_ready_o) begin
        e_val[tail_q]     <= 1'b1;
        // A depth-zero fragment is a caller bug the original flags rather than
        // computes. It still occupies a slot and still retires, so the caller
        // sees one answer per request -- it simply produces no product.
        // The work QUEUE is the record of what is outstanding now, so the old
        // `e_need` mask is state with no reader -- and state with no reader is
        // where a stale bit hides. Removed, exactly as `iss_q` was in TEXJOIN
        // when its scan went.
        //
        // Push the work, once, where the answer is already known. A depth-zero
        // fragment produces no product and enqueues nothing.
        if (!depth_zero_i)
          for (int unsigned ax = 0; ax < 2; ax++) begin
            // Indexed by the QUEUE's own write pointer, not by `tail_q`. The
            // two look interchangeable and are not: a depth-zero fragment
            // takes a table slot and pushes NO work, so the table pointer and
            // the queue pointer diverge the first time one appears. Writing at
            // `tail_q` then lands the entry in the wrong queue slot, and the
            // suite caught it as one fragment of 335 that never retired.
            wq[ax][wq_wp[ax][TW-1:0]] <= tail_q;
            wq_wp[ax] <= wq_wp[ax] + (TW+1)'(1);
          end
        e_have[tail_q]    <= depth_zero_i ? 2'b11 : 2'b00;
        e_num_u[tail_q]   <= u_over_w_i;
        e_num_v[tail_q]   <= v_over_w_i;
        e_mant[tail_q]    <= r_mant_i;
        e_k[tail_q]       <= r_k_i;
        // THE ZERO-INITIALISATION IS GONE, AND ITS ABSENCE IS THE POINT.
        //
        // `e_q_u[tail_q] <= 0` here was a SECOND WRITE ADDRESS -- `tail_q` at
        // allocation and `p3_i_q[0]` at P4 -- and an M10K simple-dual-port has
        // one write port. That, not the asynchronous read, is why `e_q_u` and
        // `e_q_v` stayed in flip-flops while `e_tag`, which has one writer and
        // one reader, INFERRED. The island's RAM Summary names both outcomes.
        //
        // It is redundant as well as costly: `e_q_u[head_q]` is read only when
        // `head_done` holds, which requires `e_have[head_q] == 2'b11`, and
        // `e_have[i][0]` is set ONLY in the same P4 branch that writes
        // `e_q_u[i]`. No reader can observe the initialised value.
        //
        // The owner's COMBINE brief states the same rule for that block's
        // scratch, in the same words: "Do NOT initialize scratch on admission:
        // that creates a second writer." Two blocks, one law.
        //
        // `e_sat` KEEPS its clear at allocation, deliberately: it is a sticky
        // flag that P4 only ever SETS, so without the clear a saturation would
        // leak into the next fragment to reuse the token. It is 16 bits in
        // total and was never a candidate for memory anyway.
        e_sat[tail_q]     <= 1'b0;
        e_dz[tail_q]      <= depth_zero_i;
        e_tag[tail_q]     <= tag_i;
        tail_q            <= (tail_q == TW'(NTOK - 1)) ? '0 : tail_q + TW'(1);
        fragments_o       <= fragments_o + 32'd1;
      end

      // ---- P0 launch: BOTH axes, independently -----------------------------
      // Each axis pops ITS OWN queue, so the two lanes touch no shared
      // variable here at all. That is the strongest form of the rule the
      // free-count race taught: the safest way not to lose one of two
      // same-clock updates to a variable is for there to be no shared
      // variable.
      for (int unsigned ax = 0; ax < 2; ax++) begin
        p1_v_q[ax] <= pk_v[ax];
        if (pk_v[ax]) begin
          p1_i_q[ax]    <= pk_i[ax];
          p1_k_q[ax]    <= e_k[pk_i[ax]];
          // The per-axis select is explicit. Each arm reads ONE array at ONE
          // address, which is the whole point of the split.
          p1_prod_q[ax] <= (ax == 0)
              ? 64'(e_num_u[pk_i[0]]) * $signed({40'd0, e_mant[pk_i[0]]})
              : 64'(e_num_v[pk_i[1]]) * $signed({40'd0, e_mant[pk_i[1]]});
          wq_rp[ax] <= wq_rp[ax] + (TW+1)'(1);
        end
      end
      // ONE assignment, both lanes. The loop above once carried
      // `products_o <= products_o + 1` inside it -- two nonblocking
      // assignments to one counter in one always_ff, so a clock that launched
      // both axes counted ONE. The paired suite caught it on the first run:
      // 334 products where 668 were issued.
      //
      // A few lines above that line sat a comment explaining this very rule
      // for a different signal. Knowing the rule is not the same as applying
      // it, which is why the counter is checked by a test rather than by care.
      products_o <= products_o + 32'(pk_v[0]) + 32'(pk_v[1]);

      // ---- P2: + the rounding constant -------------------------------------
      for (int unsigned ax = 0; ax < 2; ax++) begin
        p2_v_q[ax] <= p1_v_q[ax];
        if (p1_v_q[ax]) begin
          p2_i_q[ax]   <= p1_i_q[ax];
          p2_sum_q[ax] <= sum_c[ax];
          p2_sh_q[ax]  <= sh_c[ax];
        end
      end

      // ---- P3: the variable shift ------------------------------------------
      for (int unsigned ax = 0; ax < 2; ax++) begin
        p3_v_q[ax] <= p2_v_q[ax];
        if (p2_v_q[ax]) begin
          p3_i_q[ax]    <= p2_i_q[ax];
          p3_resc_q[ax] <= resc_c[ax];
        end
      end

      // ---- P4: saturate, select, write back ---------------------------------
      // Same bit-select discipline as the launch: `e_have[i][ax]` and
      // `e_q[i][ax]` are per-axis, so both lanes may complete the same
      // fragment on one clock.
      for (int unsigned ax = 0; ax < 2; ax++) begin
        if (p3_v_q[ax]) begin
          if (ax == 0) e_q_u[p3_i_q[0]] <= q_c[0];
          else         e_q_v[p3_i_q[1]] <= q_c[1];
          e_have[p3_i_q[ax]][ax] <= 1'b1;
        end
      end
      // `e_sat` is ONE bit for the pair, so it is written once from the OR of
      // both lanes rather than from two branches -- two branches would be the
      // same lost-update fault the launch comment describes, and here it would
      // silently drop a saturation flag.
      begin
        automatic logic set_u = p3_v_q[0] && sat_c[0];
        automatic logic set_v = p3_v_q[1] && sat_c[1];
        if (set_u && set_v && (p3_i_q[0] == p3_i_q[1])) e_sat[p3_i_q[0]] <= 1'b1;
        else begin
          if (set_u) e_sat[p3_i_q[0]] <= 1'b1;
          if (set_v) e_sat[p3_i_q[1]] <= 1'b1;
        end
      end

      // ---- retire ---------------------------------------------------------
      if (head_done && r_ready_i) begin
        e_val[head_q] <= 1'b0;
        head_q     <= (head_q == TW'(NTOK - 1)) ? '0 : head_q + TW'(1);
      end
    end
  end

endmodule : zhao_raster_perspuv_svc

`default_nettype wire
