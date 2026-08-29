# Task Log: RUN-20260829-0525 - Zixxtrixx v9 cel-main art pass

**Created:** 2026-08-29 05:25 UTC+02:00
**Status:** Complete
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

### 2026-08-29 21:42 UTC+02:00 - Broad grounded travelling-wave balance accepted

- Rejected the historical raised spear and the first travelling-wave revisions after complete-sequence review: despite local motion, the animal still read as one rigid body balancing on fork tips. Increased two incommensurate phase-lagged waves, began the struggle during gather/rise, and retained independent per-segment authority so curvature visibly travels through the upper body at all times.
- Reauthored the raised pose into a difficult weight-bearing L. Eleven uneven upper segments approach vertical, three form an elbow, and five tapered support segments lie broadly along terrain. Positive support slopes compensate the taper by eye; the root plateau moved from the old `+580` fan-tip balance to the accepted `-255` broad-body support.
- Rejected a backwards support compensation that buried bone 14 and two distal blade-curl experiments that either worsened or failed to improve penetration. The accepted fan uses a named balance-only up-bias through the flop, stays visibly lifted, and returns exactly to canonical rest.
- Inspected all 447 canonical cel-main frames plus a native-size every-four-key review. Rise and plateau continuously change curvature; the lower body remains visibly supported; failure buckles progressively rather than rotating like a rod; and recovery contains no twitch or isolated broken frame.
- Extended the committed 3D probe to track actual skinned-vertex minima for every influencing bone. At every key and midpoint through keys 77–140, all six support regions b14–b19 remain present with minima `-12..14 mm`; plateau all/blade minima are `-34/-34 mm`.
- Declared shallow support and flop impact separately. Worst terrain contact outside the impact is `-35 mm` at key 155.5; the real baked midpoint is explicitly included as a one-presentation-tick impact lead; the flop reaches `-66 mm` at key 161 inside its authored `-70..-25 mm` band.
- Four rigid-transform-invariant upper-body chord spans vary by `16/21/9/49 mm`, proving actual shape travel rather than global rotation. Maximum 60 Hz posed-station step is `263 mm`; first/final roots and all palette channels are bit-exact.
- Rebuilt every dependent tool directly and passed posed contact/shape travel, strike-tip, exact frozen target interaction, camera/LOD/outline/fixed-point limits, mesh/seam, planner, choreography and redirected reel sequence-CRC gates. Promoted the accepted visual sheets and ten gate reports under `evidence/balance-*`; final golden repinning remains deliberately deferred to complete-v9 closeout.

### 2026-08-29 23:34 UTC+02:00 - Separate slow taunt and stronger loose falling flail accepted

- Preserved historical quick taunt slot 30 without editing its builder. Captured a golden before the animation source changes and compared it against the final clean direct-build golden: both 13,224-byte `clip-30.bin` payloads have SHA-256 `6823ae3a192915931e62617647b4f0daa472f0e48442fe6670512ca7e0c05f05` and compare byte-for-byte exactly.
- Added slot 44 as a separate 120-key slow taunt. Broad lower-neck changes initiate each left/right transition, the skull follows four authored keys later, and a stronger local tilt supplies the funny head-wobble punctuation while slow loose body life continues underneath. First and final root plus all 28 palette channels are bit-exact rest.
- Inspected all 239 canonical cel-main frames and native-size samples. The complete center-left-right-left-center performance is visibly slower than slot 30, smooth throughout, and free of isolated broken frames, snaps or twitch.
- Strengthened slot 4 with a two-cycle accumulated-pitch wave travelling through the whole chain rather than adding unrelated high-frequency joint rotations. The original slow nonuniform tumble, neck loll, lateral writhe, breathing authority and blade play remain intact.
- Inspected all 288 looping cel-main frames and native-size samples. The body now moves through stronger straight, C and S silhouettes with faster visible propagation while retaining a slow loose read. Rejected diagnostic scales 290000, 210000 and 180000 because they hid or grazed fan tips; the accepted fixed-side scale 140000 keeps every bend visible.
- Extended the committed posed probe over slot 44 and the complete slot-4 loop midpoint. Slow-taunt maximum 60 Hz station motion is 20 mm. Fall regional chord travel is `349/203/68/177 mm`, propagation lags are `32/32` presentation ticks, maximum 60 Hz motion is 149 mm, and seam half-steps are `130/128 mm`.
- Clean direct rebuild of every dependent creature tool passed. A repeat render is byte-identical across all 239 slow-taunt and 288 fall frames. Posed taunt/fall/contact/continuity, strike-tip, target, limit, mesh/seam, planner, choreography and redirected reel checks all pass; promoted fourteen numbered reports and visual sheets under `evidence/taunt-fall-*`.

### 2026-08-29 23:52 UTC+02:00 - Final cel-main distance continuity accepted

- Corrected the balance presentation subject to the canonical one-shot cadence already required by its accepted animation evidence: key zero, each real midpoint once, and final key once, for `2 * (224 - 1) + 1 = 447` frames rather than an extra loop-seam midpoint.
- Rebuilt the normal and cel reels directly and rendered complete final-source cel-main sequences at the actual close/mid/far cameras: idle 576 frames, balance 447, grounded-target salto 243, flying-target salto 245 and six-salto 265.
- Inspected the representative native comparison and complete-sequence outline telemetry. Close idle holds 4 px exterior ink at projected radii `367.7..623.4 px`; balance and grounded target hold 2 px at `209.7` and `170.7..181.4 px`; flying and six-salto hold 1 px at `140.0..155.2` and `106.2..125.2 px`.
- Every sequence keeps one stable width throughout, with zero threshold transitions or flicker. Smooth three-band light remains continuous across the tube, exterior-only ink preserves interior colour and concavities, and distant animals remain coloured silhouettes rather than black masses.
- Repeated all 1,776 final representative frames after the direct rebuild; every RGB frame is byte-identical. Promoted the native close/mid/far comparison, continuity report and deterministic-repeat report under `evidence/final-cel-*`.

### 2026-08-29 18:33 UTC+02:00 - Canonical media and archive presentation accepted

- Promoted the accepted fixed-side 288-frame fall diagnostic to the actual site subject instead of retaining the stale 576-frame orbit that hid deformation. Added explicit library entries for the one-turn jump, three-turn jump and 24 m nine-salto limit so every new v9 performance is independently renderable and discoverable.
- Rendered and looked through every frame of all 21 final cel-main subjects, 5,744 frames total. The poster mosaic, final fall and slow-taunt sheets, both spring-jump sheets, nine-salto sheet and representative look, balance, attack and hit sequences show no crop, broken eye/stripe, stray triangle, black distant silhouette, target loss or isolated bad frame.
- Encoded 21 canonical full-colour WebM/PNG pairs as VP9 CRF 16, 384x240 yuv444p at 60 fps with 1152x720 same-basename posters. Every WebM decoded completely; all frame counts match source; total encoded size is 9,232,358 bytes.
- Proved all 34 previously-current v8 normal media files were preserved byte-for-byte behind the archive before replacing current names. The assembled page exposes 21 current clips and six explicitly ordered archive generations while retaining `noindex, nofollow`.
- Exercised current and nested archive radio tabs in local Edge at 1410 px and a true 390 CSS-pixel viewport. Exactly one requested panel/generation shows, archive clips have controls and never autoplay, and both widths have zero horizontal overflow. Stopped the local servers and browser profiles and verified no process remained.
- Generated two independent final 28-bone golden dumps: all 46 payload/CRC files match byte-for-byte. Comparison targeted only committed Upheaval goldens: 45 final payloads versus 40 committed, five additions and no removals. The complete deliberate re-pin remains deferred until this presentation commit establishes the immutable final source SHA.
- Promoted selected final poster/contact-sheet, media/CRC, golden-comparison and desktop/narrow interaction evidence under `evidence/final-presentation-*`.

### 2026-08-29 19:06 UTC+02:00 - Final media mode omission found and corrected

- The first explicit all-subject repeat exposed a presentation-mode omission that the generic reel CRC check could not see: the initially encoded site frames used `zhao-reel-cel.exe` and therefore the calm cel texture page, but were rendered without the required runtime `ZIXX_EXP=celmain` selector. They consequently lacked the smooth three-band cel-main lighting and projected-size exterior contour despite the earlier acceptance note.
- Discarded that media set as canonical evidence. Directly rerendered all 21 subjects twice from immutable source commit `65350e0` with `ZIXX_EXP=celmain`; all 5,744 RGB frames are byte-for-byte identical across the two independent runs.
- Looked through the corrected poster mosaic and every frame of all 21 corrected subjects. The intended exterior ink is visibly present at close, gameplay and limit distances; the complete performances retain their accepted form, eyes, elastic stripes, targets and framing.
- Replaced all 21 current WebM/PNG pairs from the corrected frames and decoded every WebM completely. Counts match all 5,744 source frames; the corrected encoded set totals 10,086,599 bytes. Reassembled the page with `noindex, nofollow` intact.
- Repeated desktop and true 390 CSS-pixel current/archive interactions after correction. Both widths retain one visible requested panel, correct autoplay/controls behavior and zero horizontal overflow. Stopped both servers and all `zv9-final-*` browser profiles; ports 8765/8766 are no longer listening.
- Replaced the stale final-presentation visual/media evidence with the corrected cel-main output and added the explicit 5,744-frame repeat plus every final direct gate report. Golden repeat/comparison evidence remains valid because clip payloads were never affected by the renderer selector.

### 2026-08-29 19:19 UTC+02:00 - Integrated, published once and production-verified

- Deliberately re-pinned the committed Upheaval contract to all 45 final clip payloads and the 28-bone pose CRCs from the reproducible dump. The final dump byte-matches the clean committed Upheaval checkout; no staging golden was used. Refreshed the committed 3D probe, true cel-main sequence CRCs, four every-frame 60 Hz contact sheets and durable provenance naming immutable source commit `65350e04b4cabd357a28296f69713cd0c9b2a880`.
- Reassembled and validated the finished site: exact `noindex, nofollow`, 21 current WebM/PNG pairs, all four new clip entries, six requested archive generations in order and no missing media. Both Python tools compile, assembly is idempotent, and every one of the 5,744 encoded frames decodes from the 10,086,599-byte VP9 set.
- Fast-forwarded both feature branches over the latest remote mains without conflicts. The production-bearing Upheaval main is `d97f7a424c9015c9ffc128406760fa5ccf370964`; zhaozhou main reached evidence commit `4ee70c628d93c432f913c5adbc738036976116fe` before this closeout record.
- Invoked `website/deploy.ps1 -Project upheaval -Branch main` exactly once. Cloudflare deployment completed at `https://19a9bf54.upheaval.pages.dev`; production is `https://upheaval.pages.dev`.
- Fetched both production aliases with cache bypass. Their index bytes match the clean committed Upheaval main checkout, preserve noindex and expose all required current/archive entries. Downloaded every production WebM/PNG pair; all bytes match the accepted corrected cel-main media, all 5,744 VP9 frames decode, and direct visual inspection of the production idle poster confirms the projected-size exterior contour is present.
- Promoted `final-presentation-22-production-verification.txt`. All direct creature gates, repeat renders, media decoding, assembly, desktop/narrow interaction and production checks pass. Stopped and verified all renderer, encoder, browser, server and deployment processes; no listeners remain on ports 8765/8766 and no v9 background job survives.

---

## Subagent Spawns

| Timestamp | Agent | Purpose | Status |
|-----------|-------|---------|--------|
| 2026-08-29 05:25 UTC+02:00 | strong Zixxtrixx modelling agent | Complete all v9 art, render, archive, gate and delivery work serially | Complete |

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

None for this pass. Zixxtrixx v9 cel-main is integrated, deliberately re-pinned, published once and verified in production.
