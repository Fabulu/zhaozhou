# Manafold version 18 — Wave D review-blocker repair and closure

**Date:** 2026-09-19
**Worker:** Claude (sole worker; owner-approved while the GPT/Codex quota is exhausted)
**Scope:** the three `V18-SWELL-FRONT-REVIEW.md` blockers, the two coordinator audits relayed during closure, and the Wave-D visual packet. Waves E and F are untouched.
**Supersedes:** the build, gate, CRC and picture receipts in `V18-SWELL-FRONT-ART.md`. That report describes the pre-review tree: final taper key `0/0`, Trick root `1690`, a duplicate Front API and default-presentation sheets.
**Verdict:** **CLOSED.** All three blockers are repaired in source. Two further Wave-D defects found during closure were repaired: a mesh fault the previous checkpoint had reported green, and review sheets rendered in the wrong presentation. The full gate matrix passes 88/88. Every changed picture was looked at in the production presentation.

## 1. Inventory: the three blockers against the dirty tree

| Blocker | In source? | Evidence |
|---|---|---|
| P1 terminal taper | **Yes.** `kLoopBladeRxMm/RzMm` final key restored to `42/26`. Only the last ring (`i + 1 == kLoopRings`) takes the named `kReturnTipCapRxMm/RzMm` override in `make_loop()`, after ordinary taper and swell. | `mspan` G6d: `1 profile diff at ring 63/63, nonterminal 0 collateral 0 actual mismatch 0`. `--fail-terminal-cap` restores the authored profile on ring 63 and fires `0x4800` (terminal-cap + root-authority burial). |
| P1 Trick support ownership | **Yes, hardened further (see §3).** `mprobe` classifies the carrier-B swell core from the bind vertex and requires B to own every planted key and midpoint. | Normal: `ANTENNA SUPPORT carrier B key+midpoints 140/140 owned, missing 0 depth-fail 0, range -33..-18 mm`. Two separate fired controls. |
| P2 duplicate Front API | **Yes.** `HingePlay::tilt_front/yaw_front` is the only Front X/Y authority. `front_flex_play()` evaluates each clip's key table into a `HingePlay`, and `loop_pose()` alone applies fold Z, then X, then Y on JunctionF. No `apply_front_flex`/`front_flex_at` remains. Drift's standing wind trail now enters through the same field (`front.tilt_front -= kDriftTrailA16`), with no post-multiply. `FrontFlexPose` survives only as the internal table-sample type. | `grep` over `tools/reel`: every JunctionF X/Y write goes through `loop_pose`. I corrected the stale HingePlay comment ("shipping curves are authored later"). |

## 2. Defects found during closure and repaired

### 2a. The zero-radius terminal cap broke the mesh (Wave-D regression, previously reported green)

`manafold-meshcheck` returned **RC 1: `FAIL 16 degenerate (zero-area) triangles`**. The 15:48 reviewfix gate log (`gate-mmeshcheck.log`) shows the same failure, while the crashed worker's checkpoint claimed all protected normals green. With `kLoopSegments = 8`, a 0/0 ring welds its eight vertices into one position. That yields 8 degenerate return-quad halves plus the 8-triangle end fan. Wave C's accepted mesh was `CLEAN, 1416 position groups, 4200 edges`.

**Repair:** `kReturnTipCapRxMm/RzMm = 2/2` mm, a vanishingly small real ring, commented in `manafold_art.h` with the reason. Result: `CLEAN, 1416 position groups, 4200 edges`, identical to Wave C's topology counts. The terminal burial sweep stays green at worst rho **1082.80 pm** (was 1079.04 at 0/0; gate 1120). The probe's rim test reads the named cap constants, so it samples the real cap.

**Picture consequence:** the degenerate apex had been feeding its normal into the visible rings beside the RearSocket. A 5× and 12× crop of Hover f296 (0-cap vs 2 mm) shows the same silhouette and shape. Only a few shade levels move on the rear-return face (up to about 150 px on the worst Hover frame, a handful elsewhere). By eye it is a wash. The cap is kept because it removes a real fault.

### 2b. The REVIEWFIX sheets were rendered in the wrong presentation

The 15:48 sheets were made with bare `zhao-reel.exe` and no environment. I reproduced them frame-exact: 9,696/9,696 frame CRCs over nine subjects × selected/Front-mute/legacy. `MANAFOLD-INDEX.md` says: *"Render invocation is not optional: `ZIXX_EXP=celmain` and `ZIXX_LIGHT=diagonal-cool-cross`, set explicitly"*, and Wave C used the cel binary with that environment. The default presentation has no contour ink, so no outline question could be answered from those sheets. All twelve `V18-REVIEWFIX-*` sheets were **regenerated** from `zhao-reel-cel.exe` with the production environment (§6). File names are unchanged.

## 3. Coordinator audit 1: `manafold_probe.cpp` support ownership

| # | Finding | Disposition |
|---|---|---|
| 1 | Tie rule `support_min == all_min` counts a tie with a non-support vertex as owned | **Fixed.** Now tracks `sample_nonsupport_min` separately and requires `support_min < nonsupport_min` strictly. A tie is a shared contact and counts as a miss. B still owns 140/140. |
| 2 | Membership read the post-deform `sv` | **Fixed.** Membership uses the undeformed `m.verts[vi]` (`bind_v`); `sv` is used only for skinning. |
| 3 | `b0==bone \|\| b1==bone` ignores weight | **Fixed.** `SkinVertex` carries `w0` (b0's weight in 1/64; b1 carries `64 - w0`). Membership now requires `b0==bone && w0>0` or `b1==bone && w0<64`. |
| 4 | Wrong-support control also fired depth | **Fixed, with a new control.** Depth is judged only on owned samples, since an unowned sample has already failed. `--fail-trick-support` now fires ownership alone: `0/140 owned, depth-fail 0`. The new `--fail-trick-support-depth` leaves ownership intact and lifts the measured support depth by the named `kTrickSupportDepthControlLiftMm = 40`. It fires depth alone: `140/140 owned, depth-fail 140, range 7..22 mm`. Each control prints exactly one FAIL line. |

I also committed a trace (`U02_TRICK_MIN_TRACE=1`) printing global minimum, support minimum, window class and owner for every Trick key and midpoint.

## 4. Coordinator audit 2: `manafold_art.h` / `manafold_model.h`

### 4.1 `kTrickLiftKey 156 → 148`: a deliberate contract change, not a narrowed gate

The per-key 3D trace (`V18-REPAIR-RECEIPTS/trick-min-trace.csv`, key sub 0, global minimum in mm):

| key | 142 | 144 | 146 | 147 | **148** | 149 | 150 | 151 | 152 | 153 | 154 | 155 | 156 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| min Y | -22 | -26 | -30 | -29 | **-36** (apron) | +19 (apron) | +128 | +294 | +437 | +623 | +842 | +936 | +906 |

Keys 150–155 are genuinely airborne in the current animation: the righting throws the creature 0.1–0.9 m clear. Restoring 156 would require contact from a body nearly a metre up, which is impossible. Coverage did not shrink. The two-key apron (148–149) still bounds depth by the crash limit, and from key 150 on the ordinary ≥40 mm float gate applies (normal probe: `min clearance 47 mm`). `kTrickLiftKey` also ends the build_trick support pivot. The authored `kRootY` hold already ended at key 148 in version 17 (`{148, kTrickPlantRootMm}`); the old 156 existed only because the crown dragged into the righting at the old height. **Declared:** contact window `[78,148)` + 2-key apron, carrier-B ownership 140/140.

### 4.2 `kRearSocketBurialMm 270 → 200`: only the buried tip moves; invariant restated precisely

A scratch A/B harness (`V18-REPAIR-RECEIPTS/burial_ab_harness.cpp.txt`) was built against the tree at 200 and against a copy at 270. It hashed every vertex's posed position over **every clip key and midpoint** and dumped the loop profile:

- 64/64 ring profiles identical;
- **10 of 2,039 vertices move**, all `b0=b1=15 (kBReturnTip), w0=64/64`, at the terminal bind station. These are exactly the ReturnTip-only terminal vertices. No visible ring moves.

The loose range assert was replaced by a precise named invariant: `kRearSocketStraightTailMm = kReturnTipFromCMm - kRearSocketFromCMm` (the accepted v17 tail, 270) and `kReturnTipPullInMm = 70`, with `static_assert(kRearSocketBurialMm == kRearSocketStraightTailMm - kReturnTipPullInMm)`. The pull-in must stay inside the tail.

### 4.3 `kTrickPlantRootMm 1706 → 1534` + support pivot: an animation change, declared

This goes beyond the review's expected ~1690, so it is declared as a Wave-D animation change made to repair the contact. Version 17's pooled whole-mesh minimum had blessed a single −25 mm instant while the crown hovered over most of the plant. Now `1534` is the authored arrival height. For keys `[78,148)`, `build_trick` offsets the root so that the carrier-B support centre captured at key 78 stays put while the body balances around it. This is a kinematic pivot, not a fit. The probe independently certifies the actual B-swell surface.

**Looked at** (production ink, Wave C vs Wave D, keys 78/102/140/150, plus the every-key sheet for keys 60–190): framing is essentially identical. Wave D's crown sits visibly **on** the dirt through the plant, where Wave C's crown hovered just above it at arrival. The approach (keys 60–77) descends continuously onto the dirt with no jump; the added key-75 root key reads as a settle. The righting (keys 149–186) springs off the head, passes through horizontal and settles to hover with no pop. Nothing floats and nothing new touches down in approach or righting; clearance there is certified by the 3D probe, not judged from pixels. The planted root reads correctly.

## 5. Build receipts

Direct build with the committed recipe (`tools/reel/build-direct.sh --output .tmp/v18-waveD-close --clean`), compiler `g++ 16.1.0 (MinGW-W64 x86_64-ucrt-posix-seh)` from `C:\programmieren\dsstuff\mingw64\bin` (the `zhao-env.ps1` toolchain). Objects and binaries were removed first. Every binary postdates the last source edit (17:23:46 vs 17:23:27). I did not use CMake and claim no CMake result.

| Binary | MD5 |
|---|---|
| `zhao-reel-cel.exe` (production renderer) | `0FA8BAE0BF6A738C485FAD2DCB77A64B` (SHA-256 `2a1654207e2e09dee9a27dec98e6be1bc62d4dd28594ec434705c0f6b42cda30`) |
| `zhao-reel.exe` | `2DC018B4945531EF3332981C555585BB` |
| `manafold-spangate.exe` (mspan) | `19E0B61CF3996A1302318F38354B4E9B` |
| `manafold-motiongate.exe` (msmooth) | `8EEA5E4AE24E1DF6A69DCAD3458EE6C6` |
| `manafold-probe.exe` (mprobe) | `974E35957A133E2B00C655CBA021FE53` |
| `manafold-public-jointgate.exe` | `BC9681F5134F5FBE2BC13AA73EF47400` |
| `manafold-qa-p12.exe` | `FA732D8B7280631B98B871EEB296C698` |
| `manafold-meshcheck.exe` | `781C62B3C98535F2729397E6C6AC66BD` |
| `manafold-outlinegate.exe` | `1DA9051ED1E9482C4A4F953A7C0EFE58` |
| `manafold-shellgate.exe` | `865CC7E054C6695E4DA668FEE2C9319B` |
| `manafold-nodule.exe` | `DBE09AAF8FB1BC451E330C67CB7B5BF6` |
| `manafold-eyesize.exe` | `CA6A184BD783D8419DCABEE505F4F9ED` |

## 6. Gate matrix: **88/88** (`V18-REPAIR-RECEIPTS/gate-matrix.txt`)

- **Normals 10/10 RC 0:** mspan, msmooth, mprobe, mjointpub, mqa, mmeshcheck (**CLEAN**; it failed before §2a), moutline, mshell `--selftest` ("every check is failable"), mnodule, meyesize.
- **mspan 35/35 RC 1, each `attributed detector fired`:** rigid ×4, clamp ×4, drift ×4, overcompact ×4, e-start/e-mid/e-presocket, posed-order, crown-order, antenna-snap, accent-switch, hold-tremor, compress-wrap, final-dwell, root-authority `0x1880`, **front-flex `0x1000`**, **swell-size `0x2000`**, **terminal-cap `0x4800`**, carrier-mute F/A/B/C/E.
- **msmooth 16/16 RC 1, attributed** (mote-visibility fires only `0x80000`).
- **Protected 27/27:** mjointpub mute F/A/B/C/E (red leg OK); mnodule F/A/B/C/E + ignore (failable leg engaged); mprobe mirror/outline/scale-inverse each fail only their own line; **trick-support** (ownership only); **trick-support-depth** (depth only, new); moutline no-opening-owner/force-post-repaint; mqa lane/seam/eyesnap/startle/rootstep/fall-wrap ("FAILABLE LEG OK"); meyesize L/R/wrong-bone.
- Key normal numbers: G6 posed ring order 339,500 steps, 0 reversed/pinched, min projection/separation 4.652/6.977 mm; G6b Front X/Y 10 clips, weakest Rest 24.37 mm (floor 20), production mismatch 0, seams 0; G6c swell 36 selected/legacy rings differ, actual mismatches 0; G7 closure endpoint 6.065 mm, tip-length error 0.550 mm, straightness sin 0.008223.
- **Selectors:** 9/9 malformed or out-of-range `ZHAO_U02_SWELL_MODE/SWELL_PM/FRONT_FLEX/FRONT_FLEX_GAIN_PM` values return RC 2.
- **Unrelated identity:** Zixxtrixx Idle 576/576 frame-exact to Wave C (`0x1408F885`).

## 7. Renders and sheets

Production presentation (`zhao-reel-cel.exe`, `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`), nine complete subjects × three modes, 9,696 frames (`V18-REPAIR-RECEIPTS/render-crc-production.txt`). Selected sequence CRCs: Hover `0x94122BE4`, Rest `0x83176096`, Flight `0x75EA27FF`, Hasty `0x36C69A71`, Drift `0xA09D0FD7`, Blown `0xDAD11DA2`, Taunt `0x5B7E861B`, Taunt III `0x47E140C8`, Trick `0x6C2AC6D2`.

Regenerated: `V18-REVIEWFIX-SWELL-SELECTED-VS-LEGACY-2X.png`, `V18-REVIEWFIX-FRONT-FLEX-AB-2X.png`, `V18-REVIEWFIX-TRICK-SUPPORT-3WAY-2X.png` and the nine `V18-REVIEWFIX-SWELL-FRONT-MANAFOLD_*-ALLFRAMES.png`. New: `V18-REPAIR-SWELL-LADDER-HOVER-PROD.png`, the ladder re-rendered after the taper restore as the review required (legacy family × `ZHAO_U02_SWELL_PM` 1000/850/700/550/400/250; the 1000 rung is byte-identical to the legacy render).

Note on the ladder: the diagnostic multiplier truncates and the shipping family was rounded, so the 400 rung differs from the selected constants by 1 mm on A rx, B rx/rz and End rz. The ART report's "equal to the integer output of the 400 pm rung" is therefore approximate. There is no visual consequence.

## 8. Visual verdict (looked at; measurement only on the comparison side)

- **Swell ladder (production, after taper restore):** 1000/850 read as beads threaded on the strap, 700/550 as knobs, **400 keeps each carrier a touch thicker than its sticks within one continuous strap**, and 250 flattens toward a uniform band. The 400 selection stands.
- **Selected vs legacy (Hover 0/320, Taunt III 142/324, Blown 44/134):** the legacy corner beads and top ball are gone. Carriers stay readable, with no waist, pinch or seam at either root. **PASS.**
- **Front-flex A/B (Hover 96, Rest 150, Flight 88, Hasty 86, Taunt 142, Taunt III 280):** the Front angle visibly changes on both axes (strongest in Taunt and Taunt III). The root stays fused with body-style material, with no kink or shear. Hasty is too distant to judge. **PASS.**
- **Trick support 3-way (keys 75/78/102/146/148/155) and every-key sheet:** planted headstand on the dirt, smooth landing and a clean spring-off in the righting. The three modes are near-identical through the plant (Front is at identity). **PASS.**
- **Production-ink composite:** continuous contour around body and antenna. No outline hole at either root, at the rear-return/End stub or at the carriers.
- **All-frame sheets:** Hover (600) and Taunt III (368) are continuous with no pop or dropped frame and a matching seam. Rest/Flight/Taunt/Hasty/Drift/Blown were only judgeable at thumbnail scale here: every frame is populated and continuous. Detail for those subjects rests on the 2× plates, the worst-changed-frame crops and the gate matrix.

## 9. Still open (none blocks Wave D)

1. **Trick framing (pre-existing, Wave F):** the planted crown sits at the bottom of frame in Wave C and Wave D alike. Wave F's planted yaw turn should revisit camera/framing.
2. **End-swell stub (pre-existing):** a short rounded End-swell post beside the rear return is visible at some angles (Rest 342, Taunt III 328, Trick 393). It is present identically in the accepted Wave-C renders and reads slimmer now. Not a Wave-D regression; worth an owner glance in the final bank review.
3. **The persistent history mist, the Flight phrase and the Trick full turn** remain Waves E/F, unchanged by this closure.

## 10. Files

Source packet: `tools/reel/manafold_art.h`, `manafold_model.h`, `manafold_clips.h`, `manafold_probe.cpp`, `manafold_spangate.cpp`, `manafold_motiongate.cpp`, `zhao_reel.cpp`.

Evidence committed: this report, `V18-SWELL-FRONT-ART.md` (superseded header added), `V18-SWELL-FRONT-REVIEW.md`, the twelve regenerated `V18-REVIEWFIX-*` sheets, `V18-REPAIR-SWELL-LADDER-HOVER-PROD.png`, `V18-REPAIR-RECEIPTS/` and `TASK_LOG.md`.

Deliberately not committed:
- the 14:44 `V18-SWELL-FRONT-*`, `V18-SWELL-LADDER-HOVER-2X`, `V18-SWELL-SELECTED-VS-LEGACY-2X` and `V18-FRONT-FLEX-AB-*` sheets, which show the blocked pre-repair geometry;
- raw frames and `.tmp/`;
- the coordinator's `V18-WAVE-EF-PREP.md` and `qwen/`;
- the stray pass-16/17 `PASS17-*` files.
