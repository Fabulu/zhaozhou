<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
3086be18 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- POSEPAGE


## `3086be18` -- POSEPAGE: FINDINGS (the harness refused the file; this commit is the document)

```
POSEPAGE: FINDINGS (the harness refused the file; this commit is the document)

`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-posepage.md` was
refused by the harness -- "Subagents should return findings as text, not write
report files" -- exactly as the brief predicted. Per the brief the findings go
in the commit messages and the coordinator transcribes. This is the
consolidated document; `3a4d2a7a`, `77a7e04e` and `11bb777e` carry the detail.

ENTRY I29. REGISTER 21 -> 21. One blocker removed, one found, one owner
decision filed. No RTL, no port change, no composition.

===========================================================================
HEADLINE
===========================================================================
The entry's own plan -- "build the page reader and the sixth requester" --
would have given I29 REAL BYTES and still no statement of WHICH FRAME.

Two blockers sit under I29 that the entry does not record. One was removable
and is removed. The other is the whole of what remains, and the owner named it
EIGHTEEN DAYS BEFORE THE ENTRY WAS WRITTEN, in a ratified document the entry
has never cited.

===========================================================================
1. BOTH INHERITED CLAIMS HELD
===========================================================================
THE STRUCK DECISION HOLDS. DOSSIERCHECK was right that "THE CREATURE ABI
FREEZE (I29) -- author the bytes" is R90 restated and already live. Verified:
ladder_page_body_v1.bin present at 448 bytes, `zref::creature_page::body` at
zref_creature_page.hpp:272, `mkcreatureladder.py --check` green. Not re-opened.

THE MUTANT`S SENTENCE IS THE REAL PRODUCER AND IT IS EXACTLY RIGHT.
`zhao_geom_bonesrc_latefetch_mutant.sv` says I29 wants "a kind-8/kind-9 page
read through MEM.GUARD to SDRAM, and a page read that misses can easily exceed
115 cycles". I29's own text says the same thing in different words. The clause
about 115 cycles is a DESIGN CONSTRAINT ON THE READER that the entry did not
carry; it is recorded into the entry now.

===========================================================================
2. EVERY RECORDED BLOCKER RE-MEASURED -- AND NOTHING HAD ROTTED
===========================================================================
Fifteen lanes have corrected an inherited citation. This one could not, and
saying so is the result.

  kind-8 bone-hierarchy freeze ... RETIRED as recorded.
  "no behavioural SDRAM model" ... RETIRED as recorded, and the record is
    EXACT: sim/models/zhao_sdram_model.sv is 219 lines, POKE at :56,
    instantiated FOUR times -- tb_zhao_shell.sv:1527, tb_zhao_mem_chain.sv:158,
    tb_zhao_mem_guard.sv:139 and tb_zhao_console_core_smoke.sv:3482 as
    `u_geom_sdram`. So a composed reader has a bench that already holds real
    memory behind the real core.
  the ~17.6 kbit unpriced store ... RETIRED. zhao_geom_bonesrc.sv built,
    SRC_STYLE-parameterised, three map rows, 830 ALM against 14,056.
  the producer ... STANDS. u_geom_mem_adapter is zhao_mem_share_n at N=5 with
    A..E all driven, so a sixth is an IN-CORE edit and no boundary port.
    zhao_geom_ladderbank still uncomposed: five hits in the core, all comments.

AND THE CORRECTION I NEARLY FILED AND DID NOT.
My first measurement of the SDRAM model returned 198 lines against the entry's
219 and I was one keystroke from writing "the citation has rotted". The
instrument was PowerShell `Measure-Object -Line`, which is not a line count.
The correct number is 219, exactly.

R186's packet named this incentive inside its own retraction: "my error ran in
the direction that made the gap look bigger and my own finding look more
important." That is the direction this one ran. A RE-MEASUREMENT LANE`S
FLATTERING DIRECTION IS FINDING ROT, NOT MISSING IT -- the mirror image of the
broken-instrument law, and worth adding to the habit.

===========================================================================
3. BLOCKER (a), FOUND AND REMOVED -- the kind-9 PAGE was never frozen, only
   the FRAME
===========================================================================
zref_creature_page.hpp says, above its `body` namespace: "A kind-9 frame reader
therefore has a layout to read and needs no lift; what it needs is a producer."

TRUE OF A FRAME, FALSE OF A PAGE.
  * creature_rules 2.1 IS headed "Storage (frozen; the Q formats are frozen --
    qformats 7.6, C1)" and freezes what one frame CONTAINS. That half stands.
  * creature_rules 5 sketches the PAGE in one clause -- "clip directory
    {slot_id u16, frame_count u16, event_count u16} + frames + event tags" --
    under a heading reading "Cartridge pages (additive; LAYOUTS FREEZE WITH
    SW.TOOLS.ASSET AT PHASE 12 ENTRY)". No magic, no version, no field order,
    no offsets, no alignment, no statement of where frame f of clip c begins.

A READER CANNOT READ A FROZEN FRAME IT CANNOT LOCATE. The container was exactly
the guessed layout cartridge.md 4's deterministic-refusal sentence forbids
inventing.

AUTHORED UNDER R90, whose recommendation item 1 asks for it in terms:
"author/freeze the body section AND A MINIMAL KIND-9 FRAME WITH A ZREF MODEL".
POSEABI did the body half; the kind-9 half was not reached. Landed in 3a4d2a7a:
  reference/include/zref/zref_clip_page.hpp
  tools/pack/mkclipbank.py  (--check, --write-golden)
  tests/golden/creature_clip/clip_page_v1.bin  (704 bytes)
  tests/geometry/clip_page_directed.cpp + mkclipbank_check, two ctests

THE FRAME`S BYTES ARE NOT MOVED. 2.1's "<= 268 B/frame at 32 bones" stays
exactly true of the contents; 320 is what one frame OCCUPIES on the 64-byte
grid. The 52 spare bytes of the frame header buy two properties the silicon
needs, both asserted by the test: a bone's quat16 is ONE 8-byte-aligned 64-bit
word (packed straight after a 12-byte root, every quaternion in the page would
straddle two beats and zhao_geom_bonesrc's fill port would need a rotate for
nothing), and a frame begins on a line boundary so "frame f" is a multiply.

THE GOLDEN IS geom_bonesrc_directed`S OWN FIXTURE, DELIBERATELY -- the same
six-bone skeleton, frame 0's rotations that file's `bone_quat(b)`. The two
goldens describe ONE creature, so a reader can be differenced against a bench
that exists. The root walks per frame and every lane is offset by the frame
number, so A READER THAT SERVES FRAME 0 FOR EVERY REQUEST IS CAUGHT BY THE
NUMBERS, not by a counter.

BOTH GATES WERE SEEN TO FIRE. Byte 200 of the golden incremented:
clip_page_directed went 44/44 -> 2 FAILED naming "first difference at byte 200:
D4 vs D5"; mkclipbank.py --check went RC 0 -> RC 1 naming the same byte.
Restored; both green. 16 decode verdicts are reached ONE CORRUPTED FIELD AT A
TIME; 10 packer refusals fire; the frame address chain is differenced against a
BYTE-LEVEL SEARCH for each frame's own root displacement rather than against
itself.

The stale sentence in zref_creature_page.hpp is CORRECTED IN PLACE, not deleted
-- a refusal outliving its own cause in one file is what this very entry
complains about, twice.

===========================================================================
4. BLOCKER (b) -- `pose_requests` HAS NO CARRIER. This is the whole gap.
===========================================================================
  * design/contracts/GEOM.POSE.md names the input: "pose_requests (from
    GEOM.MESHFETCH instance walk): {type_id, clip_id, frame_no, bone_count}".
  * reports/DOCKET.md triages MESHFETCH.dispatch as "external -- the caller's
    instance walk". So the origin is the command stream.
  * The caller's command, DrawForm 0x0300, carries form, material_set,
    transform, viewport_mask, semantic_weight, flags. NO ANIMATION STATE.
  * Searched 2026-09-21: `clip_id`, `clip_slot`, `frame_no` and `type_id`
    appear in ZERO files under fpga/rtl/geometry/. spec/commands.zidl has NO
    pose, clip or animation command at any opcode.

So the page reader and the sixth requester yield real bytes and no statement of
which frame. That is an ABI addition and by protocol rule 4 it is the owner's.

===========================================================================
5. THE RULING I29 HAS NEVER CITED -- AND IT NAMED BLOCKER (b) FIRST
===========================================================================
reports/ZHAOZHOU_ANIMATION_HPS_RESIDENCY_ARCHITECTURE.md is headed
"OWNER-RATIFIED ARCHITECTURE DECISION, 2026-09-03" and its scope line is
"creature clip-bank storage, baked 60 Hz presentation data, animation
residency, prefetch, and THE GEOM.POSE MEMORY SEAM". It governs this entry's
producer end to end and I29 does not mention it.

  10.1  "no new animation-specific arithmetic block and no direct GEOM.POSE
        connection to MEM.HPS.BRIDGE" -- the reader's shape is already ruled.
  6     frame SEALING is conditional on residency, so GEOM.POSE never waits on
        a page. This is WHY the reader must fill whole and never fetch behind
        the decode -- the same conclusion the mutant reaches from cycles.
  4.2   freezes the resident-handle law ("frame packets refer to validated
        resident animation resources, never naked long-lived local-SDRAM
        addresses") and then says in terms: "THE EXACT COMMAND-RECORD
        REPRESENTATION IS DEFERRED."

THAT DEFERRAL IS BLOCKER (b). R216's rule found it: grep the SUBJECT, not the
title, and search beyond reports/ by name.

===========================================================================
6. OWNER DECISION -- D-POSEPAGE-A
===========================================================================
A CARRIER FOR {clip_id, frame_no, sub} AT THE COMMAND BOUNDARY. I29 cannot
close without one and no packet may invent it.

RECOMMENDATION: append a new opcode, do not widen DrawForm. `DrawPosedForm` at
0x0305, carrying DrawForm's six fields plus clip_id u16, frame_no u16, sub u8,
bone_count u8. DrawForm then keeps meaning BIND POSE and does not move a byte.

Precedent, in this tree and recent: W04 added DrawWarpedForm at 0x0304 keeping
DrawForm byte-for-byte, appended AT THE END because "inserting beside DrawForm
rewrites nine goldens"; W09 gives the companion rule, "preserve the ordinary
path".

FOUR FACTS CHECKED RATHER THAN ASSUMED (see 11bb777e):
  * 0x0304 is SPOKEN FOR by W04 and not yet in the zidl at this commit -- its
    lane is live -- so the next free draw opcode is 0x0305.
  * `abi version` DOES NOT BUMP. "A new opcode is a wire change, so 3 -> 4" is
    the SetEnvironment 0x0311 precedent and has been superseded five times:
    TerrainEpoch, SubmitTerrainSet, PublishResource 0x0030, SetPost and
    SetGradeTable all add opcodes and all STAY 3. Declare it LAST.
  * THE BYTES ALREADY REACH VRAM: PublishResource 0x0030 carries a `u8 kind`,
    so a CLIP_BANK page publishes today exactly as a CREATURE_FORM page does.
    This NARROWS the decision to WHICH FRAME and nothing else.
  * `sub` is not padding: zhao_geom_pose_cache's header records that without it
    "a key and its 60 Hz midpoint alias and the cache returns the wrong
    palette", and the 2026-09-03 ruling permits baked 60 Hz for every creature.

An alternative the owner may prefer: a `SetPose` STATE command arming animation
state for subsequent DrawForms, on SetPopulation 0x0303's precedent of a state
command inside the forms range. It avoids combinatorial opcode growth when a
form is both posed and warped, at the cost of frame-scoped state. Raising it
rather than choosing it.

Cheap to reverse: nothing is built against it.

===========================================================================
7. WHAT THIS LANE DELIBERATELY DID NOT DO
===========================================================================
NO RTL READER, and the reason is not time. Its one open question is where its
request comes from, which IS D-POSEPAGE-A -- and 4.2 says that request carries
a VALIDATED RESIDENT HANDLE with generation and epoch, not a naked slot and
frame, so the trigger interface is undetermined in a way that matters. Building
it now would add a SECOND uncomposed block beside zhao_geom_bonesrc ("BUILT,
INSTALLED NOWHERE") with the port that matters shaped by a decision not taken.
R90's amendment warns about exactly this: "it must not be discovered halfway
through the packet." What the next builder needs is in the entry instead: FILL
BOTH STORES WHOLE BEFORE RAISING `req_i`, the way zhao_geom_ladderbank adopts a
page before answering a lookup.

NO COMPOSITION AND NO PARTIAL COMPOSITION. Driving the SKELETON half of
geom_pose_* from a publication-triggered kind-8 loader while the quaternion half
reads out of an unfilled store is a STUB, not half a closure. Starting a decode
on a CLIP_BANK publication would be animation policy invented in a composer --
VIEWMASK's "a worse hidden adapter than the one the entry refused to build,
wearing a settled ruling as cover".

NO MANUFACTURED REGISTER RISE. zhao_geom_bonesrc, zhao_geom_ladderbank and the
new clip-page model have no mandatory design/blocks.yml rows and I added none.
R214's PAGEIO row was legitimate because the capability already had a written
CONTRACT; the clip page now has a contract and NO SILICON, which is the wrong
half. An honest 21.

NO CHANGE TO zhao_geom_bonesrc.sv, so its committed mutant needed no refresh.
`mutant_copy_drift` run AFTER the commits (R121): OK, 57 copies.

===========================================================================
8. FOR THE HANDOVER -- I29`s remaining shape
===========================================================================
ONE absence and one build behind it:
  1. D-POSEPAGE-A, a clip/frame carrier in the ABI. OWNER.
  2. then the page reader (both kinds) as the SIXTH u_geom_mem_adapter
     requester -- an in-core edit, no boundary port, with the bytes and the
     goldens now waiting on BOTH page kinds and a smoke bench that already
     holds real SDRAM behind the real core (u_geom_sdram).

Everything else under this entry is retired: the freeze is lifted, both page
kinds are authored and frozen, the store is built and priced at 830 ALM, and
the memory model was never missing.

===========================================================================
GATES -- all at 11bb777e
===========================================================================
completion_register.py           21  (9 tie-offs + 12 disconnected); entry ids
                                     monotonic I9 I13 I14 I17 I20 I21 I25 I27
                                     I29 I32 I34 I40 -- no phantom
packet_h_tieoff_audit.py         0 SILENT (8 declared, 1 reasoned, 10 by group)
check_console_inventory.py       OK
check_prod_manifest.py           OK -- 359 modules, 73 tops
check_quartus17_syntax.py        RC 0, 563 files
check_case_labels.py             OK (self-test 3 fire / 1 no-fire)
wrapper_port_parity.py           1264 = 1264, missing=0 stale=0
mutant_drivers.py                OK -- 96 of 98 named
mutant_copy_drift.py             OK -- 57 copies, RUN AFTER COMMIT (R121)
uncashed_cheques.py              RC 0
check_counters.py                RC 0
refmodel_liveness.py             RC 0
duplicate_functions.py           RC 0
gen_prod_top.py --check          fresh (73 instances)
gen_console_board.py --check     FRESH (1279 core ports, 73 parameters)
gen_shell_paired_diff.py --check          fresh
gen_shell_paired_diff.py --check --mutant fresh   <- BOTH FORMS, see below
npm run abi:check                not run -- spec/commands.zidl untouched

DIRECTED TESTS, BUILT AND RUN at this commit (R60):
  clip_page_directed                 44 checks, 16 verdicts fired   [NEW]
  mkclipbank_check                   golden reproduced, 10 refusals [NEW]
  geom_bonesrc_directed              6 cases PASSED
  geom_bonesrc_latefetch_mutant      PASSED (the mutation is present)
  geom_ladderbank_directed           545 checks
  cmd_exec_directed                  762 checks

EIGHT SMOKE FORMS, all PASS, `%Fatal` grepped rather than exit codes (R207):
0 fatal lines in every one.
  plain          raster pixels=2560, frames_admitted=1
  -GlowTag       PASS
  -LintOnly      elaborates
  -Mutant        terr_pl_slot_overflow_o fired 1
  -UntexMutant   geom_untex_refused_o fired 16
  -BadVertex     holes=1 groups_poisoned=2 replay_poisoned=8, frame completed
  -NoEchoArm     PASS
  -BadTraceArm   PASS

===========================================================================
A TREE-WIDE FINDING, NOT THIS LANE`S (commit 51202774)
===========================================================================
`cmake --preset windows-native` could not configure in any worktree branched
before tonight: tests/mutants/zhao_shell_paired_diff_mutant.sv was seven `gth_*`
ports stale against zhao_shell_top_v2 and verilate() aborts the WHOLE configure.
Regenerated. Two things worth keeping, both confirmed here:

  * `--check` AND `--check --mutant` ARE DIFFERENT GATES. Measured before the
    repair: `--check` returned RC 0 "fresh" while the tree could not configure.
    A gate list carrying only the first form reports green, TRUTHFULLY, about
    the wrong file.
  * THE DETECTOR FOR THIS EXISTS AND COULD NOT BE INSTALLED.
    `packet_h_paired_diff_mutant_fresh` runs exactly `--check --mutant`, and its
    add_test sits BELOW the verilate() whose failure aborts the configure, so
    CMake never reaches the registration. A GATE THAT CANNOT BE INSTALLED IS NOT
    EVIDENCE, and from a green gate list it is indistinguishable from one that
    passed. Checked against this lane's two new registrations by reading
    build/tests/CTestTestfile.cmake: clip_page_directed and mkclipbank_check are
    both present, both model-only add_tests with no verilate() above them in
    their block, so neither can be silently skipped the same way -- and
    packet_h_paired_diff_mutant_fresh is now registered too.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
