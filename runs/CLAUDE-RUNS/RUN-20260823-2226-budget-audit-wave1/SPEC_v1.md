# SPEC v1: Repo-wide resource/timing/rate audit — wave 1 (budget compiler)

**Run ID:** RUN-20260823-2226
**Created:** 2026-08-23 22:26 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Replace the one-block-at-a-time rescue loop with **evidence and ranked work**.
Per `docs/OWNER_DOCKET.md` (2026-08-23, "STOP THE ONE-BLOCK-AT-A-TIME LOOP:
build a budget compiler"), success is:

1. `tools/budget/scan_rtl.py` exists and answers, from an **elaborated AST**
   (Verilator `--xml-only`), questions a regex provably cannot: how many
   nonconstant multiplies, at what operand widths, with what signedness, at
   what dependency-chain depth, in which module — plus variable shifts,
   division/modulo, serial combinational loops, wide add->compare->saturate
   chains, duplicated expensive expressions, array shape/read-style/reset-style
   and inferred RAM/ROM expectation, interface shape and inferred minimum
   initiation interval, and counters gated by deep combinational logic. Every
   finding carries RED/ORANGE/YELLOW/GREEN **with a reason**.
2. A **Quartus calibration table** measured on this exact tool
   (Quartus Prime Lite 17.0.2) and device (`5CSEBA6U23I7`) that turns
   "3 x signed 33x32, input+output registered" into a DSP/ALM number instead of
   a hand-wave. Same for RAM templates.
3. A **map-only pass** over every RTL module that has no measurement, so the
   41-of-94 census gap closes enough to rank work.
4. `reports/budget_manifest.json` + `reports/BUDGET_HEATMAP.md`, per block,
   carrying resources at HEAD, expected-vs-inferred RAM, corrected-I/O Fmax
   where it exists, WNS/TNS/hold, critical-path family, inferred II,
   items/frame, demand ratio, provenance, composition status and debt flags:
   `NO_CURRENT_FIT`, `OLD_SDC`, `NO_WORKLOAD`, `NO_II_TEST`,
   `EXPECTED_RAM_NOT_INFERRED`, `NO_SUBSYSTEM_FIT`, `NO_RESERVE`.

**The falsifiable test of deliverable 4**: run it against `zhao_field_seq`
(0 M10K, 8,901 ALMs, a 64x32 register file and three ROMs built from logic) and
`zhao_texture_tmu` (II=6 against a demand needing II=1). Both must light up RED
from mechanical rules alone. If they do not, the heatmap does not work.

### Predictions to confirm or refute — reported either way

| # | prediction | source |
| --- | --- | --- |
| P1 | `zhao_geom_project` is absent from the fit report and duplicates `zhao_terrain_project`'s 33 DSP (~50 DSP redundancy) | docket |
| P2 | `GEOM.POSE` (`quat2mat` 9 products + `mat3x4_mul` 3x 32x32/cycle) hides 14-18 DSP | docket |
| P3 | `FORGE.CLIFF` — 3 async reads over ~120 kbit, fit timed out at 5,000+ s; does any RAM infer under map? | docket (confirmed source-side) |
| P4 | `RASTER.FRAGMENT` blend: 2 products per channel are mutually exclusive; mux-before-multiply takes blend 6 -> 3 DSP | docket |
| P5 | `TERRAIN.NORMALS` six 33x33 products vs 2,000 normals/frame: 18 -> ~3 | docket |
| P6 | Pose palette 128 x 32 x 12 x 32 bits = 1,572,864 bits ~= 150 M10K = 28% of device | docket |

---

## Scope

**In Scope:**

- `tools/budget/scan_rtl.py` (Verilator XML AST scanner) + its output
- `tools/budget/gen_calib.py` (+ runner) — generated microbench modules, mapped
  and where affordable fitted, one Quartus job at a time
- `tools/budget/build_manifest.py` — merges scan + fit + map + workload into
  `reports/budget_manifest.json` and renders `reports/BUDGET_HEATMAP.md`
- `tools/quartus/run_block_map.ps1` — map-only lane (no fit) over unmeasured
  modules, recording source commit, dirty-tree state, tool version, device,
  parameter set, source-list hash
- Confirming/refuting P1-P6 with evidence
- A ranked RED list with estimated returns

**Out of Scope:**

- **Optimising or rearchitecting any block.** This run produces evidence, not
  fixes. Single exception permitted by the ruling: if a map cannot complete
  because storage is clearly uninferable, say so and STOP — do not fix it.
- Full fits of the whole census under the corrected SDC (campaign-sized; the
  map-only lane is deliberately the cheap substitute)
- Composed / subsystem-pair fits (docketed for later waves)
- Editing contracts or budgets to match new numbers (report first)

---

## Constraints

- **One Quartus job at a time. No exceptions.** Concurrent fits permanently
  lost `zhao_geom_project`'s row (`reports/QUARTUS_GOTCHAS.md` §7: three
  concurrent fits exhaust this 24 GB machine).
- **Never edit RTL while a Quartus job runs.** Disclosed as a failure twice on
  2026-08-23.
- `git checkout <rev> -- <paths>` **STAGES**. Use `git restore --worktree`, and
  check `git status` afterwards regardless.
- **Never hand-edit a measured number into a report.** Every number in the
  manifest must be traceable to a tool log kept as evidence.
- A fit/map is only evidence if it was constrained — verify
  `Info (332111): 10.000 clk` and keep the log.
- **Measure that a directive did what was asked** (GOTCHAS §3, §5): a
  `ramBlocks = 0` after a memory conversion is a failed implementation even if
  every test passes.
- Verilator is invoked as `verilator_bin.exe`, never the perl wrapper
  (`docs/BUILD.md`); `. .\tools\env\zhao-env.ps1` in the same shell first.
- Budget arithmetic uses **1,666,667 clocks/frame** (compute), NOT
  `frame_gpu_cycles` = 251,520 (raster/video deadline) —
  `design/budgets/latency.md` lines 63-78.
- DSP rule (`design/budgets/dsp.md`, corrected today): operator count is a
  **LOWER BOUND**; width and signedness change cost discontinuously.

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

- **Regex/grep arithmetic inventory.** Tried twice by the owner on
  `zhao_geom_project`: gave `0`, then gave line counts. Both useless. This is
  the entire argument for the AST scanner.
- (append as encountered)

---

## Open Questions

- Does Verilator elaborate every one of the 94 modules standalone, or do some
  need parameter/package context to reach `--xml-only`?
- Is `quartus_map` alone enough to report DSP and RAM inference, or does the
  `.map.summary` omit them until a fit runs? (Must be verified, not assumed —
  if map does not report DSPs the third deliverable needs re-planning.)
- Which of the 94 modules are packages/stubs/tops and therefore should be
  excluded from the "modules with a map" denominator?
