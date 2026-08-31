# Zixxtrixx's cel presentation in hardware

The question was whether the shipped `ZIXX_EXP=celmain` creature fits the
console. It does, and unusually well — nothing it does is shader-like. This file
records the architecture, what is now built, and the one feature that is
genuinely awkward.

**Built today: `RASTER.TOON`.** The rest is specified here and not yet RTL.

---

## What the creature actually asks for, and where each part lands

| feature | where it goes | state |
|---|---|---|
| ~1,930-tri mesh, 28 bones, ≤2 weights | ordinary meshlets; inside the frozen 32-bone limit | asset done, `GEOM.MESHFETCH`/`VDECODE` not built |
| 64-slot clip bank, hero `bake60` | extra compressed pose records; same quaternion decoder | pose store not integrated |
| RGB565 bilinear + mips, U repeat / V clamp | **exactly** what TMU v2 already does | done, 79/79 |
| Cool Cross two-light + ambient | fixed-function per-vertex; no per-pixel normal | `CREATURE.LIGHT` not built |
| smooth three-band toon | **`RASTER.TOON`** | **built, 11/11** |
| adaptive 1/2/4 px exterior ink | tile ink plane + `POST.INK` | not built; the hard one |
| spring squash/spread | small fixed pre-skin sidecar | `CREATURE.DEFORM` not built |
| moving pupils | ordinary skinned parts | nothing needed |

**No second TMU, no normal map, no sampled lightmap, no programmable material.**
The crayon grain, pigment zones and face markings are baked into the atlas, so
they are free after the one primary sample. Zixxtrixx sits at the cheap end of
the bounded 0–3-sample material architecture.

---

## RASTER.TOON — built, and why it is not a generic toon shader

The shipped ramp does **not** replace the light with the band value. It rescales
each lane by `q/mean`, so the band changes and the chromatic relationship
survives — the Cool Cross blue fill stays blue in shadow rather than becoming
dark grey. Measured through the block:

    (30000, 45000, 90000) -> (27272, 40909, 81818)     1 : 1.5 : 3 preserved

Two arithmetic details decide whether the creature looks like itself:

* **The division truncates toward zero** — C++ semantics, *not*
  `zhao_raster_attrdiv`'s round-half-away-from-zero. Reusing the attribute
  divider would be off by one on most fragments: invisible in a screenshot,
  fatal to a capture CRC.
* **The mean's own `/3` truncates too, and it decides the BAND.** One code of
  drift there moves a band edge — a visible line on the creature, not a
  rounding bit.

    thresholds {43000, 57000}    levels {28000, 50000, 82000}

**Rate: 4.11 clocks a fragment streamed → 405,515 a frame** against a 320,000
stress profile, and only *surviving cel* fragments arrive. It took four
versions: 201 → 39 → 9.29 → 4.11, and the middle two were the same mistake —
measuring a block that issues one job and waits tells you how long a job
**takes**, not how many it can **do**.

---

## The three still to build

### `GEOM.CREATURE.LIGHT` — small, well-bounded

Pull key and fill into each bone's bind space, take the packed s8×3 bind normal,
Lambert per influencing bone, clamp, blend on the existing 1/64 skin weights,
mix ~80% smooth / 20% face, emit three RGB lanes. No per-pixel dot products.

It should be a **shared sequenced service**, not four permanent dot-product
trees per vertex; the multiplier count comes from the 120,000-vertex workload
and the fit.

This got much cheaper today: `RASTER.ATTRSTEP` interpolates attributes at 0.066
divides per pixel, so carrying creature RGB and alpha no longer costs a divider
farm.

### `GEOM.CREATURE.DEFORM` — small, and deliberately *not* `GEOM.WARP`

Radial vertices contract one local axis and expand the perpendicular plane about
an authored centre; followers ride the carrier without being crushed; normals
get the inverse-transpose correction; unmarked vertices take an exact identity
bypass. The vocabulary is already bounded and exact, so a fixed block beats the
general programmable one.

**Pack it factored**, not as the C++ `DeformVertex` per vertex: group carries
centre/axis/role, vertex carries `group_id` + strength. Zixxtrixx's rings share
centres naturally.

### `RASTER.TILESTORE.INK` + `POST.INK` — the genuinely awkward one

The shipped contour is not an inverted hull and not an edge detector. It takes
the creature mask, flood-fills the background connected to the screen exterior,
dilates outward, never overwrites creature pixels, and **does not outline
enclosed holes** — which is why the S-shaped opening stays clean.

**Do not infer the mask from final RGB in hardware. Write it explicitly.** A
small separate ink plane beside the tile store — not a widening of the frozen
64-bit colour/depth/stencil word — resolved into an `INKBUF` alongside the
framebuffer.

Inverted hull is a **measured fallback, not the answer**: it outlines inner
holes, shows meshlet seams, varies width with orientation, and misbehaves under
the spring deformation.

A full Z60 one-bit mask is 11.25 KiB, so storage is not the problem. The
exterior flood and its SDRAM traffic are, and they need a measured design. This
is the most uncertain feature physically — more than the toon bands ever were.

---

## Hundreds of them: yes, but not hundreds of the hero form

200 × 1,930 = **386,000 triangles** before terrain, objects, spells or
particles. That is not a 60 Hz content tier and should not become one. The
binner study already showed a synthetic 200-creature army at only 96 triangles
each produces 19,200 triangles and 23,912 tile references — which is why the
external geometry parameter buffer (`GEOM.PARAMBUF`) is required.

The ladder carries the cel *identity* down, not the hero *representation*:

| importance | representation |
|---|---|
| hero / close | full mesh, `bake60`, full toon, exact 2–4 px exterior ink |
| near army | reduced mesh, same ramp, 1–2 px ink |
| mid | micro-mesh, simplified bands, 1 px silhouette |
| small | prebaked cel splat with painted outline |
| tiny | animated glint |

The economies hold: all instances share one atlas, instances on the same
`{type, clip, frame, sub}` share one decoded pose, and far splats bypass the
toon service entirely because the banding is baked in.

**Hundreds of cel-shaded Zixxtrixx-class creatures: yes. Hundreds of
simultaneous full-detail hero Zixxtrixxes: no, deliberately.**

---

## One decision that is not mine

**Fog ordering for cel materials.** The general law mixes fog into vertex colour
*before* rasterisation. If the toon ramp consumes already-fogged colour, distant
fog gets quantised into hard bands.

The right order for cel is *interpolate unfogged light → toon → modulate texture
→ apply fog*, which needs one extra scalar interpolant — cheap now that
`ATTRSTEP` exists. But it **contradicts the general per-vertex fog law**, so it
must be recorded as a deliberate cel-material amendment rather than inherited
quietly. That is a visual-semantics change: Class C.

---

## Pose storage: do not put it in M10K

One 28-bone decoded pose is 28 × 48 = **1,344 bytes**. A 128-tuple cache is
~168 KiB of matrices alone — a quarter of the device's M10K for pose data.

Tags and the current pose stage on-chip; decoded palettes live in the local
SDRAM hot region; instances sharing a tuple share one palette. That supports
hero `bake60` without spending the block RAM the renderer needs.
