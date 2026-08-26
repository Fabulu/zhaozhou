# Task Log: RUN-20260826-0617 - Zixxtrixx, first creature

**Created:** 2026-08-26 06:17 UTC+02:00
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260826-0617-zixxtrixx-first-creature-model/

---

## Objective

Take the first two pieces of concept art (Zixxtrixx, by S. Hofer) and turn them
into a creature that compiles through `zref::creature` and renders at 384x240,
applying the owner's three deviations from the drawing: bigger eyes, blockier
three-prong tail, head more distinct and exaggerated.

---

## Progress Timeline

### 2026-08-26 06:17 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260826-0617
- Initial context: first concept art received; no creature has been built before.

### 2026-08-26 06:20 UTC+02:00 - Reconnaissance complete (no subagents)

Read both concept sheets, all six creature guides, `zref_creature.hpp`,
`compile_creature`, and the reel tool. Located both external references.

**Concept read (Front.png, Side.png):**
- Rounded head, wider than the body, with a pink dorsal cap over a blue face.
- Two large yellow eyes with orange rims; side view shows a vertical orange slit
  pupil. A short straight mouth slit.
- Body: pink dorsal stripe running the whole length, green flanks/belly, blue
  wedge under the chin.
- Tail forks. Side view shows two long prongs plus a short middle nub = three.
- Side sheet carries two small ink thumbnails of the S-curve pose (top right).

**Found:**
- Sacrifice, Steam app 38440, installed at
  `C:\Program Files (x86)\Steam\steamapps\common\Sacrifice`. The SAXS model data
  is right there: `data\saxs*.wad` (14 files, plus `saxshero.wad`, `saxs_wiz.wad`).
- sacengine cloned at `C:\Programmieren\sacengine`, HEAD `10334b8 Volcano shape.`
  It ships `source/saxs2obj.d` -- a model-and-skeleton OBJ exporter, which is the
  tool for taking donor measurements.

### 2026-08-26 - Fabian's decisions, and the authorisation

Answers to the three open questions, plus scope:

- **Scale:** a smaller-tier creature, but still *relatively big in Sacrifice
  terms*. Read as: low on the roster ladder, physically large on screen.
- **Slither cycle:** two waves. Keep it malleable.
- **Dorsal stripe:** geometry. (Decision 3 resolved.)
- **Textures:** pull the colours directly out of the concept art and see how it
  looks. Transforming them is my call.
- **Toolchain:** install it. DMD 2.112.0 installed via winget, exit 0.
- **Attack:** it stabs with its TAIL, into the head of the target, and drives
  the target to the ground.
- **Run autonomously.** First concept; imperfect is expected and fine.
- **Leave knobs** so Fabian can influence the result or the steps on the way.
- **Publish** `upheaval.pages.dev`, shaped like zhaozhou.pages.dev: one card per
  creature, stacked vertically, each card with a control to switch between that
  creature's animations. Every animation is shot with a slow camera orbit while
  the creature performs it on the ground. Slither should travel if it can.
- **Authorisation to publish given explicitly** ("build it and push it"), which
  is the explicit call `deploy.ps1` and the naming memory both require.

### 2026-08-26 - Palette taken from the concept art

Median RGB over named regions of both scans, paper and ink excluded. The two
sheets agree, which is why these are usable as-is:

| part | Front.png | Side.png | adopted |
| --- | --- | --- | --- |
| dorsal pink | 228,203,222 | 225,203,220 | **226,203,221** |
| head blue | 16,162,213 | 22,164,212 | **20,163,213** |
| eye yellow | 246,237,171 | 245,235,162 | **246,236,167** |
| eye rim orange | 212,121,96 | (box missed the pupil) | **212,121,96** |
| flank green | 122,209,161 | 109,201,132 | **116,205,147** |

Script kept at `scratchpad/regions.py`; the boxes are in the 2000-px display
space of the scans and scale by width/2000.

### 2026-08-26 - Reel machinery confirmed (reading, not guessing)

- `sub.orbit` already yaws the world one EXACT turn over `sub.frames`. That is
  the website camera, already built. It rotates about the world ORIGIN, so a
  travelling creature must cross the origin or it orbits out of frame.
- Locomotion root motion is NOT in the clip: the reel advances `inst.x/z` along
  `facing` per frame, and `column_query` snaps `inst.y` to the ground.
- A key is held 2 ticks, so **reel frames = clip keys x 2** at `step = 1`. Clip
  lengths and subject frame counts must be chosen together or the GIF will not
  loop.
- `Bone` is `{parent, tx, ty, tz}` and **rest rotations are identity** - a pure
  translation chain. So there is no bind rotation: the prong splay, the head
  tilt and every other non-quarter-turn angle must be BAKED INTO EVERY FRAME of
  every clip. That is a real authoring cost and it is why the prongs cannot
  simply be "pointed" at build time.
- The reel enforces a **256-colour ceiling across a subject's whole frame set**.
  Creature colours compete with sky, terrain and shading bands for that budget.

### 2026-08-26 - The model, and four rendered iterations

Built `zhaozhou/tools/reel/zixxtrixx.h`: 28 bones, ~38 rigid ring parts, two
clips, every tunable a named constant in one KNOBS block at the top of the
file. Wired two subjects into the reel (`zixxtrixx-slither`,
`zixxtrixx-strike`) using the EXISTING `sub.orbit` flag, which already yaws the
world exactly one turn across a subject's frames.

**Rig shape.** Bone 0 is the root at the front of the body; the head hangs off
it, then ten spine joints run backward, then the fork and three prongs. The
skull cap, both eyes and the dorsal ridge are extra bones carrying parts. Ten
spine joints over two waves is five joints per wave, which is what sets how
smoothly it bends — for a serpent, part count IS bend smoothness.

**What each render pass taught, in order.** Every one of these was found by
rendering and looking, not by reasoning:

1. **352 colours, and a green snake on green grass.** The creature vanished
   into the default olive terrain material, because its flank green comes
   straight off the concept sheet and the two were nearly the same value.
   Changed the GROUND (to a dry ochre) rather than repainting a creature whose
   colours are the point.
2. **Too thin to read.** A true-to-life serpent is a stick at 384x240. Body
   radius 152 -> 205 mm against a 2.7 m body, so roughly 7:1 rather than the
   30:1 of a real snake. This is the same exaggeration the eyes got, applied to
   the whole animal.
3. **The tail was not rearing.** It looked like it in pass 1. It was the
   lateral wave seen end-on through a 26-degree-down camera, which reads as
   vertical. The genuine rearing was `TiltMode::kCompletely` pitching a 3.9 m
   animal off ONE ground column sample and lifting its tail a metre over a
   crest — a quadruped rule applied to something lying on the ground. Now
   `kSideways`.
4. **The eyes were buried and the cap was a helmet.** The skull is a body of
   revolution 335 mm in radius; eye bones at radial 278 put the whole yellow
   ball INSIDE the head. Moved them onto the snout where the skull has already
   narrowed. The pink cap at 208 mm swallowed the skull, so it became a crest.
5. **The rim slabbed out past the nose** once the eyes moved forward: the rim
   disc was wider than the snout it sat on. Backed the eyes onto the fuller
   part of the skull and shrank the rim.

**The colour transform, stated rather than done quietly.** The raw pencil pink
(226,203,221) and yellow (246,236,167) are very desaturated, and under the
scene light they resolved to near-white and olive — the crest read as a grey
helmet and the eye stopped reading as an eye. Hue kept, saturation pushed:
pink -> (228,146,194), yellow -> (250,226,92). Both constants carry the
original value in a comment so the change can be undone in one edit. Green and
blue survive unchanged from the sheet.

**Two things were CUT, both for the 256-colour law:**

- the blue-to-green chin transition (`kTeal`) — a whole shading band for
  something invisible at 240p;
- the mouth slit — a bone AND a band, for two pixels the skull swallowed. The
  concept has one. If Zixxtrixx is ever shot in close-up it should come back.

**The 256-colour ceiling is the real constraint on this lane**, and it was
underestimated. It is not a creature budget: sky, terrain and the creature all
compete, and EVERY EXTRA FRAME COSTS, because each frame is another camera
azimuth that re-shades sky and terrain into new entries. Measured on the way
down: 359 -> 318 (dropping the animated terrain field and the teal band).
`terrain-orbit`, which passes, spends 166 at 64 frames. That is why the slither
subject is one gait cycle per orbit and not two.

**Build note for whoever comes next:** `cmake --build` intermittently fails
regenerating `build.ninja` (a Verilator output caught mid-write), and when it
does, the reel RE-RENDERS THE STALE BINARY and reports the old numbers. Twice I
read an unchanged colour count as "the fix did nothing". Run
`cmake -S . -B build` directly (~67 s) and rebuild.

### 2026-08-26 - Shipped

**https://upheaval.pages.dev** — Cloudflare Pages project `upheaval` created,
production branch `main`, deployed on Fabian's explicit instruction. Unlisted
and `noindex`; the `noindex` gate in `deploy.ps1` passed. Verified live: page
200, both GIFs 200, style.css 200, tab markup present.

**NOT verified: the tabs in a real browser.** There is no browser automation in
this session, so the radio/label/panel structure and the CSS selectors were
checked by reading, and the assets by fetching. Nobody has clicked them.

Both animations encoded through the ported `togif.py`: palette-exact, dithering
off, never palettegen, then the GIF DECODED BACK and compared byte-for-byte
against every source frame. Both verified byte-exact.

| subject | frames | colours | gif | frame-change median |
| --- | ---: | ---: | ---: | ---: |
| zixxtrixx-slither | 64 | 239 / 256 | 871 KB | 20.6% |
| zixxtrixx-strike | 96 | 249 / 256 | 1.24 MB | 17.5% |

`expect_seq_crc` pinned for both; `reel --check` passes across the whole
library, so no existing subject drifted.

**Two mistakes worth keeping.**

1. **I twice read a stale binary's output as evidence.** `cmake --build`
   intermittently fails regenerating `build.ninja` (a Verilator output caught
   mid-write) and the shell then runs the OLD exe, which reports the OLD
   numbers. Both times I briefly concluded "the fix did nothing". The tell is
   an unchanged number after a change that must have moved it. Workaround that
   ended up being faster than fighting it: compile and link the reel directly,
   since it depends on nothing Verilator produces.
   `g++ -Itests/render -Icompiler/tests/generated -Ireference/src
   -Ireference/include -Iruntime/include -O3 -DNDEBUG -std=c++17 -c
   tools/reel/zhao_reel.cpp -o <obj>` then link with
   `build/reference/libzhao_zref.a`.
2. **Two scripted edits corrupted source files** — a here-doc ate `
` escapes
   in generated Python, and a slice-based replacement whose two bounds were in
   the wrong order inserted a block at the top of the header. Both were caught
   by the compiler and by looking at the output, but the second one silently
   de-indented a `return` and produced a page with every creature card
   truncated after the blurb, which the build did NOT catch. Read what a
   scripted edit produced.

**Left undone, deliberately:**

- Stance loop and the four idle flourishes (86 of 86 donor entities have them),
  hit reaction, death at ~2x attack.
- The strike's prongs land just above the head rather than clearly past it.
  Pushing the drive further is one number (`kArc` key 27) but the subject has
  only 7 colours of headroom, and a bigger arc exposes more surface.
- The mouth, the chin transition and the orange eye rim are all recoverable if
  the palette ever loosens or the creature is shot in close-up.
- sacengine was never run. DMD 2.112.0 is installed, but the existing measured
  notes answered every question this creature actually raised, so building it
  would have been work for its own sake. The lane is open when a question needs
  it.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| - | - | none spawned; recon done inline | - | - |

---

## Files Created

- `SPEC_v1.md`
- `TASK_LOG.md`

---

## Decisions Made

*Nothing about the creature itself is decided yet -- awaiting Fabian on the
questions in SPEC_v1 "Open Questions". Findings that constrain those choices:*

1. **The render path already exists end to end.** `tools/reel/zhao_reel.cpp`
   builds a "watchdog" quadruped inline from `zc::RingPart` + `zc::Skeleton` +
   `zc::ClipBank`, compiles it with `zc::compile_creature`, and composites it
   through the renderer's pre-resolve hook. `build/tools/zhao-reel.exe` is
   already built. Zixxtrixx is authored the same way: it is a new subject
   alongside `creature-wave-walk` and `creature-bulk-pop`.

2. **Several parts MAY share one bone.** `compile_creature` (creature_core.cpp
   L441-456) validates only `p.bone < sk.bone_count` -- there is no uniqueness
   check. "One part = one bone" bounds how a part BENDS, not how many parts a
   bone may carry. This is what makes eyes, prongs and colour bands affordable:
   they cost parts and meshlets, but not bones, and bones are the scarce
   resource for a serpent.

3. **Colour is per-part and flat, today.** `SkinVertex` carries u/v but has
   "NO colour lane"; `Meshlet` carries r/g/b as the CLUT8 page stand-in. So the
   concept's longitudinal stripes cannot be painted -- they run AROUND the ring,
   not along the body, and U is not authorable anyway (U = ring angle high byte).
   Either the stripe is built as geometry riding the body on the same bones, or
   the creature reads as banded-along-its-length, which is the wrong banding.

4. **The owner's three deviations are all cheap under (2).** Bigger eyes and
   blockier prongs are extra rigid parts on existing bones. Exaggerating the head
   is ring radii, which are free.

---

## Next Steps

1. Fabian to look at https://upheaval.pages.dev and say what is wrong with the
   creature. It is a first concept and was built to be argued with; the knobs
   in `tools/reel/zixxtrixx.h` exist so the argument is cheap to act on.
2. Someone should click the tabs in a browser. See above.
3. The rest of the animation set: stance, four idle flourishes, hit reaction,
   death.
4. Push the strike's prongs past the head, if the palette allows.

**Status: complete.** Both animations shipped, site published.
