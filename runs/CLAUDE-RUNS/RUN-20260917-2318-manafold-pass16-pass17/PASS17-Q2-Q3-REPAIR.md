# Manafold Pass 17 — Q2/Q3 continuity repair

**Date:** 2026-09-18
**Plan:** `PASS17-Q2-Q3-REPAIR-PLAN.md`
**Renderer MD5:** `41620610BF86B36EA7AEB8279434F834`

## Verdict

**PASS.** The death-eye carrier no longer resets at either settle key, and Startle keeps its hard held payoff with useful headroom under the unchanged continuity gate.

## Q2 — death-eye settle continuity

`apply_eye_schedule` now owns only the camera-relative EyeTravel carrier schedule. `antenna_knead` calls it before living nodule/hinge work. Each death calls only `apply_eye_schedule(..., eye_pm=0)` after settle: the fixed-camera base remains, while glance, expressive lean, nodules and hinge life stay off.

The committed control (`g_u02_death_fail == 5`, exposed to renders as `ZHAO_U02_DEATH_EYE_CONTROL=legacy`) restores the old unfaded-then-identity reset before static bank construction.

Q2 now differences consecutive left and right EyeTravel quaternions by absolute normalized quaternion dot and relative angular distance. It can see axis changes and equal-magnitude direction reversals that the old scalar-magnitude subtraction could miss.

### Gate result

- normal `--eyes-only`: RC 0;
- death-drop worst normalized relative eye-carrier step: **1.89°**;
- death-gutter worst normalized relative eye-carrier step: **2.57°**;
- unchanged gate: **8°/key**;
- `--fail-eyesnap`: RC 0 only after Q2 itself rejects both named settle resets:
  - drop key 117: **44.96°**;
  - gutter key 191: **59.90°**.

### Visual result

Every presentation frame passed in:

- `PASS17-Q2-DEATH-DROP-ALLFRAMES.png` (450 frames);
- `PASS17-Q2-DEATH-GUTTER-ALLFRAMES.png` (590 frames).

`PASS17-Q2-DEATH-SETTLE-AB-NATIVE.png` and `PASS17-Q2-DEATH-SETTLE-AB-4X.png` compare the last living/first dead frames against the same-binary legacy control. The normal face continues through settle and remains held; the control visibly repositions the eye shapes at the death boundary. Body contact, droop, root, closed lids, deform and eternal-rest timing remain intact.

Normal sequence CRCs:

- death-drop: `0xDB162BC8`;
- death-gutter: `0x73661F04`.

Legacy controls: `0x47BFD254` / `0x79294291`.

## Q3 — Startle root continuity

The authored displacement is unchanged. Shipping timing is now a named tuple:

```text
anticipation 8 -> arrival 12 -> hold end 22
```

The selected 250×100 eye form reaches its 1350 pm Startle scale on the same arrival key. Recoil knots, whip, squash, gaze, root amplitudes and recovery remain unchanged. `ZHAO_U02_STARTLE_TIMING=legacy|late|early` is validated before bank construction; shipping is `late`. Invalid values return RC 2.

### Gate result

- normal full `mqa`: RC 0;
- shipping Startle worst root step: **217.6 mm** (42.4 mm headroom under the unchanged 260 mm ceiling);
- `--fail-startle-step`: RC 0 only after legacy `8 -> 11 -> 21` timing makes slot 4 reach **290.0 mm** and Q3 rejects it;
- invalid timing selector: RC 2.

### Visual result

- `PASS17-Q3-STARTLE-LATE-ALLFRAMES.png`: all 160 presentation frames are continuous;
- `PASS17-Q3-STARTLE-LATE-LEGACY-NATIVE.png` and `...-4X.png`: the late arrival preserves the compressed anticipation, hard rise, large-eye/body synchronization and full held extreme. It differs only over the intended attack frames and is identical again by the held arrival/recoil.

The four-key attack remains abrupt and readable rather than floaty, while removing the one-key over-ceiling step. The conditional early-launch rung was not needed.

Normal Startle CRC: `0xD4B01A04`; legacy timing control: `0xA989EBDD`.

## Integration gates

Fresh direct builds/runs from the same source:

- `mqa` normal: RC 0;
- `mqa --eyes-only`: RC 0;
- `mqa --fail-eyesnap`: attributed control PASS;
- `mqa --fail-startle-step`: attributed control PASS;
- `mprobe`: RC 0 (clearance/contact and scale-aware eye leash);
- `meyesize`: RC 0;
- `mspan`: RC 0;
- `mnodule`: RC 0;
- invalid death-eye and Startle renderer selectors: RC 2.

## Files

- `tools/reel/manafold_art.h`
- `tools/reel/manafold_clips.h`
- `tools/reel/manafold_qa_p12.cpp`
- `tools/reel/zhao_reel.cpp`
- this report and the committed evidence plates named above.

As I always say, a surprise should make the creature jump—not make the animation skip a stair!
