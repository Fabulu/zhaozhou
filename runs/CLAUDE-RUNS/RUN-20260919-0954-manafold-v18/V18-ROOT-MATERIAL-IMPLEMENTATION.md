# Manafold version 18 — Wave C root material implementation

**Date:** 2026-09-19
**Direction:** Owner Direction 19
**Architecture:** `V18-ARCHITECTURE.md` §3 / Wave C
**Source base:** committed corrected root authority `df072d47`
**Verdict:** **PASS — medium body-style root transitions selected; no root-ink exclusion justified**

## Production generator

`tools/pack/mkmanafoldpage.py` remains the sole author of the committed `tools/reel/manafold_page.h`.

The generator now has named shipping controls:

```python
ROOT_MATERIAL_MODE = "bodyblend"
ROOT_FRONT_BLEND_ROWS = 16
ROOT_REAR_BLEND_ROWS = 16
```

and strict deterministic diagnostics:

```text
--root-mode bodyblend|legacy
--front-root-rows N
--rear-root-rows N
```

The loop band is still generated first by the exact version-17 cooler/coarser recipe. In `bodyblend` mode, a second deterministic field uses the body pigments, body grain amplitude, body stroke amplitude/frequency and body stroke axis. Quintic C2 row weights make that field authoritative at the front/rear atlas edges and hand continuously to the legacy loop field toward the free middle.

Only the loop band changes. Body, hinge, eye and star generators remain the existing paths. The generated-header provenance now truthfully says it is a committed production asset rather than “do not track.”

Invalid mode, zero width, overlapping widths and nonnumeric width all return RC 2. Bodyblend widths must be positive and leave at least one free middle row.

## Safe deterministic ladder

The live production header was never repeatedly swapped. Four source snapshots were created from committed `df072d47`, each with its own generated header, direct build and output root:

| Candidate | Front/rear rows | Header SHA-256 | Page-word SHA-256 | Renderer MD5 |
|---|---:|---|---|---|
| version-17 legacy | legacy | `14b79292d5800b12243248eb11c6aa4ed290b5a2c0795fdfd0e169f6d7449f56` | `f60d7c935480e044bf14e20990e2a9f92bb08a94b267598f1dbf15ac7193ff18` | `2FD9B451D8125DBEFAFAB7451BFBE570` |
| narrow | `8 / 8` | `bfa57fe7b3cc60c7e07c68d43824de7c519f5bbad3dd7b3dc34a2e116362d134` | `239137ef9b78d5963d1aa5fe8e217d5b83f9fd84dd5986fef39debff3a6005ac` | `B8F5C3E443D09BD430BE502C37939691` |
| **medium — selected** | **`16 / 16`** | **`95f90d46443502211ec44347eccc7b22061e6c01e03e20ce685a9a27cd939564`** | **`7632ba740e77eed2ced3fe89a56ab72c54aa8242b3314901dc2ae6348a20d000`** | `95080A676C3B5315A9DBEF58D9D10BDA` |
| broad | `24 / 24` | `4081b4bdcbbebca8aec55023ea9fdd0f38e1593f9baa4b478708e327ffe05a76` | `0e61c0cd797daa609656106fee2eefbe6ba6d6f6b506320197cd38838032d26e` | `CCD06BB2EFDCD92F317A028C14C116C3` |

The legacy candidate contains **98,302 / 98,302 exact version-17 page words**; the comments differ only because provenance/mode are now explicit. Regenerating the selected header twice produced byte-identical SHA-256 `95f90d...`.

## By-eye selection

All four candidates rendered the same five complete root-heavy clips from isolated binaries:

- Hover 600 frames;
- Rest 400;
- Taunt 280;
- Taunt III 368;
- Trick 400.

That is 2,048 frames per candidate / **8,192 reviewed presentation frames**, plus native and enlarged multi-angle root plates.

The moving read:

- **legacy:** retains the proven darker/coarser strip through each body crossing; the root still reads as a separately surfaced lobe;
- **narrow 8/8:** softens the exact edge but hands back to the coarse loop field inside the long connection, leaving a visible material change through the root support;
- **medium 16/16 — selected:** both connections carry the body’s pink/grain treatment across the visible root thickening and hand gradually to the antenna field outside it. The base reads as one body surface while A/B/C and the free middle keep the cooler/coarser antenna character;
- **broad 24/24:** also fuses the roots, but carries body treatment far into the free chain and weakens the antenna’s distinct middle surface around the outer carriers.

The selected value was chosen from the complete native motion. The atlas/support arithmetic only explains why the result is coherent; it did not choose the value.

The final working-tree renderer is MD5 `A5C94FEE8E0CA8A5FD19CC954E8B1DB6`, SHA-256 `F9135C534F9B341FB494723ACA74E885320AC704C072D70776CD40EE3E3487D7`. Its 2,048 selected frames are **2,048 / 2,048 byte-identical** to the reviewed medium candidate.

## Root-inner ink decision

A separate isolated diagnostic rebuilt the selected medium page with the complete body-inner owner omitted. Native/exact-4× comparison did not expose a clear root seam that justified changing production ink ownership. The visible difference primarily removed accepted internal body/head contour away from the connection; root fusion did not improve enough to warrant a posed exclusion.

Therefore Wave C makes **no change** to `add_visible_body_inner_edge()`, its outline gate or render ordering. The diagnostic remains exploratory under `.tmp` and is excluded from durable evidence. If later smaller-swell/Front-motion art exposes a new seam, that is new picture evidence and can reopen the conditional architecture narrowly.

## Validation

Fresh direct output: `.tmp/v18-material-final`.

| Binary | MD5 |
|---|---|
| `zhao-reel-cel.exe` | `A5C94FEE8E0CA8A5FD19CC954E8B1DB6` |
| `manafold-spangate.exe` | `61F1EA3A15F767CF63D5786981D0DAD9` |
| `manafold-motiongate.exe` | `3DCB32C7840CFC8FABF0DF386F89D1CB` |

- deterministic selected regeneration: byte-exact;
- legacy material words: 98,302 / 98,302 exact;
- generator invalid matrix: 4/4 RC 2;
- `mspan` normal RC 0; all 32 span/root controls attributed RC 1;
- `msmooth` normal RC 0; all 16 effect controls attributed RC 1;
- `mjointpub` normal and F/A/B/C/E controls green;
- `mnodule` normal and F/A/B/C/E controls green;
- `mprobe` normal `0`, scale/outline/mirror controls `1/1/1`;
- mesh, QA, eye-size, outline normal/two controls and shell normal/selftest/legacy/regression all green under their documented conventions;
- unrelated Zixxtrixx Idle: 576 / 576 frames byte-identical before/after.

No root bone/weight, geometry/profile, swell amplitude, Front motion, ink, live mist/mana, particle layer, Flight, Trick timing, site/media or RTL source changed.

## Durable evidence

- `V18-ROOT-MATERIAL-LADDER-NATIVE.png` — eight Hover orbit witnesses for all four candidates;
- `V18-ROOT-MATERIAL-ROOTS-2X.png` — enlarged front/rear root witnesses;
- `V18-ROOT-MATERIAL-{HOVER,REST,TAUNT,TAUNT3,TRICK}-ALLFRAMES.png` — selected complete motion.

Exploratory source snapshots, raw candidate frames, master mosaics, no-inner-ink diagnostic and redundant crops stay under `.tmp` and must not be staged.

## Modified production paths

- `tools/pack/mkmanafoldpage.py`
- generated `tools/reel/manafold_page.h`

Wave C is ready for independent source/picture review. Wave D still owns the five smaller-swell and real public Front-flex values; this material choice does not claim final root likeness by itself.
