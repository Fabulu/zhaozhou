<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
6072d3bf -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- FORGECOMP


## `6072d3bf` -- FINDINGS -- FORGECOMP: register 17 -> 14, and the blocker that dissolved was not on anybody's list

```
FINDINGS -- FORGECOMP: register 17 -> 14, and the blocker that dissolved was not on anybody's list

> THE HARNESS REFUSES A LANE-WRITTEN FINDINGS FILE, for the seventh lane in a
> row. This commit message IS the document. Transcribe it to
> runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-forgecomp.md.

REGISTER 17 -> 14. 8 tie-offs + 6 disconnected, from 8 + 9. Run BARE, RC 1
(normal while gaps remain). Branch gz/forgecomp, five commits.

NO TIE-OFF created, narrowed or relocated -- the core's INCOMPLETE block is
byte-identical, and packet_h_tieoff_audit reports it unchanged.

=========================================================================
1. WHAT MOVED
=========================================================================
  zhao_forge_prim        disconnected -> CONNECTED
  zhao_forge_prim_eval   disconnected -> CONNECTED
  zhao_geom_clipdoor     disconnected -> CONNECTED

Two NEW blocks land with them and take no design/blocks.yml row, because
neither is a capability: zhao_forge_pagebank (the FORGE_PROGRAM page reader AND
the 0x0302 dispatch) and zhao_forge_assemble (the join). A sixth,
zhao_forge_ring_eval, was built by FORGEPRIM and is composed here -- without it
four of the six families would have topology and no positions and the dispatch
would have to REFUSE them, which is a narrowing and not a composition.

STILL DISCONNECTED, AND MINE: zhao_forge_shadow, zhao_forge_cliff_ram. Both
named precisely in section 5.

=========================================================================
2. THE BLOCKER THAT DISSOLVED WAS NOT ON ANYBODY'S LIST
=========================================================================
Five FORGE passes moved this register by zero. Each was individually right to
refuse, and the list they shared named the page, the dispatch, GEOM.SETUP's
occupied arm, the seven-slot attribute law and the projector's two taken arms.

THE THING ACTUALLY MISSING WAS A VERTEX STORE. zhao_forge_prim emits INDEX
TRIPLES and no coordinate; the evaluators emit WORLD positions and no topology;
zref_forge_page.hpp names the ONLY thing relating them -- the ring-major
ORDERING CONVENTION -- and nothing in the tree held the vertices an index could
name. zhao_forge_assemble is that store. ONCE IT EXISTS THE REST IS WIRING.

That is R237's trap in a form worth naming: the list was HONEST, every entry
was TRUE, and it still sent five packets to the wrong question, because nobody
asked what a triple would be looked up IN.

Blocker by blocker, at the composed head:

1. CMD.EXEC's missing arm for 0x0302 -- CLOSED. Built to the TerrainField arm's
   shape. R241 D-TICK-A executed: frame_tick is two bytes of pad[11],
   little-endian, and the add to tick_phase_base WRAPS -- a phase is an angle16
   whose whole turn is the WIDTH of the field, so saturating it would stall the
   animation at the top of the turn. screen_error is DELIBERATELY NOT DECODED
   and the arm says why: the page authors segments and sides, zhao_forge_prim
   refuses out-of-range values on its own port, and no screen-error LOD clamp
   exists anywhere. A register nothing reads is the uncashed-cheque shape.
2. No bank stages a forge page -- CLOSED. Header read once at PUBLICATION, then
   a BOUNDED linear scan of record line 0 per draw, worst case count+2 line
   reads. ONE resident record, not the page: 192 bytes x 16 rows is ~15,000
   flops against an ALM budget already breached.
3. GEOM.SETUP's single arm -- SIDESTEPPED, exactly as R187 says to. Nothing
   enters at GEOM.SETUP.
4. The three-way ordered join -- UNTOUCHED, and that is the point.
5. "No free projector arm" -- TRUE OF THE SERVICE, FALSE OF THE PATH.
   zhao_part_project is a FRONT MUX on client A, not the service.
6. "THE BINDING ONE: GEOM_CLIP_ATTRS = 7" -- NOT BINDING, and it had already
   stopped being so before this packet started. R197 discharged u/w and v/w
   (attrpack branches on tri_untex_i and substitutes the zero operand); R234 D1
   settled the rest -- the Gouraud lanes are NOT branched on that bit,
   deliberately, because "an untextured primitive is still lit" and in the
   reference oracle it is the UNTEXTURED case that carries pre-lit colour there.
   Every slot has a ratified home. Only the colour's VALUE is authored.

I flag per R229 that section 2 is my flattering direction -- a discovery that
makes the work look tractable -- so it is the claim I checked hardest: the
composition is what proves it rather than the argument.

=========================================================================
3. THE THREE SEAMS, AND WHY R3 IS NOT TOUCHED
=========================================================================
  * zhao_geom_mem_adapter requester F, N 5 -> 6. memory_rules 5f's condition is
    met -- same pool, same client, same direction. One header per PUBLICATION
    plus a bounded scan per PROCEDURAL DRAW: the lightest on the share after E.
  * zhao_cmd_exec arm for 0x0302. The record previously fell to unsupported_o;
    the core's own FORGE CLUSTER section said so and that sentence is now out
    of date.
  * zhao_part_project OWNER_FORGE = 2'd2, the value its encoding has RESERVED
    since R68 sub-build 4 and its header commissioned in writing.

OWNER RULING R3 IS NOT TOUCHED, and this is the claim I checked hardest because
it is the comfortable one. R3 withholds a third PORT on zhao_project_service
and NAMES the time multiplex as the thing to keep; a third CLIENT on the front
mux is that multiplex doing its job. SHADOWSUB reached the same reading and
then WITHDREW its own arena-fill objection -- zhao_part_project takes particle
results straight out on q_* with no arena anywhere on that path -- and that
withdrawal is load-bearing here.

WHAT R3 STILL OWES IS NOT DISCHARGED AND IS NOT CLAIMED. The arm's existence
makes the FAIRNESS half measurable FOR THE FIRST TIME (before it,
tb_part_project drove the block STANDALONE and composed multi-client throughput
had never been measured from any bench in this tree). The RATE half needs a
per-client per-frame demand figure this composition does not produce.

THE ARBITER REWRITE IS BEHAVIOUR-PRESERVING FOR TWO CLIENTS, and that is a
property rather than a hope. The two-way prefer_p_q toggle became a three-way
rotating priority; with f_valid_i low the grant sequence is IDENTICAL -- from
turn 0 with both asking, geometry takes it and the turn moves to 1, particles
take it and the turn moves to 2, forge is not asking so the scan wraps. g, p,
g, p. The ten smoke forms are what says so.

=========================================================================
4. ONE OWNER DECISION, FOUND BY TRYING TO WIRE IT
=========================================================================
zhao_material_window is keyed by a PAIR: material_set (handle32) and
material_id (u16). Every draw in the ABI carries handle32[material_set] --
EXCEPT DrawProcedural, which carries handle32[material], AND handle32[material]
OCCURS EXACTLY ONCE IN ALL OF spec/commands.zidl, ON THAT LINE. No other
command names that resource kind, tools/pack writes no page keyed that way, and
NOTHING IN THE TREE STATES HOW ONE BECOMES THE OTHER.

zhao_forge_assemble presents the draw's handle as the SET and FORGE_MATERIAL_ID
(0) as the entry -- "a set of one, entry zero", the only reading available that
keeps the GAME in control of a procedural draw's material. BOTH HALVES ARE
PARAMETERS at the module edge, so a ruling that reads it differently moves two
lines and NOT ONE ABI BYTE. This is an interpretation of an existing field,
exactly like forge_kind (R108) and frame_tick (R241), and both of those were
owner rulings. DOCKED.

AND THE ART VALUES HAVE NOT BEEN LOOKED AT. A forge primitive's PRE-LIT colour
has no producer -- the frozen page carries anchors, axes, radii, a seed and a
phase and NO colour field -- so FORGE_LIT_R/G/B and FORGE_ALPHA are named core
parameters, the treatment R133's D-FORGESHADOW-A already accepted for
cast_strength_i. They are CHOSEN BY REASONING AND NOT YET BY LOOKING and the
parameter comment says so. Nothing has been rendered. Expect to move them.
DECLARED: the colour is per PRIMITIVE, so a forge triangle's Gouraud plane is
FLAT; a per-corner colour is three inputs instead of one the day the page
carries one.

GEOM_ATTR_SLOT_ALPHA = 6 is NAMED, not added. It was the only one of the
ruling-5 packet's seven slots without a parameter, which read as though the
packet were six wide plus a spare. zhao_geom_replay's own ATTRW comment
enumerates the store as "(u_over_w, v_over_w, r, g, b, alpha)" and
GEOM_ATTR_STORE_W is (GEOM_CLIP_ATTRS - 1) * 32, so slot 6 is alpha and always
was.

=========================================================================
5. WHAT STILL BLOCKS THE OTHER TWO, NAMED PRECISELY
=========================================================================
FORGE.SHADOW -- ONE ABSENCE, AND IT IS TWO LINKS SHORTER THAN THIS MORNING.
NOT TOUCHED by this packet.

The tap_* half is READY: zhao_terrain_tapshare is built and waiting,
zhao_terrain_heighttap is composed, and htp_height is COMPUTED ON EVERY TAP AND
READ BY NOTHING in the console today -- live silicon whose only customer is
this block.

What it lacks is a CASTER. zhao_geom_lodstate is the only block in the tree
emitting {world x, world z, radius, 2-bit rung, src_id} together -- five of six
fields at exact width and type -- and it was three uncomposed blocks deep.
TWO OF THOSE LINKS ARE NOW BUILT BY THIS PACKET: the adapter is at SIX
requesters (which is what zhao_geom_ladderbank needed) and the client-A THIRD
ARM exists (which is what lodstate's pr_* needed). What remains is ladderbank
itself, zhao_geom_projradius, lodstate, and a FAN ASSEMBLER -- and the
GEOM.CLIP door and the tap arbiter are both built and waiting. It is now
ordinary engineering end to end.

cast_strength_i is an AUTHORED CONSTANT by R133's D-FORGESHADOW-A, not a signal
to hunt for. Two candidate casters were checked and refused ON THE MERITS:
zhao_geom_meshfetch's cull_c{x,y,z}_o/cull_radius_o is a 3D BOUNDING SPHERE and
not a ground-contact footprint, and carries no rung; and zhao_cmd_exec's
stamp_tx_o/stamp_ty_o/stamp_radius_o is SURFACE.STAMP's terrain brush -- four
fields at exact width, LIVE IN THE CORE, and semantically wrong. Wiring it
would put a shadow under every crater and none under a creature.

FORGE.CLIFF -- R240 RE-SEARCH: NONE EXPIRED, TWO SMALLER, ONE NEW.
Recorded durably in design/contracts/FORGE.CLIFF.md.

1. PAGE ISSUER -- REAL, unamended. cmd_page_ci_i/cmd_page_cj_i occur in three
   files: the two rivals and the generated pricing top. The candidates were
   READ: zhao_terrain_jobissue is a real composed patch walker but emits 6-bit
   SUBPATCH-LOCAL origins with no absolute cell index, no extent and no lattice
   stride. blocks.yml's upstream: [TERRAIN.TESS] is not a lead -- that ledger's
   own GEOM.SETUP row names this row as design intent, not a wiring claim (R180).
2. SOLID WINDOW -- TRUE IN LETTER, AN ADAPTER IN FACT. RE-GRADE IT. Zero
   solid*_o ports tree-wide, so the sentence stands -- but
   zhao_terrain_compcache_front's cs_substance_o is a COMPOSED, LIVE,
   AUTHORITATIVE per-cell solidity oracle already routed into zhao_terrain_tess.
   What is missing is a 34x34 sweep adapter, and the halo ring is outside the
   5-bit cell index, which is exactly this contract's own "off-lattice loads as
   0" case. THE HALO IS FREE, NOT A PROBLEM. Calling it a missing PRODUCER
   sends the next reader to build a second oracle beside a working one.
3. VDIST MASTER -- REAL, but the prose overstates it. "Nothing produces a vdist
   field" is FALSE and sends the reader to rebuild ratified arithmetic:
   zhao_terrain_project.s6_invw and zhao_terrain_wcache's payload field
   [73:42] invw, Q16.16 ARE that number. What is absent is an ADDRESS-READABLE
   table indexed by cj * lat_w + ci.
4. NEW, AND NO PASS HAD LISTED IT: NOTHING CONSUMES A RIM EDGE.
   zhao_forge_cliff_ram emits zref::forge::RimEdge, and edge_ci_i, edge_side_i,
   edge_span_i and rim_edge have ZERO HITS IN ALL OF fpga/rtl. blocks.yml's
   GEOM.SETUP row already enumerates that none of the three forge output shapes
   is what that port takes. SO FEEDING THIS BLOCK FULLY WOULD STILL CLOSE
   NOTHING -- the sentence a future cliff packet needs BEFORE it starts on the
   issuer. The contract's own Notes said "the emission stage that turns an edge
   into a quad is not written"; the blocker LIST never picked it up, which is
   this repo's most repeated shape.

=========================================================================
6. FOUR THINGS THE TOOLS CAUGHT THAT NOTHING ELSE COULD HAVE
=========================================================================
THE LINT FOUND A SILENT DECODE BUG. The page bank's first draft read anchor0
one 32-bit WORD LOW -- out of rsv0 -- while every field beside it was right. No
counter, no handshake and no verdict could have seen it; a lightning bolt would
simply have started in the wrong place. verilator -Wall's UNUSEDSIGNAL, naming
the bits the record HAS and the decoder never read, is what found it. ON A
RECORD DECODER THOSE WARNINGS ARE THE CHECK, and silencing them wholesale is
how a frozen byte offset goes wrong quietly.

THE SMOKE HARNESS LIED IN THE FLATTERING DIRECTION, THREE FAILURES DEEP, AND I
NEARLY SHIPPED ITS GREEN. My first sweep reported ten forms rc=0 fatals=0. NINE
OF THEM WERE NOT EVIDENCE:

  1. `& .\\run_console_core_smoke.ps1 "-$f"` passes a STRING, which binds
     POSITIONALLY to $Repo -- EXACTLY WHAT THAT SCRIPT'S OWN HEADER WARNS ABOUT
     ("passing a flag through a shell VARIABLE ... binds positionally as $Repo").
  2. The bad repo path then resolved against the PRIMARY working directory, not
     $PWD, so it went looking inside the COORDINATOR'S CHECKOUT -- CLAUDE.md's
     [IO.File] trap reached through a .NET call inside the script.
  3. $LASTEXITCODE AFTER A POWERSHELL *SCRIPT* CALL IS THE LAST *NATIVE*
     COMMAND'S CODE, i.e. verilator's. The script threw a
     DirectoryNotFoundException and my harness read the simulator's 0.

Three silent failures stacked, every one reading PASS. THE TELL WAS DURATION --
secs=0 on a form whose sibling took 285 s -- which is this packet's own brief
arriving from the other side: it warns that RC 1 in a second is a verilation
failure, and this was RC 0 in ZERO seconds, which is worse because nothing
about it looks like an error. The harness was rewritten to SPLAT a real switch,
pass -Repo explicitly and absolutely, and judge on $? and on the absence of
exception text. THE SWEEP BELOW IS THE SECOND ONE.

THE DIRECTED TEST WAS FIRED BEFORE ITS GREEN WAS QUOTED. Shifting the expected
screen x by ONE pixel -- a plant in the test's own oracle, not in the RTL --
turned 83 passes into 17 FAILURES naming exactly the join checks ("corner B is
vertex i1's x (the LATCHED index): expected 0x3F0, got 0x3EF"). Restored by
CONTENT check and re-run to 83 green.

A POSITIVE CONTROL HAD TO MOVE, AND ITS AUTHOR HAD SAID SO IN ADVANCE.
part_project_directed went red on its own terms: its loop fired
owner_unroutable_o on owners 2 AND 3 as the two UNCLAIMED encodings, and
FORGE.PRIM has just claimed 2. The test's own comment reads: "nothing here goes
red when GEOM.LOD claims owner 2 and makes it routable; at that point this
control moves to owner 3 and the law is unchanged." Carried out rather than
argued -- AND THE OTHER HALF ADDED, because removing owner 2 from the exclusion
list is only honest if owner 2 is actually DELIVERED. A demux arm added to the
detector's exclusion list AND NOWHERE ELSE would pass the amended control and
drop every forge vertex silently -- a counter reading zero about a path that
does not work, which is the MIRROR of the fault the counter exists for. G2b
forces owner 2 and requires BOTH that the detector stays put AND that
rf_valid_o FIRES.

vtx_overflow_o OWES NO COMMITTED MUTANT, and that was checked rather than
assumed. It looked like the wq_overflow_o shape, but the vertex port has no
contract bounding the count, so offering MAX_VERTS + 1 reaches it with LEGAL
STIMULUS. The test asserts the PAIR: SILENT at exactly MAX_VERTS, FIRING at one
past it, and slot_pressure_o staying PUT across both (R95).

=========================================================================
7. COST, RECORDED AND NOT ARGUED (R236). NO QUARTUS WAS RUN.
=========================================================================
  * zhao_forge_pagebank: 0 DSP, 0 M10K. One 512-bit line register, three
    captured lines, a 16-state sequencer, eleven counters -- order 1,700 flops.
  * zhao_forge_assemble: 0 DSP of its own. Two vertex stores, 520 x 43 and
    520 x 24 bits. WHETHER THEY INFER M10K OR LAND IN FLOPS IS THE FIT
    QUESTION, and DEPTH rather than bit count is the risk -- TERRAIN.SHEETSEAM's
    row already records that for a 1,089-word plane.
  * The zhao_geom_depthquant_stream + zhao_raster_rcp24_v4 pair is a SECOND
    INSTANCE of one law (ruling D-4), not a second law, and the core's own
    header already contemplates and prices it.
  * The other four blocks were priced by their own packets.

=========================================================================
8. GATES, ALL RUN BARE
=========================================================================
  check_console_inventory           RC 0
  check_prod_manifest               RC 0
  check_quartus17_syntax            RC 0   self-test 13 fire / 22 no-fire
  check_case_labels                 RC 0
  mutant_drivers                    RC 0
  mutant_copy_drift (AFTER commit)  RC 0   60 copies / 371 modules
  uncashed_cheques                  RC 0
  refmodel_liveness                 RC 0
  duplicate_functions               RC 0
  check_counters                    RC 0
  check_findings_citations          RC 0   no NEW dangling
  wrapper_port_parity               RC 0   1361 = 1361, missing 0, stale 0
                                           (it said 49 MISSING first -- R162 working)
  packet_h_tieoff_audit (core)      RC 0   unchanged
  gen_prod_top --check              RC 0   fresh, 74 instances
  gen_console_board --check         RC 0   fresh, 1362 core ports
  gen_shell_paired_diff --check     RC 0   and --check --mutant RC 0
  completion_register               RC 1 = 14 gaps (NORMAL)
  cmake --preset windows-native     RC 0

DIRECTED TESTS, BUILT AND RUN (R60):
  forge_assemble_directed     83 checks  RC 0   NEW; plant fired 17/83
  part_project_directed       82 checks  RC 0   control moved + G2b added
  geom_mem_adapter_directed   35 checks  RC 0   at N=6
  cmd_exec_directed          977 checks  RC 0   with the 0x0302 arm

THE TEN SMOKE FORMS, ONE FROZEN TREE. `git status --porcelain` was EMPTY at
both ends of the sweep, so the closure never moved under it.

  PLAIN         ok fatals=0  265 s  raster pixels=2560  frames_admitted=1
  Mutant        ok fatals=0  309 s  (slot-overflow wrapper, inverted polarity)
  UntexMutant   ok fatals=0  311 s  (untex-decl wrapper, inverted polarity)
  NoTableLoad   ok fatals=1  284 s  INVERTED -- passes WITH one %Fatal
  BadDescriptor ok fatals=1  284 s  INVERTED -- passes WITH one %Fatal
  BadVertex     ok fatals=0  278 s
  NoEchoArm     ok fatals=0  269 s  raster pixels=2560
  BadTraceArm   ok fatals=0  274 s  raster pixels=2560
  GlowTag       ok fatals=0  273 s  raster pixels=2560
  LintOnly      ok fatals=0    0 s  (see below)

ALL TEN MATCH THE BASELINE EXACTLY -- seven at fatals=0, the two inverted forms
at fatals=1, GlowTag at fatals=0. RASTER PIXELS = 2560 UNCHANGED, which is the
number that says the clipdoor did not perturb the mesh path and the three-way
arbiter did not perturb the projector's grant order.

BOTH COMMITTED CORE MUTANTS STILL FIRE. UntexMutant matters most here: the
untextured declaration moved from a console PARAMETER to the door's
`o_untex_o`, and that wrapper sets the parameter feeding client 0's bit, so its
positive control is still live through the new path rather than bypassed by it.

AND THE ZERO-SECOND LintOnly WAS CHECKED BEFORE IT WAS QUOTED, because a gate
returning instantly is exactly the shape this repo says to distrust. It is a
real `--skip-identical` CONTENT-hash cache hit: I ran the same form earlier in
this session at 25 s against this same closure, and nothing in it has changed
since. The script's own header records the proof that the warm directory still
fails on real damage ("planting `this_is_not_systemverilog endmodule` ...
returns nonzero with a real %Error in 0.7 s"). The cache hit is evidence from
the elaboration it matched.

TWO BENCHES OWED A PORT, which is the instantiation-chain cost of widening a
composed block, paid here rather than discovered by the next configure.
tb_cmd_exec_pair carries the whole 0x0302 arm OUT; tb_geom_mem_adapter NAMES
requester F while holding it idle. Both were found by cmake --preset failing on
PINMISSING, which is the gate working.

=========================================================================
9. FOR THE COORDINATOR -- what is next, and for whom
=========================================================================
  1. FORGE.SHADOW IS TWO LINKS SHORTER than this morning: the adapter is at six
     requesters and the client-A third arm exists. What remains is
     zhao_geom_ladderbank, zhao_geom_projradius, zhao_geom_lodstate and a FAN
     ASSEMBLER -- and the door and the tap arbiter are both built and waiting.
  2. ONE OWNER DECISION IS DOCKED: DrawProcedural.material's mapping to
     (material_set, material_id). Section 4. It is two parameters, not a rewrite.
  3. THE ART VALUES NEED AN EYE, NOT A MEASUREMENT. FORGE_LIT_R/G/B and
     FORGE_ALPHA are placeholders chosen by reasoning. Nothing has been rendered.
  4. FORGE.CLIFF HAS A FOURTH BLOCKER and it is on the OUTPUT side. Do not
     brief a cliff packet against the three-item list.
  5. THE AREA QUESTION IS NAMED AND UNANSWERED. Two 520-entry stores, an extra
     reciprocal pair and a six-way share. No Quartus was run; the fit row is in
     design/fit_targets.yml.
  6. R3'S SCHEDULE PROOF is now MEASURABLE on the fairness half for the first
     time. It is still owed.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
