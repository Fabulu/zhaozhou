// zhao_geom_quat2mat.sv — quantized quaternion to 3x4 rotation matrix.
//
// A submodule of GEOM.POSE (design/contracts/GEOM.POSE.md), not a ledger block
// of its own. It is the innermost step of the per-bone decode chain:
//
//     R   = quat16_to_mat3(quats[frame][b])      <-- THIS BLOCK
//     LR  = R with the rest translation, plus root displacement at b == 0
//     A_b = (b == 0) ? LR : A_parent * LR
//     S_b = A_b * inv_rest[b]
//
// Reference: `zref::creature::quat16_to_mat3`
// (reference/src/zcreature/creature_core.cpp:49). That is the function the
// reference renderer poses every creature with.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
// A quat16 is four s16 lanes (w, x, y, z) in S1.0.14 — 1.0 is 16384. The
// nine-product formula, each element rounded by exactly one rescale(., 11):
//
//     m0  = 65536 - rescale(yy + zz, 11)   m1 = rescale(xy - wz, 11)
//     m2  = rescale(xz + wy, 11)           m3 = 0
//     m4  = rescale(xy + wz, 11)           m5 = 65536 - rescale(xx + zz, 11)
//     m6  = rescale(yz - wx, 11)           m7 = 0
//     m8  = rescale(xz - wy, 11)           m9 = rescale(yz + wx, 11)
//     m10 = 65536 - rescale(xx + yy, 11)   m11 = 0
//
// where the shift of 11 carries S1.0.14 pairs (28 fraction bits) down to the
// fx16 output, doubled: 28 - 11 = 17, and the formula's factor of 2 accounts
// for the remaining bit.
//
// **NO RENORMALIZATION** (creature_rules §2.2). The quantized quaternion is not
// exactly unit, and this block does NOT correct for it. That is a ratified
// decision, not an omission: renormalizing would need a reciprocal square root
// per bone, and the quantization scale error is instead bounded and measured.
// A renormalizing implementation would be wrong here even though it would be
// "more correct" as mathematics.
//
// **The translation column is zero.** This block produces rotation only; the
// rest translation is inserted by the stage above, which is why m3/m7/m11 are
// hard zeros rather than inputs.
//
// ---------------------------------------------------------------------------
// WIDTHS, and why this block cannot saturate
// ---------------------------------------------------------------------------
// Each lane is s16, so a product is s32 and a sum of two products is s33. The
// worst case is not the S1.0.14 range but the full s16 range an adversarial
// input could carry: |q| <= 32768, so a product is at most 2^30 and a sum of
// two at most 2^31. rescale by 11 brings that to 2^20, and 65536 - 2^20 is
// about -983,040. Every one of those fits s32 with more than ten bits to
// spare.
//
// So the saturation in `rescale_sat` provably never fires for ANY s16 input.
// It is kept because the qformats rescale is defined as round-half-up THEN
// saturate, and a rescale that silently omits half its definition is a trap for
// whoever reuses it. The directed test drives the extreme corners to show the
// bound is real rather than assumed.
//
// DSP COST: nine 16x16 products. On this device a DSP block carries two 18x18
// multipliers, so this is around five blocks — against GEOM.SKIN's eighteen
// 32x32. The pose decode is deliberately the cheap end of the creature path;
// the expensive part is the per-vertex skinning, not the per-bone rotation.
module zhao_geom_quat2mat (
    input  logic clk,
    input  logic rst_n,

    // ---- quaternion in, ready/valid ---------------------------------------
    input  logic               q_valid_i,
    output logic               q_ready_o,
    input  logic signed [15:0] q_w_i,
    input  logic signed [15:0] q_x_i,
    input  logic signed [15:0] q_y_i,
    input  logic signed [15:0] q_z_i,
    input  logic        [ 7:0] q_bone_i,   // opaque tag, returned with the result

    // ---- matrix out, ready/valid ------------------------------------------
    output logic               m_valid_o,
    input  logic               m_ready_i,
    output logic signed [31:0] m_o [12],
    output logic        [ 7:0] m_bone_o,

    output logic [31:0] bones_decoded_o
);

  // ---- round-half-up then saturate, qformats §3/§4 ------------------------
  // Round-half-up on a NEGATIVE value is the trap: adding the half and shifting
  // arithmetically rounds toward +inf, which is what rescale_s32 does. A bare
  // shift would floor, and the two disagree at every exact half — and half of
  // these nine elements are differences, so negatives are the common case here,
  // not the corner case.
  function automatic logic signed [31:0] rescale_sat(input logic signed [33:0] v);
    logic signed [33:0] r;
    begin
      r = (v + 34'sd1024) >>> 11;
      if (r > 34'sd2147483647) rescale_sat = 32'sh7FFF_FFFF;
      else if (r < -34'sd2147483648) rescale_sat = 32'sh8000_0000;
      else rescale_sat = r[31:0];
    end
  endfunction

  // ---- the nine products, exact in s33 ------------------------------------
  logic signed [33:0] xx, yy, zz, xy, xz, yz, wx, wy, wz;
  always_comb begin
    xx = 34'(q_x_i) * 34'(q_x_i);
    yy = 34'(q_y_i) * 34'(q_y_i);
    zz = 34'(q_z_i) * 34'(q_z_i);
    xy = 34'(q_x_i) * 34'(q_y_i);
    xz = 34'(q_x_i) * 34'(q_z_i);
    yz = 34'(q_y_i) * 34'(q_z_i);
    wx = 34'(q_w_i) * 34'(q_x_i);
    wy = 34'(q_w_i) * 34'(q_y_i);
    wz = 34'(q_w_i) * 34'(q_z_i);
  end

  logic signed [31:0] res [12];
  always_comb begin
    res[0]  = 32'sd65536 - rescale_sat(yy + zz);
    res[1]  = rescale_sat(xy - wz);
    res[2]  = rescale_sat(xz + wy);
    res[3]  = '0;
    res[4]  = rescale_sat(xy + wz);
    res[5]  = 32'sd65536 - rescale_sat(xx + zz);
    res[6]  = rescale_sat(yz - wx);
    res[7]  = '0;
    res[8]  = rescale_sat(xz - wy);
    res[9]  = rescale_sat(yz + wx);
    res[10] = 32'sd65536 - rescale_sat(xx + yy);
    res[11] = '0;
  end

  logic take;
  assign q_ready_o = !m_valid_o || m_ready_i;
  assign take = q_valid_i && q_ready_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      m_valid_o <= 1'b0;
      m_bone_o <= '0;
      bones_decoded_o <= '0;
      for (int i = 0; i < 12; i++) m_o[i] <= '0;
    end else begin
      if (m_valid_o && m_ready_i) m_valid_o <= 1'b0;
      if (take) begin
        for (int i = 0; i < 12; i++) m_o[i] <= res[i];
        m_bone_o <= q_bone_i;
        m_valid_o <= 1'b1;
        // Counts bones ACCEPTED, not offered: a bone held off by backpressure
        // is not decoded work done.
        if (bones_decoded_o != 32'hFFFF_FFFF) bones_decoded_o <= bones_decoded_o + 32'd1;
      end
    end
  end

endmodule : zhao_geom_quat2mat
