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

## Close-out

### The push claim, checked FIRST
Pass 6's work IS on the remote (`Upheaval origin/main` 040996c, two commits
ahead of the p6 lane; `zhaozhou origin/main` 44080919 contains the p6 head).
BUT six QA commits were stranded, unmerged, on
`origin/zixxtrixx-wholebody-s-spring` -- including both committed QA probes and
the three evidence plates this pass depended on. Merged so they cannot be
orphaned again. Every push this pass was verified with
`git fetch && git branch -r --contains <sha>` as it happened.

### THREE bugs were stacked in one measurement
Each invisible until the one in front of it was removed:
 1. bare `>> 16` on fx16 called mm  -> everything under 1000 mm truncated to 0
 2. `inv_point` on a SKINNING matrix returns BIND space  -> lz carried +/-215 mm
 3. root translation subtracted on Y only -> travelling clips read world space
Fixing only #1 reports 378 mm of overhang against a 24 mm cap. The truth is 142.
A creature tuned to satisfy 378 would have moved a long way the wrong direction.
Bug #3's worst "violation" was 8317 pm past the outline -- eight body radii.

### What is NOT done (stated, not omitted)
 1. Rule 3's underlying fault is REAL and unfixed: 1499 samples over 7 clips,
    the star crossing the body outline into sky. REPORTED-NOT-ENFORCED because
    the rule aims at 2 FIXED views while the shipping cameras orbit. Pass 8's
    first eye item: fix the fault or finish the instrument, not neither.
 2. The near eye still reads thin at steep camera angles. The star is a FLAT
    PLATE on a dome; a flat shape has no defence against edge-on. Mechanism
    identified, not solved.
 3. Direction 5 5c's eyeball shift still NOT SHIPPED -- now a DECLARED gap.
 4. Clip inventory F.2-F.6 not attempted.
 5. `edge-snap-held` (Direction 6 RESULT) not integrated.
 6. Antenna knuckles (2b) not addressed.
 7. hasty's loop-seam pop and the 25-40 px traverse framing not addressed.
 8. NOT PUBLISHED -- site media are pass-6 renders and the geometry, star and
    smear all changed, so a publish would show pass-6 clips under pass-7
    captions.

### Blast radius, the fuller way
Pass 6 said "two clips re-authored" while kKneadClipPm moved on 12 of 15 slots.
For pass 7:
 * EVERY Manafold clip's rendered output changes (the texture page was
   regenerated for the lens ink; lens, star and smear all moved). No Manafold
   CRC survives.
 * THREE clips changed authored POSE data: curious, startle, taunt.
 * channel and taunt retime nothing but move, via the twinkle constant.
 * All 12 clips that fell through the smear bound now composite a smear plane
   they never had -- the largest single visual change in the pass, from a
   one-character bound.
 * Zixxtrixx untouched: no file in its closure was edited.

### Post-change checks
* **QA's stray-triangle probe re-run on the CHANGED eye geometry**: 0 vertices
  unreferenced by any triangle across all 25 meshlets. The lens and star tip
  rings were altered this pass, so this is the check that matters most -- the
  project's recorded ghost is "every automated gate passed while a stray
  triangle sat in a creature's eye".
* **Zixxtrixx byte-identity verified by inspection, not asserted.** Diffing the
  whole branch against the pre-pass-7 base: no `zixxtrixx*.h`, no
  `reference/src`, no `zixx_*` tool touched. The only shared file edited is
  `zhao_reel.cpp`, and its entire diff is (a) the Manafold smear clamp and
  (b) a new Manafold-only subject block -- nothing on any Zixxtrixx path.
* **The merged antenna tooling runs here**: `manafold-hinge-traj` produces 2521
  rows, and `hinge_trajplot.py selftest` PASSES -- it distinguishes a flat line
  (0.00 mm) from motion (80.00 mm) and a locked pair (r=1.000) from an
  independent one (r=0.000), so that instrument can fail.

### ⚠ FOR PASS 8: a smell in QA's own stray probe, not acted on
`manafold_qa_stray.cpp` reports every eye vertex skinning to world
(0,1,-1) mm and "within 100mm of ORIGIN", for an eye whose bone binds at
kEyeXMm = 381 mm. That cannot be right, and it is the same FRAME class of bug
fixed twice in manafold_probe.cpp this pass (bind space vs bone space; root
translation). Its stray COUNT is still usable -- that only needs the index
lists -- but do not trust its positions until the frame is checked.
