# Composed shell fit — brief for the machine that can hold it

You have been handed this repository to answer **one question** that the primary
development machine cannot: **does the composed `zhao_shell_top` fit the target
device, and does it close timing?**

Everything else in this repo is already covered. Do not start other work.

## Why you and not the other machine

The composed fit does not run there. Measured 2026-08-18: `quartus_map`
committed **28.4 GB against 24 GB of RAM** and thrashed at near-zero CPU until
it was killed. So every synthesis number this project owns is a **per-block**
fit, and per-block fits cannot answer the composed question. The current
per-block total is an **upper bound** inflated by roughly 9,000 virtual pins
that become plain wires once composed, and by the total absence of cross-block
optimisation and resource sharing. Nobody knows the real figure. That is what
you are here to get.

## Run it

```powershell
cd <repo>
.\tools\quartus\run_composed_fit.ps1
```

That is the whole job. Pass `-QuartusBin <path>` if Quartus 17.0.2 Lite is not
at `C:\intelFPGA_lite\17.0\quartus\bin64`. Pass `-NoPush` if you want to inspect
before it pushes.

Before you start, **close what you can** — browsers especially. The peak is in
the `quartus_map` stage and it is the whole point of the exercise; 32 GB is a
tight fit, not a comfortable one. The script prints free RAM at the start and
warns below ~30 GB.

Expect **hours**, not minutes.

## The one rule that matters

**Only ever create files inside your own run directory.**

The other agent is working in this repository at the same time. The script mints
`reports/composed/<host>-<shortsha>-<utc>/` and writes only there, so two
machines cannot collide even on the same commit in the same second. Git merges
disjoint new files without conflict, always.

So:

- **Do not** edit `reports/synthesis/zhao_block_fit.json`, any contract, the
  ledger, any TASK_LOG, or any RTL. Not to "record the result properly", not to
  tidy, not for anything.
- **Do not** merge or rebase anyone else's work into yours. If your push is
  rejected the script fetches and rebases *your single commit* on top, and
  retries. That is safe precisely because your commit adds only files nobody
  else has.
- The script **refuses to commit** anything outside its run directory and will
  stop rather than do it. If it stops for that reason, something is wrong —
  report it, do not work around it.

The tree must be clean before you start; the script checks. A fit result is
worth nothing unless it names the exact commit it measured.

## If it fails

**Commit the failure. Do not delete it and retry quietly.**

The script already does this: a non-zero exit still writes `OUTCOME.json` and
the full `run.log`, still commits, still pushes. "The composed fit did not
complete on 32 GB either, and here is how far it got and what it was doing when
it stopped" is a real answer to the device question — arguably a more important
one than a clean pass. Discarding it just means the next person rediscovers it.

If it fails for an environment reason (Quartus missing, wrong path, dirty tree),
fix that and re-run. Each run gets its own directory, so retries never overwrite
each other and the history of attempts survives.

## What lands here

Per run:

| file | what it is |
|---|---|
| `MACHINE.json` | host, CPU, cores, RAM total and free at start, Quartus version, the commit measured |
| `OUTCOME.json` | exit code, whether it completed, wall seconds, free RAM at the end |
| `run.log` | the full transcript |
| `synthesis/`, `timing/` | the fitter and TimeQuest reports, when it gets that far |

## What this is not

Not a programmed board and not fabricated silicon. A composed fit against a
**provisional** device with virtual I/O says the design maps and closes timing
in the tool. It does not say anything ran. Keep that distinction in whatever you
report — this project has been careful about it and the care is deliberate.
