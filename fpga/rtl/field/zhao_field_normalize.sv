// zhao_field_normalize.sv — the Field IR normalise ops: OP_NORMALIZE2 and
// OP_NORMALIZE3.
//
// A submodule of the FIELD.SEQ.* family. Reference: the interpreter's
// `normalize2` (§3.12) and `zref::normalize3_approx` (qformats §7.4), which are
// the same shape and are implemented here as one datapath over two or three
// lanes.
//
// The seed table lives in `zhao_field_rcp24_rom.sv`, GENERATED from
// `zref_tables.hpp`. It is the SECOND reciprocal table in this engine and it is
// NOT the one `zhao_field_rcp` uses — `FIELD_RCP_T0` seeds a 32-bit reciprocal
// with one correction step, `RCP24_T0` seeds a 24-bit one with two. Feeding
// either function the other's table would be invisible until some normalised
// vector came out slightly short.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     n2  = sum of squares, EXACT and unsigned
//     n2 == 0                    -> all lanes zero (see the asymmetry below)
//     len = isqrt_u64(n2)                       exact floor
//     normalise len into [2^23, 2^24), counting e
//     r   = rcp_u24_norm(m)                     TWO correction steps
//     out = rescale_s32(lane * r, 31 + e)       ONE rounding per lane
//
// Five things are load-bearing:
//
// 1. **ONE ROUNDING PER LANE.** `lane * r` is exact in s64 and rescaled once by
//    `31 + e`. The shift is a function of the vector's magnitude, not a
//    constant, which is what keeps the result accurate across the whole range —
//    and it is why `e` has to survive from the normalisation loop to the final
//    rescale rather than being folded away early.
// 2. **TWO CORRECTION STEPS, NOT ONE.** `rcp_u24_norm` iterates twice.
//    `field_rcp` next door iterates ONCE. The two functions are different and
//    the count is not a tuning knob.
// 3. **THE ZERO CASE IS ASYMMETRIC BETWEEN THE TWO OPS**, and this is the one
//    that looks like a bug and is not. `normalize2` bumps the `rcp0` ledger lane
//    on a zero vector; `normalize3_approx` returns zeros and bumps NOTHING. The
//    reference really does differ, so the RTL differs too, and the test pins
//    both. Making them consistent would be tidier and would disagree with every
//    capture the software has produced.
// 4. **THE SUM OF SQUARES IS UNSIGNED.** Three squares reach 3*2^62, which
//    overflows s64 and fits u64. The reference accumulates `normalize3`'s in a
//    128-bit unsigned type and hands it to a u64 root; the value always fits.
// 5. **THE ROOT IS A FLOOR.** Shared with `zhao_field_len`, same block, same
//    exactness argument.
module zhao_field_normalize (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic               is3_i,      // 0 = NORMALIZE2, 1 = NORMALIZE3
    input  logic signed [31:0] a0_i,
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] o0_o,
    output logic signed [31:0] o1_o,
    output logic signed [31:0] o2_o,
    output logic               rcp0_o,        // set only by NORMALIZE2, see law 3
    output logic               sat_rescale_o
);

  // ---- the sum of squares, unsigned and exact -----------------------------
  function automatic logic [63:0] sq(input logic signed [31:0] v);
    logic [31:0] m;
    begin
      m = v[31] ? (~$unsigned(v) + 32'd1) : $unsigned(v);
      sq = 64'(m) * 64'(m);
    end
  endfunction

  logic [63:0] n2;
  assign n2 = sq(a0_i) + sq(a1_i) + (is3_i ? sq(a2_i) : 64'd0);

  logic is_zero;
  assign is_zero = (n2 == 64'd0);

  // ---- the exact integer square root --------------------------------------
  logic        sq_valid, sq_ready, rt_valid, rt_ready;
  logic [63:0] rt;

  zhao_field_isqrt u_isqrt (
      .clk(clk),
      .rst_n(rst_n),
      .n_valid_i(sq_valid),
      .n_ready_o(sq_ready),
      .n_i(n2),
      .r_valid_o(rt_valid),
      .r_ready_i(rt_ready),
      .r_o(rt)
  );

  // ---- normalise the length into [2^23, 2^24), counting e -----------------
  // The reference walks two while-loops. `len` is at most 2^32, so the shift is
  // bounded and the loops become a leading-zero count either way; both
  // directions are needed because a short vector's length is below 2^23.
  logic signed [7:0] e_val;
  logic [23:0] m_val;
  always_comb begin
    logic [63:0] t;
    logic signed [7:0] ee;
    t = rt;
    ee = 8'sd0;
    if (t != 64'd0) begin
      for (int k = 0; k < 64; k++) begin
        if (t < (64'd1 << 23)) begin
          t = t << 1;
          ee = ee - 8'sd1;
        end
      end
      for (int k = 0; k < 64; k++) begin
        if (t >= (64'd1 << 24)) begin
          t = t >> 1;
          ee = ee + 8'sd1;
        end
      end
    end
    m_val = t[23:0];
    e_val = ee;
  end

  // ---- rcp_u24_norm: seed, TWO correction steps, then rescale by 7 --------
  logic [ 7:0] idx;
  logic [30:0] seed;
  assign idx = m_val[22:15];  // (m - 2^23) >> 15, with bit 23 known set

  zhao_field_rcp24_rom u_rom (
      .idx_i(idx),
      .seed_o(seed)
  );

  function automatic logic [63:0] resc_u(input logic [63:0] v, input int unsigned k);
    resc_u = (k == 0) ? v : ((v + (64'd1 << (k - 1))) >> k);
  endfunction

  logic [63:0] x0, p0, w0, x1, p1, w1, x2;
  logic [31:0] r24;
  always_comb begin
    x0 = {33'd0, seed};
    p0 = 64'({40'd0, m_val}) * x0;
    w0 = p0 >> 24;
    x1 = resc_u(x0 * ((64'd2 << 30) - w0), 30);
    p1 = 64'({40'd0, m_val}) * x1;
    w1 = p1 >> 24;
    x2 = resc_u(x1 * ((64'd2 << 30) - w1), 30);
    r24 = 32'(resc_u(x2, 7));
    if (r24 > 32'h00FF_FFFF) r24 = 32'h00FF_FFFF;
  end

  // ---- one rescale per lane, by 31 + e ------------------------------------
  logic [7:0] shift_amt;
  assign shift_amt = 8'(8'sd31 + e_val);

  function automatic logic signed [31:0] resc_s(input logic signed [63:0] v,
                                                input logic [7:0] k);
    logic signed [64:0] r;
    begin
      r = (k == 8'd0) ? 65'(v) : ((65'(v) + (65'sd1 <<< (k - 8'd1))) >>> k);
      if (r > 65'sd2147483647) resc_s = 32'sh7FFF_FFFF;
      else if (r < -65'sd2147483648) resc_s = 32'sh8000_0000;
      else resc_s = r[31:0];
    end
  endfunction

  function automatic logic resc_s_fired(input logic signed [63:0] v, input logic [7:0] k);
    logic signed [64:0] r;
    begin
      r = (k == 8'd0) ? 65'(v) : ((65'(v) + (65'sd1 <<< (k - 8'd1))) >>> k);
      resc_s_fired = (r > 65'sd2147483647) || (r < -65'sd2147483648);
    end
  endfunction

  // The operands are held from accept, because the root takes 34 cycles and the
  // caller's inputs are not required to stay put.
  logic signed [31:0] h_a0, h_a1, h_a2;
  logic               h_is3, h_zero;

  logic signed [63:0] m0, m1, m2;
  always_comb begin
    m0 = $signed({{32{h_a0[31]}}, h_a0}) * $signed({32'd0, r24});
    m1 = $signed({{32{h_a1[31]}}, h_a1}) * $signed({32'd0, r24});
    m2 = $signed({{32{h_a2[31]}}, h_a2}) * $signed({32'd0, r24});
  end

  assign v_ready_o = sq_ready && (!r_valid_o || r_ready_i);
  assign sq_valid = v_valid_i && (!r_valid_o || r_ready_i);
  assign rt_ready = !r_valid_o || r_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      h_a0 <= '0; h_a1 <= '0; h_a2 <= '0;
      h_is3 <= 1'b0; h_zero <= 1'b0;
      r_valid_o <= 1'b0;
      o0_o <= '0; o1_o <= '0; o2_o <= '0;
      rcp0_o <= 1'b0;
      sat_rescale_o <= 1'b0;
    end else begin
      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;

      if (v_valid_i && v_ready_o) begin
        h_a0 <= a0_i;
        h_a1 <= a1_i;
        h_a2 <= is3_i ? a2_i : 32'sd0;
        h_is3 <= is3_i;
        h_zero <= is_zero;
      end

      if (rt_valid && rt_ready) begin
        if (h_zero) begin
          o0_o <= '0; o1_o <= '0; o2_o <= '0;
          // Law 3: NORMALIZE2 records rcp0 on a zero vector, NORMALIZE3 does
          // not. The reference really is asymmetric here.
          rcp0_o <= !h_is3;
          sat_rescale_o <= 1'b0;
        end else begin
          o0_o <= resc_s(m0, shift_amt);
          o1_o <= resc_s(m1, shift_amt);
          o2_o <= h_is3 ? resc_s(m2, shift_amt) : 32'sd0;
          rcp0_o <= 1'b0;
          sat_rescale_o <= resc_s_fired(m0, shift_amt) || resc_s_fired(m1, shift_amt) ||
                           (h_is3 && resc_s_fired(m2, shift_amt));
        end
        r_valid_o <= 1'b1;
      end
    end
  end

endmodule : zhao_field_normalize
