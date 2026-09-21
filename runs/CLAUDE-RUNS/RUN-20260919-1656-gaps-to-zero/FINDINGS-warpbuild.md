<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
20261b1b -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- WARPBUILD


## `20261b1b` -- FINDINGS-warpbuild -- the document, because the harness refuses .md under runs/

```
FINDINGS-warpbuild -- the document, because the harness refuses .md under runs/

Packet WARPBUILD, 2026-09-21. Branch gz/warpbuild from
claude/ceiling-architecture-20260912 at 9f7e2670. Own worktree, never rebased.
Authority: reports/OWNER-RATIFICATION-20260920-WARP.md, decisions W01-W18, from
owner commit 4c256137.

`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-warpbuild.md` was
refused by the harness, which the packet anticipated. This message IS that
document.

================================================================================
THE REGISTER DID NOT MOVE, AND THAT IS THE FIRST THING TO SAY
================================================================================

  BEFORE   MANDATORY GAPS REMAINING : 24   (9 tie-offs + 15 disconnected + 0 unbuilt)
  AFTER    MANDATORY GAPS REMAINING : 24   (9 tie-offs + 15 disconnected + 0 unbuilt)

Both run BARE. RC 1, which is normal while gaps remain. `zhao_geom_warp` and
`zhao_geom_parambuf` are both still in BUILT BUT NOT CONNECTED.

The packet's premise was that composing the adapter would close
`zhao_geom_warp`. IT WOULD NOT HAVE, and the reason is worth more than the
number would have been. What blocked composition was never the adapter's
parameter -- it was that no draw could set `d_warp_en_i`, so a composed Warp
would have sat permanently in its W09 bypass: present, correct, tested and
STRUCTURALLY UNREACHABLE. Shipping that to move a counter would be the register
reading green about a thing it cannot see, which the contract warns about in
writing.

So this packet built the thing that makes composition MEAN something, and then
measured that the remaining distance is longer than the prerequisite table said.

================================================================================
WHAT WAS BUILT -- four commits, each pushed as it landed
================================================================================

1. f581ea95  DrawWarpedForm 0x0304, the ABI
2. 66d1cd2d  CMD.EXEC's arm, the descriptor sidecar, the core's stale sentence
3. ce529d48  seven directed cases; case 48 watched to FAIL
4. f3d318aa  the contract's measured rows, three captures re-fared

-- THE ABI ---------------------------------------------------------------------

W04, field for field from directive 6.1. 96-byte record / 80-byte payload,
MEASURED from the generator rather than asserted: `cmd_draw_warped_form.bin
(96 B)`, and the generator's independently-derived pad map
ZHAO_PADS_DRAW_WARPED_FORM = {62, 63, 76, 77, 78, 79} agrees with 6.1's
"62..63 reserved zero" and "76..79 reserved zero" exactly.

DrawForm IS BYTE-FOR-BYTE UNCHANGED, AND THIS IS A MEASUREMENT. Of the 27 files
in tests/abi/golden/, exactly TWO moved -- the new record and zcap_minimal.zcap.
The other 25 command records are byte-identical, because the record was APPENDED
AT THE END under R171 (sample.ts stamps source_id from the command's index, so
an insert beside DrawForm churns nine goldens for no change in meaning).

`attribute_mode` is an ENUM, not the directive's u8. Directive 6.3 requires in
adjacent sentences that other values are "refused" and that "payload validation
is generated/centralized with the decoder" -- and an enum field already IS that
mechanism: oracle.ts emits a payload-relative {offset, size, valid values} row
and the decoder answers ZH_ABI_BAD_VALUE. The wire is identical, so W04's
80-byte payload does not move. Declared u8, 6.3's refusal would have become a
hand-written check in whichever consumer remembered one.

ABI version stays 3, recorded rather than inherited (6.4). Precedent six deep.

-- CMD.EXEC AND THE SIDECAR ----------------------------------------------------

The snapshot rides the SAME handshake and the SAME queue entry as the draw.

IT IS DIRECTIVE 7.1's SPLIT, NOT A WIDER QUEUE. 7.1 refuses the obvious thing by
name -- "the common draw item holds `warp_enabled` plus a compact descriptor
cookie, not an 80-byte Warp record copied through every geometry stage" -- so
`dq` gains exactly one bit and the 456-bit snapshot lives in `wq`, a RAM-shaped
array SHARING `dq`'s pointers. Sharing them is the design: 7.1 also requires the
sidecar's capacity to cover the draw queue's, and one pointer pair means there
is no second capacity to get wrong and no second overflow rule to declare.

`wq_head <= wq[dq_rp]` sits on the SAME ENABLE as `dq_head <= dq[dq_rp]`,
deliberately. This looks like CLAUDE.md's lockstep-blindness pattern and is its
mirror: that warning is about a DETECTOR whose operands share an enable; this is
a PAYLOAD, and one enable is what makes a draw/snapshot skew structurally
impossible -- R229's argument for the pose, one opcode along.

THE REFUSAL IS THE OPPOSITE OF THE POSE'S, four lines away in the same port
list, and both are right. `warp_draw_refused_o` counts a 0x0304 that names a
program and breaks one of 6.3's three between-field rules; the draw does NOT
enter the queue (contract section 9: DRAW_INVALID, "refuse before emitting
meshlets"). `pose_clip_refused_o` degrades and keeps the draw. The difference:
an unrepresentable clip has an authored fallback -- the bind pose -- and
refusing it would make a creature vanish, whereas there is no authored behaviour
for "the deformation you asked for, minus the deformation". An unwarped draw of
a creature whose record asked for one is a confidently wrong SHAPE that nothing
downstream can detect.

Three rules are checked and the rest deliberately are not: `attribute_mode`'s
own legality and the four pad bytes belong to the GENERATED decoder, and
restating them is the second-implementation failure uncashed_cheques check 3
exists to find. The three checked are the ones no generated check can see,
because each is a relationship BETWEEN fields or a sign.

VERILATOR CAUGHT A SECOND SOURCE OF TRUTH BEFORE IT WAS COMMITTED. The first
version declared WARP_ATTR_INLINE4/STREAM4 locally "because SystemVerilog cannot
import a .zidl enum". VARHIDDEN said otherwise -- the generator already emits
both into zhao_abi_pkg.sv. The local copy was DELETED rather than waived.

The oracle got an arm: render_frame.cpp's `default:` SKIPS a record, so a
capture containing 0x0304 would have drawn NOTHING. It carries the snapshot and
MIRRORS the refusal, because an oracle that accepted what the silicon refuses
would disagree with it on the only case that matters.

-- THE TESTS, AND THE ONE THAT WAS FIRED --------------------------------------

cmd_exec_directed: 850 -> 954 checks, built and run (R60). Cases 42..48.

CASE 48 IS THE ONLY ONE THAT COULD CATCH W05. A single global `current_warp`
register PASSES cases 42-47 -- each presents one warped draw at a time. Two
lines were made to model one (wq[dq_wp] and wq[dq_rp] both -> wq[0]):

  FAIL: case48: draw 0 keeps ITS program: expected 0x101, got 0x202
  FAIL: case48: draw 0's time: expected 0x1, got 0xFFFFFFFF
  FAIL: case48: the two snapshots are DISTINCT -- W05, no global current_warp
        expected 0x1, got 0x0
  [cmd_exec_directed] 9/954 checks FAILED

DRAW 0 RECEIVED DRAW 1's PROGRAM -- the earlier draw carrying the later draw's
metadata, exactly what 6.2 means by a setting that "could retroactively change
older draws". Restored: 954 passed, `git diff` on the file EMPTY.

THE RESTORE HIT THE DOCUMENTED Copy-Item TRAP. The restored file carried the
backup's older timestamp, ninja said "no work to do", and the MUTANT's binary
re-ran -- reporting the same nine failures as though the repair had not worked.
Verified by CONTENT, forced the timestamp, rebuilt, green.

No committed mutant is filed, on R95's own wording: a mutant is owed for a guard
UNREACHABLE BY LEGAL STIMULUS. Every counter here is reachable and each is
discriminated in BOTH directions inside the suite -- case 45 refuses a negative
bound per axis then accepts an all-zero one; case 46 refuses three malformed
mode/resource pairings then accepts two lawful ones; case 44 accepts a record in
which every warp rule is broken, because it names no program.

================================================================================
P8 -- THE MEASURED VERDICT: PARTIAL
================================================================================

BIND is present and STRONGER than section 9.3 asked. post_op 3 is exhaustively
decoded (OpLoad/OpCommit/OpLookup/OpFh2, a counted V_BAD_OPERATION default, a
committed control at tests/mutants/zhao_field_doorbell_op3_alias_mutant.sv) --
the old catch-all `else -> D_LOAD` is gone, and post_op_i being still [1:0] no
longer matters. BIND_PROGRAM is not a doorbell state: the doorbell is a CREDITED
RELAY (D_FH2/D_FH2_RESP, return record reserved before the post is taken), and
the binding lives in zhao_field_loader's K_BIND, which validates a six-way
conjunction against ONE EXACT OBJECT -- handle reserved bits, obj_ready,
!obj_staging, obj_gen, obj_handle32, obj_epoch, obj_full_crc. BIND never
resolves by canonical hash, which discharges section 9.1's aliasing clause.

SEAL IS NOT A BINDING OPERATION. L_SEAL is the only writer of obj_ready and is
reachable only from the INSTALL arm; K_BIND runs L_ACCEPT -> L_CHECK -> L_RETURN
and never enters it. No verb, no ticket, no counter. BIND_META and UNBIND DO NOT
EXIST -- VERB_RELEASE_BINDING is aliased with two other verbs into one arm that
only decrements obj_pins; obj_ready stays set and obj_gen is not bumped. (9.3's
"never invalidate live work silently" IS honoured, by a different mechanism: the
allocator skips pinned objects and answers V_NO_CAPACITY.) Section 9.3's four
post_kind values were SUPERSEDED, not implemented -- the RTL follows the
SHARED_FIELD repair architecture's 10.2 encoding, a stated choice cited in file.

THE CHAIN IS BROKEN IN THE MIDDLE, and that is the finding:

  handle -> canonical program          PRESENT  (fh2_resp_handle_o, pub_*)
  canonical program -> resident slot   ABSENT
  resident slot -> prepared plan       PRESENT  (hdr_assoc_gen, prep_gen, hdr_ipok)

Hash-to-slot lives in zhao_field_progcache, reachable only through the legacy
post_op=2 LOOKUP path; the loader never issues a lookup and the doorbell's FH2
arm never touches pc_lu_*. NOTHING JOINS A BINDING OBJECT TO A DIRECTORY SLOT,
so section 9.2's row field CURRENT RESIDENT SLOT has no storage anywhere in the
tree, and hdr_assoc_gen (8-bit, software-supplied) is never differenced against
obj_gen -- two generation counters in two modules that nothing compares.
pub_sel_i reaches the console boundary with NO in-console driver, which the
core's own note S2 records.

CONSEQUENCE FOR WARP: contract section 9's PROGRAM_NOT_RESIDENT / STALE_BINDING
/ PLAN_NOT_READY have NO HARDWARE THAT CAN COMPUTE THEM, and zhao_geom_warp's
d_slot_i / d_slot_valid_i have no producer that can resolve
DrawWarpedForm.warp_program into a slot. P8 GATES A WORKING WARP AS DISTINCT
FROM A COMPOSED ONE.

A DEFECT FOUND ON THE WAY, not repaired here. In zhao_field_loader, st_idx is
assigned at EXACTLY ONE SITE -- L_RESERVE, the INSTALL path -- yet L_RETURN
unconditionally emits `fh2_resp_slot_o <= st_idx` and builds fh2_resp_handle_o
from obj_gen[st_idx]. A SUCCESSFUL K_BIND REPLIES WITH THE SLOT AND GENERATION
OF WHATEVER OBJECT THE LAST INSTALL RESERVED. It survives testing because the
natural stimulus is install-then-bind-what-you-installed, where st_idx is right
by accident; it diverges on install(A), install(B), bind(A), and on a bind after
reset. fpga/rtl/field/ is not this packet's file set.

Lower severity, same family: the doorbell's ret_slot_o carries an
OBJECT-CATALOGUE INDEX on FH2 returns and a PROGCACHE DIRECTORY SLOT on
LOAD/COMMIT/LOOKUP returns -- two namespaces 9.1 forbids conflating, on one
wire, disambiguated only by ret_op_o. The file states exactly this discipline
for ret_verdict_o and does NOT state it for ret_slot_o.

================================================================================
P5 -- DISPOSITION: DISCHARGED AT THIS COMPOSITION, narrower obligation in place
================================================================================

The array is still flat; that half of the old row stands. What was never
measured is whether this console can PRODUCE two simultaneously eligible plans.
IT CANNOT.

req_ready_o is gated on `state == E_IDLE`; the FSM cannot return there without
passing E_RETIRE, itself gated on a WHOLE-FABRIC fence -- `fab_active == '0'`
across all PROGS contexts, not fab_active[cur_slot]. One state, one cur_slot,
one cur_export, none dimensioned by any parameter. prep_value is read on EXACTLY
ONE CLOCK PER POINT (the grant clock) into registers, and the running point
never touches it again. Loads cannot race that read: `ld_ready_o = (state ==
E_IDLE)` and the E_IDLE arm takes ld_valid_i in STRICT PRIORITY over a grant.

PARAMETER-INDEPENDENT across CLIENTS, PROGS, FAB_LANES (which replicates one
point across lanes and discards the surplus -- it does not add points),
FAB_GROUP_PTS, FAB_OUTSTANDING and CREDITS (whose overlap is a retired,
already-captured response in the queue, downstream of cur_export). It reopens
only on an RTL change adding a second front.

THE GENERATION CHECK IS NOT LOCKSTEP-BLIND, which had to be asked rather than
assumed. ggen_c = hdr_assoc_gen[gslot_c] is written only under LdAssoc (kind 5,
ld_data_i[7:0], per SLOT); prep_gen[i] only under LdPrepared (kind 7,
ld_data_i[47:40], per PREP INDEX). Mutually exclusive by decode, so the two
sides cannot move together -- and it has been SEEN TO FIRE:
field_host_v2_directed FT036 bumps the association generation over an unchanged
prepared file and asserts StBadPrep with prep_bad_o moving by exactly one.

THE NARROWER OBLIGATION THAT REPLACES IT: PREP-INDEX DISJOINTNESS. prep_gen is
indexed by prepared-scalar index while ggen_c is indexed by slot, and
omap_index[slot][j] is chosen by SOFTWARE. If a Warp slot and an Earth slot both
name prep index 0, the later LdPrepared overwrites the earlier value and the
seed check compares prep_gen[0] against hdr_assoc_gen[warp_slot] -- two 8-bit
bytes software assigns and nothing in RTL forces to differ. If they agree, WARP
SILENTLY CONSUMES EARTH'S SCALAR. No interleaving needed; serialisation does not
touch it.

It is unreachable today for the weakest possible reason: NOTHING PRODUCES
PREPARED SCALARS AT ALL. PREPARED_SCALARS (section 0x0022) is defined in
tools/field/gen_field_host_schema.py and mirrored into
zhao_field_host_image_pkg.sv, and NEITHER tools/field/pack_field_host.cpp NOR
compiler/src/field_ir/serialize.ts emits it; zprog_output_coverage.py over the
three shipped Earth programs reports every declared output satisfied by a
vector-register write. A GUARD QUIET BECAUSE ITS INPUT DOES NOT EXIST IS NOT A
GUARD THAT PASSED.

A CONTRADICTION SURFACED BY THE SAME MEASUREMENT, flagged for FIELD's owner:
zhao_field_host_v2.sv's header states that packet L1 measured the three shipped
Earth programs at 2/1/1 uniform outputs. zprog_output_coverage.py against those
same three .zprog files reports ZERO. Both cannot describe the shipped programs.

================================================================================
P6 -- DISPOSITION: OPEN, CHEAP, AND NOT THIS PACKET'S
================================================================================

TABLES = 2 stands. What is new is the other side, measured: the fabric's table
store is HARD-WIRED TO FOUR and not parameterised at all.
zhao_field_v3_curve.sv declares `logic [95:0] tbl_ram[0:255]` (4 x TBL_N) and
`logic [6:0] meta_n [4]`, and tl_tbl_i is a literal [1:0] through
zhao_field_v3_engine and zhao_field_v3_svcpath. The host at TSELW=1 writes
`fab_tl_tbl = 2'(ld_addr_i[...])`, zero-extending a one-bit selector into a
two-bit port.

SO TABLES=2 SAVES NO FABRIC AREA. It makes tables 2 and 3 of an already-built
RAM unaddressable. Closing it is TSELW 1->2 and LDADDRW max(PCW, TSELW+TIDXW)
7->8 -- and the doorbell is ALREADY SIZED FOR IT: the console composes
.POSTADDRW(8) against the loader's 7 and refuses a legacy load that does not fit
rather than narrowing it.

Left open because the question it answers -- does a shipped Warp program need
more than two curve tables? -- cannot be asked until a Warp program can be
admitted, and a Field-host parameter is not this packet's file set.

================================================================================
P7 -- MEASURED COST, NO FIT
================================================================================

check_prod_manifest.py reads the composed parameters out of the Verilator AST
and prints INSTR_N=32'd32 -- the value the console ELABORATES, not the value a
line of source says. W14's ceiling is 48.

Moving it: PCW = clog2(INSTR_N) goes 5 -> 6, and LDADDRW = max(PCW,
TSELW+TIDXW) = max(6, 7) stays 7, so no port width at the host's edge moves. The
uop store grows by 16 x 64-bit words PER CONTEXT at PROGS = 8 -- 8,192 bits.
THAT IS A CAPACITY NUMBER, NOT AN ALM NUMBER. Whether it infers as M10K or as
registers is what a fit answers and arithmetic does not. PHYSICAL FIT PENDING.

================================================================================
HAND-COUNTED AREA (no Quartus was run)
================================================================================

Counted from the declarations, stated as flip-flops because that is what was
counted:

  wq sidecar            456 x DRAW_Q(4) = 1,824   RAM-shaped, 1W+1R; may infer M10K
  wq_head                                  456    registers
  dq widening            1 x 4 =             4    DQ_WARP_LO
  dq_head widening                            1
  capture registers   32+32+128+128+32+8+8+96 = 464   dw_*, one set, not per-slot
  two counters                               64    warp_draws_issued_o, warp_draw_refused_o
  ----------------------------------------------
  TOTAL NEW STATE                        ~2,813 bits

Combinational: eleven offset-range comparators on rpos (16-bit); the dw_ok_c
tree (three sign bits, two 32-bit zero-compares, one 128-bit zero-compare, two
8-bit equality compares); one 456-bit 4:1 mux for the wq read. NO MULTIPLIERS,
no new memories declared as such, no new clock.

At the console boundary: 457 signal bits of new output plus 64 of counters. Real
cost for a lane nothing consumes yet, spent deliberately -- the pose lane's note
in the same file records why an unread output is worse than a priced one:
synthesis deletes the capture registers behind it and the fit prices the lane at
zero.

NONE OF THIS IS A FIT RESULT AND NONE OF IT SHOULD BE QUOTED AS ONE.

================================================================================
TIE-OFFS DECLARED: NONE
================================================================================

Nothing was tied off. Every new port is driven by a decoded record field or by a
counter. packet_h_tieoff_audit.py on fpga/rtl/prod/zhao_console_core.sv returns
RC 0 and the core's INCOMPLETE block is unchanged at 12 entries (9 boundary + 3
resolved-in-composer).

.CLIENTS(2) was NOT moved to 3, and that is the packet's own rule obeyed rather
than a gap left: a third client whose req_valid_i is constant zero is a tie-off,
and the widening lands in the act that composes the adapter.

================================================================================
THE STALE SENTENCE, AND WHY IT COST MORE THAN A STALE FACT
================================================================================

zhao_console_core.sv said, on the zhao_field_host_v2 instantiation:

  "`zhao_geom_warp.sv` does not exist in this tree, so there is nothing to drive
   it with."

Both halves were false and had been since 2026-09-20. design/blocks.yml,
design/console_inventory.yml (disposition pending_compose) and
design/contracts/GEOM.WARP.md all recorded the block as BUILT AND TESTED --
2,468 directed checks -- while this line denied it.

THE INSTRUCTIVE PART IS NOT THAT IT WAS STALE. IT NAMED THE WRONG BLOCKER.
Anyone reading it concluded the work was to BUILD a block that was already
built, and never looked for what was actually missing. A second sentence on the
same instantiation read the register correctly and drew the wrong conclusion
from it -- "GEOM.WARP is NOT BUILT AT ALL (the register's own list)" -- where
the register says BUILT BUT NOT CONNECTED, a different claim. Both corrected in
place, with what they actually said preserved.

================================================================================
GEOM.PARAMBUF IS NOT CLOSED BY THIS, AND IT WAS MEASURED RATHER THAN HOPED
================================================================================

The packet said composing Warp "plausibly" closes zhao_geom_parambuf. IT DOES
NOT. That block's blocker is an arena WRITER: every geometry memory client is
hard-coded read-only in source -- zhao_geom_meshfetch, zhao_geom_assetfetch,
zhao_geom_mem_adapter, zhao_geom_drawjob, zhao_geom_ladderbank,
zhao_geom_loomfeed -- spec/memory_rules.md 5f declares RENDER.ASSET_POOL
read-only WITH A FORMAL ASSERTION, and none of its four ledger counters appears
in any .sv. Warp is a vertex-stream deformer between SKIN and PROJECT; it
neither allocates the arena nor writes SDRAM. Treat the two as INDEPENDENT.

================================================================================
LANE CONTACT -- declared, as the packet asked
================================================================================

MY FILE SET TOUCHES FORMOWN's IN EXACTLY ONE PLACE, AND I DID NOT MAKE THE EDIT.
The composition needs the descriptor cookie to ride the job handshake through
zhao_geom_drawjob's SIDEW bundle (whose $fatal on SIDEW != 72 exists to make a
half-done layout move impossible to ship quietly), and zhao_geom_drawjob.sv is
FORMOWN's file. I stopped at zhao_cmd_exec and the console boundary and wrote
the reason where the next packet will read it -- in the boundary ports' own
comment and in the contract.

zhao_geom_clipread.sv, the packers and the goldens: NOT TOUCHED.
TERRASSEM's sp_* assembler: NOT TOUCHED.

Shared files I did edit, hunk-staged with `git diff --cached --name-only`
checked before every commit: spec/commands.zidl,
fpga/rtl/prod/zhao_console_core.sv. design/blocks.yml, design/prod_manifest.yml,
design/console_inventory.yml and tests/CMakeLists.txt: NOT EDITED -- no new
module, and the posed lane's counters are not in the ledger either, so following
that precedent avoided churn.

================================================================================
GATES, AND WHY THAT SET -- run bare, never piped
================================================================================

  npm run abi:check                clean, 35 outputs match     ABI TOUCHED
  check_console_inventory          OK, 367 / 217 elaborated    always
  check_prod_manifest              OK, 367 modules, 74 tops    always
  check_quartus17_syntax           OK, 580 files, 13/22 selftest  always; R212
  check_case_labels                OK, 49 blocks               always
  mutant_copy_drift                OK, 59 copies               always, AFTER commit (R121)
  mutant_drivers                   OK, 102 of 104 named        always
  uncashed_cheques                 RC 0                        always
  refmodel_liveness                RC 0                        always
  duplicate_functions              RC 0                        always
  completion_register              RC 1, 24 gaps               always; RC 1 normal
  wrapper_port_parity              FIRED, then 1292 = 1292     PORTS CHANGED
  check_counters                   RC 0 (reports, no gate)     always
  check_findings_citations         no new dangling             always
  packet_h_tieoff_audit            RC 0                        always
  gen_prod_top --check             fresh, 74 instances         PORTS CHANGED
  gen_console_board --check        fresh, 1297 core ports      PORTS CHANGED
  gen_shell_paired_diff --check    fresh, harness and mutant   PORTS CHANGED
  capture_diff --selftest          PASSED incl. rogue-byte     captures regenerated
  cmd_exec_directed                954 checks passed (was 850) BEHAVIOUR CHANGED, R60

wrapper_port_parity FIRED FIRST, naming all twelve new ports on both
console-core mutant wrappers -- the instrument working before its silence was
quoted. Repaired in both, respecting the line-ending trap the R229 commit
recorded (untex_decl is CRLF, slot_overflow is LF; one scripted patch across
both silently matches nothing in one of them).

All four generated artifacts were REGENERATED, never merged by hand.

CAPTURE FARE (R108) paid through the real producers, after capture_diff.py
--selftest was confirmed to FAIL on a planted content byte ("ROGUE 1 B OUTSIDE
every benign span [64..64]"). shell_golden --write regenerated z60_10frame,
storm_10frame and duo_10frame -- each 68 bytes, in exactly three spans (the
container CRC and the two 32-byte sha fields), VERDICT OK on all three.
demo_duo_markers --write covers the fifth and follows.

NO QUARTUS WAS RUN, as instructed.

================================================================================
WHAT THE NEXT PACKET SHOULD DO, IN ORDER
================================================================================

1. THE CARRIER. Widen zhao_geom_drawjob's SIDEW by a descriptor COOKIE (slot +
   generation, ~8-10 bits, NOT the 456-bit record -- 7.1 forbids copying the
   record through every stage), ride it to zhao_geom_assetfetch and add a
   v_side_o beside its per-vertex port, where src_q already proves the
   per-meshlet fanout works. DO NOT KEY IT ON src_id -- measured: that is
   capture_format.md 5's `index` ALONE, a compiler source-registry ordinal
   naming an emit SITE, shared by every draw from that statement, with
   kind/module truncated onto draw_src_truncated_o. W08 says the same from the
   other side: "source_id remains attribution, not identity." COORDINATE WITH
   FORMOWN.

2. THEN compose zhao_geom_warp + zhao_field_warp_adapter with .CLIENTS(3) and
   .INSTR_N(48) in ONE act. The splice: cut gs_v_* (u_geom_skin -> group_seq,
   internal wires, signed 32, clean) and the geom_sn_n_* group (u_geom_skin_norm
   -> u_light_skin_adapter, which are MODULE OUTPUT PORTS, not wires, s64 out
   against warp's s32 -- the light adapter's own narrowing stage becomes
   redundant). la_s_ready becomes warp's o_n_ready_i.

3. P8's MIDDLE LINK is what turns a reachable Warp into an ARMABLE one, and it
   is a FIELD packet, not a GEOM one. Without it, d_slot_i has no producer.

4. Repair zhao_field_loader's st_idx reply-identity fault FIRST -- a fit that
   measures a circuit already known wrong is wasted.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
