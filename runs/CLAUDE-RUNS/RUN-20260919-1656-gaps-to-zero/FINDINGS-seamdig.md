# FINDINGS — SEAMDIG (owner ruling R194 spent: the layer-F reader)

**Branch gz/seamdig, three commits, head 7db2b59a. Register 21 -> 21.**
**I32 did NOT close — the refusal names blockers R194 did not address.**

> TRANSCRIBED BY THE COORDINATOR — the harness refused this lane a report
> .md, the eighth in a row. 7db2b59a is an intentionally empty commit
> carrying the record.

---

## 5cbe2e62

SEAMDIG: R194 SPENT -- the layer-F reader built, and TERRAIN.BAKE gains Option A's per-vertex depth mode

OWNER RULING R194, 2026-09-20, by looking: "Shipped is fine. Slightly
different but not off." The nearest-texel rim is ACCEPTED, the terrain page
format is FROZEN AT 64x64, and zref::terrain::sheet_texel_for_vertex stands
as written -- it does NOT become the identity.

That ruling was the whole blocker. R116: "six terrain lanes have now closed
zero of the same four disconnected blocks... That is not six failures; it is
one blocker seen six times." The layer-F reader's ADDRESS GENERATOR was
exactly the contested thing. It is decided, so it is built.

WHAT IS NEW

* fpga/rtl/terrain/zhao_terrain_stampdepth.sv -- spec/terrain_rules.md 9.3's
  two stamp-to-bake laws in RTL for the first time. 9.3(a)'s ART TABLE:
  sixteen NAMED, EDITABLE fx16 metres (CLAUDE.md rule 6), indexed strength>>4,
  one symmetric round on the low-nibble delta, LAST SEGMENT HELD so strength
  255 is a value somebody chose. 9.3(b)'s nearest-texel address generator,
  tie broken downward, vertex 32 the only clamped one. Pure combinational, NO
  MULTIPLIER -- the (b-a)*fr interpolation is written as an explicit four-term
  shift-add so no tool can spend a DSP on it inside the block that exists to
  hold exactly one. Elaboration guard refuses SheetEdge != 64, citing R194.

  Entry I32 recorded a search of ALL of fpga/rtl for
  stamp_depth|kStampDepthTable|sheet_texel_for_vertex|depth_table returning
  ZERO HITS. This is where that stopped being true.

* zhao_terrain_bake_v2 gains cmd_depth_sheet_i (Option A's second depth mode),
  sheet_texel_o / sheet_strength_i (the layer-F read, on the same beat as
  layers A/B/C) and the counter sheet_vertices_dug_o.

  THE MODE IS ADDITIVE, and that is the claim that matters. A record leaving
  cmd_depth_sheet_i low is bit-identical to every record this block has ever
  baked: terrain_bake_v2_directed still passes 267/267, unchanged, measured.
  The sheet arm feeds v1's OWN scar arithmetic -- the no_bake clamp, the
  height16 rails, the 3.4 meets equality are shared, not duplicated. It is a
  new DELTA SOURCE, not a second scar pipeline.

  ONE zhao_terrain_stampdepth instance serves both halves, and that is a
  property rather than a coincidence: texel_o depends on vi_i/vj_i alone and
  the depth on strength_i alone. So the ADDRESS comes out against the LIVE
  dig cursor while the DEPTH comes out against the strength registered one
  state earlier. Asserted on all 1,089 beats, not left in a comment.

EVIDENCE -- both suites BUILT AND RUN, not merely linted (R60)

* terrain_stampdepth_directed  6,505 checks PASS. Exhaustive, because the
  input space is small enough that sampling would be a choice rather than a
  necessity: all 33x33 vertices against the address law, all 256 strengths
  against the table, the endpoints exact, the last segment held, the fx16 ->
  height16 rescale, coverage, and the composed stamp_depth_at_vertex over TWO
  disc sheets -- one interior, one with its rim TANGENT to the patch edge,
  which is the R194 placement. The tangent disc touches 4 of 33 far-edge
  vertices, the same order as R65's measured "4 of 99 shared border vertices".

* terrain_bake_v2_sheet_directed  6,548 checks PASS, with the counter FIRED
  BY STIMULUS and its negative control in the same executable:
    sheet_vertices_dug_o moved 0 -> 255 on a sheet record (radius 0, so the
    DISC law can write nothing at all and every moved height was moved by
    layer F);
    and stayed at 0 on a DISC record carrying the SAME full sheet, which
    still dug 109 vertices -- so the zero is a control and not an idle
    machine.
  Plus the 3.3 no_bake clamp fired on the sheet path (81 vertices) and the
  whole bake re-run under rolling backpressure with the same result.

  CLAUDE.md: a detector reading zero is a claim, and it is the claim to check
  hardest. This one was fired before being quoted.

TWO STALE CLAIMS FOUND AND STRUCK, both re-measured rather than inherited

* zhao_terrain_bake_v2.sv's header said zhao_prod_top.sv instantiates v1.
  It instantiates zhao_terrain_bake_v2 (u59_i), adopted by R86.
* the same paragraph said fpga/quartus/prod_fit_sources.txt carries the v1
  file. There is no such file: it is prod_fit_sources.ORPHANED.txt, whose own
  first line reads "ORPHANED 2026-09-09. NOTHING READS THIS FILE. Do not edit
  it and do not draw conclusions from it."
  A warning quoting a file nobody reads is worse than no warning -- it sends
  the next packet to repair something that is not broken.

WHAT THIS DOES NOT CLOSE, said plainly

Tie-off I32 does NOT close, and the dossier said so before the decision was
taken (DOSSIERCHECK's correction (b), and TERRAIN.PAGEIO.md section 8: "I32
does not close on this block alone"). R194 removed ONE of the four things
holding TERRAIN.BAKE out of the console, and it was the smallest. The other
three -- layer D has no reader anywhere in the machine, sc_* has no consumer
anywhere, and nothing in this console writes a height layer back to a page --
are ONE unbuilt block, TERRAIN.PAGEIO, with a written contract and no
design/blocks.yml row. NO TIE-OFF WAS CREATED by this commit and none was
moved; the completion register reads 21 before and 21 after, and the
superseded check reads 72 production roots CLEAN in both directions (R174).

REGISTRATION, because registering a block in three places is three acts

* design/console_inventory.yml -- a pending_compose disposition (G4)
* design/fit_targets.yml -- the new file into zhao_prod_top's closure.
  bake_v2 instantiated NO child before this, so every source list naming only
  the parent was complete and none of them is any more.
* tests/CMakeLists.txt -- ZHAO_TERRAIN_BAKE_V2_SV, the mutant target, and the
  new sheet suite
* zhao_prod_top.sv REGENERATED and the output READ: both new inputs take LFSR
  stimulus, both new outputs join the fold.
* tests/mutants/zhao_terrain_bake_v2_mutant.sv REGENERATED from current
  production with its one substantive line re-applied -- its own header
  demands exactly that, and a copy of an old version is a positive control for
  a block that no longer exists.

Gates at this commit: check_console_inventory OK, check_prod_manifest OK,
check_quartus17_syntax 0 forms, check_case_labels OK, check_counters RC 0,
duplicate_functions OK, uncashed_cheques RC 0, refmodel_liveness RC 0,
mutant_drivers RC 0, gen_prod_top --check fresh, gen_console_board --check
fresh, gen_shell_paired_diff --check fresh, packet_h_tieoff_audit 0 SILENT.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 037fb455

SEAMDIG: name the two new suites in TERRAIN.BAKE's ledger row, so the numbers are reproducible FROM THE TREE

`design/blocks.yml`'s TERRAIN.BAKE row has said `inputs: [stamp_results]` for
months with nothing behind it. R194 put something behind it -- Option A's
per-vertex layer-F depth mode, whose law is
`zref::terrain::stamp_depth_at_vertex` -- and the row went on naming only the
disc suites.

That gap has a measured cost in this exact subsystem, twice:

  * `terrain_bake_v2_directed` was quoted in `design/prod_manifest.yml` at
    "267/267 against the zref oracle" while NO CMAKE TARGET BUILT IT. The
    recipe lived in a header comment. The number was unreproducible from the
    tree for eleven days, and it reads in the flattering direction, which is
    why nobody looked: a number in a manifest with no way to re-run it is
    indistinguishable from a number that was re-run yesterday.
  * `design/blocks.yml` already records the identical fault against
    TERRAIN.NORMALMAP -- "no CMake target compiled the named suite".

So both new suites are named in the row that owns the capability, beside the
CMake targets that build them and the module header that cites them. Three
places, deliberately, because that is what it takes for the next person to
find one of them from wherever they start.

Gates re-run after this edit: refmodel_liveness RC 0 (95 declarations, 89
resolve), check_counters RC 0, uncashed_cheques RC 0, check_console_inventory
OK, check_prod_manifest OK, check_quartus17_syntax 0 forms, check_case_labels
OK, mutant_drivers RC 0, duplicate_functions OK. completion_register 21, with
`superseded check: 72 production roots CLEAN` -- unchanged in both directions.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 7db2b59a

SEAMDIG FINDINGS -- the harness refused a FINDINGS .md, so the record is here

The eighth lane in a row refused permission to write
`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-seamdig.md`
("Subagents should return findings as text, not write report files"). Per the
packet brief, the findings go in the commit messages instead and the
coordinator transcribes. This commit is deliberately empty of file changes --
it is the durable record, not a change.

=== 1. WHAT R194 ACTUALLY UNBLOCKED, AND WHAT IT DID NOT ===

BUILT (commit 5cbe2e62):
  fpga/rtl/terrain/zhao_terrain_stampdepth.sv -- spec/terrain_rules.md 9.3's
  two stamp-to-bake laws in RTL for the first time, and the per-vertex layer-F
  depth mode on zhao_terrain_bake_v2 that consumes it (Option A).

NOT CLOSED: tie-off I32. R194's own text says it unblocks "tie-off I32
(surf_res_*) -- directly", and the DOSSIERCHECK verification already corrected
that as OVERSTATED before the owner ruled. The correction is right, and I
re-measured all five blockers IN MY OWN TREE rather than inheriting them:

  (a) LAYER D HAS NO READER ANYWHERE. grep of every .sv/.v/.qsf/.txt under
      fpga/ for the page offset 6598 returns exactly TWO hits, and BOTH are
      the comments recording the previous search. Zero genuine uses. Bake
      needs layer D on TWO ports (vtx_nobake_i and cell_state_i).
  (b) NOTHING CONSUMES THE STAMP RESULT STREAM. grep of fpga/ AND tests/ for
      res_texel_i | res_strength_i | res_before_i: ZERO hits. I32's named
      consumer does not exist as a port anywhere.
  (c) vtx_nobake_i HAS NO PRODUCER. Every `output ...nobake` under fpga/rtl is
      a COUNTER (nobake_clamps_o). Zero producers of the section 3.3 shadow.
  (d) cmd_* HAS NO PRODUCER. zhao_terrain_cmd emits a PATCH DIRECTORY record
      (island/ix/iz/hps_addr/crc/flags/view_mask/priority), not a bake record
      ({cx, cz, radius, depth_from, depth_to, env_*}). Different packet.
  (e) TERRAIN.PAGEIO HAS NO design/blocks.yml ROW. grep: 0. Its contract file
      EXISTS (design/contracts/TERRAIN.PAGEIO.md). So the named owner of three
      open core entries (I27, I28, I32) is not a tracked capability, and
      uncashed_cheques.py cannot see it.

  AND blocks.yml names TERRAIN.BAKE as the SOLE downstream of stamp_results.
  So I32 needs an unbuilt SUBSYSTEM behind it, not a wire. That is the blocker
  R194 did not address, and it is live today.

  I also checked the one seam that would have been an honest shortcut and is
  not: zhao_terrain_writeback IS composed and does touch layer F -- but its
  input is a JOURNAL JOB (j_*) from u_terrain_jdoorbell and it reads the sheet
  from VRAM beats. It is not a consumer of a per-texel result stream.

=== 2. TWO STALE CLAIMS FOUND, BOTH IN zhao_terrain_bake_v2.sv's OWN HEADER ===

Both struck in 5cbe2e62, both re-measured rather than inherited:
  * "zhao_prod_top.sv instantiates zhao_terrain_bake -- v1". It does not. It
    instantiates zhao_terrain_bake_v2 as u59_i, adopted by owner ruling R86.
  * "fpga/quartus/prod_fit_sources.txt carries the v1 file". THAT FILE DOES
    NOT EXIST. It is prod_fit_sources.ORPHANED.txt, whose own first line reads
    "ORPHANED 2026-09-09. NOTHING READS THIS FILE. Do not edit it and do not
    draw conclusions from it."
  This is the sixth-or-so refusal in this subsystem found quoting a lapsed
  cause, and it is the worst kind: it would have sent the next packet to
  repair a real-looking problem using evidence that describes nothing.

=== 3. A FALSE-ABSENCE CHECK THAT CAME BACK TRUE, PLUS A GREP TRAP ===

Entry I32 cites `zhao_mem_share_n` as an existing arbiter of the right shape.
CHECKED, because fifteen "X does not exist" claims in this repo were false and
one asserted a PRESENCE that was false. It EXISTS and the citation is sound --
but `find fpga -name zhao_mem_share_n.sv` returns NOTHING, because the module
lives inside `fpga/rtl/memory/zhao_mem_share2.sv`. Anyone checking that
citation by filename will conclude it is absent. Recorded so the next reader
greps for `module zhao_mem_share_n`, not for a file.

=== 4. INSTRUMENT WORK: ONE COUNTER ADDED, FIRED BEFORE BEING QUOTED ===

sheet_vertices_dug_o on zhao_terrain_bake_v2. It exists because every other
counter on that block reports a wired-and-dead sheet path identically to a
working one -- surface_texels_touched_o counts covered vertices on EITHER law,
so it cannot separate them.

It was fired BY STIMULUS (the state is legally reachable, so no mutant is
wanted): 0 -> 255 on a sheet record whose cmd_radius_i is 0, a radius at which
the disc law provably writes nothing, so every moved height was moved by layer
F. Its NEGATIVE CONTROL is in the same executable: the SAME full sheet through
a DISC record leaves it at 0 while that record still digs 109 vertices -- so
the zero is a control and not an idle machine.

=== 5. THE EVIDENCE, ALL BUILT AND RUN (ruling R60), NOT MERELY LINTED ===

  terrain_stampdepth_directed        6,505 checks PASS
  terrain_bake_v2_sheet_directed     6,548 checks PASS
  terrain_bake_v2_directed (disc)      267 checks PASS -- UNCHANGED, which is
                                       the evidence the new mode is additive

All three were compiled with a STATIC direct g++ link and run, not through
ctest, per the packet brief's toolchain note about STATUS_ENTRYPOINT_NOT_FOUND
from the mixed winlibs/mingw libstdc++ looking exactly like a test crash.

A number worth recording because it corroborates the ruling from a second
direction: the seam-tangent disc in case 6 touches 4 of 33 far-edge vertices,
the same order as R65's measured "4 of 99 shared border vertices disagree".

=== 6. REGISTER, AND THE R174 CHECK ===

completion_register.py: 21 before, 21 after (9 tie-offs + 12 disconnected).
superseded check: 72 production roots CLEAN, before AND after -- so building a
child into a _v2 that is not composed manufactured no supersession violation,
exactly as R174 predicts.

NO TIE-OFF WAS CREATED. Every one of the 62 changed lines in
zhao_console_core.sv is a COMMENT line; zero RTL moved, so the
"INCOMPLETE -- TIED OFF, AND WHY" block needed no new declaration (R159), and
packet_h_tieoff_audit reports 0 SILENT.

The register did not move, and that is the honest outcome: this packet built
real function inside a block that is still disconnected for reasons R194 did
not touch. Moving the number would have required composing bake_v2 against
four unserved port groups, which is the thing the campaign's first rule
forbids.

=== 7. WHAT THE NEXT PACKET SHOULD DO ===

TERRAIN.PAGEIO is now the whole of it, and it is worth a subsystem packet
rather than another terrain lane: ONE block closes sc_* (the layer-B scar
writeback with no consumer), layer D's two reads, I27's deformation mark and
I28's writeback -- THREE core entries, one owner. It needs a design/blocks.yml
row first, because without one no gate can see that it is missing.

The sheet arbiter (the second requester on zhao_surface_sheet's req port) is
NOT on that critical path and was deliberately not built here: it would have
added a disconnected block without closing anything, and its scheduling policy
between a live stamp and a bake read is a decision, not a wire.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
=== 8. SMOKE GATE, ALL SIX FORMS, AT THE COMMIT PUSHED ===

  (no flag)      PASS   raster pixels=2560, frames_admitted=1
  -LintOnly      RC 0   silent
  -Mutant        PASS   terr_pl_slot_overflow_o fired 1 time -- "The detector
                        works; production's zero is a measurement."
  -BadVertex     PASS   one refused record dropped its batch (holes=1,
                        groups_poisoned=2, replay_poisoned=8) and the frame
                        still completed
  -NoEchoArm     PASS
  -BadTraceArm   PASS   the reserved bit was refused whole, nothing was armed

All six run with `powershell -File`, per the brief's warning that passing a
flag through a shell variable to `& .\script.ps1` binds it positionally as the
repo root.


---

