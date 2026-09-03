# CREATURE.LIGHT — the additive point-light term is APPROVED

**Owner approval, 2026-09-03:** *"It's fucking beautiful we must have it."*

This is a **contract amendment for a block that has not been designed yet.** It
is cheap now and expensive later. Nothing in shipped RTL is being asked to change
by this document — but the raster attribute budget question below is real and
belongs to the people currently closing timing.

Evidence: the working prototype is live as the "Additive light (experimental)"
subject on the bestiary, beside the unmodified multiplicative clip. Reference
implementation is on `main` at `3d1ace62`; the run is
`runs/CLAUDE-RUNS/RUN-20260903-2144-zixxtrixx-additive-light-prototype`.

---

## What it is

Creature point lights were **multiplicative only**: texel colour times
per-channel gain. Under multiplicative transport a coloured light can only
subtract, so a red source on green pigment can only make olive. The owner asked
for a red light that reads red on the green body; this is the term that does it.

Each point light gains a per-channel **additive** emission alongside its
multiplicative gain:

* `add_r/g/b` per source, Q16.16 of the 255 pixel scale.
* The additive sum accumulates from the **same per-source lambert x attenuation
  response** as the multiplicative gains. This is not optional — a flat additive
  lift raises lit and unlit faces equally and flattens form, which is precisely
  the failure the creature rig was rewritten to cure in August 2026 (a
  multiply-only scalar model landed six of twelve faces of a body of revolution
  on one identical ambient floor).
* Applied **after** the texel multiply and **before** the final saturate, so it
  bypasses the shade path's 1.0 gain ceiling. Bypassing that ceiling is the
  entire point: a source strong enough to clamp all three channels becomes a
  hue-neutral floodlight and erases both its own colour and every other source's.
* The cel path is unaffected. `apply_toon_ramp` bands on the mean and rescales
  channels by `q/mean`, so hue survives as a channel ratio either way.

## What it costs

Per source, per evaluation:

* **+3 MACs.** The dot product and the attenuation are already computed for the
  multiplicative term and are shared.
* **One saturating 3-channel add** after the texel multiply.

Where it lands is the design decision with a real hardware consequence:

| landing | cost | look |
| --- | --- | --- |
| **per-fragment** (what the prototype does) | 3 extra interpolator lanes carrying the accumulated emission, plus the add per fragment | smooth pools; this is the approved look |
| per-face | no extra lanes | **bands the pools on triangle edges** |

The reference prototype interpolates three new Gouraud lanes
(`ScreenV.ar/ag/ab`, `TriMode.add_lanes`).

**The question for the hardware lane:** what do three additional per-fragment
interpolated attributes cost in the raster pipeline as it stands, and does that
change if they are added to the block contract before the block is built rather
than after? This matters now because the conventional renderer is mid timing
closure and the fragment path is where the remaining slack lives.

An acceptable answer is "per-fragment costs X and we should take it", or
"per-fragment is unaffordable at the target clock, here is what per-face would
look like and what the owner would lose". Both are useful. What is not useful is
discovering the constraint after `CREATURE.LIGHT` is designed.

## Constraints that come with it

* **Four simultaneous sources is the budget**, with the strongest four selected
  when a scene declares more (owner, 2026-09-03). The selection pass belongs
  where the compositor reads the source array.
* Four sources x three channels is twelve adds per evaluation at the budget.
* Every emission value is an owner-editable constant in the authoring layer.

## Known behaviour worth designing around

At the pool core a strong emission can **peg a channel** — the prototype pegs R
over a few hundred pixels at its strongest, where modelling survives only in G
and B. It holds at 384x240 and a harder emission tips into neon. Saturation
behaviour at the add is therefore visible to the artist, not just arithmetic.

## Reference

* `reference/include/zref/zref_creature.hpp` — `CreaturePointLight::add_r/g/b`,
  and the `g_creature_additive_light` gate (default OFF; gate-off renders are
  CRC-identical to the multiplicative bank, proven across 22 subjects).
* `reference/src/zcreature/creature_sim.cpp` — accumulation against the shared
  lambert x attenuation response.
* `reference/src/zrender/rast.cpp` — the interpolated lanes and the saturating
  add.
* `Upheaval/creature/08-LIGHTING.md` — the light rig's durable facts, including
  the two ways a light is present but invisible.
