# FINDINGS — TERRABAKE, 2026-09-21

## THE NUMBER

**COMPLETION REGISTER 21 → 17**, measured BARE in this tree at both ends
(RC 1 while gaps remain is normal).

```
before   21  = 9 tie-offs + 12 disconnected + 0 unbuilt
after    17  = 8 tie-offs +  9 disconnected + 0 unbuilt
```

**DISCONNECTED → CONNECTED (three of my five):**
`zhao_terrain_pageio`, `zhao_terrain_bake_v2`, `zhao_terrain_sheetseam`.

**TIE-OFF ENTRY I32 CLOSED AND DELETED.** `terr_chk_*` is all that is left of
I27, whose head line now says so.

Branch `gz/terrabake` from `d58301ab` (the head of
`claude/ceiling-architecture-20260912`, read with `git ls-remote`). Own
worktree. Never rebased. Two commits, pushed as the work happened.

---

## WHAT COMPOSED, AND WHY IT HAD TO BE ONE ACT

FIVE blocks in one commit, for TERRACOMP's reason exactly: every one of them is
a producer whose only consumer is another of them, so composing any one alone
forces a tie-off and the register does not move. Seven lanes refused this seam a
block at a time and **each refusal was locally correct**.

| | |
|---|---|
| `zhao_terrain_bakerec` | **NEW.** The "third block between them" entry I32 commissioned in as many words. |
| `zhao_terrain_pageio` | Layer D in, layers B and D out, and the deformation mark. |
| `zhao_terrain_sheetseam` | Layer F at the dig's rate (R221, R231). |
| `zhao_terrain_bake_v2` | The dig. |
| `zhao_surface_sheetshare` | The two-client sheet arbiter I32 specified **by type**. |
| `zhao_terrain_psmux` | A **second instance**, chained under the first, for the lattice pass. |

Plus `u_build_share` widened `.N(3)` → `.N(4)` — no guard change, no new client
id, no new region, exactly as `TERRAIN.PAGEIO.md` §6 says.

---

## I32: WHAT ACTUALLY BLOCKED IT, AND IT WAS NOT THE FIELDS

The entry's own field table was right that three `cmd_*` fields were ABSENT and
wrong about what that meant. Two of the three were **statements about a
composition that did not exist yet**, and they answered themselves the moment
the composition did:

* `cmd_cells_i` — "layer D present". It is true because `zhao_terrain_pageio`
  reads layer D into its own buffer before it serves a vertex. A parameter, not
  a literal, because a console composed WITHOUT the page agent must set it low.
* `cmd_dual_i` — `kFlagDual`, TERRAIN.PAGESTREAM's own T5 record flag, carried
  from TERRAIN.HDRREAD's forwarded job. The same bit `u_terrain_patch` already
  reads off `tps_v_flags`.

The third is the interesting one and it is the one thing I want checked hardest.

### `cmd_depth_from_i` / `cmd_depth_to_i` are ZERO, BY PARAMETER

Every record this producer emits is a **sheet** record. Bake reads the disc
depths on exactly one path: R221's `ST_MISS` fallback. Three things could go
there and I rejected two **in the RTL header, by name**:

* **an absolute depth from the art table** — `stamp_depth(strength)`. A ratified
  law and the wrong one here: `scar_sum = h_scar + delta16` ACCUMULATES, so it
  digs the full crater a second time. **That is the defect R231 repaired**,
  reintroduced where it would be rare, unlogged and indistinguishable from
  terrain. R231 is not re-opened.
* **a fabricated `from`/`to` pair** — the ABI has no depth field, so anything
  chosen here is an art value invented inside a composition packet.

**Taken: zero, with a retry.** The record digs nothing, is counted at both ends,
and is **re-queued**.

**The retry is exact and I checked the mechanism rather than hoping.**
`zhao_terrain_sheetseam` does not consume its `before` plane on a fallback —
`bf_live_q` is cleared only `if (serve_q)` and a `seen` bit is retired only by a
*served* read — so the pre-blend strengths survive and the next issue digs the
whole delta, once. A miss is an eviction or a mid-fill release, transient by
construction.

That makes R221's fallback a **DEFERRAL** rather than a second crater shape, and
`terrain_rules` §9.2 item 3's identity licenses it. **That is D-TERRCMD-C
answered on its own terms**: the entry says the deferral law is written in
`from`/`to` depths "AND IN NOTHING ELSE" and that a sheet record therefore has
no state-exactness argument. It has one under the delta law, and
`bake_delta_idempotence_directed` case 4 is it.

Both constants are named parameters because "the fallback digs nothing" is a
decision the owner may reverse in one edit, and a decision that is not a knob is
how a wrong number becomes an unadjustable wrong number.

### D-TERRCMD-B is still true and is no longer a blocker

A **disc** record cannot be formed from the ABI at all. That stands. It stopped
blocking because nothing in this console needs one: the disc arm's producer is a
cast's progress in the FIELD subsystem, for §9.2's *"(to − from) × stencil so an
interrupted cast un-applies"*. It will arrive with the cast producer and will
need an arbiter on `cmd_*`; no port is reserved for it.

---

## THE ONE PORT I ADDED TO A COMPOSED BLOCK, AND IT IS NOT OPTIONAL

`zhao_terrain_pageio` gains **`serving_o`** — one line, `(state_q == S_SERVE)`,
no new state and no new decision.

A composer needs it and **cannot derive it**. `nb_o` is COMBINATIONAL on a
shadow plane that does not exist until the end of `S_NBV`; `idle_o` drops at the
**job accept**, roughly 1,200 clocks earlier. A dig started on `idle_o` reads an
EMPTY no-bake plane: §3.3's corner shadow silently vanishes, the bake looks
like a bake, and **every handshake and every counter agrees**. The block's own
contract already said the face is live in `S_SERVE` and nothing else; the port
publishes that fact.

`tests/terrain/tb_pageio.sv` could not elaborate without a connection for it
once the port existed, which is how the CMake configure found it. The bench now
exposes it as `c_serving`.

---

## THE VERTEX JOIN — the only glue in the chain

TERRAIN.PAGESTREAM **pushes** (`v_vi_o` is its output) and `zhao_terrain_bake_v2`
**pulls** (`vtx_vi_o` is its output). Entry I32 called this "a cursor-match
adapter — not a memory, not a reorder buffer". It is smaller than that: both
walk 33×33 with **vi as the fast axis** from (0,0) and both hold their beat
under ready/valid, so the join is

```
vtx_valid_i = ps2_a_v_valid && ssm_str_valid
ps2_a_v_ready = vtx_ready_o && ssm_str_valid
```

**The cursor equality is ASSERTED, not assumed** — "they agree by construction"
is exactly the claim the core header exists to stop anyone making silently. It
is an **assertion and not a counter**, deliberately: the desync state is
unreachable while both walkers are correct, so a counter would read zero forever
and owe a committed mutant, and the seam made the same call for the same reason
about its two-reads-in-flight invariant. A stray beat — the reachable fault — is
`u_terrain_psmux2.stray_v_o`, a real counter on a reachable state.

`ssm_str_valid` is the seam's own written instruction and is HIGH throughout any
record not on the sheet law, so a disc record or an R221 fallback is never
slowed by it.

**And the seam's read pacing survives a stalled stream**, which I checked rather
than assumed: `rd_issue_c = serve_q && !rd_held_c` is driven by BAKE'S CURSOR
ADDRESS, not by `dig_ready_i`, so a cycle in which the stream has no vertex
re-reads nothing and clears no `seen` bit. `dig_ready_i` feeds only
`dig_stall_cycles_o`. Had it paced the read, a stalled stream would have wiped
`before` bits and silently under-dug.

---

## PORTS: THIRTEEN OUT, ONE IN — and the one in is a gate keeping its meaning

Out: eight `terr_dm_*`, four `terr_cc_cs_*`, and `surf_res_ready_i`.
In: **`surf_res_taken_o`**.

TERRACOMP's process finding was that deleting boundary ports moves the register
by ZERO and costs live smoke assertions. It cost one here and it was paid rather
than absorbed. The smoke asserts `surf_res_records == surf_stamp_texels_touched`
and counted on `res_valid && res_ready`. With the ready gone, counting
`res_valid` alone would have turned an assertion about an **accept** into an
assertion about an **offer** — a gate quietly losing its meaning, green
throughout. `surf_res_taken_o` is the core's own `valid && ready` exported in
its place and the bench counts that.

`wrapper_port_parity` **1324 → 1312 = 1312**, missing 0, stale 0; both
`zhao_console_core_*_mutant.sv` wrappers refreshed in the same commit.

---

## WHAT THE SMOKE DOES AND DOES NOT PROVE HERE

Stated plainly because the pass is easy to over-read. **The composed bake chain
is QUIESCENT in the smoke and cannot be otherwise**: no page this bench loads
passes the header identity check, so `zhao_terrain_bakerec` never receives a
page identity, never issues a record, and the chain never runs. What the smoke
proves is that the console **elaborates and does not regress** with five more
blocks and thirteen fewer ports in it.

The chain's BEHAVIOUR is held by the block suites, all of which build and run:

| suite | checks |
|---|---|
| `bakerec_rtl_directed` | **84, NEW** |
| `pageio_rtl_directed` | 88, unchanged after the new port |
| `sheetseam_rtl_directed` | 118 |
| `terrain_bake_v2_sheet_directed` | 6,548 |
| `terrain_bake_v2_directed` | 267 |
| `terrain_stampdepth_directed` | 6,505 |
| `bake_delta_idempotence_directed` | 5,952 |

**WHAT IS OWED NEXT AND IS NOT DONE**: a traversal of the composed chain end to
end. `tests/terrain/world_composed_directed.cpp` already composes a terrain
chain around a played HPS and is where it belongs — a page that PASSES its
header check, a stamp on it, and then `terr_dm_bd` landing on the directory.
That is also the first stimulus that can reach entry I28's writeback path, which
has been closed since 2026-09-19 and **has never seen a beat** for exactly this
reason: nothing in the console could dirty a page. Now something can.

---

## THE NEW BLOCK's SUITE, and one assertion of mine that was wrong

`tests/terrain/bakerec_rtl_directed.cpp` — **84 checks, 0 failures, built and
run** (R60). All eleven counters asserted SILENT then FIRED from the block's own
boundary with legal stimulus (R95), so none owes a committed mutant.

**No bench wrapper, deliberately**, and it is what makes one case possible: the
DUT is the real module at its own ports, so a case can hold `io_serving_i` LOW
and require the seam to be offered nothing. No composition of the real blocks
can hold the page face dark on purpose.

The cases check **fields, not counters**, wherever a counter could be right
while the machine was wrong — a coalesce that kept the FIRST geometry reads
`coalesced_o == 1` and digs the wrong place; a retry that counted and did not
re-queue leaves `records_retried_o` correct and the deformation gone.

**One assertion of mine was wrong and the RTL was right.** I expected
`records_issued_o` not to move on a fallback attempt. It counts the SEAM's
ACCEPT, so it counts ATTEMPTS while retired+dropped count RECORDS.
`issued == retired` is the intuitive invariant and it is false; the test now
says so where the next reader will find it.

---

## TWO ROTS, AND THE SECOND ONE IS ONE DAY OLD

* **`design/contracts/TERRAIN.TESS.md`.** The packet brief named
  *"`TERRAIN.LOD` (does not exist)"* as a rot. It had already been corrected —
  by gz/terrassem, **yesterday** — and **that correction is itself now stale**:
  it says TERRAIN.LOD "is not COMPOSED", and TERRACOMP composed it hours later.
  Corrected again, with the general form written down: **a not-composed list is
  a SNAPSHOT and a snapshot in a contract reads as a law.** What that paragraph
  is actually for is the phase-6 gate captures; composition state is computed
  from the tree by `completion_register.py` on every run and belongs there.
* **Entry I32's "PAGEIO is NOT BUILT / no `blocks.yml` row"** — already struck
  in both places by gz/terrcmd before I arrived, and re-verified here before the
  entry was deleted (63,729 bytes, `- id: TERRAIN.PAGEIO` present). Two lanes
  inherited that sentence; it is now gone with the entry.
* **`design/prod_manifest.yml`** carried "Not composed" on the sheetseam row and
  "ADOPT when `zhao_terrain_bake_v2` composes" on the pageio row as LIVE claims.
  Both struck with what spent them named.

---

## TIE-OFFS DECLARED

**NONE.** No literal was tied anywhere in this composition, so nothing was owed
in the core's `INCOMPLETE` block under R159. `packet_h_tieoff_audit` reads
**8 declared, 1 reasoned, 14 by group comment, 0 SILENT** — the "by group" count
rose only because `u_terrain_psmux2` inherits the existing `b_j_flags_i (16'd0)`
comment from the instance it was chained under.

The two decisions that COULD have been tie-offs are parameters on a composed
block instead, with their reasons in the RTL header, the contract and the ledger
row: `CELLS_PRESENT` and `FALLBACK_DEPTH_FROM`/`_TO`.

---

## COST — hand-counted, never a veto (R236)

No fit was run; `PACKET-PROTOCOL.md` forbids it.

* `zhao_terrain_bakerec` — two records of ~280 flops each, a seven-state
  sequencer, eleven counters. **~700 flops, a few hundred ALM. No multiplier,
  no memory.**
* `zhao_terrain_psmux` second instance — one `last_q` flip-flop and its
  handshake fabric; the block is measured already.
* `zhao_surface_sheetshare` — the same shape.
* `u_build_share` `.N(3)` → `.N(4)` — one more requester in an existing
  round robin with an existing retirement ledger.
* The three blocks that MOVED were already built and already had fit targets.
  `F-PAGEIO1` and `F-SHEETSEAM1` exist and **neither has ever run**;
  `F-SHEETSEAM1`'s open question — whether a 1,089-word plane infers ONE M10K or
  two, since an M10K is 1,024 words deep in x8/x10 — is now a question about the
  CONSOLE's memory budget rather than a leaf's, and there are two such planes.

**The honest headline on cost: this composition ADDS five blocks to a console
already over on ALM, and nothing here measures that.** The next console fit is
the first number that will describe it.

---

## GATES — every one run BARE, output read

`tools/quartus/`: `check_console_inventory` (232 elaborated by the core, 238 fit
sources, OK), `check_prod_manifest` (OK), `check_quartus17_syntax` (614 files,
none rejected), `gen_prod_top.py` (74 instances, no diff — the new blocks are
not prod_top's), `gen_console_board.py` (1,313 core ports re-exported),
`gen_shell_paired_diff.py` and `--mutant`.

`tools/budget/`: `check_case_labels`, `mutant_copy_drift` (run AFTER the commit,
R121 — 60 copies, OK), `mutant_drivers`, `uncashed_cheques`, `refmodel_liveness`,
`duplicate_functions`, `completion_register` (RC 1, 17 — normal).

`tools/design/`: `wrapper_port_parity` (1312 = 1312), `check_counters` (all
eleven BAKEREC counters named against their ports), `check_findings_citations`,
`packet_h_tieoff_audit`.

Verilator `-Wall` over the console's own 238-source closure: **0 errors, and no
warning naming any of the new code.** Three `UNUSEDSIGNAL` warnings my first
draft produced were not waived away — each is now a named `lint_off` region with
the reason beside it (`cs_event_o`/`cs_src_id_o` are bake's TRACE outputs and
the contract says they do not enter the page agent; `pio_dm_slot`'s top bit is
the pool's refusal bit, structurally zero because the block REFUSES rather than
clamps).

**A clean lint is not evidence about an elaboration check** — the smoke's
`-LintOnly` form is, and it passed in 25 s, which is what says the tree
ELABORATES with all of this in it.

## SMOKE — all ten forms, ONE FROZEN TREE

All ten forms, run splatted (NOT nested `powershell -File`, which TERRCOMP
measured truncating every log and reporting RC 1 in ONE SECOND for all ten --
the DURATION is the tell). Measured at commit `368bf81d`, one frozen tree,
nothing edited while it ran.

```
LintOnly       rc=0 fatals=0   25s   elaborates
plain          rc=0 fatals=0  277s   SMOKE: PASS
Mutant         rc=0 fatals=0  279s   MUTANT PASS -- terr_pl_slot_overflow_o fired 1
UntexMutant    rc=0 fatals=0  276s   MUTANT PASS -- geom_untex_refused_o fired 16
NoTableLoad    rc=0 fatals=1  264s   INVERTED: the one %Fatal IS the pass
BadDescriptor  rc=0 fatals=1  273s   INVERTED: the one %Fatal IS the pass
BadVertex      rc=0 fatals=0  267s   BAD_VERTEX PASS
NoEchoArm      rc=0 fatals=0  256s   SMOKE: PASS
BadTraceArm    rc=0 fatals=0  265s   SMOKE: PASS
GlowTag        rc=0 fatals=0  273s   SMOKE: PASS
```

**Every form matches the recorded baseline**, including both inverted controls
at exactly one `%Fatal` each and no other form printing one. Durations
256--279 s against the baseline's 248--274 s; the two highest are `plain` and
`Mutant`, which overlapped the `bakerec` test build on this machine.

**GlowTag's two halves were read, not just its exit code.** With the tag:
`frags=2560 [untagged=1498 lit=1062]`, 1,344 bloom cells, 1,344 framebuffer
words changed. Plain: `[untagged=2560 lit=0]`, 0 bloom cells. Both halves are
required before either number means anything, and both are present.

**AND THE ONE LINE THAT IS ABOUT THIS PACKET**, from the plain run:

```
SMOKE: surface   texels[stamp/sheet]=[60 60] results_taken=60 occupancy=01
```

`results_taken` is the rewired counter. It reads 60 against
`surface_texels_touched` 60, so the `records == texels_touched` assertion is
still measuring an ACCEPT through `zhao_terrain_sheetseam`'s sink, and the
stamp's own path is bit-for-bit undisturbed by being put behind
`zhao_surface_sheetshare` -- `texels[stamp/sheet]` agreeing at 60 with
`occupancy=01` is the share carrying it.

---

## ONE DEVIATION FROM THE TREE's CONVENTION, DECLARED RATHER THAN HIDDEN

`zhao_terrain_bakerec`'s elaboration guard sits inside
`// synthesis translate_off`. Its siblings -- `zhao_terrain_sheetseam` and
`zhao_terrain_pageio` -- put theirs in a bare `initial begin ... end` so that
`quartus_map` elaborates it too. Verilator runs mine either way (CLAUDE.md's own
finding: `translate_off` does NOT make Verilator skip the block, proven by
planting a syntax error in one), so the guard is live everywhere a test
elaborates; what it loses is the Quartus half.

**It was left alone deliberately.** Changing it after the sweep would have made
the sweep evidence in neither direction, which is the exact trap DELTALAW hit
and re-ran four forms to escape -- for a change that was "COMMENTS ONLY". Two
pragma lines is the same argument. It is owed as a one-line follow-up in
whatever packet next touches that file, and it is recorded here so the next
reader finds the reason rather than the inconsistency.

