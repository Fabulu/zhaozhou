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
  always_comb begin
    n0_c = 96'(w0_0) * 96'(va_i) + 96'(w1_0) * 96'(vb_i) + 96'(w2_0) * 96'(vc_i);
    // The x partial of w is the NEGATED y difference, and the y partial is the
    // x difference -- the asymmetry is orient's, not a transcription slip.
    dndx_c = ((-72'(cy_by)) * 72'(va_i) + (-72'(ay_cy)) * 72'(vb_i) +
              (-72'(by_ay)) * 72'(vc_i)) <<< PIXEL_SHIFT;
    dndy_c = (72'(cx_bx) * 72'(va_i) + 72'(ax_cx) * 72'(vb_i) +
              72'(bx_ax) * 72'(vc_i)) <<< PIXEL_SHIFT;
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
