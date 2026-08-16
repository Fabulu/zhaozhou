# Zhaozhou Effects Library - Final Report (corrected by the completion session)

## Read this first

The first version of this report claimed "19/27 effects implemented, library
structure complete" while eight of the nineteen had no render and no CRC pin,
and the fast test lane was red at HEAD. This version states what is true at
commit 27ab1a1 (completion session, 2026-08-16). A catalogue entry saying
READY with no render and no pin is a citation with no evidence; that failure
mode is what this session removed.

## What is true now

**Every implemented catalogue entry has a rendered artifact and a CRC pin
that the pipeline byte-verified.** 27 entries total: 19 implemented, 8
honestly not (3 dead star classes by spec law, starfield/corona-variant/LOD
rungs not yet screened). Of the 19: 15 distinct animated reel subjects, 1
static still (field-crater.png), 3 alias entries that point at another
subject's render and say so.

## Motion trails (the owner's ask, spec §15 amendment v1.1)

*"The moving sun in space isn't fully noctis style. They have this smear to
them."* The smear is law now: a per-star ring of the last 8 light positions,
rendered as a fading ghost chain of the star's own corona sprite.

- **Ring**: 8 positions, head + length, 34 B per star. Serialized into
  `celestial_state`: **168 -> 236 B** (verified against the struct; the v1
  prefix parses unchanged). Replay-exactness is by capture, not warm-up:
  `star_trail_replay` replays from a deserialized chunk byte-exactly.
- **Level law**: `level(g) = 63 - 4g` (59..31, the ramp's bright half).
  **Two earlier laws died on measurement**: alpha-scaled ghosts cost
  ring-colours x ghost-alphas; graded level-capped ghosts carry prefix
  skirts that overlap-sum to **744 unique colours** in one subject. The flat
  law gives each ghost exactly one ramp entry; every trailed subject now
  measures <= 243 of 256.
- **Halo-skip**: ghosts never draw inside the star's own halo circle (the
  additive halo x ghost palette product has no bound otherwise).
- **Static-skip**: a resting star emits zero ghosts - star-boil and pulsar
  are byte-identical with trails in the tree (asserted by test).
- **Anchors** (`star_trail_anchor`): ring fill/evict order, level law, flat
  palette, a spaced ghost's centre pixel on black == `ramp[level(g)]`
  exactly, longest visible tail = 8, eviction at 9.

## The renders (deterministic, verified)

Rendered three times, byte-identical every run. GIFs encoded palette-exact
(`paletteuse=dither=none`, never palettegen) and verified by decoding back
and comparing every frame byte-for-byte; a gif that fails is deleted.

| Subject | Frames | GIF bytes | Palette | sequence CRC |
|---|---|---|---|---|
| star-boil (80 px re-shoot) | 63 | 586,248 | 191 | 0xADC6EB7C |
| pulsar (28 px re-shoot) | 64 | 18,688 | 210 | 0x6F2A61FC |
| noctis-flare (re-shot, trailed) | 64 | 207,623 | 243 | 0xD20023CD |
| sky-sweep (re-shot, banding fix) | 64 | 240,155 | 79 | 0x074B5DCA |
| flare-occlusion (re-shot) | 64 | 103,633 | 112 | 0x4382E5C8 |
| blue-giant | 64 | 172,778 | 48 | 0xDFAFCD70 |
| white-dwarf | 64 | 44,856 | 63 | 0x048AB345 |
| orange-giant | 64 | 191,326 | 79 | 0x66299B68 |
| blue-dwarf | 64 | 71,704 | 126 | 0xD3355069 |
| multiple (binary, curved trails) | 64 | 179,555 | 121 | 0xCA637ABD |
| infant | 64 | 115,812 | 136 | 0x5FBE7C1B |

`zhao-reel --check`: **all 15 sequence CRCs match** (the ctest
`reel_sequence_crc` lane, green for the first time since dcb32ff).

## The terrain four: shipped vs current, stated plainly

The kBandRows 8->16 sky fix (dcb32ff) landed AFTER the terrain gifs were
shot. Per owner instruction the four terrain gifs were NOT re-rendered; the
catalogue pins each shipped artifact's CRC and records the current
renderer's CRC in `crc_note` (what `--check` enforces):

| Subject | shipped (site gif) | current renderer |
|---|---|---|
| terrain-wave | 0x0222090B | 0xE89BB76B |
| terrain-impact | 0x4F97AD9B | 0x7E07D08A |
| terrain-scars | 0x86069EA1 | 0x106B4DE8 |
| terrain-breach | 0x47D4D163 | 0x839E117F |

The shipped gifs show the pre-fix (banded) sky. Re-rendering them is one
command when the owner wants it.

## What was wrong in the handover (and what I did about it)

1. **"19/27 READY" with 8 unevidenced entries.** Fixed: everything
   implemented is now rendered and pinned; the unimplemented 8 say false
   with no render and no pin.
2. **The fast lane was red at HEAD** (render_sky census, render_golden
   CRCs) - dcb32ff shipped with an fsyntax-only check. Fixed by re-pinning
   to the deliberate fix (8670c7e). render_golden's repo pins are now
   0xB1B5171A / 0x2B73E8CB; the site's zshot/assemble copies still hold the
   old pair matching the shipped duo-frame.png, and reconcile on the next
   stills re-render.
3. **Stale comment**: the star-boil case claimed "THREE large S03 giants";
   the code renders one. Corrected.
4. **S08 "Multiple" rendered as a single star** despite being the binary
   class. Now two bodies of one class orbiting the barycentre once per
   loop, each with a curved trail.
5. The first report's "S01..S09 descriptions updated" implied renders
   existed. They did not; now they do.

## Site

- **The star catalogue section is generated from effects-library.yaml**:
  the twelve-class gamut table (class, name, colour swatch, library status
  with CRC) plus six figures with full provenance. assemble.py gained the
  generator; the copy gate still hard-stops (it caught an em dash in this
  session's own copy before any page was written).
- **Owner amendment folded in**: the page now says plainly that the end goal
  is fabricated silicon, a physical console, and the MiSTer FPGA core is a
  working proving ground along the way, not the destination. One clause for
  the intended Steam release in the what-for list. Deployed via
  zhaozhou-site/deploy.ps1 to https://zhaozhou.pages.dev.

## Lane results (pass / skip / fail, never a percentage)

- fast gate: 80 pass, 1 skip (format_check, no tool on this machine),
  0 fail, out of 81.
- reel_sequence_crc: pass (15 subjects).
- render_star: pass (now 17 test functions, 2 new for trails).
- GIF encode verification: 11/11 byte-exact; 0 deleted.
- copycheck: clean on effects-library.yaml and the assembled page.

## What is still not done (honest)

- The 5 unscreened effects (starfield backdrop, corona-atmo, corona-airless,
  lod-corona, lod-point) and the 3 dead classes remain `implemented: false`.
- The four terrain gifs are one sky-fix behind the code (owner's call).
- The site's duo-frame/sky-dusk/island-terrain stills predate the banding
  fix; their provenance matches the shipped files, the repo goldens do not
  (see item 2 above).

**Completion agent:** Ms. Frizzle Mode
**Date:** 2026-08-16
**Commits:** 8670c7e (test re-pins), 6cb4e54 (motion trails), 27ab1a1
(catalogue v2)
