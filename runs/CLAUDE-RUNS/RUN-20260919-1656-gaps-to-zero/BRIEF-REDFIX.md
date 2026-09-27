# REDFIX — three inherited reds with named fixes, and two instruments that cannot fire

**Branch `gz/redfix`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**This is NOT gap work and it must not displace any.** The register is at 2
(`I34`, `I55`) and `SWAPCLOSE` is live on `I55`. You exist because **three reds
were proven inherited by measurement, each has a named repair, and NOBODY OWNS
THEM** — so unless someone is named, nothing in the tree will run two of them
again. *"INHERITED is the word that makes a red nobody's job."*

Read `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-doorcost.md` **§7
and §7.1** and `FINDINGS-MATCARRY.md`'s control-form table first. **Both
attributions were settled by a base-commit run in a throwaway worktree, not by
inference.** Do not re-litigate them; verify cheaply and proceed.

## RED 1 — PACKET-D'S ORPHANED BIT 410, AND SIX REQUIRED TESTS RED SINCE 2026-09-26

**Commit `ceba0bfe` moved `PRETEX_EARLYZ_KEY_LO` from 410 to 411 and left four
field constants behind.** So `PAYLOAD_HI = 409` while `KEY_LO = 411`, and **BIT
410 BELONGS TO NEITHER.**

* **Six of the thirteen required Packet-D tests have been red since 2026-09-26.**
* **Proven inherited:** re-measured at base in a throwaway worktree —
  `BASE_PD_FULL_RC = 2`, **identical cycle 4219.**
* **Exact replacements are in FINDINGS-doorcost §7.1.** Use them; they were
  derived from the layout, not guessed.

**AND THE INSTRUMENT HALF, WHICH IS THE PART THAT MATTERS MOST: the package's own
self-check compares each constant TO ITS OWN LITERAL, so it CAN NEVER FIRE.**
That is why a four-constant desync survived a week with a checker sitting beside
it. **Repairing the constants without repairing the checker leaves the next
desync exactly as invisible.**

**So you owe two things here, and the second is the deliverable:**

1. the constants corrected, and the six tests green;
2. **a self-check that actually relates the fields to each other** — that every
   bit of the record belongs to exactly one field, that the fields tile the width
   with no hole and no overlap — **and a demonstration that it FIRES.** Move a
   constant by one, watch it go red, put it back. A **committed mutant** is the
   right home for that demonstration if legal stimulus cannot reach it.

## RED 2 — `-UntexMutant`, ATTRIBUTED AND UNOWNED

**It is FATAL, and it is not MATCARRY's.** Measured: it fails **identically at
base `71122893`** — same counts, same message, **same simulation timestamp
`10298916000`** — with the assertion's line number shifted by exactly the lines
MATCARRY's layer-E block added above it. That is attribution by measurement, and
it doubles as proof MATCARRY's fixture change is behaviourally invisible in that
form.

**It is OUTSIDE the five-form merge gate**, which is precisely why it needs an
owner.

**The repair is one expression, and its shape is prescribed:** scope the
assertion to **the replay arm's own submitted count**, not the global
`geom_clip_submitted_o`. **DO NOT relax it to a bound.** The clause is **a real
property of R197's door**; the *counter* is what is wrong. A bound would convert
a true property into a weaker one that can no longer catch the thing it exists
for — this campaign's signature mistake, in the flattering direction.

## RED 3 — THE BOARD LINT BASELINE IS ITSELF STALE

`run_console_board_lint.ps1` is red at **291 warnings**, and **zero of them name
anything DOORCOST introduced** — but **the protocol's 262 baseline is stale**, so
the gate cannot currently distinguish a new warning from an old one.

**Re-baseline it HONESTLY, which means classify before you accept.** Produce the
count, group the warnings by kind and directory, and say which are inherited.
**An accepted red you have not read is how the hand list rotted in the first
place.** If any warning names something a recent packet added, that is a finding,
not a baseline.

**Note the known asymmetry**: `check_console_closure_lint.py` **deduplicates its
source list** while `run_console_board_lint.ps1` does **not**, so a duplicated
line in `fit_targets.yml` makes the first say OK twice while the second counts it
twice. Check for that before you attribute anything to the RTL.

## The fences

* **STAY OUT OF GEOM.** `SWAPCLOSE` owns `zhao_geom_*`, the console's geometry
  plumbing, and `TriangleDescriptor`/`ProjectedVertex`. You own
  `fpga/rtl/texture/`'s Packet-D package, the one console assertion in RED 2, and
  the board lint baseline. **If a fix seems to need a GEOM edit, STOP and say so.**
* **`fpga/rtl/prod/zhao_console_core.sv` is SHARED.** **Stage the HUNK, not the
  file** — `git add <file>` stages work you did not write, and `git checkout --`
  DISCARDS another packet's uncommitted work **with no reflog**.
* **Do not baseline a red to make a gate green.** Every red here is to be
  *repaired* or *classified*, never accepted silently. `gate_sweep.py --update`
  is not yours to run.
* **Do not write a test that asserts the bug.** After you fix bit 410 there is no
  desync to catch; assert the correct tiling and keep the positive control
  separate.
* **A committed mutant is a COPY and copies go stale** — if you cut one, note
  that `tools/rtl/gen_paramarena_mutants.py` is the pattern for a mutant
  refreshed by a committed COMMAND rather than a three-way merge.
* **No Quartus fit.** A leaf `-MapOnly` is available if you genuinely need one,
  device named; **this packet should not need any.**

## Evidence bar

* **The six Packet-D tests green**, and the count stated before and after.
* **The new self-check SEEN TO FIRE** on a deliberately moved constant. **A
  checker that has not been shown to fire has not been tested** — and the one you
  are replacing is a live example of exactly that, sitting beside the bug it
  could not see.
* **`-UntexMutant` green, with its clause still a real property** — show that the
  scoped counter still refuses the fault the global one was meant to catch.
* **The board lint count classified**, not merely re-baselined.
* **Prove every counter you quote**, and **check any control you add CAN FAIL.**
  Fourteen packets this week found their own controls vacuous, including in the
  last two merges: a positive control that had **silently become a no-op**
  because arithmetic moved one file away; a test printing a **hardcoded
  `0 failures`**; an anti-vacuity check that passed on a **constant `0xFF`**; and
  a console assertion whose own comment admitted **it could not fail.**
* **Re-run `completion_register.py` BARE** and confirm it still reads **2** —
  **you must not move it.** If your work appears to lower it, that is a bug in
  your work or in the register, not progress.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code, not a
  pipeline's.** A **PowerShell exception leaves `$LASTEXITCODE` STALE.**
* **RUN GATE 31** (`check_console_closure_lint.py`) if you touch any port.
* **`-SkipVerilate` is wrong for a control form that builds its own model**, and
  **a switch passed as a quoted string becomes POSITIONAL** — splat a hashtable.
  Both cost me a run today.
* **`UNUSEDSIGNAL` is waived across whole directories** — a dead wire raises
  nothing; count readers by hand.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** and silently eat your
  words — use `git commit -F <file>`.
* **`git show` hands back INDEX content at LF** while working copies vary.
* **Three concurrent smoke forms is this box's ceiling**, `SWAPCLOSE` is live,
  **and other repositories' builds and suites run on this machine.** Classify by
  command line **and parent PID**; kill nothing you did not start. **Read CPU as
  a RATE** — age and CPU together — or a just-spawned process reads as a wedge.
* **One `ctest` at a time per build tree**, and clean `Testing/Temporary` debris
  after killing one or the next run wedges at startup.

## Deliverable

1. **The register, measured BARE, before and after — it must still read 2.**
2. **Bit 410 fixed and the six Packet-D tests green**, counts stated.
3. **The new layout self-check, SEEN TO FIRE.**
4. **`-UntexMutant` green with its clause intact**, and a demonstration the
   scoped counter still catches the real fault.
5. **The board lint count classified**, with inherited versus new separated.
6. **Every claim in this brief or the findings you found FALSE.** The last three
   packets each found five in what they were handed; the briefs are the weak link
   and this one is no better by default.
7. **What you refused, and anything you got wrong and caught yourself.**
8. **Anything you found that belongs to `SWAPCLOSE`** — report it, do not touch it.
9. Branch and commit hash. **Push `gz/redfix` only.** Never `--force`.
