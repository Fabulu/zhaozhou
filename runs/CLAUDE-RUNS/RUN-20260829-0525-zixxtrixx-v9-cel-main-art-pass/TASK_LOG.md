# Task Log: RUN-20260829-0525 - Zixxtrixx v9 cel-main art pass

**Created:** 2026-08-29 05:25 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260829-0525-zixxtrixx-v9-cel-main-art-pass/

---

## Objective

Refine the promising-but-unsuccessful v8 cel experiment into Zixxtrixx's main presentation: distance-aware outlines, smooth legible toon lighting, calmer cel texture, corrected head/neck/eyes, stronger whole-body animation, stable target-embedded salto impact, and a generational site archive that preserves prior work without dumping it all at once.

---

## Progress Timeline

### 2026-08-29 05:25 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260829-0525.
- Created `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-4-2026-08-29.md` as the durable home for the owner's new art direction.
- Captured six ordered tasks: pass setup; cel presentation; head/neck/eyes; balance/taunt/hit/fall/salto; site promotion/archive; final gates/delivery/publication.
- The strong dedicated modelling agent from v8 will be resumed with full context; the coordinator will not author model or animation changes.

### 2026-08-29 05:41 UTC+02:00 - Prerequisite read and isolation complete

- Read `00-START-HERE.md`, all four durable owner directions, the complete Zixxtrixx worklog/supporting notes, the two named reports, creature construction/rig/animation/texture/budget law, `spec/creature_rules.md`, `spec/qformats.md` (including section 8), `spec/stars_and_flares.md` (including the direct-colour filtering law), `docs/CREATURE_ANIMATION_APPROACH.md`, and `docs/BUILD.md` before implementation edits.
- Confirmed there is no committed Zixxtrixx report newer than v8. The dirty synthesis report belongs to the independent FPGA lane.
- `docs/coding_agents/SUBAGENT_GUIDE.md`, referenced by `00-START-HERE.md`, does not exist. This is a stale pointer, not a substituted instruction.
- Created clean development branches `zixxtrixx-v9-cel-main` from `origin/main` in both repositories. The zhaozhou branch points at clean `1fa2cc1`; final delivery will not inherit the current working branch's unrelated `854a3df`/`3ec58bc` ancestry.
- An independent `field_v3_svcpath` mutation sweep is active and is currently rewriting only its declared RTL source. No branch switch, relink, configure, or staging of sweep-owned files will occur until it ends and its owner restores/verifies the tree. Creature-source inspection remains read-only meanwhile.

### 2026-08-29 06:20 UTC+02:00 - Whole-body proportion direction received

- Read durable `OWNER-DIRECTION-5-2026-08-29.md` before any proportion edit. It overrides any local nose/neck-sculpt reading of direction #4.
- Task #11 now owns the complete nose-to-tail radius progression: nose only a little thinner than the subtly-fullest neck; a long front run about as thick as the nose; a middle S only a little slimmer; a still-weighty grounded walking run; then the one strong contrast, a sustained taper into a very thin tail. Except for that tail taper, regional contrast must be flattened into gentle continuous changes with no bulbs or steps.
- Every proportion iteration will be judged and committed as a matched full-body side silhouette against `Side.png`, with the accepted walk pose beside it. Head crops and mismatched poses will not choose radii.
- Eye placement/bulge and exterior-only internal-neck-outline diagnosis remain required.

### 2026-08-29 17:02 UTC+02:00 - Shared-tree wait bypassed with clean local clone

- A WebSocket/context interruption broke the original coordinating session while the independent FPGA mutation sweep was still active.
- Waiting for that sweep became excessively conservative and delayed the first creature milestone for hours even though clone-local implementation could have proceeded safely.
- Created the separate local clone `work/zhaozhou-v9` (not a worktree), based exactly on clean `origin/main` revision `1fa2cc1ea6beb79daaf8e767472c3d323879d653`, and created branch `zixxtrixx-v9-cel-main` there.
- All remaining zhaozhou creature edits, direct builds, renders, commits and pushes will stay inside this clone with clone-local outputs; implementation will not return to the dirty original checkout.
- Added a clone-adjacent `work/Upheaval` junction to the real Upheaval repository so the texture generator retains its expected concept-asset path without duplicating assets.
- The FPGA dispatch and svcpath sweeps independently finished cleanly after the clone decision: dispatch 30/30 caught; svcpath 30 caught plus 7 proven equivalent; both reported zero survivors and clean source restoration. No sweep process was stopped or touched.
- Task #9 (pass setup and isolation) is complete. Task #10 (cel presentation) is now in progress.
- Process correction for this and future concurrent lanes: isolation must be established before work begins. Every lane in one repository gets its own clone or worktree and its own build/output directories. A run folder separates evidence; it does not isolate a checkout. No lane may let its branch switch, generated build tree, linked executable, or tracked-source sweep block another lane.

### 2026-08-29 17:48 UTC+02:00 - Cel presentation wave 1 accepted

- Extracted the renderer's projected-bound-radius law into the public `projected_bound_radius_q8` helper and routed `compose_creatures` through it without changing the default render.
- Added an isolated `ZIXX_EXP=celmain` selector: smooth three-band toon plus a projected-size contour, while every legacy selector and the archived inward contour remain unchanged.
- Cel-main visibility uses RGB-or-depth change, a four-neighbour border flood to classify exterior, and eight-neighbour outward dilation. It never overwrites a creature pixel and does not ink enclosed holes.
- Authored the first accepted native-camera controls by eye after telemetry: far/mid/close anchors 120/200/360 px and widths 1/2/4 px. Idle retains strong 4 px ink, balance resolves at 2 px, dummy salto at 2 px, and the distant fly/six cameras at 1 px.
- Added deterministic `--cel-main` texture generation with fixed pigment anchors, no hue drift, calmer atlas/fin grain, lower wobble and reduced RGB565 dither. The cel payload is separately selectable through `ZIXX_PAGE_VARIANT`.
- Verified 1,582 default frames byte-identical after the renderer refactor. Normal generation repeated byte-identically at `c90fa2a...c2b`; cel generation repeated byte-identically at `fc0113e...e8d`.
- Accepted numbered evidence `evidence/cel-presentation-01-adaptive-outline.png` and `evidence/cel-presentation-01-results.txt` at native 384×240. Task #10's first presentation milestone is ready to commit and push; form and animation remain untouched.

---

## Subagent Spawns

| Timestamp | Agent | Purpose | Status |
|-----------|-------|---------|--------|
| 2026-08-29 05:25 UTC+02:00 | strong Zixxtrixx modelling agent | Complete all v9 art, render, archive, gate and delivery work serially | Running |

---

## Files Created

- `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-4-2026-08-29.md`
- This run's `TASK_LOG.md` and `SPEC_v1.md`
- `evidence/` will hold committed comparisons and every-frame diagnostics.

---

## Decisions Made

- Cel is promoted only after it reads successfully; the v8 cel result remains archived evidence, not a quality claim.
- Outline authority is a projected-size/camera-distance art knob: strong close, substantially thinner far, never filling the animal.
- Smooth-surface toon is the main lighting direction because the owner rejects polygon-to-polygon discontinuities; faceted cel remains archive/experiment evidence.
- Old taunt is preserved. The slower neck-led left/right wobble is a new clip.
- Prior normal main and experiment menu are archived as labelled generations before replacement.
- Publishing remains authorised when the complete pass is finished, gated and worth looking at.

---

## Next Steps

1. Resume the strong modelling agent; read all durable direction including owner direction #4 and audit published v8.
2. Refine cel outline, lighting and texture with close/far comparison evidence.
3. Iterate head/neck/eyes against Side.png and unlit seam evidence.
4. Reauthor balance, add the new taunt, strengthen hit/fall, and stabilize target-embedded salto impact.
5. Promote refined cel and restructure archive generations.
6. Render all clips, run all gates, commit/push logical milestones, publish and verify production.
