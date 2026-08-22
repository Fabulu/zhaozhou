# Runs

**Every session of work on this hardware is a RUN, and the run is created at the
START of the session, before any work.** This is not paperwork. It is the only
record of *why* something was attempted, what was rejected, and what an agent
found — none of which git history preserves.

## How to start one

The tooling lives at the **workspace root**, one level above this repository:

```
C:\programmieren\zencrifice\runs\CLAUDE-RUNS\init-run.ps1 <kebab-slug>
```

It creates `RUN-YYYYMMDD-HHMM-<slug>/` containing `TASK_LOG.md` and
`SPEC_v1.md`, expanded from `docs/coding_agents/claude_run_templates/`
(also at the workspace root). A third template, `FINDINGS/FINDINGS.md`, is for
subagents to fill in.

> **The tooling is outside this repository, which is exactly why it gets
> forgotten.** It was forgotten for the whole of 2026-08-17, 08-19, 08-20 and
> most of 08-22. Those runs have been reconstructed from git history and are
> marked as such — read their preamble for what a reconstruction cannot recover.

## What goes in a run folder

| file | purpose |
| --- | --- |
| `SPEC_v1.md` | the objective as understood **at the start**, its scope, its constraints, and a **Don't Retry** section for failed approaches |
| `TASK_LOG.md` | the running record: Progress Timeline, **Subagent Spawns** table, Files Created, Decisions Made, Next Steps |
| `FINDINGS-*.md` | one per subagent, written **by that agent**, from the template |
| anything else | evidence the run produced — measurements, harnesses, reports |

`TASK_LOG.md` is updated **as the work happens**, not written up afterwards. A
retrospective log is a summary of the diff; a contemporaneous one is the reasoning.

## The Don't Retry section earns its keep

It is where a failed approach goes so it is not re-learned after a context
compaction. Real entries from this project: the build system reporting "100%
tests passed" against a tree that never rebuilt; hashing Verilator's wrapper file
instead of its logic file and concluding nothing had changed; deleting
`.ninja_deps` to clear a lock and silently relinking stale objects.

## Archiving

**The workspace root is not a git repository.** Runs created there are not
version-controlled and will be lost. Copy each completed run into this directory,
which is tracked, and commit it. On 2026-08-22 sixteen run folders were found
living only at the root, unprotected, going back to the first day of the project.

## Subagents

Each subagent gets a row in the Subagent Spawns table — timestamp, agent id,
purpose, status, link to its findings — and is told to write its own
`FINDINGS-<slug>.md` into the run folder. Agents write their own findings; do not
summarize on their behalf, because the summary loses exactly the detail the
findings file exists to keep.
