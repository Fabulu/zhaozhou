// zhao_field_ring.sv — the Field IR band op: OP_RING.
//
// A submodule of the FIELD.SEQ.* family. Reference: the interpreter's OP_RING
// case over `zref::smoothstep` (zref_trig.hpp) and `zref::field_rcp`
// (zref_rcp.hpp).
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     d = a, r0 = b, r1 = c
//     m  = rescale_s32(r0 + r1, 1)          the midpoint, EXACT in 64 bits
//     s0 = smoothstep(r0, m, d)
//     s1 = smoothstep(m, r1, d)
//     dst = fx_mul(s0, fx_sub(1.0, s1))
//
//     smoothstep(e0, e1, x):
//       dd = fx_sub(e1, e0)
//       r  = field_rcp(dd)
//       t  = fx_mul(fx_sub(x, e0), r)
//       t  = clamp(t, 0, 1)
//       t2 = fx_mul(t, t)
//       return fx_mul(t2, fx_sub(3.0, fx_mul(2.0, t)))
//
// A ring is a band: it rises across [r0, m] and falls across [m, r1], and the
// product of the two halves is what makes an annulus rather than a disc.
//
// Six things are load-bearing:
//
// 1. **THE MIDPOINT IS AN EXACT 64-BIT AVERAGE, NOT A SATURATING ADD.** The
//    reference uses `rescale_s32((int64_t)r0 + r1, 1)`, not `fx_add`. With
//    r0 = r1 = INT32_MAX the exact sum is 2^32 - 2 and the midpoint is
//    INT32_MAX — correct. An `fx_add` would saturate the SUM to INT32_MAX and
//    then halve it, giving a midpoint at half the radius and a ring in
//    completely the wrong place. The saturation is recorded in the `rescale`
//    lane, not `add`, for the same reason.
//
// 2. **THE RECIPROCAL IS `field_rcp`, INCLUDING ITS ZERO.** `r0 == r1` makes
//    both spans zero, and `field_rcp(0)` is the pinned 0x7FFFFFFF with a sticky
//    `rcp0`. A degenerate ring therefore has a DEFINED answer rather than an
//    undefined one, and the sticky lane is how anyone finds out it happened.
//
// 3. **THE CLAMP IS AFTER THE MULTIPLY, NOT BEFORE.** `t` is
//    `(x - e0) * (1/dd)` and only then clamped to [0, 1]. Clamping `x` to the
//    span first is the natural reading and gives different values wherever the
//    reciprocal is inexact — which is nearly everywhere.
//
// 4. **EACH `fx_mul` ROUNDS SEPARATELY.** The cubic is
//    `t2 * (3 - 2t)` with a rounding at every product, exactly as written. This
//    op is a chain of small multiplies and not a dot product; there is no row
//    to sum exactly.
//
// 5. **THE FALLING HALF IS `1 - s1`, NOT `smoothstep` REVERSED.** The second
//    smoothstep runs FORWARD across [m, r1] and is then subtracted from one.
//    Swapping its edges to make it fall directly is algebraically tempting and
//    rounds differently.
//
// 6. **FIVE LEDGER LANES.** `add` (four subtractions), `mul` (nine products),
//    `rescale` (the midpoint), and `rcp`/`rcp0` from the reciprocal itself. The
//    reference keeps them apart; a block that pools them can return every
//    number correctly and still misreport where the range went.
//
// ---------------------------------------------------------------------------
// ONE MULTIPLIER, ONE RECIPROCAL, ONE SMOOTHSTEP WALKED TWICE
// ---------------------------------------------------------------------------
// Nine 32x32 products and two reciprocals. The two smoothsteps are the SAME
// five states walked twice with different edges, and `zhao_field_rcp` is
// combinational so one instance serves both. Same trade as the rest of the
// engine: this is a per-sample field op, not a per-pixel path.
module zhao_field_ring (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic signed [31:0] d_i,    // reg[a]: the distance being banded
    input  logic signed [31:0] r0_i,   // reg[b]: inner radius
    input  logic signed [31:0] r1_i,   // reg[c]: outer radius

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] result_o,
    output logic               sat_add_o,
    output logic               sat_mul_o,
    output logic               sat_rescale_o,
    output logic               sat_rcp_o,
    output logic               rcp0_o
);

  localparam logic signed [31:0] FX_ONE = 32'sh0001_0000;
  localparam logic signed [31:0] FX_TWO = 32'sh0002_0000;
  localparam logic signed [31:0] FX_THREE = 32'sh0003_0000;

  localparam logic [3:0] G_IDLE = 4'd0;
  localparam logic [3:0] G_MID = 4'd1;    // the midpoint
  localparam logic [3:0] G_SPAN = 4'd2;   // dd = e1 - e0, and its reciprocal
  localparam logic [3:0] G_T = 4'd3;      // t = (x - e0) * r, then clamped
  localparam logic [3:0] G_T2 = 4'd4;     // t2 = t * t
  localparam logic [3:0] G_2T = 4'd5;     // u  = 3 - 2t
  localparam logic [3:0] G_CUBE = 4'd6;   // s  = t2 * u
  localparam logic [3:0] G_FIN = 4'd7;    // s0 * (1 - s1)
  localparam logic [3:0] G_OUT = 4'd8;

  logic [3:0] state;
  logic       half;   // 0 = the rising half, 1 = the falling half

  logic signed [31:0] h_d, h_r0, h_r1;
  logic signed [31:0] m_val;
  logic signed [31:0] e0, e1;
  logic signed [31:0] rcp_v, t_val, t2_val, u_val, s0_val, s1_val;

  // ---- the one reciprocal, walked ----------------------------------------
  logic signed [31:0] span;
  logic signed [31:0] rcp_out;
  logic               rcp_sat, rcp_zero;
  assign span = sub_sat(e1, e0);

  zhao_field_rcp u_rcp (
      .a_i      (span),
      .result_o (rcp_out),
      .sat_rcp_o(rcp_sat),
      .rcp0_o   (rcp_zero)
  );

  // ---- the one multiplier -------------------------------------------------
  logic signed [31:0] mul_a, mul_b;
  logic signed [63:0] mul_p;
  assign mul_p = mul_a * mul_b;

  always_comb begin
    case (state)
      G_T: begin
        mul_a = sub_sat(h_d, e0);
        mul_b = rcp_v;
      end
      G_T2: begin
        mul_a = t_val;
        mul_b = t_val;
      end
      G_2T: begin
        mul_a = FX_TWO;
        mul_b = t_val;
      end
      G_CUBE: begin
        mul_a = t2_val;
        mul_b = u_val;
      end
      default: begin
        // G_FIN: s0 * (1 - s1). Law 5: the falling half is ONE MINUS a forward
        // smoothstep, not a smoothstep with its edges swapped.
        mul_a = s0_val;
        mul_b = sub_sat(FX_ONE, s1_val);
      end
    endcase
  end

  // ---- the primitives, each in its own lane -------------------------------
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

  // Law 4: ONE rescale per product, round-half-up, saturating into `mul`.
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

  // Law 1: the midpoint. An EXACT 33-bit sum rescaled by one, so the average of
  // two extremes is the extreme and not half of it.
  logic signed [32:0] sum33;
  logic signed [33:0] mid_r;
  logic signed [31:0] mid_val;
  logic               mid_fired;
  assign sum33 = $signed({h_r0[31], h_r0}) + $signed({h_r1[31], h_r1});
  assign mid_r = ($signed({sum33[32], sum33}) + 34'sd1) >>> 1;
  assign mid_val = (mid_r > 34'sd2147483647)  ? 32'sh7FFF_FFFF :
                   (mid_r < -34'sd2147483648) ? 32'sh8000_0000 : mid_r[31:0];
  assign mid_fired = (mid_r > 34'sd2147483647) || (mid_r < -34'sd2147483648);

  // Law 3: the clamp is applied to the PRODUCT, after the multiply.
  logic signed [31:0] t_raw;
  assign t_raw = resc16(mul_p);

  assign v_ready_o = (state == G_IDLE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= G_IDLE;
      half <= 1'b0;
      h_d <= '0;
      h_r0 <= '0;
      h_r1 <= '0;
      m_val <= '0;
      e0 <= '0;
      e1 <= '0;
      rcp_v <= '0;
      t_val <= '0;
      t2_val <= '0;
      u_val <= '0;
      s0_val <= '0;
      s1_val <= '0;
      result_o <= '0;
      r_valid_o <= 1'b0;
      sat_add_o <= 1'b0;
      sat_mul_o <= 1'b0;
      sat_rescale_o <= 1'b0;
      sat_rcp_o <= 1'b0;
      rcp0_o <= 1'b0;
    end else begin
      case (state)
        G_IDLE: begin
          if (v_valid_i) begin
            h_d <= d_i;
            h_r0 <= r0_i;
            h_r1 <= r1_i;
            half <= 1'b0;
            sat_add_o <= 1'b0;
            sat_mul_o <= 1'b0;
            sat_rescale_o <= 1'b0;
            sat_rcp_o <= 1'b0;
            rcp0_o <= 1'b0;
            state <= G_MID;
          end
        end

        G_MID: begin
          // Law 1: the `rescale` lane, not `add`.
          m_val <= mid_val;
          sat_rescale_o <= mid_fired;
          e0 <= h_r0;
          e1 <= mid_val;
          state <= G_SPAN;
        end

        // Law 2: the reciprocal is field_rcp, zero and all. A degenerate ring
        // gets a defined answer and a sticky lane that says so.
        G_SPAN: begin
          rcp_v <= rcp_out;
          sat_add_o <= sat_add_o || sub_fired(e1, e0);
          sat_rcp_o <= sat_rcp_o || rcp_sat;
          rcp0_o <= rcp0_o || rcp_zero;
          state <= G_T;
        end

        G_T: begin
          // Law 3: clamp the PRODUCT.
          t_val <= (t_raw < 32'sd0) ? 32'sd0 : ((t_raw > FX_ONE) ? FX_ONE : t_raw);
          sat_add_o <= sat_add_o || sub_fired(h_d, e0);
          sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
          state <= G_T2;
        end

        G_T2: begin
          t2_val <= resc16(mul_p);
          sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
          state <= G_2T;
        end

        G_2T: begin
          u_val <= sub_sat(FX_THREE, resc16(mul_p));
          sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
          sat_add_o <= sat_add_o || sub_fired(FX_THREE, resc16(mul_p));
          state <= G_CUBE;
        end

        G_CUBE: begin
          sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
          if (half == 1'b0) begin
            s0_val <= resc16(mul_p);
            // The falling half: forward across [m, r1].
            half <= 1'b1;
            e0 <= m_val;
            e1 <= h_r1;
            state <= G_SPAN;
          end else begin
            s1_val <= resc16(mul_p);
            state <= G_FIN;
          end
        end

        G_FIN: begin
          result_o <= resc16(mul_p);
          sat_mul_o <= sat_mul_o || resc16_fired(mul_p);
          sat_add_o <= sat_add_o || sub_fired(FX_ONE, s1_val);
          r_valid_o <= 1'b1;
          state <= G_OUT;
        end

        G_OUT: begin
          if (r_ready_i) begin
            r_valid_o <= 1'b0;
            state <= G_IDLE;
          end
        end

        default: state <= G_IDLE;
      endcase
    end
  end

endmodule : zhao_field_ring
