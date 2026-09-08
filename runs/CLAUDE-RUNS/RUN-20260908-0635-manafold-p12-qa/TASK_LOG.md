# Task Log: RUN-20260908-0635 - [Describe objective here]

**Created:** 2026-09-08 06:35 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-0635-manafold-p12-qa/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 06:35 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-0635
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

## QA run log — Manafold pass 12 (creature 02), 2026-09-08

Lane: `C:\programmieren\zencrifice\manafold-p12-qa\{zhaozhou,Upheaval}`, own
clones from `origin/main`. zhaozhou `7c38dfb2`, Upheaval `3b65e98`.

### Calibration
* Built `cel`, `mprobe`, `mnodule`, `mspan`, `mhinge` with
  `bash tools/reel/build-direct.sh --output <dir> <target>`; each target's own
  `$?` read, all 0 (`build-main.log`, `build-mhinge.log`).
* `manafold_shellgate.cpp` built standalone with the g++ line in its own header.
* Baseline worktree `base-8996c51b` (the commit before the hinge fix) built into
  `build-pre` for the churn A/B.

### Verified so far
* Nodule gate reproduces the published table exactly, and FAILS rc 1 when
  `nodule_aim`'s correction quaternion is neutered in the production header
  (`build-mut`) — the gate fails through the real code path, not only its flag.
* hinge_play churn A/B reproduced from two binaries built here.
* Death gate control + all three `U02_DEATH_FAIL` legs witnessed failing.
* Shell gate + `--selftest`: all five checks proved failable.

### Written this run
* `zhaozhou/tools/reel/manafold_qa_p12.cpp` (+ `mqa` target in build-direct.sh):
  the corpse across ALL FIVE deform lanes, the eye-travel drive census, and root
  continuity. Committed because a probe that is thrown away is unreproducible.

### In progress at time of writing
* subagent: site/media audit (returned).
* subagent: Zixxtrixx CRC baseline build + compare.
* subagent: mist silhouette-exclusion A/B from one binary.
