# T2's identity comparison, proved — and one correction to §6.1

*2026-09-07. `tests/texture/texture_v3_window_identity.cpp`, 20 checks. Pure
C++, no DUT. Written while the `zhao_texture_v3own` fit held the RTL, which is
also why it is a model test: it is entirely outside that closure.*

---

## What T2 asked for and what this is

> **T2 — Sequence-window identity.** Add independent literal-owner/generation
> checks, **then** replace per-slot generation access. […] Prove full token
> order, membership and snapshot races.

and T1's constraint on how:

> preserve a **tested identity-only comparison** rather than one giant patch.

So the comparison comes first and the RTL replacement second. This is the
comparison. It is deliberately free of a DUT — the claim that a bounded interval
equals a 64-entry live/generation table is **mathematical, not timing** — which
means it stays valid while the owner is rebuilt underneath it.

## The result

Over 60,000 admit/retire operations and **more than a million whole-token-space
membership comparisons**, through many namespace wraps:

* the interval `live(t) = unsigned_14(t − retire) < used` and the literal
  64-entry table agree on **every one of the 16,384 tokens**, at every sweep;
* the per-slot generation byte always equals `ticket[13:6]`;
* §6.2's initialisation gives first token **slot 0, generation 1**, and slot 0
  returns after exactly 64 allocations with its generation advanced by one.

Membership is swept over the **whole** token space, not just live tokens. A
window that reported spurious live for *dead* tokens would pass any check that
only walked the live set.

## Three things demonstrated rather than quoted

* **§6.2's trap is real.** *"Do not apply a numerical subtraction directly to
  public_owner. Its slot bits are in the high position."* The test **searches**
  for a disagreement between naive public-token arithmetic and the decoded
  window, and finds one. Searched rather than asserted, so the check fails if
  somebody later "simplifies" the decode away.
* **§6.6's fence point, counted.** The first interval starting at ticket 64
  reaches the namespace wrap after exactly **16,320** allocations. The document
  states this; carrying a number across a document boundary is how it goes
  stale, so it is computed here.
* **§6.3's invariant is load-bearing.** Retiring an **interior** owner makes the
  two representations disagree. So *"retirement is strictly oldest-first"* is a
  **precondition of the replacement**, not a stylistic preference — which is
  exactly what §6.3 means by *"Do not quietly assume away holes."*

## The correction

§6.1 says:

> Both empty and full must be represented explicitly by `used`. **Pointer
> equality alone cannot distinguish them.**

The test was written to confirm that sentence and **it failed.** That is the
classic same-width-pointer FIFO caution, and it does not bite in this encoding:

* the ticket space is 14 bits — **16,384** — against a capacity of **64**;
* so `(alloc − retire) mod 16384` **equals `used`** in every reachable state;
* `alloc == retire` therefore happens only at `used == 0`. Full sits 64 apart.

**Pointer equality distinguishes empty from full perfectly well here, and `used`
is derivable.**

`used` is still right to keep — but for **§6.4's** reason, not §6.1's:

> Use pre-edge state for permission: `admit_allowed = epoch_open_q && !fence_q
> && (used_q < 64) && …`

That wants a **registered count**, not a 14-bit subtraction evaluated on the
admission path — and the admission path is precisely where this session already
measured the worst ten paths ending at `adm_ready_o`.

A correct field with a wrong justification is worth catching, because **the
justification is what the next person reasons from**: someone optimising for
area would read §6.1, find the ambiguity claim false, and delete the field —
losing the timing property nobody wrote down.

## What this does not claim

* Not that the RTL replacement is done. T2's second half — *"then replace
  per-slot generation access"* — is untouched, and it edits
  `zhao_texture_v3own.sv`, which is inside the running fit's closure.
* Not a timing or area result. The model says the representations are
  equivalent; it says nothing about what either costs.
* Not a full snapshot-race proof **in RTL**. The model now covers §6.4's rule
  (see below), which is the part that is a *decision*; the concurrent
  read/update behaviour of the actual scoreboard still belongs with T3's 8×8
  banks.


---

## Added later the same day: §6.4's snapshot race

T2's third obligation was *"prove … snapshot races"*, and the model now covers
it — because §6.4 states the race as a **deliberate conservative choice**, not as
a consequence:

> Use pre-edge state for permission … Admission and retirement may occur
> together when pre-edge `used` is below 64. At pre-edge `used == 64`,
> **initially refuse admission even if an output retires in that same cycle.**
> That conservative rule avoids a combinational downstream ready bypass and
> same-slot read/write complications.

Both readings are modelled so the difference is **visible rather than asserted**:
at the boundary the pre-edge reading refuses and a post-edge reading would have
permitted. That check is what makes the rule a real choice rather than a
restatement — if the two agreed, §6.4 would be describing nothing.

Below the boundary the simultaneous admit+retire is legal and verified to leave
the window consistent: `used` unchanged, both pointers advanced by one, the two
representations still agreeing on **every** token afterwards, the token admitted
in the racing cycle live and the one retired in it dead.

**28 checks, `g++ -Wall -Wextra` clean.** What remains for T2 is the RTL swap
itself — replacing per-slot generation access — which is where the ~477
flip-flops of `gen_q[64]` actually come back.
