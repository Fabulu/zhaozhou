# The metadata never travelled through the raw FIFO

**Found by the leaf test written for the block I had just changed.** It is also
the mechanism behind the composed-level bilinear anomaly, which three earlier
hypotheses failed to explain.

## The defect

`zhao_texture_rsp_dispatch` accepts a response into a **raw FIFO**, then
dispatches from that FIFO into the per-class queues:

```systemverilog
assign head_cls      = raw_c[raw_rp];
assign dispatch_fire = head_v && head_room;
```

Data and token are read from the FIFO (`raw_d[raw_rp]`, `raw_t[raw_rp]`). My
packet C change wrote the metadata like this:

```systemverilog
if (dispatch_fire) cq_m[head_cls][cq_wp[head_cls]] <= rsp_meta_i;   // WRONG
```

`rsp_meta_i` is the **current input**. The response being dispatched entered the
FIFO earlier. So the metadata enqueued beside a response belongs to whatever
arrived at the input on the dispatch cycle — a different response whenever the
raw FIFO holds anything.

**Data and token travel through the FIFO. Metadata did not.** It is the
stage-misalignment class again: a value read from the wrong point in a pipeline,
width-legal and lint-clean.

## Why the composed tests did not catch it, and the bilinear ones did

When the raw FIFO is empty — the common case — the response being dispatched IS
the one at the input, and the metadata happens to be right. That is why:

* the CLUT queue measured **792 checked, 0 wrong**
* the nearest queue measured **192 checked, 0 wrong**
* gate 2 passed 122 checks and gate 3 stayed byte-identical

and why the bilinear queue measured **768 checked, 32 wrong (4.2%)**: the
bilinear phase is the one that fills the raw FIFO, because its four-channel
sequencing puts several responses in flight at once.

**The 4.2% is the fraction of dispatches that happened with a non-empty FIFO.**

That unifies the finding with the earlier eliminations rather than contradicting
them. It was never slot recycling (generation check: 0), never the bank (shadow
1,176/0), and never the queue mechanism — it was the enqueue **source**.

## What this says about where the tests were

The composed suite exercised this path 1,752 times across three queues and
reported clean on two of them, because those two rarely see a non-empty FIFO.
**A leaf test with four evenly-loaded lanes found it in one run** — 239 of 240
wrong, which is what the defect looks like when the FIFO is usually busy.

The leaf test also cost two false starts of its own before it was trustworthy: a
harness ordering bug, and `TOKW` left at its default 16 while the island uses 18,
which truncated the tokens and produced 180 mismatches that were mine. Both were
found by reading the failure rather than believing it.

## The fix

The metadata must ride the raw FIFO with its response — a `raw_m` array beside
`raw_d`/`raw_c`, written at input acceptance and read at `raw_rp` on dispatch.

**Not applied yet:** `zhao_texture_rsp_dispatch` is inside the closure of the
island fit currently running, and editing a file under a running fit is the
live-tree trap. It goes in when that fit lands.

## What this means for the fit in flight

It is measuring a design whose metadata payload is misaligned. The defect is a
wiring source, not a structural one — the same registers and queue entries
exist either way — so the ALM/register/M10K figures remain a fair reading of
packet C's cost. **The Fmax figure is also fair**, since the timing path through
`cq_m` is identical whichever source feeds it.

Recorded here so the receipt is not later quoted as though it measured a correct
design.
