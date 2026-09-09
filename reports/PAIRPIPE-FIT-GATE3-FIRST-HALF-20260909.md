# The pair-pipe fitted: 794 ALM, 820 registers, 119.25 MHz — better than the bracket, and not yet like-for-like

2026-09-09. FIT GATE 3, first half. `zhao_raster_perspuv_pairpipe@regfit`,
commit `20f8f8cc`, `rtlCleanAtHead: true`, seed 1 (`shell_fit QSF pinned SEED`),
2,320 s. Fitter Status: **Successful**.

| | measured | §8.8 rule | verdict |
|---|---:|---:|:--|
| ALMs | **794** | ≤ 900 | **PASS** |
| registers | **820** | ≤ 700 | BREACH |
| RAM blocks | 2 | ≤ 1 | BREACH |
| DSP | **6** | ≤ 6 | **PASS** |
| Fmax | **119.25 MHz** | ≥ 125 across three seeds | one seed; cannot settle |
| block memory bits | 1,280 | — | — |

## It beat my own pre-registered bracket

Before the run I bracketed the fitted register count at **920–1,629**, from the two
map/fit ratios in the ledger. Actual: **820 — below the range.** The estimate was
too pessimistic in both directions, because `perspuv_svc`'s own map/fit ratio
(1.045) was the closer analogue and even that overstated it: the candidate's map
said 961 and its fit says 820, a ratio of 1.17 the *other* way.

And the two columns I said were unmeasured and would not predict — ALM and Fmax —
both moved in the good direction. **794 ALM passes a budget the block it replaces
misses by 210%.**

## A LABELLED ROW'S `ok` IS NOT A GATE PASS, and I pre-registered the wrong stamp

I predicted `status: failed:structure`, because 820 registers breaches the target's
`max_registers: 700`. The row says **`status: ok`** and carries no
`ruleViolations` field at all.

The reason is the trap already recorded in `CLAUDE.md`: *"`ruleViolations: []` on a
labelled row is silence, not compliance: labelled rows are never rule-checked."*
Counted in the current ledger:

```
labelled rows    43   with ruleViolations:  0
unlabelled rows  95   with ruleViolations: 12
```

I used `-RowLabel` because `-MapOnly` requires it and I carried the habit into a
full fit. So `ok` here means only *the fit completed*. The rule verdicts in the
table above are **hand-checked**, not reported by the tool.

That is worth more than the arithmetic: had I quoted `status: ok` as a gate pass,
this document would have claimed the candidate meets a budget it breaches on two
of four rules.

## The comparison is NOT yet like-for-like — this is the half of the gate that exists to say so

| | commit | ALM | registers | Fmax |
|---|---|---:|---:|---:|
| `perspuv_svc` (standing fit) | `9c787e4a` | 1,886 | 3,216 | 105.19 |
| `perspuv_pairpipe@regfit` | **`20f8f8cc`** | 794 | 820 | 119.25 |

**Different commits.** `design/fit_targets.yml:471` registered the candidate
precisely so the gate would measure it "against a FRESH svc row from the same
commit — the standing svc row above predates today's tree, and comparing against a
stale measurement is the error this file already documents twice."

So the deltas below are **provisional** and are labelled as such:

* ALM −1,092 (−58%)
* registers −2,396 (−74%)
* Fmax +14.06 MHz
* DSP unchanged at 6, as predicted and already scored
* RAM blocks +1

`zhao_raster_perspuv_svc@gate3-fresh` is running now.

### UPDATE, before it landed: the standing svc row is NOT stale in the sense that matters

The target's comment worried that the standing row "predates today's tree". What
actually matters is whether the **source** moved, not whether the commit did:

```
fpga/rtl/raster/zhao_raster_perspuv_svc.sv
  blob at fit commit 9c787e4a : 95d7ff17547268a1d272fabc6a8bd6d6cb11f118
  blob at HEAD               : 95d7ff17547268a1d272fabc6a8bd6d6cb11f118
  commits touching it since   : 0
```

**Identical.** The standing row measures today's source exactly.

`tools/quartus/check_fit_rules.ps1` already implements the right notion —
`Get-RowStaleness` counts commits **to the source file**, which is why its run
tagged `zhao_texture_island_v3_top` as `[STALE: 12 commits]` and left
`zhao_raster_perspuv_svc` untagged. The distinction between *the commit moved* and
*the source moved* is easy to conflate, and conflating it produces two opposite
errors: trusting a genuinely stale row, or refusing a perfectly good one. Here it
was the second.

So the deltas above are **like-for-like in the only sense that decides it — the
same RTL** — and the running fit is best understood not as a correction but as a
**reproducibility control**: same source, same tool version, same pinned seed, a
different day and a different invocation. If it returns 1,886 / 3,216 / 105.19 the
comparison is confirmed and the tool is shown deterministic across invocations. If
it does not, that is a finding about the tool rather than about either block, and
a more valuable one.

## If the fresh row confirms it

Against the island's own §21.6 overages — 3,337 ALM and 7,285 registers — a
−1,092 / −2,396 swap is **33% of the ALM overage and 33% of the register overage
at once**, from one module-name change. That would make it far and away the
largest lever found, and it would do something no other candidate does: move both
failing area criteria together, while *improving* Fmax rather than trading it.

What it still would not do: close either gate. 33% is not 100%, the register
breach is systemic across 9 of 11 components, and the composed island may not
reproduce a leaf result — this fit carries **300 virtual pins**, and the island
instantiates the block with real neighbours.

## What is unambiguously established now

* The candidate **fits**, cleanly, at today's commit, with a real receipt.
* It is **6 DSP**, confirming the pre-registered structural prediction a third time.
* It is **faster in isolation than the block it replaces was in isolation**, even
  across different commits — 119.25 against 105.19, a gap far larger than the
  4.70 MHz seed spread this repository has measured.
* Its ALM figure **passes** §8.8's budget, which `perspuv_svc` has never done.
