# PLAN — MANAFOLD, pass 4 (architecture)

**Run:** RUN-20260905-0916-manafold-pass4-architecture · **Date:** 2026-09-05
**Against:** `Upheaval/creature/Unnamed02/OWNER-DIRECTION-4-2026-09-05.md` as
amended (the teardrop-eye section and the directional-hit addition are IN it or
relayed by the coordinator and treated as binding), judged with the pass-3
`REVIEW.md` (landed, read in full), the pass-3 QA.md and TASK_LOG, the three
`RECON-U02-*.md`, the pass-3 PLAN, `10-GATE-CHECKLIST.md`, `09-ENGINE-GOTCHAS.md`
(§0/§4/§5/§10/§11/§12), `08-LIGHTING.md`, `07-MOTION-STYLE.md`, the three
concept sheets (looked at), and `CLAUDE.md`.

This is a plan, not an implementation. Every number is a **starting point or a
comparison bracket**, never a shipped value; every shipped value is chosen by
rendering and looking at native 384x240 on the SHIPPING subject under the
shipping sun rig (gotcha §12 — grep the subject builder first, every time).
Every value is a named, editable constant.

**The pass in one line:** the creature becomes MANAFOLD — its very mobile,
fully boned antenna continuously folds its mana into recognisable shapes and
kneads them into new ones, at a distance, forever — and the eyes, glow, smear
depth, and the name itself are brought along.

---

## 0. THE CENTREPIECE — how the folding works

Designed first; everything else in the pass serves it. The owner's constraints,
taken literally: the antenna is the hand; it must be VERY mobile; it shapes the
mana **without touching it** ("like a conductor", not collision); the shape is
**a function of the joint state**; a fold–hold–knead loop runs **the whole time
the creature exists**; and the acceptance test is by eye — *a viewer sees the
antenna move and believes IT did that to the mana, with nothing making contact.*

### 0.1 What a "recognisable shape" is at 384x240

The ring pocket alone is ~10–15 screen px (gotcha §10) — too small for any
shape vocabulary. The shapes therefore live at the scale of the LOOP, filling
the ring hole and deliberately spilling a little past the tube: target ~30–50
screen px across at the house camera. The medium is **fat mote strokes plus the
smear plane**: each shape is a stencil (an open or closed polyline in loop-plane
coordinates) sampled by ~16–24 mana motes — each mote a glow splat with an
opaque core under an additive halo (R7 of pass 3 stands; particles proper cap at
15.9 px flat, gotcha §11, so "particles" render as splats) — and the smear
plane's persistence fills the gaps between motes into a solid stroke.

**The vocabulary**, chosen for legibility with blobby strokes at that size, and
for rhyme with the creature's own iconography (each stencil is a named constant
table, owner-editable):

1. **RING** — an annulus; echoes the loop itself. The easiest read; the opener.
2. **FOUR-POINT STAR** — rhymes with the pupil star. The identity shape.
3. **BAR** — a thick diagonal stroke. Maximum contrast with ring/star.
4. **CRESCENT** — rhymes with the Description sheet's rear-view drawing.
5. **TRIANGLE** — three straight strokes; reads even half-kneaded.
6. **S-CURL** — a lazy spiral; the "dough being twisted" shape.

Floor is four (ring, star, bar, crescent) if the pass runs long — see the cut
order. Stencils are authored in the loop plane (the rig's truth — a billboarded
shape would decouple from the antenna). The rest yaw (~18°) plus the
three-quarter camera keeps the plane readable; if foreshortening kills a shape
at the shipping camera, the named fallback knob `kStencilFacePm` blends the
stencil plane toward the camera while positions stay rig-anchored — an authored
compromise, off by default, judged by eye.

### 0.2 How the rig drives it — the fold is arithmetic on the posed bones

No collision, no proximity tests, no parallel animation. Once per frame the fx
lane reads the posed antenna (it already does — `FxAnchors`), and three things
are derived from **joint state alone**:

* **The anchor polygon.** The posed origins of {front junction, neck, hinge A,
  hinge B, hinge C, back junction} — six anchors once Stage B lands. Every
  stencil point is expressed as a fixed **generalized barycentric weight
  vector over these anchors** (weights authored once per stencil point, at
  rest). When the joints move, the anchors move, and every stencil point moves
  as their weighted sum — *the shape folds because the rig folds, by
  construction.* Closing hinge A toward C literally folds the shape about the
  B axis. This is the mechanism; there is no other position law.
* **GRIP** — how gathered the antenna is: the anchor polygon's area as
  per-mille of its rest area (closed form from the anchors, no new state).
  Grip drives **coherence**: at low grip the motes relax off the stencil toward
  hashed cloud offsets; at high grip they converge onto it. `kGripGamma`,
  `kCloudSpreadMm` are the knobs.
* **KNEAD** — the joint velocity: sum of |per-frame posed-angle deltas| across
  the six antenna joints, smoothed over ~4 frames. Knead drives **agitation**:
  per-mote jitter amplitude, smear feed gain, ramp-gain flicker, and the morph
  rate during shape transitions. A fast antenna gesture visibly churns the
  mana; a still antenna lets it settle. `kKneadJitterMm`, `kKneadFeedPm`.
* **The DRAG term** — the sweep direction: hinge B's frame-to-frame velocity
  vector, applied to every mote with a per-mote lag of 2–6 frames
  (`kDragLagFrames`, `kDragGainPm`). When the antenna sweeps left, the whole
  mass is pulled left a beat later, across the gap, without contact — the
  iron-filings read. This term is what makes causality legible.

Because grip/knead/drag derive from the posed joints, **every** clip couples
automatically: a startle snap scatters the shape, the headstand inverts it, a
directional hit shatters it — for free, with zero per-clip mana authoring.

### 0.3 The loop — fold, hold, knead, re-form, never obviously cycling

The antenna gets a new always-on choreography layer, `antenna_knead`, added
into every clip's antenna channels the way the life layer is (07-MOTION-STYLE
§3: seasoning that is never off), with a per-clip gain `kKneadClipPm[clip]` so
big-action clips (startle, hasty, fall, hits) scale it down and the primary
action does the kneading instead. The cycle, all durations in the 07 bands
(nothing under 16 frames; one thing at a time):

1. **GATHER** (~60–90 frames): junction and hinge joints close a few degrees;
   grip rises; the cloud condenses onto stencil *k*.
2. **HOLD** (~64–96 frames): joints hold with small tremor; the shape stands
   and READS. This is where "recognisable" is won — a beat needs 16 frames to
   register; a shape needs several times that.
3. **KNEAD** (~60–120 frames): the two junction joints and hinges work in
   alternation — small counter-rotations, the two hands wedging dough; knead
   runs high; each mote morphs along its own deterministic path toward its
   station in stencil *k+1*, and the smear paints the in-betweens as smudge.
4. Repeat with the next shape.

**Anti-cycle law:** shape order and hold durations come from `fx_hash` over a
slow cycle counter (next shape ≠ current, durations 64 + hash%64), so the loop
is deterministic (replayable, CRC-stable) but does not visibly repeat inside
any clip's length. **Wander motes:** 2–3 motes are permanently exempt from the
stencil — they leave on hashed momentum walks, curve off oddly, occasionally
exit the pocket and decay through the smear. This is the owner's "some parts of
the stuff to kinda drift off in weird ways", and it also breaks the circular
read he rejected.

### 0.4 How the centrepiece is judged (first-class, by eye)

* **The naming test.** A HOLD-phase contact sheet at native and 2x: can you
  name the shape without being told? Per shape, per the shipping camera.
* **The causality strip.** 12 consecutive frames spanning one knead gesture,
  antenna and mana both in frame: the mass must move visibly AFTER and ALONG
  the antenna's sweep, with no contact anywhere in the strip.
* **The ablation gate** (07 §3, "ablate before you blame", used in reverse):
  render a diagnostic with `antenna_knead` zeroed. The mana must go limp — a
  loose cloud. **If the mana looks the same with the antenna frozen, the
  coupling is decorative and the centrepiece FAILS**, whatever else passes.
  This is the check that can actually fail (gate item 6), and it is cheap.
* **In motion**, on the published clips, beside the sheets — the whole-thing
  look that no component check replaces.

### 0.5 What it costs

Arithmetic, not measurement (gotcha §5); unit = one full-screen pass = 92,160
pixel-visits. Per conduit: 20 motes × (r≈13 px halo ≈ 540 px + r≈7 opaque core
≈ 175 px) ≈ 14,300 px ≈ **15.5%**; one strand (see §3) ≈ **11%**; centre pool
and junction accents ≈ **3%** → ~30% per conduit, ~90% for three, plus the
shared smear plane (decay ~6% + one cheap full-frame blend composite,
conduit-count-independent). Call it **~1.2 full-screen passes for three
conduits ≈ 6–7% of a frame's clock budget** in the reference renderer. Two
honest notes carried from the review: (a) the per-splat palette rebuild runs
twice per splat when the smear is on — a `GlowFrame` cache keyed on
(ramp, gain) is a cheap real win and Stage MN does it; (b) **no subject has
ever rendered several conduits' mana** — `mana_fill` runs for `ii == 0` only,
so `u02-trio` is a fake witness. Stage MN fixes the trio to run all three and
measures it; the crowd knob `kMoteCrowdPm` (motes per conduit scale-down when
several are on screen) is the named relief valve. The hardware answer stays
the recorded pair: FORGE.PRIM ribbons + `glow_persist` — this pass adds the
depth note (§2) to that record.

---

## 1. RULINGS — where sources conflict or the direction is open

### R1. The knead is field arithmetic, never collision.
The owner's "not necessarily by touching them" is a design law, not a licence.
No per-mote geometry queries, no proximity triggers. Position = barycentric
anchors; energy = grip/knead/drag scalars from joint state. If a route needs a
collision test, it is the wrong route.

### R2. The junction balls sit at the SURFACE CROSSINGS, and every ball's bone is a real hinge.
The pass-3 neck knuckle (bone-origin-sited, floating on the tube 660 mm above
the crown) is the "ball inside the antenna" — the tumour. **Remove it.** The
re-entry knuckle is the back-junction ball, "almost right" — **re-site it** at
the probed posed surface crossing, then adjust by eye (the probe finds the
crossing; the eye places the ball — measurement on the comparison side only).
**Add the front-junction ball** at the front surface crossing, on a NEW bone
`kBJunctionF` inserted into the chain (root → junctionF → neck → A → B → C →
D), so bending at the front junction bends the whole antenna. `kBLoopBase2`
(back junction) gains authored rotation so the re-entry direction gestures.
Twelve bones stand of 32; bones stay 8 B each. The antenna now has **two
body-side hinges plus neck plus A/B/C** — the "very mobile" mandate is met by
range, not by count: the knead layer's authored ranges at the junctions are
deliberately large (brackets in Stage B, picked by eye against the 07 bands).

### R3. Antenna gauge: thin front, thickening only AT the junctions.
The front tube stations (`kLoopBladeRxMm[1]`≈130/`Rz`150 at the neck) drop
toward the mid-tube gauge (~66–70), with the flare confined to the last station
before each junction ball. The back thickens "a small amount" at the re-entry.
Balls stay the thickest points per station (pass-3 R12 inequality kept and
re-checked). The pass-3 thin-tube/fat-ball read is on the protect list —
this is a siting-and-taper correction, not a rework.

### R4. Lightning: ONE strand, and the energy moves into motes.
"Fewer lightning lines — just have them be particles." `kStrandCount` 3 → 1
(the channel clip may carry 2; owner knob). The strand budget converts into
**surge motes**: 4–6 extra motes that flow along the strand's path and burst at
its ends, so the lightning reads as energised particles with one hot filament,
not a filament cloud. This also answers the reviewer's fault 4 (median frames
read as glitter): one strand can afford a denser stamp and a per-frame
brightness floor, judged at the MEDIAN frame, not the hottest (the review's own
sampling law).

### R5. The smear becomes depth-correct via a per-cell depth plane.
The bug: `smear_composite` blends over everything ("No depth test" by design,
and the owner has now rejected that design). The fix, still cheap and still
quarter-res: a second 96x60 plane of one depth value per cell. `smear_feed`
records the nearest (largest 1/w) contributing splat depth per cell; decay
leaves depth alone; the staggered hard clear zeroes it. `smear_composite`
takes the frame's depth buffer and skips any pixel whose surface is nearer
than the cell's remembered depth — exactly `glow_splat`'s own test, at cell
granularity. The 4-px blocky occlusion edge this produces is PART of the
broken-framebuffer aesthetic and is accepted, stated, and judged by eye.
Storage +11.5 KB-equivalent in the reel; cost one compare per lit pixel.
**Hardware record amendment** (`reports/U02-MANA-HARDWARE-ASKS.md`):
`glow_persist` needs a persisted per-cell depth (or must document feed-time
occlusion only, which cannot occlude a trail after the creature moves in front
of it) — spec text, costing nothing until POST.COMPOSITE is built.

### R6. Glitchier = a new preset RUNG plus a tear, not a new system.
The four-knob smear machinery (keep/step/jitter/hard-clear) is right and
stands. Direction 4 gets: (a) a **fourth live preset** past the current
long/glitchy rung — keep higher, steps longer and chunkier, jitter wider; and
(b) a **row-tear glitch**: on hashed frames, a horizontal band of the plane
composites with a 1–2 cell x-offset (`kSmearTearRows`, `kSmearTearFrames`,
`kSmearTearCells`) — the VHS tear that reads as a genuinely broken buffer.
Cheap (an index offset at composite), off by default in all but the glitchier
presets, owner-knobbed.

### R7. "More, bigger, smoother" for the motes, mechanically.
Count 7 → ~20 (the fold needs them anyway — §0.1). Halo r 9 → 12–15 px
bracket; opaque cores up with them. **Smoother rotation** is ruled a pacing
fault, not an interpolation fault: the current bullets stack two incommensurate
frequencies (the 2x term on `oy`) and 70–159-frame periods on small radii; the
fix bracket is single consistent angular velocity per mote, periods up ~1.5–2x,
radii up, the frequency-doubling term removed — rendered and LOOKED at before
shipping. If it still steps, the next suspect is px-quantisation of small
radii, checked per-frame (07 §7), and the answer is larger radii, not faster.

### R8. The mana stays in the middle — bounded by construction.
The reviewer's fault 3 (a lobe outside the loop). Stencil extents are bounded:
every stencil point's barycentric position is inside the anchor polygon by
construction (non-negative weights), and cloud/wander excursions clamp to
`kPocketBoundPm` of the polygon inradius — except the named wander motes,
whose escape is authored, small in count, and decays. The centring complaint
cannot recur for the shape mass; the smear may trail past (that is what a
smear is), and the depth fix (R5) keeps trails behind the creature honest.

### R9. Eyes: three experiments, polygon route weighted to win, teardrop expressiveness is a first-class criterion.
The review proved the page route's core fault is mechanical: the white ring is
painted at ONE gaze station on a static page while the star rides a bone —
the page **cannot** track. And the star cannot fit: arm 185 mm vs lens
half-width 125 mm — arithmetic, not tuning. And the owner has now ruled the
lens silhouette is a **teardrop — very pointy top, round bottom, asymmetric**
— which the current two-constant almond cannot express at all. The experiments
(Stage E, each with the same plate protocol):

* **X1 — all-polygon eye (expected winner).** Purple lens = faceted teardrop
  geometry from a NEW per-ring profile (per-ring width + centreline offset
  arrays + apex sharpening; ~10–12 rings × 8 segments ≈ 160–190 tris/eye,
  trivial against 1,444). White = a polygon annulus band parented to the
  PUPIL bone between star and lens — **whites trace pupils by construction.**
  Star = existing geometry, resized to fit (below). Purple page tile stays as
  pigment (gotcha §0: every part a page).
* **X2 — hybrid (cheapest delta).** Keep the page lens; DELETE the white from
  the page (pure purple field); add the same white annulus geometry on the
  pupil bone. Tracking solved for one part's cost; teardrop still requires the
  X1 profile work on the lens geometry, so X2 is X1 minus the lens rebuild.
* **X3 — the current page approach made to track.** Honest half-day timebox:
  read the creature texture path for any per-frame UV-offset capability. If
  none exists (expected — pages bake once), record the refusal with the source
  line and close the route; if one exists, prototype the scrolling-iris
  version knowing it forfeits the protected 3D star protrusion — which is
  probably disqualifying on its own.

**Containment becomes arithmetic** whichever wins: max gaze excursion =
lens half-extent − star reach − white margin, per axis, as named constants —
and the star is resized to FIT the short axis (per-axis arms:
`kPupilStarArmLongMm` / `kPupilStarArmShortMm`, the short one ≤ ~80% of the
lens half-width minus the ring). The sheet's ~20% lens share is a bracket, the
eye picks the value. Also in Stage E, independent of route: **separation** (no
touching at the top — the front sheet shows a clear pink channel between the
lenses), and **outward rotation** — the owner says they look INTO the creature;
re-run the yaw ladder and LOOK from the shipping camera, checking each eye's
lens normal points out, not mirrored inward.

**Presentation for choosing:** one comparison artefact per experiment — front/
three-quarter/side at native and 4x, a five-station gaze ladder strip, and the
curious clip re-rendered — assembled side by side with the `Front.png` eye
crop, dropped in the creature folder (`media/eye-experiments/`) and on the
pass-4 site page as an experiments row. Implementer defaults to X1 if it reads
(barge ahead); the owner overrules at leisure.

### R10. Interior glow: removed, not dimmed.
"Make it go away." The belly-core `glow_splat` calls (the depth-test-off shine
through the skin, `zhao_reel.cpp` ~2951/~3239-49) and their subject wiring are
removed for every Manafold subject. The machinery (`glow_splat`, ramps) stays —
the mana uses it. Render-and-look afterwards: no dark hole, no leftover core
on any clip. The revert path is the subject gain constant, kept at 0.

### R11. Outer layer more see-through: one rung up the committed ladder.
The mist IS the ambient rung (pass-3 R5, reviewer-protected lane). "A bit more
see-through, if possible" = re-run `U02_AMBIENT` at .32/.36/.40 on the
shipping subject, pick by eye — Direction 4 outranks Direction 3's "thicker"
where they trade off, but the rung is still chosen by looking, and .32 stays
the floor if every higher rung loses the rim entirely. The inspect subject's
murky orbit third (reviewer fault 9) gets the same one-ladder treatment on its
own rig (.20 → .24/.28 bracket), judged independently.

### R12. The outline question is answered by a calibrated instrument, with the answer expected to be "yes, by construction — with one exception to check".
The cel ink is a screen-space post pass, so its width should be
distance-invariant and identical for both creatures; the credible exception is
the eye assembly's crevices and the antenna's thin blade at distance (interior
ink doubling). Stage Q builds `inkwidth.py`: detect ink by the QUANTISED
triples the review established ((25,24,25)/(25,24,16)) or a tolerance band —
never the pre-quantisation value (`inkmask.py`'s own grave); **prove it can
fail** by feeding it a synthetically dilated outline first (gate item 6); then
measure perpendicular ink thickness (median/p10/p90) on Manafold and Zixxtrixx
at three matched camera distances and report numbers plus plates. If they
differ, say where and why.

### R13. The rename is total on every owner-facing surface; internal shorthand stays.
`git mv Upheaval/creature/Unnamed02 → creature/Manafold` (and
`UNNAMED02-INDEX.md → MANAFOLD-INDEX.md`), with every live reference updated:
`CREATURE.json`, `SCAFFOLD.json`, `SPEC.md`, `README.md`, `site-entry.json`,
`texture_recipe.py`, `validation/`, `00-START-HERE.md`, `website/creatures.json`,
`website/public/index.html`, `website/tools/tovideo.py`. zhaozhou: `git mv
tools/reel/unnamed02*.h → manafold*.h`, `u02_probe.cpp → manafold_probe.cpp`,
`u02_meshcheck.cpp → manafold_meshcheck.cpp`, `mku02page.py → mkmanafoldpage.py`;
subject names `unnamed02-*` → `manafold-*` (this pass re-encodes all media
anyway, so site filenames roll over cleanly; the pass-2/3 archive files keep
their historical names). **Kept, documented at the top of the art header:** the
`u02::` namespace, `kU02*` constants and `U02_*` env lanes — they are
creature-02 shorthand, not the placeholder name; churning them buys no
owner-visible value and risks the shared reel file. OWNER-DIRECTION files are
the owner's words and stay byte-identical. Historical mentions in gotchas/
checklists that narrate past passes stay accurate as history. Rename lands
FIRST (Stage R) so the whole pass's diff is under the true name.

### R14. Directional hits ride Zixxtrixx's precedent — and are the first deliberate deferral candidate.
Model: `zixxtrixx-damage` — **named authored contact stations**, fold envelopes
and the delayed wave shifted around the struck station; no runtime collision.
Manafold's stations: body-front, body-side, body-back, **loop-peak** (struck on
the antenna — must read differently). Mechanics stated mechanically: the root
displaces away from the blow and settles IN AIR (it floats — no stagger, no
ground brace; displacement + overshoot + a slow damped return, inside the 07
bands), the antenna whips opposite through the junction hinges with lag, eyes
wince (squint + gaze snap to the blow), and the mana coupling does the rest for
free — the impact's joint-velocity spike IS a knead spike, so the held shape
shatters to cloud and re-gathers (§0.2, no extra authoring). One subject,
`manafold-damage`, four directions in sequence like Zixxtrixx's. Hits are
fixed-position clips, so the one-column ground-snap trap does not apply.
**Budget honesty:** this is 1–2 sessions and Direction 4's core list is
already large; it is FIRST in the cut order, deferred loudly (not silently) if
the centrepiece runs long — the coordinator asked for exactly that.

### R15. The pass-3 regressions that touch this pass's publish are repaired; the instruments are fixed FIRST.
Direction 4 is narrow, but this pass republishes every clip, and shipping
drift-into-the-hillside twice is not narrowness, it is negligence. Ruled in,
bounded:
* **Drift** — the known one-column snap under a travelling root on `bump_ext=6`
  (the reviewer's root cause, same as the Zixxtrixx walk's "massive sink").
  Fix by the recorded Zixxtrixx precedent: staging constants (flatter stage
  `bump_ext` for travelling clips, per the walk's own note), not a terrain
  system. **Fall** — start inside the frame (staging constants: start height /
  camera). Both are constant-turns; one session cap together.
* **The probe gap the reviewer named first:** extend `manafold_probe` to
  re-query the terrain column along the clip's own travel for travelling
  clips. Committed, not improvised.
* **Instruments (Stage 0, before anything is judged):** `inkmask.py` gets the
  quantised-ink fix or a tolerance band and a self-test that FAILS on a
  known-bad input; `trajplot`'s creature mask stops selecting the horizon
  (mask against a creature-free background render of the same stage, or
  exclude the terrain band by column), with its own can-fail proof. Neither
  tool is cited by QA until its failure demo exists (gate item 6). The empty-
  set "6674/6674" style of proof is dead.

### R16. Protect list, inherited from the reviewer verbatim.
Stage L's structural rig defaulting in `u02_common` (no future subject may set
its own rig outside it); the one-sun bank + single four-light inspect; the .32
mist rung AND the ladder lane; the thin-tube/fat-ball antenna read; the
headstand and its probed contact; the travelling wobble mechanism and its
named constants; the opaque-core-under-halo; Zixxtrixx byte-identity; the
implementer's habit of declaring its own caveats. Nothing in this plan touches
these except as explicitly ruled above.

---

## 2. THE STAGING

Ordered; each stage independently checkable, committed and pushed with explicit
paths as it lands; every stage ends with render-and-look on the shipping
subject at native under the shipping sun. The centrepiece is first among the
creative stages, exactly as directed — but the instruments and the canvas
corrections come before it so that every folding judgement is made with honest
tools on an honest frame.

### Stage 0 — Baseline + honest instruments (half a session)
`build-direct.sh` (never `cmake --build`); reproduce the pass-3 shipped CRCs;
before-plates from shipping subjects. Fix `inkmask.py` and `trajplot`'s mask
per R15, each with a committed known-bad self-test that demonstrably fails.
Prune the reviewer's `C:\zrev` worktree if still present.
**Gate:** shipped CRCs reproduced; both instruments shown failing on bad input
and passing on good.

### Stage R — The rename (half a session)
R13, both repos: pure `git mv` commits first, reference-update commits second,
nothing else mixed in. Build and render one subject to prove the reel still
stands; Zixxtrixx spot-CRC (full identity proof re-runs at Stage Q).
**Gate:** grep finds no live `Unnamed02` reference outside owner-direction
files, archives and history narration; the build renders `manafold-hover`.

### Stage B — Bones and balls: the fold's skeleton (one session)
R2 + R3: remove the tumour ball; add `kBJunctionF` (bone + ball at the probed
front surface crossing, placed finally by eye); re-site the back ball at its
crossing; authored rotation on `kBLoopBase2`; front-tube taper redrawn
(thin, flaring only at the junctions); knead-layer range brackets authored at
the junctions and rendered as a motion ladder (small/medium/large gesture
amplitude — pick by eye; "very mobile" is the owner's adjective, so err
large and let the 07 bands bound it). Re-run closure/protrusion/thickness
probes (geometry moved); page tiles from birth for the new ball (gotcha §0).
**Gate:** plates beside the sheets front/side/back — no tumour read, a ball at
each junction, thickness inequality holds per station; every ball names its
bone in the rig table; probes green; the junction ladder plate exists.

### Stage D — The honest canvas: smear depth + glow removal + see-through (one session)
R5 (the depth plane — the depth bug is a named acceptance item), R10 (interior
glow removed), R11 (both ambient ladders). Do these before the folding so
every fold judgement sees correct occlusion and the true body.
**Gate:** a rendered orbit frame where the creature provably occludes its own
smear trail (before/after pair against the pass-3 draw-on-top); no interior
glow on any clip; ladder plates kept as owner evidence; hardware-asks
amendment committed.

### Stage FOLD — The centrepiece (two to three sessions; the pass's heart)
§0 in full: anchor-barycentric stencil system; grip/knead/drag scalars; the
`antenna_knead` choreography layer with per-clip gains; the shape library
(six, floor four); wander motes; anti-cycle hashing. Iterate author-render-
look-adjust; the naming test, causality strip and ablation gate are run every
iteration, not once at the end.
**Gate:** §0.4 — shapes nameable at native during HOLD; causality strip shows
the mana following the antenna at a distance with no contact; **the ablation
render shows the mana going limp when the antenna freezes**; the loop runs in
every clip without reading as a cycle across any clip's full length.

### Stage MN — The mana asks (one session)
R4 (one strand + surge motes, judged at the MEDIAN frame), R6 (the glitchier
preset + row tear), R7 (more/bigger/smoother, bracketed and looked at), R8
(centring bound — the outside-the-loop lobe cannot recur), the `GlowFrame`
cache, and the honest multi-conduit witness: `manafold-trio` renders all three
conduits' mana and its cost is measured and recorded (`kMoteCrowdPm` if it
needs the valve).
**Gate:** native plates — a typical (median) frame reads as energised
particles with one lightning filament; smear visibly glitchier than pass 3's
rung side by side; nothing but wander motes leaves the pocket; the trio plate
shows three folding conduits and its measured cost is in the QA.

### Stage E — The eyes (one to two sessions)
R9: the three experiments with the plate protocol, X3 timeboxed first (it is
the cheapest to kill), X1/X2 built and judged; winner landed with the teardrop
profile, per-axis star arms, arithmetic containment, separation, and the
outward-rotation fix verified by looking at all three views. Gaze scripts from
pass 3 carry over onto the winning system.
**Gate:** at native front/three-quarter/side — two separated teardrops, pointy
top round bottom, looking outward; the white travels with the star through the
whole gaze ladder; the star never leaves the lens at any authored extreme
(arithmetic + rendered extremes); the experiments artefact exists for the
owner.

### Stage T — Travel staging repairs (one session, capped)
R15: drift's stage/snap fix by the walk precedent; fall's start inside frame;
the travelling-column probe extension committed and run on drift, hasty, fall.
**Gate:** drift ends with the creature whole and on screen; fall's creature is
in frame from frame 0; the extended probe's numbers for all travelling clips
inside declared bounds.

### Stage H — Directional hits (one to two sessions; FIRST CUT)
R14: `manafold-damage`, four stations, float recoil, antenna whip, mana
shatter via the coupling. If cut: said loudly in QA and the site ships
without the tab.
**Gate:** contact sheet per direction — the struck side leads, the settle is
airborne, the shape shatters and re-gathers; 07 band metrics.

### Stage Q — Outline answer, QA, recon, publish (one session)
1. R12's `inkwidth.py` with its can-fail proof; the measured answer to the
   owner's question, in the QA and the report.
2. QA against Direction 4's **nine acceptance items** (plus the hit addition),
   shipping subjects only, worked through `10-GATE-CHECKLIST.md` top to
   bottom — item 6 applied to every instrument cited.
3. By-eye recon beside all three sheets at matched height.
4. Zixxtrixx identity: all 22, from a self-built baseline (the rename touched
   the shared reel file — prove it, byte-wise on at least the 5-risk set,
   CRC on all).
5. `ZIXX_SUNS=off` gate-off path: byte-identical proof this time (checklist
   17 — twice deferred already).
6. Kill and verify all background jobs; then **publish** via
   `Upheaval/website/deploy.ps1 -Project upheaval -Branch main` (`-Branch`
   mandatory; `noindex` stays). The site gains: Manafold naming everywhere,
   the folding on every clip, the eye-experiments row, the hits tab (if not
   cut), archives intact.

---

## 3. OWNER KNOBS (named constants, all new ones listed)

**Folding:** `kStencil*[6]` (the shape tables), `kStencilFacePm`,
`kMoteCount`, `kMoteHaloRPx`, `kMoteCoreRPx`, `kMoteCrowdPm`, `kGripGamma`,
`kCloudSpreadMm`, `kKneadJitterMm`, `kKneadFeedPm`, `kDragLagFrames`,
`kDragGainPm`, `kGatherFrames`/`kHoldFrames`/`kKneadFrames` (base + hash
spread), `kWanderCount`, `kWanderEscapeMm`, `kKneadClipPm[clip]`,
`kPocketBoundPm`.
**Antenna:** `kJunctionFrontBallMm`, `kJunctionBackBallMm`, junction offsets,
the redrawn `kLoopBladeRxMm/RzMm` stations, junction gesture ranges.
**Mana:** `kStrandCount` (1), surge-mote count/speed, mote orbit period/radius
brackets, `kSmearPresets[4]` (the new rung), `kSmearTearRows/Frames/Cells`.
**Eyes:** the per-ring teardrop profile arrays, `kEyeApexSharpPm`,
`kPupilStarArmLongMm/ShortMm`, white annulus radii, separation, outward yaw,
per-axis gaze clamps (arithmetic-derived, still named).
**Surface:** the ambient rung (both rigs), belly-glow gain (held 0).
Nothing generated-and-frozen; every one editable (CLAUDE.md rule 6).

---

## 4. WHAT THIS PASS WILL NOT DO, and the cut order

**Will not, at any budget:** collision/proximity mana (R1); reviving the page
white ring as the tracking answer if X3's timebox fails; touching Zixxtrixx
subjects, `kCel3Thresh`, or the marker orbs; more than one four-light subject;
a true non-clearing framebuffer or POST.ECHO; judging any visual off the
shipping rig or on an instrument without a failure demo; renaming the `u02::`
namespace or env lanes (R13); re-deriving eye or body geometry from sheet
measurements (the traced numbers stay demoted to brackets); publishing
mid-iteration.

**Cut order if the pass runs long (first cut first):**
1. Stage H, the directional hits — deferred LOUDLY to pass 5 (R14).
2. Fall's restage (drift's hillside fix is NOT cuttable — it ships fixed).
3. The X3 timebox (record the source-line refusal and move on).
4. The row-tear glitch (floor: the new glitchier preset rung alone).
5. Shapes 5–6 (triangle, S-curl) — floor is ring/star/bar/crescent.
6. The inspect-rig ambient re-ladder.
7. Surge motes (floor: one strand + ordinary motes).

**Never cut:** the rename; the folding loop WITH its ablation proof; the
junction balls and bones; the depth-correct smear; interior-glow removal;
more/bigger particles; the eye experiments shown (X1 and X2 at minimum) and a
landed winner with teardrop + tracking; the outline answer with measurements;
drift's hillside fix; the instrument fixes; the Zixxtrixx identity proof; the
publish.

---

## 5. Process notes for the implementer

* One implementation agent, this lane only — never
  `C:\programmieren\zencrifice\zhaozhou` or `...\Upheaval` (hardware agent
  live). Read `OWNER-DIRECTION-4-2026-09-05.md` **as amended on disk** plus
  anything newer in the creature folder and `reports/` before starting.
* Author by eye. Render. Look. Compare. Adjust. Ladders for every contested
  value (junction gesture amplitude, mote size, ambient rungs, eye yaw).
* Judge on the shipping lit subject at native; grep the rig selection first
  (§12); judge strand/mote quality at the MEDIAN frame; sample by badness.
* Commit and push per stage, explicit paths, never `git add -A`; log in
  TASK_LOG as it happens; kill background renders and verify before closing.
* The probes are committed (`manafold_probe.cpp` after Stage R); extend, never
  fork. The publish is one, at the end, `-Branch main`, `noindex`.
