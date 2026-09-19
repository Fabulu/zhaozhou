# Manafold Pass 17 final targeted integration

**Date:** 2026-09-19
**Status:** **HISTORICAL TARGETED PASS — exact-bank review later exposed P3 Death opacity and P4 Trick identity; see focused repair reports**
**Source base:** `31949deae836eb29a1add75da09f00dd6b09fc3e` plus the exact declared 12-path Pass-17 source packet
**Environment:** `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`

This report supersedes the provisional pre-palette integration. It closes the targeted gate only; the later exact 28-subject shipping generation, encode and publication remain separate delivery work.

## Independent source review

The complete uncommitted production/gate/build packet was independently reviewed before building:

1. `tools/CMakeLists.txt`
2. `tools/reel/build-direct.sh`
3. `tools/reel/manafold_art.h`
4. `tools/reel/manafold_clips.h`
5. `tools/reel/manafold_fx.h`
6. `tools/reel/manafold_lab.h`
7. `tools/reel/manafold_motiongate.cpp`
8. `tools/reel/manafold_orderplot.py`
9. `tools/reel/manafold_public_joint_metric.h`
10. `tools/reel/manafold_public_jointgate.cpp`
11. `tools/reel/manafold_spangate.cpp`
12. `tools/reel/zhao_reel.cpp`

`git diff --check` returned RC 0 apart from Git's existing LF→CRLF checkout warnings on the two gate files. No production or checker blocker survived review.

The palette repair is structurally sound:

- production passes the exact presentation period (`clip.frame_count * 2`) to `mana_build_ramps`;
- the selected one-cycle phase closes on every loop and uses quintic C2 interpolation between adjacent authored CLUT rotations;
- all 126 integer-phase Blue/Violet comparisons are byte-exact, so the interpolation cannot blur or dim the authored endpoints;
- `msmooth` traces every live CLUT entry and actual bloom-histogram-weighted emitted energy across adjacent frames, last→first and frames 0–2 after wrap;
- raw-clock and hard-switch controls are strictly attributed to the palette category alone;
- Taunt III and Startle schedule comments, and current-versus-historical report sections, now match the source.

## One clean build receipt

One new caller-owned direct output was created at `.tmp/p17-postpalette-final`; no prior object or executable was reused. The renderer and every focused gate built RC 0. A clean PowerShell/preset CMake configure also returned RC 0, and the four newly registered CMake gate targets (`manafold-motiongate`, `manafold-public-jointgate`, `manafold-eyesize`, `manafold-outlinegate`) built RC 0.

| Binary | MD5 | SHA-256 |
|---|---|---|
| `zhao-reel-cel.exe` | `8DD0AE74058E620C222289CB17A565B9` | `7FFA74F55CD6D282C1D91D967668AE85ABA1D84B5B25D79C6E71715CB0C951A6` |
| `manafold-spangate.exe` | `C0991BA37FB3AEABEBE3A1253B261FC0` | `935FE30A979A683C002FD402884345D10CDD6451DAA693C845D851F1976A8C7D` |
| `manafold-motiongate.exe` | `E04B15D97C9456B0AAE0A2C4C202C57E` | `B958D068A740C779B7836703CBA656DC6DC7D3CBEF86D446856B990D54A0BB92` |
| `manafold-public-jointgate.exe` | `631F76266BC9219FD5BB8B40465C468E` | `A5141C78174DDF1F77021809563A34855002BCED2DAE5707EAAAB01C16919AA5` |
| `manafold-qa-p12.exe` | `D4E4C04486EE196BD8159A261B37BD5A` | `EA69DEBDFA9CD9D783A9C92A8C712B10C9E049A929F104D022289B68A8731AD4` |
| `manafold-probe.exe` | `4AC654C0A41C681A1ED1F96F5CB7BCC2` | `85E7EC6694C572A663BBB559B8408CB183A6ED612615C0515BA9697F4E43A228` |
| `manafold-eyesize.exe` | `2CBF54A2BFA43CEA8945C4DE0415D171` | `EC251AC74045AB3BA478F9E352C715BC4D514D16338C4F153820843DB9870A30` |
| `manafold-nodule.exe` | `A29B81C4E21F72C5CFDB4F3A4D7FF8A6` | `C0764B29172ECB518E827481F10C6F5631A99ABE22026DFFAD150251990912DD` |
| `manafold-meshcheck.exe` | `70A11B8C853DE6B347E9B6F9A11B0D74` | `5CA1F18D7037018469CE203544B64459FA0DF0C6B408F9AB08797886B55ABFC2` |
| `manafold-outlinegate.exe` | `17CEBA66EF3BA2D8CACC852E3FF2666C` | `125E9BA11B1B05B9BFD9941059F7493BA1A2C27397B88590C2461A9EAED1B7F5` |
| `manafold-shellgate.exe` | `0283E852CF38F7E069166C613003CB6E` | `32AF9A3D1038F92CF6FB6DC3BF6882EAEFF82CC86E583AA36DAD5C195575B363` |
| `manafold-eyecam.exe` | `606BEC9622774CA50090B29316260F64` | `BA62B349221E712E53FA831C7A1D058D3123669E15DE9AC628716644ADF5ED5D` |
| `manafold-express.exe` | `EA5DDEF83AC0014D70C645FB6F38590E` | `E95A351537B458FDFF8B5C4685A5A6ACFFB6B4F6751A1440F2EF34A855987DD6` |

## Final focused matrix

The scripted matrix executed **137/137 expected outcomes**. Real child-process return codes were checked directly.

### Signed spans and public carriers

- `mspan` normal: RC 0.
- all **31/31** strict attributed controls: RC 1, zero attribution failures. These comprise 20 structural span controls, order/snap/accent/tremor/compress/final-dwell controls, and held F/A/B/C/E mutes.
- held-only normal: RC 0; held F/A/B/C/E mutes: attributed RC 1.
- held-A ladder: `60/80/100` RC 0; rejected `140` RC 1.
- malformed/trailing/overflow/out-of-range/leading-space A overrides: RC 2.
- `mjointpub` normal and inverted F/A/B/C/E public controls: RC 0 by that gate's failable-leg convention.
- `mnodule` normal and all five inverted carrier controls: RC 0.

Current normal receipts:

- 339,500 posed ring steps; zero reversal/pinch; minimum projection `6.882 mm`, separation `7.441 mm`.
- all four signed spans carry both signs; zero bound/margin breaches.
- crown witnesses pass; A–B/A–C/B–C crossings `4/4/2`.
- held f0324 visible response: Front max `20.62`, A `81.10/83.05`, B `184.13/203.70`, C `167.86/210.53`, End max `24.04 mm`.
- 48,500 carrier samples including nine held-tail samples; position step/accel/jerk `62.435/49.412/49.333 mm`; angular `7.296/5.280/5.247°`.

### Lightning, particles and palette

- `msmooth` normal: RC 0.
- all **15/15** strict effect/palette controls: attributed RC 1, zero attribution failures.
- no blackout, shape-chain break, ID/role/count change or morph reversal.
- Boil RGB step/accel/jerk `72.28/23.49/14.73`; emitted bloom energy `45.95/15.93/12.75`; seam `3.46`; integer-phase mismatch `0/126`.
- shipping free/fold-station/fold-mote/surge maxima remain within reviewed bands; Channel and lab edge/stamp/energy/visibility histories remain continuous across adjacent, seam and held-tail samples.

### Remaining gates and selectors

- `mqa` normal, eyes-only, death-eye, Startle-step, Fall-only/wrap, lane, seam and root-step failable legs: all RC 0 by the gate's positive-control convention.
- `mprobe` normal RC 0; scale-inverse/outline/mirror controls RC 1.
- `meyesize` normal/L/R/wrong-bone legs: RC 0.
- `mmeshcheck` RC 0.
- `moutline` normal/no-owner/forced-repaint: `0/1/1`.
- `mshell` normal, nine selftests, Pass-16 legacy and Pass-15 regression: RC 0.
- `meyecam` and `mexpress`: RC 0.
- 40 malformed/trailing/overflow/out-of-range/leading-space renderer cases across Trick X/Z/yaw, Taunt III yaw/roll/A, Boil cycles and palette-control names: RC 2.

## Exact targeted generation integrity

The clean renderer produced all subjects in one invocation. The committed `rgbframe.py` validated every frame header and exact byte count; metadata subject/count, contiguous zero-based filenames and every per-frame receipt row agree.

**Result: 33 subjects / 24,308 frames / zero errors.**

| Subject | Frames | Sequence CRC32C |
|---|---:|---|
| manafold-hover | 600 | `0x5BBBC743` |
| manafold-channel | 420 | `0xB26245C8` |
| manafold-startle | 160 | `0x750FFF50` |
| manafold-rest | 400 | `0x268F219E` |
| manafold-pirouette | 240 | `0xE5D139FD` |
| manafold-fall | 340 | `0x1A7047B3` |
| manafold-trick | 400 | `0x58219E79` |
| manafold-damage | 464 | `0xF6FFFAF1` |
| manafold-antenna-fixed | 420 | `0x7A47404E` |
| manafold-death-drop | 450 | `0xFC405D06` |
| manafold-death-gutter | 590 | `0xF8C8CA5F` |
| manafold-taunt3 | 368 | `0x618277B7` |
| manafold-antenna-quarter | 420 | `0x8C7267F8` |
| manafold-crackle | 600 | `0x5232CEDF` |
| manafold-mana-blue | 600 | `0xB17DB1EA` |
| manafold-mana-green | 600 | `0xE813A97B` |
| manafold-mana-boil | 600 | `0x08954AAD` |
| manafold-mana-stack | 600 | `0x19B5F3A8` |
| manalab-control-channel | 1,440 | `0xF1E4D4AA` |
| manalab-breadth-more | 1,440 | `0x0CA6E029` |
| manalab-past-the-wall | 1,440 | `0xE773DD69` |
| manalab-three-stones | 1,440 | `0x9156D99F` |
| manalab-edge-strands | 1,440 | `0xE0EA4A44` |
| manalab-held-still | 1,440 | `0xC7D76AC8` |
| manalab-snap-states | 1,440 | `0x136BDC58` |
| manalab-negative-void | 1,440 | `0x65C9E0DF` |
| manalab-two-shapes-long | 1,440 | `0x4DA8F95E` |
| manalab-edge-snap-held | 1,440 | `0xEA959E4C` |
| manafold-inspect | 600 | `0x60C70684` |
| manafold-curious | 180 | `0xEA31677C` |
| manafold-taunt | 280 | `0xB64E1088` |
| manafold-taunt2 | 240 | `0x54AAA53A` |
| manafold-lasso | 336 | `0xA57C140E` |

Boil and Blue are 1,200/1,200 byte-identical to the palette lane's reviewed selected generation, including sequence SHA-256 `b9fe790d5bcefaaafc15e2906ced54d48c5cadfc6d77b0d73655f53167594e70` and `84821bf0edb06f67fdc3b50ea914ea00272db29cae52aaac701e732423a005ef` respectively.

Same-binary comparison controls add 6,772 validated contiguous frames across eye both/L/R, five held carrier mutes, rejected Trick/Taunt III orientations and raw/hard Boil palette paths. Every invocation returned RC 0 and carries its own renderer metadata/sequence CRC.

## Every-frame picture verdict

Seven reduced master sheets include every presentation frame of all 33 subjects. Native, 2× and exact 4× plates decide the named art questions.

- **Trick (historical false acceptance):** the pure-X path kept contact and smooth mechanics, but the reduced targeted sheets did not expose that both eye plates remained edge-on through the hold. Exact-bank 4x review later rejected f0140–f0295. `PASS17-TRICK-FACE-REPAIR.md` supersedes this visual verdict with a planted +90-degree local face yaw.
- **Taunt III:** PASS. The four crown arrivals read as deliberate rankings, not twitching. The held f0304–f0344 picture keeps both eyes, open O and persistent lightning readable. `1450/750 pm` gives an unmistakable large/small side-eye; A80 retains the selected stronger read. Legacy yaw/roll hides the face again. F/A/B/C/E mutes each change its named visible crown section, with attached F/End deliberately subtler.
- **Curious / Startle / Taunt eyes:** PASS. Curious's `1250/820`, Startle's synchronized `1350/1350`, and Taunt's `1350/800` changes read at native resolution. Both-muted frames remove size acting without removing gaze/blink/roll; L/R controls isolate the named lens/star unit. No white/cyan child detachment or splinter regression appears.
- **Carrier motion / signed spans:** PASS. Damage, Startle, crown reversals, deaths, Hover/Rest, Taunt/Taunt II and Lasso move continuously at every frame. A/B/C retain large independent travel and full ordering freedom; no C–End buckle, pinch, sliding socket or buried-tip exposure returns.
- **Channel / lightning / particles:** PASS. Channel f0419→f0000 retains branch geometry and white/cyan/navy energy; shapes morph without off/reappear. Crackle, Pirouette, Lasso, both deaths and all ten lab subjects retain persistent figures and particle populations without a visible reseed or teleport.
- **Boil:** PASS. f0597–f0002 carries one continuous cyan/blue palette motion. Raw-clock visibly resets hue/brightness and hard-switch steps discrete states, so the controls remain meaningful.
- **Damage / deaths / Startle / Fall:** PASS. Four damage hits remain forceful without one-frame carrier/body jumps; both death settles remain dead and continuous; Startle keeps its payoff without the old root/antenna kick; Fall f0338→f0339 holds the grounded recovery rather than restarting.
- **Outline / shell:** PASS. Fixed and quarter views keep the complete lower/inside O ink, preserved top line and open negative space through every angle. Channel f0344/f0345 keeps foreground cyan over the internal line. The body reads solid; mist remains a narrow contour-centred band rather than broad transparency.

No additional art value or source change was needed.

## Curated current-generation evidence manifest

Stage these current final-target PNGs with the report; they are the bounded durable review packet:

### Every-frame masters

- `PASS17-POSTPALETTE-FINALTARGET-MASTER-CARRIER_ART.png`
- `PASS17-POSTPALETTE-FINALTARGET-MASTER-REACTIONS.png`
- `PASS17-POSTPALETTE-FINALTARGET-MASTER-EFFECTS.png`
- `PASS17-POSTPALETTE-FINALTARGET-MASTER-MENU.png`
- `PASS17-POSTPALETTE-FINALTARGET-MASTER-OUTLINE.png`
- `PASS17-POSTPALETTE-FINALTARGET-MASTER-LAB_A.png`
- `PASS17-POSTPALETTE-FINALTARGET-MASTER-LAB_B.png`

### Native / enlarged witnesses and same-binary controls

- `PASS17-POSTPALETTE-FINALTARGET-TRICK-WITNESSES-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-TRICK-WITNESSES-2X.png`
- `PASS17-POSTPALETTE-FINALTARGET-TRICK-SELECTED-VS-LEGACY-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-TAUNT3-WITNESSES-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-TAUNT3-PUNCHLINE-2X.png`
- `PASS17-POSTPALETTE-FINALTARGET-TAUNT3-SELECTED-VS-LEGACY-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-TAUNT3-JOINT-MUTES-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-EYE-ACTING-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-EYE-ACTING-2X.png`
- `PASS17-POSTPALETTE-FINALTARGET-EYE-CONTROLS-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-EYE-CONTROLS-2X.png`
- `PASS17-POSTPALETTE-FINALTARGET-BOIL-SEAM-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-BOIL-SEAM-4X.png`
- `PASS17-POSTPALETTE-FINALTARGET-BOIL-CONTROLS-4X.png`
- `PASS17-POSTPALETTE-FINALTARGET-CHANNEL-SEAM-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-CHANNEL-SEAM-4X.png`
- `PASS17-POSTPALETTE-FINALTARGET-CHANNEL-OUTLINE-FG-4X.png`
- `PASS17-POSTPALETTE-FINALTARGET-FALL-HOLD-NATIVE.png`
- `PASS17-POSTPALETTE-FINALTARGET-OUTLINE-VIEWS-2X.png`

The 33 individual current all-frame sheets and eight complete control sheets were generated and reviewed, but are redundant with these masters/witnesses and should remain unstaged.

## Historical/provisional exclusions

Do not cite or stage as current acceptance:

- renderer `5257148A57EE5FD4AD4713EF3E7765F3`, `mspan 8C46A237...`, `msmooth 16F8AA79...`, or any pre-palette targeted raw bank;
- `PASS17-FINALTARGET-PROVISIONAL-BOIL-SEAM-{NATIVE,4X}.png` except as explicitly historical defect evidence;
- any `PROVISIONAL`, `INTEGRATED87`, `CURRENT-RED`, `BEFORE`, 22-bone, pre-brightness, ambiguous `SMOOTH-FINAL`, `SMOOTH-ACCEPTED` or `INTEGRATED-FINAL` sheet;
- provenance-deficient `PASS17-FALL-HOLD-VS-WRAP.png` and `PASS17-ENDPOINT-AUTHORITY-{LADDER,4X}.png`;
- `.tmp/`, `out/`, raw `.rgb`, build binaries/logs, selector-dry outputs and generated but uncited duplicate PNGs.

## Disposition

**Historical targeted verdict: PASS for the pre-P3/P4 packet.** The later exact banks exposed Death Drop's untraced soft-opaque alpha cutoff and Trick's still-edge-on planted eyes, so this renderer is not a final receipt. `PASS17-DEATH-DROP-EFFECT-REPAIR.md` and `PASS17-TRICK-FACE-REPAIR.md` close the focused source/gate/picture defects. Their source must be committed and one new exact 28-subject bank reviewed before encoding.
