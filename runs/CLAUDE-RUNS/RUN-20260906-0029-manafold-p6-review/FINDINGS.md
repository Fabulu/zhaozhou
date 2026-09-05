# FINDINGS — Manafold pass 6, the BY-EYE review

**Run:** `RUN-20260906-0029-manafold-p6-review`
**Reviewer:** by-eye gate for creature 02 (*Tribute Upheaval*), pass 6.
**Object judged:** the artefacts actually shipped and live at
`https://upheaval.pages.dev` — 22 clip webms, 22 posters and the 10-clip
experimental mana reel, all pulled from the live site, all at native 384x240.
**Authority:** my eye, against `Concept/Front.png`, `Concept/Side.png` and
`Concept/Description.png`, which are the authority on this creature.

---

## 0. READ THIS FIRST — the shipped build is not reachable from source

**Pass 6 was never pushed.** After an explicit `git fetch`, and by
`git ls-remote --heads origin` on both repositories:

| repo | `origin/main` | date |
|---|---|---|
| `zhaozhou` | `460490c` *Mana lab: the fold reads when it is SEPARATED…* | 2026-09-05 23:07 |
| `untitled-game` (Upheaval) | `1a02fd9` *Direction 6 RESULT…* | 2026-09-05 23:20 |

Neither repository has **any** branch carrying the pass-6 implementation, while
the live site plainly serves it (its card reads *"MANAFOLD, pass 6"*). So a
build was deployed to production from a working tree that exists on one machine
and nowhere else.

**Three consequences, all of which shape this report:**

1. **The shipped creature is currently unreproducible.** If that tree is lost,
   the pass is lost — the site is the only copy.
2. **No source-level claim in this report is verified.** Constants, CRCs, gate
   arithmetic and the `kEyeShiftMaxPm` question are all **INHERITED** (§6).
3. **This review is therefore of the shipped artefacts only** — which, for a
   by-eye gate, is the correct object anyway. Everything below is something a
   viewer of the site can see.

**This is the highest-priority action item in the pass and it is not an art
finding: push the pass-6 tree.**

---

## 1. Is the instrument honest? (checklist §A)

Settled **before** any colour judgement, in `instrument_check.py` (committed).

* The shipped posters are 1152x720 = exact **3x nearest-neighbour** blowups of
  native; every 3x3 block is constant, so decimating by 3 recovers the real
  pixels. **PASS.**
* An ffmpeg-extracted webm frame matches the decimated poster to
  **mean |delta| 0.93–1.64** across four clips — that residual is VP9 loss, not
  a reader fault.
* **The check can fail:** the same search against a channel-rotated poster
  returns **40.1–52.9**, i.e. 25–55x worse. A reader that rotated R/B — the
  fault that has shipped twice on this project — could not hide.
* Colour calls in this report are taken from the **lossless posters**; per-frame
  work uses webm frames, whose ~1.5 LSB of codec noise is far below any
  judgement made here.

### My own instrument lied first, and it lied the documented way

`bodytrack.py` v1 located the creature as *"saturated red-dominant pink"*. The
sky in these clips is a mauve-pink measuring **r-g=69, sat=69** at row 100
against my threshold of `sat > 70`. It swallowed the sky and reported `hasty`'s
creature as **8,455 px moving 38 px**, and **100% of frames edge-clipped in
every clip** — while a contact sheet showed a ~30x40 px creature crossing the
entire frame.

That is **exactly** the fault `PASS-6-INPUTS.md` §9 records from last pass
(*"a creature mask that matched the orange horizon band, claiming 15 of 16 clips
were edge-clipped in every frame"*). I reproduced it having read the warning. It
was caught by disbelieving the number against a plate, never by the tool. The
locator is now the **cel ink outline** (ink lum-sum 65–140, ground 187, sky
327–390 — all measured, not assumed) with a leak assertion that reports SUSPECT
rather than a number when the mask exceeds 25% of frame. **The v1 numbers are
retracted and appear nowhere below.**

---

## 2. WHAT IS RIGHT — the protected list

*Item 24: a gate that only finds faults gives the next pass nothing to protect.*
**Each of these is a quality a rebuild can easily lose. Do not rebuild over
them.**

1. **THE EYE'S CONSTRUCTION IS RIGHT, AND IT IS THE PASS'S BIGGEST WIN.**
   On `curious` and `hover`, at native, the eyes are two **symmetric lenses
   pointed at both ends**, tilted into a clean **Λ** with the tops converging —
   the Front sheet's own arrangement. The eyeball is a genuine **deep violet**,
   not white. The star is **turquoise-cyan, not navy blue** — the sheet's hue,
   not the owner's loose word. And the white **traces the star point for point**
   as one rigid unit; nothing in any frame shows the white and the cyan
   disagreeing, which is §5a's whole purpose. The teardrop is gone, the ring is
   gone. *(plate-02, plate-03 bottom row, plate-11)*
2. **The antenna is ONE CONTINUOUS SKIN — the beads are gone.** §2b's own
   acceptance sentence is *"if a viewer at native 384x240 can count discrete
   spheres, the item is not done."* At native I cannot. **Item met.**
   *(plate-09)*
3. **The free-floating dongle is resolved structurally.** The band re-enters the
   body at the lower right, as the Side sheet draws it. §1 delivered.
4. **The loop is closed around a real window** on the fixed-camera clips, and
   the mana lives inside it — the Side sheet's arrangement.
5. **`hasty`'s empty desert is FIXED.** 0 off-frame frames, against pass 5's 29.
   *(bodytrack, corrected)*
6. **`drift`'s edge-clipping is FIXED.** 0% of 300 frames, against pass 5's 70.
7. **The sideways shimmy is GONE from `hasty`**, replaced by a genuine
   traverse — centroid spans **304 px of 384**. §0-QUATER's main ask delivered.
8. **The smear's character is right and the owner was right to want it.** On
   `hasty` the trail separates cleanly from the travelling creature and has the
   chunky, blocky, hard-edged "broken frame buffer" quality, tear and all. The
   *mechanism* is a keeper; only its colour and its decay are wrong (§3.1, §3.9).
9. **The body pink is strong and saturated and holds form on the lit side** —
   it reads as the sheet's magenta-pink, with real sphere shading.
10. **The ink outline is stable** across every clip and every frame.
11. **`inspect`'s collapse into `hover` is HONESTLY CAPTIONED.** The card says
    so in its own words and keeps the tab as *"the honest record of a
    reversal"*. I verified the claim: **all 600 webm frames are pixel-identical,
    max |delta| = 0.** This is the right way to ship a reversal.
12. **THE MANA LAB'S HEADLINE CLAIM IS TRUE** — see §4. It is the most
    important result on this creature.

---

## 3. FAULTS, RANKED BY DAMAGE TO THE READ

Ranked by *how much each damages what a viewer sees*, not by how hard it is to
fix. Every one carries a frame reference and a plate.

### 1. THE MANA READS AS WHITE STEAM — on every shipping clip, `channel` worst

**Plates:** `plate-02-creature-3x.png`, `plate-09-antenna.png` (6x).
**Frames:** every frame of six clips scanned; `manafold-channel` f232 (its own
poster) is representative.

Look into the loop window on `rest`, `taunt` or `curious` at 6x: it is a cloud
of **soft white blobs** with a scatter of small crisp cyan crosses. The white is
the dominant element and the cyan motes read as countable little sparkle
sprites. It reads as **steam or confetti**, not as filled plasma.

Measured per frame — my definition, stated in `clipscan.py`, and deliberately
**not** compared against pass 5's number, whose definition lives in source this
lane cannot reach:

| clip | median % of mana pixels that are **hue-neutral** | median saturation |
|---|---|---|
| **`manafold-channel`** | **74.8%** (69–79) | 54.6 |
| `manafold-hasty` | 71.3% (52–**100**) | 49.4 |
| `manafold-taunt` | 60.1% (38–80) | 70.5 |
| `manafold-curious` | 57.2% (34–78) | 76.6 |
| `manafold-hover` | 56.1% (21–77) | 77.6 |
| `manafold-rest` | 55.4% (29–75) | 77.1 |

**`channel` is the worst clip in the bank.** That is the treatment the owner
personally chose, saw, and called *"the best mana particles you ever came up
with by far"* — and Direction 5 §0-BIS makes it the protected baseline and the
house look for every clip. Three quarters of its mana has no colour left.

**Why this ranks first:** the mana is the creature's name and its mechanic, it
is on every clip, and it is the item the owner spoke about at greatest length.
Direction 3 §6b's *"must be FILLED, solid saturated colour"* is not met.

**A pointer, not a proof:** the picker variants shipping on the same page
measure **4.0% white / sat 132** (`mana-blue`), **6.6% / 109** (`mana-boil`),
**7.6% / 108** (`mana-green`). ⚠ Those are different subjects and I did not
verify they were re-rendered this pass, so this is **not** a like-for-like
comparison. It does show that a filled, saturated mana is reachable on this
creature's own renderer.

### 2. THE NEAR EYE'S STAR IS A BAR ON MOST FRAMES OF MOST CLIPS

**Plates:** `plate-04-star-leash-12x.png` (12x), `plate-13-worst-eye-frames.png`
(the worst frame of each clip, sampled by badness, not by index).

On `rest` and `taunt` the near eye's star is **a long straight bar** — white
along one edge, cyan-teal along the other — lying across the lens and the pink
body with only a corner of purple still under it. At 12x it reads as **a chrome
trim strip or a scratch**. It is not recognisable as a star and it is not
recognisable as an eye.

**This is not an occasional bad frame. It is the normal state.** Star blob
major/minor axis ratio, every frame:

| clip | median | max | **frames worse than 4:1** |
|---|---|---|---|
| `manafold-taunt` | 5.66 | 9.5 | **96.1%** |
| `manafold-taunt2` | 5.79 | 13.1 | **78.3%** |
| `manafold-rest` | 6.63 | 19.7 | **73.8%** |
| `manafold-hover` | 4.20 | 14.2 | 51.7% |
| `manafold-trick` | 4.27 | 18.8 | 51.2% |
| `manafold-curious` | 3.40 | 6.0 | 26.7% |

For calibration: the frames where the eye **works** — the clean 4-point stars I
photographed at 12x on `curious` and `hover` — score **2.3–2.7**. The can-fail
control is in `evidence/probe-canfail.txt`; two earlier versions of this probe
scored good and bad frames alike and were thrown away.

**The far eye is the other half of the same fault.** On `rest` its star sits
hard against the black ink outline on a sliver of purple and has gone
**yellow-green** — emerald core, pale olive rim. It reads as a **badge stuck on
the creature's edge**, which is §5c's own phrase for the failure.

**On §5c specifically, stated precisely:**
* Rule 2 — *"the majority of the star stays on the purple, at least 60% of its
  area"* — is **VIOLATED** on both eyes of `rest` and `taunt`: the majority is
  on the pink.
* Rule 3, the hard one — *"the star never crosses the BODY outline"* — I could
  **NOT** show violated. At 12x the ink line stops it; I saw no star pixel out
  on the sky. **The leash's hardest rule holds; its middle rule does not.**

**Why this ranks second:** the owner said *"they're so important for its
personality"*, and this creature has **no mouth and no nose**
(`Description.png`: *"Sie besitzt weder Nase noch Mund"*). The eyes are the
entire face. On the majority of frames of most clips, the face is a scratch.

### 3. THE STAR IS ~HALF ITS DRAWN PROPORTION; THE WHITE IS TOO THIN AND TOO HOT

**Plate:** `plate-11-eyes-vs-sheet.png` — the Front sheet and shipped `hover`
scaled to the same eye-pair width, so this is like-for-like.

Even on the frames where the eye works, it does not read as a **star eye**:

* **The star spans ~35-40% of its lens's length; on the sheet it spans ~60-65%**
  and nearly the lens's width — a big, dominant feature that fills the eye.
  `kStarScalePm = 950` was meant to keep **95% of the drawn size**. The drawn
  *proportion* is not what shipped.
* **The white is a 1-px hairline** where the sheet draws a bold band roughly as
  thick as the star's own limbs. The sheet's eye is a three-value read —
  purple / white / cyan. Ours is purple with a thin bright edge.
* **And where it is visible it is BLOWN.** On `taunt` f156 the far eye's white
  rim is pure white and merges with the ink outline, so it reads as a
  **specular highlight**, not as the drawn white star. On the far eye of `rest`
  it is cream/tan/olive — not white at all.
* **The star's silhouette is a narrow DART** elongated along the lens axis, with
  two long limbs and two vestigial ones. The sheet's is a **fat 4-point star
  with concave curved edges and limbs of roughly equal length**.

Diagnosis worth carrying: the white is simultaneously **too thin** and **too
hot**. A broad, matte white band — as drawn — would both read as a star and stop
being mistaken for a glint. With the star's size, these are why the working eye
still reads as *a purple lens with a glint* rather than *a star eye*.
**This is the cheapest large win available to pass 7:** a few constants, no new
machinery.

⚠ Stated carefully: `hover` is not a true front view, so I make **no claim**
about lens proportion, which foreshortens. The claim is the star's size
**relative to its own lens**, which survives moderate foreshortening along the
lens's long axis.

### 4. THE FOG IS ABSENT where "much thicker, very visible" was ordered

**Plate:** `plate-10-fog-edge.png` — 14x on clean stretches of silhouette
against open sky, on four clips, deliberately away from the mana.

The edge is: body pink, then a **3-4 px hard black ink band**, then clean flat
sky. **There is no graded translucent band on either side of it**, at native or
at 14x. Direction 5 §3 ordered the gas *"thickened by a lot, so it's still see
through, but way less so — it should be very visible"*, and
`PASS-6-ARCHITECTURE.md` D.2 specified a 2-3 px band at roughly double the
opacity with its own knobs.

**This confirms the implementer's own declared gap, and it is the largest single
unmet owner instruction in the pass.** It ranks below the eyes and the mana only
because it is an absent feature rather than a wrong-looking one.

⚠ **Honest note on my own instrument:** I wrote a fog measure into
`clipscan.py` and **it is inconclusive**. It cannot separate a fog band from the
ink outline's own antialiasing and dither, so it returns 53-87% on every clip
including those with no fog. **It cannot fail, so I am not reporting its number
as evidence.** This verdict is by eye, from the plate.

**So I settled it with a 1D pixel profile instead, which CAN fail** — if a fog
band existed the profile would show a graded ramp, and it shows none.
`manafold-curious`, row 170, crossing the left silhouette (native pixels):

```
  x130-x132   sky      (189,105,90) / (189,109,99)   <- the RGB565 dither pair,
  x133-x136   INK      (25,24,25) / (25,24,16)          bit-identical right up
  x137        (173,61,99)  <- ONE transition pixel        to the line
  x138+       body     (156,45,82) and steady
```

**The sky is unchanged to the last pixel before the ink, and the body is at full
value one pixel after it.** There is no graded band on either side — not outside
the line, and not the *"gassy outside inside the black line before you get to
the real body"* the owner actually described. The fog is **absent**, not thin.

### 5. THE FOLD STILL DOES NOT READ — and the answer is already in the building

**Plate:** `plate-12-curious-everyframe.png` (all 180 frames).

Across every frame of `curious` the mana is the **same pale fuzzy cluster** at
the antenna. It never forms a shape, never holds one, never becomes another. I
can name no shape in any frame of any shipping clip. Direction 4's headline —
the creature's own *name* — is unmet for a third pass.

**But this is now a solved problem that was not integrated.** The mana lab
proved the mechanism the same night (§4 of this report), and Direction 6's own
RESULT says *"the fold work is no longer a search — it is an integration."* The
shipping pass did not take it. **That is the single most valuable thing pass 7
can do.**

### 6. EXPRESSIVENESS IS NOT ON SCREEN — §6 is largely unmet

**Plate:** `plate-12-curious-everyframe.png`.

`curious`, every frame: the creature is **essentially rigid**. What changes
across the clip is the **camera orbiting**, not the animal acting. No visible
squash, no stretch, no inhale or exhale, no playing with the hinges. Direction 5
§6 asks for *"bouncy, its body stretches, inhales, exhales, even moreso than
Zixxtrixx"* — an explicit instruction to **exceed** a creature whose motion
style is already codified. It is not there.

Centroid travel over whole clips (ink locator): `taunt` 10x35 px in 280 frames,
`taunt2` 16x25 in 240, `curious` 11x18 in 180, `rest` 18x18 in 400.
⚠ Caveat, stated: the ink+fill mask is bimodal on some clips (outline-only on
some frames, filled on others), which can move a centroid by a few px, so these
spans are indicative rather than exact. The contact sheet is not ambiguous.

**An instrument note that matters for pass 7:** on an **orbiting** camera, hinge
articulation cannot be separated from camera rotation, by eye or by tracking.
Direction 5 §2a's *"the hinges move up and down separately"* is therefore **NOT
JUDGEABLE from the shipped clips at all**, and I record it as **NOT ASSESSED**
rather than passed or failed. To gate it you need a **fixed** camera and a
per-hinge trajectory plot — which is what §2a itself asked for.

### 7. THE ANTENNA'S KNUCKLES WERE FLATTENED AWAY WITH THE BEADS

**Plate:** `plate-09-antenna.png` (6x), against `Concept/Side.png`.

The beads are gone, which is right and is protected (§2.2). But what replaced
them is **not four gentle knuckles** — it is an **angular, faceted band**: hard
corners, straight runs, a sharp V at the lower left of `rest`, a kinked zigzag
on `curious`. It reads as **bent sheet metal or a crushed straw**, where the
Side sheet draws gentle undulations in one flowing band.

§2b named this failure in advance: *"Do not flatten the antenna to a uniform
band. That would be as wrong as the current beads, in the other direction. Four
gentle knuckles in a continuous skin is the target."* **The pass landed on the
flattened side.** The band is also considerably thicker relative to its loop
window than the sheet draws — on the Side sheet the band is thin and the window
is large.

### 8. THE TRAVERSE CLIPS RENDER THE CREATURE FAR TOO SMALL

**Plates:** `plate-frames-manafold-hasty.png`, `plate-frames-manafold-fall.png`
(every frame), `plate-06-hasty-arc.png`.

On `hasty`, `drift` and `fall` the creature is roughly **25-40 px** in a 384x240
frame. At that size there are no eyes, no antenna, no knuckles and no fold — a
pink smudge inside a grey cloud. `PASS-6-INPUTS.md` §1 identified
pixels-per-creature as *"the rare knob with no downside term"*, and the
architecture took `240000 -> 360000` for fixed-camera clips; the traverse clips
kept their own framing and **did not get the benefit**. 340 frames of a 20-px
creature on `fall` is a clip that cannot show anything.

### 9. THE SMEAR PLATEAUS INTO A FULL-FRAME HAZE AND NEVER CLEARS

**Plate:** `plate-06-hasty-arc.png` (f1 to f240).

`hasty` f1-f30: a small, compact, punchy trail — genuinely good, and the thing
the owner praised. By f180-f236 it is a **grey-green band spanning most of the
frame** that never comes back down; at f236 the creature has left and the haze
remains. `hasty`'s mana white share reaches **100%** on its worst frames.

This is `PASS-6-INPUTS.md` §4's *"the smear plateaus into haze"* **unfixed**, and
it is fault 1 seen through time instead of through colour. The lab's own
stopping rule applies: too far is when the mana starts eating the animal.

### 10. `hasty` LEAVES FRAME AT THE LOOP SEAM

**Frames:** 234-237 of 240.

Four frames (2%) touch the frame edge as the creature exits bottom-right, then
it reappears at the left with no transition — a visible **pop** on every loop of
the webm. Against pass 5's 29 empty frames this is a large improvement and
mostly a polish item, but it is a seam a viewer sees every time round.

### 11. `hasty`'s VERTICAL BOB IS A DOWNWARD RAMP

The centroid spans 85 px vertically with 92 direction reversals, so oscillation
**is** present — but the dominant motion is a **steady sink** from y~110 to
y~194 while the creature grows. It reads as descending toward the viewer rather
than as §0-QUATER's *"bob up and down"*. The bob is there and is under-amplitude
against the ramp carrying it.

### 12. A THIRD OF THE ORBIT CLIPS IS A DULL MAUVE BACK

On `curious` the orbit carries the creature into its own shadow for a long
stretch: rows 9-12 of the every-frame sheet are a **dull purple blob with no
pink and no face**, and `hover` f230 (its own worst eye frame) shows the same.
`PASS-6-INPUTS.md` §5 raised this for `hover`; it is still true, and it is now
true on `curious` too.

### 13. The pink: mostly fixed, with `channel` the outlier

**Good news first — Direction 5 §4 is largely delivered.** Median clipped
fraction of lit pink is now **3.5-5.9%** on the fixed-camera clips, with darks
at **5.7-10%**. The body holds form at both ends and reads as the sheet's strong
magenta-pink. This belongs on the protected list.

**`channel` is the exception and it still swings within the one clip:**
**14.7% median clipped (max 37%)** and **27.4% median dark (max 54%)** — both
ends lose form, on the very clip that carries the house mana. `hasty` shows
43.5% median dark, but its creature is 30 px inside a smear, so that number is
about framing, not pigment.

### 14. Minor: the `inspect` poster is not what its caption says

The caption states the two clips are *"byte-for-byte the same"*. The **video**
is — all 600 frames pixel-identical, max |delta| = 0, verified. The **posters**
differ in 243,657 pixels, because they are different frame picks of the
identical clip. Harmless, and the caption's substance is honest; the word is
just wrong for the still.

**One real loss to note, though:** `inspect` was the only subject that raised a
*different presentation*. Nothing lost its lighting — every clip now has the
many-colour rig, which is §8 delivered — but the shipped bank is now **22 clips
of one rig, two of them identical**, with no diagnostic subject left that shows
the creature any other way.

---

## 4. THE MANA LAB'S HEADLINE CLAIM — JUDGED MYSELF, AND IT IS TRUE

**Plates:** `plate-07-edgesnap-survey.png`, `plate-08-edgesnap-native.png`.

The lab claims `edge-snap-held` holds *"a crisp aqua ring in mid-air — the first
nameable mana shape this creature has produced."*

**I confirm it, independently, at TRUE NATIVE 384x240 with no zoom.** At f281 a
**closed aqua ring hangs in mid-air** to the left of the travelling creature,
drawn as a bright cyan outline with a soft glow, clearly separated from the
animal. It reads as a ring without being told that it is one. It holds from
roughly f200 to f320 and decays to a grey ghost by f361.

**The control confirms the other half:** `control-channel` at the identical
frame, at the same native resolution, has only a sparkle cluster inside the loop
window. There is no findable shape.

**This is the most important result on this creature**, and Direction 6's RESULT
reads it correctly: separation makes a shape nameable, the edge makes it crisp.
Pass 7 should be built on it.

⚠ **One caveat the lab's own plates do not carry, and it matters for anyone
reading them:** the ten `manalab-*` clips render the **PASS-5 creature**, not
pass 6. Their eyes are the old purple teardrop with a **circular white ring**
around a cyan blob, and their antenna still has **countable balls** at the top —
both plainly visible in `plate-08` at 3x. The lab commit predates the pass-6
work. **So the lab reel is evidence about the MANA MECHANISM only**, and must
not be read as evidence about the pass-6 body, eyes or antenna — nor as a
regression in them. It is simply an older creature.

---

## 5. THE DECLARED GAPS — confirmed, and ranked

The implementer declared these honestly. My job was to confirm the damage.

| declared gap | confirmed? | rank | note |
|---|---|---|---|
| **Fog thinner where much thicker was asked** | **YES** | **4** | I can find no band at all, at native or at 14x. Correctly identified as the biggest declared gap. |
| **Clip inventory F.2-F.6 not done, §6 expressiveness largely unmet** | **YES** | **6** | Confirmed on the every-frame sheet: `curious` is a camera orbit around a rigid creature. |
| `kEyeShiftMaxPm` not shipped | **INHERITED** | — | Not checkable from artefacts; source not pushed. |
| One extremes gate reported-not-enforced (instrument returns 0 mm at zero roll) | **INHERITED** | — | But see fault 2: the composed extremes **do** produce a visible defect on screen, so the gate's absence is not academic. |
| `manafold-inspect` collapsed into `manafold-hover` | **YES, and handled well** | 14 | Honestly captioned, not silently shipped twice. Video identity verified over all 600 frames. Nothing lost its lighting. |

**On §5d, the eye roll:** I checked the **composed** extremes as instructed —
worst frames sampled by badness across six clips, not one channel at a time.
**The eyes never touch each other**: even where the lenses come closest at the
top, a dark wedge of body remains between them. **I found no frame where a lens
digs into the body.** The §5d prohibitions hold. Its *expressive* payoff I could
not see — I could not find a frame where a changed Λ angle read as a brow.

---

## 6. VERIFIED vs INHERITED

**Verified by me, from the shipped artefacts:**
* Reader honesty, with a can-fail control (§1).
* Every colour, shape and motion claim in §2 and §3.
* `hover` is pixel-identical to `inspect` over all 600 frames.
* The mana lab's ring claim at native, with its control.
* Media integrity: all 32 webms decode, all are 384x240, none is 0-byte, all 67
  files fetched at HTTP 200 with non-trivial size. Exactly one `noindex`, and
  its **content** reads `noindex, nofollow` — read, not merely counted.

**INHERITED — I could verify none of it, because pass 6 is not pushed:**
* Every constant, including `kStarScalePm`, `kStarOverhangMaxPm`,
  `kEyeShiftMaxPm`, `kEyeRollMaxDeg`, `kFogRimGainPm`, `kU02CamK`.
* All CRCs, including the `0xC8987099` identity claim. (I verified the *pixels*
  instead, which for that particular claim is stronger.)
* Zixxtrixx byte-identity and any other-creature regression check.
* The gate-off / revert path being byte-identical.
* Any posed-vertex probe, containment gate or extremes gate.
* Cost and budget arithmetic.

**NOT ASSESSED, with the reason:**
* **Direction 5 §2a, per-hinge independent motion with real range.** The shipped
  clips that show the antenna best use an **orbiting** camera, on which hinge
  articulation and camera rotation are not separable. This needs a fixed camera
  and a per-hinge trajectory plot. Recorded as unassessed rather than guessed —
  this item has already been misjudged twice.

**A stale claim on the shipped page, inherited from pass 4, not from this pass:**
the site header still describes the creature as having *"all-polygon teardrops
(pointy top, round bottom)"* whose *"white rings ride the pupil bone"*. That is
the pass-4 eye — the exact construction Direction 5 §5/§5a withdrew and this
pass correctly replaced. **The page's own header now contradicts the creature it
is showing.**

---

## 7. WHAT PASS 7 SHOULD DO, IN ORDER

1. **Push the pass-6 tree.** Everything else is downstream of the build existing
   somewhere other than one laptop.
2. **Integrate `edge-snap-held`.** It is proven, it is the creature's name, and
   Direction 6 already ruled it an integration rather than a search.
3. **Give the mana its colour back.** The target is not more particles; it is
   fewer hue-neutral ones. `channel` at 74.8% white is the number to move.
4. **Fix the star: bigger, a broad matte white band, and stop it presenting
   edge-on** at the shipping three-quarter angle. A few constants plus one
   orientation rule buy back the creature's whole face.
5. **Author the fog.** It is absent, not thin.
6. **Put the expressiveness on screen** — and judge it on a fixed camera, or it
   cannot be judged at all.
7. **Raise the traverse clips' camera.** `hasty`, `drift` and `fall` currently
   cannot show any of the above.

---

## 8. Housekeeping

* **Nothing was published. No creature constant was changed.** No file outside
  this run folder was written.
* Lane isolation held: `zhaozhou/`, `Upheaval/`, `manafold-p6-impl/`,
  `manafold-mana-lab/`, `manafold-p6-architect/`, `manafold-p6-recon-*/`,
  `manafold-pass5-*/` and `manafold-p6-qa/` were never touched. All work was
  done in `manafold-p6-review/`, cloned fresh from origin.
* **Background jobs: all of mine are verified stopped**, including one orphaned
  `eyescan.py` that survived its task being cancelled — CLAUDE.md's *"stopping
  an agent does not stop its background work"*, demonstrated again. One
  `python -m http.server` on port 4173 serving `C:/programmieren/Linoctissite/`
  is running and is **not mine**: it predates this session by two hours and was
  left alone.
* The 88 MB of downloaded media and the ~8,500 extracted frames are gitignored;
  `extract_all.sh` reproduces them from the live site in one command.

## 9. Instruments committed with this run

| file | what it is | can it fail? |
|---|---|---|
| `instrument_check.py` | proves the frame reader is honest against the shipped posters | yes — channel-rotation control, 25-55x separation |
| `eyescan.py` | ranks every frame by star elongation | yes — calibrated on eye-judged good and bad frames, 2.3-2.7 vs 6.3 |
| `clipscan.py` | per-frame mana white share, pink clip/dark | mana and pink yes; **fog measure NO — declared inconclusive and not used** |
| `bodytrack.py` | per-frame creature locator on the cel ink | yes — SUSPECT assertion at 25% frame coverage; v1's sky-matching failure kept in the docstring |
| `contact.py` | contact sheet of EVERY frame, never a sample | n/a |
| `manaread.py` | mana white share on the lossless posters | yes |
| `extract_all.sh` | rebuilds every frame from the live site | n/a |
