# Composed fit — EDGEWALK registered steps — 2026-09-01 (round 4)

**79.22 MHz.** The largest single gain of the effort: **+14.56 MHz** from one
change, and **+48 %** since the first composed fit.

    commit    9a9565f    device 5CSEBA6U23I7 (provisional, NOT board truth)
    tool      Quartus Prime 17.0.2 Lite

## Five rounds

| | r0 | r1 | r2 | r3 | **r4** |
|---|---|---|---|---|---|
| change | — | modulation off cone | Early-Z skid | RMW split ×3 | **registered steps** |
| **`gpu_clk`** | 53.48 | 62.89 | 60.92 | 64.66 | **79.22 MHz** |
| worst setup | −8.697 | −5.902 | −6.416 | −5.466 | **−2.623 ns** |
| endpoints | — | 3,681 | 3,948 | 1,673 | **984** |
| ALMs | 12,569 | 12,532 | — | 12,755 | 12,794 |
| DSPs | 16 | 16 | 16 | 16 | **16** |

**About 70 % of the original violation is gone** — −8.697 ns to −2.623 ns. The
remaining gap to 100 MHz is 2.623 ns.

DSP count has never moved across five rounds. Total area cost of all the
pipelining: **+225 ALMs** on a 41,910-ALM device.

## Why this one was worth so much

The walk was recomputing `sx0 = -(cy_r - by_r)` — a **triangle-invariant** —
from the vertex registers on **every row**, with all sixteen columns' shift-add
trees hanging off it. Latching the six steps and three top-left bits once per
triangle removed a four-deep adder chain from a path that owned **100 of the
worst 100**.

`reports/MHZArchitected` called this "low risk, broad TNS". It was right on both
counts, and the endpoint count (1,673 → 984) shows the "broad" half.

## The new owner: EARLY-Z, back again

    zhao_raster_earlyz | floor_r[21]~DUPLICATE  ->  acc_mask_r[98]    -2.623 ns

| block | rows in worst 100 |
|---|---|
| `zhao_raster_earlyz` | **85** |
| `zhao_raster_resolve` | 14 |

**This is not the same Early-Z path as round 1.** That one was a READY chain
arriving from the tile store two blocks away, and the round-2 skid removed it —
correctly, as the disappearance of that signature confirms. This is
`floor_r -> acc_mask_r`: the hierarchical-Z **floor feeding the accumulator
mask**, entirely internal.

So this IS the "256-bit global feedback cone" `MHZArchitected` describes as
offender 3, arriving now that the things in front of it are gone. Note
`~DUPLICATE` in the source name: the fitter has already replicated `floor_r` to
fan it out, which is the tool saying the fanout is the problem before anyone
reads the RTL.

**`RASTER.RESOLVE` at 14 is new** — it has never been near the critical path in
any previous round.

## The skid buffer question is now answerable

Round 2's skid cost ~2 MHz and was kept because Early-Z would otherwise have
become the ceiling. Early-Z **is** the ceiling again, by a different path, so
the skid is no longer buying what it was kept for. **A fit with it reverted is
now a cheap, well-posed measurement** rather than a guess.

## Still not established

* **The board.** Provisional device; synthesis and fit evidence only.
* **Game capacity.** Renderer still at TEST capacities (128 triangles, 1,024
  references).
* **That 100 MHz is reachable without touching the clock network.**
  `gpu_clk~CLKENA0` still drives ~14,000 endpoints, and 2.623 ns of remaining
  violation is the same order as the launch/latch skew measured in round 2.
