# Manafold Pass 17 — Trick / Taunt III independent review

**Date:** 2026-09-19
**Scope:** read-only review of the selected art source, controls, reports and compact visual evidence
**Source generation reviewed:** art-selection MD5 `0CD0600416C4B322DC18E91B68D85F5E` (selection evidence only; this report predates the final integrated rebuild)

## Verdict

**Historical verdict:** Taunt III survived final-bank review, but the original pure-X/yaw-zero Trick selection did not. Exact-bank 4x review found both eye plates edge-on through the planted phrase. `PASS17-TRICK-FACE-REPAIR.md` supersedes only the Trick art verdict; the source/control review remains provenance.

## Trick

- `manafold_art.h` retains pure X (`kTrickFlipXA16=-32768`, `kTrickFlipZA16=0`) for contact and preserves the rejected Z-axis control (`0/-32768`). Exact-bank review disproved the old claim that pure X alone kept the rendered face.
- `build_trick()` now composes a separate `kTrickFaceYawA16=16384` through the existing flip envelope. The +90-degree local yaw changes the planted viewing azimuth continuously without moving the camera or changing plant keys `78..148`, root height/contact, balance wobble, antenna flex, righting, overshoot or recovery.
- Complete 400-frame `-12288..+16384` yaw ladders select +16384 by eye. Both complete lenses/stars and the antenna support read from plant arrival through the hold; yaw zero restores the exact rejected bank output and legacy Z remains the earlier rear-mass control.
- `ZHAO_U02_TRICK_FACE_YAW_A16` joins the X/Z/show-off selectors with strict full-string signed parsing. Five malformed/trailing/overflow/out-of-range/leading-space cases return RC 2, and unset uses the selected shipping constant before `u02::type()` construction.

## Taunt III

- Shipping body orientation is front-held (`yaw=0`, `roll=0`), with `6000/7000` retained as the exact rejected same-binary control.
- Held punctuation `{90,140,-180,160,-90}` is added to the crown output before the one existing `swallow_nodules()` production consumption point. `apply_public_joint_mute()` therefore remains authoritative for F/A/B/C/E; there is no private punchline path. Front and End retain rotational/body-attached semantics while A/B/C use the ordinary signed nodule/span solve.
- The punctuation follows the same C2 `flick` envelope as the held body arrival. The existing crown release, attack/hold/exit timing and root/body support remain intact.
- Eye scales `1450/750` use the same held `flick`, are authored through `set_eye_scale_pm()`, and structurally carry each Pupil child. Independent L/R/both controls isolate the intended sides.
- The focused native/4× plates show both eye forms throughout f0304–f0344; the selected body no longer presents a side/back mass. F/A/B/C/E mute plates visibly change the named held crown contribution while leaving the rest of the performance present.
- The lightning figure remains present in the open O; this art packet does not disable, dim, reseed or bypass the frozen effect mechanism to expose the face. The complete 368-frame sheet shows no obvious carrier cut, off/reappear, buckled span or outline closure.

## Evidence reviewed

- `PASS17-TRICK-AXIS-WITNESSES-NATIVE-3COL.png` (historical ladder)
- `PASS17-TRICK-SELECTED-ALLFRAMES.png` (rejected yaw-zero selection)
- `PASS17-TRICK-SELECTED-FACE-4X.png` (rejected yaw-zero selection)
- current `PASS17-TRICK-FACE-REPAIR-*` ladder/selected/control plates
- `PASS17-TAUNT3-ORIENTATION-WITNESSES-NATIVE-3COL.png`
- `PASS17-TAUNT3-SELECTED-ALLFRAMES.png`
- `PASS17-TAUNT3-SELECTED-FACE-4X.png`
- `PASS17-TAUNT3-EYE-MUTES-NATIVE.png`
- `PASS17-TAUNT3-JOINT-MUTES-NATIVE.png`

## Current integration boundary

The focused Trick correction passes unchanged contact/clearance, `mqa`, `mspan`, `meyesize`, `msmooth` normal plus all 16 controls and strict selector tests. A one-process isolation bank keeps all 27 non-Trick subjects byte-identical. A new exact canonical bank and fresh 400-frame Trick verdict remain before encode; the historical art-generation hash above and both rejected bank hashes must not be cited as final.
