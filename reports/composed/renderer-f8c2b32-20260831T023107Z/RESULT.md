# Composed fit — shell + render path — 2026-08-31

**The first completed composed fit this project has had.** Analysis,
elaboration, synthesis, fitter and TimeQuest all ran to completion, and the
result is a genuine mixed verdict: **the design FITS comfortably in area and
does NOT close timing at 100 MHz.**

    commit    f8c2b32 (QSF and RTL), run in place in fpga/quartus/shell_fit
    device    5CSEBA6U23I7, Cyclone V, provisional target -- NOT board truth
    tool      Quartus Prime 17.0.2 Build 602, Lite Edition
    stages    map 5.2 min, fitter 60.9 min, TimeQuest complete
    result    Fitter successful, 0 errors, 2 warnings

## Area — comfortable

| resource | used | available | % |
|---|---|---|---|
| ALMs | **12,569** | 41,910 | **30 %** |
| ALMs containing virtual pins | 1,608 | 41,910 | 4 % |
| registers | 14,252 | — | — |
| block memory bits | **184,256** | 5,662,720 | **3 %** |
| RAM blocks | 26 | 553 | 5 % |
| DSP blocks | **16** | 112 | **14 %** |
| PLLs | 0 | 6 | 0 % |
| real pins | 0 | 314 | 0 % |
| virtual pins | 3,214 | — | — |

**Read the ALM figure with its caveat.** Quartus warns that ALMs holding virtual
pins count toward utilisation; 1,608 of the 12,569 are those. The composed
machine on a real board has no virtual pins, so the honest range is roughly
**11,000–12,600 ALMs, 26–30 %**.

**3 % of block memory is the number to be suspicious of, not pleased by.** The
binner arena, tile store and line buffers are all here, and 184 Kbit is small
for that. It is consistent with the renderer's capacities still being the TEST
capacities — 128 triangles, 1,024 references — which
`reports/BINNER_CAPACITY_FOR_8KM_MAPS.md` already says are two orders short of a
real army. **This fit is not evidence that memory is affordable at game
capacity.** It measures the machine as configured, and the configuration is a
placeholder.

## Timing — does not close

    Slow 1100mV 100C model, Fmax

    gpu_clk      53.48 MHz     <-- target 100 MHz
    vid_clk      91.43 MHz
    audio_clk   168.18 MHz

    Setup slack

    gpu_clk      -8.697 ns   TNS -6566.237
    vid_clk       1.099 ns   TNS 0.000
    audio_clk    34.054 ns   TNS 0.000

    Hold slack   all positive (0.091 .. 0.443 ns)

**`gpu_clk` reaches 53.48 MHz against a 100 MHz target — 1.87x short**, with
−6,566 ns of total negative slack spread across many endpoints. That is not one
unlucky path; it is a broad failure, and a TNS that large means the fix is
architectural rather than a placement seed.

`vid_clk` and `audio_clk` close. Hold closes everywhere.

## What this means, stated carefully

**It fits, and it is too slow.** Those are separate results and both are real.

The 10 % reserve the charter asks for at 100 MHz is not merely missed — the
design is running at roughly half the target frequency. Wave A's closing item
was always "does the composed machine coexist at 100 MHz with reserve", and the
answer measured today is **no, not yet**.

What this fit does NOT tell us:

* **Where the critical paths are.** TNS of −6,566 ns says "many endpoints", not
  which. The next step is the Timing Closure Recommendations panel and the worst
  100 paths, not a guess.
* **Whether it is the renderer.** The render path was added to this cone for the
  first time today. This fit has no before/after: the previous shell-only fit
  never completed, so **there is no baseline to attribute the shortfall to.**
  Producing one — the same commit with the render client disabled — is the
  cheapest next measurement and should come before any optimisation.
* **Anything about the board.** 5CSEBA6U23I7 is a provisional target. This is
  synthesis and fit evidence, not programmed-board evidence, and it must not be
  quoted as the latter.
* **Anything at game capacity.** See the memory note above.

## Next, in order

1. **A shell-only baseline fit** at the same commit, so the render path's cost
   in ALMs and in Fmax is attributable rather than assumed.
2. **Read the Timing Closure Recommendations panel** and the worst paths. The
   `gpu_clk` TNS is broad; the panel names the structures.
3. **Only then** consider pipelining. `RASTER.ATTRSTEP`, `RASTER.TOON` and the
   TMU all have measured throughput headroom, so adding pipeline stages to the
   worst paths is likely affordable — but which paths is a measurement, not a
   guess, and this project has already paid three times today for reasoning
   about a number instead of reading the table that names it.
