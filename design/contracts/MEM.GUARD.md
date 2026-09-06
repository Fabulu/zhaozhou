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

The map owner: FB slot 0 @ 0x0000_0000 (bank 0), FB slot 1 @ 0x0200_0000 (bank 1 — the W2.7 bank split, spec/memory_rules.md §5), each 0x3C000 B (largest canvas — a mode switch never moves a slot); everything else — including the hole between the slots — unmapped = violation by construction. The framebuffer-write window is held under a LEASE THAT NAMES ITS WRITER, because two blocks write an inactive slot: DEBUG.FRAMEBLIT and RASTER.FBWRITE. They share the SPATIAL window and not the TEMPORAL permission -- `fb_writer` selects which client the lease is held by, and the other is refused exactly as an out-of-region request is. A second overlapping region entry was rejected: it would copy the same address law, cost more policy plumbing, and still not stop the two writers corrupting each other. VIDEO.SLOTMGR already guarantees one lease at a time with a generation, so the lease is where the writer is named; CMD.SCHEDULER selects it, MEM.GUARD enforces it, and the writer proves its traffic retired before publication. **A v1 frame uses the renderer or DebugFrameBlit, never both.** The lease holder writes exactly its granted slot and exactly `canvas_bytes(mode)` bytes (any other `DebugFrameBlit.byte_len` is rejected BEFORE the first byte); scanout owns both slots read-only.

Two Phase-3 windows have been APPENDED since, each with the block that needed it, each with constant bounds and a single direction — which is what keeps the no-escape theorem's meaning while the map grows. `GEOM.ASSET_POOL` @ `0x06A0_0000`, 22 MiB, **ENGINE1, read-only** (spec/memory_rules.md §5f): nothing forwarded there can alter a frame buffer. `TERRAIN.PAGE_POOL` @ `0x0400_0000`, `0x014E_0000` B, **TERRAIN_BUILD (client 6), write-only** (ruling T2 / §5b): the narrowest reading of T2 that lets a page be loaded at all — one of the six bank-2 regions, one client, one direction, no map input. It is NOT the state-aware permission T2 eventually wants ("a loader may write only a LOADING slot"); that needs a residency→guard interface nobody has ruled, and the gap is stated at `terrain_ok` in the RTL and in §5b rather than left for a reader to find. DEBUG still owns nothing, and neither does the unspent client 5.

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

`tests/formal/mem_guard_no_escape.sby` — no request outside a granted region is ever forwarded; no partial forwards; deny-all after reset until a grant (charter §20.4 "malformed commands cannot write outside assigned memory"). `a1_map` names every region the map has, so a new window widens the theorem instead of being exempted from it; `a1_asset_ro` and `a1_terrain_wo` / `a1_terrain_owner` state each Phase-3 window's direction and ownership as theorems rather than as consequences of `pass_ok`'s spelling; `c_forward_asset` / `c_forward_terrain` keep them non-vacuous. Shown to FIRE: dropping `req.write` from `terrain_ok` fails `a1_region` and `a1_terrain_wo`; widening the window by 64 KiB fails `a1_map` and `a1_region`; admitting ENGINE1 to the pool fails `a1_region` and `a1_terrain_owner`; withdrawing the arm leaves `c_forward_terrain` unreached.

## Synthesis / resource ceiling

Budget group `command_debug` (§25 5% ceiling). Comparators + latches.

## Integration capture cases

`captures/golden/wave2/*` — every blit in every capture flows through the guard; the COUNTERS sections imply zero violations (a violation would fault the frame and appear in deadline_faults/repeat).

## Notes

All VRAM traffic flows through the guard; the schematic draws the major clients only. Phase-2 map is deliberately minimal — later phases APPEND regions (texture/terrain/particle pools), never reshape the law.
