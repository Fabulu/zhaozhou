# The two projection engines are 66 of the machine's 192 DSP — the same nine multipliers, twice

2026-09-09, after the owner's ALM audit and rescue roadmaps landed. Read-only
structural measurement against `d3c855c0`; no fit, no simulation.

## What the audit gives, and what it does not say

| module | fitted ALM | DSP |
|---|---:|---:|
| `zhao_geom_project` | 6,199 | **33** |
| `zhao_terrain_project` | 6,068 | **33** |
| both | **12,267** | **66** |

The audit prices them. It does not say *why* they are identical, and that is the
part that decides whether consolidation is a rewrite or a rewiring.

## They instantiate the SAME core, and differ only in a tag rider

```
fpga/rtl/geometry/zhao_geom_project.sv:126    zhao_project_core #(.PAYLOAD_W(16))  u_core
fpga/rtl/terrain/zhao_terrain_project.sv:257  zhao_project_core #(.PAYLOAD_W(PAY_W)) u_core   // PAY_W = 42
```

`PAYLOAD_W` is a payload **carried alongside** the arithmetic, not an arithmetic
parameter. So the two engines' datapaths are not merely similar — they are the
same module, differently tagged. Sharing the source file did not share the
silicon, exactly as the rescue brief says.

## Where the 33 DSP actually is — and how my first count got it wrong

My first multiplier count returned **ZERO** for all three files, which is the
comfortable answer and was false. The pattern used `[a-zA-Z0-9_)\]]`, and `]`
cannot be escaped inside a POSIX bracket expression — the same fault that threw
`grep: Invalid range end` one command earlier. A counter that reports zero is a
broken instrument until proven otherwise; this one took thirty seconds to
disprove and would have hidden the entire finding.

The real arithmetic, in `fpga/rtl/common/zhao_project_core.sv`:

```
264:  function ... mul32 = $signed(...) * $signed(...)     <- a FUNCTION

359:  row_x  = mul32(mat[0], vx) + mul32(mat[1], vy) + mul32(mat[2], vz) + ...
361:  row_y  = mul32(mat[4], vx) + mul32(mat[5], vy) + mul32(mat[6], vz) + ...
363:  row_cw = mul32(mat[12],vx) + mul32(mat[13],vy) + mul32(mat[14],vz) + ...

599:  prod_x_c = ext32m(s5_ndc_x) * vp_w[s5_view]
600:  prod_y_c = ext32m(s5_ndc_y) * vp_h[s5_view]
```

**Nine `mul32` call sites**, three per matrix row. A function call is not shared
hardware — the same lesson the material combiner taught this repository when
`unit_mul_logic(...)` inside two seven-arm case statements became fourteen
physical multipliers.

Nine signed 32x32 products at Cyclone V's 3 DSP each is 27, plus the two
viewport scalings. **That reconstructs the measured 33 almost exactly**, which is
the check that makes this a structural account rather than a story.

## Why this is the first move

* **DSP is the hardest cap.** 192 counted against 112 physical and an owner
  target of <= 94. Collapsing two cores to one removes ~33 in a single change —
  more than any other single item, and it needs no new arithmetic.
* **The ALM saving comes with it.** ~12.3k across the pair, of which the
  duplicated core, its divider and its transport are the bulk.
* **It is a rewiring, not a redesign.** One physical core, the wider payload
  rider (42), and the two clients multiplexed. The exact arithmetic is
  untouched, so the numerical contract cannot drift.

## What this does NOT establish

**Not that one core is fast enough.** The roadmap's own workload example reaches
903,552 projections in a two-view stress against a ~1,333,333-clock window, so a
shared projector must stay near one vertex per clock. The saving must come from
deleting a duplicate provider, **not** from quietly halving throughput — and the
roadmap says so explicitly.

**Not a measured saving.** 33 DSP and ~6k ALM are structural predictions from
the instantiation graph and the multiplier count. They become real at a fit.

**Not the whole projection story.** Caching projected vertices and replaying
triangle references is the separate, larger win; the roadmap sequences it first
precisely so vertex identity and service timing do not change in one step.


---

# CORRECTION, same day: the arena is a PREREQUISITE, not a parallel win

The section above says, under "What this does NOT establish":

> **Not that one core is fast enough.** The roadmap's own workload example
> reaches 903,552 projections in a two-view stress against a ~1,333,333-clock
> window, so a shared projector must stay near one vertex per clock.

That caveat was right to exist and **the number in it was the wrong number for
this machine.** `design/budgets/workloads.yml:298` already carried the
correction, dated the same day, from owner brief §2.6.E:

> The sentence that stood here — *"120,000 vertices at one per clock is 7.2% of
> the frame, so a SHARED projector is affordable and two are not justified by
> rate"* — is **WITHDRAWN as unconditional**. The 7.2% is GEOMETRY'S projections
> alone. A shared projector would also carry terrain's, and TERRAIN.PROJECT
> projects triangle **CORNERS**: a 33x33 patch performs 6,144 projections for
> 1,089 unique lattice vertices… **The real figure is geometry PLUS terrain PLUS
> cache/replay, and that sum has not been computed.**

## The sum, computed

Against `computeClocksPerFrame = 1,666,666`:

| | projections / frame | % of budget | |
|---|---:|---:|---|
| terrain (256 patches x 6,144 corners) | 1,572,864 | | |
| geometry | 120,000 | | |
| **total, today's shape** | **1,692,864** | **101.6%** | **DOES NOT FIT** |
| terrain deduplicated (256 x 1,089 unique) | 278,784 | | |
| geometry | 120,000 | | |
| **total, with the arena** | **398,784** | **23.9%** | fits |

**5.64x of terrain's projection demand is redundant**, and one shared core is
infeasible on today's shape by 1.6% — which is not a rounding error, it is a
frame that does not close.

## What this changes

**The module is still right; its ORDERING was wrong.** The DSP arithmetic, the
shared-matrix evidence and the arbitration are untouched. What fails is the
throughput justification, and with it the adoption order:

> **Do not instantiate `zhao_project_service` in a composed top until the vertex
> arena exists.**

The rescue roadmap sequences this correctly — *"First, cache exact final vertices
and replay triangle references. Then replace two physical cores with one."* I
built the second step first and justified it with a rate argument the repository
had already withdrawn.

Neither line above includes the replay/cache cost itself. `workloads.yml:305`:
*"Any cache or replay used to avoid that re-projection is itself a cost, in
cycles and in memory ports, and none of it is in the 7.2%."* The arena's own
budget is the architect's problem and is not counted here.

## Why this happened, since the pattern is the point

I found a 33-DSP saving and then accepted the first throughput argument that let
me keep it. The repository's own budget file had refuted that argument hours
earlier, in a comment written for exactly this mistake. **A measurement that
supports the comfortable answer is the one to check hardest** — and the check
here cost one `grep` of a file I had already been quoting from.
