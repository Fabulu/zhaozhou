// zhao_field_sin.sv — the Field IR sine and cosine, OP_SIN and OP_COS.
//
// A submodule of the FIELD.SEQ.* family. Reference: `zref::fx_sin` and
// `zref::fx_cos` (reference/include/zref/zref_trig.hpp §7.1), which is what
// `zfield::interpret` calls for those two opcodes.
//
// The quarter-wave table lives in `zhao_field_sin_rom.sv`, GENERATED from
// `zref_tables.hpp` rather than transcribed, and checked entry by entry in the
// test.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
// The angle is `angle16`: a u16 of TURNS, so the whole circle is 65,536 and
// there is no range reduction to get wrong — the wrap is the width.
//
//     q   = a >> 14                       the quadrant, 0..3
//     a13 = a & 0x3FFF                    the position within it
//     s   = sin_quarter( (q odd) ? 0x4000 - a13 : a13 )
//     sin = (q & 2) ? -s : s
//
//     sin_quarter(v):  i = v >> 6,  t = v & 0x3F
//                      base = SIN_Q16[i]
//                      i == 256 -> base            (the endpoint, see below)
//                      d = SIN_Q16[i+1] - base
//                      base + ((d*t + 32) >> 6)
//
//     cos(a) = sin(a + 0x4000)            exact by construction
// ENFORCED-BY: tests/differential/field_sin_directed.cpp:main
//
// Four things are load-bearing:
//
// 1. **THE i == 256 ENDPOINT IS SAFE FOR A REASON WORTH KNOWING.** At the
//    quarter-turn endpoint the reference would read `SIN_Q16[257]`, one past a
//    257-entry table — a hard compile error in constexpr and an out-of-bounds
//    read at runtime. That is a C++ MEMORY-SAFETY problem, and its guard exists
//    for that; the reference says so itself, "t is 0 there, so the returned
//    VALUE is unchanged".
//
//    In RTL there is no memory to read past: the index is clamped so the ROM is
//    always addressed in range. And the VALUE needs no guard at all, because
//    `t == 0` at the endpoint makes the interpolation contribute nothing however
//    the slope came out. Mutation confirms it — removing the value guard passes
//    the exhaustive 65,536-angle test, an equivalent mutant.
//
//    Both the clamp and the guard stay. The clamp is load-bearing (it keeps the
//    ROM address in range); the guard is redundant and kept because it mirrors
//    the reference line for line, which is what makes the two readable side by
//    side.
// 2. **THE ODD QUADRANT MIRRORS, IT DOES NOT NEGATE.** `0x4000 - a13` reflects
//    the quarter wave; negating instead would give the right answer only at the
//    two ends.
// 3. **THE INTERPOLATION ROUNDS HALF UP**: `(d*t + 32) >> 6`, and `d` is SIGNED
//    — the table falls as well as rises across the quarter, so an unsigned
//    shift would wrap the descending half of every wave.
// 4. **COS IS SIN OF A SHIFTED ANGLE, AND THE SHIFT WRAPS.** `a + 0x4000` in
//    sixteen bits. Widening it first and then indexing would run off the top
//    quadrant instead of wrapping to the bottom.
//
// The endpoint is the full `0x10000`, exactly 1.0, not one ulp below it — which
// is why the table is seventeen bits wide and the result of `sin(0x4000)` is
// exactly one.
module zhao_field_sin (
    // Combinational: the sequencer owns the pipeline.
    input  logic [15:0] angle_i,
    input  logic        is_cos_i,  // 0 = OP_SIN, 1 = OP_COS

    output logic signed [31:0] result_o
);

  // cos(a) = sin(a + 0x4000), and the add WRAPS in sixteen bits.
  logic [15:0] a;
  assign a = is_cos_i ? (angle_i + 16'h4000) : angle_i;

  logic [ 1:0] q;
  logic [13:0] a13;
  assign q = a[15:14];
  assign a13 = a[13:0];

  // The odd quadrant MIRRORS about the quarter turn: 0x4000 - a13.
  //
  // Fifteen bits, not fourteen, and that is the endpoint case rather than
  // caution: when a13 == 0 the mirror is 0x4000 exactly, which does not fit
  // fourteen bits, and it is precisely the input that must drive `i` to 256.
  // Wrapping it to zero would silently return sin(0) for sin(quarter turn).
  logic [14:0] v15;
  assign v15 = q[0] ? (15'h4000 - {1'b0, a13}) : {1'b0, a13};

  logic [8:0] i;
  logic [5:0] t;
  assign i = v15[14:6];
  assign t = v15[5:0];

  logic [16:0] base, next_v;
  logic [8:0]  i_next;
  assign i_next = (i == 9'd256) ? 9'd256 : (i + 9'd1);

  zhao_field_sin_rom u_base (
      .idx_i(i),
      .val_o(base)
  );
  zhao_field_sin_rom u_next (
      .idx_i(i_next),
      .val_o(next_v)
  );

  // `d` is SIGNED: the quarter wave rises to 1.0, so within it d >= 0, but the
  // subtraction is written signed anyway because nothing here should depend on
  // the table's monotonicity being remembered.
  logic signed [17:0] d;

  // THE ONE NONCONSTANT MULTIPLY OUTSIDE `zhao_field_mul`, AND IT COSTS A DSP.
  //
  // Measured 2026-08-23: the Field cone fits in 4 DSP blocks, of which
  // `zhao_field_mul` is 3 and THIS interpolation is the fourth. The DSP ruling
  // says no production op unit keeps a private nonconstant multiplier, and an
  // 18x6 product is one.
  //
  // Routing it through the shared lane would be the literal fix and a bad one:
  // the sine table is combinational, so OP_SIN and OP_COS cost exactly what an
  // ADD costs, and both of ROT's table reads sit inside its walk. Sequencing
  // this product would make SIN and COS multi-cycle and lengthen every rotation
  // by six clocks, to save a DSP on a device with 108 spare.
  //
  // So the multiplier is kept and the DSP is not: `multstyle = "logic"` is a
  // Quartus synthesis directive that builds the product from ALMs instead. Six
  // bits of multiplier is a six-term shift-add; the fitter is better at that
  // than a DSP block is. The attribute is invisible to Verilator and to slang,
  // so simulation and the formal proof see exactly the same arithmetic.
  // ENFORCED-BY: tests/differential/field_sin_directed.cpp:main
  (* multstyle = "logic" *) logic signed [24:0] interp;
  logic signed [31:0] s_quarter;
  always_comb begin
    d = $signed({1'b0, next_v}) - $signed({1'b0, base});
    interp = ($signed({{7{d[17]}}, d}) * $signed({19'd0, t}) + 25'sd32) >>> 6;
    s_quarter = (i == 9'd256) ? 32'($signed({1'b0, base}))
                              : (32'($signed({1'b0, base})) + 32'(interp));
  end

  // The upper half of the circle is negative.
  assign result_o = q[1] ? -s_quarter : s_quarter;

endmodule : zhao_field_sin
