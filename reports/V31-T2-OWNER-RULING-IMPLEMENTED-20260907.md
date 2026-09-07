# The T2 owner-lifetime ruling: implemented, and both halves proved necessary

*2026-09-07, against `ZHAOZHOU_T2_ARCHITECTURE_DECISION_BRIEF_2026-09-07.txt`
(`8969dcaf`). Acknowledged at
`fpga/rtl/texture/OWNER-DIRECTION-T2-LIFETIME-2026-09-07.md`.*

---

## The ruling

> An owner instance has authority from its accepted admission until its ordered
> external output transfer. Once that transfer retires the owner, later events
> for that instance are stale. **Matching a slot's residual generation bits does
> not extend the owner's authority after retirement.**

## Done, in the brief's order

**Step 1 — the exact migration.** All **seven** functional readers moved to
`win_gen_of_slot()` with the predicate unchanged, including the three C1
snapshots. `fn_gen_q` classified (its only consumer was an assertion) and moved
to verification. **`gen_q`'s 512 flip-flops deleted.** A literal `vgen_q`
survives only under `` `ifndef SYNTHESIS ``, maintained by its **own** old-style
recurrence — never from the helper it checks, so the equivalence assertion is not
circular.

**Measured, mid-flight from the refit's map stage:**

| | previous | this refit |
|---|---:|---:|
| MAP registers | 4,310 | **3,750** |
| virtual pins / memory bits / DSP | 952 / 20,640 / 0 | identical |

**−560 against a recorded prediction of −560**: −512 (`gen_q`) −64 (`ftc_q`,
§13.1) +16 (window counters). All three confirmed independently.

**Step 4 — the live-owner authority checks**, as a separate commit because the
brief asks for it "as a separately testable change". C2 now requires **both**
snapshot identity and current full-ticket membership, on all three lanes.

## Both of §8.1's counterexamples are REACHABLE, and each half catches one

This is the part worth the space, because it was not obvious and I got it wrong
twice on the way.

| | schedule | reached at | caught by |
|---|---|---|---|
| **V03** future token | return for an owner not yet admitted; admission lands before the claim | offset 0 | the **snapshot** half |
| **V04** retired token | duplicate captured while live; retirement lands on its C2 | offset 0 | the **current** half |

**Proved by mutation, in both directions:**

* remove the **current** half → case 4e (V04) fails: the retired token is
  accepted;
* remove the **snapshot** half → case 4h (V03) fails, **1 of 520**: the future
  token is accepted.

So neither half is redundant, and the ruling's insistence on two time points is
not belt-and-braces — it is exactly two different defects.

## Where I was wrong, twice, the same way

**First**, I reported V04's schedule as unreached and leaned toward "structurally
prevented". My construction released `out_ready` **before** injecting the
duplicate, so the owner had already retired by capture — the wrong schedule
entirely. Ten offsets "survived" and I believed them. Reversing the order hit it
immediately.

**Then I repeated it for V03**, reporting its detector as never firing. It fires
on the first attempt once the stimulus is actually written.

The lesson is sharper than "a detector that has not fired has not been tested":
**a detector that has not fired may mean the stimulus was never written**, and
reporting that as evidence of absence is the same error as trusting a regex that
matches nothing.

## The brief also corrected two errors of mine

1. *"The earlier claim 'those four guards must change semantics before the table
   can be deleted' was too strong."* Correct — `win_gen_of_slot` reconstructs
   dead slots too, so the exact predicate and the table removal are independent.
   I had welded them.
2. My read inventory listed four functional readers; the brief's lists seven. The
   three C1 snapshots are written `gen_q [c0t_slot_q]` — **with a space** — and
   my pattern was `gen_q\[`. The brief supplies `\bgen_(q|n_c)\s*\[` for exactly
   that reason.

## Coverage against the brief's matrix

| | status |
|---|---|
| V01 exact reconstruction | continuous, against an independently maintained table; `tail==63` mutation fire-tested |
| V02 interval membership | **exhaustive** — 16,384 head residues × 65 occupancies at both boundaries |
| V03 snapshot validity | **case 4h**, reachable, snapshot half proved necessary |
| V04 identity at claim | **case 4e**, reachable, current half proved necessary |
| V05 stale after retirement | TMU / AUX / FINAL separately (4e, 4f), each checking the **row contents** after refusal — a write would leave 0xE0E / 0xA0A in the COMBINE row and does not. Still short of the write-enable **pins**, which need a probe port |
| V06 slot reuse | **case 4g** — old token before / on / after both edges |

**522 checks**, from 477 at the start of the day.

## Not done

* **V05's write-enable PINS.** The cases now observe the write's *consequence*
  — the bank row still holds the owner's own result after a refused late
  return — which is arguably the better evidence, since a harmlessly
  toggling enable is not the harm. The pins themselves still need a probe
  port or a hierarchical reference, and that remains undone.
* **§8.3's claim-to-write lease** at the physical write enables. The brief warns
  it must not become "an uncontrolled late combinational window predicate
  immediately before a bank write-enable", which is a real design constraint and
  not a rename.
* **ALM and Fmax** for the migration — the fitter is still placing, and no
  performance number is predicted. Today's record is unambiguous on that:
  accounting predictions land exactly, performance predictions did not.
