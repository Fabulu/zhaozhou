# Composed fit — after the RMW split — 2026-09-01 (round 3)

**The best number this project has, and the offender is finally the one the
architecture note named first.**

    commit    fc6395f    device 5CSEBA6U23I7 (provisional, NOT board truth)
    tool      Quartus Prime 17.0.2 Lite
    result    PASS analysis/elaboration, synthesis, fitter, TimeQuest

## The four rounds

| | round 0 | round 1 | round 2 | **round 3** |
|---|---|---|---|---|
| change | — | modulation off the cone | Early-Z ready skid | **RMW split, 3 ways** |
| **`gpu_clk`** | 53.48 | 62.89 | 60.92 | **64.66 MHz** |
| worst setup | −8.697 | −5.902 | −6.416 | **−5.466 ns** |
| **failing endpoints** | — | 3,681 | 3,948 | **1,673** |
| ALMs | 12,569 | 12,532 | — | 12,755 |
| DSPs | 16 | 16 | 16 | **16** |
| `vid_clk` | 91.43 | 92.74 | 99.52 | 104.32 |
| `audio_clk` | 168.18 | 145.18 | 117.99 | 130.4 |

**+21 % on `gpu_clk` overall**, and **failing endpoints more than halved**.

**DSP count never moved.** Splitting `zhao_raster_blend` into `_prod` and `_fin`
could plausibly have inferred extra multipliers; it did not. The split was
structural, and the arithmetic is the same three products per channel.

Cost: **+223 ALMs** and +564 registers for the two pipeline stages. That is what
a 110-bit and a 170-bit stage cost, and it is cheap for 3.7 MHz.

## What the split actually bought, and what it did not

**It did what it was designed to do, broadly.** 3,948 → 1,673 failing endpoints
is the RMW loop leaving the failing set across the whole design.

**It did NOT halve the worst path**, and the reason is that a *different*
structure now sets it. The worst path moved out of the fragment entirely:

    zhao_raster_edgewalk | cy_r[4]  ->  zhao_raster_edgewalk | pend_r[14]

| destination block | rows in worst 100 |
|---|---|
| `zhao_raster_edgewalk` | **100** |

**All 100.** Entirely internal to EDGEWALK — no RAM, no DSP, no cross-block
path.

## The architecture note's ranking was not wrong. It was EARLY.

`reports/MHZArchitected` listed EDGEWALK **first** of five offenders. Across
four fits it has been:

| round | EDGEWALK in the worst 100 |
|---|---|
| 0 | absent (all 400 were FRAGMENT) |
| 1 | absent (90 were EARLY-Z) |
| 2 | 30 |
| **3** | **100** |

Three times this session the note was called wrong for putting EDGEWALK first.
It was not wrong — **it was describing the design after the other four offenders
were fixed**, which is the state that now exists. Each round peeled off what was
hiding it.

The lesson to carry, and it cuts both ways: **the report names what is worst
NOW; the note named what would be worst LAST.** Neither is a substitute for the
other, and the reasonable procedure was the one followed — fix what the
measurement names, and expect the prediction to become true later rather than
never.

## Next, decided by this report

`RASTER.EDGEWALK`, and now with no ambiguity — 100 of 100, a register-to-register
path inside one block. The note's own prescription for it is step 2 and step 3:

> Early-Z full detection + Edgewalk registered steps / balanced popcount *(low
> risk, broad TNS)* … then streamed Edgewalk row + cross pipelines

`cy_r -> pend_r` is the row-walk state feeding the pending-row mask, which is
exactly the "wide row + popcount" structure the note describes. **Read the path
detail before changing it** — that discipline has been right every round, and
twice it contradicted the block-level reading.

## What this fit still does not establish

* **The board.** Provisional device; synthesis and fit evidence only.
* **Game capacity.** Renderer still at TEST capacities (128 triangles, 1,024
  references), so the 3 % memory figure remains a placeholder measurement.
* **That round 2's skid is paying.** It is still in. Early-Z is now nowhere near
  the critical path, so a fit with it reverted is worth one measurement.
