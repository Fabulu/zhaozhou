# FINDINGS — RUN-20260827-2339 zixxtrixx V2: Waves C/D/E + the owner's session

## Delivered

### The owner's art direction (all seven asks + the closing batches)
1. **One culminating tube.** kTaper reworked by eye against Side.png (the
   ruled shape authority): radius grows monotonically from the trunk
   forward, peaks ~1.58x mid-body just behind a blunt dome, ball envelope
   retired. No skull, no junction — the head is where the tube got widest.
2. **The head looks up** (-6000, snout +4.6 deg). The sign convention had
   burned four passes; the new COMMITTED zixx_headaim.cpp measures the
   posed snout axis and caught my own sweep misread (-34000 "looked right"
   because the head was folded 149 deg under and its back read as a face).
   Measurement removed the bias; the render chose the value.
3. **Eyes** raised toward the back line, bulge 22 -> 42 (googly rim, skull
   still round), disc enlarged, ORANGE SURROUND painted under the drawn
   eye. Front camera beside Front.png: pink cap / blue face / side eyes /
   mouth — the sheet's layout, verified unlit too.
4. **Pink reads from the front**: the crown band starts just behind the
   dome (a band ON the dome painted a pink stripe down the face — caught
   on the r5/r6 renders) and holds full width over the skull.
5. **Blue runs further down the front body** (throat wedge holds full
   width, tapers out ~1.2 m behind the junction), the liked
   blue -> dark green -> light green sequence preserved; a subtle
   two-tone upper-flank band added per Side.png.
6. **Falling relaxed by a ton** (F1 with the owner's number on it):
   per-joint S authority 16..84% travelling on one slow loop-exact cycle.
   Overlap hits 10,768 -> 67, worst 307 -> 18 mm; the allowance was
   TIGHTENED 200 -> 40. Contact sheet: nearly-straight stretches,
   distorted-S collapses, the S recurs. Wobble, not jitter.
7. **Salto apex hold** (licensed): hang 2 -> 8 keys (~0.27 s), everything
   after shifted +6, stick still 5.000 s, plunge violence untouched.
8. **Idle dead zone**: the grounded run SNAKES SIDEWAYS — world-vertical
   by quaternion CONJUGATION after three approximate axes leaked 15-30 mm
   (belly band stays [-8..-2] EXACT at full amplitude). The four approved
   idle motions are bit-identical (proven per-bone against the golden).
9. **New vocabulary** (slots 5-8): HIT (wave-lane whiplash, both ends on
   the rest pose), DEATH (shudder -> taper-following corpse pose with a
   rear-node constraint -> flank keel with rolling-tube lift -> tail curl
   -> stillness), TAIL-BALANCE (hand-keyed fork-height curve enforced
   exactly; almost-spear, wobbling; topple with declared -191 mm bite),
   LOOK-AROUND (the head-aim rig on kBHead — the root trap honoured, body
   proven planted on the contact sheet while the head turns).

### The plan items
- **P2**: diagnostic subjects zixxtrixx-still-front, fall-side, unlit,
  unlit-front, normviz, wire, walk-unlit, lodsweep + the T7 debug
  sector/band atlas (mkcreaturepage --debug + -DZIXX_DEBUG_PAGE). Kept out
  of the library and --check.
- **T4**: ONE 128x256 RGB565 body atlas (U = circumference, V =
  nose-to-tail, junction at row 50), REPEAT U / CLAMP V, mips 0..7; fins
  on their own 64x64 pages BY RULE (mip bleed). Plumbing: per-tile mode
  words (lawful — mode is per-bind), RingPart v0/v1 V-ranges, req_lod
  generalised to the tile's own dims (floor-division identity keeps 64x64
  bit-identical). ~4x the longitudinal texel density on the body.
- **T5** (the #1 owner-visible gap, endorsed): multi-scale deterministic
  crayon — coverage x quilted strokes x paper tooth, fixed-seed patch
  quilting from several real scan crops, per-material hue drift between
  hand-picked anchors. THE GRAIN READS AT WALK DISTANCE — judged on the
  render, which was the whole point.
- **T6**: per-level RGB888 area filter, tooth halves per mip level, then
  565 quantisation with stable texture-space Bayer dither.
- **T3**: already amended 2026-08-27; a 2026-08-28 amendment adds the
  V-range + per-page-mode facts.
- **W1**: DIAGNOSIS ONLY, verdict NO WALK CHANGE. Evidence: the unlit
  walk contact sheet shows no crease surviving without the light rig, and
  the loop wrap step (10.3 deg worst-bone) is SMALLER than the median
  mid-gait step (17.8 deg) — the loop closes smoother than the gait moves.
- **C2**: phase clips slots 10-17 sliced at shared keys + two authored
  phases (spear flex, air hit) with integer zero-at-ends envelopes;
  ClipBank::seams ENFORCED by compile_creature (byte-identical or the
  creature fails to compile). The enforcement caught two real seams on
  its first run (curve truncation leaves auth=1/curl=1 at keys 18/47).
- **C4/C5**: AttackPlan (sim-lane record; velocity-led intercept; spear
  vector LOCKS at unroll — projectile, not missile) + the Zixxtrixx
  planner (spin follows the APEX after the proof caught flight-length
  scaling giving a flat lance more flips than the high dive) + the
  collision-only branch law. zixx-planner.exe (committed): golden preset
  reproduces the approved salto KEY-FOR-KEY; parametric plans terminate
  within 40 mm of their locked intercepts on collinear plunges; no input
  fires a phantom impact.
- **A1**: baked 60 Hz presentation companion (root Catmull-Rom clamped to
  the segment; quat 4-tap CR renormalised; event-adjacent segments keep
  nlerp). Hero opt-in per bank; payload bytes + sub=0 pose CRCs PROVEN
  bit-identical.
- **F2**: the offline deterministic spring-chain baker exists and bakes
  (60 Hz fixed-point, warm-up loops into the periodic orbit, closure
  fade) — see Not Done for its shipping status.
- **Wave E**: LOD distance sweep (cam_k_end camera ride) — crown survives,
  silhouette holds, no visible rung pop; micro rung is a COMPILER RESULT:
  9 meshlets, 558 tris, measured micro_error 2294 fx16 (~35 mm), now
  printed by the probe. Site: 8 live tabs + Archive (MAX_TABS 9; all
  three style.css selector families extended, the nth-child/nth-of-type
  split preserved).

## The vocabulary gap (named, as asked)
The donor's 64-slot vocabulary (creature_rules 2.1) includes getUp,
knocked2Floor, 5 directional damage anims, 3 deaths, corpseRise,
takeoff/fly/land. Zixxtrixx now fills 16 slots: idle(1) walk(2) attack(3)
fall(4) hit(5) death(6) tail-balance(7) look-around(8) + the 8 attack
phases (10-17). STILL MISSING for a battle-complete creature:
- getUp / knocked2Floor (knockdown state pair) and corpseRise
- directional damage variants (we have ONE hit; the donor has 5)
- additional death variants (donor: 3; we have 1)
- a RUN (fast gait) distinct from the walk
- turn-in-place, spawn/summon entrance, victory/taunt
- swim/burrow if the game wants them
None of these existed before this run either; the gap is now explicit.

## NOT done, and what is still wrong (honest)
- **C6/C7 SKIPPED as superseded**: the plan's launch retiming and recovery
  rebuild were premised on defects; the owner's 2026-08-28 word is "the
  salto is great" plus exactly one licensed edit (the apex hold, done).
  Under plan 5.2 those edits are gated on the owner's eye, and his eye
  ruled. If he revives them, the phase-clip architecture (C2) is where
  they land.
- **F2 is PREVIEW-GATED, not shipped** (-DZIXX_F2_PREVIEW): the bake has
  real dynamics character (delayed waves, overshoot, region masses) but
  still curls ~330 mm into itself at its tightest, and the F1 relax the
  owner directed TODAY reads calmer. Promoting a half-tuned bake over his
  directed look would be wrong; the side-by-side is his to judge.
- **The balance's mid-rise hop**: the gather-to-stand handoff lifts the
  whole body ~150-280 mm for a few keys (reads as a push-up; could read
  as a float to a hard eye). The keyed fork curve makes it a one-knob fix
  if it bothers him.
- **Head-in-hook nesting is deeper than before** (idle 239 mm at the
  breath extreme vs 69 pre-rework): the culminating head + look-up
  attitude inside the same approved hook. Judged on worst-key renders
  (eye and face stay clean; reads as the sheet's nesting) and declared,
  but it is a REAL trade the owner should see.
- **The front view still shows more pink than Front.png** (the neck arch
  behind the head joins the crown visually). The face band itself matches.
- **The eye's wavy pupil** survives but reads softer than the sheet's
  bold slit at 240p distance; another atlas-space pass could push it.
- **Look-around camera**: the site shot is the standard showcase pitch;
  the face reads when the head turns toward camera, but a dedicated
  closer shot would sell the gaze better.
- **kIdleBreathLift** (head rises with the in-breath) barely moved the
  breath extreme's dig (239 stayed) — kept because it reads alive, not
  because it fixed the number.
- Sequence CRCs for the four site subjects are NOT pinned in --check (they
  never were); the golden lane is the probe + goldens + choreo + planner.
- The CLUT8 fallback tiles still carry the OLD 64x64 painting (pre-atlas
  colour asks); the fallback only renders if page_direct is removed.

## Gates at close
- zixx-probe exit 0 (idle [-8..-2] EXACT, walk [-13..+10] EXACT, attack
  -426 @62, fall min 66 airborne; all overlaps within re-authored,
  render-judged allowances; micro stats printed).
- zixx-choreo exit 0; zixx-planner exit 0 (new gate).
- zhao-reel --check: "all sequence CRCs match" (redirected to file).
- Goldens: clips 1-4 payloads re-pinned ONCE for the licensed owner edits
  (diff scope proven per clip: walk = bone 25 only; idle = snake joints
  11-16 + bone 25, roots identical), then verified bit-stable through
  T4/T5/A1/C2/C4; new clips 5-17 join the golden set at close.
