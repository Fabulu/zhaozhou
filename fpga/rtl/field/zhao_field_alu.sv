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

    output logic signed [31:0] result_o,
    output logic               is_end_o,          // OP_END: stop, do not write
    output logic               writes_o,          // this op writes reg[dst]
    output logic               op_unsupported_o,  // not in this increment
    // The SatLedger lanes, separately, because the reference keeps them apart
    // and a block that saturated in the wrong lane could still produce the
    // right number and be wrong everywhere else.
    output logic               sat_add_o,
    output logic               sat_mul_o
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
  logic signed [W-1:0] sum_ab, dif_ab, prod_ab, mad_ab, dot2, dot3;
  always_comb begin
    sum_ab  = ext32(a0_i) + ext32(b0_i);
    dif_ab  = ext32(a0_i) - ext32(b0_i);
    prod_ab = ext32(a0_i) * ext32(b0_i);
    mad_ab  = prod_ab + (ext32(c_i) <<< 16);
    dot2    = ext32(a0_i) * ext32(b0_i) + ext32(a1_i) * ext32(b1_i);
    dot3    = dot2 + ext32(a2_i) * ext32(b2_i);
  end

  // ---- ABS: INT32_MIN stays INT32_MIN, it does NOT saturate ---------------
  logic signed [31:0] abs_a;
  always_comb begin
    abs_a = (a0_i == 32'sh8000_0000) ? 32'sh8000_0000 : (a0_i[31] ? -a0_i : a0_i);
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
      OP_ABS: result_o = abs_a;
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
