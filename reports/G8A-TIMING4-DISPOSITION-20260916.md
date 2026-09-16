# G8A Timing4: work-package ledger and path-family dispositions

**Status: batch in progress, not frozen, not fitted.** Nothing here is a timing
measurement. The last physical result remains Timing3 at 90.96 MHz, red.

This exists because the owner Timing4 brief
(`reports/Zhaozhou_G8A_Timing4_110MHz_Architecture_Brief.txt`, §18) requires every
negative family to carry an explicit disposition *before* the batch is frozen,
so that "other: 94 rows" cannot be quietly counted as addressed by fixing the one
path that family was named after.

## Acceptance categories in force

From the brief §2, and they are not interchangeable:

* **RED** — any mandatory check fails at 100 MHz, or any functional/ownership/RAM
  gate fails.
* **100 MHz GREEN, LIMITED MARGIN** — canonical constraints pass without the
  reserve below. Real progress; report it as that and not as comfortable.
* **COMFORTABLE G8A** — the same physical design also passes an explicitly
  reported 110 MHz analysis at all applicable corners, zero setup/hold TNS, no
  new exclusions.
* **DESIGN AIM** — principal data stages developed toward ~115 MHz capability.
  An aim, not a forecast.

The operating requirement is still 100 MHz. 100.01 MHz is not the answer anyone
asked for.

## What the 110 MHz target actually costs

The worst Timing3 path needs about **1.903 ns**, not the 0.994 ns that would
merely reach 100 MHz, because the whole period shifts. Under unchanged clock
terms the data budget on that path falls from roughly 9.08 ns to about 7.18 ns.

And the inventory for that target is **not yet knowable** from the Timing3
evidence: the best slack in the entire 2,000-row export is +0.582 ns, so every
exported row is inside the 110 MHz band and the export is truncated. See the
"110 MHz inventory" section of `G8A-TIMING3-DSP-PATH-REPORT-20260915.md`. The
Timing4 fit now exports a slack-bounded report so this is answerable once.

## Work-package ledger

| package | what it does | state |
|---|---|---|
| **M1 / stage S** | registered source/control capture between D and recipe selection | **landed** `005578fb` |
| **M2** | byte-wide exact finish arithmetic | **landed** `30e2594f` |
| **F** | finish boundary between M and row assembly | **landed** `005578fb`, retained per brief §7.6 |
| **M3** | S+F throughput calendar, and the two narrow bypasses only if measurement demands them | **in progress** |
| **O1** | registered accepted-owner admission and reservation events | **landed** `005578fb` |
| **B1** | BIL2 vertical product/base registered in B2, latency preserved | **landed** `005578fb` |
| **D1** | one-carry-chain divider magnitude | **landed** `c683cab7` |
| **D2** | attribute verdicts captured beside the join payload | **in progress** |
| **R1T** | reciprocal identity registered before the UVW lookup | **in progress** |
| **E1** | descriptor trust verdict at an existing payload boundary | **in progress** |
| **A1** | A0 fault facts, shared subtract/borrow in the AUX divider | **landed** `005578fb` |
| **Q1** | held recoverable wrapper clear replacing combinational feedback | **landed** `005578fb` |
| **margin export** | slack-bounded path report so the band is inventoried at the fit | **landed** `01ca1ac4` |
| **fit contract** | `@g8a-timing4` runner and receipt, baseline pinned to `3bf599d5` | **landed** `26016ce1` |

`B2`/`BIL2T`, the four-slot bilerp fallback and its complete/refill island work
are **not selected**. They are the explicit fallback if the preferred three-slot
retime cannot meet the reserve, and adopting them early would cost a cycle per
bilinear sample at the island boundary for nothing.

## Negative-family dispositions

Against the 497 negative rows of the Timing3 census. Categories are the brief's.

| family | rows | worst | disposition | by |
|---|---:|---:|---|---|
| owner-mask-lifetime | 196 | -0.848 | FIXED STRUCTURALLY | O1 — the fault keeps its direct admission hold; the accepted-owner *event* is what became registered ahead of the 64-entry scoreboard |
| other | 94 | -0.533 | **PARTIAL — see below** | R1T names one member; the remaining 93 are not thereby closed |
| owner-control | 50 | -0.582 | EXPECTED TO IMPROVE INDIRECTLY | O1 removes one contributing chain; several distinct enable/payload cones remain and are re-checked post-fit |
| combine-product | 37 | -0.843 | FIXED STRUCTURALLY | M2 shortens the finish; F splits row assembly from product finishing |
| tile-control | 30 | -0.375 | FIXED STRUCTURALLY (pending) | tile abort/metadata-enable decoupling, in progress |
| uv-join-lifetime | 26 | -0.844 | FIXED STRUCTURALLY | O1, same mechanism as owner-mask-lifetime |
| bilerp-dsp2 | 25 | -0.674 | FIXED STRUCTURALLY | B1 registers the vertical product and finishes from held registers |
| early-descriptor-ram | 18 | -0.409 | FIXED STRUCTURALLY (pending) | E1, in progress |
| attribute-dsp3 | 12 | -0.366 | FIXED STRUCTURALLY | D1 collapses three dependent 98-bit adds into one |
| bank-sres-write-enable | 7 | **-0.994** | FIXED STRUCTURALLY | stage S — the worst path; source planes now terminate at S instead of reaching a DSP input register |
| fragment-expand | 2 | -0.738 | FIXED STRUCTURALLY | A1 registers the A0 fault facts that fed the AUX endpoint |

### The `other` family is the honest gap

94 rows, one named example (RCP token → UVW), and no basis for assuming the other
93 share its cause. Fixing the example is R1T's job; **classifying the remainder
is not done**, and the brief is explicit that a generic label must not absorb
them. Two things follow:

1. The Timing4 batch is scoped against a complete 100 MHz inventory in which one
   family of 94 rows is only partly understood. That is a known, stated weakness,
   not a claim of coverage.
2. The first Timing4 fit's own reports are what resolve it. The margin export
   plus a full-detail path report give per-row launch/endpoint, net-versus-cell
   delay and endpoint kind, which is what separates an enable cone from an
   arithmetic cone.

Anything still unexplained after that fit is **UNRESOLVED** and is inspected
before a second placement run is requested — not relabelled.

## Stop conditions

* Do not restore the old multipliers, relax the clock, add false paths or
  multicycle exceptions, clear ownership on faults, or fit one small correction
  while other known negative cones remain.
* 30 physical DSPs stay the target. 31 or 32 is diagnosed, not accepted.
* The 71 RAM blocks and 92,964 payload bits are reconciled, not silently shifted
  into fabric. A changed memory count needs a named reason.
* Device, seed 1, canonical 100 MHz constraints and the retained QSF effort
  settings are inherited unchanged. A clock-network or retiming experiment is a
  separate, identified run with its own reason and result.
* Preliminary measurements are disclosed as they exist. "ALMs known, timing
  pending" is a legitimate report; withholding both is not.
