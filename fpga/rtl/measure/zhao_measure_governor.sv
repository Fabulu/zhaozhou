// zhao_measure_governor.sv — MEASURE.GOVERNOR: the per-camera screen-error
// policy, and per-view degradation that cannot cross between players
// (phase 8, ZH-047).
//
// Law, in citation order:
//   design/contracts/MEASURE.GOVERNOR.md — the block contract, where every
//       chosen law below is argued at length with what it rejected.
//   design/blocks.yml — `inputs: [screen_error_stats, dispatch]`, `outputs:
//       [lod_targets]`, `upstream: [CMD.SCHEDULER, MEASURE.HISTOGRAM]`,
//       `downstream: [TERRAIN.LOD, GEOM.MESHFETCH, PART.LADDER]`,
//       `backpressure: ready_valid`, `latency: variable`, "1 decision per
//       frame per camera", counter `lod_representation_counts`,
//       `source_ids: true`, note "Hysteresis/hold constants provisional until
//       Wound Lab evidence (charter §12)".
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Practical implementation
//       path", Version 1 — "ARM predicts a pixel-error threshold per camera
//       from prior counters; FPGA performs local hierarchy traversal against
//       that threshold". THE THRESHOLD IS AN INPUT, NOT A COMPUTATION. This
//       block does not predict; it converts and it degrades.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Inputs" — each camera
//       provides a "projection scale" and a "per-camera pixel-error
//       threshold". Two numbers in; ONE ratio out (see THE SEAM).
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Stability" — "Every LOD path
//       requires: hysteresis; minimum hold duration; parent/child geomorph
//       where possible; ... camera-motion-aware anti-thrashing."
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §9 "Duo fairness" — the 45/45/10
//       split and "One player looking directly into a volcano cannot make the
//       other player's army disappear".
//   FORM_LANGUAGE_HARDWARE_CODESIGN.md §12 — `present duo { view left ...
//       budget 45% ; view right ... budget 45% ; shared emergency budget 10% }`
//       and "Split-screen budgets are declared and enforced as part of
//       presentation" (item 16). The declaration is per view, so the
//       ENFORCEMENT must be per view, and so must the degradation.
//   spec/video_rules.md §3/§3.1 — `VIDEO_DUO` is a shipped mode: two
//       independent 256x192 canvases on the 512x240 raster. Split screen is
//       not hypothetical, which is why per-view policy is not either.
//   spec/commands.zidl `SetView 0x0010` — `fx16 pixel_error`, per view. The
//       ARM's prediction, on the wire, already.
//   spec/commands.zidl `SetPresentationContract 0x0020` — `u8 view_count`.
//   spec/qformats.md §3 — `round_half_up(n/d) = floor((n + floor(d/2))/d)`
//       for `d > 0`, and the single-rounding law: ONE rounding per result.
//   design/contracts/TERRAIN.LOD.md and fpga/rtl/terrain/zhao_terrain_lod.sv —
//       the LANDED consumer, whose port list and whose ladder arithmetic this
//       block is written against.
//
// ---------------------------------------------------------------------------
// THE SEAM — TERRAIN.LOD ALREADY CHOSE IT, AND THIS BLOCK HONOURS IT
// ---------------------------------------------------------------------------
// TERRAIN.LOD landed in phase 6 against a STUB contract for this block, and
// its Notes law 8 says what it assumed:
//
//     "The governor's per-camera policy is ONE ratio, not two numbers. Charter
//      §9 gives a 'projection scale' and a 'per-camera pixel-error threshold';
//      the ladder only ever uses their quotient, and carrying the quotient
//      keeps the comparison exact. Carrying both would force a division or a
//      rounding inside the block for no expressive gain."
//
// That is honoured exactly: the two numbers arrive HERE, the division happens
// HERE — once per frame per camera, which is the cheapest place in the machine
// for it — and TERRAIN.LOD keeps its exact, rounding-free ladder.
//
// THE DIRECTION OF THE RATIO, AND A DISCREPANCY INSIDE TERRAIN.LOD ITSELF.
// TERRAIN.LOD's ladder is `dev[L] · scale <= distance · h`, coarsest level
// wins. `scale` multiplies the DEVIATION, so a LARGER scale makes the test
// HARDER and the result FINER. Its prose says the opposite — `zref_terrain_lod
// .hpp` calls `scale` "world-units of allowed error per world-unit of
// distance" and states "Larger = coarser". The prose is backwards relative to
// the arithmetic in the same file, and the arithmetic is what ships, what the
// RTL implements and what `terrain_lod_directed` pins ("scale = 256 (1.0), so
// the ladder is `dev <= distance`"). This block is written against the
// ARITHMETIC, and the discrepancy is REPORTED rather than silently resolved —
// see the contract. Dimensionally the arithmetic is the correct one:
//
//     projected pixels of error  ~=  dev · proj_scale / distance
//     admissible                <=>  dev · proj_scale / distance <= px_err
//                               <=>  dev · (proj_scale / px_err) <= distance
//
// so `scale = proj_scale / px_err` — pixels-per-unit-angle over allowed
// pixels, i.e. INVERSE tolerance. Worked end to end for the shipped Duo
// canvas in the contract, and it lands on the right pixel count exactly.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN, NOT FOUND (each also argued in the contract)
// ---------------------------------------------------------------------------
// G1. THE RATIO IS `scale = round_half_up((proj << (16 - deg)) / px_err)`,
//     CLAMPED TO 16 BITS, WITH EXACTLY ONE ROUNDING.
//     `proj` is Q8.8, `px_err` is fx16, the result is the Q8.8 `cam*_scale_i`
//     TERRAIN.LOD takes: scale_raw = proj_raw · 2^16 / px_err_raw. The
//     rounding is qformats §3's `round_half_up(n/d)` and it is the ONLY
//     rounding in the block.
//     REJECTED: truncation (cheaper by one adder, and it is not the ratified
//     integer-division rounding — a governor that rounded differently from
//     the rest of the machine would be a second rounding law); computing a
//     reciprocal and multiplying (two roundings, forbidden by §3's
//     single-rounding law, and no cheaper here at one divide per frame).
// G2. DEGRADATION IS A POWER-OF-TWO LADDER ON THE NUMERATOR, SO IT IS EXACT.
//     `deg` in 0..DEG_MAX shifts the numerator right, which divides `scale`
//     by 2^deg, which multiplies the ALLOWED PIXEL ERROR by 2^deg. A shift
//     introduces no rounding of its own, so G1's single rounding survives.
//     REJECTED: a Q8.8 degrade multiplier (1.0, 1.25, 1.5, ...). Finer
//     control, and it needs either a second rounding (banned by §3) or a
//     42-bit divide to fold the multiply into the quotient — paying a
//     four-times-wider divider for a knob no evidence exists to set.
// G3. EACH VIEW'S DEGRADE RUNG IS A FUNCTION OF THAT VIEW'S OWN PRESSURE AND
//     NOTHING ELSE. There is no path in this file from `starved1_i` to
//     `cam0_scale_o`. This is charter §9's Duo fairness sentence applied to
//     the POLICY the way MEASURE.TOKENS' law T2 applies it to the POOLS, and
//     it is the same shape deliberately: FORM_LANGUAGE §12 declares budgets
//     per view, so enforcement and degradation must both be per view or the
//     declaration means nothing.
//     REJECTED, AND THIS ONE MATTERS: a single global degrade rung driven by
//     whichever view is worse off. It is simpler, it is one register instead
//     of two, and it is exactly the failure the charter names — one player
//     looking into a volcano would coarsen the OTHER player's world. A
//     global rung also cannot express the declared 45/45 split at all: two
//     views with equal budgets and unequal load must degrade unequally, and
//     one number cannot say that.
//     ENFORCED-BY: tests/measure/measure_governor_directed.cpp:test_volcano
// G4. THE LADDER CLIMBS IMMEDIATELY AND RECOVERS ONLY AFTER A HOLD.
//     A starved frame raises `deg` at once — a stall is visible now, so the
//     response is now. Recovery waits `DEG_HOLD` unstarved frames. The
//     asymmetry IS charter §9's "no visible threshold flicker" gate: a view
//     sitting exactly at its budget would otherwise degrade and recover every
//     other frame forever.
//     REJECTED: a symmetric hold (the oscillation above, delayed but not
//     removed); immediate recovery (the same oscillation at frame rate);
//     never recovering (a single bad frame would coarsen a whole level).
// G5. THE STABILITY CONSTANTS ARE PARAMETERS OF THIS BLOCK, PROVISIONAL, AND
//     THE LEDGER ALREADY SAYS SO ("Hysteresis/hold constants provisional
//     until Wound Lab evidence"). They are not ABI fields — there is no
//     command carrying a hysteresis band — so a port would only move the
//     invention to a producer that has nothing to base it on either.
//       HYST_Q88   = 320   (1.25x). TERRAIN.LOD reads anything below 256 as
//                          256, i.e. as NO hysteresis, and charter §9 says
//                          every LOD path REQUIRES hysteresis — so 256 is not
//                          an option. 1.25x is a quarter-level of slack.
//                          REJECTED: 512 (2x), which is a full level of slack
//                          and makes the ladder lag a whole rung behind a
//                          moving camera.
//       MIN_HOLD   = 6     frames = 100 ms at 60 Hz.
//       MORPH_STEP = 10923 Q16 per frame. CHOSEN TO SATISFY A THEOREM, not
//                          picked: MIN_HOLD · MORPH_STEP >= 65536
//                          (6 · 10923 = 65538), so a geomorph ALWAYS reaches
//                          unity before the minimum hold can permit the next
//                          change. No level is ever replaced mid-morph, which
//                          is what "parent/child geomorph where possible"
//                          plus "minimum hold duration" mean together.
//                          REJECTED: 65536/MIN_HOLD = 10922 (floor), which
//                          leaves the morph 4/65536 short at the moment the
//                          hold expires — the one frame in which a half-
//                          morphed subpatch could be re-targeted.
//       DEG_HOLD   = 12    frames, twice MIN_HOLD. A degrade rung moves EVERY
//                          subpatch's target at once, so it must be rarer
//                          than a per-subpatch change or it becomes the
//                          dominant source of level churn.
// G6. `px_err == 0` YIELDS THE MAXIMUM SCALE (0xFFFF), AND `proj == 0` YIELDS
//     ZERO. Neither is a special case bolted on: they are the limits. Zero
//     allowed pixel error demands infinite precision, and the finest the
//     ladder can be asked for is the largest scale; a camera with zero
//     projection scale puts nothing on screen, and the coarsest ladder is the
//     right answer for it. REJECTED: refusing the frame (the governor would
//     have no targets to present and TERRAIN.LOD would sample stale ones —
//     silence in place of an answer).
// G7. THE COUNTER IS FRAMES SPENT AT EACH DEGRADE RUNG, SUMMED OVER ENABLED
//     VIEWS. `lod_representation_counts` needs a source, this block makes no
//     per-object representation decision, and the rung IS the representation
//     allowance the Measure granted that frame. Four rungs, four lanes — the
//     same shape TERRAIN.LOD's four lanes have.
//     REJECTED: not driving the counter at all and recording a ledger
//     deviation. The rung is a genuine measurement of what the Measure
//     allowed, and a post-mortem asking "how long was player 2 degraded" has
//     no other place to look.
//
// NOT IN THIS BLOCK, deliberately: no threshold PREDICTION (charter Version 1
// puts that on the ARM, explicitly); no error histogram and no cutoff bucket
// (MEASURE.HISTOGRAM, charter Version 2 — and `upstream: [MEASURE.HISTOGRAM]`
// is therefore an edge with nothing on the far end, named rather than faked);
// no priority heap (§9: "Do not begin with a global FPGA priority heap"); no
// camera POSITIONS, no `dual` and no `edge_*` (TERRAIN.LOD's contract groups
// them under `lod_targets`, but they are not policy — see the contract); no
// token accounting (MEASURE.TOKENS owns the pools; this block takes a
// one-bit-per-view verdict from them).
//
// Conservative SystemVerilog subset only (charter §2). No function-call result
// is indexed anywhere in this file: Verilator accepts `f(x)[7:0]`, Quartus
// 17.0 rejects it outright, and it cost GEOM.BINNER a synthesis failure that
// every simulation lane passed.

module zhao_measure_governor #(
    // The provisional §9 stability constants (law G5).
    parameter int unsigned HYST_Q88   = 320,
    parameter int unsigned MIN_HOLD   = 6,
    parameter int unsigned MORPH_STEP = 10923,
    parameter int unsigned DEG_HOLD   = 12,
    parameter int unsigned DEG_MAX    = 3
) (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // `dispatch` (CMD.SCHEDULER — SetView / SetPresentationContract).
    // `frame_i` is a one-cycle pulse at the frame boundary: decide now.
    // -----------------------------------------------------------------------
    input logic        frame_i,
    input logic [ 1:0] view_count_i,  // SetPresentationContract.view_count
    input logic [31:0] px_err0_i,     // SetView.pixel_error, fx16 unsigned
    input logic [31:0] px_err1_i,
    input logic [15:0] proj0_i,       // camera projection scale, Q8.8 unsigned
    input logic [15:0] proj1_i,
    input logic [15:0] src_id_i,      // `source_ids: true`

    // -----------------------------------------------------------------------
    // `screen_error_stats` — the per-view verdict for the frame just ended.
    // ONE BIT PER VIEW, and each one reaches only its own view's rung (G3).
    // MEASURE.TOKENS is the natural producer: a view was starved iff it had a
    // request denied against its own guaranteed pool.
    // -----------------------------------------------------------------------
    input logic starved0_i,
    input logic starved1_i,

    // -----------------------------------------------------------------------
    // `lod_targets` — HELD, not pulsed. TERRAIN.LOD samples these with each
    // descriptor and requires them stable across a patch job, so they change
    // only at `targets_valid_o` and the old values stand until then.
    // -----------------------------------------------------------------------
    output logic        targets_valid_o,  // one-cycle pulse: the targets moved
    output logic        busy_o,           // a decision is in flight
    output logic [15:0] cam0_scale_o,
    output logic [15:0] cam1_scale_o,
    output logic        cam0_en_o,
    output logic        cam1_en_o,
    output logic [15:0] hyst_o,
    output logic [ 7:0] min_hold_o,
    output logic [16:0] morph_step_o,
    output logic [15:0] src_id_o,

    // The rung each view is at, for capture and post-mortem.
    output logic [1:0] deg0_o,
    output logic [1:0] deg1_o,

    // `lod_representation_counts`: frames at each rung, summed over enabled
    // views (law G7).
    output logic [31:0] lod_rep_count0_o,
    output logic [31:0] lod_rep_count1_o,
    output logic [31:0] lod_rep_count2_o,
    output logic [31:0] lod_rep_count3_o
);

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;
  localparam logic [15:0] SCALE_MAX = 16'hFFFF;

  // The constants are structural, not runtime state.
  assign hyst_o       = 16'(HYST_Q88);
  assign min_hold_o   = 8'(MIN_HOLD);
  assign morph_step_o = 17'(MORPH_STEP);

  // ---- per-view degrade rungs (laws G3, G4) --------------------------------
  // Two registers, never cross-wired. `deg_hold` ages ONLY while the rung is
  // above zero: a view at rung 0 has nothing to recover from.
  logic [1:0] deg0_r, deg1_r;
  logic [7:0] hold0_r, hold1_r;

  assign deg0_o = deg0_r;
  assign deg1_o = deg1_r;

  // Next rung for one view. Written as a function so the two views cannot
  // drift apart, and its RESULT IS NEVER INDEXED (Quartus 17.0).
  function automatic logic [1:0] deg_next(input logic [1:0] deg, input logic [7:0] hold,
                                          input logic starved);
    begin
      if (starved) begin
        deg_next = (deg == 2'(DEG_MAX)) ? deg : (deg + 2'd1);
      end else if (deg != 2'd0 && hold >= 8'(DEG_HOLD - 1)) begin
        deg_next = deg - 2'd1;
      end else begin
        deg_next = deg;
      end
    end
  endfunction

  function automatic logic [7:0] hold_next(input logic [1:0] deg, input logic [7:0] hold,
                                           input logic starved);
    begin
      if (starved) hold_next = 8'd0;  // climbing re-arms the recovery clock
      else if (deg == 2'd0) hold_next = 8'd0;
      else if (hold >= 8'(DEG_HOLD - 1)) hold_next = 8'd0;
      else hold_next = hold + 8'd1;
    end
  endfunction

  // ---- the divider (law G1) ------------------------------------------------
  // ONE restoring divider, sequenced across the two cameras: this block runs
  // once per frame, so a second divider would buy nothing and cost a second
  // 33-step datapath. 33 iterations, because the round-half-up numerator
  // `(proj << (16-deg)) + (px_err >> 1)` is at most
  // (2^32 - 2^16) + (2^31 - 1) < 2^33, and a 33-bit numerator over a divisor
  // of at least one needs 33 quotient bits.
  localparam int unsigned STEPS = 33;

  logic [32:0] num_r;  // the shifted, rounding-biased numerator
  logic [31:0] den_r;
  logic [31:0] rem_r;  // invariant: rem_r < den_r, so 32 bits always suffice
  logic [32:0] quo_r;
  logic [ 5:0] step_r;

  // Numerator for one view, EXACT: the shift carries the degrade (G2) and the
  // bias carries qformats section 3's round-half-up. No rounding happens here
  // -- the single rounding is the floor of the division that follows.
  function automatic logic [32:0] num_of(input logic [15:0] proj, input logic [31:0] px_err,
                                         input logic [1:0] deg);
    logic [31:0] shifted;
    begin
      shifted = {16'b0, proj} << (6'd16 - {4'b0, deg});
      num_of  = {1'b0, shifted} + ({1'b0, px_err} >> 1);
    end
  endfunction

  // ---- the FSM -------------------------------------------------------------
  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_DIV0 = 3'd1;
  localparam logic [2:0] S_LOAD1 = 3'd2;
  localparam logic [2:0] S_DIV1 = 3'd3;
  localparam logic [2:0] S_DONE = 3'd4;

  logic [ 2:0] state_r;
  logic [15:0] scale0_r;  // view 0's answer, held while view 1 divides
  logic [ 1:0] nd0_r, nd1_r;  // the rungs THIS decision was taken at
  logic [ 1:0] vc_r;
  logic [15:0] sid_r;
  logic        zero0_r, zero1_r;  // px_err was zero (law G6)
  logic [31:0] px1_r;  // view 1's operands, latched at the frame pulse
  logic [15:0] pj1_r;

  assign busy_o = (state_r != S_IDLE);

  // The quotient, clamped to the 16-bit Q8.8 port TERRAIN.LOD takes.
  logic [15:0] quo_clamped;
  always_comb begin
    if (quo_r[32:16] != 17'd0) quo_clamped = SCALE_MAX;
    else quo_clamped = quo_r[15:0];
  end

  logic [32:0] rem_shift;
  logic [31:0] rem_diff;
  assign rem_shift = {rem_r, num_r[32]};
  // The 32-bit subtract is EXACT even when `rem_shift` has bit 32 set. In the
  // branch that uses it, `rem_shift >= den_r` and the true difference is
  // < den_r <= 2^32 - 1, so it equals (rem_shift[31:0] - den_r) mod 2^32.
  // Writing it 32 bits wide rather than 33 avoids carrying a bit that is
  // provably discarded -- which -Wall would flag, and rightly.
  assign rem_diff  = rem_shift[31:0] - den_r;

  // The rungs this frame decides at, computed once (laws G3, G4). NOTE the
  // shape: view 0's next rung is a function of view 0's state and view 0's
  // pressure ONLY. That is law G3 as a structural fact rather than a claim.
  logic [1:0] deg0_nxt, deg1_nxt;
  logic [7:0] hold0_nxt, hold1_nxt;
  assign deg0_nxt  = deg_next(deg0_r, hold0_r, starved0_i);
  assign deg1_nxt  = deg_next(deg1_r, hold1_r, starved1_i);
  assign hold0_nxt = hold_next(deg0_r, hold0_r, starved0_i);
  assign hold1_nxt = hold_next(deg1_r, hold1_r, starved1_i);

  logic [31:0] rep_cnt_r[0:3];
  assign lod_rep_count0_o = rep_cnt_r[0];
  assign lod_rep_count1_o = rep_cnt_r[1];
  assign lod_rep_count2_o = rep_cnt_r[2];
  assign lod_rep_count3_o = rep_cnt_r[3];

  function automatic logic [31:0] cnt_inc(input logic [31:0] cur, input logic [31:0] by);
    logic [32:0] w;
    begin
      w = {1'b0, cur} + {1'b0, by};
      if (w[32]) cnt_inc = CNT_MAX;
      else cnt_inc = w[31:0];
    end
  endfunction

  integer i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_r         <= S_IDLE;
      deg0_r          <= 2'd0;
      deg1_r          <= 2'd0;
      hold0_r         <= 8'd0;
      hold1_r         <= 8'd0;
      num_r           <= 33'd0;
      den_r           <= 32'd0;
      rem_r           <= 32'd0;
      quo_r           <= 33'd0;
      step_r          <= 6'd0;
      scale0_r        <= 16'd0;
      nd0_r           <= 2'd0;
      nd1_r           <= 2'd0;
      vc_r            <= 2'd0;
      sid_r           <= 16'd0;
      zero0_r         <= 1'b0;
      zero1_r         <= 1'b0;
      px1_r           <= 32'd0;
      pj1_r           <= 16'd0;
      targets_valid_o <= 1'b0;
      // Reset the targets to a SAFE, VALID policy rather than to zero: a zero
      // scale would make TERRAIN.LOD admit every level and put the whole world
      // at its coarsest before the first frame ever arrives. 256 is the
      // ladder's own neutral point ("dev <= distance").
      cam0_scale_o    <= 16'd256;
      cam1_scale_o    <= 16'd256;
      cam0_en_o       <= 1'b1;
      cam1_en_o       <= 1'b0;
      src_id_o        <= 16'd0;
      for (i = 0; i < 4; i = i + 1) rep_cnt_r[i] <= 32'd0;
    end else begin
      targets_valid_o <= 1'b0;

      case (state_r)
        S_IDLE: begin
          if (frame_i) begin
            // The rungs move FIRST, from the frame that just ended, and the
            // decision below is taken at the NEW rung.
            deg0_r  <= deg0_nxt;
            deg1_r  <= deg1_nxt;
            hold0_r <= hold0_nxt;
            hold1_r <= hold1_nxt;
            nd0_r   <= deg0_nxt;
            nd1_r   <= deg1_nxt;
            vc_r    <= view_count_i;
            sid_r   <= src_id_i;
            zero0_r <= (px_err0_i == 32'd0);
            zero1_r <= (px_err1_i == 32'd0);

            num_r   <= num_of(proj0_i, px_err0_i, deg0_nxt);
            // LAW G6's zero case is handled by OVERRIDING THE RESULT, but the
            // divisor is forced to 1 so the restoring invariant `rem < den`
            // holds anyway and the remainder cannot run past 32 bits. A
            // divider fed zero would double its remainder every step.
            den_r   <= (px_err0_i == 32'd0) ? 32'd1 : px_err0_i;
            rem_r   <= 32'd0;
            quo_r   <= 33'd0;
            step_r  <= 6'd0;
            state_r <= S_DIV0;
            // View 1's operands are latched HERE too: px_err1_i and proj1_i
            // are the caller's wires and need not still be there 35 cycles
            // from now. They ride these two registers until S_LOAD1.
            px1_r   <= px_err1_i;
            pj1_r   <= proj1_i;
          end
        end

        S_DIV0, S_DIV1: begin
          // One restoring step: shift the remainder up by one numerator bit,
          // subtract the divisor if it fits, and record the quotient bit.
          num_r <= {num_r[31:0], 1'b0};
          if (rem_shift >= {1'b0, den_r}) begin
            // Both branches fit 32 bits by the invariant `rem_r < den_r`:
            // rem_shift = 2*rem + bit < 2*den, so the difference is < den and
            // the untaken branch is < den. den_r is never zero (see S_IDLE).
            rem_r <= rem_diff;
            quo_r <= {quo_r[31:0], 1'b1};
          end else begin
            rem_r <= rem_shift[31:0];
            quo_r <= {quo_r[31:0], 1'b0};
          end

          if (step_r == 6'(STEPS - 1)) begin
            step_r  <= 6'd0;
            state_r <= (state_r == S_DIV0) ? S_LOAD1 : S_DONE;
          end else begin
            step_r <= step_r + 6'd1;
          end
        end

        S_LOAD1: begin
          // A whole cycle of its own, so the final quotient bit is IN `quo_r`
          // before anything reads it. The alternative -- reassembling that last
          // bit combinationally at the transition -- was written first and
          // thrown away: it duplicated the restoring step's compare in a second
          // place, which is exactly the kind of second implementation of one
          // law this project keeps out of its RTL.
          scale0_r <= zero0_r ? SCALE_MAX : quo_clamped;
          num_r    <= num_of(pj1_r, px1_r, nd1_r);
          den_r    <= (px1_r == 32'd0) ? 32'd1 : px1_r;
          rem_r    <= 32'd0;
          quo_r    <= 33'd0;
          step_r   <= 6'd0;
          state_r  <= S_DIV1;
        end

        default: begin  // S_DONE -- publish
          state_r         <= S_IDLE;
          targets_valid_o <= 1'b1;
          cam0_scale_o    <= scale0_r;
          cam1_scale_o    <= zero1_r ? SCALE_MAX : quo_clamped;
          cam0_en_o       <= (vc_r >= 2'd1);
          cam1_en_o       <= (vc_r >= 2'd2);
          src_id_o        <= sid_r;
          // LAW G7 -- frames at each rung, summed over ENABLED views only.
          // Both views at the same rung add TWO to that lane, which is why the
          // increment takes an amount rather than being a bare +1.
          if (vc_r >= 2'd2 && nd0_r == nd1_r) begin
            rep_cnt_r[nd0_r] <= cnt_inc(rep_cnt_r[nd0_r], 32'd2);
          end else begin
            if (vc_r >= 2'd1) rep_cnt_r[nd0_r] <= cnt_inc(rep_cnt_r[nd0_r], 32'd1);
            if (vc_r >= 2'd2) rep_cnt_r[nd1_r] <= cnt_inc(rep_cnt_r[nd1_r], 32'd1);
          end
        end
      endcase
    end
  end

endmodule
