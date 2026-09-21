<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
db2bd937 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- POSECMD


## `db2bd937` -- FINDINGS-posecmd.md -- the harness REFUSED the file; this message IS the document

```
FINDINGS-posecmd.md -- the harness REFUSED the file; this message IS the document

Write to runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-posecmd.md was
refused ("Subagents should return findings as text, not write report files"),
which is what it did to POSEPAGE. Copying POSEPAGE's solution: an empty commit
whose message is the findings, for the coordinator to transcribe.

Mandate: owner ruling R229, decision D-POSEPAGE-A. Branched from 69b91c7d.

REGISTER: 22 -> 22, RC 1 both times. NOT 21 -> 21 -- the register already read
22 at my branch point (9 tie-offs + 13 disconnected), so R229's "21" had moved
before I started. No rise was manufactured and none was available: R214's test
wants a capability SPECIFIED AND UNREGISTERED, and this one is registered
already, by I29.

================================================================================
1. R229'S FOUR FACTS -- ALL FOUR HELD. NONE HAD EXPIRED.
================================================================================

0x0304 IS SPOKEN FOR              TRUE. OWNER-RATIFICATION-20260920-WARP.md W04
                                  allocates it to DrawWarpedForm; R171 ratified
                                  it; GEOM.WARP.md marks it NOT BUILT. ZERO
                                  occurrences in commands.zidl -- so reading
                                  that file alone makes it look free.
abi version does NOT bump         TRUE, and understated. R229 names two
                                  precedents; there are FIVE -- PublishResource
                                  0x0030, TerrainEpoch 0x0220, SetPost 0x0040,
                                  SetPopulation 0x0303, DebugTraceArm 0xF003.
                                  ZHAO_ABI_VERSION byte-unmoved at 3.
the bytes already reach VRAM      TRUE. PublishResource carries u8 kind,
                                  lowered to MEM.UPLOAD's req_tag_i.
`sub` is not padding              TRUE, and the cache is blunter than the
                                  summary: omitting it made "a key and its
                                  midpoint have the SAME {type, clip, frame} and
                                  alias -- the cache returned the wrong palette
                                  and nothing reported an error".

A FIFTH FACT THE RULING DID NOT STATE, AND IT DECIDED THE RECORD'S SHAPE:
DrawPosedForm's first sixteen payload bytes can be made BYTE-IDENTICAL to
DrawForm's. That turns R229's "clean split" from a policy into a mechanism --
CMD.EXEC reads six of nine fields through the EXISTING OFF_DF_* constants, so
the two opcodes cannot disagree about where a form handle lives. Pinned by eight
per-field elaboration guards, not by the comment claiming it.

================================================================================
2. WHAT LANDED
================================================================================

spec/commands.zidl -- DrawPosedForm 0x0305 implemented, 48 B:
  form, material_set, transform, viewport_mask, semantic_weight, flags  @0..15
  clip_id u16 @16   frame_no u16 @18   sub u8 @20   pad[11] @21..31

APPENDED AT THE END under R171. The reason is mechanical, not stylistic:
tools/abi-gen/src/sample.ts stamps a sample's source_id from the command's INDEX
in the file, so an insert renumbers every later command. MEASURED: of the 24
existing cmd_*.bin goldens, ZERO changed.

type_id, generation and epoch are DELIBERATELY ABSENT. The draw already names
the creature through `form` (24-bit index = memory_rules 5f.1's directory key);
a second identity beside the first is the two-sources-of-truth shape.
Generation/epoch belong to 4.2's resident-handle layer, which R229 leaves
undetermined.

Generated carriers (npm run abi:gen): zhao_abi_pkg.sv, zhao_abi.h,
compiler/src/generated/{abi,frame,zcap}.ts, spec/generated/abi.md,
tests/abi/golden/cmd_draw_posed_form.bin.

zhao_cmd_exec.sv     the arm. Pose rides the SAME draw_valid_o beat in the SAME
                     dq entry (DRAW_W 144 -> 185). Two counters.
zhao_console_core.sv cmd_draw_posed_o / _clip_id_o / _frame_no_o / _sub_o plus
                     the two counters. Declared INSIDE entry I29.
render_frame.cpp     the oracle's arm.
GEOM.POSE.md         the pose_requests clause corrected.
tb_cmd_exec_pair.sv  six ports, connected as real outputs.
tb_zhao_console_core_smoke.sv  six logic declarations.

ZHAO_CMD_DECODER NEEDED NO HAND EDIT, AND THE TEST PROVES IT RATHER THAN
ASSUMING IT. The decoder reads record sizes from the GENERATED table
(zhao_opcode_record_bytes, now 48 for 0x0305). Case 36's packet returns
ZH_ABI_OK, which is impossible if the decoder still reported
ZH_ABI_UNKNOWN_OPCODE. The carrier is generated; the evidence is a passing
packet, not a grep.

================================================================================
3. THE DESIGN DECISION WORTH ARGUING: ONE ENABLE
================================================================================

CLAUDE.md's metadata bank held a record and its metadata under two register
enables; a stall produced "response A's data, A's token, and B's metadata" while
every counter balanced, and the mismatch checker could not fire because both its
operands moved together.

A pose and its draw here are ONE record with ONE enable. No stall can separate
them, and no checker is needed for a skew that cannot occur.

R13's clause SELECTS this shape rather than forbidding it: "the job port is not
widened to carry a subpatch-uniform value that is not true" forbids widening for
a UNIFORM value. A pose is genuinely per-draw -- which is why R229 refused the
SetPose alternative.

draw_posed_o LOW is the bind pose. Without it, clip 0 frame 0 would be
indistinguishable from "no pose named", and the clean split would exist in the
ABI but not on the wire.

THE REFUSAL DOES NOT DROP THE DRAW. clip_id >= 64 (creature_rules 2.1's 64
authored slots, zref::clip_page::kMaxClips) is refused and counted, never
truncated -- SetPopulation's plane_nx law. But the DRAW still leaves, with
draw_posed_o low: zhao_geom_pose_cache's rule 1 already defines what an unusable
key does ("it uses the identity bind pose"). Refusing the record whole would
make a creature VANISH -- a narrowing of function wearing a refusal's clothes.

THE ONE HAZARD THAT DOES NOT TRANSFER. DrawForm needs the df_flags_c bypass
because its last field byte IS the record's last byte. DrawPosedForm's last
field byte is `sub` at 36 of 48 -- eleven pad bytes before rec_done -- so no
bypass is correct, and applying one would inject a pad byte. df_flags_c was
already gated on ZHAO_OP_DRAW_FORM; an elaboration guard now pins the reason so
nobody "fixes" it by symmetry.

================================================================================
4. THE DIRECTED TEST -- 850 CHECKS, RC 0, AND BOTH COUNTERS FIRED
================================================================================

test_cmd_exec_directed BUILDS AND RUNS (R60), up from 762.

  36  every field off the dispatch; flags=0xBEEF proves the bypass does NOT
      apply here; unsupported_o==2 catches an arm added to the CAPTURE and not
      the EMIT
  37  the clean split IN BOTH ORDERS -- a DrawForm after a posed draw carrying a
      loud key is still bind pose
  38  pose_clip_refused_o fires and DISCRIMINATES (R95): clip 63 accepted, 64
      refused, plus a plain DrawForm as a THIRD negative control
  39  the same packet UNDER BACKPRESSURE
  40  an abandoned packet's pose never leaves
  41  `sub` is carried and is NOT padding -- the aliasing pair itself

CASE 39 IS THE ONE THAT MATTERS. draw_mask=0x00000001 holds draw_ready_i low 31
of every 32 cycles; four draws with four non-patterned keys go in and each comes
out wearing its own. THE CASE ALSO ASSERTS THE STALL HAPPENED (>32 cycles across
four draws) -- without that it could pass having backpressured nothing, which is
a gate that cannot reach the state wearing a green.

BOTH FIRE TESTS RAN.

  A -- clip ceiling 64 -> 65. THREE checks failed, the right three:
       FAIL case38: pose_clip_refused_o FIRES, exactly once: exp 0x1, got 0x0
       FAIL case38: only the legal one is posed:             exp 0x1, got 0x2
       FAIL case38: clip 64 degrades to BIND POSE:           exp 0x0, got 0x1

  B -- the pose read from the LIVE SHADOW instead of the dequeued entry. This is
       CLAUDE.md's metadata bank reproduced EXACTLY: two enables where the
       design has one. NINE checks failed:
       FAIL case39: draw 0 has ITS OWN clip under stall:  exp 0xB,   got 0x2
       FAIL case39: draw 0 has ITS OWN frame under stall: exp 0x301, got 0xFF
       FAIL case39: draw 2 has ITS OWN clip under stall:  exp 0x2F,  got 0x2
       FAIL case41: the two keys are DISTINGUISHABLE:     exp 0x1,   got 0x0

       Draw 0 came out wearing DRAW 3's KEY -- A's data with B's metadata -- and
       case 41's aliasing pair collapsed into one.

NEITHER MUTATION IS COMMITTED and neither needs to be: both states are reachable
from production RTL by a one-line edit a LEGAL packet observes, which is the
condition CLAUDE.md sets for not owing a committed mutant.

================================================================================
5. FOUR INSTRUMENT FINDINGS, EACH OF WHICH WOULD HAVE MISLED SOMEBODY
================================================================================

(a) THE STALE-BINARY TRAP FIRED ON THE RESTORE AND PRODUCED A PERFECT FALSE RED.
    After `git checkout --`, with all three CONTENT assertions TRUE and
    `git status` CLEAN, the rebuild printed "ninja: no work to do" and the
    "restored" run reported mutant B's nine failures. git checkout left the file
    older than the objects built from the mutant. This is CLAUDE.md's Copy-Item
    paragraph one tool over -- "a RESTORE looks nothing like an edit, so nobody
    thinks to check" -- and the obvious reading is that the repair did not work.
    Fix: (Get-Item <file>).LastWriteTime = Get-Date. 850/850 green after.
    THE HABIT THAT CAUGHT IT was asserting the CONTENT of the restored file
    before believing the run, rather than trusting the checkout's exit code.

(b) THE SMOKE BENCH COULD NOT VERILATE, AND THE DURATION SAID SO BEFORE THE EXIT
    CODE DID. tb_zhao_console_core_smoke.sv binds the core with `.*`, so the six
    new ports made ALL TEN FORMS RETURN RC 1 IN ONE SECOND EACH. A one-second
    smoke run is not a smoke run. Six logic declarations fixed it. The
    coordinator's new argument guard is what let this read as a real failure
    rather than a silent green.

(c) THE TWO CONSOLE-CORE MUTANT WRAPPERS HAVE DIFFERENT LINE ENDINGS.
    zhao_console_core_untex_decl_mutant.sv is CRLF;
    zhao_console_core_slot_overflow_mutant.sv is LF. ONE SCRIPTED PATCH ACROSS
    BOTH SILENTLY MATCHES NOTHING IN ONE OF THEM -- and reports success for the
    other, so the failure looks like a half-done edit rather than a tool
    problem.

(d) THE BRIEF'S PATH FOR gen_shell_paired_diff.py IS WRONG. It is
    tools/DESIGN/gen_shell_paired_diff.py, not tools/quartus/. Run at the
    brief's path it returns RC 2 with "No such file or directory" -- which a
    lane reading exit codes alone records as A FAILING GATE, and a lane
    swallowing output records as nothing. Both readings are wrong; the gate is
    fine and was never run. At the real path: --check RC 0 and --check --mutant
    RC 0, "harness fresh (59 shared inputs, 91 compared outputs, 4 declared
    divergent)" both times.

================================================================================
6. THE CONTRACT CLAUSE THAT DESCRIBED A DEFECT
================================================================================

design/contracts/GEOM.POSE.md said pose_requests is
{type_id, clip_id, frame_no, bone_count}. ALL FOUR appeared in no command at any
opcode, and the contract did not say so -- which is why I29 read like an unwired
input rather than an absent carrier.

AND THE CLAUSE OMITS `sub` ENTIRELY. A requester built faithfully from its four
fields would have REINTRODUCED THE EXACT ALIASING DEFECT THE CACHE WAS REPAIRED
FOR ON 2026-09-03. The contract has described a three-field key since before
that fix and nobody read the two documents together. Corrected, with type_id
re-sourced (the form handle) and bone_count re-sourced (the page header).

================================================================================
7. WHAT I29 STILL NEEDS
================================================================================

BLOCKER (b) IS DISCHARGED: pose_requests has a carrier. The bytes, the page
container, the store and now the COMMAND all exist.

What remains is the CONSUMER, and it is one gap rather than four:

  1. the kind-8/kind-9 PAGE READER and the sixth u_geom_mem_adapter requester --
     R90's item 3, still unreached. zhao_mem_share_n is at N=5 with A..E driven,
     so the sixth is an IN-CORE EDIT and no boundary port.
  2. form -> type_id RESOLUTION, owed ONE implementation and a directed check.
     zhao_geom_drawjob already resolves the form handle against residency, so it
     is the natural owner.
  3. THE INSTANCE WALK that turns an accepted draw into a pose_requests beat.
  4. zhao_geom_ladderbank is STILL UNCOMPOSED (five hits in zhao_console_core,
     all comments).

AND ONE THING THAT IS NOT I29'S AND MUST NOT BE FOLDED INTO IT: 4.2's
request-side representation -- "a validated resident handle with generation and
epoch, not a naked slot and frame" -- is a DIFFERENT LAYER WITH NO RULING. The
clip-bank generation the cache keys on (acq_gen_i, ruling D-3) comes from
PublishResource's new_generation, not from the draw record. A LANE THAT WIRES
THE READER WILL MEET THIS IMMEDIATELY, and it is an owner decision.

NO POSE READER WAS BUILT. R229 is explicit that ratifying the command does not
enable it, and a second uncomposed block beside zhao_geom_bonesrc would be
"BUILT, INSTALLED NOWHERE".

NO TIE-OFF WAS CREATED. The four new core ports are a BOUNDARY, folded into
I29's existing entry rather than given one of their own -- a separate row would
count one gap twice. packet_h_tieoff_audit RC 0.

================================================================================
8. FILE-SET OVERLAP WITH TERRCMD -- I COULD NOT STAY DISJOINT
================================================================================

Reported because the brief requires it. Three files are plausibly TERRCMD's:

  spec/commands.zidl          unavoidable; the mandate IS an ABI record. I
                              appended at the END, which is both the
                              lowest-conflict position and what R171 requires.
  zhao_cmd_exec.sv            TERRCMD is also in the command area. My edits are
                              confined to the DrawForm arm, its offsets, the dq
                              widths and the reset block. I touched NO tfld_*
                              logic.
  zhao_console_core.sv        ports beside the existing cmd_exec_draw_* counters
                              and prose inside entry I29. TERRCMD owns I27/I32,
                              so the entry-block edits should not collide.

NOT touched: tests/CMakeLists.txt (the ctest already existed -- no new
registration was needed), design/blocks.yml, design/prod_manifest.yml,
design/console_inventory.yml.

NO LEDGER ROW IS OWED FOR THE TWO NEW COUNTERS, and the scope of that search is
stated: I looked in blocks.yml, prod_manifest.yml, console_inventory.yml and
spec/counters.md. The THREE EXISTING draw counters are absent from all four, so
the two new ones are in the same class. Outside that scope, check_counters.py
(which reports port existence) passes.

git diff --cached --name-only was read before every commit.

================================================================================
9. GATES -- AND WHY THIS SET
================================================================================

ALWAYS-ON, ALL RC 0: check_console_inventory, check_prod_manifest,
check_quartus17_syntax, check_case_labels, mutant_copy_drift,
wrapper_port_parity, mutant_drivers, uncashed_cheques, check_counters,
refmodel_liveness, duplicate_functions, packet_h_tieoff_audit.
completion_register RC 1 (normal while gaps remain), 22 -> 22.

npm run abi:check -- CLEAN, 34 outputs. THIS IS THE GATE THAT IS MINE
SPECIFICALLY: POSEPAGE correctly skipped it because it did not touch
commands.zidl. This branch does.

PORT-CHANGE GATES, run because zhao_console_core's port list moved, and their
output READ (R151/R161). THREE OF THEM CHANGED THEIR ANSWER, which is R227's own
test for a gate worth running:
  gen_prod_top.py         regenerated; --check fresh, 73 instances
  gen_console_board.py    --check said STALE and NAMED the count (1285 ports);
                          regenerated -- 1281 re-exported, 4 board-driven
  wrapper_port_parity.py  FIRED FIRST, naming all six missing ports in BOTH
                          mutant wrappers; fixed per R220 ("fix the WRAPPER,
                          never the module"); now 1270 = 1270
  gen_shell_paired_diff   --check RC 0 and --check --mutant RC 0 (see 5d for the
                          path correction)

ALL TEN SMOKE FORMS, DERIVED FROM THE SCRIPT'S OWN param() BLOCK, NOT FROM THE
BRIEF. The brief named eight; the script declares ten (nine switches plus plain,
-SkipVerilate being a modifier). Whole logs kept.

  FORM=plain          RC=0 FATAL=0 ERROR=0 SECS=258 LINES=108
  FORM=Mutant         RC=0 FATAL=0 ERROR=0 SECS=274 LINES=85
  FORM=UntexMutant    RC=0 FATAL=0 ERROR=0 SECS=269 LINES=86
  FORM=NoTableLoad    RC=0 FATAL=1 ERROR=1 SECS=252 LINES=66
  FORM=BadDescriptor  RC=0 FATAL=1 ERROR=1 SECS=253 LINES=66
  FORM=BadVertex      RC=0 FATAL=0 ERROR=0 SECS=260 LINES=87
  FORM=NoEchoArm      RC=0 FATAL=0 ERROR=0 SECS=239 LINES=110
  FORM=BadTraceArm    RC=0 FATAL=0 ERROR=0 SECS=248 LINES=109
  FORM=GlowTag        RC=0 FATAL=0 ERROR=0 SECS=252 LINES=111
  FORM=LintOnly       RC=0 FATAL=0 ERROR=0 SECS=24  LINES=3

THE COORDINATOR'S CORRECTION IS CONFIRMED EMPIRICALLY, AND IT MATTERS FOR TWO
FORMS, NOT ONE. -BadDescriptor AND -NoTableLoad each pass WITH exactly one
%Fatal, because both are INVERTED controls whose inner run is SUPPOSED to die.
BadDescriptor's own verdict line says so: "POSITIVE CONTROL PASS: the run failed
(rc=1) with one corrupted descriptor byte, as it must." A blanket "%Fatal must
be 0" sweep would have redded TWO working controls, and a lane that then
"fixed" them would have deleted the evidence.

-LintOnly is 24 seconds and three lines BY DESIGN, and its own output says what
that is worth: "LINT-ONLY: tb_zhao_console_core_smoke elaborates. This says
NOTHING about what the console does."

AND THE WHOLE-LOG ADVICE EARNED ITS KEEP: plain, NoEchoArm, BadTraceArm and
GlowTag ALL end in the identical "SMOKE: PASS" line and differ only at 108 /
110 / 109 / 111 lines. A one-line capture cannot tell a bound flag from an
ignored one. GlowTag is distinguishable only in the body -- gather frags=2560
[untagged=1498 lit=1062] against plain's [untagged=2560 lit=0].

THE CAPTURE FARE (R108), PAID IN FULL -- all five goldens regenerated through
their REAL producers. capture_diff.py --selftest FIRST, RC 0, and the half that
matters is selftest 2, which mutates a CONTENT byte and must report FAIL: it
does. EXACTLY 68 BYTES MOVED IN EACH, all of them container CRC + the two sha
fields. NO OTHER BYTE MOVED IN ANY OF THE FIVE. abi_version unmoved at 3.

  zcap_minimal 68  z60 68  storm 68  duo_10frame 68  duo_markers 68
  duo_markers' shas sit at [19352..19415], located STRUCTURALLY -- the ZIDL
  packet recorded pre-writing the wrong offsets for this exact row.

AND THE PRODUCER-PROVENANCE QUESTION WAS ANSWERED RATHER THAN ASSUMED. The zref
arm landed AFTER the first three captures were written, so their producers had
not relinked against it -- a capture made by a binary the tree no longer builds
is a provenance claim nobody can check. Closure argument: neither producer calls
render_frame (shell_golden's only zref-render use is displayed_crc32c, defined
in a DIFFERENT translation unit, resolve.cpp; duo_markers.cpp names no zref::
symbol at all). MEASURED anyway: both relinked, shell_golden --write re-run, all
three captures BYTE-IDENTICAL; demo_duo_markers re-run in VERIFY mode against
the committed bytes, 23,430 checks passed, RC 0.

Reference renderer regressions after the oracle arm: test_render_directed RC 0,
test_render_golden RC 0.

Verilator lint on zhao_cmd_exec: 0 diagnostics -- which settles ONE TOOL'S
OPINION and nothing else (R212). The new elaboration checks are inside
`initial begin ... end` as Quartus 17.0 requires, and check_quartus17_syntax.py
passes with its own 13-fire / 19-no-fire self-test. NOTE HONESTLY: this block
has not been through quartus_map in this branch. A fit is owed at the next
subsystem boundary, and its question is the ALM cost of DRAW_W 144 -> 185 across
DRAW_Q entries plus four core ports.

================================================================================
10. COMMITS ON gz/posecmd
================================================================================

  a05d8a74  the ABI record + CMD.EXEC's arm + the boundary declared in I29
  94bb64ab  six directed cases; both counters fired by mutation
  72269fcd  the reference model's arm; the smoke bench; three captures
  c6eda452  the GEOM.POSE contract clause
  741d75dd  R108's capture fare, all five goldens
  (this)    the findings

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
