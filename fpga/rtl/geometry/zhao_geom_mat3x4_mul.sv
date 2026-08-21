// zhao_geom_mat3x4_mul.sv — 3x4 affine matrix product, one element per cycle.
//
// A submodule of GEOM.POSE (design/contracts/GEOM.POSE.md), not a ledger block
// of its own. It is the step the per-bone decode chain applies twice:
//
//     A_b = (b == 0) ? LR : A_parent * LR      <-- THIS BLOCK
//     S_b = A_b * inv_rest[b]                  <-- and again here
//
// Reference: `zref::creature::mat3x4_mul`
// (reference/src/zcreature/creature_core.cpp:74).
//
// ---------------------------------------------------------------------------
// WHY THIS ONE IS SEQUENTIAL
// ---------------------------------------------------------------------------
// The obvious implementation is combinational, and it would be thirty-six 32x32
// products — twice GEOM.SKIN's eighteen, on a device with 112 DSPs that the
// project already over-subscribes at 171. Writing that and calling it done
// would make the budget problem meaningfully worse in exchange for latency
// nobody has asked for.
//
// So this block computes ONE OUTPUT ELEMENT PER CYCLE: three 32x32 products,
// twelve cycles per matrix. That is the first of the sharing levers
// design/contracts/GEOM.SKIN.md lists as open — taken here rather than merely
// described, because the pose decode is the right place to spend latency.
//
// Twelve cycles per multiply is affordable precisely BECAUSE of what this block
// feeds. `spec/creature_rules.md` §2.2 rejected baking every pose at load (x6
// memory); the decode is a cache MISS cost, not a per-frame-per-instance cost.
// Two multiplies per bone at up to 32 bones is 768 cycles for a whole palette,
// once, shared by every instance of that creature type on that frame.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
// For output row i, column j:
//
//     j < 3:  out[i][j] = rescale(a[i][0]*b[0][j] + a[i][1]*b[1][j]
//                                 + a[i][2]*b[2][j], 16)
//     j == 3: out[i][3] = rescale(a[i][0]*b[0][3] + a[i][1]*b[1][3]
//                                 + a[i][2]*b[2][3] + (a[i][3] << 16), 16)
//
// One rescale per element, round-half-up then saturate (qformats §3/§4). The
// two cases are the SAME three products against a different column of `b`, plus
// a translation term that is zero for j < 3 — so one datapath serves both and
// there is no second multiplier set for the translation column.
//
// WIDTHS: a product is s64; three of them is s66; plus the s48 translation is
// s67. Round-half-up cannot overflow that. `>>> 16` leaves s51, which the
// saturating narrow takes to s32.
//
// Unlike the quaternion block, saturation here CAN fire: `a` and `b` are
// arbitrary fx16 affines, and a large translation chained through a large
// rotation genuinely exceeds s32. The reference saturates too (rescale_s32),
// so the two agree — but this is a real rail, not a formality, and the directed
// test drives it.
module zhao_geom_mat3x4_mul (
    input  logic clk,
    input  logic rst_n,

    // ---- operands in, ready/valid -----------------------------------------
    input  logic               in_valid_i,
    output logic               in_ready_o,
    input  logic signed [31:0] a_m_i [12],
    input  logic signed [31:0] b_m_i [12],
    input  logic        [ 7:0] in_tag_i,

    // ---- product out, ready/valid -----------------------------------------
    output logic               out_valid_o,
    input  logic               out_ready_i,
    output logic signed [31:0] out_m_o [12],
    output logic        [ 7:0] out_tag_o,

    output logic [31:0] products_done_o
);

  // ---- round-half-up then saturate, qformats §3/§4 ------------------------
  function automatic logic signed [31:0] rescale_sat16(input logic signed [66:0] v);
    logic signed [66:0] r;
    begin
      r = (v + (67'sd1 <<< 15)) >>> 16;
      if (r > 67'sd2147483647) rescale_sat16 = 32'sh7FFF_FFFF;
      else if (r < -67'sd2147483648) rescale_sat16 = 32'sh8000_0000;
      else rescale_sat16 = r[31:0];
    end
  endfunction

  // ---- held operands and the element cursor -------------------------------
  logic signed [31:0] a_q [12];
  logic signed [31:0] b_q [12];
  logic        [ 7:0] tag_q;
  logic        [ 3:0] idx;      // 0..11, the element being produced
  logic               busy;

  // Row and column of the element under the cursor. `idx` walks row-major, the
  // same order the reference's nested loops produce, so a partial result is
  // always a prefix of the reference's own.
  logic [1:0] row, col;
  assign row = idx[3:2];
  assign col = idx[1:0];

  // ---- the three products, plus the translation term ----------------------
  // The translation term is zero unless this is the fourth column; that is the
  // whole difference between the two cases in the law above.
  logic signed [66:0] acc;
  always_comb begin
    acc = 67'(a_q[{row, 2'd0}]) * 67'(b_q[{2'd0, col}])
        + 67'(a_q[{row, 2'd1}]) * 67'(b_q[{2'd1, col}])
        + 67'(a_q[{row, 2'd2}]) * 67'(b_q[{2'd2, col}]);
    if (col == 2'd3) acc = acc + (67'(a_q[{row, 2'd3}]) <<< 16);
  end

  logic accept;
  assign in_ready_o = !busy && (!out_valid_o || out_ready_i);
  assign accept = in_valid_i && in_ready_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy <= 1'b0;
      idx <= '0;
      tag_q <= '0;
      out_valid_o <= 1'b0;
      out_tag_o <= '0;
      products_done_o <= '0;
      for (int i = 0; i < 12; i++) begin
        a_q[i] <= '0;
        b_q[i] <= '0;
        out_m_o[i] <= '0;
      end
    end else begin
      if (out_valid_o && out_ready_i) out_valid_o <= 1'b0;

      if (accept) begin
        for (int i = 0; i < 12; i++) begin
          a_q[i] <= a_m_i[i];
          b_q[i] <= b_m_i[i];
        end
        tag_q <= in_tag_i;
        idx <= '0;
        busy <= 1'b1;
        out_valid_o <= 1'b0;
      end else if (busy) begin
        out_m_o[idx] <= rescale_sat16(acc);
        if (idx == 4'd11) begin
          busy <= 1'b0;
          out_valid_o <= 1'b1;
          out_tag_o <= tag_q;
          // Counts COMPLETED products, not accepted operands: a multiply still
          // walking its twelve elements is not a product this block has
          // delivered.
          if (products_done_o != 32'hFFFF_FFFF) products_done_o <= products_done_o + 32'd1;
        end else begin
          idx <= idx + 4'd1;
        end
      end
    end
  end

endmodule : zhao_geom_mat3x4_mul
