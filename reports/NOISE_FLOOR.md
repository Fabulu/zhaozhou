# The gpu_clk placement noise floor, measured

**2026-09-02. 4.39 MHz of spread from the placement seed alone, on identical
RTL.** Read this before believing any single-fit comparison in this project.

## The measurement

One commit (`aa9aba2`, the scanout line-base change), fitted twice. Identical
sources -- verified by the per-file sha256 manifest, not assumed -- identical
device, QSF and SDC. The only difference is `SEED`:

| seed | gpu_clk Fmax | worst slack | dominant owner by count |
| ---: | ---: | ---: | --- |
| 1 | 91.31 MHz | -0.952 ns | EDGEWALK 49, mem_guard 21, Early-Z 19 |
| 2 | 95.70 MHz | -0.449 ns | Early-Z 96 |
| 3 | **95.92 MHz** | -0.425 ns | Early-Z 78, cmd_dma 18 |

**Spread 4.61 MHz / 0.527 ns across three samples.**

Seeds 2 and 3 agree to within **0.22 MHz**; seed 1 sits 4.4 MHz below both.
So the distribution is not symmetric noise around a mean -- it is a fairly
repeatable result with an occasional bad draw. The commit's honest Fmax is
**~95.8 MHz**, and round 15's 91.31 was one unlucky placement that I reported
as a regression.

Two samples would have been enough to know round 15 was a draw. Three were
needed to know WHICH value is typical.

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
4. **Rank owners by WORST SLACK, never by path count.** Seed 3 makes the case
   on its own:

       zhao_vram_arbiter    -0.425      4 paths   <- sets Fmax
       zhao_cmd_dma         -0.349     18
       zhao_raster_earlyz   -0.258     78 paths   <- 0.167 ns in hand

   Early-Z owned 78 of 100 and was not the limiter at all; fixing it would
   have bought nothing. `owners.txt` ranked by count for fifteen rounds and
   pointed at the wrong block. It now ranks by worst slack and prints a
   `limiter:` line. Count is a decoy -- the same failure as before, a number
   that looks like evidence while measuring the wrong thing.
5. **`SEED 1` stays pinned in the committed QSF.** Comparability across rounds
   is still worth having -- it just is not the same thing as accuracy.
   `run_shell_fit.ps1 -Seed N` overrides the staged copy only.

## Why this went unnoticed for fifteen rounds

Every round changed RTL *and* re-placed, so variance and effect were never
separated. The series looked consistent because it was mostly moving in the
right direction for real structural reasons -- which made the numbers feel
trustworthy and stopped the question being asked. A measured number feels like
evidence, so it stops getting questioned: the project's own art law, in the
timing lane.
