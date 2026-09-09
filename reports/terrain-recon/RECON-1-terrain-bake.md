# RECON 1 — `zhao_terrain_bake` / `zhao_terrain_bake_delta`

Facts only, for the terrain architect. All file:line verified by the recon.

## Measured rows (map-only; NO fitted row exists for either)

| | DSP | registers | blockMemoryBits | est. ALM |
|---|---:|---:|---:|---:|
| `zhao_terrain_bake` (incl. u_delta) | **17** | 1,928 | **0** | 2,324 |
| `zhao_terrain_bake_delta` | 4 | 0 (combinational) | 0 | 131 |

**Neither uses a single memory bit today.**

`design/fit_targets.yml:1453-1460` flags the 17-DSP row: fitted before targets
existed and **cannot currently be re-run** through the per-block flow, so its
provenance cannot be re-verified.

## The seven multiplier sites

`zhao_terrain_bake.sv` — five, each a distinct instance:

```
346  dx2       = dx * dx                        signed 33 x 33
347  dz2       = dz * dz                        signed 33 x 33
499  radius_sq = cmd_radius_i * cmd_radius_i    signed 32 x 32   -> 3 DSP (exact table match)
280  function lat_lerp: prod = span * num       signed 33 x 7
     called TWICE, at 334 (vx) and 335 (vz)  -> two separate multipliers
```

`zhao_terrain_bake_delta.sv` — two, and they reconcile EXACTLY:

```
64  p_from = depth_from_i * s_ext    signed 32 x 18  -> 2 DSP
65  p_to   = depth_to_i   * s_ext    signed 32 x 18  -> 2 DSP
                                                   total 4 = the measured row
```

**HONEST GAP, flagged rather than guessed:** the remaining 13 DSP cannot be split
across M1/M2/M4/M5 without a Quartus resource map, because 33x33 and 33x7 are not
in the Cyclone V cost table used. The recon refused to assert a split. Do not
invent one.

## State — and the one array

`meets_row[33]`, each `logic [32:0]` = **1,089 bits** (`bake.sv:328`).

* WRITTEN one row at a time in `StEmit` (638-639)
* **READ TWO ROWS PER CYCLE** — `meets_row[cj]` and `meets_row[cj+1]` (438-439)
  during `StCell`

The RTL's own header (83-87) states this is **kept as 1,089 flops BY DESIGN**
(choice B2), because moving it to memory would relocate the §3.4 breach equality
outside this block. That is a stated design rationale, not an oversight — the
architect must engage with it rather than assume a naive RAM swap.

The two-rows-per-cycle read is exactly the shape the roadmap's north/current/
south/prefetch row-window addresses.

No other arrays exist in either file. Everything else is scalar registers.

## What it computes

Two-phase per-stamp engine, sole writer of terrain layer B (scar) and layer D
(cell state).

* **DIG**: sweeps 33x33 lattice vertices z-then-x, places each via
  `lattice_lerp`, evaluates a paraboloid stencil through a **17-step exact
  restoring divide**, applies the incremental delta `g(from) - g(to)` from
  `bake_delta`, the no_bake corner-shadow clamp and height16 saturation rails.
* **BREACH**: sweeps 32x32 cells applying `apply_breach_law`
  (SOLID -> VOID_BREACHED when all four corners meet bottom and not no_bake;
  heal otherwise). Skipped entirely if layer C/D absent.

## Contracts and law

* Reference: `zref::terrain::bake_dig`, `apply_breach_law`, `lattice_lerp` in
  `reference/src/zterrain/terrain_core.cpp`
* Contract: `design/contracts/TERRAIN.BAKE.md` (note its SHEET SEAM section:
  `stamp_results` is ambiguously named across two wire shapes; this block takes
  the per-stamp-record form)
* Ledger: `design/blocks.yml:2750-2778`, maturity UNIT_VERIFIED
* Tests: `tests/terrain/terrain_bake_directed.cpp`, `terrain_bake_random.cpp`
* `bake_delta` has NO blocks.yml row; it is proved separately by
  `tests/formal/terrain_bake_delta.sby` (formal, telescoping-rescale identity)
* Throughput target: **1 bake texel per clock**, ready_valid, variable latency

## Integration status

Instantiated ONCE, in `fpga/rtl/prod/zhao_prod_top.sv:3587`, with all inputs
driven by a shared LFSR — the standard map-only harness pattern, **not a
functional integration**. Nothing else in `fpga/rtl` instantiates it.
