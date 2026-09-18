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
## SECOND CORRECTION: "appears zero times" has a floor under it

`*.setup.rpt` prints the worst 200 paths, so an endpoint's absence means *"all
its paths are now better than the printed floor"*, not *"it has no paths"*. The
floors are **-3.293 at `@packet-h-uvw`** and **-3.123 at `@packet-h-mulstage`**,
and they move precisely because the worst tier shrank.

So the honest form of the mul-split result is a BOUND, and it is still a strong
one: `base_min_y0_r`'s worst path went from **-5.144 to better than -3.123**, an
improvement of **at least 2.021 ns**, with all 105 of its paths carried along.
What cannot be said from this receipt is by how much more.

The same correction applies in the other direction and matters more for reading
the next fit: endpoints marked NEW at `@packet-h-mulstage` — `dividend_r` with
22 paths, `walk_q_r` with 32, `metadata_genmis_base_q` with 29 — were very
probably present at `@packet-h-uvw` too, sitting just under its -3.293 floor.
**Do not read them as a regression the mul split caused.** `tools/budget/setup_path_census.py`
now prints both floors on every comparison so this cannot be misread again.

---

# RESULT: `@packet-h-satstage`, commit e24b9e4f, clean tree, 97 sources

```
ALM 27,583   DSP 63   M10K 136   registers 37,510   1,580.7 s   status ok
gpu_clk   77.80 MHz   worst -2.853   TNS -2,276.4   hold +0.086
audio_clk 72.44 MHz   vid_clk 110.35 MHz
```

## Scorecard against what was written before the fit

| # | predicted | measured | |
|---|---|---|---|
| 1 | `final_sat_r` leaves the critical set | 3 paths at -6.372 → none above -1.623 | **yes** |
| 2 | `dividend_r` collapses too | 22 + 7 duplicated paths → none; `neg_r` too | **yes** |
| 3 | worst -3.9 to -4.1, **71–72 MHz** | worst **-2.853**, gpu_clk **77.80 MHz** | better |
| 4 | ALM 27,100–27,600 | **27,583** | yes |
| 5 | DSP 63, M10K 136 unchanged | 63, 136 | yes |
| 6 | TNS -4,500 to -7,500 | **-2,276** | better |

Both "better" rows are the same effect, and it is the one the falsification
clause named: *"if it leaves but Fmax does not improve, then the ~-3.9 tier is
not three independent walls but one shared placement region."* It leaves AND
Fmax improves — so the tier was **both**. The predicted next wall (the texture
cone at -3.949) did become the wall, but at **-2.853**, because it had been
carrying placement pressure from the attrdiv cone above it. The reasoning was
right and the arithmetic was conservative.

## THE HEADLINE `fmaxMhz` FIELD NOW NAMES THE WRONG CLOCK

The row reports `fmaxMhz: 72.44, fmaxClock: audio_clk`. **The render clock is
77.80 MHz.** All 200 paths in the printed window are `gpu_clk → gpu_clk` at a
10.000 ns relationship; `audio_clk` and `vid_clk` own **zero** negative paths.

`run_block_fit.ps1:1074` takes the FIRST row of Quartus's Fmax Summary, which is
sorted ascending — the slowest clock in the design, *regardless of its
constraint*. For a single-clock leaf that is exactly right. For a composed
multi-clock top it silently changes meaning the moment the ranking flips, which
is what happened here: the render path finally overtook a domain nobody was
working on.

`audio_clk` is not a new wall. Across four fits it reads 90.41 → 103.21 → 96.47
→ 72.44 while no commit in that range touches the audio domain — a 30 MHz swing
that is the fitter spending placement where the constraints are, and it is
free to do that because `audio_clk` has no negative slack at any of the four.

**The hazard is that `min_fmax_mhz` is checked against this field**
(`fit_rules.ps1:67`). No rule binds `zhao_shell_top_v2`, and labelled rows are
never rule-checked, so nothing is currently mis-gated — but eleven tops in
`design/fit_targets.yml` carry `min_fmax_mhz: 100`, and any of them that is
multi-clock would be judged on whichever clock is slowest in absolute terms
rather than on whether the design meets its constraint. The sound metric is the
worst-slack clock's achieved Fmax, `1 / (relationship + setupSlackNs)`, which
for this row is `1 / (10.000 - 2.853) ns = 77.80 MHz` and agrees with Quartus's
own `gpu_clk` line exactly.

## Where the render path now stands

```
@packet-h-m10k      29,044 ALM   54.12 MHz   TNS -29,689
@packet-h-timing    28,959       61.52       TNS -19,985
@packet-h-uvw       27,601       66.03       TNS  -8,851
@packet-h-mulstage  27,231       61.08       TNS  -9,514
@packet-h-satstage  27,583       77.80       TNS  -2,276
```

Across the campaign: **ALM -1,461, gpu_clk +43.8%, TNS -92.3%**, 2,417 ALM
inside the 30,000 budget, 63 of 112 DSP, 136 of 553 M10K. Against the ruled
100 MHz the gap is **22.2 MHz and -2,276 ns of TNS**, and the remaining band is
texture: `zhao_texture_cache_pipe_v2` (`c2_tag` → `valid_r`) and
`zhao_texture_v3own` (`cq_own_q` → `zhao_texture_v3rq`'s `h_d_q` / `s_d_q` /
`lcnt_q`) own the worst paths, with data delays of 11.9–12.7 ns against 10.000.

And the standing caveat from the closure finding applies to every number above:
**this is the render BACK end.** The geometry front end is declared in the
closure and elaborates nowhere.
