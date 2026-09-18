# Manafold Pass 17 — Taunt III held punchline plan

**Date:** 2026-09-18
**Mode:** read-only art architecture; no source, build or render performed
**Question:** how does the existing dismissal/hold become one front-readable joke after the crown shuffle, without adding another gesture?

## Decision

Keep Taunt III's accepted timing exactly:

- crown shuffle/set-up: keys 100..146;
- dismissal attack: keys 146..149;
- held punchline: keys 149..173;
- release/loop closure: keys 173..183.

The mechanical action is a **caught-cheating side-eye**. Manafold finishes the crown shuffle, snaps into an opposed five-carrier shrug, squashes/drops the whole body, and stares with one eye conspicuously large and the other small. Front and End cock in opposite directions; A and C rise while B drops. It holds that accusation long enough to read, then dismisses the viewer and releases.

This is not a fifth beat. It is the existing `flick` arrival made coherent across the body, all five carriers and the eyes. The crown shuffle remains the set-up and releases to zero by key 146; the punchline owns keys 146..173.

## Why the current hold fails

The timing is not the fault. `PASS17-TAUNT3-PUNCHLINE.png` shows the fast arrival and long hold. The fault is the held picture:

- current full-body orientation is `yaw +6000`, `roll 7000` angle16 at the settled hold;
- the broad nodule throw, body drop and squash arrive, but the composed yaw/roll parks the face on the side/back;
- both lenses and their size asymmetry become unreadable from roughly f0304 through f0344;
- the current dismissal moves A/B/C broadly together and does not route a held all-five punctuation through `swallow_nodules`, so the existing public F/A/B/C/E mute is not yet proof about the punchline.

The committed Zixxtrixx taunt grids establish the bar: its face/eye remains readable at every extreme while the silhouette changes. Manafold's bigger body and richer antenna do not compensate for hiding the face.

## Narrow production seam

### 1. Keep the directional throw

Retain the current mostly-upward `fl_up/fl_back/fl_side` dismissal as the broad loop throw unless pictures reject it. It protects the open loop from the old edge-on collapse and gives the dismissal a direction.

### 2. Add one held five-carrier punctuation through the existing public path

Add named signed constants, selected by looking:

```cpp
constexpr int32_t kTaunt3PunchCarrierMm[5];  // Front, A, B, C, End
constexpr int32_t kTaunt3PunchLeanPm;
```

At the existing `flick` beat, multiply this one authored array by `flick` and pass it through `swallow_nodules` before `loop_pose`. That reuses:

- the real Front/End local rotations with attached centres;
- the real A/B/C nodule solve and signed spans;
- `apply_public_joint_mute` at the production consumption point.

Do not add a private punchline mute. The intended first shape family is mechanically explicit—Front/End oppose, A/C lift, B drops—but its numeric amplitudes remain authored by native render/look/adjust. The broad throw supplies direction; this array supplies the readable carrier disagreement.

If the differential pose fights the broad throw, change the named art values or their directional shares. Do not delete carrier independence, move attachment centres, or infer values from the old plate.

### 3. Choose only the held body orientation

The renderer already exposes validated same-binary controls:

```text
ZHAO_U02_TAUNT3_FLICK_YAW_A16
ZHAO_U02_TAUNT3_FLICK_ROLL_A16
```

They are sufficient for the orientation question. Do not add a body-mute mechanism or camera path. Every rung keeps the same root drop, direct held squash, antenna throw, five-carrier punctuation, eye acting and timing.

After selection, preserve the rejected current pair as named legacy constants/control. The selected pair becomes the shipping default; the environment overrides remain the one-binary ladder.

### 4. Keep the asymmetric eyes on the same arrival

Retain the current `1450/750 pm` left/right scale driven by `flick`. It is already full at f0298 and held through the punchline. `ZHAO_U02_EYE_SIZE_MUTE=both|L|R` isolates size without removing gaze, squint, roll or travel.

Do not tune eye values until a body orientation makes both purple lenses and nested stars continuously visible. If the selected face is readable but the current `620/420 pm` squint hides the large/small contrast, re-author the named squint shares by eye; do not raise scale merely to overpower a lid.

## Minimal same-binary orientation ladder

Use one freshly built binary after the crown-shuffle and continuity packets settle. Only the two existing environment values change.

| ID | Yaw A16 | Roll A16 | Role |
|---|---:|---:|---|
| `legacy-away` | `6000` | `7000` | Required rejected control; must restore the f0304..f0344 side/back hold. |
| `front-squash` | `0` | `0` | Primary candidate/control: face orientation stays with the readable pre-flick pose; body still arrives through drop and direct held squash. |
| `front-tip` | `0` | `4000` | Primary authored candidate: adds a whole-body tip without yawing the face away. |
| `mild-counter` | `-2000` | `4000` | Conditional tie-break only if the first two candidates both keep the face but leave a real unresolved flatness/direction tradeoff. |

These are comparison rungs, not a selected answer. Do not average them or derive a new pair from pixel measurements. Render the conditional fourth only if `front-squash` and `front-tip` both pass every stop condition and differ in a genuine artistic tradeoff.

`front-squash` is not a no-body-motion cheat: root drop and direct squash still change the whole silhouette and remain held. It asks whether those two existing whole-body channels already make a clear comic arrival. `front-tip` asks whether one readable-axis tip improves it.

## Exact rendering protocol

Build once, render all orientation candidates from that binary:

```powershell
$root = 'C:\programmieren\zencrifice\manafold-p16\zhaozhou'
. "$root\tools\env\zhao-env.ps1"
$bash = 'C:\Program Files\Git\bin\bash.exe'
$build = "$root\out\p17-taunt3-punchline"
& $bash "$root\tools\reel\build-direct.sh" --output $build --clean cel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = "$build\bin\zhao-reel-cel.exe"
$env:ZIXX_EXP = 'celmain'
$env:ZIXX_LIGHT = 'diagonal-cool-cross'
$env:ZHAO_U02_EYE_SIZE_MUTE = 'none'
$env:ZHAO_U02_JOINT_MUTE = 'none'

$env:ZHAO_U02_TAUNT3_FLICK_YAW_A16 = '6000'
$env:ZHAO_U02_TAUNT3_FLICK_ROLL_A16 = '7000'
& $exe "$root\out\p17-taunt3-legacy-away" manafold-taunt3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$env:ZHAO_U02_TAUNT3_FLICK_YAW_A16 = '0'
$env:ZHAO_U02_TAUNT3_FLICK_ROLL_A16 = '0'
& $exe "$root\out\p17-taunt3-front-squash" manafold-taunt3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$env:ZHAO_U02_TAUNT3_FLICK_YAW_A16 = '0'
$env:ZHAO_U02_TAUNT3_FLICK_ROLL_A16 = '4000'
& $exe "$root\out\p17-taunt3-front-tip" manafold-taunt3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

Only if needed:

```powershell
$env:ZHAO_U02_TAUNT3_FLICK_YAW_A16 = '-2000'
$env:ZHAO_U02_TAUNT3_FLICK_ROLL_A16 = '4000'
& $exe "$root\out\p17-taunt3-mild-counter" manafold-taunt3
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

Before rendering, validate that bad yaw/roll values still return RC 2. Record source commit/dirty declaration, binary MD5/SHA-256, environment, exact command and sequence CRC for every rung.

## Exact frame windows

All frame numbers are 60 Hz presentation frames:

- `f0286`: last crown-shuffle/readable pre-dismissal comparison;
- `f0292`: dismissal begins, identity-size/body comparison;
- `f0298`: antenna throw, carrier punctuation and `1450/750` eye asymmetry reach full strength;
- `f0304`: three-key body lag has landed; held punchline begins;
- `f0312`: early hold;
- `f0324`: middle of the hold, primary same-frame control witness;
- `f0344`: last fully held/readable witness;
- `f0346..f0350`: hold/release boundary;
- `f0360`: release in progress;
- `f0367`: loop-seam/final recovery witness.

Review **all 368 frames** for every primary orientation. The acceptance window is continuously f0304..f0344, not three favorable stills.

## Evidence products

Use only committed `plates.py`/`rgbframe.py`.

1. Complete every-frame sheets for `legacy-away`, `front-squash`, `front-tip`, and the optional candidate only if rendered.
2. Native frame-major orientation grid at f0286/f0292/f0298/f0304/f0312/f0324/f0344/f0350/f0360/f0367.
3. One fixed native crop box containing both purple lenses, white/cyan stars, body silhouette and enough crown to identify Manafold; reuse it for exact 4x nearest-neighbour frames f0304/f0324/f0344 across all candidates.
4. Full-frame native silhouette grid at f0298/f0304/f0324/f0344 so a face crop cannot hide an edge-on loop, collapsed span or missing carrier.
5. After orientation selection, normal versus `EYE_SIZE_MUTE=both`, `L`, and `R` at f0304/f0324/f0344.
6. After the five-carrier punctuation is live, normal versus `JOINT_MUTE=F|A|B|C|E` at f0324, plus a second frame only if one carrier is occluded there. Every mute render uses the same selected orientation.
7. Selected orientation versus legacy-away at f0304/f0324/f0344—the required current-body-orientation red control.
8. A native comparison sheet placing selected Manafold f0304/f0324/f0344 beside all tiles of `PASS17-ZIXX-TAUNT-KEYS.png` and `PASS17-ZIXX-SLOW-TAUNT-KEYS.png`. The existing grids are the accepted bar; optionally re-render both Zixxtrixx subjects once from the final binary and require them to remain unchanged.
9. Reuse the crown-shuffle f0214/f0234/f0254/f0274 evidence from `PASS17-PUBLIC-ORDERING-PLAN.md`; do not conflate those rankings with the held punchline.

A locator sheet may be downsampled, but native frames decide. A 4x crop confirms a suspected defect; it cannot rescue a joke absent at 384×240.

## Selection order

1. Confirm `legacy-away` visibly reproduces the rejected side/back hold. If it does not, the ladder lacks a valid control—stop.
2. Review every frame of `front-squash` and `front-tip` for face continuity, outline/span integrity and loop closure.
3. Reject any candidate that loses either purple lens or nested star for a sustained part of f0304..f0344.
4. Compare native orientation grids; use 4x only to confirm suspected eye/outline faults.
5. Render `mild-counter` only if both primaries pass but neither clearly owns the whole-body/readability tradeoff.
6. Select by the complete moving read, not by averaging numbers.
7. With orientation selected, render eye-size and five-carrier mutes. If the normal does not beat every ablation immediately at native scale, re-author the named component values and repeat.
8. Compare the selected held picture against both Zixxtrixx taunt grids. A viewer must read the caught-cheating side-eye without this report's labels.

## Structural gates after selection

Run from the same final source/binary generation:

- `mspan`: full-bank posed-ring/closure bounds plus crown-shuffle height/crossing/signed-span gates and A/B/C mutes;
- extend the slot-21 held-witness section to record F/A/B/C/E visible-core contribution at f0324 after the picture is accepted; each attributed mute must remove its named contribution while unrelated carriers remain live;
- `mjointpub`: existing public five-carrier mechanism and F/A/B/C/E red legs remain green;
- `meyesize`: normal, L/R mute and wrong-bone controls;
- `mprobe`: scale-aware containment and detached-star controls;
- `meyecam`: camera-relative both-eye readability across the complete hold;
- `mmeshcheck`, `mnodule`, `moutline`, `mqa` and `creature_core` normal/attributed controls.

Only after the native picture is selected should a named punchline read margin or visible-core delta be recorded below the observed witness. Gates preserve the authored result; they do not choose orientation, eye scale or carrier amplitudes.

## Stop conditions

Reject and iterate before the full bank if any is true:

- the face, either purple lens, or either nested star disappears for a sustained part of f0304..f0344;
- size asymmetry needs a 4x crop or prose to distinguish normal from `EYE_SIZE_MUTE=both`;
- `L`/`R` mutes do not isolate the expected eye;
- the selected body pose is indistinguishable from the ordinary standing ball, or the rotation overwhelms the held squash/drop;
- `legacy-away` does not restore the rejected side/back mass;
- muting any F/A/B/C/E carrier leaves the held joke visually unchanged;
- the final carrier punctuation reads as one hose motion, detached beads, a sliding socket, a buckled span, an edge-on loop or an unowned outline gap;
- foreground lightning/particles are repainted by ink, or the O opening fills/closes;
- body, eyes and crown land on different frames rather than one f0304 arrival;
- big channels drift during the hold instead of parking, or small life channels freeze completely;
- the three-key attack or f0346..f0350 release introduces a snap, smear, seam or accidental reversal;
- the joke reads only in an effects-off diagnostic and not in shipping `manafold-taunt3`;
- the selected Manafold frames do not meet or exceed the committed Zixxtrixx face-readability bar;
- any attributed gate/control fails.

## File and commit boundary

The punchline implementation belongs in the authored expression packet after the crown-shuffle structure is green:

- `manafold_art.h`: selected body orientation constants, legacy pair, named five-carrier punctuation values;
- `manafold_clips.h`: held punctuation through `swallow_nodules`; no timing rewrite;
- `zhao_reel.cpp`: existing validated yaw/roll, eye-size and joint-mute controls only; add no new mechanism unless an existing control cannot isolate the accepted picture;
- `mspan`/evidence: held slot-21 carrier witness and attributed mutes after art selection;
- run-folder every-frame sheets, native/4x A/Bs and by-eye verdict.

Do not mix Trick-axis selection, Q2/Q3 continuity work, outline mechanics, scale infrastructure or publication into this art commit.

## Acceptance sentence

Taunt III is complete only when its existing dismissal snaps into one held, front-readable caught-cheating side-eye—whole body visibly squashed/dropped/tipped, all five attached carriers in an opposed pose, one almond eye unmistakably large and the other small—while the legacy orientation and every eye/carrier mute visibly weaken the joke, the face stays readable throughout f0304..f0344, and the result beats the accepted Zixxtrixx taunts at native resolution without explanation.
