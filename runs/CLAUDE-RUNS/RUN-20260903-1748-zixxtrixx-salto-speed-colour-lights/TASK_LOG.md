# Task Log: RUN-20260903-1748 - [Describe objective here]

**Created:** 2026-09-03 17:48 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-1748-zixxtrixx-salto-speed-colour-lights/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-03 17:48 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-1748
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## 2026-09-03 — Direction 26 pass, both asks authored

**Read first:** OWNER-DIRECTION-26 (the two asks), -25/-24/-23, peel run QA.md
(what passes and must not regress), RECON-4 machinery map, zhaozhou/CLAUDE.md.

**Baseline:** built cel at HEAD `cf114f88` into `build-lane`, rendered all 22
subjects with one explicit `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`
invocation. All 22 sequence CRCs reproduce the peel-run QA table exactly
(`evidence/baseline-crc.txt`, all-match-QA: True).

### Ask 1 — the single salto is faster

* Target identified: `zixxtrixx-salto-dummy` (slot 33, page "Salto: grounded
  mark"), the plain one-turn salto. ASSUMPTION stated: salto-fly also computes
  one turn but the owner said "Only that one"; the grounded mark is the plain
  single-salto clip. One-line reversal if wrong.
* The slow read was quantified before editing: the dummy spent 18 keys on ONE
  turn where six/nine spend 7.3/8.0 keys per turn.
* Change: new owner knob `kSaltoSingleCoilKeys = 10` in `zixxtrixx.h`, applied
  in `zixx_variant_plan` for `kSlotAtkDummy` only — the exact pattern six and
  nine already use. `zixx_plan_lock_spear` never reads `coil_keys`; the target
  is static so the intercept lead is inert; spear/apex/plunge and the whole
  shared arming schedule untouched. No seam-table literal, no wobble-period
  arithmetic, no probe phase-band input changed.
* Probe: `ZIXX PROBE: PASS` on the edited build (build-work).
* Eye: clip 311 → 295 frames (exactly the 8-key coil cut ×2 ticks). The wheel
  forms, holds shape, and turns at the fast variants' rate; the arming and the
  spear dive are frame-identical in read. Sheets + native frames in evidence/.

### Ask 2 — three coloured moving lights

* The compositor supported ONE point source. Extended honestly:
  `g_creature_point_light` (single) → `g_creature_point_lights` +
  `g_creature_point_light_count` (contiguous array; count 0 = shipping default,
  same code path as the old nullptr = byte-identity boundary for every
  ordinary subject). Per-vertex/per-face response is now a per-channel
  gain-weighted SUM over sources (`PointShade3`), summed before the shade
  quantiser — linear transport, so intersecting pools MIX by construction.
  Files: `zref_creature.hpp`, `creature_sim.cpp`.
* Reel: the warm lamp is source 0, verbatim. Three new sources — blue (level
  2-lap orbit), orange (counter-rotating 3-lap orbit with bob), green (low
  near-side 3-trip longitudinal shuttle) — every colour, radius, gain, path
  extent, period and phase a named constexpr owner knob in `zhao_reel.cpp`.
  Depth-tested visible orbs, one per source, tinted per colour.
* Author-render-look loop, 3 iterations at native 384x240:
  * v1 (outer 2800, gains ~2.5): DISCO — pigment obliterated, all pools on the
    body at once. Rejected on sight.
  * v2 (outer 2000, gains ~60%): better, still 3 hues everywhere.
  * v3 (outer 1500, orbits widened, green pushed to -1000): pools are local
    and travelling; most frames rest near the accepted warm read; overlaps
    happen and read as mixed colour (magenta where warm+blue meet, the green
    pool riding the crown). Pigment and form read. ACCEPTED by eye.
    Evidence: ml-v3-sheet.png, ml-v3-f000/150/330/510.png vs ml-base-*.png.
