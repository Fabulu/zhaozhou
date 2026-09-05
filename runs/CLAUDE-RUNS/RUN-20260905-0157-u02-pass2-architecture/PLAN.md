# PLAN — Creature 02, pass 2 (architecture)

**Run:** RUN-20260905-0157-u02-pass2-architecture · **Date:** 2026-09-05
**Against:** `Upheaval/creature/Unnamed02/OWNER-DIRECTION-2-2026-09-04.md` (binding)
**Inputs:** RECON-U02-EYE-FINDINGS.md (weighted highest), RECON-U02-FORM-FINDINGS.md,
RECON-U02-FX2-FINDINGS.md, their plates, all three concept sheets,
`09-ENGINE-GOTCHAS.md`, `08-LIGHTING.md`, `07-MOTION-STYLE.md`, `CLAUDE.md`.

This is a plan, not an implementation. No value below is a shipped value.
Traced numbers are **starting points and comparison targets**; every shipped
value is chosen by rendering and looking, per the art law. Every number stays a
named, editable constant.

---

## 0. RULINGS — where the recons conflict or leave a choice

### R1. The loop is SHRUNK, never lengthened or thickened. (Settled — restated because it reverses the previous pass's belief.)
The eye recon's like-for-like measurement refutes "a quarter too short": the
loop is ~1.7x too LARGE for the body. `PLATE-side.png` shows it without a
number. The tube gauge relative to the body is roughly right; it reads as wire
only because the loop is oversized. **One edit direction: `kLoopArcMm[]` down
(~0.6x as the starting point), preserving the body-bulb ≈ 46%-of-total-height
front proportion that is already correct.** Any implementer tempted to thicken
the tube is repeating the documented CLAUDE.md failure.

### R2. Eye width: the two recons only *appear* to conflict — keep the width, cut the yaw.
The eye recon says the model's lens is "under half the width" of the sheet's;
the form recon's trace says `kEyeWideMm` 92 is **already right** (0.205 R). Both
are true: the trace measured the sheet, the eye recon read the *render*, where
`kEyeYawOutA16` 3600 turns each lens ~20° sideways and foreshortens it. Ruling:
**width constant stays near 92; the apparent narrowness is fixed by cutting the
outward yaw so the lenses face forward** — which the eye recon itself lists as a
cheap knob. Do not double-fix by also widening.

### R3. The dongle, the clipping, the lollipop and the missing bones are ONE structural job.
Both recons converge: hinge ball C's rigidly-welded return arm is the floating
dongle *and* the clipping; the plug-junction neck is the lollipop *and* the
worst missing bone (the neck fold currently leans the whole body, eyes
included). One stage (B below) does all of it: three new bones, loop closed by
construction, fold distributed for roundness, blade tapered. Fixing them
separately would touch the same rings twice.

### R4. The gas rim is thickened through the LIGHT, not built as geometry — with a declared fallback.
The rim is the darkest toon band at grazing incidence; the ramp thresholds are
CRC-locked to Zixxtrixx and untouchable. The measured lever: rim width went
4 px → 21 px → 49 px as ambient fell across three rigs, and the many-coloured
rig the owner mandates as standard (§6) carries ambient at roughly **half** Cool
Cross's. **Ruling: adopt the many-coloured rig and tune a u02-owned ambient
until the rim reads "very visible" by eye — §1c and §6 are one change.** The
see-through quality is free: the band is the same pigment at a lower level.
What is achievable: a rim plausibly in the 30–50 px-of-scanline class (the
hard-sun end of the sweep) at **zero geometry and zero fill cost**. The known
risk, named by the form recon: low ambient with a one-sided key gives a
terminator, not a rim — the key/fill *opposition* of Cool Cross must be kept in
the u02 rig. The first implementer job is to author that rig variant and look.
**Fallback if the owner's eye says "still not enough": a translucent shell** —
real new geometry, must carry a texture page (gotcha §0) and a draw-order
decision; estimated at a day of work plus judging. It is in the cut order, not
the critical path.

### R5. The smear: stamp-trail now, glow-persistence on the hardware record, §15 never on a conduit.
The §15 implementation is ~224 full-screen passes per light — four orders of
magnitude over the alternative and unaffordable at three conduits. Ruling:
**reel today = route 2 (8 stamped ghost splats per emitter, ~10% of a pass,
deliberately steppy "dropped frame buffer" read); machine = route 1
(`glow_persist` on the existing 96x60 emissive plane, 0.25 of a pass for the
whole frame, +11.5 KB)** — filed as spec text now (§G). POST.ECHO revival is
declined: cut-order 1 is the wrong foundation for a signature look, and the
glow plane buys most of it at 1/16 the pixels. If full-scene RGB feedback is
ever wanted, it goes on the record by name, not smuggled in.

### R6. Lighting: the many-coloured moving rig replaces the per-clip sun on this creature.
The two are mutually exclusive today and the four-source budget is a hard law —
sun + four coloured sources = five. Raising the budget is a hardware question
this pass does not ask. Ruling: **u02's showcase clips drop `s.sun` and set
`creature_moving_light`, with u02-owned path/radius constants** (the Zixxtrixx
paths orbit a 4 m stage centre; the conduit's ball is 0.9 m — reuse the
placement code, re-author the extents as `u02::` constants). Direction §6 says
this rig *is* the standard now; keeping a sun lane on u02 would preserve
exactly "the old single-rig look" the owner retired.

### R7. Mana menu size: SIX candidates, five survive any cut.
"We really need a lot of examples here to pick the good ones" is an explicit
ask and an acceptance item. The menu is §F; every candidate is a named variant
on machinery the fx recon proved renders today. Six gives the owner a real
choice; the floor after cuts is five because four of them are named in the
direction itself (pulsar-in-ring, big plasma, smear, lightning).

### R8. Where I depart from the eye recon's cheap/medium/rebuild sort (otherwise adopted wholesale):
* **Motion amplitude** is listed as cheap knob turns. I split it: the amplitude
  *floor* (bob, squash, hover, so motion clears the noise floor) is Stage A;
  the startle re-timing and the new clips are real animation work in Stage E.
  A knob cannot add overshoot and settle to a 130-frame crawl.
* **The eye page repaint** is listed as the largest single character win — true,
  but I schedule it **after** the lighting change (D after C), because gotcha
  §0/§7 requires judging every colour on the shipped lit path, and the shipped
  lit path is changing. Repainting first means judging twice.
* **The sheet's front-antenna kink/lean** goes in the **rest pose via the new
  bones**, not the mesh — the eye recon's own §5 says the drawn kink is one
  pose. The mesh gets the *taper* (per-ring blade arrays); the pose gets the
  attitude.

---

## THE STAGING

Ordered; each stage independently checkable; cheapest-and-highest-value first.
Commit and push per stage. Every stage ends with a render-and-look against the
sheets (the ALWAYS LOOK law), not only its listed gate.

### Stage 0 — Baseline (half a session)
Rebuild via `tools/reel/build-direct.sh` (never `cmake --build` for the reel),
reproduce the shipped clips' CRCs, re-render the recon's diagnostic set as the
before-plates. No judging any delta off an uncalibrated build (08-LIGHTING law).
**Gate:** shipped CRCs reproduced.

### Stage A — The knob pass (one session; the highest value-per-edit in the creature)
Existing named constants, one edit each, rendered and looked at in clusters:
1. **Loop scale** `kLoopArcMm[4]` toward ~0.6x; **loop lean** `kLoopFoldRootA16`
   toward upright. Preserve the 46% front proportion.
2. **Teardrop** — `kBodyTaperPm` much harder, taper starting several rings
   lower; `kBodyLeanXMm` up. (Owner knobs that already exist; the teardrop is a
   value change.)
3. **Eyes, geometry only** — `kEyeLongMm` toward the traced ~350; **flip the V
   sign** and splay toward ~28° per lens (`kEyeVAngleA16`); raise and spread
   placement (`kEyeYMm`, `kEyeZMm`); upper tips meeting near 0.68 R; cut
   `kEyeYawOutA16` (per R2); star toward `kPupilStarArmMm` ~120–150,
   `kPupilStarWideMm` ~50–55. Then **re-measure protrusion in 3D** — the
   lengthened, splayed almond swings its tips further out along the radial, so
   the crown will exceed the current 166 mm; pull `kEyeDeepMm`/`kEyeXMm` back
   until the *read* matches today's (the protected value is the current read,
   not a bigger one). The probe the form recon wrote gets **committed** this
   time (CLAUDE.md: commit the probe).
4. **Motion floor** — `kBobAmpAMm` toward ~90, `kCompressAmpPm` toward ~9000,
   `kHoverHeightMm` up until there is visible air under the creature at native
   res (Direction 1: IT FLOATS; today the ball sits on the dirt line).
5. **Camera framing** — the whole loop in shot in every showcase clip; the
   creature no longer small and low in empty sky.

**Gate:** new side/front plates beside the sheets at matched height — the body
reads as the dominant mass, the loop as a worn ring, the face as a Λ. Judged by
eye. Trajectory plot of the hover clip is no longer a flat line.

### Stage B — Structure: the rig and the closed loop (one to two sessions; the one job of R3)
1. **Three new bones** (8 → 11 of 32; budget is a non-issue):
   `kBNeck` (child of root at the loop exit — moves the neck fold OFF `kBRoot`
   so folding the antenna stops leaning the body and the eyes),
   `kBHingeD` (child of C at the return arm's start — the arm becomes a limb),
   `kBLoopBase2` (child of root at the drawn re-entry — the loop's far end
   becomes a child of the BODY). Pivot law holds: each bind translation at its
   own ball centre / fold station.
2. **Close the loop by construction** — bind the last rings to `kBLoopBase2`,
   blend the middle rings with the two-bone station blend already used
   everywhere else; `kLoopArcMm[3]`/`kLoopBuryMm` become the arm's own length
   knobs. After this, **no fold scale can open the loop** — the dongle and the
   punch-through become unrepresentable, which is the correct kind of fix.
3. **Roundness** — distribute the fold across rings (per-ring fold curve or
   more stations) so the four-corner paperclip becomes the sheet's continuous
   curve with a tall upright egg of a hole.
4. **Blade taper and the shoulder** — `kLoopBladeRxMm`/`RzMm` become per-ring
   arrays: broad at the base (sheet: ~45% of body width) flaring INTO the body
   with no plug junction (the lollipop fix), tapering toward the tip (~10%).
   The front hole in the blade comes with it. The drawn kink/lean lands in the
   rest pose on the new bones (R8).
5. **The buried-arm ink hairline** — the crack drawn down the body by the buried
   tube contour; ink-pass-level fix, done here because the burial geometry is
   being reworked anyway.
6. `unnamed02` already opts into `cap_base_fix`; keep it. Untextured-black law:
   any new ring part carries a page tile from birth.

**Gate:** a **committed loop-closure probe** — sweep fold scale 780→1160 (the
full range the clips use) and assert the return arm's every ring stays within
its declared engagement of the body; render the sweep and look. The startle
frames 40–52 that showed the dongle re-rendered clean.

### Stage C — Presentation: light, pink, rim (one session; judged as ONE cluster)
1. **The u02 many-coloured rig** — adopt `kCreatureLightMovingInspection` as the
   base; author `u02::` path constants (orbit radii, inner/outer, stage centre)
   scaled to the 0.9 m ball so the coloured pools actually land on the animal;
   set `creature_moving_light` on the showcase clips and drop their `s.sun`
   (R6). Additive emission rides through the existing normal gate.
2. **The gas rim** — look at it under the new rig first (its halved ambient
   thickens the rim on its own); then tune the u02 ambient down until the rim
   is "very visible", keeping key/fill opposition so it stays a rim (R4).
3. **The pink** — only now touch pigment: `BODY_PINK`'s ~20-count blue bias out
   (crimson, not magenta), and close the lit/shadow band spread so the creature
   stops reading as two objects. The recon's finding stands: the old rig did
   more damage than the pigment — which is why the rig moves first and the
   pigment is judged under it, at native, on dark ground.
4. **Kill the plastic** — the pale specular blob (~40% of the ball; the sheet
   has no highlight at all) goes; the lumpy terminator gets whatever the band
   values buy for free (a deeper terminator fix is in the cut order, not here).

**Gate:** by eye at native 384x240 on the shipped lit path: one-mass pink close
to the drawings, a thick see-through rim all round the silhouette, no bauble
highlight, coloured pools visibly crossing the body. `sunmeter.py` on the
solo'd sources to prove each contributes (08-LIGHTING: solo before tuning).

### Stage D — The face (one session; the largest single character win)
1. **Repaint the eye page** (`mku02page.py`): purple as the dominant field
   (~65–70% of lens area), the white reduced from a 20% slab to a **thin ring**
   (4–7%) tracing the star, the star grown to the traced ~0.34x lens length /
   1.3x lens width — wider than the purple band, so the ring balloons around it
   as drawn — with fat, organic, slightly wonky arms, not a symmetric cross.
   Star cyan deepened toward the drawn teal. The mid-grey (96,96,96) that is
   currently the eye's most common colour must not survive the repaint.
2. **Lens ink contour** — the almond gets its own ink so it separates from the
   pink at any size (the sheet outlines every lens in heavy black).
3. **Protrusion re-verified** with the committed probe after the page/geometry
   settle — target is the Stage-A-preserved read.
4. **Eye life** — the gaze/squint knobs exist and are unused; wire blink,
   squint, widen and gaze shifts into every clip's authored keys (executed with
   Stage E's clip work, listed here because it is the face's other half).

**Gate:** redo `PLATE-eyes.png` and `PLATE-front.png`; judge ONLY on the
shipped lit path (gotcha §0 — an unlit render cannot exhibit the black-part
bug, and this exact face already shipped black once). The profile view must
show a legible eye with its star, per the sheet's own side view.

### Stage E — Motion and the clip inventory (two sessions; the expressiveness mandate)
All motion inside `07-MOTION-STYLE.md` bands: silhouette half-life 32–54
frames, ≥16 frames per beat, zero per-station reversals, monotone tangents,
speed spent on payoff never wind-up. Say every motion note mechanically (§8).
1. **Rework the existing six** — hover/drift get real bobbing (the Stage A
   amplitude floor, now shaped); breathing via squash that is *unmistakable* —
   the direction asks for more stretch/inhale/exhale than Zixxtrixx; startle
   gets anticipation-snap-overshoot-settle instead of a 12-frame hop and a
   130-frame droop; curious gets re-aimed so the face points AT the object of
   curiosity (today it turns away).
2. **New clips**: `unnamed02-hasty` (accelerated flight, visibly clumsy — lag
   and overshoot in the antenna, body pitched into the travel, bob frequency
   up, a slight fishtail), `unnamed02-fall` (blown high, tumbling, antenna
   streaming, eyes wide), `unnamed02-hit` (impact squash + hinge recoil),
   at least two taunts (the hinge-play showcase: the creature grabs/waggles its
   own loop, bobs mockingly — the balls are hinges and it PLAYS with them).
3. **The eyes move in every clip** — blink floor (never-off life law), gaze
   leads direction changes, squint on effort, widen on startle/fall.
4. **What out-expresses Zixxtrixx, concretely**: Zixxtrixx has no face — this
   creature's entire acting channel (giant googly Λ-eyes) is one Zixxtrixx
   lacks; squash/stretch beyond Zixxtrixx's authored band; the hinge play; and
   the mana answering the rig (Stage F couples splat gain/position to hinge and
   breath channels), so the effect is part of the performance. Those four
   channels, all inside the same pace bands, are the answer to acceptance 7.
5. **Ground/air**: a committed hover-clearance probe (posed vertices vs terrain
   over every clip) — declared contact: NONE; visible air always. For a floater,
   accidental ground contact is the clipping bug.

**Gate:** trajectory plots per clip (no flat lines anywhere; the CLAUDE.md law),
contact sheets of every frame, band metrics inside 07's table, before/after
pairs against the pass-1 clips.

### Stage F — The mana menu (one to two sessions; owner picks, we do not)
Prerequisites, both one-constant laws from the fx recon: **the glow ramp's
floor goes to black** (`kGlowLo` → {0,0,0}; a floor above zero rims every blob)
and every additive colour sits under the channel ceiling.
The ten-emitter set is **axed** (the direction's own words). The bolt subject's
recurrence survives; the ten kinds do not.
**Six candidates**, each a named variant renderable on the reworked hover loop
(the existing solo/fx-tour mechanism is the delivery vehicle):
1. **The caged pulsar** — 12–15 px core inside the ring pocket
   (`glow_splat`, `depth_test=false` for the core), 46 px halo spilling through
   the arms so they silhouette against their own light; halo BREATHES 60→90 px
   on a ~4 Hz duty (the fix for the pulsar that never pulsed) and couples to
   the body's inhale. Anchored on its own ring-centre bone so hinge play moves
   the light.
2. **Big plasma blobs** — 2–3 large splats on the Lorentzian
   `corona_sprite_bloom(24)` profile, blue plus two alternate colour ramps
   (colour is nearly free; one 192-byte ramp each).
3. **Smeared plasma bullets** — 8–12 mini splats (r 6–10 px) launched from the
   ring, each trailing 3–4 stamped ghosts (smear route 2). The "broken frame
   buffer" candidate.
4. **LIGHTNING MANA** (also acceptance 6 — this candidate cannot be cut):
   keep `bolt_beads`' FX.LIGHTNING recurrence verbatim; fix the representation —
   stamp along segments at ~2 px so the path is continuous, two layers (hot
   narrow core over a calm wide additive halo), ghosts via the smear so a
   strike decays instead of vanishing, the baked anamorphic streak sprite on
   the strike frame, and optionally the fourth point light pulsing cyan on
   strike (bounded under the ceiling law so it tints rather than floodlights).
   The Description sheet says this IS the creature — it stores mana and
   discharges paralysing bolts through the antenna, crackling.
5. **Two-tone boil** — two counter-rotating ramps (blue core / violet outer)
   plus CLUT rotation on the ramp: churn for zero pixel cost.
6. **The drip** — 3–4 LARGE opaque droplet splats that fall and fade; the one
   non-additive read, kept from the old set's only good idea.
Combinations are knobs, not new candidates (e.g. 1+3+4 is the likely shipping
stack). Each candidate gets a short clip; all are encoded and put on the
creature's page as a picker so the owner chooses with his eyes.

**Gate:** each candidate rendered at native on the lit path and looked at; the
cost table below re-checked as arithmetic (and labelled arithmetic — there are
still no counters).

### Stage G — The hardware record, QA and publish (half a session)
1. File the hardware asks (§6 below) as a report in `zhaozhou/reports/`.
2. QA against Direction 2's nine acceptance items; the standing by-eye recon
   (render beside sheets) once more on the final build; reviewer pass per the
   owner's process.
3. **Publish** the finished pass — bestiary standing authorisation, via
   `Upheaval/website/deploy.ps1 -Project upheaval -Branch main` (`-Branch`
   mandatory), page stays `noindex`. The mana picker ships with it (site work
   rides a creature pass).

---

## 6. THE HARDWARE RECORD — exactly what is filed

Per Direction §0 (spec is the machine), stated honestly:

1. **`FORGE.PRIM` ribbon family — the WHOLE block**, correcting
   `ADDLIGHTNING.md`'s overstatement on the record: not "chiefly the position
   evaluator" but topology generator + parameter block + position evaluator +
   `zref::ForgePrim` reference oracle + directed/random tests. Today: maturity
   SPECIFIED, zero RTL, zero oracle, empty maturity log. Cost when built: a
   24-segment 3 px bolt is 48 triangles, ~400 px, 0.4% of a pass — lightning is
   a block problem, never a fill problem.
2. **`FX.LIGHTNING` as an effects-level contract** (not a seventh Forge
   family): the `ADDLIGHTNING.md` field list (anchors, tick, seed, segments,
   jitter, width, colour, intensity, branches, lifetime, semantic weight,
   viewport mask), bounded at 24 segments + 2 branches of 8; LOD via the frozen
   `PART.LADDER` ruling (ribbon → soft sprite → glint). The reel's `bolt_beads`
   authoring migrates onto the evaluator unchanged — that migration note is
   already in the header and stays.
3. **`POST.COMPOSITE` `glow_persist` mode**, sibling to `radial_decay`: one
   frozen Q0.16 decay constant, one additional 96x60 RGB565 plane (+11.5 KB
   M10K), saturating accumulate over the existing emissive glow plane.
   ~0.25 of a full-screen pass for the entire frame, any conduit count. Lands
   as spec text now; costs nothing until the block is built.
4. **Explicitly NOT asked:** POST.ECHO revival / full framebuffer feedback
   (cut-order 1, one-directional today — wrong foundation, per R5) and a
   raise of the four-light budget (resolved art-side by R6).

---

## 8. COST, with the arithmetic (unit: one full-screen pass = 92,160 pixel-visits)

The deciding case is **three conduits on screen**, likely shipping stack
(caged pulsar + bullets + lightning + stamp smear):

| element | per conduit | x3 |
|---|---:|---:|
| 46 px outer halo | 6.3% | 18.9% |
| 12–15 px ring core | 0.7% | 2.1% |
| 3 hinge splats @ 10 px | 0.9% | 2.7% |
| bolt, continuous two-layer, 24 seg | 0.4% | 1.2% |
| 12 plasma bullets @ 8 px | 2.6% | 7.8% |
| stamp-trail smear (8 ghosts) | 10.2% | 30.6% |
| **total** | **21.1%** | **63.3%** |

63.3% of one pass ≈ **3.5% of a frame's clock budget** (one pass ≈ 5.5% of
clocks at the placeholder 100 MHz). When `glow_persist` is built, the smear
line collapses to **+0.25% for the whole frame** and the total drops to ~33%.
Mesh cost is a non-issue (1,444 tris; fill governs, not geometry). Bones
11 x 8 B = 88 B/frame. All figures are arithmetic, not measurement — no
fragment/particle counters exist; every published number carries that label.
The real fill risk remains DDR and the per-pixel path ("no slack anywhere"),
which is another reason route-1 smear is the machine's answer.

---

## 9. WHAT THIS PASS WILL NOT DO, and the cut order

**Will not, at any budget:**
* Thicken the tube or lengthen the loop (refuted; R1).
* Touch `kCel3Thresh`/`kCel3Level`, re-pin Zixxtrixx CRCs, or opt the fleet
  into `cap_base_fix`.
* Plumb the celestial compositor into the creature (sky-only by construction).
* Run §15 trails on conduits (~224 passes/light) or revive POST.ECHO.
* Exceed four point lights, or keep a clip sun alongside the moving rig on u02.
* Flatten the eye protrusion (protected, artist-approved) or derive any 3D
  value mechanically from the sheets — traces are comparison targets only.
* Write a new frame reader (`rgbframe.py` only) or build the reel via
  `cmake --build`.
* Publish anything mid-iteration; the one publish is the finished pass.

**Cut order if the pass runs long (first cut first):**
1. Mana candidates 6 then 5 (drip, two-tone boil) — menu floor is five, and
   the four direction-named looks all survive.
2. The second taunt.
3. Pirouette/rest rework beyond the Stage-A amplitude floor.
4. The gas-rim geometry fallback (ship the rig-thickened rim; log the shell as
   an open item for owner judgement).
5. Crayon-grain texture redo (the specular-blob kill in Stage C stays).
6. The bolt-lit-environment pulse inside candidate 4 (the bolt itself is
   uncuttable — acceptance 6).

**Never cut:** loop closure + bones (acceptance 1), the eye rebuild
(acceptance 4), lightning existence (acceptance 6), the by-eye recon on the
final build, the publish.

---

## Process notes for the implementer

* One implementation agent at a time; this lane only — never
  `C:\programmieren\zencrifice\zhaozhou` or `...\Upheaval` (hardware agent live
  there).
* Every stage: commit and push explicit paths as the work happens; log in the
  run's TASK_LOG.
* Author by eye, render, look, compare, adjust. The traced numbers say where to
  start and how far off you are — never what to ship.
* Judge every colour and every new part on the shipped lit path at native
  384x240. An unlit render cannot exhibit the black-part bug that already cost
  this creature its face once.
* Kill background renders before closing a lane; verify nothing is running.
