<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
c60f454d -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- TERRASSEM


## `c60f454d` -- TERRASSEM FINDINGS: register 24 -> 25 honestly. The sp_* assembler is BUILT, two more blockers shrank, and a THIRD rotted citation nobody asked for

```
TERRASSEM FINDINGS: register 24 -> 25 honestly. The sp_* assembler is BUILT, two more blockers shrank, and a THIRD rotted citation nobody asked for

THE HARNESS REFUSES A LANE REPORT FILE at
runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-terrassem.md, exactly as
this packet's brief predicted and exactly as it refused gz/jobissue that morning.
This commit message IS that document -- the campaign's established pattern.

=== FINDINGS -- TERRASSEM (entry I21, TERRAIN.LOD's sp_* assembler) ===========

2026-09-21. Register 24 -> 25, run BARE, RC 1 (normal). One block BUILT and
tested, its ledger row registered by the packet that built it, THREE rotted
citations corrected, and the composition still REFUSED -- with the refusal
measured in this tree and converted from an argument into a named sequence of
four builds.

-- HEADLINE: TWO OF I21's THREE BLOCKERS WERE SMALLER THAN STATED ------------

Packet JOBISSUE named this target and quantified the payoff. It listed three
absences and refused all three BY NAME. ALL THREE WERE ATTEMPTED BEFORE ANY WAS
BELIEVED (R237: a blocker that DEFERS work gets believed and re-quoted, so check
that direction hardest). Two shrank.

  1. "sp_cx_i/sp_cz_i HAVE NO PRODUCER." They have one and the tree already
     named it: zhao_terrain_lodfeed's own w_cy_o port comment says "x and z come
     free from TERRAIN.PLACE's placed column/row stream at serve time". That
     stream is written into zhao_terrain_compcache_front on pos_* and served
     back on lat_wx_o/lat_wz_o. A NAMED SOURCE WITH NO READER IS A BUILD, NOT AN
     ABSENCE -- JOBISSUE said exactly that and was right.
     The block reads that port. It does NOT recompute placement from an origin
     and a pitch, which is the available shortcut and a SECOND IMPLEMENTATION of
     TERRAIN.PLACE's law. And it reads the SERVE port, not the write stream,
     because observing pos_we_i would force this block to hold its own
     fill/serve PARITY -- a decision the compose cache has already made
     (wx_m[(serve_par_q ? LAT_W : 0) + rd_vi_c]).

  2. "zhao_terrain_devstore HAS NO src_id COLUMN." True, AND IT DOES NOT NEED
     ONE. The store is read with a slot the assembler already holds, and
     sp_src_id_o is the id popped from the door beside that slot. A column would
     carry the id in at page load and out at serve to reach a block that was
     handed it at the door. w_src_id_o keeps its R70 reader (MEASURE.HISTOGRAM)
     untouched -- NOTHING IS NARROWED.

  3. "KEYED BY SLOT, SERVED BY SRC_ID, WITH NO MAP." Real, and it is why the
     block exists. THE ANSWER IS NOT A MAP. A src_id->slot map needs one entry
     per resident page -- 1,024, compared associatively against a 16-bit id,
     because a page stays resident across frames and is re-composed every frame
     without being re-loaded. That is a CAM.
     THE SLOT IS KNOWN AT THE COMPOSE DOOR. zhao_terrain_pagestream emits
     v_slot_o and v_src_id_o ON THE SAME VERTEX BEAT, and the compose fill starts
     on one of those beats. So the pair is captured at the door, queued, and
     popped for the patch the cache serves -- WHICH IS zhao_terrain_jobissue's
     DRAW-CONTEXT QUEUE EXACTLY, one block old. Its arming law (serve_seen_q) is
     COPIED rather than re-derived: two blocks arming off one event must not
     have two laws.

-- 1. WHAT WAS BUILT ---------------------------------------------------------

fpga/rtl/terrain/zhao_terrain_spdesc.sv -- TERRAIN.SPDESC -- with
design/contracts/TERRAIN.SPDESC.md, 1,035 checks in
tests/terrain/terrain_spdesc_directed.cpp, and rows in design/blocks.yml,
design/prod_manifest.yml and design/console_inventory.yml.

THE PAIRING IS CHECKED AND THE CHECKER CAN FIRE. door_src_mismatch_o differences
the popped src_id against serve_src_id_i. CLAUDE.md's metadata-swap chapter is
why that sentence is not sufficient alone, so explicitly: the popped id is
written by the door's acceptance and read by the pop; serve_src_id_i is
combinational off the cache's SERVE PARITY, which moves on the swap. DIFFERENT
ENABLES, DIFFERENT PORTS -- a swap that did not happen moves exactly one of the
two. The test fires it deliberately.

IT NEVER DELAYS THE LATTICE PORT'S EXISTING CLIENT, AND THAT IS FORCED RATHER
THAN CHOSEN. zhao_terrain_compcache_front's lat_req_i HAS NO READY, so a
pass-through that held a request up would not delay it -- IT WOULD DESTROY IT,
and TESS would read a stale datum with every counter agreeing.
zhao_terrain_heighttap's law is copied: upstream first, this block's read on the
cycles upstream leaves. The response needs NO MUX AT ALL; the two cases are
exclusive by construction. The directed test checks that property ON EVERY CYCLE
OF EVERY CASE, not in one case.

LOSING THAT RACE IS A DURATION, NOT A FAULT (lat_wait_clocks_o) -- JOBISSUE's own
correction from this run, applied BEFORE the mistake instead of after it.

-- 2. THE TEST CORRECTED THE DESIGN, AND THAT IS THE BEST THING HERE ---------

patches_unfresh_o was first sampled in StStart, on the cycle r_start_o is
presented. zhao_terrain_devstore LOADS r_fresh_q FROM slot_valid_q[r_slot_i]
INSIDE ITS OWN `R_IDLE: if (r_start_i)` ARM, so on that cycle the port still
carries the PREVIOUS patch's answer. The counter attributed page A's freshness to
page B -- silently, and in the flattering direction whenever A was fresh. It is
now sampled on the FIRST RECORD, in R_STREAM.

Found by READING THE PRODUCER rather than trusting the port. Lint, the Quartus
gate and functional correctness are all silent on this class.

AND THE PLANT IS PROVEN BEFORE THE FIRE IS QUOTED. Case 8 runs a fresh patch then
an unfresh one and asserts that on the second start cycle the port really did
carry the first patch's flag (start_cycles_where_fresh_differs > 0). Without
that, "the counter fired" would not distinguish a correct sample from a lucky
one -- CLAUDE.md: a gate that cannot reach the state is not evidence about the
state.

R95, MEASURED NOT ASSERTED: all five FAULT counters fire from the block's own
boundary with legal stimulus, each with a NEGATIVE CONTROL in the same case, so
NO COMMITTED MUTANT IS OWED.

  door_refused_o       case 5      store_wait_clocks_o  case 10  (instrument)
  serve_no_door_o      case 6      lat_wait_clocks_o    case 4   (instrument)
  door_src_mismatch_o  case 7      assemble_clocks_o             (instrument)
  patches_unfresh_o    case 8
  sp_order_bad_o       case 9

Case 10 also checks store_wait_clocks_o is FLAT WHILE IDLE and FLAT AGAIN
AFTERWARDS -- it is not a free runner.

-- 3. HOW FAR THE TERRAIN GROUP GOT, AND WHAT STILL STOPS IT -----------------

IT DID NOT COMPOSE, and the refusal is measured here rather than inherited.
zhao_terrain_lod's sp_* port is NO LONGER WHAT STOPS IT. Four things are:

  (a) MEASURE.GOVERNOR's SIX KNOBS -- cam0/1_scale_i, cam0/1_en_i, hyst_i,
      min_hold_i, morph_step_i. The governor is built and uncomposed.
      Width-for-width these six match its outputs exactly; THE GAPS ARE
      OWNERSHIP GAPS, NOT WIDTH GAPS. Two of the governor's own inputs have no
      producer: px_err0/1_i and view_count_i. R223 item 4 parks the governor
      transitively (its cam*_thresh_q8_o goes to zhao_geom_lodstate, inside
      R133's parked FORGE.SHADOW subsystem). THAT RULING STANDS AND THIS PACKET
      DID NOT DISSOLVE IT.
  (b) the four contract-refused edge_* neighbour levels -- one NEW declared
      tie-off when the group composes.
  (c) zhao_terrain_devstore's 185 M10K of 553 (33%), whose only reader is
      TERRAIN.LOD.
  (d) the layer-E reader, which R13 puts inside TESS.

TWO TRAPS FOR WHOEVER WIRES THIS, both found by measuring rather than by
name-matching: cam0/1_x/y/z_i are NOT the governor's -- they are zhao_view_eye's
under R63, signed [31:0] to signed [31:0] -- and dual_i is NOT the governor's
either; it is already live in the core as tps_v_flags[TERR_FLAG_DUAL_BIT], the
same net driving u_terrain_compcache.dual_i.

I DID NOT COMPOSE zhao_measure_governor ALONE, and I did not compose
zhao_view_eye, zhao_view_projscale, zhao_view_projq88 or zhao_measure_starve
alone either. Each would be a producer with no consumer -- R75's close-one-gap-
open-another, held by R223/R241.

-- 4. THE THIRD ROTTED CITATION, WHICH NOBODY ASKED ME TO LOOK FOR -----------

The packet named two. Checking their SHAPE against the governor found a third of
the same family, and it is the one that matters most:

zhao_cmd_exec.sv says "NOT pixel_error -- MEASURE.GOVERNOR is not composed", and
this campaign has been reading that as THE FIELD IS NOT AVAILABLE. BOTH FIELDS
THE GOVERNOR IS MISSING ARE IN THE RATIFIED ABI: spec/commands.zidl declares
`fx16 pixel_error` and `u8 view_count`. What is missing is a DECODE ARM IN
CMD.EXEC -- exactly the shape R63 already landed for eye[3] as steps 17/18/19 of
the view walk.

So (a) above is a BUILD, not an absence. The governor's other three inputs each
close by composing a BUILT LEAF ONTO A NET THE CORE ALREADY CARRIES:
zhao_view_projscale -> zhao_view_projq88 for proj0/1_i (both snoop proj_cfg_*_m,
as u_proj_subsystem and zhao_geom_cull already do), and zhao_measure_starve for
starved0/1_i off the composed u_measure_tokens's tok_den_*.

THAT IS THREE ROTTED REFUSALS IN ONE SUBSYSTEM, ALL THE SAME FAILURE: a reason
that outlived its ruling and was re-quoted because nobody re-measured it (R165).
NOT edited in CMD.EXEC here -- that is a different packet's act and
spec/commands.zidl is a live shared file -- but it is NOW NAMED, which is what
stopped it being named for three passes.

-- 5. THE TWO CITATIONS THE PACKET NAMED -- BOTH FIXED -----------------------

  * design/console_inventory.yml, zhao_terrain_devstore, refused on "NO RATIFIED
    COMMAND CARRIES A CAMERA POSITION". R63 LANDED fx16 eye[3] ON THAT EXACT
    COMMAND and zhao_view_eye implements it -- WHICH THE zhao_view_eye ENTRY A
    FEW LINES BELOW IN THE SAME FILE ALREADY SAYS. Corrected, with the LIVE
    reason in its place: composition order and 185 M10K, not the ABI.
  * design/contracts/TERRAIN.TESS.md said "TERRAIN.LOD (does not exist)". It has
    existed since phase 6. Corrected -- and the correction keeps the neighbouring
    clause from collapsing the same way: TERRAIN.PATCH IS composed; what does not
    exist is the composed-height cache BEHIND it.

-- 6. REGISTER, BEFORE AND AFTER (RUN BARE) ----------------------------------

  BEFORE  24   (9 tie-offs + 15 disconnected)   RC 1
  AFTER   25   (9 tie-offs + 16 disconnected)   RC 1

IT GOES UP BY ONE ON PURPOSE. R214: a block with a contract AND silicon owes a
design/blocks.yml row, and this packet registered its own rather than leaving it
for the coordinator, as JOBISSUE's row had to be added for it that morning. The
capability is mandatory -- TERRAIN.LOD cannot decide a level without it -- and A
GAP NOBODY COUNTS IS A GAP NOBODY CLOSES. The register reading honestly UP beats
it reading flatteringly flat.

NO TIE-OFF WAS CREATED AND NONE WAS NEEDED (R159): the block is not composed, so
it adds no boundary. Tie-offs are unchanged at 9.

-- 7. GATES, AND WHY THAT SET ------------------------------------------------

Every gate in the brief, RUN BARE. completion_register RC 1 is normal.

  check_console_inventory   OK   (FAILED FIRST: G4 UNCLASSIFIED on the new
                                  module -- fixed with a disposition, not a
                                  suppression)
  check_prod_manifest       OK   (FAILED FIRST: UNACCOUNTED -- fixed with a row)
  check_quartus17_syntax    RC 0, 581 files, self-test 13 fire / 22 no-fire
  check_case_labels         RC 0
  mutant_copy_drift         RC 0, 59 copies        (run AFTER the commit, R121)
  mutant_drivers            RC 0, 102 of 104
  uncashed_cheques          RC 0
  refmodel_liveness         RC 0
  duplicate_functions       RC 0
  wrapper_port_parity       RC 0, 1280 = 1280, 0 missing, 0 stale
  check_counters            RC 0
  check_findings_citations  RC 0
  completion_register       24 -> 25, RC 1 (normal)
  packet_h_tieoff_audit     RC 0
  verilator --lint-only -Wall on the new block   0 diagnostics
  terrain_spdesc_directed   1,035 / 1,035, BUILT AND RUN (R60), via ctest
  lint_terrain_spdesc       Passed

THE THREE GENERATORS ARE NOT OWED AND I PROVED IT RATHER THAN ASSERTING IT:
gen_prod_top --check FRESH (74 instances), gen_console_board --check FRESH (1285
core ports), gen_shell_paired_diff --check and --check --mutant both FRESH. NO
PORT CHANGED.

THE CONSOLE SMOKE FORMS ARE NOT OWED: no RTL inside zhao_console_core's closure
was touched. The core edit is COMMENT-ONLY -- entry I21, 67 insertions and ZERO
deletions, so no live lane's work can have been swept.

NO QUARTUS WAS RUN. Cost hand-counted in the contract: no multiplier, no memory,
order 250 flops and a couple of hundred ALM. R236 -- recorded, not a veto.

-- 8. LANE OVERLAP -----------------------------------------------------------

NO COLLISION WITH FORMOWN OR WARPBUILD. FORMOWN is kind-8/kind-9 page identity
(geometry, packers, goldens); WARPBUILD is GEOM.WARP and edits
zhao_field_host_v2's parameters inside the core. My only core hunk is a comment
block inside entry I21's ledger prose, INSERT-ONLY. Shared files touched:
tests/CMakeLists.txt (one appended block), design/blocks.yml,
design/prod_manifest.yml, design/console_inventory.yml (one row each),
design/contracts/TERRAIN.TESS.md. `git diff --cached --name-only` was read before
every commit.

-- 9. FOR THE HANDOVER -- THE NEXT PACKET ON I21 -----------------------------

It is now A NAMED SEQUENCE OF BUILDS RATHER THAN AN ARGUMENT, and the first one
is small:

  1. CMD.EXEC DECODE ARMS FOR pixel_error AND view_count. Both fields are
     already in spec/commands.zidl. This is R63's eye[3] shape, and it is the
     only genuinely new ABI-lowering work left in the chain.
  2. COMPOSE FOUR BUILT LEAVES onto nets the core already carries --
     zhao_view_eye, zhao_view_projscale -> zhao_view_projq88,
     zhao_measure_starve. None needs a new core boundary port.
  3. COMPOSE THE GOVERNOR, which R223 item 4 parks transitively. That parking is
     an OWNER decision and should be put to the owner with the sequence above
     attached, because it is the only thing in the chain that is not a build.
  4. THEN THE GROUP: devstore + SPDESC + LOD + JOBISSUE, with the SPDESC splice
     at the tt_lat_*/htp_o_* or htp_c_*/tcc_* seam, one declared tie-off for the
     four edge_* levels, and terr_chk_* owed in the same commit (the core's own
     I27 note: "whoever composes the store owes this check").

AND THE PROCESS FINDING, which is JOBISSUE's with one more data point.

I21 had been re-measured three times and never attempted; one attempt dissolved
the stated blocker and surfaced the real one. THIS PACKET ATTEMPTED THE NEXT
LAYER AND TWO OF THREE BLOCKERS SHRANK AGAIN -- and the third rotted citation was
found by taking the SHAPE of the two the packet handed me and pointing it at a
block nobody had asked about. Re-reading a refusal tells you what it says;
attempting the build tells you what is there. THEY ARE DIFFERENT INSTRUMENTS,
AND THE SECOND ONE HAS NOW BEEN RIGHT FOUR TIMES RUNNING ON THIS ENTRY.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
