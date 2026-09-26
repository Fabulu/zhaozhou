// zhao_geom_overw_sat.sv -- THE S8.24 BOUND, BUILT, WITH THE SATURATE THE
// SPEC HAS MANDATED ALL ALONG.
//
// ENFORCED-BY: tests/geometry/geom_overw_sat_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `fpga/rtl/prod/zhao_console_core.sv` entry I13, item 2 of the CELLCARRY
// paragraph:
//
//     "THE S8.24 BOUND IS REAL, AND ITS FAILURE IS SILENT. ... Terrain's
//      `terr_uv_*` is `signed [31:0]` Q16.16 in TILE units. AND THERE IS NO
//      SATURATE AND NO CLAMP ANYWHERE ON THAT MULTIPLY ... So a terrain patch
//      more than 128 tiles from the origin would WRAP, silently, with no
//      counter watching."
//
// That was read as an open law. IT IS NOT OPEN. `spec/qformats.md:75` fixes it
// in the type table, in the column headed for exactly this question:
//
//     | `u/v_over_w` | s32 | S 8.24 | saturate | round-half-up | perspective
//                                     ^^^^^^^^                    numerators |
//
// and `spec/qformats.md:56` makes `rescale_s` "round-half-up ... followed by
// saturation to the destination width". The spec already says saturate. What
// was missing was a block that does it. This is that block.
//
// ---------------------------------------------------------------------------
// THE LAW, TRANSCRIBED -- NOT DERIVED
// ---------------------------------------------------------------------------
// `zhao_geom_vattr.sv:48-58` derives the per-vertex form from the per-pixel
// recovery, and this block implements the same three lines:
//
//     value(u_over_w) = value(u) * value(invw24)
//     raw(u_over_w)   = (raw(u)/2^16) * (invw24/2^24) * 2^24
//                     = rescale_s(sext(u) * invw24, 16)          (qformats 4)
//
// with ONE rounding, round-half-up, then the saturation the type carries.
// `zref::geom_over_w` (reference/include/zref/zref_depth.hpp:110) is the
// compiled statement of it, and `zref::geom_over_w_wide` beside it is the same
// arithmetic over the wider coordinate this block takes.
//
// ---------------------------------------------------------------------------
// WHY `zhao_geom_vattr` IS RIGHT TO HAVE NO SATURATE, AND WHY TERRAIN IS NOT
// ---------------------------------------------------------------------------
// This block does NOT correct `zhao_geom_vattr`. That block's header claims
//
//     "|u| < 2^15 and invw24 < 2^24 bound the product below 2^39, so the
//      result fits s24 and NO SATURATION CASE EXISTS"
//
// and the claim is TRUE AND MEASURED: `mul_a_c` is declared `signed [15:0]`
// (`zhao_geom_vattr.sv:464`), so the mesh path's coordinate is the vertex
// record's s16 UV field and the bound is structural. The directed test's
// negative control walks ALL 65,536 s16 coordinates against a sweep of
// `invw24` and asserts `sat_o` is low on every one of them -- i.e. it proves
// vattr's sentence rather than quoting it.
//
// TERRAIN'S COORDINATE IS NOT s16. `zhao_terrain_uvlane`'s `terr_uv_*` is
// `signed [31:0]`, Q16.16 in TILE units, because a terrain patch is addressed
// in world tiles and not in a mesh's local texel window. On a s32 coordinate
// the product reaches 2^55 and `rescale_s(.,16)` reaches 2^39, which is seven
// bits outside the s32 the type is stored in. The bound |value| < 128 in S8.24
// is then a real edge that a real island crosses -- at 128 tiles from the
// origin -- and crossing it WITHOUT this block is a silent wrap: the texture
// coordinate flips sign and the patch samples from the far side of the atlas,
// with every gate in the repository passing.
//
// `zhao_raster_attrdiv_v2`'s `q_saturated_o` does NOT cover this. That guard
// watches the INTERPOLATED QUOTIENT downstream (`zhao_raster_attrdiv_v2.sv:105`);
// the wrap here happens in the PER-VERTEX NUMERATOR, upstream of the plane
// setup, so by the time attrdiv sees it the value is already a legal-looking
// number on the wrong side of the atlas. A guard on the wrong side of a wrap
// is not a guard.
//
// ---------------------------------------------------------------------------
// `sat_o` IS A DECLARATION, NOT A DIFFERENCE
// ---------------------------------------------------------------------------
// `sat_o` is high exactly when the saturate ENGAGED -- i.e. when the rounded
// product left the s32 rails -- and not when `over_w_o` happens to equal a
// rail. A coordinate whose exact result IS 0x7FFFFFFF is not a saturation and
// this block does not report one. That is the same distinction
// `zhao_texture_sheetmod`'s `applied_o` header draws, and for the same reason:
// a flag that cannot separate "saturated" from "landed on the rail" is the
// blind detector this repository has a chapter about.
//
// This block is combinational and holds no state, so it carries no counter.
// THE COMPOSER COUNTS `sat_o`, exactly as the composer counts sheetmod's
// `applied_o`. The directed test fires the flag on stimulus that exceeds the
// bound and holds it low on the entire s16 domain, so it is demonstrated in
// both directions before anybody composes it.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT
// ---------------------------------------------------------------------------
//   * It is NOT composed. Its consumer is the `pack_attr` analogue that fills
//     a terrain triangle's slots 1 and 2 -- `zhao_forge_assemble.sv:674-687`
//     is the template and writes `32'd0` into both under owner ruling R197
//     because the forge path is declared-untextured. Terrain is textured and
//     must fill them for real, which is I13's carriage. THIS IS A DEFERRAL AND
//     IT IS WRITTEN DOWN AS ONE: the packet that lays the carriage
//     instantiates this module and deletes this paragraph.
//   * It does NOT decide the coordinate's units. Whether terrain's tile-unit
//     Q16.16 is rescaled before this multiply is the carriage's question; this
//     block implements `rescale_s(uv * invw24, 16)` on whatever raw it is
//     given, which is what the spec defines.
//   * It costs a 32x24 multiplier and THAT IS THE ONE UNMEASURED CLAIM HERE.
//     The named fit question, for the fit that composes it: does the wide
//     former close on ALMs at terrain's vertex rate, or must it be
//     sequentialised the way `zhao_terrain_shademod` was? It is stated here so
//     the next packet spends the fit on a question rather than on a hunch.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply.
`default_nettype none

module zhao_geom_overw_sat (
    // The vertex's raw fx16 (S15.16) texture coordinate. Terrain presents
    // `terr_uv_*` here directly; the mesh path's s16 field is sign-extended
    // into it and then provably never saturates (see the header).
    input  var logic signed [31:0] uv_i,
    // The vertex's depth, U0.24, as `zhao_geom_depthquant` emits it.
    input  var logic        [23:0] invw24_i,

    // `rescale_s(uv_i * invw24_i, 16)`, saturated to the s32 the S8.24 type is
    // stored in: 0x7FFFFFFF above, 0x80000000 below.
    output var logic signed [31:0] over_w_o,
    // High exactly when the saturate engaged. See the header: a result that
    // lands on a rail exactly is NOT a saturation.
    output var logic               sat_o
);

  // ---- the exact product ------------------------------------------------
  // |uv| < 2^31 and invw24 < 2^24 bound |p| < 2^55, so a signed 56-bit
  // product is exact and nothing here wraps.
  logic signed [55:0] p_c;
  assign p_c = $signed({{24{uv_i[31]}}, uv_i}) * $signed({32'd0, invw24_i});

  // ---- the one rounding: rescale_s(p, 16) = (p + 2^15) >>> 16 ------------
  // The sum is taken in 57 bits so the round cannot carry out of the product
  // width, then arithmetically shifted. |r| < 2^39, so 40 signed bits hold it
  // exactly -- and the seven bits above the s32 rail are precisely what the
  // saturate below is for.
  logic signed [56:0] sum_c;
  logic signed [39:0] r_c;
  assign sum_c = $signed({p_c[55], p_c}) + 57'sd32768;
  assign r_c   = 40'(sum_c >>> 16);

  // ---- the saturation the type table mandates ---------------------------
  localparam logic signed [39:0] HI_C = 40'sh00_7FFF_FFFF;   //  2^31 - 1
  localparam logic signed [39:0] LO_C = 40'shFF_8000_0000;   // -2^31

  always_comb begin
    if (r_c > HI_C) begin
      sat_o    = 1'b1;
      over_w_o = 32'sh7FFF_FFFF;
    end else if (r_c < LO_C) begin
      sat_o    = 1'b1;
      over_w_o = 32'sh8000_0000;
    end else begin
      sat_o    = 1'b0;
      over_w_o = 32'(r_c);
    end
  end

endmodule

`default_nettype wire
