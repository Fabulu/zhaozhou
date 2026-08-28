# SPEC v1: Zixxtrixx v4 closeout: four standing faults, falling decision, sacengine vocabulary, missing animations

**Run ID:** RUN-20260828-0326
**Created:** 2026-08-28 03:26 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Zixxtrixx nominally complete:
1. PART 1 faults fixed: death pink-forward pose/camera, balance push-up hop,
   pupil character at gameplay distance, frontal eye presence (between the
   brim-at-42 and chinstrap failure modes).
2. PART 2: F1 vs F2 falling decided by eye on side-by-side contact sheets;
   loser stays behind its flag.
3. PART 3: sacengine fetched/built as far as reasonable (time-boxed); real
   animation vocabulary extracted and committed as a reference doc in
   Upheaval/creature/Zixxtrixx/; missing essential clips authored
   (getUp/knockdown, directional hits, more deaths, run gait, taunt, ...).
4. Gates green: zixx-probe, zixx-choreo, zixx-planner exit 0; goldens
   cmp-clean except LOUD deliberate re-pins; zhao-reel --check to a FILE.
5. Site: new LIVE tabs, MAX_TABS raised in assemble.py AND style.css
   selector families together; consider card grouping.
6. Renders on disk; NO deploy (owner publishes).

---

## Scope

**In Scope:**

- Death clip keel/roll or camera; balance fork-height knob; pupil boldness;
  frontal eye presence; falling decision incl. F2 tuning; new clips; site.

**Out of Scope:**

- T5 grain, head attitude (-6000), pink band 9, head-aim rig, planner,
  phase clips, salto, walk golden, approved idles, archive renders, deploy.

---

## Constraints

- Never cmake --build; use committed build-direct.sh.
- Goldens are the contract; deliberate re-pins carry LOUD provenance.
- No Python hash() for content; zlib.crc32 only.
- Determinism: fixed-point, no runtime physics, no wall clock.
- Ground contact authored; measured with zixx-probe only.
- Judge by contact sheets of every frame at fixed diagnostic cameras.

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

---

## Open Questions

- [Question 1]
