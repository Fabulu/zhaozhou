# Zencrifice — working rules

Three repos: **zhaozhou** (the console — silicon, reference oracle, tools),
**nanquan** (the language), **Upheaval** (the game, full title *Tribute
Upheaval*; the folder stays `Upheaval/`).

---

## WHEN MAKING MODELS, ANIMATIONS AND TEXTURES, WE ARE MAKING ART

**Measurement never trumps actually looking at things.**

Fabian, 2026-08-26, after a creature was rebuilt from measurements and came out
worse than the version authored by eye.

This is not a caution about being careless with numbers. It is the opposite
failure: a measured number *feels* like evidence, so it stops getting
questioned, and it quietly replaces the judgement that was supposed to be doing
the work. Three things went wrong in one pass, all the same error:

* **The body taper** was derived from a distance transform of the concept
  drawing. That is not the 3D body radius — it is the half-width of a drawn
  outline in the picture plane, conflating real thickness with foreshortening,
  with measuring across a *bend* instead of across the body, and with the places
  the shape overlaps itself. It was trusted precisely *because* it was "a
  measurement". The hand-authored version looked better.
* **The dorsal pink** was measured off the scan and shipped as "the sheet's own
  pigment". But a scanner flattens and lightens, and a pale rose that reads as
  pink on a white sheet under room light reads as grey-white on dark ground at
  240p under one key light. Matching the paper is not matching the READ, and the
  read is the thing. The earlier, "wrong", hand-saturated value was closer.
* **The crayon grain** was clipped to ±16% of luminance — narrower than the
  light rig's own range, so it was mathematically present and visually invisible.
  It measured fine and looked like flat plastic.

**The rules that follow:**

1. **Author by eye. Render. Look. Compare. Adjust.** That loop is the job.
2. **Measurement belongs on the COMPARISON side**, checking what was authored
   against the reference — never on the generation side. A tool that says "you
   are 20% off" is good; a tool that decides a radius is not.
3. **Measure things that ARE the thing, never a projection of them.** A pigment
   is a pigment. 3D form derived from a 2D drawing is not.
4. **Measurement can remove a BIAS; it cannot choose a VALUE.** Finding that a
   sampling method skewed pale is useful. The shipped colour is still chosen by
   looking at it in scene, at final resolution, against what it sits on.
5. **A drawing is an interpretation, not a scan.** The artist thickened forms
   where they needed visual weight and reached for the crayon that was in the
   box. Converting that mechanically keeps the artefacts and loses the intent.
6. **Never remove the owner's control in the name of fidelity.** Every shape,
   colour and timing value belongs in a named, editable constant. "This is
   generated from the reference, so it is not a knob" is how a wrong number
   becomes an unadjustable wrong number.
7. **Component checks passing is not likeness evidence.** A palette verified
   against the sheets, a light rig verified against a face table and a mesh
   verified by CRC can all pass while the creature is unrecognisable. Look at
   the whole thing, in motion, against the concept.

## Seeing the work properly

Judging an animation from a handful of evenly-spaced stills does not work —
uniform sampling finds the typical frame and misses the broken one. Prefer:
contact sheets of every frame; trajectory plots of tracked points over time
(a flat line IS "it never bobs"); sampling frames by *badness* rather than by
index; fixed orthographic diagnostic cameras; and before/after pairs so
regressions are visible.

## Ground contact

Clipping through the ground must be **authored, never accidental**. A belly
resting at exactly zero reads as hovering, and a spear that stops at the surface
reads weightless — so deliberate, declared penetration is correct and its
*absence* is also a bug. Each clip declares where, when and how deep; anything
outside that is the fault.

**Measure it with a COMMITTED 3D pose probe, not from the rendered frame.**
The obvious shortcut -- find the terrain in the image and count creature pixels
below it -- is unsound and will report a confident, wrong number: with a ground
plane receding in perspective, an animal STANDING on the ground is always below
the horizon, and one 2D frame cannot separate "in front of the dirt" from
"inside it". It reported 94.8% submerged on a clip that was barely touching.
Real penetration means walking every posed vertex of every clip frame against
terrain height. A probe that does this was written once and thrown away, so its
numbers are unreproducible -- commit the probe.

---

## Process

* **Every session is a RUN**, created with `zhaozhou/runs/CLAUDE-RUNS/init-run.ps1`,
  logged as it happens, archived when done. **Every creature gets its own run.**
* **Commit and push as the work happens**, not batched at the end.
* **Barge ahead.** Decide on your own judgement rather than stopping to ask;
  state the assumption, make it cheap to reverse, keep moving. Blocking costs a
  working session; a wrong call costs one edit. This does not extend to
  destructive or outward-facing actions.
* **Publishing is always an explicit call.** `Upheaval/website/deploy.ps1`
  requires `-Project` and `-Branch` and refuses a page without `noindex`.

## Build note

`cmake --build` intermittently loses a race regenerating `build.ninja` against
Verilator, and the shell then runs the **stale binary and reports the old
numbers**. A measurement that did not move after a change that must have moved
it is the tell. Compile the reel directly instead, and after any struct-layout
change **recompile every `.cpp` that uses it** — a stale object with an old
layout looks exactly like a rendering bug.
