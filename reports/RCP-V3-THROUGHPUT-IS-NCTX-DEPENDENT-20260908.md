# RCP V3's throughput advantage needs NCTX=16; the island runs NCTX=8

2026-09-08. Follows `reports/RCP-V3-SWAP-HAS-NO-LIKE-FOR-LIKE-20260908.md`, which
established that the two fit rows behind the swap recommendation differ in NCTX,
TOKW and M10K usage. This settles the functional half **without spending a fit**,
and it changes the answer.

## The measurement

`tb_rcp24_v3_pair` is already parameterised, so the island's profile is a
verilate flag, not a rebuild. Same RTL, same stimulus, same oracle; only NCTX
and TOKW differ. Four points, all measured:

| NCTX | saturated clk/recip | vs serial (6.96) | arithmetic checks |
|---|---|---|---|
| **8** — the island's | **5.78** | 17% faster | all pass |
| 10 | 4.69 | 33% | all pass |
| 12 | 4.38 | 37% | all pass |
| 16 — the profile that was fitted | **4.07** | 41% | all pass |

**Halving the contexts from 16 to 8 costs 42% of the saturated throughput**, and
the tile drops below its own declared acceptance threshold of 4.6 clocks. The
threshold is first met at **NCTX = 12**.

### Two magnitude predictions, both falsified

Worth recording, because the docket already scores this exact distinction and
the pattern held again:

1. The source comment says *"the ten-clock feedback loop"*, so the first gate
   used NCTX ≥ 10 as the sufficiency threshold. **NCTX=10 measures 4.69** and
   misses 4.6. There is no cliff at ten.
2. Fitting `rate = 4L/NCTX` to the two under-provisioned points gave L = 11.56
   and 11.73 clocks — agreeing to 1.4%, which looked like a law. It predicted
   4.07 at NCTX=12. **NCTX=12 measures 4.38.** Back-solving gives L = 13.1, so
   the "law" was a curve fitted to two points that happened to lie near each
   other.

What survived both times is the **structural** claim: throughput is
context-limited below the loop depth and saturates above it. What failed both
times is the number — once read off a comment, once fitted to two samples.
*Predict what moves, not how far.*

The gate in the test is now `NCTX >= 12`, which is where the criterion is
actually met rather than where the comment implied it would be. Below that the
test asserts the only claim still meaningful — that the tile beats the serial
reference it would replace — and prints the rate. Applying the NCTX=16 number to
an NCTX=8 run would have been the same mismatched comparison this whole report
is about.

## What this is, and what it is not

It is **not** a defect. Every arithmetic check passes at both profiles — the
boundary and random multiplier sweeps, the negative-correction coverage (49.94%
of 250,000 cases), all 24 exponents. The tile computes correctly at eight
contexts. It simply cannot keep its pipeline full with half the contexts to
interleave, which is what a context-parallel reciprocal is *for*.

It **is** a direct contradiction of the case for the swap. Against the serial
reference the advantage collapses from 41% faster (4.07 vs 6.96) to 17% (5.78 vs
6.96) — and the remaining 17% has to pay for 8 M10Ks and +377 registers that
`zhao_raster_rcp24_svc` does not spend.

To get V3's advertised behaviour the island would have to run it at **NCTX=16**,
which is *twice the contexts svc currently carries*. The comparison I recorded
presented that as a 7-ALM saving.

## Why this was cheap and the fit was not

The failed `@island-profile` fit burned 38 seconds and then two weeks of
inherited belief because nobody separated "the block rejects NCTX=8" from "the
tool mangled the parameter". It was the tool. But the *functional* question —
does this tile work, and how fast, at the island's profile — never needed
Quartus at all. One `VERILATOR_ARGS -GNCTX=8 -GTOKW=14` target answers it in
under a minute.

The habit worth keeping: **when a swap is gated on a fit, ask first whether the
cheap half of the question is a simulation.** Here the simulation is decisive
and the fit would only have priced a candidate that had already lost.

## Standing recommendation

The RCP V3 swap is **not recommended** at the island's current profile. Not
refused on principle — the arithmetic is sound and the DSP 6 → 3 halving is real
— but it does not buy what it was said to buy at NCTX=8, and buying it at
NCTX=16 is an area decision nobody has costed.

If it is pursued, the sequence is:

1. Fix the `-TopParameters` quoting guard in `run_block_fit.ps1`.
2. Fit `zhao_raster_rcp24_v3` **and** `zhao_raster_rcp24_svc` at the *same*
   profile — **NCTX=12, TOKW=14**. Twelve rather than sixteen: twelve is the
   measured point where V3 first meets its throughput criterion, so it is the
   cheapest profile at which the swap is worth anything, and four spare contexts
   nobody needs is exactly the sort of unpriced area the original comparison hid.
   The standing svc row is NCTX=8/TOKW=8 and is not a comparator for anything.
3. Report ALM, registers, DSP **and M10K** together.

`raster_rcp24_v3_island_profile` is in the fast lane and `raster_rcp24_v3_nctx10`
and `_nctx12` are nightly, so the whole curve stays measured rather than
remembered. All four are **green** — not because the shortfall was papered over,
but because a profile below the loop depth is asked the question that is true of
it. The shortfall is printed on every run:

```
T   NCTX=8 has fewer contexts than the loop needs: 5.78 clk/recip
    against the serial reference's 6.96
```
