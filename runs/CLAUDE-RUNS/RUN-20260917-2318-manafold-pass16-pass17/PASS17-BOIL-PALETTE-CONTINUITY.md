# Manafold Pass 17 — Boil palette continuity

**Date:** 2026-09-19
**Direction:** Owner Direction 18
**Source review:** `PASS17-FINAL-SOURCE-REVIEW.md` P1
**Focused verdict:** **PASS — palette source/gate and full-loop focused pictures**
**Pass verdict:** final combined integration still requires a clean post-palette rebuild.

## Defect

The live Blue/Violet CLUT used raw presentation time:

```cpp
(frame / 3) % 63
```

The fixed-camera menu loop is 600 presentation frames. Its last frame therefore used rotation 10 and frame zero used rotation 0. Geometry and mana-body motion closed while the emitted colour jumped at the loop seam. The existing `msmooth` body trace could not see it because it measured position/radius/gain, not the production palette operand.

## Production repair

`mana_build_ramps` now receives the production clip period. The Boil phase uses an integer number of revolutions over that period and therefore closes exactly. The authored default is one complete revolution over the 600-frame fixed idle.

The same-binary 1/2/3-cycle ladder was judged at native resolution over all 600 frames:

- **1 cycle selected:** the complete Blue/Violet range remains plainly visible as one broad churn without competing with the body/antenna motion;
- 2 cycles: visibly busier and rejected by the final continuity bands;
- 3 cycles: closest to the historical raw cadence, but repeats the high-contrast reversal three times and reads as palette flicker.

The colour path uses quintic C2 smootherstep between adjacent CLUT rotations. It is interpolating, not a smoothing approximation: at every integer phase the built Blue and Violet ramps are byte-exact to the authored ramp. `msmooth` verifies all 126 integer-phase comparisons, so the fix cannot silently blur, dim or remove the authored colour range.

The renderer owns strict diagnostics:

- `ZHAO_U02_BOIL_CYCLES=1|2|3`;
- `ZHAO_U02_BOIL_PALETTE_CONTROL=none|raw-clock|hard-switch`.

Malformed, trailing, overflow/range and leading-space cycle values, and malformed control names, return RC 2. Unset is shipping identity.

## Production-path gate

`msmooth` calls the same `mana_build_ramps` implementation as the renderer for every frame, the last→first seam and frames 0–2 after wrap.

It traces:

1. Euclidean RGB step/acceleration/jerk for every live entry of both production ramps;
2. seam colour delta;
3. **emitted bloom energy**, weighted by the actual `GlowAssets::bloom` CLUT-index histogram used by `glow_splat` (not the rotation-invariant sum of 63 palette entries);
4. exact integer-phase authored-ramp equality.

Selected one-cycle maxima:

| Operand | Step | Acceleration | Jerk | Gate |
|---|---:|---:|---:|---:|
| CLUT entry RGB | 72.28 | 23.49 | 14.73 | 100 / 60 / 60 |
| Emitted bloom energy (pm) | 45.95 | 15.93 | 12.75 | 80 / 60 / 60 |
| Last→first RGB seam | 3.46 | — | — | 100 |

The bands were chosen after native full-loop and badness review. They retain real headroom and reject the faster ladder/control paths; they do not generate the art.

Candidate receipts:

- 1 cycle: RC 0;
- 2 cycles: RC 1 (`141.24 / 82.43 / 90.03` RGB; `86.42 / 52.26 / 50.79` energy);
- 3 cycles: RC 1 (`204.21 / 155.16 / 243.25` RGB; `113.14 / 97.11 / 123.67` energy);
- invalid cycle: RC 2.

Two committed controls are strictly attributed to the palette category alone:

- `--fail-palette-raw-clock`: RC 1; RGB `369.39 / 369.39 / 738.78`, seam `327.75`, emitted energy `536.42 / 536.42 / 1072.84`;
- `--fail-palette-hard-switch`: RC 1; RGB `369.39 / 369.39 / 738.78`, seam `369.39`, emitted energy `240.44 / 240.44 / 480.88`.

Final focused gate MD5: `E1DD063275637C1F4FC2D5525D7482AB` (normal RC 0; 15/15 total effect controls attributed RC 1 after adding the two palette controls).

## Picture evidence

Reviewed every frame:

- `PASS17-BOIL-PALETTE-C1-ALLFRAMES.png` — selected;
- `PASS17-BOIL-PALETTE-C2-ALLFRAMES.png` — rejected faster rung;
- `PASS17-BOIL-PALETTE-C3-ALLFRAMES.png` — rejected historical-speed rung;
- `PASS17-BOIL-PALETTE-RAW-CONTROL-ALLFRAMES.png`;
- `PASS17-BOIL-PALETTE-HARD-CONTROL-ALLFRAMES.png`;
- `PASS17-BOIL-PALETTE-CYCLE-LADDER-NATIVE.png`;
- `PASS17-BOIL-PALETTE-C1-SEAM-4X.png`;
- `PASS17-BOIL-PALETTE-CONTROLS-4X.png`;
- `PASS17-BOIL-PALETTE-SELECTED-BLUE-ALLFRAMES.png`.

The selected seam stays cyan/blue continuously through f0599→f0000. The raw control visibly resets hue/brightness; the hard-switch control steps between discrete states. The Blue menu row retains its filled-plasma read across all 600 frames.

Final focused renderer MD5: `747CD6946C49EE05A248479EDECFF81A`.

The exact selected final render contains 600 Boil and 600 Blue frames and is byte-identical to the reviewed selected generation:

- Boil: 600/600 identical, sequence SHA-256 `b9fe790d5bcefaaafc15e2906ced54d48c5cadfc6d77b0d73655f53167594e70`;
- Blue: 600/600 identical, sequence SHA-256 `84821bf0edb06f67fdc3b50ea914ea00272db29cae52aaac701e732423a005ef`;
- raw-clock control: 600/600 identical to the reviewed control generation;
- hard-switch control: 600/600 identical to the reviewed control generation.

## Files changed

- `tools/reel/manafold_fx.h`
- `tools/reel/manafold_motiongate.cpp`
- `tools/reel/zhao_reel.cpp`
- current schedule comments in `tools/reel/manafold_clips.h`
- historical/current labels in `PASS17-PUBLIC-ORDERING-IMPLEMENTATION.md`
- this report and `TASK_LOG.md`

No commit, push, encode or deployment was performed. The coordinator must rebuild the complete targeted packet once from this frozen palette source before promoting final evidence.
