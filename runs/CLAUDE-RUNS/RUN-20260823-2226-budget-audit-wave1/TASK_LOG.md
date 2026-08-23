# TASK LOG — RUN-20260823-2226-budget-audit-wave1

Contemporaneous. Newest entries appended at the bottom.

## 2026-08-23 22:26 — run initialised

`runs\CLAUDE-RUNS\init-run.ps1 budget-audit-wave1` on `main`, clean tree.

Read before touching anything: `docs/OWNER_DOCKET.md` (top ruling + the TMU and
Field rearchitecture entries), `reports/REMAINING_BLOCKERS.md`,
`reports/QUARTUS_GOTCHAS.md` (nine entries + the addendum),
`design/budgets/dsp.md` (the corrected LOWER-BOUND rule),
`design/budgets/latency.md`, `docs/BUILD.md`, and
`tools/quartus/run_block_fit.ps1` in full — the last because the map-only lane
must inherit its SDC generation, its merge-never-replace semantics and its
per-invocation workspace uniquifier, all three of which exist because losing
them cost real measurements.

Facts established before writing SPEC:

* `find fpga/rtl -name '*.sv' | wc -l` = **94**. Confirms the docket's count.
* `reports/synthesis/zhao_block_fit.json` holds **57 rows**, but 11 are
  parameter variants (`variantOf`) and 5 are non-`ok` (timeout / failed), so
  the honest count of distinct modules with a usable measurement is lower than
  57 and the docket's "41 measured" is the right order.
* Verilator lives at
  `C:\programmieren\zencrifice\.tools\oss-cad-suite\bin\verilator_bin.exe`
  and is NOT on the default PATH — `tools/env/zhao-env.ps1` must be dot-sourced.

SPEC_v1.md written. Two open questions are on the critical path and get
answered first, because a wrong answer to either re-plans a whole deliverable:
(a) does `verilator --xml-only` elaborate these modules standalone, and (b)
does `quartus_map` alone report DSP and RAM inference, or is a fit required?

## 2026-08-23 22:28 - the two critical-path questions, answered before anything was built

**(a) `verilator --xml-only` DOES NOT EXIST on this machine.**

    %Error: Invalid option: --xml-only

Verilator here is **5.051**, which replaced XML with `--json-only`
(`Vtop.tree.json` + `Vtop.tree.meta.json`). Same elaborated tree, different
serialisation; the ruling allows "or equivalent". Recorded because the docket
names the flag and a reader following it literally would conclude the tool is
missing.

A second surprise from the same tool: `verilator_bin.exe --help` is
`%Error: Invalid option: --help`. The perl wrapper owns `--help`, and
`docs/BUILD.md` forbids the wrapper. Options had to be found by trying them.

**(b) `quartus_map` ALONE answers DSP inference, RAM inference and gross size.**
This was the question that could have re-planned the third deliverable, and the
answer is better than hoped. `blockmap.map.summary` carries
`Total DSP Blocks`, `Total block memory bits` and `Total registers`; the fuller
`blockmap.map.rpt` adds `Estimate of Logic utilization (ALMs needed)`, a
**RAM Summary** table naming every inferred memory with its mode and shape, a
**DSP Block Usage Summary** giving the `Two Independent 18x18` / `Sum of two
18x18` decomposition and the signed/unsigned/mixed split, and a per-entity
resource breakdown.

Consequence for the whole run: **the calibration microbenches can be MAPPED
rather than FITTED**, ~15 s a point instead of 300-1300 s. That is the
difference between ninety calibration points and none.

## 2026-08-23 22:34 - PREDICTION 1 CONFIRMED, WITH A NUMBER

    zhao_geom_project   map-only, HEAD, 5CSEBA6U23I7
    Total DSP Blocks : 33
    Two Independent 18x18 : 22
    Sum of two 18x18      : 11

**33 DSP blocks - the same 33 `zhao_terrain_project` was fitted at.** The
duplication asserted by the block's own header is now measured, and it is the
single largest item on the board.

The decomposition is worth as much as the total: 33 blocks for **11** products
is exactly **3 DSP blocks per 32x32 signed product**, and it is the first time
this project has had that ratio from the tool rather than from a datasheet.

## 2026-08-23 22:40 - the map lane, and one bug in it worth recording

`tools/quartus/run_block_map.ps1` written, inheriting run_block_fit.ps1's
merge-never-replace, its failed-run-keeps-the-prior-row guard, its
per-invocation workspace uniquifier and its `-c core.autocrlf=true` git status.
New: `sourceListHash`, because a row that names its commit but not its file
list is not provenance and this repo's two source lists have drifted once.

**The first draft read `Embedded Multiplier 9-bit elements`** - a Stratix /
Cyclone IV label - and produced `dspBlocks: null` next to a summary reading
`Total DSP Blocks : 33`. Then `estimatedAlms` came back null too, because the
`.map.summary` is `Label : Value` lines while the `.map.rpt` is ASCII TABLES
whose rows are `; Label ; Value ;`, and one parser cannot read both. Two
parsers now, and the labels are copied from the tool's output rather than from
memory. **Both bugs were silent nulls, which is the same failure shape as
everything in QUARTUS_GOTCHAS.**

## 2026-08-23 22:45 - map sweep launched: 90 modules, serial

`tools/quartus/map_sweep.ps1`, ordered by audit value rather than
alphabetically, so a sweep stopped early has still answered what was asked. One
`quartus_map` at a time. `zhao_shell_top` excluded with its reason recorded
(elaboration alone exceeded ten minutes and 16 GB in 17.0.2; it has its own
lane).

## 2026-08-23 23:05 - `tools/budget/scan_rtl.py`, and THREE RULES THAT WERE WRONG

The scanner works and answers the question the grep could not:
**`zhao_geom_project` has 11 nonconstant multiplies**, from three written `*`
operators, because `mul32` is called nine times. 91 of 91 modules elaborate.

**But three of its rules were wrong on their first draft, and each was caught
by disagreeing with a measurement rather than by inspection.**

1. **The extension-slack rule flagged the CORRECT idiom.** It went RED on
   `$signed({{32{a[31]}}, a}) * $signed({{32{b[31]}}, b})` - which is how you
   write a widening signed product, and how `zhao_geom_project` writes all nine
   of its matrix products. Refuted by the 33-DSP map result in the same hour:
   33 blocks for 11 products is the **32x32** decomposition, so Quartus folds
   the extension away. A rule left as drafted would have sent an implementer to
   rewrite nine correct lines. Rekeyed onto the honest width AFTER peeling,
   which is `zhao_geom_lod`'s actual section-5 case (72-bit operands with
   nothing to peel). **A calibration point now exists specifically to pin this**
   (`calib_widen_explicit` vs `calib_widen_implicit`) rather than resting on
   one module's total.

2. **Sizing arrays alone called three divider PIPELINES uninferred memories.**
   `dstep_dv[0:31][0:2]` is 6,048 bits of 32-stage divider pipeline written
   from a genvar loop; two of its reads use a loop variable, so `dynamic > 0`
   was true and both projectors - the two largest rows on the board - carried a
   false `EXPECTED_RAM_NOT_INFERRED`. Wrong twice over: the alarm is false, and
   it sits exactly where a real one would be lost. Fixed with **access sites
   per element**, which separates them cleanly on this repo's own data:

   | array | elements | access sites | verdict |
   | --- | ---: | ---: | --- |
   | `zhao_field_seq` `rf` | 64 | 11 | memory |
   | `zhao_forge_cliff` `prio_mem_r` | 2,048 | 2 | memory |
   | `zhao_geom_pose_cache` `tags` | 128 | 2 | memory |
   | `zhao_geom_project` `dstep_dv` | 32 | **754** | pipeline |

3. **The state variable was picked by NAME and it picked the wrong thing.**
   On `zhao_texture_tmu` it chose `ST_IDLE` - a `localparam` - over `st_r`, and
   reported `II >= 1` for the block whose **measured II is 6**. Rekeyed onto
   behaviour: the state variable is whatever a process assigns several distinct
   constants to, `LPARAM`/`GPARAM`/`GENVAR` excluded outright. TMU now infers
   **II >= 5 against a measured 6** - a lower bound, in the right direction.

**A fourth gap, found the same way.** `zhao_field_seq`'s three constant tables
are `always_comb` case trees, not arrays, so the array pass could not see any
of them - and Verilator lowers `case` into separate IF/ASSIGN statements, so
looking for one assignment holding many constants found nothing either. Keyed
per TARGET VARIABLE instead, and Field's cone now reports **4 tables totalling
20,817 bits of ROM built from LUTs** - including the sine table counted twice,
which is exactly what the Field ruling says is there.

## 2026-08-23 23:10 - the heatmap exists, and the falsifiable test PASSES

`design/budgets/workloads.yml` + `tools/budget/build_manifest.py` ->
`reports/budget_manifest.json` and `reports/BUDGET_HEATMAP.md`.

The docket set two blocks as the calibration of the flags themselves. Neither
rule was written knowing the answer:

| block | severity | mechanically, because |
| --- | --- | --- |
| `zhao_field_seq` | **RED** | `EXPECTED_RAM_NOT_INFERRED` - 22,865 expected bits (2,048 addressable + 20,817 const-ROM), zero design memories inferred by the map |
| `zhao_texture_tmu` | **RED** | `NO_RESERVE` - 850,000 samples/frame against 1,666,667/6 = 277,778 capacity, **3.06x** |
