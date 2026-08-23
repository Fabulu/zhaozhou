// zhao_field_mul.sv — the Field IR engine's ONE multiplier.
//
// A submodule of the FIELD.SEQ.* family. It has no reference of its own: it is
// not an operation, it is the silicon every operation borrows. What it must be
// bit-exact about is only this — the full 66-bit signed product of two 33-bit
// signed operands, with no rounding, no saturation and no truncation. Every
// rounding rule in the engine belongs to the op that owns it, and pushing any
// of them in here would make one shared block the second implementation of
// nine different laws.
//
// ---------------------------------------------------------------------------
// WHY 33 x 33 SIGNED
// ---------------------------------------------------------------------------
// The engine's operands are s32 register values, u32 hash words, u24 mantissas,
// 31-bit reciprocal seeds and a 49-bit correction term. Signed 33x33 covers
// every one of them:
//
//   * a signed 32-bit value sign-extends into 33 bits and multiplies exactly;
//   * an unsigned 32-bit value ZERO-extends into 33 bits, stays positive, and
//     multiplies exactly — which is why the lane is 33 and not 32;
//   * anything wider is decomposed by its CALLER into partial products through
//     this same lane. `zhao_field_rcp` does exactly that with its ~49-bit
//     `2^48 - p`, splitting it at bit 32 and issuing twice.
//
// The one thing a caller must never do is hand this block a value it has not
// extended, because a 33-bit port silently reinterprets a 33-bit unsigned
// magnitude as negative. Extension is the caller's job and is written at every
// issue site.
//
// ---------------------------------------------------------------------------
// THE TIMING CONTRACT, and the whole reason the walk still costs six clocks
// ---------------------------------------------------------------------------
// Input-registered and output-registered, so the DSP's own pipeline registers
// are the ones being inferred rather than logic in front of and behind a
// combinational multiplier:
//
//   cycle N     issue_i high, a_i/b_i presented   -> operands latched
//   cycle N+1   the product is formed             -> product latched
//   cycle N+2   p_valid_o is high and p_o holds it
//
// `p_o` then HOLDS until the next issue's product lands, so a consumer that is
// stalled for other reasons does not lose it. `p_valid_o` is a one-cycle pulse
// per issue: issues may be back-to-back, and then the pulses are too, which is
// what lets NORMALIZE fire its three output-lane products on consecutive
// cycles instead of paying the latency three times.
//
// TWO CYCLES IS THE NUMBER THE READ WALK WAS SHAPED AROUND. The sequencer
// issues its three operand pairs in Q_LATCH, Q_RD1 and Q_RD2, and their
// products land in Q_RD2, Q_GATH and Q_EXEC — the last one exactly in the
// state that consumes it. Making this block one-cycle would put a 33x33
// multiply, a 66-bit accumulate and a saturating rescale in one combinational
// path; making it three would cost DOT3 a seventh clock. Two is not a
// compromise between them, it is the only depth the existing cadence admits.
//
// ---------------------------------------------------------------------------
// THERE IS NO ARBITER, AND THAT IS A FACT ABOUT THE SEQUENCER
// ---------------------------------------------------------------------------
// Nine op controllers can drive this lane and none of them can collide,
// because `zhao_field_seq` retires ONE instruction at a time: an op is issued
// in Q_MISS and drained in Q_MWAIT before the next fetch. The mux in
// `zhao_field_exec_shared` therefore selects on the executing opcode and needs
// no round-robin, no grant and no backpressure.
//
// That is a load-bearing assumption rather than an observation, so it is
// tested as one: `tests/differential/field_seq_directed.cpp` runs each op
// ALONE and then in hostile sequences, and requires every answer and every
// saturation lane to equal its isolated result. A lane that had been left
// mid-issue by the previous op is exactly what that comparison catches.
// ENFORCED-BY: tests/differential/field_seq_directed.cpp
module zhao_field_mul (
    input logic clk,
    input logic rst_n,

    // The operands, already extended to 33 signed bits by the caller.
    input logic               issue_i,
    input logic signed [32:0] a_i,
    input logic signed [32:0] b_i,

    // The exact product. Held until the next one lands.
    output logic signed [65:0] p_o,
    output logic               p_valid_o
);

  logic signed [32:0] a_q, b_q;
  logic               v_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      a_q <= '0;
      b_q <= '0;
      v_q <= 1'b0;
      p_o <= '0;
      p_valid_o <= 1'b0;
    end else begin
      // The operand registers HOLD between issues. A consumer that reads p_o
      // late still reads its own product, and the multiplier array is not
      // re-driven by whatever happens to be on the mux.
      if (issue_i) begin
        a_q <= a_i;
        b_q <= b_i;
      end
      v_q <= issue_i;

      // Both operands are sized to the full product width BEFORE the multiply,
      // so the 66-bit result is the mathematical one and not a 33-bit
      // self-determined product widened afterwards. The casts preserve
      // signedness, which is what makes the sign extension the caller applied
      // mean what it says.
      p_o <= 66'(a_q) * 66'(b_q);
      p_valid_o <= v_q;
    end
  end

endmodule : zhao_field_mul
