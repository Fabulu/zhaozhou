# Contract — MEM.SDRAM (Local SDRAM controller)

> Ledger: `design/blocks.yml` · owner ZH-004 · phase 2 · maturity SPECIFIED · **blocked_on: hardware**

## Purpose and exclusions

128 MB local BGA SDRAM controller: refresh scheduling, bank FSM, burst shaping, the credit port the arbiter drives. All RTL is synthesizable and verified against a behavioural model; board truth (device ordering code, speed grade, measured timings) arrives from probe ZH-004 (`board_truth.json` → `fpga/rtl/generated/sdram_params.svh`). Law: `spec/memory_rules.md` §1 (D2).

Exclusions: no arbitration policy (MEM.VRAM.ARBITER), no region checking (MEM.GUARD), no HPS DDR (MEM.HPS.BRIDGE). This block never leaves SPECIFIED (blocked_on: hardware) — evidence banked, advancement gated on the hardware lane.

## Clock and reset semantics

`sdram_clk` domain (params package carries the clock phase); synchronous active-low `rst_n`. Reset: command bus idle, all banks precharged, refresh timer armed, credit port idle; init sequence (PRECHARGE-ALL + 2× AUTO_REFRESH + MODE REGISTER SET) runs before any client traffic — modelled identically by the behavioural sim model.

## Input and output packet layouts

Client port: credit-based request `{valid, write, addr[26:0], len}` (len ≤ burst 8) with reissued credits `{credits}` as bursts retire — the type is `zhao_arb_rsp_t` from zhao_pkg.sv. DRAM-side: fixed SDRAM command/word interface per the params package (CAS 3, burst 8). Refresh is internal: one AUTO_REFRESH every 780 sdram cycles on the frozen sim profile.

## Burst wrapping and the 16-byte quantum

**Until 2026-09-23 this contract contained the words `align`, `wrap` and `BL8`
exactly ZERO times**, and that is why no client could have been told the law
and no review could have checked it. It is written down here now (owner ruling
R243 / D-SDRAM-A).

> **THIS FILE IS GENERATED** by `tools/ledger/src/gen/contracts.ts`, whose
> `HEADINGS` list does not contain this section. Nothing in the tree currently
> regenerates it — `git log` shows no touch since the wave-2 fill — but a
> regeneration WOULD delete this section, so the law's durable home is
> `spec/memory_rules.md` §5c and the headers of `zhao_vram_arbiter.sv` and
> `sim/models/zhao_sdram_model.sv`. This section exists because the ruling
> named the contract's silence as the reason nobody could have been told; if
> the generator is ever run, carry it across.

`zhao_sdram_ctrl` programs the mode register **BL8 SEQUENTIAL** (`A[2:0]=011`,
`A3=0`) and derives the column straight from the request address
(`waddr = req.addr[26:1]`, `req_col = waddr[10:0]`). A JEDEC sequential burst
of length 8 **wraps inside its ALIGNED EIGHT-COLUMN BLOCK**: `col[10:3]` is held
and `col[2:0]` advances, so from column 5 the part returns `5,6,7,0,1,2,3,4`. A
column is one 16-bit word, so **the aligned quantum is SIXTEEN BYTES**.

**Two true sentences used to stand in for this one and did not cover it.** The
model's header said bursts *"wrap within the row (sequential, modulo 2048) …
so the wrap is unreachable in lawful traffic"*, and the arbiter's
`burst_words` said *"8 divides the row so a tail burst never crosses"*. Both are
correct **about the row**; the part wraps in the **block**; and both ended in a
reassurance, which is what a reader took away.

**WHERE THE LAW IS ENFORCED.** `zhao_vram_arbiter.burst_words` clamps every
offered burst to the aligned block tail (`blk_tail = 8 - col[2:0]`), so no burst
this controller is ever handed can cross the wrap — **whatever address the
client supplies**. `pend_addr[k]` still takes the client's byte address
verbatim: the arbiter aligns the BURST, not the REQUEST.

**WHAT A CLIENT OWES.** Nothing, for correctness. A request whose start address
is a multiple of 16 completes in one burst fewer (a 64-byte request is four
bursts rather than five), so alignment is worth having and is a **performance**
property. `burst_unaligned_o` on the blocks that carry it counts the cost, not a
fault.

**THE SIMULATION MODEL MATCHES THE PART.** `sim/models/zhao_sdram_model.sv`
wraps in the block (`bl8_col`). It used to increment the column linearly, so it
read **better than the silicon** and no test in this tree could fail on a
breach; R243 ordered that repaired FIRST, before the arbiter, so the bug class
would be observable rather than merely fixed.

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
