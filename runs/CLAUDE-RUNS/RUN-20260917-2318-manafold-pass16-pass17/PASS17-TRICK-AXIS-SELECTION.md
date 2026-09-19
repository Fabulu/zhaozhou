# Manafold Pass 17 — Trick axis selection

**Date:** 2026-09-19
**Direction:** preserve the full headstand while removing the long back-facing identity loss
**Art-generation binary:** MD5 `0CD0600416C4B322DC18E91B68D85F5E`
**Source:** `31949deae836eb29a1add75da09f00dd6b09fc3e` plus declared dirty Pass-17 smooth-motion/art work

## Historical verdict

**Rejected by exact-bank review:** pure X (`X=-32768 / Z=0`) preserved contact and improved the body axis, but yaw zero still projected both camera-relative eye plates edge-on through the planted phrase.

This report records the first axis ladder. `PASS17-TRICK-FACE-REPAIR.md` is the current correction: pure X remains the contact half-turn, a separate planted local yaw `+16384` keeps both eyes/stars readable, and yaw zero plus legacy Z remain exact same-binary controls.

The fixed camera, 200-key timing, plant window, root-height curve, balance wobble, antenna flex, righting and recovery remain unchanged. This is an authored selection from complete motion, not an axis derived from a frame measurement.

## Ladder reviewed

| Candidate | Axis | Moving read |
|---|---:|---|
| legacy Z | `0 / -32768` | Reproduces the broad rear-facing mass across f0160–f0295; rejected control. |
| pure X | `-32768 / 0` | Reads as an antenna headstand/cartwheel and preserves contact, but exact-bank 4x review later exposed both eye plates as edge-on; historical provisional selection only. |
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

## Current final-bank correction

A second same-binary ladder held pure X fixed and swept the independent planted local face yaw. Complete 400-frame sheets for `-12288/-8192/-4096/0/+4096/+8192/+12288/+16384` selected `+16384` by eye. It presents both full almond lenses and stars from plant arrival through the hold while preserving the visible antenna support. Current evidence is the five `PASS17-TRICK-FACE-REPAIR-*` plates listed in `PASS17-TRICK-FACE-REPAIR.md`.

## Source controls

```cpp
kTrickLegacyFlipXA16 = 0;
kTrickLegacyFlipZA16 = -32768;
kTrickFlipXA16 = -32768;
kTrickFlipZA16 = 0;
kTrickFaceYawA16 = 16384;
kTrickLegacyShowoffYawA16 = 3000;
kTrickShowoffYawA16 = 0;
```

The X/Z, planted-face-yaw and show-off selectors use strict full-string parsing and reject invalid/out-of-range inputs with RC 2. `FACE_YAW=0` restores the rejected pure-X exact-bank output.

## Remaining final-generation check

The focused correction passes contact/clearance, `mqa`, `mspan`, `meyesize`, `msmooth` normal and all 16 attributed controls. A 28-subject isolation render proves all 27 non-Trick subjects byte-identical. Because the renderer and Trick bytes changed, one new exact canonical bank and fresh complete Trick review remain mandatory before encode.
