# Task Log: RUN-20260909-0316 - [Describe objective here]

**Created:** 2026-09-09 03:16 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260909-0316-p14-reel-light-mana-camera/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-09 03:16 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260909-0316
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

## IMPL-REEL — pass 14, "light, mana, camera"

Lane: `C:\programmieren\zencrifice\manafold-p14-reel\{zhaozhou,Upheaval}`.
Items: R5 (framing + mist), R6 (lightning draws shapes), R7 lamp experiment,
R8 (dark side), R9 (loop/showcase hygiene), R10.3-4 (instrument hygiene).

### Baseline build
`tools/reel/build-direct.sh --output ../build-reel --clean cel` -> exit 0.
md5 `12ca4ca3fa95aeae45e8091c9341afc6` (build-reel/base-md5.txt).
Every A/B this pass comes from ONE binary per generation; md5 recorded each time.

### Reading before authoring — what the SOURCE says (all verified by grep/read,
### not inherited)

* **R6(a) is ALREADY DONE in the shipping tree.** `mana_fold` (fx.h:1832-1866)
  walks `fold_edge_link(ph.shape_to, i)` and calls `bolt_path(...)` for every
  linked station pair, stamping halo+core along it. The plan's "route the bolt
  primitive along fold_edge_link" is not a missing wire. What is missing is that
  the edge is stamped in the FOLD's own dim aqua (halo 220 pm x `lit`, core
  430 pm at r=2 px) while `mana_lightning` composites an INDEPENDENT white
  strand at 950-1080 pm over the same pocket with no knowledge of the stencil.
  => the mechanism to attack is the COMPETITION, not the routing.
* **R6(d) is CLOSED.** Grepped every `s.planet` site in zhao_reel.cpp: the only
  one that sets `planet = 1` for a Manafold clip is `u02_backdrop()`, called
  from exactly two places (slot 2 in `subject_u02_clip`, and `crackle`).
  Everything else sets `planet = 0` explicitly. The `ffae071e` half-landing was
  properly repaired in pass 13. No further work; recorded so nobody re-opens it.
* **`crackle` runs candidate 4 — LIGHTNING ONLY, no fold at all.** `subject_u02_clip`
  sets `s.u02_mana = 9` for every slot but 7; `crackle` then overrides to `4`.
  So `crackle` has never drawn a single stencil figure. That is why the review
  sees "a necklace sliding on a track" and why the two clips "do not look like
  one creature's two mana clips" — they are running different code.
* **R5's "copy `fall`'s mist budget" cannot be done as stated: there is no
  per-clip mist budget.** `u02::g_u02_mist` is ONE global; every showcase clip
  runs the same `kMist*` defaults. `fall`, `drift` and `hasty` are already
  byte-identical in mist config. The difference is `mist_speed_mul_pm`
  (fx.h:1008) applied twice, plus `kMistKeepPm = 420` memory integrating over a
  long horizontal traverse with the plane following at only 820 pm.
* **R8 has no free light slot.** `zref_creature.hpp:1206` caps
  `kCreatureMaxPointLights = 4` and `sample_u02_moving_sources` fills all four
  (warm/blue/orange/green). A fifth point light is a shared-core edit. So the
  rim has to come from the rig's own key/fill/ambient triple or from
  repurposing one of the four.
