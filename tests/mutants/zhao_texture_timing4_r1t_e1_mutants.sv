// zhao_texture_timing4_r1t_e1_mutants.sv -- committed inverse controls for the
// Timing4 R1T and E1 read boundaries.
//
// COMMITTED, DELIBERATELY BROKEN. This shim declares no module. A private
// inverse build compiles it immediately BEFORE the production source it
// mutates and defines exactly one selector; production never sees it.
//
// ===========================================================================
// STATUS: NEITHER MUTANT HAS EVER BEEN FIRED. NOT REGISTERED IN CMake.
//
// This file is a seam and an intention, not evidence. It is committed so the
// two production macro hooks it overrides are not left with nothing that can
// ever reach them, and so the debt is written down where the next person will
// find it instead of being rediscovered as an argument.
//
// What IS established: the healthy path. texture_uv_join_v2_directed passes
// 30/30 on tb_uv_join_v2_pair including the new trust-boundary section, and
// the island lints to a warning set identical to pristine HEAD. The R1T and
// E1 production changes stand on that. The CONTROLS do not.
//
// What is OWED before either may be registered:
//   1. Fire each one and record what it actually printed. The repo law is
//      that a detector not shown to fire has not been tested, and the
//      descriptions below are aimed, not observed.
//   2. A C++ expectation macro per selector, so the driver inverts its own
//      polarity -- the PACKET_E_EXPECT_* convention every other shim here
//      follows. Without one there is no inverse-polarity driver at all.
//   3. For R1T, determine which of the four drivers that instantiate
//      zhao_texture_island_v3_top drives enough back-to-back reciprocal
//      traffic to separate the head owner from the live token. Below that
//      traffic the mutant and production are identical and a green result
//      would mean nothing.
//   4. A C++-side collision check, which the Packet-E header requires of
//      every shim ("must independently fail SV elaboration and C++
//      preprocessing"). Only the SV side exists.
//
// Placement, which is not optional: each production file `undef`s its own
// macro at its end, so the shim must be compiled IMMEDIATELY BEFORE the file
// it overrides. Anything later gets the production default back.
//   E1  -> immediately before fpga/rtl/texture/zhao_texture_uv_join_v2.sv
//   R1T -> immediately before fpga/rtl/texture/zhao_texture_island_v3_top.sv
// ===========================================================================
//
// Both boundaries exist to stop a payload and its identity being taken from
// different moments. Neither failure is reachable with legal stimulus once the
// register is correct -- the pairing is only wrong while something else is in
// flight -- so each needs a committed mutant rather than an argument.
//
//   R1T_LIVE_TOKEN : the UVW bank is addressed by the LIVE reciprocal token
//                    instead of the registered head owner. Under back-to-back
//                    traffic the row read for one owner is stamped with
//                    another owner's reciprocal. INTENDED observable, not yet
//                    seen: the island's a_uvw_read_uses_head_identity.
//
//   E1_LIVE_TRUST  : the joined descriptor is masked by the LIVE offered
//                    usability flag instead of the verdict captured with the
//                    payload. A held descriptor then publishes under whatever
//                    verdict happens to be offered later, which is exactly the
//                    stale-row escape the register prevents. INTENDED
//                    observable, not yet seen: the directed driver's
//                    trust-boundary checks. No island assertion is in scope
//                    for the standalone join bench.
`default_nettype none

`ifdef ZHAO_TIMING4_R1T_MUTANT_LIVE_TOKEN
  `ifdef ZHAO_TIMING4_MUTANT_SELECTED
    `define ZHAO_TIMING4_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_TIMING4_MUTANT_SELECTED
    `define ZHAO_ISLAND_T4_UVW_READ_OWNER(head_owner, live_owner) live_owner
  `endif
`endif

`ifdef ZHAO_TIMING4_E1_MUTANT_LIVE_TRUST
  `ifdef ZHAO_TIMING4_MUTANT_SELECTED
    `define ZHAO_TIMING4_MUTANT_SELECTOR_COLLISION
  `else
    `define ZHAO_TIMING4_MUTANT_SELECTED
    `define ZHAO_UVJOIN_T4_DESC_TRUST(held, live) (live)
  `endif
`endif

`ifdef ZHAO_TIMING4_MUTANT_SELECTOR_COLLISION
  `error "ZHAO_TIMING4_R1T_E1_MUTANT_SELECTOR_COLLISION: define exactly one selector"
`endif

`ifdef ZHAO_TIMING4_MUTANT_SELECTED
  `undef ZHAO_TIMING4_MUTANT_SELECTED
`endif
`default_nettype wire
