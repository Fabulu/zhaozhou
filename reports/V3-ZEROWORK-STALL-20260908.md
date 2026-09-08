# RETRACTED: "a zero-work fragment does not retire in the composed island"

**Claimed 2026-09-08. Withdrawn the same day. It was the harness, not the RTL.**

With a clean reset before the phase, the zero-work path is fine:

```
zero-work wrap: submitted 300, retired 300 (slot space 64, so ~4x wrap)
rcp 300 | persp 300 | plan 0 | cache 0 | dispatch 0
expander frags 300 | phases 300 | refused 0 | live peak 10
[island_v3_fault_directed] 9 checks passed
```

300 zero-sample fragments, every one retired, none lost, none duplicated, no
foreign tag, the 64-slot owner space wrapped about four times.

## What I actually measured the first time

`submitted 16, retired 0` — and I read it as a stall on zero-work fragments.

The counters said otherwise as soon as I printed them: `plan 9 | cache 1 |
dispatch 0` across the **whole run**, including phases 1-3. That is almost no
traffic anywhere, which is not what a zero-work-specific defect looks like.

The cause is in this test. `quiesce()` never raises `fill_data_valid_i` — the
harness deliberately does not model texture memory, because phases 1-3 only need
fragments *admitted*, not completed. So every `sample_count = 1` fragment from
those phases was admitted and could never complete. By the time phase 4 ran, the
machine was full of them. **16 was PERSPUV's `NTOK`, reached by the leftovers.**

## Why I got it wrong, specifically

I checked one plausible depth (`FCTXN = 64`), found it did not match, then found
`NTOK(16)` and stopped — because 16 matched and the story was coherent: *tokens
not released, so nothing downstream completing, so zero-work fragments must be
stuck.* Every step was true. The conclusion did not follow, because I never
asked whether the **earlier phases** had already consumed those tokens.

This repository's own law names it: *the first explanation that absolves the
design is the one to check hardest*. This one did the opposite — it **accused**
the design — and I gave it exactly the same free pass, because a fresh defect in
a freshly-restructured block is a satisfying story.

The check that settled it cost one reset and one rebuild.

## What the probe is now worth

It passes, and it is worth keeping:

* it is the first composed test in this tree to drive `sample_count = 0` at all
  — `island_composed_directed` drives 3, and 1-or-2, **never 0**;
* it wraps the owner slot space ~4x, which the 392-record paired run does not;
* its non-vacuity check (`submitted >= 200`) is what makes the pass mean
  something rather than being a phase that quietly tested nothing.

**It is still not a proof of §5.1's implication.** The brief asks whether an
owner can retire while a legitimate front-end reader remains. This is one
schedule, with a free-running consumer. Absence of a trace is not absence of the
hazard, and the brief's own abstract witness (`Z1`) says the same about itself.

## Unrelated, and still open

`cnt_combine_jobs_o` reads **2,558,523,520** on a run that issued no combine
jobs, and a different garbage value on the previous run. That counter looks
unreset or X-propagating. Small, real, and not investigated here.
