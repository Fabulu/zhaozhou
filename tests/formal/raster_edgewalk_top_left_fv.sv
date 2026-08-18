// raster_edgewalk_top_left_fv.sv — formal harness for the §8 D3D top-left
// fill rule (RASTER.EDGEWALK, ZH-023; property raster_edgewalk_top_left.sby).
//
// WHAT IS PROVED, and why it is not vacuous.
//
// The DUT is zhao_raster_fill — the EXACT module zhao_raster_edgewalk
// instantiates 48 times per tile row. Nothing about the fill rule is
// restated here; the law side of each assertion is built from the spec
// formula `E0 + bias >= 0` in wide arithmetic.
//
// The free inputs are E' (the tile-local accumulator value) and r (the
// per-edge constant low byte). That parametrisation is COMPLETE, not a
// sample: (E', r) with r in [0,255] is a bijection onto the integers via
// E0 = 256*E' + r, since floor(E0/256) = E' and E0 & 255 = r. Ranging over
// free (E', r) therefore ranges over EVERY edge value the decomposition can
// carry — there is no reachability gap for the solver to hide in.
//
//   P1  a_exact         the narrow (E', rnz, tl) form the RTL evaluates is
//                       EQUAL to `E0 + bias >= 0` with bias 0 (top-left) or
//                       -1, on the exact reconstructed E0. This is the
//                       2026-08-15 spec defect stated as a theorem: a strict
//                       `>` or a bias applied to the floored E' both break it.
//
//   P2  a_exactly_once  THE ADJACENT-TRIANGLE LAW (charter §20.4). Two
//                       triangles sharing edge PQ see the edge traversed in
//                       opposite directions, so one sees E0 and the other
//                       -E0; and edge_top_left(P,Q) = !edge_top_left(Q,P),
//                       so their bias flags are complementary. The assertion
//                       is that the two sides accept EXACTLY once for every
//                       edge value: no hole, no double fill, at any subpixel
//                       position. (The `!=` is over booleans, i.e. XOR.)
//
// The cover task is load-bearing. Both assertions are unconditional
// equalities, so they cannot go vacuous through an unreachable antecedent —
// but the covers pin the interesting corners as REACHABLE anyway: the
// on-the-edge case E' = 0, r = 0 on both bias polarities, and the sub-unit
// tiebreak E' = 0, r != 0 that distinguishes the narrow form from a naive
// floor-and-compare. If any cover fails, this file is not testing what its
// comments claim.

module raster_edgewalk_top_left_fv #(
  // The shipping tile accumulator width (zhao_raster_edgewalk's ACC_W).
  parameter int unsigned W = 29
) (
  input logic                clk,
  input logic signed [W-1:0] e_free,   // E' = floor(E0 / 256) — unconstrained
  input logic        [7:0]   r_free,   // r  = E0 & 255        — unconstrained
  input logic                tl_free   // this edge's top-left flag
);

  // room for 256*E' + r and for its negation
  localparam int unsigned EW = W + 10;

  // ---- the exact edge value, reconstructed (the §8 decomposition) --------
  logic signed [EW-1:0] e0;
  assign e0 = $signed({e_free, 8'd0}) + $signed({{(EW-8){1'b0}}, r_free});

  // ---- THE LAW: inside <=> E0 + bias >= 0, bias 0 / -1 (qformats.md §8) --
  logic signed [EW-1:0] bias;
  assign bias = tl_free ? {EW{1'b0}} : {EW{1'b1}};  // 0 / -1
  logic signed [EW-1:0] sum;
  assign sum = e0 + bias;
  logic want;
  assign want = !sum[EW-1];  // sum >= 0

  // ---- THE SHIPPING LOGIC ------------------------------------------------
  logic got;
  zhao_raster_fill #(.W(W)) u_dut (
    .e_i     (e_free),
    .rnz_i   (r_free != 8'd0),
    .tl_i    (tl_free),
    .accept_o(got)
  );

  // ---- the OTHER side of the same shared edge ----------------------------
  logic signed [EW-1:0] e0b;
  assign e0b = -e0;
  logic signed [EW-1:0] eb;  // its E' = floor(-E0 / 256)
  assign eb = e0b >>> 8;
  logic [7:0] rb;            // its r  = (-E0) & 255
  assign rb = e0b[7:0];

  logic gotb;
  zhao_raster_fill #(.W(EW)) u_other (
    .e_i     (eb),
    .rnz_i   (rb != 8'd0),
    .tl_i    (!tl_free),
    .accept_o(gotb)
  );

  always_ff @(posedge clk) begin
    a_exact:         assert (got == want);
    a_exactly_once:  assert (got != gotb);
  end

  always_ff @(posedge clk) begin
    c_accept:        cover (got);
    c_reject:        cover (!got);
    // pixel centre EXACTLY on the edge, top-left side claims it
    c_on_edge_tl:    cover (e_free == {W{1'b0}} && r_free == 8'd0 && tl_free && got);
    // pixel centre EXACTLY on the edge, the other side must NOT claim it
    c_on_edge_notl:  cover (e_free == {W{1'b0}} && r_free == 8'd0 && !tl_free && !got);
    // strictly inside by LESS than one subpixel^2 unit: the floored E' is 0
    // but the pixel is inside, and the non-top-left side must still accept.
    // This is the case a bias on the floored E' drops on both sides.
    c_sub_unit:      cover (e_free == {W{1'b0}} && r_free != 8'd0 && !tl_free && got);
    // ...and its mirror on the other side rejects, i.e. exactly once
    c_sub_unit_pair: cover (e_free == {W{1'b0}} && r_free != 8'd0 && got && !gotb);
  end

endmodule : raster_edgewalk_top_left_fv
