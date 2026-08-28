# FINDINGS — RUN-20260828-0326 zixxtrixx v4: the close-out

## PART 1 — the four standing faults (all fixed, by eye)

1. **Death no longer reads pink-forward.** kDeathRoll +11600 -> -11600: the
   keel now turns AWAY from the site lens, so the corpse reads green flank +
   blue underside with the pink band on the far silhouette edge
   (death-before-after-pairs.png). Probe exit 0, no allowance moved.
   **DISCOVERY on the same contact sheet:** the final rendered frame flashed
   the living stance — anim_advance wraps every clip to key 0 and the
   presentation interpolation wraps the last segment. A one-shot clip now
   carries Clip::hold_last (sim + midpoint bake + nlerp path all clamp);
   default false, every looping clip bit-untouched. The corpse holds
   (death-lastframes-fixed.png).
2. **The balance push-up hop is gone.** It was the fork-height knob handing
   +30 mm back by k130 while the pose was still half-flat — probe showed the
   whole body's lowest vertex at +65 mm (a hover). kBalFork recovery keys
   re-authored ({123,-145} {128,-82} {132,-48} {136,-26} {140,0}); minY now
   stays in [-19..+2] through the rise and the render shows the tail run
   pressed to the dirt while the front gathers (bal-getup-before-after.png).
3. **The pupil is a slit at walk distance, within what 240p carries.**
   PUPIL_BOLD 0.05 -> 0.08, picked off a 0.05/0.08/0.11 ladder judged on the
   DELIVERED walk pixels (pupil-ladder-walk.png): 0.05 was a red crescent
   hugging the rim, 0.08 reads as a red slit crossing the disc with the
   middle swell hinted, 0.11 merges slit and rim into one blob. Declared
   plainly: the full wavy shape does NOT survive gameplay distance — it
   reads at look/idle poster distance and at zoom. A bolder simpler slit
   that survives was the brief, and 0.08 is it.
4. **Frontal eye presence.** ZIXX_EYEBULGE 16 -> 22, picked off a head-on
   16/22/28 ladder (bulge-ladder-16-22-28.png): at 16 each eye was a thin
   crescent hugging the silhouette rim; at 22 two proper eye pads with the
   crown gap intact; at 28 the swell starts wedging the skull outline.
   Profile checked, read unchanged (bulge22-side-still.png). Both recorded
   failure modes (brim at 42, chinstrap) stay well clear.

## PART 2 — falling DECIDED: F1 ships, F2 stays gated

Both falls rendered on the same fixed side camera and contact-sheeted
(fall-F1-sheet.png / fall-F2-sheet.png). F2 as shipped knots the animal
into a ball for most of the loop. One tuning pass (springs +~80%, damping
up, aero/inertia down ~40%) opens it into hooks and half-S shapes
(fall-F2-tune1-sheet.png) but the read stays crumpled and BUSY — adjacent
thumbnails differ wildly, the high-energy character the house style calls
jitter-adjacent. F1 is a long legible serpent with slow travelling
S-language in every frame — the owner's "relax by a ton" look. **F1 stays
on slot 4; F2 keeps -DZIXX_F2_PREVIEW with the improved tune. Reversal is
one line.** (My call, per the standing barge-ahead order; stated plainly.)

## PART 3 — sacengine, the vocabulary, eleven new clips

### sacengine on this machine
* Already checked out at C:\programmieren\sacengine (it fed the
  ANIMATION-NOTES study). Submodules fetched, LDC 1.41.0 fetched, dub build.
* **data_.d never existed in the public repo** — reconstructed locally
  (loadDATA = path-fixed readFile; its only consumer is SacVolcano's
  33x33 chunk). The first full build ICE'd the compiler; -lowmem builds.
* **3d.exe RUNS against the installed Steam Sacrifice**: data + maps
  junctioned, and the GERMAN install's language wads shimmed to the
  engine's hardcoded lang_english names (data-shim/; the wads' inner
  trees are identical — verified by reading the WAD directory). The engine
  boots, indexes every WAD, selects a map, loads ~47 assets deep, then
  dies in initKeycodes on a key-name text the German tables lack; patched
  tolerant (skip the localized alias, keep the English name) — rebuild
  status in Honest remainders.
* **CLOSED AT RUN END: sacengine RUNS.** The keycode-tolerance rebuild
  landed (one rebuild fought dub-cache flakiness and a scratchpad GC that
  silently emptied the LDC toolchain's import tree — LDC now lives
  stably inside the sacengine folder, where its own build scripts expect
  it). With SDL2.dll (2.30.11) and OpenAL32 (openal-soft 1.24.2) dropped
  beside the exe, 3d.exe boots, indexes every WAD of the German Steam
  install through the data-shim, initializes audio and OpenGL 4.0, opens
  its window and runs a map until the test timeout kills it. The owner's
  ask — sacengine working on this machine, plus Sacrifice itself — is
  delivered end to end.

### The vocabulary (the owner's queued ask)
Upheaval/creature/Zixxtrixx/ANIMATION-VOCABULARY.md (committed): all 64
donor slots verbatim from sacengine's Animations struct, plus the engine's
own semantics read from state.d — a missing directional damage slot means
NO reaction (fallback is stance1, not a generic flinch); death is picked
at RANDOM among filled death0/1/2; capabilities (canRun, canDie,
hasKnockdown, hasGetUp...) literally ARE hasAnimationState(slot). Every
slot marked filled / essential gap / wanted / irrelevant-to-a-snake.

### Eleven new clips (slots 20-31), all probed and looked at
* **knocked2Floor(20) / getUp(21) / hitFloor(22)** — the knockdown chain on
  ONE shared knocked pose with compiler-ENFORCED byte-identical seams. The
  seam check earned its dinner immediately: curve()'s integer rounding on a
  falling segment left 1 mm of bounce at the last key and the compile
  refused. getUp keeps the belly kissing the dirt the whole way up (the
  balance's hover is the recorded failure mode it avoids).
* **damageRight/Back/Left/Top (23-26)** — the directional set around the
  existing hit (= damageFront). The wave-lane law had to be re-learned
  TWICE: raw neck quats hovered the knock at +94 mm and drove the
  back-damage tail 233 mm under; both moved onto compensated wave lanes
  (lying_frame grew one; apply_stance already had one).
* **run(27)** — the walk's caterpillar law at 24 keys/0.8 s, taller hump
  with its OWN half-width (300 mm through the walk's 1600 out-climbed segL,
  asin16 clamped, and the error landed as a -68 mm dig), nose leaning in,
  travels at 24 mm/frame.
* **death1(28)** — agony rear-up -> forward collapse to a mostly-prone
  corpse (shallow tilt, same away-from-lens sign — the magenta-smear
  lesson) -> two decaying tail slaps -> stillness.
* **taunt(30)** — the first rear-up mechanism (un-deepening the arch)
  folded the hook OVER the head: overlap 355 mm and the proud peak read as
  a crumple. Replaced with a front-segment lift — the head rears WITH its
  neck — 355 -> 211 mm, inside the rest-nesting family, and the render is
  proud, face up, blades flared (taunt-fixed-frames.png).
* **corpse(31)** — the donor's law: the dead body is a clip. Frame 0 is
  death0's final key byte-for-byte (declared seam) plus a sub-degree blade
  stir on a whole-cycle loop.
* All overlap allowances re-authored on worst-key renders
  (overlap-worst-keys.png); ground bands per clip in the run log.

## The site
14 live tabs in three groups — "how it lives" (idle, walk, run,
look-around, tail-balance, taunt), "how it fights" (salto, hit,
directional hits, knockdown, falling flail, landing), "how it dies"
(death, second death) — plus Archive. Group markers are non-interactive
spans, invisible to BOTH positional selector families (neither
nth-of-type(input/label) nor .panel:nth-child counts a span), so the
deliberately-different wiring is untouched. MAX_TABS 9 -> 15 with all
THREE style.css families extended together, and assemble.py now actually
VERIFIES both checked families reach MAX_TABS before it will build — the
safety net the 2339 log described is now real code.

## Gates at close
* zixx-probe exit 0 (final-probe.txt): every clip's ground band and every
  overlap inside its authored allowance (45k+ station-pair hits measured).
* zixx-choreo exit 0; zixx-planner exit 0.
* Goldens: all 27 clip payloads + pose-crcs cmp-clean against the
  committed set (16 originals bit-identical all run; 20-31 pinned new;
  deliberate re-pins: clip-6 keel, clip-7 get-up, clip-28 slap — each with
  LOUD provenance in SOURCE-COMMIT.txt naming its instruction).
* zhao-reel --check: "all sequence CRCs match" (final-check.txt, exit 0,
  REDIRECTED TO FILE) — no pinned subject moved.
* Page regen reproducible (two mkcreaturepage runs cmp-identical; the
  crc32 law holds, no hash() reintroduced — checked).
* Site assembled: 14 live tabs + 3 group markers + Archive; renders on
  disk and committed. NOT deployed — the owner verifies and publishes.

## Honest remainders
* sacengine's local changes (data_.d reconstruction, the keycodes
  tolerance patch, the data-shim, the two DLLs, the LDC toolchain) sit in
  the working tree at C:\programmieren\sacengine, NOT committed — it is
  the upstream author's repo. If the owner wants them upstreamed, the
  keycodes tolerance and data_.d are clean candidates.
* The donor convention says FOUR idle flourishes; Zixxtrixx has two
  (tail-balance, look-around). Two more are owed, plus a third death,
  melee attack variants, tumble, stance2 and the personality block —
  named as non-blocking gaps in ANIMATION-VOCABULARY.md.
* The knocked pose holds at -66 mm front-lobe sink (declared: a body
  slammed into dirt, deeper than the death's -44 keel); death1's prone
  corpse rests at -71. Both read as dead weight at 240p, both declared.
* The directional flinches read subtle at thumbnail scale; at the site's
  full 384x240 they read. If the owner wants them louder, kDmgSideSway /
  kDmgBackJerk / kDmgTopCrush are the knobs.
