# Fable architecture commission — recover Cyclone V dual-18x18 physical packing

Worktree/repository: `C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912`
Branch: `claude/ceiling-architecture-20260912`
Date: 2026-09-12

## Deliverable

Write one architecture report only:

`reports/DSP-DUAL18-PACKING-ARCHITECTURE-20260912.md`

Do not implement RTL or tools. Do not edit terrain, generated files, tests, manifests, fit targets, run logs, or any existing report. Do not run Quartus, CMake, CTest, Verilator, git commit, or git push. Read/search only, then write the one report.

This is major ceiling architecture. Challenge the premise rather than laundering it into a plan.

## Programme truth to retain

- Provisional device: Cyclone V `5CSEBA6U23I7`, 41,910 ALM / 112 physical variable-precision DSP blocks / 553 M10K.
- Owner's tightened closure targets are 30,000 ALM / 85 DSP. The earlier 36,000 / 88 roadmap allocation and 94-DSP soft cap no longer define closure. Comfortable margin is required; merely landing at 85 is not closure.
- The selected census 58,359 ALM / 192 DSP / 147 M10K is partial mixed-age evidence with 34 DSP-unpriced and 45 ALM-unpriced roots. It is not a current whole-machine total.
- Start architectural arithmetic from the corrected **111-DSP conditional structural frontier** in `reports/CEILING-FRONTIER-RECONCILIATION-20260912.md`, never from the illegal 99-DSP point. `ROWS_PER_PASS=1` is not legal for dense two-view terrain. Retain RPP=3.
- Terrain composition commit `1f9a4f95` functionally composes the shared service but deliberately leaves NORMALS, DEPTHQUANT, fit, and production adoption open. Do not call the shared-projector saving installed or measured.
- D3 shell characterization is a separate in-progress measurement packet. Do not speculate from the stale shell fit.
- Every fit must answer one named architectural question at a meaningful boundary. A tiny MapOnly discriminator is acceptable only if it decides whether this entire architecture exists. Do not propose a sweep of leaf fits.
- Every detector must be positively demonstrated to fire. Use a committed renamed mutant for a fault legal stimulus cannot reach.
- Keep `zhao_shell_top.sv` untouched. Do not touch Qwen.

## Recovered lead that must be verified, not assumed

The official Cyclone V device handbook says one variable-precision DSP block supports two independent 18x18 multiplication instances, and the A6 device exposes up to 224 independent 18x18 multipliers from 112 physical blocks:

- https://docs.altera.com/r/docs/683375/current/cyclone-v-device-handbook-volume-1-device-interfaces-and-integration/resources?contentId=m1eTGdYr~hIQ930EPymwPg
- https://docs.altera.com/r/docs/683375/current/cyclone-v-device-handbook-volume-1-device-interfaces-and-integration/supported-operational-modes-in-cyclone-v-devices?contentId=P_g8Sh12N70nsgY7jCUdgg

Yet Quartus Lite 17.0.2 has repeatedly failed to co-pack independent inferred `*` operators:

- `reports/BUDGET_HEATMAP.md:403-465`: `zhao_geom_quat2mat` used nine physical DSP blocks for nine 16x16 products even though two products fit architecturally in each block.
- `reports/synthesis/blockpaths/zhao_geom_project.fit.rpt:4748-4800`: 22 physical blocks in `Two Independent 18x18` mode plus 11 `Sum of two 18x18` blocks for eleven full-width products. Each row occupies a distinct `DSP_*_N0` location.
- `tools/budget/calibration.json`: inferred signed 32x18 costs two DSP blocks; inferred 18x18 costs one.
- `reports/PROJECT-CORE-MATW18-20260910.md:245-249` records an unmeasured hand-split idea.
- Quartus's installed primitive declaration is `C:\intelFPGA_lite\17.0\quartus\eda\sim_lib\cyclonev_atoms.v:4323-4535`. `cyclonev_mac` has A/B operand ports and `resulta`/`resultb`, but the encrypted behavior and exact legal operation-mode string mean the declaration alone is not proof.
- `reports/islandrearchitecture5.md:2247-2262` and `reports/G1C-COMBINE-V1-VARIANT-A-MEASURED-20260905.md:114-140` explicitly allow an `DSP2_PACKED_OR_EXPLICIT` vendor primitive/IP variant, but it was never built.
- `reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt:1493-1528,1676-1695` already describes exact partial-product packing candidates and says arbitrary inferred expressions are not evidence.

First settle the accounting semantics: prove whether Quartus's `Total DSP Blocks` and the device's 112 limit are the same physical variable-precision unit, whether the fitted rows truly waste a second 18x18 half, and whether one supported primitive/IP instance can expose two independent full-rate results. If any premise is false, say so and stop the claimed savings.

## Architecture questions

### 1. The reusable boundary

Design the smallest vendor-isolated exact arithmetic boundary that can expose both independent 18x18 lanes of one physical Cyclone V DSP while preserving a portable behavioral model for Verilator/reference tests. Decide between direct `cyclonev_mac`, an Intel megafunction/IP wrapper, or another Quartus-17-supported construction. State exact synthesis/simulation guards, source-list implications, reset/clock-enable behavior, pipeline stages, latency contract, signedness per lane, and how to prevent generic synthesis from elaborating both vendor and behavioral implementations.

Do not rely on undocumented wishful strings. Name what must be generated or observed to obtain authoritative primitive parameters.

### 2. Exact arithmetic families

Derive bit-exact, width-explicit formulas for all useful forms, including signed minima and mixed signedness:

- two unrelated <=18x18 products packed into one block;
- signed 32 x signed 18 as two 16/18-bit partials packed into one block, with exact shifted recombination;
- current projector viewport form factored exactly from `ndc * (vp << 15)` to `(ndc * vp) << 15`, then implemented as signed32 x unsigned12 in one packed block;
- signed 32x32 and signed 33x33 products using the minimum number of packed dual-18 blocks, comparing schoolbook and Karatsuba-like decompositions and their fabric-adder costs.

Preserve exact full products. No approximation, intermediate rounding, truncation, saturation, or altered throughput.

### 3. Current, legal candidate inventory

Audit current source and the corrected frontier, not stale fitted alternatives. At minimum examine:

- `zhao_project_core` at legal RPP=3/MATW=18: current structural 24 DSP = nine 32x18 row products at two each plus two old viewport products at three each. Test whether explicit packing can plausibly make this **11**, and list every condition that could prevent it.
- `zhao_field_v3_mulbank` / four `zhao_field_mul` lanes: structural ~12 DSP; test a 33x33 exact packed target and its two-cycle cadence.
- current `zhao_geom_skin` default `MUL_LANES=3`: 9 DSP structural/fitted ancestor shape; test 32x32 exact packed target without changing workload or II.
- current `zhao_geom_cull` default two lanes: structural six DSP; test whether the same wide-product primitive applies.
- terrain bake v2's one 34x34 site, terrain normals, terrain LOD, fog, shell-attributed and texture-attributed sites only where current composition proves they remain live.
- narrow independent pairs only where they share clock/reset/enable/latency and are truly simultaneous. Never pair across clock domains, separately stallable interfaces, or unrelated production roots merely to make arithmetic add.

For each candidate state: present cost, packed target, ALM/register/timing direction, behavioral and schedule risk, evidence class, and the subsystem fit where it can be measured. Strike obsolete/superseded blocks and already-sequenced multipliers.

### 4. Both ceilings, not DSP alone

ALM is at least as dangerous as DSP. Rank variants by DSP returned per added ALM and by timing risk. Prefer:

1. exposing an otherwise idle second DSP lane with no new wide fabric compressor;
2. exact 32x18 recombination that the current inferred implementation already appears to pay in fabric;
3. wide 32/33-bit decompositions only if their fabric adder cost does not sabotage the 30,000-ALM target.

Identify where M10K can offset any added registers/ALM without changing behavior, but do not invent a resource exchange from stale global availability.

### 5. One decisive first gate

Name one pre-registered discriminator such as `dual18_physical_pack_discriminator`. It should compare:

- two independently inferred registered signed 16/18-bit products;
- one explicit dual-lane vendor implementation with identical logical ports;
- optionally one exact 32x18 assembled product built from the packed lanes.

The question is physical and narrow: does Quartus 17.0.2 on `5CSEBA6U23I7` report one total DSP block for two independent results, and does the exact 32x18 form also remain one block? Specify the exact reports/hierarchy rows that count as proof and what observations kill the architecture. This is MapOnly unless a fitter question is genuinely necessary; timing waits for a subsystem boundary.

Require a generic-vendor differential, exhaustive/boundary arithmetic where tractable, randomized full-domain tests, stall/enable checks, and positive controls before the discriminator runs. Explain how the vendor implementation is tested without making Verilator depend on an encrypted Intel atom.

### 6. Honest frontier

Provide a conditional DSP lattice from 111 toward 85 and below. Never bank overlapping savings twice. Separate:

- already implemented/functional but unfitted;
- primitive packing structurally possible but unmeasured;
- MapOnly-confirmed after the future discriminator;
- subsystem-fitted;
- production-adopted.

Find a path with comfortable headroom if the architecture is real, but do not force the arithmetic to land at the target. If only ALM-expensive wide decomposition closes DSP, say the plan fails the two-ceiling objective and recommend the next non-overlapping lever.

## Required report ending

End with:

1. verdict on whether explicit dual-18 packing is a real architecture candidate;
2. exact first implementation packet (calibration wrapper/tests only, no production rewrites);
3. exact named MapOnly gate and pass/fail outcomes;
4. production migration order only after that gate passes;
5. remaining unknowns and claims explicitly not verified.
