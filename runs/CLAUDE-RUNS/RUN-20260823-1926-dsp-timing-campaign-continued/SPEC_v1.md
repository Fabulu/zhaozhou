# SPEC v1: The DSP and timing campaign, continued

**Run ID:** RUN-20260823-1926
**Created:** 2026-08-23 19:26 UTC+02:00
**Status:** Active
**Previous Version:** continues RUN-20260823-0934 (archived)

---

## Objective

Take the console's multiplier census from **160** to the ruled ceiling of
**85-90**, and close timing on the blocks that miss it — with every figure
measured under a constrained fit rather than estimated.

Success is not a smaller number in a report. It is that each block's demand is
**derived**, its resource cost is **measured at two or three parameter points**,
and its throughput is stated as **work per second** rather than megahertz.

---

## Scope

**In Scope:**

- `zhao_texture_tmu` 28 -> 6-9 (in flight), `zhao_terrain_normals` 18 -> 1-2,
  `zhao_terrain_project` 33 (cache-then-sequence), `zhao_geom_cull` 15,
  `zhao_geom_binner` 12
- the **Field rearchitecture**, six waves, docketed 2026-08-23 — memories where
  memories belong, registered result boundaries, a narrower isqrt recurrence
- deriving **per-frame demand for the five Field profiles**, which nobody has
  ever done
- re-measuring blocks whose rows predate the SDC fix, since their ALM figures
  are the optimistic end and their Fmax figures were never measurements
- `STATUS.md`, kept readable by the owner

**Out of Scope:**

- **any game behaviour for the particle-simulation, compositor or 2D blocks** —
  owner-reserved, recorded as such, and to be docketed rather than decided
- widescreen implementation. It is **ruled** (`VIDEO_WIDE` 384x216 from a
  384x224 canvas; `WIDE_DUO` 2 x 192x144) but deliberately not scheduled
- Nanquan / compiler work

---

## Constraints

- **Never hand-edit a measured number.** Re-run the fit and let the harness
  write the row.
- **One constrained fit at a time**, and never edit RTL in the main tree while
  one is running.
- **Verify a fit was constrained** — `Info (332111): 10.000 clk` — and save it
  as evidence before the harness deletes the workspace.
- **Re-fit the unmodified RTL under the corrected SDC first** (`@pre-rearch`),
  or the before/after compares a constrained fit against an unconstrained one.
- Allocation justified by **sustained frame demand**, never a one-clock
  placeholder. Compute budget is **1,666,667 clocks/frame**, NOT the 251,520
  raster period.
- Share arithmetic **within a subsystem only**, never console-global.
- **Preserve two or three parameterised points.** They are coverage as well as
  data: mutants exist that are catchable only at a non-default setting.
- Report **Fmax AND work/frame**. `gpu_clk` is shared, so a block meeting its
  own budget can still cap every other block.
- One implementation agent at a time; parallel only for read-only recon.

## Don't Retry

- **Do not "prove" a regression by reverting a file and re-running** — the
  rebuild clears mutant residue and the pass tracks the rebuild.
- **Do not trust `(* multstyle = "logic" *)`** — accepted and silently ignored.
- **Do not trust a directive to have changed the hardware. Measure that it
  did.** A fit reporting `ramBlocks = 0` after a memory conversion is a FAILED
  implementation even if every test passes.
- **Do not score a mutant that failed to compile as caught**, and confirm the
  preflight parsed a **non-zero** count.
- **Do not assume a stopped task stopped.** A `TaskStop`ped sweep kept rewriting
  RTL under two live fits.
- **Do not treat a contract's "throughput met" as time.** Every contract
  predates the SDC fix, so every "met" is cycles-per-item until measured.

## Open Questions

- **Per-frame demand for the five Field profiles.** If it exceeds one core, the
  answer is identical cores by rate class — three would be nine DSPs against the
  original seventy-nine — not a wider arithmetic farm.
- Owner docket, open: the `rescale_s32` narrowing in the shipped skinning
  reference; the scar-texture pool size (`SURFACE.STAMP` is pool-bound, not
  rate-bound); the earth-field WRITE law; the three-bone skinning tail and the
  weight-normalisation precondition.
- Thirty-seven blocks still have **no speed figure at all**.
