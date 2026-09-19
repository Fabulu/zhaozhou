# Task Log: RUN-20260919-0432 - implement ruling I4 (collision spawn)

**Created:** 2026-09-19 04:32 UTC+02:00
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260919-0432-ruling-i4-collision-spawn/

---

## Objective

Implement `reports/RULING-I4-COLLISION-SPAWN-20260919.md`, already authorised by
the owner. Retire PART.UPDATE's `col_*_i`, give the collision event to
PART.COLLIDE, place a collision-spawned child POST-CONTACT behind a named
constant, and make `part_spawn_by_event2_o` SEEN TO MOVE. Implement, do not
decide.

---

## Progress Timeline

### 04:32 - started; baselines taken BEFORE touching anything

- completion register: 85 mandatory gaps (20 tie-offs).
- lint, console core closure from `fit_targets.yml`, no waiver file:
  113 `%Warning` lines + `Exiting due to 114 warning(s)`. The brief's "114
  baseline" is the DIAGNOSTIC total. With `tests/shell/v3_closure_inherited.vlt`
  the same lint is RC=0 and silent.
- POSITIVE CONTROL for the lint gate, on a scratchpad copy of the core with one
  instance pin connection deleted: PINMISSING appeared, total rose to 116. The
  gate fires.

### 05:0x - RTL, contracts, block benches; committed

`f4dbd5ed`. Four ports and a counter out of PART.UPDATE; event vector,
spawn-record port, counter and the `CHILD_AT_POST_CONTACT` knob into
PART.COLLIDE; the one-to-two fork moved from PART.UPDATE's output to
PART.COLLIDE's in the composer; header entry I4 deleted; three contracts
amended with correction banners.

Lint back to exactly 114 diagnostics. `part_update_directed` 264 -> 263,
`part_collide_directed` 180 -> 198, both green, both deltas accounted for
line by line.

### 05:3x - the acceptance, in the connected core

`tests/prod/run_console_core_smoke.ps1`, with six particles driven into a
heightfield at +100 under a STICK response:

    spawn_by_event=[0 0 6 0]   collisions_applied=6   contacts_stick=6
    children written=5, all 5 at the POST-CONTACT position y=102

`part_spawn_by_event2_o` moved 0 -> 6. It could not move at all before.

### 05:4x - a defect found on the way, measured and NOT repaired

Six children emitted, five written, every explaining counter zero. Written up
in `reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md`, bannered on
`PART.STATE.md`, printed by the smoke bench on every run. Pre-existing and
previously unreachable; repairing it is a PART.STATE tick-boundary decision,
and a partial repair would make the loss rarer rather than impossible.

---

## Decisions Made

- **The fork moved rather than an adapter being added.** Ruling S3 places the
  child at `pout_c`, which only exists downstream of PART.COLLIDE, so PART.SPAWN
  had to move behind it. That is composition, not arithmetic in the composer.
- **Bit 2 is OVERWRITTEN, not OR-ed.** The OR spelling would have linted
  silently and given the collision bit two authors. The waiver plus a
  simulation assertion is the honest version of the same line.
- **`collision_events_o` is a new counter on PART.COLLIDE** rather than a sum of
  the five `contacts_*`: it is wired to the event gate, so it can disagree with
  them, and disagreement is the bug. It also lets the console keep its
  `part_collisions_applied_o` port, which may not change (prod top is off limits).
- **Two dated reports were CORRECTED, not rewritten** -- their numbers belong to
  the tree they were measured from; the stale "step 6 is folded away, so this
  under-counts" claim now carries a banner saying the opposite.

---

## Next Steps

- A fit is owed: PART.UPDATE should measure SMALLER and PART.COLLIDE slightly
  larger. Nothing here has been through `quartus_map`.
- `CHILD_AT_POST_CONTACT = 0` elaborates an untested generate branch.
- Ruling S3's real acceptance is sparks in motion. Nothing was rendered.
- `reports/DEFECT-PART-STATE-LAST-CHILD-20260919.md` is open.
---

## 2026-09-19 -- Phase 1 gap campaign: root causes, not modules

**Where I am.** Mandatory gaps 78 -> 72. Three agents ran concurrently; TWOD.SAMPLER
is back, PART.PROJECT and CMD.EXEC still out.

**The leverage find.** Five register entries (I7, I14, I30, I33, FORGE.PRIM jobs)
all blamed "the absent CMD path". It was never absent -- CMD.DMA's packet stream
was ENCLOSED as body wires in `zhao_shell_top_v2`. The shell re-exports it now and
CMD.DECODER is composed. `cmd_pkt_ready_i` is a real INPUT, not an assumption that
the second consumer never backpressures: that assumption fails silently by dropping
command bytes. This does NOT close those five -- the decoder emits record headers
and a verdict, and SetView's mat4fx is payload -- the EXECUTOR does, and that is the
agent still out.

**Built TERRAIN.PLACE** (`fpga/rtl/terrain/zhao_terrain_place.sv`), the placement
owner I27 said did not exist. The arithmetic is ratified, not invented:
spec/terrain_rules.md 1.3 freezes pitch to powers of two so placement is exact
shifts. 68 checks, 0 failed, all four censuses FIRED. Gates the 15-capability
terrain cluster, the largest remaining. NOT yet composed into the core.

**Three instruments were lying, all fixed and all found by accident:**
1. the lint waiver matched `*fpga/rtl/texture*` with forward slashes while the
   register hands back backslash paths -- 112 phantom warnings that read as a
   regression in a subtree nobody touched;
2. both register closure walks anchored on a bare substring search for
   "zhao_console_core", so an agent's COMMENT mentioning it hijacked them and the
   headline went 77 -> 124 with nothing in the design changed. Both now match the
   target header, and audit() hard-fails if the two walks disagree;
3. `module_graph.strip_comments` stripped `/* */` before `//`, so a glob in a
   header comment swallowed a `module` keyword and dropped a block from
   `zhao_prod_top.sv` -- area the fit would never have seen.

**NEXT STEP, written down before I read any agent result:** compose TERRAIN.PLACE +
TERRAIN.PATCH + TERRAIN.COMPCACHE (+ TESS, entry I22) into `zhao_console_core`.
That is the whole of I27 and I22 and it is the biggest single cluster left. The
core is contended by two live agents, so hunk-level staging and `git commit --only`.

**Open for the owner:** all six UNBUILT blocks must be built (the ledger records the
2026-08-31 cut as withdrawn, quoting "I don't want to defer any unfinished blocks
now"), but INPUT.SNAC, GEOM.WARP and POST.ECHO have DELIBERATELY BLANK contracts --
every section reads "Deliberately unwritten" -- so each needs a contract authored
before any RTL exists to write.
## 2026-09-19 later -- 70 gaps, and the console executes a command and carries traffic

**Where I am.** 78 -> 70 mandatory gaps (27 tie-offs + 38 disconnected + 5 unbuilt).
All four agent packets landed: TWOD sampler, PART.PROJECT, CMD.EXEC, TERRAIN compose.

**Two things the console can do now that it could not this morning:** execute a
ratified command (SetView reaches the projector's matrix bank, SurfaceStamp reaches
the sheet), and pass its integration smoke bench -- "the connected core carries
traffic on every wire this bench can reach", 64,128 terrain bytes retired over 1,007
played bursts.

**Built by me:** TERRAIN.PLACE (the placement owner I27 said did not exist) and
MEM.UPLOAD (the HPS->VRAM path the contract opens by saying does not exist).

**FIVE register defects found and fixed, all in one family.** Closure membership
counted as connection; the unbounded scan that absorbed the next fit target; the
substring anchor that matched a COMMENT; a hand-resolved alias that outlived its
search; and a settling marker that matched PROSE, so an entry closed itself when
someone wrote a sentence about it -- and closed again when the sentence explaining
that was quoted. Every one read a CONVENTION where it should have read a STRUCTURE.
Each now has a guard I FIRED before trusting.

**The biggest remaining functional gap is that NO PIXEL TRAVERSES THE RENDER PATH.**
The smoke bench says it in its own voice: `raster pixels=0 because
frames_admitted=0 -- the V2 renderer lease is not driven by this bench;
VIDEO.SLOTMGR is still not connected`. That is the next wave's first packet.

**NEXT STEP, written down before reading any agent result:** wave of three --
VIDEO.SLOTMGR + the renderer lease (pixels), the geometry cluster (8 disconnected),
the texture cluster (6). Mine: compose MEM.UPLOAD into the core, and author the
three missing contracts.

**Still open for the owner:** INPUT.SNAC, GEOM.WARP and POST.ECHO have DELIBERATELY
BLANK contracts -- every section reads "Deliberately unwritten" -- because they were
cut on 2026-08-31 before their specs were written. The ledger records the revocation
verbatim ("I don't want to defer any unfinished blocks now ... the 2026-08-31 SS6.3
cut is withdrawn"), so I am treating them as mandatory and authoring the contracts.
Flagged rather than assumed.