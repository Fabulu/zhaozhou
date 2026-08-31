# Composed fit — after the RASTER.FRAGMENT surgery — 2026-08-31

**The first timing result this project has that moved a number on purpose.**
One change, chosen by measurement rather than prediction, and the composed
`gpu_clk` went from 53.48 MHz to **62.89 MHz**.

    commit    a9aeb07 (contains c23a5ef, the FRAGMENT change, verified in the
              staged source before the fit was trusted)
    device    5CSEBA6U23I7, Cyclone V, provisional target -- NOT board truth
    tool      Quartus Prime 17.0.2 Build 602, Lite Edition
    stages    map 5.7 min, fitter 49.5 min, TimeQuest complete
    result    Fitter successful, 0 errors, 2 warnings

## Before and after

| | `f8c2b32` (before) | `a9aeb07` (after) | delta |
|---|---|---|---|
| **`gpu_clk` Fmax** | **53.48 MHz** | **62.89 MHz** | **+17.6 %** |
| worst setup slack | −8.697 ns | −5.902 ns | **2.795 ns recovered** |
| setup TNS | −6,566.2 ns | −5,214.6 ns | **1,351.6 ns recovered (20.6 %)** |
| failing endpoints | not recorded | 3,681 | — |
| ALMs | 12,569 (30 %) | **12,532 (30 %)** | −37 |
| block memory bits | 184,256 (3 %) | 184,256 (3 %) | 0 |
| RAM blocks | 26 | 26 | 0 |
| DSP blocks | 16 | 16 | 0 |
| `vid_clk` | 91.43 MHz | 92.74 MHz | +1.31 |
| `audio_clk` | 168.18 MHz | 145.18 MHz | **−23.0** |
| hold | all positive | all positive (0.256 ns worst) | — |

**`audio_clk` lost 23 MHz and it does not matter, but it is recorded rather than
omitted.** Its target is 40 ns (25 MHz) and it closes with ~34 ns of slack; a
clock with that much headroom moves freely with placement and this is noise, not
a regression to chase. Saying so is different from not mentioning it.

**Area went slightly DOWN.** The change swapped which values the same 32 bits of
register hold and removed a multiplier layer from a combinational cone. It was
expected to be area-neutral and it was.

## What the change was

`c23a5ef`. `RASTER.FRAGMENT` stage 1 held the four raw lanes — `s1_trgb_r`,
`s1_vrgb_r`, `s1_ta_r`, `s1_va_r` — and did **two dependent multiplies in one
clock**: `unit_mul` for the texture×vertex modulation, then
`zhao_raster_blend`'s own product, then rounding, accumulation, saturation and a
64-bit tile-store write.

The modulation did not need to be there. Every register it reads, **including
`s1_state_r` where its own enables come from**, is written by the same `s0 -> s1`
transfer — and that transfer was doing nothing but copying registers. So the
modulation now happens *at* the transfer, and stage 1 holds the finished
`s1_src_rgb_r` / `s1_src_a_r`.

Same `unit_mul`, same operands, same single rounding, same widths, **no added
latency, no protocol change, no arithmetic change.** The architecture rule —
*latency may grow; initiation rate and exact arithmetic may not regress* — holds
on every clause, and the exactness was proved by test before this fit was run:
97 directed checks, 10,509 randomised writes across all four blend modes, plus
`render_pipe_directed` and 757 `shell_golden` checks.

## The next offender, NAMED

The previous fit put **all 400** worst paths in FRAGMENT. This one puts
**90 of 100 in Early-Z**:

    zhao_raster_tilestore | ram_block1a0~PORT_B_WRITE_ENABLE_REG
      ->  zhao_raster_earlyz | acc_mask_r[87]        -5.902 ns
      ->  zhao_raster_earlyz | acc_mask_r[219]       -5.900 ns
      ->  ... 88 more, each a DIFFERENT bit of the same 256-bit mask

| destination block | rows in the worst 100 |
|---|---|
| `zhao_raster_earlyz` (`acc_mask_r`) | **90** |
| `zhao_raster_tilestore` | 10 |

**This is the second time the report has contradicted the prediction.**
`reports/MHZArchitected` ranked EDGEWALK first and Early-Z third; EDGEWALK has
now been absent from the worst paths in two consecutive fits, while Early-Z has
risen to the top. The instruction to let the report decide keeps being the
correct one.

### Read the path before rewriting anything

    Clock Skew  -2.407 ns      Data Delay  13.435 ns

**Roughly 2.4 ns of the 5.9 ns violation is SKEW, not logic depth.** The fit log
also shows `gpu_clk~CLKENA0 with 13398 fanout uses global clock CLKCTRL_G3`.

So the obvious reading — "it is the 256-input `&acc_mask_next` reduction" — is
**at best half the story, and possibly the wrong half**. The reduction is on the
*output* side of `acc_mask_r`; these paths *end* at the mask bits. What is long
is the fan-in: the tile-store RAM's write-enable register reaching the qualify
logic and from there all 256 mask bits.

Shortening the AND tree cannot recover skew. The next step is to read what
actually feeds `hiz_qualify` and why it depends on a tile-store write enable —
**not** to start rewriting the reduction because its name matches the
architecture note's description.

## What this fit still does not tell us

* **Whether 100 MHz is reachable by this route.** 62.89 against 100 is still
  1.59× short with −5,215 ns of TNS across 3,681 endpoints. This is broad, not
  one path.
* **Anything about the board.** 5CSEBA6U23I7 is provisional. Synthesis and fit
  evidence, never to be quoted as programmed-board evidence.
* **Anything at game capacity.** The renderer is still at TEST capacity — 128
  triangles, 1,024 references. 3 % block memory remains a number to be
  suspicious of, not pleased by.
