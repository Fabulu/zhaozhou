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

  // THE INTERPOLATION IS A SHIFT-ADD, NOT A MULTIPLY, AND THAT IS THE DSP RULE.
  //
  // Measured 2026-08-23: the Field cone fitted in 4 DSP blocks, of which
  // `zhao_field_mul` was 3 and THIS interpolation was the fourth. The ruling
  // says no production op unit keeps a private nonconstant multiplier, and an
  // 18x6 product written as `d * t` is one.
  //
  // Routing it through the shared lane would be the literal fix and a bad one:
  // the sine table is combinational, so OP_SIN and OP_COS cost exactly what an
  // ADD costs, and both of ROT's table reads sit inside its walk. Sequencing
  // this product would make SIN and COS multi-cycle and lengthen every rotation
  // by six clocks, to save one DSP on a device with 108 spare.
  //
  // `(* multstyle = "logic" *)` was tried first and Quartus 17.0.2 SILENTLY
  // IGNORED IT -- no warning, still four DSP blocks. So the product is written
  // as what it is: `t` is SIX BITS, so `d * t` is a six-term shift-add, and
  // six terms is a shape the fitter builds better from ALMs than from a DSP.
  //
  // THIS IS THE SAME NUMBER, not an approximation. Each term is `d << k` for a
  // set bit of `t`, accumulated exactly in 25 signed bits; `|d| <= 2^16` and
  // `t <= 63`, so the sum is at most about 2^22 and cannot overflow. It is the
  // definition of multiplication, unrolled -- which is why the differential is
  // unchanged and still passes bit for bit.
  // ENFORCED-BY: tests/differential/field_sin_directed.cpp:main
  //
  // WAVE 5, 2026-08-25: THE SIX TERMS ARE SUMMED AS A BALANCED TREE, NOT AS A
  // RUNNING TOTAL.
  //
  // Measured at 36.84 MHz, the sequencer's worst path had EIGHTY of its cells
  // inside this unit and ZERO in any other -- `u_isqrt`, `u_rcp`, `u_curve`,
  // `u_noise`, `u_ring`, `u_rot`, `u_alu`, `u_mul`, `u_norm` and `u_len` all
  // contributed nothing. It was a `cin`/`cout` ripple the length of the cone.
  //
  // The cause was the accumulation ORDER, not the arithmetic: `dt = dt + term`
  // six times is six DEPENDENT 25-bit adds, so term 0's carry must settle
  // before term 5 can begin. Pairwise summation uses the same adders at depth
  // THREE. The rounding constant joins as a fourth leaf rather than a seventh
  // add, so it costs no extra level.
  //
  // IT IS THE SAME NUMBER, bit for bit, and not by approximation: integer
  // addition is associative, each term is exact in 25 signed bits, and the
  // bound above (|d| <= 2^16, t <= 63, sum about 2^22) holds for every
  // ordering, so no intermediate can overflow. The differential is unchanged.
  logic signed [24:0] term [6];
  logic signed [24:0] pair0, pair1, pair2, pair3, quad0, quad1;
  logic signed [24:0] dt;
  logic signed [24:0] interp;
  logic signed [31:0] s_quarter;
  always_comb begin
    d = $signed({1'b0, next_v}) - $signed({1'b0, base});
    for (int k = 0; k < 6; k++) term[k] = t[k] ? (25'(d) <<< k) : 25'sd0;
    pair0 = term[0] + term[1];
    pair1 = term[2] + term[3];
    pair2 = term[4] + term[5];
    // WAVE 6: `base` joins the tree as an eighth leaf instead of being added
    // after the shift. `base + (X >>> 6)` and `((base <<< 6) + X) >>> 6` are
    // EQUAL, not approximately: `base <<< 6` is a multiple of 64 and the shift
    // is an arithmetic floor, so the shifted-out bits belong entirely to X and
    // `base` cannot influence them. Seven leaves were already depth 3 and eight
    // still are, so this removes a full-width serial add for no extra level.
    pair3 = 25'sd32 + (25'($signed({1'b0, base})) <<< 6);
    quad0 = pair0 + pair1;
    quad1 = pair2 + pair3;
    dt    = quad0 + quad1;
    interp = dt >>> 6;
    // The i == 256 arm stays because it mirrors the reference line for line.
    // It is redundant, and wave 6 makes it MORE obviously so: at the endpoint
    // `t` is 0, so every `term` is 0 and the tree reduces to `pair3`, giving
    // `((base <<< 6) + 32) >>> 6` == `base` exactly. Both arms return the same
    // value for every reachable input.
    //
    // PROVEN-EQUIVALENT MUTANT, and the prediction was recorded BEFORE the run:
    // M59 replaces this whole select with `32'(interp)` and SURVIVES the
    // exhaustive 65,536-angle differential, as the argument above says it must.
    // Had it been caught, the exactness claim behind wave 6 would have been
    // wrong and the fold would need re-examining -- which is why the prediction
    // was written down first rather than the label applied afterwards.
    s_quarter = (i == 9'd256) ? 32'($signed({1'b0, base})) : 32'(interp);
  end

  // The upper half of the circle is negative.
  assign result_o = q[1] ? -s_quarter : s_quarter;

endmodule : zhao_field_sin
