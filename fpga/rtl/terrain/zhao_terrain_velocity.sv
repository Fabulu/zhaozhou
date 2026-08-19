// zhao_terrain_velocity.sv — TERRAIN.VELOCITY: the height-velocity lattice
// (phase 7, ZH-039).
//
// Law, in citation order:
//   design/contracts/TERRAIN.VELOCITY.md — the block contract, where every
//       chosen law below is argued at length.
//   design/blocks.yml — `inputs: [patch_state]`, `outputs: [height_velocity]`,
//       `upstream: [TERRAIN.PATCH]`, `downstream: [MEM.GUARD]`,
//       `latency: variable`, "1 velocity sample per clock", counter
//       `terrain_samples_evaluated`, note "Driven by the DCURVE-based Earth8
//       velocity lane (plan 1.D)".
//   spec/terrain_rules.md §4.4 — "Velocity is the Earth velocity out-lane
//       ACCUMULATED at lattice vertices (TERRAIN.VELOCITY), interpolated by
//       the same §4.3 rule." The interpolation is the CONSUMER's
//       (`column_query`); this block produces the lattice it interpolates.
//   spec/terrain_rules.md §4.2 — "the velocity lattice (height16-scaled,
//       2 B/vertex) 545 KiB", produced once per frame beside the composed
//       height cache. That is the storage format, and it is frozen.
//   spec/terrain_rules.md §4.1 — field programs are evaluated ONLY at lattice
//       vertices by the ONE interpreter. Nothing here evaluates anything.
//   spec/terrain_rules.md §9.1 — the CLOSED-interval footprint test and
//       MAX_PATCH_FIELDS = 16.
//   spec/form/field-ir.md §7.1 — the earth output record
//       {height:fx, velocity:fx, material:u32, nav_cost:fx}: velocity is
//       OUT-LANE 1, Q16.16.
//   design/ops.yml FIELD.OUT.VELOCITY — `result_q: spec/qformats.md
//       §height16`, `rounding: saturating`. The ratified bake-back.
//   spec/qformats.md §2/§9 — fx16 -> height16 is `rescale(x,8)` then saturate
//       s16; §3/§4 — `fx_add` saturates, `rescale` is round-half-up (ties
//       toward +infinity), and there is exactly ONE rounding per result.
//   reference/src/zrender/terrain.cpp — `compose_lattice` /
//       `field_velocity_lane`: the shipped renderer's velocity recording, and
//       the z-then-x order this block sweeps in.
//   reference/include/zref/zref_terrain_velocity.hpp — the oracle, a thin
//       view onto that lane, cross-checked against it over a real 33x33 patch.
//
// ---------------------------------------------------------------------------
// THE SEAM — recorded, not invented
// ---------------------------------------------------------------------------
// The ledger gives this block `inputs: [patch_state]`, `upstream:
// [TERRAIN.PATCH]`. **`patch_state` as TERRAIN.PATCH actually emits it carries
// no velocity at all** — it is {top, bottom, compose_top, dirty, src_id}, the
// §3.4 height composition. The velocity number is out-lane 1 of the SAME earth
// evaluations whose out-lane 0 TERRAIN.PATCH consumes (field-ir §7.1), so the
// ledger line is a ROUTING statement (velocity rides the same per-vertex walk
// as the composition) and not an arithmetic one. Two ports are therefore
// needed where the ledger names one, and the deviation is written down here
// and in the contract rather than resolved by fiat:
//
//   * `lane_velocity_i` — the earth velocity out-lane, from FIELD.SEQ.EARTH,
//     the same stream whose height sibling reaches TERRAIN.PATCH;
//   * `lane_covers_i` — the §9.1 closed-interval footprint answer, DECIDED
//     ONCE by the block that owns the §9.1 list (TERRAIN.PATCH's `cur_covers`,
//     now exported as `fld_covers_o`) and travelling with the lane.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN, NOT FOUND (each also argued in the contract)
// ---------------------------------------------------------------------------
// V1. THE ACCUMULATION IS A SATURATING fx_add CHAIN IN COMMAND ORDER OVER
//     COVERING LANES ONLY, WITH EXACTLY ONE BAKE-BACK AT THE END.
//     §4.4 says "accumulated" and stops. `compose_lattice` does not accumulate
//     at all — it pushes one sample per (application, covered vertex) and
//     nothing in this tree consumes them, so the reduction has never been
//     written down. It is chosen to be the SAME reduction §3.4 already applies
//     to the height lane, at the same vertex, from the same evaluations.
//     REJECTED: last-writer-wins (the ground's measured SPEED would depend on
//     command order where its measured HEIGHT does not); max-magnitude (order
//     independent, but two opposed waves that cancel exactly in height would
//     still report the larger one's full speed); accumulating in height16
//     (two roundings per lane — qformats §3's single-rounding law forbids it).
// V2. A VERTEX NO LANE COVERS HAS VELOCITY EXACTLY ZERO, and its word is
//     written anyway. §4.2's lattice is 2 B for EVERY vertex, so the word must
//     be defined; ground no live field touches is not moving. REJECTED:
//     leaving the previous frame's word (a persistence reading) — a wave's
//     trailing edge would keep a stale speed for the rest of the level and
//     there is no decay pass anywhere in this tree to retire it.
// V3. THE BLOCK OWNS ITS 33x33 z-then-x SWEEP, exactly as TERRAIN.BAKE owns
//     its own. `compose_lattice` records velocity in that order
//     ("columns ascending z-then-x — the recorded velocity order") and the
//     §4.2 lattice is addressed in it. The block therefore DRIVES
//     `vtx_vi_o`/`vtx_vj_o` and the lane producer answers. REJECTED: taking
//     (vi, vj) on the lane stream — it would let a producer reorder or skip
//     vertices and make the lattice's completeness an upstream promise instead
//     of a structural fact.
// V4. THE LANE STREAM IS VERTEX-MAJOR: `lanes_i` words per vertex, in list
//     order, vertices in sweep order. This is not a new requirement — it is
//     TERRAIN.PATCH's chosen law 1 verbatim, and it MUST be the same one
//     because both blocks consume the same evaluation stream. Stated here so
//     the two cannot drift.
// V5. THE 4x4 MOVING MASK IS PRODUCED, NEVER CONSUMED. `moving_mask_o` marks
//     the subpatches holding a vertex with non-zero velocity, by the SAME
//     rule TERRAIN.PATCH's dirty mask uses (a border vertex marks both
//     neighbours, a corner four — `zref::terrain::subpatch_mask`).
//     REJECTED, AND THIS ONE MATTERS: gating the sweep by TERRAIN.PATCH's
//     INCOMING dirty mask, to skip clean subpatches under a moving wake.
//     It would be wrong. `dirty` is `live_top != fx(base)` — DISPLACEMENT —
//     and velocity is the DCURVE derivative of the same envelope, so the two
//     are out of phase by construction: a wave's leading edge has velocity
//     with no displacement yet (not dirty, but moving) and its crest has
//     displacement with zero velocity (dirty, but not moving). A dirty-gated
//     sweep would drop exactly the leading edge of every wake. The mask is
//     therefore emitted for a consumer to UNION with, never used as a filter
//     here.
//
// ---------------------------------------------------------------------------
// WIDTHS AND SIGNEDNESS, STATED RATHER THAN ASSUMED
// ---------------------------------------------------------------------------
// The chain is fx16 (signed 32) throughout; each add is done at 33 bits and
// narrowed with the §3 saturate, one add at a time — never a wide accumulate
// then one narrow, which would silently disagree with the reference wherever a
// partial sum leaves the word. The bake-back is `(acc + 128) >>> 8` at 33 bits
// (bits [32:8] ARE the arithmetic shift) then a saturating narrow to s16.
// Every comparison is between two signed operands with explicit `$signed`
// where a concatenation or part-select would otherwise go unsigned: that trap
// cost 29 vanished tiles in GEOM.BINNER and a wholly breached island in
// TERRAIN.BAKE.
//
// NOT IN THIS BLOCK, deliberately: no field evaluation (§4.1 forbids a second
// evaluator), no §4.3 interpolation (`column_query`'s, consumer-side), no
// footprint rectangle store (TERRAIN.PATCH owns the §9.1 list; duplicating it
// would be a second implementation of one law and 2,048 flops), no clamp at
// the underside (§3.4's clamps are about a SURFACE; a rate has no such law and
// inventing one would silently zero the downward half of every wave), no VRAM
// port and no lattice-sized buffer (the block is a stream processor; the
// 2 B/vertex §4.2 store belongs to whoever owns the VRAM page).
//
// Conservative SystemVerilog subset only (charter §2). No function-call result
// is indexed anywhere in this file: Verilator accepts `f(x)[7:0]`, Quartus
// 17.0 rejects it outright, and it cost GEOM.BINNER a synthesis failure that
// every simulation lane passed.

module zhao_terrain_velocity (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // patch start: one sweep per patch per frame (§4.2 "once per frame")
    // -----------------------------------------------------------------------
    input  logic        start_valid_i,
    output logic        start_ready_o,
    input  logic [ 4:0] start_lanes_i,     // accepted list size, 0..16 (§9.1)
    input  logic [15:0] start_patch_id_i,
    input  logic [15:0] start_src_id_i,
    output logic [15:0] trace_patch_id_o,  // the patch under sweep

    // -----------------------------------------------------------------------
    // the velocity lane stream (chosen V3/V4): the block drives the address,
    // the producer answers with `lanes` words for that vertex, in list order.
    // -----------------------------------------------------------------------
    output logic        [ 5:0] vtx_vi_o,          // lattice column, 0..32
    output logic        [ 5:0] vtx_vj_o,          // lattice row, 0..32
    input  logic               lane_valid_i,
    output logic               lane_ready_o,
    input  logic signed [31:0] lane_velocity_i,   // earth out-lane 1, fx16 raw
    input  logic               lane_covers_i,     // §9.1 closed-interval answer

    // -----------------------------------------------------------------------
    // height_velocity out: one §4.2 lattice word per vertex
    // -----------------------------------------------------------------------
    output logic               vv_valid_o,
    input  logic               vv_ready_i,
    output logic signed [15:0] vv_velocity_o,  // height16, the stored word
    output logic        [ 5:0] vv_vi_o,
    output logic        [ 5:0] vv_vj_o,
    output logic               vv_moving_o,    // the word is non-zero
    output logic               vv_covered_o,   // some lane's footprint hit
    output logic        [15:0] vv_src_id_o,

    // -----------------------------------------------------------------------
    // status, counters
    // -----------------------------------------------------------------------
    output logic [15:0] moving_mask_o,  // 4x4 subpatch mask, bit row*4 + col
    output logic        patch_done_o,   // 1-cycle pulse: the lattice is complete
    output logic [31:0] terrain_samples_evaluated_o,
    output logic [31:0] velocity_add_sats_o,      // SatLedger::add
    output logic [31:0] velocity_rescale_sats_o,  // SatLedger::rescale
    output logic        idle_o
);

  // ---- frozen constants ----------------------------------------------------
  localparam logic [5:0] LatMax = 6'd32;  // Island Patch v1: 33x33 (§2)

  // ---- states --------------------------------------------------------------
  localparam logic [1:0] StIdle = 2'd0;
  localparam logic [1:0] StZero = 2'd1;  // lanes == 0: emit the V2 zero word
  localparam logic [1:0] StLane = 2'd2;  // consuming this vertex's lane words
  logic [1:0] state;

  // ---- functions -----------------------------------------------------------

  // §3 saturating fx16 add: ONE add at 33 bits, then narrow with saturation.
  function automatic logic signed [31:0] fx_add_sat(input logic signed [31:0] a,
                                                    input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = $signed({a[31], a}) + $signed({b[31], b});
      if (s > 33'sd2147483647) fx_add_sat = 32'sh7FFF_FFFF;
      else if (s < -33'sd2147483648) fx_add_sat = 32'sh8000_0000;
      else fx_add_sat = s[31:0];
    end
  endfunction

  // Did that add saturate? Split out rather than returned alongside, because
  // indexing a function's return value is exactly what Quartus 17.0 rejects.
  function automatic logic fx_add_sat_fired(input logic signed [31:0] a,
                                            input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = $signed({a[31], a}) + $signed({b[31], b});
      fx_add_sat_fired = (s > 33'sd2147483647) || (s < -33'sd2147483648);
    end
  endfunction

  // The 4x4 subpatch mask of one lattice vertex — `zref::terrain::subpatch_mask`
  // verbatim (charter §11.1's sixteen 8x8-cell subpatches). A vertex on a
  // subpatch border marks BOTH neighbours; a corner vertex marks four.
  function automatic logic [15:0] sp_mask(input logic [5:0] vi, input logic [5:0] vj);
    logic [5:0] col_lo, col_hi, row_lo, row_hi;
    logic [15:0] m;
    begin
      col_lo = (vi == 6'd0) ? 6'd0 : ((vi - 6'd1) >> 3);
      col_hi = ((vi >> 3) > 6'd3) ? 6'd3 : (vi >> 3);
      row_lo = (vj == 6'd0) ? 6'd0 : ((vj - 6'd1) >> 3);
      row_hi = ((vj >> 3) > 6'd3) ? 6'd3 : (vj >> 3);
      m = 16'd0;
      for (int r = 0; r < 4; r++) begin
        for (int c = 0; c < 4; c++) begin
          if (r >= int'(row_lo) && r <= int'(row_hi) && c >= int'(col_lo) && c <= int'(col_hi))
            m[r*4+c] = 1'b1;
        end
      end
      sp_mask = m;
    end
  endfunction

  // -------------------------------------------------------------------------
  // the held sweep
  // -------------------------------------------------------------------------
  logic        [ 4:0] c_lanes;  // 0..16, stable for the whole patch (§9.1)
  logic        [15:0] c_src;
  logic        [ 5:0] vi, vj;
  logic        [ 4:0] k;        // which lane word this vertex is on
  logic signed [31:0] acc;      // the running fx_add chain (V1)
  logic               any_cov;  // some lane covered this vertex

  // ---- the output register -------------------------------------------------
  logic               r_valid;
  logic signed [15:0] r_vel;
  logic        [ 5:0] r_vi, r_vj;
  logic               r_moving;
  logic               r_covered;
  logic        [15:0] r_src;

  wire out_free = !r_valid || vv_ready_i;

  // ---- the lane fold -------------------------------------------------------
  // A lane whose footprint misses the vertex is CONSUMED and DISCARDED, not
  // added as zero — identical in value and identical in SatLedger records.
  wire signed [31:0] acc_next = lane_covers_i ? fx_add_sat(acc, lane_velocity_i) : acc;
  wire add_sat_now = lane_covers_i && fx_add_sat_fired(acc, lane_velocity_i);
  wire last_lane = (k + 5'd1) == c_lanes;

  // ---- the bake-back (qformats §2/§9, ops.yml FIELD.OUT.VELOCITY) ----------
  // height16 = saturate_s16(rescale(acc, 8)), and `rescale(x,8)` is
  // `(x + 128) >> 8` with an ARITHMETIC shift (round-half-up, ties toward
  // +infinity). Bits [32:8] of the 33-bit sum ARE that shift; no separate
  // shifter, and nothing here can be caught by an unsigned promotion.
  // The value baked back is the vertex's FINAL accumulator, which for the
  // last-lane cycle is `acc_next` and for the zero-lane path is 0 (V2).
  logic signed [31:0] bake_in;
  // bake_rnd[7:0] are the bits the shift DISCARDS — that is what rounding is.
  // Named rather than elided so the round-half-up add is visible as one step.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [32:0] bake_rnd;
  /* verilator lint_on UNUSEDSIGNAL */
  logic signed [24:0] bake_sh;
  logic               bake_hi, bake_lo;
  logic signed [15:0] bake_vel;

  assign bake_in  = (state == StLane) ? acc_next : 32'sd0;
  assign bake_rnd = $signed({bake_in[31], bake_in}) + 33'sd128;
  assign bake_sh  = $signed(bake_rnd[32:8]);
  assign bake_hi  = bake_sh > 25'sd32767;
  assign bake_lo  = bake_sh < -25'sd32768;
  assign bake_vel = bake_hi ? 16'sh7FFF : (bake_lo ? 16'sh8000 : $signed(bake_sh[15:0]));

  wire bake_sat = bake_hi || bake_lo;
  wire last_vtx = (vi == LatMax) && (vj == LatMax);

  // A lane word is taken only when its answer has somewhere to go. On a
  // non-final lane that is always true (the accumulator is a register that is
  // being overwritten anyway); on the final lane the output register must be
  // free. With `c_lanes == 1` EVERY lane is final, so this line is exactly the
  // sustained-rate path: one lane word in, one lattice word out, per clock.
  assign lane_ready_o = (state == StLane) && (!last_lane || out_free);

  assign start_ready_o = (state == StIdle) && !r_valid;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= StIdle;
      c_lanes <= '0;
      c_src <= '0;
      vi <= '0;
      vj <= '0;
      k <= '0;
      acc <= '0;
      any_cov <= 1'b0;
      r_valid <= 1'b0;
      r_vel <= '0;
      r_vi <= '0;
      r_vj <= '0;
      r_moving <= 1'b0;
      r_covered <= 1'b0;
      r_src <= '0;
      moving_mask_o <= '0;
      patch_done_o <= 1'b0;
      trace_patch_id_o <= '0;
      terrain_samples_evaluated_o <= '0;
      velocity_add_sats_o <= '0;
      velocity_rescale_sats_o <= '0;
    end else begin
      patch_done_o <= 1'b0;

      // ---- publish / retire ------------------------------------------------
      if (r_valid && vv_ready_i) r_valid <= 1'b0;

      case (state)
        StIdle: begin
          if (start_valid_i && start_ready_o) begin
            c_lanes <= start_lanes_i;
            c_src <= start_src_id_i;
            trace_patch_id_o <= start_patch_id_i;
            vi <= '0;
            vj <= '0;
            k <= '0;
            acc <= '0;
            any_cov <= 1'b0;
            moving_mask_o <= '0;
            state <= (start_lanes_i == 5'd0) ? StZero : StLane;
          end
        end

        // lanes == 0: no evaluation touches this patch, so every vertex takes
        // the V2 zero word and the sweep runs at one lattice word per clock.
        StZero: begin
          if (out_free) begin
            r_valid <= 1'b1;
            r_vel <= 16'sd0;
            r_vi <= vi;
            r_vj <= vj;
            r_moving <= 1'b0;
            r_covered <= 1'b0;
            r_src <= c_src;
            terrain_samples_evaluated_o <= terrain_samples_evaluated_o + 32'd1;
            if (last_vtx) begin
              state <= StIdle;
              patch_done_o <= 1'b1;
            end else if (vi == LatMax) begin
              vi <= '0;
              vj <= vj + 6'd1;
            end else begin
              vi <= vi + 6'd1;
            end
          end
        end

        StLane: begin
          if (lane_valid_i && lane_ready_o) begin
            if (add_sat_now) velocity_add_sats_o <= velocity_add_sats_o + 32'd1;
            if (last_lane) begin
              r_valid <= 1'b1;
              r_vel <= bake_vel;
              r_vi <= vi;
              r_vj <= vj;
              r_moving <= bake_vel != 16'sd0;
              r_covered <= any_cov || lane_covers_i;
              r_src <= c_src;
              if (bake_sat) velocity_rescale_sats_o <= velocity_rescale_sats_o + 32'd1;
              if (bake_vel != 16'sd0) moving_mask_o <= moving_mask_o | sp_mask(vi, vj);
              terrain_samples_evaluated_o <= terrain_samples_evaluated_o + 32'd1;
              k <= '0;
              acc <= '0;
              any_cov <= 1'b0;
              if (last_vtx) begin
                state <= StIdle;
                patch_done_o <= 1'b1;
              end else if (vi == LatMax) begin
                vi <= '0;
                vj <= vj + 6'd1;
              end else begin
                vi <= vi + 6'd1;
              end
            end else begin
              acc <= acc_next;
              any_cov <= any_cov || lane_covers_i;
              k <= k + 5'd1;
            end
          end
        end

        default: state <= StIdle;
      endcase
    end
  end

  assign vtx_vi_o = vi;
  assign vtx_vj_o = vj;

  assign vv_valid_o = r_valid;
  assign vv_velocity_o = r_vel;
  assign vv_vi_o = r_vi;
  assign vv_vj_o = r_vj;
  assign vv_moving_o = r_moving;
  assign vv_covered_o = r_covered;
  assign vv_src_id_o = r_src;

  assign idle_o = (state == StIdle) && !r_valid;

endmodule : zhao_terrain_velocity
