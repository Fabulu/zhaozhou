# The gpu_clk placement noise floor, measured

**2026-09-02. 4.39 MHz of spread from the placement seed alone, on identical
RTL.** Read this before believing any single-fit comparison in this project.

## The measurement

One commit (`aa9aba2`, the scanout line-base change), fitted twice. Identical
sources -- verified by the per-file sha256 manifest, not assumed -- identical
device, QSF and SDC. The only difference is `SEED`:

| seed | gpu_clk Fmax | worst slack | top path owner |
| ---: | ---: | ---: | --- |
| 1 | 91.31 MHz | -0.952 ns | EDGEWALK 49, mem_guard 21, Early-Z 19 |
| 2 | **95.70 MHz** | -0.449 ns | **Early-Z 96**, cmd_dma 2, tilestore 2 |

**Spread: 4.39 MHz / 0.503 ns.** And that is from two samples, so it is a LOWER
BOUND on the true variance, not an estimate of it.

## What this invalidates

I quoted a "~1.5 MHz noise floor" through fifteen rounds **without ever
measuring it.** It was extrapolated from an `audio_clk` observation -- a
differently constrained domain -- and then reused as a `gpu_clk` figure. Every
"this gain is real / that one is noise" call in the series rested on it.

Re-read against ±4.4 MHz:

| round | reported | honest reading |
| --- | --- | --- |
| 11 | "+2.32 MHz" | inside noise on the headline |
| 12 | "+0.82, within noise" | right, but by luck |
| 13 | "+5.85 MHz, well above noise" | **marginal**, not the clean win claimed |
| 14 | "flat" | right |
| 15 | "4.16 MHz REGRESSION" | **not a regression at all** -- a bad draw |

**The owner table is seed-sensitive too**, which is the more damaging half.
Round 14's headline conclusion -- "EDGEWALK now passes, mem_guard is the
limiter" -- was a property of that PLACEMENT, not of the design. The same RTL
at seed 2 puts Early-Z on 96 of 100 paths and EDGEWALK nowhere.

## What still stands

* **53.48 -> ~95 MHz.** Far outside any plausible noise band. The pass worked.
* **Structural counts with a known mechanism.** "DSP-launched paths 48 -> 0"
  followed a change that provably removed `cross_r[47]` from the multiplier's
  cone, and "M10K-launched 0" likewise. A count that goes to zero because a
  specific structure was deleted is not a placement artifact.
* **Bit-exactness.** Every CRC and reference-model check is placement-independent.

## The rule going forward

1. **No architectural decision from a single fit.** Two seeds minimum, three
   before anything is reverted or declared a win.
2. **Quote a range, never a point.** "95.5 +/- 4.4" is honest; "95.47" implies a
   precision the flow does not have.
3. **Prefer structural evidence to the headline.** Owner tables move with
   placement; a path count that collapses because its structure was removed
   does not.
4. **`SEED 1` stays pinned in the committed QSF.** Comparability across rounds
   is still worth having -- it just is not the same thing as accuracy.
   `run_shell_fit.ps1 -Seed N` overrides the staged copy only.

## Why this went unnoticed for fifteen rounds

Every round changed RTL *and* re-placed, so variance and effect were never
separated. The series looked consistent because it was mostly moving in the
right direction for real structural reasons -- which made the numbers feel
trustworthy and stopped the question being asked. A measured number feels like
evidence, so it stops getting questioned: the project's own art law, in the
timing lane.
