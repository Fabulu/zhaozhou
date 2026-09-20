# Contract — the node-stream doorbell (SW.STREAM ⇄ GEOM.LOOM)

> Part of GEOM.LOOM's capability (`design/blocks.yml` ZH-041); not a ledger block of its own,
> in the same standing as `design/contracts/TERRAIN.WRITEBACK.DOORBELL.md`.
> Rulings: owner rulings **R58** and **R69** (`reports/OWNER-RULINGS-20260919-EVENING.md`),
> provisional, resting on the owner ruling of **2026-08-31 §6.4**.
> RTL: `fpga/rtl/geometry/zhao_geom_loomfeed.sv`
> Test: `tests/geometry/geom_loomfeed_directed.cpp` (the carrier with the REAL
> `zhao_geom_loom` behind it), and `tests/prod/tb_zhao_console_core_smoke.sv`
> (the carrier inside the composed console, feeding the draw's transform row).
> Positive control: `tests/mutants/zhao_geom_loomfeed_mutant.sv`.

## What was missing

`zhao_geom_loom` takes a **parent-before-child topologically sorted node stream** and a
per-frame **camera 3×3**. Nothing in `fpga/rtl` produces either, and nothing was ever
going to: the owner ruling of 2026-08-31 §6.4 puts the producer outside this console by
name —

> "The ARM/compiler supplies a parent-before-child topologically sorted stream. Loom only
> composes transforms. It does not perform: recursion; cycle detection; matrix inversion;
> gameplay event generation; autonomous gait logic; autonomous formation logic. Gait and
> formation values come from Form/Field programs. Keep-world reparenting is computed on the
> ARM between frames."

`design/contracts/GEOM.LOOM.md` calls that deletion "not a clarification, it is a large
deletion, and it is what makes this block buildable".

So when the geom3 packet composed GEOM.LOOM for ruling R29's transform palette, it opened
core entry **I50** deliberately: twelve wide stream fields and a nine-element camera basis
stood at the console's edge with no owner. Ruling **R69** answered it — *"not blocked, only
unbuilt: `zhao_part_hps` is the exact pattern and the arbiter is already N-client"* — and
**R58** named the shape: *"the R14/R43 doorbell pattern: SW.STREAM stages the sorted stream,
a CSR mailbox names it, hardware acknowledges. No second transform law."*

**What was missing was a CARRIER, not a block.** This contract is that carrier's law.

## Why the bulk rides DDR and only a pointer rides the mailbox

GEOM.LOOM.md's content tier is 256 creatures at ~28 bones plus attachments — about **9,000
nodes per frame**. At the record below that is 576,000 bytes a frame. A mailbox carrying
whole node records would be a 445-bit write port on the console edge, played 9,000 times a
frame, which is the arrangement entry I50 already refused.

The HPS-DDR bridge is this console's transport for bulk host state and `zhao_part_hps` is
its worked example at a larger volume still (1 MiB per tick, PART.STATE's generation
store). `zhao_hps_arbiter_n` has been N-client since owner ruling R4. So the mailbox
carries a **base and a ticket**, and the bytes ride the bridge as **client 4**.

## The record — 64 bytes, eight 64-bit beats, low half first

**64 is not padding and it is not free-chosen.** `zhao_hps_bridge` accepts 64-byte
*aligned* bursts of 1..64 bytes (`fpga/rtl/memory/zhao_hps_bridge.sv`:106), so 64 is the
largest burst and **the only size at which one record is exactly one burst**.

The natural packing of a node is **56 bytes** — 445 bits of fields, dominated by twelve
fx16 parameters at 48 bytes. 56 is not a burst multiple: a 56-byte stream puts the fifth
record across a burst boundary and costs a second request for every eighth node, plus an
alignment law nobody can read off the record. The eight spare bytes buy one-record-one-burst,
and that is the whole argument. `geom_loom_feed_bursts_o` is the number that would say it
had stopped being true: a header plus *n* nodes must be exactly *n*+1 bursts.

Byte offsets are **little-endian within each 64-bit beat**, the same convention
`zhao_part_hps` states for its 16-byte particle record.

### Record 0 — the STREAM HEADER, at `base`

| beat | bits | field |
|---|---|---|
| 0 | [31:0] | `magic` = `0x4D4F4F4C` ("LOOM") |
| 0 | [47:32] | `node_count`, 1..`MAX_NODES` |
| 0 | [63:48] | reserved, ignored |
| 1 | [31:0] / [63:32] | `cam_basis[0]` / `cam_basis[1]` |
| 2 | [31:0] / [63:32] | `cam_basis[2]` / `cam_basis[3]` |
| 3 | [31:0] / [63:32] | `cam_basis[4]` / `cam_basis[5]` |
| 4 | [31:0] / [63:32] | `cam_basis[6]` / `cam_basis[7]` |
| 5 | [31:0] | `cam_basis[8]` |
| 5 [63:32], 6, 7 | | reserved |

`cam_basis` is the frame's camera 3×3, row-major, fx16 S15.16 — GEOM.LOOM's `cam_basis_i`
element for element.

**The camera basis travels in the STREAM, not in the mailbox, and that is correctness
rather than packing.** `cam_basis_i` is a *held* port the loom reads as BILLBOARD nodes
compose. Latched by a mailbox write it would be two quantities loaded by two different
enables that must agree — CLAUDE.md's metadata-swap shape, where "response A's data and B's
metadata" becomes **"this frame's nodes and the next frame's camera"**, a wrong picture with
every counter balancing. In record 0 it is loaded once, by the same act that starts the
stream, and the RTL's `S_PLAY` never writes `cam_q`.

### Records 1..`node_count` — the NODES, at `base + 64*i`

| beat | bits | field |
|---|---|---|
| 0 | [9:0] | `node_index` |
| 0 | [19:10] | `parent_index` (ignored when `kind` is ROOT) |
| 0 | [23:20] | `kind`, 0..9 per GEOM.LOOM's kind table |
| 0 | [25:24] | `axis`, 0=X 1=Y 2=Z (ORBIT) |
| 0 | [26] | `bodypatch` |
| 0 | [31:27] | reserved |
| 0 | [47:32] | `angle16`, turns (ORBIT) |
| 0 | [63:48] | `src_id` |
| 1..6 | [31:0] / [63:32] | `param[0]`…`param[11]`, fx16 S15.16, two per beat |
| 7 | | reserved |

**`first` and `last` are NOT in the record.** They are the *framing* of the stream, and the
carrier derives them from the record index against the header's `node_count`, so a staged
stream cannot carry framing that disagrees with its own length. GEOM.LOOM's FRAMING refusal
(reason 5) is therefore unreachable from this feed in normal play — the same standing as that
block's own OVERFLOW guard at the shipping parameters. It stays reachable at the block level
(`tests/geometry/geom_loom_directed.cpp` fires it) and on the abandonment path of law 5
below, which is the one place a partial stream can exist. Said out loud rather than
discovered.

**The index fields are ten bits and `IDXW` is pinned to 10 by an elaboration guard.** A
carrier at a narrower `IDXW` would truncate a node index into something that still looks
legal to the loom. If GEOM.LOOM's `IDXW` ever moves, this table moves with it and so does
that guard.

## The exchange, in three messages

Every message is a ready/valid transfer across the HPS edge of `zhao_console_core`. In
Verilator the harness IS the HPS (plan D10), exactly as it is for the FRAME_RING view and
the terrain arena configuration; these are the HPS's own words, not tie-offs.

| # | direction | message | fields |
|---|---|---|---|
| D0 | HPS → FPGA | the plan's epoch identity, held. Trace only | `plan_base:u32` |
| D1 | HPS → FPGA | a POST: one staged stream, posted ahead of need | `base:u32`, `ticket:u32` |
| D2 | FPGA → HPS | a RETURN, exactly one per consumed post | `ticket:u32`, `ok:1`, `refused:1`, `reason:u3`, `loom_reason:u3`, `nodes:u16`, `plan:u32` |

**There is no D3.** The journal doorbell needs an ACK because its consumer holds a ticket
table that must be released; nothing here holds state past the return, so an ACK would be a
message with no reader. Same sentence, same reason, as the FIELD program doorbell's.

`reason`: 0 NONE · 1 ALIGN · 2 MAGIC · 3 COUNT · 4 LOOM · 5 BRIDGE.
`loom_reason` is GEOM.LOOM's own, and only meaningful when `reason` is LOOM.

## The laws

1. **Posts are consumed in order, one at a time, and are never dropped.** A post offered
   into a full mailbox is HELD — `post_ready_o` low, and `geom_loom_feed_post_stalls_o`
   counts the **cycles**, which is the number that says whether `POSTS` is big enough.
   Owner ruling **R55**'s shape.

2. **A misaligned base is refused before a burst is issued**, answered (`reason = ALIGN`)
   and counted. `zhao_hps_bridge` would reject a misaligned burst itself, but as an `err`
   with no way for software to tell "your pointer was wrong" from "the port was busy".
   Judging it here makes the answer specific. This is `zhao_part_hps`'s seed-alignment
   refusal at this seam.

3. **A header that is not a header is refused and answered.** Wrong magic (`MAGIC`), or a
   node count of zero or above `MAX_NODES` (`COUNT`). A stream of zero nodes is not a
   stream; a count above the loom's bound would otherwise be refused as OVERFLOW *after*
   `MAX_NODES` nodes had been composed and thrown away, so answering here costs one burst
   instead of a thousand. **Nothing is clamped**: a count of 2,000 is refused, never played
   as 1,024. A clamped stream is a picture of the wrong creature with every counter agreeing.

4. **The loom's refusal is RECORDED and the stream is played on to its `last`.** This law
   reads backwards from the intuitive one and the reason is in `zhao_geom_loom.sv`'s
   S_REF/S_DRAIN: a refused beat that did not carry `last` puts the loom in S_DRAIN, where
   `in_ready_o` stays HIGH and beats are **swallowed** until the stream's own `last`, and
   only then is the store scrubbed. Its own comment says why — *"waiting for a `last` that
   has gone by would swallow the NEXT stream's beats instead"*.

   So a carrier that stopped on the refusal would leave the loom draining, and **the next
   stream would be eaten up to ITS `last` with no refusal raised at all** — a whole frame of
   poses gone, reported as `ok`. The verdict is therefore latched and the stream runs to its
   end. `ret_nodes_o` reports the count **at the refusal**, not the drained total.

   The verdict is never re-derived: this carrier does not check sortedness, parentage, kinds
   or shear, because that would be a second opinion about a law GEOM.LOOM owns. R58's "no
   second transform law" extends to the refusals.

5. **A bridge refusal is handled, not argued** — owner ruling **R54**, the repair
   `zhao_part_hps` carries. `err` at the request is counted and the request is taken DOWN,
   so the re-offer is a *new* request rather than the same one being re-served by
   `zhao_hps_arbiter_n`. `ERR_RETRY_N` consecutive refusals of one burst are permanent by
   definition and FAULT the stream (`reason = BRIDGE`), answered and counted. Never a spin.

6. **An abandoned stream leaves the loom waiting for a `last`, and the next stream is played
   TWICE to give it one.** Law 5 is the only path that stops a feed without a `last`, so it
   is the only path that can leave the loom open — in S_RUN (still composing) or S_DRAIN
   (already refused the partial), and **both are released by a `last` and by nothing else**.

   The recovery therefore does not need to know which, and deliberately does not look. The
   next stream is a FLUSH PASS: played whole, its `last` releases the loom, the scrub
   follows, and the same stream is then played again — for real — into a provably idle loom.
   Only the second pass is reported. `geom_loom_feed_replayed_o` counts it, and the flush is
   bounded by construction: the staleness flag is set only by an abandonment and cleared by
   the flush pass's `last`, so a stream is flushed at most once.

   **Nothing is fabricated on this path.** The flushed beats are the same bytes, re-read from
   the same addresses.

7. **The return queue cannot overflow, by credit.** A post is consumed only while fewer than
   `RETQ` consumed posts still owe their return, and each owes exactly one.
   `geom_loom_feed_ret_overflow_o` is therefore unreachable by legal stimulus, so its zero is
   an argument rather than a measurement, and it carries a committed inverted-polarity
   mutant.

8. **Nothing times out.** A return the ARM never drains holds its credit and the mailbox
   backs up; the stall count says so. A carrier that discarded a return to keep moving would
   lose the only record that a frame's poses reached the palette.

## Arbiter placement, and why it cannot deadlock

Client **4** of `u_terr_hps_arb`, the lowest priority, **read only** — the carrier never
writes DDR, so its write arm is zero exactly as clients 0 and 1 already are.

The arbiter's law is that a continuously-asking lower index starves every higher one, so a
burst of terrain page loads makes the node stream wait, visibly, in `c4_wait_cycles`. It
cannot deadlock, and the reason is the carrier's rate rather than a dependency argument: no
terrain or particle client waits on a node transform, and the carrier holds **one 64-byte
burst in flight** against a consumer that spends **48 clocks per node** (GEOM.LOOM's own
measured figure at `MUL_LANES = 1`). Waiting stalls the stream's composition and never drops
a record.

## The rate is a cost, not a gap

One record is read, then offered, then the next is read — there is no prefetch. Against the
loom's 48 clocks per node, the carrier's 8 beats plus the bridge's 16-cycle first-beat
latency (~25 clocks) are time the loom is busy for anyway, so the feed is not the limit at
the shipping parameter. **`geom_loom_feed_wait_cycles_o` is the number that would say
otherwise**: cycles the loom was ready while the carrier had nothing to give it. If it ever
dominates, the lever is a second record in flight and it is a change to one file.

## Counters

| port | what it counts |
|---|---|
| `geom_loom_feed_posts_o` | posts consumed, all outcomes |
| `geom_loom_feed_streams_o` | streams delivered whole and composed |
| `geom_loom_feed_nodes_o` | node BEATS handed to the loom — **not** `geom_loom_nodes_o` (nodes *transformed*). They differ by exactly the beats the loom swallowed into a drain, so the gap between them is a real reading |
| `geom_loom_feed_bursts_o` | 64-byte reads granted; must equal 1 + nodes per stream |
| `geom_loom_feed_align_refused_o` | law 2 |
| `geom_loom_feed_hdr_refused_o` | law 3 (magic and count together) |
| `geom_loom_feed_refused_o` | law 4 — the loom refused a stream |
| `geom_loom_feed_faulted_o` | law 5 — the bridge refused one permanently |
| `geom_loom_feed_replayed_o` | law 6 — a flush pass happened |
| `geom_loom_feed_post_stalls_o` | law 1, in cycles |
| `geom_loom_feed_bridge_errs_o` | `err` seen at the request, transient or not |
| `geom_loom_feed_wait_cycles_o` | the rate instrument above |
| `geom_loom_feed_ret_overflow_o` | law 7; unreachable, see the mutant |

## Proof that the suite can fail

`tests/geometry/geom_loomfeed_directed.cpp` — 75 checks, **with the real `zhao_geom_loom`
behind the carrier** rather than a mock. That is deliberate: both of the hard laws above are
statements about the loom's own S_RUN/S_DRAIN behaviour, and a mock would have been written
to agree with whatever the carrier does. The first draft of law 4 was backwards and a mock
would have passed it.

Every counter is fired by legal stimulus except `ret_overflow_o`. Cases 5 and 8 are the ones
that matter: each asserts that **the stream AFTER the bad one still composes**, which is the
assertion the wrong version of its law fails.

`tests/geometry/geom_loomfeed_mutant_control.cpp` drives
`tests/mutants/zhao_geom_loomfeed_mutant.sv` with **inverted polarity** — one substantive
line, `ret_credit` forced true — and passes when the counter FIRES.

## What is not established

* **No area or timing number.** This block has never been through `quartus_map` or a fit.
  Its cost is one 64-byte record register file, a small mailbox and a small return queue,
  and that is an estimate, not a measurement.
* **The record's reserved bytes are not a growth plan.** Beat 7 of a node and beats 6–7 of a
  header are reserved because 64 is the burst size, not because a field is coming. A field
  that wants them is a change to this contract, the RTL's slices and the bench's staging in
  one pass.
* **`MAX_NODES` is mirrored, not derived.** The carrier's bound and GEOM.LOOM's are written
  side by side at the instantiation in `zhao_console_core.sv` and the match is load-bearing
  (law 3). Nothing enforces it at elaboration, because the two blocks do not see each
  other's parameters; the composer states both, in one place, with the reason beside them.
