// zhao_field_noise.sv — the Field IR lattice-noise ops: OP_NOISE2 and OP_RIDGE.
//
// A submodule of the FIELD.SEQ.* family. Reference: `zref::noise2_hash`
// (reference/include/zref/zref_fixp.hpp §7.5) and the interpreter's OP_NOISE2
// and OP_RIDGE cases.
//
// ---------------------------------------------------------------------------
// THE LAW
// ---------------------------------------------------------------------------
//     ix = (u32)(a0 >>> 16)              ARITHMETIC shift of a SIGNED value
//     iy = (u32)(a1 >>> 16)              (NOISE2), or (b >>> 16) for RIDGE
//
//     noise2_hash(x, y, seed, lane):
//       s = (x * 0x9E3779B1) ^ ((y * 0x85EBCA77) ^ seed)   lattice mix
//       s = s + lane * 0xE1                                lane salt
//       s = s * 747796405 + 2891336453                     LCG advance
//       w = ((s >> ((s >> 28) + 4)) ^ s) * 277803737       RXS-M-XS
//       return (w >> 22) ^ w
//
//     NOISE2 : dst   = hash(ix, iy, imm, 0) >> 16
//              dst+1 = hash(ix, iy, imm, 1) >> 16
//     RIDGE  : u = hash(ix, iy, imm, 0) >> 16
//              t = fx_sub(fx_add(u, u), 1.0)
//              dst = fx_sub(1.0, abs_sat(t))
//
// Six things are load-bearing:
//
// 1. **THE LATTICE INDEX IS AN ARITHMETIC SHIFT.** The register is signed and
//    `>>` in the reference is therefore sign-extending; the result is then
//    REINTERPRETED as unsigned. A logical shift agrees for every non-negative
//    coordinate and disagrees for every negative one — so a terrain that looks
//    perfect on one side of the origin is a different world on the other.
//
// 2. **THE RXS SHIFT AMOUNT IS DATA-DEPENDENT**, `(s >> 28) + 4`, i.e. 4..19.
//    That is a real variable shifter and not a constant one; it is what makes
//    this a PCG rather than a plain LCG, and hardwiring it destroys the
//    statistical property the whole op exists for while still producing
//    plausible-looking noise.
//
// 3. **EVERY MULTIPLY IS MODULO 2^32.** The constants are frozen verbatim and
//    the arithmetic wraps; nothing here saturates. A saturating multiply would
//    be wrong in a way that only shows up as a subtly different landscape.
//
// 4. **NOISE2 AND RIDGE DISAGREE ABOUT WHERE THEIR SECOND COORDINATE LIVES.**
//    NOISE2 reads an ADJACENT PAIR — `reg[a]` and `reg[a+1]`. RIDGE reads TWO
//    NAMED REGISTERS — `reg[a]` and `reg[b]`. Same hash, same seed, different
//    operand convention, and nothing in the encoding hints at it.
//
// 5. **THE OUTPUT IS THE TOP HALF.** `>> 16` puts the hash's high 16 bits in
//    the low half of the result, so the value is [0, 1) in Q16.16 and never
//    negative. Taking the low half instead is the same distribution and a
//    different number for every input.
//
// 6. **RIDGE'S FOLD SATURATES, AND `abs` HAS A RAIL.** `abs_sat(INT32_MIN)` is
//    INT32_MAX, not INT32_MIN — but `t` here is derived from a [0,1) value, so
//    that rail is unreachable and the saturating forms are exact. That is worth
//    stating rather than discovering: the reference uses saturating primitives
//    and this block matches them, so the two agree even if a future caller
//    widens the input.
//
// ---------------------------------------------------------------------------
// THE FINAL XOR-SHIFT CANNOT CHANGE THIS OP'S ANSWER, AND IS KEPT ANYWAY
// ---------------------------------------------------------------------------
// `noise2_hash` ends with `(w >> 22) ^ w`. Both ops then keep only bits [31:16].
// `w >> 22` has nothing above bit 9, so the xor perturbs bits [9:0] and the op
// throws away bits [15:0] -- the output is bit-for-bit `w[31:16]` whatever that
// last line does.
//
// It stays because the reference is the law and this block is its differential,
// not its optimiser: an implementation that agrees on every observable output
// while quietly computing something else is the thing this whole method exists
// to prevent, and the day someone widens the op to keep more bits, a version
// that had "simplified" it would be silently wrong.
//
// The mutation sweep proves the point rather than assuming it: dropping the
// xor-shift and changing its shift amount BOTH survive, and both are recorded
// as equivalent mutants with this reason attached.
//
// ---------------------------------------------------------------------------
// ONE MULTIPLIER, WALKED
// ---------------------------------------------------------------------------
// A NOISE2 needs six 32x32 products (two for the shared lattice mix, then two
// per lane) and a RIDGE needs four. They are taken ONE AT A TIME through a
// single multiplier, the way `zhao_geom_mat3x4_mul` walks a matrix: this is a
// field-program op evaluated per sample, not a per-pixel path, and six DSPs
// spent to save five cycles is a bad trade in a design that has already failed
// a fit once.
module zhao_field_noise (
    input logic clk,
    input logic rst_n,

    input  logic               v_valid_i,
    output logic               v_ready_o,
    input  logic               is_ridge_i,  // 0 = NOISE2, 1 = RIDGE
    input  logic signed [31:0] a0_i,        // x coordinate
    input  logic signed [31:0] a1_i,        // y: reg[a+1] (NOISE2) or reg[b] (RIDGE)
    input  logic        [31:0] seed_i,      // the instruction's imm

    output logic               r_valid_o,
    input  logic               r_ready_i,
    output logic signed [31:0] o0_o,        // NOISE2 lane 0, or RIDGE's result
    output logic signed [31:0] o1_o,        // NOISE2 lane 1 (zero for RIDGE)
    output logic               sat_add_o,
    output logic               sat_rescale_o
);

  localparam logic [31:0] C_X = 32'h9E37_79B1;
  localparam logic [31:0] C_Y = 32'h85EB_CA77;
  localparam logic [31:0] C_LCG_M = 32'd747796405;
  localparam logic [31:0] C_LCG_A = 32'd2891336453;
  localparam logic [31:0] C_XSM = 32'd277803737;

  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_MIX_X = 3'd1;
  localparam logic [2:0] S_MIX_Y = 3'd2;
  localparam logic [2:0] S_LCG = 3'd3;
  localparam logic [2:0] S_RXS = 3'd4;
  localparam logic [2:0] S_LANE = 3'd5;
  localparam logic [2:0] S_OUT = 3'd6;

  logic [2:0] state;

  logic signed [31:0] h_a0, h_a1;
  logic        [31:0] h_seed;
  logic               h_ridge;
  logic               lane;        // which hash lane is being walked
  logic        [31:0] mix_x;       // x * C_X
  logic        [31:0] s_mix;       // the finished lattice mix, SHARED by both lanes
  logic        [31:0] s_reg;       // the running hash word
  // Lane 0's finished hash. Only the TOP half is ever read (law 5) -- the low
  // sixteen bits are the part the reference discards, and they are kept whole
  // here rather than truncated at capture so the two lanes stay symmetrical.
  /* verilator lint_off UNUSEDSIGNAL */
  logic        [31:0] lane0;
  /* verilator lint_on UNUSEDSIGNAL */

  // Law 1: ARITHMETIC shift, then reinterpreted as unsigned.
  logic [31:0] ix, iy;
  assign ix = $unsigned(h_a0 >>> 16);
  assign iy = $unsigned(h_a1 >>> 16);

  // ---- the one multiplier, law 3: modulo 2^32, never saturating -----------
  logic [31:0] mul_a, mul_b;
  logic [31:0] mul_p;
  assign mul_p = mul_a * mul_b;

  // Law 2: the shift amount is a function of the DATA, 4..19.
  // Lane 1's finished hash, named so the two lanes read the same way. As with
  // lane 0, only the top half is read: `>> 16` is the law (law 5), and the
  // discarded half is kept whole rather than truncated early.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] lane1_h;
  /* verilator lint_on UNUSEDSIGNAL */
  assign lane1_h = (s_reg >> 22) ^ s_reg;

  logic [ 4:0] rxs_sh;
  logic [31:0] rxs_in;
  assign rxs_sh = 5'({s_reg[31:28]} + 4'd4);
  assign rxs_in = (s_reg >> rxs_sh) ^ s_reg;

  always_comb begin
    mul_a = 32'd0;
    mul_b = 32'd0;
    case (state)
      S_MIX_X: begin
        mul_a = ix;
        mul_b = C_X;
      end
      S_MIX_Y: begin
        mul_a = iy;
        mul_b = C_Y;
      end
      S_LCG: begin
        // The lane salt is added BEFORE the advance, not mixed into the
        // lattice: lane 1 is the same lattice point taken one salt further.
        mul_a = s_reg + (lane ? 32'h0000_00E1 : 32'd0);
        mul_b = C_LCG_M;
      end
      default: begin
        mul_a = rxs_in;
        mul_b = C_XSM;
      end
    endcase
  end

  // ---- RIDGE's fold, on the finished lane-0 hash --------------------------
  // Law 5: the TOP half. u is [0, 1) and never negative.
  logic signed [31:0] u_val;
  assign u_val = $signed({16'd0, lane0[31:16]});

  function automatic logic signed [31:0] add_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) + $signed({b[31], b});
      if (t > 33'sd2147483647) add_sat = 32'sh7FFF_FFFF;
      else if (t < -33'sd2147483648) add_sat = 32'sh8000_0000;
      else add_sat = t[31:0];
    end
  endfunction

  function automatic logic signed [31:0] sub_sat(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) - $signed({b[31], b});
      if (t > 33'sd2147483647) sub_sat = 32'sh7FFF_FFFF;
      else if (t < -33'sd2147483648) sub_sat = 32'sh8000_0000;
      else sub_sat = t[31:0];
    end
  endfunction

  function automatic logic add_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) + $signed({b[31], b});
      add_fired = (t > 33'sd2147483647) || (t < -33'sd2147483648);
    end
  endfunction

  function automatic logic sub_fired(input logic signed [31:0] a, input logic signed [31:0] b);
    logic signed [32:0] t;
    begin
      t = $signed({a[31], a}) - $signed({b[31], b});
      sub_fired = (t > 33'sd2147483647) || (t < -33'sd2147483648);
    end
  endfunction

  // Law 6: abs with the INT32_MIN rail, matching `abs_sat`.
  function automatic logic signed [31:0] abs_sat(input logic signed [31:0] a);
    begin
      if (a == 32'sh8000_0000) abs_sat = 32'sh7FFF_FFFF;
      else abs_sat = (a < 0) ? -a : a;
    end
  endfunction

  logic signed [31:0] ridge_t, ridge_r;
  assign ridge_t = sub_sat(add_sat(u_val, u_val), 32'sh0001_0000);
  assign ridge_r = sub_sat(32'sh0001_0000, abs_sat(ridge_t));

  assign v_ready_o = (state == S_IDLE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      h_a0 <= '0;
      h_a1 <= '0;
      h_seed <= '0;
      h_ridge <= 1'b0;
      lane <= 1'b0;
      mix_x <= 32'd0;
      s_mix <= 32'd0;
      s_reg <= 32'd0;
      lane0 <= 32'd0;
      r_valid_o <= 1'b0;
      o0_o <= '0;
      o1_o <= '0;
      sat_add_o <= 1'b0;
      sat_rescale_o <= 1'b0;
    end else begin
      case (state)
        S_IDLE: begin
          if (v_valid_i) begin
            h_a0 <= a0_i;
            h_a1 <= a1_i;
            h_seed <= seed_i;
            h_ridge <= is_ridge_i;
            lane <= 1'b0;
            sat_add_o <= 1'b0;
            sat_rescale_o <= 1'b0;
            state <= S_MIX_X;
          end
        end

        S_MIX_X: begin
          mix_x <= mul_p;
          state <= S_MIX_Y;
        end

        S_MIX_Y: begin
          // s = (x*Cx) ^ ((y*Cy) ^ seed). The seed is folded into the Y term,
          // which is not the same as folding it into the whole word -- xor is
          // associative, so it is, and the grouping is kept for readability.
          s_mix <= mix_x ^ (mul_p ^ h_seed);
          s_reg <= mix_x ^ (mul_p ^ h_seed);
          state <= S_LCG;
        end

        S_LCG: begin
          s_reg <= mul_p + C_LCG_A;
          state <= S_RXS;
        end

        S_RXS: begin
          s_reg <= mul_p;   // w, before the final xor-shift
          state <= S_LANE;
        end

        S_LANE: begin
          // (w >> 22) ^ w finishes this lane.
          if (h_ridge) begin
            lane0 <= (s_reg >> 22) ^ s_reg;
            state <= S_OUT;
          end else if (lane == 1'b0) begin
            lane0 <= (s_reg >> 22) ^ s_reg;
            lane  <= 1'b1;
            // The lattice mix is SHARED between the lanes: only the salt and
            // everything after it is walked again. It is REPLAYED from a
            // register rather than recomputed -- recomputing it here reads as
            // harmless and instantiates a second 32x32 multiplier, which is the
            // whole cost this block was shaped to avoid.
            s_reg <= s_mix;
            state <= S_LCG;
          end else begin
            o0_o <= u_val;
            o1_o <= $signed({16'd0, lane1_h[31:16]});
            r_valid_o <= 1'b1;
            state <= S_OUT;
          end
        end

        S_OUT: begin
          if (!r_valid_o) begin
            // RIDGE lands here with lane0 holding its finished hash.
            o0_o <= ridge_r;
            o1_o <= 32'sd0;
            sat_add_o <= add_fired(u_val, u_val) ||
                         sub_fired(add_sat(u_val, u_val), 32'sh0001_0000) ||
                         sub_fired(32'sh0001_0000, abs_sat(ridge_t));
            sat_rescale_o <= (ridge_t == 32'sh8000_0000);
            r_valid_o <= 1'b1;
          end else if (r_ready_i) begin
            r_valid_o <= 1'b0;
            state <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_field_noise
