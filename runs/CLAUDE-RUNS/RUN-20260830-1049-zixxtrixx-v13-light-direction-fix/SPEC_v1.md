# SPEC v1: Correct Zixxtrixx world-space light direction

**Run ID:** RUN-20260830-1049
**Created:** 2026-08-30 10:49 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Prove the executable normal, directional-light and transform conventions; repair the generic sign/space/orientation defect; then author and publish exactly one held-pose orbit named `Corrected Toplight 1` whose fixed world-space source visibly lights the dorsal surface from above.

---

## Scope

**In Scope:**

- A committed synthetic +world-up top / -world-up underside Lambert fixture.
- Representative posed Zixxtrixx dorsal and ventral packed-normal comparisons against true outward radial directions.
- Smooth and flat shading direction traces, bone/model/world/view transform trace, and direct-light-to-cel/material trace.
- A generic renderer correction if the evidence confirms a defect.
- Exactly one 600-frame, 60 Hz, ten-second held signature-S orbit, one VP9 WebM and poster.
- One prominent v13 website entry, preservation of the rejected v12 history and every archive, exact noindex, one deployment.

**Out of Scope:**

- Geometry, pose, animation, texture, pigment, cel-threshold or outline changes.
- Flipping the ten v12 constants, camera-relative light, ambient masking or an option family/grid.
- Catalogue regeneration, broad validation, Sacengine, CMake builds and gummy-spring work.

---

## Constraints

- Work only in the fresh ordinary clones under `zixxtrixx-v13-light-direction-fix-lane`; never touch shared repositories and never use a worktree.
- Read durable owner directions before work; direction #14 is binding and newest.
- Direct-build fresh dependent translation units; never run `cmake --build`.
- Stage exact paths only; never use `git add -A`; commit and push logical milestones.
- Render exactly one new animation and deploy exactly once with `website/deploy.ps1 -Project upheaval -Branch main`.
- Preserve exact `<meta name="robots" content="noindex, nofollow">` and stop all spawned jobs.

---

## Validation Budget

1. `V13-SIGN-FIXTURE`: known identity-pose top normal +Y with source-to-light +Y must return strong Lambert; underside -Y must return zero/low. Exercise smooth and flat lanes.
2. `V13-ZIXX-NORMALS`: a bounded set of posed dorsal and ventral vertices must have positive `dot(transformed_normal, outward_radial)`; record values and smooth/flat agreement.
3. `V13-SPACE-TRACE`: source inspection plus one executable camera-quarter-turn check must establish that bone/model transforms affect normals while view/orbit does not affect the light vector.
4. `V13-RESPONSE-TRACE`: one strong dorsal Lambert must survive gain, toon quantisation and material response into a bright dorsal pixel.
5. `V13-VISUAL`: inspect the four quarter-turn frames and the complete single orbit at 384x240.
6. `V13-WEB`: check only the candidate media, exact noindex and bounded desktop/narrow loading.

Stop when those six questions are answered; do not expand into a catalogue, exhaustive-frame campaign or broad suite.

---

## Don't Retry

- Do not infer +world-up illumination from positive rig Y values or comments; v12 did that and the visible result was inverted.
- Do not accept smooth/flat agreement as outwardness evidence; both lanes may agree while both are inward.
- Do not measure a posed surface against a mismatched pose or derive 3D form from a rendered projection.
- Do not tune around an unresolved convention defect.

---

## Open Questions

- Are generated packed normals outward, or did the historical negation invert an already-outward winding?
- Does each Lambert argument mean surface-to-light or incoming ray travel direction?
- Is any light vector transformed through model or view space before composition?
- Does the direct term remain correctly ordered through gain, cel quantisation and pigment response?
