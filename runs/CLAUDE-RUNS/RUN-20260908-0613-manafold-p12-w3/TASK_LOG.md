# Task Log: RUN-20260908-0613 - [Describe objective here]

**Created:** 2026-09-08 06:13 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-0613-manafold-p12-w3/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 06:13 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-0613
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## 06:13 — lane up, base confirmed

* Lane `C:\programmieren\zencrifice\manafold-p12-w3\{zhaozhou,Upheaval}`, both
  cloned and **hard-reset onto `origin/main`**:
  * zhaozhou `7c38dfb2` "The eye TRAVEL channel, built"
  * Upheaval `aca05aa` "Wave 3 scope: remove the duplicate blink entry"
  The coordinator's warning about a `fd393952` WIP base arrived after the fetch
  had already force-updated past it; nothing was ever built from the WIP.
* Build: `bash tools/reel/build-direct.sh --output ../build-w3 {cel,mnodule,mspan}`.
  `CEL_RC=0`, `MNOD_RC=0`.
* **Baseline per-nodule gate on unmodified main (the thing I must not regress):**
  ```
  0 A alone    198  273  253   ok
  1 B alone      1  212  385   ok
  2 C alone      1    4  204   ok
  3 mid down   264  360  369   ok
  opposing verticals frame 156: A -19  B -86 (down)  C +55 (up)  sep 141
  VERTICAL REACH (bone):  A -7..+3   B -194..+65   C -131..+200
  PASS: 0 failures
  ```

### Where I am (write it down before results land)
Reading order done: WAVE3-SCOPE, D9 §2/§13, D5 §6/§7/§0-TER/§0-QUATER,
OWNER-INVENTORY B3/B6/C4, 07-MOTION-STYLE §1/§2, the nodule + span gates,
`build_hover_idle`/`build_drift`/`build_hasty`, `hover_at`, `whole_wobble`.
Next: run `mspan` for the SKIN half of nodule A's vertical reach.

## 06:45 — item 1 landed: nodule A rises

Wave 2a's own comments claimed the vertical was fixed and `mspan`'s G4 measured
172.3 mm on the SOLO DIAGNOSTIC. It was true there and it had not reached the
bank. `build_taunt3` — the clip that authors the owner's own configuration —
still carried:

    // And nodule A's rise is sideways because a vertical request moves it 3 mm
    n.az = kTaunt3ShrugMm * shrug / 1000;
    n.ay = kTaunt3ShrugMm * shrug / 4000;

so the owner's "the other two swing UP" shipped as a sidestep, behind a comment
that had become false. Fixed; the comment replaced, not deleted; two new knobs
(kTaunt3ShrugLeanPm, kTaunt3ShimmyLeanPm). `mspan` grows **G5**: ball A's
vertical SKIN reach on EVERY SHIPPED SLOT, taunt3 gated at 45 mm on reach AND
gain. taunt3 now 61.2 mm live / 5.4 mm ablated / gain 55.8. Leg `--fail-nolanes`
witnessed red, rc 1. Nodule gate re-run byte-for-byte unchanged.
Pushed `3951c74c`, verified on origin.

## 06:50 — items 2 and 3 in flight

* **Item 2, flight bob.** Established first that `hover` does NOT satisfy it:
  hover bobs 132+50 mm and stays put, `drift` travels but is D3 §7's *blown*
  glide, `hasty` is the same sentence's accelerated half. The plain travelling
  flight was genuinely absent. `build_flight`, slot 22, appended. ONE clock
  drives height, pitch (the bob's own derivative), breath (squash at the bottom
  of the arc) and a per-nodule hang-back trail.
* **Item 3, the expressiveness plate.** `manafold_express.cpp` written: reads
  the DEFORM STAGE's own output in bind-space mm for both creatures, every clip
  in both banks, median and best — deliberately not a cherry-picked pair, and
  deliberately able to refute.

### Where I am
Build 2 (cel + gates) running. NEXT: build-direct.sh gets an `mexpress` target,
then render `manafold-flight` and `manafold-taunt3` and LOOK, then the pair.

## 07:05 — the flight clip, rendered and LOOKED AT, then authored down

First render (7.0 m traverse, hasty's pulled-back camera) was WRONG and the
plate said so before any number did:

* `trajplot --bg` over all 352 frames: `cy p2p 60.8 px` — the bob is there,
  four clean bounces, not a flat line. That half worked first time.
* But `mask px count 1500 -> 4600` and `bbox height 60 -> 110 px`: travel is +x
  against a three-quarter camera, so the creature flew AT the lens and tripled
  in size. It read as a zoom-in, and it was a thumbnail throughout.

Halved the traverse to 4.4 m and gave flight its own camera (`kU02CamKFlight`
250000, between the house 360000 and the traverse 148000). Both are named
constants with the reason written beside them.

## 07:05 — a stale duplicate found on the way

`manafold_probe.cpp`'s travelling-column probe carried its OWN copy of the
renderer's staging rule (`slot_id == 1 || slot_id == 8`). Adding slot 22 made
the probe measure a stage the reel does not build — it printed "slot 22 ...
bump_ext 6" while `subject_u02_clip` staged it flat at 18. The numbers agreed
by luck. Rule now lives once, in `u02::flat_staged_slot()`, and both read it.

## 07:25 — the reframed flight, measured again

```
before:  cx p2p 269.9  cy p2p 60.8  area 1500 -> 4600 (3.07x)  bbox h  60 -> 110
after :  cx p2p 271.3  cy p2p 66.4  area 4200 -> 8700 (2.07x)  bbox h 113 -> 162
```
Same screen traverse, creature roughly twice the size, approach growth cut by a
third. Four clean bounces in `cy`. The remaining growth is inherent to +x travel
against a three-quarter camera; the clean fix is a camera YAW and it is declared
not done rather than half-done.

**Loop seam, inherited not introduced:** the last two frames of 352 are already
the loop restart, with the screen-space smear leaving a ghost at the old
position. `hasty` and `drift` have the same linear traverse and the same seam --
MANAFOLD-INDEX lists "hasty's loop seam" as a known unattempted item. Declared.

## 07:30 — item 3's verdict

`mexpress` built and run over EVERY clip in BOTH banks.
**The claim is REFUTED.** Body group against body group, Zixxtrixx 27.6% against
Manafold 6.7%. Manafold's best group (its ANTENNA, 18.7%) still loses to
Zixxtrixx's body. The one half that holds: bob, 8.4% of rest span against 6.6%.
Both measures' biases are printed inside the tool's own output so the number
cannot be quoted without them.

## 07:45 — the standing identity gates, committed so they can be re-run

`probes/zixxcrc.sh` beside the creature is hardcoded to the pass-11 lane path
and that pass's base commit, so it cannot be re-run — the same fault CLAUDE.md
records about the ground-contact probe that was written once and thrown away.
Two replacements committed, both taking `LANE` from the environment, both
naming their base commit, and both rendering **under the shipping env**
(`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`; a CRC taken without it
matches on both legs while proving nothing about the published render):

* `probes/zixxcrc-p12w3.sh` — the OTHER creature is untouched.
* `probes/manafoldcrc-p12w3.sh` — the append-only claim. Four protected
  Manafold subjects must be bit-identical and **`manafold-taunt3` must MOVE**:
  a run where the taunt matched would mean nodule A's rise never reached the
  render. That leg is the gate's own failable half.

Baseline binary built from `7c38dfb2` in its own `git worktree` at `base/`,
into `build-base/`, so it depends on nothing about my working tree.
