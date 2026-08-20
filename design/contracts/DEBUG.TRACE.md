# Contract — DEBUG.TRACE (Trace ring and source IDs)

> Ledger: `design/blocks.yml` · owner ZH-074 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Selectable trace ring (charter §20.6) with source-ID propagation into the HPS trace arena.

Exclusions: no counters, no CRC. Source-ID scheme per spec/capture_format.md §5: every trace event carries the propagated source ID; the ring drains into the HPS trace arena via MEM.HPS.BRIDGE. Trace selection is a debug command; the ring is bounded and drops-oldest with a drop count (never silently stops).

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger —
the same domain CMD.DECODER presents records in, so no crossing lives here.
Reset clears the ring occupancy, the drop counter and the arming mask. The ring
STORAGE is not reset: it is a memory, and resetting a memory is what stops it
inferring as one (the lesson from TEXTURE.CACHE, CMD.DMA and the scanout line
buffer, now collected in `fpga/rtl/common/zhao_dc_sdp_ram.sv`).

## Input and output packet layouts

In, from CMD.DECODER's record port, ready/valid:

| field | width | meaning |
|---|---|---|
| `rec_valid_i` `rec_ready_o` | 1 | one decoded record |
| `rec_opcode_i` | 16 | |
| `rec_bytes_i` | 16 | |
| `rec_source_id_i` | 32 | `{kind:4, module:12, index:16}` per capture_format §5 |
| `rec_index_i` | 32 | 0-based position in the stream |

Plus `arm_i` (7), one bit per charter §20.6 stage.

Out, ready/valid, one TRACE event:

**The event layout is RATIFIED and this block does not get to invent one.**
`spec/capture_format.md` chunk `0x000A`, 32 bytes:

```
u32 tile; u32 primitive; u32 pixel; u8 stage; u8 rsv[3];
u32 expected_fx; u32 actual_fx; u32 source_id; u32 command_seq
```

Presented as `evt_valid_o` / `evt_ready_i` with the fields broken out, and
`evt_dropped_o` (32) counting what the ring could not hold.

## Backpressure rules

Backpressure: `ready_valid`.

## Memory ownership

A bounded event ring and nothing else. No VRAM port, no HPS port: writing
events into the trace arena is MEM.HPS.BRIDGE's job downstream, which is why
this block's output is a stream rather than an address.

Depth is a parameter, defaulting to 64 events (2 KiB). Held in
`zhao_dc_sdp_ram` so it infers as block RAM rather than becoming the fifth
instance of this project's recurring flip-flop-array defect.

## Q formats and rounding

**None.** Every field is an integer wire quantity. `expected_fx` and
`actual_fx` carry fx16 raw words for raster-stage divergences, but this block
only ever copies them — it performs no fixed-point arithmetic, so
`spec/qformats.md` does not apply. Stated rather than left blank so nobody hunts
for a rounding law that was never needed.

## Latency (fixed or variable)

Latency: `variable`.

## Target throughput

Target throughput: 1 trace event per clock.

## Overflow and malformed-input behaviour

**A FULL RING DROPS, IT DOES NOT STALL, and that is the load-bearing choice.**

A trace ring exists to observe a machine without changing it. A ring that
back-pressured CMD.DECODER would make the act of tracing alter the timing being
traced — and worse, would alter it only sometimes, which is the precise way to
destroy the determinism the whole capture system rests on. So when the ring is
full the event is discarded and `evt_dropped_o` counts it.

REJECTED: back-pressuring the decoder. It preserves every event and silently
trades away replay determinism, which is a far more valuable property than any
individual trace record.

A stage that is not armed produces no event at all — not a suppressed one — so
arming costs nothing when off. An `arm_i` bit above 6 has no stage and is
ignored; `stage` values 0..6 are the only legal ones.

## Counters and traces

Counters: `commands`. Source IDs: propagated.

## Scalar reference function

`zref::trace::Ring` in `reference/include/zref/zref_trace.hpp`.

The ledger declared `zref::DebugTrace`, the NINTH phantom `reference_model`
found in this tree, and unlike CMD.DECODER's there was no shipped law hiding
under another name — so this one is a real reference rather than a view.

It records the three questions the spec leaves open, each with its rejected
alternative: the `stage` byte values (the charter names seven sources and
numbers none), arming as a MASK rather than a selector (so two stages can be
correlated in one capture, which is the reason a ring exists), and zeroing the
raster fields of a decoder-stage event (because zero is checkable and undefined
is how a trace format rots).

## Directed tests

Planned: `tests/debug/debug_trace_directed.cpp`.

## Randomized differential tests

`tests/debug/debug_trace_directed.cpp --random N` (500 in the fast lane).

**Property-based, not differential**, and the distinction is stated rather than
blurred: there is one implementation today, so there is nothing to diff against.
What random input can still establish are the invariants the reference must hold
whatever it is fed — arming gates emission completely, occupancy never exceeds
depth, and every offered record is either stored or counted as dropped.

That last property is the one worth having. A ring that loses events without
counting them is indistinguishable from a working ring right up until an
investigation depends on it.

When the RTL lands this same lane becomes the differential, which is why it
lives in the binary the ledger already names.

## Formal properties

None planned for this block.

## Synthesis / resource ceiling

Budget group: `command_debug` (§25). Per-block percentages unfrozen until Phase 0 (V5 gate).

## Integration capture cases

Composition with CMD.DECODER is the point: this block's only producer is that
block's record port, which was built in the same wave. Composing the two is what
would show a field mismatch that neither block's isolated tests can see — the
GEOM.BINNER precedent, where composing with the real rasterizer immediately
exposed a tile-index-versus-pixel error.

Not yet built, and named here so it is not quietly skipped.

## Notes

Contract filled (Phase-1-active). Source-ID scheme per capture_format.md §5.
