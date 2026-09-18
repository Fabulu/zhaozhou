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

---

# RESULT: `@packet-h-texorder`, commit 709e22a8, clean tree, 89 sources

```
ALM 27,636   DSP 63   M10K 136   registers 37,578   1,592.8 s   status ok
gpu_clk 79.74 MHz   worst -2.540   TNS -2,077.4   hold +0.243
fmaxByClock: gpu_clk 79.74 | audio_clk 89.27 | vid_clk 104.87
gatingClock gpu_clk   gatingFmaxMhz 79.74   gatingPeriodNs 12.54
```

**The new receipt fields work.** `gatingFmaxMhz` reads 79.74 and agrees with
Quartus's own `gpu_clk` line to the digit, and `fmaxByClock` carries all three
domains — so this row can be read correctly without the `.sta.rpt` beside it.
This is also the first row where `fmaxClock` and `gatingClock` agree again,
because the render path is once more the slowest thing in the design.

## Scorecard: the two STRUCTURAL predictions were right, three of four NUMBERS were wrong

| # | predicted | measured | |
|---|---|---|---|
| 1 | `valid_r` leaves the band decisively | **53 paths → 0**, better than the -1.369 floor | yes |
| 2 | the rq family improves ~0.9 ns | `h_d_q` 42, `s_d_q` 37, `lcnt_q` 22, `rp_q` 7, `h_v_q`/`s_v_q` 3 — **all gone** | yes |
| 3 | worst -2.0 to -2.2, **82–84 MHz** | **-2.540, 79.74 MHz** | **missed, worse** |
| 4 | ALM 27,800–28,200 | **27,636** (+53) | **missed, better** |
| 5 | DSP 63, M10K 136 unchanged | 63, 136 | yes |
| 6 | TNS -800 to -1,400 | **-2,077** | **missed, worse** |

**164 of the 200 printed paths left the window.** Both fixes did exactly what
they were built to do, and the clock moved 1.94 MHz.

## Why the gain was small, and it is the mulstage pattern again

The wall is now a **single path** at -2.540: an `altsyncram` port-B write-enable
register in `zhao_texture_frag_expand_v2`'s fragment memory, into
`zhao_texture_binding_resolver_v2`'s `read_row_present_q`. At
`@packet-h-satstage` that same endpoint was **-2.048**. It got 0.492 ns WORSE
while everything around it improved.

That is the third time in this campaign: remove a dominant tier and a straggler
inherits the gate, degraded, because the fitter stops spending placement on it.
`base_min_y0_r` did it to `final_sat_r`, `final_sat_r` did it to the texture
band, and now the texture band has done it to a single RAM write-enable.

**Prediction 4 is the same effect seen from the other side.** I forecast ALM up
27,800–28,200 on the reasoning that both changes trade area for depth. It came
in at 27,636, +53. The pairwise table and the four parallel fence lookups cost
almost nothing — because the fitter, freed of the paths it had been fighting,
spent less elsewhere.

## The band is now flat, which changes what a fix is worth

200 paths between **-2.540 and -1.369**. There is no tier left to remove: the
worst endpoint owns ONE path, the second owns 26, and nothing owns more than 32.
**From here each fix buys a fraction of a MHz unless several land together**, and
the honest projection is that 79.74 → 100 MHz is a campaign of many small cones
rather than three more big ones.

## The next four, all named from this receipt

| slack | n | from → to |
|---:|---:|---|
| -2.540 | 1 | `fragment_m` RAM port-B write enable → `binding_resolver_v2|read_row_present_q` |
| -2.025 | 26 | `tile_pipe|plane_dndx_q[2][27]` → `attrgrad|mul_x_r[83]` |
| -1.982 | 1 | `fragment_m` RAM port-B write enable → `aux_pipe_v2|a0_input_fault_q` |
| -1.954 | 1 | `binner|d_meta_r[236]` → `attrgrad|st_r.S_IDLE` |

**`mul_x_r` at 26 paths is the largest group, and it is my own multiply split
coming back.** Having given the two multiplies their own edge, the multiply
itself is now the cost:

```systemverilog
// zhao_raster_attrgrad_v2.sv:112, with job_min_x_i declared signed [11:0]
mul_x_c = dndx_in_c * 96'(job_min_x_i);
```

**A 96x96 signed multiply in which one operand is provably 12 bits.**
SystemVerilog context-determines both operands to 96, so writing it more
narrowly does not help — the fix is to express the product explicitly at its
true width, and this repository has already done that once: D1's offender 1 was
solved with *"registered steps (r4) + CSD columns (r8)"*. A 12-bit multiplier in
CSD form is about six partial products rather than a 96-wide array.

That is the next cone, and unlike cones 1–3 it is arithmetic restructuring with
a bit-exactness obligation, so it needs the differential
(`raster_attrgrad_dsp3_diff`) watching it — which is exactly what that
differential is for and why the tree change earlier in this campaign could lean
on it.

**And cone 3 is still uncashed**: it is committed, it removes `walk_q_r`, and
`walk_q_r` is now at -1.780 — *below* all four of the above. So cone 3 alone
will not move the clock either. It goes in with whatever comes next.
