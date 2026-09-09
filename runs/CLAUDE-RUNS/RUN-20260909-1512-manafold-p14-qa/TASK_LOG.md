# Task Log: RUN-20260909-1512 - [Describe objective here]

**Created:** 2026-09-09 15:12 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-1512-manafold-p14-qa/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 15:12 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-1512
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

## QA pass 14 — progress log (written as it happens)

**Lane:** `manafold-p14-qa`, zhaozhou+Upheaval at `origin/main` (`bb3dfce6` /
`4e3d8cc`). `bb3dfce6` differs from the bank commit `dd2b8fe1` by a TASK_LOG
only — verified `git diff --stat dd2b8fe1 bb3dfce6` = 1 file, 47 insertions.
So the shipped tree IS the tree I am reading.

**Machine census before any build** (gotcha 22): no `quartus_*`, no `ffmpeg`,
no `g++`/`cc1plus`, no `zhao-reel-cel`. 11.2 GB free RAM of 23.8, 8 cores,
216 GB free on C:. One build at a time throughout.

### Done
* Item 2 (grep the switched-on state): all seven CONFIRMED. See PASS-14-QA.md §.
* `static_assert` fire test: POS_RC=0 / NEG_RC=1 with the exact message.
  Done on a COPY of the header in `qa-evidence/neg-inc/`, so the shipped tree
  was never mutated.
* Four CI gates built (`build-gates`, all four RC=0) and the CI harness re-run
  verbatim: 4 green, 3 red legs proven, `manafold-meshcheck` has NO red leg.
* Build A (shipped tree) `build-qa`, BUILD_RC=0, md5 `be5286a0…`.
* Build B (baseline `d171a608`, pre-pass-14, `kBodySegments=16`) `build-base`,
  BUILD_RC=0, md5 `fc4b2313…`.

### In flight
* `bitident.py` zixxtrixx, base vs shipped, jobs=1 → `bitident-zixx.log`.

### Next (so it is not lost when the above returns)
1. Reproduce REEL's 9 mm / 3 mm byte-identity.
2. Re-run holdmeter's calibration and sweep BELOW 1.2 — the published band's
   lower edge was never tested.
3. The ring-axis leg with the tables actually authored (the experiment nobody
   has run).
4. Prove `manafold-meshcheck` failable.

### A trap I nearly fell into, recorded
The background harness reported the cel build "completed exit code 0" while
`bin/` was EMPTY and the log ended at `LD zhao-reel-cel` with no `BUILD_RC`
line — the exact costume of gotcha §13 (a link that failed after printing LD).
It was not that. `nohup … &` returned immediately and the real `g++` was still
running; `Get-CimInstance Win32_Process` found it alive, by command line, in
this lane. **Gotcha §20a, from the direction nobody writes down: the job
outlived its launching tool call, and the harness's exit code was the
launcher's, not the build's.** The log's own `BUILD_RC=0` appeared a minute
later. Two lessons hold at once: read the real exit code, and do not read a
missing one as a failure.

## CLOSING STATE

**Deliverable:** `Upheaval/creature/Manafold/PASS-14-QA.md` + `pass14-qa-evidence/`
(Upheaval `e41745a`). Seven refutations, thirteen confirmations, eight things I
could not check, named.

### The headline
**The remaining polygonal read is the SQUASH, not the mesh — on either axis.**
Ran the ring leg nobody had run (both `[kBodyRings]` tables are constant, so the
"authoring" that blocked it is twenty-one 1000s and twenty-one 0s). Rings 11→21
leaves the chorded outline pixel-for-pixel where it was. Then ablated
`kCompressAmpPm` 10× down with the mesh untouched, and the ball is a circle.
Answers the open question in `PASS-14-LANES-PAUSED.md`: **one fault, and it is
neither candidate.** The claim it refutes is live on the owner's page.

Second: **`manafold-meshcheck` catches the zero-fill bowl** — `FAIL 640
degenerate (zero-area) triangles`, rc=1 — so "nothing said a word" is wrong, and
that gate is the one CI declares with **no red leg**. Its first witnessed failure
is mine.

### Builds and jobs (all `--clean`, every `BUILD_RC` read from its own log)
`build-gates` (4 targets) · `build-qa` (shipped) · `build-base` (`d171a608`
worktree) · `build-mmprobe` · `build-rings21` · `build-lowamp` · `build-poscontrol`
(×2). **One build at a time; never more than two single-threaded renderers; no
encoder ever started.** No `quartus_*` alive at any point.

### Process census at close
Verified: no `zhao-reel-cel`, no `g++`/`cc1plus`, no `python` job of mine, no
`ffmpeg` (none was ever started). No polling waiters were spawned.

### Tree hygiene
Five source mutations, each for one build, each reverted and the revert verified
by `diff -q` against a pre-edit `.pristine` copy **and** `git status --porcelain
tools/reel/` returning empty. zhaozhou's working tree carries only this run
folder.

### Four of my own instruments were wrong before they were right
1. A build I read as a failed link had simply not finished — gotcha §13's costume
   worn by §20a. The harness's "exit 0" was the launcher's, not the build's.
2. My first `meshcheck` fire test compiled against the **unmutated** header (for
   a quoted `#include`, GCC searches the including file's directory first) and
   printed CLEAN. Caught because the triangle count had not moved.
3. My first bit-identity positive control was **inert** — I mutated `cam_pitch`'s
   default `bias_x`, and the main render loop passes it explicitly. It came back
   green. **A positive control needs a positive control.**
4. I killed my own PowerShell process: `Where-Object { $_.CommandLine -like
   '*hold-work*' }` matches the process running the filter. The sibling
   `bitident` run was verified untouched afterwards. **"Identify by command line,
   kill by PID" needs "and exclude yourself" written after it.**

Three of those four would have produced a confident, wrong line in the report.

### Lane
**Deletable once this is pushed.** `git worktree remove base-d171a608` first.
Build trees and `.rgb` intermediates are not committed and must not be; `git
clean -fd` will not reach the frames — only `-fdX` does.
