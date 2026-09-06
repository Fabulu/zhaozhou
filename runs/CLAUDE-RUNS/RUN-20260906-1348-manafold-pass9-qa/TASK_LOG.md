# Task Log: RUN-20260906-1348 - [Describe objective here]

**Created:** 2026-09-06 13:48 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-1348-manafold-pass9-qa/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 13:48 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-1348
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

## Progress Timeline (QA, pass 9)

- **13:40** Lane cloned fresh from `origin/main`: zhaozhou @ `8641fdc3`, Upheaval @ `96626d4`.
- **13:45** OWED CHECK launched: full-site `checkmedia.py` over 1,123 declared files
  / 637.7 MB, running to completion in the background. The stderr fix
  (`err = r.stderr.strip(); if r.returncode != 0 or err:`) is present in the
  shipped tool.
- **13:51** `build-direct.sh mband` -> BUILD_RC=0 (exit code read directly, not
  through a pipeline).
- **13:55** CLAIM 3 (joints on balls) tested through the REAL code path, not the
  selftest: temporarily set `kLoopArcMm` to pass 8's `{336,344,340,380,380,1270}`,
  rebuilt into a separate output dir, ran the gate. Exit 1, "joints-on-balls FAIL",
  station 586 flagged 266 mm from knuckle-Jf. Reverted; tree clean.
- **14:05** CALIBRATION: `manafold-probe` reproduces the published closure numbers
  **989 pm / 1043 pm** exactly.
- **14:15** CLAIM 4 (the closure sweep) reproduced end to end with hinge D at 2660,
  five arm lengths. ARM=640 gives **2401 pm**, the exact number in the findings.
  Tree reverted and verified clean after every experiment.

### In progress / next
- checkmedia full-site run still going.
- Next: prove checkmedia rejects a deliberately truncated file; the follow
  mechanism (claim 2); the ablation lattice (claim 7); archive decode + captions
  (claim 8); Zixxtrixx CRC (claim 9); live-clip provenance (claim 10).

- **14:11 — THE OWED CHECK IS DISCHARGED.** Full-site `checkmedia.py`:
  `1123 declared files, 1123 decoded / every declared file decodes / CHECKMEDIA_RC=0`.
  Wall clock **13:48:48 -> 14:11:14 = 22 min 26 s**, not the ">1 hr, killed twice"
  that pass 9 reported. It was almost certainly competing with a render.
- **14:12** Decoder proved failable on a 1,067,966 B truncated fixture (131x the
  8 KB size floor, so the floor cannot be what catches it): REJECTED
  "File ended prematurely". And the hole it closes is real - on that same file
  `ffmpeg` exits **0** and `ffprobe -count_frames` reports **208** frames, so the
  pre-fix checkmedia would have passed a file missing half its frames.
- **14:20** CLAIM 2 measured on 400 frames of `mist-mid` vs `mist-parked`:
  differing px/frame **1,667..18,333** (mean 8,948), max per-channel delta 116,
  max per-pixel channel-sum 238. The follow demonstrably does something; the
  PUBLISHED range 1,027-9,425 does not reproduce and there is no committed
  instrument for it.
- **14:25** No committed probe mentions the mist at all - the follow has NO gate.

- **14:18** A/B from two binaries I built (pass-8 tip `fd1e8b04` vs pass-9 tip):
  **Zixxtrixx `idle`/`walk`/`hit` sequence CRCs are bit-identical** across the
  passes. The three fogprobe legs all changed -- including `fogprobe-off`, which
  has no mana, no smear and no mist -- because the joint stations moved. Correct,
  but it invalidates every pass-8 ablation baseline.
- **14:20** Smear source diffed function by function: `smear_update`, `smear_feed`,
  `smear_composite`, `smear_speed_mul_pm`, every `kSmear*` constant and
  `kSmearPresets[]` all IDENTICAL to pass 8. One smear CALL SITE was edited
  (behaviour-preserving), so "not one code path" is false as worded while the
  substance holds.
- **14:22** CLAIM 5 settled by measurement, not arithmetic: `manafold-hinge-traj`
  from both binaries. Distal path length 2597 -> 2625 mm (+1%); whole chain
  within +/-2%. **Excursion really is preserved.**
- **14:25** Findings written to `Upheaval/creature/Manafold/PASS-9-QA.md` with
  eight evidence files in `pass9-qa-evidence/` beside the creature -- never in
  this run folder, per CLAUDE.md.

## Decisions Made
- Used content-identity rather than mtimes for the stale-clip check: mtimes are
  meaningless in a fresh clone, and a re-encode of stale FRAMES would have a
  fresh mtime and fresh bytes while still being pass-7 pixels.
- Tested the joint gate by temporarily swapping in pass-8's `kLoopArcMm` and
  rebuilding, rather than trusting `--selftest`, because the selftest
  re-implements the check instead of calling it. Reverted and verified clean.
- Did not change any creature constant, and did not publish.

## Don't Retry
- `zhao-reel-cel.exe --help` is not a flag. argv[1] is the OUTPUT DIRECTORY, so
  `--help` starts rendering the whole reel into a directory called `--help`. It
  also survived a TaskStop and had to be killed by PID.
- Counting direction reversals on the hinge CSV without a deadband: the data is
  integer millimetres and a +/-1 dither reported 182 reversals where there were 7.
