# FOR THE ANIMATING AGENT — owner direction, 2026-08-27 07:42

Fabian committed a 503-line art-direction report as `2f94f2e` and asked the
hardware agent to make sure you saw it, because it landed while you were
mid-pass and it is not in a file you read.

**Read it in full: `zhaozhou/reports/ZixxtrixxReport.md`.** This page is a
pointer plus the two claims I could check against the code for you, so you do
not have to re-derive them.

---

## The headline

The walk is accepted. Not "good for AI" — accepted. **Treat the current walk
clip as a golden master and do not touch the shared S, head hierarchy,
smoothing or body proportions while fixing anything else.** Preserve the clip,
its fixed camera, its contact sheet, its probe output, a GIF, and the exact
source commit before you change the hierarchy for the head-bone work below.

The rest are described as local artistic and presentation problems, not
evidence the system is wrong.

## Two claims I verified in the source, so you can act on them directly

**1. The faceting is FLAT SHADING, not polygon count.** Confirmed:
`tools/reel/zhao_reel.cpp:1649` is

    const uint16_t shade = kFaceShade[ti / 2];

— one constant lighting modulation per face — and there are **zero**
occurrences of any per-vertex normal in that file. So every polygon boundary is
a genuine discontinuity in illumination. Going from 28 to 40 sides would shrink
the facets and keep announcing every one of them.

Verdict from the report, which the code supports: **smooth vertex normals
first, more polygons second.** And keep some structure so it does not go
plastic — roughly `0.8 * smooth vertex light + 0.2 * face light`, or quantise
the smooth result onto a ladder. Smoothing groups: smooth the body/neck/skull/
eye bulges; keep the fin faces, fin rims and middle spike hard.

**2. There are 7 spare bones.** Confirmed: `tools/reel/zixxtrixx.h` has
`kSpineBones = 20` plus five blade/spike bones (`kBBladeL`, `kBBladeL2`,
`kBBladeR`, `kBBladeR2`, `kBSpike`) = **25 of the 32 limit**.

Spend them non-uniformly rather than raising every count: one dedicated
head/skull attitude bone, one or two extra neck bones, one or two around the
sharp S reversal. The long grounded run needs fewer. Smooth normals fix
lighting discontinuities; extra bones fix real silhouette hinges. They are
different problems and neither substitutes for the other.

## The one diagnostic that settles the faceting question

Render the same close-up three ways and read it off:

| what you see | what it means |
| --- | --- |
| jagged outline in **fullbright** | more geometry / bones |
| smooth outline, tiles only **under lighting** | smooth normals |
| kinks only during **extreme bends** | bone distribution / weights |
| weird planes mostly on **fins** | fin topology, not resolution |

## The order Fabian's report ends on

1. Smooth body/head normals, so the animation that already works is presented properly.
2. Dedicated head attitude control, then fix the head/neck pose.
3. Pupil UV calibration — **by brute force, not reasoning**: paint an asymmetric
   debug arrow on each eye, render front/left/right, try all eight transforms
   (0/90/180/270 × mirrored). The two eyes may need *different* transforms
   because the opposite side of a wrapped surface has opposite handedness.
   Ten-minute visual search, not a geometry problem.
4. Salto: anticipation (compress → hold → release), axis-aligned final velocity,
   visual-centroid camera.
5. Custom blade meshes for the fins — their problem is topology, not resolution.
6. Falling: fixed camera first, then replace synchronised sine layering with
   baked inertial spring motion.

## The two numeric findings worth not losing

**The salto's spear points one way and travels another.** At the final plunge
the root drops ~1,445 mm while forward travel is ~17 mm, but the body is held
30° from vertical — which wants ~1,445 × tan 30° ≈ **834 mm** of forward
movement. That mismatch is why the attack vector reads wrong. Fabian's stated
preference is to keep the 30° pose and add the forward travel, so the tail tip
moves along the creature's own long axis for the last 8–12 frames.

**The idle's static patch is structural.** The front wave acts only before the
grounded section and the tail sway only after it, so spine segments ~11–16 are
deliberately excluded from both and only receive the global girth change. Do
not fix it with another vertical hump — that turns idle into slow walking. Use
a small torsional breathing wave (1–2°, phase-lagged behind the upper-body
breath) that moves the dorsal stripe without lifting the centreline.

## Falling — the structural note, not the amplitude note

"Wobble more" will not do it alone. The source applies the **complete canonical
S at full authority every frame** before adding the flail, so the body can never
go slack and reads as a rigid S-shaped sign rotating with wiggles attached. Let
S authority vary (~45–100%), let phase lag propagate head → neck → middle →
tail → fins, and make the angular velocity non-uniform rather than a turntable.

— left by the hardware agent, which cannot message your session directly
  (`ListAgents` shows none reachable) but can commit into your run folder.
