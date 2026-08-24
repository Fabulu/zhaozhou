# SPEC v1: Projector merge, phase 1 — one shared projection core

**Run ID:** RUN-20260824-0522
**Created:** 2026-08-24 05:22 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

`zhao_geom_project` and `zhao_terrain_project` implement the SAME projection
law — `zref::render::project_vertex` — twice. Extract one shared
`zhao_project_core` carrying that law, and have both blocks instantiate it,
with **bit-identical behaviour and unchanged timing and ordering on both
sides**.

Success is three things, in this order of importance:

1. **Nothing changes behaviourally.** Both callers stay bit-identical against
   the shipped oracle, cycle for cycle, including backpressure.
2. **The law lives in one file.** The two later levers — the projected-vertex
   cache and the 27-bit width narrowing — then apply once instead of twice.
3. **The DSP arithmetic is reported as measured, not as assumed.** See the
   Open Question below; the brief's 66 → 33 expectation is under test, not
   accepted.

---

## Scope

**In Scope:**

- A differential that drives BOTH existing blocks from one stimulus stream and
  compares every output every cycle, BEFORE any RTL is written. If they differ
  anywhere, that is the finding and the merge stops.
- `fpga/rtl/common/zhao_project_core.sv` — config register file, row sums,
  rescale, near-plane verdict, divider setup, the 31-step restoring
  recurrence, quotient assembly, `fx_mad`, `to_screen_xy`, with a parameterised
  opaque payload and a caller-supplied advance enable.
- Rewriting both blocks to instantiate it. `zhao_terrain_project` keeps its
  vertex sequencer, its triangle assembler and its `idle_o`;
  `zhao_geom_project` keeps its vertex interface and its counter.
- Differential of each rewritten caller against the shipped oracle AND against
  the pre-change RTL.
- Mutation sweep, in a git worktree, with forced regeneration and a binary-hash
  check.
- Map-only measurement of `zhao_geom_project`, `zhao_terrain_project` and
  `zhao_project_core`, before and after.

**Out of Scope:**

- **The projected-vertex cache.** `GEOM.WCACHE` owns it
  (`zhao_terrain_project.sv:196-205`). Docketed, not built here.
- **Width narrowing to 27 bits.** Needs a proof that 27 bits covers world
  coordinates — an owner question about map size and precision. Note where the
  proof would be needed and move on.
- Any change to the projection law itself.
- Game behaviour for the particle-simulation, compositor or 2D blocks.

---

## Constraints

- **All git goes through Git Bash.** Two git installations disagree on this
  tree: Git Bash 2.45.2 (`core.autocrlf=true`) reports clean, PowerShell git
  2.49.0 (`autocrlf` unset) reports ~294 modified files, all line-ending
  phantoms. Inherited verbatim from RUN-20260824-0317 §03:20 and confirmed
  again at 05:23 this run.
- `git add -- <explicit paths>`, never `-A`. Read `git diff --cached --stat`
  before every commit.
- `git checkout <rev> -- <paths>` **STAGES**. Use `git restore --worktree`, and
  check `git status` after.
- **One Quartus job at a time**, nothing else heavy running. Map before fit.
  Never map uncommitted RTL. Never edit RTL while a Quartus job runs.
- `tools\quartus\run_block_map.ps1 -Module <m>` — **one module per
  invocation**; it writes `zhao_block_map.json` at the END of its module list.
- Sweeps run detached, **in a git worktree with its own build directory**
  (standing owner ruling), at the shipping commit, with a **verified non-zero**
  mutant count. Lint every mutant before scoring any.
- `ctest -L fast` has **one expected baseline failure**: `ledger_check`, V16,
  `FIELD.SEQ.CORE` formal recorded pending. Another lane's gate. Confirm the
  count is still exactly one at the end.
- Never hand-edit a measured number into a report; re-run the tool.
- A stopped background task is **not** a stopped process — verify children died.

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

- **Do not call the stimulus generator inside a `Both::drive(lambda)` helper.**
  RUN-20260824-0317 failure 1: the lambda is applied to both models, so
  generating inside it advances the RNG twice and the two DUTs receive
  DIFFERENT stimulus. Every "mismatch" is then the harness comparing two
  experiments. **Generate first into a value, then apply the value to both.**
- Do not use `python - <<'PY'` heredocs (RUN-20260824-0317 failure 8, two
  retries lost); write the script to a file first.
- Do not trust `Tee-Object` to have written its file (failure 5); `ls` it.
- Do not write `\q`, `\r`, `\n` etc. into a non-raw Python string that carries
  a Windows path (failure 6) — a mangled command in a run log is a trap for
  the next agent.
- Do not read a mutation-sweep failure COUNT without reading the failure TEXT
  (failure 7).

---

## Open Questions

1. **Does the merge actually save DSPs?** — RESOLVED BY MEASUREMENT, see
   TASK_LOG. Extracting a module that both blocks *instantiate* leaves two
   physical cores and therefore two sets of multipliers. 66 → 33 requires ONE
   arbitrated instance shared by both callers, which is a different and much
   larger change. `design/contracts/GEOM.PROJECT.md:239-257` states both — "have
   both instantiate it" AND "that halves the divider cost" — and those two
   sentences cannot both be true. Measure, do not argue.
2. **Could one arbitrated instance be afforded?** Needs the two callers'
   real demand. `design/budgets/workloads.yml` has the numbers; check whether
   the terrain row is right before quoting it.
3. Where exactly would the 27-bit proof be needed, if someone later takes that
   lever?
