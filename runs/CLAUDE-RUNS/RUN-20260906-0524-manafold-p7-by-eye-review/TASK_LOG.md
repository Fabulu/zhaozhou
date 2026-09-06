# Task Log: RUN-20260906-0524 - [Describe objective here]

**Created:** 2026-09-06 05:24 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0524-manafold-p7-by-eye-review/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 05:24 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0524
- Created working directory
- Initial context: [brief description]

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

---

## Pass 7 BY-EYE REVIEW log

### Lane and calibration
- Own lane `manafold-p7-review/{zhaozhou,Upheaval}`, cloned then repointed at
  GitHub and hard-reset to `origin/main`: **zhaozhou `6f840909`, Upheaval
  `097ffbe`** -- the exact published tips, confirmed by `git fetch`.
- Built my own binary: `bash tools/reel/build-direct.sh --output <lane>/build cel`,
  **BUILD_RC=0 read from the recorded exit code**, not from a pipeline tail.
  `zhao-reel-cel.exe` 2,739,759 B -- the same size the publish run recorded.
- Judging the SHIPPED artefacts (the committed webms that production serves,
  `cmp`-verified by the publish run against production) at native 384x240.

### Instruments written, and PROVED FAILABLE before use
1. **`tools/reel/webm2rgb.py` -- COMMITTED this time.** The publish run wrote
   this and left it in its run folder, where CLAUDE.md says durable things get
   orphaned; it was gone. Selftest: rejects a 2 kB stub (the documented
   `ffprobe` hole), round-trips red as (254,0,0) and blue as (0,0,255) so a
   channel-rotating decoder cannot pass, and distinguishes the two.
2. **`plates.py crop`** -- added to the committed plate builder rather than
   hand-rolled. Out-of-range boxes RAISE; a silently clamped crop would move
   what is measured between frames (gate item 17). Proved: rejects 370,0,40,40
   on a 384-wide frame.
3. **`eyesheet.py`** -- per-frame eye locator + contact sheet. Selftest proves
   it does NOT match the mauve sky (the project's recorded mask fault), DOES
   find a planted lens at the right coordinates, and returns *no centre* rather
   than a default on a creature-less frame; misses are drawn MAGENTA and
   counted, never silently centred.

### Frame counts cross-check
Decoded clip lengths match the publish run's table exactly (hover/inspect 600,
channel 420, taunt 280, rest 400, curious 180, pirouette 240, startle 160,
taunt2 240), which independently re-confirms "all 16 clip lengths identical".

### In progress (written down BEFORE reading the antenna render)
Was: judging Direction 5 §2a by eye on `manafold-antenna-fixed`, rendered from
my own build. Next after that: the smear-plane colour attribution
(`manafold-rest` minus `manafold-fogprobe-mana` = the smear's own pixels, the
one thing the committed ablation pair CANNOT see because it has the smear off
on both sides), then eyes-touch/clip by eye, then write up.

### Findings so far (each stated as verified-by-eye or measured)
* Claim 1 star-reads-as-star: CONFIRMED at native, strongest on `curious`.
* Claim 2 `channel`'s white spike: CONFIRMED GONE at native, f231.
* Claim 4 smear on all clips: CONFIRMED by 8-clip A/B against the pass-6 archive.
* Claim 7 black notches: measured INSIDE each eye, per named part -- ~0.1 px
  per frame in both passes. Not a read fault. My first metric said the opposite
  because a dilated lens mask swallowed the body's own ink outline.
* Mana: the "white steam" verdict does NOT survive re-measurement (below).

---

# FINDINGS -- Manafold pass 7, the by-eye review

(The harness refuses a separate FINDINGS.md, exactly as the pass-7 impl run
recorded. They live here.)

## THE VERDICT

**This creature is good.** Not "a list of fixes with some wins" -- good, with
four faults, one of which is serious.

Pass 6 shipped an animal whose face was two pale scratches and whose mana was
white dust. Pass 7 ships an animal with **two turquoise four-point stars on
deep-violet lenses in a clean lambda**, a **closed antenna loop with a real mana
window**, an aqua fog that reads as gas, and the single ugliest artefact on the
old bank -- the white spike off channel's far eye -- gone. On `curious` this
creature is a genuine likeness of the Front sheet, at native, in motion.

The one thing standing between it and finished is **the antenna's surface**.

## Lane and calibration

Own lane, hard-reset to the published tips: zhaozhou 6f840909, Upheaval
097ffbe, both confirmed against the remote by git fetch BEFORE anything was
read. Built the reel myself (tools/reel/build-direct.sh, BUILD_RC=0 read from
the recorded exit code; binary 2,739,759 B, the size the publish run recorded)
and rendered the committed diagnostic subjects from it. Everything below is
either something I looked at or something I measured from my own render or from
the shipped webms. Judged at native 384x240 on the shipping rig -- the Manafold
clip subject raises creature_moving_light, so nothing here was judged under a
diagnostic light.

Decoded clip lengths match the publish run's table exactly, which independently
re-confirms "all 16 clip lengths identical".

## The seven claims

1. The star reads as a STAR -- CONFIRMED at native. Strongest on `curious`
   (180/180 frames), `hover`, `startle`, `damage`. Qualified: the FAR eye is a
   clean compact 4-point star; the NEAR eye at oblique angles is still a bar.
2. channel's white spike -- CONFIRMED GONE, f231, at native. Pass 6 shoots a
   long white spike past the outline into the sky; pass 7 has a compact star at
   the rim. The largest single visible win of the pass.
3. The fog reads as GAS -- CONFIRMED on the night clips, where it is a vivid
   aqua glitch-cloud (`crackle` is beautiful). QUALIFIED on the day clips, where
   it reads khaki-grey -- see fault 3.
4. The smear runs on all clips -- CONFIRMED by A/B against the pass-6 archive on
   eight clips. `crackle`, `curious`, `damage`, `fall`, `hit`, `startle` all
   gain a smear they did not have; `drift` and `hasty` are unchanged, exactly as
   declared (they already had it).
5. The eyes never touch, never clip -- CONFIRMED by eye over 500+ frames across
   five clips: the two lenses never merge, never intersect, and the star pops
   proud of a curved body the way the inset drawing shows. The 3-D authority
   remains the committed probe (gate A 22 mm / floor 12), which the publish run
   reproduced from its own build.
6. The antenna is one continuous surface -- CONFIRMED across a full 240-frame
   pirouette. No countable spheres, no separation, no dongle, and the loop stays
   closed around a real window. Section 2b's acceptance sentence still passes.
7. The black notches -- REFUTED AS A READ FAULT; they are gone. Measured inside
   each eye's own FILLED INTERIOR, per named part: 0.1-0.2 dark px per frame out
   of a ~900 px eye, in pass 6 AND pass 7. Confirmed by eye at 12x. This item
   can come off the list.

## The one the record owed: Section 2a, judged by eye

Rendered manafold-antenna-fixed from my own build -- camera and body root both
still, only the hinges moving -- and looked at 60 frames across the clip.

It reads as playing with its hinges. 2a passes BY EYE, not only by correlation.
The band folds into a tight Z, opens into a low hook, closes to a compact elbow
and rises again; the kinks appear at distinct stations and travel along the
band. On a creature with no mouth and no nose that is carrying real weight.
But it is playing with the wrong surface -- fault 1.

## WHAT IS RIGHT -- the protected list

1. The eye construction. Symmetric pointed lenses tilted into a lambda with the
   tops converging; deep-violet eyeball; TURQUOISE, not navy; a compact
   four-point star with concave arms. `curious` is the reference -- 180 of 180
   frames carry two legible stars. A real likeness of the Front sheet, and the
   best thing in the bank.
2. The star's new proportion. 2.82 -> 1.69 against the sheet's drawn 1.70. At
   native this is the difference between a scratch and a star. Do not undo it.
3. The silhouette. A saturated magenta onion, widest low, narrowing into a
   hooked band under a thick chunky black ink contour. It reads as the drawing
   from across the room.
4. The closed loop and its window, verified through a full turn. Body-sized, and
   the mana lives in it -- exactly what the Side sheet and the owner say. (My
   own first read called the loop solid; it is not. The window reads grey
   because the fog FILLS it, which is correct.)
5. The smear plane's colour where it works. On `crackle` and `channel` it is a
   brilliant chunky aqua: both the "glitchy broken frame buffer" the owner asked
   for and the teal the sheet asks for. Measured on the pixels the smear ALONE
   paints, where it dominates: 0.7% hue-neutral, mean RGB (133,223,224).
6. The captions. The `taunt` caption says the cross-eyed beat DOES NOT READ, in
   those words, on the live page. I checked the beat -- the near eye is a pale
   bar on essentially every frame -- and the caption is HONEST. So is
   `inspect`'s byte-identity note and `trick`'s -25 mm contact. I found no
   caption that flatters.
7. The black notches are actually fixed -- worth protecting because it is the
   project's own recorded ghost.

## FAULTS, RANKED BY DAMAGE TO THE READ

### 1. The antenna is a UNIFORM STRAP with MITRED CORNERS (worst)

On the fixed-camera diagnostic at 4x, the antenna is a constant-width ribbon
turning hard, almost right-angled corners. No taper, no knuckles anywhere. It
reads as a bent length of ribbon cable.

The Side sheet draws four rounded swellings integrated into a continuous band,
with concentric detail marking them as joints. Section 2b wrote the warning in
advance, in as many words: "Do not flatten the antenna to a uniform band. That
would be as wrong as the current beads, in the other direction." That is exactly
what shipped.

Why this outranks every eye item: the antenna is the creature's most distinctive
silhouette feature, the EMITTER the Description sheet names, and where the mana
lives; with no mouth and no nose it is one of only two expressive parts; and it
is THROWING AWAY 2a's OWN WIN -- the hinges move well, but with nothing AT the
joint stations the folds read as creases in cardboard rather than knuckles in a
limb. Section 2b's surface is cancelling 2a's performance.

Plates: 15-antenna-fixed.png, 16-antenna-facets.png.

### 2. The mana's CORES are still white steam -- and the pass's own metric cannot see it

Pass 6's #1 fault. Pass 7 reports 8.3% -> 7.9% hue-neutral. That number is taken
over EVERY BRIGHT PIXEL IN THE FRAME -- a population dominated by the pink sky
and the pink body, in which the mana is a small minority. It is structurally
incapable of detecting this fault.

I rendered the committed ablation pair from my own binary and attributed the
pixels properly. Validity check first, because the two subjects carry different
names and a name can seed things: the creature-free corner of the frame is
BYTE-IDENTICAL on 20/20 sampled frames, so the pair differs only in the effect.

Restricting to pixels where the effect, not the background, is most of the pixel
(sum of |dR|+|dG|+|dB| >= 250):

    MOTES        48.7% hue-neutral | mean sat  39 | mean RGB (215,229,236) grey-white
    SMEAR PLANE   0.7% hue-neutral | mean sat 104 | mean RGB (133,223,224) clean aqua

Half of the mote system's own core pixels are literally colourless. That is
white steam, and it matches the pass-6 review's 55-75% far better than 7.9%.
Confirmed by eye on the motes-only ablation render: the loop window fills with a
cloud of WHITE BLOBS with a thin aqua rim.

The pass-7 lane's DIAGNOSIS is right and its MEASUREMENT is wrong. It correctly
named the cause ("the opaque corona hearts sampling each ramp's HI end by
construction") and correctly credited the fog with rescuing the read. It just
under-reported how much is left.

INSTRUMENT NOTE FOR PASS 8: mana_hue_probe.py's honest mode is diffpair -- but
as committed, the fogprobe pair has THE SMEAR OFF ON BOTH SIDES, so that mode
can only ever see the motes and never the smear, which was the largest visual
change in the pass. rest minus fogprobe-mana gives the smear alone; I used
exactly that above. Stratify by how much the effect contributes to the pixel, or
the background does the talking. (My own first cut of this reported the motes as
87% "warm-dominant" -- that was the pink background showing through a faint
halo, not the mana. Refuted before it was believed.)

Plate: 17-motes-vs-smear.png (bare creature / motes only / as shipped).

### 3. The smear takes its COLOUR FROM WHAT IS BEHIND IT

The same plane that is glorious aqua on `crackle` and `channel` paints flat
khaki-grey rectangles on the daylight clips, because there it is smearing peach
sky and pink body. On `rest`, `hover` and `damage` these read as dirt or
compression blocks sitting ON the creature, not as glowing gas -- and on `drift`
and `hasty` the grey cloud is wider than the animal.

One mechanism producing the pass's best-looking and worst-looking pixels, purely
as a function of the sky behind it. The fix is a colour floor on the smear, not
less smear.

Plate: 13-smear-block-zoom.png (crackle aqua vs damage khaki).

### 4. Section 5c rule 3 is VISIBLY VIOLATED -- the star still breaks the outline

Declared REPORTED-NOT-ENFORCED, with the pass adding that "its worst VISIBLE
manifestation -- the white spike into the sky -- is gone; I looked."

The spike is gone. The fault is not. Ranking frames by badness and then LOOKING
at them, the far eye's star arm crosses the black ink contour and sits out in
the sky -- plainly, at native -- on `channel` f224, `rest` f248 and `crackle`
f472, among others. Far less ugly than pass 6's spike, but the thick ink outline
is one of the strongest things in this creature's style, and a star point
hanging outside it reads as a rendering error.

The publish run's by-eye check looked for the SPIKE and found it gone; it did
not look for the smaller crossing.

Plate: 19-rule3-rim-zoom.png.

### 5. Declared, and confirmed -- no new information

* THE NEAR EYE IS A BAR at oblique angles. Confirmed on all 280 frames of
  `taunt`: the far eye carries a clean star while the near lens is a violet
  blade with a pale scratch in it. The face reads asymmetrically for most of the
  clip. Mechanism (a flat plate on a dome) correctly identified, not solved.
* hasty's LOOP SEAM IS A REAL POP. Tracked the creature's centroid: ~9 px per
  frame through f230-237, then 38 px in a single frame at the seam. A 4x
  velocity spike, looping forever on the page.
* drift, hasty AND fall render the creature small inside a smear cloud larger
  than it. On `fall` the body is a featureless pink lozenge with no eyes visible
  at all. Three of sixteen clips show nothing of the animal.
* THE KNUCKLES, THE F.2-F.6 INVENTORY, edge-snap-held -- all correctly declared
  not attempted.

## INSTRUMENT CONFESSIONS -- four of mine were wrong

Each passed a selftest and was wrong anyway. Recorded because the PATTERN is the
finding.

1. My EYE LOCATOR matched channel's deep-purple NIGHT sky, reporting an eye area
   of 23,990 px per frame -- 27x the real one. Its selftest had a mauve DAY sky
   case and passed. Caught by DISBELIEVING THE NUMBER. This is the project's
   recorded creature-mask fault, third occurrence. Fixed; the selftest now
   carries three skies and the failure is in the docstring.
2. My RULE-3 DETECTOR matched the MANA MOTES -- the same turquoise as the star,
   by design -- and reported the star crossing the outline on 100% of frames of
   every clip, 800-1900 px for a star that is at most ~150 px. My planted-star
   failure proof passed because the synthetic scene had no mana in it.
   Constraining to teal attached to a lens helped and still leaked, so I STOPPED
   TUNING IT and used it only to rank frames by badness, then looked. That is
   how fault 4 was actually found.
3. My FIRST NOTCH METRIC said the notches got WORSE (lum<25 rising 133 -> 140 on
   rest) because a dilated lens mask swallowed the body's own black ink outline.
   Restricted to each eye's filled interior, the answer flips to ~0 in both
   passes.
4. My CREATURE-SIZE METRIC counted the terrain horizon as part of the animal,
   inflating fall's creature height from ~40 px to 156. Caught by PAINTING THE
   MASK GREEN AND LOOKING AT WHAT IT HAD SELECTED -- the cheapest honest check
   available, and it should become routine. I have NOT reported a creature-size
   number, because I could not get one I would stand behind; the plates say what
   needs saying.

Four instruments, all four confidently wrong at first, every one caught by
looking rather than by the tool. Add to that my own first impression from the
whole-bank sheet -- that the antenna loop had no window -- which was refuted by
watching a full pirouette. Small crops lie about shape.

## RANKED FOR PASS 8

1. THE ANTENNA KNUCKLES (2b). Four gentle swellings in one continuous tapering
   band. The worst-looking thing on the creature, and it is suppressing 2a's
   expression.
2. THE MOTE CORES' WHITE. Stop the corona hearts sampling the ramp's HI end --
   and fix mana_hue_probe so the fault is visible to the instrument.
3. A COLOUR FLOOR ON THE SMEAR PLANE so the daylight clips stop painting khaki.
4. SECTION 5c RULE 3 -- the star crossing the outline. Fix the fault or finish
   the instrument, not neither. Now with by-eye evidence that it is still there.
5. THE NEAR EYE AT OBLIQUE ANGLES -- give the star thickness, or bias it toward
   the camera.
6. hasty's LOOP SEAM -- a measured 38 px jump.
7. THE TRAVERSE FRAMING, then the F.2-F.6 inventory.

## Housekeeping

* COMMITTED, NOT ORPHANED: tools/reel/webm2rgb.py (the publish run wrote this,
  left it in its run folder, and it was gone by the time I needed it),
  plates.py crop, and tools/reel/eyesheet.py. Each with a selftest that fails on
  a known-bad input, each docstring carrying the way its earlier version lied.
* CHANGED NO CREATURE CONSTANT. DID NOT PUBLISH.
* Zixxtrixx untouched: no file outside those three tools and this run folder was
  edited.
