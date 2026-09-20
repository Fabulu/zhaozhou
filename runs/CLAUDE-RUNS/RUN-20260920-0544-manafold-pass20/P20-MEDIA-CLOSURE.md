# Manafold pass 20: media closure

**Date:** 2026-09-20
**Worker:** Claude (sole Opus worker)
**Verdict:** **READY-TO-PUBLISH.** The exact encode and the no-skip local gate
are complete, with real exit codes read off the things themselves.

**Accepted source:** Zhaozhou `55767880`
**Renderer:** MD5 `e95faca916627d1bddb02892c5eb67e1`
**Bank manifest SHA-256:** `a40b41549383246d7c9580c768c936f8919eb810dce7c0ecae24e3cdb1313b15`
(22 subjects, 7,992 frames)

## 1. Encode source

- **Re-validated immediately before the encode:** PASS, 22 subjects, 7,992
  frames, 2,209,692,096 bytes, **manifest unchanged**
  (`P20-MEDIA-CLOSURE-RECEIPTS/preencode-validate.log`).
- **Junction only.** `Upheaval/website/scratch-reel` was dangling at the
  deleted `p19-final-reel-22`. It was removed with `rmdir` and recreated with
  `mklink /J` pointing at `p20-final-reel-22`, which holds the 22 subject
  folders and nothing else.

## 2. Encode

`python tools/tovideo.py . <the 22 live subjects>` in **one** invocation from
`Upheaval/website`, output redirected to a file with **no pipe**
(`p20-final-encode.log`).

| Check | Result |
|---|---|
| Exit code | **`ENCODE_RC=0`** |
| WebMs written | **22/22** |
| Posters written | **22/22** |

**A first attempt was killed and rerun, deliberately.** It had been launched in
a way that lost its exit status, and CLAUDE.md is explicit that a long job's
real exit code is the thing to read. Processes were identified by command line
first (`Get-CimInstance … CommandLine`) and killed **by PID**, never by image
name, because every lane in this tree runs the same `ffmpeg.exe` and
`python.exe`. The rerun is the encode that shipped.

**Independent probe** (`P20-MEDIA-CLOSURE-RECEIPTS/p20_media_sha.py`, pass 19's
tool repointed): **44/44 files, 0 errors, PROBE_RC 0.**
- Every WebM is VP9, 384×240, `yuv444p`, 60/1, and its decoded frame count
  equals its source frame count.
- Every poster is a 1152×720 PNG.

**`P20-LIVE-MEDIA-SHA256.txt`** records the 44 live files, **44,980,318 bytes**
in total. A byte-identical copy sits beside the creature, where it switches
`checkarchive.py` into its pass-20 phase.

**43 of the 44 differ from the pass-19 receipt. One does not, and the reason is
real.** `manafold-taunt3.png` is byte-identical to pass 19's poster. Taunt III's
poster is frame 324, and **222 of its 368 frames are unchanged by the rear bow**
— frame 324 is one of them. The clip as a whole did change (146 frames differ,
sequence CRC `0x75BC4777` → `0xC81598AA`); this one still frame did not. Checked
rather than assumed: the raw frames `0324.rgb` are the same bytes in the
shipping bank and in the all-switches-off bank. Both archive locks and the
pass-20 live receipt agree on that digest, which is the correct outcome.

## 3. The live-phase control on the real tree

The encoded tree was checked **before** the pass-20 receipt existed (the file
was moved aside, then restored): `checkarchive` **failed with RC 1 on 43 of the
44 live files** — "pass-19 live file differs from its receipt"
(`P20-MEDIA-CLOSURE-RECEIPTS/checkarchive-before-receipt.log`). The 44th is the
taunt3 poster above, which legitimately still holds the pass-19 bytes. So the
pre-encode phase really does refuse live names that are not the published
pass 19. With the receipt in place it returns RC 0.

## 4. Canonical no-skip local gate (`p20-media-gates.log`)

`deploy.ps1 -Project upheaval -Branch main -AssembleOnly`, **no skip flag**,
preceded by both selftests.

| Gate | Result |
|---|---|
| Selftests | checkarchive RC 0 (**fifteen** red legs), checkfresh RC 0 |
| Assemble | 2 creatures / **760** render entries |
| Playback | Fall is controls-only and non-looping; Hover autoplays and loops |
| Archive | v17 56/56 (70,576,645 B); v18 44/44 (44,743,343 B) equal to the published v18 receipt; **pass-19 44/44 (44,320,731 B) equal to the published pass-19 receipt**; live names match the **pass-20** receipt 44/44; 2×22 locked-generation clips, 22+6 v17 clips and 10 labs each declared once; archived playback is controls+loop, no autoplay, `preload="none"` |
| Robots | exactly `noindex, nofollow` |
| Freshness | **22 live fresh / 0 archived / 0 stale / 0 absent / 0 unknown** |
| Decode | **1,508 declared / 1,508 decoded**, with no skip (1,464 before, plus the 44 pass-19 archive files) |
| Overall | **DEPLOY_ASSEMBLEONLY_RC=0**; nothing deployed |

**Generated `website/public/index.html`:** 446,851 bytes, SHA-256
`9639786f20bd547dce3908c93d083cf5d49a245e821dac145f218bf4a1f345de`.
- It carries the card "MANAFOLD, pass 20" and the provenance: source
  `55767880`, renderer MD5 and manifest SHA-256.
- It shows "Archive (15 generations)" for Manafold and declares the 22
  `archive-p19` clips.

## 5. Copy

**`website/creatures.json`:**
- The live card is "MANAFOLD, pass 20". In plain owner language it covers the
  two items:
  - **the rear connection no longer tears or over-stretches** — the band is no
    longer told to SHORTEN when its ends come together, it BOWS instead, the
    way a real cord does, so nothing pulls the skin in with it;
  - **the new kneading beat** — the ball at the very middle of the top of the
    loop presses down far enough to become the lowest ball and comes back, about
    a second of press once a loop, on all 19 clips that host it;
  - **the mana answers the press** — the lightning figure and its motes flatten,
    spread sideways and ride down with it, read from the creature's own pose
    rather than from a timer.
- A PROVENANCE paragraph carries the exact bank receipts, the three-way scope
  result (22 / 21 / 3), and states plainly what the sheets could and could not
  judge — including that "B is strictly the lowest ball" is a measurement, not
  something claimed from a picture.
- The Hover caption and note, the Inspect, Channel, Drift, Nodule taunt and
  Trick notes, and the site note were all updated.

**Creature files:** `PASS-20-FINDINGS.md` (with its mid-pass header corrected
rather than quietly rewritten) and `P20-LIVE-MEDIA-SHA256.txt` are in
`Upheaval/creature/Manafold/`, beside `P19-ARCHIVE-SHA256.txt`.
