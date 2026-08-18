// zhao_terrain_normals.sv — TERRAIN.NORMALS: the deformed-surface normal
// (phase 6, ZH-038).
//
// Law, in citation order:
//   design/contracts/TERRAIN.NORMALS.md — the block contract.
//   design/blocks.yml — `inputs: [terrain_mesh]`, `outputs: [terrain_normals]`,
//       `latency: variable`, "1 normal per vertex per clock", counter
//       `terrain_samples_evaluated`, and the note "Q formats per
//       spec/qformats.md height16/normal entries".
//   reference/src/zrender/terrain.cpp, `shade_flat_tri` — THE arithmetic this
//       block reproduces, quoted below because its Q-format algebra carries a
//       fixed defect that a reimplementation would otherwise walk straight
//       back into.
//   spec/qformats.md §3/§4 — `rescale` is a round-half-up shift then a
//       saturating narrow to the fx16 word.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC, AND THE DEFECT IT ALREADY COST
// ---------------------------------------------------------------------------
// Edges are fx16 (Q16.16) differences. A product of two Q16.16 raws is Q32.32,
// so the cross-product lanes are Q32.32 and the shift back to Q16.16 is
// rescale(., 16).
//
// `shade_flat_tri` records what happens if that is 32 instead: the normal
// quantises to WHOLE world-units squared, every component of a near-flat cell
// rounds to zero, the degenerate guard fires for every triangle, and the entire
// patch shades solid black. It was found because a 41x41 lattice over +-12 m
// (0.6 m spacing) rendered as a black silhouette while 25x25 over the same
// envelope (1.0 m spacing) looked correct. Phase-6 Mantle patches are 32x32
// cells per world patch, which is sub-metre BY DESIGN, so this block lives
// entirely inside the regime where that defect is fatal. The shift is 16.
//
// ---------------------------------------------------------------------------
// WIDTHS, STATED RATHER THAN ASSUMED
// ---------------------------------------------------------------------------
// A vertex component is signed 32 (fx16). An edge component is a difference of
// two of those, so signed 33. A cross-product term is a product of two signed
// 33 values, so signed 66; a lane is a difference of two such terms, so signed
// 67. The rounding add of 1<<15 cannot overflow that, and the arithmetic right
// shift by 16 leaves signed 51, which the saturating narrow takes to signed 32.
// Nothing here is allowed to wrap: every intermediate is carried at its full
// width, which is why the lane registers are 67 bits and not 64.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN, NOT FOUND (recorded here and in the contract)
// ---------------------------------------------------------------------------
// 1. THIS BLOCK EMITS FACE NORMALS, NOT VERTEX NORMALS. The ledger's throughput
//    line says "1 normal per vertex per clock", but the only ratified normal in
//    the tree is `shade_flat_tri`'s per-triangle cross product, and the
//    reference shades flat. Averaging adjacent face normals into a vertex
//    normal is a real technique and it is NOT ratified anywhere: it would need
//    a rule for how many neighbours a lattice-edge vertex has, what a void
//    column contributes, and whether the average is renormalised (which
//    §7.4's no-renormalisation ruling bears on). Inventing that here and
//    calling it the law is exactly the error this project has been bitten by.
//    So: face normals, matching the oracle bit-for-bit, and the vertex-normal
//    question is left open in the contract for whoever ratifies it.
// 2. THE NORMAL IS NOT NORMALISED. `shade_flat_tri` divides by |n| only at the
//    moment it takes a dot product, so the ratified quantity is the UNNORMALISED
//    Q16.16 cross product. Normalising here would insert a second rounding that
//    the reference does not have (§3 single-rounding), and §7.4's
//    `normalize3_approx` is available to any consumer that wants a unit vector.
// 3. DEGENERACY is reported, not substituted. When all three lanes are exactly
//    zero the triangle has zero area; the reference returns shade 0 for that
//    case. This block raises `degenerate_o` and emits the zero vector rather
//    than inventing an up-vector, so a consumer cannot mistake a collapsed cell
//    for a flat one.
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_terrain_normals (
    input logic clk,
    input logic rst_n,

    // terrain_mesh: one triangle of the deformed surface, fx16 world units.
    input  logic               tri_valid_i,
    output logic               tri_ready_o,
    input  logic signed [31:0] ax_i,
    input  logic signed [31:0] ay_i,
    input  logic signed [31:0] az_i,
    input  logic signed [31:0] bx_i,
    input  logic signed [31:0] by_i,
    input  logic signed [31:0] bz_i,
    input  logic signed [31:0] cx_i,
    input  logic signed [31:0] cy_i,
    input  logic signed [31:0] cz_i,
    input  logic        [15:0] src_id_i,

    // terrain_normals: the unnormalised Q16.16 face normal.
    output logic               nrm_valid_o,
    input  logic               nrm_ready_i,
    output logic signed [31:0] nx_o,
    output logic signed [31:0] ny_o,
    output logic signed [31:0] nz_o,
    output logic               degenerate_o,
    output logic        [15:0] src_id_o,

    output logic [31:0] terrain_samples_evaluated_o,
    output logic        idle_o
);

  // ---- stage 1: edges and the six products ---------------------------------
  // Registered so the two multiplies per lane are not in series with the
  // rescale; this is the block's whole latency (fixed 2, which `variable`
  // admits).
  logic               s1_valid;
  logic signed [66:0] s1_n0, s1_n1, s1_n2;
  logic        [15:0] s1_src;

  logic signed [32:0] e1x, e1y, e1z, e2x, e2y, e2z;
  always_comb begin
    e1x = $signed({bx_i[31], bx_i}) - $signed({ax_i[31], ax_i});
    e1y = $signed({by_i[31], by_i}) - $signed({ay_i[31], ay_i});
    e1z = $signed({bz_i[31], bz_i}) - $signed({az_i[31], az_i});
    e2x = $signed({cx_i[31], cx_i}) - $signed({ax_i[31], ax_i});
    e2y = $signed({cy_i[31], cy_i}) - $signed({ay_i[31], ay_i});
    e2z = $signed({cz_i[31], cz_i}) - $signed({az_i[31], az_i});
  end

  // ---- stage 2: the round-half-up rescale by 16 ----------------------------
  logic               s2_valid;
  logic signed [31:0] s2_nx, s2_ny, s2_nz;
  logic               s2_degen;
  logic        [15:0] s2_src;

  // Round-half-up then saturate, exactly rescale_s32(x, 16). The shift is
  // arithmetic, so it floors, which is what makes (x + 2^15) >> 16 round half
  // up rather than toward zero.
  function automatic logic signed [31:0] rescale16(input logic signed [66:0] x);
    logic signed [66:0] r;
    begin
      r = (x + 67'sd32768) >>> 16;
      if (r > 67'sd2147483647) rescale16 = 32'sh7FFF_FFFF;
      else if (r < -67'sd2147483648) rescale16 = 32'sh8000_0000;
      else rescale16 = r[31:0];
    end
  endfunction

  // One job in flight per stage; a full output stalls stage 2, which stalls
  // stage 1, which deasserts ready. No packet is ever dropped.
  wire s2_free = !s2_valid || nrm_ready_i;
  wire s1_free = !s1_valid || s2_free;

  assign tri_ready_o = s1_free;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_valid <= 1'b0;
      s1_n0 <= '0;
      s1_n1 <= '0;
      s1_n2 <= '0;
      s1_src <= '0;
      s2_valid <= 1'b0;
      s2_nx <= '0;
      s2_ny <= '0;
      s2_nz <= '0;
      s2_degen <= 1'b0;
      s2_src <= '0;
      terrain_samples_evaluated_o <= '0;
    end else begin
      if (s1_free) begin
        s1_valid <= tri_valid_i;
        if (tri_valid_i) begin
          s1_n0 <= (e1y * e2z) - (e1z * e2y);
          s1_n1 <= (e1z * e2x) - (e1x * e2z);
          s1_n2 <= (e1x * e2y) - (e1y * e2x);
          s1_src <= src_id_i;
        end
      end

      if (s2_free) begin
        s2_valid <= s1_valid;
        if (s1_valid) begin
          s2_nx <= rescale16(s1_n0);
          s2_ny <= rescale16(s1_n1);
          s2_nz <= rescale16(s1_n2);
          // Degenerate is judged on the RESCALED lanes, matching the
          // reference: `shade_flat_tri` computes nmag2 from fx/fy/fz, the
          // post-rescale values, so a cell whose exact cross product is
          // nonzero but rounds to zero counts as degenerate there too.
          s2_degen <= (rescale16(s1_n0) == 32'sd0) && (rescale16(s1_n1) == 32'sd0) &&
              (rescale16(s1_n2) == 32'sd0);
          s2_src <= s1_src;
          terrain_samples_evaluated_o <= terrain_samples_evaluated_o + 32'd1;
        end
      end
    end
  end

  assign nrm_valid_o  = s2_valid;
  assign nx_o         = s2_nx;
  assign ny_o         = s2_ny;
  assign nz_o         = s2_nz;
  assign degenerate_o = s2_degen;
  assign src_id_o     = s2_src;
  assign idle_o       = !s1_valid && !s2_valid;

endmodule : zhao_terrain_normals
