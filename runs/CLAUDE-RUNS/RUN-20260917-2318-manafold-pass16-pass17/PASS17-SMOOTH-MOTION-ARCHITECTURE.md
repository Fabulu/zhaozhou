# Manafold Pass 17 — smooth antenna / continuous lightning architecture

**Date:** 2026-09-18
**Binding direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-18-2026-09-18.md`
**Scope:** read-only production trace and architecture. The active crown-shuffle worktree was inspected but not edited, built, or accepted.

## Decision

Direction 18 is a **full-bank continuity requirement**, not a crown-shuffle polish pass.

1. Author all antenna carrier motion through continuous, eased trajectories with no one-frame position/rotation step at keys, presentation midpoints, reversals, tableau arrivals, clip seams, or effect handoffs.
2. Keep lightning and particle identities persistent. A new lightning shape is reached by morphing the same named stations/topology state; a new path is reached by interpolating the same named bolt points. Nothing re-hashes, reseeds, reclassifies, disappears, or respawns on one frame.
3. Evaluate the exact 60 Hz production result. Gates trace decoded visible cores and the same FX evaluators the renderer consumes; native every-frame pictures decide whether the motion reads smooth.
4. Preserve independent A/B/C travel and readable top/bottom ownership. Smoothness is achieved with time, easing, correspondence and persistent state—not by collapsing amplitudes back into one hose.

## Production ownership trace

### Antenna pose

| Stage | Production owner | What it controls |
|---|---|---|
| Ambient A/B/C translation | `tools/reel/manafold_clips.h:nodule_schedule` (currently around 1851) | Nine independent sinusoidal tracks, sampled at authored 30 Hz keys. |
| Ambient articulation | `antenna_knead` (around 2037) | Junction/neck/A/B/C/End quaternions from fold envelopes, out-of-plane sine, press-wave wag, per-cycle accents and hold tremor. |
| Public five-carrier input | `swallow_nodules` (around 1483) | Front/End rotations plus A/B/C target translations; `PublicJointMute` acts at this real consumption point. |
| Crown shuffle | `taunt3_order_target/blend/pose` (around 1527–1606), consumed in `build_taunt3` (around 4151) | Four rankings, fold offsets, endpoint punctuation and body support. Active worktree timing is six-key transitions, four-key holds and a six-key release. |
| Signed skin / closure | `loop_pose`, `Rig::set_span_delta`, rear finalizers | Solves carrier quaternions, signed helper translations and body-attached End closure. |
| Authored keys | `Rig::write` | Commits quats/local translations/scale to each 30 Hz clip key. |
| Presentation frames | `reference/src/zcreature/creature_core.cpp:bake_presentation_midpoints` (188–401), then `finalize_rear_follow_midpoints` | Constrained cubic/nlerp midpoint generation; End closure is re-solved after the bake. |
| Visible picture | `decode_pose` + compiled loop skin | What the viewer and continuity gate must read, never merely the requested target tables. |

### Effect attachment

`zhao_reel.cpp` (currently around 4480–4559) decodes the exact presentation pose, extracts Root/JunctionF/Neck/A/B/C/LoopBase2 origins, and constructs `FxAnchors`; `ring` is the posed JunctionF/A/B/C centroid. Every fold figure, free lightning strand, shape mote and ordinary particle consumes these anchors in the same presentation frame. A one-frame carrier step therefore moves the entire effect before any independent effect motion is added.

### Folded lightning figure

- `fold_phase` (`manafold_clips.h`, around 1674) deterministically chooses `shape_from`, `shape_to`, phase envelopes, morph progress and occasional full turn.
- `fold_stencils` / `fold_weights` (`manafold_fx.h`, around 2072–2350) provide 18 stations per figure and their six-anchor barycentric weights.
- `mana_fold` (around 2476) evaluates the phase at **presentation cadence** (`frame * 8` in Q4 key units), morphs station positions, applies shape rotation/knead/lasso transforms, then draws topology edges and particles.
- Existing topology logic cross-fades source-only and destination-only links by `morph_pm`; common links remain present. This is the right ownership model.

### Free lightning and particles

- In the pushed baseline, `mana_lightning` selected a new hashed diameter and jagged path from `frame / kBoltRehashFrames`; endpoint bursts and the centre glint were also keyed to that boundary. The whole strand therefore changed state on one frame.
- The active worktree has a promising **provisional** repair: `lightning_morph_phase`, `free_lightning_path` and `bolt_path_morph` keep named path points present and interpolate deterministic from/to phases; brightness, endpoint pulses and surge motes now follow continuous clocks.
- Shape-edge jag previously used `bolt_path(..., frame / 3, ...)`, replacing its interior offsets every three frames. The active tree provisionally routes it through `bolt_path_morph`.
- Shape-mote identities use stable `hm = hash(m)`, but shipping particles still receive discontinuous `hash(frame / 2, mote)` knead offsets (`manafold_fx.h`, currently around 2995), and drag consumes recent anchor-velocity samples. Any carrier step is therefore amplified by both centre motion and lag.
- Particle population/role ownership is derived from integer `n_motes`, `n_wander` and `n_shape`. When those values change (notably life/crowd fades), an ID can appear/disappear or cross the shape/wander boundary rather than retaining one role with a continuous visibility envelope.
- Fold shimmer in the baseline used a fresh per-frame hash for gain. The active tree provisionally changes it to a persistent sinusoidal phase, which is the correct direction.

## Confirmed causes of the observed behavior

### 1. Crown timing is below the house motion floor

The active crown tables are:

```text
transition 6 keys = 12 presentation frames
hold       4 keys =  8 presentation frames
release    6 keys = 12 presentation frames
```

`07-MOTION-STYLE.md` records **16 presentation frames as the minimum for a beat to register** and warns that pace, not distance, caused prior “spazzy” failures. Four large hierarchy-carrying A/B/C reorders in 46 keys, plus endpoint/body punctuation, ask attached effects to reverse too often. Cubic smoothstep makes positions C1 at the table endpoints; it cannot make an undersized interval look unhurried, and it does not give zero acceleration/jerk at joins.

Direction 18 also supersedes the existing three-key Taunt-III dismissal for antenna/effect channels. A large attached crown throw cannot be mandatory-smooth and complete in six presentation frames merely because it is called a punchline.

### 2. Per-cycle accent authority changes discontinuously

`knead_accent_pm` hashes the integer cycle and selects a different lead station. `press_wave` is **nonzero at the cycle boundary** (its mapped value is `-65536` at both ends), so changing the gain/lead on that boundary changes the resulting quaternion immediately. Worse, the accent cycle is keyed from unlagged `f` while each station's `FoldPhase` is lagged; a lagged station can still have nonzero agitation when the shared accent identity changes.

The hold tremor is also selected by `ph.seg == kSegHold`; it can enter or leave at an arbitrary sine value. Both mechanisms can add a key step even when their underlying sine/smoothstep is continuous.

### 3. Short clips crush the figure morph

When a full drift/gather/hold/knead cycle does not fit before release, `fold_phase` proportionally compresses it and allows `knead` to fall to a one-key floor. That can turn an otherwise 48–88-frame shape morph into a two-presentation-frame replacement. A short clip must show fewer/longer phrases, not the same number crushed into less time.

### 4. The pushed lightning path and fold-edge jag replace hashed state

The baseline free strand re-hashes endpoints and every jagged station at a fixed cadence. The baseline folded edge re-hashes interior offsets every three frames. Those are direct off/reappear/teleport mechanisms. The active morph helpers address them numerically but remain unaccepted until every boundary is reviewed at native resolution and their positive controls recreate the old replacement.

### 5. The fold edge has a binary presence owner

The pushed path draws the complete figure only when `coh >= kFoldEdgeCohMinPm`. Area/coherence changes with the antenna, so crossing that line turns all edges off or on together. The active tree's named nonzero presence floor is structurally appropriate for a living conduit, but must be multiplied by `life_pm` so a death's authored fade can still reach zero continuously rather than showing the floor until a hard `return 0`.

### 6. Shape stations are persistent but not explicitly corresponded

All 12 figures have 18 stations, and current morphing pairs station `i` with station `i`. Their semantic layouts differ radically: ring angular samples, star centre/spokes, bar dual strokes, cross dual strokes, open curls and closed polygons. The interpolation is mathematically continuous, but some pairings can send a station on a long crossing route and make the topology churn. First trace every actual ordered shape pair. If any route is visually wrong, author a named station correspondence/permutation for that pair (and map destination edges through its inverse); do not let a distance optimizer mechanically choose the shipped acting.

### 7. Particles add discontinuous state on top of anchor motion

At full knead, each particle receives up to 70 mm of newly hashed offset every two presentation frames. This is a literal particle teleport. Integer population/role boundaries can also pop/reclassify IDs. The drag ring then magnifies raw hinge-B velocity by 2.6× (bounded only by a 380 mm displacement cap), so a carrier snap produces a larger field snap. These layers explain why the owner's particle/lightning read is worse than the antenna alone.

### 8. Presentation midpoint smoothness is not yet proved

The generic bake is substantially safer than a chord: constrained cubic local/root/deform samples and cubic-renormalized quaternion midpoints, with exact held-channel handling. It is still generated from 30 Hz endpoints. Direction 18 requires tracing every actual 60 Hz frame. If alternating key/midpoint velocity or acceleration remains visible, Taunt III must evaluate the same continuous schedule at half-key time and own its hero midpoint samples rather than tuning around a generated staircase.

### 9. The active release handoff has a correctness trap

The provisional `fold_phase` release branch intends to morph the last active figure back to the opening figure. In the inspected implementation, the global `kq4 >= release_at` branch executes during the **first loop iteration**, before the phase walk advances `shape` through completed cycles. Its `shape_from` can therefore be the opening shape rather than the actual final shape, recreating a hard replacement exactly at release. Determine the shape at `release_at - 1 presentation step` first, then morph that stable identity to the opener. Add a seam mutation/control; do not infer this from `morph_pm` reaching 1000.

## Narrow robust repair

### A. One C2 time vocabulary for attached motion

Add a named integer quintic smootherstep (`6t^5 - 15t^4 + 10t^3`) for channels that carry antenna/effects. It has zero velocity **and acceleration** at both ends. Keep `punch_ease` for channels that do not move the antenna/effect frame only when final pictures earn it.

- Re-budget the crown phrase with editable transition/hold/release durations. Ladder the durations (not amplitudes first) until each large reorder has enough native frames to arrive and settle. The current 6/4/6 schedule is the rejected control, not a starting acceptance point.
- Separate Taunt-III's body-comedy timing from its attached-crown timing if necessary: the joke may arrive quickly, but the physical crown/effect frame must use a C2 route of sufficient duration.
- Audit **all clips**, especially Startle splay, Taunt/Taunt-II presses, lasso, deaths and the three-key dismissal. Direction 18 says “at all times,” not only keys 100–146.
- Replace `ph.seg == HOLD ? tremor : 0` with a C2 hold-entry/exit envelope.
- Cross-fade hashed accent gains and lead-station authority from cycle `n` to `n+1` on a loop-periodic C2 clock, or retire accents if an ablation shows they are not visible. Never switch lead identity while its carrier envelope is nonzero.
- On short clips, reduce phrase count and preserve a named minimum morph duration. Never compress the shape morph below the house 16-frame registration floor.

Keep independent high/middle/low targets and hierarchy compensation. The time law changes first; amplitudes remain owner knobs.

### B. Persistent lightning identity

Retain the active direction, after correcting and verifying it:

1. **Free strand:** fixed strand ID and point ID; `path[from][point] -> path[to][point]` with C2 morph, continuous brightness, endpoint pulse and surge-mote phase. No `frame / cadence` direct selection in a drawn path.
2. **Fold-edge jag:** use the same persistent point morph for every topology edge and all three navy/shimmer/white passes.
3. **Figure stations:** `shape_from -> shape_to` remains one persistent 18-station state. Topology source edges fade out while destination edges fade in; their combined figure energy never hits zero while `life_pm > 0`.
4. **Presence:** replace the coherence boolean with a continuous named envelope and an alive-floor; multiply by the continuous life fade so death can reach zero without a final hard blink.
5. **Release/seam:** resolve the real current shape before entering release, morph it to the opening shape, and make the last presentation sample adjacent to frame zero in stations, topology energy, bolt jitter, brightness and particle state.
6. **Correspondence:** trace all actual shape pairs. Add explicit editable pair correspondence only where the same-index route fails by eye.

`kFoldEdgePresenceFloorPm`, lightning morph cadence and path jitter remain named art controls. Their shipped values are selected by native render-look-adjust, not by the continuity gate.

### C. Persistent particle identities

- Evaluate a fixed ID domain. Give each ID one permanent role (shape mote or wanderer) and stable hash seed for the clip lifetime.
- Replace integer population truncation/reclassification with a continuous per-ID visibility/gain envelope derived from crowd/life/garnish. An ID fades; it never becomes a different species or respawns elsewhere.
- Replace `hash(frame / 2, mote)` knead jitter with loop-periodic interpolated noise or a hashed-phase sine. Preserve the 70 mm knob as an amplitude control; remove the teleporting clock.
- Keep independent particle motion required by Direction 13: particles do not inherit stencil rotation/skew/topology. They may follow the smoothly moving effect centre and retain their own smooth orbit/wander.
- Feed drag from the verified smooth anchor velocity. Fade the drag buffer to its seam state; never reset a visible nonzero displacement at frame zero.
- Replace per-frame hash shimmer with persistent smooth phase (the active sinusoidal change is appropriate), and gate gain continuity separately from geometry.

## Committed gate design

Extend the existing `mspan` continuity section only if it remains readable; otherwise factor its production samplers into `manafold_motiongate.cpp` / direct target `msmooth`. Do not create a second copy of the rig or FX maths.

### Exact production traces

For every shipping clip, every key and midpoint, and the loop seam (or held tail):

1. Decode the production pose and extract visible-core F/A/B/C/End centroids and endpoint orientation references from compiled skin.
2. Record position, relative quaternion angle, velocity, acceleration and jerk per carrier; record every adjacent loop-ring step as the existing posed-span gate does.
3. Use one shared `fx_anchors_from_pose` helper from both renderer and gate so anchor extraction cannot drift.
4. Through pure production evaluators, record stable IDs for:
   - each free-lightning strand point;
   - each folded figure station and topology edge gain;
   - each shape/wander particle position, role and visibility;
   - total alive figure energy, morph progress and brightness.
5. Write one CSV keyed by `{slot,presentation_frame,entity_id}`. Produce trajectory/speed/acceleration/jerk and effect-energy views with the committed plotting tool; the plots verify authored motion and do not generate values.

### Assertions

- no undeclared one-frame carrier position/orientation discontinuity;
- no key/midpoint velocity staircase or acceleration/jerk impulse above the final by-eye-approved band;
- no extra direction reversal inside one declared transition;
- tableau holds are actually held and transitions have their named minimum duration;
- lightning/mote IDs persist; no role or seed changes;
- shape boundaries are monotone in morph progress and connect exactly (`old.to == next.from`);
- source/destination topology gains cross-fade and alive figure energy never blinks to zero;
- death/release fades reach zero continuously when intended;
- all geometry, gain, particle and drag state is loop-periodic or held according to the clip contract;
- independent top/bottom ordering and both signed span directions remain green.

Thresholds are named only **after** native every-frame acceptance, with headroom. Begin comparisons with the house bands in `07-MOTION-STYLE.md` (16+ frames per beat, accepted jerk/step family), but do not fit a gate to the current rejected answer.

### Attributed positive controls

Controls must mutate production inputs/evaluators, not just add an offset inside the checker:

- `--fail-antenna-snap`: production crown schedule inserts one key step; carrier/effect continuity fails.
- `--fail-accent-switch`: restores instantaneous per-cycle gain/lead replacement; angular continuity fails.
- `--fail-lightning-switch`: production lightning evaluator uses direct integer-phase rehash; stable path IDs fail.
- `--fail-shape-blackout`: production edge envelope reaches zero at a shape handoff; energy continuity fails.
- `--fail-particle-reseed`: production mote seed/role changes at one boundary; persistent-ID position/role checks fail.
- `--fail-release-shape`: release starts from the opener rather than the actual last figure; seam/shape checks fail.

Each mutation must name its own failure while unrelated checks remain live. Keep detector operands independently clocked: consecutive decoded/render-consumed states versus prior state, never two fields written by the same failing enable.

## Visual evidence contract

After normal and all controls are green:

1. Render complete every-presentation-frame sheets at native 384×240 for Hover, fixed-idle/Crackle, Channel, Startle, Taunt, Taunt II, Taunt III, Lasso, both deaths and every live mana/lightning subject.
2. Render fixed front, three-quarter, side and rear diagnostic cameras. A moving camera cannot certify smooth appendage motion.
3. For every actual shape boundary, crown transition/reversal and clip seam, commit ±12-frame native windows and exact nearest-neighbour 4× crops.
4. Select additional windows by badness: maximum carrier step/acceleration/jerk; maximum fold-station, lightning-point and particle step; minimum alive figure energy; every identity/role boundary.
5. Place normal beside the antenna-snap, lightning-switch, shape-blackout and particle-reseed controls from the same binary.
6. Review lightning and particles both enabled and separately ablated. If disabling effects changes the perceived antenna smoothness, trace which effect layer amplifies it before touching antenna amplitude.
7. Reconfirm the owner-required A/B/C top/bottom witnesses, Front/End attachment, open O outline, solid contour mist and face readability. Smooth wrong motion is still wrong.

Uniform hero stills and a few shape samples are not evidence. Pass 15's “every 12th frame” sheet proved that shapes existed and was structurally incapable of seeing an off/reappear boundary between samples.

## File and commit boundaries

### Smooth-motion mechanism

- `tools/reel/manafold_art.h` — named C2 timing, presence/noise/cadence knobs.
- `tools/reel/manafold_clips.h` — crown timing, accent/tremor handoffs, short-clip phase planner, correct release identity.
- `tools/reel/manafold_fx.h` — persistent lightning/figure/particle state and pure trace evaluators.
- `tools/reel/zhao_reel.cpp` — validated same-binary controls and shared anchor extraction only.
- `tools/reel/manafold_spangate.cpp` or focused `manafold_motiongate.cpp` — whole-bank production continuity traces and attributed mutants.
- direct/CMake registration only if a focused gate is factored.
- this run's implementation report, gate logs, CSV/plots and every-frame evidence.

Do not mix Trick selection or final Taunt-III punchline orientation into the continuity mechanism commit. First make the motion/effect substrate continuously representable and green; then author those performances on it.

## Acceptance sentence

Direction 18 closes only when every visible antenna carrier, lightning station and persistent particle ID travels continuously through every 60 Hz frame and seam; old and new lightning shapes coexist/morph without an energy blackout or reseed; attached effects no longer amplify antenna acceleration; A/B/C still own large independent top/bottom motion; all production-path discontinuity controls fire; and complete native every-frame motion—not sparse hero samples—looks smooth.