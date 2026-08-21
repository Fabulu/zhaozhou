// zhao_field_len.sv — the Field IR length ops: OP_LEN2, OP_LEN3, OP_DIST2.
//
// A submodule of the FIELD.SEQ.* family. Reference: the interpreter's `len_of`
// (§3.11) and its OP_DIST2 case (reference/src/zfield/zfield_interpret.cpp).
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     LEN2  : sqrt( a0^2 + a1^2 )
//     LEN3  : sqrt( a0^2 + a1^2 + a2^2 )
//     DIST2 : d_i = fx_sub(a_i, b_i)  then  sqrt( d0^2 + d1^2 )
//
// with the sum of squares EXACT in u64 and the root the exact integer floor
// (`zhao_field_isqrt`), then a saturating narrow to s32 recorded in the
// `rescale` lane.
//
// Four things are load-bearing:
//
// 1. **THE SUM OF SQUARES IS UNSIGNED AND EXACT.** Each square is up to
//    (2^31)^2 = 2^62, and three of them reach 3*2^62 — which does NOT fit s64
//    but does fit u64. The reference accumulates into a `uint64_t` for exactly
//    that reason. A signed accumulator overflows on three large lanes and the
//    length comes out negative.
// 2. **THE DIFFERENCE IN DIST2 SATURATES FIRST.** It is `fx_sub`, not a plain
//    subtract: `a - b` is formed in s64 and saturated to s32 BEFORE squaring.
//    Squaring an unsaturated difference would give a different distance for far
//    apart points, and the saturation is recorded in the `add` lane.
// 3. **THE ROOT IS A FLOOR, NEVER ROUNDED.** `res^2 <= n < (res+1)^2`. Rounding
//    to nearest would be a better length and would disagree with the software
//    everywhere the root is inexact, which is almost everywhere.
// 4. **THE NARROW SATURATES INTO `rescale`, NOT `mul`.** The reference keeps
//    four ledger lanes apart; a block recording in the wrong one can still
//    return the right number and be wrong everywhere else.
//
// The result of a length is always non-negative, so no sign is reapplied — and
// the saturating narrow only ever hits the TOP rail.
module zhao_field_len (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic        [ 1:0] mode_i,   // 0 = LEN2, 1 = LEN3, 2 = DIST2
    input  logic signed [31:0] a0_i,
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,
    input  logic signed [31:0] b0_i,
    input  logic signed [31:0] b1_i,

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] result_o,
    output logic               sat_add_o,      // the DIST2 differences
    output logic               sat_rescale_o   // the narrow to s32
);

  // M_LEN2 (2'd0) is the fall-through case and so is never named in a
  // comparison; it is documented here rather than declared, because an unused
  // localparam is noise a linter is right to flag.
  localparam logic [1:0] M_LEN3 = 2'd1;
  localparam logic [1:0] M_DIST2 = 2'd2;

  // ---- fx_sub, for the DIST2 lanes ----------------------------------------
  function automatic logic signed [31:0] fx_sub_sat(input logic signed [31:0] a,
                                                    input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      if (d > 33'sd2147483647) fx_sub_sat = 32'sh7FFF_FFFF;
      else if (d < -33'sd2147483648) fx_sub_sat = 32'sh8000_0000;
      else fx_sub_sat = d[31:0];
    end
  endfunction

  function automatic logic fx_sub_fired(input logic signed [31:0] a,
                                        input logic signed [31:0] b);
    logic signed [32:0] d;
    begin
      d = $signed({a[31], a}) - $signed({b[31], b});
      fx_sub_fired = (d > 33'sd2147483647) || (d < -33'sd2147483648);
    end
  endfunction

  // ---- the lanes actually squared -----------------------------------------
  logic signed [31:0] l0, l1, l2;
  logic               sub_sat;
  always_comb begin
    if (mode_i == M_DIST2) begin
      l0 = fx_sub_sat(a0_i, b0_i);
      l1 = fx_sub_sat(a1_i, b1_i);
      l2 = 32'sd0;
      sub_sat = fx_sub_fired(a0_i, b0_i) || fx_sub_fired(a1_i, b1_i);
    end else begin
      l0 = a0_i;
      l1 = a1_i;
      l2 = (mode_i == M_LEN3) ? a2_i : 32'sd0;
      sub_sat = 1'b0;
    end
  end

  // ---- the sum of squares, UNSIGNED and exact -----------------------------
  // Each square is at most 2^62 and three of them reach 3*2^62, which overflows
  // s64 and fits u64. The reference uses uint64_t for exactly this.
  function automatic logic [63:0] sq(input logic signed [31:0] v);
    logic [31:0] m;
    begin
      m = v[31] ? (~$unsigned(v) + 32'd1) : $unsigned(v);
      sq = 64'(m) * 64'(m);
    end
  endfunction

  logic [63:0] n2;
  assign n2 = sq(l0) + sq(l1) + sq(l2);

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

  // The block is a thin shell around the root: it accepts a request when the
  // root can take one, and holds the flags that travel with it.
  logic held_sub_sat;

  assign v_ready_o = sq_ready && (!r_valid_o || r_ready_i);
  assign sq_valid = v_valid_i && (!r_valid_o || r_ready_i);
  assign rt_ready = !r_valid_o || r_ready_i;

  logic over;
  assign over = (rt > 64'd2147483647);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      held_sub_sat <= 1'b0;
      r_valid_o <= 1'b0;
      result_o <= '0;
      sat_add_o <= 1'b0;
      sat_rescale_o <= 1'b0;
    end else begin
      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;

      // The difference-saturation flag is decided at accept and rides the
      // request through the root, which takes 34 cycles.
      if (v_valid_i && v_ready_o) held_sub_sat <= sub_sat;

      if (rt_valid && rt_ready) begin
        // A length is never negative, so only the TOP rail is reachable.
        result_o <= over ? 32'sh7FFF_FFFF : $signed(rt[31:0]);
        sat_rescale_o <= over;
        sat_add_o <= held_sub_sat;
        r_valid_o <= 1'b1;
      end
    end
  end

endmodule : zhao_field_len
