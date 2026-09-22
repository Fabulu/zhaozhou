# GEOM.CLIPDOOR — the N-producer door at GEOM.CLIP's input

RTL: `fpga/rtl/geometry/zhao_geom_clipdoor.sv`
Test: `tests/geometry/geom_clipdoor_directed.cpp` (175 checks)
Positive control: `tests/mutants/zhao_geom_clipdoor_mutant.sv` +
`tests/geometry/geom_clipdoor_mutant_control.cpp` (5 checks, inverted polarity)

---

## WHY IT IS AT THIS DOOR, AND NOT AT GEOM.SETUP'S ARM

Owner ruling **R187** took SETUPDOOR's measurement and named the place:

> *"the honest door is at **GEOM.CLIP's INPUT**, not GEOM.SETUP's — it yields
> winding normalisation, 2A, bbox, zero-area reject and three-tine lockstep for
> free."*

`zhao_console_core`'s own R197 block says the same thing from the other side:
*"it is also the door R187 names for every non-mesh producer … so the arbiter
that eventually admits particles and shadow hulls will present its `untex` bit
to THIS gate, not to a second copy of it downstream."*

`zhao_geom_setup`'s arm is **one tine of a three-way ordered join** — setup's
edge functions, `zhao_geom_attrpack`'s three 240-bit planes and
`u_material_window`'s resolved material — pairing by arrival order with no tag.
A producer entering there alone deadlocks the mesh pipeline combinationally.
Entering **here** feeds all three tines through the fork that already exists,
so no tine is bypassed and nothing about the join changes.

---

## THE TWO HALVES OF ONE BEAT

In the composed console the stream into GEOM.CLIP is not one bundle:

```
producer -> zhao_material_window (MATERIAL half) -> R197's untextured gate
         -> zhao_geom_clip (TRIANGLE half)
```

and the window is a **pure combinational gate** on that path — its body is
`assign t_valid_o = t_valid_i && pass_c; assign t_ready_o = t_ready_i &&
pass_c;`. It buffers nothing and reorders nothing.

So the beat offered at the window's material input **is** the beat whose
triangle is offered at GEOM.CLIP's data input, on the same clock, and one
arbiter owns both halves with no tag and no shadow FIFO. That is why this block
emits `o_material_*` beside `o_ax_o..o_untex_o`, and why the composer must drive
both from it. Two arbiters would be the metadata-swap shape wearing a material
record.

**The block must not gain a skid.** A buffer between the two halves puts them
one beat apart, which is the exact fault above.

---

## THE ARBITRATION LAW: RUN-LENGTH FAIR, NEVER BEAT FAIR

This is the one law here that is not obvious, and it is forced by a block
downstream rather than chosen.

`zhao_material_window` publishes **one** material for the whole span between
GEOM.CLIP's input and the shell's triangle door, and **drains that span before
it changes what it publishes**. Its header states the cost in terms — *"THE COST
IS A DRAIN PER MATERIAL CHANGE"* — and its converse: *"a repeat of the SAME
{set, id} … costs nothing at all: no drain, no request, no stall."*

The span is GEOM.CLIP's three stages plus GEOM.SETUP plus GEOM.ATTRPACK, and the
attrpack fork holds the pair to roughly **one triangle every fourteen clocks**.
A beat-fair round robin between two producers of different materials would
therefore issue a full drain *and a resolve* between every pair of triangles: the
pipeline runs at the drain rate, not the triangle rate, **with every counter in
the console reading healthy**.

So the grant is held while its owner keeps offering, and the grant may move only
on a clock where the owner is *not* offering. `geom_clipdoor_directed.cpp`
section 3 is the check a beat-fair arbiter fails: twenty contended beats must
produce **zero** switches.

---

## BOTH REMAINDERS ARE DISCHARGED — 2026-09-22 (packet PARTMAT)

**The section below is the 2026-09-21 measurement and it is kept verbatim,
because it is what made the work cheap.** Read it as a record of what was owed,
not as a statement of what is missing. Both items are now RTL and the door is
composed with THREE clients: GEOM.REPLAY (0), FORGE.ASSEMBLE (1) and
PART.CLIPFEED (2).

* **The canonical depth.** `w` and its depth profile now survive
  `zhao_part_project`'s slot store and ladder queue (`PROJ_W` 99 → 132) and
  `zhao_part_expand`. `zhao_part_clipfeed` converts them with the D-4 pair —
  `zhao_geom_depthquant_stream` beside `zhao_raster_rcp24_v4` — and reorders the
  converter's completion-order answers back into offer order.
  See `design/contracts/PART.CLIPFEED.md`.
* **The material law.** Ruled by the owner on 2026-09-22, ruling 1, and the
  recommendation this contract recorded is what was adopted: a producer declares
  it carries no material, and the window publishes a **defined no-sampling
  profile** for it **without a resolve and without counting a fault**.
  `NO_MATERIAL` is a **lawful mode**, never inferred — not from a failed lookup,
  a sentinel handle, the previous span's material, or the untextured bit. It is
  a third field of this door's MATERIAL half, selected by the same grant on the
  same beat as the `{set, id}` it qualifies.

**And the run-length-fair law is what made three producers affordable.** The
composed acceptance run costs **two resolves for three spans** — measured in
`tests/prod/partmat_acceptance.cpp` section 1, not argued.

**The register prediction below was correct**: composing the second arm did not
move it. What DID move it was declaring, honestly, a gap this work found —
entry **I51**, the per-primitive raster state, which no producer in this console
has. 10 → 11, and that rise is the instrument working.

---

## WHAT THE SECOND ARM STILL OWED — MEASURED 2026-09-21 (packet CLIPDOOR)

The door is built and unit-verified. It is **not yet composed**, and the reason
is two named, measured items. **Neither of them is the seven-slot attribute
packet**, which five passes over the FORGE cluster believed was the wall.

### The attribute wall is GONE, and this is the correction that matters

SETUPDOOR's binding blocker — *"`GEOM_CLIP_ATTRS = 7` … a polygon particle has
no u/w or v/w **by law**; zeroing them would sample texel (0,0)"* — was
**discharged by owner ruling R197 on the same day it was written**, and R197
names particles in its own text: Option B *"invents u/v for shadow hulls and
particles that have none by law"*.

R197 is **silicon at this commit**, not a plan:

* `zhao_geom_clip.tri_untex_i` / `out_untex_o` — the per-primitive declaration;
* `zhao_geom_attrpack.tri_untex_i` — the only reader of the u/w and v/w slots in
  the tree, and it branches on the bit;
* `zhao_console_core`'s `cl_in_untex_c` / `cl_in_refuse_c` /
  `geom_untex_refused_o` — the refuse-and-count gate R197 point 3 requires;
* `tests/mutants/zhao_console_core_untex_decl_mutant.sv` — its positive control.

A particle therefore enters with `untex = 1` and its u/w and v/w slots are
don't-care **by ruling**, read by nothing. `GEOM_CLIP_ATTRS` does not need to
change for particles, and it must not be narrowed (PROJOUT, re-affirmed by
ATTRLANE; D1 has since filled slots 3–5).

### 1. THE PARTICLE HAS NO CANONICAL DEPTH — this is the real blocker

`zhao_part_expand.t_d_o` is **Q16.16 1/w** (`zref::ScreenV::d`). Slot 0 of
GEOM.CLIP's packet is **invw24**, and `zhao_geom_depthquant`'s header carries
owner ruling **D-4** in terms: *"all downstream consumers receive only the
canonical invw24. No consumer performs its own profile conversion."*

The two are different quantisations of the same ordering, and they share one
depth buffer with the mesh path, so a Q16.16 value in slot 0 would z-test wrong
against every mesh triangle.

The conversion **consumes `w`, not `1/w`** (the block's own header: *"THE INPUT
IS w, NOT 1/w … It consumes w and performs its OWN reciprocal"*). So:

* `w` **exists** on the particle path — `zhao_part_project`'s `a_w_i`, re-exported
  as `h_w_o [30:0]`;
* it is **dropped at the ladder queue**: `proj_wr_c = {!a_behind_i, a_x_i, a_y_i,
  a_d_i, size8_c, size16_c}` carries no `w`, so `q_*` has no `w` and
  `zhao_part_expand` never sees one.

What the arm owes, therefore: `w` carried through `zhao_part_project`'s slot
store and ladder queue to a new `q_w_o`, through `zhao_part_expand` to a new
`t_w_o`, and a `zhao_geom_depthquant_stream` + `zhao_raster_rcp24_v4` pair on the
particle side — the pattern `zhao_geom_vattr` already holds, `u_dq` beside
`u_rcp`. That pair is an **area cost on the binding constraint** and owner ruling
R236 is explicit that cost is a fact to record, not a veto; it is named here so
the next packet prices it instead of discovering it.

### 2. THE PARTICLE HAS NO MATERIAL IDENTITY — and this one is a LAW

`zhao_material_window` publishes one material per span, and R197's door
**refuses** a declared-untextured primitive under a material whose
`sample_count != 0` — correctly, and counted on `geom_untex_refused_o`. So a
particle entering while a textured mesh material is published is *dropped*, which
would be closing a gap by narrowing function.

A polygon particle has **no material at all** in the reference:
`sprites.cpp::draw_population`'s tris branch calls `raster_tri(surf, vpp, a, b,
cc, p.r, p.g, p.b, tm)` with no `TextureSpan`.

The window already has a defined answer for a material it cannot resolve — *"This
block publishes the DEFINED FAULT MATERIAL for that case (sample_count 0, which
is the legal 'this surface takes no texture sample' profile) and counts it on
`no_record_o`"* (owner ruling R20) — but reaching it by presenting an identity
that deliberately does not resolve would make `no_record_o` fire on every
particle batch and stop meaning anything.

**RULED 2026-09-22, owner ruling 1, exactly as recommended below.** The
paragraph that follows was written as a recommendation *"recorded rather than
taken"*; the owner took it. It is kept because a recommendation that was adopted
is evidence about how the decision was reached.

**This was the sibling of R197 and it had not been ruled.** R197 settled the
*attribute* law for non-mesh producers; the *material* law for the same producers
is open. The recommendation, recorded rather than taken: a per-producer
**declaration** in the same shape as R197's — a producer may declare it carries
no material, and the window publishes the no-sample profile for it **without a
resolve and without counting a fault** — with `R48`'s `ALPHA_C` /
`GEOM_REPLAY_UNTEX_DECL` as the named-seam precedent.

### What does NOT block it, checked at this commit

* **The width.** PART.EXPAND's `signed [21:0]` against GEOM.CLIP's `signed
  [20:0]` is headroom, never range: `to_screen_xy` clamps to ±524288 and the
  largest fan offset is 4080, so `max |vertex| = 528368 < 2^20`. Proved
  exhaustively in `tests/particles/part_expand_directed.cpp` section 7, and the
  invariant was seen to fire with the clamp premise withdrawn. The door takes
  21 bits; the narrowing is lossless and provable.
* **The behind bits.** `zhao_part_expand`'s `assign emits = take && p_in_i` — a
  behind-the-eye particle is consumed and produces nothing — so
  `tri_behind_i = 3'b000` for the particle arm is a *fact about the producer*,
  not a tie-off.
* **The cull mode.** `CULL_NONE`, which is `zhao_geom_clip`'s own reset value and
  the reference's `const TriMode m;` default — `draw_population` is double-sided.
* **The flat colour.** The particle's `u8` r/g/b reach the fragment through
  Gouraud slots 3–5, live since R234 D1. The expansion is **derived, not
  invented**: the consumer's own law is `zhao_raster_tile_pipe_v2::lit_unit8(v)`
  = `v[15:8]` saturating, so `{c, 8'd0}` is its exact left inverse and round-trips
  the byte. A constant plane across three identical corners is what
  GEOM.ATTRPACK produces from three equal slot values; no special case is needed.
* **The winding.** The fan's `2A` is negative for every size and zero at size 0.
  GEOM.CLIP *normalises* winding and *rejects* zero area, which is precisely why
  R187 put the door here: both are handled for free by entering at this input.
  (Pinned by `part_expand_directed.cpp` section 8.)

### And what it does NOT close

Composing the second arm **does not move the completion register**. Measured at
this commit: the register reads **22** (9 tie-offs + 13 disconnected), its nine
tie-off entries are I13, I14, I17, I20, I21, I27, I29, I32 and I34, and **I24 is
not among them** — `// I24:` in `zhao_console_core.sv` is a port-group comment in
the module's port list, not an `// I<n>.` entry in the INCOMPLETE block, so
`tools/budget/completion_register.py` never counted it. PART.EXPAND is already
counted as *connected*. The work is worth doing because it makes particles draw;
it is not worth scheduling as a register item.
