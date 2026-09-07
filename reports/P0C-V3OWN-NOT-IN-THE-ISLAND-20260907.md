# P0-C — the V3 owner is not in the composed island at all

The rearchitecture brief warns:

> The larger integration must remove the old FRAGROB lifetime/result machinery
> and redundant ordering/storage where V3 replaces them. Otherwise we risk
> building a better owner alongside the old expensive machinery and wondering
> why the composed area barely changes.

That is not a risk to be avoided. **It is the current state**, and it is worth
stating plainly because it changes what today's owner-block result means.

## The fact

`zhao_texture_v3own` is instantiated **nowhere** in `fpga/rtl/`. Every remaining
mention is a comment:

    fpga/rtl/synth/zhao_probe_v3rq_queue.sv:16   "the shape zhao_texture_v3own actually instantiates"
    fpga/rtl/texture/zhao_texture_v3bank.sv:40   "instantiation in zhao_texture_v3own connects ..."
    fpga/rtl/texture/zhao_texture_v3bank.sv:57   "the three ready queues in zhao_texture_v3own ..."

`zhao_texture_island_top.sv:888` instantiates `zhao_texture_fragrob` instead,
and `design/fit_targets.yml`'s island source list carries `zhao_texture_fragrob.sv`
with no `zhao_texture_v3own.sv` in it.

## What that means for today's numbers

| | ALM | registers | Fmax (reported) |
|---|---|---|---|
| `zhao_texture_island_top` (composed) | 13,601 | 23,181 | 66.77 |
| `zhao_texture_fragrob` (in the island) | 1,676 | 2,631 | 103.1 |
| `zhao_texture_v3own` (standalone, post-T2) | 3,348 | 3,953 | 94.05 |

**The island's 13,601 ALM contains the OLD fragrob.** Today's T2 migration —
`gen_q`'s 512 flip-flops deleted, ALM 5,709 → 3,348, core→core 91.32 → 98.18 —
is real and measured, and it is **outside the composed design**. It cannot move
the island's number until the owner is instantiated.

This also answers a question the brief raises about P0-D's remaining payoff: the
owner block's area should be judged against 3,348, but the island's area does not
yet contain any owner-block figure at all.

## The uncomfortable arithmetic, stated rather than buried

`v3own` at 3,348 ALM is **twice** `fragrob`'s 1,676. A naive swap therefore ADDS
about 1,672 ALM to a composition that is already 13,601 against a 7,500 redline.

The brief's own answer is that the saving is not in the swap but in what the swap
makes deletable — "remove the old FRAGROB lifetime/result machinery and redundant
ordering/storage where V3 replaces them". That is a claim about `rsp_dispatch`,
the ordering/result storage, and duplicated lifetime state elsewhere in the
island, and **it is not yet measured**. Nobody should assume the swap pays for
itself; it is exactly the "first explanation that absolves the design" shape, and
the two totals can be differenced once the integration exists.

The honest position: the two blocks are not interchangeable and the comparison
above is NOT like-for-like. `v3own` implements the full CAPTURE/SNAPSHOT/CLAIM/
WRITE/PUBLISH/READY event structure §6.2 asks for, including the identity law of
owner ruling T2; `fragrob` combines range, generation, liveness, required, issued
and duplicate checks with the payload write, which is the defect §6.1 names. The
area difference buys the separation. Whether the island's TOTAL falls depends on
the deletions, which are the actual P0-C work.

## Status

P0-C's precondition is established: there is no partial integration to finish and
no second ownership system live in the island — the V3 owner has simply never
been composed. The integration is a first instantiation plus the removals it
licenses, not a migration between two running systems.
