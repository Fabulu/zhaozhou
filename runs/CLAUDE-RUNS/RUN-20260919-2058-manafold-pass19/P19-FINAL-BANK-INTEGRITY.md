# Manafold pass 19: version-18 archive, exact final bank, and every-frame review

**Date:** 2026-09-19
**Worker:** Claude (sole Opus worker, no Qwen, no sub-agents)
**Verdict:** **READY-TO-ENCODE.** No fault was found. The exact bank reproduces the reviewed renders byte for byte. The scope is proven exact against version 18, and every frame was looked at on complete sheets, with close looks on the named witnesses.

**Source:** Zhaozhou `10877707` (`manafold-pass19`). `tools/reel` is clean and was last touched by `1eb115e4`, the review fix.
**Renderer:** `.tmp/p19-bank/bin/zhao-reel-cel.exe`
- MD5 `776d55758933d5284147b360ff2eda62`
- SHA-256 `eb10880f655f860306f2c46ed826ce391611b46e39343334fafdfc6df872354e`
- a clean direct build with g++ 16.1.0 (`P19-FINAL-RECEIPTS/binaries.txt`)

**Bank manifest SHA-256:** `f7edc1fbe25016970d6adc830fbea925954fb7d7282297478375c0690f036ed5` (22 subjects, 7,992 frames, 2,209,692,096 bytes)
**Raw frame root, kept for the encode:** `C:\programmieren\zencrifice\manafold-p16\p19-final-reel-22`

## 1. Version 18 archived first (Upheaval `e3e0a22`)

Before anything could overwrite a live name:
- The 44 live files were checked against the production-verified `V18-LIVE-MEDIA-SHA256.txt`: 44/44 exact.
- They were then copied to `archive-v18-manafold-*` and each copy re-hashed: 44/44, 44,743,343 bytes.
- The receipt is `V18-ARCHIVE-SHA256.txt`, beside the creature and in this run.

**Manifest.** One new archive generation, `Version 18 — 2026-09-19`, with a single `Version 18` collection declaring the 22 clips once each. It is inserted first, so it is newest. The archive note now reads FOURTEEN generations. Assemble: 738 render entries.

**`checkarchive.py` changes:**

1. It locks version 18 the way it locks version 17. That means exactly 44 rows, exact archive bytes, and archive paths that are the twins of their sources.
2. It **cross-locks** every version-18 archive row to the published version-18 live receipt, so the archive can only ever be the bytes that were verified in production.
3. It checks that the `Version 18` collection declares exactly the 22 archive sources.
4. It checks that the archived v18 videos use archive playback: controls, loop, no autoplay, `preload="none"`.
5. It moves the live-name phase one generation later:
   - Until `P19-LIVE-MEDIA-SHA256.txt` exists, every live name must still hold the published version-18 bytes.
   - Once it exists, that receipt alone judges them: 44 rows, no more, no fewer.
   - The version-17 lock (56 files) is unchanged.

**Selftest:** the valid contract passes, and **nine** red legs fire, up from five:
- an altered v18 archive file;
- a v18 archive row that is not the published bytes;
- a live file that differs from the pass-19 receipt;
- an undeclared v18 clip;
- the original five.

The first real-tree run gave RC 0: v17 56/56, v18 44/44, and the live names hold the v18 bytes 44/44.

## 2. The exact bank

**Render.** One invocation of the hashed renderer covered all 22 `kU02LiveSiteSubjects`, including Inspect.
- Environment: `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`, with `ZHAO_U02_LIVE_MIST` unset.
- **RENDER_RC=0**. Live-history is OK on all 22 subjects.

**Validation.** `P19-FINAL-RECEIPTS/p19-final-bank-validate.py` is the v18 validator. It checks headers, byte counts, contiguous names, the meta subject and frame count, and the per-frame receipt rows. Result: **PASS, 22 subjects, 7,992 frames, 0 errors.**

**Brief correction.** The brief said Inspect is not a site subject. It is: `manafold-inspect` is one of the 22 live names in both `kU02LiveSiteSubjects` and `creatures.json`. So the Inspect witness is the bank's own Inspect, and no separate render was needed.

| Subject | Frames | Sequence CRC32C | Ordered frame bytes SHA-256 |
|---|---:|---|---|
| manafold-blown | 292 | `0xCCAC7CAB` | `b14c46c935488912686e72fd8602e8cf84dddb105a97d14772c1647a4468f5bf` |
| manafold-channel | 420 | `0x8D8022A0` | `336e5a8c74d2ab7859e4ffd22cc316ac540e679029a35b842dcbd8e3ea6d3c06` |
| manafold-crackle | 600 | `0x43CA2CCD` | `009cdfbffdab16d9e790099afd251d9f17b0a8ae2275c31fd6c09eb313d236b7` |
| manafold-curious | 180 | `0x265BD173` | `174a5d3999429721585f99affa4dfc0c8ab195b2e201de4c0ccacf32d936352d` |
| manafold-damage | 464 | `0x28FAADC2` | `837b2da0c204b4126152f05cb2a80dd9a35ee4447527aad7bf76825c19f29252` |
| manafold-death-drop | 450 | `0xF4FF71A1` | `c51f78a79778e91edd365e2d125ef8802a8724cbc78d002ad76d24ecfd6e81e4` |
| manafold-death-gutter | 590 | `0x658B4D3D` | `7cfe3292747b61c7a02bd7e0f76796c61946d1a356f74a3f8393d27e2dc56160` |
| manafold-drift | 300 | `0x9A76FE69` | `c38986b90dc23863ba0de355e6dfab5d8ef12c392455506054724ce55d6aab56` |
| manafold-fall | 340 | `0x0DE69D1C` | `b175be3ba753b53d959c8f715ae47afe568751e48b1045d706da752dd494039b` |
| manafold-flight | 352 | `0x5A0897E1` | `8bf26dcb108a0a1e038f517c6044b3f6accbac0c90815ff9576b41cd31cd3749` |
| manafold-hasty | 240 | `0x14EFB828` | `7a09c405290a333a24d42c5834e631515491ff05ca9aed460ae3cf64290b7bc6` |
| manafold-hit | 140 | `0x39F4EE4F` | `2ddc344d322b399d004d2f2441390c1299592524c61d200202e4ca6c3f37cc3f` |
| manafold-hover | 600 | `0xA2D0E051` | `598ae201bc80a2aed601c354fbb934de74722cb437008f03493dc45f3e5850aa` |
| manafold-inspect | 600 | `0x779615BB` | `4e43aa738f253895b077347ff9f66b3af01cb42b2a8b49c6f275580fd3ba04c4` |
| manafold-lasso | 336 | `0xF6B893FE` | `c60883501b6fee54faf0f29d2ae232e95d7317ef9adfb0b104bcd1f0de9f6ee0` |
| manafold-pirouette | 240 | `0xE05DB5C3` | `3b0444e0a686b7db0bdf5c1e6f55f06e61d9b468dc97c671ee3b09d2139de48e` |
| manafold-rest | 400 | `0xA8B3AD33` | `b279fb87ae2fa8f06601cad46a0180c81da5fd798ba1d60002212e06aa820fc7` |
| manafold-startle | 160 | `0xFD813D7E` | `7e11071f591e463cd53c854f7e0e42a483ccd281a402a6131395ca7a3fd22e16` |
| manafold-taunt | 280 | `0x3F19A505` | `f045bd604bf1a809ca0f119170d9e99799c725c833109232fae0ed9f27edcc6f` |
| manafold-taunt2 | 240 | `0xE2F2232F` | `ce693e196a4882ebbc95cf43477588359e59b25073b1f072ecd50212f5319e84` |
| manafold-taunt3 | 368 | `0x75BC4777` | `5d73de28d5fa1ebe89291c58d77050f3e667c85bd9c29aad7e434439770a7193` |
| manafold-trick | 400 | `0x71BB47B9` | `16469e3197b26c0aa57632ea23f648419089d20e42b6722be83d8c653350d615` |

**Identity with the reviewed build.** The bank binary's MD5 differs from the review binary (`510fab16…`). That binary had already been deleted, so the two could not be diffed. The build does not embed its output path (checked), and the probable cause, the PE link timestamp, is not proven. What IS proven: the bank reproduces **all nine** reviewed shipping CRCs exactly (Hover, Inspect, Drift, Rest, Taunt III, Channel, Trick, Lasso, Pirouette; `scope-vs-v18.txt`). So what ships is byte-for-byte what the review looked at.

## 3. Scope against version 18: exact, and attributed

`P19-FINAL-RECEIPTS/scope-attribution.txt`. Two more complete 22-subject banks were rendered from the **same binary** and environment:

| Bank | Toggles | Result |
|---|---|---|
| **legacy** | `REAR_SOCKET_FRAME=legacy-root` + `MANA_LINE_SCALE=legacy` (the End ball stands down under legacy-root) | **22/22 byte-identical to the version-18 bank.** Every byte that differs from version 18 is therefore produced by the pass-19 mechanisms and nothing else. |
| **line-legacy** | shipping rear + `MANA_LINE_SCALE=legacy` | Differs from shipping on **21/22** subjects. That is where distance-scaled lines changed pixels. The exception is **Inspect**: its projected radius is at or above the 360 px full-width point, so its lines are the legacy width by construction. |
| line-legacy vs legacy | isolates the rear change | Differs on **22/22**. The End frame is structural, so every clip's rear changed. |

All 22 subjects changed against version 18, as intended: 22 through the rear, 21 through the lines.

## 4. Review

The notes were written after each look: `P19-FINAL-RECEIPTS/look-notes.md`. 19 images were read, all <=1600 px JPEG. The sheets are `P19-BANK-SHEETS/` (22 viewing JPEGs), and the close looks are `P19-FINAL-LOOKS/`. Where a comparison needed version-18 frames, it used the legacy bank, which §3 proves is byte-identical to version 18. The version-18 raw root had been deleted after its production verification.

| Subject | Verdict | Basis |
|---|---|---|
| Inspect | **PASS** | Whole sheet; 2x whole frames at f20/218/300/340; press-snap window f208-222 v18/p19 at about 2.6x. v18's stepped bracket flicks at the rear entry, while p19 is one steady strut with a rounded End ball. |
| Drift | **PASS** | Whole sheet; 4x v18/p19 f140-170. The blob becomes a fine white-cored loop at outline weight. Motes are unchanged. |
| Rest | **PASS** | Whole sheet; 3x v18/p19 f338/342. The stub nub is gone. |
| Trick | **PASS** | Whole sheet (plant, 360, righting); 3x v18/p19 f389/393. The notch is gone. |
| Taunt III | **PASS** | Whole sheet; 3x v18/p19 f320/328. The stub is gone and the lines are slightly finer. |
| Hover | **PASS** | Whole sheet and seam; 3x v18/p19 swallow f372/390. The bracket is gone, and the swallow reads as a rounded elbow (the accepted authored beat). |
| Channel | PASS at sheet scale | Whole sheet: night kept, figures continuous. |
| Lasso | PASS at sheet scale | Whole sheet: throw, off-edge flight, return. Its rear at close range was judged by the review, and the bytes are identical. |
| Hit, Startle, Curious, Taunt II | PASS at composite scale (about 67 px tiles) | Continuous. Taunt II's loop crosses the right edge as before. |
| Hasty, Pirouette, Taunt | PASS at composite scale (about 63 px) | Continuous, and no smear on Hasty. |
| Blown, Fall | PASS at composite scale (about 78 px) | Continuous. Fall lands and holds, which is the one-shot shape. |
| Flight, Death Drop | PASS at composite scale (about 61 px) | Continuous climb and sink; fall and hold. |
| Damage, Death Gutter | PASS at composite scale (about 46 px, the coarsest) | Continuity and framing only. |
| Crackle | PASS at sheet scale | Day sky, continuous. |

**What sheet scale could not judge.** The rear joint and the line weight on the 14 composite-only subjects. For those, the evidence is:
- the exact scope proof in §3;
- the review packet's 128/128 gate matrix: the rear audit R1/R2/R3 with its line census, mspan, mjointpub, msmooth and mqa;
- the six close-looked subjects above, which carry the same mechanisms.

## 5. Receipts

`P19-FINAL-RECEIPTS/` contains:
- `binaries.txt`, `bank-render.log`, `validate.log`, `bank-integrity.json` and `bank-manifest.tsv`;
- `scope-vs-v18.txt`, `scope-attribution.txt`, `scope-legacy-render.log` and `scope-linelegacy-render.log`;
- `look-notes.md`;
- the tools: `p19-final-bank-validate.py`, `banksheets.py`, `p19crops.py`, `compose.py`.

In this run: `V18-ARCHIVE-SHA256.txt`.
