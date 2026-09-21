<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
778e4846 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- SHEETSEAM


## `778e4846` -- SHEETSEAM FINDINGS -- the harness refused a FINDINGS .md, so the record is here

```
SHEETSEAM FINDINGS -- the harness refused a FINDINGS .md, so the record is here

The tenth lane in a row refused permission to write
`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-sheetseam.md`
("Subagents should return findings as text, not write report files"). Per the
packet brief the findings go in the commit messages instead and the coordinator
transcribes. The refusal was explicit, so it was NOT routed around with a
different tool.

=== 0. THE HEADLINE NUMBERS ===

  completion register      21 -> 22   (9 tie-offs + 12 -> 13 disconnected)
  superseded check         74 production roots CLEAN, before AND after
  latency option taken     THE PREFETCH
  prefetch size            1,089 bytes = ONE M10K, against the brief's 8,192
  prefetch time            1,091 cycles uncontended, 1,092 with the stamp
                           interleaving; the stamp waited 0
  directed test            sheetseam_rtl_directed, 81 checks, 0 failures,
                           BUILT AND RUN
  R221's counter           fallbacks_o, fired by STIMULUS on three
                           independent routes
  tie-offs created         NONE
  zhao_console_core.sv     53 lines added, ALL COMMENTS; zero RTL, zero removed

THE REGISTER RISING IS R214's SHAPE. completion_register.py walks
design/blocks.yml, so a capability with no row is not absent from the console
-- it is absent from the QUESTION. Without the TERRAIN.SHEETSEAM row this
packet would have built real silicon that no instrument can see, and "the
number did not move" would have been indistinguishable from "nothing was
built". It lands as BUILT BUT NOT CONNECTED rather than UNRESOLVABLE, because
the module exists at the name the register constructs by convention.

=== 1. AN INHERITED NUMBER THAT HAD EXPIRED (R165) ===

FINDINGS-pageio.md records the register at 22 at the end of that lane. On the
merged head 5720a8b0 it reads 21, with TERRAIN.PAGEIO already inside the twelve
disconnected rather than sitting as an unresolvable thirteenth. So 21 is this
packet's baseline and 22 is its result. Quoting PAGEIO's 22 as the starting
point would have reported this packet as having moved nothing -- the flattering
direction for a refusal and the wrong one for the campaign.

EVERYTHING ELSE PAGEIO MEASURED STILL HOLDS, re-verified here: the req_* port
is control-and-read with OP_ACQUIRE / OP_READ / OP_RELEASE, a 32-bit handle and
a separate pg_* stream carrying ST_HIT / ST_ALLOCATED / ST_OVERFLOW / ST_MISS;
Slots defaults to 2; sheet_texel_o is combinational on the dig cursor (`assign
sheet_texel_o = sd_texel`) and sheet_strength_i is sampled inside
`if (vtx_valid_i && vtx_ready_o)`; the core annotates the port "REAL:
SURFACE.STAMP is the only requester".

=== 2. WHICH LATENCY OPTION, AND THE PRICE OF BOTH ===

TAKEN: THE PREFETCH. The note priced the two as "1,089 round trips per record,
or an 8,192-byte second copy of layer F". THE SECOND FIGURE IS AN
OVERSTATEMENT BY 7.5x, and that correction is what decides it.

A -- THE PREFETCH IS 1,089 BYTES, NOT 8,192. Two halvings, both read off RTL:
  * BAKE NEVER READS THE TAG. zhao_terrain_bake_v2 has sheet_strength_i and no
    tag port at all. Layer F is {tag u8, strength u8}, so half of the 8,192
    bytes is a plane this consumer cannot see. 4,096 left.
  * ONLY 1,089 OF 4,096 TEXELS ARE ADDRESSABLE. Section 9.3(b), as
    zhao_terrain_stampdepth states it, is ti = (vi >= 32) ? 63 : 2*vi, so the
    33x33 lattice samples a decimated grid -- 33 distinct ti and 33 distinct
    tj. CHECKED IN RTL, not asserted: case 0 walks all 1,089 vertices through
    the real stampdepth instance and counts the distinct texels. 1,089.
  8,712 bits = ONE M10K (306 of 553 in use, R87) against the feared 65,536 --
  about seven. It is NOT a second copy of layer F; it is layer F RESAMPLED ONTO
  THE LATTICE, a different and much smaller object.

B -- ~1,090 CYCLES, ONCE PER RECORD, OFF THE DIG'S PATH. zhao_surface_sheet
  accepts a request whenever its single response slot is free, so with the
  response drained the port sustains ONE READ PER CLOCK. Measured: 1,091.

C -- THE PER-VERTEX ROUND TRIP LOSES ON THREE COUNTS, only the first is speed:
  1. ~2 cycles per vertex IN SERIES with the dig, ~2,178 added, in the worse
     place.
  2. sheet_strength_i is sampled with vtx_valid_i, WHICH THIS BLOCK DOES NOT
     OWN -- zhao_terrain_pagestream drives A/B/C and zhao_terrain_pageio drives
     nb_o. A per-vertex adapter must gate a valid owned by two other blocks, so
     the blast radius is a port change on the terrain page spine rather than
     one new file.
  3. THE SHEET'S LATENCY IS NOT BOUNDED. req_ready_o is LOW for the whole
     4,096-CYCLE CLEAR SWEEP of an allocating ACQUIRE. Per-vertex, a stamp's
     ACQUIRE lands the dig in a 4,096-cycle stall mid-lattice while holding the
     A/B/C page beat. The prefetch phase absorbs it; the dig never sees it.

A disc record prefetches NOTHING and pays nothing -- asserted, 0 requests.

2.1 AND THE ANSWER IS A VALID, NOT AN ASSUMPTION ABOUT BAKE'S TIMING

A 1,089-byte store answering combinationally is 8,712 FLOPS, eight times
pageio's shadow plane. A synchronous M10K answers one cycle late. Bake leaves
three cycles between advancing the cursor at StEmit and raising vtx_ready_o at
StVtx -- so a registered read would always be in time, AND RELYING ON THAT IS A
COUPLING TO A TRAVERSAL, the exact trade pageio wrote down and refused for its
own shadow plane. So the block publishes str_valid_o, which the composer ANDs
into vtx_valid_i; bake's own port comment already specifies that contract.
It costs ZERO stall cycles at bake's real traversal -- measured, and fired
before the zero was quoted.

=== 3. THE HANDLE AND ITS LIFETIME: THERE ISN'T ONE, AND THAT IS FORCED ===

The brief warns that "a bake that acquires and never releases leaks one of
Slots = 2". True -- and the way not to leak a slot is not to take one. THIS
BLOCK ISSUES OP_READ AND NOTHING ELSE.

Not thrift. zhao_surface_sheet.sv's do_acquire_new sets dir_live, starts the
4,096-cycle clear sweep and answers ST_ALLOCATED -- it ALLOCATES A BLANK SHEET,
every texel zero, every vertex uncovered, the record digs nothing. That is
R221's EXPLICITLY REFUSED "dig zero", wearing a residency costume so that no
status code says a miss happened, AND it steals one of Slots = 2 from the only
block terrain_rules 7 allows to write layer F. OP_READ is the only opcode that
reports residency without changing it.

MEASURED, NOT CLAIMED: the bench samples the seam's own req_op on every cycle
of every case. Over the whole suite -- 0 OP_ACQUIRE, 0 OP_RELEASE, thousands of
OP_READ, and res_occupancy_o BYTE-IDENTICAL across every bake.

=== 4. THE ARBITER: POLICY ADOPTED, NOT RE-DECIDED ===

zhao_surface_sheetshare.sv is the block entry I32 asks for, at the exact type
it names. ROUND ROBIN, one last_q flip-flop, adopted from zhao_terrain_psmux.
Both priority orders are rejected in the header WITH their numbers: priority to
the stamp costs the prefetch ~13 cycles of 1,089 and is unbounded in a burst;
priority to the bake locks the stamp out for 1,089 consecutive cycles on the
player's own action. Round robin bounds both at one beat, so neither number has
to be trusted. Measured: the stamp waited 0 cycles; 1,091 -> 1,092.

PAGEIO's ITEM-3 HAZARD IS NOT AN ARBITER'S TO SOLVE, and that is a correction
worth recording. "A bake that reads a half-applied stamp digs a shape the
player did not make" is a FRAME-ORDER question -- bake runs in 9.2's bake
window, after the stamp pass -- and no arbiter priority changes it. What the
arbiter must actually do is not starve either client.

THE ONE SAFETY ARGUMENT AN ARBITER OWES WAS CHECKED RATHER THAN INHERITED. The
grant reads both clients' valid combinationally, which makes each client's
ready a function of its own valid, and that is safe only while neither client
derives valid from ready. Client A, zhao_surface_stamp: req_valid_o is
(acq_valid && !acq_sent) || (cursor_slot && geom_ready && fld_ok && covered &&
s1_free_next); its only uses of req_ready_i are read_path_ok -- which feeds
fld_ready_o and advance, NOT req_valid_o -- and the acq_sent latch. Client B is
pure registered state. Both are named in the share's header so the next reader
does not have to re-derive it.

=== 5. R221 ITSELF, AND THE PROOF ITS COUNTER FIRES ===

The fallback is ONE BIT: bk_depth_sheet_o goes low and zhao_terrain_bake_v2
runs the law it always ran. R221's own reasoning is the whole implementation --
SEAMDIG measured the sheet mode additive (terrain_bake_v2_directed 267/267
unchanged), so a fallback record is bit-identical to the same record with
cmd_depth_sheet_i low, and that equivalence is 267 EXISTING checks.

ANY non-ST_HIT fails the record, and A MISS ANYWHERE IN THE 1,089 READS FAILS
THE WHOLE RECORD, not the vertex -- mixing laws inside one crater would invent
a fourth shape, half sheet and half disc with a seam nobody authored, and R221
forbids inventing a third.

fallbacks_o FIRED BY STIMULUS ON THREE INDEPENDENT ROUTES, no mutant required
(a miss is legally reachable at this block's own port -- pageio's distinction
between its five stimulus-fired counters and wq_overflow_o):

  case 2   a handle nobody ever acquired. The store's own verdict is read
           FIRST through client A, so the test is anchored to ST_MISS and not
           merely to "something went wrong"
  case 4   the sheet RELEASEd mid-prefetch
  case 10  a real Slots = 2 ACQUIRE overflow -- R221 calls a miss "a residency
           failure", and this is that failure produced by the store's own C2
           policy rather than by a handle chosen to be absent

ITS NEGATIVE CONTROL IS IN THE SAME EXECUTABLE: on a resident sheet
fallbacks_o stays 0 while sheet_served_o moves and all 1,089 vertices carry the
byte the store holds. And case 3 asserts a DISC record is not counted as a
fallback -- a disc record did not fall back, it chose.

5.1 THE CHECKS THEMSELVES WERE FIRED, AGAINST TWO DELIBERATE DEFECTS
Scratchpad only, never in the tree, so there was no live-tree hazard and no
Copy-Item restore to get wrong.

  * A STATUS-BLIND FILL (treat every response as a hit) -- R221's defect in its
    exact shape, one dropped `if` away at all times. 12 FAILURES ACROSS THREE
    INDEPENDENT CASES (2, 4, 10). CASE 1 STILL PASSES, which is the point: a
    status-blind seam is perfect on a hit and invisible to every happy-path
    check.
  * A TRANSPOSED READ INDEX -- 1,040 of 1,089 vertices wrong, in four cases.

=== 6. TWO DEFECTS FOUND IN MY OWN WORK BEFORE IT SHIPPED ===

Both silent in the flattering direction; neither would have failed a gate.

THE SHARE'S FIRST VERSION HALVED THE PORT'S BANDWIDTH.
zhao_surface_sheet's req_ready_o gates on pg_slot_free = !pg_valid_q ||
pg_ready_i, so the store accepts a request IN THE CYCLE IT HANDS BACK THE
PREVIOUS RESPONSE -- it sustains one read per clock. An arbiter that grants
only on !busy_q turns that into one read every two clocks, doubling the
prefetch to 2,178 cycles WITH EVERY HANDSHAKE LEGAL AND EVERY COUNTER
AGREEING. can_grant_c is the one line that fixes it and it carries a paragraph
saying so. Found by measuring the fill, not by reading the code.

MY OWN BENCH'S STIMULUS WAS WRONG AND THE COUNTER WAS RIGHT. The first draft of
dig() held dig_ready_i high through every address change and dig_stall_cycles_o
read 1,089 -- one per vertex. Bake's vtx_ready_o is (state == StVtx) &&
sc_free and the cursor advances at StEmit, three states earlier, so the real
consumer never does that. The dig loop now transcribes bake's state sequence
and says where it came from; THE OLD STIMULUS IS KEPT DELIBERATELY AS CASE 6,
the counter's positive control. The temptation was to weaken the assertion;
what it actually needed was a bench that models the consumer it claims to.

A THIRD, SMALLER ONE, in the measurement rather than the design: case 9 first
reported the CONTENDED prefetch at 990 cycles against 1,091 uncontended, which
is the one thing it could not honestly be. offer() measures only from where it
starts and the case had already spent ~100 cycles setting up. The preamble is
added back now and the comment says what the tell was.

=== 6b. AND TWO MORE, FOUND BY RE-READING MY OWN CONTROL PATH (commit 72023e6a) ===

Neither changes any observable behaviour -- 81 checks still pass, 1,091
prefetch cycles unchanged -- and neither was found by a gate.

A FILL NO LONGER STARTS WHILE ANYTHING IS IN FLIGHT. The abort arm can leave
ONE request outstanding; if the producer then immediately offers a DIFFERENT
patch, that response belongs to the OLD handle while w_idx_q has been reset
for the NEW one -- one patch's strength byte written into another patch's slot
0, every handshake legal, every counter balanced. This block's own record-swap
defect, arriving through the back door of the file whose header claims to have
designed it out.

IT WAS ALREADY UNREACHABLE, and that is the interesting part: the str_q write
is gated on state_q == S_FILL and the store answers exactly one cycle after
its request fires, so the stale response always lands in the S_WATCH cycle
BETWEEN the two fills. Traced, and sound today. It is still a TWO-BLOCK TIMING
ARGUMENT holding a correctness property, about a port whose ledger row and
module header both call its latency VARIABLE -- and CLAUDE.md's lockstep
chapter is entirely about correctness properties that rest on WHEN things
happen. So `out_q == 0` is now a condition on starting a fill. NO TEST
DISTINGUISHES THE TWO VERSIONS, said plainly rather than papered over with a
case that would pass either way.

THE out_q DECREMENT IS GUARDED AGAINST UNDERFLOW. Two bits; an orphan response
with nothing outstanding would take it to 3, fill_done_c requires
out_next_c == 0, and the block would WEDGE -- a silent stall, which this
file's own psmux citation calls worse evidence than a counted anomaly.

=== 6c. THE ELABORATION GUARDS FIRE. MEASURED, NOT ASSUMED ===

CLAUDE.md: "--lint-only does not run initial blocks. A clean lint is not
evidence about an elaboration check." This block has four of them and they had
a clean lint and no evidence. A scratchpad copy of tb_sheetseam.sv that
instantiates the DUT with .Lat(34), .SheetEdge(66) was built with the REAL
test main and the recipe that already works -- PAGEIO's lesson that a bespoke
driver is a second build recipe and a second thing that can be wrong -- and
run:

    %Fatal: zhao_terrain_sheetseam.sv:679: Assertion failed in
            TOP.tb_sheetseam.u_seam: zhao_terrain_sheetseam: SheetEdge must be
            64 -- the terrain page format is FROZEN by owner ruling R194

The parent's initial block runs before the child's, so the message is this
block's own and not zhao_terrain_stampdepth's. Nothing in the tree changed;
the break lives in the scratchpad and the evidence is this paragraph.

=== 6d. THE TREE COULD NOT CONFIGURE, AND IT WAS NOT MINE ===

Found only because validating my own tests/CMakeLists.txt edit is the one
reason anybody ran `cmake --preset windows-native`. It failed at
`verilate(test_shell_paired_diff_mutant)` with 7 PINMISSING promoted to an
error: tests/mutants/zhao_shell_paired_diff_mutant.sv instantiates
zhao_shell_top_v2 and is short the seven gth_* gather outputs.

NOT MINE, and checkable: both sides of the mismatch are BYTE-IDENTICAL to my
base 5720a8b0 and neither is in `git diff --name-only 5720a8b0 HEAD`.

FOUR INSTRUMENTS WERE GREEN THROUGH A TREE THAT CANNOT CONFIGURE, each blind
for its own stated reason:
  * gen_shell_paired_diff.py --check (the registered ctest) validates the
    GENERATED harness and nothing else. The generator writes a SECOND file and
    asks about it only under `--check --mutant`, which NOTHING RAN. Run by
    hand, it said "FAIL: ... is STALE" immediately. The tool always knew.
  * mutant_copy_drift.py SKIPPED THE FILE: it compares commit order against a
    PRODUCTION module and this copy's subject is a GENERATED TEST HARNESS, so
    it is one of that tool's own "16 files matched no production module" --
    counted in a parenthesis on line 1 and never named.
  * wrapper_port_parity.py (R220's new gate) covers the two console-core
    wrappers and compares PORT LISTS; this fault is in an INSTANTIATION's PIN
    LIST.
  * mutant_drivers.py asks whether something BUILDS the mutant. Something
    does. It cannot ask whether that build succeeds.

R220 quoted R162 a day early: "a wrapper cannot drift in its BODY, but its
PORT LIST can." This is the THIRD mirror file and the gate R220 built does not
reach it. It sat because NOTHING CONFIGURES THE TREE -- every lane builds its
directed tests by direct static g++ link and the smoke forms have their own
build directories under $env:TEMP.

REPAIRED BY REGENERATION, not narrowing:
`python tools/design/gen_shell_paired_diff.py --mutant`, eight lines, seven of
them the missing pins connected EMPTY exactly as the generator already does in
the non-mutant harness. The one substantive mutation (MUTANT_PORT =
px_valid_o) is applied by the generator itself, which refuses to emit a mutant
whose mutated port is not compared.

AND THE MISSING GATE IS ADDED: packet_h_paired_diff_mutant_fresh runs
`--check --mutant`, and it was SEEN TO FIRE in both directions minutes apart
-- RC 1 on the stale file, RC 0 after the regeneration.

That file is outside this packet's declared set and no live lane owns a shell
mutant; IF IT CONFLICTS, DROP THAT COMMIT. The finding is the valuable half.
=== 7. WHAT DID NOT CLOSE, AND THE BLOCKER R221 DID NOT ADDRESS ===

NEITHER I32 NOR I27 MOVED, AND THE SEAM WAS NEVER WHAT BLOCKED THEM.

zhao_terrain_bake_v2's cmd_* RECORD PORT HAS NO PRODUCER ANYWHERE IN THE TREE.
Re-verified here 2026-09-21: zhao_terrain_cmd emits rec_island_o / rec_ix_o /
rec_iz_o / rec_hps_addr_o / rec_crc_o / rec_flags_o -- a patch DIRECTORY record
-- and nothing resembling {cx, cz, radius, depth_from, depth_to, env_*}. Bake
cannot be composed, so its consumers cannot be either.

AND job_handle_i HAS NO PRODUCER FOR THE SAME REASON, which is the one new
thing this packet adds to that gap. Whatever block eventually emits bake
records must emit the patch's SHEET handle32 beside them. It must NOT be
synthesised from cmd_patch_id_i -- zhao_surface_sheet's choice C4: "the handle
is the identity the ABI carries; using anything else re-derives identity that
was already stated." A second identity law invented inside a record producer is
the kind nobody looks for later. Recorded in entry I32 and in section 7 of the
new contract.

BOTH NEW BLOCKS ARE THEREFORE BUILT AND NOT COMPOSED, the same disposition as
zhao_terrain_pageio and for the same reason. Composing the share alone would
put an arbiter between SURFACE.STAMP and a DEAD SECOND CLIENT -- a tie-off
wearing a block's clothes, and the campaign's first rule forbids it.

NO TIE-OFF WAS CREATED. fpga/rtl/prod/zhao_console_core.sv gained 53 lines, ALL
OF THEM COMMENTS (`git diff | grep '^+' | grep -v '^+//'` returns nothing, zero
lines removed), so the "INCOMPLETE -- TIED OFF, AND WHY" block needed no new
declaration (R159) and packet_h_tieoff_audit stays RC 0.

=== 8. TWO LAPSED REFUSALS STRUCK ===

  * design/contracts/TERRAIN.BAKE.md's "THE SHEET SEAM -- explicitly undecided,
    not invented" said closing the seam "needs two laws that do not exist
    anywhere in this tree". BOTH EXIST -- 9.3(a)'s art table and 9.3(b)'s
    nearest-texel resample, ratified by R15/R56/R65, accepted BY LOOKING under
    R194, in RTL since 2026-09-20. STRUCK THROUGH, NOT DELETED: the reasoning
    was right and the next reader should see a careful refusal expiring. Sixth
    in this subsystem.
    WHAT THE AMENDMENT REFUSES TO OVERSTATE: the `inputs: [stamp_results]`
    ambiguity in that section's own table is NOT resolved by any of this. R194
    and R221 settled the per-vertex depth SOURCE inside a record, not who
    produces the records -- the same distinction R194's own text got wrong when
    it said it unblocks I32 "directly", and which DOSSIERCHECK corrected before
    the owner ruled.
  * design/contracts/TERRAIN.PAGEIO.md's decision 5 is ANSWERED, with the three
    corrections above recorded on that page rather than only in the new
    contract, because it is the page the next reader opens.

=== 9. INSTRUMENTS ===

Nine counters on the seam, four on the share. Every one asserted SILENT on the
clean path and then FIRED:

  fallbacks_o (R221's)    ghost handle / mid-fill RELEASE / Slots = 2 overflow
  miss_texels_o           the same
  refetches_o             the handle swapped under a running fill
  dig_stall_cycles_o      dig_ready held high through a jumping cursor, 40/40
  bad_texels_o            an odd texel, x2
  stray_done_o            bake_done with nothing in flight
  share pg_orphan_o       a SECOND, STANDALONE share instance the bench drives
  share pg_op_mismatch_o  directly -- legal stimulus at that module's own port,
                          psmux's exact reasoning for stray_v_o

NO COMMITTED MUTANT IS NEEDED AND NONE WAS WRITTEN. At these blocks' own ports
the illegal input is ordinary stimulus, because the bench stands where
SURFACE.STAMP and TERRAIN.BAKE will stand.

THE ONE STRUCTURAL INVARIANT NO LEGAL INPUT CAN REACH -- two reads in flight --
CARRIES A SIMULATION ASSERTION AND NOT A COUNTER, deliberately: a counter no
legal input can move owes a committed mutant, and this is a statement about the
COMPOSITION rather than about this design's own reachable states.

THE RECORD-SWAP DEFECT IS DESIGNED OUT, TWICE. pf_handle_q is latched by this
block at fill start and differenced against the handle the producer is
OFFERING; rd_texel_q is loaded by this block's read issue and differenced
against the texel BAKE's cursor drives. Both comparisons have their two sides
loaded by DIFFERENT ENABLES -- CLAUDE.md's lockstep chapter applied before the
fact rather than after it.

=== 10. FILE SETS, AND THE TWO LIVE LANES ===

DISJOINT from VIEWMASK (TERRAIN.GROUP_SEQ / I21) and TAGPROD (I20). This packet
touched no *group_seq* file and no raster file. The shared files it did touch
are design/blocks.yml, design/console_inventory.yml, design/prod_manifest.yml,
design/fit_targets.yml, tests/CMakeLists.txt and
fpga/rtl/prod/zhao_console_core.sv -- every one an INSERTION AT A DISTINCT
ANCHOR, and the core's is comment-only inside entry I32.
`git diff --cached --name-only` was read before every commit.

ONE PRE-EXISTING DRIFT, NOT MINE. mutant_copy_drift.py reports RC 1 on
zhao_geom_bonesrc_latefetch_mutant (mutant 3b87e411, production 10635433, both
from the POSEABI lane on 2026-09-21 and both ANCESTORS of my base 5720a8b0).
My diff touches neither file. Run AFTER my commits, per R121.

=== 11. GATES, at the pushed head, in this worktree ===

  completion_register.py        22 (9 tie-offs + 13 disconnected); 21 before
                                superseded check: 74 production roots CLEAN
  check_console_inventory.py    OK    361 modules
  check_prod_manifest.py        OK    361 modules, 73 tops, 166 excluded
  check_quartus17_syntax.py     RC 0  566 files, no rejected forms
  check_case_labels.py          OK
  mutant_copy_drift.py          RC 1  ONE drift, pre-existing (POSEABI), none new
  wrapper_port_parity.py        1264 = 1264 both wrappers, missing=0 stale=0
  mutant_drivers.py             RC 0  96 of 98, the same 2 on the debt list
  uncashed_cheques.py           RC 0
  check_counters.py             RC 0  all nine sheetseam_* resolve BY EXPLICIT
                                MAPPING -- read the list, not just the RC
  refmodel_liveness.py          RC 0
  duplicate_functions.py        RC 0
  gen_prod_top.py --check       FRESH 73 instances -- checked, not assumed
  gen_console_board.py --check  FRESH 1279 core ports, 73 parameters
  gen_shell_paired_diff --check FRESH
  packet_h_tieoff_audit.py      RC 0  0 SILENT; core RTL byte-untouched
  verilator --lint-only -Wall   0 warnings on both new blocks and the bench
  sheetseam_rtl_directed        81 checks, 0 failures -- BUILT AND RUN (R60),
                                by a direct static g++ link AND through the
                                registered path: cmake --preset RC 0,
                                `cmake --build build --target
                                test_sheetseam_rtl_directed` BUILD_RC=0, and
                                `ctest -R sheetseam_rtl_directed` PASSED.
                                Quoted from the build's own exit code, not a
                                pipeline's
  ctest lint_terrain_sheetseam / lint_surface_sheetshare /
        packet_h_paired_diff_fresh / packet_h_paired_diff_mutant_fresh /
        console_core_tieoff_audit                       5/5 PASSED

NO reference_model: ON THE NEW LEDGER ROW, AND THE ABSENCE IS A CLAIM. The
arithmetic law this seam feeds is 9.3's, whose oracle
zref::terrain::stamp_depth_at_vertex is ALREADY declared on TERRAIN.BAKE.
Declaring it twice is exactly what uncashed_cheques.py check 3 exists to catch.
This block implements no arithmetic -- it is a residency and latency adapter,
and the reference renderer has no counterpart at all, since
zref::render::sheet_for "creates on first use" and the miss this block exists
to handle CANNOT HAPPEN in software.

=== 12. THE SEVEN CONSOLE-CORE SMOKE FORMS ===

Run with `powershell -NoProfile -ExecutionPolicy Bypass -File`, one per form,
FULL OUTPUT CAPTURED TO A FILE AND FILTERED AFTERWARDS (R82: never filter the
live stream -- a failing run is the one you cannot re-capture). Each log was
then GREPPED FOR %Fatal, because R207 records a form that PASSED while
printing nine of them: a `$finish` in a verdict arm is not a `return`, so arms
after the verdict still run.

  form            RC   %Fatal  %Error  what it showed
  (no flag)        0      0       0    PASS -- raster pixels=2560 over 160
                                       bursts, 14 triangles, 1 admitted frame,
                                       1190 fragments carried a texel,
                                       frames_admitted=1
  -LintOnly        0      0       0    silent; "elaborates. This says NOTHING
                                       about what the console does."
  -Mutant          0      0       0    PASS -- terr_pl_slot_overflow_o fired
                                       1 time. "The detector works;
                                       production's zero is a measurement."
  -UntexMutant     0      0       0    PASS -- geom_untex_refused_o fired 16
                                       times (want 16), clip_submitted=0,
                                       raster_pixels=0, the window's
                                       accounting held. This is R220's wrapper
                                       and it is still exact; this packet
                                       changed no core port, and
                                       wrapper_port_parity 1264 = 1264 says so
                                       statically while this says so by
                                       elaborating it
  -BadVertex       0      0       0    PASS -- one refused record dropped its
                                       batch (holes=1, groups_poisoned=2,
                                       replay_poisoned=8) and the frame
                                       completed. The same three numbers
                                       SEAMDIG recorded
  -NoEchoArm       0      0       0    PASS
  -BadTraceArm   0      0       0    PASS -- trace armed=0000000
                                       stored=0 dropped=0 against 13 decoder
                                       records, arms=0 arm_refused=1: the
                                       reserved bit was refused WHOLE and
                                       nothing was armed
ALL SEVEN FORMS READ A TREE NOBODY WAS EDITING. The one file this packet
touches that IS in their 219-source closure -- fpga/rtl/prod/zhao_console_core.sv
-- was committed BEFORE the first form started, and the only edits made while
they ran were to files outside it (zhao_terrain_sheetseam.sv,
zhao_surface_sheetshare.sv, both uncomposed; the script globs no .sv, checked).
CLAUDE.md: a suite whose inputs moved underneath it is not evidence in either
direction, and the greens are worth no more than the reds.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
