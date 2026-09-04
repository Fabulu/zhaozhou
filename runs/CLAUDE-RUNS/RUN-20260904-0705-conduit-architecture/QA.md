# QA — creature 02, the mana conduit

**Reviewer:** independent QA pass, 2026-09-04. First review gate; the creature
shipped straight from its implementer with no review.
**Lane:** zhaozhou `2acab260`, Upheaval `3c44805` (published site built from `40a46da`).
**Standard:** `Upheaval/creature/Unnamed02/OWNER-DIRECTION-1-2026-09-04.md`.
**Evidence:** `evidence/qa/`.

**Deployment was left alone.** Nothing found rises to "must not stay live":
Zixxtrixx is provably untouched, no video is broken or missing, the archive is
intact, `noindex` is present. The faults below are art faults and one optimistic
number — they are reported, not hot-fixed, per the no-authored-value rule.

---

## Verdict per acceptance point

| # | Acceptance | Verdict |
|---|---|---|
| 1 | Four balls, three upper ones articulating as real hinges | **PASS** |
| 2 | Eyes bulging, partly polygonal, more expressive than Zixxtrixx's | **FAIL** — the pupil star renders black |
| 3 | Body visibly compresses; organic, bouncy, jiggly | **PASS** |
| 4 | Several distinct animations; no attacks, no gait; it floats | **PASS** |
| 5 | Ten distinct particle kinds in varied colours + lightning + sun effects incl. skybox bloom | **NEEDS AN OWNER EYE** — all ten exist and register, but one of the ten *is* the lightning, and "big effects in its centre" reads small |
| 6 | Cheap enough for several on screen, with the cost stated | **NEEDS AN OWNER EYE** — cheap enough, but the stated cost is ~1.4–2x optimistic |
| 7 | Textured | **PASS** |
| 8 | Live, at the top, with a video per animation | **PASS** |

Plus the trust claim:

| | Claim | Verdict |
|---|---|---|
| T | Zixxtrixx untouched — 22 bank subjects CRC-identical | **PASS, independently reproduced** |

---

## 1. Four balls, three articulating hinges — PASS

`unnamed02_rig.h` carries 8 bones with a genuine chain `root -> HingeA -> HingeB
-> HingeC`, each hinge pivoting **at its own ball centre**. Not decoration.

They actually move: `loop_alive()` gives each hinge a phase-lagged sinusoidal
fold-scale (front leads, rear follows) plus an out-of-plane tilt, and separate
clips drive distinct fold scales (`open`/`perk`/`whip`/`flare`).

Confirmed from the renders, not just the source: in the **static-camera** `rest`
clip the enclosed loop-window area swings 2335 -> 4607 px, very nearly 2x, within
one clip. A rigid decoration cannot do that.

## 2. The eyes — FAIL

The lens itself is right: purple almond, white inner rim, clearly **bulging** and
clearly **faceted/partly polygonal** at 240p, riding its own bone so gaze
rotations sweep across the surface. That part is good, and it is more articulated
than Zixxtrixx's painted disc.

**But the four-pointed pupil star renders BLACK, in every shipped clip.** All
three sheets draw it cyan, and it is the whole face of a creature with neither
nose nor mouth.

The authored value is correct (`kStarR/G/B` = 64,220,240 cyan) and no texture is
involved (`make_star_blade` leaves `page` at the default 255 = flat colour). It
is the **lighting**:

| subject | cyan-family px | near-black px |
|---|---|---|
| `u02-s4-unlit` (flat colour) | 194 | 0 |
| `u02-s4-front` (shipped lit path) | 0 | 288 |

Same pixels, before and after: the implementer's own `evidence/form-front.png`
holds 288 cyan pixels (72 at native). Reading those exact 72 coordinates out of
the shipped lit render of the same view gives **mean RGB (0.0, 1.6, 2.1), max
(0, 4, 8) — 100% near-black**. Not a shaded cyan (that keeps G,B above R); zero
light.

**How it survived:** the form evidence the eyes were judged against was rendered
on a flat/unlit path where the star *is* cyan. The fault is invisible in that
evidence and present in every shipped frame — the house law's own failure mode,
a component check passing standing in for likeness evidence.

Details and knobs: `evidence/qa/pupil-star-black.txt`, picture in
`evidence/qa/look-eyes-allclips.png`.

## 3. Compression — PASS

Measured on the static-camera `rest` clip, so nothing is camera artefact:

- equator width swings **11 px (14.7%)**
- lower semi-axis swings **14 px (17.6%)**
- **corr(width, height) = -0.969**

Near-perfect anti-correlation is textbook squash-and-stretch: it widens exactly
as it shortens, ~15 cycles over the clip. "Slightly but visibly" is exactly what
this is. Trace: `evidence/qa/traj-compression.png`.

The **`kVStretchPm` anisotropic-projection fix works** — a ball renders round.
Median rendered vertical/horizontal radius ratio is **1.09** (`rest`) and 1.14
(`curious`), against the ~1.66 raw anisotropy the constant compensates. Not an
ellipse.

**`cap_base_fix` is correctly gated:** all 4 creature-02 parts set it true;
`zixxtrixx.h` never mentions it, so Zixxtrixx keeps the legacy layout by default
— proven empirically by the bank identity below.

## 4. Animations — PASS

Ten clips, none an attack, none a gait (there are no legs). Measured signatures
show eight genuinely distinct behaviours, not one motion relabelled:

| clip | centroid x range | area range | motion/frame |
|---|---:|---:|---:|
| rest | 3.0 | 9% | 0.44 |
| crackle | 4.9 | 16% | 1.31 |
| channel | 7.9 | 24% | 0.92 |
| curious | 18.6 | 32% | 1.14 |
| pirouette | 31.5 | 65% | 1.73 |
| startle | 37.1 | 31% | 1.98 |
| hover | 42.0 | 78% | 1.30 |
| drift | 98.4 | 84% | 2.71 |

`crackle` is the nice one: the body barely moves (4.9) while motion-per-frame is
high (1.31) — the motion is all in the effects, which is what "it makes crackling
sounds" should look like.

**It floats.** The committed `u02_probe` rebuilt and re-run by me: clearance holds
in every clip at every key and every 60 Hz midpoint, minimum **88 mm**. The
creature never touches the ground, so Zixxtrixx's penetration law correctly does
not apply. Good that this probe is committed rather than thrown away.

## 5. The mana — NEEDS AN OWNER EYE

**Ten kinds exist and all ten reach the screen.** Classifying additive deltas in
`u02-fx-tour` against the ten authored colours, every kind registers with a mean
cosine similarity of 0.91-0.99 — motes, sparks, wisps, ring-pulse, helix,
droplets, drain, glints, bolt, shield. Varied colours: yes. Sun effects: four
per-clip sun moods plus the **planet-sun skybox bloom** (`s.planet = 1`,
"violet-thick: pure formless bloom") staged behind the channel/crackle/trio
clips — clearly visible, and scene-level so the two-suns-per-view cap is never
touched. That is all delivered.

Three things for the owner to decide:

1. **One of the ten IS the lightning.** Kind 9 is `bolt-beads`. Direction 1 asks
   for ten particle kinds *plus* lightning; as built there are nine mana kinds
   plus a bolt counted inside the ten. Arguable either way — the owner's call.
2. **"Big particle or lightning effects in its CENTRE" reads small.** 62% of all
   particle blobs are a **single pixel** at native 384x240 and 79% are <=2 px.
   They read as a fine coloured shimmer, not as big effects. The centre *glow*
   does deliver a real centre effect; the particles do not read as "big".
3. The implementer's own two flags are **confirmed**: `wisps` (magenta on a hot
   pink body) is the lowest-contrast kind, and the bolt genuinely reads as
   separated beads in stills. Motion helps the bolt; it does not fix the beads.

## 6. Cost — NEEDS AN OWNER EYE

The claim is ~5.2% worst case, ~4.7% idle, ~15.5% for three. **I checked the
arithmetic and it is optimistic by roughly 1.4-2x.**

`glow_splat` scans the **square** box `(2r)^2`, skipping a pixel only *after* a
depth compare. With `kCentreGlowRadiusPx = 46` and `kCentreGlowCorePx = 13`:

| | scanned | blended |
|---|---:|---:|
| outer aura | 8,464 px = 9.18% | <= 5,933 px = 6.44% |
| inner core | 676 px = 0.73% | <= 474 px = 0.51% |
| + particles | ~0.4% | ~0.4% |
| **per conduit** | **~10.3%** | **~7.4%** |
| **three conduits** | **~31%** | **~22%** |

The quoted 4.7% is reachable only by counting depth-rejected pixels as free —
fair for blend cost, optimistic for fill cost, since the loop still visits every
pixel in the box. The outer aura *alone* (6.4% blended) already exceeds the
quoted 5.2% total.

Two sub-claims are correct and verified: the sky bloom really is scene-level and
shared, and the 64-entry glow ramp really is rebuilt once per frame regardless of
instance count. This probably still *is* cheap enough for several on screen — but
the owner asked for the cost to be stated, and the stated figure is not the work
done. Knob: `kCentreGlowRadiusPx` is quadratic (46 -> 36 cuts the outer box 39%).
Working: `evidence/qa/cost-arithmetic-recheck.txt`.

## 7. Textured — PASS

Body, loop and hinges sample the 256x256 atlas tile; the lenses sample a separate
64x64 eye page. Visible as mottling on the body at native resolution. The star
blades are deliberately untextured flat colour ("the star is geometry, never
paint"). `unnamed02_page.h` is generated and gitignored — I re-ran
`tools/pack/mku02page.py` and it **regenerates byte-identical**, so the texture is
reproducible and not a one-off artefact. (Tree restored; nothing modified.)

## 8. Site — PASS

- `unnamed02` is `creatures[0]`; **Unnamed 02 is the first card in DOM order**,
  Zixxtrixx second.
- All **420** declared media files across both creatures exist and are non-empty.
  All ten creature-02 clips decode at **384x240 / 60 fps** with the expected frame
  counts (600/420/600/300/180/160/400/240/600/420). Nothing broken, nothing missing.
- **Exactly one** `<meta name="robots" content="noindex, nofollow">`. The second
  `noindex` in the file is footer prose, not a meta.
- Zixxtrixx's archive tab is intact: 92 archive entries, 11 explicit "Archive
  Generation" headings plus the earlier named generations, all serving.
- The live index at `https://upheaval.pages.dev/` serves at **288,912 bytes —
  byte-count-identical to the local build** — with the same `Built 2026-09-04
  09:59 UTC` stamp, one noindex meta, and Unnamed 02 first.

*Not independently verified:* per-file HTTP status of the live media. This
sandbox's egress blocks sub-paths — a long-live Zixxtrixx asset returns the same
`000` as the new ones, so this is a sandbox limit, not evidence of a fault. The
implementer's report that all ten return 200 live is **inherited, not confirmed**.

*Minor, pre-existing, not creature 02's fault:* Zixxtrixx's `archive_note` prose
still opens "Fifteen preserved generations" while the index says nineteen. It sits
inside the byte-untouched zixxtrixx entry and predates this pass.

## T. Zixxtrixx untouched — PASS, independently reproduced

I did not take this on trust. I extracted a **pristine `a311faf6`** tree with
`git archive` (never touching the lane's `.git`), built it myself with
`build-direct.sh`, built the current tree separately, and rendered all 22 bank
subjects through **both** binaries with `ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross`.

**All 22 subjects identical**, CRC and frame count — and every value also matches
the implementer's `evidence-final-bank-identity.txt`, so that file is confirmed
rather than believed. Full table: `evidence/qa/bank-identity-independent.txt`,
raw captures in `bank-crc-pristine-a311faf6.txt` / `bank-crc-current-2acab260.txt`.

Corroborated on the Upheaval side: the `zixxtrixx` manifest entry is semantically
identical to its state at `8749a5a` (the last commit before any creature-02 work);
all 11 creature-02 commits touch **zero** zixxtrixx paths; and the 64 zixxtrixx
media files that changed since then all changed in `bc72d89` "Direction 30 media:
the calmed sun bank", a prior unrelated lane.

The implementer's report that the gate caught a careless `sed` which had silently
retimed five bank cameras is consistent with what I see — and is the reason this
gate is worth its cost.

---

## Sheet likeness — the second eye

I compared height-normalised silhouettes like-for-like, side against `Side.png`
and front against `Front.png`: `evidence/qa/likeness-overlay.png` (red = sheet,
blue = render, purple = agreement).

**What is right:** the body's size and placement relative to the whole creature
is good — `body_width / total_height` is 0.519 on the sheet and 0.543 in the
render, +4.6%. The hot pink reads correctly at 240p. The flat blade-like loop is
genuinely flat, broad from the side and narrow from the front, exactly as the two
sheets together establish.

**What is off — and the implementer's own flags understate it:**

1. **The loop's *character*, not just its width.** The drawing's loop is a smooth,
   rounded, organic teardrop. The render's is an **angular bent-wire polygon** —
   straight tube segments meeting at visible corners. Three hinges plus a root
   give four straight segments, so this follows from the rig; but the read is
   "coat-hanger" where the sheet is "loop of flesh". The loop also measures ~27%
   short in height relative to the creature. The implementer called this "slightly
   narrower"; it is more than width.
2. **From the front the antenna is a uniform rod.** The sheet shows a tapered,
   organic blade that flares where it meets the body, with a visible narrow
   opening. The render shows a **constant-width vertical stick with bead bulges**
   and no visible loop opening head-on.
3. **The body is a sphere where the sheet is a teardrop.** In the drawing the body
   narrows gradually upward and flows continuously into the loop; in the render a
   round ball meets a distinctly separate thin tube. This is the single biggest
   contributor to the "lollipop / banjo" read.
4. **The eyes are smaller than drawn.** Confirmed — the sheet's stars are visibly
   larger in the overlay. The implementer's flag is right.

Every one of these is a named knob (`kLoopBladeRxMm`, `kLoopBladeRzMm`,
`kLoopArcMm[]`, `kLoopFold*A16`, `kBodyTaperPm[]`, `kEyeLongMm`, `kEyeWideMm`), so
all are owner-adjustable. None is a blocker; all want the owner's eye.

---

## For the owner — what to look at, and what to watch for

**Look at these three clips, in this order:**

1. **`Trio`** — the strongest clip and the one that proves the brief. Three
   conduits, one shared stage, one shared sky bloom. Watch the **big pale bloom**
   behind them and the coloured mana around each. *Watch for:* whether three at
   once feels affordable to you — that is the requirement the cost note is about.
2. **`Channel`** — watch the **bolt** in the loop window. It reads as separated
   white beads rather than a continuous arc; decide whether that crackle is what
   you want. Also watch the sky bloom staged screen-left.
3. **`Rest`** — the calmest clip, and the best place to see the **up-and-down
   compression**. It is genuinely there (-0.969 squash/stretch correlation).
   Watch the antenna lag and settle behind the body.

**Then look at any clip's face, close up.** The pupil stars are **black**, not
cyan. That is the one thing I would not ship as-is.

**Everything needing your eye, with the constant that moves it:**

| What | Constant(s) | Where |
|---|---|---|
| **Pupil star renders black, not cyan** (the fault) | needs a lighting/emissive fix; partial mitigation via `kPupilStarArmMm` 78, `kPupilStarWideMm` 30, `kPupilStarThinMm` 16, `kEyeBulgeMm` 88 | `unnamed02_art.h` |
| Eyes could be larger / more diagonal | `kEyeLongMm` 250, `kEyeWideMm` 92, `kEyeVAngleA16` 2600, `kEyeTiltA16` 2200 | `unnamed02_art.h` |
| Loop reads angular and ~27% short vs the drawing | `kLoopArcMm[4]`, `kLoopFoldRootA16` / `kLoopFoldAA16` / `kLoopFoldBA16` / `kLoopFoldCA16` | `unnamed02_art.h` |
| Loop too thin / not organic enough | `kLoopBladeRxMm` 105, `kLoopBladeRzMm` 32 | `unnamed02_art.h` |
| Body reads as a sphere, not a teardrop | `kBodyTaperPm[]`, `kBodyLeanXMm[]` | `unnamed02_art.h` |
| Mana particles read small ("big effects in its centre") | per-kind `k*N` counts and sizes; `kCentreGlowRadiusPx` 46 for the centre glow | `unnamed02_fx.h`, `unnamed02_art.h` |
| Wisps too subtle (magenta on pink) | `kColWisps` {150,55,115}, `kWispsN` 10 | `unnamed02_fx.h` |
| Bolt reads as separated beads | `kBoltSegs` 12, `kBolt2Segs` 8, `kBoltJitterMm` 130, `kBoltRehashFrames` 3 | `unnamed02_fx.h` |
| Effect cost higher than stated | `kCentreGlowRadiusPx` 46 (quadratic), `kCentreGlowCorePx` 13 | `unnamed02_art.h` |
| Is the lightning one of the ten, or extra? | a reading of Direction 1 — your call | — |
| **The creature still has no name** | — | — |

## What I verified vs inherited

**Verified myself, from a build or a render I made:** the 22-subject bank identity
against a pristine baseline I extracted and built; the black pupil star; the
squash-and-stretch correlation; ball roundness; loop-window articulation; the
clearance probe; clip distinctness; the ten particle colours reaching screen; the
cost arithmetic; texture-page determinism; every declared media file's existence
and decodability; the site's card order, noindex count and archive integrity; and
that the live index matches the local build.

**Inherited, not confirmed:** per-file HTTP 200 status of the live media (sandbox
egress blocks sub-paths — a known-good Zixxtrixx asset behaves identically, so
this is a tooling limit, not a finding).

**Could not confirm either way:** nothing else.

## Housekeeping

Baseline extraction, both builds and all render output were made in the session
scratchpad, never in either repo. No background jobs were left running (verified
by process check). No authored art value was changed. `unnamed02_page.h` was
regenerated for a determinism check and restored byte-identical.
