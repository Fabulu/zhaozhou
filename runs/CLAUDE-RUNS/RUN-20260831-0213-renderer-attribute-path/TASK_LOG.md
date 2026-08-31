# TASK_LOG — RUN-20260831-0213-renderer-attribute-path

The renderer's attribute path: build it, compose it, and price it against a
frame. Continued from a prior context; this folder was created part-way through,
so the entries before it are reconstructed from the commits they produced.

## What landed

| commit | what |
|---|---|
| `f5350af` | RASTER.ATTRDIV — the divide, exact rounding, 36 clocks measured |
| `9764d2e` | RASTER.ATTRDIV.SVC — tagged service, UNITS sweep, in-order retire |
| `25d96d2` | RASTER.INTERP — the plane stepped onto pixel CENTRES |
| `401bac5` | GEOM.CLIP — attributes follow their vertices through the winding flip |
| `cf61d17` | RASTER.RCP24 — full 16,777,215-input domain against the frozen hash |
| `615ed5f` | RASTER.PERSPUV — the perspective divide, shift derived then attacked |
| `8418364` | RADIX becomes a parameter: 36 clocks -> 20 |
| `6abc31e` | TEXJOIN — internal sequence identity, AUX concurrent at zero cost |
| `e7d8818` | THE SEAM — five blocks together, 1,644 pixels, exact |
| `270f436` | GEOM.WCACHE shell (the owner ruling's outstanding half) |
| `43fe72e`, `b3bfdca` | reports/PER_PIXEL_BUDGET.md |
| `db02ada` | reports/OPEN-SPEC-DEPTH-QUANTISATION.md |
| `8a353a6` | tools/render/count_fragment_load.cpp — overdraw measured |
| `54168d3`, `e093171` | reports/CONSOLE_REMAINING.md, and its correction |
| `a0bc76d` | CORRECTION: edgewalk setup is 5 clocks, not 21 |

## Evidence

* 125/125 on the raster, geometry, render, ledger, lint and formal gates.
* `ledger:check` green: 92 blocks, V1-V17 + V19-V23 + staleness.
* Every block's own directed test green; each new check mutation-tested where a
  mutation was meaningful.

## Measurements that changed a decision

* the divide: 36 clocks -> 20 at radix 4; the UNITS x RADIX grid reaches
  658,978 divides a frame;
* `rcp_u24` matches `RCP24_FULL_HASH` over its entire input domain;
* AUX runs concurrently with the primary TMU at **zero** added clocks
  (1,446 either way);
* overdraw measured exactly: 1.00x for a full-screen pass, 2.20x for an army;
* edgewalk setup is 5 clocks, which reprices ruling 4 from 32% of a frame to
  7.67%.

## Three things I got wrong, and how each was caught

1. **Edgewalk setup measured over the wrong interval.** Accept-to-first-beat is
   21 clocks and contains the whole 16-row walk. My self-check -- that the figure
   was identical across three coverages -- could not have failed, because the
   walk is always 16 rows. Caught by re-reading the state machine. Corrected in
   `a0bc76d`, with the wrong reasoning kept in the test header.
2. **Overwrote an existing 268-line test.** `geom_wcache_directed.cpp` already
   existed; the Write tool said "updated" and I did not read it. Caught by CMake
   refusing a duplicate target, not by me. Restored from git.
3. **Claimed seven blocks were buildable.** They are not: four contracts are
   stubs with 15 TODO sections, one is a documented refusal, and two need data
   formats that do not exist. Caught by opening the contracts, which the first
   pass never did. Corrected in `e093171`.

4. **A regression I introduced and did not see for a day.** `da6ca7a` landed
   ruling 2 (one lifecycle per tile) and updated two render tests but not
   `terrain_project_chain`, which composes the same `bin_pipe`. It went red and
   stayed red because every gate I ran was scoped to `raster_|render_|geom_` and
   that file is `terrain_` -- and the three full-label runs I started were each
   killed before finishing. Diagnosed by reverting the three files of `da6ca7a`
   until it passed, which cleared both my GEOM.CLIP widening and the creature
   lane's merge. The RTL was right; the test encoded the old law. Fixed in
   `4c76318`.

   **The lesson is about the scope, not the bug.** A scoped gate drawn around
   the blocks being EDITED misses the blocks that CONSUME them. 177/177 now,
   over a sweep wide enough to have caught it.

## Open, and all of them decisions rather than work

1. `wmin`, `wmax`, `scale` — blocks GEOM.PROJECT's attribute carry.
2. The binner arena capacity.
3. What `276,480` counts — decides whether two per-pixel blocks need replicating.
4. Seven contracts to write, or a ruling on who may write them.
