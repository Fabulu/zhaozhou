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

### 2026-08-29 11:27 UTC+02:00 - Complete form, eyes and coordinated pupil system accepted

- Reauthored the full nose-to-tail profile through named regional radius controls, including the gentle nose/neck/front/middle/ground progression and one sustained thin-tail contrast. Judged complete side silhouette beside `Side.png` and in the accepted walk camera, never generated from a 2D measurement.
- Moved both eyes noseward, strengthened only their local head support, removed the painted static slit, and added two mirrored pupil bones with one deterministic target-led gaze intent. Added a compile-time `ZIXX_PUPIL_MOTION=0` frozen control.
- Built each orange eye marking as a seven-ring skinned elastic ribbon: boundary tips stay with the painted eye/head bind, shoulder rings blend, and the core follows the pupil. Vertical/diagonal travel extends one arm while contracting the other.
- Added slot 45's 64-key static-head acceptance sweep and fixed side, front, opposite-flank, close and gameplay-distance reel subjects. Inspected all committed normal/cel-main every-frame sheets, extrema, holds, reversal and settle, plus moving/deforming idle and look clips. No gap, overshoot, detachment, width accident, texture swim, edge escape, snap or crossed/independent gaze was seen; gameplay motion remains a restrained visible 1–2 pixel change.
- Same-pose normal/faceted/smooth/cel-main/unlit/normal-visualization/wireframe diagnosis found no actual neck seam. Exterior-only contouring removes the false internal joint ink while preserving the real sky-visible hook concavity.
- The new grounded form initially changed slot 30 through shared stance slopes. Added a historical local stance override used only by `build_taunt()`. All 26 legacy bone channels and root channels now match the committed v8 quick-taunt payload byte-for-byte; only the two appended identity pupil channels and 28-bone header are new.
- Clean direct rebuild of every dependent tool succeeded. Posed-vertex probe, mesh check and slot-30 compatibility pass. Two independent 42-file golden dumps are identical. Normal and cel pages each regenerate identically twice and match the tracked headers at `ad321900...f8b` and `04359015...a3d`.
- Committed durable numbered form, seam, static-pupil, frozen-pupil and real moving/deforming every-frame evidence under `evidence/`; `form-eye-00-acceptance.txt` records all named controls, CRCs, hashes and adjudication.

### 2026-08-29 14:49 UTC+02:00 - Shared compression, jump and six/nine-salto milestone accepted

- Replaced the local front-only squash with one shared, named whole-animal concertina used by attacks and jumps. Native side/top every-frame review shows the head, neck and front descending strongly into an almost-flat read while every rear region joins the same compression and the tube retains volume.
- Added one deterministic programmable jump builder. Slots 46 and 47 share compression, six-interval visible release, 4,800 mm apex, authored landing bite, absorption and exact settle while selecting one or three complete turns. The first four-interval release visibly popped and was rejected; every revised native release and landing frame was inspected before accepting the narrow 1,150 mm landing-step guard.
- Preserved slot 35 as a distinct six-turn attack at exactly 12,000 mm smooth authored trajectory apex and added slot 48 as a nine-turn watchdog attack at exactly 24,000 mm, exactly twice the six-salto trajectory apex. These declared apexes exclude the orientation-dependent wheel re-pivot. Stable target entry/hold, 1,198 mm extraction, delayed recoil, landing and exact recovery are authored phases rather than renderer freezes.
- Made identical authored hold endpoints remain identical at the real baked 60 Hz midpoint. Target checks skin actual posed watchdog/winged-watchdog triangles into rendered world space after independent terrain-column snap, smoothed ground tilt, facing and translation; terrain checks walk the actual posed attacker mesh at every key and midpoint.
- Canonicalized one-shot and spring evidence to `2 * (keys - 1) + 1` samples: key 0, every real midpoint once, and the final authored key once without cadence wrap. Accepted counts are 143/143 jump samples, 265 six-salto, 367 nine-salto and 59/59 spring diagnostics.
- Visually inspected every canonical frame plus native matched release, landing, six/nine flight, target entry, hold, extraction and spring phase sheets. The revised launch is continuous; the remaining fastest jump sample reads as the deliberate landing slam; both wheels remain coherent; target embedding is stable and extraction visibly clears before recoil.
- Direct clean rebuild and all committed probes pass: posed contact/overlap/spring/jump, striketip duration/apex/turn/hold/extraction/recovery, actual target triangles, camera/LOD/outline/fixed-point limits, mesh seams, planner and choreography. `zhao-reel --check` was redirected and reports all sequence CRCs match.
- Fresh one-salto and nine-salto rerenders are byte-identical to the accepted renders. Golden comparison used only committed Upheaval goldens; slot 30 remains byte-identical across all 26 historical bone channels and complete root, no staging golden was used or modified, and final v9 repinning remains deliberately deferred.
- Independent read-only review caught three proof/planner faults before commit. The target-triangle gate now reproduces rendered world transforms; moving-target lead converges through the actual impact key (key 70, exact target/intercept `(14400, 4000)` mm); and generic attacks retain the 12,000 mm ceiling while slot 48 alone opts into 24,000 mm. Spin count now follows the final locked apex, making slot 34 a coherent two-turn 6,407 mm shot; all 245 canonical frames were inspected after rerender.
- Rebuilt every dependent tool directly and reran all seven motion gates plus redirected `zhao-reel --check` after the review fixes. Every gate passes; post-review one-salto and nine-salto renders remain byte-identical to accepted output.
- Promoted 13 numbered visual sheets and 13 numbered acceptance/gate reports under `evidence/motion-*`. Task #12 is complete; balance, new taunt and hit/fall work remain separate milestones.

### 2026-08-29 18:48 UTC+02:00 - Hit, air recoil and exact victim freeze accepted

- Reauthored the generic/front reaction as an immediate asymmetric fold through a visible struck length, a full-body shove and a delayed wave through the grounded run into the supporting tail. Extended the clip from 40 to 50 authored keys so the delayed tail envelope finishes naturally at bit-exact rest instead of leaving residual motion in the final pose.
- Iterated the mirrored side reactions four times by looking at complete native sequences. Rejected technically-valid versions that foreshortened the animal into a compact blob; selected a strong local hairpin plus 229/230 mm whole-body side shove that retains the readable long body. Back now surges forward around a separate down/forward fold; top has its own deep crush and duck.
- Strengthened standalone slot 16 into a smooth complete-spear bow with skull counter-whip and blade flare while retaining exact phase seams. Inspected every native slot-48 frame through entry, exact embedded hold, extraction, recoil and landing; the victim stays still, extraction clears first, and the stronger recoil remains controlled rather than twitchy.
- Extended the committed posed probe with accepted-image comparison envelopes for struck-section displacement, delayed tail propagation, directional distinction/mirroring, low-frequency station continuity, exact rest and whole-spear bow. Measurements gate the eye-authored result; none generated an art value.
- Extended actual victim-triangle validation to compare both the complete decoded victim bone palette and rendered world transform exactly from impact onward. Slots 33, 34 and 48 pass entry/hold/extraction at every key and midpoint with `victim frozen pose+world exact`.
- Rebuilt every dependent creature tool directly and passed the posed probe, strike-tip, target, limit, mesh/seam, planner, choreography and redirected reel sequence-CRC gates. Promoted five every-frame/native visual sheets and nine reports under `evidence/impact-*`; final golden repinning remains deferred to complete-v9 closeout.

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
