# FINDINGS — POSTMEAS (post_gather / measure_governor / geom_parambuf)

**Branch `gz/postmeas`, head `e74b897e`, 7 commits. Register 21 → 21, all three REFUSED.**
**Comment-only: one file, zero non-comment lines. No tie-off created, none owed.**

> **TRANSCRIBED BY THE COORDINATOR** — the harness refuses a subagent writing a
> report `.md` (R183). **READ COMMIT 5 BEFORE 1 AND 4: IT RETRACTS THEIR
> HEADLINE.** The packet asked for exactly that ordering.

---

## `86ea7e5e`

FINDINGS-postmeas 1/4: refuse all three; POST.GATHER, and R37 CANNOT unblock it

The harness refuses a FINDINGS-*.md from a subagent, exactly as it did for
terrain6, terrain7 and terrcomp -- FOUR LANES IN A ROW NOW. Per those
precedents this report IS the commit message; transcribe commits 1..4 to
runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-postmeas.md.

Branch gz/postmeas from 982c354ba73bbab7b86d893e888c94d99c348525 on
claude/ceiling-architecture-20260912 (head confirmed by git ls-remote, not the
stale tracking ref). Own worktree C:\programmieren\zencrifice\gz-postmeas.

REGISTER 21 -> 21. ALL THREE COMPOSITIONS REFUSED. No RTL changed: the diff is
comment-only (verified -- zero non-comment lines in `git diff`), one file. No
tie-off was created, so none was owed in the INCOMPLETE block (R159), and the
tie-off audit is bit-identical before and after.

=== 0. BASELINE AND FINAL, MEASURED IN MY OWN TREE ===
completion_register      21 (9 tie-offs + 12 disconnected + 0 unbuilt)  BOTH
superseded check         73 production roots CLEAN                      BOTH
packet_h_tieoff_audit    8 declared, 1 reasoned, 10 by group, 0 SILENT  BOTH
core closure modules     225                                            BOTH
capabilities connected   85                                             BOTH

CORRECTION TO THE BRIEF: the tie-off audit reads 8/1/10/0 in my tree, not the
7/1/10/1 the Wave-4 brief quotes and not terrcomp's 7/1/10/1. The core moved
between those heads; u_material_resolve.dir_valid_i has been given a reason
since. 0 SILENT is the number to hold.

=== 1.1 zhao_post_gather -- REFUSED, AND R37 IS NOT THE GATE PEOPLE THINK ===

R37 is live and load-bearing; the brief asked me to check rather than inherit
it, and it is. But the received framing -- "one owner art ruling stands between
this block and a composition" -- IS WRONG IN THE FLATTERING DIRECTION, and this
is the packet's main result.

spec/stars_and_flares.md section 1 is marked "(frozen)" and defines EXACTLY ONE
channel of `tag = (channel << 6) | strength`: GLOW = 0b01.

Measured across all of spec/, WITH A POSITIVE CONTROL FIRED BEFORE THE NULL WAS
QUOTED, because a grep that finds no channel and a grep that finds nothing look
identical:
    "GLOW = 0b01" / "glow-tag"   6 hits   <- POSITIVE CONTROL: sweep sees subject
    0b00, 0b10, 0b11             0 hits
    refraction, heat haze        0 hits
    shockwave                    1 hit, creature_rules.md:152, a gameplay
                                 effect, not a tag channel

zhao_post_gather consumes THREE planes. Glow has a channel (and R37's
coefficient law over it). f_disp_x_i/f_disp_y_i and f_ink_i HAVE NO TAG CHANNEL
AT ALL. Allocating one edits a section the spec calls FROZEN -- an ABI/spec
decision, which is a DIFFERENT KIND of owner call from R37's coefficients
(PACKET-PROTOCOL rule 4). So a ruling on R37 closes the glow third and leaves
two planes with no source. ANYONE QUEUEING BEHIND R37 EXPECTING IT TO RELEASE
THIS BLOCK SHOULD BE TOLD THAT BEFORE THEY QUEUE.

THE INK NAME COLLISION is how this gets misread as already served.
spec/commands.zidl:777 DOES define an `ink` -- "rgb565, the exterior-ink
colour, stage 9" -- and it is ALREADY COMPOSED, as post_look_ink_rgb_w on
u_post_composite.ink_rgb_i. That is a PER-FRAME CONSTANT COLOUR from SetPost.
This block's f_ink_i is a PER-FRAGMENT MASK BIT. Same word, different quantity,
and the composed one is the one a grep finds first.

WHAT R37 IS WAITING ON IS THE OWNER'S EYE, AND THE ARTEFACT IS RENDERED AND
COMMITTED. Measured, because "the sheet was never made" and "the sheet is made
and unjudged" are different asks and only one of them is a packet's:
    reports/post-gather-law/gather_law_contact.png   PRESENT (38,683 bytes)
    tools/post/gather_law_render.cpp                 PRESENT
    tools/post/gather_law_sheet.py                   PRESENT
    zref::post::gather (kGlowKnee=24, kGlowSlope=0x1C, kGlowTint, kGlowMaster)
        PRESENT in reference/include/zref/zref_post.hpp
R37 OWES NO FURTHER PACKET WORK ON THE GLOW THIRD. IT OWES A LOOK.

R65 IS NOT LOAD-BEARING HERE -- the brief asked, and the answer is NO. Measured:
"R65" appears ZERO times in design/contracts/POST.GATHER.md and ZERO times in
fpga/rtl/compositor/zhao_post_gather.sv. R65 is seam_dig_contact.png and gates
TERRAIN.BAKE's sheet_texel_for_vertex. The two are both unjudged contact sheets
and nothing else. DO NOT INHERIT R65 AS A GATHER BLOCKER.

A FOURTH GAP NOBODY HAS NAMED: THE FLUSH STREAM CARRIES NO ADDRESS. c_index_o
is FOUR BITS, 0..15, "within the tile". u_post_composite reads by
{gd_view_o, gd_cx_o, gd_cy_o} -- absolute, per view. zhao_post_gather has NO
tile-x, NO tile-y and NO view port at all; its only tile signals are the
tile_start_i/tile_flush_i pulses. And the contract does not name the mechanism
either: its entire statement of this seam is one sentence, "Writes out for
POST.COMPOSITE". So the store must be told the tile ORIGIN, and the cheapest
correct source is the one I17 already identified for the other half -- the
shell's absolute fb_x_o/fb_y_o latched at tile_start_i, giving
cell = {tile_y/4 + c_index_o[3:2], tile_x/4 + c_index_o[1:0]}. That is a port on
the STORE, not on this block, and it is INDEPENDENT OF R37.

I NEARLY SHIPPED A FALSE ALARM HERE AND CHECKED FIRST. gd_cx_o is declared
[$clog2(LINE_W+1)-3:0], which I first read as a divide-by-8 against
POST.GATHER's divide-by-4 -- a resolution mismatch. IT IS NOT ONE: [N-3:0] is
N-2 bits, and composite drives x_f_q[XW-1:2], a divide by four, matching
TILE=16 / CELLS=4. THE RESOLUTIONS AGREE. Recorded because R166's lesson is that
a reading firing on everything is as broken as one firing on nothing, and the
check was one line.

THE PLANE STORE HAS A PRICE AND NOBODY HAS PUT IT IN FRONT OF THE OWNER -- the
same omission I17 records for the HUD store one bullet up, a TECHNOLOGY named
instead of a NUMBER produced. design/contracts/POST.GATHER.md's own "The count"
section costs it: 128 x 60 = 7,680 cells, 31,680 bytes, "at the natural 256 x 40
M10K shape that is THIRTY M10Ks" -- 30/553 = 5.4% of the device, against the HUD
store's 153/553 = 27.7%. The corrected Duo geometry (6,144 cells, two 64 x 48
views, reports/DUO-QUARTER-PLANE-GEOMETRY-20260918.md) makes it smaller still.
The owner has ruled in exactly this currency:
reports/OWNER-RULING-M10K-CEILINGS-20260918.md waves 19 M10K through as "3.4% of
a 553-M10K device". MEMORY IS NOT THE GATE HERE; THE UNDEFINED CONTENTS ARE. The
next lane should stop costing the store and start costing the channels.

THE LEDGER CORROBORATES THE ABSENT STORE FROM A SECOND, INDEPENDENT DIRECTION.
design/blocks.yml's POST.GATHER row declares ONE counter,
post_gather_vram_bytes_by_client. That string is in NO .sv file; the RTL exports
fragments_o, glow_saturations_o, disp_clamps_o and cells_flushed_o instead. A
VRAM-bytes-by-client counter cannot belong to a block whose own contract says it
"reads no external memory" -- IT DESCRIBES THE PLANE STORE, and the ledger has
been carrying the unbuilt block's counter on the built block's row. The same row
declares inputs: [dispatch] and upstream: [CMD.SCHEDULER, RASTER.RESOLVE] while
zhao_post_gather.sv contains the strings cmd/dispatch/sched ZERO times -- the
"ledger asserts an edge neither side realises at port level" shape already
recorded for PART.LADDER, found independently in two subsystems today.

ONE INHERITED CITATION THAT HELD, said because six lanes reported the opposite
today. I17's zhao_shell_top_v2.sv:1181 is still exactly
`.fb_tag_o(rp_fb_tag_unused), .fb_addr_o(rp_fb_addr_unused),` with the unused
wires at :1094-1095. The repair really is one 8-bit port.

VERDICT: four blockers of two kinds. (1) a FROZEN-spec channel allocation for
displacement and ink -- owner, and NOT R37; (2) R37's glow coefficients --
owner, sheet already rendered; (3) the RTL adapter, which (1) and (2) define;
(4) a plane store that does not exist, priced at ~30 M10K and needing a tile
origin nobody has specified. Composing today ties off two of three input planes
and dangles a six-signal output: closing one register entry by opening several.

WHAT CHANGED IN THIS COMMIT: comment text only, in
fpga/rtl/prod/zhao_console_core.sv -- entry I17's POST.GATHER bullets, entry
I18-siblings (MEASURE.GOVERNOR), and the GEOM.PARAMBUF entry. 180 insertions,
1 deletion, zero non-comment lines.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `1f98ed26`

FINDINGS-postmeas 2/4: MEASURE.GOVERNOR refused -- both ends, positive control, and NOT a circular deadlock

Empty commit; transport for the findings the harness will not let me write to a
file. Transcribe with commits 1, 3 and 4.

=== 1.2 zhao_measure_governor -- REFUSED ===

post3b refused this on 2026-09-20. I re-verified rather than inherited (R165).
THE OUTPUT BLOCKER HOLDS.

Enumerated in my own tree: 88 instantiation sites, 87 distinct modules in
zhao_console_core.sv -- and that 88 AGREES EXACTLY with the figure R166 quotes
for the tie-off audit's subject ("it could see 18 of 88 instantiations"), which
is a second instrument arriving at the same census.

    zhao_terrain_lod      NOT composed (only the generated pricing top)
    zhao_geom_lod         NOT composed; instantiated only by
                          zhao_geom_lodstate.sv:348, itself not composed
    zhao_geom_lodstate    NOT composed
    zhao_part_ladder      COMPOSED -- and an owner decision, not wiring

THE PORT SEARCH WAS GIVEN A POSITIVE CONTROL, which post3b's version did not
state. My first pattern (`input var logic ...`) returned ZERO matches and I
nearly reported that as the finding -- the core writes `input  logic`, no `var`,
so the pattern was blind to all 283 of its input ports. Corrected: the pattern
sees 283 input ports, and of those EXACTLY ONE matches
lod/gov/thresh/scale/deg/px_err/starv -- part_prj_gov_floor_i. One match out of
283 VISIBLE is a measurement; one out of an unknown number is a guess, and I had
the guess in hand before I checked.

And part_prj_gov_floor_i is not a way in. The I24 note measures it: the
governor's ratified output table routes EVERY policy output to TERRAIN.LOD and
gives PART.LADDER nothing; deg0_o/deg1_o are spelled "capture / post-mortem";
design/contracts/PART.LADDER.md never contains the words "deg", "floor" or
"degrade"; design/blocks.yml asserts only the abstract edge. THE LEDGER ASSERTS
AN EDGE NEITHER CONTRACT REALISES AT PORT LEVEL -- the same shape as
POST.GATHER's `inputs: [dispatch]` in commit 1, found independently in two
subsystems today.

IT IS NOT A CIRCULAR REFUSAL -- CHECKED, because it looks like one and a false
deadlock is exactly the thing a campaign breaks by mistake. TERRAIN.LOD does
cite cam0/1_scale_i, so "each refuses because of the other" is available and
would have been a flattering two-module close. But terrcomp measured FIVE
FURTHER ABSENCES on TERRAIN.LOD with nothing to do with the governor:
  * sp_cx_i / sp_cz_i have no producer
  * sp_src_id_i is dropped at the join -- zhao_terrain_devstore contains the
    string "src_id" ZERO times
  * the four edge_* neighbour levels have ownership REFUSED IN WRITING in
    MEASURE.GOVERNOR.md
  * zhao_terrain_devstore is uncomposed, has NO design/blocks.yml row at all,
    and costs 185 M10K of 553 (33%)
  * TERRAIN.PROJECT is absent entirely
COMPOSING THE GOVERNOR WOULD NOT RELEASE TERRAIN.LOD. Breaking the cycle at this
end buys nothing and the pair must not be sold as a two-module close.

INPUT SIDE, CONFIRMED UNCHANGED:
  starved0/1_i   reachable today -- zhao_measure_starve exists (70 checks) and
                 zhao_measure_tokens IS composed with den_valid_o/den_view_o/
                 den_reason_o already wired out
  proj0/1_i      real producer exists: zhao_view_projq88 (19 checks), but its
                 own `vw` comes from cfg address 17, I14's VIEWPORT RECT, which
                 only the external host port writes. A genuine hole.
  px_err0/1_i,
  view_count_i   TWO new CMD.EXEC arms. zhao_cmd_exec's
                 ZHAO_SET_VIEW_OFF_PIXEL_ERROR is the EXCLUSIVE upper bound of
                 the matrix copy, so the field is never staged into sv_* at all,
                 and SetPresentationContract.view_count is lowered nowhere in
                 fpga/rtl.

So the governor is blocked at BOTH ends and the two ends need different kinds of
work. Composing it today would open cam0/1_scale_o and cam0/1_thresh_q8_o as NEW
boundary outputs with nothing behind them -- closing one register entry by
creating tie-offs, which is the move rule 1 forbids and which R159 says the
register cannot see.

Recorded in the core at entry I18-siblings, in place, at the claim.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `ccccd834`

FINDINGS-postmeas 3/4: GEOM.PARAMBUF refused -- the blocker got STRONGER for the third time

Empty commit; transport for the findings the harness will not let me write to a
file. Transcribe with commits 1, 2 and 4.

=== 1.3 zhao_geom_parambuf -- REFUSED ===

The entry's claim is that NOTHING IN fpga/rtl WRITES A GEOMETRY RECORD INTO
MEMORY. Re-measured per R165, WITH A POSITIVE CONTROL, because a grep that finds
no writer and a grep that finds nothing are indistinguishable.

POSITIVE CONTROL: the same pattern returns the five writers the entry names --
  zhao_debug_frameblit.sv:391      guard_req_o.write = 1'b1
  zhao_mem_upload.sv:394           guard_req_o.write = 1'b1
  zhao_raster_fbwrite.sv:218       guard_req_o.write = 1'b1
  zhao_terrain_pageloader.sv:381   guard_req_o.write = 1'b1
  zhao_terrain_writeback.sv:583    hps_req_o.write   = 1'b1
The only other `write = 1'b1` in the tree is zhao_sdram_ctrl.sv:188's
`cmd_write`, the controller's own command encoding and not a guard request. THE
COUNT OF FIVE IS EXACT AND NONE IS GEOMETRY. All five citations held.

A SIXTH READ-ONLY GEOMETRY CLIENT THE ENTRY DOES NOT LIST:
  zhao_geom_loomfeed.sv:457        assign hps_req_o.write = 1'b0;
With meshfetch (:340), assetfetch (:360), drawjob (:308), ladderbank (:222) and
zhao_geom_mem_adapter's .FORCE_READ(1'b1) (:175), that is SIX CLIENTS, ALL
HARD-CODED READ-ONLY. The claim has now got stronger on two successive
re-checks, which is the direction that almost never happens.

spec/memory_rules.md 5f declares RENDER.ASSET_POOL read-only with a formal
assertion (a1_render_asset_ro), so a writer built today would target a region
the guard is PROVEN to refuse.

zhao_geom_arena.sv is still not this arena's allocator: PTR_W = 8 over
CHUNKS = 256 cannot name this block's 32-bit next_chunk over
ARENA_CHUNKS = 65536, and it belongs to GEOM.BINNER by its own first line.

THE ABSENT OWNER IS THE ARENA'S WRITER AND ITS ALLOCATOR, the same one entry I39
names for GEOM.ASSEMBLE's per-view vertex-id base. Pairing this block with
GEOM.ASSEMBLE (which is composed and emits a TriangleDescriptor's FIELDS) would
be an encode immediately undone by a decode with no memory between them -- a
disconnected implementation with extra steps, whatever the handshakes did.

LEDGER CORROBORATION RE-MEASURED, AND IT SURFACED AN INSTRUMENT DEFECT -- see
commit 4. All four declared counters (parambuf_records_written,
parambuf_chunks_allocated, parambuf_stale_handles, parambuf_overflow_frames) are
still absent from every .sv, EXCEPT the entry's own sentence saying so.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `a20b8677`

FINDINGS-postmeas 4/4: a claim that falsified its OWN grep, and the owner decision

Empty commit; transport for the findings the harness will not let me write to a
file. Transcribe with commits 1, 2 and 3.

=== 2. INSTRUMENT DEFECT: A CLAIM THAT FALSIFIED ITS OWN GREP ===

Found in the GEOM.PARAMBUF entry of zhao_console_core.sv. It reads:

    design/blocks.yml declares four counters ... and all four strings appear in
    that file and in NO .sv FILE IN THE REPOSITORY.

  grep -r parambuf_records_written --include=*.sv fpga/

now returns ONE HIT, and that hit IS THE SENTENCE MAKING THE CLAIM. Writing the
claim into a .sv file falsified the grep that checks it. Excluding the entry
itself all four are still zero and THE SUBSTANCE IS INTACT; I corrected the
wording in place (commit 1).

A SELF-FALSIFYING CLAIM IS WORSE THAN A STALE ONE, because the reader's own
instrument appears to refute the entry and the natural conclusion is that the
whole entry has rotted -- so a reader spot-checking one citation throws out a
refusal that is not only true but has got STRONGER twice.

This is terrcomp's prod_fit_sources.txt shape -- "the warning is at line 1 and
the evidence is at line 121, and nobody scrolls up" -- measured there at 4
failures out of 4.

AND IT HAPPENED A THIRD TIME IN THIS RUN, IN THE FINDINGS FILES THEMSELVES.
FINDINGS-terrcomp.md section 2.4 item 3 quotes as LIVE the R83 defect ("the
governor's port is [15:0] Q8.8 capping at 255.996, so it saturates below 90 deg
hfov and PEGS THE LOD LADDER AT ITS FINEST RUNG"). THAT IS FALSE IN MY TREE:
zhao_measure_governor.sv and zhao_view_projq88.sv both carry PROJW = 20 (Q12.8)
-- R98's two ports, and they agree; proj0_i/proj1_i are [PROJW-1:0]. Terrcomp
SELF-CORRECTED THIS IN ITS OWN ADDENDUM 2, but the correction sits ~270 lines
BELOW the claim, and a reader who greps for zhao_terrain_lod lands in 2.4 and
never sees it.

THREE INSTANCES, ONE MECHANISM: THE CORRECTION IS NOT WHERE THE GREP LANDS.
Terrcomp's recommendation for prod_fit_sources.txt was to put the warning IN THE
PATH. The general form: a correction must be placed where the access pattern
already goes, not appended where the writer happens to be. I did that for the
two core entries -- correction in place, at the claim. I did NOT do it for
FINDINGS-terrcomp.md: that is a landed record and not mine to rewrite.
COORDINATOR CALL -- a one-line pointer at 2.4 item 3 would spend it.

=== 3. WHAT I CHANGED ===
No RTL. Comment text only, in fpga/rtl/prod/zhao_console_core.sv (commit 1,
180 insertions / 1 deletion, zero non-comment lines):
  * entry I17's POST.GATHER bullets -- the frozen-channel measurement with its
    positive control, the ink name collision, R37's true status, R65 measured
    NOT load-bearing, the store's 30/553 price, the unnamed tile-address seam,
    the resolution check that came back clean, the ledger corroboration;
  * entry I18-siblings -- the third re-verification with the 283-port positive
    control and the 88/87 census, the explicit non-circularity with TERRAIN.LOD,
    and the spent R83 blocker;
  * the GEOM.PARAMBUF entry -- the sixth read-only client, the positive control
    on the writer sweep, and the self-falsifying-grep repair.
NO TIE-OFF WAS CREATED, so the `INCOMPLETE -- TIED OFF, AND WHY` block is
untouched and nothing was owed there (R159). Tie-off audit identical before and
after: 8 declared, 1 reasoned, 10 by group comment, 0 SILENT.

=== 4. CORRECTIONS TO THE BRIEF ===
1. "POST.GATHER has historically been associated with R37 and R65" -- R37 yes;
   R65 NO, zero citations in either the contract or the RTL.
2. "The core is at 0 SILENT" -- true; and the Wave-4 brief's 7/1/10/1 is spent.
   Mine reads 8 declared / 1 reasoned / 10 by group / 0 SILENT.
3. ALL FOURTEEN GATE PATHS IN MY BRIEF ARE CORRECT. I checked each exists before
   running it and none was missing; the Wave-4 brief's four-wrong-paths
   correction does not apply to the POSTMEAS brief.
4. No line-number citation in my brief was wrong, because it carried none --
   which is the right way round, and worth saying since six lanes corrected the
   coordinator's citations today.

=== 5. OWNER DECISIONS FOUND -- ONE, AND IT IS NEW ===
ALLOCATE TAG CHANNELS FOR DISPLACEMENT AND INK, OR CUT THOSE TWO PLANES.
spec/stars_and_flares.md section 1 is FROZEN with one channel (GLOW = 0b01) and
two of POST.GATHER's three input planes have no source. Evidence in commit 1.

RECOMMENDATION: put it to the owner IN THE SAME BREATH AS R37, since both are
answered by looking at the same sheet and one without the other closes nothing.
Two facts make it cheap to answer: the encoding has TWO FREE channel values
(0b10, 0b11) plus a spare (0b00), so NO WIDENING IS NEEDED; and the plane store
that would hold the result costs ~30 M10K = 5.4% of the device, in a currency
the owner has already ruled generously on
(reports/OWNER-RULING-M10K-CEILINGS-20260918.md).

R37 ITSELF NEEDS NO FURTHER PACKET WORK ON THE GLOW THIRD -- the contact sheet
is rendered and committed at reports/post-gather-law/gather_law_contact.png. It
needs the owner's eye, which is what
reports/OWNER-RULINGS-20260919-EVENING.md:188 already says.

=== 6. FALSE-PRESENCE CLAIMS FOUND ===
  * LEDGER: design/blocks.yml gives POST.GATHER inputs: [dispatch] /
    upstream: [CMD.SCHEDULER] against RTL with zero cmd/dispatch/sched strings,
    and one counter (post_gather_vram_bytes_by_client) that belongs to the
    UNBUILT plane store, not to this block.
  * NAMING: `ink` in commands.zidl:777 is composed and is NOT f_ink_i.
  * NO FALSE ABSENCES FOUND. Everything the three entries called missing is
    missing, and one of them (parambuf) is MORE missing than recorded.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `9bfa51ff`

FINDINGS-postmeas 5/5: I RETRACT my own headline -- R37 DOES cover all three planes, and there is NO new owner decision

Transcribe with commits 1..4. THIS COMMIT OVERRIDES commit 1's conclusion and
commit 4's section 5 in full. Read it before transcribing either.

=== WHAT I WITHDRAW ===
Commit 1 concluded, and commit 4 section 5 recorded as a NEW OWNER DECISION:
    "spec/stars_and_flares.md section 1 is FROZEN with one channel (GLOW =
     0b01) and two of POST.GATHER's three input planes have no source ... a
     ruling on R37 would not unblock this composition ... ALLOCATE TAG CHANNELS
     FOR DISPLACEMENT AND INK, OR CUT THOSE TWO PLANES."

THAT IS FALSE. R37's proposal ALREADY DECIDES ALL THREE PLANES, explicitly.
design/contracts/POST.GATHER.md, the R37 section, decision 3 of 3:

    "Displacement and ink are NOT invented. Channels `0b10` and `0b11` are
     unallocated in the spec, so a fragment carrying one contributes nothing
     and is COUNTED (`reserved_channel`). Under this law `c_disp_x_o`,
     `c_disp_y_o` and `c_ink_o` are ZERO -- a statement about what v1 does, not
     an omission. Ink arrives as a look value on `SetPost` instead (R36), not
     from a tag."

zref::post::gather implements exactly that: kChannelNone = 0, kChannelGlow = 1,
kChannelReserved2 = 2, kChannelReserved3 = 3, and tag_to_fragment() sets
reserved_channel = true for any allocated-but-unknown channel, "so the day
refraction is specified, the counter says whether anything was already drawing
it."

SO THERE IS NO SECOND OWNER DECISION. THERE IS ONE, R37, AND RATIFYING IT
DEFINES THE ADAPTER COMPLETELY. My section 5 recommendation ("put a channel
allocation to the owner in the same breath as R37") should NOT be actioned; it
would ask the owner to re-decide something the proposal in front of them already
decides. Striking it is the whole point of this commit.

=== HOW I GOT IT WRONG, because the mechanism is the reusable part ===
I read the contract's R37 section through `grep -n "R37" -B4 -A12`. That window
printed the heading and the coefficient table, then jumped to the directed
tests, SKIPPING THE THREE NUMBERED DECISIONS IN BETWEEN -- one of which is the
answer. I then measured spec/ carefully, with a positive control I was rather
pleased with, and reported a conclusion about a file I had not opened at the
point I drew it.

THAT IS TERRCOMP'S DIAGNOSED SHARED CAUSE VERBATIM -- "I CITED A FILE I HAD NOT
OPENED AT THE POINT I DREW THE CONCLUSION" -- one day later, in a packet that
had read terrcomp's diagnosis before starting. A positive control proves the
instrument can see; it says NOTHING about whether the instrument was aimed at
the right file. I had rigour on the half that was already fine.

AND NOTE WHICH WAY IT FAILED. CLAUDE.md's law is that a broken instrument lies
in the direction that makes the answer look better, smaller or simpler. THIS
ONE LIED IN THE DIRECTION THAT MADE THE GAP LOOK BIGGER AND MY OWN FINDING LOOK
MORE IMPORTANT -- a brand-new owner decision nobody had found, in a campaign
where finding one is the prize. NOBODY AUDITS GOOD NEWS, and "good news" for a
packet is not the same as good news for the design. The self-flattering
direction is a second blind spot and the rulings do not name it yet.

I found it by re-opening zref_post.hpp for an unrelated reason -- to check
whether channel 0b00 was really spare, one clause in my own commit message that
I wanted to verify before it stood. The habit that caught this was checking a
throwaway detail of my OWN claim. Recommend it generally.

=== WHAT SURVIVES, MEASURED AND RE-CHECKED AGAINST WHOLE FILES ===
All of these are re-verified by reading the entire contract, not a grep window:

1. THE CHANNEL CENSUS IS RIGHT, and it is the EVIDENCE FOR decision 3 rather
   than a counter to it. spec/ defines one channel; positive control 6 hits.
2. R65 IS NOT LOAD-BEARING FOR POST.GATHER. Zero citations in the contract or
   the RTL. Unchanged.
3. R37 OWES A LOOK, NOT PACKET WORK, on the law itself. The contact sheet is
   rendered and committed at reports/post-gather-law/gather_law_contact.png.
   Unchanged, and now the ONLY owner item on this block.
4. THE FLUSH STREAM CARRIES NO ADDRESS. c_index_o is 4 bits "within the tile";
   POST.COMPOSITE reads {view, cx, cy} absolute; zhao_post_gather has no tile-x,
   tile-y or view port. RE-CHECKED BY READING THE WHOLE CONTRACT this time: it
   never names the mechanism. Its entire statement of the seam is "Writes out
   for POST.COMPOSITE"; "Integration capture cases" are scenarios, not seams.
   STANDS, and it is independent of R37.
5. THE PLANE STORE: ~30 M10K = 5.4% of 553, the contract's own shape-based
   number (and the contract itself warns that dividing bytes by block size
   "reads like a measurement and is not one" -- the 30 is not that). Ceiling:
   1,200 ALM, 0 DSP, <= 30 M10K. STANDS.
6. zhao_shell_top_v2.sv:1181 still discards fb_tag_o. One 8-bit port. STANDS.
7. LEDGER: post_gather_vram_bytes_by_client is in no .sv. QUALIFIED --
   check_counters.py already prints this row and about fifteen others, and its
   own last line says "It REPORTS; it does not gate", so the raw mismatch is
   known. What is mine is the reading: a VRAM-bytes-BY-CLIENT counter cannot
   belong to a block whose contract says it "reads no external memory", so the
   row NAMES THE MISSING STORE. Three lists, no two matching: contract asks
   five counters, RTL exports four, ledger declares one.
8. The contract's three zref::post::* citations were checked for the
   phantom-citation shape its own text records. glow_pack565, glow_accumulate
   and disp_to_pixels ALL EXIST. Clean.

=== THE CORRECTED VERDICT ON zhao_post_gather -- STILL REFUSED ===
ONE owner item (R37, awaiting the owner's eye on a rendered sheet) and THREE
build items: the tag->fragment adapter (fully defined the moment R37 lands),
one 8-bit shell port, and a plane store that does not exist and whose tile
origin nobody has specified. Composing today still opens boundary with nothing
behind it. The refusal is unchanged; only my reason for it is smaller and
truer.

AND ONE THING FOR WHOEVER COMPOSES IT AFTER R37: c_disp_x_o, c_disp_y_o and
c_ink_o will ship as DECLARED ZEROS under the ruled v1 law. That must go in the
INCOMPLETE -- TIED OFF, AND WHY block as a RULED SCOPE with R37 cited, not left
as an undeclared tie-off the register cannot see (R159).

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `29d7a591`

FINDINGS-postmeas: correct my own "roughly fifteen" to 119 -- second self-catch

Commit 5 and the I17 bullet said check_counters.py prints the POST.GATHER
counter mismatch as "one of roughly fifteen" rows. COUNTED: 119 such rows
across 43 blocks. I wrote fifteen from an eyeball of the output tail and was
wrong by eight-fold.

The correction STRENGTHENS the point rather than weakening it. 119 declared
counters with no RTL port and no mapping means the raw mismatch carries almost
no signal on its own, so the whole of the value is the READING -- a
VRAM-bytes-BY-CLIENT counter cannot belong to a block whose contract says it
"reads no external memory", so that row names the missing PLANE STORE. Had I
left "fifteen" standing, a reader would have taken the bare mismatch as
evidence. It is not; the interpretation is.

SECOND NUMBER OF MY OWN I HAVE CAUGHT BY RE-COUNTING TODAY. The first was the
core census (I wrote "90 sites, 84 distinct" and it is 88/87, the 90 having
included 2 struct-type declarations my pattern mistook for instantiations).
Both were caught by checking a throwaway figure in my own work rather than by
any gate -- the same habit that caught the R37 retraction in commit 5. No gate
in this repo checks a number written in a comment, so the only instrument is
re-counting, and it has now paid three times in one packet.

Comment-only; zero non-comment lines changed. Gates unaffected.

(Amended once, unpushed, to strip a UTF-8 BOM that PowerShell 5.1's
`Out-File -Encoding utf8` had put at the head of the subject line. Noted
because a BOM in a commit SUBJECT is invisible in most viewers and sorts
strangely; the other five messages were written without it.)

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `e74b897e`

FINDINGS-postmeas: fix a cross-reference, and declare a LIVE-TREE EDIT I made myself

Two small things, one of which is a process fault of mine worth recording
because the campaign's own rule caught me rather than a gate.

1. THE CROSS-REFERENCE. The I17 retraction bullet pointed at "the retraction
   two bullets down"; it is the NEXT bullet. Counted and corrected. A pointer
   nobody checks is exactly how a correction stops being reachable, which is
   the subject of the very entry it sits in (commit 4's "THE CORRECTION IS NOT
   WHERE THE GREP LANDS").

2. I EDITED A FILE INSIDE A RUNNING BUILD'S CLOSURE. `zhao_console_core.sv` is
   one of the 218 sources the console smoke verilates. While the plain form was
   running I committed the "119" correction to that same file. That is
   CLAUDE.md's live-tree trap and QUARTUS_GOTCHAS section 11, committed by me,
   in a packet whose whole subject is re-measuring before quoting.

   SCOPED HONESTLY RATHER THAN WAVED AWAY. The edit landed after the log line
   "declared model units: 67" -- i.e. after Verilator had parsed the sources
   and while it was compiling the 71 translation units -- so the built DUT is
   almost certainly the pre-edit file, which differs from the post-edit file by
   FOUR COMMENT LINES. "Almost certainly" is not the standard this run holds,
   so:
     * only the PLAIN form overlapped the edit. -Mutant, -BadVertex,
       -NoEchoArm and -BadTraceArm each start after the previous form exits, so
       all four read a settled tree and are clean BY CONSTRUCTION, not by
       argument.
     * the plain form is RE-RUN from the quiescent tree at this commit, and the
       re-run is the one quoted.

   I did NOT kill the batch to restart it, deliberately. Ruling R81 forbids
   killing by process name or start time, several lanes share this machine and
   all of them run verilator_bin and g++, and a killed g++ leaves the
   no-error-text COMPILE FAILED signature the protocol documents. Waiting cost
   about forty minutes of wall time and zero risk to anyone else's gating run.
   That is the right trade and it is the one the protocol already made.

SIX SMOKE FORMS GREEN at the previous commit: LintOnly, plain (raster
pixels=2560, frames_admitted=1, SMOKE_RC=0), -Mutant, -BadVertex, -NoEchoArm,
-BadTraceArm, all SMOKE_RC=0.

Comment-only; zero non-comment lines changed.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

