# FINDINGS -- carriers lane (gz/carriers)

*The harness blocks subagents from writing report files, so this packet put its
report in a COMMIT MESSAGE. Transcribed here verbatim by the coordinator, because
files under `fpga/rtl/` cite this path and a citation to a file that does not
exist is an uncashed cheque one level down. Source commit: `4f438e62`.*

---

FINDINGS-carriers: I34 refused with a SECOND, larger blocker, measured

The harness blocked writing FINDINGS-carriers.md into the run folder, so this
commit message IS the findings, as PACKET-PROTOCOL.md allows. The durable half
-- the I34 analysis and its owner decision -- is in this commit as the entry's
own text in `zhao_console_core.sv`, because a run folder is the wrong home for
anything durable.

REGISTER: 22 at e0484b78, 21 at the last pushed commit.
(10 tie-offs + 11 disconnected + 1 unbuilt -> 9 + 11 + 1. The brief said 22 and
it read 22 -- the first brief number this run that was not stale.)

  32abe8d9  I50 part 1 -- the carrier, its record, its directed test, its mutant
  1355438e  I50 CLOSED -- composed, .N(4) -> .N(5), the bench staged in DDR
  (this)    the I34 write-up in the core header

================================================================
GAP CLOSED -- I50, GEOM.LOOM's node stream and camera basis
================================================================
Rulings R58 and R69, on the owner ruling of 2026-08-31 6.4. Full argument is in
1355438e's message and in design/contracts/GEOM.LOOM.STREAM.md. The two things
worth repeating here:

TWO LAWS WERE BACKWARDS IN THE FIRST DRAFT. Both wrong versions were the
intuitive ones and both would have lost a whole frame of poses IN SILENCE.

  * Law 4, "the loom refused, so stop feeding it", is wrong. zhao_geom_loom's
    S_DRAIN keeps in_ready_o HIGH and SWALLOWS beats until the stream's own
    `last`. A carrier that stopped would leave it draining and the NEXT stream
    would be eaten to ITS `last` with no refusal raised at all.
  * Law 6, keyed on seeing a FRAMING refusal, covers S_RUN and misses S_DRAIN
    entirely -- a draining loom raises no refusal, so the trigger never comes
    and the stream is reported `ok` while every beat is swallowed. It is keyed
    on the ABANDONMENT now.

THE REAL LOOM IS WHAT CAUGHT BOTH. The DUT is the carrier with zhao_geom_loom
behind it, not a mock, deliberately: a mock would have been written to agree
with whatever the carrier does. Cases 5 and 8 each assert THE STREAM AFTER THE
BAD ONE STILL COMPOSES, which is the assertion the wrong version fails.

================================================================
GAP REFUSED -- I34, and the blocker that matters is NOT the recorded one
================================================================

(A) THE STATED BLOCKER HOLDS, re-checked rather than inherited. `program_hash`,
`prog_hash`, `programHash` across all of fpga/rtl including synth/: ZERO hits.
zhao_field_host's own header says the other half -- "Nothing inside the console
loads a field program ... its owner is CMD.EXEC's TerrainField 0x0200 arm ...
That arm is not built."

A PRESENCE CLAIM WAS CORRECTED, which is the more dangerous shape. The recorded
build list implies `fld_add_hash_i` is a key TERRAIN.PATCH uses. zhao_terrain_
patch.sv:125-126 marks it and `fld_add_cmd_i` "trace only" and the block keys
nothing on either -- its list is FOOTPRINTS. So the mapping is needed to let the
ADAPTER resolve a slot, not to let the patch accept a lane.

BIND holds and costs nothing at the ABI: post_op_i is [1:0] and 2'd3 is unspent.
The cheaper BIND {handle32, SLOT} -- the doorbell's return already hands software
the slot -- is REJECTED: a slot can be EVICTED after the bind and software's copy
goes stale in silence. The hash survives eviction because the DIRECTORY is the
authority on residency.

(B) THE LARGER BLOCKER. THE COMPOSED FRONT IS THE ARRANGEMENT FIELD.SEQ.EARTH's
OWN CONTRACT EXCLUDES, IN WRITING. FIELD.SEQ.EARTH.md:105 --

  "The v2 generic 12-in/4-out host stream (27,225 clocks/association measured
   against a 10,416 allowance) is not part of this profile's production path."

This console composes a generic per-point front of that family, and at these
parameters it is WORSE per point:

  * zhao_field_host's E_ZERO is REGS clocks PER POINT (the file says "REGS
    clocks, once per point"; the console's instantiation repeats it) and E_WRITE
    is IN_LANES+1. At the composed REGS=32 / IN_LANES=13 that is >= 46 clocks of
    transport per point BEFORE ONE INSTRUCTION, against the v2 front's 25.
  * One Earth association is 1,089 lattice vertices (33x33) and the adapter owes
    ONE RUN PER COVERING LANE PER VERTEX.
  * ONE lane over ONE patch is >= 1,089 x 46 = 50,094 clocks against the 10,416
    allowance -- about 481%, transport alone. The v2 arrangement Fieldv3.md
    condemned was 261%.

All four recorded build items can be built correctly and still miss the declared
allowance by ~5x. Same lever entry I5 already records on the particle seam, so
one repair serves both.

NOTHING WAS BUILT FOR I34, deliberately. Building the adapter against a front
about to change shape, or composing any one item alone, would dangle at the core
boundary and put the register UP.

================================================================
OWNER DECISION FOUND -- the Earth field-height rate
================================================================
  1. ACCEPT for v1 -- it exists and does not fit its budget. Makes the allowance
     a lie.
  2. TAKE R44 LITERALLY and swap TERRAIN.PATCH for the probes' field-major
     four-wide whole-patch architecture. The ~481% is what that call is about.
  3. RECOMMENDED -- give the front the "same program, same uniforms" fast path
     the contract already asks for. FIELD.SEQ.EARTH.md: "Uniform lanes live in
     the scalar bank, loaded ONCE per association"; the front zeroes and reloads
     all of them PER POINT instead. Skipping E_ZERO and rewriting only the
     varying lanes takes 46 clocks to ~3 -- 1,089 x 3 = 3,267 per association,
     INSIDE the allowance -- with no new architecture and no second reducer.
     CAVEAT, named rather than discovered: E_ZERO exists because "a program that
     reads a register it did not write would read that point's value". The fast
     path is sound only for a program whose read-before-write set is empty, or
     if the zeroing is narrowed to that set -- which zfield::decode can declare,
     because it is software and already walks the image. Bounded work.

================================================================
INSTRUMENT DEFECTS FOUND
================================================================
1. THE SLOT-OVERFLOW MUTANT WRAPPER WENT PORT-STALE AGAIN AND FAILED LOUDLY.
   tests/mutants/zhao_console_core_slot_overflow_mutant.sv could not elaborate
   against the core's new ports -- the CORRECT failure direction for a wrapper
   and exactly what its header predicts, the opposite of a stale COPY which
   fails by continuing to pass. Its port block was re-lifted from production
   VERBATIM BY SCRIPT rather than hand-transcribed. -Mutant fires again. Second
   consecutive pass in which the only thing that ran it was the packet that
   broke it.

2. design/console_inventory.yml IS CRLF AND THE REST OF THE TREE IS LF. An
   insert anchored on "...:\n" matched ZERO times and the assertion caught it.
   This is R61's hazard from the other side: the trap is not only that a lone CR
   becomes a line break, it is that LINE ENDINGS ARE NOT UNIFORM ACROSS design/,
   so a script anchoring on \n silently misses in some files and matches in
   others. Every edit script in this packet detects the file's own newline first.

================================================================
FALSE-ABSENCE CLAIMS
================================================================
None asserted, and the one I checked hardest was TRUE: the FIELD lane's "no
handle -> hash producer anywhere in fpga/rtl" survived the same three searches
plus synth/. Worth recording -- this run has corrected five inherited causes, and
it is not always the refusal that is wrong. The check stays cheap either way.

================================================================
GUARDS ADDED, AND WHETHER EACH WAS SEEN TO FIRE
================================================================
  geom_loom_feed_post_stalls_o     stimulus  case 6 -- the return port is SHUT
                                             so law 7's credit stops the drain,
                                             the mailbox fills, post_ready_o
                                             goes low. Three posts was not
                                             enough (the drain kept up); eight is
  geom_loom_feed_align_refused_o   stimulus  case 2, with bursts_o asserted
                                             UNCHANGED -- not one burst issued
  geom_loom_feed_hdr_refused_o     stimulus  cases 3 and 4 (magic, zero count,
                                             over-count), each with bursts_o
                                             asserted to move by exactly one
  geom_loom_feed_refused_o         stimulus  case 5, loom's own NOT_SORTED out
  geom_loom_feed_bridge_errs_o     stimulus  case 7, one transient refusal and
                                             the stream STILL composes
  geom_loom_feed_faulted_o         stimulus  case 8, four consecutive refusals
  geom_loom_feed_replayed_o        stimulus  case 8b, corroborated independently
                                             by the LOOM's own FRAMING counter
                                             firing on the flush pass
  geom_loom_feed_wait_cycles_o     stimulus  case 9 -- a zero would mean the rate
                                             instrument is not wired
  geom_loom_feed_ret_overflow_o    MUTANT    zhao_geom_loomfeed_mutant.sv +
                                             geom_loomfeed_mutant_control
                                             (inverted): posts=8 streams=8
                                             ret_overflow=1
  (its negative control)           stimulus  case 10 asserts 0 through all nine
  terr_pl_slot_overflow_o          wrapper   -Mutant smoke, fired 1 time

Both halves are present for every counter asserted zero.

================================================================
GATES AT THIS COMMIT, all run in this worktree
================================================================
  completion_register.py            21 (RC 1, normal), down from 22
  check_console_inventory.py        OK
  check_prod_manifest.py            OK (71 instances)
  gen_prod_top.py --check           fresh
  gen_console_board.py --check      fresh (1231 core ports)
  mutant_copy_drift.py              OK -- no new drift
  check_quartus17_syntax.py         RC 0 (534 files)
  gen_shell_paired_diff.py --check  fresh
  check_case_labels.py              OK
  console-board lint (waived set)   silent RC 0
  run_console_core_smoke.ps1        PASS, raster pixels=2560, frames_admitted=1
    -Mutant                         PASS, terr_pl_slot_overflow_o fired 1 time
    -BadVertex / -NoEchoArm / -BadTraceArm   PASS
  cmd_exec_directed (R60)           677 checks passed
  geom_loomfeed_directed            75 checks passed
  geom_loomfeed_mutant_control      3 checks passed (inverted polarity)
  geom_loom_directed                31,697 checks passed

NOT RUN: Quartus. The coordinator fits at completion. What this packet adds is
area-relevant and UNMEASURED, and the honest statement is that it is unmeasured.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>

