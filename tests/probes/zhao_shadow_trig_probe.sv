// zhao_shadow_trig_probe -- DSPHUNT's POSITIVE CONTROL for the 2026-09-26
// unit-circle table narrowing in `zhao_forge_shadow`.
//
// WHAT THIS FILE IS
// -----------------
// It is `fpga/rtl/forge/zhao_forge_shadow.sv` AS IT STOOD AT COMMIT 5049399c,
// with the module renamed and nothing else altered. It was produced
// MECHANICALLY, by `git show` and a single identifier substitution, precisely
// so that it is the old arithmetic rather than somebody's reading of it. A
// C++ restatement of the old table would prove only that the DUT agrees with
// MY TRANSCRIPTION -- which is the part most likely to be wrong, being written
// by the same person at the same time from the same reading.
//
// WHAT CHANGED IN PRODUCTION. `unit_cos` and `unit_sin` declared
// `logic signed [31:0]` while every one of their sixteen values lies in
// [-65536, +65536]. Production now declares them `logic signed [17:0]`, whose
// range is [-131072, +131071] -- so no returned integer moves. They feed
// `$signed(rad_q) * unit_cos(tbl_c)` into a 64-bit wire, which was therefore a
// 32x32 multiply and is now 32x18.
//
// WHAT IT MEASURED -- quartus_map 17.0.2, 5CSEBA6U23I7, -MapOnly:
//
//   row                              DSP  9x9  18x18pr  sum2  +36  reg  ALUT
//   zhao_forge_shadow@gzdsp-base       7    1        4     2    0  733   510
//   zhao_forge_shadow@gzdsp-tab18      5    1        2     0    2  733   450
//
// -2 DSP and -60 ALUTs, register count UNCHANGED at 733. NOTE THERE WAS NEVER
// AN `Independent 27x27` IN THIS BLOCK -- the brief's sorting column would
// have skipped it entirely, and it paid anyway.
//
// *** DO NOT "REFRESH" THIS FILE AGAINST PRODUCTION. ***
//
// The usual law for a committed copy is the opposite of what applies here. A
// mutant copy goes stale in the flattering direction and must be merged
// forward, and `tools/budget/mutant_copy_drift.py` exists to catch that. This
// file is NOT in its scope (it scans tests/mutants/) and must not be brought
// into it. It is the BASELINE the -2 DSP was measured against, and
// `tests/proofs/forge_shadow_trig_differential.cpp` verilates it as
// `Vshadow_old` against shipping production -- refresh it and that becomes a
// comparison of the new block WITH ITSELF, green forever, proving nothing.
//
// This file is a PROBE, not production. Nothing composes it and nothing may.
// ---------------------------------------------------------------------------

// zhao_shadow_trig_probe.sv -- FORGE.SHADOW: contact shadows, as ordinary geometry.
//
// design/contracts/FORGE.SHADOW.md, written 2026-09-03 from
// BORING_3D_FUNDAMENTALS_AUDIT.md R8. Full shadow maps are deliberately absent
// from this console and that is defensible; what was missing is any settled
// replacement for the most basic visual job there is. For a game of floating
// islands and airborne creatures, "is that thing touching the ground or hovering
// above it" is not a refinement, it is LEGIBILITY.
//
// The audit's estimate is the reason it is worth doing: "very small if done as
// geometry", and "can make more perceptual difference than several expensive
// material effects."
//
// ---------------------------------------------------------------------------
// THE EXCLUSIONS ARE THE DESIGN
// ---------------------------------------------------------------------------
// No shadow map. No depth pass from the light, no shadow buffer, no second
// view. No new framebuffer and no new raster hardware -- the output is ordinary
// transparent geometry through the main renderer. No shadow UNIT: this is a
// primitive generator, a sibling of FORGE.PRIM, not a lighting stage. No
// self-shadowing, none cast onto other creatures, none from terrain onto
// terrain -- CONTACT ONLY. And no occlusion query: whether the ground is really
// below is answered by a few height taps, not by visibility.
//
// ---------------------------------------------------------------------------
// THE FROZEN LADDER, and it is a COARSENESS FLOOR the governor selects
// ---------------------------------------------------------------------------
//   near hero  | projected hull -- a 16-vertex ellipse conformed from taps
//   near army  | 8-vertex blob
//   mid        | tiny dark splat (4 vertices)
//   far        | none
//
// Exactly as PART.LADDER treats particle representation: the same idea and the
// same refusal to let a distant creature spend near-hero geometry. The governor
// may only make a rung COARSER, never finer, which is why `rung_floor_i` is a
// floor and the comparison below is a max rather than an override.
//
// ONE UNIT-CIRCLE TABLE SERVES ALL THREE RUNGS. The 16 entries are the near-hero
// ellipse; the 8-vertex blob takes every second entry and the 4-vertex splat
// every fourth. Three tables would be three chances for them to disagree about
// where vertex zero is, and the emission ORDER is part of the contract because
// "two orderings produce the same picture and different capture CRCs".
//
// ---------------------------------------------------------------------------
// THE ONE THING THAT MUST NOT BE GOT WRONG, in the contract's own words
// ---------------------------------------------------------------------------
// "Depth bias, and it must be authored rather than accidental." CLAUDE.md's
// ground-contact law applies directly: clipping through the ground must be
// authored, never accidental, and a belly resting at exactly zero reads as
// hovering. A shadow at exactly the terrain height z-fights with it; a shadow
// biased too far reads as floating detached from its caster.
//
// So the bias is a NAMED, EDITABLE CONSTANT PER RUNG -- the four parameters
// below -- and its correctness is decided BY LOOKING. A shadow that measures
// right and looks detached is wrong, and no number in this file settles that.
// This is art (CLAUDE.md), and these parameters exist so the owner can move it.
//
// ---------------------------------------------------------------------------
// REFUSALS ARE CORRECT BEHAVIOUR HERE, NOT FAULTS
// ---------------------------------------------------------------------------
// A caster with no ground beneath it -- over a void cell, off the island edge,
// above the keel -- emits NO SHADOW, and the contract is explicit that this is
// correct rather than a fault: "an airborne creature over a chasm should not
// have a shadow pasted at some default height." A radius of zero emits nothing.
// Height taps that disagree wildly (a cliff edge) still conform; the hull
// follows them and may look odd on a knife-edge, "and that is a content problem,
// not a hardware refusal."
//
// The counters below therefore separate REFUSALS THAT ARE CORRECT (void, zero
// radius, far rung) from ONE THAT IS NOT (a tap stream that disagreed with the
// rung's declared fixed count). Counting them together would make a healthy
// scene look broken and a broken one look busy.

`default_nettype none

module zhao_shadow_trig_probe
  import zhao_pkg::*;
#(
    // Vertices per rung. spec: "8-16 vertex ellipse" near hero, "4-8 vertex
    // blob" near army, "tiny dark splat" mid.
    parameter int unsigned VTX_HERO  = 16,
    parameter int unsigned VTX_ARMY  = 8,
    parameter int unsigned VTX_MID   = 4,

    // -----------------------------------------------------------------------
    // THE DEPTH BIAS, PER RUNG, IN fx16 METRES. AUTHORED, AND MEANT TO BE MOVED.
    // -----------------------------------------------------------------------
    // These are the knobs the contract demands exist. They are NOT derived from
    // anything and must not become derived: CLAUDE.md, rule 6 -- "never remove
    // the owner's control in the name of fidelity ... 'this is generated from
    // the reference, so it is not a knob' is how a wrong number becomes an
    // unadjustable wrong number."
    //
    // Starting values: 0.02 m at the near rungs, growing with coarseness because
    // a coarser hull follows the ground less closely and needs more clearance to
    // avoid z-fighting on a slope. 1311 raw = 0.0200 m; 2621 = 0.0400 m.
    // CHOSEN BY REASONING AND NOT YET BY LOOKING -- the contract's look-gate (a
    // creature walking across flat ground, a slope, a cliff edge and a breach,
    // at 240p, in motion) has not been run, and until it has, these are a
    // starting point rather than a result.
    parameter int signed BIAS_HERO = 32'sd1311,
    parameter int signed BIAS_ARMY = 32'sd1966,
    parameter int signed BIAS_MID  = 32'sd2621,

    parameter int unsigned CENSUS_W = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // -----------------------------------------------------------------------
    // one shadow caster
    // -----------------------------------------------------------------------
    input  var logic               cast_valid_i,
    output var logic               cast_ready_o,
    input  var logic signed [31:0] cast_x_i,       // world x, fx16
    input  var logic signed [31:0] cast_z_i,       // world z, fx16
    input  var logic signed [31:0] cast_radius_i,  // world metres, fx16
    input  var logic        [ 7:0] cast_strength_i,// unit8: value = raw/256
    input  var logic        [ 1:0] cast_rung_i,    // 0 hero 1 army 2 mid 3 far
    input  var logic        [15:0] cast_src_id_i,

    // The governor's COARSENESS FLOOR. It may only make a rung coarser; a
    // governor that could refine would let a distant creature spend near-hero
    // geometry, which is the whole thing the ladder refuses.
    input  var logic        [ 1:0] rung_floor_i,

    // -----------------------------------------------------------------------
    // terrain height taps -- a FIXED COUNT PER RUNG, so the cost is bounded and
    // knowable rather than dependent on terrain roughness.
    // -----------------------------------------------------------------------
    output var logic               tap_req_valid_o,
    input  var logic               tap_req_ready_i,
    output var logic signed [31:0] tap_x_o,
    output var logic signed [31:0] tap_z_o,
    input  var logic               tap_rsp_valid_i,
    input  var logic signed [31:0] tap_height_i,
    input  var logic               tap_no_ground_i,  // void cell / off the edge

    // -----------------------------------------------------------------------
    // the vertex stream to GEOM.SETUP, in a DECLARED DETERMINISTIC ORDER
    // -----------------------------------------------------------------------
    output var logic               vtx_valid_o,
    input  var logic               vtx_ready_i,
    output var logic signed [31:0] vtx_x_o,
    output var logic signed [31:0] vtx_y_o,
    output var logic signed [31:0] vtx_z_o,
    output var logic        [ 7:0] vtx_alpha_o,     // unit8, from cast_strength
    output var logic               vtx_last_o,      // final vertex of this hull
    output var logic        [15:0] vtx_src_id_o,
    // WHICH RUNG ACTUALLY EMITTED, after the governor's floor was applied. It is
    // an output rather than an internal latch because the ladder's decision is
    // the thing a look-gate argues about: "the shadow is too coarse here" is
    // answerable from this and unanswerable without it.
    output var logic        [ 1:0] vtx_rung_o,

    // -----------------------------------------------------------------------
    // census. SATURATING, and split by whether the refusal is CORRECT.
    // -----------------------------------------------------------------------
    output var logic [CENSUS_W-1:0] shadows_emitted_o,
    output var logic [CENSUS_W-1:0] no_ground_o,      // correct: over a void
    output var logic [CENSUS_W-1:0] zero_radius_o,    // correct: nothing to draw
    output var logic [CENSUS_W-1:0] far_rung_o,       // correct: ladder says none
    output var logic [CENSUS_W-1:0] tap_protocol_o    // NOT correct: a real fault
);

  initial begin
    if (VTX_HERO != 16 || VTX_ARMY != 8 || VTX_MID != 4) begin
      $fatal(1, "zhao_shadow_trig_probe: the ladder's vertex counts are 16/8/4; one unit-circle table serves all three by stride");
    end
    if (CENSUS_W < 1) begin
      $fatal(1, "zhao_shadow_trig_probe: CENSUS_W must be positive");
    end
  end

  localparam logic [1:0] R_HERO = 2'd0;
  localparam logic [1:0] R_ARMY = 2'd1;
  localparam logic [1:0] R_MID  = 2'd2;
  localparam logic [1:0] R_FAR  = 2'd3;

  // ==========================================================================
  // THE UNIT CIRCLE, 16 ENTRIES, fx16. ONE TABLE, THREE RUNGS, BY STRIDE.
  // ==========================================================================
  // cos and sin of k*2pi/16 at fx16 (x65536), rounded half-up. Written out
  // rather than computed so the values are auditable and so nothing in the
  // synthesised design contains a trig evaluation.
  //   65536, 60547, 46341, 25080, 0, -25080, -46341, -60547, ...
  function automatic logic signed [31:0] unit_cos(input logic [3:0] k);
    begin
      case (k)
        4'd0:  unit_cos =  32'sd65536;
        4'd1:  unit_cos =  32'sd60547;
        4'd2:  unit_cos =  32'sd46341;
        4'd3:  unit_cos =  32'sd25080;
        4'd4:  unit_cos =  32'sd0;
        4'd5:  unit_cos = -32'sd25080;
        4'd6:  unit_cos = -32'sd46341;
        4'd7:  unit_cos = -32'sd60547;
        4'd8:  unit_cos = -32'sd65536;
        4'd9:  unit_cos = -32'sd60547;
        4'd10: unit_cos = -32'sd46341;
        4'd11: unit_cos = -32'sd25080;
        4'd12: unit_cos =  32'sd0;
        4'd13: unit_cos =  32'sd25080;
        4'd14: unit_cos =  32'sd46341;
        default: unit_cos = 32'sd60547;
      endcase
    end
  endfunction

  // sin(k) = cos(k - 4), i.e. the same table rotated by a quarter turn. Deriving
  // it rather than tabulating it twice is not cleverness: two tables are two
  // chances to disagree about where vertex zero is, and the emission order is
  // part of the contract.
  function automatic logic signed [31:0] unit_sin(input logic [3:0] k);
    begin
      unit_sin = unit_cos(k - 4'd4);
    end
  endfunction

  // How many vertices this rung emits, and the table stride that gives them.
  function automatic logic [4:0] rung_vtx(input logic [1:0] r);
    begin
      case (r)
        R_HERO:  rung_vtx = 5'(VTX_HERO);
        R_ARMY:  rung_vtx = 5'(VTX_ARMY);
        R_MID:   rung_vtx = 5'(VTX_MID);
        default: rung_vtx = 5'd0;
      endcase
    end
  endfunction

  function automatic logic [3:0] rung_stride(input logic [1:0] r);
    begin
      case (r)
        R_HERO:  rung_stride = 4'd1;
        R_ARMY:  rung_stride = 4'd2;
        R_MID:   rung_stride = 4'd4;
        default: rung_stride = 4'd1;
      endcase
    end
  endfunction

  function automatic logic signed [31:0] rung_bias(input logic [1:0] r);
    begin
      case (r)
        R_HERO:  rung_bias = BIAS_HERO;
        R_ARMY:  rung_bias = BIAS_ARMY;
        default: rung_bias = BIAS_MID;
      endcase
    end
  endfunction

  // THE FLOOR IS A MAX, NOT AN OVERRIDE. A higher code is a coarser rung, so the
  // governor can only ever push the number up.
  wire [1:0] rung_c = (cast_rung_i > rung_floor_i) ? cast_rung_i : rung_floor_i;

  // ==========================================================================
  // STATE
  // ==========================================================================
  localparam logic [2:0] S_IDLE  = 3'd0;
  localparam logic [2:0] S_TAP   = 3'd1;   // request one tap
  localparam logic [2:0] S_WAIT  = 3'd2;   // await its height
  localparam logic [2:0] S_EMIT  = 3'd3;   // stream the hull
  localparam logic [2:0] S_DROP  = 3'd4;   // refused: emit nothing, count it

  logic [2:0]  st_q;
  logic [1:0]  rung_q;
  logic [4:0]  n_q;        // vertices this hull
  logic [3:0]  stride_q;
  logic [4:0]  k_q;        // which vertex / which tap
  logic signed [31:0] cx_q, cz_q, rad_q;
  logic [7:0]  strength_q;
  logic [15:0] src_q;
  logic signed [31:0] bias_q;
  // One height per vertex, conformed. 16 is the widest rung.
  logic signed [31:0] h_m [0:15];

  // The vertex being offered: centre + radius * unit circle, height + bias.
  wire [3:0] tbl_c = 4'(k_q) * stride_q;
  // radius * unit  -- fx16 * fx16 >> 16. One multiplier, shared across x and z
  // by the state machine offering one vertex at a time.
  wire signed [63:0] off_x_w = $signed(rad_q) * unit_cos(tbl_c);
  wire signed [63:0] off_z_w = $signed(rad_q) * unit_sin(tbl_c);

  assign cast_ready_o = (st_q == S_IDLE);

  assign tap_req_valid_o = (st_q == S_TAP);
  assign tap_x_o = cx_q + 32'(off_x_w >>> 16);
  assign tap_z_o = cz_q + 32'(off_z_w >>> 16);

  assign vtx_valid_o  = (st_q == S_EMIT);
  assign vtx_x_o      = cx_q + 32'(off_x_w >>> 16);
  assign vtx_z_o      = cz_q + 32'(off_z_w >>> 16);
  assign vtx_y_o      = h_m[k_q[3:0]] + bias_q;
  assign vtx_alpha_o  = strength_q;
  assign vtx_last_o   = (st_q == S_EMIT) && (k_q + 5'd1 == n_q);
  assign vtx_src_id_o = src_q;
  assign vtx_rung_o   = rung_q;

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q       <= S_IDLE;
      rung_q     <= R_FAR;
      n_q        <= '0;
      stride_q   <= 4'd1;
      k_q        <= '0;
      cx_q       <= '0;
      cz_q       <= '0;
      rad_q      <= '0;
      strength_q <= '0;
      src_q      <= '0;
      bias_q     <= '0;
      for (i = 0; i < 16; i = i + 1) h_m[i] <= '0;
      shadows_emitted_o <= '0;
      no_ground_o       <= '0;
      zero_radius_o     <= '0;
      far_rung_o        <= '0;
      tap_protocol_o    <= '0;
    end else begin
      case (st_q)
        S_IDLE: begin
          if (cast_valid_i) begin
            cx_q       <= cast_x_i;
            cz_q       <= cast_z_i;
            rad_q      <= cast_radius_i;
            strength_q <= cast_strength_i;
            src_q      <= cast_src_id_i;
            rung_q     <= rung_c;
            n_q        <= rung_vtx(rung_c);
            stride_q   <= rung_stride(rung_c);
            bias_q     <= rung_bias(rung_c);
            k_q        <= '0;
                  // THE THREE CORRECT REFUSALS, each counted on its own terms.
            if (rung_c == R_FAR) begin
              if (!(&far_rung_o)) far_rung_o <= far_rung_o + 1'b1;
              st_q <= S_DROP;
            end else if (cast_radius_i == 32'sd0) begin
              if (!(&zero_radius_o)) zero_radius_o <= zero_radius_o + 1'b1;
              st_q <= S_DROP;
            end else begin
              st_q <= S_TAP;
            end
          end
        end

        S_TAP: begin
          if (tap_req_ready_i) st_q <= S_WAIT;
        end

        S_WAIT: begin
          if (tap_rsp_valid_i) begin
            if (tap_no_ground_i) begin
              // NO GROUND BENEATH THE CASTER. Correct, not a fault: "an
              // airborne creature over a chasm should not have a shadow pasted
              // at some default height." One void tap voids the whole hull --
              // a partial hull would be a shadow with a bite out of it, which
              // reads as a rendering bug rather than as a chasm.
              if (!(&no_ground_o)) no_ground_o <= no_ground_o + 1'b1;
              st_q <= S_DROP;
            end else begin
              h_m[k_q[3:0]] <= tap_height_i;
              if (k_q + 5'd1 == n_q) begin
                k_q  <= '0;
                st_q <= S_EMIT;
              end else begin
                k_q  <= k_q + 5'd1;
                st_q <= S_TAP;
              end
            end
          end
        end

        S_EMIT: begin
          if (vtx_ready_i) begin
            if (k_q + 5'd1 == n_q) begin
              if (!(&shadows_emitted_o)) shadows_emitted_o <= shadows_emitted_o + 1'b1;
              k_q  <= '0;
              st_q <= S_IDLE;
            end else begin
              k_q <= k_q + 5'd1;
            end
          end
        end

        default: begin  // S_DROP -- one cycle, emits nothing
          st_q <= S_IDLE;
        end
      endcase

      // A TAP RESPONSE NOBODY ASKED FOR is the one refusal here that is a real
      // fault, and it is counted apart from the three correct ones on purpose:
      // lumping them together would make a scene full of legitimate chasms look
      // broken, and a genuinely broken tap path look busy.
      if (tap_rsp_valid_i && (st_q != S_WAIT)) begin
        if (!(&tap_protocol_o)) tap_protocol_o <= tap_protocol_o + 1'b1;
      end
    end
  end

endmodule

`default_nettype wire
