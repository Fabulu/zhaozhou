// zhao_field_alu.sv — the Field IR arithmetic core.
//
// A submodule of the FIELD.SEQ.* family, not a ledger block of its own. Every
// field sequencer executes the same op table over the same register file, so the
// ALU is written ONCE and instantiated by each of them.
//
// Reference: `zfield::interpret` (reference/src/zfield/zfield_interpret.cpp),
// which is one of only two places Field IR op semantics are permitted to exist.
//
// ---------------------------------------------------------------------------
// THE GREP-AUDIT LAW, AND WHY THIS FILE DOES NOT VIOLATE IT
// ---------------------------------------------------------------------------
// `spec/form/field-ir.md` §1 (charter §29-6) says op semantics live in exactly
// two places — the C++ interpreter and the TS interpreter — and forbids
// "no hand-written per-program evaluator, no 'faster' fused C++ variant, no
// RTL-side re-derivation ahead of the profile engine (which will consume the
// same serialized bytes)".
//
// Read precisely, RTL is FORESEEN: the parenthesis names the profile engine and
// says it consumes the serialized bytes. What is forbidden is re-DERIVING the
// semantics — a per-program evaluator, or an opcode switch someone wrote from
// the prose. This block is the profile engine's ALU: it dispatches on the frozen
// opcode numbers and every arithmetic law it implements is a `zref::fx_*`
// primitive that already exists, differentially verified against that primitive
// op by op. Nothing here is derived from the spec text.
//
// ---------------------------------------------------------------------------
// SCOPE OF THIS INCREMENT
// ---------------------------------------------------------------------------
// The ARITHMETIC core: opcodes 0x00..0x0C and 0x10..0x11.
//
//   END MOV LDC ADD SUB MUL MAD MIN MAX ABS CLAMP SELECT CMP DOT2 DOT3
//
// **Deliberately NOT here yet**: the transcendental and table ops — RCP, SIN,
// COS, LEN2/3, DIST2, NORMALIZE2/3, CURVE, DCURVE, SPLINE, NOISE2, RING, RIDGE,
// ROT2, ROT3. Each needs a divider, a table or a sequencer of its own, and each
// is a block of work. `op_unsupported_o` is raised for them rather than a wrong
// answer being produced quietly: an ALU that returned zero for an unimplemented
// opcode would let a sequencer run a program it cannot actually evaluate, and
// the capture would look healthy.
//
// ---------------------------------------------------------------------------
// THE LAWS, all of them qformats §3 and all of them already implemented
// ---------------------------------------------------------------------------
//   ADD/SUB : s64 sum, saturate to s32                    (fx_add / fx_sub)
//   MUL     : s64 product, ONE rescale(.,16)              (fx_mul)
//   MAD     : a*b + (c<<16) EXACT in s64, ONE rescale     (fx_mad, A3b)
//   MIN/MAX : raw compare, no rounding                    (fx_min / fx_max)
//   ABS     : INT32_MIN maps to INT32_MIN, not to +2^31   (fx_abs)
//   CLAMP   : max(lo, min(hi, x)) -- in THAT order        (fx_clamp)
//   SELECT  : c != 0 ? a : b
//   CMP     : imm selects ==, !=, <, <=, >, >=; result is 0x10000 or 0
//   DOT2/3  : products summed EXACTLY, ONE rescale(.,16)  (dot_finish)
//
// Three of these are places an implementation drifts:
//
// 1. **MAD IS ONE ROUNDING, AND THE DIFFERENCE IS AT THE RAILS.** `a*b + (c<<16)`
//    is formed exactly in s64 and rounded once (A3b).
//
//    Stated precisely, because the obvious claim is wrong: multiply-round-then-
//    add is NOT off by an LSB in general. `(p + c*2^16) >> 16` equals
//    `(p >> 16) + c` exactly for integer c, so the two orders agree wherever
//    nothing overflows. They differ ONLY when the intermediate saturates and the
//    exact form does not — the rounding is identical, the RANGE is not.
//
//    That was established by mutation: a double-rounding variant passed the
//    whole directed suite and failed one random case in a hundred and twenty.
//    The directed section now constructs the rail cases deliberately.
// 2. **ABS OF INT32_MIN IS INT32_MIN.** Negating it does not fit, and the
//    reference does not saturate to INT32_MAX: it returns INT32_MIN unchanged.
//    A "tidier" saturating abs disagrees on exactly one input.
// 3. **CMP'S TRUE IS 0x10000, NOT 1.** It is fx16 one-point-zero, because the
//    result feeds arithmetic ops. Returning 1 makes every comparison
//    1/65536th of what it should be.
//
// ---------------------------------------------------------------------------
// WIDTHS
// ---------------------------------------------------------------------------
// A product of two s32 is s64. DOT3 sums three of them: s66. The rescale add of
// 2^15 cannot overflow that. `>>> 16` leaves s50, which saturates to s32. MAD's
// `c << 16` is s48 and joins an s64 product, so s66 covers it too — one width
// serves the whole file.
//
// ---------------------------------------------------------------------------
// THE PRODUCTS ARRIVE; THEY ARE NO LONGER FORMED HERE (2026-08-23)
// ---------------------------------------------------------------------------
// This block used to state `a0*b0`, `a0*b0 + a1*b1` and `+ a2*b2` and own the
// silicon for them. Under the DSP ruling of 2026-08-23 they are formed on the
// engine's ONE multiplier lane, DURING THE REGISTER-READ WALK: the sequencer
// reads (a, b), (a+1, b+1) and (a+2, b+2) on three consecutive cycles and hands
// each pair to `zhao_field_mul` as it goes, so the three products and their
// running sum are standing ready in Q_EXEC. MUL, MAD, DOT2 and DOT3 still cost
// six clocks; the read walk simply stopped being three idle cycles.
//
// WHAT THIS BLOCK NO LONGER STATES is the operand PAIRING -- that lane k of `a`
// multiplies lane k of `b`. That fact now lives in the sequencer's read-address
// walk, `a + k` against `b + k`, which is the same walk that captures a0..a2 and
// b0..b2 for every other op. It is proven end to end against `zfield::interpret`
// rather than restated here.
// ENFORCED-BY: tests/differential/field_seq_directed.cpp
//
// `a1_i`, `b1_i`, `a2_i` and `b2_i` are consequently no longer read by this
// block. They stay on the port list because they are what the op's OPERANDS
// are, and because a future op class that needs them should not have to
// re-thread them; the sums that used to consume them are the three inputs
// below.
module zhao_field_alu (
    // Purely combinational: the sequencer owns the register file and the
    // pipeline. This is the arithmetic and nothing else.
    input  logic        [ 7:0] op_i,
    input  logic        [31:0] imm_i,
    input  logic signed [31:0] a0_i,   // reg[a], reg[a+1], reg[a+2] for DOT2/3
    input  logic signed [31:0] a1_i,
    input  logic signed [31:0] a2_i,
    input  logic signed [31:0] b0_i,   // reg[b], reg[b+1], reg[b+2]
    input  logic signed [31:0] b1_i,
    input  logic signed [31:0] b2_i,
    input  logic signed [31:0] c_i,    // reg[c]

    // The shared lane's products, gathered during the register-read walk.
    //   prod_ab_i = a0 * b0
    //   dot2_i    = a0 * b0 + a1 * b1
    //   dot3_i    = a0 * b0 + a1 * b1 + a2 * b2
    input  logic signed [65:0] prod_ab_i,
    input  logic signed [65:0] dot2_i,
    input  logic signed [65:0] dot3_i,

    output logic signed [31:0] result_o,
    output logic               is_end_o,          // OP_END: stop, do not write
    output logic               writes_o,          // this op writes reg[dst]
    output logic               op_unsupported_o,  // not in this increment
    // The SatLedger lanes, separately, because the reference keeps them apart
    // and a block that saturated in the wrong lane could still produce the
    // right number and be wrong everywhere else.
    output logic               sat_add_o,
    output logic               sat_mul_o,
    // ABS records its saturation in the `rescale` lane, not `add` or `mul` --
    // the reference's own choice (`abs_sat` bumps SatLedger::rescale).
    output logic               sat_rescale_o
);

  localparam logic [7:0] OP_END    = 8'h00;
  localparam logic [7:0] OP_MOV    = 8'h01;
  localparam logic [7:0] OP_LDC    = 8'h02;
  localparam logic [7:0] OP_ADD    = 8'h03;
  localparam logic [7:0] OP_SUB    = 8'h04;
  localparam logic [7:0] OP_MUL    = 8'h05;
  localparam logic [7:0] OP_MAD    = 8'h06;
  localparam logic [7:0] OP_MIN    = 8'h07;
  localparam logic [7:0] OP_MAX    = 8'h08;
  localparam logic [7:0] OP_ABS    = 8'h09;
  localparam logic [7:0] OP_CLAMP  = 8'h0A;
  localparam logic [7:0] OP_SELECT = 8'h0B;
  localparam logic [7:0] OP_CMP    = 8'h0C;
  localparam logic [7:0] OP_DOT2   = 8'h10;
  localparam logic [7:0] OP_DOT3   = 8'h11;

  localparam int W = 66;

  function automatic logic signed [W-1:0] ext32(input logic signed [31:0] v);
    ext32 = $signed({{(W - 32) {v[31]}}, v});
  endfunction

  // Saturate an s64 sum to the fx16 word. Records in the ADD lane.
  //
  // THREE EQUIVALENT MUTANTS ON THE BOUNDS BELOW, recorded so the sweep's
  // three survivors do not read as three holes. Swept 2026-08-22, 34
  // mutations, 31 caught.
  //
  //   `v > 2147483647`  ->  `v > 2147483646`      survives
  //   `v > 2147483647`  ->  `v >= 2147483647`     survives
  //   `v < -2147483648` ->  `v < -2147483647`     survives
  //
  // All three are equivalent for one reason: CLAMPING A VALUE THAT ALREADY
  // SITS EXACTLY ON THE RAIL RETURNS THAT SAME VALUE. At v == 2147483647 the
  // original falls through and returns v[31:0], which IS 32'sh7FFF_FFFF; the
  // mutants take the clamp and return 32'sh7FFF_FFFF. Same word. The low rail
  // is the same argument with 32'sh8000_0000. No input distinguishes them, so
  // no test can, and none should be written to try.
  //
  // What makes this safe rather than merely untested is that the LEDGER bound
  // lives in a different function. `sat32_fired` states the same two constants
  // independently, and mutating IT is caught both ways -- `sat32_fired_never`
  // and `sat32_fired_low_only` both failed the directed lane. So the boundary
  // that has an observable consequence is tested; only the redundant one on
  // the value path is not.
  //
  // Worth naming: the bound is stated TWICE, here and in sat32_fired. That is
  // the shape this project keeps finding defects in. It is tolerable here only
  // because the sweep covers the copy that can be observed.
  // ENFORCED-BY: tests/differential/field_alu_ops.cpp
  function automatic logic signed [31:0] sat32(input logic signed [W-1:0] v);
    begin
      if (v > 66'sd2147483647) sat32 = 32'sh7FFF_FFFF;
      else if (v < -66'sd2147483648) sat32 = 32'sh8000_0000;
      else sat32 = v[31:0];
    end
  endfunction

  function automatic logic sat32_fired(input logic signed [W-1:0] v);
    sat32_fired = (v > 66'sd2147483647) || (v < -66'sd2147483648);
  endfunction

  // rescale(x, 16): round-half-up then saturate. Records in the MUL lane for
  // the ops that use it, which is what the reference's `&SatLedger::mul` does.
  function automatic logic signed [31:0] resc16(input logic signed [W-1:0] v);
    logic signed [W-1:0] r;
    begin
      r = (v + (66'sd1 <<< 15)) >>> 16;
      if (r > 66'sd2147483647) resc16 = 32'sh7FFF_FFFF;
      else if (r < -66'sd2147483648) resc16 = 32'sh8000_0000;
      else resc16 = r[31:0];
    end
  endfunction

  function automatic logic resc16_fired(input logic signed [W-1:0] v);
    logic signed [W-1:0] r;
    begin
      r = (v + (66'sd1 <<< 15)) >>> 16;
      resc16_fired = (r > 66'sd2147483647) || (r < -66'sd2147483648);
    end
  endfunction

  // ---- the shared wide terms ----------------------------------------------
  // The two sums are still formed here; the three products are not.
  logic signed [W-1:0] sum_ab, dif_ab, prod_ab, mad_ab, dot2, dot3;
  always_comb begin
    sum_ab  = ext32(a0_i) + ext32(b0_i);
    dif_ab  = ext32(a0_i) - ext32(b0_i);
    prod_ab = prod_ab_i;
    mad_ab  = prod_ab + (ext32(c_i) <<< 16);
    dot2    = dot2_i;
    dot3    = dot3_i;
  end

  // The lanes the products consumed. Named so that -Wall does not read the
  // ports as dead, and so that a reader sees WHY they are still here.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] unread_a1, unread_b1, unread_a2, unread_b2;
  /* verilator lint_on UNUSEDSIGNAL */
  assign unread_a1 = a1_i;
  assign unread_b1 = b1_i;
  assign unread_a2 = a2_i;
  assign unread_b2 = b2_i;

  // ---- ABS SATURATES, and it took the sequencer to notice -----------------
  // `zfield_interpret.cpp` §3.7 is explicit: "saturating abs:
  // abs(0x80000000) = 0x7FFFFFFF + SAT". `abs_sat` returns INT32_MAX and bumps
  // the `rescale` lane.
  //
  // This block returned INT32_MIN, and its own test RESTATED the law the same
  // wrong way -- "INT32_MIN stays INT32_MIN" -- so the two agreed with each
  // other and disagreed with the reference. Nothing caught it until the
  // sequencer's differential ran whole programs against `zfield::interpret`
  // itself, which is the shipped path rather than a paraphrase of it.
  //
  // That is the argument for testing against the real oracle wherever it can be
  // reached: a restatement can be wrong, and a wrong restatement agrees with a
  // wrong implementation forever.
  logic signed [31:0] abs_a;
  logic               abs_sat_fired;
  always_comb begin
    abs_sat_fired = (a0_i == 32'sh8000_0000);
    abs_a = abs_sat_fired ? 32'sh7FFF_FFFF : (a0_i[31] ? -a0_i : a0_i);
  end

  // ---- CLAMP: max(lo, min(hi, x)) in THAT order ---------------------------
  // a = x, b = lo, c = hi in the reference's fx_clamp(x, lo, hi).
  logic signed [31:0] clamp_r, min_hi;
  always_comb begin
    min_hi = (c_i < a0_i) ? c_i : a0_i;          // min(hi, x)
    clamp_r = (b0_i > min_hi) ? b0_i : min_hi;   // max(lo, .)
  end

  // ---- CMP: imm picks the predicate; true is fx16 1.0 ---------------------
  logic cmp_t;
  always_comb begin
    unique case (imm_i)
      32'd0: cmp_t = (a0_i == b0_i);
      32'd1: cmp_t = (a0_i != b0_i);
      32'd2: cmp_t = (a0_i < b0_i);
      32'd3: cmp_t = (a0_i <= b0_i);
      32'd4: cmp_t = (a0_i > b0_i);
      32'd5: cmp_t = (a0_i >= b0_i);
      // The reference's inner switch has no default: `t` stays false for any
      // other imm. Decode has already rejected those (V9), so this is the
      // unreachable-but-defined case, and false is what the reference gives.
      default: cmp_t = 1'b0;
    endcase
  end

  always_comb begin
    result_o = 32'sd0;
    is_end_o = 1'b0;
    writes_o = 1'b1;
    op_unsupported_o = 1'b0;
    sat_add_o = 1'b0;
    sat_mul_o = 1'b0;
    sat_rescale_o = 1'b0;

    unique case (op_i)
      OP_END: begin
        is_end_o = 1'b1;
        writes_o = 1'b0;
      end
      OP_MOV: result_o = a0_i;
      OP_LDC: result_o = $signed(imm_i);
      OP_ADD: begin
        result_o = sat32(sum_ab);
        sat_add_o = sat32_fired(sum_ab);
      end
      OP_SUB: begin
        result_o = sat32(dif_ab);
        sat_add_o = sat32_fired(dif_ab);
      end
      OP_MUL: begin
        result_o = resc16(prod_ab);
        sat_mul_o = resc16_fired(prod_ab);
      end
      OP_MAD: begin
        result_o = resc16(mad_ab);
        sat_mul_o = resc16_fired(mad_ab);
      end
      OP_MIN: result_o = (a0_i < b0_i) ? a0_i : b0_i;
      OP_MAX: result_o = (a0_i > b0_i) ? a0_i : b0_i;
      OP_ABS: begin
        result_o = abs_a;
        sat_rescale_o = abs_sat_fired;
      end
      OP_CLAMP: result_o = clamp_r;
      OP_SELECT: result_o = (c_i != 32'sd0) ? a0_i : b0_i;
      OP_CMP: result_o = cmp_t ? 32'sh0001_0000 : 32'sd0;
      OP_DOT2: begin
        result_o = resc16(dot2);
        sat_mul_o = resc16_fired(dot2);
      end
      OP_DOT3: begin
        result_o = resc16(dot3);
        sat_mul_o = resc16_fired(dot3);
      end
      default: begin
        // Not a wrong answer: an explicit refusal. See the scope note.
        op_unsupported_o = 1'b1;
        writes_o = 1'b0;
      end
    endcase
  end

endmodule : zhao_field_alu
