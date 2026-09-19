# Manafold Pass 17 smooth-checker final audit

**Date:** 2026-09-19
**Scope:** frozen `manafold_fx.h`, `manafold_lab.h`, `manafold_motiongate.cpp`, `manafold_spangate.cpp` and final Direction-18 reports
**Historical checker verdict:** **PASS after repair.** The three findings below are retained as the audit record and closed in `Resolution`; the later clean post-palette integration is green in `PASS17-FINAL-TARGETED-INTEGRATION.md`.

## Findings

### P1 — Fold/surge mote visibility can pop without any detector firing

**Files:** `tools/reel/manafold_fx.h:2749-2756,3354-3360,3533-3539`; `tools/reel/manafold_motiongate.cpp:228-240,294-310,391-418`

Production now keeps mote IDs, roles and population domains fixed, then expresses crowd/life/garnish changes through `mote_visibility_pm` / `surge_mote_visibility_pm`. That is the right mechanism, but `msmooth` never puts either visibility field through a `ScalarHistory`.

The gate only:

- checks count/role identity;
- skips positional samples while visibility is zero; and
- measures positions for visible motes.

It does not gate visibility step, acceleration, jerk, loop seam or held-tail settling, and none of the twelve mutants introduces a visibility-only pop.

**Failure scenario:** a future edit changes `persistent_mote_visibility_pm` or a death/garnish handoff from a continuous envelope to `visibility = condition ? 1000 : 0`. Counts and roles stay fixed and all positions remain smooth, so normal `msmooth` remains green while the attached particle cloud visibly turns off/reappears — the owner's exact complaint.

**Required correction:** add independent fold- and surge-mote visibility histories across every adjacent frame, frames 0–2 after wrap and repeated held-final ticks. Gate visual-backed step/acceleration/jerk bands and add an attributed visibility-pop mutant that changes visibility without changing count, role or position.

### P2 — Effect-mutant “attribution” does not reject unrelated failures

**Files:** `tools/reel/manafold_motiongate.cpp:83-86,1178-1221,1381-1527`; comparison: `tools/reel/manafold_spangate.cpp:1306-1345,1434-1446`

`msmooth` does prove that each selected mutant reaches its named operand: the final `fault_caught` switch checks the relevant metric and prints `attributed detector fired`. But all ordinary failures accumulate in one undifferentiated `g_failures` counter. The gate never records category bits and never rejects a mutant that also trips unrelated detectors.

That is weaker than `mspan`, whose `expected_category` / `allowed_categories` contract requires the named failure and rejects unrelated categories. It also makes the reports' claim that all twelve controls are “explicitly attributed” stronger than the source demonstrates.

**Failure scenario:** a future brightness-seam mutant still trips the named gain detector but accidentally also changes mote roles or lab topology. The invocation returns RC 1 and prints `attributed detector fired`, so the matrix cites it as a clean positive control despite testing several mechanisms at once.

**Required correction:** add effect failure categories, expected/allowed masks per mutant and the same named-plus-no-unrelated enforcement used by `mspan`. Causal secondary failures may be explicitly allowed; unrelated failures must make attribution fail.

### P2 — The lab still applies a second C2 ease despite reports claiming one

**Files:** `tools/reel/manafold_lab.h:378-415,692-698`; `runs/CLAUDE-RUNS/RUN-20260917-2318-manafold-pass16-pass17/PASS17-LIGHTNING-PARTICLE-CONTINUITY.md:18`; `PASS17-LIGHTNING-PARTICLE-FINAL-REVIEW.md:74`

`lab_phase()` already authors gather/release `amp_pm` through `motion_c2_ease`. `lab_fill()` then evaluates `motion_c2_ease(ph.amp_pm)` again when handing coherence to the visible fold. The earlier review explicitly identified double-easing, while current reports state that the lab uses “one C2 envelope” / “single lab C2 easing.” Source and evidence claims therefore still disagree.

Composition of two C2 functions remains continuous, so this is not itself a snap, but it compresses the effective authority transition and can move acceleration/jerk peaks away from the authored timeline.

**Required correction:** either consume `ph.amp_pm` directly for the coherence handoff, or document the second response as intentional, name it independently and review/gate that authored response. Do not call the current code single-eased.

## Verified closures

Source inspection confirms these previously found gaps are closed:

- loops replay frames 0, 1 and 2 after the last frame; held clips replay three exact final ticks;
- shipping and lab gates trace entity step/acceleration/jerk plus loop seams;
- emitted stamp counts and radius-weighted accumulated energy are derived from the same subdivision law production uses;
- lab edge presence, stamp count and accumulated energy gate step/acceleration/jerk;
- signed Q12 MVC weights use `int32_t`, clamp truly outside authored points inward, preserve exact affine unity and have a wrapped-weight control;
- fixed-period clocks are clip-periodic with an explicit seam fault;
- same-shape morph reversal is exempted only when `shape_from == shape_to`;
- fold/surge point identities, counts and roles are persistent, and positional reseed mutants exist;
- Blue/Green/Boil/Pulsar/Stack bodies are traced through production candidates with cyclic centre/radius/gain histories;
- production and gates share `fx_anchors_from_pose`;
- `mspan` has strict expected/allowed category attribution for all 31 controls.

The report generation labels are internally honest about the boundary: checker hashes `mspan 6A9E653C…` / `msmooth 60651579…` postdate renderer `16A89823…`, and both reports require a new exact final integration render rather than presenting the older pictures as checker-complete evidence.

## Resolution

- Fold, surge and lab mote visibility now has an independent `ScalarHistory` across every adjacent frame, frames 0–2 after loop wrap and repeated held-final ticks. Stable IDs/roles/positions can no longer hide an off/reappear fault.
- `--fail-mote-visibility` changes rendered visibility while leaving ID, role and position intact. It drives fold, surge and lab visibility beyond the reviewed bands and fires only `kFailMoteVisibility`.
- `msmooth` now records failure-category bits. Each of all thirteen mutants declares expected and causally allowed masks; missing named categories or unrelated categories are attribution failures. Fresh logs show 13/13 exact attributed RC 1 controls.
- Lab coherence consumes `ph.amp_pm` directly. `lab_phase` remains the sole C2 easing authority.

Fresh normal `msmooth` is RC 0, MD5 `F558ECE81519B2F1DB2746A4E0D81480`; normal visibility maxima are fold `8/8/16 pm`, surge `0/0/0 pm`, lab `0/0/0 pm`; `git diff --check` is clean. The effect checker packet is frozen. The expanded span gate (MD5 `F532D1D5EAABCC5027E5B9A77E78138A`) shares `mjointpub`'s exact public metric; selected A80 f0324 F/A/B/C/E centroid/max values are `6.12/20.62`, `81.10/83.05`, `184.13/203.70`, `167.86/210.53`, `1.80/24.04 mm`, all above the public floor. Focused held normal is RC 0 and five mutes are exactly `kCatCrown`; refactored `mjointpub` normal/five controls remain green. Strict pre-type punch-A parsing rejects malformed/trailing/overflow/out-of-range values with RC 2. Full normal `mspan` is RC 0 with zero signed-budget breach, 31/31 attributed controls, and 18 mm F–A headroom. The A140 rung's 53 receipts remain the positive art-bound witness.

## Acceptance requirement

The checker and selected A80 art packet are source-green. One clean integrated renderer must now reproduce the selected Trick/Taunt III/eye pictures and run both complete normal/mutant matrices before final-bank isolated every-frame review.
