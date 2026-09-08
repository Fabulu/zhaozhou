# §7 candidate: one metadata join before the class fan-out

Owner brief §7. Groundwork only — **no saving is claimed here**, because the
brief forbids it until the V3 MAP report exists:

> *"It also requires the new V3 MAP report before assigning a saving: the old
> island's 'P0-E closed' census is not an inventory of this changed
> composition."*

The Stage C fit is still running. What follows is the read-site inventory and
the packet shape, both facts available today.

## What exists now

```
logic [20:0] sampmeta_m [64][3];        // 192 entries x 21 bits = 4,032 bits
record: {nib, fmt[2:0], fv[7:0], fu[7:0], addr[0]}
```

| site | line | role |
|---|---|---|
| write | 1369 | planner request handshake, keyed by the identity the response will return under |
| read | 1626 | **bilinear** — `bil_meta`, from which `bil_fmt = bil_meta[19:17]` |
| read | 1809 | **CLUT** |
| read | 1934 | **nearest** — `near_meta` |

**One writer, three readers, all asynchronous, all indexed by a different
token.** That is the structure the brief identifies: replicating a RAM per
reader is wasteful, and arbitrating one RAM between them serialises three lanes
that are independent today.

## Why the join point already exists

`zhao_texture_rsp_dispatch` is *already* the common stream. Its interface is
one input and four class outputs:

```
in :  rsp_valid_i / rsp_data_i / rsp_tok_i / rsp_class_i
out:  clut_* | near_* | bil_* | err_*      (each: data + tok)
```

Every response passes through it **before** the split. So the change is not a
new stage — it is moving an existing read to a place the data already flows:

```
cache response
  -> reserve destination capacity
  -> synchronous metadata read        (ONE port, on the common stream)
  -> capture data + metadata + matching identity
  -> existing dispatcher and class queues   (payload +21 bits each)
  -> existing decoding lanes, unchanged
```

The three async read ports collapse to one synchronous read. `sampmeta_m`
becomes a genuine one-writer/one-reader bank, which is the shape that can live
in memory rather than in flops.

## The cost side, stated because it is not free

Each class queue's payload grows by the metadata width. Four outputs
(`clut`, `near`, `bil`, `err`) currently carry `DATAW + TOKW`. The brief is
explicit that register count is a means and not the objective — *"adding a few
hundred pipeline registers to remove a long cone can be exactly right"* — so the
trade is queue payload against 4,032 bits of multi-ported storage plus three
selection cones, and **only a fit can settle it.**

## Preconditions before this is attempted

1. **The V3 MAP report.** Without it there is no inventory of this composition,
   and the P0-E census describes a different machine.
2. **§3's contract repairs first.** The brief orders it that way, and three
   permanently-zero fault ports should not be carried into a storage change.
3. **A credited read reservation.** The brief's own bundled model check (`J1`,
   32 schedules x 10,000 cycles, max reserved 4) covers the credited 3-cycle
   read join and detects an ignore-in-flight mutant. Capacity must be reserved
   *before* the read is launched, or a stalled consumer drops a response that
   has already left the bank.

## What this is not

It is not "put the array in RAM". That framing produces either a replicated
bank per reader or an arbiter that serialises the decode lanes, and the brief
rejects both. The change is **where the join happens**, not what the storage is
made of — the storage shape follows from moving the join.

---

# CORRECTION: 21 bits was the wrong subset

**Post-fit brief §5.1, and it is right.** This report described the join as
carrying the existing 21-bit `sampmeta_m` row and widening the class queues by
21 bits. That is not the design; it is a fragment of it.

> *"The earlier owner brief already specified a 40-bit metadata record including
> palette identity and owner-generation alignment. The new working report
> reduces its description to the existing 21-bit sampmeta row ... That is
> incomplete relative to both the earlier decision and the new palette timing
> evidence."*

## Why the subset defeats the purpose

The 21-bit row is `{nibble, format, fraction_v, fraction_u, byte_select}`. It
contains **no palette slot and no palette generation** — those live in
`palslot_m[64]` and `palgen_m[64]`, written at admission and read on the
response side to feed the CLUT lookup.

And the palette binding read is precisely what sits on the island's worst
*internal* path:

```
rsp_dispatch|cq_rp[0][0] -> palette_res|cold_o[26]     -2.093 ns
  class-queue pointer -> queued route token
  -> 64-owner palette binding selection      <-- palslot_m / palgen_m
  -> resident slot/generation -> classification -> counter
```

**Moving only the 21 bits would have widened three queues, changed a storage
structure, cost a fit, and left the measured critical family exactly where it
is.** It would have looked like progress and bought nothing on the path that
gates the clock. That is a more expensive mistake than doing nothing.

## The full record, 40 bits

| field | bits |
|---|---|
| descriptor owner generation | 8 |
| palette slot | 2 |
| palette generation | 8 |
| resolved format | 3 |
| fraction U | 8 |
| fraction V | 8 |
| byte selection | 1 |
| CLUT4 nibble selection | 1 |
| reserved | 1 |
| **total** | **40** |

Address `{owner_slot[5:0], sample_index[1:0]}` — 256 rows, **of which sample
index 3 is invalid**. The brief's caution is worth keeping verbatim: *"A table
address fitting in eight bits is not proof that every encoded sample is legal."*
`zhao_texture_ident_pkg.sample_index_legal()` already names that rejection, so
the check has somewhere to live.

The descriptor generation is an **alignment check against the returned token**,
not payload — the same class of check as the `mat_aligned_c` compare that fixed
COMBINE this morning.

## What this changes about the ordering

Nothing about the sequence — §3's repairs, then §4's palette experiment, then
this. But it changes what "this" is: **the join subsumes the palette binding
read**, so it and the P-CNT experiment attack the same cone from two ends.
P-CNT takes the counter off the tail; the full join takes the binding
selection off the head.

That also means the two must be measured in a stated order, or their effects
will be attributed to whichever landed second.

## The deletion inventory, exact

Every response-side indexed read the 40-bit join subsumes, from source:

| table | writer | response-side readers |
|---|---|---|
| `sampmeta_m[64][3]`, 21 b | planner accept `:1383` | bilinear `:1640`, CLUT `:1823`, nearest `:1948` |
| `palslot_m[64]`, 2 b | admission `:1250` | palette `lu_slot_i` `:1803` |
| `palgen_m[64]`, 8 b | admission `:1251` | palette `lu_gen_i` `:1804` |

**Five asynchronous response-side reads across three tables**, all indexed by a
route token, replaced by one synchronous read on the common stream.

Lines 1803-1804 are the *"64-owner palette binding selection"* named in the
critical cone. **That is the pair my 21-bit version left in place.**

## The 40-bit row is exactly one M10K, and that is not a coincidence

```
256 rows x 40 bits = 10,240 bits
one M10K           = 10,240 bits
```

Address `{owner_slot[5:0], sample_index[1:0]}` gives 256 rows; the record is 40
bits; the product is the M10K's exact capacity. The brief's dimensions are
chosen so the joined table is **one block**, not one-and-a-bit.

Current storage for comparison: 4,032 + 128 + 512 = **4,672 bits** spread over
three tables with five asynchronous read ports. The join uses more raw bits and
far fewer ports — which is the whole trade, and why *"put the array in RAM"* is
the wrong framing. An async-read table costs flops and selection cones
regardless of how few bits it holds; `uvw_m` was 4,096 bits and cost 2,053 ALM.

**No saving is claimed here either.** 4,672 -> 10,240 declared bits is an
increase; whether it is a win depends on flops and selection logic removed
versus one M10K and 40 bits of extra queue payload consumed, and only a fit
settles that. What is established is the port count: **five reads become one.**
