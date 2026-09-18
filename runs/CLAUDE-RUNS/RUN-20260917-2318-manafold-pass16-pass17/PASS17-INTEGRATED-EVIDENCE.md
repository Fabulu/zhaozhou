# Manafold Pass 17 final 23-bone integrated evidence

**Date:** 2026-09-18
**Renderer:** MD5 `F7C2DAA085661C1E00CADF09C65B5F8A`
**Environment:** `zhao-env.ps1`; `ZIXX_EXP=celmain`; `ZIXX_LIGHT=diagonal-cool-cross`
**Build:** clean direct `cel`, then direct `mspan`, `mmeshcheck`, `mnodule`, `mprobe`, `moutline`, `mqa` in `out/p17-final23-evidence/`

## Focused gate matrix

| Gate | Normal | Controls |
|---|---:|---|
| `mspan` | RC 0 | 20/20 rigid/clamp/drift/overcompact/stage/posed-order mutants RC 1 |
| `mmeshcheck` | RC 0 | — |
| `mnodule` | RC 0 | existing attributed controls retained |
| `mprobe` | RC 0 | existing mirror/detached-outline/scale-inverse controls retained |
| `moutline` | RC 0 | no-opening-owner RC 1; forced-post-repaint RC 1 |
| Fall Q5 | `--fall-only` RC 0 | `--fail-fall-wrap` RC 0 after Q5 independently witnesses the expected failure |

`mspan` reads the actual compiled/decoded shipping bank:

- 23/32 bones, seven translation-only helpers;
- 668 loop vertices with zero ownership/weight mismatch;
- 13,158 retired-lane samples, all zero;
- 0/7 zero-delta helper palette differences;
- zero authored/staged-fraction/bound/margin breaches;
- all four spans carry both positive and negative signed deltas;
- 307,020 actual posed ring steps over every key and midpoint: zero reversals, zero pinches; minimum projection 6.891 mm and minimum separation 7.442 mm at slot 0 key 79 sub 0 C-End;
- rear helper/socket/tip closure remains inside its fixed-point tolerance.

The omnibus `mqa` remains RC 1 on separate historical Q2/Q3 items in this recorded final23 evidence binary; Fall Q5 is isolated so those failures cannot certify it. `PASS17-Q2-Q3-REPAIR.md` and `PASS17-Q2-Q3-REVIEW.md` later close those omnibus debts without changing the span/outline evidence recorded here.

## C-End collapse repair — PASS

A fresh renderer rebuilt after the final 23-bone posed-order repair re-rendered the exact five failures and their complete ±12-frame windows:

| Subject | Exact witness | Sequence CRC32C | Verdict |
|---|---:|---:|---|
| Hover | f0157 | `0x1E9FD4B3` | PASS |
| Channel | f0160 | `0xB89B63D1` | PASS |
| Rest | f0164 | `0xCF41AC99` | PASS |
| Flight | f0202 | `0xE5F2C25B` | PASS |
| Crackle/fixed idle | f0157 | `0x7F3022F7` | PASS |

Every frame in the five 25-frame windows was inspected. The former broad diagonal/buckled C-End shoulder is gone. Each sequence retains a distinct smooth return stick and End carrier, open O, continuous non-beaded skin, body-attached socket, and no crack, local reversal, pinch, sliding core or detached tip. Native and exact nearest-neighbour 4x before/after evidence corroborates the production-path ring walk.

Final evidence:

- `PASS17-SPAN-CEND-FINAL23-NATIVE.png`
- `PASS17-SPAN-CEND-FINAL23-4X.png`
- `PASS17-SPAN-CEND-FINAL23-BEFORE-AFTER-4X.png`
- `PASS17-SPAN-CEND-FINAL23-{HOVER,CHANNEL,REST,FLIGHT,CRACKLE}-WINDOW-5COL.png`

The older `PASS17-SPAN-CEND-REPAIR-PROVISIONAL-*` and 22-bone outline-integrated images are stale checkpoints, not acceptance evidence.

This closes the C-End structural-collapse blocker. Direction 16 as a whole remains open until public A/B/C art gives every carrier both top and bottom ordering positions and the corrected targets are re-reviewed.

## Direction 17 complete O outline — PASS

From this same final binary:

- `moutline` normal/mutants: `0/1/1`;
- fixed/quarter/Channel normal CRC32C: `0x8F08D358` / `0x2CC13DC4` / `0xB89B63D1`;
- all 1,260 normal frames retain an open negative-space O and complete coherent lower/inside black boundary;
- the accepted upper/head-top line remains when unobscured;
- no-opening-owner control removes only the lower/inside ownership while the O remains open and the top line remains;
- forced-post-repaint visibly draws stale black through nearer cyan lightning, while normal correctly leaves lightning in front (clearest Channel f0345);
- explicit `ZHAO_U02_OUTLINE_CONTROL=none` is 577/577 Zixxtrixx Idle files byte-identical to unset.

Final evidence:

- `PASS17-OUTLINE-FINAL23-{FIXED,QUARTER,CHANNEL}-ALLFRAMES.png`
- `PASS17-OUTLINE-FINAL23-OPENING-AB-{NATIVE,4X}.png`
- `PASS17-OUTLINE-FINAL23-EFFECT-AB-{NATIVE,4X}.png`

Direction 17 is closed on the final integrated binary.

## Fall — source/site/art closed, media pending

Q5 proves the f338/f339 final partner holds resolved root XYZ, every bone quaternion, local translation, uniform scale, primary deformation and all extra lanes. The same-binary legacy-wrap control independently fires. Isolated visual review confirms grounded recovery instead of the rejected restart. Upheaval retains `loop:false`, controls-only playback and corrected copy (commits `851be73`, `b3b0fa2`). Final Pass-17 encoding, freshness/decode and production-byte verification remain delivery work.

## Commands

Representative build:

```powershell
. .\tools\env\zhao-env.ps1
& "C:\Program Files\Git\bin\bash.exe" tools/reel/build-direct.sh `
  --output out/p17-final23-evidence --clean cel
```

Each focused direct target was then built into the same caller-owned output. Gates were invoked directly with logs under `out/p17-final23-evidence/`; renders used the hashed binary with the environment above and subjects named in the tables. `tools/reel/plates.py` produced all committed evidence without interpolation.

## Disposition

No commit, push, encode, deployment or art-value change occurred in this evidence pass. The next work is public A/B/C top/bottom ordering, Trick axis selection, `taunt3` face/punchline repair and integrated eye-expression review before the full 28-subject bank.
