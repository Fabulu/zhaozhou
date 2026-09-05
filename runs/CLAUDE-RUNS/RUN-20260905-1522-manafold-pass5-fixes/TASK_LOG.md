# Task Log: RUN-20260905-1522 - [Describe objective here]

**Created:** 2026-09-05 15:22 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260905-1522-manafold-pass5-fixes/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-05 15:22 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260905-1522
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

## Pass 5 — bounded fixes after the pass-4 gates

**Brief:** QA.md (RUN-20260905-0933) is the authoritative verdict; where it
refutes REVIEW.md, QA wins. Items 2-10 of QA's ranked list are in scope.

### ITEM 1 IS PARKED, DELIBERATELY — the owner is deciding it

Shape legibility at native is a collision between two owner directions
("make the particles bigger", twice, vs "fold into recognisable shapes").
Both gates agree the cause is the 14-20 px mote halo drawing a 56 px
stencil with stations ~10 px apart. **No knob on that axis was touched in
this pass**: motes not shrunk, stencil not enlarged, camera not moved,
pocket not widened. It waits for the owner's call.

### Work items (QA's ranked order)
- [ ] 2. mana covers the antenna (taunt2 worst, rest, damage) — judge by looking
- [ ] 3. hit/startle never knead; curious 18/180 — first-cycle compression
- [ ] 4. build integrity: commit page + hard error + .gitignore + comments
- [ ] 5. eye gate re-baselined on LENS geometry; star arm NOT touched
- [ ] 6. kKneadClipPm[14] dead knob — guard `< 14` -> `< 15`
- [ ] 7. ablation gate retired -> U02_FOLD_FREEZE (bones animate, field frozen)
- [ ] 8. white crescent -> ring
- [ ] 9. hover loop seam (orbit wrap quantisation + release feed fade)
- [ ] 10. renders/manafold-inspect.png poster shipped
- [ ] Zixxtrixx byte-identity from pristine baseline (walk/idle/damage CRCs)

## Results (as measured, own builds, own renders)

**Calibration:** own baseline build of cc5ff8d9 (pristine worktree /c/mf5wt)
reproduced manafold-hover `0x5B44FCF2` — the shipped value both pass-4 gates
reported. Baseline zixxtrixx CRCs (ZIXX_EXP=celmain): walk `0x81155EDB`,
idle `0x118660EF`, damage `0x1EA126EE` (walk/idle equal QA's own).

* **Item 2 (mana buries antenna):** drag clamped by magnitude (kDragMaxMm
  380), rest 700->500 / taunt2 500->380 knead gains, fold trail fed by halo
  colour at CORE footprint. Looked at: taunt2 f80/f120/f166/f200, rest
  f335, damage f150 — the loop's ink and tube read through the mana on all
  of them. hasty f131 lagging-gap read INTACT (looked at against the
  pass-4 shipped frame). A 760 feed ladder rung was rendered and REJECTED
  (more feed whitens — the ramp whitens with intensity).
* **Item 3 (knead coverage):** fold_phase first-cycle compression. Own
  U02_FOLD_DEBUG renders: hit 0 -> 36 knead frames, startle 0 -> 48,
  curious 18 -> 51. Clips whose first cycle already fit are untouched.
* **Item 4 (build integrity):** page committed (819 KB, deterministic),
  __has_include guard removed (hard compile error), build-direct.sh checks
  reel+cel with a message naming the generator, .gitignore:91 stale line
  removed, both false comments replaced. Clean-worktree proof after commit:
  see below.
* **Item 5 (eye):** probe reports per named part — lens 1228 pm / 102 mm,
  star 1269 / 121, white ring 1302 / 135 (ring is prouder after item 8) —
  and GATES the lens at >= 1215 pm. Star arm untouched. More stand-off NOT
  taken: raising the dome (kEyeDeepMm 90) would bury the star (blade spans
  75..101 mm) and re-break containment; kEyeXMm risks the three-quarter
  silhouette; QA made it conditional and the lens never regressed.
* **Item 6 (dead knob):** `slot < 14` -> `slot < 15`; damage now runs its
  authored 250 (2.8x calmer — visible in the damage f150 look).
* **Item 7 (ablation gate):** retired; the code is the proof (stated at the
  old gate's site); U02_FOLD_FREEZE=1 is the can-fail render gate — proven
  discriminating: frozen taunt2 `0xD03110F6` vs live `0x59ADB544`, and the
  frozen render shows the mana holding the rest layout while the antenna
  gestures (looked at, f166).
* **Item 8 (white crescent):** tube 15 -> 22 mm, offset 52 -> 60 mm. Front
  view: near-complete ring on both eyes; far eye rings fully at
  three-quarter (looked at, 4x). Probe gates all OK.
* **Item 9 (loop seam):** orbit/wander frequencies quantised to whole
  cycles per clip; smear feed fades with the RELEASE amp. Hover seam
  4.07 -> 1.89, ratio 5.62 -> 2.39 (house class: idle 2.13). Instrument
  (evidence/seam.py) calibrated: reproduces the reviewer's 4.07 and 3.54
  on the baseline frames exactly.
* **Item 1: PARKED, deliberately** — owner decision between "bigger
  particles" and "nameable shapes". No knob on that axis touched.
