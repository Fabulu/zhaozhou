# Owner ruling — M10K ceilings yield to ALM savings

> "Using some more M10K is fine, we have enough, particularly if it saves ALMs.
> They're our only weapon against our massive ALM debt."
> — Fabian, 2026-09-18

## What this settles

A per-block M10K ceiling in a contract is **not** a reason to reject an
arrangement that trades memory for logic. Where the two conflict, the ALM debt
wins and the ceiling moves, with the new number recorded.

The immediate case: `POST.COMPOSITE.md` caps the block at **8 M10K**. The
implementation needs **7** for the nine-line ring, and the owner plan's §11.2
exact product-vector grading table needs **6** more (9,216 logical bits, three
simultaneous 72-bit reads → six simple-dual-port slices, *"Six, not one"*).
13 total, 19 if double-banked. The worker shipped nine real multipliers instead
and recorded the conflict rather than deciding it.

**Ruled: take the table.** 19 M10K against a 553-M10K device is 3.4%.

## The three things this ruling does NOT license

These come from the owner plan and this repo's own history, and the ruling above
does not touch any of them.

1. **It is not permission to spend memory to remove DSPs.** DSPs are not the
   binding constraint — 112 available, and no campaign has been DSP-bound. The
   PART.COLLIDE worker declined quarter-square ROMs on exactly this basis
   (~100 M10K to remove ~10 DSPs) and that judgement stands.
2. **It is not permission for full-frame lookup tables or port replication.**
   Plan §11.2 forbids a generic 65,536-entry RGB565 remap by name, and says
   *"Full-frame lookup tables and ignored port replication are not free memory
   tricks."* The ruling raises a ceiling; it does not delete the architecture.
3. **Logical bits are still not physical M10Ks.** Plan §14.5: *"Physical M10K
   reserve must be measured, not inferred from 18.2% logical bit occupancy."*
   Every M10K figure quoted for new work in this session — 7 for the ring, 6 for
   the table — is **shape arithmetic, not a measurement.** Whether Quartus infers
   the ring as M10K at all is unverified. The ruling authorises the trade; only a
   fit reports what it cost.

## The ranking heuristic, and its stated limit

The standing "~200 ALM per added M10K" rule remains the way to rank candidates.
Plan §14.5 qualifies it precisely: it is *"a useful ranking heuristic only on NET
parent measurements; a timing/DSP fix below that rate can still be necessary."*

So: rank with it, never close with it. A candidate that clears 200 ALM/M10K on a
leaf and loses it in the parent's queues and routing has not saved anything.
