# The pair-pipe is not a DSP lever. It is the register lever — and the like-for-like map to prove it was already on disk

2026-09-09. Completes `PAIRPIPE-IS-NOT-A-DSP-LEVER-20260909.md`, whose last line
says the comparison it needed **"is running now"**. It finished, and nobody read
it against the pair-pipe. Zero new compute was needed for anything below.

## The frame was wrong, for the second time today

`zhao_raster_perspuv_pairpipe` was assessed against **DSP**, correctly found not
to help — it keeps both multipliers by design, because *"the two axes compute
different numerators against a shared mantissa and saturate independently…
serializing them would halve the throughput to buy nothing"* — and came off the
lever list.

Meanwhile the criterion it *does* move is the one nobody was measuring. The
island misses its register budget by **+81%**, worse than its ALM miss of +44%,
and `zhao_raster_perspuv_svc` is the largest single register consumer in it.

This is the same shape as `rcp24_v3`, judged as an 8%-of-ALM lever and dismissed
while being decisive for DSP. **Twice in one day a block was dismissed against the
criterion it does not help.** The lesson is not "check DSP and registers"; it is
that a lever list organised by *remedy* silently inherits whichever criterion was
urgent the day it was written.

## What the pair-pipe deletes, in its own words

> `zhao_raster_perspuv_svc` carries TWO of everything on the control side: two
> work queues, two write pointers, two read pointers, two emptiness tests, two
> token selects. It then reassembles the axes at the far end through a
> sixteen-entry table with a per-axis `e_have` join and two result tables
> `e_q_u`/`e_q_v`. **None of that duplication does anything.**
>
> The join disappears because there is nothing left to join: the pair travels as
> ONE item through one pipeline, so `e_have`, `e_q_u`, `e_q_v` and the
> sixteen-entry operand tables have no reason to exist.

Those are exactly the structures that hold `perspuv_svc`'s registers. Its `_q`
pipeline state is only **594 bits — inside its 700-bit budget line**; the
**3,376-bit token table** is 85% of the block. The pair-pipe removes the table
rather than trying to make it inferable.

It is not a proposal. It rests on `PERSPUV-AXIS-LOCKSTEP-PROOF-20260908.md`, an
induction proof that the two schedulers are bit-identical for all time, and
`tests/raster/perspuv_lockstep_directed.cpp` asserts that every cycle against the
elaborated RTL, with depth-zero fragments in the mix and a live-probe control so
its zeros mean something.

## The like-for-like comparison, which now exists

The earlier report refused to compare a fit row against a map row, and was right
to: *"'2,255 fewer registers' is sitting right there and it would be wrong."* It
estimated a ~1.7× fit/map replication from two `texture_combine` pairs and
discounted the pair-pipe to *"perhaps ~1,600 fitted"*, calling that **still not a
number I have measured**, and launched `perspuv_svc@map` to fix it.

That row is on disk. Both of these are MapOnly, same tool, same device, both
`rtlCleanAtHead: true`:

| | registers | memory bits | DSP |
|---|---:|---:|---:|
| `zhao_raster_perspuv_svc@map` | **3,361** | 256 | 6 |
| `zhao_raster_perspuv_pairpipe@map` | **961** | **1,280** | 6 |
| delta | **−2,400** | +1,024 | 0 |

**And it corrects the discount.** The assumed ratio does not hold for this block:

```
perspuv_svc   FIT 3,216 registers    MAP 3,361 registers    map/fit = 1.045
texture_combine pairs (the report's basis)                   map/fit ≈ 0.59
```

For `perspuv_svc`, map and fit registers are within 4.5% — fitting does **not**
replicate them the way it does in the combiner pairs. So the earlier ~1,600
estimate rested on a ratio measured on different blocks, and it was too
conservative.

## What is established, and what is still an estimate

**Measured, like-for-like:** the pair-pipe maps to **2,400 fewer registers** and
puts **1,024 more bits in memory** at identical DSP.

**Estimated:** its *fitted* register count, because **the pair-pipe has never been
fitted**. Bracketing with the two ratios this ledger actually contains:

| assumed map/fit | pair-pipe fitted | saving vs `svc`'s fitted 3,216 | share of the 7,285 overage |
|---|---:|---:|---:|
| 1.045 (this block) | ~920 | ~2,296 | **32%** |
| 0.59 (combine pairs) | ~1,629 | ~1,587 | **22%** |

So **22–32% of the island's register overage, from a block that is already built,
already proven, and already tested.** That is by a wide margin the largest single
lever found today — the monotone-chain encoding was ~6%, the ROM packets 618 ALM,
`OWNERS` closed at 32/64, and the RAM-inference route is still hunting a blocker
after six eliminated candidates.

**Not claimed:** that it closes the register gate. 22–32% is not 100%, the
register breach is systemic across 9 of 11 components, and ALM and Fmax are
separate failures.

**Not claimed:** ALM or Fmax effects. Neither map row carries ALMs, and neither
block's Fmax appears in a map. The pair-pipe's header expects an ALM saving too
and that remains unmeasured.

## The one thing that would close it

**One MapOnly is not enough; this needs one FIT of `zhao_raster_perspuv_pairpipe`.**
It is the only way to turn the 22–32% range into a number, and it also returns the
ALM and Fmax columns that no map can. A fit is the owner's call under §0.2 — but of
the fits waiting, this is the one with the clearest question attached: *does
replacing `perspuv_svc` with the pair-pipe remove ~2,300 registers and what does it
cost in ALM and Fmax?*

It is also **not** one of the twelve queued fits, and it is a leaf, not the island
— minutes-to-an-hour, not 1.5–4 hours.

## The loose end this closes, and the habit it argues for

`PAIRPIPE-IS-NOT-A-DSP-LEVER` ended with *"`zhao_raster_perspuv_svc@map` is
running now… it is the difference between a claim and a measurement."* The run
completed and the comparison was never made, so the conservative estimate stood
as the working number for a day while the evidence to sharpen it sat in the
ledger.

**A report that ends by naming a running job needs something that reads the row
when it lands.** That is the same gap as the four `.rgb`-purge lesson: the
half-fix was thorough and the other half was never built.
