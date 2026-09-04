# RECON 3 (THE EYE) — creature 02 against the sheets

**Date:** 2026-09-05 · **Mode:** read-only on source; built and rendered only.
**Method:** rendered the model on the shipped lit path (`ZIXX_EXP=celmain`,
`zhao-reel-cel`), put it beside `Concept/Side.png` and `Concept/Front.png` at
matched scale, and looked. Numbers appear only where they confirm something the
eye already found, or where they refute an inherited claim.

**Plates** (scratchpad `recon-eye/`):

| plate | what it shows |
| --- | --- |
| `PLATE-side.png` | `Concept/Side.png` \| `u02-s4-side`, matched total height |
| `PLATE-front.png` | `Concept/Front.png` \| `u02-s4-front`, matched total height |
| `PLATE-eyes.png` | the sheet's eye pair \| the model's, matched body-bulb width |
| `side-3x.png`, `front-3x.png`, `tq-3x.png` | the three diagnostic views, 3x |
| `front-crop8x.png`, `tq-crop8x.png`, `side-crop8x.png` | the face at 8x |
| `startle-full.png` | eight full 384x240 frames of the shipped startle clip |
| `contact-unnamed02-startle.png`, `contact-unnamed02-curious.png` | every 4th/5th frame |
| `loop-clip.png` | startle frames 40–52 at 4x — the floating/clipping arm |

**Views used.** `u02-s4-side` (cam_yaw 0) and `u02-s4-front` (cam_yaw 0x4000),
the committed diagnostic stage. Caveat for the reader: that stage carries
`cam_ps ≈ 15°` of downward pitch, so it is not a true orthographic elevation and
the sheets are drawn dead-on. It is close enough for silhouette work; it is not
close enough to argue about a few percent, and I have not.

---

## 0. Verdict in one line

The creature currently reads as **a glossy pink balloon on a bent paperclip,
with no face**. The sheet reads as **a soft fleshy teardrop wearing a thick ring,
with two enormous purple eyes that are the entire character**. Almost every
fault below is one of those two sentences.

---

## 1. Inherited claims — verified or refuted

I re-derived each of these from the plates rather than accepting them.

| inherited claim | verdict |
| --- | --- |
| loop reads angular where the drawing is round | **CONFIRMED** |
| …and roughly a quarter too short | **REFUTED — it is the opposite** |
| front antenna is a uniform rod where the sheet tapers | **CONFIRMED**, and understated |
| body is a sphere where the sheet is a teardrop → "lollipop" | **CONFIRMED**, strongly |
| eyes wrong in shape, size, placement and colour | **CONFIRMED on all four, plus a fifth** |
| one dongle floats free and clips | **CONFIRMED**, but only under animation |

### The refutation, because it matters

**Do not lengthen the loop.** Measured like-for-like in the matched side views
(per-row silhouette widths, `u02-s4-side` against the `Side.png` tracing):

* sheet — loop outer width ≈ **0.72×** the body lobe's width; loop outer height
  ≈ **0.85×** the body lobe's height.
* model — loop outer width ≈ **1.3×** the body ball's width; loop outer height
  ≈ **1.4×** the body ball's height.

**The loop is roughly 1.7× too large relative to the body**, or equivalently the
body is ~40% too small for it. `PLATE-side.png` shows this without any number:
at matched total height the sheet's body is visibly the dominant mass and the
model's is a small bauble under an oversized ring.

This is the `CLAUDE.md` failure repeating verbatim — "we had moved the value
confidently in the wrong direction". Acting on "a quarter too short" would have
made the worst fault in the creature substantially worse.

**Corollary that changes the fix.** The loop's *tube gauge relative to the body*
is roughly right (sheet tube ≈ 7–9% of body width at the thin spans; the model
is in the same neighbourhood). The tube reads as **wire** because it is stretched
around a loop that is 1.7× too big. **Shrink the loop and the gauge reads
correctly.** Thickening the tube instead produces a fat wrong loop.

---

## 2. The faults, ranked by damage to the read

### 1 — THE CREATURE HAS NO FACE (worst by a wide margin)

`PLATE-eyes.png`. At matched body width:

* **Size.** Each sheet almond runs roughly the full depth of the body's lower
  half and its outer edge nearly touches the silhouette. The model's lens is
  about **a third of that length and under half the width** — two small pills.
* **The V is INVERTED.** The sheet's almonds converge at the **top**, at the
  body's centreline, and splay **down and out** — a Λ. The model's converge at
  the **bottom** — a V. A Λ reads soft, open, curious; a V reads pinched. The
  creature's whole disposition is upside down, and this is a sign flip.
* **Placement.** Model eyes sit too low — their bottom tips almost reach the
  ball's bottom ink — and too close together at the bottom. Sheet: they start
  high, near the body's widest point, and open outward.
* **Shape.** Sheet: a sharply **pointed almond** (a vesica), and the points are
  most of what makes it read as an eye. Model: a **blunt pill** with rounded
  ends.
* **Colour, and this is the direction's own bullet.** Sampling the model's eye,
  the single most common colour is **(96, 96, 96) — a neutral mid-grey.** Not
  white, not purple: no hue at all. Direction 2 says *"the whites are not white
  — they are a DEEP PURPLE."* They are grey. The lens purple that is there
  renders at (24, 0, 48)/(24, 0, 72), i.e. **near-black**, where the sheet's
  indigo is (72, 48, 120)/(96, 72, 144) — a readable mid-value purple. The model
  purple is roughly **three times too dark**.
* **The white ring is missing and the layout is inverted.** Sheet: purple is the
  dominant field (around the star and filling both points), a **thin white ring**
  traces the star, and the **cyan star is big**, nearly filling the ring. Model:
  a **grey slab** occupies the middle, the star is a thin cross sitting *on* the
  grey, and purple survives only as dark caps top and bottom.
* **The star is not a star.** Sheet: a fat, organic, slightly wonky four-point
  starfish. Model: a symmetric 2–3 px cross.
* **No ink on the lens.** The sheet outlines each almond in heavy black, which
  is what makes it separate from the pink at any size. The model's lens meets
  the body with no contour and dissolves.
* **They are invisible from the shipped camera.** In every showcase clip
  (`startle-full.png`, both contact sheets) both eyes collapse into one dark
  sliver on the right edge with two or three cyan pixels. Look at
  `PLATE-side.png`: the sheet's own **side** view makes the eye a dominant,
  fully legible feature with its star. Ours is a hairline.

### 2 — THE LOLLIPOP: a ball with a stick in it

`PLATE-front.png` and `PLATE-side.png`.

* **The body is a sphere.** `kBodyTaperPm` only pinches the top five rings and
  only to 660‰; below the equator it is a pure ball. The sheet is a **teardrop /
  fat comma** whose upper-left flank is broad and continuous.
* **There is a neck, and there must not be one.** On the sheet the loop's
  descending arm *is* the body's upper flank — about a third of the body's
  width, with no junction anywhere. The model plugs a ~1/8-width cylinder into
  the top of a ball at a right angle. This single discontinuity is the lollipop.
* **The front loses the base flare entirely.** The sheet's front antenna is
  **wide at the base (~45% of body width), tapering to a point (~10%)**, and it
  is *kinked and leaning*. The model is a **dead-straight, dead-vertical,
  constant-gauge rod (14–22 px against an 82 px body) with three beads on it.**
  `kLoopBladeRxMm`/`RzMm` are single constants for the whole tube, so no taper
  is even expressible today.
* **The sheet's front loop has a HOLE; the model's has none.** A real slot of
  white paper is visible through the sheet's blade. Ours is solid and symmetric.
  The sheet's asymmetry and lean are a large part of the creature's life.

**What is already right here:** the gross front proportion. Body bulb ≈ **46%**
of total height on both the sheet and the model. Do not disturb it.

### 3 — THE LOOP IS AN ANGULAR WIRE QUADRILATERAL

The bend happens at exactly four stations (root, hinge A, B, C), so the loop
is a **bent-paperclip polygon with four hard corners and straight legs**. The
sheet's ring is a **continuous soft curve** with only gentle undulation, and its
hole is a tall upright egg. The model's hole is a squat leaning quadrilateral
(outer W/H ≈ 1.05 against the sheet's 0.80), and the whole assembly **lies back
about 30°** where the sheet's stands nearly upright.

### 4 — IT DOES NOT MOVE

`contact-unnamed02-curious.png` is 36 frames in which nothing happens to the
body at all. Ink-bbox trajectory over the full clips:

* **curious, 180 frames:** body top swings **4 px**, bottom **2 px**, height
  **2 px**. That is a flat line. The `CLAUDE.md` note applies exactly: *a flat
  line IS "it never bobs".*
* **startle, 160 frames:** one hop of **21 px** taken over ~12 frames, then a
  **130-frame crawl** back to rest. That is a slow droop, not a startle — no
  overshoot, no recoil, no settle.
* **Height swing over the whole startle: 8 px.** The body never squashes.
  `kCompressAmpPm = 3300` (~5%) on a 78 px ball is ~4 px, and the toon band
  edge swallows it. Direction 1 wants it "slight but unmistakable"; Direction 2
  wants **more than Zixxtrixx**. It is currently below the noise floor.
* **The eyes never move once** across either clip — no blink, no squint, no
  widen, no gaze shift. The gaze knobs exist (`kGazeMaxA16`, `kSquintMaxA16`)
  and the clips barely touch them. For a creature with no nose and no mouth this
  is the whole expressiveness deficit.
* **"Curious" turns the face away.** In rows 3–4 of the contact sheet the body
  yaws so the eye sliver shrinks toward nothing. The clip named for looking at
  something looks away.
* Only the **antenna** has genuine life — it swings and lags convincingly. That
  is the one piece of good animation in the creature.

### 5 — THE FLOATING / CLIPPING ARM (confirmed, and only visible in motion)

`loop-clip.png`, startle frames 40–52. At frame 44 the loop's return arm swings
out and its **cut end hangs in mid-air off the body's right shoulder**, ink-capped,
reading exactly as the owner described — a dongle come loose. In the neighbouring
frames the same arm **passes straight through the body ball**, with the ink
outline drawn across the pink. The source comment is candid about it: *"the
return arm dives into the body."* The still diagnostic pose hides this entirely;
it is an animation-only fault.

Related artefact in the same plate: a **dark hairline runs down inside the body**
from the neck — the buried tube's contour being inked onto the body surface. It
reads as a crack in the creature.

### 6 — THE PINK IS SPLIT IN TWO, AND NEITHER HALF IS THE SHEET'S PINK

Sheet body pink: **(216, 72, 120)** / **(192, 48, 96)** — one strong crimson-rose,
essentially a single value across the whole drawing, matte.

Model: **lit band (240, 72, 168)** — brighter, hotter and more magenta than the
sheet — against **shadow band (72, 24, 48)/(96, 24, 48)** — a dark maroon at
roughly a third the sheet's value. The average is fine; the *read* is a
two-object creature.

Direction 2 asks for "darker, stronger pink — like the drawings". What happened
is the shadow got darker and the light got **paler**, and chroma dropped in both.
The fix is to **close the band spread** and land both bands near the sheet: a lit
band around (232, 88, 136) and a shadow around (168, 44, 92), never (72, 24, 48).

### 7 — IT READS AS SHINY PLASTIC, NOT CRAYON

* A **huge pale near-white specular blob** covers ~40% of the ball with a hot
  core (clearest in `startle-full.png` and `contact-unnamed02-curious.png`).
  The sheet has **no highlight at all**. This alone makes it a Christmas bauble.
* The **toon terminator is a lumpy polygon** that visibly follows the mesh rings
  and segments (`side-crop8x.png`). It reads as a chipped patch of paint stuck
  on the ball, not as light. Horizontal ring banding is visible across the body.
* The body's **white speckles read as dust or snow**, not as the sheet's strong
  directional crayon grain.

### 8 — IT DOES NOT FLOAT

The ball's bottom ink sits on the dirt line in every showcase frame. Hover
height 900 mm against a stretched body half-height of ~747 mm leaves ~150 mm —
one or two pixels at 240p. Direction 1's headline is **"IT FLOATS"**. There is
no visible air under it, no gap, no shadow separation. It reads as resting on
the ground.

### 9 — THE GAS RIM DOES NOT EXIST

Direction 2.1c says keep it and **thicken it a lot; it should be very visible**.
There is no fog/rim/gas geometry anywhere in `unnamed02_*.h`; the only related
thing is an FX corona splat. What is visible today is a one-or-two-pixel darker
fringe just inside the ink, which is shading, not a gas layer. This is new work,
not a knob turn.

### 10 — THE SHOWCASE CAMERA CROPS OFF THE BEST SHAPE

In `contact-unnamed02-curious.png` rows 3–4 the loop leaves frame entirely, and
in the startle it is cut at the top. The loop is the creature's most distinctive
feature and it is repeatedly out of shot; the creature also sits small and low
with a great deal of empty sky. (Cheap: framing constants.)

---

## 3. WHAT IS RIGHT — protect these

1. **The eyes standing proud of the surface.** Owner and artist both like it;
   it is a deliberate, sanctioned departure from the sheets, which cannot draw a
   form standing off a surface. **Keep the protrusion.** Only its *manner* needs
   care: today it reads as flat wafers standing on edge (`tq-crop8x.png`,
   `kEyeDeepMm 58` vs `kEyeWideMm 92`, plus 20° of outward yaw), and it should
   read as a lens swelling out of the body. Proud, yes; a stuck-on guitar pick, no.
2. **The black ink outline.** Thick, consistent, closes the silhouette, and it is
   the single strongest match to the sheets' heavy hand-drawn line. It is what
   makes the render read as the drawing at all. Do not thin it.
3. **The gross front proportion** — body bulb ≈ 46% of total height on both the
   sheet and the model. Any loop resize must preserve this.
4. **The flat blade construction** — broad in the loop plane, narrow across it.
   That is a correct reading of the two sheets together, and it is why the front
   and side silhouettes differ the way the sheets differ. Resize the loop; do not
   abandon the blade.
5. **The faceted eye lens is real geometry** (8 segments, and the facets do read
   at 240p). Direction 1's "partly polygonal" is achieved. Keep the facets.
6. **The star pupil is real geometry on its own bone**, and it renders cyan — the
   black-face bug from last pass is genuinely fixed. Keep the construction, grow
   the star.
7. **The antenna's secondary motion** — it swings, lags and settles convincingly.
   The best animation in the creature. Build the expressiveness on this.
8. **The hinge knuckles exist at the loop corners** and read as joints from the
   side. Right idea; they only look like beads-on-a-string from the front because
   the tube has no taper for them to sit in.
9. **The stage.** Dry brown ground under a warm sky reads the pink creature
   clearly — the Zixxtrixx lesson applied. Keep it.
10. **The lit path itself** (celmain, three-band toon + ink) is sound. The fault
    is the band *values* and the specular, not the path.
11. **The teardrop machinery already exists** — `kBodyTaperPm` and
    `kBodyLeanXMm` are per-ring owner knobs. The teardrop is a value change,
    not a rebuild.

---

## 4. For the architect — cheap vs. rebuild

### Cheap: existing named constants, one edit each

* **Loop size** — scale `kLoopArcMm[4]` to ~0.6× (or raise `kBodyRadiusMm`).
  This is the highest-value single edit in the creature and it also fixes the
  "wire" read for free.
* **Loop lean** — `kLoopFoldRootA16` toward upright.
* **Teardrop** — push `kBodyTaperPm` much harder and start the taper several
  rings lower; increase `kBodyLeanXMm`.
* **Eye V inversion** — flip the sign on `kEyeVAngleA16` / `kEyeTiltA16`. A sign.
* **Eye size** — `kEyeLongMm` 250 → ~380, `kEyeWideMm` 92 → ~150.
* **Eye placement** — `kEyeYMm` −70 → positive (raise them); cut
  `kEyeYawOutA16` so the lenses face forward instead of sideways.
* **Star size** — `kPupilStarArmMm` 78 → ~150, `kPupilStarWideMm` 30 → ~55.
* **Motion amplitude** — `kBobAmpAMm` 26 → ~90; `kCompressAmpPm` 3300 → ~9000.
* **Hover height** — `kHoverHeightMm` up until there is visible air.
* **Startle timing** — the return is 130 frames; it wants overshoot and settle.
* **Camera framing** — `cam_dist`/`cam_eye` so the whole loop is in shot.

### Medium: repaint or new per-ring arrays

* **The eye page** (`mku02page.py`) — purple field, **thin** white ring, **big**
  star. This is the largest single win for the creature's character.
* **Toon band values** — close the lit/shadow spread onto the sheet's pink and
  kill the pale specular blob.
* **Blade taper** — `kLoopBladeRxMm`/`RzMm` must become per-ring arrays, wide at
  the base, narrow at the tip.
* **Lens contour** — give the almond its own ink so it separates from the pink.
* **Eye animation** — the gaze/squint knobs exist; the clips need to use them.

### Rebuild: real new work

* **The loop's roundness.** Bending at four discrete stations *is* the
  quadrilateral. Making it round means distributing the fold across the rings —
  a per-ring fold curve, or many more stations. Rig and pose work.
* **The neck / teardrop continuity.** Removing the plug junction so the loop's
  base flares into the body is the lollipop fix and the biggest form job here.
  It probably wants the loop's lower rings to widen into a shoulder, or a new
  shoulder part.
* **The gas rim.** Does not exist. New translucent shell geometry or a
  render-side rim pass, plus a cost statement.
* **The buried-arm ink artefact** — the hairline drawn across the body by the
  buried tube end. Ink-pass level.
* **The clipping return arm** — needs the arm to terminate *inside* a body that
  actually meets it, which is the same job as the neck rebuild.

---

## 5. Where the sheet must NOT be copied

* **The sheet's single side-view eye** is the artist drawing a 3/4 read on a
  profile. Do not put one eye on the flank in 3D; put two on the front and make
  them large enough that one of them still reads in profile.
* **The eyes flush with the surface.** The sheet draws them flat because paper
  is flat. The model pokes them out and **that is protected** — artist-approved.
* **The front view's exact asymmetric kink** is one drawn pose, not a bind shape.
  It belongs in the rest pose, not baked into the mesh.
* **The paper's white** is not a colour in the creature; nothing should be
  matched to it.

---

## 6. Reproduce

```
bash tools/reel/build-direct.sh --output <dir> cel
ZIXX_EXP=celmain <dir>/bin/zhao-reel-cel.exe <out> \
    u02-s4-side u02-s4-front u02-s4-tq u02-s4-wire u02-s4-ids \
    unnamed02-startle unnamed02-curious
python tools/reel/rgbframe.py png <out>/u02-s4-front/0000.rgb front.png 3
```

Frames were read only through `tools/reel/rgbframe.py`. No new frame reader was
written. No source was edited; nothing was committed or published. No background
jobs were left running.
