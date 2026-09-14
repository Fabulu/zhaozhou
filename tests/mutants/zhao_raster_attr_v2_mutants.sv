// zhao_raster_attr_v2_mutants.sv -- COMMITTED, DELIBERATELY BROKEN controls.
//
// This file declares no module and is absent from production source lists.  A
// private inverse build compiles it immediately before the exact production RTL;
// each selector changes one arithmetic leaf in the real modules, with no copied
// divider or row-walker implementation that could drift.
`default_nettype none

// Restore the retained candidate's negative exact-half behaviour.  For negative
// n, floor((n + floor((area-1)/2))/area) differs from current rast.cpp only when
// an even-area quotient is exactly halfway: it moves that tie away from zero.
`ifdef ZHAO_ATTR_V2_MUTANT_NEG_HALF
  `define ZHAO_ATTR_V2_ROUND_NUM(num, area_ext) \
      (($signed(num) < 0) \
          ? ($signed(num) + (($signed(area_ext) - 97'sd1) >>> 1)) \
          : ($signed(num) + ($signed(area_ext) >>> 1)))
`endif

// Omit the entire global-min-X tile offset, grad_x*(tile_x-min_x), after the
// row quotient is computed at min_x.  This is NOT a fresh tile-edge divide: it
// deliberately leaves the min_x row quotient unadvanced into the current tile.
`ifdef ZHAO_ATTR_V2_MUTANT_OMIT_MIN_X_ACCUM
  `define ZHAO_ATTR_V2_TILE_SEED(row_q, tile_offset) \
      ($signed(row_q) + ($signed(tile_offset) & 46'sd0))
`endif

`default_nettype wire
