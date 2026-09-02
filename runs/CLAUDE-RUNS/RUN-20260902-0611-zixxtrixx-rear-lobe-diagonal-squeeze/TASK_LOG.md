# Task Log: RUN-20260902-0611 — Zixxtrixx rear lobe, backward head travel, diagonal deeper squeeze (Owner Direction 22)

**Created:** 2026-09-02 06:11 UTC+02:00
**Status:** In Progress
**Lane:** isolated clones at `zixxtrixx-wholebody-s-spring-20260901/`, branch
`zixxtrixx-wholebody-s-spring` in both repos. Shared checkouts untouched.

---

## Objective

Fix Direction 22's four faults — rear lobe AT THE COLLAPSED POSE (top
priority, three passes of false "fixed" claims), backward head travel without
neck over-curl, diagonal down-and-back squeeze, deeper compression — while
preserving Directions 20/21's accepted results. See SPEC_v1.md.

---

## Progress Timeline

### 2026-09-02 06:11 — Task started; reading

- Read Direction 22 (binding), 21, 20; CLAUDE.md art law; both previous runs.
- **Why the previous "fixed" claim failed:** RUN-20260901-2121 proved the rear
  curl (-80 deg) at the ASSEMBLED pose and cited "the renders show the rear
  curling as authored" — but the COLLAPSED pose table straightens the tail
  back out (-14500 -> -1600 a16 at the tip), and collapsed is the pose the
  owner watches through the hold. The proof was made at the wrong pose.
  Recorded in SPEC Don't-Retry.
- Verified the machinery end to end: absolute per-segment headings, chain from
  nose, station-14 planted with real root compensation (metres/mm fix kept),
  Catmull-Rom route through 4 knots, life layer, deform sidecar. The rig does
  not stop the rear from curving; the tables author it flat.

### 2026-09-02 — Direction 22 gained section 5 while reading

- The owner added: "It doesn't have to be a perfect S at the end, just a coil
  taken to its extreme, standing on the last tail point. A real maximization
  of oomph." Where it disagrees with the S wording, section 5 WINS.
- Design consequence: the collapsed pose is re-targeted from "S squeezed from
  the top with a rear lobe" to "coil wound to its extreme, STOOD ON THE LAST
  TAIL POINT": tail tip is the ground contact and base; the body stacks in
  wound runs above it; the head aims forward at the top; the whole pile
  presses lower than the shipped 64%.
- Plan: keep kSpringPlantSegment=14 as the fixed reference and author its
  COLLAPSED Y lift POSITIVE (station 14 rises onto the coil); the tail tip
  reaches the ground with the declared bite as an authored consequence,
  checked by the probe. All machinery survives; tables + named lift/flatten
  constants change.
