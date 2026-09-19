# R3: client A's schedule, with geometry, particles and FORGE.SHADOW sharing it

Owed by owner ruling R3 (`reports/OWNER-RULINGS-20260919-EVENING.md`): *"Keep
the time-multiplex. No third port in v1. Owed: a written schedule proof that
geometry, particles and FORGE.SHADOW's instance-centre 1/w share client A's
bandwidth within the frame at the guaranteed content tier."*

Written 2026-09-19 by the geometry packet (RUN-20260919-1656-gaps-to-zero).
Every rate below cites the file that states it, and each number that is
MEASURED says so.

## 1. The server

* **One `zhao_project_core`**, inside `zhao_project_service`, inside
  `zhao_proj_subsystem` (`u_proj_subsystem` in `zhao_console_core`). The
  console does not override `ROWS_PER_PASS`, so it is 3: **one vertex per
  clock** (`zhao_project_service.sv`, parameter comment).
* **Two ports, round-robin with a toggling priority**, work-conserving: "the
  client that was granted becomes the low-priority one next time ... With both
  saturated each gets every other clock" (same file, ARBITRATION). Client A
  can therefore always get at least half the core while B is saturated, and
  all of it otherwise.
* **Client A is itself multiplexed by PART.PROJECT** (`zhao_part_project.sv`):
  geometry passes straight through, and particles take the cycles geometry
  does not want. Results are demultiplexed by the rider's top bit.
* **The frame** is 1,666,666 gpu clocks (`design/budgets/workloads.yml`,
  `computeClocksPerFrame`, the conservative floor at 100 MHz / 60 Hz).

## 2. The demand at the guaranteed tier (grants per frame)

| client | who | per frame | source |
|---|---|---|---|
| A | geometry, **both views** | 240,000 | 120,000 vertices ruled (`workloads.yml` `zhao_geom_skin`); GEOM.GROUP_SEQ projects each vertex once per visible view, so Duo with every vertex in both views is 2x |
| A | particles | 65,536 | the particle ring's tier (`zhao_part_project.sv` header, "65,536 costs about 295,000 clocks") |
| A | FORGE.SHADOW instance centres | <= 256 | the guaranteed creature tier is 32 kMesh creatures machine-wide plus one giant (`design/contracts/GEOM.PARAMBUF.md`); one centre per instance per view; 256 is a 4x margin over 2 x 33 |
| B | terrain, with the arena | 278,784 | `zhao_project_service.sv`, check 1 ("with the arena 278,784 + 120,000 = 398,784") |

**Total core grants: 584,576 per frame = 35.1% of 1,666,666.**
Client A's share is 305,792 (18.3%) and client B's 278,784 (16.7%).

## 3. Why it fits, and why no client starves

1. **Capacity.** The core serves one grant per clock and the arbiter is
   work-conserving, so any mix of demand totalling D grants completes within D
   clocks of backlogged service. D = 584,576 <= 1,666,666, a 64.9% margin.
2. **A against B.** With both saturated, A receives >= 1/2 of the core: its
   305,792 grants need at most 611,584 clocks even in the worst interleave --
   36.7% of the frame.
3. **Particles against geometry, inside A.** Geometry has priority on A, so
   the question is whether geometry can deny particles. It cannot, for a
   PRODUCER reason: GEOM.GROUP_SEQ offers at most two grants (one per view)
   per skinned vertex, and GEOM.SKIN retires at most one vertex per twelve
   clocks (`zhao_geom_skin.sv`; MEASURED slower in composition, see 5). So
   geometry wants at most 2/12 of A's cycles and particles see at least 10/12
   of them; halved again by B contention, still >= 5/12 per clock. PART.PROJECT's
   own ceiling is lower -- `SLOTS = 8` in flight over the core's 36-clock
   latency, 8/36 = 0.22 per clock (`PART_PROJ_SLOTS_C`) -- so particles are
   bounded by their SLOTS, not by geometry: 65,536 in about 295,000 clocks,
   17.7% of the frame of WALL time, overlapping everything else.
4. **FORGE.SHADOW's centres** are <= 256 grants: 0.015% of the frame. They fit
   in any gap, and ride PART.PROJECT's multiplex exactly as particles do.

**Conclusion: at `ROWS_PER_PASS = 3`, geometry, particles and FORGE.SHADOW
share client A within the frame with a 64.9% margin on the core, and no
client can starve another. No third port is needed. R3 holds.**

## 4. What this proof EXCLUDES, so nobody spends the margin twice

* **The DSP lever `ROWS_PER_PASS = 1` (15 DSP instead of 33) is OFF the table
  while these clients share the core.** At one vertex per three clocks the same
  demand is 3 x 584,576 = 1,753,728 clocks -- 105% of the frame. The service
  header's own figure (71.8%) predates particles and a second geometry view.
* **The margin is on the PROJECTOR, not on the geometry pipeline.** See 5.

## 5. The rate that does NOT fit, found while writing this (not a client-A issue)

* **GEOM.SKIN.NORM gates GEOM.SKIN.** The two sit on an AND-fork
  (`zhao_console_core.sv`, entry I43), and SKIN.NORM is one-at-a-time with a
  32-iteration serial root. The smoke bench MEASURES `fork_stall_cycles=63`
  over 4 vertices, so a vertex costs about 12 + 16 = 28 clocks. At 120,000
  vertices that is ~3.4M clocks, **about 2x the frame**. The projector would
  be idle waiting for vertices long before its bandwidth mattered.
* **GEOM.REPLAY** (built this session) MEASURES 56 clocks per view-triangle,
  dominated by the three DEPTHQUANT lanes' reciprocal round trip. At ~40,000
  triangles in two views that is ~4.5M clocks, **about 2.7x the frame**. Its
  contract names the lever (a streaming DEPTHQUANT through the multi-context
  rcp24_v4, overlapped with the next triangle's lookups).

Both are Phase-2 rate work on blocks, not on client A, and neither is hidden by
this proof.

## 6. The second half of R3: GEOM.LOD's `proj_radius_q8`, and why it is NOT built yet

R3 says: "Then use the multiplex to feed GEOM.LOD's `proj_radius_q8` so
FORGE.SHADOW's rung has a producer." The multiplex CAN carry it (section 3.4):
a centre projection is one more guest stream through PART.PROJECT, and the
arithmetic is already ratified and already called
(`zref::render::draw_form_marker`'s world branch, which PART.PROJECT
transcribes). It was not built this session, because GEOM.LOD takes FIVE inputs
and `proj_radius_q8` is only one of them (handover section 12):

* `thresh_q8` -- the governor's per-camera pixel-error threshold.
  `zhao_measure_governor` emits per-camera SCALES and no threshold; the ledger
  edge is an intention, not a port.
* `bound_radius`, `micro_error`, `splat_error`, `glint_error` -- per-creature-
  TYPE constants in the kind-8 creature form page, whose byte layout
  `spec/cartridge.md` 202-208 FREEZES until Phase-12 entry ("the packer refuses
  to emit them ... never a guessed layout").

A projected radius delivered to a block that cannot be composed would connect
nothing -- the same test GEOM.LOOM and TERRAIN.VISIBLE fail in the core's
refused-blocks list. **Recommendation:** build the centre stream in the same
packet that gives GEOM.LOD its threshold, and ask the owner whether the kind-8
freeze may be lifted for the four LOD constants alone (they are four numbers
per creature type, not a layout).
