# Task Log: RUN-20260826-1615 - Zixxtrixx remake (concept match)

**Created:** 2026-08-26 16:15 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260826-1615-zixxtrixx-remake-concept-match/

---

## Objective

Rebuild Zixxtrixx from S. Hofer's concept sheets per `zhaozhou/reports/MODELINGGUIDE`.
The existing model is a technical prototype, not an approved likeness. NOT a
hardware-budget rescue job: the console resolves to RGB565 and the 256-colour
rule is a GIF-export gate that was wrongly allowed to redesign the creature.

---

## Progress Timeline

### 2026-08-26 16:15 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260826-1615
- Created working directory
- Taking over from the hardware agent's handoff (REMAKE-BRIEF + REMAKE-WORKPLAN, 372148e).

### 2026-08-26 16:20 - Read the authoritative text; two requirements the distilled workplan lost

Read MODELINGGUIDE (488 lines) in full, then REMAKE-BRIEF and REMAKE-WORKPLAN.
The workplan is a faithful distillation of the advice section, but **two things
in Fabian's own words at the head of the file did not survive into it**:

1. **"The first animations we keep in an archival tab."** (line 2) The website
   must keep the prototype's slither and strike, not replace them.
2. **"At the end it should come straight down. So salto up, become like a
   straight spear and smash down with real power."** (line 6) The triple salto
   ENDS AS A STRAIGHT SPEAR travelling straight down. The workplan only says
   "the tail prongs spear downward in front of the head", which is weaker and
   loses the straightening.

Both are now in SPEC_v1 scope.

Also noted: the artist is **Sabina** ("Sabina's drawing", MODELINGGUIDE §13),
signed S. Hofer.

### 2026-08-26 16:25 - Recon subagent spawned (step 0)

One read-only agent, per the workplan. Brief covers: why the model does not
match; what in the reference implementation permits or blocks a continuous
skinned shell; which texture path exists and which seam is missing; the exact
transform between an authored colour and a pixel; how Sacrifice avoids
disconnected sections; an animation vocabulary including continuous rotation
composition; and costs. Its report goes to FINDINGS-R1-remake-recon.md in this
folder. **The design is mine; the report is evidence, not a decision.**

### 2026-08-26 16:35 - COLOUR: measured properly, and the first pass's error found

Fabian, mid-turn: *"enjoy the 16 bit colors and make damn sure you get the
colors right."* So this went first.

**The first pass measured the MEDIAN of each crayon region.** That is wrong in a
specific direction: crayon coverage is uneven and white paper shows through, so
a region's median is the pigment *lightened by an unknown amount of paper* --
and by a different amount per region. The pale results then looked washed out
when rendered, so they were **hand-saturated to compensate**, and the
compensation was recorded as deliberate art direction. It was not: it was an
eye-correction for a measurement error, and it went the wrong way.

**Correct method:** inside each region drop ink and bare paper, then take the
mean of the most-saturated quartile -- the strokes where the crayon actually
laid down colour. No hand-correction needed afterwards.

| | measured properly | first pass shipped |
| --- | --- | --- |
| pink | 233,188,206 | 206,130,175 (too magenta and dark) |
| yellow | 243,232,142 | 250,226,92 (too acid) |
| blue | 3,145,205 | 20,163,213 (too pale) |
| green | 120,184,68 | 116,205,147 (far too mint; the sheet is grass green) |
| orange | 218,106,71 | DROPPED for the GIF palette |

**RGB565 costs at most 6/255 on one channel across the whole palette.** The
format is not a constraint on this creature and no colour decision may be
justified by it.

**The orange is ONE pencil serving TWO features** -- the eye ring in Front.png
and the wavy slit pupil in Side.png. Measured (211,96,68) and (220,110,73), the
same crayon at different coverage. REMAKE-BRIEF was right not to reconcile them
by dropping one.

Evidence shipped as `Upheaval/creature/Zixxtrixx/PALETTE.md` +
`PALETTE-PROOF.png` (every swatch beside the crop it came from, with its RGB565
roundtrip) + the two scripts under `Zixxtrixx/tools/`.

### 2026-08-26 16:40 - Looked at both sheets myself

Not working from descriptions. What I see, confirming REMAKE-BRIEF:

- The S is a **ribbon** with bands running ALONG its length: pink on the outer
  (dorsal) edge of the whole curve, green as the body band, blue at the head and
  running back along the inner throat. This is why a per-part flat colour cannot
  express it and why the stripe has to be texture or a following region.
- **TWO tail blades**, narrow, tapering, green-centred with pink edging on both
  edges, with a small V notch at the fork. Not three horns.
- Side-view eye: yellow disc, **heavy black ink ring**, red-orange wavy vertical
  slit pupil. The orange RING is the front view's feature.
- Every form carries a heavy black ink contour. There is no outline pass; this
  is a real part of the drawing's identity and is currently unreproducible.

### 2026-08-26 17:10 - Recon returned; two corrections from Fabian

Recon report: `FINDINGS-R1-remake-recon.md`, 1,278 lines, sections A-H. The
design is mine; that file is evidence.

**Fabian corrected me twice, and was right both times:**

1. **The tail.** *"It has two prongs, left and right, far apart, flat, but
   pointy. And in the middle, there's a tiny one."* Verified at native
   resolution: a short sharp spike stands in the V between the two big blades.
   `REMAKE-BRIEF.md` says "TWO prongs, not three" -- that is WRONG, and acting
   on it would have deleted a feature the first model already had right. The
   brief is now annotated with a correction. The separation is LATERAL; the
   first model splayed them about Z (up/down), the wrong axis.
2. **The front view's "big bulgey head" is an optical illusion.** *"The bulge on
   front view is just the snakey part rising up above the head."* Segmenting by
   pigment proves it: in Front.png the PINK sits at y 191..304, ABOVE the blue
   head at y 280..574. Pink is the dorsal band, so that mass is body arching
   over the head. My "broad flattened skull, 1.29:1, at 1.41x the body" was
   measuring head PLUS arch. Corrected to ~1.12:1 -- modestly wider than tall,
   not dramatically flattened. The elliptical-ring case is weaker for the skull
   than I wrote, though it still holds for the flat tail blades.

Also from Fabian: **"stop asking questions and barge ahead"** -- permanent,
saved as [[barge-ahead-dont-ask]]. **One run per creature** from now on, added
to [[runs-are-mandatory]]. And the model must be able to REACH the concept
silhouettes, so those masks are the canonical stance target, not merely a
comparison image.

### 2026-08-26 17:30 - THE COLOUR FIX, both halves of it

Recon section D found the other half, and it is the bigger half.

**RGB565 is innocent.** Worst quantisation error across every candidate colour
is 4/255, and resolve applies a 4x4 Bayer dither on top.

**The damage was the shading chain.** One white key light, no fill, no light
colour: `s = 0.25 + 0.75*lambert`, quantised to 1/16, applied as a SCALAR
multiply to R, G and B alike. Two measured consequences:

- On a body of revolution, **six of twelve faces -- half the surface -- land on
  the identical ambient floor.** Not shaded: one flat dark colour, no form.
- A scalar multiply preserves the hue RATIO while collapsing ABSOLUTE chroma.
  The sheet's dorsal pink has a channel spread of 23 counts at full light and
  **6 at the floor.** Six of 255 is not a colour. **That is the grey helmet**,
  and the saturation push was the only lever available against it.

MODELINGGUIDE:249 predicted exactly this: *"Do not solve pastel colours becoming
muddy merely by pushing saturation harder. First correct the material response,
lighting and texture."*

**Fix: white key + cool per-channel ambient + warm bounce fill from below.**
`shade_flat_tri_dir` factored out of `shade_flat_tri` (arithmetic verbatim, so
terrain is untouched); `creature_light()` composes the three terms per channel.

**The constants were SOLVED, not chosen** -- a 120,000-point sweep over fill
direction, key strength and both tints, scored on shadow floor, worst-case
chroma spread of the concept's pink, distinct face values, and highlight
neutrality. `tools/tune_creature_light.py` reproduces it. Over 36 faces:

| | old | new |
| --- | ---: | ---: |
| darkest face gain | 0.250 | **0.375** |
| distinct face values | 7 | **21** |
| worst pink chroma spread | 11 | **17** |
| brightest face | 0.938 | **1.000** (unity was unreachable) |
| highlight neutrality | n/a | exact |

My first hand-picked rig made two of those four metrics WORSE -- it fixed the
flat half but pushed the floor to 0.188 and chroma to 9, because the fill
direction wasted its X component on a body whose normals lie in the Y-Z plane.
Solving for the constants instead of guessing them is what fixed it.

The sweep chose a WARM fill from BELOW under a cool ambient without being told
to prefer it -- ground bounce off the ochre terrain plus sky. Physical coherence
fell out of the scoring for free.

### 2026-08-26 17:45 - The measured palette shipped, and the GIF gate demoted

`zixxtrixx.h` now carries the sheet's own pigment: green 120,184,68; pink
233,188,206; blue 3,145,205; yellow 243,232,142; **orange 218,106,71 RESTORED**
and back on the eye rim. Prongs re-splayed about Y (left/right) at +-39 degrees,
with the middle spike straight back.

The per-channel rig tripled the shade count, so creature subjects now exceed 256
colours -- which is the guide's point, not a regression. Added
`SceneSubject::full_colour`: a subject that sets it is exempt from the palette
gate and ships full-colour frames. Set on both Zixxtrixx subjects AND both
watchdog creature subjects, because the rule must not shape any creature.

**Consequences recorded rather than hidden:**
- `creature-wave-walk` re-pinned 0x6BEECDE5 -> 0x4B8730D6; `creature-bulk-pop`
  0x327DBB91 -> 0xEDBA0DD2. Intentional; the rig moves every creature render.
- **zhaozhou-site's shipped creature GIFs are now stale** and need regenerating
  through the full-colour path. Flagged in the subject source.
- Both Zixxtrixx CRCs deliberately UNPINNED -- MODELINGGUIDE section 12: do not
  pin a visual CRC until the new appearance has been inspected.
- `reel --check`: all sequence CRCs match.

### STILL AHEAD -- the geometry rebuild has NOT started

Colour and lighting are done; the model is still the first prototype's geometry
wearing correct colours. Remaining, in workplan order: the 2-bone continuous
shell (recon B.5 -- the ring builder has NEVER emitted a blend, every vertex is
`{part.bone, part.bone, 64}`; ~20 lines, tooling only), the S posture, the
measured non-monotonic taper, flat blades, the texture pipeline (`tools/pack/`
is an empty `.gitkeep`), caterpillar locomotion, the triple salto ending as a
straight spear, the falling flail, slow cameras, the LOD ladder, and the
archival tab.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| 16:25 | a658b5b (recon) | visual/animation/asset-pipeline recon, read-only | DONE, 1278 lines | `FINDINGS-R1-remake-recon.md` |

---

## Files Created

- `SPEC_v1.md`, `TASK_LOG.md`
- `Upheaval/creature/Zixxtrixx/PALETTE.md` (the canonical palette + the method)
- `Upheaval/creature/Zixxtrixx/PALETTE-PROOF.png` (swatch beside source crop)
- `Upheaval/creature/Zixxtrixx/tools/pigment_sample.py`, `tools/palette_proof.py`

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*
