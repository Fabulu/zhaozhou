# EARTH60 capacity — what one Field core can actually do in a frame

*Owner directive of 2026-08-25 (`reports/WordOfCaution`): do not declare Field
complete because one FIELD.SEQ instance reaches 100 MHz. Derive the end-to-end
live-terrain workload and show the frontier.*

**Status: the arithmetic below is MEASURED where it says measured and
PARAMETERISED where the number is a game-behaviour decision. Two inputs are
Fabian's and are deliberately left as variables rather than guessed.**

---

## The headline, stated first because it changes the architecture question

**The cost model everything was priced against assumes ONE INSTRUCTION PER
CLOCK. The engine that exists retires one instruction every SEVEN clocks, at
best.**

`spec/terrain_rules.md` §4.2 says the worst legal patch is
`16 programs × 1,089 vertices × ≤32 instrs = 557,568 field instructions`, and
calls that "~33% of a 1.67 M-cycle frame at the cost-model's 100 MHz placeholder
clock **and 1 instr/cycle**".

That last clause is the load-bearing one, and it is not what was built.
`zhao_field_seq` walks `Q_FETCH → Q_LATCH → Q_RD1 → Q_RD2 → Q_RD3 → Q_GATH →
Q_EXEC` per instruction, strictly sequentially, with no overlap between
instructions. Measured minimum: **7 clocks**.

So that same worst patch is not 33% of a frame. It is **557,568 × 7 =
3,902,976 clocks — 2.34 FRAMES for a single patch.**

Reaching 100 MHz does not fix this. It was never a clock problem.

---

## Measured inputs (from `tests/differential/field_seq_directed.cpp`, at `2727d85`)

Clocks per instruction, measured on the shipped RTL:

| op | clocks | | op | clocks |
| --- | ---: | --- | --- | ---: |
| MOV, ADD, MUL, MAD | 7 | | NOISE2 | 30 |
| DOT2, DOT3 | 7 | | CURVE | 30 |
| SIN, COS | 7 | | DCURVE | 27 |
| RCP | 16 | | SPLINE | 46 |
| RIDGE | 23 | | LEN2, LEN3, DIST2 | 49 |
| ROT2 | 27 | | RING | 55 |
| ROT3 | 28 | | NORMALIZE2 | 67 |
| | | | **NORMALIZE3** | **68** |

SIN and COS remain 7 clocks after waves 8 and 10 added two cycles of table
latency — absorbed by the operand-latch depth. ROT2/ROT3 went 24/25 → 27/28,
the two wait states those waves spent.

Fixed by spec: **1,089 vertices per patch**; **1,666,667 compute clocks per
frame** at 100 MHz / 60 Hz.

Fixed by contract: **TERRAIN.PATCH consumes `1 + n` clocks per vertex** with `n`
accepted field lanes (`design/contracts/TERRAIN.PATCH.md`).

---

## Cost of one patch-field pass

`1,089 vertices × I instructions × N clocks-per-instruction`

| I (instrs/program) | clocks | % of frame | affordable/frame | with 20% reserve |
| ---: | ---: | ---: | ---: | ---: |
| 4 | 30,492 | 1.8% | 54 | **43** |
| 8 | 60,984 | 3.7% | 27 | **21** |
| 16 | 121,968 | 7.3% | 13 | **10** |
| 32 (spec max) | 243,936 | 14.6% | 6 | **5** |

`N = 7` above is the **cheapest possible** instruction. A realistic terrain
program is not all-simple:

| program shape | N_avg | clocks | patch-fields/frame (20% reserve) |
| --- | ---: | ---: | ---: |
| 8 simple | 7.00 | 60,984 | **21** |
| 8 with one RCP | 8.12 | 70,785 | **18** |
| 8 with one CURVE | 9.88 | 86,031 | **15** |
| 8 with one RING | 13.00 | 113,256 | **11** |
| 8 with one NORMALIZE3 | 14.62 | 127,413 | **10** |

---

## The gap against the spec's own live-field budget

`spec/terrain_rules.md` §4.2 sizes the live-field case at **8 Erupts × ≤16
patches = ≤128 live-field patches**, inside a 256-patch composed-cache budget.

| | patch-fields/frame | cores needed for 128 |
| --- | ---: | ---: |
| 8 simple instrs | 21 | **6.1** |
| 8 with one CURVE | 15 | **8.5** |
| 8 with one NORMALIZE3 | 10 | **12.8** |

**One core is between 6x and 13x short of the spec's own worst case**, before
any reserve beyond 20% and before TERRAIN.PATCH's intake is counted.

---

## CORRECTION 2026-08-25: the TERRAIN.PATCH ceiling below is WRONG by 16x

**The 142% headline in this section is my error and it should not drive any
architecture decision.** It is corrected here rather than deleted so the
reasoning stays visible.

I read the spec's `≤128 live-field patches` as *128 unique patches each carrying
16 fields*. It is not. The derivation reads:

> 8 Erupts × ≤16 patches = ≤128 live-field patches

That is 8 Erupts each covering up to 16 patches — **128 patch-field
ASSOCIATIONS**. `MAX_PATCH_FIELDS = 16` is a different quantity: the per-patch
lane bound (8 Erupts + 8 Quakes). Multiplying them gives 2,048 associations,
**sixteen times the frozen donor case**.

The cost is `1,089 × (unique patches + associations)`:

| frozen case | clocks | % of frame |
| --- | ---: | ---: |
| max overlap — 8 unique × 16 fields | 148,104 | **8.9%** |
| 16 unique × 8 fields | 156,816 | 9.4% |
| no overlap — 128 unique × 1 field | 278,784 | **16.7%** |
| *my published figure (2,048 assoc)* | *2,369,664* | *142.2%* |

**So TERRAIN.PATCH is cheap headroom under the frozen case, not a second fatal
ceiling.** The ordered-pipeline probe is still worth having — it is ~61 ALMs per
stage and buys real margin — but it is not an emergency, and **Field remains the
priority**. Treating it as co-equal would have spent effort in the wrong place
on the strength of an arithmetic slip of mine.

The section below is left as written, with its numbers now known to describe a
scenario 16× beyond anything the donor derivation produces.

## TERRAIN.PATCH intake, which is a separate ceiling

`1 + n` clocks per vertex, 1,089 vertices per patch, 128 patches:

| n (overlapping fields/vertex) | clocks | % of frame |
| ---: | ---: | ---: |
| 1 | 278,784 | 16.7% |
| 2 | 418,176 | 25.1% |
| 4 | 696,960 | 41.8% |
| 8 | 1,254,528 | 75.3% |
| 16 (spec max) | 2,371,392 | **142% — over budget alone** |

So intake is comfortable at low `n` and is itself the binding constraint at high
`n`, independent of how many Field cores exist. **Replicating Field cores does
not help past `n ≈ 8`** unless the composer, its queues and the composed-height
storage widen too — which is precisely the warning in the directive.

---

## The two inputs that are NOT mine to choose

Everything above is parameterised on:

1. **Affected patches per frame in heavy combat.** The spec's 128 is a
   *correctness* bound on intake (`MAX_PATCH_FIELDS`), and it says so: "an
   intake correctness bound, not a per-frame affordability certificate". The
   number that matters for Earth60 is how many patches actually change in a
   frame during heavy combat.
2. **Overlapping Earth fields per vertex (`n`).** Drives both the sequencer cost
   (programs per patch) and the TERRAIN.PATCH ceiling above.

These are game-behaviour decisions. Picking values that make the budget close
would be exactly the kind of invented behaviour the standing instruction
forbids, so they stay variables and the frontier is shown instead.

---

## The architecture options, and which one the measurement favours

**A. Replicate Field cores.** 6–13 cores at ~4,500 ALM each is 27k–58k ALM
against a 41,910 ALM device. **Does not fit at the high end**, and does not
address the `1 + n` intake ceiling.

**B. Pipeline the sequencer for THROUGHPUT rather than frequency.** The engine
is sequential: seven states per instruction with no overlap. A pipelined walk
that retires one instruction per clock would deliver exactly the 1 instr/cycle
the cost model already assumes — a **7x** improvement, larger than anything the
clock work has produced, and it closes the gap with one core rather than six.

Waves 3–10 raised the clock from 8.59 to 58.99 MHz, which is 6.9x. **A
throughput fix is worth about the same again, and the two multiply.**

**C. Reduce the workload** — fewer instructions per program, fewer live patches,
or lower live-field overlap. An owner decision, not a hardware one.

**Recommendation: B before more of A or more clock work.** The clock is at 58.99
MHz and needs 1.7x; the throughput deficit is 7x and is the reason a
100 MHz Field core would still miss Earth60. Fixing the clock first and
discovering the throughput gap afterwards would be the expensive order.

---

## What this does NOT claim

* No composed-shell measurement. Every frequency here is a leaf fit, and the
  routing fraction reached 48% at wave 10 (`QUARTUS_GOTCHAS` §12).
* No statement about `TERRAIN.BAKE`'s backlog. The directive requires the
  pipelined-divider or fetched-stencil route to be chosen and measured before
  terrain sign-off; neither has been.
* `MAX_OP_CYCLES = 80` is treated here as a **liveness constant, not a product
  law**, per the directive. Earlier waves in `reports/FIELD_IR_ENGINE.md`
  reported "twelve clocks of headroom" as though it were a budget; that framing
  was wrong and is corrected here.
