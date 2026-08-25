# FIELD v2 Model (Phase 1 + Phase 2) - Findings

**Agent ID:** 20260825-1808-field-v2-model
**Created:** 2026-08-25 18:08 (local)
**Parent Task:** Owner-authorised FIELD v2 redesign — histogram + throughput model (no RTL)
**Status:** Complete

## Summary
Earth opcode histogram measured from the committed program builders (hashes
verified against goldens); throughput model swept over W∈{2,4}, N∈{4,8},
mul lanes 1–4. Full deliverable: `reports/FIELD_V2_MODEL.md`. Recommended
point: **width 4, 8 wavefronts, 2 mul lanes**, with CURVE/DCURVE, DIST2/isqrt
and RING pipelined and uniform-op hoisting in the front-end; NORMALIZE has
zero Earth demand.

## Findings
- Histogram (measured): impact_wave 30 / wave_pool 27 / crater_ring 28 instr;
  aggregate MUL 29, LDC 21, SUB 11, CURVE 4, DIST2 3, RCP 3, CLAMP 3,
  DCURVE 2, SIN 1, COS 1, RING 1, CMP 1, SELECT 1, ADD 1, END 3. NORMALIZE: 0.
- As-is units: nothing closes at any (W,N,L) — 439–930% of budget; binding
  unit is CURVE (impact_wave), DIST2 (wave_pool), RING (crater_ring).
- With the big three pipelined: W=2 never closes (front-end); W=4 L=1 never
  closes (MUL); W=4 L≥2 closes at 94.1% bound by RCP's *derived* II=9.
- Uniform hoisting (RCP + 2 CURVE + DCURVE are per-association in the real
  programs) drops worst case to 52.3% and relaxes unit targets to II ≤ 9.
- Measured RF probe (12 M10K, 375 ALM, 96.54 MHz) supports a lane-private
  3-copy organisation: 12 M10K at 4 lanes × 8 wavefronts × 32 regs.

## Recommendations
- Measure RCP II at the unit boundary before trusting the no-hoisting closure.
- Pipeline in demand order: CURVE/DCURVE, DIST2/isqrt, RING; 2 mul lanes.
- Do not build NORMALIZE throughput for Earth.

## Files Examined
- compiler/src/field_ir/{impact_wave,wave_pool,crater_ring,builder,types}.ts (+ dist)
- compiler/tests/generated/{impact_wave,wave_pool,crater_ring}.hpp
- reports/FIELD_RESOURCE_MODEL.md, reports/synthesis/zhao_block_fit.json
- fpga/rtl/field/zhao_field_curve.sv, zhao_field_sin.sv (read-only)
- fpga/rtl/synth/zhao_probe_banked_rf.sv (read-only)
