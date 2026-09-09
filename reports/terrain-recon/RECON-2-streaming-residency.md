# Terrain recon 2: the streaming / residency path

2026-09-09, rescue phase. Read-only. Every figure below is either a ledger row
read through the census's own selector or an explicit "no figure exists".

## The blocks

| block | ALM | reg | M10K | mem bits | DSP | timing | composed? |
|---|---:|---:|---:|---:|---:|---|---|
| `zhao_terrain_residency_v2` | 2,234 | 1,226 | 16 | 150,528 | 0 | **slack -6.597 ns, FAILS** | prod top |
| `zhao_terrain_pagestream` | 1,649 | 2,043 | 0 | 0 | 0 | 97.11 MHz (short of 100) | **nowhere** |
| `zhao_terrain_cmd` | 1,069 | 869 | 0 | 0 | 0 | 90.87 MHz (short) | **nowhere** |
| `zhao_terrain_patch` | -- | -- | -- | -- | -- | -- | prod top |
| `zhao_terrain_velocity` | -- | -- | -- | -- | -- | -- | prod top |
| `zhao_pair_pagestream_patch` | rules only | | | | | | bench wrapper |

**Two production blocks have NO measured row anywhere in the repository.**
`design/fit_targets.yml:1432-1434` lists `zhao_terrain_patch` as a fit target
with no `rules:` block, and no `reports/synthesis/blockpaths/zhao_terrain_patch.*`
exists. The 1,584 ALM figure I put in the recon brief for it **is not corroborated
by anything on disk** -- I supplied it and the recon could not find its source.
Same for `zhao_terrain_velocity` (`fit_targets.yml:1436-1438`). Any plan that
prices those two blocks is pricing a guess.

## Memory picture

| array | where | shape | bits | class |
|---|---|---|---:|---|
| `keyram[4][256]` | residency_v2:277 | 107x256x4 | 109,568 | already RAM (altsyncram confirmed) |
| `statram[4][256]` | residency_v2:277 | 57x256x4 | 58,368 declared | **RAM but SHORT 17 bits/entry** |
| `buf_q[3]` | pagestream:306 | 512x3 | 1,536 | **impossible** -- read combinationally through a byte-lane mux the same cycle as emit; `max_m10k:0` is a deliberate gate |
| `fp_x0/z0/x1/z1[16]` | patch:229 | 32x16 x4 | 2,048 | candidate by ACCESS SHAPE (1 wr addr, 1 rd addr) but too narrow to be worth an M10K |
| `lat_mem[64]` | pair wrapper:106 | 64x64 | 4,096 | bench-only, already RAM |

**The `statram` shortfall is an open defect, not a rounding difference.** Quartus
infers `WIDTH_A=40` where the declaration is 57, so only 150,528 of 167,936 bits
land in RAM and the remaining 17 bits/entry are **unaccounted -- not flops, not
MLAB**. Written up in `reports/RESIDENCY-V2-MISSING-BITS-20260907.md`. 164 bits
declared per entry, 147 actually resident.

## Data flow

Page pool -> `pagestream` streams a 33x33 lattice, layers A/B/C, one vertex per
beat through three 64-byte staging buffers -> `patch` composes top/bottom/scar per
vertex -> `velocity` accumulates. `residency_v2` is queried independently: a
256-set x 4-way directory keyed on `{resource_epoch, island_id, patch_ix,
patch_iz}`, 1,024 slots.

## The two uncomposed blocks

`pagestream` and `cmd` are instantiated **nowhere in production** -- only the
bench wrapper puts pagestream and patch together. `cmd` is blocked on a shell
record-framer byte width (16 where 32 is needed), per `prod_manifest.yml:491`.
So the streaming path's cost is partly a cost the console does not yet carry, and
partly two blocks whose cost nobody has measured. Both facts cut against treating
this domain's ALM total as settled.

## Contracts

`TERRAIN.RESIDENCY.md`, `.PAGESTREAM.md`, `.PATCH.md`, `.VELOCITY.md`, `.CMD.md`,
`.ISLAND.md` all exist. No contract for the pair wrapper (correctly -- it is a
characterization harness and says so, documenting three unmodeled glue gaps).
Reference models: `reference/include/zref/zref_terrain{,_patch,_velocity}.hpp`.
