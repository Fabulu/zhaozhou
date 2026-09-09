# The island scored against the document that set its redline — and ALM is not the worst miss

2026-09-09. From reading `reports/islandrearchitecture4.md` (3,829 lines,
`de7e7e7b`, 2026-09-03), the document the 7,500-ALM redline comes from, which had
never been read on this lane. Read via `git show origin/main:<path>`.

All session the texture problem has been stated as *"the island fits at 10,837
against a 7,500 redline and there is no 3,336-ALM lever in it."* That is true and
it is the wrong headline.

## The full §21.6 gate, scored

`@g2-prod` — the shipping profile, `status ok`, `rtlCleanAtHead true`, commit
`82a4f317`:

| criterion | measured | target | hard | verdict | vs hard |
|---|---:|---:|---:|:--:|---:|
| ALM | 10,837 | 6,600 | 7,500 | **FAIL** | +3,337 (+44%) |
| registers | 16,285 | 8,000 | 9,000 | **FAIL** | **+7,285 (+81%)** |
| M10K | 49 | 56 | 64 | **PASS** | −15 (−23%) |
| DSP | 17 | 13 | 14 | **FAIL** | +3 (+21%) |
| Fmax MHz | 82.05 | 120 | 115 | **FAIL** | −33 (−29%) |

**Four of five fail, and the register breach is nearly twice the ALM breach in
proportional terms.** The ALM number is the one that has been quoted for days
because it is the one a lever hunt can act on. It is not the biggest.

### On comparing like with like

The resource rows are directly comparable: same island, same device
`5CSEBA6U23I7`, same tool Quartus Prime Lite 17.0.2.

The Fmax comparison is weaker and is stated as such. §21.6's 115 MHz is specified
for the full composition under a realistic cache/fill memory model with random
backpressure; `@g2-prod` is a fit of the island top. It is fairer here than the
usual leaf-fit caveat allows, because this island's worst-overall path was
measured equal to its worst-internal path — the boundary is no longer the
limiter, so 82.05 is an internal number rather than a virtual-pin artefact. It is
still not the composition the gate describes, and it should not be quoted as if
it were.

## The three-seed rule has never been met, or recorded

§21.6 requires **three seeds**, and the terrain brief's §15 repeats it: *"Run more
than one placement seed before claiming margin... publish every result, not only
the best. A best-seed 100.01 MHz headline is not robust evidence."*

**There is no seed field in any of the 133 rows of `zhao_block_fit.json`.** Not
recorded as one seed, not recorded as three — absent as a *field*.

**Correction to the first version of this sentence**, which said every Fmax here
is a single unrecorded-seed result. Seed information does exist for a few rows,
encoded in the **row label** rather than a field, and one block has a genuine
three-seed set:

| `zhao_raster_rcp24_svc` | Fmax |
|---|---:|
| `@p0b-s1` | 64.89 |
| `seed2` | 68.63 |
| `seed3` | 63.93 |
| | **spread 4.70 MHz (7.3%)** |

So the three-seed rule has been satisfied **once, for one block, out of 133
rows** — and never for the island.

That 4.70 MHz spread is itself useful, and it cuts both ways:

* It does **not** rescue the island. 82.05 against 115 MHz is a 33 MHz gap, an
  order of magnitude beyond seed noise, so the Fmax verdict stands regardless of
  seed.
* It **does** invalidate any near-threshold single-seed claim. `rcp24_v3` at
  100.95 MHz is one seed, and a 4.70 MHz spread puts a 100.95 result within
  noise of failing 100 MHz. Rescue-brief §7.1 already forbids using that number
  to argue the composed island closes 100 MHz; the seed spread is a second,
  independent reason the same number cannot carry that weight.

The schema gap is still real and is now fixed going forward — `run_block_fit.ps1`
records `fitterSeed` and `seedSource` — but a label-encoded seed is not nothing,
and saying so was wrong.

## What the memory-first remedy actually achieved

The document's §0 thesis:

> The important resource symptom is not merely the ALM total. It is **25,123
> registers against only 11 M10Ks**: state that belongs in memories and narrow
> token queues was implemented as wide flip-flop arrays and replicated payload
> FIFOs.

Measured against that:

| | prototypes (§0) | `@g2-prod` | budget |
|---|---:|---:|---:|
| registers | 25,123 | 16,285 | 9,000 hard |
| M10K | 11 | **49** | 32–56 expected |
| ALM | 15,749 | 10,837 | 7,500 hard |

**The remedy worked on the axis it was aimed at.** M10K went 11 → 49 and lands
inside its expected band with 23% to spare — the payload really is in memory now.
Registers fell 35% and ALMs fell 31%.

And then it stalled, exactly where the earlier measurement said it would:
`zhao_texture_v3own`'s per-owner status arrays are read **and written in full
every clock**, so they cannot be memory-backed at all — an M10K has two write
ports, not sixty-four. What remains over budget is the state the remedy
structurally cannot reach.

## The conflict, stated plainly

The per-component budget in §3.3 has **eleven** rows summing to 6,600 ALM:
FRAGROB + token fabric, RCP24 scheduler v2, perspective pair pipeline, binding
tables + TMU planner v2, synchronous texture cache v2, class router + decode
stores, transactional resident palette, serial bilinear channel engine, Mosaic
CSD pipeline, material combiner, AUX v2.

**None of them is per-owner transaction status.** The nearest is *"FRAGROB +
token fabric — 900 ALM, 1,200 reg, 14–20 M10K"*, and the shape of that line is
the whole point: the token fabric was budgeted at 900 ALM **because its state was
assumed to be memory-resident**, which is what the 14–20 M10Ks are for.

`zhao_texture_v3own` alone is **2,707 ALM** and its state is provably not
memory-resident.

So the 7,500 ALM and 9,000 register numbers are not arbitrary and they are not
wrong — they are **derived from an architecture model in which transaction state
lives in memory.** The implementation satisfied that model for payload and cannot
satisfy it for live status. The redline and the structure disagree, and no lever
inside the island reconciles them.

Rescue-brief §0.2: *"Surface a real unresolved contract conflict rather than
silently choosing the cheap side."* This is that conflict, and the cheap side
would be to quietly restate the redline as met, or to quietly move it.

## What the document does NOT provide

**No escalation clause.** There is no "if the island cannot meet 7,500, do X"
anywhere in 3,829 lines. Its status line reads:

> Status: IMPLEMENTATION ARCHITECTURE / **PROPOSED OWNER RULING**
> ...It becomes binding when the owner adopts or commits it.

So the numbers being failed are a *proposal* whose adoption is the owner's act,
and the document that set them declined to say what happens when they are missed.
That is the decision, and it is unambiguously Fabian's.

## What was already done without this document in the lane

Its §3.4 tripwires are already implemented in `design/fit_targets.yml`, cited by
section number — `S3.4: require DSP == 0 for the CSD variant` (measured 4),
`S3.4 FRAGROB: reject registers > 2,500`, plus `min_m10k` and `min_memory_bits`
minima on six and four targets respectively. An earlier pass absorbed §3.4 from a
copy read elsewhere. The rule the document is emphatic about —

> A fit that meets Fmax while violating its memory/DSP structure is not a pass.

— is enforced.

One `fit_targets.yml` comment already carries the `v3own` finding in the right
form: *"NOT a min_m10k rule. The fix is not 'put the arrays in memory'."*

## What this changes

Nothing about the sequencing: texture stays the immediate task and the reduction
programme stays deferred. Nothing was fitted, no RTL was touched, and no
acceptance criterion was moved in either direction.

What it changes is **what the owner is being asked to decide**. Not "can you find
3,336 ALM" — that question is closed and the answer is no. It is:

1. the register budget is missed by **81%**, worse than ALM, and by the same
   structural cause;
2. **M10K passes**, so the memory-first architecture is not failing — it is
   finished on the axis it could reach;
3. Fmax misses by 33 MHz against a target no fit in this repository can support
   anyway, because **no seed was ever recorded**;
4. the budget those numbers come from has **no line item for the state that
   overruns it**, and is a proposed ruling with no escalation path.
