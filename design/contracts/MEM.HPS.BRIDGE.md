# Contract — MEM.HPS.BRIDGE (HPS-DDR burst bridge)

> Ledger: `design/blocks.yml` · owner ZH-020 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

The FPGA-side client of the framework's HPS-DDR port: a FUNCTIONAL burst bridge (generic request/response core) carrying frame-packet ring traffic, PCM ring reads, blit sources and the trace arena. In Verilator the harness C++ IS the HPS and answers bursts deterministically (plan D10). Law: `spec/memory_rules.md` §3-§4.

Exclusions: no framework-AXI adapter RTL in Phase 2 (the framework adapter is the hardware-lane seam — this contract is the seam record), no VRAM access (that is the SDRAM side), no ownership policy over ring slots (descriptor law is spec/memory_rules.md §4; CMD.SCHEDULER owns FRAME_RING transitions).

## Clock and reset semantics

`gpu_clk` domain, synchronous active-low `rst_n`; the physical HPS interface is asynchronous (framework clock) — the documented `async_bridge: true` crossing lives at the hardware-lane adapter seam, modelled in sim as the fixed harness latency profile (16 gpu cycles request→first beat, 1 beat/cycle after). Reset: no bursts in flight, client ports idle, credit state re-armed.

## Input and output packet layouts

Client side: `zhao_hps_burst_req_t {valid, write, addr[31:0], len[6:0] (1..64 B), client}` with data beats; response `zhao_hps_burst_rsp_t {beat_valid, data[63:0], last, err}`. Bursts are 64-B aligned. Ring descriptors (FRAME_RING / PCM_RING) are 32-B structures read/written as bursts — layouts frozen in spec/memory_rules.md §4.

## Backpressure rules

Credit/one-burst-per-client: one burst in flight per client port; the response stream is ready/valid. The HPS never applies flow control beyond the response latency profile in sim.

## Memory ownership

HPS DDR is HPS-owned; the FPGA reads rings/arenas and writes ONLY the words the descriptor law grants: the `state` word of a FRAME_RING descriptor it owns (READY→FPGA_RUNNING→DONE→FREE), the `fpga_read_ptr` of PCM_RING, and designated trace-arena extents. No shared mutable structure ever has two writers (charter law; formalizable as the no-two-owners property family).

## Q formats and rounding

None (addresses, lengths, counters).

## Latency (fixed or variable)

Variable: 16 gpu cycles to first beat + 1 cycle/beat on the sim profile (recorded in captures; board profile is ZH-004/ZH-020 data, hardware lane).

## Target throughput

1 burst in flight per client; ≥ 1 beat per gpu cycle once streaming.

## Overflow and malformed-input behaviour

Burst length 0 or > 64, or a misaligned address, is a protocol violation: rejected at the port (`err`), counted, nothing issued — never a wild DRAM access. An `err` response fails the requesting client's transaction safely (the client's own contract defines recovery; e.g. CMD.DMA drops the packet with deadline_faults).

## Counters and traces

`hps_ddr_bytes_by_client` — per-client payload bytes both directions, shadow-latched at frame_tick. Trace: burst ledger (client, addr, len, direction) to the harness.

## Scalar reference function

`zref::HpsBridge` — burst oracle under the sim latency profile: exact beat times and bytes-by-client accounting for a request stream.

## Directed tests

`tests/memory/hps_bridge_directed.cpp` — read/write bursts incl. multi-beat; latency profile exact; malformed burst rejected with nothing issued; descriptor state-word write path.

## Randomized differential tests

`tests/memory/hps_bridge_directed.cpp (burst-bookkeeping oracle covers it)` — PCG burst streams from all client classes vs `zref::HpsBridge` (beat-exact + counters).

## Formal properties

No standalone SBY in Phase 2 (the burst core is simple combinational scheduling; the ownership laws live in the CMD slot-FSM and guard properties). Revisit with the AXI adapter (hardware lane).

## Synthesis / resource ceiling

Budget group `platform` (§25 14% ceiling). Buffers sized by max burst (64 B × clients).

## Integration capture cases

`captures/golden/wave2/duo_markers.zcap` — `hps_ddr_bytes_by_client` per frame (blit bytes + ring reads exactly accounted); the harness-as-HPS serves all 600 frames deterministically (run-twice-assert-identical).

## Notes

D10: the functional core is lane-portable; only the adapter seam is hardware-specific. Command/particle/audio/trace traffic is accounted per client (§25) at THIS block, not in the clients.
