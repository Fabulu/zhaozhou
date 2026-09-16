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

## MEASURED: 100 MHz IS MET — `@g8a-timing5`

Source commit `fd78352c`, `rtlCleanAtHead: true`, `treeCleanAtHead: true`,
seed 1, 48 sources, digest `42a3e4504f07…`, `physical-top-ports`, 803.3 s.
**`status: ok`** — no rule violations, which includes the 100 MHz requirement.

| | Timing3 | Timing4 | **Timing5** |
|---|---:|---:|---:|
| status | `failed:structure` | `failed:structure` | **`ok`** |
| Fmax | 90.96 MHz | 94.46 MHz | **108.37 MHz** |
| setup WNS | −0.994 ns | −0.587 ns | **+0.772 ns** |
| setup TNS | −131.275 ns | −0.721 ns | **0** |
| hold WNS / TNS | +0.242 / 0 | +0.237 / 0 | +0.250 / 0 |
| ALMs | 13,195 | 12,940 | 13,076 |
| DSP | 30 | 30 | **30** |
| RAM blocks | 71 | 71 | **71** |
| memory bits | 92,964 | 92,964 | **92,964** |
| registers | 22,496 | 22,735 | 22,857 |
| negative paths (of 2,000) | 497 | 2 | **0** |
| rows below the 110 MHz band | unknowable¹ | 333 | **12** |

¹ the Timing3 export was truncated; see below.

The delta from Timing4 is **D2** plus the **quantiser numerator split**, and it
cost **+136 ALMs** and no DSP, RAM or memory bits at all.

### Which acceptance category this is

Against §2, precisely and not generously:

* Not **RED**. Every mandatory check passes at 100 MHz.
* **100 MHz GREEN** — and the reserve is real, not marginal: **+0.772 ns of
  worst-case setup slack and zero setup TNS**, with hold clean at +0.250/0.
* **NOT yet COMFORTABLE G8A.** That category needs a reported 110 MHz analysis
  with zero TNS, and 108.37 MHz is **1.63 MHz short**. Twelve paths stand in the
  way — six in the island, five in `zhao_raster_earlyz`, one in `v3own`.

So: the operating requirement is met with margin, and the comfortable target is
a named, bounded, twelve-path job rather than a second campaign.

### Why the gain is larger than the two paths that were fixed

Clearing a −0.587 ns path should buy about 100.7 MHz, because Timing4's
third-worst path sat at +0.069 ns. 108.37 MHz means the rest of the
distribution moved too, and the honest statement is that the batch did it
rather than either change alone: D2 took a six-way coordinate comparison out of
the abort fanout, the quantiser split removed a long tail from the resolve
cycle, and with the two hard paths gone the fitter had freedom it did not have
before. Attributing the whole 13.91 MHz to the split would be a claim this
measurement does not support.

## MEASURED: the Timing4 result

Source commit `be615625`, `rtlCleanAtHead: true`, `treeCleanAtHead: true`,
seed 1, 48 sources, digest `e7947623b3e3…`, `physical-top-ports`, 846.8 s.
Thirteen raw artifacts are retained under `reports/synthesis/blockpaths/` and
the row is in `reports/synthesis/zhao_block_fit.json`.

**There is no `@g8a-timing4` receipt, and there will not be one.** The reason is
an operator error worth keeping: I read the runner's *"the live tree cannot
reach this fit"* as licence to keep working inside its declared closure. It does
protect the MEASUREMENT — the numbers below come from the snapshot and are sound
— but the D2 work done during the run regenerated the G8A manifest, so the
runner's post-check found the manifest no longer matched the one it started from
and refused to stamp a receipt. Refusing was right: a receipt whose manifest
describes different sources than the fit consumed is exactly the
worthless-but-reassuring row the `rtlCleanAtHead` law exists for. **A snapshot
protects the fit from the tree; it does not protect the receipt from the tree.**

It cannot be written after the fact either, because the receipt tool requires the
row's commit to equal HEAD and HEAD has moved on. So the evidence for this row is
the row's own `sourceCommit` / `sourceDigest` / `rtlCleanAtHead`, plus the
retained `.sources.sha256`, whose **48 file hashes were each checked against the
manifest at `be615625` and all 48 agree**. Provenance is complete; the
convenience wrapper around it is missing.

| | Timing3 `3bf599d5` | **Timing4** | delta |
|---|---:|---:|---:|
| ALMs | 13,195 | **12,940** | −255 |
| Fmax | 90.96 MHz | **94.46 MHz** | **+3.50** |
| setup WNS | −0.994 ns | **−0.587 ns** | +0.407 |
| setup TNS | −131.275 ns | **−0.721 ns** | **+130.554** |
| hold WNS / TNS | +0.242 / 0 | +0.237 / 0 | — |
| DSP | 30 | 30 | 0 |
| RAM blocks | 71 | 71 | 0 |
| memory bits | 92,964 | 92,964 | 0 |
| registers | 22,496 | 22,735 | +239 |

### The verdict is RED, and the shape of the failure changed completely

94.46 MHz is below 100 MHz, so by §2 this is **RED**. It is not "nearly green"
and it is a long way from COMFORTABLE. Say that first, because the rest of this
section is good news and good news is what goes unaudited.

**The result that matters is TNS, not Fmax.** −131.275 ns became −0.721 ns: a
99.45% reduction. Timing3 had 497 negative paths out of 2,000 summarised rows
and a design that was broadly, diffusely slow. Timing4 has a total negative
slack of 0.721 ns against a worst path of 0.587 ns, which means **essentially
one path, plus a fraction of another, is left.** That is a different engineering
problem from the one the brief was written against: not "the machine is slow"
but "one cone is".

+3.50 MHz for −255 ALMs and no DSP, RAM or memory-bit change is the honest
summary of what eleven work packages bought.

### The two remaining paths are in a block no work package touched

2,000 summarised rows. **Two are negative. Both are in `zhao_raster_resolve`.**

| slack | skew | data | from → to |
|---:|---:|---:|---|
| **−0.587** | −0.487 | 9.920 | `u_resolve\|q_data_r[61]` → `u_resolve\|fifo_q[2][14]` |
| **−0.134** | −0.493 | 9.461 | `u_resolve\|q_data_r[45]` → `u_resolve\|fifo_q[2][4]` |

The third-worst path in the whole design is **+0.069 ns**.

Every one of the eleven Timing4 work packages targeted the texture island, the
combiner, AUX, the divider, the join or tile control. `zhao_raster_resolve` was
not among them, and it is now the entire 100 MHz gap. The packages did their
job and then handed the bottleneck to a block nobody was looking at.

Note the skew column before concluding anything about logic: these two paths
carry **−0.487/−0.493 ns of clock skew** against −0.108 to −0.150 on typical
rows. Roughly a third of the worst path's deficit is skew rather than data
delay. The data delay is still 9.920 ns of a 10.000 ns period, so this is a
genuinely long path AND a badly skewed one, and whoever takes it should check
which half is cheaper to buy back before rewriting the queue.

### The 110 MHz inventory, which Timing3 could not answer

The Timing3 export was truncated — its best slack was +0.582 ns, below the
110 MHz band's +0.909, so every exported row was inside the band and the true
population was unknown. This export's best slack is **+1.554 ns**, above both
bands, so the 2,000 worst paths bound the question completely.

| target | threshold | rows below | dominant endpoints |
|---|---:|---:|---|
| 100 MHz | +0.000 ns | **2** | resolve 2 |
| 110 MHz | +0.909 ns | **333** | island 146, attrgrad_dsp3 87, resolve 36, tile_pipe 30, texture_stage 29 |
| 115 MHz | +1.304 ns | **1,069** | island 670, attrgrad_dsp3 120, resolve 62, tile_pipe 59, texture_stage 43 |

This is the number the brief wanted and the Timing3 evidence could not supply.
**100 MHz is two paths in one block. 110 MHz is 333 paths across five**, and the
island's 146 and `zhao_raster_attrgrad_dsp3`'s 87 are the bulk of it — the
latter being a block the Timing4 packages also never touched.

The practical consequence for sequencing: 100 MHz is a small, targeted job.
COMFORTABLE at 110 MHz is a second campaign of comparable size to the one just
completed, and its inventory now exists to plan against.

### What is still owed, in nanoseconds

* **100 MHz** needs **+0.587 ns** on the worst path.
* **110 MHz** needs **+1.496 ns** on it, because the whole period shifts.

The 110 MHz inventory question from the Timing3 report is now answerable: this
fit exported the slack-bounded report, so the band is inventoried rather than
truncated. `reports/synthesis/blockpaths/zhao_raster_texture_v3_fit_top@g8a-timing4.setup.margin.rpt`
holds it.

### Two things this measurement does NOT say

* It says nothing about the **whole machine**. 12,940 ALMs is a subsystem
  number. The 30,000-ALM and 85-DSP targets are whole-machine and only
  G8C/production composition can answer them.
* It does not contain **D2**, which landed after the snapshot. The tile-control
  family's disposition stays open until a fit that includes it — attempt 2, from
  `6f9ab770`.

## Work-package ledger

| package | what it does | state |
|---|---|---|
| **M1 / stage S** | registered source/control capture between D and recipe selection | **landed** `005578fb` |
| **M2** | byte-wide exact finish arithmetic | **landed** `30e2594f` |
| **F** | finish boundary between M and row assembly | **landed** `005578fb`, retained per brief §7.6 |
| **M3** | S+F throughput calendar, and the two narrow bypasses only if measurement demands them | **partly landed** `2517a356` — §8.3 WB→Q bypass landed with its positive control; §8.4 still owed, see below |
| **O1** | registered accepted-owner admission and reservation events | **landed** `005578fb` |
| **B1** | BIL2 vertical product/base registered in B2, latency preserved | **landed** `005578fb` |
| **D1** | one-carry-chain divider magnitude | **landed** `c683cab7` |
| **D2** | attribute verdicts captured beside the join payload | **in progress** |
| **R1T** | reciprocal identity registered before the UVW lookup | **RTL landed, CONTROL OWED** — see below |
| **E1** | descriptor trust verdict at an existing payload boundary | **RTL landed, CONTROL OWED** — see below |
| **A1** | A0 fault facts, shared subtract/borrow in the AUX divider | **landed** `005578fb` |
| **Q1** | held recoverable wrapper clear replacing combinational feedback | **landed** `005578fb` |
| **margin export** | slack-bounded path report so the band is inventoried at the fit | **landed** `01ca1ac4` |
| **fit contract** | `@g8a-timing4` runner and receipt, baseline pinned to `3bf599d5` | **landed** `26016ce1` |

### R1T and E1 carry an unpaid control, and it is recorded as unpaid

Both RTL changes are in, and both rest on healthy-path evidence only:
`texture_uv_join_v2_directed` 30/30 on `tb_uv_join_v2_pair` including the new
trust-boundary section, and an island lint whose warning set is identical to
pristine HEAD.

`tests/mutants/zhao_texture_timing4_r1t_e1_mutants.sv` is committed and
**deliberately not registered in CMake**. Neither mutant has ever been built,
let alone fired. The first report of this work said they had been; checking
the build directory rather than the report showed two directories, `baseline`
and a *healthy* directed build, and no file on disk mentioning `ZHAO_TIMING4`
at all. The instrument was quoted without anyone watching it go off — the
exact thing the broken-instrument law names, and the correction came from the
author of the work.

Registering them now would put two tests in the suite whose green means
nothing: there is no inverse-polarity driver (no `PACKET_E_EXPECT_*`
expectation macro was written), no C++-side collision check, and for R1T no
determination of which of the four island drivers even reaches the fault —
below back-to-back reciprocal traffic the mutant and production are identical.
The shim's own header carries the full list of what is owed.

This is a **partial** disposition for `early-descriptor-ram` and for R1T's one
member of `other`. The structural change is real; the evidence that it is the
change that matters is not yet in hand.

### §8.4 is owed, and measurement now says why

M3 measured the S+F recurrence at 8 and the saturated rate at 0.878
phases/clk, 12% short of 1.000. The §8.3 WB→Q forwarding landed and moved the
recurrence 8→7 and lone 3-phase latency 27→25 — but the rate went 0.878→0.867,
which is to say **unchanged**. That is the useful result: it falsifies the
brief's stated root cause. The bottleneck is not the phase loop. It is the
**context recycle tail** — a context is freed only at the output handshake, so
DONE plus the completion read plus the response slots cost about five cycles.
§8.4 (WB-final-to-completion-read forwarding) is therefore owed on evidence
rather than on the brief's say-so, and the 0.133 phases/clk shortfall is its
acceptance number.

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
| early-descriptor-ram | 18 | -0.409 | **PARTIAL** — RTL landed, control owed | E1; healthy path 30/30, mutant never fired |
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
