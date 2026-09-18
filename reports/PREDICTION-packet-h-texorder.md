# Prediction for `@packet-h-texorder`, written before the fit starts

Two changes, batched deliberately — neither was worth a fit alone, and the
sequencing note in the roadmap says why: at `@packet-h-satstage` the cache
pipe's `valid_r` (53 paths, -2.853) sat **0.056 ns** ahead of the request
queue's `h_d_q` (42 paths, -2.797), so repairing either one alone would have
reported almost nothing.

Baseline `@packet-h-satstage`: ALM 27,583, 63 DSP, 136 M10K, **gpu_clk 77.80
MHz**, worst -2.853, TNS -2,276.4.

## What is in this fit

1. **`zhao_texture_cache_pipe_v2` — compare first, select second.** The mask
   used to be built by selecting the winning lane's tag and *then* comparing
   four lanes against it, 28 bits wide, behind a LANES-deep priority chain.
   The receipt put 6.575 ns of a 12.684 ns path in exactly that span. Now a
   pairwise `(tag, idx)` table is computed from registers beside the priority
   chain and the mask is a one-hot pick from it.
2. **The island's COMBINE fence — evaluate every entry, then select.** It used
   to index a 64-wide fence with the output of `u_own`'s queue read: two
   chained array reads, 5.70 ns of a 12.277 ns path. Now four fence lookups run
   beside each other and `cmb_rp_o` selects. Removes the 1.43 ns read mux from
   the cone; the 4.27 ns of 64-wide select remains.

## Prediction

1. **`valid_r` leaves the top of the band decisively** — not by 0.5 ns but by
   most of 6.575, so it should fall below every endpoint currently listed.
2. The rq family (`h_d_q`, `s_d_q`, `lcnt_q`, `rp_q`, `h_v_q`, `s_v_q` —
   ~110 of 200 paths) improves by **about 0.9 ns**, to roughly -1.9.
3. **The new wall is `walk_q_r` at -2.056**, which neither change touches, or
   the rq family just behind it. **Worst -2.0 to -2.2, gpu_clk 82–84 MHz.**
4. **ALM goes UP, 27,800–28,200.** Cone 1 replaces four 28-bit comparators with
   a 4x4 pairwise table — sixteen, of which six are distinct — and cone 2 turns
   one 64:1 mux into four. Neither is free, and a prediction that only ever
   forecasts savings is not a prediction.
5. DSP 63 and M10K 136 unchanged; neither change touches a multiplier or an
   array.
6. TNS improves to **-800 to -1,400**.

## What would falsify the reasoning rather than the numbers

**If `valid_r` is still the worst path**, the 6.575 ns I attributed to
select-then-compare was not that at all, and the Data Arrival Path was read
wrong — the levels are named `m_tag_c[*]` and `m_mask_c[*]`, so this would be a
surprise worth understanding before any further reordering.

**If the rq family does not move at all**, Quartus has re-factored the four
parallel fence lookups back into "select then look up" — a legal
transformation, since they share structure and the tool optimises for area
unless told otherwise. That would be the more interesting outcome: it would
mean this class of fix needs a registered boundary or a synthesis attribute to
survive, not just a source reordering, and it would cast doubt on cone 1's
mechanism as well.

**If ALM rises by much more than 600**, the pairwise table is being built for
all sixteen ordered pairs rather than the six distinct ones, and the loop
should be written to exploit symmetry.
