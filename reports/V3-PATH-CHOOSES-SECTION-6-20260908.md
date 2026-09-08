# The V3 receipt chooses §6, and it does so on its own evidence

The owner brief deferred the architecture choice deliberately:

> *"Which goes first depends on the first actual V3 composition's physical
> report."*

and gave the criterion in §0:

> *"Long raw-depth / normalization / seed path -> section 6.
>  Large expanded metadata / multi-read glue -> section 7."*

The report now exists.

## The measured gate

```
worst path   slack -2.134 ns
  from       frag_depth_i[14]
  to         zhao_raster_rcp24_svc:u_rcp|c_x[0][22]
```

**Raw fragment depth, straight from the input pin into RCP context state.**
That is the first branch of the criterion, named almost word for word.

Cross-check: −2.134 ns against a 10 ns clock gives **82.41 MHz**, which is
exactly the Fmax the fit reported. The gating path and the headline number are
the same measurement, so neither is an artefact of how I read the report.

## Why this matters more than it looks

The brief was careful to warn against reusing the diagnosis it had:

> *"This work is conditional on fresh V3 attribution; the brief does not blindly
> reuse yesterday's critical-path diagnosis."*

It had this path from the **legacy** island. It could have been a property of
the old composition, or of Stage A's boundary, or of that fit's placement. It is
not: the same family gates the V3 composition, on its own 4-hour fit, with a
digest-verified specimen.

**A prediction made on one design and confirmed on a different one is worth
considerably more than the same number measured twice.**

## What §6 actually asks for

> *"accept and reserve a context -> normalize -> capture seed/prepared operands
> -> existing registered issue/multiply pipeline."*

with the constraint that matters:

> *"The important detail is reserve at acceptance, not several cycles later when
> preparation finishes. Otherwise the added pipeline can repeatedly allocate the
> same apparently free context."*

That is the same reservation-versus-acceptance distinction as M6 and as v3own's
§11.1 — the third time this exact hazard appears in this subsystem. The brief's
bundled `A1` check (admission fork, 8 truth cases) detects the phantom-admission
mutant, and the island's own ingress law is already stated:

```
external_fire = frag_valid && owner_ready && rcp_ready
owner_fire    = external_fire
rcp_fire      = external_fire
```

**One acceptance event.** Whatever `rcp_ready` becomes — "preparation
reservation available" — that must not become two.

## §7 is not refuted, it is deprioritised

The metadata join is still a real structure: one writer, three asynchronous
readers, and a join point that already exists in `rsp_dispatch`. Nothing
measured here argues against it. It simply is not what gates the clock, and the
brief's ordering is explicit.

Its groundwork is committed at
`reports/V3-METADATA-JOIN-CANDIDATE-20260908.md` and still carries no saving
claim, correctly, because the MAP-level inventory has not been done.

## Second-tier families, recorded but not acted on

Below the RCP gate, the 200 summarised paths cluster on `cq_rp` (the combine
ready-queue read pointer) reaching palette `cold_o`/`stale_o` outputs and a RAM
block. Noted so the next reader does not rediscover it, and **not** interpreted
here: M6 as amended says a path family is evidence to look at, not a conclusion
to draw.

## Prerequisite, unchanged

§3's contract repairs land first — A, B and C are already in, with their
positive fault tests at 17 checks. The brief orders it that way and a storage or
timing change on top of unproven observability is how a regression becomes
invisible.
