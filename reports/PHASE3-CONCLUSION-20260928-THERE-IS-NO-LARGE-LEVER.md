# Phase 3, concluded: the available optimization is ~10% of the placement gap

Coordinator, 2026-09-28. This is the answer to the standing goal's clause 2 —
*"get the real full console fit finished so we know where we stand"* — for the
part a fit cannot answer, because the fit cannot run.

**Three independent measurements, taken tonight, converge.** Each was expected to
find a lever. Each found a small one.

## The three

**1. Lookup-for-computation: ~2,000–2,500 ALUTs.**
A five-row map sweep with a positive control proved that registering a table's
read converts it to an M10K ROM at **145 comb ALUTs → 0**, straight from a case
statement, no rewrite. Then `tools/budget/case_tables.py` (self-tested against
that very ROM, so a null result is a real absence) found the production console
contains **exactly two** combinational constant tables. Everything else it
surfaces is generated fit-harness scaffolding. The "twelve blocks, 32,784 ALUTs
of pure computation" I had pointed at are **genuine arithmetic** — `mulbank`,
`edgewalk`, `spline`, `normalize`, `devstore`, `part_update` — and no lookup
replaces arithmetic.

**2. Deduplication: ~4,000 ALUTs.**
`PHASE3-THE-LEVER-IS-DUPLICATION-20260928.md` opened at 40,091 ALUTs "in
duplicated instances" and **corrected itself** at the top: the `n` column implies
*n comparable copies* and for the two largest rows that is false. What survives
is `zhao_field_v3_normalize`, `zhao_field_isqrt` and `zhao_geom_mat3x4_mul` —
about 4,000 ALUTs, each needing a shared service with arbitration between
subsystems that run concurrently.

**3. Parallelism: there is no knob.**
This was the lever I held back as "second, with headroom of the right order".
Enumerating every numeric parameter the console composes returns **widths and
capacities, not replication factors**: `OUT_LANES=7`, `IN_LANES=15`,
`LAT_W/LAT_H=33`, `MAX_FIELDS=16`, `SUBPATCHES=16`, `TILES=576`. These are
*function-defining* — they are the field count, the lattice, the tile grid — and
the directive forbids reducing exactly these: *not authority to cut 16 fields to
4, remove Gouraud or detail normals, shrink the guaranteed giant.*

**The console is not N copies of one machine. It is many distinct machines.**
That is why deduplication found only 4,000 ALUTs and why there is no `N` to turn
down.

## What that adds up to

| lever | measured | share of the 66,766 placement gap | of the 210,066 shipping gap |
|---|---:|---:|---:|
| case tables → ROM | ~2,000–2,500 | ~3–4% | ~1% |
| genuine duplication | ~4,000 | ~6% | ~2% |
| parallelism reduction | **no knob that is not a feature** | — | — |
| **total available** | **~6,000–6,500** | **~10%** | **~3%** |

**Cashing every optimization identified leaves the design at roughly 287,000
ALUTs against a 227,120 ceiling for placement on the largest installed die, and
83,820 for the shipping part.**

## The conclusion, stated plainly

**This is not an optimization problem and it has not been one for some time.**
The console is ~294,000 ALUTs of largely distinct logic. Reaching the shipping
part means removing ~71% of it, and the measurements above say that cannot come
from duplication, tables or parallelism. **It could only come from function** —
which the directive forbids, correctly, and which is the owner's call and not
mine.

This is what the directive means by *"a measured engineering impossibility is a
finding, not permission to invent a pass."* **The finding is now three
measurements deep rather than one fit deep**, which is the difference between
this and the escalation I filed and then retired.

Note what is NOT claimed: that no individual block could be made smaller. Any of
them could, by redesign, and `zhao_terrain_devstore` at 7,070 ALUTs is worth a
look on its own merits. That is per-block engineering at a few thousand ALUTs a
time, not a lever, and at 71% it does not change the answer.

## What I am doing about it

1. **Cash both levers anyway.** ~6,000 ALUTs is hygiene, not a plan, but an
   uncashed cheque is this repository's most expensive recurring habit and both
   are now measured and cheap to justify. The ROM conversion is one cycle of
   latency at 13 call sites; the consolidation is a shared service. Neither is
   started — each is a packet with a latency-contract change and consumers.
2. **Stop treating whole-console placement as the near-term gate.** Every real
   measurement of the past week came from standalone `-MapOnly` runs, because
   the console fit has never once completed. That is already how this campaign
   works; it should be said out loud rather than discovered each time.
3. **The one owner question that survives**, and it gates nothing: *what is
   whole-console placement FOR* — bring-up, a demo, or timing evidence? Each
   implies a different reduced diagnostic target, and I can build any of them
   honestly. What I cannot do is produce a full-capability console that places
   on this silicon, and I now have three measurements saying so rather than an
   argument.
