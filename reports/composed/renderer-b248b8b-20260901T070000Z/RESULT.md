# Composed fit — CSD columns — 2026-09-01 (round 8)

**A mixed result that has to be read by the paths, not by the headline.**

    gpu_clk       84.97 -> 81.00 MHz    -3.97
    worst setup   -1.769 -> -2.345 ns
    endpoints     808 -> 586            -27 %
    DSPs          16 -> 16              the round-7 failure did NOT recur
    ALMs          12,698 -> 12,693

## The change did exactly what it was aimed at

**EDGEWALK is gone from the worst 100 entirely.** It owned **71 of 100** at
round 6 and owns **0** now. Endpoints fell 27 %. Canonical signed-digit columns
removed the block from the failing set.

**And the DSP count held at 16**, which was the primary thing under test after
round 7 inferred twelve of them from `gi * sx`. The explicit shift form cannot
be misread, and was not.

## The Fmax fell for a different reason

    earlyz | floor_r[0]  ->  earlyz | acc_mask_r[66]     -2.345 ns

| block | rows in worst 100 |
|---|---|
| `zhao_raster_earlyz` | **100** |

Early-Z was 29 of 100 at round 6, behind EDGEWALK. With EDGEWALK removed it is
the sole limiter — and in this placement its cone came out at −2.345 rather than
the ~−1.7 it must have been sitting at before.

**So the −3.97 MHz is Early-Z's path placing worse, not the columns hurting.**
That is a claim the path list supports directly rather than an argument: the
block the change touched left the list completely.

## Which number is the honest one

**84.97 MHz (round 6) remains the best MEASURED figure**, and it is what should
be quoted until something beats it. Round 8's configuration has a better
endpoint profile and a worse Fmax.

The two are not contradictory — 586 failing endpoints with a worse worst-path
means the distribution tightened while one path got unlucky. This machine's
placement noise is real and has been measured: `audio_clk` moved −27 MHz and
+20 MHz between fits with no change to it at all, and it moved again here
(131.56 → 111.27).

**The change is KEPT** on the strength of the path evidence, not the headline —
but unlike round 2's skid, this is a measurement (71 → 0 rows) rather than a
prediction about the future.

## Next, and it is the last named offender

`MHZArchitected` step 2a: **Early-Z full detection**. Its offender 3 is "a
256-bit global feedback cone", and `floor_r -> acc_mask_r` is exactly that —
the hierarchical-Z floor feeding the accumulator mask, now the only thing left
in the worst 100.

Every other named offender has been dealt with:

| note's offender | outcome |
|---|---|
| 1 EDGEWALK wide row + popcount | registered steps (r4) + CSD columns (r8) — **gone from the list** |
| 2 FRAGMENT RAM→2 multiplier layers→RAM | RMW split three ways (r3) — **gone** |
| 3 EARLY-Z 256-bit feedback cone | **the only one left** |
| 4 BINNER six parallel products | never appeared in any fit's worst 100 |
| 5 FBWRITE dynamic byte-mask | never appeared in any fit's worst 100 |
