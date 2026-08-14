# Contract — DEBUG.COUNTERS (Counter aggregation)

> Ledger: `design/blocks.yml` · owner ZH-072 · phase 2 · maturity SPECIFIED

## Purpose and exclusions

Aggregate the §25 mandatory counter set from the distributed per-block shadows through a vblank read-mux window, and expose it to debug tooling / the .zcap COUNTERS section. Law: `spec/counters.md` (D9) — counter_id = catalog index; distributed counters, frame-synchronous snapshot, NO global event bus.

Exclusions: no trace (DEBUG.TRACE), no CRCs (DEBUG.CRC), no counter OWNERSHIP (each block owns and increments its counters; this block only reads shadows), no catalog edits (adding a counter is a ledger edit, rule V15).

## Clock and reset semantics

`gpu_clk` domain, synchronous active-low `rst_n`; per-block shadow crossings are gray-coded toggles (SYS.CDC law) — the shadows are stable for a whole frame by construction (latched at frame_tick). Reset: read-mux idle, snapshot buffer zero, no reads in flight.

## Input and output packet layouts

Input: per-block shadow sets `{counter_id: u16, value: u64}` behind their CDC toggles, addressed by counter_id. Output: the read-mux window — one `(counter_id, u64)` pair per beat, ready/valid, addressed by counter_id (u16), plus the composed .zcap COUNTERS section body `u32 count + count × {u16 counter_id; u16 rsv; u64 expected_value}` (ascending counter_id — capture_format.md §4.2).

## Backpressure rules

`ready_valid` on the read port; the window is vblank-scheduled so the consumer always keeps up (a stalled read retries next vblank — shadows are stable).

## Memory ownership

None (registers only); the capture writer owns bytes.

## Q formats and rounding

u64 saturating values (spec/counters.md §4); `max_tile_list_depth` is read-clear high-water. No rounding.

## Latency (fixed or variable)

Variable: one read beat per clock during the window; whole-catalog read bounded by catalog size × 1.

## Target throughput

1 counter update (read beat) per clock.

## Overflow and malformed-input behaviour

Counters saturate at 0xFFFF_FFFF_FFFF_FFFF, never wrap (charter §29-11 — record, remain correct). An out-of-catalog counter_id read (protocol violation in sim) asserts; there is no silent fallback value. A counter with no live owner reads 0 (legal, spec/counters.md §5).

## Counters and traces

The composition itself is traceable: the harness compares every composed snapshot against `zref::DebugCounters` per frame (the golden capture COUNTERS sections are the evidence).

## Scalar reference function

`zref::DebugCounters` — snapshot oracle: given per-block event streams and the tick schedule, the exact shadow set and composed section bytes per frame.

## Directed tests

`tests/debug/debug_counters_directed.cpp` — injected event counts per block, forced tick, read-mux compare; saturation; high-water read-clear; ascending-id composition; read never disturbs live counters.

## Randomized differential tests

`tests/debug/debug_counters_random.cpp` — PCG event streams across blocks vs `zref::DebugCounters`, bit-exact sections.

## Formal properties

No standalone SBY: the laws (saturation, latch-at-tick) are enforced in the owning blocks' properties and the differential compare; the mux itself is combinational scheduling.

## Synthesis / resource ceiling

Budget group `command_debug` (§25 5% ceiling).

## Integration capture cases

`captures/golden/wave2/*` — every capture's COUNTERS section byte-compared against the ZRef-composed expectation; the Duo marker demo asserts the zero-set {deadline_faults, scanout_starvation_cycles, audio_underruns, input_sequence_gaps, rumble_frames_dropped} for all 600 frames.

## Notes

Counter set = the counter_catalog of design/blocks.yml (rules V12/V15). The catalog-index law makes every counter_id stable across captures: append-only, never renumbered.
