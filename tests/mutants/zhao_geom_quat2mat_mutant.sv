// zhao_geom_quat2mat_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to make the R4 sequencer's NEW machinery -- the operand-mux
// schedule feeding the shared multiplier's product bank -- a demonstrated
// instrument rather than an argument. The sequencer's characteristic fault is
// not a wrong product but a RIGHT product in the WRONG bank slot: every
// handshake completes, every counter balances, the walk length is exactly the
// declared 10 cycles, and only the matrix elements that read the swapped
// slots move. Exactly the class of corruption the pre-R4 spatial tests could
// never see, because the spatial arm has no schedule to break.
//
// The one substantive change, in g_seq's operand mux:
//
//     4'd7: begin m_a = w_q; m_b = y_q; end      // wy -> p_q[7]
//     4'd8: begin m_a = w_q; m_b = z_q; end      // wz -> p_q[8]
//  -> 4'd7: begin m_a = w_q; m_b = z_q; end      // wz lands in p_q[7]
//  -> 4'd8: begin m_a = w_q; m_b = y_q; end      // wy lands in p_q[8]
//
// so m1/m2/m4/m8 (the elements mixing wy and wz) are wrong whenever
// wy != wz -- and EXACTLY right for any quaternion where they coincide,
// which is why the control also drives an identity quat to show the break
// passing a weak vector.
//
// INVERTED POLARITY: driven by tests/geometry/geom_quat2mat_mutant_control.cpp
// against the same zref oracle the directed suite uses; the control PASSES
// when the differential comparison FAILS on the schedule-sensitive vector.
// Evidence about the instrument, not about the design.
//
// The module is RENAMED so a source-list mistake can never elaborate it in
// place of the real one, and it lives under tests/ where
// check_forbidden_sources.py will not find it in a production closure.
//
// REGENERATE IT if zhao_geom_quat2mat.sv changes shape: this is a copy, and a
// copy of an old version is a positive control for a block that no longer
// exists.

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
// MUL_LANES — owner ruling R4, 2026-09-09
// ---------------------------------------------------------------------------
// Fabian: "relax the 1 bone/clock rule. It must have been arbitrary."
// (reports/OWNER-RULINGS-20260909-2300.md §R4; the relaxation is recorded at
// GEOM.POSE.md's throughput-target line.)
//
// MUL_LANES = 1 (DEFAULT): ONE shared 16x16 multiplier walked across the nine
//   products by an operand-mux sequencer, on the zhao_terrain_normals.sv:203
//   pattern. 1 DSP. Latency: valid rises 10 cycles after accept (9 product
//   captures + 1 output-forming cycle). The operand mux is a 4:1 of 16-bit
//   lanes per side — small enough that the input-cone concern that pushed
//   zhao_project_core to a shifting hold bank does not arise here.
// MUL_LANES = 9: the original spatial arrangement — all nine products exist
//   at once, valid rises 1 cycle after accept. 9 DSP.
//
// The two arms are BIT-IDENTICAL in every output: the products are exact
// integers whichever cycle they are computed in, and the sums and the single
// rescale are shared. Sequencing moves cycles, never bits.
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
// DSP COST at MUL_LANES=1: one 16x16 product — 1 DSP by the measured
// calibration (tools/budget/calibration.json: 1 DSP from 8 to 27 bits).
// At MUL_LANES=9: nine, which is what this block measured before R4.
module zhao_geom_quat2mat_mutant #(
    // 1 = one shared multiplier lane (default, ruling R4); 9 = spatial.
    parameter int MUL_LANES = 1
) (
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

  // Quartus 17 needs elaboration checks inside `initial begin ... end` (a
  // bare module-scope `if` is a synthesis syntax error there), and
  // `--lint-only` does not run this block — only elaboration does.
  initial begin
    if (MUL_LANES != 1 && MUL_LANES != 9) begin
      $fatal(1, "zhao_geom_quat2mat_mutant: MUL_LANES must be 1 or 9, got %0d", MUL_LANES);
    end
  end

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

  generate
    if (MUL_LANES == 9) begin : g_spatial

      // ---- the nine products, exact in s33 --------------------------------
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
            // Counts bones ACCEPTED, not offered: a bone held off by
            // backpressure is not decoded work done.
            if (bones_decoded_o != 32'hFFFF_FFFF) bones_decoded_o <= bones_decoded_o + 32'd1;
          end
        end
      end

    end else begin : g_seq

      // ---- the sequenced arm: one multiplier, nine steps -------------------
      // Product bank index map (the schedule and the sum expressions must
      // agree; the committed mutant breaks exactly this agreement):
      //   0 xx   1 yy   2 zz   3 xy   4 xz   5 yz   6 wx   7 wy   8 wz
      logic signed [15:0] w_q, x_q, y_q, z_q;
      logic        [ 7:0] bone_q;
      logic               busy;
      logic        [ 3:0] mseq;        // 0..8 product steps, 9 = form outputs
      logic signed [31:0] p_q [9];

      logic signed [15:0] m_a, m_b;
      always_comb begin
        unique case (mseq)
          4'd0: begin m_a = x_q; m_b = x_q; end
          4'd1: begin m_a = y_q; m_b = y_q; end
          4'd2: begin m_a = z_q; m_b = z_q; end
          4'd3: begin m_a = x_q; m_b = y_q; end
          4'd4: begin m_a = x_q; m_b = z_q; end
          4'd5: begin m_a = y_q; m_b = z_q; end
          4'd6: begin m_a = w_q; m_b = x_q; end
          // MUTANT: the two case bodies below are SWAPPED (the one
          // substantive change) -- wz lands in the wy slot and vice versa.
          4'd7: begin m_a = w_q; m_b = z_q; end
          4'd8: begin m_a = w_q; m_b = y_q; end
          default: begin m_a = 16'sd0; m_b = 16'sd0; end
        endcase
      end

      // THE ONE nonconstant multiply in this arm. 16x16 -> s32, exact.
      wire signed [31:0] m_p = m_a * m_b;

      // The same twelve expressions as the spatial arm, fed from the bank.
      logic signed [31:0] res [12];
      always_comb begin
        res[0]  = 32'sd65536 - rescale_sat(34'(p_q[1]) + 34'(p_q[2]));  // yy+zz
        res[1]  = rescale_sat(34'(p_q[3]) - 34'(p_q[8]));               // xy-wz
        res[2]  = rescale_sat(34'(p_q[4]) + 34'(p_q[7]));               // xz+wy
        res[3]  = '0;
        res[4]  = rescale_sat(34'(p_q[3]) + 34'(p_q[8]));               // xy+wz
        res[5]  = 32'sd65536 - rescale_sat(34'(p_q[0]) + 34'(p_q[2]));  // xx+zz
        res[6]  = rescale_sat(34'(p_q[5]) - 34'(p_q[6]));               // yz-wx
        res[7]  = '0;
        res[8]  = rescale_sat(34'(p_q[4]) - 34'(p_q[7]));               // xz-wy
        res[9]  = rescale_sat(34'(p_q[5]) + 34'(p_q[6]));               // yz+wx
        res[10] = 32'sd65536 - rescale_sat(34'(p_q[0]) + 34'(p_q[1]));  // xx+yy
        res[11] = '0;
      end

      logic take;
      assign q_ready_o = !busy && (!m_valid_o || m_ready_i);
      assign take = q_valid_i && q_ready_o;

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          busy <= 1'b0;
          mseq <= '0;
          w_q <= '0; x_q <= '0; y_q <= '0; z_q <= '0;
          bone_q <= '0;
          m_valid_o <= 1'b0;
          m_bone_o <= '0;
          bones_decoded_o <= '0;
          for (int i = 0; i < 9; i++) p_q[i] <= '0;
          for (int i = 0; i < 12; i++) m_o[i] <= '0;
        end else begin
          if (m_valid_o && m_ready_i) m_valid_o <= 1'b0;
          if (take) begin
            w_q <= q_w_i; x_q <= q_x_i; y_q <= q_y_i; z_q <= q_z_i;
            bone_q <= q_bone_i;
            mseq <= '0;
            busy <= 1'b1;
            // Counts bones ACCEPTED, not offered — the same law as the
            // spatial arm, at the same handshake edge.
            if (bones_decoded_o != 32'hFFFF_FFFF) bones_decoded_o <= bones_decoded_o + 32'd1;
          end else if (busy) begin
            if (mseq != 4'd9) begin
              p_q[mseq] <= m_p;
              mseq <= mseq + 4'd1;
            end else begin
              for (int i = 0; i < 12; i++) m_o[i] <= res[i];
              m_bone_o <= bone_q;
              m_valid_o <= 1'b1;
              busy <= 1'b0;
            end
          end
        end
      end

    end
  endgenerate

endmodule : zhao_geom_quat2mat_mutant
