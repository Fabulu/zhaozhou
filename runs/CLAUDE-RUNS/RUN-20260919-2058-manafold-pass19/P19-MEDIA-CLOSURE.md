# Manafold pass 19: media closure

**Date:** 2026-09-19
**Worker:** Claude (sole Opus worker)
**Verdict:** **READY-TO-PUBLISH.** The exact encode and the no-skip local gate are complete.

**Accepted source:** Zhaozhou `10877707`
**Renderer:** MD5 `776d55758933d5284147b360ff2eda62`
**Bank manifest SHA-256:** `f7edc1fbe25016970d6adc830fbea925954fb7d7282297478375c0690f036ed5` (22 subjects, 7,992 frames)

## 1. Encode source

- **Re-validated before the encode:** PASS, with the manifest unchanged (`P19-MEDIA-CLOSURE-RECEIPTS/prencode-validate.log`).
- **Junction only.** `Upheaval/website/scratch-reel` had been dangling at the deleted `v18-final-reel-22`. It was removed with `rmdir` and recreated with `mklink /J` to point at `p19-final-reel-22`, which holds the 22 subject folders.

## 2. Encode

`python tools/tovideo.py . <the 22 live subjects>` ran in one invocation from `Upheaval/website`. Output was redirected to a file, with no pipe (`p19-final-encode.log`).

| Check | Result |
|---|---|
| Exit code | **`ENCODE_RC=0`** |
| WebMs written | **22/22** |
| Posters written | **22/22** |

**Independent probe** (`P19-MEDIA-CLOSURE-RECEIPTS/p19_media_sha.py`, which is v18's tool repointed): **44/44 files, 0 errors, PROBE_RC 0.**
- Every WebM is VP9, 384×240, `yuv444p`, 60/1, and its decoded frame count equals its source frame count.
- Every poster is a 1152×720 PNG.

**`P19-LIVE-MEDIA-SHA256.txt`** records the 44 live files, **44,320,731 bytes** in total. A byte-identical copy sits beside the creature, where it switches `checkarchive.py` into its pass-19 phase.
- All 44 files differ from the version-18 receipt.
- Those version-18 bytes are held by the `archive-v18-manafold-*` copies.

## 3. The live-phase control on the real tree

The encoded tree was checked before the pass-19 receipt existed: `checkarchive` **failed with RC 1 on all 44 live files** ("version-18 live file differs from its receipt") (`P19-MEDIA-CLOSURE-RECEIPTS/checkarchive-before-receipt.log`). So the pre-encode phase really does refuse live names that are not the published version 18. With the receipt in place, it returns RC 0.

## 4. Canonical no-skip local gate (`p19-media-gates.log`)

The invocation was `deploy.ps1 -Project upheaval -Branch main -AssembleOnly`, with no skip flag and FFmpeg 9.0.1 on `PATH`. It was preceded by both selftests.

| Gate | Result |
|---|---|
| Selftests | checkarchive RC 0 (**nine** red legs), checkfresh RC 0 |
| Assemble | 2 creatures / 738 render entries |
| Playback | Fall is controls-only and non-looping; Hover autoplays and loops |
| Archive | v17 56/56 (70,576,645 bytes); **v18 44/44 (44,743,343 bytes), equal to the published v18 receipt**; live names match the pass-19 receipt 44/44; 22 v18 clips, 22+6 v17 clips and 10 labs each declared once; archived playback is controls+loop, no autoplay, `preload="none"` |
| Robots | exactly `noindex, nofollow` |
| Freshness | **22 live fresh / 0 stale / 0 absent / 0 unknown** |
| Decode | **1,464 declared / 1,464 decoded**, with no skip (1,420 before, plus the 44 v18 archive files) |
| Overall | **DEPLOY_ASSEMBLEONLY_RC=0**; nothing deployed |

**Generated `website/public/index.html`:** 435,313 bytes, SHA-256 `6092a6427c45a9dc03d19f7ea6b6e23e05a70f0b76f632635f3814cf639738d7`.
- It carries the card "MANAFOLD, pass 19" and the provenance: source `10877707`, renderer MD5 and manifest SHA-256.
- It shows "Archive (14 generations)" for Manafold.

## 5. Copy

**`website/creatures.json`:**
- The live card is "MANAFOLD, pass 19". In plain language it covers:
  - the rear connection made whole, with the back ball kept as a ball;
  - the calmer rear;
  - mana lines that thin with distance;
  - that everything else is version 18.
- A PROVENANCE paragraph carries the exact bank receipts and states what the sheets could and could not judge.
- The Hover caption, the Inspect note and the Drift note were updated.
- The site note now reads pass 19.

**Creature files:** `PASS-19-FINDINGS.md` and `PASS-19-PLAN.md` are in `Upheaval/creature/Manafold/`.
