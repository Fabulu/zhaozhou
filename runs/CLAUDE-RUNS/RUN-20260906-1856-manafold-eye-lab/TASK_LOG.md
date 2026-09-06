# Task Log: RUN-20260906-1856 - [Describe objective here]

**Created:** 2026-09-06 18:56 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-1856-manafold-eye-lab/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 18:56 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-1856
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

## Field trip journal — EYE LAB (Manafold creature 02)

**Lane:** `C:\programmieren\zencrifice\manafold-eye-lab\{zhaozhou,Upheaval}`, both cloned
fresh from `origin/main` 2026-09-06. Ships nothing; output is plates + findings.

**Brief:** OWNER-DIRECTION-7 §12 (+ §5a-d, §7.5, §7.7). Four questions:
travel, the near-eye bar, blink-by-squash, star placement/size.

### Reading pass — mechanism facts established before authoring anything

1. `kBEyeL/R` are children of `kBRoot`, bind at `(kEyeXMm=400, vmm(90), ±215)`.
   A bone rotates about ITS OWN origin, so `apply_eye_shift` spins the lens on
   the spot. **Travel needs a pivot at the BODY centre**, not at the lens.
2. `kEyeShiftPivotMm = 0` is not merely unshipped, the mechanism is
   **half-written**: `make_eye_lens` sets `rs.cx = fxu(kEyeShiftPivotMm)` and
   `build_skeleton` gives the pupil bone a matching `+pivot` bind, but the EYE
   bone's own bind is never pulled inward by `pivot`. Setting the constant
   non-zero today translates the whole assembly OUTWARD and relocates no pivot.
3. `DeformSample{flatten,spread}` is ONE global sample per frame; `sub` is the
   half-tick interpolation rung, NOT a second channel. Per-part opt-in is
   `deform_role/axis/strength/center` on the RingSpec.
4. **Only `make_body` opts in.** The lens and star declare `kNone`, so the
   eyes are RIGID against the breath today — they do not follow the pulsation
   at all. §7.7's "computed against the neutral shape" fault is live, not
   hypothetical.

### Outcome

27 variants rendered at 520 frames each on the shipping rig
(`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`), judged at native 384x240.

**TRAVEL: 14 deg authored, 22 deg as the clamp.** Built as an arc about the
body's own vertical axis at `kEyeTravelPivotXMm = 33`. Lens depth 771 pm and
crown 149 mm at EVERY angle 0..45 -- the conform is free, because the body's
horizontal cross-section is a circle. At 32/45 the eyes leave the face and read
as fins. Travel IMPROVES the near eye (bar rate 55.4% -> 32.7% at 14 deg).

**THE NEAR-EYE BAR IS SOLVABLE.** Mechanism: the lens carries 180 mm of extent
along the eye's outward axis and the star carries 32 mm, so at obliquity the
lens keeps its silhouette and the star collapses. Answer: the CYAN thickens to a
solid form (46) and the WHITE slims to a true outline (12) -- the half of
`make_star`'s own recorded lesson that was never taken. Star-gone rate
11.5% -> 2.9%; cyan on screen 96 -> 215 px.

### Three faults found, and how

1. **The travel sign was inverted** -- both eyes carried inward, gap 0 mm. The
   zero sat MID-RAMP (keys 37..59); by 45 deg they had passed through each other
   and come out clean. An extremes-only gate would have passed it. Gotcha 17 on
   a new channel.
2. **The probe's own body map was hollow** -- 11 rings cannot fill 6144
   azimuth-by-height cells, and empty cells skipped vertices silently. Caught
   ONLY by the cross-check against `manafold_art.h`'s recorded 123 mm.
3. **The probe's sink threshold was wrong** -- it fired on the untouched
   control, because the lens is a dome EMBEDDED in the surface and is always
   inside the body's envelope. The shipped gate compares against REST; so does
   this one now.

### And measurement lost to looking three times

`barcensus.py` ranked `bar-domed`(62) first while its star had lost its arms;
ranked `bar-thicker` well because it removed the cyan being counted; and ranked
`bar-fat-proud` above `bar-cyan-fat` because it has no term for "do the two eyes
look like the same organ", which was the thing being decided. Each number was
correct and answered a question nobody asked. All three are on the record.

Findings: `Upheaval/creature/Manafold/EYE-LAB-FINDINGS.md`. Nothing published.
