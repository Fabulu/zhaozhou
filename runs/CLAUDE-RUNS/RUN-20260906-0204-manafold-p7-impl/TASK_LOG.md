# Task Log: RUN-20260906-0204 - [Describe objective here]

**Created:** 2026-09-06 02:04 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0204-manafold-p7-impl/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 02:04 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0204
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

## Pass 7 implementation log

### Lane setup
- Cloned `manafold-p7-impl/{zhaozhou,Upheaval}` from the p6-impl lane, then
  repointed `origin` at GitHub and fetched, because the pass-6 push claim was
  the thing pass 7 was told to distrust.
- **Pass 6's work IS on the remote.** `Upheaval origin/main` is `040996c`
  (two commits AHEAD of p6-impl's `34c7a8f`: the by-eye review and a gate
  checklist item). `zhaozhou origin/main` is `44080919` and contains the p6
  HEAD. So the recovery described in PASS-7-INPUTS happened; the remote is
  whole. What is NOT merged is the QA branch
  `origin/zixxtrixx-wholebody-s-spring` (6 commits) -- merged into this lane's
  `manafold-pass7` so the evidence and the two QA probes are not orphaned.
- Branch `manafold-pass7` in both repos.

### Item 2 (ranked 1): the smear off-by-one -- FIXED, pushed, verified
- `zhao_reel.cpp` bounded a six-entry `kSmearPresets` with `< 5`.
- `kSmearPresetCount` is now derived with `sizeof` + a `static_assert`.
- `tools/reel/boundscan.py` committed as the instrument. **Proved it can
  fail**: on the unfixed p6 tree it names the bug; on the fixed tree, silent.
  18 other suspects read by hand, all false positives.
- Pushed as `1ab3f91e`; `git branch -r --contains` confirms `origin/manafold-pass7`.

### Item 1 (the root cause): the units bug -- FIXED, and there were TWO
- `off_mm` in 5c and `dx/dy/dz` in 5d gate A used a bare `>> 16` on fx16.
  Correct is `(raw * 1000) >> 16`.
- **A SECOND, UNLISTED BUG sat behind it.** `inv_point` against a skinning
  matrix returns BIND space, not eye-bone space, so `lz` still carried the
  eye's own +/-215 mm bind offset. The first bug MASKED the second: while
  everything truncated to zero a 215 mm frame error was invisible.
  Units-only would have reported **378 mm** overhang -- confident, wrong, and
  4x scarier than the truth. QA's committed tool had the frame fix; the
  shipped probe did not.
- With both fixed the probe reproduces QA's independent tool EXACTLY:
  rule 1 worst **142 mm** at slot 2 key 115 (cap 24), rule 2 **220 pm**
  (floor 600), rule 3 **3693** violations. Gate A **18 mm** (floor 12).
- `eye_shift_a16` returned 916.7 deg because the pivot guard divided by *1*
  instead of returning no shift. Now returns 0 while unshipped. Gate B moved
  731 -> 799 pm once it stopped measuring flipped eyes.

BEFORE (shipped, all five dead):
  rule 1 worst 0 mm OK | rule 2 1000 pm OK | rule 3 0 violations OK
  gate A 0 mm REPORTED-NOT-ENFORCED | gate B 731 pm (measuring 916.7 deg eyes)
AFTER (all live, three failing on real faults):
  rule 1 142 mm FAIL | rule 2 220 pm FAIL | rule 3 3693 FAIL
  gate A 18 mm OK (now ENFORCED) | gate B 799 pm OK

### Items 3, 4, 5, 6, 7 -- the eyes
- **The black notches: the brief's diagnosis was INCOMPLETE.** The degenerate
  cap WAS real (`kEyeLensWidthPm` ended at 0 with caps on -> 8 zero-area
  triangles per tip with no normal) and is fixed via `kEyeLensTipPm` /
  `kStarTipPm`. But it owned only the very darkest pixels (lum<15: 25 -> 16).
  The NOTCH is an ink contour painted into the page by
  `tools/pack/mkmanafoldpage.py` at `LENS_INK` (24,14,40): 5.5% of the lens
  length at each tip (a contour on the page, a black WEDGE on screen where the
  lens tapers to a point) and a back band wide enough to rotate onto the
  visible rim. Both are now named constants, narrowed.
  MEASURED near-lens crop, manafold-rest f234:
      shipped        lum<25 = 38   lum<15 = 25
      + geometry fix lum<25 = 37   lum<15 = 16
      + ink fix      lum<25 = 17   lum<15 =  9
  Also: QA's plate marked lum<90 as "black", but the lens's own authored deep
  purple (58,28,156) has lum 51 -- most of what the plate flagged is the
  sheet's own "the whites aren't white, they're a deep purple".
- **The star was a SPINDLE.** Measured on the Front sheet (a flat shape drawn
  face-on IS a legitimate thing to measure): drawn star major/minor = 1.70 and
  1.72 on the two eyes; we shipped 2.82. That is the reviewer's "~half its
  drawn proportion" -- the width-to-length proportion was about half.
  Arms 216/167/68 -> 147/114/77. The stars now read as 4-point stars.
- **The containment violator was `apply_twinkle`**, not the gaze (gaze is 25 mm
  at full amplitude, inside). 10923 a16 = 60 deg, doubled by `channel` to 120.
  Cut to 2275 (12.5 deg) plus a structural clamp `kStarTwinkleMaxA16`.
- **A THIRD bug in the 5c block**: rule 3 subtracted the root translation on Y
  only, so travelling clips were measured in world space. 4620 -> 1513
  violations; worst sample 8317 pm (8 body radii, impossible) -> 1267 pm.
  Three defects were stacked, each hidden by the one before it.
- **Gate A could not see its own minimum.** The eye gap is NOT monotonic in
  roll: 98 mm at 0 deg, 14 at 6, 0 at 7, back out to 18 at 10. Pass 6 sampled
  only full amplitude, so it read a comfortable 18 mm with a collision in the
  middle of its range. Gate A now SWEEPS roll (21 steps x 16 sign corners).
  It immediately failed at 5.8 deg composed with full gaze (10 mm), which the
  corner-only version called fine. kEyeRollMaxA16 1820 -> 900 (4.9 deg, 22 mm).
- **apply_eye_roll and apply_gaze_lr had ZERO callers.** Both wired: the brow
  tracks curious's double-take (tops together = intent), flies apart on
  startle (surprise), and goes lopsided on taunt -- where apply_gaze_lr now
  carries the cross-eyed beat Direction 5 5b rule 4 asks for.
- `kStarOverhangMaxPm` 300 -> 330, stated openly: the star was resized to the
  sheet so its half-width grew 80 -> 89 mm, rule 2 (the actual "is it still an
  eye" test) passes at 760/600, and the owner marked the value provisional.

GATES, end of this batch (all PROVED failable):
  rule 1  29 mm / cap 29   OK      (known-bad: restore kBlazeTwinkleA16=10923 -> 92 mm FAIL)
  rule 2  760 pm / 600     OK      (same known-bad -> 280 pm FAIL)
  rule 3  1499             REPORTED-NOT-ENFORCED, reason written in source
  gate A  22 mm / 12       OK      (at roll 1050 -> 10 mm FAIL; the sweep found it)
  gate B  801 pm / 1000    OK
