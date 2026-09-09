# TASK LOG — RUN-20260909-1512-manafold-p14-by-eye-review

**Lane:** `C:\programmieren\zencrifice\manafold-p14-review\{zhaozhou,Upheaval}`
at `origin/main` (zhaozhou `bb3dfce6`, Upheaval `4e3d8cc`).
**Mandate:** BY-EYE REVIEW of the published pass-14 Manafold bank. Say whether
the creature is good, not whether the gates passed.

---

## Progress timeline

* **1512** Run opened. Read `Upheaval/CLAUDE.md` (art law, real exit codes,
  looking-is-cheapest), `CONCEPT-DESCRIPTION.md`, `OWNER-DIRECTION-10`,
  `PASS-14-PLAN.md` section 4 (the SEES lines), `PASS-14-FINDINGS-FACE.md`.
* **1516** Looked at all three concept sheets directly (Front, Side,
  Description inset) rather than inheriting a summary. Recorded my own reads:
  fat symmetric lens ~2.6–3:1, star ~0.55 of the lens, tight Λ with inner tips
  nearly touching, closed loop with a window about the size of the body,
  four gentle knuckles in a continuous skin.
* **1520** Verified the bank decodes: all 28 webm, 384x240 @ 60, frame counts
  recorded. `taunt3` 368, matching the plan.
* **1525** `inspect` f150 at 4x then 7x. The face is a pair of daggers with a
  small star. Confirmed R1 did not execute (FINDINGS-FACE section 2 says so
  outright).
* **1535** `hover` f37/f393/f573. Confirmed the eye degenerates to a rim sliver;
  FINDINGS-FACE's "no eye at all" at f393 is **overstated** — at 7x there is a
  thin blade. Recorded as a correction, not a fault.
* **1540** Built an ink-outline bbox instrument. **Ran it on a known-negative
  first (item 40): it fired on terrain shadow, 35 px on a creature-free strip.
  DISCARDED.** Cropped by eye instead.
* **1550** `hover` full contact sheet, every 15. Found the dead back half
  (f240–f600). Native-vs-2x pair on f393 confirms the blotch shading reads at
  native, not just under magnification.
* **1600** `taunt3` every-8. Two holds confirmed present. **Raised "three eyes"
  off the contact sheet; a 5x crop REFUTED it — two eyes.** Recorded.
* **1610** `death-drop`: sag confirmed, and every-4 sampling of f0–f136 shows
  **no drop at all**.
* **1620** `hasty`: contact sheet, then 4x on f228–f238. **The creature exits
  the right edge. R5's claim REFUTED.** Cross-correlated the horizon profile to
  test whether the follow-cam shipped: +93 px terrain shift, residual 3.1 — it
  did ship, and is undertuned.
* **1625** Built a magenta creature mask to measure the traverse. **Known-
  negative: 3701 px on a creature-free strip of sky. DISCARDED.** Second failed
  instrument of the session; both recorded in the review.
* **1635** `drift` left-edge crop — clip confirmed, **and the single most
  valuable frame of the review appeared**: both eyes visible, the face-on one a
  fat almond with a big star, the raking one a dagger. R3 before R1.
* **1645** `blown`: 96-frame plum egg at the apex, clipped by the frame top.
  `channel` f150–f190: ring chords confirmed on squash.
* **1700** Direction 10 rung plates, both skies. The two backdrops disagree;
  recommendation is a per-backdrop halo constant, MID core on both.
* **1715** Last-frame survey of all 28 clips. **Refuted my own "the loop is
  collapsed across the bank"** — it is open in ~22 of 28. Found the wrap ghost
  still present on published `hasty`/`flight`/`taunt3`.
* **1725** `trick` handstand, the comedy benchmark. Wrote the review.

## Decisions made

* Reviewed the **published webm only**. Source was read exactly once, to check
  whether `hasty`'s follow-cam existed at the shipped commit — not to review it.
* **Started no build and no render.** The load rule permits a single renderer
  but the ring-count experiment needs a compiler, so it is handed to the next
  lane as a named hypothesis rather than run here.
* Every presence metric run on a known-negative first. Two of three failed and
  were discarded; only the horizon cross-correlation survived and it is labelled
  as such in the review.

## Files created

* `Upheaval/creature/Manafold/PASS-14-REVIEW.md`
* `Upheaval/creature/Manafold/pass14-review-plates/` — 13 plates, indexed in the
  review's section 7.

## Next steps

Handed to the next pass in the review's section 4: R3 before R1; stop holding
beats on the back of the head; re-axis `blown`'s tumble; raise
`kU02HastyBiasX`; turn `kBodyRings`; chase the wrap ghost on the published
webm; one framing-and-scale pass over the deaths, `blown` and `fall`.

## Lane disposition

`manafold-p14-review/` can be deleted once this run and the review are pushed.
No background work was started; nothing is running.
