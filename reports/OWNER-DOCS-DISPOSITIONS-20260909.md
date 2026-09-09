# Dispositions for the last two unread owner documents

2026-09-09. `reports/OWNER-DOCUMENT-INDEX.md` asks for exactly this and says why:

> **Superseded is a disposition and should be recorded as one** — an unread
> instruction and a satisfied instruction look identical from here.

Five owner documents were absent from this lane. Three were read earlier today
(`TERRAIN_31MHZ_REARCHITECTURE.txt`, `islandrearchitecture4.md`, the index
itself). These are the last two. Both read via `git show origin/main:<path>`;
the working tree was not touched and `origin/main` was not merged.

---

## `bumomapping.md` — 2026-09-05, `4094fd8a`, one line

> we need detail bump mapping for terrain. please architect it and set it up for
> production. I hope it is not too expensive but terrain is the star of the show
> and we neglected giving it first class treatment.

**Disposition: OPEN, unarchitected, and deferred — but not by the document people
would assume.**

It is *not* covered by `TERRAIN_31MHZ_REARCHITECTURE.txt`, which is about the
`zhao_pair_tess_normals` timing pair and says nothing about bump mapping. It is
*not* covered by `zhaozhou-terrain-mipmapping-architecture-2026-09-05.txt` (421
lines, present here) either — mipmapping is filtering, not surface normal
perturbation. So this is a **distinct architecture request with no document
answering it**, which is precisely the state that looks identical to a satisfied
one from a filename.

Deferred by the terrain brief's own §16 Step 0 — *"Finish the texture island. No
diversion of that closure effort"* — and by §0.2. Recorded here so that when
terrain opens, this is on the list rather than rediscovered.

**One thing worth flagging to the owner:** the sentence *"I hope it is not too
expensive"* is a cost question, and terrain already has one open cost problem —
the `31.10 MHz` pair. Bump mapping adds per-pixel normal work to a subsystem whose
existing arithmetic is the thing being rearchitected. The honest sequence is the
terrain brief's B0–B5 first, then price bump mapping against whatever the pair
becomes, not against today's numbers.

---

## `reports/ADDLIGHTNING.md` — 2026-09-04, `fba7899e`, 175 lines

Lightning as a **composition of existing effects blocks**, not a new family:

```
deterministic bolt path -> FORGE.PRIM ribbon geometry -> RASTER.FRAGMENT additive
  -> POST.GATHER glow tag -> POST.COMPOSITE bloom     (plus PART.* sparks)
```

with a deliberately bounded contract — *"Draw one deterministic ribbon with at
most 24 segments and at most two bounded branches"*, explicitly **not** *"any
number of branching antialiased electrical lines with arbitrary widths"*.

**Its central claim was TESTED, and it holds.**

> FORGE.PRIM is not fully usable yet… it currently owns topology only… the module
> literally has no position or params inputs. Its own header says that positions
> are supposed to come from a separate evaluator.

`fpga/rtl/forge/zhao_forge_prim.sv` takes a job descriptor — `j_family_i`,
`j_segments_i`, `j_sides_i`, `j_material_i`, `j_view_mask_i`, `j_src_id_i` — and
emits triangle **indices** (`t_i0_o`, `t_i1_o`, `t_i2_o`) plus material, src_id,
last and four counters. **No position port. No params port.** And its header line
39 says so outright:

> positions come from `params` through the evaluator, and a topology that
> depended on a position would stop being bounded.

Only two modules exist under `fpga/rtl/forge/` — `zhao_forge_prim.sv` and
`zhao_forge_cliff.sv` — so the evaluator is not unfinished, it is **absent**.

**Disposition: BLOCKED on an unbuilt block, and that blocker is now visible.**

`FORGE.PRIM.EVAL` has been added to `design/prod_manifest.yml`'s
`unpriced_requirements:`. It was previously the third class of silence from the
A 0 accounting work — not counted wrong, not unknown, but **having no line at
all**, so it never appeared even in the unpriced count. The census now reports six
unbuilt requirements where it reported five.

Lightning is the owner's stated want and its actual blocker was sitting in an
unread document rather than in the bill. That is the whole reason to record a
disposition.

**What this does NOT do:** it does not architect the evaluator, price it, or
schedule it. It makes the dependency countable. Building it is effects-lane work,
outside §0.1's authorised list, and it is not on the texture critical path.

---

## Both documents, and the pattern

Neither is a texture-island item, so neither is startable now. What both show is
that a document's *filename* and its *disposition* are unrelated:
`bumomapping.md` looks like it might be covered by two terrain documents and is
covered by neither, and `ADDLIGHTNING.md` looks like a feature request and is
actually a correct architectural analysis whose one falsifiable claim survives
checking.
