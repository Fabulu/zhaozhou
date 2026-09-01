# Composed fit — RESOLVE capture stage — 2026-09-01 (round 6)

**84.97 MHz. +59 % overall, and 80 % of the original violation closed.**

    commit 9586602    device 5CSEBA6U23I7 (provisional, NOT board truth)

## Seven rounds

| | r0 | r1 | r2 | r3 | r4 | r5 | **r6** |
|---|---|---|---|---|---|---|---|
| change | — | modulation | +skid | RMW split | reg'd steps | −skid | **resolve capture** |
| **`gpu_clk`** | 53.48 | 62.89 | 60.92 | 64.66 | 79.22 | 80.30 | **84.97** |
| worst setup | −8.697 | −5.902 | −6.416 | −5.466 | −2.623 | −2.453 | **−1.769** |
| endpoints | — | 3,681 | 3,948 | 1,673 | 984 | 1,314 | **808** |
| ALMs | 12,569 | 12,532 | — | 12,755 | 12,794 | 12,658 | **12,698** |
| DSPs | 16 | 16 | 16 | 16 | 16 | 16 | **16** |

**Net area for the whole effort: +129 ALMs** on a 41,910-ALM device (0.3 %).
**DSP count never moved once across seven fits.**

## The RAM-launched structure is now gone from the worst paths

Rounds 3 and 6 removed the same shape twice — `RAM read -> combinational logic
-> register` — first from FRAGMENT, then from RESOLVE. That structure carries
~2 ns of M10K clock skew, and **neither the tile store nor any RAM appears in
the worst 100 any more.**

The remaining paths are fabric-to-fabric, where skew is small.

## The new worst path is round 4's own register

    edgewalk | sx1_r[1]~DUPLICATE  ->  edgewalk | pend_r[7]     -1.769 ns

| block | rows in worst 100 |
|---|---|
| `zhao_raster_edgewalk` | 71 |
| `zhao_raster_earlyz` | 29 |

`sx1_r` is **the step register added in round 4**, and its presence here is
evidence the hoist worked rather than a regression: the path used to start at
`cy_r` and run through a subtract before reaching the column trees. Now it
starts at the register, and what remains behind it is the column tree, the fill
test and `pend_r`.

`~DUPLICATE` on both endpoints means the fitter has already replicated the
register and the mask bit to spread fanout — the tool doing what it can before
anyone edits RTL.

## What is left, honestly

**1.769 ns.** Two candidates, and the report has not yet said which dominates:

* the column tree -> fill test -> `pend_r` chain that still hangs off `sx1_r`;
* Early-Z's `acc_mask_r` cone at 29 of 100.

**Read the path detail before choosing.** That discipline has been right every
round of this effort, and twice it contradicted the block-level ranking — most
recently at round 5, where "Early-Z 94 of 100" was a destination-block count and
the actual worst path was in RESOLVE.

## Still not established

* **The board.** Provisional device; synthesis and fit evidence only.
* **Game capacity.** Renderer still at TEST capacities (128 triangles, 1,024
  references).
* **100 MHz.** 84.97 is not 100, and the last stretch has not been shown to be
  reachable by datapath work alone.
