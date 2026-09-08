# Task Log: RUN-20260908-2201 - [Describe objective here]

**Created:** 2026-09-08 22:01 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-2201-manafold-p13-qa/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 22:01 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-2201
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

## QA lane, Manafold pass 13 — running log

**Lane:** `C:\programmieren\zencrifice\manafold-p13-qa\{zhaozhou,Upheaval}`,
zhaozhou `25d86c03`, Upheaval `d4568f6`. Base for every A/B: zhaozhou
`ffae071e` in a `git worktree` at `qa-work/zz-base`.

**Builds (all `--clean`, exit codes read from the BUILD, not a pipeline):**
`qa-work/bh` (HEAD) and `qa-work/bb` (BASE), targets cel/mprobe/mqa/mspan/
mnodule, every RC=0 (`qa-work/build_all.sh`). `shellgate.exe` built by hand
(it has no `build-direct.sh` target, by declared design).

**Frames read only through `tools/reel/rgbframe.py`.** Every render under
`ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`.

Where I was when the long jobs went out: mqa/mprobe/mspan/mnodule reproduced on
both binaries; star-offset measured base vs head; shell A/B measured; drift
wrapseam reproduced; Zixxtrixx 1896-frame leg proven with a positive control.
Outstanding when `bitident.py` (71 subjects) and the blown/lasso luminance
render were launched: the flag-off mutant, the luminance measurement, and
writing `PASS-13-QA.md`.

## Closing state

`PASS-13-QA.md` and `pass13-qa-evidence/` are committed and pushed to
Upheaval `main` (`ab28862`, `e3aca3d`, `e64769e`, after a rebase onto the
coordinator's `a2bf474`). This run log is zhaozhou `fd600af0`.

The coordinator's priority item — "the wrap fix is on but the ghost is still
there" — was adjudicated with a MUTANT binary at the same commit carrying only
the four opt-in lines commented out. Every figure the coordinator measured
reproduces to the pixel on my flag-ON build, so the bank has the fix on and the
metric is sound; the conclusion was drawn one frame late (f351 is the next
loop's first frame; the fabricated frame is f350, where the ghost falls
188 → 151 and the frame's own motion falls 7.95x → 0.90x of the clip's interior
median). All four travelling clips confirmed, each changing exactly two frames.

Left running when this log was written: `bitident.py` over all 71
`zixxtrixx-*` subjects, BASE `ffae071e` vs HEAD `25d86c03`, shipping env — the
69/71 / 15,634-frame leg. The 1896-frame leg is already proven independently
(four subjects, per-frame sha256, with a positive control) and is in the
report.
