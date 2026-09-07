# TASK_LOG — RUN-20260907-1828-manafold-p12-implA

**Role:** IMPLEMENTER A, Manafold pass 12 — form, rig, motion.
**Lane:** `C:\programmieren\zencrifice\manafold-p12-a\{zhaozhou,Upheaval}` (own clones from origin/main).
**Plan:** `Upheaval/creature/Manafold/PASS-12-PLAN.md` — Wave 0 D1 + D4, then Wave 1 lane A (A1–A6).
**Files I own:** `manafold_art.h`, `manafold_rig.h`, `manafold_clips.h`, `manafold_model.h`, probes.
**Blocked on:** B's edge-radii commit must land on origin/main before I touch `manafold_art.h`.

---

## Log

### 18:28 — lane set up
Cloned Upheaval + zhaozhou from origin/main into `manafold-p12-a/`. Run initialised.

### 18:30 — read the binding direction
OWNER-DIRECTION-9 (all 12 sections), PASS-12-PLAN §0–3 and §6–10.

### 18:35 — COORDINATOR MESSAGE: D9 §12 landed (owner correction on the eyes)
Pulled Upheaval; §12 is now in the file. Two things:
1. **The star is CENTRED at rest** — supersedes D7 §12.1's "high as drawn".
   Centre is the DEFAULT; gaze/roll/twinkle/travel are departures that return
   to it. *"They're not centered in the eye enough as it is"* = a LIVE DEFECT
   in the current build, not just future policy.
2. **A "cyan lozenge" at 45° is a DEFECT.** The white outline is part of the
   rigid star unit (D5 §5a/§5b) and must travel with it at every angle. The
   eye lab's "thicken cyan, slim white" trade is exactly what makes the white
   vanish side-on — the white may need a MINIMUM ON-SCREEN WIDTH rather than a
   fixed world thickness.
This is Wave 2a work (Implementer C, "may be A"). Recorded here so it cannot be
lost; if the rest-centre offset lives in a file I own, I fix it in this lane and
say so. Otherwise it goes to the findings as a named handoff.

### WHERE I AM (kept current, per CLAUDE.md's "write down where you were")
- Next: read `manafold_rig.h` / `manafold_art.h` / `manafold_model.h` to locate
  the taper table, the loop-pose chain, the re-entry anchor and the wag drive.
- Then: Wave 0 D1 (round-body silhouette probe) and D4 (rear-hinge spazz).

### 18:40 — COORDINATOR MESSAGE 2: D9 §12.3 (the eye ROOT CAUSE)
Pulled. The eye parts are flat; travel moves them around the ball without
TURNING them, so they foreshorten to nothing. Fix is ORIENTATION (each part
turns by the travel angle so it keeps facing along the surface normal), not
thickness — and the lab's `bar-cyan-fat` compensation must be RE-MEASURED and
probably REVERTED. **This raises A1 (round body) in my lane**: on a ball the
outward normal is trivially the direction from centre.

### 18:45 — ⚠ LOAD-BEARING FINDING FOR WAVE 2a: THE EYE TRAVEL IS NOT SHIPPED
`manafold_art.h:757  constexpr int32_t kEyeShiftPivotMm = 0;   // NOT SHIPPED`
and `eye_shift_a16()` (manafold_clips.h:298) returns 0 unconditionally while
the pivot is 0. **`apply_eye_shift()` therefore moves nothing, in every clip,
today.** There is no `kEyeTravelMaxDeg` constant anywhere in the tree.

So D9 §6 ("45° is back on"), §6.1 (standoff), §6.2 (trace the bounce), §12.2
(the white must survive 45°) and §12.3 (rotate about their own axes) all
describe behaviour of a channel that **does not run**. Wave 2a is not a tuning
job on a shipped travel — it is BUILDING the travel, and §12.3's own-axis
rotation should be authored INTO it from the start rather than added after.
Pass 7 declared this gap honestly (the comment is true); it has simply never
been closed. Reported up, not silently absorbed.

### 18:47 — baseline build OK
`build-direct.sh --output ../build cel` -> BUILD_RC=0, `zhao-reel-cel.exe`.
