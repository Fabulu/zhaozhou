// zhao_field_isqrt.sv — exact integer square root of a u64.
//
// A submodule of the FIELD.SEQ.* family, shared by OP_LEN2, OP_LEN3, OP_DIST2
// and (later) the NORMALIZE ops. Reference: `zref::isqrt_u64`
// (reference/include/zref/zref_trig.hpp §7.2), the restoring digit recurrence
// the interpreter calls.
//
// ---------------------------------------------------------------------------
// EXACT, NOT APPROXIMATE
// ---------------------------------------------------------------------------
// The result is the true floor of the square root: `res^2 <= n < (res+1)^2`.
// This is binary longhand — the same algorithm as long division by hand — not a
// Newton iteration and not a table. A field program's lengths must be
// bit-identical to the software's, and no approximation reproduces a floor
// exactly at every one of 2^64 inputs.
//
// ---------------------------------------------------------------------------
// THE ALIGNMENT LOOP IS AN OPTIMISATION AND IS DELIBERATELY NOT IMPLEMENTED
// ---------------------------------------------------------------------------
// The reference opens with `while (bit > num) bit >>= 2;`, skipping leading
// digit pairs. Those skipped iterations are not free of effect in the abstract —
// but with `res == 0` and `bit > num`, the test `num >= res + bit` is false, so
// the body reduces to `res >>= 1`, which leaves `res` at zero. Every skipped
// iteration is therefore a no-op, and starting unconditionally from bit 62 gives
// the identical result.
//
// So this block runs a fixed THIRTY-TWO iterations for every input. That is a
// property worth having in hardware for its own sake: the latency does not
// depend on the operand, so nothing downstream has to model a variable delay and
// no timing side-channel exists.
// ENFORCED-BY: tests/differential/field_len_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WIDTHS
// ---------------------------------------------------------------------------
// `n` is u64. `res` reaches 2^32 - 1 at most (isqrt of 2^64 - 1), so it is 33
// bits during the recurrence and 32 at the end. `res + bit` is compared against
// `num`, both u64. `bit` walks 62, 60, ... 0 — thirty-two positions.
module zhao_field_isqrt (
    input logic clk,
    input logic rst_n,

    input  logic        n_valid_i,
    output logic        n_ready_o,
    input  logic [63:0] n_i,

    output logic        r_valid_o,
    input  logic        r_ready_i,
    output logic [63:0] r_o
);

  typedef enum logic [1:0] {S_IDLE, S_RUN, S_DONE} state_e;
  state_e state;

  logic [63:0] num, res, bitp;
  logic [ 5:0] step;

  // One iteration of the recurrence, exactly as the reference writes it.
  logic [63:0] sum;
  assign sum = res + bitp;

  assign n_ready_o = (state == S_IDLE) && (!r_valid_o || r_ready_i);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      num <= '0;
      res <= '0;
      bitp <= '0;
      step <= '0;
      r_valid_o <= 1'b0;
      r_o <= '0;
    end else begin
      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;

      unique case (state)
        S_IDLE: begin
          if (n_valid_i && n_ready_o) begin
            num <= n_i;
            res <= '0;
            bitp <= 64'd1 << 62;
            step <= '0;
            state <= S_RUN;
          end
        end

        S_RUN: begin
          if (num >= sum) begin
            num <= num - sum;
            res <= (res >> 1) + bitp;
          end else begin
            res <= res >> 1;
          end
          bitp <= bitp >> 2;
          if (step == 6'd31) state <= S_DONE;
          else step <= step + 6'd1;
        end

        S_DONE: begin
          r_o <= res;
          r_valid_o <= 1'b1;
          state <= S_IDLE;
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_field_isqrt
