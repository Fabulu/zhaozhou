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
