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

### 2026-08-26 - Fabian's review: "a giant mess"

He looked at the built creature and rejected essentially all of it. Recorded
verbatim because the distilled version has lost detail twice already:

> "The front of the face is mangled beyond belief. It's a weird spinning discs.
> The colors aren't at the right place. I think we can give more polygons for
> this thing to actually look rount. The fins are gargantuan while on the
> reference sketch they are small. All the animations are super wrong. Idle
> phases into the ground, doesn't bob, no S shape. Only tail wag is ok. The pink
> on the back should be like neon pink, it's just not even close to strong
> enough. The texture is completely one-colored instead of the crayon-like
> texture I expected. Walk is I guess some kind of sine wave? But it basically
> flies. It should stik to the groun, head held up, S shape, and the part of the
> body that is on the ground slowly does the caterpillar thing with the sine
> wave. Yours even clips through the ground. No clipping through the ground,
> ever. Tripple salso the thing just rolls up once. I guess the fin somehow
> rolls up three times which should be impossible. It should roll up, the whole
> body should rotate three times, then it quickly unrolls, becomes straight, and
> tail first flies towards the enemy - or the ground. Don't make it go through
> the ground, no clipping. Falling is the only thing that might be ok, but it
> should look more distressed, more frantic flailing with the face. Even here,
> have it keep its S shape. The S shape should be signature everywhere"

**The two rules that come out of this and outrank everything else:**

1. **THE S SHAPE IS THE SIGNATURE AND BELONGS IN EVERY ANIMATION**, falling
   included. Head held up.
2. **NO CLIPPING THROUGH THE GROUND, EVER.**

**What I got wrong, and the pattern behind it.** I verified each piece against
the thing I had just changed -- the palette against the sheets, the light rig
against a face table, the chain against `--check` -- and every one of those
checks passed. What I never did was look at the whole animal in motion against
the concept and ask whether it was the creature. Component-level evidence is
not likeness evidence, and I let a stack of green checks stand in for the one
judgement that mattered.

Specifically wrong:
- **the face**: the eye is painted at U=0, which is a texel column that WRAPS,
  so the disc is split across the tile seam and lands on the snout as well as
  the flank. That is the "weird spinning discs".
- **the S**: `apply_stance` divides its authority by 3, so the canonical pose is
  a third of what the constants say. That is why there is no S anywhere.
- **the ground**: nothing in the rig knows where the ground is. The root is
  ground-snapped, but every posed vertex is free to swing below it, and the
  walk's arch does exactly that.
- **the salto**: the accumulated turn is spread over the spine, but the blades
  are children of the fork and inherit the whole chain's rotation, so they
  appear to loop while the body barely rolls once.
- **the texture**: the grain is clipped to 0.84..1.16, which is +-16% of
  luminance. On screen, under a light rig whose own range is wider than that, it
  is invisible. The crayon is in the page and never reaches the eye.
- **the fins**: `kBladeW0` went 122 -> 205 to fix a "reads as spikes" note.
  The sheet's blades are SMALL. I over-corrected from one render and never
  checked the correction against the drawing.

Handed to a fable agent with the feedback verbatim.

### 2026-08-26 - Silhouette overlay gate written

`Upheaval/creature/Zixxtrixx/tools/silhouette_overlay.py`. Lifts the creature
out of a rendered frame by HUE (the reel's sky and terrain are both warm, with
R >= G >= B; the animal's green, pink and blue break that ordering), normalises
both it and the cached concept mask to a common box so only SHAPE is compared,
and reports IoU plus what each has that the other does not. Writes a three-panel
concept / model / difference image, with magenta for form the concept has and
the model lacks and green for form the model invented.

This is the check that would have caught the gargantuan fins and the flat
stance, and it is deliberately blunt for that reason.

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

---

## R2 — art-direction pass (fable), 2026-08-26 evening

Fabian's review addressed end to end; see FINDINGS-R2-art-direction-pass.md.
All four clips rebuilt, ground contact measured and authored, face fixed at
three layers (seam law, nose dome, eye footprint), neon pink shipped, grain
made visible, taper moved to hand-set KNOBS. reel --check green. Pushed:
zhaozhou 0a4a587/bceeaf4/db31848, Upheaval b4bdb08.
