# Contract — MEM.GUARD (Memory guard)

> Ledger: `design/blocks.yml` · owner ZH-029 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Region/ownership checker between EVERY fabric client and VRAM: a malformed command can never write (or read) outside the region its frame slot owns. Requests that pass are forwarded to the arbiter; violations are dropped, pulsed and counted — formally no-escape. Law: `spec/memory_rules.md` §5.

Exclusions: no arbitration (MEM.VRAM.ARBITER), no DRAM (MEM.SDRAM), no HPS DDR (MEM.HPS.BRIDGE), no policy table maintenance (the region map is compiled in per phase; Phase 2 has exactly two FB regions).

## Clock and reset semantics

`gpu_clk` domain, synchronous active-low `rst_n`. Reset: deny-all (no region is owned until CMD.SCHEDULER grants for the frame), violation latch clear. Reset reaches idle.

## Input and output packet layouts

Client side: `zhao_guard_req_t {valid, write, client, addr[26:0], len, byte_enable}` (frozen in zhao_pkg.sv). Guard response: `zhao_guard_rsp_t {ok, violation}` — combinational verdict, 1 cycle. Forwarded side: the same request onto the arbiter port. Region map input: per-client `{base, size}` registers written by CMD.SCHEDULER at frame grant (Phase-2 values are the constants of spec/memory_rules.md §5).

## Backpressure rules

`ready_valid` end-to-end: the guard never drops a legal request and never invents one; a denied request is ANSWERED (violation), not retried, and the client's contract defines recovery.

## Memory ownership

The map owner: FB slot 0 @ 0x0000_0000, FB slot 1 @ 0x0003_C000, each 0x3C000 B (largest canvas — a mode switch never moves a slot); everything else unmapped = violation by construction. Blit DMA writes exactly its granted slot and exactly `canvas_bytes(mode)` bytes (any other `DebugFrameBlit.byte_len` is rejected BEFORE the first byte); scanout owns both slots read-only.

## Q formats and rounding

Unsigned integer address/length compare only (exact, no rounding).

## Latency (fixed or variable)

Fixed: 1 cycle (`fixed:1`).

## Target throughput

1 checked request per clock.

## Overflow and malformed-input behaviour

A request outside the client's region — or a write to a read-only region, or a length crossing the region end — is denied: NOTHING is written (formal `mem_guard_no_escape`), `guard_violation` pulses, the drop is counted in the client's trace. Address arithmetic is done at 32 bits internally so `addr+len` cannot wrap past the map. No request is ever partially forwarded.

## Counters and traces

Feeds `vram_bytes_by_client` / `hps_ddr_bytes_by_client` accounting (accepted bytes only); violation events trace to the harness with the full request (the saved-failing-vector lane, charter §29-17).

## Scalar reference function

`zref::MemoryGuard` — verdict oracle: given the region map and request stream, the exact ok/violation sequence.

## Directed tests

`tests/memory/mem_guard_directed.cpp` — out-of-region write rejected, nothing written (shadow memory); read-only violation; boundary exactness (last byte in / first byte out); wrap-address attempt; blit length mismatch rejected pre-byte.

## Randomized differential tests

`tests/memory/mem_guard_directed.cpp (verdict oracle; no separate random file)` — PCG request streams incl. adversarial boundary fuzz vs `zref::MemoryGuard`, plus 64 KiB shadow-memory integrity (no escape, ever).

## Formal properties

`tests/formal/mem_guard_no_escape.sby` — no request outside a granted region is ever forwarded; no partial forwards; deny-all after reset until a grant (charter §20.4 "malformed commands cannot write outside assigned memory").

## Synthesis / resource ceiling

Budget group `command_debug` (§25 5% ceiling). Comparators + latches.

## Integration capture cases

`captures/golden/wave2/*` — every blit in every capture flows through the guard; the COUNTERS sections imply zero violations (a violation would fault the frame and appear in deadline_faults/repeat).

## Notes

All VRAM traffic flows through the guard; the schematic draws the major clients only. Phase-2 map is deliberately minimal — later phases APPEND regions (texture/terrain/particle pools), never reshape the law.
