# G1-C: variant A LOGIC2 is measured, and it fails its own bar

**Date** 2026-09-05
**Block** `zhao_texture_material_combine_v1`
**Fit** commit `4215c1c7`, 9,417 s, Cyclone V 5CSEBA6U23I7

`islandrearchitecture5.md` §15.5 does not merely describe variant A — it states
the condition under which it is preferred:

> **A. LOGIC2** — two exact 9x9/9x8 multipliers in ALM logic; zero DSP;
> **preferred if <= 800 ALMs and >= 125 MHz**.

That condition can now be evaluated instead of assumed.

## The measurement

```
alms      1994   of 41910        registers 1383
dspBlocks    2   of 112          ramBlocks    0
fmaxMhz  29.74   clk             virtualPins 596
```

| | measured | §15.5-A / §3.3 | |
|---|---|---|---|
| DSP | **2** | ≤ 2 (§3.4 tripwire) | **MET** |
| ALMs | **1,994** | ≤ 800 | **2.5× over** |
| fmax | **29.74 MHz** | ≥ 125 MHz | **4.2× short** |
| registers | **1,383** | 500 (§3.3) | 2.8× over |

**The DSP rule is met and nothing else is.** Variant A's own preference
condition is not satisfied, and it is not close on either half of it.

## Against the block it replaced

| | II=1, refuted | variant A LOGIC2 |
|---|---|---|
| ALMs | 494 | **1,994** |
| registers | 524 | **1,383** |
| DSP | **8** | **2** |
| fmax | 100.12 MHz | **29.74 MHz** |

Six DSP blocks were bought for **1,500 ALMs and 70 MHz**. On a device with 112
DSPs — of which the whole composed island uses 17 — that is a bad trade, and it
is the trade §15.5's threshold existed to prevent being made blindly.

**The DSP tripwire was still right to fire.** The II=1 block was refused for
writing eight independent `*` operators and assuming they would pack, which it
did and they did not. What the tripwire could not say is that the *replacement*
should be measured against its own bar before being adopted. It has been now.

## What is NOT concluded

**Not that §15.5 variant A is wrong.** This is one implementation of it. The
1,383 registers and 1,994 ALMs are not two multipliers — two 9×9 multipliers in
logic are on the order of 130 ALMs. The rest is the **microjob scheduler**: a
record file of depth 2 holding three samples, intermediates, a required mask, a
completed mask and an in-flight mask, plus a two-lane issue loop that scans
every record and every job every cycle. That scheduler is what §15.3 asks for
and it is not free.

**Not that the arithmetic is wrong.** `material_combine_v1_diff` passes 16
checks against `zref::material::combine`, including exact per-recipe job counts,
out-of-order retirement and back-pressure. The block is correct and expensive.

## The critical path, read — and it is the COUNTERS

```
From   Decoder5~9_OTERM21DUPLICATE
To     Add46~41_OTERM2248_OTERM2772
Slack  -23.624 ns    (10 ns target -> 33.6 ns period -> 29.74 MHz)
```

A decoder feeding a wide adder, on all 1,820 summarised paths.

**The datapath cannot be responsible.** Every datapath signal in this block is
7, 8 or 9 bits; the only 32-bit signals are the seven counters. A 9-bit add does
not make a 23 ns path.

**The counters can.** `jobs_inc[rec[i].recipe] = jobs_inc[rec[i].recipe] + 1` is
a BLOCKING add on a DECODED index, inside a nested loop over `RECS x 7` jobs. So
synthesis must serialise up to fourteen conditional 32-bit adds into one carry
chain — decode, select, add, repeat — which is exactly `Decoder -> Add`.

**Fixed by narrowing the increment, not the counter.** At most two jobs issue
per cycle, so the per-cycle increment never exceeds 2 and needs two bits. The
chain becomes 2-bit adds; only the commit stays 32-bit, where the eight adds are
independent and parallel. `material_combine_v1_diff` still passes 16 checks,
including the exact per-recipe job counts — the counters are narrower per cycle
and still count the same totals.

**The irony is worth keeping.** These are the §15.4 counters that caught the
double-issue bug this morning, and they are why the block did not close timing.
A re-fit will say how much of the 29.74 MHz they were worth; **no prediction is
offered**, because the island's obvious explanation was worth 4 MHz of 36 and
the honest lesson from that is not to guess the size of an effect before
measuring it.

### And a measurement tool got it wrong again

The script that split the island's paths into "starts at a virtual pin" versus
"starts inside the design" — the split that produced the island's decisive
finding — classified **all 1,820** of this block's paths as virtual-pin starts.
It keys on `:` or `|` in the node name, which is hierarchy punctuation. A
single-module fit has no hierarchy, so every internal node looked like a pin.

The tool was right for the island and silently wrong here, and it would have
supported the flattering conclusion that this block's timing is all boundary
artefact. It was caught only because `Decoder5` and `Add46` obviously are not
pins.

**Not a comparison against the island's 69.05 MHz.** Different design, different
constraint set, 596 virtual pins on a block with 744 synthesis registers.

## The decision this hands over

§15.5 names two variants and a rule for choosing between them:

> **B. DSP2_PACKED_OR_EXPLICIT** — explicit vendor primitive/IP only; maximum
> two DSP blocks for the whole combiner; **accepted only if the fitter proves
> the count and composition improves.**

Variant A has now been fitted and does not meet its condition. The architecture's
own procedure therefore points at building B and comparing — B is capped at the
same 2 DSP, so the comparison is purely ALMs and fmax.

Three options, and this report picks none of them:

1. **Build variant B** and compare. The architecture asks for exactly this and
   the cap makes it a fair test.
2. **Attack the scheduler, not the multipliers.** The record file and the
   two-lane scan are where the 1,994 ALMs are, not in the arithmetic. A
   depth-1 record or a narrower issue scan may recover most of it without
   touching either variant's multiplier choice.
3. **Reconsider whether 2 DSP is worth 1,500 ALMs** on a device using 17 of 112.
   That is a re-reading of §3.4's tripwire in the light of a measurement it did
   not have, and it is an owner call, not a quiet amendment.

Nothing is blocked on the answer. The refuted `zhao_texture_combine` stays in
the tree, its manifest row and `texture_combine_diff.cpp` with it, because the
replacement has now been measured and **has not earned the deletion**.
