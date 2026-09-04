# A prediction about `cache_pipe`, written before its fit returns

Filed 2026-09-04 while `quartus_map` is running on
`zhao_texture_cache_pipe`. Predictions written after the fact are worthless;
this one is timestamped by its commit.

## What the committed row says today

    status ok            registers 11328   blockMemoryBits 128   M10K 2   alms 5903
    lastAttemptStatus failed:structure

Third-worst ALM count in the census, and 11,328 registers against 128 bits of
memory — the D19m signature.

## What the source says

The block declares, inside a per-lane `generate` with `LANES = 4`:

    logic [15:0]      data_r [LINES * HW_PL];   // 16 x 8 = 128 x 16b = 2,048 bits
    logic [TAG_W-1:0] tag_r  [LINES];           //      16 x 24b     =   384 bits

so about **8,192 bits of line data** and 1,536 of tags across four lanes —
which, in flops, is most of the 11,328.

**But the read is the good shape.** `QUARTUS_GOTCHAS.md` §14 says what decides
inference is the logic between the array read and the first register, and here
there is none:

    ram_dat[gl] <= g_lane[gl].data_r[rd_daddr[gl]];

Array read straight into a flop, exactly like `zhao_raster_tilestore`'s
`ram0_q <= ram0[b0_raddr]` (which inferred) and unlike `tmu_pipe`'s
`decode16(pal_dat_r[...])` (which did not).

The author was plainly thinking about this. The comment beside the one array
that stays flops says so outright:

> `valid_r` stays a flat flop array: 4 x 16 = 64 bits, it NEEDS the reset a
> memory cannot give, and putting it in memory would let a line read valid
> before the fill engine had cleared it.

## The prediction

**`data_r` will infer as memory in this fit, and the register count will come
back far below 11,328** — roughly 2,000–3,500, being the pipeline registers,
the tags, and `valid_r`.

The recorded 11,328 is from `sourceCommit 8faaa240`; the row also carries a
LATER `lastAttemptStatus: failed:structure` under a different commit, so **the
`ok` numbers are stale by construction** (D19l). This block has been
restructured since they were taken.

## The caveat that could make me wrong in a boring way

Four separate 2,048-bit arrays are small. Quartus may place them in **MLAB**
(LUT-based) memory rather than M10K, which would show as **low registers but
also low `blockMemoryBits` and few `ramBlocks`** — the storage moved out of
flops without appearing in the M10K column.

So the falsifiable half is **registers**, not memory bits. If registers come
back near 11,328, §14's rule is incomplete and I want to know that more than I
want to be right.

## Why this is worth writing at all

Every mechanism I proposed for D19m today was plausible and two were wrong,
each refuted by a counterexample already in the tree. The cheapest way to stop
that pattern is to state what a rule predicts **before** the measurement rather
than after, so the rule can fail visibly instead of being quietly reshaped to
fit whatever arrives.

---

# OUTCOME: confirmed, on both halves

Synthesis completed 18:14:27, about twelve minutes after the prediction was
committed.

    predicted   registers far below 11,328, roughly 2,000-3,500
    measured    Total registers        : 3097
                Total block memory bits: 8,320   (was 128)
                Total DSP Blocks       : 0

**3,097 registers** — inside the predicted range, a 73% fall from the recorded
11,328. And the memory half landed too, which the caveat allowed to fail:

    altsyncram:g_lane[0].data_r  Simple Dual Port  128 deep x 16 wide
    altsyncram:g_lane[1].data_r  Simple Dual Port  128 deep x 16 wide
    altsyncram:g_lane[2].data_r  Simple Dual Port  128 deep x 16 wide
    altsyncram:g_lane[3].data_r  Simple Dual Port  128 deep x 16 wide
    altsyncram:rq_src_rtl_0      Simple Dual Port    4 deep x 16 wide
    altsyncram:rs_src_rtl_0      Simple Dual Port    4 deep x 16 wide

Four lanes x 128 x 16 = **8,192 bits**, which is the figure computed from the
source declaration before the fit ran, plus 128 for the two small ones = 8,320.
It went to real block memory, not the MLAB fallback the caveat warned about.

## What this establishes, and what it does not

**§14 works forward.** The rule was derived from four blocks after the fact;
this is the first time it was used to state an outcome *before* a measurement,
and the outcome matched in registers, in destination, and in exact bit count.
That is a stronger claim than "consistent with the cases it was built from".

**D19l is confirmed in the concrete.** The `ok` row saying 11,328 registers and
128 memory bits was stale — the block had been restructured under a later commit
whose only trace was `lastAttemptStatus`. A reader trusting `status` would have
gone looking for a defect that had already been fixed. **The third-worst ALM
figure in the census belonged to code that no longer exists.**

**It does not establish that §14 is complete.** One correct prediction on a
block whose read shape was already the good one is weak evidence about the cases
where the rule is hard. The test that matters is `tmu_pipe` after its palette
read is registered, and that has not been run.
