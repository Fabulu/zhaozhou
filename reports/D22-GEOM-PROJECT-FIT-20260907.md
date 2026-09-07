# D22 step 4 — GEOM.PROJECT measured, and my own gate measured the wrong scope

*2026-09-07. `zhao_geom_project` had never been fitted and had no entry in
`design/fit_targets.yml`; sixteen of the twenty-four geometry blocks are still
in that position. The target was written with its prediction stated before the
fit, around the claim the block's own header makes.*

---

## The claim under test

> **THE DUPLICATION IS GONE. THIS BLOCK IS NOW A THIN SHELL.**

It used to hold a complete copy of `project_vertex` — the same helpers, the same
31-stage restoring recurrence, the same viewport `fx_mad` — that
`zhao_terrain_project` also held, and its header called that *"A COST, NOT A
FEATURE"*.

## The result

```
zhao_geom_project     5,977 ALM   6,570 reg   27 M10K   3,095 bits   33 DSP   61.09 MHz
zhao_terrain_project  6,068 ALM   6,685 reg   23 M10K   3,822 bits   33 DSP   (no fmax)
```

**98.5% of the ALMs, 98.3% of the registers, and identically 33 DSP.** The two
blocks fit to the same size.

## The header is right, and the fit alone would have said it was wrong

The entity census separates the two things the header's two sentences claim:

| entity | ALUT (self) | reg (self) | mem bits | DSP |
|---|---:|---:|---:|---:|
| `zhao_geom_project` **itself** | **41** | **32** | 0 | 0 |
| `zhao_project_core:u_core` | 7,793 | 5,453 | 3,095 | 33 |

**The shell is thin — 41 ALUTs, 0% of the block.** *"This block is now a thin
shell"* is confirmed, and confirmed by the only instrument that could confirm
it, because the fit total says 5,977 ALM and would have read as a refutation.

**"The duplication is gone" is a different claim and this fit cannot settle
it.** Each block instantiates its own `zhao_project_core`. Sharing a module
*definition* removes a copy of the source; it does not remove a copy of the
hardware. In the assembled console there are still two cores unless they are
time-shared, and no leaf fit of either block can see that. What the fit does
establish is that the core is one core's worth of silicon in each — 33 DSP on
both sides, not 66 on one.

## MY GATE WAS WRONG, AND WRONG IN AN UNSATISFIABLE WAY

I wrote `max_m10k: 0` and called it *"the sharpest line in the entry — a shell
that owns no storage cannot have any."* It measured **27**.

The census says exactly why, and the error is mine rather than the design's: I
attributed `zhao_terrain_project`'s 23 M10K to *"its triangle framing"*, and
they are not framing at all — **the storage belongs to the core** (3,095 bits,
27 M10K, every one of them inside `u_core`). The shell really does own none.

So the rule was a statement about the **shell** applied to a fit that
necessarily contains the **core**. It could not have passed however thin the
shell became. That is the same defect `fit_targets.yml` already records twice
in this file — `min_m10k: 17` derived from a capacity argument that ignored
MLAB, and `min_memory_bits` counting one of the device's two memory kinds — and
it is the third instance: **a rule that a correct design cannot satisfy.**

The lesson is not "be less strict". It is that a per-block fit measures a
CLOSURE, and a rule about one entity inside that closure has to be checked
against the entity census, not the fit total.

## And the shared core is 39% short of the product clock

`61.09 MHz`, and the worst path is core-to-core **inside `u_core`** — no
boundary to blame, by `split_setup_paths.py`'s classification.

That matters more than the area question. `zhao_project_core` is instantiated
by both the geometry and the terrain projection paths, so **one block's clock
problem is on both lanes at once**, and the standing goal is terrain hardware.
It joins the four blocks already recorded as genuinely short in
`FMAX-WHAT-ACTUALLY-LIMITS-IT-20260907.md`, and it is the first of them that
two subsystems depend on.

## What is now known that was not

* GEOM.PROJECT has a measured row at last, and D22 step 4 has evidence rather
  than an assertion.
* The shell claim is **true**, established by census.
* The sharing claim is **unmeasurable by leaf fit** and needs either a composed
  fit containing both projectors or a decision to time-share one core.
* The projection core misses the product clock by 39%, on two lanes.
