# Contract — FORGE.SHADOW (Contact shadows, as ordinary geometry)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: `fpga/rtl/forge/zhao_forge_shadow.sv` — BUILT and UNIT-TESTED
> (`tests/forge/forge_shadow_directed.cpp`, 39 checks), NOT COMPOSED. The
> "RTL: not built" line that stood here was stale; corrected 2026-09-20 by the
> geomseam packet, which read the file. The ledger's `tests:` rows still say
> `PLANNED -- NOT WRITTEN` and are stale the same way.
> Reference: `zref::forge::shadow_*` — PLANNED AND NOT WRITTEN. Searched
> `reference/` for `shadow_hull` on 2026-09-20: zero hits, so the ledger's
> `reference_model: zref::forge::shadow_hull` names a function nobody wrote.

## Purpose and exclusions

FORGE.SHADOW emits a small terrain-conforming transparent hull under a creature
or object, so it looks **attached to the ground**.

**Written 2026-09-03 from `BORING_3D_FUNDAMENTALS_AUDIT.md` R8.** Full shadow
maps are deliberately absent from this console and that is defensible. What was
missing is any settled replacement for the most basic visual job there is — and
for a game of **floating islands and airborne creatures**, "is that thing
touching the ground or hovering above it" is not a refinement, it is legibility.

The audit's estimate is the reason this is worth doing now: **very small if
done as geometry**, and *"can make more perceptual difference than several
expensive material effects."*

**Exclusions, and they are the whole design:**

* **No shadow map.** No depth pass from the light, no shadow buffer, no second
  view.
* **No new framebuffer and no new raster hardware.** The output is ordinary
  transparent geometry through the main renderer.
* **No shadow unit.** This is a primitive generator, a sibling of
  `FORGE.PRIM`, not a lighting stage.
* **No self-shadowing, no shadows cast onto other creatures, no shadows from
  terrain onto terrain.** Contact only.
* **No occlusion query.** Whether the ground is really below is answered by a
  few height taps, not by visibility.

## The frozen ladder

| range | shadow |
|---|---|
| **near hero** | projected low-poly hull, or an 8–16 vertex ellipse, conformed from a few terrain height taps |
| **near army** | 4–8 vertex blob |
| **mid** | tiny dark splat |
| **far** | none |

The ladder is a **coarseness floor selected by the governor**, exactly as
`PART.LADDER` treats particle representation — the same idea and the same
refusal to let a distant creature spend near-hero geometry.

## Input and output packet layouts

**In**, per shadow caster: `{ world_x, world_z, radius, strength, rung,
src_id }` — where `rung` selects the ladder step and `strength` is a unit8 the
content author owns.

**Terrain height taps in**: the block asks for a small fixed number of heights
around the caster and conforms the hull to them. **Fixed count per rung**, so
the cost is bounded and knowable rather than dependent on terrain roughness.

**Out**: a vertex stream to `GEOM.SETUP`, in a **declared deterministic order**
— the same rule `FORGE.PRIM` obeys, and for the same reason: two orderings
produce the same picture and different capture CRCs.

## Backpressure rules

Ready/valid. It is a background producer: a stalled shadow must never delay a
creature. Under pressure the governor lowers the rung, which is a coarser
shadow rather than a missing one — **a shadow that vanishes is worse than a
crude one**, because the creature appears to take off.

## Memory ownership

None. It reads terrain heights through the ordinary terrain path.

## Q formats and rounding

Positions fx16 world units, as everything else in the geometry path.
`strength` is unit8 (value = raw/256, so 255 is the largest representable and
not 1.0).

## Where the transparency comes from — OWNER RULING R89, and R48 reconciled

**This section exists because two written laws disagreed.** Owner ruling R48
(2026-09-19) fixed the console's vertex alpha at the named constant `ALPHA_C` =
fx16 1.0 (opaque), reasoning *"no ratified vertex format carries alpha, so
nothing is being stubbed"*. This contract's exclusions say the output is
*"ordinary **transparent** geometry through the main renderer"*, and
`zhao_forge_shadow.sv:147` emits a per-vertex `vtx_alpha_o`. A shadow composed
under R48 as written would be a flat **opaque** polygon under every creature —
the two documents contradicted each other, and each was cited on its own.

**R89 resolves it, and both documents now say the same thing.** The resolution
is not a compromise, it is a reading of what this block actually produces:

* `zhao_forge_shadow.sv:295` is `assign vtx_alpha_o = strength_q`, and
  `strength_q` is latched per CASTER. **Alpha is already constant over the
  hull.** The per-vertex port carries a per-primitive quantity.
* So what FORGE.SHADOW needs is a **flat per-triangle alpha**, and the console
  already has carriage for exactly that: `tri_continuation_tail_i`'s
  `vertex_alpha` field, feeding the composed blend ALU (six instances in
  `zhao_raster_fragment.sv:490-510`). Giving that open boundary a producer
  is the whole job.
  **NAME CORRECTED 2026-09-20 (forgeshadow packet), because the old wording
  sends a reader to a grep that returns nothing.** The six instances are
  `zhao_raster_blend_prod` (`:490`, `:493`, `:496`) and `zhao_raster_blend_fin`
  (`:502`, `:505`, `:508`). The line range and the count were exactly right;
  the MODULE NAME was not. **`zhao_raster_blend` itself is instantiated
  nowhere in `fpga/`** — it is the unsplit reference wrapper, kept so the
  formal proof targets shipping logic (`zhao_raster_fragment.sv:358`). Only
  the `_prod` half carries the alpha port, and it is called `a_i`.
* **R48 therefore stands, unamended and true.** No ratified vertex format
  carries alpha and none is being invented here. The alternative — a fourth
  `zhao_geom_attrpack` plane AND a fourth `zhao_raster_tile_pipe_v2` lane, for
  a value that does not vary across the primitive — is a real feature
  (interpolated per-vertex alpha) that should be commissioned as one, not
  smuggled in as part of closing a shadow gap.

**So this contract's `vtx_alpha_o` is hereby declared PER-PRIMITIVE, not
per-vertex.** It is emitted once per caster and every vertex of that caster's
hull carries the same value; a consumer is entitled to sample it once.
Interpolating it is not required and, until the fourth attribute lane exists,
not possible.

Recorded in both places by ruling: here, and against R48 in
`reports/OWNER-RULINGS-20260919-EVENING.md`. *A contract corrected in one place
and not the other is how this pair got here.*

## R89 IS DECIDED, THE CONSUMER IS REAL, AND THE VALUE IT WANTS HAS NO PRODUCER

Added 2026-09-20 by the **forgeshadow** packet, which was scheduled by ruling
R132 on the premise that R89 was FORGE.SHADOW's last blocker and therefore
*"the cheapest remaining unlock in the whole run"*. **That premise does not
hold, and this section is the evidence, so the next lane inherits a verified
list instead of a spent one.** R130's own procedural fix is what produced it:
*a lane's closing recommendation is a claim about the tree as that lane saw
it* — so it was re-asked rather than implemented.

### The half of R89 that is TRUE, and is now traced end to end

**The consumer is real, composed, and complete.** The carriage from the tail's
`vertex_alpha` to the blend's alpha port is **ten hops with no constant, no
tie-off and no dangling bit anywhere inside the datapath**:

| # | file:line | what happens |
|---|---|---|
| 1 | `zhao_console_core.sv:5655` | `input logic [47:0] tri_continuation_tail_i` — **the open boundary** |
| 2 | `zhao_shell_top_v2.sv:188`, `:1143` | pass-through |
| 3 | `zhao_geom_bin_pipe_v2.sv:64`, `:236` | packed into the frozen 1157-bit metadata ABI at bits `[345:298]` |
| 4 | `zhao_geom_binner_v2` | stored and drained opaquely |
| 5 | `zhao_raster_tile_pipe_v2.sv:266` | `incoming_continuation_tail_w = job_meta_i[345:298]` |
| 6 | `zhao_raster_tile_pipe_v2.sv:657-658` | cast to `zhao_raster_continuation_tail_v2_t` |
| 7 | `zhao_raster_texture_stage_v3.sv:286` | `frag_vert_a_o = …post_earlyz.vertex_alpha` |
| 8 | `zhao_raster_tile_pipe_v2.sv:868`, `:957` | into `zhao_raster_fragment.frag_vert_a_i` |
| 9 | `zhao_raster_fragment.sv:737, 722, 461, 701` | `s0_va_r → s1_src_a_r → s2_src_a_r` |
| 10 | `zhao_raster_fragment.sv:490/493/496` | `zhao_raster_blend_prod.a_i` — **the consumer** |

The field law is `zhao_render_texture_pkg.sv:44-54`: `vertex_alpha` is bits
**[23:16]** of the 48, asserted by the package's own one-hot span self-test at
`:784-786`. So R89's sentence *"the console already has carriage for exactly
that"* is **correct and now documented rather than asserted**.

### THE CORRECTION THAT MATTERS: `ALPHA_C` IS NOT ON THIS PATH AND NEVER WAS

R48 and R89 are both written as though `ALPHA_C` were the seam that a shadow
alpha would replace. **It is not the same seam.** Searched every occurrence in
the tree:

* `ALPHA_C` is declared **once**, `zhao_geom_vattr.sv:193-194`, as a **32-bit
  fx16** parameter `32'h0001_0000`, and used **once**, `:537`, writing
  `rep_data_o[191:160]` — **per-vertex attribute slot 3**.
* Slot 3 **has no interpolator and no lane**: `zhao_geom_attrpack` emits
  exactly three planes and `zhao_raster_tile_pipe_v2.sv:601` has exactly three
  attribute lanes. `ALPHA_C` is written into a slot nothing reads.
* The blend's `a_i` is an **8-bit unit8** off the continuation tail, by the
  ten-hop chain above. The two never meet.
* A **third** opaque constant, `MAT_BASE_ALPHA_C = 8'hFF`
  (`zhao_console_core.sv:15205`), is the *texture* base alpha in the flat
  request, and is also not `a_i`.

**Three unrelated opaque constants, and none of them drives the blend.** This
does not disturb R89's ruling — flat-alpha is still the right route — but it
does mean the sentence *"`ALPHA_C` remains its named seam"* describes a
different feature (interpolated per-vertex alpha) than the one being produced
here, and a reader who replaces `ALPHA_C` will have changed nothing.

### THE VALUE R89 WANTS TO TRAVERSE HAS NO PRODUCER ANYWHERE

R89 names the producer precisely: *a caster's `strength_q`*. **`strength_q` is
latched from `cast_strength_i`, and `cast_strength_i` has no producer in the
tree, no source in the ladder, and no field in the ABI.**

* **`zhao_geom_lodstate` does not emit it.** Its caster output
  (`zhao_geom_lodstate.sv:188-195`) is exactly
  `{c_valid, c_instance_id, c_x, c_z, c_radius, c_rung, c_view}` — **no
  strength**. The header at `:180-187` is explicit that the block "emits the
  measured quantity and invents no factor".
* **Nothing else produces one.** Searched `strength` across all of `fpga/rtl`:
  every hit is SURFACE.STAMP's u16 (`zhao_cmd_exec.sv:381`), the FIELD stamp
  adapter, `DEBUG_RUMBLE`, `SET_ENVIRONMENT`'s `tint_strength`, or
  `zhao_shell_top_v2.sv:1178`'s `pg_strength_i(8'd0)` — a particle-glue port
  already tied to zero. **None is a shadow strength.**
* **No ratified command carries one.** Searched `spec/` and `design/`: the only
  hits for a shadow strength are this contract's own prose, `:56` and `:99`.

So FORGE.SHADOW's `cast_strength_i` is today **unproduceable**, and composing
the block would make it a **tie-off** — the one thing the campaign forbids. This
is the same shape as `ALPHA_C`'s own origin: a quantity a contract assumes and
no format carries.

### AND R89 WAS NEVER THE ONLY BLOCKER — FOUR REMAIN, ALL VERIFIED FIRST-HAND

R132 scheduled this work believing R89 was the last one. Walking every port
group of `zhao_forge_shadow` (`:106-164`):

| port group | blocker | status |
|---|---|---|
| `tap_*` (`:131-137`) | **NONE — this one is genuinely clear.** `zhao_terrain_heighttap` mirrors these ports signal for signal (its `:21`, `:168`) and is **already composed** at `zhao_console_core.sv:9233`. | ✅ |
| `cast_strength_i` (`:118`) | **no producer anywhere** (above). Owner decision. | ❌ |
| `cast_{x,z,radius,rung,src_id}` (`:115-120`) | `zhao_geom_lodstate`'s `c_*`, and **LODSTATE is not composed**; it needs `zhao_geom_ladderbank`, which needs an ENGINE1 share and a page-publication path. | ❌ |
| `rung_floor_i` (`:125`) | its only producer is the **uncomposed `zhao_measure_governor`** (R118, itself blocked at both ends). | ❌ |
| `vtx_*` (`:142-154`) | **no consumer.** Route A (batch, through `GEOM.GROUP_SEQ`'s `v_*`) **DEADLOCKS** on `zhao_geom_vattr.sv:490`'s `done_o`, a six-term AND requiring a lit rgb and a u/v that a shadow hull has neither of. Route B (private arena) is unbuilt and needs the client-A widening, an arbiter at GEOM.CLIP's door, and the absolute→rebased frame conversion. | ❌ |

**LODSTATE and FORGE.SHADOW are MUTUALLY blocked** — `c_*` has no consumer
because FORGE.SHADOW is uncomposed, and `cast_*` has no producer because
LODSTATE is uncomposed. **They can only be composed together, and even then the
other three blockers remain.** So FORGE.SHADOW is a **subsystem packet**, not a
wiring job, and it should not be scheduled as one again.

### What was deliberately NOT done, and why

Closing `tri_continuation_tail_i` the way `tri_flat_request_i` was closed
(`zhao_console_core.sv:15228`) was considered and **refused**. That closure is
legitimate because a *majority* of the flat request's fields come from a real
composed producer (`mw_pub_*`, MATERIAL.RESOLVE) and only the unproduceable
ones are named constants with rulings. **The tail has no such producer for any
of its four fields** — `zhao_console_core.sv:15198-15202` says so in terms, that
the vertex colour "is left at its constants deliberately rather than invented".
Building the tail from four constants would be **moving a tie-off from a port
into the core**, which closes a gap on the register while changing nothing in
the silicon. That is the campaign's first prohibition and it is also the
flattering move, which is why it is written down here rather than just avoided.

## Latency (fixed or variable)

`variable` — a height tap takes the terrain path's latency.

## Overflow and malformed-input behaviour

* **A caster with no ground beneath it** — over a void cell, off the island
  edge, or above the keel — emits **no shadow**, and that is correct rather
  than a fault. An airborne creature over a chasm should not have a shadow
  pasted at some default height.
* **A radius of zero** emits nothing.
* **Height taps that disagree wildly** (a cliff edge under the caster) still
  conform: the hull follows them. It may look odd on a knife-edge, and that is
  a content problem, not a hardware refusal.

## THE ONE THING THAT MUST NOT BE GOT WRONG

**Depth bias, and it must be authored rather than accidental.** CLAUDE.md's
ground-contact law applies directly: clipping through the ground must be
authored, never accidental, and *a belly resting at exactly zero reads as
hovering*. A shadow at exactly the terrain height z-fights with it; a shadow
biased too far reads as floating detached from its caster.

So the bias is a **named, editable constant per rung**, and its correctness is
decided **by looking** — a shadow that measures right and looks detached is
wrong.

## Scalar reference function

**PLANNED AND NOT WRITTEN**: `zref::forge::shadow_hull(rung, x, z, radius,
heights[])` returning the vertex list in emission order, and
`zref::forge::shadow_rung(distance, governor_floor)`.

## Directed tests

**PLANNED AND NOT WRITTEN**: each rung's exact vertex and triangle count; the
emission order deterministic and identical across two runs and under stalls; a
caster over a void emitting nothing; the governor floor overriding a near rung;
and the height-tap count fixed per rung regardless of terrain.

## Randomized differential tests

Planned, against the scalar model, over caster positions including island edges
and breach holes.

## Integration capture cases

None on hardware. **And a look-gate, because this is art**: a creature walking
across flat ground, a slope, a cliff edge and a breach, at 240p, watched in
motion. A contact shadow either sells the contact or it does not, and no
measurement decides that.

## Synthesis / resource ceiling

Expected **very small**: a vertex generator with a fixed table per rung, plus
the tap request logic. It has no arithmetic beyond placing vertices. The audit's
own assessment: *"may cost essentially zero new raster hardware."*

## Notes

The material architecture already mentions precise shadows as terrain-conforming
polygons; this contract is the **automatic, per-creature** case that had no
rule. The two are compatible: an authored precise shadow is content, this is
the default every creature gets for free.
