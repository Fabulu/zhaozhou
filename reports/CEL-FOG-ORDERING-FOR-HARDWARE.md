# Cel/fog ordering — for the hardware conversation

**Status: DECIDED. Owner ruling D-5, 2026-09-03. Implementation outstanding.**

The docket entry that sent this back as an open question was stale; it is
corrected. `spec/qformats.md` §8 already carries the ruling in full, including
the ordering table and an explicit note that three of its own sentences are now
false.

## The issue in one sentence

The toon ramp is a **quantiser**. Handed colour that already contains fog, it
cannot distinguish "less lit" from "further away" — both arrive as the same
darker number and snap to the same band edge. Fog varies smoothly by design; a
toon ramp destroys smooth variation by design. Running one through the other
turns a gradient into a staircase, and the staircase *moves with the creature*,
which is what makes it read as a bug rather than a style.

## The ruling, as frozen

D-5's ordering, from `spec/qformats.md` §8:

| | ordinary material | cel material |
|---|---|---|
| 1 | lighting | lighting |
| 2 | interpolate lighting + fog factor | interpolate lighting + fog factor |
| 3 | — | **toon quantisation** |
| 4 | texture/material combination | texture/material combination |
| 5 | **fog final source RGB** | **fog final source RGB** |
| 6 | framebuffer blend | framebuffer blend |

Bands come from lighting alone — which is what they describe — and fog fades
smoothly across an already-banded surface. Fog-exempt classes (sky family, HUD,
deliberately emissive/additive) take an **explicit bypass**.

D-5 names two visible errors in the old order, not one: fog quantised into hard
toon bands, **and** texture modulation multiplying the fog colour itself.

## The three questions, answered

### 1. Does the reference reel already do this? — NO. It documents the opposite.

This is the one that mattered, and the answer is not the comfortable one.

* `reference/src/zrender/rast.cpp:306` applies `apply_toon_ramp()` to the
  interpolated vertex lanes.
* `reference/include/zref/zref_fragment.hpp:117` described those lanes as
  "vertex RGB: lit, tinted and **ALREADY FOGGED**".
* `reference/src/zrender/internal.hpp:79` cited "qformats §8: the fogged colour
  rides the ordinary Gouraud path".
* `fpga/rtl/raster/zhao_raster_fragment.sv:92-107` cited the superseded §8 text
  as *ratified law* and reasoned from it.

So the oracle's **documentation** encoded exactly the order D-5 forbids, in four
places, each of which would have steered an implementer straight into the wrong
build. All four are corrected in this change.

### 2. Will the oracle and the silicon disagree when this lands? — NO, because no fog exists yet.

**No fog mix is implemented anywhere in the tree.** `fog_near`/`fog_far` appear
only in the ABI wire struct (`zhao_abi_pkg.sv`) and the sky env state;
`FogMode` defaults to `Off` and **nothing in `reference/` or `tools/` ever sets
it**. Nothing computes a fog factor and nothing performs a fog blend.

The consequences are all favourable and worth stating plainly:

* No golden CRC can move, because no pixel is fogged today.
* Implementing D-5 is **purely additive** — there is no already-fogged colour
  anywhere that would have to be un-fogged first.
* The "ALREADY FOGGED" comment described a stage that does not exist.

This is the cheapest possible moment to get the order right, and it stops being
cheap the moment any content turns fog on.

### 3. Was the old order even implementable? — No, and the docket already knew.

DOCKET R7, unprompted:

> The arithmetic is specified; `RASTER.FRAGMENT` says colour arrives already
> fogged; `GEOM.PROJECT` has no colour input to have fogged it with.

So §8's order was not merely bad for cel materials — the stage nominated to
apply the fog had nothing to apply it to. D-5 resolves an impossibility, not
just a preference. That is worth knowing before anyone argues for the old order
on compatibility grounds: there is no working implementation of it to be
compatible with.

## Cost

One extra scalar interpolant per vertex, riding `ATTRSTEP`
(`fpga/rtl/raster/zhao_raster_attrstep.sv`, landed). No extra pass, buffer or
block. The factor computation itself — `f_raw` / `f` / `f8` in §8 — survives
D-5 unchanged and is still frozen; only what is *carried* and *when the mix
happens* changed.

## What this change does, and what it does not

**Done here:** the four superseded citations corrected, so the tree no longer
instructs an implementer to build the wrong order; docket D12 corrected from
"open question" to "decided, implementation outstanding".

**Not done here:** the fog stage itself — the ATTRSTEP factor lane, and the mix
at the final source colour between material combination and framebuffer blend.
That is real RTL plus a matching reference implementation plus tests, and it
should land in the reference first, because the reel is what defines correct.
