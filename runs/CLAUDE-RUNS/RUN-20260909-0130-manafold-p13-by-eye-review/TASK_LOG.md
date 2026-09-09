# Task Log: RUN-20260909-0130 - [Describe objective here]

**Created:** 2026-09-09 01:30 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-0130-manafold-p13-by-eye-review/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 01:30 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-0130
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

## By-eye review of the shipped pass-13 bank — progress log

- Lane reset to origin/main: zhaozhou `48710ba0`, Upheaval `98069b0`.
  ⚠ The bank was rendered from zhaozhou `25d86c03`; origin has since moved
  (`48710ba0` = "False comment: blown's retime block contradicted its own
  constant"). Reviewing the SHIPPED webm regardless, per brief.
- All 28 shipped webm decoded to PNG, every frame: 11,916 frames total.
  Frame counts match the bank; the encode is complete (not truncated at 12/28).
- ⚠ TRAP FOUND: the scratchpad already held frame dirs named `manafold-*`
  from an earlier lane, including a 392-frame `manafold-blown` — the BASE
  blown, next to the shipped 292-frame one. Quarantined to
  `scratchpad/STALE-DO-NOT-USE/` before any looking. Reviewing the wrong
  artefact was one keystroke away.
- Colour sanity check on the decode: saturated pixels mean RGB (181,99,109),
  R dominant with B>G = magenta-pink. Channels are NOT rotated.

### In progress when this line was written
Judging `taunt3` as a comic performance (R3). Next step after that: the
antenna nodules (item 1, asked 6x) and the rear hinge (item 2, asked 3x).
Two subagents are out on (a) channel+crackle lightning, (b) deaths/flight/
lasso/travelling-clip seams.

### Findings so far
1. Eye star is authored far TOO SMALL in a lens that is TOO ELONGATED.
   Concept sheets: lens aspect 2.6-3.2, star spans 52-60% of the lens's long
   axis, star is 27-49% of eye area. Render: aspect 4.0-4.9, star spans
   0.12-0.23 of lens length. This is a PROPORTION fault, not a registration
   fault -- which is why three passes of re-centring have not satisfied the
   owner.
2. The star still VANISHES at oblique angles (owner D9 s12.3, not fixed):
   `hover` f393 and f573 show a bare purple crescent with no star at all.
3. My own threshold-based "starless" count was WRONG -- `hover` f133/f137 do
   have stars. Caught by looking. Numbers narrowed to the proportion claim.

### Closed
Review written to `Upheaval/creature/Manafold/PASS-13-REVIEW.md`, 23 plates in
`pass13-review-plates/`. Verdict: the model is good, the bank is not yet, and
the three headline pass-13 items (taunt3 funny, the eyes, the lightning shapes)
do not land.

Two claims from helper lanes were CHECKED AND REFUTED/CORRECTED before shipping:
* "The mana FX layer is composited at QUARTER RESOLUTION" -- refuted. 0.0% of
  aligned 4x4 blocks in the mana region are constant, same as the body. The
  cited equal-value runs are VP9's own quantisation in dark areas of the
  delivered webm. Would have sent the next pass hunting a non-existent bug.
* My own "trails look fine on drift/hasty" -- corrected. I had sampled f60/f120
  only; the mist accumulates into a field by the end of both clips.

Also reconciled a QA-vs-lane disagreement: the `flight` seam frame QA calls
f350/f351 and the motion lane calls f351/f352 is THE SAME FRAME, 0- vs 1-based.
