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
## 2026-09-19 evening -- A PIXEL TRAVERSES THE RENDER PATH

**The milestone.** `raster pixels=1536 bursts=96 issued=1536 retired=1536 fatal=0`
from 16 triangles in 1 admitted frame, every issued word retired by the arbiter.
This morning it was zero and the smoke bench blamed VIDEO.SLOTMGR.

**And the bench's own sentence was wrong three times over.** All six lease blocks
were ALREADY composed in the shell. The real blockers were two wires: GEOM.SETUP's
`out_area2_o` never reached `tri_area2_i` (header entry 7 claimed "port for port";
it was 20 of 21), so the tile pipe read area2==0 as PROFILE AREA BAD and sank 72 of
72 jobs; and `u_guard_render` was handed the BLITTER's window, so MEM.GUARD denied
everything and FBWRITE went fatal after 1,536 fragments. Both fired as positive
controls when reverted.

**I23 is DELETED, not closed.** The geometry asset path composes: MESHFETCH +
ASSETFETCH through one MEM_ADAPTER on the shell's single guard socket, into
VDECODE and ASSEMBLE, with CULL on the projector's EXISTING matrix bank. The smoke
bench now writes the same `vdec_record()` bytes into SDRAM instead of playing them,
so the only difference between the old reading and the new one is the memory path.
`zhao_geom_project` deliberately NOT composed -- a second projector is ~6,199 ALM
and 33 DSP, and the one in `u_proj_subsystem` is already there.

**I23's stated reason was false and had been for a long time.** "No behavioural
SDRAM model in this tree" -- `sim/models/zhao_sdram_model.sv` is 219 lines and
cycle-true. The thing that actually blocked geometry was four files lower:
`ZHAO_RENDER_ASSET_BASE`'s `default: pass_ok = 1'b0` in `zhao_pkg`, whose own
comment says so. THE REFUSAL SURVIVED ITS OWN CAUSE BY FOUR FILES.

**GEOM.LOOM built** as a streaming matrix composer over the EXISTING
`zhao_geom_mat3x4_mul` -- I nearly briefed a duplicate before finding it.

**MY OWN ERROR, recorded because I got the correction wrong too.** Commit
b788812f reverted the video packet's 064acc93 through a stale shared index. I
checked, diffed my commit against 759b449a -- which was NOT its parent -- and
reported "false alarm, nothing reverted". Diffing ACROSS the agent's commit
cancelled the revert out. The packet re-landed it as 9887da8d. The CLAUDE.md
section warning about exactly this was written by me one minute before I did it.

**Where it stands.** 70 mandatory gaps (30 tie-offs + 37 disconnected + 3 unbuilt),
58 capabilities connected, closure 143 modules. Unwaived lint 120 with attribution
(6 inherited from the newly-composed asset path); waived still SILENT.

**NEXT:** the tie-off count is rising as composition makes real seams visible (26
-> 30). That is honest, not regression -- but it means the remaining work is now
mostly BOUNDARY entries with named absent owners, and the two biggest named owners
are CMD.SCHEDULER (I14/I30/I33/I36) and the Packet-D attribute carriage (I20).
## 2026-09-19 night -- CMD.SCHEDULER was never absent, and the recipe ate a packet

**THE TENTH FALSE ABSENCE, and the biggest.** `zhao_cmd_scheduler.sv` -- 25 KB,
a contract, a directed suite and a formal proof -- has been instantiated as
`u_sched` at `zhao_shell_top_v2.sv:725` since 2026-08-16, INSIDE the shell
`zhao_console_core` already instantiates. FOUR header entries (I14, I30, I33,
I36) rested on it being missing. It is also the WRONG owner: its own first line
calls it "the 3-slot frame ownership FSM" and its dispatch sinks are
DEBUG.FRAMEBLIT, INPUT.RUMBLE and VIDEO.MODE. The draws were arriving and dying
at its line 384, "all other opcodes: counted, no dispatch".

**The draw job is two halves with different answers.** The DISPATCH half was a
missing arm in a live block -- `DrawForm 0x0300` now reaches the console through
a third CMD.EXEC arm sharing the existing framer, verdict gate and commit FSM.
The JOB half is a missing RULING: spec/memory_rules.md 5f ratifies the asset
pool's region and then says "Not decided: the pool's internal layout ... still
open", so nothing turns a 24-bit handle into a descriptor address. Three of six
job fields have no ratified producer, and a job is atomic, so it was NOT
half-driven.

**GEOM.ATTRPACK landed** -- "the front end that asks three times and packs the
planes", which is the I20 blocker that makes the 1,536 pixels flat.

**THE PRIVATE-INDEX RECIPE REVERTED A PACKET IN FULL**, 529 deletions, after
being written into CLAUDE.md. `git read-tree HEAD` snapshots HEAD at that
moment; run it in one tool call and `git commit` in another, and anything that
commits in between makes your index describe the past. Every local signal said
fine -- apply returned 0, the staged diff was correct -- and the ONLY tell was
the commit summary reporting four files when one was staged. CLAUDE.md now
requires read-tree and commit to be atomic, or a `rev-parse HEAD` re-check
immediately before committing. A private index protects another agent's INDEX;
it does nothing for their COMMITS.

**Where it stands.** 69 mandatory gaps. Three packets live: terrain, texture +
attributes, FIELD.

**NEXT:** when a slot frees, the board<->core join -- `zhao_console_board` is one
of the two required tops and deliberately does not instantiate the core. It has
waited all session because the core's port list has not been still long enough.
## 2026-09-19 late -- the board is soldered to the core, and FIELD v3 ships

**THE SECOND REQUIRED TOP EXISTS.** `fd665eca`: `zhao_console_board` instantiates
`zhao_console_core`. The board's own header had a section titled "WHAT THIS FILE
DELIBERATELY DOES NOT DO: INSTANTIATE zhao_console_core", honest at the time --
the core declared 687 ports and four packets were editing it. That reason expired
and nothing had re-read it. The inventory gate now walks BOTH tops: 167 modules
elaborated by the core, 5 more added by the board.

**FIELD v3 IS THE ENGINE AND v1 IS OUT** (`da57defe`). And the finding behind it is
the one to remember: `zhao_probe_v3_full.sv` in `fpga/rtl/synth/` WAS the composed
v3 engine -- executor, service path, both mul banks, five differential gates on it.
Nothing was missing from the datapath. What was missing was a production NAME, a
HOME and a FIT TARGET. Four files moved out of synth/, no line of circuit changed.
"`probe` in a filename kept a finished engine out of the machine for three weeks."

**The cited deadlock was fixed three weeks before the sentence citing it.** The
manifest and the core both quoted a stale probe header about a SPLINE/RING
park-forever; `zhao_field_ops_pkg` made it structurally impossible on 2026-08-29.
Three claims in the FIELD manifest block were false and ALL THREE READ LOW.

**THE SAME LAW, SIX AND SEVEN TIMES.** "A file is not a module" bit twice more:
  * my own `check_console_inventory` took a module's name to be its filename, so
    it called `zhao_raster_quant.sv` dead (its second module is
    `zhao_raster_quant_fin`), I removed it on the gate's say-so, and the console
    lint broke;
  * `module_graph.build()` -- shared by gen_prod_top, check_prod_manifest and
    check_ownership_roles -- had `if decl[mod] == p: continue`, which suppressed
    EVERY EDGE INSIDE a multi-module file. Eleven real edges invisible, and in the
    flattering direction: a missed edge makes a module look uninstantiated, so it
    reads as dead weight.

**Where it stands.** 64 mandatory gaps (32 tie-offs + 29 disconnected + 3 unbuilt).
Console inventory gate OK on all four gates across both tops.

**THE ONE THING THAT MUST NOT BE FORGOTTEN BEFORE THE FIT:** the console was
composing FIELD at the SCALAR BENCH POINT -- `PROGS(8)`, `REGS(32)` -- while the
shipped machine is `CTX=32 LANES=4 REGS=64 OUTSTANDING=16 LONGQ=16 DIST_BANKS=8
RING_UNITS=8`, and `zhao_field_host` did not expose most of those knobs at all. A
fit taken before `703c1174`/`d9854632` would have measured a one-lane,
eight-context machine and called it the console. Check the selected
parameterisation before believing any FIELD area number.

**NEXT:** geometry/forge remainder in flight; then terrain's remaining 10.
## 2026-09-19 night -- 62 gaps, and eleven of them are decisions

**Where it stands.** 62 mandatory gaps (34 tie-offs + 25 disconnected + 3 unbuilt),
70 capabilities connected, from 78 gaps this morning. Both required tops exist and
the board is soldered to the core. A pixel traverses the render path. The console
executes ratified commands. FIELD v3 is the composed engine.

**ELEVEN OF THE 34 TIE-OFFS CANNOT BE CLOSED BY BUILDING ANYTHING.** They want a
ruling. Written up with a recommendation and evidence each in
`reports/OWNER-DOCKET-20260919.md`. The two I marked SAFE are in flight; the rest
are the owner's, and the largest by area is the three "unbuilt" optional alternates.

**The census was counting twelve modules TWICE** -- the v3 service modules were
listed as tops while living inside `zhao_field_host`. 70 -> 58 instances. Note the
direction: this one INFLATES, and a big number looks like honest bad news, so
nobody audits it either.

**Instrument defects found today, all one law -- READ THE STRUCTURE, NOT THE
CONVENTION:** closure membership counted as connection; an unbounded scan; a
substring anchor matching a comment; an alias outliving its search; a settling
marker matching prose; a filename standing in for a module (TWICE -- in my own new
gate, and in `module_graph` where it hid 11 real edges); and duplicate entry ids
that the register counted twice and never mentioned. Every one now has a guard that
was FIRED before it was trusted.

**Fourteen "X does not exist" claims have been false today.** The two that should
change how anyone reads a refusal: CMD.SCHEDULER, called absent by four entries
while instantiated in the shell since 2026-08-16; and a finished FIELD v3 engine
that sat in `fpga/rtl/synth/` for three weeks because `probe` was in its filename.
The terrain lane then found `zhao_probe_walk_earth.sv` the same way.

**THE PHASE 2 FACT THAT MATTERS MORE THAN THE GAP COUNT:** FIELD is not in the
47,582 ALM figure -- that fit's own sources name one field file, and
THE-NUMBER-20260919.md says so. FIELD is composed now, so its cost is ADDITIVE to a
budget already 5,672 ALM and 39 DSP over, and FIELD at the SCALAR configuration is
charged ~13,700 ALM against a 4,500 envelope. No ruling fixes that.

**NEXT:** three packets in flight -- the material directory key (makes the texture
island sample), terrain's absent owners, geometry/forge owners. Phase 1 is not done
and the fit stays closed until it is.