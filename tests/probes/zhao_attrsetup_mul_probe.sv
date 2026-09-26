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
// READ VARIANT 0 FIRST, ALWAYS. It is the POSITIVE CONTROL: the PRE-REPAIR
// production arithmetic copied verbatim, and it reproduced the block on all
// seven measured numbers -- 45 DSP, 24 Independent 27x27, 15 Two Independent
// 18x18, 6 Sum of two 18x18, 225 registers, 940 combinational ALUTs, 468
// virtual pins -- against zhao_geom_attrsetup@gz-base, digest bd84da4c2517. A
// control arm that does not reproduce the block is measuring a different
// machine, and every number taken from the other arms is then about something
// else.
//
// *** ARM 0 IS NOW A BASELINE, NOT A MIRROR. DO NOT "REFRESH" IT. ***
//
// Production adopted arm 3's group-D form on 2026-09-26, so arm 0 is
// deliberately the arithmetic zhao_geom_attrsetup NO LONGER HAS. The usual law
// for a committed copy is the opposite of what applies here: a mutant copy goes
// stale in the flattering direction and must be three-way-merged forward, and
// tools/budget/mutant_copy_drift.py exists to catch exactly that. This file is
// not in its scope (it scans tests/mutants/) and must not be brought into it.
//
// Two things depend on arm 0 staying exactly as it is:
//   * every row in the eight-arm table was measured against it, so refreshing
//     it silently invalidates all eight;
//   * tests/proofs/attrsetup_mul_narrow_differential.cpp verilates it as
//     `Vattrsetup_old` and compares it against shipping production over 20,013
//     vectors. Refresh arm 0 and that differential becomes a comparison of the
//     new block with ITSELF -- green forever, proving nothing. That is the
//     broken-instrument law in its most flattering possible form.
//
// If production's group W or N or its output widths ever change, arm 0 stops
// being a positive control for the CURRENT block. That is fine and expected:
// its job is to be the 2026-09-26 baseline. Add a new arm; do not edit this one.
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
// ARMS 5, 6 AND 7 SPLIT ARM 3, which is the only one that moved and which
// changes TWO things at once. dndx negates INSIDE the wide cast,
// `(-(72'(cy_by))) * 72'(va_i)`; dndy does not, `72'(cx_bx) * 72'(va_i)`, and is
// the same plain shape as groups W and N, which arms 1 and 2 showed cost
// nothing. So "the width" and "the negation" are two hypotheses and arm 3
// cannot separate them:
//   5  dndy narrowed ONLY (22x32 -> 54 bits). No negation anywhere in it, so
//      this arm is a pure WIDTH question.
//   6  dndx narrowed ONLY (23x32 -> 55 bits). Width and negation together.
//   7  NOTHING NARROWED. Production widths throughout, and dndx negates the
//      PRODUCT instead of the operand: `-(72'(cy_by) * 72'(va_i))`. This is a
//      pure NEGATION-PLACEMENT question at the declared width, and it is
//      bit-identical because negation distributes over multiplication in
//      two's complement modulo 2^72.
// Between them, 5 and 7 decide it: if the cost is width, 5 pays and 7 does not;
// if the cost is the negation defeating sign-extension recognition, 7 pays and
// 5 does not.
//
// WHAT IT MEASURED, quartus_map 17.0.2, 5CSEBA6U23I7, ~14 s per arm. Rows are
// in reports/synthesis/zhao_block_fit.json as zhao_attrsetup_mul_probe@probe-mN
// with their own source digests, and summaries under reports/synthesis/blockpaths/:
//
//   arm  change from production             DSP  27x27  18x18pr  sum2  reg  ALUT
//   ---  --------------------------------   ---  -----  -------  ----  ---  ----
//    0   nothing (POSITIVE CONTROL)          45     24       15     6  225   940
//    1   W  46x46 -> 22x21                   45     24       15     6  225   940
//    2   N  96x96 -> 46x32                   45     24       15     6  225   940
//    3   D  72x72 -> 23x32 (both partials)   36     15       15     6  225   836
//    4   arms 1 + 2 + 3                      36     15       15     6  225   836
//    5   dndy only  72x72 -> 22x32           45     24       15     6  225   940
//    6   dndx only  72x72 -> 23x32           36     15       15     6  225   836
//    7   dndx negation moved OUT, 72 kept    36     15       15     6  225   887
//
// THE ANSWER, AND IT IS NOT THE ONE THE BRIEF PROPOSED. The declared widths are
// INNOCENT. Arms 1, 2 and 5 are the plain `WIDE'(narrow) * WIDE'(narrow)` shape
// and all three cost exactly nothing: Quartus 17.0.2 already strips that sign
// extension and was already multiplying at the true widths. Arm 7 names the
// mechanism -- it leaves every width at 72 and only moves the minus sign
// outside the multiply, and it recovers all nine blocks. So what costs is
// NEGATING THE SIGN-EXTENDED VALUE: `-(sext(x,72))` is a 72-bit subtract from
// zero, after which the top 50 bits are no longer a recognisable replication of
// bit 21 and Quartus must multiply a genuine 72-bit operand. dndy never negates
// and was never affected, which is why arm 5 is flat.
//
// Arm 6 (836 ALUTs) beats arm 7 (887) by 51 ALUTs for the same 9 blocks, so
// production took the narrowing rather than just relocating the minus sign.
// Arm 4 shows no interaction whatever: 1 + 2 + 3 is exactly 3.
//
// AND MY FIRST STORY WAS WRONG, WHICH IS THE REASON ARMS 1 AND 2 EXIST. From
// the baseline table alone there is a clean single-cause story: the six edge
// products are declared 46x46, 46 needs two 27-bit chunks, 2 x 2 = 4 chunk
// multiplies each, and 6 x 4 = 24 -- EXACTLY the 24 Independent 27x27 in the
// report. It is an arithmetic coincidence and it is false; arm 1 narrows those
// six products to their true 22x21 and the row does not move by one block.
//
// EVERY ARM IS BIT-IDENTICAL TO ARM 0 BY CONSTRUCTION, and that is the whole
// point: narrowing a MULTIPLY is legal, narrowing a RESULT is not. A signed
// m x n product needs exactly m+n bits, so 22x21->43, 46x32->78 and 23x32->55
// are EXACT, not truncating. The sign-extension back to 46 / 96 / 72 is free
// wire, the sums and the `<<< PIXEL_SHIFT` happen at the production width, and
// n0_o / dndx_o / dndy_o keep their declared widths to the bit.
//
// The 23 in group D is not a typo for 22, and the reason first written here was
// wrong. Negating inside 22 bits wraps at -2^21, and I claimed the declared port
// range reaches that. IT DOES NOT: cy_by is a 22-bit signed difference of two
// signed 21-bit coordinates, so its range is +/-(2^21 - 1) for any port values
// whatever, and 22 bits would also have been exact. 23 is kept because it makes
// the expression exact for the full DECLARED width of cy_by -- resting on
// cy_by's own type rather than on a range argument about its producer -- and
// because it is measured to cost nothing (arm 6 and arm 3 are identical).
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
  localparam bit NARROW_W  = (VARIANT == 1) || (VARIANT == 4);
  localparam bit NARROW_N  = (VARIANT == 2) || (VARIANT == 4);
  // Group D is two independent expressions and arms 5..7 separate them.
  localparam bit NARROW_DX = (VARIANT == 3) || (VARIANT == 4) || (VARIANT == 6);
  localparam bit NARROW_DY = (VARIANT == 3) || (VARIANT == 4) || (VARIANT == 5);
  // Production widths, negation moved out of the multiply. Arm 7 only.
  localparam bit DX_NEG_OUT = (VARIANT == 7);

  initial begin
    if (VARIANT > 7) begin
      $fatal(1, "zhao_attrsetup_mul_probe: VARIANT must be 0..7");
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

  // ---- the x partial: THE ONE WITH THE NEGATION -----------------------------
  generate
    if (NARROW_DX) begin : g_dx_narrow
      // The negation is taken at 23 bits, which is exact for every 22-bit
      // input -- see the header. 23 x 32 -> 55 bits is EXACT. The sum and the
      // shift stay at 72 bits.
      logic signed [22:0] ncy_by, nay_cy, nby_ay;
      logic signed [54:0] dxp0, dxp1, dxp2;
      always_comb begin
        ncy_by = -(23'(cy_by));
        nay_cy = -(23'(ay_cy));
        nby_ay = -(23'(by_ay));
        dxp0   = ncy_by * va_i;
        dxp1   = nay_cy * vb_i;
        dxp2   = nby_ay * vc_i;
        dndx_c = (72'(dxp0) + 72'(dxp1) + 72'(dxp2)) <<< PIXEL_SHIFT;
      end
    end else if (DX_NEG_OUT) begin : g_dx_negout
      // PRODUCTION WIDTHS, negation moved OUT of the multiply. Bit-identical:
      // -(a*b) == (-a)*b in two's complement modulo 2^72.
      always_comb begin
        dndx_c = (-(72'(cy_by) * 72'(va_i)) - (72'(ay_cy) * 72'(vb_i)) -
                  (72'(by_ay) * 72'(vc_i))) <<< PIXEL_SHIFT;
      end
    end else begin : g_dx_wide
      // PRODUCTION, VERBATIM. Declared 72 x 72; operands are 22 and 32.
      always_comb begin
        dndx_c = (((-(72'(cy_by)))) * 72'(va_i) + ((-(72'(ay_cy)))) * 72'(vb_i) +
                  ((-(72'(by_ay)))) * 72'(vc_i)) <<< PIXEL_SHIFT;
      end
    end
  endgenerate

  // ---- the y partial: THE ONE WITHOUT -------------------------------------
  generate
    if (NARROW_DY) begin : g_dy_narrow
      // 22 x 32 -> 54 bits is EXACT. The sum and the shift stay at 72 bits.
      logic signed [53:0] dyp0, dyp1, dyp2;
      always_comb begin
        dyp0   = cx_bx * va_i;
        dyp1   = ax_cx * vb_i;
        dyp2   = bx_ax * vc_i;
        dndy_c = (72'(dyp0) + 72'(dyp1) + 72'(dyp2)) <<< PIXEL_SHIFT;
      end
    end else begin : g_dy_wide
      // PRODUCTION, VERBATIM. Declared 72 x 72; operands are 22 and 32.
      always_comb begin
        dndy_c = (72'(cx_bx) * 72'(va_i) + 72'(ax_cx) * 72'(vb_i) +
                  72'(bx_ax) * 72'(vc_i)) <<< PIXEL_SHIFT;
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
