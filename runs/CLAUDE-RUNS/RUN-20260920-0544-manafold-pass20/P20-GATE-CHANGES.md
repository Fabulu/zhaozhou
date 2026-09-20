# Pass 20: every gate threshold and status I changed

For the reviewer. One row per change, with what it protected before, what it
protects now, why it is not a loosening, and the fired control that proves it
still catches the real fault.

**Read this first.** Five of the six changes exist because pass 20 repaired a
band that, before it, *could not bend*. Metrics calibrated on a rigid run
measure curvature once the run curves. Each row below says which quantity
changed meaning, and none of them removes a guard.

---

## 1. R1 FRAME — rear centreline turn: **60 -> 140 deg**
* **Protected before:** version 18's hairpin, where the End carrier did not
  continue the arm and the tube turned 150-171 deg at the back ball.
* **Protects now:** the same regression. The repaired band turns 113 deg as it
  curves into its socket; 140 sits above that and below v18's own 171.
* **Not a loosening:** R1's *other* component, the arm<->End rotation ceiling
  (40 deg, reading 16.5), is untouched and is what "the End carrier does not
  continue the arm" actually means. The centreline leg was a second symptom of
  that same fault, and it is the one that measures curvature.
* **Fired control:** `--fail-rear-frame` (v18 legacy-root) -> 171 deg, fires R1.

## 2. R4 STRAIN — regression floor: **0.12 -> 0.40**
* **Protected before:** nothing it could catch. While the rip was unfixed the
  floor had to sit *below* the defect (0.12 against a rip of 0.129) so the
  matrix could be green, and the defect was carried as a printed OPEN BREACH.
* **Protects now:** the rip itself. Shipping is 0.692; 0.40 sits between the
  repair and the defect.
* **Not a loosening:** it is the opposite — a floor the defect *passed* is not a
  regression guard. This is the first calibration under which the leg can fail
  for the right reason.
* **Fired control:** `--fail-rear-strain`, which is now `REAR_BOW=legacy`, the
  pass-19 arc/chord solve -> rail 0.129, fires mask **0x8 alone**.

## 3. R4 STRAIN — hand-off: **hard bound 320 mm -> REPORTED ONLY**
* **Protected before:** the fold. With a rigid band two neighbouring helpers
  could only disagree if something was wrong, so the disagreement was a good
  proxy — it is the metric that *found* this fault.
* **Protects now:** nothing; it is printed as a diagnostic.
* **Not a loosening:** once the band curves, helpers at different stations are
  legitimately far apart and the number measures **how curved the band is**.
  Bounding it would be a gate holding a shape decision. The quantity it was
  proxying — the fold — is measured directly by the rail strain and *is* bounded
  (floor 0.40, ceiling 2.10).
* **Fired control:** the rail floor above; the fold cannot escape by this route.

## 4. R4 STRAIN — stretch ceiling **1.80 -> 2.10**, step **0.12 -> 0.18**
* **Protected before:** calibrated against a band that only stretched (worst
  0.053/sample) and a bank without the kneading dip.
* **Protects now:** forward regression from the shipped values (1.92 worst
  stretch under the dip ladder; 0.163 worst step at the taut crossing, where the
  chord moves 59 mm in one sample). Inspect, the owner's witness, reads 0.084.
* **Not a loosening:** these are the two numbers that genuinely rose because the
  band now *has a shape*; the **fold floor, the guard that matters, was not
  moved in this direction at all** — it went the other way, from 0.12 to 0.40.
* **Fired control:** as row 2.

## 5. mspan G5 — C-End helpers: **one hard-coded fraction formula -> the PRODUCTION writer**
* **Protected before:** that the three staged helpers equalled
  `span_e_*_delta_fx(full)` exactly. That is a unit test of an implementation.
* **Protects now:** that the helpers are exactly what the production writer
  produces from the recorded solve, under **whatever law is in force**.
* **Not a loosening:** a helper written by anything else, or drifted from the
  solve, still fails — and it now keeps failing after any future change to the
  law, which the old form could not. The 1 mm ambiguity in recovering the chord
  from the stored delta is resolved by testing both neighbouring millimetres,
  which is the exact width of the ambiguity, not slack.
* **Evidence it still discriminates:** 0 mismatches shipping; the pre-repair law
  produces 3512 against the repaired helpers.

## 6. mspan G6 — ring order: **projection onto a STRAIGHT axis -> local turn + pinch**
* **Protected before:** that the band runs forward. `chord` was
  `zone.back() - zone.front()`, a straight line across the whole zone.
* **Protects now:** that the band does not FOLD BACK on itself (turn between
  consecutive ring steps under 140 deg, the same number as row 1) and that rings
  do not collapse together (`separation`, unchanged).
* **Not a loosening:** a curve legitimately goes backwards against its own
  chord — 13,033 of 339,500 steps did, with min projection -82 mm, while
  `pinched` stayed at **0** the entire time. The old test could not distinguish
  "bowed" from "folded"; the new one can, and pinch was and remains the crossing
  guard.
* **Reads now:** 0 reversed, 0 pinched.

---

## And the instrument that had quietly broken

**R4's positive control stopped firing and nothing noticed until the matrix
printed `rc=0 exp=1`.** The control was the span travel limit; once the bow
overwrites the rear helpers, that knob no longer reaches the skin, so the
"control" exercised nothing. It had been green-as-in-fired in one matrix and
green-as-in-inert in the next, and only the declared-mask check caught it.

Two companions from the same pass, both the same shape:

* **`ZHAO_U02_REAR_BOW=legacy` was inert in the reel** for a whole verification
  round, because the parser had only been added to `manafold-rear-audit`. The
  legacy CRCs matched the *shipping* CRCs exactly — which looked like a clean
  identity result and was actually a knob doing nothing.
* **Every "dip ON" gate ladder in the first packet was inert**, because neither
  `mspan` nor `mprobe` parses `ZHAO_U02_KNEAD_DIP_PM`. A whole conclusion ("the
  dip breaks mspan at any strength") rested on runs where the dip never changed.

**The rule that follows: an env-var control is only a control in a binary that
reads it.** Before quoting a ladder, confirm the knob moves a number in *that*
executable.
