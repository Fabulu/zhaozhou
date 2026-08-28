# Task Log: RUN-20260828-0326 - Zixxtrixx v4 closeout

**Created:** 2026-08-28 03:26 UTC+02:00
**Status:** In Progress (owner redirect: model first)
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260828-0326-zixxtrixx-v4-closeout/

---

## Objective

Finish the Zixxtrixx rework fully: PART 1 four standing faults, PART 2 falling decision, PART 3 sacengine vocabulary + missing animations. Owner: barge ahead, never stop to ask.

---

## Progress Timeline

### 2026-08-28 03:26 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260828-0326
- Created working directory
- Initial context: continuing RUN-20260827-2140 / -2339 / RUN-20260828-0227; the last run's Honest remainders + vocabulary gap list is the work queue.

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

### 2026-08-28 03:5x - PART 1 item 1 DONE: death pink-forward

- Rendered death, contact-sheeted every 2nd frame (death-before-sheet.png):
  after the keel (row 3 on) the corpse is a magenta smear — the +11600 roll
  turns the dorsal band square at the 3/4 site camera.
- kDeathRoll +11600 -> -11600 (keel AWAY): corpse reads green flank + blue
  underside, pink on the far silhouette edge (death-flip-sheet.png,
  death-before-after-pairs.png). Probe exit 0, no allowance moved.
- DISCOVERY on the same sheet: final rendered frame flashed the STANCE —
  anim_advance wraps one-shot clips to key 0 and interpolation wraps the
  last segment. Added Clip::hold_last (default false, looping clips
  bit-untouched); death sets it. Fixed in sim + midpoint bake + nlerp path.
  Evidence: death-lastframes-fixed.png (189/190/191 all hold the corpse).
- Golden drift scope PROVEN: clip-6.bin + pose-crcs clip-6 keys 31..95 only.
  Re-pinned with provenance (zhaozhou 1ac98b0, Upheaval d9e1084).

### 2026-08-28 04:2x - PART 1 items 2/3/4 DONE

- Item 2 (balance hop): probe showed minY +65 mm at k128 — the whole
  half-flat body hovered during the get-up. kBalFork recovery keys
  re-authored ({123,-145},{128,-82},{132,-48},{136,-26}); minY now
  [-19..+2] through the rise; render confirms tail run pressed to dirt
  (bal-getup-before-after.png). clip-7 re-pinned, provenance in golden.
- Item 4 (frontal eyes): head-on at bulge 16 each eye was a thin crescent
  at the silhouette rim. Ladder 16/22/28 (bulge-ladder-16-22-28.png):
  22 = two proper eye pads, crown gap intact, no ridge, profile unchanged
  (bulge22-side-still.png). ZIXX_EYEBULGE 16 -> 22. Between the two
  recorded failure modes (brim at 42, chinstrap) — well clear of both.
- Item 3 (pupil): at the walk camera the disc is ~10 px; 0.05 delivered a
  red crescent hugging the rim. PUPIL_BOLD ladder 0.05/0.08/0.11 on
  delivered walk pixels (pupil-ladder-walk.png): 0.08 reads as a red slit
  crossing the disc, middle swell hinted; 0.11 merges into the rim blob.
  PUPIL_BOLD -> 0.08. The full wavy shape does NOT survive gameplay
  distance — declared; it reads at look/idle poster distance.
- Page regen reproducible (two regens cmp-identical). Goldens cmp-clean
  after texture+mesh changes (clip payloads untouched, as expected).

### 2026-08-28 04:4x - PART 2 DECIDED: F1 ships, F2 stays gated

- Rendered both falls on the fixed side camera, contact-sheeted every 2nd
  frame. F2 as shipped: knotted ball most of the loop (fall-F2-sheet.png).
  One tuning pass (springs +~80%, damping up, aero/inertia down ~40%):
  opens into hooks/half-S (fall-F2-tune1-sheet.png) but still crumpled and
  high-energy — jitter-adjacent by the house table. F1: long legible
  serpent, slow travelling S, calm (fall-F1-sheet.png).
- DECISION (mine, per the standing barge-ahead order): F1 stays on slot 4;
  F2 keeps -DZIXX_F2_PREVIEW with the better tune. One-line reversal.
- reel --check after eye changes: all sequence CRCs match (check-after-eye.txt).

### 2026-08-28 05:3x - PART 3: sacengine + vocabulary + eleven new clips

- sacengine FOUND already checked out at C:\programmieren\sacengine (it fed
  the ANIMATION-NOTES study). Fetched submodules, LDC 1.41.0, dub build.
  Reconstructed the never-committed data_.d (loadDATA = readFile; only
  consumer is SacVolcano). First build ICE'd; -lowmem builds. 3d.exe RUNS
  against the installed Steam Sacrifice (data/maps junctioned; the GERMAN
  language wads shimmed to the engine's hardcoded lang_english names —
  inner trees are identical). Startup now dies in initKeycodes on a
  missing German key-name text; patched tolerant, rebuild in flight.
- ANIMATION-VOCABULARY.md committed (Upheaval 81b7ece): all 64 donor slots
  verbatim + engine fallback semantics + the Zixxtrixx scorecard.
- ELEVEN new clips (slots 20-31): knocked2Floor/getUp/hitFloor (shared
  knocked pose, compiler-ENFORCED seams — the check caught a real 1 mm
  seam break from curve()'s negative-slope rounding), damageRight/Back/
  Left/Top (donor law: missing direction = NO reaction), run (walk law at
  0.8 s cadence, wider hump half after asin clamp dug -68), death1 (agony
  rear-up -> prone collapse -> tail slaps), taunt (front-lift rear-up; the
  first cut un-deepened the lobe and folded the hook OVER the head —
  overlap 355 -> 211 after the mechanism swap), corpse (death's final key
  + sub-degree blade stir, seam-checked).
- The wave-lane law RE-LEARNED twice: raw neck quats in knock (+94 hover)
  and damageBack (tail -233) — both moved onto compensated wave lanes.
- Probe exit 0 (all allowances re-authored on worst-key renders,
  overlap-worst-keys.png); choreo 0; planner 0. Goldens: 16 old clips
  bit-identical, 11 new pinned, pose-crcs append-only (0 removed lines).

### 2026-08-28 06:0x - Site wave + final gates

- creatures.json: 14 live renders reordered into three groups ("how it
  lives / fights / dies") + Archive; six new entries voiced like the
  existing ones. tovideo.py: six new POSTER frames.
- style.css: ALL THREE positional families extended 9 -> 15 (label-lit,
  panel-reveal nth-child, focus-visible — the two checked families stay
  deliberately different). .tabgroup style added: group markers are
  non-interactive spans that NEITHER family counts.
- assemble.py: MAX_TABS 15 + check_css_wiring() — it now parses style.css
  and refuses the build unless both checked families reach MAX_TABS (the
  net the 2339 log described, now real).
- Final gates: probe 0, choreo 0, planner 0, goldens all 27 clips +
  pose-crcs cmp-clean. reel --check + assemble pending the encode queue.
- sacengine: 3d.exe (pre-patch build) boots, indexes all WADs, selects a
  map, loads deep, dies in initKeycodes on a German-localization gap;
  tolerance patch written, -lowmem rebuild in flight.

### 2026-08-28 06:4x - death1 slap loudness + close

- The 6000-amplitude slap on the last two thin segments moved ~130 pixels
  (frame diff) — a whisper. Widened to the last five segments at 13000;
  first attempt let the rear-node constraint SEE the slap and the root
  dove (front -146, probe). Constraint decoupled; two decaying slaps now
  read (death2-tailslap-strip-v4.png). clip-28 re-pinned, provenance.
- Site assembled: 14 live tabs + 3 groups + Archive verified in the HTML.
  All renders re-encoded and committed (Upheaval 9ac4239). No deploy.
- Final gates green (see FINDINGS). scratch-reel confirmed gitignored —
  the 830 MB trap from run 0227 not repeated.

### 2026-08-28 07:3x - sacengine CLOSED: it runs

- The tolerance rebuild landed after two flaky failures (dub cache lost
  zlib externals once; the scratchpad GC silently EMPTIED the LDC
  import tree — "unable to read module typecons" — LDC re-extracted to
  a stable home inside the sacengine folder).
- SDL2.dll 2.30.11 + OpenAL32 (openal-soft 1.24.2) dropped beside the
  exe. 3d.exe now: boots, indexes all WADs (German install via shim),
  initializes audio, loads OpenGL 4.0, opens its window and RUNS a map
  until the test timeout kills it. Proof runs in the log above.
- Status: Task complete. Zixxtrixx nominally complete; renders on disk,
  committed, NOT deployed (owner publishes).

### 2026-08-28 07:4x - OWNER REDIRECT (coordinator relay): THE MODEL FIRST

- Verbatim: "You completely fucked up the head even more... super rigid S
  ... the head is this weird hinge on it... drops down like crazy...
  droopy ball. Not a seamless tube at all. It needs to move way up."
  "we really need to fix the model in the first place."
- Reordered queue: 1) head attitude WAY up (render-judged wide sweep, not
  the number), 2) seamless hinge (longer falloff), 3) RELAX the S (the
  freeze is lifted — he names it as a cause), 4) fall must yield, not
  topple ("rigidly falls over like a log"), 5) look-around carries the
  living idle body, 6) balance slower/looser, 7) hit more impactful,
  8) NEW: salto variations (ground dummy air-hit, flying dummy, 6-salto).
- Status back to In Progress.

### 2026-08-28 07:5x - OWNER OVERRIDE: stop running sacengine

- Verbatim (relayed): "you're running sacengine on my computer now, I
  don't know what the point of it is. Please just statically extract what
  you can. don't want to do it like this."
- Confirmed no 3d.exe process running; no further builds or launches.
  Everything downloaded stays inert on disk.
- The vocabulary deliverable was ALREADY produced statically: the
  committed ANIMATION-VOCABULARY.md reads the Animations struct and the
  state.d fallback semantics from source text — no execution involved.
  The run-log claims about launching the engine stand as history, struck
  as practice going forward.
- Target dummy for salto variations identified by the coordinator: the
  watchdog quadruped in zhao_reel.cpp (creature-wave-walk subject).

## Owner review of the v3 build — the head is worse, and everything reads RIGID

*"You completely fucked up the head even more. Part of it is the super rigid S.
You kept that, and now the head is this weird hinge on it. It should have a
hinge, but it should seamlessly fit into the rest of the body. Instead it drops
down like crazy and now looks like this droopy ball. Not a seamless tube at all.
It needs to move way up."* And: *"we really need to fix the model in the first
place."*

**Model is priority one; everything else waits.** The head droops badly despite
attitude -6000 nominally being snout +4.6° — the number is not producing the
read it claims, so sweep WIDE and pick by contact sheet. The hinge stays but
must be seamless (the head-aim falloff is likely creasing the surface — lengthen
and soften it). **The canonical S is UNFROZEN**: he previously said not to change
it, and now names its rigidity as a cause.

**Rigidity is the through-line of the entire review**, and our own house style
already prescribes the cure — wobble is slow, loose, low-frequency, the bend
travelling through the body:
* **falling**: *"just rigidly falls over like a log"* — the worst of them.
* **look-around**: *"twitchy... should look more like the idle animation, with an
  actually live moving body. Too rigid."* The idle's four-period living body must
  run UNDERNEATH the head turn.
* **tail-balance**: *"too fast and robotic. More wobbly creatureness."*
* **hit**: *"just a weird twitch. Should be more exaggerated and impactful."*

**New work — salto variations**, using the AttackPlanner: air-hit a ground target
dummy, air-hit a FLYING target dummy, and a six-salto version. Target dummy
confirmed present: the **watchdog quadruped** in `zhao_reel.cpp` (6 bones, 6
rigid ring parts; `subject_creaturewalk()` ~L2966, LOD variant `creature-bulk-pop`
~L3660). Reel subjects only — never the shipped creature.

## sacengine: STOP RUNNING IT — static extraction only

*"you're running sacengine on my computer now, I don't know what the point of it
is. Please just statically extract what you can. don't want to do it like this."*

My brief said "build or run it as far as you reasonably can" — **that was my
error**, and it is overruled. Verified nothing of it is running; the clone at
`C:\programmieren\sacengine` left in place, untouched.

**The vocabulary was then extracted in ONE GREP, no execution** — which is the
owner's point exactly. `sacengine/source/animations.d`: `Animations` is a union
of `char[4][64]` with a named struct, so the 64 slots are its fields in order.
The source comments are load-bearing (`knocked2Floor`/`getUp` "only for walking
creatures"; `walk` "only different from run for Eldred"; wizard-only and
peasant-reuse families; `stance2` = "stance when damaged").

**It confirms every gap the owner named, and hands us a gift:**
* **THREE attack slots** — his salto variations are not extras, they are
  `attack1` and `attack2`, slots the format expects.
* **Five directional damage slots** (`damageFront/Right/Back/Left/Top`) — our one
  generic hit is doing five jobs, which is why it reads as a weird twitch.
* **Three death slots**; **`run` distinct from `walk`**; **`knocked2Floor`+`getUp`**
  the knockdown pair; **FOUR idle slots**, homes for tail-balance and look-around
  with one spare.
* Unfilled and relevant: `tumble`, `hitFloor`, `corpse`, `stance2`, `rise`,
  `thrash`, `notify`. Not applicable: wizard, carry/pull and flying families.

## Texture direction from the owner

1. *"Try to see if you can get more high res textures that look more textured
   onto the snake. It looks much better but still a bit single coloured."*
   Two levers, and the second is probably the bigger one. **Resolution:** the
   atlas is 128×256; `zref_texture.hpp` gives `log2w`/`log2h` FOUR BITS EACH
   ([11:8], [15:12]), so the format allows far more than we use — 128×256 was
   chosen, not imposed. The real constraint is the texture memory budget (64 KiB
   L0, ~85 KiB with mips today); check it and go as high as it truly allows,
   stating the number. **Grain amplitude:** "still a bit single coloured" is
   almost exactly the earlier failure where crayon grain was clipped to ±16% of
   luminance — narrower than the light rig's own range, so mathematically present
   and visually invisible, and it measured fine while reading as flat plastic.
   Resolution without amplitude will not fix this. Judge in-scene at gameplay
   distance; **the atlas preview looking mottled is not evidence.**
2. *"Maybe a bit too much dark blue in front and too little of the awesome dark
   green."* This partially reverses his earlier "carry the blue further down the
   front body" — it overshot. Pull blue back, give the room to dark green.
   `Side.png` has green dominant with blue as a front region, not half the animal.
3. *"Blue and dark green should kind of meld into each other, which should meld
   into the light green."* The structural note: the three are a CONTINUOUS
   PROGRESSION, not three bands with edges. Soften into gradients so the eye
   cannot find a boundary — crayon strokes overlap at transitions rather than
   butting against a line.

Fenced off: the pink band at 9, the pink crown, the eye disc size/separation
(brim and chinstrap failures both on record). Priority still model-and-head first.

## Sacengine, closed out properly

Two background shell tasks of the agent were still polling a sacengine BUILD log
and `3d.exe` was produced at 06:09 — **after** the owner's stop instruction.
Killed both; confirmed no sacengine process running; the clone left untouched.
**My fault twice:** the original brief said "build or run it as far as you
reasonably can", and when that was overruled I sent the stop without checking for
already-spawned background tasks. Lesson: **a stop instruction to an agent does
not stop the background work it has already launched — kill the tasks too.**
None of it was needed; the vocabulary came from one grep of `animations.d`.

### Taper refinement: slim the upper S, the NECK is broadest, then the head

*"The upper S part gets a bit too broad and big. The broadest biggest should be
neck and then head. Neck doesn't really exist right now because it's just a huge
dropoff from the rest of the tube."*

* **The upper S carries too much girth** — slimming it is the enabling change,
  because while it is nearly as broad as what follows, nothing downstream can
  read as a distinct region.
* **Girth order: body (slimmer) → NECK (broadest) → HEAD.** So the profile is NOT
  a single monotonic ramp to the nose; it builds through a broad neck and peaks
  into the head. A shaped curve, authored by eye against `Side.png`.
* **The neck is missing because there is a CLIFF where it should be.** It must
  become a readable stretch with its own length and girth, not a boundary event.

**Held against the unchanged law:** still ONE CONTINUOUS TUBE, no skull, no
junction. The neck is the broadest stretch of that same tube and must arrive and
depart smoothly. **We are removing a cliff, not relocating it** — both failure
modes are on record (the early "sheer drop like a cliff", then the
"dissolved into the tube" over-correction), so author between them by rendering.

Likely secondary benefit: a slimmer upper S should itself soften the "droopy
ball" read, because a thinner body behind a large head reads as neck-and-head
rather than as a lump with a ball stuck on it.

Judged on the side diagnostic camera beside `Side.png`, candidate ladder on one
contact sheet, frames committed to `evidence/`.

### Correction: KEEP the S — relax how it is HELD, not its shape

*"We really need to keep the S, but we also need to keep it less rigid. The
rigidity worked in our favor a lot because it's a difficult shape, but we need to
find ways to relax it without ruining it. It also needs to look organic and
bobb-y."*

**I had told the agent "the S is UNFROZEN — relax it", which reads as licence to
change the shape. Corrected.** The S is the signature and it stays. It is a hard
shape to hold and the rigidity is what has been holding it — hence "worked in our
favor". Soften the wrong thing and the S is lost.

**The distinction that makes this actionable: the S is a POSE; rigidity is how
that pose is HELD.** Not where the body goes — the quality of it going there.
From a wire bent into an S, to an animal choosing to sit in one.

Levers offered (all authored by eye): non-uniform curvature (a mechanical arc has
constant radius; an organic curve changes radius along its length — probably the
single biggest win); slight asymmetry between the lobes; the whole S slowly
DRIFTING so key 0's S is not key 40's; secondary motion travelling through the
body; and "bobb-y", which is a direct instruction — the head-bob he calls magic,
extended as a quality through the whole shape, remembering that the head is bone
0 = ROOT so it is a root-displacement problem.

**The idle's recipe is the model to copy**: four motions on four incommensurate
periods so the loop never resolves. That is the clip he called "absolutely sick".

**Frequency rule decides all of it: wobble is not jitter.** His "twitchy"
look-around and "robotic" tail-balance are the same axis as the twice-rejected
falling clip — too fast and too mechanical, not too little motion. Fewer and
slower, not more and faster.

**The whole review resolves to one sentence: the shapes are right; the way the
creature HOLDS them is mechanical.** Fix the holding, keep the shapes.

### 2026-08-28 08:3x - THE MODEL WAVE (owner redirect, priority one)

- Side.png measured by eye: the head is the arc's fat end at ~62% height,
  a SHORT fall out of the crown. Ours plunged a 66-deg stroke and parked
  the head at 33%. Stance re-authored: neck starts shallow (-27/-47/-60
  deg), crown rounded (-1500), dive softened; kBodyY 539 -> 648 solved by
  sine-sum so the grounded run keeps its exact height (probe: idle
  [-8..-3], walk [-11..+10] — the belly law holds).
- Attitude: WIDE sweep -16000..+14000 on one sheet from the fixed side
  camera (attitude-sweep-wide.png). -12000 continues the crown's line —
  seamless, eye up; -6000 already droops; -16000 creases. Picked -12000.
- Hinge: kSkullRigidTo 5 -> 4, kSkullBlendTo 9 -> 11, linear 64->0
  falloff replaces the four hard 13-steps.
- Taper: neck now the BROADEST stretch (1410 at t=220), upper S slimmed
  (1090/890 through the arc); grounded-run radii (t>=560) untouched to
  the millimetre — they are load-bearing for the ground slopes.
- Evidence: model-before-after-side.png, model-progression-side.png,
  model-front-v1-v3.png, model-walk-sheet.png. Overlap hits fell 45k->6k
  (the head left its own hook). All gates green; 22/27 clip payloads
  re-pin (the model moved under every clip — the redirect's point).

### 2026-08-28 09:0x - THE RIGIDITY WAVE (owner redirect items 4-7)

- FALL ("rigidly falls over like a log"): every bend term grew, the
  rotation wanders (roll 5600/yaw 4200), warp 8200 hesitates harder,
  authority swings 8..92% travelling faster down the body. Contact sheet
  (fall-yielding-sheet.png): adjacent thumbnails differ in SHAPE — a body
  crumpling and unfurling, not a log. Airborne min 268mm; overlap 36<40.
- LOOK-AROUND ("twitchy... should look like the idle"): the idle's living
  body EXTRACTED into idle_body(amp) — the idle proven BIT-IDENTICAL at
  amp 1000 — and the look rebuilt on it at 800 with the itinerary
  retimed onto 192 keys (1.5x slower).
- TAIL-BALANCE ("too fast and robotic"): 160 -> 224 keys, same
  choreography 1.4x slower, wobble 900 -> 1150 on a slower cycle.
- HIT ("just a weird twitch"): 18 -> 28 keys, deepen 900, jerk 7800, the
  body SHOVED -55mm along the blow, two slow decaying overshoots.
  Worst-key render layers cleanly; allowance re-authored 215 -> 255 with
  evidence. Directional set scaled to match (6400/6200/980/4200).

### Rig correction: the head IS joint rotation — the fault is the body under it

*"head should be joint rotation, it's just that the joint needs to fit onto a
body that doesn't suddenly have the head drop 5 meters"*

**I had been repeating a lesson past its expiry date.** "The head is bone 0 = the
ROOT, so head motion is root displacement, never a joint rotation" was TRUE of
the ORIGINAL rig and is why two early head-bob passes failed. It stopped being
true when the dedicated `head_attitude` bone (the V2 plan's bone 25) was added —
that bone is precisely what makes head rotation a proper joint rotation. I kept
citing the trap after the trap was gone. Owner's framing supersedes mine.

**The mechanism is right; the GEOMETRY under the joint is wrong.** Rotating the
head currently flings it down an enormous distance — that is the droopy-ball
read. Two likely causes, both already in scope:

1. **Pivot placement.** If the pivot is offset from where the head's mass is, any
   rotation swings the head through a large arc instead of turning it in place.
   The head should turn about ITSELF, not orbit a distant point.
2. **What the joint is mounted on** — the same fault as the missing neck. There
   is a cliff where the neck should be and the head bone hangs off the end of it.

**So the taper work and the head-aim work are ONE problem.** Slim the upper S,
build the broad neck, place the head joint so the head pivots about itself at the
end of that neck — the drop should resolve as a consequence. Then sweep the
resting attitude wide and pick off a contact sheet; it must go WAY up.

Verification is direct: rotate the head joint through its range with the body
visibly planted. A big arc or a drop means the pivot is wrong; a crease at the
weighting falloff means lengthen and soften it. Still one continuous tube.

**Lesson worth keeping beyond this creature: a hard-won rig lesson has a SCOPE,
and re-citing it after the rig changed made me argue against the correct fix.**

### Death and flailing: refine, do not restart

*"Death is unusually good for a first attempt, but needs to be more organic.
Flailing is getting better but try making it more alive."*

First praise either clip has had — both are on track, so this is refinement.

**DEATH → ORGANIC.** What reads as inorganic in a death is UNIFORMITY: a body
giving way at a constant rate, keeling as one rigid unit along a single smooth
curve. Levers: collapse in STAGES rather than one continuous keel (the front
gives, half-catches, then the rest follows — the pauses sell it); uneven rate
(slow fail, sudden give, slow settle, never constant velocity); asymmetry, since
a smooth balanced death reads as choreography; and **a last small settle after it
looks finished**, which does enormous work for "this was alive a moment ago". The
`corpse` slot exists, so the death can END somewhere instead of resolving tidily
inside its own loop.

**FLAILING → ALIVE, and the distinction is the whole thing: an object falling is
passive and gets tossed; a living creature falling FIGHTS.** Intention — brief
attempts at control between losses of it, trying to right itself, failing,
trying again. Vary the character across the loop so no two seconds read alike.
**And USE THE HEAD-AIM RIG:** head rotation is a proper joint rotation now, and a
falling animal's head is the most alive thing about it — it looks where it is
going, snaps toward the ground, reacts. That is the payoff for the rig and costs
nothing new to build; probably the strongest "alive" signal available here.

**Frequency rule still governs, and this clip has twice been rejected for the
opposite failure: wobble is not jitter.** "Alive" means loose, yielding and
intentional — NOT faster or busier. If in doubt, slow it down and add a REACTION
rather than another oscillation.

### 2026-08-28 09:4x - THE PIVOT + the Side.png gate

- Owner rig correction: "head should be joint rotation, it's just that
  the joint needs to fit onto a body that doesn't suddenly have the head
  drop 5 meters." kHeadPivotMm was 0 — the comment SAID centroid, the
  number said nose-tip, so every attitude change ORBITED the cranium
  about its own snout. Now 190 (station ~3.5 centroid); the pivoted sweep
  (attitude-sweep-pivoted.png) shows the head turning IN PLACE across
  -18000..+12000. Attitude re-judged: -12000 keeps (near-horizontal,
  clean crown, continues the curl).
- NEW STANDING GATE (owner: "always compare with side view on sketch"):
  every head/neck change now produces sidecmp-NN.png — the side camera
  beside Side.png at matched scale. sidecmp-01.png committed: the front
  half now reads as the sheet (fat horizontal head at height, eye
  high-forward, blue running back, pink over the whole crown).
