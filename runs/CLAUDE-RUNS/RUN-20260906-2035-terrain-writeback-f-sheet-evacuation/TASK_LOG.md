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

## THE ISLAND'S REAL LIMITER, FOUND AND REPAIRED

`reports/V31-ISLAND-REAL-LIMITER-20260907.md`, then `4ff7d48c`.

**The block that looks worst was not the problem.** `zhao_texture_aux_pipe`
reports **63.63 MHz**, the lowest of any island block, against a core→core of
120.37. Its worst paths all start at the input port `req_wx_i[3]` and run
combinationally into the divider — and **it does not appear in the composed
island's worst paths even once.** Chasing it would have been wasted work; the
alarming number was the artefact.

**The composed fit names the real one:**

    -2.936  zhao_raster_perspuv_svc:u_persp|head_q[1]
            -> zhao_texture_fragrob:u_fragrob|altsyncram:axg_m_rtl_0

(The five worse paths at −4.800 all start at `pal_ld_gen_i`, an island-top input
port, so they set the reported 67.57 but have a boundary to blame. −2.936 is the
honest limiter and agrees with the endpoint split's 77.30.)

**And it is §16.2's defect again, this time in a block the island composes.**
Seven arrays indexed by `head_q` at NTOK = 16, two of them 32 bits — a 16-way
select between a queue pointer and the next block's M10K. The file's own header
says *"P0 pop a queue, register the operands"*; the internal pipeline does, the
external outputs did not.

**Repaired with a skid** so the handshake absorbs the cycle and the island top is
untouched. Both internal retirement sites moved to the internal ready together —
this file records that separating them once produced a free-count that grew
without bound and hung the lane.

**Verified at both levels with real before runs:**

| | before | after |
|---|---|---|
| block | 666 products / 335 clocks, 1.99/clk | **identical** |
| island composed | *(running)* | **119 checks pass** |

Timing benefit unmeasured until a refit — the lane is busy with the v3own
T4+fence refit.

## `ticketq_rh` got its own test, and the RTL caught the test

18 checks (`3ad546ff`). rcp24's bench covers the CONSUMER; it never saturates the
DONE queue, so §5.3's D-versus-D+2 question is never reached by it, and no
correctness check sees a RATE. Measured directly: accepts **exactly 16, not 18**,
and **zero bubbles in 300 cycles**.

Fire-tested by disabling the spare slot — 200 bubbles of 300, drain collapses
from 16 to 1. The spare is load-bearing.

My first version drove `pop_i` high through warmup and the wrapper latched
`err_o`, correctly: `pop_i && empty_o` is a consumer protocol violation. The test
was wrong and the detector was right.

## An unrelated repair found while checking my own work did not break it

`zhao_prod_top` is **`failed:quartus_map.exe`** — the top that answers *"what does
the planned console cost when counted ONCE?"* does not map, so that question has
no answer today. `reports/PROD-TOP-STRUCT-PORTS-20260907.md`.

Cause confirmed in `gen_prod_top.py:parse_ports`: it takes the **first
identifier** as the port name, so a struct-typed port
(`output var zhao_guard_rsp_t guard_rsp_o`) yields a wire named after the TYPE at
width one — `logic [1-1:0] u23_zhao_guard_rsp_t` — and two ports of the same
struct type in one instance collide.

**There is no lint target for the generated top.** Every other significant module
has one. Its only check is a multi-hour Quartus run, so a defect Verilator
reports in two seconds sat behind the most expensive gate available.

**Recorded, not chased** — the owner's direction names "an interesting new
bottleneck elsewhere" precisely, and this is one. None of the 16 errors mention
any block changed today; that was the question that started the check.

## Mid-flight reading of the refit's map stage (read-only, before the fitter finishes)

`quartus_map` completed at 13:02; the fitter is still placing. Three things
checked now rather than assumed later:

**1. `` `ifndef SYNTHESIS `` genuinely works.** T2 step 1's shadow window and
identity assertions were added on the explicit claim that they *"must not move
the next fit's numbers"* — a claim about a tool's behaviour, so it was tested.
The map report has **zero** occurrences of `sh_alloc_gen_q`, `sh_retire_gen_q`,
`tkt_chk_q`, `gen_chk_s`; and **64** of `crs_q`. The guard holds; the
before/after that decides T2 is clean.

**2. T4's declared cost is exactly right.**

| | MAP registers | virtual pins | memory bits | DSP |
|---|---:|---:|---:|---:|
| previous `19bd8bd2` | 4,246 | 952 | 20,640 | 0 |
| this refit | **4,310** | 952 | 20,640 | 0 |

**+64 exactly** — `crs_q` and nothing else, which is what T4's report predicted
in advance. It also says the fence rewrite added **no** registers, as a change
that only moves a select out of a loop should. Scope matches (952 pins both), so
T7's matched-scope requirement is satisfied before the numbers arrive.

**3. No new Quartus warnings.** `Warning (10036)` count is **5 before and 5
now** — same class, same three files, line numbers shifted only by the code added
above them. They are assertion-only signals (`ovf_c` feeds nothing but
`a_rq_no_overflow`), which Quartus reports as unused once it drops assertions.

Only ALM and Fmax remain, and those come from the fitter.

---

## THE REFIT LANDED — the fence rewrite is measured, and it over-recovered

| | reported | **core→core** | ALM | reg |
|---|---:|---:|---:|---:|
| `@v3-full` (before today) | 75.79 | **89.09** | 5,678 | 4,864 |
| `19bd8bd2` fence + credit | 77.16 | **77.16** | 6,094 | 4,756 |
| `22442fd0` **+ rewrite + T4** | **87.37** | **91.32** | **5,709** | 4,863 |

**Falsifier fired the right way.** `fence_open_q -> fence_open_q` is **not in the
worst paths at all**. The 64-way `gen_n_c[tail_next_c]` select is out of the
permission loop.

**Internal ceiling ended HIGHER than it started** — 89.09 → 77.16 → **91.32**.
And ALM went **down 385** while carrying T4's `crs_q`. Against the original the
whole day is **+31 ALM (+0.5%)** for a correctness repair, a wrap fence, a
registered admission credit and **+15.3% reported clock**.

**The new honest limiter is `gen_q -> iss_q`** — the per-slot generation table,
named for the THIRD time today and exactly T2's target. §6.8 predicted the
mechanism; the fit has now decided the timing half three times over.

`ALM 5709 > 1800` stands: retained and failing, gate untouched, per §12.4.

## POSITION BEFORE THE NEXT FIT (written first, third time today)

Launched `zhao_raster_perspuv_svc` — #1 in the refit order, because it carries
today's registered output boundary aimed at the island's worst core→core path.

**Before:** reported **96.62**, ALM **1,910**, registers **3,157**, M10K **1**,
DSP 6, Fmax 96.62, `failed:structure` on `registers 3157 > 700` and
`ALM 1910 > 900`.

**Two predictions with falsifiers:**
1. The `head_q` 16-way select leaves the retirement path. Falsifier: if the worst
   path still launches from `head_q` through seven arrays, the boundary did not
   move it.
2. `e_q_u`/`e_q_v` (16x32 each, ~1,024 flops) may now infer as memory, since
   their reads terminate at a flop per QUARTUS_GOTCHAS 14. Falsifier: registers
   roughly unchanged and M10K still 1 means they did not, and something else in
   the read path blocks absorption.

**Next while it runs:** T2 step 2 — move Group A (admission + wrap) off the
generation table. Now strongly motivated: the measurement says `gen_q` is the
limiter, and step 1's assertions already prove the identity holds every cycle.
v3own is outside this fit's closure.

## T2 STEP 2 — the ISSUE lanes leave the table (`2b377444`)

The refit named `gen_q[4][4] -> iss_q[52][0]` as the honest limiter, and that
path IS lines 459/470's `gen_q[iss_t_slot_c] == iss_t_gen_c`. Both lanes now use
§6.1's interval — a 14-bit subtract and compare on registers — instead of two
64-way array selects.

**This is step 1 being cashed in.** `a_win_live_matches_table` and its
boundary-aimed twin have asserted exactly this equivalence on real traffic every
cycle since step 1. The equivalence was proved *before* it was relied on, which
is T2's stated order and the reason step 1 existed at all.

§6.2's trap avoided explicitly: internal ticket is `{gen, slot}`, the public
token is `{slot, gen}`. Tickets are built by hand rather than by reusing an owner
word.

477 checks pass, no assertion fired. Seven readers of `gen_q` remain and the
assertions still cross-check it, so this stays reversible one site at a time.

## Case 4b — the coverage gap T2 step 2 opened, found and closed

Moving the ISSUE lanes to `win_live` created a guard whose **reject** path
nothing exercised: the bench drives those lanes only with live owners, so the
accept path was covered hard (mis-ordering the ticket fails 43 checks) and the
reject path was never reached. Case 4 covered exactly this on the *return* lane
and had no issue-lane counterpart.

**The fire test makes the case for itself.** With `win_live` forced to `1'b1` —
a guard that fails OPEN, which is how this kind of guard actually fails —
**exactly 2 of 481 fail, and they are the two new checks.** All 477 pre-existing
checks pass with owner validation on the issue path effectively disabled.

Without case 4b, step 2 could have shipped a permanently-true guard behind a
green bench.

481 checks pass on the real RTL.

## PERSPUV MEASURED — the island's worst path clears the product clock

| | before | after |
|---|---:|---:|
| **Fmax** | 96.62 | **105.19** |
| ALM | 1,910 | **1,886** |
| registers | 3,157 | 3,216 |
| M10K | 1 | 1 |

**Prediction 1 confirmed and not by inference:** `head_q` launches **zero** of
the worst forty paths, and every remaining slack is **positive** — the block no
longer misses timing anywhere. ALM went DOWN 24: the 16-way select across seven
arrays cost more than the registers replacing it.

**Prediction 2 FALSIFIED, as its falsifier said it would be if wrong:**
`e_q_u`/`e_q_v` did NOT infer as memory. Registers +59, M10K still 1. Registering
the read was necessary but not sufficient — the file's own header records the
same outcome for `e_num_u`/`e_num_v`, and `e_tag` remains the only array here
that becomes RAM.

**Four predictions falsified today, three confirmed.** Every falsified one was
about MAGNITUDE or MECHANISM inferred from source; every confirmed one was about
STRUCTURE — which path leaves, which signal disappears. Predict what moves, not
how far.

## POSITION BEFORE THE ISLAND REFIT (fourth time today)

Launched `zhao_texture_island_top` — #2 in the order, and the only thing that
says whether a block-level win composes.

**Before (4 commits stale, which is itself why it is on the list):** reported
**67.57**, core→core **77.30**, ALM 16,192, registers 28,490, 32 M10K, 17 DSP.
Worst paths: five at −4.800 from `pal_ld_gen_i` (an island input port, so
boundary-blamed), then **−2.936 `perspuv_svc|head_q -> fragrob|axg_m`** — the
one just repaired.

**Prediction:** the −2.936 family is gone. **Falsifier:** if a `head_q`-launched
path into fragrob is still there, the block-level win did not compose.

**Not predicted:** the reported figure. The palette-load paths at −4.800 are
untouched and will likely still set it.

## T2: EIGHT OF TWELVE SITES MOVED, one decision left

| group | sites | status |
|---|---|---|
| A — admission + wrap | 3 | **moved** |
| B — ISSUE lanes (the measured limiter) | 2 | **moved** |
| B — READY-ticket eligibility | 2 | **moved** |
| B — in-loop comparisons | 4 | **not moved** — semantic |
| C — `g0_owner_q` lookup | 1 | **moved** |

Every move: 481 bench checks, no assertion fired, and each derivation asserted
against the structure it replaced. Three of those assertions have been
fire-tested and caught the exact errors they exist for — the tautological form,
the naive site-3 form, and the ticket built in public order.

**The one open question is a decision, not effort.** The four in-loop guards have
no `live_q` term; adding one via `win_live` would reject a stale event arriving
after release but before reallocation, which is accepted today. The site calls
itself *"a fault-injection and drain-boundary guard"*, so tightening may be
right — but §11.1's lesson this morning was exactly that conflating two events
in one bit is how a final gets authorised by a reservation. **`gen_q`'s 512
flip-flops cannot be recovered until this is settled.**

An exactly-equivalent fallback exists: hoist `gen_q[c4t_slot_q] == c4t_gen_q`
out of the loop — one 64-way select instead of 64 comparisons — which buys the
logic without touching semantics.

**Nothing from T2 is measured yet.** Next v3own refit compares against
**87.37 reported / 91.32 core→core, 5,709 ALM, 4,863 reg** on matched scope.
Prediction: `gen_q -> iss_q` gone from the worst families. NOT predicted: ALM or
register movement — the table is still there, and today's score on predictions is
three confirmed (all structural) against four falsified (all about magnitude).

## The last safe T2 logic win: the in-loop comparisons hoisted

Exactly equivalent — inside the loop the guard already establishes
`c4t_slot_q == i`, so `gen_q[i]` IS `gen_q[c4t_slot_q]`. **256 comparators
become 4 selects** across the four lanes.

Deliberately NOT `win_live`, for the semantic reason already recorded. This buys
the logic, changes no behaviour, and leaves the decision clearly stated.

481 checks pass, no assertion fired.

**T2 now stands at: every site that can be moved without a semantics decision
has been moved.** `gen_q` survives as four hoisted selects plus the assertion
cross-checks; its 512 flip-flops are recoverable only once the drain-guard
question is answered.

## Honest position with the island fit running

* **Running:** `zhao_texture_island_top` — the only thing that says whether
  perspuv's block-level win (96.62 → 105.19, clearing the product clock)
  composes. Prediction and falsifier recorded above.
* **Needs a decision, not work:** the four in-loop drain guards.
* **Needs the toolchain:** the six remaining §12.4 refits, and a v3own refit to
  measure today's eight T2 moves.
* **Deliberately deferred:** the palette load seam — the island's *reported*
  limiter. The running fit will say whether those paths still dominate, and
  guessing before that is exactly what today falsified four times.

## §16.3's test list closed, and §16.5's rate recheck started

**§16.3's verification obligation is met.** Its five named tests, checked item by
item against the actual benches rather than assumed:

* *ready dropping just after a read launches* — **written today**, adversarial
  supply/demand over 4,000 cycles, exactly-once and in-order, fire-tested
  against an unreserved read launch (drain collapses 16 → 2);
* *shuffled completions* — covered: results are keyed **by token**, so a token
  paired with the wrong result mismatches;
* *zero/nonzero interleaving* — covered: zeros are a scheduled phase, every 97th;
* *slot reuse* — covered: 4,104 requests through 16 contexts, ~256 reuses each;
* *distinct U/V* — perspuv's bench, a different block.

**I had written that two of these were "still not covered". That was wrong** —
they were, and leaving it would have sent the next pass to write tests that
already exist. Checked and corrected.

**§16.5 is triggered by my own change and is now running.** It says: *"If a
repair changes the arithmetic feedback loop, admission queue latency or CONTEXT
REUSE LATENCY, rerun eight/sixteen/thirty-two-context rate comparisons. The old
knee is evidence about the old topology, not a universal number."*

The registered head returns a context to the free queue one cycle later, so
context reuse latency changed and the 16-context knee is no longer evidence for
this topology. Building NCTX = 8 / 16 / 32.

§16.5 also endorses the change itself: *"An output-only register cut can improve
the external interface without changing multiplier initiation interval. That is
the preferred first experiment."* That is exactly what was done — and §16.6's
caution is noted too: 90.54 was never 100 MHz closure, and no number is claimed
for rcp24 until it refits.

## §16.5's recheck: the knee is still 16, re-measured for the NEW topology

| NCTX | clocks / 4,104 | **per reciprocal** | suite |
|---:|---:|---:|---|
| 8 | 23,852 | **5.81** | rate gate fails, 51/52 |
| **16** | 16,608 | **4.05** | 52 pass |
| 32 | 16,553 | **4.03** | 52 pass |

Doubling to 32 buys **0.5%** while doubling every `[NCTX]` array; halving to 8
costs **43%**. So sixteen stays, and the registered head did not move the knee.

This was **mandatory, not optional** — §16.5 requires the rerun whenever a repair
changes context reuse latency, and returning a context one cycle later is exactly
that. The old knee stopped being evidence the moment the wrapper landed.

§16.5 also endorses the change that triggered it: an output-only register cut is
*"the preferred first experiment"*, the initiation interval is unchanged at 4.05,
and no stage went inside the recurrence.

§16.6's caution restated rather than quietly dropped: **no clock is claimed for
rcp24.** It has not refitted since the change; it is #4 in the order, and the
comparison is against 90.54 reported / 129.18 core→core on matched scope.

## §13.1 / T6's first item: the per-owner fetched bit is gone from the logic

§13.1 instructs it directly — *"Remove the per-owner ftc/fetched array. It
represented a property already encoded by a monotone ordered cursor."*

`!ftc_q[fetch_q]` was the array's only reader. `fetch_q` is monotone, so it
revisits a slot only after 64 fetches, by which time that slot must have been
re-admitted — and admission clears ftc. `unf_cnt_q` gates the whole condition, so
the term could never be the reason a fetch was blocked.

**Asserted BEFORE removal, not argued.** `a_ftc_bit_is_redundant` checks that
whenever every other `fetch_fire_c` condition holds, `ftc_q[fetch_q]` is already
clear. It passed across the whole 481-check bench — wrap and drain included —
*before* the term came out. That ordering is the whole point: the argument sounds
airtight, and today's record on airtight-sounding arguments is four falsified
predictions against three confirmed.

**The array stays in the source and stays maintained.** Nothing synthesised reads
it, so Quartus removes its 64 flip-flops as dead logic while simulation keeps it
proving the property. The check that licensed the removal survives the removal —
if a future change makes the bit load-bearing again, the assertion fires rather
than the array quietly mattering.

481 checks pass. This is T6's first named item: *"Remove redundant fetched bitmap
under the F reservation proof."*

## §13.2's partition asserted, and the register accounting for the NEXT v3own fit

**§13.2** warns *"avoid two counters whose overlap is inferred only from
naming."* This block has exactly the two it describes — `unf_cnt_q` = |[F, A)|
and `out_res_q` = |[E, F)| — and `live_cnt_q`, maintained independently by
admission and emission, must equal their sum if the intervals genuinely
partition the live set. **Now asserted every cycle**, and it holds across the
full bench. A double-counted owner is a credit that never returns, which presents
as a hang thousands of cycles later — exactly the kind of accounting that fails
silently.

### What is synthesised and what is not, checked rather than assumed

* `sh_alloc_gen_q` / `sh_retire_gen_q` — **synthesised** (16 flops). Correct:
  T2 step 2 made them load-bearing for the ISSUE and READY lanes.
* `gen_chk_s`, `tkt_chk_q` — **sim-only**, inside `` `ifndef SYNTHESIS ``. They
  drive nothing but assertions.
* The 27 assertions outside the guard are the pre-existing pattern; Quartus drops
  immediate assertions, which is why they have never cost anything.

### Register prediction for the next v3own refit, with its falsifier

Against this fit's **4,863 FIT registers**:

| change | expected |
|---|---:|
| window generation counters now synthesised | **+16** |
| `ftc_q` dead after §13.1 (nothing synthesised reads it) | **−64** |
| `gen_q` — still alive, four hoisted readers | 0 |
| **net** | **≈ −48** |

**Falsifier:** if registers do *not* fall by roughly 48, either `ftc_q` was not
eliminated — meaning something still reads it and §13.1's removal was
incomplete — or the window counters cost more than the two 8-bit registers they
appear to be. Either would be worth knowing immediately rather than being
absorbed into a larger delta.

No ALM or Fmax prediction. Today's score is three confirmed predictions (all
structural) against four falsified (all about magnitude), and this one is
deliberately an accounting claim rather than a performance one.

## §22.2's phase-control invariants — none of them existed

§22.2 says *"assert at every edge for every active row"* and lists them. Checked
before writing anything: **none of the first four were asserted anywhere in this
file.** The same audit that found §16.3's one genuine gap, applied to §22.

Added, gated on `live_q` per the section's "ACTIVE row":

* committed ⊆ claimed ⊆ issued ⊆ required (three assertions);
* **combine_issued implies combine_reserved** — exactly T4's separation: `cbi` is
  §11.1's event 3, `crs` is event 2, and an issue never reserved would mean the
  credit was bypassed;
* **final_claimed implies actual combine_issued** — M6's property, which until
  today rested on a single directed case and is now a continuous row invariant;
* final_done implies final_claimed.

One rotating row per cycle via `gen_chk_s`, so the ring is swept many times over
at a fraction of the cost of 64 comparisons per edge.

**Fire-tested:** committing a source that was never claimed trips
`a_p22_cmt_sub_clm` immediately, alongside two functional failures.

481 checks pass on the real RTL, none fired.

### Where the brief's checklists now stand

* **§13.1** — fetched bit removed, under an assertion proved first. T6's first item.
* **§13.2** — three-interval partition asserted.
* **§14.1** — admission cone verified structurally: two registers, quiet one
  register away. `reports/V31-S14-ADMISSION-CONE-20260907.md`.
* **§16.2/.3/.5** — seam repaired, handoff law verified, test list complete, the
  context-rate knee re-measured at 16.
* **§22.2** — invariants now exist and are demonstrated to fire.
* **§14.2** — NOT done: its nine drain conditions span blocks outside this one,
  several inside the running island fit's closure. Belongs with §10 integration.

## §22's verification matrix audited section by section

The method that found §16.3's gap, applied to §22. Each subsection's named cases
checked against the actual benches rather than assumed covered.

* **§22.1** identity model — zero/64/wrap were covered; **one, 63, every head
  position, and the LATER-generation handle were not.** Added: membership as
  exactly the interval at used = 0/1/63/64 across 66 head positions on a coprime
  stride (so intervals crossing numeric zero are structural, not lucky), and
  same-slot handles one generation back **and forward** from every live owner.
  Fire-tested with `<=` for `<`: 4 of 33 fail.
* **§22.2** phase-control invariants — **none of the first four existed.** Added
  all six, including "combine_issued implies combine_reserved" (T4's separation)
  and "final_claimed implies actual combine_issued" (M6, previously resting on a
  single directed case). Fire-tested: committing an unclaimed source trips
  `a_p22_cmt_sub_clm`.
* **§22.3** completion timing — duplicates and AUX-same-edge/AUX-last were
  covered; **reverse sample order and AUX-before-all-samples were not.** Added
  as one case, checking rows land by sample INDEX rather than arrival order.
* **§22.4** issue races — return-before-issue, malformed index 3, independent
  TMU/AUX and high-bit generations were covered; **the repeated issue
  notification was not.** Added, checking both halves: no second outstanding
  request, and the already-claimed result not erased.

**489 checks pass.** §22.5–22.9 concern T5's candidate registers and later
integration stages that do not exist yet.

---

## THE BIGGEST FINDING OF THE DAY IS SOMEBODY ELSE'S WIN, MEASURED FOR THE FIRST TIME

Read mid-flight from the island refit's map stage while the fitter placed.

**Six of §10 L0's eight arrays now infer as RAM** — `fctx_m`, `flod_m`, `fpgn_m`,
`fcls_m`, `fpsl_m`, `faux_m`. Only `uvw_m` and `class_m` remain in fabric.

At map: **−4,923 registers (−17.6%), +5,312 block memory bits**, and the entity
walk puts **the entire register drop inside `zhao_texture_island_top` itself**,
where those arrays live. Child deltas are trivial by comparison —
`perspuv_svc` +72 (mine), `fragrob` +2, `tmu_plan` +1.

**It is not today's work.** Six island sources changed across ten commits since
the Sep 6 fit; four are from earlier sessions (`b55959f0`, `d80f29b4`,
`3a06a590`, `a1846867`). My perspuv change is positively excluded from the memory
rise: its own fit measured registers **up** 59 with M10K unchanged at 1.

**And it supersedes my own L0 report from this morning.** L0 read the CURRENT
source and cited the PREVIOUS fit's uninferred list — CLAUDE.md's *"never compare
a current file to an old measurement"* wearing a new costume, because the
comparison was implicit rather than written as one. The tell was available and
unused: `compare_rows.py` already flagged that island row as four commits stale,
in a report I wrote the same morning.

L0 is now bannered as superseded rather than left to be read standalone. Its
access-pattern walk — single-writer, single-reader, one shared write event — is
still correct, and is precisely what made the conversion possible.

**§10's storage lane is further along than the L-series assumes: not eight
arrays, but two.** And `uvw_m` is literally §10.2's subject, with its credited
N0–N4 join already specified stage by stage.

**Still unknown:** ALM, Fmax, and whether the six conversions cost M10K blocks
disproportionately — §12's lesson today was that shallow arrays each burn a whole
block, and `fpsl_m`/`fcls_m` are narrow. The completed fit's RAM summary decides
that, and it should be read before anyone calls this a clean win.

## Roadmap nudge, worked in order

1. **Fit running** (island) — worked outside its closure throughout.
2. **G1-D §4.3** — already filled through 4.3c. Added **§4.3d** with the refit's
   map-stage attribution, following §4.3b's own precedent that Analysis &
   Synthesis answers attribution on its own. ALM/Fmax explicitly left pending.
3. **Toolchain not idle** — the island refit is #2 in the order.

**One roadmap item deliberately not done: D22 step 4 / GEOM.PROJECT.** Owner
direction `49fc32e9` names *projection* as "not the current implementation
priority", and the nudge's list predates it. COMBINE.V1's DSP measurement is
already answered in the docket. perspuv's per-axis array split is inside the
running fit's closure and is correctly gated.

**§22.5's hostile schedule added instead** — all 64 owners ready with COMBINE
stalled, released, every owner exactly once and in order. Case 13 stalls the
OUTPUT; stalling COMBINE is the different point that makes ready rows accumulate
and stresses the queues whose capacity contract was fixed today. **495 checks.**

## §22 audit continued: §22.5, §22.6, §22.7

* **§22.5** — all 64 ready with **COMBINE** stalled (case 13 stalls the OUTPUT,
  a different point), released, every owner exactly once and in order. Also
  exercises §5.3's capacity contract in the composed owner rather than in
  isolation.
* **§22.6** — "consumer ready toggling on every edge" and "pointer wrap while
  head/spare still contain older packets" had no counterpart. Added as one
  4,000-cycle identity-and-order run. Fire-tested, with an honest note on which
  detector caught it: breaking §5.4's read reservation trips the module's own
  `a_rq_no_overflow` first, so the new checks are a second net behind a faster
  one.
* **§22.7** — "unchanged 64-bit context" was checked only in case 1, which
  completes IN ORDER, so the check sat where it could not fail. Added to case
  16, the out-of-order case where a context can actually be mispaired.

**496 checks** in the owner bench, **28** in the queue bench.

The recurring shape across all six gaps found today: **the check often existed,
but in the case that could not exercise it.** That is the same failure as an
untested detector, wearing better clothes.

---

## THE OWNER ANSWERED THE BLOCKING DECISION, and corrected me twice

`8969dcaf` — `ZHAOZHOU_T2_ARCHITECTURE_DECISION_BRIEF_2026-09-07.txt`, 1,652
lines. Acknowledged on the hardware branch in
`fpga/rtl/texture/OWNER-DIRECTION-T2-LIFETIME-2026-09-07.md`.

**The ruling:** an owner has authority from admission until its ordered external
output transfer; after that, later events are stale, and *"matching a slot's
residual generation bits does not extend the owner's authority after
retirement."* Applies separately to TMU/AUX/FINAL at C4 and to the COMBINE
handshake, which is a different lifecycle event.

**Correction 1, and it was my framing:** *"The earlier claim 'those four guards
must change semantics before the table can be deleted' was too strong."* Right —
`win_gen_of_slot()` reconstructs DEAD slots' generations too, so the exact
predicate can be kept *and* the table deleted. I had welded the policy question
to the physical representation.

**Correction 2 — my inventory was short by three, because of a SPACE.** The brief
lists seven functional readers; I found four. The missed three are the C1
snapshots, written `gen_q [c0t_slot_q]` — my pattern was `gen_q\[`. The brief
anticipates it and supplies `\bgen_(q|n_c)\s*\[`. A pattern that matches nothing
reports no problem, and that inventory was used to argue what could be removed.

## T2's exact migration is DONE — `gen_q`'s 512 flip-flops are gone

All seven readers now use `win_gen_of_slot()`, predicate unchanged. C1 captures
from the same pre-edge allocator state. `fn_gen_q` classified (only consumer was
an assertion) and moved to verification. `gen_q`/`gen_n_c` deleted from the
synthesised design; a literal `vgen_q` survives only under `` `ifndef SYNTHESIS ``
maintained by its **own** old-style recurrence, so the equivalence assertion is
not circular. **496 checks pass.**

The live-owner authority checks the ruling requires are step 4 and deliberately
NOT in that commit.

## ISLAND FIT LANDED — G1-D §4.3e

**ALM 16,192 → 13,601 (−16%), registers 28,490 → 23,181 (−18.6%), M10K 32 → 36.**
Reported 66.77 / core→core 75.51. **Prediction held: zero of the worst forty
paths launch from `head_q`** — perspuv is off the composed critical path. New
core→core limiter is internal to `zhao_raster_rcp24_svc`.

Against the roadmap's benchmarks: **2.06× nominal, 1.81× redline, 1.72×
standalone sum**, down from 2.45/2.16/2.05. Gates untouched.

## POSITION BEFORE THE OWNER REFIT (fifth time today)

Launching `zhao_texture_v3own` — the brief's own next action, *"prepare a matched
owner fit"*.

**Before (`22442fd0`):** reported **87.37**, core→core **91.32**, ALM **5,709**,
FIT registers **4,863**, 17 M10K / 20,640 bits, 952 pins.

**Register prediction, revised for the migration:** −512 (`gen_q` deleted) −64
(`ftc_q` dead per §13.1) +16 (window counters) ≈ **−560**, so roughly **4,300**.
**Falsifier:** if registers do not fall by ~500, the table did not actually leave
synthesis and something still reads it.

**ALM: not predicted.** A 64-way select is replaced by one comparison and a mux
per site, which should also fall — but four magnitude predictions were falsified
today against three structural ones, so only the structural claim is made:
`gen_q` should appear in no path.

## The owner's step 4 (§8.2) is implemented — and honestly qualified

C2 now requires **both** snapshot identity and current full-ticket membership,
on all three return lanes. §8.1's counterexamples are why: a FUTURE token defeats
a current-only check, a RETIRED token defeats a snapshot-only one. The brief is
explicit this was not already satisfied by T2 step 2's `win_live()` on ISSUE.

**V02 done exhaustively** — all 16,384 head residues × 65 occupancies, probed at
both interval boundaries. 4.26M probes. The previous block sampled 66 positions.

**V03/V04 instrumented, and the result is negative.** Neither schedule occurs
anywhere in the 496-check bench. That is the brief's "structurally prevented"
branch, and it means **today's §8.2 change is correct per the ruling but not
demonstrated load-bearing by the current tests.** Saying otherwise would be
exactly the untested-detector failure this repository documents.

Not claimed: that the schedules are unreachable. V04 looks plausible for a
duplicate return arriving as its owner retires. Constructing it — sweeping the
injection offset against the `out_ready` release edge — is the next step.

## V04 IS REACHABLE — and I had reported the opposite an hour earlier

**Corrected.** I recorded the V04 schedule as unreached and leaned toward
"structurally prevented". Wrong, and the reason matters: my first construction
released `out_ready` **before** injecting the duplicate, so the owner had already
retired by capture and `c1t_live_q` was false at snapshot — a different schedule
entirely. Ten offsets "survived" and I believed them.

Injecting **first** and releasing after hits it at **offset zero**.

So a duplicate return captured while its owner is live, whose C2 claim lands
after retirement, is ordinary reachable traffic. A snapshot-only predicate stays
true across that window.

**The full chain, closed:**

1. the owner ruled that authority ends at the ordered output transfer;
2. §8.2 implemented — C2 requires snapshot identity **and** current membership;
3. the schedule **constructed** and shown reachable;
4. case 4e verifies the ruling's own words — refused, commits nothing, owner not
   resurrected, still emits once with its own context;
5. **mutation: remove the §8.2 term and case 4e fails.** The term is
   load-bearing, not belt and braces.

The V04 *assertion* is removed — the schedule is legitimate traffic, so keeping
it would abort on valid behaviour. V03's detector stays; that schedule has not
been constructed and has not fired.

**500 checks pass.**

### Still open from the brief's matrix

* **V05** wants stale-after-retirement tested for **TMU, AUX and FINAL
  separately**, observing rejection classification *and* actual bank write
  enables. Case 4e covers TMU only, and observes classification but not write
  enables.
* **V06** same slot reused with a new generation, old token delivered before/on/
  after the capture and claim edges.
* **§8.3's claim-to-write lease** at the physical write enables — the brief warns
  it must not become "an uncontrolled late combinational window predicate
  immediately before a bank write-enable".

## The owner's V-matrix, worked through

| case | status |
|---|---|
| **V01** exact reconstruction | `a_win_gen_of_slot` against an independently maintained `vgen_q`, every cycle; the `tail==63` mutation fire-tested |
| **V02** live interval membership | **exhaustive** — all 16,384 head residues × 65 occupancies at both boundaries, 4.26M probes; `<=` and public-order mutations fire-tested |
| **V03** C1 snapshot validity | detector in place, **has not fired** — schedule not constructed, reported as such |
| **V04** current identity at claim | **REACHABLE** (offset 0), case 4e, and §8.2's term proven load-bearing by mutation |
| **V05** stale after retirement | TMU (4e), **AUX and FINAL (4f)** — each lane separately, as the ruling requires. *Partial:* observes classification and payload, not the bank write-enable pins |
| **V06** slot reused, new generation | case 4g — old token delivered before / on / after the capture and claim edges; new owner intact on every axis |

**515 checks**, from 477 this morning.

Still open: V03's construction, V05's write-enable observation, and §8.3's
claim-to-write lease at the physical write enables — which the brief warns must
not become "an uncontrolled late combinational window predicate immediately
before a bank write-enable".

## THE REGISTER PREDICTION IS EXACT: −560, measured at map

Read mid-flight; the fitter is still placing.

| | previous (T4+fence) | this refit |
|---|---:|---:|
| MAP registers | 4,310 | **3,750** |
| virtual pins | 952 | 952 |
| memory bits | 20,640 | 20,640 |
| DSP | 0 | 0 |

**−560, against a recorded prediction of −560:**

| component | predicted |
|---|---:|
| `gen_q` deleted (T2 migration) | −512 |
| `ftc_q` dead (§13.1) | −64 |
| window generation counters now synthesised | +16 |
| **net** | **−560** |

So all three are confirmed independently: the 512-flop table genuinely left
synthesis, §13.1's fetched bit is gone, and the window counters cost exactly the
two bytes they look like.

**And this refines today's rule about predictions.** The score was three
structural predictions confirmed against four magnitude predictions falsified,
and I wrote "predict what moves, not how far." That was half right. The sharper
rule: **accounting predictions can be exact; performance predictions cannot.**
Registers are countable from the source — flip-flops declared, flip-flops
deleted — so a careful count lands on the number. ALM and Fmax depend on what
the fitter chooses to do with the logic, and every one of those I got wrong
today.

ALM and Fmax still pending from the fitter, and deliberately not predicted.

---

## §8.3's lease was already established — by the §8.2 change

Checked rather than assumed. All three bank write enables are registered from
the C2 acceptance:

    c3t_we_q <= c2t_acc_c ? c1t_bit_c[2:0] : 3'b000;
    c3a_we_q <= c2a_acc_c;
    c3f_we_q <= c2f_acc_c;

and `c2t_acc_c` now requires `c2t_idok_c`, which carries current membership. So
the RAM write enable *is* "current instance owns this slot and source",
registered, with the ownership test at C2 rather than in a late window in front
of the RAM — which is precisely the shape §8.3 permits and the shape it warns
against building. It was NOT true this morning: `idok` was snapshot-only, so the
physical enable did not reflect current ownership. §8.3 therefore needs no edit,
and by its own warning should not get one.

## §22.8's late old packet, added to case 19

"Return a late old packet during local drain" is one of §22.8's named states and
is the T2 ruling's own scenario. The wrap fence is the only local drain this
block has, so the case lives inside case 19 rather than beside it — reaching the
fence costs 16,320 admissions, and a second drain purely to keep the cases
visually separate would double the run for no extra evidence.

The attacker takes the SLOT of a still-live draining owner and the GENERATION of
that slot's previous occupant. Payload is a fixed 40-bit constant, checked
against every `mkres()` a legitimate owner in the run can produce — a detector a
collision could spoof is not a detector.

## The stale build graph, second instance, new tell

`cmake --build` failed with `MODMISSING: Cannot find file containing module
'zhao_raster_ticketq_rh'` for `zhao_raster_rcp24_v3.sv:226` — and the file *is*
in that test's `SOURCES` in `tests/CMakeLists.txt` (line 2495). The list was
right; `build/build.ninja` was generated before the line existed, and because
the failing rule is part of build.ninja's own regeneration, ninja could not
rebuild the graph that would have fixed it. CLAUDE.md's fix applied exactly:
regenerate through `cmake --preset windows-native`, never through another
`cmake --build`. Configure clean, 34.4 s.

**The new tell is worth keeping: the source list naming the file while Verilator
still cannot find the module means the GRAPH is stale, not the list.** The
instinct is to go add the file again, which would be a no-op followed by
confusion.

## The 22.8 patch deleted a detector, and the suite went green

Worth writing down in full, because it is this repository's own law happening to
me while I was writing a test *for* that law.

The patch's last anchor was the existing line

    zhao::check(dut->ev_err_stale_o == 0, "wrap run: no stale rejections", ...)

and my replacement text did not contain it. So the insertion **removed** the
assertion. The suite then reported 525 checks passed — and I read that as "the
late packets are refused silently, with no counter recording them", which is a
plausible, interesting, and completely wrong finding. It was wrong in the
comfortable direction: it made the DUT look quiet rather than making my patch
look broken.

What actually caught it was refusing to accept `ev_err_stale=0` as good news.
Two steps:

1. **Fire test.** Give the attacker the CORRECT generation and its return
   becomes legitimate. Result: `ev_err_dup_o` moved 0 → 8. That proved the
   injection path was live and reaching the DUT — 8 packets in, 8 counted.
2. **Read the fourth counter.** §19.7's partition assertion proves exactly one
   of {range, stale, unsol, dup, accept} fires per valid C1 beat, so the eight
   refusals had to be in *some* bucket. I had checked three. Printing all four
   gave `stale=8 unsol=0 dup=0 range=0` — the DUT had been counting them
   correctly the entire time, and the only broken thing was my patch.

The restored line reads `== attacks` instead of `== 0`, which is strictly
stronger than what was there this morning: every late packet was refused AND
counted, and nothing else in a 16,320-admission run was refused for any reason.
The fire test doubles as its fire test — with the correct generation the count
lands in `dup` instead, so `stale == 8` fails.

526 checks, all passing.

**The lesson is about anchors, not about the DUT.** A patch whose anchor is an
existing assertion silently deletes that assertion unless the replacement text
carries it forward, and the resulting suite is greener than before. Anchoring on
the line *above* the target, or asserting the check count went UP by the number
added, would both have caught it immediately — the count went 522 → 525 for four
added checks, and I noticed the arithmetic was off by one and did not chase it.

## WHERE I WAS, written before reading the fit

The `zhao_texture_v3own` fit reached `quartus_sta` — its last stage — so the T2
migration's ALM and Fmax land next. Recorded now, because fit results redirect
the work and the half-finished thing in hand is what gets lost.

**In hand, and unfinished:** §22.8's remaining named states. The late-old-packet
one is done. The next is *"an empty body with a pending queue read, the exact
state the old occupancy omitted"* — that is a `zhao_texture_v3rq` test, not an
owner-block one, so it does not touch the fit's closure either. After that,
*"delay one external adapter's acknowledgement; the namespace must not
reopen"*, which needs an adapter-ack port the owner block does not have yet and
is therefore integration work, not a test I can write today.

**The before-picture to compare against**, recorded this morning: reported
87.37, core→core **91.32**, ALM **5,709**, FIT registers **4,863**, 17 M10K /
20,640 bits, 952 pins. MAP has already confirmed registers 4,310 → **3,750**,
exactly the predicted −560.

**The structural prediction on record:** `gen_q` should appear in NO path.
ALM and Fmax are deliberately NOT predicted — today's score is three structural
predictions confirmed against four magnitude predictions falsified.

## THE T2 FIT LANDED — and the baseline had to be recovered, not recalled

**ALM 5,709 → 3,348 (−41.4%). Core→core 91.32 → 98.18 (+7.5%). Registers
4,863 → 3,953. M10K, bits, DSP and pins all unchanged.**
`reports/V31-T2-OWNER-FIT-20260907.md`.

**The first thing that had to be settled was WHICH baseline**, because the
ledger holds two `v3own` rows and my written note (ALM 5,709 / 87.37 / 91.32)
matched neither. The other row, `@v3-full`, is ALM 5,678 / 75.79 MHz at an older
commit — close enough to my note to be mistaken for it, and wrong in both
directions: comparing against it would have understated the ALM change and
overstated the frequency change.

The baseline came from the previous run's own log instead —
`fit-v3own-t4-fence.log`, same target, same three sources, same script, one
commit earlier, `RULE zhao_texture_v3own: ALM 5709`. That is a like-for-like
predecessor and my note was a faithful record of it.

**The structural prediction held exactly.** Every `*gen_q` in the top-200 path
report is a pipeline register or a shared allocator counter; the bare 64×8
`gen_q` table appears **zero** times, and `ftc_q` zero times — Quartus
dead-code-eliminated it exactly as §13.1 said it would. The new worst internal
path is `req_q[2][1] -> iss_q[18][1]`, the per-owner bitplanes.

**What did not happen:** the ALM gate still fails, 3,348 against `max_alms: 1800`,
status `failed:structure`. Not relaxed, not moved. Core→core 98.18 is still below
the 100 MHz product clock. A 41% cut that still misses the budget by 86% is
progress, not arrival.

## Next fit launched immediately: `zhao_raster_rcp24_v3 -RowLabel v3-rh`

The toolchain was idle for about a minute. This measures the §16.3 registered-head
DONE queue swap, which I made today and no fit has ever seen — and it de-risks
the composed island fit that follows, because rcp24 sits inside the island and an
unmeasured regression there would confound the island number.

**Baseline, `@v3-full` (commit `7d55fa84`): ALM 1,230, registers 1,944, reported
Fmax 90.54, 6 RAM, 3 DSP.** The block's core→core was 129.18, so the reported
number is the one the wrapper is meant to move.

**Structural prediction, the only one made:** no worst path should start at
`u_doneq|mem_q[..]` or `u_doneq|head_q[..]` any more — those were the origin of
every worst path in the baseline, and replacing them is the entire point of the
wrapper. **Falsifier:** if they still originate there, the DONE instance did not
actually get swapped, or the fitter flattened the wrapper back.

## P0-B implemented: the RCP selection/execution boundary is registered

`zhao_raster_rcp24_svc.sv`. The island's worst internal path was
`c_val[5] -> c_m.raddr_a[0]` at −3.243 ns: the round-robin priority scan over
`c_val`/`c_pend` reached the context storage's READ ADDRESS in its own cycle,
and then the operand mux and the 32×64 multiply in that same cycle. Selection
and execution shared one clock.

**S1, a registered issue record**, now sits between them. `{context, phase}` is
captured at selection; the operand read and the multiply address storage from
flops instead of from the arbiter's cone. The arithmetic is byte-for-byte
unchanged — §5.4 is explicit that the exact law, wrap and truncation are not to
be touched to buy a fit, so only the index feeding the operands moved.

**Eligibility is surrendered at SELECTION, not at execution.** §5.3 and the
brief's C01 model both name this as the way this exact change goes wrong:
clearing `c_pend` when the multiply begins would leave a one-cycle window in
which the same job is selected twice.

**Measured, against the recorded baseline:**

| | before | after |
|---|---|---|
| checks | 5 pass | 5 pass |
| multiplier launches / reciprocal | 1628 / 407 = **4.00** | 1628 / 407 = **4.00** |
| clocks per reciprocal | 4.01 (1631) | 4.01 (1632) |

One extra clock across the whole 407-reciprocal run — the added latency, hidden
by the other contexts in flight. §16.1's four-clock rate is retained and the
launch ratio is exactly 4.00, so no job was issued twice.

## The new detectors were shown to FIRE

Four assertions went into the RTL rather than a bench. Fire-tested by building
the C01 failure mode on purpose — deleting `c_pend[pick_i] <= 0` from selection
and clearing `c_pend[s1_i_q]` at execution instead:

    %Error: zhao_raster_rcp24_svc.sv:378: Assertion failed in
      TOP.tb_rcp24_pair.u_svc.a_svc_s1_not_eligible: 'assert' failed.

Immediate. Source restored from the pre-mutation copy and verified clean of the
marker.

**And the fire test caught the stale-binary trap first.** The first run after
the mutation printed numbers IDENTICAL to the baseline — 1628 launches, 4.00
each — which is exactly the documented tell. The exe was timestamped 17:57:09
against a source of 17:58:57: the mutation had never been compiled and I was
reading the old binary. Comparing the two mtimes before believing the result is
what separated "the detector does not fire" from "the detector was never built".

## Owner ask, mid-run: cel/fog ordering

Answered and partly implemented. The gating question — "does the reference reel
already do this?" — is **no, and it documented the opposite in four places**:
`rast.cpp:306` ramps the interpolated lanes, `zref_fragment.hpp:117` called them
"ALREADY FOGGED", `internal.hpp:79` cited "the fogged colour rides the ordinary
Gouraud path", and `zhao_raster_fragment.sv:92` cited the superseded §8 text as
*ratified law* and reasoned from it. All four corrected.

**But no fog mix exists anywhere in the tree.** `fog_near`/`fog_far` live only in
the ABI wire struct and sky env state; `FogMode` defaults to `Off` and nothing in
`reference/` or `tools/` ever sets it. So no golden CRC can move and the D-5
order is purely additive — there is no fogged colour to un-fog. The "ALREADY
FOGGED" comment described a stage that does not exist.

Two corrections to the handover: the ruling is already written into
`spec/qformats.md` §8 in full (only the docket was stale — D12 corrected), and
D-5 names **two** errors, the toon staircase and texture modulation multiplying
the fog colour. DOCKET R7 also already knew the old order was unimplementable:
`GEOM.PROJECT` has no colour input to have fogged anything with.

Not built: the ATTRSTEP factor lane and the post-toon mix. It should land in the
reference first (the reel defines correct) and it touches the vertex-attribute
path the rearchitecture brief defers — flagged rather than opened unilaterally.

## P0-C's precondition: the V3 owner has never been composed

`zhao_texture_v3own` is instantiated **nowhere** in `fpga/rtl/`. Every remaining
mention is a comment. `zhao_texture_island_top.sv:888` instantiates
`zhao_texture_fragrob`, and the island's source list in `design/fit_targets.yml`
carries `fragrob.sv` with no `v3own.sv`.

So the island's 13,601 ALM / 23,181 registers / 66.77 MHz contains the OLD
fragrob (1,676 ALM, 2,631 registers), and today's owner result — 5,709 → 3,348,
core→core 91.32 → 98.18 — sits entirely outside the composed design. It is real
and it cannot move the island until the block is instantiated.

**The arithmetic that must not be buried:** v3own at 3,348 is TWICE fragrob's
1,676, so a naive swap ADDS ~1,672 ALM to a composition already 13,601 against a
7,500 redline. The brief's answer is that the saving lives in what the swap makes
deletable, which is unmeasured. That is precisely the "first explanation that
absolves the design" shape, so it is written down as an open claim rather than an
assumption. The two blocks are also not like-for-like: v3own implements the whole
CAPTURE/SNAPSHOT/CLAIM/WRITE/PUBLISH/READY structure §6.2 requires, fragrob fuses
the checks into the payload write, which is the defect §6.1 names.

Useful consequence: there is no partial integration to finish and no second
ownership system running in the island. P0-C is a first instantiation plus the
removals it licenses.

## WHERE I WAS, before reading the rcp24 v3-rh fit

**In hand:** D-5's fog stage, reference-first, on the owner's explicit go-ahead
("Do the rearchitecture. Now's the time. Geom's probably fucked right now
anyway"). Implemented and building clean; `render_directed` **all green**, which
is the property that matters — with fog off the whole change must be
byte-identical, and it is.

**Unfinished:** the per-vertex factor is not yet WIRED to a caller (no projection
path computes `ScreenV::fogf` from the guarded `w` yet), and no test turns fog on
to exercise the mix. Both are the next step; the lane, the law and the mix exist.

**Baseline for the fit about to be read:** `zhao_raster_rcp24_v3@v3-full`,
commit `7d55fa84` — ALM **1,230**, registers **1,944**, reported Fmax **90.54**,
6 RAM, 3 DSP; block core→core was 129.18.

**Structural prediction on record:** no worst path should start at
`u_doneq|mem_q[..]` or `u_doneq|head_q[..]` any more. **Falsifier:** if they
still do, the DONE instance was not actually swapped or the fitter flattened the
wrapper back.

## §16.3 fit landed: area yes, clock no

`@v3-rh` vs `@v3-full`: **ALM 1,230 → 1,023 (−207), registers 1,944 → 1,460
(−484), M10K 6 → 8, reported Fmax 90.54 → 90.41.**

The structural prediction held exactly — `u_doneq` appears **zero** times in the
new path report, where it previously owned the worst internal path. The flop
array became RAM, which is where the −484 registers went.

**And it bought no clock**, which is the brief's own sentence arriving on my own
change: *"Removing the worst path is not the same as fixing the clock."* §16.2's
order (DONE queue first, not the multiplier) was right and it worked as an AREA
change; it simply was not the block's limiter.

**The 129.18 → 114.00 internal line is not a regression and must not be read as
one.** A path report samples ~200 paths; `internal_paths.py` reports the worst
internal path *among those sampled*, which is not the worst internal path in the
design. Removing the doneq family lets a multiplier path that was always there
become visible. Saying "15 MHz slower" would be exactly the mismatched-comparison
error. Report: `reports/S163-TICKETQ-RH-FIT-20260907.md`.

## POSITION BEFORE THE P0-B FIT

Launching `zhao_raster_rcp24_svc` — the brief's first-priority scheduler change,
on the specimen P0-A resolved.

**Before:** ALM **1,041**, registers **1,101**, reported Fmax **68.46**. Gate is
`max_alms: 650 / max_registers: 600`, already failing, and not relaxed.

**Structural prediction, the only one made:** the S1 register must appear in the
design — `s1_i_q` / `s1_ph_q` present — and the selection cone must no longer
reach a context read address in one hop, i.e. no worst path of the shape
`c_val[..] -> c_m.raddr_a[..]`. **Falsifier:** if that family survives, the
register did not break the cone and the change bought nothing.

**ALM and Fmax NOT predicted.** Adding a register normally costs area; whether
the shorter cone pays for it at this block's boundary is the question, and the
island's −3.243 family is the number that actually matters, which only a
composed fit can answer.

## Fog is MADE — reference-first, end to end, fire-tested

Owner: *"you said fog on my go-ahead? Well make the fog"*. Done in the oracle:
the `ScreenV::fogf` lane (alpha's exact interpolation shape), `zref_fog.hpp` with
§8's frozen factor law, **the producer** `apply_vertex_fog` reading the guarded
`w` that `ProjOut` already carried, and the mix at the final source colour after
the ramp — with `kAlpha` fogging the source before blending and `kAdditive`
unable to fog by construction per the frozen exempt list.

**A real defect in the frozen text had to be resolved:** §8 says `f = 1` is
CLEAR, but its mix weights toward `fog_c` by `f8`, which inverts fog. D-5
replaced that mix wholesale, so the surviving law is the factor and the weight is
its COMPLEMENT — argued at the call site, not silently chosen.

**The acceptance test was shown to fire.** `test_d5_fog_after_toon_quantiser`
asserts the toon band edges land on identical pixels with and without constant
fog. Injecting the forbidden pre-ramp order failed both assertions immediately;
source restored, `render_directed` all green — and green with fog off means
byte-identical, so no golden moved.

RTL still to come. The oracle defines correct, which is the order the handover's
own first question demanded.

## The fog RTL — `zhao_raster_fog`, differential against the oracle

D-5's step 5 in hardware, sitting after `zhao_raster_toon` and TEXTURE.COMBINE
and before `zhao_raster_blend`. Registered output boundary with a skid, per
QUARTUS_GOTCHAS 14 and §16.2's own lesson: the mix is cheap, the boundary is
where the timing goes.

**Verified against the reference, not against the spec prose.** The test uses
the SAME expression `rast.cpp` uses — copied deliberately, so that changing the
law forces both to be edited in one commit — and demands bit equality:
**13,416 fragments compared, 0 mismatches, 9 checks pass.** The sweep includes
both saturation rails and two out-of-range factors, so the clamp is proved to be
a clamp.

**Fire-tested with the exact defect the polarity discussion is about**: weight by
`f8` instead of its complement, i.e. inverted fog. Result **13,386 of 13,416
mismatches**, and the clear-rail assertion — written precisely for this — caught
it by name: *"at f = 0x10000 (CLEAR) the mix is the exact identity"*. Restored,
rebuilt, 0 mismatches again, exe newer than source both times.

Two RTL assertions ride along: a clear fragment is bit-identical to its input,
and a disabled (exempt) fragment cannot be fogged at all — so §8's frozen exempt
list is honoured by construction rather than by a caller remembering.

**Not done:** instantiating it in a composed top and wiring the ATTRSTEP factor
lane. The block, its law and its differential exist; nothing renders through it
yet.

## P0-B FIT: the Fmax delta is inside the block's own seed noise

`@p0b-s1` vs baseline: **ALM 1,041 → 1,200 (+159)**, registers 1,101 → 1,037,
reported Fmax 68.46 → 64.89, core→core 80.33 → 72.64.

**And the ledger already contained the reason not to read those Fmax numbers as
a result.** `svcseed2` and `svcseed3` share source commit `1c0a7f44` and differ
only in fitter seed: **68.63 vs 63.93 — 4.70 MHz of spread on identical RTL.**
My 3.57 MHz movement is inside that band, so this fit establishes neither a
regression nor an improvement. ALM +159 IS outside the noise and is a real cost.

**The structural prediction held.** The `c_m.raddr_a` endpoint is gone from the
worst path — baseline `m1_i_q[1] -> c_m.raddr_a[0]`, seed2 `c_val[4] ->
c_m.raddr_a[2]`, mine `c_val[0] -> Add7~21`. The register broke the
selection-to-RAM-address hop, which is what it was for. What did not change is
the LAUNCH point: the NCTX priority scan over `c_val && c_pend` still starts the
worst path, now feeding an adder instead of an address port.

§5.2 anticipated exactly this and sanctions the next move — two-level
arbitration with registered group heads — while warning not to build it before
"the first measured cut establishes what remains". It now has.
Report: `reports/P0B-RCP-ISSUE-STAGE-FIT-20260907.md`.

## Fog's geometry half, and a test that had to be repaired by its own fire test

`zhao_geom_fogfactor` computes §8's per-vertex factor from the guarded `w` that
`zhao_geom_project` already emits on `out_w_o`. `k` is a per-FRAME config port,
not computed here — §8 makes the denominator frame-constant, and a per-vertex
`field_rcp` would pay a divider to recompute a number that cannot have changed.
Differential against `zref_fog`: **401 vertices, 0 mismatches**, covering clear,
ramp and fully-fogged.

**The fire test found the test wrong, not the RTL.** Deleting the round-half-up
from the RTL produced **0 mismatches** — the sweep stepped `w` at exact metre
boundaries, so the product's low bits were aligned and the rounding term never
changed an outcome. It was measuring the clamp and calling it the arithmetic.
With an unaligned sub-metre stride the same mutation produces **138 mismatches**.
A differential that cannot see a rounding change is not a differential.

## POSITION BEFORE THE COMPOSED ISLAND FIT

Launching `zhao_texture_island_top`. This is the measurement P0-B actually needs:
the RCP among its real neighbours, where seed noise is diluted across a much
larger design and the island's own −3.243 ns family is the number that matters.

**Before:** ALM **13,601**, registers **23,181**, reported **66.77**, core→core
**75.51**, worst internal `rcp24_svc|c_val[5] -> rcp24_svc|c_m.raddr_a[0]` at
−3.243 ns. Benchmarks: 6,600 nominal / 7,500 redline / 7,913 standalone sum.

**Structural prediction, the only one:** the worst internal path must no longer
end at `c_m.raddr_a` — the S1 register removes that hop, and the standalone fit
already showed the endpoint gone. **Falsifier:** if the island still reports
`c_val -> c_m.raddr_a`, the composed build did not pick up the change.

**ALM and Fmax NOT predicted.** The standalone block grew 159 ALM; whether the
island's total grows by that, less, or more is a placement question. The brief
also warns an independent dispatch→FRAGROB family near −2 ns exists, so removing
one family need not move the clock at all.

## Fog's last mile, and the exact place it stops

`FogParams` (enabled, near, far, k, and the horizon colour) now threads into
`draw_heightfield`, defaulted OFF so every existing caller is bit-identical --
`render_directed` all green confirms it. Terrain is the first entry on §8's
FOGGED list, and it has exactly ONE projection site, so `apply_vertex_fog` is
called in exactly one place for the top lattice and one for the dual bottom.
`TriMode` gets the fog colour and the mix happens in the rasteriser, after the
ramp.

**Where it stops, precisely:** `render_frame.cpp:510` still calls
`draw_heightfield` without fog, because **nothing in the renderer consumes
`EnvState`**. `env_state.cpp` only serialises and deserialises it, and
`sky_and_beams.md` §4a says so in as many words -- *"the stand-in renderer does
not yet consume it (wiring it is the weather wave's consumer change)"*.

So the fog pipeline is complete and verified end to end EXCEPT for one
assignment: resolving `EnvState` + the active sky set's horizon colour into a
`FogParams` at frame scope. That is the weather wave's consumer change, which
the spec scopes elsewhere, and it is one call site away.

Stated plainly rather than described as "implemented", because the comment that
started this whole thread -- `vertex RGB: lit, tinted and ALREADY FOGGED` --
was itself a description of a stage that did not exist.

## §22.8, two more states — one already covered, one newly tested

**"Exercise an empty body with a pending queue read, the exact state the old
occupancy omitted"** — ALREADY COVERED, and I checked before writing anything.
`texture_v3rq_directed` was built for exactly this: its header carries the
three-edge counterexample (`rp_q` advances when the read is ISSUED, so the entry
leaves `body_occ_c` an edge before it reaches a head register) and line 109
asserts `zero_cycles == 0`, with line 142 the streaming form. Writing a second
test for it would have been duplicated work dressed as coverage.

**"Reset with work in each pipeline location" + "a killed or reset context must
not write back into its successor" (§6.3)** — NEW, case 23. Traffic runs with
COMBINE and output both held shut so owners pile up mid-pipeline, half of them
with one of two sources returned; the case then asserts that state genuinely
EXISTS (`ev_live_o > 0`, `ev_quiet_o == 0`) before resetting, because a reset
test on an idle block proves nothing and passes anyway.

After the reset it injects a return for a PRE-RESET owner — a slot and
generation that were legitimate moments earlier, distinguished by nothing except
that their owner no longer exists. No output, no admission. Then it admits and
completes a fresh owner, so the refusals above cannot be a block that simply
died.

**533 checks**, up from 526.

The ghost's rejection rests on the same current-membership term V04 already
proved necessary by mutation, so the mechanism has been fire-tested even though
this case was not re-mutated.

## `gen_prod_top.py`'s struct-port bug, fixed

The parser took the FIRST identifier in a port declaration as the port name.
Builtin types are stripped, but a user-defined type is not a keyword and
survives, so `input var zhao_guard_req_t req_i` produced a port called
`zhao_guard_req_t` wired to a one-bit signal. `zhao_prod_top` failed
`quartus_map` for exactly this, and it presents as a missing signal rather than
as a parser fault — which is why it survived two separate port changes.

The name is the LAST identifier outside brackets. Verified on four declarations
covering a struct port, a packed vector, a signed vector and a parameterised
width, then confirmed by regenerating and diffing the top:

    - logic [1-1:0] u17_zhao_guard_req_t;    - .zhao_guard_req_t(u17_zhao_...)
    + zhao_guard_req_t u17_guard_req_o;      + .guard_req_o(u17_guard_req_o)
                                             - .zhao_client_e(u17_src[42 +: 1])
                                             + .m_client_i(u17_src[42 +: 1])

Three ports named after their types, in one instance. A user-defined type now
also gives its wire that type instead of a guessed-width `logic`.

**BOUNDED CLAIM.** This fixes the NAMING defect and is demonstrated by the diff.
It does NOT establish that `zhao_prod_top` now passes `quartus_map`: the struct
and enum INPUTS are still stimulated from slices of a packed source vector, and
whether that elaborates is a separate question needing the toolchain, which is
busy with the island fit. Recorded as untested rather than assumed fixed.

## And the manifest checker caught my own two new blocks

`check_prod_manifest.py` failed immediately with `UNACCOUNTED: zhao_geom_fogfactor`
and `UNACCOUNTED: zhao_raster_fog` — a good instrument doing its job on the
person who just added them. Both registered as counted production blocks;
200 modules, 75 tops, 58 inside, 67 excluded, check OK.

## V05 fully closed — the write-enable PIN, not just its consequence

My own earlier note said *"the pins need a probe port"*. They did not. A
`/* verilator public */` marker is a COMMENT: Quartus never sees it, no port is
added, no area is spent, and simulation can read the flop directly. All three
bank enables — `c3t_we_q`, `c3a_we_q`, `c3f_we_q` — are now observable.

Case 4i drives an owner to full retirement through the output, then returns a
late packet on its exact handle and watches the enable across 20 cycles. It
never asserts. That separates "the write was never enabled" from "the write was
enabled and the data happened to be identical", which no consequence-based check
can do.

**With the non-vacuity guard that matters:** the same pin is then observed HIGH
on a legitimate return. Without it the case would pass against a signal that is
simply always zero — precision at zero being a tell, not a result.

**538 checks.**

### Three build traps in one small change, all documented ones

1. The `verilator public` marker changed the model's shape and the incremental
   verilate left a STALE generated file referencing `tail_q`.
2. Deleting the verilate output directory to fix that removed the `.cmake` file
   **build.ninja's own regeneration depends on** — so ninja could not rebuild
   the graph that would have fixed it. Exactly CLAUDE.md's trap, caused by me,
   and its documented fix worked: regenerate through `cmake --preset`.
3. Then the exe was stale twice more, and both times the tell was the same:
   `case 4i` did not print and the count stayed at 533. Comparing exe mtime
   against source mtime is what settled it — 19:34 exe against a 19:55 source
   the first time, 20:01 against 19:55 the second.

The lesson is not "be careful". It is that **the check is cheap and the failure
is silent**: a stale binary reports the old number with total confidence, and
today that has now happened four separate times.

## §22.9's forbidden-source check, built and self-fire-tested

> "The owner skeleton's fake COMBINE and tokenized fake services must not appear
> in the feature-live fit closure. List them explicitly in a forbidden-source
> check for production measurements."

`tools/quartus/check_forbidden_sources.py`. Current state: **36 targets, 30
production, 6 fixture, clean** — no production closure names a fixture today.

**Why it is a tool and not care.** A fixture in a production closure does not
fail. It fits, reports ALMs and an Fmax, and every number is wrong in the
flattering direction: a probe wrapper is smaller than what it probes, a `pair`
harness ties off ports the real design drives, a tokenized fake service answers
instantly where the real one stalls. It reads as a healthy measurement of a
design nobody built. That is the repository's own "a broken instrument lies in
ONE direction" law applied to the SOURCE LIST rather than to a parser.

**A directory rule, not a name list**, because a name list must be updated by the
person adding the next probe — the person least likely to remember. The rule
covers files that do not exist yet.

**And it proves it can FIRE, on every run.** `self_fire_test()` runs a known-bad
closure through the same `check_text` the real audit uses; if the rule stops
firing, `main` returns 2 and refuses to print a pass. Verified by disabling the
rule and watching the tool refuse rather than report OK.

The fire input also proves the rule DISCRIMINATES: it contains a `zhao_probe_*`
target naming the very same file, which must NOT be flagged, so a lazy "does
this path appear anywhere" rule fails the self-test instead of passing it.

**The fire test runs on SYNTHETIC text, never on `design/fit_targets.yml`** —
that file is read LIVE by a running fit at preflight (QUARTUS_GOTCHAS §13), so
editing it to exercise a tool is a way to corrupt a 90-minute measurement.

## §22.10 mutation 10 demonstrated, and the ledger made honest

`occ_o = lcnt_q` → `occ_o = body_occ_c` reinstates the owner's own §5 defect.
Four checks failed, first by name: *"occupancy is NEVER zero between an accepted
push and its head arrival"*. Restored, 28 checks pass, digest recorded — §22.10
asks for exactly that, and for the standard that a mutation which fails to
COMPILE is not evidence. This one compiled and ran.

**3 of 14 demonstrated**, and the other eleven are listed by name in
`reports/V31-S2210-MUTATION-LEDGER-20260907.md` rather than left implied. Item 8
is the instructive gap: it has a passing behavioural case but no mutation, and a
passing case shows the design is right while a mutation shows the TEST would
notice if it stopped.

## COMBINE: a deletion trigger that fired and was never executed

`design/prod_manifest.yml` says of `zhao_texture_combine` — refuted as D19q for
8 DSP against §3.4's "reject DSP > 2" — *"When that fit lands, delete this row,
its RTL, and tests/texture/texture_combine_diff.cpp together."*

**The fit landed.** `material_combine_v1` is `ok` at 1,475 ALM / 2 DSP / 36.28.
The trigger fired, nobody acted, and a block the architecture's own tripwire
refuted is still registered production and still in `zhao_prod_top`. Same shape
as CLAUDE.md's `.gitignore` lesson: a rule that records what should happen is
not the thing that makes it happen.

Meanwhile V2 supersedes V1 in the island — 870 vs 1,475 ALM, 114.04 vs 36.28 MHz.
**Checked rather than assumed**, because "smaller AND faster" is the comfortable
claim: V2's header quotes the recovery brief's "preserve all eight recipes" and
says the equations are V1's "byte for byte", and
`test_every_recipe_matches_the_oracle` runs 200 fragments per recipe across all
eight against the oracle. It is a like-for-like replacement.

Written up as a RECOMMENDATION, not executed: both are deletions of RTL and
tests, which is outside what this session decides alone.
`reports/COMBINE-SUPERSESSION-LEDGER-20260907.md`.

## Durable findings moved out of the run folder

CLAUDE.md: *"A run folder is the wrong home for anything durable — every pass
creates a new one."* Three of today's findings are methodology or state that the
next pass needs, so they are now docket entries rather than log prose:

* **M1 — a block's reported Fmax carries ~4.7 MHz of fitter-seed noise.**
  `svcseed2` 68.63 vs `svcseed3` 63.93 on the SAME commit. Sets the rule that a
  single-seed leaf-block difference under ~5 MHz is not evidence, and names the
  honest instruments: a composed fit, or several seeds.
* **M2 — COMBINE's deletion trigger fired and was never executed.**
* **M3 — the V3 owner is instantiated nowhere**, so today's −41% ALM sits
  outside the composed design, and a naive swap ADDS ~1,672 ALM.

## Fit targets for the two fog blocks: prepared, not applied

Written to the scratchpad rather than into `design/fit_targets.yml`, because
that file is read LIVE at preflight (QUARTUS_GOTCHAS §13) and the island fit is
running. It costs nothing to apply the moment the toolchain frees.

**And my first draft of it was wrong in a way worth recording.** I gave both
blocks a guessed `max_alms`. This file's own practice is to set a gate "at the
measurement with a little headroom rather than at an aspiration", and CLAUDE.md's
sharper form is that a rule written after the fit it governs reports a pass. A
guessed ceiling either passes and proves nothing or fails and gets edited. The
revised entries carry only STRUCTURAL rules — `max_dsp: 0` for the fog mix,
because a unit8 weighting that infers a DSP is written wrong rather than merely
large, and `max_dsp: 1` for the factor, because more than one multiply means the
per-frame reciprocal leaked into the per-vertex path, which is exactly what the
config port exists to prevent.

## Mutation campaign: 3 → 6 of 14, and the two that matter are next to each other

* **22.10-8** (final authorised by RESERVATION not ACCEPTANCE) — four M6 checks
  fail. Closes the gap where item 8 had a passing case but no mutation.
* **22.10-6** (same-row source OR → last-writer assignment) — case 1 fails six
  ways: nothing emits, nothing combines, no owner retires, the island never
  quiesces.
* **22.10-5** (publish before payload write) — **ESCAPED all 538 checks**, and
  then escaped my first fix as well.

**The contrast is the finding.** Items 5 and 6 mutate the SAME bitplane three
lines apart. Breaking WHICH bits are set stops the machine in the first case of
the suite. Breaking WHEN they are set was invisible to every check, because they
all observe the END of a transaction and one cycle of early publication changes
no final value. Severity of the source edit is no guide to detectability; only
the observable is.

**And the first fix for item 5 was instrumented on the wrong signal.** Case 24
was written against `ev_commits_o` — which increments on `c4t_v_q`, the C4 stage
valid, not on the commit bitplane the mutation altered — so the new check passed
against the very mutation it existed to catch. `cmt_q` is now `verilator public`
and case 24 watches it directly; against the mutation both its assertions fail.

That is the broken-instrument law twice inside fifteen minutes, on work written
in those fifteen minutes. A mutation campaign is normally described as testing
the DESIGN; this one tested the TESTS and found two blind spots.

**541 checks**, restored digest `2733389d405f0d9e`.

## Mutation campaign 6 → 9 of 14, and then a deliberate stop

* **22.10-4** recent-claim forwarding removed → `a_reject_partition_t` fires.
* **22.10-7** combine_reserved omitted on the credited pop → `a_p22_cbi_implies_crs`
  fires, an invariant written hours earlier the same day.
* **22.10-2** membership subtraction truncated to the slot field → six checks,
  led by "a stale GENERATION on the issue lane is refused", and it broke
  THROUGHPUT as well as identity (58 of 64 emitted, then 0), because owners that
  falsely test live corrupt the retirement accounting too.

**The pattern across nine mutations is now stable and worth carrying forward:
bench checks catch corrupted VALUES; in-RTL invariants catch broken ORDERING and
broken PARTITIONS.** Item 5 escaped 538 checks precisely because it corrupted
neither — it changed WHEN a bit was set, and nothing was watching that. The
remaining mutations most likely to escape are the timing-shaped ones for the
same reason.

## STOPPING THE BUILDS ON PURPOSE — they are slowing the fit

Checked rather than assumed: `quartus_fit` CPU time went 10,143.6 → 10,239.8 s
across 45 wall-seconds, so the fit is alive and multi-threaded, not stuck.

But that is also the problem. Each mutation cycle needs a full Verilator rebuild
of the owner block, and those have grown from ~2 minutes to ~8 as they compete
with the fit for cores; this island fit is at ~2 h against the previous one's
~90 min. **The fit is the higher-value deliverable** — it is P0-B's real answer
and G1-D's headline — so continuing to spend cores on mutation rebuilds is
optimising the cheaper thing.

Switched to work that needs no compiler.

## G1-D §4.3f written BEFORE the result

The island before-picture, its prediction and its falsifier are now in the report
ahead of the number, so the comparison cannot be arranged after the fact:
ALM 13,601, registers 23,181, reported 66.77, core→core 75.51, worst internal
`c_val[5] -> c_m.raddr_a[0]` at −3.243 ns, against 6,600 / 7,500 / 7,913.

**Structural prediction only:** the `c_m.raddr_a` endpoint must be gone. ALM and
Fmax deliberately unpredicted, with the reasons recorded — the standalone block
grew 159 ALM, an independent dispatch→FRAGROB family near −2 ns still exists, and
docket M1's seed noise is diluted by composition but not abolished. What counts
as a good result is stated in advance so it cannot be rationalised afterwards.

## THE ISLAND FIT LANDED — P0-B is worth +12 MHz in composition

`@p0b-island`, 9,364 s. **Reported Fmax 66.77 → 78.80 (+12.03, +18%).**
ALM 13,601 → **13,615 (+14)**. Registers +114, M10K 36 → 37, DSP unchanged.
Worst-path slack −3.243 → −2.690.

**The prediction held**: the worst path no longer ends at `c_m.raddr_a`. All four
worst paths now run `rcp24_svc|c_pend[7] -> perspuv_svc|e_num_*`.

**+12 MHz clears the bar I set in advance** — §4.3f pre-registered "under ~2 MHz
is movement, not improvement" — and it is 2.6× docket M1's 4.70 MHz seed band.
The standalone fit could not have shown this: there, the same change looked like
−3.57 MHz inside a 4.70 MHz noise band.

**The area result is the genuine surprise.** The standalone block grew +159 ALM
for the register; the composed island grew **+14**. Placement absorbed almost all
of it. A leaf fit priced the change at ELEVEN TIMES its cost in situ — the
opposite of the usual direction, and a concrete argument for the brief's
insistence on composed measurement.

**What did not change is the launch point.** Every worst path still starts at
`c_pend[7]`, the round-robin eligibility scan; only the destination moved. §5.2's
two-level arbitration is now the sanctioned next step on evidence.

**Still failing the budget:** 13,615 ALM is 2.06× nominal, 1.82× redline, 1.72×
the standalone sum, unmoved because the area did not move. Timing improved, size
did not. And `status: ok` on a LABELLED row does not mean the gates passed —
variant rows are not rule-checked (`v3own@v3-full` is `ok` at 5,678 against
`max_alms: 1800`). Recorded so nobody reads that field as a pass.

**A stale file nearly became a claim.** My first `raddr_a` count read
`zhao_texture_island_top.setup.rpt` — 16:30, the PREVIOUS island fit. The
labelled run writes `zhao_texture_island_top@p0b-island.setup.rpt`, a different
file. The mtime check caught it. Second time today that comparing timestamps
before believing a number was the thing that worked.

## AUDITING THE GOOD NEWS: a second seed of the same island source

+12 MHz is the best number this session produced, which is exactly why it gets
the scrutiny a bad number would. Docket M1's 4.70 MHz seed band was measured on a
LEAF block; **the composed island's own seed spread is unknown**, so "12 exceeds
4.7" compares against a band from a different design.

Launching `zhao_texture_island_top -Seed 3 -RowLabel '@p0b-island-s3'` — the same
sources, the same digest, a different fitter seed. It answers two things at once:
whether the +12 MHz survives a reseed, and what a composed island's seed spread
actually is, which no measurement in this repository currently records.

**Before:** `@p0b-island` reported **78.80**, ALM **13,615**, registers 23,295,
M10K 37, DSP 17, worst path `rcp24_svc|c_pend[7] -> perspuv_svc|e_num_v[13][17]`
at −2.690 ns.

**Prediction:** none on Fmax — that is the quantity under test and predicting it
would defeat the purpose. **Structural prediction:** the worst path should still
LAUNCH from `c_pend[..]`, because the eligibility scan is a source-level
structure and not a placement accident. **Falsifier:** if seed 3 lands near
66.77, the +12 MHz was seed luck and §4.3f must be rewritten.

Fog blocks also registered in `design/fit_targets.yml` now that the toolchain is
free — structural DSP rules only, no guessed ALM ceilings.

## P0-E measured while the reseed runs: the prize is ONE array, not a restructuring

`island_top` declares twenty per-context side tables totalling **17,040 bits** —
P0-E's "duplicated state" by name. But the measured top-level pool is ~6,981
registers, so most of them are already RAM. Taking 17,040 as the target would
have been wrong by 2.5×.

The map report says which and why. Eighteen uninferred RAMs in the island, in two
categories that must not be confused: **fourteen are "inappropriate RAM SIZE"**,
correct refusals for arrays too small for an M10K, nothing to fix. **Four are
"asynchronous read logic"**, the fixable pattern.

Then checking each of the four SHRANK the prize:

* `uvw_m` — 64b × 64 = **4,096 flip-flops**, the real target;
* `zhao_field_rcp24_rom|Ram0` — a pure `always_comb` case lookup, §6.2's
  field_rcp table. Combinational is what it IS, not a defect;
* `fragrob|tok_m` — 16 entries, small;
* `class_m` — 32 flops, negligible.

**"Four arrays share a fixable pattern" was the satisfying version and it was
wrong.** One array is worth fixing, and registering its read moves 4,096 flops
into an M10K the island has 516 spare of.

**This also refines docket M3.** That entry says the v3own integration's saving
lives in unmeasured deletions. Part of it is now measured and it is not a
deletion at all — it is a read-port change on ONE named array, independent of the
ownership rework and available without it. A much cheaper piece of work than the
restructuring it was bundled with.

## I HAD TO RETRACT THE SESSION'S BEST NUMBER

The island's +12.03 MHz is NOT attributable to P0-B, and I wrote it up as if it
were.

**What I should have checked first:** where the OLD island's worst paths were.
They were PALETTE — six of the top six, launched from the input port
`pal_ld_gen_i[4]`, worst −4.977; sixteen of the worst forty. The first rcp24 path
ranked SEVENTH at −3.243. In the new report palette appears **zero times in 8,680
paths**. P0-B's register is inside `zhao_raster_rcp24_svc` and cannot move a
palette port path.

**Then I checked whether the comparison was even clean**, and it is:
`git diff` over all fifteen island sources between the two commits shows **one
file changed**, `rcp24_svc.sv`. Same 1,487 pins, same DSP, same source count. So
the palette family moved through PLACEMENT — a register added in one block
changes global placement, and port-launched paths are the most sensitive thing
there is to that.

**What survives:** the structural prediction (`c_m.raddr_a` gone, replaced by
`c_pend -> e_num_*`) held exactly, and ALM +14 composed against +159 standalone,
which no placement argument touches.

**What I got wrong, precisely.** I pre-registered "under ~2 MHz is movement, not
improvement", saw 12.03, and treated clearing that bar as settling the question.
**Clearing a pre-registered bar proves the MOVEMENT is real. It proves nothing
about the CAUSE.** Pre-registration guarded the wrong failure mode and I used it
as if it guarded both. That is the "first explanation that absolves" law in its
flattering direction, on the best number of the session, which is exactly when it
is hardest to see.

Corrected in G1-D §4.3f and docket M4; new docket M6 records the general rule —
**a composed reported-Fmax delta is attributable only if the gating path family
is the SAME before and after.**

## M6 applied uniformly, including where it does NOT cost me

Having retracted the island's +12 MHz, the same rule has to be run against the
session's other Fmax claim rather than only the inconvenient one.

**The T2 owner fit fails the same test.** Baseline worst path `fence_open_q ->
fence_open_q` (recorded in the T1 fence report); result worst path
`req_q[2][1] -> iss_q[18][1]`. Different families, so core→core 91.32 → 98.18
is movement whose cause those two numbers do not establish.

**But the two cases are not equally weak, and flattening them would be its own
dishonesty.** The island's palette family moved with ONE file changed and no
structural connection to it — placement roulette. The owner block's change
deleted 512 flip-flops and 41% of its ALM; that is a structural upheaval which
would plausibly move many families at once. Plausible is not established. The
honest form: the block got faster, the change is large enough to explain it, and
nothing here isolates the two.

**Unaffected in both cases:** the structural claims. ALM on identical scope,
registers predicted −560 and measured −560, `gen_q` and `ftc_q` in zero paths.
Those are statements about what the netlist CONTAINS, and no gating-family or
placement argument touches them.

**And the audit's most useful result:** §4.3e, written two days ago, already did
this — *"−4.800, a virtual pin into PALETTE_RES | −4.977, THE SAME PIN"*. The
discipline existed; my §4.3f dropped it while being scrupulous about prediction.
Being rigorous about one failure mode does not transfer to another.

## The reseed is a clean experiment, and the M6 sweep is worse news than one error

**Reseed confirmed clean:** `@p0b-island-s3` hashes to digest `c9283ca728dd` —
IDENTICAL to `@p0b-island`. Same fifteen sources, seed the only variable. CPU
4,417 → 4,469 across 40 wall-seconds, so it is working.

**Then the new index answered a question I had not thought to ask.** Sweeping
every base/variant pair on disk for a shared gating family: **0 of 4**.

    rcp24_svc / @p0b-s1        -4.607  -5.411  CHANGED
    island_top / @p0b-island   -4.977  -2.690  CHANGED
    v3own / @v3-full           -0.633  -3.194  CHANGED
    probe_banked_rf / @v3hot   -0.358  -0.736  CHANGED

**Not over-claiming:** these are not four controlled experiments. `v3own` vs
`@v3-full` are commits far apart, and one row is a synthesis probe. The sweep
compares whatever shares a base name.

**What it does establish:** a shared gating family between two fits of the same
module is not the normal case here — it is unobserved in every pair on disk. So
my island error was not a lapse against a background of sound comparisons; it was
the first time anyone checked.

**The consequence for how this project reports:** structural claims carry the
weight — ALM on identical scope, registers against a prediction, what the netlist
contains. Those survive placement entirely. Frequency deltas need the family
named on both sides, and now they can have it.

## OWNER DIRECTION: the island restructure is to happen

*"we need the island restructure to happen. Make sure you do whatever
measurements you need, finish the current one. Then have a fable agent architect
the restructure."*

**Measurements finished first, as instructed.** Mutation §22.10-11 (F advances
without a reserved packet slot) → `a_out_reserved` fires. **Eleven of fourteen.**
Every credit invariant in the block — `a_cmb_reserved`, `a_out_reserved`,
`a_p22_cbi_implies_crs`, `a_reject_partition_t` — has now caught the mutation it
was written for. RTL restored, 541 checks.

**FABLE architect launched** for P0-C, briefed with today's measurement base
rather than left to rediscover it: the 4-of-60 port overlap, the +2,404 ALM /
+6,981 register glue pool, `uvw_m`'s 4,096 async-read flops, the composed island
numbers and their budgets, and the T2 identity law it must preserve.

**And bound by today's measurement discipline**, which is the part most likely to
be skipped: M1's seed noise, M6's gating-family rule and the worst-path index,
M4's leaf-versus-composed mis-pricing. Its plan must make every stage
independently measurable with a falsifier, because two changes in one fit produce
one unattributable number — which is exactly the error I made and retracted
today.

The island reseed continues in parallel; it is unrelated to the restructure and
settles P0-B's attribution.
