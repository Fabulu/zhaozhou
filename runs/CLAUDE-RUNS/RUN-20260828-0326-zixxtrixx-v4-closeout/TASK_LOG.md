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
