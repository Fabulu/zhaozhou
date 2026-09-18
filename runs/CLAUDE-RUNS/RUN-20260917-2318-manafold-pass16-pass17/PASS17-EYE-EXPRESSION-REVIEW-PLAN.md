# Manafold Pass 17 eye-expression review plan

**Date:** 2026-09-18
**Scope:** render/review plan only; no source, build, render, commit or publish action in this packet
**Binding direction:** Owner Direction 14 — eyes must visibly become larger **and** smaller for expression, after replacing the inherited dagger/splinter neutral form

## Acceptance question

At final 384×240 presentation, do the selected 250×100×40 mm eyes visibly act larger and smaller as one registered lens/white-star/cyan-star unit in Curious, Startle and Taunt, while blink, travel, gaze, roll and body following remain readable?

Taunt III is the fourth expression clip, but it is not eligible for this verdict until its held punchline is front-readable. The present f0300–f0350 side/back hold hides the face, so rendering that source again would measure orientation rather than eye acting.

## Source state to put on the pictures

- Selected neutral form: **250×100×40 mm**.
- Same-binary rejected form: **270×84×40 mm** (`ZHAO_U02_EYE_FORM=legacy`).
- Curious: left `1250 pm`, right `820 pm`, driven by the double-take focus curve.
- Startle: both `1350 pm`, driven by the widen curve.
- Taunt: left `1350 pm`, right `800 pm`, driven by the cross-eyed hold.
- Taunt III: left `1450 pm`, right `750 pm`, driven by the dismissal/flick hold.
- Renderer controls: `ZHAO_U02_EYE_SIZE_MUTE=none|L|R|both` and `ZHAO_U02_EYE_FORM=current|legacy`.

Every run below must use one freshly built binary. Only environment selectors change. Record source commit/dirty state, binary MD5, `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`, exact command and per-subject sequence CRC.

## Minimal same-binary render matrix

| Rung | Form | Size mute | Subjects | Question |
|---|---|---|---|---|
| S0 shipping | current | none | Curious, Startle, Taunt | Does authored size acting read in motion? |
| S1 identity control | current | both | Curious, Startle, Taunt | What disappears when size alone is removed? All other motion must remain. |
| S2 left ablation | current | L | Curious, Taunt | Does only the named left-eye large/small contribution disappear? |
| S3 right ablation | current | R | Curious, Taunt | Does only the named right-eye contribution disappear? |
| F0 selected neutral | current | both | Hit | Current almond under fixed oblique impact poses, with size acting removed. |
| F1 rejected neutral | legacy | both | Hit | Exact old dagger/splinter control from the same binary. |
| P0 travel protection | current | none | Hover | No size track: full orbit protects ordinary travel/readability against integration regressions. |

Do not render L/R ablations for Startle merely to enlarge the matrix. Startle intentionally enlarges both eyes together; S0 versus S1 answers its visual question, while `meyesize` proves independent L/R mechanics.

After Taunt III's body orientation is repaired, add a second bounded matrix from that **new** binary:

| Rung | Form | Size mute | Subject |
|---|---|---|---|
| T0 shipping | current | none | Taunt III |
| T1 identity | current | both | Taunt III |
| T2 left ablation | current | L | Taunt III |
| T3 right ablation | current | R | Taunt III |

The existing Hit F0/F1 form control remains sufficient; do not re-prove the static form inside Taunt III.

## Exact review windows and witnesses

All frame numbers below are 60 Hz presentation frames.

### Curious — 180 frames

Review every frame. The eye scale follows the same `kSide` curve as gaze.

- `f0000`: exact neutral/identity.
- `f0016`: first full left-large/right-small fixation arrives (key 8).
- `f0076`: end of first full fixation (key 38).
- `f0088–f0100`: glance-away trough; the source retains a small `120 pm` focus term rather than exact identity.
- `f0108`: second full fixation snaps back (key 54).
- `f0132`: end of second full fixation (key 66), immediately before the scheduled blink window.
- `f0156`: exact identity return (key 78).

Native A/B priorities: S0 versus S1 at f0016/f0076/f0108/f0132; S0/S2/S3 at f0016 and f0108. The large/small change must survive the body yaw, gaze and brow rather than read as those channels alone.

### Startle — 160 frames

The Q3 continuity repair is a prerequisite. Current source lands body/scale at key 11 (`f0022`); the ratified repair plan moves the held arrival to key 12 (`f0024`) and must move `kWide`/eye-size arrival with it. Confirm the final source table before labelling evidence.

Expected post-repair witnesses:

- `f0000`: neutral.
- `f0016`: anticipation, identity-size eyes.
- `f0024`: both eyes reach full `1350 pm` with the body payoff.
- `f0042`: held wide extreme.
- `f0072`: last full-size frame (key 36).
- `f0104`: returned to identity (key 52).

S0 versus S1 at f0024/f0042/f0072 is the decision. Preserve the fast surprised read: a larger eye that arrives before/after the body is not synchronized expression.

### Taunt — 280 frames

Review every frame, not only the frozen hold.

- `f0120`: cross/size beat begins (key 60).
- `f0136`: full `1350/800 pm` asymmetry arrives (key 68).
- `f0140–f0148`: full asymmetry while the left wink arrives, before the ordinary blink.
- `f0150–f0159`: ordinary blink crosses the hold; do **not** use these frames alone to judge size.
- `f0164`: late held wink/asymmetry.
- `f0172`: final full cross/size frame (key 86).
- `f0192`: identity return (key 96).

Use S0/S1 at f0136, f0144, f0164 and f0172. Use S2/S3 at f0136 and f0164. The left-large eye is deliberately also the winked eye; if the large read vanishes beneath lid rotation, the current schedule/value combination fails visually even though the scale track is correct.

### Taunt III — defer until face-readable punchline

Current source is not acceptance evidence: it turns the face away during the exact scale hold. After the orientation repair, review all 368 frames and these witnesses:

- `f0292`: dismissal begins, identity-size comparison.
- `f0298`: full `1450/750 pm` asymmetry arrives.
- `f0304`: body lag has landed; held punchline starts.
- `f0324`: mid-hold.
- `f0344`: end of readable hold.
- `f0350`: release.

T0/T1/T2/T3 must use the same frames. The face, both purple lenses and both nested stars must remain visible throughout f0304–f0344. Compare the accepted result beside the committed Zixxtrixx taunt grids; the joke must read without prose.

### Static form and travel protection

- Hit F0/F1: reuse fixed presentation witnesses `f0016`, `f0028`, `f0036` from the selected form ladder.
- Hover P0: all 600 frames; native witnesses `f0000`, `f0150`, `f0300`, `f0450` cover one full orbit.

## Evidence products

Use the committed `tools/reel/plates.py` / `rgbframe.py`; do not write another frame reader.

1. **Every-frame sheets:** S0 and S1 for Curious, Startle and Taunt; T0/T1 only after Taunt III repair; P0 Hover protection. Downsampling is permitted for the locator sheet, but every presentation frame must be present.
2. **Native 384×240 A/B grids:** exact witnesses above, shipping beside identity control. Native frames decide whether the expression reads.
3. **Exact 4× nearest-neighbour eye crops:** same native crop box for every rung of one subject/frame. Choose the box by looking at the first normal native frame, record it, and never silently move/clamp it between controls.
4. **L/R isolation grids:** Curious and Taunt S0/S2/S3 at their two strongest non-blink witnesses.
5. **Form control:** Hit F0/F1 at f0016/f0028/f0036, native and 4×.
6. **Protected before/after witnesses:** selected Hover orbit frames and Taunt's non-size channels. A contact sheet locates a candidate; a native/crop confirms any claimed absence or splinter.

## Structural and containment gates

Run from the same settled source as the renderer:

- `test_creature_core`: optional scale identity, midpoint and child propagation.
- `meyesize` normal.
- `meyesize --fail-mute L`, `--fail-mute R`, `--fail-wrong-bone`; each must print its attributed red-leg success, not merely return green because another assertion failed.
- `mprobe` normal: scale-aware whole-bank star/lens/body leash.
- `mprobe --fail-scale-inverse` and `--fail-outline`: both deliberate faults must turn the relevant leash red.
- `meyecam`: both-eye camera readability across the integrated bank.
- `mmeshcheck`: visible lens/star ownership and topology remain clean.
- `mexpress`: descriptive comparison only; it cannot overrule pictures.
- Renderer invalid selectors (bad form, out-of-range dimension, bad mute) must return RC 2.

`meyesize` already proves track allocation only on slots 3/4/11/21, exact L/R Q1.15 values, no scale leakage and Eye→Pupil basis registration. The review must now prove that correct machinery produces the requested read.

## Source-visible risks the pictures must answer

1. **Curious never swaps sides.** The focus is nonnegative, so left is always the large eye and right always the small one. That is valid asymmetry only if both remain visible under the fixed-camera root yaw.
2. **Curious's glance-away is not exact neutral.** `focus=120` yields a residual size difference; use f0000/f0156 as true identity controls rather than calling f0092 neutral.
3. **Startle synchronization is currently moving.** Q3's planned 11→12 arrival retime must move the size/widen arrival with the body. Review only the repaired source.
4. **Taunt stacks three silhouette changes.** Cross-eyed gaze, left wink and ordinary blink overlap the size hold. The left-large eye may be hidden by the wink; use non-blink and blink witnesses separately.
5. **Taunt III currently measures the wrong thing.** Its scale is strongest exactly while the face is side/back. Fix orientation first. Even afterward, `620/420 pm` squint and body squash may visually mask the size contrast; T0/T1 isolates that question.
6. **Uniform scale must remain one nested unit.** Lens, white and cyan star should grow/shrink together around the co-located Eye/Pupil centre. Any sliding star or changing white/cyan registration is a structural fail, not an art-value tune.
7. **A gate passing is not likeness evidence.** Q1.15 values of 1250/820 etc. are not proof that the native picture reads larger/smaller.

## Visual stop conditions

Stop and re-author before the full bank if any is true:

- S0 cannot be distinguished from S1 immediately at native scale on the named non-blink witnesses.
- Curious or Taunt L/R mutes do not isolate the expected eye in the picture.
- Startle's eye enlargement leads or trails its body arrival after Q3 repair.
- A reduced eye becomes a white/cyan splinter, loses its purple support, sinks into the body or disappears at an oblique view.
- An enlarged eye touches its partner, clips the body/outline, loses the pointed almond, or reads as a pasted sticker.
- White and cyan stars shift relative to each other or to their purple lens.
- Blink, gaze, roll or travel becomes unreadable because size dominates or fights the channel.
- Taunt III's face is not continuously readable throughout f0304–f0344, regardless of how large the numeric scale contrast is.
- The legacy Hit control does not restore the rejected dagger/splinter read, or current form regresses toward it.
- Any containment/camera/topology gate or attributed control fails.

## Acceptance sentence

Eye expression is complete only when the selected almond visibly becomes larger **and** smaller in shipping motion at native resolution, size-only ablations remove the read without removing gaze/blink/roll/travel, lens/white/cyan remain one registered unit, and the repaired Taunt III uses the asymmetry on a face the viewer can actually see.
