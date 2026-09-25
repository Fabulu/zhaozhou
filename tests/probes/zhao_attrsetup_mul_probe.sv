// zhao_attrsetup_mul_probe -- the ATTRSETUP packet's one-change-at-a-time instrument.
//
// WHY THIS FILE EXISTS
// --------------------
// `zhao_geom_attrsetup` costs 45 DSP blocks -- 40% of the entire shipping
// 5CSEBA6U23I7 -- from 164 lines and 225 registers. Measured standalone,
// `zhao_geom_attrsetup@dsp-census-20260926` and `@gz-base`, both digest
// bd84da4c2517:
//
//     Total DSP Blocks      45      Two Independent 18x18   15
//     Total registers      225      Sum of two 18x18         6
//     combinational ALUTs  940      Independent 27x27       24
//
// The hypothesis under test, from BRIEF-ATTRSETUP.md and HANDOVER §15.23:
// EVERY multiply in the block is written at a declared width far above what
// its operands can hold, and DSP inference follows declared width.
//
//     w0_0   = -(46'(cx_bx) * 46'(by_i)) + ...      operands 22 and 21
//     n0_c   =  96'(w0_0)  * 96'(va_i)  + ...       operands 46 and 32
//     dndx_c = (-(72'(cy_by))) * 72'(va_i) + ...    operands 22 and 32
//
// "I can see a wide literal" is not a measurement, and the brief says so.
// Quartus strips redundant sign extension in some shapes and not others, and
// WHICH shapes is exactly what this probe is for.
//
// THE LAW THIS PROBE EXISTS TO RESPECT: "a probe that does this was written
// once and thrown away, so its numbers are unreproducible -- commit the
// probe." (CLAUDE.md, ground contact.) PALRAM committed its five arms for the
// same reason, and its first single-cause story was REFUTED by a control it
// had already predicted. Run every arm even when you think you have it.
//
// HOW TO USE IT
// -------------
//   tools\quartus\attrsetup_map_probe.ps1 -Label '@probe-m0' -TopParameters VARIANT=0
//
// READ VARIANT 0 FIRST, ALWAYS. It is the POSITIVE CONTROL: the production
// arithmetic copied verbatim, and it must reproduce 45 DSP / 225 registers.
// A control arm that does not reproduce the block is measuring a different
// machine, and every number taken from the other arms is then about something
// else.
//
// THE ARMS. Each differs from ARM 0 by exactly one multiply group:
//   0  production, verbatim.                                POSITIVE CONTROL
//   1  group W narrowed: the six edge products multiply at 22x21 into 43 bits,
//      then sign-extend into the same 46-bit accumulation.
//   2  group N narrowed: the three attribute products multiply at 46x32 into
//      78 bits, then sign-extend into the same 96-bit sum.
//   3  group D narrowed: the six partial products multiply at 23x32 into 55
//      bits, then sign-extend into the same 72-bit sum and the same shift.
//   4  arms 1, 2 and 3 together -- meaningful only once each is measured
//      alone, and present so the INTERACTION can be read rather than assumed.
//
// EVERY ARM IS BIT-IDENTICAL TO ARM 0 BY CONSTRUCTION, and that is the whole
// point: narrowing a MULTIPLY is legal, narrowing a RESULT is not. A signed
// m x n product needs exactly m+n bits, so 22x21->43, 46x32->78 and 23x32->55
// are EXACT, not truncating. The sign-extension back to 46 / 96 / 72 is free
// wire, the sums and the `<<< PIXEL_SHIFT` happen at the production width, and
// n0_o / dndx_o / dndy_o keep their declared widths to the bit.
//
// The 23 in group D is not a typo for 22. Production negates the 22-bit
// difference INSIDE a 72-bit cast, which is exact for every 22-bit input;
// negating inside 22 bits would wrap at -2^21. 23 bits reproduces production
// exactly for the full declared input range, not merely for the range GEOM.CLIP
// actually delivers. Differential stimulus drives these ports directly, so the
// declared range is the one that has to match.
//
// Quartus 17.0 syntax law, both halves: explicit `generate`/`endgenerate` (an
// implicit generate is a syntax error there) and elaboration guards inside
// `initial begin ... end` (a module-scope `if` is a syntax error there, and
// `--lint-only` never runs the block, so a clean lint is no evidence about it).
//
// This file is a PROBE, not production. Nothing composes it and nothing may.
`default_nettype none

module zhao_attrsetup_mul_probe #(
    parameter int unsigned VARIANT = 0
) (
    input var logic clk,
    input var logic rst_n,

    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [20:0] ax_i, ay_i,
    input  var logic signed [20:0] bx_i, by_i,
    input  var logic signed [20:0] cx_i, cy_i,
    input  var logic signed [31:0] va_i, vb_i, vc_i,

    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [95:0] n0_o,
    output var logic signed [71:0] dndx_o,
    output var logic signed [71:0] dndy_o
);

  localparam int unsigned PIXEL_SHIFT = 8;

  // Which multiply group this arm narrows. Derived from VARIANT so that the
  // arm number is the only thing a driver has to get right, and so that the
  // map report's Parameter Settings table states which arm produced it.
  localparam bit NARROW_W = (VARIANT == 1) || (VARIANT == 4);
  localparam bit NARROW_N = (VARIANT == 2) || (VARIANT == 4);
  localparam bit NARROW_D = (VARIANT == 3) || (VARIANT == 4);

  initial begin
    if (VARIANT > 4) begin
      $fatal(1, "zhao_attrsetup_mul_probe: VARIANT must be 0..4");
    end
  end

  // ---- the six partials, identical in every arm -----------------------------
  logic signed [21:0] cy_by, ay_cy, by_ay;
  logic signed [21:0] cx_bx, ax_cx, bx_ax;
  always_comb begin
    cy_by = 22'(cy_i) - 22'(by_i);
    ay_cy = 22'(ay_i) - 22'(cy_i);
    by_ay = 22'(by_i) - 22'(ay_i);
    cx_bx = 22'(cx_i) - 22'(bx_i);
    ax_cx = 22'(ax_i) - 22'(cx_i);
    bx_ax = 22'(bx_i) - 22'(ax_i);
  end

  // ---- GROUP W: the three w at the origin ----------------------------------
  logic signed [45:0] w0_0, w1_0, w2_0;

  generate
    if (!NARROW_W) begin : g_w_wide
      // PRODUCTION, VERBATIM. Declared 46 x 46; operands are 22 and 21.
      always_comb begin
        w0_0 = -(46'(cx_bx) * 46'(by_i)) + (46'(cy_by) * 46'(bx_i));
        w1_0 = -(46'(ax_cx) * 46'(cy_i)) + (46'(ay_cy) * 46'(cx_i));
        w2_0 = -(46'(bx_ax) * 46'(ay_i)) + (46'(by_ay) * 46'(ax_i));
      end
    end else begin : g_w_narrow
      // 22 x 21 -> 43 bits is EXACT. The accumulation stays at 46 bits, so the
      // emitted w is the same 46-bit value production emits.
      logic signed [42:0] wp0a, wp0b, wp1a, wp1b, wp2a, wp2b;
      always_comb begin
        wp0a = cx_bx * by_i;
        wp0b = cy_by * bx_i;
        wp1a = ax_cx * cy_i;
        wp1b = ay_cy * cx_i;
        wp2a = bx_ax * ay_i;
        wp2b = by_ay * ax_i;
        w0_0 = -(46'(wp0a)) + 46'(wp0b);
        w1_0 = -(46'(wp1a)) + 46'(wp1b);
        w2_0 = -(46'(wp2a)) + 46'(wp2b);
      end
    end
  endgenerate

  // ---- GROUP N: the numerator at the origin --------------------------------
  logic signed [95:0] n0_c;

  generate
    if (!NARROW_N) begin : g_n_wide
      // PRODUCTION, VERBATIM. Declared 96 x 96; operands are 46 and 32.
      always_comb begin
        n0_c = 96'(w0_0) * 96'(va_i) + 96'(w1_0) * 96'(vb_i) + 96'(w2_0) * 96'(vc_i);
      end
    end else begin : g_n_narrow
      // 46 x 32 -> 78 bits is EXACT. The sum stays at 96 bits.
      logic signed [77:0] np0, np1, np2;
      always_comb begin
        np0  = w0_0 * va_i;
        np1  = w1_0 * vb_i;
        np2  = w2_0 * vc_i;
        n0_c = 96'(np0) + 96'(np1) + 96'(np2);
      end
    end
  endgenerate

  // ---- GROUP D: the two partials of the numerator plane --------------------
  // The x partial of w is the NEGATED y difference and the y partial is the x
  // difference. The asymmetry is orient's, not a transcription slip, and it is
  // reproduced exactly in both arms.
  logic signed [71:0] dndx_c, dndy_c;

  generate
    if (!NARROW_D) begin : g_d_wide
      // PRODUCTION, VERBATIM. Declared 72 x 72; operands are 22 and 32.
      always_comb begin
        dndx_c = (((-(72'(cy_by)))) * 72'(va_i) + ((-(72'(ay_cy)))) * 72'(vb_i) +
                  ((-(72'(by_ay)))) * 72'(vc_i)) <<< PIXEL_SHIFT;
        dndy_c = (72'(cx_bx) * 72'(va_i) + 72'(ax_cx) * 72'(vb_i) +
                  72'(bx_ax) * 72'(vc_i)) <<< PIXEL_SHIFT;
      end
    end else begin : g_d_narrow
      // The negation is taken at 23 bits, which is exact for every 22-bit
      // input -- see the header. 23 x 32 -> 55 bits is EXACT. The sum and the
      // shift stay at 72 bits.
      logic signed [22:0] ncy_by, nay_cy, nby_ay;
      logic signed [54:0] dxp0, dxp1, dxp2, dyp0, dyp1, dyp2;
      always_comb begin
        ncy_by = -(23'(cy_by));
        nay_cy = -(23'(ay_cy));
        nby_ay = -(23'(by_ay));
        dxp0   = ncy_by * va_i;
        dxp1   = nay_cy * vb_i;
        dxp2   = nby_ay * vc_i;
        dyp0   = cx_bx * va_i;
        dyp1   = ax_cx * vb_i;
        dyp2   = bx_ax * vc_i;
        dndx_c = (72'(dxp0) + 72'(dxp1) + 72'(dxp2)) <<< PIXEL_SHIFT;
        dndy_c = (72'(dyp0) + 72'(dyp1) + 72'(dyp2)) <<< PIXEL_SHIFT;
      end
    end
  endgenerate

  // ---- one in flight, identical in every arm -------------------------------
  assign v_ready_o = !r_valid_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r_valid_o <= 1'b0;
      n0_o      <= '0;
      dndx_o    <= '0;
      dndy_o    <= '0;
    end else begin
      if (v_valid_i && v_ready_o) begin
        n0_o      <= n0_c;
        dndx_o    <= dndx_c;
        dndy_o    <= dndy_c;
        r_valid_o <= 1'b1;
      end else if (r_valid_o && r_ready_i) begin
        r_valid_o <= 1'b0;
      end
    end
  end

endmodule : zhao_attrsetup_mul_probe

`default_nettype wire
