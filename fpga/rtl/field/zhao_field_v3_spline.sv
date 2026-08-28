// zhao_field_v3_spline.sv — SPLINE's Catmull-Rom arithmetic for four points at
// once: the three coefficients, a two-step Horner, and the half.
//
// ENFORCED-BY: tests/differential/field_v3_spline_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND WHAT IT DELIBERATELY IS NOT
// ---------------------------------------------------------------------------
// OP_SPLINE is two things bolted together:
//
//   THE LOOKUP   a six-step search for the segment, a clamp, the parameter t,
//                and FOUR neighbour reads y[i-1..i+2]
//   THE CURVE    three Catmull-Rom coefficients, a two-step Horner, and a half
//
// This block is the second half. It takes t and the four neighbours as inputs,
// exactly as `zhao_field_v3_ring` takes its prepared reciprocals, and for the
// same reason: the half that needs a table cache is somebody else's, and
// pretending otherwise would mean a second copy of the curve service.
//
// THAT SPLIT IS A REAL DECISION AND IT IS NOT FREE. `zhao_probe_curve_svc`
// already owns a table cache and a six-step search, and it CAPTURES ONE ENTRY
// on the way down -- precisely because CURVE needs only one. SPLINE needs
// FOUR, so it is a SECOND READER of that cache rather than another mode of the
// service. Widening it is the remaining work, and it is written down in
// reports/FIELD_V3_REMAINING_OPS.md rather than left to be discovered.
//
// The brief already calls SPLINE a cold service lane, which is the same
// conclusion reached from the other end.
//
// ---------------------------------------------------------------------------
// THE LAW: zfield::steps::exec_op's OP_SPLINE arm, expression for expression
// ---------------------------------------------------------------------------
//     C1 = sat32(p2 - p0)
//     C2 = sat32(2*p0 - 5*p1 + 4*p2 - p3)
//     C3 = sat32(-p0 + 3*p1 - 3*p2 + p3)
//
//     u  = fx_mad(t, C3, C2)        Horner, step one
//     u  = fx_mad(t, u,  C1)        Horner, step two
//     v  = fx_mul(t, u)
//     dst = fx_add(p1, rescale(v, 1))
//
// THREE THINGS ARE LOAD-BEARING:
//
// 1. **THE COEFFICIENTS SATURATE AT 32 BITS, and their small multiples are
//    EXACT.** 2*p0, 5*p1, 4*p2 are computed at 64 bits and only the RESULT is
//    clamped. Doing them in 32 bits would wrap on large control points and
//    give a smooth, wrong curve.
//
// 2. **fx_mad IS ONE ROUNDING -- AND HERE THAT IS NOT OBSERVABLE.** `a*b +
//    (c << 16)` is formed at full width and rescaled once. Writing it the
//    other way round -- rescale the product, then add c -- gives THE SAME
//    NUMBER, because `c << 16` has sixteen zero low bits and adding it
//    commutes with the `>> 16`. Measured over 200,000 random pairs: zero
//    differences. Mutant S12 survives for exactly that reason and carries the
//    proof.
//
//    An earlier version of this comment claimed the distinction mattered here,
//    by analogy with ROT. It does not. The rounding difference that IS real is
//    rounding each PRODUCT separately, which is what ROT does deliberately --
//    and SPLINE's Horner has only one product per step, so the question never
//    arises. The analogy was the error, not the arithmetic.
//
// 3. **THE FINAL HALF IS A RESCALE BY ONE, not a shift.** `rescale(v, 1)`
//    rounds half up and saturates; `v >>> 1` does neither.
module zhao_field_v3_spline (
    input var logic clk,
    input var logic rst_n,

    // ---- request: one four-point group -------------------------------------
    // `t` is the segment parameter, already clamped to [0, 1] by the lookup.
    // p0..p3 are the four neighbouring control points for that point's
    // segment -- per point, because four points can land in four segments.
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] t_0_i, t_1_i, t_2_i, t_3_i,
    input  var logic signed [31:0] p0_0_i, p0_1_i, p0_2_i, p0_3_i,
    input  var logic signed [31:0] p1_0_i, p1_1_i, p1_2_i, p1_3_i,
    input  var logic signed [31:0] p2_0_i, p2_1_i, p2_2_i, p2_3_i,
    input  var logic signed [31:0] p3_0_i, p3_1_i, p3_2_i, p3_3_i,
    input  var logic        [ 7:0] tag_i,

    // ---- reply -------------------------------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] o0_0_o, o0_1_o, o0_2_o, o0_3_o,
    output var logic        [ 3:0] sat_mul_o,
    output var logic        [ 3:0] sat_add_o,
    output var logic        [ 3:0] sat_rescale_o,
    output var logic        [ 7:0] tag_o,

    // ---- the shared four-wide bank (engine property) -----------------------
    output var logic               mul_issue_o,
    input  var logic               mul_ready_i,
    output var logic signed [32:0] mul_a_0_o, mul_a_1_o, mul_a_2_o, mul_a_3_o,
    output var logic signed [32:0] mul_b_0_o, mul_b_1_o, mul_b_2_o, mul_b_3_o,
    input  var logic               mul_valid_i,
    input  var logic signed [65:0] mul_p_0_i, mul_p_1_i, mul_p_2_i, mul_p_3_i
);

  localparam int LANES = 4;

  localparam logic [3:0] P_IDLE = 4'd0;
  localparam logic [3:0] P_H1   = 4'd1;   // t * C3   (+ C2 in the mad)
  localparam logic [3:0] P_H1W  = 4'd2;
  localparam logic [3:0] P_H2   = 4'd3;   // t * u    (+ C1)
  localparam logic [3:0] P_H2W  = 4'd4;
  localparam logic [3:0] P_V    = 4'd5;   // t * u
  localparam logic [3:0] P_VW   = 4'd6;
  localparam logic [3:0] P_OUT  = 4'd7;
  // THE COEFFICIENTS NEED THEIR OWN CLOCK. They were formed in the state
  // that ISSUES t*C3, and a non-blocking assignment lands a clock later --
  // so the multiply went out against the PREVIOUS group's C3. Every value
  // was wrong and every value was still a plausible spline, because a stale
  // coefficient is a real coefficient for a different segment.
  localparam logic [3:0] P_COEF = 4'd8;

  logic [3:0] state;
  logic [7:0] h_tag;
  logic signed [31:0] h_t [LANES], h_p0 [LANES], h_p1 [LANES];
  logic signed [31:0] h_p2 [LANES], h_p3 [LANES];
  logic signed [31:0] c1 [LANES], c2 [LANES], c3 [LANES];
  logic signed [31:0] u  [LANES];
  logic [3:0] fired_mul, fired_add, fired_resc;

  // ---- the reference's arithmetic -----------------------------------------
  function automatic logic signed [31:0] sat32(input logic signed [63:0] v);
    begin
      if (v > 64'sd2147483647) sat32 = 32'sh7FFF_FFFF;
      else if (v < -64'sd2147483648) sat32 = 32'sh8000_0000;
      else sat32 = v[31:0];
    end
  endfunction

  function automatic logic sat32_fired(input logic signed [63:0] v);
    begin
      sat32_fired = (v > 64'sd2147483647) || (v < -64'sd2147483648);
    end
  endfunction

  // fx_mad: a*b + (c << 16), formed at full width and rescaled ONCE. The
  // product arrives from the bank; the addend is folded in here, which is what
  // keeps it to a single rounding.
  function automatic logic signed [31:0] mad_fin(input logic signed [65:0] p,
                                                 input logic signed [31:0] c);
    logic signed [66:0] s;
    logic signed [66:0] r;
    begin
      s = 67'(p) + (67'(c) <<< 16);
      r = (s + 67'sd32768) >>> 16;
      if (r > 67'sd2147483647) mad_fin = 32'sh7FFF_FFFF;
      else if (r < -67'sd2147483648) mad_fin = 32'sh8000_0000;
      else mad_fin = r[31:0];
    end
  endfunction

  function automatic logic mad_fired(input logic signed [65:0] p,
                                     input logic signed [31:0] c);
    logic signed [66:0] s;
    logic signed [66:0] r;
    begin
      s = 67'(p) + (67'(c) <<< 16);
      r = (s + 67'sd32768) >>> 16;
      mad_fired = (r > 67'sd2147483647) || (r < -67'sd2147483648);
    end
  endfunction

  function automatic logic signed [31:0] resc16(input logic signed [65:0] v);
    logic signed [66:0] r;
    begin
      r = (67'(v) + 67'sd32768) >>> 16;
      if (r > 67'sd2147483647) resc16 = 32'sh7FFF_FFFF;
      else if (r < -67'sd2147483648) resc16 = 32'sh8000_0000;
      else resc16 = r[31:0];
    end
  endfunction

  function automatic logic resc16_fired(input logic signed [65:0] v);
    logic signed [66:0] r;
    begin
      r = (67'(v) + 67'sd32768) >>> 16;
      resc16_fired = (r > 67'sd2147483647) || (r < -67'sd2147483648);
    end
  endfunction

  // The final half: rescale by ONE, which rounds half up and saturates. A
  // shift does neither.
  function automatic logic signed [31:0] resc1(input logic signed [31:0] v);
    logic signed [33:0] r;
    begin
      r = (34'(v) + 34'sd1) >>> 1;
      if (r > 34'sd2147483647) resc1 = 32'sh7FFF_FFFF;
      else if (r < -34'sd2147483648) resc1 = 32'sh8000_0000;
      else resc1 = r[31:0];
    end
  endfunction

  function automatic logic signed [31:0] add_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) + $signed({b[31], b});
      if (t > 33'sd2147483647) add_sat = 32'sh7FFF_FFFF;
      else if (t < -33'sd2147483648) add_sat = 32'sh8000_0000;
      else add_sat = t[31:0];
    end
  endfunction

  function automatic logic add_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) + $signed({b[31], b});
      add_fired = (t > 33'sd2147483647) || (t < -33'sd2147483648);
    end
  endfunction

  // ---- the bank's operands ------------------------------------------------
  logic signed [32:0] mul_a [LANES], mul_b [LANES];
  logic signed [65:0] prod  [LANES];
  assign prod[0] = mul_p_0_i;
  assign prod[1] = mul_p_1_i;
  assign prod[2] = mul_p_2_i;
  assign prod[3] = mul_p_3_i;

  always_comb begin
    for (int l = 0; l < LANES; l++) begin
      mul_a[l] = 33'(h_t[l]);
      case (state)
        P_H1:    mul_b[l] = 33'(c3[l]);
        P_H2:    mul_b[l] = 33'(u[l]);
        default: mul_b[l] = 33'(u[l]);
      endcase
    end
  end

  assign mul_issue_o = (state == P_H1) || (state == P_H2) || (state == P_V);
  assign mul_a_0_o = mul_a[0];
  assign mul_a_1_o = mul_a[1];
  assign mul_a_2_o = mul_a[2];
  assign mul_a_3_o = mul_a[3];
  assign mul_b_0_o = mul_b[0];
  assign mul_b_1_o = mul_b[1];
  assign mul_b_2_o = mul_b[2];
  assign mul_b_3_o = mul_b[3];

  assign v_ready_o = (state == P_IDLE);
  assign tag_o     = h_tag;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= P_IDLE;
      h_tag <= 8'd0;
      fired_mul <= 4'b0;
      fired_add <= 4'b0;
      fired_resc <= 4'b0;
      r_valid_o <= 1'b0;
      sat_mul_o <= 4'b0;
      sat_add_o <= 4'b0;
      sat_rescale_o <= 4'b0;
      for (int l = 0; l < LANES; l++) begin
        h_t[l] <= '0;
        h_p0[l] <= '0;
        h_p1[l] <= '0;
        h_p2[l] <= '0;
        h_p3[l] <= '0;
        c1[l] <= '0;
        c2[l] <= '0;
        c3[l] <= '0;
        u[l] <= '0;
      end
      o0_0_o <= '0;
      o0_1_o <= '0;
      o0_2_o <= '0;
      o0_3_o <= '0;
    end else begin
      case (state)
        P_IDLE: begin
          if (v_valid_i) begin
            h_tag <= tag_i;
            h_t[0] <= t_0_i;  h_t[1] <= t_1_i;  h_t[2] <= t_2_i;  h_t[3] <= t_3_i;
            h_p0[0] <= p0_0_i; h_p0[1] <= p0_1_i; h_p0[2] <= p0_2_i; h_p0[3] <= p0_3_i;
            h_p1[0] <= p1_0_i; h_p1[1] <= p1_1_i; h_p1[2] <= p1_2_i; h_p1[3] <= p1_3_i;
            h_p2[0] <= p2_0_i; h_p2[1] <= p2_1_i; h_p2[2] <= p2_2_i; h_p2[3] <= p2_3_i;
            h_p3[0] <= p3_0_i; h_p3[1] <= p3_1_i; h_p3[2] <= p3_2_i; h_p3[3] <= p3_3_i;
            fired_mul <= 4'b0;
            fired_add <= 4'b0;
            fired_resc <= 4'b0;
            sat_mul_o <= 4'b0;
            sat_add_o <= 4'b0;
            sat_rescale_o <= 4'b0;
            state <= P_COEF;
          end
        end

        // Law 1: the small multiples are formed at 64 bits and only the RESULT
        // is clamped. In 32 bits they would wrap on large control points and
        // give a smooth, wrong curve.
        P_COEF: begin
          begin
            for (int l = 0; l < LANES; l++) begin
              c1[l] <= sat32(64'(h_p2[l]) - 64'(h_p0[l]));
              c2[l] <= sat32(64'sd2 * 64'(h_p0[l]) - 64'sd5 * 64'(h_p1[l]) +
                             64'sd4 * 64'(h_p2[l]) - 64'(h_p3[l]));
              c3[l] <= sat32(-64'(h_p0[l]) + 64'sd3 * 64'(h_p1[l]) -
                             64'sd3 * 64'(h_p2[l]) + 64'(h_p3[l]));
              if (sat32_fired(64'(h_p2[l]) - 64'(h_p0[l])) ||
                  sat32_fired(64'sd2 * 64'(h_p0[l]) - 64'sd5 * 64'(h_p1[l]) +
                              64'sd4 * 64'(h_p2[l]) - 64'(h_p3[l])) ||
                  sat32_fired(-64'(h_p0[l]) + 64'sd3 * 64'(h_p1[l]) -
                              64'sd3 * 64'(h_p2[l]) + 64'(h_p3[l])))
                fired_resc[l] <= 1'b1;
            end
            state <= P_H1;
          end
        end

        P_H1: if (mul_ready_i) state <= P_H1W;
        P_H1W: if (mul_valid_i) begin
          // Law 2: ONE rounding. The addend is folded in at full width.
          for (int l = 0; l < LANES; l++) begin
            u[l] <= mad_fin(prod[l], c2[l]);
            if (mad_fired(prod[l], c2[l])) fired_mul[l] <= 1'b1;
          end
          state <= P_H2;
        end

        P_H2: if (mul_ready_i) state <= P_H2W;
        P_H2W: if (mul_valid_i) begin
          for (int l = 0; l < LANES; l++) begin
            u[l] <= mad_fin(prod[l], c1[l]);
            if (mad_fired(prod[l], c1[l])) fired_mul[l] <= 1'b1;
          end
          state <= P_V;
        end

        P_V: if (mul_ready_i) state <= P_VW;
        P_VW: if (mul_valid_i) begin
          // The flags are per lane and the outputs are named, so the loop does
          // the former and the four assignments below do the latter.
          for (int l = 0; l < LANES; l++) begin
            if (resc16_fired(prod[l])) fired_mul[l] <= 1'b1;
            if (add_fired(h_p1[l], resc1(resc16(prod[l])))) fired_add[l] <= 1'b1;
          end
          // Law 3: the half is a RESCALE by one -- rounds half up and
          // saturates -- and then a saturating add onto p1.
          o0_0_o <= add_sat(h_p1[0], resc1(resc16(prod[0])));
          o0_1_o <= add_sat(h_p1[1], resc1(resc16(prod[1])));
          o0_2_o <= add_sat(h_p1[2], resc1(resc16(prod[2])));
          o0_3_o <= add_sat(h_p1[3], resc1(resc16(prod[3])));
          state <= P_OUT;
        end

        P_OUT: begin
          if (!r_valid_o) begin
            sat_mul_o <= fired_mul;
            sat_add_o <= fired_add;
            sat_rescale_o <= fired_resc;
            r_valid_o <= 1'b1;
          end else if (r_ready_i) begin
            r_valid_o <= 1'b0;
            state <= P_IDLE;
          end
        end

        default: state <= P_IDLE;
      endcase
    end
  end

endmodule : zhao_field_v3_spline
