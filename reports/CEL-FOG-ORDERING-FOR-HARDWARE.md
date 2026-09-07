# Cel-material fog ordering — the brief for the hardware lane

**Written 2026-09-07 for Fabian to hand to the hardware lane.**

**Status: the OWNER RULING ALREADY EXISTS and is frozen.** It was made on
2026-08-31 and is recorded in `reports/OWNER-RULINGS-COMPLETE-20260831.md` §5 as
**"STATUS: FROZEN VISUAL LAW"**. What is outstanding is the **implementation**,
not the decision. *(The docket still lists D12 among the open items; that entry
is stale and is being corrected.)*

---

## 1. The problem, in one sentence

**The general renderer mixes fog into vertex colour *before* rasterisation. If a
cel material's toon ramp then consumes already-fogged colour, the fog itself gets
quantised into the toon bands — so distant creatures show hard, stepped rings of
fog instead of a smooth fade.**

## 2. Why it happens

The toon ramp is a **quantiser**. It takes a continuous lighting value and snaps
it to a small number of bands — that is the entire point of cel shading.

The general law hands it a colour that already has fog mixed in. So the quantiser
cannot tell "this surface is lit less" from "this surface is further away": both
arrive as the same darker number, and **both get snapped to the same band edge.**

Fog is meant to vary **smoothly** with distance. A toon ramp is designed to
destroy smooth variation. Running one through the other converts a gradient into
a staircase, and the staircase **moves as the creature moves**, which is what
makes it read as a defect rather than as style.

## 3. The fix — the ruling as frozen on 2026-08-31

**Cel materials are an explicit exception to the general per-vertex-fog rule.**

The cel order is:

```
  interpolate UNFOGGED lighting
    -> apply toon bands
      -> modulate the texture
        -> apply fog
          -> enter the ordinary post chain
```

**Fog is applied AFTER the quantiser instead of before it.** The bands are then
computed from lighting alone — which is what they describe — and the fog fades
smoothly across an already-banded surface.

**Scope, exactly as ruled:**
* **Non-cel materials keep the existing general per-vertex fog path**, unless a
  separate material recipe says otherwise.
* **Emissive/additive spell surfaces, sky and HUD stay exempt** under their own
  recipes.

## 4. What it costs

**One extra scalar interpolant per vertex** — the unfogged lighting value has to
survive to the fragment stage alongside the fogged one.

`reports/ZIXXTRIXX_CEL_IN_HARDWARE.md` calls this **"cheap now that `ATTRSTEP`
exists"**: the exact-stepping work already landed (`RASTER.ATTRSTEP`, recorded in
the docket's DONE list as *"exact stepping, 15.1× fewer divides"*), so adding one
more interpolated scalar rides machinery that is already built and already paid
for.

**No extra pass, no extra buffer, no new block** — it is an ordering change plus
one interpolant.

## 5. Why it was flagged as needing the owner in the first place

It **contradicts a general law**. The per-vertex fog rule is a stated principle
of the renderer, and this makes cel materials a permanent, deliberate exception.

The document's own words: *"it must be recorded as a deliberate cel-material
amendment rather than inherited quietly. That is a visual-semantics change:
Class C."*

**That is the whole reason it went to the owner** — not because the fix is
doubtful, but because a silent exception to a general law is how a renderer
becomes a pile of special cases nobody can reason about. **It has now been ruled
and recorded, so the exception is deliberate and documented.**

## 6. What the hardware lane actually needs to do

1. **Implement the cel order** for cel-tagged materials: keep the unfogged
   lighting interpolant, quantise on it, modulate texture, then apply fog.
2. **Leave every other material path alone** — this is a cel exception, not a
   change to the fog law.
3. **Verify the thing it exists to prevent**: render a cel creature receding into
   fog and confirm the fog fades **smoothly** rather than stepping, while the
   *lighting* bands stay crisp. Both halves matter — losing the bands would be
   the opposite failure.

## 7. Questions the hardware lane may reasonably ask back

* **Does the reference implementation already do this?** The creature reel
  renders cel creatures every pass; if it applies fog before the ramp, its output
  and the silicon's would disagree once this lands, and the reel is the oracle.
  **Worth checking before implementing, not after.**
* **Which materials carry the cel tag today**, and is the tag already in the
  material record, or does it need adding?
* **Does anything downstream depend on the fogged vertex colour** arriving at the
  ramp — any effect that has quietly come to rely on the current ordering?

---

# AUTHORISED — 2026-09-07

**Owner: *"alright, seems small enough, we're doing it."*** and, after the
hardware lane's findings, **_"I said to build it, now you can operate as if it's
there."_**

**The fog stage is authorised to be built, reference-first.** The hardware lane
had asked for the explicit word before opening vertex-attribute work in
`GEOM.PROJECT` that a rearchitecture brief defers — **it has it.**

## What the hardware lane found, and why it made this easy

**There is no fog at all today.** `FogMode` defaults to `Off`; nothing in
`reference/` or `tools/` sets it; nothing computes a factor; nothing performs a
mix. The *"already fogged"* comments described **a stage that does not exist.**

* **No golden CRC can move** — the oracle and the silicon cannot disagree about
  something neither of them does.
* **The work is purely additive.** There is no fogged colour anywhere that would
  have to be un-fogged first.
* **This is the cheapest moment it will ever have**, and it stops being cheap the
  day any content switches fog on.

**Four documents asserted the opposite of the ruling** — `rast.cpp:306`,
`zref_fragment.hpp:117`, `internal.hpp:79` and `zhao_raster_fragment.sv:92`, the
last reasoning from superseded §8 text *as ratified law*. **Any one of them would
have steered an implementer into the wrong build.** All four corrected. *(That is
the false-comment fault class, in the hardware corpus, in a `.sv` file — see
`Upheaval/creature/10-GATE-CHECKLIST.md` item 8.)*

## Two corrections to this brief, both strengthening it

1. **The decision is not merely frozen in a rulings file — it is in the SPEC.**
   `spec/qformats.md` §8 carries **D-5 (2026-09-03)** in full, with the ordering
   table and an explicit note marking three of its own sentences false. **Only
   the docket was stale.**
2. **D-5 fixes TWO errors, not one.** Besides the toon staircase, it names
   **texture modulation multiplying the fog colour itself.**

**And the old order was never implementable** — `DOCKET` R7 already recorded it:
*"RASTER.FRAGMENT says colour arrives already fogged; GEOM.PROJECT has no colour
input to have fogged it with."* **There is no working implementation of the old
order to be compatible with**, so nobody can argue it on compatibility grounds.

## For the CREATURE lane — "operate as if it's there"

**Manafold and Zixxtrixx may now assume the cel fog ordering exists**: unfogged
lighting → toon → texture → fog. This is the standing rule applied
(*"treat the specs of the machine like the specs, not what's there now"*).

⚠ **And a word collision worth stating once, because it will confuse somebody:**
**Manafold's "fog" is not this fog.** The creature's translucent shell and its
mana mist (`kFogThicknessPm`, the mist plane) are **creature-authored effects on
the creature's own material**. They are unrelated to scene distance fog and
`FogMode`, and **Direction 9 §7's shell work is unaffected** by this either way.
