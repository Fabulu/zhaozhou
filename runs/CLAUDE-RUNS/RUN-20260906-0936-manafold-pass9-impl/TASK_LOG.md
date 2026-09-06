# Task Log: RUN-20260906-0936 - [Describe objective here]

**Created:** 2026-09-06 09:36 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0936-manafold-pass9-impl/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 09:36 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0936
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

---

## Pass 9 — Manafold implementer

**Lane:** `C:\programmieren\zencrifice\manafold-p9-impl\{zhaozhou,Upheaval}`,
cloned fresh from `origin/main`.
zhaozhou @ `fd1e8b04`, Upheaval @ `000e534`.

### Reading done (0936-1000)
- `OWNER-DIRECTION-7-2026-09-06.md` in full, incl. the new §8.
- `PASS-8-FINDINGS.md`, `PASS-8-INPUTS.md`, `ARCHIVE-GAP-RECONSTRUCTION.md`.
- `OWNER-DIRECTION-5` (0-TER, 0-QUATER, 3, 5c, 6, 7), `OWNER-DIRECTION-6` + RESULT.
- `09-ENGINE-GOTCHAS.md` in full.

### The brief, as I read it
1. **HEADLINE: the MIST.** Missing. Soft, hazy, no shape of its own, leaving the
   wrong-frame-buffer ghost. **Attached to the creature** — D6 §0-TER option 3
   (creature-relative smear plane) is now explicitly IN SCOPE, asked for by name.
   Must not undo pass 8's colour work (motes 22.8% neutral / sat 87; smear 0.0%
   neutral / sat 168).
2. Carried, ranked: §5c rule 3 ENFORCEMENT (centring hypothesis REFUTED — it does
   not shrink on its own); near-eye bar at oblique; `hasty` 38 px loop seam;
   traverse framing; clip inventory F.2-F.6.
3. The eye-travel EXPERIMENT (D7 §7) — a build, not a scale.
4. Publish + archive the outgoing generation, then the pass 3/4 archive
   reconstruction.

### DO NOT (owner overruled pass 8)
- Shapes READ edge-drawn. No separation / world hold / fold-then-release / scale-up.
- Do NOT fix the shape-into-antenna clipping this pass (deferred explicitly).

### In progress
Explore agent mapping the mana/smear/fog code in `tools/reel/`. Next step after
it returns: locate the smear plane and the posed root anchor, and decide how the
mist attaches.

### 1000-1200 — done so far
- **THE MIST** built and shipped (commit `024c6b48`). A SECOND plane, creature-
  relative, 48x30 at 8x. Variant sheet rendered; `mid` chosen by eye as the
  shipping default (`thick`, the first authored value, crossed the mana lab's
  "eats the animal" wall). `parked` control differs by 1,027-9,425 px/frame, so
  the follow is proven to do something on screen.
- **JOINT PLACEMENT (D7 §9.1)** (commit `ea5ae326`). Verified first, as the
  direction demands: pass 8 fixed the MOTION, the PLACEMENT was still wrong.
  stNeck 586 and stD 2030 sat in smooth run; the re-entry ball 2660 had no
  joint. Stations re-cut onto the five balls/junctions. Total band length
  unchanged.
- **THE GATE** (commit `be344419`). joints-on-balls in the committed band probe,
  PASS on the shipped geometry, and PROVED FAILABLE against pass 8's own
  stations (266 mm / 380 mm from the nearest ball vs a 120 mm tolerance).

### Constraint discovered and written into the source
Skinning stations must be >= 2*blend = 330 mm apart or the ladder's branch flips
before the previous weight saturates and the skin steps. That is what forces the
spare bone to share the front junction's pivot rather than being deleted.

### HUE FLAG for the owner (D7 §10.3's check, NOT acted on)
The owner calls the mist "greenish". On screen it renders AQUA/TURQUOISE. Per
instruction I have NOT pushed the hue green; it goes to him with plates.

### IN PROGRESS
Rendering manafold-antenna-fixed + manafold-channel to LOOK at the joints.
NEXT: D7 §9.2 (knead must rotate AND stretch the shapes, deliberately, no spazz).
⚠ D7 §11 STAGE 2 (tight particle-to-fold sync) is explicitly NOT this pass.

### 1200-1430
- §9.1 joints: moved D to the re-entry ball, **closure broke**, established the
  pass-8 baseline (989/1013 OK), swept arm length and anchor, reverted D, and
  paid for the neck's doubled lever (kKneadGripNeckA16 4100->2100). Closure back
  to 989/1043. Re-entry ball's joint declared OWED, printed by the probe.
- §9.2: found the shape rotation had a period LONGER THAN THE CLIP (430 vs 400)
  and a peak rate of 0.09 deg/frame. Amplitudes/periods raised; still pure
  sinusoids so reversal density is unchanged.
- Archive gap CLOSED: pass 7 archived (outgoing), passes 3 and 4 reconstructed
  from git. Two corrections to the plan found by doing it (the `unnamed02` name,
  and a 0-byte `inspect` clip that cannot be recovered).
- `PASS-9-FINDINGS.md` + 4 plates committed beside the creature.

### IN PROGRESS
Full 22-clip bank render (18/22). Then encode, assemble, deploy.
`checkmedia.py` running over the whole site in parallel.
