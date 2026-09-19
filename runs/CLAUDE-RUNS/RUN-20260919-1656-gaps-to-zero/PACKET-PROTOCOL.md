# Packet protocol — RUN-20260919-1656-gaps-to-zero

Every packet in this run reads this file first, then
`reports/HANDOVER-20260919.md` §1, §4, §5, §6, §9 and `CLAUDE.md` in the repo
root. This file is the brief's common half; your prompt carries only your lane.

## The goal and the two rules that were broken most often

Drive `python tools/budget/completion_register.py` from **61** to zero.

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
coordinator for the commit and `git merge` it into your branch.
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
| `python tools/budget/mutant_copy_drift.py` | no NEW drift you caused |
| `python tools/quartus/check_quartus17_syntax.py` | RC 0 |
| console-board lint (see handover §4, and "Lint, both halves") | waived: silent RC 0 |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tests/prod/run_console_core_smoke.ps1` | PASS, `raster pixels=1536`, `frames_admitted=1` |

Plus your own directed test for anything you built, linted with `-Wall`
explicitly (`verilate()` does not pass it). Any new guard/counter must be seen
to FIRE — by stimulus, or by a committed mutant under `tests/mutants/`.

## Traps (full list in handover §5)

* Bash is broken in this tree. **PowerShell only.**
* Backtick is PowerShell's escape character inside double quotes.
* Read/write files with `[IO.File]::ReadAllText/WriteAllText`, not
  `Get-Content | -join`. Read fully, transform, THEN open for writing.
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
