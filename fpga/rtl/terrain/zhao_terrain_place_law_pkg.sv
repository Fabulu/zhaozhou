// zhao_terrain_place_law_pkg.sv -- TERRAIN.PLACE's ratified arithmetic, in ONE
// place, so that two callers cannot disagree about where a patch is.
//
// Law: spec/terrain_rules.md 2.1
//      design/contracts/TERRAIN.EDGERECON.md  ("the honest shape is ... so
//      PREPARE and EMIT are bit-identical BY CONSTRUCTION rather than by two
//      implementations agreeing")
//      reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt section 2
//
// ===========================================================================
// WHY THIS PACKAGE EXISTS, AND WHY IT IS A PACKAGE RATHER THAN A SECOND BLOCK
// ===========================================================================
// `zhao_terrain_prepwalk` needs a patch's world x/z during the PREPARE pass,
// for a patch that is NOT the one `zhao_terrain_place` currently holds. The
// three candidates TERRAIN.EDGERECON.md lists are all wrong and the contract
// says why:
//
//   (1) a second `zhao_terrain_place` instance -- a DUPLICATE PROVIDER, which
//       ruling R16 retired `zhao_terrain_visible` for being;
//   (2) a cx/cz column on `zhao_terrain_devstore` -- pays a store to carry a
//       number that is two shifts away, and TERRAIN.LODFEED (which writes the
//       deviations) never sees a placement or a patch coordinate anyway;
//   (3) time-multiplexing PLACE's `vtx_*` port -- DEAD, because `hdr_ready_o`
//       is tied high (`zhao_terrain_place.sv:287`) and a header acceptance
//       DISPLACES the latched patch and launches the 66-write `pos_we_o` fill.
//       You cannot ask PLACE about another patch without destroying the patch
//       it is placing.
//
// And the fourth option -- recomputing `wx = (ix*32 + i) <<< (16 + pitch)`
// inside the walker -- is the one the contract refuses BY NAME, because it is
// a second implementation of ratified arithmetic and I27 already refused
// inlining it into a composer.
//
// So the arithmetic moves OUT of `zhao_terrain_place` into this package and
// that block calls it. Nothing about the placement law changes; the functions
// below are `zhao_terrain_place.sv`'s own `place32`, `units_fits` and
// `units_of`, moved verbatim, and that block now has no copy of its own. Two
// callers, ONE definition, and "bit-identical" is a structural property rather
// than a claim somebody has to re-check.
//
// THIS IS NOT A SECOND PROVIDER. A package holds no state, has no ports, costs
// no ALMs by existing, and cannot be "composed" or left disconnected. The
// provider of a PLACED PATCH is still `zhao_terrain_place` and only it: that
// block still owns the header acceptance, the envelope check, the pitch
// refusal, the census counters and the compose-cache fill. What moved is the
// shifter, which was never the provider.
//
// Conservative SystemVerilog subset only (charter section 2).
`ifndef ZHAO_TERRAIN_PLACE_LAW_PKG_SV
`define ZHAO_TERRAIN_PLACE_LAW_PKG_SV

package zhao_terrain_place_law_pkg;

  // ==========================================================================
  // THE FOUR LEGAL PITCHES.
  // ==========================================================================
  // shift = 16 + pitch_log2, so {-1,0,1,2} -> {15,16,17,18}. Held as four
  // constants selected by a 2-bit code rather than computed, so no barrel
  // shifter appears in any caller.
  localparam int unsigned SH_HALF = 15;  // pitch_log2 = -1, 0.5 m
  localparam int unsigned SH_ONE  = 16;  // pitch_log2 =  0, 1.0 m
  localparam int unsigned SH_TWO  = 17;  // pitch_log2 = +1, 2.0 m  (canonical)
  localparam int unsigned SH_FOUR = 18;  // pitch_log2 = +2, 4.0 m

  // `units` is (patch_coord * 32 + index) -- the lattice vertex counted in
  // CELLS from the island datum. patch_coord is s16 and the index is 0..32, so
  // units needs 16 + 5 + 1 = 22 bits signed.
  localparam int unsigned UNITS_W = 22;

  // ---- is this pitch one of the four the spec admits? ----------------------
  // Hoisted out of `zhao_terrain_place`'s `pitch_ok_c` so that a caller which
  // takes a pitch from the ISLAND DESCRIPTOR rather than from a page header
  // applies the identical test. Owner directive section 2 makes the island's
  // pitch authoritative for every page of that generation, which means the
  // legality test is now asked in two places and must be one function.
  function automatic logic pitch_legal(input logic signed [7:0] p);
    begin
      pitch_legal = (p >= -8'sd1) && (p <= 8'sd2);
    end
  endfunction

  // ==========================================================================
  // THE PLACEMENT SHIFTER.
  // ==========================================================================
  // The shift is done at the OUTPUT width, 32 bits, and never wider. That is
  // exact for every placement `units_fits` admits, and it is the reason no
  // 41-bit intermediate appears anywhere: carrying one would mean nine bits
  // that exist only to be discarded, which reads to a linter -- correctly --
  // as logic with no consumer.
  function automatic logic signed [31:0] place32
      (input logic signed [UNITS_W-1:0] units, input logic signed [7:0] p);
    logic signed [31:0] u;
    begin
      u = 32'(units);
      // Four constants, selected. NOT `u <<< (16 + p)`; see the header of
      // zhao_terrain_place.sv, which explains why a barrel shifter is refused.
      case (p)
        -8'sd1:  place32 = u <<< SH_HALF;
         8'sd0:  place32 = u <<< SH_ONE;
         8'sd1:  place32 = u <<< SH_TWO;
         8'sd2:  place32 = u <<< SH_FOUR;
        default: place32 = '0;   // refused upstream; never placed
      endcase
    end
  endfunction

  // RANGE, DECIDED ON THE OPERAND RATHER THAN THE RESULT.
  //
  // `units <<< sh` is exact in s32 exactly when `units` fits in 32 - sh bits
  // signed, i.e. -2**(31-sh) <= units < 2**(31-sh). Testing the input this way
  // costs one comparison against a constant; testing the output would need the
  // wide shift this arithmetic otherwise never performs, so the cheap test and
  // the honest test are the same test here.
  function automatic logic units_fits
      (input logic signed [UNITS_W-1:0] units, input logic signed [7:0] p);
    logic signed [UNITS_W-1:0] lim;
    begin
      case (p)
        -8'sd1:  lim = UNITS_W'(1) <<< (31 - SH_HALF);   // 2**16
         8'sd0:  lim = UNITS_W'(1) <<< (31 - SH_ONE);    // 2**15
         8'sd1:  lim = UNITS_W'(1) <<< (31 - SH_TWO);    // 2**14
         8'sd2:  lim = UNITS_W'(1) <<< (31 - SH_FOUR);   // 2**13
        default: lim = UNITS_W'(0);
      endcase
      units_fits = (units < lim) && (units >= -lim);
    end
  endfunction

  // ---- the two placements any caller must answer ---------------------------
  // Column x for lattice index i, row z for lattice index j.
  function automatic logic signed [UNITS_W-1:0] units_of
      (input logic signed [15:0] coord, input logic [5:0] idx);
    begin
      units_of = UNITS_W'(coord) * UNITS_W'(32) + UNITS_W'({1'b0, idx});
    end
  endfunction

endpackage

`endif
