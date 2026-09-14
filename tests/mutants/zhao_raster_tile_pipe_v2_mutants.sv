// zhao_raster_tile_pipe_v2_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-D selector hooks.  NOT SHIPPED.
// This file declares no module and is absent from production source lists.  Each
// private inverse build compiles it before the exact tile RTL.  Packet-C identity
// and old-ready controls are not copied here: the same build uses
// zhao_raster_texture_stage_v3_mutants.sv followed by the exact Packet-C source.
`default_nettype none

// The private directed wrapper enables handshake-only pause pins in the exact
// product modules.  They add no storage or alternate datapath and do not exist
// when the product source is compiled without this test-only file.
`define ZHAO_PACKET_D_TEST_HOOKS

// Corrupt only lane 1's joined column identity at column 3.  Columns 0/1 first
// fill the real skid and column 2 occupies Early-Z under the private stage stall;
// the column-3 fault therefore proves simultaneous attr+Early-Z drop accounting.
`ifdef ZHAO_PACKET_D_MUTANT_COORDINATE
  `define ZHAO_PACKET_D_LANE1_COL(col) \
      (((col) == 4'd3) ? ((col) ^ 4'b0001) : (col))
`endif

// Delete only Packet-C/V3 quiet from ordinary pipe_empty.  A delayed texture
// return must then permit an observably early swap/resolve.
`ifdef ZHAO_PACKET_D_MUTANT_OMIT_V3_QUIET
  `define ZHAO_PACKET_D_PIPE_V3_QUIET(quiet) 1'b1
`endif

// Restore the superseded downstream-ready-only skid law.  Combined with Packet
// C's identity mutant, stage ready drops on the mismatch edge and the occupied
// skid head cannot be synchronously cancelled, so complete quiet is unreachable.
`ifdef ZHAO_PACKET_D_MUTANT_SKIP_CANCEL
  `define ZHAO_PACKET_D_SKID_DN_READY(aborting, stage_ready) (stage_ready)
`endif

`default_nettype wire
