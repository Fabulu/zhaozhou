# Blocked on hardware — wave 1 closeout

> Owner: orchestrator (hardware lane). Every entry names the EXACT unblocking
> action. Ledger enforcement: `blocked_on: hardware` blocks can never advance
> from SPECIFIED regardless of evidence (rule V2, plan §4) until the
> orchestrator clears the lane. Currently 6 blocks across 4 owner issues.

Machine truth (updated 2026-08-15): this machine has **Quartus Prime Lite
17.0.2** (`C:/intelFPGA_lite/17.0/quartus/bin64/quartus_sh.exe`) — the
synthesis lane was opened in commit 5189f0c, which produced the first real
fabric numbers from the standing ABI synth probe. It still has **no MiSTer
board and no DE10-compatible hardware** (ENV: oss-cad-suite 20260814, mingw64
g++ 16.1.0, Node 20.17, Windows 11).

So the entries below split in two: everything needing a *board* remains
blocked, while everything needing only *synthesis* is now achievable locally
and is no longer covered by this document's blanket claim. Nothing was frozen
from assumptions while they are open (charter §23 Phase-0 gate: "no
architecture constants frozen from internet assumptions").

## ZH-000 — MiSTer `sys/` vendoring (SYS.* blocks: 3)

Blocks: `SYS.CLOCK`, `SYS.RESET`, `SYS.CDC` (design/blocks.yml, all
`blocked_on: hardware`, SPECIFIED).

- **What is missing:** the upstream MiSTer framework `sys/` directory
  (MiSTer-devel/Main_MiSTer), vendored pin-by-upstream-hash.
- **Why blocked:** vendoring without a board to verify against would freeze a
  moving target; the Verilator lane needs only `fpga/rtl/`.
- **In place meanwhile:** `fpga/sys/PROVENANCE.md` documents the
  pin-by-upstream-hash strategy; the RTL tree is conservative-SV and
  lint-clean without sys/.
- **EXACT unblocking action:** clone MiSTer-devel/Main_MiSTer at a named
  commit, vendor `sys/` into `fpga/sys/` with the hash recorded in
  `PROVENANCE.md`, build the empty core once on hardware, then flip the three
  SYS.* blocks' `blocked_on` in design/blocks.yml (orchestrator) — RTL_VERIFIED
  evidence per the normal ladder from there.

## ZH-002 — Quartus synthesis report extraction (SW.TOOLS.REPORT)

Block: `SW.TOOLS.REPORT` (SPECIFIED, `blocked_on: hardware`).

- **NO LONGER BLOCKED ON MISSING QUARTUS** (2026-08-15). Quartus Prime Lite
  17.0.2 is installed and commit 5189f0c opened the lane. What remains is
  ordinary work, not a hardware blocker: run the fitter over the stub top and
  point the parser at the real report.
- **What is missing:** the parser has still not been exercised against a real
  fitter report. The schema (reports/synthesis/*.json) is designed.
- **EXACT unblocking action:** synthesize the stub top with the installed
  Quartus and run the tool over the actual report; every `resource_actual`
  entry must come from the parser output — never hand-typed (charter
  discipline; V5 activates at SYNTHESIZED).

## ZH-003 … ZH-006 — Phase-0 board probes (SW.TOOLS.BOARDPROBE, MEM.SDRAM)

Blocks: `SW.TOOLS.BOARDPROBE`, `MEM.SDRAM` (SPECIFIED, `blocked_on: hardware`).

| Issue | Probe | Unblocks |
|---|---|---|
| ZH-003 | SDRAM memtest + timings probe | MEM.SDRAM timing constants (currently "cycle-approximate until board_truth.json exists", ledger note) |
| ZH-004 | local SDRAM bandwidth (sequential/strided) + burst latency vs length, **and the device's bank-address mapping** | memory arbitration policy + §25 budget reality; **the W2.7 FB-slot bank split assumes byte-address bit 25 selects the DRAM bank** (`ZHAO_FB_SLOT1_BASE = 0x0200_0000` — spec/memory_rules.md §5, RATIFICATION-REQUEST-fb-slot-bank-split.md). If the board maps banks differently, the slot-1 base is the single retune knob |
| ZH-005 | clock tree / stable graphics clock | CDC + video timing choices |
| ZH-006 | input latency probe | INPUT.* latency contracts |

- **What is missing:** `reports/board_truth.json` does not exist — by design,
  no probe result has ever been simulated or assumed.
- **EXACT unblocking action:** run each probe on the physical board
  (framework DDR burst probe, memtest, clock measurement, input loopback),
  emit `reports/board_truth.json` from the probe tools (machine-written,
  never hand-edited), then the orchestrator clears `blocked_on` and the
  affected constants move from "provisional" to board-frozen in one ledger
  edit each.

## Consequences downstream (states that cannot be reached yet)

- **INTEGRATED / HARDWARE_PROVEN** for every RTL block: requires the board.
  **SYNTHESIZED** no longer requires anything missing — Quartus is installed
  (5189f0c) — so that rung is reachable work rather than a hardware blocker.
  No RTL block has violated the ladder by skipping ahead.
- **8-hour stress / 24-hour memory stress** (charter Phase-0 gate): requires
  the board; not runnable, not simulated.
- **MiSTer runtime product** (`runtime/mister/`): empty on purpose, folds
  under ZH-000 with SYS.*.

## Not blocked (checked, for the record)

The desktop lane is NOT blocked: ZEmu + runtime/desktop stubs link and replay
the golden frame green (CTest `zemu_smoke`, `desktop_smoke`). Formal (sby)
runs locally via oss-cad-suite. The hardware lane above is the complete list.
