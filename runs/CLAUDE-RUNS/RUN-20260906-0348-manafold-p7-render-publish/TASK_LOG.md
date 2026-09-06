# Task Log: RUN-20260906-0348 - [Describe objective here]

**Created:** 2026-09-06 03:48 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0348-manafold-p7-render-publish/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 03:48 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0348
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

## Pass 7 RENDER-AND-PUBLISH log

### Objective
The pass-7 code is merged to `main` in both repos; the live site still serves
pass-6 media. The pass-7 implementer correctly refused to publish because the
geometry, star, smear and fog all changed. This run re-renders the bank from a
clean clone of `origin/main`, encodes, corrects the captions, archives the
outgoing generation and publishes.

### Lane
`C:\programmieren\zencrifice\manafold-p7-publish\{zhaozhou,Upheaval}`, cloned
fresh from GitHub. NOT the hardware agent's checkouts.
- `zhaozhou` @ `9a97c4f3` (expected `9a97c4f3` — MATCH)
- `Upheaval` @ `d0a36e4` (expected `d0a36e4` — MATCH)

### Build (step 1)
`bash tools/reel/build-direct.sh --output <lane>/build cel`
- **`BUILD_RC=0`**, read from the recorded exit code and not from `tail -1`
  (09-ENGINE-GOTCHAS §13).
- `manafold_page.h` is committed (819,853 B), so the pass-4 "clean checkout
  renders the creature BLACK" trap is guarded by a hard build error rather
  than a `__has_include`.
- `build/bin/zhao-reel-cel.exe`, 2,739,759 B.

### MISTAKE MADE AND CORRECTED (recording it, per the project's own habit)
I ran `zhao-reel-cel.exe` with NO arguments to read its usage. There is no
usage: gotcha §8 says the reel takes its output dir as the first positional
and parses no flags, so a bare invocation **renders every wired subject**. It
began writing `blue-dwarf/`, `terrain-*/`, `pulsar/` etc. into the lane root.
Stopped via TaskStop, 15 junk directories deleted, `git status` clean in both
repos. Nothing inside either repo was touched. The gotcha is real and I walked
straight into it.

### Rig verified by READING, not inferring (gate A.1 / gotcha §12)
`zhao_reel.cpp:5140`, inside `subject_u02_clip`:
    s.creature_moving_light = true;
Every Manafold clip subject raises the many-colour moving rig, and that gates
the per-clip `kU02Sun*` suns OFF. This is what makes the two pass-6 "under the
hover sun" / "under the shipping sun" captions false, and it is why they were
corrected in the pass-7 code commit.

### Render (step 2) — ONE binary invocation (gate item 20)
Binary md5 recorded to `render-p7-binary.md5` before the run.
    ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross \
      build/bin/zhao-reel-cel.exe Upheaval/website/scratch-reel \
      <22 subjects, named explicitly>
22 subjects = the 16 motion clips + the 6-variant mana menu. Named explicitly
because a bare output-dir invocation renders the whole library (gotcha §8).

### Archive of the outgoing generation (step 4) — DONE BEFORE encoding
32 files (16 webm + 16 poster png) copied
`manafold-<clip>.{webm,png}` -> `archive-pass6-u02-<clip>.{webm,png}`.
- Outgoing hashes recorded to `pass6-outgoing.md5` first.
- Copies verified byte-identical to their sources: **0 mismatches**.
- Follows the `archive-pass5-u02-*` precedent exactly (the 16 motion clips;
  pass 5's generation did not archive the mana menu either).
- `assemble.py` derives the poster as `.webm` -> `.png` and does NOT
  existence-check it, so a missing poster would fail silently. Both copied.
- `MAX_ARCHIVE_GENERATIONS = 19`; Manafold goes 3 -> 4 generations. Room.

### LOOKING AT IT (step "look before you ship")
Method: the outgoing pass-6 clip is only available as an encoded webm, so
`webm2rgb.py` lifts one frame back into the reel's own `.rgb` format (8-byte
header + RGB24) and the comparison is then built by the COMMITTED
`tools/reel/plates.py` on the COMMITTED `rgbframe.py` reader. No fifth
hand-rolled frame reader: `rgbframe.py`'s own docstring records that two of
the four confidently-wrong diagnostics on this creature were bad readers.

**hover f160, the front-on face frame, at NATIVE 384x240** (the acceptance
scale Direction 5 names four times):
* pass 6: the two eyes read as two thin pale STREAKS. You cannot tell they
  are stars.
* pass 7: they read as two small STARS. The shape is legible at native.
  That is the pass's headline art win and it clears the bar it is judged at.

**hover f160 at 7x**, both eyes: pass 6's far-eye star is a long thin spindle
running the length of the lens. Pass 7's is a compact FOUR-POINT star, teal
core, white outline tracing the points as one unit. Matches the authored
change exactly (arms 216/167/68 -> 147/114/77 mm; aspect 2.82 -> 1.69 against
the sheet's drawn 1.70).

**channel f231 (the owner's own pick) at 3x** -- the biggest single fix:
* pass 6 shoots a LONG WHITE SPIKE off the far eye, out past the body outline
  and into the sky. That is section 5c rule 3 violated, visibly, on the
  published clip.
* pass 7: the spike is GONE. Compact star at the rim. This is the
  apply_twinkle cut (60 deg -> 12.5) plus the re-proportioned arms.
* The mana cluster is denser and now carries chunky blocky smear fragments.
  In pass 6 it is sparse dots.

**rest f234 and curious f60**: the fog reads as GAS, not sparkle. Pass 6 has
clean isolated cyan dots on flat ground; pass 7 has chunky glitchy haze around
the loop and trailing off it, with the neck still readable through it.
Direction 5 section 3's "very visible ... still see through" reads as met, at
native. kFogThicknessPm 1000 -> 4500, and the smear plane composites at all
now (kSmearPresetCount derived with a static_assert).

**taunt f150** -- the honest half:
* The far eye is much better: pass 6's long white tail is gone, the star is
  compact and clean.
* The NEAR eye's star is still a pale BAR, not a star, and a contact sheet of
  24 frames across the clip shows it on most of them. It is THINNER than pass
  6's -- a real but partial improvement.
* This is the DECLARED, KNOWN limitation: a flat plate on a dome has no
  defence against edge-on. Mechanism identified in pass 7, not solved.
  Recorded here as seen, not inherited from the report.

**Black notches**: not reading as hard black wedges at the lens tips at native
or at 7x on the frames examined. The ink contour remains in the page, narrowed.
I did NOT reproduce the implementer's near-lens crop metric, because my crop is
a different region and the number would not be comparable -- so this is a
by-eye statement, and it is bounded to the frames I looked at.

**Whole-bank sweep at native** (all 16 motion clips, one poster frame each,
looked at as one sheet rather than clip by clip): nothing unexpected. The two
things that look bad are both DECLARED and PRE-EXISTING, and I checked rather
than assumed:
* `hasty` and `drift` render the creature at 25-40 px inside a grey smear
  cloud larger than the animal. A/B against the pass-6 archive at the same
  frames shows the two passes are near-identical here -- these are the two
  clips that ALREADY had the smear in pass 6 (with `still`, they are the
  three the table bound did not silence), so the fog thickening did not push
  them past the lab's own "the mana starts eating the animal" line. Not a
  pass-7 regression. The framing is the by-eye review's open fault and was
  not attempted this pass.
* The trail reads GREY rather than cyan. That is the "mana reads as white
  steam" fault, ranked first by the pass-6 review and only partially won back
  this pass (8.3% -> 7.9% hue-neutral). Declared, not hidden.

**Cross-eyed taunt, checked specifically and it does NOT read.** A 12-frame
strip through the held beat at 6x: the near eye's star is a pale bar on every
one of them, and a bar has no aim to see. The feature is genuinely wired
(`apply_gaze_lr` had zero callers before this pass) but the camera angle
defeats it. Captioned as wired-but-not-reading rather than as delivered --
this is precisely the class of quietly-true-sounding caption I was sent to
stop.

### Site-structure gates
* Manafold is `creatures[0]` -- first card. There is no sort in assemble.py.
* Exactly ONE `<meta name="robots" content="noindex, nofollow">`, content
  read and not merely counted.
* Manafold archive generations 3 -> 4 (limit 19). Zixxtrixx untouched at 19,
  which is EXACTLY the limit -- the next Zixxtrixx generation will need
  MAX_ARCHIVE_GENERATIONS and both style.css selector families extended
  together. Noted for whoever hits it.
* All ten `manalab-*` experiment variants present on disk and declared.

### THE PASS-7 CRCs
All 22 from ONE binary invocation (gate item 20), md5 of that binary recorded
before the run, `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`,
`RENDER_RC=0` read from the exit code.

| subject | sequence_crc32c | frames | unique colours |
|---|---|---|---|
| `manafold-channel` | `0x3FDF36DC` | 420 | 19632 |
| `manafold-crackle` | `0x3A525137` | 600 | 19686 |
| `manafold-curious` | `0xDCFC2CFD` | 180 | 16883 |
| `manafold-damage` | `0x9E0450D0` | 464 | 18624 |
| `manafold-drift` | `0xD2262040` | 300 | 11580 |
| `manafold-fall` | `0x5E0CFF92` | 340 | 12153 |
| `manafold-hasty` | `0x0542043E` | 240 | 11200 |
| `manafold-hit` | `0x5164E679` | 140 | 16201 |
| `manafold-hover` | `0xF8468CC4` | 600 | 20557 |
| `manafold-inspect` | `0xF8468CC4` | 600 | 20557 |
| `manafold-mana-aqua` | `0x8B070C49` | 600 | 18753 |
| `manafold-mana-blue` | `0x719A34C1` | 600 | 17446 |
| `manafold-mana-boil` | `0xF114D862` | 600 | 19127 |
| `manafold-mana-cyan` | `0x7F9219C1` | 600 | 18115 |
| `manafold-mana-green` | `0x97FC84CD` | 600 | 18754 |
| `manafold-mana-stack` | `0xE638C8C8` | 600 | 20417 |
| `manafold-pirouette` | `0xA88B307B` | 240 | 16732 |
| `manafold-rest` | `0xDD58FC3B` | 400 | 19961 |
| `manafold-startle` | `0x4DB9C19C` | 160 | 15911 |
| `manafold-taunt` | `0x124D3C3F` | 280 | 17974 |
| `manafold-taunt2` | `0x37F045F6` | 240 | 17966 |
| `manafold-trick` | `0x4D7EA672` | 400 | 19692 |

`manafold-hover` and `manafold-inspect` carry the SAME CRC (`0xF8468CC4`) and
diff to 0 differing frames of 600. The Inspect caption's byte-identity claim
therefore still holds in pass 7, verified from this binary rather than
inherited from pass 6's report.

### Clip lengths, pass 6 vs pass 7 -- all 16 IDENTICAL
Checked because `tovideo.py` selects each poster by a NAMED FRAME INDEX, so a
clip that changed length would silently move its poster onto a different beat
and the card would stop showing the moment it was chosen for. Pass-6 lengths
read by DECODING the archive webms (`nb_read_frames`), pass-7 lengths counted
from the `.rgb` frames:

  hover 600 · inspect 600 · channel 420 · trick 400 · damage 464 · hasty 240
  fall 340 · hit 140 · taunt 280 · taunt2 240 · drift 300 · curious 180
  startle 160 · rest 400 · pirouette 240 · crackle 600

Every one the same. This also independently confirms the implementer's
"channel and taunt retime nothing but move".

### Encode fidelity, checked rather than assumed
VP9 CRF16 / yuv444p is lossy, so the contract's claim that it is
"indistinguishable on flat shading at this resolution" is a claim. Decoded
`manafold-hover.webm` frame 160 back out and compared against its source
`.rgb`: max per-channel delta 15/10/14, mean delta where changed 1.15. Side by
side at 7x on the eye, the four-point star, its white outline and the ink are
all intact. The contract holds on this creature.

### THE LIVE SITE BEFORE STATE (so the after-check can actually prove a change)
    curl https://upheaval.pages.dev/
    HTTP 200, 322,798 bytes
    "MANAFOLD, pass 6"      present
    "2026-09-06"            ABSENT (no Pass 6 archive tab)
    name="robots"           exactly 1
322,798 B is byte-for-byte the size of the committed pass-6 `public/index.html`,
so production is serving exactly the pass-6 build. After the deploy the page
must say "MANAFOLD, pass 7" and carry the 2026-09-06 archive tab; if it does
not, the deploy was demoted to a preview and `-Branch` was the reason.

### The generated texture page is NOT stale -- checked, not assumed
CLAUDE.md: "a generated file that nobody regenerates is a stale file with a
reassuring provenance line at the top." Pass 7 narrowed the lens ink INSIDE
`tools/pack/mkmanafoldpage.py`, so if the committed `manafold_page.h` had not
been regenerated, every frame I rendered would still carry the pass-6 notches
and the whole ink fix would be invisible while the source said it was done.

Regenerated to a scratch path (never over the committed file, which would
dirty the tree the CRCs were taken from) and compared:

    python tools/pack/mkmanafoldpage.py <scratch>/regen_manafold_page.h
    cmp <scratch>/regen_manafold_page.h tools/reel/manafold_page.h  -> IDENTICAL

819,853 B both. So the committed page IS the pass-7 page, and as a free
by-product this is the two-regens-identical determinism check that
CREATURE.json lists in required_checks.

### The committed 3D probe, run from MY OWN build (gate item 7: reproduce, don't inherit)
`build-direct.sh --output <lane>/build mprobe` -> `MPROBE_BUILD_RC=0`. Built
while ffmpeg was encoding but with NO render running, and
`zhao-reel-cel.exe` kept its 03:50 timestamp -- it was not relinked, so the
CRCs above still belong to the binary that produced the frames.

Every headline number the implementer reported comes back IDENTICAL:

    CLEARANCE CONTRACT HOLDS (>= 40 mm everywhere), 16 slots
    slot 13 (trick) DECLARED CONTACT keys 78..156: deepest vertex -23 mm
        (declared -25, accepted -60..-5)                          OK
    5c rule 1  overhang worst 29 mm / cap 29                      OK
    5c rule 2  worst 760 pm / floor 600                           OK
    5c rule 3  1499 violations       REPORTED-NOT-ENFORCED
    5d gate A  closest approach 22 mm / floor 12                  OK
    5d gate B  lens deepest 801 pm / floor 1000                   OK
    eyeball-shift NOT SHIPPED -- a DECLARED gap, and the probe says so itself

This settles the one caption on the card that makes a PHYSICAL claim: Trick's
"-25 mm declared ground contact" is still true in pass 7, measured by the
committed probe walking posed vertices against terrain height -- never from
rendered pixels, which CLAUDE.md records as unsound under perspective.

Rule 3 remains a real, unfixed fault (the star crossing the body outline in 7
clips) reported and not enforced, because the instrument is aimed at 2 fixed
orthographic views while the shipping cameras orbit. Its worst VISIBLE
manifestation -- the white spike shooting off channel's far eye into the sky
-- is gone; I looked. It is pass 8's first eye item and it ships declared,
not hidden.

### Pass 6's wins, checked for damage rather than assumed safe
Eight frames through a full `pirouette` turn, 3x:
* The antenna is ONE CONTINUOUS SURFACE body-to-re-entry through every angle.
  No countable spheres -- section 2b's own acceptance sentence still passes.
  Pass 7 touched the eyes, not the antenna, and the render confirms it.
* The loop still closes around a real window; the dongle is still resolved.
* The knuckles still read ANGULAR and faceted (pass 6's over-correction).
  Declared not attempted this pass; visible, unchanged, and honest.

### VERDICT: worth publishing
The standing authorisation is for a pass that is DONE and WORTH LOOKING AT.
This one is: the eyes read as stars at native where they read as streaks
before, the fog is gas, the smear runs everywhere, and the ugliest visible
artefact on the old bank -- the white spike off channel's far eye into the
sky -- is gone. The things that are still wrong are all DECLARED, all
pre-existing, and none of them regressed: the near eye at steep angles, the
grey trail, the 25-40 px traverse framing, the faceted knuckles, rule 3.
Nothing new broke. Publishing.

### Zixxtrixx: untouched, and PROVED at the artefact level
Two things, and the second is the stronger one:
* `git status` over `website/public/renders/` lists ONLY `manafold-*` files as
  modified for the whole of this run. Every Zixxtrixx byte on the site is
  literally the byte that was already there -- which is a stronger statement
  for a PUBLISH than re-rendering and comparing CRCs would be, because it is
  the shipped artefact itself and not a reproduction of it.
* All 808 Zixxtrixx declared files (410 sources + their derived posters)
  DECODED: 0 failures. Not probed -- decoded, frame by frame.

---

## PUBLISHED

    .\deploy.ps1 -Project upheaval -Branch main
    Uploaded 45 files (950 already uploaded), 995 total
    Deployment complete

`-Branch main` given explicitly. The per-deployment alias wrangler prints
(`eecb68c2.upheaval.pages.dev`) is NOT the evidence -- that URL exists for
previews too, which is exactly the trap. The evidence is the PRODUCTION host:

    curl https://upheaval.pages.dev/
    HTTP 200, 333,305 B          (before the deploy: 322,798 B)
    "MANAFOLD, pass 7"           (before: "MANAFOLD, pass 6")
    Pass 6 - 2026-09-06 tab      (before: absent)
    name="robots" content="noindex, nofollow"   exactly 1
    16 archive-pass6 media refs, 10 manalab variants
    LIVE PAGE BYTE-IDENTICAL to the locally assembled public/index.html

And the media, end to end -- fetched FROM PRODUCTION and DECODED, because a
page that references a clip is not a clip that plays:

    manafold-channel.webm        200  2,494,492 B  420 frames decoded
    manafold-hover.webm          200  3,604,337 B  600 frames decoded
    archive-pass6-u02-rest.webm  200  2,435,417 B  400 frames decoded
    manalab-edge-snap-held.webm  200  4,556,599 B  800 frames decoded
    manafold-channel.png         200     79,778 B  decoded

All five `cmp`-identical to the local files, so production is serving the new
bytes and not a cached generation.

### Pushes, verified from the remote (gate item 27)
Pass 6 reported "pushed" with sixteen commits still local, so every push this
run was checked with `git fetch` + `git branch -r --contains`:

    Upheaval  local main 4b7d35e == origin/main 4b7d35e
    zhaozhou  local main 5428fa64 == origin/main 5428fa64

A publish deploys from local files and is not a backup; these are.

### Background work, verified stopped (gate item 28)
No `zhao-reel`, `zhao-reel-cel`, `manafold-probe` or `ffmpeg` process alive.
The one stray render started this run (the bare no-arg invocation) was killed
and its 15 junk directories deleted.

### FOR PASS 8
1. **5c rule 3 is still a real, unfixed fault** -- 1499 samples over 7 clips,
   reported-not-enforced because the instrument uses 2 fixed orthographic
   views while the shipping cameras orbit. Fix the fault or finish the
   instrument, not neither.
2. **The near eye at steep angles.** The star is a flat plate on a dome. It
   is what stops the cross-eyed taunt gag from reading even though the gag is
   correctly wired.
3. **A STALE COMMENT in `manafold_fx.h`** around `kFogThicknessPm`: the prose
   says 2000 "was the first value where the gassy shell read as deliberately
   thick fog", but the shipped constant is 4500. The value moved and the
   comment did not. Not touched this run -- editing the reel closure would
   have broken the match between the tree and the CRCs being published -- but
   it should be corrected before it is read as authority.
4. **Zixxtrixx sits at EXACTLY MAX_ARCHIVE_GENERATIONS (19).** Its next
   archive generation needs `assemble.py`'s limit and both `style.css`
   selector families extended together, or assemble will refuse.
