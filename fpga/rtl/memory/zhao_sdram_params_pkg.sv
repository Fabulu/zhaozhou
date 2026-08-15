// zhao_sdram_params_pkg.sv — the ONE SDRAM parameter set (plan W2.5, D2).
//
// Law: spec/memory_rules.md §1 (D2). Everything SDRAM-shaped — the
// synthesizable controller (zhao_sdram_ctrl.sv), the behavioural testbench
// model (sim/models/zhao_sdram_model.sv, TESTBENCH-ONLY) and the zref oracles
// (reference/include/zref/zref_mem.hpp) — is parameterized through THIS
// package so the sim profile exists in exactly one place.
//
// *** ZH-004 SEAM (the only include site): ***
//   The board probe ZH-004 later emits `fpga/rtl/generated/sdram_params.svh`
//   from board_truth.json. When that file exists, the build defines
//   ZHAO_SDRAM_BOARD_PARAMS and includes it HERE; it provides the same
//   localparams with measured board values. Until then the conservative sim
//   profile below is the FROZEN verification profile (captures record its
//   version, plan R3). Downstream blocks verify against this profile, never
//   against hoped-for board numbers.
//
// The numbers below are provisional sim constants, NOT board truth. The
// obligations that unfreeze them are spec/memory_rules.md §1 (device code,
// speed grade, clock stability, sustained bandwidth, measured
// tRCD/tRP/tRC/refresh accounting, thermal).
//
// Conservative SystemVerilog subset only (charter §2): package, localparams.
// Lint: clean under `verilator --lint-only -Wall` (lint_mem_params CTest).

package zhao_sdram_params_pkg;

`ifdef ZHAO_SDRAM_BOARD_PARAMS
  // ---- board truth (ZH-004): generated/sdram_params.svh defines every ----
  // ---- localparam below with measured values; nothing else changes.   ----
  `include "sdram_params.svh"
`else
  // ---- conservative SIM profile (spec/memory_rules.md §1, D2, FROZEN) ----
  // (params are consumed by different importers; Verilator's UNUSEDPARAM
  //  fires per importer — the same pattern zhao_pkg.sv uses)
  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned CAS_LATENCY      = 3;    // read data latency after READ
  localparam int unsigned BURST_LENGTH     = 8;    // words per READ/WRITE burst
  localparam int unsigned T_RCD            = 3;    // ACTIVE -> READ/WRITE
  localparam int unsigned T_RP             = 3;    // PRECHARGE -> ACTIVE
  localparam int unsigned T_RC             = 9;    // ACTIVE -> ACTIVE (same bank)
  localparam int unsigned REFRESH_INTERVAL = 780;  // cycles between AUTO_REFRESH
  /* verilator lint_on UNUSEDPARAM */
`endif

  // ---- derived scheduling quantities (also frozen law; see the law table
  // ---- in zhao_sdram_ctrl.sv's header for the cycle-exact derivation) ----
  /* verilator lint_off UNUSEDPARAM */

  // REFRESH_OVERHEAD (spec/memory_rules.md §2): one AUTO_REFRESH steals
  // T_RP (precharge-all) + T_RC (refresh cycle) = 12 sdram cycles.
  localparam int unsigned REFRESH_OVERHEAD = T_RP + T_RC;

  // REFRESH_URGENT: a refresh whose counter has waited this many cycles past
  // the interval preempts client grants (it may still lose ONE burst span to
  // the arbiter's aging override — see the ctrl law table). 40 = the arbiter
  // liveness bound (zhao_pkg.ZHAO_ARB_LIVENESS_BOUND) as the deferral budget.
  localparam int unsigned REFRESH_URGENT    = 40;

  // MAX_BURST_SPAN: worst grant-to-grant span of one burst under this profile
  // (bank-conflict READ: PRE, T_RP, ACT, T_RCD, READ, CAS, BURST; the law
  // table in zhao_sdram_ctrl.sv derives the exact number). The arbiter's
  // aging-override threshold and the refresh deferral bound consume it.
  localparam int unsigned MAX_BURST_SPAN = 19;

  // Geometry: 128 MB = 4 banks x 8192 rows x 2048 cols x 2 B (16-bit words).
  localparam int unsigned BANKS        = 4;
  localparam int unsigned ROW_BITS     = 13;
  localparam int unsigned COL_BITS     = 11;
  localparam int unsigned WORD_BYTES   = 2;
  /* verilator lint_on UNUSEDPARAM */

endpackage : zhao_sdram_params_pkg
