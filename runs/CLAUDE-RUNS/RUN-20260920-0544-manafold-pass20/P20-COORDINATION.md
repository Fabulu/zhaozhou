# Manafold pass 20 — coordinator record

Decisions and directions given by the coordinating session, in order. The
implementer's and architect's own reports hold the engineering; this file holds
who decided what, and why, so the next session does not have to reconstruct it.

## Owner direction and its correction

Direction 21 first read as an emergence breach ("rips the connecting part out of
its body"). The owner corrected it the same day, verbatim: *"the rear doesn't
leave the body, but it rips a big piece out and it stretches too much, which
leads me to conclude there's too much motion in the back nodule."* The
correction reached the implementer while it was still diagnosing, so no gate was
built for the wrong reading. The superseded reading is kept in the direction
file for the record.

The owner also named the reference for the new beat: *"nodule taunt already does
the kneading motion at times"* — slot 21 / Taunt III.

## Coordinator decisions

1. **Item 1 is a defect in the motion AND in the gate.** Pass 19 shipped
   128/128 green while the rip was visible, so the gate was treated as part of
   the fault from the start. The diagnosis bore that out: every rear metric was
   built from ring centroids, which cancel a surface fold, and `mmeshcheck`
   reads the bind mesh — no leg measured a posed surface.
2. **Refused: widening `kSpanStretchMaxPm[C-E]`.** Getting B strictly lowest
   would have needed a 19% overshoot of that bound. It feeds the attachment
   guard, and pass 20 exists because that attachment tore. A bound protecting
   attachment is not relaxed to admit a new beat. Recorded in
   P20-GATE-CHANGES.md §7 as considered and rejected.
3. **Refused: shipping the dip off.** The owner asked for it on all animations;
   "ships off" is not an outcome. Each packet had to either ship it or state
   exactly what blocked it.
4. **Owner decision requested once**, when B-strictly-lowest needed a solver
   change rather than a knob: publish pass 20 with the repaired rip and the
   visible knead and defer the dip, hold and change the solver first, or allow
   the bound overshoot. The owner chose **hold and change the solver first**.
5. **Architecture before implementation** for the solver change (a FABLE
   architect, read-only), per the standing creature-pass process. It earned its
   packet twice: revision 1's cheap falsifying experiment killed the first
   mechanism in ten minutes with no render, and revision 2 found that the
   claimed pin leak was really an off-by-one-bone chain walk, which had also
   produced the "3x" compaction estimate.
6. **A CRC scare was resolved, not chased.** The implementer reported that
   pass-19's recorded CRCs did not reproduce in this tree. They do:
   `0x40E1DBF1` / `0x7E4F8487` in P19-IMPLEMENTATION.md are PRE-REVIEW values
   recorded before the reviewer's End-ball fix (1eb115e4). The authoritative
   receipt, P19-FINAL-BANK-INTEGRITY.md, records exactly the CRCs this tree
   reproduces. No integrity problem; the chase was called off.
7. **Gate thresholds were not allowed to absorb art.** Where a threshold moved,
   the implementer had to state what it protected before, what it protects now,
   why that is not a loosening, and which control fires on the genuine defect.
   The final depth (2200) was chosen to fit G9's unmoved 8° ceiling, not the
   other way round. Three inert instruments surfaced during the pass (R4's
   control had silently stopped firing; a duck was scaled by 1e6 instead of
   1e3), which is why the independent review was briefed to assume there are
   more.

## Process notes

* No Qwen this pass (the owner reserved it for another agent). GPT/Codex quota
  exhausted until 2026-09-25, so the implementer and reviewer are Opus, run one
  at a time, with the coordinator organizing only.
* Publication is deliberately deferred to after the independent review; the
  owner's standing authorisation covers publishing a finished pass, and this one
  is not finished until the review and the exact bank exist.
