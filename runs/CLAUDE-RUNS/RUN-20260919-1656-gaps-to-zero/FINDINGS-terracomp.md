<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
89a9ce32 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- TERRACOMP


## `89a9ce32` -- TERRACOMP FINDINGS: register 25 -> 21. The TERRAIN decision chain is COMPOSED, and the group is what made it possible

```
TERRACOMP FINDINGS: register 25 -> 21. The TERRAIN decision chain is COMPOSED, and the group is what made it possible

THE HARNESS REFUSES A LANE REPORT FILE at
runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-terracomp.md, exactly as
this packet's brief predicted and exactly as it refused gz/jobissue and
gz/terrassem. This commit message IS that document -- the campaign's established
pattern.

=== FINDINGS -- TERRACOMP (entry I21, the TERRAIN subpatch decision chain) =====

2026-09-21. Register 25 -> 21, run BARE, RC 1 (normal). FOUR modules moved from
BUILT BUT NOT CONNECTED into the console. Tie-offs unchanged at 9. Nothing new
was designed: every block in this packet already existed, tested, with a
contract. The register was stuck because they were BUILT and not COMPOSED.

  BEFORE  25   (9 tie-offs + 16 disconnected)   RC 1
  AFTER   21   (9 tie-offs + 12 disconnected)   RC 1
  connected mandatory capabilities   87 -> 91

MOVED FROM DISCONNECTED TO CONNECTED:
  zhao_measure_governor   sets TERRAIN.LOD's six knobs from the ABI and the
                          token stream
  zhao_terrain_spdesc     assembles the sixteen sp_* descriptors per served
                          patch
  zhao_terrain_lod        decides the level, the morph and the neighbour clamp
  zhao_terrain_jobissue   turns the decision into TERRAIN.GROUP_SEQ's job, and
                          retires the patch

ALSO COMPOSED, as the group's own producers, adding no register credit and
leaving no dangling port: zhao_terrain_devstore, zhao_view_eye,
zhao_view_projscale, zhao_view_projq88, zhao_measure_starve. Nine in one commit.

-- 1. WHY THE GROUP AND NOT A LEAF -------------------------------------------

R75 forbids closing one gap by opening another, and R223/R241 hold
zhao_measure_governor BY NAME for exactly that -- so five previous lanes
CORRECTLY refused to wire it. The resolution is not to argue with R75. It is to
compose the whole chain in one edit, so every producer meets its consumer:

  CMD.EXEC.pixel_error/view_count ----\
  VIEW.PROJSCALE -> VIEW.PROJQ88 -----> MEASURE.GOVERNOR --six knobs--\
  MEASURE.TOKENS.den_* -> STARVE ----/                                 |
  VIEW.EYE (cfg 19/20/21, R63) ---------------- cam0/1_x/y/z ----------+
                                                                       v
  TERRAIN.LODFEED -> DEVSTORE -> SPDESC -> TERRAIN.LOD -> JOBISSUE -> GROUP_SEQ

R75 was never the obstacle. The LEAF was.

-- 2. THREE BLOCKERS DISSOLVED, NONE RE-MEASURED -- ALL ATTEMPTED ------------

R237: a blocker that DEFERS work gets believed and re-quoted. All three of I21's
remaining blockers were attempted before any was believed, and all three went.

  * "MEASURE.GOVERNOR's px_err0/1_i and view_count_i have no producer."
    BOTH FIELDS ARE IN THE RATIFIED ABI. spec/commands.zidl declares
    `fx16 pixel_error` on SetView and `u8 view_count` on
    SetPresentationContract, and ZHAO_SET_VIEW_OFF_PIXEL_ERROR = 84 has been in
    the generated package all along. WHAT WAS MISSING WAS A DECODE ARM.
    zhao_cmd_exec.sv's own line "NOT pixel_error -- MEASURE.GOVERNOR is not
    composed" had been read by three passes as "the field does not exist".
    Landed as R63's eye[3] shape, with the budget published at the END of the
    view walk so it rides the same sv_dirty edge as the matrix and the eye.

  * "The devstore costs 185 M10K and that is a blocker."
    THE OWNER GRANTED IT, EXPLICITLY -- ruling R234 D3, 2026-09-21,
    reports/OWNER-RULINGS-20260919-EVENING.md: "GRANT THE 185 FOR THE TERRAIN
    DEVIATION STORE", inside a budget line that prices the console at 541 of 553
    M10K WITH it. FIVE PASSES TREATED A GRANTED COST AS A VETO. This one cost
    nothing to check and had been quoted for two days.

  * "sp_* has no assembler." zhao_terrain_spdesc was built hours earlier by
    TERRASSEM, which also wrote the sequence this packet executed.

-- 3. WHAT STILL BLOCKS THE REST OF I21, named precisely ---------------------

I21 stays open as ONE boundary entry. Its remainder is four signals:

  (a) terr_job_mat_a_i / mat_b_i / weight_i -- ruling R13 rules these the WRONG
      CARRIER and their honest closure is REMOVAL once TESS gains a
      per-triangle layer-E path. zhao_terrain_jobissue has no port for any of
      them. NOT invented here -- that is the hidden-adapter failure I21 exists
      to prevent.
  (b) terr_job_view_mask_i -- wants a producer that DOES NOT EXIST. Nothing
      carries a view mask alongside a page: zhao_terrain_seq emits
      is_view_mask_o at job issue, and HDRREAD, PSMUX and PAGESTREAM forward
      flags:u16, slot, gen, epoch and src_id between them -- and no mask. TWO
      SHORTCUTS WERE ATTEMPTED AND BOTH REFUSED: latching tis_view_mask live
      joins two things that move independently (I21's own objection, still
      correct), and a src_id-keyed side queue DESYNCS PERMANENTLY the first
      time a page is refused after issue -- a bad pitch, a guard denial, a
      short burst -- so its identity check fires once and is wrong forever.
      THE BUILD IS NAMED: one more forwarded field on HDRREAD / PSMUX /
      PAGESTREAM, beside `flags`. It travels through the issuer's draw context
      now, so even with a static source it arrives JOINED to its patch.
  (c) terr_sparse_fill_i -- NOT a gap. An OWNER KNOB by group_seq's own header,
      legal only against a VALID_MODE = 0 shell.
  (d) THE ONE DECLARED TIE-OFF (R159): TERRAIN.LOD's four edge_* neighbour
      levels, driven from 8'h00, declared INSIDE entry I21 so it adds no entry.
      Their producer is a CROSS-PATCH RECONCILIATION PASS no block performs: it
      needs every patch of a frame decided before any is tessellated, and this
      console decides one at a time. THE CONSTANT IS THE SAFE ONE, NOT THE
      CHEAP ONE -- 0 is the FINEST level, so a neighbour read as 0 clamps the
      edge finer than needed: more triangles, NO CRACK. The coarsest constant
      would have seamed the ground AND measured smaller, which is the direction
      a resource-pressed campaign is biased to pick.

-- 4. THE TIE-OFF IS A LITERAL BECAUSE THE AUDIT CANNOT SEE A CONSTANT --------

The first version used `localparam TERR_EDGE_UNKNOWN_C`.
tools/design/packet_h_tieoff_audit.py counts LITERAL connections, so it reported
"10 by group comment, 0 SILENT" with FOUR TIE-OFFS IT COULD NOT SEE -- in the
one commit whose entire subject is not hiding them. A named constant also reads
to a human as a knob somebody CHOSE rather than an absence somebody is OWED. It
now reports 14.

-- 5. THE DEFECT LINT COULD NOT FIND, AND THE ONE NOTHING FOUND --------------

TWO DEFECTS IN MY OWN COMPOSITION. Neither was caught by a gate.

(i) THE DEVSTORE'S SLOT WIDTH. It was first keyed on TERR_MEMSLOT (11 bits --
the pool slot PLUS its refusal bit) instead of TERR_SLOTW (10, the directory
handle). That asks for 2,048 rows: TWICE the granted 185 M10K, for 1,024 pages
that exist. verilator --lint-only returned RC 0. The block's own elaboration
guard fired 198 seconds into the plain console smoke:

    %Fatal: zhao_terrain_devstore.sv:265: SLOTS must be 1 << SLOTW

--LINT-ONLY DOES NOT RUN `initial` BLOCKS. CLAUDE.md says so in as many words
and it was still worth the 198 seconds to be shown it. Repaired by keying on
TERR_SLOTW, with the narrowing ARGUED from the two named zero-extension sites in
the core -- u_terrain_hdrread's .j_slot_i({1'b0, tis_slot}) and
u_terrain_mipfeed's .j_slot_i({1'b0, tmq_j_slot}) -- rather than asserted, plus
an elaboration guard refusing any parameterisation where it would truncate a
real handle. Equality-tolerant, because the slot-overflow mutant elaborates with
TERR_MEMSLOT == TERR_SLOTW.

(ii) THE `dual` FLAG WAS THE WRONG PATCH'S, and this is the one worth reading.
Entry I21 and FINDINGS-terrassem BOTH say dual_i "is already live in this file
as tps_v_flags[TERR_FLAG_DUAL_BIT]". That net is correct where it is already
used -- TERRAIN.PATCH and the compose cache's FILL -- and WRONG for TERRAIN.LOD,
which decides the SERVED patch. With the cache holding one page filling and
another served THOSE ARE DIFFERENT PAGES, so the composition would have decided
each patch with the NEXT patch's underside flag: missing or spurious undersides,
with every counter agreeing.

NOTHING FLAGGED IT. It was found by reading TERRAIN.LOD's contract to answer a
DIFFERENT question -- does the composer owe a hold? -- and noticing that the
contract lists dual_i among the things that must be stable. THE STABILITY
QUESTION IS WHAT EXPOSED THE IDENTITY ONE. A correct-sounding sentence in two
findings documents and one ledger entry is not a measurement.

zhao_terrain_spdesc gains door_dual_i and patch_dual_o. Its door queue ALREADY
carries the {slot, src_id} pair captured at fill acceptance and popped at serve;
the flag is the same fact about the same page on the same beat, one bit per
entry. terrain_spdesc_directed case 2 now serves a DUAL page while a NON-dual
page sits at the door -- the exact arrangement a live net gets wrong -- and
1,035 checks became 1,037.

-- 6. AND THE CONTRACT THE COMPOSER OWED ------------------------------------

design/contracts/TERRAIN.LOD.md: the governor's targets "are **not registered**:
they are sampled as each descriptor is decided. THEY MUST BE HELD STABLE ACROSS
A PATCH JOB." That is a requirement ON THE CALLER and the caller is the
composer. Wiring the governor's outputs straight across was correct-LOOKING and
wrong: the governor republishes on core_tick_c and nothing makes a frame
boundary miss the middle of a patch. Sixteen subpatches decided against two
different budgets is a level mismatch between adjacent subpatches of the SAME
patch -- a crack, at whatever rate the frame edge lands inside a job.

One register, enabled by zhao_terrain_lod's OWN idle_o. No new law is invented:
idle_o is the block's own statement that it is between patches.

-- 7. THE MISTAKE I MADE AND REVERSED, WHICH IS THE PROCESS FINDING ----------

MY FIRST COMMIT DELETED THE THIRTEEN BOUNDARY JOB PORTS. It looked like the
clean thing to do: the fields have an internal producer now, so why keep a way
in?

THE REGISTER MOVED BY EXACTLY THE SAME AMOUNT EITHER WAY. The four gaps that
closed are MODULES becoming connected; A PORT IS NOT A MODULE. What deleting
them actually cost was EVIDENCE. tb_zhao_console_core_smoke.sv injected one
subpatch job there, and that injection was the only thing in the console smoke
that made TERRAIN.TESS run. SIX assertions stand on it. All six went dead:

    SMOKE: TERRAIN.TESS emitted no window vertex -- GROUP_SEQ -> TESS job port
           is dead          (tess_vertices=0 tess_refs=0 b_grants=0)

The internal producer CANNOT reach them in that bench, and the reason is
upstream of everything this packet built: every terrain page the smoke plays
FAILS ITS CRC, so no page becomes resident, TERRAIN.SEQ issues no compose job,
the cache never fills, never serves, and never opens the door SPDESC and
JOBISSUE wait at.

THE TEMPTING MOVE WAS TO RELAX THE SIX ASSERTIONS AND CALL THE COMPOSITION
CLEAN. That is removing coverage to make a number look better -- and the number
was not even affected. So the port is back as a DECLARED OVERRIDE, on the
pattern this file already uses for the host's proj_cfg_*_i: the boundary wins
the cycle, the issuer drives every cycle it does not. A 13-field 2:1 mux, order
30 ALM. terr_cc_serve_release_i is OR-ed rather than muxed, because it is a
PULSE and the two producers describe different patches.

With it back, the same bench reports:
    terrain   tess_vertices=81 tess_refs=128 fills_forwarded=81 b_grants=81
    projector a_grants=78 b_grants=81 replay_triangles=128
    terrlight refs_taken=128 lights=128 shaded=128 normals=128

THE REAL HOLE -- the smoke's terrain pages not loading -- is now NAMED in entry
I21 instead of being papered over by the injection.

  THE GENERAL FORM: ASK WHAT THE NUMBER WOULD HAVE DONE WITHOUT THE DELETION.
  A composition packet is under pressure to make a boundary disappear, and a
  boundary that disappears LOOKS like progress whether or not it is. The
  register counts capabilities, not ports.

-- 8. A NEW COUNTER, AND ITS PLANT IS PROVEN (R95) --------------------------

view_count_refused_o on CMD.EXEC. video_rules.md 3.1 ratifies two views, so 1
and 2 are the lawful bytes; CMD.DMA's Phase-2 structural walk deliberately omits
the decoder's BAD_VALUE step, so an unlawful byte CAN arrive. CMD.SCHEDULER
judges that record's `mode` and NOT this field, so without this counter the
verdict would have had no owner.

tests/command/cmd_exec_directed.cpp cases 50 / 50b, all fired with LEGAL
stimulus -- no committed mutant owed:
  * FIRES on view_count = 7, and separately on 0 (a decode that refused "too
    big" and accepted "none" would pass the first check and present nothing);
  * NEGATIVE CONTROL in the same case: a lawful 2 is adopted, counter flat;
  * AND IT HOLDS -- the unlawful byte is not adopted. A counter that fired while
    the bad value was adopted anyway is a reassuring instrument on a broken
    decode;
  * the two views' pixel_error budgets are asserted PER VIEW and DIFFERENT,
    which is what a swap or a single shared register would break.

THE PLANT IS PROVEN, NOT ASSERTED. "977 checks passed" says nothing about
whether case50b RAN. A deliberate inversion -- assert the counter reads 99 --
was built and run: it FAILED with "expected 0x63, got 0x1". Got ONE: the check
is reachable AND the counter really reaches 1 on the unlawful byte. Reverted,
rebuilt, 977 pass.

-- 9. GATES, ALL RUN BARE ---------------------------------------------------

  check_console_inventory   RC 0   226 elaborated, 232 sources, latest wired
  check_prod_manifest       RC 0   368 modules, each counted once
  check_quartus17_syntax    RC 0   583 files
  check_case_labels         RC 0
  mutant_copy_drift         RC 0   run AFTER the commit (R121)
  mutant_drivers            RC 0
  uncashed_cheques          RC 0
  refmodel_liveness         RC 0
  duplicate_functions       RC 0
  wrapper_port_parity       RC 0   1324 = 1324, 0 missing, 0 stale
  check_counters            RC 0
  check_findings_citations  RC 0
  packet_h_tieoff_audit     RC 0   8 declared, 1 reasoned, 14 by group, 0 SILENT
  completion_register       25 -> 21, RC 1 (normal)
  cmd_exec_directed         977 / 977, BUILT AND RUN (R60), plant proven
  terrain_spdesc_directed   1,037 / 1,037, BUILT AND RUN
  lint zhao_terrain_spdesc  RC 0, 0 diagnostics
  console core LintOnly     RC 0, 24 s
  terrain_jobissue_directed 138 / 138, BUILT AND RUN

THE NINE CONSOLE SMOKE FORMS, derived from the script's own param() block and
run on ONE frozen tree.  Seven at rc=0 fatals=0; NoTableLoad and BadDescriptor
at rc=0 fatals=1, which is their INVERTED pass.  Every duration 248-274 s -- a
form returning in about a second is a verilation failure, and none did.

  plain        rc=0 fatals=0 secs=269     NoTableLoad   rc=0 fatals=1 secs=261
  Mutant       rc=0 fatals=0 secs=274     BadDescriptor rc=0 fatals=1 secs=262
  UntexMutant  rc=0 fatals=0 secs=274     BadVertex     rc=0 fatals=0 secs=253
  NoEchoArm    rc=0 fatals=0 secs=248     BadTraceArm   rc=0 fatals=0 secs=257
  GlowTag      rc=0 fatals=0 secs=260

THE SWEEP WAS RUN THREE TIMES AND THE FIRST TWO WERE DISCARDED, which is worth
recording because discarding them was the right call both times. Run 1 caught
the devstore SLOTS guard. Run 2 was started and then the tree moved underneath
it -- CLAUDE.md: a suite whose inputs moved is not evidence in EITHER
direction -- so it was killed, its processes verified gone, and the tree frozen
before run 3. A tenth run of the plain form AFTER the final comment-only commit
returned rc=0 fatals=0 in 263 s with identical terrain figures.

AND A NOTE ON READING THEM: packet_h_tieoff_audit returned RC -1 through a
PowerShell pipeline and RC 0 bare, because Select-Object closed the pipe. That
is the brief's own warning, met once.

THE GENERATORS: zhao_prod_top.sv (74 instances) and zhao_console_board.sv
(1,325 core ports) regenerated and READ; gen_shell_paired_diff --check and
--check --mutant both FRESH. Both zhao_console_core wrappers under
tests/mutants/ updated by hand.

NO QUARTUS -- the owner: "We're not fitting now." Cost hand-counted: 185 M10K
(GRANTED, R234 D3) plus order 2,500 ALM and NO new multiplier. R236: recorded,
never a veto.

-- 10. FOR THE NEXT PACKET --------------------------------------------------

  1. THE VIEW MASK'S CARRIER -- one forwarded field on HDRREAD / PSMUX /
     PAGESTREAM beside `flags`. That closes I21 down to R13's three ruled
     removals and nothing else.
  2. zhao_geom_lodstate / FORGE.SHADOW -- the governor's cam*_thresh_q8_o and
     deg0/1_o are the ONLY outputs in this composition with no reader, and
     R234 D2 has already commissioned that subsystem. R223 item 4 parked the
     governor transitively ON THAT ONE PORT; the rest of the block has a
     consumer now.
  3. THE SMOKE'S TERRAIN CRC FAILURE is now the thing standing between this
     chain and any console-level evidence about it. It was always there; the
     injected job was hiding it, and the injection is now declared as the
     workaround it is rather than read as coverage.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
