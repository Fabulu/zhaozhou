# Task Log: RUN-20260827-1926 - [Describe objective here]

**Created:** 2026-08-27 19:26 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260827-1926-sweep-coverage-audit/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-27 19:26 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260827-1926
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## Why this run exists

`reports/SWEEP_COVERAGE_AUDIT.md`: **38 of 59 modules with a test lane had
never been mutated.** A green lane says the block passes its tests; only a
sweep says the tests would NOTICE the block being wrong. This run is the work
of closing that, block by block.

The first four were done before this run folder existed (they are in
`RUN-20260826-0113-field-v2-curve-and-longop-interlock`, which was the Field
run and the wrong home for them):

| block | first run | after closing gaps |
| --- | --- | --- |
| TERRAIN.PATCH | 19/20 | 20/20 |
| DEBUG.FRAMEBLIT | 12/20 | 16 caught + 4 proven equivalent |
| GEOM.WCACHE | 15/18 | 18/18 |
| CMD.DMA | 6/21 → 12/21 (harness bug) | 19 caught + 1 proven equivalent |

**Seven real test gaps found and closed. Five equivalences proved. One bug in
my own tooling**, in two places.

## What the four taught, carried forward

* **A sweep must run EVERY ctest lane of a shared binary.** CMD.DMA is three
  ctest lanes over one executable; running the bare exe scored 6 of 21 instead
  of 12. The terrain sweep had the same flaw (`test_field_out_height` is
  registered ONLY as `--random 300`) -- it did not invalidate that 20/20,
  because adding lanes can only INCREASE the catch count, but the coverage
  behind it was thinner than it looked.
* **A mutant that cannot build is a discard, not evidence.** Eleven mutations
  across the four blocks first orphaned a signal or a parameter and failed the
  LINTER. The preflight catches them before anything is scored, which is the
  whole reason it runs first.
* **A mutant is only caught if pristine and mutant reach DIFFERENT verdicts.**
  CMD.DMA reports BAD_LENGTH from nine separate gates, so four of my first
  attempts had the mutant fall through to another gate reporting the same
  status. They looked like reasonable tests and proved nothing.
* **Equivalence must be PROVED and machine-readable**, or it is a hole with a
  note attached. The drivers now consult an `EQUIVALENT` table and FAIL on an
  undeclared survivor. Two of the five proofs name a *re-score condition*
  rather than the mutant -- a canvas that is not a multiple of 64, and
  SLOT_BUF_BYTES rising above 1,048,536 -- because that is the thing that will
  actually change.

## 19:26 — RASTER.RESOLVE, and a stale law found while reading

Next block on the list. Reading it to author the mutant table turned up two
statements of the dither law that contradicted the code they described:
`zhao_raster_resolve.sv`'s header quoted green at `(B*32 + 16)` and declared
GREEN IS DIFFERENT, while the module instantiates green at AMP=16/RND=8.

The code is right; the header is the pre-2026-08-16 law, and that old 32/16 is
exactly what wrapped the six-bit field at full white. `test_green_amplitude`
carried the same stale premise and kept passing because it is built well --
its candidate arithmetic only measures whether the vectors can tell the laws
apart, and every real check goes to the oracle.

Both corrected in `2ee5f28`, in place and with the reason, because a header
that contradicts its code is how the next reader repairs working silicon into
the bug it warns about.

**Not yet swept.** The Field v3 agent holds the build tree and has
`tests/CMakeLists.txt` and `reference/CMakeLists.txt` uncommitted and
half-edited; every sweep runs `cmake -S . -B build` and would configure
against work in progress. Authoring the mutant table is the useful part that
needs no build.

## 22:10 — RASTER.RESOLVE 17/17, RASTER.EARLYZ 15/16

**RASTER.RESOLVE: 17 of 17 caught**, first run, no survivors, nothing to
declare equivalent (`a4919a8`). The first block in the audit to come back
clean.

That is not "beyond suspicion" — it is "this suite already had teeth", which
the four before it did not. The difference is visible in how it was written:
its checks go to the ORACLE rather than to remembered RTL behaviour, and
`test_green_amplitude` measures its own discriminating power (2,037 of 4,096
(g, Bayer) pairs) before it checks anything. A suite that asks *could these
vectors even tell the two laws apart* is the one that survives a sweep.

**RASTER.EARLYZ: 15 of 16 caught** (`f0db971` authored the table). The block
is unlike the rest of the audit and the table was built around that: almost
every other block is wrong when it computes a wrong value, but this one is
wrong when it REJECTS SOMETHING IT SHOULD NOT HAVE — invisible in a pixel
comparison unless a test draws geometry the exact late test would have kept.
So the mutants were split into reject-less (safe, costs throughput) and
reject-more (silent correctness failure), and every reject-more mutant was
caught, including the inverted test.

### The survivor, E12, and why it took two phases to reach

`hiz_qualify` drops its `!reject_c` term, letting a REJECTED fragment into the
hierarchical-Z accumulator.

It is safe in the depth sense, and the reasoning matters: a rejected fragment
has `depth <= floor`, it enters a MIN, so `acc_min` can never exceed the floor
and the `> floor` guard blocks any rise. **The floor cannot move backwards.**

What changes is COVERAGE. The rejected fragment marks its pixel, so the
accumulator fills and RESETS early, discarding evidence the oracle still
holds — and the divergence surfaces a whole tile later, as a floor that fails
to rise and a reject count that drifts. The oracle carries the same `!reject`
term, so this is a real divergence and not an equivalence.

Nothing in the file reached it because rejection needs the floor RAISED first:
with the clear depth at 0 and larger-is-closer, almost nothing is rejected at
the start of a tile. The closing case is therefore two-phase — cover the tile
to raise the floor, then submit rejected-but-otherwise-qualifying fragments
over half the tile, then a genuine cover.

### Status: WRITTEN, NOT VERIFIED

The case is authored and **deliberately not committed**. Three sessions now
share one build tree and I could not build it:

    ninja: error: failed recompaction: Permission denied
    Error copying ... Vzhao_probe_curve_svc.cmake: No such file or directory

The second is the Field agent mid-build on probe 4 (the curve service), whose
CMake entry names files that do not exist yet. My earlier "63 checks passed"
in that window was the OLD binary — the build had failed and the run proved
nothing about the new case.

Committing an unverified test would be worse than waiting: my first attempt at
the CMD.DMA F05 case failed on PRISTINE RTL, which is exactly what verification
is for. It goes in when the tree is free.
