# Task Log: RUN-20260905-1910 - [Describe objective here]

**Created:** 2026-09-05 19:10 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260905-1910-manafold-p6-recon-engine/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-05 19:10 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260905-1910
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

## RECON B (engine / plasma / rig cost) — log

- Lane cloned to `manafold-p6-recon-engine/{zhaozhou,Upheaval}`. Hardware lane untouched.
- Read OWNER-DIRECTION-5, `creature/09-ENGINE-GOTCHAS.md` (all 13 sections).
- **Key early finding:** `tools/reel/manafold_fx.h` (1308 lines) ALREADY contains
  a caged pulsar (`kPulsar*`), a 5-rung SMEAR PLANE with decay/jitter/hard-clear/
  row-tear, a lightning strand evaluator (`mana_lightning`, FX.LIGHTNING
  recurrence), a 9-entry mana MENU (`mana_fill`), and `glow_splat` / `smear_*`.
  Recon must therefore report "what exists" not "what to invent".
- Located pulsar: `effects-library.yaml` id `star-s11-pulsar`, reel subject
  `pulsar`, `subject_pulsar()` at `zhao_reel.cpp:4627`, star class S11.

## PRIORITY CHANGE (coordinator) — is `channel`'s mana the axed mote field?

**ANSWER: YES, the same system. STOP-AND-RAISE.** Evidence, all in the clone:

- `zhao_reel.cpp:5070` `s.u02_mana = slot == 2 ? 9 : (slot == 7 ? 0 : 3);`
  channel is slot 2 -> candidate **9**.
- `manafold_fx.h:996` `case 9: // the CHANNEL stack: the fold + the lightning strand`
  -> `mana_fold(frame, slot, keys, A, stfx, kRampAqua, crowd_pm, out); mana_lightning(...)`.
- `case 3` (every other clip) is the SAME `mana_fold(... kRampAqua ...)` call with no
  lightning. Identical mechanism, identical ramp, identical mote knobs.

So the ONLY differences between channel's mana and every other clip's are:
lightning stacked on top; smear rung 1 (SHORT/CLEAN) vs rung 3; `kKneadClipPm[2]=900`
(the 2nd-highest drive in the bank); and the violet `planet=1` bloom backdrop.

**Second, larger finding:** `cr_ctx.u02_add_pop / u02_tri_pop / u02_opq_pop` are
cleared at `zhao_reel.cpp:3919-3921` and **never filled anywhere**. Manafold puts
ZERO primitives on the engine particle path. Everything it draws is `glow_splat`.
The "ten-emitter particle set" that Direction 2 axed is already gone; the words
"too tiny and too many" fit THAT set (~76 point sprites, mean 2.5 px, per SPEC.md),
not `mana_fold`'s 24 splats of 7-10 px halo + opaque core.
