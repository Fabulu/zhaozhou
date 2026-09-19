# Manafold Pass 17 lightning/particle continuity review

**Date:** 2026-09-19
**Scope:** independent source review plus verified repair of the Direction-18 effect packet.

## Verdict

**Historical verdict:** **superseded after repair.** This report records the first review and its `F9404CA5…` / `FF151E82…` mechanism closure. Integrated Channel seam review later reopened brightness and shipping-candidate coverage; `PASS17-LIGHTNING-PARTICLE-FINAL-REVIEW.md` is the current verdict tied to `16A89823…` / `FF3B90A9…`. The findings/resolutions below remain provenance, not final acceptance.

## Findings

### P1 — Full-bank histories omit the loop seam and frame-zero state reset

**Files:** `tools/reel/manafold_motiongate.cpp:104-209`, `tools/reel/manafold_motiongate.cpp:244-274`, `tools/reel/manafold_fx.h:2644-2675`

`trace_clip` pushes presentation frames `0 .. frame_count*2-1` and stops. It never pushes frame zero again after the final frame, so position, velocity, acceleration and jerk checks do not evaluate the loop seam. `check_phase_clocks` checks phase IDs only; it neither compares physical paths across the seam nor checks stateful `FoldState` output. It also increments `bad_endpoints` when frame zero begins mid-morph, prints the count, and never fails on it.

This matters because production explicitly resets `prev_rel`, drag buffers, `drag_smooth`, `knead_smooth`, `knead_visible`, and `drag_idx` whenever `frame == 0`. A regression where the final state does not settle to those reset values can snap exactly at the wrap while every current G5 history remains green.

**Failure scenario:** the final Taunt/idle particle or edge point ends away from the freshly reset frame-zero position; last→first visibly jumps, but the gate never forms that sample pair.

**Required correction:** evaluate at least one wrapped frame-zero sample after the final sample using production state semantics, and include it in point/mote kinematics. Assert `bad_endpoints == 0`. Add an attributed seam-state/reset mutant or committed positive control.

### P1 — Free-lightning surge motes are not traced or mutated

**Files:** `tools/reel/manafold_fx.h:2034-2055`, `tools/reel/manafold_motiongate.cpp:104-109`, `tools/reel/manafold_motiongate.cpp:187-208`, `tools/reel/manafold_motiongate.cpp:319-339`

`mana_lightning` renders `kSurgeMotes` along the primary free strand, but it records only lightning path points in `FxContinuityTrace`. The trace's particle arrays cover only the folded figure's `kMoteCount` population written by `mana_fold`. The full-bank particle histories and `--fail-particle-reseed` therefore do not observe the newly rewritten surge-mote clock at all.

**Failure scenario:** a future edit restores endpoint teleport/reseed for surge motes while folded motes remain smooth; G3/G5 and the reseed mutant still report green even though visible lightning-attached particles pop.

**Required correction:** give surge motes stable typed IDs, positions, visibility and roles in the trace (separate domain or tagged IDs), run their full-bank kinematics including wrap, and make the reseed positive control fire both relevant particle mechanisms or provide a separate attributed surge-reseed control.

### P2 — Experimental lab continuity changes have no production-path gate coverage

**Files:** `tools/reel/manafold_lab.h:532-552`, `tools/reel/manafold_lab.h:711-765`, `tools/reel/manafold_lab.h:845-953`, `tools/reel/manafold_motiongate.cpp:17-27`

The packet materially changes lab extra strands, canonical station interpolation, edge topology cross-fade, mote jitter and knead behavior. `manafold_motiongate.cpp` includes `manafold.h`, never evaluates `lab_fill`, and iterates only the production clip bank through `mana_fill(3)` plus free lightning. The renderer build proves the lab compiles, not that its paths remain continuous.

There is also a timing divergence worth making explicit: shipping `fold_phase` already returns C2-eased `morph_pm`, while `lab_fold` applies `motion_c2_ease` again. This remains continuous but is not the same transition law the report describes as shared.

**Required correction:** exercise every shipping/site lab variant through `lab_fill` at all presentation frames (including seam), or narrow the report's claim and defer lab continuity. Decide deliberately whether double easing is wanted; do not call it the same production law while it differs.

### P2 — Count/role checks and mutants are not positively controlled

**Files:** `tools/reel/manafold_fx.h:2602-2610`, `tools/reel/manafold_fx.h:3112-3160`, `tools/reel/manafold_motiongate.cpp:184-208`, `tools/reel/manafold_motiongate.cpp:424-444`

The gate reports zero folded-mote count/role changes, but no committed mutant makes either detector fire. The three controls cover path switching, edge blackout, and fold-mote positional reseed only. While the normal gate is red on unrelated authored fold trajectories, all mutant invocations also return nonzero regardless; return code alone is not attribution.

**Required correction:** after the authored trajectory lane makes normal `msmooth` green, rerun all controls and require their named failures specifically. Add a count/role-pop mutant (or a selftest that invokes and attributes it) before citing those zero counters as positive evidence.

## Reviewed properties that are sound

- `fx_anchors_from_pose` uses compiled bind positions plus production skinning and is now called by the renderer, avoiding copied anchor arithmetic.
- Free lightning path IDs, endpoint interpolation, brightness interpolation, pulse clocks and per-ID mote phases are deterministic and clip-periodic.
- Canonical HEART mapping is per shape rather than pair-local; closed HEART topology remains compatible with reversed station order.
- Fold-edge presence reaches exact zero only at authored death while retaining a nonzero alive floor.
- Fold motes use a fixed ID/role domain and continuous visibility; 64-bit intermediates bound the new interpolation and sinusoidal arithmetic safely at current constants.
- Direct and CMake `msmooth` registrations are complete.
- Switch/blackout/fold-mote-reseed mutations alter independent production mechanisms rather than merely changing expected outputs, but their final attribution must be re-demonstrated after normal RC becomes zero.

## Resolution — final generation

1. **Loop seams and reset state — fixed and positively controlled.** Every looping clip appends production frame zero after its final presentation sample, including state reset, for 1,560 traced entity seams. `bad_endpoints` is fatal. Clip-periodic integer-cycle rotation/knead clocks close physically; `--fail-loop-seam` restores the old fixed-period clock and fires the named seam detector.
2. **Surge motes — fixed and independently controlled.** Free-strand surge motes have fixed typed IDs, role, visibility and positions in `FxContinuityTrace`; full-bank and all ten lab variants trace their step/acceleration/jerk/seam. `--fail-surge-reseed` independently fires surge kinematics.
3. **Lab coverage/law — fixed.** `msmooth` executes every site lab variant through `lab_fill`, including up to four strands, 72 persistent motes, edge identities and loop reset. Double easing is removed. The 720-key lab timeline provides 190–252-frame C2 morphs, 24+ frame holds, a 120-key return, a looped C2 traverse and smooth world-hold leash/ring/drag handoff. Final lab station/mote/strand/surge seam and kinematic metrics are recorded in the implementation report and all gates are green.
4. **Count/role instrumentation — fixed and positively controlled.** Fixed production and lab ID domains are checked independently for fold and surge populations. `--fail-mote-count` and `--fail-mote-role` alter the rendered domain/path and each fires only its named detector.
5. **Signed MVC defect discovered during repair — fixed.** A tiny negative interior coordinate had wrapped through `uint16_t` to `65532`, placing HEART stations tens of metres away. Q12 distances, signed `int32_t` weights, exact affine normalization and range/sum invariants remove it; `--fail-weight-wrap` reproduces and catches the original tuple.
6. **Same-shape reversal false positive — fixed without weakening real morphs.** A 1000→0 progress reset is ignored only when source and destination shape IDs are identical. `--fail-morph-reverse` reverses a distinct-shape morph and fires chain kinematics/reversal detection.

Final normal `msmooth` is RC 0. All nine controls return RC 1 and print `attributed detector fired`. Native every-frame review covers Channel, Pirouette, Taunt III, Crackle, Trick, Death Gutter and held-still lab; exact badness-selected native/4× sequences cover the Lasso/Trick and lab maxima. No off/reappear, alternate-frame particle reseed or visible acceleration/jerk snap reproduces. This nine-control generation is historical; the later final audit below supersedes its checker verdict.

## Final audit closure

The final audit found one visibility operand still missing, weaker-than-claimed mutant attribution, and a remaining lab double ease. Checker generation `F558ECE81519B2F1DB2746A4E0D81480` repairs all three:

- per-ID fold/surge and lab mote visibility is traced through adjacent frames, seams and held-tail samples; normal maxima are fold `8/8/16 pm`, surge `0/0/0 pm`, lab `0/0/0 pm`;
- `--fail-mote-visibility` produces an off/reappear fault without changing ID, role or position and fires the visibility category;
- all thirteen controls enforce expected and allowed category masks, rejecting unrelated detector failures instead of accepting omnibus red;
- lab coherence consumes the already C2-eased phase amplitude directly, so the authority envelope is applied once.

Fresh normal `msmooth` is RC 0 and 13/13 controls return RC 1 with exact attribution. The expanded span gate shares the exact `mjointpub` metric; selected A80 f0324 F/A/B/C/E centroid/max values are `6.12/20.62`, `81.10/83.05`, `184.13/203.70`, `167.86/210.53`, `1.80/24.04 mm`, all above the 20 mm floor. Focused held normal/five mutes and refactored `mjointpub` controls pass. Full normal `mspan` is RC 0 with zero signed-budget breaches and 31/31 attributed controls; the rejected A140 rung's 53 receipts prove the unchanged art bound can fire.

## Acceptance boundary

Direction 18's effect checker and selected A80 ordering/held/signed packet are source-green. The exact final integration binary must rerun expanded `mspan` with 31 controls and `msmooth` with 13 controls, then isolated every-frame review.
