# FIT GATE 4: the like-for-like RCP comparison, and what it can and cannot claim

2026-09-09. `zhao_raster_rcp24_svc@g4-nctx12` landed (the third attempt -- two
predecessors were killed by external stops, one at ~98% done). It is the matched
partner the owner's DSP ruling has been waiting on.

Both rows: `status: ok`, `rtlCleanAtHead: true`, real source digests, identical
`topParameters: NCTX=12 TOKW=14`. `NCTX` and `TOKW` are declared and used
identically in both files -- `NCTX` sizes the per-context state, `TOKW` is the
token port width -- so this is one parameterisation, two architectures.

| | `svc` | `v3` | delta |
|---|---|---|---|
| ALM | 1,802 | **986** | **-816 (-45.3%)** |
| DSP | 6 | **3** | **-3 (halved)** |
| M10K | 1 | 8 | **+7** |
| registers | 1,514 | 1,402 | -112 |
| blockMemoryBits | 288 | 2,502 | +2,214 |
| Fmax | 56.24 | **100.95** | **+44.71 MHz** |

**The owner's ruling was "halve the DSPs even if it costs". At this
parameterisation it does not cost -- v3 is smaller, faster, and halves them.**
The only thing it spends is seven M10K.

---

## THE CLAIMS SPLIT INTO TWO KINDS, AND ONLY ONE KIND IS SAFE

The island instantiates `zhao_raster_rcp24_svc #(.NCTX(8), .TOKW(14))`. **NCTX=12
is not the island's operating point**, and no fitted row exists for either module
at NCTX=8/TOKW=14. So before quoting the table above, the two halves have to be
separated -- this is the mismatched-pose law, and the bias here runs in the
direction that flatters the conclusion I want.

### Parameter-INVARIANT, and these are the load-bearing ones

* **DSP: `svc` reports 6 in every row it has ever produced; `v3` reports 3 in
  every row it has ever produced.** Across NCTX 8/12/16, TOKW 8/14, three seeds,
  map-only and full fits alike, neither number has moved once. The DSP count is a
  property of the multiplier structure, not of the context array. **The halving is
  established independently of where NCTX sits.**
* **M10K: `svc` reports 0-1, `v3` reports 6-8, everywhere.** The +7 is likewise
  structural, not a parameter artefact.
* **Fmax separation: the two ranges do not overlap and are not close.** `svc`
  across five rows spans **56.24 - 68.63 MHz**. `v3` across four spans **90.41 -
  100.95 MHz**. The best `svc` row ever measured is **21.8 MHz below the worst
  `v3` row ever measured**, and the product clock is 100 MHz. No row at the exact
  operating point is needed to see that `svc` does not close it and `v3` can.

### Parameter-DEPENDENT, and stated with its bias

**The -816 ALM is measured at NCTX=12 and is very likely an OVERSTATEMENT of the
advantage at the island's NCTX=8.** `svc` holds its whole context state in
flip-flops and logic (`c_m` 24 b, `c_x` 32 b, `c_w` 64 b, `c_k`, `c_tok`, `c_val`,
`c_pend`, `c_ph`, all `[NCTX]` arrays, and `blockMemoryBits` of 0 at NCTX=8
confirms none of it reached RAM). `v3` puts the equivalent state in block memory.
So `svc`'s ALM should climb steeply with NCTX and `v3`'s should barely move --
and `v3`'s measured slope is **~12 ALM per context** (1,034 at NCTX=16 -> 986 at
NCTX=12, TOKW fixed at 14).

`svc`'s slope is **not measured**, because its only two rows differ in *both*
parameters (NCTX 8->12 *and* TOKW 8->14 together account for the +761 ALM). I am
not going to divide that between them by assertion. Compare like with like or do
not compare -- and the honest consequence is that the ALM row of the headline
table is directional, not quotable at the operating point.

---

## WHAT THE MISMATCHED COMPARISON WOULD HAVE SAID

This gate exists because `OWNER-DECISION-RCP-V3-20260908.md` refused to put
`v3@g4-nctx12` beside the standing `svc` row at NCTX=8/TOKW=8. Now that both
numbers exist, the size of the error it avoided can be stated:

| | mismatched (v3@12/14 vs svc@8/8) | like-for-like (both @12/14) |
|---|---|---|
| ALM | -55 | **-816** |
| Fmax | +32.49 | **+44.71** |

**The mismatched comparison understated v3's advantage by 761 ALM.** That is the
interesting direction. The usual failure mode is a comparison that flatters; this
one would have made a decisive architectural win look like a rounding difference,
and the DSP ruling might have been argued as "barely worth it" on the strength of
it. A mismatched comparison is not biased toward comfort -- it is just wrong, and
which way it points is luck.

## THE QUESTION THE GATE WAS OPENED TO ANSWER

> *"does svc also infer M10K at TOKW=14? If it does, v3's eight stop being a
> differentiator. If it does not, eight blocks is the price of the DSP halving."*

**It does -- but only one block, 288 bits.** `svc` at TOKW=14 infers a single
M10K where at TOKW=8 it inferred none. So v3's eight are very nearly undiminished
as a differentiator: the price of the DSP halving is **+7 M10K**, not +8, and not
zero.

Against the device budget (553 M10K, 112 DSP) that is a good trade on its face:
seven blocks out of 553 is 1.3% of a resource we are not short of, buying three
DSPs out of a budget that is **over by roughly 180 against 112**. It also points
the same way as the memory-for-ALM-and-DSP study: **v3 already IS the "use memory
instead" design** for this block. It was built, measured, and shelved behind a
fit; the fit now says it wins.

## WHAT THIS DOES NOT SETTLE

The swap itself. `RCP-V3-SWAP-HAS-NO-LIKE-FOR-LIKE` and
`RCP-V3-THROUGHPUT-IS-NCTX-DEPENDENT` still stand: v3's throughput criterion is
NCTX-dependent and the island runs NCTX=8, which is the *low* end. Area and DSP
now favour v3 decisively. **Fmax does not transfer to the island** -- see
`ISLAND-TIMING-IS-ONE-REGISTER-BIT-20260909.md`, which shows the island's reported
62.83 MHz is set by a pin path unrelated to the tile, and that a zero-delay
reciprocal would move the internal-only ceiling by 4.08 MHz before hitting the
next path out of the same source register.

Throughput at NCTX=8 was already measured on 2026-09-08 and is **5.78 clk/recip,
17% faster than the 6.96 serial reference**, below the tile's own 4.6 threshold,
all arithmetic checks passing. What gate 4 changes is that memo's economics, not
its throughput number: it priced the 17% against "8 M10Ks and +377 registers"
taken from the mismatched pair. Matched, v3 spends **112 fewer** registers. The
+7 M10K is real.
