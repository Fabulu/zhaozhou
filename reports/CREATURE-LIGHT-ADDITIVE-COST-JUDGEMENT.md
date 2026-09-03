# Three more per-fragment interpolants: what they cost

**Answering the CREATURE.LIGHT additive-emission handoff
(`reports/CREATURE-LIGHT-ADDITIVE-TERM.md`), 2026-09-03.**

The question asked: *what do three additional per-fragment interpolated
attributes cost in the raster pipeline as it stands, and does the answer change
if this enters the contract before the block is built?*

## The judgement

**Take the per-fragment version.** It does not threaten timing, and the axis it
does load is one you are already paying down for other reasons.

**And yes, it matters enormously that this lands before the block is designed**
— not because the answer changes, but because the attribute count is a
**parameter of a sizing sweep that has not been run yet**, and running it with
the right number is free while retrofitting it is not.

## Why timing is the wrong worry

The attribute path is **elastic, not combinational**.

`zhao_raster_attrdiv_svc` is a **shared service**: `UNITS` divide engines behind
a tag, where the tag exists — its own words — *"so the CALLER can say which
attribute and which pixel this was."* Attributes are **time-multiplexed through
it**, not replicated into parallel hardware. Three more attributes are three
more tagged requests.

So adding lanes **does not lengthen a combinational path**. It adds requests to
a ready/valid service that backpressures. The critical path in this renderer is
Early-Z's 256-bit presence lookup fed by TilePipe's column encoder — measured at
~2.6 ns, and the thing `SaveTheRendered.md` commit one just moved off the
critical path by registering the cursor. Attributes are nowhere near it.

**The nanoseconds you are fighting for are not in this path.**

## What it actually costs, in order of size

**1. Storage and bandwidth — the real cost.** Three more attribute planes per
triangle in `GEOM.PARAMBUF`, and three more interpolated values in the fragment
packet. That lands on **ALM and M10K**, which is precisely the axis the texture
island is currently **2.2× over its redline** on. Not fatal, but it is the
scarce resource, so it should be counted rather than waved through.

**2. Divide-service throughput.** `RASTER.ATTRSTEP` exists specifically to make
this cheap: its proof is **640,000 pixel-attributes, zero mismatches, 0.099
divides per pixel against 1.000** — a 15.1× reduction. Attributes are *stepped*
across a tile, not divided per pixel. Three more planes cost three more
per-tile seeds, not three more per-pixel divides.

**3. `UNITS` on the divide service, possibly.** And this is the part worth
saying plainly: **the service was built to answer exactly this question.** It
exposes `accepted_o`, `retired_o` and `stall_clocks_o` because — its own
comment — *"the wall is whichever resource REFUSES, and a service that cannot
report its own refusals cannot be sized."* Its `UNITS` and `RADIX` are described
as *"the two independent knobs, and the fit decides which is cheaper — not this
file."*

## So the measurement to run, rather than the argument to have

Run the existing `attrdiv_svc` sweep (`u1/u2/u4/u8`, already registered as
tests) **with the attribute count raised by three**, and read `stall_clocks_o`
on a real frame. That answers "does the service saturate" with a number instead
of a judgement. If it does, `UNITS` is the knob and the fit says whether more
units or shorter units is cheaper.

**That sweep is cheap and it is the honest form of this answer.** Everything
above is structural reasoning; the sweep is evidence.

## Why "before the block is designed" is the whole point

Three things are still parameters, and all three want the real number:

* `ATTRS` in the attribute setup and parameter-buffer sizing;
* `UNITS` and `RADIX` on the divide service;
* the fragment packet width through FRAGROB.

Deciding now means the sizing sweep includes the emission lanes and the answer
is a measured row. Deciding after `CREATURE.LIGHT` exists means retrofitting
three interpolants into a closed budget and a fitted island — which is the
expensive version of the same change, and exactly the outcome the handoff says
is worth spending time to avoid.

## On per-face, since it was offered as the alternative

**Per-face is the wrong trade here and the owner already judged it visually.**
It saves the three interpolant lanes — the *cheapest* part of the cost — and
pays with banding on triangle edges, which is a visible artefact on the exact
feature whose justification is that it looks beautiful. Trading the smallest
cost for the largest regression is a bad exchange even before the aesthetic
judgement, and the aesthetic judgement has been made.

## The constraint the prototype found, which is a design input not a tuning note

> at the pool core a strong emission can peg a channel — the prototype pegs red
> over a few hundred pixels at its strongest, where modelling survives only in
> green and blue. It holds at 384×240, and a harder emission tips into neon.

That is **saturation behaviour visible to the artist**, so it belongs in the
contract as a named, editable constant with a stated failure mode — not as an
emergent property of where the saturate happens to sit. The art law applies
directly: the shipped value is chosen by looking at it in scene, at final
resolution, against what it sits on.

It also interacts with a ruling made today. **D-1** fixed the composition order
for exactly this class of term:

    for each independent light i:
        raw_i = shade_flat_tri_dir_unclamped(n, L_i)
        ndl_i = clamp01(raw_i + normal_detail_i)
        rgb  += light_colour_i * ndl_i
    rgb += ambient + spill
    final = saturate(rgb)

The additive emission is a **per-source term accumulated into `rgb`**, and it
must saturate **once at the end** alongside ambient and spill — not per source,
which would clip each contribution separately and change the colour of an
overlap. The handoff's own formulation agrees ("applied after the texel multiply
and before the final saturate"); this just names where it sits in the ruled
order so the two cannot drift.

## Summary

| | |
|---|---|
| timing | **not threatened** — elastic path, shared tagged service, far from the critical path |
| ALM / M10K | **the real cost**, on the axis already over budget. Count it. |
| per-pixel divides | **~0.099 per pixel**, not 1.0 — ATTRSTEP already solved this |
| the measurement | raise the attribute count, run the existing `u1/u2/u4/u8` sweep, read `stall_clocks_o` |
| before vs after | **decisively before** — `ATTRS`, `UNITS`, `RADIX` and the packet width are all still parameters |
| per-face | **no** — saves the cheapest part, costs the most visible thing |
