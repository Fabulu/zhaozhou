# §12 — the owner experiment's memory, reconciled per instance

*2026-09-07. Master recovery handoff §12.2: "The measured 17 M10Ks / 20,640 bits
must be reconciled **per instance** rather than explained by a guessed missing
bank." This is that walk. Analysis only — no RTL touched; `zhao_texture_v3own`
is inside the running fit's closure and the live-tree trap (QUARTUS_GOTCHAS §11)
forbids editing it, not reading it.*

---

## Which instance §12.2 is talking about

`zhao_texture_v3own@v3-full` is the **only** row in the 103-block ledger
measuring exactly 17 M10K / 20,640 bits. Its ALM figure, **5,678**, is
independently the number §12.4 warns about by name — *"do not silently edit
max_alms to 5,700 because 5,678 happened to be measured."* Two separate
identifiers agree, so the instance is identified rather than assumed.

Fitted at source `0f5ce601`, which is **before** today's registered-credit and
fence changes. This is therefore the *before* picture on the memory axis, and
the fit running now will move it.

## The handoff's own arithmetic, checked by hand first

Nothing downstream is worth computing if the premises do not add up:

| claim | check |
|---|---|
| §12.2 persistent payload = 16,896 | 4,096 + 7,680 + 2,560 + 2,560 ✓ |
| §12.2 ready bodies = 2,688 | 3 × 64 × 14 ✓ |
| §12.2 combined logical = 19,584 | 16,896 + 2,688 ✓ |
| §12.3 profile total = 69 | the eighteen rows sum to 69 ✓ |
| §12.3 derivation | 64 − 3 + 2 + 6 = 69 ✓ |

All five hold.

## The per-instance walk

`zhao_texture_v3own` instantiates **nine** `zhao_texture_v3bank` bodies — six
directly, three more one level down inside the `zhao_texture_v3rq` queues, which
is why a flat read of the top misses them:

| instance | width | depth | bits | §12.2 family |
|---|---:|---:|---:|---|
| `u_ctx` | `CTXW` 64 | 64 | 4,096 | context64 |
| `u_sres[0..2]` | `RESW` 40 | 64 | 7,680 | three sample-result40 |
| `u_ares` | 40 | 64 | 2,560 | AUX-result40 |
| `u_fres` | 40 | 64 | 2,560 | final-result40 |
| `u_rq_tmu/u_body` | `OWNERW` 14 | 64 | 896 | ready body |
| `u_rq_aux/u_body` | 14 | 64 | 896 | ready body |
| `u_rq_init/u_body` | 14 | 64 | 896 | ready body |
| **total** | | | **19,584** | |

Two things this settles that the handoff left open:

* **The payload families map one-to-one onto instances**, 16,896 bits exactly.
* **The "old three 64×14 ready bodies" are named.** §12.2 refers to them only by
  shape. They are `u_rq_tmu`, `u_rq_aux` and `u_rq_init` — each a
  `zhao_texture_v3rq` whose entire storage is one nested `v3bank` at
  `WIDTH = OWNERW = SLOTW + GENW = 14`. So the candidate profile's "removes
  ready bodies" is a concrete instruction about three named instances.

The walk is confirmed from a second, independent place: `design/fit_targets.yml`
already gates this block at `min_m10k: 9` and `min_memory_bits: 19584`, which is
the instance count and the bit total the walk derives.

## The remainder, left as a remainder

    measured   20,640 bits / 17 blocks
    walked     19,584 bits /  9 instances
    REMAINDER   1,056 bits /  8 blocks

**1,056 bits and 8 blocks are unattributed.** They are recorded that way and not
explained, for the reason §12.2 gives and the reason this session already
learned the hard way: a figure reported earlier today as "14,132 in named blocks"
was a *subtraction presented as a sum*, and a subtraction can never show a
remainder because it defines one away. `entity_census.py` prints an UNACCOUNTED
line now for exactly this. So does this report.

**A candidate was tested and rejected.** §12.3 names a "demo COMBINE 174-bit and
output120-bit packet body". The COMBINE width is confirmed against the RTL —
`cq_own_q`(14) + `cq_s0/s1/s2_q`(3×40) + `cq_ax_q`(40) = **exactly 174**, so the
handoff's figure is right. But those bodies total 174×4 + 118×4 = **1,168 bits**,
not 1,056, so they do **not** close the gap. Publishing them as the explanation
would have been numerology dressed as attribution — the shape §12.5 forbids:
*"no grand sum of source operator counts is a fitted ALM/DSP count."*

Note also that §12.3's "output120" is a **16-deep global plane**, not this
4-deep local queue whose entry is 118 bits (14 + 40 + 64). They are different
structures that happen to have similar widths; they are not each other.

## What actually closes this, and it is running right now

The instrument is the fitter's own per-RAM summary, and **no such table exists on
disk for this block** — `blockfit.fit.rpt` only entered
`tools/quartus/run_block_fit.ps1`'s harvest list today. `RAM-INFERENCE-SCAN.txt`
is a *source* scan and cannot substitute for it under §12.5.

The `zhao_texture_v3own` fit in progress will therefore produce the **first**
harvested per-RAM table for this block. When it lands, the remainder above is a
lookup, not an investigation.

## The ALM position, stated plainly

`max_alms: 1800` in the gate is §12.4's owner/control allocation. Measured
**5,678 — 3.15× over.** §12.4 anticipates precisely this case and says what to
do with it:

> A useful intermediate can fail an allocation and still be worth retaining. Its
> status must say precisely that. If a correct reduced owner remains above 1,800,
> reconcile the whole island before deciding another architecture change or
> asking for a resource reallocation.

So the owner is **retained and failing its allocation**, and the next step is the
whole-island reconciliation, not a relaxed gate. This is the third gate this
session that a comfortable reading would have relaxed and evidence did not
support; the other two were `residency_v2`'s 17-bit shortfall and the `max_m10k`
figure corrected to 30 with reasoning rather than to fit a measurement.

## What this does not claim

* Not that 17 blocks is wrong. It is the fitted number; only its *attribution*
  is incomplete.
* Not that removing the three ready bodies saves three blocks. Their 2,688 bits
  are 2.6% of one M10K's capacity apiece; §10.4's shared rows, not per-array
  conversion, are where blocks come back.
* Not a prediction of the running fit. The registered credit added a counter and
  the fence added a phase register; both are logic, and neither was expected to
  move block memory.


---

# CLOSED — the fitter's RAM table arrived, and the remainder is fully attributed

*Later the same day. The `zhao_texture_v3own` fit finished at 4,213 s and is the
**first** fit of this block to harvest `blockfit.fit.rpt`, which is exactly the
instrument this report said was needed.*

The Fitter RAM Summary lists **15 RAMs totalling 20,640 bits / 17 M10K**, which
matches the ledger row to the bit and to the block.

| group | bits | M10K |
|---|---:|---:|
| the 9 `v3bank` instances walked above | 19,584 | 10 |
| `cq_s0_q`, `cq_s1_q`, `cq_s2_q`, `cq_ax_q` — 4×40 each | 640 | 4 |
| `oq_ctx_q` — 4×64 | 256 | 2 |
| `oq_res_q` — 4×40 | 160 | 1 |
| **total** | **20,640** | **17** |

**The remainder was 1,056 bits and it is 1,056 bits.** The block count reconciles
too: the instance walk is **10** M10K, not the 9 the gate names, because `u_ctx`
is 64 bits wide against the M10K's 40-bit port limit and takes two slices —
§12.1 says this in advance (*"A 64-bit context on an ordinary 40-bit-wide
simple-dual-port mapping needs two, not one"*), and the fitter confirms it.

## The rejected hypothesis was right in kind and wrong by exactly the amount that mattered

This report proposed the COMBINE and output packet bodies as the explanation,
computed 1,168 against a remainder of 1,056, and **refused to publish it**. The
measurement says the bodies *are* the answer — but only the wide fields:

* the four 40-bit `cq_*` and the 40/64-bit `oq_*` fields became RAM;
* **`cq_own_q` and `oq_own_q` did not.** At 14 bits × 4 they stayed in fabric.

`14×4 + 14×4 = 112`, and `1,168 − 112 = 1,056`. The arithmetic that looked like
numerology was one term too generous, and the term it was wrong about is the one
a bit-count model cannot predict — whether the fitter finds a field worth a
block. **A sum over declared fields is not a fitted memory map**, which is §12.5's
rule, and this is the cleanest demonstration of it available.

## The finding worth acting on: 41% of the blocks hold 5% of the bits

**Seven of seventeen M10Ks hold 1,056 bits.** Each is a **four-entry** queue
occupying a whole 10,240-bit block — about **0.7% utilisation**, and `oq_ctx_q`
spends *two* blocks on 256 bits. The fitter's own column says why they did not go
to MLABs: *"Fits in MLABs: No — Unsupported Port Usage"*.

That is precisely what §12.1 warns about — *"Independently timed
readers/writers determine memory geometry as much as total bit count does"* — and
what §5.6 says about physical mapping: *"A spare M10K is not automatically
cheaper if it adds inconvenient routing or poorly utilized width slices."*

**This changes the shape of the M10K question.** §12.3's 69-block proposal is
built from payload geometry; it does not anticipate small in-flight queues each
claiming a block. Any island budget that counts planes and forgets four-deep
queues will be short — and here that error would be 7 blocks in one module
alone. Worth carrying into the whole-island reconciliation rather than
rediscovering per block.

**Not proposed here:** which of the seven should be fabric instead. That is a
port-usage question per queue, it interacts with the read-during-write mode the
table records, and §5.6 asks for both mappings compared *"under the same workload
and constraints"* rather than argued.
