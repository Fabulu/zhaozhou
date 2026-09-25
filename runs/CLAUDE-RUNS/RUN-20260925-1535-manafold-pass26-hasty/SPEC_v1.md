# SPEC v1: Manafold pass 26 — Hasty must read as being in a hurry

**Run ID:** RUN-20260925-1535
**Created:** 2026-09-25 15:35 UTC+02:00
**Status:** Complete
**Previous Version:** N/A

---

## Objective

Owner Direction 27: *"It is, however, not very hasty. Make it look like it's
actually in a hurry, both in facial expression and speed."*

Success is **visual**: Hasty at native, complete motion, against a before/after
— does the creature read as being in a hurry? Gates protect byte-identity,
contact, continuity and the rig; they do not decide the question.

---

## Scope

**In Scope:**

- `manafold-hasty` **speed**: traverse rate, cadence, lean, settle, clip length.
- `manafold-hasty` **face**: eye direction, eye size, blink cadence, brow.
- The **staging** those need (camera framing and follow), since the pass-25
  camera was sized for a traverse the clip no longer had.

**Out of Scope — closed by the owner in Direction 27:**

- Crackle's rear damping — *"Crackle is fine from what I can see."*
- Hover's front-spin channel — *"fine."*
- Hasty's loop-seam hitch **as a defect** — accepted because the creature
  traverses. A new figure is reported, not defended.
- Every other live subject: byte-identical.

---

## Constraints

- Named constants; **no bound relaxed**.
- An **exact-off control** reproducing pass-25 bytes for Hasty.
- The full matrix in one invocation; **no gate default may sample a subset**.
- Keep the pass-21 rig, the knead, the pass-25 bolt clearance (96 mm) and the
  mana character.
- **Fire every control** added or touched.
- Read real exit codes, never through a pipe. One ctest at a time.
- Identify processes by command line before killing; kill only my own.

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

- **Do not derive the camera follow from the projection arithmetic.**
  "65536 bias units = 1 NDC = 192 px of a 384 px frame" gives 89,170 and is
  wrong by 1.25× — the viewport's horizontal NDC is ~2.49 wide, not 2.0.
  Rendered, it left the creature drifting 164 px where the arithmetic promised
  40. Solve from measured rungs and **verify with a third**.
- **Do not scale the follow from `kU02HastyBiasX`.** That constant was picked as
  "enough to keep the creature in shot", not as a tracker; it is ~2.7× short.
- **Do not author the traverse along +X.** The u02 camera is a fixed 45°
  three-quarter, so +X is half depth: the creature grows and sinks and no
  lateral follow can hold it. Travel along `(cos, 0, −sin)`.
- **Do not lerp the follow on the frame index** for a looping traverse. The last
  rendered frame shows key 0, so the aim ends a whole traverse away from the
  creature and the subject **vanishes** at the seam.
- **Do not use the centroid direction-change count as a cadence measure.** It
  returns 34 / 34 / 42 for 9 / 13 / 17 cycles — the antenna dominates it. Look
  at a strip of consecutive frames instead.
- **Do not quote `screenmotion.py`'s `bg_dx` on this staging.** The terrain is a
  smooth gradient carrying a screen-anchored dither; the band correlator cannot
  track it and returns the wrong sign. The ground-relative rate is authored.
- **Do not raise a gate ceiling to fit an authored value.** mqa's 135 mm
  root-continuity bound refused a 165 mm bob; the amplitude moved instead, and
  the picture did not suffer.
- **Do not accept a value that clears a bound by a rounding error.** 150 mm
  passed by 0.3 mm and was rejected for exactly that.

---

## Open Questions

- **Drift (slot 1) has the same 45° traverse fault** — it travels along +X
  against the same three-quarter camera, so it also grows and sinks. Out of
  scope this pass and untouched (drift is byte-identical). Worth its own pass.
- **The pass-24 ambient eye SIZE layer still cannot reach 17 of 22 clips**,
  because `Rig::write` only applies it where a scale track exists and only five
  clips enable one. Hasty joining them is declared. Whether the others should
  is an owner question, not something this pass should have silently changed.
