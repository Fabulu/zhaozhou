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
