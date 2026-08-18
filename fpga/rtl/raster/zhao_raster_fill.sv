// zhao_raster_fill.sv — the §8 D3D top-left fill predicate for ONE pixel
// centre against ONE edge. Instantiated 3× per column (48× per tile row) by
// zhao_raster_edgewalk.
//
// Law: spec/qformats.md §8 — inside ⟺ E0 + bias ≥ 0 on the EXACT edge value,
// bias 0 for a top-left edge and −1 otherwise.
//
// This module is a separate file for ONE reason: it is the piece the formal
// property tests/formal/raster_edgewalk_top_left.sby proves, and a proof of
// a COPY of the fill rule would be worthless. The .sby instantiates exactly
// this module — the same bytes the tile walker synthesises.
//
// The narrow form (§8 "the RTL keeps its s32 tile stepping"). The edge value
// is carried as E' = E0 >>> 8 (floor) plus the per-edge constant bit
// rnz = (E0 & 255 != 0); the low 8 bits of E0 are the same at every pixel
// centre of a tile because both edge steps are multiples of 256. Writing
// E0 = 256·E' + r with r ∈ [0,255]:
//
//     bias  0 :  E0 ≥ 0  ⟺  E' ≥ 0
//     bias −1 :  E0 ≥ 1  ⟺  E' > 0 ∨ (E' = 0 ∧ r ≠ 0)
//
// both of which are the single expression below. Proved exactly equal to
// `E0 + bias ≥ 0` for every (E', r) by the .sby above.
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_raster_fill #(
  // Accumulator width. The tile walker ships W = 29 (the saturated
  // tile-local domain); the formal harness also instantiates a wider one to
  // carry the negated edge value of the opposite triangle.
  parameter int unsigned W = 29
) (
  input  logic signed [W-1:0] e_i,      // E' = floor(E0 / 256)
  input  logic                rnz_i,    // (E0 & 255) != 0 — constant per edge
  input  logic                tl_i,     // top-left edge ⇒ bias 0, else bias −1
  output logic                accept_o  // the pixel centre is covered by this edge
);

  assign accept_o = (!e_i[W-1]) && (tl_i || rnz_i || (e_i != {W{1'b0}}));

endmodule : zhao_raster_fill
