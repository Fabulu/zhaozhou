# FINDINGS -- terrain7 lane (gz/terrain7)

*The harness blocks subagents from writing report files, so this packet put its
report in a COMMIT MESSAGE. Transcribed here verbatim by the coordinator, because
files under `fpga/rtl/` cite this path and a citation to a file that does not
exist is an uncashed cheque one level down. Source commit: `78c644ca`.*

---

FINDINGS-terrain7 (the harness blocks report files; transcribe from here)

The production fit's root wires TWO superseded modules and no instrument
was asking it. Register 22 -> 22.

Branch gz/terrain7, from e0484b78. NO GAP CLOSED, and that is the honest
headline: all four of my disconnected blocks and all three of my boundaries
are blocked on absences I verified rather than inherited. What this packet
produced instead is SIX CORRECTED BLOCKERS -- two of which reversed the
diagnosis outright -- one instrument defect with a committed detector, one
new blind spot closed, and four owner decisions.

=== 0. THE VEHICLE FOR THIS FINDINGS FILE ===
`runs/CLAUDE-RUNS/.../FINDINGS-terrain7.md` was REFUSED by the harness, exactly
as terrain6's was. It is here instead, as the brief instructs.

=== 1. NEW BLIND SPOT CLOSED: the production fit's root was never asked ===
`completion_register.py:superseded_in_closure()` is scoped to
`console_closure()` -- the modules under `zhao_console_core`'s fit target.
`zhao_prod_top` is a DIFFERENT ROOT, it is what the production fit actually
builds, and NOTHING WAS ASKING IT the owner's question. The blind spot was
exactly the size of the pin-out top. `superseded_in_prod_fit()` now asks it,
and it found TWO -- each verified by hand before reporting:

  wired  zhao_shell_top      superseded by  zhao_shell_top_v2
         zhao_prod_top.sv:3671 instantiates it
         console_inventory.yml:330-333  disposition: superseded,
           why: "...owner ruling 2026-09-19 'only the latest version'"
         prod_fit_sources.txt:38 carries zhao_shell_top.sv -- and
           zhao_shell_top_v2.sv IS NOT IN THAT LIST AT ALL

  wired  zhao_terrain_bake   superseded by  zhao_terrain_bake_v2
         zhao_prod_top.sv:4076 instantiates it
         console_inventory.yml:360-363, same ruling cited verbatim
         prod_fit_sources.txt:122 carries zhao_terrain_bake.sv

STATED PRECISELY, because the severity depends on what zhao_prod_top IS.
It is the GENERATED FLAT CENSUS top -- one instance of every production block
side by side for pin-out and area, not a shipping hierarchy. So this is not
"the console ships the old shell". It is "**the production AREA and DSP
numbers are measured on the superseded blocks**", which is precisely what the
owner's ruling is about: "fitting an old version measures a machine nobody
ships ... spends ALM and DSP on dead weight".
AND `zhao_console_core.sv` IS NOT IN prod_fit_sources.txt AT ALL, so the
core's own instantiation of `zhao_shell_top_v2` (zhao_console_core.sv:9980)
does NOT bring v2 into this fit. The two roots disagree about which shell
exists and nothing compared them.
THE DIRECTION IS THE UNAUDITED ONE: `zhao_terrain_bake_v2` exists to spend
ONE multiplier where v1 spends SEVEN, so measuring v1 OVERSTATES the block's
DSP. A big number looks like honest bad news, and nobody audits bad news
either -- CLAUDE.md says so in as many words.

THE LEDGER RECORDED THE RULING, THE FIT LIST IGNORED IT, AND EVERY GATE WAS
GREEN: check_prod_manifest OK, gen_prod_top --check "fresh (70 instances)",
and check_console_inventory OK -- the last of which prints, in these words,
"**the latest version is the one wired**". That sentence is TRUE of the
console core's closure, which is what it measures, and it reads as a claim
about the machine. It is the flattering-direction failure in one line: a
correct answer to a narrower question than the reader thinks was asked. This
is the repository's standing shape -- the
knowledge was written down, correctly, and nothing read it back -- and
`zhao_prod_top` is GENERATED, so it wears a reassuring provenance line while
doing it. It is the .gitignore lesson and the uncashed cheque in one.

REPORTED, NOT FATAL, deliberately. Turning it fatal today puts a pre-existing
condition in every concurrent worker's path for a defect none of them caused,
which is "never close one gap by opening another" applied to instruments. The
docstring says TURN IT FATAL once the coordinator decides, exactly as
`superseded_in_closure()`'s own docstring says of itself.

=== 2. INSTRUMENT DEFECT: ruling R59's own price was wrong by 2.4x ===
Landed at defef317.

`zhao_terrain_devstore`'s record gained a 16-bit subpatch centre height
(RECW = 3*DEVW + 16 = 88) and THAT comment was updated. The two localparams
DERIVED from it were not:
    ROWW = HALF * RECW;   // 576   <- 704
    ACCW = ROWW - RECW;   // 504   <- 616
Cosmetic, except the numbers did not stay in the comments: the block's header
discharges R59 ("report both sizes") and computes them from 576, publishing
116 M10K where its own parameters say 141.

AND R59'S OWN PREMISE IS THE SAME ERROR ONE STEP FURTHER BACK. The ruling
prices the store at "~786 kbit = ~77 M10K of 553 (14%)". 786,432 bits is
1024 x 16 x 3 x **16** -- a 16-bit deviation where DEVW is 24 -- and it counts
NO HISTORY AT ALL.

    naive (both surfaces)    2 x 141 + 44 = 326 M10K  (59%)
    taken (top only, bit-id)     141 + 44 = 185 M10K  (33%)
    R59's stated premise                  = ~77 M10K  (14%)

The saving R59 asked for is real and is 141 M10K. THE RESIDUAL IS 2.4x WHAT
THE RULING WAS DECIDED AGAINST. 14% is affordable; 33% is an argument -- which
is why nobody audited it. Corrected in the block and in all three places
zhao_console_core.sv quotes it. Also corrected from the same stale 72-bit
record: the "obvious array" row (176 M10K not 144, 35 wasted not 28), the
17-bit packing refused (108 M10K, saving 33, so 23% not 29%), and the SDRAM
third form (176 B/patch, 176 KB, 44 KB/frame -- not 144 B, 147 KB, 36 KB).

THE DETECTOR, AND THE FACT THAT IT WAS BROKEN FIRST.
tools/design/check_localparam_comments.py, ctest `localparam_comments`.
Version one accepted any comment LEADING with a number and required a `;`
terminator. Measured against the tree: THIRTEEN findings, EVERY ONE A FALSE
POSITIVE (prose describing another quantity -- `// 64 B / (8 words * 2 B)`
beside a 4, `// 21x21 signed` beside a 42), AND IT MISSED THE ONE REAL CASE,
because DEVW arrives in the module parameter port list terminated by a COMMA,
so RECW and ROWW never resolved and the file read CLEAN. Thirteen false
positives, zero true ones, silence in the flattering direction. Both halves
fixed; the positive control is asserted at import and reproduces the
comma-terminated list, the unterminated last parameter and a prose comment
that must not fire.
    --self-test    FIRED: ROWW claims 576, is 704
    before repair  337 files, 2 disagreements (devstore ROWW, ACCW)
    after repair   337 files, 0 disagreements

=== 3. GAPS REFUSED, every blocker RE-SEARCHED (662c0a33) ===

I32 -- SURFACE.STAMP's stamp_results into TERRAIN.BAKE. REFUSED.
(a) THE A/B/C PAGE-SERVER BLOCKER EXPIRED -- the sixth this run. The entry
    said the page "is reachable from in here only through the guard socket
    entry I26 records as absent". I26 CLOSED 2026-09-19. `u_terrain_pagestream`
    emits v_base_o/v_scar_o/v_bottom_o/v_vi_o/v_vj_o -- bake's vtx_base_i/
    vtx_scar_i/vtx_bottom_i and its two index outputs PORT FOR PORT, same
    widths, same signedness, same vi=column/vj=row convention. `u_terrain_psmux`
    already shares that exact stream between two clients, so a third is a
    widening of a block with a contract and a test, not a composer's mux. Left
    on the read side: a cursor-match adapter (streamer PUSHES, bake PULLS) and
    vtx_nobake_i, which has no producer.
(b) AND THE REAL BLOCKER IS ON THE OTHER SIDE AND NOBODY HAD RECORDED IT:
    **sc_*, BAKE'S LAYER-B SCAR WRITEBACK, HAS NO CONSUMER ANYWHERE.**
    SEARCHED: compcache_front takes cell-state writes and composed heights and
    never a scar; pagestream is read-only; terrain_writeback writes the F
    SHEET, not layer B. NOTHING IN THE CLOSURE WRITES A PAGE'S HEIGHT LAYERS.
    That inverts the refusal: the DIG phase's INPUT is now served and its
    OUTPUT is the hole. A bake whose scar cannot be stored has not deformed
    anything -- it computed a deformation and dropped it. Which is also why
    I27's mark has no writer and why I28's writeback never saw a beat: ONE
    ABSENT OWNER, THREE ENTRIES, previously filed under the wrong one.
(c) The sheet arbitration is real but MIS-SCOPED: it reads as though a
    scheduler had to be INVENTED. zhao_terrain_psmux (2-client round-robin,
    contract + test, composed here) and zhao_mem_share_n (same machine, N a
    parameter) are proven templates; neither has the sheet port's type, so this
    is ONE SMALL BLOCK WITH A CONTRACT, not a subsystem. Not what blocks it.
(d) The two dig laws: CONFIRMED not expired. Case-insensitive sweep of ALL of
    fpga/rtl incl. synth/ and tests/mutants/ for stamp_depth|kStampDepthTable|
    sheet_texel_for_vertex|depth_table|strength_to_depth|texel_for_vertex:
    4 hits, EVERY ONE A COMMENT in zhao_console_core.sv, and TWO OF THEM ARE
    THE PREVIOUS BLOCKER'S OWN QUOTED SEARCH STRING.

I27 -- the directory's deformation mark and handle check. REFUSED, AND IT
ASSERTED A PRESENCE THAT IS NOT THERE (handover failure number fifteen, the
dangerous shape). "terr_dm_* ... Its writer is TERRAIN.BAKE" is an INTENTION,
not a port match. **zhao_terrain_bake_v2 HAS NO DEFORMATION-MARK PORT OF ANY
KIND.** Its whole output set is sc_*, cs_*, dig_done_o/bake_done_o,
breach_active_o, trace_patch_id_o and six counters -- no slot, no generation,
no epoch, no per-layer dirty bit, which is exactly what terr_dm_slot_i,
terr_dm_gen_i, terr_dm_epoch_i, terr_dm_bd_i, terr_dm_f_i and terr_dm_mips_i
are. Bake never learns the page identity of the patch it digs. So composing
bake would NOT close this half, and acting on the old sentence meant
connecting ports to a module that does not have them. The terr_chk_* half is
unchanged: its first honest caller is lodfeed-with-the-devstore, which costs
the 185 M10K above.

I21 -- TERRAIN.GROUP_SEQ's subpatch job port. REFUSED. Five blockers
re-searched; FOUR SURVIVED, the fifth was false twice over.
 1. THE LAYER-E READER -- CONFIRMED ABSENT. zref_terrain_page.hpp:318 puts
    layer E at page offset 7,622; THAT LITERAL APPEARS IN ZERO FILES UNDER
    fpga/. pagestream.sv:251 reads exactly '{A_OFF, B_OFF, C_OFF};
    compcache_front holds layer D and composed heights and no material plane.
    The console treats matA/matB/weight as values handed in at the core's edge
    and FORWARDED (zhao_terrain_project's own port comment) to the texture
    mosaic. R13's per-triangle join is a change to TESS and GROUP_SEQ.
 2. NEIGHBOUR EDGE LEVELS -- CONFIRMED ABSENT, deliberately. Ten hits
    tree-wide; the only RTL driver is the LFSR u59_src in the generated
    prod_top. MEASURE.GOVERNOR.md:243-248 REFUSES OWNERSHIP IN WRITING.
 3. The view-mask reconciliation -- unchanged; owner decision D4 below.
 4. terr_cc_serve_release_i -- unchanged, same absent owner.
 5. cam0_scale_i/cam1_scale_i -- FALSE ON EXISTENCE, TWICE. See D2.

zhao_terrain_velocity -- REFUSED, and the entry is ALREADY CORRECT; verified
independently and nothing to change. lane_velocity_i's blocker is not "no
producer" loosely: FIELD.SEQ.EARTH was RULED NEVER TO EXIST as a block
(design/contracts/FIELD.SEQ.EARTH.md:14) and the FIELD v3 fabric IS composed
here as u_field_host -- with clients S and F only. Velocity is out-lane 1 of
the same E-profile evaluation whose out-lane 0 the height uses, so it is
blocked by EXACTLY what I34's height lane is blocked by: no E-profile client
adapter and no uniform descriptor producer (CMD.EXEC's TerrainField arm).
Plus its own "two walkers over one page" scheduler.

zhao_terrain_normalmap -- REFUSED, blocker verified STILL TRUE. Its fragment
inputs are f_valid_i/f_u_i/f_v_i/f_detail_i/f_lod_i.
zhao_geom_bin_pipe_v2 exposes only stage_fragment_* -- NO u, NO v, NO lod, NO
detail, and NO `ready` companion to stage_fragment_valid_o -- which the core
itself calls "a structural probe ... not a handshaked stream". Nothing in the
repository instantiates the module outside its own directed test. And
case-insensitive sweep of ALL of spec/ for normalmap|normal map|normal-map|
bump: ZERO HITS (every "bump" is a version bump). It has a contract, a ledger
row, an oracle and a 4,738-check suite and NO RATIFIED SPEC SENTENCE.

=== 4. OWNER DECISIONS ===

D1 -- R65 IS STILL OWED THE OWNER'S EYE, AND IT BLOCKS I32's OPTION A.
THE HARD STOP, AND I DID NOT CROSS IT. The coordinator's Option A is recorded
in the entry so it is not re-opened: bake keeps its parametric disc and gains
a second per-vertex depth mode fed from layer F through the R15 laws.
IT IS NOT BUILDABLE TODAY AND THE REASON IS AN ART JUDGEMENT. Option A needs a
layer-F reader, and a layer-F reader IS an implementation of 9.3(b)'s
sheet_texel_for_vertex -- the nearest-texel fallback whose acceptability is
R65's open question. The render was made and CHANGED THE QUESTION. Ratified
9.3(c): the fallback "does not produce a visible CRACK ALONG THE SEAM. It
produces a rim that is wrong by up to one vertex, EVERYWHERE, and the seam is
one of the places it is wrong." Measured on the worst placement the tool can
construct: 4 of 99 shared border vertices disagree, worst tear 3.25 m -- the
dig's FULL DEPTH -- but it lands inside a staircase the 1 m lattice already
produces, and the DIFF panel shows the two laws differing all the way round
the crater, not at the seam.
THE QUESTION NOBODY HAS PUT TO THE OWNER: is a rim wrong by up to one vertex
everywhere acceptable? Only looking settles it. If no, the format moves to
65x65 and sheet_texel_for_vertex becomes the IDENTITY -- so the reader's
address generator is EXACTLY the contested thing. Building it now would commit
silicon to a format decision the owner has not made.
RECOMMENDATION: put reports/terrain-seam-dig/seam_dig_contact.png in front of
the owner BEFORE briefing any packet on Option A. 9.3(a)'s depth table is
format-INDEPENDENT and buildable today; alone it would be a block nothing
drives, so it is not worth a packet by itself.

D2 -- MEASURE.GOVERNOR's proj*_i SATURATES, SILENTLY, IN THE EXPENSIVE
DIRECTION. Not my block, but it is what stands between TERRAIN.LOD and its
camera scale. cam*_scale_i's recorded blocker is FALSE ON EXISTENCE TWICE:
zhao_measure_governor.sv:228-229 emits cam0_scale_o/cam1_scale_o PORT FOR
PORT, and zhao_view_projscale.sv EXISTS in fpga/rtl/common/ (which is why a
terrain-scoped grep misses it), built for R68 and tested. Neither is composed
in the core, and projscale is INSTANTIATED BY NO PRODUCTION ROOT AT ALL
(prod_manifest.yml:1019, "not-yet-adopted") -- BUILT-INSTALLED-NOWHERE, and a
deferral citing no ruling is an open question.
BUT RULING R83 AMENDS R73 AND THE ERROR IS SILENT: the derived proj reaches
443.41 at 60 deg over 512 px and the governor's proj0_i is [15:0] Q8.8,
CAPPING AT 255.996. It saturates below 90 deg hfov single-view and below 53.13
on Duo, and a saturated proj PEGS THE LOD LADDER AT ITS FINEST RUNG --
maximum triangle cost, no visible symptom, no counter that can see it. R83
rules 20-bit Q12.8.
RECOMMENDATION: the Q12.8 widening lands with whoever owns MEASURE.GOVERNOR
BEFORE anyone composes TERRAIN.LOD's scale input. Wiring it first composes a
block already known wrong wherever the camera is wide -- "a fit that measures
a circuit you already know is wrong" one layer up.

D3 -- zhao_prod_top WIRES TWO SUPERSEDED MODULES. Evidence in section 1. I did
NOT change it: it moves the production fit's closure and area, it touches
three generated/shared files other lanes gate on, and I am told not to run
Quartus. RECOMMENDATION: either cash the cheque (regenerate zhao_prod_top
against zhao_shell_top_v2 and zhao_terrain_bake_v2, add them to
prod_fit_sources.txt, and spend one fit on it) or record each as
not-yet-adopted WITH A RULING CITATION so it stops reading as an open
deferral. Then turn superseded_in_prod_fit() fatal.

D4 -- I21's VIEW-MASK RECONCILIATION (unchanged, restated). The compose door's
mask is EIGHT bits (T5's per-player tag); job_view_mask_i is TWO, documented
"bit v = project into view v". A PLAYER mask and a PROJECTOR VIEW mask. On a
two-view machine they very probably coincide, and "very probably coincide" is
how a hidden adapter gets written. Needs a sentence from the owner, not an
assign.

=== 5. FALSE CLAIMS FOUND (six; two are PRESENCE claims) ===
 FALSE  I32: page reachable only via I26's absent socket (absence)
 FALSE  I27: "terr_dm_*'s writer is TERRAIN.BAKE"            (PRESENCE)
 FALSE  I21: cam*_scale_i has no producer                    (absence)
 FALSE  zhao_view_projscale absent                           (absence)
 FALSE  GEOM.LOD entry: governor emits "no pixel-error threshold at all"
        -- cam*_thresh_q8_o at :258-259, assigned at :563-564 under R26
 FALSE  velocity: "the height lane's producer is the absent FIELD.SEQ.EARTH"
 TRUE   layer-E reader absent; edge_* producer absent; the R15 laws have no
        RTL; normalmap's fragment stream never leaves bin_pipe_v2
 NEW    sc_* has no consumer -- never claimed either way, and none exists

=== 6. WHAT THIS LANE NEEDS NEXT, in order ===
 1. THE OWNER LOOKS AT THE SEAM RENDER (D1). Everything in Option A waits on
    it, and the packer does not exist yet, so the format is cheaper to move
    now than it will ever be again.
 2. A PAGE WRITER. sc_* has no consumer, terr_dm_* has no writer, I28's
    writeback has never seen a beat -- three entries, ONE missing block. It
    must hold the residency slot and generation, because that also closes
    I27's first half.
 3. MEASURE.GOVERNOR's Q12.8 widening (D2), then compose governor + projscale,
    and cam*_scale_i stops being an I21 blocker.
 4. A layer-E reader -- TERRAIN.PAGESTREAM subsystem work under R13.
Items 2 and 4 are blocks with contracts. Item 1 is a five-minute look that
unblocks a packet. Item 3 is a defect repair cheaper than the bug.

=== 7. GATES AT THIS COMMIT ===
completion_register            22  (10 tie-offs + 11 disconnected + 1 unbuilt)
check_console_inventory        OK
check_prod_manifest            OK
gen_prod_top --check           fresh (70 instances)
gen_console_board --check      fresh (1215 core ports, 72 parameters)
mutant_copy_drift              OK, no new drift
check_quartus17_syntax         RC 0
gen_shell_paired_diff --check  fresh
check_case_labels              OK
check_localparam_comments      OK   (NEW; fired on the pre-repair tree)
lint_terrain_devstore -Wall    0 warnings, 0 errors
smoke -LintOnly                elaborates
smoke                          PASS, raster pixels=2560, frames_admitted=1
smoke -Mutant/-BadVertex/-NoEchoArm/-BadTraceArm   PASS
R60, ALL BUILT AND RUN at this commit:
    cmd_exec_directed              677 checks passed
    terrain_lodpath_directed       286 checks, 0 failures
    terrain_lodhist_directed       158 checks, 0 failures
    terrain_bake_v2_directed       267 checks passed
    stamp_to_bake_laws_directed    306 checks, 0 failures

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>

