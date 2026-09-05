# Manafold pass 5 — the BY-EYE REVIEW

**Run:** RUN-20260905-1805-manafold-pass5-by-eye-review
**Reviewed:** zhaozhou `f9bf26bf`, Upheaval `78140d0` (both the expected heads)
**Baseline built and rendered myself:** zhaozhou `209ae66a` — the pass-5 commit
*after* the build fix and *before* all three visual commits, so the creature is
textured and the only difference is what pass 5 changed about the look.
**Status:** Complete. Nothing in the shipped tree was modified. Nothing published.

Every plate named below is in `plates/` beside this file.

---

## 0. Was the instrument honest? (gate checklist section A)

| check | result |
| --- | --- |
| Does the subject select the SHIPPING rig? | **Yes, and I read the builder, not the name.** `u02_common()` (`zhao_reel.cpp:4983`) sets `s.sun = &kU02SunCalm` for **every** u02 subject, the s4 form diagnostics included; each clip then overrides with its own named sun. Only `manafold-inspect` clears it and raises the four-light rig. The pass-3 trap is closed. |
| Rendered the shipping way? | `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`, set explicitly on every render. |
| Frames read with `rgbframe.py`? | Yes, and `python rgbframe.py selftest` -> **PASS** before anything was judged. |
| Calibrated against a known build? | Yes — I reproduced the pass's own headline numbers rather than inheriting them. |
| Judged at native 384x240 first? | Yes. Zoom only afterwards, and the gap between the two is itself a finding (section 5). |
| Can my own checks FAIL? | Every mask carries a synthetic selftest (body pink -> hit; horizon orange / sky / ground -> miss). Two of my own instruments failed those tests and were replaced — 0.2, 0.3. |

### 0.1 I hit the stale-binary trap, on myself

My first base build was `5c40593f`. It renders the creature **solid black** —
`u02-s4-front` has **77 unique colours**. That is the untextured-black bug
(`09-ENGINE-GOTCHAS.md` sections 0/7), and it independently confirms that pass 5's
commit `209ae66a` fixed a real, shipped-severity defect.

I re-pointed the worktree to `209ae66a` and rebuilt — and **the link failed**:
`ld: cannot open output file ... zhao-reel-cel.exe: Permission denied`, because a
render still held the exe. `tail -1` of the build log said `LD zhao-reel-cel`,
which reads exactly like success. Every base plate and number produced before I
noticed came from the black-creature binary and was thrown away. After a clean
rebuild, `209ae66a` renders `u02-s4-front` with **803 unique colours** — the exact
77-vs-803 pair commit `209ae66a` claims, reproduced rather than inherited.

### 0.2 My first badness metric measured the wrong THING (gate item 16)

I first counted near-white, **low-saturation** pixels as "smear fog". Base against
head read *5-6x more fog at head* — a damning number. The plate showed why it was
wrong: the pre-pass-5 cloud is **strongly cyan** (saturated, so the metric skipped
it) and the pass-5 cloud is **white-grey** (counted). The metric was measuring a
**hue change**, not coverage, and would have shipped a confident, backwards
conclusion. Replaced with a hue-independent mana mask (`B >= R` and
`max channel > 170`) that catches both clouds and excludes warm sky, warm ground
and pink body.

### 0.3 A framing sweep that was reading the horizon

My first creature mask (`r>150, r-g>95, b>80`) reported that fifteen of sixteen
clips touch both frame edges on every frame. It was matching the orange horizon
band `(214,113,82)`. Tightened to `r-g>110, b>95` and given a synthetic selftest;
the corrected sweep is 3.4.

### 0.4 And once my eye was wrong and the number was right

At 2x I read `trick` f200 as having **no ink outline at all**. An ink count over
the whole clip says the ink is present in every frame (min 1187 px against a 1291
median; no dropouts anywhere in any clip). A 4x zoom confirms the outline is
there — it is simply thin against a bright sky at that scale. Recorded because it
cuts the other way from the house lesson: the gate was right and the glance was
wrong. `plates/trick-f200-zoom.png`.

---

## 1. THE FIVE CLAIMS, one line each, judged by looking

| # | claim | verdict |
| --- | --- | --- |
| 1 | The mana no longer buries the antenna (taunt2 f80/f120/f166/f200, rest f335, damage f150) | **TRUE, and it is the pass's big visible win** — at f166 the base build is a solid cyan mass with no loop in it at all and the head build shows the whole tube, both limbs and every ball. Mana screen coverage is down **47-68% on every clip**. But the fault is *reduced, not removed*: the frames the pass named are not the clip's worst frames (3.3). |
| 1b | hasty f131's "lagging gap" is preserved | **TRUE.** The trail still lags with a clean gap behind the creature, and f131 is one of the best-reading frames in the bank. The trail is markedly thinner and greyer than base though — hasty took the largest coverage cut of any clip (-68%) and it was not a target. |
| 2 | Knead coverage: hit 0->36, startle 0->48, curious 18->51 | **The COUNTS are true — I reproduced 36 / 48 / 51 exactly from my own `U02_FOLD_DEBUG` renders. The READ is not.** It reads as a sparkle cloud drifting and redistributing, never as a shape being kneaded into another shape (3.2). |
| 3 | The eye's white: crescent -> near-complete ring (tube 15->22, offset 52->60) | **TRUE and clearly visible.** Base is a thin one-sided arc; head is a thick band wrapping most of the star, and at taunt2 f166 it is a complete oval ring. `plates/eyes-base-vs-head.png`. The near eye at three-quarter is still a sliver, but that is foreshortening, not this fix. |
| 4 | Hover loop seam 4.07 -> 1.89 | **TRUE, reproduced to three digits.** My own measurement: wrap delta **4.074 -> 1.890**, ratio **5.50 -> 2.41**. It generalises — rest 3.54->1.80, taunt2 4.15->2.26, curious 3.72->1.51. The difference image shows the residual is **entirely in the mana**; the body and loop wrap cleanly. `plates/hover-seam-diff.png`. |
| 5 | The damage clip runs its authored knead 250 (was dead), "visibly calmer at f150" | **The knob turns — verified in the diff** (`slot < 14` -> `slot < 15`, orphaning index 14, so damage really did run at 700). **Calmer at f150 — TRUE**: base has the loop's left half inside a cyan mass, head has a clean readable loop with the mana off to the side. |

---

## 2. WHAT IS RIGHT — the protected list

**Do not rebuild over any of these.** Each was verified by looking, in this run.

1. **The antenna reads through the mana now.** This is the headline and it is real
   and large. `plates/base-vs-head-CORRECT.png` is the evidence; do not trade it
   back.
2. **The hover loop wraps cleanly in POSE.** Body and loop are continuous across
   the seam; only the mote pattern still ticks.
3. **The white annulus is a ring.** Thicker, prouder, and it survives the shipping
   three-quarter angle on the far eye.
4. **The far eye is a genuinely good googly eye.** It pokes proud of the
   silhouette, catches a bright ring and a cyan star, and matches the description
   sheet's "abstehendes Auge" better than anything else on the creature. The
   artist-approved protrusion is intact.
5. **The antenna's gauge and joint reading is right.** Thin tube, thickest at the
   balls, a ball at the front junction and at the back, no tumour ball inside the
   tube. `plates/u02-s4-tq-2x.png`. The loop closes.
6. **The refutation holds.** The loop measures ~0.72x the body's width on the side
   sheet and the model now matches. Do not re-enlarge it.
7. **The ink outline is present and stable in every clip** — no dropouts, no
   distance anomaly, median ~1200-1500 px, min 1187 on the clip that comes
   closest to camera.
8. **The mana is depth-correct.** I hunted the Direction-4 bug at the worst frames
   of five clips and found no case of the smear drawing over the body.
9. **The `trick` headstand.** Real motion (centroid p2p 32 x 34 px, bbox height
   70 px), a genuine gesture, the best-animated clip in the bank.
10. **The build fixes itself.** A clean checkout now renders 803 colours instead of
    77, and a missing page is a hard error. Proven by building both.
11. **The first ~10 frames of every clip look excellent.** `rest` f15 and `taunt2`
    f2 are the target read: crisp cyan sparks inside a perfectly readable loop
    (`plates/smear-depth-check.png`). Whatever gets fixed next, those frames are
    the reference for what the mana should look like.

---

## 3. FAULTS, ranked by how much they damage the read

### 3.1 The mana lost its COLOUR to buy its legibility — worst damage
`plates/base-vs-head-CORRECT.png`, `plates/curious-f75-pocket-8x.png`

Pass 5 bought the antenna back by thinning the mana, and the mana went **white**
doing it. Over whole clips, mana saturation fell from **109-123 to 85-100** on
every clip except hasty. At native the mana now reads as **steam**, not filled
plasma: the visible mass is a 14-20 px near-white halo (`230,255,255`) and the
only saturated part is a 2 px cyan core.

Direction 3 section 6b is explicit — *"All the particles are far too transparent.
They must be FILLED. Solid, saturated colour... filled blues and greens."* That
requirement has moved backwards. **The next pass's job is to recover saturation
without recovering coverage** — they are separate knobs and pass 5 moved both.

### 3.2 The FOLD does not read as folding — the headline feature is not landing
`plates/curious-fold-segments-3x.png`, `plates/hit-knead-window.png`

Direction 4 section 0 is the pass's centrepiece: *fold the mana into recognisable
shapes, then knead it into new ones, continuously.* By eye, at native and at 3x:

* **No shape is nameable in any frame I looked at, in any clip.** Not a ring, not
  a star, not a bar. It is a cloud of sparkles.
* **There is one fold cycle per clip, not a loop.** `U02_FOLD_DEBUG` on `curious`:
  gather f0-52, hold f53-100, knead f101-151, release f152-179 — **one** shape
  transition (`3 -> 0`) in 180 frames. `hit` and `startle` the same. Only `damage`
  (464 frames) gets two. A viewer sees one shape change per clip; the direction
  asks for a loop that is *always* going on.
* The knead window *is* visibly more agitated than the hold window — the motes
  redistribute. It reads as **turbulence**, not as hands shaping something.

The counts pass 5 fixed are real and were worth fixing. The feature still is not
legible, and that is the parked item (section 5), not a pass-5 regression.

### 3.3 The smear reaches a steady-state haze and never comes back down
Measured, then looked at.

Near-white pixel count per frame, `taunt2`: f0 **126** -> f50 662 -> f130 **1101**,
and it stays between 700 and 1100 for the rest of the clip. `rest`: f0 **79** ->
plateau 500-690 for 300 frames. **The cleanest frames of every clip are its first
ten**, and the clip never returns to them.

Direction 3 section 6d was precise: *"never clears is too much... it does decay.
The frame clears eventually."* Per-mote it decays; in aggregate it saturates. The
only "clearing" is the loop wrap, which is a cut, not a decay. This is the whole
difference between `rest` f15 (excellent) and `rest` f124 (the loop's left limb
inside a white sheet).

The pass's named good frames are also not the clip's worst frames:
`plates/named-vs-worst.png` puts taunt2 f120 (named) beside taunt2 f125 (the
clip's worst), rest f335 beside rest f124, damage f150 beside damage f395. In
each pair the named frame reads and the worst frame still buries the loop's front
limb. Worst windows: taunt2 ~f105-130, rest ~f120-130 and ~f245-295, damage
~f390-415, curious ~f130-170.

### 3.4 `hasty` plays 29 frames of empty desert, and `drift` runs into the frame edge
`plates/hasty-gap.png` (the bottom-right tile is a shipped frame)

Corrected framing sweep, whole bank:

| clip | frames | creature ENTIRELY absent | clipped at left edge | clipped at right |
| --- | ---: | ---: | ---: | ---: |
| **hasty** | 240 | **29 (12%)** | 41 | 31 |
| drift | 300 | 0 | 70 (23%) | 0 |
| everything else | — | 0 | 0 | 0 |

`hasty` flies off the right of frame at ~f210 and does not come back until f238 —
on a looping webm that is a second of empty landscape and then a pop back in from
the left. Same fault class pass 4 fixed for `fall` ("190 of 340 frames showed
empty sky"); live on `hasty` today.

### 3.5 The near eye has no visible pupil at the angle every clip ships at
`plates/rest-blink.png` (6x), `plates/taunt2-named-native.png` (native)

At the three-quarter camera the two eyes read completely differently: the far eye
is a round bug-eye with a bright ring and a star; the **near eye is a long purple
slash with a thin grey sliver in it and no readable star at native.** The
creature's face is one eye plus a dark mark.

Worse, the squint that stands in for a blink (`kSquintMaxA16` 9000 ~ 49 deg, at
870 pm ~ 43 deg of eye YAW, every 192 frames for 10 frames) **rotates the star and
ring out of the near lens entirely** — for those ten frames that eye is a blank
purple almond. It reads as the eye rolling back, not as a blink. Direction 3 asked
for *"blink open and closed"*; nothing in the bank ever closes an eye (measured:
visible lens area varies 25%, never collapses).

### 3.6 The eyes still vanish from the side — raised in Direction 3, still open
`plates/u02-s4-side-2x.png` against `plates/concept-side.png`

The side sheet draws the eye **large, front and centre on the profile** — roughly
45% of the body's height, purple lens, white ring, cyan star, fully legible. The
model's side view shows a thin purple fingernail on the silhouette edge and no
star at all. Direction 3 section 2: *"They must read from the SIDE too, as the
side sheet shows. Right now they effectively vanish off-axis. More 3D."*
Unchanged. The cause is geometric — an outward V of only `kEyeVAngleA16` 2600
(~14 deg) keeps both eyes on the front of the ball.

### 3.7 The see-through gas rim / mist is GONE, and it was asked for twice
Searched the whole source: **no mist, rim, haze or gas constant exists anywhere**
in `manafold_art.h`, `manafold_model.h`, `manafold_fx.h` or
`tools/pack/mkmanafoldpage.py`, and none is visible on any render.

The only outer halo in the source is the belly glow's, and its own comment says
`kCentreGlowRadiusPx = 46; // OUTER halo: rims the ~32 px body` sitting beside
`kCentreGlowCorePx = 13; // INNER core: shines THROUGH the body`. Both are
switched off by the single knob `kBellyGlowGainPm = 0`, which pass 4 zeroed to
satisfy Direction 4 section 4 (*"the glowy bit inside the creature: make it go
away"*).

**One knob served two requirements that point opposite ways.** Direction 2
section 1c called the gas rim *"a good accident. Do not remove it... thicken the
fog by a lot... it should be very visible"*, and Direction 3 section 5 asked for
it back see-through. Removing the interior glow took the rim with it. This wants
splitting into two knobs, not re-tuning one.

### 3.8 Both taunts are almost motionless
Committed `trajplot.py` (its own selftest run first: PASS, the mask can fail),
with creature-free background plates from `ZIXX_HIDE_CREATURE=1`:

| clip | frames | centroid x p2p | centroid y p2p | bbox height p2p |
| --- | ---: | ---: | ---: | ---: |
| **taunt** | 280 | **3.5 px** | 18.6 | 13 |
| **taunt2** | 240 | **5.8 px** | 10.0 | 15 |
| rest | 400 | 11.3 | 5.2 | 11 |
| trick | 400 | 32.0 | 34.5 | 70 |
| drift | 300 | 314.4 | 93.8 | 55 |

**Both taunts move the creature less horizontally than `rest` does.** A taunt that
travels 3.5 px in nine seconds is a bob with a lean, not a gesture. The 240-frame
contact sheets of `taunt2` (`plates/sheet-taunt2-a.png`, `-b.png`) show an
essentially frozen silhouette for the whole clip. Direction 3 section 7:
*"Taunt — can be more fun."* Not addressed.

### 3.9 A third of `hover` has no face and no pink
`plates/dark-frames.png`

`hover` orbits, and for ~126 of 600 frames (f283-f436) the camera is behind the
creature: no eyes, and the body drops to a dull mauve — 126 frames carry under
200 saturated-pink pixels. On the bestiary's primary idle clip, a third of the run
is a featureless purple blob. `inspect` is worse by this measure (196 of 600
frames, min **2 px** of pink at f515) because the four coloured lights re-hue the
body away from pink entirely; several of its frames, f0 included, are close to a
black silhouette.

### 3.10 The front silhouette is a ball, not a teardrop
`plates/u02-s4-front-2x.png` against `plates/concept-front.png`

From the front the body is a near-circle with a small nub. The sheet's body is an
onion: widest low, concave shoulders, drawn to a point at the neck. Direction 3
section 4 asked for *"a bit more tear shape"*. It reads more teardrop **upside
down** in the headstand than it does standing. Also, from the front the eyes are
smaller and higher than the sheet's, which places them low and large enough to
dominate the face; and the four-point star reads as a cyan blob, not a star, at
2x — at native it is a dot.

---

## 4. VERIFIED vs INHERITED

**Verified myself, in this run, from my own builds and renders:** all five claims
above; the 77/803 colour pair; the seam numbers 4.074/1.890 and ratio 5.50/2.41;
knead frame counts 36/48/51; the `slot < 14` off-by-one (read in the diff); mana
coverage and saturation deltas on eight clips against a base I built; the framing
sweep; the trajectory numbers; the ink-outline sweep; the absence of any mist
constant; the eye ring, side-view and blink reads.

**Inherited — I did NOT confirm these and the next reader should not assume them:**

* **Cost.** The ~5.2% / 12% / 3-3.5% figures are arithmetic, not measurement.
  There are no fragment counters; I did not attempt them.
* **Byte-identity of Zixxtrixx and the archive generations.** I never built the
  pristine Zixxtrixx baseline. That is the QA agent's lane.
* **The posed-vertex clearance probe** and the closure-rim gate (1071 pm at slot
  11 key 69). I did not run `manafold-probe`.
* **The `U02_FOLD_FREEZE=1` coupling proof.** I read the gate and find the design
  argument sound (fixed barycentric weights, no proximity term anywhere) but I did
  not render the freeze variant.
* **Site state** — ordering, the single `noindex`, the media manifest, the encoded
  webms. Untouched and unexamined.
* **The trio / multi-conduit path.** Not rendered.
* **`channel`, `crackle`, and the `mana-*` picker variants.** Rendered but only
  spot-checked, not judged closely.

---

## 5. THE PARKED ITEM — evidence for the owner, nothing changed

`plates/parked-item-variants-native.png` (the honest read) and
`plates/parked-item-variants-3x.png` (to see what is happening).

**No shipped constant was touched.** The variants were built in a throwaway
worktree (`zhaozhou-exp`, never committed) that exposes three knobs and the
subject camera as environment overrides. All seven show the **same fold moment**,
`curious` f75, a HOLD frame — the moment a shape is supposed to be nameable.

| variant | what it costs, concretely |
| --- | --- |
| **A — shipping** (halo 7-10 px, 24 motes, stencil 300) | Baseline. At native: a small white smudge over the loop. Mana 1515 px, saturation 92.7. |
| **F — bigger motes** (halo 11-15), *the literal owner ask, said twice* | **The best-looking mana of the seven at native by a wide margin** — a distinctly cyan, luminous mass. Mana 2362 px, **saturation 114.1**. Cost: it fuses into one cloud and buries the loop's left limb completely — exactly the fault pass 5 just fixed. |
| **B — smaller motes** (halo 4-6) | At 3x a ring becomes visible. **At native it buys nothing**: mana falls to 1378 px and saturation to 83.3, so the mana becomes a fainter, whiter smudge and the shape is *still* not nameable. |
| **E — smaller and more** (halo 4-6, 40 motes) | The ring fattens back into a band. Worst saturation of all (71.8). Strictly worse than B. |
| **C — bigger stencil** (300 -> 470 mm) | The shape grows and a ring is readable at 3x, but the mana now sits over and outside the loop's limb — it breaks Direction 3 section 6c, *"the mana is always in the middle of the circle and stays there."* |
| **D — closer camera** (`cam_k` 240k -> 360k) | **The only variant that gains on both axes at once.** Individual motes read as distinct cyan sparks AND the ring reads AND the loop stays legible. Mana 2145 px, **saturation 117.2 — higher than the bigger-mote variant** — with every mana knob left exactly as shipped. The cost is framing, not the effect: the creature fills the frame, so it no longer matches Zixxtrixx's clip scale, and travelling clips (`drift` already covers 314 px, `hasty` already leaves frame) would need their own camera. |
| **G — combo** (halo 4-6, 34 motes, stencil 430) | A white ring band. Middling on every axis. |

**The one thing worth telling the owner.** The parked trade-off has been framed as
*"bigger particles"* against *"nameable shapes"*. At native 384x240 that framing is
not quite right: **shrinking the motes does not buy a nameable shape at native — it
only buys one at zoom, while losing the particles.** The axis that actually buys
both is **pixels per creature**. Variant D changes no mana constant at all and gets
more mana, more saturated mana, *and* a readable ring, purely by putting more
pixels on the creature. If the owner wants both of the things he has asked for,
the honest lever is the creature's size on screen — a camera decision, or a bigger
creature — not the mote radius.

---

## 6. What I would fix first, in order

1. **Get the colour back into the mana without getting the coverage back.**
   Saturation and coverage are separate knobs; pass 5 moved both. Target the read
   of `rest` f15 / `taunt2` f2, sustained across the clip.
2. **Cap the smear's steady state** so frame 120 looks like frame 10. Today the
   plateau is 5-10x the clean level and never returns.
3. **Put the owner's decision on the parked item in front of him** with section
   5's plates; nothing else about the mana should move until that is settled.
4. **`hasty`'s 29 empty frames and `drift`'s 70 clipped ones** — a staging fix,
   cheap, and it is a shipped clip on the site.
5. **The near eye at three-quarter** — it carries no pupil at native, and the
   squint rotates it out entirely. This is the creature's whole face.
6. **Split the belly-glow knob in two** so the see-through gas rim can come back
   without the interior glow (3.7).
7. **Give the taunts a gesture.** 3.5 px of travel in nine seconds.
8. **More than one fold cycle per clip**, if the loop is meant to be continuous.

---

## Files examined

`Upheaval/creature/10-GATE-CHECKLIST.md`, `07-MOTION-STYLE.md`, `08-LIGHTING.md`,
`09-ENGINE-GOTCHAS.md`, `CLAUDE.md`; `Upheaval/creature/Manafold/MANAFOLD-INDEX.md`
and all four `OWNER-DIRECTION-*.md`; `Concept/Front.png`, `Side.png`,
`Description.png`; `zhaozhou/tools/reel/zhao_reel.cpp` (subject builders, env
gates), `manafold_art.h`, `manafold_clips.h`, `manafold_fx.h`, `manafold.h`,
`build-direct.sh`, `rgbframe.py`, `trajplot.py`;
`zhaozhou/tools/pack/mkmanafoldpage.py`; the diffs of `209ae66a`, `0514adab`,
`bbbc5e5c`, `fa0f572d`.

## Reproduction

    git clone <zhaozhou> && git checkout f9bf26bf          # head
    git worktree add ../zhaozhou-base 209ae66a             # the baseline
    bash tools/reel/build-direct.sh --output <dir> cel     # both, CLEAN
    ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross <dir>/bin/zhao-reel-cel.exe <out> manafold-<clip>
    U02_FOLD_DEBUG=1 ...                                   # the fold telemetry
    ZIXX_HIDE_CREATURE=1 ...                               # the trajplot background plates
    python zhaozhou/tools/reel/trajplot.py --bg bg/<clip> traj renders/<clip>

Check the exe's mtime after every build. `tail` of the log is not the exit code.
