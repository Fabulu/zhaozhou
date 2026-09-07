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

> **AND THAT PARAGRAPH IS ALSO WRONG — the RTL said so within the hour.**
>
> It was turned into an assertion rather than into code, and the assertion
> failed on the first run. Allocation is strict round-robin and `alloc_gen`
> increments when the tail wraps 63→0, so within a pass:
>
> ```
> gen_q[s] == alloc_gen        for s already allocated this pass  (s <  tail_q)
> gen_q[s] == alloc_gen - 1    for s still ahead                  (s >= tail_q)
> ```
>
> `tail_p1` is ahead **except when `tail_q == 63`**, where it wraps to slot 0 —
> allocated at the *start* of this pass, so it holds `alloc_gen`. My rule was
> therefore wrong at exactly `tail_q == 63`: **the wrap boundary, the one case
> the fence exists for.**
>
> A fence that is wrong only at the wrap is wrong silently for 16,320
> allocations. Writing the belief as an assertion cost one build; writing it as
> RTL would have cost a fit and a bench that passes.
>
> **This is the whole argument for T2's assert-then-move order**, and it is now
> a demonstration rather than a principle. The assertion in the RTL states the
> full `gen_of_slot(s)` identity over all slots — which is what Groups B and C
> actually need — rather than the neighbour special case.

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


---

# STEP 2 LANDED for the measured limiter (`2b377444`)

The refit reordered this plan's own priorities. Group A was listed first because
it is simplest; the fit said otherwise:

    -0.950  gen_q[4][4] -> iss_q[52][0]      core->core, 91.32 MHz

That path is **Group B sites 4 and 5** — `gen_q[iss_t_slot_c] == iss_t_gen_c` at
lines 459/470, the ISSUE-lane validity checks. So those moved first, and the
plan's ordering is corrected by measurement rather than by preference.

Both lanes now evaluate §6.1's interval — a 14-bit subtract and compare on
registers — in place of two 64-way array selects, which is §6.8's *"one bounded
arithmetic identity check per event lane"* made concrete.

## Step 1 is what made this safe, and that is the point of the whole order

`a_win_live_matches_table` and its boundary-aimed twin have asserted exactly this
equivalence on **real traffic, every cycle, since step 1** — including the case a
uniform sweep barely reaches, which is why the second check walks `retire + k`
for `k = 0..63` across the live edge. The identity was proved before anything
depended on it.

## Verified

* **477 checks pass**, no assertion fired.
* **§6.2's trap fire-tested in RTL.** Building the ticket as `{slot, gen}` — the
  public order, which §6.2 warns against by name — fails **43 checks starting at
  case 1**. The model test searches for a counterexample to that trap; the bench
  now demonstrates it on the hardware description too.
* Verilator `-Wall` clean.

## What is left of the table

Seven readers, and the assertions still cross-check every one of them:

* **Group A** (3 sites) — admission and the wrap fence. Site 3 still needs its
  own assertion first, per the correction above.
* **Group B** remainder (4 sites) — the return lanes and the in-loop comparisons
  at 996–1020, which are the four that are 64-way today.
* **Group C** (1 site) — `g0_owner_q`.

**Timing benefit unmeasured.** The fit lane is busy with the perspuv refit, and
three source-reading predictions were falsified today, so no number is claimed
here. The next v3own refit compares against **87.37 reported / 91.32 core→core**
on matched scope.


## Why step 2 STOPS at the ISSUE lanes

The obvious next move is the four in-loop comparisons at 1032–1056
(`gen_q[i] == c4t_gen_q` and friends). It is not being made, for two reasons.

**They are not the measured limiter.** They feed `cmt_n_c` / `fdn_n_c`, and
neither appears in the refit's worst families — the named path was the ISSUE
lane, which is now on the window. Moving them would be a change made from
reading source, which is the thing that was falsified three times today.

**And the natural hoist is not behaviour-preserving.** Replacing
`(c4t_slot_q == i) && (gen_q[i] == c4t_gen_q)` with `win_live({c4t_gen_q,
c4t_slot_q})` silently adds a `live_q[c4t_slot_q]` term the original does not
have: a stale event arriving after release but before reallocation would match
the generation and be **accepted** today, **rejected** after. That may well be
more correct — the site's own comment calls it *"a fault-injection and
drain-boundary guard"* — but it is a semantic change wearing the costume of a
refactor, and it needs its own case rather than a ride on a timing fix.

An exactly-equivalent hoist does exist — lift `gen_q[c4t_slot_q] == c4t_gen_q`
out of the loop, one 64-way select instead of 64 comparisons — and that is the
right first move **if** the next refit names these paths. Measure, then move.


## The coverage gap step 2 opened, and closing it (case 4b)

Moving the ISSUE lanes to `win_live` introduced a guard whose **reject** path
nothing exercised. The bench drives those lanes only with live owners, so:

* the **accept** path was covered hard — mis-ordering the ticket fails 43 checks;
* the **reject** path was never reached at all.

An accept-only test cannot distinguish a correct guard from one that is
permanently true. Case 4 covered exactly this on the *return* lane and had no
issue-lane counterpart.

**Case 4b** adds a stale-generation issue, an issue for a slot that is not live,
and — so it cannot pass on a guard that refuses *everything* — a legitimate issue
and return that must still complete. `ev_err_issue_o` counts the refusal
(`iss_tmu_valid_i && !iss_t_ok_c`) so the property is observed, not inferred from
an absence.

**Fire-tested, and the result makes the case for itself.** With `win_live`
forced to `1'b1` — a guard that fails open, which is how this kind of guard
actually fails — the bench reports:

    2 of 481 checks FAILED
    FAIL: a stale GENERATION on the issue lane is refused        expected 1, got 0
    FAIL: and an issue for an owner that is not live at all      expected 2, got 1

**Exactly the two new checks, and nothing else.** All 477 pre-existing checks
pass with a permanently-true guard. Without case 4b, T2 step 2 could have
disabled owner validation on the issue path and left a green bench behind it.


---

# GROUP A LANDED, and the mistake I made in prose is now caught in RTL

All three Group A sites now read the window instead of the table:

| site | was | is |
|---|---|---|
| `adm_gen_c` | `gen_q[tail_q] + 1` | `alloc_gen` |
| `wrap_block_c` | `gen_q[tail_q] == 8'hFF` | `alloc_gen == 8'h00` |
| `wrap_at_tail_p1_c` | `gen_q[tail_p1_c] == 8'hFF` | `(tail_q == 63) ? alloc_gen == 8'hFF : alloc_gen == 8'h00` |

Two 64-way selects of an 8-bit array leave the design.

## The assertions were kept honest, which mattered immediately

`a_win_gen_matches_table` compared `adm_gen_c` against `sh_alloc_gen_q`. Group A
made `adm_gen_c` **be** `sh_alloc_gen_q` — so that form silently became a
**tautology**, an assertion that cannot fail. It now compares against the
**table**, which is what the derivation actually claims, and two more were added
on the same principle for sites 2 and 3.

## Site 3's fire test is the whole argument for this method

The mutant is the **naive** form — `alloc_gen == 8'h00`, dropping the
`tail_q == 63` case. That is not an invented error: it is precisely what this
plan asserted in prose earlier today, before an assertion corrected it.

    %Error: zhao_texture_v3own.sv:1612: Assertion failed in
            a_win_wrap_p1_matches_table

**Caught immediately.** A fence that is wrong only at the wrap boundary is
otherwise silent for 16,320 allocations, and no functional check in the bench
distinguishes the two forms — the mutant's *checks* all pass; only the assertion
fires.

481 checks pass on the real derivation, no assertion fired.

## What is left of the table

Five readers: the four in-loop comparisons at 1032–1056 (Group B remainder,
deliberately deferred — see above, the natural hoist is not behaviour-preserving)
and `g0_owner_q` (Group C). `gen_q` cannot be deleted, and its 512 flip-flops
cannot be recovered, until those five go.
