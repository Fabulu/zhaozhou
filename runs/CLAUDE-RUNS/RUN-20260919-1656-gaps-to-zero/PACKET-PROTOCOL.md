# Packet protocol — RUN-20260919-1656-gaps-to-zero

Every packet in this run reads this file first, then
`reports/HANDOVER-20260919.md` §1, §4, §5, §6, §9 and `CLAUDE.md` in the repo
root. This file is the brief's common half; your prompt carries only your lane.

## The goal and the two rules that were broken most often

Drive `python tools/budget/completion_register.py` to zero. It opened this run
at **61**; at 2026-09-20 morning it reads **27** (14 tie-offs + 11 disconnected
+ 2 unbuilt). Quote the number you measured, never this one.

1. **Never close a gap by removing, narrowing, stubbing, tying off or
   disconnecting function.** A gap closes when a REAL producer drives a REAL
   implementation that feeds a REAL consumer, with a test that shows the value
   traversing. Moving a tie-off from one port to another is not closing it.
2. **Never compose an older version of anything.** `check_console_inventory.py`
   G3 enforces it; check it anyway whenever you compose.
3. **Do not run Quartus.** Fit at completion only; the coordinator runs it.
4. If a gap genuinely needs an OWNER decision (a law contradicted in writing, an
   ABI change, a schedule guarantee), STOP on that gap, write the decision with
   evidence and a recommendation into your FINDINGS, and move to the next gap in
   your lane. Do not decide it yourself and do not fake around it.

## ISOLATION — you work in your OWN git worktree, never the main checkout

The main checkout `C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912`
is the coordinator's. **Do not edit files in it.** Instead:

```powershell
cd C:\programmieren\zencrifice\zhaozhou-ceiling-lane-20260912
git fetch origin claude/ceiling-architecture-20260912
git worktree add -b gz/<lane> C:\programmieren\zencrifice\gz-<lane> FETCH_HEAD
cd C:\programmieren\zencrifice\gz-<lane>
. tools\env\zhao-env.ps1
```

Build in your worktree's own `build\` (`cmake --preset windows-native` from
PowerShell with the env sourced). Your scratch files go in your worktree or are
prefixed `gz-<lane>-`.

**Landing — CHANGED 2026-09-19 evening by the owner: the COORDINATOR merges. You never rebase.**

Commit on your own branch `gz/<lane>` as work closes, gate it IN YOUR WORKTREE,
and push ONLY your own branch:

```powershell
git push origin HEAD:gz/<lane>      # your branch, never the shared one, never --force
```

Do NOT fetch-and-rebase onto the shared branch, do NOT push to
`claude/ceiling-architecture-20260912`, and do NOT re-gate after other packets
land. The coordinator merges your branch, resolves conflicts, and runs the gates
once on the merged result. If you need something another packet landed, ask the
coordinator for the commit and `git merge <commit-hash>` it into your branch. Merge the HASH: in a worktree, `git fetch` may not advance the `origin/...` tracking ref, and merging that ref then says "Already up to date", which reads as good news and is not.

**AND THE SAME STALE REF LIES WHEN YOU QUERY IT, which is the half this file
missed.** The coordinator checked `git cat-file -e origin/claude/...:<path>` for
the two contact sheets the owner is waiting on and was told **NOT ON ORIGIN**
for both. They were on origin — in the commit that had just been pushed
successfully. `origin/claude/...` was sitting eight commits back at `f98f5846`
while `FETCH_HEAD` and `HEAD` agreed exactly. **A stale tracking ref reports a
pushed file as unpushed**, and unlike the merge direction it does not even say
something reassuring: it says something alarming and false, and the obvious
response is to re-push work that is already there. **Ask `FETCH_HEAD` or the
literal commit hash, never `origin/<branch>`**, in both directions.
Commit messages via `git commit -F file` (PowerShell mangles `-m` multiline).
End every commit message with:
`Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>`

## Gate list — all must hold at the commit you push

| command | must say |
|---|---|
| `python tools/budget/completion_register.py` | a number no higher than when you started (RC 1 is normal while gaps remain) |
| `python tools/quartus/check_console_inventory.py` | OK |
| `python tools/quartus/check_prod_manifest.py` | OK (it was RED at f98f5846 — texture lane owns that repair; do not make it worse) |
| `python tools/quartus/gen_prod_top.py --check` | fresh |
| `python tools/quartus/gen_console_board.py --check` | fresh (regenerate it after any core port change) |
| `python tools/budget/mutant_copy_drift.py` | no NEW drift you caused — **and RUN IT AFTER YOUR COMMIT, not before** (ruling R121). It compares COMMIT ORDER, so against a staged-but-uncommitted tree it answers about the tree *before* your work and returns a green that means nothing. It admits the other half of this itself when it skips: *"one side has uncommitted edits, so there is no commit order to read."* Every other gate here reads files and is correct on a staged tree; this one reads git history and is not. The coordinator recorded a stale green this way and only caught it after committing. |
| `python tools/quartus/check_quartus17_syntax.py` | RC 0 |
| console-board lint (see handover §4, and "Lint, both halves") | waived: silent RC 0 |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tests/prod/run_console_core_smoke.ps1` | PASS, **`raster pixels=2816`** (reference-derived by `smoke_geom_fixture_gen.cpp`; ctest `smoke_geom_fixture_fresh`), `frames_admitted=1`. **Was 2560 until 2026-09-26**: TERRAINVISIBLE made the fixture show terrain under `reports/DECISION-20260926-TERRAIN-FIXTURE.md`, the generator now models the terrain patch through `zref::terrain::tessellate`, and the tile union went 10 -> 11. |
| the same script with `-Mutant` | PASS (the slot-overflow mutant wrapper fires; it went 129 ports stale because nothing ran it) |
| `-BadVertex`, `-NoEchoArm`, `-BadTraceArm` | PASS — the three other committed controls. **Every new switch needs its own build-directory TAG** in the script, or it silently shares the plain run's object directory |
| `python tools/design/gen_shell_paired_diff.py --check` | fresh (regenerate after ANY shell port change — three packets were stopped by a stale one) |
| `python tools/budget/check_case_labels.py` | OK (ruling R62: a `"caseN:"` label must sit in block N) |
| `npm run abi:check` | clean, if you touched `spec/commands.zidl` |

Plus your own directed test for anything you built, linted with `-Wall`
explicitly (`verilate()` does not pass it). Any new guard/counter must be seen
to FIRE — by stimulus, or by a committed mutant under `tests/mutants/`.

**RULING R60: the directed tests must BUILD and RUN, not merely lint.** The
static gate set is all Python plus Verilator lint and it stayed green through a
merge that broke `cmd_exec_directed`'s braces, so the test was not running at
all. Build and run `test_cmd_exec_directed` plus your own lane's directed tests
at the commit you push, and quote their check counts.

**A port you add to a block adds a PINMISSING to every bench that instantiates
it.** `zhao_cmd_exec` gained five trace ports and `tb_cmd_exec_pair` was not
updated, so the whole verilate step failed on the merged tree — on a file the
packet never touched. Grep the tree for every instantiation of anything whose
ports you change, benches included, and connect them.

## Two rules about YOUR OWN PROCESSES AND OUTPUT, both learned the hard way on 2026-09-20

**NEVER KILL BY PROCESS NAME OR START TIME (ruling R81).** Several lanes and
the coordinator share this machine and all of them run `verilator_bin` and
`g++`. **Classify by command line and parent PID FIRST, and only ever kill your
own subtree.** A packet killed two `verilator_bin` PIDs before checking and one
of them was the coordinator's live `-NoEchoArm` smoke; the merge it was gating
then reported three failing forms that were nothing but the kills.

A killed `g++` is also the whole of the `COMPILE FAILED` mystery: no `.o`, **no
error text at all**, the same file compiles clean by hand, an identical re-run
passes. `tools/quartus/run_block_fit.ps1:955-966` already carries this incident
from 2026-09-06 — two concurrent fits, the later one dead ten minutes in with
no error in its log, *"the signature of an external Stop-Process"*, two
50-minute placements lost — and scopes its kills to its own parent PID in both
directions. Copy that, do not re-derive it.

**CAPTURE THE FULL OUTPUT, THEN FILTER THE FILE (ruling R82).** Four separate
instances in one day of throwing the evidence away at capture time, including
twice by the coordinator and once by the script that gates every merge. Write
`... 2>&1 | Out-File -Encoding utf8 <log>` and read the log. Never
`| Select-String` or `| Select-Object` on the live stream, and never
`| Out-Null`. Filtering costs nothing when the run passes and costs the entire
diagnosis when it fails — and a failing run is precisely the one you cannot go
back and re-capture.

## Traps (full list in handover §5)

* ~~Bash is broken in this tree. PowerShell only.~~ **STALE, CORRECTED 2026-09-26 (CELLCARRY).**
  Bash works. The coordinator has used it all campaign for git, greps and
  python. **What is still true is narrower and is the part that matters:**
  use PowerShell for anything that runs `run_block_fit.ps1`, `cmake
  --preset` or the smoke scripts, because the toolchain environment comes
  from `tools/env/zhao-env.ps1`; and **heredocs into `bash -c` fail on long
  or quote-heavy text** -- write the file and run it, or use the Write
  tool. A blanket "bash is broken" sent packets round a working road.
* Backtick is PowerShell's escape character inside double quotes.
* Read/write files with `[IO.File]::ReadAllText/WriteAllText`, not
  `Get-Content | -join`. Read fully, transform, THEN open for writing.
  **AND ALWAYS WITH AN ABSOLUTE PATH — this very line used to aim packets at
  the coordinator's checkout.** `[IO.File]` resolves a RELATIVE path against
  `[Environment]::CurrentDirectory`, which is the session's *startup*
  directory and **never changes when you `cd`**. Two packets wrote into
  `zhaozhou-ceiling-lane-20260912` this way on 2026-09-20 while believing they
  were in their own worktree; one caught it only because `git status` came
  back CLEAN after a write it had just seen succeed. That is the tell, and it
  is a quiet one. Use `Join-Path $PSScriptRoot ...` or a full literal path,
  and if you do hit someone else's tree, revert with `git apply --reverse`,
  **never `git checkout --`** — unstaged work has no reflog and other lanes
  had live edits in those files both times.
* A comment whose first word after `//` is `verilator` is a pragma → lint error.
* Quartus 17 rejects: inline `for (genvar`, module-scope `if` guards, implicit
  generate, `-W'(x)`. `check_quartus17_syntax.py` catches these.
* Verilated test mains call `zhao::exit_hard`, never `return`.
* ctest: env sourced; one ctest per build tree; ~0 CPU on the CHILD = blocked.
* A port on a leaf costs its WHOLE instantiation chain plus every bench.
* After a Verilator parent changes, stale partitions → delete `V<module>.dir`
  and `cmake --preset windows-native`.

## SEARCH before declaring anything absent, and NAME what you searched

Fifteen "X does not exist" claims in this repo were false, and one asserted a
PRESENCE that was false. Look in `fpga/rtl/synth/`, look for `probe`-named
files, check whether each refusal's stated cause is still true, and check what
a claim points AT, not only whether it is true.

## Reporting

Write `FINDINGS-<lane>.md` into
`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/` **in your worktree** and land
it with your last commit. It must carry: gaps closed (register before → after),
gaps refused with the exact blocker and what you searched, owner decisions
found (evidence + recommendation), false-absence claims found, instrument
defects found and whether their guard was fired. Your final message to the
coordinator: the register number at your last pushed commit, the commit hashes,
and the owner decisions, in under 40 lines.

When done: `git worktree remove C:\programmieren\zencrifice\gz-<lane>` only
after your last push succeeded, and confirm nothing of yours is still running.
