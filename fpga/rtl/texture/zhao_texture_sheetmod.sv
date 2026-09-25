// zhao_texture_sheetmod.sv -- THE SURFACE SHEET'S VISIBLE EFFECT.
//
// ENFORCED-BY: tests/texture/sheetmod_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS, AND WHAT IT SUPERSEDES
// ---------------------------------------------------------------------------
// `design/contracts/TEXTURE.AUX.V2.md` ("No AUX-as-sample-2 law") and
// `design/contracts/TEXTURE.COMBINE.md` ("Required planes, AUX, status, and raw
// index") both end the same sentence:
//
//     "AUX status participates in final status only when AUX is required; tag
//      and strength are RESERVED FOR A LATER VISIBLE TERRAIN-EFFECT
//      COMPOSITION and Packet B does not claim that effect is connected."
//
// THIS BLOCK IS THAT COMPOSITION, and the clause above is superseded by it --
// see the decision record in `design/contracts/TEXTURE.SHEETMOD.md`. What is
// NOT superseded, and is re-affirmed here rather than quietly weakened: AUX
// still never occupies, aliases or substitutes for a TMU sample, and NO RECIPE
// consumes tag or strength as an RGB, alpha, weight or raw-index OPERAND. The
// modulation below happens AFTER the recipe has produced its colour, on the
// recipe's own result, which is a different statement from being a recipe
// operand -- and it is the ORACLE'S OWN SHAPE: `span.mod_r/g/b` multiplies the
// texel the recipe produced, it is not one of the texels.
//
// ---------------------------------------------------------------------------
// THE LAW, TRANSCRIBED -- NOT DERIVED
// ---------------------------------------------------------------------------
// `reference/src/zrender/terrain.cpp` states it twice and this file implements
// both statements, which are the same arithmetic written two ways:
//
//   line 45   the file's own header:   "sheet tint law: rgb = rgb * (255 -
//                                       strength/2) / 256 (max ~50%)"
//   line 638  `sheet_factor`:          if (sheet == nullptr) return 65536;
//                                      return (255 - (strength >> 1)) << 8;
//   line 718  the untextured path:     tint = 255 - (sheet_at(i, j) >> 1);
//   line 756  and its application:     (lit * tint + 128) >> 8
//
// Those two are EXACTLY equal and the equality is why this block is three
// lines of arithmetic rather than a Q16.16 multiplier:
//
//     value(factor) = ((255 - (s>>1)) << 8) / 2^16
//     rescale( rgb * factor , 16 )  =  (rgb * ((255-(s>>1))<<8) + 2^15) >> 16
//                                   =  (rgb * (255-(s>>1))      + 2^7 ) >> 8
//                                   =  unit_mul(rgb, 255 - (s>>1))
//
// so the Q16.16 form and the unit8 form agree bit for bit for every one of the
// 256 x 256 operand pairs, and the directed test proves that exhaustively
// rather than taking this paragraph's word for it. ONE rounding, round-half-up,
// spec/qformats.md section 3.
//
// ---------------------------------------------------------------------------
// WHY THE IDENTITY IS `ENABLE`, NOT `STRENGTH == 0`
// ---------------------------------------------------------------------------
// This is the one trap in the law and the oracle names it in a comment of its
// own: *"tint only when a sheet exists: even t = 255 would darken by ~0.4%
// ((v*255+128)>>8 < v for v >= 129), shifting every unstamped colour"*.
//
// So `255 - (0 >> 1) = 255` is NOT the multiplicative identity -- 255/256 is.
// `sheet_factor`'s `if (sheet == nullptr) return 65536` is a SEPARATE ARM and
// this block keeps it separate: when `en_i` is low the RGB passes through
// UNTOUCHED, bit for bit, and `modulated_o` does not move. A block that tried
// to express "no sheet" as "strength 0" would darken every non-terrain
// fragment in the machine by 0.4% and no counter anywhere would say so.
//
// `en_i` is the fragment's own `aux_required`, which is the MATERIAL's
// declaration -- the same bit the combiner's required-source mask is built
// from. It is not inferred from the strength byte and it is not inferred from
// a non-zero context (owner directive of 2026-09-23 section 3: "Profile
// selection is explicit, not inferred from whether a port happens to be zero").
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT
// ---------------------------------------------------------------------------
//   * It is not a sampler. It receives a strength byte that SURFACE.SHEET
//     already returned and TEXTURE.AUX already typed; it issues nothing and
//     addresses nothing.
//   * It does not touch ALPHA. Charter section 12's law is an RGB tint and the
//     oracle applies it to the three colour channels only.
//   * It does not read `tag`. The tag byte's effects are a separate,
//     unauthored family (terrain_rules 6.5, "for tag/strength effects") and
//     inventing one here would be the art decision this repository forbids a
//     composer. The tag remains unreachable-by-effect and reachable-by-read,
//     which is exactly the state `zref_aux.hpp`'s choice A1 argued for.
//   * It holds no state. Purely combinational, so it adds no latency to the
//     combiner's finish stage and needs no handshake of its own.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply.
`default_nettype none

module zhao_texture_sheetmod (
    // THE FRAGMENT'S OWN DECLARATION. High = this fragment's material declared
    // an AUX (surface-sheet) source, so the sheet tint applies. Low = the
    // oracle's `sheet == nullptr` arm: exact unity, RGB untouched.
    input  var logic        en_i,
    // Layer F's strength byte for this fragment, as SURFACE.SHEET returned it
    // and `zhao_texture_aux_pipe_v2` typed it ([31:24] of the AUX plane).
    input  var logic [ 7:0] strength_i,
    // The recipe's finished colour, {r, g, b} with r in [23:16].
    input  var logic [23:0] rgb_i,

    output var logic [23:0] rgb_o,
    // High exactly when the output differs from the input BY CONSTRUCTION of
    // the enable -- i.e. when the tint arm was taken. It is deliberately NOT
    // `rgb_o != rgb_i`: at strength 0 and rgb 0 the tint arm produces the same
    // bits, and a counter that could not distinguish "applied and happened to
    // agree" from "not applied" would be the blind detector this repository
    // has a chapter about. The composer counts THIS.
    output var logic        applied_o
);

  // The oracle's `255 - (strength >> 1)`, which is the tint byte, in [128, 255].
  logic [7:0] tint_c;
  assign tint_c = 8'd255 - {1'b0, strength_i[7:1]};

  // spec/qformats.md section 3: (a*b + 128) >> 8, round-half-up, ONE rounding.
  // The fourth instance of this three-line primitive in the tree
  // (`zhao_raster_fragment`, `zhao_post_composite`, `zhao_post_gather_tag`);
  // a shared function would be a package dependency on three leaves that
  // deliberately have none.
  function automatic logic [7:0] unit_mul(input logic [7:0] a, input logic [7:0] b);
    logic [15:0] p;
    p = {8'd0, a} * {8'd0, b};
    unit_mul = 8'((p + 16'd128) >> 8);
  endfunction

  always_comb begin
    if (en_i) begin
      rgb_o = {unit_mul(rgb_i[23:16], tint_c),
               unit_mul(rgb_i[15: 8], tint_c),
               unit_mul(rgb_i[ 7: 0], tint_c)};
    end else begin
      rgb_o = rgb_i;
    end
    applied_o = en_i;
  end

endmodule

`default_nettype wire
