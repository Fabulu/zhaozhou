# QUEUE — atmospheric rain, and spell-driven sky darkening

*Owner request, 2026-08-16: "we want atmospheric rain effects that also darken the sky as the clouds come in. Spells might cause this."*

## What the ratified sky already gives us (read before designing)

`spec/sky_and_beams.md` has the cloud layer as a first-class citizen:
- **Cloud sheet** `sky_cloud_sheet`: 8×8 vertex grid (128 tris), Z-test on / Z-write off, **alpha blend** `sky_cloud_fade` (`out = dst·(1−a)+src·a`, `a = tex.a × vertex.a`), 256×256 ARGB4444 with u/v-repeat + mips.
- **Per-vertex alpha is baked at generation** — `α=(1−r²)·max_alpha` — and `max_alpha` is **per sky-set**. Scroll is tick-driven and deterministic.
- **The sun already reads through the cloud** additively; hard punch-through is an explicit revisit hook (ARM per-tick sub-grid vertex-alpha update, capture-exact).
- `DrawSky 0x0310` carries `sky_set`, cloud scroll u/v, `drum_yaw`, layer flags; **`0x0311..0x031F` are reserved for extensions** — the amendment path exists and was reserved for exactly this kind of thing.

**Implication: the darkening half of this request is mostly a sky-set problem, not a new architecture problem.** An overcast sky-set (darker drum ramp, higher `max_alpha`, heavier cloud) plus the existing blend path gives "clouds come in, sky darkens" with **zero new hardware** — it is asset + parameter variation on a ratified lane. The spell-driven part is state driving a **crossfade between sky-sets**, which needs a law (see below).

## Design questions to settle (spec work first)

1. **Sky-set crossfade law.** Darkening must be deterministic and capture-exact. Options: (a) discrete set switch at a tick (simple, may pop); (b) authored intermediate sets (cheap, memory cost per step); (c) a per-tick interpolation parameter carried in an extended DrawSky — needs the reserved opcode and a version bump of this file. **Decide with a cost for each.** The Noctis discipline suggests (b) with few steps; the CLUT-ramp heritage means sky palettes swap cheaply.
2. **Does the drum ramp darken, or does a cloud veil darken it?** The elevation-ramp continuity law (§1.2) must survive the transition — the seam fix is recent and load-bearing. If darkening is done by drawing denser cloud over everything instead of re-ramping the sky, continuity is inherited for free and the cost is blend fill. Quantify the fill.
3. **Rain itself.** Candidate lanes, in ascending cost:
   - **PART billboard streaks** (the donor's own approach: S4's P1 atlas particles, per-species gravity and bounce flags — a `rain` species is a table row). Cheap, proven pattern, hardware lane already chartered.
   - **2-D screen-space rain** via the existing TWOD block if it is a post-style overlay — investigate what TWOD's contract permits; if it can drive a full-screen effect within budget this may be cheapest of all.
   - **Streaks in the resolve path** — likely refused territory (§26 pressure); do not design here without checking.
4. **Rain collision/ground response.** Splashes (P2 sprite billboards) and terrain wet-darkening (`SURFACE.STAMP` material conversion — the same lane that does Frozen-Ground-proper). Wet-darkening the terrain sells the storm more than the rain itself does; it reuses the ratified stamp material-conversion law.
5. **Spell causality.** "Spells might cause this" — the weather state must be sim-side state that the capture records (the `celestial_state` chunk discipline from `spec/stars_and_flares.md` is the precedent: replay from any captured frame reproduces the weather bit-exactly). A storm is a first-class game state, not a renderer toggle.
6. **God beams through storm cloud** ties into the existing queued beams work — the occlusion_intensity law already reads the heightfield; cloud occlusion is the sibling question. Coordinate rather than duplicating.

## Sequencing

After the effect-library wave (running) and the queued clouds/beams work, since both build machinery this wants to reuse. Spec-first: this note's questions answered as an addendum to `spec/sky_and_beams.md`, then reference-model implementation, then reel subjects (`storm-roll-in` is the obvious gallery piece: bright sky → overcast → rain → clearing).

## Owner-facing one-liner

Clouds, darkening, and rain are asset-and-parameter work on a ratified lane plus a particle species and a state law — no rearchitecture, pending the crossfade decision.
