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

---

## 7. Packet 4: `kSpanStretchMaxPm` / `kSpanCompactionMinPm` — **NOT CHANGED**

Recorded because it was *considered and rejected*, and a reviewer should see the
decision rather than its absence.

* **The attachment bound `kSpanStretchMaxPm[C-E] = 440` was not touched.** Pass
  20 exists because that attachment tore; a bound protecting attachment does not
  move to fit a new beat. At the shipping dip C-E reads 404 pm, with 36 pm of
  headroom.
* **A-B stretch (480→490) and B-C compaction (−430→−450) were prototyped**, to
  let the two spans *between the free carriers* absorb the knead — which is where
  a knead belongs and neither is an attachment span. They cleared all 6 bound
  breaches at gain 650. **They are not in the shipped tree**, because clearing
  the envelope simply exposed two other mspan legs (carrier jerk, and
  "SpanDeltaE and RearSocket do not meet at End") that fail at every amplitude
  with that mechanism. Shipping a widened bound to buy a red gate elsewhere
  would be strictly worse than not widening it.
* **`kSpanMinRunMm = 80` untouched throughout.** It is the guard that actually
  keeps the antenna attached; at the prototyped B-C floor the remaining run was
  212 mm, nowhere near it.

The full ledger, per station, before and after, is in `P20-DIP-STOP.md`.

---

## Packet 6 (2026-09-20)

### NEW: mspan **G11 WALK PAIRING** (category `kCatWalk`, 0x8000)

* **What it protects.** That a loop span's arc is paired with the span delta on
  the bone that ENDS it. Nothing protected this before, and two copies of the
  same walk drifted at birth: packet 5's dent read each delta one bone early,
  which moved C by up to 344 mm on 3365 of 3424 dent samples and was invisible
  in the one configuration (duck 1000) that zeroes every delta.
* **How.** Distinct probe deltas (125 / 375 / 750 mm, multiples of 125 so the
  fx16 round trip through `set_span_delta` is exact) on a rest rig; each
  reconstructed segment must equal `kLoopArcMm[i]` plus that span's delta.
* **Positive control.** `--fail-walk-pairing` runs the packet-5 one-bone-early
  walk against the same rig: **4 of 5 segments mispaired, worst +750 mm**,
  attributed to `kCatWalk` alone. Normal: 0 of 5, +0 mm.
* **Not a loosening.** It is an addition; no existing ceiling, floor or
  tolerance moved.

### NEW strict selectors (RC 2)

`ZHAO_U02_KNEAD_DIP_SOLVER=fold` (unknown name),
`ZHAO_U02_KNEAD_DENT_SWING_PM=1001`,
`ZHAO_U02_KNEAD_DENT_OVERPRESS_PM=3001`,
`ZHAO_U02_KNEAD_DENT_DEPTH_PM=6001`.

### RANGE (not a bound) widened

`ZHAO_U02_KNEAD_DENT_DEPTH_PM` accepts 0..6000 instead of 0..4000. `s` is the
product of four per-mille factors (clip share x motion x dip gain x depth), so
at the shipping gain the mirror sits at depth 4850 and the old cap made the
knob's own useful setting unreachable. No gate reads this value; it selects an
experiment, and the strict selector above still rejects anything past it.

### Not changed

`kSpanStretchMaxPm`, `kSpanCompactionMinPm`, `kSpanMinRunMm`, every R1..R5
ceiling, G5..G10's thresholds, and `kAntennaMaxAngularStepDeg/AccelDeg/JerkDeg`
(8/6/6) — which is what the swing fails, and it was left exactly where it was.

---

## Packet 7 (2026-09-20)

### NO gate threshold changed, and one was deliberately NOT changed

`kAntennaMaxAngularStepDeg` stays at 8 degrees. The roll-stable aim brought the
dent's worst angular step from 55-80 deg to 7.772, and the shipping DEPTH was
then chosen to fit that ceiling (2200 measures 7.772; 2250 measures 8.080 and is
over). The ceiling was never a candidate for moving: a gate that the art value
has to fit is doing its job, and an 8 that becomes a 9 to admit a number is the
loosening this pass exists to avoid.

### NEW matrix legs: byte identity, in the matrix rather than in a receipt

`e-identity-carried` and `e-identity-legacy` render hover / inspect / taunt3 and
compare the CRCs against literals in the script:

* `ZHAO_U02_KNEAD_DIP_SOLVER=carried` -> `0x79D3F0C5 0x0710E704 0xC81598AA`,
  the packet-5 shipping bytes. The solver the dent replaces is provably
  untouched.
* `... REAR_BOW=legacy` -> `0xE6DD5EBA 0xDE1F5918 0x75BC4777`, the pass-19 bow
  path at `b7c096c2`.

These were checked by hand every packet and written into a markdown file, which
is exactly how a receipt goes stale. They are now legs that fail.

### The dip ships ON

`n-mrear-dip` now judges R5 on the shipping configuration rather than on a knob
turned on for the leg. The comment that said "the dip ships OFF" was updated
with it.
