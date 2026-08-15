# Contract — MEM.SDRAM (Local SDRAM controller)

> Ledger: `design/blocks.yml` · owner ZH-004 · phase 2 · maturity SPECIFIED · **blocked_on: hardware**

## Purpose and exclusions

128 MB local BGA SDRAM controller: refresh scheduling, bank FSM, burst shaping, the credit port the arbiter drives. All RTL is synthesizable and verified against a behavioural model; board truth (device ordering code, speed grade, measured timings) arrives from probe ZH-004 (`board_truth.json` → `fpga/rtl/generated/sdram_params.svh`). Law: `spec/memory_rules.md` §1 (D2).

Exclusions: no arbitration policy (MEM.VRAM.ARBITER), no region checking (MEM.GUARD), no HPS DDR (MEM.HPS.BRIDGE). This block never leaves SPECIFIED (blocked_on: hardware) — evidence banked, advancement gated on the hardware lane.

## Clock and reset semantics

`sdram_clk` domain (params package carries the clock phase); synchronous active-low `rst_n`. Reset: command bus idle, all banks precharged, refresh timer armed, credit port idle; init sequence (PRECHARGE-ALL + 2× AUTO_REFRESH + MODE REGISTER SET) runs before any client traffic — modelled identically by the behavioural sim model.

## Input and output packet layouts

Client port: credit-based request `{valid, write, addr[26:0], len}` (len ≤ burst 8) with reissued credits `{credits}` as bursts retire — the type is `zhao_arb_rsp_t` from zhao_pkg.sv. DRAM-side: fixed SDRAM command/word interface per the params package (CAS 3, burst 8). Refresh is internal: one AUTO_REFRESH every 780 sdram cycles on the frozen sim profile.

## Backpressure rules

Credit-based at this edge (D3): a client may request only with credit; each accepted beat consumes one; credits return on retirement. The controller never drops a credited request — worst-case grant latency is the arbiter liveness bound (spec/memory_rules.md §2).

## Memory ownership

The whole 128 MB is VRAM behind MEM.GUARD; this controller enforces no policy, only timing. Per-client byte accounting belongs to the arbiter.

## Q formats and rounding

None (address/count integers only).

## Latency (fixed or variable)

Variable, profile-exact: read = tRCD(3) + CAS(3) + burst(≤8); write = tRCD(3) + burst(≤8); bank conflict adds tRP(3)+tRC overhead; refresh steals tRC+tRP(12) — all frozen in `spec/memory_rules.md` §1 and asserted cycle-exact against the model.

## Target throughput

1 word per sdram cycle sustained within a burst; burst-8 granularity at the client port.

## Overflow and malformed-input behaviour

An uncredited request is a protocol violation (assertion in sim; the arbiter never issues one). Address bits beyond 128 MB cannot arrive (27-bit port, guard-limited). Refresh never starves a bank indefinitely (refresh bound property, banked below). Timing violations cannot be expressed by the port contract.

## Counters and traces

`sdram_refresh_stalls` (cycles stolen by refresh), `sdram_bank_conflicts` (activate-on-active-bank penalties) — both shadow-latched at frame_tick. Trace: per-transaction timing ledger to the harness (oracle compare).

## Scalar reference function

`zref::SdramController` — transaction-level timing oracle under the frozen sim profile: given the request stream, the exact per-burst retirement times.

## Directed tests

`tests/memory/sdram_directed.cpp` — refresh steals counted on a deterministic schedule; bank-conflict penalty exact; read/write latency profiles vs the model cycle-for-cycle.

## Randomized differential tests

`tests/memory/mem_random.cpp` — three-way random (arbiter+ctrl+behavioural model vs oracles) with a 64 KiB shadow-memory integrity compare (spec/memory_rules.md §7).

## Formal properties

`tests/formal/mem_sdram_refresh_bound.sby` — refresh never deferred past the interval bound (BANKED: runs only against the synthesizable core; maturity evidence held for the hardware lane).

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). Absolute resources unfrozen until Phase 0 (V5 gate; charter §25).

## Integration capture cases

Banked (blocked_on: hardware): the obligation list a board capture must satisfy post-ZH-004 is `spec/memory_rules.md` §1 (device code, speed grade, clocks, sustained bandwidth, measured tRCD/tRP/tRC, refresh accounting, thermal). Phase-2 captures run against the sim profile and record its version.

## Notes

Timings are board data (ZH-004); the simulation profile is conservative and FROZEN — downstream blocks verify against it, never against hoped-for board numbers.
