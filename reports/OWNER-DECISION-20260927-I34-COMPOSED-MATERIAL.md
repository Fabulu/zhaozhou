# OWNER DECISION — 2026-09-27 — BUILD `TERRAIN.COMPOSED_MATERIAL`

**Owner ruling. This is standing direction, not a packet's choice.** It answers
the escalation raised by `I34CLOSE` and recorded in `FINDINGS-I34CLOSE.md`.

---

## THE RULING, IN THE OWNER'S WORDS

> **Take option 1.**
>
> **Build `TERRAIN.COMPOSED_MATERIAL` and complete directive §13.7's
> `-FieldActive` positive console path.**
>
> **Do not retire or refuse `COMPOSED_MATERIAL` into the spec at this stage.** The
> bandwidth measurement is valid evidence against the presently assumed
> publication shape, but **it is not authority to supersede a destination I
> explicitly left live when I retired `COMPOSED_NAV`.**
>
> **NAV is not the precedent for silently removing this.** NAV received an
> explicit owner architecture decision **and a real replacement production
> capability**. No equivalent supersession exists for `COMPOSED_MATERIAL`.
>
> **The completion rule here is deliberately strict: `I34` must become TRUE IN
> THE ASSEMBLED CONSOLE, not close because the document was edited to match what
> happens to exist.**
>
> Acceptance must include a real `-FieldActive` console run with anti-vacuity:
>
> * a real production Field program is **installed and executed**;
> * the field **covers the intended terrain**;
> * its material write produces a value that **cannot equal the authored baseline
>   by accident**;
> * that composed material **reaches the intended production consumer**;
> * the **uncovered/control form restores the authored result**;
> * the **no-field forms remain unchanged**.
>
> **Build the commissioned `COMPOSED_MATERIAL` path and measure its actual
> cost.** Do not silently cut field capacity, semantics, update behavior, or the
> destination in order to meet the bandwidth budget.
>
> **If the completed implementation proves that the specified publication
> semantics cannot meet the frame/bandwidth contract, STOP with that measured
> conflict and escalate it to me.** That would justify a new owner architecture
> decision. **The existing bandwidth estimate alone does not.**
>
> **Do not launch another full-console fit for this.** Close the functional
> obligations first; **targeted local measurement is fine when required by the
> implementation.**
>
> **`I34` closes only when BOTH `COMPOSED_MATERIAL` and the positive
> `-FieldActive` console path are genuinely exercised.**

---

## WHAT THIS SUPERSEDES

**It supersedes `FINDINGS-I34CLOSE.md`'s recommendation** to record a
COMPOSEPUB-shaped refusal of `COMPOSED_MATERIAL` into `spec/memory_rules.md`.
That recommendation was correctly escalated rather than taken, and it is now
**declined**. The R64 precedent and the consumer-test parallel do **not** carry,
for the reason the owner gives: **NAV was replaced, not removed.**

**It does NOT supersede** the bandwidth measurement itself, which stands as
evidence about *the presently assumed publication shape* — and which may yet
become the basis of a new decision, **but only after the path is built and the
conflict is measured rather than estimated.**

## WHAT IT MEANS FOR THE REGISTER

**`I34` cannot be closed by editing prose**, and this is now doubly binding,
because the entry's `BOUNDARY` head is **already known to be stale**:
`terr_pt_fld_valid_i`, `_ready_o` and `_height_i` occur in
`zhao_console_core.sv` five times, **every one a comment, zero as a port
declaration**, while `completion_register.py` classifies the entry by
regex-matching that word.

**That makes a prose edit the easiest way to move the number and the one thing
forbidden.** The owner's sentence is the governing test: *"`I34` must become true
in the assembled console."*

## THE STANDING BAR FOR ANY PACKET ON THIS ENTRY

1. **The six acceptance clauses above are a CHECKLIST, not a summary.** Each is
   separately demonstrable and each must be demonstrated.
2. **Anti-vacuity is the point of all six.** The decisive one is *"a value that
   cannot equal the authored baseline by accident"* — this console has twice
   shipped a check that passed on a constant, and once counted cells instead of
   values.
3. **The control forms are half the evidence.** An uncovered field restoring the
   authored result, and the no-field forms unchanged, are what prove the covered
   run measured the field rather than the weather.
4. **Cost is MEASURED, not estimated, and it is reported either way.** Targeted
   local measurement is authorised; a full-console fit is not.
5. **A measured impossibility is a finding and an escalation — never a licence to
   trim.** Capacity, semantics, update behaviour and destination are all fixed.
