# Runs

**Every session of work on this hardware is a RUN, and the run is created at the
START of the session, before any work.** This is not paperwork. It is the only
record of *why* something was attempted, what was rejected, and what an agent
found — none of which git history preserves.

## How to start one

From the repository root:

```
runs\CLAUDE-RUNS\init-run.ps1 <kebab-slug>
```

It creates `RUN-YYYYMMDD-HHMM-<slug>/` **in this directory**, containing
`TASK_LOG.md` and `SPEC_v1.md` expanded from
`docs/coding_agents/claude_run_templates/`. A third template,
`FINDINGS/FINDINGS.md`, is for subagents to fill in.

> **This tooling used to live at the workspace root, outside the repository,
> which is exactly why it got forgotten** — for the whole of 2026-08-17, 08-19,
> 08-20 and most of 08-22. Those runs were reconstructed from git history and
> are marked as such; read their preamble for what a reconstruction cannot
> recover.
>
> On 2026-08-23 the script and the templates were moved **into the repository**,
> so a run is created in the tracked directory from the outset and there is no
> copy step left to forget. The root copy is stale — it was already missing two
> archived runs — and should not be used. Verified by running the in-repo script
> end to end.

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

Runs are now created directly in this tracked directory, so there is nothing to
copy — **but they still have to be committed.** An uncommitted run folder is as
lost as one at the root if the machine dies.

The history behind this: the workspace root is not a git repository, and on
2026-08-22 sixteen run folders were found living only there, unprotected, going
back to the first day of the project. Copying each run into the repo was the
fix — and then the copy step itself got forgotten, which is why the tooling
moved instead of the runs. `ARCHIVE.md` is tracked here now too; it previously
existed only at the root, so the index of the archive was the one file the
archive could not protect.

Commit the run as the work happens, not at the end.

## Subagents

Each subagent gets a row in the Subagent Spawns table — timestamp, agent id,
purpose, status, link to its findings — and is told to write its own
`FINDINGS-<slug>.md` into the run folder. Agents write their own findings; do not
summarize on their behalf, because the summary loses exactly the detail the
findings file exists to keep.
