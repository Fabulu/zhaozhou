# FINDINGS — geom lane (branch `gz/geom`, final 5a1413d9)

Transcribed by the coordinator; the harness blocks subagent report files. Register **50** at 5a1413d9
(merged with shared 004aa435; the packet started at 61).

## Closed
| register | what | commit |
|---|---|---|
| 61→60 | GEOM.PROJECT → `zhao_proj_subsystem` client A (R3); `zhao_geom_project` superseded | c4449fb8 |
| 60→59 | I37: new `zhao_geom_desc_crc` (728 checks against `zhao_abi::zhao_crc32c`); the fixture writes a real CRC | 7228d596 |
| 59→57 | GEOM.REPLAY built and composed: I11 and I38 close; DEPTHQUANT composed as 3 lanes on one rcp24_v4; the bench's triangle door removed; opens I46 (R11) | 668ad7dc, 9bc3dd1e |
| 51→50 | R2: GEOM.LIGHT = `zhao_light_stream` via `zhao_light_skin_adapter`; I43 closes; opens I48; the smoke asserts every vertex is lit at zref's value | 5d0eefc3, 5a1413d9 |

Also landed: dd716da2 (fresh configure fixed) and d47f169e (R3 proof,
`reports/R3-CLIENT-A-SCHEDULE-PROOF-20260919.md`: 584,576 grants/frame = 35.1%, no starvation,
ROWS_PER_PASS=1 excluded at 105%).

Pixel gate: 2560, generated from zref by `smoke_geom_fixture_gen.cpp` and held fresh by ctest. The fixture is 8
triangles in both views, 2 clipped, 14 accepted, 10 tiles. frames_admitted=1 and issued==retired are hard assertions.

Narrowed, not closed: I12 (the lookup closed; the origin stays), I13 (the geometry side closed; terrain stays), I24 (the cull
mode stays), I39 (the raster word stays).

## Refused (blocker, and what was searched)
* I12: nothing reads the origin; the projector takes world positions (→ R27).
* I13: terrain needs a 2-producer merge plus its own attribute packet (DEPTHQUANT invw24, SHADE); R21 adds a
  terrain-normal client to the vertex store.
* I14: the viewport table is not in the ABI (→ R30); `proj_en_i`, pixel_error and tokens need GOVERNOR/TOKENS.
* I24: cull mode comes only from I39's raster word.
* I29: pages come via the VRAM arbiter and `zhao_sdram_ctrl`, and there is no behavioural SDRAM model (same as I23).
* I36: desc_addr, format and xform[12] are unratified (→ R29). I39: no raster_state layout (→ R28).
* I46: the writer joins a decode-time value to an arena index that exists only at GROUP_SEQ issue, so it needs its own packet.
* I48: see R25.
* GEOM.LOOM: no producer of its node stream; its consumer is GEOM.WARP (R7).
* GEOM.PARAMBUF: SDRAM-record layer; nothing writes or reads those records; blocked on the SDRAM model and a spill writer.
* GEOM.LOD `proj_radius_q8`: the multiplex can carry it, but `thresh_q8` has no producer and kind-8 is frozen (→ R26).

## Owner decisions → R25–R31 (OWNER-RULINGS-20260919-EVENING.md)
D1 SetEnvironment/bank law; D2 LOD constants plus thresh_q8; D3 arena origin; D4 raster_state; D5 draw-job layout;
D6 viewport table; D7 throughput: replay costs 56 clk per view-triangle ≈ 2.7× the frame, and the SKIN/SKIN.NORM fork
≈ 28 clk per vertex ≈ 2× (smoke fork_stall=63 over 4 vertices). The projector has 64.9% margin.

## False absences and presences
* The packet's own first I48 draft said "no ratified law"; §4a ratifies the direction and the expansion. Corrected.
* The old DEPTHQUANT refusal no longer held (per-lane latches handle the one-clock-late read).
* I39's "offset from PARAMBUF's arena allocator" describes the SDRAM arena; on-chip allocation is per meshlet, so the offset is 0.
* I13's "geometry triangles have no customer" became false once replay existed.
* The project_service header's 71.8% at ROWS_PER_PASS=1 predates particles and the second view; the true figure is 105%.

## Instrument defects
1. The fresh-configure break was hidden by cached build trees (fixed dd716da2); a fresh-configure CI step is recommended.
2. In a worktree, `git fetch` did not advance the tracking ref, so the merge said "up to date" (caught with `merge-base
   --is-ancestor`). Now in PACKET-PROTOCOL.
3. The coordinator's I46→I47 text renumber also rewrote a reference to geom's I46 in the smoke bench. Restored in 5a1413d9.
4. A lost newline pasted `//// I40.` into I39's body; the register's "NOT a tie-off" body check FIRED.
5. `ledger_check` cannot run in a worktree (tsc missing); its 10 schema errors predate this run.
6. New guards seen firing: att_skew (the `-BadAttribute` run and the directed test), the desc_crc counters, every replay counter (78 checks),
   `-BadDescriptor`, and the three light mutant controls.
7. **Existing DEFECT, not fixed:** a vertex that VDECODE refuses starves GROUP_SEQ and deadlocks the geometry path; no counter
   sees it (→ R31).
