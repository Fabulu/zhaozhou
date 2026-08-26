# SPEC v1: Zixxtrixx remake — concept match, continuous shell, real textures

**Run ID:** RUN-20260826-1615
**Created:** 2026-08-26 16:15 UTC+02:00
**Status:** Active

---

## Objective

Rebuild Zixxtrixx from the concept sheets. The existing model is a successful
technical prototype and an unapproved likeness; it is not to be polished
conservatively because it compiles.

Authoritative task text: `zhaozhou/reports/MODELINGGUIDE`. Ordered plan:
`Upheaval/creature/Zixxtrixx/REMAKE-WORKPLAN.md`. What the sheets actually show:
`REMAKE-BRIEF.md`.

---

## The framing that governs every decision

**This is NOT a hardware-budget rescue job.**

- The console resolves to **RGB565**. Measured: the worst quantisation error
  across the whole creature palette is 6/255 on one channel. The format is not
  a constraint on this creature.
- **The 256-colour rule is a GIF-export gate only.** It cost the creature its
  orange, its mouth and its throat transition, and distorted the four remaining
  colours. It must never again decide what the creature looks like.
- **`RingPart` is an offline authoring convenience, not a hardware limit.**
  Hardware receives compiled meshlets. A custom-mesh path may be added on the
  TOOLING side. Silicon is not to be redesigned.

---

## Scope

**In scope, in the workplan's order:**

0. Recon subagent (running) — evidence report, parent owns the design.
1. Art direction: upright compact S, theatrical and slightly ridiculous.
2. Static model matching the concept FIRST, with silhouette overlays.
3. Continuous skinned shell — shared boundary vertices, no internal caps,
   2-bone blended joints. This is what fixes the cracks.
4. A real, reusable creature texture pipeline; crayon surface; longitudinal UVs.
5. RGB565 master render; full-colour website format; GIF demoted to fallback.
6. Caterpillar locomotion — vertical and longitudinal, not lateral.
7. Triple salto mortale attack **ending as a straight spear coming straight
   down** (Fabian's own words at the head of MODELINGGUIDE; the distilled
   workplan lost this detail).
8. A/B 30 Hz held poses against 60 Hz presentation interpolation.
9. Falling flail loop.
10. Separate, SLOW preview cameras (one revolution over 12-16 s).
11. A measured LOD ladder, every rung crack-free.
12. The verification gates in MODELINGGUIDE §12.

**Also required, and also lost from the distilled workplan:** the first
animations are kept in an **archival tab** on the website. Fabian, MODELINGGUIDE
line 2: *"The first animations we keep in an archival tab."*

**Out of scope:**

- Publishing. `deploy.ps1` needs an explicit `-Project` and `-Branch` and
  refuses a page without `noindex`. Having a name does not authorise it.
  (Note: the site WAS published to upheaval.pages.dev in RUN-20260826-0617 on
  Fabian's explicit instruction, and the Pages project now exists. Re-publishing
  still needs him to say so.)
- Voicelines / audio. Recorded as character direction that should inform MOTION.
- Game behaviour for particle-simulation, compositor or 2D blocks.
- Any silicon change.

---

## Constraints that are not mine to change

- <= 2 bone influences per vertex; <= 32 bones
- meshlet <= 64 unique vertices, <= 96..126 triangles
- 30 Hz animation keys, 2 sim ticks per key (subject to the §8 A/B)
- determinism: replay-exact, integer authoring path
- nothing is copied from Sacrifice; measurements and derived laws only

---

## Don't Retry

- **Do not take the MEDIAN of a crayon region to get its colour.** It returns
  the pigment lightened by paper showing through, differently per region. The
  first pass did this, then hand-saturated to compensate, and recorded the
  compensation as art direction. Correct method and evidence: `PALETTE.md`.
- Do not switch a joint between `quat_y` and `quat_z` on a magnitude threshold
  (the current attack does this). It pops. Compose rotations properly.
- Do not reduce ring segments to satisfy a palette budget. Six sides were
  chosen for the GIF gate and must not survive by inertia.

---

## Open Questions (for Fabian, not to be decided alone)

1. **The tail has TWO prongs in the concept, not three.** The current model has
   three. Confirmed by looking: two long tapering blades, green centre, pink
   edging, with a small notch at the fork.
2. **The guide asks for BROADER, flatter, leaf/blade/fin-like tail forms than
   the sheets actually show** — the drawn blades are narrow lancets. That is a
   deliberate art-direction change, not a concept match. Which wins?
3. **Green disagrees between sheets**: Side is a grass green (120,184,68), Front
   is cooler and more emerald (67,193,124). Side wins by default as the larger,
   more definitive statement of the body. Confirm.
4. **The conversation transcript MODELINGGUIDE §0 tells the agent to find is not
   in the repo.** Those phrases appear only in MODELINGGUIDE itself. Treating
   MODELINGGUIDE as the conversation; not claiming to have read a transcript
   that is not there.
5. **The black ink contour** on every form in both sheets is a large part of why
   the drawing reads as a drawing. There is no outline pass. Worth one?
