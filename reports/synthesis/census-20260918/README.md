# `zhao_prod_top@whole-console-sizing` — synthesis kept, fit STOPPED

## What happened

Launched before the owner's completion plan arrived, under the belief that
`zhao_prod_top` was the console. It is not: plan §1.3 requires it stay labelled
`RESOURCE_CENSUS_DISCONNECTED`.

**Analysis & Synthesis SUCCEEDED** (20:51:41, ~170 min, 147 snapshotted sources,
digest `de21d0f5a7d1`). Its numbers are real and are kept here.

**The Fitter was stopped at 34 minutes**, deliberately, because it could not
succeed:

```
synthesis reported            1,605,869 combinational ALUTs
Cyclone V packs 2 ALUTs/ALM   ~803,000 ALMs required
5CEBA9F31C7 (sizing device)   113,560 ALMs
                              7.1x OVER
```

The sizing device was chosen *specifically so the measurement could complete*,
and the design is seven times too large for it. Placement would have ground for
hours and then reported "cannot fit" — an outcome derivable in one line of
arithmetic. Owner, on being shown the running fit: *"Why are we fitting? The
console is not done."* Correct.

**This is not the plan's "do not kill a running fit" case.** That rule (§13.1) is
about not disrupting a fit merely to adopt a new document. This one was stopped
because it was answering a question nobody asks, on a top the same document
forbids calling a console, at a size that guarantees failure.

## What is kept, and why

| file | why |
|---|---|
| `blockfit.map.summary` | the headline numbers: 297 DSP, 105,881 registers, 1,029,005 memory bits |
| `blockfit.flow.rpt` | timing/versions/provenance of the run |
| `blockfit.map.rpt` (8.6 MB) | the full hierarchy — the per-entity attribution that found `zhao_vertex_arena` at 92.2% |

The 8.6 MB hierarchy report is preserved **on disk here but deliberately not
committed** — its one durable finding is already written up in
`reports/WHERE-THE-CENSUS-ACTUALLY-LIVES-20260918.md`, and this repository has a
chapter on intermediates that accumulate because nothing deletes them.

## What these numbers are, stated precisely

The **OLD GROSS CENSUS** of plan §14.1, measured by synthesis instead of summed
from per-module maps. Not a console. Not on the target device. Not containing any
of the eight blocks built on 2026-09-18, because the snapshot predates them.

Its two durable contributions:

1. **297 DSP**, spread across twelve identifiable owners — including 66 DSP of
   projector duplication (`zhao_geom_project` and `zhao_terrain_project` each
   carrying a `zhao_project_core` at 33) that had only ever been estimated.
2. **`zhao_vertex_arena` at 1,480,718 own ALUTs — 92.2% of everything**, with all
   eight `altsyncram` instances beside it at zero. Storage that did not become
   memory. Whether that is a real cost or an inference failure is the open
   question, and it will follow the design into any console fit, so it is worth
   settling before the next measurement rather than after.

## What replaces it

`zhao_console_core` — the connected machine the plan requires by name. Being
assembled now. That is the top worth fitting.
