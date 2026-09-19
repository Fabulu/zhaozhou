# Manafold pass 19: diagnosis and fix plan

**Date:** 2026-09-19
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-20-2026-09-19.md`
**Worker:** Claude (sole Opus worker). No Qwen.
**Baseline:** Zhaozhou `255c6ece` (v18 production source `db2bcf0e` plus a qa-checker-only change). The baseline renderer is the accepted v18 binary `.tmp/v18-int-final/bin/zhao-reel-cel.exe`, MD5 `0f082622d4ca0c58d012d1f0de555723`. Its fresh renders of Hover `0x24B3FE60`, Inspect `0x95A27283` and Drift `0x69158A83` match the v18 bank exactly.

**Verdict:** Items 1 and 2 have **one structural cause**: the End carrier's frame points the wrong way. Item 3 has a narrow cause in the renderer. Both fixes are small, have exact legacy controls, and leave every art value on a named knob.

## Instrument

`tools/reel/manafold_rear_audit.cpp` (build target `mrear`) is new and committed. It reads the posed skeleton and the compiled skin through `zc::decode_pose` / `zc::skin_vertex`, never through pixels. For every key and baked midpoint (60 Hz) it reports:
- the rotation between the return arm (HingeD) and the End carrier (RearSocket);
- the turning angle of the posed tube centreline (ring centroids) in the rear window;
- the End carrier's local rotation, and the motion energy of the End ring, the last free run and carrier C.

`--rings <key>` dumps the root-local centreline. It is a diagnostic. It chooses no values.

## 1. What reads as torn (item 1): looked at first

Production ink (`ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`), Inspect f0/f10/f20/f30, 3× crop of the rear entry (`p19-look/inspect-rear-4x-a.jpg`, scratch). At the body entry the arm narrows. Then a **second, parallel piece of tube juts out beside it** as a stepped notch or bracket, and the joint reads as broken. The problem is not a material seam, not an ink seam and not a depth or shell artefact. It is **geometry**: the tube folds back on itself.

## 2. Bone audit: why it folds (items 1 and 2 are one cause)

### Every authority that moves the rear chain

| Authority | What it writes | Frame |
|---|---|---|
| Closure aim, `finalize_rear_follow` | `kBHingeD` re-aimed each key/midpoint from the end of C to the body-following socket point S. The signed C→End span stretches to reach S. | Root |
| Body follow | `kBRearSocket` / `kBReturnTip` translations from the deformed body point (breathing) | Root |
| `hinge_play` End station | `tilt_end/yaw_end` onto `kBRearSocket` (1500/1100 a16 × `kHingeAxisScalePm[4]=700`, periods 23/51 keys) | Root-local |
| `antenna_knead` "B2" wag | `quat_z` onto `kBRearSocket`, `kKneadWagB2A16 = 4600` a16 (25°) × agit envelope × `press_wave` (22-key period) | Root-local |
| `swallow_nodules` End beat | `quat_x` onto `kBRearSocket`, 20 a16/mm × swallow press (idle 96 mm, i.e. 10.5°) | Root-local |
| Per-clip End curves | Lasso `end_turn`, nodule-solo seg 4 | Root-local |
| v18 root helpers 24/25/26 | RearPre / TurnMid / RearRoot = 1/3, 2/3, 1 of `conj(arm) × socket`, over rings 50/51/52 | re-expression |
| Signed span helpers 19–22 | exact fractions of the C→End stretch | translation |

### The fault: `kBRearSocket` has no rest orientation

The skeleton is bound as one straight chain along +Y. Every chain bone gets its attitude from its pose rotation: JunctionF carries `kNeckRestYawA16` plus the fold, and A/B/C carry their folds. **When pass 16 split the End carrier off the chain as a Root child, it kept an identity rest rotation.** Its End rings are therefore laid along **Root +Y (straight up)**, while the arm arrives travelling **down** into the body.

The audit, Hover key 0 (root-local):

```
HingeD +Y  ( 0.175, -0.932, -0.317)   <- the arm descends into the body
RearSocket +Y ( 0.000, +0.996, +0.093)   <- the End rings point straight up
ring 52 (-402, 707)  ring 53 (-385, 579)  ring 54 (-369, 452)  ring 55 (-358, 382)
ring 56 (-358, 429) ... ring 57 = End (-358, 475) ... ring 62 (-360, 707)
```

The tube dives through S to 382 mm and **turns back up**, and rings 58–62 stand up to 207 mm above the socket, out of the body, beside the incoming arm. That upright post is the v18 open item "End-swell stub" (Rest 342, Taunt III 328, Trick 393). It is also the owner's "almost ripped off" notch.

| Clip (slot) | arm↔End frame rotation, max / mean | rear centreline turn, max (ring) | End-local rotation max, step/sample |
|---|---|---|---|
| Hover/Inspect (0) | 169.8° / 150.7° | 166.5° (55) | 25.7°, 5.38° |
| Rest (5) | 169.7 / 149.8 | 166.3 (55) | 21.0, 3.37 |
| Taunt III (21) | 175.1 / 168.3 | 170.5 (55) | 9.9, 1.15 |
| Channel (2) | 170.3 / 151.0 | 164.8 (55) | 24.0, 5.08 |
| Drift (1) | 169.5 / 149.3 | 169.1 (55) | 22.4, 3.96 |
| Trick (13) | 173.0 / 167.0 | 171.1 (55) | 0.0, 0.00 |
| Lasso (19) | 173.2 / 151.0 | 163.4 (55) | 23.6, 3.44 |

For reference, the largest centreline turn in the front window (the drawn loop corners) is 77–107°. The rear hairpin is 150–170° **on every sample of every clip**. It is structural, not a clip or amplitude problem. The legacy-split skin (v17) has the same bone frames. Version 18's rotation helpers turned the same 150° mismatch into an explicit rigid curl over three rings (50/51/52) instead of an LBS pinch.

### Why this is also the "spazzy" last ball and "a bone too much" (item 2)

The End carrier's own rotation authorities (the 25° press-wave B2 wag, the 5.8°/4.2° hinge-play buzz and the 10.5° swallow beat) all turn the **upright stub**. The stub is a 200+ mm lever standing out of the body, and it turns independently of the arm beside it: up to 5.4° per 60 Hz sample in Hover. The arm meanwhile re-aims at S as carrier C moves (arm direction ±47° over the Hover loop). So two pieces at the rear move on unrelated clocks, the joint between them tears and closes, and the owner sees one bone too many. **The End carrier is exactly that bone: a whole Root-framed segment pointing the wrong way at the end of the chain.**

The arm itself (the "last antenna part") moves because C moves. Its root-local path is half of C's (Hover: C 11.6 m, last-run centre 5.9 m, End ring 0.54 m per loop). That is inherited upstream motion, not a rear fault. It is judged by eye after the frame fix, and damped only if it still reads spazzy.

## 3. Mana fold lines (item 3)

- **The outline ink** scales with distance. `cel_main_ink_width(primary_radius_q8)` maps the creature's projected bound radius to 1 px (≤120 px), 2 px (200 px) and 4 px (≥360 px).
- **The fold lines do not.** The strand's navy backing (`kFoldStrandDarkRPx = 14`), blue shimmer and white core (`kFoldStrandCoreRPx = 2`), and the free bolt strands (`kBoltHaloRPx = 6`, `kBoltCoreRPx = 3`), are pushed with constant **screen-pixel** radii and drawn with those radii whatever the distance.
- Drift sits at a projected radius of 128 px (ink 1 px). There the fold figure becomes a fat glowing blob over the tiny antenna window (`p19-look/drift-overview.jpg`).

**Narrowest change:**
1. Mark line splats with an appended `ManaSplat::line` flag. It is set only on fold-edge strand layers and bolt strands. Motes, bodies and glows keep their size.
2. At draw time the renderer scales a line splat's radius by the **same operand the ink uses**, the primary projected radius, relative to a named reference radius at or above which the width is exactly the legacy width. There is a named 1 px floor.
3. `ZHAO_U02_MANA_LINE_SCALE=distance|legacy` is strict, and `legacy` reproduces v18 bytes.
4. The reference radius is a named knob, `ZHAO_U02_MANA_LINE_REF_PX`, chosen by eye on Drift (far) against Hover/Taunt III (near). Close-up subjects at or above the reference are byte-identical by construction.

## 4. Fix plan

### Rear (items 1 and 2): give the End carrier the arm's frame

In `finalize_rear_follow` (keys) and `finalize_rear_follow_midpoints`:
- `RearSocket = Base × Authored`.
- `Authored` is the unchanged root-local composition of every End authority above.
- `Base` is the arm's arrival frame `qd = Q_C × aim`. That is the rotation the chain already computes to reach S. A named follow knob can blend `Base` toward the constant rest arrival frame for the ladder.

The staged helpers then carry only `conj(qd) × RearSocket = Authored` (≤ ~25°) instead of a 150° hairpin. The End rings continue straight along the arm into the body. The swell's outer half sits on the arm, its inner half is buried, and the ReturnTip continuation, already placed along that same line by `kRearSocketBurialMm`, now agrees with the rings.

No bone is added or removed. RearSocket keeps its identity, its body-following centre and its public End authority. What changes is the frame that authority acts in: **a joint bend relative to the arm**, not a free stub.

Named knobs:
- `ZHAO_U02_REAR_SOCKET_FRAME=arm|legacy-root` (strict). `legacy-root` is the exact v18 control.
- `kRearSocketArmFollowPm` (1000 = arm frame, 0 = constant rest arrival), with `ZHAO_U02_REAR_SOCKET_FOLLOW_PM` for the ladder.
- `kRearSocketJointGainPm` (1000 = authored End rotations as today), with `ZHAO_U02_REAR_JOINT_GAIN_PM`. It scales only the End carrier's own authored rotation. It is the "a bit wiggly" knob, chosen from a complete-motion ladder.
- If the last free run still reads over-animated after the frame fix, a C/End station damping knob is considered and chosen by eye. Otherwise nothing upstream changes.

The call contract stays "once per freshly built clip". All call sites (bank, nodule, jointgate, qa, probe, spangate) finalize fresh builds exactly once.

### What stays protected

Front root, signed spans, root-authority palettes (RearRoot still equals RearSocket's rotation), terminal cap burial, Trick support/pin, all effect identities, the live-history contract and every archive byte. Only Manafold content changes: no generic format, no RTL, no fit.

### Gates

- **Existing, all green:** mspan (incl. root/terminal-cap/swell/front-flex and 35 controls), msmooth (16 controls), mprobe (incl. Trick support/pin legs), mjointpub (End mute leg, whose visible End response must survive the frame change), mqa (incl. Wave-F legs), mmeshcheck, moutline, mshell (+selftest), mnodule, meyesize, live-history, checkarchive (site unchanged).
- **New `manafold-rear-audit --gate`:**
  - **R1 FRAME:** every shipping key/midpoint, arm↔End rotation and rear centreline turn under named ceilings. The positive control `--fail-rear-frame` (legacy-root) must fire R1 only.
  - **R2 JOINT:** the End joint's angular step per 60 Hz sample is bounded. The positive control `--fail-rear-joint` (joint gain ×3) must fire R2 only.
  - **R3 LINE:** the mana-line width law gives exact legacy width at or above the reference, never exceeds legacy, is monotone in distance and has a 1 px floor. The positive control `--fail-line-scale` (legacy law) must fire R3 only.
- **Legacy toggles:** `ZHAO_U02_REAR_SOCKET_FRAME=legacy-root` + `ZHAO_U02_MANA_LINE_SCALE=legacy` must reproduce the v18 bytes (Hover `0x24B3FE60`, Inspect `0x95A27283`, Drift `0x69158A83`, Taunt III `0x07EACF1D`) exactly.

### Art acceptance

Art is accepted by eye:
- complete production-ink motion for Inspect, Hover, Taunt III, Rest, Channel, Drift and Trick;
- native and 4× rear plates, before and after;
- a follow/joint-gain ladder on Inspect and Taunt III;
- a line-reference ladder on Drift (far) against Hover (near).
