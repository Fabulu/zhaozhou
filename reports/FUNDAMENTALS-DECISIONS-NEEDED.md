# The decisions items 1–7 are blocked on

**2026-09-03.** Working `BORING_3D_FUNDAMENTALS_AUDIT.md`'s priority order
produced six new contracts and two amendments. It also produced **eight
questions that are the owner's, not mine** — and the honest thing is to put
them in one place with a recommendation each, rather than pick silently and
have the choice discovered later as a fact.

Each has a **default**: what happens if nobody rules. Where the default is
safe, say so and move on. Where it is not, it is marked.

---

## D-1 · The clamp in the flat-shade law · **BLOCKS `TERRAIN.SHADE` RTL**

`shade_flat_tri_dir` clamps to `[0, 0x10000]` **inside** the function. Any
additive term — detail normals, a second sun, a spell light — must be summed
**before** that clamp, or it cannot brighten a face that is slightly turned
away.

* **(a)** the law grows an **unclamped variant**; both it and the additive terms
  consume it; the clamp moves to where shade becomes colour
* **(b)** the additive term is passed **into** the function, which keeps sum and
  clamp together

**Recommendation: (a).** It keeps detail out of a function terrain and creatures
share, and it is a **pure refactor whose golden CRCs must not move** — which is
also how you know it was done correctly.

**Default if unruled: nothing gets built.** `TERRAIN.SHADE` and
`TERRAIN.NORMALMAP` both wait. **This is the one to answer first.**

---

## D-2 · Cartridge resource kinds · **BLOCKS `MATERIAL.RESOLVE` and generic upload**

`.zpak` has no generic texture-page or material-set kind, while creature parts
claim texture pages, terrain names tilesets, and `DrawForm` takes a material-set
handle.

* **(A)** generic `TEXTURE_PAGE` + `MATERIAL_SET` + `MESH_STREAM`
* **(B)** every family page carries its own subpages, one nested layout
* **(C)** generic immutable `RESOURCE_BLOB` + typed manifests

**Recommendation: A.** Only A lets `MEM.UPLOAD` be one general mover for all
immutable assets — which audit R5 requires — and only A gives
`TEXTURE.CACHE`'s invalidate one obvious publisher. B needs three family loaders
to agree forever; C adds indirection this console has refused elsewhere.

**Default if unruled: every asset compiler invents its own layout**, which the
audit names as the thing that cannot work. **Not safe.**

---

## D-3 · Cache coherence mechanism · `MEM.UPLOAD`

Publishing a mapping must make every cache unable to return the old bytes.

* **(a)** drive an **explicit invalidate** at the caches holding that resource
* **(b)** **bind the cache tag to the resource generation**, so an old line
  cannot match a new request

**Recommendation: (b).** Structural rather than procedural, needs no list of
who caches what, and the generation is already carried for staleness.

**Default if unruled: neither happens**, which is today's state — invalidate
inputs exist and nothing drives them. **Not safe**, and invisible when wrong:
correct memory, wrong picture, no counter moves.

---

## D-4 · Where depth quantisation lives

* **(a)** a separate `GEOM.DEPTHQUANT` block (contract written)
* **(b)** an explicit stage **inside `GEOM.PROJECT`**

**Recommendation: either, and (b) is arguably tidier** since the reciprocal is
produced there. The audit is indifferent and specific about the alternative:
*"just do not leave it in integration glue."* The contract exists separately
only because adding an unbuilt responsibility to a built, verified block hides
unbuilt work inside a green row.

**Default if unruled: it stays glue**, differently in each of the twelve places
that consume `invw24`. **Not safe.**

---

## D-5 · The fog carrier · audit R7

The fog arithmetic is specified. `RASTER.FRAGMENT` says it does no fog because
vertex colour arrives already fogged — and `GEOM.PROJECT` has **no colour or
environment input** with which to have fogged it. `SetEnvironment` is still a
**reserved** command.

Two things to rule:

1. **Who applies fog.** Recommendation: `GEOM.LIGHT`, since it is the block
   that will hold the environment record and produce vertex colour anyway.
2. **The cel ordering**, and this one is visible:

   * general law today: `lighting → fog → interpolate → toon`
   * cel probably wants: `unfogged lighting → interpolate → toon → texture → fog`

   Fogging before toon **quantises fog into hard bands**, which is exactly
   wrong for distance.

**Recommendation: rule the cel path as the second ordering**, and do it
**before the colour packet freezes** — afterwards it is an ABI change rather
than a wiring choice.

**Default if unruled: fog is never carried at all**, because no block has the
input. Safe in the sense that nothing breaks; the game simply has no distance
haze.

---

## D-6 · Local spell lighting · audit R9

Prose today, not a command format or hardware. For a game whose spectacle is
magic, the failure mode is specific: *"if giant magical fireballs bloom but cast
no coloured light onto nearby creatures, terrain or structures, the effects can
look pasted onto the world."*

Three receivers, three different sensible answers:

| receiver | recommended v1 |
|---|---|
| creatures / objects | bounded **top-K** diffuse lights, K small and fixed |
| terrain | sun + **broad spill / global flash** — not every point light |
| particles / spells themselves | emissive/additive + the existing glow tags |

**Recommendation: adopt that table as v1** and note that the accumulator does
not need K permanent engines merely because K terms are legal — sequenced
accumulation at the skinner's rate is the default.

**Default if unruled: no dynamic lights at all.** Safe for the machine, costly
for the look, and cheap to defer since `GEOM.LIGHT` is where it would land.

---

## D-7 · Water and lava · audit R10

`TWOD.PLANE` explicitly refuses to be a general world-depth plane; intersecting
liquid must be triangles through the main renderer. That is fine and needs no
hardware.

**The question is only whether the game wants liquid surfaces.**

* **yes** → freeze the cheap v1 material: low-poly surface, scrolling texture,
  alpha or additive, optional vertex waves, depth test on with an **explicit**
  depth-write policy, no reflection, no refraction, no screen-space copy,
  explicit sorting rule
* **no** → **say so and delete the phrase from the ledger**

**Recommendation: answer it either way.** The danger is neither option — it is
the stale ledger phrase *"water/lava"* making someone assume a solution exists
when the plane engine refuses the relevant depth behaviour.

---

## D-8 · Non-terrain normal maps · **refuse explicitly**

The terrain normal-map unit is cheap **precisely because a heightfield's
tangent frame is world-axis-aligned**. It does not generalise to creatures or
props, which would need a real tangent frame per vertex.

**Recommendation: explicitly REFUSE general tangent-space normal maps in v1**,
in writing, rather than leave it unstated — because the danger is someone
inferring that the terrain block generalises and budgeting as if it does.

**Default if unruled: the inference gets made.** Cheap to prevent now.

---

## And one thing that is not a decision, but is owed

The near-plane law **drops an entire triangle when any vertex is behind the
eye**. Deliberate, documented, and not a gap — but it **needs a close-camera
stress reel**: giants, Zixx, beams and large terrain triangles, with the camera
pushed in. It can visibly pop geometry, and the only way to know how badly is
to watch it.
