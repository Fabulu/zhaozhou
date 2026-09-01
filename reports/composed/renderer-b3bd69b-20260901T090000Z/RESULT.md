# Composed fit — Early-Z unique counting — 2026-09-01 (round 9)

**85.62 MHz — new best. +60 % overall, 81 % of the original violation closed.**

    commit b3bd69b    device 5CSEBA6U23I7 (provisional, NOT board truth)

| | r0 | r3 | r4 | r6 | r8 | **r9** |
|---|---|---|---|---|---|---|
| **`gpu_clk`** | 53.48 | 64.66 | 79.22 | 84.97 | 81.00 | **85.62** |
| worst setup | −8.697 | −5.466 | −2.623 | −1.769 | −2.345 | **−1.679** |
| **endpoints** | — | 1,673 | 984 | 808 | 586 | **430** |
| ALMs | 12,569 | 12,755 | 12,794 | 12,698 | 12,693 | **12,707** |
| DSPs | 16 | 16 | 16 | 16 | 16 | **16** |

Net area for the entire effort: **+138 ALMs**, 0.3 % of the device. DSP count
unchanged across nine fits.

## Bro's offender 3 is closed

Early-Z owned **100 of 100** worst paths at round 8. It owns **19** now.

Replacing `acc_full = &acc_mask_next` — a 256-input reduction whose result fanned
back to every mask bit's next-state mux — with

    seen       = acc_mask[address]
    new_pixel  = qualify && !seen
    round_done = new_pixel && (seen_count == 255)

did exactly what the note said it would. **Endpoints fell 586 → 430**, which is
the "large total negative slack over hundreds of endpoints" the note predicted
this structure would cause, going away.

## Every named offender in `MHZArchitected` is now accounted for

| offender | outcome |
|---|---|
| 1 EDGEWALK wide row + popcount | registered steps (r4) + CSD columns (r8) |
| 2 FRAGMENT RAM→2 multiplier layers→RAM | RMW split three ways (r3) |
| 3 EARLY-Z 256-bit feedback cone | **unique-coverage counting (r9)** |
| 4 BINNER six parallel products | **never appeared in nine fits** |
| 5 FBWRITE dynamic byte-mask | **never appeared in nine fits** |

## The remaining path, and it is EDGEWALK again

    edgewalk | sx0_r[7]~DUPLICATE  ->  edgewalk | pend_r[6]     -1.679 ns

| block | rows in worst 100 |
|---|---|
| `zhao_raster_edgewalk` | 69 |
| `zhao_raster_earlyz` | 19 |

`sx0_r` is the step register from round 4 and the CSD columns from round 8 are
already in front of it, so what is left is the **fill test → `row_cov` →
`pend_r`** tail rather than the arithmetic. That is `MHZArchitected` step 3 —
*"install the full streamed Edgewalk row and cross pipelines"* — the one
remaining step of the plan that the measurements still confirm.

## What is NOT the answer, on this evidence

**Binner and FBWRITE.** Steps 5 and 6 target blocks that have not appeared in a
single worst-100 across nine consecutive fits. Doing them would be following the
document against its own instruction to let the report decide.

**The shell.** `ShellFixes.md`'s three items are already done (two of them
improved upon — see `reports/DOCKET.md`), and none of `starve_samp`,
`starvation`, `cdc_err`, the DMA or the framer appears in this fit's worst 100
either.
