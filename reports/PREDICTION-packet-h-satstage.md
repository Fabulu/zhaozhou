# Prediction for `@packet-h-satstage`, written before the fit starts

Measured at `@packet-h-mulstage` (commit d85e2965): ALM 27,231, 63 DSP,
136 M10K, Fmax 61.08 MHz, worst -6.372, TNS -9,514.0, hold +0.194.

## What the previous fit actually established

The multiply split did exactly what it was built to do, and the headline
number hid it. At `@packet-h-uvw` **105 of the 1,600 summarised paths ended
at `base_min_y0_r`**, worst -5.144 -- the overall gating endpoint. At
`@packet-h-mulstage` that endpoint appears **zero times**. The paths did not
get faster; they left the critical set entirely.

Fmax still fell 66.03 -> 61.08 because `final_sat_r` was 0.026 ns behind
`base_min_y0_r` at uvw (-5.118 against -5.144) and degraded to -6.372 once
it stopped sharing a placement priority with the endpoint above it. That is
a placement trade, not a logic regression: nothing in this fit's closure
touches the divider.

## The change under test

Commit 73125832 moves `zhao_raster_attrdiv_v2`'s saturation test off the
input edge. `final_sat_r` and the rounded `dividend_r` are now computed from
registers captured one edge earlier, in a new `D_SAT` state.

## Prediction

1. `final_sat_r` leaves the critical set the way `base_min_y0_r` did -- zero
   paths ending there in the summary, not merely fewer.
2. `dividend_r` (29 paths, worst -3.869, all from `dndy_r`) also collapses:
   D_IDLE now loads it as pure sign-extension of `num_i`, and the round it
   used to carry moved to D_PREP behind registers.
3. If both collapse, the next wall is the texture cone at -3.949
   (`frag_expand|fragment_m_rtl_0_bypass` -> `aux|a0_input_fault_q`), which
   this change does not touch. **Worst -3.9 to -4.1, Fmax 71 to 72 MHz.**
4. ALM 27,100 to 27,600 -- the state register gains a bit and one state; the
   97-bit round moves rather than duplicates.
5. DSP 63 unchanged. M10K 136 unchanged.
6. TNS improves to somewhere between -4,500 and -7,500.

## What would falsify the reasoning rather than the numbers

If `final_sat_r` is still in the critical set, the cone does not start where
the source says it does and the split was authored against a wrong reading.
If it leaves but Fmax does not improve, then the ~-3.9 tier is not three
independent walls but one shared placement region, and the next lever is
floorplanning rather than retiming -- a materially different conclusion, and
the one that would be worth knowing.

## CORRECTION, same day, before the fit returned

The sentence above first read *"105 of the 1,600 summarised paths"*. There are
not 1,600 paths. `*.setup.rpt` holds a **Summary of Paths** table with **200**
rows, all negative, plus **Data Arrival Path** and **Data Required Path**
detail tables whose incremental-delay rows have the same column shape. My
extractor matched all three and reported 1,600, of which 1,400 were delay
increments being counted as passing paths.

The tell was a histogram with **exactly zero** paths in each of [-3,-2),
[-2,-1) and [-1,0) across three independent fits, and exactly 1,400 at or above
zero in all three. That is not a distribution; it is two tables glued together.
Precision at zero is a tell, not a result — this file's own law, fourth
instance this week, and once again the error read in the flattering direction:
*"1,400 of 1,600 paths already meet timing"* is a comfortable sentence and a
false one.

Corrected, the evidence is STRONGER, not weaker: `base_min_y0_r` was **105 of
the 200 worst paths, 52.5%**, not 6.6%.

And one thing the corrected reading makes visible that the wrong one hid:
**TNS is -9,514 ns over paths whose worst is -6.372.** Two hundred paths cannot
carry that; at the best slack shown (-3.0) it takes at least ~3,200 negative
paths. The report shows the worst 200 of some thousands, so no count in this
document is a count of the machine's negative paths — only of the window
Quartus prints.