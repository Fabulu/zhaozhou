// zhao_abi_pkg_formal_stub.sv — FORMAL-ONLY stub of the generated ABI
// package for the SymbiYosys lane (testbench component, NEVER synthesis or
// the Verilator ctests — those read the real generated package).
//
// Why: fpga/rtl/generated/zhao_abi_pkg.sv (frozen, byte-identity-gated)
// uses unpacked-array localparams and unpacked function arguments that
// yosys's SystemVerilog frontend cannot parse, and the suite carries no
// sv2v. The audio formal cone references NOTHING from zhao_abi_pkg:
// zhao_pkg's only use of it is a wildcard import (`import zhao_abi_pkg::*`),
// and zhao_audio_fifo uses exclusively zhao_pkg's OWN declarations
// (ZHAO_CNT_AUDIO_UNDERRUNS, ZHAO_COUNTER_VAL_BITS, zhao_counter_snap_t).
// An empty package therefore elaborates the DUT source byte-for-byte as
// committed — no substitution of anything under verification.
//
// The .sby copies this file into the workdir AS `zhao_abi_pkg.sv`
// ([files] rename) so the real zhao_pkg.sv is read unmodified.
//
// If a future block's formal property DOES reference generated ABI items,
// extend this stub with exactly those items (values from the generated
// file, never invented) — or route a zhao_pkg.sv change request to the
// architect for a yosys-parseable ABI split.

package zhao_abi_pkg;
  // intentionally empty (see header)
endpackage : zhao_abi_pkg
