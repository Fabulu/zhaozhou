# NAVSERVICE — build the CPU navigation query the owner just commissioned

**Branch `gz/navservice`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Read `reports/OWNER-DECISION-20260926-I34-NAV.md` FIRST, in full.** It is an
OWNER decision taken 2026-09-26, it **supersedes part of the vacation
directive**, and it is your specification. This brief does not replace it.

**This is not a hardware packet.** It is CPU/runtime work, and it may span
repositories.

## What was decided, in one paragraph

Navigation truth and its query service belong to **SW.CPUCOLL / the CPU
simulation runtime**, as the terrain ownership contract already said. **The FPGA
is no longer required to publish a nav lattice into SDRAM** — `COMPOSED_NAV` is
struck, and neither its original range nor DECISION RECORD 1's relocation is
live. **`FIELD.WRITE.NAV` and its behaviour are PRESERVED**: this is an
ownership decision, **not** permission to delete navigation, ignore nav writes,
change their meaning, or cut supported field capacity.

## THE JOB

**Implement the smallest useful PRODUCTION navigation query** against actual
runtime terrain and active-field state:

* **hard passability**, and
* **composed movement cost**, for a location/cell.

**Expose it through the runtime interface that Form simulation and game AI can
actually call.** The owner is explicit: *"A new reference-only helper, debug
counter, or testbench-only read is not completion."*

**You do NOT need** a complete pathfinding framework, tactical AI, army
formations, or the full game engine. **You need the real cost/passability
service those systems will consume.**

## LOOK BEFORE YOU BUILD — and not only in this repository

The owner's instruction, quoted because it is easy to skip: *"Reuse any
appropriate implementation already present in the game/language repositories;
**absence from this console repository is not proof of absence from the entire
project.**"*

**This campaign's single most repeated failure is the false absence**, and the
last four packets each found one. A grep confined to `zhaozhou/` is exactly the
truncation that produces it. **Search `Upheaval/` and `nanquan/` too**, and the
runtime trees, before concluding anything does not exist.

**What is known here:** `compose_nav` exists at `zref_fieldir.hpp:121` —
command-ordered saturating addition of signed cost deltas, floored at zero
afterwards. `spec/terrain_rules.md:505-507` specifies the CPU mirror as
**re-derivation**. SW.CPUCOLL is `maturity: SPECIFIED` with an **empty**
`maturity_log`, living at `runtime/mister/.gitkeep`. **What is missing is the
runtime query consuming that calculation, not the definition of navigation
cost.**

## SEMANTICS — preserve, do not re-invent

* **Command order, signed Q16.16 contributions, saturating accumulation, the
  final nonnegative cost floor, field coverage, and optional-output presence.**
  **An absent output is not a write of zero.**
* **Navigation cost must NEVER make hard-blocked terrain passable.** Preserve
  the canonical terrain/lattice rules. **Do not substitute render LOD** and do
  not invent a different off-lattice field evaluation.
* **The query must use a coherent terrain/field/tick generation.** Field
  creation, evolution, expiry and removal must affect the answer correctly.
  **Do not cache animated fields forever under a dirty-bit rule that only
  notices stamps.**
* **Use shared semantics, not a separately hand-transcribed approximation.** A
  second transcription of ratified arithmetic is the exact failure
  `CLAUDE.md`'s "read the SIBLING contract" chapter is about, and this tree has
  shipped it twice.
* Missing sampling, cache and interface details are **yours to resolve under
  the existing delegation** — decide, document, proceed.

## NO SECOND WRITER

**The CPU derives its navigation state from its own canonical terrain and the
same accepted field commands, ordering and tick state.** Do **not** feed FPGA
results back as another writer of canonical simulation state.
`zhao_terrain_writeback.sv:27-32` already refuses mirrored state under T4 as a
second-writer violation — **do not create the thing that rule exists to stop.**

**And clean up behind the decision:** *"Explicitly retire or classify
unnecessary nav-specific hardware staging/output paths under this new ownership
decision, preserving shared-engine behavior and existing program/ABI
compatibility."* **Classify rather than delete where you are unsure**, and say
which you did for each path. **`FIELD.WRITE.NAV` itself stays.**

## ACCEPTANCE — the owner's list, not mine

**Exercise the PRODUCTION runtime query through real field activation and
terrain state, NOT by injecting a final cost into a fake consumer.** Show:

1. **No-field results match the authored baseline.**
2. **A nonzero field changes the queried cost in its covered region.**
3. **A small route-selection test using that same API responds to the changed
   costs**, then responds correctly **when the field expires or is removed.**
4. **Hard-blocked terrain remains blocked, including with negative cost
   deltas.**
5. **Overlapping fields, saturation, command order and generation changes
   follow the existing rules.**

**The route-selection test may be a small harness. The query implementation and
its state must be PRODUCTION CODE, not a disposable test substitute.**

**Measure CPU work and memory for representative workloads.** Two explicit
prohibitions, both about flattering numbers:

* **Do not claim ARM performance from an OMEN benchmark.** If you cannot measure
  on the target, say the number is desktop and unvalidated for ARM.
* **Do not hide unbounded full-world evaluation behind an apparently cheap
  API.** State the complexity and what bounds it.

## HONEST COMPLETION — and I34 does NOT close on your say-so alone

* **Record this as an architecture revision.** The old FPGA-publication
  requirement is marked **SUPERSEDED**, never *"implemented"*. That is already
  done in the directive, the docket and the decision record — **link your
  replacement obligation to it, do not re-litigate it.**
* **Navigation stays genuinely open until the service is implemented,
  integrated and tested.** The owner: *"Do not make the register reach zero
  through a dated stopgap, a renamed gap, or a computed-but-unread lane."*
* **Material is a SEPARATE half of I34 and is not yours.** It continues through
  its real Field-to-material path, and **it does not close because authored
  terrain materials render** — the bar is that **a Field material write changes
  the intended consumer.** Do not touch it; do not claim it.

## Evidence bar

* **The five acceptance items above, on the production API.**
* **Prove your own instruments.** A test that passes because it never ran is
  this campaign's most expensive habit. If you assert a cost is unchanged,
  show a case where it changes.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE** (no pipe — it reports the pipe's
  status otherwise) if you touch anything the register reads.

## Traps

* **Read the BUILD's exit code, not a pipeline's.** `| tail` reports `tail`'s
  status; a PowerShell *exception* leaves `$LASTEXITCODE` carrying the previous
  command's value. Both produced false greens here this week.
* **Configure from PowerShell with `tools/env/zhao-env.ps1` sourced**, always.
* **`std::ofstream` faults at -O1 on this toolchain** — use C stdio.
* **One `ctest` at a time per build tree**, and clean
  `build/Testing/Temporary/CTestCheckpoint.txt` and `LastTest.log.tmp*` after
  killing one or the next run wedges at startup.
* **A suite reads the LIVE TREE** — do not edit sources while one runs; the reds
  are phantom and the greens are worthless.
* **`[IO.File]` ignores `cd`** — absolute paths only, or your writes land in the
  coordinator's checkout.
* **Stage your HUNK on any shared file**, never `git add <file>`, and never
  `git checkout --` on one.
* **The scratchpad is shared between concurrent agents** — prefix every temp
  file with `navservice_`.

## Deliverable

1. **The service**: where it lives, what its interface is, and **who can call
   it** — named callers, not a claim of callability.
2. **What you REUSED** and what you had to write, with the cross-repo search
   that established it. **Name the repositories you searched.**
3. **The five acceptance items**, each with its evidence.
4. **The CPU work and memory measurement**, with the target honestly labelled.
5. **Which nav-specific hardware paths you retired and which you classified**,
   and why each.
6. **Every claim in this brief or the decision record you found FALSE.** Every
   packet this week found at least one; most were mine.
7. **What you refused.**
8. **Anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/navservice` only.** Never `--force`.
