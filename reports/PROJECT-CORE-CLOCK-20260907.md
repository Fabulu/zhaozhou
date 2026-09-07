# `zhao_project_core` misses the clock on both lanes, and one cut may fix it

*2026-09-07. From `zhao_geom_project`'s first fit. No RTL was changed to produce
this — the previous two passes on other blocks read the source, guessed, edited
and moved the clock by 4%.*

---

## Why this block matters more than its row

`zhao_project_core` is instantiated by **both** `zhao_geom_project` and
`zhao_terrain_project`. It measured **61.09 MHz**, worst path core-to-core
inside `u_core` with no boundary to blame. **39% short of the product clock, on
two lanes at once**, and the standing goal is terrain hardware.

It is the first block in `FMAX-WHAT-ACTUALLY-LIMITS-IT-20260907.md`'s short list
that two subsystems depend on.

## The path, hop by hop

Worst path `−6.370 ns`, 15.906 ns of data delay, 28 interconnect and 28 cell
hops, launched from `s5_ndc_x`:

```
  0.000  uTco   s5_ndc_x[18]                 the launching register
  3.762  CELL   u_core|Mult9~430|resulta[16]  <-- the DSP, combinational out
  1.392  IC     u_core|Mult9~796|datac
  0.837  CELL   u_core|Mult9~796|cout        the multiply's own carry chain
  ...    (Mult9~768..816, ~0.6 ns of cin/cout)
  0.355  CELL   u_core|Mult9~816|sumout      ---- 6.611 ns cumulative ----
  0.804  IC     u_core|Add114~101|datac
  0.837  CELL   u_core|Add114~101|cout       a full carry chain
  ...    (Add114~105..21, ~0.6 ns)
  0.355  CELL   u_core|Add114~21|sumout
  0.564  IC     u_core|Add118~21|datad
  0.912  CELL   u_core|Add118~21|sumout      a second adder
  0.759  IC     u_core|scr_fx_x[7]~0|datab
  0.544  CELL   u_core|scr_fx_x[7]~0|combout saturation
  0.664  IC     u_core|scr_fx_x[7]~2|dataf
  0.098  CELL   u_core|scr_fx_x[7]~2|combout
  0.804  IC     u_core|Add123~33|datac       a third adder
```

**Multiply → add → add → saturate → add, in one cycle.** That is the viewport
`fx_mad` the block's header names, and the DSP's output register is not being
used: `Mult9~430|resulta` is a combinational cell output worth **3.762 ns**, a
quarter of the whole path.

## The proposal, with the split arithmetic done rather than assumed

Register the multiply's result so Quartus uses the DSP's own output register —
which costs no fabric, because the register is inside the DSP block already.

The per-hop numbers say where that lands:

| stage | delay | implied |
|---|---:|---:|
| `s5_ndc_x` → registered product | **6.611 ns** | ~151 MHz |
| product → `Add114` → `Add118` → saturate → `Add123` | **9.295 ns** | ~107 MHz |

**Both halves fit inside 10 ns, so one cut may be enough.** That is a real
prediction with a derivation, not a hope, and it is falsifiable: if a refit
comes back and the second half has not landed under 10 ns, the split was in the
wrong place and the remaining chain needs its own cut between `Add118` and the
saturation.

**The price is one cycle of latency in the projection core, on both lanes.**
Every consumer of `zhao_geom_project` and `zhao_terrain_project` sees it. That
is why this is written down rather than done in the same pass that measured it —
the last two times a pipeline register went in on a reading rather than a
measurement, it bought 4.2%.

## A coverage asymmetry found on the way, and it is the third of its kind

Mutating `rescale16_row` to drop its rounding term — round-half-up becoming
truncation:

| suite | checks | detects it |
|---|---:|---|
| `geom_project_directed` | 900 | **22 failed** |
| `terrain_project_directed` | 2,011 | **none** |

Both blocks instantiate the same core. The terrain suite is blind to the row
rescale's rounding because its matrices make the row products exact — its own
comment says so: *"m33 = 1 raw makes clip.w = 1, so ndc = clip << 16 exactly"*.

**This is the same failure `tess_harness.hpp` already documents by name**, which
is what makes it worth recording rather than shrugging at:

> Filling only on the height16 grid makes every parent difference EVEN, so the
> geomorph halving never has a remainder and **a truncation in place of
> round-half-up survives the whole suite** — which is exactly what a mutation
> sweep found.

Three instances now, all the same shape: a fixture whose data is too regular to
exercise the rounding it is meant to protect. `terrain_tess_normals` was fixed
today by adding void cells; this one would need terrain projection cases whose
row products are inexact. **Not fixed here** — it is a test change on a
different lane from the one being measured, and bundling it would confuse which
change moved what.

## What this does not claim

* Not that one cut reaches 100 MHz for the whole block. It addresses **this**
  path. 2,000 paths were summarised and the next families down are unexamined.
* Not that the DSP output register is free of consequences beyond latency —
  `Mult9` feeds a carry chain that may itself be retimed differently once the
  product is registered.
* Not that 61.09 MHz is the assembled figure. It is a leaf fit, and the
  classification says the worst path is internal, which makes it a floor rather
  than an artefact — but the assembled design adds real inter-block routing this
  fit does not have.
