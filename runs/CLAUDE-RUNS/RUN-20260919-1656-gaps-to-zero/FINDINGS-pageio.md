# FINDINGS — PAGEIO (TERRAIN.PAGEIO: the row, the block, and 21 -> 22)

**Branch gz/pageio, 8 commits, head 294c5c01. Register 21 -> 22.**
**The rise is the instrument starting to work (owner ruling R214).**

> TRANSCRIBED BY THE COORDINATOR — the harness refused this lane a report
> .md, the ninth in a row.

---

## 9471cc0c

PAGEIO: give TERRAIN.PAGEIO a design/blocks.yml row, BEFORE the RTL

Owner ruling R210: TERRAIN.PAGEIO has a written contract and no ledger row,
so no gate can see that it is missing. tools/budget/completion_register.py
walks design/blocks.yml, so a capability with NO ROW is not absent from the
console -- it is absent from the QUESTION. The register's 21 was never wrong.
It was answering a smaller question than its readers assumed.

The row lands FIRST, before any RTL, because until it exists a block built
here would close nothing any instrument can see, and there would be no way to
demonstrate progress.

MEASURED, in this tree, before and after:

  before   MANDATORY GAPS REMAINING : 21   (9 tie-offs + 12 disconnected
                                            + 0 unbuilt + 0 uncited
                                            + 0 unresolvable)
  after    MANDATORY GAPS REMAINING : 22   (9 tie-offs + 12 disconnected
                                            + 0 unbuilt + 0 uncited
                                            + 1 UNRESOLVABLE)

The rise is the instrument starting to work, not a regression. It lands as
UNRESOLVABLE rather than UNBUILT because resolve_module() constructs
zhao_terrain_pageio by convention and RTL.rglob finds no such file; the
register counts an unresolvable capability AS A GAP and says so. Building
fpga/rtl/terrain/zhao_terrain_pageio.sv moves it to BUILT BUT NOT CONNECTED,
which is also a gap and also correct; only composition retires it.

TWO TRAPS IN blocks.yml, both avoided deliberately:

  * R179, implementation: is LOAD-BEARING. successor_in() requires a module's
    tail to fullmatch v\d+, so a rival implementation -- anything not _vN --
    is structurally invisible to the register. The module is therefore named
    zhao_terrain_pageio, with no version suffix and no implementation: key,
    so the naming convention resolves it directly. Only 9 of 98 rtl rows carry
    implementation: and ledger_blocks()'s own docstring says it is not usable
    as the resolution source.
  * R180, upstream: is DESIGN INTENT, not a wiring claim. The four names on
    this row's upstream: (TERRAIN.SEQ, TERRAIN.RESIDENCY, MEM.GUARD,
    TERRAIN.BAKE) are intent. NONE of them is a port on anything today,
    because the block does not exist yet. Read it as the dataflow graph the
    architecture wants, never as a statement about the tree.

TWO FINDINGS RECORDED IN THE ROW'S notes:, both re-verified here rather than
inherited:

1. RULING T4 ALREADY ANSWERS THE CONTRACT'S "OWNER DECISION 1", so it is not
   an owner decision. The contract asks whether layer B is persisted through
   the HPS journal or whether a direct pool write is the whole of it, and
   calls it the owner's sentence to write. zhao_terrain_writeback.sv's header
   states T4 as "B and D are NEVER written back (the HPS keeps the canonical
   mirror current from the same deterministic commands)", and gives that as
   the reason layer F -- which has no canonical mirror -- must go behind an
   ACK barrier while B and D must not. The pool write is the whole of it: no
   second doorbell, no second ticket space. The contract's own recommended
   reading is the ruled one.

2. THE CONTRACT'S SECTION 4 RECOMMENDATION OF BYTE ENABLES IS STRUCTURALLY
   UNAVAILABLE. Section 4 says "Byte enables are preferable and the guard
   request type already carries .be". The guard REFUSES a sparse .be:

     zhao_mem_guard.sv:  be_ok = (req.be == mask_of(req.len));
                         shape_ok = len_ok && be_ok;

   and its header says byte_enable "must be the FULL contiguous mask over
   [addr, addr+len) (Phase-2 clients issue whole spans; partial-word masking
   is NOT in the Phase-2 arbiter)". zhao_vram_arbiter.sv confirms it from the
   other side -- it converts len to WORDS (words_of(client_req[k].len)) and
   the SDRAM controller never sees a byte mask at all. So the VRAM write
   granularity in this machine is a 64-bit WORD, and a sparse be is a guard
   VIOLATION rather than an optimisation.

   The edge handling is therefore a read-modify-write -- and it is SMALLER
   than the contract feared. Not the edge BURSTS: the edge WORDS, of which
   there are exactly two per plane.

     layer B [2242, 4420):  first word 2240 carries layer A's last 2 bytes
                            last  word 4416 carries layer C's first 4 bytes
     layer D [6598, 7622):  first word 6592 carries layer C's last 6 bytes
                            last  word 7616 carries layer E's first 2 bytes

   Four shared words per bake; everything between them is wholly owned. The
   directed test section 4 asks for -- neighbouring layers byte-identical
   after a bake -- is the acceptance test for exactly this.

Also adds eight pageio_* names to counter_catalog, named apart from writeback_*
because they measure a different pool access: writeback evacuates layer F to
the HPS journal, this reads layer D and writes layers B and D back into the
TERRAIN.PAGE_POOL slot in place.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## bd68c951

PAGEIO: build zhao_terrain_pageio, and lift the section 3.3 shadow into zref

The block TERRAIN.PAGEIO's contract describes: bake's page window, layer D in,
layers B and D out, inside one resident TERRAIN.PAGE_POOL slot. It serves the
FOUR zhao_terrain_bake_v2 page ports that nothing in the machine served.

RE-VERIFIED IN THIS TREE, not inherited (R165: a blocker is a claim about a
moment). All four of SEAMDIG's absence findings still hold:

  * LAYER D HAS NO READER. Every `6598`/`6,598` under fpga/ is the area figure
    "6,598 ALM" in three headers, or a COMMENT recording a previous instance of
    this same search. zhao_terrain_pagestream reads '{A_OFF, B_OFF, C_OFF};
    zhao_terrain_pageloader writes whole pages and reads none back;
    zhao_terrain_writeback touches layer F only.
  * ZERO CONSUMERS of res_texel_i / res_strength_i / res_before_i in fpga/ OR
    tests/ -- entry I32's named consumer does not exist as a port at all.
  * vtx_nobake_i HAS NO PRODUCER. The only non-comment hits are
    zhao_prod_top's generated stimulus fold (`u59_src[140 +: 1]`).
  * cmd_* HAS NO PRODUCER. zhao_terrain_cmd emits rec_island_o / rec_ix_o /
    rec_iz_o / rec_hps_addr_o / rec_crc_o / rec_flags_o -- a patch DIRECTORY
    record. Nothing resembling bake's cmd_cx/cz/radius/depth_from/depth_to.

THE ONE LAW THE BLOCK OWNS IS LIFTED INTO THE ORACLE FIRST.

Contract section 3 warns: "the only standalone statement of that reduction
today is a TEST helper, tests/terrain/bake_dev.hpp:nobake_shadow(). Building
RTL against a test helper is how a second implementation of a ratified law is
born." It was worse than that -- there were already TWO copies, the test
helper and an inline loop inside bake_dig() in terrain_core.cpp. Both now call
`zref::terrain::nobake_corner_shadow`, and so does the RTL's nbv_q. One
statement, three callers (charter 29-6). g++ -fsyntax-only clean.

AND THE RTL BUILDS IT BY SCATTER, WHICH IS THE SAME LAW BY A DIFFERENT ROUTE.
The oracle GATHERS -- for a vertex, look at its up-to-four corner cells. nbv_q
SCATTERS -- for each protected cell, set the four vertices it corners. Every
(vertex, cell) incidence is visited exactly once either way. The scatter is
1,024 single-cell steps with no multi-port read; the gather would need four
random reads of a 1,024-entry plane on every vertex. That equivalence is an
ARGUMENT and the directed test checks it rather than believing it.

DESIGN DECISIONS, each stated so it is reversible in one place:

  * FORM (b), the contract's own recommendation on ruling R59, WITH its
    "further halving" taken. dwrd_q (136 x 64 = 8,704 b, ~1 M10K) is ONE
    buffer serving layer D's read AND write; bwrd_q (280 x 64 = 17,920 b,
    ~2 M10K) is the layer-B write window. ~3 M10K total, against form (a)'s
    ~8. At 306 of 553 M10K used (R87) that is not a formality.
  * BOTH BUFFERS ARE 64 BITS WIDE because a guard read beat cannot be stalled.
    Byte- and halfword-granular access from the bake face is therefore a
    two-cycle read-modify-write, which is free against bake's ~20-cycle
    per-vertex spine.
  * THE NO_BAKE SHADOW IS IN FLOPS (1,089 bits), not a RAM, because nb_o must
    answer any (vi,vj) COMBINATIONALLY -- the page server delivers
    base/scar/bottom/nobake on one beat or the vertex is not ready, and in
    form (b) this block does not own that valid. It is this block's one
    flop-heavy structure, declared in the header as the first thing to revisit
    if the fit says the block is too big. The cheaper alternative (a two-row
    window, 66 flops) is written down and NOT taken: it is only correct for a
    cursor that scans z-then-x, so it trades an area number for a coupling to
    TERRAIN.BAKE's traversal order.
  * slot_scaled() is COPIED VERBATIM from zhao_terrain_pagestream.sv, per the
    contract: "A fifth spelling of the same thing is a fifth thing that can be
    subtly different." 21,376 = 2^14+2^12+2^9+2^8+2^7, no DSP.
  * THE TWO-CYCLE GUARD VERDICT LAW is copied from zhao_terrain_writeback:
    *_REQ waits on rsp.ready (a LEVEL), *_VERD reads .ok/.violation one cycle
    later. Testing them in one arm reads every pass as a denial, silently --
    the defect found in both geometry fetchers on 2026-09-06.

THE METADATA-SWAP DEFECT IS DESIGNED OUT, NOT COUNTED.

cell_ci_i/cell_cj_i are bake's LIVE cursor and advance the moment bake accepts
a cell, while this block may still hold cell_valid_o for the previous one.
Delivering the held byte against the moved cursor is CLAUDE.md's exact
record-swapping defect: cell A's data under cell B's address, every handshake
and every counter agreeing. So cell_valid_o is gated on the address LATCHED AT
READ ISSUE, and the two sides of that comparison are loaded by DIFFERENT
enables -- the offered pair by bake, the latched pair by this block -- so it is
not the lockstep-blind kind of check. A cursor that moves under a pending read
drops the answer, re-issues, and counts cell_refetch_o.

A PARTIAL PLANE IS NEVER WRITTEN. zhao_terrain_bake_v2 raises sc_valid_o at
StEmit for EVERY vertex (sc_touched_o is what says which were inside the
stencil), so anything short of 1,089 means bwrd_q's interior still holds the
PREVIOUS bake's words. Writing it would corrupt the page under a clean
handshake, so the write is REFUSED with V_SHORT_B and the job completes with
done_ok_o low. Layer D is all-or-nothing the same way: 1,024 or 0 (a record
with cmd_cells_i low runs no breach phase, which is legal), never in between.

ELEVEN COUNTERS, and what fires each is written beside it. nobake_mutated_o is
the interesting one: TERRAIN.BAKE preserves bits 7:2 (cs_state_o <=
{cell_state_i[7:2], sub_out}), so under a legal bake it is UNREACHABLE and
reads zero -- which is a claim, and the claim to check hardest. It is fired by
STIMULUS at this block's own port (presenting a cs byte whose kNoBakeBit
disagrees with the plane), not by a mutant, because at this boundary that is
legal stimulus. The byte still lands: narrowing a write is how a silent
divergence is made permanent, and the counter is what makes it visible.

GATES ON THIS COMMIT:
  verilator --lint-only -Wall            0 warnings, 0 errors
  check_quartus17_syntax.py              556 files, no rejected forms
  g++ -fsyntax-only on terrain_core.cpp  clean
  completion_register.py                 TERRAIN.PAGEIO moves UNRESOLVABLE ->
                                         BUILT BUT NOT CONNECTED. Still a gap,
                                         correctly: total stays 22. Only
                                         composition retires it, and
                                         composition needs TERRAIN.BAKE
                                         composed, which this block does not
                                         and must not decide.

Every elaboration check is inside `initial begin ... end` and there are no
implicit generates, because both forms lint clean under Verilator with zero
diagnostics and are SYNTAX ERRORS in quartus_map. And `--lint-only` does not
run `initial` blocks, so the clean lint above says nothing whatever about
those checks -- the directed test's parameterised build is what elaborates
them.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## d996d078

PAGEIO: pageio_rtl_directed -- 79 checks, and it found two real defects

R60: a directed test must BUILD AND RUN, not merely lint. This one builds and
runs (79 checks, 0 failures) and it earned its keep on the first execution.

TWO DEFECTS IT FOUND IN THE RTL COMMITTED ONE CHANGE AGO. Both are fixed here,
and both are the kind that no counter and no lint could have seen.

1. A READY THAT DOES NOT ACCEPT. `sc_ready_o` was

       (state_q == S_SERVE) && (op_q == OP_NONE)

   which is true in the very cycle the serve arbiter takes the CELL branch
   instead -- the cell read has priority, because TERRAIN.BAKE BLOCKS on it
   while a write merely backpressures. A producer that read the level and
   advanced lost that beat SILENTLY. It cost exactly one scar word per bake:
   1,088 of 1,089.

   The instructive part is what the machine then did. `sc_seen_q` came up one
   short, the partial-plane guard refused the whole page write with V_SHORT_B,
   and the test reported a REFUSED BAKE -- a correct verdict about a defect two
   hundred lines away. The guard worked; nothing about the failure pointed at
   the handshake. Each ready now carries the arbiter's own priority, and the
   comment says why the resulting ready-depends-on-valid cannot deadlock (both
   of bake's valids are registered).

2. `bake_done_i` ABORTED AN IN-FLIGHT WRITE. It is a one-cycle pulse, the last
   cs write of a record is a two-cycle read-modify-write, and
   `zhao_terrain_bake_v2` raises `bake_done_o` one cycle after its last
   handshake -- so the pulse lands mid-RMW. The retirement arm set
   `op_q <= OP_NONE` and the write was dropped.

   THE RESULT WAS 1,023 OF 1,024 CELLS. The page write completed, the
   deformation mark was published, done_ok was high, and every counter agreed.
   One cell kept its old substance -- which is one wrong breach decision, the
   exact fault class contract section 5 says no counter can see. The pulse is
   now LATCHED (`bake_pend_q`) and acted on when the buffer is quiet, and the
   serve arbiter starts nothing new once a record is retiring, because a
   DROPPED sc accept would also have incremented `sc_seen_q` -- which is how a
   lost write becomes an unnoticed one.

A third change is a correctness fix the bench design surfaced before it ran: a
held cell answer is now invalidated when a cs write targets the SAME cell.
`dwrd_q` is one buffer for both directions, so a write changes the byte a held
answer was read from. TERRAIN.BAKE's own order never re-reads a written cell,
which is precisely why this could have sat here wrong indefinitely -- invisible
under the one traversal the machine uses, and a promise the block makes to any
traversal. The bench drives the re-read and checks the NEW byte comes back.

WHAT THE BENCH CHECKS, and why each one is not a count:

  * LAYERS A, C AND E ARE BYTE-IDENTICAL after a bake. This is the headline.
    The guard has no byte enables, so a 64-byte burst is the smallest writable
    thing, and layer B's and layer D's edge bursts carry 2, 4, 6 and 58 foreign
    bytes. NOTHING IN THE MACHINE READS THOSE LAYERS BACK -- a block that
    clobbered them would pass every other gate forever. The bench reads the
    page image back out of the played fabric and compares byte for byte,
    including the 64-byte header, layers F/G/H, and all three neighbouring
    slots.

  * `nb_o` MATCHED `zref::terrain::nobake_corner_shadow` ON ALL 1,089 VERTICES.
    The RTL SCATTERS (each protected cell sets the four vertices it corners);
    the oracle GATHERS (each vertex looks at its four cells). "Those are the
    same law" is an argument, and this is where it is checked. The fixture puts
    kNoBakeBit on ~8% of cells, which lands the shadow near 28% of vertices --
    a mix, rather than the all-ones a 50% plane would give, which would pass
    against an `nb_o` tied high.

  * EVERY CELL READ RETURNED THE ORIGINAL BYTE. Contract section 5's "further
    halving" -- one layer-D buffer for both directions -- rests on the breach
    phase never re-reading a written cell. The breach drive advances the cursor
    AT THE ACCEPT, exactly as bake does, so the block is reading cell k+1 while
    the cs write for cell k is still in flight.

  * A REFUSAL IS NOT A PARTIAL WRITE. The short-layer-B case ends the dig sweep
    one vertex early and asserts NOT ONE burst was written and the page is
    byte-identical.

FIVE COUNTERS, EACH PROVED SILENT AND THEN FIRED:

  counter             silent on the clean bake     fired by
  guard_denied_o      yes                          cfg_deny_mode on request 0
  stale_gen_o         yes                          a job epoch != cfg_epoch_i
  jobs_refused_o      yes                          slot 1024, and short layer B
  nobake_mutated_o    yes                          a cs byte with kNoBakeBit flipped
  cell_refetch_o      yes                          the cursor moved under a held answer

None needs a committed mutant. At THIS block's boundary the illegal input is
ordinary stimulus, because the bench stands where TERRAIN.BAKE will stand --
which is the distinction between `wq_overflow_o` (unreachable while the
full-guard is correct, so it needs `tests/mutants/`) and these.

`nobake_mutated_o` also proves its own policy: the mutated byte LANDS, and the
test asserts it landed. Narrowing a write is how a silent divergence is made
permanent; the counter is what makes it visible.

AND THE DETECTOR WAS SHOWN TO FIRE. A mutant of this block that SKIPS the two
layer-B edge reads was built and run in the scratchpad -- never in the tree, so
there was no live-tree hazard and no Copy-Item restore to get wrong. It failed
with:

    bursts_read (17 D + 2 B edge)   expected 19, got 17
    LAYER A is byte-identical       expected 0, got 2
    LAYER C is byte-identical       expected 0, got 60

EXACTLY 2 and EXACTLY 60 -- the counts the block's own header predicts from the
layout (layer A's tail in burst 35, layer C's head in burst 69), and layer C's
6-byte tail correctly survives because layer D's edge bursts ARE read. The
arithmetic and the detector corroborate each other rather than agreeing by
construction.

`tb_pageio.sv` follows `tb_pagestream.sv`: a PLAYED guard transcribed from
`zhao_mem_guard.sv` (`rsp.ready` a LEVEL, `rsp.ok` a PULSE the cycle after, and
never both high), plus the REAL `zhao_mem_guard` watching the DUT's own request
wires as an observer that drives nothing. The clean case asserts
shadow_viol == 0 and shadow_ok == shadow_fwd == shadow_req: the real guard
admits and forwards every request this block makes, in both directions, which
is the evidence that no guard amendment is needed. The one thing added over
pagestream's bench is a WRITE engine, because this is the first terrain block
that both reads and writes the pool.

GATES: verilator --lint-only -Wall on the DUT and on tb_pageio, 0 warnings;
check_quartus17_syntax.py clean; the executable runs 79 checks with 0 failures.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 742f2461

PAGEIO: declare zhao_terrain_pageio in the inventory, the manifest and a fit gate

Three different acts, as CLAUDE.md says: registering a block in the ledger, the
manifest and a fit source list are not one thing. The ledger row landed first
(R210); these are the other two, plus a standalone fit target.

  design/console_inventory.yml   pending_compose, with the evidence and the
                                 blocker, so check_console_inventory's G4 can
                                 stop calling it UNCLASSIFIED
  design/prod_manifest.yml       not-yet-adopted, with the discharge condition
  design/fit_targets.yml         F-PAGEIO1, written BEFORE the fit

`not-yet-adopted` RATHER THAN A `top:` ROW IS A DECISION, not paperwork, and it
is the one thing in this packet that could have been taken quietly and was not.
Adding an unfitted block to the production fit's closure changes what that fit
measures. This one carries a 1,089-flop shadow plane and ~3 M10K of page
buffers, on a device at 97% of its ALM ceiling. That is an area decision, and it
belongs at a fit gate with a stated question -- never inside a packet, and never
by a row appearing in a list.

The wording is also the mechanism. tools/budget/uncashed_cheques.py reads
`not-yet-adopted` and leaves the module PENDING in its check 1, and it already
does:

    ** zhao_terrain_pageio   PENDING   fit target, never measured

which is exactly the watch this row wants. A `superseded` or `probe` note would
have CLOSED the question; `not-yet-adopted` is a deferral written down, and the
tool is what will ask whether it was performed. That distinction is what let a
~6,000-ALM saving sit unexecuted for two weeks with the plan, the analysis and
the prerequisite all correct.

F-PAGEIO1's ACCEPTANCE QUESTION IS THE SHADOW PLANE, and it is named rather than
left for the fit to imply. `nb_o` must answer any (vi,vj) COMBINATIONALLY --
in contract form (b) the page server delivers base/scar/bottom/nobake on one
beat or the vertex is not ready, and this block does not own that valid -- so a
RAM cannot serve it and `nbv_q` is 1,089 flops plus a variable bit-select. The
cheaper alternative is written into the module header: a two-row register
window, 66 flops, correct only for a cursor that scans z-then-x. It buys area
with a coupling to TERRAIN.BAKE's traversal order. Do not take it on an
argument; take it on this number, if this number says to.

The rule set carries two things worth reading:

  * `min_m10k: 2` as well as `max_m10k: 6`. This is the OPPOSITE of
    zhao_terrain_pagestream's `max_m10k: 0` two rows below, for the opposite
    reason: pagestream's staging buffers are read combinationally by a byte lane
    on the emit cycle, so a memory there costs a cycle per vertex. These are
    read one word per guard beat with a cycle to spare, so they MUST infer
    memories -- and a fit that reports ZERO M10K has put 26,624 bits in flops,
    which is the failure the floor exists to catch. A max alone cannot see it.
  * `max_registers: 1800` computed from the structure BEFORE the fit runs, so
    the first receipt is a measurement against a number chosen without sight of
    it.

GATES, all green on this commit:

  check_console_inventory.py   OK -- 356 modules, every reachable module a
                               source, the latest version wired, everything out
                               with a declared reason
  check_prod_manifest.py       OK -- 356 modules, 71 tops, 163 excluded, every
                               module counted once or declared absent
  check_quartus17_syntax.py    OK -- 557 files, no rejected forms
  check_case_labels.py         OK
  mutant_copy_drift.py         OK -- 56 copies, none stale
  mutant_drivers.py            OK
  uncashed_cheques.py          OK -- and it names this block PENDING, which is
                               the row working as designed
  check_counters.py            OK
  refmodel_liveness.py         OK
  duplicate_functions.py       OK
  gen_prod_top.py --check      FRESH, 71 instances (this packet changed no port
                               on any composed block, so the generated top is
                               unaffected -- checked rather than assumed)
  gen_console_board.py --check FRESH
  gen_shell_paired_diff --check FRESH
  completion_register.py       22 gaps; superseded check: 72 production roots
                               CLEAN (R174 -- this block is not a _vN and
                               composes nothing, so it manufactures no
                               supersession violation, verified rather than
                               assumed)
  packet_h_tieoff_audit.py     RC=0, and fpga/rtl/prod/zhao_console_core.sv is
                               BYTE-UNTOUCHED by this packet. NO TIE-OFF WAS
                               CREATED, so there is nothing to declare in the
                               core's `INCOMPLETE -- TIED OFF, AND WHY` block.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 86f525e6

PAGEIO: amend the contract with what the build found, and REFUSE decision 5

The contract is the durable home for this, beside the thing it governs. A run
folder is not (every pass makes a new one) and neither is a report nobody reads
back -- which is this repository's most repeated failure shape.

Four amendments, all from building the block rather than from reading about it.

1. THE PREAMBLE'S OWN RULE WAS WRONG, AND IT SAYS SO NOW.

   It read: "no design/blocks.yml row yet, deliberately... a mandatory
   capability that is not built is a gap, and this packet is forbidden to close
   one gap by opening another."

   Owner ruling R210 found the opposite. completion_register.py WALKS
   design/blocks.yml, so a capability with no row is not absent from the
   console -- it is absent from the QUESTION. The register's total was never
   wrong; it was answering a smaller question than its readers assumed. The row
   landed first and the total rose 21 -> 22. The old sentence is struck through
   rather than deleted, with the correction beside it, because the reasoning
   was careful and the next person should see how a careful argument produced a
   blind instrument.

2. SECTION 4's BYTE-ENABLE RECOMMENDATION IS STRUCTURALLY UNAVAILABLE.

   It said "Byte enables are preferable and the guard request type already
   carries .be". The type does; the GUARD REFUSES A SPARSE ONE:

       zhao_mem_guard.sv:  be_ok    = (req.be == mask_of(req.len));
                           shape_ok = len_ok && be_ok;

   with the header adding "partial-word masking is NOT in the Phase-2 arbiter",
   and zhao_vram_arbiter.sv confirming from the other side -- it converts len to
   WORDS and the SDRAM controller never sees a byte mask at all.

   Left uncorrected, that sentence is a wrong recommendation in the one document
   the next implementer would read first. It is struck through with the measured
   reason and the real cost: read-modify-write, and SMALLER than the section
   feared -- layer D's seventeen bursts are read anyway to serve cell_state_i,
   so both its edge bursts are already held; only layer B's two are read for
   their foreign bytes. 19 reads, 52 writes.

3. DECISION 1 WAS NEVER AN OWNER DECISION. RULING T4 ALREADY ANSWERED IT.

   The contract asks whether layer B needs the HPS journal, and calls it the
   owner's sentence to write, noting it "changes the block's size by more than a
   factor of two". zhao_terrain_writeback.sv's header states T4 as "B and D are
   NEVER written back (the HPS keeps the canonical mirror current from the same
   deterministic commands)" -- and gives it as the REASON layer F, which has no
   canonical mirror, needs the doorbell. The pool write is the whole of it.

   The shape is worth more than the answer: the contract escalated a decision
   that already had a ruling, in a header its own section 4 cites for a
   different reason. Before escalating, grep for the ruling.

4. DECISION 5, WHICH THE CONTRACT DID NOT NAME, IS STATED AND REFUSED.

   SEAMDIG deliberately did not build the sheet arbiter: "its policy between a
   live stamp and a bake read is a decision, not a wire." Measured against the
   ACTUAL port, it is not a scheduler-shaped problem at all, and calling it an
   arbiter understates it by three whole items.

   zhao_surface_sheet's req_* is a CONTROL-AND-READ port, not a memory read:
   req_op_i carries OP_ACQUIRE / OP_READ / OP_RELEASE, it takes a 32-bit
   residency HANDLE, and the answer returns on a separate response stream pg_*
   with a STATUS -- ST_HIT / ST_ALLOCATED / ST_OVERFLOW / ST_MISS. Bake wants a
   COMBINATIONAL lookup: sheet_texel_o is combinational on the dig cursor and
   sheet_strength_i is sampled with vtx_valid_i, on the same beat as
   base/scar/bottom/nobake.

   So four things are missing and only the third is an arbiter:

     1. A HANDLE AND ITS LIFETIME. Who issues OP_ACQUIRE for the patch's sheet
        page and who issues OP_RELEASE? A bake that acquires and never releases
        leaks one of `Slots` -- which defaults to 2.
     2. A LATENCY ADAPTER. A ready/valid request with a separate response
        stream cannot answer on the beat bake samples. Either bake's dig stalls
        per vertex -- 1,089 round trips per record -- or something prefetches
        the 64x64 sheet, which is 8,192 bytes and a second copy of layer F on
        chip.
     3. THE ARBITER PROPER. SURFACE.STAMP writes the same sheet in the same
        frame. A bake that reads a half-applied stamp digs a shape the player
        did not make; a stamp that waits for a bake drops frames.
     4. A LAW FOR ST_MISS, and this one is not an engineering question. If the
        sheet page is not resident when a cmd_depth_sheet_i record digs, the
        vertex gets SOMETHING, and the options are not cosmetically different:
        FAIL THE RECORD (the deformation does not happen and the player's
        action is silently lost), DIG ZERO (it happens with no depth -- a
        visible no-op), or FALL BACK TO THE PARAMETRIC DISC (section 9.3's
        other law, a different shape).

   RECOMMENDATION, AND IT IS ONLY THAT: decision 4 first, because it is the one
   with a player-visible consequence, and the other three are cheap once it is
   written. Do not take 1-3 without it -- an arrangement built around a miss
   policy nobody chose will have chosen one.

   Nothing was built for this seam. Adding a disconnected adapter around an
   unchosen miss policy would look like progress and would foreclose the
   decision.

Decision 2 is TAKEN as the contract recommended (a bake does not bump the
generation; pageio_rtl_directed asserts the echo, so the other reading cannot
be adopted silently -- it turns a test red). Decision 3 is HALF taken: dm_f_o
low is settled and not a decision, dm_mips_o high is a SAFE DEFAULT under
pagestream's still-open MIPGEN ruling and is labelled as such, high being the
reading under which a stale mip can never be shown. It is one line beside a
comment that says it is a default.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 5d56fd7f

PAGEIO: clamp the write-beat prefetch, and PROVE the elaboration guards run

Two things, both from CLAUDE.md laws rather than from a failing gate.

1. THE WRITE-BEAT PREFETCH ADDRESSED ONE WORD PAST THE BUFFER.

   The write path prefetches the next beat's word so a two-cycle memory read
   costs no extra cycle:

       S_BWR_BEAT: bwrd_ra = burst_q*BEATS + (guard_wready_i ? beat_q+1 : beat_q)

   On the ACCEPTED LAST BEAT OF THE LAST BURST that is 34*8 + 8 = 280, one past
   the end of a 280-entry array. The value is never used -- the final beat is
   already on the wire and the state machine leaves -- so it is benign in
   behaviour, which is exactly why nothing caught it: 79 directed checks green,
   Verilator -Wall silent, the guard observer happy.

   It is still an out-of-range index into a structure meant to infer an M10K,
   and Quartus is free to treat that differently from Verilator. "Benign today"
   is not a property anyone re-checks. The advance is now clamped at the last
   beat (`beat_next_c`), which keeps every address inside both buffers by
   construction. 79 checks still green; lint still 0; check_quartus17_syntax
   still clean over 557 files.

2. THE `initial` ELABORATION GUARDS FIRE. MEASURED, NOT ASSUMED.

   CLAUDE.md: "`--lint-only` does not run `initial` blocks. Linting a
   deliberately broken parameterisation returns RC=0 and says nothing whatever
   about the elaboration `$fatal` guarding it. A clean lint is not evidence
   about an elaboration check."

   This block has eight of them -- the layout arithmetic the contract's section
   4 says to "check at elaboration rather than trusting". They had a clean lint
   and no evidence. A scratchpad copy of tb_pageio.sv that instantiates the DUT
   with `.D_BYTES(512)` was built and run:

       %Fatal: zhao_terrain_pageio.sv:364: Assertion failed in
               TOP.tb_pageio.u_dut: pageio: layer D must be 1024 cells x
               1 byte (got 512)

   The guards run. Nothing in the tree changed; the break lives in the
   scratchpad and the evidence is this message.

AND A TOOLCHAIN FACT FOUND GETTING THERE, because it wasted time and will waste
somebody else's. The first attempt at the above was a five-line bespoke `main`
that constructs the model and evals once, statically linked at -O0. It built
cleanly and then HUNG: alive, producing no output, at 0.015 CPU SECONDS after
several minutes.

That is CLAUDE.md's "ALIVE AT ZERO CPU" tell, and in a non-interactive session
it is indistinguishable from "the guard did not fire" -- a silent control
reading negative for a reason that has nothing to do with the design. It is
consistent with the documented launcher-popup trap (an invisible GUI dialog
waits forever at zero CPU) and with the mixed winlibs/mingw libstdc++ that
`run_console_core_smoke.ps1`'s own header documents.

The fix was not to debug it: REUSE THE BUILD RECIPE THAT ALREADY WORKS. The
same sources compiled at -O1 with the real test main and tests/harness/
zhao_sim.cpp ran immediately and printed the %Fatal. A bespoke driver is a
second build recipe, and a second build recipe is a second thing that can be
wrong in a way that looks like a result.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 6b31daa4

PAGEIO: entry I27's "third block, not a wire" now exists -- record it there

29 lines, ALL OF THEM COMMENTS, in entry I27. Zero non-comment lines changed;
`git diff | grep -v '^+//'` returns nothing. The tie-off audit stays at
0 SILENT (8 declared, 1 reasoned, 10 by group comment).

WHY THIS EDIT IS WORTH TOUCHING A SHARED FILE FOR. Entry I27 describes its
missing owner so exactly that it reads as a work order:

    "Whoever drives the mark has to hold the slot and generation the patch was
     served under and pair them with bake's completion -- and that is a THIRD
     BLOCK, NOT A WIRE."

`zhao_terrain_pageio` IS that third block, and it was built this session. Left
unrecorded, the next reader of I27 finds a precise specification for a block
that already exists and builds it again -- which is exactly the failure
CLAUDE.md records under "before commissioning a new block, grep the tree for
the thing it replaces", where a projected-vertex arena was designed, built,
linted, Quartus-gated, directed-tested and committed with two fired positive
controls while `zhao_vertex_arena` already existed with 58 formal assertions
and a committed SymbiYosys proof. Not one gate can ask whether a module needed
to exist. Only a sentence in the place people look can.

The note says four things and each is checkable from the tree:

  * the block exists, with its ledger id, its contract and its ruling;
  * its `dm_slot_o`/`dm_gen_o`/`dm_epoch_o`/`dm_bd_o`/`dm_f_o`/`dm_mips_o` are
    a PORT-FOR-PORT match to `terr_dm_*_i` -- which is the thing I27 corrected
    itself about on 2026-09-20, when "its writer is TERRAIN.BAKE" turned out to
    assert a presence that was not there (bake has no deformation-mark port of
    any kind). This entry's own correction is why the new claim is stated as a
    port match rather than as an intention;
  * WHY it exists at all -- it is already the block that holds the residency
    identity, because its real job is serving the four TERRAIN.BAKE page ports
    nothing served. The mark is not a bolt-on;
  * THIS HALF STILL DOES NOT CLOSE. `zhao_terrain_pageio` is BUILT and NOT
    COMPOSED, because its own consumer `zhao_terrain_bake_v2` is not composed
    either. Wiring the mark alone would connect a port to a block nothing
    drives. I27 stays a gap and the register still counts it.

It also records a coupling the entry did not name: a bake does NOT bump the
generation, and if it did, `terr_chk_stale_o` in this same entry's other half
would start firing on live handles. The two halves of I27 are coupled by a
decision, not by an accident, and the next person narrowing this entry should
know that before moving either.

A NOTE ON PROCESS, recorded rather than hidden. The console-core smoke forms
were running when this edit landed, and `zhao_console_core.sv` IS in their
219-source closure. CLAUDE.md's rule is that a suite reading a live tree gives
an answer worth nothing in either direction, and the mechanism it names is
half-written files read mid-write. That mechanism does not apply here: the
write is atomic (whole file) and comment-only, so every form read a complete
file whose RTL semantics are identical to the one before it. Forms 1 and 2 had
already passed against the pre-edit tree; the rest read the post-edit one. No
further edit to anything in that closure will be made while it runs.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 294c5c01

PAGEIO: test the write-invalidation instead of claiming it -- 88 checks

The module header said a held cell answer is invalidated by a write to the same
cell, "and the bench drives that re-read too". IT DID NOT. The cursor-move case
drives a re-read after a CURSOR MOVE; nothing drove one after a WRITE. A header
that describes a test the suite does not contain is the same defect this
repository keeps finding in its own instruments -- a claim standing in for a
measurement, in the flattering direction.

So the case is written rather than the sentence weakened.

`case_write_invalidates_held_cell`: hold an answer for cell 0, write cell 0
with a byte that cannot be confused with the original, NEVER consume the held
answer, and require (a) the stale answer to be withdrawn the moment the write
is accepted and (b) the re-read to produce the NEW byte. It also asserts
`cell_refetch_o` does NOT move: an ordinary write invalidation is not a fault
and must not be charged to a fault counter, or that counter stops meaning
"bake's cursor moved under a pending read".

WHY THIS PATH IS WORTH A CASE OF ITS OWN. `dwrd_q` is one buffer for both
directions (contract section 5's "further halving"), so a cs write changes the
byte a held answer was read from. TERRAIN.BAKE's own traversal never re-reads a
written cell -- which is exactly why this could sit wrong indefinitely. It is
INVISIBLE under the one order the machine uses today, and it is the block's
promise to any order. The contract's argument buys the single buffer; the
invalidation is what makes the buffer honest.

AND THE CHECK WAS FIRED. A scratchpad mutant with the invalidation removed
fails exactly the two new assertions:

    FAIL: invalidate: the stale answer is withdrawn the moment the write is
          accepted
    FAIL: invalidate: the re-read returned the NEW byte, not the held one
          (expected 169, got 0)

169 is the written byte; 0 is the original. Without the invalidation the block
serves the stale one, which is the defect in its exact shape. The mutant lives
in the scratchpad, never in the tree.

A SMALLER FINDING WORTH RECORDING, because it is the good direction. The first
draft of this case wrote a cell without pulsing `dig_done`, and the block's own
`a_breach_after_dig` assertion stopped the run:

    %Error: pageio: a cell state was written before dig_done_o

The assertion caught the TEST. That is what a sound assertion does to unsound
stimulus, and it is the first time in this packet an instrument fired at
something other than the design -- which is itself evidence the assertion is
wired to something real. The stimulus now pulses `dig_done` and says why.

88 checks, 0 failures. Lint 0 warnings. check_quartus17_syntax clean over 557
files.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

