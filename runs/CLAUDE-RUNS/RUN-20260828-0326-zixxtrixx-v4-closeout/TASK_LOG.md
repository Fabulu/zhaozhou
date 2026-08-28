# Task Log: RUN-20260828-0326 - Zixxtrixx v4 closeout

**Created:** 2026-08-28 03:26 UTC+02:00
**Status:** Complete
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
