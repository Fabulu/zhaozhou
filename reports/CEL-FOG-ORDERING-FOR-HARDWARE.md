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

## BUILT, reference-first, on the owner's go-ahead (2026-09-07)

> "Do the rearchitecture. Now's the time. Geom's probably fucked right now
> anyway." — then, when I reported the lane existed but nothing produced it:
> "you said fog on my go-ahead? Well make the fog"

So it is made, end to end, in the oracle:

* **The lane.** `ScreenV::fogf`, Q16.16, defaulting to `0x10000` = CLEAR.
  Interpolated with exactly the alpha lane's shape — one round-half-up division
  at setup, exact s32 stepping, full barycentric re-evaluation at row starts.
  D-5 made the factor an ordinary interpolant, so it uses the ordinary machinery.
* **The law.** `reference/include/zref/zref_fog.hpp` — §8's `frame_k` and
  `vertex_factor`, frozen arithmetic reproduced exactly, including the disabled
  case `fog_far <= fog_near` as a deterministic no-op.
* **The producer**, which is what makes it a feature rather than a possibility:
  `apply_vertex_fog(ProjOut&, near, far, k, L)`. `ProjOut::w` already carried the
  guarded forward distance §8's `d` asks for. A behind-the-eye vertex is left
  CLEAR rather than fully fogged — its primitive is culled, and fogging a vertex
  nobody draws would only put a surprising number in a struct.
* **The mix**, at the final source colour: after the toon ramp, after material
  combination, before the framebuffer blend. `kAlpha` fogs the SOURCE before
  blending so the destination — fogged when it was written — is not fogged
  twice. `kAdditive` cannot fog by construction, honouring §8's frozen exempt
  list.

### One defect in the frozen text, resolved and documented

§8 defines `f = 1` as CLEAR, but its mix formula weights toward `fog_c` by `f8`,
which at `f8 = 255` ("clear") returns the fog colour — inverted fog. D-5
**replaced** that mix wholesale ("everything from 'Mix (frozen)' to the end"),
retaining it only as the record, so the surviving law is the factor, and the
weight toward `fog_c` is its COMPLEMENT. That is the only reading consistent
with §8's own polarity comment, and it is argued at the call site rather than
silently chosen.

Everything else is preserved: the unit8 law (raw/256, not /255) and ONE rounding
per channel.

### The acceptance test, and it was shown to FIRE

`test_d5_fog_after_toon_quantiser` renders one lit gradient twice — clear, and
under CONSTANT fog — and asserts the toon band edges land on **exactly the same
pixels**. Constant fog is the sharpest available probe: under the correct order
it is a uniform recolour that cannot move an edge; under the wrong order it is a
uniform shift that moves every edge a threshold sits near.

Fire-tested by building the forbidden order on purpose — fogging the Gouraud
lanes before `apply_toon_ramp`:

    FAIL: D-5: the toon band edges land on exactly the same pixels with and
          without fog -- bands come from LIGHTING alone.
    FAIL: D-5: constant fog recolours the bands without creating or destroying any

Both fired. Source restored, `render_directed` **all green**, and green with fog
off means byte-identical: no golden moved.

**Still not done:** the RTL. The oracle now defines correct, which is the order
question 1 demanded.
