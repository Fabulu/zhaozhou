# Manafold Pass 17 — Trick / Taunt III independent review

**Date:** 2026-09-19
**Scope:** read-only review of the selected art source, controls, reports and compact visual evidence
**Source generation reviewed:** art-selection MD5 `0CD0600416C4B322DC18E91B68D85F5E` (selection evidence only; this report predates the final integrated rebuild)

## Verdict

**Historical verdict:** **PASS for the selected art/source packet.** The later final integrated contact and frozen-checker acceptance is green in `PASS17-FINAL-TARGETED-INTEGRATION.md`.

No correctness defect survived review. The source values, same-binary controls, comments and two implementation reports agree. The selected full-frame and focused plates support the stated art decisions without relying on diagnostic-only behavior.

## Trick

- `manafold_art.h` selects pure X (`kTrickFlipXA16=-32768`, `kTrickFlipZA16=0`) and preserves the exact rejected Z-axis control (`0/-32768`). Shipping show-off yaw is `0`; legacy yaw `3000` remains separately selectable.
- `build_trick()` composes the selected X/Z path only through the existing root flip envelope. Plant keys `78..148`, root-height/contact curve, balance wobble, antenna flex, righting, overshoot and recovery are unchanged.
- Pure X keeps the face axis present through the plant. The fixed native comparison rejects legacy Z's rear mass and the mixed candidate's side-tumble read. Complete selected contact sheets show a continuous approach, planted hold and recovery rather than a camera concealment or cut.
- `ZHAO_U02_TRICK_FLIP_X_A16`, `..._Z_A16` and `...SHOWOFF_YAW_A16` use strict full-string signed parsing, reject malformed/trailing/out-of-range values with RC 2, and default to the selected shipping constants before `u02::type()` construction. No diagnostic selector leaks into unset shipping behavior.

## Taunt III

- Shipping body orientation is front-held (`yaw=0`, `roll=0`), with `6000/7000` retained as the exact rejected same-binary control.
- Held punctuation `{90,140,-180,160,-90}` is added to the crown output before the one existing `swallow_nodules()` production consumption point. `apply_public_joint_mute()` therefore remains authoritative for F/A/B/C/E; there is no private punchline path. Front and End retain rotational/body-attached semantics while A/B/C use the ordinary signed nodule/span solve.
- The punctuation follows the same C2 `flick` envelope as the held body arrival. The existing crown release, attack/hold/exit timing and root/body support remain intact.
- Eye scales `1450/750` use the same held `flick`, are authored through `set_eye_scale_pm()`, and structurally carry each Pupil child. Independent L/R/both controls isolate the intended sides.
- The focused native/4× plates show both eye forms throughout f0304–f0344; the selected body no longer presents a side/back mass. F/A/B/C/E mute plates visibly change the named held crown contribution while leaving the rest of the performance present.
- The lightning figure remains present in the open O; this art packet does not disable, dim, reseed or bypass the frozen effect mechanism to expose the face. The complete 368-frame sheet shows no obvious carrier cut, off/reappear, buckled span or outline closure.

## Evidence reviewed

- `PASS17-TRICK-AXIS-WITNESSES-NATIVE-3COL.png`
- `PASS17-TRICK-SELECTED-ALLFRAMES.png`
- `PASS17-TRICK-SELECTED-FACE-4X.png`
- `PASS17-TAUNT3-ORIENTATION-WITNESSES-NATIVE-3COL.png`
- `PASS17-TAUNT3-SELECTED-ALLFRAMES.png`
- `PASS17-TAUNT3-SELECTED-FACE-4X.png`
- `PASS17-TAUNT3-EYE-MUTES-NATIVE.png`
- `PASS17-TAUNT3-JOINT-MUTES-NATIVE.png`

## Required final integration (not a defect in this packet)

After the active checker source freezes, one clean integrated renderer must re-run Trick contact/clearance, `mspan` held-punchline attribution, `msmooth`, eye/public-joint/outline gates, invalid selector controls and exact selected-vs-legacy frames. The art-generation hash above must not be cited as the final Pass-17 bank generation.
