# RECON 1/2 — Creature 02, second pass: FORM, RIG, TEXTURE, EYES

**Date:** 2026-09-05 · **Scope:** OWNER-DIRECTION-2 §1a, §1b, §1c, §1d, §2, §6.
Effects (§3) belong to the other recon. **Read-only on source; nothing committed.**

**Lane:** zhaozhou `780bf4f0` / Upheaval `aa364ee` (both at `origin/main`).
**Build:** `tools/reel/build-direct.sh --output <scratch>/build reel` (never cmake).
**Render:** `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross` set explicitly.
No background jobs were left running. `sacengine` never invoked.

**Diagnostic images** (scratchpad `recon-form/`):

| file | what it proves |
| --- | --- |
| `dongle-evidence.png` | the free-floating arm and the punch-through, four clip frames |
| `published-posters.png` | the six shipped posters the owner was looking at |
| `gasrim-lit-vs-unlit.png` | the gas rim is lighting, not geometry |
| `gasrim-three-rigs.png` | rim width 4 px / 21 px / 49 px across three rigs |
| `eyes-sheet-vs-render.png` | **the key image** — sheet and render at identical framing |
| `sheet-eye-trace.png` | the traced ball circle and eye axes drawn on the sheet |
| `edge-zoom-curious.png` | the rim at 8x on a shipped clip frame |
| `u02-s4-{front,side,tq,wire,unlit}-x3.png` | the form diagnostics |

Everything below is marked **[V]** verified by render or by arithmetic over the
committed constants, or **[I]** inferred with the mechanism named.

---

## 1a. The floating dongle — FOUND, and it is worse than "one dongle"

**It is the RETURN ARM of the antenna loop: hinge ball C plus the whole tube
segment below it.** [V]

### How it is attached now

`unnamed02_rig.h:39-42` — the bone chain is `root -> A -> B -> C` and **stops
at C**. `unnamed02_model.h:109-141` builds the loop as one chain part of 26
rings whose total length is
`kLoopBuryMm + kLoopArcMm[0..3] + kLoopBuryMm` = 250+1441+540+949+1227+250 mm.
The weight rule at `unnamed02_model.h:125-138` puts every ring above station
`stC` fully on `kBHingeC`. So the last **1477 mm of tube — the return arm plus
its buried tail, over a third of the whole loop — is rigidly welded to bone C**
and has no joint of its own. Where it ends up is entirely a consequence of C's
rotation.

### Why it clips, and why it floats

`loop_pose` (`unnamed02_clips.h:129-138`) scales all four fold angles by a
per-mille knob every clip drives. Walking the bind chain through those rotations
and testing the tube tip against the body ellipsoid (rx 450, ry 450·1.66 = 747):

| fold scale | where the tube tip lands | verdict |
| ---: | --- | --- |
| **780** (startle f14) | 1267 mm outside the body | **free in the air** |
| **880** (curious f45) | 603 mm outside | **free in the air** |
| **940** (channel) | 194 mm outside | free |
| **955** (hover/drift/rest floor) | 94 mm outside | free |
| **1000** (rest pose) | 183 mm *inside* | connected only by intersection |
| **1085** (channel peak) | 66 mm inside — near the far surface | about to emerge |
| **1160** (startle f22) | 349 mm outside the **front** | **punched through** |

**Break-even is fold scale ≈ 975.** Every clip in the bank crosses it:
hover / drift / rest reach 955, channel 940, curious 880, startle 780.
At pm ≤ 955 the *entire* return arm — not just the tip — never comes within
94 mm of the body; at pm 780 its closest point is 1267 mm clear.
See `dongle-evidence.png`: startle f14 is the hanging tail, startle f22 is the
capped tube end poking out of the far side of the ball, curious f45 the tail
again, curious f0 the rest pose spearing in with no join.

The owner's two words are the two failure modes of the same defect: it **floats
free like a tail** below pm 975 and **clips into the creature** above it. It is
never *connected*.

### What connecting it properly requires

1. **A bone at the re-entry joint.** The return arm needs its own bone below C
   so the arm is a driven limb, not C's rigid tail.
2. **A closure constraint, or a re-parented tail.** The clean fix is to make the
   loop's far end a child of the BODY rather than a free end of the chain: bind
   the last rings to a `kBLoopBase2` bone parented to `kBRoot` at the drawn
   re-entry point, and blend the middle rings between it and the new arm bone —
   exactly the two-bone blend already used at every other station
   (`unnamed02_model.h:119-138`). Then the loop is closed by construction and no
   fold scale can open it.
3. `kLoopArcMm[3]` (1227) and `kLoopBuryMm` (250) become the arm's own length
   knobs instead of a hope that C's rotation lands them somewhere useful.

---

## 1b. Hinges and bones — the inventory, and what is missing

### The skeleton today — 8 bones (`unnamed02_rig.h:15-51`) [V]

| bone | id | drives |
| --- | --- | --- |
| `kBRoot` | 0 | the body ball, hover translation, body attitude — **and the neck fold** |
| `kBHingeA` | 1 | hinge ball A + the first loop arc |
| `kBHingeB` | 2 | hinge ball B (the peak) |
| `kBHingeC` | 3 | hinge ball C + **the entire unjointed return arm** |
| `kBEyeL` / `kBEyeR` | 4, 5 | the two lenses |
| `kBPupilL` / `kBPupilR` | 6, 7 | the two star pairs |

The three drawn ball hinges **do** have bones. The owner's other half —
"the parts connected to the body are also hinges" — does not.

### Hinges with NO bone [V]

1. **The neck joint, where the loop leaves the body.** There is no bone for it.
   `loop_pose` applies the neck fold to **`kBRoot` itself**
   (`unnamed02_clips.h:134`), and `kBRoot` carries the body ball
   (`unnamed02.h:95`) and parents both eyes. **The "neck hinge" is therefore a
   whole-creature lean that swings the face with it** — it cannot articulate the
   antenna independently at all. This is a real defect, not just a missing knob.
   The loop's lowest rings are likewise skinned to `kBRoot`
   (`unnamed02_model.h:127-129`), so the body bone doubles as the neck bone.
2. **The re-entry joint, where the loop's far end returns to the body** — §1a.
   Nothing exists past `kBHingeC`.
3. **The return arm itself** — no bone; welded to C.

### The bone budget — there is enormous room [V]

`05-BUDGETS.md`: the cap is **32 bones**; donor creatures ran **11–32**.
Frame cost is 8 B per bone per frame (`quat16`); a 32-bone frame is ≤ 268 B.
**We are using 8. Twenty-four are free.** Adding the missing joints is cheap and
uncontroversial:

| add | why |
| --- | --- |
| `kBNeck` (child of root, at the neck exit) | frees the antenna from the body lean — **the highest-value single bone in the creature** |
| `kBHingeD` (child of C, at the return arm's start) | makes the arm a limb |
| `kBLoopBase2` (child of root, at the re-entry) | closes the loop by construction |

That is **11 bones** — a third of the cap, and still less than the smallest donor
creature. A further optional `kBAttitude` (cut in the first pass, see the header
comment at `unnamed02_rig.h:4-6`) and per-lens lid/squash bones would still leave
headroom. **Nothing about the bone budget constrains this pass.**

Reminder for the implementer: the pivot law already observed here — each hinge
bone's bind translation sits **at its own ball's centre / on the tube at its fold
station** — must hold for the new bones too.

---

## 1c. The gas rim — what actually makes it

**It is the darkest band of the three-band toon ramp landing at grazing
incidence on a sphere. It is a LIGHTING artefact. There is no shell, no
outline-adjacent geometry, and the ink has nothing to do with it.** [V]

### Proof

`gasrim-lit-vs-unlit.png` — same pose, same camera (`u02-s4-tq` vs
`u02-s4-unlit`, the fullbright diagnostic). Scanline y=185, the pixel just
inside the ink:

* **lit:** `(90,32,74)` then `(165,53,123)` — two darker rings before full pigment
* **fullbright:** `(230,73,148)` — full pigment right up to the ink, **no rim at all**

The black line itself is a separate, opaque, **exterior-only** flood ring
(`zhao_reel.cpp:2855-2922`, `ink_width=3` at this body size) that never
overwrites a creature pixel — so it cannot be producing an interior band.

### Why it reads see-through

The ramp preserves hue: each channel is scaled by `quantised/mean`
(`creature_sim.cpp:728-739`), so the band is the *same pigment at a lower level*.
That is exactly why it reads as a veil over the body rather than a painted ring —
and it is why any thickening route that works through the light keeps the
see-through quality for free.

### The knob that thickens it a lot

The band boundary is where the lambert response crosses
`kCel3Thresh[2] = {43000, 57000}` (`creature_sim.cpp:723`). Two candidate knobs:

* **`kCel3Thresh` / `kCel3Level` — DO NOT TOUCH.** They are global constants
  shared with Zixxtrixx's CRC-locked bank. Moving them re-renders 22 subjects.
* **The creature's light rig — this is the knob.** `CreatureLightRig`
  (`zref_creature.hpp:1024-1030`) carries `ambient_r/g/b`, `key_gain`,
  `fill_r/g/b`. Measured rim width on the same frame, native, scanline y=185:

  | rig | ambient | dark rim |
  | --- | --- | ---: |
  | `diagonal-silver-moon` | high | **4 px** |
  | `diagonal-cool-cross` (shipped) | .40/.45/.56 | **21 px** |
  | `diagonal-hard-sun` | low | **49 px** |

  **Ambient is the correct term to move.** [I, mechanism named] It is the
  isotropic component: it is what lifts the *whole* sphere above the threshold,
  so lowering it grows the dark band **evenly around the silhouette**. Lowering
  `key_gain` instead grows a terminator across one side — visible in
  `gasrim-three-rigs.png` bottom, where hard-sun's 49 px is half the ball in
  shadow, not a rim. The cool-cross rig produces an even ring precisely because
  its strong *opposing* crossfill lights both sides and leaves only the grazing
  edge dark; **keep the key/fill opposition, cut ambient.**

**Convergence worth flagging to the architect:** the many-coloured rig asked for
in §6, `kCreatureLightMovingInspection` (`creature_sim.cpp:522-527`), has
ambient **.19/.22/.28 — roughly half Cool Cross's**. Adopting it will thicken
the gas rim substantially on its own. §1c and §6 are one change, not two.

---

## 1d. The pink — sheet against render

**Sampled from a rendered frame at native 384x240 with `tools/reel/rgbframe.py`**
(no second reader written), `u02-s4-front`, celmain + diagonal-cool-cross,
magenta-only mask (the sky is also pink — an orange pink; separating on `B-G`
is required or the sample is 100% sky). [V]

| | R | G | B | hex |
| --- | ---: | ---: | ---: | --- |
| **Sheet** `Front.png`, median of the eroded body fill | 219 | 70 | 126 | `#DB467E` |
| **Shipped pigment** `BODY_PINK` | 230 | 74 | 146 | `#E64A92` |
| **Shipped pigment** `BODY_PINK_DEEP` | 196 | 44 | 112 | `#C42C70` |
| **Rendered, lit band** (1434 px) | **255** | **85** | **189** | `#FF55BD` |
| Rendered, mid band | 230 | 73 | 165 | |
| Rendered, shadow/gas band | 156 | 53 | 123 | |
| Rendered, all body pink, median | 247 | 81 | 173 | |

**Two separate causes, and the pigment is the smaller one.**

1. The authored pigment is already close to the sheet in R and G, but **20 counts
   bluer** (B 146 vs 126) — a magenta where the sheet is a crimson.
2. **The light does the rest.** The lit band renders at `(255, 85, 189)`: red is
   **clipped at 255** while blue is pushed **+43 above the pigment**. The
   diagonal-cool-cross rig is blue-weighted end to end — ambient .40/.45/.56 and
   fill .25/.38/.55 — so the channel with the least headroom clips and the
   bluest gains most. The result is the pale lilac the owner is objecting to.
   Deepening `BODY_PINK` alone will not fix it; red will simply clip again.

**Constants:** `tools/pack/mku02page.py:36-39` — `BODY_PINK`, `BODY_PINK_DEEP`,
`LOOP_PINK` (214,62,132), `HINGE_PINK` (238,98,162). The page is regenerated by
`mku02page.py` and is gitignored. The fallback flats `kGreyR/G/B`
(`unnamed02_art.h:179`) are only used when the page is absent.

Per `CLAUDE.md`: the sheet numbers above are **a comparison, never the shipped
value**. The scanner flattens and lightens, and the read at 240p on dark ground
under this rig is the thing. The finding is the *bias* — too blue, and clipping
red — not a colour to paste in.

---

## 2. THE EYES — the big one

**`eyes-sheet-vs-render.png` is the deliverable here.** Sheet and render are
cropped to the same 1.25 × ball-radius box, so the two faces are directly
comparable.

### Traced from `Concept/Front.png` [V]

Traced by segmenting the sheet and measuring the merged lens (purple + white
ring + cyan star) by PCA. **All lengths are in units of the drawn head-ball
radius R**, so they are scale-free and transfer straight to `kBodyRadiusMm`.

Reference frame: drawn ball centre, R = 539 px at full sheet resolution;
the whole creature is 1.87 ball diameters tall.

| traced quantity | value | as a constant (R = `kBodyRadiusMm` = 450) |
| --- | --- | --- |
| lens **half-length** | **0.78 R** | `kEyeLongMm` ≈ **350** (now 250) |
| lens **half-width** | **0.205 R** | `kEyeWideMm` ≈ **92** (now 92 — **already right**) |
| lens **aspect** | **3.8 : 1** | now 2.72 |
| lens centre, off the midline | **±0.42 R** | `kEyeZMm` ≈ **190** (now 150) |
| lens centre, below ball centre | **0.11 R** | `kEyeYMm` ≈ **−50** (now −70) |
| **tilt from vertical, each lens** | **≈ 28°** (25.2 and 31.6 measured) | `kEyeVAngleA16` ≈ **5100** (now 2600 = 14.3°) |
| **included V angle** | **≈ 57°** | now ≈ 29° |
| the V **apex** | on the midline, **0.68 R above the ball centre** | the two upper tips are **0.026 R apart — they touch** |
| lower tips | reach **≈ 1.05 R** from centre — they meet the outline | |
| **star span** | **0.34 × lens length**, **1.3 × lens width**, ≈ **0.54 R** | `kPupilStarArmMm` ≈ **120** (now 78); `kPupilStarWideMm` ≈ **50** (now 30) |
| star centre | **0.06 lens-lengths above** the lens centre | |

The star is **wider than the purple band is wide** (1.3×) — it bulges past the
almond's sides, which is why the white ring balloons around it in the drawing.

**Area split inside one lens — this is the colour finding:**

| | purple | white ring | cyan star |
| --- | ---: | ---: | ---: |
| **sheet** | **65–71 %** | **4–7 %** | **20–22 %** |
| **render** | 75–93 % | 1–20 % | **6–7 %** |

### The pigments [V]

| | sheet (median) | shipped | verdict |
| --- | --- | --- | --- |
| the "whites" — **deep purple** | **(105, 71, 161)** `#6947A1` | `EYE_PURPLE` (104, 42, 168) | **already essentially right.** Sheet is slightly greyer (G 71 vs 42). The deep-purple instruction is satisfied by the existing constant. |
| the ring | (240–248, 241–249, 243–249) | `EYE_RIM_WHITE` (246,242,250) | correct |
| **the star blue** | **(44, 179, 205)** / (24, 169, 197) `#2CB3CD` | `STAR_CYAN` (64, 220, 240) | shipped runs **lighter and greener**; the sheet is a deeper teal |

`mku02page.py:40-48`; `kLensR/G/B` and `kStarR/G/B` at `unnamed02_art.h:110-114`
are the pageless fallbacks only.

### How the current eyes differ — the list

1. **Far too short.** 0.56 R half-length against 0.78 R. The width is already
   right, so the almond is a stub, aspect 2.7 where the drawing is 3.8.
2. **Nowhere near splayed enough.** 14.3° from vertical against ~28°. In the
   render the two lenses are nearly parallel; the drawing is a wide V.
3. **Too close together and too low.** ±0.33 R against ±0.42 R, and sitting
   below the ball centre where the drawing runs from 0.68 R *above* the centre
   down to the silhouette.
4. **No apex.** The drawing's two upper tips touch on the midline and the eyes
   read as one V-shaped face. The render's lenses are two separate slivers.
5. **The white is a fat solid patch, not a ring.** `mku02page.py:110-123` paints
   white over `|v−0.5| < 0.21` **and** `du < 0.40` — the middle 42 % of the lens
   length across 40 % of its circumference, a filled blob. The drawing gives
   white **4–7 %** of the lens area as a thin outline around the star. This is
   why the render's eye reads as a white pill rather than a purple almond.
6. **The star is a third the size it should be**, and reads as a thin symmetric
   cross where the drawing is a chunky four-pointed star with tapering, slightly
   curved arms, set at an angle.
7. The star's cyan is lighter and greener than the drawn teal.

### Popping proud of the surface — **PROTECTED, measure it and keep it**

Owner clarification relayed 2026-09-05: *"Eye needs to keep being a 3d thing
that pokes out like it is now. Artist said she likes it that way very much."*
The sheets draw the lenses flush; **the model wins on this one point.** Trace the
shape, proportion, spacing, V angle, star form and colours — **not the flushness.**

The current protrusion, computed in 3D over the posed lens and star surfaces
against the body ellipsoid (not measured off a frame — a projection would lie
here): [V]

| | ellipsoid radius | stands proud |
| --- | ---: | ---: |
| lens bone centre | 1.014 | 6 mm (already outside) |
| **lens outermost point** | **1.369** | **166 mm — 37 % of the body radius** |
| **star outermost point** | **1.223** | **100 mm** |

At the shipped scale (body ball ≈ 73 px across at native 384×240, ≈ 12.3 mm/px)
that is roughly **13 px** of lens crown and **8 px** of star standing off the
ball. **These are the numbers to preserve.**

The knobs that produce them: `kEyeXMm` 425 (against `kBodyRadiusMm` 450 — the
bone is already outside the surface), `kEyeDeepMm` 58 (bulge depth),
`kEyeBulgeMm` 88 (the star's stand-off and its gaze pivot radius), and the
`face_rest` rotations `kEyeYawOutA16` 3600 / `kEyeVAngleA16` 2600 /
`kEyeTiltA16` 2200.

**Warning for the implementer:** growing `kEyeLongMm` to ~350 and doubling
`kEyeVAngleA16` swings the almond tips further out along the same radial, so the
protrusion will *increase* as a side effect — the lens crown will pass well
beyond 1.369. Re-measure it after the change and pull `kEyeDeepMm`/`kEyeXMm`
back if the tips start reading detached. The target is the current *read*, not a
bigger one.

Also protected by construction and worth not losing: the pupil pivots sit at the
lens centres (`unnamed02_rig.h:49-50`) so gaze rotations sweep the stars *across*
the faceted lens instead of sliding over it; and the lens is real faceted
geometry at `kEyeFacetSegments` 8, which is the "partly polygonal" read from
Direction 1.

---

## 6. The many-coloured lighting — identified

**It is the FOUR-SOURCE MOVING LIGHT, not the per-clip suns.** [V]

* Rig: `zc::kCreatureLightMovingInspection` (`creature_sim.cpp:522-527`) —
  a dim base (ambient .19/.22/.28, key .24, fill .07/.10/.15) that deliberately
  leaves room for the coloured pools.
* Sources: **four** world-space `CreaturePointLight`s on authored Direction-27
  paths — the tamed warm lamp (emits nothing), **blue**, **red** and **green** —
  placed by `place_zixx_moving_sources` (`zhao_reel.cpp:2459-2510`), each with a
  multiplicative gain **and** a Direction-28/30 additive emission.
* The site names it in as many words: the Zixxtrixx card's second tab,
  *"Four-source colour-light inspection with the normal additive emission"*
  (`website/creatures.json`), and Direction 30 records that the prototype's
  settings became the shipped settings.

**Not** the per-clip suns: `unnamed02` already uses those
(`subject_u02_clip` sets `s.sun = &kU02Sun*`), and they are what the owner is
calling "the old single-rig look".

### How a subject selects it

One flag: **`SceneSubject::creature_moving_light = true`**
(`zhao_reel.cpp:1087`; set at `zhao_reel.cpp:5189`). At render time
(`zhao_reel.cpp:2760-2765`) that swaps in the inspection rig, points
`g_creature_point_lights` at the subject's four `moving_sources`, sets the count
to 4 and turns on `g_creature_additive_light`. Everything is saved and restored
around the compose call, so it is subject-scoped by construction.

### Three things the architect must know before adopting it [V]

1. **It is mutually exclusive with the clip sun today.**
   `zhao_reel.cpp:3204` reads
   `sub.sun != nullptr && g_zixx_suns_enabled && !sub.creature_moving_light`.
   Every `unnamed02` clip currently sets `s.sun`. One of the two has to give.
2. **The point-light array holds exactly four.**
   `kCreatureMaxPointLights = 4` (`zref_creature.hpp:1097`), and
   `08-LIGHTING.md` states the four-source budget as a hard law. Four moving
   sources plus a sun is five. **Sun or many-coloured, not both**, unless the
   budget is raised in the reference — which is a hardware-lane question, not an
   art-pass one.
3. **The paths are staged for Zixxtrixx and must be re-authored.**
   Placement is instance-relative (`inst.x/y/z`) but offset by
   `zixx::kStageCentreMm`, and the extents — orbits of 2050/1550/1700 mm, inner
   900 / outer 2600 mm — are sized to a ~4 m signature-S. `u02::kStageCentreMm`
   already exists (`unnamed02_art.h:159`, value 0). The conduit's body ball is
   900 mm across; the pool radii will need u02-owned constants or the colour
   events will not land on the animal.
4. **Bonus, already noted in §1c:** the rig's low ambient thickens the gas rim
   for free. Do §1c and §6 as one change and judge them together.

---

## Cross-cutting notes for the architect

* **Everything above is a named, editable constant.** Nothing found here needs a
  generated value; §1a and §1b need new *bones*, which is structure, not a knob.
* **The pale pink and the missing gas rim are the same underlying cause** — the
  cool, blue-heavy, high-ambient Cool Cross rig. Both §1c and §1d point at a
  u02-owned light rig, and §6 hands one over. Judge the three together, at
  native, on dark ground, before touching `BODY_PINK`.
* **`kVStretchPm` = 1660 is load-bearing** (`09-ENGINE-GOTCHAS.md` §1). Every
  vertical dimension in the eye numbers above is quoted in *on-screen* ball
  units, which is the frame `kVStretchPm` already corrects into. Do not stretch
  them a second time.
* **Untextured parts render black under `celmain`** (`09-ENGINE-GOTCHAS.md` §0,
  §7). Any new part — a gas shell, a re-cut star, a new lens page — must carry a
  page tile or it ships black. This has already bitten this creature once.
* The other open art reads from `UNNAMED02-INDEX.md` §7 are all still true and
  all still knobs: the loop reads as an angular coat-hanger and ~27 % short, the
  antenna reads as a uniform rod from the front, and the body reads as a sphere
  plus a tube where the sheet is a continuous teardrop. **The `s4-side` and
  `s4-tq` renders here confirm every one of them.** Fixing §1a by closing the
  loop is the natural moment to also re-author `kLoopArcMm[]` and the fold
  angles toward the drawn teardrop.

## What was NOT established

* Whether a lower-ambient u02 rig thickens the rim *evenly* — the mechanism is
  named and the three-rig sweep is consistent with it, but no rig with cool-cross
  key/fill opposition **and** cut ambient exists to render. **[I]** First job for
  the implementer: author one and look.
* Any effects question — the mana, the pulsar, the lightning. That is recon 2.
