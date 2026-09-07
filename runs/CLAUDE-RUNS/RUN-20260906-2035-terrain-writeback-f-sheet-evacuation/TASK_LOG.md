# Task Log: RUN-20260906-2035 - [Describe objective here]

**Created:** 2026-09-06 20:35 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-2035-terrain-writeback-f-sheet-evacuation/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 20:35 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-2035
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## Plan settled (from sources), 2026-09-06

**T4 decides the payload: layer F ONLY.** B/D are never written back (HPS owns the
canonical mirror and keeps it current from the same deterministic commands), so the
block's payload is the 8,192-byte surface sheet and nothing else.

**The two accesses, derived rather than assumed:**
* SOURCE = layer F *inside the page*, in `TERRAIN.PAGE_POOL` (local SDRAM). T2/§5b:
  "There are no separate permanent E/F/H pools -- those layers live *inside* the
  21,376-byte page." So the read is a MEM.GUARD read of the page pool by client 6.
  **MEM.GUARD is write-only there today. This block needs a read arm.** Reported,
  not made.
* DESTINATION = the HPS terrain journal (T4: "copy exactly F to the HPS terrain
  journal ... wait for journal acknowledgement"). That is MEM.HPS.BRIDGE write
  traffic. The bridge RTL ALREADY has a write path (`wr_valid/wr_data/wr_last`,
  `req.write`); only the contract's granted-writes list needs the journal arena.
* `TERRAIN.WRITEBACK_STAGING` (0x0578_0000, 64 x 8 KiB) is deliberately NOT used in
  v1 and stays unmapped: the ACK barrier holds the slot anyway, so store-and-forward
  buys nothing and costs two more guard passes over 8 KiB.

**Layer F is not 64-B aligned inside the page.** Offsets: hdr 64, A/B/C 2178 each,
D 1024, E 3072 => F starts at byte 10,694 and ends at 18,886. 10694 = 64*167 + 6.
So the block reads the ALIGNED SUPERSET (129 chunks from 10,688) and realigns by a
constant 6-byte lane: `out = {next_src, this_src}[8*LANE +: 64]`.

**One extra read: the 64-byte page header**, to check `{island_id, ix, iz}` before a
single journal byte moves. The guard read arm admits reads of the WHOLE pool, so
"journalled another patch's scars" becomes possible; the header restates the key
(terrain_rules 2.1, "redundancy is a corruption check") and turns it into a refusal.

**ACK barrier:** ticket table (ACK_SLOTS=4), seq matched, `wb_*` to
TERRAIN.RESIDENCY emitted ONLY on a good ACK. Never fabricated.

## Build note for this run: the shared tree could not regenerate

`cmake --build build --target test_writeback_rtl_directed` failed twice with

    Error copying file (if different) from ".../Vtb_terrain_seq.dir/Vtb_terrain_seq.cmake"
    to ".../Vtb_terrain_seq_copy.cmake": No such file or directory
    ninja: error: rebuilding 'build.ninja': subcommand failed

This is CLAUDE.md's documented trap exactly: a verilate rule that is part of
`build.ninja`'s own regeneration fails, so ninja cannot rebuild the graph that
would fix it. The broken rule belongs to **TERRAIN.SEQ**, which another lane owns
and is editing right now -- not to anything in this lane. Retried once as
instructed; still broken.

A fresh lane-local tree (`build-wb`) was started and did not finish configuring
in ten minutes. The lane therefore builds this one target DIRECTLY:

    verilator_bin --cc --top-module tb_writeback --Mdir <scratch>/obj_wb \
      fpga/rtl/generated/zhao_abi_pkg.sv fpga/rtl/common/zhao_pkg.sv \
      fpga/rtl/memory/zhao_mem_guard.sv fpga/rtl/terrain/zhao_terrain_writeback.sv \
      tests/terrain/tb_writeback.sv
    g++ -std=c++17 -O2 -I<Mdir> -I$VERILATOR_ROOT/include -I.../vltstd \
      -Ireference/include -Iruntime/include -Itests/harness \
      -o test_wb.exe <Mdir>/*.cpp $VERILATOR_ROOT/include/verilated.cpp \
      $VERILATOR_ROOT/include/verilated_threads.cpp tests/harness/zhao_sim.cpp \
      tests/terrain/writeback_rtl_directed.cpp

`zhao_zref` is not needed: everything this test uses from zref is header-only,
and `zhao_crc32c` is `inline` in the generated `zhao_abi.h`. The mingw runtime
must be on PATH ahead of oss-cad-suite's or the exe dies with
STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139) before `main`.

**The CTest registration is committed and correct** (`writeback_rtl_directed` +
`lint_terrain_writeback` in tests/CMakeLists.txt); it will build the moment the
SEQ lane's rule is fixed.

## FINDINGS

**1. MEM.GUARD must gain a read arm, and here is the narrowest form.**
Layer F lives INSIDE the page (T2: "no separate permanent E/F/H pools"), so
evacuating it is a MEM.GUARD READ of `TERRAIN.PAGE_POOL` by client 6 — and that
window is write-only. The arm needed is one direction bit on the arm already
there: a separate `terrain_rd_ok` over the same constant bounds for the same
single client, so the two directions stay two theorems. NOT made (MEM.GUARD is
formally proven and its proof was re-run today). Four narrower forms considered
and each impossible or forbidden — see the contract.

**2. MEM.HPS.BRIDGE needs no RTL, only one line of contract.** It already
carries writes (`req.write` + `wr_valid/wr_data/wr_last`). What it lacks is the
PERMISSION: the F-sheet journal arena added to its granted-writes list.

**3. `zhao_hps_bridge.hps_bytes` is `[4:0][31:0]` and is indexed by a 3-bit
client id.** `ZHAO_CLIENT_TERRAIN_BUILD = 6` is out of range, so every byte
this block and TERRAIN.PAGELOADER move across the bridge is silently
unaccounted in `hps_ddr_bytes_by_client`. Reported, not fixed — MEM.HPS.BRIDGE
is not this lane's. A §25 budget group reading zero for a client that moves
41 MB/s is a broken instrument.

**4. The bridge's write channel has no `wr_ready`.** It consumes a beat only
while `busy && busy_write && issued`, and `issued` comes AFTER the client's
grant pulse — so a client that streams on the grant loses beats silently. This
block takes the acceptance level as a sideband `hps_wready_i`; the requested
amendment is that the bridge expose the level it already computes.

**5. Layer F is six bytes off a burst boundary and the tree had no layer offset
table.** Summing `spec/terrain_rules.md` §2 puts F at page byte 10,694
(= 64x167 + 6). The table now lives once in `zref_terrain_page.hpp` with a
static_assert on every running total; the block reads the aligned superset and
realigns by a constant lane (a part-select, i.e. wiring). 129 chunks in, 128
bursts out.

**6. `tools/rtl/check_guard_verdict.py` HAD A BLIND SPOT AND READ CLEAN.**
Found by breaking this block on purpose and watching the alarm NOT go off. Its
depth walk counted `begin`/`end` from column 0 of the arm's opening line, and
the commonest shape in this tree is `end else if (guard_rsp_i.ready) begin` --
whose leading `end` (closing the PREVIOUS arm) cancelled this arm's `begin`, so
the walker stopped on the opening line and never read the body holding `.ok`.
Fixed; a second self-test example in the missed shape added; all ten clients
re-checked clean; the fixed gate then reported
`zhao_terrain_writeback.sv:900 tests .ok in the SAME arm as .ready`.
This is the broken-instrument law exactly -- the defect made the answer look
better, and nobody audits good news.

**7. The suite had a hole in its own barrier coverage, found the same way.**
Both directed unmatched-ACK cases ran with an EMPTY ticket table, so a block
that matched ANY outstanding ticket passed them; only the random phase and the
ledger caught it, saying `every draw agreed with the oracle (expected 0, got
230)`. Section 7a' now holds a real ticket while a stranger's ACK arrives.

**Result: 294 checks, 0 failures, 1,083,921 gpu clocks.** Twelve perturbations,
each shown to fire with its exact failure text (contract, *Proof that the suite
can fail*). Lint clean on both the RTL and the bench under `-Wall`.

**Could not close:** the guard read arm and its proof (MEM.GUARD's lane);
the MEM.HPS.BRIDGE grant and its two defects; layer F has no CRC, so a sheet
corrupted in local SDRAM between the stamp and the eviction is journalled as-is
(a page-format change, SW.STREAM's); what happens to a slot whose ACK never
arrives or whose journal NAKs (unruled -- T11's ABORT is the only mechanism
that exists); `design/blocks.yml` (owned elsewhere -- entry requested in
LEDGER-ENTRY-REQUEST.md).

## 2026-09-07 -- position before reading the PAGESTREAM refit

Written BEFORE opening the result, per CLAUDE.md: fit results redirect the work
and the half-finished thing in hand is what gets lost.

WHERE I WAS: GEOM.PROJECT's fit target is committed (aea9d9d6). The next step
was to launch the queued fits the moment the toolchain freed --
`zhao_terrain_residency_v2` first (queued longest; its `min_memory_bits` FAIL
sits on a row two commits stale and cannot be judged), then `zhao_geom_project`
(never measured, target written today).

STILL OPEN AND UNTOUCHED: COMBINE.V1's DSP measurement; perspuv's per-axis array
split; nine items awaiting an owner ruling; the LOD deviation calculator, which
is blocked on two of them.

## 2026-09-07 -- PAGESTREAM refit FAILED, and what the failure turned out to be

`zhao_terrain_pagestream` refit: 1,649 ALM, 2,043 reg, 0 M10K, 0 DSP -- every
resource rule passed -- and **93.91 MHz against a 100 MHz clock**, so the
`min_fmax_mhz` rule added this morning FAILED it. First firing of that rule, on
the first block that carried it, which is a detector shown to fire.

Splitting all 2,000 summarised paths: 23 negative-slack paths, EVERY ONE ending
at a virtual pin; worst core-to-core path 107.38 MHz. So the block is not slow.

Two things came out of chasing that:

1. **The seam is the real question and no leaf fit can answer it.**
   PAGESTREAM's outputs are combinational (4.30 ns of buffer read) and PATCH
   puts a 33-bit saturating add plus two clamps on those same inputs before its
   first flop. Each leaf fit sees one half. `zhao_terrain_compose_seam.sv` wires
   the two together so it can be measured; three GLUE points named at their
   sites (placement, the DUAL flag, the 32->16 source_id), each existing only
   in a bench today. Fit target written with the prediction BEFORE the fit.
   Queued. (87443fc6)

2. **43 of 51 rows with an fmax are below 100 MHz, and nothing had ever read
   that field.** Split properly, only FIVE miss the clock with no boundary to
   blame -- and the worst is COMBINE.V1 at 29.74 MHz core-to-core, which is the
   same defect as its known DSP overrun rather than a second one. Report:
   `reports/FMAX-WHAT-ACTUALLY-LIMITS-IT-20260907.md`. Probe committed, its
   self-check shown to fire.

The split changed its own answer twice while being written (aux_pipe 63.63 ->
120.37 on the both-ends rule, aux_div6 87.45 -> 103.00 on separating reset), so
the first two versions of this note would both have been wrong.

NEXT: residency_v2 fit is running -- its Fmax row is stale too, not just its
memory line. Then geom_project, then the compose seam.

## 2026-09-07 -- the pair wrappers, and two of my own errors

Found `fpga/rtl/synth/zhao_pair_*.sv`: four registered characterisation
wrappers built 2026-08-23 for exactly the reason today's path split rediscovered
("raw leaf blocks with hundreds of virtual pins are poor physical models").

MY ERROR 1: `zhao_terrain_compose_seam.sv`, written this morning, exposed the
pair's ports directly and would have measured the same virtual-pin
contamination it existed to remove. Deleted; replaced by
`zhao_pair_pagestream_patch.sv` in the established shape.

MY ERROR 2: I then wrote fit targets for the four existing wrappers with source
lists guessed from their NAMES. Three of four were wrong. Caught by the
numbers, not by re-reading: two pairs report DSP blocks that neither named leaf
contains, and no wrapper holds a multiply. Lists now resolved from actual
instantiations and verified by lint. Nothing had been fitted against a wrong
list.

AND THE FINDING THAT MATTERS: none of the four had a target, so 31.10 MHz on
TESS+NORMALS -- the terrain geometry path, worst number in the tree -- has been
sitting unjudged for a fortnight. All four now gated on the product clock; all
four fail (31.10, 37.25, 55.52, 88.79).

The pairs are all SMALLER than their leaf sums, and the comfortable reading is
that the wrappers under-build so the numbers do not count. The DSP column
refuses it: 30-62% fewer DSPs, and a virtual pin never consumed a DSP. That is
logic being folded away, so these are the numbers of a REDUCED circuit and the
full one will not be faster.

## 2026-09-07 -- island handed to a FABLE architect; a repair held back

Owner asked why I had stopped on the texture island. I had not stopped for a
blocker -- I deprioritised it, wrongly, given 16,192 ALM against a 7,500
redline. Directed to take it seriously, then to send a FABLE architect at the
77.30 MHz core-to-core number and have it research everything.

ARCHITECT LAUNCHED. Brief: independent census from the map report, critical
path re-read, ranked rearchitecture with derivations and blast radius, the R6
exact-ordering cost, and a defensible redline for the island AS SPECIFIED NOW.
Architecture only -- no RTL edits, no Quartus runs (the toolchain is busy).
Sent it a follow-up naming the two TMU owner-direction files beside the RTL,
which my first brief missed: the TMU target is SUPERSEDED and awaiting a
replacement spec, so no proposal may be justified against the retired 850,000.

A SECOND ARCHITECTURE BRIEF FROM THE OWNER IS EXPECTED IN THE REPO TODAY.
`tools/maintenance/watch_for_owner_brief.sh` now watches for it -- upstream
commits or a new OWNER-DIRECTION/BRIEF/SPEC file anywhere in the tree -- because
CLAUDE.md records direction being posted four times and never reaching the
working agent. It reports NOTHING FOUND rather than going quiet.

HELD BACK DELIBERATELY, so as not to mutate files the architect is measuring:
a repair to `zhao_texture_material_combine_v2.sv`. `refused_recipe_o` is
assigned ONLY in reset and never incremented, which is why the island's map
report carries `Warning (10240) ... inferring latch(es) for variable
"refused_recipe_o"`. Chased it: the counter is CORRECTLY always zero --
`bad_recipe_c` is hardwired 1'b0 because `f_recipe_i` is three bits, all eight
encodings are real recipes, the oracle refuses only `recipe >= kRecipeCount`
with kRecipeCount == 8, and the command ABI packs the field into three bits
too. Unrepresentable, not merely unobserved.

The defect is therefore NOT the RTL. It is that
`tests/texture/texture_combine_diff.cpp:249` compares dut.refused_recipe_o to
the oracle's refused_unknown_recipe and PASSES ONLY BECAUSE BOTH ARE
STRUCTURALLY ZERO -- reading exactly like the saturation checks two lines above
it, which deliberately assert `> 0` first to prove the case was reached. A
check that cannot fail, wearing the shape of coverage. Patch is written and
staged in the scratchpad (fix_recipe.py): an explicit hold plus the reason in
the RTL, and a restated check that asserts unreachability instead of agreement.
Apply once the architect reports.

Also checked and NOT a bug: `jobs_by_recipe_o += 2`. Deliberate and documented
-- two lanes fire per product-bearing phase, which is exactly what
zref::material::product_jobs() says.

## 2026-09-07 -- withdrew the leaf-sum argument; six blocks need refits

Chasing TESS+NORMALS (31.10 MHz, worst on the terrain path) into the RTL found
NORMALS already rearchitected to ONE shared 33x33 multiplier sequenced over six
steps -- which made its 18-DSP row impossible, and it is: the row predates
bfc74710 by two commits.

That killed the argument I published this morning. All four pair-vs-leaf-sum
comparisons contained a stale row; two also contained leaves with NO row,
summed as zero. Withdrawn in the report. compare_rows.py now refuses such a sum
outright rather than footnoting it, and was shown to fire both ways.

STANDS: the four pair rows are fresh, so 31.10 / 37.25 / 55.52 / 88.79 MHz and
the gate are unaffected.

REFIT QUEUE (six stale leaves, in the order they matter):
  zhao_terrain_normals    STALE 2  -- the shared-multiplier rearchitecture is
                                     unmeasured; expect 18 DSP -> ~3
  zhao_raster_fragment    STALE 2
  zhao_geom_binner        STALE 2
  zhao_texture_tmu        STALE 4  -- architect's territory, do not touch
  zhao_texture_bilerp     STALE 1  -- architect's territory
  zhao_raster_tilestore   STALE 1
NO ROW AT ALL: zhao_raster_blend_prod, zhao_raster_blend_fin, zhao_raster_fill.

NORMALS first when the toolchain frees: its row is both stale and the reason
the terrain pair looks the way it does.

## 2026-09-07 -- TERRAIN.NORMALS pipelined; the worst number in the tree attacked

The 31.10 MHz pair traced to one cycle in NORMALS holding a 6-way operand mux,
a 33x33 signed multiply, a sign-extend to 67 bits AND a 67-bit subtract -- with
a SECOND 67-bit adder hung off the same combinational product at the last step,
which a comment justified as "keeps the walk at 6".

Registered the product. Walk 6 -> 7 clocks, latency/II 7 -> 8; the contract says
`latency: variable` and that sentence already paid for the sequencing that made
this one multiplier. Prediction written INTO THE FILE, not just the commit: the
pair should move well above 31.10, not to 100, because TESS is the other half
and is unexamined. If the refit does not move it, the multiply was not the
limit and the comment is the record of a wrong guess.

61,833 checks pass across four lanes. Fire test: one accumulate arm reverted to
the unregistered product -> 4,840 of 20,003 random-differential failures, so
the suite is sensitive to precisely the off-by-one this change invites.

FIT QUEUE now, in order, once residency_v2 clears:
  1. zhao_pair_tess_normals   -- does the pipeline register move 31.10?
  2. zhao_terrain_normals     -- leaf row STALE 2, and now stale 3; expect
                                18 DSP -> ~3 from the shared-multiplier work
  3. zhao_geom_project        -- never measured
  4. zhao_pair_pagestream_patch
  5. zhao_terrain_residency_v2 re-read

## 2026-09-07 -- the owner's v3.1 brief landed, and M0 is answered from disk

The watcher fired on commit 9c4300fe "Agent please read - v3.1 rearchitecture":
reports/ZHAOZHOU_TEXTURE_V3_1_REARCHITECTURE_2026-09-07.txt, 4,344 lines. It
supersedes the CONTROL recommendations of the V3 architecture, not the texture
mathematics, and it explicitly incorporates my delivery through 756f08c --
citing the top-level census as "direct evidence that a control-only replacement
cannot be bolted under the old top-level capture/reorder scaffolding".

Its work order is M0-M8. M0: "Recover owner physical attribution and timing
endpoint classes; read the existing old-island census immediately, WITHOUT
QUEUING A REDUNDANT MAP." The FABLE architect independently ranked the same
work R1, first, ahead of everything else.

M0 IS ANSWERED, with no Quartus run -- the map report was already on disk:

  97% of the V3 owner's registers (4,085 of 4,220) and 98% of its ALUTs
  (6,416 of 6,532) are in zhao_texture_v3own ITSELF. The three ready queues
  cost 38-40 ALUTs each. The six banks cost ZERO ALUTs and ZERO registers --
  pure M10K, exactly as designed.

So the 3.15x area breach is the CONTROL PLANE, entirely. Not the banks, not the
queues. That is the strongest available confirmation of V3.1's thesis and it
came from evidence already on disk.

reports/V31-M0-OWNER-ATTRIBUTION-20260907.md. Names what it does NOT settle:
which PART of the control plane (that needs §19's ablations), whether the
replacement fits, and anything about the old island's 13,459 top registers.

tools/quartus/entity_census.py committed rather than retyped. Its first version
printed "UNINFERRED RAM (2)" then "0 ... 0" because its reason-regex was
`[a-z ]+` and stopped at the capital in "inappropriate RAM size". Caught within
a minute by the exactly-zero rule, from the tool's own docstring.

## 2026-09-07 -- position before the residency_v2 result lands

quartus_sta is up, so the fit is in its last stage. Written BEFORE reading it.

WHERE I AM: V3.1's work order M0 is done and pushed. Next is M1 ("fix
ready-queue occupancy and separate quiescence from hot admission") and, per §0,
M6's correctness test WRITTEN AT M0 even though the interface change lands
later -- "any existing behavior that accepts a final result before COMBINE has
accepted the corresponding input is not something to preserve for parity".

WHEN THE ROW LANDS: judge `min_memory_bits` against a FRESH row at last. The
old row FAILED on 150,528 bits < 167,936 required, and the prediction recorded
before the refit was 150,528/(256x4) = 147 bits per entry = 107 + 40. Its Fmax
row (61.38, core-to-core also 61.38 -- a set index into an M10K address port)
is equally stale and needs re-reading, not just the memory line.

ROADMAP NUDGE ITEMS, all three now answered or reassigned:
  D22 step 4 GEOM.PROJECT -- target written and committed (aea9d9d6); the block
      had NO target at all and 16 of 24 geometry blocks still have none. Queued.
  COMBINE.V1 DSP          -- ANSWERED: 2 DSP against a rule of 2, passes. Area
      (1,475 vs 800) and clock (36.28, no path summary) still open.
  perspuv per-axis split  -- now inside the V3.1 lane and the FABLE report; do
      not touch island RTL independently of them.

## 2026-09-07 -- residency_v2 landed: prediction exact, gate still unjudgeable

Fresh row 43b1ba2c: 2,234 ALM / 1,226 reg / 16 M10K / 150,528 bits / 60.25 MHz.

150,528 is BIT-IDENTICAL to the stale row, so the min_memory_bits FAIL is real
and the pre-refit prediction (147 bits/entry) is confirmed exactly. The RAM
Summary is sharper than the note's guess: keyram inferred at its full 107 bits,
statram at 40 -- the M10K's max port width -- and the missing top 17 are pin
count plus three flags, with crc(32)+seq(8) making the low 40.

NOT in flops (1,226 registers vs 17,408 bits). Beyond that nothing harvested can
say where they are, and I nearly claimed otherwise: the setup report's 1,324
"MLAB" hits are MLABCELL_Xn_Yn_Nn PLACEMENT COORDINATES of ordinary logic, not
memory. Corrected before it reached a report.

So min_memory_bits is LEFT ALONE. run_block_fit now harvests fit.rpt, whose
Fitter Resource Usage Summary separates M10K from MLAB -- the fourth instance of
"the evidence was deleted with the workspace", and the harvest block's own
comment had predicted exactly this failure mode.

Toolchain refilled immediately: zhao_pair_tess_normals is fitting, to answer
whether the NORMALS product register moved 31.10 MHz. Prediction on record: it
moves well above 31.10 but not to 100, because TESS is unexamined.

## 2026-09-07 -- the NORMALS prediction was wrong; TESS is the limiter

zhao_pair_tess_normals refit: 1,579 ALM / 1,574 reg / 9 DSP / 32.42 MHz.
31.10 -> 32.42, a 4.2% move for +56 ALM and +185 registers. I predicted "well
above 31.10". Wrong, and the comment I put in the RTL said what that means: the
multiply was not the limit.

Splitting the pair's 1,803 paths by which block each END sits in:

  wrapper lattice mem -> TESS   129 paths  -20.848  ->  32.42 MHz
  TESS -> TESS                  625 paths  -14.931  ->  40.11 MHz
  NORMALS -> NORMALS            693 paths   -3.752  ->  72.72 MHz
  TESS -> NORMALS (the seam)     66 paths   +2.780  -> 138.50 MHz
  NORMALS -> TESS                 1 path    +5.165  -> 206.83 MHz

TESS is the limiter twice over. The worst path is the lattice read turned into
a vertex Y in one cycle -- `vy[pend_slot] <= m_y` with m_y = fx_add_sat(...) --
through Add65 carry chains. The SEAM is fine at 138/206 MHz, which is the
opposite of the PAGESTREAM->PATCH seam.

NORMALS at 72.72 is still 27% short, so even a perfect TESS leaves this pair
failing. Whether 72.72 is an improvement is UNMEASURED: the pre-change fit had
no path summary at all.

Kept the NORMALS change -- the hazard it removed is real -- but it was aimed at
the wrong block and I should have split the paths BEFORE editing RTL. The pair
had no path summary, which is exactly why it was ungated for a fortnight; the
first move should have been to fit it and look, not to read the RTL and guess.

NEXT: TESS's lattice-read-to-vertex path is the terrain geometry lane's real
clock problem. Toolchain is idle -- queue geom_project and pair_pagestream_patch.

## 2026-09-07 -- TESS's cell_solid named, by census not by reading

Followed the pair's path census into TESS rather than editing again. Worst 200
TESS->TESS paths: sources solid(49), eg(43), ea, pend_last(85); destinations
vh(69), subpatch_rejected_o(42), f_kind, pend_slot. Those meet in exactly one
place -- `cell_solid`, zhao_terrain_tess.sv:304-316: a 64-iteration
combinational double loop, four comparisons per cell against ea*j_s and eb*j_s,
reducing to one bit that gates want_issue. j_s is a 4-bit REGISTER (1/2/4/8),
so those are runtime multiplies, not shifts by a literal.

Proposal written, NOT implemented: the window is a rectangle over an 8x8
bitmap, so `cell_solid = ((solid & win_mask) == win_mask)` with win_mask
REGISTERED. The win is registering it, which needs ea/eb/j_s stable a cycle
early -- and whether they are is a question about the run-cell walk that the
report does not answer. That is the same trade that just failed to pay on
NORMALS, so it gets established before it gets built.

reports/TERRAIN-TESS-CLOCK-20260907.md. Names TWO repairs, not one: cell_solid
(the 40.11 MHz TESS->TESS family) and the lattice-read-to-vy saturating add
(the 32.42 MHz family). Neither alone reaches 100, and NORMALS at 72.72 is
still 27% short, so the lane needs work in three places.

## 2026-09-07 -- TESS cell_solid rewritten as a mask; a coverage hole found

Behaviour-identical rewrite: 8x8 double loop with 4 comparisons per cell (256
comparisons, 128 multiply sites against a REGISTER j_s) becomes two 8-bit span
masks and an outer product (2 multiply sites, 16 comparisons). No latency
change, no state added, so the suites must pass unchanged -- 48,510 do.

FIRE TEST FOUND A COVERAGE HOLE: dropping the row term from the outer product
fails terrain_tess_directed (5 of 6,751) and is INVISIBLE to
terrain_tess_normals (41,731 all pass). The suite with six times the checks
cannot see the solidity window at all. Recorded; widening its stimulus to reach
void run-cells is its own change.

NEXT for this lane, in order:
  1. Fit zhao_pair_tess_normals again -- does the mask move 40.11?
     Prediction: TESS->TESS moves; the 32.42 lattice->vy family does NOT.
  2. If more is needed, register win_mask (no latency cost, five paired
     assignment sites, stale mask = wrong solidity answer).
  3. The lattice->vy family is a separate repair: vy[pend_slot] <= m_y lands a
     saturating add on the same edge as the memory read that feeds it.

## 2026-09-07 -- closed the tess_normals coverage hole

Cause: tess_harness.hpp's make_lattice fills cell_state with kSolid everywhere,
so the solidity window's answer was always yes across all 41,731 checks.

Fix: a SECOND sweep over make_island_voids(), existing sweep untouched. Voids
straddle the run-cell grid deliberately (a 4x4 block = a whole run-cell at
levels 0-2; a diagonal of singles = never whole above level 0). Asserts the
voids actually removed geometry -- 5,232 vs 5,440 triangles, 208 removed --
so the new sweep cannot be a re-test of the solid case.

Verified by re-running the SAME mutation: 0 failures before, 636 after.
47,221 checks pass on restored RTL.

## 2026-09-07 -- swept every suite sharing the tess fixture, by mutation

Ran the SAME mutation against all four suites that share tess_harness.hpp
rather than reasoning from greps:

  terrain_tess_directed   6,751     5 failed   punches voids
  terrain_tess_random     2,277    45 failed   random void cells
  terrain_tess_normals   46,709   636 failed   after today's fix (was 0)
  terrain_lod_tess           93     0 failed   CORRECT -- non-dual by design

lod_tess uses make_lattice(false) with job.dual=false, and the reference rule
is `sol = !lat.dual || substance == kSolid`, so solidity is unconditional on a
legacy page. "Does not detect" and "has a hole" are different findings; the
distinction is recorded so nobody widens a suite that is already right.

The hole was in the LARGEST of the four, and only there.

## 2026-09-07 -- V3.1 FIRST discharged, THIRD implemented

FIRST's second half: four-way endpoint split of zhao_texture_v3own@v3-full.

  core -> core  1,510 paths  -1.225  ->  89.09 MHz
  core -> port    239 paths  -3.194  ->  75.79 MHz   <-- the reported number
  port -> core    204 paths  -1.181  ->  89.44 MHz
  port -> port     47 paths  -1.104  ->  90.06 MHz

The brief's caution names the actual worst path: u_rq_tmu|wp_q[0] ->
adm_accept_o. Ten paths end at the admission outputs and they are the ten worst
in the fit. The chain is nameable end to end -- wp_q -> body_occ_c -> occ_o ->
rq_occ_c==0 -> quiet_c -> adm_ready_o -> adm_accept_o -- and it is exactly the
"broad quiescence feedback" §0 removes. Three routes found it independently.

AND MY OWN O1 REPAIR SITS ON IT. Adding ld_q to occ_o widened the sum feeding
the design's worst path. §5.2 warned in advance; recorded honestly rather than
discovered later.

THIRD implemented, which is the remedy rather than a revert:
  - CAPACITY parameter separated from DEPTH (§5.3: BDEPTH is physical)
  - registered lcnt_q = accepted pushes - accepted pops
  - full_o and the new owned_empty_o both from the LOGICAL count
  - occ_o becomes lcnt_q, so no future stage can fall out of the accounting the
    way ld_q did -- the CLASS is removed, not just the instance
  - no same-cycle full/pop bypass, per §5.3's explicit warning
  - v3own's quiet_c now uses owned_empty_o (§5.1's distinction), and rq_occ_c
    is kept as the optional diagnostic with a reasoned lint waiver

NO STALENESS INTRODUCED, and the argument matters because a registered
occupancy feeding a DRAIN is where a one-cycle lag would be a correctness bug:
wp_q/rp_q are registers too, so the old body_occ_c at cycle N already reflected
transfers through N-1. Same visibility, same edges.

## 2026-09-07 -- FOURTH part one; the fence needs a phase machine, not a register

Registered owner credit landed, taken from live_next_c so it is EXACT rather
than one cycle late. Fire test: taking it from live_cnt_q instead trips
a_no_live_overwrite immediately -- the 65th-owner hazard, predicted then shown.

The fence is NOT registered, and the reason is in the RTL: wrap_block_c becomes
true on the edge that moves tail_q onto the wrapping slot, so a registered
permission would still be asserted for that cycle and ONE admission could pass
on a non-quiescent island. That is the generation-reuse hazard.

Established the net BEFORE touching the protocol: deleting the fence fails case
19 twice by name. A phase machine (next-state gen/tail + reopen for exactly one
admission after quiescence) is what FOURTH actually asks for, and it is not
being written in the pass that measured the need.

STATE OF THE BRIEF'S TEN INSTRUCTIONS:
  FIRST   done -- entity attribution + four-way endpoint split
  SECOND  done -- occ_o repair, consumer audit, omitted-pending mutation fired
  THIRD   done -- registered logical credit, CAPACITY split from DEPTH
  FOURTH  part one done (credit); part two (fence phase machine) specified
  FIFTH.. not started
  M6's test written early per §0, running as a WILL_FAIL lane

## 2026-09-07 -- GEOM.PROJECT landed; the shell claim is true, my gate was not

5,977 ALM / 6,570 reg / 27 M10K / 33 DSP / 61.09 MHz, against terrain_project's
6,068 / 6,685 / 23 / 33. Same size.

Entity census splits the header's two claims: the SHELL is 41 ALUTs (0%) --
"thin shell" CONFIRMED -- while "the duplication is gone" is unmeasurable by a
leaf fit, because each block instantiates its own core and sharing a module
definition is not sharing hardware.

max_m10k: 0 was UNSATISFIABLE. The 27 M10K are all inside u_core; I had
attributed terrain's 23 to "triangle framing" and they are the core's. A rule
about the shell applied to a closure containing the core. Third such rule in
that file. Corrected to 30 with the reasoning kept.

zhao_project_core is 39% short of the product clock, core-to-core, and it is
instantiated on BOTH the geometry and terrain lanes.

Toolchain refilled immediately: pair_tess_normals refitting to test the
cell_solid mask. Prediction on record: TESS->TESS moves off 40.11; the 32.42
lattice->vy family does not.

## 2026-09-07 -- project_core's clock: a DSP output register nobody used

61.09 MHz, core-to-core, and the block is on BOTH the geometry and terrain
lanes. Path anatomy from the setup report: multiply -> add -> add -> saturate
-> add in one cycle, 15.906 ns, with the DSP's combinational output worth
3.762 ns of it.

Registering the product splits it 6.611 / 9.295 ns -- both inside 10 ns, so one
cut may suffice. Prediction recorded with its derivation and its falsifier.
NOT implemented: one cycle of latency on two lanes, and the last two pipeline
registers put in on a reading bought 4.2%.

Nearly edited rescale16_row instead (68-bit add + two 68-bit compares; there IS
a bit-exact narrowing since (x + 2^15) >>> 16 == x[67:16] + x[15]). The per-hop
numbers say it is not on this path. Checked before editing this time.

THIRD coverage asymmetry: terrain_project_directed (2,011 checks) is blind to
rescale16_row's rounding because its matrices make row products exact;
geom_project_directed catches it in 22 of 900. Same shape as the tess fixture.
Recorded, not fixed -- different lane from the one being measured.

## 2026-09-07 -- closed the third coverage asymmetry (terrain row rescale)

terrain_project_directed was blind to rescale16_row's rounding because every
matrix uses m00 = kOne, making row products exact. Case 7 reaches the fx_mad
half; nothing reached the ROW half.

Case 7b uses m00 = 1 RAW so the row product is the world coordinate itself,
sweeping ...7FFF / ...8000 / ...8001 across seven multiples of 0x10000, and
counts the halves it actually hit (instrumentation only, the file's own idiom).

  before  2,011 checks   0 failed on the mutation
  after   2,264 checks  42 failed

Three coverage asymmetries closed or recorded today, all one shape: a fixture
too regular to exercise the rounding it protects. tess_harness.hpp names this
failure mode explicitly and it keeps recurring.

## 2026-09-07 -- committed the mutation sweep

Three coverage holes of one shape today, all found by the same hand technique.
Committed as tools/maintenance/mutation_sweep.py so it stops living in whoever
remembers it.

Safety shaped by the recorded zero-byte-backup incident: never writes its own
backup, refuses unless the target is clean in git, restores via git checkout in
a finally, verifies byte-for-byte, refuses if git is unreachable. Also refuses
an ambiguous --find.

Its own safety path had a defect Python caught and I did not: a `return` inside
`finally` discards exceptions passing through it. Removed.

Verified three ways: ambiguity refusal, dirty-file refusal, and a real sweep
where both project suites now detect (22/900 and 42/2264) with the file
restored and verified.

## 2026-09-07 -- the cell_solid mask bought 2.1%; the prediction was wrong

32.42 -> 33.10 MHz, ALM 1,579 -> 1,574 (five). TESS->TESS went 40.11 -> 37.23,
the wrong direction. NORMALS->NORMALS 72.72 -> 85.50.

Cause of the wrong prediction, nameable: I derived the mask by COUNTING
OPERATORS IN THE SOURCE rather than measuring where the delay was. Quartus had
already collapsed the loop -- five ALMs is the proof. The endpoint census names
which signals meet; it does not name where the time is.

Per-hop data for the worst path: 2.5 ns memory clock-to-out, then Add65/Add66
carry chains. 28.080 ns total, cell_solid nowhere in it. That is the OTHER
repair the report already named and ranked bigger -- and I did the smaller one.

Mask kept: bit-identical, cheaper source, coverage hole closed. Not recorded as
a timing win.

NEXT: register the lattice sample before fx_add_sat. Per-hop numbers on disk to
size it before writing this time.

## 2026-09-07 -- reached is not observable: the fx_mad case now can fail

Case 7 asserted it reached the fx_mad half and was RIGHT (534/801), yet the
mutation went undetected: scr_fx is fx16, out is S12.8 via (x+128)>>8, so a
1-LSB mad error is 1/256 of an output LSB and survives only when
scr_fx mod 256 == 127.

Histogram over 801 vertices: scr_fx mod 256 landed only in [0,31] and
[224,255]. m33 = 3 makes ndc = c*65536/3, so scr_fx steps by an exact multiple
of 256 -- the sweep step aliased to the output quantisation. Searched every
viewport 1..32 x divisor 1..16: NO combination escapes, because for an integer
divisor ndc mod 512 takes only d values.

Constructed instead: need ndc*vp == 255 mod 512; for vp=3 that is ndc = 85,
597, 1109, 1621, and m33 = kOne makes ndc == clip.x so it can be asked for.

  before  2,264 checks  DID NOT NOTICE
  after   2,313 checks  detected, 8 failed

geom_project_directed still blind -- and WITHDRAWN as a hole an hour later.
For ANY EVEN viewport dimension, vp<<15 has >=16 trailing zeros, so mad's low
16 bits are always zero and the rounding is UNREACHABLE. That suite uses
256x192 and 320x200 -- all even -- so it is correct, not blind. See
reports/PROJECT-MAD-ROUNDING-20260907.md.

## 2026-09-07 -- TESS's 28 ns is the geomorph blend, measured per hop

Not cell_solid. The chain is m_dab -> rescale1 -> m_hc -> m_d -> m_prod (a
17x34 multiply) -> rescale16 -> m_y -> vy: seven arithmetic stages including a
multiply, all on the same edge as the lattice read. Add65/66/67 plus 113 small
carry hops.

Needs AT LEAST THREE CUTS (two gives 9.4 ns stages with no margin). Boundaries
at m_hc, m_prod, m_step. Three cycles of latency; `latency: variable` covers it.

NOT implemented -- the last two attempts on this lane were written from a
reading and this is the first end-to-end measurement. Next pass has per-hop
numbers to size each cut and the mutation sweep to prove the suites still see a
wrong answer afterwards.

Order of learning on this block: NORMALS product register (+1.3), cell_solid
mask (+0.7, 5 ALMs), then the measurement. Wrong order, and now written down.

## 2026-09-07 -- committed path_anatomy; it corrected me on its first run

The per-hop walk had been hand-rolled four times. Committed as
tools/quartus/path_anatomy.py.

It immediately showed my hand extraction was PARTIAL: I stopped at Add67 and
reported "three chained adders". The real path is FIVE adders and a two-stage
multiply, and Mult4's two DSP hops (3.938 + 2.569 = 6.507 ns) are the two
biggest in the path -- 23% of the 28.080 ns.

DSP output registers unused here too, same as zhao_project_core. Same free cut
available in both.

The tool sums and counts sub-threshold hops rather than dropping them: 155 hops
under 0.30 ns are 2.692 ns.

## 2026-09-07 -- path_anatomy bug found by an impossible number; DSP sweep

Sweep reported bilerp_lane with 9.792 ns of DSP inside a 3.741 ns data path.
262% cannot happen. Cause: the segment was a flat 30,000 chars, which spans
SEVERAL short paths. TESS's 28 ns path masked it; bilerp's 3.7 ns did not.
Bounded at the next path header, plus a containment check that warns when
attributed hops exceed the reported data path.

Corrected sweep:
  zhao_geom_project        15.906 ns data,  6.611 DSP  42%
  zhao_pair_tess_normals   28.080 ns data,  8.599 DSP  31%
  zhao_texture_bilerp_lane  3.741 ns data,  0.729 DSP  19%

Both blocks that miss the clock spend a third to a half of their worst path in
a COMBINATIONAL DSP output -- resulta as a CELL delay, not through the DSP's
own output register. Terrain and geometry converge on one mechanical fix.
bilerp is comfortable and needs nothing.

## 2026-09-07 -- project_core: the product is registered

The cut chosen by measurement, not reading: path_anatomy put 6.611 ns of the
15.906 ns path at the registered-product boundary, leaving 9.295 ns after.
Both inside 10 ns, so ONE cut.

Checked BEFORE editing: terrain_project_directed bounds at 3N+64 = 448 and
measured 422. After: 423. Exactly +1 -- latency, not initiation interval.

Two aliasing traps handled: cx13/cy13 ride along registered (vp_*[s5_view] read
a cycle later would take the NEXT vertex's viewport), and s6_valid joins busy_o
(a stage missing from that reduction reports idle while holding a vertex --
the same defect as the ready queue's omitted pending read).

All nine project lanes pass. Fit queued behind residency_v2 to judge the clock.
Prediction on record: the second half lands under 10 ns; if not, the split was
wrong and the chain needs a cut between Add118 and the saturation.

## 2026-09-07 -- lint_shell_top was red; a fix that never reached the producer

Checking that project_core's new stage did not break the shell (geom_project is
in ZHAO_SHELL_RTL) found lint_shell_top ALREADY red -- verified by linting with
the previous core, which fails identically. Not my regression.

hb_wr_ready / hb_wr_early are the tail of 98d7030e ("the HPS bridge's write
channel had no READY, so a beat offered a cycle early vanished"). The bridge
raises READY, the shell wires it out, nothing connects it back to the producer.
Harmless today ONLY because both arbiter client write ports are tied to 1'b0.

The trap: zhao_hps_arbiter has no b_wr_ready_i -- b_wr_valid_o is a pure
output. The READY cannot be honoured by wiring; it needs an arbiter port and a
stall in its write mux. Until then 98d7030e is present in the bridge and INERT
at the shell.

Sunk with the reasoning rather than deleted; the lane is green. The full shell
ctest is still running alongside the fit.

## 2026-09-07 -- TESS's three cuts sized by search; shell suite green so far

Enumerated all three-cut sets over the measured boundaries. Best worst-stage
8.599 ns -> 116.3 MHz, cuts after Add66 | Add68 | Mult4 (register m_half, m_d,
m_prod). EVERY viable set cuts after the multiply -- same DSP finding as
project_core.

Falsifier stated: if TESS->TESS (37.23) is the same chain from a register, the
same cuts fix it and the block nears 100; if not, it lands near 37.

Shell suite (checking project_core's new stage): 9 of 16 done, all Passed,
including shell_project_path_directed (167 s) and shell_clip_path_directed --
the two that most directly exercise the projector. lint_shell_top green after
the hb_wr_ready repair.

## 2026-09-07 -- residency_v2 refit: MLAB refuted, the gate holds

Third fit, 6c1d1fa3: 150,528 bits again, prediction held exactly. But fit.rpt
(harvested this morning FOR this question) says Memory LABs = 0 and ALMs used
for memory = 0. The missing 17 bits/entry are NOT in MLAB, not in registers
(1,226 vs 17,408), not removed (3 registers, all s0_ev~n), and not in a ninth
memory (8 rows total 150,528 = the fitter's own figure).

Verified from source rather than assumed: SETS 256, WAYS 4, PINW 6, SEQW 16 ->
STATW 57, KEYW 107, declared 167,936 = the rule's threshold exactly.

Eliminated: written-from-constants (s0_crc is input-derived), s_pack too narrow
(it is 57 bits), nothing-reads-them (all six accessors used, two in
comparisons).

Absent: pin(6) + bd + f + mips + crc[31:24] -- EIGHT BITS OF THE PAGE CRC.

GATE HELD. Would have been the third relaxed on a refuted diagnosis; first two
were caught after the fact. Experiment queued: declare statram 40 wide, change
nothing else -- same numbers proves the bits were never there.

Toolchain refilled: geom_project refit running to judge project_core's cut.

## 2026-09-07 -- TESS's cuts are not a drop-in; checked before writing

Two pre-edit checks. (1) terrain_tess_directed:676 bounds cycles <= 3N+75 =
459 and measures 456 -- three cuts land exactly on it, zero margin, on an
assertion whose comment calls 3 reads/triangle the target rate. (2) m_y is
consumed IN THE CYCLE IT IS PRODUCED, twice: vy[pend_slot] <= m_y AND
o_by <= last_y (= m_y when pend_kind != 0) at pend_last. Registering three deep
makes the emit read stale vertices.

So the cuts need a WALK RESTRUCTURE, not three registers. project_core's cut
was mechanical because stage 6 is pure feed-forward; TESS's geomorph feeds
state and emission on the same edge.

Sizing stands (Add66|Add68|Mult4, 8.599 ns, 116.3 MHz). Left for a pass that
owns the block, with the hazard documented.

## 2026-09-07 -- OWNER DIRECTION RECEIVED: texture first, terrain deferred

Commit 49fc32e9. Acknowledged with SHA-256 in
fpga/rtl/texture/OWNER-DIRECTION-TEXTURE-FIRST-2026-09-07.md (beside the RTL,
not in this run folder -- the handoff and CLAUDE.md agree on that point).

  handoff  B024D625...48EE7DB9
  audit    E918D9AF...C50A20C8

Compute decision recorded rather than improvised: the running geom_project fit
finishes (10 min into 50, judges a committed prediction, frees nothing texture
can use yet since the next milestone is RTL). No further non-texture fit
queued; the three next in line are cancelled by name. Nothing reverted.

NEXT: FOURTH part two, the acknowledged fence. Hazard already characterised and
case 19 already fired on purpose.

## 2026-09-07 -- FOURTH part two written; and a self-inflicted build trap

THE FENCE IS IMPLEMENTED. Master handoff §6.1 states the hazard in the same
words this file used when it stopped short: "do not put a flop on wrap_block
and call it solved ... compute the permission from the same NEXT-STATE
tail/generation event that commits". So:

  tail_next_c    = tail_q + (adm_fire_c ? 1 : 0)
  wrap_block_n_c = (gen_n_c[tail_next_c] == all ones)

and a four-phase machine (OPEN / STOP / FINISH / REOPEN) per §6.2, scoped to
the "legacy transitional fence" that authorises EXACTLY ONE wrapping admission
and then resumes normal checks.

adm_ready_o is now `credit_ok_q && fence_open_q` -- quiet_c is OUT of the
admission cone, which §6.1 requires and which removes the design's worst path
(-3.194 ns, wp_q -> occ_o -> quiet_c -> adm_ready_o -> adm_accept_o; ten of the
ten worst paths ended at these outputs).

TWO OF §6.2's PHASES ARE NOT IMPLEMENTED, deliberately and named in the RTL:
QUIESCE_PRODUCERS and DRAIN_RESIDUAL_TRANSPORT need a producer ACKNOWLEDGEMENT
interface that does not exist, and §6.3 is explicit that "a debug idle signal
is not a cancellation agreement" -- synthesising one from quiet_c would be the
delayed-quiet-bit §6.1 forbids.

fn_slot_q / fn_gen_q were latched but unread (lint caught it). Rather than
waive them they now do §6.2's job -- "every required acknowledgement belongs to
the HELD request" -- via three assertions: the reopen's slot and generation
match the held request, and the permission authorises exactly one admission.

THE BUILD TRAP, SELF-INFLICTED. A stale Verilator object gave an undefined
ConstPool reference at link. I removed the target's generated directory, which
deleted a `_copy.cmake` that build.ninja's OWN REGENERATION depends on --
turning a link error into "ninja: error: rebuilding 'build.ninja'". CLAUDE.md
documents exactly this and its remedy: regenerate through `cmake --preset`,
never through another `cmake --build`. Doing so now.

THE LESSON: rm -rf on a verilate target directory is the wrong first remedy for
a stale-object link error. `cmake --preset` was the right move from the start,
and the trap is documented one paragraph away from the symptom I hit.

## 2026-09-07 -- FOURTH part two lands; my first fence was worse than none

First version put the wrap test only in the phase TRANSITION, not the
permission. After 64x255 admissions every slot is at generation 255, so the
first wrapping admission walked through FN_OPEN. Case 19 failed TWICE: 32 early
reopens on the new check and the pre-existing "wrapping admission on a
QUIESCENT island" went red. My fence was worse than what it replaced, and the
test said so before the commit.

Fixed: fence_open_q <= ((fn_n_c == FN_OPEN) && !wrap_block_n_c) || REOPEN.
wrap_block_n_c at N-1 answers "would an admission at N consume an exhausted
slot", which is exactly what §6.1 asks for.

adm_ready_o = credit_ok_q && fence_open_q -- quiet_c out of the admission cone,
which removes the design's worst path. Two §6.2 phases absent and named
(producer ACK interface does not exist; §6.3 forbids faking it from quiet_c).

All six v3 lanes green. Next: T1's remaining deliverable is a scoped
before/after timing classification, which needs a zhao_texture_v3own fit --
texture work, permitted, queued behind the running geom_project.

## 2026-09-07 -- stopped the stalled shell suite; why, and what it had proved

The shell ctest sat at 12/16 with zero CPU for over an hour. Cause is mine: I
ran `cmake --preset windows-native` mid-flight to repair the build.ninja trap,
which regenerated the tree underneath a running suite.

Stopped it deliberately rather than leaving it, and verified no ctest process
survived -- CLAUDE.md: "stopping an agent does not stop its background work;
kill the background tasks too, then verify nothing is running before assuming a
lane is closed."

WHAT IT HAD ALREADY PROVED, which is why stopping costs nothing: 12 of 16
passed, including shell_project_path_directed (167 s) and
shell_clip_path_directed (188 s) -- the two that exercise the projector whose
pipeline stage prompted the run -- plus meshfetch, assemble, vdecode,
assetfetch, indexfetch, realmem, depth, setup and draw. lint_shell_top went
green after the hb_wr_ready repair. The four unreached tests are shell paths
that do not instantiate the projector.

It was also NON-TEXTURE work under the owner's texture-first direction, so
freeing the lane is the instructed behaviour rather than a convenience.

## 2026-09-07 -- position before the geom_project result

quartus_sta is up, so the fit is in its last stage. Written BEFORE reading it.

WHERE I AM: texture lane, T1 functionally complete (next-state credit,
registered permission, drain FSM, quiet_c out of the admission cone, case 19
extended with §13.2's two DRAIN/RESET properties, six v3 lanes green). T1's
remaining deliverable is the scoped before/after timing classification, which
needs a zhao_texture_v3own fit -- queued for the moment this fit releases.

THE BEFORE-PICTURE IS ALREADY RECORDED so the comparison is like-for-like:
  core -> core  1,510 paths  -1.225  ->  89.09 MHz
  core -> port    239 paths  -3.194  ->  75.79 MHz   (all ten worst end at the
  port -> core    204 paths  -1.181  ->  89.44 MHz    admission outputs)
  port -> port     47 paths  -1.104  ->  90.06 MHz

WHEN THE ROW LANDS: it judges project_core's registered product against a
prediction on record -- 6.611 / 9.295 ns, both inside 10 ns, so the second half
should land under 10 and the block should move off 61.09 MHz. If it does not,
the split was in the wrong place and the chain needs a cut between Add118 and
the saturation. That is non-texture work and will be RECORDED, not pursued.

## 2026-09-07 -- project_core 61.09 -> 73.62 MHz (+20.5%); next limit named

Prediction half falsified: said ~107, got 73.62 (13.58 ns vs the 9.295
predicted). Falsifier was on record and fired. The cut was still right -- the
error was assuming the remaining chain was next.

New worst path: mat -> row_x -> Mult0~124|resulta (3.938 ns, a DIFFERENT DSP)
-> Mult0~93. The ROW TRANSFORM, not the viewport mad. Same shape, output
register unused. Next cut would be row_x/y/w registered before rescale16_row.
NOT PURSUED -- texture first; recorded for the pass that owns it.

path_anatomy's containment check fired on its own tool again: bounding at the
next "Data Arrival Path" still spanned into the "Data Required Path" rows.
Fixed. Second self-caught defect in that tool.

Toolchain refilled with TEXTURE: zhao_texture_v3own fitting, for T1's scoped
before/after timing classification. Fence checks proven sensitive by
mutation_sweep (detected, 2/469, restored byte-for-byte).

## 2026-09-07 -- the owner's handoff caught 382 registers I had hidden

§10.1 quotes my island census and corrects it: rows total 27,591, not 27,973,
leaving 382 unexplained. Correct. My ad-hoc script computed "in named blocks"
as total MINUS top-self (14,514) -- a subtraction presented as a sum. Children
actually total 14,132; 382 registers and 553 ALUTs belong to neither.

A subtraction can never show a remainder because it defines one away -- the
same shape as the anti-vacuity failures found elsewhere today.

The COMMITTED tool already summed correctly (14,132). The published error was
in the throwaway script, which is the rule about committing probes earning
itself again. entity_census.py now prints the remainder explicitly.

## 2026-09-07 -- §10 L0 inventory; and a backspace in my own regex

L0 done (analysis only, no storage-lane RTL, no worktree -- this session holds
the control lane). All eight named arrays are SINGLE-writer, SINGLE-reader, so
§10.1's three-access warning does not bite. Seven share one write event
(always_ff@708, the ingress capture): eight fields of one record on one event,
which is what makes §10.4's consolidation available.

Reads are CONTINUOUS assigns -- hence "uninferred due to asynchronous read
logic". Port schedule clean; the obstacle is read timing, which §10.2/§10.3's
credited joins absorb.

Recorded what L0 does NOT license, since -10,304 registers has been quoted
twice today without it: several arrays are 32-512 bits, far below an M10K, so
individual conversion spends a block on 32 bits. §10.4's shared rows are the
point.

MY SCRIPT HIT A DOCUMENTED TRAP TWICE: first it tracked block starts but never
ends (so continuous assigns read as registered); then the fix's regex ended in
a \b that a heredoc turned into a LITERAL BACKSPACE, so it matched nothing and
printed the reassuring answer. CLAUDE.md records this verbatim. Found with
cat -A. Write scripts to a file, not through a heredoc, when they have escapes.

---

## POSITION BEFORE THE v3own FIT LANDS (written first, deliberately)

CLAUDE.md: "when the fit comes back: write down where you were BEFORE reading
it. Fit results redirect the work -- that is what they are for -- and the
half-finished thing you were holding in your head is exactly what gets lost."

**In progress:** §12 of the master recovery handoff, the allocation that must
balance. Two reports landed and are pushed (`c80e7a48`, `2c95a19e`).

**What the fit is for, and its before-picture — do not re-derive these:**

* TIMING (T1's last deliverable). Before: core→core 1,510 paths, −1.225,
  **89.09 MHz**; core→port 239 paths, −3.194, **75.79 MHz**; port→core 89.44;
  port→port 90.06. All ten worst paths ended at `adm_accept_o` / `adm_ready_o` /
  `adm_owner_o` via `wp_q → occ_o → rq_occ_c == 0 → quiet_c`.
  **Prediction on record: those ten are gone**, because `adm_ready_o` is now
  `credit_ok_q && fence_open_q`. If they are still there, the registered credit
  did not reach the output and the change is cosmetic.
* MEMORY. Before: **17 M10K / 20,640 bits** at source `0f5ce601`. The per-
  instance walk accounts for 19,584 across nine `v3bank` bodies and leaves a
  **1,056-bit / 8-block REMAINDER**, deliberately unattributed. The fit's
  per-RAM summary is the instrument that closes it, and this is the FIRST fit
  of this block to harvest `blockfit.fit.rpt`, so look for that table.
  Neither change was expected to move block memory — both are logic.
* ALM. Before: **5,678 against `max_alms: 1800`**. §12.4 says retain-and-fail,
  reconcile the island, do not relax the gate.

**Next step regardless of the result:** the eight named refits in
`reports/V31-ISLAND-BUDGET-BLOCKED-20260907.md` — every one a texture-island
block, so the list is inside the owner's texture-first direction, not a detour.
`zhao_texture_material_combine_v2` is first: it is the island's live combiner,
has NEVER been fitted, and its target was added today. Do NOT launch it while
v3own is running (handoff action 4 forbids a competing fit).

**Still open in §14, unchanged by this fit:** T4 (turn M6 from WILL_FAIL into a
passing test — `cbi` on `cmb_valid && cmb_ready`), then T2/T3/T5/T6. T4 edits
`zhao_texture_v3own.sv`, which is IN the running closure, so it cannot start
until the fit exits.

## While the fit ran: V3.1 §5.7, the local queue fit gate

§5.7 asks for it before the owner demo is rebuilt, and it did not exist.

* `fpga/rtl/synth/zhao_probe_v3rq_queue.sv` — registered stimulus → DUT →
  registered hash sink, in the shape of the four pair wrappers. Production
  shape from v3own itself: WIDTH = OWNERW = 14, DEPTH = 64, **CAPACITY = 64
  explicitly** (§5.3: a body of 64 plus two heads must not advertise 66).
  Every status output folded into the hash so the fitter cannot delete the
  logic that produces it. Verilator `-Wall` clean.
* Lint flagged `stim_q[29:14]` unused. Fixed by USING the bits — unused
  stimulus lets the fitter fold the datapath and report a queue cheaper than
  the one that exists. Low half XORs into write data; last two modulate pop
  duty, which is §5.7's "sustained pop rate".
* `design/fit_targets.yml` target with `min_fmax_mhz: 100` (§5.7's "intended
  product constraint", stated not invented), `min/max_m10k: 1` (from the
  instance walk), `max_dsp: 0`.
* **NOT LAUNCHED** — handoff action 4 forbids a competing fit.

### The simulation half, and two things it taught

`full_o` was not checked ANYWHERE before today. It is the output the registered
logical credit exists to drive, so an off-by-two would have shipped as two
owner credits the island does not have, with every existing check still green.

**Fire-tested rather than assumed**: re-verilated with `-GCAPACITY=66` — the
exact defect §5.3 names — and four checks failed with `got=0x42`. Note the RTL
was **not** edited to do this; `zhao_texture_v3rq.sv` is in the running fit's
closure, and a parameter override mutates elaboration without touching the
working tree. That is the technique to reuse whenever a fit is live.

**My first version of the test was wrong and the RTL said so.** It drove
`wr_en_i` into a full queue; `a_rq_no_write_when_full` stopped the run. Writing
while full is a producer error the queue may assume never happens, not a case it
absorbs. The test now obeys that and checks what the island depends on: `full_o`
stays asserted and occupancy holds at capacity, so it never glitches low and
re-opens admission. 21 checks pass.

### Build note worth keeping

`cmake --build` was avoided (documented stale-binary race). Direct path that
works while a fit holds the tree: `verilator_bin.exe --cc` into a build dir
**without spaces** (GNU Make refuses spaces, and the scratchpad path contains
"Fabian Trunz"), then `g++ -static -static-libgcc -static-libstdc++` over the
generated sources plus `tests/harness/zhao_sim.cpp`. Without the static flags
the exe dies at 127 on missing DLLs, which looks exactly like a crash.

## T2's identity comparison landed (still outside the fit's closure)

`tests/texture/texture_v3_window_identity.cpp`, 20 checks, pure C++ — see
`reports/V31-T2-WINDOW-IDENTITY-20260907.md`. Interval and literal table agree
on every one of 16,384 tokens across >1M comparisons and many namespace wraps.

**§6.1 is wrong in one sentence** and the test caught it: pointer equality DOES
distinguish empty from full at a 14-bit ticket width against 64 capacity. `used`
survives on §6.4's grounds (registered pre-edge permission), not §6.1's.

**Still not done for T2:** snapshot races, and the actual replacement of
per-slot generation access — the latter edits `zhao_texture_v3own.sv`, inside
the running closure.

## T4 LANDED — and the reason it could have landed hours earlier

`383e45df`. `cbi_q` now means §11.1's **event 3** (actual acceptance on
`cmb_valid && cmb_ready`); a new `crs_q` inherits **event 2** (the credited
reservation). Previously a final could be accepted, and its payload written, for
an owner whose COMBINE input had never been taken.

Default lane **469 checks pass** — "preserving all other finals and stalls" holds.
`ZHAO_M6` lane **477 pass**: M6 was a documented expected failure and now passes.

**The blocking belief was stale, and that is the expensive part.** I deferred T4
for hours on `QUARTUS_GOTCHAS` §11's live-tree rule. `run_block_fit.ps1` has
SNAPSHOTTED its sources since 2026-09-03 — it prints *"snapshot: N source(s)
copied into the workspace; the live tree cannot reach this fit"* on every run,
and its own comment says *"an ordinary edit to the live tree now cannot affect
this run at all."* §11 still opened with "Nothing is copied into the workspace".
Corrected in `0487f1c3` with a superseding box.

**What is NOT superseded:** §13 — `design/fit_targets.yml` IS still read live,
once per block at preflight. I edited it twice today with a truncating write
during a fit; safe only because this is a single-block run long past preflight.
Config and sources now have different rules.

### Still to do on T4
Remove the `ZHAO_M6` env guard around case 22 and delete the `WILL_FAIL` ctest
entry `texture_v3own_m6_final_before_accept` — a WILL_FAIL test that passes is
itself a ctest failure, which is the designed signal. Waiting on the pre-change
fire-test build that is using the current test file.

### T4 finished, with the falsifier run

M6's `ZHAO_M6` guard and the `WILL_FAIL` ctest entry are both gone; case 22 runs
in the default bench, now **477 checks**.

| lane | before T4 | after T4 |
|---|---:|---:|
| default | 469 pass | 469 pass |
| M6 | **4 FAILED** | 477 pass |

The "before" column is a real run: the pre-change RTL was extracted from git and
rebuilt against the same bench. Its four failures ARE the defect. 469 before and
469 after is what "preserving all other finals and stalls" looks like as
evidence rather than as a claim.

### §5.4's throughput property, measured for the first time

*"The two-head organization can sustain one external pop per clock after warmup
with continuous supply."* Nothing checked it — every other check in that file is
a correctness check, and a queue delivering 0.9 pops/clock satisfies all of them.
Zero bubbles in 400 cycles. Fire-tested by starving supply to one cycle in four:
300 bubbles, 100 pops of 400.

### A third stale document, found by a gate rather than by reading

`design/prod_manifest.yml` claimed *"V2 is not instantiated by anything yet"* and
declared it absent because *"counting a block the machine does not contain would
inflate every budget"*. **The island contains it**, unconditionally, line 1956,
no generate block in the file. The justification runs backwards — the live
combiner is UNDERSTATED. With no fit target until today, its cost was invisible
in both accounting domains at once.

I also corrected my own §12.4 report, which had called that divergence
"declared". True of `prod_top`, false of the entry as a whole — the comfortable
reading explained most of the evidence and stopped me reading the rest.

### Ready for the fit landing (nothing left to debug then)

* `split_setup_paths.py` verified working. Compare against **75.79 reported /
  89.09 core→core**, worst core→core `zhao_texture_v3rq:u_rq_t -> Mux2~4_OTERM3179`.
* `blockfit.fit.rpt` is confirmed in the harvest list (added today), so this is
  the first fit of this block to yield a per-RAM table — the instrument that
  closes the 1,056-bit / 8-block remainder.
* The fit predates T4; it answers T1's question, not T4's.

---

## THE FIT LANDED — T1 answered, and it found my own regression

`reports/V31-T1-FENCE-FIT-20260907.md`. Scope checked first (both rows 952
virtual pins, same memory geometry) because T7 requires matched scope.

| | before | after |
|---|---:|---:|
| reported | 75.79 | **77.16** |
| **core→core** | **89.09** | **77.16** |
| ALM | 5,678 | 6,094 |
| registers | 4,864 | 4,756 |
| M10K / bits | 17 / 20,640 | **unchanged** |

**Prediction held.** Not one of the worst 25 paths ends at an admission output;
the ten that did are gone, and reported/core→core now coincide. Memory did not
move, as predicted.

**And the fence I added is now the worst path** — 89.09 → 77.16 internal. Both
new critical families (`fence_open_q` self-loop, eight `credit_ok_q → Mux*`) are
mine. Reported Fmax improved 1.8%; the real limit got worse, and saying only the
first would be the flattering half of a true sentence.

**Diagnosed and fixed** (`df1deda7`): 71% of the 12.804 ns path was the 64-way
`gen_n_c[tail_next_c]` select. Rewritten to precompute both candidates from
registers — exact, because `gen_n_c` differs from `gen_q` at exactly one index
and `tail_next_c` never points at it. Timing benefit UNMEASURED; a refit decides.

**The strongest result is the third family:** `gen_q[40][4] → ev_err_issue_o/iss_q`,
independent of anything I changed. That is T2's target, and §6.8 predicted the
mechanism. T2 now has a measured timing case, not only a register-count one.

## 1,056-bit remainder CLOSED

The first harvested `blockfit.fit.rpt` gives the RAM table: 15 RAMs, 20,640 bits,
17 M10K — matching the ledger exactly. The remainder is the `cq_*`/`oq_*` bodies.
My rejected hypothesis was wrong by exactly 112 = the two 14-bit `*_own` fields,
which stayed in fabric. Refusing to publish it was right.

**New finding: 7 of 17 M10Ks hold 1,056 bits** — four-deep queues at 0.7%
utilisation, "Fits in MLABs: No — Unsupported Port Usage". §12.3's 69-block
proposal counts planes and does not anticipate this.

## Now running / next

* `zhao_texture_material_combine_v2` FIT LAUNCHED — the island's live combiner,
  never fitted, first of the eight §12.4 blockers.
* Then a v3own refit carrying T4 + the fence rewrite (this fit predates both).
* Then `zhao_probe_v3rq_queue` (§5.7).

## T2 STEP 1 — the window asserted against this RTL, not just against a model

`reports/V31-T2-REPLACEMENT-PLAN-20260907.md` has all 12 `gen_q` sites in three
groups (3 disappear, 9 are one shared validity test, 1 is a small lookup).

**The reframing that matters:** three of §6.1's four fields ALREADY EXIST in the
low bits — `tail_q` = alloc, `emit_q` = retire (increments only, so §6.3's
oldest-first invariant holds by construction and is already asserted at line
1536), `live_cnt_q` = used with §6.1's exact update. So T2 is an **extension of
live machinery**, not a parallel representation to swap in: extend two 6-bit
pointers to 14 bits and delete the 512-flop table that stores what those 16 bits
imply. That is §6.8's 576 bits, reached from the other side.

Step 1 adds the shadow window plus three assertions under `` `ifndef SYNTHESIS ``
— deliberately, so the scaffolding cannot contaminate the before/after fit that
decides T2. The sharp one checks the identity where it bites: at admission the
table says `gen_q[tail_q] + 1` and the window says `alloc_gen`; if those ever
disagree the replacement is unsound, and it fires that cycle.

**I wrote one of them wrong and caught it before it became evidence.**
`a_win_used_matches_span` first TRUNCATED the 14-bit span to 7 bits to compare
against the count — so a span of 128 against a count of 0 would have compared
EQUAL and the assertion would have passed on exactly the corruption it exists to
find. Widened the count instead of narrowing the span. Truncation always fails
in the reassuring direction, which is the house law.

## Checked that nothing from the owner is still undelivered

CLAUDE.md: *"Instructions are not delivered until they are read."* The handoff
commit carried an audit ZIP whose README says the consolidated TXT is **"not a
literal concatenation of all original code appendices"** — i.e. content exists
that the TXT drops.

Ran the ZIP's own `recover_original_reports.py --offline` (local git objects
only, no network, output outside the repository, read-only by construction).
Three byte-verified originals came back:

* **S01** texture rearchitecture — **already in `reports/`**, and verified
  byte-identical: the 4,344-byte size difference is exactly 4,344 CR characters.
* **S02** control fabric — already in `reports/`, and the source of today's
  §5/§6 work.
* **S03** terrain 31 MHz — the brief the audit calls *"explicitly deferred"*,
  consistent with texture-first.

**So nothing is undelivered.** Verified rather than assumed, and the check paid
for itself: re-reading S01 §15.1 showed it had **predicted the ten physical
M10Ks** (nine instances, `u_ctx` taking two slices) and had **prescribed the
refusal** this morning's report made — *"nor may it be assigned to a particular
queue without the per-instance RAM report."*

One §15.1 instruction is still open: the gate is an aggregate
`min_memory_bits`, and §15.1 says that is *"useful but not sufficient... require
per-bank names, logical geometry, physical mode"*. The data now exists in the
harvested `blockfit.fit.rpt`. Recorded, not built — the owner put tool expansion
below finishing the island.

---

## COMBINE V2 LANDED, and the v3own REFIT is launched

**`zhao_texture_material_combine_v2`, first fit ever**: 870 ALM, 918 reg, 6 M10K,
2 DSP, **114.04 MHz — above the product clock**. Status ok.
`reports/V31-COMBINE-V2-FIRST-FIT-20260907.md`.

Against the V1 that `zhao_prod_top` still instantiates: **41% fewer ALMs and 3.1×
the clock** (V1 is 36.28 MHz). So the production resource top — which answers
*"what does the planned console cost when counted ONCE?"* — is currently costing
a block 70% larger and a third the speed of the one the island contains. That is
now a decision with numbers, not a naming inconsistency.

**§12.3's COMBINE allowance is confirmed.** The 69-block profile budgets
*"COMBINE local payload/scratch/tag allowance — 6"* and flags it *"NOT a
measurement"*. Measured: **exactly 6**.

**Blocker 1 of 8 cleared.**

## POSITION BEFORE THE NEXT FIT LANDS (written first, again)

Launched: `zhao_texture_v3own` refit carrying **T4** (`383e45df`) and the **fence
rewrite** (`df1deda7`). The shadow-window assertions are all `` `ifndef
SYNTHESIS ``, so they cannot contaminate it.

**Before-picture — do not re-derive:** reported **77.16**, core→core **77.16**,
ALM **6,094**, FIT registers **4,756**, 17 M10K / 20,640 bits, 952 virtual pins.
Worst path `fence_open_q~0 -> fence_open_q~0`, −2.960, **12.804 ns**, of which
the four `Mux9` levels (the 64-way `gen_n_c[tail_next_c]` select) are **9.136 ns
— 71%**.

**Prediction on record:** the fence self-loop should fall out of the worst family
— the adder and the 64-way select are no longer inside the permission loop, only
a 2:1 mux is. **Falsifier:** if `fence_open_q -> fence_open_q` is still the worst
path with a similar `Mux9` chain, the rewrite did not move the select and the
equivalence argument, though sound, addressed the wrong structure.

**Not predicted:** a specific MHz. Three source-reading predictions were
falsified today; this one names a structure, not a number.

**Also expect:** ALM to move — T4 adds `crs_q` (+64 flops, stated) and the fence
rewrite adds a second 64-way select in parallel rather than one in series, which
may cost area to buy depth.

**Next regardless of result:** the remaining seven §12.4 blockers, all stale rows
needing refits, all texture-island blocks.

## §16's DONE-queue seam repaired, while the refit holds the lane

`zhao_raster_ticketq_rh.sv` (`9b6ef66c`). §16.2's claim checked in the source
(line 18 promises a registered head, line 64 is `dout_o = mem_q[head_q]`) **and
in the fit** — every worst path of `rcp24_v3@v3-full` runs
`u_doneq|mem_q[..] -> r_tok_o`, 90.54 reported against 129.18 core→core.

**I deferred this once and the deferral was wrong on its own terms.** The reason
I wrote down was "the fit lane is busy", but only the *timing* confirmation needs
Quartus; the functional work does not. Corrected and finished.

A **wrapper**, so §16.4's "do not replace every ticket queue blindly" is
satisfied structurally — the other three instances are byte-identical, not
argued to be unaffected. Two heads, because one would cost the throughput gate;
the pattern is v3rq's, whose one-pop-per-clock was measured this morning.

**Before/after, both real runs:**

| lane | before | after |
|---|---:|---:|
| saturated | 16,582 (4.04/recip) | 16,608 (**4.05**) |
| 1-in-7 | 29,550 (7.20) | 29,655 (7.23) |

52 checks pass both sides; the gate is 4.0–4.6, so §16.1's four-clock rate is
retained at a cost of **0.16%**.

**Scope stated plainly:** `zhao_raster_rcp24_v3` is `not-yet-adopted` and nothing
instantiates it — `rcp24_svc` is what the island composes. So this repairs the
**successor**, as §16 intends, and does not move the island's current numbers.
Claiming otherwise would be the comfortable reading.
