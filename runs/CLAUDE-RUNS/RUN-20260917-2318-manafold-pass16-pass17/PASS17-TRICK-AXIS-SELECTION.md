# Manafold Pass 17 — Trick axis selection

**Date:** 2026-09-19
**Direction:** preserve the full headstand while removing the long back-facing identity loss
**Art-generation binary:** MD5 `0CD0600416C4B322DC18E91B68D85F5E`
**Source:** `31949deae836eb29a1add75da09f00dd6b09fc3e` plus declared dirty Pass-17 smooth-motion/art work

## Verdict

**Selected by eye: pure X half-turn, `X=-32768 / Z=0`, with show-off yaw `0`.**

The old `X=0 / Z=-32768` remains the exact rejected same-binary control. The selected path keeps the body visibly balanced over the planted antenna instead of showing a featureless rear mass through the entire hold. The fixed camera, 200-key timing, plant window, root-height curve, balance wobble, antenna flex, righting and recovery are unchanged.

This is an authored selection from complete motion, not an axis derived from a frame measurement.

## Ladder reviewed

| Candidate | Axis | Moving read |
|---|---:|---|
| legacy Z | `0 / -32768` | Reproduces the broad rear-facing mass across f0160–f0295; rejected control. |
| pure X | `-32768 / 0` | Reads as an antenna headstand/cartwheel, keeps the face-side eye forms present and preserves a centred plant; selected. |
| primary mixed | `-32000 / -6000` | Adds a sideways cant without improving identity; weaker than pure X. |
| conditional mixed | `-28000 / -12000` | Turns the plant into a broad horizontal/side-tumble read; rejected. |

The inherited sinusoidal show-off yaw was separately removed from shipping (`3000 -> 0`). With the selected X axis it pushed the face-side forms back toward the edge while the balance wobble and antenna flex already kept the hold alive. `kTrickLegacyShowoffYawA16=3000` and `ZHAO_U02_TRICK_SHOWOFF_YAW_A16` preserve the comparison.

## Evidence

- `PASS17-TRICK-LEGACY-Z-ALLFRAMES.png`
- `PASS17-TRICK-PURE-X-ALLFRAMES.png`
- `PASS17-TRICK-MIXED-X32-Z6-ALLFRAMES.png`
- `PASS17-TRICK-AXIS-WITNESSES-NATIVE-3COL.png`
- `PASS17-TRICK-AXIS-FACE-4X.png`
- `PASS17-TRICK-PURE-VS-CONDITIONAL-4X.png`
- `PASS17-TRICK-SELECTED-ALLFRAMES.png`
- `PASS17-TRICK-SELECTED-FACE-4X.png`

All 400 presentation frames of each primary candidate and the selected result were reviewed. The selected action approaches continuously, remains an inverted antenna balance through the complete plant, and returns through the existing overshoot/recovery without a visual cut. The conditional mixed candidate visibly violates the intended upright headstand read and was not promoted.

## Source controls

```cpp
kTrickLegacyFlipXA16 = 0;
kTrickLegacyFlipZA16 = -32768;
kTrickFlipXA16 = -32768;
kTrickFlipZA16 = 0;
kTrickLegacyShowoffYawA16 = 3000;
kTrickShowoffYawA16 = 0;
```

The X/Z and show-off selectors use strict full-string parsing and reject invalid/out-of-range inputs with RC 2.

## Remaining final-generation checks

The pictures select the art. Final acceptance still requires one clean rebuild after the concurrently active smoothness-checker freeze, followed by `mprobe` contact/clearance, `mspan`, `msmooth`, `mqa`, `mnodule`, `mjointpub`, `meyecam`, `meyesize`, `moutline`, mesh checks and a final selected-vs-legacy render from that same binary. This art-generation binary is selection evidence, not the final bank generation.
