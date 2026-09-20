// zhao_raster_attrdiv_v2.sv -- current rast.cpp signed attribute divide.
//
// q = sat_s32(floor((num + floor(area/2)) / area)), area > 0.
// Unlike the retained unversioned candidate, a negative exact half therefore
// rounds toward positive infinity.  Saturation is a valid oracle result and is
// reported separately from the terminal zero-area error. TWO preparation clocks
// reuse the dividend bank in sequence -- raw, rounded, magnitude -- so rounding,
// the saturation judgement and magnitude formation each get their own edge
// without adding a second wide register bank. The second of them was added
// 2026-09-18 to take the saturation compare off the input edge; see D_SAT.
//
// ENFORCED-BY: tests/raster/raster_attrgrad_v2_directed.cpp
// ENFORCED-BY: tests/raster/raster_attrdiv_v2_rem_directed.cpp
//
// ---------------------------------------------------------------------------
// THE REMAINDER IS AN OUTPUT (added 2026-09-20 under owner ruling R104)
// ---------------------------------------------------------------------------
// `zhao_raster_attrdiv_svc` and `zhao_raster_attrstep` seed an exact
// quotient/remainder recurrence and could not move off the superseded v1
// divider while v2 published no remainder. R104 funded publishing it.
//
// WHAT IS PUBLISHED IS NOT `rem_r`, AND NOT v1's `rem_o`. Three distinct
// quantities live here and conflating any two of them is the whole trap:
//
//   v1's rem_o      = (2|n| + A) mod 2A          -- divisor 2A, range [0, 2A)
//   v2's rem_r      = (|M| + A - 1) mod A        -- the raw restoring residue
//   v2's rem_o      = M - q_o*A,  M = n + A/2    -- the TRUE floor remainder,
//                                                   range [0, A)
//
// v1 divides a DOUBLED dividend by a DOUBLED divisor, so its remainder is mod
// 2A and cannot be produced here at any price short of changing v2's
// arithmetic. `design/prod_manifest.yml` said so before this port existed and
// it was right: the consumers' algebra is re-derived, not transplanted.
//
// The raw residue is not published either, because the negative branch divides
// `|M| + A - 1` (the bit-vector identity in the magnitude comment below), so
// `rem_r` is off by the fixup
//
//     rem_o = neg_r ? (A - 1 - rem_r) : rem_r
//
// which is one conditional subtract on the edge that already latches the
// quotient -- no extra clock, no second divider, no DSP. With it, the pair
// this block publishes satisfies the plain Euclidean invariant
//
//     q_o * A + rem_o == n + floor(A/2),   0 <= rem_o < A
//
// for every non-saturated, non-error completion, which is exactly the seed a
// floor-quotient recurrence wants and is CONTINUOUS ACROSS ZERO -- unlike v1's
// magnitude pair, which is why v1's consumers need two sign branches and v2's
// do not.
//
// THE WIDTH IS 47 BITS AND THAT IS MEASURED, NOT ASSUMED. R104 asked for the
// top bit of the 49-bit `rem_r` to be proven clear at publication rather than
// argued from the mathematics, because the mathematics describes the converged
// value and a port publishes whatever is in the register. A restoring divider
// leaves `rem_r < den_r <= 2^47 - 1`, so bits 48 AND 47 are both clear -- and
// `rem_range_err_o` is the standing instrument that says so out loud instead of
// the comment being the only evidence. It is UNREACHABLE by legal stimulus
// while the restoring compare is correct, so its positive control is the
// committed mutant `tests/mutants/zhao_raster_attrdiv_v2_remwidth_mutant.sv`.
//
// ON SATURATION AND ERROR the pair is meaningless and `rem_o` reads zero, the
// same convention v1 used for `bad_r`. A saturated quotient is clamped, so
// `q_o*A + rem_o == M` cannot hold; consumers must read `q_saturated_o` and
// `q_error_o` before using the pair.
`default_nettype none

// Test-only selector hooks are defined by the committed mutant source compiled
// immediately before this file.  Production gets the identity law below.
`ifndef ZHAO_ATTR_V2_ROUND_NUM
`define ZHAO_ATTR_V2_ROUND_NUM(num, area_ext) \
    ($signed(num) + ($signed(area_ext) >>> 1))
`endif

// The restoring compare, as a selectable leaf for the SAME reason the rounding
// law is one: `rem_range_err_o` watches for a residue that outgrew 47 bits, and
// that state is UNREACHABLE while this compare is correct. No legal stimulus
// can move the counter, so "it can fire" would stay an argument forever. The
// mutant turns `>=` into `>`, which lets a residue equal to the divisor survive
// a step and grow past 2^47 on a large area.
//
// This is a seam, not a knob: production gets the identity law below, and
// nothing but tests/mutants/zhao_raster_attrdiv_v2_remwidth_mutant.sv ever
// defines it otherwise. A COPY of this divider was deliberately not taken --
// see tests/mutants/zhao_raster_attr_v2_mutants.sv's header for why, and
// CLAUDE.md for the three lanes that paid for a copy going stale.
`ifndef ZHAO_ATTR_V2_REM_GE
`define ZHAO_ATTR_V2_REM_GE(rem, den) ((rem) >= (den))
`endif

module zhao_raster_attrdiv_v2 #(
    parameter int unsigned RADIX = 2
) (
    input  var logic               clk,
    input  var logic               rst_n,

    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [95:0] num_i,
    input  var logic        [46:0] area_i,

    output var logic               r_valid_o,
    input  var logic               r_ready_i,
    output var logic signed [31:0] q_o,
    output var logic               q_saturated_o,
    output var logic               q_error_o,
    // The TRUE floor remainder of this divide: q_o*area_i + rem_o == num_i +
    // floor(area_i/2), with 0 <= rem_o < area_i. Zero and meaningless when
    // q_saturated_o or q_error_o is set. See THE REMAINDER IS AN OUTPUT above.
    output var logic [46:0]        rem_o,

    output var logic [31:0]        divides_o,
    output var logic [31:0]        saturations_o,
    output var logic [31:0]        errors_o,
    output var logic [31:0]        busy_clocks_o,
    // Completions whose raw restoring residue did not fit 47 bits. Unreachable
    // while the restoring compare below is correct; it exists so the width
    // claim on rem_o is a live instrument rather than a sentence in a header.
    // Positive control: tests/mutants/zhao_raster_attrdiv_v2_remwidth_mutant.sv
    output var logic [31:0]        rem_range_err_o
);

  localparam int unsigned RBITS = (RADIX == 4) ? 2 : 1;
  // A leading zero makes the walk an even number of bits at radix four.
  localparam int unsigned QPOS  = 98;
  localparam int unsigned STEPS = QPOS / RBITS;

  // D_SAT SPLITS THE SATURATION TEST OFF THE INPUT EDGE, which widened the
  // state register from two bits to three.
  //
  // `sat_pos_c`/`sat_neg_c` used to be evaluated combinationally from `num_i`
  // in D_IDLE: one 97-bit round-and-add followed by TWO 97-bit signed
  // comparisons, all on the same edge that captured the value. And `num_i` is
  // `zhao_raster_attrgrad_v2`'s freshly-summed `row_num_c`, so the adder tree
  // upstream shares that edge too.
  //
  // The 2026-09-18 composed fit measured the pair as one path: 15.67 ns against
  // a 10.000 ns period, of which THIS module held 8.68 -- `Add0` at 3.21, the
  // `LessThan0` chain at 3.87 and `final_sat_r` at 1.60. That is why the
  // divider's share grew when the tree shrank: they were never two paths.
  //
  // Now D_IDLE captures the raw numerator, D_PREP rounds it and judges the
  // REGISTERED value, and D_SAT acts on the verdict. Every operand of the
  // comparison starts at a flip-flop.
  localparam logic [2:0] D_IDLE = 3'd0;
  localparam logic [2:0] D_RUN  = 3'd1;
  localparam logic [2:0] D_DONE = 3'd2;
  localparam logic [2:0] D_PREP = 3'd3;
  localparam logic [2:0] D_SAT  = 3'd4;

  logic [2:0] st_r;
  logic [6:0] iter_r;
  logic       neg_r;

  logic [97:0] dividend_r;
  logic [97:0] qmag_r;
  logic [48:0] rem_r;
  logic [46:0] den_r;

  logic signed [31:0] final_q_r;
  logic               final_sat_r;
  logic               final_err_r;
  // The floor remainder, already fixed up for the negative branch. Loaded on
  // the same edge that loads final_q_r, so publishing it costs no clock -- the
  // exact-latency gate in raster_attrgrad_v2_directed.cpp would catch it if it
  // did.
  logic        [46:0] final_rem_r;

  logic signed [96:0] area_ext_c;
  logic signed [96:0] rounded_num_r_c;
  logic signed [96:0] pos_sat_limit_c;
  logic signed [96:0] neg_sat_limit_c;
  logic               sat_pos_r_c, sat_neg_r_c;
  logic        [97:0] magnitude_c;

  // EVALUATED IN D_PREP FROM REGISTERS, not in D_IDLE from the inputs.
  //
  // The arithmetic is unchanged, expression for expression -- `area_ext_c` is
  // the same sign-extension of the same 47-bit area, and the exact-law leaf is
  // still `ZHAO_ATTR_V2_ROUND_NUM` with the same two operands. Only the SOURCE
  // of those operands moved: `den_r` instead of `area_i`, and the raw numerator
  // held in `dividend_r` instead of `num_i`. Both were captured on the previous
  // edge, so the cone now starts at flip-flops.
  //
  // The committed negative-half mutant still selects exactly this expression
  // and nothing else, which is why the split is a relocation rather than a
  // rewrite of the law.
  always_comb begin
    area_ext_c      = $signed({50'd0, den_r});
    rounded_num_r_c = `ZHAO_ATTR_V2_ROUND_NUM($signed(dividend_r[96:0]),
                                              area_ext_c);
    pos_sat_limit_c = area_ext_c <<< 31;    // (INT32_MAX + 1) * area
    neg_sat_limit_c = -(area_ext_c <<< 31); // INT32_MIN * area
    sat_pos_r_c = (den_r != 47'd0) && (rounded_num_r_c >= pos_sat_limit_c);
    sat_neg_r_c = (den_r != 47'd0) && (rounded_num_r_c <  neg_sat_limit_c);
  end

  // The fitted G8A path formerly combined input rounding with this 98-bit
  // magnitude/bias formation on the dividend register's input. D_PREP now
  // captures the signed rounded value in dividend_r; D_SAT reuses that same
  // bank to form the unsigned long-division dividend one clock later. (Those
  // were D_IDLE and D_PREP until 2026-09-18; the states shifted by one when the
  // saturation compare moved off the input edge, and the bank reuse below is
  // unchanged.)
  // ONE 98-BIT CARRY CHAIN, NOT THREE. The negative branch used to materialize
  // a negate, then add the denominator, then subtract one -- three dependent
  // 98-bit additions in the D_PREP cone, which the Timing3 census reports as
  // the ATTR3/divider family (dividend_r[0] -> dividend_r[96]).
  //
  // The saving is an exact bit-vector identity, not an approximation. In two's
  // complement -x is (~x)+1, so modulo 2^98:
  //
  //     (-x + d - 1)  ==  ((~x) + 1 + d - 1)  ==  ((~x) + d)
  //
  // The explicit -1 is precisely what cancels the negate's carry-in, so the
  // complement can be added to the denominator directly. This holds for every
  // 98-bit x including the most negative value, where an explicit negate would
  // have wrapped anyway; the truncating modulo arithmetic is unchanged.
  //
  // Same D_PREP edge, same register bank, same state sequence, no extra clock
  // and no DSP. Do not "simplify" this back into a materialized negate.
  always_comb begin
    if (neg_r)
      magnitude_c = (~dividend_r) + 98'({51'd0, den_r});
    else
      magnitude_c = dividend_r;
  end

  assign v_ready_o = (st_r == D_IDLE) && !r_valid_o;

  logic [48:0] rem_shift_c;
  logic [48:0] d1_c, d2_c, d3_c;
  logic [1:0]  digit_c;
  logic [48:0] rem_next_c;
  logic [97:0] dividend_next_c;
  logic [97:0] qmag_next_c;

  always_comb begin
    d1_c = {2'd0, den_r};
    d2_c = d1_c << 1;
    d3_c = d1_c + d2_c;
    digit_c = 2'd0;

    if (RADIX == 4) begin
      rem_shift_c = (rem_r << 2) | 49'({47'd0, dividend_r[97:96]});
      if (rem_shift_c >= d3_c) begin
        digit_c    = 2'd3;
        rem_next_c = rem_shift_c - d3_c;
      end else if (rem_shift_c >= d2_c) begin
        digit_c    = 2'd2;
        rem_next_c = rem_shift_c - d2_c;
      end else if (rem_shift_c >= d1_c) begin
        digit_c    = 2'd1;
        rem_next_c = rem_shift_c - d1_c;
      end else begin
        rem_next_c = rem_shift_c;
      end
    end else begin
      rem_shift_c = (rem_r << 1) | 49'({48'd0, dividend_r[97]});
      if (`ZHAO_ATTR_V2_REM_GE(rem_shift_c, d1_c)) begin
        digit_c    = 2'd1;
        rem_next_c = rem_shift_c - d1_c;
      end else begin
        rem_next_c = rem_shift_c;
      end
    end

    dividend_next_c = dividend_r << RBITS;
    qmag_next_c = (qmag_r << RBITS) | 98'(digit_c);
  end

  initial begin
    if ((RADIX != 2) && (RADIX != 4))
      $fatal(1, "zhao_raster_attrdiv_v2 RADIX must be 2 or 4");
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r           <= D_IDLE;
      iter_r         <= 7'd0;
      neg_r          <= 1'b0;
      dividend_r     <= 98'd0;
      qmag_r         <= 98'd0;
      rem_r          <= 49'd0;
      den_r          <= 47'd0;
      final_q_r      <= 32'sd0;
      final_sat_r    <= 1'b0;
      final_err_r    <= 1'b0;
      final_rem_r    <= 47'd0;
      r_valid_o      <= 1'b0;
      q_o            <= 32'sd0;
      q_saturated_o  <= 1'b0;
      q_error_o      <= 1'b0;
      rem_o          <= 47'd0;
      divides_o      <= 32'd0;
      saturations_o  <= 32'd0;
      errors_o       <= 32'd0;
      busy_clocks_o  <= 32'd0;
      rem_range_err_o <= 32'd0;
    end else begin
      if (st_r != D_IDLE) busy_clocks_o <= busy_clocks_o + 32'd1;

      case (st_r)
        D_IDLE: begin
          if (v_valid_i && v_ready_o) begin
            // Capture the RAW numerator. dividend_r is still the one
            // preparation bank -- it now holds three things in sequence rather
            // than two: the raw value here, the rounded value in D_PREP, the
            // unsigned magnitude in D_SAT. Still no second 97-bit bank.
            // 96-bit input into a 98-bit bank: TWO sign bits, not one. The
            // old form extended an already-97-bit rounded value by one, and
            // reusing its shape here silently lost a bit -- caught by
            // WIDTHEXPAND rather than by a test, which is the cheaper place.
            dividend_r   <= {{2{num_i[95]}}, num_i};
            final_err_r   <= (area_i == 47'd0);
            den_r         <= area_i;
            rem_r         <= 49'd0;
            qmag_r        <= 98'd0;
            st_r           <= D_PREP;
          end
        end

        D_PREP: begin
          // Round and judge, both from registers. `rounded_den_c` and the two
          // saturation limits derive from `den_r`, not `area_i`, so nothing in
          // this cone reaches back to a module input.
          dividend_r  <= {rounded_num_r_c[96], rounded_num_r_c};
          neg_r       <= rounded_num_r_c[96];
          final_sat_r <= sat_pos_r_c || sat_neg_r_c;
          st_r        <= D_SAT;
        end

        D_SAT: begin
          // A saturated or refused divide has no meaningful pair: the quotient
          // is clamped, so q*A + rem cannot equal M. Publish zero, as v1 did
          // for `bad_r`. The D_RUN branch below overwrites this with the real
          // remainder on a different edge, so there is no conflict.
          final_rem_r <= 47'd0;
          if (final_err_r) begin
            final_q_r <= 32'sd0;
            st_r      <= D_DONE;
          end else if (final_sat_r && !neg_r) begin
            final_q_r <= 32'sh7fff_ffff;
            st_r      <= D_DONE;
          end else if (final_sat_r) begin
            final_q_r <= -32'sh8000_0000;
            st_r      <= D_DONE;
          end else begin
            // A negative floor is -ceil(abs/area), hence area-1.
            dividend_r <= magnitude_c;
            iter_r     <= 7'(STEPS);
            st_r       <= D_RUN;
          end
        end

        D_RUN: begin
          dividend_r <= dividend_next_c;
          qmag_r     <= qmag_next_c;
          rem_r      <= rem_next_c;
          if (iter_r == 7'd1) begin
            final_q_r <= neg_r
                ? 32'(-$signed({1'b0, qmag_next_c[31:0]}))
                : 32'($signed({1'b0, qmag_next_c[31:0]}));
            final_sat_r <= 1'b0;
            final_err_r <= 1'b0;
            // THE FIXUP. The negative branch divided |M| + A - 1, so its
            // residue is (|M| - 1) mod A and the true floor remainder is
            // A - 1 - that. The positive branch divided M itself, so its
            // residue already IS the floor remainder. One conditional subtract
            // on the edge that already latches the quotient.
            final_rem_r <= neg_r ? (den_r - 47'd1 - rem_next_c[46:0])
                                 : rem_next_c[46:0];
            // THE WIDTH CLAIM, AS AN INSTRUMENT. A correct restoring compare
            // leaves rem_next_c < den_r <= 2^47-1, so these two bits are dead.
            // If they are ever live the truncation above is silently wrong, and
            // this counter is the only thing that would say so.
            if (|rem_next_c[48:47]) rem_range_err_o <= rem_range_err_o + 32'd1;
            st_r        <= D_DONE;
          end else begin
            iter_r <= iter_r - 7'd1;
          end
        end

        D_DONE: begin
          q_o           <= final_q_r;
          q_saturated_o <= final_sat_r;
          q_error_o     <= final_err_r;
          rem_o         <= final_rem_r;
          r_valid_o     <= 1'b1;
          divides_o     <= divides_o + 32'd1;
          if (final_sat_r) saturations_o <= saturations_o + 32'd1;
          if (final_err_r) errors_o <= errors_o + 32'd1;
          st_r <= D_IDLE;
        end

        default: st_r <= D_IDLE;
      endcase

      if (r_valid_o && r_ready_i) r_valid_o <= 1'b0;
    end
  end

endmodule : zhao_raster_attrdiv_v2

`undef ZHAO_ATTR_V2_ROUND_NUM
`undef ZHAO_ATTR_V2_REM_GE
`default_nettype wire
