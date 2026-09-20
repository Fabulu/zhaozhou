// zhao_geom_projradius.sv -- GEOM.LOD's `proj_radius_q8_i`, which is
// `zref::creature::projected_bound_radius_q8` and nothing else.
// Owner ruling R68 sub-build 4.
//
// ENFORCED-BY: tests/geometry/geom_projradius_directed.cpp:main
// REFERENCE:   zref::creature::projected_bound_radius_q8
//              (reference/src/zcreature/creature_sim.cpp)
//
// ---------------------------------------------------------------------------
// THE LAW, VERBATIM
// ---------------------------------------------------------------------------
//     if (clip.w.raw <= 0) return false;                       // behind the eye
//     kx  = max(|vp.m[0][0]|, |vp.m[0][1]|, |vp.m[0][2]|);
//     rnum = kx * bound_radius * viewport_w * 128;             // __int128
//     rden = clip.w.raw << 16;
//     radius_q8 = (rnum + rden / 2) / rden;
//
// That last line is `spec/qformats.md` section 3's round-half-up, since
// rnum >= 0 and rden > 0. ONE rounding, at the end, and this block performs
// exactly one.
//
// ---------------------------------------------------------------------------
// WHY IT IS *NOT* `zhao_part_project`'s HALF-EXTENT, WHICH IS THE NEAR-MISS
// ---------------------------------------------------------------------------
// `zhao_console_core.sv`'s FORGE.SHADOW entry says the arithmetic "is SETTLED
// and already CALLED in this console: `zhao_part_project` transcribes
// `zref::render::draw_form_marker`'s world branch bit for bit -- half =
// |rescale_s32(fx_mul(radius_fx16, d), 8)| with d = 1/w".
//
// THAT IS A DIFFERENT NUMBER, and adopting it would have been the whole defect.
// Worked out: `fx_mul(R, d)` with d = 1/w in Q16.16 gives fx16 of R/w, and
// rescaling by 8 gives S12.8 of R/w -- the half-extent in units of NDC. The
// creature law above is S12.8 of `kx * R * viewport_w / (2 * w)`, i.e. the same
// ratio multiplied by `kx * viewport_w / 2`, which converts NDC to PIXELS.
// The two agree only when `kx * viewport_w == 2`, which no camera satisfies.
//
// A marker's half-extent and a creature's projected bound radius are both
// "the size of that thing on screen" in English and are not the same quantity,
// and `zref::creature::lod_raw` is calibrated against the second. Feeding it
// the first would put every creature several rungs coarse and nothing
// downstream could tell -- the ladder would still answer, still be stable,
// still pass a hysteresis test. So this block implements the creature law and
// differentials against the creature oracle.
//
// ---------------------------------------------------------------------------
// ZERO DSP, ZERO M10K, AND WHY THE RATE PERMITS IT
// ---------------------------------------------------------------------------
// The widest product is kx * R * vw at up to 2^74, and the divisor is up to
// 2^40. Nothing on this machine has a 76-bit multiplier or a 76/40 divider, and
// building one in parallel would be absurd for the rate: this is evaluated ONCE
// PER CREATURE INSTANCE PER VIEW PER FRAME. At `zhao_geom_drawjob`'s content
// tier of 256 instance transforms across two views that is 512 evaluations in
// 1,666,666 clocks -- 3,255 clocks available for each, and 121 are used.
//
// So every product is a SHIFT-ADD and the quotient is a restoring divide:
//
//   phase M1  32 steps   acc = kx * bound_radius            (<= 2^62)
//   phase M2  12 steps   acc = acc * viewport_w             (<= 2^74)
//   phase BIAS 1 step    acc = acc + (w << 8)               -- the rounding term
//   phase DIV 76 steps   quo = acc / (w << 9)
//
// THE EXACT REDUCTION THAT MAKES 76 BITS ENOUGH, because it is the one step in
// here a reader should check rather than trust. The literal law needs
// (rnum + rden/2) / rden with rnum up to 2^81. But 2^7 divides rnum
// (the `* 128`), rden/2 = w << 15 and rden = w << 16, so it divides all three
// exactly -- and dividing numerator and denominator of a floor by a common
// factor leaves the floor unchanged, because the RATIONAL is unchanged. Hence
//
//     radius_q8 = floor( (kx * R * vw + (w << 8)) / (w << 9) )
//
// with a 75-bit numerator instead of an 82-bit one. This is a rewriting of the
// law, not an approximation of it, and the directed test differentials against
// the reference rather than against this identity.
//
// ---------------------------------------------------------------------------
// WHAT IT REFUSES
// ---------------------------------------------------------------------------
// BEHIND THE EYE is not an answer of zero, it is the absence of an answer --
// `zref` returns false and `creature_sim` then `continue`s past the whole
// creature. `ans_ok_o` carries that, and a caller must not tick the ladder on
// it: a zero projected radius means "too small to see", which is the COARSEST
// rung, and a creature behind the camera would be quietly demoted and then
// held there by the hysteresis for fifteen ticks after it came back into view.
//
// A NON-POSITIVE BOUND RADIUS is also refused and counted. The ladder bank
// guarantees positive stored values, but a bank MISS answers zero, and a
// caller that ticked on a miss must be caught here rather than divided by.
//
// A QUOTIENT PAST 31 BITS SATURATES and is counted. The reference narrows a
// __int128 to int32_t, which is implementation-defined; saturating is the
// defined behaviour and the counter says when the two could differ. It needs
// a creature whose bound projects to more than 8.4 million subpixels -- 32,768
// screen pixels -- so it is unreachable with any legal viewport and carries a
// COMMITTED MUTANT rather than a stimulus case.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_geom_projradius #(
    // The rider carried in lockstep with the request and handed back with its
    // answer. The caller owns its meaning; this block never reads a bit.
    parameter int unsigned TAGW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one evaluation in -------------------------------------------------
    input  var logic               req_valid_i,
    output var logic               req_ready_o,
    // The view's row-0 magnitude bound and viewport width, from
    // `zhao_view_projscale`.
    input  var logic        [31:0] kx_i,
    input  var logic        [11:0] vw_i,
    // The creature type's bind-pose bound radius, fx16, from
    // `zhao_geom_ladderbank`.
    input  var logic signed [31:0] bound_radius_i,
    // The shared projector's clip `w` for the INSTANCE CENTRE, and its own
    // behind-the-eye verdict for the same point. Both come back from client A
    // together, so they cannot describe different vertices.
    input  var logic        [30:0] w_i,
    input  var logic               behind_i,
    input  var logic [TAGW-1:0]    tag_i,

    // ---- the answer out ----------------------------------------------------
    output var logic               ans_valid_o,
    input  var logic               ans_ready_i,
    output var logic signed [31:0] radius_q8_o,
    // LOW means there is no radius -- behind the eye, or a non-positive bound
    // radius. It is NOT a zero radius; see the header.
    output var logic               ans_ok_o,
    output var logic [TAGW-1:0]    tag_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] evaluations_o,  // answers with ans_ok high
    output var logic [31:0] behind_o,       // refused: behind the eye or w == 0
    output var logic [31:0] bad_bound_o,    // refused: bound_radius <= 0
    output var logic [31:0] saturated_o     // the quotient did not fit 31 bits
);

  // The numerator kx*R*vw is under 2^74 and the bias adds under 2^39, so 76
  // bits hold it with room. The divisor w<<9 is under 2^40.
  localparam int unsigned NUMW = 76;
  localparam int unsigned DENW = 40;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`.
  // `--lint-only` does not run it, so a clean lint says nothing about it; it is
  // fired by parameter override in the directed test.
  initial begin
    if (TAGW < 1 || TAGW > 32)
      $fatal(1, "zhao_geom_projradius: TAGW is %0d; the rider is 1..32 bits", TAGW);
  end

  localparam logic [3:0] S_IDLE   = 4'd0;
  localparam logic [3:0] S_M1     = 4'd1;   // acc = kx * bound_radius
  localparam logic [3:0] S_M2LOAD = 4'd2;   // move that product to the multiplicand
  localparam logic [3:0] S_M2     = 4'd3;   // acc = acc * viewport_w
  localparam logic [3:0] S_BIAS   = 4'd4;   // acc = acc + (w << 8)
  localparam logic [3:0] S_DIV    = 4'd5;   // quo = acc / (w << 9)
  localparam logic [3:0] S_FIN    = 4'd6;   // the last quotient bit is IN quo
  localparam logic [3:0] S_OUT    = 4'd7;   // hold the answer until it is taken

  logic [3:0] st_q;

  logic [NUMW-1:0] acc_q;   // the accumulating product, then the numerator
  logic [NUMW-1:0] mc_q;    // the shifted multiplicand
  logic [NUMW-1:0] quo_q;   // the quotient
  logic [31:0]     mp_q;    // the multiplier, shifted right one bit per step
  logic [DENW-1:0] den_q;
  logic [DENW-1:0] rem_q;   // invariant: rem_q < den_q
  logic [30:0]     w_q;
  logic [11:0]     vw_q;
  logic [6:0]      step_q;
  logic [TAGW-1:0] tag_r;

  assign req_ready_o = (st_q == S_IDLE);
  assign ans_valid_o = (st_q == S_OUT);
  assign tag_o       = tag_r;

  // ---- the refusals, decided on the accepting edge -------------------------
  // `behind_i` and `w_i == 0` are ONE refusal with two spellings: the
  // reference's test is `clip.w.raw <= 0`, and the projector reports the sign
  // half on `behind` and the magnitude half on `w`.
  wire no_depth_c  = behind_i || (w_i == 31'd0);
  wire bad_bound_c = (bound_radius_i <= 32'sd0);

  // ---- the restoring step --------------------------------------------------
  logic [DENW:0]   rem_shift_c;
  logic [DENW-1:0] rem_diff_c;
  logic            fits_c;
  assign rem_shift_c = {rem_q, acc_q[NUMW-1]};
  assign fits_c      = (rem_shift_c >= {1'b0, den_q});
  // Exact even when bit DENW is set: in the branch that uses it
  // rem_shift_c >= den_q and the true difference is < den_q <= 2^DENW - 1, so
  // it equals the low-DENW subtraction. Written DENW wide rather than DENW+1
  // to avoid carrying a bit that is provably discarded, which -Wall would flag.
  assign rem_diff_c  = rem_shift_c[DENW-1:0] - den_q;

  // Anything above bit 30 does not fit the signed 32-bit port.
  wire quo_over_c = |quo_q[NUMW-1:31];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q          <= S_IDLE;
      acc_q         <= '0;
      mc_q          <= '0;
      quo_q         <= '0;
      mp_q          <= 32'd0;
      den_q         <= '0;
      rem_q         <= '0;
      w_q           <= 31'd0;
      vw_q          <= 12'd0;
      step_q        <= 7'd0;
      tag_r         <= '0;
      radius_q8_o   <= 32'sd0;
      ans_ok_o      <= 1'b0;
      evaluations_o <= 32'd0;
      behind_o      <= 32'd0;
      bad_bound_o   <= 32'd0;
      saturated_o   <= 32'd0;
    end else begin
      case (st_q)
        S_IDLE: begin
          if (req_valid_i) begin
            tag_r <= tag_i;
            vw_q  <= vw_i;
            w_q   <= w_i;
            if (no_depth_c) begin
              behind_o    <= behind_o + 32'd1;
              radius_q8_o <= 32'sd0;
              ans_ok_o    <= 1'b0;
              st_q        <= S_OUT;
            end else if (bad_bound_c) begin
              bad_bound_o <= bad_bound_o + 32'd1;
              radius_q8_o <= 32'sd0;
              ans_ok_o    <= 1'b0;
              st_q        <= S_OUT;
            end else begin
              acc_q  <= '0;
              mc_q   <= NUMW'(kx_i);                 // the multiplicand
              mp_q   <= $unsigned(bound_radius_i);   // positive by the guard
              den_q  <= DENW'({w_i, 9'd0});
              rem_q  <= '0;
              quo_q  <= '0;
              step_q <= 7'd0;
              st_q   <= S_M1;
            end
          end
        end

        S_M1: begin
          // One shift-add step. `bound_radius` is 32 bits, so 32 steps.
          if (mp_q[0]) acc_q <= acc_q + mc_q;
          mc_q <= {mc_q[NUMW-2:0], 1'b0};
          mp_q <= {1'b0, mp_q[31:1]};
          if (step_q == 7'd31) begin
            step_q <= 7'd0;
            st_q   <= S_M2LOAD;
          end else begin
            step_q <= step_q + 7'd1;
          end
        end

        S_M2LOAD: begin
          // A whole cycle of its own, so the M1 product is IN `acc_q` before it
          // becomes the multiplicand. The alternative -- folding this into the
          // first M2 step -- was written first and thrown away: it needed the
          // partial product in two places and read as two different loops.
          mc_q   <= acc_q;
          acc_q  <= '0;
          mp_q   <= {20'd0, vw_q};
          step_q <= 7'd0;
          st_q   <= S_M2;
        end

        S_M2: begin
          // The M1 product (<= 2^62) times the 12-bit viewport width.
          if (mp_q[0]) acc_q <= acc_q + mc_q;
          mc_q <= {mc_q[NUMW-2:0], 1'b0};
          mp_q <= {1'b0, mp_q[31:1]};
          if (step_q == 7'd11) begin
            step_q <= 7'd0;
            st_q   <= S_BIAS;
          end else begin
            step_q <= step_q + 7'd1;
          end
        end

        S_BIAS: begin
          // The round-half-up term, `rden / 2` after the common-factor
          // reduction. Its own state because it must land before the first
          // division step reads `acc_q`'s top bit.
          acc_q  <= acc_q + NUMW'({w_q, 8'd0});
          rem_q  <= '0;
          quo_q  <= '0;
          step_q <= 7'd0;
          st_q   <= S_DIV;
        end

        S_DIV: begin
          // One restoring step: shift the remainder up by one numerator bit,
          // subtract the divisor if it fits, and record the quotient bit.
          acc_q <= {acc_q[NUMW-2:0], 1'b0};
          rem_q <= fits_c ? rem_diff_c : rem_shift_c[DENW-1:0];
          quo_q <= {quo_q[NUMW-2:0], fits_c};
          if (step_q == 7'(NUMW - 1)) begin
            step_q <= 7'd0;
            st_q   <= S_FIN;
          end else begin
            step_q <= step_q + 7'd1;
          end
        end

        S_FIN: begin
          // A whole cycle of its own, for the reason `zhao_measure_governor`'s
          // S_LOAD1 has one: the final quotient bit is written by the edge that
          // LEAVES S_DIV, so the answer is not readable until now. Reassembling
          // it combinationally at the transition duplicates the restoring
          // compare in a second place, which is exactly the kind of second
          // implementation of one law this project keeps out of its RTL.
          ans_ok_o      <= 1'b1;
          evaluations_o <= evaluations_o + 32'd1;
          if (quo_over_c) begin
            radius_q8_o <= 32'sh7FFF_FFFF;
            saturated_o <= saturated_o + 32'd1;
          end else begin
            radius_q8_o <= $signed({1'b0, quo_q[30:0]});
          end
          st_q <= S_OUT;
        end

        default: begin  // S_OUT -- the answer is held until it is taken
          if (ans_ready_i) st_q <= S_IDLE;
        end
      endcase
    end
  end

endmodule : zhao_geom_projradius

`default_nettype wire
