# Manafold pass 20: pass-19 archive, exact final bank, and every-frame review

**Date:** 2026-09-20
**Worker:** Claude (sole Opus worker, no sub-agents, no Qwen)
**Verdict:** **READY-TO-ENCODE.** No fault was found in the creature. The exact
bank reproduces the re-review's four verified CRCs byte for byte, the scope is
proven exact against pass 19 on all 22 subjects, and every frame was looked at
on complete sheets with close looks on the rear join, the knead and the
particle reaction.

**Two findings, both about INSTRUMENTS and neither about a shipped byte**, are
in `P20-FINAL-RECEIPTS/scope-finding.md` and §4 below.

**Source:** Zhaozhou `5576788009e343317c768666e32f9218f4321e6f` (`manafold-pass20`,
the re-review head). `tools/reel` is clean.
**Renderer:** `.tmp/p20-bank/bin/zhao-reel-cel.exe`
- MD5 `e95faca916627d1bddb02892c5eb67e1`
- SHA-256 `59c81490c7046be46062ac48a2118ff881676fa95dc419450a3edbb5b297560e`
- a clean direct build with g++ 16.1.0 (`P20-FINAL-RECEIPTS/binaries.txt`)

**Bank manifest SHA-256:** `a40b41549383246d7c9580c768c936f8919eb810dce7c0ecae24e3cdb1313b15`
(22 subjects, 7,992 frames, 2,209,692,096 bytes)
**Raw frame root, kept for the encode:** `C:\programmieren\zencrifice\manafold-p16\p20-final-reel-22`

## 1. Pass 19 archived first (Upheaval `495ac1e`)

Before anything could overwrite a live name:
- The 44 live files were checked against the production-verified
  `P19-LIVE-MEDIA-SHA256.txt`: **44/44 exact**.
- They were then copied to `archive-p19-manafold-*` and each COPY re-hashed:
  **44/44, 44,320,731 bytes** — the same total the pass-19 production
  verification recorded.
- The receipt is `P19-ARCHIVE-SHA256.txt`, beside the creature and in this run.

**Manifest.** One new archive generation, `Pass 19 — 2026-09-19`, with a single
`Pass 19` collection declaring the 22 clips once each. It is inserted first, so
it is newest. The archive note now reads FIFTEEN generations. Assemble: 760
render entries, up from 738.

**`checkarchive.py` changes:**

1. It locks pass 19 the way it locks version 18: exactly 44 rows, exact archive
   bytes, archive paths that are the twins of their sources.
2. It **cross-locks** every pass-19 archive row to the published pass-19 live
   receipt, so the archive can only ever be the bytes production served.
3. It checks that the `Pass 19` collection declares exactly the 22 archive
   sources, and that those videos use archive playback (controls, loop, no
   autoplay, `preload="none"`).
4. It moves the live-name phase one generation later: until
   `P20-LIVE-MEDIA-SHA256.txt` exists, every live name must still hold the
   published pass-19 bytes; once it exists, that receipt alone judges them.
5. The two locks are now ONE table (`LOCKED`) rather than two copies of the same
   code, and `LIVE_PHASES` is the phase ladder. Adding a generation is one row
   plus one receipt.

**A red leg that had stopped testing what it named.** The selftest found its
collections by NEGATIVE INDEX into the fixture. Appending the pass-19 collection
silently moved every one of those legs onto a different row — they would all
still have passed, while testing the wrong thing. They are found by label now.
That is the detector-reading-zero law in a test fixture.

**Selftest:** the valid contract passes and **fifteen** red legs fire, up from
nine. The four new ones are an altered pass-19 archive file, a pass-19 archive
row that is not the published bytes, a pass-19 archive path that is not its
source's twin, and a live file that differs from the pass-20 receipt; plus an
undeclared pass-19 clip and a missing pass-19 collection.

Real tree, real exit codes: assemble RC 0, checkarchive RC 0 (v17 56/56 /
70,576,645 bytes; v18 44/44 / 44,743,343; pass-19 44/44 / 44,320,731; live names
still holding the published pass-19 bytes 44/44), checkplayback RC 0.

## 2. The exact bank

**Render.** ONE invocation of the hashed renderer covered all 22
`kU02LiveSiteSubjects`.
- Environment: `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`, with
  `ZHAO_U02_LIVE_MIST` unset and **no override of any kind**.
- **RENDER_RC=0.** Live-history is OK on all 22 subjects.

**Validation.** `P20-FINAL-RECEIPTS/p20-final-bank-validate.py` (pass 19's
validator) checks headers, byte counts, contiguous zero-based names, the meta
subject and frame count, and the per-frame receipt rows. Result: **PASS, 22
subjects, 7,992 frames, 0 errors.**

**The re-review's four verified CRCs all reproduce exactly:**

| | recorded by the re-review | this bank |
|---|---|---|
| hover | `0x93B95AEE` | **`0x93B95AEE`** |
| inspect | `0xD00478A1` | **`0xD00478A1`** |
| taunt3 | `0xC81598AA` | **`0xC81598AA`** |
| blown | `0xC6AAF7AD` | **`0xC6AAF7AD`** |

So what ships is byte-for-byte what the independent review looked at.

| Subject | Frames | Sequence CRC32C | Ordered frame bytes SHA-256 |
|---|---:|---|---|
| manafold-blown | 292 | `0xC6AAF7AD` | `9099399ebb8b5c6e36aba1633bedc9ab8e2015cba640ef4393bf3d61a45a0352` |
| manafold-channel | 420 | `0x0FC52D19` | `7eb7616867dacf411955241ef9269d49c6dc6867fff36088e7e92faf0b7bd0b8` |
| manafold-crackle | 600 | `0xC6D1F16C` | `b87a67543e9e4d6e6936d0217f529a442baea80bb31923e380516c3e52946f7e` |
| manafold-curious | 180 | `0xED285AB6` | `9553c258c67987821bb79c2fb68b1373ef0ad1aeb2c37673b521c156648a1de5` |
| manafold-damage | 464 | `0x5D486AE7` | `d3aa57bf2a3ecc85cbe3f0beb77a49113ef6118dc79df44454cba092b5a78a49` |
| manafold-death-drop | 450 | `0xCBA3990E` | `4e923df59d322721686e1e77c91ae5c9196355a5f84f2f0b5b13ab06a549ba86` |
| manafold-death-gutter | 590 | `0x186AA00F` | `1a488afee8fbf5970dffcdf4a0e304bb31692a465d300c4e831ae845dfc204b2` |
| manafold-drift | 300 | `0xB4855BA7` | `fd15991ce860bb6fe13593e02fd111467fc4a2616d56e97206749105a89d2038` |
| manafold-fall | 340 | `0x02518063` | `bedc6c8b34fc847de3e1316b2109117cd5bd043b4cfe9f705a103a1c49742c7a` |
| manafold-flight | 352 | `0xFD8E3AE8` | `079a325d6e8f0e4e17d52ce5679a749c2358fdaf49e362aa1ba78e677c04707e` |
| manafold-hasty | 240 | `0xEB7D6F85` | `eca9f06476d8a6417977a94e1ee5bdda649f9624d91613b333c1c7823bae750d` |
| manafold-hit | 140 | `0xF64B887F` | `4d3c9783216120161fc0f7e9a784c505ead17644d049ce6c2391174cc7e0cbfa` |
| manafold-hover | 600 | `0x93B95AEE` | `bea0dad97fff3dece36593bd49d5759709a7ce08f017de70b51c4fb9926ab157` |
| manafold-inspect | 600 | `0xD00478A1` | `2e1edb7bf988fbfd7ee9337f6e7552e474d7abe7043a6bc63a79563b0b6cd2fd` |
| manafold-lasso | 336 | `0xA3D16C94` | `629afb4fc20a470052b2244cca4525b14e77e95d8f9f769fc923a2657404d62f` |
| manafold-pirouette | 240 | `0xC5B176F8` | `8338def46edaec549fde2757b5e63d9a85e8a053ba755990d77090a7daaea784` |
| manafold-rest | 400 | `0xEB3FB76D` | `1c09cf313bf573f19c5fe804379750d1b103a28c02010ab35eea9f4b542d6b0e` |
| manafold-startle | 160 | `0xD64F3B15` | `684f0b9434beacf07347dfa826a16a2e5337df3d794e10ef993fe1c6884cd158` |
| manafold-taunt | 280 | `0xBF2461C3` | `b0a3d674d2aaea9b4f8014eeb3896a3f61b2afad33a326ac60bd2aa58346427e` |
| manafold-taunt2 | 240 | `0x46D3619D` | `1701f4c0fca0ed40ed3705a9a64a773d1baaf005a04c7049947eb690af4a9f58` |
| manafold-taunt3 | 368 | `0xC81598AA` | `7e45f4a12297fd6a2930c7497458c1d20d8d6ce0d47546ecbf93054b02902842` |
| manafold-trick | 400 | `0xABAB51E1` | `53b925ed25493e1ec13e55947f2842e743db18268a3b7707a976d284106a71e8` |

## 3. Scope against pass 19: exact, and attributed

`P20-FINAL-RECEIPTS/scope-attribution.txt`. Three more complete 22-subject banks
were rendered from the **same binary** and environment. The comparison is the
per-subject ordered-frame-bytes SHA-256 — the same quantity pass 19 recorded —
so the pass-19 leg is checked against pass 19's own committed table. (Pass 19's
raw root was deleted after its production verification, exactly as the process
says it should be.)

| Bank | Toggles | Result |
|---|---|---|
| **pass19** | `KNEAD_DIP_PM=0` + `REAR_BOW=legacy` + `FOLD_DIP_PM=0` | **22/22 byte-identical to pass 19.** Every byte that differs from pass 19 is produced by the three pass-20 mechanisms and nothing else. |
| **rearonly** | shipping bow, no beat, no reaction | differs from pass19 on **22/22**. The rear repair is structural, so every clip's rear changed. |
| **reaction-only** | `KNEAD_DIP_PM=0` only | differs from rearonly on **3/22** — Blown, Fall, Trick. |
| shipping vs rearonly | the beat and its reaction | differs on **21/22**. The exception is **Taunt III**, which authors no dip, exactly as declared. |

All 22 subjects changed against pass 19: 22 through the rear, 21 through the
beat, 3 through the reaction answering their own ambient motion.

## 4. The two findings, and why they are not blockers

Both are in `P20-FINAL-RECEIPTS/scope-finding.md` in full.

**(a) The "pass-19 identity" leg was missing an operand.** All pass, the claim
was that `KNEAD_DIP_PM=0 REAR_BOW=legacy` reproduces pass 19 byte for byte. It
was only ever checked on Hover, Inspect and Taunt III. Measured on all 22, that
two-switch leg reproduces pass 19 on **19 of 22**: **Blown, Fall and Trick
differ.** Pass 20 has THREE exact-off switches, not two — the third is the
particle reaction, `ZHAO_U02_FOLD_DIP_PM`, whose own constant block says so. It
is read from the POSE rather than from the dip's schedule, on purpose, so it
answers any authored dip; those three clips sag carrier B past the 90 mm onset
on their own. With all three off, all 22 reproduce pass 19 exactly.

This is the pass's own pattern once more, and the fifth instance of it: a
comparison wired to fewer operands than the thing it compares, sampled on the
three clips that could not fail. **Repaired**: `e-identity-pass19` now carries
the third switch, a new `e-identity-pass19-ambient` leg witnesses on the three
clips that discriminate, and the old two-switch configuration is kept as
`e-reaction-ambient-live`, a positive control that must NOT equal pass 19.

**(b) A code comment overstates one example.** `manafold_fx.h` says Taunt III's
crown shuffle rouses the fold too. Measured over all 368 frames it does not:
Taunt III is `0xC81598AA` with the beat on, with it off, and with the reaction
off. Its B-lowest reading is entirely its own crown shuffle, as the re-review
proved. The true list of clips the reaction answers without a beat is Blown,
Fall and Trick, and it is recorded beside the claim.

**Neither changes a shipped byte**, and the shipping bank reproduces all four
re-review CRCs.

## 5. Review: every frame

Notes were written after each look, before the next image was requested:
`P20-FINAL-RECEIPTS/look-notes.md`. **20 images**, all ≤1600 px JPEG q80. The
sheets are `P20-BANK-SHEETS/` (22 every-frame PNGs plus viewing JPEGs) and the
looks are `P20-FINAL-LOOKS/`.

**All 22 subjects, all 7,992 frames, looked at on complete every-frame sheets.**
No missing, blank, black, torn or half-drawn frame anywhere in the bank, and no
discontinuity at any row break or loop seam that sheet scale can resolve.

| Subject | Verdict | Basis |
|---|---|---|
| Inspect | **PASS** | whole sheet (600); rear join at 2x whole-frame and 5x on f380 vs pass 19; knead at NATIVE f250/280/310 and 3x loop top; mana at 3x |
| Hover | **PASS** | whole sheet (600); knead at NATIVE f500/532/560 and 3x loop top vs beat-off; mana at 3x; loop seam f599→f0 |
| Channel | **PASS** | whole sheet (420); rear join at 2x and **6x on f080** vs pass 19; reaction at 2x on f212 |
| Drift | **PASS** | whole sheet (300); reaction at 2x on f165 vs beat-off |
| Taunt III | **PASS** | whole sheet (368); the crown shuffle reads; byte-identical with the beat off, as declared |
| Blown | PASS | composite sheet (292); reaction at 2x on f166 — the weakest read of the beat, and honestly so |
| Crackle, Death II/death-gutter, Hits/damage, Death/death-drop | PASS at sheet or composite scale (46–78 px tiles) | complete, contiguous, continuous, framing holds |
| Trick, Rest | PASS at composite scale (~62 px) | Trick's plant holds through the full 360; Rest is a steady idle |
| Flight, Mana lasso, Fall | PASS at composite scale (~47 px) | climb/hold, throw-and-return, land-and-hold |
| Taunt, Lasso/taunt2 | PASS at composite scale (~60 px) | continuous; taunt2's loop crosses the right edge as before |
| Hasty, Pirouette, Curious, Startle, Hit | PASS at composite scale (~51 px) | continuous, no smear on Hasty, framing holds |

**Frames were chosen by BADNESS, not by index.** `P20-FINAL-RECEIPTS/dipframes.py`
differences the shipping bank against the beat-off control per frame; the
largest is the deepest press. Inspect f280 (16,654 px of 92,160 moved), Hover
f532 (12,828), Channel f212 (9,557), Blown f166 (3,549), Drift f165 (2,687),
**Taunt III 0 of 368** — which is the scope table agreeing with the pictures.

### What the looks say

* **The rear connection.** At 6x on Channel f080 — the exact 361 mm / 113°
  frame — pass 19 shows a narrowed band that constricts further just above the
  junction and meets the body at a hard V. Pass 20 has one continuous fused
  trunk: the ink runs down the band and sweeps into the body's shoulder in a
  single curve. At 5x on Inspect f380 pass 19 shows a splayed wedge with a
  notch and a step in the fill; pass 20 has neither. **The tear and the
  over-stretch are gone, and it is visible rather than inferred.**
* **The knead, at 384×240.** Up, press, up. With the beat off the loop's top is
  a clean arch; with it on the middle of that run is driven down into a deep
  trough almost to the body while the shoulders stay up. It is a different,
  deliberate shape, not a wobble.
* **The mana reacts.** At the press the lightning figure sits lower and spreads
  sideways, and the motes go with it — the field is squeezed, not restyled.
  Clearest on Channel and Inspect, correctly small on Drift.

### What sheet scale could NOT judge, stated plainly

* The rear joint, the ball ranking, line weight and the particle reaction on
  every subject that was only seen at composite scale. Those rest on the exact
  scope proof in §3, the gate matrix, and the six subjects looked at closely.
* **"B is strictly the lowest ball" is not a claim this review makes from a
  picture.** A silhouette shows the middle-top ball pressing down to become the
  low point of the loop; the full 3D ranking against the outer balls is R5's
  (19 of 19 hosting clips, worst margin +37 mm), re-measured independently in
  the re-review. The pictures are consistent with it and do not establish it.

## 6. Receipts

`P20-FINAL-RECEIPTS/` contains `binaries.txt`, `bank-integrity.json`,
`bank-manifest.tsv`, `validate.log`, `scope-attribution.txt` / `.json`,
`scope-finding.md`, `scope.log`, `pass19-recorded-bank.tsv`, `dipframes.txt`,
`look-notes.md`, `sheets.log`, and the tools `p20-final-bank-validate.py`,
`scope.py`, `dipframes.py`, `banksheets.py`, `compose.py`, `p20crops.py`.
`P20-ARCHIVE-RECEIPTS/` holds `archive_p19.py` and its log. In this run:
`P19-ARCHIVE-SHA256.txt`.
