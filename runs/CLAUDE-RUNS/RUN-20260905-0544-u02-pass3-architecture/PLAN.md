# PLAN — Creature 02, pass 3 (architecture)

**Run:** RUN-20260905-0544-u02-pass3-architecture · **Date:** 2026-09-05
**Against:** `Upheaval/creature/Unnamed02/OWNER-DIRECTION-3-2026-09-05.md`
(binding, including the owner's smear refinement folded into §6d), judged with
`RUN-20260905-0207-u02-pass2-implementation/REVIEW.md`, the three
`RECON-U02-*.md`, the pass-2 PLAN, `09-ENGINE-GOTCHAS.md` (§12 especially),
`08-LIGHTING.md`, `07-MOTION-STYLE.md`, the three concept sheets, `CLAUDE.md`.

This is a plan, not an implementation. Every number below is a **starting point
or a comparison target**, never a shipped value; every shipped value is chosen
by rendering and looking, at native 384x240, on the SHIPPING subject. Every
value stays a named, editable constant.

**The one law this pass must not break again (gotcha §12):** pass 2's own
by-eye verdict was made on `u02-s4` plates that never select the shipping rig,
and it was wrong on both counts the owner then caught. In this pass **no visual
judgement counts unless the rendered subject provably selects the rig it ships
under** — grep the subject builder first, every time. Stage L makes the s4
diagnostics select the shipping presentation so this cannot recur.

---

## 0. RULINGS — where sources conflict, ruled here

### R1. Direction 3 supersedes Direction 2 on lighting, wholesale.
"The many-coloured lighting is our standard" is dead. **Exactly one subject
carries the four moving lights; every other subject is a directional sun over a
Zixxtrixx-style base rig.** No blend, no per-clip exceptions, no marker orbs
anywhere on this creature.

### R2. The eye trace loses to the two eyes that agree.
The pass-2 trace said lens half-length ~350 mm at 3.8:1 aspect. The reviewer
(at the shipping three-quarter) and the owner (from the published clips) both
say the almonds are over-long splinters. The trace measured the flat sheet
dead-on; the shipping read is a lens wrapped on a sphere at three-quarter —
this is the mismatched-poses law (`CLAUDE.md`). **Ruling: the shipping READ
governs. Shorten and fatten the almond by eye at the shipping camera** until it
reads as the sheet's plump teardrop; the traced numbers are demoted to
sanity brackets, not targets. Do not defend 350/3.8:1 with the trace again.

### R3. The face fault is SHAPE, not area — so the fix is aspect + page, not size.
Reviewer: white is 10–20% of lens area yet dominates as a 30 px bright arc;
purple overshot to 78–90%; the star never grew (5.5–9.8% vs the sheet's
20–22%). One repaint fixes all three: **white becomes a compact ring traced
around the star contour** (never running the lens length), **the star grows to
the sheet's share**, purple returns to the dominant-but-not-total field. The
geometry change is aspect only (R2). No structural eye work is reopened.

### R4. Side readability needs SOME outward yaw back — pass 2 over-cut it.
Pass 2 cut `kEyeYawOutA16` from 3600 to face the lenses forward (correct for
the front read), and the eyes now vanish off-axis (owner: "invisible from the
side; more 3D"). Zero yaw makes each lens edge-on in profile. **Ruling: restore
partial outward yaw and dome the lens** so each eye reads as a proud curved
lens from front AND side, like the Description sheet's own rear-oblique eye
drawing. Cheapest experiment, first thing in Stage F: a four-plate yaw ladder
(0 / 1200 / 2400 / 3600) rendered at front, three-quarter and side under the
shipping sun rig — pick by eye, then pull `kEyeXMm`/tips back until the
assembly sits inside the silhouette at three-quarter (the reviewer's fault 2).
The 160 mm protected protrusion depth is kept and re-verified with the
committed probe; the protrusion must stop reading as a bolt-on slab, which the
aspect shortening (R2) does most of on its own.

### R5. The mist is the ambient knob under the SUN rig now — and it has a real tension, bracketed.
The rim is the darkest toon band at grazing incidence; ambient sets both its
width (lower = wider) and its brightness (lower = darker). Pass 2's ambient
(.14/.15/.19) bought a wide band that is nearly black — which is exactly
"completely opaque". v1's Cool Cross ambient (.40/.45/.56) gave the thin
see-through mist the owner liked. The two asks — thicker AND translucent —
pull the one knob in opposite directions, so the answer is a **midpoint found
by ladder, not a computed value**: author `kU02SunRig` starting from
`diagonal-cool-cross` verbatim (key/fill opposition kept — R4 of the pass-2
plan stands) and render an **ambient ladder** (~.40 / .32 / .26 / .20) on the
shipping sun subject; pick the rung where the rim is clearly thicker than v1
and the pink still glows through it. This same rung also fixes the reviewer's
fault 6 (the creature spending half the orbit as dark murk) — one knob, two
owner complaints. **Fallback if no rung satisfies both eyes: the translucent
shell** (real geometry, page tile from birth per gotcha §0, a day of work) —
in the cut order, not the critical path, and shipped only if the ladder fails.
The four-light inspection subject gets the same ladder treatment on
`kU02MovingRig`'s ambient afterward, judged independently.

### R6. The smear is a DECAYING, GLITCHY persistence — not a non-clearing buffer.
Owner refinement (in Direction 3 as amended): *"never clears is too much, but
longer than usual in games. A bit glitchy."* **Ruling: build a persistence
emulation of the recon's glow-plane route in the reel** — a persistent 96x60
RGB accumulation plane fed by everything the mana draws, decayed per frame and
composited additively, exactly the shape of the `glow_persist` hardware ask —
instead of scaling up per-bullet stamped ghosts. Owner knobs, all named:
* `kSmearKeepPm` — per-frame retention (the decay length).
* `kSmearStepFrames` — decay applied in discrete steps every N frames, so the
  trail visibly stutters down instead of fading smoothly. This IS the glitch.
* `kSmearJitterPm` — small per-cell retention jitter (uneven decay).
* `kSmearHardClearFrames` — a bounded interval after which a cell fully
  clears, so the buffer provably resets (the "it does decay" guarantee).
A smooth exponential fade is the named failure — it reads as an ordinary
motion trail. At least two smear variants ship (short/clean vs long/glitchy)
so the length and glitch amount are picked by eye. The hardware record
(`reports/U02-MANA-HARDWARE-ASKS.md`) gets a small amendment: `glow_persist`
carries a decay constant **and an optional quantised-decay step** — filed as
spec text, still costing nothing until the block is built.

### R7. "Filled" means an opaque core, not a hotter additive gain.
Additive splats over the bright peach sky can never read solid — that is why
everything reads transparent. **Ruling: every mana body gets two layers: a
saturated OPAQUE core (non-additive, the drip's draw mode, depth-tested) under
the existing additive halo.** The core is what "filled" means; the halo keeps
the glow. Colours move to the owner's named family — aquamarine and cyan lead,
with filled blues and greens as variants (`kManaAquaMid/Hi`, `kManaSeaGreen*`
ramps; the existing `kManaCyan*` deepened toward teal per the eye recon's
pigment finding).

### R8. The mana anchors to the ring centre and STAYS there.
`FxAnchors.ring` (the hinge centroid) already exists and moves with hinge play.
**Ruling: every candidate's position law becomes ring-centre + a bounded
wobble** (a small integer Lissajous well inside the pocket), and the bullets
stop being ballistic escapees: they orbit/jiggle around the centre with
`kBulletSpreadMm` bounding their excursion, dying before they reach the blade.
"A little spray is fine" = the spread knob, small. Nothing drifts to the edge.

### R9. Lightning is strands that BUZZ — continuous, plural, alive.
The pass-2 plan already specified ~2 px continuous stamping and it never
landed (reviewer: beads with gaps; owner: sparks). **Ruling: it lands this
time, and it is upgraded to the owner's image: 2–3 simultaneous strands**
across the ring pocket's middle, each a continuous two-layer path (hot ~1–2 px
near-white core over a calm wider additive halo), re-hashed on the existing
`kBoltRehashFrames` cadence so they visibly buzz, ghosting through the smear
plane so strikes decay instead of vanishing. Stamp spacing is the checkable
gate: **zero visible gaps at native on the channel clip's hottest frames** —
the reviewer's own plate (`08-lightning-reads-as-beads.png`) is the
before-reference to beat.

### R10. The clip verdict is taken whole; lasso gets ONE fix attempt, then the axe.
Every clip named in Direction 3 §7 is reworked this pass (staging below). The
lasso's fault is plausibly the un-USED body-side hinges (the bones exist since
pass 2; nothing visibly bends there and no knuckle marks the joint). It gets
one rebuild on the junction hinges after Stage G lands; if it still does not
read by eye, **cut it** — the owner explicitly allowed "fixed or cut", and a
broken clip on the page is worse than an absent one (the drip lesson).

### R11. The body-side hinges exist as BONES but not as THINGS — give them geometry and motion.
`kBNeck` and `kBLoopBase2` were added in pass 2, yet the owner repeats "there
are no hinges where the antennae meet the creature". What is missing is the
READ: no visible knuckle ball at either junction, and no clip visibly
articulates there. **Ruling: two new hinge-ball parts at the neck exit and the
re-entry** (same construction as balls A/B/C, each with a page tile from birth
— gotcha §0), sized into the new thickness order (R12), plus junction
articulation authored into the clips (Stage G). No new bones are needed; 11 of
32 stand.

### R12. The thickness order is BALLS > TUBE, enforced as a single inequality.
Direction 3 §3: the tube is thinner than the joints it runs between. **Ruling:
scale the blade arrays (`kLoopBladeRxMm/RzMm`) down ~0.7x as a starting point
and keep/raise the hinge-ball radii so that every ball — A, B, C and the two
new junction balls — is visibly the thickest point on the antenna at native.**
The pass-2 taper stays (broad base flaring into the body, thin tip); only the
gauge drops. The tube-vs-ball inequality is checkable per station and goes in
the QA list.

### R13. Six mana variants ship, all of them alive; drip is CUT.
The reviewer proved drip dead in 579 of 600 frames; shipping it broken
overstated the menu. The pass-3 menu, every entry re-judged at native on the
lit shipping path before it counts:
1. **Aquamarine smeared plasma** — the §6d candidate rebuilt on R6/R7/R8:
   filled cores, centre-anchored, mid-length glitchy smear. The lead.
2. **Cyan smeared plasma, long/glitchier smear** — same treatment, deeper
   teal-cyan, `kSmearKeepPm` and `kSmearStepFrames` at the far end. The two
   smear variants double as the owner's decay/glitch picker.
3. **Filled deep blue** — big opaque-cored blobs, minimal smear.
4. **Filled sea-green** — the "try greens" ask, same treatment as 3.
5. **Boil-centre, enlarged** — Direction 6e verbatim: the two-tone boil's
   middle grown ~1.6x, the outer ring removed, centre-anchored.
6. **The stack** — caged pulsar core + buzzing strands + aquamarine smear:
   the likely shipping combination, shown assembled so the owner judges the
   whole, not only parts.
Lightning strands additionally run on the channel clip regardless of menu
choice (they are the creature's identity, not a menu item). Colour is nearly
free (one 192-byte ramp each — recon §3.3); the count is bounded by encode
and judging time, not by cost.

### R14. What is DEFERRED from Direction 3's long list, said now.
* **The second trick clip.** The headstand (owner-named) ships; a second trick
  (a loop-wheel roll) is authored only if the pass runs ahead — first entry in
  the cut order.
* **The translucent mist shell** — only if the R5 ladder fails by eye.
* **Pirouette and rest rework** beyond the global wobble/expressiveness floor
  — not named in Direction 3.
* **The ring-hole egg shape and the front slot** (reviewer residuals, middling
  severity, not in Direction 3) — one knob turn each if free, else logged.
* Everything in "will not do" (§9).

---

## THE STAGING

Ordered; each stage independently checkable; cheapest and highest-value first.
Lighting goes FIRST because every later visual judgement must be made under
the shipping rig (gotcha §12) — judging the face before fixing the rig means
judging everything twice. Commit and push per stage, explicit paths. Every
stage ends with a render-and-look on the SHIPPING subject at native.

### Stage 0 — Baseline (half a session)
`tools/reel/build-direct.sh` (never `cmake --build`); reproduce the shipped
clip CRCs; render before-plates from the SHIPPING subjects
(`unnamed02-taunt`, `unnamed02-hover`, the channel, a mana subject) — not s4.
**Gate:** shipped CRCs reproduced; before-plates on the shipping rig exist.

### Stage L — Lighting: the regression, killed first (one session)
1. **Restore the per-clip sun.** `subject_u02_clip` stops setting
   `creature_moving_light`; each clip gets `s.sun = &kU02Sun*` (four such
   constants survive from pass 1 at `zhao_reel.cpp:2185-2188`; author the
   missing ones for the newer clips — sun far away per Direction 29: the
   whole body inside attenuation 1, height authored against the fall clip's
   apex, not the hover).
2. **One subject keeps the four lights: a new `unnamed02-inspect`** — hover
   keys, orbit camera, `creature_moving_light = true`, `kU02MovingRig`, the
   u02 source paths from pass 2 unchanged. This is "the inspection showcase"
   and it is the ONLY carrier.
3. **The marker orbs go for u02.** A `SceneSubject` flag (or species check)
   skips `draw_zixx_moving_source_markers` on u02 subjects, including
   inspect. Zixxtrixx's own subjects keep their orbs — untouched bank.
4. **`kU02SunRig`** — the u02-owned base rig for sun clips, starting as
   `diagonal-cool-cross` verbatim; plumb it the way `moving_rig` already is
   (`cr_ctx` override around the compose, subject-scoped, saved/restored).
5. **The s4 diagnostics select the shipping presentation** — sun +
   `kU02SunRig` — so every future by-eye plate is honest (gotcha §12).
**Gate:** every u02 subject renders; grep-proof that only `unnamed02-inspect`
sets the moving light; no orb pixel in any u02 frame; Zixxtrixx bank
CRC-identical (the rig plumbing touches shared code — prove it, all 22).

### Stage M — The mist ladder (half a session; rides Stage L's rig)
Render the ambient ladder (~.40/.32/.26/.20 classes) on `unnamed02-hover`
under `kU02SunRig`; look for: rim clearly thicker than v1, pink glowing
through it, no dark-murk orbit phases. Pick by eye; then run the same
exercise on `kU02MovingRig`'s ambient for the inspect subject.
**Gate:** by eye at native — a translucent, clearly visible mist all round the
grazing edge, and the body reads as one pink mass through the whole loop.
Keep the ladder plates as the owner-facing evidence of the trade.

### Stage F — The face (one session)
1. **Aspect by eye at the shipping camera** (R2): `kEyeLongMm` down /
   `kEyeWideMm` up until each lens reads as a plump teardrop at three-quarter
   native; apex toward the sheet's high meeting point (the pass-2 residual).
2. **The yaw ladder** (R4): pick the outward yaw that makes the eye read
   front AND side; dome the lens (curvature, not tip stand-off); pull
   `kEyeXMm`/tips back inside the silhouette at three-quarter; re-verify the
   protected ~160 mm protrusion with the committed `u02_probe`.
3. **The page repaint** (R3): white ONLY as a ring traced around the star
   contour (dilate the star mask a few px — never a length-wise arc); star
   grown toward the sheet's ~20% lens share; purple back to the dominant
   field (~65-70%); star teal deepened toward the drawn (44,179,205) class.
4. **Star containment** (Direction 3 §2): clamp gaze travel so the star plus
   its white ring never crosses the lens ink at any authored gaze extreme —
   an authored clamp on `kGazeMaxA16`/pivot radius, proven by rendering the
   gaze extremes, and kept as a QA check.
**Gate:** at native, on `unnamed02-taunt` (three-quarter) and a side view
under the shipping sun rig: two plump purple almonds with white-ringed teal
stars, both inside the silhouette, legible in profile. Before/after beside
the reviewer's `02-face-at-the-shipped-three-quarter.png`.

### Stage A — Antennae and body form (one session)
1. **Thickness order** (R12): blade arrays ~0.7x, balls thickest, per-station
   inequality checked.
2. **Junction knuckles** (R11): two new ball parts at neck exit and re-entry,
   page tiles from birth, sized into the ball family.
3. **Teardrop** harder (`kBodyTaperPm`, `kBodyLeanXMm`), judged against the
   side sheet at matched height.
4. Re-run the committed closure and protrusion probes (geometry moved);
   tighten the closure gate toward real rim vertices as the reviewer asked.
**Gate:** side/front plates beside the sheets: a teardrop wearing a thin ring
whose joints are the fattest points, junction knuckles visible; probes green.

### Stage G — Motion (two sessions; the expressiveness mandate)
All inside `07-MOTION-STYLE.md` bands; every note stated mechanically.
1. **The whole-creature wobble** (Direction 3 §4), mechanically: a slow bend
   that starts at the loop peak, travels down through C-B-A into the neck,
   and arrives in the body as a lean-plus-squash a few frames later — front
   leads, bottom follows; two incommensurate periods (46/102-frame class);
   amplitude well above the pass-2 floor; **plus root PITCH** (up/down
   angling) alongside the existing yaw, in hover, drift, idle and curious.
2. **Idle** slower: bob periods up ~1.5x, amplitude kept.
3. **Hasty directional**: straight-line travel crossing a fixed-camera shot
   (the Zixxtrixx walk staging precedent — start half the travel back, cross
   through centre), pitched into the travel, fishtail kept, no circuit.
4. **Drift rebuilt**, mechanically: a wind-blown lateral glide — the body
   banks into a sideways slide, translates across the shot in a lazy S,
   antenna trailing against the travel, two visible corrections where it
   over-banks and recovers. No yaw-circle.
5. **Fall longer**: keys up ~1.5-2x (100 now), higher start, an extra tumble
   axis, the catch kept.
6. **Taunt funnier**, mechanically: big anticipation wind-up, then the loop
   waggle held at its extreme for a readable beat (≥16 frames) with a wink,
   then a smug settle-bob. Comedy is the HOLD.
7. **The headstand trick** (`unnamed02-trick`): it pitches over, plants the
   loop peak on the ground (declared, authored ground contact — the probe
   declares the contact window and depth), balances upside down with the
   body wobbling above, antenna flexing at the junction hinges, then rights
   itself with overshoot. Owner-suggested, uncuttable among the tricks.
8. **Lasso**: one rebuild on the junction hinges (visible bend at neck and
   re-entry, knuckles selling the pivot); cut if it still fails by eye (R10).
9. **Startle**: sharper snap, wider eye-flare, one recoil overshoot.
10. **Eyes and antennae in EVERY clip**: blink floor stays; add per-clip gaze
    scripts (look-arounds, direction leads, double-takes in curious), squint
    on effort, widen on fall/startle; junction-hinge gestures (a shrug, a
    perk-up) as the antenna's acting channel.
**Gate:** trajectory plots per clip (no flat lines); contact sheets of every
frame; band metrics inside 07's table; the headstand's ground contact probed
in 3D against its declaration; before/after pairs per reworked clip.

### Stage X — The mana (one to two sessions; the biggest single item)
Prerequisites already in place from pass 2 (glow floor black, ink ordering).
1. **The smear plane** (R6): the 96x60 persistence buffer, four named knobs,
   fed by all mana drawing; two authored variants (short/clean, long/glitchy).
2. **Filled cores** (R7): opaque non-additive core under every additive halo;
   aquamarine/cyan lead ramps, blue and sea-green variants.
3. **Centre anchoring** (R8): ring-centre + bounded wobble for every
   candidate; bullets re-lawed to jiggle, not escape.
4. **Strand lightning** (R9): 2-3 continuous two-layer strands buzzing across
   the pocket, ghosting through the smear; gap-free at native, verified on
   the hottest frames.
5. **The boil centre** (R13 #5): middle grown, outside removed.
6. **The menu**: six subjects (R13) rendered on the reworked hover loop under
   the shipping sun rig, encoded, and shipped as the picker; drip removed
   from the page.
7. **Hardware record amendment**: `glow_persist` gains the decay constant +
   quantised-step note in `reports/U02-MANA-HARDWARE-ASKS.md`.
**Gate:** at native on the lit shipping path: mana reads SOLID (an opaque
core is visible against the sky), sits in the middle through the whole loop,
trails noticeably longer than a normal game trail and visibly stutters as it
decays, and the lightning shows no gaps. Reviewer's drip/bead plates are the
before-references.

### Stage Q — QA, recon, publish (half a session)
1. QA against Direction 3's eight acceptance items, judged on shipping
   subjects only.
2. The standing by-eye recon: final build beside all three sheets, matched
   height, on the shipping rig (Stage L.5 makes this honest by default).
3. Zixxtrixx identity proof (all 22, CRC) — Stage L touched shared plumbing.
4. Kill all background renders; verify nothing is running.
5. **Publish** via `Upheaval/website/deploy.ps1 -Project upheaval -Branch
   main` (`-Branch` mandatory, page stays `noindex`) — the finished pass, and
   nothing before it. The site picks up: inspect tab, mana menu (six), the
   new/reworked clips, the pass-2 archive kept.

---

## 8. COST, with the arithmetic (unit: one full-screen pass = 92,160 pixel-visits)

Deciding case: **three conduits on screen**, the likely stack (filled core +
halo + bullets + strands + smear). Every figure is arithmetic, not
measurement — no fragment counters exist; label it so wherever published.

| element | per conduit | x3 |
|---|---:|---:|
| 46 px additive halo | 6.3% | 18.9% |
| ~15 px opaque core (same px, REPLACE mode) | 0.7% | 2.1% |
| 10 bullets @ 8 px, centre-bounded | 2.2% | 6.6% |
| 2-3 strands, continuous two-layer, 24 seg class | 0.8% | 2.4% |
| junction/hinge accent splats | 0.9% | 2.7% |
| **subtotal, no smear** | **10.9%** | **32.7%** |
| smear: persistence plane (96x60, whole frame) | — | **+25%** |
| **total** | | **~58%** |

~58% of one pass ≈ **3.2% of a frame's clock budget** (one pass ≈ 5.5% of
clocks at the placeholder 100 MHz). Compare pass 2's stamp-trail route at
63.3%: the persistence plane is cheaper at three conduits AND is
conduit-count-independent — a fourth conduit adds ~11%, not ~21%. Sun-vs-four-
lights is also a per-frame saving on every non-inspect clip: one source's MACs
instead of four. Mesh remains a non-issue (two new junction balls ≈ +100 tris
on 1,444; fill governs). Bones stay 11 x 8 B = 88 B/frame. The real fill risk
remains DDR and the per-pixel path ("no slack anywhere"), which is exactly why
the smear rides a quarter-res plane.

---

## 9. WHAT THIS PASS WILL NOT DO, and the cut order

**Will not, at any budget:**
* Re-derive eye or body geometry from sheet measurements (R2 demotes the
  trace; the art law governs).
* Touch `kCel3Thresh`/`kCel3Level`, re-pin Zixxtrixx CRCs, or alter any
  Zixxtrixx subject (markers included — the orb removal is u02-only).
* Keep four moving lights on any subject but `unnamed02-inspect`, or exceed
  the four-source budget anywhere.
* Build a genuinely non-clearing framebuffer, revive POST.ECHO, or run §15
  trails on conduits.
* Flatten the eye protrusion (protected) or let the almond tips back outside
  the silhouette.
* Thicken the tube or enlarge the loop (still refuted; R1 of pass 2 stands).
* Ship drip, or count a dead candidate in the menu.
* Judge any visual on a subject whose rig selection has not been read in
  source this pass (gotcha §12).
* Write a new frame reader (`rgbframe.py` only) or build via `cmake --build`.
* Publish anything mid-iteration; the one publish is the finished pass.

**Cut order if the pass runs long (first cut first):**
1. The second trick clip (headstand stays — owner-named).
2. Mana variants 4 then 3 (sea-green, deep blue) — floor is four variants:
   two smears, the boil centre, the stack.
3. The lasso rebuild (straight to cut; the owner allowed it).
4. The mist translucent shell (only reachable if the R5 ladder failed; if cut,
   ship the best ladder rung and log the shell for owner judgement).
5. The drift lazy-S (keep the lateral glide, drop the S curvature).
6. Ring-hole/front-slot residual knob turns.

**Never cut:** the lighting restoration + marker removal (acceptance 1), the
mist ladder (5), the face work (2), the thickness order + junction knuckles
(3), strand lightning (6), filled + centred mana with the glitchy smear and
at least two smear variants (6), the headstand (7), directional hasty (7),
the by-eye recon on shipping subjects, the Zixxtrixx identity proof, the
publish.

---

## Process notes for the implementer

* One implementation agent at a time; this lane only — never
  `C:\programmieren\zencrifice\zhaozhou` or `...\Upheaval` (hardware agent
  live there).
* Read `OWNER-DIRECTION-3-2026-09-05.md` as amended (the smear refinement is
  folded into §6d) before starting; check the creature folder and `reports/`
  for anything newer.
* Every stage: commit and push explicit paths as the work happens; log in the
  run TASK_LOG; kill background renders before closing.
* Author by eye. Render. Look. Compare. Adjust. Ladders (ambient, yaw, smear)
  are how this pass makes its judgement calls cheap — render the bracket,
  pick by eye, name the constant.
* Judge every colour and every part on the shipping lit subject at native
  384x240. Grep the subject's rig selection first, every time (§12).
* The probes are committed (`u02_probe.cpp`); extend them (closure toward rim
  vertices, the headstand contact window), never fork them.
