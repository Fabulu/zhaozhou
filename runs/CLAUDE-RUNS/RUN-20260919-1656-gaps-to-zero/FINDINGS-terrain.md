# FINDINGS — terrain packet (1st pass)

Transcribed by the coordinator from the packet's final report. The harness blocked
the subagent from writing report files, so this was not written by the packet itself.

Register at 7353eb7b: **57** (packet entered at 61; GEOM.PROJECT and I37 landed in parallel).

## Closed
* `b91f26b4` **I6** — PART.COLLIDE reads live terrain with the R1 normal, bit-exact against a new
  reference function. A cache stage in front of PART.COLLIDE keeps the particle path from ever
  waiting on a terrain read, at a fixed 6 clocks per particle instead of the ledger's 1 (DSP is
  over budget). The particle population's world origin has no producer in the core, so it is
  added to I7 rather than opened as a new entry. terrain_rules §4.4 is amended for collision.
  Tests: heighttap 2,272 checks, chain 1,297.
* `7353eb7b` TERRAIN.PROJECT → shared projector client B, the same bookkeeping correction as
  client A. Smoke shows client B carrying terrain: 81 grants, 128 triangles. **No ruling names
  client B explicitly; reverting is one alias line.**
* `2d68ce70` A real PART.STATE bug: a tick could close before its last judged record and its
  children came back, so they slipped into the next tick with every counter still balanced. The
  old timing just happened to win the race. Smoke went from 4 of 6 children written back to 6 of 6.
  A regression test fails if the fix is removed.
* `f8e0f906` I21 analysis (comment only).

## Refused, with blockers
* I26 (R4): the arbiter exists, but the shell has no guard, no read-beat routing and no write path
  for TERRAIN.BUILD's reserved memory slot 6. It should follow MEM.UPLOAD's socket.
* I28 / WRITEBACK: the journal address and ticket have no owner, and dirty eviction is
  unreachable without BAKE.
* I32 / BAKE: the strength→depth and 64×64→33×33 laws do not exist, and no command carries dig
  depths (searched `commands.zidl`).
* R5: SHADE needs a sun direction, which has no producer in the core, and its per-triangle output
  has no consumer until I13.
* R6: the change touches three reference headers, four tests and the compiler's C++ emitter, and
  closes nothing until R8 gives ISLAND/VISIBLE a consumer.
* R8 / I44: not started.

## Owner decisions raised → see OWNER-RULINGS R13–R16
1. I21: the material comes from layer E per cell, but the job port carries one value per subpatch.
   Where do the two join?
2. The writeback journal slot and ticket need an SW.STREAM→FPGA doorbell contract.
3. BAKE's two laws, or a dig command that carries depths.
4. Whether hardware TERRAIN.VISIBLE is still v1, given that ruling T5 moved the visible set into
   software.

## False claim
* The heighttap comment "the SAME net that drives PLACE's pitch" points at a header wire that reads
  127 (refuse) between headers. Wired literally, it lints clean and faults almost every tap. The
  core now holds the pitch instead, and the comment is corrected.

## Instrument defects
* The packet's own chain test missed a ±1 rounding fault until cases were pinned at the tie. Both
  directions now fail it.
* The stale-binary problem happened twice (`ninja: no work to do` after a restore). Both times the
  rebuild was forced.
