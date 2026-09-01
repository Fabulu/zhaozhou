# Task Log: RUN-20260901-1738 — Zixxtrixx whole-body S spring (Owner Direction 20)

**Created:** 2026-09-01 17:38 UTC+02:00
**Status:** In Progress
**Lane:** isolated clones at `zixxtrixx-wholebody-s-spring-20260901/`, branch
`zixxtrixx-wholebody-s-spring` in both repos. The shared checkouts are
hardware-owned and were never touched.

---

## Objective

Rebuild the rejected jump/somersault anticipation against Owner Direction 20's
six counts. See `SPEC_v1.md`.

---

## Progress Timeline

### Looking first

Rendered the published baseline (`zixxtrixx-spring-side`, `-jump-one`,
`-salto-dummy`) and the tail-balance reference, and built every-frame contact
sheets plus a frame-to-frame motion-energy trace. What the owner is describing
is all visible in those two artefacts:

- The published arming **straightens the body into a diagonal rail** at frame 3
  and again at frame 43, with the head thrown forward.
- Frames ~24-35 are **exactly zero motion** — a literal frozen hold.
- The release is a **spike train at 36/38/42** — a 30 Hz judder, because the
  authored half-key presentation poses did not lie on the interpolated route.
- The landing recovery **stops dead**.

Plotting the four authored pose tables as centrelines showed the loaded pose is
a **corrugation**, not an S: three small bumps whose head sits 89 mm FORWARD of
and 129 mm below its resting place. The balance clip, by contrast, takes its
head 803 mm back and 1149 mm up.

### The unit bug

`spring_anchor_offset` converted its fx16 result with `rx / (1 << 16)`, which
yields **metres**, and every consumer spends the value as **millimetres**. At
the deepest authored pose the real support correction is (-83, -311) mm and the
function returned (0, 0). Station 14 was therefore never planted — the **head
was pinned** and the body swung forward underneath it. That is precisely the
owner's biggest complaint, and it also made `kSpringDeclaredBiteMm` and the
whole `kSpringOpenSupportLift` route dead numbers.

### What was authored

Committed `tools/reel/zixx_springpose.cpp` so the centreline numbers behind
every judgement below are reproducible. Then, by eye against the balance clip:

| Direction 20 | What changed |
|---|---|
| 2. leans forward, destroys the S | Unit fix + three re-authored pose tables. Head travel is now monotone BACKWARD: 0 → -643 → -1267 → -1712 mm, rising to +1058 mm. |
| 1. rigid | One C1 Catmull-Rom route replaces three smoothstep legs that each fell to zero velocity at an interior pose; plus the balance clip's own three devices (two travelling waves, per-station arrival lag, support-faded authority). |
| 1. rigid somersault | The wheel was one identical pitch on every joint — a compass circle. Now a named taper (a spiral) and a travelling breathe, phase-locked to the 23-key stride the looping coil clip requires. |
| 3. two jerks | Entry seam: the trapezoid ramp starts from rest. Release seam: re-spaced in arm units (was a 79 mm stall then a 1073 mm slam) and every half key put back on the route. Landing: one slow damped bounce, and a +24 mm single-key root pop removed from the surface-bias curve. |
| 4. too fast | 12 keys of arming + a SIX-key frozen hold → 16 keys of arming + a two-key living beat, on a trapezoidal speed profile. Head travel per key: 18/71/104/168/212, a ~180-220 plateau, then 135/91/52/13. |
| 5. balance quality, stands on its tail | The life layer is the balance clip's device set. The assembled pose genuinely rises onto a planted tail. |
| 6. S to the last tail station | Every joint of the loaded pose turns; the two zero-turn joints are the lobe apexes, which is what an S is. |

### Faults found and fixed along the way

- The monolithic attack's spine **never received the life clock** — the edit had
  landed on `attack_choreo_sample` (which uses `key`) while `build_attack`'s
  loop uses `f`. The golden clip had no wobble while every planned consumer did,
  and the golden clip is what the diagnostic subject renders. Caught by the
  bit-exact parity gate.
- `zc::AttackPlan`'s engine defaults still read 12/6, so every planned attack
  variant kept arming on the rejected schedule. The creature now states its own
  spring timing.
- A time-based wheel breathe **broke the phase-clip seam contract**
  (`kSlotAtkCoil` is a looping duplicate of attack key 29 that must equal key
  52). Phase-locked to that stride rather than abandoned.
- The re-authored support route had never actually run, so its values were
  decorative; with the plant honest they buried the loaded animal **130 mm**
  into terrain against a 34 mm declaration.
- **A landing is not an arming.** The old loaded pose was low and flat, so
  reusing it as the landing cushion happened to read as absorbing. The loaded
  coil is now tall and leaning back, and landing into it made the animal REAR UP
  on impact. The landing now stops at `kJumpLandingAbsorbArm`.

### Probe

36 assertions failed after the re-author. Triaged rather than widened:

- **Satisfied, not retuned:** the bit-exact release and whole pre-lift parity
  gates (they found two real faults, above).
- **Re-authored:** ground contact. Declared `kSpringDeclaredLoadedBiteMm` 60,
  `kJumpLandingLoadedBiteMm` 48, `kJumpImpactMinBiteMm` 15. Measured: spring
  -55, landing impact -30, handoff -37, settle -40..-10. Weighted, never
  hovering.
- **Re-derived bands:** the held-brace gate demanded the hold not move by more
  than ONE millimetre — a faithful description of exactly what the owner
  rejected. Also the phase-duration windows, the deformation-identity window
  (now follows the arm clock rather than naming a key that moved), and
  `kJumpMaxStationStepMm` 1150 → 1300.

`ZIXX PROBE: PASS`.

---

## Subagent Spawns

| Timestamp | Purpose | Status | Findings |
|---|---|---|---|
| 2026-09-01 | Map each failing probe assertion to its source line and classify it as a structural invariant vs a regression band recorded from the old art | Complete | 17 assertions mapped; six clean bands identified, two invariants found to be wearing stale parameters, and two bit-exact parity gates flagged as "satisfy, do not retune" — which is exactly where the two real faults were. |

---

## Evidence

Under `evidence/`: every-frame contact sheets of the spring before and after,
the spring arming beside the balance rise, the landing before and after, the
authored-route centreline plots, motion-energy traces, and the probe output.
