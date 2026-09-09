# TASK_LOG — RUN-20260909-2216-forge-prim-eval

Goal: build FORGE.PRIM.EVAL, the lightning position evaluator the owner's
reports/ADDLIGHTNING.md names as THE missing work. Lane: fpga/rtl/forge +
reference model. No fits, no commits (owner reviews and commits).

## Log

- 22:0x  Read ADDLIGHTNING.md in full; zhao_forge_prim.sv; FORGE.PRIM.md.
         Confirmed: NO FORGE.PRIM.EVAL contract exists (all 118 contract
         files checked) — the brief stands; wrote the contract fresh.
- 22:1x  Found the rounding discrepancy: FORGE.PRIM.md "round-half away from
         zero" vs qformats §4 round-half-up. qformats followed; flagged in
         the report for the owner.
- 22:1x  tools/forge/gen_jitter_rom.py written and run — JITTER_Q16 emitted
         twice from one formula (SV ROM + zref header). 256x18 = 4,608 bits
         = one M10K at 45%.
- 22:2x  zref_forge_eval.hpp — the exact law: rational lerp
         floor((2Di+N)/2N), xorshift32-per-point jitter streams, literal
         seed^2 (salted), fused single-rounding displacement, branches
         growing from the CAPTURED jittered attach point.
- 22:3x  zhao_forge_prim_eval.sv — one operand-muxed 33x33 multiplier
         (terrain_normals mseq pattern), one 40-cycle restoring divider
         (zero DSP), caps refused-not-clamped, 7 counters.
- 22:4x  Lint: 0 diagnostics after fixing 1-bit branch index, unused start
         copies, sim-assert lint scope. check_quartus17_syntax: clean (220
         files).
- 22:5x  forge_prim_eval_directed: FIRST RUN TRAP — `exe | tail` printed
         nothing with RC=0 because the exe died at 127 (missing winlibs
         DLLs) and tail succeeded. The pipeline-RC lesson, live. With PATH
         right: 967 checks passed.
- 23:0x  Mutant committed (tests/mutants/zhao_forge_prim_eval_mutant.sv,
         S_E1 advance compare broken) + inverse-polarity driver:
         walk_overrun_o FIRED (=1). Jitter ROM directed: 512 checks.
- 23:1x  Registered: blocks.yml (FORGE.PRIM.EVAL entry + counter catalog +
         counter_ports), fit_targets.yml (gate F-EVAL1 with its exact
         question), prod_manifest.yml (excluded: not-yet-adopted; the
         unpriced row REMOVED because that list means "no RTL" — census now
         honestly says 5 unbuilt), tests/CMakeLists.txt (4 targets).
         check_prod_manifest OK (215 modules), check_counters resolves all 7.
- 23:2x  Upgraded the rate claim to MEASURED: worst legal bolt 5,451 clocks
         = 0.327% of computeClocksPerFrame, printed by the test with a
         12,000-clock regression tripwire. 968 checks.
- 23:3x  Contract + report written. Temp build dir removed.

## Note for the next lane

Another session is live in this repo (blocks.yml / prod_manifest.yml /
tests/CMakeLists.txt changed on disk mid-run); every shared-file edit here
applied cleanly and its checker was re-run afterwards.

## FINDINGS

1. The evaluator EXISTS now: unit-verified bit-for-bit (968+512+2 checks),
   deterministic under stalls by construction, caps hard, counters live.
2. What still blocks a bolt on screen is the FX.LIGHTNING dispatch seam
   (eval + prim ribbon jobs into GEOM.SETUP) — composition, not a block.
3. Fit gate F-EVAL1 (batched): multiplier ≤4 DSP w/ output reg, table
   infers 1 M10K, prim+eval inside 2,800 ALM / 10 DSP / 2 M10K.
4. Owner decision wanted: FORGE.PRIM.md rounding sentence vs qformats §4.
5. Creature-reel lightning renders = separate lane, untouched, outstanding.
