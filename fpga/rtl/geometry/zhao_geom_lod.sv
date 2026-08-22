// zhao_geom_lod.sv — the CREATURE representation ladder: raw rung selection
// plus the charter §9 stability law (minimum hold + hysteresis), one instance
// per evaluation (phase 8, ZH-037, GEOM.MESHFETCH).
//
// Law, in citation order:
//   reference/src/zcreature/creature_sim.cpp:167 — `zref::lod_raw` and
//       `zref::lod_update`. THE law, shipped and already used by the reference
//       simulation. This block implements those two functions and nothing else;
//       every constant below is read out of them rather than restated.
//   reference/include/zref/zref_creature.hpp, "LOD ladder" — the four rungs
//       (kMesh/kMicro/kSplat/kGlint), `kLodHoldTicks = 15`, and the
//       screen-space error law in prose.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Stability" — hysteresis and a
//       minimum hold are MANDATORY on every LOD path. §10 — the meshlet bands.
//   design/blocks.yml — GEOM.MESHFETCH `upstream: [CMD.SCHEDULER,
//       MEASURE.GOVERNOR]`, so `thresh_q8_i` is the governor's per-camera
//       pixel-error target. MEASURE.GOVERNOR is already UNIT_VERIFIED.
//   design/contracts/MEASURE.GOVERNOR.md — the constants are provisional.
//
// WHAT THIS BLOCK IS NOT. GEOM.MESHFETCH's purpose line gives it three jobs;
// this is only the third. It does not fetch meshlet descriptors, and it does
// not "cull against camera visibility sectors" — a phrase that appears exactly
// twice in this repository, both times in that purpose line and in the contract
// generated from it, with no spec, no reference and no second sentence
// anywhere. That is on the owner docket rather than guessed at; see
// reports/PHANTOM_REFERENCES.md, addendum 2026-08-22.
//
// ---------------------------------------------------------------------------
// THERE IS NO DIVIDER IN HERE, AND THAT IS THE DESIGN
// ---------------------------------------------------------------------------
//
// Written out directly, the law divides. The error of rung r is
//
//     err_r = round_half_up(proj * e_r / R)          (R = bound_radius)
//
// and a switch test compares against a boundary
//
//     bnd_r = round_half_up(thresh * R / e_r)
//
// so a literal transcription needs two 64/32 VARIABLE dividers — sequential,
// tens of cycles each, sitting on the per-instance path.
//
// NONE OF THEM ARE NEEDED, because every use of a quotient here is a COMPARISON
// against an integer, and for integers N >= 0, e > 0 and integer T:
//
//     floor(N/e) <= T   <=>   N < (T+1)*e
//     floor(N/e) >= T   <=>   N >= T*e
//
// Both are exact — identities, not approximations — so each divide becomes a
// multiply and a compare:
//
//   RUNG LEGALITY.  err_r <= thresh
//                     <=>  proj*e_r + R/2  <  (thresh + 1) * R
//     which mentions e_r and R only inside products. No divide at all, and the
//     rounding term survives exactly rather than being dropped.
//
//   COARSENING (eager: 10% BELOW the target rung's boundary).
//     The test is 10*proj <= 9*bnd, and bnd is an integer, so
//                     <=>  bnd >= ceil(10*proj / 9)  =:  K
//                     <=>  thresh*R + e/2  >=  K * e
//     leaving a division by the CONSTANT 9 — a multiply and a shift.
//
//   REFINING (lazy: 10% ABOVE the current rung's boundary).
//     The test is 10*proj >= 11*bnd, so
//                     <=>  bnd <= floor(10*proj / 11)  =:  M
//                     <=>  thresh*R + e/2  <  (M + 1) * e
//     leaving a division by the constant 11.
//
// So the whole ladder is multiplies and compares.
//
// THE IDENTITIES HOLD ONLY FOR A NON-NEGATIVE NUMERATOR. C++ integer division
// truncates toward zero, which equals floor only there, so a negative input
// would not cost this block precision — it would make it DISAGREE with
// `zref::lod_raw`. That is exactly the block's domain (`proj_radius_q8` is a
// projected radius, the three error terms are magnitudes, and `bound_radius`
// comes from `isqrt_u64`), and it is asserted below rather than assumed.
//
// A NOTE ON TWO CONSTANTS THAT ARE DELIBERATELY NOT USED. `compile_creature`
// sets `splat_error = bound_radius / 2` and `glint_error = bound_radius`
// (reference/src/zcreature/creature_core.cpp:569–570), which would make the
// glint rung's error exactly `proj` and collapse two of the three products.
// It is NOT exploited here: that relation is a property of the software that
// compiles creature types, nothing in hardware enforces it, and a block that
// silently assumed it would be wrong for any type built another way. If it is
// ever worth the logic it needs an assertion that FIRES when the relation
// breaks, not a silent assumption. Recorded so the opportunity is not lost and
// so nobody rediscovers it later and applies it without the guard.
//
// ---------------------------------------------------------------------------
// FIVE PRODUCTS, ONE MULTIPLIER: THE RATE NEVER ASKED FOR PARALLELISM
// ---------------------------------------------------------------------------
//
// MEASURED COST (Quartus 17.0.2 Lite, 5CSEBA6U23I7, block fit):
//
//   first synthesis        1,436 ALMs   28 DSPs   -- 72-bit operands
//   after narrowing        1,303 ALMs   18 DSPs   -- 64-bit, two products shared
//   after SEQUENCING       1,183 ALMs    6 DSPs   -- one multiplier, 5 cycles
//
// The 72-bit slack was free in simulation and expensive in silicon: a 72-bit
// operand asks for a 72x72 multiplier when the honest need is 32x32. Narrowing
// to a proven-sufficient 64, computing thresh*R ONCE (it appears in both the
// legality bound and the boundary numerator), and selecting the boundary
// multiplier operand before the multiply rather than after it took a third of
// the DSPs out.
//
// THE SECOND THIRD CAME FROM THE CLOCK, NOT FROM WIDTH. 18 DSPs was five 32x32
// products standing side by side -- thresh*R, the three legality products, and
// the boundary product -- in a block that evaluates ONCE PER INSTANCE PER
// FRAME. At 60 Hz and even ten thousand live instances that is 600k
// evaluations per second against a 50 MHz clock: eighty clocks available per
// evaluation, and one was being used. The parallelism was never bought by the
// rate; it was the shape a combinational transcription happens to take.
//
// So the five products now walk through ONE multiplier. There is exactly one
// `*` on the datapath below (`mul_p`), its operands are selected by the state,
// and the block answers in five clocks instead of one. Nothing about the
// arithmetic changed -- the same five products, the same widths, the same
// comparisons, the same oracle -- only WHEN each one is evaluated.
//
// THE ESTIMATE ABOVE THIS COMMENT USED TO SAY "roughly 8", and it was wrong in
// the cheap direction: it assumed only the three legality products would share.
// Once a sequencer exists, thresh*R and the boundary product have no reason to
// stay outside it, and all five went through one multiplier for 6.
//
// AND THE ALMs FELL TOO, which is the part worth carrying to the next block.
// The obvious objection to sequencing is that it trades area for DSPs: 250 more
// registers had to come from somewhere. They came from ALMs that already
// existed -- a Cyclone V ALM carries registers whether or not the design uses
// them -- while the five parallel 64-bit product-and-compare datapaths
// collapsed into one. Net: -120 ALMs and -12 DSPs, at once. Sequencing is not a
// trade here. It is a strict improvement bought with four clocks the rate was
// never going to miss.
//
// ---------------------------------------------------------------------------
// LATENCY AND HANDSHAKE
// ---------------------------------------------------------------------------
//
//   `ready_o`  high exactly when the block can accept work (state IDLE).
//   `tick_i`   accepted only while `ready_o`; ignored otherwise, so a caller
//              that ticks blindly cannot corrupt an evaluation in flight.
//   `valid_o`  a ONE-CYCLE pulse, 5 clocks after the edge that accepted the
//              tick. `rung_o`, `hold_o` and `raw_o` are the answer for that
//              tick's inputs on that cycle and hold their value afterwards.
//
//   cycle       0          1          2          3          4          5
//   state       IDLE       S_MICRO    S_SPLAT    S_GLINT    S_BND      IDLE
//   multiplier  thresh*R   proj*e_u   proj*e_s   proj*e_g   bnd*e_sel  --
//   the edge    latches    legality   legality   legality   answer,    --
//   ENDING it   + th_r     of kMicro  of kSplat  of kGlint  valid_o
//
// So `tick_i` is accepted by the edge ending cycle 0 -- which is also the edge
// that captures thresh*R, because those two operands are the ports themselves
// and need no latch first -- and `valid_o` is high during cycle 5. Five clocks,
// and the differential checks the number rather than merely waiting it out.
//
// THE INPUTS ARE LATCHED AT ACCEPT. The caller owns the ladder state and is
// free to move on the cycle after `ready_o` falls; nothing below reads a port
// after the accepting edge except `thresh_q8_i` and `bound_radius_i`, which
// are consumed by the thresh*R product ON that edge.
//
// Throughput is therefore one evaluation per five clocks, or 10 M/s at 50 MHz.
// The consumer (GEOM.MESHFETCH's descriptor fetch) is still unbuilt, so that
// is a number to check against a real instance count when there is one, not a
// number anything has yet asked for.

`default_nettype none

module zhao_geom_lod (
    input wire clk,
    input wire rst_n,

    // one evaluation of one instance's ladder
    input wire tick_i,

    // the projected bound radius of this instance, S12.8 px
    input wire signed [31:0] proj_radius_q8_i,
    // the governor's per-camera pixel-error threshold, S12.8
    input wire signed [31:0] thresh_q8_i,

    // the compiled creature type's geometry, fx16
    input wire signed [31:0] bound_radius_i,
    input wire signed [31:0] micro_error_i,
    input wire signed [31:0] splat_error_i,
    input wire signed [31:0] glint_error_i,

    // LADDER STATE IS THE CALLER'S. GEOM.MESHFETCH holds one LodState per live
    // instance, so this block takes the state in and hands the stepped state
    // back rather than owning an array whose size it cannot know.
    input wire [ 1:0] rung_i,
    input wire [15:0] hold_i,

    output logic [ 1:0] rung_o,
    output logic [15:0] hold_o,
    output logic [ 1:0] raw_o,
    output logic        valid_o,
    // high when a tick will be accepted; see LATENCY AND HANDSHAKE above
    output logic        ready_o
);

  // charter §9, and `zref::kLodHoldTicks`
  localparam logic [15:0] HOLD_TICKS = 16'd15;

  // ---- the sequencer ------------------------------------------------------
  // One state per product. The state is not an optimisation knob: it IS the
  // operand selector for the single multiplier, so a state that goes missing
  // does not slow the block down, it feeds the next comparison a stale product.
  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_MICRO = 3'd1;
  localparam logic [2:0] S_SPLAT = 3'd2;
  localparam logic [2:0] S_GLINT = 3'd3;
  localparam logic [2:0] S_BND = 3'd4;

  logic [2:0] state;
  assign ready_o = (state == S_IDLE);

  // ---- what the accepting edge latches ------------------------------------
  // `thresh_q8_i` is NOT among them: it is consumed by the thresh*R product on
  // the accepting edge itself and never read again.
  logic signed [31:0] proj_q;
  logic signed [31:0] r_q;
  logic signed [31:0] e_micro_q, e_splat_q, e_glint_q;
  logic [ 1:0] rung_q;
  logic [15:0] hold_q;

  // ---- rung legality: proj*e_r + R/2 < (thresh+1)*R ------------------------
  //
  // 64 BITS, NOT 72, AND THE WIDTH WAS MEASURED RATHER THAN GUESSED. The first
  // synthesis of this block carried 72-bit operands as "deliberate slack" and
  // cost **1,436 ALMs and 28 DSPs** — a quarter of the device's 112 DSPs for one
  // LOD evaluator — because a 72-bit operand asks Quartus for a 72x72
  // multiplier when the honest need is 32x32.
  //
  // 64 is provably enough. Every operand is bounded by 2^31, so the widest
  // product is proj*e_r at under 4.61e18, and adding R/2 keeps it under
  // 4.61e18 + 1.08e9 — comfortably inside the 9.22e18 a signed 64-bit holds.
  // The widest term anywhere below is K*e_sel at about 5.13e18, still inside it.
  localparam int W = 64;

  // The one multiplier's operands and product. DECLARED here because the
  // legality comparison below reads the product; the operand SELECTION lives
  // further down, next to the boundary operand it also has to choose.
  logic signed [W-1:0] mul_a, mul_b, mul_p;


  // thresh*R IS COMPUTED ONCE. It appears in the legality bound as
  // (thresh+1)*R and in the switch boundary's numerator as thresh*R + e/2, and
  // (thresh+1)*R is just thresh*R + R — so the two shared a multiplier all
  // along and were paying for it twice. It is the first product through the
  // shared multiplier, on the accepting edge.
  logic signed [W-1:0] th_r;

  logic signed [W-1:0] legal_rhs;
  assign legal_rhs = th_r + W'(r_q);

  // THE THREE RUNGS ARE WRITTEN OUT, NOT LOOPED, AND THAT IS DELIBERATE.
  //
  // This was a `logic signed [31:0] e_of[1:3]` filled in an always_comb and
  // indexed by a genvar inside a generate loop, and it was WRONG: all three
  // legality bits came out equal, so the block could only ever return the
  // coarsest rung or the finest -- never kMicro or kSplat. It still agreed with
  // the oracle on 27,618 of 29,459 checks, because most cases legitimately land
  // on rung 3 or rung 0, and it was the corner sweep that exposed it: ref=2,
  // dut=0, at four different bound radii and at R as small as 65,536.
  //
  // There are exactly THREE rungs with error terms -- the ladder is fixed by
  // charter 9, not parameterised -- so the loop bought nothing and cost the one
  // bug a loop can hide. Three named terms cannot silently share a value. The
  // sequencing did not weaken that: the three still have three named states,
  // three named error registers and three named legality flops.
  //
  // Rung 0 (kMesh) has no error term on purpose: it is the fallback taken when
  // no coarser rung is legal, and the reference never evaluates an error for it.
  logic signed [W-1:0] half_r;
  assign half_r = W'(r_q) >>> 1;

  // The legality comparison is shared because the three rungs differ only in
  // WHICH product reached it, and the state already says that.
  logic legal_next;
  assign legal_next = ((mul_p + half_r) < legal_rhs);

  logic legal_micro, legal_splat, legal_glint;

  // ---- raw = the COARSEST legal rung (the reference walks 3 down to 1) -----
  logic [1:0] raw;
  always_comb begin
    if (legal_glint) raw = 2'd3;
    else if (legal_splat) raw = 2'd2;
    else if (legal_micro) raw = 2'd1;
    else raw = 2'd0;
  end

  // ---- the switch test, only ever consulted when raw != rung --------------
  // Which rung's boundary is used differs by direction: coarsening looks at the
  // TARGET rung's boundary, refining at the CURRENT one's. Same shape both
  // ways, so one selected error term serves.
  logic coarsening;
  logic [1:0] bnd_rung;
  logic signed [31:0] e_sel;
  assign coarsening = (raw > rung_q);
  assign bnd_rung   = coarsening ? raw : rung_q;
  always_comb begin
    case (bnd_rung)
      2'd1:    e_sel = e_micro_q;
      2'd2:    e_sel = e_splat_q;
      2'd3:    e_sel = e_glint_q;
      default: e_sel = 32'sd0;  // rung 0 has no error term; the e == 0 path below
    endcase
  end

  // N = thresh*R + e/2 — the boundary's numerator, before the divide that is
  // never performed. Reuses the shared thresh*R product above.
  logic signed [W-1:0] bnd_num;
  assign bnd_num = th_r + (W'(e_sel) >>> 1);

  // K = ceil(10*proj / 9) and M = floor(10*proj / 11): division by a CONSTANT,
  // which synthesis turns into a multiply and a shift.
  //
  // THE DIVISION PATH IS 40 BITS, NOT 64, AND QUARTUS IS WHY. At 72 bits the
  // block did not synthesise at all:
  //
  //   Error (272006): In lpm_divide megafunction, LPM_WIDTHN must be less
  //                   than or equals to 64
  //
  // Three frontends and the differential were all perfectly happy with it --
  // this is the same shape as the inline `genvar` that only the Quartus run
  // caught, and it is the argument for running the fit rather than trusting
  // that three frontends agree.
  //
  // 40 bits is not a workaround, it is the honest width: `proj_radius_q8_i` is
  // bounded by 2^31, so 10*proj < 2^35 and both quotients are under 2^32.
  logic signed [39:0] proj10;
  logic signed [39:0] k_ceil;
  logic signed [39:0] m_floor;
  assign proj10  = 40'(proj_q) * 40'sd10;
  assign k_ceil  = (proj10 + 40'sd8) / 40'sd9;
  assign m_floor = proj10 / 40'sd11;

  // ONE BOUNDARY MULTIPLIER, NOT TWO. The coarsening and refining tests can
  // never both need a product in the same evaluation, so selecting the operand
  // BEFORE the multiply rather than after it is the same arithmetic with half
  // the silicon.
  //
  // What makes that safe is not a comment: ONE signal, `coarsening`, selects
  // both the operand below and the branch that consumes it. There is no second
  // condition that could drift out of step with the first -- splitting them is
  // the only way to break this, and it would take deleting the signal.
  // ENFORCED-BY: fpga/rtl/geometry/zhao_geom_lod.sv:coarsening
  logic signed [W-1:0] bnd_mul_a;
  logic signed [W-1:0] bnd_cmp;
  assign bnd_mul_a = coarsening ? W'(k_ceil) : (W'(m_floor) + W'(1));

  // ---- THE ONE MULTIPLIER -------------------------------------------------
  //
  // There is exactly one `*` on this datapath and every product in the ladder
  // goes through it. The operands are widened to the result width rather than
  // left to the assignment context -- the same spelling `zhao_geom_cull` uses
  // for `mul_pc`, and for the same measured reason.
  //
  // WHAT MAKES THIS SAFE is that the state selecting the operands is the SAME
  // state selecting which register catches the product. There is no second
  // sequencer that could drift out of step with the first.
  // ENFORCED-BY: fpga/rtl/geometry/zhao_geom_lod.sv:state
  always_comb begin
    case (state)
      S_MICRO: begin
        mul_a = W'(proj_q);
        mul_b = W'(e_micro_q);
      end
      S_SPLAT: begin
        mul_a = W'(proj_q);
        mul_b = W'(e_splat_q);
      end
      S_GLINT: begin
        mul_a = W'(proj_q);
        mul_b = W'(e_glint_q);
      end
      S_BND: begin
        mul_a = bnd_mul_a;
        mul_b = W'(e_sel);
      end
      default: begin  // S_IDLE — thresh*R, taken straight off the ports
        mul_a = W'(thresh_q8_i);
        mul_b = W'(bound_radius_i);
      end
    endcase
  end
  assign mul_p = mul_a * mul_b;
  // the S_BND phase of the shared multiplier — the product itself, named for
  // the comparison that reads it
  assign bnd_cmp   = mul_p;

  // The reference special-cases e == 0 to a boundary of ZERO rather than
  // dividing, and the transformed tests must not be used there — they were
  // derived under e > 0. With bnd = 0 the two tests degenerate to
  // `10*proj <= 0` and `10*proj >= 0`, which on this block's non-negative
  // domain are `proj == 0` and `true`.
  logic switch_ok;
  always_comb begin
    if (e_sel == 32'sd0) begin
      switch_ok = coarsening ? (proj_q == 32'sd0) : 1'b1;
    end else if (coarsening) begin
      switch_ok = (bnd_num >= bnd_cmp);
    end else begin
      switch_ok = (bnd_num < bnd_cmp);
    end
  end

  // ---- the stepped state --------------------------------------------------
  // The reference caps `hold` at 0xFFFF on the two "stay" paths that can be
  // reached repeatedly, and increments freely on the `hold < 15` path where no
  // overflow is possible. Saturating everywhere is equivalent, and is what is
  // written here; the differential covers the distinction.
  logic [15:0] hold_inc;
  assign hold_inc = (hold_q == 16'hFFFF) ? 16'hFFFF : (hold_q + 16'd1);

  logic [ 1:0] rung_next;
  logic [15:0] hold_next;
  always_comb begin
    if (raw == rung_q) begin
      rung_next = rung_q;
      hold_next = hold_inc;
    end else if (hold_q < HOLD_TICKS) begin
      rung_next = rung_q;
      hold_next = hold_inc;
    end else if (switch_ok) begin
      rung_next = raw;
      hold_next = 16'd0;
    end else begin
      rung_next = rung_q;
      hold_next = hold_inc;
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state       <= S_IDLE;
      rung_o      <= 2'd0;
      hold_o      <= 16'd0;
      raw_o       <= 2'd0;
      valid_o     <= 1'b0;
      proj_q      <= 32'sd0;
      r_q         <= 32'sd1;
      e_micro_q   <= 32'sd0;
      e_splat_q   <= 32'sd0;
      e_glint_q   <= 32'sd0;
      rung_q      <= 2'd0;
      hold_q      <= 16'd0;
      th_r        <= '0;
      legal_micro <= 1'b0;
      legal_splat <= 1'b0;
      legal_glint <= 1'b0;
    end else begin
      valid_o <= 1'b0;
      case (state)
        S_IDLE: begin
          if (tick_i) begin
            proj_q    <= proj_radius_q8_i;
            r_q       <= bound_radius_i;
            e_micro_q <= micro_error_i;
            e_splat_q <= splat_error_i;
            e_glint_q <= glint_error_i;
            rung_q    <= rung_i;
            hold_q    <= hold_i;
            th_r      <= mul_p;  // thresh * R, off the ports on this edge
            state     <= S_MICRO;
          end
        end
        S_MICRO: begin
          legal_micro <= legal_next;
          state       <= S_SPLAT;
        end
        S_SPLAT: begin
          legal_splat <= legal_next;
          state       <= S_GLINT;
        end
        S_GLINT: begin
          legal_glint <= legal_next;
          state       <= S_BND;
        end
        S_BND: begin
          rung_o  <= rung_next;
          hold_o  <= hold_next;
          raw_o   <= raw;
          valid_o <= 1'b1;
          state   <= S_IDLE;
        end
        default: state <= S_IDLE;
      endcase
    end
  end

`ifdef FORMAL
  // THE DOMAIN IS ASSERTED, NOT ASSUMED. Every divide above was removed with an
  // identity that holds only for a non-negative numerator, because C++ integer
  // division truncates toward zero and that equals floor only there. A negative
  // input would not cost precision — it would make this block DISAGREE with
  // `zref::lod_raw`, silently. So it fires instead of drifting.
  // ENFORCED-BY: tests/differential/geom_lod_directed.cpp
  always_ff @(posedge clk) begin
    if (rst_n && tick_i && ready_o) begin
      a_domain_proj : assert (proj_radius_q8_i >= 32'sd0);
      a_domain_thresh : assert (thresh_q8_i >= 32'sd0);
      a_domain_bound : assert (bound_radius_i > 32'sd0);
      a_domain_micro : assert (micro_error_i >= 32'sd0);
      a_domain_splat : assert (splat_error_i >= 32'sd0);
      a_domain_glint : assert (glint_error_i >= 32'sd0);
      a_rung_in_range : assert (rung_i <= 2'd3);
    end
  end
`endif

endmodule

`default_nettype wire
