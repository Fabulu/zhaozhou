# Composed fit — skid removed — 2026-09-01 (round 5)

**80.3 MHz**, and a decision made on reasoning in round 2 is now settled by
measurement.

    commit 7a7265a    device 5CSEBA6U23I7 (provisional, NOT board truth)

## Six rounds

| | r0 | r1 | r2 | r3 | r4 | **r5** |
|---|---|---|---|---|---|---|
| change | — | modulation off cone | +skid | RMW split ×3 | registered steps | **−skid** |
| **`gpu_clk`** | 53.48 | 62.89 | 60.92 | 64.66 | 79.22 | **80.30** |
| worst setup | −8.697 | −5.902 | −6.416 | −5.466 | −2.623 | **−2.453** |
| endpoints | — | 3,681 | 3,948 | 1,673 | 984 | 1,314 |
| ALMs | 12,569 | 12,532 | — | 12,755 | 12,794 | **12,658** |
| DSPs | 16 | 16 | 16 | 16 | 16 | **16** |

**+50 % overall. 72 % of the original violation closed** (−8.697 → −2.453 ns).

## The skid verdict: it was pure cost, and I kept it two rounds too long

Removing it **gained 1.08 MHz and returned 257 registers and 136 ALMs**.

In round 2 the skid cost 2 MHz and I kept it, arguing it was "prepaid work"
because Early-Z would otherwise be the ceiling. That argument was plausible and
**unverified**, and it stayed unverified for two rounds because I never made it
checkable. It stopped being true at round 3, when the RMW split shortened the
same ready chain independently.

**The lesson is not "the skid was wrong."** It is that a change kept on an
argument needs a scheduled re-test, and the re-test only became cheap once
round 4 changed the surrounding conditions. A decision defended rather than
measured will stay wrong quietly.

`zhao_skid2.sv` remains in the tree — proven, documented, unused. The next block
needing a ready-path cut should not rewrite it.

## Read the endpoint count carefully

**984 → 1,314 while the worst path improved.** That is not a regression: the
slack profile is flattening as the design approaches closure — fewer very bad
paths, more paths clustered just behind the worst. Quoted alone it would look
like one.

## The remaining offender

    tilestore | ram0 PORT_B_WRITE_ENABLE_REG  ->  earlyz | acc_mask_r[*]

| block | rows in worst 100 |
|---|---|
| `zhao_raster_earlyz` | **94** |
| `zhao_raster_resolve` | 6 |

This is the `floor_r`/`acc_mask_r` cone — `MHZArchitected`'s offender 3, the
"256-bit global feedback cone", now unambiguously last in the queue. Its own
prescription is full-detection restructuring so the mask is not a single global
reduction feeding a large register bank.

## Remaining gap: 2.453 ns

Still open, and still not established as pure datapath:
`gpu_clk~CLKENA0` drives ~14,000 endpoints, and the launch/latch skew measured in
round 2 was 1.995 ns — **the same order as the entire remaining violation.**
Before the next RTL surgery, the clock network deserves one measurement of its
own.
