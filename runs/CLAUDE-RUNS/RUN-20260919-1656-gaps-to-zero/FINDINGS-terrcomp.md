# FINDINGS — TERRCOMP (terrain composition), 2026-09-20

**Branch `gz/terrcomp`, head `604c0ce3`, six commits, ZERO file changes.**
Register 21 → 21, all four compositions REFUSED with measured blockers.

> **TRANSCRIBED BY THE COORDINATOR.** The packet could not write this file —
> the harness refuses a `FINDINGS-*.md` from a subagent, the same as terrain6
> and terrain7 — so it put its findings in its commit messages and asked for
> them to be transcribed. What follows is those messages verbatim, in order.
> **That the packet had to route its own report through git is itself worth
> fixing**: a lane whose findings live only in commit subjects is one merge
> away from being unreadable.

---

## `8c7a141d`

FINDINGS-terrcomp: refuse all four terrain compositions, with measured blockers

The harness refuses a FINDINGS-*.md from a subagent, exactly as it did for
terrain6 and terrain7. Per those precedents this report IS the commit message;
transcribe it to
runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-terrcomp.md.

Branch gz/terrcomp from 75b96d266fa64509ca9a99bfad05bec26f5f290c on
claude/ceiling-architecture-20260912 (head confirmed by git ls-remote, not the
stale tracking ref).

ZERO GAPS CLOSED and that is the honest headline. Register 21 -> 21,
superseded check 73 production roots CLEAN -> CLEAN, tie-off audit unchanged.
No RTL changed, so no tie-off entry was created and none was owed. Seventh
terrain lane to close none of these four, and I am not claiming to be the
exception -- but three inherited blockers were WRONG in my tree, one in the
flattering direction.

=== 0. BASELINE AND FINAL, MEASURED IN MY OWN TREE ===
completion_register       21 (9 tie-offs + 12 disconnected + 0 unbuilt)  BOTH
superseded check          73 production roots CLEAN                      BOTH
packet_h_tieoff_audit     7 declared, 1 reasoned, 10 by group, 1 SILENT  BOTH
The 1 SILENT row (u_material_resolve.dir_valid_i) is pre-existing, not mine.
Next free core entry number remains I51.

=== 1. CORRECTED CLAIMS (five; two invert the conclusion) ===

1.1 THE BRIEF'S SPLIT OF 21 IS WRONG. It says "9 tie-offs + 11 disconnected +
1 unbuilt" and lists eleven modules. Measured: 9 + 12 + 0, and the twelfth is
zhao_geom_warp, absent from the brief's list. Total right, split wrong.
Nothing is unbuilt any more.

1.2 FALSE: "zhao_terrain_bake_v2 is already producing a supersession
relationship." Measured BEFORE touching anything: superseded check: 73
production roots CLEAN. It is producing no violation and cannot.

The mechanism is the half the brief's gloss of R163 drops:
superseded_in_roots() reports every INSTANTIATED module with a higher-versioned
sibling. zhao_terrain_bake (v1) is instantiated NOWHERE in fpga/rtl -- zero
sites, production or bench. No instantiation, no closure membership, no
violation. E1's zhao_terrain_patch_v2 case fired because zhao_terrain_patch IS
composed; bake's v1 is not, so the cases are not analogous. prod_manifest.yml
records v1 as deliberately retained on disk as the executable oracle v2 is
differenced against (267/267).

So "make sure the number moves the way you expect" had the correct answer: it
cannot move in either direction from anything done to bake.

1.3 SPENT: terrain7's D3 (zhao_prod_top wires two superseded modules).
Re-measured per R165. FULLY CASHED, BOTH HALVES.
  * zhao_prod_top.sv instantiates zhao_shell_top_v2 and zhao_terrain_bake_v2
    u59_i. terrain7's cited :3671/:4076 v1 sites are gone.
  * design/fit_targets.yml under `- top: zhao_prod_top` carries
    fpga/rtl/common/zhao_shell_top_v2.sv and
    fpga/rtl/terrain/zhao_terrain_bake_v2.sv, with a comment recording what
    left: "zhao_shell_top.sv, and the four blocks only it composed ... plus
    zhao_terrain_bake and its delta child."
D3 can be struck. R86's sequencing instruction was carried out.

1.4 THE INSTRUMENT FINDING: prod_fit_sources.txt has been misread FOUR times
in eleven days, and its own first line forbids exactly that.

This nearly became my own fifth instance. fpga/quartus/prod_fit_sources.txt
lists zhao_shell_top.sv and zhao_terrain_bake.sv and contains neither _v2.
Read alone that says the production fit dies with MODMISSING on two modules
zhao_prod_top instantiates. It says nothing of the kind. The file's FIRST TWO
LINES, in capitals:

    # ORPHANED 2026-09-09. NOTHING READS THIS FILE. Do not edit it and do not
    # draw conclusions from it.

It then names the real list (design/fit_targets.yml under
`- top: zhao_prod_top`), the real walker
(check_prod_manifest.py::check_fit_sources), and records that a 2026-09-09
sweep had already made this mistake once -- which is WHY the header exists.

Since that header was written the file has been quoted as evidence four more
times:
  2026-09-09  a resource sweep -- "a confident wrong conclusion about a failing
              production fit ... nearly acted on"
              (RESCUE-ROADMAP-CONSOLIDATED-20260909.md), the commemorated
              incident
  2026-09-20  terrain7 -- cites :38 and :122 as the evidence for D3
  2026-09-20  R86, IN OWNER-RULINGS-20260919-EVENING.md -- "fpga/quartus/
              prod_fit_sources.txt contains the v1 files while
              zhao_shell_top_v2.sv is absent from it entirely", load-bearing
              in a RULING
  2026-09-20  this lane's own recon subagent -- reported "a real defect ... a
              MODMISSING waiting for the next production fit"

The SAME rulings file also records the truth ("prod_fit_sources.txt is
orphaned (the real list is fit_targets.yml)"). It contains both the correct
fact and a ruling that contradicts it, about a thousand lines apart.

THE MECHANISM, AND IT IS THE POINT. Everyone reaches this file through a grep
for a module name. A grep returns
`121: fpga/rtl/terrain/zhao_terrain_bake.sv` -- a line indistinguishable from
a live source-list entry, carrying no marker. THE WARNING IS AT LINE 1 AND THE
EVIDENCE IS AT LINE 121, AND NOBODY SCROLLS UP. Prose at the top of a file is
invisible to the only access pattern anyone uses.

This is CLAUDE.md's standing shape -- the knowledge was written down,
correctly, and nothing read it back -- with the sting that the mitigation was
tried, is well written, is in capitals, and has a measured failure rate of 4/4.

RECOMMENDATION (coordinator call, not a packet's). Prose has been shown not to
work here. Make a grep hit self-labelling:
    git mv fpga/quartus/prod_fit_sources.txt \
           fpga/quartus/prod_fit_sources.ORPHANED.txt
so every future Select-String result carries the warning IN THE PATH, where
the reader is already looking. Deleting it is also defensible now that
fit_targets.yml is demonstrably the live list. I did NOT do either: renaming a
file that four reports and one ruling cite by path, mid-run, with concurrent
lanes gating, is a shared-artifact decision and not mine to take unilaterally.
The evidence above is what makes it cheap.

1.5 R163's DERIVED RULE IS RIGHT; ITS STATED MECHANISM IS NARROWER than the
brief repeats it. "A _v2 file that exists but is NOT composed CREATES a
violation" is false in general and measurably false for bake. The true
statement: a _v2 file that exists WHILE ITS V1 IS COMPOSED creates a violation.
E1's derived rule -- born in the commit that composes it -- still stands,
because the v1 is usually composed when somebody adds a v2. But a lane that
reads the brief's phrasing and then measures 73 roots CLEAN will think its
instrument is broken. It is not.

=== 2. THE FOUR REFUSALS (every blocker re-searched, not inherited) ===

SCALE, MEASURED: the four carry 89 input ports (81 excluding clk/rst_n).
With today's composed core, FEWER THAN TEN have a real producer -- bake's four
page-stream reads through the psmux, and essentially nothing else. That is the
size of the honest gap and the size of the dishonest composition available.

2.1 zhao_terrain_normalmap -- REFUSED. Owner decision R115 AND an absent
producer; either alone is sufficient.
 (a) R115 IS LIVE AND IS NOT MINE. Contract, ledger row, oracle and a
     registered 4,738-check suite (terrain_normalmap_directed, with
     terrain_normalmap_break_oracle as a fired WILL_FAIL control) -- and NO
     RATIFIED SPEC SENTENCE.

     RE-MEASURED MYSELF rather than cited, with a positive control on my own
     instrument (the campaign's own law: measure whether a tool can see its
     subject before quoting it):
         grep -rniE "normalmap|normal[ -]map" spec/     ->  0 hits
         grep -rniE "terrain\.shade|terrain_rules" spec/ -> 14 hits
     So spec/ does discuss terrain blocks and the sweep can see them; it finds
     no normal-map sentence because there is none. terrain7's wider pattern
     returns 37 hits, and I confirmed EVERY ONE is the word "bump" in the sense
     of a version bump -- which is why the wider pattern should not be quoted
     as though it found something.
 (b) EVEN IF RATIFIED, THE FRAGMENT STREAM HAS NO PRODUCER. Inputs are
     f_valid_i/f_ready_o/f_u_i[S15.16]/f_v_i[S15.16]/f_detail_i/f_lod_i[LODW]/
     f_src_id_i. The nearest thing is zhao_geom_bin_pipe_v2's
     stage_fragment_*, which has no u, no v, no lod, no detail and no `ready`
     companion -- the core calls it "a structural probe ... not a handshaked
     stream". And zhao_terrain_normalmap is instantiated NOWHERE AT ALL: not
     in the core, not in zhao_prod_top, not in any bench. It is the only one of
     my four absent even from the generated census top.

2.2 zhao_terrain_velocity -- REFUSED. Same absent owner as I34's height lane.
The block drives an address (vtx_vi_o/vtx_vj_o) and expects the producer to
answer on lane_valid_i / lane_velocity_i (earth out-lane 1, fx16) /
lane_covers_i (the section 9.1 closed-interval answer).

The blocker is precise, not "no producer" loosely: FIELD.SEQ.EARTH was RULED
NEVER TO EXIST as a block (design/contracts/FIELD.SEQ.EARTH.md), and the FIELD
v3 fabric IS composed as u_field_host -- with clients S and F only. Velocity is
out-lane 1 of the same E-profile evaluation whose out-lane 0 the height lane
uses, so it is blocked by exactly what core entry I34 is blocked by: no
E-profile client adapter and no uniform descriptor producer (CMD.EXEC's
TerrainField arm). Plus its own "two walkers over one page" scheduler.

I34's own argument governs and applies verbatim: a constant on the lane "is not
a neutral value -- it is a field program that raises or lowers every vertex of
every patch by the same amount, and it would be invisible because the result is
still a real composed height." Velocity and I34 should close together.

2.3 zhao_terrain_bake_v2 -- REFUSED. terrain7's inversion holds, AND IS TOO
KIND. "The input is served, the output is the hole" is true of THREE of bake's
FIVE page port groups, not all of the read side.

design/contracts/TERRAIN.PAGEIO.md -- written 2026-09-20 by terrain8,
deliberately with NO design/blocks.yml row -- already records the correction,
and the direction of terrain7's error is the UNFLATTERING one: there is MORE
missing, not less. Bake touches the page on FIVE port groups:
    vtx_base/scar/bottom (+vi/vj)   layers A,B,C  read   SERVED by pagestream
    vtx_nobake_i                    layer D       read   NOBODY
    cell_state_i (+ci/cj/valid/ready) layer D     read   NOBODY
    sc_*                            layer B       write  NOBODY
    cs_*                            layer D       write  NOBODY
The contract's evidence, which I re-ran rather than inherited: layer D sits at
page offset 6,598 (zref_terrain_page.hpp kLayerDOff) and that offset appears
ZERO times under fpga/ as a page offset; zhao_terrain_pagestream reads exactly
'{A_OFF, B_OFF, C_OFF}; zhao_terrain_writeback touches layer F only.
TWO OF THE EIGHT PAGE LAYERS HAVE NO READER IN THE MACHINE, and both are on
TERRAIN.BAKE's critical path.

  * The part that IS served: u_terrain_pagestream emits v_base_o/
    v_scar_o/v_bottom_o/v_vi_o/v_vj_o against bake's vtx_base_i/vtx_scar_i/
    vtx_bottom_i and its two index outputs -- port for port, same widths, same
    signedness, same vi=column/vj=row convention. u_terrain_psmux already
    shares that stream between two clients, so a third is a widening of a block
    with a contract and a test.
  * sc_* -- BAKE'S LAYER-B SCAR WRITEBACK -- HAS NO CONSUMER. CONFIRMED
    INDEPENDENTLY: I swept every input/output line in fpga/rtl/**/*.sv for
    "scar". EVERY hit is a composition READ (zhao_terrain_patch.scar_i,
    zhao_terrain_patch_acc.in_scar_0..3_i, zhao_terrain_mipfeed.v_scar_i,
    zhao_terrain_pagestream.v_scar_o). NOTHING IN THIS CONSOLE WRITES A HEIGHT
    LAYER BACK TO A PAGE. A bake whose scar cannot be stored has not deformed
    anything -- it computed a deformation and dropped it.
  * Layer D has no reader either (terrain8's correction to I32), and the cs_*
    seam onto compcache_front DROPS section 3.3's six flag bits, because the
    cache stores only logic [1:0] sub_m.
  * vtx_nobake_i has no producer, and the cmd_* record (sixteen ports) needs
    the SURFACE.STAMP -> patch-bake-record adapter, which requires 9.3(b)'s
    sheet_texel_for_vertex -- BLOCKED BEHIND R65, an ART judgement on
    reports/terrain-seam-dig/seam_dig_contact.png that only the owner's eye can
    settle.
THE MISSING BLOCK IS NAMED AND ITS CONTRACT IS WRITTEN:
design/contracts/TERRAIN.PAGEIO.md EXISTS and has NO design/blocks.yml row --
so the named owner of three open core entries is not a tracked capability and
uncashed_cheques.py cannot see it. That one block closes sc_*, I27's
deformation mark and I28's writeback: THREE ENTRIES, ONE OWNER.

2.4 zhao_terrain_lod -- REFUSED, and this is the one that looked closest. The
core has a pre-built, commented insertion point for it, which is exactly what
makes it dangerous.

WHAT IS GENUINELY THERE. u_terrain_lodfeed is composed and emits w_valid_o/
w_slot_o/w_sp_o/w_dev1_o/w_dev2_o/w_dev3_o/w_cy_o/w_src_id_o.
zhao_terrain_devstore's w_* inputs match that PORT FOR PORT. The core's own
comment at tlf_w_ready promises the join: "When entry I21 closes and
zhao_terrain_devstore joins this stream, this becomes the AND of the two
readies and NOTHING ELSE CHANGES." And devstore's r_* outputs -- r_dev1/2/3_o,
r_prev_level_o, r_prev_morph_o, r_hold_o -- match zhao_terrain_lod's
sp_dev1/2/3_i, sp_prev_level_i, sp_prev_morph_i, sp_hold_i exactly, at DEVW=24
and MORPHW=17. The chain lodfeed -> devstore -> lod -> devstore.h_* is real and
was designed.

WHY IT STILL CANNOT CLOSE HONESTLY. Six absences, measured:
 1. sp_cx_i / sp_cz_i (subpatch centre world X and Z) HAVE NO PRODUCER.
    devstore carries r_cy_o -- the centre HEIGHT -- and nothing else
    positional. X and Z would have to be reconstructed from r_sp_o plus a patch
    world origin that never arrives on this path.
 2. sp_src_id_i HAS NO PRODUCER THROUGH THE STORE. Measured:
    zhao_terrain_devstore.sv contains the string "src_id" ZERO times. lodfeed
    produces w_src_id_o, the store does not carry it, so the id is dropped at
    the join and cannot reach the selector.
 3. cam0/1_scale_i needs zhao_measure_governor, ITSELF one of the twelve
    disconnected -- closing one gap by opening another. Worse, R83 rules it
    currently WRONG: the derived proj reaches 443.41 while the governor's port
    is [15:0] Q8.8 capping at 255.996, so it saturates below 90 deg hfov and

> **RETRACTED BELOW — READ ADDENDUM 2.** This item quotes R83's Q8.8
> saturation as LIVE. It is not: the widening is DONE, `PROJW = 20`, Q12.8 in
> 20 bits, with a real producer. TERRCOMP self-corrected this ~270 lines
> further down, **where a grep for `Q8.8` never lands** — which is owner
> ruling R175's shape exactly (a warning the reader's search window never
> shows them). Pointer added by the coordinator 2026-09-20 on packet
> POSTMEAS's recommendation; the retraction stays where its author wrote it.

    PEGS THE LOD LADDER AT ITS FINEST RUNG -- maximum triangle cost, no visible
    symptom, no counter that can see it. Wiring it first composes a circuit
    already known wrong.
 4. edge_nz/pz/nx/px_i (neighbour edge levels) have no producer and ownership
    is REFUSED IN WRITING in MEASURE.GOVERNOR.md. The only driver anywhere is
    an LFSR in the generated census top.
 5. zhao_terrain_devstore is not composed, is instantiated nowhere, and HAS NO
    design/blocks.yml ROW AT ALL -- not even a tracked capability. It costs
    185 M10K of 553 (33%), and composing it OWES I27's terr_chk_* staleness
    check in the same commit, because the store keys by slot and an eviction
    mid-walk would file page A's deviations under page B.
 6. THE OUTPUT HAS NO CONSUMER. TERRAIN.PROJECT is zhao_terrain_lod's only
    declared downstream and is NOT composed. And I21 already refuses the
    obvious alternative in writing: the selector's output is "EXACTLY
    zhao_terrain_tess's job port" but NOT zhao_terrain_group_seq's, which
    additionally needs job_view_mask, job_mat_a, job_mat_b and job_weight --
    TERRAIN.LOD emits none of the four, and inventing them at the composer is
    the hidden-adapter failure.

COMPOSING zhao_terrain_lod ALONE WOULD CREATE ROUGHLY TWENTY-TWO TIE-OFFS
(sp_cx, sp_cz, sp_src_id, ten cam*, hyst, min_hold, morph_step, dual, four
edge_*) AND DANGLE A THIRTEEN-SIGNAL OUTPUT PORT. The register would have read
21 -> 20. That is precisely C1's GEOM.WARP shape and precisely what R159 says
the number cannot see.

=== 3. THE R115 DECISION, PREPARED BOTH WAYS ===
The brief asks what I would do under each outcome. The answer is nearly the
same under both, and THAT is the useful finding: R115 is not currently on the
critical path for anything.

IF THE OWNER RATIFIES: ratification alone CLOSES NOTHING, because blocker (b)
survives it -- there is still no handshaked per-fragment u/v/lod/detail stream
anywhere in the console. The work that becomes real, in order:
 1. Write the spec sentence AGAINST THE CONTRACT, NOT AGAINST THE RTL. The
    ledger row warns of exactly this; writing one now to match the
    implementation ratifies whatever got built. The contract and
    zref::terrain::normalmap_delta_s9 predate the RTL and are the honest
    source.
 2. A FRAGMENT TAP IS THE ACTUAL COST, and it is a block, not a wire:
    zhao_geom_bin_pipe_v2's stage_fragment_* must gain u, v, lod, detail and a
    ready. That is a port change on a composed block, so it costs its whole
    instantiation chain plus every bench.
 3. Then compose normalmap between TERRAIN.SHADE and TERRAIN.PROJECT -- both
    of which are THEMSELVES UNCOMPOSED (zhao_terrain_shade lives inside
    u_terrain_lightlane; zhao_terrain_project is absent entirely).
So ratification buys a queue position behind two other subsystems. Ledger's own
estimate: ~380 ALM / 2 DSP / 8 M10K for the block, plus the tap.

IF THE OWNER SUPERSEDES: clean, cheap, immediate, and the ledger has already
done most of the paperwork:
 1. Set superseded_by: on the TERRAIN.NORMALMAP row with the ruling cited, in
    superseded_verdict()'s required form (ruling + replacement + a BUILT test).
    There is no replacement block, so the note must say the capability is CUT,
    not replaced -- the row already carries cut_order: 1, the ONLY cut_order: 1
    in my four.
 2. THE CUT SEAM IS A PROPERTY, NOT A PROMISE, and this is what makes it safe:
    strength = 0 is a BIT-EXACT NO-OP (zref::terrain::normalmap_is_noop), so
    removing the block changes nothing else in the pipeline. Unusually clean,
    and designed in.
 3. The register drops 21 -> 20 HONESTLY, by the same mechanism that already
    excuses TERRAIN.ISLAND and TERRAIN.VISIBLE ("superseded by a ruling: 2").
 4. Keep the RTL and the 4,738-check suite on disk. They cost nothing
    uninstantiated and the block is genuinely finished.

MY RECOMMENDATION: SUPERSEDE, or defer explicitly with a ruling citation. The
owner's own words in the ledger are the argument -- "normal maps are not the
thing presently threatening it. The broken texture storage structures are."
The block is ~4% of the ALM the texture recovery is already targeting, its two
consumers are both uncomposed, and its producer does not exist. THIS IS THE ONE
GAP IN MY SET THAT AN OWNER SENTENCE CAN CLOSE TODAY, and it is the cheapest
gap left anywhere on the board.

ONE THING THE OWNER SHOULD KNOW BEFORE CHOOSING: the detail normal has NO Y
COMPONENT BY CONSTRUCTION, so under a sun at the zenith the relief fades out.
That is declared in the ledger rather than discovered, and it is why the
look-gate was specified as a MOVING SUN AT 240p rather than a still frame.
Under the art law that is a LOOK, and it has never been taken.

=== 4. WHAT THIS LANE NEEDS NEXT, in dependency order ===
 1. R115 -- one owner sentence; closes a gap today. The only cheap one.
 2. R65 -- the owner's eye on seam_dig_contact.png. Still unspent, seven lanes
    deep. Gates TERRAIN.BAKE's Option A and the terrain page format, and the
    packer still does not exist, so the format is still cheaper to change now
    than it will ever be.
 3. TERRAIN.PAGEIO -- the page writer. Contract written, no blocks.yml row, no
    RTL. Closes sc_*, I27's deformation mark and I28's writeback: three
    entries, one block. The highest-leverage unbuilt thing in terrain.
 4. MEASURE.GOVERNOR's R83 Q12.8 widening, then compose governor +
    zhao_view_projscale -- after which cam*_scale_i stops blocking TERRAIN.LOD
    and I21.
 5. zhao_terrain_devstore's 185 M10K is an owner-sized decision (33% of the
    device's memory) and should be put as one, not discovered inside a
    composition packet.

=== 5. FALSE-ABSENCE / FALSE-PRESENCE LEDGER ===
 FALSE (presence)  brief: bake_v2 "already producing a supersession
                   relationship" -- 73 roots CLEAN, v1 instantiated nowhere
 FALSE (presence)  my own recon subagent: prod_fit_sources.txt staleness is
                   "a MODMISSING waiting for the next production fit" -- the
                   file is orphaned and says so in line 1
 EXPIRED           terrain7 D3: zhao_prod_top wires two superseded modules --
                   both halves cashed
 WRONG SPLIT       brief: "11 disconnected + 1 unbuilt" -- measured 12 + 0
 TOO BROAD         brief's gloss of R163 -- the violation needs the v1 COMPOSED
 TRUE, re-confirmed  sc_* has no consumer; layer D has no reader; edge_* has no
                   producer; normalmap's fragment stream has no producer;
                   spec/ has zero normalmap sentences
 NEW               zhao_terrain_devstore carries NO src_id at all (0
                   occurrences), so zhao_terrain_lod.sp_src_id_i has no
                   producer even with the store composed
 NEW               design/contracts/TERRAIN.PAGEIO.md exists with NO blocks.yml
                   row -- the named owner of three open entries is untracked

=== 6. GATES AT THIS COMMIT ===
All run in my worktree with tools/env/zhao-env.ps1 sourced, using the
coordinator's corrected paths. The four the brief placed under tools/design/
DO exist at the corrected paths and all pass -- confirming the correction.

check_console_inventory.py          RC=0
check_prod_manifest.py              RC=0
check_quartus17_syntax.py           RC=0
check_case_labels.py                RC=0
mutant_drivers.py                   RC=0
uncashed_cheques.py                 RC=0
check_counters.py                   RC=0
refmodel_liveness.py                RC=0
gen_prod_top.py --check             RC=0  fresh, 71 instances
gen_console_board.py --check        RC=0  FRESH (1274 core ports, 72 params)
gen_shell_paired_diff.py --check    RC=0  fresh (59 shared in, 91 compared out,
                                          4 declared divergent)
completion_register.py              21, superseded 73 roots CLEAN (RC 1 normal)
packet_h_tieoff_audit.py            7 declared, 1 reasoned, 10 group, 1 SILENT
                                          -- byte-identical to baseline
mutant_copy_drift.py                run AFTER this commit per R121; see below
smoke (plain)                       PASS, raster pixels=2560, frames_admitted=1
smoke -LintOnly/-Mutant/-BadVertex/-NoEchoArm/-BadTraceArm   see below

NOTE ON gen_prod_top: 71 instances, against terrain7's recorded 70 -- consistent
with the v2 swaps having landed since.

=== 7. WHAT A REFUSAL COSTS, AND WHY IT IS THE RIGHT ANSWER ===
Seven terrain lanes have now closed none of these four. That is not seven
failures; it is A SMALL SET OF ABSENT OWNERS SEEN SEVEN TIMES -- TERRAIN.PAGEIO
for bake's half, one unspent owner look (R65), one unwritten spec sentence
(R115), and one known-wrong governor (R83). The four modules are finished,
tested and correct. What they lack is a page writer, a fragment stream, an
E-profile client and a governor -- none of which is a wiring act.

THE REGISTER CANNOT DISTINGUISH "nothing instantiates it" FROM "nothing can
honestly instantiate it yet", and those are different engineering states.
Across my four alone, 89 input ports exist and fewer than ten have a real
producer today. Composing all four would have read 21 -> 17 and buried roughly
fifty undeclared tie-offs -- available on any afternoon, and the campaign's
single largest act of self-deception.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `755ea2ea`

FINDINGS-terrcomp ADDENDUM: I overstated TERRAIN.PAGEIO's leverage, and its own contract says so

Correction to my own preceding commit (8c7a141d), found before anyone acted on
it. Transcribe this WITH that commit; it amends section 4 item 3 and section
2.3's closing sentence. No tree change; register still 21, superseded still 73
roots CLEAN, tie-off audit still 7/1/10/1.

=== WHAT I GOT WRONG ===

I wrote that TERRAIN.PAGEIO "closes sc_*, I27's deformation mark and I28's
writeback: THREE ENTRIES, ONE BLOCK. The highest-leverage unbuilt thing in
terrain."

design/contracts/TERRAIN.PAGEIO.md has a section 8 headed "What this does NOT
close", and its first sentence is the warning I walked into:

    "Honest accounting, because the attraction of this block is that it looks
     like it closes three entries and it does not."

Measured against that section, each of my three claims:

 * I28 IS ALREADY CLOSED -- rulings R14 and R4, 2026-09-19. It is not waiting
   on this block at all. What is true, and is a different statement, is that
   its path HAS NEVER SEEN A BEAT because nothing in the console can dirty a
   page. I turned "its path is untested" into "it is an open entry", which is
   the same class of error as the I27 `terr_dm_*` false presence: an INTENTION
   read as a port match.
 * I27 HALF-CLOSES. PAGEIO gives `terr_dm_*` a writer -- and on present
   evidence is the ONLY honest one, since it is the one block holding the slot
   and generation the patch was served under. The `terr_chk_*` half is
   unchanged and still waits on lodfeed-with-the-devstore, which ruling R87
   prices at 185 M10K of 553 (33%) -- NOT R59's stated ~77 (14%).
 * I32 DOES NOT CLOSE ON THIS BLOCK ALONE. PAGEIO removes the PAGE obstacle.
   The RECORD obstacle -- `stamp_results` -> `cmd_*`, which is what I32 is
   actually about -- is separate and is ruling R15's depth-table work.
 * AND THE REGISTER DOES NOT MOVE until bake itself composes, which needs all
   of the above plus a fit.

So the honest summary is: **PAGEIO unblocks work; it closes nothing by
itself.** That is still worth doing and it is still the right next block. It is
not the lever I described, and the difference matters to whoever schedules it.

=== AND ITS R65-INDEPENDENCE IS REAL BUT NARROWER THAN IT SOUNDS ===

Measured: `grep -ciE "R65|seam_dig|sheet_texel|stamp_depth"
design/contracts/TERRAIN.PAGEIO.md` returns 0. So PAGEIO genuinely does NOT
depend on the owner look R65 is waiting for -- that gates bake's `cmd_*` stamp
record, a different seam. Building PAGEIO can start without the owner's eye on
seam_dig_contact.png.

BUT IT IS NOT "BUILDABLE TODAY" EITHER, because it surfaces FOUR OWNER
DECISIONS OF ITS OWN (its section 7), and two of them are sizing decisions:

 1. IS LAYER B PERSISTED THROUGH THE HPS JOURNAL, or is a direct pool write the
    whole of it? Layer F's writeback does not simply write the pool -- it
    streams to an HPS journal under the R14 doorbell contract, with tickets and
    acknowledgements. Section 2 calls layer B "persistent deformation" in the
    SAME WORDS it uses for the surface sheet. The contract's own
    recommendation is the pool-write reading, because `terr_dm_bd_i` (a
    per-layer dirty bit for B and D) already exists on the directory and would
    be meaningless otherwise -- but it states plainly that this
    "changes the block's size by MORE THAN A FACTOR OF TWO."
 2. DOES A BAKE BUMP THE PAGE'S GENERATION? If yes, every handle held across a
    bake is stale and `terr_chk_stale_o` starts firing on live handles; if no,
    two different page contents share one generation and the generation stops
    identifying content. Recommendation: it does not bump. Unwritten either
    way, and the readings are not cosmetically different.
 3. WHAT SETS `dm_f_o` AND `dm_mips_o`? Depends on pagestream's still-open
    ruling about which surface MIPGEN sees: mips from layer A alone are
    untouched by a bake; mips from `compose_top` are invalidated by every bake.
    The contract notes these are the same question and should be answered once.
 4. Flagged not owned: the LAYER-E READER, offset 7,622, zero hits under
    fpga/, blocking entry I21 under ruling R13.

=== WHY I AM RECORDING THIS RATHER THAN QUIETLY FIXING IT ===

The error is this repository's documented signature and I produced a textbook
instance. My "three entries, one block" came from terrain7's summary and the
I27/I32 prose, both of which predate the PAGEIO contract; the contract was
written specifically to correct it and I cited the contract WITHOUT READING ITS
SECTION 8. That is R164's shape exactly -- amplifying an earlier finding
without re-measuring its scope -- and CLAUDE.md's: "the confident one-line
summary is where the error lives, and the check is almost always one grep of a
file already open."

The check that caught it cost one `sed -n '310,367p'` of a file I had already
opened twice.

REVISED RECOMMENDATION, replacing section 4 item 3: TERRAIN.PAGEIO is still the
right next terrain block and is the only proposed writer of `terr_dm_*`. Brief
it as "unblocks the bake path and half-closes I27", never as "closes three
entries", and put its four owner decisions -- especially decision 1, the
factor-of-two one -- to the owner BEFORE the packet starts, not inside it.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `dadb598a`

FINDINGS-terrcomp ADDENDUM 2: R83's governor defect is REPAIRED -- I repeated an expired blocker, which is the thing I criticised the brief for

Second correction to my own findings (8c7a141d, amended by 755ea2ea).
Transcribe with them. No tree change; register still 21, superseded still 73
roots CLEAN, tie-off audit still 7 declared / 1 reasoned / 10 group / 1 SILENT,
core entry list still I13 I14 I17 I20 I21 I27 I29 I32 I34.

=== WHAT I GOT WRONG ===

In refusal 2.4 I wrote, as blocker 3 against composing zhao_terrain_lod:

    "R83 rules it currently WRONG: the derived proj reaches 443.41 while the
     governor's port is [15:0] Q8.8 capping at 255.996 ... Wiring it first
     composes a circuit already known wrong."

**That is expired.** I inherited it from terrain7's D2 and repeated it without
opening the file -- in a packet whose section 1 criticises the brief for
exactly this, and which quotes R165 at the top. Fifth expired blocker found in
this run, and the first one I authored myself.

MEASURED, in zhao_measure_governor.sv's own port comment:

    parameter int unsigned PROJW = 20
    "THE CAMERA PROJECTION SCALE, Q12.8 unsigned (PROJW = 20)."
    "THIS PORT WAS TOO NARROW TO CARRY IT UNTIL 2026-09-20. The owner ruled it
     (R83, amending R73; R98 added that the widening lands in TWO ports) and
     BOTH MOVED IN ONE COMMIT."
    "DONE: PROJW = 20 (Q12.8, ceiling 4095.996), which covers 512-wide down to
     about 7 degrees. Law G1's numerator grew from under 2^33 to under 2^37,
     so STEPS went 33 -> 37 -- and it is now DERIVED from PROJW rather than
     written down twice."

The saturation table terrain7 quoted is still in the file -- kept deliberately,
"because a format validated by one example near its ceiling is not validated"
-- and that is exactly why it still reads as a live defect to anyone who greps
for the numbers rather than reading the paragraph under them. **The table is
the ARGUMENT FOR the repair, not evidence of an outstanding one.** A fixed
defect that keeps its evidence is easy to re-report as open; that is worth
knowing as a general shape.

AND A PRODUCER NOW EXISTS, same date: `zhao_view_projq88` derives the quantity
under R73 as `rhu(kx_raw * viewport_w / 512)` from `zhao_view_projscale`'s
snoop of the projector cfg bus. No ABI field was added, because
`zref::creature::projected_bound_radius_q8` already defines it.

=== THE CORRECTED BLOCKER, WHICH IS WEAKER AND MORE ACTIONABLE ===

Measured -- all three files exist, none is instantiated in zhao_console_core.sv:

    zhao_view_projscale    fpga/rtl/common/zhao_view_projscale.sv       composed 0
    zhao_view_projq88      fpga/rtl/common/zhao_view_projq88.sv         composed 0
    zhao_measure_governor  fpga/rtl/measure/zhao_measure_governor.sv    composed 0

So blocker 3 against TERRAIN.LOD is NOT "the governor is known wrong". It is
"three built, tested, uncomposed modules stand between TERRAIN.LOD and its
camera scale, and the chain has no known defect." That is a COMPOSITION, not a
repair -- and it is now the cleanest remaining lever in this area, because it
serves TERRAIN.LOD's cam*_scale_i AND entry I21's, which were the same blocker
seen twice.

terrain7's D2 recommendation -- "the Q12.8 widening lands with whoever owns
MEASURE.GOVERNOR BEFORE anyone composes TERRAIN.LOD's scale input" -- IS
DISCHARGED. D2 can be struck alongside D3.

=== WHAT DOES NOT CHANGE ===

TERRAIN.LOD is still REFUSED, and the other five blockers are untouched and
were each measured by me:

 1. sp_cx_i / sp_cz_i have no producer (devstore carries r_cy_o only).
 2. sp_src_id_i has no producer -- zhao_terrain_devstore.sv contains "src_id"
    ZERO times.
 4. edge_nz/pz/nx/px_i have no producer; MEASURE.GOVERNOR.md refuses ownership
    in writing.
 5. zhao_terrain_devstore is uncomposed, has NO design/blocks.yml row at all,
    costs 185 M10K of 553 (33%), and owes I27's terr_chk_* in the same commit.
 6. The output has no consumer: TERRAIN.PROJECT is uncomposed, and I21 refuses
    the group_seq alternative in writing because TERRAIN.LOD emits none of
    job_view_mask / job_mat_a / job_mat_b / job_weight.

Composing it would still create roughly twenty tie-offs and dangle a
thirteen-signal output. The refusal stands; one of its six legs was rotten and
is now removed, which makes the refusal more trustworthy rather than less.

=== THE HABIT THIS COST ===

Both of my corrections this packet (755ea2ea on TERRAIN.PAGEIO, this one on
R83) have the identical cause: I CITED A FILE I HAD NOT OPENED AT THE POINT I
CITED IT. In both cases the correcting text was inside the very file I named,
and in both cases one `sed -n` of a file already in hand was the whole check.

R165 says a refusal is a claim about a moment. So is a blocker quoted from
another lane's report -- and a report is a SNAPSHOT of a tree that has moved.
The rule I would write for the next packet: **re-open the file behind every
inherited blocker before repeating it, even when the lane that wrote it was
careful, and ESPECIALLY when its evidence is a table of numbers rather than a
sentence.** terrain7 was careful and was right on the day.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `9cc835a2`

FINDINGS-terrcomp ADDENDUM 3: TERRAIN.SHADE IS composed -- I said it was not, and the error made my own recommendation look better

Third and final correction to 8c7a141d (see also 755ea2ea, dadb598a).
Transcribe with them. No tree change; register 21, superseded 73 roots CLEAN,
tie-off audit 7/1/10/1, entry list I13 I14 I17 I20 I21 I27 I29 I32 I34.

=== WHAT I GOT WRONG ===

In section 3, "IF THE OWNER RATIFIES", step 3, I wrote:

    "Then compose normalmap between TERRAIN.SHADE and TERRAIN.PROJECT -- both
     of which are THEMSELVES UNCOMPOSED (zhao_terrain_shade lives inside
     u_terrain_lightlane; zhao_terrain_project is absent entirely)."

The parenthetical is right and the sentence it qualifies is wrong. **Living
inside `u_terrain_lightlane` IS being composed** -- the console's closure is a
hierarchy, not a list of top-level instances. Measured:

    zhao_terrain_lightlane.sv:292   zhao_terrain_normals u_normals (
    zhao_terrain_lightlane.sv:326   zhao_terrain_shade #(
    zhao_console_core.sv            zhao_terrain_lightlane #( ... )   x1

and neither TERRAIN.SHADE nor TERRAIN.NORMALS appears anywhere in
`completion_register.py`'s gap output, because both ARE connected. I wrote
"uncomposed" of a capability the register counts as closed.

NOTE THE DIRECTION: this error made the RATIFY branch look WORSE than it is --
two absent consumers instead of one -- which is the branch I was arguing
against. The flattering-direction law applies to arguments as well as to
instruments, and this is a small instance of it pointing at my own
recommendation.

=== THE CORRECTED PICTURE, WHICH IS STILL NOT A GREEN LIGHT ===

Of normalmap's two declared neighbours:

 * TERRAIN.SHADE -- COMPOSED, inside u_terrain_lightlane, with the sun arriving
   from SetEnvironment via zhao_light_env under R25. Its producer chain is real
   end to end.
 * TERRAIN.PROJECT -- genuinely absent, as I said.

But the shade's output does not stay in the machine. `u_terrain_lightlane`
drives `.light_base_o (terr_light_base_o)`, and `terr_light_base_o` is an
OUTPUT IN THE CORE'S OWN MODULE PORT LIST -- a boundary port, entry I13's
absent merge. So the lit value leaves the console as one signed 32-bit scalar
shade and nothing inside consumes it.

**So there is still nowhere for normalmap to sit.** Its detail term "rides on
TERRAIN.SHADE's base light", and that base light is already on its way out of
the core by the time it exists. Inserting normalmap means either going INSIDE
`zhao_terrain_lightlane` -- a change to a composed block with a contract and a
test, not a composition -- or waiting for I13's merge and TERRAIN.PROJECT.

=== WHAT THIS CHANGES IN THE RECOMMENDATION ===

Nothing about the refusal, and one thing about the cost.

RATIFY is cheaper than I priced it -- one absent neighbour, not two -- but the
fragment tap (blocker (b): no u, no v, no lod, no detail, no ready on
`zhao_geom_bin_pipe_v2`'s nine `stage_fragment_*` outputs, measured by me) is
untouched and remains the real cost, together with I13.

MY RECOMMENDATION IS UNCHANGED: **supersede, or defer with a ruling citation.**
It rests on the owner's own sentence about priorities, on `cut_order: 1`, and
on the bit-exact `strength = 0` no-op seam -- none of which this correction
touches.

=== THREE CORRECTIONS, ONE CAUSE ===

This packet produced three self-corrections (TERRAIN.PAGEIO's leverage, R83's
repaired governor, and this one). All three have the same cause and it is worth
stating once, because it is the most transferable thing here:

**I asserted a fact about a file at a moment when I had not opened that file,
on the strength of an earlier lane's summary or my own earlier note.** Each
check that overturned it was one `grep` or one `sed -n` of a file already named
in the sentence I was writing.

Two of the three errors ran in the flattering direction for whatever I was
arguing at the time; the third ran against it. That is the honest distribution
and it is why the rule cannot be "watch out for flattering claims" -- it has to
be **open the file**.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `a792fbfb`

FINDINGS-terrcomp ADDENDUM 4 (NEW FINDING, not a correction): the ratified terrain law package is imported ONLY by the module that is NOT composed

Fourth and last entry for 8c7a141d (with 755ea2ea, dadb598a, 9cc835a2).
Transcribe with them. No tree change; register 21, superseded 73 roots CLEAN,
tie-off audit 7/1/10/1.

Found by following the brief's own instruction -- "read
`zhao_terrain_patch_law_pkg.sv` before writing any terrain arithmetic,
re-implementing what it holds is the duplication this campaign exists to stop."
I wrote no terrain arithmetic, so I went looking for whether anyone else still
does. They do, and it is in the silicon rather than in a probe.

=== THE MEASUREMENT ===

    file                     composed in zhao_console_core   imports law pkg
    zhao_terrain_patch                    1                        0
    zhao_terrain_lodfeed                  1                        0
    zhao_terrain_patch_acc                0                        1
    zhao_terrain_field_walk               0                        0

**THE ONLY IMPORTER OF THE RATIFIED LAW PACKAGE IS THE ONE MODULE THAT IS NOT
COMPOSED.** Both composed modules carry their own copies of the same
arithmetic. Tree-wide, `zhao_terrain_patch_law_pkg` is named by exactly three
files: itself, `zhao_terrain_patch_acc.sv`, and `tests/CMakeLists.txt`.

The live copies, each byte-identical in effect to a package function:

  * `zhao_terrain_patch.sv:192` -- its own FULL `fx_add_sat` function BODY,
    not a delegation. Against `zhao_tp_fx_add_sat`.
  * `zhao_terrain_patch.sv:274` -- `cur_covers`, the inline section 9.1
    closed-interval test. Against `zhao_tp_covers`.
  * `zhao_terrain_patch.sv:299,300,301` -- `base_fx`/`scar_fx`/`bot_fx`, each
    `{{8{x[15]}}, x, 8'b0}`. Against `zhao_tp_h16_to_fx`.
  * `zhao_terrain_lodfeed.sv:210` -- `lat_fx_c`, the same shift, with the
    comment "qformats 9: raw << 8, EXACT". Against `zhao_tp_h16_to_fx`.
  * `zhao_terrain_field_walk.sv:207` -- `mask_c[l]`, the covers test the
    package header names explicitly as a duplicate. Uncomposed, so it costs no
    silicon, but it is still a second implementation of ratified law.

And the CORRECT pattern is in the tree to copy:
`zhao_terrain_patch_acc.sv:192-195` keeps a short local `fx_add_sat` whose
whole body is `fx_add_sat = zhao_tp_fx_add_sat(a, b);` -- a rename, not a
reimplementation. That is what the other three should look like.

=== WHY THIS IS THE UNCASHED-CHEQUE SHAPE, EXACTLY ===

CLAUDE.md records the canonical instance: extract the shared projector, notice
it deduplicated the SOURCE and not the SILICON, write that down, build the
prerequisite -- and never instantiate the one core. Here:

  1. notice the terrain arithmetic is duplicated -- done, and E1 measured it
  2. factor it into a package -- done, correctly, with cited law per function
  3. point a consumer at it -- done, for `zhao_terrain_patch_acc`
  4. convert the COMPOSED consumers -- **never happened**

So the duplication is gone from the file E1 promoted and is untouched in the
two modules that actually ship. The package's own header says the duplication
"was already in the tree, in the files this packet promoted" -- and
`zhao_terrain_patch.sv` is named in it twice as the file `fx_add_sat` was
"lifted verbatim from" (its lines 192-201). **The source of the law still holds
a private copy of it.**

THIS IS NOT A CRITICISM OF E1. Converting `zhao_terrain_patch.sv` and
`zhao_terrain_lodfeed.sv` means editing two COMPOSED production modules and
re-gating the console -- plainly outside a packet that was promoting probes.
The failure is that nothing is watching for step 4.

=== AND THE INSTRUMENT CANNOT SEE IT, BY ITS OWN ADMISSION ===

`uncashed_cheques.py` check 3 compares the `reference_model` STRINGS declared
in `design/blocks.yml`. The package header states the boundary in its own
words: *"two byte-identical function bodies in one directory are invisible to
it."* I confirmed the consequence -- `uncashed_cheques.py` is RC=0 on this tree
and reports none of the five sites above.

So this is a real duplication, in composed silicon, that no gate in the
campaign's set can detect, written down accurately in a header nobody reads
back. Same shape as `.gitignore` and the raw frames; same shape as
`prod_fit_sources.txt` in my section 1.4.

=== RECOMMENDATION ===

Small, mechanical, and it belongs with whoever next re-gates the terrain
closure -- NOT as a packet of its own, and NOT in a composition packet, because
it touches two composed modules and every bench that instantiates them:

  1. Add `import zhao_terrain_patch_law_pkg::*;` to `zhao_terrain_patch.sv` and
     `zhao_terrain_lodfeed.sv`, add the package to both SOURCES lists, and
     replace the five sites with the package functions -- keeping a local
     one-line alias where the short name aids reading, as `patch_acc` does.
  2. The result must be BIT-IDENTICAL. `terrain_patch_directed` and
     `terrain_lodpath_directed` / `terrain_lodhist_directed` are the evidence,
     and a diff of the counters is the check -- these are pure renames and any
     behavioural change means the copies had ALREADY drifted, which is the more
     interesting outcome and the reason to do it before they do.
  3. Convert `zhao_terrain_field_walk.sv`'s `mask_c` when it composes.

A DETECTOR IS THE MORE VALUABLE HALF, and the package header already specifies
it: the existing check compares declared `reference_model` strings, so it
cannot see two identical function BODIES. A check that normalises whitespace
and compares function bodies ACROSS files within a subsystem would have caught
all five sites, and would catch the next one. That is a small tool and it
closes a class rather than an instance.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `604c0ce3`

FINDINGS-terrcomp ADDENDUM 5: R172's blind spot exists in a SECOND tool, its canary cannot see it, and that is why addendum 4's duplication is undetected

Final entry for 8c7a141d (with 755ea2ea, dadb598a, 9cc835a2, a792fbfb).
Transcribe with them. No tree change. All six smoke forms green (table at the
end). Register 21, superseded 73 roots CLEAN, tie-off audit 7/1/10/1.

This also CORRECTS addendum 4's recommendation, which asked for a tool that
already exists.

=== 1. THE COORDINATOR'S R172, VERIFIED IN MY TREE, WITH A FIRED CONTROL ===

R172: `packet_h_tieoff_audit.py`'s `LITERAL` matched `16'd0` and not `18'sd0`,
so signed literals were invisible; tree-wide 128 silent, not 97.

MY PACKET IS UNAFFECTED, and the reason is structural rather than lucky: I
added NO literal to any port map, because I changed no file at all. All five of
my commits are empty; `git diff --stat 75b96d26..HEAD` is empty. There is no
tie-off of mine for the tool to miss, signed or unsigned.

BUT MY QUOTED BASELINE DESERVED A REAL NUMBER RATHER THAN A CAVEAT, so I
measured it both ways. I patched a SCRATCHPAD COPY of the tool with the
coordinator's corrected regex -- deliberately not the tree, which stays clean
and which the coordinator has already fixed on their branch:

    broken regex, zhao_console_core.sv : 7 declared, 1 reasoned, 10 group, 1 SILENT
    R172 regex,   zhao_console_core.sv : 7 declared, 1 reasoned, 10 group, 1 SILENT

IDENTICAL -- and an identical result is exactly the shape that means either
"no signed literals here" or "my patch never took effect". So I FIRED A
POSITIVE CONTROL before quoting the null, on a two-port probe carrying one
signed and one unsigned literal:

    broken regex : 1 SILENT   (catches 16'd0, misses 18'sd0)
    R172 regex   : 2 SILENT   (catches both)

The instrument can tell the difference, and it found none. **So at 75b96d26
there are no signed literals in `zhao_console_core.sv`'s port maps, and the
core's 1 SILENT is the same row under either regex** -- `u_material_resolve
.dir_valid_i`, pre-existing and not mine. (The coordinator reports the core at
0 SILENT on their branch; that is a fix landing after my base, not a
disagreement.)

=== 2. THE SAME DEFECT IN `duplicate_functions.py`, AND ITS CANARY IS BLIND TO IT ===

Looking for whether addendum 4's duplication ought to have been caught, I
applied CLAUDE.md's corollary -- grep for the thing before commissioning it --
and **found that the detector I recommended building ALREADY EXISTS**:
`tools/budget/duplicate_functions.py`, whose header describes precisely
addendum 4's class ("the same arithmetic, written out in N separate modules ...
`uncashed_cheques.py` check 3 cannot see this one").

It runs, RC=0, and reports many real duplicate groups. It reports **ZERO**
mentions of `fx_add_sat`, although `zhao_terrain_patch.sv:192` and
`zhao_terrain_patch_acc.sv:192` both define a function of exactly that name.

THE CAUSE, read from its own source rather than inferred, then tested:

    FUNC = re.compile(
        r"^\s*function\s+(?:automatic\s+)?"
        r"(?:[A-Za-z_]\w*\s*(?:\[[^\]]*\]\s*)?)?"   # optional return type + range
        r"([A-Za-z_]\w*)\s*(?:\(|;)",               # the NAME
        re.MULTILINE)

The return-type group allows **ONE identifier** plus an optional range. A
return type of `logic signed [31:0]` is TWO identifiers before the range, so
the pattern cannot match it at all. Tested directly:

    MISSED   function automatic logic signed [31:0] fx_add_sat(
    MISSED   function automatic logic signed [8:0]  add_sat9(
    CAUGHT   function automatic logic [15:0] decode16(
    CAUGHT   function automatic logic unit_mul(

MEASURED TREE-WIDE over every `.sv` under `fpga/rtl`:

    function declaration lines : 524
    missed by the tool         : 144  (27%)
      of which SIGNED return   : 126
      other (multiline/typedef):  18

**A quarter of the design's functions are invisible to the duplicate detector,
and they are the signed ones** -- which is to say the arithmetic. Heights,
velocities, normals, projections, saturating adds: exactly the population where
a second implementation of ratified law does damage, and exactly the population
the tool cannot see.

AND ITS SELF-CHECK CANNOT DETECT THIS. The file carries a deliberate canary:

    # A name that MUST be found, or the pattern has rotted and every
    # "no duplicates" below would be a false negative.
    CANARY = "unit_mul"

`unit_mul` returns plain `logic`. **The canary is an UNSIGNED example, so it
passes at full health while the pattern is blind to 126 declarations.** The
author did the right thing -- asserted the pattern still matches a known-good
example -- and picked an example from the half that works. That is CLAUDE.md's
"a detector that has not been shown to FIRE has not been tested" with the
subtler ending: a control that only covers the happy path is a control that
certifies the blind spot.

THIS IS THE SAME DEFECT AS R172, IN AN INDEPENDENT TOOL, FOUND THE SAME DAY.
Two regexes, two authors, two purposes, one missing `signed`. That is no longer
a bug; it is a PATTERN worth sweeping for. **RECOMMENDED: grep every regex in
`tools/` that parses SystemVerilog for whether it admits `signed`.** I did not
do the sweep -- it is outside my lane and would touch tools other packets are
gating on -- but two independent hits make it likely to pay.

=== 3. CORRECTION TO ADDENDUM 4's RECOMMENDATION ===

Addendum 4 ended: "A DETECTOR IS THE MORE VALUABLE HALF ... a check that
normalises whitespace and compares function bodies ACROSS files within a
subsystem would have caught all five sites."

**Wrong in the expensive direction: do not build it.** The detector exists and
is well designed. The repair is ONE REGEX -- allow a multi-word return type,
which fixes `signed` and the 18 multiline/typedef cases with it -- plus a
CANARY THAT INCLUDES A SIGNED EXAMPLE, so this cannot recur silently. After
that it still will not catch `zhao_terrain_patch.sv`'s copy against the
PACKAGE, because the names differ (`fx_add_sat` vs `zhao_tp_fx_add_sat`) and
the tool keys on the name by design -- but it WILL start seeing the 126
signed functions it has never seen, which is where the next one will be.

I nearly commissioned a duplicate of an existing tool inside a packet whose own
finding is about duplication. Recorded because that is funnier than it is
excusable, and because the check that prevented it is the one CLAUDE.md names:
grep the tree for the thing it replaces.

=== 4. ALL SIX SMOKE FORMS, AT a792fbfb ===

    plain          PASS -- raster pixels=2560, frames_admitted=1, draws=1 jobs=3
    -LintOnly      tb_zhao_console_core_smoke elaborates
    -Mutant        PASS -- terr_pl_slot_overflow_o fired 1 time(s), INVERTED
                   polarity; "the detector works; production's zero is a
                   measurement"
    -BadVertex     PASS -- one refused record dropped its batch (holes=1,
                   groups_poisoned=2, replay_poisoned=8) and the frame completed
    -NoEchoArm     PASS -- negative control, echo disarmed: complete=0 written=0
    -BadTraceArm   PASS -- the reserved stage_mask bit was refused whole and
                   nothing was armed

Static set all RC=0; `mutant_copy_drift` run AFTER the commit per R121: "OK --
every committed mutant copy is at least as new as the module it copies".

MINOR TRANSCRIPTION FIX to the main commit: I wrote that bake's `cmd_*` record
is "sixteen ports". Counted: **15 ports, 14 of them inputs**. The argument is
unchanged; the number is not.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

