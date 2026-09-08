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
