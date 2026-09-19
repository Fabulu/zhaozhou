# Manafold version 18: integrated gate and exact final bank

**Date:** 2026-09-19
**Worker:** Claude (sole Opus worker)
**Verdict:** **READY-TO-ENCODE.** No blocking item. The owner calls in §6 are recorded but do not block the encode.

**Source:** Zhaozhou `db2bcf0e` on `manafold-v18`. This commit adds the Trick plant pin; every other file is Wave F (`d7d51171`/`bf51325f`).
**Renderer:** `.tmp/v18-int-final/bin/zhao-reel-cel.exe`
- MD5 `0f082622d4ca0c58d012d1f0de555723`
- SHA-256 `52596ea411e57c2a7a2474dc39b39c76834e380d59e401a16760bb67d015ed03`

**Bank manifest SHA-256:** `bdaac548e5dc956b7aa4afac57dae73e265861b25e3160788a26cbd8aa0628bd` (22 subjects, 7,992 frames, 2,209,692,096 bytes).
**Raw frame root, kept for the encode:** `C:\programmieren\zencrifice\manafold-p16\v18-final-reel-22`
**Full-resolution every-frame sheets:** `C:\programmieren\zencrifice\manafold-p16\v18-final-sheets`. The viewing JPEGs are committed in `V18-FINAL-SHEETS/`.

## 1. Part 1: the Trick planted-antenna slide

**Problem.** The balance wobble and the JunctionF X balance flex rotate the body about its root. The plant was pinned in height only, so carrier B's contact skated 178.5 mm through the pause.

**Change.** The support-point XZ pivot, built in Wave F for the spin, now runs across the whole contact window, keys `[kTrickPlantKey, kTrickLiftKey)` = 78..147:
- At every planted key, the root XZ absorbs the horizontal displacement of carrier B's chain point from where it touched down.
- The spin compensation is added on top of the pin, unchanged.
- At the lift, the offset held on the last planted key is released to zero with a C2 quintic over `kTrickPinReleaseKeys = 12` keys (0.4 s), riding the righting.
- The wobble's rotation, the pause timing (78..100), the flex and the timing of the spin, overshoot and correction are all untouched.

**Toggle.** `TrickPlantPin {kPinned, kLegacy}`, selected by `ZHAO_U02_TRICK_PLANT_PIN=pinned|legacy`, strict (RC 2 on a bad value). `ZHAO_U02_TRICK_PIN_RELEASE_KEYS` accepts 2..30. `legacy` is the exact-off control and reproduces Wave-F Trick `0xBF695C69` byte for byte.

**Gate.** mqa **Q6d PLANT PIN** checks the no-spin contact patch (height-weighted carrier-B vertices, the same estimator as Q6c) against touchdown on every key 78..147. The bound is **24 mm**: about 1.5 px at the Trick camera, 1.85x the measured roll of the contact round the curved swell, and 7x under the control. The reason is written in the source.

| Measure | Value |
|---|---|
| Shipped Q6d | **12.99 mm** (key 131) |
| Shipped contact vs its own touchdown (reported) | 13.89 mm |
| Positive control `--fail-trick-plant-pin` (legacy) | **178.46 mm**, fires Q6d only (attributed) |

Q6b is now judged on the spin's own compensation: the shipped root XZ minus the no-spin control's, because since the pin the control's root also moves in XZ. The spin join ratios are unchanged from Wave F: 0.009 / 0.017 / 0.109. Q3's worst Trick step is 172.7 mm, under the 240 mm ceiling.

**Decision by eye: ship the pin.**
- `V18-FINAL-TRICK-PIN-AB-NATIVE.jpg`: legacy and pinned at native scale, from touchdown through the lift.
- `V18-FINAL-TRICK-PIN-AB-CONTACT-SLITSCAN.jpg`: the contact band for every frame f140..f319, stacked. In legacy the whole contact weaves left and right through the pause and slants through the hold. Pinned, it stands nearly vertical.
- `V18-FINAL-TRICK-PIN-AB-RELEASE-NATIVE.jpg`: at f290..f322 the pinned body sits a few px left of legacy and converges by about f314. There is no pop.
- `V18-FINAL-TRICK-PLANT-3X.jpg`: final-bank 3x crops of f156..f298 against a fixed screen column. The crown holds its column on the dirt through the pause, the turn and the hold, and leaves only at the lift.

The body still sways visibly. The pinned version reads as balancing on a planted tip, and the legacy version as sliding. It does not read worse, so the pin ships and the toggle stays.

## 2. Part 2: the integrated gate

**Build.** Clean direct build (`build-direct.sh --output .tmp/v18-int-final --clean cel`, then the 11 gate targets) from `db2bcf0e` with `tools/reel` clean, g++ 16.1.0. All binaries are listed in `V18-FINAL-RECEIPTS/binaries.txt`. The same build supplied the gates and the bank renderer.

**Not run: CMake.** The plan lists "direct/CMake builds". CMake was not run and no CMake result is claimed. Wave D recorded that the generated graph cannot rebuild itself (missing Verilator `*.cmake` copy inputs), and nothing here touches RTL.

**Gate matrix: 113/113** (`V18-FINAL-RECEIPTS/gate-matrix.txt`).
- **Normals, 11/11 RC 0:** mspan, msmooth, mprobe, mjointpub, mqa, mmeshcheck, moutline, mshell, mshell selftest, mnodule, meyesize.
- **mspan controls, 35/35.** Each fires its own attributed detector.
- **msmooth controls, 16/16.** Each fires inside its allowed mask.
- **Protected legs, 27/27.** These are the mjoint and mnodule mutes, the mprobe mirror/outline/scale/support/support-depth legs, the moutline self-tests, the six mqa legacy legs, and meyesize L/R/wrong-bone.
- **Wave F and integration controls, 5/5**, each firing only its own category: Q6a spin-gain, Q6b spin-ease, Q6c spin-pivot, **Q6d plant-pin**, Q7 flight-seam.
- **Selectors, 16/16 RC 2:** Wave F's 14 plus the plant-pin and pin-release selectors.
- **Live-history gate:** normal 22/22 OK; the `legacy` control fires on every live subject; the `list-drift` control fires.

**Exact-off identities** (`identity-renders.txt`), all rendered with the final binary:

| Identity | CRC | Matches |
|---|---|---|
| Flight, v17-neutral knobs | `0x5B272AB3` | Wave E |
| Trick, `SPIN=none` + `PLANT_PIN=legacy` + v17 camera (k 360000, bias 0) | `0x22563A37` | Wave E |
| Trick, `PLANT_PIN=legacy` | `0xBF695C69` | Wave F bank |
| Drift, `CAM_BX=0` | `0x3A11AB0C` | Wave E |
| crackle-legacy | `0xEDDC80D7` | Wave F |
| Zixxtrixx Idle, production env | `0x1408F885` | HEAD |

**The two gates Wave F loosened:**

1. **mqa Q3 Trick root-step ceiling of 240 mm.**
   - **Declared:** a per-slot table row for slot 13 only, with its reason in the source.
   - **Bounded:** the measured step is 172.7 mm, at peak turn speed (key 111). 240 is 1.39x that.
   - **Cannot hide a fault elsewhere:** every other clip keeps its own ceiling or the 135 mm default, and the `--fail-rootstep` teleport control still fires.
   - **Residual:** a Trick-only teleport between 135 and 240 mm would pass Q3. Inside the contact window, Q6b (C2 joins), Q6c and Q6d (support drift) now cover root XZ. Outside it the Trick root is authored Y plus a C2 release. **Acceptable.**
2. **mprobe TRAVEL float gate leaving Trick's declared contact keys (78-2..148+2) to the contact contract.**
   - **Declared:** slot 13 only, with a comment in the source.
   - **Bounded:** those keys are still judged by the contact contract. The deepest vertex is -36 mm (accepted -60..-5), and carrier B owns 140/140 support samples plus 82/82 during the turn. The float gate still judges every airborne Trick key (worst 68 mm) and every other clip.
   - **Residual, not blocking:** the contact contract assumes the snap plane. The TRAVEL row prints the terrain rise under the moved root on every key, and today it is **0 mm** on the whole path. It is printed but not gated inside the exempt window. A later change that puts Trick on sloped ground would have to gate it.

## 3. Part 3: the exact final bank

The single hashed renderer received all 22 live names (`kU02LiveSiteSubjects`) in **one invocation**, with the production environment `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross` and `ZHAO_U02_LIVE_MIST` unset. Renderer RC 0.

`V18-FINAL-RECEIPTS/v18-final-bank-validate.py` is the P17 validator narrowed to the 22 live subjects. It checks:
- every header, every byte count and the dimensions;
- contiguous zero-based names;
- meta subject, frame count and per-frame receipt rows.

**PASS: 22 subjects, 7,992 frames, 0 errors.** The manifest is `bank-manifest.tsv`: sorted subject rows of subject, frames, CRC32C, ordered-frame SHA-256 and bytes, tab-separated with LF. Its SHA-256 is `bdaac548...28bd`.

| Subject | Frames | Sequence CRC32C | Ordered frame bytes SHA-256 |
|---|---:|---|---|
| manafold-blown | 292 | `0x840DA184` | `57d09760d550ce40b0fc050ea7fe20ecf557b7c8f8ccda7ddb16a4085bc98547` |
| manafold-channel | 420 | `0xD49861D0` | `ea29946e11d28bdae7edfd452f19caea6d35b49a98f09b8d3a7fc000d70ed350` |
| manafold-crackle | 600 | `0xCC1BDA8B` | `1b04280607638734b45cc551d1c0e8813d2f76bbc5cb6d0060e87d488bb3281f` |
| manafold-curious | 180 | `0xFD63139B` | `d799c8958b56ff2afa1e332aa2da876985ae0c065366ff803d4e2334b403ddb8` |
| manafold-damage | 464 | `0xBADF776E` | `e26e0824468db7bfbab39ecda2ee587f2585ca57f4828c590ceebd4858c5653e` |
| manafold-death-drop | 450 | `0x99FCC8A1` | `865aad22aca5a7482a44908d587fed3328dd999e3c007dbc0aea27e51d5b1375` |
| manafold-death-gutter | 590 | `0x3BA123EC` | `09b2821ba88d09903c6cbb0a9de2d9b975c7019fa8e377bdffbcb45171287e09` |
| manafold-drift | 300 | `0x69158A83` | `06765873e5066ec11cc9353a2c638e5974b1cad621f5fa19f1c7f1184c5d6411` |
| manafold-fall | 340 | `0x1F062A86` | `f75ed433ffd9b3a4851276d65e22dbf3f83662208f5eba4973c9f27902e735bf` |
| manafold-flight | 352 | `0x9FBC05CB` | `8667e1455f465a7059fe26ace43edd30eb6d8c84d838e4275b647344a7c4e950` |
| manafold-hasty | 240 | `0xC6FB59FB` | `28642acaf21852fe76c051615ac7ab798f4b44c1975fb189d7d52297409532e0` |
| manafold-hit | 140 | `0x3BABB107` | `6e1d253fe8123a7899e570ae9569a48e5e5b1a572ec6841c5e1a850b32a554cb` |
| manafold-hover | 600 | `0x24B3FE60` | `2631dedc6775c32816cc12876eefb4aee47289bca2236660c5f0520dde6d02f9` |
| manafold-inspect | 600 | `0x95A27283` | `ea4359bf5f9799b7708f68a857e7b143db347d5a0fbd61a8b419360f38e56b86` |
| manafold-lasso | 336 | `0x26666BC0` | `965e85a84e42a055c4823ba38e47c8b3952a6fe0eb200ef424cc79bdeb54baaf` |
| manafold-pirouette | 240 | `0x18DB7B27` | `61e2f57bef172799022e32a1670c354a0a2d36b3c97ec41d10fee5b4589e4cf1` |
| manafold-rest | 400 | `0xD25EF330` | `35840a88d7c0805ba178f38a9d9a2e7d5efedfb185e212cc1bcc7ed670e6cd81` |
| manafold-startle | 160 | `0x11C587EF` | `5b4ac5a92c99768f736ff6a9ecc25d64bf310f11aad7916956131f8deec7f73e` |
| manafold-taunt | 280 | `0x3874DBF4` | `403220b92b95d691edd57b955f9dbfbd3acdf4174e18d7f72d191fee93ccb567` |
| manafold-taunt2 | 240 | `0x499AC40C` | `cb55c18b2e9d1064c91e6bb5bb69d181a83923883f5e3b4da7be451398f63f7a` |
| manafold-taunt3 | 368 | `0x07EACF1D` | `89e1b4339aca83d8aa0d9f852fd1251bb63c82fec0c319e1fb3b482c9ec74036` |
| manafold-trick | 400 | `0xAB4D78E9` | `f328dc5e3ba4d8a7b498703de9b1df93255a3d9a089dd13f22d6240786934f43` |

**Scope vs Wave F** (`scope-vs-waveF.txt`):
- **21/22 subjects are byte-identical** to the Wave-F final bank.
- **Only Trick changed** (`0xBF695C69` -> `0xAB4D78E9`), which is the intended plant pin.
- Against the same-binary `PLANT_PIN=legacy` render, Trick differs only on f156..f318, which is keys 78..159: the contact window plus the release.
- Key 78's frame differs even though its pin offset is exactly zero. The probable cause is a neighbour-reading post-pass such as `finalize_rear_follow`. This was not traced.
- The final Trick is byte-identical to the dev render reviewed in §1.

## 4. Per-subject visual verdict

**Coverage.** 18 images were read in total. Every one of the 22 complete every-frame sheets was looked at, either whole or in a composite. The sheets were read at their <=1600 px JPEG scale, where each tile is about 55-80 px wide. At that scale I can judge continuity, pops, framing, seams and gross effect reads. **I cannot judge fine detail** such as root seams, ink ownership or particle crossings. For the 21 byte-identical subjects, that detail transfers from the Wave D/E/F reviews through exact byte equality, not from this pass.

| Subject | Verdict | Basis |
|---|---|---|
| Trick | **PASS** | Whole sheet: approach, flip, plant f156, pause, one turn with overshoot, correction, righting f296-324, recovery, seam. Native A/B, slit-scan, release and 3x plant witnesses (§1). |
| Flight | PASS | Whole sheet: two clear climb-and-sink cycles, apex above the horizon, low troughs, continuous seam. Detail from Wave F L5/L6 (bytes identical). |
| Drift | PASS, with the owner call below | Whole sheet: in frame for the whole traverse. The wrap f299->f0 is the authored travelling seam. The subject is tiny at sheet scale, so mana detail is not judged here. |
| Hasty | PASS at sheet scale | No trail or smear is visible at thumbnail scale. The no-history claim rests on the live-history gate (22/22 OK, control fires) and Wave E. The subject is too small here to confirm by eye. |
| Crackle | PASS | Whole sheet, 600 frames: day sky, normal mana, continuous. |
| Blown | PASS | Whole sheet: blast, tumble, catch, return, continuous seam. |
| Taunt III | PASS | Whole sheet, 368 frames: continuous flick sequence, seam. |
| Hover | PASS | Whole sheet, 600 frames. The seam was checked at native f597-f2 (`V18-FINAL-HOVER-SEAM-NATIVE.jpg`): continuous. |
| Death Drop | PASS | Whole sheet: fall, settle, held corpse. The old f0220-0236 cutoff region reads continuous at sheet scale. |
| Death Gutter | PASS | Whole sheet: fall into the dirt, continuous hold to f589. |
| Curious, Hit, Startle, Pirouette | PASS at composite scale | Continuous. Detail by byte identity. |
| Taunt, Taunt II, Fall | PASS at composite scale | Continuous. Taunt II's thrown loop crosses the right edge around f140-199, as it did before (bytes identical). Fall holds its last frame. |
| Channel, Lasso | PASS at composite scale | Continuous. Channel keeps night. Lasso's throw leaves and returns. |
| Damage, Rest | PASS at composite scale | Continuous. |
| Inspect | PASS at composite scale | Continuous. |

## 5. Receipts

`V18-FINAL-RECEIPTS/` contains:
- `gate-matrix.txt`, `binaries.txt`, `identity-renders.txt`;
- `mqa-normal.log`, `mqa-control-trick-plant-pin.log`, `mprobe-normal.log`, `live-history-normal.log`;
- `bank-integrity.json`, `bank-manifest.tsv`, `scope-vs-waveF.txt`, `bank-render.log`;
- the scratch tools used: `v18-final-bank-validate.py`, `banksheets.py`, `gatematrix_int.sh`.

## 6. Open items (owner calls, non-blocking)

1. The plant pin ships, and the toggle is kept. If the owner prefers the old slide, the switch is `TrickPlantPin::kLegacy`.
2. Drift has no framing margin at either end (Wave F).
3. The End-swell stub is unchanged (Wave D).
4. Flight's underside is darker at the troughs. This shading predates version 18.
5. The TRAVEL exemption residual (§2.2).

The next packet is to encode exactly these 22 subjects from the frame root above. Do not re-render.
