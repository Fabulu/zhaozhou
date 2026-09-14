// zhao_raster_texture_stage_v3_mutants.sv
//
// COMMITTED, DELIBERATELY BROKEN Packet-C selector hooks.  NOT SHIPPED.
// This file declares no module and is absent from production source lists.  A
// private inverse build compiles it immediately before the exact production
// zhao_raster_texture_stage_v3.sv.  The selected macro changes one seam in that
// real stage; there is no copied stage implementation to drift.
`default_nettype none

// Identity-only positive control.  Corrupt exactly returned raster sequence 1;
// the remaining 128 retirement bits and all 48 result bits stay untouched.  The
// ordinary stage must detect it, drop that output plus every later ordered
// output, and reach finite texture quiet.
`ifdef ZHAO_PACKET_C_MUTANT_IDENTITY_ONLY
  `define ZHAO_PACKET_C_RETURNED_SEQUENCE(sequence) \
      (((sequence) == 32'd1) ? ((sequence) ^ 32'h0000_0001) : (sequence))
`endif

// Superseded match-gated ready law, combined with the same one-sided identity
// corruption.  Once sequence 1 reaches the ordered head, ready remains false;
// the owner cannot release, quiet cannot arrive, and synthetic RELEASE must not
// be claimed.  This is the precise deadlock Packet C's always-drain law removes.
`ifdef ZHAO_PACKET_C_MUTANT_OLD_READY
  `define ZHAO_PACKET_C_RETURNED_SEQUENCE(sequence) \
      (((sequence) == 32'd1) ? ((sequence) ^ 32'h0000_0001) : (sequence))
  `define ZHAO_PACKET_C_V3_OUT_READY(aborting, mismatch, fragment_ready, match) \
      ((fragment_ready) && (match))
`endif

`default_nettype wire
