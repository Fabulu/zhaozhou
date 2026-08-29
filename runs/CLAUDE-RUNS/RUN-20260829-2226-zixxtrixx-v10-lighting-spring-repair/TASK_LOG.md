# Task Log: RUN-20260829-2226 - Zixxtrixx v10 lighting and rigid-spring repair

**Created:** 2026-08-29 22:26 UTC+02:00
**Status:** In Progress
**Working Directory:** `runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/`

---

## Objective

Preserve the approved v9 whole-body proportions and rendering style while repairing systemic light assignment/flicker and recurring malformed eye polygons at their roots, reconnecting tail fins, making constrained face changes, replacing the rear-curling spring with a shared rigid-S top-down compression, validating every production frame, and publishing one fully verified noindex v10 generation with v9 archived intact.

---

## Tracker Status

| Task | Scope | Status |
|---|---|---|
| #17 | setup and durable direction | Complete — `0d1f45e` / `f75306f`, pushed |
| #18 | lighting and eye artifacts | In progress — claimed |
| #19 | constrained face and fins | Pending |
| #20 | shared rigid-S spring | Pending |
| #21 | full validation | Pending |
| #22 | publish and handoff | Pending |

Only one task is active at a time. A task becomes complete only after its evidence, gates, logical commit and push are complete.

---

## Progress Timeline

### 2026-08-29 22:26 UTC+02:00 - Task started

- Generated Run ID `RUN-20260829-2226`.
- Owner assigned one sole implementation/modelling lane; no subagents may be spawned.
- Claimed tracker task #17; all later tasks remain pending.
- Recorded isolated clone contract:
  - zhaozhou clone `work/zhaozhou-v10`, branch `zixxtrixx-v10-lighting-spring`, base `54e74372367fa389d74d7bf74125352ae7bc6bf7`.
  - Upheaval clone `work/Upheaval-v10`, branch `zixxtrixx-v10-lighting-spring`, base `d97f7a424c9015c9ffc128406760fa5ccf370964`.
- Shared checkouts, hardware/migration lane and shared build/render outputs are forbidden. No worktree or Sacengine use; no `cmake --build`; direct lane-local builds only.

### 2026-08-29 22:41 UTC+02:00 - Prerequisite reading complete

- Read both clone-local `CLAUDE.md` files.
- Read every durable `OWNER-DIRECTION-*.md` through #9 in `Upheaval/creature/Zixxtrixx/`.
- Read `CREATURE-ASSET-OWNERSHIP-ARCHITECTURE.md`; migration remains docs-only and frozen from production creature edits until parent handoff after v10.
- Compared zhaozhou reports after the v9 closeout commit and read the only newer report, `reports/HARDWARE-LANE-STATUS.md`; it confirms the migration branch is separate and this lane must not release it.
- Read the complete v9 `TASK_LOG.md`, `SPEC_v1.md`, form/eye, motion/spring, impact, balance, taunt/fall and final production evidence summaries.
- Key inherited baseline: immutable v9 source `65350e04b4cabd357a28296f69713cd0c9b2a880`; integrated v9 Upheaval main `d97f7a424c9015c9ffc128406760fa5ccf370964`; published deployment `https://19a9bf54.upheaval.pages.dev`; 21 clips / 5,744 decoded cel-main frames.
- Wrote durable `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-10-2026-08-29.md` faithfully recording the approved-proportion freeze, systemic lighting/eye diagnosis, constrained face/fins and rigid-S spring requirements.
- Committed and pushed the durable direction early on the Upheaval v10 branch as `f75306f`.
- Filled v10 SPEC and TASK_LOG with ordered tracker mapping, evidence requirements and explicit failed approaches not to retry.

### 2026-08-29 22:48 UTC+02:00 - Setup pushed; lighting/eye diagnosis claimed

- Committed and pushed the zhaozhou run setup as `0d1f45e`.
- Task #17 is complete only after both setup commits reached their remote branches.
- Claimed task #18. No art constants or creature/renderer source have been changed.
- Next action is a clean direct-build v9 baseline followed by fixed-camera every-frame reproduction and root-cause isolation.

---

## Subagent Spawns

None. Owner explicitly requires this to remain a sole implementation/modelling lane.

---

## Files Created

- `Upheaval/creature/Zixxtrixx/OWNER-DIRECTION-10-2026-08-29.md`
- `runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/SPEC_v1.md`
- `runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/TASK_LOG.md`
- `runs/CLAUDE-RUNS/RUN-20260829-2226-zixxtrixx-v10-lighting-spring-repair/evidence/` for committed v10 diagnostics and acceptance output.

---

## Decisions Made

- Approved v9 whole-body proportions are frozen; no radius or centreline redesign is authorised.
- Lighting and malformed-eye artifacts are diagnosed and repaired before any face, fin or spring art changes.
- Normal Gouraud and true cel-main share root-cause investigation; thresholds/global brightness cannot mask the defect.
- Eye artifact acceptance is exhaustive across all production clips/cameras/modes, not keyed to the named knockdown example.
- The spring is a mostly rigid-S top-down descent with head ground contact and no rear roll/curl, shared by all jumps/saltos.
- Every-frame native visual review is authoritative; diagnostics compare and rank but do not author art.
- V9 archive media and exact noindex contract are immutable inputs to v10 promotion.

---

## Next Steps

1. Commit/push this run setup and mark #17 complete.
2. Claim #18.
3. Establish clean direct-build baseline and reproduce lighting/eye defects with fixed-camera normal/cel-main, unlit/normals/wire/debug views, every-frame sheets, badness ranking and temporal surface tracking.
4. Explain the true structural roots before implementing the smallest reversible fixes.
