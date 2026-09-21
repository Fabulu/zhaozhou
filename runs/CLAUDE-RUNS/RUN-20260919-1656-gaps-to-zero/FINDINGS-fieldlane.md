<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
48c46633, 07055e85, 1b4280e6 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- FIELDLANE


## `48c46633` -- FIELDLANE: I34's blockers are ALL spent, the owner decided it against the entry's own recommendation, and the dead sweep was live in THREE files

```
FIELDLANE: I34's blockers are ALL spent, the owner decided it against the entry's own recommendation, and the dead sweep was live in THREE files

FINDINGS-fieldlane.md -- the harness refused this lane's write to the run
folder, as the brief predicted, so the findings are here and the coordinator
transcribes. No tool was switched to route around the refusal.

Entry I34 (TERRAIN.PATCH's field-height lane + its section 9.1 intake).
Register 22 -> 22. Comment-only: no RTL logic, no port change, no new module.

HEADLINE: R165 STILL HOLDS AND IT UNDERSTATED THE RESULT. It found two of
I34's three blockers spent on 2026-09-19. Today ALL of them are spent, the
owner decision the entry ends on has been DECIDED -- against the entry's own
recommendation -- and what remains is a scoped subsystem BUILD, not an absence.

== 1. THE FOUR BLOCKERS, EACH RE-MEASURED 2026-09-21 ==

S1. "WHAT ACTUALLY BLOCKS THE HEIGHT LANE IS THE UNIFORMS ... the 9.1 list
    intake carries NONE of them" -- SPENT. zhao_cmd_exec carries
    tfld_start_tick_o, tfld_duration_o and tfld_params_o (256 b, p0..p7
    Q16.16 LE) plus the four footprint fx16, tfld_handle_o and tfld_cmd_o.
    Landed by CMDFIELD at commit 33571772, "I34's build item (a)". Build
    items (a) AND (b) are both done -- (b) rides the same handshake rather
    than a table, deliberately, because a table's address port would have had
    no driver.

S2. "THE BLOCKER IS THE HANDLE -> PROGRAM-HASH MAPPING, AND IT HAS NO
    PRODUCER ... ZERO hits" -- SPENT. zhao_field_loader.sv declares
    pub_handle_o AND pub_prog_hash_o off pub_sel_i [2:0] over 8 objects,
    composed in the core on u_field_loader and promoted as fld_ldr_pub_*.
    The loader's own comment says that indexed read "costs one mux the
    descriptor table needs anyway" -- it was built for this consumer.

S3. "promoting a file out of synth/ is the FIELD lane's act and not a terrain
    packet's" -- DONE 2026-09-20. fpga/rtl/synth/zhao_probe_walk_earth.sv and
    zhao_probe_patch_acc.sv NO LONGER EXIST. They are
    fpga/rtl/terrain/zhao_terrain_field_walk.sv and
    fpga/rtl/terrain/zhao_terrain_patch_acc.sv, both `not-yet-adopted` in
    design/prod_manifest.yml. A FALSE-ABSENCE CLAIM IN REVERSE: the entry
    tells the next packet to READ a path that no longer exists, in the very
    paragraph written to stop it building a second walker.

S4. "THE OWNER DECISION, with three options and a recommendation" -- DECIDED,
    and the answer is OPTION 2. See section 3.

So R165 holds and is now CONSERVATIVE. Its two struck blockers are confirmed
struck; its one LIVE blocker -- section 13.7's TerrainField producer -- has
since been built too.

== 2. THE RECURRENCE R190 PREDICTED, FOUND IN THE WILD, IN THREE FILES ==

R190: "R165 already flagged this blocker as expired, and the core entry
re-asserted it afterwards anyway ... when a blocker is struck, the strike has
to land in the ENTRY, not only in a ruling."

It had landed in no entry at all, and the same dead sweep was live in three
production files:

  1. zhao_console_core.sv I34 -- asserts the zero-hit sweep TWICE, the second
     time stamped "re-checked rather than inherited". The re-check was honest
     and it was the same stale pattern.
  2. zhao_cmd_exec.sv -- the TerrainField arm's own header, "zero hits,
     re-checked this pass", written by the packet that BUILT the arm.
  3. zhao_field_host.sv -- "That arm is not built.", under a heading reading
     THE ONE THING THAT IS ABSENT, NAMED PRECISELY.

All three are corrected in this commit. A rotted citation under a heading
claiming precision is the worst form of it: that is the sentence everyone
quotes.

WHY THE SWEEP KEPT READING ZERO, so it is not re-derived: the pattern
`program_hash|prog_hash|programHash` WAS NEVER WRONG. pub_prog_hash_o matches
prog_hash. Re-running it today returns 22 hits under fpga/rtl. A zero-hit
sweep is a claim about a MOMENT and needs re-running when it is QUOTED, not
when it was written.

zhao_field_host.sv's correction is the sharpest: an ABSENT producer became an
UNCOMPOSED one. zhao_console_core instantiates zhao_cmd_exec and connects not
one tfld_* port, so ld_* still has no driver -- a different repair with a
different owner, and the old sentence sent readers at the wrong one.

== 3. THE OWNER DECIDED I34, AGAINST THE ENTRY'S OWN RECOMMENDATION ==

The entry recommends OPTION 3 (the front's "same program, same uniforms" fast
path, 46 clocks -> ~3). That recommendation is ruling R91, and the rulings
file stamps R91 "(provisional, coordinator)" -- NOT an owner ruling. Exactly
the authority distinction CFGARM measured.

The owner then ruled directly, in
reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt section 13.1:

  "FH18 deliberately selects the field-major patch-working-set form, already
   represented by zhao_probe_patch_acc and the amended Earth contract. R91's
   goal is retained; its claim that a front-only three-clock transport change
   is the whole fix is not."
  ... "Do not leave two opposite stream-order laws alive."

That is OPTION 2, named and taken, with option 3 explicitly declined as
insufficient. The entry had been carrying a DECLINED recommendation as its
live one.

== 4. WHAT I DELIBERATELY DID NOT BUILD, AND WHY THE ATTRACTIVE ACT IS FORBIDDEN ==

The tempting composition is ONE WIRE: join CMD.EXEC's tfld_* to
terr_pt_fld_add_* inside zhao_console_core. The producer exists, the
destination is ratified section 9.1, and it would DELETE EIGHT BOUNDARY INPUTS
without dangling anything -- strictly net-negative on ports.

Directive section 13.2 forbids it in writing, twice:

  "Introduce the production field-major implementation under the TERRAIN.PATCH
   capability (for example zhao_terrain_patch_v2), retaining the old
   implementation as a transaction/numeric oracle for paired tests." It owns
   "the bounded 16-entry field intake in command order".

  "The old fld_add_hash_i/fld_add_cmd_i fields are trace-only in the current
   RTL. Store the actual required association identity in the new list
   explicitly; do not pretend the existing block already retained a program
   binding."

So the intake MOVES to v2 and the old serial block leaves the shipping
datapath for the patch it owns. Wiring the producer into the block the owner
has just made an ORACLE is composing the superseded arrangement -- CLAUDE.md's
"only the latest version gets composed" in its general form.
zhao_terrain_patch_v2 DOES NOT EXIST in this tree.

I also declined to build the {handle -> hash} resolver standalone. It is real
and small, but pub_sel_i is a CORE BOUNDARY INPUT the core does not drive, so
composing it needs build item (d)'s two-client share; and a built-but-
uncomposed module is an uncashed_cheques "BUILT, INSTALLED NOWHERE" and moves
the register the wrong way. An honest 22 beats a dishonest 21.

== 5. WHAT ACTUALLY REMAINS, SCOPED RATHER THAN RE-DERIVED ==

ONE build, owned by the Earth integration packet, every part already scoped by
the owner:

  13.2 -- zhao_terrain_patch_v2, the field-major capability owner, eight named
          responsibilities. UNBUILT.
  13.3 -- exact semantic equivalence per vertex; the loop transpose is
          permitted "because the order of additions at EACH vertex is
          unchanged". Saturating arithmetic is not associative: never sum wide
          and clamp once, never sort fields by program.
  13.4 -- the accumulator's MISSING ready/valid, backpressure and phase
          exclusivity. design/prod_manifest.yml DECLARES this gap rather than
          hiding it ("KNOWN OPEN and not hidden ... deliberately NOT in the
          promotion commit so the rename stays a provable no-op"). That row is
          a model of how to defer.
  13.5 -- the serial-channel memory organisation, ~20 M10K for four
          accumulators, "a PACKING CANDIDATE, not a fitted claim". Read against
          R222's HUD refusal at 180/553 and I21's deviation store at 185/553:
          three features now bidding on a device whose M10K occupancy has never
          been measured.
  13.6 -- patch lifecycle.
  13.7 -- the lowering is BUILT; its other half is NOT. "Add a positive
          composed mode such as -FieldActive" requiring nonzero actual runs,
          correct output values, real terrain consumption and recovery after a
          deliberately bad association. The smoke has EIGHT forms and none is
          that one, and 13.7 says the existing no-program form is the REFUSAL
          CONTROL, not the positive gate.

The one wire is one wire OF that packet, not a prerequisite for it.

== 6. INSTRUMENT NOTES ==

* No counter was added, so none is claimed to fire. This lane changed comments
  only.
* THE REGISTER'S PHANTOM-GAP TRAP WAS CHECKED DELIBERATELY, not assumed: the
  inserted block is ~100 new comment lines and the count held at 22 with I34
  still classified `boundary`. No inserted line begins "// I<n>.".
* completion_register.py HARD-FAILS on "NOT a tie-off" in an entry BODY, and
  reclassifies on "TIED TO ZERO"/"STRUCTURALLY STUCK" anywhere in the blob.
  Both were avoided by reading the patterns first rather than by luck -- worth
  knowing before any lane edits an entry at length.
* packet_h_tieoff_audit: 8 declared, 1 reasoned, 10 by group comment, 0 SILENT.
  Unchanged; this lane added no literal connections.
* mutant_copy_drift: the two core copies (slot_overflow at c92011d1, untex_decl
  at 466a24d6) were ALREADY behind the core's last commit 6d817488 before this
  lane touched anything -- both verified ancestors of HEAD. Pre-existing, not
  caused here.

== 7. FOR THE HANDOVER ==

I34's real remaining shape is ONE subsystem build with a complete owner
specification and ZERO open blockers. It is no longer a decision-board item. It
should be scheduled as the Earth integration packet against sections 13.2-13.7,
and it needs a fit gate for 13.5's memory claim -- which the directive itself
calls a packing candidate rather than a fitted number.

THREE LANES IN THREE NIGHTS HAVE NOW FOUND BLOCKERS SPENT BY RE-MEASUREMENT --
VIEWMASK on I21, CFGARM on I14, this one on I34. Not one needed more than an
hour, and not one needed to change RTL to do it. The cheapest work available in
this campaign is reading the entry against today's tree; the most expensive is a
packet commissioned against a blocker that expired before it started.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

## `07055e85` -- FIELDLANE 2/2: the blocker I34 never stated is the strongest one, three more dead-path citations, and a correction to my own previous commit

```
FIELDLANE 2/2: the blocker I34 never stated is the strongest one, three more dead-path citations, and a correction to my own previous commit

FINDINGS-fieldlane.md part 2 (the harness refused this lane's write to the run
folder; findings live in these two commit messages and the coordinator
transcribes). Register 22 -> 22. Still comment-only: no RTL logic, no port
change, no new module.

== 1. A BLOCKER I34 NEVER STATED, AND IT OUTRANKS THE FOUR IT DID ==

design/console_inventory.yml's own `why` text carries it, and this entry does
not: "closing I34 needs velocity/material/nav channels that
zhao_terrain_patch.sv does not have, and section 20.8 forbids closing it by
wiring only height."

VERIFIED IN THE RTL rather than inherited. zhao_terrain_patch offers exactly
ONE return lane -- fld_valid_i / fld_ready_o / fld_height_i -- and has no
velocity, material or nav_cost input anywhere on the block. The Earth record
declares FOUR output channels; the composed consumer can receive one.

That is a CONSUMER-SIDE gap, entirely independent of directive section 13.1's
architecture ruling, and it would forbid the one-wire tfld_* -> fld_add_* join
even if 13.2 had never been written. Recorded in the entry as S5.

THE SHAPE OF THIS IS THE SAME DEFECT RUNNING THE OTHER WAY. The four blockers
I34 DID state had all expired; the one that had not expired lived only in a
ledger's prose and never reached the entry. An entry can be wrong by carrying
dead blockers AND by missing live ones, and both come from the same cause --
nothing reads the neighbours back.

== 2. THREE MORE LIVE CITATIONS OF A PATH THAT NO LONGER EXISTS ==

Commit 1 struck the dead `fpga/rtl/synth/zhao_probe_walk_earth.sv` citation
inside I34. A tree-wide sweep found three more, all in production or oracle
files, all corrected here:

  * zhao_console_core.sv, the TERRAIN.VELOCITY discussion -- "whoever takes the
    height lane should look at fpga/rtl/synth/zhao_probe_walk_earth.sv FIRST
    ... and it is still in synth/ under a probe name". A live instruction to
    read a dead path, with a stale conclusion attached. Its ARGUMENT ("a file
    is not a probe because its name says so") was right, and acting on it is
    what moved the file.
  * zhao_console_core.sv, the FORGE.CLIFF near-miss inventory -- names the same
    dead path; the near-miss finding itself is unchanged by the move.
  * reference/src/zfield/zfield_plan.cpp -- cites BOTH dead paths, and cites
    zhao_probe_walk_earth.sv as "THE RTL LAW HERE" for the 297-group constant.
    The oracle was citing its own governing RTL by a path that does not
    resolve. Law, assertion and the number 297 all unchanged; only the paths
    were wrong.

Correct names throughout: fpga/rtl/terrain/zhao_terrain_field_walk.sv and
fpga/rtl/terrain/zhao_terrain_patch_acc.sv.

== 3. A CORRECTION TO MY OWN PREVIOUS COMMIT MESSAGE ==

Commit 1's instrument notes said the core's two mutant files were "copies"
already behind the core's last commit, "pre-existing, not caused here". THAT
PREMISE IS WRONG AND I CHECKED IT BY HAND RATHER THAN QUOTING THE GREEN.

mutant_copy_drift.py returned OK after my commit, which contradicted my own
prediction that committing the core would put both copies behind it. Rather
than accept the reassurance, I tested the tool's wrapper exemption directly:
zhao_console_core_slot_overflow_mutant.sv contains the literal line
`  zhao_console_core #` -- it INSTANTIATES production rather than copying it.
Both core mutants are WRAPPERS, so they elaborate whatever production
currently is and CANNOT go stale. The tool is right, the exemption is right,
and my prediction was wrong.

Worth keeping for two reasons. First, the lane that edits the core next should
know its mutants are wrappers, so no refresh is owed. Second, this is the
pleasant case of the broken-instrument law: the green was CORRECT and my model
was wrong, and the only way to tell that apart from a blind instrument was to
open the file. A green you predicted is worth nothing; a green that surprises
you is the one to open.

== 4. AND A PROCESS ERROR OF MINE, RECORDED BECAUSE IT IS CHEAP TO REPEAT ==

I started the eight smoke forms in the background and then edited
zhao_console_core.sv twice while they ran. CLAUDE.md names this exactly: a
SUITE reads the live tree, nothing snapshots for it, and "the greens are worth
no more than the reds". I killed the run rather than read it.

Killing it surfaced the companion rule in the same file. TaskStop ended the
loop and LEFT AN ORPHAN: a powershell running
`run_console_core_smoke.ps1 -UntexMutant` survived with its parent gone --
"stopping an agent does not stop its background work", live. Per R81 it was
classified by COMMAND LINE and parent chain before anything was killed, and
only my own subtree was touched; no verilator_bin or g++ belonging to another
lane was signalled. Verified zero survivors before re-running.

The smoke forms were then run once, from scratch, on the settled tree.

== 5. GATES AT THIS COMMIT ==

register 22 (unchanged, I34 still `boundary`); check_console_inventory,
check_prod_manifest, check_quartus17_syntax, check_case_labels,
wrapper_port_parity, mutant_drivers, uncashed_cheques, check_counters,
refmodel_liveness, duplicate_functions all RC 0; gen_prod_top --check,
gen_console_board --check, gen_shell_paired_diff --check AND --check --mutant
all RC 0 (both forms run explicitly, and both were green -- the bare form now
covers the mutant, as CFGARM intended); mutant_copy_drift RC 0 after commit
per R121; packet_h_tieoff_audit 8 declared, 1 reasoned, 10 by group comment,
0 SILENT -- unchanged, this lane added no literal connections.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

## `1b4280e6` -- FIELDLANE 3/3: I34 IS the directive's COMMIT G, the owner forbids its shortcut BY ENTRY NUMBER, and a ctest that has measured nothing since 19 September

```
FIELDLANE 3/3: I34 IS the directive's COMMIT G, the owner forbids its shortcut BY ENTRY NUMBER, and a ctest that has measured nothing since 19 September

FINDINGS-fieldlane.md part 3 (harness refused the run-folder write; findings
are in these three commit messages). Register 22 -> 22. Comment-only.

== 1. I34 IS COMMIT G, AND NOTHING IN THE TREE SAID SO ==

Directive section 20.8 is titled "Commit G -- Earth production path and one
reducer", and it is I34's remaining work exactly:

  "Implement the bounded command/association bridge, field-major scheduler and
   single patch-working-set accumulator. Reuse the existing probe's numerical
   reducers with real phase/backpressure/lifetime control. Preserve the rest of
   the terrain page/cache interface rather than treating a probe as a whole
   terrain subsystem."

Section 20.8 is cited in three files already (zhao_cmd_exec.sv,
design/console_inventory.yml, design/prod_manifest.yml) and NOT ONE of them
says I34 IS that commit. The entry is now labelled with it, so the next packet
inherits a named commit with a written scope instead of a gap with a
re-derived one.

== 2. THE OWNER FORBIDS I34's SHORTCUT BY ENTRY NUMBER ==

Section 20.8, verbatim:

  "Route height, velocity, material and nav outputs from the same evaluation to
   their real owners. DO NOT CLOSE I34 BY WIRING ONLY HEIGHT while declaring the
   other three channels present because they have spare bus bits."

The owner names the entry and forbids the exact act. That is now quoted in the
entry beside S5. My refusal of the one-wire join was reached from section 13.2
and from the consumer's single return lane BEFORE this sentence was found; the
sentence is independent confirmation, and it is the one a future packet should
be shown first because it is the shortest.

Section 20.1 completes the framing and removes any reading of this entry as
blocked: "A missing producer already named and designed here is not such a
contradiction: it is THE WORK THE PACKET WAS COMMISSIONED TO DO." It also says
"Nobody patches the same host/engine interface independently without
coordinating its exact schema" -- a second reason one lane should not have
joined tfld_* into the patch intake on its own judgement.

== 3. A CTEST THAT HAS MEASURED NOTHING SINCE 2026-09-19, AND IS GREEN ==

REPORTED, NOT FIXED: it lives in tests/CMakeLists.txt, a shared file, and
choosing the repair is the geometry lane's call. Naming it here so it is not
lost.

  * tests/prod/run_console_core_smoke.ps1 records at its own line 121:
    "-BadAttribute: RETIRED 2026-09-19 (geom2 packet, core entry I46 CLOSED)".
    The switch is GONE from param() -- `BadAttribute` has exactly ONE hit in
    the whole script and it is that comment.
  * tests/CMakeLists.txt still registers `console_core_attrpack_control`
    ("Registered 2026-09-19 with GEOM.ATTRPACK", LABELS "prod;mutant"), and it
    invokes the smoke with `-BadAttribute`.

Two packets, the same day, opposite directions, neither seeing the other --
R220's shape exactly.

I PROVED THE BINDING RATHER THAN ASSERTING IT, with a throwaway script outside
the tree carrying the same param() signature. The result is QUIETER than the
brief's warning about positional binding: with `-File`, PowerShell SILENTLY
SWALLOWS an undeclared `-Switch`. It does not bind to $Repo, it does not warn,
and RC is 0. Measured: `Repo=[] BuildIn=[] Mutant=False`, PROBE_RC=0.

So `console_core_attrpack_control` runs THE PLAIN SMOKE and passes -- a
duplicate of console_core_smoke wearing the name of a positive control for
attribute carriage. The smoke script's own retirement note predicted this class
in writing: "an inverted-polarity control whose define no longer changes
anything would PASS by failing for an unrelated reason". The realised version is
gentler and worse: it passes by SUCCEEDING at the wrong test.

Recommended repair, for whoever owns it: either re-declare the switch with
working logic, or retire the ctest with the control. Deleting a control is
removing function, so this lane did neither.

AND ONE MORE, SMALLER, FROM THE SAME SWEEP: `-BadDescriptor` is a committed
positive control for the geometry asset path, documented in the script, with
its own build-directory tag `baddesc` -- and it is in NO ctest and in NO gate
list. Nothing runs it. That is the uncashed-cheque shape in the test harness:
built, installed nowhere.

== 4. GATES AT THIS COMMIT ==

register 22 (I34 still `boundary`, and deliberately re-checked after ~200 added
comment lines for the phantom-gap trap); the fourteen static gates all RC 0,
including gen_shell_paired_diff --check AND --check --mutant run separately;
mutant_copy_drift RC 0 after commit per R121; packet_h_tieoff_audit 8 declared,
1 reasoned, 10 by group comment, 0 SILENT; all eight smoke forms run from
scratch on a settled tree and grepped for %Fatal rather than trusting RC, per
R207.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
