# §16.3 — the registered-head DONE queue, measured

`zhao_raster_rcp24_v3@v3-rh`, commit `33e004b4`, 2,897 s.
Baseline `@v3-full`, commit `7d55fa84`.

| | @v3-full | @v3-rh | change |
|---|---|---|---|
| ALM | 1,230 | **1,023** | −207 (−16.8%) |
| registers | 1,944 | **1,460** | −484 (−24.9%) |
| M10K | 6 | **8** | +2 |
| DSP | 3 | 3 | — |
| reported Fmax | 90.54 | **90.41** | −0.13 |
| worst sampled INTERNAL path | 129.18 | 114.00 | see below |

## The structural prediction held

Recorded before the fit: *"no worst path should start at `u_doneq|mem_q[..]` or
`u_doneq|head_q[..]`; if they still do, the DONE instance was not actually
swapped or the fitter flattened the wrapper back."*

`u_doneq` appears **zero** times in the new fit's path report. Before, the worst
internal path was `zhao_raster_ticketq:u_doneq|count_q[4] -> u_freeq|count_q[1]`.
The DONE queue is off the critical path entirely, and the −484 registers with
+2 M10K is the flop array becoming RAM, which is where the area went.

## And it bought no clock at all

Reported Fmax moved 90.54 → 90.41. That is the headline worth keeping, because
it is exactly what the rearchitecture brief warns about in its own words:

> **Removing the worst path is not the same as fixing the clock.**

§16.2 said to "start with the DONE queue/output seam. Do not rewrite the
multiplier first." That was the right order and it worked as an area change; it
simply was not the block's clock limiter. The reported number is gated by port
paths, which a leaf fit cannot avoid, and those did not move.

## What the 129.18 → 114.00 line does NOT say

It does **not** say the design got 15 MHz slower, and reading it that way would
be a measurement error of the kind this repository keeps a law about.

A Quartus path report samples its worst ~200 paths. `internal_paths.py` reports
the worst INTERNAL path *among those sampled*, which is a different quantity
from the worst internal path in the design. Before this change the sample was
dominated by port paths plus the `u_doneq` family; the multiplier datapath
`a0_d_q[11] -> a1_m_q[12]` need never have appeared in it. Removing the doneq
family from the top of the list lets a path that was always there become
visible.

So the honest statement is: **the worst internal path is now the multiplier
datapath at 114.00 MHz**, and whether it moved is not established by these two
numbers. Establishing it would need the same path family looked up in both
reports, not the top-of-list compared.

That the multiplier is what surfaces next is itself the expected outcome — it is
precisely the thing §16.2 said not to rewrite first, and now it is the visible
internal limiter rather than a guess.

## Status

§16.3's registered head/spare pair: **landed and measured.** Area down, DONE
queue off the critical path, four-clock rate retained (22 directed checks
including §16.3's ready-drop case, plus rcp24 V3's own 52). No clock gain at the
block boundary, and none claimed.
