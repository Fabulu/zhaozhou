// zhao_geom_skin.sv — GEOM.SKIN: rigid and two-weight skinning.
//
// Contract: design/contracts/GEOM.SKIN.md
// Reference: `zref::skin_vertex` (reference/include/zref/zref_creature.hpp,
// implemented in reference/src/zcreature/creature_core.cpp). That function is
// what every creature the reference renderer has ever drawn was skinned with,
// so "RTL matches the oracle" means "the hardware moves vertices exactly where
// the shipped pictures put them".
//
// ---------------------------------------------------------------------------
// THE LAW, and the one part of it that is easy to get wrong
// ---------------------------------------------------------------------------
//
// Rigid (`b1 == b0` or `w0 == 64`), one bone:
//
//     o = rescale( A.m[r]*x + A.m[r+1]*y + A.m[r+2]*z + (A.m[r+3] << 16), 16 )
//
// Two-weight, otherwise, with `w1 = 64 - w0`:
//
//     pa = A.m[r]*x + A.m[r+1]*y + A.m[r+2]*z + (A.m[r+3] << 16)
//     pb = B.m[r]*x + B.m[r+1]*y + B.m[r+2]*z + (B.m[r+3] << 16)
//     o  = rescale( w0*pa + w1*pb, 22 )
//
// **SINGLE ROUNDING IS THE LAW** (qformats §3, A3b). `pa` and `pb` are NEVER
// rounded before the blend: the whole expression is exact and rounded ONCE, by
// 22 — sixteen fraction bits from the matrix product plus six from the 1/64
// weight quanta. Rounding the two skins separately and then blending would be
// a double rounding and would disagree with the reference by an LSB on a large
// share of vertices, which is exactly the sort of difference that shows up as
// a shimmering silhouette rather than as an obvious break.
//
// The rigid path is not an optimisation of the blend path; it rescales by 16,
// not 22, because there is no weight scale in it. Treating rigid as
// `w0 = 64, w1 = 0` through the 22 path is arithmetically identical and is
// what the reference's own branch avoids, so this block branches too and the
// directed test pins both against the same oracle.
//
// ---------------------------------------------------------------------------
// WIDTHS, stated rather than assumed
// ---------------------------------------------------------------------------
// A matrix element and a coordinate are both signed 32, so a product is signed
// 64. Three of them plus the translation (a signed 32 shifted left 16, so
// signed 48) needs signed 67. The weight is 0..64, seven bits unsigned, so
// `w0*pa` is signed 74, and the sum of two such terms is signed 75. The
// rounding add of 2^21 cannot overflow that. An arithmetic shift right by 22
// leaves signed 53, which the saturating narrow takes to signed 32.
//
// DSP COST, recorded because this project is already over its DSP budget:
// the blend path issues EIGHTEEN 32x32 products (two matrices, three rows,
// three terms) plus two 75-bit weight multiplies. That is the largest
// multiplier count of any block written so far. It is the law's cost, not an
// implementation choice — but design/budgets/dsp.md is where it has to be
// argued against a frame budget, and this block should be read as evidence for
// that argument rather than as a fait accompli.
module zhao_geom_skin (
    input  logic clk,
    input  logic rst_n,

    // ---- vertex in, ready/valid -------------------------------------------
    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic signed [31:0] v_x_i,
    input  logic signed [31:0] v_y_i,
    input  logic signed [31:0] v_z_i,
    input  logic        [ 6:0] v_w0_i,     // 1/64 quanta; 64 == rigid
    input  logic               v_rigid_i,  // b1 == b0, decided upstream
    input  logic        [15:0] v_src_id_i,

    // ---- the two bone matrices for this vertex, row-major fx16 ------------
    // Presented with the vertex. The palette itself is GEOM.POSE's to own; this
    // block holds no cache and never addresses memory.
    input  logic signed [31:0] a_m_i [12],
    input  logic signed [31:0] b_m_i [12],

    // ---- skinned vertex out, ready/valid ----------------------------------
    output logic               o_valid_o,
    input  logic               o_ready_i,
    output logic signed [31:0] o_x_o,
    output logic signed [31:0] o_y_o,
    output logic signed [31:0] o_z_o,
    output logic        [15:0] o_src_id_o,

    output logic [31:0] vertices_transformed_o
);

  // ---- the exact row product, shared by both paths ------------------------
  // 67 bits: three signed-64 products plus a signed-48 translation.
  function automatic logic signed [66:0] row_product(input logic signed [31:0] m0,
                                                     input logic signed [31:0] m1,
                                                     input logic signed [31:0] m2,
                                                     input logic signed [31:0] m3,
                                                     input logic signed [31:0] x,
                                                     input logic signed [31:0] y,
                                                     input logic signed [31:0] z);
    logic signed [66:0] acc;
    begin
      acc = 67'(m0) * 67'(x) + 67'(m1) * 67'(y) + 67'(m2) * 67'(z) + (67'(m3) <<< 16);
      row_product = acc;
    end
  endfunction

  // ---- round-half-up shift then saturate, qformats §3/§4 ------------------
  // Round-half-up on a NEGATIVE value is the trap: adding the half and shifting
  // arithmetically gives round-half-up (toward +inf), which is what
  // rescale_s32 does. A shift alone would floor, and the two disagree at every
  // exact half.
  function automatic logic signed [31:0] rescale_sat(input logic signed [74:0] v,
                                                     input int unsigned sh);
    logic signed [74:0] r;
    begin
      r = (v + (75'sd1 <<< (sh - 1))) >>> sh;
      if (r > 75'sd2147483647) rescale_sat = 32'sh7FFF_FFFF;
      else if (r < -75'sd2147483648) rescale_sat = 32'sh8000_0000;
      else rescale_sat = r[31:0];
    end
  endfunction

  logic signed [66:0] pa [3];
  logic signed [66:0] pb [3];
  logic signed [74:0] blend [3];
  logic        [ 6:0] w1;

  always_comb begin
    w1 = 7'd64 - v_w0_i;
    for (int r = 0; r < 3; r++) begin
      pa[r] = row_product(a_m_i[r*4], a_m_i[r*4+1], a_m_i[r*4+2], a_m_i[r*4+3], v_x_i, v_y_i,
                          v_z_i);
      pb[r] = row_product(b_m_i[r*4], b_m_i[r*4+1], b_m_i[r*4+2], b_m_i[r*4+3], v_x_i, v_y_i,
                          v_z_i);
      // The whole expression, exact. Nothing is rounded here.
      blend[r] = 75'(pa[r]) * 75'({1'b0, v_w0_i}) + 75'(pb[r]) * 75'({1'b0, w1});
    end
  end

  logic signed [31:0] res [3];
  always_comb begin
    for (int r = 0; r < 3; r++) begin
      // The branch the reference takes, for the reason it takes it: the rigid
      // path carries no weight scale, so it rescales by 16 and never forms the
      // blend at all.
      res[r] = (v_rigid_i || (v_w0_i == 7'd64)) ? rescale_sat(75'(pa[r]), 16)
                                                : rescale_sat(blend[r], 22);
    end
  end

  logic take;
  assign v_ready_o = !o_valid_o || o_ready_i;
  assign take = v_valid_i && v_ready_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      o_valid_o <= 1'b0;
      o_x_o <= '0; o_y_o <= '0; o_z_o <= '0; o_src_id_o <= '0;
      vertices_transformed_o <= '0;
    end else begin
      if (o_valid_o && o_ready_i) o_valid_o <= 1'b0;
      if (take) begin
        o_x_o <= res[0];
        o_y_o <= res[1];
        o_z_o <= res[2];
        o_src_id_o <= v_src_id_i;
        o_valid_o <= 1'b1;
        // `vertices_transformed` is the shared catalog counter GEOM.LOOM and
        // GEOM.WARP also carry, so this stage reports under the same name rather
        // than inventing a skinning-specific one. It counts vertices ACCEPTED,
        // not offered: a vertex held off by backpressure is not work done.
        if (vertices_transformed_o != 32'hFFFF_FFFF) vertices_transformed_o <= vertices_transformed_o + 32'd1;
      end
    end
  end

endmodule : zhao_geom_skin
