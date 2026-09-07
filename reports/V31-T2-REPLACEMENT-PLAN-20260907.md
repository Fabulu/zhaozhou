# T2's RTL half: every `gen_q` site, and what replaces it

*2026-09-07. Written after the fit, not before — §6.8 says "the fit must decide
the net ALM and timing benefit", and it has now decided part of it. The identity
itself is already proved in `texture_v3_window_identity.cpp` (28 checks); this is
the mechanical half.*

---

## Why this is now evidence-led rather than speculative

The `zhao_texture_v3own` fit put **`gen_q[40][4] → ev_err_issue_o[*]` and
`→ iss_q[*][3]`** among its worst path families, and a 64-way select over
`gen_*` accounted for **71% of the single worst path**. §6.8 predicted the
mechanism in the abstract:

> events no longer need a **64-way generation select** and live-bit select in
> addition to their other scoreboard reads. There is **one bounded arithmetic
> identity check per event lane** instead.

`gen_q` is 64 × 8 = **512 flip-flops** and, per the census, sits in the 98% of
ALUTs that are in the top itself.

## The window is already three-quarters built, which changes the risk

Before planning a replacement it is worth checking what is actually there. All
three of §6.1's fields already exist in the RTL — in their **low bits**:

| §6.1 | present today as | note |
|---|---|---|
| `alloc_ticket` | `tail_q`, `SLOTW` = 6 bits | advances on `adm_fire_c` |
| `retire_ticket` | `emit_q`, 6 bits | `if (out_fire_c) emit_q <= emit_q + 1` — **increments only** |
| `used` | `live_cnt_q`, `CNTW` = 7 bits | `live_cnt_q + adm_fire_c - out_fire_c` — the same update §6.1 specifies |

Two consequences.

**§6.3's invariant already holds by construction.** *"Retirement is strictly
oldest-first"* is not an assumption to be imposed on this design; `emit_q` is a
pointer that only increments, and line 1536 already asserts the retiring owner is
`emit_q` and live. The model test shows the identity **fails** if a hole is
punched, so this mattered — and it is satisfied.

**So T2 is not "introduce a new representation".** It is: *extend two existing
6-bit pointers to 14 bits, and delete the 512-flop table that stores per-slot
what those 16 extra bits hold.* `gen_q[64][8]` plus `live_q[64]` is 576 bits of
state expressing what 16 bits of pointer already imply — which is precisely the
576 §6.8 names, arrived at from the other direction.

That reframing matters for risk: the change becomes an **extension** of live,
tested machinery rather than a parallel mechanism that must be swapped in.

## The two primitives everything reduces to

From §6.2's encoding (`slot = ticket[5:0]`, `generation = ticket[13:6]`) and
§6.1's interval:

```systemverilog
// Is a supplied public token live? -- 6.1, and 6.5's validation pipeline.
//   ticket    = {generation, slot}  (internal order; decode the public token first)
//   live(t)   = ((t - retire_q) & 14'h3FFF) < used_q
//
// What generation does a LIVE slot currently carry?  -- one 6-bit subtract and
// one 14-bit add, no 64-way select:
//   ticket_of_slot(s) = retire_q + ((s - retire_q[5:0]) & 6'h3F)
//   gen_of_slot(s)    = ticket_of_slot(s)[13:6]
```

`ticket_of_slot` is exact because slots repeat every 64 and `used ≤ 64`, so
exactly one ticket in `[retire, retire+64)` carries any given slot.

## Every site, classified

Twelve live sites. They fall into three groups, and **seven of the twelve are the
same question**.

| # | line | current | replacement | group |
|---|---|---|---|---|
| 1 | 267 | `adm_gen_c = gen_q[tail_q] + 1` | `alloc_q[13:6]` — admission already *is* the next ticket | **A: free** |
| 2 | 268 | `wrap_block_c = gen_q[tail_q] == 8'hFF` | `&alloc_q` (all 14 bits set) | A |
| 3 | 376 | `wrap_at_tail_p1_c` | folds into #2 — one wrap test, not two | A |
| 4 | 459 | `gen_q[iss_t_slot_c] == iss_t_gen_c` | `live(iss_t_ticket)` | **B: validity** |
| 5 | 470 | `gen_q[iss_a_slot_c] == iss_a_gen_c` | `live(iss_a_ticket)` | B |
| 6 | 651 | `gen_q[c4t_slot_q] == c4t_gen_q` | `live(c4t_ticket)` | B |
| 7 | 655 | `gen_q[c4a_slot_q] == c4a_gen_q` | `live(c4a_ticket)` | B |
| 8 | 996 | `gen_q[i] == c4t_gen_q` *(inside the 64-way loop)* | hoist: `live(c4t_ticket)` once, then index by slot | B |
| 9 | 998 | `gen_q[i] == c4a_gen_q` *(ditto)* | same | B |
| 10 | 1000 | `gen_q[i] == c4f_gen_q` *(ditto)* | same | B |
| 11 | 1020 | `gen_q[i] == cmb_owner_o[GENW-1:0]` *(ditto — T4's)* | same | B |
| 12 | 1384 | `g0_owner_q <= {fetch_q, gen_q[fetch_q]}` | `gen_of_slot(fetch_q)` | **C: lookup** |
| 13 | 1558 | `gen_q[fn_slot_q] == fn_gen_q` | `live(fn_ticket)` | B |

**Group A (3 sites) disappears.** Admission and the wrap fence stop needing a
table at all — they are properties of `alloc_q`. That also deletes the structure
the fit just named as 71% of the worst path, and #3 is the line rewritten this
morning, which becomes unnecessary rather than merely cheaper.

**Group B (9 sites) is one function.** All nine ask *"is this token still the
live occupant of its slot?"* — §6.5's ticket validation. Four of them
(996–1020) currently sit **inside** the per-owner loop, so they are 64-way
comparisons today; hoisting them is exactly §6.8's "one bounded arithmetic
identity check per event lane".

**Group C is one site.** Only `g0_owner_q` genuinely needs "what generation does
this slot carry", and it is a 6-bit subtract plus a 14-bit add.

## A correction to Group A: site 3 is not as simple as sites 1 and 2

Sites 1 and 2 read `gen_q[tail_q]` — the slot being allocated **now** — and the
step-1 assertion establishes that this always equals `alloc_gen`. Those two are
straightforward.

Site 3 (`wrap_at_tail_p1_c`, line 376) reads `gen_q[tail_p1_c]` — a slot
**ahead** of the tail, which has not yet been reallocated in the current pass and
therefore still carries the **previous** generation, `alloc_gen − 1`. It is not
`alloc_gen`, and writing it as such would put the wrap fence one whole namespace
out.

This is exactly the kind of off-by-one that a fence hides until a 16,320-cycle
wrap, so it is written down before the code is touched rather than discovered
in a bench. Two consequences:

* The step-2 rewrite of site 3 must be derived, not pattern-matched from
  sites 1 and 2.
* The step-1 assertion should be extended to cover `gen_q[tail_p1_c]` against
  `alloc_gen − 1` **before** site 3 moves, so the relation is proved on real
  traffic first — same discipline that made site 1 safe.

Under a full window the whole question dissolves — per-slot exhaustion and
namespace wrap coincide, because allocation is strictly round-robin so
`gen_q[s]` is just `floor(ticket/64)` for the ticket that allocated `s`, and
§6.6's fence lands at the 16,320 allocations the model test counts. But that is
true of the *finished* replacement, not of the intermediate state where the
table and window coexist, and the intermediate state is where the bug would go.

## What must not be assumed

* **§6.8's own caution stands.** *"The net register reduction is smaller than
  576."* Removing `gen_q` (512) and `live_q` (64) adds `alloc_q`, `retire_q`,
  `used_q`, fence state and validation-pipeline registers. This plan does not
  predict a number, and the sites above are a *decomposition*, not a saving.
* **§6.3's invariant is a precondition, and it is already tested.** The identity
  holds only while retirement is strictly oldest-first with no interior holes —
  the model test punches a hole deliberately and shows the representations
  diverge. If per-owner cancellation is ever a product requirement, §6.3 says to
  add ordered tombstones rather than quietly assume the hole away.
* **Group B may become its own critical path.** §6.8: *"If the window validation
  itself becomes a critical path, register it or share predecoded high/low
  comparison terms."* That is precisely the mistake made and then measured today
  with the fence, so the validation should be shared across lanes from the
  start rather than instantiated nine times.
* **T2 says to retain the old readiness and wide packet queues** for this
  attribution point, so this change should not be bundled with T5.

## Order of work

1. Add `alloc_q` / `retire_q` / `used_q` **alongside** the existing table, with
   an assertion that the two representations agree on every event — the RTL
   equivalent of the model test, and the thing that makes the swap safe.
2. Move Group A. The wrap fence and admission stop reading the table.
3. Move Group B, one shared validation, hoisting the four in-loop comparisons.
4. Move Group C.
5. Delete `gen_q` and `live_q`, then refit and compare on matched scope.

Step 1 is what makes the rest reversible one site at a time, which is T1's
instruction — *"preserve a tested identity-only comparison rather than one giant
patch"* — applied to the RTL.
