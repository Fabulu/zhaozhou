# SPEC v1: Manafold pass 20 -- Owner Direction 21

**Run ID:** RUN-20260920-0544
**Created:** 2026-09-20 05:44 UTC+02:00
**Status:** Closed 2026-09-20 after the independent review's block was cleared
**Previous Version:** N/A

---

## Objective

Owner Direction 21, three items and an acceptance rule:

1. The rear nodule moves too much: it rips a big piece out of the body and
   stretches the skin. Reduce the motion and/or the reach of its influence, and
   make the gate catch surface strain in the socket region with a fired control.
2. A new KNEADING beat: the middle-top carrier (B) sometimes travels down far
   enough to become the LOWEST ball, then returns. C2, authored, on ALL
   animations, generalised from Taunt III's crown shuffle rather than invented
   as a second system.
3. The mana particles must visibly REACT when it happens.
4. Art acceptance is visual, at native resolution, on the complete motion.

Success is: item 1 repaired at the root with a fired control, item 2 delivered
on every animation that can host it AT THE SHIPPING VALUES, item 3 visible by
eye, no bound relaxed, and every legacy toggle byte-exact against pass 19.

---

## Scope

**In Scope:**

- The rear chain solve (arc vs chord), the kneading dip mechanism, the fold's
  reaction to it, and the gates over all three.
- Gate instruments: their bounds, their controls, and whether each can fire.

**Out of Scope:**

- The 22-subject bank render, the encode, the merge to main and the deploy.
  Publication is deliberately withheld until the pass is finished and reviewed;
  the standing bestiary authorisation covers a FINISHED pass, not a file save.
- Waves E/F items carried from version 18 (the history mist, the Flight phrase,
  the Trick full turn).

---

## Constraints

- **No bound may be relaxed**, the C-E attachment bound above all: pass 20
  exists because that attachment tore.
- **Legacy toggles byte-exact against pass 19.** The contract is
  `ZHAO_U02_KNEAD_DIP_PM=0 ZHAO_U02_REAR_BOW=legacy`.
- **Every control fires in its own category**, and a counter reading zero is a
  claim to be checked, not evidence.
- **No gate may override a shipping constant and then quote the figure.**
- Art values are chosen BY EYE and only bounded by gates; a gate never picks a
  value.

---

## Don't Retry

*Failed approaches, so they are not re-learned after compaction.*

- **Do not widen `kSpanStretchMaxPm[C-E]`** to admit the dip. Considered and
  refused by the coordinator: it is the attachment guard, and this pass exists
  because the attachment tore.
- **Do not ship the dip off.** The owner asked for it on all animations.
- **Do not use the A-C CHORD as the dent aim's reference axis.** It is the
  swing's own exact rotation axis and looks right on paper, but it is not
  perpendicular to the segment and the aim's +Y-component drop mangles it:
  measured G9 9.773 at depth 2200 and 164.5 at 6000, against the triangle
  normal's 7.591 and 15.9.
- **Do not conclude that the ranking and the continuity ceiling trade
  monotonically.** They appeared to (4 clips at 7.77 deg, 19 at 50.96), and it
  was a property of the AIM's axis quantisation, not of the mechanism. With
  `arc_from_y_about` the trade disappears.
- **Do not measure "did the dip return" as B's own vertical travel.** Every
  other layer dominates it; a permanently stuck dip still measures 280-520 mm.
- **Do not measure the dent pin as C's POSITION.** `loop_walk(g, 4, ...)`
  composes HingeC's rotation after advancing the position, so the position is
  structurally independent of the pin. Measure the FRAME.
- **Do not make the particle reaction an addition to the `agit` scalar.** The
  fold already runs near the top of it; the result measured present and was
  invisible (588/600 frames byte-identical).

---

## Open Questions

- The bank render, encode and publish are the next session's, once the owner has
  looked at the knead. The pass is finished in source and evidence.
- `manafold_rear_audit.cpp` parses six authoring-ladder environment variables
  with bare `std::atoi`, so a malformed value silently becomes 0 rather than
  returning RC 2. Recorded, not fixed; the judged-configuration banner makes the
  consequence visible.
