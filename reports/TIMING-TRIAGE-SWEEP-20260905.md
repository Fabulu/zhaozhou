# Timing triage across every archived block report

**Date** 2026-09-05
**Tool** `tools/quartus/analyse_paths.py` over
`reports/synthesis/blockpaths/*.setup.summary.rpt`

## READ THIS BEFORE THE TABLE

**Every row is as of its own date, and the dates span three days.** The archived
reports were written whenever each block was last fitted:

```
2026-09-03  perspuv
2026-09-04  tilestore, terrain_residency_v2, aux_div6, aux_pipe, bilerp_lane,
            cache_pipe, fragrob, mosaic, palette_res, rsp_dispatch, tmu_plan
2026-09-05  texture_combine, island_top, material_combine_v1
```

So this is **not a comparison of fifteen blocks under one baseline.** It is
fifteen separate measurements taken over three days against source that has
changed in between — perspuv's RTL was modified today and is being re-fitted as
this is written, which makes its row already stale.

`CLAUDE.md` names this exact error: *"Never compare a current file to an old
measurement. Declared-versus-measured is a real check; declared-today versus
measured-a-week-ago is a different question wearing the same shape, and it
produces confident nonsense."*

**This sweep is therefore a TRIAGE HINT — where to look next — and not a
verdict on any block.** It is written down because the ranking is useful and
because running it took two minutes, not because the numbers are comparable.

## The sweep

Worst path that starts INSIDE the design, and the boundary headroom (how much
perfect boundary timing could recover before internal paths bind):

| block | worst internal | headroom | as of |
|---|---|---|---|
| `zhao_texture_material_combine_v1` | **−23.624** | −19.326 | 09-05 |
| `zhao_terrain_residency_v2` | **−6.292** | −2.561 | 09-04 |
| `zhao_texture_island_top` | −3.630 | +0.852 | 09-05 |
| `zhao_texture_mosaic` | −1.137 | +1.486 | 09-04 |
| `zhao_raster_perspuv_svc` | −0.463 | +1.732 | 09-03 *(stale)* |
| `zhao_raster_tilestore` | −0.404 | −1.694 | 09-04 |
| `zhao_texture_palette_res` | −0.198 | −0.331 | 09-04 |
| `zhao_texture_bilerp_lane` | −0.031 | −2.559 | 09-04 |
| `zhao_texture_combine` | +0.012 | −0.729 | 09-05 |
| `zhao_texture_cache_pipe` | +0.166 | −0.476 | 09-04 |
| `zhao_texture_aux_div6` | +0.291 | +1.962 | 09-04 |
| `zhao_texture_fragrob` | +0.301 | −1.190 | 09-04 |
| `zhao_texture_tmu_plan` | +0.485 | +1.779 | 09-04 |
| `zhao_texture_aux_pipe` | +0.492 | +6.207 | 09-04 |
| `zhao_texture_rsp_dispatch` | +0.983 | −1.319 | 09-04 |

## Two things worth acting on

### 1. `zhao_terrain_residency_v2` is the second-worst block in the tree

−6.292 ns internal with **negative** headroom, so its own logic binds and not
its boundary. It is not part of the texture island and nothing in today's work
touched it. As far as this sweep can tell, nobody has looked at it.

That is a lead, not a finding — its report is from 09-04 and its constraint set
has not been checked against the island's.

### 2. Most island blocks close individually while the island does not

Seven of the fifteen have POSITIVE internal slack. Of the eleven approved island
components, `cache_pipe`, `fragrob`, `tmu_plan`, `aux_pipe` and `rsp_dispatch`
all close on their own — and the composed island is −3.630.

**This is the composed-versus-leaf story in its plainest form**, and it is the
same shape as the ALM finding in `G1D-COMPOSED-ISLAND-20260905.md` inverted: for
AREA the per-block rows were a tight upper bound (2.4%), while for TIMING they
are not a bound at all. A block that closes alone can be the limit once it is
placed among ten neighbours competing for the same fabric.

`zhao_raster_rcp24_svc` is the clearest case: it is the island's critical path
and has no row of its own here at all.

## What this does not say

* Nothing about which block is "worst" in any absolute sense — three days of
  drift makes the ordering indicative only.
* Nothing about whether the constraint set is identical across these fits. The
  block flow copies one SDC, but that has not been verified per row, and
  perspuv's 82 MHz measured figure does not sit comfortably with its −0.463
  slack, which is itself a reason to distrust cross-row arithmetic here.
* Nothing about the composed console. Every row is a leaf or a single island.
