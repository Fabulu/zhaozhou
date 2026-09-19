# Task Log: RUN-20260919-2058 - [Describe objective here]

**Created:** 2026-09-19 20:58 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260919-2058-manafold-pass19/

---

## Objective


Manafold pass 19 from Owner Direction 20: smooth, whole rear antenna connection at the back ball; calmer, less over-animated rear ball and last segment (audit for a doubled/competing bone first); mana fold connecting lines that thin with distance like the outline ink. Publish when finished.

---

## Progress Timeline

### 2026-09-19 20:58 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260919-2058
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

### 2026-09-19 20:58 - Started
- Branches `manafold-pass19` created in Zhaozhou and Upheaval from the production-verified version-18 mains (Zhaozhou `f3f061f7`, Upheaval `2132bb87`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-20-2026-09-19.md`.
- No Qwen this pass (the owner: another agent is using it). One Opus worker implements; the coordinator organizes. The GPT/Codex quota is still exhausted until 2026-09-25, so this continues the version-18 arrangement.

### 2026-09-19 21:40 - Diagnosis (sole Opus worker)
- Baseline renderer = accepted v18 `.tmp/v18-int-final/bin/zhao-reel-cel.exe` (MD5 0f082622...); fresh Hover/Inspect/Drift CRCs match the v18 bank.
- New committed instrument `tools/reel/manafold_rear_audit.cpp` (`build-direct.sh mrear`).
- ROOT CAUSE items 1+2: kBRearSocket (Root child since pass 16) has no rest orientation -> End rings point Root +Y (up) while the arm arrives downward; tube hairpins 150-170 deg at ring 55 on every sample of every clip; rings 58-62 stand up to 207 mm out of the body as the "End-swell stub", swung by the End authorities (25 deg B2 wag, hinge-play, swallow) independently of the arm.
- Item 3: fold/bolt line splats have constant screen-px radii; ink scales by projected radius.
- Plan in P19-DIAGNOSIS.md: RearSocket = arm arrival frame x authored; line splats scaled by the ink's operand; strict legacy toggles; new rear/line gate.
