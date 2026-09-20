# Task Log: RUN-20260920-0544 - [Describe objective here]

**Created:** 2026-09-20 05:44 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260920-0544-manafold-pass20/

---

## Objective


Manafold pass 20 from Owner Direction 21: (1) stop the rear connecting part leaving the body (a contact/attachment breach visible in Inspect; pass 19 damping was not enough and its gate missed it); (2) new authored kneading move where the middle-top ball (carrier B) sometimes dips low enough to become the lowest ball, on every animation; (3) the mana particles visibly react to that dip. Publish when finished.

---

## Progress Timeline

### 2026-09-20 05:44 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260920-0544
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

### 2026-09-20 05:44 - Started
- Branches `manafold-pass20` created in both repos from the production-verified pass-19 mains (Zhaozhou `4b3d4576`, Upheaval `d7086d2e`).
- Owner direction recorded durably: `Upheaval/creature/Manafold/OWNER-DIRECTION-21-2026-09-20.md`.
- One Opus worker implements; the coordinator organizes. No Qwen (another agent may still hold it; the owner restricted it in pass 19 and has not released it). GPT/Codex quota is still exhausted until 2026-09-25.
- Item 1 is treated as a defect in BOTH the motion and the gate: pass 19 shipped with 128/128 green while the socket still emerged, so the burial/attachment gate is part of the fault.

### 2026-09-20 ~06:10 - OWNER CORRECTION to item 1 (received mid-orientation)
Verbatim: *"the rear doesn't leave the body, but it rips a big piece out and it
stretches too much, which leads me to conclude there's too much motion in the
back nodule."*

Re-scope: item 1 is **not** an emergence/burial breach. No "socket left the
body" gate, no burial-threshold chase. The fault is **excess motion in the back
nodule (End carrier / rear ball)** dragging the body surface: a big piece reads
as ripped out and the skin stretches too far.
- Look for the stretch signature: skin weights spanning the socket, vertices
  shared between rear chain and body being pulled, a long swell run stretching
  rather than bending, pass 19's arm-following frame adding range, plus any
  remaining authored/oscillating rear authority.
- Fix = reduce the back nodule's motion range and/or how far its influence
  reaches into the body. Keep it alive (Direction 20 "a bit wiggly").
- Gate = per-frame surface stretch / attachment strain in the socket region
  (bounded edge-length / triangle-area change over body+socket vertices) plus a
  bound on the rear nodule's own motion range, with a fired positive control
  reproducing today's rip. Must state which pass-19 checks were blind to stretch.
- Inspect stays the primary witness; before/after at native and 4x.

Items 2 and 3 unchanged. Continuing from orientation, not restarting.

### Orientation findings so far (for P20-DIAGNOSIS.md)
- `antenna_knead` (manafold_clips.h:2368) is already the **always-on** layer on
  every performing clip. It is where the shared knead lives.
- Item 2's reference mechanism is Direction 16's **CROWN SHUFFLE**:
  `taunt3_order_pose`/`taunt3_order_target` (manafold_clips.h:1868-1950) with
  `kTaunt3OrderRank[4][3]` + High/Mid/Low mm tables (manafold_art.h:3526-3567).
  Its comment states the intent exactly: "Four held A/B/C rankings give every
  free carrier top and bottom ownership." That is already "B becomes lowest".
- The single production consumption point for carrier heights is
  `swallow_nodules(g, swal[5], lean_pm)` (manafold_clips.h:1824), described as
  keeping everything "under the same F/A/B/C/E public mute and attachment law".
