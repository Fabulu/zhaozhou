# The island's live combiner, measured for the first time

*2026-09-07. `zhao_texture_material_combine_v2`, 1,547 s, status **ok**. This
block had never been fitted: it existed only as a *source* inside
`zhao_texture_island_top` and the production list, and it got a fit target of
its own today. It was the first of the eight blockers in
`V31-ISLAND-BUDGET-BLOCKED-20260907.md`.*

---

## The numbers, against the version the production top still carries

| | `..._v1` | **`..._v2`** | |
|---|---:|---:|---|
| ALM | 1,475 | **870** | −41% |
| registers | 893 | 918 | +25 |
| M10K | 0 | **6** | new |
| DSP | 2 | 2 | §3.4 tripwire held |
| **Fmax** | **36.28** | **114.04** | **×3.1** |

**V2 clears the 100 MHz product clock at 114.04 MHz. V1 sits at 36.28.**

## Why this matters beyond one row

**The production resource top is costing the wrong block.** `zhao_prod_top`
instantiates **V1**; the island instantiates **V2**. That divergence was found
earlier today in `design/prod_manifest.yml`, whose entry claimed *"V2 is not
instantiated by anything yet"* — false, and now demonstrably expensive. The
production top answers the owner's question *"what does the planned console cost
when counted ONCE?"* and it is currently answering with a block that is **70%
larger in ALM and a third of the clock** of the one the machine actually
contains.

That is not a proposal to swap it here. The manifest's own rule is that changing
what the production top instantiates is *"a decision to take deliberately rather
than as a side effect"*. But the decision now has numbers attached instead of
being a naming inconsistency.

**§12.3's COMBINE allowance was explicitly a guess, and it is exactly right.**
The 69-block profile lists:

    COMBINE local payload/scratch/tag allowance               6

and §12.3 flags it: *"The six-block COMBINE allowance is **NOT a measurement**."*
It is now a measurement, and it is **6**. One of the profile's admitted
soft numbers is hard.

**It also sharpens today's other M10K finding.** V1 used **zero** M10Ks; V2 uses
six. So the island gained six blocks by adopting the better combiner, and nobody
counted them, because the block had no fit. Combined with the seven M10Ks that
`zhao_texture_v3own`'s four-deep queues consume, the island's block budget has
two separate uncounted sources — and both were invisible for the same reason:
**a block with no fit target has no cost.**

## What this does not settle

* Not the whole-island reconciliation. Seven of the eight blockers remain, all
  stale rows needing refits, all named in the blocked report.
* Not that 6 M10Ks is right. It is what the fitter chose for V2's internal
  storage; whether some of it should be fabric is §5.6's mapping question, asked
  the same way as for the four-deep queues.
* Not a like-for-like V1/V2 comparison of *function*. V2 is the paired-phase
  design; the owner's brief keeps V1 as the historical comparison rather than
  claiming they compute the same thing at the same cost.
