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
