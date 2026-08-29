# SPEC v1: Zixxtrixx v10 lighting and rigid-spring repair

**Run ID:** RUN-20260829-2226
**Created:** 2026-08-29 22:26 UTC+02:00
**Status:** Active
**Previous Version:** `RUN-20260829-0525-zixxtrixx-v9-cel-main-art-pass`

---

## Objective

Deliver and publish a Zixxtrixx v10 that preserves the approved v9 whole-body proportions and rendering style while repairing systemic lighting assignment/flicker, structurally eliminating malformed eye polygons, reconnecting the tail fins, making the constrained face changes, and replacing the rear-curling spring with a shared rigid-S top-down compression whose head reaches ground and whose complete silhouette releases coherently.

---

## Scope

**In Scope:**

- Preserve the accepted v9 whole-body radius progression and resting S exactly unless a non-form implementation repair requires no authored proportion change.
- Reproduce and root-cause the systemic lighting assignment/flicker across normal Gouraud and true cel-main, fixed cameras, normal and deforming clips.
- Add temporal per-surface lighting diagnostics, debug views, badness ranking and all-frame native contact-sheet review.
- Find and structurally repair recurring malformed eye polygons across every clip, camera and both presentation modes.
- Move both eyes further noseward while preserving bulge, coordinated gaze, pupil movement and elastic orange stripe boundary contact.
- Moderately enlarge the mouth without returning to the old nearly/twice-as-large mouth.
- Reauthor tail-fin topology/attachment/outline so fins visibly join the body in every pose, view and distance.
- Replace the shared spring with a mostly rigid-S top-down descent, head-to-ground contact, optional small authored head flatten, almost-flat full-S compression, no rear roll/curl, and coherent whole-S release.
- Apply the corrected spring to `jump-one`, `jump-multi`, every salto variant, and attack anticipation; inspect attack contact and landing too.
- Render and inspect every frame of every production clip at native 384x240 in the required modes/cameras.
- Run direct-built posed/contact/strike/target/limit/mesh/planner/choreography/reel/golden/determinism/media/browser gates and two byte-identical independent true cel-main generations.
- Preserve and archive v9 as a usable generation; regenerate all v10 PNG/WebM; decode every frame; preserve prior archive media and exact `noindex, nofollow`.
- Integrate current remote mains without losing hardware commits, publish exactly once when complete, verify production and immutable deployment/media URLs, and stop all lane jobs.

**Out of Scope:**

- Re-tapering, rebuilding or otherwise changing the approved whole-body proportions.
- Creature-ownership migration or release of that migration lane; parent session owns the post-v10 handoff.
- Hardware/RTL edits or interference with the frozen hardware/migration lane.
- Sacengine execution, worktrees, shared-checkout edits, shared build/render outputs or `cmake --build`.
- Global brightening, toon-threshold widening or geometry-density changes used to conceal lighting defects.
- Deleting any `website/public/renders/archive-2026-08-27-*` or `archive-2026-08-28-*` file.

---

## Constraints

- Work only in isolated clones:
  - zhaozhou: `work/zhaozhou-v10`, branch `zixxtrixx-v10-lighting-spring`, base `54e74372367fa389d74d7bf74125352ae7bc6bf7`.
  - Upheaval: `work/Upheaval-v10`, branch `zixxtrixx-v10-lighting-spring`, base `d97f7a424c9015c9ffc128406760fa5ccf370964`.
- One sole implementation/modelling agent; no subagents.
- Read both `CLAUDE.md` files, every durable owner direction, architecture report, reports newer than v9, v9 closeout/evidence summaries and these templates before source edits.
- Diagnose and explain lighting and eye-artifact roots before claiming a fix.
- Author by eye at native resolution; measurements compare and guard but never choose art values.
- Keep every shape, colour, light and timing control named and editable.
- Never run Sacengine, use a worktree, invoke `cmake --build`, stage via `git add -A`, or touch shared checkouts.
- Direct-build every dependent `.cpp` after any struct-layout change; use lane-local outputs only.
- Inspect all production frames, not sparse uniform samples; gates are regression evidence, not likeness evidence.
- Commit and push each logical milestone as it lands.
- Preserve exact `noindex, nofollow`; deploy once only after the entire pass is finished and worth looking at, with `website/deploy.ps1 -Project upheaval -Branch main`.
- Stop and verify every render/build/encoder/browser/server/deploy job before handoff.

---

## Required Work Order / Tracker Mapping

1. **#17 setup:** durable owner direction, prerequisite read, SPEC/TASK_LOG, clean branch/base/output record.
2. **#18 lighting/eye artifacts:** reproduce, diagnose, explain and repair systemic lighting plus malformed eye topology; add diagnostics and evidence; commit/push.
3. **#19 constrained face/fins:** noseward eyes, moderate mouth, continuous fin attachment without body-proportion changes; render/look/adjust; commit/push.
4. **#20 rigid-S spring:** completely replace shared rear-curling compression and inspect required all-frame sequences; commit/push.
5. **#21 full validation:** every production frame plus complete direct gate, deterministic generation, media and browser validation.
6. **#22 publish/handoff:** archive v9, integrate mains, regenerate/promote v10, publish exactly once, production verification, stop jobs and report exact SHAs/URLs.

---

## Acceptance Evidence

- Fixed-camera every-frame native comparisons of pre-fix versus fixed normal Gouraud and true cel-main.
- Normals, unlit, debug-light and topology/wire evidence sufficient to identify root causes.
- Badness-ranked lighting frames and temporal per-surface diagnostics with explicit pre/post results.
- Full all-clip/all-camera/all-mode malformed-eye-polygon review or an equivalent exhaustive committed diagnostic plus visual inspection.
- Native face and tail-fin evidence across representative close/far, side/front/opposite/three-quarter and deforming poses.
- Every-frame `jump-one`, `jump-multi`, `salto-six`, `salto-nine`, attack contact/landing evidence proving whole S descent, head ground reach, no rear curl and coherent release.
- Every production clip reviewed frame-by-frame for lighting, eye polygons, fin connection, contact, twitch and preserved v9 behaviour.
- Direct gate outputs, two true cel-main byte-identical generations, complete media decode/counts and desktop/narrow browser checks.
- v9 archive preservation inventory and v10 production/deployment byte verification.

---

## Don't Retry

- Treating the current rear-body roll/curl as a spring and merely changing its amplitude.
- Generating compression by uniformly scaling or paper-thinning body cross-sections.
- Retapering the accepted body to accommodate face, fin or spring work.
- Global brightening or wider toon thresholds as a lighting repair.
- Assuming cel thresholds are the root before comparing shared normals/transforms/interpolation/state.
- Fixing only the named knockdown eye frame instead of the structural artifact class.
- Hiding fin seams by weakening every outline or painting over detached topology.
- Returning to the historical nearly/twice-as-large mouth.
- Sparse frame sampling; it misses isolated broken frames.
- Trusting a gate without looking at the complete rendered creature.
- `cmake --build`; the known regeneration race can execute stale binaries.
- Publishing an intermediate save or half-fixed milestone.

---

## Open Questions

None blocking. Root-cause investigation chooses the smallest reversible renderer/model repair supported by fixed-camera every-frame evidence; art values are selected by native-resolution visual judgement.
