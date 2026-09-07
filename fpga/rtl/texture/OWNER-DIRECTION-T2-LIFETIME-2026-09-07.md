# ACKNOWLEDGED: the T2 owner-lifetime ruling

**Received and read** on the hardware branch `zixxtrixx-v8-closeout`, commit
`8969dcaf`, file `reports/ZHAOZHOU_T2_ARCHITECTURE_DECISION_BRIEF_2026-09-07.txt`
(1,652 lines). Acknowledged here rather than in a run folder, because a run
folder is orphaned by the next pass.

## The ruling, restated so the next reader needs no second document

> An owner instance has authority from its accepted admission until its ordered
> external output transfer. Once that transfer retires the owner, later events
> for that instance are stale. **Matching a slot's residual generation bits does
> not extend the owner's authority after retirement.**
>
> A stale event must not change that owner's scoreboard, create ready work,
> authorize COMBINE, publish a result, release a credit, or resurrect the owner.
> The receiver may consume and discard a bad transport packet so that transport
> can drain. **Consuming a packet is not accepting its claimed ownership.**

Applies separately to (a) TMU publication at C4, (b) AUX at C4, (c) FINAL at C4,
and (d) the actual COMBINE input handshake that sets `combine_issued` — which is
**not** a fourth C4 return but a different lifecycle event with its own
acceptance rule.

## The correction I am accepting, and it was mine to make

> The earlier claim "those four guards must change semantics before the table
> can be deleted" was **too strong**. The policy question is legitimate;
> treating it as an unavoidable physical-representation blocker is not.

That was my framing, in `V31-T2-REPLACEMENT-PLAN-20260907.md`, and it is wrong.
`win_gen_of_slot()` — already in the RTL — reconstructs the residual generation
of **dead** slots as well as live ones, so the exact current predicate can be
preserved *and* `gen_q` deleted. The policy ruling and the table removal are two
separate changes, and I had welded them together.

## And my read inventory was incomplete — by a regex

The brief's audit lists **seven** functional readers. Mine found four.

The three missed are the C1 snapshots at lines 1329, 1370, 1409:

    c1t_tgen_q <= gen_q [c0t_slot_q];
    c1a_tgen_q <= gen_q [c0a_slot_q];
    c1f_tgen_q <= gen_q [c0f_slot_q];

**They were missed because of a space.** My search was `gen_q\[`; the source
writes `gen_q [c0t_slot_q]`. The brief anticipates exactly this — *"Search in a
way that catches whitespace and both current/next-state arrays"* — and supplies
`\bgen_(q|n_c)\s*\[`, which finds all seven.

This is the documented failure mode in CLAUDE.md's own words: a pattern that
matches nothing reports no problem, and **nobody audits good news.** The
inventory it produced was used to argue what could and could not be removed.

## What I am doing, in the brief's stated order

1. **Exact generation-reconstruction migration first** — all seven readers to
   `win_gen_of_slot()` / `hist_gen()`, preserving today's predicate exactly. C1
   snapshots captured from the **same pre-edge allocator state**, not recomputed
   a clock later from a moved cursor.
2. `fn_gen_q <= gen_n_c[tail_next_c]` classified and rewritten explicitly — it
   reads NEXT-state data, so it is not a mechanical rename.
3. `gen_q` retained **only** in a synthesis-excluded verification section,
   updated from its **own** old-style per-slot recurrence — not from the helper
   it exists to check, which would make the equivalence assertion circular.
4. **Then** the live-owner authority checks, as a separately testable change.
5. A matched owner fit.

Not doing: switching to terrain, restarting the architecture, relaxing resource
gates, or equating source edits with ALM gains. The in-progress island fit is
being left to finish; the completed texture repairs stand.
