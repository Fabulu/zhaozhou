# Contract — FORGE.PRIM (Procedural primitives)

> Ledger: `design/blocks.yml` · owner ZH-044 · phase 11 · maturity SPECIFIED

## Purpose and exclusions

**Six v1 families**, bounded subdivision with screen-error LOD:

1. ribbon
2. radial fan / ring
3. tube
4. radial shell
5. billboard sheet
6. terrain cliff / skirt

**REMOVED from v1 by ruling R11 (2026-09-02): shard burst, chain, cone and
spline wall.** They are not refused forever; they are not ratified, and they
need separate ratification to return. **Shard burst in particular is a particle
population**, not a Forge primitive — building it here would put a second
particle system in the geometry path.

The list above is a **ceiling, not a floor**. A broad purpose sentence is how
extra generators get ratified without anyone deciding to ratify them, which is
exactly what the previous version of this line did.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`.

Reset abandons the primitive in flight. **A partially emitted primitive is never
committed** — see the acceptance rule below; this block emits whole primitives or
nothing, so a reset cannot leave a half-built ribbon in the geometry stream.

## Input and output packet layouts
### The v1 primitive families — CLOSED, owner ruling 2026-08-31 §6.5

    ribbon          radial fan / ring       tube
    radial shell    billboard sheet         terrain cliff / skirt

**Six families, one bounded topology generator.** The ruling collapses the
block's original list, and the collapses are the interesting part:

| was listed separately | is actually |
|---|---|
| shard burst | a **particle population** — `PART.*`, not geometry |
| chain | a tube/ribbon, or repeated meshlet instances |
| spline wall | a ribbon or tube use |
| low cone | a radial fan or shell use |

That is four things deleted from the hardware by recognising them as uses of
something else. The block's purpose line still lists them; **this ruling
supersedes it**, and the sky modes and `beam_cone` named there are billboard
sheet and radial shell uses respectively.

### Limits — frozen

    MAX_SEGMENTS = 64
    MAX_SIDES    = 8

Worst case per primitive: 64 × 8 = 512 quads = **1,024 triangles**.

### Job in

`{ family, segments, sides, params, material_id, view_mask, src_id }`

### Out

A vertex stream to `GEOM.SETUP`, in a declared deterministic order — because two
orderings of the same primitive produce the same picture but different capture
CRCs, and the capture is the contract.

## Backpressure rules
Ready/valid to `GEOM.SETUP`. A stall mid-primitive holds the generator's cursor;
it does not restart and it does not skip.

**`job_ready_o` never depends on `job_valid_i`.** The generator accepts a job
only when it can guarantee the whole primitive — see acceptance below.

## Memory ownership
**None.** Parameters arrive in the job; the topology is generated, not fetched.

That is the defining property of this block and the reason it exists: a ribbon
described by 8 parameters costs 8 parameters of bandwidth instead of 1,024
triangles of vertex data. If it ever grows a fetch path, it has become a mesh
renderer and the saving is gone.

## Q formats and rounding
Positions fx16 S15.16, matching the geometry path. UVs fx16.

Ring positions come from the frozen `SIN_Q16` quarter-wave table — **the same
generated table the rest of the tree uses**, so a ring here and a rotation
elsewhere agree exactly. No new trigonometry is introduced.

Segment stepping is a **recurrence with an exact reseed** per primitive, the
same structure as `RASTER.ATTRSTEP` and `TWOD.PLANE`: it removes a multiply per
segment without changing an emitted bit, provided the reseed is exact — and the
directed test asserts exactly that.

One rounding per emitted component, round-half away from zero.

## Latency (fixed or variable)
**Variable in a bounded way**: proportional to `segments × sides`, bounded at
1,024 triangles by the frozen limits. Bounded is the property that matters —
`GEOM.SETUP` behind it can size its buffering from the worst case.

## Target throughput
**One emitted vertex per clock** (ledger).

A worst-case primitive is 512 quads; at 4 vertices each that is ~2,048 clocks,
**0.15 % of a frame**. A hundred such primitives would be 15 %, which is the
number that matters — a spell built from many maximal ribbons is the case to
watch, not a single one.

## Overflow and malformed-input behaviour
### The acceptance rule — from the ruling, and it is the whole safety story

> **Subdivision is selected before acceptance. Never emit a partial primitive.**

So the sequence is: compute the required vertex count, check it against the
limits **and** against the remaining frame budget, then accept or refuse. Once
accepted, the primitive is emitted whole.

| condition | behaviour |
|---|---|
| `segments > 64` or `sides > 8` | refuse before emitting anything |
| unknown family | refuse — never substitute a similar one |
| degenerate parameters (zero radius, zero length) | refuse; a degenerate primitive is a caller bug, and emitting zero-area triangles wastes setup and binning |
| frame vertex budget would be exceeded | refuse the whole primitive, count it |

**Refusing before acceptance is what makes "never emit a partial primitive"
achievable at all** — a check performed mid-emission would already have written
vertices that cannot be recalled.

## Counters and traces
* `triangles_submitted` (ledger)
* `primitives_by_family[6]`
* `primitives_refused_by_reason[4]`
* `vertices_emitted`, `max_vertices_in_primitive`
* `subdivision_histogram` — what `segments × sides` real content actually asks
  for, which is how the 64/8 limits get validated or challenged with evidence

## Scalar reference function

`zref::forge::prim_triangle` and neighbours
(`reference/include/zref/zref_forge.hpp`) — written 2026-09-03. This contract
previously cited `zref::ForgePrim`, **a symbol nobody had ever defined**; see
`reports/PHANTOM-CITATIONS-AUDIT.md`. The oracle is named for what it owns
rather than for the block.

It owns the six families' TOPOLOGY: the effective grid, whether a ring closes,
the vertices per ring, the triangle count, the emission order and the refusal
taxonomy. Positions are **not** here — the evaluator's `params` and `SIN_Q16`
own those, and a topology that depended on a position would stop being bounded.

It does **not** own setup, clipping or binning — it emits vertices into the
existing geometry path and everything downstream is unchanged. A primitive from
Forge and the same shape as a mesh must reach the rasteriser identically.

## Directed tests

**`tests/forge/forge_prim_directed.cpp` — WRITTEN**, 11 checks. Every family's
triangle count including the contract's own worst case `{tube, 64, 8}` = 1,024;
a tube's ring closing where a ribbon's stays open; the order identical across
two runs and under a stalling consumer; `last` marking exactly one triangle;
the four deleted families refused with nothing emitted; 65 segments, 9 sides
and zero refused; a job outside the view skipped rather than refused; and the
whole legal space walked against `zref::forge::prim_triangle` position by
position.

The items below are **planned and not written**, and are named without paths
for that reason:

* each of the six families at minimum and maximum subdivision, against a
  hand-computed vertex list;
* **64/8 accepted, 65/9 refused**, with nothing emitted on refusal — the limit,
  at its boundary;
* **the segment recurrence equals exact evaluation** at every segment, for
  several primitives. This is the `ATTRSTEP` property and the licence for the
  optimisation;
* a ring built here and a rotation computed elsewhere agree, because both use
  `SIN_Q16` — the cross-check that stops a second trigonometry law appearing;
* degenerate parameters refused, not emitted as zero-area triangles;
* **budget refusal mid-stream**: a primitive that would overflow is refused
  whole, and the vertices already emitted for *previous* primitives are
  untouched;
* emission order is deterministic and matches the reference exactly — the
  capture-CRC property.

## Randomized differential tests

**Planned, not written.** RTL against `zref::forge::prim_triangle`.

Random families and subdivisions across the whole legal range, plus a
deliberate illegal fraction. **Bias toward maximal subdivision**, because a
uniform draw over `segments` spends most of its time on small primitives and
almost never exercises the 1,024-triangle worst case that sizes everything
downstream.

Report the family and refusal mix.

## Formal properties
**Planned, not written** — a formal whole-primitive proof:

* **never a partial primitive** — for every input and every backpressure
  pattern, an accepted job emits exactly its computed vertex count, and a
  refused job emits zero. This is the ruling's rule as a safety property, and it
  is the one that matters: a half-emitted tube is geometry, not an error;
* the emitted count never exceeds `4 × MAX_SEGMENTS × MAX_SIDES`;
* acceptance is decided before the first vertex is emitted;
* handshake hygiene; reset drops in flight without committing.

## Synthesis / resource ceiling
Unbuilt. **Ceiling: 2,800 ALMs, 10 DSPs, ≤ 2 M10K.**

The DSPs are the segment/ring evaluation; the sequencing lever from
`zhao_geom_lod` applies here too and should be measured before choosing flat.

M10K is for the parameter staging only. **Zero vertex storage** — this block
generates into a stream and buffers nothing, which is what keeps it small and is
the reason it beats sending the same geometry as a mesh.

## Integration capture cases
* **a spell built from many maximal ribbons** — the 15 %-of-frame case flagged
  above, and the one that tests whether the parameter-not-geometry saving
  survives contact with real content.
* **each family in one frame**, so the emission order and material handling are
  exercised together.
* **a budget-refusal frame** — one primitive refused, the frame completes, and
  the refusal is attributable to its command.
* **sky modes and `beam_cone`** — the purpose section's named uses, built from
  billboard sheet and radial shell, confirming the ruling's collapse actually
  covers them rather than only appearing to.

## Notes

Subdivision caps are spec constants; overflow degrades gracefully.
