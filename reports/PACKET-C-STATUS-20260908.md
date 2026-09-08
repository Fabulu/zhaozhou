# Packet C: where it stands, and the constraint on step 2

## Step 1 — DONE and verified in the composed island

`zhao_texture_metajoin` — 256 × 40 bits (exactly one M10K), one writer at
planned-sample acceptance, one **synchronous** reader on the common response
stream.

| evidence | result |
|---|---|
| leaf suite `metajoin_directed` | **7 checks**, all 192 legal rows, every field distinct |
| shadow inside the composed island | **1,176 comparisons, 0 mismatches** |
| gate 2 | **121 checks** (was 119) |
| oracle | 119, unaffected — the assertion is under `ISLAND_V3` |

The bank returns exactly what `sampmeta_m`, `palslot_m` and `palgen_m` would
return, on every response crossing the common stream, inside the real
composition. Nothing downstream consumes it, so island behaviour is unchanged by
construction.

**Two defects were caught by this process rather than by review**, and both are
worth keeping in mind for step 2:

* the bank's five hand-typed field offsets were **all off by one** — lint-clean,
  every output driven, every field wrong. Offsets are now derived from widths
  with an elaboration-time `$fatal`.
* the shadow's first assertion sat in the fault probe and reported **"0
  comparisons, 0 mismatches"**. The non-vacuity check failed it correctly: that
  probe drives `sample_count = 0` and never serves texture memory, so no
  response reaches the common stream. Zero over zero.

## Step 2 — blocked on a shared-block decision, not on effort

The brief's pipeline is explicit:

> *cache response -> reserve destination capacity -> synchronous metadata read
> -> capture data + metadata + matching identity -> existing dispatcher and
> class queues -> decode*

So the metadata must travel **with** the response through the class queues. That
means `zhao_texture_rsp_dispatch` gains a `rsp_meta_i` input and four
`*_meta_o` outputs, with `cq_m[cls][idx]` beside the existing `cq_d`/`cq_t`.

**`zhao_texture_rsp_dispatch` is instantiated by BOTH tops** — the V3 island and
`zhao_texture_island_top`, which is gate 3's oracle. Changing it changes the
reference the whole restructure is measured against. The brief and this run's
own rule both say the same thing: *editing an oracle destroys the comparison
that makes the work checkable.*

Three ways forward, and the choice is an owner call, not a barge-ahead:

1. **Add the ports and leave them unconnected in the oracle.** Legal in both
   Verilator and Quartus, and the oracle's logic is untouched — but its
   instantiation line changes, and a floating input in a reference design is
   the kind of thing that is fine until it is not.
2. **Parameterise the widening** (`META_EN`, default 0) so the oracle elaborates
   exactly as today and only the V3 top turns it on. Costs a parameter and two
   code paths in a block that currently has one.
3. **Fork the dispatcher** for V3, as was done for the island top itself. Most
   faithful to the "do not touch the oracle" rule, and it duplicates a block
   that is otherwise identical.

There is a second timing question underneath: the bank's read result arrives
**one cycle after** `rsp_valid_i`, so response and metadata are not aligned at
the dispatcher's input. That is what the brief's *"reserve destination capacity"*
step exists to solve, and its bundled `J1` check (32 schedules × 10,000 cycles,
max reserved 4) models exactly this credited read join. It is a real design
step, not plumbing.

## Recommendation

Step 1 stands on its own and is safe to keep: a verified bank, shadow-agreeing
1,176 times, consuming nothing. **Step 2 should not start until the dispatcher
question is answered**, because all three options change what gate 3 means, and
gate 3 is the only reason the V3 composition can be trusted at all.

---

# SUPERSEDED: step 2 was not blocked, and packet C is now complete

The section above says step 2 needs an owner decision on the shared dispatcher.
**It did not.** The parameterised option (`META_EN`, default 0) leaves the
oracle's netlist identical, so gate 3's comparison is preserved rather than
altered — and that is checkable, not arguable. I escalated a question I could
answer, and the check took one run: gate 3 still reports 392 byte-identical
records after the change.

## Final state

**All five asynchronous response-side reads are on the class queue.** Five
falsifiers, all zero:

| falsifier | result |
|---|---|
| bank vs live tables (shadow, common stream) | 1,176 / 0 |
| CLUT queue alignment | 792 / 0 |
| nearest queue | 192 / 0 |
| bilinear queue | 768 / 0 |
| dispatcher leaf (`META_EN=1`) | 5/5 |
| conservation across the join | 1,176 in / 1,176 out |

Gate 2 **124 checks**, gate 3 **392 byte-identical**, oracle **119**.

## What it actually took, and the part worth remembering

The join could not be delivered by moving five wires. Two independent errors
were **cancelling**:

1. the dispatcher captured metadata from the current input rather than the FIFO
   entry being dispatched — 239 of 240 wrong at the leaf;
2. the island fed it the bank's answer, which arrives one cycle after the read
   is launched.

Whenever the raw FIFO was empty — most of the time — they agreed, and the
composed suite reported two of three class queues clean. **Fixing either alone
was a regression**: repairing the dispatcher took the composed bilinear count
from 32 wrong to 256.

The credited read join removes both causes at once by making the response, its
metadata and its identity one captured record: accept from the cache only when
the join stage can hand on what it holds, and fire the bank read only on an
accepted beat. That is precisely what the brief specified and what I twice
deferred as "plumbing".

## Still open

The `@pktC-fixed` re-fit. The earlier receipt — **+2,778 ALM, −8.43 MHz** —
priced the join's cost on a design that had the misalignment AND still read the
tables, so it measured the overhead with none of the benefit. Whether packet C
earns its place is that fit's answer, not this report's.
