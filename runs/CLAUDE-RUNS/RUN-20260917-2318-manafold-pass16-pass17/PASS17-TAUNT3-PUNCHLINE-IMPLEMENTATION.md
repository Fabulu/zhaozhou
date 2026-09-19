# Manafold Pass 17 — Taunt III punchline implementation

**Date:** 2026-09-19
**Directions:** readable comic punchline, asymmetric large/small eyes, independent five-carrier acting, mandatory smooth antenna/effect motion
**Art-generation binary:** MD5 `0CD0600416C4B322DC18E91B68D85F5E`
**Source:** `31949deae836eb29a1add75da09f00dd6b09fc3e` plus declared dirty Pass-17 smooth-motion/art work

## Verdict

**Selected by eye: front-held `yaw=0 / roll=0`, with one held five-carrier accusation.**

The result reads as the planned caught-cheating side-eye: the crown shuffle resolves into a wide, dropped/squashed body, one unmistakably large almond eye and one small eye, opposed attached carrier punctuation, and a continuous lightning figure in the open O. The old `6000 / 7000` side/back hold remains the rejected same-binary control.

The existing attack/hold/release family is preserved: Direction-18 work had already widened the dismissal to a C2 eight-key arrival, and this pass changes the held picture rather than adding another beat.

## Orientation ladder

| Candidate | Yaw / roll | Read |
|---|---:|---|
| legacy away | `6000 / 7000` | Side/back hold; size acting is difficult to read. Rejected control. |
| front squash | `0 / 0` | Both eyes continuously readable, strongest size contrast, held low/wide silhouette; selected. |
| front tip | `0 / 4000` | Readable but less stable at the end of the hold; body tip competes with the eye joke. |

The conditional counter-yaw rung was unnecessary because the primary front-held candidate clearly won at native resolution.

## Held carrier punctuation

The punchline now combines the crown table output and a held signed five-carrier array at the single existing `swallow_nodules` production consumption point:

```cpp
kTaunt3PunchFrontMm = 90;
kTaunt3PunchAMm = 80;
kTaunt3PunchBMm = -180;
kTaunt3PunchCMm = 160;
kTaunt3PunchEndMm = -90;
kTaunt3PunchLeanPm = 150;
```

Front and End cock oppositely while retaining body attachment; A and C rise while B drops. The broad upward/back dismissal still supplies direction. Because the two contributions are combined before the one public path, existing `PublicJointMute` controls isolate F/A/B/C/E without a private diagnostic mechanism.

A later held-aware span gate found the first `A=140` version exceeded F–A's fixed extension budget on 53 presentation samples. `PASS17-HELD-PUNCH-A-SELECTION.md` records the same-binary `60/80/100/140` ladder and selects `A=80` by eye: stronger than 60 at native, with useful margin that the near-ceiling 100 did not earn visually. All other held values and the complete punchline timing remain unchanged.

## Eye acting

The inherited selected values remain:

```text
left 1450 pm / right 750 pm
```

They are driven by the same held `flick` arrival as the body and carriers. At native f0324, the normal clearly differs from `both` muted; independent L/R mutes isolate the expected large and small sides. No scale increase was needed to overpower a hidden face.

## Evidence

Orientation and complete motion:

- `PASS17-TAUNT3-LEGACY-AWAY-ALLFRAMES.png`
- `PASS17-TAUNT3-FRONT-SQUASH-ALLFRAMES.png`
- `PASS17-TAUNT3-FRONT-TIP-ALLFRAMES.png`
- `PASS17-TAUNT3-ORIENTATION-WITNESSES-NATIVE-3COL.png`
- `PASS17-TAUNT3-ORIENTATION-FACE-4X.png`
- `PASS17-TAUNT3-SELECTED-ALLFRAMES.png`
- `PASS17-TAUNT3-SELECTED-FACE-4X.png`

Attributed controls:

- `PASS17-TAUNT3-EYE-MUTES-NATIVE.png`
- `PASS17-TAUNT3-EYE-MUTES-4X.png`
- `PASS17-TAUNT3-JOINT-MUTES-NATIVE.png`
- `PASS17-HELD-A-LADDER-A80-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-WITNESSES-NATIVE.png`
- `PASS17-HELD-A-LADDER-4X.png`

All 368 selected presentation frames were reviewed. The f0304–f0344 hold keeps both eye forms readable; body, carrier punctuation and eye scale arrive coherently; the O remains open around the continuously present lightning figure; no off/reappear, carrier snap, buckled span or outline repaint was seen. F/A/B/C/E mutes each alter the held crown silhouette at native resolution, with B/C strongest and attached F/End deliberately subtler.

The selected native picture meets the Zixxtrixx comparison bar in the way available to this body plan: the eyes remain visible through the extreme, and the complete silhouette changes rather than becoming a featureless sphere. It does not copy Zixxtrixx's gesture.

## Source controls

```cpp
kTaunt3LegacyFlickYawA16 = 6000;
kTaunt3LegacyFlickRollA16 = 7000;
kTaunt3FlickYawA16 = 0;
kTaunt3FlickRollA16 = 0;
```

Existing strict yaw/roll, eye-size and F/A/B/C/E selectors remain the same-binary red controls.

## Remaining final-generation checks

This art-generation was built before the concurrently active checker lane closed its final mote-visibility/mutant-attribution/lab-coherence audit. Therefore art is selected but the generation is not final evidence. After checker freeze, rebuild once and require `mspan` normal plus 31 attributed mutants, `msmooth` normal plus its final mutant matrix, eye/containment/public-joint/outline/contact gates, and a fresh f0304–f0344 render matching this selected picture. No commit, encode or publication is authorized from this generation.
