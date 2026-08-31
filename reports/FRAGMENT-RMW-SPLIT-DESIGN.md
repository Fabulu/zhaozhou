# The Fragment RMW split — designed, costed, and blocked on one tradeoff

**Status: design complete, implementation deliberately NOT started.** The
blocking item is a tradeoff that touches an explicit architecture rule, and it
is the owner's to settle. Everything needed to implement it once he does is
below.

Written 2026-08-31 after the round-2 fit (`49ad539`, 60.92 MHz).

## What the measurement says has to happen

    launch clock path    7.856 ns
    data path           14.361 ns   <-- must fall below 7.95
    data required       15.801 ns
    slack               -6.416 ns

The data path is the read-modify-write loop, in **one cycle**:

    tilestore RAM read data
      -> dst_r8/g8/b8, dst_depth, dst_tag, dst_sten      (field extract)
      -> the three tests: alpha, stencil, depth           -> `live`
      -> zhao_raster_blend x3                             DSP mult 3.785 ns
      -> out_depth / out_tag / out_sten                   carry chains ~2 ns
      -> wr_data_o -> tilestore RAM write

**It has to be roughly halved.** That is a pipeline split, not a tweak.

## Why the loop is one cycle today, and it is not an accident

`zhao_raster_fragment`'s own header records the reason, and it is a good one:

> Stage 1 stands on `rd_data_i` COMBINATIONALLY — that is what collapses read,
> test, blend and write into one cycle and buys the **write-first hazard
> immunity**.

A second fragment hitting the same tile address cannot read stale data, because
there is never a window in which a write is pending. `raster_fragment_random`
reports **3,232 same-pixel chains** in 10,509 writes, so that traffic is real,
not theoretical — roughly **31 %** of writes follow another write to the same
address.

The block also carries a documented consequence of the collapse: **the stall
re-issues its own read**, because `rd_data_i` does not persist. That hack
exists only to support the single-cycle design.

## The split, at the right place

Not at the RAM boundary — at the **midpoint of the logic**, which is where the
delay actually is:

| stage | work | est. |
|---|---|---|
| **A** (existing s1) | field extract + the three tests + `out_depth`/`out_tag`/`out_sten` | ~2.3 ns |
| **B** (new s2) | `zhao_raster_blend` ×3 → `wr_data_o` → RAM write | ~6–7 ns |

Registers to add between them: `dst_rgb` (24), `src_rgb` (24), `src_a` (8),
blend mode, `out_depth` (24), `out_tag` (8), `out_sten` (8), `live`, `addr`,
`src_id` — about **110 bits**, comparable to the skid buffer already added.

Splitting at the RAM boundary instead was considered and rejected: it removes
only the ~1.5 ns of RAM-output-to-logic and leaves the whole blend intact, so it
lands around 12.9 ns — better, and **still not under 7.95**.

## THE BLOCKING TRADEOFF

Once the write is one cycle later, a same-address fragment in stage A reads a
value the pending stage-B write has not yet committed. There are exactly two
ways out and **they are not equivalent**:

### Option 1 — forward (the note's "small in-flight address CAM")

`dst_word = (s2 writing the same address) ? wr_data_o : rd_data_i`

Preserves one fragment per clock on every traffic pattern.

**But `wr_data_o` is stage B's blend output.** Forwarding it into stage A's
tests, which then register into stage B, creates the path
`blend -> mux -> tests -> registers` — **which is the long path again, in a new
shape.** Forwarding a value that is expensive to compute does not help unless
the forward is taken from a *later* register, which means a deeper pipeline and
a multi-entry scoreboard rather than the "small CAM" the note imagines.

### Option 2 — stall one cycle on address conflict

Hold stage A for one cycle so the write lands first. **Trivially correct**, and
it keeps both stages short.

**But it regresses initiation rate on same-address traffic** — ~31 % of writes
would take a bubble, so effective throughput becomes roughly 1.3 clocks per
fragment on that mix. And the architecture rule adopted for this whole effort
is explicit:

> *latency may grow; **initiation rate** and exact arithmetic **may not
> regress***

**Option 2 violates the rule as written.** Whether it violates its *intent* is
the question, and that is not mine to answer: the rule exists to stop
optimisations that quietly cost frame rate, and a 1.3× on a third of writes is
exactly the kind of cost it was written to catch.

## What the owner is actually being asked

One of:

* **(a)** accept a data-dependent bubble on same-address fragments — simple,
  safe, and a knowing amendment to the architecture rule;
* **(b)** pay for true forwarding — a deeper blend pipeline with a multi-entry
  scoreboard, more area, more latency, and materially more risk in the block
  that has the subtlest hazard reasoning in the renderer;
* **(c)** neither yet — bank the analysis, and see whether the **clock** problem
  below changes the arithmetic first.

## And the reason (c) is not a dodge

The same report shows `gpu_clk~CLKENA0` driving **13,682 fanout**, with launch
and latch clock paths differing by **1.995 ns**.

**No datapath work recovers skew.** Even a perfectly split RMW loop starts ~2 ns
in debt. That is a clocking problem, it is **not on `MHZArchitected`'s list of
five offenders at all**, and it may well be cheaper to fix than option (b).

Measuring it first is defensible sequencing rather than delay: it could change
which of (a) and (b) is even necessary.

## Why this file exists instead of a commit

The session's own discipline says measure before changing, and the two previous
rounds both punished acting on a plausible reading. This change is:

* the subtlest hazard logic in the renderer, with documented reasoning that
  exists specifically to prevent the bug this split reintroduces;
* a ~110-bit, multi-stage refactor of a block on the critical path;
* and gated on a tradeoff that contradicts a written rule.

Implementing it hastily at the end of a long session, on a block whose failure
mode is *silently wrong pixels on 31 % of writes*, is how a fast change becomes
a slow bug. The design is done; the decision is one sentence from the owner.
