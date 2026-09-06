# Contract — MEM.VRAM.ARBITER (VRAM arbiter)

> Ledger: `design/blocks.yml` · owner ZH-026 · phase 2 · maturity RTL_VERIFIED

## Purpose and exclusions

Guaranteed-client-liveness arbiter between fabric VRAM clients (through MEM.GUARD) and the SDRAM controller: scanout strict priority, other guaranteed clients round-robin, best-effort class reserved (empty Phase 2), credit-based at the SDRAM edge, ready/valid at client ports. Law: `spec/memory_rules.md` §2 (D3).

Exclusions: no region/ownership checking (MEM.GUARD filters first), no DRAM timing (MEM.SDRAM), no HPS traffic (MEM.HPS.BRIDGE). Leaf (serves MEM.GUARD requests).

## Clock and reset semantics

`gpu_clk` domain, synchronous active-low `rst_n`. Reset: all grants idle, credits re-armed to their initial issue, RR pointer to client 0; reset reaches idle (SYS.RESET law).

## Input and output packet layouts

Client side: `zhao_guard_rsp_t`-relayed request `{valid, write, client, addr, len}` per client; grant side `{grant, credits}`. SDRAM side: the credit request port of MEM.SDRAM (burst ≤ 8). Client ids are the frozen `zhao_client_e` (scanout, blit_dma, engine0, engine1, debug).

## Backpressure rules

Credit-based at the SDRAM edge (a request consumes credit; credits reissue on retirement); ready/valid at client ports. Scanout preempts at BURST BOUNDARIES only (never mid-burst). One outstanding request per client.

## Memory ownership

None of its own — it grants access; MEM.GUARD owns region law, the controller owns the DRAM. Per-client accepted bytes are counted here.

## Q formats and rounding

Unsigned integer byte/word counts only.

## Latency (fixed or variable)

Variable, formally bounded: every guaranteed client's first burst served within the proven-tight bounds — scanout 34 refresh-free / 47 operational, RR class 52 refresh-free / 65 operational sdram cycles (derivation `spec/memory_rules.md` §2.1; see "Formal properties" below). The old formula `B = G·MAX_BURST + REFRESH_OVERHEAD = 40` was disproven 2026-08-16 — this line quoted it for a further wave after the correction below landed, which is exactly the prose-drift failure rules V17/V20 now police.

## Target throughput

1 accepted request per clock at the client ports; SDRAM-side beat scheduling per the controller profile.

## Overflow and malformed-input behaviour

An uncredited or out-of-region request cannot arrive (guard + credit protocol); a simulated protocol violation trips an assertion (no silent path). No request is ever dropped or reordered within a client; starvation of a guaranteed client is impossible by the formal bound.

## Counters and traces

`vram_bytes_by_client` (per-client accepted payload bytes) — shadow-latched at frame_tick; `deadline_faults` participates via the bandwidth-budget test (zero starvation under the Duo worst case). Trace: the complete grant order to the harness (differential key).

## Scalar reference function

`zref::VramArbiter` — grant oracle: given per-client request timelines, credits and the controller retirement stream, the exact grant order and per-client byte counts.

## Directed tests

`tests/memory/vram_arbiter_directed.cpp` — scanout preempts at a burst boundary; RR fairness among guaranteed clients; refresh steal visible; bandwidth budget (Duo worst line + blit ⇒ zero starvation, spec/memory_rules.md §2).

## Randomized differential tests

`tests/memory/mem_random.cpp` — three-way random vs `zref::VramArbiter` + `zref::SdramController` with grant-order equality and 64 KiB shadow-memory integrity.

## Formal properties

`tests/formal/mem_vram_arbiter_liveness.sby` — the B bound: every guaranteed client granted within B cycles (charter §20.4 "arbiters eventually service guaranteed clients").

**B was corrected on 2026-08-16.** The wave-2 constant B = 40 had never been elaborated and is false; the proof fails at 40. Re-derived and re-proven per `spec/memory_rules.md` §2.1 (ratified: `runs/CLAUDE-RUNS/RUN-20260814-2154-wave2-phase2-console-shell/RATIFICATION-arbiter-liveness-bound.md`):

| client | B refresh-free (proven tight) | B operational (+13 refresh steal) |
|---|---|---|
| scanout (strict priority) | 34 | **47** |
| RR class (blit, engine) | 52 | **65** |

Both refresh-free bounds are proven in BOTH directions: the `bmc` task passes at them and `bmc_tight_rr` / `bmc_tight_scanout` FAIL at bound−1 (51 / 33), so they are the exact worst cases. The bound is a steady-state bound, measured from an acceptance at or after `init_done` — power-on is not covered by B.

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). Small — mux + counters.

## Integration capture cases

`captures/golden/wave2/duo_markers.zcap` — `vram_bytes_by_client` snapshots per frame (blit + scanout byte accounting law: 2 × canvas reads + 1 × canvas blit per frame at steady state).

## Notes

Best-effort class ports exist but are unwired in Phase 2 (plan §6 deferred) — the class reservation is part of this contract, not an afterthought.

**Seven client ports since 2026-09-06 (ruling T3).** The array index IS the client id — `ctrl_req.client = zhao_client_e'(offer_client)` — so slot identity and client identity are one fact, and adding `ZHAO_CLIENT_TERRAIN_BUILD = 6` means an array of 0..6 with a HOLE at 5 rather than packing terrain into the next free slot. Client 5 is the id T3 reserves and forbids spending; its port is dead by construction (no arbitration arm names it) and `port_grant[5]` is forced low, so a request there is refused rather than latched into a slot that can never be served — an accepted-but-unservable request reads at the requester as a hang. TERRAIN_BUILD is arbitrated **below DEBUG**, background class, never promoted for lateness. `mem_vram_arbiter_liveness` was re-run with seven ports: `bmc` PASS at RR 52 / scanout 34, both `expect fail` tightness tasks still FAIL at bound−1, `cover` PASS — the bounds are unchanged and still exact.
