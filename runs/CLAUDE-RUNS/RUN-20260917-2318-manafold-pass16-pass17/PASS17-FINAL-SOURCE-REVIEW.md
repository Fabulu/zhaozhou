# Manafold Pass 17 final combined-source review

**Date:** 2026-09-19
**Scope:** every current uncommitted production/gate/build change atop pushed `e150a384` / `31949dea`
**Verdict:** **REOPENED/CORRECTED — P1/P2 remain closed; final-bank review found and the focused lane repaired P3. One new clean combined bank is required.**

## Findings

### P1 — Boil palette churn is not clip-periodic and is absent from `msmooth`

**Files:** `tools/reel/manafold_fx.h:3755-3763`; `tools/reel/zhao_reel.cpp:3258-3262`; `tools/reel/manafold_motiongate.cpp:1173-1228`

`mana_build_ramps()` still derives the Blue/Violet CLUT rotation solely from raw presentation `frame`:

```cpp
const int rot = (frame / kBoilRotDiv) % 63;
```

It receives neither clip length nor a persistent loop phase. The live Manafold menu uses the 300-key fixed idle (600 presentation frames), so its last visible frame has `rot = (599 / 3) % 63 = 10`, while the next loop starts at `rot = 0`. The Boil body remains spatially continuous but its palette jumps ten CLUT positions at the seam.

The new shipping-mana instrument cannot see this. `trace_mana_bodies()` gates only body position, radius, gain, count and role; it never builds/traces the production ramp or its emitted colour/energy. A loop-seam hue/brightness snap can therefore ship with normal `msmooth` RC 0. This is the same failure class as the Channel geometry-continuous/brightness-discontinuous seam that reopened the earlier effect packet.

**Required correction:** make the boil palette clock clip-periodic (pass the presentation span/clip keys or equivalent persistent phase into `mana_build_ramps`; do not stop the churn), trace the production ramp state or emitted colour/energy at adjacent frames and last→first, and add an attributed fixed-period/palette-seam mutant. Review `manafold-mana-boil` f0599→f0000 at native and exact 4× before final acceptance.

**Resolution (2026-09-19):** `mana_build_ramps` now receives the production
presentation period and uses an integer-cycle phase with quintic C2 interpolation
between exact authored CLUT entries. Native full-loop 1/2/3-cycle review selected
one visible broad revolution; faster rungs read as repeated palette flicker. The
new production-path gate traces every Blue/Violet entry and actual bloom-histogram-
weighted emitted energy across adjacent frames and the loop seam. Normal is RC 0;
raw-clock and hard-switch controls are strictly attributed RC 1. Full details and
focused picture receipts are in `PASS17-BOIL-PALETTE-CONTINUITY.md`.

### P2 — source comments and the main ordering report mixed superseded timelines/generations with current claims

**Files:** `tools/reel/manafold_clips.h:4290-4326,2522-2525`; `runs/.../PASS17-PUBLIC-ORDERING-IMPLEMENTATION.md:51-83,85-120,122-153`

The implementation is internally coherent, but several comments still describe the previous schedule: the crown is said to occupy keys 100–146 and its shrug to be zero at key 100, while shipping starts the crown at key 56, releases at 132–144, and intentionally overlaps the first attack with the C2 shrug release. The Startle comment names old 14/22 whip knots while the table now uses 16/28/44/60/72.

The ordering report starts with a current PASS verdict but its central “native review” and metric sections cite superseded renderer `6720179D…`, 45,255 samples / 30 span controls, `B5438DEA…` and nine effect controls; later sections cite the final `F532D1D5…` / `F558ECE8…`, 31 span controls and 13 effect controls. The history is useful, but current and superseded generations need explicit subsection labels so no later findings/card cites an obsolete number as the final receipt.

**Required correction:** update the source comments to the actual 56–144 crown and current Startle knots; label old report sections historical/provisional and make the pending final integrated generation the sole current verdict/receipt.

**Resolution (2026-09-19):** Startle comments now name the current
16/28/44/60/72 C2 knots. Taunt III comments name the 56–132 crown tableaux and
132–144 release, including the intentional first-attack/shrug overlap. The
ordering report labels `6720179D`, 45,255/30 and `B5438DEA`/nine-control sections
as superseded history; its A80 held-punch section is current, and the clean
post-palette integrated report is explicitly the pre-P3 targeted receipt.

### P3 — Death Drop's rendered opaque backing ignored its traced fade

**Files:** `tools/reel/manafold_clips.h`; `tools/reel/manafold_fx.h`;
`tools/reel/manafold_motiongate.cpp`; `tools/reel/zhao_reel.cpp`

Exact final-bank review found a large navy/black folded-mana mass disappearing
between Death Drop f0233 and f0234. The old instrument traced life, gain,
visibility, stamp population and accumulated energy, all of which fell smoothly.
The renderer's soft opaque alpha was a separate untraced operand: lowering a dark
palette's gain made it black, not transparent, while alpha stayed full until the
splat stopped existing.

**Resolution (2026-09-19):** soft opaque splats now carry identity-defaulted
`opacity_pm`; both death life laws use the full Q4 clock and a C2 envelope; fold
backing/core/mote opacity follows that death envelope independently of colour
gain. `msmooth` traces the exact backing-opacity operand through both settle tails.
The strictly attributed `--fail-death-effect-cutoff` control restores the exact
old 450-frame output and cutoff. Normal/new control are `0/1`, all sixteen effect
controls remain attributed, and complete repaired Death Drop/Death Gutter sheets
pass. See `PASS17-DEATH-DROP-EFFECT-REPAIR.md`.

## Combined-source checks that survived review

No additional source defect was found in these areas:

- 23-bone signed spans, staged fractions, A80 budget, shared held/public carrier metric and anchored F/End semantics;
- crown target separation, C2 interpolation, public mute path, body/eye parenting and strict renderer/checker selectors;
- pure-X Trick composition and front-held Taunt III source controls;
- saturating deform representation and deliberate wrap mutant;
- signed high-precision MVC weights, exact affine normalization and wrapped-weight control;
- clip-periodic free/fold path clocks, actual final-shape release and narrow same-shape reversal exemption;
- persistent fold/surge mote IDs, roles, counts, visibility and reseed controls;
- repeated-final dwell, loop frames 0–2, production stamp populations, radius-weighted energy, lab derivatives/layers and expected/allowed mutant-category attribution;
- shared production/gate `fx_anchors_from_pose` and direct/CMake `msmooth` registration.

The pre-P3 post-palette integration built renderer MD5 `8DD0AE74058E620C222289CB17A565B9`, `mspan` `C0991BA37FB3AEABEBE3A1253B261FC0`, and `msmooth` `E04B15D97C9456B0AAE0A2C4C202C57E`; its 137/137 matrix and 24,308-frame pictures remain valid history for the unaffected packet, not the final renderer receipt. The focused P3 repair clean-build is renderer `19816A52B969C2BE65A797E3E17C1795` and `msmooth AB3520FEAC7BD90C2227E95EE4510427`: normal RC 0, all sixteen attributed controls RC 1, repaired Death Drop/Death Gutter pictures green, and Hover/Channel 1,020/1,020 byte-identical. A new combined build and exact 28-subject bank must supersede both generations.

## Exact source commit manifest after the P1/P2 repair

Stage these **12 paths only** for the production/gate/tools commit:

1. `tools/CMakeLists.txt`
2. `tools/reel/build-direct.sh`
3. `tools/reel/manafold_art.h`
4. `tools/reel/manafold_clips.h`
5. `tools/reel/manafold_fx.h`
6. `tools/reel/manafold_lab.h`
7. `tools/reel/manafold_motiongate.cpp`
8. `tools/reel/manafold_orderplot.py`
9. `tools/reel/manafold_public_joint_metric.h`
10. `tools/reel/manafold_public_jointgate.cpp`
11. `tools/reel/manafold_spangate.cpp`
12. `tools/reel/zhao_reel.cpp`

Do not include `TASK_LOG.md` in the source commit. Verify cached path count exactly 12, cached diff/check clean, clean direct cel + CMake/direct gate builds, normal matrices and all 31+16 controls before commit.

Suggested subject: `Complete Manafold smooth public performance`

Required trailer:

```text
Co-Authored-By: Claude Code <noreply@anthropic.com>
```

## Exact report manifest for the subsequent evidence commit

After the final integrated rebuild, stage these text receipts (including this review):

- `runs/CLAUDE-RUNS/RUN-20260917-2318-manafold-pass16-pass17/TASK_LOG.md`
- `.../PASS17-SMOOTH-MOTION-ARCHITECTURE.md`
- `.../PASS17-PUBLIC-ORDERING-IMPLEMENTATION.md`
- `.../PASS17-LIGHTNING-PARTICLE-CONTINUITY.md`
- `.../PASS17-LIGHTNING-PARTICLE-REVIEW.md`
- `.../PASS17-LIGHTNING-PARTICLE-FINAL-REVIEW.md`
- `.../PASS17-SMOOTH-ORDERING-CODE-REVIEW.md`
- `.../PASS17-SMOOTH-CHECKER-FINAL-AUDIT.md`
- `.../PASS17-SMOOTH-ORDERING-INTEGRATED-REVIEW.md` (explicitly stale/provisional history)
- `.../PASS17-TRICK-AXIS-SELECTION.md`
- `.../PASS17-TAUNT3-PUNCHLINE-IMPLEMENTATION.md`
- `.../PASS17-TRICK-TAUNT3-CODE-REVIEW.md`
- `.../PASS17-HELD-PUNCH-ART-DIAGNOSIS.md`
- `.../PASS17-HELD-PUNCH-A-SELECTION.md`
- `.../PASS17-FINAL-SOURCE-REVIEW.md`
- `.../PASS17-BOIL-PALETTE-CONTINUITY.md`
- `.../PASS17-DEATH-DROP-EFFECT-REPAIR.md`
- `.../PASS17-FINAL-TARGETED-INTEGRATION.md` (pre-P3 targeted acceptance; historical after the death repair)

Selection-ladder PNGs may accompany those reports only after `PASS17-FINAL-TARGETED-INTEGRATION.md` confirms the selected current source. The exact decision set is:

- `PASS17-HELD-A-LADDER-A60-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-A80-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-A100-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-A140-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-WITNESSES-NATIVE.png`
- `PASS17-HELD-A-LADDER-4X.png`
- `PASS17-TRICK-LEGACY-Z-ALLFRAMES.png`
- `PASS17-TRICK-PURE-X-ALLFRAMES.png`
- `PASS17-TRICK-MIXED-X32-Z6-ALLFRAMES.png`
- `PASS17-TRICK-AXIS-WITNESSES-NATIVE-3COL.png`
- `PASS17-TRICK-AXIS-FACE-4X.png`
- `PASS17-TRICK-PURE-VS-CONDITIONAL-4X.png`
- `PASS17-TRICK-SELECTED-ALLFRAMES.png`
- `PASS17-TRICK-SELECTED-FACE-4X.png`
- `PASS17-TAUNT3-LEGACY-AWAY-ALLFRAMES.png`
- `PASS17-TAUNT3-FRONT-SQUASH-ALLFRAMES.png`
- `PASS17-TAUNT3-FRONT-TIP-ALLFRAMES.png`
- `PASS17-TAUNT3-SELECTED-ALLFRAMES.png`
- `PASS17-TAUNT3-SELECTED-FACE-4X.png`
- `PASS17-TAUNT3-SELECTED-VS-LEGACY-NATIVE.png`
- `PASS17-TAUNT3-EYE-MUTES-NATIVE.png`
- `PASS17-TAUNT3-EYE-MUTES-4X.png`
- `PASS17-TAUNT3-JOINT-MUTES-NATIVE.png`
- `PASS17-BOIL-PALETTE-C1-ALLFRAMES.png`
- `PASS17-BOIL-PALETTE-C2-ALLFRAMES.png`
- `PASS17-BOIL-PALETTE-C3-ALLFRAMES.png`
- `PASS17-BOIL-PALETTE-RAW-CONTROL-ALLFRAMES.png`
- `PASS17-BOIL-PALETTE-HARD-CONTROL-ALLFRAMES.png`
- `PASS17-BOIL-PALETTE-CYCLE-LADDER-NATIVE.png`
- `PASS17-BOIL-PALETTE-C1-SEAM-4X.png`
- `PASS17-BOIL-PALETTE-CONTROLS-4X.png`
- `PASS17-BOIL-PALETTE-SELECTED-BLUE-ALLFRAMES.png`
- `PASS17-DEATH-DROP-EFFECT-REPAIR-ACCEPTED-ALLFRAMES.png`
- `PASS17-DEATH-GUTTER-EFFECT-REPAIR-ACCEPTED-ALLFRAMES.png`
- `PASS17-DEATH-DROP-EFFECT-FADE-ACCEPTED-2X.png`
- `PASS17-DEATH-DROP-EFFECT-HARD-CUT-CONTROL-2X.png`
- `PASS17-DEATH-DROP-EFFECT-ACCEPTED-VS-CONTROL-NATIVE.png`

Add only the final integration report's explicitly enumerated current-generation PNGs. Do **not** bulk-add other run-folder images.

## Explicit exclusions

Exclude from both commits:

- `.tmp/`, `out/`, raw `.rgb`, encoder/build logs and generated binaries;
- every `PROVISIONAL`, `INTEGRATED87`, `BEFORE`, `CURRENT-RED` or superseded 22-bone/pre-brightness image;
- provenance-deficient `PASS17-FALL-HOLD-VS-WRAP.png` and `PASS17-ENDPOINT-AUTHORITY-{LADDER,4X}.png` until regenerated from the final bank;
- ambiguous duplicate `PASS17-SMOOTH-FINAL-*`, `PASS17-SMOOTH-ACCEPTED-*` and `PASS17-INTEGRATED-FINAL-*` sheets unless the final integration report names the exact files and generation;
- old wrong nodule witness sheets and any image not cited by a committed report;
- active full-bank/encode/site outputs, which belong to the later finished-pass delivery commit.

## Acceptance boundary

1. **Done/pushed:** P1 Boil CLUT is clip-periodic, interpolating, visually reviewed and gated with two attributed controls.
2. **Done/pushed:** P2 stale comments/report generations are explicitly reconciled.
3. **Historical:** the pre-P3 clean targeted binary passed its complete matrix and 33-subject visual review.
4. **Correctly blocked:** the first exact 28-subject bank exposed P3's Death Drop soft-opaque cutoff at f0233→f0234.
5. **Focused repair done:** soft-opaque death visibility now follows a C2 opacity envelope; the exact legacy output is a strictly attributed positive control; both complete deaths pass.
6. **Still required:** commit/push the P3 source/evidence packet, clean-build one new combined renderer, rerun the complete 31+16 matrix, regenerate the exact 28-subject bank and resume isolated every-frame review from its new manifest.
