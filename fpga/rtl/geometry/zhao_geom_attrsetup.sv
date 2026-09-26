// zhao_geom_attrsetup.sv — GEOM.ATTRSETUP: one attribute's interpolation plane.
//
// ENFORCED-BY: tests/geometry/geom_attrsetup_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT IT EMITS, AND WHY THAT IS EXACTLY THE ORACLE
// ---------------------------------------------------------------------------
// `reference/src/zrender/rast.cpp` interpolates every attribute as
//
//     attr(x,y) = round_half_up( (w0*va + w1*vb + w2*vc) / area )
//
// with a 128-bit numerator and a divide per attribute per pixel. That reads
// like a different law from `spec/qformats.md`'s "interpolate by plane
// equation", and for one commit this project believed it was.
//
// It is not. The edge functions step by CONSTANTS -- rast.cpp's own
// `w0 += dw0_dx` -- so the NUMERATOR is itself an exact integer plane:
//
//     N(x,y) = N0 + x*dNdx + y*dNdy       with no rounding in the stepping
//
// and the divide applied to the stepped numerator gives the same bits as the
// divide applied to a recomputed one. Proved over 32,805 pixel-attributes and
// five triangle shapes in tests/proofs/attribute_plane_equivalence.cpp.
//
// So this block emits the plane of the NUMERATOR, and the divide stays
// downstream, per pixel. "Interpolate by plane equation" was always describing
// the numerator; nothing in the reference has to move.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC, TERM BY TERM
// ---------------------------------------------------------------------------
// With the edge functions the raster uses,
//
//     w0(P) = orient(B, C, P)    w1(P) = orient(C, A, P)    w2(P) = orient(A, B, P)
//     orient(U, V, P) = (V.x - U.x)*(P.y - U.y) - (V.y - U.y)*(P.x - U.x)
//
// each w is linear in P with constant partials, so
//
//     dw0/dx = -(C.y - B.y)      dw0/dy = (C.x - B.x)
//     dw1/dx = -(A.y - C.y)      dw1/dy = (A.x - C.x)
//     dw2/dx = -(B.y - A.y)      dw2/dy = (B.x - A.x)
//
// and the numerator plane is the same combination of them:
//
//     N0   = w0(0,0)*va + w1(0,0)*vb + w2(0,0)*vc
//     dNdx = (dw0/dx*va + dw1/dx*vb + dw2/dx*vc) * PIXEL
//     dNdy = (dw0/dy*va + dw1/dy*vb + dw2/dy*vc) * PIXEL
//
// PIXEL is 256 because screen coordinates carry EIGHT fractional bits, so one
// pixel of x is 256 coordinate units -- rast.cpp scales its own `dw0_dx` by the
// same 256. Getting that wrong makes every gradient 256x too small and every
// triangle flat, which is the same units mistake this project has already made
// once in a test.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT DO
// ---------------------------------------------------------------------------
// It does not divide. It does not know what the attribute MEANS -- `invw24`,
// `u_over_w`, `v_over_w`, colour and alpha all take the same plane and differ
// only in what the caller does with the quotient. It does not clip, cull, or
// normalise winding: GEOM.CLIP has already made area > 0 and swapped B/C, and
// this block would produce a sign-flipped plane for a back-facing triangle
// exactly as the oracle would.
//
// ONE ATTRIBUTE PER REQUEST. A textured Gouraud triangle needs seven planes and
// asks seven times. That keeps the block small and makes the attribute COUNT a
// scheduling decision upstream rather than a width decision here -- which is
// what lets early-Z pay for only `invw24` and leave `u`/`v` to survivors.
`default_nettype none

module zhao_geom_attrsetup (
    input var logic clk,
    input var logic rst_n,

    // ---- one triangle, one attribute --------------------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    // Winding-normalised screen vertices, S 12.8, the same 21-bit canvas
    // coordinates GEOM.CLIP emits.
    input  var logic signed [20:0] ax_i, ay_i,
    input  var logic signed [20:0] bx_i, by_i,
    input  var logic signed [20:0] cx_i, cy_i,
    // The attribute at each vertex. S 8.24 for u_over_w and v_over_w; the same
    // port carries invw24, colour and alpha, which is the point of not naming
    // the attribute here.
    input  var logic signed [31:0] va_i, vb_i, vc_i,

    // ---- the plane of the NUMERATOR ---------------------------------------
    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    // Bounds: a coordinate difference is at most 2^22 and an attribute 2^31, so
    // a w is under 2^45 and a term under 2^76; three of them need 78 bits. 96
    // is carried so the widths are obviously sufficient rather than exactly
    // sufficient -- this block is once per triangle, not per pixel.
    //
    // THESE THREE WIDTHS ARE THE SPECIFICATION AND THEY ARE NOT KNOBS. The
    // block's entire justification is that it emits EXACTLY the oracle's
    // numerator, proved over 32,805 pixel-attributes. Narrowing a MULTIPLY is
    // legal; narrowing a RESULT is not. The 2026-09-26 DSP repair narrowed
    // multiplies only -- see "THE PARTIALS MULTIPLY AT THEIR TRUE WIDTHS"
    // below, which also records that the 96 here costs ZERO DSP blocks and was
    // measured to, so nobody needs to come back and shrink it.
    output var logic signed [95:0] n0_o,
    output var logic signed [71:0] dndx_o,
    output var logic signed [71:0] dndy_o
);

  // One pixel of x or y, in the S 12.8 coordinate units the canvas uses.
  localparam int unsigned PIXEL_SHIFT = 8;

  // ---- the six partials and the three w at the origin ----------------------
  // All combinational from the input pins, and registered below on accept: this
  // is a setup block that runs once per triangle, so the depth is affordable
  // where it would not be on a per-pixel path.
  logic signed [21:0] cy_by, ay_cy, by_ay;   // the x partials, negated below
  logic signed [21:0] cx_bx, ax_cx, bx_ax;   // the y partials
  always_comb begin
    cy_by = 22'(cy_i) - 22'(by_i);
    ay_cy = 22'(ay_i) - 22'(cy_i);
    by_ay = 22'(by_i) - 22'(ay_i);
    cx_bx = 22'(cx_i) - 22'(bx_i);
    ax_cx = 22'(ax_i) - 22'(cx_i);
    bx_ax = 22'(bx_i) - 22'(ax_i);
  end

  // orient(U, V, origin) = (V.x - U.x)*(0 - U.y) - (V.y - U.y)*(0 - U.x)
  //                      = -(V.x - U.x)*U.y + (V.y - U.y)*U.x
  logic signed [45:0] w0_0, w1_0, w2_0;
  always_comb begin
    w0_0 = -(46'(cx_bx) * 46'(by_i)) + (46'(cy_by) * 46'(bx_i));
    w1_0 = -(46'(ax_cx) * 46'(cy_i)) + (46'(ay_cy) * 46'(cx_i));
    w2_0 = -(46'(bx_ax) * 46'(ay_i)) + (46'(by_ay) * 46'(ax_i));
  end

  // ---- the plane ----------------------------------------------------------
  logic signed [95:0] n0_c;
  logic signed [71:0] dndx_c, dndy_c;

  // THE PARTIALS MULTIPLY AT THEIR TRUE WIDTHS. THAT IS WORTH 9 DSP BLOCKS --
  // 45 -> 36, 8% of the entire shipping 5CSEBA6U23I7 -- AND ZERO BITS.
  //
  // Until 2026-09-26 both partials were written `(-(72'(cy_by))) * 72'(va_i)`,
  // i.e. sign-extend the 22-bit difference to 72, negate it there, and multiply
  // 72 x 72. The whole file was written that way on purpose: "96 is carried so
  // the widths are obviously sufficient rather than exactly sufficient -- this
  // block is once per triangle, not per pixel." That trade is correct against
  // ALUTs and latency and it is why the block is under a thousand ALUTs. What
  // nobody measured is where the cost landed, and it landed on the DSP ceiling,
  // which the failed console fit breaches by 335%.
  //
  // BUT THE DECLARED WIDTH IS NOT WHAT COSTS, AND THAT MATTERS FOR EVERY OTHER
  // BLOCK SOMEBODY IS ABOUT TO "FIX". Measured one change at a time with
  // tests/probes/zhao_attrsetup_mul_probe.sv, quartus_map 17.0.2, ~14 s a run:
  //
  //   arm  change from production                          DSP  27x27  ALUT
  //    0   nothing (POSITIVE CONTROL)                        45     24   940
  //    1   w0_0..w2_0  46x46 -> 22x21                        45     24   940
  //    2   n0_c        96x96 -> 46x32                        45     24   940
  //    5   dndy only   72x72 -> 22x32                        45     24   940
  //    6   dndx only   72x72 -> 23x32                        36     15   836
  //    7   dndx NEGATION MOVED OUT, widths left at 72        36     15   887
  //    3   both partials narrowed (SHIPPED HERE)             36     15   836
  //
  // Arms 1, 2 and 5 are the same plain `WIDE'(narrow) * WIDE'(narrow)` shape and
  // ALL THREE COST NOTHING: Quartus 17.0.2 already strips that sign extension
  // and was already multiplying at the true widths. Arm 7 is the one that names
  // the mechanism -- it leaves every declared width at 72 and merely moves the
  // minus sign outside the multiply, and it recovers all nine blocks. So the
  // cost is NEGATING THE SIGN-EXTENDED VALUE: `-(sext(x,72))` is a 72-bit
  // subtract from zero, after which the top 50 bits are no longer a recognisable
  // replication of bit 21 and Quartus must multiply a genuine 72-bit operand.
  // dndy, which never negates, was never affected. The rule to carry away is
  // that a wide CAST is free and an ARITHMETIC OPERATION applied to the widened
  // value before the multiply is not.
  //
  // 23 IS NOT A TYPO FOR 22, AND MY FIRST REASON FOR IT WAS WRONG. Negating
  // inside 22 bits wraps at -2^21, and I wrote here that the ports' declared
  // range reaches that value. IT DOES NOT: cy_by is a 22-bit SIGNED difference
  // of two signed 21-bit coordinates, so its range is +/-(2^21 - 1) for ANY
  // port values whatever, not merely for the ones GEOM.CLIP delivers -- the
  // -2^21 corner of its declared type is unreachable from the pins. 22 bits
  // would therefore also have been exact.
  //
  // 23 is kept for two honest reasons and not the wrong one. It makes the
  // expression exact for the full DECLARED width of cy_by, so its correctness
  // rests on cy_by's own type rather than on a range argument about the block
  // that produces it -- and that argument is exactly the kind that stops being
  // true when somebody widens a coordinate. And it costs NOTHING: probe arm 6
  // (23 bits) and arm 3 (23 bits) both measure 36 DSP / 836 ALUTs, identical.
  //
  // NOTHING ABOUT THE EMITTED VALUE MOVED. A signed m x n product needs exactly
  // m + n bits, so 23 x 32 -> 55 and 22 x 32 -> 54 are EXACT and not truncating;
  // the sign-extension back to 72 is wire; the three-way sum and the
  // `<<< PIXEL_SHIFT` still happen at 72 bits; and n0_o, dndx_o and dndy_o keep
  // their declared widths to the bit. PROVED, not asserted:
  // tests/proofs/attribute_plane_equivalence.cpp and
  // tests/geometry/geom_attrsetup_directed.cpp, which gains a differential that
  // runs the old 72-bit expressions and these side by side on the same stimulus.
  logic signed [22:0] ncy_by, nay_cy, nby_ay;  // negated x partials, exact at 23
  logic signed [54:0] dxp0, dxp1, dxp2;        // 23 x 32 -> 55 bits, exact
  logic signed [53:0] dyp0, dyp1, dyp2;        // 22 x 32 -> 54 bits, exact

  always_comb begin
    n0_c = 96'(w0_0) * 96'(va_i) + 96'(w1_0) * 96'(vb_i) + 96'(w2_0) * 96'(vc_i);
    // The x partial of w is the NEGATED y difference, and the y partial is the
    // x difference -- the asymmetry is orient's, not a transcription slip.
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

  // ---- one in flight -------------------------------------------------------
  // `v_ready_o` is a function of registers only, never of `r_ready_i`.
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

endmodule : zhao_geom_attrsetup

`default_nettype wire
