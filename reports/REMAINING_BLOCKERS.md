# What is actually blocking every remaining block

**Date:** 2026-08-21
**Method:** each of the 24 remaining `SPECIFIED`, non-deferred, non-hardware-
blocked RTL blocks was trial-advanced to `REFERENCE_COMPLETE` in a scratch copy
of `design/blocks.yml`, the ledger check run, the errors recorded, and the file
restored. Dashboard-staleness errors are filtered out as noise.

## The headline

**There are no cheap advances left.** `TERRAIN.BAKE` was the last block that was
finished but merely unrecorded — it advanced today with no work beyond
regenerating a diagram. `SURFACE.SHEET` and `TERRAIN.VELOCITY` each needed one
real test written. Everything after them needs the block itself built.

Today's advances went: `GEOM.SKIN`, `DEBUG.TRACE`, `SURFACE.SHEET`,
`TERRAIN.BAKE`, `TERRAIN.VELOCITY`, plus both halves of `GEOM.POSE`. That
exhausted the backlog of *built-but-unrecorded* work.

## The three shapes, and how many of each

### A. Greenfield — 16 blocks

`FIELD.PROGCACHE`, `GEOM.MESHFETCH`, `GEOM.PROJECT`, `GEOM.VDECODE`,
`GEOM.WCACHE`, `GEOM.LOOM`, `GEOM.WARP`, `MEASURE.HISTOGRAM`, all seven `PART.*`,
`FORGE.PRIM`, `TWOD.PLANE`, `TWOD.SPRITE`, `POST.GATHER`, `POST.COMPOSITE`.

Identical error shape every time:

- **V6** — both declared test paths do not exist;
- **V17** — the `reference_model` is a phantom, *and* the contract names no
  `zref::` symbol under "Scalar reference function".

So each one needs, in order: a reference (or a forward to the real law), the
contract's reference section, RTL, a differential, a random lane, a mutation
sweep. That is the full DEBUG.TRACE treatment, sixteen times.

**One of them is much cheaper than the rest.** `GEOM.PROJECT` cites
`zref::GeomProject`, which is a phantom — but `zref::render::project_vertex` is
real, is what `TERRAIN.PROJECT` already uses, and `TERRAIN.PROJECT` is already
`UNIT_VERIFIED` with working RTL. The ledger even records why the two are
separate: *"Kept separate from GEOM.PROJECT by architect ruling (1.D): merging
later is a trivial edit."* This is a kind-1 phantom with a proven neighbour.

### B. Blocked on the Field IR sequencers — 5 blocks

`FIELD.SEQ.EARTH`, `FIELD.SEQ.FLOW`, `FIELD.SEQ.FORMATION`, `FIELD.SEQ.STAMP`,
and `TERRAIN.PATCH` downstream of them.

All four sequencers report the **same three** V10 blockers:
`FIELD.MOV`, `FIELD.ADD`, `FIELD.SUB` — the Field IR's base arithmetic ops, none
of which has a differential test. They are shared, so writing those three unlocks
the op layer for all four at once.

But the op tests are differentials, and a differential needs RTL to differ
against. No `FIELD.SEQ.*` block has any. **So the three op tests are not the
blocker; the sequencer RTL is**, and the op tests come with the first sequencer
that exists.

`TERRAIN.PATCH` is a different case, documented in
`reports/PHANTOM_REFERENCES.md`: its three remaining op blockers are sinks that
belong to `FIELD.SEQ.EARTH`, and it is blocked on that block being built rather
than on anything of its own.

### C. Registered-but-intentionally-late — 1 block

`MEASURE.HISTOGRAM`. The ledger's own note says it: *"Charter §12 calls this
Version 2: registered now, built late."* Its blockers are real but so is the
decision not to build it yet.

## What this means for order

The wave order does not change, but the *cost* per block just became uniform and
much higher, and it is worth saying plainly: from here, "get through the waves"
means writing sixteen blocks, not clearing a backlog.

The cheapest next steps, in order:

1. **`GEOM.PROJECT`** — kind-1 phantom, real oracle, and a working sibling to
   pattern-match against.
2. **`FIELD.PROGCACHE`** — a cache, and the pose cache built today is a close
   structural analogue (tags, residency, counters, delegated storage).
3. **The first `FIELD.SEQ.*` sequencer** — expensive, but it unblocks four
   blocks plus `TERRAIN.PATCH`, and brings the three shared op tests with it.

The `PART.*` family (seven blocks) and the compositor family (four) are each a
coherent chunk that would be better done together than interleaved, since they
share references.

---

## Addendum: the Field IR sequencers are constrained, not merely unbuilt

`spec/form/field-ir.md` §1 carries a grep-audit law (charter §29-6) that changes
what "build FIELD.SEQ.*" is allowed to mean:

> Field IR *op semantics* exist in exactly two places — the C++ generic
> interpreter (`zfield::interpret`) and the TS interpreter … There is and shall
> be no third implementation: no hand-written per-program evaluator, no "faster"
> fused C++ variant, **no RTL-side re-derivation ahead of the profile engine
> (which will consume the same serialized bytes)**. A reviewer greps for the
> op-name switch outside those two files and must find none.

Read carefully, this is a design constraint rather than a prohibition. RTL is
foreseen — the parenthesis names "the profile engine" and says it consumes the
same serialized bytes. What is forbidden is a sequencer that re-derives op
semantics: a per-program hardwired evaluator, or an RTL opcode switch written
from the spec by hand.

So the five sequencer blocks are not just expensive, they have a required shape:
**a byte-code engine that executes `.zprog` images**, differentially verified
against `zfield::interpret` on the committed `.zvec` corpus. Anything that reads
like a second implementation of the op table will fail the grep audit by
construction, however well it tests.

Two consequences worth planning around:

1. The three shared op blockers (`FIELD.MOV`, `FIELD.ADD`, `FIELD.SUB`) come with
   that engine and are differentials against the same interpreter — they are not
   separate work.
2. The engine is one block's worth of effort that unblocks five. That makes it
   better value than its size suggests, and it is the reason the "first
   `FIELD.SEQ.*` sequencer" sits third on the cheapest-next list above rather
   than last.

`FIELD.PROGCACHE` is clear of this constraint: it caches and validates programs
and never evaluates one. Its validation half is already law —
`zfield::decode` with thirteen named error classes — so only its cache policy
needs deciding.

---

## Addendum 2: the cheap blocks are gone, and what that leaves

Since the map above was written, five greenfield blocks were built and verified —
`GEOM.PROJECT`, `FIELD.PROGCACHE`, `PART.EXPAND`, `PART.SOFT`, plus both halves of
`GEOM.POSE`. Four of the five were **kind-1** phantoms: the law already existed
under another name and only had to be found, cited and pinned.

**A systematic scan says there are no more of those.** Every remaining
`reference_model` was checked against the reference tree for a law shipped under a
different name. The results:

| Block | Is the law already shipped? |
| --- | --- |
| `GEOM.VDECODE` | **No.** Meshlets hold plain `SkinVertex` — there is no compressed form anywhere, and the ledger says the format belongs to `SW.TOOLS.ASSET`: *"one spec, two ends"*. |
| `POST.GATHER`, `POST.COMPOSITE` | **No.** `zref_aux.hpp` says of the distortion map that *"the offset arithmetic belongs to whoever"* — it is explicitly unassigned. Bloom, flash and grading exist only in the star/sky path, which is a different block's law. |
| `TWOD.SPRITE`, `TWOD.PLANE` | **No.** `blit_pattern_8x8` is a form-marker blit, not a HUD sprite pipeline with descriptors, affine and CLUT paths. |
| `GEOM.LOOM`, `GEOM.WARP` | **No.** Transform-graph evaluation and Warp8 deformation are unimplemented in software as well. |
| `PART.SPAWN/STATE/UPDATE/COLLIDE` | **No.** `zref::render::Particle` is a draw-time snapshot the renderer is HANDED. Nothing simulates particles. |
| `PART.LADDER` | Partly. The seven rungs are charter §9 and the counter lanes are `zref::measure`, but the ledger says the thresholds are *"provisional until Phase-10 evidence"* — the numbers are explicitly not ratified. |

## So the remaining 37 split three ways, and only one is mine to do alone

**1. Needs a spec another block owns.** `GEOM.VDECODE` is the clear case: the
vertex compression format has two ends and the pack side is `SW.TOOLS.ASSET`'s.
Inventing one end unilaterally would create exactly the kind of unratified law
this project keeps catching. `PART.LADDER`'s thresholds are the same shape —
recorded as provisional pending evidence that does not exist yet.

**2. Needs the Field IR engine.** Five blocks, one engine, required shape already
documented in addendum 1. This is large but it is unambiguous work: a byte-code
engine over `.zprog`, differentially verified against `zfield::interpret` on the
committed `.zvec` corpus. **It is the single highest-value remaining item** and
nothing about it needs a decision from anyone.

**3. Needs behaviour decided.** The four particle-simulation blocks, the two
compositor blocks and the two 2D blocks have no law in software, no ratified spec
section, and no donor behaviour to extract. Each one means choosing how the game
behaves — how a particle spawns, ages and collides; what bloom looks like — and
then writing that choice down as the reference before any RTL. That is design
work, and the choices belong to the person whose game it is.

## The honest statement of scope

"Finish the full hardware" is not one more sitting's work. Group 2 is the next
substantial thing I can do without input. Group 3 is roughly a dozen blocks whose
*behaviour* has never been decided, and doing them well means deciding it
deliberately rather than having me invent it and record the invention as law.

---

## 2026-08-21 (RESOLVED, same day) — CMD.DMA now synthesises

> **`Quartus Prime Analysis & Synthesis was successful. 0 errors.`** 21:06
> elapsed. This block had never once been successfully processed: the census
> row is `failed:quartus_map` (16.2 GB elaboration) and at HEAD it was
> `timeout` at 4,838 s.
>
> **THE FIX WAS NOT THE REDESIGN THIS SECTION CALLED FOR.** The analysis below
> was right about the cause and wrong about the remedy, and the correction is
> worth more than the original entry.
>
> The loop is bounded at **192**. The reachable maximum is **64**:
> `fetched` is zeroed when the fetch is accepted, `M_HDR_REQ` issues exactly
> ONE burst, `burst_len` caps at 64 bytes, and `M_HDR_WAIT` adds 8 per beat and
> leaves on `last`. So iterations 64..191 had their `k < seed_end` guard false
> in **every reachable state** — 128 steps of unreachable logic that synthesis
> had to build a ~1,248-stage dependent chain for before discarding.
>
> Bounding the loop at 64 is **exactly equivalent**, and the diff is one
> number. No incremental CRC state machine was needed. The bound is now
> asserted in the formal cone rather than argued in prose, because it is the
> reason the loop is safe.
>
> Evidence: 43 directed + 139,113 random checks (1,000 frames of packets);
> mutation sweep 11 / 10 caught / 1 recorded equivalent / 0 discarded.
>
> **The lesson is about the diagnosis, not the bug.** "156 dependent CRC steps"
> was measured and true. "Therefore it needs an incremental CRC redesign" was
> inferred and false — nobody had asked how many of those steps could actually
> execute. A cone that large is worth a bound check before it is worth a
> rewrite.
>
> **SYNTHESIS IS FIXED; PLACEMENT IS NOT.** The same run then failed the
> FITTER at 2,839 s — a real failure, not a timeout (the limit is 3,000 s and
> the exit was non-zero). So this block now reaches two stages further than it
> ever has, and `failed:quartus_fit` is the new wall.
>
> | attempt | result |
> | --- | --- |
> | census (`96c0394`, with `blit_buf`) | `failed:quartus_map`, 16.2 GB |
> | HEAD after step 6 | `timeout`, 4,838 s |
> | HEAD + bounded CRC loop | **synthesis 0 errors**, then `failed:quartus_fit`, 2,839 s |
>
> **The cause is NOT yet established and is deliberately not recorded here as
> if it were.** The workspace is auto-deleted on success paths, so the fitter's
> own error was not captured; the next run keeps it.
>
> A PREDICTION, held as a prediction: `slot_buf` still has `blit_buf`'s defect
> in miniature — initialiser, async-reset write, combinational read — so it
> does not infer as RAM at 4,096 entries, and
> `assign pkt_byte_o = slot_buf[rd_off]` is a **4,096:1 byte mux**, on the order
> of 32,760 LUTs against a 41,910-ALM device.
>
> That is exactly the shape of reasoning that was wrong about the CRC loop an
> hour earlier: a true measurement, an inferred remedy, written down as
> required work. The fitter's error decides it, not this paragraph.
>
> ### THE FITTER'S ERROR, CAPTURED
>
> ```
> Error (170011): Design contains 95328 blocks of type combinational node.
>                 However, the device contains only 83820 blocks.
> Error (11802): Can't fit design in device.
> ```
>
> **This one block needs 114% of the whole device's combinational capacity.**
>
> The prediction above was directionally right and badly undersized: it named
> the read mux at roughly 32,760 LUTs, and the measurement is nearly three
> times that. The half it did not name is probably the larger one —
> `slot_buf[wr_off + 32'(i)] <= ...` writes eight bytes at a **variable index**
> into a 4,096-entry array, which is a 4,096-way write decoder on top of the
> 4,096:1 read mux. So: predicted the cause, missed the dominant term. Recorded
> that way rather than as a hit.
>
> ### The fix is already written down in the RTL, and was deferred
>
> `zhao_cmd_dma.sv` says of this array:
>
> > "Still NOT an M10K, and the contract says why: the write lives in an
> > async-reset process and the read is combinational, and an M10K has no reset
> > port and a registered read. Fixing that is a protocol change (the beat
> > stream needs a one-cycle read lead) and is deliberately not done here."
>
> That is the same defect `blit_buf` had, the same one `zhao_scanout_linebuf`
> was cured of by moving to `zhao_dc_sdp_ram` with a registered read, and the
> same one Quartus Error 276003 named on the composed shell. **Three memories,
> one defect, and this is the last of them.**
>
> **What it blocks:** the composed fit contains `CMD.DMA`, so Step 8 remains
> gated. Synthesis is no longer the obstacle; placement is, and the remedy is
> the known protocol change rather than anything new.
>
> **What is now known that was not:** the block synthesises, so the CRC cone
> was a real and separate problem, and the remaining cost is entirely
> `slot_buf`'s shape. That is a much smaller and better-specified piece of work
> than "CMD.DMA cannot be fitted".
>
> ### ATTEMPT 1: re-describe as 512 x 64 words. MEASURED WORSE. REVERTED.
>
> | shape | combinational nodes (device has 83,820) |
> | --- | ---: |
> | `logic [7:0] slot_buf [0:4095]` (shipped) | **95,328** |
> | `logic [63:0] slot_buf [0:511]` + byte accessor | **109,350** |
>
> The reasoning was: both sides move aligned 8-byte groups, so a write becomes
> ONE word and the 4,096-way write decoder disappears, while constant-offset
> reads fold to constant slices. That reasoning predicted a large reduction. It
> was wrong by 14,022 nodes IN THE WRONG DIRECTION.
>
> The change itself was sound and bit-identical — lint clean, 43 directed and
> 139,113 random checks, and `cmd_random`'s transcript hash unchanged at
> `0xb95b5f70a413bdbd` across 1,000 frames. It was reverted because the
> measurement rejected it, not because it was incorrect.
>
> **Why it grew is NOT established, and this entry does not guess.** Two
> inferences about this block have already been wrong tonight — "the CRC cone
> needs an incremental redesign" (it needed a bound check) and "words will
> shrink it" (they enlarged it). A third guess written down as fact would be
> the pattern, not the exception.
>
> ### What the measurements DO establish
>
> **A re-description does not fix this. Only a real memory does.** Both shapes
> are register arrays with a combinational read, and both overflow the device
> by themselves. The remedy has been written in the RTL from the start:
>
> > "the write lives in an async-reset process and the read is combinational,
> > and an M10K has no reset port and a registered read. Fixing that is a
> > protocol change (the beat stream needs a one-cycle read lead)."
>
> That is the work: a registered read, a one-cycle lead in the beat stream, and
> the initialiser dropped so the array can infer as RAM — the same cure
> `zhao_scanout_linebuf` received via `zhao_dc_sdp_ram`. It is a protocol
> change touching `CMD.DECODER`'s byte stream, which is why it was deferred
> originally and why it is not a same-session edit.
>
> **Step 8 remains gated.**
>
> ### ATTEMPT 2: split the readers so `slot_buf` can be a memory. FAILED, and
> ### it corrects the recorded remedy.
>
> The remedy recorded above — and in the RTL since the block was written — is
> "a registered read and a one-cycle lead in the beat stream". **That is
> incomplete, and the reason is why this block resists becoming a memory at
> all.**
>
> A RAM has one or two ports. `slot_buf` has **three independent
> arbitrary-offset readers** plus the write:
>
> | reader | offset | line |
> | --- | --- | --- |
> | the streamed byte | `rd_off`, walks the packet | `pkt_byte_o` |
> | header fields, header CRC, payload-CRC seed | below 64 | `hget*`, `crc_final` |
> | **the payload CRC compare** | `36 + command_bytes` — anywhere | `hget32(36 + cb)` |
> | **the RECORD WALK** | `36 + walk_off` — anywhere | `hget16(36 + walk_off)` |
>
> I attempted the obvious split: a 64-byte register window shadowing the
> header, leaving `slot_buf` with the stream as its only reader. The premise
> was that every non-streaming reader lives below byte 64. **It does not.** The
> record walk random-accesses record headers throughout the payload, and the
> payload CRC compare reads at an offset that depends on the packet's length.
>
> Measured: `cmd_dma_directed` 8 of 43 checks failed. Note that `cmd_random`
> PASSED with an identical transcript hash, so the directed lane is what caught
> it — the random lane never built a packet whose walk reached past the window.
>
> Reverted.
>
> ### What the remedy actually is
>
> Not one registered read but **three**, each needing a cycle of lead in the
> state machine that uses it:
>
> 1. the streamed byte (`M_STREAM` pre-issues the next address);
> 2. the record walk (`M_WALK` presents an address and waits a cycle before
>    reading `op`/`rb`);
> 3. the payload CRC compare.
>
> Plus the write moved into a process with no async reset and the initialiser
> dropped, or the array cannot infer as RAM regardless of the reads.
>
> **The alternative worth weighing first** is to stop random-accessing the
> buffer at all: have the record walk consume the streamed bytes rather than
> re-read them, which is how a decoder normally works and would leave one
> reader by construction. That is a larger change to `CMD.DMA` and possibly to
> the `CMD.DECODER` seam, and it is a design decision rather than a repair.
>
> **This is the third theory about this block to be corrected by evidence** —
> after "the CRC cone needs an incremental redesign" (it needed a bound check)
> and "words will shrink it" (they grew it). The pattern is consistent: the
> measurements are reliable, the inferences from them are not, and each one
> only fell over when something ran.

## 2026-08-21 — CMD.DMA still cannot be fitted, and the cause is a design defect (SUPERSEDED, kept for the reasoning)

**Measured, not inferred.** After step 6 removed the 1.97 Mbit `blit_buf`,
`zhao_cmd_dma` was re-fitted at HEAD. It did **not** succeed:

| | result |
| --- | --- |
| census (96c0394, with `blit_buf`) | `failed:quartus_map` — 16.2 GB elaboration |
| HEAD (no `blit_buf`) | **`timeout` — 4,838 s without finishing** |

Removing the buffer moved this block from *immediate failure* to *does not
finish*. That is progress on the memory axis and **not a fix**.

**A CORRECTION.** The step 6 commit called that removal "the composed-fit
unblock". It is not, on two counts: the QSF source-list drift was a second
blocker (fixed, and now gated by `tests/lint/source_list_parity`), and this
block still cannot be characterised at all.

### The cause

`zhao_cmd_dma.sv`, in the state machine's `always_ff`:

```systemverilog
for (int unsigned k = 36; k < 192; k++) begin
  if (k < seed_end) begin
    cseed = zhao_abi_pkg::zhao_crc32c_step(cseed, slot_buf[k]);
  end
end
```

**156 dependent CRC-32C byte steps unrolled into one clock cycle** — about
1,248 chained XOR/shift stages in a single combinational cone, each with a
guarded read of a 4,096-entry register array. Synthesis is not running out of
memory; it is trying to flatten a 156-deep serial dependency chain.

The `crc_final()` helper immediately above it carries a note showing the
authors knew about this class of problem: *"the loop bound is the constant 32;
a parameter-bounded loop would put SLOT_BUF_BYTES muxed CRC steps in the formal
cone for nothing."* The 32-step loop was bounded deliberately. The 156-step one
was not.

**This is not only a synthesis-time problem.** A 156-byte serial CRC chain in
one cycle will not close timing on any device. The block is unsynthesisable as
written, and the per-block census never said so because this block has never
been successfully fitted.

### What it blocks

The composed fit contains `CMD.DMA`, so **the composed fit cannot be expected
to complete until this is fixed.** Step 8 of the FRAMEBLIT integration is
gated on it.

### The shape of the fix (NOT yet done, and it is a real redesign)

The payload CRC seed must be computed **incrementally across cycles**, the way
the fetch path already accumulates `crc_pay_r` one bridge beat at a time,
rather than re-walked over the staging buffer in one cycle. `slot_buf` also
still has `blit_buf`'s original defect in miniature — an initialiser, an
async-reset write process and a combinational read — so it will not infer as a
RAM either, at 4,096 entries instead of 30,720.

Both wanted the same treatment and only the large one got it.
