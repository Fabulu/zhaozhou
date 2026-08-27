# FOR THE MODELLING AGENT — owner direction #2, 2026-08-27 09:09

Fabian committed a second report as `97aae26` and again asked the hardware
agent to forward it. **Read it in full: `zhaozhou/reports/Headache.md`.**

This is the pointer plus the claims I could check in the source for you.

---

## The verdict, in his words

> Zixxtrixx is good. His face is currently fucked.

The salto is now **proof the animation system works** — compression and hold,
explosive climb, 12 m apex, three rotations, an 11 m plunge aligned to the
spear axis, camera moving to the spear midpoint, impact shake, five-second
burial. **Freeze it**, along with the walk and the idle body movement, and do a
**head-only run** that is forbidden from touching the shared S, the walk curves
or the salto timing.

## Checked in the source

**1. The polygon change went the wrong way, and it is confirmed.**
`tools/reel/zixxtrixx.h:69` is now `constexpr int kSides = 30;` — up from 28 —
while `tools/reel/zhao_reel.cpp` still shades per face (`kFaceShade`, two
references, and no per-vertex normal anywhere). So the creature gained
triangles (~3,360 body tris at 30×57 before head/fins) and kept every polygon
boundary announced by the lighting.

That is the wrong trade, exactly as the report says: **more tube triangles,
same visible faceting.** Smooth vertex normals are the cure; sides are not.
Direction #1 said this and it is still true.

**2. The droop: the source's own comment is the thing to distrust.**
`tools/reel/zixxtrixx.h:257` asserts `+4000 (~22 deg up), not less: the
showcase cameras look DOWN ~15 deg`, and line 260 carries the `4000`. The
publication shows the head drooping anyway.

**Stop deriving this verbally.** Render a sweep from ONE fixed side camera at
`-8000, -6000, -4000, -2000, 0, +2000, +4000, +6000, +8000`, put all nine on
one contact sheet, and pick the one that looks right. Ten minutes of brute
force beats another paragraph explaining why +4000 ought to work.

This is the same shape as the pupil-transform advice in direction #1, and it is
the second time the same class of mistake has cost a pass.

## What the report says to fix, in his order

1. **Rebuild the skull volume BEHIND the nose.** The taper was raised at t=0
   (870→1050) with the four-ring nose dome left in place, so the whole forward
   tube inflated — that is where the protruding snout came from. Restore the
   first two or three nose rings, move the added volume rearward to eye
   stations ~3–8, widest at the eyes, then taper decisively into a smaller,
   blunter nose. **Larger cranium, not larger muzzle.** Do not chase the nose
   with dome multipliers; the volume is in the wrong place along the axis.
2. **Give the skull independent attitude control** — the dedicated head bone
   from direction #1. There are 7 spare bones (25 of 32). It lets the head look
   up without re-solving the canonical S and risking the accepted walk.
3. **Eliminate or shorten the overlapping head shell.** The blue head/throat is
   a second complete ring-chain over twelve rings, only ~3 mm larger than the
   body beneath it. With a fatter bulb and a tighter hook, two near-coincident
   surfaces fight. The right fix is for the MAIN body rings to be the skull and
   the blue/pink/green to come from the texture; at minimum shorten the shell
   to the genuinely blue face/throat.
4. **Shrink the mouth by about half.** The report measures the texture as ink
   across 18 of 64 angular texels (x=8..25) — about 101°, a grin wrapping a
   quarter of the head. Target 8–10 texels, 3–4 rows, one-pixel ink boundary,
   slightly hand-wobbled rather than a perfect rectangle. I could not locate the
   mouth writer by name in `zixxtrixx_page.h`, so treat those coordinates as
   the report's measurement rather than as mine.

## The process change he is asking for, which matters more than any of the above

The agent built a **front-facing acceptance frame** and convinced itself the
enlarged head passed. That single portrait could not show the side droop, the
nose projection, the self-intersection, the mouth circumference, or clipping in
motion. **The live publication caught what the curated evidence missed.**

The acceptance gate should become: fixed side, fixed front, fixed
three-quarter, one slow orbit, maximum idle bend, maximum walk bend, the
compressed salto anticipation pose — plus an automated **non-adjacent ring
overlap warning**.

That probe is cheap and worth writing once: after skinning a pose, compare
non-neighbouring ring-centre distances against the sum of their radii. It does
not need triangle-exact collision. It only needs to shout when the enlarged
skull is inside the neck or trunk.

— left by the hardware agent; `ListAgents` still shows no reachable session, so
  this is the channel.
