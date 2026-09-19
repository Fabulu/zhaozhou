# Manafold Pass 17 smooth-ordering code review

**Date:** 2026-09-19
**Scope:** uncommitted post-`e150a384` / `31949dea` crown, carrier, lightning, particle, lab, renderer and gate packet
**Method:** read-only source/diff/report review; no build, render, source edit, staging or commit

## Verdict

**PASS after repair.** The review findings below are retained as provenance. All checker gaps are closed and selected A80 brings the held punchline inside its signed budget without weakening any gate; the clean post-palette targeted generation later reproduced the complete packet in `PASS17-FINAL-TARGETED-INTEGRATION.md`.

## Findings

### P1 — Hold-last playback after the final sample is absent from both continuity gates

**Files:**

- `tools/reel/manafold_spangate.cpp:1026-1048`
- `tools/reel/manafold_motiongate.cpp:223-239`
- `tools/reel/manafold_qa_p12.cpp` (Fall Q5 proves final key/midpoint equality, not settling after it)

`mspan` evaluates exactly `frame_count * 2` presentation samples. Looping clips use modular history, but `hold_last` clips merely start at frame 3 and stop at the final midpoint. `msmooth` similarly appends frames 0–2 only when `!clip.hold_last`; a held clip receives no repeated final samples.

That misses the transition from the last authored velocity to the zero velocity of the held state. It also misses stateful FX evolution while the animation frame remains clamped: `FoldState` filters can continue changing on repeated calls with the same final frame. Fall is now deliberately hold-last, and both deaths hold their final pose. Exact final-key/final-midpoint equality does not prove that the next three held ticks have bounded acceleration/jerk or stable effect state.

**Failure scenario:** Fall reaches its final presentation midpoint with a nonzero carrier/effect velocity. The next runtime tick repeats the same animation frame, making carrier velocity zero while stateful coherence/drag continues settling. The visible hitch occurs after the encoded final sample, so Q5, G9 and `msmooth` all remain green.

**Required correction:** for every `hold_last` clip, replay at least three additional samples with the final key/subframe clamped exactly as production playback does. Include those samples in carrier and all FX position/brightness velocity, acceleration and jerk histories. Add a control that preserves final key/midpoint equality but leaves a nonzero terminal velocity/state transient, proving these held-tail operands can fire independently.

### P1 — Brightness checks omit discrete rendered stamp population/total energy

**Files:**

- `tools/reel/manafold_fx.h:3147-3155`
- `tools/reel/manafold_fx.h:3162-3175`
- `tools/reel/manafold_fx.h:3248-3252`
- `tools/reel/manafold_motiongate.cpp:208-209, 303-320, 441-478`

The new trace records per-edge presence and nominal navy/core gain; shimmer is the average gain across stamps. Production rendering independently quantizes each edge segment to an integer `nst` from path length. When `nst` crosses an integer boundary, an entire stamp is added or removed in one frame, changing actual deposited splat count/total layer energy even when every traced per-stamp gain is smooth. Neither `edge_energy_pm` nor `edge_layer_gain_pm` includes `nst`, total splat count, radius-weighted energy, or accumulated layer energy. Free-lightning `bolt_stamp()` has the same discrete count but no brightness/population trace.

**Failure scenario:** a smoothly morphing bolt segment crosses the next `kFoldEdgeStampMm` boundary at the Channel loop seam. Its traced presence and per-stamp gains pass, but one white/shimmer/navy stamp appears at once and the rendered branch flashes—the same class of visual defect Direction 18 just reopened.

Current every-frame review is evidence for this binary, but the committed gate claim that it measures shipping layer energy is structurally incomplete.

**Required correction:** trace the exact production stamp count and an accumulated rendered-energy proxy per stable edge/layer (and free strand), using the same subdivision result the renderer consumes. Gate adjacent/seam step, acceleration and jerk, and add a positive control that changes subdivision/population without changing per-stamp gain. Alternatively make subdivision continuous/fixed-identity so population cannot change, then prove that invariant directly.

### P2 — `mspan` mutant runs are red but not self-attributed

**Files:**

- `tools/reel/manafold_spangate.cpp:1219-1258`
- `tools/reel/manafold_spangate.cpp:1300-1344`
- `runs/CLAUDE-RUNS/RUN-20260917-2318-manafold-pass16-pass17/PASS17-PUBLIC-ORDERING-IMPLEMENTATION.md:117-120`

The CLI selects crown, mute, snap, accent, tremor, compression and span mutants, then runs the entire gate and returns only `g_failures != 0`. There is no mutant-specific assertion that the named detector fired, nor a check that a mute removed the named carrier evidence. Consequently “30/30 controls fire” can be certified by any unrelated red check in the omnibus run. `msmooth` has explicit fault-specific attribution logic; `mspan` does not.

**Failure scenario:** `--fail-hold-tremor` does not reach production quaternions after a refactor, but an unrelated closure or ordering check is red. The invocation still returns RC 1 and is reported as a fired tremor control.

**Required correction:** expose/check named detector results (ordering witnesses, specific mute carrier, carrier angular continuity, compression wrap, posed ring order, etc.) and require each selected mutant's intended predicate to fail. For carrier mutes, require the named carrier's evidence to disappear while unrelated carriers/contracts remain live. A red omnibus baseline or unrelated failure must not satisfy attribution.

### P2 — Lab edge smoothness is only step-gated, not derivative/layer-energy gated

**Files:**

- `tools/reel/manafold_motiongate.cpp:629-789`
- `tools/reel/manafold_motiongate.cpp:792-885`
- `tools/reel/manafold_lab.h:796-837`

Shipping fold edges now have per-edge presence and navy/shimmer/core gain histories with step/acceleration/jerk bounds. Lab variants expose only `edge_presence_pm`/aggregate `edge_energy_pm`; `trace_lab_variant()` retains the previous presence value and computes only maximum step. `check_lab_continuity()` checks that step and blackout, but not lab edge-presence acceleration/jerk or actual layer gains. This is weaker than the report's “full lab/shipping 60 Hz step/accel/jerk” claim and leaves historical SNAP-labelled rows least instrumented in the exact domain Direction 18 changed.

**Required correction:** give `LabContinuityTrace` the same stable per-edge layer-gain operands as production fold rendering, and run cyclic step/acceleration/jerk histories through frames 0–2. Make the brightness/topology controls exercise lab operands too, or add an attributed lab-specific discontinuity control.

## Report consistency cleanup

`PASS17-PUBLIC-ORDERING-IMPLEMENTATION.md:136-153` still names the older `B5438DEA…` gate, nine effect controls and pre-brightness metrics, while the final effect reports identify renderer `16A89823…`, gate `FF3B90A9…`, brightness-complete metrics and ten controls. The active integration lane is expected to reconcile this, but the evidence commit must contain one explicit current generation and label all earlier hashes/sheets historical.

## Verified sound by source review

- Crown rankings, independent A/B/C controls and Front/End punctuation are authored, held and C2-eased rather than oscillator-generated.
- Impact deformation saturates instead of wrapping `uint16_t` in shipping mode.
- Signed high-precision MVC weights normalize to exact Q12 unity; the wrapped-weight fault is explicit.
- `fx_anchors_from_pose()` is shared by production and gates.
- Free/fold/surge path clocks are clip-periodic; loop traces replay frames 0–2 for cyclic derivatives.
- Fold and surge mote identity/count/role domains are fixed and fault-injectable.
- Same-shape morph resets are exempted without exempting distinct-shape reversals.
- Blue/Green/Boil/Pulsar/Stack production bodies are traced with stable centres/radii/gains.
- Direct and CMake registration for `msmooth` are present.

## Resolution — checker-complete source (2026-09-19)

All four findings are repaired in the settled source and independently exercised:

1. **Held tails are real samples now.** `mspan` and `msmooth` append three repeated final presentation ticks to every `hold_last` clip, using the production pose decoder and persistent `FoldState` update. Normal checks cover nine held-tail samples. `--fail-final-dwell` leaves authored final key/midpoint equality untouched but breaks the first parked tick; both gates attribute it to their held-tail continuity operands.
2. **Production stamp population and accumulated energy are traced.** Rendering and gates share `path_segment_stamp_count()` / `path_stamp_count()`. Free strands and every stable fold/lab edge expose actual emitted stamp counts plus radius-weighted accumulated layer energy. Adjacent, loop-seam and held-tail step/acceleration/jerk histories are gated. Normal fold population is `24/24/48` step/accel/jerk with wide structural headroom (`32/48/72`), and accumulated fold energy is `21/21/39 pm`; lab is `4/4/8` for both domains. `--fail-stamp-count` changes population without changing authored per-stamp gains and fires shipping and lab population/energy detectors.
3. **Every carrier mutant is self-attributed.** `mspan` records failure-category bits and each selected mutant declares its named detector plus only explicit causal categories. All **31/31** controls return RC 1 with `attributed detector fired`; an unrelated omnibus failure or a silent control produces `UNATTRIBUTED` and fails the control contract.
4. **Lab derivatives match shipping coverage.** All ten variants now retain stable per-edge presence, halo/core population and accumulated energy histories through frames 0–2 after wrap. Step/acceleration/jerk and per-layer energy are asserted; count/role and population controls must fire the lab operands as well as shipping operands.

Fresh normal `mspan` and `msmooth` are RC 0. All 31 span controls and all 12 effect controls are attributed RC 1. Final checker binary MD5s are `6A9E653CA58537CA3832F9D112712183` (`mspan`) and `60651579CF0508F068F1DFC43C290703` (`msmooth`). No authored motion/effect values were changed by this checker repair. The paused integration lane must clean-build one renderer from this frozen source before citing final pictures.

That four-finding checker generation was independently green and remains provenance. A final audit then found visibility, attribution, and double-ease gaps. They are now also closed:

5. **Rendered mote visibility is independently gated.** Fold, surge and lab per-ID visibility histories cover adjacent, seam and repeated-final samples. Normal maxima are fold `8/8/16 pm`, surge/lab `0/0/0 pm`; `--fail-mote-visibility` changes visibility without changing position/ID/role and fires the visibility category.
6. **Effect controls reject unrelated failures.** All thirteen `msmooth` controls declare expected and causally allowed category masks; a missing named category or any unrelated category is an attribution failure. Fresh 13/13 logs show exact attributed RC 1 controls.
7. **Lab coherence is single-eased.** `lab_phase` owns the C2 envelope and `lab_fill` consumes `ph.amp_pm` directly.
8. **Held public proof shares one production metric.** `mspan` and `mjointpub` call `manafold_public_joint_metric.h`; anchored Front/End use max-vertex/orientation response while free A/B/C require centroid plus max. At f0324 all five clear 20 mm. Focused held normal is RC 0 and five mutes fail only `kCatCrown`.

Final checker MD5s are `mspan F532D1D5EAABCC5027E5B9A77E78138A` and `msmooth F558ECE81519B2F1DB2746A4E0D81480`. Native/4× same-binary review selected A80 from the structurally legal 60/80/100 rungs; full normal `mspan` is RC 0 with zero budget breach, focused held normal/five mutes pass, and 31/31 span plus 13/13 effect controls are attributed RC 1. The rejected A140 rung remains the positive art-bound witness. No comparison gate was moved.

As I always say, a smooth field trip includes the frames after the bus parks—and counts every lightbulb, not merely how bright each bulb was!
