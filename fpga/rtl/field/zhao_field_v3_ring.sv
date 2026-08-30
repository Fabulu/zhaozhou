// zhao_field_v3_ring.sv — UOP_RING_PREP for four points at once: nine
// separately-rounded products on the shared bank, and NO reciprocal.
//
// ENFORCED-BY: tests/differential/field_v3_ring_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THERE IS NO DIVIDER HERE, WHICH IS THE PLANNER'S DECISION NOT MINE
// ---------------------------------------------------------------------------
// OP_RING is two smoothsteps and a product, and a smoothstep contains a
// division. The obvious hardware reading is "RING needs two reciprocals per
// point", and reports/REMAINING_BLOCKERS.md says exactly that.
//
// The planner already decided otherwise. `zfield_plan.cpp` lowers RING three
// ways:
//
//   nothing varying              -> lower_uniform: the whole op is computed
//                                   ONCE in the prep and never reaches a
//                                   vector unit at all
//   d varying, radii uniform     -> lower_prepared_ring: UOP_RING_PREP
//   radii varying                -> lower_varying: the full OP_RING
//
// In the prepared case the two reciprocals are `PrepUop{OP_RCP, ...}` in the
// UNIFORM plan, computed once per field instance rather than once per point.
// What reaches the vector engine is `zfield::steps::ring_prepared`, whose
// nine products contain no division at all.
//
// So this block implements UOP_RING_PREP. A ring's radii are program
// constants in every use the engine has, and the planner is swept 16/16
// against the interpreter, so this is the shape the software already commits
// to rather than an optimisation invented here.
//
// THE VARYING-RADII CASE IS NOT IMPLEMENTED, and that is a deliberate,
// recorded gap rather than an oversight. It needs a per-point reciprocal, and
// a per-point reciprocal is a second shared resource with its own arbitration,
// refusal and starvation questions -- the class of thing that has cost the
// most time in this engine. It should be built when something needs it, with
// the composition test written first.
//
// ---------------------------------------------------------------------------
// THE LAW: zfield::steps::ring_prepared, product for product
// ---------------------------------------------------------------------------
//     t0  = fx_mul(fx_sub(d, r0), rA)              (1)
//     t0  = clamp(t0, 0, 1<<16)
//     t0s = fx_mul(t0, t0)                         (2)
//     u0  = fx_mul(2<<16, t0)                      (3)
//     s0  = fx_mul(t0s, fx_sub(3<<16, u0))         (4)
//
//     t1  = fx_mul(fx_sub(d, m), rB)               (5)
//     t1  = clamp(t1, 0, 1<<16)
//     t1s = fx_mul(t1, t1)                         (6)
//     u1  = fx_mul(2<<16, t1)                      (7)
//     s1  = fx_mul(t1s, fx_sub(3<<16, u1))         (8)
//
//     dst = fx_mul(s0, fx_sub(1<<16, s1))          (9)
//
// EVERY PRODUCT IS ROUNDED SEPARATELY, as in ROT and for the same reason: the
// reference does it that way and a fused chain is a different number. Nine
// products, nine rescales, and the subtractions saturate in between.
//
// NINE FOUR-WIDE BANK REQUESTS, not thirty-six multiplies: every operand is
// per point, so all four points take each step together. The two smoothsteps
// are NOT overlapped -- steps 5..8 could run beside 1..4 on a wider bank, but
// on a four-wide bank they would simply queue, and the sequential form is the
// one whose rounding is obviously the reference's.
module zhao_field_v3_ring (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    // `d` is the varying operand; r0, m, rA and rB come from the PREP and are
    // the same for all four points, which is what "prepared" means.
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] d_0_i, d_1_i, d_2_i, d_3_i,
    input  var logic signed [31:0] r0_i,
    input  var logic signed [31:0] m_i,
    input  var logic signed [31:0] rA_i,
    input  var logic signed [31:0] rB_i,
    // ---- SMOOTH MODE: stop after the FIRST smoothstep -----------------------
    //
    // Every Earth builder expands `smoothstep(e0, e1, x)` into seven varying
    // uops, and this unit already computes exactly that sequence as products
    // P1..P4 -- the whole first half of `ring_prepared`. Contracting those
    // seven uops into one request is bit-exact (39,321 combinations of
    // distance, edge and span, zero value and zero ledger mismatches) and was
    // measured as a LOSS when it ran the full nine-product recipe: the ring
    // went from serving one program to serving every point of all three, and
    // the shared multiplier bank could not carry it. Doubling the unit count
    // recovered almost nothing, which is what identified the PRODUCTS rather
    // than the units as the cost.
    //
    // A smoothstep needs four of the nine. `smooth_i` stops the walk after P4
    // and answers with s0, skipping the dead second branch and the identity
    // multiply by one. Same arithmetic, same order, same rounding -- simply
    // fewer steps taken.
    input  var logic               smooth_i,
    input  var logic        [ 7:0] tag_i,

    // ---- reply -------------------------------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] o0_0_o, o0_1_o, o0_2_o, o0_3_o,
    output var logic        [ 3:0] sat_add_o,
    output var logic        [ 3:0] sat_mul_o,
    output var logic        [ 7:0] tag_o,

    // ---- the shared four-wide bank (engine property) -----------------------
    output var logic               mul_issue_o,
    input  var logic               mul_ready_i,
    output var logic signed [32:0] mul_a_0_o, mul_a_1_o, mul_a_2_o, mul_a_3_o,
    output var logic signed [32:0] mul_b_0_o, mul_b_1_o, mul_b_2_o, mul_b_3_o,
    input  var logic               mul_valid_i,
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic signed [65:0] mul_p_0_i, mul_p_1_i, mul_p_2_i, mul_p_3_i
    /* verilator lint_on UNUSEDSIGNAL */
);

  localparam int LANES = 4;

  localparam logic signed [31:0] FX_ONE   = 32'sh0001_0000;
  localparam logic signed [31:0] FX_TWO   = 32'sh0002_0000;
  localparam logic signed [31:0] FX_THREE = 32'sh0003_0000;

  // One issue state and one wait state per product, nine of each. Named for
  // the reference's steps so the mux below reads as its formula.
  localparam logic [4:0] G_IDLE = 5'd0;
  localparam logic [4:0] G_P1   = 5'd1;   // (d - r0) * rA
  localparam logic [4:0] G_P2   = 5'd3;   // t0 * t0
  localparam logic [4:0] G_P3   = 5'd5;   // 2 * t0
  localparam logic [4:0] G_P4   = 5'd7;   // t0s * (3 - u0)
  localparam logic [4:0] G_P5   = 5'd9;   // (d - m) * rB
  localparam logic [4:0] G_P6   = 5'd11;  // t1 * t1
  localparam logic [4:0] G_P7   = 5'd13;  // 2 * t1
  localparam logic [4:0] G_P8   = 5'd15;  // t1s * (3 - u1)
  localparam logic [4:0] G_P9   = 5'd17;  // s0 * (1 - s1)
  localparam logic [4:0] G_OUT  = 5'd19;

  // An issue state is even and its wait state is the next odd one, so the two
  // read as one step and the FSM is a straight walk.
  logic [4:0] state;
  // Which wait state carries the answer, and therefore where the walk stops.
  logic       smooth_r;
  logic [4:0] last_wait_c;
  assign last_wait_c = smooth_r ? (G_P4 + 5'd1) : (G_P9 + 5'd1);
  logic       is_issue_c;
  // ---------------------------------------------------------------------------
  // P3 AND P7 ARE SHIFTS, NOT PRODUCTS
  // ---------------------------------------------------------------------------
  // The reference writes `fx_mul(F(2 << 16), t)` and this block used to issue
  // it into the shared multiplier bank like any other product. It is not one.
  // `t` has just been CLAMPED to [0, 1.0] by `clamp01`, so 2*t is in [0, 2.0]
  // and cannot overflow; 2.0 is exactly representable, so
  //
  //     fx_mul(2<<16, t) = resc16(131072 * t) = ((131072*t) + 32768) >> 16
  //
  // and 131072*t is an exact multiple of 65536, so the rounding term never
  // carries. The result is t << 1 with no residue and no ledger event. That is
  // proved EXHAUSTIVELY, not argued: all 65,537 values of t in [0, 1.0] give
  // an identical raw and an untouched SatLedger.
  //
  // ENFORCED-BY: tests/differential/field_v3_ring_svc_directed.cpp:main
  //
  // WHY IT MATTERS, WHICH IS NOT THE ARITHMETIC. Every service in this engine
  // -- every ring unit, every root bank, trig, curve -- issues into ONE
  // four-wide multiplier bank, and crater_ring measured that bank at 75%
  // occupancy while parameter sweeps of RING_UNITS 8/16/32 and DIST_BANKS 4/8
  // moved the frame cost by not one clock. More units behind a saturated bank
  // are more consumers of the constraint. These two steps were 2 of the 9
  // grants a full ring makes and 1 of the 4 a smooth ring makes, and deleting
  // them deletes the BANK TRANSACTION as well as the arithmetic: the walk
  // skips the wait state too, because there is nothing in flight to wait for.
  logic       is_shift_c;
  assign is_shift_c = (state == G_P3) || (state == G_P7);
  assign is_issue_c = (state != G_IDLE) && (state != G_OUT) && (state[0] == 1'b1) &&
                      !is_shift_c;

  logic [7:0]         h_tag;
  logic signed [31:0] h_d [LANES];
  logic signed [31:0] h_r0, h_m, h_rA, h_rB;

  logic signed [31:0] t0 [LANES], t0s [LANES], u0 [LANES], s0 [LANES];
  logic signed [31:0] t1 [LANES], t1s [LANES], u1 [LANES], s1 [LANES];
  logic [3:0]         fired_mul, fired_add;

  // ---- the reference's arithmetic, function for function ------------------
  function automatic logic signed [31:0] resc16(input logic signed [63:0] v);
    logic signed [64:0] r;
    begin
      r = (65'(v) + 65'sd32768) >>> 16;
      if (r > 65'sd2147483647) resc16 = 32'sh7FFF_FFFF;
      else if (r < -65'sd2147483648) resc16 = 32'sh8000_0000;
      else resc16 = r[31:0];
    end
  endfunction

  function automatic logic resc16_fired(input logic signed [63:0] v);
    logic signed [64:0] r;
    begin
      r = (65'(v) + 65'sd32768) >>> 16;
      resc16_fired = (r > 65'sd2147483647) || (r < -65'sd2147483648);
    end
  endfunction

  function automatic logic signed [31:0] sub_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) - $signed({b[31], b});
      if (t > 33'sd2147483647) sub_sat = 32'sh7FFF_FFFF;
      else if (t < -33'sd2147483648) sub_sat = 32'sh8000_0000;
      else sub_sat = t[31:0];
    end
  endfunction

  function automatic logic sub_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) - $signed({b[31], b});
      sub_fired = (t > 33'sd2147483647) || (t < -33'sd2147483648);
    end
  endfunction

  // fx_clamp does NOT saturate-report: it is a clamp to a range, which is the
  // op's own law rather than an overflow. The reference's fx_clamp takes no
  // ledger, and that is why nothing is recorded here.
  function automatic logic signed [31:0] clamp01(input logic signed [31:0] v);
    begin
      if (v < 32'sd0) clamp01 = 32'sd0;
      else if (v > FX_ONE) clamp01 = FX_ONE;
      else clamp01 = v;
    end
  endfunction

  // ---- the bank's operands, one step at a time ----------------------------
  logic signed [32:0] mul_a [LANES], mul_b [LANES];
  logic signed [63:0] prod  [LANES];
  assign prod[0] = mul_p_0_i[63:0];
  assign prod[1] = mul_p_1_i[63:0];
  assign prod[2] = mul_p_2_i[63:0];
  assign prod[3] = mul_p_3_i[63:0];

  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      mul_a[l] = 33'sd0;
      mul_b[l] = 33'sd0;
      case (state)
        G_P1: begin
          mul_a[l] = 33'(sub_sat(h_d[l], h_r0));
          mul_b[l] = 33'(h_rA);
        end
        G_P2: begin
          mul_a[l] = 33'(t0[l]);
          mul_b[l] = 33'(t0[l]);
        end
        G_P3: begin
          mul_a[l] = 33'(FX_TWO);
          mul_b[l] = 33'(t0[l]);
        end
        G_P4: begin
          mul_a[l] = 33'(t0s[l]);
          mul_b[l] = 33'(sub_sat(FX_THREE, u0[l]));
        end
        G_P5: begin
          mul_a[l] = 33'(sub_sat(h_d[l], h_m));
          mul_b[l] = 33'(h_rB);
        end
        G_P6: begin
          mul_a[l] = 33'(t1[l]);
          mul_b[l] = 33'(t1[l]);
        end
        G_P7: begin
          mul_a[l] = 33'(FX_TWO);
          mul_b[l] = 33'(t1[l]);
        end
        G_P8: begin
          mul_a[l] = 33'(t1s[l]);
          mul_b[l] = 33'(sub_sat(FX_THREE, u1[l]));
        end
        default: begin
          mul_a[l] = 33'(s0[l]);
          mul_b[l] = 33'(sub_sat(FX_ONE, s1[l]));
        end
      endcase
    end
  end

  // In smooth mode the walk never reaches P5..P9, so those terms are dead
  // rather than suppressed -- the state simply never gets there.
  // P3 and P7 are absent: they are shifts and never ask the bank.
  assign mul_issue_o = (state == G_P1) || (state == G_P2) ||
                       (state == G_P4) || (state == G_P5) || (state == G_P6) ||
                       (state == G_P8) || (state == G_P9);
  assign mul_a_0_o = mul_a[0];
  assign mul_a_1_o = mul_a[1];
  assign mul_a_2_o = mul_a[2];
  assign mul_a_3_o = mul_a[3];
  assign mul_b_0_o = mul_b[0];
  assign mul_b_1_o = mul_b[1];
  assign mul_b_2_o = mul_b[2];
  assign mul_b_3_o = mul_b[3];

  assign v_ready_o = (state == G_IDLE);
  assign tag_o     = h_tag;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= G_IDLE;
      smooth_r <= 1'b0;
      h_tag <= 8'd0;
      h_r0 <= '0;
      h_m <= '0;
      h_rA <= '0;
      h_rB <= '0;
      fired_mul <= 4'b0;
      fired_add <= 4'b0;
      r_valid_o <= 1'b0;
      sat_add_o <= 4'b0;
      sat_mul_o <= 4'b0;
      for (int l = 0; l < LANES; l++) begin
        h_d[l]  <= '0;
        t0[l]   <= '0;
        t0s[l]  <= '0;
        u0[l]   <= '0;
        s0[l]   <= '0;
        t1[l]   <= '0;
        t1s[l]  <= '0;
        u1[l]   <= '0;
        s1[l]   <= '0;
      end
      o0_0_o <= '0;
      o0_1_o <= '0;
      o0_2_o <= '0;
      o0_3_o <= '0;
    end else begin
      case (state)
        G_IDLE: begin
          if (v_valid_i) begin
            h_d[0] <= d_0_i;
            h_d[1] <= d_1_i;
            h_d[2] <= d_2_i;
            h_d[3] <= d_3_i;
            h_r0 <= r0_i;
            h_m  <= m_i;
            h_rA <= rA_i;
            h_rB <= rB_i;
            h_tag <= tag_i;
            smooth_r <= smooth_i;
            fired_mul <= 4'b0;
            fired_add <= 4'b0;
            sat_add_o <= 4'b0;
            sat_mul_o <= 4'b0;
            state <= G_P1;
          end
        end

        G_OUT: begin
          if (!r_valid_o) begin
            sat_mul_o <= fired_mul;
            sat_add_o <= fired_add;
            r_valid_o <= 1'b1;
          end else if (r_ready_i) begin
            r_valid_o <= 1'b0;
            state <= G_IDLE;
          end
        end

        default: begin
          // EVERY ISSUE HOLDS UNTIL GRANTED, and every wait holds until the
          // product lands. The operands are registers that do not move, so a
          // refusal costs one clock.
          // The two shift steps complete in their own clock and skip their
          // wait state, because no product is in flight to wait for.
          if (is_shift_c) begin
            for (int l = 0; l < LANES; l++) begin
              if (state == G_P3) u0[l] <= t0[l] <<< 1;
              else               u1[l] <= t1[l] <<< 1;
            end
            state <= state + 5'd2;
          end else if (is_issue_c) begin
            if (mul_ready_i) state <= state + 5'd1;
          end else if (mul_valid_i) begin
            for (int l = 0; l < LANES; l++) begin
              if (resc16_fired(prod[l])) fired_mul[l] <= 1'b1;
              case (state)
                // The clamp is part of t, not a separate step: the reference
                // clamps immediately after the product and before squaring.
                G_P1 + 5'd1: t0[l]  <= clamp01(resc16(prod[l]));
                G_P2 + 5'd1: t0s[l] <= resc16(prod[l]);
                // P3 and P7 no longer land here -- their wait states are
                // never entered. Named rather than deleted so the walk still
                // reads against the reference's numbered products.
                G_P4 + 5'd1: s0[l]  <= resc16(prod[l]);
                G_P5 + 5'd1: t1[l]  <= clamp01(resc16(prod[l]));
                G_P6 + 5'd1: t1s[l] <= resc16(prod[l]);
                G_P8 + 5'd1: s1[l]  <= resc16(prod[l]);
                // The ninth product is the ANSWER and is written below, once
                // rather than per lane. Named rather than defaulted so the
                // reader sees that all ten wait states are accounted for.
                default: ;
              endcase
            end
            // THE ANSWER IS THE LAST PRODUCT THIS MODE TAKES: the ninth for a
            // full ring, the FOURTH for a smoothstep -- P4 is `s0`, which is
            // what the seven contracted uops computed.
            if (state == last_wait_c) begin
              o0_0_o <= resc16(prod[0]);
              o0_1_o <= resc16(prod[1]);
              o0_2_o <= resc16(prod[2]);
              o0_3_o <= resc16(prod[3]);
            end
            state <= (state == last_wait_c) ? G_OUT : (state + 5'd1);
          end
        end
      endcase

      // The saturating subtractions happen in the operand mux, so their
      // reporting belongs where the operands are FORMED rather than where a
      // product lands. Sampled on the issue clock, which is when the mux is
      // driving them.
      if (is_issue_c && mul_ready_i) begin
        for (int l = 0; l < LANES; l++) begin
          case (state)
            G_P1: if (sub_fired(h_d[l], h_r0))         fired_add[l] <= 1'b1;
            G_P4: if (sub_fired(FX_THREE, u0[l]))      fired_add[l] <= 1'b1;
            G_P5: if (sub_fired(h_d[l], h_m))          fired_add[l] <= 1'b1;
            G_P8: if (sub_fired(FX_THREE, u1[l]))      fired_add[l] <= 1'b1;
            G_P9: if (sub_fired(FX_ONE, s1[l]))        fired_add[l] <= 1'b1;
            default: ;
          endcase
        end
      end
    end
  end

endmodule : zhao_field_v3_ring
