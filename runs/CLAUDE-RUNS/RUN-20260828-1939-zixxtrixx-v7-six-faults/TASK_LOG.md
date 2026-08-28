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

### Items 9-11: the salto family

9. **The salto CAMERA is jittery** — *"Salto camera is too jittery."* Hypothesis
   relayed: the tracking camera follows a point attached to the SPINNING body, so
   the rotation transfers into the shot; the six-somersault variant is worst
   because twice the revolutions means twice the transferred wobble. It should
   track a SMOOTH quantity — the ballistic path, or a low-passed centre —
   something describing where the animal is GOING, not how it is ORIENTED. Falls
   under the recorded rule that the camera is part of the animation and must
   frame the moment rather than ride the subject. (This is also the legitimate
   reason to touch `trk_*`, which the owner had queried earlier.)
10. **The six-somersault spin is still wrong** — *"the saltos aren't the elegant
    wheel but a weird jittery spin, totally wrong."* The previous run's fix (a
    phase-boundary theta snap, made one continuous accumulated function) was
    NECESSARY BUT NOT SUFFICIENT. The authored three-salto's character is an
    **elegant WHEEL**: the body holds a constant curl and ROLLS. A "jittery spin"
    is what you get when the SHAPE changes during rotation — so the fault is
    likely the coil parameter or per-joint distribution varying through the spin,
    not the interpolation. The wheel needs the curl HELD while theta advances.
    His shortcut stands: take the authored clip's own curves and give them more
    revolutions; do not defend a generated result that looks worse than the
    hand-authored one. Diagnostic: contact-sheet both — a constant rotating
    silhouette is the wheel, a shape changing frame to frame is the fault.
11. **The flying salto target should have WINGS** — it is currently the watchdog
    quadruped hovering, which reads as a ground animal suspended. Reel-only prop:
    keep it cheap, out of the site card and out of the creature page set, and
    give the wings a little motion since a static winged prop reads nearly as odd
    as a wingless one.

**Pattern worth noting across 10 and the earlier salto fix:** a correct mechanical
fix (continuous theta) did not deliver the desired READ, because the fault was
never only in the interpolation. **A gate passing is not the thing looking right**
— the same lesson as the head, where a measured-horizontal snout still hung from a
descending hook.

### Item 12: the mouth

*"The mouth should move up some closer to where the supposed nose is to be more
visible. Should be in line with the drawing."*

Move it up the head toward the snout end. With the head now carried level after
the front-S reconstruction, a mouth low on the underside is invisible from every
camera the creature is actually seen from.

**Which sheet governs, and why this is not a contradiction:** the standing rule
is `Side.png` owns FORM and `Front.png` owns MARKING PLACEMENT. **A mouth
position is a marking-placement question, so `Front.png` IS authoritative here** —
this is precisely the case the front sheet is for. The "front sheet is wonky"
finding concerns its PROPORTIONS and apparent bulge, not where features sit on
the face. Worth stating plainly, because the front sheet has been (correctly)
distrusted for seven runs and that habit could now overshoot into ignoring it for
the one job it is good at.

Two things on record so they are not re-earned:
* The mouth was once ink across **18 of 64 angular texels — about 101° of
  circumference**, a cartoon grin wrapping a quarter of the head. It was cut down
  deliberately. **Do not widen it while moving it.**
* Moving a small feature up a curving dome risks it foreshortening into
  invisibility or smearing across the crown. Judge it RENDERED at 240p from the
  front and three-quarter cameras; a texel-space check cannot tell you whether it
  reads.

Texture-page change, not geometry — cheap. Sits with the head block.

### Item 13: stray triangle in the right eye (death2) — a BUG, not an art call

*"Also in death 2 I see a weird stray triangle in the right eye."*

Pose-dependent by inference: it shows in `zixxtrixx-death2` and nowhere reported,
so the geometry is fine at rest and breaks at a pose that clip reaches. That
points at SKINNING rather than topology — **a vertex bound to the wrong bone sits
in roughly the right place while neighbouring bones are aligned, and flies off
the moment a pose separates them**, dragging its triangles into a spike. Death2
keels the animal, so it diverges further than most clips. The eye discs have been
resized and moved twice recently (17×33 → 12×24, and moving again for the
forward/down/bulge item), so a leftover vertex from those edits is the prime
suspect. Cheap to also rule out a degenerate/flipped triangle.

Method required: contact-sheet every frame to find the triggering keys; render
those keys zoomed, **unlit and with normal visualisation**, to separate "wrong
place" from "shaded wrong"; then **interrogate the posed vertices with a
committed probe, not an inference from the image** — same principle as ground
contact and the tail tip, and `zixx-probe`/`zixx-striketip` are the precedents.
Check whether the same vertex misbehaves at lesser magnitude in other clips: if
so it is one bug with one visible symptom and fixing it clears latent damage.

**Explicitly forbidden: papering over it** by nudging the disc or hiding it under
a texture change — that leaves a broken vertex to resurface in the next clip that
bends far enough.

**Gate implication worth recording:** probe, choreo, planner and `--check` all
pass while this is visibly wrong. The owner spotted it by looking. If a
mesh-integrity check would have caught it, that gate is worth having — this is
the same lesson as *"component checks passing is not likeness evidence"*, now in
its correctness form rather than its art form.

### 2026-08-28 2x:xx - Agent start: the thirteen-item run

Read first, in full: CLAUDE.md (art law), 01-RING-CONSTRUCTION.md (sheet
gate), 03-ANIMATION.md (house style), OWNER-DIRECTION-3 (front-S laws),
RUN 1730 TASK_LOG, zixxtrixx.h complete (4776 lines), zhao_reel subjects,
mkcreaturepage.py eye/mouth regions, sidecmp.py.

THE PLAN, in blocks, head first (the owner looks there first):
  H1 fat run behind the neck (fault 5): shorten + steepen the kTaper drop.
  H2 flat face (fault 2): dome the terminal rings of the ONE tube -- extra
     nose rings if V mapping allows (checking compiler V law first), no
     overlay part, ever.
  H3 eyes (fault 3): forward + down + more bulge; between the recorded
     brim-42 and chinstrap failures. Geometry stations + page rows.
  H4 mouth (item 12): up toward the snout per Front.png (marking placement
     is the front sheet's jurisdiction). Not wider.
  H5 neck seams (fault 4): DIAGNOSED FROM SOURCE before rendering: the
     junction ring (station 11 = kHeadEnd) binds {kBHead w=6/64, spine}
     in the head part but {spine3 w17, spine4} in the body part --
     head_station_bind's blend window INCLUDES kHeadEnd, so the two
     copies of the "bit-identical" ring skin apart whenever kBHead or
     spine3 moves: a pose-dependent open seam, worst mid-breath.
     ~12 mm at full idle head-lift by hand estimate. Will verify unlit +
     with a junction-gap probe before fixing (fix: blend must reach ZERO
     at kHeadEnd and hand over to station_bind exactly).
  H6 stray right-eye triangle in death2 (item 13): probe the posed eye
     vertices at the offending keys; no papering over.
  T1 tail (fault 1): kBladeRoll -> 0; new kTailRoll rolls the END OF THE
     TAIL (last spine joints, distributed) about the tube axis inside
     tail_rest so every clip inherits it and the blades ride along.
     Splay 3000 kept ("further apart" stays), spike + two-edge colours
     verified after.
  R1 balance fall (item 6): progressive buckling on the topple (per-joint
     lag + overshoot), fight that grows and fails, impact ripple, head
     reacts; idle-recipe life under the whole clip. Wobble not jitter.
  R2 taunt (item 7): rebuilt on idle_body underneath the performance.
  R3 hit deformation (item 8): sharp local fold at the struck stations,
     few keys, released into the APPROVED ring-out (untouched).
  R4 rigidity audit: strike/notify/knock family contact sheets, any
     frozen region gets the idle recipe.
  S1 salto camera jitter: track a smooth path, not the spinning body.
  S2 six-salto: the authored three-salto's own curves with more turns --
     the wheel must HOLD while theta advances.
  S3 wings on the flying dummy (reel prop only).
Then: gates green, goldens re-pinned with provenance, all 17 site clips
re-rendered to canonical names, deploy NOT run.

### Item 14: the taunt gets the Indian head wobble

*"Taunt should include the snake doing a cheeky fast side to side to side etc.
headshake, like the Indian 'I am being funny' headshake."*

The defining characteristics, because getting them wrong makes it a different
gesture:
* **It is a TILT/ROLL, not a turn** — ear toward shoulder, not yaw. A yaw reads
  as "no"; the roll reads as playful. Most important part.
* A slight figure-eight quality rather than a flat metronome.
* Quick and light, three to five repetitions, then done.

**Rig check required:** the head-aim bone was built for yaw and pitch ("left and
right, up and down"). If it cannot ROLL, this gesture cannot be authored on it.
Adding the axis must obey the recorded rule to **compose rotations properly
rather than switch axes on a threshold**, which pops on every crossing.

**A DELIBERATE EXCEPTION to the house style, made consciously.** The standing law
is *wobble is not jitter — fewer and slower, not more and faster*, and this
creature has twice been rejected for motion reading as vibration. **This gesture
is DEFINED by being fast and light**; slowing it into a languid sway destroys it.

The resolution is a LAYERING, and it is worth keeping as a general technique:
**fast crisp gesture on top, slow loose body underneath.** That contrast is what
reads as personality — fast head plus fast body is the buzzing that was rejected.
It also satisfies item 7 ("all parts of body should bend") without contradiction:
the body's liveliness is the slow layer, the gesture is the fast layer on top.

Judging note: **a roll is nearly invisible in pure side view** — use a front or
three-quarter camera, and check it at 240p, where a subtle tilt on a small head
can vanish entirely and may need more amplitude than looks right in a zoom.

### Item 15: texture experiments — licence, with the colour scheme fixed

*"See if you can make textures cooler. Not with more resolution, but by changing
the textures. Make some experiments and put them up next to the normal snake
animations. Tag them as experimental so you can go ham."* Then, clarifying:
*"The colour scheme should stick but texture details are fair game."*

**Explicitly NOT resolution** — the 256×512 atlas stays. The surface itself is
what should get more interesting, and the experimental tag exists so bold swings
are free. A timid variation wastes the licence.

**FIXED:** the palette (fought for hard — a pink measured off the scan read grey
at 240p and had to be re-chosen by eye; that is `CLAUDE.md`'s canonical lesson),
and the circumferential law (pink top / light-green sides / dark-green underside
/ blue triangular bib).

**FAIR GAME:** how the pigment is laid down (stroke direction, tooth, coverage,
wax build-up, paper showing through, hatching); how the colour boundaries behave
(hand-wobbled edges, crayon overlap, heavier deposit where a stroke turned,
instead of computed gradients); detail WITHIN a colour (chevrons, banding, scale
suggestion, in the creature's own colours); value/saturation variation.

**The strongest single idea, and it has never been tried: THE BLACK INK CONTOUR.**
Both sheets carry a bold black outline around the creature and around every
colour region. Our renders have none. That line is the drawing's most recognisable
feature, and black line work is not a palette change.

**Scope test recorded:** *would someone glancing at it still say "that is
Zixxtrixx, drawn differently"?* Yes → texture detail. "That is a different
creature's colours" → out of scope.

Constraints: determinism (fixed seed, `zlib.crc32`, two regens cmp-identical),
RGB565 direct colour, and **judged at gameplay distance through the mip chain** —
a texture that is gorgeous at zoom and mush at 240p has failed, which is why
grain amplitude had to be raised once already. Published as clearly-labelled
experimental tabs in their own group beside the normal animations; raising
`MAX_TABS` means moving `assemble.py` and all three `style.css` selector families
together. **The normal snake must render byte-identically before and after** —
that is precisely what makes a wild experiment safe.

**Queued LAST**, behind the head/neck block, the rigidity pass and the salto
family. It must not delay the head.

### Item 15 expanded: ALL the experiments, plus three of mine

*"Contour sounds fire! Do all of them, think up two more, this might be going
places."* Then: *"If it's not a lot of trouble maybe try to cel shade one or two
versions."*

Build the whole set, each as its own labelled experimental tab: the **black ink
contour** (the sheets carry it around the creature and every colour region; our
renders never have), directional strokes, wax build-up at the melds, paper tooth,
hand-wobbled boundaries, drawn snake markings.

**My additions, chosen to PAIR with the contour rather than compete with it:**

**A) "Colouring outside the lines."** In a hand-coloured drawing the crayon never
registers exactly with the ink: it overshoots on one edge and leaves bare paper on
another. **That mis-registration is the signature of hand-colouring**, and it is
invisible in every computed texture because a computed fill always registers
perfectly with its own boundary. Vary the offset along the length so it wanders
rather than reading as a uniform margin. With the contour, this may be the whole
effect.

**B) BOIL — the only TEMPORAL experiment.** Hand-drawn animation is redrawn every
frame, so line and fill shimmer frame to frame. No static grain achieves that, and
at 240p a boiling contour could look extraordinary. **Determinism is satisfiable:**
derive it from the animation key or tick — never wall-clock, never `Math.random`,
never process-salted `hash()` — and it stays replay-exact. It will move the
sequence CRCs of any subject using it, so keep it on experimental subjects.

**C) CEL SHADING — a LIGHTING experiment, the first in the batch.** Should be
cheap: the Gouraud path already computes a per-vertex lighting term, so cel
shading is that term **quantised to two or three steps** before it modulates the
texel — a small transform on a value we already have, thresholds as named
constants. No new lane, no silicon question.

**And the pairing worth stating: cel shading + the black contour IS the concept
art's own look** — bold outline, flat crayon fill, no smooth shading. At least one
variant must combine them; separately they are two effects, together they may be
the thing. Do a hard two-tone and a softer three-tone, because at 240p a hard
terminator can read as a mistake on a curved tube and that is a looking question.

Bonus if cheap: faint pencil under-drawing showing through the crayon.

**Scope note carried:** paper white, graphite grey and ink black are colours of
the MEDIUM, not of the creature. "The colour scheme should stick" governs
Zixxtrixx's own pigments. **Cel shading is conditioned on being easy** — if it
demands restructuring the shade path or touching the hardware lane's territory,
stop and say so.

All of it gated to experimental subjects; the normal creature must render
byte-identically throughout. Still queued LAST.
