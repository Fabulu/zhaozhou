<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
7b7e3e48 -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- DELTALAW


## `7b7e3e48` -- FINDINGS-deltalaw -- the harness refuses a lane .md, so the record is here

```
FINDINGS-deltalaw -- the harness refuses a lane .md, so the record is here

Packet DELTALAW, owner ruling R231. Branch gz/deltalaw from 28ddaef9 (the head
of claude/ceiling-architecture-20260912, read with `git ls-remote`, never from
a tracking ref). Own worktree. Never rebased.

COMPLETION REGISTER: 22 -> 22, measured in MY OWN TREE at both ends
(9 tie-offs + 13 disconnected; RC 1 is normal). NO TIE-OFF CREATED, so there is
nothing to declare in the core's INCOMPLETE block. I32 did not close and could
not; the reason is section 7.

=============================================================================
1. THE FOUR PIECES ALL STILL HOLD, re-verified here rather than taken on report
=============================================================================
  1. SURFACE.STAMP.md S3, lines 108-113, verbatim as quoted.
  2. Its *Rejected:* line names the built branch, verbatim as quoted.
  3. `zref_terrain_page.hpp` -- `stamp_depth_at_vertex` returns
     `stamp_depth(strength[tj * kSheetEdge + ti])`. One plane. ABSOLUTE.
  4. `zhao_terrain_bake_v2.sv` -- `scar_sum = h_scar + delta16`. ACCUMULATING.

AND A FIFTH THE BRIEF DID NOT NAME, which is the whole defect in one line and
was sitting two lines from `scar_sum` the entire time:

    delta16 = c_sheet ? sd_depth_h16 : (g_from_c - g_to_c)

The DISC arm was always a delta. Two laws fed one accumulator and only one of
them differenced.

=============================================================================
2. WHAT WAS BUILT -- eight commits, 21 files, +1,888 / -30
=============================================================================
2671443d  the law: oracle, bake, seam, stamp, and the idempotence test
7f5f328a  the seam's handle check, the torn-flag fix, and a blind gate
e6146580  the law recorded in the spec; three stale sentences struck
c0961aed  a stamp arriving during a dig was lost SILENTLY
d87e0d91  the skid had the SAME defect it was built to fix, one layer out
c21d22e7  register the three new counters, which the ledger could not see
444dc68e  "empty" must mean the skid too -- a timing argument replaced by a gate
f099aa8d  the contract gains the skid, which the RTL had and the prose did not

* `zref::terrain::stamp_delta_at_vertex` -- a DIFFERENCE OF the ratified
  absolute law, not a second art table. The absolute form is NOT deleted: it is
  9.3(c)'s composed law and `stamp_to_bake_laws_directed` still holds it.
* `zhao_terrain_bake_v2` gains `sheet_before_i` and a SECOND
  `zhao_terrain_stampdepth` INSTANCE -- an instance, not a copy, because
  9.3(a)'s art table is a named editable constant set and there must be exactly
  one of it in this tree.
* `zhao_terrain_sheetseam` gains `sheet_before_o`, a 1,089x9-bit plane
  ({seen, before}) fed from a `stamp_results` SINK, and a one-deep skid.
* `zhao_surface_stamp` gains `res_handle_o` -- `st_handle` presented, not a new
  register and not a derived identity (SURFACE.SHEET choice C4).
* `zhao_console_core` gains `surf_res_handle_o`; `zhao_prod_top.sv` and
  `zhao_console_board.sv` regenerated; both mutant wrappers, the smoke bench
  and `zhao_terrain_bake_v2_mutant.sv` updated.
* spec/terrain_rules.md gains 9.3(d), the delta law.

`surf_res_before_o` -- THE PORT ENTRY I32 IS NAMED AFTER -- now has its first
consumer in this tree. PAGEIO had measured "ZERO CONSUMERS of `res_texel_i` /
`res_strength_i` / `res_before_i` in `fpga/` OR `tests/`". No longer true.

THE PLANE CLEARS ON CONSUME -- by the dig's own read. Two simpler schemes were
designed first and are rejected in the RTL header with their reasons: a SWEEP
has a 1,089-cycle window in which an arriving stamp is silently wiped (it
UNDER-digs, R221's refused "dig zero"), and an EPOCH TAG aliases, so an aliased
entry reads as seen with a stale `before` and DOUBLE-DIGS -- the very defect,
reintroduced by the instrument meant to prevent it.

NO COLD-START HOLE. A patch is only baked BECAUSE something stamped it, and
those stamps arrive at the sink first reporting `before = 0` on a fresh sheet.

=============================================================================
3. THE TEST -- and a counter is not the answer
=============================================================================
`tests/terrain/bake_delta_idempotence_directed.cpp`, BUILT AND RUN (R60):
**5,952 checks, 0 failures.**

Nine counters on the seam and six on the bake read CORRECTLY for the whole life
of the defect, because every one measures the ANSWER and the fault was in the
QUESTION. So it reads no counter to decide anything: it bakes, carries the scar
forward exactly as the page would, bakes again, and DIFFERENCES against zref.
R215's shape.

  [1] a first bake digs exactly what the absolute law dug (317 vertices)
  [2] IDEMPOTENCE -- the same stamp issued twice dug once
  [3] POSITIVE CONTROL -- `before` forced to zero DOUBLES the crater on the
      same 317 vertices. Case 2 is worthless without it: a test passing on an
      INERT machine looks exactly like one passing on a CORRECT machine.
  [4] 9.2 item 3's deferral identity: from->mid then mid->to == from->to
  [5] a deepened stamp digs the INCREMENT -- separating "correctly zero" from
      "always zero"
  [6] the disc law untouched; `sheet_vertices_dug_o` does not move

EVERY TEST AT THE FINAL COMMIT, built and run, no regressions:
  bake_delta_idempotence_directed   5,952  RC 0   NEW
  sheetseam_rtl_directed          81 -> 118 RC 0  (+37: cases 13, 14, 15, 16)
  terrain_bake_v2_sheet_directed    6,548  RC 0   unchanged
  terrain_bake_v2_directed            267  RC 0   unchanged (the disc law)
  terrain_stampdepth_directed       6,505  RC 0   unchanged
  stamp_to_bake_laws_directed         306  RC 0   unchanged
  surface_stamp_directed              107  RC 0   unchanged
  surface_stamp_chain                  34  RC 0   unchanged
  cmd_exec_directed                   850  RC 0   unchanged
  terrain_bake_v2_mutant_control            RC 0   fault present (160/1024 cells)
  terrain_bake_v2_mutant_negative           RC 1   WILL_FAIL target: 0/1024, correct

`terrain_bake_v2_sheet_directed` passing UNCHANGED **is** the cold-equivalence
proof: it leaves `sheet_before_i` at zero and gets the same 6,548 answers.

=============================================================================
4. FOUR DEFECTS I INTRODUCED AND CAUGHT -- all the packet's own shape
=============================================================================
Every one was GREEN on every gate, and every one was found by re-reading my own
code. The common signature: **a counter said the thing arrived, and the fault
was whether it LANDED.**

a. A RECORD COULD DIG FROM ANOTHER PATCH'S `before` PLANE. The plane is one
   patch deep and TEXEL-indexed and nothing checked whose data it held. Fixed
   with `bf_ok_c`. The seam's own section 6 record-swap defect, through the
   door R231 opened.

b. THE TORN FLAG COULD LATCH FOR THE LIFE OF THE MACHINE. Cleared only on a
   SERVED dig, while a torn record FALLS BACK and a fallback leaves `serve_q`
   low. The sheet law would have gone silently dead with every counter
   agreeing. It now clears on ANY retirement. Case 14 asserts the RECOVERY, not
   just the fallback -- a test written to the happy path would have missed it.

c. A STAMP ARRIVING DURING A DIG WAS LOST SILENTLY. `bf_q` has ONE write port
   and the dig's read must also WRITE (it clears `seen` on consume), so the
   store's `else if (sr_take_c)` could not run on a collision -- while
   `sr_take_c` was still TRUE, so `before_texels_o` counted it. Not rare:
   `rd_issue_c` runs about one cycle in four through a dig. Fixed with a
   one-deep skid.

   AND THE SKID THEN HAD THE SAME DEFECT ONE LAYER OUT: a result arriving while
   the skid DRAINS found the port carrying the older entry and went nowhere,
   counted again. Fixed with `sr_direct_c`.

d. "EMPTY" DID NOT MEAN THE SKID. A parked entry is counted in `bf_live_q` and
   not yet written, so a `bf_live_q` cleared at `bake_done_i` with the skid
   occupied would declare the plane free while a `seen` bit was on its way in.
   Very nearly unreachable -- which is the reason to make it structural rather
   than the reason not to, exactly as the block's own S_WATCH drain condition
   was, in the same words. One AND gate: `bf_empty_c`.

BOTH SKID DEFECTS WERE FIRED BEFORE THEY WERE FIXED:
  * case 15 against the pre-skid RTL (installed from 7f5f328a, measured, put
    back): `== 111 checks, 1 failures ==`, EXACTLY ONE -- the phase-0
    collision -- **with the `before_texels_o == 4` check PASSING beside it.**
  * case 16 against the unfixed skid: `== 118 checks, 1 failures ==`, again
    exactly one, again with `before_texels_o == 2` passing beside it.
The two-line signature is identical in both, and identical to R231's own
defect: the counter says N arrived, the plane holds N-1.

Case 15 could NOT see (c)'s second layer, and that is the part worth keeping:
it injects one result per collision and never two in consecutive cycles, so it
is a correct test of the COLLISION and structurally blind to the DRAIN. **A
test that covers a mechanism is not a test that covers the mechanism's own
edge, and the edge is where the second copy of a bug lives.**

(d) has no test case, deliberately, and the reason is stated rather than left
as a gap: I could not construct legal stimulus that reaches it, which is why
the guard is structural. A counter there would be one no legal input can move
and would owe a committed mutant -- and it is a statement about a composition
that does not exist yet rather than about a reachable state, which is the same
call this block already made for its two-reads-in-flight invariant (an
assertion, not a counter).

=============================================================================
5. TWO INSTRUMENTS FOUND BLIND
=============================================================================
a. `wrapper_port_parity` READ 1270 = 1270 AFTER I ADDED A PORT.

   A number that did not move after a change that must have moved it. I had
   inserted `surf_res_handle_o` with a PowerShell here-string that ate its
   trailing newline, so the core carried TWO DECLARATIONS ON ONE LINE. Verilog
   is whitespace-insensitive, so `gen_prod_top --check`, `gen_console_board`,
   `check_quartus17_syntax` and `verilator -Wall` were ALL HAPPY. The parity
   tool's port regex is LINE-ANCHORED, so it stopped counting the port -- and
   then reported wrapper and module EQUAL, because it could not see the
   difference it exists to find. THE GREEN WAS MANUFACTURED BY THE DEFECT.

   Now 1271 = 1271. THE NUMBER MOVING IS THE EVIDENCE that the gate can see the
   port at all. Follow-up worth having: the tool cannot distinguish "this port
   is absent" from "this port is on a line I did not parse", and both read as
   silence. Cross-checking its count against `gen_console_board`'s -- which
   parses differently and saw the port BOTH times -- would close it.

b. `check_counters` RETURNED RC 0 WITH MY THREE NEW COUNTERS ABSENT FROM ITS
   OUTPUT ENTIRELY. It reads `design/blocks.yml`, and its own closing line is
   the warning: "it REPORTS; it does not gate." A counter that exists in RTL,
   fires under stimulus and is asserted in a directed test was still INVISIBLE
   to the ledger. Registered in all three places blocks.yml needs; the tool now
   NAMES all three against their ports.

=============================================================================
6. THE STALE-BINARY TRAP FIRED THREE TIMES, and twice it read as a real red
=============================================================================
All three are documented in CLAUDE.md and all three still cost time.

1. RESTORING after case 15's fire test: `RESTORED_OK=True` on a content check,
   `LastWriteTime` forced to now, and the rebuild still printed **`ninja: no
   work to do`** -- so the "restored" run served the MUTANT's failure as a
   false red. Read as a result it says "the fix does not work".
2. Purging the two `Vtb_sheetseam.dir` partitions then produced **`ninja:
   error: rebuilding 'build.ninja': subcommand failed`** -- a stale graph
   unable to regenerate ITSELF.
3. `terrain_bake_v2_mutant_control` reported **"the mutant did NOT present its
   documented fault, so it is not evidence about anything"** -- which reads
   exactly like a botched mutant refresh. I re-derived the mutation set from
   `5cbe2e62` to check (a ONE-LINE diff; mine was faithful). After purging that
   target's partitions and reconfiguring: 160 of 1,024 layer-D cells disagree,
   fault present, RC 0.

The cure each time is the one CLAUDE.md names and it is NOT another
`cmake --build`: `cmake --preset windows-native`, then build.

ONE ADDITION TO THE FILE'S OWN ADVICE. It says to verify content and force the
timestamp. I did both and ninja skipped anyway -- the source file's mtime was
NEWER than the verilate output directory's. **Content-verifying a restore is
necessary and is not sufficient.** What settled it was purging the partition
directories and re-running to get a DIFFERENT answer. The assertion that works
is on the RESULT, not on the file: `sheetseam_rtl_directed` reporting 118
checks rather than 111 is what proves the binary is current.

=============================================================================
7. WHAT I DID NOT DO, AND WHY -- the second reader is STILL THERE
=============================================================================
The brief's item 2 was "remove the sheet re-read that S3 rejects". I did not,
and I do not believe it can be done as a seam repair. Stated plainly rather
than quietly:

**THE SHEET CANNOT TELL YOU WHAT A TEXEL USED TO BE.** Layer F holds `after`
and nothing else. Only the STAMP knows `before`. So the prefetch still reads
the sheet -- for `after` -- and the `before` plane comes from `stamp_results`,
which is the half S3 was actually about.

S3's "saves BAKE a second read port onto the sheet" is achieved only if BAKE
consumes `stamp_results` as delta EVENTS -- `scar[vertex(texel)] += d(after) -
d(before)`, scatter-driven, with layer B itself as the accumulator and no
plane, no skid and no residency at all. That removes the sheet read entirely,
and it is a REARCHITECTURE OF BAKE'S SHEET MODE, not a seam change: it replaces
a 33x33 lattice traversal with a texel stream. It cannot be built or tested
end-to-end today because neither block is composed and `cmd_*` has no producer.

The cost of leaving it is MEASURED, not assumed, and it is small:
`zhao_surface_sheetshare`'s round robin bounds both clients at one beat, and
SHEETSEAM measured the stamp waiting **0 cycles** with the prefetch going
1,091 -> 1,092. S3's rate-budget worry is real in principle and about 0.1% here.

RECOMMENDED for whoever builds the `cmd_*` record producer: take the scatter
form then, and delete the prefetch, the plane, the skid and `bf_empty_c` with
it. All four exist only to carry `before` across a gap that the scatter form
does not have.

=============================================================================
8. I32 -- THREE HOLDS SPENT, ONE FIELD GAINED, STILL OPEN
=============================================================================
VERIFIED IN MY OWN TREE, all four of the brief's claims:
  * `zhao_terrain_pageio.sv` is **63,729 bytes**.
  * `design/blocks.yml:2377` carries `- id: TERRAIN.PAGEIO`. Exact line.
  * it consumes `sc_*` and serves layer D (`cell_ci_i` in, `cell_state_o` out).
  * `cmd_exec_stamp_patch_w` is a real `logic [31:0]` in the core, so C4 IS
    satisfied by CARRYING and needs no new identity law.

**AND `cmd_depth_sheet_i` NOW HAS A PRODUCER** -- the seam's
`bk_depth_sheet_o`. So I32's "four absent fields" are THREE.

**I32 DOES NOT CLOSE.** `cmd_depth_from_i`, `cmd_depth_to_i` and `cmd_cells_i`
still have no source anywhere: every `depth_from`/`depth_to` hit under
`fpga/rtl` is bake's own port, its internal `c_from`/`c_to`, or
`zhao_terrain_bake_delta`'s. D-TERRCMD-B stands, and with it the refusal.
The register did not move and I do not claim it did.

=============================================================================
9. FIVE CORRECTIONS TO CLAIMS THIS PACKET WAS TOLD TO RELY ON
=============================================================================
a. "OPERATION 0 REPLACES THE TEXEL" IS WRONG -- it is `max(dst, src)`
   (`zref::surface::blend_of_abi_operation`, SURFACE.STAMP S1, and that
   contract's formal-properties section, which names REPLACE as the separate
   `kBlendReplace = 5` reachable only through `cmd_blend_en_i`). The sentence
   is in R231, in TERRAIN.BAKE.md and in entry I32. THE RULING IS RIGHT AND THE
   REASON GIVEN FOR IT WAS NOT, and the difference is not cosmetic: the stated
   reason implies idempotence UNCONDITIONALLY, the true one gives it for
   `src <= dst`. Corrected in all three places.

b. "A COMMITTED MUTANT (MUTATION 8) STANDING GUARD" -- mutation 8 is a row in
   SURFACE.STAMP.md's table headed "Before the rearchitecture (2026-08-21),
   kept for the record". A HISTORICAL sweep, not a live control.

c. "1,089 BYTES = ONE M10K" IS NOT MEASURED. An M10K is 1,024 WORDS deep in
   x8/x10 mode; the plane is 1,089 WORDS. Depth, not bit count, may force a
   second block -- and there are now two planes. FLAGGED in the contract and in
   the manifest row rather than decided, because this packet may not run
   Quartus. F-SHEETSEAM1 already carries `min_m10k` as well as `max_m10k`.

d. `stamp_depth(0) == 0` WAS UNDEFENDED BY ANY GATE. The delta law reduces to
   the absolute one exactly when it holds, so it is now `static_assert`ed. It
   removes no owner control: strength 0 means NOT STAMPED and there is no look
   decision about the depth of an absent scar.

e. THE PRICE R231 RECORDED IS LIGHT. "One more 1,089-byte M10K half" -- the
   plane is 1,089 x 9 bits, so 9,801 bits, plus a 32-bit handle, an 11-bit
   count, two flags and the skid. Right in blocks, light in bits.

AND ONE THING THE TEST FOUND ABOUT THE LAW ITSELF: the RTL converts each depth
to height16 SEPARATELY and then subtracts, so it rounds TWICE. That is what
makes 9.2's deferral identity EXACT -- every intermediate term of
h(d(mid))-h(d(0)) + h(d(to))-h(d(mid)) cancels as an integer, for any table and
any rounding rule. Rounding an fx16 difference once would give each deferred
step its own error. A driver differencing against this must rescale twice;
taking the convenient form would have been a test agreeing with itself.
Recorded at both ends.

=============================================================================
9b. THE SMOKE FORMS -- DERIVED, not enumerated, and one of them lied twice
=============================================================================
The list was derived from the script, as the brief instructs:

    awk '/^param\(/,/^\)/' tests/prod/run_console_core_smoke.ps1 \
      | grep -oE '\[switch\]\$\w+'

TEN switches. `-SkipVerilate` is a modifier, so the forms are NINE plus plain.
Confirmed in this tree; the brief's count is right.

  plain          RC 0   244 s   SMOKE: PASS, raster pixels=2560, frames_admitted=1
  Mutant         RC 0   265 s   SMOKE: MUTANT PASS -- terr_pl_slot_overflow_o fired 1 time(s)
  UntexMutant    RC 0   302 s
  NoTableLoad    RC 0   274 s   %Fatal = 1  <- INVERTED, and one is the pass
  BadDescriptor  RC 0   281 s   %Fatal = 1  <- INVERTED, and one is the pass
  BadVertex      RC 0   259 s
  NoEchoArm      RC 0   230 s
  BadTraceArm    RC 0   233 s
  GlowTag        RC 0   230 s
  LintOnly       RC 0     0 s   see below

R207 handled: the two inverted forms print EXACTLY ONE `%Fatal` each and a
clean log there would be the failure. No other form printed one.

`-LintOnly` IS NOT AN INVERTED CONTROL, and its zero is justified rather than
assumed (TERRCMD's standard, and all three of its bind-confirmations hold
here): it elaborates and never runs the console, so it has no verdict arm to
die in. Its verdict line is LintOnly-specific -- "LINT-ONLY:
tb_zhao_console_core_smoke elaborates. This says NOTHING about what the console
does." -- the plain run's markers are correctly ABSENT, and RC is 0 rather than
the 2 an unknown argument returns.

AND IT PRODUCED A ZERO-BYTE LOG WITH RC 0 IN ZERO SECONDS, which is the
broken-instrument signature exactly: good news manufactured by nothing
happening. It was NOT that. The form ran and passed; the script reports through
`Write-Host`, which `Out-File` on the success stream cannot capture, so MY
RUNNER lost the output. Run in the foreground the verdict line is right there.
**An empty log is not a result in either direction** -- and this is the third
distinct way my own harness produced a false reading in this packet, after the
nested-powershell truncation and the positional `$Repo` bind.

PROVENANCE OF THESE NUMBERS, stated because CLAUDE.md is strict about it: five
forms (BadDescriptor, BadVertex, NoEchoArm, BadTraceArm, GlowTag) verilated
AFTER the last change to any file in the `zhao_console_core` fit closure, so
they describe the final tree directly. The other four (plain, Mutant,
UntexMutant, NoTableLoad) verilated BEFORE commit e6146580, which edited
`zhao_console_core.sv` -- COMMENTS ONLY, but the tree moved under them, and a
suite whose inputs moved is not evidence in either direction. **Those four were
therefore re-run at the final tree**, and it is the re-run that is quoted:

  plain          RC 0   260 s   SMOKE: PASS, px=2560, frames_admitted=1
  Mutant         RC 0   282 s   SMOKE: MUTANT PASS
  UntexMutant    RC 0   258 s   SMOKE: MUTANT PASS
  NoTableLoad    RC 0   250 s   %Fatal = 1, the inverted control's pass
Nothing since has touched a file in that closure: the later commits are the
seam (not in the closure -- checked, the 222-source list has no
`zhao_terrain_sheetseam`), `design/blocks.yml`, and contract prose.

=============================================================================
10. GATES -- every one RUN, and why this set
=============================================================================
The change is RTL BEHAVIOUR plus a PORT change on the core, so both gate sets
apply in full (R227: a gate earns its runtime by being able to change its
answer; every one of these could, and two of them did).

ALWAYS -- all RC 0:
  check_console_inventory   check_prod_manifest    check_quartus17_syntax
  check_case_labels         mutant_copy_drift      mutant_drivers
  uncashed_cheques          check_counters         refmodel_liveness
  duplicate_functions       packet_h_tieoff_audit
  wrapper_port_parity       1271 = 1271, missing 0, stale 0
  completion_register       RC 1 (normal), 22

PORTS CHANGED, so also -- all RC 0. These live in TWO directories,
`tools/quartus/` and `tools/design/`, which is the false red the brief warns
of; both paths verified before reading any exit code:
  gen_prod_top.py --check          (regenerated first, output READ)
  gen_console_board.py --check     (regenerated first, output READ)
  gen_shell_paired_diff.py --check
  gen_shell_paired_diff.py --check --mutant

`mutant_copy_drift` was run AFTER the commit (R121): 57 copies checked, OK.

LINT -Wall RC 0: sheetseam, bake_v2, surface_stamp, bake_v2_mutant.

NOT RUN: Quartus. PACKET-PROTOCOL.md forbids it and the protocol outranks any
brief, including the one that sent me.

=============================================================================
11. A RUNNER THAT LIED THREE TIMES, never the bench
=============================================================================
* A nested `powershell -NoProfile -File <runner>` truncated EVERY smoke log at
  three lines and reported RC 1 in ONE SECOND for all ten forms -- the
  documented "could not verilate" signature exactly. Run in the foreground the
  same form passes in 244 s. THE DURATION TOLD THE TRUTH BEFORE THE EXIT CODE
  DID, and the exit code was reporting the wrong process.

* Passing the switch as the STRING "-Mutant" binds POSITIONALLY to the script's
  `$Repo`. The script's own guard caught it and refused -- and the refusal
  names `...\zhaozhou-ceiling-lane-20260912\-LintOnly\design\fit_targets.yml`,
  THE COORDINATOR'S CHECKOUT, because a relative `$Repo` resolves against
  `[Environment]::CurrentDirectory`, which `Set-Location` never updates.
  CLAUDE.md's `[IO.File]` trap, live, in a READ rather than a write. Splat it
  (`& .\run.ps1 @{Mutant=$true}`) and it binds. The plain run was unaffected --
  `$Repo` defaults from `$PSScriptRoot` -- and the proof it read MY tree is
  that its first attempt failed on MY missing port.

* `-LintOnly` produced a ZERO-BYTE log with RC 0 in ZERO seconds, which is the
  broken-instrument signature exactly. It was not that: the script reports
  through `Write-Host`, which `Out-File` on the success stream cannot capture.
  The form ran and passed. **An empty log is not a result in either direction.**

=============================================================================
12. SHARED FILES
=============================================================================
`zhao_console_core.sv`, `tests/CMakeLists.txt`, `design/blocks.yml` and
`design/prod_manifest.yml` were all touched. `git diff --cached --name-only`
was read before every commit and each one carried exactly my own files. I work
in my own worktree, which has its own index, so no other lane's staging was
disturbed in either direction.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
