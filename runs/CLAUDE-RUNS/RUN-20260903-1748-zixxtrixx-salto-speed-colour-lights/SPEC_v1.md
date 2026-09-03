# SPEC — Direction 26: single salto faster; three coloured lights on the moving-light render

**Run:** RUN-20260903-1748-zixxtrixx-salto-speed-colour-lights
**Lane:** `zixxtrixx-wholebody-s-spring-20260901` — zhaozhou `main` @ cf114f88, Upheaval `main` @ 15105f4.
**Direction:** `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-26-2026-09-03.md`. Small pass, two asks.

## Ask 1 — the single salto is faster, ONLY that one

* **Subject identified as `zixxtrixx-salto-dummy`** (slot 33, `kSlotAtkDummy`, page label
  "Salto: grounded mark"): the only pure single-salto attack clip. Its plan computes
  `spin_mturns = 1000` (one turn) over `coil_keys = 18` — 2.3–2.6× more keys per turn than
  the six (44/6 = 7.3) and nine (72/9 = 8) variants. That slow one-turn wheel is the "slow"
  read.
  * ASSUMPTION, stated: `salto-fly` also computes one turn, but the owner said "Only that
    one", the page shows one plain salto clip, and the lane brief says "the one-turn salto"
    (singular). Target = salto-dummy. Cheap to reverse: the same named constant can be
    applied to the fly plan in one line.
* **Mechanism:** override `p.coil_keys` (and nothing shared) in `zixx_variant_plan` for
  `kSlotAtkDummy` via a new named owner constant, following the exact pattern the six and
  nine variants already use. `zixx_plan_lock_spear` does not read `coil_keys`; the target
  is static (v = 0) so the intercept lead is unaffected; spear, plunge, apex unchanged.
* **Untouched:** the shared arming schedule (`kSaltoCompressEndKey` 50 / hold 58 / release),
  the peel spring, all pose tables, the seam table, the wobble period 23, attack key 29 = 52.
* **Proof:** 22-subject CRC sweep before and after; every subject except salto-dummy
  (and moving-light, Ask 2) byte-identical.

## Ask 2 — three more coloured moving lights (blue, orange, green)

* The creature light model (`reference/src/zcreature/creature_sim.cpp`) supports exactly ONE
  point source (`zc::g_creature_point_light`). Extend to a contiguous ARRAY + count
  (`g_creature_point_lights` / `g_creature_point_light_count`); count 0 is the shipping
  default and keeps every ordinary subject byte-identical (the nullptr identity boundary
  becomes a count-0 identity boundary, same code path).
* Per-vertex/per-face point response becomes a per-channel gain-weighted SUM over sources —
  linear light transport, so overlapping pools mix by construction.
* Reel side: `sample_zixx_moving_source` keeps the existing warm lamp verbatim as source 0;
  three new samplers add blue, orange, green sources on their own periodic paths (orbits +
  longitudinal sweep) with tighter pools so intersections are events, not a permanent wash.
  Every colour, radius, gain, path extent and period is a named `constexpr` owner knob.
* Markers: each source keeps a depth-tested visible orb tinted its own colour.
* Only `zixxtrixx-moving-light` may change; all other subjects byte-identical (their path
  has zero point lights).

## Bounds

Verify the two changed subjects by eye at native resolution; prove byte-identity of the
other 20 by CRC; probe green; then the publication sweep (render 22, encode, archive
Generation Sixteen, assemble, deploy once).
