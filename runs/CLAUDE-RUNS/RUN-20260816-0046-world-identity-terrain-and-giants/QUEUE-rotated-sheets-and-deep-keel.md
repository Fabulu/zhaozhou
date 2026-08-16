# QUEUE — rotated terrain sheets, and the deep textured keel

*Owner requests, 2026-08-16. Both concern the same limitation: a heightfield is a 2-D sheet, and right now our islands look like paper.*

## 1. The deep keel — "terrain looks solid, not flimsy"

**Owner:** *"Sacrifice with its islands had a dropoff at the end of its sheet that was textured with a simple automatically made texture that looked like rock. Long dropoff. Made the terrain look solid instead of a flimsy sheet."*

**Measured, and the owner is right.** The `terrain-breach` demo digs 30 m through **~22 m of local thickness on a 320 m island** — a 1:15 thickness-to-width ratio. It reads as a sheet because it *is* one.

The donor's numbers (S5 §3.3, `sacmap.d:106, 504-522`): vertical curtain quads from every boundary vertex down to `top − mapDepth`, **`mapDepth = 50 m` fixed**, textured with the environment's `edge` texture; the underside is the top surface mirrored at −50 m with negated normals.

**Our position is architecturally better but visually worse right now.** We render rim walls to the *modelled bottom surface* — true local thickness rather than a constant — but nothing authors the bottom deep enough, and the walls carry placeholder flat colours. Both halves of the donor's trick are missing.

**Actions (terrain wave):**
- **Bottom-surface generation must produce a deep keel by default.** Pick the depth from the donor's 50 m as a floor, scaled to our island sizes; state the derivation. A shallow bottom must be a deliberate authoring choice, never the default.
- **Rock strata on the rim walls** — `FORGE.CLIFF` (specified, unbuilt) for the banding, `TEXTURE.MOSAIC` (specified, unbuilt) for the tiles. The donor's "automatically made texture" is the cheap version: a procedural/tiled rock band, no authored art needed. Our zero-blend diet already fits it.
- The keel is also what makes **breaches** read: a hole through 22 m is a scratch, a hole through a deep keel is a hole through a *world*.

## 2. Rotated terrain sheets — vertical structures from the same machinery

**Owner:** *"what if, along with the big terrain, we made some terrain stuff like skyscrapers and mountains out of several other, rotated sheets of terrain… 4 sheets of terrain and a top for a deformable skyscraper."*

**Verdict: feasible, and the mechanism is already half-specced.** `ADDENDUM-world-terrain-creatures-giants.md` §B2.1 requires GEOM.LOOM to **parent terrain-class patches under animated transform nodes** (the terrain-class giant — a walkable, deformable torso riding a skeleton). A rotated *static* sheet is the strictly simpler case of that same capability.

**Why it composes cleanly:** field programs evaluate in the patch's **local** lattice space. Rotate a patch vertical and a deformation program dents it perpendicular to its own face — precisely the desired behaviour for a wall taking an impact. The compose-lattice law (§4.3), the 4-corner breach law, the incremental bake law and the `no_bake` clamp are all indifferent to the patch's world orientation; they operate on the lattice. **No frozen-format change is implied.**

**The real work, none of it fatal:**
1. **Seams.** Four walls plus a top gives eight corner joins that must stay watertight under deformation. Decide whether sheets share edge vertices (expensive, exact) or are independently clothed with a join strip (cheap, donor-flavoured). Prototype before committing.
2. **"Which surface am I on."** `column_query` currently assumes one vertical column per cell. A world with vertical sheets needs a query that resolves multiple candidate surfaces along a ray, and a rule for what a creature stands on. **This is the substantive design item** — get it right before shipping vertical playfields.
3. **Breach continuity across sheets.** Punching through a wall must agree with the floor sheet it meets, or you get a hole that is solid from the other side.
4. **Budget interaction.** Each sheet consumes patch-list slots against the frozen `MAX_PATCH_FIELDS = 16` and `BAKE_PATCH_BUDGET = 64`. A skyscraper is 5 patches; a city block of them is not free. Cost it before promising cities.
5. **LOD and culling** against a screen-space-error governor that has only ever seen horizontal sheets.

**Why this is worth doing:** it converts our one real terrain limitation — no overhangs, no verticals — into a compositional trick, using machinery already built and already proven deformable. Deformable skyscrapers, mountains with genuine vertical faces, and (per the addendum) walkable creature-terrain all fall out of the same capability.

**Sequencing:** after the keel and texturing land. Those two make existing terrain look right; this one adds new terrain kinds. Do not start it while the renderer is still flat-shaded — a rotated flat-shaded sheet is unreadable.
