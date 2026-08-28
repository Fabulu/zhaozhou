# Task Log: RUN-20260828-1939 - [Describe objective here]

**Created:** 2026-08-28 19:39 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260828-1939-zixxtrixx-v7-six-faults/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-28 19:39 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260828-1939
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

## Owner review of the front-S build — EIGHT items (coordinator relay)

Opens with *"It's better than it was though. We're getting there."* Direction is
right; keep what works.

1. **The fin rotation rotated the wrong thing — MY RELAY'S FAULT.** He had said
   "further apart and rotated 80 degrees"; I passed on "rotate them" without
   flagging that "them" was ambiguous. He meant the TAIL END rolls and the blades
   inherit it. Blades back to zero, roll the tail bones. Also the sounder
   construction: a blade rotated independently of its limb fights that limb in
   every clip; a rolled tail carries its blades for free.
2. **Front of the face is FLAT, must round** — *"without attaching a weird
   helmet."* Hard constraint: the head-shell overlay was deleted once for causing
   self-intersection and must not return. The dome comes from the terminal rings
   of the one continuous tube. Blunt is not flat.
3. **Eyes forward, DOWN a little, more bulge**, still side-mounted — between the
   two recorded failure modes (bulge 42 became a lateral BRIM/hat; the orange
   rings once MET across the crown as a chinstrap).
4. **Visible neck SEAMS, pose-dependent.** Intermittent is diagnostic: a seam
   that shows only in some poses is GEOMETRY OR NORMALS, not texture. Likely the
   position-keyed normals no longer keying identically across the seam after the
   front-S centreline change, or weights differing across the boundary. Diagnose
   unlit + normal-viz first. *"Pull some skin over that"* is the right instinct —
   continuity, not a patch.
5. **Too fat after the neck, for too long — "you barely see the S anymore."** The
   girth-850 ladder scaled everything UNIFORMLY; this is the profile's SHAPE, not
   its scale. Shorten the fat run, steepen the drop behind the neck. Re-scaling
   down would cost the neck its prominence.
6. **Tail-balance fall rigid "like a stick"**; the clip generally "wonky".
7. **Taunt too rigid — "all parts of body should bend."**
8. **Hits: the wobble is APPROVED, the impact is not.** *"Really bend the hit part
   of the snake out of shape."* Not a bigger shove — DEFORMATION at the struck
   stations, sharp and brief, released into the existing ring-out.

### Two patterns worth naming
**Rigidity is SYSTEMIC.** Items 6 and 7 join the falling flail (*"rigidly falls
over like a log"*) and the look-around (*"twitchy"*) — four clips. The recurring
fault is animating the section performing the action while the rest is carried as
a rigid rod. On a serpent no part is uninvolved. Cure: the idle's four-period
living body running UNDERNEATH each performance (`idle_body(amp)` exists for
exactly this). One systematic pass, not piecemeal fixes.

**Item 8 is the deliberate exception to a law we have been enforcing for seven
runs.** Every pass has been told bends must be smooth — no kinks, no notches. But
a body that CANNOT deform sharply reads rigid, which is the very complaint above.
**Smoothness is the resting law; violence is authored** — extreme at the contact
key, confined to a few keys, resolved by the approved wobble.

**Cheap diagnostic recorded:** contact-sheet every frame and look for a region of
the silhouette identical across the sheet. A row of unchanging shapes IS the
rigidity, visible at a glance where per-frame inspection misses it.

*Note: the previous agent's transcript was gone, so this went to a fresh agent
with full context rather than a resume.*
