# P0-A — the RCP specimen resolves, in favour of the repository source

The rearchitecture brief of 2026-09-07 opens P0-A with a provenance gate:

> The fresh setup report names `scan_cur_q`, `e_busy`, `e_ra` and `c_m.raddr_a`.
> The source returned during this review instead exposes a round-robin `rr_q`
> scan over `c_val` and `c_pend` … they are not interchangeable source
> representations. … If the source names do not map, label the path-to-RTL
> attribution unresolved.

They map. The mismatch was in the returned source specimen, not in the fit.

## What the fitted island report actually says

`reports/synthesis/blockpaths/zhao_texture_island_top.setup.rpt`, worst
internal path:

    -3.243  -4.977  75.51  66.77
    zhao_raster_rcp24_svc:u_rcp|c_val[5]~DUPLICATE
      -> zhao_raster_rcp24_svc:u_rcp|c_m.raddr_a[0]~0_OTERM1091

The slack is the brief's own −3.243 ns and the endpoint is the brief's own
`c_m.raddr_a`. Counted in that report:

| name | occurrences |
|---|---|
| `c_val` | 28 |
| `scan_cur` | **0** |
| `e_busy` | **0** |

and `scan_cur_q`, `scan_cur`, `e_busy`, `e_ra` appear **nowhere in
`fpga/rtl/`** either — not in the RCP files, not in any file. Two of the
brief's four names are absent from the evidence; the other two are ours.

**So the specimen is `fpga/rtl/raster/zhao_raster_rcp24_svc.sv`**, and a
line-level patch aimed at `scan_cur_q`/`e_busy` would have been aimed at
hardware this repository does not contain. The brief was right to gate on this
and right not to claim the correspondence it had.

## The mapping the gate asked for: selector → context address → endpoint

`c_m` is `logic [23:0] c_m [NCTX]` (line 103) — inferred as RAM, so
`c_m.raddr_a` is its **read address port**. The path is then, in one
combinational cycle:

    rr_q (line 153)
      -> the NCTX-way priority scan over c_val[idx] && c_pend[idx]  (160-161)
      -> pick_i
      -> c_m[pick_i], c_x[pick_i], c_w[pick_i], c_ph[pick_i]        (174-179)
      -> c_m.raddr_a

`pick_i` is produced by a rotate-and-first-match loop and is consumed the same
cycle as a RAM read address and as four operand selects. That is precisely the
structure P0-B names:

> Separate selection/reservation, context reads, operand preparation and
> execution. Reserve work when selection is accepted, not several cycles later
> when execution begins.

The failing family is therefore genuine, correctly diagnosed, and attributable
to a specific source construct — the round-robin selector feeding a synchronous
RAM's address in its own cycle.

## What this does NOT establish

* **`zhao_raster_rcp24_svc` is not `zhao_raster_rcp24_v3`.** The island's
  limiter is in `svc`. The block fit running at the time of writing measures
  `rcp24_v3` and the §16.3 registered-head DONE-queue swap, which is a
  different module and a different question. Neither substitutes for the other.
* No claim is made here about how much a reservation stage would buy. The brief
  is explicit that an independent dispatch→FRAGROB family around −2 ns also
  exists, so removing this one does not deliver the clock by itself.

## Status

P0-A's specimen gate: **resolved, attribution established.** P0-B may proceed
against `zhao_raster_rcp24_svc.sv` lines 153–179.
