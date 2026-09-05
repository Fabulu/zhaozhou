# G1-C: the II=1 material combiner is refuted by its own tripwire

**Date** 2026-09-05
**Block** `zhao_texture_combine` (fpga/rtl/texture/zhao_texture_combine.sv)
**Fit** commit `28db7708`, 1,640.1 s, Cyclone V 5CSEBA6U23I7

## The measurement

| | measured | rule | source of the rule |
|---|---|---|---|
| ALMs | **494** | 650 | islandrearchitecture5.md §3.3 |
| registers | **524** | 500 | §3.3 |
| DSP blocks | **8** | 2 | §3.4 tripwire "reject DSP > 2" |
| fmax | **100.12 MHz** | — | |

Status `failed:structure`, two violations: `DSP 8 > allowed 2` and
`registers 524 > allowed 500`.

## The tripwire did the job it was set to do

The rule was written into `design/fit_targets.yml` **before** the fit, with its
own pre-committed response recorded beside it:

> The DSP rule is the interesting one. This implementation takes the CONTRACT's
> II=1 and spends four parallel byte products; the architecture instead
> describes two product lanes with queued continuations. **If this fires, that
> IS the evidence for moving to the two-lane scheduled form** — a decision made
> against a number rather than ahead of one.

It fired. The response is therefore already decided, and raising `max_dsp` to
fit the measurement would be precisely the "a rule written after the fit it
governs reports a pass" failure CLAUDE.md records.

## What the architecture actually said, and what was built instead

`reports/islandrearchitecture5.md` §15.5, last line, verbatim:

> **Do not write six independent `*` operators and assume they pack.**

`zhao_texture_combine.sv` has **eight** independent `unit_mul` call sites, each
an inferred `*`. The fitter proved they do not pack: one DSP block each.

§15.2 explains why the parallel form is wrong on its own terms rather than
merely expensive — *the TMU supplies at most one sample per clock*, so a
two-sample recipe cannot retire faster than one fragment per two sample clocks
no matter how many multipliers stand ready. The II=1 form buys throughput the
sample supply cannot use.

### The first reading of this result was wrong, and the error is worth keeping

The island totals 7,913 ALM against a 7,500 redline but only **14 of 112 DSP**,
so the initial reading was "DSPs are not the scarce resource; keep the eight and
raise the rule". That is wrong on the facts, because it treats the choice as
DSP-versus-ALM when the architecture's preferred variant is cheaper on **both**:

> **A. LOGIC2** — two exact 9x9/9x8 multipliers in ALM logic; **zero DSP**;
> preferred if <= 800 ALMs and >= 125 MHz.

Two shared multipliers over three cycles is less silicon than eight always-live
ones on either axis. The trade only looks like a trade if you have already
assumed the parallel structure.

Variant B (`DSP2_PACKED_OR_EXPLICIT`) is explicitly conditional: *explicit
vendor primitive/IP only; maximum two DSP blocks for the whole combiner;
accepted only if the fitter proves the count and composition improves.* Inferred
`*` operators are neither explicit nor proven, so the built block is not
variant B either. It is the shape §15.2 exists to forbid.

## A second, separate gap: two recipes are missing

The architecture names **eight** recipes (§15.1):

```
0 PASSTHRU  1 MODULATE  2 MODULATE2X  3 LERP
4 ADD_SAT   5 MASK      6 TERRAIN_DETAIL_LIGHT  7 TERRAIN_DETAIL_MASK
```

Both `zhao_texture_combine.sv` and the reference model
`reference/include/zref/zref_material.hpp` stop at 5 (`kRecipeCount = 6`) and
**refuse recipe 6 as illegal**. The two missing ones are the three-sample
terrain recipes — and DETAIL_LIGHT is the worst case the whole §15.4 capacity
argument is built on:

> Worst named three-sample terrain recipe DETAIL_LIGHT requires six byte
> products per fragment: 276,480 fragments x 6 = 1,658,880 byte products.

So the capacity analysis that justifies two lanes cannot be exercised against
either implementation today. This is not a consequence of the DSP finding; it
is an independent shortfall found while reading §15 to answer it, and it means
the block's directed and differential tests, which pass, cover six of eight
recipes while reporting full coverage of the recipes they know about.

## What happens next

1. Keep `max_dsp: 2`, `max_alms: 650`, `max_registers: 500` unchanged. The rule
   is the architecture's, not this pass's, and it was correct.
2. Build `zhao_texture_material_combine_v1` to §15.3/§15.5-A: a fragment record
   with a completed-microjob mask, a COMBINE_FIFO of depth 8, two LOGIC2 product
   lanes and queued continuations. PASSTHRU and ADD_SAT bypass the lanes
   entirely.
3. Extend the reference model to all eight recipes first, so the RTL has an
   oracle for the three-sample paths before it is written rather than after.
4. Add the per-recipe product-job counters §15.4 requires ("Counters must record
   actual product jobs by recipe"), so the 80%-capacity question is answerable
   from a trace instead of from arithmetic.
5. Re-fit and compare against the same three rules.

`zhao_texture_combine.sv` stays in the tree until its replacement measures
better — deleting the refuted variant before the successor is measured would
leave G1-C with no combiner at all and no record of why the shape changed.

## Evidence rule this run relied on

The island totals used above are a **sum of standalone per-block fits**, which
is not a composed measurement and is not a device total. The same census sums
to 342 DSP against 112 available, which is impossible and is the tell: the rows
include mutually exclusive variants (`@lanes2`, `@lanes1-io`, `@pre-rearch`,
`svcseed2/3`) and no cross-block packing. Treat every number here as a
per-block figure. The composed island measurement is G1-D.
