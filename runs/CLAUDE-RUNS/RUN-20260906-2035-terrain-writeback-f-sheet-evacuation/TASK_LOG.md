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
