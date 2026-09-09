# Task Log: RUN-20260909-2235 - TERRAIN.SHADE RTL (exact law, zero DSP)

**Created:** 2026-09-09 22:35 UTC+02:00
**Status:** Complete (uncommitted — owner reviews and commits, per brief)
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-2235-terrain-shade-rtl/

---

## Objective

Build TERRAIN.SHADE — `dot(n,L)/|n|` once per triangle, bit-exact against
the compiled `zref::render::shade_flat_tri_dir_unclamped` — the block whose
absence means production terrain has no lighting path. Refuse DSP and ALM,
spend M10K, no Quartus fit, no commits.

---

## Progress Timeline

### 2026-09-09 22:35 - Task started (run folder created after recon)

- Read, in brief order: contract, law source (`terrain.cpp:65`), thin view,
  bump report, `zhao_terrain_normals.sv`, CLAUDE.md; then rescue §14,
  `div_rhu_s128` (rast.cpp:31), `isqrt_u64` (zref_trig.hpp:79),
  workload demand (2,000 normals/frame), fit_targets/manifest formats,
  harness + mutant patterns.
- Found the contract's packet formats wrong (s1.15 sun/base vs the law's
  Q16.16) — same error its own oracle history records. Brief inherits it.
- Wrote `fpga/rtl/terrain/zhao_terrain_shade.sv`: quarter-square M10K for
  all six products, overlapped restoring isqrt, 64-step floor divide,
  fill-at-reset table, 4 counters. Lint 0; Quartus-17 form gate clean.
- Wrote `tests/terrain/terrain_shade_rtl_directed.cpp`: two-tier
  differential vs COMPILED law (linked `build/reference/libzhao_zref.a`).
  **4,142 / 4,142 pass. Latency 145 fixed. Counters exact:
  3,723 / 75 / 23 / 31. `--break-oracle` fails 1/4,142 — instrument
  proven.**
- Registered: tests/CMakeLists.txt (3 lanes incl. WILL_FAIL break-oracle),
  fit_targets leaf (question stated), prod_manifest `unused` deferral,
  blocks.yml row, contract amendments A1–A6.
- `check_prod_manifest.py`: my entry accounted; residual 2 UNACCOUNTED are
  a CONCURRENT lane's uncommitted forge files
  (RUN-20260909-2216-forge-prim-eval) — verified by differential stash,
  left alone deliberately; that lane registered them itself and the FINAL check is OK across all 215 modules.
- Report: `reports/TERRAIN-SHADE-IMPLEMENTATION-20260909.md`.

---

## Files Created / Modified

- NEW `fpga/rtl/terrain/zhao_terrain_shade.sv`
- NEW `tests/terrain/terrain_shade_rtl_directed.cpp`
- NEW `reports/TERRAIN-SHADE-IMPLEMENTATION-20260909.md`
- MOD `tests/CMakeLists.txt`, `design/blocks.yml`,
  `design/prod_manifest.yml`, `design/fit_targets.yml`,
  `design/contracts/TERRAIN.SHADE.md`

## Decisions Made

1. **Ports are Q16.16 signed 32 both ways** (contract amendments A1/A2) —
   forced by bit-exactness; the s1.15 rows could not carry the law's odd
   light constant or its 0x10000 full-scale.
2. **Zero-DSP shape**: quarter-square table (1 M10K) + add/sub root and
   divide; II=147 vs demand of 1/833 clocks. The II=1/II=3 DSP Pareto rows
   are not fitted — recorded as A3 with the rate argument.
3. **No committed mutant** — all four counters reachable with port
   stimulus and asserted with exact counts; nothing guards an unreachable
   state.
4. **Did not touch** the forge lane's manifest gap, geometry/, bake*,
   normalmap.sv (other lanes, some live).

## Next Steps

- Owner: review + commit; the art-law LOOK gate (moving sun, 240p) still
  stands and precedes any seam tuning.
- Seam lane: TERRAIN.PROJECT colour port contract, sun ABI word,
  NORMALS→SHADE pair bench.
- Fit: G-SHADE1, batched with the terrain-lighting subsystem (G-BUMP1
  batch). Question stated in `design/fit_targets.yml`.
