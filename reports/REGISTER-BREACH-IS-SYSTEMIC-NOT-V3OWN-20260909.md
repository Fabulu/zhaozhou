# The register breach is systemic — nine of eleven components — and its worst offender is a raster block

2026-09-09. Follows `ISLAND-SCORED-AGAINST-ITS-OWN-BUDGET-20260909.md`, which
found that `@g2-prod` misses its register budget by **+81%** against **+44%** for
ALM. This is the diagnosis of that miss. No fit was run: per-entity attribution
was already on disk in
`reports/synthesis/blockpaths/zhao_texture_island_v3_top@g2-prod.fit.rpt`.

**This corrects the shape of the whole texture investigation.** For days the
finding has been *"`zhao_texture_v3own` is 81% of the overage"*. That is true of
**ALM** and it is false of **registers**, and registers are the larger breach.

## Registers do not distribute like ALMs

Direct children of the island, ranked by registers:

| entity | registers | % | cum | ALM | % ALM |
|---|---:|---:|---:|---:|---:|
| **`zhao_raster_perspuv_svc`** | **3,240** | 19.9% | 19.9% | 1,710 | 15.8% |
| `zhao_texture_v3own` | 3,018 | 18.5% | 38.4% | **2,818** | **26.0%** |
| `zhao_texture_cache_pipe` | 2,945 | 18.1% | 56.5% | 1,151 | 10.6% |
| `zhao_texture_rsp_dispatch` | 1,321 | 8.1% | 64.6% | 471 | 4.3% |
| `zhao_texture_aux_pipe` | 1,226 | 7.5% | 72.2% | 502 | 4.6% |
| `zhao_raster_rcp24_svc` | 990 | 6.1% | 78.2% | 1,003 | 9.3% |
| `zhao_texture_tmu_plan` | 972 | 6.0% | 84.2% | 771 | 7.1% |
| `zhao_texture_material_combine_v2` | 749 | 4.6% | 88.8% | 537 | 5.0% |
| `zhao_texture_frag_expand` | 578 | 3.5% | 92.3% | 146 | 1.4% |
| `zhao_texture_palette_res` | 427 | 2.6% | 95.0% | 274 | 2.5% |
| `zhao_texture_bilerp_lane` | 173 | 1.1% | 96.0% | 81 | 0.7% |
| others (metajoin, uv_join, mosaic) | 211 | 1.3% | 97.3% | 66 | 0.6% |
| island top's own logic | 435 | 2.7% | 100% | 1,306 | 12.1% |

**The single largest register consumer is `zhao_raster_perspuv_svc`**, which has
not been named once in this investigation — because its ALM share is only 15.8%
and every pass was sorting by ALM. It is also not a texture block; it is
perspective interpolation, resident in the island by composition.

The top three — `perspuv_svc`, `v3own`, `cache_pipe` — are **9,203 registers,
56.5% of the island**, in near-equal thirds. `v3own`'s ALM dominance (26.0%, over
1.6× the next block) has no register analogue.

All three are **flat**: their registers are their own logic, not submodules.
`perspuv_svc` has one child (an `altsyncram` holding zero registers);
`cache_pipe` has six, all zero-register memories; `v3own` holds 2,841 of its
3,018 itself, with 173 in three `v3rq` queues.

## Against the §3.3 per-component budget

`islandrearchitecture4.md` §3.3 gives eleven components an ALM and register
target. Mapping measured entities onto them is **a judgement, and it is stated as
one** — the FRAGROB row in particular is read as covering the token fabric, so
`frag_expand + v3own + metajoin + uv_join` are summed against it.

| component (§3.3) | ALM bud | reg bud | ALM | reg | ×ALM | ×reg |
|---|---:|---:|---:|---:|---:|---:|
| FRAGROB + token fabric | 900 | 1,200 | 3,008 | 3,767 | **3.34** | 3.14 |
| RCP24 scheduler v2 | 650 | 600 | 1,003 | 990 | 1.54 | 1.65 |
| perspective pair pipeline | 900 | 700 | 1,710 | 3,240 | 1.90 | **4.63** |
| binding tables + TMU planner v2 | 700 | 500 | 771 | 972 | 1.10 | 1.94 |
| synchronous texture cache v2 | 900 | 900 | 1,151 | 2,945 | 1.28 | **3.27** |
| class router + decode stores | 350 | 400 | 471 | 1,321 | 1.35 | **3.30** |
| transactional resident palette | 250 | 200 | 274 | 427 | 1.10 | 2.13 |
| serial bilinear channel engine | 250 | 200 | 81 | 173 | 0.32 | 0.86 |
| Mosaic CSD pipeline | 500 | 350 | 23 | 40 | **0.05** | **0.11** |
| material combiner | 650 | 500 | 537 | 749 | 0.83 | 1.50 |
| AUX v2 | 550 | 500 | 502 | 1,226 | 0.91 | **2.45** |
| **TOTAL (mapped)** | **6,600** | **6,050** | **9,530** | **15,850** | **1.44** | **2.62** |

**Nine of eleven components exceed their register budget. Seven of eleven exceed
their ALM budget.** The register overrun is not a localised defect with a
localised remedy — it is the budget's register model being wrong, or the
implementation's pipelining being uniformly deeper than the model assumed, across
almost every block independently.

Two components are dramatically **under** budget, which shows the budget was not
uniformly optimistic but specifically wrong in places:

* **Mosaic CSD pipeline: 23 ALM against 500 budgeted, 40 registers against 350.**
  It also measures **0 DSP**, so §3.4's *"require DSP == 0 for the CSD variant"*
  is satisfied in this fit. (A `fit_targets.yml` comment reads *"S3.4: require
  DSP == 0 for the CSD variant. Measured 4 today"* — that describes an older row,
  not `@g2-prod`. Comparing a current fit to that comment would have produced a
  confident wrong conclusion, which is the law about never comparing a current
  file to an old measurement.)
* **serial bilinear channel engine: 81 ALM against 250.**

## The DSP breach, by contrast, IS localised — and one rejected lever closes it

| component | DSP budget | measured |
|---|---|---:|
| perspective pair pipeline | 6 | 6 ✓ |
| RCP24 scheduler v2 | 3–4 | **6** ✗ |
| serial bilinear channel engine | 1 | **3** ✗ |
| material combiner | 0–2 | 2 ✓ |
| Mosaic CSD pipeline | 0 | 0 ✓ |
| **island** | 11–13 target, **14 hard** | **17** |

Every DSP over budget sits in exactly two blocks. And the ledger settles the
first one — **every** row ever recorded, across all parameterisations:

* `zhao_raster_rcp24_svc` — **6 DSP** in all six rows
* `zhao_raster_rcp24_v3` — **3 DSP** in all eight rows

**Swapping `rcp24_svc` for `rcp24_v3` is −3 DSP, taking the island from 17 to
exactly the 14 hard redline**, and brings that component inside its §3.3 budget
of 3–4. It also removes roughly 870 ALM.

That swap was assessed earlier and set aside as *"only 8% of the island"* — which
was the right verdict against a 3,336-ALM ALM problem and the wrong frame. Judged
against the criterion it actually decides, it is **sufficient, not marginal**: it
is the difference between failing DSP and meeting it.

It is not free. `rcp24_svc` measures 68.46 MHz and `v3` reaches 90.41–100.95 MHz
across its rows, so the swap looks favourable on frequency too, but every one of
those is a single seed and the only measured seed spread here is 4.70 MHz. And
`@island-profile` — the one attempt to fit v3 under the island's own parameters —
is recorded `incomplete:failed:quartus_map.exe`. **The swap is a measured DSP
result and an unmeasured island result.** Its remaining question is a fit, and
fits are the owner's call.

## What this changes for the redline decision

The decision was previously framed as one number: find 3,336 ALM or move the
7,500 line. It is actually three different problems with three different shapes:

1. **ALM (+44%)** — concentrated. `v3own` is 26% of the island and its state
   cannot be memory-backed. No lever inside the island closes it. Genuinely a
   contract conflict, as previously reported.
2. **Registers (+81%, the largest breach)** — **systemic**. Nine of eleven
   components over budget, worst offender a raster block at 4.63×, top three in
   near-equal thirds. This is not a `v3own` problem and cannot be fixed by
   anything done to `v3own`. Either the register budget is wrong or the island is
   pipelined deeper than the architecture assumed — and deciding which is an
   owner question about the *budget*, not an engineering hunt.
3. **DSP (+21%)** — localised, and **closable today** by a swap already built,
   already fitted standalone, and already priced. Pending one island fit.

## What this does not establish

**Not a new measurement.** Every number is read from the existing `@g2-prod` fit
report and the existing ledger. Nothing was fitted, no RTL changed, no criterion
moved.

**Not that the register budget is wrong.** It may be right and the implementation
uniformly over-pipelined. The evidence here says the breach is distributed, not
which side of the comparison is at fault — and §3.3 itself says these are
"architecture budgets, not predicted fit results."

**Not a claim about `perspuv_svc` being defective.** It is the largest register
consumer and 4.63× its budget line; whether that line ever described this block
is exactly the ambiguity in the mapping declared above.
