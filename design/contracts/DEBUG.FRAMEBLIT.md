# Contract — DEBUG.FRAMEBLIT (Debug frame blit)

> Ledger: `design/blocks.yml` · owner ZH-075 · phase 2 · maturity SPECIFIED
>
> Design: `reports/CMD.DMA_Redesign_Proposal.md`, Part 2. This block did not
> exist before that proposal; it is the blit path lifted out of CMD.DMA.

## Purpose and exclusions

Execute one `DebugFrameBlit` as a single-pass transaction: lease an invisible
framebuffer slot, stream the source from HPS DDR in 64-byte chunks through
guarded local-SDRAM writes, accumulate CRC-32C over the source, and publish the
slot READY only if every byte was written and the CRC matched.

**Why it is its own block.** `DebugFrameBlit` is a debug-umbrella command and is
never game-facing, yet as shipped it lived inside CMD.DMA and carried a
whole-canvas staging buffer — 245,760 bytes, 1,966,080 bits. That buffer is what
pushed the command front end, and therefore the whole shell, out of a fittable
size. A debug-only transport path must not be able to do that, and production
builds can eventually omit this block entirely.

**Not here:** the slot manager's `FREE → WRITING → READY → DISPLAYED → FREE`
state machine, which lives at the shell / frame-control seam. This block consumes
a lease and reports `publish` or `release`; it does not decide which slot is
displayed.

## Clock and reset semantics

Single `clk`, asynchronous active-low `rst_n`, `gpu` domain. Reset returns the
transaction to idle and clears both counters. **A reset mid-transaction publishes
nothing** — `blit_publish_o` is a registered pulse that only the `B_PUBLISH`
state raises.

## Input and output packet layouts

**Request** (ready/valid, from CMD.SCHEDULER's dispatch): `req_dst_slot_i` (u8,
the frozen ABI field), `req_mode_i` (u8 ABI video mode), `req_src_i` (u32 HPS
address), `req_len_i` (u32), `req_crc_i` (u32 expected CRC-32C).

**Lease** (from the shell/frame-control seam): `fb_lease_valid_i`,
`fb_lease_slot_i`, `fb_lease_generation_i` (u16).

The two terminal events each carry the identity of the lease they belong to:
`release_valid_o` / `release_slot_o` / `release_generation_o`, and
`publish_valid_o` / `publish_slot_o` / `publish_generation_o`. A bare pulse
cannot say WHICH slot and WHICH generation it refers to, and after an ABA
re-grant that is exactly the question; the slot manager accepts an event only
when the slot is WRITING and the generation matches.

**Retirement** (from MEM.VRAM.ARBITER's BLIT client credit port):
`retire_words_i` (u8), 16-bit SDRAM words. This is the only source of truth for
"the write landed".

**HPS reader**: `zhao_hps_burst_req_t` / `zhao_hps_burst_rsp_t` plus
`hps_req_grant_i`, client `ZHAO_CLIENT_BLIT_DMA`, 64 bytes per burst. The
request is HELD stable until the grant; the bridge has one request port and
CMD.DMA will share it.

**Guarded write**: `zhao_guard_req_t` / `zhao_guard_rsp_t` plus a real data
handshake — `guard_wdata_o`, `guard_wvalid_o`, **`guard_wready_i`**,
`guard_wlast_o`.

**Completion**: `done_o` (one pulse per request) and `status_o`, where 0 is the
only success.

## Backpressure rules

Ready/valid on the request port and on the write-data port.

**While `guard_wvalid_o && !guard_wready_i`, the data and the `last` marker are
HELD stable.** The old DMA emitted write data with no ready at all and caught
overflow afterwards with a sticky error; a beat that moves under a stalled
consumer is a corrupted pixel nobody can trace back.

## Memory ownership

**One 64-byte chunk buffer**: `logic [63:0] chunk [0:7]`, 512 bits.

That number is the point of the block. The old design's 1,966,080-bit staging
buffer existed only to satisfy the old atomicity law; the amendment below lets
the framebuffer slot itself be the transaction buffer, which is what double
buffering is for.

A ping-pong second chunk would let the next HPS burst fill while the previous
waits for local-SDRAM service, at 1,024 bits. Not implemented: the old design
also did the read and the write as separate phases, so single-buffering should
not fundamentally worsen the transaction cost, and the measurement to justify the
second buffer does not exist yet.

## Q formats and rounding

**None.** No fixed-point value passes through this block. The only arithmetic is
CRC-32C accumulation and byte-offset bookkeeping.

The CRC is the reflected Castagnoli polynomial `0x82F63B78`, initial
`0xFFFFFFFF`, final XOR `0xFFFFFFFF` — bit for bit what `zhao_abi::zhao_crc32c`
computes, which is the one generated implementation and the oracle the test
compares against.

## Latency (fixed or variable)

Variable, and dominated by the source: one 64-byte burst plus eight guarded write
beats per chunk, repeated `canvas_bytes(mode) / 64` times.

## Target throughput

One guarded 64-bit beat per clock when neither the bridge nor the guard stalls.

This is a debug path. It is not on any frame budget and no rate is promised
beyond "it does not stall the shell", which the ready/valid handshakes secure.

## Overflow and malformed-input behaviour

**THE AMENDED ATOMICITY LAW.**

    OLD: no byte is written to VRAM before CRC verification.
    NEW: no framebuffer slot becomes visible or READY before every byte has been
         written, all writes have retired, and the CRC matches.

**"Retired" means the memory system said so.** The first implementation of this
block advanced its own retirement counter when a chunk was handed downstream,
which made the law above false by construction: a slot could be published while
its data was still in the write FIFO, the arbiter, or the controller's pending
burst. Retirement now arrives from outside on `retire_words_i` and is
accumulated in every state, because credits return while the next chunk is
being read.

The publication condition is `retired == len`, exactly as the review specifies —
not `>=`. That is deliberate and it carries an assumption worth naming: the
arbiter's credits for this client must sum to exactly the bytes it was asked to
write. If a future arbiter ever credited at a coarser granularity than the
request, this block would wait forever rather than publish early. Waiting
forever is the safe direction and is visible; publishing early is neither.

The amendment is sound because raw writes into an inactive, uncommitted slot are
not visible: the shell only toggles slot readiness when `blit_status == 0`, and
FRAMECTL only swaps to a committed READY slot. The externally meaningful commit
point was never the first write.

**A dirty unpublished slot is an accepted outcome**, not a compromise. On any
failure the slot is released FREE with whatever bytes happened to land in it. The
only thing that must never happen is publishing it.

**But it is released only after it drains, and only if it was ever owned.** Two
corrections from the integration review:

- A slot released while writes are still in flight can be leased to a new
  transaction and then overwritten by the dead one's bytes. Failure therefore
  goes `B_ABORT_STOP` -> `B_ABORT_DRAIN` -> `B_RELEASE`, and nothing touches the
  lease until `retired == issued`.
- A bad length, a missing lease and a slot mismatch all fail BEFORE ownership is
  acquired, so releasing on them would free somebody else's lease. `owns_lease`
  is set only once every validation passes, and release is gated on it. An error
  completion is a status, not an ownership transition.

Every failure is distinguishable, because "the blit did not appear" is otherwise
unactionable:

| `status_o` | meaning |
| --- | --- |
| 0 | published |
| 1 | `len != canvas_bytes(mode)` — rejected before a lease is even examined |
| 2 | no lease at start |
| 3 | `dst_slot` does not match the leased slot |
| 4 | the lease lapsed or was re-granted mid-transaction |
| 5 | HPS bridge error |
| 6 | the guard refused a write |
| 7 | every byte written, CRC wrong |

**THE LEASE, and why the generation is part of it.** `dst_slot` from the frozen
ABI is no longer trusted merely because it is 0 or 1 — it must match the slot the
shell leased. The lease must then hold for the WHOLE transaction, checked every
cycle rather than once per chunk.

The generation closes an ABA hole: a lease that drops and is re-granted for the
SAME slot mid-transaction looks identical to one that never lapsed, while the
bytes already written belong to somebody else's lease. The generation is latched
at accept and compared every cycle, so a re-grant is a lease loss.

**The check happens at the publication edge itself.** Checking the lease in one
state and pulsing publish in a later one leaves a window in which it can lapse
in between; the final check and the pulse are the same state on the same edge.

**And losing it stops side effects immediately.** `abort_pending` latches the
first failure; no new HPS request, guard request or write beat starts after it.
`guard_req_o.valid` is additionally gated on the LIVE lease, so no request is
even asserted on a cycle the slot is not ours. A burst already granted is
drained without being stored or folded into the CRC.

**Why not two-pass DDR.** Reading the source twice — once to verify, once to
commit — preserves "zero guard writes on reject" and is unsound here: the pixel
arena is a raw HPS address with no descriptor or ownership state, so the HPS can
mutate it between the passes. Pass 1 verifies bytes A, pass 2 commits bytes B,
and the CRC that was checked describes data that is no longer there. Making it
sound would first require a sealed pixel-arena descriptor or lease that forbids
HPS writes between the passes. Single-pass has no such hole: if the source
changes mid-read, the stream fails its CRC and the dirty inactive slot is never
published.

## Counters and traces

`blits_published_o` and `blits_rejected_o` (u32, saturating). Both are exposed
because a blit that quietly never appears is otherwise indistinguishable from one
that was never issued.

The ledger's `hps_ddr_bytes_by_client` lane is fed by the shared HPS reader at
the seam, not counted separately here.

No trace hookup yet.

## Scalar reference function

`zref::debug::run_blit` — `reference/include/zref/zref_frameblit.hpp`.

A NEW reference for a NEW block: the proposal's Part 2 written as an executable
law so the RTL has something to be a differential against. The CRC arithmetic
DELEGATES to `zhao_abi::zhao_crc32c`, the one generated implementation, exactly
as `zref::cmd2::Crc32c` does — nothing here re-implements a polynomial.

## Directed tests

`tests/debug/debug_frameblit_directed.cpp` — 97 checks. The harness IS the HPS,
the guard and the memory system, and injects each failure at a chosen byte
offset.

**The harness used to say yes to everything**, which is how the block passed 43
checks while being wrong in six ways. Its guard now validates the request
ADDRESS against the leased slot's base and can make a request wait; its bridge
requires a real grant; and its memory returns retirement credits that can be
withheld or frozen outright.

Mutation sweep: 13 mutations, one per defect the review named. **11 caught, 2
recorded equivalent** (`guard_request_after_loss`, masked by the combinational
gate whose own removal IS caught; and `publish_generation_live`, which cannot
differ because publication already requires the two generations to be equal),
0 discarded.

Sections: the happy path; a wrong CRC; the three lease refusals; a lease that
lapses mid-blit; **a lease that lapses for three cycles and recovers**; the ABA
re-grant; a guard denial; a bridge error; write backpressure at four stall
periods; and the proposal's own composition test — keep slot 0 displayed, stream
a deliberately bad blit into leased slot 1, and prove slot 0 is never touched.

The transient-drop section exists because a mutation removing the per-cycle lease
watch passed everything else: the per-chunk check at each 64-byte boundary masked
it. A lease that drops and recovers between boundaries is invisible to that
check, and during those cycles the slot was not ours.

## Randomized differential tests

None yet, and the reason is worth stating rather than leaving as an omission: the
interesting axis here is not operand values but the CROSS PRODUCT of failure
point and transaction phase, and the directed cases already place each failure at
a chosen offset. A random lane would mostly re-run the happy path.

The gap worth filling later is randomised STALL patterns on both the bridge and
the guard simultaneously, which is where a handshake bug would hide.

## Formal properties

`tests/formal/debug_frameblit_safety.sby`, lane `formal_debug_frameblit_safety`,
recorded green in `design/formal_runs.yml`. **27 assertions to depth 44**
(btormc), **8 covers all reached** — `c_publish` at k = 18, `c_crc_fail` at
k = 21.

The covers are what make it mean anything. Every assertion is an implication, so
a model that cannot publish satisfies the publish properties while proving
nothing — the shape that made MEM.GUARD's whole lane and CMD.DMA's assertion (b)
vacuous. A publication is reachable here ONLY because of the FORMAL-ONLY
`FORMAL_CANVAS_BYTES = 64` parameter: at the real 184,320-byte canvas a
transaction is 2,880 chunks and over 46,000 cycles, and no bounded model gets
near it.

**Scope (V19): a single-chunk transaction.** The multi-chunk loop is not in the
cone; `a_scope_single_chunk` fires if the canvas is raised, so a wider proof
needs a re-justified depth rather than a quietly larger number. The ctest lane
runs the real canvas.

Two things surfaced while writing it:

- `a_pub_nofail` failed at k = 2 until the model was made to start from a REAL
  reset. Without that constraint every register begins unconstrained and the
  first counterexample is a fabricated state that publishes out of nowhere —
  which says nothing about the design.
- The state-level abort check in `B_GUARD_REQUEST` is provably redundant given
  the combinational gate on `guard_req_o.valid`, which is why its mutation is
  recorded as equivalent rather than as a hole.

The proposal named the set and they are the right ones:

- `publish → CRC matched`
- `publish → issued_bytes == retired_bytes == expected_len`
- `publish → the lease was continuously valid`
- `guard write → the leased slot`, and `→ the slot is not displayed`
- `CRC failure → no publish`; `bridge/guard failure → no publish`
- `reset during transaction → no publish`
- each destination byte written exactly once on success
- `wvalid && !wready → data and last remain stable`

Every one is a safety property over a small state machine, so all are within
reach of a bounded proof. The first three are the ones that matter, and all
three are now proven.

## Synthesis / resource ceiling

Not yet fitted, and **the whole point of the block is a resource claim**, so this
section is the one to close first.

By construction: 512 bits of chunk buffer, a byte-serial CRC-32C lane, and a
twelve-state controller. Against the old path's 1,966,080-bit buffer, which is
what the composed fit named when it failed.

The composed synthesis run that motivated this redesign is in
`reports/composed/`; re-running it with the blit split out is the acceptance
evidence for this block, and it has not been run.

## Integration capture cases

None yet. The first is the proposal's composition test on real hardware paths:
slot 0 displayed, a bad blit into leased slot 1, and slot 0's displayed CRC
unchanged. It needs the slot-manager seam, which is not built.
