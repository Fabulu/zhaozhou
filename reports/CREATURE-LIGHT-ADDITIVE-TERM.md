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

### RULED, 2026-09-03, by the hardware lane: PER-FRAGMENT. Timing is the wrong worry.

The landing question is answered. **Per-fragment is taken; per-face is refused** —
it saves the interpolant lanes, which are the cheapest part, and pays with banding
on the exact feature whose justification is that it looks beautiful.

**Timing is not threatened.** `attrdiv_svc` is a shared *tagged* service — the tag
exists so a caller can say which attribute and which pixel a request belongs to.
Three more attributes are three more tagged requests, **not three more dividers**,
and nothing lengthens a combinational path. The critical path is Early-Z's 256-bit
presence lookup fed by TilePipe's column encoder (~2.6 ns), which the
Save-the-Renderer work has already moved off. Per-pixel divides are a non-issue:
ATTRSTEP measured 0.099 divides per pixel against a budget of 1.000.

**The cost lands on a different axis: ALM and M10K** — three more planes in
`GEOM.PARAMBUF` and three more values in the fragment packet — which is exactly
the axis the island is currently 2.2x over on. Not fatal, but **counted, not waved
through.**

The honest form of the answer is a measurement the service was built to give:
raise the attribute count, run the existing u1/u2/u4/u8 sweep, and read
`stall_clocks_o`. The wall is whichever resource refuses, and a service that
cannot report its refusals cannot be sized.

**Deciding before the block is designed matters decisively.** `ATTRS`, `UNITS`,
`RADIX` and the packet width are all still parameters, so deciding now means the
sizing sweep includes it. Deciding later means retrofitting into a closed budget.

## Saturate ONCE, at the end — not per source

**Contract refinement from the hardware lane, and it is load-bearing.**

The emission is a per-source term **accumulated into rgb**, and the result
saturates **once at the end**, alongside ambient and spill. Saturating per source
would clip each contribution separately and **change the colour of an overlap** —
which would silently destroy the mixing that motivated the feature.

This interacts with the D-1 ruling (`shade_flat_tri_dir_unclamped` holds the
arithmetic verbatim; `shade_flat_tri_dir` became `clamp01` of it). Note the
distinction the hardware lane drew, correcting its own earlier wording: additive
terms sum before the clamp **within one light**. Across lights they do not — a
second source's negative dot must clamp to zero **independently**, or it would
subtract another light's illumination.

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
