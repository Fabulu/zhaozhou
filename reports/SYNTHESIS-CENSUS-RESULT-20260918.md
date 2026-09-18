# `zhao_prod_top@whole-console-sizing` — Analysis & Synthesis SUCCEEDED

The map stage completed at 20:51:41 after ~170 minutes. The fitter
(`quartus_fit`) is now running and will produce the ALM number.

## What it says

```
Analysis & Synthesis Status : Successful
Quartus Prime Version       : 17.0.2 Build 602 SJ Lite Edition
Top-level Entity Name       : zhao_prod_top
Family                      : Cyclone V
Logic utilization (in ALMs) : N/A          <- map does not report this; the FITTER does
Total registers             : 105,881
Total pins                  : 0
Total virtual pins          : 4
Total block memory bits     : 1,029,005
Total DSP Blocks            : 297
Total PLLs                  : 0
```

Source digest `de21d0f5a7d1` over 147 declared files, snapshotted — the live tree
could not reach this fit, so everything built today is outside it.

## Read this as the plan requires, not as a console number

`Zhaozhou_True_Console_Completion_Plan` §1.3 is explicit: **`zhao_prod_top` is
`RESOURCE_CENSUS_DISCONNECTED` and must not be relabelled a console** because its
independent stimulus inhibits merging and does not prove internal capacity
survives or that all states are live.

So this is **the OLD GROSS CENSUS of §14.1 — but measured by synthesis instead of
summed from per-module maps.** That distinction is the whole value of it.

It is also on the SIZING device `5CEBA9F31C7`, not the 41,910-ALM target
`5CSEBA6U23I7`. Fitted on a part chosen only so the measurement can complete.

## The DSP number is the headline, and our estimate was LOW

| source | DSP | how obtained |
|---|---:|---|
| `BUDGET_HEATMAP.md` | 185 | **summed per-module maps** |
| `CEILING-FRONTIER-RECONCILIATION` | 192 | summed |
| **this synthesis** | **297** | **one Quartus run over the whole selection** |

**297 against a 112-DSP target device is 2.65x over.** The per-module sums
understated it by ~60%, and I had already been treating 185 as the alarming
figure.

This matters for a reason beyond the number: I spent part of today writing that
"DSPs are not the binding constraint", struck it when the owner corrected me, and
recorded 185/112 as the real position. **The real position is worse than the
corrected version.** The error direction was consistent throughout — every
successive measurement of DSP has been larger than the estimate it replaced.

It also re-prices the DSP register's candidates: at 297 the shared projector's
33 DSP is ~11% of the overage, and PART.COLLIDE's ~10 is ~3.4%. Neither is close
to sufficient alone.

## Block memory: 1,029,005 bits, and this is the plan's own cautionary number

1,029,005 of the target's 5,662,720 memory bits is **18.2%** — the exact figure
plan §14.5 warns about: *"Physical M10K reserve must be measured, not inferred
from 18.2% logical bit occupancy."*

So the warning was written about this measurement, and it stands: **1,029,005
logical bits is NOT 100 M10Ks.** Physical occupancy depends on the shape of each
memory — depth, width, port count — and a 128 x 25 bank can consume a whole M10K
while carrying 3,200 bits. The fitter reports the physical count; the map does
not. Do not spend the "81.8% free" that this number appears to offer.

## What is NOT known yet

* **ALMs.** `N/A` at the map stage is normal for Cyclone V — the fitter assigns
  ALMs. The 41,910-vs-X question is still open and is the next thing to land.
* **Physical M10Ks**, for the reason above.
* **Timing.** No SDC was applied in this lane; no Fmax claim exists.
* **Whether it fits the target part.** It is not being fitted to
  `5CSEBA6U23I7`. A 297-DSP design cannot, since that part has 112 — so a target
  fit would fail on DSP alone, before ALMs were even considered.

## What this does NOT change

Everything built today — four particle blocks, the geometry group sequencer,
GEOM.LIGHT, POST.COMPOSITE, MEASURE.HISTOGRAM — is **outside this measurement**,
because the snapshot predates all of it and none of them is in any manifest. The
census describes the machine as it stood this morning, which is exactly what a
baseline is for.
