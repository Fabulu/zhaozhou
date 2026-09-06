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
