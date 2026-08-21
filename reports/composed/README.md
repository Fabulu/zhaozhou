# Composed shell fit — brief for the machine that can hold it

> # SUPERSEDED 2026-08-21. DO NOT ACT ON THIS BRIEF.
>
> **The handoff described below is not needed. The 28.4 GB was a bug, not a
> requirement.**
>
> The project's last line was
> `set_instance_assignment -name VIRTUAL_PIN ON -to *`. A wildcard instance
> assignment is matched against **every node name in the design**, not the
> top-level ports it was written for, so Quartus carried a VIRTUAL_PIN
> candidacy — a pattern match and a database entry — for every internal net in
> the cone. Fixed in `d1a2b8a` by naming the 101 ports of `zhao_shell_top`
> explicitly.
>
> With that fix, composed analysis and synthesis **run on the development
> machine**: 42:33 at a **6.2 GB** peak (`f3506b6`). The commit that landed it
> says so directly: *"The work-PC handoff stays paused and is very likely
> unnecessary now."*
>
> **What actually blocks the composed fit** is Quartus Error 276003 — registers
> that cannot convert to RAM megafunctions, because two memories have
> asynchronous read logic:
> `zhao_scanout_linebuf|mem` (**since fixed**, it now uses `zhao_dc_sdp_ram`
> with a registered read) and `zhao_cmd_dma|blit_buf` (**not fixed**; the
> combinational read is still there). That is an RTL task, not a hardware one.
>
> **And treat 6.2 GB as unexplained rather than settled.** `9c693a9` measured
> that parsing the entire source cone costs 0.24 GB — parsing is free — and
> that the cost is in ELABORATION, which is superlinear here for a top of
> sixteen ordinary instances with no generate blocks and no large arrays.
> *"There is nothing pathological in the design."* The suspect is the Quartus
> 17.0.2 Lite elaborator itself, and **trying a newer Quartus is the named
> lever that nobody has pulled.**
>
> This file is kept rather than deleted because the false 28.4 GB figure
> "was believed long enough to shape decisions" (`9c693a9`), and a deleted
> file cannot warn the next person who half-remembers it.


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
the `quartus_map` stage; 32 GB is a tight fit, not a comfortable one, and
Windows itself will already be holding several GB. The script prints free RAM at
the start and warns below ~30 GB.

### Peak memory is the thing to manage

The script already defaults to `-Processors 1`, and that is the biggest single
lever. The 28.4 GB measurement was taken with the project's
`NUM_PARALLEL_PROCESSORS 4`: each worker carries its own working set of the
netlist, so the committed footprint scales with the count. Dropping to one
trades wall time for memory and **changes nothing about the result** — the same
work, divided differently. The override is applied to the STAGED copy of the
project only, so the committed QSF is untouched and per-block characterization
elsewhere is unaffected.

If it still will not fit, in order:

1. Raise the **page file** to 64 GB on the fastest drive. Paging is slow, but a
   run that finishes slowly beats one that dies. The original failure thrashed
   at near-zero CPU because 28.4 GB against 24 GB of physical RAM is hopeless;
   28.4 GB against 32 GB with headroom behind it is a different situation.
2. Reboot first and start the run before opening anything else. A freshly
   booted Windows holds far less than one that has been up for days.
3. Report back rather than lowering synthesis quality to force a pass. A fit
   that only completes at reduced effort is a different measurement, and this
   project would rather know the real number than have a comfortable one.

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
