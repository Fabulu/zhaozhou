# Manafold Pass 17 lightning/particle final review

**Date:** 2026-09-19
**Scope:** final palette-complete Direction-18 effect source and clean combined integration (`msmooth E04B15D97C9456B0AAE0A2C4C202C57E`)
**Verdict:** **PASS.** The effect mechanism, fifteen attributed controls, selected A80 ordering and held five-carrier proof are green in the clean post-palette build; the exact targeted pictures reproduce the selected art.

## Findings

### P1 — `msmooth` does not exercise every shipping mana mechanism

**Files:** `tools/reel/manafold_motiongate.cpp:147-184`, `tools/reel/manafold_fx.h:1989-2002`, `tools/reel/manafold_fx.h:3420-3500`

`trace_clip()` forces candidate 3 (`mana_fold`) and then separately invokes `mana_lightning` for every clip. That is useful mechanism coverage, but it is not the complete shipping/site candidate set claimed by the reports. The live Manafold bank also contains:

- candidate 1 pulsar (`centre_wobble` plus fixed-period breath);
- candidates 2/7 filled blue/green bodies (`centre_wobble` on three persistent bodies);
- candidate 5 boil (`centre_wobble`);
- candidate 8 stack (pulsar + free lightning + fold);
- candidate 9 channel stack with the authored free-strand life envelope.

`centre_wobble()` uses raw frame clocks with fixed 97/61/113-frame periods and has no clip-length parameter. Those clocks are not loop-periodic for the 600-frame fixed idle and other shipping subjects—the same class of seam defect just repaired for stencil rotation. The pulsar radius clock is also untraced. None of these rendered entity centres/radii/visibility states appears in `FxContinuityTrace`, so the full-bank seam/velocity/acceleration/jerk checks and nine mutants cannot see a pop in the Blue, Green, Boil, Stack, or pulsar layers.

**Failure scenario:** `manafold-mana-blue`, Green, Boil, or Stack reaches its final frame at an arbitrary wobble/breath phase and resets to frame zero; the attached bodies visibly jump while `msmooth` remains RC 0 because it traced a synthetic fold+lightning stack instead.

**Required correction:** trace the exact candidate used by every live subject (or explicitly enumerate every candidate mechanism through its production `mana_fill` call) with stable IDs for each visible body, centre, radius/gain and visibility. Make all live wobble/breath clocks clip-periodic without a high-frequency common-divisor buzz. Include last→first and full cyclic kinematics, plus an attributed fixed-period/reseed control for these candidates. Do not claim “full shipping” until this is green.

### P1 — cyclic acceleration/jerk is incomplete after the loop seam

**Files:** `tools/reel/manafold_motiongate.cpp:147-291`, `tools/reel/manafold_motiongate.cpp:531-636`

Both shipping and lab traces append only one wrapped sample: frame zero after the final presentation frame. That computes the seam velocity, acceleration and jerk *at frame zero*, but the first pass through the clip had no prior cyclic history, so it never computes:

- acceleration at frame 1 using the seam velocity;
- jerk at frame 1;
- jerk at frame 2.

This matters because `mana_fold` resets state at frame zero, then its filters begin evolving again. A reset transient can appear one or two frames after the seam while every reported cyclic accel/jerk maximum stays green.

**Failure scenario:** frame zero itself lands at the right position, but reset `FoldState` produces a velocity change at frame 1; the visible effect hitches immediately after wrap, outside the gate's derivative window.

**Required correction:** after the final frame, replay at least frames 0, 1, and 2 through production reset/state semantics (enough samples to form all cyclic derivatives), while counting seam position only on the last→zero pair. Apply the same rule to every shipping and lab entity domain. Add a state-reset mutant whose position seam can remain bounded but whose post-seam acceleration/jerk fires.

### P2 — user-facing lab labels still promise hard snapping

**Files:** `tools/reel/manafold_lab.h:143-145`, `tools/reel/manafold_lab.h:268-280`, `tools/reel/manafold_lab.h:316-329`, `tools/reel/manafold_lab.h:415-418`, `tools/reel/zhao_reel.cpp:8857`

Direction 18 correctly changed every lab row—including the historical SNAP row—to one persistent C2 identity; `V.snap` now changes scatter amplitude rather than replacing topology. But the variant table and comments still say “SNAP between held shapes” and “snap transitions.” `zhao_reel.cpp` publishes `V.mechanism` as the subject note, so this is user-visible false documentation and invites a future maintainer to restore the rejected discontinuity.

Keep stable subject slugs if archive compatibility needs them, but rename the field/comment semantics to smooth scatter and update the displayed mechanism copy to describe the current C2 behavior.

## Resolution — brightness-complete generation

1. **Every live candidate is now traced through production.** Pulsar, Blue, Green, Boil and Stack expose stable body IDs, roles, centres, radii, gains and visibility through `FxContinuityTrace`; `msmooth` runs candidate 1/2/5/7/8 through the fixed-camera idle. Their wobble and breath clocks use clip-periodic integer cycles. Normal maxima are position `37.95/17.33/17.57 mm` step/accel/jerk with `34.22 mm` seam, and Pulsar radius `7/3/4 px`; zero count/role changes. The fixed-period seam fault drives these bodies hundreds of millimetres and is caught.
2. **Cyclic derivatives are complete.** Shipping and all ten lab histories replay frames 0, 1 and 2 after the final frame using production state reset, so seam velocity, acceleration and jerk at frames 0–2 all have cyclic history. The lab state resets every field, coherence returns to the named seam value, drag fades on the C2 release, and all lab metrics are green.
3. **Brightness and topology energy are first-class operands.** Each stable fold edge traces presence plus navy/shimmer/core gains at every adjacent frame and across the loop. The stateful area/coherence authority is visibly low-pass filtered and C2-handed to `kCohBasePm`; Lasso's coherence floor shares `shape_mix_pm`. Normal presence/layer maxima are `166/82/85 pm` step/accel/jerk. `--fail-brightness-seam` restores the raw-EMA handoff and fires only the presence/layer step/accel/jerk detectors (`780/781/1561 pm`).
4. **Lab copy matches behavior.** Historical subject slugs remain stable, but public mechanism descriptions now say persistent C2 smooth scatter rather than promising the hard snap Direction 18 retired.
5. **Visual stop passed.** Final `16A89823…` every-frame sheets cover Channel plus Blue/Green/Boil/Stack. All 2,820 final raw frames are byte-identical to the tile-by-tile reviewed generation before the lab-copy-only rebuild. Channel f0419→f0000 retains the same visible branch geometry and carries white/cyan/navy brightness continuously at native and exact 4×; the mana bodies retain smooth motion/breathing with no last→first reset.
6. **Controls are complete.** Normal `msmooth` is RC 0. All ten controls return RC 1 with attributed detectors: the prior nine plus `--fail-brightness-seam`. The final gate is `FF3B90A900C8603C8477C1A4420474E3`.

## Post-review checker closure

The later combined-source review found four instrument gaps beyond the brightness-complete generation: held-final dwell, emitted stamp population/accumulated energy, carrier mutant attribution, and lab edge derivatives/layer energy. They are now closed in source:

- three repeated final ticks are evaluated for every held clip through production state updates;
- renderer and gates share the exact stamp subdivision law, with stable count and radius-weighted energy histories for free, fold and lab layers;
- lab and shipping histories gate step/acceleration/jerk across adjacent, seam and held-tail samples;
- all 31 pre-held-punch span controls and the then-current 12 effect controls were explicitly attributed, including `--fail-final-dwell` and `--fail-stamp-count`.

That intermediate checker generation was RC 0 with MD5s `mspan` `6A9E653CA58537CA3832F9D112712183` and `msmooth` `60651579CF0508F068F1DFC43C290703`. It is superseded by the final checker-audit amendment below; its pictures remain provenance rather than final integration evidence.

## Verified sound properties

- Signed Q12 MVC weights use `int32_t`, normalize to exact affine unity, and the wrapped-weight control reproduces the original defect.
- Persistent free/fold/surge point and mote identities, canonical HEART mapping, continuous alive-edge presence, periodic fold clocks, single lab C2 easing, and stable count/role domains are structurally coherent.
- The final thirteen effect mutants alter independent mechanisms and enforce expected/allowed failure-category masks; the brightness mutant fires edge-presence/per-stamp-gain operands, the population and held-tail controls exercise accumulated energy and parked state, and the visibility mutant changes rendered mote visibility without moving or re-identifying motes.
- The implementation history references renderer `16A898233C599BA4BB1ACC443B9C1872` and earlier gate `FF3B90A900C8603C8477C1A4420474E3`; both are explicitly historical. Final checker MD5 is `F558ECE81519B2F1DB2746A4E0D81480`.
- Final Channel and mana-menu evidence closes the previously missing shipping-candidate and post-seam derivative coverage.

## Final checker-audit amendment

The last source audit found that persistent position/identity did not prove visible particle continuity, and that effect-mutant attribution still admitted unrelated detector failures. Checker generation `F558ECE81519B2F1DB2746A4E0D81480` closes both:

- fixed fold and surge mote IDs now retain independent visibility histories at every adjacent frame, loop seam and repeated final tick; shipping fold visibility is `8/8/16 pm` step/acceleration/jerk, surge visibility is `0/0/0 pm`, and all lab visibility histories are `0/0/0 pm`;
- `--fail-mote-visibility` leaves IDs, roles and positions intact while making rendered fold/surge and lab visibility discontinuous; it fires only the new visibility category;
- all thirteen effect controls enforce expected and allowed category masks. The named category must fire; unrelated categories make the invocation an attribution failure. Secondary categories are admitted only when causally downstream of the mutated production operand;
- lab coherence consumes the already-C2-authored `ph.amp_pm` directly, applying one envelope exactly once.

Fresh normal `msmooth` is RC 0; 13/13 mutants return RC 1 with exact attributed category masks; `git diff --check` is clean. The expanded span gate shares `mjointpub`'s exact visible-core metric; selected A80 F/A/B/C/E centroid/max deltas are `6.12/20.62`, `81.10/83.05`, `184.13/203.70`, `167.86/210.53`, and `1.80/24.04 mm`, so all five clear the unchanged 20 mm floor. Focused held normal and five mutes are exactly green/attributed; full normal `mspan` is RC 0 with zero bound/margin breaches and all four spans retain both signs. The rejected A140 rung's 53 receipts remain historical proof that the unchanged gate reaches this art decision.

## Palette-complete amendment

Final combined-source review then found the Boil Blue/Violet CLUT still reset its
raw `(frame / 3) % 63` clock from rotation 10 to 0 at the 600-frame menu seam.
That visible operand was outside the thirteen controls. Production now passes the
clip period into an interpolating, clip-periodic C2 ramp. Native 1/2/3-cycle
full-loop review selected one complete visible revolution; every integer phase is
byte-exact to the authored ramp, so the repair does not blur or dim its range.

The checker traces each production CLUT entry and bloom-histogram-weighted emitted
energy across adjacent frames and the seam. Focused `msmooth` MD5
`E1DD063275637C1F4FC2D5525D7482AB` is RC 0; new strictly attributed
`--fail-palette-raw-clock` and `--fail-palette-hard-switch` controls are RC 1,
bringing the palette-complete effect total to **15/15**. Final selected Boil/Blue
frames are byte-identical to the reviewed one-cycle generation. See
`PASS17-BOIL-PALETTE-CONTINUITY.md` for exact metrics and pictures.

## Acceptance boundary

Direction 18's palette-complete effect checker and Direction 16's selected A80 ordering/held/signed packet pass their complete reviewed mechanisms. The clean post-palette integration rebuilt renderer `8DD0AE74058E620C222289CB17A565B9`, `mspan C0991BA37FB3AEABEBE3A1253B261FC0`, and `msmooth E04B15D97C9456B0AAE0A2C4C202C57E`: both normal gates are RC 0, all 31 span and 15 effect controls are exactly attributed RC 1, and the 33-subject 24,308-frame targeted bank passed isolated every-frame review. The remaining boundary is the later exact 28-subject shipping generation.
